// ==WindhawkMod==
// @id              taskbar-always-on-top
// @name            Taskbar Always On Top
// @description     When an auto-hidden taskbar slides in, show it above every other window, even other topmost windows
// @version         0.1.1
// @author          Shaerif
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luser32 -lshell32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Always On Top
If you use an auto-hidden taskbar, some windows can cover the screen edge. A
window that is set to stay on top is the usual cause. You move the mouse to the
edge, the taskbar slides in behind that window, and you can't click it.

This mod fixes that. When the taskbar slides in, it moves the taskbar above every
other window, including other topmost windows. When the taskbar hides again, the
mod leaves it alone.

This isn't the same as moving the taskbar to the top of the screen. The mod never
changes where the taskbar sits.

## How it works
Among windows that stay on top, the one that was set last is drawn in front. The
mod uses that rule. A small background thread wakes up every few milliseconds
and checks the taskbar. If the taskbar is on screen and a visible topmost window
sits above it, the mod asks Windows to set the taskbar topmost again. That puts
it in front of the others. It doesn't take focus, and it doesn't move or resize
anything.

The mod ignores windows that belong to explorer.exe, such as jump lists, thumbnail
previews, the tray popups, the clock flyout and context menus. That way it never
draws the taskbar over them. The one exception is a File Explorer folder window.
Those can be set to stay on top, so the mod counts them. The mod also waits while
a menu is open on the taskbar or while a mouse button is held down.

The thread does nothing when the taskbar is hidden or when Windows shows no
auto-hide. When the mod unloads, it stops the thread and puts each taskbar window
back the way it found it.

The mod works only in File Explorer, where the taskbar lives. It doesn't block or
change anything else.

## Settings
- Apply to secondary monitors: also handle the taskbars on your other monitors.
  On by default.
- Check interval (milliseconds): how often the mod looks at the taskbar. The
  default is 50. The allowed range is 10 to 1000. A longer time uses less CPU
  but reacts later.

## Limits
- The mod was tested on one PC: Windows 11 build 26300, one monitor, auto-hide
  on. In that test the taskbar went above a topmost test window from another
  process. Secondary monitors and taskbars that aren't at the bottom weren't
  tested. No other PC was checked. Expect to adjust it.
- Windows keeps windows in layers called bands. A window can't pass another
  window that sits in a higher band, and this mod can't change that. Some system
  windows, some accessibility tools and exclusive fullscreen games may stay in
  front.
- A program that keeps setting itself topmost over and over can fight the mod.
  The mod retries at most twice a second, so the taskbar may flicker in front
  of such a window.
- It does nothing if the taskbar isn't set to auto-hide.

## Tutorial
First turn on "Automatically hide the taskbar" in Settings, Personalization,
Taskbar, Taskbar behaviors. Then open a window that stays on top and place it at
the screen edge where the taskbar hides.

1. Move the mouse to the screen edge. The taskbar slides in and now sits in front
   of the topmost window.
2. Press the Windows key. Windows already brings the taskbar forward when Start
   opens, so the mod isn't involved here.
3. Set Check interval (milliseconds) higher if you want less CPU use. Set it
   lower if the taskbar shows behind the window for a moment before it jumps in
   front.

### Gotchas
- Step 1: The mod can't tell the taskbar is shown until it has slid at least
  half way in. You may see a short moment behind the window.
- Step 3: A value below 10 counts as 10, and a value above 1000 counts as 1000.
- If a topmost window covers the very last pixel row of the screen, Windows won't
  reveal the taskbar at all. This is a Windows limit, not the mod.
- With a left or right taskbar and another monitor beside it, the mod may judge
  the hidden taskbar as shown. The effect is harmless. It just sets the taskbar
  topmost a little more often.

## Version history
- 0.1.1: Stopped the mod from drawing the taskbar over jump lists and other
  explorer popups.
- 0.1.0: First release.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- includeSecondary: true
  $name: Apply to secondary monitors
- intervalMs: 50
  $name: Check interval (milliseconds)
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <map>

static std::atomic<bool> g_includeSecondary{true};
static std::atomic<int> g_intervalMs{50};

// State of a taskbar window the mod has touched.
struct Tracked {
    bool wasTopmost;
    ULONGLONG lastAssert;
};

// Only the worker thread writes this while the mod runs. Uninit reads it after the
// thread has stopped.
static std::map<HWND, Tracked> g_tracked;

static HANDLE g_stopEvent = nullptr;
static HANDLE g_thread = nullptr;

static const ULONGLONG kRetryGapMs = 500;
static const ULONGLONG kAutoHideRefreshMs = 1000;

static void LoadSettings() {
    g_includeSecondary = Wh_GetIntSetting(L"includeSecondary") != 0;
    int interval = Wh_GetIntSetting(L"intervalMs");
    if (interval < 10) interval = 10;
    if (interval > 1000) interval = 1000;
    g_intervalMs = interval;
}

// True when at least half of the taskbar thickness lies on the monitor.
static bool IsRevealed(HWND hWnd, RECT* outRect) {
    if (!IsWindowVisible(hWnd)) return false;
    RECT r{};
    if (!GetWindowRect(hWnd, &r)) return false;
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    int thick = std::min(w, h);
    if (thick <= 0) return false;

    HMONITOR mon = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (!mon || !GetMonitorInfoW(mon, &mi)) return false;

    RECT inter{};
    if (!IntersectRect(&inter, &r, &mi.rcMonitor)) return false;
    int shown = std::min(inter.right - inter.left, inter.bottom - inter.top);
    if (shown * 2 < thick) return false;

    *outRect = r;
    return true;
}

// Real File Explorer folder windows. A user may keep one on top, so they count.
static bool IsFolderWindow(HWND w) {
    wchar_t cls[32];
    int n = GetClassNameW(w, cls, ARRAYSIZE(cls));
    return n > 0 && (wcscmp(cls, L"CabinetWClass") == 0 || wcscmp(cls, L"ExploreWClass") == 0);
}

// True for explorer popups the taskbar must not draw over: jump lists, flyouts,
// tray popups, the edge grip window and tool or popup windows owned by explorer.
static bool IsShellPopup(HWND w, DWORD taskbarPid) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == taskbarPid && !IsFolderWindow(w)) return true;

    HWND owner = GetWindow(w, GW_OWNER);
    if (!owner) return false;
    DWORD ownerPid = 0;
    GetWindowThreadProcessId(owner, &ownerPid);
    if (ownerPid != taskbarPid) return false;
    return (GetWindowLongPtrW(w, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0 ||
           (GetWindowLongPtrW(w, GWL_STYLE) & WS_POPUP) != 0;
}

// True when a menu or popup menu is open on the taskbar thread.
static bool MenuActive(DWORD taskbarTid) {
    GUITHREADINFO gti{sizeof(gti)};
    if (!GetGUIThreadInfo(taskbarTid, &gti)) return false;
    return (gti.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE)) != 0;
}

static bool MouseButtonDown() {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
           (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}

// True when a visible window above the taskbar overlaps it. Windows that belong to
// explorer.exe do not count, except File Explorer folder windows.
static bool IsCovered(HWND taskbar, DWORD taskbarPid, const RECT& taskbarRect) {
    for (HWND w = GetWindow(taskbar, GW_HWNDPREV); w; w = GetWindow(w, GW_HWNDPREV)) {
        if (!IsWindowVisible(w)) continue;
        if (IsShellPopup(w, taskbarPid)) continue;
        RECT r{};
        if (!GetWindowRect(w, &r)) continue;
        RECT inter{};
        if (IntersectRect(&inter, &r, &taskbarRect)) return true;
    }
    return false;
}

static void CheckTaskbar(HWND taskbar, ULONGLONG now) {
    RECT r{};
    if (!IsRevealed(taskbar, &r)) return;
    DWORD taskbarPid = 0;
    DWORD taskbarTid = GetWindowThreadProcessId(taskbar, &taskbarPid);
    if (!IsCovered(taskbar, taskbarPid, r)) return;
    if (MenuActive(taskbarTid)) return;

    auto it = g_tracked.find(taskbar);
    if (it == g_tracked.end()) {
        Tracked t{};
        t.wasTopmost = (GetWindowLongPtrW(taskbar, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        t.lastAssert = 0;
        it = g_tracked.emplace(taskbar, t).first;
    }
    if (it->second.lastAssert != 0 && now - it->second.lastAssert < kRetryGapMs) return;
    it->second.lastAssert = now;

    // The async flag keeps this thread from waiting on the taskbar's own thread.
    SetWindowPos(taskbar, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
}

static bool AutoHideOn() {
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    UINT_PTR state = SHAppBarMessage(ABM_GETSTATE, &abd);
    return (state & ABS_AUTOHIDE) != 0;
}

static DWORD WINAPI WorkerThread(LPVOID) {
    DWORD pid = GetCurrentProcessId();
    ULONGLONG lastAutoHideCheck = 0;
    bool autoHide = false;

    while (WaitForSingleObject(g_stopEvent, (DWORD)g_intervalMs.load()) == WAIT_TIMEOUT) {
        ULONGLONG now = GetTickCount64();
        if (lastAutoHideCheck == 0 || now - lastAutoHideCheck >= kAutoHideRefreshMs) {
            autoHide = AutoHideOn();
            lastAutoHideCheck = now;
        }
        if (!autoHide) continue;
        if (MouseButtonDown()) continue;

        HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (primary) CheckTaskbar(primary, now);

        if (!g_includeSecondary) continue;
        HWND secondary = nullptr;
        while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
            DWORD owner = 0;
            GetWindowThreadProcessId(secondary, &owner);
            if (owner == pid) CheckTaskbar(secondary, now);
        }
    }
    return 0;
}

BOOL Wh_ModInit() {
    LoadSettings();

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        Wh_Log(L"Could not create the stop event");
        return FALSE;
    }
    g_thread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    if (!g_thread) {
        Wh_Log(L"Could not start the worker thread");
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        return FALSE;
    }
    Wh_Log(L"Started: interval=%d secondary=%d", g_intervalMs.load(), g_includeSecondary.load() ? 1 : 0);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}

void Wh_ModUninit() {
    if (g_stopEvent) SetEvent(g_stopEvent);
    if (g_thread) {
        if (WaitForSingleObject(g_thread, 5000) == WAIT_TIMEOUT) {
            Wh_Log(L"Worker thread did not stop in time");
        }
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }

    // Put back the topmost state each taskbar window had before the mod touched it.
    for (const auto& entry : g_tracked) {
        if (!IsWindow(entry.first)) continue;
        if (entry.second.wasTopmost) continue;
        SetWindowPos(entry.first, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
    }
    g_tracked.clear();
}
