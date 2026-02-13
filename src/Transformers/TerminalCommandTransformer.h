#pragma once
#include <string>
#include <Models/TerminalCommand.h>

class TerminalCommandTransformer {
public:
    TerminalCommand transform(const std::string& raw) const;
};
