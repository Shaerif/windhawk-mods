// ==WindhawkMod==
// @id              offscreen-window-rescue
// @name            Off-Screen Window Rescue
// @description     Moves windows back onto the screen when they first appear outside the visible work area
// @version         0.2.0
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
# Off-Screen Window Rescue
Some windows open outside the visible screen area. Installers do this often.
You can see them in the taskbar, but you can't reach them with the mouse.

This mod checks each normal top-level window once, when it first appears. If less
than the chosen share of the window is on a monitor, the mod moves it to the
center of the nearest monitor work area.

It checks only on first show. After that, you can drag a window partly off the
edge and it stays where you put it.

Only windows with a title bar are checked. Menus, tooltips, overlays and
fullscreen games are left alone.

File Explorer and core system processes are excluded for stability. Add more
@exclude lines in the Source code tab for any program you want to skip.

## Settings
- Minimum visible share: the window is moved when less than this percent of its
  area is on screen.
- Log moves: write a line to the Windhawk log each time a window is moved.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- minVisiblePercent: 50
  $name: Minimum visible share (percent)
- logMoves: true
  $name: Log moves
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <mutex>
#include <unordered_set>

struct {
    int minVisiblePercent;
    bool logMoves;
} g_settings;

static std::mutex g_checkedMutex;
static std::unordered_set<HWND> g_checked;

void LoadSettings() {
    g_settings.minVisiblePercent = Wh_GetIntSetting(L"minVisiblePercent");
    if (g_settings.minVisiblePercent < 1) g_settings.minVisiblePercent = 1;
    if (g_settings.minVisiblePercent > 100) g_settings.minVisiblePercent = 100;
    g_settings.logMoves = Wh_GetIntSetting(L"logMoves") != 0;
}

// True the first time a window is seen, false after that.
static bool FirstSight(HWND hWnd) {
    std::lock_guard<std::mutex> lock(g_checkedMutex);
    if (g_checked.size() > 4096) g_checked.clear();
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
    if (IsIconic(hWnd) || IsZoomed(hWnd)) return false;
    return true;
}

// Share of the window area (0-100) that lies inside the nearest monitor work area.
static int VisiblePercent(const RECT& r) {
    LONGLONG area = (LONGLONG)(r.right - r.left) * (r.bottom - r.top);
    if (area <= 0) return 100;
    HMONITOR mon = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (!mon || !GetMonitorInfoW(mon, &mi)) return 100;
    RECT inter{};
    if (!IntersectRect(&inter, &r, &mi.rcWork)) return 0;
    LONGLONG vis = (LONGLONG)(inter.right - inter.left) * (inter.bottom - inter.top);
    return (int)(vis * 100 / area);
}

static void RescueIfOffScreen(HWND hWnd, const wchar_t* why) {
    if (!IsCandidate(hWnd)) return;
    if (!FirstSight(hWnd)) return;
    RECT r{};
    if (!GetWindowRect(hWnd, &r)) return;
    int pct = VisiblePercent(r);
    if (pct >= g_settings.minVisiblePercent) return;

    HMONITOR mon = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (!mon || !GetMonitorInfoW(mon, &mi)) return;

    int w = r.right - r.left;
    int h = r.bottom - r.top;
    int maxW = mi.rcWork.right - mi.rcWork.left;
    int maxH = mi.rcWork.bottom - mi.rcWork.top;
    if (w > maxW) w = maxW;
    if (h > maxH) h = maxH;
    int x = mi.rcWork.left + (maxW - w) / 2;
    int y = mi.rcWork.top + (maxH - h) / 2;

    SetWindowPos(hWnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);

    if (g_settings.logMoves) {
        Wh_Log(L"[%s] moved %p from (%ld,%ld,%ld,%ld) visible=%d%% to (%d,%d)",
               why, hWnd, r.left, r.top, r.right, r.bottom, pct, x, y);
    }
}

using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;
BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    BOOL ret = ShowWindow_Original(hWnd, nCmdShow);
    if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL || nCmdShow == SW_SHOWNA ||
        nCmdShow == SW_SHOWNOACTIVATE || nCmdShow == SW_RESTORE || nCmdShow == SW_SHOWDEFAULT) {
        RescueIfOffScreen(hWnd, L"ShowWindow");
    }
    return ret;
}

using SetWindowPos_t = decltype(&SetWindowPos);
SetWindowPos_t SetWindowPos_Original;
BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND after, int X, int Y, int cx, int cy, UINT flags) {
    BOOL ret = SetWindowPos_Original(hWnd, after, X, Y, cx, cy, flags);
    if (flags & SWP_SHOWWINDOW) {
        RescueIfOffScreen(hWnd, L"SetWindowPos");
    }
    return ret;
}

using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t CreateWindowExW_Original;
HWND WINAPI CreateWindowExW_Hook(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y,
                                 int w, int h, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    HWND hWnd = CreateWindowExW_Original(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    if (hWnd && (style & WS_VISIBLE) && !parent) {
        RescueIfOffScreen(hWnd, L"CreateWindowExW");
    }
    return hWnd;
}

BOOL Wh_ModInit() {
    LoadSettings();
    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original);
    Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original);
    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook, (void**)&CreateWindowExW_Original);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}
