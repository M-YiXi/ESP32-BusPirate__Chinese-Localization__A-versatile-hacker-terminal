#include "HdUartController.h"

/*
Constructor
*/
HdUartController::HdUartController(ITerminalView& terminalView, IInput& terminalInput, IInput& deviceInput,
                                   HdUartService& hdUartService, UartService& uartService, ArgTransformer& argTransformer, UserInputManager& userInputManager)
    : terminalView(terminalView), terminalInput(terminalInput), deviceInput(deviceInput),
      hdUartService(hdUartService), uartService(uartService), argTransformer(argTransformer), userInputManager(userInputManager) {}

/*
Entry point for HDUART commands
*/
void HdUartController::handleCommand(const TerminalCommand& cmd) {
    if      (cmd.getRoot() == "bridge") handleBridge();
    else if (cmd.getRoot() == "config") handleConfig();
    else    handleHelp();
}

/*
Entry point for HDUART instructions
*/
void HdUartController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    auto result = hdUartService.executeByteCode(bytecodes);
    terminalView.println("");
    terminalView.print("HDUART读取: "); // 汉化
    terminalView.println(result.empty() ? "无数据" : "\n\n" + result); // 汉化
    terminalView.println("");
}

/*
Bridge mode read/write
*/
void HdUartController::handleBridge() {
    terminalView.println("HDUART桥接: 运行中... 按下[任意ESP32按键]停止."); // 汉化

    std::string echoBuffer;

    while (true) {
        // Receive UART
        while (hdUartService.available()) {
            char incoming = hdUartService.read();
            if (!echoBuffer.empty() && incoming == echoBuffer.front()) {
                echoBuffer.erase(0, 1); // Filter for echo line
            } else {
                terminalView.print(std::string(1, incoming));
            }
        }

        // Terminal input
        char c = terminalInput.readChar();
        if (c != KEY_NONE) {
            hdUartService.write(c);
            echoBuffer += c; // echo char
        }

        // Device input
        c = deviceInput.readChar();
        if (c != KEY_NONE) {
            terminalView.println("\nHDUART桥接: 已被用户停止."); // 汉化
            break;
        }
    }
}

/*
Config
*/
void HdUartController::handleConfig() {
    terminalView.println("HDUART配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t pin = userInputManager.readValidatedPinNumber("共享TX/RX引脚", state.getHdUartPin(), forbidden); // 汉化
    state.setHdUartPin(pin);

    uint32_t baud = userInputManager.readValidatedUint32("波特率", state.getHdUartBaudRate()); // 汉化
    state.setHdUartBaudRate(baud);

    uint8_t dataBits = userInputManager.readValidatedUint8("数据位(5-8)", state.getHdUartDataBits(), 5, 8); // 汉化
    state.setHdUartDataBits(dataBits);

    char defaultParity = state.getHdUartParity().empty() ? 'N' : state.getHdUartParity()[0];
    char parity = userInputManager.readCharChoice("校验位(N/E/O)", defaultParity, {'N', 'E', 'O'}); // 汉化
    state.setHdUartParity(std::string(1, parity));

    uint8_t stopBits = userInputManager.readValidatedUint8("停止位(1或2)", state.getHdUartStopBits(), 1, 2); // 汉化
    state.setHdUartStopBits(stopBits);

    bool inverted = userInputManager.readYesNo("是否反转信号?", state.isHdUartInverted()); // 汉化
    state.setHdUartInverted(inverted);

    hdUartService.configure(baud, dataBits, parity, stopBits, pin, inverted);

    terminalView.println("HDUART配置已生效.\n"); // 汉化
}

/*
Help
*/
void HdUartController::handleHelp() {
    terminalView.println("未知的HDUART命令. 使用方法:"); // 汉化
    terminalView.println("  bridge       交互模式"); // 汉化
    terminalView.println("  config       设置TX/RX引脚、波特率等参数"); // 汉化
    terminalView.println("  [0x1 r:255]  指令语法"); // 汉化
}

/*
Ensure Configuration
*/
void HdUartController::ensureConfigured() {
    uartService.end();

    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    hdUartService.end();

    // User could have set the same pin to a different usage
    // eg. select UART, then select I2C, then select UART
    // Always reconfigure pins before use
    auto rx = state.getHdUartPin();
    auto parityStr = state.getHdUartParity();
    auto baud = state.getHdUartBaudRate();
    auto dataBits = state.getHdUartDataBits();
    auto stopBits = state.getHdUartStopBits();
    bool inverted = state.isHdUartInverted();
    char parity = !parityStr.empty() ? parityStr[0] : 'N';

    hdUartService.configure(baud, dataBits, parity, stopBits, rx, inverted);
}