// ==WindhawkMod==
// @id              same-app-window-cycler
// @name            Same-App Window Cycler
// @description     Press Alt+` to cycle between the windows of the app you're using, like Cmd+` on a Mac
// @version         0.1.0
// @author          Shaerif
// @include         explorer.exe
// @compilerOptions -luser32 -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Same-App Window Cycler
Alt+Tab mixes the windows of every app. A Mac has a second key, Cmd+`, that cycles
only between the windows of the current app. Linux desktops have the same idea.
Windows has nothing like it. This mod adds it.

## Keys
- Alt+` goes to the next window of the current app.
- Alt+Shift+` goes the other way.

You can change the modifier in the settings.

## Rules
- Only windows of the same program file count. Two Notepad windows cycle. Notepad
  and Word don't.
- Hidden, tool and cloaked windows are skipped. Windows on other virtual desktops
  are skipped when "current virtual desktop only" is on.
- Minimized windows are restored when you cycle to them, if you allow it.
- If the app has one window, nothing happens.

## Limits
- Store apps share one frame host, so they all look like the same app. The mod
  skips them.
- A window that runs as administrator can't be switched to from Explorer.

## Tutorial
The ` key is the one above Tab on a US keyboard.

1. Open two or more windows of one program and click one so it's in front.
   ![Three overlapping test windows. Window A is in front with its title bar active, Window B sits behind it and Window C is at the back](https://raw.githubusercontent.com/Shaerif/windhawk-mods/main/docs/screenshots/same-app-window-cycler-step-1-window-a-in-front.png)
2. Press Alt+` once. The window furthest back comes to the front.
   ![The same three windows after one press. Window C is now in front, and Window A's title text has turned gray](https://raw.githubusercontent.com/Shaerif/windhawk-mods/main/docs/screenshots/same-app-window-cycler-step-2-first-press-window-c.png)
3. Press Alt+` again. The window that's now furthest back comes to the front.
   ![The same three windows after a second press, with Window B now in front](https://raw.githubusercontent.com/Shaerif/windhawk-mods/main/docs/screenshots/same-app-window-cycler-step-3-second-press-window-b.png)
4. Press Alt+Shift+` to go the other way.

### Gotchas
- Step 1: A window with no taskbar button may have a hidden owner window. The
  mod skips any window that has an owner.
- Step 2: If you hold the keys down, the switch doesn't repeat. You get one
  switch per press.
- Step 3: If the app has only one window that counts, nothing happens and the
  log stays quiet.
- Step 4: With three or more windows, the code always brings the second window
  in the stack to the front. So more presses flip between the same two windows.
  No test covers this step.

Read the full guide in
[docs/same-app-window-cycler.md](https://github.com/Shaerif/windhawk-mods/blob/main/docs/same-app-window-cycler.md).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- modifier: alt
  $name: Modifier key
  $options:
  - alt: Alt
  - ctrl_alt: Ctrl+Alt
  - win: Win
- includeMinimized: true
  $name: Include minimized windows
- currentDesktopOnly: true
  $name: Current virtual desktop only
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <dwmapi.h>

#include <algorithm>
#include <string>
#include <vector>

#define WM_APP_QUIT (WM_APP + 1)

struct {
    UINT modifiers;
    bool includeMinimized;
    bool currentDesktopOnly;
} g_settings;

static HANDLE g_thread = nullptr;
static DWORD g_threadId = 0;
static HWND g_msgWnd = nullptr;
static HANDLE g_readyEvent = nullptr;

static void LoadSettings() {
    PCWSTR mod = Wh_GetStringSetting(L"modifier");
    UINT m = MOD_ALT;
    if (mod && wcscmp(mod, L"ctrl_alt") == 0) m = MOD_CONTROL | MOD_ALT;
    else if (mod && wcscmp(mod, L"win") == 0) m = MOD_WIN;
    Wh_FreeStringSetting(mod);
    g_settings.modifiers = m | MOD_NOREPEAT;
    g_settings.includeMinimized = Wh_GetIntSetting(L"includeMinimized") != 0;
    g_settings.currentDesktopOnly = Wh_GetIntSetting(L"currentDesktopOnly") != 0;
}

static bool GetImagePath(HWND hWnd, std::wstring& out) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    if (!pid) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    wchar_t buf[MAX_PATH * 2];
    DWORD len = ARRAYSIZE(buf);
    bool ok = QueryFullProcessImageNameW(h, 0, buf, &len) != 0;
    CloseHandle(h);
    if (ok) out.assign(buf, len);
    return ok;
}

static bool EndsWithNoCase(const std::wstring& s, const wchar_t* tail) {
    size_t n = wcslen(tail);
    if (s.size() < n) return false;
    return _wcsicmp(s.c_str() + s.size() - n, tail) == 0;
}

static bool IsCyclable(HWND hWnd) {
    if (GetWindow(hWnd, GW_OWNER)) return false;
    LONG_PTR ex = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) return false;
    if (GetWindowTextLengthW(hWnd) == 0) return false;
    BOOL iconic = IsIconic(hWnd);
    if (iconic) {
        if (!g_settings.includeMinimized) return false;
    } else if (!IsWindowVisible(hWnd)) {
        return false;
    }
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        // DWM_CLOAKED_SHELL (2) marks a window on another virtual desktop.
        bool onOtherDesktop = (cloaked == 2);
        if (g_settings.currentDesktopOnly || !onOtherDesktop) return false;
    }
    return true;
}

struct EnumCtx {
    std::wstring path;
    std::vector<HWND> windows;
};

static BOOL CALLBACK EnumProc(HWND hWnd, LPARAM lParam) {
    EnumCtx* ctx = (EnumCtx*)lParam;
    if (!IsCyclable(hWnd)) return TRUE;
    std::wstring p;
    if (!GetImagePath(hWnd, p)) return TRUE;
    if (_wcsicmp(p.c_str(), ctx->path.c_str()) != 0) return TRUE;
    ctx->windows.push_back(hWnd);
    return TRUE;
}

static void ForceForeground(HWND hWnd) {
    if (IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE);
    HWND fg = GetForegroundWindow();
    DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    DWORD me = GetCurrentThreadId();
    bool attached = false;
    if (fgThread && fgThread != me) {
        attached = AttachThreadInput(me, fgThread, TRUE) != 0;
    }
    BringWindowToTop(hWnd);
    SetForegroundWindow(hWnd);
    if (attached) AttachThreadInput(me, fgThread, FALSE);
}

static void Cycle(bool forward) {
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    EnumCtx ctx;
    if (!GetImagePath(fg, ctx.path)) return;
    if (EndsWithNoCase(ctx.path, L"\\ApplicationFrameHost.exe")) return;
    if (EndsWithNoCase(ctx.path, L"\\explorer.exe") && !IsCyclable(fg)) return;

    EnumWindows(EnumProc, (LPARAM)&ctx);
    if (ctx.windows.size() < 2) return;

    // EnumWindows lists windows from the top of the stack down. The front window
    // is first. Forward brings the bottom one up. Backward brings the second one up.
    HWND target = forward ? ctx.windows.back() : ctx.windows[1];
    if (target == fg) return;
    Wh_Log(L"Cycling %zu windows %s", ctx.windows.size(), forward ? L"forward" : L"backward");
    ForceForeground(target);
}

static LRESULT CALLBACK MsgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_HOTKEY:
            if (wParam == 1) Cycle(true);
            else if (wParam == 2) Cycle(false);
            return 0;
        case WM_DESTROY:
            UnregisterHotKey(hWnd, 1);
            UnregisterHotKey(hWnd, 2);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void RegisterKeys(HWND hWnd) {
    UnregisterHotKey(hWnd, 1);
    UnregisterHotKey(hWnd, 2);
    if (!RegisterHotKey(hWnd, 1, g_settings.modifiers, VK_OEM_3)) {
        Wh_Log(L"Could not register the next-window hotkey (error %lu)", GetLastError());
    }
    if (!RegisterHotKey(hWnd, 2, g_settings.modifiers | MOD_SHIFT, VK_OEM_3)) {
        Wh_Log(L"Could not register the previous-window hotkey (error %lu)", GetLastError());
    }
}

static DWORD WINAPI ThreadProc(LPVOID) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = MsgWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WindhawkSameAppCyclerWnd";
    RegisterClassW(&wc);
    g_msgWnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                               wc.hInstance, nullptr);
    if (g_msgWnd) RegisterKeys(g_msgWnd);
    SetEvent(g_readyEvent);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_APP_QUIT) {
            if (g_msgWnd) DestroyWindow(g_msgWnd);
            continue;
        }
        if (msg.message == WM_APP) {
            if (g_msgWnd) RegisterKeys(g_msgWnd);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_msgWnd = nullptr;
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
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
    if (g_threadId) PostThreadMessageW(g_threadId, WM_APP, 0, 0);
}

void Wh_ModUninit() {
    if (g_thread) {
        PostThreadMessageW(g_threadId, WM_APP_QUIT, 0, 0);
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_readyEvent) {
        CloseHandle(g_readyEvent);
        g_readyEvent = nullptr;
    }
}
