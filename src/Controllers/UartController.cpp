#include "UartController.h"

/*
Constructor
*/
UartController::UartController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IInput& deviceInput,
    UartService& uartService,
    SdService& sdService,
    HdUartService& hdUartService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    UartAtShell& uartAtShell
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      deviceInput(deviceInput),
      uartService(uartService),
      sdService(sdService),
      hdUartService(hdUartService),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      uartAtShell(uartAtShell) 
{}


/*
Entry point for command
*/
void UartController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "scan") handleScan();
    else if (cmd.getRoot() == "ping") handlePing();
    else if (cmd.getRoot() == "read") handleRead();
    else if (cmd.getRoot() == "write") handleWrite(cmd);
    else if (cmd.getRoot() == "bridge") handleBridge();
    else if (cmd.getRoot() == "at") handleAtCommand(cmd);
    else if (cmd.getRoot() == "spam") handleSpam(cmd);
    else if (cmd.getRoot() == "glitch") handleGlitch();
    else if (cmd.getRoot() == "xmodem") handleXmodem(cmd);
    else if (cmd.getRoot() == "swap") handleSwap();
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Entry point for instructions
*/
void UartController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = uartService.executeByteCode(bytecodes);
    terminalView.println("");
    terminalView.print("UART 读取: "); // 汉化
    if (!result.empty()) {
        terminalView.println("");
        terminalView.println("");
        terminalView.println(result);
        uartService.clearUartBuffer();
    } else {
        terminalView.print("无数据"); // 汉化
    }
    terminalView.println("");
}

/*
Bridge
*/
void UartController::handleBridge() {
    terminalView.println("UART 桥接: 正在运行... 按下[任意ESP32按键]停止。\n"); // 汉化
    while (true) {
        // Read from UART and print to terminal
        if (uartService.available()) {
            char c = uartService.read();
            terminalView.print(std::string(1, c));
        }

        // Read from user input and write to UART
        char c = terminalInput.readChar();
        if (c != KEY_NONE) {
            uartService.write(c);
        }
        
        // Read from device input and stop bridge if any
        c = deviceInput.readChar();
        if (c != KEY_NONE) {  
            terminalView.println("\nUART 桥接: 已被用户停止。"); // 汉化
            break;
        }
    }
}

/*
Read
*/
void UartController::handleRead() {
    terminalView.println("UART 读取: 持续输出数据，按下[ENTER]停止..."); // 汉化
    uartService.flush();

    while (true) {
        // Stop if ENTER is pressed
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("");
            terminalView.println("UART 读取: 已被用户停止。"); // 汉化
            break;
        }

        // Print UART data as it comes
        while (uartService.available() > 0) {
            char c = uartService.read();
            terminalView.print(std::string(1, c));
        }
    }
}

/*
AT Command shell
*/
void UartController::handleAtCommand(const TerminalCommand& cmd) {
        uartAtShell.run();
}

/*
Write
*/
void UartController::handleWrite(TerminalCommand cmd) {
    std::string raw = cmd.getSubcommand() + cmd.getArgs();
    std::string decoded = argTransformer.decodeEscapes(raw);
    uartService.print(decoded);
    terminalView.println("UART 写入: 文本已发送，波特率 " + std::to_string(state.getUartBaudRate())); // 汉化
}

/*
Ping
*/
void UartController::handlePing() { 
    std::string response;
    unsigned long start = millis();
    size_t probeIndex = 0;

    terminalView.println("UART 探测: 正在探测5秒..."); // 汉化
    uartService.clearUartBuffer();

    while (millis() - start < 5000) {
        // Envoi progressif
        if (probeIndex < probes.size()) {
            uartService.write(probes[probeIndex]);
            probeIndex++;
        }

        // Lecture continue
        char c;
        while (uartService.available() > 0) {
            c = uartService.read();
            response += c;
        }

        delay(10);
    }

    // Analyse ASCII simple
    size_t asciiCount = 0;
    std::string result = "";
    for (char c : response)
        if (isprint(static_cast<unsigned char>(c)) || isspace(c)) {
            asciiCount++;
            result += c;
        }

    if (asciiCount < 5) {
        terminalView.println("UART 探测: 无响应。"); // 汉化
        return;
    }

    terminalView.println("UART 响应: "); // 汉化
    terminalView.println("");
    terminalView.println(result);
    terminalView.println("");

    terminalView.println("UART 探测: 检测到设备"); // 汉化
}

/*
Scan
*/
void UartController::handleScan() {
    terminalView.println("UART 扫描: 正在运行... 按下[ENTER]取消"); // 汉化
    terminalView.println("");
    terminalView.println("[提示]"); // 汉化
    terminalView.println("  UART扫描器会通过迭代切换波特率尝试检测正确值"); // 汉化
    terminalView.println("  并发送预定义的探测指令"); // 汉化
    terminalView.println("");

    uartService.clearUartBuffer();
    scanCancelled = false;

    for (int baud : baudrates) {
        if (scanCancelled) return;
        if (scanAtBaudrate(baud)) {
            state.setUartBaudRate(baud);
            uartService.switchBaudrate(baud);
            terminalView.println("");
            terminalView.println("UART 扫描: 已将波特率写入UART配置。"); // 汉化
            terminalView.println("UART 扫描: 检测到波特率 " + std::to_string(baud)); // 汉化
            terminalView.println("");
            return;
        }
    }

    uartService.switchBaudrate(state.getUartBaudRate()); // restore previous
    terminalView.println("UART 扫描: 未检测到设备。"); // 汉化
    terminalView.println("");
}

bool UartController::scanAtBaudrate(int baud) {
    const size_t maxResponseSize = 8192;
    uartService.switchBaudrate(baud);
    terminalView.println("→ 测试波特率 " + std::to_string(baud)); // 汉化
    uartService.clearUartBuffer();

    std::string response;
    size_t asciiCount = 0;
    size_t probeIndex = 0;
    unsigned long start = millis();

    while (millis() - start < 1500) {
        if (checkScanCancelled()) return false;
        sendNextProbe(probeIndex);
        updateResponse(response, asciiCount, maxResponseSize);

        if (isValidResponse(response, asciiCount)) {
            terminalView.println("");
            terminalView.println("预览:"); // 汉化
            auto cleaned = argTransformer.filterPrintable(response.substr(0, 100));
            terminalView.println(cleaned + "...");
            return true;
        }

        delay(10);
    }

    return false;
}

bool UartController::checkScanCancelled() {
    char key = terminalInput.readChar();
    if (key == '\r' || key == '\n') {
        terminalView.println("UART 扫描: 已被用户取消。"); // 汉化
        uartService.switchBaudrate(state.getUartBaudRate()); // restore previous
        scanCancelled = true;
        return true;
    }
    return false;
}

void UartController::sendNextProbe(size_t& probeIndex) {
    if (probeIndex < probes.size()) {
        uartService.write(probes[probeIndex]);
        probeIndex++;
    }
}

void UartController::updateResponse(std::string& response, size_t& asciiCount, size_t maxSize) {
    unsigned long readStart = millis();
    const unsigned long readTimeout = 150;  // ms
    while (uartService.available() > 0 && millis() - readStart < readTimeout) {
        char c = uartService.read();

        if (response.length() >= maxSize) {
            char dropped = response.front();
            if (isprint(dropped) || isspace(dropped)) asciiCount--;
            response.erase(0, 1);
        }

        if (isprint(c) || isspace(c)) asciiCount++;
        response += c;
    }
}

bool UartController::isValidResponse(const std::string& response, size_t asciiCount) {
    if (response.empty()) return false;

    float ratio = static_cast<float>(asciiCount) / response.size();
    float entropy = computeEntropy(response);

    bool plausibleLength = response.length() >= 32;
    bool readableEnough = ratio >= 0.85f;
    bool entropyOK = entropy >= 3.0f && entropy <= 7.5f;

    return plausibleLength && readableEnough && entropyOK;
}

float UartController::computeEntropy(const std::string& data) {
    std::unordered_map<char, size_t> freq;
    for (char c : data)
        freq[c]++;

    float entropy = 0.0f;
    for (auto& p : freq) {
        float prob = static_cast<float>(p.second) / data.size();
        entropy -= prob * std::log2(prob);
    }
    return entropy;
}

/*
Spam
*/
void UartController::handleSpam(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || cmd.getArgs().empty()) {
        terminalView.println("使用方法: spam <文本> <毫秒>"); // 汉化
        return;
    }

    // Find the last space to separate SSID and password
    std::string full = cmd.getSubcommand() + " " + cmd.getArgs();
    size_t pos = full.find_last_of(' ');
    if (pos == std::string::npos || pos == full.size() - 1) {
        terminalView.println("使用方法: spam <文本> <毫秒>"); // 汉化
        return;
    }
    auto textRaw = full.substr(0, pos);
    auto msRaw = full.substr(pos + 1);

    std::string text = argTransformer.decodeEscapes(textRaw);

    if (!argTransformer.isValidNumber(msRaw)) {
        terminalView.println("使用方法: spam <文本> <毫秒>"); // 汉化
        return;
    }

    uint32_t delayMs = argTransformer.toUint32(msRaw);
    unsigned long lastSend = 0;

    terminalView.println(
        "UART 持续发送: 每隔 " + std::to_string(delayMs) + 
        " 毫秒发送 \"" + text + 
        "\"，波特率 " + std::to_string(state.getUartBaudRate()) + 
        "... 按下[ENTER]停止。" // 汉化
    );

    while (true) {
        // Stop if ENTER pressed
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\nUART 持续发送: 已被用户停止。"); // 汉化
            break;
        }

        // Send if delay elapsed
        unsigned long now = millis();
        if (now - lastSend >= delayMs) {
            uartService.print(text);
            lastSend = now;
        }

        delay(1);
    }
}

/*
Xmodem
*/
void UartController::handleXmodem(const TerminalCommand& cmd) {
    std::string action = cmd.getSubcommand();
    std::string path = cmd.getArgs();

    if (action.empty()) {
        terminalView.println("使用方法: xmodem <接收/发送> <路径>"); // 汉化
        return;
    }

    if (path.empty()) {
        terminalView.println("错误: 缺少路径参数（示例: /file.txt）"); // 汉化
        return;
    }
    
    // Normalize path
    if (!path.empty() && path[0] != '/') {
        path = "/" + path;
    }

    terminalView.println("\nXMODEM 配置:"); // 汉化

    // Xmodem block size
    uint8_t defaultBlockSize = uartService.getXmodemBlockSize();
    uint8_t blockSize = userInputManager.readValidatedUint8("块大小（通常为128或1024）", defaultBlockSize, 1, 128); // 汉化
    uartService.setXmodemBlockSize(blockSize);

    // Xmodem id size
    uint8_t defaultIdSize = uartService.getXmodemIdSize();
    uint8_t idSize = userInputManager.readValidatedUint8("块ID大小（字节）(1-4)", defaultIdSize, 1, 4); // 汉化
    uartService.setXmodemIdSize(idSize);

    // Xmodem CRC
    bool useCrc = userInputManager.readYesNo("是否使用CRC校验？", true); // 汉化
    uartService.setXmodemCrc(useCrc);

    terminalView.println("\nXMODEM 配置完成\n"); // 汉化

    if (action == "recv") {
        handleXmodemReceive(path);
    } else if (action == "send") {
        handleXmodemSend(path);
    } else {
        terminalView.println("使用方法: xmodem <接收/发送> <路径>"); // 汉化
    }
}

void UartController::handleXmodemSend(const std::string& path) {
    // Open SD with SPI pin
    auto sdMounted = sdService.configure(state.getSpiCLKPin(), state.getSpiMISOPin(), 
                    state.getSpiMOSIPin(), state.getSpiCSPin());

    //Check SD mounted
    if (!sdMounted) {
        terminalView.println("UART XMODEM: 未检测到SD卡。请检查SPI引脚"); // 汉化
        return;
    }

    // Open the file
    File file = sdService.openFileRead(path);
    if (!file) {
        terminalView.println("UART XMODEM: 无法打开文件"); // 汉化
        return;
    }

    // Infos
    terminalView.println(" [提示]  WEBUI界面不会显示进度条。"); // 汉化
    terminalView.println("         进度条仅在USB串口可见。"); // 汉化
    terminalView.println("         文件传输期间请耐心等待。\n"); // 汉化
    std::stringstream ss;
    ss << "         预计传输时长: ~" 
    << (uint32_t)((file.size() * 10.0) / state.getUartBaudRate()) 
    << " 秒。\n";
    terminalView.println(ss.str());

    // Send it
    terminalView.println("UART XMODEM: 正在发送..."); // 汉化
    bool ok = uartService.xmodemSendFile(file);
    file.close();
    sdService.end();

    // Result
    terminalView.println(ok ? "\nUART XMODEM: 发送成功，文件已传输完成" : "\nUART XMODEM: 文件传输失败"); // 汉化

    // Close Xmodem
    uartService.end();
    ensureConfigured();

    // Close SD
    sdService.end();
}

void UartController::handleXmodemReceive(const std::string& path) {
    // Open sd card with SPI pin
    auto sdMounted = sdService.configure(state.getSpiCLKPin(), state.getSpiMISOPin(), 
                    state.getSpiMOSIPin(), state.getSpiCSPin());

    //Check SD mounted
    if (!sdMounted) {
        terminalView.println("UART XMODEM: 未检测到SD卡。请检查SPI引脚"); // 汉化
        return;
    }

    // Create target file
    File file = sdService.openFileWrite(path);
    if (!file) {
        terminalView.println("UART XMODEM: 无法创建文件。"); // 汉化
        return;
    }

    // Infos
    terminalView.println("");
    terminalView.println("  [提示] XMODEM接收模式为阻塞模式。"); // 汉化
    terminalView.println("         WEBUI界面不会显示进度条。"); // 汉化
    terminalView.println("         进度条仅在USB串口可见。"); // 汉化
    terminalView.println("         设备将等待传入数据"); // 汉化
    terminalView.println("         最长等待时间为2分钟。传输开始后，"); // 汉化
    terminalView.println("         必须完成传输才能退出。\n"); // 汉化

    // Receive
    terminalView.println("UART XMODEM: 正在接收..."); // 汉化
    bool ok = uartService.xmodemReceiveToFile(file);
    file.close();
    sdService.end();

    // Result
    terminalView.println("");
    terminalView.println(ok ? 
        ("UART XMODEM: 接收成功，文件已保存至 " + path) : // 汉化
        "UART XMODEM: 接收失败"); // 汉化

    // Close Xmodem
    uartService.end();
    ensureConfigured();

    // Close SD
    if (!ok) {
        sdService.deleteFile(path); // delete created file
    }
    sdService.end();
}

/*
Config
*/
void UartController::handleConfig() {
    terminalView.println("UART 配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t rxPin = userInputManager.readValidatedPinNumber("RX引脚编号", state.getUartRxPin(), forbidden); // 汉化
    state.setUartRxPin(rxPin);

    uint8_t txPin = userInputManager.readValidatedPinNumber("TX引脚编号", state.getUartTxPin(), forbidden); // 汉化
    state.setUartTxPin(txPin);

    uint32_t baud = userInputManager.readValidatedUint32("波特率", state.getUartBaudRate()); // 汉化
    state.setUartBaudRate(baud);

    uint8_t dataBits = userInputManager.readValidatedUint8("数据位 (5-8)", state.getUartDataBits(), 5, 8); // 汉化
    state.setUartDataBits(dataBits);

    char defaultParity = state.getUartParity().empty() ? 'N' : state.getUartParity()[0];
    char parityChar = userInputManager.readCharChoice("校验位 (N/E/O)", defaultParity, {'N', 'E', 'O'}); // 汉化
    state.setUartParity(std::string(1, parityChar));

    uint8_t stopBits = userInputManager.readValidatedUint8("停止位 (1或2)", state.getUartStopBits(), 1, 2); // 汉化
    state.setUartStopBits(stopBits);

    bool inverted = userInputManager.readYesNo("是否反转引脚？", state.isUartInverted()); // 汉化
    state.setUartInverted(inverted);

    uint32_t config = uartService.buildUartConfig(dataBits, parityChar, stopBits);
    state.setUartConfig(config);
    uartService.configure(baud, config, rxPin, txPin, inverted);

    terminalView.println("UART 配置完成。"); // 汉化
    terminalView.println("");
}

/*
Help
*/
void UartController::handleHelp() {
    terminalView.println("");
    terminalView.println("未知的UART命令。使用方法:"); // 汉化
    terminalView.println("  scan");
    terminalView.println("  ping");
    terminalView.println("  read");
    terminalView.println("  write <文本>"); // 汉化
    terminalView.println("  bridge");
    terminalView.println("  at");
    terminalView.println("  spam <文本> <毫秒>"); // 汉化
    terminalView.println("  glitch");
    terminalView.println("  xmodem recv <目标路径>"); // 汉化
    terminalView.println("  xmodem send <文件路径>"); // 汉化
    terminalView.println("  swap");
    terminalView.println("  config");
    terminalView.println("  原始指令格式, ['AT' D:100 r:128]"); // 汉化
    terminalView.println("");
}

/*
Glitch
*/
void UartController::handleGlitch() {
    terminalView.println("UART 毛刺攻击: 暂未实现"); // 汉化
}

/* 
Swap pins
*/
void UartController::handleSwap() {
    uint8_t rx = state.getUartRxPin();
    uint8_t tx = state.getUartTxPin();

    // Swap in state
    state.setUartRxPin(tx);
    state.setUartTxPin(rx);

    // Reconfigure UART with swapped pins
    uartService.end();

    uint32_t baud = state.getUartBaudRate();
    uint32_t config = state.getUartConfig();
    bool inverted = state.isUartInverted();

    uartService.configure(baud, config, state.getUartRxPin(), state.getUartTxPin(), inverted);

    terminalView.println(
        "UART 引脚交换: RX/TX已交换。RX=" + std::to_string(state.getUartRxPin()) + // 汉化
        " TX=" + std::to_string(state.getUartTxPin())
    );
    terminalView.println("");
}

/*
Ensure Config
*/
void UartController::ensureConfigured() {
    // hdUartService.end() // It crashed the app for some reasons, not rly needed

    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // User could have set the same pin to a different usage
    // eg. select UART, then select I2C, then select UART
    // Always reconfigure pins before use
    uartService.end();

    uint8_t rx = state.getUartRxPin();
    uint8_t tx = state.getUartTxPin();
    uint32_t baud = state.getUartBaudRate();
    uint32_t config = state.getUartConfig();
    bool inverted = state.isUartInverted();

    uartService.configure(baud, config, rx, tx, inverted);
}