#include "InfraredController.h"

/*
Constructor
*/
InfraredController::InfraredController(
    ITerminalView&           view,
    IInput&                  terminalInput,
    InfraredService&         service,
    LittleFsService&         littleFsService,
    ArgTransformer&          argTransformer,
    InfraredRemoteTransformer& infraredRemoteTransformer,
    UserInputManager&        userInputManager,
    UniversalRemoteShell&    universalRemoteShell
)
    : terminalView(view),
      terminalInput(terminalInput),
      infraredService(service),
      littleFsService(littleFsService),
      argTransformer(argTransformer),
      infraredRemoteTransformer(infraredRemoteTransformer),
      userInputManager(userInputManager),
      universalRemoteShell(universalRemoteShell)
{}

/*
Entry point to handle Infrared command
*/
void InfraredController::handleCommand(const TerminalCommand& command) {
    if (command.getRoot() == "config")            handleConfig();
    else if (command.getRoot() == "send")         handleSend(command);
    else if (command.getRoot() == "receive")      handleReceive();
    else if (command.getRoot() == "devicebgone")  handleDeviceBgone();
    else if (command.getRoot() == "remote")       handleRemote();
    else if (command.getRoot() == "replay")       handleReplay(command);
    else if (command.getRoot() == "record")       handleRecord();
    else if (command.getRoot() == "load")         handleLoad(command);
    else if (command.getRoot() == "jam")          handleJam();
    else if (command.getRoot() == "setprotocol")  handleSetProtocol();
    else handleHelp();
}

/*
Send
*/
void InfraredController::handleSend(const TerminalCommand& command) {
    std::istringstream iss(command.getArgs());
    std::string subStr, cmdStr;
    iss >> subStr >> cmdStr;
    auto addrStr = command.getSubcommand();

    if (addrStr.empty() || subStr.empty() || cmdStr.empty()) {
        terminalView.println("参数缺失。使用方法: send <设备> <子设备> <指令>"); // 汉化
        return;
    }

    int device, subdevice, function;
    if (!argTransformer.parseInt(addrStr, device) ||
        !argTransformer.parseInt(subStr, subdevice) ||
        !argTransformer.parseInt(cmdStr, function)) {
        terminalView.println("无效的数字格式。请使用十进制或十六进制。"); // 汉化
        return;
    }

    InfraredCommand infraredCommand;
    infraredCommand.setDevice(device);
    infraredCommand.setSubdevice(subdevice);
    infraredCommand.setFunction(function);
    infraredCommand.setProtocol(state.getInfraredProtocol());

    for (int i = 0; i < 3; ++i) {
        infraredService.sendInfraredCommand(infraredCommand);
        delay(100);
    }

    terminalView.println("红外指令已发送，使用协议: " + InfraredProtocolMapper::toString(state.getInfraredProtocol())); // 汉化
}

/*
Receive
*/
void InfraredController::handleReceive() {
    bool decode = userInputManager.readYesNo("是否解码红外信号?", true); // 汉化

    terminalView.println("红外接收: 等待信号..."); // 汉化
    terminalView.println("按下[ENTER]停止.\n");
    
    infraredService.startReceiver();

    while (true) {
        // Stop on ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\n红外接收: 已被用户停止."); // 汉化
            break;
        }

        if (decode) {
            // Decode signal
            InfraredCommand cmd = infraredService.receiveInfraredCommand();
            if (cmd.getProtocol() != RAW) {
                terminalView.println("");
                terminalView.println("接收到红外信号:"); // 汉化
                terminalView.println("  协议   : " + InfraredProtocolMapper::toString(cmd.getProtocol())); // 汉化
                terminalView.println("  设备   : " + std::to_string(cmd.getDevice())); // 汉化
                terminalView.println("  子设备 : " + std::to_string(cmd.getSubdevice())); // 汉化
                terminalView.println("  指令   : " + std::to_string(cmd.getFunction())); // 汉化
                terminalView.println("");
                terminalView.println("红外接收: 等待下一个信号，或按下[ENTER]退出."); // 汉化
            }
        } else {
            // Raw mode
            std::vector<uint16_t> timings;
            uint32_t khz = 0;

            if (infraredService.receiveRaw(timings, khz)) {
                terminalView.println("\n原始时序: "); // 汉化

                bool mark = true;
                for (uint16_t t : timings) {
                    terminalView.print(mark ? "+" : "-");
                    terminalView.print(std::to_string(t));
                    terminalView.print(" ");
                    mark = !mark;
                }
                terminalView.println("");
            }
        }
    }

    infraredService.stopReceiver();
}

/* 
DeviceBgone
*/
void InfraredController::handleDeviceBgone() {
    terminalView.println("发送Device-B-Gone指令... 按下[ENTER]停止"); // 汉化

    for (const auto& cmdStruct : universalOnOff) {

        // Convert to InfraredCommand model
        auto cmd = InfraredCommand(
            cmdStruct.proto,
            cmdStruct.device,
            cmdStruct.subdevice,
            cmdStruct.function
        );
        
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("红外Device-B-Gone: 已被用户中断."); // 汉化
            return;
        }

        for (int i = 0; i < 2; ++i) { // send 2x per command
            infraredService.sendInfraredCommand(cmd);
            delay(100);
        }

        terminalView.println(
            "已发送开关指令至 协议=" + InfraredProtocolMapper::toString(cmd.getProtocol()) + // 汉化
            " 设备=" + std::to_string(cmd.getDevice()) + // 汉化
            " 子设备=" + std::to_string(cmd.getSubdevice()) + // 汉化
            " 指令=" + std::to_string(cmd.getFunction()) // 汉化
        );
    }

    terminalView.println("Device-B-Gone指令序列执行完成."); // 汉化
}

/*
Universal Remote
*/
void InfraredController::handleRemote() {
    universalRemoteShell.run();
}

/*
Replay
*/
void InfraredController::handleReplay(const TerminalCommand& command) {

    // Optional replay count
    uint32_t replayCount = 0; // 0 = infinite
    const std::string sub = command.getSubcommand();
    if (!sub.empty() && argTransformer.isValidNumber(sub)) {
        replayCount = argTransformer.toUint32(sub);
    }

    // Record frames
    std::vector<IRFrame> tape;
    if (!recordFrames(tape)) {
        return; // No frames captured
    }

    // Playback frames
    playbackFrames(tape, replayCount);
}

void InfraredController::handleRecord() {

    // Mount LittleFS
    if (!littleFsService.mounted()) {
        littleFsService.begin();
        if (!littleFsService.mounted()) {
            terminalView.println("红外录制: LittleFS未挂载. 终止操作."); // 汉化
            return;
        } else {
            terminalView.println("红外录制: LittleFS已挂载."); // 汉化
        }
    }

    // Space check: need at least 8 kb free
    constexpr size_t MIN_FREE_BYTES = 8 * 1024;
    size_t free = littleFsService.freeBytes();
    if (free < MIN_FREE_BYTES) {
        terminalView.println(
            "红外录制: LittleFS空间不足. 需要至少8KB可用空间, 当前仅有 " + // 汉化
            std::to_string(free) + " 字节." // 汉化
        );
        return;
    }

    // Record decoded commands
    std::vector<InfraredFileRemoteCommand> cmds;
    cmds.reserve(64);

    terminalView.println("\n红外录制: 等待红外帧(最多64个)... 按下[ENTER]停止.\n"); // 汉化

    infraredService.startReceiver();

    while (true) {
        // Stop ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\n红外录制: 已停止."); // 汉化
            break;
        }

        if (cmds.size() >= 64) {
            terminalView.println("\n红外录制: 已达到最大64个保存指令上限, 停止录制.\n"); // 汉化
            break;
        }

        InfraredCommand decoded = infraredService.receiveInfraredCommand();

        // ignore RAW / invalid
        if (decoded.getProtocol() == RAW) {
            continue;
        }

        terminalView.println("");
        terminalView.println("接收到红外信号:"); // 汉化
        terminalView.println("  协议   : " + InfraredProtocolMapper::toString(decoded.getProtocol())); // 汉化
        terminalView.println("  设备   : " + std::to_string(decoded.getDevice())); // 汉化
        terminalView.println("  子设备 : " + std::to_string(decoded.getSubdevice())); // 汉化
        terminalView.println("  指令   : " + std::to_string(decoded.getFunction())); // 汉化
        terminalView.println("");

        // Save the command ?
        if (!userInputManager.readYesNo("是否保存该指令?", true)) { // 汉化
            terminalView.println("\n已跳过. 按下[ENTER]停止或等待下一个信号...\n"); // 汉化
            continue;
        }

        // If yes, Ask function name
        std::string defFunc = "cmd_" + std::to_string(cmds.size() + 1);
        std::string funcName = userInputManager.readSanitizedString("输入指令名称", defFunc, false); // 汉化
        if (funcName.empty()) funcName = defFunc;

        // Build cmd
        InfraredFileRemoteCommand cmd;
        cmd.functionName = funcName;
        cmd.protocol     = decoded.getProtocol();

        // Address
        uint8_t device = static_cast<uint8_t>(decoded.getDevice() & 0xFF);
        uint8_t sub    = static_cast<uint8_t>((decoded.getSubdevice() < 0 ? 0 : decoded.getSubdevice()) & 0xFF);
        cmd.address     = (static_cast<uint16_t>(sub) << 8) | device;

        cmd.function    = static_cast<uint8_t>(decoded.getFunction() & 0xFF);

        // Unused for non-RAW
        cmd.rawData = nullptr;
        cmd.rawDataSize = 0;
        cmd.frequency = 0;
        cmd.dutyCycle = 0.0f;

        cmds.push_back(cmd);

        terminalView.println("\n✅ 已保存 '" + funcName + "'\n"); // 汉化
        terminalView.println("红外录制: 等待下一个信号... 按下[ENTER]停止并保存.\n"); // 汉化
    }

    infraredService.stopReceiver();

    if (cmds.empty()) {
        terminalView.println("红外录制: 未保存任何指令.\n"); // 汉化
        return;
    }

    // Ask filename
    std::string defName = "ir_record_" + std::to_string(millis() % 1000000); // court
    std::string fileBase = userInputManager.readSanitizedString("输入文件名", defName, false); // 汉化
    if (fileBase.empty()) fileBase = defName;

    std::string path = "/" + fileBase;
    if (path.size() < 4 || path.substr(path.size() - 3) != ".ir") {
        path += ".ir";
    }

    // Serialize to file format
    std::string text = infraredRemoteTransformer.transformToFileFormat(fileBase, cmds);

    // Write to LittleFS
    if (!littleFsService.write(path, text)) {
        terminalView.println("红外录制: 写入文件失败: " + path); // 汉化
        return;
    }

    terminalView.println("\n✅ 红外录制: 文件已保存: " + path); // 汉化
    terminalView.println("可使用'load'命令或连接Web终端获取该文件.\n"); // 汉化
}

bool InfraredController::recordFrames(std::vector<IRFrame>& tape) {
    tape.clear();
    tape.reserve(MAX_IR_FRAMES);

    terminalView.println("红外重放: 录制原始红外帧(最多64个)... 按下[ENTER]停止.\n"); // 汉化

    // Start the capture
    infraredService.startReceiver();
    uint32_t lastMillis = millis();
    while (true) {
        // Stop if Enter pressed
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') break;

        // Max frames reached
        if (tape.size() >= MAX_IR_FRAMES) {
            terminalView.println("\n红外重放: 已达到最大64个帧上限, 停止录制...\n"); // 汉化
            break;
        }

        // Attempt to capture
        std::vector<uint16_t> timings;
        uint32_t khz = 0;
        if (infraredService.receiveRaw(timings, khz)) {
            const uint32_t now = millis();
            const uint32_t gap = tape.empty() ? 0u : (now - lastMillis);
            lastMillis = now;

            tape.push_back(IRFrame{ std::move(timings), khz, gap });
            terminalView.println(
                "  📥 已捕获帧 #" + std::to_string(tape.size()) + // 汉化
                " (间隔 " + std::to_string(gap) + " 毫秒, 载波 " + std::to_string(khz) + " 千赫兹)" // 汉化
            );
        }
    }
    infraredService.stopReceiver();

    // Nothing
    if (tape.empty()) {
        terminalView.println("红外重放: 未捕获到任何帧. 无内容可重放."); // 汉化
        return false;
    }

    return true;
}

void InfraredController::playbackFrames(const std::vector<IRFrame>& tape, uint32_t replayCount) {
    if (replayCount == 0) {
        terminalView.println("\n红外重放: 按原始延迟重放. 按下[ENTER]停止.\n"); // 汉化
    } else {
        terminalView.println("\n红外重放: 按原始延迟重放 " + std::to_string(replayCount) + // 汉化
                             " 次. 按下[ENTER]停止.\n"); // 汉化
    }

    // Loop through the frames and send them
    uint32_t playedLoops = 0;
    while (true) {
        if (replayCount > 0 && playedLoops >= replayCount) break;

        for (size_t i = 0; i < tape.size(); ++i) {
            const auto& f = tape[i];

            // Check for Enter press and wait for gap
            uint32_t start = millis();
            while (millis() - start < f.gapMs) {
                char c = terminalInput.readChar();
                if (c == '\r' || c == '\n') {
                    terminalView.println("\n红外重放: 已被用户停止."); // 汉化
                    return;
                }
                delay(1);
            }

            // Log and send frame
            terminalView.println(
                "  📤 发送帧 #" + std::to_string(i) + // 汉化
                " (间隔 " + std::to_string(f.gapMs) + " 毫秒, 载波 " + std::to_string(f.khz) + " 千赫兹)" // 汉化
            );
            infraredService.sendRaw(f.timings, f.khz);
        }
        ++playedLoops;
    }

    terminalView.println("\n红外重放: 执行完成 (" + std::to_string(playedLoops) + " 次循环)."); // 汉化
}

/*
Set protocol
*/
void InfraredController::handleSetProtocol() {
    terminalView.println("");
    terminalView.println("选择红外协议:"); // 汉化

    std::vector<InfraredProtocolEnum> protocols;

    for (int i = 0; i <= static_cast<int>(RAW); ++i) {
        InfraredProtocolEnum proto = static_cast<InfraredProtocolEnum>(i);
        std::string name = InfraredProtocolMapper::toString(proto);

        // avoid double name
        if (!name.empty() && 
            std::find_if(protocols.begin(), protocols.end(),
                [proto](InfraredProtocolEnum e) { return InfraredProtocolMapper::toString(e) == InfraredProtocolMapper::toString(proto); }) == protocols.end()) {
            protocols.push_back(proto);
            terminalView.println("  " + std::to_string(protocols.size()) + ". " + name);
        }
    }

    terminalView.println("");
    terminalView.print("协议编号 > "); // 汉化

    std::string inputStr;
    while (true) {
        char c = terminalInput.handler();
        if (c == '\r' || c == '\n') {
            terminalView.println("");
            break;
        }

        if (c == CARDPUTER_SPECIAL_ARROW_DOWN || 
            c == CARDPUTER_SPECIAL_ARROW_UP) {
            terminalView.print(std::string(1, c));
            continue;
        }        

        if (std::isdigit(c)) {
            if (c != CARDPUTER_SPECIAL_ARROW_DOWN || c != CARDPUTER_SPECIAL_ARROW_UP) {
                inputStr += c;
            }
            terminalView.print(std::string(1, c));
        } else {
            terminalView.println("\n无效输入: 仅允许输入数字."); // 汉化
            return;
        }
    }

    if (inputStr.empty()) {
        terminalView.println("未输入任何内容."); // 汉化
        return;
    }

    int index = std::stoi(inputStr);
    if (index >= 1 && index <= static_cast<int>(protocols.size())) {
        InfraredProtocolEnum selected = protocols[index - 1];
        GlobalState::getInstance().setInfraredProtocol(selected);
        terminalView.println("协议已切换为 " + InfraredProtocolMapper::toString(selected)); // 汉化
    } else {
        terminalView.println("无效的协议编号."); // 汉化
    }
}

/*
Load
*/
void InfraredController::handleLoad(TerminalCommand const& command) {
    if (!littleFsService.mounted()) {
        littleFsService.begin();
        return;
    }

    // Get IR files from LittleFS
    auto files = littleFsService.listFiles(/*root*/ "/", ".ir");
    if (files.empty()) {
        terminalView.println("红外: LittleFS根目录('/')下未找到.ir文件."); // 汉化
        return;
    }

    // Select file
    terminalView.println("\n=== LittleFS中的.ir文件 ==="); // 汉化
    uint16_t idxFile = userInputManager.readValidatedChoiceIndex("文件编号", files, 0); // 汉化
    const std::string& chosen = files[idxFile];

    // Check size
    int MAX_FILE_SIZE = 32 * 1024; // 32 KB
    auto fileSize = littleFsService.getFileSize("/" + chosen);
    if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
        terminalView.println("\n红外: 文件大小无效(>32KB): " + chosen); // 汉化
        return;
    }

    // Load file content
    std::string text;
    if (!littleFsService.readAll("/" + chosen, text)) {
        terminalView.println("\n红外: 读取文件失败: " + chosen); // 汉化
        return;
    }

    // Verify format
    if (!infraredRemoteTransformer.isValidInfraredFile(text)) {
        terminalView.println("\n红外: 无法识别的.ir格式或文件为空: " + chosen); // 汉化
        return;
    }

    // Extract commands
    auto cmds = infraredRemoteTransformer.transformFromFileFormat(text);
    if (cmds.empty()) {
        terminalView.println("\n红外: 文件中未找到任何指令: " + chosen); // 汉化
        return;
    }

    // Cmds names
    auto cmdStrings = infraredRemoteTransformer.extractFunctionNames(cmds);
    cmdStrings.push_back("退出文件"); // 汉化 - for exit option

    while (true) {
        // Select command
        terminalView.println("\n=== 文件'" + chosen + "'中的指令 ==="); // 汉化
        uint8_t idxCmd = userInputManager.readValidatedChoiceIndex("指令编号", cmdStrings, 0); // 汉化
        if (idxCmd == cmdStrings.size()-1) {
            terminalView.println("退出指令发送...\n"); // 汉化
            break;
        }

        // Send
        infraredService.sendInfraredFileCommand(cmds[idxCmd]);
        terminalView.println("\n ✅  已发送文件'" + chosen + "'中的指令'" + cmds[idxCmd].functionName + "'"); // 汉化
    }
}

/*
Config
*/
void InfraredController::handleConfig() {
    terminalView.println("\n红外配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t txPin = userInputManager.readValidatedPinNumber("红外TX引脚", state.getInfraredTxPin(), forbidden); // 汉化
    uint8_t rxPin = userInputManager.readValidatedPinNumber("红外RX引脚", state.getInfraredRxPin(), forbidden); // 汉化

    state.setInfraredTxPin(txPin);
    state.setInfraredRxPin(rxPin);
    infraredService.configure(txPin, rxPin);

    // Protocol
    auto selectedProtocol = InfraredProtocolMapper::toString(state.getInfraredProtocol());
    terminalView.println("当前协议: '" + selectedProtocol + "'"); // 汉化
    terminalView.println("可使用'setprotocol'命令修改协议"); // 汉化

    terminalView.println("红外配置完成.\n"); // 汉化
}

/*
Jam
*/
void InfraredController::handleJam() {
    // Mode
    std::vector<std::string> modes = infraredService.getJamModeStrings();
    uint16_t midx = userInputManager.readValidatedChoiceIndex("选择干扰模式", modes, 0); // 汉化

    // kHz
    uint16_t khz = 38;
    if (modes[midx] == "carrier") {
        std::vector<std::string> khzChoices = infraredService.getCarrierStrings();
        uint16_t kidx = userInputManager.readValidatedChoiceIndex("选择载波频率(千赫兹)", khzChoices, 3); // 汉化
        khz = (uint16_t)std::stoi(khzChoices[kidx]);
    }

    // density
    uint8_t density =  userInputManager.readValidatedInt("密度(1-20)", 10, 1, 20); // 汉化

    terminalView.println("\n红外干扰: 发送随机信号..."); // 汉化
    terminalView.println("按下[ENTER]停止."); // 汉化

    uint32_t sweepIdx = 0;
    uint32_t bursts = 0;

    while (true) {
        // Stop ENTER
        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\n红外干扰: 已被用户停止."); // 汉化
            break;
        }

        infraredService.sendJam(midx, khz, sweepIdx, density);
        bursts++;
    }
}

/*
Help
*/
void InfraredController::handleHelp() {
    terminalView.println("未知的红外命令. 使用方法:"); // 汉化
    terminalView.println("  send <地址> <子地址> <指令>"); // 汉化
    terminalView.println("  receive");
    terminalView.println("  setprotocol");
    terminalView.println("  devicebgone");
    terminalView.println("  remote");
    terminalView.println("  replay");
    terminalView.println("  record");
    terminalView.println("  load");
    terminalView.println("  jam");
    terminalView.println("  config");
}

void InfraredController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // Always reconfigure before use
    uint8_t tx = state.getInfraredTxPin();
    uint8_t rx = state.getInfraredRxPin();
    infraredService.configure(tx, rx);
}