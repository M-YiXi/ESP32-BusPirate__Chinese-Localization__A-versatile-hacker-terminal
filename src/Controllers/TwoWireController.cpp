#include "TwoWireController.h"

/*
Constructor
*/
TwoWireController::TwoWireController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    UserInputManager& userInputManager,
    TwoWireService& twoWireService,
    SmartCardShell& smartCardShell
)
    : terminalView(terminalView)
    , terminalInput(terminalInput)
    , userInputManager(userInputManager)
    , twoWireService(twoWireService)
    , smartCardShell(smartCardShell)
{
}

/*
Entry point for 2WIRE command
*/
void TwoWireController::handleCommand(const TerminalCommand& cmd) {
    if      (cmd.getRoot() == "config")    handleConfig();
    else if (cmd.getRoot() == "sniff")     handleSniff();
    else if (cmd.getRoot() == "smartcard") handleSmartCard(cmd);
    else                                   handleHelp();
}

/*
Entry point for 2WIRE instruction
*/
void TwoWireController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    terminalView.println("[待实现] 2WIRE指令支持功能尚未开发。"); // 汉化
}

/*
Sniff
*/
void TwoWireController::handleSniff() {

    // IRAM overflow for M5tick, not enough space left in IRAM
    #ifndef DEVICE_M5STICK

    ensureConfigured();
    terminalView.println("2WIRE 嗅探器: 正在CLK/IO引脚上运行... 按下[ENTER]停止\r\n"); // 汉化

    if (!twoWireService.startSniffer()) {
        terminalView.println("启动嗅探器失败（请检查引脚/配置）。"); // 汉化
        return;
    }

    // Event types
    const uint8_t EVT_START = 1;
    const uint8_t EVT_STOP  = 2;
    const uint8_t EVT_DATA  = 3;

    std::vector<uint8_t> frame; // Accumulates bytes between START and STOP
    bool running = true;

    while (running) {
        // Drain events produced by the sniffer
        uint8_t t, d;
        while (twoWireService.getNextSniffEvent(t, d)) {
            if (t == EVT_START) {
                frame.clear();
            }
            else if (t == EVT_DATA) {
                frame.push_back(d);
            }
            else if (t == EVT_STOP) {
                // Print cmd on a single line
                if (frame.size() == 3) {
                    //  3 bytes => command (OP, A, B)
                    uint8_t op = frame[0], A = frame[1], B = frame[2];
                    const char* name = "UNKNOWN";
                    switch (op) {
                        case 0x30: name = "READ_MAIN";        break;
                        case 0x31: name = "READ_SECURITY";     break;
                        case 0x34: name = "READ_PROTECTION";   break;
                        case 0x33: name = "COMPARE_PSC_BYTE";  break;
                        case 0x38: name = "WRITE_MAIN";        break;
                        case 0x39: name = "WRITE_SECURITY";    break;
                        case 0x3C: name = "WRITE_PROTECTION";  break;
                    }
                    char line[96];
                    snprintf(line, sizeof(line),
                             "指令 %-16s : [%02X %02X %02X]\r\n", // 汉化CMD为指令
                             name, op, A, B);
                    terminalView.print(line);
                } else {
                    // Otherwise, consider it a response
                    terminalView.print("响应数据            :"); // 汉化RESP data为响应数据
                    for (size_t i = 0; i < frame.size(); ++i) {
                        char buf[6];
                        snprintf(buf, sizeof(buf), " %02X", frame[i]);
                        terminalView.print(buf);
                    }
                    terminalView.print("\r\n");
                }
                frame.clear();
            }
        }

        // Exit on [ENTER]
        int ch = terminalInput.readChar();
        if (ch == '\r' || ch == '\n') {
            running = false;
        }
    }

    twoWireService.stopSniffer();
    terminalView.println("\r\n2WIRE 嗅探器: 已被用户停止。"); // 汉化

    #else

    terminalView.println("\r\n2WIRE 嗅探器: M5Stick设备不支持该功能。"); // 汉化

    #endif
}

/*
Smartcard
*/
void TwoWireController::handleSmartCard(const TerminalCommand& cmd) {
    smartCardShell.run();
}

/*
Configuration
*/
void TwoWireController::handleConfig() {
    terminalView.println("2WIRE 配置："); // 汉化
    const auto& forbidden = state.getProtectedPins();

    uint8_t clk = userInputManager.readValidatedPinNumber("CLK引脚", state.getTwoWireClkPin(), forbidden); // 汉化
    state.setTwoWireClkPin(clk);

    uint8_t io = userInputManager.readValidatedPinNumber("IO引脚", state.getTwoWireIoPin(), forbidden); // 汉化
    state.setTwoWireIoPin(io);

    uint8_t rst = userInputManager.readValidatedPinNumber("RST引脚", state.getTwoWireRstPin(), forbidden); // 汉化
    state.setTwoWireRstPin(rst);

    twoWireService.configure(clk, io, rst);

    terminalView.println("2WIRE 配置已生效。\n"); // 汉化
}

/*
Help
*/
void TwoWireController::handleHelp() {
    terminalView.println("未知的2Wire命令。使用方法："); // 汉化
    terminalView.println("  config");
    terminalView.println("  sniff");
    terminalView.println("  smartcard ");
    terminalView.println("  [0xAB r:4] 指令语法 [暂未实现]"); // 汉化
}

/*
Ensure Configuration
*/
void TwoWireController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    } 

    twoWireService.configure(
        state.getTwoWireClkPin(),
        state.getTwoWireIoPin(),
        state.getTwoWireRstPin()
    );
}