#include "Controllers/EthernetController.h"

/*
Entry point for command
*/
void EthernetController::handleCommand(const TerminalCommand& cmd) {
    const auto& root = cmd.getRoot();

    if      (root == "config")    handleConfig();
    else if (root == "connect")   handleConnect();
    else if (root == "nc")        handleNetcat(cmd);
    else if (root == "nmap")      handleNmap(cmd);
    else if (root == "discovery") handleDiscovery(cmd);
    else if (root == "ping")      handlePing(cmd);
    else if (root == "ssh")       handleSsh(cmd);
    else if (root == "telnet")    handleTelnet(cmd);
    else if (root == "modbus")    handleModbus(cmd);
    else if (root == "http")      handleHttp(cmd);
    else if (root == "lookup")    handleLookup(cmd);
    else if (root == "status")    handleStatus();
    else if (root == "reset")     handleReset();
    else                          handleHelp();
}

/*
Connect using DHCP
*/
void EthernetController::handleConnect() {

    unsigned long timeoutMs = 5000;

    terminalView.println("以太网: DHCP获取中…"); // 汉化
    if (!ethernetService.beginDHCP(timeoutMs)) {
        if (!ethernetService.linkUp()) {
            terminalView.println("以太网: 无链路(网线未插)."); // 汉化
        } else {
            terminalView.println("以太网: DHCP获取失败."); // 汉化
        }
        return;
    }

    terminalView.println("\n=== 以太网: 通过DHCP连接成功 ==="); // 汉化
    terminalView.println("  IP地址 : " + ethernetService.getLocalIP()); // 汉化
    terminalView.println("  网关   : " + ethernetService.getGatewayIp()); // 汉化
    terminalView.println("  子网掩码 : " + ethernetService.getSubnetMask()); // 汉化
    terminalView.println("  DNS服务器 : " + ethernetService.getDns()); // 汉化
    terminalView.println("==============================\n");
}

/*
Config W5500
*/
void EthernetController::handleConfig() {
    terminalView.println("以太网(W5500)配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();
    uint8_t defCS   = state.getEthernetCsPin();
    uint8_t defRST  = state.getEthernetRstPin();
    uint8_t defSCK  = state.getEthernetSckPin();
    uint8_t defMISO = state.getEthernetMisoPin();
    uint8_t defMOSI = state.getEthernetMosiPin();
    uint8_t defIRQ  = state.getEthernetIrqPin();
    uint32_t defHz  = state.getEthernetFrequency();

    // User input for configuration
    uint8_t cs   = userInputManager.readValidatedPinNumber("W5500 CS引脚",   defCS,   forbidden); // 汉化
    uint8_t sck  = userInputManager.readValidatedPinNumber("W5500 SCK引脚",  defSCK,  forbidden); // 汉化
    uint8_t miso = userInputManager.readValidatedPinNumber("W5500 MISO引脚", defMISO, forbidden); // 汉化
    uint8_t mosi = userInputManager.readValidatedPinNumber("W5500 MOSI引脚", defMOSI, forbidden); // 汉化
    uint8_t irq  = userInputManager.readValidatedPinNumber("W5500 IRQ引脚",  defIRQ,  forbidden); // 汉化

    // RST optional
    bool useReset = userInputManager.readYesNo(
        "是否使用复位(RST)引脚?", // 汉化
        false
    );

    uint8_t rst = 255;
    if (useReset) {
        rst = userInputManager.readValidatedPinNumber("W5500 RST引脚", rst, forbidden); // 汉化
    }

    // Frequency SPI
    uint32_t hz = userInputManager.readValidatedUint32("SPI频率(赫兹)", defHz); // 汉化

    // MAC addr (optional)
    std::string macStr;
    std::array<uint8_t,6> mac = state.getEthernetMac();

    auto confirmation = userInputManager.readYesNo(
        "是否使用自定义MAC地址?", // 汉化
        false
    );

    // Ask for MAC if confirmed
    if (confirmation) {
        macStr = userInputManager.readValidatedHexString(
            "MAC地址(格式:DE AD BE EF 00 42)", // 汉化
            6,
            false
        );

        argTransformer.parseMac(macStr, mac);
    }

    // Save
    state.setEthernetCsPin(cs);
    state.setEthernetSckPin(sck);
    state.setEthernetMisoPin(miso);
    state.setEthernetMosiPin(mosi);
    state.setEthernetRstPin(useReset ? rst : 255);
    state.setEthernetIrqPin(irq);
    state.setEthernetFrequency(hz);
    state.setEthernetMac(mac);

    // Configure
    bool confirm = ethernetService.configure(
        cs,
        (useReset ? rst : -1),
        sck,
        miso,
        mosi,
        irq,
        hz,
        mac
    );

    if (confirm) {
        terminalView.println("\n ✅ W5500以太网已配置完成.\n"); // 汉化
        return;
    }

    terminalView.println("\n ❌ W5500以太网配置失败. 请检查接线.\n"); // 汉化
}

/*
W55000 Status
*/
void EthernetController::handleStatus() {
    const bool link      = ethernetService.linkUp();
    const bool connected = ethernetService.isConnected();

    const std::string mac = ethernetService.getMac();
    const std::string ip  = ethernetService.getLocalIP();
    const bool hasIp      = (ip != "0.0.0.0");

    terminalView.println("\n=== 以太网状态 ==="); // 汉化
    terminalView.println(std::string("  链路状态 : ") + (link ? "UP" : "DOWN")); // 汉化
    terminalView.println(std::string("  MAC地址  : ") + mac); // 汉化

    if (connected) {
        terminalView.println(std::string("  IP地址   : ") + ip); // 汉化
        terminalView.println(std::string("  子网掩码 : ") + ethernetService.getSubnetMask()); // 汉化
        terminalView.println(std::string("  网关     : ") + ethernetService.getGatewayIp()); // 汉化
        terminalView.println(std::string("  DNS服务器 : ") + ethernetService.getDns()); // 汉化
    } else if (link && !hasIp) {
        terminalView.println("  IP地址   : (等待DHCP分配)"); // 汉化
    } else if (!link) {
        terminalView.println("  IP地址   : (无链路)"); // 汉化
    } else {
        terminalView.println(std::string("  IP地址   : ") + ip); // 汉化
    }
    terminalView.println("========================\n");
}

/*
Reset
*/
void EthernetController::handleReset()
{
    ethernetService.hardReset();
    terminalView.println("以太网: 接口已重置. 已断开连接. [功能待实现]."); // 汉化
}

/*
Help
*/
void EthernetController::handleHelp() {
    terminalView.println("以太网命令:"); // 汉化
    terminalView.println("  status");
    terminalView.println("  connect");
    ANetworkController::handleHelp();
    terminalView.println("  reset");
    terminalView.println("  config");
}

/*
Ensure W5500 is configured
*/
void EthernetController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // Reconfigure in case these pins have been used somewhere else
    auto cs = state.getEthernetCsPin();
    auto sck = state.getEthernetSckPin();
    auto miso = state.getEthernetMisoPin();
    auto mosi = state.getEthernetMosiPin();
    auto rst = state.getEthernetRstPin();
    auto irq = state.getEthernetIrqPin();
    auto frequency = state.getEthernetFrequency();
    auto mac = state.getEthernetMac();

    ethernetService.configure(cs, rst, sck, miso, mosi, irq, frequency, mac);
}