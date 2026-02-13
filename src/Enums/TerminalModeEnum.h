#pragma once
#include <string>

enum class TerminalMode {
    Serial,
    Web,
    None
};

class TerminalModeEnumMapper {
public:
    static std::string toString(TerminalMode mode) {
        switch (mode) {
            case TerminalMode::Serial: return "Serial";
            case TerminalMode::Web:    return "Web   ";
            case TerminalMode::None:   return "None  ";
            default:                   return "Unknown";
        }
    }
};
