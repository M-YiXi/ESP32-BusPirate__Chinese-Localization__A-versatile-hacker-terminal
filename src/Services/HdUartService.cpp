#include "HdUartService.h"

void HdUartService::configure(unsigned long baud, uint8_t dataBits, char parity, uint8_t stopBits, uint8_t ioPin, bool inverted) {
    // 从原始参数构建UART配置
    uart_config_t uart_config = buildUartConfig(baud, dataBits, parity, stopBits);

    // 配置GPIO为开漏模式
    gpio_config_t gpioConf = {
        .pin_bit_mask = (1ULL << ioPin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&gpioConf);

    // 配置信号反转
    if (isInverted) {
        uart_set_line_inverse(HD_UART_PORT, UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV);
    } else {
        uart_set_line_inverse(HD_UART_PORT, UART_SIGNAL_INV_DISABLE);
    }

    // 应用UART配置
    uart_driver_install(HD_UART_PORT, UART_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    uart_param_config(HD_UART_PORT, &uart_config);

    // 将UART信号路由到共享引脚
    esp_rom_gpio_connect_out_signal(ioPin, UART_PERIPH_SIGNAL(HD_UART_PORT, SOC_UART_TX_PIN_IDX), false, false);
    esp_rom_gpio_connect_in_signal(ioPin, UART_PERIPH_SIGNAL(HD_UART_PORT, SOC_UART_RX_PIN_IDX), false);
}

void HdUartService::end() {
    uart_driver_delete(HD_UART_PORT);
}

void HdUartService::write(uint8_t data) {
    uart_write_bytes(HD_UART_PORT, &data, 1);
    uart_wait_tx_done(HD_UART_PORT, pdMS_TO_TICKS(100));
}

void HdUartService::write(const std::string& str) {
    uart_write_bytes(HD_UART_PORT, str.c_str(), str.length());
    uart_wait_tx_done(HD_UART_PORT, pdMS_TO_TICKS(100));
}

bool HdUartService::available() const {
    size_t len = 0;
    uart_get_buffered_data_len(HD_UART_PORT, &len);
    return len > 0;
}

char HdUartService::read() {
    uint8_t c;
    int len = uart_read_bytes(HD_UART_PORT, &c, 1, pdMS_TO_TICKS(10));
    return (len == 1) ? static_cast<char>(c) : '\0';
}

std::string HdUartService::executeByteCode(const std::vector<ByteCode>& bytecodes) {
    std::string result;
    uint32_t timeout = 2000;  // 默认超时时间：2000毫秒
    uint32_t received = 0;     // 已接收字节数
    uint32_t start;            // 计时起始时间

    for (const ByteCode& code : bytecodes) {
        switch (code.getCommand()) {
            case ByteCodeEnum::Write:
                // 重复发送指定字节
                for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                    write(code.getData());
                }
                break;

            case ByteCodeEnum::Read:
                // 读取指定数量的字节，超时则停止
                start = millis();
                while (received < code.getRepeat() && (millis() - start < timeout)) {
                    if (available()) {
                        result += read();
                        ++received;
                    } else {
                        delay(1);
                    }
                }
                break;

            case ByteCodeEnum::DelayMs:
                // 毫秒级延时
                delay(code.getRepeat());
                break;

            case ByteCodeEnum::DelayUs:
                // 微秒级延时
                delayMicroseconds(code.getRepeat());
                break;

            default:
                // 未知指令，忽略
                break;
        }
    }

    return result;
}

uart_config_t HdUartService::buildUartConfig(unsigned long baud, uint8_t bits, char parity, uint8_t stop) {
    uart_word_length_t dataBits;
    uart_parity_t parityMode;
    uart_stop_bits_t stopBits;

    // 数据位配置
    switch (bits) {
        case 5: dataBits = UART_DATA_5_BITS; break;
        case 6: dataBits = UART_DATA_6_BITS; break;
        case 7: dataBits = UART_DATA_7_BITS; break;
        case 8:
        default: dataBits = UART_DATA_8_BITS; break;  // 默认8位数据位
    }

    // 校验位配置
    switch (parity) {
        case 'E': parityMode = UART_PARITY_EVEN; break;    // 偶校验
        case 'O': parityMode = UART_PARITY_ODD; break;     // 奇校验
        default:  parityMode = UART_PARITY_DISABLE; break; // 默认禁用校验
    }

    // 停止位配置
    stopBits = (stop == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1;  // 默认1位停止位

    // 组装UART配置结构体
    uart_config_t config = {
        .baud_rate = static_cast<int>(baud),        // 波特率
        .data_bits = dataBits,                      // 数据位
        .parity = parityMode,                       // 校验位
        .stop_bits = stopBits,                      // 停止位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      // 禁用硬件流控
        .source_clk = UART_SCLK_APB                 // 时钟源：APB时钟
    };

    return config;
}