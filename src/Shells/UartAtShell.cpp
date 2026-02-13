#include "UartAtShell.h"
#include <sstream>
#include <cstdio>
#include <regex>

UartAtShell::UartAtShell(ITerminalView& terminalView,
                         IInput& terminalInput,
                         UserInputManager& userInputManager,
                         ArgTransformer& argTransformer,
                         UartService& uartService)
: terminalView(terminalView),
  terminalInput(terminalInput),
  userInputManager(userInputManager),
  argTransformer(argTransformer),
  uartService(uartService) {}

void UartAtShell::run() {
    while (true){
        terminalView.println("\n=== UART AT 命令行 ===");
        
        // 选择模式
        AtMode mode{};
        if (!selectMode(mode)) {
            terminalView.println("正在退出 UART AT 命令行...\n");
            return;
        }

        // 在模式下循环操作
        actionLoop(mode);
    }
}

void UartAtShell::actionLoop(AtMode& mode) {
    while (true) {
        const auto& actions = getAtActionsFor(mode);
        const AtActionItem* chosen = nullptr;

        terminalView.println("\n=== UART AT 命令行 ===");

        // 选择操作
        selectAction(actions, chosen);

        // 返回
        if (!chosen) {
            terminalView.println("返回模式选择...\n");

            break;
        }

        // 如果需要参数则询问
        std::string cmd;
        if (!buildCommandFromArgs(chosen->command, chosen->args, chosen->args_count, cmd)) {
            terminalView.println("⚠️  命令已取消.\n");
            continue;
        }

        // 如果是破坏性命令则确认
        if (!confirmIfDestructive(*chosen)) {
            terminalView.println("⚠️  破坏性命令已取消.\n");
            continue;
        }

        // 发送命令
        terminalView.println("发送: " + cmd + " ... 等待响应");
        auto response = sendAt(cmd);

        // 响应
        terminalView.println("\n=== 响应 ===");
        if (response.empty()) {
            terminalView.println("\n设备无响应.\n");
        } else {
            std::string responseFormatted = "\n" + response;
            terminalView.println(responseFormatted.c_str());
        }
    }
}

bool UartAtShell::selectMode(AtMode& outMode) {
    std::vector<std::string> items;
    items.reserve(kAtModesCount + 1); // + "退出"

    // 每个模式
    for (std::size_t i = 0; i < kAtModesCount; ++i) {
        std::string label = joinLabel(kAtModes[i].emoji, kAtModes[i].name);
        if (i < 9) label = " " + label; // 视觉对齐 1..9
        items.push_back(std::move(label));
    }

    // 退出选项 (始终在最后)
    static constexpr const char* kExitLabel = "🚪  退出命令行";
    items.emplace_back(kExitLabel);

    // 选择
    int index = userInputManager.readValidatedChoiceIndex("选择 AT 模式", items, 0);
    if (index < 0) return false;

    const std::size_t uindex = static_cast<std::size_t>(index);

    // 选择 "退出" 或超出范围
    if (uindex >= kAtModesCount) return false;

    outMode = kAtModes[uindex].mode;
    return true;
}

bool UartAtShell::selectAction(AtActionSlice actions, const AtActionItem*& outAction) {
    std::vector<std::string> items;
    items.reserve(actions.size + 1); // + 返回

    std::size_t i = 0;
    for (; i < actions.size; ++i) {
        const auto& a = actions.data[i];
        auto label = joinLabel(a.emoji, a.label, a.command);
        if (i < 9) label = " " + label;
        items.push_back(std::move(label));
    }

    // 选项 "返回"
    items.push_back(i > 9 ? "↩️   返回" : " ↩️   返回");

    int index = userInputManager.readValidatedChoiceIndex("选择命令", items, 0);
    if (index < 0) return false;

    if (static_cast<std::size_t>(index) == actions.size) {
        return false; // 返回
    }

    outAction = &actions.data[static_cast<std::size_t>(index)];
    return true;
}

std::string UartAtShell::buildPromptText(const AtActionArg& a, size_t idx) const {
    std::string label = a.name ? std::string(a.name) : ("参数#" + std::to_string(idx + 1));
    std::string p = "输入 " + label;
    if (a.hint && *a.hint) {
        p += " (例如 ";
        p += a.hint;
        p += ")";
    }
    if (!a.required && a.defaultValue) {
        p += " [默认: ";
        p += a.defaultValue;
        p += "]";
    }
    p += ": ";
    return p;
}

std::string UartAtShell::readUserLine(const std::string& prompt) {
    terminalView.print(prompt);
    return userInputManager.getLine();
}

bool UartAtShell::isInChoices(const std::string& v, const char* choices) const {
    if (!choices) return false;
    std::string vv = argTransformer.toLower(v);
    std::string cs = argTransformer.toLower(choices);

    size_t start = 0;
    while (true) {
        size_t bar = cs.find('|', start);
        std::string tok = cs.substr(start, (bar == std::string::npos) ? (cs.size() - start) : (bar - start));
        if (vv == tok) return true;
        if (bar == std::string::npos) return false;
        start = bar + 1;
    }
}

bool UartAtShell::validateAndFormat(const AtActionArg& a, const std::string& raw, std::string& out) const {
    if (raw.empty()) { terminalView.println("❌ 此字段为必填."); return false; }

    switch (a.type) {
        case AtArgType::Phone:
        case AtArgType::String:
            out = raw;
            return true;

        case AtArgType::Uint:
            if (!argTransformer.isValidNumber(raw)) {
                terminalView.println("❌ 需要无符号整数.");
                return false;
            }
            out = std::to_string(argTransformer.toUint32(raw));
            return true;

        case AtArgType::Int: {
            int iv = 0;
            if (!argTransformer.parseInt(raw, iv)) {
                terminalView.println("❌ 需要有符号整数.");
                return false;
            }
            out = std::to_string(iv);
            return true;
        }

        case AtArgType::Bool01:
            if (raw == "0" || raw == "1") { out = raw; return true; }
            terminalView.println("❌ 请输入 0 或 1.");
            return false;

        case AtArgType::HexBytes: {
            auto bytes = argTransformer.parseHexList(raw);
            if (bytes.empty()) {
                terminalView.println("❌ 需要十六进制字节 (例如 \"01 AA 03\").");
                return false;
            }
            out.clear();
            for (size_t i = 0; i < bytes.size(); ++i) {
                if (i) out += ' ';
                out += argTransformer.toHex(bytes[i], 2);
            }
            return true;
        }

        case AtArgType::Choice:
            if (!isInChoices(raw, a.choices)) {
                terminalView.println("❌ 无效选择.");
                return false;
            }
            out = raw;
            return true;

        case AtArgType::Regex:
            try {
                if (!a.pattern) return false;
                if (!std::regex_match(raw, std::regex(a.pattern))) {
                    terminalView.println("❌ 格式无效.");
                    return false;
                }
                out = raw;
                return true;
            } catch (...) {
                terminalView.println("❌ 正则表达式错误.");
                return false;
            }
    }
    return false;
}

bool UartAtShell::acquireArgValue(const AtActionArg& a, size_t idx, std::string& accepted, bool& hasValue) {
    // 重置输出
    accepted.clear();
    hasValue = false;

    // 持续提示直到获得有效值, 或如果是可选无默认值则跳过
    while (true) {
        const std::string prompt = buildPromptText(a, idx);
        std::string raw = readUserLine(prompt);

        // 如果输入为空
        if (raw.empty()) {
            // 可选且有默认值 -> 取默认值
            if (!a.required && a.defaultValue) {
                accepted = a.defaultValue;
                hasValue = true;
                return true;
            }
            // 可选无默认值 -> 完全跳过该值
            if (!a.required && !a.defaultValue) {
                // 保持 hasValue = false, accepted = ""
                return true; // 跳过
            }
            // 必填 -> 提示并循环
            terminalView.println("❌ 此字段为必填.");
            continue;
        }

        // 非空输入: 验证
        if (validateAndFormat(a, raw, accepted)) {
            hasValue = true;
            return true;
        }
        // 否则: 验证打印了错误, 继续循环
    }
}

std::string UartAtShell::placeholderFor(size_t idx) const {
    return "%" + std::to_string(idx + 1);
}

void UartAtShell::applyArgToCommand(std::string& cmd, size_t idx, const std::string& accepted, bool hasValue) const {
    const std::string ph = placeholderFor(idx);
    size_t pos = cmd.find(ph);

    if (pos != std::string::npos) {
        cmd.replace(pos, ph.size(), accepted);
        return;
    }

    // 模板中没有占位符: 仅当确实有值时才追加
    if (hasValue) {
        cmd += (idx ? "," : " ");
        cmd += accepted;
    }
}

bool UartAtShell::buildCommandFromArgs(const char* commandTemplate,
                                       const AtActionArg* args,
                                       std::size_t argCount,
                                       std::string& outCmd) {
    if (args == nullptr || argCount == 0) {
        outCmd = commandTemplate;
        return true;
    }

    std::string cmd = commandTemplate;

    for (std::size_t i = 0; i < argCount; ++i) {
        const AtActionArg& a = args[i];
        std::string accepted;
        bool hasValue = false;

        if (!acquireArgValue(a, i, accepted, hasValue)) {
            // 如果你想能够取消: return false;
        }

        applyArgToCommand(cmd, i, accepted, hasValue);
    }

    outCmd = std::move(cmd);
    return true;
}

bool UartAtShell::confirmIfDestructive(const AtActionItem& action) {
    if (!action.destructive) return true;

    terminalView.println("⚠️  此操作可能具有破坏性: " + std::string(action.label));
    std::vector<std::string> choices = { "否, 取消", "是, 继续" };
    int c = userInputManager.readValidatedChoiceIndex("确定吗?", choices, 0);
    return (c == 1);
}

std::string UartAtShell::sendAt(const std::string& cmd, uint32_t timeoutMs /*=500*/) {
    // uartService.flush();

    // 发送
    uartService.write(cmd);
    uartService.write("\r\n");
    
    const uint32_t start = millis();
    std::string resp = "";
    uint32_t lastByteTs = start;
    
    // 读取直到超时
    while (millis() - start < timeoutMs) {
        while (uartService.available() > 0) {
            char c = uartService.read();
            resp.push_back(c);
        }
        delay(1);
    }

    return resp;
}

std::string UartAtShell::joinLabel(const char* emoji, const char* text, const char* rawCmd) {
    std::string s;
    if (emoji && *emoji) { s += emoji; s += "  "; }
    if (rawCmd && *rawCmd) {
        s += rawCmd;
        s+= " - ";
    }
    if (text && *text)  { s += text; }
    return s;
}