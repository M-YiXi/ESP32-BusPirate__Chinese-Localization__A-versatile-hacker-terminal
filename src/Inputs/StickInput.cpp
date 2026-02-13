#if defined(DEVICE_M5STICK) || defined(DEVICE_STICKS3)

#include "StickInput.h"

// 构造函数：初始化M5Stick/StickS3硬件驱动
StickInput::StickInput() {
    M5.begin(); // 初始化M5Stick/StickS3核心硬件（按键、显示屏、电源管理等）
}

// 按键映射：将板载物理按键映射为对应的功能按键码
char StickInput::mapButton() {
    M5.update(); // 更新M5硬件状态（必须调用，否则无法获取最新按键状态）

    // BtnA按键（确认键）按下 → 返回确认按键码
    if (M5.BtnA.wasPressed()) return KEY_OK;
    // BtnB按键按下 → 返回左方向键按键码
    if (M5.BtnB.wasPressed()) return KEY_ARROW_LEFT;
    // 电源按键（BtnPWR）按下 → 返回右方向键按键码
    if (M5.BtnPWR.wasPressed()) return KEY_ARROW_RIGHT;

    // 无按键按下时返回无按键标识
    return KEY_NONE;
}

// 读取按键字符：封装按键映射逻辑，对外提供统一的按键读取接口
char StickInput::readChar() {
    return mapButton();
}

// 按键处理：阻塞等待任意板载按键按下，直到检测到按键后返回对应的按键码
char StickInput::handler() {
    char c = KEY_NONE;
    // 循环检测按键状态，直到有任意按键被按下
    while ((c = mapButton()) == KEY_NONE) {
        delay(10); // 短延时消抖，降低CPU占用率，避免高频轮询
    }
    return c;
}

// 等待按键按下：阻塞等待任意板载按键按下，仅检测动作不返回按键码
void StickInput::waitPress() {
    // 循环检测按键状态，直到有任意按键被按下
    while (mapButton() == KEY_NONE) {
        delay(10); // 短延时消抖，降低CPU占用率，避免高频轮询
    }
}

#endif