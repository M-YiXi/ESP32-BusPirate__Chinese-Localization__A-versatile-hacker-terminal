#pragma once

#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"
#include "Transformers/ArgTransformer.h"
#include "Services/I2cService.h"
#include "Managers/BinaryAnalyzeManager.h"

class I2cEepromShell {
public:
    I2cEepromShell(
        ITerminalView& view,
        IInput& input,
        I2cService& i2cService,
        ArgTransformer& argTransformer,
        UserInputManager& userInputManager,
        BinaryAnalyzeManager & binaryAnalyzeManager
    );

    void run(uint8_t addr = 0x50);

private:
    // 可用操作 //汉化
    inline static constexpr const char* kActions[] = {
        " 🔍 探测",          //汉化
        " 📊 分析",          //汉化
        " 📖 读取字节",      //汉化
        " ✏️  写入字节",     //汉化
        " 🗃️  ASCII 转储",  //汉化
        " 🗃️  原始转储",     //汉化
        " 💣 擦除 EEPROM",  //汉化
        " 🚪 退出命令行"    //汉化
    };

    // 可用 EEPROM 型号 //汉化
    inline static constexpr const char* kModels[] = {
        " 24x01  (1 Kbit)",
        " 24x02  (2 Kbit)",
        " 24x04  (4 Kbit)",
        " 24x08  (8 Kbit)",
        " 24x16  (16 Kbit)",
        " 24x32  (32 Kbit)",
        " 24x64  (64 Kbit)",
        " 24x128 (128 Kbit)",
        " 24x256 (256 Kbit)",
        " 24x512 (512 Kbit)",
        " 24x1025 (1 Mbit)",
        " 24x1026 (1 Mbit)",
        " 24xM01  (1 Mbit)",
        " 24xM02  (2 Mbit)"
    };

    static constexpr size_t kActionsCount = sizeof(kActions) / sizeof(kActions[0]);
    static constexpr size_t kModelsCount  = sizeof(kModels)  / sizeof(kModels[0]);

    std::vector<uint16_t> memoryLengths = {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1025, 1025, 1025, 2048
    };

    ITerminalView& terminalView;
    IInput& terminalInput;
    I2cService& i2cService;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    BinaryAnalyzeManager& binaryAnalyzeManager;
    std::string selectedModel = "未知"; //汉化
    uint32_t selectedLength = 0;
    bool initialized = false;
    uint8_t selectedI2cAddress;

    void cmdProbe();
    void cmdAnalyze();
    void cmdRead();
    void cmdWrite();
    void cmdDump(bool raw = false);
    void cmdErase();
};