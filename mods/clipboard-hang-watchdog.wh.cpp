// ==WindhawkMod==
// @id              clipboard-hang-watchdog
// @name            Clipboard Hang Watchdog
// @description     Finds the program that holds the clipboard open or blocks it, logs it, and can end it
// @version         0.1.0
// @author          Shaerif
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Clipboard Hang Watchdog
Copy and paste can freeze for every program when one program misbehaves.
Windows lets only one program hold the clipboard open at a time. If a program
opens it and never closes it, everyone else is locked out.

This mod checks the clipboard twice a second and catches two cases:

1. A window keeps the clipboard open longer than the chosen time.
2. The program that owns the copied data stops responding. Other programs then
   wait for it, up to 30 seconds, when they paste.

Each case writes a line to a log file with the program name, process ID and
window title, so you can see who caused it. The log is saved to
`%LOCALAPPDATA%\clipboard-hang-watchdog.log`.

The mod can also end the program that holds the clipboard open. This is off by
default. Ending a program loses its unsaved work, so turn it on only if you
accept that. Programs on the protected list are never ended.

## Limits
- Windows reports the holder only when it opened the clipboard with a window
  handle. A program that opened it with no window shows up as unknown, and the
  mod can't name it.
- If the Windows Clipboard User Service is stuck, this mod can't see it. The
  signs are a frozen Win+V history and new copies not appearing. Sign out and
  back in to reset that service.

## Settings
- Seconds before a hold is reported.
- Action: log only, or log and end the holding program.
- Seconds before the holding program is ended (action 2 only).
- Log size limit: when the log passes this size it moves to a .old file and a new log starts. Set 0 to keep everything.
- Protected programs: names, separated by commas, that are never ended.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- thresholdSeconds: 5
  $name: Seconds before a hold is reported
- action: logOnly
  $name: Action
  $options:
  - logOnly: Log only
  - endHolder: Log and end the holding program
- killAfterSeconds: 15
  $name: Seconds before the holding program is ended
- maxLogKB: 100
  $name: Log size limit in KB (0 = no limit)
- protectedProcesses: explorer.exe,dwm.exe,csrss.exe,winlogon.exe,svchost.exe,sihost.exe,services.exe,lsass.exe
  $name: Protected programs (comma separated)
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <string>

static int g_thresholdSec = 5;
static int g_action = 1;
static int g_killAfterSec = 15;
static int g_maxLogKB = 100;
static std::wstring g_protected;

static HANDLE g_stop = nullptr;
static HANDLE g_thread = nullptr;

static void LoadSettings() {
    g_thresholdSec = Wh_GetIntSetting(L"thresholdSeconds");
    if (g_thresholdSec < 1) g_thresholdSec = 1;
    PCWSTR a = Wh_GetStringSetting(L"action");
    g_action = (a && wcscmp(a, L"endHolder") == 0) ? 2 : 1;
    Wh_FreeStringSetting(a);
    g_killAfterSec = Wh_GetIntSetting(L"killAfterSeconds");
    if (g_killAfterSec < g_thresholdSec) g_killAfterSec = g_thresholdSec;
    g_maxLogKB = Wh_GetIntSetting(L"maxLogKB");
    PCWSTR p = Wh_GetStringSetting(L"protectedProcesses");
    g_protected = p ? p : L"";
    Wh_FreeStringSetting(p);
    for (auto& c : g_protected) c = (wchar_t)towlower(c);
}

static void Log(PCWSTR fmt, ...) {
    wchar_t msg[600];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(msg, 599, fmt, ap);
    va_end(ap);
    msg[599] = 0;
    Wh_Log(L"%s", msg);

    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    std::wstring path = std::wstring(dir) + L"\\clipboard-hang-watchdog.log";
    if (g_maxLogKB > 0) {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa) &&
            fa.nFileSizeHigh == 0 &&
            fa.nFileSizeLow > (DWORD)g_maxLogKB * 1024) {
            std::wstring old = path + L".old";
            MoveFileExW(path.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    int h12 = t.wHour % 12;
    if (h12 == 0) h12 = 12;
    wchar_t line[700];
    int len = swprintf(line, 700, L"%04d-%02d-%02d %d:%02d:%02d %s  %s\r\n",
                       t.wYear, t.wMonth, t.wDay, h12, t.wMinute, t.wSecond,
                       t.wHour < 12 ? L"AM" : L"PM", msg);
    if (len > 0) {
        // Write as UTF-8.
        char buf[2200];
        int bytes = WideCharToMultiByte(CP_UTF8, 0, line, len, buf, sizeof(buf),
                                        nullptr, nullptr);
        DWORD written;
        if (bytes > 0) WriteFile(f, buf, bytes, &written, nullptr);
    }
    CloseHandle(f);
}

struct WinInfo {
    DWORD pid = 0;
    std::wstring name = L"?";
    wchar_t title[120] = L"";
};

static WinInfo Describe(HWND h) {
    WinInfo w;
    GetWindowThreadProcessId(h, &w.pid);
    GetWindowTextW(h, w.title, 120);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, w.pid);
    if (p) {
        wchar_t path[MAX_PATH];
        DWORD sz = MAX_PATH;
        if (QueryFullProcessImageNameW(p, 0, path, &sz)) {
            std::wstring s(path);
            size_t i = s.find_last_of(L'\\');
            w.name = (i == std::wstring::npos) ? s : s.substr(i + 1);
        }
        CloseHandle(p);
    }
    return w;
}

static bool IsProtected(const std::wstring& name) {
    std::wstring n = name;
    for (auto& c : n) c = (wchar_t)towlower(c);
    size_t start = 0;
    while (start <= g_protected.size()) {
        size_t end = g_protected.find(L',', start);
        if (end == std::wstring::npos) end = g_protected.size();
        std::wstring item = g_protected.substr(start, end - start);
        while (!item.empty() && item.front() == L' ') item.erase(0, 1);
        while (!item.empty() && item.back() == L' ') item.pop_back();
        if (!item.empty() && item == n) return true;
        start = end + 1;
    }
    return false;
}

static void EndHolder(HWND h) {
    WinInfo w = Describe(h);
    if (w.pid == 0 || w.pid == GetCurrentProcessId() || IsProtected(w.name)) {
        Log(L"Not ending %s (pid %lu): protected program", w.name.c_str(),
            w.pid);
        return;
    }
    HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, w.pid);
    if (!p) {
        Log(L"Could not open %s (pid %lu) to end it, error %lu", w.name.c_str(),
            w.pid, GetLastError());
        return;
    }
    BOOL ok = TerminateProcess(p, 1);
    Log(L"Ended %s (pid %lu): %s", w.name.c_str(), w.pid,
        ok ? L"done" : L"failed");
    CloseHandle(p);
}

static DWORD WINAPI Watch(LPVOID) {
    HWND heldHwnd = nullptr;
    ULONGLONG heldSince = 0;
    bool heldReported = false, heldEnded = false;
    HWND hungHwnd = nullptr;
    ULONGLONG hungSince = 0;
    bool hungReported = false;

    while (WaitForSingleObject(g_stop, 500) == WAIT_TIMEOUT) {
        ULONGLONG now = GetTickCount64();

        HWND h = GetOpenClipboardWindow();
        if (h) {
            if (h != heldHwnd) {
                heldHwnd = h;
                heldSince = now;
                heldReported = false;
                heldEnded = false;
            }
            ULONGLONG sec = (now - heldSince) / 1000;
            if (!heldReported && sec >= (ULONGLONG)g_thresholdSec) {
                heldReported = true;
                WinInfo w = Describe(h);
                Log(L"HELD OPEN %llus by %s (pid %lu) window \"%s\"", sec,
                    w.name.c_str(), w.pid, w.title);
            }
            if (g_action == 2 && !heldEnded &&
                sec >= (ULONGLONG)g_killAfterSec) {
                heldEnded = true;
                EndHolder(h);
            }
        } else {
            if (heldHwnd && heldReported) {
                Log(L"Clipboard released after %llus",
                    (now - heldSince) / 1000);
            }
            heldHwnd = nullptr;
        }

        HWND o = GetClipboardOwner();
        if (o && IsHungAppWindow(o)) {
            if (o != hungHwnd) {
                hungHwnd = o;
                hungSince = now;
                hungReported = false;
            }
            ULONGLONG sec = (now - hungSince) / 1000;
            if (!hungReported && sec >= (ULONGLONG)g_thresholdSec) {
                hungReported = true;
                WinInfo w = Describe(o);
                Log(L"OWNER NOT RESPONDING for %llus: %s (pid %lu) window "
                    L"\"%s\"",
                    sec, w.name.c_str(), w.pid, w.title);
            }
        } else {
            if (hungHwnd && hungReported) Log(L"Clipboard owner responding again");
            hungHwnd = nullptr;
        }
    }
    return 0;
}

BOOL Wh_ModInit() {
    LoadSettings();
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stop) return FALSE;
    g_thread = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr);
    if (!g_thread) {
        CloseHandle(g_stop);
        g_stop = nullptr;
        return FALSE;
    }
    Log(L"Watchdog started (report after %ds, action %d)", g_thresholdSec,
        g_action);
    return TRUE;
}

void Wh_ModSettingsChanged() { LoadSettings(); }

void Wh_ModUninit() {
    if (g_stop) SetEvent(g_stop);
    if (g_thread) {
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_stop) {
        CloseHandle(g_stop);
        g_stop = nullptr;
    }
}
