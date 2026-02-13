#ifndef DEVICE_M5STICK 

#include "UsbS3Controller.h"

/*
Constructor
*/
UsbS3Controller::UsbS3Controller(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IInput& deviceInput,
    IUsbService& usbService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager
)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      deviceInput(deviceInput),
      usbService(usbService),
      argTransformer(argTransformer),
      userInputManager(userInputManager)
{}

/*
Entry point for command
*/
void UsbS3Controller::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "stick") handleUsbStick();
    else if (cmd.getRoot() == "keyboard") handleKeyboard(cmd);
    else if (cmd.getRoot() == "mouse") handleMouse(cmd);
    else if (cmd.getRoot() == "gamepad") handleGamepad(cmd);
    else if (cmd.getRoot() == "reset") handleReset();
    else if (cmd.getRoot() == "config") handleConfig();
    else handleHelp();
}

/*
Keyboard
*/
void UsbS3Controller::handleKeyboard(const TerminalCommand& cmd) {
    auto sub = cmd.getSubcommand();

    if (sub.empty()) handleKeyboardBridge();
    else if (sub == "bridge") handleKeyboardBridge();
    else handleKeyboardSend(cmd);
}

/*
Keyboard Send
*/
void UsbS3Controller::handleKeyboardSend(const TerminalCommand& cmd) {
    terminalView.println("USB 键盘: 正在配置..."); // 汉化
    usbService.keyboardBegin();
    terminalView.println("USB 键盘: 初始化..."); // 汉化
    auto full = cmd.getArgs().empty() ? cmd.getSubcommand() : cmd.getSubcommand() + " " + cmd.getArgs();
    usbService.keyboardSendString(full);
    // usbService.reset();
    terminalView.println("USB 键盘: 字符串已发送。"); // 汉化
}

/*
Keyboard Bridge
*/
void UsbS3Controller::handleKeyboardBridge() {
    terminalView.println("USB 键盘桥接: 所有按键将发送至USB HID设备。"); // 汉化
    usbService.keyboardBegin();

    auto sameHost = false;
    if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
        terminalView.println("\n[警告] 如果USB设备与终端连接到同一主机，"); // 汉化
        terminalView.println("       可能会因回车键导致循环问题。"); // 汉化
        terminalView.println("       （将键盘桥接到同一主机无实际意义）\n"); // 汉化
        
        sameHost = userInputManager.readYesNo("是否连接到同一主机？(是/否)", true); // 汉化
    
        if (sameHost) {
            terminalView.println("同一主机模式，回车键将不会发送至USB HID设备。"); // 汉化
        }
    }    

    terminalView.println("USB 键盘: 桥接已启动.. 按下[任意ESP32按键]停止。"); // 汉化

    while (true) {
        // Esp32 button to stop
        char k = deviceInput.readChar();
        if (k != KEY_NONE) {
            terminalView.println("\r\nUSB 键盘桥接: 已被用户停止。"); // 汉化
            break;
        }
        
        // Terminal to keyboard hid
        char c = terminalInput.readChar();

        // if we send \n in the terminal web browser
        // and the usb hid are on the same host
        // then it will loop infinitely
        if (c != KEY_NONE) { 
            if (c == '\n' && sameHost) continue;
            usbService.keyboardSendString(std::string(1, c));
            delay(20); // slow down looping
        }
    }
}

/*
Mouse Move
*/
void UsbS3Controller::handleMouseMove(const TerminalCommand& cmd) {
    int x, y = 0;

    // mouse move x y
    if (cmd.getSubcommand() == "move") {
        auto args = argTransformer.splitArgs(cmd.getArgs());
        if (args.size() < 2 ||
            argTransformer.isValidSignedNumber(args[0]) == false ||
            argTransformer.isValidSignedNumber(args[1]) == false) {
            terminalView.println("使用方法: mouse move <x> <y>"); // 汉化
            return;
        }
        x = argTransformer.toClampedInt8(args[0]);
        y = argTransformer.toClampedInt8(args[1]);
    // mouse x y
    } else {
        if (argTransformer.isValidSignedNumber(cmd.getSubcommand()) == false ||
            argTransformer.isValidSignedNumber(cmd.getArgs()) == false) {
            terminalView.println("使用方法: mouse <x> <y>"); // 汉化
            return;
        }
        x = argTransformer.toClampedInt8(cmd.getSubcommand());
        y = argTransformer.toClampedInt8(cmd.getArgs());
    }

    usbService.mouseMove(x, y);
    terminalView.println("USB 鼠标: 移动偏移量 (" + std::to_string(x) + ", " + std::to_string(y) + ")"); // 汉化
}

/*
Mouse Click
*/
void UsbS3Controller::handleMouseClick() {
    // Left click
    usbService.mouseClick(1);
    delay(100);
    usbService.mouseRelease(1);
    terminalView.println("USB 鼠标: 单击指令已发送。"); // 汉化
}

/*
Mouse
*/
void UsbS3Controller::handleMouse(const TerminalCommand& cmd)  {
    
    if (cmd.getSubcommand().empty()) {
        terminalView.println("使用方法: mouse <x> <y>"); // 汉化
        terminalView.println("       mouse click");
        terminalView.println("       mouse jiggle [毫秒]"); // 汉化
        return;
    }

    terminalView.println("USB 鼠标: 配置HID设备..."); // 汉化
    usbService.mouseBegin();
    terminalView.println("USB 鼠标: 初始化HID设备..."); // 汉化

    if (cmd.getSubcommand() == "click") handleMouseClick();
    else if (cmd.getSubcommand() == "jiggle") handleMouseJiggle(cmd);
    else handleMouseMove(cmd);
}

/*
Mouse Jiggle
*/
void UsbS3Controller::handleMouseJiggle(const TerminalCommand& cmd) {
    int intervalMs = 1000; // defaut

    if (!cmd.getArgs().empty() && argTransformer.isValidNumber(cmd.getArgs())) {
        auto intervalMs = argTransformer.parseHexOrDec32(cmd.getArgs());
    }

    terminalView.println("USB 鼠标: 随机移动已启动（间隔 " + std::to_string(intervalMs) + " 毫秒）... 按下[ENTER]停止。"); // 汉化

    while (true) {
        // Random moves
        int dx = (int)random(-127, 127);
        int dy = (int)random(-127, 127);
        if (dx == 0 && dy == 0) dx = 1;

        usbService.mouseMove(dx, dy);

        // wait interval while listening for ENTER
        unsigned long t0 = millis();
        while ((millis() - t0) < intervalMs) {
            auto c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("USB 鼠标: 随机移动已停止。\n"); // 汉化
                return;
            }
            delay(10);
        }
    }
}

/*
Gamepad
*/
void UsbS3Controller::handleGamepad(const TerminalCommand& cmd) {
    terminalView.println("USB 游戏手柄: 配置HID设备..."); // 汉化
    usbService.gamepadBegin();

    std::string subcmd = cmd.getSubcommand();
    std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), ::tolower);

    if (subcmd == "up" || subcmd == "down" || subcmd == "left" || subcmd == "right" ||
        subcmd == "a" || subcmd == "b") {
        
        usbService.gamepadPress(subcmd);
        terminalView.println("USB 游戏手柄: 按键指令已发送。"); // 汉化

    } else {
        terminalView.println("USB 游戏手柄: 未知输入。请尝试 up, down, left, right, a, b"); // 汉化
    }
}

/*
Stick
*/
void UsbS3Controller::handleUsbStick() {
    terminalView.println("USB 存储棒: 启动中... USB驱动器可能需要30秒才能识别"); // 汉化
    usbService.storageBegin(state.getSdCardCsPin(), state.getSdCardClkPin(),
                            state.getSdCardMisoPin(), state.getSdCardMosiPin());

    if (usbService.isStorageActive()) {
        terminalView.println("\n ✅ USB 存储棒配置完成。正在挂载驱动器... (最多需要30秒)\n"); // 汉化
    } else {
        terminalView.println("\n ❌ USB 存储棒配置失败。未检测到SD卡。\n"); // 汉化
    }
}

/*
Config
*/
void UsbS3Controller::handleConfig() {
    terminalView.println("USB 配置:"); // 汉化

    auto confirm = userInputManager.readYesNo("是否为USB配置SD卡引脚？", false); // 汉化

    if (confirm) {
        const auto& forbidden = state.getProtectedPins();
    
        uint8_t cs = userInputManager.readValidatedPinNumber("SD卡 CS引脚", state.getSdCardCsPin(), forbidden); // 汉化
        state.setSdCardCsPin(cs);
    
        uint8_t clk = userInputManager.readValidatedPinNumber("SD卡 CLK引脚", state.getSdCardClkPin(), forbidden); // 汉化
        state.setSdCardClkPin(clk);
    
        uint8_t miso = userInputManager.readValidatedPinNumber("SD卡 MISO引脚", state.getSdCardMisoPin(), forbidden); // 汉化
        state.setSdCardMisoPin(miso);
    
        uint8_t mosi = userInputManager.readValidatedPinNumber("SD卡 MOSI引脚", state.getSdCardMosiPin(), forbidden); // 汉化
        state.setSdCardMosiPin(mosi);
    }
    terminalView.println("USB 配置完成。"); // 汉化

    if (state.getTerminalMode() == TerminalTypeEnum::Standalone) {
        terminalView.println("");
        return;
    };

    terminalView.println("\n[警告] 如果使用USB串口终端模式，"); // 汉化
    terminalView.println("       执行USB命令可能会中断会话。"); // 汉化
    terminalView.println("       若连接丢失，请使用Web UI或重启设备。\n"); // 汉化
}

/*
Reset
*/
void UsbS3Controller::handleReset() {
    usbService.reset();
    terminalView.println("USB 重置: 禁用接口..."); // 汉化
}

/*
Help
*/
void UsbS3Controller::handleHelp() {
    terminalView.println("未知命令。"); // 汉化
    terminalView.println("使用方法:"); // 汉化
    terminalView.println("  stick");
    terminalView.println("  keyboard");
    terminalView.println("  keyboard <文本>"); // 汉化
    terminalView.println("  mouse <x> <y>");
    terminalView.println("  mouse click");
    terminalView.println("  mouse jiggle [毫秒]"); // 汉化
    terminalView.println("  gamepad <按键>, 例如: A, B, LEFT..."); // 汉化
    terminalView.println("  reset");
    terminalView.println("  config");
}

/*
Ensure Configuration
*/
void UsbS3Controller::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }
}

#endif