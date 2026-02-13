#include "SerialTerminalInput.h"
#include <Arduino.h>

// 串口按键处理：阻塞等待串口接收到数据，读取并返回第一个字符
char SerialTerminalInput::handler() {
    // 循环等待，直到串口缓冲区有可用数据
    while (!Serial.available()) {}
    // 读取并返回串口接收到的字符
    return Serial.read();
}

// 等待串口按键按下：阻塞等待串口数据，读取后丢弃（仅检测按键动作，不使用数据）
void SerialTerminalInput::waitPress() {
    // 循环等待，直到串口缓冲区有可用数据
    while (!Serial.available()) {}
    Serial.read(); // 读取数据并丢弃，仅完成“等待按下”的逻辑
}

// 读取串口字符：非阻塞方式读取串口字符，无数据时返回无按键标识
char SerialTerminalInput::readChar() {
    // 检查串口缓冲区是否有可用数据
    if (Serial.available()) {
        // 有数据时读取并返回字符
        return Serial.read();
    }
    // 无数据时返回无按键标识
    return KEY_NONE;
}