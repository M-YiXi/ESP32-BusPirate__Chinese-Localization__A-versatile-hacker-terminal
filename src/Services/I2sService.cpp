#include "I2sService.h"
#include "math.h"

#ifdef DEVICE_CARDPUTER
    #include <M5Unified.h>
#endif

void I2sService::configureOutput(uint8_t bclk, uint8_t lrck, uint8_t dout, uint32_t sampleRate, uint8_t bits) {
    if (initialized) {
        // 卸载已安装的I2S驱动
        i2s_driver_uninstall(port);

        // 释放之前使用的引脚
        if (prevBclk != GPIO_NUM_NC) gpio_matrix_out(prevBclk, SIG_GPIO_OUT_IDX, false, false);
        if (prevLrck != GPIO_NUM_NC) gpio_matrix_out(prevLrck, SIG_GPIO_OUT_IDX, false, false);
        if (prevDout != GPIO_NUM_NC) gpio_matrix_out(prevDout, SIG_GPIO_OUT_IDX, false, false);
    }

    #ifdef DEVICE_CARDPUTER
        // 停止麦克风，启动扬声器
        M5.Mic.end();
        M5.Speaker.begin();
    #endif

    // 配置I2S输出参数
    i2s_config_t config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),          // 主模式+发送模式
        .sample_rate = sampleRate,                                    // 采样率
        .bits_per_sample = (i2s_bits_per_sample_t)(bits),             // 采样位宽
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                 // 左右声道
        .communication_format = I2S_COMM_FORMAT_I2S,                  // I2S标准格式
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                     // 中断优先级
        .dma_buf_count = 4,                                           // DMA缓冲区数量
        .dma_buf_len = 256,                                           // 单个DMA缓冲区长度
        .use_apll = false,                                            // 不使用APLL时钟
        .tx_desc_auto_clear = true,                                   // 自动清理发送描述符
        .fixed_mclk = 0                                               // 不使用固定MCLK
    };

    // 安装I2S驱动
    esp_err_t err = i2s_driver_install(port, &config, 0, nullptr);

    // 配置引脚为输出模式
    pinMode(bclk, OUTPUT);
    pinMode(lrck, OUTPUT);
    pinMode(dout, OUTPUT);

    // 手动映射引脚避免冲突
    gpio_matrix_out(bclk, I2S0O_BCK_OUT_IDX, false, false);
    gpio_matrix_out(lrck, I2S0O_WS_OUT_IDX, false, false);
    #ifdef DEVICE_M5STICK
        gpio_matrix_out(dout, I2S0O_DATA_OUT0_IDX, false, false);
    #else
        gpio_matrix_out(dout, I2S0O_SD_OUT_IDX, false, false);
    #endif

    // 保存引脚信息，供下次配置时释放
    prevBclk = bclk;
    prevLrck = lrck;
    prevDout = dout;

    initialized = true;
}

void I2sService::configureInput(uint8_t bclk, uint8_t lrck, uint8_t din, uint32_t sampleRate, uint8_t bits) {
    if (initialized) {
        // 卸载已安装的I2S驱动
        i2s_driver_uninstall(port);
    }

    #ifdef DEVICE_CARDPUTER
        // 停止扬声器，启动麦克风
        M5.Speaker.end();
        M5.Mic.begin();
    #endif

    // 配置I2S输入参数
    i2s_config_t config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),          // 主模式+接收模式
        .sample_rate = sampleRate,                                    // 采样率
        .bits_per_sample = (i2s_bits_per_sample_t)(bits),             // 采样位宽
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,                  // 仅左声道
        .communication_format = I2S_COMM_FORMAT_I2S,                  // I2S标准格式
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                     // 中断优先级
        .dma_buf_count = 4,                                           // DMA缓冲区数量
        .dma_buf_len = 256,                                           // 单个DMA缓冲区长度
        .use_apll = false,                                            // 不使用APLL时钟
        .tx_desc_auto_clear = false,                                  // 关闭发送描述符自动清理
        .fixed_mclk = 0                                               // 不使用固定MCLK
    };

    // 配置I2S引脚映射
    i2s_pin_config_t pins = {
        .bck_io_num = bclk,           // BCLK引脚
        .ws_io_num = lrck,            // LRCK引脚
        .data_out_num = I2S_PIN_NO_CHANGE,  // 输出引脚不变
        .data_in_num = din            // 输入数据引脚
    };

    // 安装驱动并设置引脚
    i2s_driver_install(port, &config, 0, nullptr);
    i2s_set_pin(port, &pins);

    initialized = true;
}

void I2sService::playTone(uint32_t sampleRate, uint16_t freq, uint16_t durationMs) {
    if (!initialized) return;

    // 计算总采样点数
    int samples = (sampleRate * durationMs) / 1000;
    int16_t buffer[2];  // 双声道缓冲区

    // 生成正弦波并发送
    for (int i = 0; i < samples; ++i) {
        float t = (float)i / sampleRate;
        int16_t sample = sinf(2.0f * PI * freq * t) * 32767;  // 生成正弦波采样值
        buffer[0] = sample;  // 左声道
        buffer[1] = sample;  // 右声道
        size_t written;
        i2s_write(port, buffer, sizeof(buffer), &written, portMAX_DELAY);
    }
}

void I2sService::playToneInterruptible(uint32_t sampleRate, uint16_t freq, uint32_t durationMs, std::function<bool()> shouldStop) {
    const int16_t amplitude = 32767;          // 最大振幅（16位）
    const int chunkDurationMs = 20;           // 每块音频时长（毫秒）
    const int samplesPerChunk = (sampleRate * chunkDurationMs) / 1000;  // 每块采样点数
    int16_t buffer[2 * samplesPerChunk];      // 双声道块缓冲区

    uint32_t elapsed = 0;                     // 已播放时长
    while (elapsed < durationMs) {
        // 生成当前块的正弦波数据
        for (int i = 0; i < samplesPerChunk; ++i) {
            float t = (float)(i + (elapsed * sampleRate / 1000)) / sampleRate;
            int16_t sample = sinf(2.0f * PI * freq * t) * amplitude;
            buffer[2 * i] = sample;    // 左声道
            buffer[2 * i + 1] = sample;// 右声道
        }

        // 发送音频数据
        size_t written;
        i2s_write(port, buffer, sizeof(buffer), &written, portMAX_DELAY);
        elapsed += chunkDurationMs;

        // 检查是否需要停止播放
        if (shouldStop()) {
            break;
        }
    }
}

void I2sService::playPcm(const int16_t* data, size_t numBytes) {
    if (!initialized || data == nullptr || numBytes == 0) return;

    // 播放PCM音频数据
    size_t bytesWritten = 0;
    i2s_write(port, data, numBytes, &bytesWritten, portMAX_DELAY);
}

size_t I2sService::recordSamples(int16_t* outBuffer, size_t sampleCount) {
    if (!initialized) return 0;

    // 计算需要读取的字节数
    size_t totalRead = 0;
    size_t bytesToRead = sampleCount * sizeof(int16_t);

    // 读取采样数据到缓冲区
    uint8_t* buffer = reinterpret_cast<uint8_t*>(outBuffer);
    while (totalRead < bytesToRead) {
        size_t readBytes = 0;
        i2s_read(port, buffer + totalRead, bytesToRead - totalRead, &readBytes, portMAX_DELAY);
        totalRead += readBytes;
    }

    // 返回实际读取的采样点数
    return totalRead / sizeof(int16_t);
}

void I2sService::end() {
    if (initialized) {
        // 卸载I2S驱动
        i2s_driver_uninstall(port);
        initialized = false;
    }
}

bool I2sService::isInitialized() const {
    return initialized;
}