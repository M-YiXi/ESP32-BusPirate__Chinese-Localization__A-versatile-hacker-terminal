#include "CommandHistoryManager.h"

// 添加命令到历史记录
void CommandHistoryManager::add(const std::string& line) {
    // 非空且与最后一条记录不同时才添加
    if (!line.empty() && (history.empty() || history.back() != line)) {
        history.push_back(line);

        // 超出最大记录数时删除最旧的一条
        if (history.size() > maxHistory) {
            history.erase(history.begin());
        }
    }
    // 重置索引到历史记录末尾
    index = history.size();
}

// 向上翻查历史命令（上一条）
const std::string& CommandHistoryManager::up() {
    if (index > 0) {
        --index;
    }
    // 获取当前索引对应的命令，无则为空
    currentLine = (index < history.size()) ? history[index] : "";
    return currentLine;
}

// 向下翻查历史命令（下一条）
const std::string& CommandHistoryManager::down() {
    if (index < history.size() - 1) {
        ++index;
    } else {
        // 翻到末尾时重置索引并清空当前行
        index = history.size();
        currentLine.clear();
    }
    // 获取当前索引对应的命令，无则为空
    currentLine = (index < history.size()) ? history[index] : "";
    return currentLine;
}

// 重置历史记录索引（回到末尾）
void CommandHistoryManager::reset() {
    index = history.size();
}

// 获取历史记录的总条数
size_t CommandHistoryManager::size() const {
    return history.size();
}