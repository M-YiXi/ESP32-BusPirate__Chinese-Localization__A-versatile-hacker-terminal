#pragma once
#include <vector>
#include <string>

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"

class GuideShell {
public:
    GuideShell(ITerminalView& tv,
               IInput& in,
               UserInputManager& uim);

    void run();

private:
    ITerminalView& terminalView;
    IInput& terminalInput;
    UserInputManager& userInputManager;

    inline static constexpr const char* actions[] = {
        " 🚀 快速入门",       //汉化
        " 🧩 命令示例",       //汉化
        " 🌐 Web 终端",      //汉化
        " 🧰 指令语法",      //汉化
        " 🐍 Python 自动化", //汉化
        " 📒 文件系统",      //汉化
        " ⚠️  安全（电压）",  //汉化
        " 🚪 退出"          //汉化
    };

    inline static constexpr size_t actionsCount =
        sizeof(actions) / sizeof(actions[0]);

    void cmdQuickStart();
    void cmdExamples();
    void cmdWebTerminal();
    void cmdInstructionSyntax();
    void cmdPythonAutomation();
    void cmdLittleFS();
    void cmdSafety();
};