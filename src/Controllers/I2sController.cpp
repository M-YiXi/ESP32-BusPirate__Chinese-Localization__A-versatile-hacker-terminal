#include "I2sController.h"

/*
Constructor
*/
I2sController::I2sController(ITerminalView& terminalView, IInput& terminalInput,
                             I2sService& i2sService, ArgTransformer& argTransformer,
                             UserInputManager& userInputManager)
    : terminalView(terminalView), terminalInput(terminalInput),
      i2sService(i2sService), argTransformer(argTransformer),
      userInputManager(userInputManager) {}

void I2sController::handleCommand(const TerminalCommand& cmd) {
    if (cmd.getRoot() == "config") {
        handleConfig();
    } else if (cmd.getRoot() == "play") {
        handlePlay(cmd);
    } else if (cmd.getRoot() == "record") {
        handleRecord(cmd);
    } else if (cmd.getRoot() == "test") {
        handleTest(cmd);
    } else if (cmd.getRoot() == "reset") {
        handleReset();
    } else {
        handleHelp();
    }
}


/*
Play
*/
void I2sController::handlePlay(const TerminalCommand& cmd) {
    auto args = argTransformer.splitArgs(cmd.getArgs());

    if (!argTransformer.isValidNumber(cmd.getSubcommand())) {
        terminalView.println("使用方法: play <频率> [持续时间毫秒]"); // 汉化
        return;
    }

    uint16_t freq = argTransformer.parseHexOrDec32(cmd.getSubcommand());

    // Infinite until ENTER press
    if (args.empty()) {
        terminalView.println("\nI2S播放: 音调 @ " + std::to_string(freq) + " Hz (按下[ENTER]停止)...\n"); // 汉化

        i2sService.playToneInterruptible(state.getI2sSampleRate(), freq, 0xFFFF, [&]() -> bool {
            char ch = terminalInput.readChar();
            return ch == '\n' || ch == '\r';
        });
    
    // Duration or until ENTER press
    } else if (args.size() == 1 && argTransformer.isValidNumber(args[0])) {
        uint16_t duration = argTransformer.parseHexOrDec32(args[0]);

        terminalView.println("\nI2S播放: 音调 @ " + std::to_string(freq) + " Hz 持续 " + std::to_string(duration) + " 毫秒 (或按下[ENTER]提前停止)...\n"); // 汉化

        i2sService.playToneInterruptible(state.getI2sSampleRate(), freq, duration, [&]() -> bool {
            char ch = terminalInput.readChar();
            return ch == '\n' || ch == '\r';
        });

    } else {
        terminalView.println("使用方法: play <频率> [持续时间毫秒]"); // 汉化
        return;
    }

    terminalView.println("I2S播放: 完成."); // 汉化
}

/*
Record
*/
void I2sController::handleRecord(const TerminalCommand& cmd) {
    terminalView.println("I2S录音: 正在进行... 按下[Enter]停止.\n"); // 汉化

    // Configure input
    i2sService.configureInput(
        state.getI2sBclkPin(),
        state.getI2sLrckPin(),
        state.getI2sDataPin(),
        state.getI2sSampleRate(),
        state.getI2sBitsPerSample()
    );

    constexpr size_t batchSize = 2048;
    constexpr size_t groupCount = 16;
    std::vector<int16_t> buffer(batchSize);

    int16_t dynamicMax = 5000; // initial value

    while (true) {
        size_t samplesRead = i2sService.recordSamples(buffer.data(), batchSize);
        size_t samplesPerGroup = samplesRead / groupCount;

        // Find peak on the batch
        int16_t batchPeak = 0;
        for (size_t i = 0; i < samplesRead; ++i) {
            int16_t val = abs(buffer[i]);
            if (val > batchPeak) batchPeak = val;
        }

        // Progressive update
        if (batchPeak > dynamicMax) dynamicMax = batchPeak;
        else dynamicMax = (dynamicMax * 9 + batchPeak) / 10;

        std::string line;
        for (size_t g = 0; g < groupCount; ++g) {
            int16_t peak = 0;
            for (size_t i = 0; i < samplesPerGroup; ++i) {
                size_t idx = g * samplesPerGroup + i;
                int16_t val = abs(buffer[idx]);
                if (val > peak) peak = val;
            }

            int level = (peak * 100) / (dynamicMax == 0 ? 1 : dynamicMax);
            if (level > 100) level = 100;
            if (level < 0) level = 0;

            char buf[6];
            sprintf(buf, "%03d ", level);
            line += buf;
        }

        terminalView.println(line);

        char ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') break;
    }

    // Reconfigure output
    i2sService.configureOutput(
        state.getI2sBclkPin(),
        state.getI2sLrckPin(),
        state.getI2sDataPin(),
        state.getI2sSampleRate(),
        state.getI2sBitsPerSample()
    );

    terminalView.println("\nI2S录音: 已被用户停止.\n"); // 汉化
}

/*
Test
*/
void I2sController::handleTest(const TerminalCommand& cmd) {
    std::string mode = cmd.getSubcommand();

    if (mode.empty()) {
        terminalView.println("使用方法: test <扬声器|麦克风>"); // 汉化
        return;
    }

    if (mode[0] == 's') {
        handleTestSpeaker();
    }
    else if (mode[0] == 'm') {
        handleTestMic();
    }
    else {
        terminalView.println("使用方法: test <扬声器|麦克风>"); // 汉化
    }
}

/*
Test Speaker
*/
void I2sController::handleTestSpeaker() {
    terminalView.println("I2S扬声器测试: 运行完整测试...\n"); // 汉化

    // Show pin config
    terminalView.println("使用引脚:"); // 汉化
    terminalView.println("  BCLK : " + std::to_string(state.getI2sBclkPin()));
    terminalView.println("  LRCK : " + std::to_string(state.getI2sLrckPin()));
    terminalView.println("  DATA : " + std::to_string(state.getI2sDataPin()));
    terminalView.println("");

    auto rate = state.getI2sSampleRate();

    // Melody
    terminalView.println("  播放旋律..."); // 汉化
    const uint16_t melody[] = {262, 294, 330, 349, 392, 440, 494, 523}; // C major
    for (uint16_t f : melody) {
        i2sService.playTone(rate, f, 200);
        delay(50);
    }
    delay(1000);

    // Frequency Sweep
    terminalView.println("  频率扫描..."); // 汉化
    for (uint16_t f = 100; f <= 3000; f += 300) {
        i2sService.playTone(rate, f, 100);
    }
    delay(800);

    // Low Freq
    terminalView.println("  低频测试..."); // 汉化
    for (uint16_t f : {50, 100, 150, 200, 250, 300, 350, 400, 450, 500}) {
        i2sService.playTone(rate, f, 400);
        delay(100);
    }
    delay(800);

    // High Freq
    terminalView.println("  高频测试..."); // 汉化
    for (uint16_t f = 10000; f <= 16000; f += 1000) {
        i2sService.playTone(rate, f, 300);
        delay(100);
    }
    delay(800);

    // Beep Pattern
    terminalView.println("  提示音模式(短/长)..."); // 汉化
    i2sService.playTone(rate, 800, 100);
    delay(100);
    i2sService.playTone(rate, 800, 100);
    delay(100);
    i2sService.playTone(rate, 800, 100);
    delay(100);
    i2sService.playTone(rate, 800, 400);
    delay(100);
    i2sService.playTone(rate, 800, 400);
    delay(100);
    i2sService.playTone(rate, 800, 400);
    delay(800);

    // Binary pattern (square wave)
    terminalView.println("  二进制音调模式..."); // 汉化
    for (int i = 0; i < 15; ++i) {
        i2sService.playTone(rate, 1000, 50);
        delay(50);
    }
    delay(800);

    // Config for PCM playback
    i2sService.configureOutput(
        state.getI2sBclkPin(),
        state.getI2sLrckPin(),
        state.getI2sDataPin(),
        12000,
        16
    );
    
    // PCM Playback test
    terminalView.println("  PCM音频播放..."); // 汉化
    i2sService.playPcm(PcmSoundtestComplete, sizeof(PcmSoundtestComplete));
    
    // Restaure config
    i2sService.configureOutput(
        state.getI2sBclkPin(),
        state.getI2sLrckPin(),
        state.getI2sDataPin(),
        state.getI2sSampleRate(),
        state.getI2sBitsPerSample()
    );
    
    terminalView.println("\nI2S扬声器测试: 完成."); // 汉化
}

/*
Test Mic
*/
void I2sController::handleTestMic() {
    terminalView.println("\nI2S麦克风: 分析输入信号...\n"); // 汉化

    i2sService.configureInput(
        state.getI2sBclkPin(),
        state.getI2sLrckPin(),
        state.getI2sDataPin(),
        state.getI2sSampleRate(),
        state.getI2sBitsPerSample()
    );

    // Show pin config
    terminalView.println("使用引脚:"); // 汉化
    terminalView.println("  BCLK : " + std::to_string(state.getI2sBclkPin()));
    terminalView.println("  LRCK : " + std::to_string(state.getI2sLrckPin()));
    terminalView.println("  DATA : " + std::to_string(state.getI2sDataPin()));
    terminalView.println("");

    constexpr size_t sampleCount = 4096;
    std::vector<int16_t> buffer(sampleCount);
    size_t read = i2sService.recordSamples(buffer.data(), sampleCount);

    if (read == 0) {
        terminalView.println("\nI2S麦克风: 读取I2S麦克风数据失败."); // 汉化
        return;
    }

    int32_t sum = 0;
    int16_t minVal = INT16_MAX;
    int16_t maxVal = INT16_MIN;

    for (size_t i = 0; i < read; ++i) {
        int16_t val = buffer[i];
        sum += abs(val);
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
    }

    int avgAmplitude = sum / read;
    int peakToPeak = maxVal - minVal;

    // Score
    std::string verdict;
    if (avgAmplitude < 30 || peakToPeak < 60) {
        verdict = "检测到弱信号或无信号"; // 汉化
    } else if (avgAmplitude > 200 && peakToPeak > 400) {
        verdict = "检测到强且有效的信号"; // 汉化
    } else {
        verdict = "信号强度低，可能音量太小？"; // 汉化
    }

    // Display summay
    terminalView.println("汇总信息:"); // 汉化
    terminalView.println("  平均振幅 : " + std::to_string(avgAmplitude)); // 汉化
    terminalView.println("  最小值     : " + std::to_string(minVal)); // 汉化
    terminalView.println("  最大值     : " + std::to_string(maxVal)); // 汉化
    terminalView.println("  峰峰值     : " + std::to_string(peakToPeak)); // 汉化
    terminalView.println("  结论       : " + verdict); // 汉化

    // Reconfig output
    i2sService.configureOutput(
        state.getI2sBclkPin(),
        state.getI2sLrckPin(),
        state.getI2sDataPin(),
        state.getI2sSampleRate(),
        state.getI2sBitsPerSample()
    );

    terminalView.println("\nI2S麦克风: 完成."); // 汉化
}

/*
Config
*/
void I2sController::handleConfig() {
    terminalView.println("I2S配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t bclk = userInputManager.readValidatedPinNumber("BCLK引脚", state.getI2sBclkPin(), forbidden); // 汉化
    state.setI2sBclkPin(bclk);

    uint8_t lrck = userInputManager.readValidatedPinNumber("LRCK/WS引脚", state.getI2sLrckPin(), forbidden); // 汉化
    state.setI2sLrckPin(lrck);

    uint8_t data = userInputManager.readValidatedPinNumber("DATA引脚", state.getI2sDataPin(), forbidden); // 汉化
    state.setI2sDataPin(data);

    uint32_t freq = userInputManager.readValidatedUint32("采样率(例如 44100)", state.getI2sSampleRate()); // 汉化
    state.setI2sSampleRate(freq);

    uint8_t bits = userInputManager.readValidatedUint8("每个采样的位数(例如 16)", state.getI2sBitsPerSample()); // 汉化
    state.setI2sBitsPerSample(bits);

    #if defined(DEVICE_TEMBEDS3) || defined(DEVICE_TEMBEDS3CC1101)
        terminalView.println("\n[警告] 由于内部引脚冲突，I2S在T-Embed设备上可能无法正常工作。"); // 汉化
        terminalView.println("          这包括与显示屏共用的SPI引脚。请谨慎使用。"); // 汉化
        terminalView.println("          设备可能会在该提示后出现卡死情况。\n"); // 汉化
    #endif

    i2sService.configureOutput(bclk, lrck, data, freq, bits);

    terminalView.println("I2S已配置完成.\n"); // 汉化
}


/*
Help
*/
void I2sController::handleHelp() {
    terminalView.println("可用的I2S命令:"); // 汉化
    terminalView.println("  play <频率> [持续时间]"); // 汉化
    terminalView.println("  record ");
    terminalView.println("  test <扬声器|麦克风>"); // 汉化
    terminalView.println("  reset");
    terminalView.println("  config");
}


/*
Reset
*/
void I2sController::handleReset() {
    i2sService.end();

    // Config output
    i2sService.configureOutput(state.getI2sBclkPin(),
                         state.getI2sLrckPin(),
                         state.getI2sDataPin(),
                         state.getI2sSampleRate(),
                         state.getI2sBitsPerSample());

    terminalView.println("I2S重置: 已切换为TX(输出)模式."); // 汉化
}


/*
Ensure configuration
*/
void I2sController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    } else {
        // Reapply
        i2sService.end();
        i2sService.configureOutput(state.getI2sBclkPin(), state.getI2sLrckPin(),
                             state.getI2sDataPin(), state.getI2sSampleRate(),
                             state.getI2sBitsPerSample());
    }
}