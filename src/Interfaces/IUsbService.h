#pragma once

#include <string>

class IUsbService {
public:
    virtual ~IUsbService() = default;

    // HID Keyboard
    virtual void keyboardBegin() = 0;
    virtual void keyboardSendString(const std::string& text) = 0;
    virtual void keyboardSendChunkedString(const std::string& data, size_t chunkSize, unsigned long delayBetweenChunks) = 0;

    // HID Mouse
    virtual void mouseBegin() = 0;
    virtual void mouseMove(int x, int y) = 0;
    virtual void mouseClick(int button) = 0;
    virtual void mouseRelease(int button) = 0;

    // HID Gamepad
    virtual void gamepadBegin() = 0;
    virtual void gamepadPress(const std::string& name) = 0;

    // Mass Storage
    virtual void storageBegin(uint8_t cs, uint8_t clk, uint8_t miso, uint8_t mosi) = 0;

    // Status
    virtual bool isKeyboardActive() const = 0;
    virtual bool isStorageActive() const = 0;
    virtual bool isMouseActive() const = 0;
    virtual bool isGamepadActive() const = 0;

    // Reset state
    virtual void reset() = 0;
};
