
#include "SmartCardShell.h"

SmartCardShell::SmartCardShell(
    TwoWireService& twoWireService,
    ITerminalView& terminalView,
    IInput& terminalInput,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager
)
    : twoWireService(twoWireService),
      terminalView(terminalView),
      terminalInput(terminalInput),
      argTransformer(argTransformer),
      userInputManager(userInputManager)
{}

void SmartCardShell::run() {
    const std::vector<std::string> actions = {
        " 🔍 探测",
        " 🛡️  安全检查",
        " 🔓 解锁卡片",
        " 📝 设置 PSC",
        " 📋 获取 PSC",
        " ✏️  写入",
        " 🗃️  转储",
        " 🚫 保护",
        " 🚪 退出命令行"
    };

    while (true) {
        terminalView.println("\n=== SLE44XX 智能卡命令行 ===");
        int index = userInputManager.readValidatedChoiceIndex("选择智能卡操作", actions, 0);

        if (index == -1 || actions[index] == " 🚪 退出命令行") {
            terminalView.println("正在退出智能卡命令行...\n");
            break;
        }

        switch (index) {
            case 0: cmdProbe();     break;
            case 1: cmdSecurity();  break;
            case 2: cmdUnlock();    break;
            case 3: cmdPsc("set");  break;
            case 4: cmdPsc("get");  break;
            case 5: cmdWrite();     break;
            case 6: cmdDump();      break;
            case 7: cmdProtect();   break;
            default:
                terminalView.println("未知选项.\n");
                break;
        }
    }
}

/*
智能卡安全检查
*/
void SmartCardShell::cmdSecurity() {
    twoWireService.resetSmartCard();

    terminalView.println("2WIRE 安全检查: 正在执行...\n");
    
    // 安全存储器
    terminalView.println("   [安全存储器] 命令: 0x31 0x00 0x00");
    twoWireService.sendCommand(0x31, 0x00, 0x00);
    auto sec = twoWireService.readResponse(4);

    // 验证
    bool allZero = std::all_of(sec.begin(), sec.end(), [](uint8_t b) { return b == 0x00; });
    bool allFF   = std::all_of(sec.begin(), sec.end(), [](uint8_t b) { return b == 0xFF; });
    if (sec.empty() || allZero || allFF) {
        terminalView.println("2WIRE 安全检查: ❌ 未检测到智能卡 (响应无效)");
        return;
    }
    
    // 显示
    std::stringstream secOut;
    secOut << "   安全字节: ";
    for (auto b : sec) secOut << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
    terminalView.println(secOut.str());

    if (!sec.empty()) {
        uint8_t attempts = twoWireService.parseSmartCardRemainingAttempts(sec[0]);
        terminalView.println("   剩余解锁尝试次数: " + std::to_string(attempts));
    }

    terminalView.println("\n2WIRE 安全检查: ✅ 完成.");
}

/*
智能卡探测 (ATR)
*/
void SmartCardShell::cmdProbe() {
    terminalView.println("\n2WIRE ATR: 正在执行...\n");

    // ATR
    auto atr = twoWireService.performSmartCardAtr();
    std::stringstream ss;
    ss << "ATR: ";

    // 验证
    if (atr.empty() || atr[0] == 0x00 || atr[0] == 0xFF) {
        terminalView.println("2WIRE ATR: ❌ 未收到智能卡响应");
        return;
    }

    // 解码并显示
    auto decodedAtr = twoWireService.parseSmartCardAtr(atr);
    for (uint8_t b : atr) {
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    terminalView.println(decodedAtr);
    
    twoWireService.resetSmartCard();
    terminalView.println("2WIRE ATR: ✅ 完成.");
}

/*
智能卡转储
*/
void SmartCardShell::cmdDump() {
    twoWireService.resetSmartCard();
    delay(10);
    terminalView.println("\n2WIRE 转储: 正在读取完整存储器 (MAIN + SEC + PROTECT)...");

    // 转储 256 字节 + 安全存储器 + 保护存储器
    auto dump = twoWireService.dumpSmartCardFullMemory();
    if (dump.size() != 264) {
        terminalView.println("\n2WIRE 转储: ❌ 失败, 大小不符.");
        return;
    }

    // 验证数据
    bool allZero = std::all_of(dump.begin(), dump.end(), [](uint8_t b) { return b == 0x00; });
    bool allFF   = std::all_of(dump.begin(), dump.end(), [](uint8_t b) { return b == 0xFF; });
    if (allZero || allFF) {
        terminalView.println("\n2WIRE 转储: ❌ 智能卡为空或未检测到智能卡");
        return;
    }

    // 主存储器 (0-255)
    terminalView.println("\n[主存储器]");
    for (int i = 0; i < 256; i += 16) {
        std::stringstream line;
        line << std::hex << std::uppercase << std::setfill('0');
        line << std::setw(2) << i << ": ";
        for (int j = 0; j < 16; ++j) {
            line << std::setw(2) << static_cast<int>(dump[i + j]) << " ";
        }
        terminalView.println(line.str());
    }

    // 安全存储器 (256-259)
    terminalView.println("\n[安全存储器]");
    std::stringstream sec;
    sec << "SEC: ";
    for (int i = 256; i < 260; ++i) {
        sec << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(dump[i]) << " ";
    }
    uint8_t attempts = twoWireService.parseSmartCardRemainingAttempts(dump[256]);
    sec << "→ 剩余尝试次数: " << std::dec << (int)attempts;
    terminalView.println(sec.str());

    // 保护存储器 (260-263)
    terminalView.println("\n[保护存储器]");
    std::stringstream prt;
    prt << "PRT: ";
    for (int i = 260; i < 264; ++i) {
        prt << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(dump[i]) << " ";
    }
    terminalView.println(prt.str());

    twoWireService.resetSmartCard();
    terminalView.println("\n2WIRE 转储: ✅ 完成.");
}

/*
智能卡保护
*/
void SmartCardShell::cmdProtect() {
    twoWireService.resetSmartCard();
    terminalView.println("⚠️ 智能卡将永久禁用主存储器写入.");
    bool confirm = userInputManager.readYesNo("确定要永久锁定吗?", false);
    if (!confirm) {
        terminalView.println("\n❌ 锁定已取消.");
        return;
    }

    bool ok = twoWireService.protectSmartCard();
    if (ok) terminalView.println("\n✅ 智能卡成功锁定 (写入已禁用).");
    else    terminalView.println("\n❌ 锁定智能卡失败.");
}

/*
智能卡解锁
*/
void SmartCardShell::cmdUnlock() {
    twoWireService.resetSmartCard();
    terminalView.println("2WIRE 解锁: 正在尝试解锁过程...");

    // 提示输入 PSC (PIN 码)
    auto pscStr = userInputManager.readValidatedHexString("输入 PSC (PIN 码) (例如: 123456)", 3);
    auto psc = argTransformer.parseHexList(pscStr);

    // 解锁
    bool success = twoWireService.unlockSmartCard(psc.data());

    if (success) {
        terminalView.println("\n✅ 解锁成功: 已授予主存储器访问权限.");
    } else {
        terminalView.println("\n❌ 解锁失败: PSC 错误或无剩余尝试次数.");
    }

    // 剩余尝试次数
    auto secAfter = twoWireService.readSmartCardSecurityMemory();
    if (!secAfter.empty()) {
        uint8_t attempts = twoWireService.parseSmartCardRemainingAttempts(secAfter[0]);
        terminalView.println("→ 剩余尝试次数: " + std::to_string(attempts));
    }
}

/*
智能卡 PSC (PIN 码)
*/
void SmartCardShell::cmdPsc(const std::string& subcommand) {
    twoWireService.resetSmartCard();
    std::string arg = subcommand;

    if (arg.empty()) {
        arg = "get"; // 默认 "get"
    }

    // 获取 PSC
    if (arg == "get") {
        uint8_t psc[3];
        bool ok = twoWireService.getSmartCardPSC(psc);
        if (ok) {
            terminalView.println("\nℹ️  注意: 仅当智能卡解锁时才能读取 PSC (PIN 码).");
            std::stringstream ss;
            ss << "🔐 当前 PSC (PIN 码): ";
            for (int i = 0; i < 3; ++i)
                ss << std::hex << std::setw(2) << std::setfill('0') << (int)psc[i] << " ";
            terminalView.println(ss.str());

        } else {
            terminalView.println("\n❌ 读取 PSC (PIN 码) 失败.");
        }

    // 设置 PSC
    } else if (arg == "set") {
        // 提示输入 PSC (PIN 码)
        auto pscStr = userInputManager.readValidatedHexString("输入 PSC (PIN 码) (例如: 123456)", 3);
        auto psc = argTransformer.parseHexList(pscStr);

        bool ok = twoWireService.updateSmartCardPSC(psc.data());
        if (ok) {
            terminalView.println("\n✅ PSC (PIN 码) 更新成功.");
        } else {
            terminalView.println("\nℹ️  注意: 仅当智能卡解锁时才能设置 PSC (PIN 码).");
            terminalView.println("❌ 更新 PSC (PIN 码) 失败.");
        }
    }
}

// 智能卡写入
void SmartCardShell::cmdWrite() {
    twoWireService.resetSmartCard();

    int offset = userInputManager.readValidatedUint8("输入偏移量 (0-255 或 0x..)", 0);
    if (offset < 0 || offset >= 256) {
        terminalView.println("\n❌ 无效偏移量 (必须在 0 到 255 之间).");
        return;
    }

    int data = userInputManager.readValidatedUint8("输入数据字节 (0-255 或 0x..)", 0);
    if (data < 0 || data > 0xFF) {
        terminalView.println("\n❌ 无效数据字节.");
        return;
    }

    bool ok = twoWireService.writeSmartCardMainMemory(static_cast<uint8_t>(offset), static_cast<uint8_t>(data));
    if (ok) terminalView.println("\n✅ 写入成功.");
    else  {
        terminalView.println("\nℹ️  注意: 如果无法写入, 请先解锁智能卡.");
        terminalView.println("❌ 写入失败.");
    }
}