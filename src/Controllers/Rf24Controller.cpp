#include "Controllers/Rf24Controller.h"

/*
Entry point for rf24 commands
*/
void Rf24Controller::handleCommand(const TerminalCommand& cmd) {
    const std::string& root = cmd.getRoot();

    if (root == "help") handleHelp();
    else if (root == "config")     handleConfig();
    else if (root == "sniff")      handleSniff();
    else if (root == "scan")       handleScan();
    else if (root == "sweep")      handleSweep();
    else if (root == "jam")        handleJam();
    else if (root == "setchannel") handleSetChannel();
    else                           handleHelp();
}

/*
Ensure NRF24 is configured before use
*/
void Rf24Controller::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    uint8_t ce = state.getRf24CePin();
    uint8_t csn = state.getRf24CsnPin();
    uint8_t sck = state.getRf24SckPin();
    uint8_t miso = state.getRf24MisoPin();
    uint8_t mosi = state.getRf24MosiPin();
    rf24Service.configure(csn, ce, sck, miso, mosi);
}

/*
Sniff
*/
void Rf24Controller::handleSniff() {
    terminalView.println("RF24: 正在嗅探频道 " + std::to_string(rf24Service.getChannel()) + "... 按下[ENTER]停止。\n"); // 汉化

    rf24Service.initRx();
    rf24Service.startListening();
    while (true) {
        auto c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        if (rf24Service.available()) {
            uint8_t tmp[32] = {0};
            if (rf24Service.receive(tmp, sizeof(tmp))) {

                // Display hex and ASCII
                for (int row = 0; row < 32; row += 16) {
                    // Hex
                    for (int i = 0; i < 16; i++) {
                        terminalView.print(argTransformer.toHex(tmp[row + i], 2) + " ");
                    }

                    terminalView.print(" | ");

                    // ASCII
                    for (int i = 0; i < 16; i++) {
                        char c = (tmp[row + i] >= 32 && tmp[row + i] <= 126) ? tmp[row + i] : '.';
                        terminalView.print(std::string(1, c));
                    }

                    terminalView.println("");
                }
            }
        }
    }
    rf24Service.stopListening();
    rf24Service.flushRx();
    terminalView.println("\nRF24: 嗅探已被用户停止。\n"); // 汉化
}

/*
Scan
*/
void Rf24Controller::handleScan() {
    uint32_t dwell = 128; // µs 
    uint8_t levelHold[126 + 1] = {0}; // 0..200
    uint8_t threshold = userInputManager.readValidatedUint8("高阈值 (10..200)？", 20, 10, 200); // 汉化
    int bestCh = -1;
    uint8_t bestVal = 0;
    const uint8_t DECAY = 6;  // decay per sweep
    
    terminalView.println("RF24: 正在扫描频道 0 至 125... 按下[ENTER]停止。\n"); // 汉化
    
    rf24Service.initRx();
    while (true) {
        // Cancel
        char c = terminalInput.readChar();
        if (c == '\n' || c == '\r') break;

        // Sweep
        for (uint8_t ch = 0; ch <= 125; ++ch) {
            rf24Service.setChannel(ch);
            rf24Service.startListening();
            delayMicroseconds(dwell);
            rf24Service.stopListening();

            // Measure
            uint8_t instant = 0;
            if (rf24Service.testRpd())          instant = 200;
            else if (rf24Service.testCarrier()) instant = 120;

            // Decrease hold level
            if (levelHold[ch] > DECAY) levelHold[ch] -= DECAY;
            else levelHold[ch] = 0;

            // Get max
            if (instant > levelHold[ch]) levelHold[ch] = instant;

            if (levelHold[ch] >= threshold) {
                // Save best
                if (levelHold[ch] > bestVal) {
                    bestVal = levelHold[ch];
                    bestCh = ch;
                }

                // Log
                terminalView.println(
                    "  检测到信号: 频道=" + std::to_string(ch) + // 汉化
                    "  频率=" + std::to_string(2400 + ch) + " MHz" + // 汉化
                    "  信号强度=" + std::to_string(levelHold[ch]) // 汉化
                );
            }
        }
    }

    // Result
    terminalView.println("");
    if (bestCh >= 0) {
        // Log best
        terminalView.println(
            "最佳频道: ch=" + std::to_string(bestCh) + // 汉化
            "  频率=" + std::to_string(2400 + bestCh) + " MHz" + // 汉化
            "  峰值强度=" + std::to_string(bestVal) // 汉化
        );

        // Ask to apply to config
        if (userInputManager.readYesNo("是否将最佳频道保存到配置？", true)) { // 汉化
            rf24Service.setChannel((uint8_t)bestCh);
            terminalView.println("RF24: 频道已设置为 " + std::to_string(bestCh) + "。\n"); // 汉化
        } else {
            terminalView.println("RF24: 频道未修改。\n"); // 汉化
        }
    } else {
        terminalView.println("\nRF24: 未检测到任何信号活动。\n"); // 汉化
    }
}

/*
Jam
*/
void Rf24Controller::handleJam() {
    auto confirm = userInputManager.readYesNo("RF24 干扰: 该操作将发送随机信号。是否继续？", false); // 汉化
    if (!confirm) return;

    // List of group names
    std::vector<std::string> groupNames;
    groupNames.reserve(RF24_GROUP_COUNT);
    for (size_t i = 0; i < RF24_GROUP_COUNT; ++i) groupNames.emplace_back(RF24_GROUPS[i].name);
    groupNames.emplace_back("全频段 (0..125)"); // 汉化

    // Select group
    int choice = userInputManager.readValidatedChoiceIndex("选择要干扰的频段组：", groupNames, 0); // 汉化

    // Get group
    const Rf24ChannelGroup* group = nullptr;
    if (choice >= (int)RF24_GROUP_COUNT) {
        // Full range
        uint8_t fullRangeChannels[126];
        for (uint8_t i = 0; i <= 125; ++i) fullRangeChannels[i] = i;
        static const Rf24ChannelGroup fullRangeGroup = {
            " 全频段", // 汉化
            fullRangeChannels,
            126
        };
        group = &fullRangeGroup;
    } else {
        group = &RF24_GROUPS[choice];
    }
    
    terminalView.println("\nRF24: 正在对目标频段发送干扰噪声... 按下[ENTER]停止。"); // 汉化

    rf24Service.stopListening();
    rf24Service.setDataRate(RF24_2MBPS);
    rf24Service.powerUp();
    rf24Service.setPowerMax();

    // Jam loop
    bool run = true;
    while (run) {
        for (size_t i = 0; i < group->count; ++i) {
            // Cancel
            int k = terminalInput.readChar();
            if (k == '\n' || k == '\r') { run = false; break; }

            // Sweep
            rf24Service.setChannel(group->channels[i]);
        }
    }

    rf24Service.flushTx();
    rf24Service.powerDown();
    terminalView.println("RF24: 干扰已被用户停止。\n"); // 汉化
}

/*
Sweep
*/
void Rf24Controller::handleSweep() {
    // Params
    int dwellMs  = userInputManager.readValidatedInt("每个频道驻留时间 (毫秒)", 10, 10, 1000); // 汉化
    int samples  = userInputManager.readValidatedInt("每个频道采样数", 80, 1, 100); // 汉化
    int thrPct   = userInputManager.readValidatedInt("活动度阈值 (%)", 1, 0, 100); // 汉化

    terminalView.println("\nRF24 扫频: 频道 0–125"
                         " | 驻留时间=" + std::to_string(dwellMs) + " 毫秒" + // 汉化
                         " | 采样数=" + std::to_string(samples) + // 汉化
                         " | 阈值=" + std::to_string(thrPct) + "%... 按下[ENTER]停止。\n"); // 汉化

    bool run = true;
    while (run) {
        for (int ch = 0; ch <= 125 && run; ++ch) {
            // Cancel
            char c = terminalInput.readChar();
            if (c == '\n' || c == '\r') { run = false; break; }

            rf24Service.setChannel(static_cast<uint8_t>(ch));

            // Analyze activity
            int hits = 0;
            for (int s = 0; s < samples; ++s) {
                rf24Service.startListening();
                delayMicroseconds((dwellMs * 1000) / samples);
                rf24Service.stopListening();
                if (rf24Service.testRpd()) {
                    hits += 2;
                }

                if  (rf24Service.testCarrier()) {
                    hits++;
                }
            }

            int activityPct = (hits * 100) / samples;
            if (activityPct > 100) activityPct = 100;

            // Log if above threshold
            if (activityPct >= thrPct) {
                terminalView.println(
                    "  频道 " + std::to_string(ch) + " (" + // 汉化
                    std::to_string(2400 + ch) + " MHz)" +
                    "  活动度=" + std::to_string(activityPct) + "%" // 汉化
                );
            }
        }
    }

    rf24Service.flushRx();
    terminalView.println("\nRF24 扫频: 已被用户停止。\n"); // 汉化
}

/*
Set Channel
*/
void Rf24Controller::handleSetChannel() {
    uint8_t ch = userInputManager.readValidatedUint8("频道 (0..125)？", 76, 0, 125); // 汉化
    rf24Service.setChannel(ch);
    terminalView.println("RF24: 频道已设置为 " + std::to_string(ch) + "。"); // 汉化
}

/*
NRF24 Configuration
*/
void Rf24Controller::handleConfig() {
    uint8_t csn = userInputManager.readValidatedInt("NRF24 CSN引脚？", state.getRf24CsnPin()); // 汉化
    uint8_t sck  = userInputManager.readValidatedInt("NRF24 SCK引脚？", state.getRf24SckPin()); // 汉化
    uint8_t miso = userInputManager.readValidatedInt("NRF24 MISO引脚？", state.getRf24MisoPin()); // 汉化
    uint8_t mosi = userInputManager.readValidatedInt("NRF24 MOSI引脚？", state.getRf24MosiPin()); // 汉化
    uint8_t ce  = userInputManager.readValidatedInt("NRF24 CE引脚？", state.getRf24CePin()); // 汉化
    state.setRf24CsnPin(csn);
    state.setRf24SckPin(sck);
    state.setRf24MisoPin(miso);
    state.setRf24MosiPin(mosi);
    state.setRf24CePin(ce);

    bool ok = rf24Service.configure(csn, ce, sck, miso, mosi);

    configured = true; // consider configured even if not detected to avoid re-asking
    terminalView.println(ok ? "\n ✅ 检测到NRF24并完成配置。\n" : "\n ❌ 未检测到NRF24。请检查接线。\n"); // 汉化
}

/*
Help
*/
void Rf24Controller::handleHelp() {
    terminalView.println("RF24 命令列表:"); // 汉化
    terminalView.println("  scan");
    terminalView.println("  sniff");
    terminalView.println("  sweep");
    terminalView.println("  jam");
    terminalView.println("  setchannel");
    terminalView.println("  config");
}