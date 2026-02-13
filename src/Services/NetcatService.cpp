#include "NetcatService.h"
#include <Arduino.h>
#include <lwip/sockets.h>   // ESP32 lwIP协议栈的socket通信头文件
#include <cstring>          // 字符串操作头文件
#include <freertos/FreeRTOS.h> // FreeRTOS实时操作系统头文件
#include <freertos/task.h>     // FreeRTOS任务管理头文件

// Netcat任务参数结构体：用于向FreeRTOS任务传递多参数（任务函数仅支持单个void*参数）
struct NetcatTaskParams
{
    std::string host;      // 目标连接的主机IP地址
    int verbosity;         // 日志详细程度（当前代码未实际使用）
    bool buffered;         // 是否启用行缓冲模式发送数据
    uint16_t port;         // 目标连接的端口号
    NetcatService *service;// 指向NetcatService类实例的指针
};

/**
 * @brief 创建FreeRTOS任务并启动TCP连接流程
 * @param host 目标主机IP
 * @param verbosity 日志等级（未使用）
 * @param port 目标端口
 * @param lineBuffer 是否启用行缓冲
 */
void NetcatService::startTask(const std::string &host, int verbosity, uint16_t port, bool lineBuffer)
{
    // 动态分配任务参数结构体并初始化
    auto *params = new NetcatTaskParams{host, verbosity, lineBuffer, port, this};
    // 创建FreeRTOS任务并绑定到ESP32核心1（避免与WiFi等外设冲突）
    // 参数说明：任务函数/任务名/栈大小/参数/优先级/任务句柄/绑定核心
    xTaskCreatePinnedToCore(connectTask, "NetcatConnect", 20000, params, 1, nullptr, 1);
    delay(500); // 短暂延迟，确保任务正常启动
}

/**
 * @brief FreeRTOS任务的入口函数，实际执行TCP连接逻辑
 * @param pvParams 任务参数（NetcatTaskParams结构体指针）
 */
void NetcatService::connectTask(void *pvParams)
{
    // 将void*类型参数转换为实际的结构体指针
    auto *params = static_cast<NetcatTaskParams *>(pvParams);
    // 调用NetcatService实例的connect方法建立TCP连接
    params->service->connect(params->host, params->verbosity, params->port, params->buffered);
    delete params;   // 释放动态分配的参数内存，避免内存泄漏
    vTaskDelete(nullptr); // 连接流程完成后，删除当前FreeRTOS任务
}

/**
 * @brief 执行TCP连接的核心逻辑
 * @param host 目标主机IP
 * @param verbosity 日志等级（未使用）
 * @param port 目标端口
 * @param lineBuffer 是否启用行缓冲
 * @return 连接成功返回true，失败返回false
 */
bool NetcatService::connect(const std::string &host, int verbosity, uint16_t port, bool lineBuffer)
{
    buffered = lineBuffer; // 保存行缓冲模式配置
    if (!openSocket(host, port)) // 创建并连接socket失败则返回false
        return false;
    setNonBlocking(); // 将socket设置为非阻塞模式（避免读写阻塞任务）
    connected = true; // 标记连接状态为已连接
    return true;
}

/**
 * @brief 创建TCP socket并连接到目标主机
 * @param host 目标主机IP
 * @param port 目标端口
 * @return 连接成功返回true，失败返回false
 */
bool NetcatService::openSocket(const std::string &host, uint16_t port)
{
    // 创建IPv4 TCP套接字：AF_INET=IPv4，SOCK_STREAM=TCP协议，IPPROTO_IP=IP协议
    sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) // socket创建失败
        return false;

    // 初始化目标地址结构体
    sockaddr_in dest{};
    dest.sin_family = AF_INET; // IPv4协议族
    dest.sin_port = htons(port); // 端口号转换为网络字节序（大端序）
    // 将字符串格式的IP地址转换为网络字节序的二进制地址
    inet_pton(AF_INET, host.c_str(), &dest.sin_addr);

    // 发起TCP连接
    if (::connect(sock, (sockaddr *)&dest, sizeof(dest)) != 0)
    {
        ::close(sock); // 连接失败，关闭socket
        sock = -1;     // 重置socket描述符
        return false;
    }
    return true; // 连接成功
}

/**
 * @brief 将socket设置为非阻塞模式
 * @note 非阻塞模式下，读写操作无数据时会立即返回，不会阻塞任务执行
 */
void NetcatService::setNonBlocking()
{
    int flags = fcntl(sock, F_GETFL, 0); // 获取当前socket的标志位
    fcntl(sock, F_SETFL, flags | O_NONBLOCK); // 添加非阻塞标志
}

/**
 * @brief 检查当前是否处于已连接状态
 * @return 已连接返回true，未连接返回false
 */
bool NetcatService::isConnected() const
{
    // 双重校验：socket描述符有效 且 连接状态标记为true
    return sock >= 0 && connected;
}

/**
 * @brief 发送单个字符（支持行缓冲/即时发送两种模式）
 * @param c 要发送的字符
 */
void NetcatService::writeChar(char c)
{
    if (!isConnected()) // 未连接则直接返回
        return;

    // 行缓冲模式：字符先存入缓冲区，遇到换行/回车时一次性发送
    if (buffered)
    {
        txBuf.push_back(c); // 将字符加入发送缓冲区
        // 遇到换行符(\n)或回车符(\r)时触发发送
        if (c == '\n' || c == '\r')
        {
            if (c == '\r') 
                txBuf.push_back('\n'); // 单独的\r自动补\n，统一换行格式
            // 发送缓冲区中的所有数据
            ::send(sock, txBuf.data(), txBuf.size(), 0);
            txBuf.clear(); // 清空发送缓冲区
        }
    }
    else // 非缓冲模式：收到字符后立即发送
    {
        ::send(sock, &c, 1, 0);
    }
}

/**
 * @brief 非阻塞读取socket接收的数据
 * @return 读取到的字符串（无数据/连接关闭返回空字符串）
 */
std::string NetcatService::readOutputNonBlocking()
{
    if (!isConnected()) // 未连接则返回空字符串
        return "";

    char buf[256]; // 接收缓冲区，单次最多读取256字节
    // 非阻塞接收数据：无数据时返回<=0（EWOULDBLOCK错误），有数据返回读取的字节数
    int n = ::recv(sock, buf, sizeof(buf), 0);
    if (n <= 0) // 无数据或连接已关闭
        return "";
    return std::string(buf, n); // 将接收到的字符转为字符串返回
}

/**
 * @brief 关闭TCP连接并释放资源
 */
void NetcatService::close()
{
    if (sock >= 0) // socket描述符有效时才执行关闭操作
    {
        ::shutdown(sock, SHUT_RDWR); // 关闭socket的读写方向
        ::close(sock);              // 关闭socket文件描述符
        sock = -1;                  // 重置socket描述符
    }
    connected = false; // 标记连接状态为未连接
}