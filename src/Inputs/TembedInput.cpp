#if defined(DEVICE_TEMBEDS3) || defined(DEVICE_TEMBEDS3CC1101)

#include "TembedInput.h"
#include "Inputs/InputKeys.h"
#include <esp_sleep.h>
#include <Arduino.h>

// 构造函数：初始化旋转编码器、按键引脚和状态变量
TembedInput::TembedInput()
    // 初始化旋转编码器（A/B相引脚，锁存模式为TWO03）
    : encoder(TEMBED_PIN_ENCODE_A, TEMBED_PIN_ENCODE_B, RotaryEncoder::LatchMode::TWO03),
      lastInput(KEY_NONE),    // 上一次输入的按键码（初始化为无按键）
      lastPos(0),             // 编码器上一次的位置值（初始化为0）
      lastButton(false),      // 编码器按键上一次的状态（初始化为未按下）
      pressStart(0)           // 按键按下起始时间（用于长按检测）
{
    encoder.setPosition(0); // 重置编码器位置为初始值0
    // 配置编码器按键引脚为上拉输入模式
    pinMode(TEMBED_PIN_ENCODE_BTN, INPUT_PULLUP);
    // 配置侧边按键引脚为上拉输入模式
    pinMode(TEMBED_PIN_SIDE_BTN, INPUT_PULLUP);
}

// 状态更新：检测编码器旋转、按键按下状态，并检查关机请求
void TembedInput::tick() {
    encoder.tick(); // 更新编码器状态（必须调用以获取最新旋转位置）

    int pos = encoder.getPosition(); // 获取编码器当前位置值
    // 编码器向左旋转 → 记录左方向键按键码
    if (pos < lastPos) {
        lastInput = KEY_ARROW_LEFT;
        lastPos = pos;
    }
    // 编码器向右旋转 → 记录右方向键按键码
    else if (pos > lastPos) {
        lastInput = KEY_ARROW_RIGHT;
        lastPos = pos;
    }
    // 编码器按键按下（低电平）且上一次状态为未按下 → 记录确认键按键码
    else if (!digitalRead(TEMBED_PIN_ENCODE_BTN) && !lastButton) {
        lastInput = KEY_OK;
        lastButton = true;
    }
    // 编码器按键释放（高电平）→ 重置按键状态
    else if (digitalRead(TEMBED_PIN_ENCODE_BTN)) {
        lastButton = false;
    }

    checkShutdownRequest(); // 检测是否长按编码器/侧边按键触发关机
}

// 读取输入字符：非阻塞方式读取最新的输入按键码，读取后清空状态
char TembedInput::readChar() {
    tick(); // 先更新输入状态
    char c = lastInput; // 保存当前输入的按键码
    lastInput = KEY_NONE; // 清空输入状态，避免重复读取
    return c;
}

// 输入处理：阻塞等待任意有效输入，检测到后返回对应的按键码
char TembedInput::handler() {
    while (true) {
        char c = readChar();
        // 检测到有效按键码时返回
        if (c != KEY_NONE) return c;
        delay(5); // 短延时降低CPU占用，避免高频轮询
    }
}

// 等待按键按下：阻塞等待任意有效输入，仅检测动作不返回按键码
void TembedInput::waitPress() {
    while (true) {
        // 检测到有效输入时退出等待
        if (readChar() != KEY_NONE) return;
        delay(5); // 短延时降低CPU占用，避免高频轮询
    }
}

// 检测关机请求：长按编码器按键/侧边按键3秒触发深度睡眠
void TembedInput::checkShutdownRequest() {
    // 检测编码器按键或侧边按键是否按下（低电平）
    if (!digitalRead(TEMBED_PIN_ENCODE_BTN) || !digitalRead(TEMBED_PIN_SIDE_BTN)) {
        unsigned long start = millis(); // 记录按键按下起始时间

        // 等待3秒，持续检测按键是否保持按下状态
        for (int i = 3; i > 0; --i) {
            // 多次验证按键是否仍处于按下状态（消抖+防误触）
            for (int j = 0; j < 10; ++j) {
                // 任意按键释放则退出检测，不触发关机
                if (digitalRead(TEMBED_PIN_ENCODE_BTN) && digitalRead(TEMBED_PIN_SIDE_BTN)) return;
                delay(100); // 每100ms检测一次，总计3秒（3*10*100ms）
            }
        }

        // 若执行到此处，说明按键持续按下3秒 → 触发深度睡眠关机
        shutdownToDeepSleep();
    }
}

// 关机进入深度睡眠：执行关机流程并配置深度睡眠唤醒条件
void TembedInput::shutdownToDeepSleep() {
    view.shutDown(); // 执行设备关机界面/状态处理
    delay(3000); // 等待3秒，让用户看到关机提示
    // 配置外部唤醒源：侧边按键（低电平触发唤醒）
    esp_sleep_enable_ext0_wakeup((gpio_num_t)TEMBED_PIN_SIDE_BTN, 0);
    // 进入深度睡眠模式（低功耗）
    esp_deep_sleep_start();
}

#endif