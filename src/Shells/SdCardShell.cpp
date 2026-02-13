#include "SdCardShell.h"

SdCardShell::SdCardShell(SdService& sdService, ITerminalView& view, IInput& input, ArgTransformer& argTransformer, UserInputManager& userInputManager)
    : sd(sdService), terminalView(view), terminalInput(input), currentDir("/"), argTransformer(argTransformer), userInputManager(userInputManager) {}

void SdCardShell::run() {
    terminalView.println("- SD 命令行：输入 'help' 查看命令。输入 'exit' 退出。"); //汉化

    while (true) {
        terminalView.print(currentDir + " $ "); //汉化（保持提示符不变，但这是用户界面，可汉化？通常保留 $ 符号。我们只改文字部分，但这里 $ 是提示符，不汉化。所以保留）
        std::string input = userInputManager.getLine();

        if (input.empty()) continue;
        if (input == "exit") break;

        executeCommand(input);
    }

    terminalView.println("- 正在退出 SD 命令行。\n"); //汉化
}

void SdCardShell::executeCommand(const std::string& input) {
    std::istringstream iss(input);
    std::string cmd;
    iss >> cmd;

    if (cmd == "ls") cmdLs();
    else if (cmd == "cd") cmdCd(iss);
    else if (cmd == "mkdir") cmdMkdir(iss);
    else if (cmd == "touch") cmdTouch(iss);
    else if (cmd == "rm") cmdRm(iss);
    else if (cmd == "cat") cmdCat(iss);
    else if (cmd == "echo") cmdEcho(iss);
    else if (cmd == "help") cmdHelp();
    else terminalView.println("未知命令：" + cmd); //汉化
}

void SdCardShell::cmdLs() {
    auto files = sd.listElementsCached(currentDir);

    for (const auto& f : files) {
        std::string fullPath = currentDir;
        if (fullPath.back() != '/') fullPath += '/';
        fullPath += f;

        if (sd.isDirectory(fullPath)) {
            terminalView.println(" 📁 " + f);
        } else {
            std::string ext = sd.getFileExt(f);
            std::string icon;

            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == "txt" || ext == "md" || ext == "log" || ext == "csv" || ext == "pdf") {
                icon = " 📝";
            } else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif" || ext == "webp") {
                icon = " 🖼️ ";
            } else if (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "flac" || ext == "m4a") {
                icon = " 🎵";
            } else if (ext == "mp4" || ext == "avi" || ext == "mov" || ext == "mkv" || ext == "webm") {
                icon = " 🎞️ ";
            } else if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz") {
                icon = " 📦";
            } else if (ext == "ino" || ext == "cpp" || ext == "c" || ext == "h" ||
                       ext == "py" || ext == "js" || ext == "ts" || ext == "html" ||
                       ext == "css" || ext == "json" || ext == "xml" || ext == "sh") {
                icon = " 💻";
            } else if (ext == "bin") {
                icon = " 🧾";
            } else {
                icon = " 📄"; // Default
            }

            terminalView.println(icon + " " + f);
        }
    }
}

void SdCardShell::cmdCd(std::istringstream& iss) {
    std::string arg;
    iss >> arg;

    if (arg.empty()) {
        currentDir = "/";
        return;
    }

    std::string newPath;

    if (arg[0] == '/') {
        newPath = normalizePath(arg);
    } else {
        newPath = resolveRelativePath(currentDir, arg); //  ../.., images/ etc.
    }

    if (sd.isDirectory(newPath)) {
        currentDir = newPath;
    } else {
        terminalView.println("目录未找到：" + newPath); //汉化
    }
}

void SdCardShell::cmdMkdir(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        terminalView.println("用法：mkdir <目录名>"); //汉化
        return;
    }
    std::string fullPath = currentDir + "/" + name;
    if (sd.ensureDirectory(fullPath)) {
        terminalView.println("目录已创建：" + name); //汉化
        sd.removeCachedPath(currentDir); // 已更改，清除缓存以重新加载 //汉化
    }
    else terminalView.println("创建目录失败。"); //汉化
}

void SdCardShell::cmdTouch(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        terminalView.println("用法：touch <文件名>"); //汉化
        return;
    }
    std::string fullPath = currentDir + "/" + name;
    if (sd.writeFile(fullPath, "")) {
        terminalView.println("文件已创建：" + name); //汉化
        sd.removeCachedPath(currentDir); // 已更改，清除缓存以重新加载 //汉化
    }
    else terminalView.println("创建文件失败。"); //汉化
}

void SdCardShell::cmdRm(std::istringstream& iss) {
    std::string name;
    iss >> name;
    if (name.empty()) {
        terminalView.println("用法：rm <文件或目录>"); //汉化
        return;
    }
    std::string fullPath = currentDir + "/" + name;
    if (sd.isFile(fullPath)) {
        if (sd.deleteFile(fullPath)) {
            terminalView.println("文件已删除。"); //汉化
            sd.removeCachedPath(currentDir); // 已更改，清除缓存以重新加载 //汉化
        }
        else terminalView.println("删除文件失败。"); //汉化

    } else if (sd.isDirectory(fullPath)) {
        if (sd.deleteDirectory(fullPath)) {
            terminalView.println("文件夹已删除。"); //汉化
            sd.removeCachedPath(currentDir); // 已更改，清除缓存以重新加载 //汉化
        }
    } else {
        terminalView.println("路径未找到。"); //汉化
    }
}

void SdCardShell::cmdHelp() {
    terminalView.println(" 可用命令："); //汉化
    terminalView.println("  ls                : 列出目录中的文件"); //汉化
    terminalView.println("  cd <目录>         : 切换目录"); //汉化
    terminalView.println("  cat <文件>        : 显示文本文件内容"); //汉化
    terminalView.println("  echo 文本 > 文件  : 用文本覆盖文件"); //汉化
    terminalView.println("  echo 文本 >> 文件 : 将文本追加到文件"); //汉化
    terminalView.println("  mkdir <目录>      : 创建新目录"); //汉化
    terminalView.println("  touch <文件>      : 创建空文件"); //汉化
    terminalView.println("  rm <文件/目录>    : 删除文件或目录"); //汉化
    terminalView.println("  help             : 显示此帮助信息"); //汉化
    terminalView.println("  exit             : 退出 SD 命令行"); //汉化
}

void SdCardShell::cmdCat(std::istringstream& iss) {
    constexpr size_t MAX_DISPLAY_CHARS = 4096;

    std::string filename;
    iss >> filename;
    if (filename.empty()) {
        terminalView.println("用法：cat <文件名>"); //汉化
        return;
    }

    std::string fullPath = currentDir;
    if (fullPath.back() != '/') fullPath += '/';
    fullPath += filename;

    if (!sd.isFile(fullPath)) {
        terminalView.println("文件未找到：" + filename); //汉化
        return;
    }

    std::string content = sd.readFileChunk(fullPath, 0, MAX_DISPLAY_CHARS);
    terminalView.println(content);
    if (content.length() == MAX_DISPLAY_CHARS) {
        terminalView.println("\n...（文件过长）"); //汉化
    }
}

void SdCardShell::cmdEcho(std::istringstream& iss) {
    std::vector<std::string> tokens;
    std::string word;

    while (iss >> word) {
        tokens.push_back(word);
    }

    if (tokens.size() < 3) {
        terminalView.println("用法：echo <文本> > <文件名>  或  >> <文件名>"); //汉化
        return;
    }

    // 查找 > 或 >> 的位置 //汉化
    size_t redirPos = tokens.size();
    std::string redir;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == ">" || tokens[i] == ">>") {
            redir = tokens[i];
            redirPos = i;
        }
    }

    if (redir.empty() || redirPos == tokens.size() - 1) {
        terminalView.println("用法：echo <文本> > <文件名>  或  >> <文件名>"); //汉化
        return;
    }

    // 文本 //汉化
    std::string text;
    for (size_t i = 0; i < redirPos; ++i) {
        if (!text.empty()) text += " ";
        text += tokens[i];
    }

    // 路径 //汉化
    std::string filename = tokens.back();
    std::string fullPath = currentDir;
    if (fullPath.back() != '/') fullPath += '/';
    fullPath += filename;

    // 解码转义字符，例如 \\n //汉化
    auto decodedText = argTransformer.decodeEscapes(text);
    
    // 写入或追加到文件 //汉化
    bool append = (redir == ">>");
    if (sd.writeFile(fullPath, decodedText, append)) {
        terminalView.println((append ? "已追加到 " : "已写入 ") + filename); //汉化
        sd.removeCachedPath(currentDir); // 已更改，清除缓存以重新加载 //汉化
    } else {
        terminalView.println("写入失败：" + filename); //汉化
    }
}

std::string SdCardShell::normalizePath(const std::string& path) {
    std::vector<std::string> parts;
    std::istringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        if (token.empty() || token == ".") continue;
        if (token == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(token);
        }
    }

    std::string result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        result += parts[i];
        if (i < parts.size() - 1) result += '/';
    }

    return result;
}

std::string SdCardShell::resolveRelativePath(const std::string& base, const std::string& arg) {
    std::string combined = base;
    if (!combined.empty() && combined.back() != '/') {
        combined += '/';
    }
    combined += arg;
    return normalizePath(combined);
}