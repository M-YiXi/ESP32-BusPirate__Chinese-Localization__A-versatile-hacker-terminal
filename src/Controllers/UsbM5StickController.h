#pragma once

#ifdef DEVICE_M5STICK


#include "Interfaces/IUsbController.h"
#include "Interfaces/ITerminalView.h"

class UsbM5StickController : public IUsbController {
public:
    explicit UsbM5StickController(ITerminalView& terminalView);

    void handleCommand(const TerminalCommand& cmd) override;
    void ensureConfigured() override;

private:
    ITerminalView& terminalView;
};

#endif