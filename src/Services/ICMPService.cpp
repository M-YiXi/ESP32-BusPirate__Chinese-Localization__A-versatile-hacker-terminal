#include "ICMPService.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>
#include <algorithm>
#include <ESP32Ping.h>

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "ping/ping_sock.h"
extern "C"
{
#include "esp_ping.h"
}

portMUX_TYPE ICMPService::icmpMux = portMUX_INITIALIZER_UNLOCKED;
std::vector<std::string> ICMPService::icmpLog;
bool ICMPService::stopICMPFlag = false;

// 任务参数结构体
struct ICMPTaskParams
{
    std::string targetIP;      // 目标IP地址
    int count;                 // ping次数
    int timeout_ms;            // 超时时间（毫秒）
    int interval_ms;           // ping间隔（毫秒）
    ICMPService *service;      // 服务实例指针
};

struct DiscoveryTaskParams
{
    std::string deviceIP;      // 设备自身IP
    ICMPService *service;      // 服务实例指针
};

ICMPService::ICMPService() {}

ICMPService::~ICMPService()
{
    cleanupICMPService();
}

void ICMPService::cleanupICMPService()
{
    // 重置上次ping结果
    pingReady = false;
    pingRC = ping_rc_t::ping_error;
    pingMedianMs = -1;
    pingTX = 0;
    pingRX = 0;
    report.clear();
}

std::string ICMPService::getPingHelp() const{
    std::string helpMenu = std::string("用法：ping <主机> [-c <次数>] [-t <超时时间>] [-i <间隔>]\r\n选项：\r\n ") + 
        "\t-c <次数>    ping的次数（默认：5）\r\n " +
        "\t-t <超时时间>  超时时间（毫秒，默认：1000）\r\n" +
        "\t-i <间隔>  ping之间的间隔（毫秒，默认：200）";
    return helpMenu;
}

static bool resolve_ipv4_to_ip_addr(const std::string &targetIP, ip_addr_t &out)
{
    // 先尝试直接解析IP地址
    ip4_addr_t a4{};
    if (ip4addr_aton(targetIP.c_str(), &a4))
    {
        out.type = IPADDR_TYPE_V4;
        out.u_addr.ip4 = a4;
        return true;
    }
    // 解析失败则使用DNS解析
    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    struct addrinfo *res = nullptr;
    if (getaddrinfo(targetIP.c_str(), nullptr, &hints, &res) != 0 || !res)
        return false;
    out.type = IPADDR_TYPE_V4;
    out.u_addr.ip4.addr = ((sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    return true;
}

static int median_ms(std::vector<uint32_t> &v)
{
    if (v.empty())
        return -1;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n & 1)
        return (int)v[n / 2];
    return (int)((v[n / 2 - 1] + v[n / 2] + 1) / 2);
}

void ICMPService::discoveryTask(void* params){
    auto* taskParams = static_cast<DiscoveryTaskParams*>(params);
    std::string deviceIP = taskParams->deviceIP;
    ICMPService* service = taskParams->service;
    ip4_addr_t targetIP;
    uint32_t targetsResponded = 0;

    pushICMPLog("发现：正在扫描网络设备... 按[回车]停止。\r\n");

    if (!ip4addr_aton(deviceIP.c_str(), &targetIP))
    {
        pushICMPLog("发现：解析IP地址失败 " + deviceIP);
        delete taskParams;
        vTaskDelete(nullptr);
        return;
    }

    // 拆分IPv4地址的四个段
    uint8_t o1 = ip4_addr1(&targetIP);
    uint8_t o2 = ip4_addr2(&targetIP);
    uint8_t o3 = ip4_addr3(&targetIP);
    uint8_t deviceIndex = ip4_addr4(&targetIP);

    // 扫描同网段1-254的所有IP
    for (uint8_t targetIndex = 1; targetIndex < 255; targetIndex++)
    {
        // 检查用户是否停止扫描
        if (ICMPService::getICMPServiceStatus() == true)
        {
            pushICMPLog("发现：用户已停止扫描\r\n");
            delete taskParams;
            vTaskDelete(nullptr);
            return;
        }

        // 跳过自身IP
        if (targetIndex == deviceIndex)
            continue;

        // 重新构建目标IP地址
        IP4_ADDR(&targetIP, o1, o2, o3, targetIndex);

        // 转换为点分十进制字符串
        char targetIPCStr[16];
        ip4addr_ntoa_r(&targetIP, targetIPCStr, sizeof(targetIPCStr));
        std::string targetIPStr(targetIPCStr);

        service->cleanupICMPService();
        auto *params = new ICMPTaskParams{targetIPStr, 2, 150, 100, service};

        #ifndef DEVICE_M5STICK
        // 创建ping任务
        xTaskCreatePinnedToCore(pingAPI, "ICMPPing", 4096, params, 1, nullptr, 1);

        // 等待ping完成
        while (!service->pingReady)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // ping成功则记录设备
        if (service->pingRC == ping_rc_t::ping_ok){
            pushICMPLog("发现设备：" + targetIPStr);
            targetsResponded++;
        }
        #else
        // M5Stick使用ESP32Ping库（避免IRAM溢出）
        // 该库无法设置ping超时，每个ping耗时约5秒
        // TODO: 优化超时逻辑
        const unsigned long t0 = millis();
        const bool ok = Ping.ping(targetIPStr.c_str(), 1);
        const unsigned long t1 = millis();
        if (ok) {
            pushICMPLog("发现设备：" + targetIPStr);
            targetsResponded++;
        } else {
            pushICMPLog("未发现设备：" + targetIPStr);
        }
        #endif
    }

    // 输出扫描结果
    pushICMPLog(std::to_string(targetsResponded) + " 台设备在线，" + 
        std::to_string(254 - targetsResponded) + " 台设备离线");

    service->discoveryReady = true;

    delete taskParams;
    vTaskDelete(nullptr);
}

void ICMPService::startDiscoveryTask(const std::string deviceIP)
{
    report.clear();
    discoveryReady = false;
    stopICMPFlag = false;

    // 启动网络发现任务
    auto* p = new DiscoveryTaskParams{deviceIP, this};
    xTaskCreatePinnedToCore(discoveryTask, "ICMPDiscover", 8192, p, 1, nullptr, 0);
}

void ICMPService::startPingTask(const std::string &targetIP, int count, int timeout_ms, int interval_ms)
{
    // 先清理上次ping结果
    this->cleanupICMPService();

    // 构建ping任务参数（使用默认值处理无效输入）
    auto *params = new ICMPTaskParams{targetIP,
                                      count > 0 ? count : 5,
                                      timeout_ms > 0 ? timeout_ms : 1000,
                                      interval_ms > 0 ? interval_ms : 200,
                                      this};
    // 创建ping任务
    xTaskCreatePinnedToCore(pingAPI, "ICMPPing", 4096, params, 1, nullptr, 1);

    // 等待ping任务完成
    while(!this->pingReady)
        vTaskDelay(pdMS_TO_TICKS(10));

    // 生成ping报告
    this->report.clear();
    if (this->pingRC == ping_rc_t::ping_ok || this->pingRC == ping_rc_t::ping_timeout) {
        this->report = "--- " + targetIP + " ping 统计信息 ---\r\n";
        this->report += std::to_string(this->pingTX) + " 个数据包已发送，";
        this->report += std::to_string(this->pingRX) + " 个已接收，";
        this->report += std::to_string(100 - this->pingRX * 100 / this->pingTX) + "% 数据包丢失，";
        this->report += " 中位延迟 " + std::to_string(this->pingMedianMs) + " 毫秒\r\n";
    } else if (this->pingRC == ping_rc_t::ping_resolve_fail) {
        this->report = "解析 \"" + targetIP + "\" 失败\r\n";
    } else if (this->pingRC == ping_rc_t::ping_session_fail) {
        this->report = "创建ping会话失败\r\n";
    } else {
        report = "未知错误\r\n";
    }
        
}

void ICMPService::pingAPI(void *pvParams)
{
    auto *params = static_cast<ICMPTaskParams *>(pvParams);
    ICMPService *service = params->service;

    // 解析目标IP地址
    ip_addr_t target{};
    if (!resolve_ipv4_to_ip_addr(params->targetIP, target))
    {
        service->pingRC = ping_rc_t::ping_resolve_fail;
        service->pingReady = true;
        delete params;
        vTaskDelete(nullptr);
        return;
    }

    // 配置esp_ping参数
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = target;  // 目标地址
    config.count = params->count; // ping总次数
    config.interval_ms = params->interval_ms; // ping间隔
    config.timeout_ms = params->timeout_ms;   // 超时时间

    // ping上下文（存储RTT和接收计数）
    struct Ctx
    {
        std::vector<uint32_t> rtts;    // 往返时间列表
        volatile uint32_t rx = 0;      // 接收成功的数据包数
        volatile bool done = false;    // ping完成标志
    } ctx;

    // 注册ping回调函数
    esp_ping_callbacks_t cbs{};
    cbs.on_ping_success = [](esp_ping_handle_t h, void *arg)
    {
        uint32_t time_ms = 0;
        // 获取RTT（往返时间）
        esp_ping_get_profile(h, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms));
        auto *c = static_cast<Ctx *>(arg);
        c->rtts.push_back(time_ms);
        c->rx++;
    };
    cbs.on_ping_timeout = [](esp_ping_handle_t, void *)
    {
        // 超时不记录（仅统计成功的包用于计算中位值）
    };
    cbs.on_ping_end = [](esp_ping_handle_t, void *arg)
    {
        static_cast<Ctx *>(arg)->done = true;
    };
    cbs.cb_args = &ctx;

    // 创建ping会话
    esp_ping_handle_t h = nullptr;
    if (esp_ping_new_session(&config, &cbs, &h) != ESP_OK)
    {
        service->pingRC = ping_rc_t::ping_session_fail;
        service->pingReady = true;
        delete params;
        vTaskDelete(nullptr);
        return;
    }

    // 启动ping
    esp_ping_start(h);

    // 等待ping完成（最大等待时间：次数*(超时+间隔)+100ms）
    uint32_t wait_ms = (uint32_t)config.count * (config.timeout_ms + config.interval_ms) + 100;
    uint32_t t0 = millis();
    while (!ctx.done && (millis() - t0) < wait_ms)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 停止并销毁ping会话
    esp_ping_stop(h);
    esp_ping_delete_session(h);

    // 处理ping结果
    service->pingTX = (int)config.count;          // 发送的数据包数
    service->pingRX = (int)ctx.rx;                // 接收的数据包数
    service->pingMedianMs = median_ms(ctx.rtts);  // 计算中位延迟

    // 设置ping结果状态
    if (service->pingRX > 0)
        service->pingRC = ping_rc_t::ping_ok;          // ping成功
    else if (service->pingRX == 0)
        service->pingRC = ping_rc_t::ping_timeout;     // ping超时

    service->pingReady = true;

    delete params;
    vTaskDelete(nullptr);
}

void ICMPService::clearICMPLogging() {
    // 线程安全清空ICMP日志
    portENTER_CRITICAL(&icmpMux);
    icmpLog.clear();
    stopICMPFlag = false;
    portEXIT_CRITICAL(&icmpMux);
}

void ICMPService::pushICMPLog(const std::string& line) {
    // 线程安全添加ICMP日志（限制最大日志数）
    portENTER_CRITICAL(&icmpMux);
    icmpLog.push_back(line);
    if (icmpLog.size() > ICMP_LOG_MAX) {
        size_t excess = icmpLog.size() - ICMP_LOG_MAX;
        icmpLog.erase(icmpLog.begin(), icmpLog.begin() + excess);
    }
    portEXIT_CRITICAL(&icmpMux);
}

bool ICMPService::getICMPServiceStatus() {
    // 线程安全获取ICMP服务状态（是否停止）
    bool v;
    portENTER_CRITICAL(&ICMPService::icmpMux);
    v = ICMPService::stopICMPFlag;
    portEXIT_CRITICAL(&ICMPService::icmpMux);
    return v;
}

std::vector<std::string> ICMPService::fetchICMPLog() {
    // 线程安全获取并清空ICMP日志
    std::vector<std::string> batch;
    portENTER_CRITICAL(&ICMPService::icmpMux);
    batch.swap(ICMPService::icmpLog);
    portEXIT_CRITICAL(&ICMPService::icmpMux);
    return batch;
}

void ICMPService::stopICMPService(){
    // 线程安全停止ICMP服务
    portENTER_CRITICAL(&ICMPService::icmpMux);
    ICMPService::stopICMPFlag = true;
    portEXIT_CRITICAL(&ICMPService::icmpMux);
}