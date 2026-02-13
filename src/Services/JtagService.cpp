// 移植 https://github.com/Aodrulez/blueTag/ 到 ESP32 平台
// 感谢原作者 Atul Alex Cherian

/* 
    [ blueTag - 基于 RP2040 开发板的硬件黑客多功能工具 ]

        灵感来源于 JTAGulator。 

    [参考资料 & 特别致谢]
        https://github.com/grandideastudio/jtagulator
        https://research.kudelskisecurity.com/2019/05/16/swd-arms-alternative-to-jtag/
        https://github.com/jbentham/picoreg
        https://github.com/szymonh/SWDscan
        Yusufss4 (https://gist.github.com/amullins83/24b5ef48657c08c4005a8fab837b7499?permalink_comment_id=4554839#gistcomment-4554839)
        Arm 调试接口架构规范 (debug_interface_v5_2_architecture_specification_IHI0031F.pdf)
        
        Flashrom 支持 : 
            https://www.flashrom.org/supported_hw/supported_prog/serprog/serprog-protocol.html
            https://github.com/stacksmashing/pico-serprog

        Openocd 支持  : 
            http://dangerousprototypes.com/blog/2009/10/09/bus-pirate-raw-bitbang-mode/
            http://dangerousprototypes.com/blog/2009/10/27/binary-raw-wire-mode/
            https://github.com/grandideastudio/jtagulator/blob/master/PropOCD.spin
            https://github.com/DangerousPrototypes/Bus_Pirate/blob/master/Firmware/binIO.c

        USB转串口支持 :
            https://github.com/xxxajk/pico-uart-bridge
            https://github.com/Noltari/pico-uart-bridge

        CMSIS-DAP 支持 :
            https://github.com/majbthrd/DapperMime
            https://github.com/raspberrypi/debugprobe
*/

#include "JtagService.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define SWD_DELAY_US 5                     // SWD协议延时（微秒）
#define LINE_RESET_CLK_CYCLES 52           // 线路复位时钟周期数
#define JTAG_TO_SWD_CMD 0xE79E             // JTAG转SWD命令码
#define SWD_TO_JTAG_CMD 0xE73C             // SWD转JTAG命令码
#define SWDP_ACTIVATION_CODE 0x1A          // SWD调试端口激活码
#define MAX_DEVICES_LEN      32            // 单个JTAG链允许的最大设备数量
#define MIN_IR_LEN           2             // 单设备指令寄存器(IR)最小长度（符合IEEE 1149.1标准）
#define MAX_IR_LEN           32            // 单设备指令寄存器(IR)最大长度
#define MAX_IR_CHAIN_LEN     MAX_DEVICES_LEN * MAX_IR_LEN   // 选中IR时JTAG链的最大总长度
#define MAX_DR_LEN           4096          // 数据寄存器(DR)最大长度

// --- JTAG 相关函数 ---

void JtagService::configureJtag(uint8_t tck, uint8_t tms, uint8_t tdi, uint8_t tdo, int trst) {
    _pinTCK = tck;
    _pinTMS = tms;
    _pinTDI = tdi;
    _pinTDO = tdo;
    _pinTRST = trst;

    // 配置JTAG引脚方向和初始电平
    gpio_set_direction((gpio_num_t)_pinTCK, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)_pinTCK, 0);
    gpio_set_direction((gpio_num_t)_pinTMS, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)_pinTDI, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)_pinTDO, GPIO_MODE_INPUT);

    // 配置TRST复位引脚（若指定）
    if (trst >= 0) {
        gpio_set_direction((gpio_num_t)_pinTRST, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)_pinTRST, 1);  // 取消断言（释放复位）
    }
}

void JtagService::tckPulse() {
    tdoRead(); // 时钟脉冲同时读取TDO引脚值
}

void JtagService::tdiWrite(bool val) {
    // 向TDI引脚写入电平值
    gpio_set_level((gpio_num_t)_pinTDI, val ? 1 : 0);
}

void JtagService::tmsWrite(bool val) {
    // 向TMS引脚写入电平值（控制JTAG状态机）
    gpio_set_level((gpio_num_t)_pinTMS, val ? 1 : 0);
}

bool JtagService::tdoRead() {
    // 产生TCK时钟脉冲并读取TDO引脚值
    gpio_set_level((gpio_num_t)_pinTCK, 1);
    bool val = gpio_get_level((gpio_num_t)_pinTDO);
    gpio_set_level((gpio_num_t)_pinTCK, 0);
    return val;
}

void JtagService::restoreIdle() {
    // 恢复JTAG状态机到运行测试/空闲状态
    tmsWrite(true);
    for (int i = 0; i < 5; ++i) tckPulse();
    tmsWrite(false);
    tckPulse(); // 进入运行测试/空闲状态
}

void JtagService::enterShiftDR() {
    // 进入数据寄存器(DR)移位状态
    tmsWrite(true); tckPulse(); // 选择DR寄存器
    tmsWrite(false); tckPulse(); // 捕获DR数据
    tmsWrite(false); tckPulse(); // 进入DR移位状态
}

void JtagService::enterShiftIR() {
    // 进入指令寄存器(IR)移位状态
    tmsWrite(true); tckPulse(); // 选择DR寄存器
    tmsWrite(true); tckPulse(); // 选择IR寄存器
    tmsWrite(false); tckPulse(); // 捕获IR数据
    tmsWrite(false); tckPulse(); // 进入IR移位状态
}

uint32_t JtagService::bitReverse(uint32_t n) {
    // 32位数据按位反转（适配JTAG数据传输格式）
    uint32_t r = 0;
    for (int i = 0; i < 32; ++i) {
        r <<= 1;
        r |= n & 1;
        n >>= 1;
    }
    return r;
}

uint32_t JtagService::shiftArray(uint32_t pattern, int bits) {
    // 移位发送指定长度的比特流并返回读取结果
    uint32_t result = 0;
    for (int i = 1; i <= bits; ++i) {
        if (i == bits) tmsWrite(true); // 最后一位时切换状态机
        tdiWrite(pattern & 1);
        pattern >>= 1;
        result <<= 1;
        result |= tdoRead();
    }
    return result;
}

uint32_t JtagService::sendData(uint32_t pattern, int bits) {
    // 向JTAG链发送数据并返回读取结果
    enterShiftDR();
    uint32_t out = shiftArray(pattern, bits);
    tmsWrite(true); tckPulse(); // 更新DR寄存器
    tmsWrite(false); tckPulse(); // 回到运行测试/空闲状态
    return out;
}

int JtagService::detectDevices() {
    // 检测JTAG链中的设备数量
    restoreIdle();
    enterShiftIR();
    tdiWrite(true);
    for (int i = 0; i < MAX_IR_CHAIN_LEN; ++i) tckPulse();
    tmsWrite(true); tckPulse(); // 退出IR移位-1
    tmsWrite(true); tckPulse(); // 更新IR寄存器
    tmsWrite(true); tckPulse(); // 选择DR寄存器
    tmsWrite(false); tckPulse(); // 捕获DR数据
    tmsWrite(false); tckPulse(); // 进入DR移位状态

    int x;
    // 发送探测时钟脉冲
    for(x = 0; x < MAX_DEVICES_LEN; x++)
    {
        tckPulse();
    }

    // 检测设备数量
    tdiWrite(false);
    for(x = 0; x < (MAX_DEVICES_LEN - 1); x++) {
        if(tdoRead() == false) {
            break;
        }
    }

    // 超出最大设备数则返回0
    if (x > (MAX_DEVICES_LEN - 1))
    {
        x = 0;
    }

    // 恢复空闲状态
    tmsWrite(true); tckPulse();
    tmsWrite(true); tckPulse();
    tmsWrite(false); tckPulse(); // 运行测试/空闲状态
    return (x);
}

void JtagService::getDeviceIDs(int count, std::vector<uint32_t>& ids) {
    // 获取JTAG链中指定数量设备的ID码
    ids.clear();
    restoreIdle();
    enterShiftDR();
    tdiWrite(true);
    tmsWrite(false);

    for (int i = 0; i < count; ++i) {
        uint32_t id = 0;
        // 读取32位设备ID
        for (int b = 0; b < 32; ++b) {
            id <<= 1;
            id |= tdoRead();
        }
        ids.push_back(bitReverse(id)); // 反转后存入列表
    }
    restoreIdle();
}

uint32_t JtagService::bypassTest(int count, uint32_t pattern) {
    // 执行JTAG旁路测试（验证链路连通性）
    if (count <= 0 || count > MAX_DEVICES_LEN) return 0;
    restoreIdle();
    enterShiftIR();
    tdiWrite(true);
    for (int i = 0; i < count * MAX_IR_LEN; ++i) tckPulse();
    tmsWrite(true); tckPulse(); // 退出IR移位-1
    tmsWrite(true); tckPulse(); // 更新IR寄存器
    tmsWrite(false); tckPulse(); // 运行测试/空闲状态
    return bitReverse(sendData(pattern, 32 + count));
}

bool JtagService::isValidDeviceID(uint32_t id) {
    // 验证设备ID是否有效（符合JTAG ID码格式规范）
    int idcode = (id & 0x7F) >> 1;
    int bank   = (id >> 8) & 0xF;
    return idcode > 1 && idcode <= 126 && bank <= 8;
}

void JtagService::jtagInitChannels(const std::vector<uint8_t>& pins, bool pulsePins) {
    // 初始化JTAG扫描用的引脚（配置上拉/下拉）
    for (auto pin : pins) {
        gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
        if (pulsePins) {
            gpio_pulldown_en((gpio_num_t)pin); // 使能下拉
        } else {
            gpio_pullup_en((gpio_num_t)pin);   // 使能上拉
        }
    }
}

void JtagService::jtagResetChannels(const std::vector<uint8_t>& pins, int trstPin, bool pulsePins) {
    // 复位JTAG扫描引脚（恢复默认状态）
    for (auto pin : pins) {
        if (trstPin >= 0 && pin == trstPin) {
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
            if (pulsePins) {
                gpio_pulldown_en((gpio_num_t)pin);
            } else {
                gpio_pullup_en((gpio_num_t)pin);
            }
        } else {
            gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
        }
    }
}

bool JtagService::scanJtagDevice(
    const std::vector<uint8_t>& pins,
    uint8_t& outTDI, uint8_t& outTDO,
    uint8_t& outTCK, uint8_t& outTMS,
    int& outTRST,
    std::vector<uint32_t>& outDeviceIDs,
    bool pulsePins,
    void (*onProgress)(size_t, size_t)
) {
    // 扫描指定引脚列表中的JTAG设备，自动识别引脚分配
    int channelCount = pins.size();
    uint32_t tempDeviceId;
    bool volatile foundPinout = false;
    outDeviceIDs.clear();
    size_t progressCount = 0;
    size_t maxPermutations = channelCount * (channelCount - 1) * (channelCount - 2) * (channelCount - 3);

    // 遍历所有引脚组合（TDI/TDO/TCK/TMS互不重复）
    for (auto tdi : pins) {
        for (auto tdo : pins) {
            if (tdi == tdo) continue;
            for (auto tck : pins) {
                if (tck == tdi || tck == tdo) continue;
                for (auto tms : pins) {
                    if (tms == tck || tms == tdo || tms == tdi) continue;

                    // 回调进度更新
                    if (onProgress) onProgress(++progressCount, maxPermutations);

                    // 初始化引脚并配置JTAG
                    jtagInitChannels(pins, pulsePins);
                    if (pulsePins) {
                        for (auto ch : pins) {
                            gpio_set_direction((gpio_num_t)ch, GPIO_MODE_INPUT);
                            gpio_pullup_en((gpio_num_t)ch);
                        }
                    }

                    configureJtag(tck, tms, tdi, tdo);
                    int deviceCount = detectDevices();

                    // 未检测到设备则跳过当前组合
                    if (deviceCount <= 0) {
                        continue;
                    }

                    // 执行旁路测试验证链路
                    uint32_t dataIn = esp_random();
                    uint32_t dataOut = bypassTest(deviceCount, dataIn);

                    // 数据回传一致说明链路有效
                    if (dataIn == dataOut) {
                        std::vector<uint32_t> ids;
                        getDeviceIDs(deviceCount, ids);
                        tempDeviceId = ids.empty() ? 0 : ids[0];

                        if (ids.empty()) {
                            Serial.println("       未读取到设备ID");
                            continue;
                        }

                        // 验证设备ID有效性
                        if (!isValidDeviceID(tempDeviceId)) {
                            continue;
                        }

                        // 找到有效JTAG引脚配置
                        foundPinout = true;
                        outTDI = tdi;
                        outTDO = tdo;
                        outTCK = tck;
                        outTMS = tms;
                        outTRST = -1;
                        outDeviceIDs = ids;

                        // 尝试识别TRST复位引脚
                        for (auto trst : pins) {
                            if (trst == tms || trst == tck || trst == tdo || trst == tdi) continue;

                            if (onProgress) onProgress(++progressCount, maxPermutations);

                            gpio_set_direction((gpio_num_t)trst, GPIO_MODE_INPUT);
                            if (pulsePins) {
                                gpio_pullup_en((gpio_num_t)trst);
                            } else {
                                gpio_pulldown_en((gpio_num_t)trst);
                            }

                            usleep(10);

                            std::vector<uint32_t> tmpIDs;
                            getDeviceIDs(1, tmpIDs);

                            // 复位后设备ID变化说明是TRST引脚
                            if (!tmpIDs.empty() && tmpIDs[0] != tempDeviceId) {
                                outTRST = trst;
                                break;
                            }
                        }

                        // 复位引脚并返回成功
                        jtagResetChannels(pins, outTRST, pulsePins);
                        return true;
                    }

                    jtagResetChannels(pins, outTRST, pulsePins);
                }
            }
        }
    }

    // 扫描完成未找到设备
    if (!foundPinout && onProgress) {
        onProgress(maxPermutations, maxPermutations);
    }

    return false;
}

// --- SWD 相关函数 ---

void JtagService::swdDelay() {
    // SWD协议时序延时
    esp_rom_delay_us(SWD_DELAY_US);
}

void JtagService::swdClockPulse() {
    // 产生SWD时钟脉冲（SWCLK引脚电平翻转）
    gpio_set_level((gpio_num_t)_pinSWCLK, 0);
    swdDelay();
    gpio_set_level((gpio_num_t)_pinSWCLK, 1);
    swdDelay();
}

void JtagService::swdSetReadMode() {
    // 设置SWDIO引脚为读取模式（输入）
    gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_INPUT);
}

void JtagService::swdSetWriteMode() {
    // 设置SWDIO引脚为写入模式（输出）
    gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_OUTPUT);
}

void JtagService::swdWriteBit(bool value) {
    // 向SWDIO引脚写入单个比特并产生时钟脉冲
    gpio_set_level((gpio_num_t)_pinSWDIO, value);
    swdClockPulse();
}

void JtagService::swdWriteBits(uint32_t value, int length) {
    // 向SWDIO引脚写入指定长度的比特流（低位先行）
    for (int i = 0; i < length; i++) {
        swdWriteBit((value >> i) & 1);
    }
}

bool JtagService::swdReadBit() {
    // 读取SWDIO引脚单个比特并产生时钟脉冲
    bool value = gpio_get_level((gpio_num_t)_pinSWDIO);
    swdClockPulse();
    return value;
}

bool JtagService::swdReadAck() {
    // 读取SWD协议应答位（3位ACK码），返回是否为确认应答(0b001)
    uint8_t ack = 0;
    for (int i = 0; i < 3; i++) {
        ack |= (swdReadBit() << i);
    }
    return ack == 0b001;
}

void JtagService::swdResetLineSWDJ() {
    // 复位SWD/JTAG线路（符合ARM SWD协议规范）
    swdSetWriteMode();
    gpio_set_level((gpio_num_t)_pinSWDIO, 1);
    for (int i = 0; i < LINE_RESET_CLK_CYCLES + 10; ++i) {
        swdClockPulse();
    }
}

void JtagService::swdToJTAG() {
    // SWD模式切换为JTAG模式
    swdResetLineSWDJ();
    swdWriteBits(SWD_TO_JTAG_CMD, 16);
}

void JtagService::swdArmWakeUp() {
    // 唤醒ARM芯片的SWD调试端口
    swdSetWriteMode();
    gpio_set_level((gpio_num_t)_pinSWDIO, 1);
    for (int i = 0; i < 8; i++) swdClockPulse();

    // ARM SWD唤醒序列
    const uint8_t alert[16] = {
        0x92, 0xF3, 0x09, 0x62, 0x95, 0x2D, 0x85, 0x86,
        0xE9, 0xAF, 0xDD, 0xE3, 0xA2, 0x0E, 0xBC, 0x19
    };
    for (int i = 0; i < 16; i++) swdWriteBits(alert[i], 8);

    swdWriteBits(0x00, 4); // 空闲位
    swdWriteBits(SWDP_ACTIVATION_CODE, 8); // 发送激活码
}

bool JtagService::swdTrySWDJ(uint32_t& idcodeOut) {
    // 尝试通过SWD/JTAG切换读取设备ID码
    swdArmWakeUp();
    swdResetLineSWDJ();
    swdWriteBits(JTAG_TO_SWD_CMD, 16);
    swdResetLineSWDJ();
    swdWriteBits(0x00, 4); // 空闲位
    swdWriteBits(0xA5, 8); // 读取IDCODE命令

    swdSetReadMode();
    swdClockPulse(); // 切换读写方向

    // 应答失败则返回false
    if (!swdReadAck()) return false;

    // 读取32位IDCODE
    uint32_t idcode = 0;
    for (int i = 0; i < 32; ++i) {
        idcode |= (swdReadBit() << i);
    }
    swdReadBit(); // 跳过奇偶校验位
    swdSetWriteMode();
    swdClockPulse();

    idcodeOut = idcode;
    return true;
}

bool JtagService::scanSwdDevice(const std::vector<uint8_t>& pins, uint8_t& swdio, uint8_t& swclk, uint32_t& idcodeOut) {
    // 扫描指定引脚列表中的SWD设备，自动识别SWDIO/SWCLK引脚
    for (auto clk : pins) {
        for (auto io : pins) {
            if (clk == io) continue;
            _pinSWDIO = io;
            _pinSWCLK = clk;

            // 初始化SWD引脚
            gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_OUTPUT);
            gpio_set_direction((gpio_num_t)_pinSWCLK, GPIO_MODE_OUTPUT);
            gpio_set_level((gpio_num_t)_pinSWDIO, 1);
            gpio_set_level((gpio_num_t)_pinSWCLK, 1);

            // 尝试读取SWD设备ID
            bool found = swdTrySWDJ(idcodeOut);
            if (found) {
                swdio = io;
                swclk = clk;
                swdToJTAG(); // 切回JTAG模式
                return true;
            }

            // 复位引脚状态
            gpio_set_direction((gpio_num_t)_pinSWDIO, GPIO_MODE_INPUT);
            gpio_set_direction((gpio_num_t)_pinSWCLK, GPIO_MODE_INPUT);
        }
    }
    return false;
}