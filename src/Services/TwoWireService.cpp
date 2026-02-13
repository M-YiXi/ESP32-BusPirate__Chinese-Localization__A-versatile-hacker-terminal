#include "TwoWireService.h"
#include <Arduino.h>

/**
 * @brief 配置两线通信引脚（CLK/IO/RST）并初始化引脚状态
 * @param clk CLK时钟引脚号
 * @param io IO数据引脚号（双向）
 * @param rst RST复位引脚号
 * @note 重置引脚状态，设置为输出模式并初始化电平
 */
void TwoWireService::configure(uint8_t clk, uint8_t io, uint8_t rst) {
    clkPin = clk;
    ioPin = io;
    rstPin = rst;

    // 重置引脚状态（清除原有配置）
    gpio_reset_pin((gpio_num_t)clkPin);
    gpio_reset_pin((gpio_num_t)rstPin);
    gpio_reset_pin((gpio_num_t)ioPin);

    // 设置引脚为输出模式
    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)rstPin, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_OUTPUT);

    // 初始化引脚电平
    gpio_set_level((gpio_num_t)clkPin, 0);  // CLK初始低电平
    gpio_set_level((gpio_num_t)rstPin, 0);  // RST初始低电平
    gpio_set_level((gpio_num_t)ioPin, 1);   // IO释放（高电平）
}

/**
 * @brief 释放两线通信引脚（恢复为输入浮空状态）
 * @note 清空引脚输出电平，设置为输入模式并取消上下拉
 */
void TwoWireService::end() {
    // 重置引脚电平
    gpio_set_level((gpio_num_t)clkPin, 0);
    gpio_set_level((gpio_num_t)rstPin, 0);

    // 设置为输入模式，浮空状态（无上下拉）
    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)clkPin, GPIO_FLOATING);

    gpio_set_direction((gpio_num_t)rstPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)rstPin, GPIO_FLOATING);

    gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)ioPin, GPIO_FLOATING);
}

/**
 * @brief 设置RST引脚电平
 * @param level true=高电平，false=低电平
 */
void TwoWireService::setRST(bool level) {
    gpio_set_level((gpio_num_t)rstPin, level ? 1 : 0);
}

/**
 * @brief 设置CLK引脚电平
 * @param level true=高电平，false=低电平
 */
void TwoWireService::setCLK(bool level) {
    gpio_set_level((gpio_num_t)clkPin, level ? 1 : 0);
}

/**
 * @brief 设置IO引脚电平（支持双向切换）
 * @param level true=释放为输入模式（浮空），false=输出低电平
 * @note IO引脚是双向的：高电平设为输入，低电平设为输出并拉低
 */
void TwoWireService::setIO(bool level) {
    if (level) {
        // 释放IO：设为输入模式，浮空
        gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)ioPin, GPIO_FLOATING);
    } else {
        // 拉低IO：设为输出模式，输出低电平
        gpio_set_pull_mode((gpio_num_t)ioPin, GPIO_FLOATING);
        gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)ioPin, 0);
    }
}

/**
 * @brief 读取IO引脚当前电平
 * @return true=高电平，false=低电平
 * @note 读取前先将IO设为输入模式
 */
bool TwoWireService::readIO() {
    gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT);
    return gpio_get_level((gpio_num_t)ioPin);
}

/**
 * @brief 产生一个时钟脉冲（CLK低→高→低）
 * @note 每个脉冲持续约10微秒（低5us，高5us）
 */
void TwoWireService::pulseClock() {
    setCLK(false);   // CLK拉低
    delayMicroseconds(5);
    setCLK(true);    // CLK拉高
    delayMicroseconds(5);
    setCLK(false);   // CLK拉低
}

/**
 * @brief 向两线总线写入1个比特
 * @param bit 要写入的比特值（true=1，false=0）
 * @note 先设置IO电平，再产生时钟脉冲锁存数据
 */
void TwoWireService::writeBit(bool bit) {
    setIO(bit);      // 设置IO电平
    pulseClock();    // 时钟脉冲锁存
}

/**
 * @brief 从两线总线读取1个比特
 * @return 读取到的比特值（true=1，false=0）
 * @note 拉高CLK后读取IO电平，再拉低CLK
 */
bool TwoWireService::readBit() {
    setCLK(true);    // CLK拉高（数据有效）
    delayMicroseconds(5);
    bool bit = readIO(); // 读取IO电平
    setCLK(false);   // CLK拉低
    delayMicroseconds(5);
    return bit;
}

/**
 * @brief 向两线总线写入1个字节（LSB低位优先）
 * @param byte 要写入的字节数据
 */
void TwoWireService::writeByte(uint8_t byte) {
    // 逐位写入（0-7位，低位优先）
    for (uint8_t i = 0; i < 8; i++) {
        writeBit((byte >> i) & 1);
    }
}

/**
 * @brief 从两线总线读取1个字节（LSB低位优先）
 * @return 读取到的字节数据
 */
uint8_t TwoWireService::readByte() {
    uint8_t b = 0;
    // 逐位读取（0-7位，低位优先）
    for (uint8_t i = 0; i < 8; i++) {
        bool bit = readBit();
        if (bit) b |= (1 << i);
    }
    return b;
}

/**
 * @brief 发送两线协议起始信号（START）
 * @note 协议时序：IO高→CLK高→IO低→CLK低
 */
void TwoWireService::sendStart() {
    setIO(true);
    setCLK(true);
    delayMicroseconds(5);
    setIO(false);
    delayMicroseconds(5);
    setCLK(false);
}

/**
 * @brief 发送两线协议停止信号（STOP）
 * @note 协议时序：IO低→CLK高→IO高→CLK低
 */
void TwoWireService::sendStop() {
    setIO(false);
    setCLK(true);
    delayMicroseconds(5);
    setIO(true);
    delayMicroseconds(5);
    setCLK(false);
}

/**
 * @brief 发送3字节命令帧（START + 3字节数据 + STOP）
 * @param a 命令第1字节
 * @param b 命令第2字节
 * @param c 命令第3字节
 */
void TwoWireService::sendCommand(uint8_t a, uint8_t b, uint8_t c) {
    sendStart();     // 起始信号
    writeByte(a);    // 第1字节
    writeByte(b);    // 第2字节
    writeByte(c);    // 第3字节
    sendStop();      // 停止信号
}

/**
 * @brief 读取指定长度的响应数据
 * @param len 要读取的字节数
 * @return 读取到的字节数组
 */
std::vector<uint8_t> TwoWireService::readResponse(uint16_t len) {
    std::vector<uint8_t> data;
    for (uint16_t i = 0; i < len; i++) {
        data.push_back(readByte());
    }
    return data;
}

/**
 * @brief 发送指定数量的时钟脉冲
 * @param ticks 脉冲数量
 */
void TwoWireService::sendClocks(uint16_t ticks) {
    for (uint16_t i = 0; i < ticks; i++) {
        pulseClock();
    }
}

/**
 * @brief 等待IO引脚变为高电平（超时退出）
 * @param maxTicks 最大等待时钟脉冲数
 * @return 超时前IO变为高电平返回true，否则返回false
 */
bool TwoWireService::waitIOHigh(uint32_t maxTicks) {
    for (uint32_t i = 0; i < maxTicks; ++i) {
        pulseClock();
        if (readIO()) return true;
    }
    return false; // 超时
}

/**
 * @brief 执行智能卡ATR（应答复位）操作
 * @return ATR响应数据（前4字节）
 * @note 适配SLE44xx系列智能卡的ATR时序
 */
std::vector<uint8_t> TwoWireService::performSmartCardAtr() {
    // 预加载时钟（避免首次调用延迟）
    setCLK(true);
    delayMicroseconds(5);
    setCLK(false);

    // 启动ATR流程
    setIO(false);     // IO设为输出低
    setRST(false);    // RST拉低
    delay(1);         // 等待1ms
    setRST(true);     // 释放RST
    pulseClock();     // 时钟脉冲
    delayMicroseconds(50); // 等待50us
    setRST(false);    // RST拉低
    setIO(true);      // IO设为输入

    return readResponse(4); // 读取4字节ATR数据
}

/**
 * @brief 解析智能卡ATR数据（格式化输出）
 * @param atr ATR原始数据（至少4字节）
 * @return 格式化的ATR解析字符串
 */
std::string TwoWireService::parseSmartCardAtr(const std::vector<uint8_t>& atr) {
    char line[64];
    std::string out;

    if (atr.size() < 4) {
        snprintf(line, sizeof(line), "ATR过短（%d字节）\r\n", (int)atr.size());
        return line;
    }

    // 转换为ATR结构体（SLE44xx专用）
    const sle44xx_atr_t* head = reinterpret_cast<const sle44xx_atr_t*>(atr.data());

    // 原始ATR数据
    snprintf(line, sizeof(line), "   ATR: 0x%02X 0x%02X 0x%02X 0x%02X\r\n", atr[0], atr[1], atr[2], atr[3]);
    out += line;

    // 协议类型
    snprintf(line, sizeof(line), "   协议类型: %s (%d)\r\n",
             (head->protocol_type == 0b1010) ? "S" : "未知",
             head->protocol_type);
    out += line;

    // 结构标识符
    out += parseSmartCardStructureIdentifier(head->structure_identifier);

    // 读取模式
    out += "   读取模式: ";
    out += head->read_with_defined_length ? "固定长度读取\r\n" : "读取至末尾\r\n";

    // 数据单元大小
    if (head->data_units == 0b0000) {
        out += "   数据单元: 未定义\r\n";
    } else {
        int size = 1 << (head->data_units + 6);
        snprintf(line, sizeof(line), "   数据单元: %d\r\n", size);
        out += line;
    }

    // 数据单元位长度
    int bit_len = 1 << head->data_units_bits;
    snprintf(line, sizeof(line), "   数据单元位长度: %d\r\n", bit_len);
    out += line;

    return out;
}

/**
 * @brief 解析智能卡结构标识符
 * @param id 结构标识位（3位）
 * @return 格式化的结构描述字符串
 */
std::string TwoWireService::parseSmartCardStructureIdentifier(uint8_t id) {
    std::string out = "   结构标识符: ";
    switch (id) {
        case 0b000:
            out += "保留（ISO/IEC使用）\r\n";
            break;
        case 0b010:
            out += "标准内存结构（Type 1）\r\n";
            break;
        case 0b110:
            out += "专有内存\r\n";
            break;
        default:
            out += "应用专用\r\n";
            break;
    }
    return out;
}

/**
 * @brief 解析智能卡剩余验证尝试次数
 * @param statusByte 状态字节（低3位表示尝试次数）
 * @return 剩余尝试次数（0-3）
 */
uint8_t TwoWireService::parseSmartCardRemainingAttempts(uint8_t statusByte) {
    resetSmartCard();
    uint8_t attemptsBits = statusByte & 0x07; // 取低3位
    int attempts = 0;
    // 统计置位的位数（每1位代表1次尝试）
    for (int i = 0; i < 3; ++i) {
        if (attemptsBits & (1 << i)) {
            attempts++;
        }
    }
    return attempts;
}

/**
 * @brief 完整读取智能卡所有内存（主内存+安全内存+保护内存）
 * @return 内存数据（共264字节：256主内存+4安全+4保护）
 */
std::vector<uint8_t> TwoWireService::dumpSmartCardFullMemory() {
    resetSmartCard();
    std::vector<uint8_t> dump;

    // 主内存（256字节）
    sendCommand(0x30, 0x00, 0x00);
    for (int i = 0; i < 256; ++i) {
        dump.push_back(readByte());
    }

    // 安全内存（4字节）
    sendCommand(0x31, 0x00, 0x00);
    for (int i = 0; i < 4; ++i) {
        dump.push_back(readByte());
    }

    // 保护内存（4字节）
    sendCommand(0x34, 0x00, 0x00);
    for (int i = 0; i < 4; ++i) {
        dump.push_back(readByte());
    }

    return dump; // 总计264字节
}

/**
 * @brief 重置智能卡（发送256个时钟脉冲）
 */
void TwoWireService::resetSmartCard() {
    sendClocks(256);
}

/**
 * @brief 更新智能卡安全尝试次数模式
 * @param pattern 尝试次数模式位（低3位有效）
 */
void TwoWireService::updateSmartCardSecurityAttempts(uint8_t pattern) {
    sendCommand(0x39, 0x00, pattern);
    resetSmartCard();
}

/**
 * @brief 对比智能卡验证数据（PSC校验）
 * @param address 验证地址（1-3对应PSC的3个字节）
 * @param value 验证值
 */
void TwoWireService::compareSmartCardVerificationData(uint8_t address, uint8_t value) {
    sendCommand(0x33, address, value);
    resetSmartCard();
}

/**
 * @brief 写入智能卡安全内存
 * @param address 内存地址（0-3）
 * @param value 写入值
 */
void TwoWireService::writeSmartCardSecurityMemory(uint8_t address, uint8_t value) {
    sendCommand(0x39, address, value);
    resetSmartCard();
}

/**
 * @brief 写入智能卡保护内存
 * @param address 内存地址（0-3）
 * @param value 写入值
 */
void TwoWireService::writeSmartCardProtectionMemory(uint8_t address, uint8_t value) {
    sendCommand(0x3C, address, value);
    resetSmartCard();
}

/**
 * @brief 写入智能卡主内存并校验
 * @param address 主内存地址（0-255）
 * @param value 写入值
 * @return 写入并校验成功返回true，否则返回false
 */
bool TwoWireService::writeSmartCardMainMemory(uint8_t address, uint8_t value) {
    sendCommand(0x38, address, value);
    resetSmartCard();

    // 读取校验
    auto readBack = readSmartCardMainMemory(address, 1);
    return !readBack.empty() && readBack[0] == value;
}

/**
 * @brief 读取智能卡主内存
 * @param startAddress 起始地址（0-255）
 * @param length 读取长度
 * @return 读取到的内存数据
 */
std::vector<uint8_t> TwoWireService::readSmartCardMainMemory(uint8_t startAddress, uint16_t length) {
    std::vector<uint8_t> buf;
    sendCommand(0x30, startAddress, 0x00);
    for (uint16_t i = 0; i < length; ++i) {
        buf.push_back(readByte());
    }
    return buf;
}

/**
 * @brief 读取智能卡安全内存（4字节）
 * @return 安全内存数据
 */
std::vector<uint8_t> TwoWireService::readSmartCardSecurityMemory() {
    sendCommand(0x31, 0x00, 0x00);
    return readResponse(4);
}

/**
 * @brief 读取智能卡保护内存（4字节）
 * @return 保护内存数据
 */
std::vector<uint8_t> TwoWireService::readSmartCardProtectionMemory() {
    sendCommand(0x34, 0x00, 0x00);
    return readResponse(4);
}

/**
 * @brief 更新智能卡PSC（个人安全码，3字节）
 * @param psc 3字节PSC数组
 * @return 更新并校验成功返回true，否则返回false
 */
bool TwoWireService::updateSmartCardPSC(const uint8_t psc[3]) {
    // 写入PSC到安全内存（地址1-3）
    writeSmartCardSecurityMemory(1, psc[0]);
    writeSmartCardSecurityMemory(2, psc[1]);
    writeSmartCardSecurityMemory(3, psc[2]);

    // 读取校验
    std::vector<uint8_t> secmem = readSmartCardSecurityMemory();
    if (secmem.size() < 4) {
        return false;
    }

    if (secmem[1] != psc[0] || secmem[2] != psc[1] || secmem[3] != psc[2]) {
        return false;
    }
    return true;
}

/**
 * @brief 读取智能卡当前PSC（个人安全码）
 * @param out_psc 输出3字节PSC数组
 * @return 读取成功返回true，否则返回false
 */
bool TwoWireService::getSmartCardPSC(uint8_t out_psc[3]) {
    std::vector<uint8_t> secmem = readSmartCardSecurityMemory();
    if (secmem.size() < 4) {
        return false;
    }
    out_psc[0] = secmem[1];
    out_psc[1] = secmem[2];
    out_psc[2] = secmem[3];
    return true;
}

/**
 * @brief 保护智能卡（写入0xFFFFFFFF到保护内存）
 * @return 写入并校验成功返回true，否则返回false
 */
bool TwoWireService::protectSmartCard() {
    uint32_t value = 0xFFFFFFFF; // 保护内存全1

    // 写入4字节到保护内存
    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t byte = (value >> (8 * i)) & 0xFF;
        writeSmartCardProtectionMemory(i, byte);
    }

    // 读取校验
    auto check = readSmartCardProtectionMemory();
    if (check.size() != 4) return false;

    uint32_t readValue = 
        (check[3] << 24) |
        (check[2] << 16) |
        (check[1] << 8)  |
        (check[0]);

    return (readValue & value) == value;
}

/**
 * @brief 解锁智能卡（验证PSC并重置安全状态）
 * @param psc 3字节PSC数组
 * @return 解锁成功返回true，否则返回false
 */
bool TwoWireService::unlockSmartCard(const uint8_t psc[3]) {
    // 读取安全内存
    std::vector<uint8_t> secmem = readSmartCardSecurityMemory();
    if (secmem.size() < 1) {
        return false;
    }

    // 验证剩余尝试次数
    uint8_t sec = secmem[0];
    uint8_t pattern = 0;

    if (sec & 0b100) {
        pattern = 0b011;
    } else if (sec & 0b010) {
        pattern = 0b101;
    } else if (sec & 0b001) {
        pattern = 0b110;
    } else {
        // 无剩余尝试次数
        return false;
    }

    // 更新安全尝试次数
    updateSmartCardSecurityAttempts(pattern);

    // 发送PSC验证
    compareSmartCardVerificationData(1, psc[0]);
    compareSmartCardVerificationData(2, psc[1]);
    compareSmartCardVerificationData(3, psc[2]);

    // 重置安全内存
    writeSmartCardSecurityMemory(0, 0xFF);

    // 读取校验
    std::vector<uint8_t> secmemAfter = readSmartCardSecurityMemory();
    if (secmemAfter.size() < 1 || secmemAfter[0] != 0x07) {
        return false;
    }

    return true;
}

// ================== 嗅探器：辅助函数 ==================

/**
 * @brief 推送嗅探事件到队列（IRAM_ATTR：中断中执行）
 * @param type 事件类型（1=START，2=STOP，3=DATA）
 * @param data 事件数据（DATA事件为字节值，其他为0）
 * @note 队列满时丢弃事件，线程安全（临界区保护）
 */
inline void IRAM_ATTR TwoWireService::pushEvent(uint8_t type, uint8_t data) {
    uint16_t next = (sn_qHead + 1) % SNIFF_Q_SIZE;
    if (next == sn_qTail) return; // 队列溢出，丢弃

    // 写入事件到队列
    sn_q[sn_qHead].type = type;
    sn_q[sn_qHead].data = data;

    sn_qHead = next;
}

/**
 * @brief 从嗅探队列弹出事件（非阻塞）
 * @param type 输出事件类型
 * @param data 输出事件数据
 * @return 成功弹出返回true，队列为空返回false
 * @note 临界区保护，保证线程安全
 */
bool TwoWireService::popEvent(uint8_t& type, uint8_t& data) {
    if (sn_qTail == sn_qHead) return false;

    portENTER_CRITICAL(&sn_mux); // 进入临界区
    if (sn_qTail == sn_qHead) { portEXIT_CRITICAL(&sn_mux); return false; }

    // 读取队列事件
    type = sn_q[sn_qTail].type;
    data = sn_q[sn_qTail].data;

    sn_qTail = (sn_qTail + 1) % SNIFF_Q_SIZE;
    portEXIT_CRITICAL(&sn_mux); // 退出临界区
    return true;
}

// ================== 嗅探器核心函数 ==================

/**
 * @brief CLK中断处理函数封装（IRAM_ATTR：中断中执行）
 * @param arg 类实例指针
 */
void IRAM_ATTR TwoWireService::clk_isr_thunk(void* arg) {
    reinterpret_cast<TwoWireService*>(arg)->onClkRisingISR();
}

/**
 * @brief IO中断处理函数封装（IRAM_ATTR：中断中执行）
 * @param arg 类实例指针
 */
void IRAM_ATTR TwoWireService::io_isr_thunk(void* arg) {
    reinterpret_cast<TwoWireService*>(arg)->onIoChangeISR();
}

/**
 * @brief CLK上升沿中断处理（IRAM_ATTR：中断中执行）
 * @note 解析数据位，完成字节后推送DATA事件
 */
void IRAM_ATTR TwoWireService::onClkRisingISR() {
    if (!sn_active) return;

    // 验证START条件
    if (sn_startPending) {
        sn_startPending = false;
        sn_inFrame = true;
        pushEvent(/*START=*/1, 0);
    }

    if (!sn_inFrame) return;  // 忽略帧外的比特

    // 读取IO电平，组装字节（低位优先）
    int io = gpio_get_level((gpio_num_t)ioPin);
    if (io) sn_currentByte |= (1u << sn_bitIndex);
    sn_bitIndex++;
    // 1字节（8位）读取完成
    if (sn_bitIndex >= 8) {
        pushEvent(/*DATA=*/3, sn_currentByte);
        sn_bitIndex = 0;
        sn_currentByte = 0;
    }
}

/**
 * @brief IO电平变化中断处理（IRAM_ATTR：中断中执行）
 * @note 检测START/STOP协议帧边界
 */
void IRAM_ATTR TwoWireService::onIoChangeISR() {
    if (!sn_active) return;

    int clk = gpio_get_level((gpio_num_t)clkPin);
    int io  = gpio_get_level((gpio_num_t)ioPin);

    if (clk) {
        // START条件：IO从高变低（CLK为高）
        if (sn_lastIO == 1 && io == 0) {
            if (!sn_inFrame && !sn_startPending) {
                sn_bitIndex = 0;
                sn_currentByte = 0;
                sn_startPending = true;   // START待确认
            }
        }
        // STOP条件：IO从低变高（CLK为高）
        else if (sn_lastIO == 0 && io == 1) {
            if (sn_inFrame) {
                sn_inFrame = false;
                sn_bitIndex = 0;
                sn_currentByte = 0;
                pushEvent(/*STOP=*/2, 0);
            } else if (sn_startPending) {
                // START取消（无数据位）
                sn_startPending = false;
            }
        }
    }
    sn_lastIO = (uint8_t)io;
}

/**
 * @brief 启动两线总线嗅探器
 * @return 启动成功返回true，否则返回false
 * @note 配置CLK/IO引脚为中断输入，注册ISR处理函数
 */
bool TwoWireService::startSniffer() {
    if (clkPin == 0xFF || ioPin == 0xFF) return false;

    // 安装ISR服务（仅首次调用）
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return false;
        }
        isr_service_installed = true;
    }

    // 设置CLK/IO为输入模式（非侵入式嗅探）
    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)clkPin, GPIO_FLOATING);

    gpio_set_direction((gpio_num_t)ioPin,  GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)ioPin,  GPIO_PULLUP_ONLY); 

    // 初始化嗅探器状态
    sn_active = false;
    sn_inFrame      = false;
    sn_startPending = false;
    sn_bitIndex = 0;
    sn_currentByte = 0;
    sn_lastIO = (uint8_t)gpio_get_level((gpio_num_t)ioPin);
    sn_qHead = sn_qTail = 0;

    // 配置中断类型
    gpio_set_intr_type((gpio_num_t)clkPin,
        SNIFF_SAMPLE_ON_NEGEDGE ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE);
    gpio_set_intr_type((gpio_num_t)ioPin,  GPIO_INTR_ANYEDGE);

    // 注册中断处理函数
    gpio_isr_handler_add((gpio_num_t)clkPin, &TwoWireService::clk_isr_thunk, this);
    gpio_isr_handler_add((gpio_num_t)ioPin,  &TwoWireService::io_isr_thunk,  this);

    // 启用中断
    gpio_intr_enable((gpio_num_t)clkPin);
    gpio_intr_enable((gpio_num_t)ioPin);

    sn_active = true;
    return true;
}

/**
 * @brief 停止两线总线嗅探器
 * @note 禁用中断，移除ISR处理函数，恢复引脚状态
 */
void TwoWireService::stopSniffer() {
    if (!sn_active) return;
    sn_active = false;

    // 移除ISR处理函数
    gpio_isr_handler_remove((gpio_num_t)clkPin);
    gpio_isr_handler_remove((gpio_num_t)ioPin);

    // 重置引脚为输入浮空状态
    gpio_set_direction((gpio_num_t)clkPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)clkPin, GPIO_FLOATING);
    gpio_set_direction((gpio_num_t)ioPin, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)ioPin, GPIO_FLOATING);
}

/**
 * @brief 获取下一个嗅探事件（非阻塞）
 * @param type 输出事件类型
 * @param data 输出事件数据
 * @return 成功获取返回true，队列为空返回false
 */
bool TwoWireService::getNextSniffEvent(uint8_t& type, uint8_t& data) {
    return popEvent(type, data);
}

/**
 * @brief 打印一次嗅探事件（格式化输出到Stream）
 * @param out 输出流（如Serial）
 * @note 格式：[ 0xXX 0xXX ]（START=[，STOP=]，DATA=0xXX）
 */
void TwoWireService::printSniffOnce(Stream& out) {
    uint8_t t, d;
    while (getNextSniffEvent(t, d)) {
        if (t == 1)      { out.print('['); }          // START事件
        else if (t == 2) { out.println(']'); }        // STOP事件
        else if (t == 3) { char buf[6]; sprintf(buf, " 0x%02X", d); out.print(buf); } // DATA事件
        else             { out.println("U"); }        // 未知事件
    }
}