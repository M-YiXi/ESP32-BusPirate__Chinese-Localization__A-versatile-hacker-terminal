#pragma once

#ifdef DEVICE_M5STAMPS3

#include "Interfaces/IInput.h"
#include <M5Unified.h>

class StampS3Input : public IInput {
public:
    StampS3Input();

    char readChar() override;
    char handler() override;
    void waitPress() override;

private:
    char mapButton();
};

#endif