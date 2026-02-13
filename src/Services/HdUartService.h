#pragma once

#include <vector>
#include <Arduino.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "hal/uart_types.h"
#include "soc/uart_periph.h"
#include "Models/ByteCode.h"

#define HD_UART_PORT UART_NUM_2
#define UART_RX_BUFFER_SIZE 256

class HdUartService {
public:
    void configure(unsigned long baud, uint8_t dataBits, char parity, uint8_t stopBits, uint8_t ioPin, bool inverted);
    void write(uint8_t data);
    void write(const std::string& str);
    bool available() const;
    char read();
    std::string readLine();
    std::string executeByteCode(const std::vector<ByteCode>& bytecodes);
    void flush();
    uart_config_t buildUartConfig(unsigned long baud, uint8_t bits, char parity, uint8_t stop);
    void end();

private:
    uint8_t ioPin;
    unsigned long baudRate;
    uint32_t serialConfig;
    bool isInverted;

};
