// Text editor for the SD file menu ("Edit file"), Cardputer only.
//
// The whole file sits in one heap buffer (no String per line, so no heap
// fragmentation), capped at ED_MAX. Positions are byte offsets into it; lines
// end in "\n" or "\r\n" (kept as found). Long lines scroll sideways, and only
// the rows that changed are redrawn.
//
// Keys: characters insert (; . , / included), Fn + ; . , / move, Ctrl + ; .
// page up/down, Ctrl + , / line start/end, Fn+S save, Fn+Q or ` quit,
// Ctrl+H help.
#include "text_editor.h"
#if defined(ARDUINO_M5STACK_CARDPUTER)
#include "display.h"
#include "idf/launcher_platform.h"
#include "mykeyboard.h"
#include "settings.h"
#include <ArduinoJson.h>
#include <Keyboard_def.h>
#include <algorithm>
#include <globals.h>

#define ED_MAX 32768   // largest file the editor accepts, in bytes
#define ED_Y (LH + 2)  // text starts under the status line

namespace {
char *b;                  // the text, not NUL-terminated
size_t len, cap, cur, top; // cursor and first visible line (offsets)
size_t hs;                // first visible column
int rows, cols;
bool crlf, dirty;
String name;

size_t bol(size_t p) {
    while (p && b[p - 1] != '\n') p--;
    return p;
}
size_t eol(size_t p) {
    while (p < len && b[p] != '\n' && b[p] != '\r') p++;
    return p;
}
size_t nextLine(size_t p) { // start of the next line, len if there is none
    p = eol(p);
    if (p < len && b[p] == '\r') p++;
    if (p < len && b[p] == '\n') p++;
    return p;
}
bool lastLine(size_t p) { return eol(p) >= len; }
void moveTo(size_t ls, size_t c) { cur = std::min(ls + c, eol(ls)); }
void up(int n) {
    size_t ls = bol(cur), c = cur - ls;
    while (n-- > 0 && ls) ls = bol(ls - 1);
    moveTo(ls, c);
}
void down(int n) {
    size_t ls = bol(cur), c = cur - ls;
    while (n-- > 0 && !lastLine(ls)) ls = nextLine(ls);
    moveTo(ls, c);
}
void left() {
    if (!cur) return;
    cur--;
    if (cur && b[cur] == '\n' && b[cur - 1] == '\r') cur--;
}
void right() {
    if (cur >= len) return;
    cur += (b[cur] == '\r' && cur + 1 < len && b[cur + 1] == '\n') ? 2 : 1;
}

bool ins(const char *s, size_t n) {
    if (len + n > ED_MAX) return false;
    if (len + n > cap) {
        size_t nc = std::min<size_t>(ED_MAX, len + n + 1024);
        char *nb = (char *)realloc(b, nc);
        if (!nb) return false;
        b = nb;
        cap = nc;
    }
    memmove(b + cur + n, b + cur, len - cur);
    memcpy(b + cur, s, n);
    len += n;
    cur += n;
    dirty = true;
    return true;
}
void backspace() {
    if (!cur) return;
    size_t n = (cur > 1 && b[cur - 1] == '\n' && b[cur - 2] == '\r') ? 2 : 1;
    cur -= n;
    memmove(b + cur, b + cur + n, len - cur - n);
    len -= n;
    dirty = true;
}
bool newline() { // keeps the current line's indentation
    size_t ls = bol(cur), i = ls;
    while (i < cur && (b[i] == ' ' || b[i] == '\t')) i++;
    String ind(crlf ? "\r\n" : "\n");
    for (size_t j = ls; j < i; j++) ind += b[j];
    return ins(ind.c_str(), ind.length());
}

// Scroll so the cursor is visible.
void fit() {
    size_t ls = bol(cur);
    if (cur < top) top = ls;
    int n = 0;
    for (size_t p = top; p < ls; p = nextLine(p)) n++;
    for (; n >= rows; n--) top = nextLine(top);
    size_t c = cur - ls;
    if (c < hs) hs = c;
    else if (c >= hs + cols) hs = c - cols + 1;
}

void drawRow(int r, size_t p, bool valid) {
    String s;
    s.reserve(cols);
    size_t e = valid ? eol(p) : 0;
    for (size_t i = p + hs; valid && i < e && (int)s.length() < cols; i++) {
        char c = b[i];
        s += (c == '\t') ? ' ' : (c < 32 || c > 126) ? '?' : c;
    }
    while ((int)s.length() < cols) s += ' ';
    const int y = ED_Y + r * LH;
    tft->drawString(s, 0, y);
    if (valid && cur >= p && cur <= e) {
        const int x = cur - p - hs;
        drawCharAt(x * LW, y, s[x], BGCOLOR, FGCOLOR);
    }
}
void drawLine(size_t ls) { // the row showing the line that starts at ls, if visible
    int r = 0;
    for (size_t p = top; p < ls && r < rows; p = nextLine(p)) r++;
    if (r < rows) drawRow(r, ls, true);
}
void drawAll() {
    size_t p = top;
    bool valid = true;
    for (int r = 0; r < rows; r++) {
        drawRow(r, p, valid);
        if (valid && lastLine(p)) valid = false;
        else if (valid) p = nextLine(p);
    }
}
void status(const char *msg = nullptr) {
    size_t line = 1;
    for (size_t i = 0; i < cur; i++)
        if (b[i] == '\n') line++;
    String pos = String(line) + ":" + String(cur - bol(cur) + 1);
    String s = msg ? String(msg) : (dirty ? "*" : "") + name;
    const int room = cols - pos.length() - 1;
    if ((int)s.length() > room) s = s.substring(0, room);
    while ((int)s.length() < room + 1) s += ' ';
    tft->setTextColor(BGCOLOR, FGCOLOR);
    tft->drawString(s + pos, 0, 0);
    tft->setTextColor(FGCOLOR, BGCOLOR);
}
void redraw() {
    tft->fillScreen(BGCOLOR);
    tft->setTextSize(FP);
    tft->setTextColor(FGCOLOR, BGCOLOR);
    drawAll();
    status();
}

int choose(std::vector<Option> o) { // loopOptions() returning the index picked, -1 on Esc
    int c = -1;
    for (size_t i = 0; i < o.size(); i++) o[i].operation = [&c, i]() { c = i; };
    loopOptions(o);
    resetGlobals();
    return c;
}

bool jsonOk() { // syntax check only: an empty filter keeps nothing, so no extra memory
    JsonDocument doc, filter;
    return !deserializeJson(
        doc, (const char *)b, len, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(32)
    );
}

bool save(const String &path) {
    String lower = path;
    lower.toLowerCase();
    if ((lower.endsWith(".json") || path == CONFIG_FILE) && !jsonOk() &&
        choose({{"Invalid JSON: save anyway", nullptr}, {"Cancel", nullptr}}) != 0)
        return false;
    const String tmp = path + ".tmp";
    File f = SDM.open(tmp, FILE_WRITE);
    bool ok = f && f.write((const uint8_t *)b, len) == len;
    if (f) f.close();
    ok = ok && SDM.remove(path) && SDM.rename(tmp, path);
    if (!ok) {
        SDM.remove(tmp);
        displayError("Save failed", true);
        return false;
    }
    dirty = false;
    if (path == CONFIG_FILE) getConfigs(); // or the launcher would overwrite the edit later
    return true;
}

void help() {
    static const char *const lines[] = {
        "Fn + ; . , /   move",   "Ctrl + ; .     page up/down", "Ctrl + , /     line start/end",
        "Tab            2 spaces", "Fn + S         save",         "Fn + Q or `    quit",
        "Ctrl + H       this help", "any key        close",
    };
    const int n = sizeof(lines) / sizeof(lines[0]), w = 31 * LW + 8, h = n * LH + 8;
    const int x = (tftWidth - w) / 2, y = (tftHeight - h) / 2;
    tft->fillRoundRect(x, y, w, h, 4, BGCOLOR);
    tft->drawRoundRect(x, y, w, h, 4, FGCOLOR);
    for (int i = 0; i < n; i++) tft->drawString(lines[i], x + 4, y + 4 + i * LH);
    resetGlobals();
    while (!_getKeyPress().pressed) vTaskDelay(pdMS_TO_TICKS(20));
}

bool has(const std::vector<uint8_t> &v, uint8_t k) { return std::find(v.begin(), v.end(), k) != v.end(); }
} // namespace

bool textEditorSupports(const String &path) {
    static const char *const ext[] = {".json", ".conf", ".txt", ".ini", ".cfg", ".csv", ".md", ".log"};
    String p = path;
    p.toLowerCase();
    for (const char *e : ext)
        if (p.endsWith(e)) return true;
    return false;
}

void textEditor(const String &path) {
    File f = SDM.open(path, FILE_READ);
    if (!f) return displayError("Can't open file", true);
    len = f.size();
    if (len > ED_MAX) {
        f.close();
        return displayError("File too large (32KB max)", true);
    }
    cap = std::max<size_t>(len, std::min<size_t>(ED_MAX, len + 1024));
    b = (char *)malloc(cap);
    if (!b) {
        f.close();
        return displayError("Not enough memory", true);
    }
    len = f.read((uint8_t *)b, len);
    f.close();
    if (memchr(b, 0, len)) {
        free(b);
        b = nullptr;
        return displayError("Not a text file", true);
    }
    crlf = memchr(b, '\r', len);
    cur = top = hs = 0;
    dirty = false;
    name = path.substring(path.lastIndexOf('/') + 1);
    cols = tftWidth / LW;
    rows = (tftHeight - ED_Y) / LH;
    redraw();

    char lastArrow = 0; // repeats while an arrow is held (ADV keyboard sends repeat pulses only)
    unsigned long lastKey = 0;
    for (;;) {
        keyStroke k = _getKeyPress();
        if (!k.pressed) {
            char a = 0;
            if (lastArrow && launcherMillis() - lastKey > 300) {
                if (check(UpPress)) a = ';';
                else if (check(DownPress)) a = '.';
                else if (check(PrevPress)) a = ',';
                else if (check(NextPress)) a = '/';
            }
            if (!a || a != lastArrow) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            k.word.push_back(a);
            k.fn = true;
        } else lastKey = launcherMillis();
        UpPress = DownPress = PrevPress = NextPress = SelPress = EscPress = false;

        const bool ctrl = (k.modifiers & 1) || has(k.modifier_keys, KEY_LEFT_CTRL);
        const bool tab = has(k.hid_keys, KEY_TAB);
        const size_t oTop = top, oHs = hs, oLine = bol(cur);
        bool full = false, quit = false;
        const char *msg = nullptr;
        lastArrow = 0;

        if (k.del) {
            full = cur && b[cur - 1] == '\n';
            backspace();
        }
        bool sawTab = false, sawEnter = false;
        for (char c : k.word) {
            if (tab && c == KEY_TAB) c = '\t'; // the ADV keyboard sends Tab as KEY_TAB in word
            if (k.enter && c == KEY_ENTER) c = '\n';
            const char lc = tolower(c);
            if (ctrl) {
                if (lc == ';' || lc == ':') up(rows - 1);
                else if (lc == '.' || lc == '>') down(rows - 1);
                else if (lc == ',' || lc == '<') cur = bol(cur);
                else if (lc == '/' || lc == '?') cur = eol(cur);
                else if (lc == 'h') {
                    help();
                    full = true;
                }
            } else if (k.fn && lc != '\n' && lc != '\t') {
                if (lc == ';') up(1);
                else if (lc == '.') down(1);
                else if (lc == ',') left();
                else if (lc == '/') right();
                else if (lc == 's') msg = save(path) ? "Saved" : nullptr, full = true;
                else if (lc == 'q') quit = true;
                if (lc && strchr(";.,/", lc)) lastArrow = lc;
            } else if (c == '`') quit = true;
            else if (c == '\n') sawEnter = full = true, msg = newline() ? nullptr : "File full";
            else if (c == '\t') sawTab = true, msg = ins("  ", 2) ? nullptr : "File full";
            else if (c >= 32 && c < 127 && !ins(&c, 1)) msg = "File full";
        }
        if (tab && !sawTab) msg = ins("  ", 2) ? nullptr : "File full";      // original Cardputer keyboard
        if (k.enter && !sawEnter) full = true, msg = newline() ? nullptr : "File full";

        if (quit) {
            int c = dirty ? choose({{"Save", nullptr}, {"Don't save", nullptr}, {"Cancel", nullptr}}) : 1;
            if (c == 1 || (c == 0 && save(path))) break;
            full = true;
        }
        fit();
        if (full || top != oTop || hs != oHs) redraw();
        else {
            if (bol(cur) != oLine) drawLine(oLine);
            drawLine(bol(cur));
        }
        status(msg);
    }
    free(b);
    b = nullptr;
    name = "";
    resetGlobals();
}
#endif
