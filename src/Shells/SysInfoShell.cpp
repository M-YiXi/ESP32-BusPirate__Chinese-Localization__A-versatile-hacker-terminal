#include "SysInfoShell.h"
#include <cstdarg>
#include <cstdio>

SysInfoShell::SysInfoShell(ITerminalView& tv,
                           IInput& in,
                           UserInputManager& uim,
                           ArgTransformer& at,
                           SystemService& sys,
                           WifiService& wifi)
    : terminalView(tv)
    , terminalInput(in)
    , userInputManager(uim)
    , argTransformer(at)
    , systemService(sys)
    , wifiService(wifi) {}

void SysInfoShell::run() {
    bool loop = true;
    while (loop) {
        terminalView.println("\n=== 系统命令行 ===");

        int choice = userInputManager.readValidatedChoiceIndex("选择操作", actions, actions.size() - 1);

        switch (choice) {
            case 0: cmdSummary(); break;
            case 1: cmdHardwareInfo(); break;
            case 2: cmdMemory(); break;
            case 3: cmdPartitions(); break;
            case 4: cmdFS(); break;
            case 5: cmdNVS(false); break;
            case 6: cmdNVS(true); break;
            case 7: cmdNet(); break;
            case 8: cmdReboot(); break;
            case 9: // 退出
            default:
                loop = false;
                break;
        }
    }

    terminalView.println("正在退出系统命令行...\n");
}

void SysInfoShell::cmdSummary() {
    terminalView.println("\n=== 系统摘要 ===");
    terminalView.println("型号         : " + systemService.getChipModel());
    terminalView.println("运行时间     : " + std::to_string(systemService.getUptimeSeconds()) + " s");
    
    const int rr = systemService.getResetReason();
    terminalView.println(std::string("复位原因     : ") + resetReasonToStr(rr) + " (" + std::to_string(rr) + ")");
    
    terminalView.println("堆栈总计     : " + std::to_string(systemService.getStackTotal()   / 1024) + " KB");
    terminalView.println("堆总计       : " + std::to_string(systemService.getHeapTotal()   / 1024) + " KB");
    terminalView.println("PSRAM 总计   : " + std::to_string(systemService.getPsramTotal()  / 1024) + " KB");
    terminalView.println("Flash 总计   : " + std::to_string(systemService.getFlashSizeBytes() / 1024) + " KB");

    terminalView.println("固件         : " + std::string("版本 " + state.getVersion()));
    terminalView.println("构建日期     : " + std::string(__DATE__) + " " + std::string(__TIME__));
    terminalView.println("IDF 版本     : " + systemService.getIdfVersion());
    terminalView.println("Arduino 核心 : " + systemService.getArduinoCore());
}

void SysInfoShell::cmdHardwareInfo() {
    terminalView.println("\n=== 硬件信息 ===");

    // 芯片
    terminalView.println("型号             : " + systemService.getChipModel());
    terminalView.println("CPU 核心数       : " + std::to_string(systemService.getChipCores()));
    terminalView.println("CPU 频率         : " + std::to_string(systemService.getCpuFreqMhz()) + " MHz");

    // 特性 (WiFi/BT/BLE)
    const uint32_t f = systemService.getChipFeaturesRaw();
    std::string features;
    if (f & CHIP_FEATURE_WIFI_BGN) features += "WiFi ";
    if (f & CHIP_FEATURE_BT)       features += "BT ";
    if (f & CHIP_FEATURE_BLE)      features += "BLE ";
    if (features.empty())          features = "?";
    terminalView.println("特性             : " + features);

    terminalView.println("修订版本         : " + std::to_string(systemService.getChipRevision()));
    const int fullrev = systemService.getChipFullRevision();
    if (fullrev >= 0) {
        terminalView.println("完整修订版本     : " + std::to_string(fullrev));
    }

    // Flash
    terminalView.println("Flash 总计       : " + std::to_string(systemService.getFlashSizeBytes() / 1024) + " KB");
    terminalView.println("Flash 速度       : " + std::to_string(systemService.getFlashSpeedHz() / 1000000U) + " MHz");
    terminalView.println("Flash 模式       : " + std::string(flashModeToStr(systemService.getFlashModeRaw())));
    terminalView.println("Flash 芯片 ID    : " + systemService.getFlashJedecIdHex());

    // 程序
    const size_t sku = systemService.getSketchUsedBytes();
    const size_t skt = systemService.getSketchFreeBytes(); // 返回总空间
    const size_t skl = skt - sku; // 剩余空间
    const int    pctInt = skt ? static_cast<int>((sku * 100.0f / skt) + 0.5f) : 0;

    terminalView.println("程序总大小       : " + std::to_string(skt / 1024) + " KB");
    terminalView.println("程序剩余空间     : " + std::to_string(skl / 1024) + " KB");
    terminalView.println("程序使用率       : " + std::to_string(pctInt) + " %");
    terminalView.println("程序 MD5        : " + systemService.getSketchMD5());
}

void SysInfoShell::cmdMemory() {
    terminalView.println("\n=== 内存 ===");
    
    // 堆栈
    char line[96];
    size_t stTot  = systemService.getStackTotal();
    float totKB  = stTot  / 1024.0f;
    snprintf(line, sizeof(line), "堆栈总计         : %.2f KB", totKB);
    terminalView.println(line);

    #ifndef DEVICE_M5STICK // M5StickC 无足够 IRAM 处理此部分

    size_t stUsed = systemService.getStackUsed();
    float usedKB = stUsed / 1024.0f;
    float freeKB = (stTot > stUsed ? (stTot - stUsed) : 0) / 1024.0f;
    float pct    = (stUsed * 100.0f) / (float)stTot;
    int   stPct  = (stUsed * 100.0 / stTot) + 0.5;
    snprintf(line, sizeof(line), "堆栈剩余         : %.2f KB", freeKB);
    terminalView.println(line);
    snprintf(line, sizeof(line), "堆栈已用         : %.2f KB (%.0f%%)", usedKB, pct);
    terminalView.println(line);

    #endif

    // 堆
    const size_t heapTotal = systemService.getHeapTotal();
    const size_t heapFree  = systemService.getHeapFree();
    const int    heapPct   = heapTotal ? static_cast<int>(((heapTotal - heapFree) * 100.0f / heapTotal) + 0.5f) : 0;
    terminalView.println("堆总计           : " + std::to_string(heapTotal / 1024) + " KB");
    terminalView.println("堆剩余           : " + std::to_string(heapFree / 1024) + " KB");
    terminalView.println("堆已用           : " + std::to_string((heapTotal - heapFree) / 1024) + " KB (" + std::to_string(heapPct) + "%)");
    terminalView.println("最小剩余堆       : " + std::to_string(systemService.getHeapMinFree()  / 1024) + " KB");
    terminalView.println("最大分配堆       : " + std::to_string(systemService.getHeapMaxAlloc() / 1024) + " KB");

    // PSRAM
    const size_t psramTotal = systemService.getPsramTotal();
    const size_t psramFree  = systemService.getPsramFree();
    const int    psramPct   = psramTotal ? static_cast<int>(((psramTotal - psramFree) * 100.0f / psramTotal) + 0.5f) : 0;
    terminalView.println("PSRAM 总计       : " + std::to_string(psramTotal / 1024) + " KB");
    terminalView.println("PSRAM 剩余       : " + std::to_string(psramFree / 1024) + " KB");
    terminalView.println("PSRAM 已用       : " + std::to_string((psramTotal - psramFree) / 1024) + " KB (" + std::to_string(psramPct) + "%)");
    terminalView.println("最小剩余 PSRAM   : " + std::to_string(systemService.getPsramMinFree()  / 1024) + " KB");
    terminalView.println("最大分配 PSRAM   : " + std::to_string(systemService.getPsramMaxAlloc() / 1024) + " KB");
}

void SysInfoShell::cmdPartitions() {
    terminalView.println("\n=== 分区表 ===");
    terminalView.println(systemService.getPartitions());
}

void SysInfoShell::cmdFS() {
    terminalView.println("\n=== LittleFS ===");
    if (systemService.littlefsBegin(true)) { // 若挂载失败则自动格式化
        const size_t total = systemService.littlefsTotalBytes();
        const size_t used  = systemService.littlefsUsedBytes();
        const size_t freeB = (total >= used) ? (total - used) : 0;

        terminalView.println("总计  : " + std::to_string(total / 1024) + " KB");
        terminalView.println("已用  : " + std::to_string(used  / 1024) + " KB");
        terminalView.println("剩余  : " + std::to_string(freeB / 1024) + " KB");

        // systemService.littlefsEnd();
    } else {
        terminalView.println("LittleFS 未挂载.");
    }
}

void SysInfoShell::cmdNVS(bool listEntries) {
    terminalView.println("\n=== NVS ===");
    if (listEntries) {
        terminalView.println(systemService.getNvsEntries());
        return;
    }
    
    terminalView.println(systemService.getNvsStats());
}

void SysInfoShell::cmdNet() {
    terminalView.println("\n=== 网络信息 ===");

    auto ssid     = wifiService.getSsid();     if (ssid.empty()) ssid = "N/A";
    auto bssid    = wifiService.getBssid();    if (bssid.empty()) bssid = "N/A";
    auto hostname = wifiService.getHostname(); if (hostname.empty()) hostname = "N/A";

    terminalView.println("基本 MAC     : " + systemService.getBaseMac());
    terminalView.println("AP MAC       : " + wifiService.getMacAddressAp());
    terminalView.println("STA MAC      : " + wifiService.getMacAddressSta());
    terminalView.println("IP           : " + wifiService.getLocalIp());
    terminalView.println("子网掩码     : " + wifiService.getSubnetMask());
    terminalView.println("网关         : " + wifiService.getGatewayIp());
    terminalView.println("DNS1         : " + wifiService.getDns1());
    terminalView.println("DNS2         : " + wifiService.getDns2());
    terminalView.println("主机名       : " + hostname);

    terminalView.println("SSID         : " + ssid);
    terminalView.println("BSSID        : " + bssid);

    const int status = wifiService.getWifiStatusRaw();
    if (status == 3 /* WL_CONNECTED */) {
        terminalView.println("RSSI         : " + std::to_string(wifiService.getRssi()) + " dBm");
        terminalView.println("信道         : " + std::to_string(wifiService.getChannel()));
    } else {
        terminalView.println("RSSI         : N/A");
        terminalView.println("信道         : N/A");
    }

    terminalView.println("模式         : " + std::string(wifiService.wifiModeToStr(wifiService.getWifiModeRaw())));
    terminalView.println("状态         : " + std::string(wifiService.wlStatusToStr(status)));
    terminalView.println("配网启用     : " + std::string(wifiService.isProvisioningEnabled() ? "是" : "否"));
}

void SysInfoShell::cmdReboot(bool hard) {
    auto confirmation = userInputManager.readYesNo("重启设备? (y/n)", false);
    if (confirmation) {
        terminalView.println("\n正在重启, 会话将丢失...");
        systemService.reboot(hard);
    }
}