#pragma once

#include <Arduino.h>
#include "Services/InfraredService.h"
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Transformers/ArgTransformer.h"
#include "Managers/UserInputManager.h"
#include "Data/UniversalRemoteCommands.h"

class UniversalRemoteShell {
public:
    UniversalRemoteShell( ITerminalView& view, IInput& input, InfraredService& irService, ArgTransformer& argTransformer, UserInputManager& userInputManager);
    void run();

private:
    InfraredService& infraredService;
    ITerminalView& terminalView;
    IInput& terminalInput;
    ArgTransformer& argTransformer;
    UserInputManager& userInputManager;

    void sendCommandGroup(const InfraredCommandStruct* group, size_t size);
};
