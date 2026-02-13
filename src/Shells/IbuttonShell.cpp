#include "IbuttonShell.h"
#include <sstream>
#include <iomanip>
#include <cstring>

/**
 * @brief 构造函数：初始化iButton RW1990交互Shell的依赖组件
 * @param terminalView 终端视图接口（负责文本输出）
 * @param terminalInput 输入接口（负责用户输入/按键检测）
 * @param userInputManager 用户输入管理类（输入验证、选择读取）
 * @param argTransformer 参数转换工具（十六进制字符串解析）
 * @param oneWireService 单总线服务类（底层iButton操作）
 */
IbuttonShell::IbuttonShell(ITerminalView& terminalView,
                           IInput& terminalInput,
                           UserInputManager& userInputManager,
                           ArgTransformer& argTransformer,
                           OneWireService& oneWireService)
    : terminalView(terminalView),
      terminalInput(terminalInput),
      userInputManager(userInputManager),
      argTransformer(argTransformer),
      oneWireService(oneWireService) {}

/**
 * @brief 运行iButton RW1990交互Shell主循环
 * @note 显示操作菜单，根据用户选择执行读取/写入/复制ID操作，选择退出项时终止循环
 */
void IbuttonShell::run() {
    const std::vector<std::string> actions = {
        " 🔍 读取ID",
        " ✏️  写入ID",
        " 📋 复制ID",
        " 🚪 退出Shell"
    };

    while (true) {
        terminalView.println("\n=== iButton RW1990交互Shell ===");
        int index = userInputManager.readValidatedChoiceIndex("选择操作", actions, 0);

        // 检测退出选择或输入异常
        if (index == -1 || actions[index] == " 🚪 退出Shell") {
            terminalView.println("退出iButton交互Shell...\n");
            break;
        }

        // 执行选中的操作
        switch (index) {
            case 0: cmdReadId(); break;    // 读取iButton ID
            case 1: cmdWriteId(); break;   // 写入iButton ID
            case 2: cmdCopyId(); break;    // 复制iButton ID（源→目标）
            default:
                terminalView.println("❌ 无效选择，执行默认操作。\n");
                break;
        }
    }
}

/**
 * @brief 【操作】读取iButton的ROM ID（8字节）
 * @note 支持用户按回车终止读取，读取后验证CRC校验位，输出格式化的十六进制ID
 */
void IbuttonShell::cmdReadId() {
    terminalView.println("iButton读取：按[回车]停止。\n");

    while (true) {
        // 检测用户按键（回车/换行则终止）
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\niButton读取：用户已停止操作。");
            break;
        }
        delay(100); // 降低读取频率，避免硬件频繁检测

        // 单总线复位（检测iButton是否连接）
        uint8_t buffer[8];
        if (!oneWireService.reset()) continue;

        terminalView.println("iButton读取：正在读取...");
        oneWireService.write(0x33);  // 发送读取ROM命令（0x33）
        oneWireService.readBytes(buffer, 8); // 读取8字节ROM ID

        // 格式化ROM ID为大写十六进制字符串（XX XX XX XX XX XX XX XX）
        std::ostringstream oss;
        oss << std::uppercase << std::hex << std::setfill('0');
        for (int i = 0; i < 8; ++i) {
            oss << std::setw(2) << static_cast<int>(buffer[i]);
            if (i < 7) oss << " ";
        }

        terminalView.println("ROM ID：" + oss.str());

        // 校验CRC（前7字节的CRC8应等于第8字节）
        uint8_t crc = oneWireService.crc8(buffer, 7);
        if (crc != buffer[7]) {
            terminalView.println("❌ ROM ID校验（CRC）错误。");
        }

        break; // 读取完成后退出循环
    }
}

/**
 * @brief 【操作】向iButton写入指定的8字节ROM ID
 * @note 支持用户终止等待、最多8次重试写入，写入后验证ID是否匹配
 */
void IbuttonShell::cmdWriteId() {
    terminalView.println("iButton ID写入：输入8字节ID（示例：01 AA 03 BB 05 FF 07 08）");

    // 读取并验证用户输入的8字节十六进制ID
    std::string hexStr = userInputManager.readValidatedHexString("输入ROM ID（8字节）", 8);
    std::vector<uint8_t> idBytes = argTransformer.parseHexList(hexStr);

    // 校验ID长度（必须正好8字节）
    if (idBytes.size() != 8) {
        terminalView.println("❌ ID长度无效，必须正好8字节。");
        return;
    }

    const int maxRetries = 8; // 最大写入重试次数
    int attempt = 0;
    bool success = false;

    terminalView.println("iButton ID写入：等待设备连接...按[回车]停止");

    // 等待iButton连接（单总线复位成功），支持用户终止
    while (!oneWireService.reset()) {
        delay(1);
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\niButton写入：用户已停止操作。");
            return;
        }
    }

    // 尝试写入并验证ID
    while (attempt < maxRetries && !success) {
        attempt++;
        terminalView.println("尝试次数 " + std::to_string(attempt) + "...");

        // 写入ID到RW1990芯片
        oneWireService.writeRw1990(state.getOneWirePin(), idBytes.data(), idBytes.size());
        delay(50); // 写入后延时，确保数据生效

        // 验证写入结果
        uint8_t buffer[8];
        if (!oneWireService.reset()) continue; // 复位失败则跳过本次验证
        oneWireService.write(0x33);            // 读取ROM命令
        oneWireService.readBytes(buffer, 8);   // 读取写入后的ID

        // 比较前7字节（第8字节为CRC，无需比较）
        if (memcmp(buffer, idBytes.data(), 7) != 0) {
            terminalView.println("❌ ROM ID字节不匹配。");
            continue;
        }

        success = true; // 验证通过，标记写入成功
    }

    // 输出写入结果
    if (success) terminalView.println("✅ ID写入成功。");
    else         terminalView.println("❌ ID写入失败。");
}

/**
 * @brief 【操作】复制iButton ID（从源标签复制到目标克隆标签）
 * @note 分两步：读取源ID → 写入目标ID，全程支持用户终止，写入最多8次重试
 */
void IbuttonShell::cmdCopyId() {
    terminalView.println("iButton复制：插入源标签...按[回车]停止\n");

    // 等待源标签连接，支持用户终止
    while (!oneWireService.reset()) {
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\niButton复制：用户已停止操作。");
            return;
        }
        delay(100);
    }

    // 读取源标签的ROM ID
    uint8_t id[8];
    oneWireService.write(0x33);  // 发送读取ROM命令
    oneWireService.readBytes(id, 8);
    std::vector<uint8_t> idVec(id, id + 8); // 转换为vector便于后续操作

    // 格式化并输出源标签ID
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < 8; ++i) {
        oss << std::setw(2) << static_cast<int>(idVec[i]);
        if (i < 7) oss << " ";
    }
    terminalView.println("ROM ID：" + oss.str());

    // 提示用户切换为目标标签
    terminalView.println("移除源标签，插入目标克隆标签...准备好后按[回车]。");
    while (true) {
        auto c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("开始写入ID...");
            break;
        }
    }

    // 等待目标标签连接，支持用户终止
    const int maxRetries = 8;
    int attempt = 0;
    bool success = false;

    while (!oneWireService.reset()) {
        auto key = terminalInput.readChar();
        if (key == '\r' || key == '\n') {
            terminalView.println("\niButton复制：用户已停止操作。");
            return;
        }
        delay(1);
    }

    // 尝试写入源ID到目标标签并验证
    while (attempt < maxRetries && !success) {
        attempt++;
        terminalView.println("尝试次数 " + std::to_string(attempt) + "...");

        // 写入ID到目标标签
        oneWireService.writeRw1990(state.getOneWirePin(), idVec.data(), idVec.size());
        delay(50);

        // 验证写入结果
        uint8_t buffer[8];
        if (!oneWireService.reset()) continue;
        oneWireService.write(0x33);
        oneWireService.readBytes(buffer, 8);

        // 比较前7字节
        if (memcmp(buffer, idVec.data(), 7) != 0) {
            terminalView.println("ROM ID字节不匹配。");
            continue;
        }

        success = true;
        break;
    }

    // 输出复制结果
    if (success) {
        terminalView.println("✅ 复制完成。");
    } else {
        terminalView.println("❌ ID复制失败。");
    }
}