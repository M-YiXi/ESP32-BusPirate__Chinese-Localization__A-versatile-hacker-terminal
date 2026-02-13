#include "Rf24Service.h"

/**
 * @brief 配置nRF24L01模块的SPI引脚并初始化模块
 * @param csnPin CSN引脚（片选，低电平有效）
 * @param cePin CE引脚（使能，控制收发模式）
 * @param sckPin SPI时钟引脚
 * @param misoPin SPI MISO引脚（主入从出）
 * @param mosiPin SPI MOSI引脚（主出从入）
 * @param spiSpeed SPI通信速率（Hz）
 * @return 初始化成功返回true，失败返回false
 * @note 初始化前会释放原有radio实例、重启SPI总线，避免资源冲突
 */
bool Rf24Service::configure(
        uint8_t csnPin,
        uint8_t cePin,
        uint8_t sckPin,
        uint8_t misoPin,
        uint8_t mosiPin,
        uint32_t spiSpeed
    ) {
    // 保存引脚和SPI参数
    cePin_ = cePin;
    csnPin_ = csnPin;
    sckPin_ = sckPin;
    misoPin_ = misoPin;
    mosiPin_ = mosiPin;
    spiSpeed_ = spiSpeed;

    // 释放原有nRF24实例，避免内存泄漏
    if (radio_) delete radio_;
    SPI.end();          // 停止当前SPI总线
    delay(10);          // 短暂延迟，稳定总线状态
    SPI.begin(sckPin_, misoPin_, mosiPin_, csnPin); // 初始化指定引脚的SPI总线
    
    // 创建新的nRF24实例
    radio_ = new RF24(cePin_, csnPin_);
    // 检查实例创建成功且模块初始化成功
    if (!radio_ || !radio_->begin(&SPI)) return false;
    isInitialized = true; // 标记模块已初始化
    return true;
}

/**
 * @brief 初始化nRF24L01为接收模式（基础参数配置）
 * @note 仅当模块已初始化时执行，配置：关闭自动应答、禁用CRC、1Mbps速率、2字节地址宽度
 */
void Rf24Service::initRx() {
    if (!isInitialized) return;
    radio_->setAutoAck(false);        // 关闭自动应答（AA）
    radio_->setCRCLength(RF24_CRC_DISABLED); // 禁用CRC校验
    radio_->setDataRate(RF24_1MBPS);  // 设置数据传输速率：1Mbps
    radio_->setAddressWidth(2);       // 设置地址宽度：2字节
}

/**
 * @brief 设置nRF24L01的工作频道
 * @param channel 频道号（0~125，对应2.400~2.525GHz）
 */
void Rf24Service::setChannel(uint8_t channel) {
    if (isInitialized) {
        radio_->setChannel(channel);
    }
}

/**
 * @brief 获取当前nRF24L01的工作频道
 * @return 当前频道号（未初始化返回0）
 */
uint8_t Rf24Service::getChannel() {
    if (isInitialized) {
        return radio_->getChannel();
    }
    return 0;
}

/**
 * @brief 唤醒nRF24L01模块（从掉电模式进入待机/工作模式）
 */
void Rf24Service::powerUp() {
    if (isInitialized) {
        radio_->powerUp();
    }
}

/**
 * @brief 关闭nRF24L01模块（软/硬掉电）
 * @param hard true=硬掉电（完全断电，功耗最低），false=软掉电（停止载波）
 */
void Rf24Service::powerDown(bool hard) {
    if (!isInitialized) {
        return;
    }

    if (hard) {
        radio_->powerDown(); // 硬掉电：模块进入最低功耗模式
        return;
    }
    
    radio_->stopConstCarrier(); // 软掉电：停止持续载波输出
}

/**
 * @brief 设置nRF24L01的发射功率等级
 * @param level 功率等级（RF24_PA_MIN/-18dBm、RF24_PA_LOW/-12dBm、RF24_PA_HIGH/-6dBm、RF24_PA_MAX/0dBm）
 */
void Rf24Service::setPowerLevel(rf24_pa_dbm_e level) {
    if (isInitialized) {
        radio_->setPALevel(level);
    }
}

/**
 * @brief 设置nRF24L01为最大发射功率，并启动持续载波输出
 * @note 最大功率：0dBm（RF24_PA_MAX），载波频率对应当前频道+45？（模块特定配置）
 */
void Rf24Service::setPowerMax() {
    if (isInitialized) {
        radio_->setPALevel(RF24_PA_MAX); // 设置发射功率为最大值
        radio_->startConstCarrier(RF24_PA_MAX, 45); // 启动持续载波输出
    }
}

/**
 * @brief 让nRF24L01进入接收模式（开始监听数据）
 */
void Rf24Service::startListening() {
    if (isInitialized) {
        radio_->startListening();
    }
}

/**
 * @brief 停止nRF24L01的接收模式（切换为发射模式/待机）
 */
void Rf24Service::stopListening() {
    if (isInitialized) {
        radio_->stopListening();
    }
}

/**
 * @brief 设置nRF24L01的数据传输速率
 * @param rate 速率（RF24_250KBPS/250kbps、RF24_1MBPS/1Mbps、RF24_2MBPS/2Mbps）
 */
void Rf24Service::setDataRate(rf24_datarate_e rate) {
    if (isInitialized) {
        radio_->setDataRate(rate);
    }
}

/**
 * @brief 设置nRF24L01的CRC校验长度
 * @param length CRC长度（RF24_CRC_DISABLED/禁用、RF24_CRC_8/8位、RF24_CRC_16/16位）
 */
void Rf24Service::setCrcLength(rf24_crclength_e length) {
    if (isInitialized) {
        radio_->setCRCLength(length);
    }
}

/**
 * @brief 打开nRF24L01的写管道（发射模式地址）
 * @param address 64位管道地址（nRF24L01的地址为大端序）
 */
void Rf24Service::openWritingPipe(uint64_t address) {
    if (isInitialized) {
        radio_->openWritingPipe(address);
    }
}

/**
 * @brief 打开nRF24L01的读管道（接收模式地址）
 * @param number 管道号（0~5，nRF24L01支持6个接收管道）
 * @param address 64位管道地址
 */
void Rf24Service::openReadingPipe(uint8_t number, uint64_t address) {
    if (isInitialized) {
        radio_->openReadingPipe(number, address);
    }
}

/**
 * @brief 发送数据到已配置的写管道
 * @param buf 待发送数据的缓冲区指针
 * @param len 待发送数据的长度（nRF24L01最大32字节）
 * @return 发送成功返回true，失败返回false
 */
bool Rf24Service::send(const void* buf, uint8_t len) {
    if (!isInitialized) return false;
    bool ok = radio_->write(buf, len); // 写入数据并发送
    return ok;
}

/**
 * @brief 检查是否有可接收的数据
 * @return 有数据返回true，无数据/未初始化返回false
 */
bool Rf24Service::available() {
    if (!isInitialized) return false;
    return radio_->available();
}

/**
 * @brief 接收数据（读取当前可用的数据包）
 * @param buf 接收数据的缓冲区指针
 * @param len 期望接收的长度（实际长度由getDynamicPayloadSize获取）
 * @return 接收成功返回true，失败返回false
 * @note 会自动获取动态负载大小，并读取对应长度的数据到缓冲区
 */
bool Rf24Service::receive(void* buf, uint8_t len) {
    if (!isInitialized) return false;
    if (radio_->available()) {
        len = radio_->getDynamicPayloadSize(); // 获取实际数据包长度
        radio_->read(buf, len);                // 读取数据到缓冲区
        return true;
    }
    return false;
}

/**
 * @brief 检查nRF24L01芯片是否正常连接
 * @return 连接正常返回true，未连接/未初始化返回false
 */
bool Rf24Service::isChipConnected() {
    if (!isInitialized) return false;
    return radio_->isChipConnected();
}

/**
 * @brief 测试是否检测到载波信号（Carrier Detect）
 * @return 检测到载波返回true，未检测到/未初始化返回false
 */
bool Rf24Service::testCarrier() {
    if (!isInitialized) return false;
    return radio_->testCarrier();
}

/**
 * @brief 测试是否检测到接收功率检测信号（RPD，Received Power Detector）
 * @return 检测到信号返回true，未检测到/未初始化返回false
 */
bool Rf24Service::testRpd() {
    if (!isInitialized) return false;
    return radio_->testRPD();
}

/**
 * @brief 清空接收缓冲区（RX FIFO）
 */
void Rf24Service::flushRx() {
    if (!isInitialized) return;
    radio_->flush_rx();
}

/**
 * @brief 清空发送缓冲区（TX FIFO）
 */
void Rf24Service::flushTx() {
    if (!isInitialized) return;
    radio_->flush_tx();
}