#ifdef DEVICE_M5STICK

#include "UsbM5StickService.h"

void UsbM5StickService::keyboardBegin() {}
void UsbM5StickService::keyboardSendString(const std::string&) {}
void UsbM5StickService::keyboardSendChunkedString(const std::string&, size_t, unsigned long) {}

void UsbM5StickService::mouseBegin() {}
void UsbM5StickService::mouseMove(int, int) {}
void UsbM5StickService::mouseClick(int) {}
void UsbM5StickService::mouseRelease(int) {}

void UsbM5StickService::gamepadBegin() {}
void UsbM5StickService::gamepadPress(const std::string&) {}

void UsbM5StickService::storageBegin(uint8_t, uint8_t, uint8_t, uint8_t) {}

bool UsbM5StickService::isKeyboardActive() const { return false; }
bool UsbM5StickService::isStorageActive() const { return false; }
bool UsbM5StickService::isMouseActive() const { return false; }
bool UsbM5StickService::isGamepadActive() const { return false; }

void UsbM5StickService::reset() {}


#endif