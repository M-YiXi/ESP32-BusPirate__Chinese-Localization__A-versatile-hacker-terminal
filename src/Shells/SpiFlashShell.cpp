#include "SpiFlashShell.h"

SpiFlashShell::SpiFlashShell(
    SpiService& spiService,
    ITerminalView& view,
    IInput& input,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    BinaryAnalyzeManager& binaryAnalyzeManager
)
    : spiService(spiService),
      terminalView(view),
      terminalInput(input),
      argTransformer(argTransformer),
      userInputManager(userInputManager),
      binaryAnalyzeManager(binaryAnalyzeManager)
{
    // 无
}

void SpiFlashShell::run() {
    while (true) {
        terminalView.println("\n=== SPI Flash 命令行 ===");

        // 选择操作
        int index = userInputManager.readValidatedChoiceIndex("选择 SPI Flash 操作", actions, 0);

        // 退出
        if (index == -1 || actions[index] == "🚪 退出命令行") {
            terminalView.println("正在退出 SPI Flash 命令行...\n");
            break;
        }

        // 分发
        switch (index) {
            case 0: cmdProbe();   break;
            case 1: cmdAnalyze(); break;
            case 2: cmdSearch();  break;
            case 3: cmdStrings(); break;
            case 4: cmdRead();    break;
            case 5: cmdWrite();   break;
            case 6: cmdDump();    break;
            case 7: cmdDump(true); break;
            case 8: cmdErase();   break;
            default:
                terminalView.println("未知操作.\n");
                break;
        }
    }
}

/*
Flash 探测
*/
void SpiFlashShell::cmdProbe() {
    uint8_t id[3] = {0};
    spiService.readFlashIdRaw(id);

    std::stringstream idStr;
    terminalView.println("");
    idStr << "SPI Flash ID: "
          << std::hex << std::uppercase << std::setfill('0')
          << std::setw(2) << (int)id[0] << " "
          << std::setw(2) << (int)id[1] << " "
          << std::setw(2) << (int)id[2];
    terminalView.println(idStr.str());

    // 检查常见的无效响应
    if ((id[0] == 0x00 && id[1] == 0x00 && id[2] == 0x00) ||
        (id[0] == 0xFF && id[1] == 0xFF && id[2] == 0xFF)) {    
        terminalView.println("未检测到 SPI Flash (总线错误或无芯片).");
        return;
    }

    const FlashChipInfo* chip = findFlashInfo(id[0], id[1], id[2]);

    // 数据库中已知
    if (chip) {
        terminalView.println("制造商: " + std::string(chip->manufacturerName));
        terminalView.println("型号: " + std::string(chip->modelName));
        terminalView.println("容量: " +
            std::to_string(chip->capacityBytes / (1024UL * 1024UL)) + " MB\n");
        return;
    }

    // 后备, 未知芯片
    const char* manufacturer = findManufacturerName(id[0]);
    terminalView.println("制造商: " + std::string(manufacturer));

    // 估算容量
    uint32_t size = 1UL << id[2];
    std::stringstream sizeStr;
    if (size >= (1024 * 1024)) {
        sizeStr << (size / (1024 * 1024)) << " MB (估算)";
    } else {
        sizeStr << size << " 字节 (估算)";
    }
    terminalView.println("估算容量: " + sizeStr.str());
    terminalView.println("");
}

/*
Flash 分析
*/
void SpiFlashShell::cmdAnalyze() {
    if (!checkFlashPresent()) return;

    // 起始地址
    uint32_t start = 0;
    terminalView.println("\nSPI Flash 分析: SPI Flash 从 0x00000000... 按 [ENTER] 停止.");

    // 获取 Flash 大小
    uint8_t id[3];
    spiService.readFlashIdRaw(id);
    const FlashChipInfo* chip = findFlashInfo(id[0], id[1], id[2]);
    uint32_t flashSize = chip ? chip->capacityBytes : spiService.calculateFlashCapacity(id[2]);

    // 分析
    BinaryAnalyzeManager::AnalysisResult result = binaryAnalyzeManager.analyze(
        0,
        flashSize,
        [&](uint32_t addr, uint8_t* buf, uint32_t len) {
            spiService.readFlashData(addr, buf, len);
        }
    );

    // 计算摘要
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
        terminalView.println("\n  未找到已知文件签名.");
    }

    terminalView.println("\n  SPI Flash 分析: 完成.\n");
}

/*
Flash 字符串提取
*/
void SpiFlashShell::cmdStrings() {
    // 检查芯片是否存在
    if (!checkFlashPresent()) return;

    // 验证并解析参数
    uint8_t minStringLen = userInputManager.readValidatedUint8("字符串最小长度:", 10);

    terminalView.println("\nSPI Flash: 正在提取字符串... 按 [ENTER] 停止.\n");


    const uint32_t blockSize = 512;
    uint8_t buffer[blockSize];
    std::string currentStr;
    uint32_t currentAddr = 0;
    uint32_t stringStartAddr = 0;
    bool inString = false;

    // 获取 Flash 大小
    uint8_t id[3];
    spiService.readFlashIdRaw(id);
    const FlashChipInfo* chip = findFlashInfo(id[0], id[1], id[2]);
    uint32_t flashSize = chip ? chip->capacityBytes : spiService.calculateFlashCapacity(id[2]);

    // 分块读取 Flash
    for (uint32_t addr = 0; addr < flashSize; addr += blockSize) {
        spiService.readFlashData(addr, buffer, blockSize);

        // 读取块
        for (uint32_t i = 0; i < blockSize; ++i) {
            uint8_t b = buffer[i];
            uint32_t absoluteAddr = addr + i;

            if (b >= 32 && b <= 126) {  // 可打印 ASCII
                if (!inString) {
                    inString = true;
                    stringStartAddr = absoluteAddr;
                }
                currentStr += static_cast<char>(b);
            } else {
                if (inString && currentStr.length() >= minStringLen) {
                    terminalView.println(
                        "0x" + argTransformer.toHex(stringStartAddr, 6) + ": " + currentStr
                    );
                }
                currentStr.clear();
                inString = false;
            }

            // 如果用户按 ENTER 则退出
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("\nSPI Flash: 用户取消提取.");
                return;
            }
        }
    }

    // 处理最后剩余的字符串
    if (inString && currentStr.length() >= minStringLen) {
        terminalView.println(
            "0x" + argTransformer.toHex(stringStartAddr, 6) + ": " + currentStr
        );
    }

    terminalView.println("\nSPI Flash: 字符串提取完成.\n");
}

/*
Flash 搜索
*/
void SpiFlashShell::cmdSearch() {
    // 检查芯片是否存在
    if (!checkFlashPresent()) return;

    auto startAddr = 0;

    // 搜索模式
    terminalView.print("输入要搜索的字符串: ");
    std::string pattern = userInputManager.getLine();

    terminalView.println("\n正在搜索 \"" + pattern + "\" 在 SPI Flash 中从 0x" + argTransformer.toHex(startAddr, 6) + "... 按 [ENTER] 停止.\n");

    const uint32_t blockSize = 512;
    const uint32_t contextSize = 16;  // 前后字符数
    uint8_t buffer[blockSize + 32];

    // 获取 Flash 大小
    uint8_t id[3];
    spiService.readFlashIdRaw(id);
    const FlashChipInfo* chip = findFlashInfo(id[0], id[1], id[2]);
    uint32_t flashSize = chip ? chip->capacityBytes : spiService.calculateFlashCapacity(id[2]);

    // 分块读取 Flash
    for (uint32_t addr = startAddr; addr < flashSize; addr += blockSize - pattern.size()) {
        spiService.readFlashData(addr, buffer, blockSize + pattern.size() - 1);
        
        // 读取块
        for (uint32_t i = 0; i <= blockSize; ++i) {
            if (i + pattern.size() > blockSize + pattern.size() - 1) break;

            bool match = true;
            for (size_t j = 0; j < pattern.size(); ++j) {
                if (buffer[i + j] != pattern[j]) {
                    match = false;
                    break;
                }
            }
            
            // 找到匹配的字符串
            if (match) {
                uint32_t matchAddr = addr + i;
                std::string context;

                // 模式之前
                for (int j = (int)i - (int)contextSize; j < (int)i; ++j) {
                    if (j >= 0) {
                        char c = (char)buffer[j];
                        context += (isprint(c) ? c : '.');
                    }
                }

                // 模式
                context += "[";
                for (size_t j = 0; j < pattern.size(); ++j) {
                    char c = (char)buffer[i + j];
                    context += (isprint(c) ? c : '.');
                }
                context += "]";

                // 模式之后
                for (uint32_t j = i + pattern.size(); j < i + pattern.size() + contextSize && j < blockSize + pattern.size(); ++j) {
                    char c = (char)buffer[j];
                    context += (isprint(c) ? c : '.');
                }

                terminalView.println("0x" + argTransformer.toHex(matchAddr, 6) + ": " + context);
            }

            // 允许用户中断
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("\nSPI Flash 搜索: 用户已取消.\n");
                return;
            }
        }
    }

    terminalView.println("\n搜索完成.");
}

/*
Flash 读取
*/
void SpiFlashShell::cmdRead() {
    // 检查芯片是否存在
    if (!checkFlashPresent()) return;
    
    auto addrStr = userInputManager.readValidatedHexString("起始地址 (例如 00FF00) ", 0, true);
    auto address = argTransformer.parseHexOrDec16("0x" + addrStr);
    uint32_t count = userInputManager.readValidatedUint32("读取字节数:", 16);

    // 分块读取 Flash
    terminalView.println("SPI Flash 读取: 正在进行... 按 [ENTER] 停止");
    terminalView.println("");
    readFlashInChunks(address, count);
    terminalView.println("");
}

/*
Flash 分块读取
*/
void SpiFlashShell::readFlashInChunks(uint32_t address, uint32_t length) {
    uint8_t buffer[1024];
    uint32_t remaining = length;
    uint32_t currentAddr = address;

    // 显示块
    while (remaining > 0) {
        uint32_t chunkSize = (remaining > 1024) ? 1024 : remaining;
        spiService.readFlashData(currentAddr, buffer, chunkSize);

        for (uint32_t i = 0; i < chunkSize; i += 16) {
            std::stringstream line;

            // 地址
            line << std::hex << std::uppercase << std::setfill('0')
                 << std::setw(6) << (currentAddr + i) << ": ";

            // 十六进制
            for (uint32_t j = 0; j < 16; ++j) {
                if (i + j < chunkSize) {
                    line << std::setw(2) << (int)buffer[i + j] << " ";
                } else {
                    line << "   ";
                }
            }

            // ASCII
            line << " ";
            for (uint32_t j = 0; j < 16; ++j) {
                if (i + j < chunkSize) {
                    char c = static_cast<char>(buffer[i + j]);
                    line << (isprint(c) ? c : '.');
                }
            }
            terminalView.println(line.str());

            // 检查用户是否按 ENTER 停止
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("\n用户中断读取.");
                return;
            }
        }

        currentAddr += chunkSize;
        remaining -= chunkSize;
    }
}

void SpiFlashShell::readFlashInChunksRaw(uint32_t address, uint32_t length) {
    uint8_t buffer[1024];
    uint32_t remaining = length;
    uint32_t current   = address;

    while (remaining > 0) {
        uint32_t n = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        spiService.readFlashData(current, buffer, n);
        for (uint32_t i = 0; i < n; ++i) {
            terminalView.print(buffer[i]);
        }
        current   += n;
        remaining -= n;
    }
}

uint32_t SpiFlashShell::readFlashCapacity() {
    // 验证 Flash 容量
    uint8_t id[3];
    spiService.readFlashIdRaw(id);
    const FlashChipInfo* chip = findFlashInfo(id[0], id[1], id[2]);
    uint32_t flashCapacity = 0;
    if (chip) {
        flashCapacity = chip->capacityBytes;
    } else {
        flashCapacity = spiService.calculateFlashCapacity(id[2]);
        std::stringstream capStr;
        capStr << "从 ID 估算容量: " << (flashCapacity >> 20) << " MB";
        terminalView.println(capStr.str());
    }

    return flashCapacity;   
}

/*
Flash 写入
*/
void SpiFlashShell::cmdWrite() {
    // 检查芯片是否存在
    if (!checkFlashPresent()) return;

    // 地址
    auto addrStr = userInputManager.readValidatedHexString("起始地址 (例如 00FF00) ", 0, true);
    auto addr = argTransformer.parseHexOrDec16("0x" + addrStr);

    std::vector<uint8_t> data;

    // 询问是否写入 ASCII 字符串
    if (userInputManager.readYesNo("写入 ASCII 字符串?", true)) {
        terminalView.println("输入 ASCII 字符串 (支持 \\n, \\x41 等):");
        std::string ascii = userInputManager.getLine();
        std::string decoded = argTransformer.decodeEscapes(ascii);
        data.assign(decoded.begin(), decoded.end());
    } else {
        // 十六进制字节列表
        std::string hexStr = userInputManager.readValidatedHexString("输入字节值 (例如 01 A5 FF...) ", 0, true);
        data = argTransformer.parseHexList(hexStr);
    }

    // 确认
    if (!userInputManager.readYesNo("SPI Flash 写入: 确认写入操作?", false)) {
        terminalView.println("SPI Flash 写入: 已取消.\n");
        return;
    }

    // 验证
    if (data.empty()) {
        terminalView.println("SPI Flash 写入: 无效数据格式.");
        return;
    }

    // 写入
    terminalView.println("正在写入 " + std::to_string(data.size()) + " 字节到地址 0x" +
                         argTransformer.toHex(addr, 6));

    uint32_t freq = state.getSpiFrequency();
    spiService.writeFlashPatch(addr, data, freq);

    terminalView.println("SPI Flash 写入: 完成.\n");
}

/*
Flash 擦除
*/
void SpiFlashShell::cmdErase() {
    // 检查芯片是否存在
    if (!checkFlashPresent()) return;
    
    terminalView.println("");
    if (!userInputManager.readYesNo("SPI Flash 擦除: 擦除整个 Flash 存储器?", false)) {
        terminalView.println("SPI Flash 擦除: 已取消.\n");
        return;
    }

    uint32_t freq = state.getSpiFrequency();
    const uint32_t sectorSize = 4096; // 标准
    uint32_t flashSize = readFlashCapacity();

    // 擦除扇区并显示进度
    const uint32_t totalSectors = flashSize / sectorSize;
    terminalView.print("正在进行");
    for (uint32_t i = 0; i < totalSectors; ++i) {
        uint32_t addr = i * sectorSize;
        spiService.eraseFlashSector(addr, freq);

        // 显示点
        if (i % 64 == 0) terminalView.print(".");
    }

    terminalView.println("\r\nSPI Flash 擦除: 完成.\n");
}

/*
Flash 转储
*/
void SpiFlashShell::cmdDump(bool raw) {
    if (!checkFlashPresent()) return;

    terminalView.println("\nSPI Flash: 从 0x000000 完整转储... 按 [ENTER] 停止.\n");

    if (raw) {
        auto confirm = userInputManager.readYesNo("原始模式用于 Python 脚本, 是否继续?", false);
        if (!confirm) return;
    }

    // 获取 Flash 大小
    uint32_t flashSize = readFlashCapacity();

    // 分块读取
    if (raw) readFlashInChunksRaw(0, flashSize); 
    else readFlashInChunks(0, flashSize);

    terminalView.println("\nSPI Flash 转储: 完成.\n");
}


/*
检查芯片
*/
bool SpiFlashShell::checkFlashPresent() {
    uint8_t id[3];
    spiService.readFlashIdRaw(id);

    bool invalid = (id[0] == 0xFF && id[1] == 0xFF && id[2] == 0xFF) ||
                   (id[0] == 0x00 && id[1] == 0x00 && id[2] == 0x00);

    if (invalid) {
        terminalView.println("未检测到 SPI Flash (总线错误或无芯片).\n");
        return false;
    }

    return true;
}