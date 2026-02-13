#include "ThreeWireController.h"

/*
Constructor
*/
ThreeWireController::ThreeWireController(
    ITerminalView& terminalView,
    IInput& terminalInput,
    UserInputManager& userInputManager,
    ThreeWireService& threeWireService,
    ArgTransformer& argTransformer,
    ThreeWireEepromShell& threeWireEepromShell)
  : terminalView(terminalView),
    terminalInput(terminalInput),
    userInputManager(userInputManager),
    threeWireService(threeWireService),
    argTransformer(argTransformer),
    threeWireEepromShell(threeWireEepromShell) {}

/*
Entry point for command
*/
void ThreeWireController::handleCommand(const TerminalCommand& cmd) {
    if      (cmd.getRoot() == "config")  handleConfig();
    else if (cmd.getRoot() == "eeprom")  handleEeprom(cmd);
    else                                 handleHelp();
}

/*
Entry point for instructions
*/
void ThreeWireController::handleInstruction(const std::vector<ByteCode>& bytecodes) {
    terminalView.println("指令处理功能尚未实现"); // 汉化
}

/*
EEPROM
*/
void ThreeWireController::handleEeprom(const TerminalCommand& cmd) {
    threeWireEepromShell.run();
}

/*
Help
*/
void ThreeWireController::handleHelp() {
    terminalView.println("未知的3WIRE命令。使用方法："); // 汉化
    terminalView.println("  eeprom");
    terminalView.println("  config");
}

/*
Config
*/
void ThreeWireController::handleConfig() {
    terminalView.println("3WIRE 配置："); // 汉化
    
    // Pins
    const auto& forbidden = state.getProtectedPins();
    uint8_t cs = userInputManager.readValidatedPinNumber("CS引脚", state.getThreeWireCsPin(), forbidden); // 汉化
    state.setThreeWireCsPin(cs);
    uint8_t sk = userInputManager.readValidatedPinNumber("SK引脚", state.getThreeWireSkPin(), forbidden); // 汉化
    state.setThreeWireSkPin(sk);
    uint8_t di = userInputManager.readValidatedPinNumber("DI引脚", state.getThreeWireDiPin(), forbidden); // 汉化
    state.setThreeWireDiPin(di);
    uint8_t doPin = userInputManager.readValidatedPinNumber("DO引脚", state.getThreeWireDoPin(), forbidden); // 汉化
    state.setThreeWireDoPin(doPin);

    // Configure the service, default value for eeprom
    threeWireService.configure(cs, sk, di, doPin, 46, state.isThreeWireOrg8());
    terminalView.println("3WIRE 配置完成。\n"); // 汉化
    configured = true;
}

/*
Ensure configured
*/
void ThreeWireController::ensureConfigured() {
    if (!configured) {
        handleConfig();
        configured = true;
    }

    // Pins could have been used elsewhere, reconfigure the service
    auto cs = state.getThreeWireCsPin();
    auto sk = state.getThreeWireSkPin();
    auto di = state.getThreeWireDiPin();
    auto doPin = state.getThreeWireDoPin();
    auto modelId = state.getThreeWireEepromModelIndex(); 
    auto org8 = state.isThreeWireOrg8();
    threeWireService.configure(cs, sk, di, doPin, modelId, org8);
}