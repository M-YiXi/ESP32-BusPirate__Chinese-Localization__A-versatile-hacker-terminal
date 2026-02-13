#include "OneWireEepromShell.h"

OneWireEepromShell::OneWireEepromShell(
    ITerminalView& view,
    IInput& input,
    OneWireService& oneWireService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    BinaryAnalyzeManager& binaryAnalyzeManager
) :
    terminalView(view),
    terminalInput(input),
    oneWireService(oneWireService),
    argTransformer(argTransformer),
    userInputManager(userInputManager),
    binaryAnalyzeManager(binaryAnalyzeManager)
{
}

void OneWireEepromShell::run() {
    cmdProbe();

    while (true) {
        terminalView.println("\n=== DS24/28 EEPROM 命令行 ==="); //汉化
        int index = userInputManager.readValidatedChoiceIndex("选择 EEPROM 操作", actions, 0); //汉化
        if (index == -1 || actions[index] == " 🚪 Exit Shell") break;

        switch (index) {
            case 0: cmdProbe(); break;
            case 1: cmdAnalyze(); break;
            case 2: cmdRead();  break;
            case 3: cmdWrite(); break;
            case 4: cmdDump();  break;
            case 5: cmdErase(); break;
            default: terminalView.println("无效操作。"); break; //汉化
        }
    }
    terminalView.println("正在退出 1-Wire EEPROM 命令行...\n"); //汉化
}

void OneWireEepromShell::cmdProbe() {
    terminalView.println("\n正在探测 1-Wire EEPROM..."); //汉化
    uint8_t id[8];

    if (!oneWireService.eeprom2431Probe(id)) {
        terminalView.println("\n ❌ 未检测到支持的 EEPROM"); //汉化
        terminalView.println("    (DS2431, DS2433, DS28E01)"); //汉化
        return;
    }

    if (!oneWireService.getEepromModelInfos(id, eepromModel, eepromSize, eepromPageSize)) {
        uint8_t familyCode = id[0];
        terminalView.println("\n ⚠️ 未知家族码：0x" + argTransformer.toHex(familyCode, 2)); //汉化
        eepromModel = "Unknown";
        eepromSize = 128;
        eepromPageSize = 8;
    }

    terminalView.println("\n ✅ 检测到 EEPROM：" + eepromModel + " (0x" + argTransformer.toHex(id[0], 2) + ")"); //汉化
    terminalView.println(" 大小：" + std::to_string(eepromSize) + " 字节 | 页大小：" + std::to_string(eepromPageSize) + " 字节"); //汉化
}

void OneWireEepromShell::cmdRead() {
    terminalView.println("\n📖 读取 EEPROM"); //汉化

    auto addr = userInputManager.readValidatedUint32("起始地址", 0); //汉化

    if (addr >= eepromSize) {
        terminalView.println("\n ❌ 无效地址。"); //汉化
        return;
    }

    uint16_t len = userInputManager.readValidatedUint32("读取字节数", 16); //汉化
    if (addr + len > eepromSize) len = eepromSize - addr;

    terminalView.println("");
    for (uint16_t i = 0; i < len; ++i) {
        uint8_t value = oneWireService.eeprom2431ReadByte(addr + i);
        terminalView.println("  [0x" + argTransformer.toHex(addr + i, 2) + "] = " + argTransformer.toHex(value, 2)); //汉化（地址/值显示保留）
    }
}

void OneWireEepromShell::cmdWrite() {
    terminalView.println("\n✏️  写入 EEPROM（按页）"); //汉化

    uint16_t addr = userInputManager.readValidatedUint32("起始地址", 0); //汉化
    if (addr >= eepromSize) {
        terminalView.println("\n ❌ 无效起始地址。"); //汉化
        return;
    }

    std::string hexStr = userInputManager.readValidatedHexString("输入十六进制字节（例如：AA BB CC ...）：", 0, true); //汉化
    std::vector<uint8_t> data = argTransformer.parseHexList(hexStr);

    if (addr + data.size() > eepromSize) {
        terminalView.println("\n ❌ 数据超出 EEPROM 大小。"); //汉化
        return;
    }

    auto confirm = userInputManager.readYesNo("确认在地址 0x" + argTransformer.toHex(addr, 2) + " 处写入？", false); //汉化

    if (!confirm) {
        terminalView.println("\n ❌ 写入已取消。"); //汉化
        return;
    }

    // Write row by row
    size_t offset = 0;
    while (offset < data.size()) {
        size_t chunkSize = std::min((size_t)8, data.size() - offset);
        uint16_t absoluteAddr = addr + offset;

        // Copy to 8-byte buffer
        uint8_t buffer[8] = {0};
        memcpy(buffer, data.data() + offset, chunkSize);

        // Calculate rowAddress = absoluteAddr / 8
        uint8_t rowAddr = absoluteAddr / 8;

        // Write the row
        bool ok = oneWireService.eeprom2431WriteRow(rowAddr, buffer, true);
        if (!ok) {
            terminalView.println("\n ❌ 行 " + std::to_string(rowAddr) + " 写入失败。"); //汉化
            return;
        }

        offset += chunkSize;
    }

    terminalView.println("\n ✅ EEPROM 写入完成。"); //汉化
}

void OneWireEepromShell::cmdDump() {
    terminalView.println("\n🗃️ EEPROM 转储：正在读取整个存储器...\n"); //汉化

    const uint8_t bytesPerLine = 16;

    for (uint16_t addr = 0; addr < eepromSize; addr += bytesPerLine) {
        std::vector<uint8_t> line = oneWireService.eeprom2431Dump(addr, bytesPerLine);
        std::string formattedLine = argTransformer.toAsciiLine(addr, line);
        terminalView.println(formattedLine);

        // 用户中断 //汉化
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') {
            terminalView.println("\n ❌ 用户取消转储。"); //汉化
            return;
        }
    }

    terminalView.println("\n ✅ EEPROM 转储完成。"); //汉化
}

void OneWireEepromShell::cmdErase() {
    terminalView.println("\n💣 EEPROM 擦除：正在将 0x00 写入整个存储器..."); //汉化
    if (!userInputManager.readYesNo("确认擦除？", false)) { //汉化
        terminalView.println("擦除已取消。"); //汉化
        return;
    }

    uint8_t buffer[8];
    memset(buffer, 0x00, sizeof(buffer));

    for (uint16_t addr = 0; addr < eepromSize; addr += 8) {
        uint8_t rowAddr = addr / 8;

        bool ok = oneWireService.eeprom2431WriteRow(rowAddr, buffer, true);
        if (!ok) {
            terminalView.println("\n ❌ 行 " + std::to_string(rowAddr) + " 擦除失败。"); //汉化
            return;
        }
    }

    terminalView.println("\n ✅ EEPROM 擦除完成。"); //汉化
}

void OneWireEepromShell::cmdAnalyze() {
    terminalView.println("\n📊 分析 1-Wire EEPROM..."); //汉化

    // Analyze chunked
    auto result = binaryAnalyzeManager.analyze(
        0, // Start address
        eepromSize,
        // Fetch function
        [&](uint32_t addr, uint8_t* buf, uint32_t len) {
            auto chunk = oneWireService.eeprom2431Dump(addr, len);
            memcpy(buf, chunk.data(), len);
        },
        32 // Block size
    );

    // Format summary and display results
    auto summary = binaryAnalyzeManager.formatAnalysis(result);
    terminalView.println(summary);

    if (!result.foundSecrets.empty()) {
        terminalView.println("\n  检测到的密钥："); //汉化
        for (const auto& s : result.foundSecrets) terminalView.println("    " + s);
    }

    if (!result.foundFiles.empty()) {
        terminalView.println("\n  检测到的文件签名："); //汉化
        for (const auto& f : result.foundFiles) terminalView.println("    " + f);
    } else {
        terminalView.println("\n  未找到已知文件签名。"); //汉化
    }

    terminalView.println("\n ✅ 分析完成。"); //汉化
}