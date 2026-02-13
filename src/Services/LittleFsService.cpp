#include "LittleFsService.h"

LittleFsService::~LittleFsService() {
    end(); // 析构时释放文件系统资源
}

bool LittleFsService::begin(bool formatIfFail, bool readOnly) {
    // 已挂载则更新只读状态并返回成功
    if (_mounted) {
        _readOnly = readOnly;
        return true;
    }

    // 挂载LittleFS文件系统（挂载失败时可选格式化）
    _mounted = LittleFS.begin(formatIfFail);
    _readOnly = readOnly;
    return _mounted;
}

void LittleFsService::end() {
    // 卸载文件系统并重置挂载状态
    if (_mounted) {
        LittleFS.end();
        _mounted = false;
    }
}

bool LittleFsService::normalizeUserPath(const std::string& in, std::string& out, bool dir) {
    // 规范化用户输入路径：确保绝对路径、过滤../、合并连续/
    std::string p = in;
    // 补充根目录前缀，确保路径为绝对路径
    if (p.empty() || p[0] != '/') p = "/" + p;
    // 禁止路径包含../（防止目录遍历攻击）
    if (p.find("..") != std::string::npos) return false;

    // 将连续的//合并为单个/
    std::string q; q.reserve(p.size());
    char prev = 0;
    for (char c : p) {
        if (c=='/' && prev=='/') continue;
        q.push_back(c);
        prev = c;
    }
    out.swap(q);
    // 确保目录路径以/结尾，文件路径不以/结尾
    ensureDirSlashes(out, dir);
    return true;
}

void LittleFsService::ensureDirSlashes(std::string& p, bool dir) {
    // 调整路径末尾的/：目录路径保留/，文件路径去除/
    if (dir) {
        // 非根目录的路径末尾补充/
        if (p.size() > 1 && p.back() != '/') p.push_back('/');
        // 根目录"/"保持不变
    } else {
        // 移除文件路径末尾多余的/
        while (p.size() > 1 && p.back() == '/') p.pop_back();
    }
}

bool LittleFsService::exists(const std::string& userPath) const {
    // 检查文件/目录是否存在
    if (!_mounted) return false;
    return LittleFS.exists(userPath.c_str());
}

bool LittleFsService::isDir(const std::string& userPath) const {
    // 检查指定路径是否为目录
    if (!_mounted) return false;

    // 处理根目录特殊情况
    if (userPath.empty() || userPath == "/") {
        fs::File root = LittleFS.open("/");
        return (bool)root && root.isDirectory();
    }

    fs::File dir = LittleFS.open(userPath.c_str());
    return (bool)dir && dir.isDirectory();
}

std::vector<LittleFsService::Entry> LittleFsService::list(const std::string& userDir) const {
    // 列出指定目录下的所有文件/目录项
    std::vector<Entry> out;
    if (!_mounted) return out;

    fs::File dir = LittleFS.open(userDir.c_str());
    if (!dir || !dir.isDirectory()) return out;

    // 遍历目录中的所有文件/子目录
    for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        std::string full = f.name();
        std::string name = full;

        // 提取相对目录的文件名（去除父目录前缀）
        if (full.rfind(userDir, 0) == 0) {
            name = full.substr(userDir.size());
            if (!name.empty() && name[0] == '/') name.erase(0, 1);
        } else if (full.size() && full[0]=='/') {
            // 处理根目录下的文件，去除开头的/
            name = full.substr(1);
        }

        // 添加目录项信息（名称、大小、是否为目录）
        out.push_back(Entry{
            /*name=*/name,
            /*size=*/static_cast<size_t>(f.size()),
            /*isDir=*/f.isDirectory()
        });
        f.close();
    }
    dir.close();
    return out;
}

size_t LittleFsService::getFileSize(const std::string& userPath) const {
    // 获取文件大小（字节）
    if (!_mounted) return 0;

    std::string p;
    // 路径规范化失败则返回0
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return 0;

    fs::File f = LittleFS.open(p.c_str(), "r");
    if (!f) return 0;

    size_t size = f.size();
    f.close();
    return size;
}

bool LittleFsService::readAll(const std::string& userPath, std::string& out) const {
    // 读取文件全部内容到字符串
    if (!_mounted) return false;
    std::string p;
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return false;

    fs::File f = LittleFS.open(p.c_str(), "r");
    if (!f) return false;

    out.clear();
    out.reserve(f.size()); // 预分配内存提升效率
    std::vector<uint8_t> buf(2048); // 2KB读取缓冲区
    while (true) {
        int n = f.read(buf.data(), buf.size());
        if (n < 0) { f.close(); return false; } // 读取错误
        if (n == 0) break; // 读取完成
        out.append(reinterpret_cast<const char*>(buf.data()), n);
    }
    f.close();
    return true;
}

bool LittleFsService::readChunks(const std::string& userPath,
                                 const std::function<bool(const uint8_t*, size_t)>& writer) const {
    // 分块读取文件内容，通过回调函数处理每块数据
    if (!_mounted) return false;
    std::string p;
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return false;

    fs::File f = LittleFS.open(p.c_str(), "r");
    if (!f) return false;

    uint8_t buf[4096]; // 4KB读取缓冲区
    bool ok = true;
    while (true) {
        int n = f.read(buf, sizeof(buf));
        if (n < 0) { ok = false; break; } // 读取错误
        if (n == 0) break; // 读取完成
        // 回调处理当前块数据，回调返回false则终止读取
        if (!writer(buf, (size_t)n)) { ok = false; break; }
    }
    f.close();
    return ok;
}

bool LittleFsService::ensureParentDirs(const std::string& userFilePath) const {
    // 确保文件的父目录存在（不存在则创建）
    auto pos = userFilePath.find_last_of('/');
    // 无父目录（根目录）则直接返回成功
    if (pos == std::string::npos || pos == 0) return true;
    const std::string dir = userFilePath.substr(0, pos);
    return mkdirRecursive(dir);
}

bool LittleFsService::write(const std::string& userPath, const std::string& data, bool append) {
    // 重载：字符串数据写入文件（支持追加模式）
    return write(userPath, reinterpret_cast<const uint8_t*>(data.data()), data.size(), append);
}

bool LittleFsService::write(const std::string& userPath, const uint8_t* data, size_t len, bool append) {
    // 二进制数据写入文件（支持追加/覆盖模式）
    if (!_mounted || _readOnly) return false;

    // 确保父目录存在
    if (!ensureParentDirs(userPath)) return false;

    const char* mode = append ? "a" : "w";
    // 打开文件（覆盖模式时创建新文件，追加模式时保留原有文件）
    fs::File f = LittleFS.open(userPath.c_str(), mode, append ? false : true);
    if (!f) return false;

    // 分块写入数据（4KB每块）
    size_t off = 0;
    const size_t CHUNK = 4096;
    bool ok = true;
    while (off < len) {
        size_t n = (len - off < CHUNK) ? (len - off) : CHUNK;
        size_t w = f.write(data + off, n);
        // 写入字节数不匹配则判定为失败
        if (w != n) { ok = false; break; }
        off += n;
    }
    f.close();
    return ok;
}

bool LittleFsService::mkdirRecursive(const std::string& userDir) const {
    // 递归创建目录（支持多级目录）
    if (!_mounted || _readOnly) return false;

    std::string d;
    if (!normalizeUserPath(userDir, d, /*dir=*/true)) return false;

    size_t start = 1; // 跳过开头的/
    while (start < d.size()) {
        size_t next = d.find('/', start);
        if (next == std::string::npos) next = d.size();
        std::string cur = d.substr(0, next);
        // 目录不存在则创建
        if (cur.size() > 1 && !LittleFS.exists(cur.c_str())) {
            if (!LittleFS.mkdir(cur.c_str())) return false;
        }
        start = next + 1;
    }
    return true;
}

bool LittleFsService::removeFile(const std::string& userPath) {
    // 删除指定文件
    if (!_mounted || _readOnly) return false;
    std::string p;
    if (!normalizeUserPath(userPath, p, /*dir=*/false)) return false;
    return LittleFS.remove(p.c_str());
}

bool LittleFsService::rmdirRecursiveImpl(const std::string& userDir) {
    // 递归删除目录实现（先删子文件/子目录，再删当前目录）
    fs::File dir = LittleFS.open(userDir.c_str());
    if (!dir || !dir.isDirectory()) return false;

    // 遍历并删除目录内所有内容
    for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        std::string child = f.name();
        if (f.isDirectory()) {
            f.close();
            // 递归删除子目录
            if (!rmdirRecursiveImpl(child)) { dir.close(); return false; }
        } else {
            f.close();
            // 删除文件
            if (!LittleFS.remove(child.c_str())) { dir.close(); return false; }
        }
    }
    dir.close();
    // 删除空目录
    return LittleFS.rmdir(userDir.c_str());
}

bool LittleFsService::rmdirRecursive(const std::string& userDir) {
    // 递归删除目录（禁止删除根目录）
    if (!_mounted || _readOnly) return false;
    std::string d;
    if (!normalizeUserPath(userDir, d, /*dir=*/true)) return false;

    // 保护根目录，禁止删除/
    if (d == "/") return false;

    return rmdirRecursiveImpl(d);
}

bool LittleFsService::renamePath(const std::string& fromUserPath, const std::string& toUserPath) {
    // 重命名文件/目录
    if (!_mounted || _readOnly) return false;
    std::string a, b;
    if (!normalizeUserPath(fromUserPath, a, /*dir=*/false)) return false;
    if (!normalizeUserPath(toUserPath,   b, /*dir=*/false)) return false;

    // 确保目标路径的父目录存在
    if (!ensureParentDirs(b)) return false;

    return LittleFS.rename(a.c_str(), b.c_str());
}

bool LittleFsService::getSpace(size_t& total, size_t& used) const {
    // 获取文件系统总空间和已使用空间
    if (!_mounted) return false;
    total = LittleFS.totalBytes();
    used  = LittleFS.usedBytes();
    return true;
}

size_t LittleFsService::freeBytes() const {
    // 计算文件系统剩余可用空间
    size_t total=0, used=0;
    if (!getSpace(total, used)) return 0;
    return (total > used) ? (total - used) : 0;
}

bool LittleFsService::format() {
    // 格式化LittleFS文件系统
    if (_mounted) LittleFS.end(); // 先卸载
    bool ok = LittleFS.format();
    // 格式化后重新挂载
    _mounted = LittleFS.begin(true, _basePath.c_str(), 10, _partitionLabel.c_str());
    return ok && _mounted;
}

const char* LittleFsService::mimeFromPath(const char* path) {
    // 根据文件扩展名获取MIME类型
    if (!path) return "application/octet-stream";
    const char* dot = std::strrchr(path, '.');
    // 无扩展名则返回二进制流类型
    if (!dot) return "application/octet-stream";
    // 匹配常见文件扩展名的MIME类型
    if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm")) return "text/html";
    if (!strcasecmp(dot, ".css"))  return "text/css";
    if (!strcasecmp(dot, ".js"))   return "application/javascript";
    if (!strcasecmp(dot, ".json")) return "application/json";
    if (!strcasecmp(dot, ".png"))  return "image/png";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".gif"))  return "image/gif";
    if (!strcasecmp(dot, ".svg"))  return "image/svg+xml";
    if (!strcasecmp(dot, ".ico"))  return "image/x-icon";
    if (!strcasecmp(dot, ".txt"))  return "text/plain";
    if (!strcasecmp(dot, ".wasm")) return "application/wasm";
    // 默认返回二进制流类型
    return "application/octet-stream";
}

bool LittleFsService::isSafeRootFileName(const std::string& name) const {
    // 检查文件名是否符合安全规范（防止路径遍历/非法字符）
    if (name.empty()) return false;
    if (name.find('\0') != std::string::npos) return false; // 禁止空字符
    if (name.find("..") != std::string::npos) return false; // 禁止上级目录
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false; // 禁止路径分隔符
    return true;
}

std::vector<std::string> LittleFsService::listFiles(const std::string& userDir, const std::string& extension) const {
    // 列出指定目录下指定扩展名的所有文件
    std::vector<std::string> files;
    if (!_mounted) return files;

    fs::File dir = LittleFS.open(userDir.c_str());
    if (!dir || !dir.isDirectory()) return files;

    // 遍历目录中的文件
    for (fs::File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (!f.isDirectory()) {
            std::string name = f.name();
            // 去除路径开头的/
            if (!name.empty() && name[0] == '/') name.erase(0, 1);

            // 匹配文件扩展名（忽略大小写）
            auto pos = name.rfind('.');
            if (pos != std::string::npos) {
                std::string ext = name.substr(pos);
                // 扩展名转小写
                for (auto& c : ext) c = static_cast<char>(tolower(c));
                if (ext == extension) {
                    files.push_back(name);
                }
            }
        }
        f.close();
    }
    dir.close();

    return files;
}