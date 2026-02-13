#include "BinaryAnalyzeManager.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>

BinaryAnalyzeManager::BinaryAnalyzeManager(ITerminalView& view, IInput& input)
    : terminalView(view), terminalInput(input) {}

const char* BinaryAnalyzeManager::detectSensitivePattern(const uint8_t* buf, size_t size) {
    static const char* patterns[] = {
        "-----BEGIN RSA PRIVATE KEY-----", "-----BEGIN PRIVATE KEY-----", "-----BEGIN CERTIFICATE-----",
        "ssh-rsa", "ssh-ed25519", "password=", "pwd=", "pass:", "login:", "user:", "admin",
        "http://", "https://", "ftp://", "CONFIG_", "ENV_", "PATH=", "HOME=", "DEVICE="
    };
    static const char* labels[] = {
        "RSA私钥", // 汉化
        "私钥", // 汉化
        "证书", // 汉化
        "SSH RSA密钥", // 汉化
        "SSH Ed25519密钥", // 汉化
        "密码", // 汉化
        "密码", // 汉化
        "密码", // 汉化
        "登录名", // 汉化
        "用户名", // 汉化
        "管理员相关字符串", // 汉化
        "网址", // 汉化
        "网址", // 汉化
        "FTP网址", // 汉化
        "配置变量", // 汉化
        "环境变量", // 汉化
        "路径变量", // 汉化
        "主目录变量", // 汉化
        "设备变量" // 汉化
    };
    static const size_t patternCount = sizeof(patterns) / sizeof(patterns[0]);

    for (size_t i = 0; i < patternCount; ++i) {
        size_t len = strlen(patterns[i]);
        for (size_t j = 0; j + len <= size; ++j) {
            bool match = true;
            for (size_t k = 0; k < len; ++k) {
                char c1 = std::tolower((unsigned char)buf[j + k]);
                char c2 = std::tolower((unsigned char)patterns[i][k]);
                if (c1 != c2) {
                    match = false;
                    break;
                }
            }
            if (match) return labels[i];
        }
    }
    return nullptr;
}

const char* BinaryAnalyzeManager::detectFileSignature(const uint8_t* buf, size_t size) {
    for (size_t sig = 0; sig < knownSignaturesCount; ++sig) {
        const auto& s = knownSignatures[sig];
        if (size < s.length) continue;
        for (size_t i = 0; i + s.length <= std::min(size_t(64), size); ++i) {
            if (buf[i] != s.pattern[0]) continue;
            if (memcmp(buf + i, s.pattern, s.length) == 0) return s.name;
        }
    }
    return nullptr;
}

BinaryBlockStats BinaryAnalyzeManager::analyzeBlock(const uint8_t* buffer, size_t size) {
    uint32_t printable = 0, nulls = 0, ff = 0, counts[256] = {0};
    float entropy = 0;
    for (size_t i = 0; i < size; ++i) {
        uint8_t b = buffer[i];
        counts[b]++;
        if (b >= 32 && b <= 126) printable++;
        if (b == 0x00) nulls++;
        if (b == 0xFF) ff++;
    }
    for (int i = 0; i < 256; ++i) {
        if (counts[i]) {
            float p = (float)counts[i] / size;
            entropy -= p * log2(p);
        }
    }
    return {entropy, printable, nulls, ff, detectFileSignature(buffer, size)};
}

BinaryAnalyzeManager::AnalysisResult BinaryAnalyzeManager::analyze(
    uint32_t start,
    uint32_t totalSize,
    std::function<void(uint32_t address, uint8_t* buffer, uint32_t size)> fetch,
    uint32_t blockSize
) {
    const uint32_t overlap = 32;
    uint8_t buffer[blockSize + overlap];

    uint32_t printableTotal = 0, nullsTotal = 0, ffTotal = 0, blocks = 0;
    float entropySum = 0;
    std::vector<std::string> foundFiles, foundSecrets;
    uint32_t totalBlocks = (totalSize - start) / blockSize;
    uint32_t dotInterval = std::max(totalBlocks / 30, 1u);

    terminalView.print("分析中"); // 汉化

    for (uint32_t addr = start; addr < totalSize; addr += blockSize, ++blocks) {
        uint32_t readAddr = (addr >= overlap) ? (addr - overlap) : 0;
        uint32_t readSize = (addr >= overlap) ? (blockSize + overlap) : (blockSize + addr);
        fetch(readAddr, buffer, readSize);

        const uint8_t* blockData = buffer + (addr >= overlap ? overlap : 0);

        BinaryBlockStats stats = analyzeBlock(blockData, blockSize);
        entropySum += stats.entropy;
        printableTotal += stats.printable;
        nullsTotal += stats.nulls;
        ffTotal += stats.ff;

        if (stats.signature) {
            std::stringstream ss;
            ss << "0x" << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << addr;
            ss << " → " << stats.signature;
            foundFiles.push_back(ss.str());
        }

        const char* sensitive = detectSensitivePattern(buffer, readSize);
        if (sensitive) {
            std::stringstream ss;
            ss << "0x" << std::hex << std::uppercase << std::setw(6) << std::setfill('0') << addr;
            ss << " → 疑似" << sensitive; // 汉化
            foundSecrets.push_back(ss.str());
        }

        if (blocks % dotInterval == 0) {
            terminalView.print(".");
        }

        char c = terminalInput.readChar();
        if (c == '\r' || c == '\n') {
            terminalView.println("\n[部分分析] 已被用户终止。\n"); // 汉化
            break;
        };
    }

    float avgEntropy = (blocks > 0) ? (entropySum / blocks) : 0;
    return {avgEntropy, blocks * blockSize, blocks, printableTotal, nullsTotal, ffTotal, foundFiles, foundSecrets};
}

std::string BinaryAnalyzeManager::formatAnalysis(const AnalysisResult& result) {
    if (result.totalBytes == 0) return "❌ 未分析任何数据。\n"; // 汉化

    float printablePct = 100.0f * result.printableTotal / result.totalBytes;
    float nullsPct     = 100.0f * result.nullsTotal     / result.totalBytes;
    float ffPct        = 100.0f * result.ffTotal        / result.totalBytes;
    uint32_t dataBytes = result.totalBytes - (result.nullsTotal + result.ffTotal);
    float dataPct      = 100.0f * dataBytes / result.totalBytes;

    float normalizedEntropy = result.avgEntropy / 8.0f;
    int barLength = 20;
    int filled = std::round(normalizedEntropy * barLength);

    std::string bar = "[";
    for (int i = 0; i < barLength; ++i)
        bar += (i < filled) ? '#' : '.';
    bar += "]";

    std::string interpretation;
    if (normalizedEntropy >= 0.95f)
        interpretation = "→ 可能是加密/压缩数据"; // 汉化
    else if (normalizedEntropy >= 0.85f)
        interpretation = "→ 大部分为压缩数据"; // 汉化
    else if (normalizedEntropy >= 0.65f)
        interpretation = "→ 混合内容"; // 汉化
    else if (normalizedEntropy >= 0.4f)
        interpretation = "→ 部分结构化数据"; // 汉化
    else if (normalizedEntropy >= 0.2f)
        interpretation = "→ 包含填充数据"; // 汉化
    else
        interpretation = "→ 可能为空数据"; // 汉化

    char line[512];
    snprintf(line, sizeof(line),
        "\n\n\r📊 分析摘要：\n\r" // 汉化
        " • 总字节数：     %u\n\r" // 汉化
        " • 已分析块数：   %u\n\r" // 汉化
        " • 平均熵值：     %.2f / 8.00\n\r" // 汉化
        " • 熵值进度条：   %s %s\n\r" // 汉化
        " • 可打印字符占比：%.2f%%\n\r" // 汉化
        " • 空字节占比：   %.2f%%\n\r" // 汉化
        " • 0xFF字节占比： %.2f%%\n\r" // 汉化
        " • 有效数据占比： %.2f%%\r", // 汉化
        result.totalBytes,
        result.blocks,
        result.avgEntropy,
        bar.c_str(),
        interpretation.c_str(),
        printablePct,
        nullsPct,
        ffPct,
        dataPct
    );

    return std::string(line);
}

std::vector<std::string> BinaryAnalyzeManager::extractPrintableStrings(const uint8_t* buf, size_t size, size_t minLen) {
    std::vector<std::string> strings;
    std::string current;
    for (size_t i = 0; i < size; ++i) {
        char c = buf[i];
        if (c >= 32 && c <= 126) {
            current += c;
        } else {
            if (current.length() >= minLen) {
                strings.push_back(current);
            }
            current.clear();
        }
    }
    if (current.length() >= minLen) strings.push_back(current);
    return strings;
}

const FileSignature BinaryAnalyzeManager::knownSignatures[] = {
    // Executables / Boot
    { "ELF可执行文件",          (const uint8_t*)"\x7F""ELF", 4 }, // 汉化
    { "U-Boot镜像文件",           (const uint8_t*)"\x27\x05\x19\x56", 4 }, // 汉化

    // Archives / Compression
    { "GZIP压缩包",            (const uint8_t*)"\x1F\x8B", 2 }, // 汉化
    { "ZIP压缩包",             (const uint8_t*)"\x50\x4B\x03\x04", 4 }, // 汉化
    { "7z压缩包",              (const uint8_t*)"\x37\x7A\xBC\xAF\x27\x1C", 6 }, // 汉化
    { "XZ压缩文件",           (const uint8_t*)"\xFD\x37\x7A\x58\x5A\x00", 6 }, // 汉化
    { "LZMA压缩文件",         (const uint8_t*)"\x5D\x00\x00", 3 }, // 汉化
    { "LZ4帧数据",               (const uint8_t*)"\x04\x22\x4D\x18", 4 }, // 汉化

    // File systems
    { "SquashFS文件系统",                (const uint8_t*)"hsqs", 4 }, // 汉化
    { "CRAMFS文件系统",                  (const uint8_t*)"\x45\x3D\xCD\x28", 4 }, // 汉化
    { "JFFS2文件系统",                   (const uint8_t*)"\x85\x19\x03\x20", 4 }, // 汉化
    { "UBI/UBIFS文件系统",               (const uint8_t*)"\x55\x42\x49\x23", 4 }, // 汉化
    { "Ext2/3/4超级块",     (const uint8_t*)"\x53\xEF", 2 }, // 汉化 // offset 0x438 en réalité

    // Images
    { "PNG图片",               (const uint8_t*)"\x89PNG", 4 }, // 汉化
    { "JPEG图片",              (const uint8_t*)"\xFF\xD8\xFF", 3 }, // 汉化
    { "GIF图片",               (const uint8_t*)"GIF8", 4 }, // 汉化
    { "BMP图片",               (const uint8_t*)"BM", 2 }, // 汉化

    // Documents
    { "PDF文档",            (const uint8_t*)"%PDF-", 5 }, // 汉化
    { "RTF文档",            (const uint8_t*)"{\\rtf", 5 }, // 汉化
    { "SQLite 3数据库",             (const uint8_t*)"SQLite format 3", 16 }, // 汉化

    // Audio / Video
    { "MP3音频（ID3标签）",               (const uint8_t*)"ID3", 3 }, // 汉化
    { "WAV音频",               (const uint8_t*)"RIFF", 4 }, // 汉化 // + "WAVE" after 8 bytes
    { "AVI视频",               (const uint8_t*)"RIFF", 4 }, // 汉化 // + "AVI " after 8 bytes

    // Divers
    { "TAR归档文件（ustar格式）",     (const uint8_t*)"ustar", 5 }, // 汉化
    { "RAFFS文件系统",                   (const uint8_t*)"\x52\x41\x46\x46\x53", 5 }, // 汉化
};
const size_t BinaryAnalyzeManager::knownSignaturesCount = sizeof(BinaryAnalyzeManager::knownSignatures) / sizeof(FileSignature);