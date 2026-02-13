#pragma once

#ifdef DEVICE_M5STICK

#include "Interfaces/IUsbService.h"

class UsbM5StickService : public IUsbService {
public:
    void keyboardBegin() override;
    void keyboardSendString(const std::string&) override;
    void keyboardSendChunkedString(const std::string&, size_t, unsigned long) override;

    void mouseBegin() override;
    void mouseMove(int, int) override;
    void mouseClick(int) override;
    void mouseRelease(int) override;

    void gamepadBegin() override;
    void gamepadPress(const std::string&) override;

    void storageBegin(uint8_t, uint8_t, uint8_t, uint8_t) override;

    bool isKeyboardActive() const override;
    bool isStorageActive() const override;
    bool isMouseActive() const override;
    bool isGamepadActive() const override;

    void reset() override;
};

#endif