#include "Controllers/WifiController.h"
#include "Vendors/wifi_atks.h"

/*
Entry point for command
*/
void WifiController::handleCommand(const TerminalCommand &cmd)
{
    const auto &root = cmd.getRoot();

    if (root == "connect") handleConnect(cmd);
    else if (root == "disconnect") handleDisconnect(cmd);
    else if (root == "status") handleStatus(cmd);
    else if (root == "ap") handleAp(cmd);
    else if (root == "spoof") handleSpoof(cmd);
    else if (root == "scan") handleScan(cmd);
    else if (root == "probe") handleProbe();
    else if (root == "ping") handlePing(cmd);
    else if (root == "sniff") handleSniff(cmd);
    else if (root == "webui") handleWebUi(cmd);
    else if (root == "ssh") handleSsh(cmd);
    else if (root == "telnet") handleTelnet(cmd);
    else if (root == "nc") handleNetcat(cmd);
    else if (root == "nmap") handleNmap(cmd);
    else if (root == "modbus") handleModbus(cmd);
    else if (root == "http") handleHttp(cmd);
    else if (root == "lookup") handleLookup(cmd);
    else if (root == "discovery") handleDiscovery(cmd);
    else if (root == "reset") handleReset();
    else if (root == "deauth") handleDeauth(cmd);
    else handleHelp();
}

std::vector<std::string> WifiController::buildWiFiLines() {
    std::vector<std::string> lines;
    lines.reserve(4);

    int mode   = wifiService.getWifiModeRaw();
    int status = wifiService.getWifiStatusRaw();

    // MODE
    lines.push_back(
        std::string("模式 ") + WifiService::wifiModeToStr(mode) // 汉化
    );

    // Disconnection
    if (status != WL_CONNECTED) {
        lines.push_back("WiFi 已断开连接"); // 汉化
        return lines;
    }

    // Connected
    lines.push_back("WiFi 已连接"); // 汉化

    // STA IP
    std::string staIp = wifiService.getLocalIP();
    if (!staIp.empty())
        lines.push_back(staIp);

    // SSID
    std::string ssid = wifiService.getSsid();
    if (!ssid.empty()) {
        std::string nameLimited = ssid.length() > 15 ? ssid.substr(0, 15) + "..." : ssid;
        lines.push_back(nameLimited);
    }

    return lines;
}

/*
Connect
*/
void WifiController::handleConnect(const TerminalCommand &cmd)
{
    std::string ssid;
    std::string password;
    auto args = argTransformer.splitArgs(cmd.getSubcommand());

    // No args provided, we need to check saved creds or scan and select networks
    if (cmd.getSubcommand().empty()) {

        // Check saved creds
        nvsService.open();
        ssid = nvsService.getString(state.getNvsSsidField());
        password = nvsService.getString(state.getNvsPasswordField());
        nvsService.close();
        auto confirmation = false;

        // Creds found
        if (!ssid.empty() && !password.empty()) {
            confirmation = userInputManager.readYesNo(
                "WiFi：是否使用保存的 " + ssid + " 认证信息？(是/否)", true // 汉化
            );
        } 

        // Select network if no creds or not confirmed
        if (!confirmation) {
            terminalView.println("WiFi：正在扫描可用网络..."); // 汉化
            auto networks = wifiService.scanNetworks();
            int selectedIndex = userInputManager.readValidatedChoiceIndex("\n选择Wi-Fi网络", networks, 0); // 汉化
            ssid = networks[selectedIndex];
            terminalView.println("已选SSID：" + ssid); // 汉化
            terminalView.print("密码："); // 汉化
            password = userInputManager.getLine();
        }

    // Args provided
    } else  {
        // Concatenate subcommand and args
        std::string full = cmd.getSubcommand() + " " + cmd.getArgs();
    
        // Find the last space to separate SSID and password
        size_t pos = full.find_last_of(' ');
        if (pos == std::string::npos || pos == full.size() - 1) {
            terminalView.println("使用方法: connect <ssid> <密码>"); // 汉化
            return;
        }
        ssid = full.substr(0, pos);
        password = full.substr(pos + 1);
    }

    terminalView.println("WiFi：正在连接到 " + ssid + "..."); // 汉化

    wifiService.setModeApSta();
    wifiService.connect(ssid, password);
    if (wifiService.isConnected()) {
        terminalView.println("");
        terminalView.println("WiFi：已成功连接到Wi-Fi！"); // 汉化
        terminalView.println("      如需使用基于网页的命令行界面，请重置设备并选择WiFi Web模式"); // 汉化
        terminalView.println("");
        terminalView.println("[无屏模式] 无屏幕时启动WebUI的方法："); // 汉化
        terminalView.println("  1. 重置设备（开机时不要按住板载按键）"); // 汉化
        terminalView.println("  2. 设备上电后，你有3秒时间按下板载按键"); // 汉化
        terminalView.println("  3. 内置LED状态说明："); // 汉化
        terminalView.println("     • 蓝色  = 未保存Wi-Fi认证信息。"); // 汉化
        terminalView.println("     • 白色  = 正在连接中"); // 汉化
        terminalView.println("     • 绿色  = 已连接，可在浏览器中打开WebUI"); // 汉化
        terminalView.println("     • 红色  = 连接失败，请通过串口重新尝试连接"); // 汉化
        terminalView.println("");
        terminalView.println("WiFi Web UI地址：http://" + wifiService.getLocalIP()); // 汉化

        // Save creds
        nvsService.open();
        nvsService.saveString(state.getNvsSsidField(), ssid);
        nvsService.saveString(state.getNvsPasswordField(), password);
        nvsService.close();
    } else {
        terminalView.println("WiFi：连接失败。"); // 汉化
        wifiService.reset();
        delay(100);
    }
}

/*
Disconnect
*/
void WifiController::handleDisconnect(const TerminalCommand &cmd)
{
    wifiService.disconnect();
    terminalView.println("WiFi：已断开连接。"); // 汉化
}

/*
Status
*/
void WifiController::handleStatus(const TerminalCommand &cmd)
{
    auto ssid     = wifiService.getSsid();     if (ssid.empty()) ssid = "未获取"; // 汉化
    auto bssid    = wifiService.getBssid();    if (bssid.empty()) bssid = "未获取"; // 汉化
    auto hostname = wifiService.getHostname(); if (hostname.empty()) hostname = "未获取"; // 汉化

    terminalView.println("\n=== Wi-Fi 状态信息 ==="); // 汉化
    terminalView.println("工作模式     : " + std::string(wifiService.getWifiModeRaw() == WIFI_MODE_AP ? "接入点(AP)" : "终端(STA)")); // 汉化
    terminalView.println("AP MAC地址   : " + wifiService.getMacAddressAp()); // 汉化
    terminalView.println("STA MAC地址  : " + wifiService.getMacAddressSta()); // 汉化
    terminalView.println("IP地址       : " + wifiService.getLocalIp()); // 汉化
    terminalView.println("子网掩码     : " + wifiService.getSubnetMask()); // 汉化
    terminalView.println("网关地址     : " + wifiService.getGatewayIp()); // 汉化
    terminalView.println("DNS1         : " + wifiService.getDns1()); // 汉化
    terminalView.println("DNS2         : " + wifiService.getDns2()); // 汉化
    terminalView.println("主机名       : " + hostname); // 汉化

    terminalView.println("SSID         : " + ssid);
    terminalView.println("BSSID        : " + bssid);
    terminalView.println("配网功能启用 : " + std::string(wifiService.isProvisioningEnabled() ? "是" : "否")); // 汉化

    const int status = wifiService.getWifiStatusRaw();
    if (status == 3 /* WL_CONNECTED */) {
        terminalView.println("信号强度(RSSI): " + std::to_string(wifiService.getRssi()) + " dBm"); // 汉化
        terminalView.println("信道         : " + std::to_string(wifiService.getChannel())); // 汉化
    } else {
        terminalView.println("信号强度(RSSI): 未获取"); // 汉化
        terminalView.println("信道         : 未获取"); // 汉化
    }

    terminalView.println("工作模式     : " + std::string(wifiService.wifiModeToStr(wifiService.getWifiModeRaw()))); // 汉化
    terminalView.println("连接状态     : " + std::string(wifiService.wlStatusToStr(status))); // 汉化
    terminalView.println("配网功能启用 : " + std::string(wifiService.isProvisioningEnabled() ? "是" : "否")); // 汉化
    terminalView.println("====================\n");
}

/*
Access Point
*/
void WifiController::handleAp(const TerminalCommand &cmd)
{
    auto ssid = cmd.getSubcommand();

    if (ssid.empty())
    {
        terminalView.println("使用方法: ap <ssid> <密码>"); // 汉化
        terminalView.println("       ap spam");
        return;
    }

    if (ssid == "spam") {
        handleApSpam();
        return;
    }

    auto full = cmd.getSubcommand() + " " + cmd.getArgs();

    // Find the last space to separate SSID and password
    size_t pos = full.find_last_of(' ');
    if (pos == std::string::npos || pos == full.size() - 1) {
        terminalView.println("使用方法: connect <ssid> <密码>"); // 汉化
        return;
    }
    ssid = full.substr(0, pos);
    auto password = full.substr(pos + 1);

    // Already connected, mode AP+STA
    if (wifiService.isConnected())
    {
        wifiService.setModeApSta();
    }
    else
    {
        wifiService.setModeApOnly();
    }

    if (wifiService.startAccessPoint(ssid, password))
    {
        terminalView.println("WiFi：接入点已启动，SSID为 " + ssid); // 汉化
        terminalView.println("AP IP地址：" + wifiService.getApIp()); // 汉化

        auto nvsSsidField = state.getNvsSsidField();
        auto nvsPasswordField = state.getNvsPasswordField();
        auto ssid = nvsService.getString(nvsSsidField, "");
        auto password = nvsService.getString(nvsPasswordField, "");

        // Try to reconnect to saved WiFi
        if (!ssid.empty() && !password.empty())
        {
            wifiService.connect(ssid, password);
        }

        if (wifiService.isConnected())
        {
            terminalView.println("STA IP地址：" + wifiService.getLocalIp()); // 汉化
        }
    }
    else
    {
        terminalView.println("WiFi：接入点启动失败。"); // 汉化
    }
}

/*
AP Spam
*/
void WifiController::handleApSpam()
{
    terminalView.println("WiFi：信标群发已启动... 按下[ENTER]停止。"); // 汉化
    while (true)
    {
        beaconCreate(""); // func from Vendors/wifi_atks.h

        // Enter press to stop
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;
        delay(10);
    }

    terminalView.println("WiFi：信标群发已停止。\n"); // 汉化
}

/*
Scan
*/
void WifiController::handleScan(const TerminalCommand &)
{
    terminalView.println("WiFi：正在扫描网络..."); // 汉化
    delay(300);

    auto networks = wifiService.scanDetailedNetworks();

    for (const auto &net : networks)
    {
        std::string line = "  SSID：" + net.ssid; // 汉化
        line += " | 加密方式：" + wifiService.encryptionTypeToString(net.encryption); // 汉化
        line += " | BSSID：" + net.bssid; // 汉化
        line += " | 信道：" + std::to_string(net.channel); // 汉化
        line += " | 信号强度：" + std::to_string(net.rssi) + " dBm"; // 汉化
        if (net.open)
            line += " [开放]"; // 汉化
        if (net.vulnerable)
            line += " [易受攻击]"; // 汉化
        if (net.hidden)
            line += " [隐藏]"; // 汉化

        terminalView.println(line);
    }

    if (networks.empty())
    {
        terminalView.println("WiFi：未发现任何网络。"); // 汉化
    }
}

/*
Probe
*/
void WifiController::handleProbe() 
{
    terminalView.println("WiFi：开始探测开放网络的互联网访问权限..."); // 汉化
    terminalView.println("\n[警告] 该操作会尝试连接周边的开放网络。\n"); // 汉化

    // Confirm before starting
    auto confirmation = userInputManager.readYesNo("是否启动Wi-Fi探测以查找可访问互联网的网络？", false); // 汉化
    if (!confirmation) {
        terminalView.println("WiFi：探测已取消。\n"); // 汉化
        return;
    }

    // Stop any existing probe
    if (wifiOpenScannerService.isOpenProbeRunning()) {
        wifiOpenScannerService.stopOpenProbe();
    }
    wifiOpenScannerService.clearProbeLog();

    // Start the open probe service
    if (!wifiOpenScannerService.startOpenProbe()) {
        terminalView.println("WiFi：探测启动失败。\n"); // 汉化
        return;
    }

    terminalView.println("WiFi：互联网访问探测中... 按下[ENTER]停止。\n"); // 汉化

    // Start the open probe task
    while (wifiOpenScannerService.isOpenProbeRunning()) {
        // Display logs
        auto batch = wifiOpenScannerService.fetchProbeLog();
        for (auto& ln : batch) {
            terminalView.println(ln.c_str());
        }

        // Enter Press to stop
        int ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            wifiOpenScannerService.stopOpenProbe();
            break;
        }

        delay(10);
    }

    // Flush final logs
    for (auto& ln : wifiOpenScannerService.fetchProbeLog()) {
        terminalView.println(ln.c_str());
    }
    terminalView.println("WiFi：开放网络探测已结束。\n"); // 汉化
}

/*
Sniff
*/
void WifiController::handleSniff(const TerminalCommand &cmd)
{
    terminalView.println("WiFi嗅探已启动... 按下[ENTER]停止。\n"); // 汉化

    wifiService.startPassiveSniffing();
    wifiService.switchChannel(1);

    uint8_t channel = 1;
    unsigned long lastHop = 0;
    unsigned long lastPull = 0;

    while (true)
    {
        // Enter Press
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n')
            break;

        // Read sniff data
        if (millis() - lastPull > 20)
        {
            auto logs = wifiService.getSniffLog();
            for (const auto &line : logs)
            {
                terminalView.println(line);
            }
            lastPull = millis();
        }

        // Switch channel every 100ms
        if (millis() - lastHop > 100)
        {
            channel = (channel % 13) + 1; // channel 1 to 13
            wifiService.switchChannel(channel);
            lastHop = millis();
        }

        delay(5);
    }

    wifiService.stopPassiveSniffing();
    terminalView.println("WiFi嗅探已停止。\n"); // 汉化
}

/*
Spoof
*/
void WifiController::handleSpoof(const TerminalCommand &cmd)
{
    auto mode = cmd.getSubcommand();
    auto mac = cmd.getArgs();

    if (mode.empty() && mac.empty())
    {
        terminalView.println("使用方法: spoof sta <mac>"); // 汉化
        terminalView.println("       spoof ap <mac>");
        return;
    }

    WifiService::MacInterface iface = (mode == "sta")
                                          ? WifiService::MacInterface::Station
                                          : WifiService::MacInterface::AccessPoint;

    terminalView.println("WiFi：正在将 " + mode + " 端MAC地址伪造为 " + mac + "..."); // 汉化

    bool ok = wifiService.spoofMacAddress(mac, iface);

    if (ok)
    {
        terminalView.println("WiFi：MAC地址伪造成功。"); // 汉化
    }
    else
    {
        terminalView.println("WiFi：MAC地址伪造失败。"); // 汉化
    }
}

/*
Reset
*/
void WifiController::handleReset()
{
    wifiService.reset();
    terminalView.println("WiFi：接口已重置。已断开所有连接。"); // 汉化
}

/*
Web Interface
*/
void WifiController::handleWebUi(const TerminalCommand &)
{
    if (wifiService.isConnected())
    {
        auto ip = wifiService.getLocalIP();
        terminalView.println("");
        terminalView.println("[警告] 若你通过串口连接设备，"); // 汉化
        terminalView.println("       Web UI将无法激活。"); // 汉化
        terminalView.println("       请重置设备并选择WiFi Web模式。"); // 汉化
        terminalView.println("");
        terminalView.println("[无屏模式] 无屏幕时启动WebUI的方法："); // 汉化
        terminalView.println("  1. 重置设备（开机时不要按住板载按键）"); // 汉化
        terminalView.println("  2. 设备上电后，你有3秒时间按下板载按键"); // 汉化
        terminalView.println("  3. 内置LED状态说明："); // 汉化
        terminalView.println("     • 蓝色  = 未保存Wi-Fi认证信息。"); // 汉化
        terminalView.println("     • 白色  = 正在连接中"); // 汉化
        terminalView.println("     • 绿色  = 已连接，可在浏览器中打开WebUI。"); // 汉化
        terminalView.println("     • 红色  = 连接失败，请通过串口重新尝试连接"); // 汉化
        terminalView.println("");
        terminalView.println("WiFi Web UI地址：http://" + ip); // 汉化
    }
    else
    {
        terminalView.println("WiFi Web UI：未连接网络。请先连接网络以查看访问地址。"); // 汉化
    }
}

/*
Config
*/
void WifiController::handleConfig()
{
    if (state.getTerminalMode() == TerminalTypeEnum::Standalone) return;

    terminalView.println("[警告] 若你通过Web CLI连接设备，"); // 汉化
    terminalView.println("       执行Wi-Fi相关命令可能导致"); // 汉化
    terminalView.println("       终端会话断开连接。"); // 汉化
    terminalView.println("       请勿使用：sniff、probe、connect、scan、spoof..."); // 汉化
    terminalView.println("       若连接丢失，请使用USB串口或重启设备。\n"); // 汉化
}

/*
Help
*/
void WifiController::handleHelp()
{
    terminalView.println("WiFi 命令列表："); // 汉化
    terminalView.println("  scan                - 扫描周边Wi-Fi网络"); // 补充说明
    terminalView.println("  connect             - 连接到Wi-Fi网络"); // 补充说明
    terminalView.println("  sniff               - 嗅探Wi-Fi数据包"); // 补充说明
    terminalView.println("  probe               - 探测开放网络的互联网访问权限"); // 补充说明
    terminalView.println("  spoof sta <mac>     - 伪造STA端MAC地址"); // 补充说明
    terminalView.println("  spoof ap <mac>      - 伪造AP端MAC地址"); // 补充说明
    terminalView.println("  deauth [ssid]       - 发送解除认证帧"); // 补充说明
    terminalView.println("  status              - 查看Wi-Fi状态信息"); // 补充说明
    terminalView.println("  disconnect          - 断开Wi-Fi连接"); // 补充说明
    terminalView.println("  ap <ssid> <password>- 创建Wi-Fi接入点"); // 补充说明
    terminalView.println("  ap spam             - 启动信标群发"); // 补充说明
    ANetworkController::handleHelp();
    terminalView.println("  webui               - 查看Web UI访问地址"); // 补充说明
    terminalView.println("  reset               - 重置Wi-Fi接口"); // 补充说明
}

/*
Ensure Configuration
*/
void WifiController::ensureConfigured()
{
    if (!configured)
    {
        handleConfig();
        configured = true;
    }
}

/*
Deauthenticate stations attack
*/
void WifiController::handleDeauth(const TerminalCommand &cmd)
{   
    auto target = cmd.getSubcommand();
    
    // Select network if no target provided
    if (target.empty()) {
        terminalView.println("WiFi：正在扫描可用网络..."); // 汉化
        auto networks = wifiService.scanNetworks();
        int selectedIndex = userInputManager.readValidatedChoiceIndex("\n选择Wi-Fi网络", networks, 0); // 汉化
        target = networks[selectedIndex];
    }

    // if the SSID have space in name, e.g "Router Wifi"
    if (!cmd.getArgs().empty())
    {
        target += " " + cmd.getArgs();
    }

    terminalView.println("WiFi：正在向 \"" + target + "\" 发送解除认证帧..."); // 汉化

    bool ok = wifiService.deauthApBySsid(target);

    if (ok)
        terminalView.println("WiFi：解除认证帧已发送。"); // 汉化
    else
        terminalView.println("WiFi：未找到指定SSID。"); // 汉化
}