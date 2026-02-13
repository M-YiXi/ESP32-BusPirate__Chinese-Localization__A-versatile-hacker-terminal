#pragma once

#include <Interfaces/IInput.h>
#include <Arduino.h>
#include <vector>

class SerialTerminalInput : public IInput {
public:
    char handler() override;
    void waitPress() override;
    char readChar() override;
};
