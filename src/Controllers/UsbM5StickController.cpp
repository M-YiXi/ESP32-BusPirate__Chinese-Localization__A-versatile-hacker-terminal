#ifdef DEVICE_M5STICK

#include "Controllers/UsbM5StickController.h"

UsbM5StickController::UsbM5StickController(ITerminalView& terminalView)
    : terminalView(terminalView) {}

void UsbM5StickController::handleCommand(const TerminalCommand&) {
    terminalView.println("\n  [提示] M5StickC Plus 2 设备不支持USB功能。"); // 汉化
    terminalView.println("");
}

void UsbM5StickController::ensureConfigured() {
    terminalView.println("\n  [提示] 跳过USB配置，M5StickC Plus 2 设备不支持该功能。\n"); // 汉化
    terminalView.println("");
}

#endif