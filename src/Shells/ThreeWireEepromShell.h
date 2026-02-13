#pragma once
#include <string>
#include <vector>
#include "Interfaces/ITerminalView.h"
#include "Interfaces/IInput.h"
#include "Managers/UserInputManager.h"
#include "Services/ThreeWireService.h"
#include "Transformers/ArgTransformer.h"
#include "States/GlobalState.h"

class ThreeWireEepromShell {
public:
    ThreeWireEepromShell(
        ITerminalView& terminalView,
        IInput& terminalInput,
        UserInputManager& userInputManager,
        ThreeWireService& threeWireService,
        ArgTransformer& argTransformer);

    void run();

private:
    void cmdProbe();
    void cmdRead();
    void cmdWrite();
    void cmdDump();
    void cmdErase();


    ITerminalView& terminalView;
    IInput& terminalInput;
    UserInputManager& userInputManager;
    ThreeWireService& threeWireService;
    ArgTransformer& argTransformer;
    GlobalState& state = GlobalState::getInstance();
};
