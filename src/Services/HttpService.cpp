#include "HttpService.h"

void HttpService::ensureClient(bool https, bool insecure, int timeout_s)
{
    // 若客户端未初始化或HTTPS类型变更则重新创建
    if (!client_inited_ || client_https_ != https) {
        client_.reset(); // 正确销毁旧客户端
        if (https) {
            auto* c = new WiFiClientSecure();
            if (insecure) static_cast<WiFiClientSecure*>(c)->setInsecure();
            client_.reset(c);
        } else {
            auto* c = new WiFiClient();
            client_.reset(c);
        }
        client_https_ = https;
        client_inited_ = true;
    } else {
        // 复用现有客户端
        if (https) {
            auto* c = static_cast<WiFiClientSecure*>(client_.get());
            if (insecure) c->setInsecure();
        }
    }
}

bool HttpService::beginHttp(const std::string& url, int timeout_ms)
{
    http_.setTimeout(timeout_ms);
    http_.setReuse(false);

    return http_.begin(*client_, url.c_str());
}

void HttpService::startGetTask(const std::string& url, int timeout_ms, int bodyMaxBytes, bool insecure,
                               int stack_bytes, int core, bool onlyContent)
{
    ready = false;
    auto* p = new HttpGetParams{url, timeout_ms, bodyMaxBytes, insecure, onlyContent, this};
    xTaskCreatePinnedToCore(&HttpService::getTask, "HttpGet", stack_bytes,
                            p, 1, nullptr, core);
}

void HttpService::getTask(void* pv)
{
    auto* p = static_cast<HttpGetParams*>(pv);
    auto* self = p->self;
    self->ready.store(false, std::memory_order_relaxed);

    // 判断请求是否为HTTPS协议
    const bool isHttps = (p->url.rfind("https://", 0) == 0);

    // 复用HTTP客户端
    self->ensureClient(isHttps, p->insecure, p->timeout_ms / 1000);
    std::string result;

    // 初始化HTTP请求
    if (!self->beginHttp(p->url, p->timeout_ms)) {
        result = "错误：初始化失败";
        self->http_.getStream().stop();
        self->http_.end();
        delete p;
        vTaskDelete(nullptr);
        return;
    }

    // 收集指定的响应头
    self->http_.collectHeaders(HttpService::headerKeys,
                               sizeof(HttpService::headerKeys) / sizeof(HttpService::headerKeys[0]));
    // 添加请求头：禁用压缩、关闭连接
    self->http_.addHeader("Accept-Encoding", "identity");
    self->http_.addHeader("Connection", "close");

    // 发送GET请求
    const int code = self->http_.GET();
    if (code > 0) {
        // 响应头信息
        if (!p->onlyContent) {
            result = "HTTP/1.1 " + std::to_string(code) + "\r\n";
            const int n = self->http_.headers();
            for (int i = 0; i < n; i++) {
                result += self->http_.headerName(i).c_str();
                result += ": ";
                result += self->http_.header(i).c_str();
                result += "\r\n";
            }
        }

        // 处理Content-Type响应头
        const String ct = self->http_.header("Content-Type");
        const bool hasCT   = ct.length() > 0;
        const bool isJson  = hasCT && (ct.indexOf("json")  >= 0);  // JSON格式
        const bool isText = hasCT && (ct.indexOf("plain") >= 0);  // 纯文本格式

        if (isJson) {
            if (!p->onlyContent) result += "\r\nJSON 内容:\n";
            result += HttpService::getJsonBody(self->http_, p->bodyMaxBytes);
        }
        else if (isText) {
            if (!p->onlyContent) result += "\r\n文本内容:\n";
            result += HttpService::getTextBody(self->http_, p->bodyMaxBytes);
        }
    } else {
        result = "错误：";
        result += self->http_.errorToString(code).c_str();
    }

    // 清理HTTP资源
    self->http_.getStream().stop();
    self->http_.end();
    vTaskDelay(pdMS_TO_TICKS(10));

    // 标记响应已就绪
    self->response = std::move(result);
    self->ready.store(true, std::memory_order_release);

    delete p;
    vTaskDelete(nullptr);
}

std::string HttpService::fetchJson(const std::string& url ,int bodyMaxBytes)
{
    constexpr int timeout_ms   = 10000;  // 默认超时时间：10秒
    constexpr bool insecure    = true;   // 禁用SSL证书验证
    constexpr int stack_bytes  = 20000;  // 任务栈大小
    constexpr int core         = 1;      // 绑定到核心1

    // 启动GET请求任务
    startGetTask(url, timeout_ms, bodyMaxBytes,
                 insecure, stack_bytes, core, true);

    // 等待响应返回
    unsigned long start = millis();
    while (!isResponseReady() && millis() - start < timeout_ms) {
        delay(100);
    }

    if (!isResponseReady()) {
        return "错误：等待响应超时";
    }

    // 获取响应内容
    std::string resp = lastResponse();

    return resp;
}

std::string HttpService::getJsonBody(HTTPClient& http, int bodyMaxBytes)
{
    std::string out;
    if (bodyMaxBytes <= 0) return out;

    WiFiClient* stream = http.getStreamPtr();
    size_t budget = static_cast<size_t>(bodyMaxBytes);  // 剩余可读取字节数

    int size = http.getSize();
    size_t target = (size > 0) ? (size_t)size : budget;  // 目标读取字节数
    if (size > 0 && target > budget) target = budget;    // 不超过最大限制

    static constexpr size_t CHUNK = 256;  // 每次读取的块大小
    uint8_t buf[CHUNK];

    out.reserve(std::min(target, (size_t)128));  // 预分配内存

    const unsigned long IDLE_TIMEOUT_MS = 1200;  // 空闲超时：1.2秒
    unsigned long lastDataMs = millis();         // 最后一次收到数据的时间
    size_t readTotal = 0;                        // 已读取总字节数

    // 判断是否可以继续读取
    auto canContinue = [&]() -> bool {
        if (budget == 0) return false;                       // 无剩余字节可读取
        if (stream->available() > 0) return true;            // 有数据可读取
        if ((size < 0) && !stream->connected() && stream->available() == 0) return false;  // 连接关闭且无数据
        return (millis() - lastDataMs) < IDLE_TIMEOUT_MS;    // 未超时
    };

    while (canContinue()) {
        int avail = stream->available();
        if (avail <= 0) { delay(1); continue; }

        size_t toRead = (size_t)avail;
        if (toRead > CHUNK)  toRead = CHUNK;    // 不超过块大小
        if (toRead > budget) toRead = budget;   // 不超过剩余字节数

        int n = stream->read(buf, (int)toRead);
        if (n <= 0) { delay(1); continue; }

        size_t oldSz = out.size();
        out.resize(oldSz + (size_t)n);
        memcpy(&out[oldSz], buf, (size_t)n);

        readTotal += (size_t)n;
        budget    -= (size_t)n;
        lastDataMs = millis();

        if (size > 0 && readTotal >= (size_t)size) break;  // 读取完成
        if (budget == 0) {
            out += "...[内容已截断]";
            break;
        }
    }
    return out;
}

std::string HttpService::getTextBody(HTTPClient& http, size_t maxBytes)
{
    Stream& s = http.getStream();
    std::string out;
    out.reserve(maxBytes);  // 预分配内存

    const unsigned long deadline = millis() + 3000;  // 读取超时：3秒
    while ((millis() < deadline) && (out.size() < maxBytes)) {
        while (s.available() && out.size() < maxBytes) {
            int c = s.read();
            if (c < 0) break;
            out.push_back(static_cast<char>(c));
        }
        if (!s.available()) delay(10);
    }
    return out;
}

std::string HttpService::lastResponse()
{
    std::string out;
    response.swap(out);
    ready.store(false, std::memory_order_release);
    return out;
}

bool HttpService::isResponseReady() const noexcept 
{
    return ready.load(std::memory_order_acquire);
}