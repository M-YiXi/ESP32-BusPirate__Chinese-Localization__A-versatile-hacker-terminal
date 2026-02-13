#pragma once

#include "Models/TerminalCommand.h"

class IUsbController {
public:
    virtual ~IUsbController() = default;

    // Entry point for command
    virtual void handleCommand(const TerminalCommand& cmd) = 0;

    virtual void ensureConfigured() = 0;
};
