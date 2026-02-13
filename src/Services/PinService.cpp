#include "PinService.h"

/**
 * @brief 将引脚配置为普通输入模式（无上/下拉）
 * @param pin ESP32的GPIO引脚号
 * @note 同时记录该引脚的上下拉状态为NOPULL
 */
void PinService::setInput(uint8_t pin) {
    pinMode(pin, INPUT);          // 配置引脚为普通输入
    pullState[pin] = NOPULL;      // 记录上下拉状态：无拉取
}

/**
 * @brief 将引脚配置为上拉输入模式
 * @param pin ESP32的GPIO引脚号
 * @note 上拉模式下，引脚无外部输入时默认高电平
 */
void PinService::setInputPullup(uint8_t pin) {
    pinMode(pin, INPUT_PULLUP);   // 配置引脚为上拉输入
    pullState[pin] = PULL_UP;     // 记录上下拉状态：上拉
}

/**
 * @brief 将引脚配置为下拉输入模式
 * @param pin ESP32的GPIO引脚号
 * @note 下拉模式下，引脚无外部输入时默认低电平
 */
void PinService::setInputPullDown(uint8_t pin) {
    pinMode(pin, INPUT_PULLDOWN); // 配置引脚为下拉输入
    pullState[pin] = PULL_DOWN;   // 记录上下拉状态：下拉
}

/**
 * @brief 将引脚配置为输出模式
 * @param pin ESP32的GPIO引脚号
 */
void PinService::setOutput(uint8_t pin) {
    pinMode(pin, OUTPUT);         // 配置引脚为输出模式
}

/**
 * @brief 将引脚设置为高电平（强制配置为输出模式）
 * @param pin ESP32的GPIO引脚号
 */
void PinService::setHigh(uint8_t pin) {
    setOutput(pin);  // 先强制配置为输出模式，避免引脚模式错误
    digitalWrite(pin, HIGH);      // 输出高电平
}

/**
 * @brief 将引脚设置为低电平（强制配置为输出模式）
 * @param pin ESP32的GPIO引脚号
 */
void PinService::setLow(uint8_t pin) {
    setOutput(pin);  // 先强制配置为输出模式，避免引脚模式错误
    digitalWrite(pin, LOW);       // 输出低电平
}

/**
 * @brief 读取引脚的数字电平状态
 * @param pin ESP32的GPIO引脚号
 * @return 高电平返回true，低电平返回false
 * @note 直接调用ESP32底层gpio_get_level函数，效率更高
 */
bool PinService::read(uint8_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

/**
 * @brief 切换引脚的上拉状态（上拉↔普通输入）
 * @param pin ESP32的GPIO引脚号
 */
void PinService::togglePullup(uint8_t pin) {
    pullType enabled = pullState[pin];  // 获取当前引脚的上下拉状态

    if (enabled == PULL_UP) {
        setInput(pin);                  // 当前是上拉→切换为普通输入
    } else {
        setInputPullup(pin);            // 当前非上拉→切换为上拉输入
    }
}

/**
 * @brief 切换引脚的下拉状态（下拉↔普通输入）
 * @param pin ESP32的GPIO引脚号
 */
void PinService::togglePullDown(uint8_t pin) {
    pullType enabled = pullState[pin];  // 获取当前引脚的上下拉状态

    if (enabled == PULL_DOWN) {
        setInput(pin);                  // 当前是下拉→切换为普通输入
    } else {
        setInputPullDown(pin);          // 当前非下拉→切换为下拉输入
    }
}

/**
 * @brief 读取引脚的模拟量值（ADC）
 * @param pin ESP32的ADC可用引脚号
 * @return 模拟量值（0~4095，ESP32默认12位ADC）
 * @note 先配置为普通输入模式，确保ADC读取正常
 */
int PinService::readAnalog(uint8_t pin) {
    pinMode(pin, INPUT);  // 配置为普通输入模式（ADC读取要求）
    return analogRead(pin);            // 读取模拟量值
}

/**
 * @brief 配置引脚的PWM输出（自动适配最大可行分辨率）
 * @param pin ESP32的GPIO引脚号
 * @param freq PWM频率（Hz）
 * @param dutyPercent PWM占空比（0~100%）
 * @return 配置成功返回true，失败返回false
 * @note 基于ESP32的LEDC外设实现PWM，通道号=引脚号%8
 */
bool PinService::setupPwm(uint8_t pin, uint32_t freq, uint8_t dutyPercent) {
    if (dutyPercent > 100) dutyPercent = 100;  // 限制占空比最大值为100%

    int channel = pin % 8;  // 分配LEDC通道（共8个通道，按引脚号取模）

    // 从14位到1位遍历，找到最大的兼容分辨率（分辨率越高，PWM精度越高）
    int resolution = -1;
    for (int bits = 14; bits >= 1; --bits) {
        // 检查该频率和分辨率是否可行，且LEDC初始化成功
        if (isPwmFeasible(freq, bits)) {
            if (ledcSetup(channel, freq, bits)) {  // 初始化LEDC通道
                resolution = bits;
                break;
            }
        }
    }
    if (resolution < 0) return false;  // 无可用分辨率，配置失败

    ledcAttachPin(pin, channel);  // 将引脚绑定到LEDC通道

    // 计算占空比对应的数值：dutyMax = 2^resolution - 1
    uint32_t dutyMax = (1UL << resolution) - 1;
    // 占空比转换：百分比→实际数值（避免浮点运算）
    uint32_t dutyVal = ((uint32_t)dutyPercent * dutyMax) / 100U;
    ledcWrite(channel, dutyVal);  // 设置PWM占空比

    return true;
}

/**
 * @brief 控制舵机角度（基于PWM实现）
 * @param pin ESP32的GPIO引脚号
 * @param angle 舵机角度（0~180度）
 * @note 舵机控制标准：50Hz PWM（20ms周期），1ms脉冲=0度，2ms脉冲=180度
 */
void PinService::setServoAngle(uint8_t pin, uint8_t angle) {
  const int channel = 0;               // 固定使用LEDC通道0
  const int freq = 50;                // 舵机标准PWM频率：50Hz
  const int resolution = 14;          // 14位分辨率（ESP32最大稳定分辨率）

  // 初始化LEDC通道并绑定引脚
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(pin, channel);

  // 计算PWM周期和最大占空比值
  const uint32_t periodUs = 1000000UL / freq;    // 周期（微秒）：50Hz→20000μs
  const uint32_t dutyMax  = (1UL << resolution) - 1;  // 最大占空比值

  // 将角度映射为脉冲宽度：0°→1000μs，180°→2000μs
  uint32_t pulseUs = map(angle, 0, 180, 1000, 2000);
  // 脉冲宽度转换为占空比数值：dutyVal = (脉冲宽度 / 周期) * dutyMax
  uint32_t dutyVal = (pulseUs * dutyMax) / periodUs;

  ledcWrite(channel, dutyVal);  // 输出舵机控制PWM
}

/**
 * @brief 检查指定频率和分辨率的PWM是否可行
 * @param freq 目标PWM频率（Hz）
 * @param resolutionBits PWM分辨率（1~14位）
 * @return 可行返回true，不可行返回false
 * @note 基于ESP32 80MHz APB时钟计算分频参数，判断是否在合法范围
 */
bool PinService::isPwmFeasible(uint32_t freq, uint8_t resolutionBits) {
    // 分辨率范围校验（ESP32 LEDC支持1~14位）
    if (resolutionBits < 1 || resolutionBits > 14) return false;

    const uint32_t baseClkHz     = 80000000UL;   // ESP32 LEDC基础时钟：80MHz
    const uint32_t maxDivParam   = 0x3FFFF;      // 分频参数最大值（LEDC硬件限制）
    if (freq == 0) return false;                 // 频率不能为0

    // 分频参数计算公式：div_param = 基础时钟 / (频率 * 2^分辨率)
    uint64_t denom = (uint64_t)freq * (1ULL << resolutionBits);
    if (denom == 0) return false;                // 分母不能为0

    uint32_t divParam = (uint32_t)(baseClkHz / denom);
    // 分频参数需在1~maxDivParam之间才合法
    return (divParam >= 1 && divParam <= maxDivParam);
}

/**
 * @brief 获取指定引脚的上下拉状态
 * @param pin ESP32的GPIO引脚号
 * @return 上下拉状态（NOPULL/PULL_UP/PULL_DOWN）
 */
PinService::pullType PinService::getPullType(uint8_t pin){
    return(pullState[pin]);
}

/**
 * @brief 获取所有配置过上下拉状态的引脚列表
 * @return 配置过上下拉的引脚号向量
 */
std::vector<uint8_t> PinService::getConfiguredPullPins() {
    std::vector<uint8_t> pins;
    pins.reserve(pullState.size());  // 预分配内存，提升效率

    // 遍历上下拉状态映射表，收集所有引脚号
    for (const auto& entry : pullState) {
        pins.push_back(entry.first);
    }

    return pins;
}