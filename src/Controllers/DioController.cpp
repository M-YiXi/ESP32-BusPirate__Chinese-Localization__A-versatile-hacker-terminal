#include "Controllers/DioController.h"
#include <sstream>
#include <algorithm>

/*
Constructor
*/
DioController::DioController(ITerminalView& terminalView, IInput& terminalInput, PinService& pinService, ArgTransformer& argTransformer)
    : terminalView(terminalView), terminalInput(terminalInput), pinService(pinService), argTransformer(argTransformer) {}

/*
Entry point to handle a DIO command
*/
void DioController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "sniff") handleSniff(cmd);
    else if (cmd.getRoot() == "read")   handleReadPin(cmd); 
    else if (cmd.getRoot() == "set")    handleSetPin(cmd);
    else if (cmd.getRoot() == "pullup") handlePullup(cmd);
    else if (cmd.getRoot() == "pulldown") handlePulldown(cmd);
    else if (cmd.getRoot() == "pwm")    handlePwm(cmd);
    else if (cmd.getRoot() == "toggle") handleTogglePin(cmd);
    else if (cmd.getRoot() == "pulse")  handlePulse(cmd);
    else if (cmd.getRoot() == "measure") handleMeasure(cmd);
    else if (cmd.getRoot() == "servo")  handleServo(cmd);
    else if (cmd.getRoot() == "jam")    handleJamPin(cmd);
    else if (cmd.getRoot() == "reset")  handleResetPin(cmd);
    else                                handleHelp();
    
}

/*
Read
*/
void DioController::handleReadPin(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: read <引脚号>"); // 汉化
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Read")) return;
    int value = pinService.read(pin);
    terminalView.println("引脚 " + std::to_string(pin) + " = " + std::to_string(value) + (value ? " (高电平)" : " (低电平)")); // 汉化
}

/*
Set
*/
void DioController::handleSetPin(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: set <引脚号> <IN/OUT/HI/LOW>"); // 汉化
        return;
    }

    std::string arg = cmd.getArgs();
    if (arg.empty()) {
        terminalView.println("DIO设置: 缺少模式参数 (IN/OUT/HI/LOW)."); // 汉化
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Set")) return;
    char c = std::toupper(arg[0]);

    switch (c) {
        case 'I':
            pinService.setInput(pin);
            terminalView.println("DIO设置: 引脚 " + std::to_string(pin) + " 设为输入模式"); // 汉化
            break;
        case 'O':
            pinService.setOutput(pin);
            terminalView.println("DIO设置: 引脚 " + std::to_string(pin) + " 设为输出模式"); // 汉化
            break;
        case 'H':
        case '1':
            pinService.setOutput(pin);
            pinService.setHigh(pin);
            terminalView.println("DIO设置: 引脚 " + std::to_string(pin) + " 设为高电平"); // 汉化
            break;
        case 'L':
        case '0':
            pinService.setOutput(pin);
            pinService.setLow(pin);
            terminalView.println("DIO设置: 引脚 " + std::to_string(pin) + " 设为低电平"); // 汉化
            break;
        default:
            terminalView.println("未知命令. 使用 I, O, H (1), 或 L (0)."); // 汉化
            break;
    }
}

/*
Pullup
*/
void DioController::handlePullup(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: pullup <引脚号>"); // 汉化
        return;
    }

    int pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Pullup")) return;
    pinService.setInputPullup(pin);

    terminalView.println("DIO上拉: 已在引脚 " + std::to_string(pin) + " 启用"); // 汉化
}

/*
Pulldown
*/
void DioController::handlePulldown(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: pulldown <引脚号>"); // 汉化
        return;
    }

    int pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Pulldown")) return;
    pinService.setInputPullDown(pin);

    terminalView.println("DIO下拉: 已在引脚 " + std::to_string(pin) + " 启用"); // 汉化
}

/*
Sniff
*/
void DioController::handleSniff(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: sniff <引脚号>"); // 汉化
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Sniff")) return;
    PinService::pullType pull =  pinService.getPullType(pin);
    
    switch (pull)
    {
    case PinService::NOPULL:
        pinService.setInput(pin);
        break;
    case PinService::PULL_UP:
        pinService.setInputPullup(pin);
        break;
    case PinService::PULL_DOWN:
        pinService.setInputPullDown(pin);
        break;
    
    default:
        break;
    }
    

    terminalView.println("DIO嗅探: 监控引脚 " + std::to_string(pin) + "... 按下[ENTER]停止"); // 汉化
    
    int last = pinService.read(pin);
    terminalView.println("初始状态: " + std::to_string(last)); // 汉化

    unsigned long lastCheck = millis();
    while (true) {
        // check ENTER press
        if (millis() - lastCheck > 10) {
            lastCheck = millis();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("DIO嗅探: 已停止."); // 汉化
                break;
            }
        }

        // check pin state
        int current = pinService.read(pin);
        if (current != last) {
            std::string transition = (last == 0 && current == 1)
                ? "低电平  -> 高电平" // 汉化
                : "高电平 -> 低电平"; // 汉化
            terminalView.println("引脚 " + std::to_string(pin) + ": " + transition); // 汉化
            last = current;
        }
    }
}

/*
Pwm
*/
void DioController::handlePwm(const TerminalCommand& cmd) {
    auto sub = cmd.getSubcommand();
    auto args = argTransformer.splitArgs(cmd.getArgs());

    if (!sub.empty() && args.size() != 2) {
        terminalView.println("DIO PWM: 语法错误. 使用方法:"); // 汉化
        terminalView.println("  pwm <引脚号> <频率> <占空比>"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(sub) ||
        !argTransformer.isValidNumber(args[0]) ||
        !argTransformer.isValidNumber(args[1])) {
        terminalView.println("DIO PWM: 所有参数必须是有效数字."); // 汉化
        return;
    }

    uint8_t pin = argTransformer.toUint8(sub);
    if (!isPinAllowed(pin, "PWM")) return;

    uint32_t freq = argTransformer.toUint32(args[0]);
    uint8_t duty = argTransformer.toUint8(args[1]);

    if (duty > 100) {
        terminalView.println("DIO PWM: 占空比必须在0到100之间."); // 汉化
        return;
    }

    bool ok = pinService.setupPwm(pin, freq, duty);
    if (!ok) {
        terminalView.println("DIO PWM: 无法生成 " + std::to_string(freq) +
                            " Hz的信号. 尝试更高频率或使用toggle命令."); // 汉化
        return;
    }

    terminalView.println("DIO PWM: 引脚 " + std::to_string(pin) +
                         " (" + std::to_string(freq) + "Hz, " +
                         std::to_string(duty) + "% 占空比)."); // 汉化
}

/*
Measure edges
*/
void DioController::handleMeasure(const TerminalCommand& cmd) {
    auto args = argTransformer.splitArgs(cmd.getArgs());

    if (cmd.getSubcommand().empty()) {
        terminalView.println("使用方法: edgecount <引脚号> [持续时间_ms]"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("DIO测量: 无效的引脚号."); // 汉化
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Measure")) return;

    uint32_t durationMs = 1000;
    if (!args.empty() && argTransformer.isValidNumber(args[0])) {
        durationMs = std::min(argTransformer.toUint32(args[0]), 5000u);
        if (durationMs == 5000) {
            terminalView.println("注意: 持续时间限制为最大5000毫秒."); // 汉化
        }
    }

    terminalView.println("DIO边沿计数: 采样引脚 " + std::to_string(pin) +
                         " 持续 " + std::to_string(durationMs) + " 毫秒..."); // 汉化

    PinService::pullType pull =  pinService.getPullType(pin);
    
    switch (pull)
    {
    case PinService::NOPULL:
        pinService.setInput(pin);
        break;
    case PinService::PULL_UP:
        pinService.setInputPullup(pin);
        break;
    case PinService::PULL_DOWN:
        pinService.setInputPullDown(pin);
        break;
    
    default:
        break;
    }
    
    int last = pinService.read(pin);
    uint32_t rising = 0, falling = 0;

    unsigned long startMs = millis();

    while (millis() - startMs < durationMs) {
        int current = pinService.read(pin);
        if (current != last) {
            if (last == 0 && current == 1) ++rising;
            else if (last == 1 && current == 0) ++falling;
            last = current;
        }
    }

    terminalView.println("");
    terminalView.println(" 结果:"); // 汉化
    terminalView.println("  • 上升沿数量:     " + std::to_string(rising)); // 汉化
    terminalView.println("  • 下降沿数量:     " + std::to_string(falling)); // 汉化

    uint32_t totalEdges = rising + falling;
    float freqHz = (totalEdges / 2.0f) / (durationMs / 1000.0f);

    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed << "  • 近似频率: " << freqHz << " Hz\n"; // 汉化
    terminalView.println(oss.str());
}

/*
Toggle
*/
void DioController::handleTogglePin(const TerminalCommand& cmd) {
    
    auto args = argTransformer.splitArgs(cmd.getArgs());
    
    if (cmd.getSubcommand().empty() || args.empty()) {
        terminalView.println("使用方法: toggle <引脚号> <毫秒>"); // 汉化
        return;
    }
    
    if (!argTransformer.isValidNumber(cmd.getSubcommand()) ||
    !argTransformer.isValidNumber(args[0])) {
        terminalView.println("DIO翻转: 无效的参数."); // 汉化
        return;
    }
    
    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Toggle")) return;

    uint32_t intervalMs = argTransformer.toUint32(args[0]);
    
    pinService.setOutput(pin);
    bool state = false;

    terminalView.println("\nDIO翻转: 引脚 " + std::to_string(pin) + " 每 " + std::to_string(intervalMs) + "毫秒翻转一次...按下[ENTER]停止."); // 汉化
    terminalView.println("");

    unsigned long lastToggle = millis();
    unsigned long lastCheck = millis();

    while (true) {
        unsigned long now = millis();

        // check ENTER press every 10ms
        if (now - lastCheck > 10) {
            lastCheck = now;
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("DIO翻转: 已停止."); // 汉化
                break;
            }
        }

        // toggle pin at defined interval
        if (now - lastToggle >= intervalMs) {
            lastToggle = now;
            state = !state;
            if (state) {
                pinService.setHigh(pin);
            } else {
                pinService.setLow(pin);
            }
        }
    }
}

/*
Reset
*/
void DioController::handleResetPin(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty()) {
        terminalView.println("使用方法: reset <引脚号>"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("DIO重置: 无效的引脚号."); // 汉化
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Reset")) return;

    // Detacher PWM
    ledcDetachPin(pin);

    // Reset Pullup
    pinService.setInput(pin);

    terminalView.println("DIO重置: 引脚 " + std::to_string(pin) + " 恢复为输入模式 (无上拉, 无PWM)."); // 汉化
}

void DioController::handleServo(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || cmd.getArgs().empty()) {
        terminalView.println("使用方法: servo <引脚号> <角度>"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand()) ||
        !argTransformer.isValidNumber(cmd.getArgs())) {
        terminalView.println("DIO舵机: 无效的参数."); // 汉化
        return;
    }

    uint8_t pin = argTransformer.parseHexOrDec(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Servo")) return;

    uint8_t angle = argTransformer.parseHexOrDec(cmd.getArgs());
    pinService.setServoAngle(pin, angle);
    terminalView.println("DIO舵机: 引脚 " + std::to_string(pin) + " 设为角度 " + std::to_string(angle) + "度."); // 汉化
}

/*
Pulse
*/
void DioController::handlePulse(const TerminalCommand& cmd) {
    if (cmd.getSubcommand().empty() || cmd.getArgs().empty()) {
        terminalView.println("使用方法: pulse <引脚号> <持续时间_us>"); // 汉化
        return;
    }

    if (!argTransformer.isValidNumber(cmd.getSubcommand()) ||
        !argTransformer.isValidNumber(cmd.getArgs())) {
        terminalView.println("DIO脉冲: 无效的参数."); // 汉化
        return;
    }

    uint8_t pin = argTransformer.parseHexOrDec(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Pulse")) return;

    uint32_t durationUs = argTransformer.toUint32(cmd.getArgs());

    // Configure en sortie
    pinService.setOutput(pin);

    pinService.setHigh(pin);
    delayMicroseconds(durationUs);
    pinService.setLow(pin);

    terminalView.println("DIO脉冲: 引脚 " + std::to_string(pin) +
                         " 高电平持续 " + std::to_string(durationUs) + " 微秒."); // 汉化
}

/*
Jam
*/
void DioController::handleJamPin(const TerminalCommand& cmd) {

    auto args = argTransformer.splitArgs(cmd.getArgs());
    uint32_t minUs = 5;
    uint32_t maxUs = 100;

    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: jam <引脚号> [最小_us] [最大_us]"); // 汉化
        return;
    }

    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (!isPinAllowed(pin, "Jam")) return;


    if (args.size() >= 1 && argTransformer.isValidNumber(args[0])) {
        minUs = argTransformer.toUint32(args[0]);
    }
    if (args.size() >= 2 && argTransformer.isValidNumber(args[1])) {
        maxUs = argTransformer.toUint32(args[1]);
    }

    if (minUs < 1) minUs = 1;
    if (maxUs < minUs) maxUs = minUs;

    pinService.setOutput(pin);

    terminalView.println("DIO随机翻转: 引脚 " + std::to_string(pin) +
                         " 随机翻转 [" + std::to_string(minUs) + ".." + std::to_string(maxUs) +
                         "] 微秒... 按下[ENTER]停止."); // 汉化
    terminalView.println("");

    bool state = false;
    unsigned long lastCheck = millis();

    while (true) {
        // check ENTER press every 10ms
        if (millis() - lastCheck > 10) {
            lastCheck = millis();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("DIO随机翻转: 已被用户停止."); // 汉化
                break;
            }
        }

        state = !state;
        if (state) pinService.setHigh(pin);
        else       pinService.setLow(pin);

        // random delay between minUs and maxUs
        uint32_t span = (maxUs - minUs);
        uint32_t waitUs = minUs + (span ? (esp_random() % (span + 1)) : 0);
        delayMicroseconds(waitUs);
    }
}

/*
Help
*/
void DioController::handleHelp() {
    terminalView.println("未知的DIO命令. 使用方法:"); // 汉化
    terminalView.println("  sniff <引脚号>"); // 汉化
    terminalView.println("  read <引脚号>"); // 汉化
    terminalView.println("  set <引脚号> <H/L/I/O>"); // 汉化
    terminalView.println("  pullup <引脚号>"); // 汉化
    terminalView.println("  pulldown <引脚号>"); // 汉化
    terminalView.println("  pwm <引脚号> <频率> <占空比>"); // 汉化
    terminalView.println("  servo <引脚号> <角度>"); // 汉化
    terminalView.println("  measure <引脚号> [毫秒]"); // 汉化
    terminalView.println("  pulse <引脚号> <微秒>"); // 汉化
    terminalView.println("  toggle <引脚号> <毫秒>"); // 汉化
    terminalView.println("  jam <引脚号> [最小_us] [最大_us]"); // 汉化
    terminalView.println("  reset <引脚号>"); // 汉化
}

bool DioController::isPinAllowed(uint8_t pin, const std::string& context) {
    const auto& protectedPins = state.getProtectedPins();
    
    if (std::find(protectedPins.begin(), protectedPins.end(), pin) != protectedPins.end()) {
        terminalView.println("DIO " + context + ": 引脚 " + std::to_string(pin) + " 受保护 无法使用."); // 汉化
        return false;
    }

    if (pin > 48 /* max pin for S3 */) {
        terminalView.println("DIO " + context + ": 引脚 " + std::to_string(pin) + " 超出范围 (0-48)."); // 汉化
        return false;
    }

    if (pin < 0) {
        terminalView.println("DIO " + context + ": 引脚 " + std::to_string(pin) + " 无效."); // 汉化
        return false;
    }

    return true;
}

std::vector<std::string> DioController::buildPullConfigLines() {
    std::vector<std::string> lines;

    auto pins = pinService.getConfiguredPullPins();
    std::sort(pins.begin(), pins.end());

    size_t total = 0;

    for (uint8_t pin : pins) {
        auto pull = pinService.getPullType(pin);

        if (pull == PinService::PULL_UP) {
            lines.push_back("GPIO " + std::to_string(pin) + " 上拉"); // 汉化
        } 
        else if (pull == PinService::PULL_DOWN) {
            lines.push_back("GPIO " + std::to_string(pin) + " 下拉"); // 汉化
        } else {
            continue;
        }

        ++total;

        // Limit to 4 displayable lines
        if (lines.size() == 4) {
            if (pins.size() > total) {
                lines.back() += " ...";
            }
            break;
        }
    }

    return lines;
}