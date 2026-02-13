#include "SpiEepromShell.h"

SpiEepromShell::SpiEepromShell(
    SpiService& spiService,
    ITerminalView& view,
    IInput& input,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    BinaryAnalyzeManager& binaryAnalyzeManager
) :
    spiService(spiService),
    terminalView(view),
    terminalInput(input),
    argTransformer(argTransformer),
    userInputManager(userInputManager),
    binaryAnalyzeManager(binaryAnalyzeManager)
{
}

void SpiEepromShell::run() {
    // 选择 EEPROM 型号
    int selectedModelIndex = userInputManager.readValidatedChoiceIndex("\n选择 EEPROM 类型", models, modelsCount, 0);
    if (selectedModelIndex < 0) {
        terminalView.println("无效选择. 已中止.\n");
        return;
    }

    // 获取参数
    eepromSize = memoryLengths[selectedModelIndex];
    pageSize = pageLengths[selectedModelIndex];
    eepromModel = models[selectedModelIndex];
    size_t p = eepromModel.find('|'); // 删除第一个 '|' 之后的所有内容
    eepromModel.resize(p);
    bool isSmall = selectedModelIndex < 3; // 型号 25X010, 25X020, 25X040 为小容量

    auto mosi = state.getSpiMOSIPin();
    auto miso = state.getSpiMISOPin();
    auto sclk = state.getSpiCLKPin();
    auto cs = state.getSpiCSPin();
    auto wp = 999; // 默认写保护引脚
    
    // 初始化 EEPROM
    bool ok = spiService.initEeprom(mosi, miso, sclk, cs, pageSize, eepromSize, wp, isSmall);
    if (!ok) {
        terminalView.println("\n初始化 EEPROM 失败. 请检查连接.");
        terminalView.println("HOLD 引脚必须连接到 VCC 才能检测 EEPROM.\n");
        return;
    }

    while (true) {
        terminalView.println("\n=== SPI EEPROM 命令行 ===");

        // 选择操作
        int index = userInputManager.readValidatedChoiceIndex("选择 EEPROM 操作", actions, actionsCount, 0);

        // 退出
        if (index == -1 || actions[index] == " 🚪 退出命令行") {
            terminalView.println("正在退出 SPI EEPROM 命令行...\n");
            break;
        }
        // 执行选中的操作
        switch (index) {
            case 0: cmdProbe(); break;
            case 1: cmdAnalyze(); break;
            case 2: cmdRead();  break;
            case 3: cmdWrite(); break;
            case 4: cmdDump();  break;
            case 5: cmdDump(true); break;
            case 6: cmdErase(); break;
            default:
                terminalView.println("未知操作.");
                break;
        }
    }
    spiService.closeEeprom();
}

void SpiEepromShell::cmdProbe() {
    terminalView.println("\n[信息] 正在探测 SPI EEPROM...");

    const bool ok = spiService.probeEeprom();

    if (ok) {
        terminalView.println("\n ✅ 检测到 EEPROM.");
        terminalView.println(" 型号     :" + eepromModel);
        terminalView.println(" 大小      : " + std::to_string(eepromSize / 1024) + " Kbytes");
        terminalView.println(" 页大小 : " + std::to_string(pageSize) + " 字节");
    } else {
        terminalView.println("\n ❌ 未找到 EEPROM.");
    }
}

void SpiEepromShell::cmdRead() {
    terminalView.println("\n📖 读取 EEPROM");

    auto addrStr = userInputManager.readValidatedHexString("起始地址 (例如 00FF00) ", 0, true);
    uint32_t addr = argTransformer.parseHexOrDec32("0x" + addrStr);

    if (addr >= eepromSize) {
        terminalView.println("\n ❌ 错误: 起始地址超出 EEPROM 大小.\n");
        return;
    }

    uint32_t count = userInputManager.readValidatedUint32("读取字节数:", 16);
    if (addr + count > eepromSize) {
        count = eepromSize - addr;
    }

    terminalView.println("");
    const uint8_t bytesPerLine = 16;

    for (uint32_t i = 0; i < count; i += bytesPerLine) {
        uint8_t buffer[bytesPerLine];
        uint8_t lenToRead = std::min<uint32_t>(bytesPerLine, count - i);

        bool ok = spiService.readEepromBuffer(addr + i, buffer, lenToRead);
        if (!ok) {
            terminalView.println("\n ❌ 读取失败于 0x" + argTransformer.toHex(addr + i, 6));
            return;
        }

        std::vector<uint8_t> line(buffer, buffer + lenToRead);
        std::string formattedLine = argTransformer.toAsciiLine(addr + i, line);
        terminalView.println(formattedLine);
    }

    terminalView.println("");
}


void SpiEepromShell::cmdWrite() {
    terminalView.println("\n✏️  写入 EEPROM");

    uint32_t addr = userInputManager.readValidatedUint32("起始地址:", 0);

    if (userInputManager.readYesNo("写入 ASCII 字符串?", true)) {
        terminalView.print("输入 ASCII 字符串: ");
        std::string input = userInputManager.getLine();
        std::string decoded = argTransformer.decodeEscapes(input);
        bool ok = spiService.writeEepromBuffer(addr, (const uint8_t*)decoded.data(), decoded.size());
        terminalView.println(ok ? "\n ✅ 写入成功" : "\n ❌ 写入失败");
    } else {
        std::string hexStr = userInputManager.readValidatedHexString("输入十六进制字节 (例如 AA BB CC) ", 0, true);
        std::vector<uint8_t> data = argTransformer.parseHexList(hexStr);
        bool ok = spiService.writeEepromBuffer(addr, data.data(), data.size());
        terminalView.println(ok ? "\n ✅ 写入成功" : "\n ❌ 写入失败");
    }
}

void SpiEepromShell::cmdDump(bool raw) {
    terminalView.println("\n🗃️ EEPROM 转储: 正在读取整个存储器...");

    if (raw) {
        auto confirm = userInputManager.readYesNo("原始转储用于 Python 脚本. 是否继续?", false);
        if (!confirm) return;
    }

    const uint32_t totalSize = eepromSize;
    const uint32_t lineSize  = 16;
    uint8_t buffer[lineSize];

    for (uint32_t addr = 0; addr < totalSize; addr += lineSize) {
        // 读取
        bool ok = spiService.readEepromBuffer(addr, buffer, lineSize);
        if (!ok) {
            if (!raw) {
                terminalView.println("\n ❌ 读取失败于 0x" + argTransformer.toHex(addr, 6));
            }
            return;
        }

        if (raw) {
            // 原始模式
            for (uint32_t i = 0; i < lineSize; i++) terminalView.print(buffer[i]);
        } else {
            // ASCII 模式
            std::vector<uint8_t> line(buffer, buffer + lineSize);
            std::string formattedLine = argTransformer.toAsciiLine(addr, line);
            terminalView.println(formattedLine);

            // 取消
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("\n ❌ 用户取消转储.");
                return;
            }
        }
    }

    if (!raw) {
        terminalView.println("\n ✅ EEPROM 转储完成.");
    }
}

void SpiEepromShell::cmdErase() {
    terminalView.println("\n💣 EEPROM 擦除: 正在将 0xFF 写入整个存储器...");

    if (!userInputManager.readYesNo("确认擦除?", false)) {
        terminalView.println("擦除已取消.");
        return;
    }

    const uint32_t totalSize = eepromSize;
    const uint32_t blockSize = 64;
    uint8_t ff[blockSize];
    std::fill_n(ff, blockSize, 0xFF);
    
    terminalView.print("正在擦除");
    for (uint32_t addr = 0; addr < totalSize; addr += blockSize) {
        bool ok = spiService.writeEepromBuffer(addr, ff, blockSize);
        if (!ok) {
            terminalView.println("\n ❌ 写入失败于 0x" + argTransformer.toHex(addr, 6));
            return;
        }

        // 进度反馈
        if (addr % 1024 == 0) terminalView.print(".");
    }

    terminalView.println("\r\n\n ✅ EEPROM 擦除完成.");
}

void SpiEepromShell::cmdAnalyze() {
    terminalView.println("\nSPI EEPROM 分析: 从 0x00000000... 按 [ENTER] 停止.");

    if (!spiService.probeEeprom()) {
        terminalView.println("\n ❌ 未找到 EEPROM. 已中止.");
        return;
    }

    // 分块分析 EEPROM
    auto result = binaryAnalyzeManager.analyze(
        0,
        eepromSize,
        [&](uint32_t addr, uint8_t* buf, uint32_t len) {
            if (!spiService.readEepromBuffer(addr, buf, len)) {
                memset(buf, 0xFF, len);
            }
        }
    );

    // 摘要
    auto summary = binaryAnalyzeManager.formatAnalysis(result);
    terminalView.println(summary);

    // 密钥
    if (!result.foundSecrets.empty()) {
        terminalView.println("\n  检测到的敏感模式:");
        for (const auto& entry : result.foundSecrets) {
            terminalView.println("    " + entry);
        }
    }

    // 文件
    if (!result.foundFiles.empty()) {
        terminalView.println("\n  检测到的文件签名:");
        for (const auto& entry : result.foundFiles) {
            terminalView.println("    " + entry);
        }
    } else {
        terminalView.println("\n 未找到已知文件签名.");
    }

    terminalView.println("\n ✅ SPI EEPROM 分析: 完成.");
}