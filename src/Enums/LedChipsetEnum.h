#pragma once

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <FastLED.h>

// Enum is from FastLED, ESPIChipsets

class LedChipsetMapper {
public:
    struct ChipsetInfo {
        std::string name;
        bool usesClock;
    };

    static ESPIChipsets fromString(const std::string& name) {
        std::string lowered = toLower(name);
        auto it = stringToEnum.find(lowered);
        if (it != stringToEnum.end()) {
            return it->second;
        }
        return ESPIChipsets::APA102; // fallback
    }

    static std::string toString(ESPIChipsets chipset) {
        auto it = enumToString.find(chipset);
        if (it != enumToString.end()) {
            return it->second;
        }
        return "apa102"; // fallback
    }

    static std::vector<std::string> getAllChipsets() {
        std::vector<std::string> names;
        for (const auto& kv : stringToEnum) {
            names.push_back(kv.first);
        }
        return names;
    }

    static bool isClockBased(const std::string& name) {
        std::string lowered = toLower(name);
        auto it = stringToChipsetInfo.find(lowered);
        if (it != stringToChipsetInfo.end()) {
            return it->second.usesClock;
        }
        return true;
    }

    static std::string normalize(const std::string& name) {
        std::string lowered = toLower(name);
        return stringToEnum.count(lowered) ? lowered : "apa102";
    }

private:
    static std::string toLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        return out;
    }

    static const std::map<std::string, ESPIChipsets> stringToEnum;
    static const std::map<ESPIChipsets, std::string> enumToString;
    static const std::map<std::string, ChipsetInfo> stringToChipsetInfo;
};

// --- MAPPINGS ---

inline const std::map<std::string, ESPIChipsets> LedChipsetMapper::stringToEnum = {
    {"apa102",     APA102},
    {"apa102hd",   APA102HD},
    {"dotstar",    DOTSTAR},
    {"dotstarhd",  DOTSTARHD},
    {"lpd6803",    LPD6803},
    {"lpd8806",    LPD8806},
    {"ws2801",     WS2801},
    {"ws2803",     WS2803},
    // {"sm16716",    SM16716}, // crash the app
    {"p9813",      P9813},
    {"sk9822",     SK9822},
    {"sk9822hd",   SK9822HD},
    {"hd107",      HD107},
    {"hd107hd",    HD107HD}
};

inline const std::map<ESPIChipsets, std::string> LedChipsetMapper::enumToString = [] {
    std::map<ESPIChipsets, std::string> result;
    for (const auto& pair : LedChipsetMapper::stringToEnum) {
        result[pair.second] = pair.first;
    }
    return result;
}();

inline const std::map<std::string, LedChipsetMapper::ChipsetInfo> LedChipsetMapper::stringToChipsetInfo = {
    {"apa102",     {"APA102",     true}},
    {"apa102hd",   {"APA102HD",   true}},
    {"dotstar",    {"DOTSTAR",    true}},
    {"dotstarhd",  {"DOTSTARHD",  true}},
    {"lpd6803",    {"LPD6803",    true}},
    {"lpd8806",    {"LPD8806",    true}},
    {"ws2801",     {"WS2801",     true}},
    {"ws2803",     {"WS2803",     true}},
    // {"sm16716",    {"SM16716",    true}}, // crash the app
    {"p9813",      {"P9813",      true}},
    {"sk9822",     {"SK9822",     true}},
    {"sk9822hd",   {"SK9822HD",   true}},
    {"hd107",      {"HD107",      true}},
    {"hd107hd",    {"HD107HD",    true}},
};
