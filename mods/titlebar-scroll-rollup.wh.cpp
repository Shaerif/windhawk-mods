// ==WindhawkMod==
// @id              titlebar-scroll-rollup
// @name            Title Bar Scroll Roll-Up
// @description     Scroll the mouse wheel on a title bar to roll the window up to its title bar and back
// @version         0.1.0
// @author          Shaerif
// @include         explorer.exe
// @compilerOptions -luser32 -lgdi32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Title Bar Scroll Roll-Up
KDE and older Linux desktops can roll a window up so only its title bar shows.
You scroll the mouse wheel on the title bar. Scroll the other way and the window
rolls back down. This mod adds that to Windows.

## How it works
Scroll up on a title bar to roll the window up. Scroll down to unroll it.
The mod doesn't resize the window. It clips the window so only the title bar
shows. The app doesn't notice, so apps with a minimum size still work.

## Notes
- The taskbar still lists a rolled-up window and its thumbnail shows the full
  window.
- The mod only acts on the title bar. Scrolling in the window body works as usual.
- Maximized, minimized and full screen windows are skipped.
- Windows that run as administrator can't be changed from Explorer. They are
  skipped.
- All rolled-up windows come back when you unload the mod.

## Tutorial
1. Open a window that isn't maximized, minimized or full screen. Rest the
   pointer on an empty part of its title bar.
   ![A test window called Demo window with its two lines of text in view. A red box frames its title bar, and a red arrow points down at the spot where the pointer rested](https://raw.githubusercontent.com/Shaerif/windhawk-mods/main/docs/screenshots/titlebar-scroll-rollup-step-1-before.png)
2. Scroll the wheel up once. The window rolls up to its title bar.
   ![The same test window after one scroll up on its title bar. The title bar is still there in the red box, and below it only the frame outline is left, with the backdrop showing through](https://raw.githubusercontent.com/Shaerif/windhawk-mods/main/docs/screenshots/titlebar-scroll-rollup-step-2-after.png)
3. Keep the pointer on the title bar and scroll down. The window comes back.
4. Scroll in the body of the window. Nothing changes, and the app gets the
   scroll as usual.

### Gotchas
- Step 1: The minimize, maximize and close buttons don't count as title bar. A
  scroll on them does nothing.
- Step 2: The window keeps its real size. The mod clips it, so the frame
  outline you see is the full window.
- Step 3: A scroll down on a window the mod didn't roll up goes to the app as
  usual.
- Step 4: The mod only watches the top 100 pixels of the window, scaled for its
  DPI. A point lower down never counts.

Read the full guide in
[docs/titlebar-scroll-rollup.md](https://github.com/Shaerif/windhawk-mods/blob/main/docs/titlebar-scroll-rollup.md).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- enabled: true
  $name: Roll up on title bar scroll
- reverse: false
  $name: Reverse the scroll direction
- modifier: none
  $name: Hold a key while scrolling
  $options:
  - none: No key needed
  - ctrl: Ctrl
  - shift: Shift
  - alt: Alt
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <dwmapi.h>

#include <set>

#define WM_APP_QUIT (WM_APP + 1)

enum Modifier { MOD_NONE_KEY, MOD_CTRL_KEY, MOD_SHIFT_KEY, MOD_ALT_KEY };

struct {
    bool enabled;
    bool reverse;
    Modifier modifier;
} g_settings;

static HANDLE g_thread = nullptr;
static DWORD g_threadId = 0;
static HANDLE g_readyEvent = nullptr;
static HHOOK g_hook = nullptr;
static std::set<HWND> g_rolled;

static void LoadSettings() {
    g_settings.enabled = Wh_GetIntSetting(L"enabled") != 0;
    g_settings.reverse = Wh_GetIntSetting(L"reverse") != 0;
    PCWSTR m = Wh_GetStringSetting(L"modifier");
    g_settings.modifier = MOD_NONE_KEY;
    if (m) {
        if (wcscmp(m, L"ctrl") == 0) g_settings.modifier = MOD_CTRL_KEY;
        else if (wcscmp(m, L"shift") == 0) g_settings.modifier = MOD_SHIFT_KEY;
        else if (wcscmp(m, L"alt") == 0) g_settings.modifier = MOD_ALT_KEY;
    }
    Wh_FreeStringSetting(m);
}

static bool ModifierHeld() {
    switch (g_settings.modifier) {
        case MOD_CTRL_KEY: return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        case MOD_SHIFT_KEY: return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        case MOD_ALT_KEY: return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        default: return true;
    }
}

static void Unroll(HWND hWnd) {
    if (IsWindow(hWnd)) SetWindowRgn(hWnd, nullptr, TRUE);
    g_rolled.erase(hWnd);
}

static void DropDeadEntries() {
    for (auto it = g_rolled.begin(); it != g_rolled.end();) {
        if (!IsWindow(*it)) it = g_rolled.erase(it);
        else ++it;
    }
}

static bool CanRoll(HWND hWnd) {
    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if ((style & WS_CAPTION) != WS_CAPTION) return false;
    if (style & WS_CHILD) return false;
    LONG_PTR ex = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW) return false;
    if (IsZoomed(hWnd) || IsIconic(hWnd)) return false;
    // Skip windows that are full screen.
    RECT r{};
    if (!GetWindowRect(hWnd, &r)) return false;
    HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (mon && GetMonitorInfoW(mon, &mi)) {
        if (r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top && r.right >= mi.rcMonitor.right &&
            r.bottom >= mi.rcMonitor.bottom) {
            return false;
        }
    }
    return true;
}

static int TitleHeight(HWND hWnd, const RECT& wr) {
    UINT dpi = GetDpiForWindow(hWnd);
    if (!dpi) dpi = 96;
    int caption = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
    int visibleTop = 0;
    RECT ext{};
    if (SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &ext, sizeof(ext)))) {
        visibleTop = ext.top - wr.top;
        if (visibleTop < 0) visibleTop = 0;
    }
    return visibleTop + caption + 1;
}

static bool Roll(HWND hWnd) {
    if (g_rolled.count(hWnd)) return true;
    if (!CanRoll(hWnd)) return false;
    // Skip windows that already use their own region, for example rounded shapes.
    HRGN probe = CreateRectRgn(0, 0, 0, 0);
    int existing = GetWindowRgn(hWnd, probe);
    DeleteObject(probe);
    if (existing != ERROR) return false;

    RECT wr{};
    if (!GetWindowRect(hWnd, &wr)) return false;
    int w = wr.right - wr.left;
    int h = TitleHeight(hWnd, wr);
    HRGN rgn = CreateRectRgn(0, 0, w, h);
    if (!rgn) return false;
    if (!SetWindowRgn(hWnd, rgn, TRUE)) {
        DeleteObject(rgn);
        return false;
    }
    g_rolled.insert(hWnd);
    return true;
}

static bool HandleWheel(const MSLLHOOKSTRUCT* info) {
    if (!g_settings.enabled || !ModifierHeld()) return false;
    short delta = (short)HIWORD(info->mouseData);
    if (delta == 0) return false;

    HWND under = WindowFromPoint(info->pt);
    if (!under) return false;
    HWND root = GetAncestor(under, GA_ROOT);
    if (!root) return false;

    RECT wr{};
    if (!GetWindowRect(root, &wr)) return false;
    UINT dpi = GetDpiForWindow(root);
    if (!dpi) dpi = 96;
    int band = MulDiv(100, dpi, 96);
    if (info->pt.y < wr.top || info->pt.y > wr.top + band) return false;

    DWORD_PTR hit = 0;
    LPARAM lp = MAKELPARAM((short)info->pt.x, (short)info->pt.y);
    if (!SendMessageTimeoutW(root, WM_NCHITTEST, 0, lp, SMTO_ABORTIFHUNG | SMTO_BLOCK, 40, &hit)) {
        return false;
    }
    if (hit != HTCAPTION) return false;

    DropDeadEntries();
    bool up = delta > 0;
    if (g_settings.reverse) up = !up;
    if (up) return Roll(root);
    if (g_rolled.count(root)) {
        Unroll(root);
        return true;
    }
    return false;
}

static LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && wParam == WM_MOUSEWHEEL) {
        if (HandleWheel((const MSLLHOOKSTRUCT*)lParam)) return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

static DWORD WINAPI ThreadProc(LPVOID) {
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
    if (!g_hook) Wh_Log(L"Could not install the mouse hook (error %lu)", GetLastError());
    SetEvent(g_readyEvent);
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_APP_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
    std::set<HWND> copy = g_rolled;
    for (HWND h : copy) Unroll(h);
    return 0;
}

BOOL Wh_ModInit() {
    LoadSettings();
    g_readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, &g_threadId);
    if (!g_thread) {
        if (g_readyEvent) CloseHandle(g_readyEvent);
        g_readyEvent = nullptr;
        return FALSE;
    }
    WaitForSingleObject(g_readyEvent, 3000);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}

void Wh_ModUninit() {
    if (g_thread) {
        PostThreadMessageW(g_threadId, WM_APP_QUIT, 0, 0);
        WaitForSingleObject(g_thread, 5000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_readyEvent) {
        CloseHandle(g_readyEvent);
        g_readyEvent = nullptr;
    }
}
