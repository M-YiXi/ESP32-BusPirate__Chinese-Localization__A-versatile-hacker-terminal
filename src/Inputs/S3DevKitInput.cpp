#include "S3DevKitInput.h"
#include "InputKeys.h"

// BOOT按键引脚定义（S3 DevKit开发板的默认BOOT按键对应GPIO0）
#define BOOT_BUTTON_PIN 0

// 构造函数：初始化BOOT按键引脚为上拉输入模式
S3DevKitInput::S3DevKitInput() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
}

// 按键映射：检测BOOT按键状态并映射为对应的按键码
char S3DevKitInput::mapButton() {
    // BOOT按键按下时（低电平）返回确认键码
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        return KEY_OK;
    }
    // 无按键按下时返回无按键标识
    return KEY_NONE;
}

// 读取按键字符：封装按键映射逻辑，对外提供统一的按键读取接口
char S3DevKitInput::readChar() {
    return mapButton();
}

// 按键处理：阻塞等待BOOT按键按下，直到检测到按键后返回对应的按键码
char S3DevKitInput::handler() {
    char c = KEY_NONE;
    // 循环检测按键，直到检测到BOOT按键按下
    while ((c = mapButton()) == KEY_NONE) {
        delay(5); // 短延时消抖，避免误触发
    }
    return c;
}

// 等待按键按下：阻塞等待任意BOOT按键按下，无返回值
void S3DevKitInput::waitPress() {
    // 循环检测按键，直到检测到BOOT按键按下
    while (mapButton() == KEY_NONE) {
        delay(5); // 短延时消抖，避免误触发
    }
}