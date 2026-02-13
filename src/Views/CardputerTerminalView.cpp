#ifdef DEVICE_CARDPUTER

#include "CardputerTerminalView.h"

void CardputerTerminalView::initialize() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(BACKGROUND_COLOR);
    M5Cardputer.Display.setTextColor(TEXT_COLOR);
    M5Cardputer.Display.setTextWrap(false);
    M5Cardputer.Display.setTextSize(1);

    scrW = M5Cardputer.Display.width();
    scrH = M5Cardputer.Display.height();

    // 精灵 1 bpp (240x135 占用 4 KB)
    termSprite.setColorDepth(1); // 1 位/像素
    spriteReady = termSprite.createSprite(scrW, scrH);  
    if (spriteReady) {
        termSprite.setPaletteColor(0, BACKGROUND_COLOR);
        termSprite.setPaletteColor(1, TEXT_COLOR);
        termSprite.setTextWrap(false);
        termSprite.setTextSize(1);
    }

    originX = 0;
    originY = 0;

    recomputeMetrics();
    termReset();
    renderAll();
}

void CardputerTerminalView::welcome(TerminalTypeEnum& /*terminalType*/, std::string& terminalInfos) {
    clear();
    println(" ____       ____  ");
    println("| __ )     |  _ \\");
    println("|  _ \\     | |_) |");
    println("| |_) |    |  __/ ");
    println("|____/ US  |_| IRATE ");
    println("使用方向键滚动"); // 汉化
    println("输入 'mode' 开始"); // 汉化
}

void CardputerTerminalView::print(const std::string& text) {
    if (text.empty()) { maybeRender(); return; }
    
    bool sawScroll = false;    
    auto decoded = htmlDecodeBasic(text);

    std::string filtered;
    filtered.reserve(decoded.size());
    for (unsigned char b : decoded) {
        if (b == (unsigned char)CARDPUTER_SPECIAL_ARROW_UP) {
            int maxScroll = (int)history.size();
            if (scrollOffset < maxScroll) scrollOffset++;
            sawScroll = true;
            continue;
        }
        if (b == (unsigned char)CARDPUTER_SPECIAL_ARROW_DOWN) {
            if (scrollOffset > 0) scrollOffset--;
            sawScroll = true;
            continue;
        }

        // 普通文本，重置滚动偏移
        if (scrollOffset > 0) { 
            scrollOffset = 0; 
            sawScroll = true;
        }

        filtered.push_back((char)b);
    }

    // 发送到解析器
    for (unsigned char b : filtered) feedFilteredByte(b);

    dirty = true;
    char last = filtered.empty() ? '\0' : filtered.back();
    if (sawScroll || instantRender || last=='\n' || last=='\r' || last==' ') {
        renderAll();
        lastRenderMs  = millis();
        dirty         = false;
        instantRender = false;
    } else {
        maybeRender();  // 节流
    }
}

void CardputerTerminalView::print(const uint8_t data) {
    feedFilteredByte(data);
    // 如果是换行、回车、空格或强制立即渲染，则立即渲染
    if (data=='\n' || data=='\r' || data== ' ' || instantRender) {
        renderAll(); lastRenderMs = millis(); 
        dirty=false; 
        instantRender=false; 
    }
    else { dirty = true; maybeRender(); }
}

void CardputerTerminalView::println(const std::string& text) {
    auto decoded = htmlDecodeBasic(text);
    feedFilteredBytes((const uint8_t*)decoded.data(), decoded.size());
    feedFilteredByte('\n');
    renderAll();
    lastRenderMs = millis();
    dirty = false;
}

void CardputerTerminalView::printPrompt(const std::string& mode) {
    instantRender = true;
    print(mode + "> ");
}

void CardputerTerminalView::waitPress() {
    print("\n按任意键继续...\n"); // 汉化
}

void CardputerTerminalView::clear() {
    if (spriteReady) termSprite.fillScreen(0);
    M5Cardputer.Display.fillScreen(BACKGROUND_COLOR);
    u8_cp = 0; u8_rem = 0;
    history.clear();
    scrollOffset = 0;
    termReset();
    renderAll();
}

// --------------------- 终端核心 ---------------------

void CardputerTerminalView::termReset() {
    lines.assign(rows, std::string(cols, ' '));
    curRow = 0;
    curCol = 0;
    ansiReset();
}

void CardputerTerminalView::termPutChar(char c) {
    if (c >= ' ') {
        lines[curRow][curCol] = c; // 覆盖
        curCol++;
        if (curCol >= cols) {
            curCol = 0;
            curRow++;
            if (curRow >= rows) {
                termScrollUp();
                curRow = rows - 1;
            }
        }
    }
}

void CardputerTerminalView::termPutText(const char* s, size_t n) {
    for (size_t i = 0; i < n; ++i) termPutChar(s[i]);
}

void CardputerTerminalView::termNewLine() {
    curCol = 0;
    curRow++;
    if (curRow >= rows) {
        termScrollUp();
        curRow = rows - 1;
    }
}

void CardputerTerminalView::termCarriageReturn() {
    curCol = 0;
}

void CardputerTerminalView::termBackspace() {
    if (curCol > 0) {
        curCol--;
        lines[curRow][curCol] = ' ';
    }
}

void CardputerTerminalView::termScrollUp() {
    // 保存到历史
    history.push_back(std::move(lines[0]));
    if ((int)history.size() > historyMax) history.pop_front();

    // 滚动
    for (int r = 1; r < rows; ++r) lines[r - 1] = std::move(lines[r]);
    lines[rows - 1].assign(cols, ' ');

    if (scrollOffset > 0) scrollOffset++;
}

void CardputerTerminalView::termEraseInLine(int mode) {
    if (mode == 0) { // 光标 -> 行尾
        for (int c = curCol; c < cols; ++c) lines[curRow][c] = ' ';
    } else if (mode == 1) { // 行首 -> 光标
        for (int c = 0; c <= curCol; ++c) lines[curRow][c] = ' ';
    } else if (mode == 2) { // 整行
        lines[curRow].assign(cols, ' ');
    }
}

void CardputerTerminalView::termEraseInDisplay(int mode) {
    if (mode == 2) { // 全部
        for (int r = 0; r < rows; ++r) lines[r].assign(cols, ' ');
        curRow = 0; curCol = 0;
        return;
    }
    if (mode == 0) { // 光标 -> 结尾
        termEraseInLine(0);
        for (int r = curRow + 1; r < rows; ++r) lines[r].assign(cols, ' ');
    } else if (mode == 1) { // 开头 -> 光标
        for (int r = 0; r < curRow; ++r) lines[r].assign(cols, ' ');
        termEraseInLine(1);
    }
}

void CardputerTerminalView::termMoveCursorRel(int dx, int dy) {
    curRow += dy;
    curCol += dx;
    if (curRow < 0) curRow = 0;
    if (curRow >= rows) curRow = rows - 1;
    if (curCol < 0) curCol = 0;
    if (curCol >= cols) curCol = cols - 1;
}

void CardputerTerminalView::termMoveCursorAbs(int row1, int col1) {
    int r = row1 - 1;
    int c = col1 - 1;
    if (r < 0) r = 0; if (r >= rows) r = rows - 1;
    if (c < 0) c = 0; if (c >= cols) c = cols - 1;
    curRow = r;
    curCol = c;
}

// --------------------- ANSI 解析器 ---------------------

void CardputerTerminalView::ansiReset() {
    inEsc = false;
    inCSI = false;
    csiParams.clear();
    csiParamAcc = -1;
}

void CardputerTerminalView::ansiFinalizeCSI(char final) {
    auto get = [&](size_t i, int def1) -> int {
        if (i < csiParams.size() && csiParams[i] >= 0) return csiParams[i] == 0 && def1 ? 1 : csiParams[i];
        return def1 ? 1 : 0;
    };

    switch (final) {
        case 'A': termMoveCursorRel(0, -get(0, 1)); break; // 上
        case 'B': termMoveCursorRel(0,  get(0, 1)); break; // 下
        case 'C': termMoveCursorRel( get(0, 1), 0); break; // 右
        case 'D': termMoveCursorRel(-get(0, 1), 0); break; // 左
        case 'K': { int mode = get(0, 0); termEraseInLine(mode); } break;
        case 'J': { int mode = get(0, 0); termEraseInDisplay(mode); } break;
        case 'H':
        case 'f': { int r = get(0, 1); int c = get(1, 1); termMoveCursorAbs(r, c); } break;
        default: break;
    }
}

void CardputerTerminalView::ansiFeed(char c) {
    if (!inEsc) {
        switch (c) {
            case '\r': termCarriageReturn(); return;
            case '\n': termNewLine(); return;
            case '\b': termBackspace(); return;
            case '\t': { int spaces = 4 - (curCol % 4); while (spaces--) termPutChar(' '); return; }
            case 0x1B: inEsc = true; return; // ESC
            default:
                if (c >= ' ') termPutChar(c);
                padBeforeErase = (c == ' ');
                return;
        }
    } else {
        if (!inCSI) {
            if (c == '[') {
                inCSI = true; csiParams.clear(); csiParamAcc = -1; return;
            } else {
                ansiReset(); return; // 未处理的 ESC X
            }
        } else {
            if (c >= '0' && c <= '9') {
                int d = c - '0';
                if (csiParamAcc < 0) csiParamAcc = d;
                else csiParamAcc = csiParamAcc * 10 + d;
                return;
            } else if (c == ';') {
                csiParams.push_back(csiParamAcc < 0 ? 0 : csiParamAcc);
                csiParamAcc = -1;
                return;
            } else {
                padBeforeErase = false;
                csiParams.push_back(csiParamAcc < 0 ? 0 : csiParamAcc);
                ansiFinalizeCSI(c);
                ansiReset();
                return;
            }
        }
    }
}

// --------------------- 渲染 ---------------------

void CardputerTerminalView::drawLine(const std::string& s, int16_t y, bool keepTrailingSpaces) {
    size_t end = s.size();

    if (!keepTrailingSpaces) {
        while (end > 0 && s[end - 1] == ' ') --end;
    }

    if (spriteReady) {
        termSprite.setCursor(originX, y);
        termSprite.printf("%.*s", (int)end, s.c_str());
    } else {
        M5Cardputer.Display.setCursor(originX, y);
        M5Cardputer.Display.printf("%.*s", (int)end, s.c_str());
    }
}

void CardputerTerminalView::renderAll() {
    if (spriteReady) {
        termSprite.fillScreen(0);
        termSprite.setTextColor(1);
        termSprite.setTextSize(1);
    } else {
        M5Cardputer.Display.fillScreen(BACKGROUND_COLOR);
        M5Cardputer.Display.setTextColor(TEXT_COLOR);
        M5Cardputer.Display.setTextSize(1);
    }

    const int H = (int)history.size();
    if (scrollOffset > H) scrollOffset = H;

    // H (历史) + rows (缓冲区)
    const int total   = H + rows;
    int endIdx        = total - 1 - scrollOffset;   // 虚拟索引
    if (endIdx < 0) endIdx = 0;
    int startIdx      = endIdx - (rows - 1);
    if (startIdx < 0) startIdx = 0;

    // 渲染行
    int16_t y = originY;
    for (int i = 0; i < rows; ++i) {
        int idx = startIdx + i;

        bool isActiveBufferLine =
            (scrollOffset == 0) && (idx >= H) && ((idx - H) == curRow);

        if (idx < H) {
            // 历史行 -> 修剪尾部空格
            drawLine(history[idx], y, /*keepTrailingSpaces=*/false);
        } else {
            int li = idx - H;
            if (li >= 0 && li < rows) {
                // 当前缓冲区行，保留尾部空格
                drawLine(lines[li], y, /*keepTrailingSpaces=*/isActiveBufferLine);
            }
        }
        y += charH;
    }

    if (scrollOffset == 0) {
        int16_t cx = originX + curCol * charW;
        int16_t cy = originY + curRow * charH + charH - 2;
        if (spriteReady) termSprite.fillRect(cx, cy, charW, 2, 1);
        else M5Cardputer.Display.fillRect(cx, cy, charW, 2, TEXT_COLOR);
    }

    if (spriteReady) termSprite.pushSprite(0, 0);
}

void CardputerTerminalView::maybeRender() {
    if (!dirty) return;
    uint32_t now = millis();
    if (now - lastRenderMs >= frameIntervalMs) {
        renderAll();
        lastRenderMs = now;
        dirty = false;
    }
}

void CardputerTerminalView::recomputeMetrics() {
    // 字符尺寸
    charW = 6;
    charH = 12;

    // 可用区域
    int16_t usableW = scrW - 2 * padX;
    int16_t usableH = scrH - 2 * padY;
    if (usableW < charW) usableW = charW;
    if (usableH < charH) usableH = charH;

    // 网格
    cols = usableW / charW; if (cols > 80) cols = 80; if (cols < 20) cols = 20;
    rows = usableH / charH; if (rows < 4) rows = 4;

    // 剩余空间
    int16_t leftover = usableH - rows * charH;

    // 原点
    originX = padX;
    originY = padY + leftover / 2;

    // 重置缓冲区
    lines.assign(rows, std::string(cols, ' '));
    curRow = 0; curCol = 0;
}

// --------------------- UTF-8 / HTML 过滤 ---------------------

void CardputerTerminalView::feedFilteredBytes(const uint8_t* data, size_t n) {
    for (size_t i = 0; i < n; ++i) feedFilteredByte(data[i]);
}

void CardputerTerminalView::feedFilteredByte(uint8_t b) {
    if (u8_rem == 0) {
        if (b < 0x80) {
            // ASCII 直接 → ANSI
            ansiFeed(static_cast<char>(b));
        } else if ((b & 0xE0) == 0xC0) {
            u8_cp = (b & 0x1F);
            u8_rem = 1;
        } else if ((b & 0xF0) == 0xE0) {
            u8_cp = (b & 0x0F);
            u8_rem = 2;
        } else if ((b & 0xF8) == 0xF0) {
            u8_cp = (b & 0x07);
            u8_rem = 3;
        }
    } else {
        if ((b & 0xC0) == 0x80) { // 后续字节
            u8_cp = (u8_cp << 6) | (b & 0x3F);
            if (--u8_rem == 0) {
                emitCodepoint(u8_cp);
                u8_cp = 0;
            }
        } else {
            u8_cp = 0; u8_rem = 0;
            feedFilteredByte(b);
        }
    }
}

void CardputerTerminalView::emitCodepoint(uint32_t cp) {
    std::string repl = mapCodepointToASCII(cp);
    for (char ch : repl) ansiFeed(ch);
}

std::string CardputerTerminalView::mapCodepointToASCII(uint32_t cp) const {
    // ASCII 直接
    if (cp <= 0x7E) {
        if (cp >= 0x20) return std::string(1, static_cast<char>(cp));
        return "";
    }

    // 空格
    if (cp == 0x00A0) return " "; // nbsp

    // 引号
    if (cp == 0x2018 || cp == 0x2019) return "'";  // ‘ ’
    if (cp == 0x201C || cp == 0x201D) return "\""; // “ ”

    // 破折号
    if (cp == 0x2013 || cp == 0x2014 || cp == 0x2212) return "-"; // – — −

    // 省略号/项目符号
    if (cp == 0x2026) return "...";  // …
    if (cp == 0x2022) return "*";    // •

    // 箭头
    if (cp == 0x2190) return "<-";
    if (cp == 0x2192) return "->";
    if (cp == 0x2191) return "^";
    if (cp == 0x2193) return "v";

    // 杂项
    if (cp == 0x2713 || cp == 0x2705) return "v";  // ✓ / ✔
    if (cp == 0x2717 || cp == 0x274C) return "x"; // ❌
    if (cp == 0x00B0) return " deg ";   // °
    if (cp == 0x03BC) return "u";       // µ

    return "";
}

std::string CardputerTerminalView::htmlDecodeBasic(const std::string& s) const {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '&') {
            size_t j = s.find(';', i + 1);
            if (j != std::string::npos && j - i <= 10) {
                std::string ent = s.substr(i + 1, j - (i + 1)); // 不含 & ;
                if (ent == "amp")  { out.push_back('&'); i = j; continue; }
                if (ent == "lt")   { out.push_back('<'); i = j; continue; }
                if (ent == "gt")   { out.push_back('>'); i = j; continue; }
                if (ent == "quot") { out.push_back('"'); i = j; continue; }
                if (ent == "apos") { out.push_back('\''); i = j; continue; }
                if (ent == "nbsp") { out.push_back(' '); i = j; continue; }

                // 数字实体   
                if (!ent.empty() && ent[0] == '#') {
                    uint32_t cp = 0;
                    bool ok = false;
                    if (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) {
                        // 十六进制
                        try {
                            cp = static_cast<uint32_t>(std::stoul(ent.substr(2), nullptr, 16));
                            ok = true;
                        } catch (...) { ok = false; }
                    } else {
                        // 十进制
                        try {
                            cp = static_cast<uint32_t>(std::stoul(ent.substr(1), nullptr, 10));
                            ok = true;
                        } catch (...) { ok = false; }
                    }
                    if (ok) {
                        auto repl = mapCodepointToASCII(cp);
                        out += repl;
                        i = j;
                        continue;
                    }
                }
                // 未知实体
            }
        }
        out.push_back(ch);
    }
    return out;
}

#endif