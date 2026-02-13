#include "I2cController.h"

/*
Constructor
*/
I2cController::I2cController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    I2cService& i2cService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    I2cEepromShell& eepromShell
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      i2cService(i2cService),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      eepromShell(eepromShell)
{}

/*
Entry point to handle I2C command
*/
void I2cController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "scan") handleScan();
    else if (cmd.getRoot() == "sniff") handleSniff();
    else if (cmd.getRoot() == "ping") handlePing(cmd);
    else if (cmd.getRoot() == "identify") handleIdentify(cmd);
    else if (cmd.getRoot() == "write") handleWrite(cmd);
    else if (cmd.getRoot() == "read") handleRead(cmd);
    else if (cmd.getRoot() == "dump") handleDump(cmd);
    else if (cmd.getRoot() == "slave") handleSlave(cmd);
    else if (cmd.getRoot() == "glitch") handleGlitch(cmd);
    else if (cmd.getRoot() == "flood") handleFlood(cmd);
    else if (cmd.getRoot() == "jam") handleJam();
    else if (cmd.getRoot() == "eeprom") handleEeprom(cmd);
    else if (cmd.getRoot() == "recover") handleRecover();
    else if (cmd.getRoot() == "monitor") handleMonitor(cmd);
    else if (cmd.getRoot() == "swap") handleSwap();
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Entry point to handle I2C instruction
*/
void I2cController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = i2cService.executeByteCode(bytecodes);
    if (!result.empty()) {
        terminalView.println("I2C读取:\n"); // 汉化
        terminalView.println(result);
    }
}

/*
Scan
*/
void I2cController::handleScan() {
    terminalView.println("I2C扫描: 正在扫描I2C总线... 按下[ENTER]停止"); // 汉化
    terminalView.println("");
    bool found = false;

    for (uint8_t addr = 1; addr < 127; ++addr) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("I2C扫描: 已被用户取消."); // 汉化
            return;
        }
        
        i2cService.beginTransmission(addr);
        if (i2cService.endTransmission() == 0) {
            std::stringstream ss;
            ss << "在0x" << std::hex << std::uppercase << (int)addr << "发现设备"; // 汉化
            terminalView.println(ss.str());
            found = true;
        }
    }

    if (!found) {
        terminalView.println("I2C扫描: 未发现任何I2C设备."); // 汉化
    }
    terminalView.println("");
}

/*
Sniff
*/    
void I2cController::handleSniff() {
    terminalView.println("I2C嗅探: 监听SCL/SDA总线... 按下[ENTER]停止.\n"); // 汉化
    i2c_sniffer_begin(state.getI2cSclPin(), state.getI2cSdaPin()); // dont need freq to work
    i2c_sniffer_setup();

    std::string line;

    while (true) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;

        while (i2c_sniffer_available()) {
            char c = i2c_sniffer_read();

            if (c == '\n') {
                line += "  ";
                terminalView.println(line);
                line.clear();
            } else {
                line += c;
            }
        }
        delayMicroseconds(100);
    }

    i2c_sniffer_reset_buffer();
    i2c_sniffer_stop();
    i2cService.configure(state.getI2cSdaPin(), state.getI2cSclPin(), state.getI2cFrequency());
    terminalView.println("\n\nI2C嗅探: 已停止."); // 汉化
}

/*
Ping
*/
void I2cController::handlePing(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty()) {
        terminalView.println("使用方法: ping <I2C地址>"); // 汉化
        return;
    }

    const std::string& arg = cmd.getSubcommand();
    uint8_t address = 0;

    std::stringstream ss(arg);
    int temp = 0;

    // Detect hex prefix
    if (arg.rfind("0x", 0) == 0 || arg.rfind("0X", 0) == 0) {
        ss >> std::hex >> temp;
    } else {
        ss >> std::dec >> temp;
    }

    if (ss.fail() || temp < 0 || temp > 127) {
        terminalView.println("I2C Ping: 无效的地址格式. 使用十六进制(例如 0x3C)."); // 汉化
        return;
    }

    address = static_cast<uint8_t>(temp);

    std::stringstream result;
    result << "Ping 0x" << std::hex << std::uppercase << (int)address << ": "; // 汉化

    i2cService.beginTransmission(address);
    uint8_t i2cResult = i2cService.endTransmission();

    if (i2cResult == 0) {
        result << "I2C Ping: 收到ACK响应! 设备存在."; // 汉化
    } else {
        result << "I2C Ping: 无响应(NACK或错误)."; // 汉化
    }

    terminalView.println(result.str());
}

/*
Write
*/
void I2cController::handleWrite(const TerminalCommand& cmd) {
    auto args = argTransformer.splitArgs(cmd.getArgs());

    if (args.size() < 2) {
        terminalView.println("使用方法: write <地址> <寄存器> <值>"); // 汉化
        return;
    }

    // Args
    const std::string& addrStr = cmd.getSubcommand();
    const std::string& regStr = args[0];
    const std::string& valStr = args[1];

    // Verify inputs
    if (!argTransformer.isValidNumber(addrStr) ||
        !argTransformer.isValidNumber(regStr) ||
        !argTransformer.isValidNumber(valStr)) {
        terminalView.println("错误: 无效的参数. 使用十进制或带0x前缀的十六进制值."); // 汉化
        return;
    }

    // Parse input
    uint8_t addr = argTransformer.parseHexOrDec(addrStr);
    uint8_t reg  = argTransformer.parseHexOrDec(regStr);
    uint8_t val  = argTransformer.parseHexOrDec(valStr);

    // Ping addr
    i2cService.beginTransmission(addr);
    uint8_t pingResult = i2cService.endTransmission();
    
    // Check ping
    if (pingResult != 0) {
        std::stringstream error;
        error << "I2C Ping: 0x" << std::hex << std::uppercase << (int)addr
              << " 无响应. 终止写入操作."; // 汉化
        terminalView.println(error.str());
        return;
    }

    // Write
    i2cService.beginTransmission(addr);
    i2cService.write(reg);
    i2cService.write(val);
    i2cService.endTransmission();

    terminalView.println("I2C写入: 数据已发送."); // 汉化
}

/*
Read
*/
void I2cController::handleRead(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty()) {
        terminalView.println("使用方法: read <地址> <寄存器>"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand()) ||
        !argTransformer.isValidNumber(cmd.getArgs())) {
        terminalView.println("错误: 无效的参数. 使用十进制或带0x前缀的十六进制值."); // 汉化
        return;
    }

    uint8_t addr = argTransformer.parseHexOrDec(cmd.getSubcommand());
    uint8_t reg  = argTransformer.parseHexOrDec(cmd.getArgs());

    // Check I2C device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C读取: 在" + cmd.getSubcommand() + "地址未发现设备"); // 汉化
        return;
    }

    // Write register address first
    i2cService.beginTransmission(addr);
    i2cService.write(reg);
    i2cService.endTransmission(false);

    i2cService.requestFrom(addr, 1);
    if (i2cService.available()) {
        int value = i2cService.read();
        std::stringstream ss;
        ss << "0x" << std::hex << std::uppercase << value;
        terminalView.println("读取结果: " + ss.str()); // 汉化
    } else {
        terminalView.println("I2C读取: 无可用数据."); // 汉化
    }
}

/*
Config
*/
void I2cController::handleConfig() {
    terminalView.println("I2C配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t sda = userInputManager.readValidatedPinNumber("SDA引脚", state.getI2cSdaPin(), forbidden); // 汉化
    state.setI2cSdaPin(sda);

    uint8_t scl = userInputManager.readValidatedPinNumber("SCL引脚", state.getI2cSclPin(), forbidden); // 汉化
    state.setI2cSclPin(scl);

    uint32_t freq = userInputManager.readValidatedUint32("频率", state.getI2cFrequency()); // 汉化
    state.setI2cFrequency(freq);

    i2cService.configure(sda, scl, freq);

    terminalView.println("I2C已配置完成.\n"); // 汉化
}

/*
Slave
*/
void I2cController::handleSlave(const TerminalCommand& cmd) {
    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: slave <地址>"); // 汉化
        return;
    }

    // Parse arg
    uint8_t addr = argTransformer.parseHexOrDec(cmd.getSubcommand());
    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();

    // Validate arg
    if (addr < 0x08 || addr > 0x77) {
        terminalView.println("I2C从机: 无效的地址. 必须在0x08到0x77之间."); // 汉化
        return;
    }

    terminalView.println("I2C从机: 监听地址0x" + argTransformer.toHex(addr) +
                         "... 按下[ENTER]停止.\n"); // 汉化
    
    // Start slave
    i2cService.clearSlaveLog();
    i2cService.beginSlave(addr, sda, scl);

    std::vector<std::string> lastLog;
    while (true) {
        // Enter press
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;

        // Get master log from slave and display it
        auto currentLog = i2cService.getSlaveLog();
        if (currentLog.size() > lastLog.size()) {
            for (size_t i = lastLog.size(); i < currentLog.size(); ++i) {
                terminalView.println(currentLog[i]);
            }
            lastLog = currentLog;
        }
        delay(1);
    }

    // Close slave
    i2cService.endSlave();
    ensureConfigured();
    terminalView.println("\nI2C从机: 已被用户停止."); // 汉化
}

/*
Dump
*/
void I2cController::handleDump(const TerminalCommand& cmd) {
    // Validate sub
    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: dump <地址> [长度]"); // 汉化
        return;
    }

    // Parse addr
    uint8_t addr = argTransformer.parseHexOrDec(cmd.getSubcommand());
    uint16_t start = 0x00;
    uint16_t len = 256;

    // Check I2C device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C数据导出: 在" + cmd.getSubcommand() + "地址未发现设备"); // 汉化
        return;
    }
    
    // Validate and parse arg
    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (args.size() >= 1 && argTransformer.isValidNumber(args[0])) {
        len = argTransformer.parseHexOrDec16(args[0]);
    }

    std::vector<uint8_t> values(len, 0xFF);
    std::vector<bool> valid(len, false);

    // Device registers are readable
    if (i2cService.isReadableDevice(addr, start)) {
        terminalView.println("I2C数据导出: 0x" + argTransformer.toHex(addr) +
                             " 从0x" + argTransformer.toHex(start) +
                             "开始读取" + std::to_string(len) + "字节... 按下[ENTER]停止.\n"); // 汉化

        performRegisterRead(addr, start, len, values, valid);

    // Not readable
    } else {
        terminalView.println("I2C数据导出: 地址0x" + argTransformer.toHex(addr) +
                             "的设备可能不支持标准寄存器访问 — 尝试原始读取..."); // 汉化

        performRawRead(addr, start, len, values, valid);
    }

    // Not able to read any data
    if (std::all_of(valid.begin(), valid.end(), [](bool b) { return !b; })) {
        terminalView.println("I2C数据导出: 无法读取任何数据 — 设备返回NACK或不支持该协议.\n"); // 汉化
        return;
    }

    printHexDump(start, len, values, valid);
}

void I2cController::performRegisterRead(uint8_t addr, uint16_t start, uint16_t len,
                                        std::vector<uint8_t>& values, std::vector<bool>& valid) {
    const uint8_t CHUNK_SIZE = 16;
    const bool use16bitAddr = (start + len - 1) > 0xFF;
    int consecutiveErrors = 0;

    for (uint16_t offset = 0; offset < len; offset += CHUNK_SIZE) {
        if (consecutiveErrors >= 3) {
            terminalView.println("I2C数据导出: 连续3次错误 已终止."); // 汉化
            return;
        }

        uint16_t reg = start + offset;
        uint8_t toRead = (offset + CHUNK_SIZE <= len) ? CHUNK_SIZE : (len - offset);

        // Write register address (1 or 2 bytes)
        i2cService.beginTransmission(addr);
        if (use16bitAddr) {
            i2cService.write((reg >> 8) & 0xFF); // MSB
            i2cService.write(reg & 0xFF);        // LSB
        } else {
            i2cService.write((uint8_t)(reg & 0xFF));
        }
        bool writeOk = (i2cService.endTransmission(false) == 0);  // No stop
        if (!writeOk) {
            consecutiveErrors++;
            continue;
        }

        // Read chunk
        uint8_t received = i2cService.requestFrom(addr, toRead, true);
        if (received == toRead) {
            for (uint8_t i = 0; i < toRead; ++i) {
                char key = terminalInput.readChar();
                if (key == '\r' || key == '\n') {
                    terminalView.println("I2C数据导出: 已被用户取消."); // 汉化
                    return;
                }

                if (i2cService.available()) {
                    values[offset + i] = i2cService.read();
                    valid[offset + i] = true;
                }
            }
            consecutiveErrors = 0;
        } else {
            while (i2cService.available()) i2cService.read();  // Flush
            consecutiveErrors++;
        }

        delay(1);
    }
}

void I2cController::performRawRead(uint8_t addr, uint16_t start,
                                   uint16_t len,
                                   std::vector<uint8_t>& values,
                                   std::vector<bool>& valid) {
    values.assign(len, 0xFF);
    valid.assign(len, false);

    terminalView.println("I2C数据导出: 尝试原始读取..."); // 汉化

    // Write start register
    i2cService.beginTransmission(addr);
    i2cService.write(start);
    if (i2cService.endTransmission(false) != 0) {
        return;  // NACK
    }

    // Read len from register addr
    uint16_t received = i2cService.requestFrom(addr, (uint8_t)len, true);
    for (uint16_t i = 0; i < received && i < len; ++i) {
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("I2C数据导出: 已被用户取消."); // 汉化
            return;
        }
        if (i2cService.available()) {
            values[i] = i2cService.read();
            valid[i] = true;
        }
    }

    while (i2cService.available()) i2cService.read();
}

void I2cController::printHexDump(uint16_t start, uint16_t len,
                                 const std::vector<uint8_t>& values, const std::vector<bool>& valid) {
    for (uint16_t lineStart = 0; lineStart < len; lineStart += 16) {
        std::string line;
        char addrStr[8];
        snprintf(addrStr, sizeof(addrStr), "%02X:", start + lineStart);
        line += addrStr;

        for (uint8_t i = 0; i < 16; ++i) {
            uint16_t idx = lineStart + i;
            if (idx < len) {
                if (valid[idx]) {
                    char hex[4];
                    snprintf(hex, sizeof(hex), " %02X", values[idx]);
                    line += hex;
                } else {
                    line += " ??";
                }
            } else {
                line += "   ";
            }
        }

        line += "  ";

        for (uint8_t i = 0; i < 16; ++i) {
            uint16_t idx = lineStart + i;
            if (idx < len && valid[idx]) {
                char c = values[idx];
                line += (c >= 32 && c <= 126) ? c : '.';
            } else {
                line += '.';
            }
        }

        terminalView.println(line);
    }
    terminalView.println("");
}

/*
Identify
*/
void I2cController::handleIdentify(const TerminalCommand& cmd) {
    // Validate subcommand
    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: identify <地址>"); // 汉化
        return;
    }

    // Parse I2C address
    uint8_t address = argTransformer.parseHexOrDec(cmd.getSubcommand());
    uint16_t start = 0x00;
    uint16_t len = 256;

    std::stringstream ss;
    ss << "\n\r 📟 I2C 0x" + argTransformer.toHex(address) + " 设备识别结果\n"; // 汉化

    // Search for known addresses
    bool matchFound = false;
    for (size_t i = 0; i < i2cknownAddressesCount; ++i) {
        if (i2cKnownAddresses[i].address == address) {
            matchFound = true;
            ss << "\r  ➤ 可能是: - [" << i2cKnownAddresses[i].type << "] " << i2cKnownAddresses[i].component << "\n"; // 汉化
        }
    }

    if (!matchFound) {
        ss << "\r  ➤ 在地址0x" << argTransformer.toHex(address) << "未找到匹配设备\n"; // 汉化
    }

    terminalView.println(ss.str());
}

/*
Recover
*/
void I2cController::handleRecover() {
    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();
    uint32_t freq = state.getI2cFrequency();

    terminalView.println("I2C重置: 尝试恢复I2C总线..."); // 汉化

    // Release I2C bus
    i2cService.end();
    // 16 clock pulse + STOP condition
    bool success = i2cService.i2cBitBangRecoverBus(scl, sda, freq);
    // Reconfigure I2C
    i2cService.configure(sda, scl, freq);

    if (success) {
        terminalView.println("\nI2C重置: SDA已释放. 总线恢复成功."); // 汉化
    } else {
        terminalView.println("\nI2C重置: 恢复后SDA仍为低电平, 总线可能仍处于卡死状态."); // 汉化
    }
}

/*
Glitch
*/
void I2cController::handleGlitch(const TerminalCommand& cmd) {
    // Validate arg
    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: glitch <地址>"); // 汉化
        return;
    }

    // Parse and get I2C default config
    uint8_t addr = argTransformer.parseHexOrDec(cmd.getSubcommand());
    uint8_t scl = state.getI2cSclPin();
    uint8_t sda = state.getI2cSdaPin();
    uint32_t freqHz = state.getI2cFrequency();

    // Check I2C device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C干扰: 在" + cmd.getSubcommand() + "地址未发现设备"); // 汉化
        return;
    }

    terminalView.println("I2C干扰: 攻击地址0x" + argTransformer.toHex(addr) + "的设备...\n"); // 汉化
    delay(500);

    terminalView.println(" 1. 发送随机无效数据..."); // 汉化
    i2cService.floodRandom(addr, freqHz, scl, sda);
    delay(50);

    terminalView.println(" 2. 发送大量START序列..."); // 汉化
    i2cService.floodStart(addr, freqHz, scl, sda);
    delay(50);

    terminalView.println(" 3. 过度读取(读取超出预期的字节数)..."); // 汉化
    i2cService.overReadAttack(addr, freqHz, scl, sda);
    delay(50);

    terminalView.println(" 4. 读取无效/未映射的寄存器..."); // 汉化
    i2cService.invalidRegisterRead(addr, freqHz, scl, sda);
    delay(50);

    terminalView.println(" 5. 模拟时钟拉伸干扰..."); // 汉化
    i2cService.simulateClockStretch(addr, freqHz, scl, sda);
    delay(50);

    terminalView.println(" 6. 快速发送START/STOP序列..."); // 汉化
    i2cService.rapidStartStop(addr, freqHz, scl, sda);
    delay(50);

    terminalView.println(" 7. 干扰ACK阶段..."); // 汉化
    i2cService.glitchAckInjection(addr, freqHz, scl, sda);
    delay(50);

    terminalView.println(" 8. 在SCL/SDA总线上注入随机噪声..."); // 汉化
    i2cService.randomClockPulseNoise(scl, sda, freqHz);
    delay(50);

    ensureConfigured();
    terminalView.println("\nI2C干扰: 完成. 目标设备可能无响应或数据损坏."); // 汉化
}

/*
Flood
*/
void I2cController::handleFlood(const TerminalCommand& cmd) {
    // Validate arg
    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: flood <地址>"); // 汉化
        return;
    }

    // Parse arg
    uint8_t addr = argTransformer.parseHexOrDec(cmd.getSubcommand());
    
    // Check device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C泛洪: 在" + cmd.getSubcommand() + "地址未发现设备"); // 汉化
        return;
    }
    
    terminalView.println("I2C泛洪: 持续读取地址0x" + argTransformer.toHex(addr) + "... 按下[ENTER]停止."); // 汉化
    while (true) {
        // Enter to stop
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\nI2C泛洪: 已被用户停止."); // 汉化
            break;
        }

        // Random register address
        uint8_t reg = esp_random() & 0xFF;

        // Transmit only register
        i2cService.beginTransmission(addr);
        i2cService.write(reg);
        i2cService.endTransmission(true);
    }
}

/*
Jam
*/
void I2cController::handleJam() {
    uint8_t scl = state.getI2cSclPin();
    uint8_t sda = state.getI2cSdaPin();
    uint32_t freqHz = state.getI2cFrequency();

    terminalView.println("I2C总线干扰: 干扰SCL/SDA总线... 按下[ENTER]停止.\n"); // 汉化

    // Release I2C bus
    i2cService.end();

    while (true) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;

        i2cService.injectRandomGlitch(scl, sda, freqHz);
    }

    // Try recovering bus after jamming
    i2cService.i2cBitBangRecoverBus(scl, sda, freqHz);

    // Reconfigure I2C
    ensureConfigured();
    terminalView.println("\nI2C总线干扰: 已被用户停止.\n"); // 汉化
}

/*
Monitor
*/
void I2cController::handleMonitor(const TerminalCommand& cmd) {
    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: monitor <地址> [延迟_ms]"); // 汉化
        return;
    }

    uint8_t addr = argTransformer.parseHexOrDec(cmd.getSubcommand());
    uint16_t len = 256;
    uint32_t delayMs = 500;

    // Optional delay
    auto args = argTransformer.splitArgs(cmd.getArgs());
    if (!args.empty() && argTransformer.isValidNumber(args[0])) {
        delayMs = argTransformer.parseHexOrDec32(args[0]);
    }

    // Check device presence
    i2cService.beginTransmission(addr);
    if (i2cService.endTransmission()) {
        terminalView.println("I2C监控: 在0x" + argTransformer.toHex(addr) + "未发现设备"); // 汉化
        return;
    }

    terminalView.println("I2C监控: 监控地址0x" + argTransformer.toHex(addr) + "的寄存器变化... 按下[ENTER]停止.\n"); // 汉化

    std::vector<uint8_t> prev(len, 0xFF);
    std::vector<uint8_t> curr(len, 0xFF);
    std::vector<bool> valid(len, false);

    // First read to initialize prev
    if (i2cService.isReadableDevice(addr, 0x00)) {
        performRegisterRead(addr, 0x00, len, prev, valid);
    } else {
        performRawRead(addr, 0x00, len, prev, valid);
    }

    while (true) {
        // Try register read
        if (i2cService.isReadableDevice(addr, 0x00)) {
            performRegisterRead(addr, 0x00, len, curr, valid);
        } else {
            performRawRead(addr, 0x00, len, curr, valid);
        }

        // Compare and show changes
        for (uint16_t i = 0; i < len; ++i) {
            if (valid[i] && curr[i] != prev[i]) {
                std::stringstream ss;
                ss << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << i
                   << ": 0x" << std::setw(2) << (int)prev[i]
                   << " -> 0x" << std::setw(2) << (int)curr[i];
                terminalView.println(ss.str());
                prev[i] = curr[i];
            }
        }

        // Check for user input to stop
        uint32_t elapsed = 0;
        while (elapsed < delayMs) {
            char key = terminalInput.readChar();
            if (key == '\r' || key == '\n') {
                terminalView.println("\nI2C监控: 已被用户停止."); // 汉化
                return;
            }
            delay(10);
            elapsed += 10;
        }
    }

    terminalView.println("\nI2C监控: 已停止."); // 汉化
}

/*
EEPROM
*/
void I2cController::handleEeprom(const TerminalCommand& cmd) {
    uint8_t addr = 0x50; // Default EEPROM I2C address

    auto sub = cmd.getSubcommand();
    if (!sub.empty()) {
        if (!argTransformer.isValidNumber(sub)) {
            terminalView.println("使用方法: eeprom [地址]"); // 汉化
            return;
        }

        auto parsed = argTransformer.parseHexOrDec(sub);
        if (parsed < 0x03 || parsed > 0x77) { // plage valide I2C 7-bit
            terminalView.println("❌ 无效的I2C地址. 必须在0x03到0x77之间."); // 汉化
            return;
        }

        addr = parsed;
    }

    eepromShell.run(addr);
    ensureConfigured();
}

/*
Help
*/
void I2cController::handleHelp() {
    terminalView.println("未知的I2C命令. 使用方法:"); // 汉化
    terminalView.println("  scan");
    terminalView.println("  ping <地址>"); // 汉化
    terminalView.println("  identify <地址>"); // 汉化
    terminalView.println("  sniff");
    terminalView.println("  slave <地址>"); // 汉化
    terminalView.println("  read <地址> <寄存器>"); // 汉化
    terminalView.println("  write <地址> <寄存器> <值>"); // 汉化
    terminalView.println("  dump <地址> [长度]"); // 汉化
    terminalView.println("  glitch <地址>"); // 汉化
    terminalView.println("  jam");
    terminalView.println("  flood <地址>"); // 汉化
    terminalView.println("  recover");
    terminalView.println("  monitor <地址> [延迟_ms]"); // 汉化
    terminalView.println("  eeprom [地址]"); // 汉化
    terminalView.println("  swap");
    terminalView.println("  config");
    terminalView.println("  原始指令, 例如: [0x13 0x4B r:8]"); // 汉化
}

/*
Swap SDA and SCL pins
*/
void I2cController::handleSwap() {
    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();

    // Swap in state
    state.setI2cSdaPin(scl);
    state.setI2cSclPin(sda);

    // Reconfigure I2C with swapped pins
    i2cService.configure(state.getI2cSdaPin(), state.getI2cSclPin(), state.getI2cFrequency());

    terminalView.println(
        "I2C引脚交换: SDA/SCL已交换. SDA=" + std::to_string(state.getI2cSdaPin()) +
        " SCL=" + std::to_string(state.getI2cSclPin())
    ); // 汉化
    terminalView.println("");
}

/*
Config
*/
void I2cController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // User could have set the same pin to a different usage
    // eg. select I2C then select UART then select I2C
    // Always reconfigure pins before use
    i2cService.end();
    uint8_t sda = state.getI2cSdaPin();
    uint8_t scl = state.getI2cSclPin();
    uint32_t freq = state.getI2cFrequency();
    i2cService.configure(sda, scl, freq);
}