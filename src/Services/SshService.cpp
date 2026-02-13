#include "SshService.h"
#include <Arduino.h>

/**
 * @brief SSH连接任务的参数结构体
 * @note 用于向异步任务传递SSH连接所需的所有参数
 */
struct SshTaskParams {
    std::string host;      // SSH服务器地址（IP/域名）
    std::string user;      // SSH登录用户名
    std::string pass;      // SSH登录密码
    int verbosity;         // SSH日志详细程度（0=静默，越高日志越详细）
    int port;              // SSH服务器端口（默认22）
    SshService* service;   // 指向SshService实例的指针
};

/**
 * @brief 启动SSH连接异步任务（核心函数）
 * @param host SSH服务器地址
 * @param user 登录用户名
 * @param pass 登录密码
 * @param verbosity 日志详细程度
 * @param port SSH端口
 * @note 任务创建后固定运行在ESP32的Core 1，栈大小20000，优先级1；延迟2秒确保任务启动
 */
void SshService::startTask(const std::string& host, const std::string& user, const std::string& pass, int verbosity, int port) {
    // 动态创建任务参数结构体并赋值
    auto* params = new SshTaskParams{host, user, pass, verbosity, port, this};
    // 创建并启动异步任务（绑定到Core 1）
    xTaskCreatePinnedToCore(connectTask, "SSHConnect", 20000, params, 1, nullptr, 1);
    delay(2000); // 延迟2秒，确保任务有足够时间开始执行
}

/**
 * @brief SSH连接任务的入口函数（FreeRTOS任务）
 * @param pvParams 任务参数（SshTaskParams指针）
 * @note 负责调用实际的连接逻辑，执行完成后释放参数并删除自身任务
 */
void SshService::connectTask(void* pvParams) {
    // 转换参数类型
    auto* params = static_cast<SshTaskParams*>(pvParams);
    // 调用SshService实例的连接方法
    params->service->connect(params->host, params->user, params->pass, params->verbosity, params->port);
    delete params; // 释放参数内存
    vTaskDelete(nullptr); // 删除当前任务
}

/**
 * @brief 建立SSH连接（核心逻辑）
 * @param host SSH服务器地址
 * @param user 登录用户名
 * @param pass 登录密码
 * @param verbosity 日志详细程度
 * @param port SSH端口
 * @return 连接成功返回true，失败返回false
 * @note 流程：创建会话→设置参数→建立连接→认证→打开通道→请求PTY→启动Shell
 */
bool SshService::connect(const std::string& host, const std::string& user, const std::string& pass, int verbosity, int port) {
    // 创建新的SSH会话
    session = ssh_new();
    if (!session) return false;

    // 设置SSH会话参数
    ssh_options_set(session, SSH_OPTIONS_HOST, host.c_str());     // 服务器地址
    ssh_options_set(session, SSH_OPTIONS_USER, user.c_str());     // 用户名
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);            // 端口
    ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity); // 日志详细程度

    // 建立SSH连接
    if (ssh_connect(session) != SSH_OK) {
        close(); // 连接失败，关闭会话
        return false;
    }

    // 依次执行认证、打开通道、请求PTY、启动Shell，任一环节失败则返回false
    if (!authenticate(pass)) return false;
    if (!openChannel()) return false;
    if (!requestPty()) return false;
    if (!startShell()) return false;

    connected = true; // 标记SSH已成功连接
    return true;
}

/**
 * @brief SSH密码认证
 * @param password 登录密码
 * @return 认证成功返回true，失败返回false
 */
bool SshService::authenticate(const std::string& password) {
    // 执行密码认证
    int rc = ssh_userauth_password(session, nullptr, password.c_str());
    if (rc != SSH_AUTH_SUCCESS) {
        return false;
    }
    return true;
}

/**
 * @brief 打开SSH会话通道
 * @return 通道打开成功返回true，失败返回false
 */
bool SshService::openChannel() {
    // 创建新的SSH通道
    channel = ssh_channel_new(session);
    // 打开会话通道，失败则返回false
    if (!channel || ssh_channel_open_session(channel) != SSH_OK) {
        return false;
    }
    return true;
}

/**
 * @brief 请求PTY（伪终端）
 * @return 请求成功返回true，失败返回false
 * @note 启动Shell前需先请求PTY，否则Shell无法正常交互
 */
bool SshService::requestPty() {
    if (ssh_channel_request_pty(channel) != SSH_OK) {
        return false;
    }
    return true;
}

/**
 * @brief 启动SSH Shell会话
 * @return Shell启动成功返回true，失败返回false
 */
bool SshService::startShell() {
    if (ssh_channel_request_shell(channel) != SSH_OK) {
        return false;
    }
    return true;
}

/**
 * @brief 检查SSH连接是否有效
 * @return 连接有效返回true，无效返回false
 * @note 需同时满足：连接标记为true、会话/通道不为空、通道已打开且未到EOF
 */
bool SshService::isConnected() const {
    if (!connected || !session || !channel) return false;
    return ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel);
}

/**
 * @brief 向SSH通道写入单个字符
 * @param c 要写入的字符
 * @note 仅当连接有效时执行写入操作
 */
void SshService::writeChar(char c) {
    if (!isConnected()) return;
    ssh_channel_write(channel, &c, 1);
}

/**
 * @brief 阻塞读取SSH通道输出
 * @return 读取到的字符串（无数据返回空字符串）
 * @note 此方法会阻塞直到有数据可读或连接关闭
 */
std::string SshService::readOutput() {
    if (!isConnected()) return "";
    char buf[256]; // 读取缓冲区（256字节）
    // 阻塞读取通道数据
    int n = ssh_channel_read(channel, buf, sizeof(buf), 0);
    return (n > 0) ? std::string(buf, n) : ""; // 有数据则返回字符串，否则返回空
}

/**
 * @brief 非阻塞读取SSH通道输出
 * @return 读取到的字符串（无数据返回空字符串）
 * @note 此方法不会阻塞，立即返回当前可用数据
 */
std::string SshService::readOutputNonBlocking() {
    if (!isConnected()) return "";
    char buf[256]; // 读取缓冲区（256字节）
    // 非阻塞读取通道数据
    int n = ssh_channel_read_nonblocking(channel, buf, sizeof(buf), 0);
    return (n > 0) ? std::string(buf, n) : ""; // 有数据则返回字符串，否则返回空
}

/**
 * @brief 关闭SSH连接并释放资源
 * @note 先关闭通道→释放通道→断开会话→释放会话→重置连接状态
 */
void SshService::close() {
    // 关闭并释放SSH通道
    if (channel) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        channel = nullptr;
    }
    // 断开并释放SSH会话
    if (session) {
        ssh_disconnect(session);
        ssh_free(session);
        session = nullptr;
    }
    connected = false; // 重置连接状态标记
}