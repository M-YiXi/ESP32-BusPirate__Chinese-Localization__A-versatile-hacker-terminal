#include "ThreeWireEepromShell.h"

ThreeWireEepromShell::ThreeWireEepromShell(
    ITerminalView& terminalView,
    IInput& terminalInput,
    UserInputManager& userInputManager,
    ThreeWireService& threeWireService,
    ArgTransformer& argTransformer)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      userInputManager(userInputManager),
      threeWireService(threeWireService),
      argTransformer(argTransformer) {}

void ThreeWireEepromShell::run() {

    const std::vector<std::string> actions = {
        "🔍 探测",
        "📖 读取字节",
        "✏️  写入字节",
        "🗃️  转储 EEPROM",
        "💣 擦除 EEPROM",
        "🚪 退出命令行"
    };

    // EEPROM 型号
    std::vector<std::string> modelOptions = threeWireService.getSupportedModels();
    int modelIndex = userInputManager.readValidatedChoiceIndex("\n选择 EEPROM 型号", modelOptions, state.getThreeWireEepromModelIndex());
    int modelId = threeWireService.resolveModelId(modelOptions[modelIndex]);
    terminalView.println("\n✅ 已选型号: " + modelOptions[modelIndex] + " (ID: " + std::to_string(modelId) + ")");
    state.setThreeWireEepromModelIndex(modelIndex);

    // 组织方式
    terminalView.println("\n⚠️  ORG 是 EEPROM 芯片上的物理引脚.");
    terminalView.println("   将其接地为 8 位 (x8) 组织.");
    terminalView.println("   接 VCC 为 16 位 (x16) 组织.");
    terminalView.println("   这适用于具有可配置 ORG 引脚的芯片 (大多数).");
    terminalView.println("   固定组织芯片:");
    terminalView.println("     • 93xx56A → 始终 8 位");
    terminalView.println("     • 93xx56B → 始终 16 位\n");
    bool org8 = userInputManager.readYesNo("EEPROM 组织为 8 位?", false);
    state.setThreeWireOrg8(org8);
    
    auto cs = state.getThreeWireCsPin();
    auto sk = state.getThreeWireSkPin();
    auto di = state.getThreeWireDiPin();
    auto doPin = state.getThreeWireDoPin();
    threeWireService.end();
    threeWireService.configure(cs, sk, di, doPin, modelId, org8);
    
    while (true) {
        // 选择操作
        terminalView.println("\n=== 3WIRE EEPROM 命令行 ===");
        int index = userInputManager.readValidatedChoiceIndex("选择 EEPROM 操作", actions, 0);

        // 退出
        if (index == -1 || actions[index] == "🚪 退出命令行") {
            terminalView.println("正在退出 EEPROM 命令行...\n");
            break;
        }

        // 分发
        switch (index) {
            case 0: cmdProbe(); break;
            case 1: cmdRead(); break;
            case 2: cmdWrite(); break;
            case 3: cmdDump(); break;
            case 4: cmdErase(); break;
        }
    }
}

/*
EEPROM 探测
*/
void ThreeWireEepromShell::cmdProbe() {
    bool isOrg8 = state.isThreeWireOrg8();
    bool isBlank = true;

    if (isOrg8) {
        std::vector<uint8_t> data = threeWireService.dump8();
        for (uint8_t val : data) {
            if (val != 0xFF) {
                isBlank = false;
                break;
            }
        }
    } else {
        std::vector<uint16_t> data = threeWireService.dump16();
        for (uint16_t val : data) {
            if (val != 0xFFFF) {
                isBlank = false;
                break;
            }
        }
    }

    if (!isBlank) {
        terminalView.println("\n3WIRE EEPROM: 检测到 ✅\n");
    } else {
        terminalView.println("\n3WIRE EEPROM: 未检测到 EEPROM 或 EEPROM 为空 ❌\n");
    }
}

/*
EEPROM 读取
*/
void ThreeWireEepromShell::cmdRead() {
    auto addrStr = userInputManager.readValidatedHexString("起始地址 (例如 00FF00) ", 0, true);
    auto addr = argTransformer.parseHexOrDec16("0x" + addrStr);
    uint8_t count = userInputManager.readValidatedUint8("读取字节数:", 16);
    bool isOrg8 = state.isThreeWireOrg8();

    terminalView.println("");
    if (count == 1) {
        if (isOrg8) {
            uint8_t val = threeWireService.read8(addr);
            terminalView.println("✅ 3WIRE EEPROM: 读取 0x" + argTransformer.toHex(addr, 4) +
                                 " = 0x" + argTransformer.toHex(val, 2));
        } else {
            uint16_t val = threeWireService.read16(addr);
            terminalView.println("✅ 3WIRE EEPROM: [读取] 0x" + argTransformer.toHex(addr, 4) +
                                 " = 0x" + argTransformer.toHex(val, 4));
        }
    } else {
        if (isOrg8) {
            std::vector<uint8_t> values;
            for (uint16_t i = 0; i < count; ++i) {
                values.push_back(threeWireService.read8(addr + i));
            }
            for (size_t i = 0; i < values.size(); i += 16) {
                uint32_t displayAddr = addr + i;
                size_t chunkSize = std::min<size_t>(16, values.size() - i);
                std::vector<uint8_t> chunk(values.begin() + i, values.begin() + i + chunkSize);
                terminalView.println(argTransformer.toAsciiLine(displayAddr, chunk));
            }
        } else {
            std::vector<uint16_t> values;
            for (uint16_t i = 0; i < count; ++i) {
                values.push_back(threeWireService.read16(addr + i));
            }
            for (size_t i = 0; i < values.size(); i += 8) {
                uint32_t displayAddr = (addr + i) * 2;
                size_t chunkSize = std::min<size_t>(8, values.size() - i);
                std::vector<uint16_t> chunk(values.begin() + i, values.begin() + i + chunkSize);
                terminalView.println(argTransformer.toAsciiLine(displayAddr, chunk));
            }
        }
    }
    terminalView.println("");
}

/*
EEPROM 写入
*/
void ThreeWireEepromShell::cmdWrite() {
    auto addrStr = userInputManager.readValidatedHexString("起始地址:", 0, true);
    auto addr = argTransformer.parseHexOrDec16("0x" + addrStr);
    auto hexStr = userInputManager.readValidatedHexString("输入字节值 (例如 01 A5 FF...) ", 0, true);
    auto data = argTransformer.parseHexList(hexStr);

    bool isOrg8 = state.isThreeWireOrg8();
    threeWireService.writeEnable();

    terminalView.println("");
    for (size_t i = 0; i < data.size(); ++i) {
        if (isOrg8) {
            threeWireService.write8(addr + i, data[i]);
            terminalView.println("3WIRE EEPROM: 写入 0x" + argTransformer.toHex(addr + i, 4) +
                                    " = 0x" + argTransformer.toHex(data[i], 2) + " ✅");
        } else {
            if (i + 1 >= data.size()) break; // 不完整
            uint16_t val = (data[i] << 8) | data[i + 1];
            threeWireService.write16(addr + (i / 2), val);
            terminalView.println("3WIRE EEPROM: 写入 0x" + argTransformer.toHex(addr + (i / 2), 4) +
                                    " = 0x" + argTransformer.toHex(val, 4) + " ✅");
            ++i; // 消耗 2 字节
        
        }
    }
    terminalView.println("");

    threeWireService.writeDisable();
}

/*
EEPROM 转储
*/
void ThreeWireEepromShell::cmdDump() {
    bool isOrg8 = state.isThreeWireOrg8();
    uint16_t start = 0;

    terminalView.println("");
    if (isOrg8) {
        auto data = threeWireService.dump8();
        for (size_t i = start; i < data.size(); i += 16) {
            uint32_t addr = i;
            size_t chunkSize = std::min<size_t>(16, data.size() - i);
            std::vector<uint8_t> chunk(data.begin() + i, data.begin() + i + chunkSize);
            terminalView.println(argTransformer.toAsciiLine(addr, chunk));
        }
    } else {
        auto data = threeWireService.dump16();
        for (size_t i = start; i < data.size(); i += 8) {
            uint32_t addr = i * 2;
            size_t chunkSize = std::min<size_t>(8, data.size() - i);
            std::vector<uint16_t> chunk(data.begin() + i, data.begin() + i + chunkSize);
            terminalView.println(argTransformer.toAsciiLine(addr, chunk));
        }
    }
    terminalView.println("");
}

/*
EEPROM 擦除
*/
void ThreeWireEepromShell::cmdErase() {
    
    auto confirmation = userInputManager.readYesNo("确定要擦除 EEPROM 吗?", false);
    if (!confirmation) {
        terminalView.println("\n3WIRE EEPROM: ❌ 擦除已取消.\n");
        return;
    }

    threeWireService.writeEnable();
    threeWireService.eraseAll();
    threeWireService.writeDisable();
    bool isOrg8 = state.isThreeWireOrg8();
    bool success = true;

    if (isOrg8) {
        auto data = threeWireService.dump8();
        for (uint8_t val : data) {
            if (val != 0xFF) {
                success = false;
                break;
            }
        }
    } else {
        auto data = threeWireService.dump16();
        for (uint16_t val : data) {
            if (val != 0xFFFF) {
                success = false;
                break;
            }
        }
    }

    if (success) {
        terminalView.println("\n3WIRE EEPROM: ✅ 擦除成功.\n");
    } else {
        terminalView.println("\n3WIRE EEPROM: ❌ 擦除验证失败.\n");
    }
}