#include "SubGhzService.h"
#include "driver/rmt.h"
#include <sstream>

// -------------------------- 基础配置与初始化 --------------------------

/**
 * @brief 配置CC1101模块的SPI引脚、射频参数并初始化
 * @param spi SPI总线实例
 * @param sck SPI时钟引脚
 * @param miso SPI MISO引脚
 * @param mosi SPI MOSI引脚
 * @param ss SPI片选引脚
 * @param gdo0 CC1101的GDO0引脚（数据输出/输入）
 * @param mhz 工作频率（MHz）
 * @param paDbm 发射功率（dBm）
 * @return 配置成功返回true，失败返回false
 * @note 适配不同硬件平台（TembedS3CC1101/M5Stick/Cardputer）的SPI初始化逻辑
 */
bool SubGhzService::configure(SPIClass& spi, uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t ss,
                              uint8_t gdo0, float mhz, int paDbm)
{
    // 赋值硬件参数
    sck_ = sck; 
    miso_ = miso; 
    mosi_ = mosi; 
    ss_ = ss;
    gdo0_ = gdo0;
    mhz_ = mhz;
    paDbm_ = paDbm;
    ccMode_ = true;

    // SPI实例适配（不同硬件平台差异化处理）
    #ifdef DEVICE_TEMBEDS3CC1101
    initTembed();                  // 初始化Tembed S3硬件
    ELECHOUSE_cc1101.setSPIinstance(&spi);
    #elif defined(DEVICE_M5STICK) || defined(DEVICE_CARDPUTER)
    SPI.end();                     // 停止原有SPI总线
    delay(10);                     // 延迟稳定
    SPI.begin(sck_, miso_, mosi_, ss_); // 重新初始化指定引脚的SPI
    ELECHOUSE_cc1101.setSPIinstance(&SPI);
    #else
    ELECHOUSE_cc1101.setSPIinstance(&spi);
    #endif

    // 初始化CC1101核心参数
    ELECHOUSE_cc1101.setSpiPin(sck_, miso_, mosi_, ss_); // 设置SPI引脚
    ELECHOUSE_cc1101.setGDO0(gdo0_);                     // 设置GDO0引脚
    ELECHOUSE_cc1101.Init();                             // 初始化CC1101
    applyDefaultProfile();                               // 应用默认配置文件

    // 检查CC1101是否配置成功
    isConfigured_ = ELECHOUSE_cc1101.getCC1101();

    return isConfigured_;
}

/**
 * @brief 切换CC1101的工作频率
 * @param mhz 目标频率（MHz）
 * @note 适配Tembed硬件的射频路径切换，确保不同频段天线匹配
 */
void SubGhzService::tune(float mhz)
{
    if (!isConfigured_) return;
    mhz_ = mhz;
    ELECHOUSE_cc1101.SetRx(mhz_);  // 设置接收频率

    #ifdef DEVICE_TEMBEDS3CC1101
    selectRfPathFor(mhz_);         // Tembed硬件：根据频率选择射频路径
    #endif
 
    delay(2); // 延迟稳定频率
}

/**
 * @brief 测量指定时长内的峰值RSSI（接收信号强度）
 * @param holdMs 测量时长（毫秒）
 * @return 峰值RSSI值（dBm，范围-127~0，-127表示无信号）
 */
int SubGhzService::measurePeakRssi(uint32_t holdMs)
{
    if (!isConfigured_) return -127;
    if (holdMs < 1) holdMs = 1;    // 最小测量时长1ms

    const unsigned long t0 = millis();
    int peak = -127;               // 初始峰值设为最小
    while (millis() - t0 < holdMs) {
        int r = ELECHOUSE_cc1101.getRssi(); // 读取当前RSSI
        if (r > peak) peak = r;             // 更新峰值
        delay(1);
    }
    return peak;
}

/**
 * @brief 获取支持的频段列表
 * @return 频段名称向量（用户可见文本已汉化）
 */
std::vector<std::string> SubGhzService::getSupportedBand() const {
    return std::vector<std::string>(std::begin(kSubGhzScanBandNames), std::end(kSubGhzScanBandNames));
}

/**
 * @brief 获取指定频段下支持的所有频率
 * @param band 频段名称
 * @return 频率列表（MHz）
 */
std::vector<float> SubGhzService::getSupportedFreq(const std::string& band) const {
    std::vector<float> out;

    int bandIdx = -1;
    // 查找频段对应的索引
    for (int i = 0; i < 4; ++i) {
        if (band == kSubGhzScanBandNames[i]) {
            bandIdx = i;
            break;
        }
    }

    if (bandIdx < 0) {
        return out;
    }

    // 获取频段的频率范围边界
    int start = kSubGhzRangeLimits[bandIdx][0];
    int end   = kSubGhzRangeLimits[bandIdx][1];

    // 填充该频段下的所有频率
    out.reserve(static_cast<size_t>(end - start + 1));
    for (int i = start; i <= end; ++i) {
        out.push_back(kSubGhzFreqList[i]);
    }

    return out;
}

/**
 * @brief 设置扫描频段（支持名称/索引两种方式）
 * @param s 频段名称或索引字符串
 */
void SubGhzService::setScanBand(const std::string& s) {
    // 按名称匹配频段
    for (int i = 0; i < 4; ++i) {
        if (s == kSubGhzScanBandNames[i]) {
            scanBand_ = static_cast<SubGhzScanBand>(i);
            return;
        }
    }

    // 按索引匹配（字符串转数字）
    char* end = nullptr;
    long idx = std::strtol(s.c_str(), &end, 10);
    if (end != s.c_str()) {
        scanBand_ = static_cast<SubGhzScanBand>(idx);
    }
}

// -------------------------- 原始脉冲嗅探（RMT驱动） --------------------------

/**
 * @brief 启动原始脉冲嗅探（基于RMT驱动）
 * @param pin 嗅探引脚（连接CC1101的GDO0或直接接收射频信号）
 * @return 启动成功返回true，失败返回false
 * @note RMT配置：RX模式、滤波开启、空闲阈值3ms、滤波阈值200us
 */
bool SubGhzService::startRawSniffer(int pin) {
    // 配置RMT接收参数
    rmt_config_t rxconfig;
    rxconfig.rmt_mode = RMT_MODE_RX;                  // RX模式
    rxconfig.channel = RMT_RX_CHANNEL;                // 指定RMT通道
    rxconfig.gpio_num = (gpio_num_t)pin;              // 嗅探引脚
    rxconfig.clk_div = RMT_CLK_DIV;                   // 时钟分频
    rxconfig.mem_block_num = 2;                       // 内存块数量
    rxconfig.flags = 0;
    rxconfig.rx_config.idle_threshold = 3 * RMT_1MS_TICKS; // 空闲阈值（3ms）
    rxconfig.rx_config.filter_ticks_thresh = 200 * RMT_1US_TICKS; // 滤波阈值（200us）
    rxconfig.rx_config.filter_en = true;              // 开启滤波

    // 安装RMT驱动
    if (rmt_config(&rxconfig) != ESP_OK) return false;
    if (rmt_driver_install(rxconfig.channel, RMT_BUFFER_SIZE, 0) != ESP_OK) return false;

    // 获取环形缓冲区句柄并启动接收
    if (rmt_get_ringbuf_handle(rxconfig.channel, &rb_) != ESP_OK) return false;
    rmt_rx_start(rxconfig.channel, true);

    return true;
}

/**
 * @brief 检查嗅探器缓冲区是否溢出
 * @return 溢出返回true，正常返回false
 * @note 剩余空间小于128字节判定为溢出
 */
bool SubGhzService::isSnifferOverflowing() const {
    if (!rb_) return false;
    size_t freeBytes = xRingbufferGetCurFreeSize(rb_);
    return freeBytes < 128;
}

/**
 * @brief 清空嗅探器缓冲区（释放所有未处理的脉冲数据）
 */
void SubGhzService::drainSniffer() {
    if (!rb_) return;
    while (true) {
        size_t rx_size = 0;
        rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb_, &rx_size, 0);
        if (!item) break;          // 无数据则退出
        vRingbufferReturnItem(rb_, (void*)item); // 释放缓冲区
    }
}

/**
 * @brief 停止原始脉冲嗅探并释放RMT资源
 */
void SubGhzService::stopRawSniffer() {
    rmt_rx_stop(RMT_RX_CHANNEL);   // 停止RMT接收
    drainSniffer();                // 清空缓冲区
    rmt_driver_uninstall(RMT_RX_CHANNEL); // 卸载RMT驱动
    rb_ = nullptr;                 // 重置缓冲区句柄
    ELECHOUSE_cc1101.setSidle();   // CC1101进入空闲状态
}

/**
 * @brief 读取原始脉冲数据并格式化为用户可读字符串（核心汉化点）
 * @return 脉冲描述字符串 + 脉冲数量
 * @note 输出文本已完全汉化，包含脉冲数量、频率、总时长、高低电平及时长
 */
std::pair<std::string, size_t> SubGhzService::readRawPulses() {
    if (!rb_) return {"", 0};

    size_t rx_size = 0;
    rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb_, &rx_size, 0);
    if (!item) return {"", 0};

    size_t n = rx_size / sizeof(rmt_item32_t);
    uint32_t totalDuration = 0;
    // 计算总时长（时钟周期）
    for (size_t i = 0; i < n; i++) {
        totalDuration += item[i].duration0;
        totalDuration += item[i].duration1;
    }

    // 格式化输出（用户可见文本汉化）
    std::ostringstream oss;
    oss << "[原始 " << n << " 个脉冲 | 频率=" << mhz_
        << " MHz | 时长=" << totalDuration << " 时钟周期]\r\n";

    int col = 0;
    for (size_t i = 0; i < n; i++) {
        oss << (item[i].level0 ? '高' : '低') << ":" << item[i].duration0
            << " | "
            << (item[i].level1 ? '高' : '低') << ":" << item[i].duration1
            << "   ";
        if (++col % 4 == 0) oss << "\r\n"; // 每4个脉冲换行，提升可读性
    }
    oss << "\n\r";

    vRingbufferReturnItem(rb_, (void*)item); // 释放内存
    return {oss.str(), n};
}

/**
 * @brief 读取原始帧数据（未格式化的RMT脉冲结构体）
 * @return RMT脉冲结构体列表
 */
std::vector<rmt_item32_t> SubGhzService::readRawFrame() {
    std::vector<rmt_item32_t> frame;
    if (!rb_) return frame;

    size_t rx_size = 0;
    rmt_item32_t* item = (rmt_item32_t*) xRingbufferReceive(rb_, &rx_size, 0);
    if (!item) return frame;

    size_t n = rx_size / sizeof(rmt_item32_t);
    frame.assign(item, item + n); // 复制脉冲数据

    vRingbufferReturnItem(rb_, (void*)item);
    return frame;
}

/**
 * @brief 发送原始帧数据（基于BitBang方式）
 * @param pin 发送引脚
 * @param items RMT脉冲结构体列表
 * @param tick_per_us 时钟周期/微秒（用于转换时长）
 * @return 发送成功返回true，失败返回false
 */
bool SubGhzService::sendRawFrame(int pin, const std::vector<rmt_item32_t>& items, uint32_t tick_per_us) {
    if (!isConfigured_ || items.empty()) return false;

    // 启动TX BitBang模式（引脚设为输出）
    if (!startTxBitBang()) return false;

    // 时钟周期转微秒的工具函数
    auto ticks_to_us = [tick_per_us](uint32_t ticks) -> uint32_t {
        if (!tick_per_us) return 0;
        return (ticks + (tick_per_us/2)) / tick_per_us;
    };

    // 逐脉冲发送（BitBang）
    for (const auto& it : items) {
        gpio_set_level((gpio_num_t)pin, it.level0 ? 1 : 0); // 设置电平
        uint32_t us0 = ticks_to_us(it.duration0);           // 转换时长
        if (us0) esp_rom_delay_us(us0);                     // 保持电平

        gpio_set_level((gpio_num_t)pin, it.level1 ? 1 : 0);
        uint32_t us1 = ticks_to_us(it.duration1);
        if (us1) esp_rom_delay_us(us1);
    }

    // 重置引脚为低电平
    gpio_set_level((gpio_num_t)pin, 0);
    return true;
}

/**
 * @brief 启动TX BitBang模式（配置引脚为输出）
 * @return 配置成功返回true，失败返回false
 */
bool SubGhzService::startTxBitBang() {
    if (!isConfigured_) return false;

    gpio_config_t io{};
    io.pin_bit_mask = (1ULL << gdo0_);          // 引脚掩码
    io.mode = GPIO_MODE_OUTPUT;                 // 输出模式
    io.pull_up_en = GPIO_PULLUP_DISABLE;        // 禁用上拉
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;    // 禁用下拉
    io.intr_type = GPIO_INTR_DISABLE;           // 禁用中断
    return gpio_config(&io) == ESP_OK;
}

/**
 * @brief 停止TX BitBang模式（恢复引脚为输入）
 * @return 恢复成功返回true，失败返回false
 */
bool SubGhzService::stopTxBitBang() {
    if (!isConfigured_) return false;

    // 强制引脚为低电平
    gpio_set_direction((gpio_num_t)gdo0_, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)gdo0_, 0);
    delay(1);

    // 重置为输入模式（下拉使能）
    gpio_config_t io{};
    io.pin_bit_mask  = (1ULL << gdo0_);
    io.mode          = GPIO_MODE_INPUT; 
    io.pull_up_en    = GPIO_PULLUP_DISABLE;
    io.pull_down_en  = GPIO_PULLDOWN_ENABLE;
    io.intr_type     = GPIO_INTR_DISABLE;

    ELECHOUSE_cc1101.setSidle(); // CC1101进入空闲状态

    return gpio_config(&io) == ESP_OK;
}

/**
 * @brief 发送单个原始脉冲（指定时长和电平）
 * @param pin 发送引脚
 * @param duration 脉冲时长（正数=高电平，负数=低电平，单位微秒）
 * @return 发送成功返回true，失败返回false
 */
bool SubGhzService::sendRawPulse(int pin, int duration) {
    if (!isConfigured_) return false;

    if (duration < 0) {
        gpio_set_level((gpio_num_t)pin, 0);     // 低电平
        delayMicroseconds(-duration);           // 时长取反
    } else {
        gpio_set_level((gpio_num_t)pin, 1);     // 高电平
        delayMicroseconds(duration);
    }
    return true;
}

/**
 * @brief 发送随机脉冲串（用于干扰/测试）
 * @param pin 发送引脚
 * @return 发送成功返回true，失败返回false
 * @note 脉冲参数：256个脉冲、平均时长200us、抖动±30%
 */
bool SubGhzService::sendRandomBurst(int pin)
{
    if (!isConfigured_) return false;

    constexpr int ITEMS_PER_BURST = 256;   // 脉冲数量
    constexpr int MEAN_US         = 200;   // 平均时长（微秒/相位）
    constexpr int JITTER_PCT      = 30;    // 抖动百分比

    // 计算抖动上下限
    int j     = (MEAN_US * JITTER_PCT) / 100;
    int minUs = MEAN_US - j; if (minUs < 1) minUs = 1;
    int maxUs = MEAN_US + j; if (maxUs < minUs) maxUs = minUs;

    // 生成指定范围内随机数的工具函数
    auto rnd_between = [&](int lo, int hi) -> uint32_t {
        uint32_t span = static_cast<uint32_t>(hi - lo + 1);
        return static_cast<uint32_t>(lo) + (esp_random() % span);
    };

    // 初始随机电平
    int level = (esp_random() & 1) ? 1 : 0;

    // 逐脉冲发送（BitBang）
    for (int i = 0; i < ITEMS_PER_BURST; ++i) {
        // 相位0
        gpio_set_level((gpio_num_t)pin, level);
        level ^= 1; // 翻转电平
        esp_rom_delay_us(rnd_between(minUs, maxUs));

        // 相位1
        gpio_set_level((gpio_num_t)pin, level);
        level ^= 1;
        esp_rom_delay_us(rnd_between(minUs, maxUs));
    }

    // 重置引脚为低电平
    gpio_set_level((gpio_num_t)pin, 0);
    return true;
}

// -------------------------- 配置文件（Profile）管理 --------------------------

/**
 * @brief 应用扫描配置文件（自定义调制参数）
 * @param dataRateKbps 数据速率（kbps）
 * @param rxBwKhz 接收带宽（kHz）
 * @param modulation 调制方式（0=2FSK,1=GFSK,2=OOK/ASK等）
 * @param packetMode 是否启用数据包模式
 * @return 应用成功返回true，失败返回false
 */
bool SubGhzService::applyScanProfile(float dataRateKbps,
                                  float rxBwKhz,
                                  uint8_t modulation,
                                  bool packetMode)
{
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();                // 进入空闲状态
    ELECHOUSE_cc1101.setCCMode(packetMode ? 0 : 1); // 设置CC模式
    ELECHOUSE_cc1101.setModulation(modulation); // 设置调制方式
    ELECHOUSE_cc1101.setDRate(dataRateKbps);    // 设置数据速率
    ELECHOUSE_cc1101.setRxBW(rxBwKhz);          // 设置接收带宽
    ELECHOUSE_cc1101.setSyncMode(0);            // 关闭同步模式
    ELECHOUSE_cc1101.setWhiteData(false);       // 关闭数据白化
    ELECHOUSE_cc1101.setCrc(false);             // 关闭CRC校验
    ELECHOUSE_cc1101.setCRC_AF(false);          // 关闭CRC自动确认
    ELECHOUSE_cc1101.setAdrChk(0);              // 关闭地址检查
    ELECHOUSE_cc1101.setLengthConfig(1);        // 可变长度数据包
    ELECHOUSE_cc1101.setPacketLength(0xFF);     // 最大数据包长度
    ELECHOUSE_cc1101.SetRx(mhz_);               // 设置接收频率
    return true;
}

/**
 * @brief 应用默认配置文件（OOK/ASK调制，4.8kbps，135kHz带宽）
 * @param mhz 工作频率（MHz）
 * @return 应用成功返回true，失败返回false
 */
bool SubGhzService::applyDefaultProfile(float mhz) {
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();                // 进入空闲状态
    ELECHOUSE_cc1101.setPktFormat(0);           // 数据包格式：标准模式
    ELECHOUSE_cc1101.setLengthConfig(1);        // 可变长度
    ELECHOUSE_cc1101.setPacketLength(0xFF);     // 最大长度255
    ELECHOUSE_cc1101.setCCMode(0);              // RAW模式（GDO0输出原始数据）
    ELECHOUSE_cc1101.setMHZ(mhz);               // 设置频率
    ELECHOUSE_cc1101.setModulation(2);          // 调制方式：OOK/ASK
    ELECHOUSE_cc1101.setDRate(4.8f);            // 数据速率：4.8kbps
    ELECHOUSE_cc1101.setRxBW(135.0f);           // 接收带宽：135kHz
    ELECHOUSE_cc1101.setSyncMode(0);            // 关闭同步
    ELECHOUSE_cc1101.setWhiteData(false);       // 关闭数据白化
    ELECHOUSE_cc1101.setCrc(false);             // 关闭CRC
    ELECHOUSE_cc1101.setCRC_AF(false);          // 关闭CRC自动确认
    ELECHOUSE_cc1101.setAdrChk(0);              // 关闭地址检查

    return true;
}

/**
 * @brief 应用嗅探配置文件（OOK/ASK，50kbps，135kHz带宽）
 * @param mhz 工作频率（MHz）
 * @return 应用成功返回true，失败返回false
 */
bool SubGhzService::applySniffProfile(float mhz) {
    if (!isConfigured_) return false;
    // CC1101配置为"原始/异步"OOK模式
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(mhz);
    ELECHOUSE_cc1101.setModulation(2);      // 调制：ASK/OOK
    ELECHOUSE_cc1101.setDRate(50);          // 数据速率：50kbps
    ELECHOUSE_cc1101.setRxBW(135.0f);       // 接收带宽：135kHz
    ELECHOUSE_cc1101.setSyncMode(0);        // 无同步
    ELECHOUSE_cc1101.setWhiteData(0);       // 关闭数据白化
    ELECHOUSE_cc1101.setCrc(0);             // 关闭CRC
    ELECHOUSE_cc1101.setAdrChk(0);          // 关闭地址检查
    ELECHOUSE_cc1101.setDcFilterOff(true);  // 关闭直流滤波
    ELECHOUSE_cc1101.setPktFormat(3);       // 异步串行模式
    ELECHOUSE_cc1101.SetRx(mhz);            // 设置接收频率
    return true;
}

/**
 * @brief 应用原始发送配置文件（OOK/ASK，50kbps，异步串行模式）
 * @param mhz 工作频率（MHz）
 * @return 应用成功返回true，失败返回false
 */
bool SubGhzService::applyRawSendProfile(float mhz) {
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(mhz);
    ELECHOUSE_cc1101.setModulation(2);   // 调制：OOK/ASK
    ELECHOUSE_cc1101.setDRate(50);       // 数据速率：50kbps
    ELECHOUSE_cc1101.setRxBW(135.0f);    // 接收带宽：135kHz
    ELECHOUSE_cc1101.setSyncMode(0);     // 无同步
    ELECHOUSE_cc1101.setWhiteData(0);    // 关闭数据白化
    ELECHOUSE_cc1101.setCrc(0);          // 关闭CRC
    ELECHOUSE_cc1101.setAdrChk(0);       // 关闭地址检查
    ELECHOUSE_cc1101.setDcFilterOff(true); // 关闭直流滤波
    ELECHOUSE_cc1101.setPktFormat(3);    // 异步串行模式（GDO0=数据）
    ELECHOUSE_cc1101.SetTx();            // 设置为发送模式
    return true;
}

/**
 * @brief 按名称应用预设配置文件（支持多种主流亚GHz协议）
 * @param name 预设名称（如FuriHalSubGhzPresetOok270Async/RcSwitch协议号）
 * @param mhz 工作频率（MHz）
 * @return 应用成功返回true，失败返回false
 */
bool SubGhzService::applyPresetByName(const std::string& name, float mhz) {
    if (!isConfigured_) return false;
    ELECHOUSE_cc1101.setSidle();
    ELECHOUSE_cc1101.setMHZ(mhz);

    // 默认参数初始化
    ELECHOUSE_cc1101.setWhiteData(0);
    ELECHOUSE_cc1101.setCrc(0);
    ELECHOUSE_cc1101.setCRC_AF(0);
    ELECHOUSE_cc1101.setAdrChk(0);
    ELECHOUSE_cc1101.setSyncMode(0);

    // 适配FuriHal预设
    if (name == "FuriHalSubGhzPresetOok270Async") {
        ELECHOUSE_cc1101.setModulation(2); // OOK调制
        ELECHOUSE_cc1101.setRxBW(270.0f);  // 接收带宽270kHz
        ELECHOUSE_cc1101.setDRate(10.0f);  // 数据速率10kbps
        ELECHOUSE_cc1101.setPktFormat(3);  // 异步串行模式
        ELECHOUSE_cc1101.SetTx();          // 发送模式
        return true;
    }
    if (name == "FuriHalSubGhzPresetOok650Async") {
        ELECHOUSE_cc1101.setModulation(2);
        ELECHOUSE_cc1101.setRxBW(650.0f);
        ELECHOUSE_cc1101.setDRate(10.0f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPreset2FSKDev238Async") {
        ELECHOUSE_cc1101.setModulation(0); // 2-FSK调制
        ELECHOUSE_cc1101.setDeviation(2.380f); // 频偏2.38kHz
        ELECHOUSE_cc1101.setRxBW(238.0f);      // 接收带宽238kHz
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPreset2FSKDev476Async") {
        ELECHOUSE_cc1101.setModulation(0);
        ELECHOUSE_cc1101.setDeviation(47.607f);
        ELECHOUSE_cc1101.setRxBW(476.0f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPresetMSK99_97KbAsync") {
        ELECHOUSE_cc1101.setModulation(4); // MSK调制
        ELECHOUSE_cc1101.setDeviation(47.607f);
        ELECHOUSE_cc1101.setDRate(99.97f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }
    if (name == "FuriHalSubGhzPresetGFSK9_99KbAsync") {
        ELECHOUSE_cc1101.setModulation(1); // GFSK调制
        ELECHOUSE_cc1101.setDeviation(19.043f);
        ELECHOUSE_cc1101.setDRate(9.996f);
        ELECHOUSE_cc1101.setPktFormat(3);
        ELECHOUSE_cc1101.SetTx();
        return true;
    }

    // RcSwitch协议（按数字协议号匹配）
    char* end=nullptr;
    long proto = std::strtol(name.c_str(), &end, 10);
    if (end != name.c_str()) {
        ELECHOUSE_cc1101.setModulation(2); // OOK调制
        ELECHOUSE_cc1101.setRxBW(270.0f);  // 接收带宽270kHz
        ELECHOUSE_cc1101.setDRate(10.0f);  // 数据速率10kbps
        ELECHOUSE_cc1101.setPktFormat(3);  // 异步串行模式
        ELECHOUSE_cc1101.SetTx();          // 发送模式
        return true;
    }

    // 降级策略：应用原始发送配置
    return applyRawSendProfile(mhz);
}

/**
 * @brief 发送OOK调制的时序数据（内部函数）
 * @param timings 时序列表（单位微秒，交替高低电平）
 * @return 发送成功返回true，失败返回false
 */
bool SubGhzService::sendTimingsOOK_(const std::vector<int32_t>& timings) {
    if (!startTxBitBang()) return false;
    int level = 1; // 初始电平：高
    for (int32_t us : timings) {
        gpio_set_level((gpio_num_t)gdo0_, level); // 设置电平
        if (us > 0) esp_rom_delay_us((uint32_t)us); // 保持时长
        level ^= 1; // 翻转电平
    }
    gpio_set_level((gpio_num_t)gdo0_, 0); // 重置为低电平
    stopTxBitBang();                      // 停止BitBang模式
    return true;
}

/**
 * @brief 追加高低电平时序对到列表（内部工具函数）
 * @param v 时序列表
 * @param hi_us 高电平时长（微秒）
 * @param lo_us 低电平时长（微秒）
 */
static inline void appendPair(std::vector<int32_t>& v, int hi_us, int lo_us) {
    if (hi_us>0) v.push_back(hi_us);
    if (lo_us>0) v.push_back(lo_us);
}

/**
 * @brief 发送RcSwitch协议数据（内部函数）
 * @param key 要发送的键值（64位）
 * @param bits 键值位数
 * @param te_us 时间基准（微秒）
 * @param proto 协议号（1/2/11）
 * @param repeat 重复发送次数
 * @return 发送成功返回true，失败返回false
 * @note 支持主流RcSwitch协议：1(350us)、2(650us)、11(350us，通用)
 */
bool SubGhzService::sendRcSwitch_(uint64_t key, uint16_t bits, int te_us, int proto, int repeat) {
    if (!bits) return false;
    if (te_us <= 0) te_us = 350;      // 默认时间基准350us
    if (repeat <= 0) repeat = 10;     // 默认重复10次

    // 协议时序参数（同步/0/1的高低电平时长）
    int sync_hi=0, sync_lo=0, zero_hi=0, zero_lo=0, one_hi=0, one_lo=0;

    if (proto == 1) {            // 协议1：350us，同步1:31，0=1:3，1=3:1
        sync_hi = te_us * 1;  sync_lo = te_us * 31;
        zero_hi = te_us * 1;  zero_lo = te_us * 3;
        one_hi  = te_us * 3;  one_lo  = te_us * 1;
    } else if (proto == 2) {     // 协议2：650us，同步1:10，0=1:2，1=2:1
        sync_hi = te_us * 1;  sync_lo = te_us * 10;
        zero_hi = te_us * 1;  zero_lo = te_us * 2;
        one_hi  = te_us * 2;  one_lo  = te_us * 1;
    } else {                     // 通用协议11：350us，同步1:23，0=1:2，1=2:1
        proto   = 11;
        sync_hi = te_us * 1;  sync_lo = te_us * 23;
        zero_hi = te_us * 1;  zero_lo = te_us * 2;
        one_hi  = te_us * 2;  one_lo  = te_us * 1;
    }

    // 预分配时序列表内存
    std::vector<int32_t> timings;
    timings.reserve((bits * 2 + 4) * repeat);

    // 重复发送指定次数
    for (int r = 0; r < repeat; ++r) {
        // 同步码
        appendPair(timings, sync_hi, sync_lo);

        // 数据位（MSB到LSB）
        for (int i = bits - 1; i >= 0; --i) {
            bool b = (key >> i) & 1ULL;
            if (b) appendPair(timings, one_hi, one_lo);  // 发送1
            else   appendPair(timings, zero_hi, zero_lo); // 发送0
        }
        // 帧间间隔（空）
    }

    return sendTimingsOOK_(timings);
}

/**
 * @brief 发送Princeton协议数据（PT2262芯片）
 * @param key 键值（64位）
 * @param bits 键值位数
 * @param te_us 时间基准（微秒）
 * @return 发送成功返回true，失败返回false
 * @note Princeton协议：TE≈350us，1=1h3l，0=3h1l，同步=1h31l
 */
bool SubGhzService::sendPrinceton_(uint64_t key, uint16_t bits, int te_us) {
    // 复用RcSwitch协议1实现Princeton发送
    return sendRcSwitch_(key, bits, te_us>0?te_us:350, /*proto*/1, /*repeat*/10);
}

/**
 * @brief 发送二进制原始数据（内部函数）
 * @param bytes 二进制数据
 * @param te_us 每位时长（微秒）
 * @param bits 发送位数（忽略，默认发送全部）
 * @param msb_first 是否MSB优先（未使用）
 * @param invert 是否反转电平（未使用）
 * @return 发送成功返回true，失败返回false
 * @note 发送规则：空闲低电平，从最后一个字节开始，每个字节LSB到MSB
 */
bool SubGhzService::sendBinRaw_(const std::vector<uint8_t>& bytes,
                                int te_us,
                                int bits /*忽略*/,
                                bool /*msb_first 未使用*/,
                                bool /*invert 未使用*/) {
    if (bytes.empty()) return false;
    if (te_us <= 0) te_us = 100; // 默认每位时长100us

    // 总位数（字节数*8）
    const int total_bits = int(bytes.size()) * 8;
    const int limit_bits = total_bits; // 发送全部位

    if (!startTxBitBang()) return false;

    // 初始空闲低电平
    gpio_set_level((gpio_num_t)gdo0_, 0);

    int sent = 0;

    // 从最后一个字节开始发送
    for (int bi = int(bytes.size()) - 1; bi >= 0 && sent < limit_bits; --bi) {
        uint8_t b = bytes[bi];

        // 每个字节按LSB到MSB发送
        for (int i = 0; i < 8 && sent < limit_bits; ++i, ++sent) {
            bool one = (b >> i) & 0x01; // LSB优先
            gpio_set_level((gpio_num_t)gdo0_, one ? 1 : 0);
            esp_rom_delay_us((uint32_t)te_us);
        }
    }

    // 发送完成后恢复低电平
    gpio_set_level((gpio_num_t)gdo0_, 0);

    stopTxBitBang();
    return true;
}

/**
 * @brief 发送原始时序数据（对外接口）
 * @param timings 时序列表（正数=高电平，负数=低电平，单位微秒）
 * @return 发送成功返回true，失败返回false
 */
bool SubGhzService::sendRawTimings(const std::vector<int32_t>& timings) {
    return sendTimingsRawSigned_(timings);
}

/**
 * @brief 发送带符号的原始时序数据（内部函数）
 * @param timings 时序列表（正数=高电平，负数=低电平，单位微秒）
 * @return 发送成功返回true，失败返回false
 */
bool SubGhzService::sendTimingsRawSigned_(const std::vector<int32_t>& timings) {
    if (!startTxBitBang()) return false;

    // 初始空闲低电平
    gpio_set_level((gpio_num_t)gdo0_, 0);

    // 逐时序发送
    for (int32_t t : timings) {
        if (t == 0) continue;          // 跳过0时长
        bool high = (t > 0);           // 正数=高电平
        uint32_t us = (uint32_t)(high ? t : -t); // 取绝对值
        gpio_set_level((gpio_num_t)gdo0_, high ? 1 : 0);
        if (us) esp_rom_delay_us(us);  // 保持电平
    }

    // 恢复空闲低电平
    gpio_set_level((gpio_num_t)gdo0_, 0);
    stopTxBitBang();
    return true;
}

/**
 * @brief 发送亚GHz指令（统一对外发送接口）
 * @param cmd 亚GHz指令结构体（包含频率、预设、协议、数据等）
 * @return 发送成功返回true，失败返回false
 * @note 支持多种协议：RAW/BinRAW/RcSwitch/Princeton/默认（CAME/HOLTEK/NICE）
 */
bool SubGhzService::send(const SubGhzFileCommand& cmd) {
    if (!isConfigured_) return false;

    // 确定工作频率（优先使用指令中的频率，否则用当前频率）
    float mhz = cmd.frequency_hz ? (cmd.frequency_hz / 1e6f) : mhz_;
    tune(mhz);  // 更新天线和接收频率

    // 应用指定的预设配置（失败则降级为原始发送配置）
    if (!applyPresetByName(cmd.preset, mhz)) {
        if (!applyRawSendProfile(mhz)) return false;
    }

    // 异步TX模式下，GDO0作为数据输入，使用BitBang方式发送
    switch (cmd.protocol) {
        case SubGhzProtocolEnum::RAW:
            return sendRawTimings(cmd.raw_timings); // 发送原始时序

        case SubGhzProtocolEnum::BinRAW: {
            const int te = cmd.te_us ? cmd.te_us : 100;
            const int total_bits = int(cmd.bitstream_bytes.size()) * 8;
            return sendBinRaw_(cmd.bitstream_bytes, te, total_bits,
                            /*msb_first*/false, /*invert*/false);
        }

        case SubGhzProtocolEnum::RcSwitch: {
            int proto = 11; // 默认协议11
            char* end=nullptr;
            long p = std::strtol(cmd.preset.c_str(), &end, 10);
            if (end != cmd.preset.c_str()) proto = (int)p; // 按预设解析协议号
            int te = cmd.te_us ? cmd.te_us : ((proto==2)?650:350); // 时间基准
            return sendRcSwitch_(cmd.key, cmd.bits ? cmd.bits : 24, te, proto, /*repeat*/10);
        }

        case SubGhzProtocolEnum::Princeton:
            return sendPrinceton_(cmd.key, cmd.bits ? cmd.bits : 24, cmd.te_us ? cmd.te_us : 350);

        default: {
            // 降级策略：按RcSwitch协议11发送（适配CAME/HOLTEK/NICE等）
            if (!cmd.key) return false;
            const int te   = cmd.te_us ? cmd.te_us : 270;
            const int bits = cmd.bits  ? cmd.bits  : 24;
            return sendRcSwitch_(cmd.key, bits, te, /*proto*/11, /*repeat*/10);
        }
    }
}

// -------------------------- Tembed S3 CC1101 硬件适配 --------------------------

/**
 * @brief 初始化Tembed S3硬件（禁用其他SPI设备，配置射频开关）
 */
void SubGhzService::initTembed() {
    // 硬件引脚定义
    const int BOARD_PWR_EN   = 15;    // 电源使能引脚
    const int BOARD_SDA_PIN  = 18;    // I2C SDA
    const int BOARD_SCL_PIN  = 8;     // I2C SCL
    const int BOARD_TFT_CS   = 41;    // TFT片选
    // SPI总线
    const int BOARD_SPI_SCK  = 11;
    const int BOARD_SPI_MOSI = 9;
    const int BOARD_SPI_MISO = 10;
    // TF卡
    const int BOARD_SD_CS    = 13;
    // LoRa/CC1101
    const int BOARD_LORA_CS  = 12;
    const int BOARD_LORA_IO2 = 38;
    const int BOARD_LORA_IO0 = 3;

    // 禁用其他SPI设备（避免干扰）
    pinMode(41, OUTPUT);
    digitalWrite(41, HIGH);
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);

    // 配置射频开关引脚（默认433MHz）
    pinMode(rfSw1_, OUTPUT);
    pinMode(rfSw0_, OUTPUT);
    digitalWrite(rfSw1_, HIGH);
    digitalWrite(rfSw0_, HIGH);

    // 给CC1101供电
    pinMode(BOARD_PWR_EN, OUTPUT);
    digitalWrite(BOARD_PWR_EN, HIGH);
}

/**
 * @brief 根据频率选择射频路径（天线切换）
 * @param mhz 工作频率（MHz）
 * @note 支持315/433/868/915MHz频段的天线切换
 */
void SubGhzService::selectRfPathFor(float mhz) {
    if (rfSw0_ < 0 || rfSw1_ < 0) return;

    // 频率到射频开关的映射
    uint8_t sel; // 0=315MHz, 1=868/915MHz, 2=433MHz
    if (mhz >= 300.0f && mhz <= 348.0f) {
        sel = 0; // 315MHz频段
    } else if ((mhz >= 387.0f && mhz <= 464.0f)) {
        sel = 2; // 433MHz频段
    } else if (mhz >= 779.0f && mhz <= 928.0f) {
        sel = 1; // 868/915MHz频段
    } else {
        sel = 2; // 降级为433MHz
    }

    if (sel == rfSel_) return; // 无需切换
    rfSel_ = sel;

    // 配置射频开关引脚
    switch (sel) {
        case 0: // 315MHz: SW1=高, SW0=低
            digitalWrite(rfSw1_, HIGH);
            digitalWrite(rfSw0_, LOW);
            break;
        case 1: // 868/915MHz: SW1=低, SW0=高
            digitalWrite(rfSw1_, LOW);
            digitalWrite(rfSw0_, HIGH);
            break;
        case 2: // 433MHz: SW1=高, SW0=高
        default:
            digitalWrite(rfSw1_, HIGH);
            digitalWrite(rfSw0_, HIGH);
            break;
    }
}