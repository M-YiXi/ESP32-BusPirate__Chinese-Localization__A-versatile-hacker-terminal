#include "ActionDispatcher.h"

/*
Constructor
*/
ActionDispatcher::ActionDispatcher(DependencyProvider& provider)
    : provider(provider) {}


/*
Setup
*/
void ActionDispatcher::setup(TerminalTypeEnum terminalType, std::string terminalInfos) {
    provider.getDeviceView().initialize();
    provider.getDeviceView().welcome(terminalType, terminalInfos);

    if (terminalType == TerminalTypeEnum::Serial) {
        provider.getTerminalView().initialize();
        provider.getTerminalView().waitPress();
        provider.getTerminalInput().waitPress();
        provider.getTerminalView().welcome(terminalType, terminalInfos);
    } else {
        provider.getTerminalView().initialize();
        provider.getTerminalView().welcome(terminalType, terminalInfos);
    }
}

/*
Run loop
*/
void ActionDispatcher::run() {
    while (true) {
        auto mode = ModeEnumMapper::toString(state.getCurrentMode());
        provider.getTerminalView().printPrompt(mode);
        std::string action = getUserAction();
        if (action.empty()) {
            continue;
        }
        dispatch(action);
    }
}

/*
Dispatch
*/
void ActionDispatcher::dispatch(const std::string& raw) {    
    if (raw.empty()) return;
    
    char first = raw[0];

    // Instructions
    if (first == '[' || first == '>' || first == '{') {
        std::vector<Instruction> instructions = provider.getInstructionTransformer().transform(raw);
        dispatchInstructions(instructions);
        return;
    }

    // Macros
    if (first == '(') {
        provider.getTerminalView().println("宏功能暂未实现。"); // 汉化
        return;
    }

    // Terminal Command
    TerminalCommand cmd = provider.getCommandTransformer().transform(raw);
    dispatchCommand(cmd);
}

/*
Dispatch Command
*/
void ActionDispatcher::dispatchCommand(const TerminalCommand& cmd) {
    // Mode change command
    if (cmd.getRoot() == "mode" || cmd.getRoot() == "m") {
        ModeEnum maybeNewMode = provider.getUtilityController().handleModeChangeCommand(cmd);
        if (maybeNewMode != ModeEnum::None) {
            setCurrentMode(maybeNewMode);
        }
        return;
    }

    // Global command (help, logic, mode, P, p...)
    if (provider.getUtilityController().isGlobalCommand(cmd)) {
        provider.getUtilityController().handleCommand(cmd);
        if (cmd.getRoot() == "logic"){
            // hack to rerender the pinout view after logic analyzer cmd
            setCurrentMode(state.getCurrentMode());
        } 
        return;
    }

    // Mode specific command
    switch (state.getCurrentMode()) {
        case ModeEnum::HIZ:
            if (state.getTerminalMode() == TerminalTypeEnum::Standalone) {
                provider.getTerminalView().println("请输入 'mode' 选择工作模式。"); // 汉化
                return;
            }
            provider.getTerminalView().println("请输入 'help' 查看帮助或 'mode' 选择模式"); // 汉化
            break;
        case ModeEnum::OneWire:
            provider.getOneWireController().handleCommand(cmd);
            break;
        case ModeEnum::UART:
            provider.getUartController().handleCommand(cmd);
            break;
        case ModeEnum::HDUART:
            provider.getHdUartController().handleCommand(cmd);
            break;
        case ModeEnum::I2C:
            provider.getI2cController().handleCommand(cmd);
            break;
        case ModeEnum::SPI:
            provider.getSpiController().handleCommand(cmd);
            break;
        case ModeEnum::TwoWire:
            provider.getTwoWireController().handleCommand(cmd);
            break;
        case ModeEnum::ThreeWire:
            provider.getThreeWireController().handleCommand(cmd);
            break;
        case ModeEnum::DIO:
            provider.getDioController().handleCommand(cmd);
            break;
        case ModeEnum::LED:
            provider.getLedController().handleCommand(cmd);
            break;
        case ModeEnum::Infrared:
            provider.getInfraredController().handleCommand(cmd);
            break;
        case ModeEnum::USB:
            provider.getUsbController().handleCommand(cmd);
            break;
        case ModeEnum::Bluetooth:
            provider.getBluetoothController().handleCommand(cmd);
            break;
        case ModeEnum::WiFi:
            provider.getWifiController().handleCommand(cmd);
            setCurrentMode(state.getCurrentMode()); // Rerender pinout view after WiFi commands
            break;
        case ModeEnum::JTAG:
            provider.getJtagController().handleCommand(cmd);
            break;
        case ModeEnum::I2S:
            provider.getI2sController().handleCommand(cmd);
            break;
        case ModeEnum::CAN_:
            provider.getCanController().handleCommand(cmd);
            break;
        case ModeEnum::ETHERNET:
            provider.getEthernetController().handleCommand(cmd);
            break;
        case ModeEnum::SUBGHZ:
            provider.getSubGhzController().handleCommand(cmd);
            break;
        case ModeEnum::RFID:
            provider.getRfidController().handleCommand(cmd);
            break;
        case ModeEnum::RF24_:
            provider.getRf24Controller().handleCommand(cmd);
            break;
    }

   // Handled in specific mode, we need to rerender the pinout view
   if (cmd.getRoot() == "config" || cmd.getRoot() == "setprotocol" || cmd.getRoot() == "trace"
       || cmd.getRoot() == "pullup" || cmd.getRoot() == "pulldown" || cmd.getRoot() == "reset"
       || cmd.getRoot() == "swap") {
        setCurrentMode(state.getCurrentMode());
   } 
}

/*
Dispatch Instructions
*/
void ActionDispatcher::dispatchInstructions(const std::vector<Instruction>& instructions) {
    // Convert raw instructions into bytecodes vector
    auto bytecodes = provider.getInstructionTransformer().transformByteCodes(instructions);

    switch (state.getCurrentMode()) {
        case ModeEnum::OneWire:
            provider.getOneWireController().handleInstruction(bytecodes);
            break;
        case ModeEnum::UART:
            provider.getUartController().handleInstruction(bytecodes);
            break;
        case ModeEnum::HDUART:
            provider.getHdUartController().handleInstruction(bytecodes);
            break;
        case ModeEnum::I2C:
            provider.getI2cController().handleInstruction(bytecodes);
            break;
        case ModeEnum::SPI:
            provider.getSpiController().handleInstruction(bytecodes);
            break;
        case ModeEnum::TwoWire:
            provider.getTwoWireController().handleInstruction(bytecodes);
            break;
        case ModeEnum::ThreeWire:
            provider.getThreeWireController().handleInstruction(bytecodes);
            break;
        case ModeEnum::LED:
            provider.getLedController().handleInstruction(bytecodes);
            break;
        default:
            provider.getTerminalView().println("当前模式下无法执行指令。"); // 汉化
            return;
    }

    // Line by line bytecode
    provider.getTerminalView().println("");
    provider.getTerminalView().println("字节码序列："); // 汉化
    for (const auto& code : bytecodes) {
        provider.getTerminalView().println(
            ByteCodeEnumMapper::toString(code.getCommand()) +
            " | 数据=" + std::to_string(code.getData()) + // 汉化
            " | 位数=" + std::to_string(code.getBits()) + // 汉化
            " | 重复次数=" + std::to_string(code.getRepeat()) // 汉化
        );
    }
    provider.getTerminalView().println("");
}

/*
User Action
*/
std::string ActionDispatcher::getUserAction() {
    std::string inputLine;
    auto mode = ModeEnumMapper::toString(state.getCurrentMode());
    size_t cursorIndex = 0;

    while (true) {
        provider.getDeviceInput().readChar(); // to check shutdown request for T-Embeds
        char c = provider.getTerminalInput().readChar();
        if (c == KEY_NONE) continue;

        if (handleCardputerEscapeSequence(c, cursorIndex, inputLine, mode)) continue;
        if (handleEscapeSequence(c, inputLine, cursorIndex, mode)) continue;
        if (handleEnterKey(c, inputLine)) return inputLine;
        if (handleBackspace(c, inputLine, cursorIndex, mode)) continue;
        if (handlePrintableChar(c, inputLine, cursorIndex, mode));
    }
}

/*
User Action: Cardputer Special Arrows, standalone mode only
*/
bool ActionDispatcher::handleCardputerEscapeSequence(char c, size_t& cursorIndex, std::string& inputLine, const std::string& mode) {
    if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
        return false;
    }

    if (c == CARDPUTER_SPECIAL_ARROW_UP) {
        provider.getTerminalView().print(std::string(1, CARDPUTER_SPECIAL_ARROW_UP));
    } else if (c == CARDPUTER_SPECIAL_ARROW_DOWN) {
        provider.getTerminalView().print(std::string(1, CARDPUTER_SPECIAL_ARROW_DOWN));
    } else if (c == '\t') {
        if (c != '\t') return false;

        // Clear current line
        inputLine.clear();

        // Recall previous command from history
        inputLine = provider.getCommandHistoryManager().up();

        // Move cursor to end and redraw
        cursorIndex = inputLine.length();
        provider.getTerminalView().print("\r" + mode + "> " + inputLine + "\033[K");

        return true;
    } else {
        return false;
    }

    return true;
}

/*
User Action: Escape
*/
bool ActionDispatcher::handleEscapeSequence(char c, std::string& inputLine, size_t& cursorIndex, const std::string& mode) {
    if (c != '\x1B') return false;

    if (provider.getTerminalInput().readChar() == '[') {
        char next = provider.getTerminalInput().readChar();

        if (next == 'A') {
            inputLine = provider.getCommandHistoryManager().up();
            cursorIndex = inputLine.length();
        } else if (next == 'B') {
            inputLine = provider.getCommandHistoryManager().down();
            cursorIndex = inputLine.length();
        } else if (next == 'C') {
            if (cursorIndex < inputLine.length()) {
                cursorIndex++;
                provider.getTerminalView().print("\x1B[C");
            }
            return true;
        } else if (next == 'D') {
            if (cursorIndex > 0) {
                cursorIndex--;
                provider.getTerminalView().print("\x1B[D");
            }
            return true;
        } else {
            return false;
        }

        provider.getTerminalView().print("\r" + mode + "> " + inputLine + "\033[K");
        return true;
    }

    return false;
}

/*
User Action: Enter
*/
bool ActionDispatcher::handleEnterKey(char c, const std::string& inputLine) {
    if (c != '\r' && c != '\n') return false;

    provider.getTerminalView().println("");
    provider.getCommandHistoryManager().add(inputLine);
    return true;
}

/*
User Action: Backspace
*/
bool ActionDispatcher::handleBackspace(char c, std::string& inputLine, size_t& cursorIndex, const std::string& mode) {
    if (c != '\b' && c != 127) return false;
    if (cursorIndex == 0) return true;

    cursorIndex--;
    inputLine.erase(cursorIndex, 1);

    provider.getTerminalView().print("\r" + mode + "> " + inputLine + " \033[K");

    int moveBack = inputLine.length() - cursorIndex;
    for (int i = 0; i <= moveBack; ++i) {
        provider.getTerminalView().print("\x1B[D");
    }

    return true;
}

/*
User Action: Printable
*/
bool ActionDispatcher::handlePrintableChar(char c, std::string& inputLine, size_t& cursorIndex, const std::string& mode) {
    if (!isprint(c)) return false;

    inputLine.insert(cursorIndex, 1, c);
    cursorIndex++;

    provider.getTerminalView().print("\r" + mode + "> " + inputLine + "\033[K");

    int moveBack = inputLine.length() - cursorIndex;
    for (int i = 0; i < moveBack; ++i) {
        provider.getTerminalView().print("\x1B[D");
    }

    return true;
}

/*
Set Mode
*/
void ActionDispatcher::setCurrentMode(ModeEnum newMode) {
    PinoutConfig config;
    state.setCurrentMode(newMode);
    config.setMode(ModeEnumMapper::toString(newMode));
    auto proto = InfraredProtocolMapper::toString(state.getInfraredProtocol());

    switch (newMode) {
        case ModeEnum::HIZ:
            provider.disableAllProtocols();
            break;
        case ModeEnum::OneWire:
            provider.getOneWireController().ensureConfigured();
            config.setMappings({ "数据引脚 GPIO " + std::to_string(state.getOneWirePin()) }); // 汉化
            break;
        case ModeEnum::UART:
            provider.getUartController().ensureConfigured();
            config.setMappings({
                "发送引脚 GPIO " + std::to_string(state.getUartTxPin()), // 汉化
                "接收引脚 GPIO " + std::to_string(state.getUartRxPin()), // 汉化
                "波特率 " + std::to_string(state.getUartBaudRate()), // 汉化
                "数据位 " + std::to_string(state.getUartDataBits()), // 汉化
            });
            break;
        case ModeEnum::HDUART:
            provider.getHdUartController().ensureConfigured();
            config.setMappings({
                "收发引脚 GPIO " + std::to_string(state.getHdUartPin()), // 汉化
                "波特率 " + std::to_string(state.getHdUartBaudRate()), // 汉化
                "数据位 " + std::to_string(state.getHdUartDataBits()), // 汉化
                "校验位 " + state.getHdUartParity(), // 汉化
            });
            break;
        case ModeEnum::I2C:
            provider.getI2cController().ensureConfigured();
            config.setMappings({
                "SDA引脚 GPIO " + std::to_string(state.getI2cSdaPin()), // 汉化
                "SCL引脚 GPIO " + std::to_string(state.getI2cSclPin()), // 汉化
                "频率 " + std::to_string(state.getI2cFrequency()) // 汉化
            });
            break;
        case ModeEnum::SPI:
            provider.getSpiController().ensureConfigured();
            config.setMappings({
                "MOSI引脚 GPIO " + std::to_string(state.getSpiMOSIPin()), // 汉化
                "MISO引脚 GPIO " + std::to_string(state.getSpiMISOPin()), // 汉化
                "时钟引脚 GPIO " + std::to_string(state.getSpiCLKPin()), // 汉化
                "片选引脚 GPIO " + std::to_string(state.getSpiCSPin()) // 汉化
            });
            break;
        case ModeEnum::TwoWire:
            provider.getTwoWireController().ensureConfigured();
            config.setMappings({
                "数据引脚 GPIO " + std::to_string(state.getTwoWireIoPin()), // 汉化
                "时钟引脚 GPIO " + std::to_string(state.getTwoWireClkPin()), // 汉化
                "复位引脚 GPIO " + std::to_string(state.getTwoWireRstPin()) // 汉化
            });
            break;
        case ModeEnum::ThreeWire:
            provider.getThreeWireController().ensureConfigured();
            config.setMappings({
                "片选引脚 GPIO " + std::to_string(state.getThreeWireCsPin()), // 汉化
                "时钟引脚 GPIO " + std::to_string(state.getThreeWireSkPin()), // 汉化
                "输入引脚 GPIO " + std::to_string(state.getThreeWireDiPin()), // 汉化
                "输出引脚 GPIO " + std::to_string(state.getThreeWireDoPin()) // 汉化
            });
            break;
        case ModeEnum::DIO: {
            config.setMappings(provider.getDioController().buildPullConfigLines());
            break;
        }
        case ModeEnum::LED:
            provider.getLedController().ensureConfigured();
            config.setMappings({
                "数据引脚 GPIO " + std::to_string(state.getLedDataPin()), // 汉化
                "时钟引脚 GPIO " + std::to_string(state.getLedClockPin()), // 汉化
                "LED数量 " + std::to_string(state.getLedLength()), // 汉化
                state.getLedProtocol()
            });
            break;
        case ModeEnum::Infrared:
            provider.getInfraredController().ensureConfigured();
            config.setMappings(
                {"红外发送引脚 GPIO " + std::to_string(state.getInfraredTxPin()), // 汉化
                 "红外接收引脚 GPIO " + std::to_string(state.getInfraredRxPin()), // 汉化
                  proto
            });
            break;
        case ModeEnum::USB:
            provider.getUsbController().ensureConfigured();
            break;
        case ModeEnum::Bluetooth:
            provider.getBluetoothController().ensureConfigured();
            break;
        case ModeEnum::WiFi:
            provider.getWifiController().ensureConfigured();
            config.setMappings(provider.getWifiController().buildWiFiLines());
            break;
        case ModeEnum::JTAG: {
            provider.getJtagController().ensureConfigured();
            std::vector<std::string> lines;
            const auto& pins = state.getJtagScanPins();
            size_t totalPins = pins.size();

            for (size_t i = 0; i < std::min(totalPins, size_t(4)); ++i) {
                std::string line = "扫描引脚 GPIO " + std::to_string(pins[i]); // 汉化

                // only 4 lines can be displayed
                if (i == 3 && totalPins > 4) {
                    line += " ...";
                }

                lines.push_back(line);
            }

            config.setMappings(lines);
            break;
        }
        case ModeEnum::I2S:
            provider.getI2sController().ensureConfigured();
            config.setMappings({
                "位时钟引脚 GPIO " + std::to_string(state.getI2sBclkPin()), // 汉化
                "帧时钟引脚 GPIO " + std::to_string(state.getI2sLrckPin()), // 汉化
                "数据引脚 GPIO " + std::to_string(state.getI2sDataPin()), // 汉化
                "采样率 " + std::to_string(state.getI2sSampleRate()) // 汉化
            });
            break;

        case ModeEnum::CAN_:
            provider.getCanController().ensureConfigured();
            config.setMappings({
                "片选引脚 GPIO " + std::to_string(state.getCanCspin()), // 汉化
                "时钟引脚 GPIO " + std::to_string(state.getCanSckPin()), // 汉化
                "输入引脚 GPIO " + std::to_string(state.getCanSiPin()), // 汉化
                "输出引脚 GPIO " + std::to_string(state.getCanSoPin()) // 汉化
            });
            break;
        case ModeEnum::ETHERNET:
            provider.getEthernetController().ensureConfigured();
            config.setMappings({
                "片选引脚 GPIO " + std::to_string(state.getEthernetCsPin()), // 汉化
                "时钟引脚 GPIO " + std::to_string(state.getEthernetSckPin()), // 汉化
                "输出引脚 GPIO " + std::to_string(state.getEthernetMosiPin()), // 汉化
                "输入引脚 GPIO " + std::to_string(state.getEthernetMisoPin()) // 汉化
            });
            break;

        case ModeEnum::SUBGHZ:
            provider.getSubGhzController().ensureConfigured();
            config.setMappings({
                "时钟引脚 GPIO " + std::to_string(state.getSubGhzSckPin()), // 汉化
                "输入引脚 GPIO " + std::to_string(state.getSubGhzMisoPin()), // 汉化
                "输出引脚 GPIO " + std::to_string(state.getSubGhzMosiPin()), // 汉化
                "片选引脚 GPIO " + std::to_string(state.getSubGhzCsPin()), // 汉化
            });
            break;

        case ModeEnum::RFID:
            provider.getRfidController().ensureConfigured();
            config.setMappings({
                "RFID数据引脚 GPIO " + std::to_string(state.getRfidSdaPin()), // 汉化
                "RFID时钟引脚 GPIO " + std::to_string(state.getRfidSclPin()) // 汉化
            });
            break;

        case ModeEnum::RF24_:
            provider.getRf24Controller().ensureConfigured();
            config.setMappings({
                "使能引脚 GPIO " + std::to_string(state.getRf24CePin()), // 汉化
                "片选引脚 GPIO " + std::to_string(state.getRf24CsnPin()), // 汉化
                "时钟引脚 GPIO " + std::to_string(state.getRf24SckPin()), // 汉化
                "输出引脚 GPIO " + std::to_string(state.getRf24MosiPin()) // 汉化
            });
            break;
    }

    // Show the new mode pinout
    provider.getDeviceView().show(config);
}