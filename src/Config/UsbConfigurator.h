#pragma once

#include <Interfaces/IUsbService.h>
#include <Interfaces/IUsbController.h>
#include <Transformers/ArgTransformer.h>
#include <Managers/UserInputManager.h>
#include <Interfaces/ITerminalView.h>
#include <Interfaces/IInput.h>

struct UsbComponents {
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;
    IUsbService& usbService;
    IUsbController& usbController;
};

class UsbConfigurator {
public:
    static UsbComponents configure(ITerminalView& terminalView, IInput& terminalInput, IInput& deviceInput);
};
