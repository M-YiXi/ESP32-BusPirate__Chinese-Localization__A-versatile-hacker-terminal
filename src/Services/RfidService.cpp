#include "RfidService.h"

// -------------------------- 基础配置与初始化 --------------------------

/**
 * @brief 配置RFID模块的I2C引脚（SDA/SCL）
 * @param sda I2C数据引脚（SDA）
 * @param scl I2C时钟引脚（SCL）
 * @note 仅保存引脚参数，不执行实际初始化，需调用begin()完成初始化
 */
void RfidService::configure(uint8_t sda, uint8_t scl) {
    _sda = sda;          // 保存SDA引脚号
    _scl = scl;          // 保存SCL引脚号
    _configured = true;  // 标记引脚已配置
}

/**
 * @brief 初始化RFID模块（启动I2C通信并检测模块）
 * @return 初始化成功返回true，失败返回false
 * @note 仅当引脚已配置（configure调用过）时执行，避免重复初始化
 */
bool RfidService::begin() {
    // 未配置引脚直接返回失败
    if (!_configured) {
        return false;
    }

    // 已初始化过直接返回成功
    if (_begun) {
        return true;
    }

    // 启动RFID模块（基于配置的SDA/SCL引脚）
    _begun = _rfid.begin(_sda, _scl);
    return _begun;
}

// -------------------------- 核心操作（读/写/克隆/擦除） --------------------------

/**
 * @brief 读取RFID卡数据
 * @param cardBaudRate 卡片通信波特率（适配不同类型RFID卡）
 * @return 操作状态码（0=成功，非0=对应错误码）
 */
int RfidService::read(int cardBaudRate)      { return _rfid.read(cardBaudRate); }

/**
 * @brief 向RFID卡写入数据
 * @param cardBaudRate 卡片通信波特率
 * @return 操作状态码（0=成功，非0=对应错误码）
 */
int RfidService::write(int cardBaudRate)     { return _rfid.write(cardBaudRate); }

/**
 * @brief 向RFID卡写入NDEF格式数据（NFC标签专用）
 * @return 操作状态码（0=成功，非0=对应错误码）
 */
int RfidService::write_ndef()                { return _rfid.write_ndef(); }

/**
 * @brief 擦除RFID卡中的数据
 * @return 操作状态码（0=成功，非0=对应错误码）
 */
int RfidService::erase()                     { return _rfid.erase(); }

/**
 * @brief 克隆RFID卡数据（复制UID/数据到目标卡）
 * @param checkSak 是否校验SAK值（增强克隆准确性）
 * @return 操作状态码（0=成功，非0=对应错误码）
 */
int RfidService::clone(bool checkSak)        { return _rfid.clone(checkSak); }

// -------------------------- 只读属性获取（UID/SAK/ATQA等） --------------------------

/**
 * @brief 获取RFID卡的UID（唯一标识）
 * @return UID字符串（十六进制格式，如"01 23 45 67"）
 */
std::string RfidService::uid() const         { return std::string(_rfid.printableUID.uid.c_str()); }

/**
 * @brief 获取RFID卡的SAK值（Select Acknowledge，选择确认码）
 * @return SAK字符串（十六进制格式，如"08"）
 * @note SAK用于标识卡片类型和功能
 */
std::string RfidService::sak() const         { return std::string(_rfid.printableUID.sak.c_str()); }

/**
 * @brief 获取RFID卡的ATQA值（Answer To Request A，请求应答码）
 * @return ATQA字符串（十六进制格式，如"00 04"）
 * @note ATQA是卡片对请求命令的应答，用于识别卡片类型
 */
std::string RfidService::atqa() const        { return std::string(_rfid.printableUID.atqa.c_str()); }

/**
 * @brief 获取RFID卡的PICC类型（卡片类型描述）
 * @return PICC类型字符串（如"MIFARE Classic 1K"）
 */
std::string RfidService::piccType() const    { return std::string(_rfid.printableUID.picc_type.c_str()); }

/**
 * @brief 获取RFID卡所有页面的原始数据
 * @return 所有页面数据拼接的字符串（十六进制格式）
 */
std::string RfidService::allPages() const    { return std::string(_rfid.strAllPages.c_str()); }

// -------------------------- 可写属性设置（UID/SAK/ATQA） --------------------------

/**
 * @brief 设置要写入/克隆的RFID卡UID
 * @param uidHex UID的十六进制字符串（如"01234567"或"01 23 45 67"）
 */
void RfidService::setUid(const std::string& uidHex) {
    // 将UID字符串赋值到RFID模块的可打印UID结构体中
    _rfid.printableUID.uid  = uidHex.c_str();
}

/**
 * @brief 设置要写入/克隆的RFID卡SAK值
 * @param sakHex SAK的十六进制字符串（如"08"）
 */
void RfidService::setSak(const std::string& sakHex) {
    _rfid.printableUID.sak = sakHex.c_str();     // 示例："08"
}

/**
 * @brief 设置要写入/克隆的RFID卡ATQA值
 * @param atqaHex ATQA的十六进制字符串（如"00 04"）
 */
void RfidService::setAtqa(const std::string& atqaHex) {
    _rfid.printableUID.atqa = atqaHex.c_str();    // 示例："00 04"
}

// -------------------------- 页面数据管理 --------------------------

/**
 * @brief 获取RFID卡的总页面数
 * @return 总页面数（如MIFARE Classic 1K为64页）
 */
int  RfidService::totalPages() const         { return _rfid.totalPages; }

/**
 * @brief 获取RFID卡的有效数据页面数（排除系统页）
 * @return 数据页面数
 */
int  RfidService::dataPages() const          { return _rfid.dataPages; }

/**
 * @brief 检查页面数据读取是否成功
 * @return 读取成功返回true，失败返回false
 * @note 需同时满足：pageReadSuccess标记为true 且 pageReadStatus为SUCCESS
 */
bool RfidService::pageReadOk() const {
    return _rfid.pageReadSuccess && (_rfid.pageReadStatus == RFIDInterface::SUCCESS);
}

// -------------------------- 辅助功能 --------------------------

/**
 * @brief 根据操作状态码获取对应的状态描述信息
 * @param rc 操作返回的状态码
 * @return 状态描述字符串（如"Success"、"Card not found"）
 */
std::string RfidService::statusMessage(int rc) const {
    return std::string(_rfid.statusMessage(rc).c_str());
}

/**
 * @brief 加载RFID卡的dump数据（十六进制原始数据）到模块
 * @param dump dump数据字符串（每行对应一个页面，末尾建议带换行）
 * @note 若dump末尾无换行，自动补充换行符，确保模块解析正常
 */
void RfidService::loadDump(const std::string& dump) {
    if (!dump.empty() && dump.back() == '\n') {
        _rfid.strAllPages = dump.c_str(); // 已有换行，直接赋值
    } else {
        _rfid.strAllPages = (dump + "\n").c_str(); // 补充换行后赋值
    }
}

/**
 * @brief 解析已加载的RFID卡数据（将原始数据解析为页面/字段格式）
 * @note 需先调用loadDump加载数据，再调用此函数解析
 */
void RfidService::parseData() {
    _rfid.parse_data();
}

/**
 * @brief 获取支持的RFID标签类型列表
 * @return 标签类型向量（当前支持MIFARE/ISO14443A、FeliCa）
 */
std::vector<std::string> RfidService::getTagTypes() const {
    return { " MIFARE / ISO14443A", " FeliCa" };
}

/**
 * @brief 获取MIFARE系列卡片类型列表
 * @return MIFARE类型向量（经典型16字节页、NTAG/超轻型4字节页）
 */
std::vector<std::string> RfidService::getMifareFamily() const {
    return  { " MIFARE Classic (16 bytes)", " NTAG/Ultralight (4 bytes)" };
}