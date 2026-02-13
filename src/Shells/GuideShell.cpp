#include "GuideShell.h"
#include "Managers/UserInputManager.h"
#include "Transformers/ArgTransformer.h"
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"

/**
 * @brief 构造函数：初始化引导Shell的依赖组件
 * @param tv 终端视图接口（负责文本输出）
 * @param in 输入接口（负责用户输入）
 * @param uim 用户输入管理类（负责输入验证）
 */
GuideShell::GuideShell(ITerminalView& tv,
                       IInput& in,
                       UserInputManager& uim)
    : terminalView(tv)
    , terminalInput(in)
    , userInputManager(uim) {}

/**
 * @brief 运行引导Shell主循环
 * @note 显示引导菜单，根据用户选择执行对应指南，选择退出项时终止循环
 */
void GuideShell::run() {
    bool loop = true;
    
    while (loop) {
        terminalView.println("\n=== 引导Shell ===");
        // 读取用户选择的指南项（验证输入有效性）
        int choice = userInputManager.readValidatedChoiceIndex("请选择指南主题", actions, actionsCount, actionsCount - 1);
        switch (choice) {
            case 0: cmdQuickStart(); break;    // 快速入门
            case 1: cmdExamples(); break;      // 使用示例
            case 2: cmdWebTerminal(); break;   // Web终端说明
            case 3: cmdInstructionSyntax(); break; // 指令语法
            case 4: cmdPythonAutomation(); break;  // Python自动化
            case 5: cmdLittleFS(); break;      // LittleFS文件系统
            case 6: cmdSafety(); break;        // 安全规范
            default:
                loop = false; // 选择退出项，终止循环
                break;
        }
    }

    terminalView.println("退出引导Shell...\n");
}

/**
 * @brief 快速入门指南（核心功能、模式、命令语法说明）
 */
void GuideShell::cmdQuickStart() {
    terminalView.println("\n=== 快速入门 ===\n");

    terminalView.println("工具用途：");
    terminalView.println("  探索和调试硬件及无线总线（WiFi/I2C/UART等）。");
    terminalView.println("");

    terminalView.println("工作模式：");
    terminalView.println("  选择要操作的总线模式");
    terminalView.println("  可输入 'mode' 或 'm' 切换模式。");
    terminalView.println("");

    terminalView.println("命令语法：");
    terminalView.println("  <参数>  必选参数");
    terminalView.println("  [参数]  可选参数");
    terminalView.println("");
    terminalView.println("  示例：");
    terminalView.println("    read <地址> <寄存器>");
    terminalView.println("    read 0x3C 0x00");
    terminalView.println("");
    terminalView.println("    dump <地址> [长度]");
    terminalView.println("    dump 0x50 256");
    terminalView.println("");

    terminalView.println("Shell菜单：");
    terminalView.println("  部分命令会打开子菜单。");
    terminalView.println("  通过序号选择要执行的操作。");
    terminalView.println("  选择“退出”返回上级菜单。");
    terminalView.println("");

    terminalView.println("提示：");
    terminalView.println("  在任意位置输入 'help' 查看完整命令列表。");
    terminalView.println("  查看Wiki获取详细使用流程。");
    terminalView.println("  https://github.com/geo-tp/ESP32-Bus-Pirate/wiki");
}

/**
 * @brief 常用工作流程示例（各总线模式的典型操作）
 */
void GuideShell::cmdExamples() {
    terminalView.println("\n=== 模式：常用工作流程 ===\n");

    terminalView.println("[WIFI] 连接与探索：");
    terminalView.println("  mode wifi");
    terminalView.println("  scan");
    terminalView.println("  connect");
    terminalView.println("  status");
    terminalView.println("  nmap 192.168.1.10 -p 22");
    terminalView.println("  lookup mac 44:38:39:ff:ef:57");
    terminalView.println("  nc 192.168.1.12 80");
    terminalView.println("  ap MyHotspot 12345678");
    terminalView.println("  ping google.com ");
    terminalView.println("");

    terminalView.println("[I2C] 模块调试：");
    terminalView.println("  mode i2c");
    terminalView.println("  scan");
    terminalView.println("  ping 0x13");
    terminalView.println("  read 0x13 0x00      (读取寄存器)");
    terminalView.println("  write 0x13 0x01 0x0 (写入寄存器)");
    terminalView.println("  monitor 0x13 500    (监控寄存器变化)");
    terminalView.println("");

    terminalView.println("[DIO] 引脚驱动/监测：");
    terminalView.println("  mode dio");
    terminalView.println("  read 1");
    terminalView.println("  set 1 L");
    terminalView.println("  set 1 HIGH");
    terminalView.println("  set 1 O");
    terminalView.println("  toggle 1 250");
    terminalView.println("  sniff 1");
    terminalView.println("");

    terminalView.println("[UART] 串口目标设备：");
    terminalView.println("  mode uart");
    terminalView.println("  scan                (自动波特率检测)");
    terminalView.println("  write \"AT\"");
    terminalView.println("  read");
    terminalView.println("  spam Hello 1000");
    terminalView.println("  xmodem send /f.txt  (发送文件)");
    terminalView.println("");

    terminalView.println("注意：");
    terminalView.println("  以上仅为示例。");
    terminalView.println("  查看Wiki获取详细使用流程。");
    terminalView.println("  https://github.com/geo-tp/ESP32-Bus-Pirate/wiki");
}

/**
 * @brief Web终端使用说明（无屏板卡的WiFi Web模式启动方法）
 */
void GuideShell::cmdWebTerminal() {
    terminalView.println("\n=== Web终端 ===\n");

    terminalView.println("使用Web界面：");
    terminalView.println(" mode wifi");
    terminalView.println(" connect");
    terminalView.println(" reboot, pick WiFi Web");
    terminalView.println("");

    terminalView.println("精简版板卡（无屏幕）：");
    terminalView.println("  启动Wi-Fi Web模式：");
    terminalView.println("    • 重置设备");
    terminalView.println("    • 按下板卡按键 < 3秒");
    terminalView.println("    • LED状态说明：");
    terminalView.println("       白色 : 正在连接WiFi");
    terminalView.println("       蓝色  : 无已保存的WiFi配置");
    terminalView.println("       绿色 : 连接成功");
    terminalView.println("       红色   : 连接失败");
    terminalView.println("");
    terminalView.println("  重要提示：");
    terminalView.println("    上电时请勿按住BOOT键");
    terminalView.println("");

    terminalView.println("注意事项：");
    terminalView.println("  • 部分命令会中断会话");
    terminalView.println("    示例：Web终端下执行 wifi disconnect");
    terminalView.println("    示例：串口终端下执行 usb reset / usb mode");
    terminalView.println("");
    terminalView.println("提示：");
    terminalView.println("  大量输出建议使用串口终端。");
    terminalView.println("  例如：嗅探器（I2C/单总线）。");
}

/**
 * @brief 底层指令语法说明（[]包裹的字节级操作指令）
 */
void GuideShell::cmdInstructionSyntax() {
    terminalView.println("\n=== 指令语法 [ ... ] ===\n");

    terminalView.println("用途：");
    terminalView.println("  发送底层总线操作指令。");
    terminalView.println("");

    terminalView.println("工作原理：");
    terminalView.println("  [ ] 内的所有内容均为一条指令。");
    terminalView.println("  解析为字节级别的操作动作。");
    terminalView.println("  执行逻辑依赖当前激活的工作模式。");
    terminalView.println("");

    terminalView.println("常用示例：");
    terminalView.println("  [0xAA 0xBB]          写入字节");
    terminalView.println("  [r:4]                读取4个字节");
    terminalView.println("  [\"ABC\"]            写入ASCII字符串");
    terminalView.println("  [d:10]               延时10微秒");
    terminalView.println("  [D:1]                延时1毫秒");
    terminalView.println("");

    terminalView.println("组合示例：");
    terminalView.println("  [0xA0 d:10, r:2 0xB1 r]");
    terminalView.println("    写入 → 延时 → 读取2字节 → 写入 → 读取");
    terminalView.println("");
    terminalView.println("  [d:100 D:2]");
    terminalView.println("    延时100微秒 → 再延时2毫秒");
    terminalView.println("");
    terminalView.println("  [\"AT\" d:10 r:255]");
    terminalView.println("    写入AT → 等待 → 读取响应");
    terminalView.println("");
}

/**
 * @brief Python自动化脚本说明（串口通信）
 */
void GuideShell::cmdPythonAutomation() {
    terminalView.println("\n=== Python自动化（串口） ===\n");
    terminalView.println("代码仓库：ESP32-Bus-Pirate-Scripts");
    terminalView.println("https://github.com/geo-tp/ESP32-Bus-Pirate-Scripts");
    terminalView.println("");
    terminalView.println("最简示例：");
    terminalView.println("  bp = BusPirate.auto_connect()");
    terminalView.println("  bp.start()");
    terminalView.println("  bp.change_mode(\"dio\")");
    terminalView.println("  bp.send(\"set 1 LOW\")");
    terminalView.println("  response = bp.receive_all(2)");
    terminalView.println("  bp.stop()");
}

/**
 * @brief LittleFS文件系统说明（Web UI文件管理）
 */
void GuideShell::cmdLittleFS() {
    terminalView.println("\n=== LittleFS / Web UI文件管理 ===\n");
    terminalView.println("LittleFS是存储在Flash中的轻量级文件系统。");
    terminalView.println("使用Web UI：点击“Files”按钮上传/下载/删除文件。");
    terminalView.println("例如：可加载和记录红外码文件。");
    terminalView.println("");
    terminalView.println("限制：");
    terminalView.println("  • 8MB Flash的板卡约有4.5MB可用空间（近似值）。");
    terminalView.println("  • 刷入不同固件可能会覆盖现有文件。");
}

/**
 * @brief 安全规范与电压说明
 */
void GuideShell::cmdSafety() {
    terminalView.println("\n=== 安全规范 / 电压说明 ===\n");
    terminalView.println("仅使用3.3V / 5V电压。");
    terminalView.println("请勿连接更高电压的外设（可能损坏ESP32）。");
}