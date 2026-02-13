#include "LedService.h"
#include <FastLED.h>
#include <Enums/LedProtocolEnum.h>
#include <Enums/LedChipsetEnum.h>

extern CFastLED FastLED; // 声明FastLED为全局变量

LedService::LedService() {}

void LedService::configure(uint8_t dataPin, uint8_t clockPin, uint16_t length, const std::string& protocol, uint8_t brightness) {
    // 如果已有LED数组，先清理并释放内存
    if (leds) {
        FastLED.clear(true);
        delete[] leds;
        leds = nullptr;
        delay(20);
    }
    
    FastLED = CFastLED();  // 完全重置FastLED
    ledCount = length;
    leds = new CRGB[ledCount];
    FastLED.clear();
    FastLED.clearData();

    /*
    FastLED需要在编译时确定引脚和协议，
    除了为每种协议编写switch分支外暂无其他方法
    该功能无法在Windows上编译，仅在Linux编译时定义：
    -D ENABLE_FASTLED_PROTOCOL_SWITCHES
    */

    // --- 仅需DATA引脚的单总线协议 ---

    #ifdef ENABLE_FASTLED_PROTOCOL_SWITCHES

    auto proto = LedProtocolEnumMapper::fromString(protocol);
    if (proto != LedProtocolEnum::UNKNOWN) {
        switch (proto) {
            case LedProtocolEnum::NEOPIXEL:
                FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(leds, ledCount); break;
            case LedProtocolEnum::WS2812:
                FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::WS2812B:
                FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::WS2811:
                FastLED.addLeds<WS2811, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::WS2811_400:
                FastLED.addLeds<WS2811_400, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::WS2813:
                FastLED.addLeds<WS2813, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::WS2815:
                FastLED.addLeds<WS2815, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::WS2816:
                FastLED.addLeds<WS2816, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::GS1903:
                FastLED.addLeds<GS1903, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::SK6812:
                FastLED.addLeds<SK6812, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::SK6822:
                FastLED.addLeds<SK6822, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::APA104:
                FastLED.addLeds<APA104, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::APA106:
                FastLED.addLeds<APA106, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::PL9823:
                FastLED.addLeds<PL9823, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::GE8822:
                FastLED.addLeds<GE8822, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::GW6205:
                FastLED.addLeds<GW6205, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::GW6205_400:
                FastLED.addLeds<GW6205_400, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::LPD1886:
                FastLED.addLeds<LPD1886, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::LPD1886_8BIT:
                FastLED.addLeds<LPD1886_8BIT, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::SM16703:
                FastLED.addLeds<SM16703, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::TM1829:
                FastLED.addLeds<TM1829, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::TM1812:
                FastLED.addLeds<TM1812, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::TM1809:
                FastLED.addLeds<TM1809, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::TM1804:
                FastLED.addLeds<TM1804, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::TM1803:
                FastLED.addLeds<TM1803, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::UCS1903:
                FastLED.addLeds<UCS1903, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::UCS1903B:
                FastLED.addLeds<UCS1903B, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::UCS1904:
                FastLED.addLeds<UCS1904, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::UCS2903:
                FastLED.addLeds<UCS2903, LED_DATA_PIN, GRB>(leds, ledCount); break;
            case LedProtocolEnum::UCS1912:
                FastLED.addLeds<UCS1912, LED_DATA_PIN, GRB>(leds, ledCount); break;
            default:
                delete[] leds;
                leds = nullptr;
                return;
        }
        usesClock = false; // 单总线协议无需时钟引脚
        FastLED.setBrightness(brightness);
        FastLED.show();
        return;
    }

    // --- 需要DATA + CLOCK引脚的SPI类芯片集 ---

    auto chipset = LedChipsetMapper::fromString(protocol);
    switch (chipset) {
        case LPD6803:
            FastLED.addLeds<LPD6803, LED_DATA_PIN, LED_CLOCK_PIN>(leds, ledCount); break;
        case LPD8806:
            FastLED.addLeds<LPD8806, LED_DATA_PIN, LED_CLOCK_PIN>(leds, ledCount); break;
        case WS2801:
            FastLED.addLeds<WS2801, LED_DATA_PIN, LED_CLOCK_PIN>(leds, ledCount); break;
        case WS2803:
            FastLED.addLeds<WS2803, LED_DATA_PIN, LED_CLOCK_PIN>(leds, ledCount); break;
        case SM16716:
            FastLED.addLeds<SM16716, LED_DATA_PIN, LED_CLOCK_PIN>(leds, ledCount); break;
        case P9813:
            FastLED.addLeds<P9813, LED_DATA_PIN, LED_CLOCK_PIN>(leds, ledCount); break;
        case APA102:
            FastLED.addLeds<APA102, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        case APA102HD:
            FastLED.addLeds<APA102, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        case DOTSTAR:
            FastLED.addLeds<DOTSTAR, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        case DOTSTARHD:
            FastLED.addLeds<DOTSTARHD, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        case SK9822:
            FastLED.addLeds<SK9822, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        case SK9822HD:
            FastLED.addLeds<SK9822HD, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        case HD107:
            FastLED.addLeds<HD107, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        case HD107HD:
            FastLED.addLeds<HD107HD, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, ledCount); break;
        default:
            delete[] leds;
            leds = nullptr;
            return;
    }
    #else
        Serial.println("\n\n当前编译版本禁用了FastLED协议配置功能。");
        Serial.println("你可能正在Windows系统上编译该项目。");
        Serial.println("由于Windows编译器限制，未包含FastLED 'addLeds<>()' 分支逻辑。");
        Serial.println("如需启用完整的LED支持，请在Linux系统编译并定义 ENABLE_FASTLED_PROTOCOL_SWITCHES。\n\n");
        FastLED.addLeds<WS2812, LED_DATA_PIN, GRB>(leds, ledCount);
    #endif

    usesClock = true; // SPI类协议需要时钟引脚
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void LedService::fill(const CRGB& color) {
    // 无LED数组则直接返回
    if (!leds) return;
    FastLED.clear(true);
    // 填充所有LED为指定颜色
    for (uint16_t i = 0; i < ledCount; ++i) {
        leds[i] = color;
    }
    FastLED.show();
}

void LedService::set(uint16_t index, const CRGB& color) {
    // 索引越界或无LED数组则直接返回
    if (!leds || index >= ledCount) return;

    // 先清除该LED当前颜色
    leds[index] = CRGB::Black;
    FastLED.show();

    // 设置新颜色并显示
    leds[index] = color;
    FastLED.show();
}

void LedService::resetLeds() {
    // 重置所有LED为熄灭状态
    if (!leds) return;
    fill(CRGB::Black);
    FastLED.clear(true);
    animationRunning = false;
}

void LedService::runAnimation(const std::string& type) {
    // 无LED数组则直接返回
    if (!leds) return;
    animationRunning = true;
    FastLED.clear();

    // 根据动画类型执行对应效果
    if (type == "blink") {
        // 闪烁动画：白色闪烁3次
        for (int i = 0; i < 3 && animationRunning; ++i) {
            fill(CRGB::White);
            delay(50);
            resetLeds();
            delay(50);
        }
    } else if (type == "rainbow") {
        // 彩虹动画：循环256色
        for (int j = 0; j < 256 && animationRunning; ++j) {
            for (uint16_t i = 0; i < ledCount; ++i) {
                leds[i] = CHSV((i * 10 + j) % 255, 255, 255);
            }
            FastLED.show();
            delay(1);
        }
    } else if (type == "chase") {
        // 追逐动画：蓝色LED逐个移动
        for (int i = 0; i < ledCount * 2 && animationRunning; ++i) {
            fill(CRGB::Black);
            leds[i % ledCount] = CRGB::Blue;
            FastLED.show();
            delay(100);
        }
    } else if (type == "cycle") {
        // 循环动画：红/绿/蓝依次显示
        CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue};
        for (int c = 0; c < 3 && animationRunning; ++c) {
            fill(colors[c]);
            delay(100);
        }
    } else if (type == "wave") {
        // 波浪动画：蓝色亮度渐变
        for (int t = 0; t < 256 && animationRunning; ++t) {
            for (uint16_t i = 0; i < ledCount; ++i) {
                uint8_t level = (sin8(i * 8 + t));
                leds[i] = CHSV(160, 255, level);
            }
            FastLED.show();
            delay(1);
        }
    }
    animationRunning = false;
}

bool LedService::isAnimationRunning() const {
    // 返回动画运行状态
    return animationRunning;
}

std::vector<std::string> LedService::getSingleWireProtocols() {
    // 获取所有单总线LED协议列表
    return LedProtocolEnumMapper::getAllProtocols();
}

std::vector<std::string> LedService::getSpiChipsets() {
    // 获取所有SPI类LED芯片集列表
    return LedChipsetMapper::getAllChipsets();
}

std::vector<std::string> LedService::getSupportedProtocols() {
    // 获取所有支持的LED协议/芯片集（合并单总线+SPI）
    std::vector<std::string> all = getSingleWireProtocols();
    std::vector<std::string> spi = getSpiChipsets();
    all.insert(all.end(), spi.begin(), spi.end());
    return all;
}

std::vector<std::string> LedService::getSupportedAnimations() {
    // 获取所有支持的动画效果名称
    return {
        "blink", "rainbow", "chase", "cycle", "wave"
    };
}

CRGB LedService::parseStringColor(const std::string& input) {
    // 字符串转小写工具函数
    auto toLower = [](const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    };

    std::string lowered = toLower(input);

    // 预定义命名颜色映射表
    static const std::map<std::string, CRGB> namedColors = {
        {"black", CRGB(0x00, 0x00, 0x00)},
        {"off", CRGB(0x00, 0x00, 0x00)},
        {"white", CRGB(0xFF, 0xFF, 0xFF)},
        {"on", CRGB(0xFF, 0xFF, 0xFF)},
        {"red", CRGB(0xFF, 0x00, 0x00)},
        {"green", CRGB(0x00, 0x80, 0x00)},
        {"blue", CRGB(0x00, 0x00, 0xFF)},
        {"yellow", CRGB(0xFF, 0xFF, 0x00)},
        {"cyan", CRGB(0x00, 0xFF, 0xFF)},
        {"magenta", CRGB(0xFF, 0x00, 0xFF)},
        {"purple", CRGB(0x80, 0x00, 0x80)},
        {"orange", CRGB(0xFF, 0xA5, 0x00)},
        {"pink", CRGB(0xFF, 0xC0, 0xCB)},
        {"brown", CRGB(0xA5, 0x2A, 0x2A)},
        {"gray", CRGB(0x80, 0x80, 0x80)},
        {"navy", CRGB(0x00, 0x00, 0x80)},
        {"teal", CRGB(0x00, 0x80, 0x80)},
        {"olive", CRGB(0x80, 0x80, 0x00)},
        {"lime", CRGB(0x00, 0xFF, 0x00)},
        {"aqua", CRGB(0x00, 0xFF, 0xFF)},
        {"maroon", CRGB(0x80, 0x00, 0x00)},
        {"silver", CRGB(0xC0, 0xC0, 0xC0)},
        {"gold", CRGB(0xFF, 0xD7, 0x00)},
        {"skyblue", CRGB(0x87, 0xCE, 0xEB)},
        {"violet", CRGB(0xEE, 0x82, 0xEE)},
        {"turquoise", CRGB(0x40, 0xE0, 0xD0)},
        {"coral", CRGB(0xFF, 0x7F, 0x50)},
        {"indigo", CRGB(0x4B, 0x00, 0x82)},
        {"salmon", CRGB(0xFA, 0x80, 0x72)},
        {"beige", CRGB(0xF5, 0xF5, 0xDC)},
        {"khaki", CRGB(0xF0, 0xE6, 0x8C)},
        {"plum", CRGB(0xDD, 0xA0, 0xDD)},
        {"orchid", CRGB(0xDA, 0x70, 0xD6)},
        {"tan", CRGB(0xD2, 0xB4, 0x8C)},
        {"chocolate", CRGB(0xD2, 0x69, 0x1E)},
        {"crimson", CRGB(0xDC, 0x14, 0x3C)},
        {"tomato", CRGB(0xFF, 0x63, 0x47)},
        {"darkpink", CRGB(0xFF, 0x14, 0x93)},
        {"darkblue", CRGB(0x00, 0xBF, 0xFF)},
    };

    // 查找匹配的命名颜色
    auto it = namedColors.find(lowered);
    if (it != namedColors.end())
        return it->second;

    return CRGB::White; // 无匹配则默认返回白色
}

CRGB LedService::parseHtmlColor(const std::string& input) {
    // 字符串转小写工具函数
    auto toLower = [](const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    };

    std::string lowered = toLower(input);

    // 解析HTML颜色格式（#RRGGBB 或 0xRRGGBB）
    std::string hex = lowered;
    if (hex.rfind("#", 0) == 0)
        hex = "0x" + hex.substr(1);  // 将#开头转换为0x开头

    // 验证格式并解析16进制颜色值
    if (hex.rfind("0x", 0) == 0 && hex.length() == 8) {
        try {
            uint32_t value = std::stoul(hex, nullptr, 16);
            uint8_t r = (value >> 16) & 0xFF; // 提取红色分量
            uint8_t g = (value >> 8) & 0xFF;  // 提取绿色分量
            uint8_t b = value & 0xFF;         // 提取蓝色分量
            return CRGB(r, g, b);
        } catch (...) {
            return CRGB::White; // 解析失败返回白色
        }
    }

    return CRGB::White; // 格式不匹配返回白色
}