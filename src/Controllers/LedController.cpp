#include "LedController.h"
#include <sstream>
#include <algorithm>

/*
Constructor
*/
LedController::LedController(ITerminalView& terminalView, IInput& terminalInput,
                             LedService& ledService, ArgTransformer& argTransformer,
                             UserInputManager& userInputManager)
    : terminalView(terminalView), terminalInput(terminalInput),
      ledService(ledService), argTransformer(argTransformer), userInputManager(userInputManager) {}

/*
Command
*/
void LedController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "fill") {
        handleFill(cmd);
    } else if (cmd.getRoot() == "scan") {
        handleScan();
    } else if (cmd.getRoot() == "set") {
        handleSet(cmd);
    } else if (cmd.getRoot() == "reset") {
        handleReset(cmd);
    } else if (cmd.getRoot() == "blink") {
        handleAnimation(cmd);
    } else if (cmd.getRoot() == "rainbow") {
        handleAnimation(cmd);
    } else if (cmd.getRoot() == "chase") {
        handleAnimation(cmd);
    } else if (cmd.getRoot() == "cycle") {
        handleAnimation(cmd);
    } else if (cmd.getRoot() == "wave") {
        handleAnimation(cmd);
    } else if (cmd.getRoot() == "config") {
        handleConfig();
    } else if (cmd.getRoot() == "setprotocol") {
        handleSetProtocol();
    } else {
        handleHelp();
    }
}

/*
Instructions
*/
void LedController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    terminalView.println("[ERROR] LED指令未实现。"); // 汉化
}

/*
Scan
*/
void LedController::handleScan() {
    terminalView.println("\n  [INFO] LED协议扫描。"); // 汉化
    terminalView.println("         将会为每个协议播放一段简短的'chase'（追逐）动画。"); // 汉化
    terminalView.println("         观察LED灯：它们应逐个亮起蓝色，"); // 汉化
    terminalView.println("         然后依次熄灭。如果显示正常，请按下[ENTER]。"); // 汉化
    terminalView.println("         否则等待3秒后将尝试下一个协议。\n"); // 汉化

    terminalView.println("你要扫描哪种类型的LED？"); // 汉化
    terminalView.println("  1. 单线制（仅DATA引脚）"); // 汉化
    terminalView.println("  2. 带时钟（DATA + CLOCK引脚）\n"); // 汉化

    // Ask for LED type
    uint8_t typeChoice = 0;
    while (true) {
        typeChoice = userInputManager.readValidatedUint8("选择", 1); // 汉化
        if (typeChoice == 1 || typeChoice == 2) break;
        terminalView.println("无效选择。请输入1或2。"); // 汉化
    }

    // Define scanned protocol
    std::vector<std::string> protocols = (typeChoice == 1)
        ? LedService::getSingleWireProtocols()
        : LedService::getSpiChipsets();

    // Get saved pins and leds count
    uint8_t dataPin = state.getLedDataPin();
    uint8_t clockPin = state.getLedClockPin();
    uint16_t length = state.getLedLength();
    uint8_t brightness = state.getLedBrightness();
    
    // Run chase animation for each protocol until ENTER is pressed
    for (const auto& proto : protocols) {
        terminalView.println("正在尝试协议: " + proto); // 汉化
        ledService.configure(dataPin, clockPin, length, proto, brightness);
        ledService.resetLeds();

        terminalView.println(">>> 如果LED蓝色追逐动画显示正常，请按下[ENTER]（3秒后自动跳过）..."); // 汉化

        // Show the animation for 3 sec or until ENTER is press
        unsigned long start = millis();
        while (millis() - start < 3000) {
            char key = terminalInput.readChar();
            // Found, save the protocol
            if (key == '\r' || key == '\n') {
                terminalView.print("\nLED: 找到匹配协议: " + proto); // 汉化
                terminalView.println("。已成功保存到配置中。"); // 汉化
                state.setLedProtocol(proto);
                return;
            }
            ledService.runAnimation("chase");
        }
        ledService.resetLeds(); // in case of some anim persist in this mode

    }
    terminalView.println("\nLED: 未找到匹配的协议。"); // 汉化
    ensureConfigured();
}

/*
Fill
*/
void LedController::handleFill(const TerminalCommand& cmd) {
    std::vector<std::string> args = argTransformer.splitArgs(cmd.getArgs());

    // fill <r> <g> <b>
    if (!args.empty() &&
        argTransformer.isValidNumber(cmd.getSubcommand()) &&
        args.size() >= 2 &&
        argTransformer.isValidNumber(args[0]) &&
        argTransformer.isValidNumber(args[1])) {

        std::vector<std::string> full = {cmd.getSubcommand(), args[0], args[1]};
        CRGB rgb = parseFlexibleColor(full);
        ledService.fill(rgb);
        return;
    }

    // fill <hex|name>
    std::vector<std::string> colorArg = {cmd.getSubcommand()};
    CRGB rgb = parseFlexibleColor(colorArg);
    ledService.fill(rgb);
}

/*
Set
*/
void LedController::handleSet(const TerminalCommand& cmd) {
    auto args = argTransformer.splitArgs(cmd.getArgs());

    if (args.empty()) {
        terminalView.println("使用方法: set <序号> <十六进制RGB颜色 | 红 绿 蓝 | 颜色名称>"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("错误: 无效的序号格式。"); // 汉化
        return;
    }

    uint16_t index = argTransformer.parseHexOrDec(cmd.getSubcommand());
    CRGB rgb = parseFlexibleColor(args);

    ledService.set(index, rgb);
}

/*
Reset
*/
void LedController::handleReset(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty()) {
        ledService.resetLeds();
        terminalView.println("LED: 已将所有LED重置为默认状态。"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("LED: 语法错误。使用方法:"); // 汉化
        terminalView.println("  reset");
        terminalView.println("  reset <LED序号>"); // 汉化
        return;
    }

    uint16_t index = argTransformer.parseHexOrDec(cmd.getSubcommand());
    ledService.set(index, CRGB::Black);
    terminalView.println("LED: 已重置LED " + std::to_string(index)); // 汉化
}

/*
Config
*/
void LedController::handleConfig() {
    terminalView.println("LED配置:"); // 汉化
    
    // Get default
    const auto& forbidden = state.getProtectedPins();
    uint8_t defaultDataPin = state.getLedDataPin();
    uint8_t defaultClockPin = state.getLedClockPin();
    uint16_t defaultLength = state.getLedLength();

    // LEDs pins, locked because FastLED needs them at compile time
    terminalView.println("[WARNING] 数据引脚无法修改。当前数据引脚设置为: " + std::to_string(defaultDataPin)); // 汉化
    terminalView.println("[WARNING] 时钟引脚无法修改。当前时钟引脚设置为: " + std::to_string(defaultClockPin)); // 汉化

    // LEDs count
    uint16_t length = userInputManager.readValidatedUint32("LED数量", defaultLength); // 汉化
    if (length <= 0) length = 1;

    // LED Brightness
    uint8_t defaultBrightness = state.getLedBrightness();
    uint8_t brightness = userInputManager.readValidatedUint8("亮度 (0–255)", defaultBrightness); // 汉化

    // LED Protocol
    auto selectedProtocol = state.getLedProtocol();
    terminalView.println("当前协议: '" + selectedProtocol + "'"); // 汉化
    terminalView.println("可使用'setprotocol'命令修改协议"); // 汉化
    terminalView.println("或使用'scan'命令自动检测协议"); // 汉化

    // Configure
    ledService.configure(defaultDataPin, defaultClockPin, length, selectedProtocol, brightness);
    ledService.resetLeds();
    terminalView.println("LED配置完成。\n"); // 汉化

    // Update state
    state.setLedLength(length);
    state.setLedBrightness(brightness);
    state.setLedProtocol(selectedProtocol);
}

/*
Animation
*/
void LedController::handleAnimation(const TerminalCommand& cmd) {
    const auto& validTypes = ledService.getSupportedAnimations();

    // Check correct anim type
    const std::string& type = cmd.getRoot();
    if (std::find(validTypes.begin(), validTypes.end(), type) == validTypes.end()) {
        terminalView.println("LED: 未知的动画类型: " + type); // 汉化
        return;
    }
    
    // Run anim until user ENTER press
    terminalView.println("LED: 正在播放动画: " + type + "... 按下[ENTER]停止。"); // 汉化
    while (true) {
        char key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\nLED: 动画已停止。"); // 汉化
            break;
        }

        ledService.runAnimation(type);
    }
}

/*
Set Protocol
*/
void LedController::handleSetProtocol() {
    terminalView.println("\n设置LED协议:"); // 汉化

    std::vector<std::string> oneWire = LedService::getSingleWireProtocols();
    std::vector<std::string> spiChipsets = LedService::getSpiChipsets();
    std::vector<std::string> allProtocols;
    size_t index = 1;

    // Show single wire protocols
    terminalView.println("  -- 单线制协议（仅DATA引脚） --"); // 汉化
    for (const auto& proto : oneWire) {
        terminalView.println("  " + std::to_string(index++) + ". " + proto);
        allProtocols.push_back(proto);
    }

    // Show clocked
    terminalView.println("  -- 带时钟芯片组（DATA + CLOCK引脚） --"); // 汉化
    for (const auto& proto : spiChipsets) {
        terminalView.println("  " + std::to_string(index++) + ". " + proto);
        allProtocols.push_back(proto);
    }

    // Convert saved protocol to index
    std::string currentProtocol = state.getLedProtocol();
    auto it = std::find(allProtocols.begin(), allProtocols.end(), currentProtocol);
    size_t currentIndex = (it != allProtocols.end()) ? std::distance(allProtocols.begin(), it) + 1 : 1;


    // Ask for protocol index
    terminalView.println("");
    uint8_t choice = 0;
    while (true) {
        choice = userInputManager.readValidatedUint8("选择", currentIndex); // 汉化
        if (choice >= 1 && choice <= allProtocols.size()) break;
        terminalView.println("无效选择。请重试。"); // 汉化
    }

    // Configure
    std::string selectedProtocol = allProtocols[choice - 1];
    state.setLedProtocol(selectedProtocol);
    ensureConfigured();
    terminalView.println("LED协议已切换为 " + selectedProtocol); // 汉化
}

/*
Help
*/
void LedController::handleHelp() {
    terminalView.println("未知的LED命令。使用方法:"); // 汉化
    terminalView.println("  scan");
    terminalView.println("  fill blue");
    terminalView.println("  set 1 red");
    terminalView.println("  blink");
    terminalView.println("  rainbow");
    terminalView.println("  chase");
    terminalView.println("  cycle");
    terminalView.println("  wave");
    terminalView.println("  reset [LED序号]"); // 汉化
    terminalView.println("  setprotocol");
    terminalView.println("  config");
}

/*
Ensure Configuration
*/
void LedController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // Reconfigure
    std::string protocol = state.getLedProtocol();
    uint8_t data = state.getLedDataPin();
    uint8_t clock = state.getLedClockPin();
    uint16_t length = state.getLedLength();
    uint8_t brightness = state.getLedBrightness();
    ledService.configure(data, clock, length, protocol, brightness);
}

/*
Utils
*/
CRGB LedController::parseFlexibleColor(const std::vector<std::string>& args) {
    if (args.empty()) return CRGB::Black;

    // 3 nombres
    if (args.size() >= 3 &&
        argTransformer.isValidNumber(args[0]) &&
        argTransformer.isValidNumber(args[1]) &&
        argTransformer.isValidNumber(args[2])) {

        uint8_t r = argTransformer.toUint8(args[0]);
        uint8_t g = argTransformer.toUint8(args[1]);
        uint8_t b = argTransformer.toUint8(args[2]);
        return CRGB(r, g, b);
    }

    // #RRGGBB ou 0xRRGGBB
    const std::string& token = args[0];
    if (token.rfind("#", 0) == 0 || token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
        return ledService.parseHtmlColor(token);
    }

    // blue, red, etc.
    return ledService.parseStringColor(token);
}