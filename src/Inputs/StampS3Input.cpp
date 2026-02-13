#ifdef DEVICE_M5STAMPS3

#include "Inputs/StampS3Input.h"
#include "Inputs/InputKeys.h"

// 构造函数：初始化M5StampS3硬件驱动
StampS3Input::StampS3Input() {
    M5.begin(); // 初始化M5StampS3核心硬件（包括按键、显示屏等）
}

// 按键映射：检测板载BtnA按键状态并映射为对应的按键码
char StampS3Input::mapButton() {
    M5.update(); // 更新M5StampS3硬件状态（必须调用以获取最新按键状态）

    // 检测BtnA按键是否被按下（单次按下触发，自动消抖）
    if (M5.BtnA.wasPressed()) return KEY_OK;

    // 无按键按下时返回无按键标识
    return KEY_NONE;
}

// 读取按键字符：封装按键映射逻辑，对外提供统一的按键读取接口
char StampS3Input::readChar() {
    return mapButton();
}

// 按键处理：阻塞等待BtnA按键按下，直到检测到按键后返回对应的按键码
char StampS3Input::handler() {
    char c = KEY_NONE;
    // 循环检测按键状态，直到BtnA按键被按下
    while ((c = mapButton()) == KEY_NONE) {
        delay(10); // 短延时消抖，降低CPU占用率，避免频繁检测
    }
    return c;
}

// 等待按键按下：阻塞等待BtnA按键按下，无返回值
void StampS3Input::waitPress() {
    // 循环检测按键状态，直到BtnA按键被按下
    while (mapButton() == KEY_NONE) {
        delay(10); // 短延时消抖，降低CPU占用率，避免频繁检测
    }
}

#endif