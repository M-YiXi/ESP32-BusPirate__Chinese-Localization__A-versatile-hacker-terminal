#pragma once

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"
#include "Transformers/ArgTransformer.h"
#include "Services/SpiService.h"
#include "Managers/BinaryAnalyzeManager.h"
#include "Models/TerminalCommand.h"
#include "States/GlobalState.h"

class SpiFlashShell {
public:
    SpiFlashShell(
        SpiService& spiService,
        ITerminalView& view,
        IInput& input,
        ArgTransformer& argTransformer,
        UserInputManager& userInputManager,
        BinaryAnalyzeManager& binaryAnalyzeManager
    );

    void run();

private:
    const std::vector<std::string> actions = {
        " 🔍 探测 Flash",
        " 📊 分析 Flash",
        " 🔎 搜索字符串",
        " 📜 提取字符串",
        " 📖 读取字节",
        " ✏️  写入字节",
        " 🗃️  ASCII 转储",
        " 🗃️  原始转储",
        " 💣 擦除 Flash",
        "🚪 退出命令行"
    };

    SpiService& spiService;
    ITerminalView& terminalView;
    IInput& terminalInput;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    BinaryAnalyzeManager& binaryAnalyzeManager;
    GlobalState& state = GlobalState::getInstance();

    void cmdProbe();
    void cmdAnalyze();
    void cmdSearch();
    void cmdStrings();
    void cmdRead();
    void cmdWrite();
    void cmdErase();
    void cmdDump(bool raw = false);
    void readFlashInChunks(uint32_t address, uint32_t length);
    void readFlashInChunksRaw(uint32_t address, uint32_t length);
    uint32_t readFlashCapacity();
    bool checkFlashPresent();
};