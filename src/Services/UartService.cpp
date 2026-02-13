#include "UartService.h"

/**
 * @brief 配置UART串口参数（波特率、数据位/校验位/停止位、引脚、电平反转）
 * @param baud 波特率（如9600/115200）
 * @param config UART配置字（可通过buildUartConfig生成）
 * @param rx RX接收引脚号
 * @param tx TX发送引脚号
 * @param inverted 是否电平反转（true=反转，false=正常）
 * @note 配置前先停止Serial1，避免配置冲突
 */
void UartService::configure(unsigned long baud, uint32_t config, uint8_t rx, uint8_t tx, bool inverted) {
    Serial1.end(); // 重新配置前先停止串口
    Serial1.begin(baud, config, rx, tx, inverted);
}

/**
 * @brief 关闭UART串口并释放资源
 */
void UartService::end() {
    Serial1.end();
}

/**
 * @brief 读取一行输入（支持退格键处理，以\r或\n结束）
 * @return 读取到的文本行（不含换行符）
 * @note 支持退格删除（\b/127），输入回显到串口，自动处理\r\n换行
 */
std::string UartService::readLine() {
    std::string input;
    bool lastWasCR = false; // 上一个字符是否是\r

    while (true) {
        if (!Serial1.available()) continue; // 无数据则等待
        
        char c = Serial1.read();

        if (c == '\r') {
            lastWasCR = true;
            Serial1.println(); // 回显换行
            break; // 回车结束输入
        }

        if (c == '\n') {
            if (!lastWasCR) {
                Serial1.println(); // 换行结束输入
                break;
            }
            continue; // 忽略连续的\n
        }

        // 处理退格键（\b或127）
        if (c == '\b' || c == 127) {
            if (!input.empty()) {
                input.pop_back(); // 删除最后一个字符
                Serial1.print("\b \b"); // 串口回显退格（删除显示的字符）
            }
        } else {
            input += c; // 追加有效字符
            Serial1.print(c); // 回显字符
            lastWasCR = false;
        }
    }

    return input;
}

/**
 * @brief 发送字符串到UART（无换行）
 * @param msg 待发送的字符串
 */
void UartService::print(const std::string& msg) {
    Serial1.print(msg.c_str());
}

/**
 * @brief 发送字符串到UART（带换行）
 * @param msg 待发送的字符串
 */
void UartService::println(const std::string& msg) {
    Serial1.println(msg.c_str());
}

/**
 * @brief 检查UART接收缓冲区是否有数据
 * @return 有数据返回true，无数据返回false
 */
bool UartService::available() const {
    return Serial1.available();
}

/**
 * @brief 读取单个字符（阻塞，直到有数据）
 * @return 读取到的字符
 */
char UartService::read() {
    return Serial1.read();
}

/**
 * @brief 发送单个字符到UART
 * @param c 待发送的字符
 */
void UartService::write(char c) {
    Serial1.write(c);
}

/**
 * @brief 发送原始字符串数据到UART（无编码转换）
 * @param str 待发送的字符串
 */
void UartService::write(const std::string& str) {
    Serial1.write(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
}

/**
 * @brief 执行字节码序列（支持写数据/读数据/延时操作）
 * @param bytecodes 字节码数组
 * @return 读取到的所有数据（仅Read指令会产生返回值）
 * @note 超时时间：Read指令单次超时2秒，支持重复次数配置
 */
std::string UartService::executeByteCode(const std::vector<ByteCode>& bytecodes) {
    std::string result;
    uint32_t timeout = 2000; // 读取超时时间（2秒）
    uint32_t start;
    uint32_t received = 0;

    for (const ByteCode& code : bytecodes) {
        switch (code.getCommand()) {
            case ByteCodeEnum::Write:
                // 重复发送指定数据
                for (uint32_t i = 0; i < code.getRepeat(); ++i) {
                    Serial1.write(code.getData());
                }
                break;

            case ByteCodeEnum::Read:
                // 读取指定数量的字节（带超时）
                start = millis();
                received = 0; // 重置已接收计数
                while (received < code.getRepeat() && (millis() - start < timeout)) {
                    if (Serial1.available()) {
                        char c = Serial1.read();
                        result += c;
                        ++received;
                    } else {
                        delay(10); // 无数据时短暂延时，降低CPU占用
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

/**
 * @brief 切换UART波特率（无需重新初始化串口）
 * @param newBaud 新的波特率值
 */
void UartService::switchBaudrate(unsigned long newBaud) {
    Serial1.updateBaudRate(newBaud);
}

/**
 * @brief 刷新UART发送缓冲区（等待所有数据发送完成）
 * @note 注意：此处原代码误写为Serial.flush()，实际应使用Serial1.flush()
 */
void UartService::flush() {
    Serial1.flush(); // 修正原代码的Serial→Serial1错误
}

/**
 * @brief 清空UART接收缓冲区
 * @note 最多读取512字节，避免缓冲区溢出
 */
void UartService::clearUartBuffer() {
    const size_t maxBytes = 512; // 缓冲区最大清理字节数
    size_t count = 0;
    while (count < maxBytes && available()) {
        read(); // 读取并丢弃数据
        count++;
    }
}

/**
 * @brief 构建UART配置字（根据数据位/校验位/停止位）
 * @param dataBits 数据位（5/6/7/8）
 * @param parity 校验位（'N'=无校验，'E'=偶校验，'O'=奇校验）
 * @param stopBits 停止位（1/2）
 * @return 对应的UART配置字（如SERIAL_8N1/SERIAL_8E2）
 */
uint32_t UartService::buildUartConfig(uint8_t dataBits, char parity, uint8_t stopBits) {
    // 默认配置：8位数据位、无校验、1位停止位
    uint32_t config = SERIAL_8N1;
    
    // 根据数据位和停止位调整基础配置
    if (dataBits == 5) config = (stopBits == 2) ? SERIAL_5N2 : SERIAL_5N1;
    else if (dataBits == 6) config = (stopBits == 2) ? SERIAL_6N2 : SERIAL_6N1;
    else if (dataBits == 7) config = (stopBits == 2) ? SERIAL_7N2 : SERIAL_7N1;
    else if (dataBits == 8) config = (stopBits == 2) ? SERIAL_8N2 : SERIAL_8N1;

    // 添加校验位配置（偶校验/奇校验）
    if (parity == 'E') config |= 0x02; // 偶校验
    else if (parity == 'O') config |= 0x01; // 奇校验

    return config;
}

/*
XMODEM协议相关功能
*/
File* UartService::currentFile = nullptr; // 当前XMODEM传输的文件指针

/**
 * @brief 设置XMODEM块大小（每个数据块的字节数）
 * @param size 块大小（如128/1024）
 */
void UartService::setXmodemBlockSize(int32_t size) {
    xmodemBlockSize = size;
}

/**
 * @brief 设置XMODEM块ID长度（字节数）
 * @param size ID长度（如1/2/4）
 */
void UartService::setXmodemIdSize(int8_t size) {
    xmodemIdSize = size;
}

/**
 * @brief 获取当前XMODEM块大小
 * @return 块大小值
 */
int32_t UartService::getXmodemBlockSize() const {
    return xmodemBlockSize;
}

/**
 * @brief 获取当前XMODEM块ID长度
 * @return ID长度值
 */
int8_t UartService::getXmodemIdSize() const {
    return xmodemIdSize;
}

/**
 * @brief 设置XMODEM校验方式（CRC/和校验）
 * @param enabled true=启用CRC校验，false=使用和校验
 */
void UartService::setXmodemCrc(bool enabled) {
    xmodemProtocol = enabled ? XModem::ProtocolType::CRC_XMODEM : XModem::ProtocolType::XMODEM;
}

/**
 * @brief 设置XMODEM接收块回调函数（接收数据时调用）
 * @param handler 回调函数指针：参数(块ID, ID长度, 数据, 数据长度)，返回是否接收成功
 */
void UartService::setXmodemReceiveHandler(bool (*handler)(void*, size_t, byte*, size_t)) {
    xmodem.setRecieveBlockHandler(handler); // 注：原代码拼写错误recieve→receive，保留原写法
}

/**
 * @brief 设置XMODEM发送块回调函数（发送数据时调用）
 * @param handler 回调函数指针：参数(块ID, ID长度, 数据缓冲区, 数据长度)
 */
void UartService::setXmodemSendHandler(void (*handler)(void*, size_t, byte*, size_t)) {
    xmodem.setBlockLookupHandler(handler);
}

/**
 * @brief 初始化XMODEM协议（配置串口、块大小、ID长度、校验方式）
 */
void UartService::initXmodem() {
    xmodem.begin(Serial1, xmodemProtocol); // 绑定Serial1到XMODEM
    xmodem.setDataSize(xmodemBlockSize);   // 设置块大小
    xmodem.setIdSize(xmodemIdSize);        // 设置块ID长度
}

/**
 * @brief XMODEM发送块回调函数（从文件读取指定块数据）
 * @param blk_id 块ID指针
 * @param idSize ID长度
 * @param data 数据缓冲区（输出）
 * @param dataSize 数据长度（块大小）
 * @note IRAM_ATTR：若需中断中执行可添加，此处保留原逻辑
 */
void UartService::blockLookupHandler(void* blk_id, size_t idSize, byte* data, size_t dataSize) {
    if (!currentFile) {
        return; // 无文件指针，直接返回
    }

    // 将块ID转换为整数（大端序）
    uint32_t blockId = 0;
    for (size_t i = 0; i < idSize; ++i) {
        blockId = (blockId << 8) | ((uint8_t*)blk_id)[i];
    }

    // 计算文件偏移量（块ID × 块大小）
    size_t offset = blockId * dataSize;
    if (!currentFile->seek(offset)) {
        return; // 偏移定位失败
    }

    // 从文件读取数据到缓冲区
    size_t readBytes = currentFile->read(data, dataSize);
    // 不足块大小的部分用0x1A（EOF字符）填充
    if (readBytes < dataSize) {
        memset(data + readBytes, 0x1A, dataSize - readBytes);
    }

    // 打印发送进度（汉化输出）
    Serial.printf("正在发送块: %u\n\r", (unsigned int)blockId);
}

/**
 * @brief XMODEM接收块回调函数（将接收的块数据写入文件）
 * @param blk_id 块ID指针
 * @param idSize ID长度
 * @param data 接收的数据缓冲区
 * @param dataSize 数据长度（块大小）
 * @return 写入成功返回true，失败返回false
 */
bool UartService::receiveBlockHandler(void* blk_id, size_t idSize, byte* data, size_t dataSize) {
    if (!currentFile) {
        return false; // 无文件指针，接收失败
    }

    // 将块ID转换为整数（大端序）
    uint32_t blockId = 0;
    for (size_t i = 0; i < idSize; ++i) {
        blockId = (blockId << 8) | ((uint8_t*)blk_id)[i];
    }
    // 打印接收进度（汉化输出）
    Serial.printf("正在接收块: %u\r\n", (unsigned int)blockId);

    // 写入数据到文件，返回是否全部写入
    return currentFile->write(data, dataSize) == dataSize;
}

/**
 * @brief 通过XMODEM协议发送文件
 * @param file 待发送的文件对象（已打开）
 * @return 发送成功返回true，失败返回false
 * @note 自动计算块数，构建块ID数组，发送完成后释放内存
 */
bool UartService::xmodemSendFile(File& file) {
    if (!file || file.isDirectory()) return false; // 文件无效或为目录，返回失败

    // 初始化XMODEM
    initXmodem();
    currentFile = &file; // 绑定当前文件
    xmodem.setBlockLookupHandler(blockLookupHandler); // 设置发送回调

    // 计算文件总块数
    size_t fileSize = file.size();
    size_t blockSize = xmodemBlockSize;
    size_t totalBlocks = (fileSize + blockSize - 1) / blockSize; // 向上取整
    const size_t idSize = xmodemIdSize;

    // 分配内存存储所有块ID（大端序）
    byte* all_ids = (byte*)malloc(totalBlocks * idSize);
    for (size_t i = 0; i < totalBlocks; ++i) {
        unsigned long long blk_id = i + 1; // 块ID从1开始
        for (size_t j = 0; j < idSize; ++j) {
            all_ids[i * idSize + j] = (blk_id >> (8 * (idSize - j - 1))) & 0xFF;
        }
    }

    // 分配虚拟数据缓冲区数组（实际数据由回调读取）
    byte** dummy_data = (byte**)malloc(sizeof(byte*) * totalBlocks);
    size_t* dummy_lens = (size_t*)malloc(sizeof(size_t) * totalBlocks);
    for (size_t i = 0; i < totalBlocks; ++i) {
        dummy_data[i] = nullptr; // 无预分配数据
        dummy_lens[i] = blockSize; // 每个块的大小
    }

    // 构建批量发送容器
    struct XModem::bulk_data container = {
        .data_arr = dummy_data, // 数据缓冲区数组
        .len_arr = dummy_lens,  // 块长度数组
        .id_arr = all_ids,      // 块ID数组
        .count = totalBlocks    // 总块数
    };

    // 执行批量发送
    bool result = xmodem.send_bulk_data(container);

    // 释放内存，清空文件指针
    free(all_ids);
    free(dummy_data);
    free(dummy_lens);
    currentFile = nullptr;

    return result;
}

/**
 * @brief 通过XMODEM协议接收文件并写入指定文件
 * @param file 接收数据的文件对象（已打开，可写）
 * @return 接收成功返回true，失败返回false
 */
bool UartService::xmodemReceiveToFile(File& file) {
    if (!file || file.isDirectory()) return false; // 文件无效或为目录，返回失败

    // 初始化XMODEM
    initXmodem();
    currentFile = &file; // 绑定当前文件

    // 设置接收回调，执行接收
    xmodem.setRecieveBlockHandler(receiveBlockHandler);
    bool ok = xmodem.receive();

    // 清空文件指针
    currentFile = nullptr;
    return ok;
}