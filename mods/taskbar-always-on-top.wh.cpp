// ==WindhawkMod==
// @id              taskbar-always-on-top
// @name            Taskbar Always On Top
// @description     Keep an auto-hidden taskbar above every other window, but never above a menu or a flyout
// @version         0.1.4
// @author          Shaerif
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luser32 -lshell32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Always On Top
If you use an auto-hidden taskbar, some windows can cover the screen edge. A
window that's set to stay on top is the usual cause. You move the mouse to the
edge, the taskbar slides in behind that window, and you can't click it.

There's a second case. File Explorer itself sometimes sends the taskbar to the
back right after you click a taskbar button. Then the taskbar shows up behind the
window you just opened, and you can't see it or use it until you click somewhere
else.

This mod fixes both. The taskbar stays above every other window, including other
topmost windows. When the taskbar hides again, the mod leaves it alone. Menus and
flyouts are the one thing the taskbar never covers. See "Popups and menus" below.

This isn't the same as moving the taskbar to the top of the screen. The mod never
changes where the taskbar sits.

## How it works
The mod does two things.

First, it watches the calls File Explorer makes to move the taskbar in the window
stack. Explorer sometimes thinks the window in front is a fullscreen app. Then it
sends the taskbar to the bottom of the stack and takes away its topmost state. The
mod catches that call and keeps the taskbar where it was. Only a real fullscreen
app gets through, so a video or a game still covers the taskbar. The mod also
catches the calls that raise the taskbar while a menu or a flyout is open, and
leaves the stack alone, so the taskbar can't jump over the menu.

Second, a small background thread wakes up every few milliseconds and checks the
taskbar. If the taskbar lost its topmost state anyway, the thread gives it back on
the next check. If the taskbar is on screen and a visible topmost window sits above
it, the thread sets the taskbar topmost again. Among windows that stay on top, the
one that was set last is drawn in front, so that puts the taskbar in front of the
others. It doesn't take focus, and it doesn't move or resize anything.

## What counts as a fullscreen app
The test is strict on purpose. When the mod isn't sure, the taskbar stays on top.
The window in front counts as fullscreen only when all of this is true:

- Windows itself reports a fullscreen app, a Direct3D fullscreen game, a
  presentation or a fullscreen Store app. The mod asks with
  SHQueryUserNotificationState, the same call the notification system uses.
- The window rect is the same as the monitor rect, not one pixel more or less.
- The window isn't maximized.
- The window has no title bar and no sizing frame.
- The window isn't a click-through, no-activate or tool window. Overlays look
  like that, and an overlay is never a fullscreen app.
- The window doesn't belong to explorer.exe. A File Explorer folder window is the
  one exception.

A browser video in F11 mode and a borderless game pass every point. A desktop
app with its own title bar that fills the screen doesn't, and neither does a
transparent overlay that covers the screen. Version 0.1.2 took such an overlay
for a fullscreen app and let explorer send the taskbar to the back. That's fixed.

## Popups and menus
A menu or a flyout that opens from the taskbar must stay in front of it. That
holds for the menu of a tray icon, a jump list, a thumbnail preview, Start,
Search, Quick settings, the clock flyout, the notification center, a tooltip and
any owned popup window from any program. Version 0.1.3 got this wrong when
auto-hide was on. It saw the menu of a tray icon as a window that covers the
taskbar, raised the taskbar over it, and cut off the lower part of the menu.

Since 0.1.4 the mod looks for popups before it touches the taskbar. A popup is a
visible window that overlaps the taskbar and is a menu, a known flyout class, a
window with an owner, or a borderless window without a title bar. While one is
open, the mod doesn't raise the taskbar, and it also stops explorer's own calls
that would raise it. When it finds a topmost popup that already sits under the
taskbar, it moves the taskbar right below that popup. It never moves, hides or
activates the popup itself. It waits 150 milliseconds after the last popup has
closed before it raises the taskbar again.

Three kinds of windows never count as a popup: a click-through window, the
invisible edge grip of a taskbar resize mod, and a window that spans at least 90
percent of the monitor in both directions. The mod also waits while a menu is
open on the taskbar thread or while a mouse button is held down.

The mod does nothing when Windows shows no auto-hide. When the mod unloads, it
stops the thread and its hooks. The taskbar then stays on top, which is the Windows
default, unless a real fullscreen app is in front.

The mod works only in File Explorer, where the taskbar lives.

## Settings
- Apply to secondary monitors: also handle the taskbars on your other monitors.
  On by default.
- Check interval (milliseconds): how often the mod looks at the taskbar. The
  default is 50. The allowed range is 10 to 1000. A longer time uses less CPU
  but reacts later.
- Write a debug log: writes what the mod does to taskbar-always-on-top-log.txt
  in your temp folder. Off by default. The file stops growing after 500 lines
  per session. Each line about a blocked call names the caller and the window
  in front, with its rect, styles and what the fullscreen test decided. Each
  popup that overlaps the taskbar gets a line with its class, its program, its
  rect and whether it sits above or below the taskbar. A raise call from
  explorer that the mod held back gets a line with the caller too.

## Limits
- The mod was tested on one PC: Windows 11 build 26300, one monitor, auto-hide
  on. Secondary monitors and taskbars that aren't at the bottom weren't tested.
  No other PC was checked. Expect to adjust it.
- A game that runs maximized or keeps a title bar counts as a normal window, so
  the taskbar stays on top of it. With auto-hide on you only see a thin line at
  the edge until you move the mouse there.
- A borderless window without a title bar that overlaps the taskbar counts as a
  popup. The taskbar won't rise above such a window. That's on purpose, since
  a covered menu is worse than a covered taskbar edge.
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
2. Click a taskbar button. The window opens and the taskbar stays in front of it
   until you move the mouse away.
3. Right click a taskbar button. The jump list opens above the taskbar as usual.
4. Click a tray icon that opens a menu. The whole menu shows above the taskbar.
5. Set Check interval (milliseconds) higher if you want less CPU use. Set it
   lower if the taskbar shows behind the window for a moment before it jumps in
   front.

### Gotchas
- Step 1: The mod can't tell the taskbar is shown until it has slid at least
  half way in. You may see a short moment behind the window.
- Step 5: A value below 10 counts as 10, and a value above 1000 counts as 1000.
- If a topmost window covers the last pixel row of the screen, Windows won't
  reveal the taskbar at all. This is a Windows limit, not the mod.
- With a left or right taskbar and another monitor beside it, the mod may judge
  the hidden taskbar as shown. The effect is harmless. It just sets the taskbar
  topmost a little more often.

## Version history
- 0.1.4: Stopped the taskbar from covering menus and flyouts when auto-hide is
  on. The mod raised the taskbar over the menu of a tray icon, since that menu
  belongs to another program and the old check only knew explorer's own popups.
  Now any popup that overlaps the taskbar holds the taskbar back, explorer's
  raise calls are held back too, and a topmost popup found under the taskbar
  gets the taskbar moved below it. The debug log names each popup.
- 0.1.3: Made the fullscreen test strict. A screen-sized overlay from a desktop
  app passed the old test, so explorer could still send the taskbar to the back
  after a click on a taskbar button. The mod now also asks Windows whether a
  fullscreen app runs, wants the exact monitor rect, and rejects overlays. The
  debug log now prints the window styles and the reason for each decision.
- 0.1.2: Kept the taskbar on top after a click on a taskbar button. Explorer
  could send it to the back, and the mod now stops that unless a fullscreen app
  is in front. Added the debug log setting.
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
- debugLog: false
  $name: Write a debug log
  $description: Writes what the mod does to taskbar-always-on-top-log.txt in your temp folder
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <map>
#include <set>
#include <vector>

static std::atomic<bool> g_includeSecondary{true};
static std::atomic<int> g_intervalMs{50};
static std::atomic<bool> g_debugLog{false};

// The worker thread keeps this up to date. The hooks read it, because asking for the
// auto-hide state inside a hook would send a message to the taskbar itself.
static std::atomic<bool> g_autoHide{false};

// Set by the worker while a popup overlaps a taskbar, and for a short time after
// the popup closed. The hooks read it and hold back explorer's raise calls.
static std::atomic<bool> g_popupBlock{false};

// Set while the mod unloads, so its own calls that put the taskbar back pass through.
static std::atomic<bool> g_unloading{false};

// Time of the last SetWindowPos the worker made for each taskbar window. Only the
// worker thread touches this.
static std::map<HWND, ULONGLONG> g_lastAssert;

// Popups the worker saw in its last scan. A popup gets one log line when it shows
// up. Only the worker thread touches this.
static std::set<HWND> g_knownPopups;

// Tick of the last scan that found a popup. Only the worker thread touches this.
static ULONGLONG g_lastPopupSeen = 0;

// Tick of the last log line about a raise call the hooks held back.
static std::atomic<ULONGLONG> g_lastRaiseLog{0};

static HANDLE g_stopEvent = nullptr;
static HANDLE g_thread = nullptr;

static const ULONGLONG kRetryGapMs = 500;
static const ULONGLONG kRestoreGapMs = 150;
static const ULONGLONG kPopupGapMs = 150;
static const ULONGLONG kRaiseLogGapMs = 500;
static const ULONGLONG kAutoHideRefreshMs = 1000;
static const int kMaxLogLines = 500;

static std::atomic<int> g_logLines{0};

static void LoadSettings() {
    g_includeSecondary = Wh_GetIntSetting(L"includeSecondary") != 0;
    int interval = Wh_GetIntSetting(L"intervalMs");
    if (interval < 10) interval = 10;
    if (interval > 1000) interval = 1000;
    g_intervalMs = interval;
    g_debugLog = Wh_GetIntSetting(L"debugLog") != 0;
}

// Writes a line to the Windhawk log, and to a file in the temp folder when the
// debug log setting is on.
static void LogLine(PCWSTR format, ...) {
    WCHAR text[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf(text, ARRAYSIZE(text) - 1, format, args);
    va_end(args);
    text[ARRAYSIZE(text) - 1] = L'\0';
    Wh_Log(L"%s", text);

    if (!g_debugLog || g_logLines.fetch_add(1) >= kMaxLogLines) return;
    WCHAR temp[MAX_PATH];
    DWORD n = GetTempPathW(ARRAYSIZE(temp), temp);
    if (!n || n >= ARRAYSIZE(temp)) return;
    WCHAR path[MAX_PATH + 40];
    _snwprintf(path, ARRAYSIZE(path) - 1, L"%staskbar-always-on-top-log.txt", temp);
    path[ARRAYSIZE(path) - 1] = L'\0';

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR line[1100];
    _snwprintf(line, ARRAYSIZE(line) - 1, L"%02u:%02u:%02u.%03u %s\r\n", st.wHour, st.wMinute,
               st.wSecond, st.wMilliseconds, text);
    line[ARRAYSIZE(line) - 1] = L'\0';
    char utf8[3400];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes <= 1) return;

    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, utf8, bytes - 1, &written, nullptr);
    CloseHandle(file);
}

// Names the module and offset of a code address, such as explorer.exe+0x1234.
static void DescribeAddress(void* address, WCHAR* out, int outLen) {
    HMODULE module = nullptr;
    WCHAR path[MAX_PATH];
    DWORD n = 0;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)address, &module) && module) {
        n = GetModuleFileNameW(module, path, ARRAYSIZE(path));
    }
    if (!n || n >= ARRAYSIZE(path)) {
        _snwprintf(out, outLen - 1, L"%p", address);
    } else {
        PCWSTR name = wcsrchr(path, L'\\');
        name = name ? name + 1 : path;
        _snwprintf(out, outLen - 1, L"%s+0x%llX", name,
                   (unsigned long long)((BYTE*)address - (BYTE*)module));
    }
    out[outLen - 1] = L'\0';
}

// Names the program that owns a process, such as explorer.exe, for the log.
static void DescribeProcess(DWORD pid, WCHAR* out, int outLen) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    WCHAR path[MAX_PATH];
    DWORD n = ARRAYSIZE(path);
    if (process && QueryFullProcessImageNameW(process, 0, path, &n) && n) {
        PCWSTR name = wcsrchr(path, L'\\');
        name = name ? name + 1 : path;
        _snwprintf(out, outLen - 1, L"%s", name);
    } else {
        _snwprintf(out, outLen - 1, L"pid %lu", pid);
    }
    out[outLen - 1] = L'\0';
    if (process) CloseHandle(process);
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

// True for explorer popups the taskbar mustn't draw over: jump lists, flyouts,
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

// True for the taskbar windows this mod handles. They must belong to this process.
static bool IsHandledTaskbar(HWND w) {
    if (!w) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId()) return false;
    wchar_t cls[32];
    int n = GetClassNameW(w, cls, ARRAYSIZE(cls));
    if (n <= 0) return false;
    if (wcscmp(cls, L"Shell_TrayWnd") == 0) return true;
    return g_includeSecondary && wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0;
}

// A cloaked window is one the desktop manager doesn't draw, such as a closed
// Start menu or a window on another virtual desktop. IsWindowVisible still says
// yes for it.
static bool IsCloaked(HWND w) {
    int cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(w, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
           cloaked != 0;
}

// True when a rect spans at least 90 percent of the monitor in both directions.
// Such a window is a screen-sized app or an overlay, never a popup.
static bool IsScreenSized(const RECT& r, const RECT& monitor) {
    LONG mw = monitor.right - monitor.left;
    LONG mh = monitor.bottom - monitor.top;
    return (r.right - r.left) * 10 >= mw * 9 && (r.bottom - r.top) * 10 >= mh * 9;
}

// Window classes that Windows and the shell use for menus, flyouts, tooltips and
// previews.
static bool IsKnownPopupClass(PCWSTR cls) {
    return wcscmp(cls, L"#32768") == 0 || wcscmp(cls, L"tooltips_class32") == 0 ||
           wcsncmp(cls, L"Xaml", 4) == 0 || wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0 ||
           wcsncmp(cls, L"TopLevelWindowForOverflow", 25) == 0 ||
           wcscmp(cls, L"TaskListThumbnailWnd") == 0 || wcscmp(cls, L"TaskListOverlayWnd") == 0 ||
           wcscmp(cls, L"NotifyIconOverflowWindow") == 0 || wcsstr(cls, L"Popup") != nullptr;
}

// One popup that overlaps a taskbar.
struct PopupInfo {
    HWND hwnd = nullptr;
    bool above = false;
    bool topmost = false;
    DWORD pid = 0;
    WCHAR cls[64] = L"";
    RECT rect{};
};

// Decides whether a window is a popup that overlaps the taskbar rect. A popup is a
// visible window that isn't cloaked, isn't a taskbar, isn't click-through, isn't
// the edge grip of a resize mod, doesn't span the screen, and is a menu, a known
// flyout class, an owned window or a borderless window without a title bar.
static bool IsPopupOverTaskbar(HWND w, const RECT& taskbarRect, const RECT& monitor,
                               PopupInfo& p) {
    if (IsHandledTaskbar(w)) return false;
    if (!IsWindowVisible(w) || IsIconic(w)) return false;
    RECT r{};
    if (!GetWindowRect(w, &r)) return false;
    if (r.right <= r.left || r.bottom <= r.top) return false;
    RECT inter{};
    if (!IntersectRect(&inter, &r, &taskbarRect)) return false;
    if (IsCloaked(w)) return false;
    WCHAR cls[64] = L"";
    GetClassNameW(w, cls, ARRAYSIZE(cls));
    if (wcscmp(cls, L"WindhawkTaskbarRowsGrip") == 0) return false;
    LONG_PTR exStyle = GetWindowLongPtrW(w, GWL_EXSTYLE);
    if (exStyle & WS_EX_TRANSPARENT) return false;
    if (IsScreenSized(r, monitor)) return false;
    LONG_PTR style = GetWindowLongPtrW(w, GWL_STYLE);
    bool borderless = (style & WS_POPUP) != 0 && (style & WS_CAPTION) != WS_CAPTION;
    bool owned = GetWindow(w, GW_OWNER) != nullptr;
    if (!IsKnownPopupClass(cls) && !borderless && !owned) return false;

    p.hwnd = w;
    p.topmost = (exStyle & WS_EX_TOPMOST) != 0;
    GetWindowThreadProcessId(w, &p.pid);
    wcscpy(p.cls, cls);
    p.rect = r;
    return true;
}

// What one scan of the window stack found around a taskbar.
struct PopupScan {
    int above = 0;
    int belowTopmost = 0;
    // The lowest topmost popup that sits under the taskbar. The taskbar goes below it.
    HWND lowestBelow = nullptr;
    std::vector<PopupInfo> popups;
    bool any() const { return above > 0 || belowTopmost > 0; }
};

// Walks the windows above the taskbar, and the topmost windows below it, and
// collects the popups that overlap the taskbar rect. A popup that isn't topmost
// and sits below the taskbar can't be hurt by a raise, so it isn't collected.
static void ScanPopups(HWND taskbar, const RECT& taskbarRect, PopupScan& s) {
    HMONITOR mon = MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    RECT monitor{};
    if (mon && GetMonitorInfoW(mon, &mi)) monitor = mi.rcMonitor;

    for (HWND w = GetWindow(taskbar, GW_HWNDPREV); w; w = GetWindow(w, GW_HWNDPREV)) {
        PopupInfo p;
        if (!IsPopupOverTaskbar(w, taskbarRect, monitor, p)) continue;
        p.above = true;
        s.above++;
        if (s.popups.size() < 16) s.popups.push_back(p);
    }
    for (HWND w = GetWindow(taskbar, GW_HWNDNEXT); w; w = GetWindow(w, GW_HWNDNEXT)) {
        if (!IsWindowVisible(w)) continue;
        // The topmost windows come first. The first plain window ends the band.
        if (!(GetWindowLongPtrW(w, GWL_EXSTYLE) & WS_EX_TOPMOST)) break;
        PopupInfo p;
        if (!IsPopupOverTaskbar(w, taskbarRect, monitor, p)) continue;
        p.above = false;
        s.belowTopmost++;
        s.lowestBelow = w;
        if (s.popups.size() < 16) s.popups.push_back(p);
    }
}

// Text about one popup for the log: class, program, rect and position.
static void DescribePopup(const PopupInfo& p, WCHAR* out, int outLen) {
    WCHAR program[MAX_PATH];
    DescribeProcess(p.pid, program, ARRAYSIZE(program));
    _snwprintf(out, outLen - 1, L"%s from %s at %ld,%ld,%ld,%ld, %s the taskbar%s",
               p.cls[0] ? p.cls : L"?", program, p.rect.left, p.rect.top, p.rect.right,
               p.rect.bottom, p.above ? L"above" : L"below", p.topmost ? L"" : L", not topmost");
    out[outLen - 1] = L'\0';
}

// Everything the fullscreen test looked at. The log prints it.
struct FrontWindow {
    HWND hwnd = nullptr;
    WCHAR cls[64] = L"";
    RECT rect{};
    RECT monitor{};
    LONG_PTR style = 0;
    LONG_PTR exStyle = 0;
    bool zoomed = false;
    // Answer from SHQueryUserNotificationState, or -1 when the call wasn't made.
    int notifyState = -1;
    HRESULT notifyResult = S_OK;
    bool fullscreen = false;
    PCWSTR reason = L"";
};

// The strict fullscreen test. The window in front counts as a real fullscreen app
// only when every check passes. When the test can't read something, the answer is
// "not fullscreen", so the taskbar stays on top.
static bool RealFullscreenAppInFront(HWND taskbar, FrontWindow& f) {
    f.fullscreen = false;
    f.hwnd = GetForegroundWindow();
    if (!f.hwnd) {
        f.reason = L"no window is in front";
        return false;
    }
    GetClassNameW(f.hwnd, f.cls, ARRAYSIZE(f.cls));
    f.style = GetWindowLongPtrW(f.hwnd, GWL_STYLE);
    f.exStyle = GetWindowLongPtrW(f.hwnd, GWL_EXSTYLE);
    f.zoomed = IsZoomed(f.hwnd) != FALSE;
    if (!GetWindowRect(f.hwnd, &f.rect)) {
        f.reason = L"the window rect can't be read";
        return false;
    }
    HMONITOR mon = MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (!mon || !GetMonitorInfoW(mon, &mi)) {
        f.reason = L"the monitor rect can't be read";
        return false;
    }
    f.monitor = mi.rcMonitor;

    DWORD pid = 0;
    GetWindowThreadProcessId(f.hwnd, &pid);
    // The desktop, the taskbar and its popups belong to explorer. They're never a
    // fullscreen app. A folder window can be, so it goes on to the other checks.
    if (pid == GetCurrentProcessId() && !IsFolderWindow(f.hwnd)) {
        f.reason = L"it belongs to explorer";
        return false;
    }
    if (!IsWindowVisible(f.hwnd)) {
        f.reason = L"it isn't visible";
        return false;
    }
    if (f.zoomed) {
        f.reason = L"it's maximized";
        return false;
    }
    if ((f.style & WS_CAPTION) == WS_CAPTION) {
        f.reason = L"it has a title bar";
        return false;
    }
    if (f.style & WS_THICKFRAME) {
        f.reason = L"it has a sizing frame";
        return false;
    }
    // Overlays are click-through, don't take focus, or hide from the taskbar. A
    // fullscreen app is none of that.
    if (f.exStyle & WS_EX_TRANSPARENT) {
        f.reason = L"it's a click-through overlay";
        return false;
    }
    if (f.exStyle & WS_EX_NOACTIVATE) {
        f.reason = L"it's a no-activate overlay";
        return false;
    }
    if (f.exStyle & WS_EX_TOOLWINDOW) {
        f.reason = L"it's a tool window";
        return false;
    }
    if (!EqualRect(&f.rect, &f.monitor)) {
        f.reason = L"its rect isn't the monitor rect";
        return false;
    }

    // Windows has its own view. Ask it last, since the call costs more than the
    // checks above and most windows never get this far.
    QUERY_USER_NOTIFICATION_STATE state = QUNS_ACCEPTS_NOTIFICATIONS;
    f.notifyResult = SHQueryUserNotificationState(&state);
    if (FAILED(f.notifyResult)) {
        f.reason = L"Windows couldn't say whether a fullscreen app runs";
        return false;
    }
    f.notifyState = (int)state;
    if (state != QUNS_BUSY && state != QUNS_RUNNING_D3D_FULL_SCREEN &&
        state != QUNS_PRESENTATION_MODE && state != QUNS_APP) {
        f.reason = L"Windows doesn't see a fullscreen app";
        return false;
    }
    f.fullscreen = true;
    f.reason = L"it's a real fullscreen app";
    return true;
}

// Text about the window in front for the log: class, rect, monitor, styles, state
// and the decision with its reason.
static void DescribeFront(const FrontWindow& f, WCHAR* out, int outLen) {
    if (!f.hwnd) {
        _snwprintf(out, outLen - 1, L"none (%s)", f.reason);
    } else {
        _snwprintf(out, outLen - 1,
                   L"%s at %ld,%ld,%ld,%ld on monitor %ld,%ld,%ld,%ld style=0x%08llX "
                   L"ex=0x%08llX maximized=%d notify=%d. Fullscreen: %s, %s",
                   f.cls[0] ? f.cls : L"?", f.rect.left, f.rect.top, f.rect.right,
                   f.rect.bottom, f.monitor.left, f.monitor.top, f.monitor.right,
                   f.monitor.bottom, (unsigned long long)f.style,
                   (unsigned long long)f.exStyle, f.zoomed ? 1 : 0, f.notifyState,
                   f.fullscreen ? L"yes" : L"no", f.reason);
    }
    out[outLen - 1] = L'\0';
}

// True when a call would send a taskbar window down: to the bottom of the window
// stack, or out of the topmost group.
static bool IsDemotion(HWND hWnd, HWND insertAfter, UINT flags) {
    if (flags & SWP_NOZORDER) return false;
    if (insertAfter != HWND_BOTTOM && insertAfter != HWND_NOTOPMOST) return false;
    if (g_unloading || !g_autoHide) return false;
    return IsHandledTaskbar(hWnd);
}

// True when a call would put a taskbar window at the top of the stack or of the
// topmost group. Such a call draws the taskbar over an open menu.
static bool IsRaise(HWND hWnd, HWND insertAfter, UINT flags) {
    if (flags & SWP_NOZORDER) return false;
    if (insertAfter != HWND_TOPMOST && insertAfter != HWND_TOP) return false;
    if (g_unloading || !g_autoHide) return false;
    return IsHandledTaskbar(hWnd);
}

// Logs a call that would send the taskbar down. Returns true when the mod should
// keep the taskbar where it is, which is always unless a real fullscreen app is in
// front.
static bool KeepOnTop(HWND hWnd, HWND insertAfter, PCWSTR api, void* caller) {
    FrontWindow f;
    bool fullscreen = RealFullscreenAppInFront(hWnd, f);
    WCHAR who[MAX_PATH + 32];
    DescribeAddress(caller, who, ARRAYSIZE(who));
    WCHAR front[400];
    DescribeFront(f, front, ARRAYSIZE(front));
    LogLine(L"%s from %s tried to send the taskbar %s. Window in front: %s. %s", api, who,
            insertAfter == HWND_BOTTOM ? L"to the bottom" : L"out of the topmost group", front,
            fullscreen ? L"The call goes through." : L"The taskbar stays on top.");
    return !fullscreen;
}

// Decides whether a raise call must leave the stack alone. That's the case while
// a popup overlaps the taskbar. The worker's flag answers first. When it says no,
// a quick scan of the windows around the taskbar answers, since a menu may have
// opened a moment ago.
static bool KeepUnderPopups(HWND hWnd, HWND insertAfter, PCWSTR api, void* caller) {
    if (!g_popupBlock) {
        RECT tr{};
        if (!GetWindowRect(hWnd, &tr)) return false;
        PopupScan s;
        ScanPopups(hWnd, tr, s);
        if (!s.any()) return false;
    }
    ULONGLONG now = GetTickCount64();
    ULONGLONG last = g_lastRaiseLog.load();
    if (last == 0 || now - last >= kRaiseLogGapMs) {
        g_lastRaiseLog = now;
        WCHAR who[MAX_PATH + 32];
        DescribeAddress(caller, who, ARRAYSIZE(who));
        LogLine(L"%s from %s tried to raise the taskbar %s while a popup is open. The stack stays put.",
                api, who, insertAfter == HWND_TOPMOST ? L"to the topmost group" : L"to the top");
    }
    return true;
}

using SetWindowPos_t = decltype(&SetWindowPos);
static SetWindowPos_t SetWindowPos_Original;

static BOOL WINAPI SetWindowPos_Hook(HWND hWnd, HWND insertAfter, int x, int y, int cx, int cy,
                                     UINT flags) {
    if (IsDemotion(hWnd, insertAfter, flags) &&
        KeepOnTop(hWnd, insertAfter, L"SetWindowPos", __builtin_return_address(0))) {
        flags |= SWP_NOZORDER;
    } else if (IsRaise(hWnd, insertAfter, flags) &&
               KeepUnderPopups(hWnd, insertAfter, L"SetWindowPos", __builtin_return_address(0))) {
        flags |= SWP_NOZORDER;
    }
    return SetWindowPos_Original(hWnd, insertAfter, x, y, cx, cy, flags);
}

using DeferWindowPos_t = decltype(&DeferWindowPos);
static DeferWindowPos_t DeferWindowPos_Original;

static HDWP WINAPI DeferWindowPos_Hook(HDWP info, HWND hWnd, HWND insertAfter, int x, int y,
                                       int cx, int cy, UINT flags) {
    if (IsDemotion(hWnd, insertAfter, flags) &&
        KeepOnTop(hWnd, insertAfter, L"DeferWindowPos", __builtin_return_address(0))) {
        flags |= SWP_NOZORDER;
    } else if (IsRaise(hWnd, insertAfter, flags) &&
               KeepUnderPopups(hWnd, insertAfter, L"DeferWindowPos", __builtin_return_address(0))) {
        flags |= SWP_NOZORDER;
    }
    return DeferWindowPos_Original(info, hWnd, insertAfter, x, y, cx, cy, flags);
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
// explorer.exe don't count, except File Explorer folder windows.
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

// True when a visible explorer popup above the taskbar reaches over it. Raising the
// taskbar then would draw it over the popup. A thin strip that lies fully inside
// the taskbar, such as an edge grip, doesn't count.
static bool PopupOverTaskbar(HWND taskbar, DWORD taskbarPid) {
    RECT tr{};
    if (!GetWindowRect(taskbar, &tr)) return true;
    for (HWND w = GetWindow(taskbar, GW_HWNDPREV); w; w = GetWindow(w, GW_HWNDPREV)) {
        if (!IsWindowVisible(w)) continue;
        if (!IsShellPopup(w, taskbarPid)) continue;
        RECT r{};
        if (!GetWindowRect(w, &r)) continue;
        RECT inter{};
        if (!IntersectRect(&inter, &r, &tr)) continue;
        if (EqualRect(&inter, &r)) continue;
        return true;
    }
    return false;
}

static void RaiseTaskbar(HWND taskbar) {
    // The async flag keeps this thread from waiting on the taskbar's own thread.
    SetWindowPos(taskbar, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
}

// Puts the taskbar right below a topmost popup that it covers. The taskbar stays
// in the topmost group, since the popup is in it.
static void LowerBelowPopup(HWND taskbar, HWND popup) {
    SetWindowPos(taskbar, popup, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
}

// Logs each popup the worker hasn't seen before, when the debug log is on.
static void LogNewPopups(const PopupScan& s, std::set<HWND>& seen) {
    for (const PopupInfo& p : s.popups) {
        seen.insert(p.hwnd);
        if (!g_debugLog || g_knownPopups.count(p.hwnd)) continue;
        WCHAR text[400];
        DescribePopup(p, text, ARRAYSIZE(text));
        LogLine(L"Popup over the taskbar: %s.", text);
    }
}

// One check of one taskbar. Returns true when a popup holds the taskbar back.
static bool CheckTaskbar(HWND taskbar, ULONGLONG now, std::set<HWND>& seenPopups) {
    if (!IsWindowVisible(taskbar)) return false;
    DWORD taskbarPid = 0;
    DWORD taskbarTid = GetWindowThreadProcessId(taskbar, &taskbarPid);
    if (taskbarPid != GetCurrentProcessId()) return false;
    // A real fullscreen app in front wins. Explorer moves the taskbar out of its way.
    FrontWindow f;
    if (RealFullscreenAppInFront(taskbar, f)) return false;
    if (MenuActive(taskbarTid)) return true;

    RECT tr{};
    if (!GetWindowRect(taskbar, &tr)) return false;
    PopupScan s;
    ScanPopups(taskbar, tr, s);
    LogNewPopups(s, seenPopups);

    bool blocked;
    if (s.any()) {
        g_lastPopupSeen = now;
        blocked = true;
    } else {
        blocked = g_lastPopupSeen != 0 && now - g_lastPopupSeen < kPopupGapMs;
    }

    ULONGLONG& lastAssert = g_lastAssert[taskbar];
    if (s.lowestBelow) {
        // The taskbar covers a menu. Put it right under the lowest one, at most once
        // per restore gap, so a popup that moves doesn't cause a storm of calls.
        if (lastAssert == 0 || now - lastAssert >= kRestoreGapMs) {
            lastAssert = now;
            for (const PopupInfo& p : s.popups) {
                if (p.hwnd != s.lowestBelow) continue;
                WCHAR text[400];
                DescribePopup(p, text, ARRAYSIZE(text));
                LogLine(L"The taskbar covered a popup: %s. Moved the taskbar below it.", text);
            }
            LowerBelowPopup(taskbar, s.lowestBelow);
        }
        return true;
    }
    if (blocked) return true;

    bool topmost = (GetWindowLongPtrW(taskbar, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (!topmost) {
        // The taskbar lost its topmost state by a path the hooks don't see. Give it
        // back, but never over an open jump list or flyout.
        if (PopupOverTaskbar(taskbar, taskbarPid)) return false;
        if (lastAssert != 0 && now - lastAssert < kRestoreGapMs) return false;
        lastAssert = now;
        WCHAR front[400];
        DescribeFront(f, front, ARRAYSIZE(front));
        LogLine(L"The taskbar wasn't topmost. Window in front: %s. Made it topmost again.", front);
        RaiseTaskbar(taskbar);
        return false;
    }

    RECT r{};
    if (!IsRevealed(taskbar, &r)) return false;
    if (!IsCovered(taskbar, taskbarPid, r)) return false;
    if (lastAssert != 0 && now - lastAssert < kRetryGapMs) return false;
    lastAssert = now;
    RaiseTaskbar(taskbar);
    return false;
}

static bool AutoHideOn() {
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    UINT_PTR state = SHAppBarMessage(ABM_GETSTATE, &abd);
    return (state & ABS_AUTOHIDE) != 0;
}

static DWORD WINAPI WorkerThread(LPVOID) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    DWORD pid = GetCurrentProcessId();
    ULONGLONG lastAutoHideCheck = 0;

    while (WaitForSingleObject(g_stopEvent, (DWORD)g_intervalMs.load()) == WAIT_TIMEOUT) {
        ULONGLONG now = GetTickCount64();
        if (lastAutoHideCheck == 0 || now - lastAutoHideCheck >= kAutoHideRefreshMs) {
            g_autoHide = AutoHideOn();
            lastAutoHideCheck = now;
        }
        if (!g_autoHide) {
            g_popupBlock = false;
            continue;
        }
        if (MouseButtonDown()) continue;

        bool blocked = false;
        std::set<HWND> seenPopups;
        HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (primary) blocked = CheckTaskbar(primary, now, seenPopups) || blocked;

        if (g_includeSecondary) {
            HWND secondary = nullptr;
            while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd",
                                              nullptr)) != nullptr) {
                DWORD owner = 0;
                GetWindowThreadProcessId(secondary, &owner);
                if (owner == pid) blocked = CheckTaskbar(secondary, now, seenPopups) || blocked;
            }
        }
        g_popupBlock = blocked;
        g_knownPopups.swap(seenPopups);
    }
    return 0;
}

BOOL Wh_ModInit() {
    LoadSettings();

    // If a hook can't be set, the worker thread still does its part.
    if (!Wh_SetFunctionHook((void*)SetWindowPos, (void*)SetWindowPos_Hook,
                            (void**)&SetWindowPos_Original)) {
        Wh_Log(L"Couldn't hook SetWindowPos");
    }
    if (!Wh_SetFunctionHook((void*)DeferWindowPos, (void*)DeferWindowPos_Hook,
                            (void**)&DeferWindowPos_Original)) {
        Wh_Log(L"Couldn't hook DeferWindowPos");
    }

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        Wh_Log(L"Couldn't create the stop event");
        return FALSE;
    }
    g_thread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    if (!g_thread) {
        Wh_Log(L"Couldn't start the worker thread");
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        return FALSE;
    }
    LogLine(L"Started 0.1.4: interval=%d secondary=%d", g_intervalMs.load(),
            g_includeSecondary.load() ? 1 : 0);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}

static void LeaveTaskbar(HWND taskbar) {
    if (!IsHandledTaskbar(taskbar)) return;
    if (!(GetWindowLongPtrW(taskbar, GWL_EXSTYLE) & WS_EX_TOPMOST)) return;
    FrontWindow f;
    if (!RealFullscreenAppInFront(taskbar, f)) return;
    SetWindowPos(taskbar, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
}

void Wh_ModUninit() {
    g_unloading = true;
    if (g_stopEvent) SetEvent(g_stopEvent);
    if (g_thread) {
        if (WaitForSingleObject(g_thread, 5000) == WAIT_TIMEOUT) {
            Wh_Log(L"The worker thread didn't stop in time");
        }
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }

    // The taskbar stays on top, the Windows default. Only when a real fullscreen app
    // is in front does the mod hand the taskbar back to explorer's choice.
    LeaveTaskbar(FindWindowW(L"Shell_TrayWnd", nullptr));
    HWND secondary = nullptr;
    while ((secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        LeaveTaskbar(secondary);
    }
    g_lastAssert.clear();
    g_knownPopups.clear();
    g_popupBlock = false;
}
