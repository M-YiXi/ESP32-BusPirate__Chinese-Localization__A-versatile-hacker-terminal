#include "WifiOpenScannerService.h"

portMUX_TYPE WifiOpenScannerService::probeMux = portMUX_INITIALIZER_UNLOCKED;
std::vector<std::string> WifiOpenScannerService::probeLog;

bool WifiOpenScannerService::startOpenProbe(uint32_t scanIntervalMs) {
    if (openProbeRunning.load()) return true;
    openProbeRunning = true;

    BaseType_t ok = xTaskCreatePinnedToCore(
        &WifiOpenScannerService::openProbeTaskThunk,
        "wifi_open_probe",
        6144,               // 栈大小 //汉化
        this,
        1,                  // 低优先级 //汉化
        &openProbeHandle,
        0                   // 核心 0 //汉化
    );
    if (ok != pdPASS) {
        openProbeRunning = false;
        openProbeHandle = nullptr;
        return false;
    }
    ulTaskNotifyValueClear(openProbeHandle, 0xFFFFFFFF);
    xTaskNotify(openProbeHandle, scanIntervalMs, eSetValueWithOverwrite);
    return true;
}

void WifiOpenScannerService::stopOpenProbe() {
    if (!openProbeRunning.load()) return;
    openProbeRunning = false;
    if (openProbeHandle) {
        // 需要时唤醒 //汉化
        xTaskNotifyGive(openProbeHandle);
        for (int i=0; i<40 && openProbeHandle; ++i) vTaskDelay(pdMS_TO_TICKS(25));
    }
}

void WifiOpenScannerService::openProbeTaskThunk(void* arg) {
    auto* self = static_cast<WifiOpenScannerService*>(arg);
    uint32_t interval = 2500, v=0;
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &v, pdMS_TO_TICKS(10)) == pdTRUE && v>0) interval = v;
    self->openProbeTask(interval);
    vTaskDelete(nullptr);
}

bool WifiOpenScannerService::isOpenAuth(int enc) {
    return enc == WIFI_AUTH_OPEN;
}

void WifiOpenScannerService::openProbeTask(uint32_t scanIntervalMs) {
    pushProbeLog("[探测] 已启动，正在尝试连接开放 Wi-Fi 网络..."); //汉化
    WiFi.mode(WIFI_STA);

    while (openProbeRunning.load()) {
        unsigned long scanMs = 0;
        int n = doScan(/*显示隐藏网络=*/true, scanMs); //汉化
        if (n >= 0) {
            pushProbeLog("[扫描] 发现 " + std::to_string(n) + " 个网络，耗时 " + std::to_string(scanMs) + " 毫秒"); //汉化
            pushProbeLog("[扫描] 正在处理每个网络的探测连接..."); //汉化
            processAllNetworks(n);
        } else {
            pushProbeLog("[错误] 扫描失败"); //汉化
        }

        pushProbeLog("[完成] 探测周期结束。重新启动... 按 [回车] 停止"); //汉化

        uint32_t slept = 0;
        while (slept < scanIntervalMs && openProbeRunning.load()) {
            if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50)) > 0) break;
            slept += 50;
        }
    }

    pushProbeLog("[探测] 用户已停止"); //汉化
    openProbeHandle = nullptr;
}

// ===== 步骤（小函数）===== //汉化

int WifiOpenScannerService::doScan(bool showHidden, unsigned long& outScanMs) {
    unsigned long t0 = millis();
    int n = WiFi.scanNetworks(/*异步=*/false, showHidden); //汉化
    outScanMs = millis() - t0;

    // 恢复 //汉化
    if (n >= 0) maybeRecoverFromFastScan(outScanMs);
    return n;
}

void WifiOpenScannerService::maybeRecoverFromFastScan(unsigned long scanMs) {
    if (scanMs < 20) {
        pushProbeLog("[警告] 快速扫描（<20ms），正在重置 WiFi STA..."); //汉化
        WiFi.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(300));
        WiFi.mode(WIFI_STA);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void WifiOpenScannerService::processAllNetworks(int count) {
    for (int i = 0; i < count && openProbeRunning.load(); ++i) {
        processOneNetwork(i);
    }
}

void WifiOpenScannerService::processOneNetwork(int idx) {
    const int enc = WiFi.encryptionType(idx);
    const bool open = isOpenAuth(enc);
    const std::string ssid = getSsid(idx);

    // 仅处理开放网络 //汉化
    if (!open) {
        char skip[256];
        snprintf(skip, sizeof(skip),
                 "[跳过] SSID=\"%s\" 加密=%s (非开放)", //汉化
                 ssid.c_str(), encToStr(enc));
        pushProbeLog(skip);
        return;
    }

    std::string ip;
    unsigned long connectMs = 0;
    const unsigned long timeoutMs = 12000;

    // 编译时 M5Stick 会 IRAM 溢出，跳过连接检查 //汉化
    #ifdef DEVICE_M5STICK
        char line[256];
        snprintf(line, sizeof(line),
                "[跳过] SSID=\"%s\" 加密=%s -> 开放。M5Stick 上无法检查互联网访问", //汉化
                ssid.c_str(), encToStr(enc));
        pushProbeLog(line);
    #else
        const bool ok = connectToNetwork(ssid, /*是开放网络=*/true, timeoutMs, ip, connectMs); //汉化
        if (!ok) {
            char line[320];
            snprintf(line, sizeof(line),
                    "[尝试]  SSID=\"%s\" 加密=%s -> 连接失败 (%lums)", //汉化
                    ssid.c_str(), encToStr(enc), connectMs);
            pushProbeLog(line);
            safeDisconnect();

            // 测试 HTTP/互联网 //汉化
            int httpCode = -1; unsigned long httpMs = 0;
            const bool internet = performHttpCheck(httpCode, httpMs);

            snprintf(line, sizeof(line),
                    "[尝试]  SSID=\"%s\" 加密=%s -> 已连接 ip=%s (连接耗时 %lums) HTTP=%d (%s, %lums)", //汉化
                    ssid.c_str(), encToStr(enc), ip.c_str(), connectMs, httpCode,
                    internet ? "互联网正常" : "无互联网", httpMs); //汉化
            pushProbeLog(line);

            safeDisconnect(50);
        }
    #endif
}

bool WifiOpenScannerService::connectToNetwork(const std::string& ssid,
                                          bool isOpen,
                                          unsigned long timeoutMs,
                                          std::string& outIp,
                                          unsigned long& outElapsedMs) {
    unsigned long t0 = millis();

    WiFi.begin(ssid.c_str(), isOpen ? nullptr : "");

    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    const bool ok = (WiFi.status() == WL_CONNECTED);
    outElapsedMs = millis() - t0;
    outIp = ok ? std::string(WiFi.localIP().toString().c_str()) : std::string();

    // 记录结果 //汉化
    if (ok) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[尝试]  SSID=\"%s\" -> 已连接 ip=%s (连接耗时 %lums)", //汉化
                 ssid.c_str(), outIp.c_str(), outElapsedMs);
        pushProbeLog(msg);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "[尝试]  SSID=\"%s\" -> 连接失败 (%lums)", //汉化
                 ssid.c_str(), outElapsedMs);
        pushProbeLog(msg);
    }

    return ok;
}

bool WifiOpenScannerService::performHttpCheck(int& outHttpCode, unsigned long& outHttpMs) {
    unsigned long t0 = millis();
    HTTPClient http;

    if (http.begin("http://connectivitycheck.gstatic.com/generate_204")) {
        http.setTimeout(4000);
        outHttpCode = http.GET();
        http.end();
        outHttpMs = millis() - t0;
        if (outHttpCode == 204) return true;
        if (outHttpCode > 0)   return true;
    } else {
        if (http.begin("http://example.com")) {
            http.setTimeout(4000);
            outHttpCode = http.GET();
            http.end();
            outHttpMs = millis() - t0;
            return outHttpCode > 0;
        }
    }
    outHttpMs = millis() - t0;
    return false;
}

void WifiOpenScannerService::safeDisconnect(unsigned delayMs) {
    WiFi.disconnect(true);
    if (delayMs) vTaskDelay(pdMS_TO_TICKS(delayMs));
}

const char* WifiOpenScannerService::encToStr(int enc) const {
    switch (enc) {
        case WIFI_AUTH_OPEN:           return "开放"; //汉化
        case WIFI_AUTH_WEP:            return "WEP"; //汉化（标准缩写保留）
        case WIFI_AUTH_WPA_PSK:        return "WPA"; //汉化
        case WIFI_AUTH_WPA2_PSK:       return "WPA2"; //汉化
        case WIFI_AUTH_WPA_WPA2_PSK:   return "WPA+WPA2"; //汉化
        case WIFI_AUTH_WPA2_ENTERPRISE:return "WPA2-企业"; //汉化
        case WIFI_AUTH_WPA3_PSK:       return "WPA3"; //汉化
        case WIFI_AUTH_WPA2_WPA3_PSK:  return "WPA2+WPA3"; //汉化
        case WIFI_AUTH_WAPI_PSK:       return "WAPI"; //汉化
        default:                       return "未知"; //汉化
    }
}

std::string WifiOpenScannerService::getSsid(int idx) const {
    String s = WiFi.SSID(idx);
    return s.length() ? std::string(s.c_str()) : std::string("隐藏 SSID"); //汉化
}

void WifiOpenScannerService::pushProbeLog(const std::string& line) {
    portENTER_CRITICAL(&probeMux);
    probeLog.push_back(line);
    if (probeLog.size() > PROBE_LOG_MAX) {
        size_t excess = probeLog.size() - PROBE_LOG_MAX;
        probeLog.erase(probeLog.begin(), probeLog.begin() + excess);
    }
    portEXIT_CRITICAL(&probeMux);
}

std::vector<std::string> WifiOpenScannerService::fetchProbeLog() {
    std::vector<std::string> batch;
    portENTER_CRITICAL(&probeMux);
    batch.swap(probeLog);
    portEXIT_CRITICAL(&probeMux);
    return batch;
}

void WifiOpenScannerService::clearProbeLog() {
    portENTER_CRITICAL(&probeMux);
    probeLog.clear();
    portEXIT_CRITICAL(&probeMux);
}