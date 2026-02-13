#pragma once

#include <FastLED.h>
#include <string>
#include <vector>
#include <map>

class LedService {
public:
    LedService();

    void configure(uint8_t dataPin, uint8_t clockPin, uint16_t length, const std::string& protocol, uint8_t brightness);
    void fill(const CRGB& color);
    void set(uint16_t index, const CRGB& color);
    void resetLeds();
    void runAnimation(const std::string& type);
    bool isAnimationRunning() const;
    static std::vector<std::string> getSingleWireProtocols();
    static std::vector<std::string> getSpiChipsets();
    static std::vector<std::string> getSupportedProtocols();
    static std::vector<std::string> getSupportedAnimations();
    CRGB parseStringColor(const std::string& input);
    CRGB parseHtmlColor(const std::string& input);
private:
    CRGB* leds = nullptr;
    uint16_t ledCount = 0;
    bool usesClock = false;
    bool animationRunning = false;
};