#ifdef DEVICE_CARDPUTER

#include "CardputerInput.h"


char CardputerInput::handler() {
    while(true) {
        // 更新键盘状态
        M5Cardputer.update();
        
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        
        // 用于滚动的特殊方向键（上），支持长按
        if (!status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_UP)) {
            delay(50); // 消抖延时
            return CARDPUTER_SPECIAL_ARROW_UP;
        }
        // 用于滚动的特殊方向键（下），支持长按
        if (!status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_DOWN)) {
            delay(50); // 消抖延时
            return CARDPUTER_SPECIAL_ARROW_DOWN;
        }
    
        // 无按键状态变化时跳过
        if (!M5Cardputer.Keyboard.isChange()) {
            continue;
        }
    
        // 无按键按下时跳过
        if (!M5Cardputer.Keyboard.isPressed()) {
            continue;
        }
        
        // 组合键（Fn+上方向键）返回标准上方向键
        if (status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_UP)) return KEY_ARROW_UP;
        // 组合键（Fn+下方向键）返回标准下方向键
        if (status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_DOWN)) return KEY_ARROW_DOWN;
        // 回车键
        if (status.enter) return KEY_OK;
        // 删除键
        if (status.del) return KEY_DEL;

        // 遍历按下的字符键
        for (auto c : status.word) {
            // %符号存在兼容问题：该符号需要两次输入才能正常显示，此处做兼容处理
            if (c == '%') {
                return '5'; 
            }
            return c; // 返回第一个按下的字符
        }
    
        delay(10); // 消抖延时
        return KEY_NONE;
    }
}

// 等待任意按键按下
void CardputerInput::waitPress() {
  while(true){
    M5Cardputer.update();
    // 检测到按键状态变化且有按键按下时退出等待
    if (M5Cardputer.Keyboard.isChange()) {
      if (M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        return;
      }
    }
    delay(10); // 轮询延时
  }
}

// 读取单个按键字符
char CardputerInput::readChar() {
    M5Cardputer.update();
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    
    // 用于滚动的特殊方向键（上），支持长按
    if (!status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_UP)) {
        delay(50); // 消抖延时
        return CARDPUTER_SPECIAL_ARROW_UP;
    }
    // 用于滚动的特殊方向键（下），支持长按
    if (!status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_DOWN)) {
        delay(50); // 消抖延时
        return CARDPUTER_SPECIAL_ARROW_DOWN;
    }

    // 无按键状态变化时返回无按键标识
    if (!M5Cardputer.Keyboard.isChange()) return KEY_NONE;
    // 无按键按下时返回无按键标识
    if (!M5Cardputer.Keyboard.isPressed()) return KEY_NONE;

    // 修复;和.按键映射颠倒的问题（Fn+方向键）
    if (status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_UP)) return KEY_ARROW_UP;
    if (status.fn && M5Cardputer.Keyboard.isKeyPressed(KEY_ARROW_DOWN)) return KEY_ARROW_DOWN;

    // 回车键
    if (status.enter) return KEY_OK;
    // 删除键
    if (status.del) return KEY_DEL;
    // Tab键
    if (status.tab) return KEY_TAB_CUSTOM;

    // 遍历按下的字符键
    for (auto c : status.word) {
        delay(5); // 消抖延时
        
        // %符号存在兼容问题：该符号需要两次输入才能正常显示，此处做兼容处理
        if (c == '%') {
            return '5'; 
        }
        return c; // 返回第一个按下的字符
    }

    return KEY_NONE;
}

#endif