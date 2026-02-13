#pragma once

#include "Services/OneWireService.h"
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Transformers/ArgTransformer.h"
#include "Managers/UserInputManager.h"
#include "Managers/BinaryAnalyzeManager.h"

class OneWireEepromShell {
public:
    OneWireEepromShell(
        ITerminalView& view,
        IInput& input,
        OneWireService& oneWireService,
        ArgTransformer& argTransformer,
        UserInputManager& userInputManager,
        BinaryAnalyzeManager& binaryAnalyzeManager
    );

    void run();

private:
    void cmdProbe();
    void cmdRead();
    void cmdWrite();
    void cmdDump();
    void cmdErase();
    void cmdAnalyze();

    OneWireService& oneWireService;
    ITerminalView& terminalView;
    IInput& terminalInput;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    BinaryAnalyzeManager& binaryAnalyzeManager;

    const std::vector<std::string> actions = {
        " 🔍 探测",
        " 📊 分析",
        " 📖 读取",
        " ✏️  写入",
        " 🗃️  转储",
        " 💣 擦除",
        " 🚪 退出命令行"
    };

    std::string eepromModel = "DS2431"; // 默认
    uint8_t eepromPageSize = 8;
    uint16_t eepromSize = 128;
};