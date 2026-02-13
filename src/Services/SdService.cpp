#include "SdService.h"

/**
 * @brief SdService构造函数
 * @note 初始化SD卡相关状态，默认未挂载
 */
SdService::SdService() {}

/**
 * @brief 配置SD卡的SPI引脚并挂载SD卡
 * @param clkPin SPI时钟引脚（SCK）
 * @param misoPin SPI MISO引脚（主入从出）
 * @param mosiPin SPI MOSI引脚（主出从入）
 * @param csPin SD卡片选引脚（CS/SS）
 * @return 挂载成功返回true，失败/已挂载返回false
 * @note 挂载前先初始化SPI总线，短暂延迟保证总线稳定
 */
bool SdService::configure(uint8_t clkPin, uint8_t misoPin, uint8_t mosiPin, uint8_t csPin) {
    // 已挂载则直接返回成功
    if (sdCardMounted) return true;

    // 初始化指定引脚的SPI总线
    SPI.begin(clkPin, misoPin, mosiPin, csPin);
    delay(10); // 延迟10ms，稳定SPI总线状态

    // 尝试挂载SD卡（指定CS引脚和SPI总线）
    if (!SD.begin(csPin, SPI)) {
        sdCardMounted = false; // 标记未挂载
        return false;
    }

    sdCardMounted = true; // 标记SD卡已挂载
    return sdCardMounted;
}

/**
 * @brief 卸载SD卡并释放SPI总线资源
 * @note 先调用SD.end()卸载，再停止SPI总线，重置挂载状态
 */
void SdService::end() {
    SD.end();          // 卸载SD卡
    SPI.end();         // 停止SPI总线
    sdCardMounted = false; // 重置挂载状态
}

/**
 * @brief 判断指定路径是否为文件（非目录）
 * @param filePath 文件路径（如"/data/test.txt"）
 * @return 是文件返回true，不是/文件不存在返回false
 * @note 打开文件后需检查是否为目录，且必须关闭文件避免资源泄漏
 */
bool SdService::isFile(const std::string& filePath) {
    File f = SD.open(filePath.c_str());
    if (f && !f.isDirectory()) {
        f.close(); // 关闭文件
        return true;
    }
    return false;
}

/**
 * @brief 判断指定路径是否为目录
 * @param path 目录路径（如"/data"）
 * @return 是目录返回true，不是/目录不存在返回false
 */
bool SdService::isDirectory(const std::string& path) {
    File f = SD.open(path.c_str());
    if (f && f.isDirectory()) {
        f.close(); // 关闭目录句柄
        return true;
    }
    return false;
}

/**
 * @brief 获取SD卡挂载状态
 * @return 已挂载返回true，未挂载返回false
 */
bool SdService::getSdState() {
    return sdCardMounted;
}

/**
 * @brief 列出指定目录下的文件/目录列表（文件夹优先，按名称排序）
 * @param dirPath 目录路径（如"/"）
 * @param limit 列表数量限制（0表示无限制，默认256）
 * @return 文件名/目录名向量（先文件夹后文件，均按名称排序）
 * @note 排除以"."开头的隐藏文件，避免系统文件干扰
 */
std::vector<std::string> SdService::listElements(const std::string& dirPath, size_t limit) {
    // 限制为0则设为256（默认最大数量）
    if (limit == 0) {
        limit = 256;
    }

    std::vector<std::string> filesList;   // 文件列表
    std::vector<std::string> foldersList; // 文件夹列表

    // SD卡未挂载则返回空列表
    if (!sdCardMounted) {
        return filesList;
    }

    // 打开目标目录
    File dir = SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) {
        return filesList; // 目录打开失败返回空
    }

    // 遍历目录下的所有条目
    File file = dir.openNextFile();
    size_t i = 0;
    while (file) {
        // 排除隐藏文件（以.开头）
        if (file.name()[0] != '.') {
            if (file.isDirectory()) {
                foldersList.push_back(file.name()); // 加入文件夹列表
            } else {
                filesList.push_back(file.name());   // 加入文件列表
            }
            i++;
        }

        // 达到数量限制则停止遍历
        if (i > limit) {
            break;
        }

        file = dir.openNextFile(); // 打开下一个条目
    }

    // 按名称排序（文件夹和文件分别排序）
    std::sort(foldersList.begin(), foldersList.end());
    std::sort(filesList.begin(), filesList.end());
    // 合并列表：文件夹在前，文件在后
    foldersList.insert(foldersList.end(), filesList.begin(), filesList.end());

    return foldersList;
}

/**
 * @brief 以二进制方式读取文件内容
 * @param filePath 文件路径
 * @return 二进制数据向量（未挂载/文件不存在返回空）
 * @note 预分配内存（reserve）提升读取效率，避免频繁扩容
 */
std::vector<uint8_t> SdService::readBinaryFile(const std::string& filePath) {
    std::vector<uint8_t> content;
    if (!sdCardMounted) {
        return content;
    }

    // 以只读模式打开文件
    File file = SD.open(filePath.c_str(), FILE_READ);
    if (file) {
        content.reserve(file.size()); // 预分配文件大小的内存
        while (file.available()) {
            content.push_back(file.read()); // 逐字节读取二进制数据
        }
        file.close(); // 关闭文件
    }
    return content;
}

/**
 * @brief 以文本方式读取文件内容
 * @param filePath 文件路径
 * @return 文本字符串（未挂载/文件不存在返回空）
 */
std::string SdService::readFile(const std::string& filePath) {
    std::string content;
    if (!sdCardMounted) {
        return content;
    }

    // 打开文件（默认只读模式）
    File file = SD.open(filePath.c_str());
    if (file) {
        while (file.available()) {
            content += static_cast<char>(file.read()); // 逐字符读取文本
        }
        file.close();
    }
    return content;
}

/**
 * @brief 分块读取文件内容（指定偏移量和最大字节数）
 * @param filePath 文件路径
 * @param offset 读取起始偏移量（字节）
 * @param maxBytes 最大读取字节数
 * @return 读取到的文本字符串（失败返回空）
 * @note 使用512字节缓冲区，减少IO次数，提升读取效率
 */
std::string SdService::readFileChunk(const std::string& filePath, size_t offset, size_t maxBytes) {
    std::string content;
    if (!sdCardMounted) return content;

    File file = SD.open(filePath.c_str());
    if (!file) return content;

    // 跳转到指定偏移量
    if (!file.seek(offset)) {
        file.close();
        return content;
    }

    const size_t BUFFER_SIZE = 512; // 缓冲区大小（适配SD卡扇区大小）
    char buffer[BUFFER_SIZE];

    size_t totalRead = 0;
    // 未到文件末尾且未读满maxBytes时继续读取
    while (file.available() && totalRead < maxBytes) {
        // 计算本次读取的字节数（不超过缓冲区和剩余需读字节）
        size_t toRead = std::min(BUFFER_SIZE, maxBytes - totalRead);
        size_t bytesRead = file.readBytes(buffer, toRead); // 读取到缓冲区
        content.append(buffer, bytesRead); // 追加到结果字符串
        totalRead += bytesRead;
    }

    file.close();
    return content;
}

/**
 * @brief 写入文本文件（覆盖/追加模式）
 * @param filePath 文件路径
 * @param data 待写入的文本数据
 * @param append 是否追加模式（true=追加，false=覆盖）
 * @return 写入成功返回true，失败返回false
 */
bool SdService::writeFile(const std::string& filePath, const std::string& data, bool append) {
    if (!sdCardMounted) {
        return false;
    }

    // 打开文件：追加模式/FILE_APPEND，覆盖模式/FILE_WRITE
    File file = SD.open(filePath.c_str(), append ? FILE_APPEND : FILE_WRITE);
    if (file) {
        // 写入文本数据（转换为uint8_t*）
        file.write(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
        file.close();
        return true;
    }
    return false;
}

/**
 * @brief 写入二进制文件（覆盖模式）
 * @param filePath 文件路径
 * @param data 待写入的二进制数据向量
 * @return 写入成功返回true，失败返回false
 */
bool SdService::writeBinaryFile(const std::string& filePath, const std::vector<uint8_t>& data) {
    if (!sdCardMounted) {
        return false;
    }

    File file = SD.open(filePath.c_str(), FILE_WRITE);
    if (file) {
        // 写入二进制数据（直接使用vector的data()和size()）
        file.write(data.data(), data.size());
        file.close();
        return true;
    }
    return false;
}

/**
 * @brief 追加文本到文件末尾
 * @param filePath 文件路径
 * @param data 待追加的文本数据
 * @return 追加成功返回true，失败返回false
 * @note 等价于writeFile(filePath, data, true)
 */
bool SdService::appendToFile(const std::string& filePath, const std::string& data) {
    if (!sdCardMounted) {
        return false;
    }

    File file = SD.open(filePath.c_str(), FILE_APPEND);
    if (file) {
        file.write(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
        file.close();
        return true;
    }
    return false;
}

/**
 * @brief 删除指定文件
 * @param filePath 文件路径
 * @return 删除成功返回true，失败/文件不存在返回false
 */
bool SdService::deleteFile(const std::string& filePath) {
    if (!sdCardMounted) {
        return false;
    }

    // 文件存在则删除
    if (SD.exists(filePath.c_str())) {
        return SD.remove(filePath.c_str());
    }
    return false;
}

/**
 * @brief 获取文件的扩展名（不含小数点）
 * @param path 文件路径/文件名
 * @return 扩展名字符串（如"txt"、"bin"，无扩展名返回空）
 */
std::string SdService::getFileExt(const std::string& path) {
    // 查找最后一个小数点的位置
    size_t pos = path.find_last_of('.');
    // 存在小数点且不是最后一个字符时，返回小数点后内容
    return (pos != std::string::npos && pos < path.length() - 1) ? path.substr(pos + 1) : "";
}

/**
 * @brief 获取指定路径的父目录路径
 * @param path 文件/目录路径
 * @return 父目录路径（如"/data/test.txt"→"/data"，根目录返回"/"）
 */
std::string SdService::getParentDirectory(const std::string& path) {
    // 查找最后一个斜杠的位置
    size_t pos = path.find_last_of('/');
    // 存在斜杠且不是第一个字符时，返回斜杠前内容；否则返回根目录
    return (pos != std::string::npos && pos > 0) ? path.substr(0, pos) : "/";
}

/**
 * @brief 列出目录条目（带缓存，避免重复遍历）
 * @param path 目录路径
 * @return 目录条目列表（优先返回缓存，无缓存则调用listElements并缓存）
 * @note 缓存最多保存50个目录，条目数>4时才缓存，避免小目录占用内存
 */
std::vector<std::string> SdService::listElementsCached(const std::string& path) {
    // 缓存中存在该目录则直接返回
    if (cachedDirectoryElements.find(path) != cachedDirectoryElements.end()) {
        return cachedDirectoryElements[path];
    }

    // 无缓存则读取目录条目
    std::vector<std::string> elements = listElements(path);
    // 条目数>4时才缓存（小目录无需缓存）
    if (elements.size() > 4) {
        // 缓存数量达到50时，删除最旧的缓存
        if (cachedDirectoryElements.size() >= 50) {
            cachedDirectoryElements.erase(cachedDirectoryElements.begin());
        }
        cachedDirectoryElements[path] = elements; // 存入缓存
    }
    return elements;
}

/**
 * @brief 设置指定目录的缓存条目列表
 * @param path 目录路径
 * @param elements 目录条目列表
 */
void SdService::setCachedDirectoryElements(const std::string& path, const std::vector<std::string>& elements) {
    cachedDirectoryElements[path] = elements;
}

/**
 * @brief 删除指定目录的缓存条目
 * @param path 目录路径
 */
void SdService::removeCachedPath(const std::string& path) {
    cachedDirectoryElements.erase(path);
}

/**
 * @brief 获取文件名（不含路径和扩展名）
 * @param path 文件路径（如"/data/test.txt"）
 * @return 纯文件名（如"test"）
 */
std::string SdService::getFileName(const std::string& path) {
    // 查找最后一个斜杠（路径分隔符）
    size_t lastSlash = path.find_last_of('/');
    // 查找最后一个小数点（扩展名分隔符）
    size_t lastDot = path.find_last_of('.');
    // 无扩展名或扩展名在路径分隔符前，将lastDot设为字符串长度
    if (lastDot == std::string::npos || lastDot < lastSlash) {
        lastDot = path.length();
    }
    // 文件名起始位置：斜杠后一位（无斜杠则从0开始）
    size_t start = (lastSlash != std::string::npos) ? lastSlash + 1 : 0;
    // 截取文件名（起始位置到扩展名前）
    return path.substr(start, lastDot - start);
}

/**
 * @brief 确保指定目录存在（不存在则创建）
 * @param directory 目录路径
 * @return 目录存在/创建成功返回true，失败返回false
 */
bool SdService::ensureDirectory(const std::string& directory) {
    if (!sdCardMounted) {
        return false;
    }

    // 目录不存在则创建
    if (!SD.exists(directory.c_str())) {
        return SD.mkdir(directory.c_str());
    }
    return true; // 目录已存在
}

/**
 * @brief 以只读模式打开文件
 * @param path 文件路径
 * @return File对象（未挂载/文件不存在返回空File）
 */
File SdService::openFileRead(const std::string& path) {
    if (!sdCardMounted) return File();
    return SD.open(path.c_str(), FILE_READ);
}

/**
 * @brief 以写入（覆盖）模式打开文件
 * @param path 文件路径
 * @return File对象（未挂载/打开失败返回空File）
 */
File SdService::openFileWrite(const std::string& path) {
    if (!sdCardMounted) return File();
    return SD.open(path.c_str(), FILE_WRITE);
}

/**
 * @brief 删除多级目录（递归删除目录内所有文件/子目录）
 * @param dirPath 待删除的目录路径
 * @return 删除成功返回true，失败返回false
 * @note 先递归删除子目录/文件，再删除空目录
 */
bool SdService::deleteDirectory(const std::string& dirPath) {
    if (!sdCardMounted) return false;
    // 打开目标目录
    File dir = SD.open(dirPath.c_str());
    if (!dir || !dir.isDirectory()) return false;

    // 遍历目录内所有条目
    File entry = dir.openNextFile();
    while (entry) {
        // 拼接条目完整路径
        std::string entryPath = std::string(dirPath) + "/" + entry.name();
        if (entry.isDirectory()) {
            deleteDirectory(entryPath); // 递归删除子目录
        } else {
            SD.remove(entryPath.c_str()); // 删除文件
        }
        entry = dir.openNextFile(); // 下一个条目
    }

    dir.close(); // 关闭目录句柄
    return SD.rmdir(dirPath.c_str()); // 删除空目录
}