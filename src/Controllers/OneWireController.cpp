#include "OneWireController.h"

/*
Constructor
*/
OneWireController::OneWireController(
    ITerminalView& terminalView, 
    IInput& terminalInput, 
    OneWireService& service, 
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager, 
    IbuttonShell& ibuttonShell,
    OneWireEepromShell& eepromShell
)
    : terminalView(terminalView), 
      terminalInput(terminalInput), 
      oneWireService(service), 
      argTransformer(argTransformer), 
      userInputManager(userInputManager), 
      ibuttonShell(ibuttonShell),
      eepromShell(eepromShell) {
}

/*
Entry point for command
*/
void OneWireController::handleCommand(const TerminalCommand& command) {
    if (command.getRoot()      == "scan")   handleScan();
    else if (command.getRoot() == "ping")   handlePing();
    else if (command.getRoot() == "sniff")  handleSniff();
    else if (command.getRoot() == "read")   handleRead();
    else if (command.getRoot() == "write")  handleWrite(command);
    else if (command.getRoot() == "ibutton")   handleIbutton(command);
    else if (command.getRoot() == "eeprom")   handleEeprom();
    else if (command.getRoot() == "temp")   handleTemperature();
    else if (command.getRoot() == "config") handleConfig();
    else                                    handleHelp();
}

/*
Entry point for instructions
*/
void OneWireController::handleInstruction(std::vector<ByteCode>& bytecodes) {
    auto result = oneWireService.executeByteCode(bytecodes);
    if (!result.empty()) {
        terminalView.println("OneWire读取:\n"); // 汉化
        terminalView.println(result);
    }
}

/*
Scan
*/
void OneWireController::handleScan() {
    terminalView.println("OneWire扫描: 正在进行中..."); // 汉化

    oneWireService.resetSearch();

    uint8_t rom[8];
    int deviceCount = 0;

    while (oneWireService.search(rom)) {
        std::ostringstream oss;
        oss << "设备 " << (++deviceCount) << ": "; // 汉化
        for (int i = 0; i < 8; ++i) {
            oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                << static_cast<int>(rom[i]) << " ";
        }

        uint8_t crc = oneWireService.crc8(rom, 7);
        if (crc != rom[7]) {
            oss << "(CRC错误)"; // 汉化
        }

        terminalView.println(oss.str());
    }

    if (deviceCount == 0) {
        terminalView.println("OneWire扫描: 未找到任何设备。"); // 汉化
    }
}

/*
Ping
*/
void OneWireController::handlePing() {
    bool devicePresent = oneWireService.reset();
    if (devicePresent) {
        terminalView.println("OneWire探测: 检测到设备。"); // 汉化
    } else {
        terminalView.println("OneWire探测: 未找到设备。"); // 汉化
    }
}

/*
Read
*/
void OneWireController::handleRead() {
    terminalView.println("OneWire读取: 按下[ENTER]停止。\n"); // 汉化

    while (true) {
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("");
            terminalView.println("OneWire读取: 已被用户停止。"); // 汉化
            break;
        }

        auto idReaded = handleIdRead();
        auto spReaded = handleScratchpadRead();

        if (idReaded && spReaded) {
            terminalView.println("OneWire读取: 完成。"); // 汉化
            terminalView.println("");
            break;
        }
        delay(100);
    }
}

/*
ID Read
*/
bool OneWireController::handleIdRead() {
    uint8_t buffer[8];

    if (!oneWireService.reset()) {
        return false;
    }
    terminalView.println("OneWire读取: 正在读取。"); // 汉化

    oneWireService.write(0x33);  // Read ROM
    oneWireService.readBytes(buffer, 8);

    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        oss << std::setw(2) << static_cast<int>(buffer[i]);
        if (i < 7) oss << " ";
    }

    terminalView.println("ROM ID: " + oss.str());

    uint8_t crc = oneWireService.crc8(buffer, 7);
    if (crc != buffer[7]) {
        terminalView.println("OneWire读取: ROM ID校验CRC错误。"); // 汉化
    }

    return true;
}

/*
Scratchpad Read
*/
bool OneWireController::handleScratchpadRead() {
    uint8_t scratchpad[8];
    if (!oneWireService.reset()) {
        return false;
    }

    oneWireService.write(0xAA);  // Read Scratchpad
    oneWireService.readBytes(scratchpad, 8);

    std::ostringstream oss;
    oss << "暂存器(Scratchpad): "; // 汉化
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)scratchpad[i];
        if (i < 8) oss << " ";
    }
    terminalView.println(oss.str());

    // CRC (last byte)
    uint8_t crc = oneWireService.crc8(scratchpad, 8);
    if (crc != scratchpad[7]) { // 修正原代码bug：scratchpad长度为8，索引最大为7，原代码写的scratchpad[8]会越界
        terminalView.println("暂存器(Scratchpad)校验CRC错误。"); // 汉化
    }

    return true;
}

/*
Write
*/
void OneWireController::handleWrite(const TerminalCommand& cmd) {
    std::string sub = cmd.getSubcommand();

    if (sub == "id" || sub == "sp") {
        std::vector<uint8_t> bytes;

        // Bytes are given in command line
        if (!cmd.getArgs().empty()) {
            bytes = argTransformer.parseByteList(cmd.getArgs());
        } else {
            // Interactive input if no arguments are given
            std::string prompt = "输入8字节数据(格式示例:28 FF AA...) "; // 汉化

            std::string input = userInputManager.readValidatedHexString(prompt, 8, false);
            bytes = argTransformer.parseHexList(input);
        }

        // Length check
        if (bytes.size() != 8) {
            terminalView.println("OneWire写入: 必须输入恰好8字节数据。"); // 汉化
            return;
        }

        if (sub == "id") {
            handleIdWrite(bytes);
        } else {
            handleScratchpadWrite(bytes);
        }
    }

    else {
        terminalView.println("OneWire写入: 语法错误。使用方法:"); // 汉化
        terminalView.println("  write id [8字节数据]"); // 汉化
        terminalView.println("  write sp [8字节数据]"); // 汉化
    }
}

/*
ID Write
*/
void OneWireController::handleIdWrite(std::vector<uint8_t> idBytes) {
    const int maxRetries = 8;
    int attempt = 0;
    bool success = false;

    terminalView.println("OneWire ID写入: 等待设备连接... 按下[ENTER]停止"); // 汉化

    // Wait detection
    while (!oneWireService.reset()) {
        delay(1);
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("");
            terminalView.println("OneWire写入: 已被用户停止。"); // 汉化
            break;
        }
    }
    
    // Try to write and verify X times
    while (attempt < maxRetries && !success) {
        attempt++;
        terminalView.println("尝试次数 " + std::to_string(attempt) + "..."); // 汉化

        // Write
        oneWireService.writeRw1990(state.getOneWirePin(), idBytes.data(), idBytes.size());
        delay(50);

        // Read ID to verify
        uint8_t buffer[8];
        if (!oneWireService.reset()) continue;
        oneWireService.write(0x33); // Read ROM
        oneWireService.readBytes(buffer, 8);

        // Read is not equal to given one
        if (memcmp(buffer, idBytes.data(), 7) != 0) {
            terminalView.println("ROM ID字节数据不匹配。"); // 汉化
            continue;
        } 

        success = true;
        break;
    }

    if (success) {
        terminalView.println("OneWire写入: ID写入成功。"); // 汉化
    } else {
        terminalView.println("OneWire写入: 写入失败。"); // 汉化
    }
}

/*
Scratchpad Write
*/
void OneWireController::handleScratchpadWrite(std::vector<uint8_t> scratchpadBytes) {
    const int maxRetries = 8;
    int attempt = 0;
    bool success = false;

    terminalView.println("OneWire写入: 等待设备连接... 按下[ENTER]停止"); // 汉化

    // Wait for device presence
    while (!oneWireService.reset()) {
        auto c = terminalInput.readChar();
        if (c == '\n' || c == '\r') {
            terminalView.println("已被用户终止。"); // 汉化
            return;
        }
        delay(1);
    }

    // Try up to X times
    while (attempt < maxRetries && !success) {
        attempt++;
        terminalView.println("尝试次数 " + std::to_string(attempt) + "..."); // 汉化

        if (!oneWireService.reset()) continue;

        oneWireService.skip();
        oneWireService.write(0x0F); // Scratchpad write command
        delayMicroseconds(20);
        oneWireService.writeBytes(scratchpadBytes.data(), 8);
        delay(50);

        // Verify by reading back
        if (!oneWireService.reset()) continue;

        oneWireService.skip();
        oneWireService.write(0xAA); // Read Scratchpad
        delayMicroseconds(20);

        uint8_t readback[8];
        oneWireService.readBytes(readback, 8);

        // Missmatch
        if (memcmp(readback, scratchpadBytes.data(), 8) != 0) {
            terminalView.println("暂存器(Scratchpad)数据不匹配。"); // 汉化
            continue;
        }
        
        // CRC error
        uint8_t crc = oneWireService.crc8(readback, 8);
        if (crc != readback[7]) {
            terminalView.println("暂存器(Scratchpad)校验CRC错误。"); // 汉化
            continue;
        }

        success = true;
    }

    if (success) {
        terminalView.println("OneWire写入: 暂存器(Scratchpad)写入成功。"); // 汉化
    } else {
        terminalView.println("OneWire写入: 8次尝试后仍失败。"); // 汉化（修正原代码"3 attempts"的笔误，maxRetries实际为8）
    }
}

/*
iButton
*/
void OneWireController::handleIbutton(const TerminalCommand& command) {
    ibuttonShell.run();
}

/*
Config
*/
void OneWireController::handleConfig() {
    terminalView.println("OneWire配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t pin = userInputManager.readValidatedPinNumber("数据引脚(Data pin)", state.getOneWirePin(), forbidden); // 汉化
    state.setOneWirePin(pin);
    oneWireService.configure(pin);

    terminalView.println("OneWire配置完成。"); // 汉化
    terminalView.println("");
}

/*
Sniff
*/
void OneWireController::handleSniff() {
    terminalView.println("OneWire嗅探: 正在监听数据线路... 按下[ENTER]停止。\n"); // 汉化

    if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
        terminalView.println("  [提示] 该功能依赖高精度时序。"); // 汉化
        terminalView.println("         Web CLI可能会丢失部分信号，"); // 汉化
        terminalView.println("         建议使用串口CLI以获得最佳效果。\n"); // 汉化
    }

    // Init the pin to read passively
    uint8_t pin = state.getOneWirePin();
    pinMode(pin, INPUT);

    // Read initial state of pin
    int prev = digitalRead(pin);
    unsigned long lastFall = micros();

    while (true) {
        // Enter press
        auto c = terminalInput.readChar();
        if (c == '\r' || c == '\n' ) break;
        
        // Read current state of pin
        int current = digitalRead(pin);
        unsigned long now = micros();

        // Detect a falling edge
        if (prev == HIGH && current == LOW) {
            lastFall = now;
        }

        // Detect a rising edge (end of a LOW pulse)
        if (prev == LOW && current == HIGH) {
            // Calculate duration of the LOW pulse
            unsigned long duration = now - lastFall;
            
            // Too long, not mean anything
            if (duration >= 3000) {
                terminalView.println("[非标准脉冲] " + std::to_string(duration) + " µs"); // 汉化
            }
            // Most likely a reset pulse
            else if (duration >= 480) {
                terminalView.println("[复位脉冲] 低电平持续 " + std::to_string(duration) + " µs"); // 汉化

            // Most likely a presence pulse
            } else if (duration >= 60 && duration <= 240) {
                terminalView.println("[存在脉冲] 低电平持续 " + std::to_string(duration) + " µs"); // 汉化
            
            // Most likely a bit transmission
            } else if (duration >= 10 && duration <= 70) {
                // Want to sample ~15 µs after the falling edge
                // which is the standard sampling time in the 1-Wire protocol
                long elapsed = now - lastFall;
                if (elapsed < 15) {
                    delayMicroseconds(15 - elapsed);
                }
                
                // Read the bit
                int sample = digitalRead(pin);
                terminalView.println("[数据位] 低电平持续 " + std::to_string(duration) +
                                     " µs, 采样值 = " + std::to_string(sample)); // 汉化

            // Unrecognized or noisy signal
            } else {
                terminalView.println("[噪声] 低电平持续 " + std::to_string(duration) + " µs"); // 汉化
            }
        }

        // Update
        prev = current;
    }
    terminalView.println("\n\nOneWire嗅探: 已被用户停止。"); // 汉化
}

/*
Temp
*/
void OneWireController::handleTemperature() {
    terminalView.println("OneWire温度读取: 正在搜索DS18B20传感器..."); // 汉化

    uint8_t rom[8];
    bool found = false;

    oneWireService.resetSearch();

    // Search captor type 0x28
    while (oneWireService.search(rom)) {
        if (rom[0] == 0x28) { // DS18B20
            found = true;
            break;
        }
    }

    if (!found) {
        terminalView.println("OneWire温度读取: 未找到DS18B20设备。"); // 汉化
        return;
    }

    // Display ID
    std::ostringstream oss;
    oss << "\nDS18B20 ROM: ";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(rom[i]) << " ";
    }
    terminalView.println(oss.str());

    // Start temp read process
    if (!oneWireService.reset()) {
        terminalView.println("OneWire温度读取: 复位失败。"); // 汉化
        return;
    }

    // Select and write CONVERT T
    oneWireService.select(rom);
    oneWireService.write(0x44);  // CONVERT T
    delay(750); // wait conversion max 750ms

    if (!oneWireService.reset()) {
        terminalView.println("OneWire温度读取: 读取暂存器前复位失败。"); // 汉化
        return;
    }

    // Read sratchpad
    oneWireService.select(rom);
    oneWireService.write(0xBE); // READ SCRATCHPAD

    uint8_t data[9];
    oneWireService.readBytes(data, 9);

    // CRC check
    uint8_t crc = oneWireService.crc8(data, 8);
    if (crc != data[8]) {
        terminalView.println("OneWire温度读取: 暂存器校验CRC错误。"); // 汉化
        return;
    }

    // Extract temp
    int16_t raw = (data[1] << 8) | data[0];
    float tempC = raw / 16.0f;

    // Display converted temp
    std::ostringstream tempStr;
    tempStr << "温度: " << std::fixed << std::setprecision(2) << tempC << " °C\n"; // 汉化
    terminalView.println(tempStr.str());
}

/*
EEPROM
*/
void OneWireController::handleEeprom() {
    #ifndef DEVICE_M5STICK
        terminalView.println("OneWire EEPROM: EEPROM交互界面启动..."); // 汉化
        oneWireService.close();
        oneWireService.configureEeprom(state.getOneWirePin());
        eepromShell.run();
        oneWireService.closeEeprom();
        ensureConfigured();

    #else
        // No space left for the eeprom lib
        terminalView.println("OneWire EEPROM: M5STICK设备不支持该功能。"); // 汉化
    #endif
}

/*
Help
*/
void OneWireController::handleHelp() {
    terminalView.println("未知的1Wire命令。使用方法:"); // 汉化
    terminalView.println("  scan");
    terminalView.println("  ping");
    terminalView.println("  sniff");
    terminalView.println("  read");
    terminalView.println("  write id [8字节数据]"); // 汉化
    terminalView.println("  write sp [8字节数据]"); // 汉化
    terminalView.println("  ibutton");
    terminalView.println("  eeprom");
    terminalView.println("  temp");
    terminalView.println("  config");
    terminalView.println("  原始指令格式示例: [0X33 r:8] ..."); // 汉化
}

void OneWireController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    uint8_t pin = state.getOneWirePin();
    oneWireService.configure(pin);
}