#include "I2cService.h"
#include "driver/gpio.h"

void I2cService::configure(uint8_t sda, uint8_t scl, uint32_t frequency) {
    Wire.end();
    Wire.begin(sda, scl, frequency);
}

void I2cService::beginTransmission(uint8_t address) {
    Wire.beginTransmission(address);
}

void I2cService::write(uint8_t data) {
    Wire.write(data);
}

bool I2cService::endTransmission(bool sendStop) {
    return Wire.endTransmission(sendStop);
}

uint8_t I2cService::requestFrom(uint8_t address, uint8_t quantity, bool sendStop) {
    return Wire.requestFrom(address, quantity, sendStop);
}

int I2cService::read() {
    return Wire.read();
}

bool I2cService::available() const {
    return Wire.available();
}

bool I2cService::end() const {
    return Wire.end();
}

std::string I2cService::executeByteCode(const std::vector<ByteCode>& bytecodes) {
    std::string result;
    uint8_t currentAddress = 0;
    bool transmissionStarted = false;
    bool expectAddress = false;

    for (const auto& code : bytecodes) {
        switch (code.getCommand()) {
            case ByteCodeEnum::Start:
                // 等待接收设备地址
                expectAddress = true;
                break;

            case ByteCodeEnum::Stop:
                // 停止I2C传输
                if (transmissionStarted) {
                    Wire.endTransmission();
                    transmissionStarted = false;
                }
                break;

            case ByteCodeEnum::Write:
                if (expectAddress) {
                    // 第一个写入操作：设置设备地址
                    currentAddress = code.getData();
                    Wire.beginTransmission(currentAddress);
                    transmissionStarted = true;
                    expectAddress = false;
                } else {
                    // 后续写入操作：发送数据
                    if (!transmissionStarted) {
                        Wire.beginTransmission(currentAddress);
                        transmissionStarted = true;
                    }
                    for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                        Wire.write(code.getData());
                    }
                }
                break;

            case ByteCodeEnum::Read: {
                // 读取数据前先结束写传输（不发送停止信号）
                if (transmissionStarted) {
                    Wire.endTransmission(false);  // 释放总线
                    transmissionStarted = false;
                }

                uint8_t toRead = code.getRepeat();
                uint8_t readAddr = currentAddress;

                // 请求从设备读取指定字节数
                Wire.requestFrom(readAddr, toRead);
                
                // 读取数据并转换为十六进制字符串
                for (uint8_t i = 0; i < toRead && Wire.available(); ++i) {
                    uint8_t val = Wire.read();
                    char hex[5];
                    snprintf(hex, sizeof(hex), "%02X ", val);
                    result += hex;
                }
                break;
            }

            case ByteCodeEnum::DelayMs:
                // 毫秒级延时
                delay(code.getRepeat());
                break;

            case ByteCodeEnum::DelayUs:
                // 微秒级延时
                delayMicroseconds(code.getRepeat());
                break;

            default:
                break;
        }
    }

    // 若传输未停止，主动发送停止信号
    if (transmissionStarted) {
        Wire.endTransmission();
    }

    return result;
}

bool I2cService::isReadableDevice(uint8_t addr, uint8_t startReg) {
    // 写入寄存器地址
    beginTransmission(addr);
    write(startReg);
    bool writeOk = (endTransmission(false) == 0);
    if (!writeOk) return false;

    // 读取一个字节验证设备是否可读
    uint8_t received = requestFrom(addr, 1, true);
    if (received != 1 || !available()) return false;

    read();  // 读取并清空缓冲区
    return true;
}

/*
I2C从设备相关功能
*/

std::vector<std::string> I2cService::slaveLog;
uint8_t I2cService::slaveResponseBuffer[16] = {};
size_t I2cService::slaveResponseLength = 1;
portMUX_TYPE I2cService::slaveLogMux = portMUX_INITIALIZER_UNLOCKED;

void I2cService::beginSlave(uint8_t address, uint8_t sda, uint8_t scl, uint32_t freq) {
    Wire.end();
    Wire1.end();
    // 初始化I2C从设备
    Wire1.begin(address, sda, scl, freq);

    // 注册从设备回调函数
    Wire1.onReceive(onSlaveReceive);
    Wire1.onRequest(onSlaveRequest);
}

void I2cService::endSlave() {
    Wire1.end();
}

void I2cService::setSlaveResponse(const uint8_t* data, size_t len) {
    // 设置从设备响应数据（最大16字节）
    size_t copyLen = (len < sizeof(slaveResponseBuffer)) ? len : sizeof(slaveResponseBuffer);
    memcpy(slaveResponseBuffer, data, copyLen);
    slaveResponseLength = copyLen;
}

std::vector<std::string> I2cService::getSlaveLog() {
    // 线程安全获取从设备日志
    portENTER_CRITICAL(&slaveLogMux);
    std::vector<std::string> copy = slaveLog;
    portEXIT_CRITICAL(&slaveLogMux);
    return copy;
}

void I2cService::clearSlaveLog() {
    // 线程安全清空从设备日志
    portENTER_CRITICAL(&slaveLogMux);
    slaveLog.clear();
    portEXIT_CRITICAL(&slaveLogMux);
}

void I2cService::onSlaveReceive(int len) {
    // 从设备接收主机数据的回调函数
    std::string entry = "主机写入：";
    while (Wire1.available()) {
        uint8_t b = Wire1.read();
        char hex[5];
        snprintf(hex, sizeof(hex), " %02X", b);
        entry += hex;
    }

    // 记录日志（线程安全）
    portENTER_CRITICAL(&slaveLogMux);
    slaveLog.push_back(entry);
    portEXIT_CRITICAL(&slaveLogMux);
}

void I2cService::onSlaveRequest() {
    // 从设备响应主机读取请求的回调函数
    portENTER_CRITICAL(&slaveLogMux);
    slaveLog.push_back("主机请求读取");
    portEXIT_CRITICAL(&slaveLogMux);

    // 发送预设的响应数据
    Wire1.write(slaveResponseBuffer, slaveResponseLength);
}

/*
I2C时序干扰/故障注入相关功能
*/

void I2cService::i2cBitBangDelay(uint32_t delayUs) {
    if (delayUs > 0) esp_rom_delay_us(delayUs);
}

void I2cService::i2cBitBangSetLevel(uint8_t pin, bool level) {
    gpio_set_level((gpio_num_t)pin, level);
}

void I2cService::i2cBitBangSetOutput(uint8_t pin) {
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
}

void I2cService::i2cBitBangSetInput(uint8_t pin) {
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
}

void I2cService::i2cBitBangStartCondition(uint8_t scl, uint8_t sda, uint32_t delayUs) {
    // 生成I2C起始条件：SDA先拉低，再拉低SCL
    i2cBitBangSetInput(sda);  // 上拉模式
    i2cBitBangSetInput(scl);  // 上拉模式
    i2cBitBangDelay(delayUs);

    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, LOW); // SDA拉低
    i2cBitBangDelay(delayUs);

    i2cBitBangSetOutput(scl);
    i2cBitBangSetLevel(scl, LOW); // SCL拉低
    i2cBitBangDelay(delayUs);
}

void I2cService::i2cBitBangStopCondition(uint8_t scl, uint8_t sda, uint32_t delayUs) {
    // 生成I2C停止条件：SCL先拉高，再拉高SDA
    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, LOW); // SDA拉低
    i2cBitBangDelay(delayUs);

    i2cBitBangSetInput(scl);      // SCL拉高
    i2cBitBangDelay(delayUs);

    i2cBitBangSetInput(sda);      // SDA拉高
    i2cBitBangDelay(delayUs);
}

void I2cService::i2cBitBangWriteBit(uint8_t scl, uint8_t sda, bool bit, uint32_t d) {
    // 位bang方式写入单个比特
    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, bit);
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 1);   // SCL拉高（采样）
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 0);   // SCL拉低
    i2cBitBangDelay(d);
}

void I2cService::i2cBitBangWriteByte(uint8_t scl, uint8_t sda, uint8_t byte, uint32_t d, bool& ack) {
    // 位bang方式写入一个字节，并获取ACK/NACK
    for (int i = 7; i >= 0; --i) {
        i2cBitBangWriteBit(scl, sda, (byte >> i) & 0x01, d);
    }

    // 等待从设备ACK/NACK
    i2cBitBangSetInput(sda);
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 1);   // SCL拉高（采样ACK）
    i2cBitBangDelay(d);
    ack = (gpio_get_level((gpio_num_t)sda) == 0); // ACK表示SDA拉低
    i2cBitBangSetLevel(scl, 0);   // SCL拉低
    i2cBitBangDelay(d);
    i2cBitBangSetOutput(sda);
}

void I2cService::i2cBitBangReadByte(uint8_t scl, uint8_t sda, uint32_t d, bool nackLast) {
    // 位bang方式读取一个字节，并发送ACK/NACK
    uint8_t data = 0;
    i2cBitBangSetInput(sda);
    for (int i = 7; i >= 0; --i) {
        i2cBitBangSetLevel(scl, 1);   // SCL拉高（从设备输出数据）
        i2cBitBangDelay(d);
        if (gpio_get_level((gpio_num_t)sda)) {
            data |= (1 << i);
        }
        i2cBitBangSetLevel(scl, 0);   // SCL拉低
        i2cBitBangDelay(d);
    }

    // 发送ACK/NACK（最后一个字节发NACK）
    i2cBitBangSetOutput(sda);
    i2cBitBangSetLevel(sda, nackLast ? 1 : 0); // 最后字节发NACK，否则发ACK
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 1);
    i2cBitBangDelay(d);
    i2cBitBangSetLevel(scl, 0);
    i2cBitBangSetLevel(sda, 1);
}

bool I2cService::i2cBitBangRecoverBus(uint8_t scl, uint8_t sda, uint32_t freqHz) {
    // 恢复卡死的I2C总线（发送16个SCL脉冲）
    uint32_t delayUs = 500000 / freqHz; // 半周期

    // SCL/SDA设为输入（上拉）
    i2cBitBangSetInput(scl);
    i2cBitBangSetInput(sda);
    i2cBitBangDelay(delayUs);

    // 若SDA拉低，发送SCL脉冲直到SDA释放
    if (gpio_get_level((gpio_num_t)sda) == 0) {
        i2cBitBangSetOutput(scl);
        for (int i = 0; i < 16; ++i) {
            i2cBitBangSetLevel(scl, 0);
            i2cBitBangDelay(delayUs);
            i2cBitBangSetLevel(scl, 1);
            i2cBitBangDelay(delayUs);

            if (gpio_get_level((gpio_num_t)sda) == 1) break;
        }
    }

    // 发送停止条件
    i2cBitBangStopCondition(scl, sda, delayUs);

    delay(20); // 等待总线稳定

    return gpio_get_level((gpio_num_t)sda) == 1;
}

void I2cService::rapidStartStop(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    // 快速发送START+STOP序列（总线压力测试）
    uint32_t d = 500000 / freqHz;
    bool ack;
    for (int i = 0; i < 500; ++i) {
        i2cBitBangStartCondition(scl, sda, 0);
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
        i2cBitBangStopCondition(scl, sda, 0);
    }
}

void I2cService::floodStart(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    // 连续发送START+地址（不发送STOP，总线占用测试）
    uint32_t d = 500000 / freqHz;
    for (int i = 0; i < 1000; ++i) {
        i2cBitBangStartCondition(scl, sda, 0);
        bool ack;
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
    }
}

void I2cService::floodRandom(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    // 发送随机数据洪水（总线干扰测试）
    uint32_t d = 500000 / freqHz;
    for (int i = 0; i < 100; ++i) {
        // 起始条件
        i2cBitBangStartCondition(scl, sda, 0);

        // 发送地址+随机数据
        bool ack = false;
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
        for (int j = 0; j < 20; ++j) {
            i2cBitBangWriteByte(scl, sda, rand() & 0xFF, d, ack);
        }

        // 停止条件
        i2cBitBangStopCondition(scl, sda, 0);
        delay(5);
    }
}

void I2cService::overReadAttack(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    // 过度读取攻击（连续读取1024字节）
    uint32_t d = 500000 / freqHz;
    bool ack;

    // 起始条件
    i2cBitBangStartCondition(scl, sda, 0);

    // 发送读地址
    i2cBitBangWriteByte(scl, sda, (address << 1) | 1, d, ack);

    // 连续读取1024字节（全部发ACK）
    for (int i = 0; i < 1024; ++i) {
        i2cBitBangReadByte(scl, sda, d, false);  // ACK
    }
    i2cBitBangReadByte(scl, sda, d, true); // 最后字节发NACK

    // 停止条件
    i2cBitBangStopCondition(scl, sda, 0);
}

void I2cService::invalidRegisterRead(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    // 读取无效寄存器攻击（0xFF寄存器）
    uint32_t d = 500000 / freqHz;
    bool ack;

    for (int i = 0; i < 512; ++i) {
        // 起始条件
        i2cBitBangStartCondition(scl, sda, 0);

        // 写入设备地址+无效寄存器地址
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);  // 写操作
        i2cBitBangWriteByte(scl, sda, 0xFF, d, ack);          // 无效寄存器

        // 重复起始条件
        i2cBitBangSetLevel(sda, 1); i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(d);
        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(d);
        i2cBitBangSetLevel(scl, 0);

        // 发送读地址并读取（发NACK）
        i2cBitBangWriteByte(scl, sda, (address << 1) | 1, d, ack); // 读操作
        i2cBitBangReadByte(scl, sda, d, true); // NACK

        // 停止条件
        i2cBitBangStopCondition(scl, sda, 0);
        delay(2);
    }
}

void I2cService::simulateClockStretch(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    // 模拟时钟拉伸干扰
    uint32_t d = 500000 / freqHz;
    bool ack;

    for (int i = 0; i < 50; ++i) {
        // 模拟起始条件
        i2cBitBangSetLevel(sda, 1); i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(d);
        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(d);
        i2cBitBangSetLevel(scl, 0);

        // 发送地址+数据
        i2cBitBangWriteByte(scl, sda, address << 1, d, ack);
        i2cBitBangWriteByte(scl, sda, 0xA5, d, ack);

        delay(2); // 模拟从设备时钟拉伸导致的延迟

        // 模拟停止条件
        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(d);
        i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(d);
        i2cBitBangSetLevel(sda, 1); i2cBitBangDelay(d);

        delay(2); // 模拟从设备时钟拉伸导致的延迟
    }
}

void I2cService::glitchAckInjection(uint8_t address, uint32_t freqHz, uint8_t scl, uint8_t sda) {
    // ACK注入干扰（伪造ACK响应）
    uint32_t d = 500000 / freqHz;
    bool ack;

    // 起始条件
    i2cBitBangStartCondition(scl, sda, 0);

    // 发送设备地址
    i2cBitBangWriteByte(scl, sda, address << 1, d, ack);

    // 伪造10次空数据+ACK
    for (int i = 0; i < 10; ++i) {
        for (int b = 7; b >= 0; --b)
            i2cBitBangWriteBit(scl, sda, (0x00 >> b) & 1, d);

        // 伪造ACK响应
        i2cBitBangSetOutput(sda);
        i2cBitBangSetLevel(sda, 0); i2cBitBangDelay(1);
        i2cBitBangSetLevel(scl, 1); i2cBitBangDelay(1);
        i2cBitBangSetLevel(scl, 0);
    }

    // 停止条件
    i2cBitBangStopCondition(scl, sda, 0);
}

void I2cService::sclSdaGlitch(uint8_t scl, uint8_t sda) {
    // SCL/SDA电平毛刺注入（总线干扰）
    for (int i = 0; i < 20; ++i) {
        i2cBitBangSetOutput(scl);
        i2cBitBangSetLevel(scl, 0);
        i2cBitBangSetOutput(sda);
        i2cBitBangSetLevel(sda, 0);
        delayMicroseconds(5 + (esp_random() % 10));  // 5–15微秒低电平

        i2cBitBangSetInput(scl);
        i2cBitBangSetInput(sda);
        delayMicroseconds(5 + (esp_random() % 10));
    }
}

void I2cService::randomClockPulseNoise(uint8_t scl, uint8_t sda, uint32_t freqHz) {
    // 随机时钟脉冲噪声（总线干扰）
    uint32_t d = 500000 / freqHz;

    i2cBitBangSetOutput(scl);
    i2cBitBangSetOutput(sda);

    for (int i = 0; i < 100; ++i) {
        i2cBitBangSetLevel(scl, random(2));
        i2cBitBangSetLevel(sda, random(2));
        delayMicroseconds(random(d));
    }
}

void I2cService::injectRandomGlitch(uint8_t scl, uint8_t sda, uint32_t freqHz) {
    // 随机注入干扰（三种干扰方式随机选）
    if (freqHz == 0) freqHz = 100000;

    switch (esp_random() % 3) {
        case 0:
            randomClockPulseNoise(scl, sda, freqHz);
            break;

        case 1:
            sclSdaGlitch(scl, sda);
            break;

        case 2:
            // 针对保留地址发送快速START/STOP（无实际设备）
            rapidStartStop(0x7F, freqHz, scl, sda);
            break;
    }
}

/*
I2C EEPROM相关功能
*/

bool I2cService::initEeprom(uint16_t chipSizeKb, uint8_t addr) {
    // 初始化EEPROM设备
    eeprom.setMemoryType(chipSizeKb);
    return eeprom.begin(addr);
}

bool I2cService::eepromWriteByte(uint16_t address, uint8_t value) {
    // 向EEPROM写入单个字节
    return eeprom.write(address, value);
}

uint8_t I2cService::eepromReadByte(uint16_t address) {
    // 从EEPROM读取单个字节
    return eeprom.read(address);
}

bool I2cService::eepromPutString(uint32_t address, const std::string& str) {
    // 向EEPROM写入字符串
    String arduinoStr(str.c_str());
    return eeprom.putString(address, arduinoStr) > 0;
}

bool I2cService::eepromGetString(uint32_t address, std::string& outStr) {
    // 从EEPROM读取字符串
    String arduinoStr;
    eeprom.getString(address, arduinoStr);
    outStr = std::string(arduinoStr.c_str());
    return true;
}

void I2cService::eepromErase(uint8_t fill) {
    // 擦除EEPROM（用指定值填充）
    eeprom.erase(fill);
}

bool I2cService::eepromDetectMemorySize() {
    // 自动检测EEPROM容量
    uint32_t size = eeprom.detectMemorySizeBytes();
    if (size > 0) {
        eeprom.setMemorySizeBytes(size);
        return true;
    }
    return false;
}

uint8_t I2cService::eepromDetectAddressBytes() {
    // 自动检测EEPROM地址字节数
    uint8_t bytes = eeprom.detectAddressBytes();
    eeprom.setAddressBytes(bytes);
    return bytes;
}

uint16_t I2cService::eepromDetectPageSize() {
    // 自动检测EEPROM页大小
    uint16_t size = eeprom.detectPageSizeBytes();
    eeprom.setPageSizeBytes(size);
    return size;
}

uint8_t I2cService::eepromDetectWriteTime(uint8_t testCount) {
    // 检测EEPROM写入耗时
    return eeprom.detectWriteTimeMs(testCount);
}

uint32_t I2cService::eepromLength() {
    // 获取EEPROM总容量（字节）
    return eeprom.length();
}

uint32_t I2cService::eepromGetMemorySize()  {
    // 获取EEPROM配置的容量（字节）
    return eeprom.getMemorySizeBytes();
}

uint16_t I2cService::eepromPageSize()  {
    // 获取EEPROM页大小（字节）
    return eeprom.getPageSizeBytes();
}

uint8_t I2cService::eepromWriteTimeMs() {
    // 获取EEPROM写入耗时（毫秒）
    return eeprom.getWriteTimeMs();
}

uint8_t I2cService::eepromAddressBytes() {
    // 获取EEPROM地址字节数
    return eeprom.getAddressBytes();
}

bool I2cService::eepromIsConnected() {
    // 检查EEPROM是否连接
    return eeprom.isConnected();
}

bool I2cService::eepromIsBusy() {
    // 检查EEPROM是否忙
    return eeprom.isBusy();
}