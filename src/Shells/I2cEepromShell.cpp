#include "I2cEepromShell.h"

/**
 * @brief 构造函数：初始化I2C EEPROM交互Shell的依赖组件
 * @param view 终端视图接口（负责文本输出）
 * @param input 输入接口（负责用户输入）
 * @param i2cService I2C服务类（底层EEPROM操作）
 * @param argTransformer 参数转换工具（十六进制/十进制解析、格式化）
 * @param userInputManager 用户输入管理类（输入验证、选择读取）
 * @param binaryAnalyzeManager 二进制内容分析类（检测文件签名、敏感信息）
 */
I2cEepromShell::I2cEepromShell(
    ITerminalView& view,
    IInput& input,
    I2cService& i2cService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    BinaryAnalyzeManager& binaryAnalyzeManager
) : terminalView(view),
    terminalInput(input),
    i2cService(i2cService),
    argTransformer(argTransformer),
    userInputManager(userInputManager),
    binaryAnalyzeManager(binaryAnalyzeManager) {}

/**
 * @brief 运行I2C EEPROM交互Shell主循环
 * @param addr EEPROM的I2C地址（7位地址，如0x50）
 * @note 先选择EEPROM型号并初始化，再进入操作菜单循环，直到用户选择退出
 */
void I2cEepromShell::run(uint8_t addr) {

    // 选择EEPROM型号
    int selectedModelIndex = userInputManager.readValidatedChoiceIndex("\n选择EEPROM型号", kModels, kModelsCount, 0);
    
    // 初始化EEPROM（根据选中的型号设置容量）
    uint16_t selectedType = memoryLengths[selectedModelIndex];
    if (!i2cService.initEeprom(selectedType, addr)) {
        terminalView.println("\n❌ 未在0x" + argTransformer.toHex(addr, 2) + "地址检测到EEPROM，操作终止。\n");
        return;
    }
    
    // 设置全局变量，标记初始化完成
    terminalView.println(
        std::string("\n✅ EEPROM初始化成功: ") + kModels[selectedModelIndex]
    );
    selectedModel = kModels[selectedModelIndex];
    selectedLength = memoryLengths[selectedModelIndex];
    selectedI2cAddress = addr;
    initialized = true;

    // 主操作循环
    while (true) {
        // 显示操作菜单并读取用户选择
        terminalView.println("\n=== I2C EEPROM交互Shell ===");
        int index = userInputManager.readValidatedChoiceIndex("选择EEPROM操作", kActions, kActionsCount, kActionsCount - 1);
        if (index == -1 || kActions[index] == " 🚪 退出Shell") {
            terminalView.println("退出EEPROM交互Shell...\n");
            break;
        }

        // 执行选中的操作
        switch (index) {
            case 0: cmdProbe(); break;    // 探测EEPROM信息
            case 1: cmdAnalyze(); break;  // 分析EEPROM内容
            case 2: cmdRead(); break;     // 读取指定地址数据
            case 3: cmdWrite(); break;    // 写入指定地址数据
            case 4: cmdDump(); break;     // 全量导出（十六进制/ASCII格式）
            case 5: cmdDump(true); break; // 全量导出（原始二进制格式）
            case 6: cmdErase(); break;    // 擦除整个EEPROM
        }
    }
}

/**
 * @brief 【操作】探测EEPROM基本信息（容量、页大小、地址字节数、写入延时）
 */
void I2cEepromShell::cmdProbe() {
    uint32_t length = i2cService.eepromLength();       // 总容量
    uint32_t memSize = i2cService.eepromGetMemorySize(); // 内存大小
    uint16_t pageSize = i2cService.eepromPageSize();   // 页大小
    uint8_t addrBytes = i2cService.eepromAddressBytes(); // 地址字节数（1/2）
    uint8_t writeTime = i2cService.eepromWriteTimeMs(); // 单次写入延时（ms）

    terminalView.println("\n📄 EEPROM信息汇总:");
    terminalView.println(" • 总容量:     " + std::to_string(length) + " 字节");
    terminalView.println(" • 内存大小:  " + std::to_string(memSize) + " 字节");
    terminalView.println(" • 页大小:    " + std::to_string(pageSize) + " 字节");
    terminalView.println(" • 地址长度: " + std::to_string(addrBytes) + " 字节");
    terminalView.println(" • 写入延时:   " + std::to_string(writeTime) + " 毫秒");
}

/**
 * @brief 【操作】分析EEPROM二进制内容（检测文件签名、敏感信息）
 * @note 逐块读取EEPROM数据并交给BinaryAnalyzeManager分析，输出分析结果
 */
void I2cEepromShell::cmdAnalyze() {
    uint32_t eepromSize = i2cService.eepromLength();
    uint32_t start = 0;
    terminalView.println("\n🔍 正在分析EEPROM内容...\n");

    // 分块分析EEPROM内容（通过回调函数逐块读取数据）
    BinaryAnalyzeManager::AnalysisResult result = binaryAnalyzeManager.analyze(
        start,
        eepromSize,
        [&](uint32_t addr, uint8_t* buf, uint32_t len) {
            // 回调函数：读取指定地址的len个字节到buf
            for (uint32_t i = 0; i < len; ++i)
                buf[i] = i2cService.eepromReadByte(addr + i);
        }
    );

    // 格式化并输出分析摘要
    auto summary = binaryAnalyzeManager.formatAnalysis(result);
    terminalView.println(summary);

    // 输出检测到的文件签名
    if (!result.foundFiles.empty()) {
        terminalView.println("\n📁 检测到的文件签名:");
        for (const auto& file : result.foundFiles) {
            terminalView.println("   - " + file);
        }
    }

    // 输出检测到的潜在敏感信息
    if (!result.foundSecrets.empty()) {
        terminalView.println("\n🔑 发现的潜在敏感信息:");
        for (const auto& secret : result.foundSecrets) {
            terminalView.println("   - " + secret);
        }
    }
}

/**
 * @brief 【操作】读取EEPROM指定地址的字节数据（十六进制+ASCII格式输出）
 * @note 支持读取1-16字节，地址超出范围时自动调整读取长度
 */
void I2cEepromShell::cmdRead() {
    // 读取并验证起始地址（十六进制字符串，如00FF00）
    auto addrStr = userInputManager.readValidatedHexString("起始地址（例如：00FF00） ", 0, true);
    auto addr = argTransformer.parseHexOrDec16("0x" + addrStr);
    uint32_t eepromSize = i2cService.eepromLength();
    
    // 检查地址是否超出EEPROM容量
    if (addr >= eepromSize) {
        terminalView.println("\n❌ 错误：起始地址超出EEPROM容量范围。");
        return;
    }
    
    // 读取并验证要读取的字节数（最大16字节）
    uint8_t count = userInputManager.readValidatedUint8("读取字节数:", 16);
    terminalView.println("");
    
    // 调整读取长度（避免超出EEPROM末尾）
    if (addr + count > eepromSize) {
        count = eepromSize - addr;
    }

    // 按每行16字节格式化输出
    const uint8_t bytesPerLine = 16;
    for (uint16_t i = 0; i < count; i += bytesPerLine) {
        std::vector<uint8_t> line;
        for (uint8_t j = 0; j < bytesPerLine && (i + j) < count; ++j) {
            line.push_back(i2cService.eepromReadByte(addr + i + j));
        }

        // 格式化地址+十六进制+ASCII字符串
        std::string formattedLine = argTransformer.toAsciiLine(addr + i, line);
        terminalView.println(formattedLine);
    }    
}

/**
 * @brief 【操作】向EEPROM指定地址写入字节数据
 * @note 支持输入十六进制格式的字节列表（如01 A5 FF），逐字节写入
 */
void I2cEepromShell::cmdWrite() {
    // 读取并验证起始地址
    auto addrStr = userInputManager.readValidatedHexString("起始地址:", 0, true);
    auto addr = argTransformer.parseHexOrDec16("0x" + addrStr);
    
    // 读取并解析要写入的十六进制字节数据
    auto hexStr = userInputManager.readValidatedHexString("输入字节值（例如：01 A5 FF...） ", 0, true);
    auto data = argTransformer.parseHexList(hexStr);

    // 逐字节写入EEPROM
    bool ok = true;
    for (size_t i = 0; i < data.size(); ++i) {
        i2cService.eepromWriteByte(addr + i, data[i]);
    }

    terminalView.println("\n✅ 数据写入完成。");
}

/**
 * @brief 【操作】全量导出EEPROM数据（支持格式化/原始模式）
 * @param raw 是否启用原始二进制模式（true=原始字节输出，false=十六进制+ASCII格式）
 * @note 原始模式适用于Python脚本解析，格式化模式支持用户中断导出
 */
void I2cEepromShell::cmdDump(bool raw) {
    uint32_t addr = 0;                  // 从地址0开始导出
    uint32_t count = i2cService.eepromLength(); // 导出整个EEPROM容量

    // 原始模式确认（防止误操作）
    if (raw) {
        auto confirm = userInputManager.readYesNo("原始模式适用于Python脚本解析，是否继续？", false);
        if (!confirm) return;
    }

    const uint8_t bytesPerLine = 16;

    if (raw) {
        // 原始二进制模式：直接输出字节数据
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t value = i2cService.eepromReadByte(addr + i);
            terminalView.print(value); 
        }
    } else {
        // 格式化模式：十六进制+ASCII输出，支持回车中断
        terminalView.println("");
        for (uint32_t i = 0; i < count; i += bytesPerLine) {
            std::vector<uint8_t> line;
            for (uint8_t j = 0; j < bytesPerLine && (i + j) < count; ++j) {
                line.push_back(i2cService.eepromReadByte(addr + i + j));

                // 检测用户输入（回车/换行则中断导出）
                char c = terminalInput.readChar();
                if (c == '\n' || c == '\r') {
                    terminalView.println("\n❌ 导出操作被用户中断。");
                    return;
                }
            }
            // 格式化并输出当前行
            std::string formatted = argTransformer.toAsciiLine(addr + i, line);
            terminalView.println(formatted);
        }
    }
}

/**
 * @brief 【操作】擦除整个EEPROM（填充0xFF）
 * @note 需用户二次确认，防止误擦除
 */
void I2cEepromShell::cmdErase() {
    bool confirm = userInputManager.readYesNo("⚠️  确定要擦除整个EEPROM吗？", false);
    if (confirm) {
        terminalView.println("正在擦除...");
        i2cService.eepromErase(0xFF); // 擦除并填充0xFF
        terminalView.println("\n✅ EEPROM擦除完成。");
    } else {
        terminalView.println("\n❌ 操作已取消。");
    }
}