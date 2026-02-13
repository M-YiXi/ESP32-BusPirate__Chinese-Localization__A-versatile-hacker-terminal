#include "CanService.h"
#include <SPI.h>
#include <cstdio>
#include <cstring>

void CanService::configure(uint8_t cs, uint8_t sck, uint8_t miso, uint8_t mosi, uint32_t bitrateKbps) {
    // 保存参数用于reset()调用
    csPin = cs;
    sckPin = sck;
    misoPin = miso;
    mosiPin = mosi;
    kbps = bitrateKbps;

    reset();
}

void CanService::reset() {
    SPI.end();
    delay(10);
    SPI.begin(sckPin, misoPin, mosiPin, csPin);
    delay(50);
    mcp2515.reset();
    mcp2515.setBitrate(resolveBitrate(kbps));
    mcp2515.setNormalMode();
}

void CanService::end() {
    SPI.end();
}

bool CanService::sendFrame(uint32_t id, const std::vector<uint8_t>& data) {
    struct can_frame frame;
    frame.can_id = id;
    frame.can_dlc = data.size();
    memcpy(frame.data, data.data(), data.size());
    return mcp2515.sendMessage(&frame) == MCP2515::ERROR_OK;
}

bool CanService::readFrame(struct can_frame& outFrame) {
    if (!mcp2515.checkReceive()) return false;
    return mcp2515.readMessage(&outFrame) == MCP2515::ERROR_OK;
}

std::string CanService::readFrameAsString() {
    struct can_frame frame;
    if (!readFrame(frame)) return "";

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "| ID: 0x%03X | DLC: %d | 数据:", frame.can_id, frame.can_dlc);

    std::string result = buffer;
    for (int i = 0; i < frame.can_dlc; ++i) {
        snprintf(buffer, sizeof(buffer), " %02X", frame.data[i]);
        result += buffer;
    }
    return result;
}

std::string CanService::getStatus() {
    uint8_t status = mcp2515.getStatus();
    uint8_t interrupts = mcp2515.getInterrupts();
    uint8_t errors = mcp2515.getErrorFlags();
    std::string result;

    // --- 状态位 ---
    result += "   状态位:";
    bool hasStatus = false;
    if (status & MCP2515::CANINTF_RX0IF) { result += " RX0有消息"; hasStatus = true; }
    if (status & MCP2515::CANINTF_RX1IF) { result += " RX1有消息"; hasStatus = true; }
    if (status & MCP2515::CANINTF_TX0IF) { result += " TX0请求"; hasStatus = true; }
    if (status & MCP2515::CANINTF_TX1IF) { result += " TX1请求"; hasStatus = true; }
    if (status & MCP2515::CANINTF_TX2IF) { result += " TX2请求"; hasStatus = true; }
    if (status & MCP2515::CANINTF_ERRIF) { result += " 错误中断"; hasStatus = true; }
    if (status & MCP2515::CANINTF_WAKIF) { result += " 唤醒中断"; hasStatus = true; }
    if (status & MCP2515::CANINTF_MERRF) { result += " 报文错误中断"; hasStatus = true; }
    if (!hasStatus) result += " 无";
    result += "\n\r";

    // --- 中断 ---
    result += "   中断:";
    bool hasInterrupt = false;
    if (interrupts & MCP2515::CANINTF_RX0IF) { result += " RX0"; hasInterrupt = true; }
    if (interrupts & MCP2515::CANINTF_RX1IF) { result += " RX1"; hasInterrupt = true; }
    if (interrupts & MCP2515::CANINTF_TX0IF) { result += " TX0"; hasInterrupt = true; }
    if (interrupts & MCP2515::CANINTF_TX1IF) { result += " TX1"; hasInterrupt = true; }
    if (interrupts & MCP2515::CANINTF_TX2IF) { result += " TX2"; hasInterrupt = true; }
    if (interrupts & MCP2515::CANINTF_ERRIF) { result += " 错误"; hasInterrupt = true; }
    if (interrupts & MCP2515::CANINTF_WAKIF) { result += " 唤醒"; hasInterrupt = true; }
    if (interrupts & MCP2515::CANINTF_MERRF) { result += " 报文错误"; hasInterrupt = true; }
    if (!hasInterrupt) result += " 无";
    result += "\n\r";

    // --- 错误标志 ---
    result += "   错误标志:";
    bool hasError = false;
    if (errors & MCP2515::EFLG_RX0OVR) { result += " RX0溢出"; hasError = true; }
    if (errors & MCP2515::EFLG_RX1OVR) { result += " RX1溢出"; hasError = true; }
    if (errors & MCP2515::EFLG_TXBO)   { result += " 发送总线关闭"; hasError = true; }
    if (errors & MCP2515::EFLG_TXEP)   { result += " 发送被动状态"; hasError = true; }
    if (errors & MCP2515::EFLG_RXEP)   { result += " 接收被动状态"; hasError = true; }
    if (errors & MCP2515::EFLG_TXWAR)  { result += " 发送警告"; hasError = true; }
    if (errors & MCP2515::EFLG_RXWAR)  { result += " 接收警告"; hasError = true; }
    if (errors & MCP2515::EFLG_EWARN)  { result += " 错误警告"; hasError = true; }
    if (!hasError) result += " 无错误";
    result += "\n\r";

    // --- 错误计数器 ---
    char errcounters[64];
    snprintf(errcounters, sizeof(errcounters), "   发送错误数: %u \n\r   接收错误数: %u\n",
             mcp2515.errorCountTX(), mcp2515.errorCountRX());
    result += errcounters;

    return result;
}

void CanService::setFilter(uint32_t id) {
    // 将MCP2515切换到配置模式
    mcp2515.setConfigMode();

    // 为两个过滤器掩码设置全掩码
    mcp2515.setFilterMask(MCP2515::MASK0, false, 0x7FF);
    mcp2515.setFilterMask(MCP2515::MASK1, false, 0x7FF);

    // 为所有过滤器设置相同的过滤ID
    mcp2515.setFilter(MCP2515::RXF0, false, id);
    mcp2515.setFilter(MCP2515::RXF1, false, id);
    mcp2515.setFilter(MCP2515::RXF2, false, id);
    mcp2515.setFilter(MCP2515::RXF3, false, id);
    mcp2515.setFilter(MCP2515::RXF4, false, id);
    mcp2515.setFilter(MCP2515::RXF5, false, id);

    // 恢复到正常模式
    mcp2515.setNormalMode();
}

void CanService::setMask(uint32_t mask) {
    mcp2515.setFilterMask(MCP2515::MASK0, false, mask);
}

void CanService::setBitrate(uint32_t bitrateKbps) {
    mcp2515.setBitrate(resolveBitrate(bitrateKbps));
    mcp2515.setNormalMode();
}

void CanService::flush() {
    unsigned long start = millis();
    struct can_frame tmp;

    while (millis() - start < 10) {
        if (!readFrame(tmp)) break;
    }
}

CAN_SPEED CanService::resolveBitrate(uint32_t kbps) {
    switch (kbps) {
        case 5:    return CAN_5KBPS;
        case 10:   return CAN_10KBPS;
        case 20:   return CAN_20KBPS;
        case 31:   return CAN_31K25BPS;
        case 33:   return CAN_33KBPS;
        case 40:   return CAN_40KBPS;
        case 50:   return CAN_50KBPS;
        case 80:   return CAN_80KBPS;
        case 100:  return CAN_100KBPS;
        case 125:  return CAN_125KBPS;
        case 200:  return CAN_200KBPS;
        case 250:  return CAN_250KBPS;
        case 500:  return CAN_500KBPS;
        case 1000: return CAN_1000KBPS;
        default:   return CAN_125KBPS;
    }
}

uint32_t CanService::closestSupportedBitrate(uint32_t kbps) {
    const uint32_t supported[] = {
        5, 10, 20, 31, 33, 40, 50, 80, 100, 125, 200, 250, 500, 1000
    };

    uint32_t best = supported[0];
    uint32_t minDiff = abs((int32_t)(kbps - best));

    for (size_t i = 1; i < sizeof(supported)/sizeof(supported[0]); ++i) {
        uint32_t diff = abs((int32_t)(kbps - supported[i]));
        if (diff < minDiff) {
            best = supported[i];
            minDiff = diff;
        }
    }

    return best;
}

bool CanService::probe() {
    // 回环测试
    if (mcp2515.setLoopbackMode() != MCP2515::ERROR_OK) return false;

    // 准备测试帧
    struct can_frame f{};
    f.can_id  = 0x123;
    f.can_dlc = 2;
    f.data[0] = 0xAA;
    f.data[1] = 0xAA;

    // 发送测试帧
    if (mcp2515.sendMessage(&f) != MCP2515::ERROR_OK) {
        mcp2515.setNormalMode();
        return false;
    }

    // 接收返回的帧
    unsigned long t0 = millis();
    while (millis() - t0 < 20) {
        if (mcp2515.checkReceive()) {
            struct can_frame rx{};
            if (mcp2515.readMessage(&rx) == MCP2515::ERROR_OK &&
                rx.can_id == f.can_id && rx.can_dlc == f.can_dlc &&
                rx.data[0] == f.data[0] && rx.data[1] == f.data[1]) {
                mcp2515.setNormalMode();
                return true; // CAN总线正常，收到了发送的测试帧
            }
        }
        delay(1);
    }
    mcp2515.setNormalMode();
    return false;
}