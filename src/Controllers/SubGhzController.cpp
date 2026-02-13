#include "SubGhzController.h"

/*
Entry point for commands
*/
void SubGhzController::handleCommand(const TerminalCommand& cmd) {
    const std::string root = cmd.getRoot();

    if (root == "sniff")             handleSniff(cmd);
    else if (root == "scan")         handleScan(cmd);
    else if (root == "sweep")        handleSweep();
    else if (root == "setfrequency") handleSetFrequency();
    else if (root == "setfreq")      handleSetFrequency();
    else if (root == "replay")       handleReplay(cmd);
    else if (root == "jam")          handleJam(cmd);
    else if (root == "bruteforce")   handleBruteforce();
    else if (root == "decode")       handleDecode(cmd);
    else if (root == "trace")        handleTrace();
    else if (root == "listen")       handleListen();
    else if (root == "load")         handleLoad();
    else if (root == "config")       handleConfig();
    else                             handleHelp();
}

/*
Sniff for signals
*/
void SubGhzController::handleSniff(const TerminalCommand&) {
    float f = state.getSubGhzFrequency();
    uint32_t count = 0;

    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: 未检测到模块。请先执行'config'命令。"); // 汉化
        return;
    }
    
    terminalView.println("SUBGHZ 嗅探: 频率 @ " + std::to_string(f) + " MHz... 按下[ENTER]停止\n"); // 汉化
    
    subGhzService.startRawSniffer(state.getSubGhzGdoPin());
    while (true) {
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') {
            break;
        }

        auto [line, pulseCount] = subGhzService.readRawPulses();
        if (pulseCount > 8) { // ignore too short frames, likely noise
            count += pulseCount;
            terminalView.println(line);
        }

        if (subGhzService.isSnifferOverflowing()) {
            terminalView.println("\n[警告] SUBGHZ 嗅探器: 检测到缓冲区溢出！正在清空缓冲区...\n"); // 汉化
            subGhzService.drainSniffer();
        }
    }
    subGhzService.stopRawSniffer();

    terminalView.println("\nSUBGHZ 嗅探: 已被用户停止。共捕获 " + std::to_string(count) + " 个脉冲\n"); // 汉化
}

/*
Scan for frequencies
*/
void SubGhzController::handleScan(const TerminalCommand& cmd) {
    const auto& args = cmd.getArgs();
    auto bands = subGhzService.getSupportedBand();
    
    // Band selection
    auto bandIndex = userInputManager.readValidatedChoiceIndex("选择频段：", bands, 0); // 汉化
    subGhzService.setScanBand(bands[bandIndex]);
    std::vector<float> freqs = subGhzService.getSupportedFreq(bands[bandIndex]);

    // RSSI threshold / hold time selection
    int holdMs = userInputManager.readValidatedInt("输入每个频率的驻留时间（毫秒）：", 4, 1, 5000); // 汉化
    int rssiThr = userInputManager.readValidatedInt("输入RSSI检测阈值（dBm）：", -67, -127, 0); // 汉化

    // Prepare the scan
    if (!subGhzService.applyScanProfile(4.8f, 200.0f, 2 /* OOK */, true)) {
        terminalView.println("SUBGHZ: 未检测到模块。请先执行'config'命令。"); // 汉化
        return;
    }

    terminalView.println("SUBGHZ 扫描: 已启动。驻留时间=" + std::to_string(holdMs) +
                         " 毫秒, 阈值=" + std::to_string(rssiThr) + " dBm.... 按下[ENTER]停止。\n"); // 汉化

    std::vector<int>  best(freqs.size(), -127);
    std::vector<bool> wasAbove(freqs.size(), false);
    bool stopRequested = false;

    // Scanning
    while (!stopRequested) {
        for (size_t i = 0; i < freqs.size(); ++i) {
            // User enter press
            int c = terminalInput.readChar();
            if (c == '\n' || c == '\r') { stopRequested = true; break; }

            // Set the freq
            float f = freqs[i];
            subGhzService.tune(f);

            // Measure peak on freq for ms
            int peak = subGhzService.measurePeakRssi(holdMs);
            if (peak > best[i]) best[i] = peak;

            // Log spike if any
            if (peak >= rssiThr && !wasAbove[i]) {
                terminalView.println(" [峰值] 频率=" + argTransformer.toFixed2(f) + " MHz  RSSI=" + std::to_string(peak) + " dBm"); // 汉化
                wasAbove[i] = true;
            } else if (peak < rssiThr - 2) {
                wasAbove[i] = false;
            }
        }
    }

    // Summary
    std::vector<size_t> idx(freqs.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b){ return best[a] > best[b]; });
    terminalView.println("\n [扫描结果] 最强峰值："); // 汉化
    const size_t n = std::min<size_t>(5, idx.size());
    for (size_t k = 0; k < n; ++k) {
        size_t i = idx[k];
        terminalView.println("   " + argTransformer.toFixed2(freqs[i]) + " MHz  RSSI=" + std::to_string(best[i]) + " dBm");
    }

    // Ask to save best frequency if above threshold
    if (!idx.empty() && best[idx[0]] > -120) {
        auto confirm = userInputManager.readYesNo(" 是否将调谐频率保存为最强频率（" + argTransformer.toFixed2(freqs[idx[0]]) + " MHz）？", true); // 汉化
        if (!confirm) return;
        subGhzService.tune(freqs[idx[0]]);
        terminalView.println(" [频率] 保存到配置：" + argTransformer.toFixed2(freqs[idx[0]]) + " MHz\n"); // 汉化
        state.setSubGhzFrequency(freqs[idx[0]]);
    } else {
        subGhzService.tune(state.getSubGhzFrequency());
    }
}

/*
Set frequency
*/
void SubGhzController::handleSetFrequency() {
    terminalView.println("");
    terminalView.println("选择SubGHz频率："); // 汉化

    // Band selection
    auto bands = subGhzService.getSupportedBand();
    std::vector<std::string> displayBands = bands;
    if (!displayBands.empty()) displayBands[0] = " 自定义频率"; // 汉化
    int bandIndex = userInputManager.readValidatedChoiceIndex("频段", displayBands, 0); // 汉化

    // Custom frequency
    if (bandIndex == 0) {
        float mhz = userInputManager.readValidatedFloat("输入自定义频率（MHz）：", state.getSubGhzFrequency(), 0.0f, 1000.0f); // 汉化
        state.setSubGhzFrequency(mhz);
        subGhzService.tune(mhz);
        terminalView.println("SUBGHZ: 频率已修改为 " + argTransformer.toFixed2(mhz) + " MHz\n"); // 汉化
        return;
    }

    // Choose in predefined frequencies
    subGhzService.setScanBand(bands[bandIndex]);
    std::vector<float> freqs = subGhzService.getSupportedFreq(bands[bandIndex]);
    int index = userInputManager.readValidatedChoiceIndex("可选频率", freqs, 0); // 汉化
    float selected = freqs[index];

    // Apply
    state.setSubGhzFrequency(selected);
    subGhzService.tune(selected);

    terminalView.println("SUBGHZ: 频率已修改为 " + argTransformer.toFixed2(selected) + " MHz\n"); // 汉化
}

/*
Replay
*/
void SubGhzController::handleReplay(const TerminalCommand&) {
    const float f = state.getSubGhzFrequency();

    // Gap between each frame for emmitting
    auto gap = userInputManager.readValidatedInt("帧间隔（毫秒）：", 100, 0, 10000); // 汉化

    // Set profile to read frames
    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: 未检测到模块。请先执行'config'命令。"); // 汉化
        return;
    }

    // Start sniffer
    if (!subGhzService.startRawSniffer(state.getSubGhzGdoPin())) {
        terminalView.println("SUBGHZ: 启动原始嗅探器失败。"); // 汉化
        return;
    }

    terminalView.println("SUBGHZ 重放: 正在录制最多64帧 @ " +
                         std::to_string(f) + " MHz... 按下[ENTER]停止。\n"); // 汉化

    std::vector<std::vector<rmt_item32_t>> frames;
    frames.reserve(64);

    bool stop = false;
    while (!stop && frames.size() < 64) {
        // Cancel
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') { stop = true; break; }

        // Read frame
        auto items = subGhzService.readRawFrame();
        if (items.size() < 5) { // Most likely noise
            continue;
        }
        frames.push_back(std::move(items));
        terminalView.println(" [已捕获第 " + std::to_string(frames.size()) + " 帧]"); // 汉化

        if (subGhzService.isSnifferOverflowing()) {
            terminalView.println("\n[警告] SUBGHZ 嗅探器: 检测到缓冲区溢出！正在清空缓冲区...\n"); // 汉化
            subGhzService.drainSniffer();
        }
    }

    subGhzService.stopRawSniffer();
    terminalView.println("\nSUBGHZ: 共捕获 " + std::to_string(frames.size()) + " 帧数据。"); // 汉化

    if (frames.empty()) {
        terminalView.println("SUBGHZ: 无数据可重放。\n"); // 汉化
        return;
    }

    // Profil TX + sender
    if (!subGhzService.applyRawSendProfile(f)) {
        terminalView.println("SUBGHZ: 应用TX配置文件失败。"); // 汉化
        return;
    }

    // Start sending frames
    terminalView.print("SUBGHZ: 正在重放数据...\r\n"); // 汉化
    bool okAll = true;
    auto gdo = state.getSubGhzGdoPin();
    bool confirm = true;
    while (confirm) {
        for (size_t i = 0; i < frames.size(); ++i) {
            if (!subGhzService.sendRawFrame(gdo, frames[i])) {
                terminalView.println(" ❌ 第 " + std::to_string(i + 1) + " 帧发送失败"); // 汉化
                okAll = false;
                break;
            } else {
                terminalView.println(" ✅ 第 " + std::to_string(i + 1) + " 帧发送成功 ... (" + std::to_string(gap) + "毫秒间隔) "); // 汉化
            }
            delay(gap); // inter frame gap
        }
        confirm = userInputManager.readYesNo("SUBGHZ: 重放完成。是否再次重放？", true); // 汉化
    }

    terminalView.println(okAll ? "SUBGHZ: 重放完成，无错误。\n" : "SUBGHZ: 重放完成，存在错误。\n"); // 汉化
    subGhzService.stopTxBitBang(); // ensure stopped
}

/*
Jam
*/
void SubGhzController::handleJam(const TerminalCommand&) {
    auto confirm = userInputManager.readYesNo("\nSUBGHZ 干扰: 该操作将发送随机信号。是否继续？", false); // 汉化
    if (!confirm) return;

    confirm = userInputManager.readYesNo("是否干扰多个频率？", true); // 汉化
    if (confirm) {
        handleBandJam();
        return;
    }

    float f = userInputManager.readValidatedFloat("输入要干扰的频率（MHz）", state.getSubGhzFrequency(), 0.0f, 1000.0f); // 汉化
    
    // Apply TX profile with freq
    if (!subGhzService.applyRawSendProfile(f)) {
        terminalView.println("在 " + argTransformer.toFixed2(f) + " MHz 频率下应用TX配置文件失败"); // 汉化
        return;
    }

    terminalView.println("SUBGHZ 干扰: 正在运行 @ " + argTransformer.toFixed2(f) + " MHz... 按下[ENTER]停止。"); // 汉化
    delay(5); // let display the message
    
    auto gdo = state.getSubGhzGdoPin();
    subGhzService.startTxBitBang();
    while (true) {
        // Stop
        auto c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Jam
        pinService.setHigh(gdo);
        delayMicroseconds(30);
        pinService.setLow(gdo);
    }

    subGhzService.stopTxBitBang();
    terminalView.println("SUBGHZ 干扰: 已被用户停止。\n"); // 汉化
}

/*
Band Jam
*/
void SubGhzController::handleBandJam() {
    // Select band
    auto bands = subGhzService.getSupportedBand();
    std::vector<std::string> bandsWithCustom = bands;

    int bandIndex = userInputManager.readValidatedChoiceIndex(
        "选择频段：", bandsWithCustom, 0 // 汉化
    );

    // Set band and get freqs
    subGhzService.setScanBand(bands[bandIndex]);
    auto freqs = subGhzService.getSupportedFreq(bands[bandIndex]);

    // Prompt for dwell and gap
    int dwellMs = userInputManager.readValidatedInt("每个频率的驻留时间（毫秒）：", 5, 1, 10000); // 汉化
    int gapUs   = userInputManager.readValidatedInt("脉冲间隔（微秒）：",      1, 0, 500000); // 汉化

    const uint8_t gdo = state.getSubGhzGdoPin();

    terminalView.println("\nSUBGHZ 干扰: 正在运行... 按下[ENTER]停止。"); // 汉化
    terminalView.println("频段: " + bands[bandIndex] + ", 频率数量=" + std::to_string(freqs.size()) +
                         ", 驻留时间=" + std::to_string(dwellMs) + " 毫秒\n"); // 汉化

    bool stop = false;
    subGhzService.startTxBitBang();

    while (!stop) {
        for (size_t i = 0; i < freqs.size() && !stop; ++i) {
            // Cancel
            char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') { stop = true; break; }

            float f = freqs[i];
            // Apply TX profile with freq
            if (!subGhzService.applyRawSendProfile(f)) {
                terminalView.println("在 " + argTransformer.toFixed2(f) + " MHz 频率下应用TX配置文件失败"); // 汉化
                subGhzService.stopTxBitBang();
                return;
            }

            unsigned long t0 = millis();
            while (!stop && (millis() - t0 < static_cast<unsigned long>(dwellMs))) {
                char c2 = terminalInput.readChar();
                if (c2 == '\n' || c2 == '\r') { stop = true; break; }

                // Random burst
                if (!subGhzService.sendRandomBurst(gdo)) {
                    terminalView.println("在 " + argTransformer.toFixed2(f) + " MHz 频率下发送失败"); // 汉化
                    break;
                }

                // Jam
                for (int i = 0; i < 64; i++) {
                    pinService.setHigh(gdo);
                    delayMicroseconds(30);
                    pinService.setLow(gdo);
                }

                // Gap
                int remain = gapUs;
                while (remain > 0 && !stop) {
                    char c3 = terminalInput.readChar();
                    if (c3 == '\n' || c3 == '\r') { stop = true; break; }
                    int chunk = std::min(remain, 1000);
                    delayMicroseconds(chunk);
                    remain -= chunk;
                }
            }
        }
    }

    subGhzService.stopTxBitBang();
    subGhzService.tune(state.getSubGhzFrequency());
    terminalView.println("SUBGHZ 干扰: 已被用户停止。\n"); // 汉化
}

/*
Decode
*/
void SubGhzController::handleDecode(const TerminalCommand&) {
    float f = state.getSubGhzFrequency();

    // Sniff profil
    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: 未检测到模块。请先执行'config'命令。"); // 汉化
        return;
    }

    // Start sniffing
    if (!subGhzService.startRawSniffer(state.getSubGhzGdoPin())) {
        terminalView.println("\n启动原始嗅探器失败。\n"); // 汉化
        return;
    }

    terminalView.println("SUBGHZ 解码: 正在监听 @ " +
                         std::to_string(f) + " MHz... 按下[ENTER]停止。\n"); // 汉化

    std::vector<rmt_item32_t> frame;
    bool stop = false;
    while (!stop) {
        // Cancel
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') { stop = true; break; }

        // Read and analyze frame
        frame = subGhzService.readRawFrame();
        if (!frame.empty() && frame.size() >= 5) {
            auto result = subGhzAnalyzeManager.analyzeFrame(frame);
            terminalView.println(result);
        }

        if (subGhzService.isSnifferOverflowing()) {
            terminalView.println("\n[警告] SUBGHZ 嗅探器: 检测到缓冲区溢出！正在清空缓冲区...\n"); // 汉化
            subGhzService.drainSniffer();
        }
    }

    subGhzService.stopRawSniffer();
    terminalView.println("SUBGHZ 解码: 已被用户停止。\n"); // 汉化
}

/*
Trace
*/
void SubGhzController::handleTrace() {
    const float f = state.getSubGhzFrequency();

    // Sniff profile
    if (!subGhzService.applySniffProfile(f)) {
        terminalView.println("SUBGHZ: 未检测到模块。请先执行'config'命令。"); // 汉化
        return;
    }

    terminalView.println("\nSUBGHZ 信号追踪: 在ESP32屏幕上显示 " + argTransformer.toFixed2(f) + " MHz 信号... 按下[ENTER]停止。\n"); // 汉化

    const uint8_t gdo = state.getSubGhzGdoPin();
    uint32_t sampleUs = 900;

    // Update device view
    deviceView.clear();
    deviceView.topBar("SubGHz Trace", false, false);

    std::vector<uint8_t> buffer;
    buffer.reserve(240); // screen width default

    unsigned long lastPoll = millis();

    // Samples loop
    while (true) {
        // Cancel
        if (millis() - lastPoll >= 10) {
            lastPoll = millis();
            const char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') {
                terminalView.println("SUBGHZ 信号追踪: 已被用户停止。\n"); // 汉化
                break;
            }
        }

        // Samples
        buffer.push_back(pinService.read(gdo));

        // Render
        if (buffer.size() >= 240) {
            buffer.resize(240);
            deviceView.drawLogicTrace(gdo, buffer, 1);
            buffer.clear();
        }

        delayMicroseconds(sampleUs);
    }
}

/*
Sweep
*/
void SubGhzController::handleSweep() {
    // Select band
    auto bands = subGhzService.getSupportedBand();
    int bandIndex = userInputManager.readValidatedChoiceIndex("选择频段：", bands, 0); // 汉化
    subGhzService.setScanBand(bands[bandIndex]);
    std::vector<float> freqs = subGhzService.getSupportedFreq(bands[bandIndex]);
    if (freqs.empty()) {
        terminalView.println("SUBGHZ 扫频: 所选频段无可用频率。"); // 汉化
        return;
    }

    // Params
    int dwellMs  = userInputManager.readValidatedInt("每个频率的驻留时间（毫秒）", 300, 20, 5000); // 汉化
    int windowMs = userInputManager.readValidatedInt("检测窗口粒度（毫秒）",      20,  5,  200); // 汉化
    int thrDbm   = userInputManager.readValidatedInt("RSSI阈值（dBm）",        -67, -120, 0); // 汉化

    // Scan profile
    if (!subGhzService.applyScanProfile(4.8f, 200.0f, 2 /* OOK */, true)) {
        terminalView.println("SUBGHZ: 未配置。请先执行'config'命令。"); // 汉化
        return;
    }

    terminalView.println("\nSUBGHZ 扫频: " + bands[bandIndex] +
                         " | 驻留时间=" + std::to_string(dwellMs) + " 毫秒" +
                         " | 检测窗口=" + std::to_string(windowMs) + " 毫秒" +
                         " | 阈值=" + std::to_string(thrDbm) + " dBm... 按下[ENTER]停止。\n"); // 汉化
    
    // Sweep and analyze each frequency
    bool run = true;
    while (run) {
        for (size_t i = 0; i < freqs.size() && run; ++i) {
            // Cancel
            char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') { run = false; break; }

            // Tune
            float f = freqs[i];
            subGhzService.tune(f);

            // Analyze
            auto line = subGhzAnalyzeManager.analyzeFrequencyActivity(dwellMs, windowMs, thrDbm,
                // measure(windowMs)
                [&](int winMs){ return subGhzService.measurePeakRssi(winMs); },
                // shouldAbort()
                [&](){
                    char cc = terminalInput.readChar();
                    if (cc == '\n' || cc == '\r') { run = false; return true; }
                    return false;
                },
                /*neighborLeftConf=*/0.f,
                /*neighborRightConf=*/0.f
            );

            // Log result
            terminalView.println("  " + argTransformer.toFixed2(f) + " MHz  " + line);
        }
    }

    terminalView.println("\nSUBGHZ 扫频: 已被用户停止。\n"); // 汉化
}

/*
Load
*/
void SubGhzController::handleLoad() {
    if (!littleFsService.mounted()) {
        littleFsService.begin();
    }

    // List .sub files
    auto files = littleFsService.listFiles(/*root*/ "/", ".sub");
    if (files.empty()) {
        terminalView.println("SUBGHZ: 在LittleFS根目录（'/'）中未找到.sub文件。\n"); // 汉化
        return;
    }

    // Select file
    terminalView.println("\n=== LittleFS中的.sub文件 ==="); // 汉化
    int fileIndex = userInputManager.readValidatedChoiceIndex("文件序号", files, 0); // 汉化
    std::string filename = files[fileIndex];

    // Check size
    int MAX_FILE_SIZE = 32 * 1024; // 32 KB
    auto fileSize = littleFsService.getFileSize("/" + filename);
    if (fileSize == 0 || fileSize > MAX_FILE_SIZE) {
        terminalView.println("\nSUBGHZ: 文件大小无效（>32KB）: " + filename + " (" + std::to_string(fileSize) + " 字节)\n"); // 汉化
        return;
    }
    
    terminalView.println("\nSUBGHZ: 正在加载文件 '" + filename + "' (" + std::to_string(fileSize) + " 字节)..."); // 汉化

    // Load file
    std::string fileContent;
    fileContent.reserve(fileSize + 1);
    if (!littleFsService.readAll("/" + filename, fileContent)) {
        terminalView.println("\nSUBGHZ: 读取文件 " + filename + " 失败\n"); // 汉化
        return;
    }

    // Validate
    if (!subGhzTransformer.isValidSubGhzFile(fileContent)) {
        terminalView.println("\nSUBGHZ: 无效的.sub文件: " + filename + "\n"); // 汉化
        return;
    }

    // Parse
    auto frames = subGhzTransformer.transformFromFileFormat(fileContent);
    if (frames.empty()) {
        terminalView.println("\nSUBGHZ: 解析.sub文件失败: " + filename + "\n"); // 汉化
        return; 
    }

    // Get frame names
    auto summaries = subGhzTransformer.extractSummaries(frames);
    summaries.push_back("退出"); // for exit option 汉化

    while (true) {
        // Select frame to send
        terminalView.println("\n=== 文件 '" + filename + "' 中的命令 ==="); // 汉化
        uint16_t idx = userInputManager.readValidatedChoiceIndex("帧序号", summaries, 0); // 汉化

        // Exit
        if (idx == summaries.size() - 1) {
            terminalView.println("退出命令发送...\n"); // 汉化
            break;
        }

        // Send
        terminalView.println("\n 正在发送第 #" + std::to_string(idx + 1) + " 帧..."); // 汉化
        const auto& cmd = frames[idx];
        if (subGhzService.send(cmd)) {
            terminalView.println(" ✅ " + summaries[idx]);
        } else {
            terminalView.println(" ❌ 第 #" + std::to_string(idx + 1) + " 帧发送失败"); // 汉化
        }
    }
}

/*
Listen
*/
void SubGhzController::handleListen() {
    // Params
    float mhz = userInputManager.readValidatedFloat("输入频率（MHz）：", state.getSubGhzFrequency(), 0.0f, 1000.0f); // 汉化
    int   rssiGate = userInputManager.readValidatedInt("RSSI门限（dBm）：", -65, -127, 0); // 汉化
    state.setSubGhzFrequency(mhz);

    // Radio init
    if (!subGhzService.applySniffProfile(mhz)) {
        terminalView.println("SUBGHZ: 未检测到模块。请先执行'config'命令。"); // 汉化
        return;
    }
    subGhzService.tune(mhz);

    // I2S init with configured pins
    i2sService.configureOutput(
        state.getI2sBclkPin(), state.getI2sLrckPin(), state.getI2sDataPin(),
        state.getI2sSampleRate(), state.getI2sBitsPerSample()
    );

    terminalView.println("\nSUBGHZ: RSSI转音频映射 @ " + argTransformer.toFixed2(mhz) +
                         " MHz... 按下[ENTER]停止。\n"); // 汉化

    terminalView.println("[提示] 使用已配置的I2S引脚进行音频输出。\n"); // 汉化

    // Mapping params
    const uint16_t fMin = 800;      // Hz for weak signals
    const uint16_t fMax = 12000;    // Hz for strong signals
    const uint16_t toneMs = 1;      // short sound
    const uint16_t refreshUs = 200; // 5 kHz update rate

    while (true) {
        // Stop on ENTER
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Get RSSI
        int rssi = subGhzService.measurePeakRssi(1);
        if (rssi >= rssiGate) {
            float norm = (rssi + 120.0f) / 120.0f; // normalize dbm
            if (norm < 0) norm = 0;
            if (norm > 1) norm = 1;
            uint16_t freqHz = fMin + (uint16_t)(norm * (fMax - fMin));

            // Play the tone on I2S configured pins
            i2sService.playTone(state.getI2sSampleRate(), freqHz, toneMs);
        }

        delayMicroseconds(refreshUs);
    }

    terminalView.println("\nSUBGHZ 音频监听: 已被用户停止。\n"); // 汉化
}

/*
Config CC1101
*/
void SubGhzController::handleConfig() {
    terminalView.println("\nSubGHz 配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();    

    // CC1101 pins
    uint8_t sck  = userInputManager.readValidatedPinNumber("CC1101 SCK引脚",  state.getSubGhzSckPin(),  forbidden); // 汉化
    state.setSubGhzSckPin(sck);

    uint8_t miso = userInputManager.readValidatedPinNumber("CC1101 MISO引脚", state.getSubGhzMisoPin(), forbidden); // 汉化
    state.setSubGhzMisoPin(miso);

    uint8_t mosi = userInputManager.readValidatedPinNumber("CC1101 MOSI引脚", state.getSubGhzMosiPin(), forbidden); // 汉化
    state.setSubGhzMosiPin(mosi);

    uint8_t ss   = userInputManager.readValidatedPinNumber("CC1101 SS/CS引脚", state.getSubGhzCsPin(), forbidden); // 汉化
    state.setSubGhzCsPin(ss);

    uint8_t gdo0 = userInputManager.readValidatedPinNumber("CC1101 GDO0引脚", state.getSubGhzGdoPin(), forbidden); // 汉化
    state.setSubGhzGdoPin(gdo0);

    float freq = state.getSubGhzFrequency(); 

    // Configure
    auto isConfigured = subGhzService.configure(
        deviceView.getScreenSpiInstance(),
        sck, miso, mosi, ss,
        gdo0, freq
    );

    // CC1101 feedback
    if (!isConfigured) {
        terminalView.println("\n ❌ 检测CC1101模块失败。请检查接线。\n"); // 汉化
    } else {

        if (state.getTerminalMode() != TerminalTypeEnum::Standalone) {
            terminalView.println("\n[提示] 对于SubGHz功能，建议使用**USB串口**连接。"); // 汉化
            terminalView.println("       USB串口具有更低的延迟和更可靠的日志输出。"); // 汉化
            terminalView.println("       WiFi网页界面可能会引入延迟并丢失脉冲数据。\n"); // 汉化
        }
        
        // Apply settings
        subGhzService.tune(freq);
        subGhzService.applyScanProfile();
        terminalView.println(" ✅ 检测到CC1101模块并使用默认频率完成配置。"); // 汉化
        terminalView.println(" 使用'setfrequency'或'scan'命令修改频率。\n"); // 汉化
        configured = true;
    }
}

void SubGhzController::handleBruteforce() {
    // Adapted From Bruce: https://github.com/pr3y/Bruce
    // Bruteforce attack for 12 Bit SubGHz protocols

    auto gdo0 = state.getSubGhzGdoPin();
    std::vector<std::string> protocolNames(
        std::begin(subghz_protocol_list),
        std::end(subghz_protocol_list)
    );

    // Prompt for protocol index
    auto protocolIndex = userInputManager.readValidatedChoiceIndex("\n选择要暴力破解的协议：", protocolNames, 0); // 汉化
    auto bruteProtocol = protocolNames[protocolIndex];

    // Freq
    float mhz = userInputManager.readValidatedFloat("输入频率（MHz）：", 433.92f, 0.0f, 1000.0f); // 汉化
    state.setSubGhzFrequency(mhz);

    // Profil TX + sender
    if (!subGhzService.applyRawSendProfile(mhz)) {
        terminalView.println("应用TX配置文件失败。"); // 汉化
        return;
    }

    c_rf_protocol protocol;
    int bits = 0;

    // Protocol selection
    if (bruteProtocol == " Nice 12 Bit") {
        protocol = protocol_nice_flo();
        bits = 12;
    } else if (bruteProtocol == " Came 12 Bit") {
        protocol = protocol_came();
        bits = 12;
    } else if (bruteProtocol == " Ansonic 12 Bit") {
        protocol = protocol_ansonic();
        bits = 12;
    } else if (bruteProtocol == " Holtek 12 Bit") {
        protocol = protocol_holtek();
        bits = 12;
    } else if (bruteProtocol == " Linear 12 Bit") {
        protocol = protocol_linear();
        bits = 12;
    } else if (bruteProtocol == " Chamberlain 12 Bit") {
        protocol = protocol_chamberlain();
        bits = 12;
    } else {
        terminalView.println("SUBGHZ 暴力破解: 该协议尚未实现。"); // 汉化
        return;
    }

    // Repeat
    auto bruteRepeats = userInputManager.readValidatedUint8("输入每个码的重复发送次数：", 1); // 汉化
    subGhzService.startTxBitBang();

    // Send all codes
    terminalView.println("SUBGHZ 暴力破解: 正在发送" + bruteProtocol + "协议的所有码值... 按下[ENTER]停止。\n"); // 汉化
    auto count = 0;
    for (int i = 0; i < (1 << bits); ++i) {
        for (int r = 0; r < bruteRepeats; ++r) {
            for (const auto &pulse : protocol.pilot_period) { 
                subGhzService.sendRawPulse(gdo0, pulse); 
            }

            for (int j = bits - 1; j >= 0; --j) {
                bool bit = (i >> j) & 1;
                const std::vector<int> &timings = protocol.transposition_table[bit ? '1' : '0'];
                for (auto duration : timings) { 
                    subGhzService.sendRawPulse(gdo0, duration); 
                }
            }

            for (const auto &pulse : protocol.stop_bit) { subGhzService.sendRawPulse(gdo0, pulse); }
            
        }

        // Display progress
        count++;
        if (count % 100 == 0) {
            terminalView.println(" " + bruteProtocol + " @ " + argTransformer.toFixed2(mhz) + " MHz 已发送 " + std::to_string(count) + " 个码值。"); // 汉化
        }

        // Cancel
        char cc = terminalInput.readChar();
        if (cc == '\n' || cc == '\r') {
            terminalView.println("\nSUBGHZ 暴力破解: 已被用户停止。\n"); // 汉化
            subGhzService.stopTxBitBang();
            return;
        }
    }

    subGhzService.stopTxBitBang();
    terminalView.println("\nSUBGHZ 暴力破解: 完成。\n"); // 汉化
}

/*
Ensure SubGHz is configured
*/
void SubGhzController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    };

    uint8_t cs = state.getSubGhzCsPin();
    uint8_t gdo0 = state.getSubGhzGdoPin();
    uint8_t sck = state.getSubGhzSckPin();
    uint8_t miso = state.getSubGhzMisoPin();
    uint8_t mosi = state.getSubGhzMosiPin();
    float freq = state.getSubGhzFrequency();

    subGhzService.configure(deviceView.getScreenSpiInstance(), sck, miso, mosi, cs, gdo0, freq);
}

/*
Help
*/
void SubGhzController::handleHelp() {
    terminalView.println("SubGHz 命令列表:"); // 汉化
    terminalView.println("  scan");
    terminalView.println("  sweep");
    terminalView.println("  sniff");
    terminalView.println("  decode");
    terminalView.println("  replay");
    terminalView.println("  jam");
    terminalView.println("  bruteforce");
    terminalView.println("  trace");
    terminalView.println("  load");
    terminalView.println("  listen");
    terminalView.println("  setfrequency");
    terminalView.println("  config");
}