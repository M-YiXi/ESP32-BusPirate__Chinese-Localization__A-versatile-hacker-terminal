#include "SpiController.h"

/*
Constructor
*/
SpiController::SpiController(ITerminalView& terminalView, IInput& terminalInput, 
                             SpiService& spiService, SdService& sdService, ArgTransformer& argTransformer,
                             UserInputManager& userInputManager, BinaryAnalyzeManager& binaryAnalyzeManager,
                             SdCardShell& sdCardShell, SpiFlashShell& spiFlashShell, SpiEepromShell& spiEepromShell)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      spiService(spiService),
      sdService(sdService),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      binaryAnalyzeManager(binaryAnalyzeManager),
      sdCardShell(sdCardShell),
      spiFlashShell(spiFlashShell),
      spiEepromShell(spiEepromShell)
{}

/*
Entry point for command
*/
void SpiController::handleCommand(const TerminalCommand& cmd) {
    if      (cmd.getRoot() == "sniff")  handleSniff();
    else if (cmd.getRoot() == "sdcard") handleSdCard();
    else if (cmd.getRoot() == "slave")  handleSlave();
    else if (cmd.getRoot() == "flash")  handleFlash(cmd);
    else if (cmd.getRoot() == "eeprom") handleEeprom(cmd);
    else if (cmd.getRoot() == "help")   handleHelp();
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Entry point for instructions
*/
void SpiController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = spiService.executeByteCode(bytecodes);
    if (!result.empty()) {
        terminalView.println("SPI读取:\n"); // 汉化
        terminalView.println(result);
    }
}

/*
Sniff
*/
void SpiController::handleSniff() {
    #ifdef DEVICE_M5STICK

        terminalView.println("SPI嗅探器: 由于SPI总线共享，M5Stick设备不支持该功能。"); // 汉化
        return;

    #endif

    // Select line
    std::vector<std::string> choices = { " MOSI", " MISO" };
    int choice = userInputManager.readValidatedChoiceIndex("选择要嗅探的线路", choices, 0); // 汉化
    bool sniffMosi = (choice == 0);

    // Pins
    int sclk = state.getSpiCLKPin();
    int miso = state.getSpiMISOPin();
    int mosi = state.getSpiMOSIPin();
    int cs   = state.getSpiCSPin();

    // Release SPI if in use
    spiService.end();

    // Mapping 
    int slaveMisoPin = sniffMosi ? miso : -1;
    int slaveMosiPin = sniffMosi ? mosi : miso;

    terminalView.println("SPI嗅探器: 正在运行... 按下[ENTER]停止。"); // 汉化

    terminalView.println("");
    terminalView.println("  [提示]"); // 汉化
    terminalView.println("    SPI嗅探模式被动监听SPI总线。"); // 汉化
    terminalView.println("    请将SCK、MOSI、MISO和CS线路连接到Bus Pirate。"); // 汉化
    terminalView.println("    仅当CS（片选）引脚激活时才会捕获数据。"); // 汉化
    terminalView.println("");

    // Launch SPI slave on the selected line
    spiService.startSlave(sclk, slaveMisoPin, slaveMosiPin, cs);

    // Log data until user stops
    const char* tag = sniffMosi ? "[MOSI] " : "[MISO] ";
    while (true) {
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        auto packets = spiService.getSlaveData();
        for (const auto& packet : packets) {
            if (packet.empty()) continue;
            std::stringstream ss;
            ss << tag;
            for (uint8_t b : packet) {
                ss << std::hex << std::uppercase
                   << std::setw(2) << std::setfill('0') << (int)b << " ";
            }
            terminalView.println(ss.str());
        }
    }

    terminalView.println("\nSPI嗅探器: 正在停止... 请稍候。"); // 汉化
    spiService.stopSlave(sclk, slaveMisoPin, slaveMosiPin, cs);
    spiService.end();
    spiService.configure(mosi, miso, sclk, cs, state.getSpiFrequency());
    terminalView.println("SPI嗅探器: 已被用户停止。\n"); // 汉化
}

/*
Flash
*/
void SpiController::handleFlash(const TerminalCommand& cmd) {
    spiFlashShell.run();
}

/*
EEPROM
*/
void SpiController::handleEeprom(const TerminalCommand& cmd) {
    spiEepromShell.run();
    ensureConfigured();
}

/*
Slave
*/
void SpiController::handleSlave() {
    #ifdef DEVICE_M5STICK

    terminalView.println("SPI从机模式: 由于SPI总线共享，M5Stick设备不支持该功能。"); // 汉化
    return;

    #endif

    spiService.end(); // Stop master mode if active
    
    int sclk = state.getSpiCLKPin();
    int miso = state.getSpiMISOPin();
    int mosi = state.getSpiMOSIPin();
    int cs   = state.getSpiCSPin();

    terminalView.println("SPI从机模式: 正在运行... 按下[ENTER]停止。"); // 汉化
    spiService.startSlave(sclk, miso, mosi, cs);

    terminalView.println("");
    terminalView.println("  [提示]"); // 汉化
    terminalView.println("    SPI从机模式被动监听SPI总线。"); // 汉化
    terminalView.println("    SPI主机发送的所有命令都会被捕获并记录"); // 汉化
    terminalView.println("    仅当CS（片选）引脚激活时才会捕获数据。"); // 汉化
    terminalView.println("");

    while (true) {
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Read slave data from master
        auto packets = spiService.getSlaveData();
        for (const auto& packet : packets) {
            std::stringstream ss;
            ss << "[MOSI] ";
            for (uint8_t b : packet) {
                ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)b << " ";
            }
            terminalView.println(ss.str());
        }
    }
    terminalView.println("\nSPI从机模式: 正在停止... 请稍候。"); // 汉化
    spiService.stopSlave(sclk, miso, mosi, cs);
    spiService.end();
    spiService.configure(mosi, miso, sclk, cs, state.getSpiFrequency());
    terminalView.println("SPI从机模式: 已被用户停止。\n"); // 汉化
}

/*
SD Card
*/
void SpiController::handleSdCard() {
    uint8_t cs, clk, miso, mosi;
    cs = state.getSpiCSPin();
    clk = state.getSpiCLKPin();
    miso = state.getSpiMISOPin();
    mosi = state.getSpiMOSIPin();

    // Internal SD card slot
    if (state.getHasInternalSdCard()) {
        auto confirm = userInputManager.readYesNo("使用内置SD卡插槽？", true); // 汉化
        if (confirm) {
            cs = state.getSdCardCsPin();
            clk = state.getSdCardClkPin();
            miso = state.getSdCardMisoPin();
            mosi = state.getSdCardMosiPin();
        }
    }

    terminalView.println("SD卡: 正在挂载..."); // 汉化
    delay(500);

    // Configure
    spiService.end();
    bool success = sdService.configure(
        clk,
        miso,
        mosi,
        cs
    );

    if (!success) {
        terminalView.println("SD卡: 挂载失败。请检查配置和接线后重试。\n"); // 汉化
        return;
    }
    
    // SD Shell
    terminalView.println("SD卡: 挂载成功。正在加载交互界面...\n"); // 汉化
    sdCardShell.run();

    // Reconfigure
    sdService.end();
    spiService.end();
    ensureConfigured();
}

/*
Help
*/
void SpiController::handleHelp() {
    terminalView.println("");
    terminalView.println("未知的SPI命令。使用方法:"); // 汉化
    terminalView.println("  sniff");
    terminalView.println("  sdcard");
    terminalView.println("  slave");
    terminalView.println("  flash");
    terminalView.println("  eeprom");
    terminalView.println("  config");
    terminalView.println("  原始指令示例: [0x9F r:3]"); // 汉化
    terminalView.println("");
}

/*
Config
*/
void SpiController::handleConfig() {
    terminalView.println("SPI配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t mosi = userInputManager.readValidatedPinNumber("MOSI引脚", state.getSpiMOSIPin(), forbidden); // 汉化
    state.setSpiMOSIPin(mosi);

    uint8_t miso = userInputManager.readValidatedPinNumber("MISO引脚", state.getSpiMISOPin(), forbidden); // 汉化
    state.setSpiMISOPin(miso);

    uint8_t sclk = userInputManager.readValidatedPinNumber("SCLK引脚", state.getSpiCLKPin(), forbidden); // 汉化
    state.setSpiCLKPin(sclk);

    uint8_t cs = userInputManager.readValidatedPinNumber("CS引脚", state.getSpiCSPin(), forbidden); // 汉化
    state.setSpiCSPin(cs);

    uint32_t freq = userInputManager.readValidatedUint32("频率", state.getSpiFrequency()); // 汉化
    state.setSpiFrequency(freq);

    spiService.configure(mosi, miso, sclk, cs, freq);

    terminalView.println("SPI配置完成。\n"); // 汉化
}

/*
Ensure SPI is configured
*/
void SpiController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }

    spiService.end();
    sdService.end();
    // Reconfigure, user could have used these pins for another mode
    uint8_t sclk = state.getSpiCLKPin();
    uint8_t miso = state.getSpiMISOPin();
    uint8_t mosi = state.getSpiMOSIPin();
    uint8_t cs   = state.getSpiCSPin();
    int freq   = state.getSpiFrequency();
    spiService.configure(mosi, miso, sclk, cs, freq);
}