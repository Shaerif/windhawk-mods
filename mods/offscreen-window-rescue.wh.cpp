// ==WindhawkMod==
// @id              offscreen-window-rescue
// @name            Off-Screen Window Rescue
// @description     Moves installer and dialog windows back onto the screen when they open outside the visible work area
// @version         0.1.0
// @author          Shaerif
// @include         msiexec.exe
// @include         setup.exe
// @include         *setup*.exe
// @include         *install*.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Off-Screen Window Rescue
Some installers open their window outside the visible screen area. You can see
it in the taskbar, but you can't reach it with the mouse.

This mod runs inside installer processes (msiexec.exe and setup-style exes). When
one of their top-level windows is shown or moved, the mod checks it. If less than
the chosen share of the window is on a monitor, it moves the window to the center
of the nearest monitor work area.

Add more programs by adding more @include lines in the Source code tab.

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

struct {
    int minVisiblePercent;
    bool logMoves;
} g_settings;

void LoadSettings() {
    g_settings.minVisiblePercent = Wh_GetIntSetting(L"minVisiblePercent");
    if (g_settings.minVisiblePercent < 1) g_settings.minVisiblePercent = 1;
    if (g_settings.minVisiblePercent > 100) g_settings.minVisiblePercent = 100;
    g_settings.logMoves = Wh_GetIntSetting(L"logMoves") != 0;
}

static bool IsCandidate(HWND hWnd) {
    if (!IsWindow(hWnd)) return false;
    if (GetAncestor(hWnd, GA_PARENT) != GetDesktopWindow()) return false;
    LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    if (style & WS_CHILD) return false;
    LONG_PTR ex = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW) return false;
    if (IsIconic(hWnd) || IsZoomed(hWnd)) return false;
    return true;
}

// Share of the window area (0-100) that lies inside any monitor work area.
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
    static thread_local bool busy = false;
    if (busy || !IsCandidate(hWnd)) return;
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

    busy = true;
    SetWindowPos(hWnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    busy = false;

    if (g_settings.logMoves) {
        Wh_Log(L"[%s] moved %p from (%ld,%ld,%ld,%ld) visible=%d%% to (%d,%d)",
               why, hWnd, r.left, r.top, r.right, r.bottom, pct, x, y);
    }
}

using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;
BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    BOOL ret = ShowWindow_Original(hWnd, nCmdShow);
    if (nCmdShow != SW_HIDE && nCmdShow != SW_MINIMIZE && nCmdShow != SW_SHOWMINIMIZED &&
        nCmdShow != SW_SHOWMINNOACTIVE) {
        RescueIfOffScreen(hWnd, L"ShowWindow");
    }
    return ret;
}

using SetWindowPos_t = decltype(&SetWindowPos);
SetWindowPos_t SetWindowPos_Original;
BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND after, int X, int Y, int cx, int cy, UINT flags) {
    BOOL ret = SetWindowPos_Original(hWnd, after, X, Y, cx, cy, flags);
    if (!(flags & SWP_NOMOVE) && IsWindowVisible(hWnd)) {
        RescueIfOffScreen(hWnd, L"SetWindowPos");
    }
    return ret;
}

BOOL Wh_ModInit() {
    LoadSettings();
    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original);
    Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook, (void**)&SetWindowPos_Original);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}
