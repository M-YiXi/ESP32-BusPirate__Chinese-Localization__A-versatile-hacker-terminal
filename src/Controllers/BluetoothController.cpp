#include "Controllers/BluetoothController.h"

/*
Constructor
*/
BluetoothController::BluetoothController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    IInput& deviceInput,
    BluetoothService& bluetoothService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager
) : terminalView(terminalView),
    terminalInput(terminalInput),
    deviceInput(deviceInput),
    bluetoothService(bluetoothService),
    argTransformer(argTransformer),
    userInputManager(userInputManager) {}

/*
Entry point for BT command
*/
void BluetoothController::handleCommand(const TerminalCommand& cmd) {
    const auto& root = cmd.getRoot();

    if      (root == "scan")     handleScan();
    else if (root == "pair")     handlePair(cmd);
    else if (root == "spoof")    handleSpoof(cmd);
    else if (root == "sniff")    handleSniff(cmd);
    else if (root == "status")   handleStatus();
    else if (root == "server")   handleServer(cmd);
    else if (root == "keyboard") handleKeyboard(cmd);
    else if (root == "mouse")    handleMouse(cmd);
    else if (root == "reset")    handleReset();
    else handleHelp();
}

/*
Scan
*/
void BluetoothController::handleScan() {
    terminalView.println("蓝牙扫描：正在扫描 10秒内完成...\n"); // 汉化
    auto lines = bluetoothService.scanDevices(10);
    if (lines.empty()) {
        terminalView.println("蓝牙扫描：未发现设备"); // 汉化
        return;
    }

    for (const auto& line : lines) {
        terminalView.println("  " + line + "\n");
    }
}

/*
Pair
*/
void BluetoothController::handlePair(const TerminalCommand& cmd) {
    bluetoothService.switchToMode(BluetoothMode::CLIENT);
    std::string addr = cmd.getSubcommand();
    if (addr.empty()) {
        terminalView.println("使用方法：pair <MAC地址>"); // 汉化
        return;
    }

    terminalView.println("蓝牙配对：正在尝试连接 " + addr + "..."); // 汉化
    auto services = bluetoothService.connectTo(addr);
    if (!services.empty()) {
        terminalView.println("蓝牙配对：连接成功"); // 汉化
        terminalView.println("蓝牙配对：已发现服务"); // 汉化
        for (const auto& uuid : services) {
            terminalView.println("  - " + uuid);
        }
    } else {
        terminalView.println("蓝牙配对：连接 " + addr + " 失败"); // 汉化
    }
}

/*
Status
*/
void BluetoothController::handleStatus() {
    terminalView.println("蓝牙状态："); // 汉化
    if (bluetoothService.getMode() == BluetoothMode::NONE) {
        terminalView.println("  模式：未初始化"); // 汉化
        return;
    } else if (bluetoothService.getMode() == BluetoothMode::CLIENT) {
        terminalView.println("  模式：客户端"); // 汉化
    } else {
        terminalView.println("  模式：服务器"); // 汉化
    }

    terminalView.println("  已连接：" + std::string(bluetoothService.isConnected() ? "是" : "否")); // 汉化
    std::string mac = bluetoothService.getMacAddress();
    if (!mac.empty()) {
        terminalView.println("  MAC地址：" + mac); // 汉化
    } else {
        terminalView.println("  MAC地址：未知"); // 汉化
    }
}

/*
Sniff
*/
void BluetoothController::handleSniff(const TerminalCommand& cmd) {
    terminalView.println("蓝牙嗅探：已启动... 按下[ENTER键]停止\n"); // 汉化
    bluetoothService.switchToMode(BluetoothMode::CLIENT);
    BluetoothService::startPassiveBluetoothSniffing();

    unsigned long lastPull = 0;
    while (true) {
        // Enter press
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') break;

        // Show paquets if any
        if (millis() - lastPull > 200) { 
            auto logs = BluetoothService::getBluetoothSniffLog();
            for (const auto& line : logs) {
                terminalView.println(line);
            }
            lastPull = millis();
        }
        delay(10);
    }

    BluetoothService::stopPassiveBluetoothSniffing();
    terminalView.println("蓝牙嗅探：用户已停止\n"); // 汉化
}

/*
Server
*/
void BluetoothController::handleServer(const TerminalCommand& cmd) {
    if (bluetoothService.getMode() == BluetoothMode::SERVER && bluetoothService.isConnected()) {
        terminalView.println("蓝牙服务器：已启动"); // 汉化
        return;
    }

    std::string name = cmd.getSubcommand();
    if (name.empty()) {
        name = "Bus-Pirate-Bluetooth";
    }

    terminalView.println("蓝牙服务器：正在启动BLE HID服务器 名称为 \"" + name + "\"..."); // 汉化
    bluetoothService.startServer(name);
    terminalView.println("→ 现在可从手机或电脑进行配对"); // 汉化
}

/*
Keyboard
*/
void BluetoothController::handleKeyboard(const TerminalCommand& cmd) {
    if (bluetoothService.getMode() != BluetoothMode::SERVER) {
        terminalView.println("蓝牙键盘：发送数据前请先启动服务器"); // 汉化
        return;
    }

    auto sub = cmd.getSubcommand();
    if (sub.empty() || sub == "bridge") {
        handleKeyboardBridge();
        return;
    }

    // if spaces in text, cmd.getArgs() is not empty
    auto full = cmd.getArgs().empty() ? 
                        cmd.getSubcommand() : 
                        cmd.getSubcommand() + " " + cmd.getArgs();

    bluetoothService.sendKeyboardText(full);
    terminalView.println("蓝牙键盘：字符串已发送"); // 汉化
}

/*
Keyboard Bridge
*/
void BluetoothController::handleKeyboardBridge() {
    terminalView.println("蓝牙键盘桥接：所有按键将发送至BLE HID"); // 汉化
    bool sameHost = false;
    if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
        terminalView.println("\n[警告] 若BLE设备与终端连接同一主机"); // 汉化
        terminalView.println("          可能导致回车键循环问题"); // 汉化
        terminalView.println("          （同一主机桥接键盘无实际意义）\n"); // 汉化
    
        sameHost = userInputManager.readYesNo("是否连接同一主机？(y/n)", true);
        if (sameHost) {
            terminalView.println("同一主机 回车键将不发送至BLE HID"); // 汉化
        }
    }

    terminalView.println("蓝牙键盘：桥接已启动 按下[任意ESP32按键]停止"); // 汉化
    while (true) {
        // Stop if any esp32 button pressed
        char k = deviceInput.readChar();
        if (k != KEY_NONE) {
            terminalView.println("\r\n蓝牙键盘桥接：用户已停止"); // 汉化
            break;
        }

        // Relay terminal -> BLE HID
        char c = terminalInput.readChar();
        if (c != KEY_NONE) {
            if (c == '\n' && sameHost) continue;
            bluetoothService.sendKeyboardText(std::string(1, c));
            delay(20); 
        }
    }
}

/*
Mouse
*/
void BluetoothController::handleMouse(const TerminalCommand& cmd) {
    if (bluetoothService.getMode() != BluetoothMode::SERVER) {
        terminalView.println("蓝牙鼠标：发送数据前请先启动服务器"); // 汉化
        return;
    }

    // mouse click
    if (cmd.getSubcommand() == "click") {
        bluetoothService.clickMouse();
        terminalView.println("蓝牙鼠标：单击已发送"); // 汉化
        return;
    }

    // mouse jiggle
    if (cmd.getSubcommand() == "jiggle") {
        handleMouseJiggle(cmd);
        return;
    }

    auto args = argTransformer.splitArgs(cmd.getArgs());

    // mouse move x y
    if (args.size() == 2 && cmd.getSubcommand() == "move" &&
        argTransformer.isValidSignedNumber(args[0]) &&
        argTransformer.isValidSignedNumber(args[1])) {

        int8_t x = argTransformer.toClampedInt8(args[0]);
        int8_t y = argTransformer.toClampedInt8(args[1]);

        bluetoothService.mouseMove(x, y);
        terminalView.println("蓝牙鼠标：已移动 (" + std::to_string(x) + ", " + std::to_string(y) + ")"); // 汉化
        return;
    }
    
    // mouse x y
    if (args.size() != 1 ||
        !argTransformer.isValidSignedNumber(cmd.getSubcommand()) ||
        !argTransformer.isValidSignedNumber(args[0])) {
        terminalView.println("使用方法：mouse <x> <y> 或 mouse click"); // 汉化
        return;
    }

    int8_t x = argTransformer.toClampedInt8(cmd.getSubcommand());
    int8_t y = argTransformer.toClampedInt8(args[0]);

    bluetoothService.mouseMove(x, y);
    terminalView.println("蓝牙鼠标：已移动 (" + std::to_string(x) + ", " + std::to_string(y) + ")"); // 汉化
}

/*
Mouse Jiggle
*/
void BluetoothController::handleMouseJiggle(const TerminalCommand& cmd) {
    int intervalMs = 1000; // default

    // Optional interval arg
    const std::string& arg = cmd.getArgs();
    if (!arg.empty() && argTransformer.isValidNumber(arg)) {
        intervalMs = argTransformer.parseHexOrDec32(arg);
    }

    terminalView.println(
        "蓝牙鼠标：抖动已启动（" + std::to_string(intervalMs) +
        " 毫秒）... 按下[ENTER键]停止"
    ); // 汉化

    while (true) {
        // Random move
        int8_t dx = (int8_t)random(-127, 127);
        int8_t dy = (int8_t)random(-127, 127);
        if (dx == 0 && dy == 0) dx = 1;

        bluetoothService.mouseMove(dx, dy);
        delay(30);

        // Wait for interval while listening for ENTER
        unsigned long t0 = millis();
        while ((millis() - t0) < (unsigned long)intervalMs) {
            int c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("蓝牙鼠标：抖动已停止\n"); // 汉化
                return;
            }
            delay(10);
        }
    }
}

/*
Spoof
*/
void BluetoothController::handleSpoof(const TerminalCommand& cmd) {
    std::string mac = cmd.getSubcommand();
    if (bluetoothService.isConnected() || bluetoothService.getMode() != BluetoothMode::NONE) {
        terminalView.println("蓝牙伪装：初始化蓝牙前需设置地址 请使用'reset'命令"); // 汉化
        return;
    }

    if (mac.empty()) {
        terminalView.println("使用方法：spoof <MAC地址>"); // 汉化
        return;
    }

    bool success = bluetoothService.spoofMacAddress(mac);
    if (success) {
        terminalView.println("蓝牙伪装：MAC地址已覆盖为 " + mac); // 汉化
    } else {
        terminalView.println("蓝牙伪装：设置MAC地址失败"); // 汉化
    }
}

/*
Reset
*/
void BluetoothController::handleReset() {
    bluetoothService.stopServer();
    terminalView.println("蓝牙：重置完成"); // 汉化
}

/*
Config
*/
void BluetoothController::handleConfig() {
    #ifdef DEVICE_M5STICK
        bluetoothService.releaseBtClassic();
    #endif
}

/*
Help
*/
void BluetoothController::handleHelp() {
    terminalView.println("蓝牙命令："); // 汉化
    terminalView.println("  scan");
    terminalView.println("  pair <mac>");
    terminalView.println("  spoof <mac>");
    terminalView.println("  sniff");
    terminalView.println("  status");
    terminalView.println("  server");
    terminalView.println("  keyboard");
    terminalView.println("  keyboard <text>");
    terminalView.println("  mouse <x> <y>");
    terminalView.println("  mouse click");
    terminalView.println("  mouse jiggle [ms]");
    terminalView.println("  reset");
}

/*
Ensure Config
*/
void BluetoothController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }
}
