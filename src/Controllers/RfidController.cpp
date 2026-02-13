#include "RfidController.h"

/*
Constructor
*/
RfidController::RfidController(
    ITerminalView& view,
    IInput& input,
    RfidService& rfidService,
    UserInputManager& uim,
    ArgTransformer& transformer
) : terminalView(view),
    terminalInput(input),
    rfidService(rfidService),
    userInputManager(uim),
    argTransformer(transformer) {}

/*
Entry point for command
*/
void RfidController::handleCommand(const TerminalCommand& cmd) {
    const std::string root = cmd.getRoot();

    if (root == "read")             handleRead(cmd);
    else if (root == "write")       handleWrite(cmd);
    else if (root == "clone")       handleClone(cmd);
    else if (root == "erase")       handleErase(cmd);
    else if (root == "config")      handleConfig();
    else                            handleHelp();
}

/*
Read
*/
void RfidController::handleRead(const TerminalCommand&) {
    auto modes = rfidService.getTagTypes();
    int mode = userInputManager.readValidatedChoiceIndex("选择标签类型", modes, 0); // 汉化
    
    const uint32_t PRINT_INTERVAL_MS = 300;
    uint32_t lastPrint = 0;
    
    terminalView.println("\nRFID读取: 等待标签靠近... 按下[ENTER]停止。\n"); // 汉化
    while (true) {
        // User enter press
        int ch = terminalInput.readChar(); 
        if (ch == '\n' || ch == '\r') break;

        uint32_t now = millis();
        if ((now - lastPrint) >= PRINT_INTERVAL_MS) {
            lastPrint = now;

            // Read and display tag infos if any
            int rc = rfidService.read(mode);
            if (rc == RFIDInterface::SUCCESS) {
                terminalView.println(std::string(" [标签] UID   : ") + rfidService.uid()); // 汉化
                terminalView.println(std::string("       ATQA  : ") + rfidService.atqa());
                terminalView.println(std::string("       SAK   : ") + rfidService.sak());
                terminalView.println(std::string("       类型  : ") + rfidService.piccType() + "\n"); // 汉化
            }
        }
        delay(1);
    }

    terminalView.println("\nRFID读取: 完成。\n"); // 汉化
}

/*
Write
*/
void RfidController::handleWrite(const TerminalCommand&) {
    std::vector<std::string> choices = {
        " UID（魔术卡专用）", // 汉化
        " 块/页数据" // 汉化
    };

    int sel = userInputManager.readValidatedChoiceIndex("选择写入选项", choices, 0); // 汉化

    if (sel == 0) handleWriteUid();
    else          handleWriteBlock();
}

/*
Write UID
*/
void RfidController::handleWriteUid() {
    terminalView.println("RFID写入UID: 该操作需要MIFARE Classic魔术卡（可重写块0）。"); // 汉化

    // UID, variable length 4/7/10 bytes
    std::string uidHex = userInputManager.readValidatedHexString(
        "UID（4、7或10字节）", /*numBytes*/0, /*ignoreLen*/true, /*digitsPerItem*/2 // 汉化
    );

    // Validate length
    std::string uidHexClean = uidHex;
    uidHexClean.erase(std::remove(uidHexClean.begin(), uidHexClean.end(), ' '), uidHexClean.end());
    size_t uidLen = uidHexClean.size() / 2;
    if (!(uidLen == 4 || uidLen == 7 || uidLen == 10)) {
        terminalView.println("无效的UID长度。必须是4、7或10字节。\n"); // 汉化
        return;
    }
    rfidService.setUid(uidHexClean);

    // SAK (1 byte hex)
    std::string sakHex = userInputManager.readValidatedHexString("SAK（1字节，示例：08）", 1, false, 2); // 汉化
    rfidService.setSak(sakHex);

    // ATQA (2 bytes hex)
    std::string atqaHex = userInputManager.readValidatedHexString("ATQA（2字节，示例：00 04）", 2, false, 2); // 汉化
    rfidService.setAtqa(atqaHex);

    // Parse defined fields
    rfidService.parseData();

    // Wait for user to place the card
    terminalView.println("RFID写入UID: 请放置魔术卡。按下[ENTER]取消。\n"); // 汉化
    while (true) {
        // User enter press
        int ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            terminalView.println("RFID写入UID: 已被用户停止。\n"); // 汉化
            return;
        }

        // Write UID
        int rc = rfidService.clone(false); // dont check sak
        if (rc == RFIDInterface::SUCCESS) {
            terminalView.println("RFID写入UID: 完成。\n"); // 汉化
            return;
        } else if (rc == RFIDInterface::TAG_NOT_PRESENT) {
            delay(5);
            continue;
        } else {
            terminalView.println(" -> " + rfidService.statusMessage(rc));
            terminalView.println("RFID写入: UID写入仅支持块0可重写的卡片。"); // 汉化
            terminalView.println("");
            return;
        }
    }
}

/*
Write Block
*/
void RfidController::handleWriteBlock() {
    // Tag type
    std::vector<std::string> modes = rfidService.getTagTypes();
    int mode = userInputManager.readValidatedChoiceIndex("目标标签类型", modes, 0); // 汉化

    // Size per item (block/page)
    size_t bytesPerItem = 16;
    if (mode == 0) {
        // MIFARE Classic or NTAG/Ultralight
        auto fam = rfidService.getMifareFamily();
        int famIdx = userInputManager.readValidatedChoiceIndex("目标标签系列", fam, 0); // 汉化
        bytesPerItem = (famIdx == 0) ? 16 : 4;
    } else {
        // FeliCa
        bytesPerItem = 16;
    }

    // Block/page index
    int index = userInputManager.readValidatedInt(
        (bytesPerItem == 16) ? "块索引" : "页索引", // 汉化
        /*def*/ 4, /*min*/ 0, /*max*/ 4095
    );
    std::string hex = userInputManager.readValidatedHexString(
        (bytesPerItem == 16) ? "数据（16字节）" : "数据（4字节）", // 汉化
        /*numBytes*/ bytesPerItem, /*ignoreLen*/ false, /*digitsPerItem*/ 2
    );

    // Load payload
    std::string dump = "Page " + std::to_string(index) + ": " + hex + "\n";
    rfidService.loadDump(dump);

    // Wait and write
    terminalView.println("RFID写入: 请将目标标签靠近读卡器。按下[ENTER]停止。\n"); // 汉化
    while (true) {
        // Cancel
        int ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            terminalView.println("RFID写入: 已被用户停止。\n"); // 汉化
            return;
        }

        // Write
        int rc = rfidService.write(mode);
        if (rc == RFIDInterface::SUCCESS) {
            terminalView.println("RFID写入: 完成。\n"); // 汉化
            return;
        } else if (rc == RFIDInterface::TAG_NOT_PRESENT) {
            // keep trying until a tag is detected
            delay(5);
            continue;
        } else {
            terminalView.println(" -> " + rfidService.statusMessage(rc));
            terminalView.println("");
            return;
        }
    }
}

/*
Erase
*/
void RfidController::handleErase(const TerminalCommand&) {
    // Confirm
    bool confirm = userInputManager.readYesNo(
        "RFID擦除: 该操作将擦除标签数据。是否继续？", false // 汉化
    );
    if (!confirm) { terminalView.println("已终止。\n"); return; } // 汉化

    terminalView.println("RFID擦除: 请将待擦除标签靠近读卡器... 按下[ENTER]停止。\n"); // 汉化

    while (true) {
        // Cancel
        int ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            terminalView.println("RFID擦除: 已被用户停止。\n"); // 汉化
            return;
        }

        // Try erasing
        int rc = rfidService.erase();
        if (rc == RFIDInterface::SUCCESS) {
            terminalView.println("RFID擦除: 完成。\n"); // 汉化
            return;
        } else if (rc != RFIDInterface::TAG_NOT_PRESENT) {
            // Any other error -> report and stop
            terminalView.println(" -> " + rfidService.statusMessage(rc));
            terminalView.println("RFID擦除: 标签擦除失败。\n"); // 汉化
            terminalView.println("");

            return;
        }
    }
}

/*
Clone
*/
void RfidController::handleClone(const TerminalCommand&) {
    auto modes = rfidService.getTagTypes();
    int mode = userInputManager.readValidatedChoiceIndex("选择标签类型", modes, 0); // 汉化

    terminalView.println("\nRFID UID克隆: 等待Mifare源标签靠近... 按下[ENTER]停止。\n"); // 汉化

    const uint32_t PRINT_INTERVAL_MS = 300;
    uint32_t lastPrint = 0;
    bool haveSource = false;

    while (true) {
        // Cancel
        int ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') return;

        uint32_t now = millis();
        if ((uint32_t)(now - lastPrint) >= PRINT_INTERVAL_MS) {
            lastPrint = now;

            // Read
            int rc = rfidService.read(mode);
            if (rc == RFIDInterface::SUCCESS) {
                haveSource = true;
                terminalView.println(std::string(" [源标签] UID   : ") + rfidService.uid()); // 汉化
                terminalView.println(std::string("          ATQA  : ") + rfidService.atqa());
                terminalView.println(std::string("          SAK   : ") + rfidService.sak());
                terminalView.println(std::string("          类型  : ") + rfidService.piccType() + "\n"); // 汉化
                break;
            }
        }
        delay(1);
    }

    if (!haveSource) {
        terminalView.println("\nRFID UID克隆: 未检测到源标签。\n"); // 汉化
        return;
    }

    // Confirm
    terminalView.println("\n请将目标卡片放置在PN532读卡器上。"); // 汉化
    bool proceed = userInputManager.readYesNo("是否准备好开始克隆？", true); // 汉化
    if (!proceed) { terminalView.println("RFID UID克隆: 已被用户取消。\n"); return; } // 汉化
    terminalView.println("RFID UID克隆: 等待Mifare目标标签靠近... 按下[ENTER]停止。"); // 汉化

    // Cloning
    while (true) {
        // Cancel
        int ch = terminalInput.readChar();
        if (ch == '\n' || ch == '\r') {
            terminalView.println("RFID UID克隆: 已被用户停止。\n"); // 汉化
            return;
        }

        int rc = rfidService.clone();
        if (rc == RFIDInterface::SUCCESS) {
            terminalView.println(" -> 克隆成功"); // 汉化
            terminalView.println("RFID UID克隆: 完成。\n"); // 汉化
            return;
        } else if (rc == RFIDInterface::TAG_NOT_PRESENT) {
            // target not detected
            delay(5);
            continue;
        } else {
            // other error: report and stop
            terminalView.println(" -> " + rfidService.statusMessage(rc));
            terminalView.println("RFID UID克隆: 克隆操作可能需要使用'魔术卡'。"); // 汉化
            terminalView.println("");
            return;
        }
    }
}

/*
Config
*/
void RfidController::handleConfig() {
    terminalView.println("RFID配置:"); // 汉化

    const auto& forbidden = state.getProtectedPins();

    uint8_t sda = userInputManager.readValidatedPinNumber("PN532 SDA引脚", state.getRfidSdaPin(), forbidden); // 汉化
    state.setRfidSdaPin(sda);

    uint8_t scl = userInputManager.readValidatedPinNumber("PN532 SCL引脚", state.getRfidSclPin(), forbidden); // 汉化
    state.setRfidSclPin(scl);

    // Configure + begin
    rfidService.configure(sda, scl);
    bool ok = rfidService.begin();

    if (!ok) {
        terminalView.println("\n ❌ RFID: PN532初始化失败。请检查接线。\n"); // 汉化
        configured = false;
    } else {
        terminalView.println("\n ✅ RFID: 检测到PN532模块并完成初始化。\n"); // 汉化
        configured = true;
    }
}

/*
Help
*/
void RfidController::handleHelp() {
    terminalView.println("RFID命令列表:"); // 汉化
    terminalView.println("  read");
    terminalView.println("  write");
    terminalView.println("  clone");
    terminalView.println("  erase");
    terminalView.println("  config");
}

/*
Ensure Configuration
*/
void RfidController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
        return;
    }

    // Reapply
    rfidService.configure(state.getRfidSdaPin(), state.getRfidSclPin());
    rfidService.begin();
}