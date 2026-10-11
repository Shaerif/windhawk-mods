// ==WindhawkMod==
// @id              window-rules
// @name            Window Rules
// @description     Give any app a set position, size, monitor, always-on-top flag or transparency when its window opens
// @version         0.1.0
// @author          Shaerif
// @include         *
// @exclude         explorer.exe
// @exclude         csrss.exe
// @exclude         dwm.exe
// @exclude         winlogon.exe
// @exclude         LogonUI.exe
// @exclude         fontdrvhost.exe
// @exclude         sihost.exe
// @exclude         ShellHost.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Window Rules
KDE lets you say "this app always opens here, at this size". Windows does not.
This mod adds that. You write a rule for a program. When one of its windows first
opens, the mod applies the rule.

## Rule fields
- Process: the program file name, for example `notepad.exe`. Required.
- Title contains: optional text the window title must contain.
- Monitor: 1 for the first monitor, 2 for the second, and so on. 0 keeps the
  current monitor.
- X and Y: offset in pixels from the top left corner of the monitor work area.
- Width and Height: size in pixels. 0 keeps the current size.
- Maximize: open the window maximized.
- Always on top: keep the window above others.
- Opacity: 10 to 100 percent. 0 leaves it alone.

The first rule that matches wins. A rule that has a process name and nothing else
does nothing.

## Position rule
The mod moves the window when you set a monitor, an X or Y offset, or a size.
The final window is always kept inside the monitor work area. A rule can never
place a window off screen.

## Examples
1. Notepad on the second monitor, 900 by 700 pixels, 50 pixels from the corner:
   process `notepad.exe`, monitor 2, X 50, Y 50, width 900, height 700.
2. A calculator that stays on top and is slightly see-through:
   process `CalculatorApp.exe`, always on top on, opacity 90.

## Limits
- Rules apply once, when the window first appears. Move it afterward and it stays
  where you put it.
- Windows that set their own transparency keep it. The opacity field skips them.
- File Explorer and core system processes are excluded for stability.
- Windows that run as administrator cannot be changed by a normal process.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- enabled: true
  $name: Enable rules
- logRules: true
  $name: Log applied rules
- rules:
  - - process: ""
    - title: ""
    - monitor: 0
    - x: 0
    - y: 0
    - width: 0
    - height: 0
    - maximize: false
    - alwaysOnTop: false
    - opacity: 0
  $name: Rules
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

struct Rule {
    std::wstring title;
    int monitor = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool maximize = false;
    bool alwaysOnTop = false;
    int opacity = 0;
};

static bool g_enabled = true;
static bool g_log = true;
static std::wstring g_exeName;
static std::vector<Rule> g_rules;
static std::mutex g_mutex;
static std::unordered_set<HWND> g_checked;

static std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

static std::wstring GetExeName() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (!n) return L"";
    const wchar_t* slash = wcsrchr(path, L'\\');
    return Lower(slash ? slash + 1 : path);
}

static void LoadSettings() {
    std::vector<Rule> rules;
    g_enabled = Wh_GetIntSetting(L"enabled") != 0;
    g_log = Wh_GetIntSetting(L"logRules") != 0;
    for (int i = 0; i < 200; i++) {
        PCWSTR proc = Wh_GetStringSetting(L"rules[%d].process", i);
        bool empty = !proc || !*proc;
        std::wstring procLower = empty ? L"" : Lower(proc);
        Wh_FreeStringSetting(proc);
        if (empty) {
            // Rules can have blank process names in the middle. Stop only at the end of the list.
            PCWSTR probe = Wh_GetStringSetting(L"rules[%d].title", i);
            bool missing = !probe || !*probe;
            Wh_FreeStringSetting(probe);
            if (missing && Wh_GetIntSetting(L"rules[%d].monitor", i) == 0 &&
                Wh_GetIntSetting(L"rules[%d].width", i) == 0 &&
                Wh_GetIntSetting(L"rules[%d].opacity", i) == 0) {
                break;
            }
            continue;
        }
        if (procLower != g_exeName) continue;
        Rule r;
        PCWSTR title = Wh_GetStringSetting(L"rules[%d].title", i);
        if (title && *title) r.title = Lower(title);
        Wh_FreeStringSetting(title);
        r.monitor = Wh_GetIntSetting(L"rules[%d].monitor", i);
        r.x = Wh_GetIntSetting(L"rules[%d].x", i);
        r.y = Wh_GetIntSetting(L"rules[%d].y", i);
        r.width = Wh_GetIntSetting(L"rules[%d].width", i);
        r.height = Wh_GetIntSetting(L"rules[%d].height", i);
        r.maximize = Wh_GetIntSetting(L"rules[%d].maximize", i) != 0;
        r.alwaysOnTop = Wh_GetIntSetting(L"rules[%d].alwaysOnTop", i) != 0;
        r.opacity = Wh_GetIntSetting(L"rules[%d].opacity", i);
        rules.push_back(r);
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_rules.swap(rules);
}

static bool FirstSight(HWND hWnd) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_checked.size() > 2048) g_checked.clear();
    return g_checked.insert(hWnd).second;
}

static bool IsCandidate(HWND hWnd) {
    if (!IsWindow(hWnd)) return false;
    if (GetAncestor(hWnd, GA_PARENT) != GetDesktopWindow()) return false;
    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if (style & WS_CHILD) return false;
    if ((style & WS_CAPTION) != WS_CAPTION) return false;
    LONG_PTR ex = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (ex & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) return false;
    return true;
}

struct MonitorPick {
    int target;
    int index;
    HMONITOR found;
};

static BOOL CALLBACK MonitorEnum(HMONITOR mon, HDC, LPRECT, LPARAM lp) {
    MonitorPick* p = (MonitorPick*)lp;
    p->index++;
    if (p->index == p->target) {
        p->found = mon;
        return FALSE;
    }
    return TRUE;
}

static bool FindRule(HWND hWnd, Rule& out) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_rules.empty()) return false;
    wchar_t buf[512];
    int n = GetWindowTextW(hWnd, buf, ARRAYSIZE(buf));
    std::wstring title = Lower(std::wstring(buf, n > 0 ? n : 0));
    for (const Rule& r : g_rules) {
        if (!r.title.empty() && title.find(r.title) == std::wstring::npos) continue;
        out = r;
        return true;
    }
    return false;
}

static void ApplyRule(HWND hWnd, const wchar_t* why) {
    if (!g_enabled || !IsCandidate(hWnd)) return;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_rules.empty()) return;
    }
    if (!FirstSight(hWnd)) return;
    Rule rule;
    if (!FindRule(hWnd, rule)) return;

    RECT wr{};
    if (!GetWindowRect(hWnd, &wr)) return;

    bool wantsMove = rule.monitor > 0 || rule.x != 0 || rule.y != 0 || rule.width > 0 || rule.height > 0;
    if (wantsMove) {
        HMONITOR mon = nullptr;
        if (rule.monitor > 0) {
            MonitorPick pick{rule.monitor, 0, nullptr};
            EnumDisplayMonitors(nullptr, nullptr, MonitorEnum, (LPARAM)&pick);
            mon = pick.found;
        }
        if (!mon) mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{sizeof(mi)};
        if (mon && GetMonitorInfoW(mon, &mi)) {
            int maxW = mi.rcWork.right - mi.rcWork.left;
            int maxH = mi.rcWork.bottom - mi.rcWork.top;
            int w = rule.width > 0 ? rule.width : wr.right - wr.left;
            int h = rule.height > 0 ? rule.height : wr.bottom - wr.top;
            if (w > maxW) w = maxW;
            if (h > maxH) h = maxH;
            int x = mi.rcWork.left + rule.x;
            int y = mi.rcWork.top + rule.y;
            if (x + w > mi.rcWork.right) x = mi.rcWork.right - w;
            if (y + h > mi.rcWork.bottom) y = mi.rcWork.bottom - h;
            if (x < mi.rcWork.left) x = mi.rcWork.left;
            if (y < mi.rcWork.top) y = mi.rcWork.top;
            SetWindowPos(hWnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    if (rule.alwaysOnTop) {
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (rule.opacity >= 10 && rule.opacity <= 100) {
        LONG_PTR ex = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
        if (!(ex & WS_EX_LAYERED)) {
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
            SetLayeredWindowAttributes(hWnd, 0, (BYTE)(rule.opacity * 255 / 100), LWA_ALPHA);
        }
    }
    if (rule.maximize) {
        ShowWindow(hWnd, SW_MAXIMIZE);
    }
    if (g_log) Wh_Log(L"[%s] applied a rule to window %p", why, hWnd);
}

using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;
BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    BOOL ret = ShowWindow_Original(hWnd, nCmdShow);
    if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL || nCmdShow == SW_SHOWNA ||
        nCmdShow == SW_SHOWNOACTIVATE || nCmdShow == SW_RESTORE || nCmdShow == SW_SHOWDEFAULT) {
        ApplyRule(hWnd, L"ShowWindow");
    }
    return ret;
}

using SetWindowPos_t = decltype(&SetWindowPos);
SetWindowPos_t SetWindowPos_Original;
BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND after, int X, int Y, int cx, int cy, UINT flags) {
    BOOL ret = SetWindowPos_Original(hWnd, after, X, Y, cx, cy, flags);
    if (flags & SWP_SHOWWINDOW) ApplyRule(hWnd, L"SetWindowPos");
    return ret;
}

using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t CreateWindowExW_Original;
HWND WINAPI CreateWindowExW_Hook(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y, int w,
                                 int h, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    HWND hWnd = CreateWindowExW_Original(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    if (hWnd && (style & WS_VISIBLE) && !parent) ApplyRule(hWnd, L"CreateWindowExW");
    return hWnd;
}

BOOL Wh_ModInit() {
    g_exeName = GetExeName();
    LoadSettings();
    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original);
    Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original);
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook,
                       (void**)&CreateWindowExW_Original);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}
