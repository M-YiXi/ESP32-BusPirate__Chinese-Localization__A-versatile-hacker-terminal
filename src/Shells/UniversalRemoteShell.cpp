#include "UniversalRemoteShell.h"

UniversalRemoteShell::UniversalRemoteShell(
    ITerminalView& view,
    IInput& input,
    InfraredService& irService,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager
) : infraredService(irService),
    terminalView(view),
    terminalInput(input),
    argTransformer(argTransformer),
    userInputManager(userInputManager) {}

void UniversalRemoteShell::run() {
    // 遥控操作
    const std::vector<std::string> actions = {
        " ⏻ 开/关",
        " 🔇 静音",
        " ▶️  播放",
        " ⏸️  暂停",
        " 🔊 音量加",
        " 🔉 音量减",
        " 🔼 频道加",
        " 🔽 频道减",
        " 🚪 退出命令行"
    };

    terminalView.println("红外: 通用红外遥控启动...\n");

    while (true) {
        // 显示操作
        terminalView.println("=== 通用遥控命令行 ===");
        int index = userInputManager.readValidatedChoiceIndex("选择遥控操作", actions, 0);
        if (index < 0 || index >= (int)actions.size()) {
            terminalView.println("无效选择.\n");
            continue;
        }

        // 处理退出
        if (actions[index] == " 🚪 退出命令行") {
            terminalView.println("红外: 正在退出红外遥控命令行...\n");
            break;
        }

        terminalView.println("正在发送所有代码: " + actions[index] + "... 按 [ENTER] 停止.\n");
        switch (index) {
        case 0: sendCommandGroup(universalOnOff,        sizeof(universalOnOff)        / sizeof(universalOnOff[0]));        break;
        case 1: sendCommandGroup(universalMute,         sizeof(universalMute)         / sizeof(universalMute[0]));         break;
        case 2: sendCommandGroup(universalPlay,         sizeof(universalPlay)         / sizeof(universalPlay[0]));         break;
        case 3: sendCommandGroup(universalPause,        sizeof(universalPause)        / sizeof(universalPause[0]));        break;
        case 4: sendCommandGroup(universalVolUp,        sizeof(universalVolUp)        / sizeof(universalVolUp[0]));        break;
        case 5: sendCommandGroup(universalVolDown,      sizeof(universalVolDown)      / sizeof(universalVolDown[0]));      break;
        case 6: sendCommandGroup(universalChannelUp,    sizeof(universalChannelUp)    / sizeof(universalChannelUp[0]));    break;
        case 7: sendCommandGroup(universalChannelDown,  sizeof(universalChannelDown)  / sizeof(universalChannelDown[0]));  break;
        }


    }
}

void UniversalRemoteShell::sendCommandGroup(const InfraredCommandStruct* group, size_t size) {
    for (size_t i = 0; i < size; ++i) {

        InfraredCommand cmd(group[i].proto, group[i].device, group[i].subdevice, group[i].function);
        infraredService.sendInfraredCommand(cmd);
        delay(100);

        // 按回车停止
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') {
            terminalView.println(" ⛔ 用户已停止.\n");
            return;
        }
        
        // 显示发送的命令信息
        terminalView.println(
            " ✅ 已发送 协议=" + InfraredProtocolMapper::toString(cmd.getProtocol()) +
            " 设备=" + std::to_string(cmd.getDevice()) +
            " 子设备=" + std::to_string(cmd.getSubdevice()) +
            " 命令=" + std::to_string(cmd.getFunction())
        );
    }
    terminalView.println("");
}