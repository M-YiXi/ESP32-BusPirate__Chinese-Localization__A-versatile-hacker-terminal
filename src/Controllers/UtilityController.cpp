#include "Controllers/UtilityController.h"

/*
Constructor
*/
UtilityController::UtilityController(
    ITerminalView& terminalView,
    IDeviceView& deviceView,
    IInput& terminalInput,
    PinService& pinService,
    UserInputManager& userInputManager,
    ArgTransformer& argTransformer,
    SysInfoShell& sysInfoShell,
    GuideShell& guideShell
)
    : terminalView(terminalView),
      deviceView(deviceView),
      terminalInput(terminalInput),
      pinService(pinService),
      userInputManager(userInputManager),
      argTransformer(argTransformer),
      sysInfoShell(sysInfoShell),
      guideShell(guideShell)
{}

/*
Entry point for command
*/
void UtilityController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "help" || cmd.getRoot() == "h" || cmd.getRoot() == "?") handleHelp();
    else if (cmd.getRoot() == "P")                                               handleEnablePullups();
    else if (cmd.getRoot() == "p")                                               handleDisablePullups();
    else if (cmd.getRoot() == "logic")                                           handleLogicAnalyzer(cmd);
    else if (cmd.getRoot() == "analogic")                                        handleAnalogic(cmd);
    else if (cmd.getRoot() == "system")                                          handleSystem();
    else if (cmd.getRoot() == "guide")                                           handleGuide();
    else if (cmd.getRoot() == "man")                                             handleGuide();
    else {
        terminalView.println("未知命令。请尝试 'help'。"); // 汉化
    }
}

/*
Mode Change
*/
ModeEnum UtilityController::handleModeChangeCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() != "mode" && cmd.getRoot() != "m") {
        terminalView.println("无效的模式切换命令。"); // 汉化
        return ModeEnum::None;
    }

    if (!cmd.getSubcommand().empty()) {
        ModeEnum newMode = ModeEnumMapper::fromString(cmd.getSubcommand());
        if (newMode != ModeEnum::None) {
            terminalView.println("模式已切换为 " + ModeEnumMapper::toString(newMode)); // 汉化
            terminalView.println(""); 
            return newMode;
        } else {
            terminalView.println("未知模式：" + cmd.getSubcommand()); // 汉化
            return ModeEnum::None;
        }
    }

    return handleModeSelect();
}

/*
Mode Select
*/
ModeEnum UtilityController::handleModeSelect() {
    terminalView.println("");
    terminalView.println("选择模式："); // 汉化
    std::vector<ModeEnum> modes;

    for (int i = 0; i < static_cast<int>(ModeEnum::COUNT); ++i) {
        ModeEnum mode = static_cast<ModeEnum>(i);
        std::string name = ModeEnumMapper::toString(mode);
        if (!name.empty()) {
            modes.push_back(mode);
            terminalView.println("  " + std::to_string(modes.size()) + ". " + name);
        }
    }

    terminalView.println("");
    terminalView.print("模式编号 > "); // 汉化
    auto modeNumber = userInputManager.readModeNumber();

    if (modeNumber == -1) {
        terminalView.println("");
        terminalView.println("");
        terminalView.println("输入无效。"); // 汉化
        return ModeEnum::None;
    } else if (modeNumber >= 1 && modeNumber <= modes.size()) {
        ModeEnum selected = modes[modeNumber - 1];
        if (static_cast<int>(selected) > 9) {
            terminalView.println(""); // Hack to render correctly on web terminal
        }
        terminalView.println("");
        terminalView.println("模式已切换为 " + ModeEnumMapper::toString(selected)); // 汉化
        terminalView.println("");
        return selected;
    } else {
        terminalView.println("");
        terminalView.println("无效的模式编号。"); // 汉化
        terminalView.println("");
        return ModeEnum::None;
    }
}

/*
Pullup: p
*/
void UtilityController::handleDisablePullups() {
    auto mode = state.getCurrentMode();
    switch (mode) {
        case ModeEnum::SPI:
            pinService.setInput(state.getSpiMISOPin());
            terminalView.println("SPI：已禁用MISO引脚上拉电阻"); // 汉化
            break;

        case ModeEnum::I2C:
            pinService.setInput(state.getI2cSdaPin());
            pinService.setInput(state.getI2cSclPin());
            terminalView.println("I2C：已禁用SDA、SCL引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::OneWire:
            pinService.setInput(state.getOneWirePin());
            terminalView.println("1-Wire：已禁用DQ引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::UART:
            pinService.setInput(state.getUartRxPin());
            terminalView.println("UART：已禁用RX引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::HDUART:
            pinService.setInput(state.getHdUartPin());
            terminalView.println("HDUART：已禁用IO引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::TwoWire:
            pinService.setInput(state.getTwoWireIoPin());
            terminalView.println("2-WIRE：已禁用DATA引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::JTAG:
            for (auto pin : state.getJtagScanPins()) {
                pinService.setInput(pin);
            }
            terminalView.println("JTAG：已禁用所有扫描引脚上拉电阻。"); // 汉化
            break;

        default:
            terminalView.println("该模式下不适用上拉电阻配置。"); // 汉化
            break;
    }
}

/*
Pullup P
*/
void UtilityController::handleEnablePullups() {
    auto mode = state.getCurrentMode();
    switch (mode) {
        case ModeEnum::SPI:
            pinService.setInput(state.getSpiMISOPin());
            pinService.setInputPullup(state.getSpiMISOPin());
            terminalView.println("SPI：已启用MISO引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::I2C:
            pinService.setInputPullup(state.getI2cSdaPin());
            pinService.setInputPullup(state.getI2cSclPin());
            terminalView.println("I2C：已启用SDA、SCL引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::OneWire:
            pinService.setInputPullup(state.getOneWirePin());
            terminalView.println("1-Wire：已启用DQ引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::UART:
            pinService.setInputPullup(state.getUartRxPin());
            terminalView.println("UART：已启用RX引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::HDUART:
            pinService.setInputPullup(state.getHdUartPin());
            terminalView.println("HDUART：已启用IO引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::TwoWire:
            pinService.setInputPullup(state.getTwoWireIoPin());
            terminalView.println("2-WIRE：已启用DATA引脚上拉电阻。"); // 汉化
            break;

        case ModeEnum::JTAG:
            for (auto pin : state.getJtagScanPins()) {
                pinService.setInputPullup(pin);
            }
            terminalView.println("JTAG：已启用所有扫描引脚上拉电阻。"); // 汉化
            break;

        default:
            terminalView.println("该模式下不适用上拉电阻配置。"); // 汉化
            break;
    }
}

/*
Logic
*/
void UtilityController::handleLogicAnalyzer(const TerminalCommand& cmd) {

    uint16_t tDelay = 500;
    uint16_t inc = 100; // tDelay will be decremented/incremented by inc
    uint8_t step = 1; // step of the trace display kind of a zoom

    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: logic <引脚编号>"); // 汉化
        return;
    }

    // Verify protected pin
    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (state.isPinProtected(pin)) {
        terminalView.println("逻辑分析仪：该引脚受保护或已被保留。"); // 汉化
        return;
    }
    terminalView.println("\n逻辑分析仪：正在监控引脚 " + std::to_string(pin) + "... 按下[ENTER]停止。"); // 汉化
    terminalView.println("正在ESP32屏幕上显示波形...\n"); // 汉化

    pinService.setInput(pin);
    std::vector<uint8_t> buffer;
    buffer.reserve(320); // 320 samples

    unsigned long lastCheck = millis();
    deviceView.clear();
    deviceView.topBar("Logic Analyzer", false, false);

    while (true) {
        // Enter press
        if (millis() - lastCheck > 10) {
            lastCheck = millis();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                // fdufnews 2025/10/24 added to restore cursor position when leaving
                if (state.getTerminalMode() == TerminalTypeEnum::Serial)
                    terminalView.print("\n\n\n\n\r"); // 4 lines down to place cursor just under the logic trace
                terminalView.println("逻辑分析仪：已被用户停止。"); // 汉化
                break;
            }
            if (c == 's'){
                if (tDelay > inc){
                    tDelay -= inc;
                    terminalView.println("延迟 : " + std::to_string(tDelay) + "\n"); // 汉化
                }
            };
            if (c == 'S'){
                if (tDelay < 10000){
                    tDelay += inc;
                    terminalView.println("延迟 : " + std::to_string(tDelay) + "\n"); // 汉化
                }
            };
            if (c == 'z'){
                if (step > 1){
                    step--;
                    terminalView.println("步长 : " + std::to_string(step) + "\n"); // 汉化
                }
            };
            if (c == 'Z'){
                if (step < 4){
                    step++;
                    terminalView.println("步长 : " + std::to_string(step) + "\n"); // 汉化
                }
            };
        }

        // Draw
        if (buffer.size() >= 320) {
            buffer.resize(320);
            deviceView.drawLogicTrace(pin, buffer, step);

            // The poor man's drawLogicTrace() on terminal
            // draws a 132 samples sub part of the buffer to speed up the things
            if (state.getTerminalMode() == TerminalTypeEnum::Serial){
                terminalView.println("");
                uint8_t pos = 0;
                for(size_t i = 0; i < 132; i++, pos++){
                     terminalView.print(buffer[i]?"-":"_");
                }
                terminalView.print("\r\x1b[A");  // Up 1 line to put cursor at the correct place for the next draw
            }
            buffer.clear();
        }

        buffer.push_back(pinService.read(pin));
        delayMicroseconds(tDelay);
    }
}

/*
Analogic
*/
void UtilityController::handleAnalogic(const TerminalCommand& cmd) {
    uint16_t tDelay = 500;
    uint16_t inc = 100; // tDelay will be decremented/incremented by inc
    uint8_t step = 1; // step of the trace display kind of a zoom

    if (cmd.getSubcommand().empty() || !argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: analogic <引脚编号>"); // 汉化
        return;
    }

    // Verify protected pin
    uint8_t pin = argTransformer.toUint8(cmd.getSubcommand());
    if (state.isPinProtected(pin)) {
        terminalView.println("模拟信号：该引脚受保护或已被保留。"); // 汉化
        return;
    }
    if (!state.isPinAnalog(pin)){
        terminalView.println("模拟信号：该引脚非模拟引脚"); // 汉化
        return;
    };

    terminalView.println("\n模拟信号：正在监控引脚 " + std::to_string(pin) + "... 按下[ENTER]停止。"); // 汉化
    terminalView.println("正在ESP32屏幕上显示波形...\n"); // 汉化

    pinService.setInput(pin);
    std::vector<uint8_t> buffer;
    buffer.reserve(320); // 320 samples

    unsigned long lastCheck = millis();
    deviceView.clear();
    deviceView.topBar("Analog plotter", false, false);
    int count = 0;
    while (true) {
        // Enter press
        if (millis() - lastCheck > 10) {
            lastCheck = millis();
            char c = terminalInput.readChar();
            if (c == '\r' || c == '\n') {
                terminalView.println("\n模拟信号：已被用户停止。"); // 汉化
                break;
            }
            if (c == 's'){
                if (tDelay > inc){
                    tDelay -= inc;
                    terminalView.println("\n延迟 : " + std::to_string(tDelay) + "\n"); // 汉化
                }
            };
            if (c == 'S'){
                if (tDelay < 10000){
                    tDelay += inc;
                    terminalView.println("\n延迟 : " + std::to_string(tDelay) + "\n"); // 汉化
                }
            };
            if (c == 'z'){
                if (step > 1){
                    step--;
                    terminalView.println("\n步长 : " + std::to_string(step) + "\n"); // 汉化
                }
            };
            if (c == 'Z'){
                if (step < 4){
                    step++;
                    terminalView.println("\n步长 : " + std::to_string(step) + "\n"); // 汉化
                }
            };
            count++;
            if ((count > 50) && (state.getTerminalMode() != TerminalTypeEnum::Standalone)){
                int raw = pinService.readAnalog(pin);
                float voltage = (raw / 4095.0f) * 3.3f;

                std::ostringstream oss;
                oss << "   模拟引脚 " << static_cast<int>(pin)
                    << ": " << raw
                    << " (" << voltage << " 伏)"; // 汉化
                terminalView.println(oss.str());
                count = 0;
            }
        }

        // Draw
        if (buffer.size() >= 320) {
            buffer.resize(320);
            deviceView.drawAnalogicTrace(pin, buffer, step);
            buffer.clear();
        }

        buffer.push_back(pinService.readAnalog(pin) >> 4); // convert the readAnalog() value to a uint8_t (4096 ==> 256)
        delayMicroseconds(tDelay);
    }
}

/*
System Information
*/
void UtilityController::handleSystem() {
    sysInfoShell.run();
}

/*
Firmware Guide (man)
*/
void UtilityController::handleGuide() {
    guideShell.run();
}

/*
Help
*/
void UtilityController::handleHelp() {
    terminalView.println("");
    terminalView.println("   +=== 帮助：可用命令 ===+"); // 汉化
    terminalView.println("");

    terminalView.println(" 通用命令："); // 汉化
    terminalView.println("  help                 - 显示此帮助信息"); // 汉化
    terminalView.println("  man                  - 显示固件使用指南"); // 汉化
    terminalView.println("  system               - 显示系统信息"); // 汉化
    terminalView.println("  mode <name>          - 设置当前工作模式"); // 汉化
    terminalView.println("  logic <pin>          - 逻辑分析仪"); // 汉化
    terminalView.println("  analogic <pin>       - 模拟信号绘图仪"); // 汉化
    terminalView.println("  P                    - 启用上拉电阻"); // 汉化
    terminalView.println("  p                    - 禁用上拉电阻"); // 汉化

    terminalView.println("");
    terminalView.println(" 1. 高阻态（HiZ）："); // 汉化
    terminalView.println("  (默认模式)           - 所有线路禁用"); // 汉化

    terminalView.println("");
    terminalView.println(" 2. 1WIRE：");
    terminalView.println("  scan                 - 扫描1-Wire设备"); // 汉化
    terminalView.println("  ping                 - 探测1-Wire设备"); // 汉化
    terminalView.println("  sniff                - 监控1-Wire通信流量"); // 汉化
    terminalView.println("  read                 - 读取ID + 暂存器（SP）"); // 汉化
    terminalView.println("  write id [8 bytes]   - 写入设备ID"); // 汉化
    terminalView.println("  write sp [8 bytes]   - 写入暂存器"); // 汉化
    terminalView.println("  temp                 - 读取温度"); // 汉化
    terminalView.println("  ibutton              - iButton操作"); // 汉化
    terminalView.println("  eeprom               - EEPROM操作"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化
    terminalView.println("  [0xAA r:8] ...       - 指令语法格式"); // 汉化

    terminalView.println("");
    terminalView.println(" 3. UART：");
    terminalView.println("  scan                 - 自动检测波特率"); // 汉化
    terminalView.println("  ping                 - 发送数据并等待响应"); // 汉化
    terminalView.println("  read                 - 按当前波特率读取数据"); // 汉化
    terminalView.println("  write <text>         - 按当前波特率发送数据"); // 汉化
    terminalView.println("  bridge               - 全双工桥接模式"); // 汉化
    terminalView.println("  at                   - AT指令操作"); // 汉化
    terminalView.println("  spam <text> <ms>     - 每隔指定毫秒发送文本"); // 汉化
    terminalView.println("  glitch               - 时序攻击"); // 汉化
    terminalView.println("  xmodem <send> <path> - 通过XMODEM发送文件"); // 汉化
    terminalView.println("  xmodem <recv> <path> - 通过XMODEM接收文件"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化
    terminalView.println("  swap                 - 交换RX和TX引脚"); // 汉化
    terminalView.println("  ['Hello'] [r:64]...  - 指令语法格式"); // 汉化
 
    terminalView.println("");
    terminalView.println(" 4. HDUART：");
    terminalView.println("  bridge               - 半双工IO模式"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化
    terminalView.println("  [0x1 D:10 r:255]     - 指令语法格式"); // 汉化

    terminalView.println("");
    terminalView.println(" 5. I2C：");
    terminalView.println("  scan                 - 查找设备"); // 汉化
    terminalView.println("  ping <addr>          - 检查ACK响应"); // 汉化
    terminalView.println("  identify <addr>      - 识别设备"); // 汉化
    terminalView.println("  sniff                - 监控通信流量"); // 汉化
    terminalView.println("  slave <addr>         - 模拟I2C从设备"); // 汉化
    terminalView.println("  read <addr> <reg>    - 读取寄存器"); // 汉化
    terminalView.println("  write <a> <r> <val>  - 写入寄存器"); // 汉化
    terminalView.println("  dump <addr> [len]    - 读取所有寄存器"); // 汉化
    terminalView.println("  glitch <addr>        - 运行攻击序列"); // 汉化
    terminalView.println("  flood <addr>         - 饱和目标IO"); // 汉化
    terminalView.println("  jam                  - 向I2C总线发送干扰噪声"); // 汉化
    terminalView.println("  monitor <addr> [ms]  - 监控寄存器变化"); // 汉化
    terminalView.println("  eeprom [addr]        - I2C EEPROM操作"); // 汉化
    terminalView.println("  recover              - 尝试总线恢复"); // 汉化
    terminalView.println("  swap                 - 交换SDA和SCL引脚"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化
    terminalView.println("  [0x13 0x4B 0x1]      - 指令语法格式"); // 汉化

    terminalView.println("");
    terminalView.println(" 6. SPI：");
    terminalView.println("  sniff                - 监控通信流量"); // 汉化
    terminalView.println("  sdcard               - SD卡操作"); // 汉化
    terminalView.println("  slave                - 模拟SPI从设备"); // 汉化
    terminalView.println("  flash                - SPI Flash操作"); // 汉化
    terminalView.println("  eeprom               - SPI EEPROM操作"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化
    terminalView.println("  [0x9F r:3]           - 指令语法格式"); // 汉化

    terminalView.println("");
    terminalView.println(" 7. 2WIRE：");
    terminalView.println("  sniff                - 监控2WIRE通信流量"); // 汉化
    terminalView.println("  smartcard            - 智能卡操作"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化
    terminalView.println("  [0xAB r:4]           - 指令语法格式"); // 汉化

    terminalView.println("");
    terminalView.println(" 8. 3WIRE：");
    terminalView.println("  eeprom               - 3WIRE EEPROM操作"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 9. DIO：");
    terminalView.println("  sniff <pin>          - 跟踪引脚电平切换状态"); // 汉化
    terminalView.println("  read <pin>           - 获取引脚状态"); // 汉化
    terminalView.println("  set <pin> <H/L/I/O>  - 设置引脚状态"); // 汉化
    terminalView.println("  pullup <pin>         - 设置引脚上拉"); // 汉化
    terminalView.println("  pulldown <pin>       - 设置引脚下拉"); // 汉化
    terminalView.println("  pulse <pin> <us>     - 向引脚发送脉冲"); // 汉化
    terminalView.println("  servo <pin> <angle>  - 设置舵机角度"); // 汉化
    terminalView.println("  pwm <pin freq duty%> - 向引脚设置PWM"); // 汉化
    terminalView.println("  toggle <pin> <ms>    - 周期性切换引脚电平"); // 汉化
    terminalView.println("  measure <pin> [ms]   - 计算引脚信号频率"); // 汉化
    terminalView.println("  jam <pin> [min max]  - 随机高低电平干扰"); // 汉化
    terminalView.println("  reset <pin>          - 恢复默认设置"); // 汉化

    terminalView.println("");
    terminalView.println(" 10. LED：");
    terminalView.println("  scan                 - 尝试检测LED类型"); // 汉化
    terminalView.println("  fill <color>         - 填充所有LED为指定颜色"); // 汉化
    terminalView.println("  set <index> <color>  - 设置指定LED颜色"); // 汉化
    terminalView.println("  blink                - 所有LED闪烁"); // 汉化
    terminalView.println("  rainbow              - 彩虹动画效果"); // 汉化
    terminalView.println("  chase                - 追逐灯光效果"); // 汉化
    terminalView.println("  cycle                - 循环切换颜色"); // 汉化
    terminalView.println("  wave                 - 波浪动画效果"); // 汉化
    terminalView.println("  reset                - 关闭所有LED"); // 汉化
    terminalView.println("  setprotocol          - 选择LED通信协议"); // 汉化
    terminalView.println("  config               - 配置LED参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 11. 红外（INFRARED）："); // 汉化
    terminalView.println("  send <dev> sub <cmd> - 发送红外信号"); // 汉化
    terminalView.println("  receive              - 接收红外信号"); // 汉化
    terminalView.println("  setprotocol          - 设置红外通信协议"); // 汉化
    terminalView.println("  devicebgone          - 设备关机信号群发"); // 汉化
    terminalView.println("  remote               - 万能遥控器命令"); // 汉化
    terminalView.println("  replay [count]       - 重放录制的红外帧"); // 汉化
    terminalView.println("  record               - 将红外信号录制到文件"); // 汉化
    terminalView.println("  load                 - 从文件系统加载.ir文件"); // 汉化
    terminalView.println("  jam                  - 发送随机红外信号"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 12. USB：");
    terminalView.println("  stick                - 将SD卡挂载为USB存储"); // 汉化
    terminalView.println("  keyboard             - 启动键盘桥接"); // 汉化
    terminalView.println("  mouse <x> <y>        - 移动鼠标光标"); // 汉化
    terminalView.println("  mouse click          - 鼠标左键单击"); // 汉化
    terminalView.println("  mouse jiggle [ms]    - 鼠标随机移动"); // 汉化
    terminalView.println("  gamepad <key>        - 按下游戏手柄按键"); // 汉化
    terminalView.println("  reset                - 重置接口"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 13. 蓝牙（BLUETOOTH）："); // 汉化
    terminalView.println("  scan                 - 发现设备"); // 汉化
    terminalView.println("  pair <mac>           - 与设备配对"); // 汉化
    terminalView.println("  sniff                - 嗅探蓝牙数据"); // 汉化
    terminalView.println("  spoof <mac>          - 伪造MAC地址"); // 汉化
    terminalView.println("  status               - 显示当前状态"); // 汉化
    terminalView.println("  server               - 创建HID服务器"); // 汉化
    terminalView.println("  keyboard             - 启动键盘桥接"); // 汉化
    terminalView.println("  mouse <x> <y>        - 移动鼠标光标"); // 汉化
    terminalView.println("  mouse click          - 鼠标单击"); // 汉化
    terminalView.println("  mouse jiggle [ms]    - 鼠标随机移动"); // 汉化
    terminalView.println("  reset                - 重置接口"); // 汉化

    terminalView.println("");
    terminalView.println(" 14. WIFI：");
    terminalView.println("  scan                 - 列出Wi-Fi网络"); // 汉化
    terminalView.println("  connect              - 连接到网络"); // 汉化
    terminalView.println("  ping <host>          - 探测远程主机"); // 汉化
    terminalView.println("  discovery            - 发现网络设备"); // 汉化
    terminalView.println("  sniff                - 监控Wi-Fi数据包"); // 汉化
    terminalView.println("  probe                - 搜索网络接入点"); // 汉化
    terminalView.println("  spoof ap <mac>       - 伪造AP MAC地址"); // 汉化
    terminalView.println("  spoof sta <mac>      - 伪造终端MAC地址"); // 汉化
    terminalView.println("  status               - 显示Wi-Fi状态"); // 汉化
    terminalView.println("  deauth [ssid]        - 解除主机认证"); // 汉化
    terminalView.println("  disconnect           - 断开Wi-Fi连接"); // 汉化
    terminalView.println("  ap <ssid> <password> - 设置接入点"); // 汉化
    terminalView.println("  ap spam              - 群发随机信标"); // 汉化
    terminalView.println("  ssh <h> <u> <p> [p]  - 打开SSH会话"); // 汉化
    terminalView.println("  telnet <host> [port] - 打开telnet会话"); // 汉化
    terminalView.println("  nc <host> <port>     - 打开netcat会话"); // 汉化
    terminalView.println("  nmap <h> [-p ports]  - 扫描主机端口"); // 汉化
    terminalView.println("  modbus <host> [port] - Modbus TCP操作"); // 汉化
    terminalView.println("  http get <url>       - HTTP(s) GET请求"); // 汉化
    terminalView.println("  http analyze <url>   - 获取分析报告"); // 汉化
    terminalView.println("  lookup mac|ip <addr> - 查找MAC或IP地址"); // 汉化
    terminalView.println("  webui                - 显示Web UI的IP地址"); // 汉化
    terminalView.println("  reset                - 重置接口"); // 汉化

    terminalView.println("");
    terminalView.println(" 15. JTAG：");
    terminalView.println("  scan swd             - 扫描SWD引脚"); // 汉化
    terminalView.println("  scan jtag            - 扫描JTAG引脚"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 16. I2S：");
    terminalView.println("  play <freq> [ms]     - 播放指定频率的正弦波（毫秒）"); // 汉化
    terminalView.println("  record               - 持续读取麦克风数据"); // 汉化
    terminalView.println("  test <speaker|mic>   - 运行基础音频测试"); // 汉化
    terminalView.println("  reset                - 恢复默认设置"); // 汉化
    terminalView.println("  config               - 配置参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 17. CAN：");
    terminalView.println("  sniff                - 打印所有接收的帧"); // 汉化
    terminalView.println("  send [id]            - 发送指定ID的帧"); // 汉化
    terminalView.println("  receive [id]         - 捕获指定ID的帧"); // 汉化
    terminalView.println("  status               - CAN控制器状态"); // 汉化
    terminalView.println("  config               - 配置MCP2515参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 18. 以太网（ETHERNET）："); // 汉化
    terminalView.println("  connect              - 通过DHCP连接"); // 汉化
    terminalView.println("  status               - 显示以太网状态"); // 汉化
    terminalView.println("  ping <host>          - 探测远程主机"); // 汉化
    terminalView.println("  discovery            - 发现网络设备"); // 汉化
    terminalView.println("  ssh <h> <u> <p> [p]  - 打开SSH会话"); // 汉化
    terminalView.println("  telnet <host> [port] - 打开telnet会话"); // 汉化
    terminalView.println("  nc <host> <port>     - 打开netcat会话"); // 汉化
    terminalView.println("  nmap <h> [-p ports]  - 扫描主机端口"); // 汉化
    terminalView.println("  modbus <host> [port] - Modbus TCP操作"); // 汉化
    terminalView.println("  http get <url>       - HTTP(s) GET请求"); // 汉化
    terminalView.println("  http analyze <url>   - 获取分析报告"); // 汉化
    terminalView.println("  lookup mac|ip <addr> - 查找MAC或IP地址"); // 汉化
    terminalView.println("  reset                - 重置接口"); // 汉化
    terminalView.println("  config               - 配置W5500参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 19. 亚千兆（SUBGHZ）："); // 汉化
    terminalView.println("  scan                 - 搜索最佳频率"); // 汉化
    terminalView.println("  sniff                - 原始帧嗅探"); // 汉化
    terminalView.println("  sweep                - 分析频段"); // 汉化
    terminalView.println("  decode               - 接收并解码帧"); // 汉化
    terminalView.println("  replay               - 录制并重放帧"); // 汉化
    terminalView.println("  jam                  - 干扰选定频率"); // 汉化
    terminalView.println("  bruteforce           - 暴力破解12位密钥"); // 汉化
    terminalView.println("  trace                - 观察RX信号轨迹"); // 汉化
    terminalView.println("  load                 - 从文件系统加载.sub文件"); // 汉化
    terminalView.println("  listen               - RSSI转音频映射"); // 汉化
    terminalView.println("  setfrequency         - 设置工作频率"); // 汉化
    terminalView.println("  config               - 配置CC1101参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 20. RFID：");
    terminalView.println("  read                 - 读取RFID标签数据"); // 汉化
    terminalView.println("  write                - 向标签写入UID/块数据"); // 汉化
    terminalView.println("  clone                - 克隆Mifare UID"); // 汉化
    terminalView.println("  erase                - 擦除RFID标签"); // 汉化
    terminalView.println("  config               - 配置PN532参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 21. RF24：");
    terminalView.println("  scan                 - 搜索最佳活跃信道"); // 汉化
    terminalView.println("  sniff                - 嗅探原始帧"); // 汉化
    terminalView.println("  sweep                - 分析信道活跃度"); // 汉化
    terminalView.println("  jam                  - 干扰选定信道组"); // 汉化
    terminalView.println("  setchannel           - 设置工作信道"); // 汉化
    terminalView.println("  config               - 配置NRF24参数"); // 汉化

    terminalView.println("");
    terminalView.println(" 指令（大多数模式下可用）："); // 汉化
    terminalView.println(" 请参考文档查看指令语法格式。"); // 汉化
    terminalView.println("");
    terminalView.println(" 注意：使用 'mode' 命令在不同模式间切换"); // 汉化
    terminalView.println("");
}

bool UtilityController::isGlobalCommand(const TerminalCommand& cmd) {
    std::string root = cmd.getRoot();

    // Help is not available in standalone mode, too big to print it
    if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
        if (root == "help" || root == "h" || root == "?") return true;
    }

    return (root == "mode"  || root == "m" || root == "l" ||
            root == "logic" || root == "analogic" || root == "P" || root == "p") || 
            root == "system" || root == "guide" || root == "man";
}