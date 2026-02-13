#include "SystemService.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <nvs.h>

namespace {
    /**
     * @brief 向字符串追加一行（带换行符）
     * @param s 目标字符串
     * @param line 要追加的行内容
     */
    inline void appendLine(std::string& s, const std::string& line) {
        s += line;
        s += "\r\n";
    }

    /**
     * @brief 字符串右填充空格（用于格式化输出）
     * @param s 源字符串
     * @param w 目标宽度
     * @return 填充后的字符串
     */
    inline std::string padRight(const char* s, size_t w) {
        std::string r = s ? s : "";
        if (r.size() < w) r.append(w - r.size(), ' ');
        return r;
    }

    /**
     * @brief NVS数据类型转字符串（内部工具函数）
     * @param t NVS类型枚举值
     * @return 类型缩写字符串（如U8/STR/BLOB）
     */
    auto nvsTypeToStr = [](uint8_t t) -> const char* {
        switch (t) {
            case NVS_TYPE_U8:   return "U8";
            case NVS_TYPE_I8:   return "I8";
            case NVS_TYPE_U16:  return "U16";
            case NVS_TYPE_I16:  return "I16";
            case NVS_TYPE_U32:  return "U32";
            case NVS_TYPE_I32:  return "I32";
            case NVS_TYPE_U64:  return "U64";
            case NVS_TYPE_I64:  return "I64";
            case NVS_TYPE_STR:  return "STR";
            case NVS_TYPE_BLOB: return "BLOB";
            default:            return "?";
        }
    };
}

// -----------------------------
// 芯片 / 运行时信息
// -----------------------------

/**
 * @brief 获取芯片型号（如ESP32/ESP32-S3）
 * @return 芯片型号字符串
 */
std::string SystemService::getChipModel() const
{
    return std::string(ESP.getChipModel());
}

/**
 * @brief 获取系统运行时长（秒）
 * @return 从启动到现在的秒数
 */
uint32_t SystemService::getUptimeSeconds() const
{
    return millis() / 1000u;
}

/**
 * @brief 获取系统重置原因
 * @return 重置原因枚举值（对应esp_reset_reason_t）
 */
int SystemService::getResetReason() const
{
    return static_cast<int>(esp_reset_reason());
}

/**
 * @brief 获取CPU当前频率（MHz）
 * @return CPU频率值
 */
int SystemService::getCpuFreqMhz() const
{
    return getCpuFrequencyMhz();
}

// -----------------------------
// 芯片详细信息
// -----------------------------

/**
 * @brief 获取芯片核心数
 * @return 核心数量（如2表示双核）
 */
int SystemService::getChipCores() const
{
    esp_chip_info_t ci{};
    esp_chip_info(&ci);
    return ci.cores;
}

/**
 * @brief 获取芯片修订版本号
 * @return 芯片修订版本
 */
int SystemService::getChipRevision() const
{
    esp_chip_info_t ci{};
    esp_chip_info(&ci);
    return ci.revision;
}

/**
 * @brief 获取芯片完整修订版本号
 * @return 完整修订版本值
 */
int SystemService::getChipFullRevision() const
{
    esp_chip_info_t ci{};
    esp_chip_info(&ci);
    return ci.full_revision;
}

/**
 * @brief 获取芯片特性原始值（位图）
 * @return 芯片特性位图（对应ESP_CHIP_FEATURE_*宏）
 */
uint32_t SystemService::getChipFeaturesRaw() const
{
    esp_chip_info_t ci{};
    esp_chip_info(&ci);
    return ci.features;
}

// -----------------------------
// 版本信息
// -----------------------------

/**
 * @brief 获取ESP-IDF版本号
 * @return IDF版本字符串（如v5.1.2）
 */
std::string SystemService::getIdfVersion() const
{
    return std::string(esp_get_idf_version());
}

/**
 * @brief 获取Arduino核心版本/板型信息
 * @return Arduino核心或板型字符串
 */
std::string SystemService::getArduinoCore() const
{
#ifdef ARDUINO_BOARD
    return std::string(ARDUINO_BOARD);
#else
    return "Arduino 默认核心";
#endif
}

// -----------------------------
// 栈 / 堆 / PSRAM（片外RAM）
// -----------------------------

/**
 * @brief 获取当前任务栈已使用大小（字节）
 * @return 已使用栈大小，失败返回-1
 */
size_t SystemService::getStackUsed() const {
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    size_t usedNow = 0;

    // 获取所有任务快照
    TaskSnapshot_t snaps[16];
    UBaseType_t count = 0;
    UBaseType_t got = uxTaskGetSnapshotAll(snaps, 16, &count);
    if (got == 0) {
        return -1;
    }

    // 查找当前任务的快照
    const TaskSnapshot_t* self = nullptr;
    for (UBaseType_t i = 0; i < got; ++i) {
        if (snaps[i].pxTCB == h) {
            self = &snaps[i];
            break;
        }
    }
    if (!self) {
        return -1;
    }

    // 计算已使用栈大小（栈底 - 栈顶）
    uintptr_t top = reinterpret_cast<uintptr_t>(self->pxTopOfStack);
    uintptr_t end = reinterpret_cast<uintptr_t>(self->pxEndOfStack);
    if (top == 0 || end == 0 || end <= top) {
        return -1;
    }

    usedNow = end - top;
    return usedNow;
}

/**
 * @brief 获取当前任务栈总大小（字节）
 * @return 栈总大小（由CONFIG_ARDUINO_LOOP_STACK_SIZE定义）
 */
size_t SystemService::getStackTotal() const {
    return CONFIG_ARDUINO_LOOP_STACK_SIZE;
}

/**
 * @brief 获取堆总大小（字节）
 * @return 堆总容量
 */
size_t SystemService::getHeapTotal() const
{
    return ESP.getHeapSize();
}

/**
 * @brief 获取当前空闲堆大小（字节）
 * @return 空闲堆容量
 */
size_t SystemService::getHeapFree() const
{
    return ESP.getFreeHeap();
}

/**
 * @brief 获取运行期间最小空闲堆大小（字节）
 * @return 最小空闲堆容量（反映内存峰值使用情况）
 */
size_t SystemService::getHeapMinFree() const
{
    return ESP.getMinFreeHeap();
}

/**
 * @brief 获取堆中可分配的最大连续内存块（字节）
 * @return 最大可分配堆块大小
 */
size_t SystemService::getHeapMaxAlloc() const
{
    return ESP.getMaxAllocHeap();
}

/**
 * @brief 获取PSRAM总大小（字节）
 * @return PSRAM总容量（无PSRAM返回0）
 */
size_t SystemService::getPsramTotal() const
{
    return ESP.getPsramSize();
}

/**
 * @brief 获取当前空闲PSRAM大小（字节）
 * @return 空闲PSRAM容量
 */
size_t SystemService::getPsramFree() const
{
    return ESP.getFreePsram();
}

/**
 * @brief 获取运行期间最小空闲PSRAM大小（字节）
 * @return 最小空闲PSRAM容量
 */
size_t SystemService::getPsramMinFree() const
{
    return ESP.getMinFreePsram();
}

/**
 * @brief 获取PSRAM中可分配的最大连续内存块（字节）
 * @return 最大可分配PSRAM块大小
 */
size_t SystemService::getPsramMaxAlloc() const
{
    return ESP.getMaxAllocPsram();
}

// -----------------------------
// 闪存 / 固件（Sketch）
// -----------------------------

/**
 * @brief 获取闪存总大小（字节）
 * @return 闪存容量
 */
size_t SystemService::getFlashSizeBytes() const
{
    return ESP.getFlashChipSize();
}

/**
 * @brief 获取闪存工作频率（Hz）
 * @return 闪存频率值
 */
uint32_t SystemService::getFlashSpeedHz() const
{
    return ESP.getFlashChipSpeed();
}

/**
 * @brief 获取闪存工作模式原始值
 * @return 闪存模式值（对应FLASH_MODE_*宏）
 */
int SystemService::getFlashModeRaw() const
{
    return ESP.getFlashChipMode();
}

/**
 * @brief 获取闪存JEDEC ID（十六进制字符串）
 * @return JEDEC ID字符串，读取失败返回"读取失败"
 */
std::string SystemService::getFlashJedecIdHex() const
{
    uint32_t jedec = 0;
    if (esp_flash_read_id(nullptr, &jedec) != ESP_OK) {
        return "读取失败";
    }

    char buf[12];
    std::snprintf(buf, sizeof(buf), "0x%06X",
                  static_cast<unsigned>(jedec & 0xFFFFFFu));
    return std::string(buf);
}

/**
 * @brief 获取固件（Sketch）已使用大小（字节）
 * @return 固件占用闪存大小
 */
size_t SystemService::getSketchUsedBytes() const
{
    return ESP.getSketchSize();
}

/**
 * @brief 获取固件分区剩余空间（字节）
 * @return 固件分区空闲大小
 */
size_t SystemService::getSketchFreeBytes() const
{
    return ESP.getFreeSketchSpace();
}

/**
 * @brief 获取固件MD5校验值
 * @return MD5字符串
 */
std::string SystemService::getSketchMD5() const
{
    return std::string(ESP.getSketchMD5().c_str());
}

// -----------------------------
// 网络（原始值）
// -----------------------------

/**
 * @brief 获取设备基础MAC地址（EFUSE中默认MAC）
 * @return MAC地址字符串（格式：XX:XX:XX:XX:XX:XX）
 */
std::string SystemService::getBaseMac() const
{
    uint8_t m[6]{};
    esp_efuse_mac_get_default(m);

    char macStr[18];
    std::snprintf(macStr, sizeof(macStr),
                  "%02X:%02X:%02X:%02X:%02X:%02X",
                  m[0], m[1], m[2], m[3], m[4], m[5]);

    return std::string(macStr);
}

// -----------------------------
// 文件系统（LittleFS / SPIFFS）
// -----------------------------

/**
 * @brief 初始化LittleFS文件系统
 * @param autoFormat 自动格式化标记（true=挂载失败时自动格式化）
 * @return 初始化成功返回true，失败返回false
 */
bool SystemService::littlefsBegin(bool autoFormat) const
{
    return LittleFS.begin(autoFormat);
}

/**
 * @brief 关闭LittleFS文件系统
 */
void SystemService::littlefsEnd() const
{
    LittleFS.end();
}

/**
 * @brief 获取LittleFS总容量（字节）
 * @return 文件系统总大小
 */
size_t SystemService::littlefsTotalBytes() const
{
    return LittleFS.totalBytes();
}

/**
 * @brief 获取LittleFS已使用容量（字节）
 * @return 文件系统已用大小
 */
size_t SystemService::littlefsUsedBytes() const
{
    return LittleFS.usedBytes();
}

// -----------------------------
// 分区信息（格式化输出）
// -----------------------------

/**
 * @brief 获取分区表信息（格式化文本，用户可见）
 * @return 分区信息字符串（包含运行中/启动/下一个OTA分区+全部分区列表）
 */
std::string SystemService::getPartitions() const
{
    std::string out;
    const esp_partition_t* run  = esp_ota_get_running_partition();   // 运行中分区
    const esp_partition_t* boot = esp_ota_get_boot_partition();      // 启动分区
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr); // 下一个OTA分区

    /**
     * @brief 格式化单个分区信息（内部工具函数）
     * @param p 分区指针
     * @return 分区信息字符串
     */
    auto partLine = [](const esp_partition_t* p) -> std::string {
        if (!p) {
            return "(无)";
        }

        // 分区类型转字符串
        const char* type = (p->type == ESP_PARTITION_TYPE_APP)  ? "应用" :
                           (p->type == ESP_PARTITION_TYPE_DATA) ? "数据" :
                                                                  "未知";

        char b[128];
        std::snprintf(b, sizeof(b), "%-4s %-8s  @0x%06X  %u字节",
                      type, p->label, static_cast<unsigned>(p->address), static_cast<unsigned>(p->size));
        return std::string(b);
    };

    // 输出关键分区信息
    appendLine(out, std::string("运行中  : ") + partLine(run));
    appendLine(out, std::string("启动分区: ") + partLine(boot));
    appendLine(out, std::string("下一个OTA: ") + partLine(next));

    appendLine(out, "");
    appendLine(out, "类型  标签      地址      大小(字节)");

    // 遍历全部分区
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY,
                                                     ESP_PARTITION_SUBTYPE_ANY,
                                                     nullptr);
    if (!it) {
        appendLine(out, "(无分区信息)");
        return out;
    }

    for (auto iter = it; iter; iter = esp_partition_next(iter)) {
        const esp_partition_t* p = esp_partition_get(iter);
        if (!p) {
            continue;
        }

        // 分区类型转字符串
        const char* type = (p->type == ESP_PARTITION_TYPE_APP)  ? "应用" :
                           (p->type == ESP_PARTITION_TYPE_DATA) ? "数据" :
                                                                  "未知";

        char line[128];
        std::snprintf(line, sizeof(line), "%-4s %-8s 0x%06X  %u",
                      type, p->label, static_cast<unsigned>(p->address), static_cast<unsigned>(p->size));
        appendLine(out, line);
    }

    #ifdef esp_partition_iterator_release
        esp_partition_iterator_release(it); // 释放迭代器
    #endif

    return out;
}

// -----------------------------
// NVS（非易失性存储）格式化输出
// -----------------------------

/**
 * @brief 获取NVS全局统计信息（格式化文本）
 * @return NVS统计字符串（已用/空闲条目数、命名空间数量等）
 */
std::string SystemService::getNvsStats() const {
    std::string out;

    nvs_stats_t st{};
    if (nvs_get_stats(nullptr, &st) == ESP_OK) {
        appendLine(out, "已用条目数    : " + std::to_string(st.used_entries));
        appendLine(out, "空闲条目数    : " + std::to_string(st.free_entries));
        appendLine(out, "总条目数      : " + std::to_string(st.total_entries));
        appendLine(out, "命名空间数量  : " + std::to_string(st.namespace_count));
    } else {
        appendLine(out, "当前编译版本不支持获取NVS统计信息。");
    }
    return out;
}

/**
 * @brief 获取NVS所有条目列表（格式化文本）
 * @return NVS条目字符串（命名空间、键、类型）
 */
std::string SystemService::getNvsEntries() const {
    std::string out;

    // 查找NVS所有条目
    nvs_iterator_t it = nvs_entry_find("nvs", nullptr, NVS_TYPE_ANY);
    if (!it) {
        appendLine(out, "(无NVS条目)");
        return out;
    }

    // 固定宽度（适配等宽字体）
    constexpr size_t W_NS  = 16;  // 命名空间宽度
    constexpr size_t W_KEY = 20;  // 键名宽度

    // 输出表头
    appendLine(out, padRight("命名空间", W_NS) + " " + padRight("键名", W_KEY) + " 类型");

    // 遍历所有NVS条目
    for (nvs_iterator_t iter = it; iter; iter = nvs_entry_next(iter)) {
        nvs_entry_info_t info{};
        nvs_entry_info(iter, &info);

        // 格式化单行条目
        std::string line = padRight(info.namespace_name, W_NS)
                         + " "
                         + padRight(info.key, W_KEY)
                         + " "
                         + nvsTypeToStr(info.type);
        appendLine(out, line);
    }

    #if defined(nvs_release_iterator)
        nvs_release_iterator(it); // 释放迭代器
    #endif

    return out;
}

/**
 * @brief 重启设备
 * @param hard 硬重启标记（true=Arduino风格重启，false=IDF原生重启）
 */
void SystemService::reboot(bool hard) const {
    if (hard) {
        ESP.restart();    // Arduino风格重启
    } else {
        esp_restart();    // IDF原生重启
    }
}