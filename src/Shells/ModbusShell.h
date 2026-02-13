#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include "Services/ModbusService.h"
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Transformers/ArgTransformer.h"
#include "Managers/UserInputManager.h"
#include "States/GlobalState.h"

class ModbusShell {
public:
    ModbusShell(
        ITerminalView& view,
        IInput& input,
        ArgTransformer& argTransformer,
        UserInputManager& userInputManager,
        ModbusService& modbusService
    );

    void run(const std::string& host, uint16_t port);

private:
    // 操作
    void cmdConnect();
    void cmdSetUnit();
    void cmdReadHolding();            // FC03
    void cmdWriteHolding();           // FC06 / FC16
    void cmdReadInputRegisters();     // FC04
    void cmdReadCoils();              // FC01
    void cmdWriteCoils();             // FC05 / FC0F
    void cmdReadDiscreteInputs();     // FC02
    void cmdMonitorHolding();         // FC03 轮询

    // 辅助函数
    void printHeader();
    void printRegs(const std::vector<uint16_t>& regs, uint16_t baseAddr);
    void printCoils(const std::vector<uint8_t>& coilBytes, uint16_t baseAddr, uint16_t qty);
    void clearReply() { _reply = ModbusService::Reply{}; }
    bool waitReply(uint32_t timeoutMs);
    void installModbusCallbacks();

    ModbusService&     modbusService;
    ITerminalView&     terminalView;
    IInput&            terminalInput;
    ArgTransformer&    argTransformer;
    UserInputManager&  userInputManager;
    GlobalState&       state = GlobalState::getInstance();

    std::string hostShown = "";
    uint16_t    portShown = 502;
    uint8_t     unitId    = 1;
    uint32_t    reqTimeoutMs  = 6000;
    uint32_t    idleTimeoutMs = 60000;
    uint32_t    monitorPeriod = 500;
    
    ModbusService::Reply _reply;

    inline static const char* actions[] = {
        " 📖 读保持寄存器 (FC03)",
        " ✏️  写保持寄存器 (FC06/FC16)",
        " 📘 读输入寄存器 (FC04)",
        " 🔎 读线圈 (FC01)",
        " ✏️  写线圈 (FC05/FC0F)",
        " 📘 读离散输入 (FC02)",
        " ⏱️  监视保持寄存器 (FC03 轮询)",
        " 🆔 设置单元 ID",
        " 🔌 更改目标",
        "🚪 退出命令行"
    };

    static constexpr size_t actionsCount = sizeof(actions) / sizeof(actions[0]);
};
