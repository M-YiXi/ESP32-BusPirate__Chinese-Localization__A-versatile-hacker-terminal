#include "JtagController.h"

/*
Constructor
*/
JtagController::JtagController(ITerminalView& terminalView, IInput& terminalInput, JtagService& jtagService, UserInputManager& userInputManager)
    : terminalView(terminalView), terminalInput(terminalInput), jtagService(jtagService), userInputManager(userInputManager) {}

/*
Entry point that handles JTAG commands
*/
void JtagController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "scan") handleScan(cmd); 
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Scan
*/
void JtagController::handleScan(const TerminalCommand& cmd) {
    auto type = cmd.getSubcommand();

    if (type[0] == 's') handleScanSwd();
    else if (type[0] == 'j') handleScanJtag();
}

/*
Scan SWD
*/
void JtagController::handleScanSwd() {
    terminalView.println("JTAG: 正在扫描SWD设备..."); // 汉化

    uint8_t swdio, swclk;
    uint32_t idcode;
    std::vector<uint8_t> swdCandidates = state.getJtagScanPins();

    bool found = jtagService.scanSwdDevice(swdCandidates, swdio, swclk, idcode);

    if (found) {
        terminalView.println("\n 找到SWD设备!"); // 汉化
        terminalView.println("  • SWDIO  : GPIO " + std::to_string(swdio));
        terminalView.println("  • SWCLK  : GPIO " + std::to_string(swclk));
        terminalView.println("  • IDCODE : 0x" + std::to_string(idcode));
        terminalView.println("  ✅ SWD扫描完成.\n"); // 汉化
    } else {
        terminalView.println("\nJTAG: 未在可用GPIO上找到SWD设备。"); // 汉化
    }
}

/*
Scan JTAG
*/
void JtagController::handleScanJtag() {    
    terminalView.println("JTAG: 正在扫描JTAG设备..."); // 汉化

    std::vector<uint8_t> jtagCandidates = state.getJtagScanPins();
    uint8_t tdi, tdo, tck, tms;
    int trst;
    std::vector<uint32_t> ids;

    bool found = jtagService.scanJtagDevice(
        jtagCandidates,
        tdi, tdo, tck, tms, trst,
        ids,
        true, // detect pullup
        nullptr // callback progression
    );

    if (found) {
        terminalView.println("\n 找到JTAG设备!"); // 汉化
        terminalView.println("  • TDI   : GPIO " + std::to_string(tdi));
        terminalView.println("  • TDO   : GPIO " + std::to_string(tdo));
        terminalView.println("  • TCK   : GPIO " + std::to_string(tck));
        terminalView.println("  • TMS   : GPIO " + std::to_string(tms));
        if (trst >= 0) {
            terminalView.println("  • TRST  : GPIO " + std::to_string(trst));
        }

        for (size_t i = 0; i < ids.size(); ++i) {
            char buf[11];
            snprintf(buf, sizeof(buf), "0x%08X", ids[i]);
            terminalView.println("  • IDCODE[" + std::to_string(i) + "] : " + buf);
        }

        terminalView.println("  ✅ 扫描完成.\n"); // 汉化
    } else {
        terminalView.println("\nJTAG: 未在可用GPIO上找到JTAG设备。"); // 汉化
    }
}

/*
Config
*/
void JtagController::handleConfig() {
    terminalView.println("JTAG/SWD配置:"); // 汉化

    // Default
    const std::vector<uint8_t> defaultPins = state.getJtagScanPins();

    // Protected
    const std::vector<uint8_t> protectedPins = state.getProtectedPins();

    // Ask user for pin group
    std::vector<uint8_t> selectedPins = userInputManager.readValidatedPinGroup(
        "要扫描的GPIO引脚（SWD/JTAG）", // 汉化
        defaultPins,
        protectedPins
    );

    // Save it
    state.setJtagScanPins(selectedPins);

    // Show confirmation
    terminalView.print("已设置扫描引脚（SWD/JTAG）："); // 汉化
    for (uint8_t pin : selectedPins) {
        terminalView.print(std::to_string(pin) + " ");
    }
    terminalView.println("\r\nJTAG/SWD配置完成.\n"); // 汉化
}

/*
Help
*/
void JtagController::handleHelp() {
    terminalView.println("");
    terminalView.println("未知的JTAG命令。使用方法："); // 汉化
    terminalView.println("  scan swd");
    terminalView.println("  scan jtag");
    terminalView.println("  config");
    terminalView.println("");
}

/*
Ensure Configuration
*/
void JtagController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }
}