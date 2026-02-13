#include "CanController.h"

CanController::CanController(ITerminalView& terminalView, IInput& terminalInput, UserInputManager& userInputManager,
                             CanService& canService, ArgTransformer& argTransformer)
    : terminalView(terminalView), terminalInput(terminalInput), userInputManager(userInputManager),
      canService(canService), argTransformer(argTransformer) {}

/*
Entry point for CAN commands
*/
void CanController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "sniff")          handleSniff();
    else if (cmd.getRoot() == "send")      handleSend(cmd);
    else if (cmd.getRoot() == "receive")   handleReceive(cmd);
    else if (cmd.getRoot() == "status")    handleStatus();
    else if (cmd.getRoot() == "config")    handleConfig();
    else handleHelp();
}

/*
Sniff all CAN frames
*/
void CanController::handleSniff() {
    canService.reset();
    
    terminalView.println("CAN嗅探: 等待帧数据... 按下[ENTER]停止.\n"); // 汉化
    
    unsigned long lastFrameTime = millis();
    while (true) {
        auto frame = canService.readFrameAsString();

        // Received frame
        if (!frame.empty()) {
            terminalView.println(" 📥 " + frame);
            lastFrameTime = millis();  // reset timer
        }

        // Reset CAN if no frame for 3 seconds
        if (millis() - lastFrameTime > 3000) {
            canService.reset();
            lastFrameTime = millis();
        }

        // Abort if ENTER is pressed
        char ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            terminalView.println("\nCAN嗅探: 已被用户停止."); // 汉化
            break;
        }
    }
}

/*
Status of the CAN controller
*/
void CanController::handleStatus() {

    std::string status = canService.getStatus();
    terminalView.println("\n  CAN状态:"); // 汉化
    terminalView.println(status);
}

/*
Send a CAN frame with specific ID
*/
void CanController::handleSend(const TerminalCommand& cmd) {

    int id;
    if (!cmd.getSubcommand().empty() && argTransformer.isValidNumber(cmd.getSubcommand())) {
        id = argTransformer.parseHexOrDec16(cmd.getSubcommand());
    } else {
        // Ask user for ID
        id = userInputManager.readValidatedCanId("Filter CAN ID", 0x123);
    }
    
    // Check max value allowed for an id
    if (id > 0x7FF) {
        terminalView.println("\n❌ 仅支持11位标准ID (最大值0x7FF)."); // 汉化
        return;
    }

    // Ask for data bytes
    terminalView.println("输入以空格分隔的字节 (例如 '01 02 0A FF'):"); // 汉化
    std::string hexString = userInputManager.readValidatedHexString("", 0, true);

    // Convert hex string to byte vector
    std::vector<uint8_t> data = argTransformer.parseHexList(hexString);

    if (canService.sendFrame(id, data)) {
        terminalView.println("\nCAN发送: ✅ 帧数据已发送至0x" + argTransformer.toHex(id, 3)); // 汉化
    } else {
        terminalView.println("\nCAN发送: ❌ 发送帧数据至0x" + argTransformer.toHex(id, 3) + "失败"); // 汉化
    }
}

/*
Receive CAN frames with filtering by frame ID
*/
void CanController::handleReceive(const TerminalCommand& cmd) {
    terminalView.println("CAN接收: 按ID过滤"); // 汉化

    int id;
    if (!cmd.getSubcommand().empty() && argTransformer.isValidNumber(cmd.getSubcommand())) {
        id = argTransformer.parseHexOrDec16(cmd.getSubcommand());
    } else {
        // Ask user for ID
        id = userInputManager.readValidatedCanId("Filter CAN ID", 0x123);
    }
    
    // Check max value allowed
    if (id > 0x7FF) {
        terminalView.println("\n❌ 仅支持11位标准ID."); // 汉化
        return;
    }

    // Filter by ID
    canService.setFilter(id);
    
    // Flush internal buffer
    canService.flush();

    terminalView.println("等待ID为0x" + argTransformer.toHex(id, 3) + "的CAN帧数据... 按下[ENTER]停止.\n"); // 汉化

    unsigned long lastFrameTime = millis();
    while (true) {
        std::string frameStr = canService.readFrameAsString();

        // Received frame
        if (!frameStr.empty()) {
            terminalView.println(" 📥 " + frameStr);
            lastFrameTime = millis();  // reset timer
        }

        // Reset CAN if no frame for 3 seconds
        if (millis() - lastFrameTime > 3000) {
            canService.reset();
            lastFrameTime = millis();
        }

        // Abort if ENTER is pressed
        char ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            terminalView.println("\nCAN接收: 已被用户停止."); // 汉化
            break;
        }
    }

    // Reset filter
    canService.reset();
}

/*
Help message for CAN commands
*/
void CanController::handleHelp() {
    terminalView.println("可用的CAN命令:"); // 汉化
    terminalView.println("  sniff");
    terminalView.println("  send [id]");
    terminalView.println("  receive [id]");
    terminalView.println("  status");
    terminalView.println("  config");
}

/*
Configure the CAN controller
*/
void CanController::handleConfig() {

    terminalView.println("CAN配置:"); // 汉化
    terminalView.println("\n请确保使用的是MCP2515 CAN模块.\n"); // 汉化
    
    const auto& forbidden = state.getProtectedPins();

    // CS pin is fixed, no need to configure
    uint8_t cs = state.getCanCspin();
    terminalView.print("MCP2515 CS引脚已固定为: " + std::to_string(cs)); // 汉化
    terminalInput.waitPress();
    terminalView.println("");

    // Configure SCK
    uint8_t sck = userInputManager.readValidatedPinNumber("MCP2515 SCK pin", state.getCanSckPin(), forbidden);
    state.setCanSckPin(sck);

    // Configure SI (MOSI)
    uint8_t si = userInputManager.readValidatedPinNumber("MCP2515 SI (MOSI) pin", state.getCanSiPin(), forbidden);
    state.setCanSiPin(si);

    // Configure SO (MISO)
    uint8_t so = userInputManager.readValidatedPinNumber("MCP2515 SO (MISO) pin", state.getCanSoPin(), forbidden);
    state.setCanSoPin(so);

    // Configure bitrate
    uint32_t kbps = userInputManager.readValidatedUint32("Speed in kbps", state.getCanKbps());
    uint32_t adjusted = canService.closestSupportedBitrate(kbps);
    state.setCanKbps(adjusted);
    if (adjusted != kbps) {
        terminalView.println("⚠️ 请求的比特率" + std::to_string(kbps) + " kbps不受支持. 改用" + std::to_string(adjusted) + " kbps."); // 汉化
    }

    // Apply configuration
    canService.configure(cs, sck, so, si, kbps);

    // Test MCP2515 responsiveness
    auto probeOk = canService.probe();
    if (!probeOk) {
        terminalView.println("\n ❌ MCP2515 CAN配置失败. 请检查接线.\n"); // 汉化
        return;
    }
    terminalView.println("\n ✅ MCP2515 CAN已配置完成.\n"); // 汉化
}

/*
Ensure CAN is configured before any operation
*/
void CanController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // Always reapply config in case pins were reassigned elsewhere
    canService.configure(
        state.getCanCspin(),
        state.getCanSckPin(),
        state.getCanSoPin(),
        state.getCanSiPin(),
        state.getCanKbps()
    );
}