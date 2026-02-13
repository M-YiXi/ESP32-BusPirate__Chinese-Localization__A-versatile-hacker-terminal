#include "ModbusShell.h"

ModbusShell::ModbusShell(
    ITerminalView& view,
    IInput& input,
    ArgTransformer& argTransformer,
    UserInputManager& userInputManager,
    ModbusService& modbusService
)
: terminalView(view)
, terminalInput(input)
, argTransformer(argTransformer)
, userInputManager(userInputManager)
, modbusService(modbusService) 
{
}

void ModbusShell::run(const std::string& host, uint16_t port) {
    installModbusCallbacks();

    hostShown = host;
    portShown = port ? port : 502;

    if (!modbusService.setTarget(hostShown, portShown)) {
        terminalView.println("MODBUS：DNS/目标地址错误。\n"); //汉化
        return;
    }

    modbusService.begin(reqTimeoutMs, idleTimeoutMs, 4);
    terminalView.println("");

    bool start = true;
    while (start) {
        printHeader();
        auto choice = userInputManager.readValidatedChoiceIndex("选择 Modbus 操作", actions, actionsCount, 0); //汉化

        switch (choice) {
            case 0: cmdReadHolding();         break;
            case 1: cmdWriteHolding();        break;
            case 2: cmdReadInputRegisters();  break;
            case 3: cmdReadCoils();           break;
            case 4: cmdWriteCoils();          break;
            case 5: cmdReadDiscreteInputs();  break;
            case 6: cmdMonitorHolding();      break;
            case 7: cmdSetUnit();             break;
            case 8: cmdConnect();             break;
            case 9: terminalView.println("Modbus 命令行已关闭。\n"); start = false; //汉化
        }
    }
    modbusService.clearCallbacks();
    modbusService.setTarget("", 0);
}

// ===== 操作 ===== //汉化

void ModbusShell::cmdConnect() {
    terminalView.print("主机或 IP："); //汉化
    std::string h = userInputManager.getLine();
    uint16_t p = userInputManager.readValidatedUint32("端口", 502); //汉化

    terminalView.println("正在连接到 " + h + ":" + std::to_string(p) + " ..."); //汉化
    if (!modbusService.setTarget(h, p)) {
        terminalView.println("DNS/目标地址错误。\n"); //汉化
        return;
    }
    hostShown = h;
    portShown = p;

    modbusService.begin(reqTimeoutMs, idleTimeoutMs, 4);

    terminalView.println(" ✅ 成功。\n"); //汉化
}

void ModbusShell::cmdSetUnit() {
    unitId  = userInputManager.readValidatedUint8("单元 ID (1-247)", unitId, 1, 247); //汉化
    terminalView.println(" ✅ 成功。\n"); //汉化
}

void ModbusShell::cmdReadHolding() {
    // 地址和数量 //汉化
    uint16_t addr = userInputManager.readValidatedUint32("起始地址", 0); //汉化
    uint16_t qty = userInputManager.readValidatedUint32("数量", 8); //汉化

    // 读取 //汉化
    terminalView.println("正在读取...\n"); //汉化
    clearReply();
    Error e = modbusService.readHolding(unitId, addr, qty);
    if (e != SUCCESS) {
        ModbusError me(e);
        terminalView.println(std::string("请求错误：") + (const char*)me + "\n"); //汉化
        return;
    }

    if (!waitReply(reqTimeoutMs + 1000)) {
        terminalView.println("超时。\n"); //汉化
        return;
    }

    if (_reply.ok) {
        printRegs(_reply.regs, addr);
        terminalView.println("");
    } else if (_reply.fc & 0x80) {
        char buf[64]; snprintf(buf, sizeof(buf), "异常 0x%02X\n", _reply.exception); //汉化
        terminalView.println(buf);
    } else {
        terminalView.println((_reply.error + "\n").c_str());
    }
}

void ModbusShell::cmdWriteHolding() {
    // 数值 //汉化
    int addr = userInputManager.readValidatedUint32("起始地址", 0); //汉化
    std::string line = userInputManager.readValidatedHexString("输入 16 位值：", 0, true, 4); //汉化
    auto vals = argTransformer.parseHexList16(line);

    // 确认 //汉化
    bool confirm = userInputManager.readYesNo(
        "向地址 " + std::to_string(addr) + " 写入 " + std::to_string(vals.size()) + " 个寄存器？", false); //汉化
    if (!confirm) { terminalView.println("已取消。\n"); return; } //汉化

    // 写入 //汉化
    terminalView.println("正在写入...\n"); //汉化
    clearReply();
    Error e = SUCCESS;
    if (vals.size() == 1) {
        e = modbusService.writeHoldingSingle(unitId, (uint16_t)addr, vals[0]);   // FC06
    } else {
        e = modbusService.writeHoldingMultiple(unitId, (uint16_t)addr, vals);    // FC16
    }
    if (e != SUCCESS) {
        ModbusError me(e);
        terminalView.println(std::string("请求错误：") + (const char*)me + "\n"); //汉化
        return;
    }

    if (!waitReply(reqTimeoutMs + 1000)) {
        terminalView.println("超时。\n");  //汉化
        return;
    }

    if (_reply.ok && (_reply.fc == 0x10 || _reply.fc == 0x06)) {
        terminalView.println(" ✅ 成功。\n"); //汉化
    } else if (_reply.fc & 0x80) {
        char buf[64]; snprintf(buf, sizeof(buf), "异常 0x%02X\n", _reply.exception); //汉化
        terminalView.println(buf);
    } else {
        terminalView.println((_reply.error + "\n").c_str());
    }
}

void ModbusShell::cmdMonitorHolding() {
    uint16_t addr = userInputManager.readValidatedUint32("起始地址", 0); //汉化
    uint16_t qty = userInputManager.readValidatedUint32("数量", 8); //汉化
    uint32_t period = userInputManager.readValidatedUint32("周期 (毫秒)", 500); //汉化

    terminalView.println("正在监视... 按 [回车] 停止。\n"); //汉化

    std::vector<uint16_t> last;
    bool stop = false;
    while (true) {
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') { terminalView.println("已停止。\n"); break; } //汉化

        clearReply();
        modbusService.readHolding(unitId, addr, qty);

        // 等待响应并处理用户输入 //汉化
        const uint32_t t0 = millis();
        while ((millis() - t0) < reqTimeoutMs) {
            if (_reply.ready) break;

            // 按回车停止 //汉化
            char k = terminalInput.readChar();
            if (k == '\r' || k == '\n') { 
                stop = true; 
                break; 
            }
            delay(5);
        }

        if (_reply.ok && _reply.regs != last) {
            printRegs(_reply.regs, addr);
            terminalView.println("");
            last = _reply.regs;
        }
    }
}

void ModbusShell::cmdReadInputRegisters() {
  uint16_t addr = userInputManager.readValidatedUint32("起始地址 (输入寄存器)", 0); //汉化
  uint16_t qty  = userInputManager.readValidatedUint32("数量 (最大 125)", 1); //汉化

  terminalView.println("正在读取 (FC04)...\n"); //汉化
  clearReply();
  Error e = modbusService.readInputRegisters(unitId, addr, qty);
  if (e != SUCCESS) { ModbusError me(e); terminalView.println(std::string("请求错误：") + (const char*)me + "\n"); return; } //汉化

  if (!waitReply(reqTimeoutMs + 1000)) { terminalView.println("超时。\n"); return; } //汉化

  if (_reply.ok) { printRegs(_reply.regs, (uint16_t)addr); terminalView.println(""); }
  else if (_reply.fc & 0x80) { char buf[64]; snprintf(buf, sizeof(buf), "异常 0x%02X\n", _reply.exception); terminalView.println(buf); } //汉化
  else { terminalView.println((_reply.error + "\n").c_str()); }
}

void ModbusShell::cmdReadCoils() {
    uint16_t addr = userInputManager.readValidatedUint32("起始地址 (线圈)", 0); //汉化
    uint16_t qty  = userInputManager.readValidatedUint32("数量", 8); //汉化

    terminalView.println("正在读取 (FC01)...\n"); //汉化
    clearReply();
    Error e = modbusService.readCoils(unitId, addr, qty);
    if (e != SUCCESS) { ModbusError me(e); terminalView.println(std::string("请求错误：") + (const char*)me + "\n"); return; } //汉化

    if (!waitReply(reqTimeoutMs + 1000)) { terminalView.println("超时。\n"); return; } //汉化
    if (!_reply.ok) {
        // 显示错误 //汉化
        terminalView.println((_reply.error + "\n").c_str());
        if (!_reply.raw.empty()) {
            std::string hex;
            for (auto b : _reply.raw) { char x[4]; snprintf(x, sizeof(x), "%02X ", b); hex += x; }
            terminalView.println("原始响应：" + hex + "\n"); //汉化
        }
        return;
    }

    const size_t need = (qty + 7) / 8;
    if (_reply.coilBytes.size() < need) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                "响应过短：需要 %u 字节（%u 个线圈），实际收到 %u。\n", //汉化
                (unsigned)need, (unsigned)qty, (unsigned)_reply.coilBytes.size());
        terminalView.println(msg);
        // 十六进制转储 //汉化
        if (!_reply.raw.empty()) {
            std::string hex;
            for (auto b : _reply.raw) { char x[4]; snprintf(x, sizeof(x), "%02X ", b); hex += x; }
            terminalView.println("原始响应：" + hex + "\n"); //汉化
        }
        return;
    }
}

void ModbusShell::cmdWriteCoils() {
    uint16_t addr = userInputManager.readValidatedUint32("起始地址 (线圈)", 0); //汉化

    terminalView.println("输入位值（0/1），格式如 '1 0 1 1' 或 '1011'"); //汉化
    terminalView.print("> "); //汉化
    std::string line = userInputManager.getLine();
    if (line.empty()) { terminalView.println("已取消。\n"); return; } //汉化

    // 解析位 //汉化
    auto bits = argTransformer.parse01List(line);
    if (bits.empty()) { terminalView.println("未找到有效位。\n"); return; } //汉化

    // 确认 //汉化
    if (!userInputManager.readYesNo("向地址 " + std::to_string(addr) + " 写入 " + std::to_string(bits.size()) +
                                    " 个线圈？", false)) { //汉化
        terminalView.println("已取消。\n"); return; //汉化
    }

    terminalView.println("正在写入线圈...\n"); //汉化
    clearReply();
    Error e = SUCCESS;

    if (bits.size() == 1) {
        // FC05
        e = modbusService.writeSingleCoil(unitId, addr, bits[0] != 0);
    } else {
        // FC0F
        auto packed = argTransformer.packLsbFirst(bits);
        e = modbusService.writeMultipleCoils(unitId, addr, packed, bits.size());
    }
    if (e != SUCCESS) { ModbusError me(e); terminalView.println(std::string("请求错误：") + (const char*)me + "\n"); return; } //汉化
    if (!waitReply(reqTimeoutMs + 1000)) { terminalView.println("超时。\n"); return; } //汉化
    if (!_reply.ok) { terminalView.println((_reply.error + "\n").c_str()); return; }

    terminalView.println(" ✅ 成功。\n"); //汉化
    clearReply();
}

void ModbusShell::cmdReadDiscreteInputs() {
  uint16_t addr = userInputManager.readValidatedUint32("起始地址 (离散输入)", 0); //汉化
  uint16_t qty  = userInputManager.readValidatedUint32("数量", 8); //汉化

  terminalView.println("正在读取 (FC02)...\n"); //汉化
  clearReply();
  Error e = modbusService.readDiscreteInputs(unitId, addr, qty);
  if (e != SUCCESS) { ModbusError me(e); terminalView.println(std::string("请求错误：") + (const char*)me + "\n"); return; } //汉化

  if (!waitReply(reqTimeoutMs + 1000)) { terminalView.println("超时。\n"); return; } //汉化

  if (_reply.ok) { printCoils(_reply.coilBytes, addr, qty); terminalView.println(""); }
  else if (_reply.fc & 0x80) { char buf[64]; snprintf(buf, sizeof(buf), "异常 0x%02X\n", _reply.exception); terminalView.println(buf); } //汉化
  else { terminalView.println((_reply.error + "\n").c_str()); }
}

// === 辅助函数 === //汉化

void ModbusShell::installModbusCallbacks() {
  modbusService.setOnReply([this](const ModbusService::Reply& r, uint32_t token){
    _reply.fc       = r.fc;
    _reply.ok       = r.ok;
    _reply.exception= r.exception;
    _reply.error    = r.error;
    _reply.regs     = r.regs;
    _reply.ready    = true;
  });
}

bool ModbusShell::waitReply(uint32_t timeoutMs) {
    const uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (_reply.ready) return true;
        delay(10);
    }
    return _reply.ready;
}

void ModbusShell::printHeader() {
    terminalView.println("=== Modbus 命令行 ==="); //汉化
    terminalView.println(
        "目标：" + (hostShown.empty() ? std::string("<未设置>") : hostShown) + ":" + std::to_string(portShown) + //汉化
        " | 单元：" + std::to_string(unitId)
    );
    terminalView.println("");
}

void ModbusShell::printRegs(const std::vector<uint16_t>& regs, uint16_t baseAddr) {
    for (size_t i=0;i<regs.size();++i) {
        const uint16_t v = regs[i];
        char line[96];
        snprintf(line, sizeof(line), "R[%u] (地址 %u) = 0x%04X  (%5u)", //汉化
                 (unsigned)i, (unsigned)(baseAddr + i), v, v);
        terminalView.println(line);
    }
}

void ModbusShell::printCoils(const std::vector<uint8_t>& coilBytes, uint16_t baseAddr, uint16_t qty) {

    for (uint16_t i = 0; i < qty; ++i) {
        const size_t iByte = i >> 3;        // i / 8
        const uint8_t iBit = i & 0x07;      // i % 8
        uint8_t bit = 0;
        if (iByte < coilBytes.size()) {
            bit = (coilBytes[iByte] >> iBit) & 0x01; // LSB-first
        }
        char line[64];
        snprintf(line, sizeof(line), "C[%u] (地址 %u) = %u", //汉化
                    (unsigned)i, (unsigned)(baseAddr + i), (unsigned)bit);
        terminalView.println(line);
    }
}