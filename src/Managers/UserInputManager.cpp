#include "UserInputManager.h"

std::string UserInputManager::getLine(bool onlyNumber /* = false */) {
    std::string result;
    size_t cursorIndex = 0;

    while (true) {
        char c = terminalInput.handler();

        if (c == CARDPUTER_SPECIAL_ARROW_UP) {
            terminalView.print(std::string(1, CARDPUTER_SPECIAL_ARROW_UP));
            continue;
        }

        if (c == CARDPUTER_SPECIAL_ARROW_DOWN) {
            terminalView.print(std::string(1, CARDPUTER_SPECIAL_ARROW_DOWN));
            continue;
        }

        // 回车键
        if (c == '\r' || c == '\n') break;

        // 退格键
        if ((c == '\b' || c == 127) && cursorIndex > 0) {
            cursorIndex--;
            result.erase(cursorIndex, 1);
            terminalView.print("\b \b");
            continue;
        }

        if (result.size() >= MAX_ALLOWED_INPUT_LENGTH) {
            continue;
        }

        // 仅允许数字
        if (onlyNumber && !std::isdigit(c)) {
            continue;
        }

        // 可打印字符
        if (isprint(c)) {
            result.insert(cursorIndex, 1, c);
            cursorIndex++;
            terminalView.print(std::string(1, c));
        }
    }

    terminalView.println("");
    return result;
}

std::string UserInputManager::readSanitizedString(const std::string& label,
                                                  const std::string& def,
                                                  bool onlyLetter /* = false */)
{
    while (true) {
        terminalView.print(label + " [" + def + "]: ");
        std::string input = getLine(false);

        // 按回车返回默认值
        if (input.empty()) {
            return def;
        }

        std::string out;
        out.reserve(input.size());

        for (char c : input) {
            unsigned char uc = static_cast<unsigned char>(c);

            if (std::isalpha(uc)) {
                out.push_back(c);
            }
            else if (!onlyLetter && std::isdigit(uc)) {
                out.push_back(c);
            }
            else if (!onlyLetter && (c == '_' || c == '+' || c == '-')) {
                out.push_back(c);
            }
            // 忽略其他字符
        }

        // 所有字符都被过滤？
        if (out.empty()) {
            terminalView.println("输入无效。允许输入：字母" +
                                 std::string(onlyLetter ? "" : "、数字、下划线'_'"));
            continue;
        }

        return out;
    }
}

uint8_t UserInputManager::readValidatedUint8(const std::string& label, uint8_t def, uint8_t min, uint8_t max) {
    while (true) {
        terminalView.print(label + " [" + std::to_string(def) + "]: ");
        std::string input = getLine();
        if (input.empty()) return def;

        if (argTransformer.isValidNumber(input)) {
            uint8_t val = argTransformer.toUint8(input);
            if (val >= min && val <= max) return val;
        }

        terminalView.println("输入无效。请输入 " + std::to_string(min) + "-" + std::to_string(max) + " 之间的数字");
    }
}

uint8_t UserInputManager::readValidatedUint8(const std::string& label, uint8_t defaultVal) {
    return readValidatedUint8(label, defaultVal, 0, 255);
}

uint32_t UserInputManager::readValidatedUint32(const std::string& label, uint32_t def) {
    while (true) {
        terminalView.print(label + " [" + std::to_string(def) + "]: ");
        std::string input = getLine();
        if (input.empty()) return def;

        if (argTransformer.isValidNumber(input)) {
            return argTransformer.toUint32(input);
        }

        terminalView.println("数字格式无效。");
    }
}

char UserInputManager::readCharChoice(const std::string& label, char def, const std::vector<char>& allowed) {
    while (true) {
        terminalView.print(label + " [" + def + "]: ");
        std::string input = getLine();
        if (input.empty()) return def;

        char c = toupper(input[0]);
        if (std::find(allowed.begin(), allowed.end(), c) != allowed.end()) return c;

        terminalView.println("选项无效。");
    }
}

bool UserInputManager::readYesNo(const std::string& label, bool def) {
    while (true) {
        terminalView.print(label + " [" + (def ? "y" : "n") + "]: ");
        std::string input = getLine();
        if (input.empty()) return def;

        char c = tolower(input[0]);
        if (c == 'y') return true;
        if (c == 'n') return false;

        terminalView.println("请输入 y 或 n 作答。");
    }
}

uint8_t UserInputManager::readModeNumber() {
    std::string inputDigit;
    size_t cursorIndex = 0;

    while (true) {
        char c = terminalInput.handler();

        if (c == CARDPUTER_SPECIAL_ARROW_UP) {
            terminalView.print(std::string(1, CARDPUTER_SPECIAL_ARROW_UP));
            continue;
        }

        if (c == CARDPUTER_SPECIAL_ARROW_DOWN) {
            terminalView.print(std::string(1, CARDPUTER_SPECIAL_ARROW_DOWN));
            continue;
        }

        // 回车键
        if (c == '\r' || c == '\n') {
            terminalView.println("");
            break;
        }

        // 退格键
        if ((c == '\b' || c == 127) && cursorIndex > 0) {
            cursorIndex--;
            inputDigit.erase(cursorIndex, 1);
            terminalView.print("\b \b");
            continue;
        }

        // 仅允许数字
        if (std::isdigit(c)) {
            inputDigit.insert(cursorIndex, 1, c);
            cursorIndex++;
            terminalView.print(std::string(1, c));
        }
    }

    if (inputDigit.empty()) {
        return -1;
    }

    return std::stoi(inputDigit);
}

uint8_t UserInputManager::readValidatedPinNumber(const std::string& label, uint8_t def, uint8_t min, uint8_t max, const std::vector<uint8_t>& forbiddenPins) {
    while (true) {
        int val = readValidatedUint8(label, def, min, max);
        if (std::find(forbiddenPins.begin(), forbiddenPins.end(), val) != forbiddenPins.end()) {
            terminalView.println("该引脚为保留/保护引脚，无法使用。");
            continue;
        }
        return val;
    }
}

uint8_t UserInputManager::readValidatedPinNumber(const std::string& label, uint8_t def, const std::vector<uint8_t>& forbiddenPins) {
    return readValidatedPinNumber(label, def, 0, 48, forbiddenPins);
}

std::vector<uint8_t> UserInputManager::readValidatedPinGroup(
    const std::string& label,
    const std::vector<uint8_t>& defaultPins,
    const std::vector<uint8_t>& protectedPins
) {
    while (true) {
        // 拼接默认引脚字符串
        std::string defaultStr;
        for (size_t i = 0; i < defaultPins.size(); ++i) {
            if (i > 0) defaultStr += " ";
            defaultStr += std::to_string(defaultPins[i]);
        }
        
        // 显示默认列表 [1 2 3 ...]
        terminalView.print(label + " [" + defaultStr + "]: ");

        // 获取用户输入
        std::string input = getLine();

        // 空输入，使用默认引脚
        if (input.empty()) {
            return defaultPins;
        }

        std::stringstream ss(input);
        std::vector<uint8_t> pins;
        int val;
        
        // 验证输入
        bool valid = true;
        while (ss >> val) {
            // 引脚号无效
            if (val < 0 || val > 48) {
                terminalView.println("无效引脚号: " + std::to_string(val));
                valid = false;
                break;
            }
            // 引脚受保护
            if (std::find(protectedPins.begin(), protectedPins.end(), val) != protectedPins.end()) {
                terminalView.println("引脚 " + std::to_string(val) + " 为保护/保留引脚，不可使用。");
                valid = false;
                break;
            }
            pins.push_back(static_cast<uint8_t>(val));
        }

        // 输入有效且非空
        if (valid && !pins.empty()) {
            return pins;
        }

        terminalView.println("请输入有效的、非保护的GPIO引脚号，多个引脚用空格分隔。");
    }
}

std::string UserInputManager::readValidatedHexString(
    const std::string& label,
    size_t numItems,
    bool ignoreLen,
    size_t digitsPerItem /* = 2 */)
{
    while (true) {
        terminalView.print(label + "(十六进制): ");
        std::string input = getLine();

        // 移除空格
        input.erase(std::remove_if(input.begin(), input.end(),
                    [](unsigned char c){ return std::isspace(c); }), input.end());

        // 空输入？
        if (input.empty()) {
            if (ignoreLen) {
                // 默认值（1个项，值为0）
                return (digitsPerItem == 2) ? "00" : "0000";
            }
            terminalView.println("❌ 输入不能为空。");
            continue;
        }

        // 验证是否为十六进制字符
        bool valid = std::all_of(input.begin(), input.end(),
                        [](unsigned char c){ return std::isxdigit(c); });
        if (!valid) {
            terminalView.println("❌ 包含无效字符。仅允许十六进制数字（0-9, A-F）。");
            continue;
        }

        // 验证长度
        if (ignoreLen) {
            if (input.length() % digitsPerItem != 0) {
                terminalView.println("❌ 长度必须是 " + std::to_string(digitsPerItem) + " 位十六进制数的倍数。");
                continue;
            }
        } else {
            const size_t expected = numItems * digitsPerItem;
            if (input.length() != expected) {
                terminalView.println("❌ 长度无效。预期长度为 " + std::to_string(expected) + " 位十六进制数。");
                continue;
            }
        }

        // 按每digitsPerItem位插入空格分隔
        std::string spaced;
        for (size_t i = 0; i < input.length(); i += digitsPerItem) {
            if (i > 0) spaced += ' ';
            spaced += input.substr(i, digitsPerItem);
        }

        return spaced;
    }
}

uint16_t UserInputManager::readValidatedCanId(const std::string& label, uint16_t defaultValue) {
    while (true) {
        terminalView.print(label + " (十六进制，最多3位) [默认值: " + argTransformer.toHex(defaultValue, 3) + "]: ");
        std::string input = getLine();

        if (input.empty()) return defaultValue;

        // 移除空格
        input.erase(std::remove_if(input.begin(), input.end(), ::isspace), input.end());

        // 允许"0x"前缀
        if (input.rfind("0x", 0) == 0 || input.rfind("0X", 0) == 0) {
            input = input.substr(2);  // 移除"0x"前缀
        }

        // 验证是否为有效十六进制
        bool valid = std::all_of(input.begin(), input.end(), [](char c) {
            return std::isxdigit(static_cast<unsigned char>(c));
        });

        if (!valid) {
            terminalView.println("❌ 包含无效字符。仅允许十六进制数字（0-9, A-F）。");
            continue;
        }

        // 检查长度
        if (input.length() > 3) {
            terminalView.println("❌ 长度过长。标准CAN ID 最大为 0x7FF（3位十六进制）。");
            continue;
        }

        // 转换为数值
        uint16_t id = std::stoul(input, nullptr, 16);

        // 检查最大值
        if (id > 0x7FF) {
            terminalView.println("❌ 值超过标准11位CAN ID范围（最大值 0x7FF）。");
            continue;
        }

        return id;
    }
}

int UserInputManager::readValidatedChoiceIndex(const std::string& label, const std::vector<std::string>& choices, int defaultIndex) {
    // 显示选项列表
    terminalView.println(label + ":");
    for (size_t i = 0; i < choices.size(); ++i) {
        std::string prefix = (i == defaultIndex) ? "*" : " ";
        terminalView.println("  [" + std::to_string(i + 1) + "] " + prefix + choices[i]);
    }

    // 询问选择序号
    terminalView.print("请输入序号（默认 " + std::to_string(defaultIndex + 1) + "）: ");
    std::string input = getLine(true); // 仅允许数字
    input = argTransformer.toLower(argTransformer.filterPrintable(input));

    // 默认值
    if (input.empty()) return defaultIndex;

    // 验证序号
    int index;
    if (!argTransformer.parseInt(input, index) || index < 1 || index > (int)choices.size()) {
        terminalView.println("❌ 选项无效，使用默认值。");
        return defaultIndex;
    }

    return index - 1; // 转换为0起始索引
}

int UserInputManager::readValidatedInt(const std::string& label, int def, int min /* = -127 */, int max /* = 127 */) {
    while (true) {
        terminalView.print(label + " [" + std::to_string(def) + "]: ");
        std::string input = getLine();
        if (input.empty()) return def;

        int val = 0;
        if (argTransformer.parseInt(input, val) && val >= min && val <= max) {
            return val;
        }
        terminalView.println("输入无效。请输入 " + std::to_string(min) + "-" + std::to_string(max) + " 之间的数字");
    }
}

int UserInputManager::readValidatedChoiceIndex(const std::string& label, const std::vector<int>& choices, int defaultIndex) {
    std::vector<std::string> strChoices;
    strChoices.reserve(choices.size());
    for (int val : choices) {
        strChoices.push_back(std::to_string(val));
    }
    return readValidatedChoiceIndex(label, strChoices, defaultIndex);
}

int UserInputManager::readValidatedChoiceIndex(const std::string& label,
                                               const std::vector<float>& choices,
                                               int defaultIndex) {
    std::vector<std::string> strChoices;
    strChoices.reserve(choices.size());

    for (float f : choices) {
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss << std::setprecision(2) << f; 
        strChoices.push_back(oss.str());
    }
    return readValidatedChoiceIndex(label, strChoices, defaultIndex);
}

int UserInputManager::readValidatedChoiceIndex(const std::string& label,
                                               const char* const* choices,
                                               size_t count,
                                               int defaultIndex)
{
    if (!choices || count == 0) {
        terminalView.println("❌ 无可用选项。");
        return -1;
    }

    if (defaultIndex < 0 || defaultIndex >= (int)count) {
        defaultIndex = 0;
    }

    terminalView.println(label + ":");
    for (size_t i = 0; i < count; ++i) {
        const char* s = choices[i] ? choices[i] : "";
        const bool isDef = ((int)i == defaultIndex);

        terminalView.println(
            "  [" + std::to_string(i + 1) + "] " +
            std::string(isDef ? "* " : "  ") +
            s
        );
    }

    terminalView.print("请输入序号（默认 " + std::to_string(defaultIndex + 1) + "）: ");
    std::string input = getLine(true); // 仅允许数字

    if (input.empty()) return defaultIndex;

    int idx = 0;
    if (!argTransformer.parseInt(input, idx) || idx < 1 || idx > (int)count) {
        terminalView.println("❌ 选项无效，使用默认值。");
        return defaultIndex;
    }

    return idx - 1;
}

float UserInputManager::readValidatedFloat(const std::string& label,
                                           float def,
                                           float min,
                                           float max) {
    while (true) {
        terminalView.print(label + " [" + std::to_string(def) + "]: ");
        std::string input = getLine();
        if (input.empty()) return def;

        // 移除空格
        input.erase(std::remove_if(input.begin(), input.end(), ::isspace), input.end());

        try {
            float v = std::stof(input);
            if (v >= min && v <= max) return v;
        } catch (...) {
            // 转换失败，继续循环
        }
        terminalView.println("输入无效。请输入 " + std::to_string(min) + " ~ " + std::to_string(max) + " 之间的数值");
    }
}