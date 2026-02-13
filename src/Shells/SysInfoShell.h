#pragma once
#include <string>
#include <vector>
#include <esp_system.h> 
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"
#include "Transformers/ArgTransformer.h"
#include "Services/SystemService.h"
#include "Services/WifiService.h"
#include "States/GlobalState.h"

class SysInfoShell {
public:
    SysInfoShell(ITerminalView& terminalView,
                 IInput& terminalInput,
                 UserInputManager& userInputManager,
                 ArgTransformer& argTransformer,
                 SystemService& systemService,
                 WifiService& wifiService);

    void run();

private:
    std::vector<std::string> actions = {
        " 📊 系统摘要",
        " 📟 硬件信息",
        " 🗄️  内存",
        " 🧩 分区表",
        " 🗂️  LittleFS",
        " 🧰 NVS 统计",
        " 📒 NVS 条目",
        " 🌐 网络",
        " 🔄 重启",
        "🚪 退出"
    };

    const char* resetReasonToStr(int r) {
        switch (static_cast<esp_reset_reason_t>(r)) {
            case ESP_RST_POWERON:   return "上电复位";
            case ESP_RST_EXT:       return "外部复位";
            case ESP_RST_SW:        return "软件复位";
            case ESP_RST_PANIC:     return "异常复位";
            case ESP_RST_INT_WDT:   return "中断看门狗";
            case ESP_RST_TASK_WDT:  return "任务看门狗";
            case ESP_RST_WDT:       return "其他看门狗";
            case ESP_RST_DEEPSLEEP: return "深度睡眠唤醒";
            case ESP_RST_BROWNOUT:  return "掉电复位";
            case ESP_RST_SDIO:      return "SDIO";
            default:                return "未知";
        }
    }

    const char* flashModeToStr(int m) {
        switch (m) {
            case 0: return "QIO";
            case 1: return "QOUT";
            case 2: return "DIO";
            case 3: return "DOUT";
            case 4: return "FAST_READ";
            case 5: return "SLOW_READ";
            default:   return "?";
        }
    }

    // 操作
    void cmdSummary();
    void cmdHardwareInfo();
    void cmdMemory();
    void cmdPartitions();
    void cmdFS();
    void cmdNVS(bool listEntries);
    void cmdNet();
    void cmdReboot(bool hard = false);

    ITerminalView&     terminalView;
    IInput&            terminalInput;
    UserInputManager&  userInputManager;
    ArgTransformer&    argTransformer;
    SystemService&     systemService;
    WifiService&       wifiService;
    GlobalState&       state = GlobalState::getInstance();
};