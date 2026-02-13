#include "ANetworkController.h"

/*
Constructor
*/
ANetworkController::ANetworkController(
    ITerminalView& terminalView, 
    IInput& terminalInput, 
    IInput& deviceInput,
    WifiService& wifiService, 
    WifiOpenScannerService& wifiOpenScannerService,
    EthernetService& ethernetService,
    SshService& sshService,
    NetcatService& netcatService,
    NmapService& nmapService,
    ICMPService& icmpService,
    NvsService& nvsService,
    HttpService& httpService,
    TelnetService& telnetService,
    ArgTransformer& argTransformer,
    JsonTransformer& jsonTransformer,
    UserInputManager& userInputManager,
    ModbusShell& modbusShell
)
: terminalView(terminalView),
  terminalInput(terminalInput),
  deviceInput(deviceInput),
  wifiService(wifiService),
  wifiOpenScannerService(wifiOpenScannerService),
  ethernetService(ethernetService),
  sshService(sshService),
  netcatService(netcatService),
  nmapService(nmapService),
  icmpService(icmpService),
  nvsService(nvsService),
  httpService(httpService),
  telnetService(telnetService),
  argTransformer(argTransformer),
  jsonTransformer(jsonTransformer),
  userInputManager(userInputManager),
  modbusShell(modbusShell)
{
}

/*
ICMP Ping
*/
void ANetworkController::handlePing(const TerminalCommand &cmd)
{
    if (!wifiService.isConnected() && !ethernetService.isConnected()) {
        terminalView.println("Ping：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    const std::string host = cmd.getSubcommand();
    if (host.empty() || host == "-h" || host == "--help") {
        terminalView.println(icmpService.getPingHelp());
        return;
    }   
    
    #ifndef DEVICE_M5STICK

    auto args = argTransformer.splitArgs(cmd.getArgs());
    int pingCount = 5, pingTimeout = 1000, pingInterval = 200;

    for (int i=0;i<args.size();i++) {
        if (args[i].empty()) continue; // Skip empty args
        auto argument = args[i];
        if (argument == "-h" || argument == "--help") {
            terminalView.println(icmpService.getPingHelp());
            return;
        } else if (argument == "-c") {
            if (++i < args.size()) {
                if (!argTransformer.parseInt(args[i], pingCount) || args[i].empty()) {
                    terminalView.println("无效的计数数值。"); // 汉化
                    return;
                }
            }
        } else if (argument == "-t") {
            if (++i < args.size()) {
                if (!argTransformer.parseInt(args[i], pingTimeout) || args[i].empty()) {
                    terminalView.println("无效的超时数值。"); // 汉化
                    return;
                }
            }
        } else if (argument == "-i") {
            if (++i < args.size()) {
                if (!argTransformer.parseInt(args[i], pingInterval) || args[i].empty()) {
                    terminalView.println("无效的间隔数值。"); // 汉化
                    return;
                }
            }
        }
    }

    icmpService.startPingTask(host, pingCount, pingTimeout, pingInterval);
    while (!icmpService.isPingReady())
        vTaskDelay(pdMS_TO_TICKS(50));

    terminalView.print(icmpService.getReport());


    #else  

    // Using ESP32Ping library to avoid IRAM overflow

    const unsigned long t0 = millis();
    const bool ok = Ping.ping(host.c_str(), 1);
    const unsigned long t1 = millis();
    if (ok) {
        terminalView.println("Ping：" + host + " 测试成功，耗时 " + std::to_string(t1 - t0) + " 毫秒"); // 汉化
    } else {
        terminalView.println("Ping：" + host + " 测试失败。"); // 汉化
    }

    #endif
}

/*
Discovery  
*/
void ANetworkController::handleDiscovery(const TerminalCommand &cmd)
{
    bool wifiConnected = wifiService.isConnected();
    bool ethConnected = ethernetService.isConnected();
    phy_interface_t phy_interface = phy_interface_t::phy_none;

    // Which interface to scan
    auto mode = globalState.getCurrentMode();
    if (wifiConnected && mode == ModeEnum::WiFi){
        phy_interface = phy_interface_t::phy_wifi;
    }
    else if (ethConnected && mode == ModeEnum::ETHERNET) {
        phy_interface = phy_interface_t::phy_eth;
    }
    else {
        terminalView.println("设备发现：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    const std::string deviceIP = phy_interface == phy_interface_t::phy_wifi ? wifiService.getLocalIP() : ethernetService.getLocalIP();
    icmpService.startDiscoveryTask(deviceIP);

    while (!icmpService.isDiscoveryReady()) {
        // Display logs
        auto batch = icmpService.fetchICMPLog();
        for (auto& line : batch) {
            terminalView.println(line);
        }

        // Enter Press to stop
        int terminalKey = terminalInput.readChar();
        if (terminalKey == '\n' || terminalKey == '\r') {
            icmpService.stopICMPService();
            break;
        }
        char deviceKey = deviceInput.readChar();
        if (deviceKey == KEY_OK) {
            icmpService.stopICMPService();
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    delay(500);
    // Flush final logs
    for (auto& line : icmpService.fetchICMPLog()) {
        terminalView.println(line);
    }

    ICMPService::clearICMPLogging();
    icmpService.clearDiscoveryFlag();
    //terminalView.println(icmpService.getReport());
}

/*
Netcat
*/
void ANetworkController::handleNetcat(const TerminalCommand& cmd)
{
    // Check connection
    if (!wifiService.isConnected() && !ethernetService.isConnected())
    {
        terminalView.println("Netcat：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }
    // Args: nc <host> <port>
    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (cmd.getSubcommand().empty() || args.size() < 1) {
        terminalView.println("使用方法：nc <目标地址> <端口号>"); // 汉化
        return;
    }

    std::string host = cmd.getSubcommand();
    std::string portStr = args[0];

    if (!argTransformer.isValidNumber(portStr)) {
        terminalView.println("Netcat：无效的端口号。"); // 汉化
        return;
    }
    int port = argTransformer.parseHexOrDec16(portStr);
    if (port < 1 || port > 65535) {
        terminalView.println("Netcat：端口号必须在1到65535之间。"); // 汉化
        return;
    }

    terminalView.println("Netcat：正在连接 " + host + " ，端口号 " + portStr + "..."); // 汉化
    netcatService.startTask(host, 0, port, true);

    // Wait for connection
    unsigned long start = millis();
    while (!netcatService.isConnected() && millis() - start < 5000) {
        delay(50);
    }

    if (!netcatService.isConnected()) {
        terminalView.println("\r\nNetcat：连接失败。"); // 汉化
        netcatService.close();
        return;
    }

    terminalView.println("Netcat：已连接。Shell已启动... 按下[任意ESP32按键]停止。\n"); // 汉化

    while (true) {
        char deviceKey = deviceInput.readChar();
        if (deviceKey != KEY_NONE)
            break;

        char terminalKey = terminalInput.readChar();
        if (terminalKey == KEY_NONE) {
            std::string out = netcatService.readOutputNonBlocking();
            if (!out.empty()) terminalView.print(out);
            delay(10);
            continue;
        }

        netcatService.writeChar(terminalKey);
        terminalView.print(std::string(1, terminalKey)); // local echo
        if (terminalKey == '\r' || terminalKey == '\n') terminalView.println("");

        std::string output = netcatService.readOutputNonBlocking();
        if (!output.empty()) terminalView.print(output);
        delay(10);
    }

    netcatService.close();
    terminalView.println("\r\n\nNetcat：会话已关闭。"); // 汉化
}

/*
Nmap
*/
void ANetworkController::handleNmap(const TerminalCommand &cmd)
{
    // Check connection
    if (!wifiService.isConnected() && !ethernetService.isConnected())
    {
        terminalView.println("Nmap：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    auto args = argTransformer.splitArgs(cmd.getArgs());

    // Parse args
    // Parse hosts first
    auto hosts_arg = cmd.getSubcommand();
    
    // First helper invoke
    if (hosts_arg.compare("-h") == 0 || hosts_arg.compare("--help") == 0  || hosts_arg.empty()){
        terminalView.println(nmapService.getHelpText());
        return;
    }

    if(!nmapService.parseHosts(hosts_arg)) {
        terminalView.println("Nmap：无效的目标地址。"); // 汉化
        return;
    }

    // Check the first char of args is '-'
    if (!args.empty() && (args[0].empty() || args[0][0] != '-')) {
        terminalView.println("Nmap：参数必须以'-'开头（例如：-p 22）"); // 汉化
        return;
    }

    nmapService.setArgTransformer(argTransformer);
    auto tokens = argTransformer.splitArgs(cmd.getArgs());
    auto options = NmapService::parseNmapArgs(tokens);
    this->nmapService.setOptions(options);
    
    // Second helper
    if (options.help) {
        terminalView.println(nmapService.getHelpText());
        return;
    }

    if (options.hasTrash){
        // TODO handle this better
        //terminalView.println("Nmap: Invalid options.");
    }

    if (options.hasPort) {
        nmapService.setLayer4(options.tcp);
        // Parse ports
        if (!nmapService.parsePorts(options.ports)) {
            terminalView.println("Nmap：无效的-p参数值。请使用80,22,443或1000-2000格式。"); // 汉化
            return;
        }
    } else {
        nmapService.setLayer4(options.tcp);
        // Set the most popular ports
        nmapService.setDefaultPorts(options.tcp);
        terminalView.println("Nmap：使用前100个常用端口（可能需要几秒钟）"); // 汉化
    }

    // Re-use it for ICMP pings
    nmapService.setICMPService(&icmpService);
    nmapService.startTask(options.verbosity);
    
    while(!nmapService.isReady()){
        delay(100);
    }

    terminalView.println(nmapService.getReport());
    nmapService.clean();
    
    terminalView.println("\r\n\nNmap：扫描完成。"); // 汉化
}

/*
SSH
*/
void ANetworkController::handleSsh(const TerminalCommand &cmd)
{
    // Check connection
    if (!wifiService.isConnected() && !ethernetService.isConnected())
    {
        terminalView.println("SSH：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    // Check args
    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (cmd.getSubcommand().empty() || args.size() < 2)
    {
        terminalView.println("使用方法：ssh <目标地址> <用户名> <密码> [端口号]"); // 汉化
        return;
    }

    // Check port
    int port = 22;
    if (args.size() == 3)
    {
        if (argTransformer.isValidNumber(args[2]))
        {
            port = argTransformer.parseHexOrDec16(args[2]);
        }
    }

    std::string host = cmd.getSubcommand();
    std::string user = args[0];
    std::string pass = args[1];

    // Connect, start the ssh task
    terminalView.println("SSH：正在以" + user + "身份连接 " + host + " ，端口号 " + std::to_string(port) + "..."); // 汉化
    sshService.startTask(host, user, pass, false, port);

    // Wait 5sec for connection success
    unsigned long start = millis();
    while (!sshService.isConnected() && millis() - start < 5000)
    {
        delay(500);
    }

    // Can't connect
    if (!sshService.isConnected())
    {
        terminalView.println("\r\nSSH：连接失败。"); // 汉化
        sshService.close();
        return;
    }

    // Connected, start the bridge loop
    terminalView.println("SSH：已连接。Shell已启动... 按下[任意ESP32按键]停止。\n"); // 汉化
    while (true)
    {
        char terminalKey = terminalInput.readChar();
        if (terminalKey != KEY_NONE)
            sshService.writeChar(terminalKey);

        char deviceKey = deviceInput.readChar();
        if (deviceKey != KEY_NONE)
            break;

        std::string output = sshService.readOutputNonBlocking();
        if (!output.empty())
            terminalView.print(output);

        delay(10);
    }

    // Close SSH
    sshService.close();
    terminalView.println("\r\n\nSSH：会话已关闭。"); // 汉化
}

/*
HTTP
*/
void ANetworkController::handleHttp(const TerminalCommand &cmd)
{
    #ifndef DEVICE_M5STICK

    // Check connection
    if (!wifiService.isConnected() && !ethernetService.isConnected())
    {
        terminalView.println("HTTP：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    const auto sub = cmd.getSubcommand();

    // http get <url>
    if (sub == "get" && !cmd.getArgs().empty()) {
        handleHttpGet(cmd);
        return;
    // PH for POST, PUT, DELETE
    } else if (sub == "post" || sub == "put" || sub == "delete") {
        terminalView.println("HTTP：目前仅实现GET方法。"); // 汉化
        return;
    // http analyze <url>
    } else if (sub == "analyze") {
        handleHttpAnalyze(cmd);
        return;
    // http <url>
    } else if (!sub.empty() && cmd.getArgs().empty()) {
        handleHttpGet(cmd);
        return;
    } else {
        terminalView.println("使用方法：http <get|post|put|delete> <网址>"); // 汉化
    }

    #else

    terminalView.println("HTTP：不支持M5Stick设备。"); // 汉化

    #endif
}

/*
HTTP GET
*/
void ANetworkController::handleHttpGet(const TerminalCommand &cmd)
{
    if (cmd.getSubcommand() == "get" && cmd.getArgs().empty())
    {
        terminalView.println("使用方法：http get <网址>"); // 汉化
        return;
    }

    // Support for http <url> or http get <url>
    auto arg = cmd.getArgs().empty() ? cmd.getSubcommand() : cmd.getArgs();
    std::string url = argTransformer.ensureHttpScheme(arg);

    terminalView.println("HTTP：正在向 " + url + " 发送GET请求..."); // 汉化
    httpService.startGetTask(url, 10000, 8192, true, 30000);

    // Wait until timeout or response is ready
    const unsigned long deadline = millis() + 10000;
    while (!httpService.isResponseReady() && millis() < deadline) {
        delay(50);
    }

    if (httpService.isResponseReady()) {
        terminalView.println("\n========== HTTP GET =============");
        terminalView.println(argTransformer.normalizeLines(httpService.lastResponse()));
        terminalView.println("=================================\n");

    } else {
        terminalView.println("\nHTTP：错误，请求超时"); // 汉化
    }

    httpService.reset();
}

/*
HTTP Analayze
*/
void ANetworkController::handleHttpAnalyze(const TerminalCommand& cmd)
{
    if (cmd.getArgs().empty()) {
        terminalView.println("使用方法：http analyze <网址>"); // 汉化
        return;
    }

    // Ensure URL has HTTP scheme and then extract host
    const std::string url  = argTransformer.ensureHttpScheme(cmd.getArgs());
    const std::string host = argTransformer.extractHostFromUrl(url);
    std::vector<std::string> lines;
    std::string resp;

    // === urlscan.io (last public scan) ====
    const std::string urlscanUrl =
        "https://urlscan.io/api/v1/search?datasource=scans&q=page.domain:" + host + "&size=1";

    terminalView.println("HTTP分析：" + urlscanUrl + "（最新公开扫描）..."); // 汉化
    resp = httpService.fetchJson(urlscanUrl, 8192);
    terminalView.println("\n===== URLSCAN LATEST =====");
    lines = jsonTransformer.toLines(jsonTransformer.dechunk(resp));
    for (auto& l : lines) terminalView.println(l);
    terminalView.println("==========================\n");


    // === ssllabs.com ====
    const std::string ssllabsUrl =
        "https://api.ssllabs.com/api/v3/analyze?host=" + url;
        

    terminalView.println("HTTP分析：" + ssllabsUrl + "（SSL实验室）..."); // 汉化
    resp = httpService.fetchJson(ssllabsUrl, 16384);

    terminalView.println("\n===== SSL LABS =====");
    lines = jsonTransformer.toLines(jsonTransformer.dechunk(resp));
    for (auto& l : lines) terminalView.println(l);
    terminalView.println("====================\n");
    httpService.reset();

    // ==== W3C HTML Validator (optional) ====
    auto confirm = userInputManager.readYesNo("\nAnalyze with the W3C Validator?", false);
    if (confirm) {
        const std::string w3cUrl =
            "https://validator.w3.org/nu/?out=json&doc=" + url;

        terminalView.println("分析：" + w3cUrl + "（W3C验证器）..."); // 汉化
        resp = httpService.fetchJson(w3cUrl, 16384);
        terminalView.println("\n===== W3C RESULT =====");
        lines = jsonTransformer.toLines(jsonTransformer.dechunk(resp));
        for (auto& l : lines) terminalView.println(l);
        terminalView.println("======================\n");
        httpService.reset();
    }
    terminalView.println("\nHTTP分析：完成。"); // 汉化
}

/*
Lookup
*/
void ANetworkController::handleLookup(const TerminalCommand& cmd)
{
    # ifndef DEVICE_M5STICK

    if (!wifiService.isConnected() && !ethernetService.isConnected()) {
        terminalView.println("信息查询：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    const std::string sub = cmd.getSubcommand();
    if (sub == "mac") {
        handleLookupMac(cmd);
    } else if (sub == "ip") {
        handleLookupIp(cmd);
    } else {
        terminalView.println("使用方法：lookup mac <地址>"); // 汉化
        terminalView.println("       lookup ip <地址或网址>"); // 汉化
    }

    #else

    terminalView.println("信息查询：不支持M5Stick设备。"); // 汉化

    #endif
}

/*
Lookup MAC
*/
void ANetworkController::handleLookupMac(const TerminalCommand& cmd)
{
    if (cmd.getArgs().empty()) {
        terminalView.println("使用方法：lookup mac <MAC地址>"); // 汉化
        return;
    }

    const std::string mac = cmd.getArgs();
    const std::string url = "https://api.maclookup.app/v2/macs/" + mac;

    terminalView.println("MAC地址查询：" + url + " ..."); // 汉化

    std::string resp = httpService.fetchJson(url, 1024 * 4);

    terminalView.println("\n===== MAC LOOKUP =====");
    auto lines = jsonTransformer.toLines(resp);
    for (auto& l : lines) {
        terminalView.println(l);
    }
    terminalView.println("======================\n");

    httpService.reset();
}

/*
Lookup IP info
*/
void ANetworkController::handleLookupIp(const TerminalCommand& cmd)
{
    if (cmd.getArgs().empty()) {
        terminalView.println("使用方法：lookup ip <地址或网址>"); // 汉化
        return;
    }

    const std::string target = cmd.getArgs();
    const std::string url = "http://ip-api.com/json/" + target;
    const std::string url2 = "https://isc.sans.edu/api/ip/" + target + "?json";
    std::vector<std::string> lines;
    std::string resp;

    terminalView.println("IP地址查询：" + url + " ..."); // 汉化

    resp = httpService.fetchJson(url, 1024 * 4);
    terminalView.println("\n===== IP LOOKUP =====");
    lines = jsonTransformer.toLines(resp);
    for (auto& l : lines) terminalView.println(l);
    terminalView.println("=====================");

    resp = httpService.fetchJson(url2, 1024 * 4);
    lines = jsonTransformer.toLines(resp);
    for (auto& l : lines) terminalView.println(l);
    terminalView.println("=====================\n");

    httpService.reset();
}

/*
Telnet
*/
void ANetworkController::handleTelnet(const TerminalCommand &cmd)
{
    if (!wifiService.isConnected() && !ethernetService.isConnected()) {
        terminalView.println("TELNET：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    if (cmd.getSubcommand().empty()) {
        terminalView.println("使用方法：telnet <目标地址> [端口号]"); // 汉化
        return;
    }

    // Get host and port
    const std::string host = cmd.getSubcommand();
    uint16_t port = 23; // default telnet
    if (argTransformer.isValidNumber(cmd.getArgs())) {
        port = argTransformer.parseHexOrDec16(cmd.getArgs());
    }

    // Connect to telnet
    terminalView.println("TELNET：正在连接 " + host + " 的 " + std::to_string(port) + " 端口..."); // 汉化
    if (!telnetService.connectTo(host, static_cast<uint16_t>(port), 3000)) {
        terminalView.println("TELNET：连接失败：" + telnetService.lastError()); // 汉化
        return;
    }

    // Connection success
    terminalView.println("TELNET：已连接。Shell已启动... 按下[任意ESP32按键]停止。\n"); // 汉化
    while (true)
    {
        // terminal to telnet
        char k = terminalInput.readChar();
        if (k != KEY_NONE) telnetService.writeChar(k);

        // device button press to stop
        if (deviceInput.readChar() != KEY_NONE) break;

        // telnet to terminal
        telnetService.poll();
        std::string out = telnetService.readOutputNonBlocking();
        if (!out.empty()) terminalView.print(out);

        delay(5);
    }

    telnetService.close();
    terminalView.println("\r\n\nTELNET：会话已关闭。"); // 汉化
}

/*
Modbus
*/
void ANetworkController::handleModbus(const TerminalCommand &cmd)
{
    // Verify connection
    if (!wifiService.isConnected() && !ethernetService.isConnected()) {
        terminalView.println("Modbus：你必须先连接Wi-Fi或以太网，请先使用'connect'命令。"); // 汉化
        return;
    }

    // Verify host
    const std::string host = cmd.getSubcommand();
    if (host.empty()) {
        terminalView.println("使用方法：modbus <目标地址> [端口号]"); // 汉化
        return;
    }

    // Port
    uint16_t port = 502; // default modbus
    if (argTransformer.isValidNumber(cmd.getArgs())) {
        port = argTransformer.parseHexOrDec16(cmd.getArgs());
    }

    // Start shell
    terminalView.println("正在启动Modbus shell..."); // 汉化
    modbusShell.run(host, port);
}

/*
Help
*/
void ANetworkController::handleHelp()
{
    terminalView.println("  ping <目标地址>"); // 汉化
    terminalView.println("  discovery（设备发现）"); // 汉化
    terminalView.println("  ssh <目标地址> <用户名> <密码> [端口号]"); // 汉化
    terminalView.println("  telnet <目标地址> [端口号]"); // 汉化
    terminalView.println("  nc <目标地址> <端口号>"); // 汉化
    terminalView.println("  nmap <目标地址> [-p 端口范围]"); // 汉化
    terminalView.println("  modbus <目标地址> [端口号]"); // 汉化
    terminalView.println("  http get <网址>"); // 汉化
    terminalView.println("  http analyze <网址>"); // 汉化
    terminalView.println("  lookup mac <MAC地址>"); // 汉化
    terminalView.println("  lookup ip <地址或网址>"); // 汉化
}