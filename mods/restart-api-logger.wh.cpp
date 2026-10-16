// ==WindhawkMod==
// @id              restart-api-logger
// @name            Restart API Logger
// @description     Logs which Windows API Windows Update uses to restart the PC, and who called it
// @version         0.1.0
// @author          Shaerif
// @include         MoUsoCoreWorker.exe
// @include         TrustedInstaller.exe
// @compilerOptions -luser32 -ladvapi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Restart API Logger
Windows Update sometimes restarts the PC on its own. The Windows event log tells
you which program asked for the restart, but it doesn't tell you which API call
was used. This mod finds that out.

Version 1 only logs. It blocks nothing, delays nothing and changes nothing. Every
hooked call goes straight on to the real function with the same arguments, and
you get the real result back.

## What it hooks
The mod loads into two Windows Update programs, MoUsoCoreWorker.exe and
TrustedInstaller.exe. It hooks every shutdown or restart API it finds in the
process:
- ExitWindowsEx
- InitiateSystemShutdownExW, InitiateSystemShutdownExA, InitiateSystemShutdownW
- InitiateShutdownW
- NtShutdownSystem, NtSetSystemPowerState, NtInitiatePowerAction
- SetSuspendState

If an API isn't in the process, the mod skips it and says so in the log.

## What it logs
For each call it logs the API name, the reason code and flags in hex, and who
called it. In detailed mode it also logs the process path, the process and
thread IDs, every argument, the return address, and the caller module with its
offset. It logs the result too, if the process is still alive when the call
returns.

## Check that it loaded
When the mod loads into a process, it logs one line with the process path and
the lists of hooked and skipped APIs. If you never see that line for
TrustedInstaller.exe or MoUsoCoreWorker.exe, Windhawk didn't inject the mod into
that process. Windhawk protects some system processes, and this isn't verified
for these two.

## Where the log goes
The mod writes to the Windhawk log. A restart can kill the process before that
log is saved, so it also appends each line to a file:

`%ProgramData%\restart-api-logger.log`

The mod writes and closes the file before it calls the real function. The file
is capped at 256 KB. When it gets bigger, the mod drops the oldest lines.

## Settings
- Log detail: basic logs the process, API, reason and flags. Detailed logs
  everything.
- Also write a log file: turns the ProgramData file on or off.

## Tutorial
1. Open `%ProgramData%\restart-api-logger.log` and find the INIT line for each
   program.
   ![The restart logger log file in a plain text window with two INIT lines, both from MoUsoCoreWorker.exe, each listing nine hooked APIs, skipped=[none] and the log file path](https://raw.githubusercontent.com/Shaerif/windhawk-mods/main/docs/screenshots/restart-api-logger-step-1-log-file.png)
2. Leave the mod on until Windows Update restarts your PC. Nothing shows on
   screen while it waits.
3. After the restart, open the file again and find the CALL lines. Each one
   names the API, the program, the reason code and flags in hex, and the module
   that made the call.
4. Check the line right after each CALL line for a RETURN line with the result.

Our test log has no restart in it yet, so steps 2 to 4 come from the code.

### Gotchas
- Step 1: No INIT line for a program means Windhawk didn't load the mod into it.
  On our test PC it loaded into both.
- Step 2: You get a new INIT line each time Windhawk loads the mod into a new
  copy of either program. Our file held 64 of them by the evening of October
  16, 2026.
- Step 3: With Also write a log file in ProgramData off, the CALL line went to
  the Windhawk log only, and a restart can cut that log off.
- Step 4: A missing RETURN line isn't a failure. A restart can end the program
  first, and the CALL line is already on disk by then.

Read the full guide in
[docs/restart-api-logger.md](https://github.com/Shaerif/windhawk-mods/blob/main/docs/restart-api-logger.md).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- logLevel: detailed
  $name: Log detail
  $options:
  - basic: Basic (process, API, reason, flags)
  - detailed: Detailed (everything)
- fileLog: true
  $name: Also write a log file in ProgramData
*/
// ==/WindhawkModSettings==

#include <windows.h>

#include <cstdio>
#include <cwchar>

static const wchar_t kLogFileName[] = L"restart-api-logger.log";
static const DWORD kMaxLogBytes = 256 * 1024;
static const DWORD kKeepLogBytes = 192 * 1024;

static volatile LONG g_detailed = 1;
static volatile LONG g_fileLog = 1;

static wchar_t g_procPath[MAX_PATH] = L"?";
static wchar_t g_logPath[MAX_PATH] = L"";
static volatile LONG g_openFailLogged = 0;

static SRWLOCK g_logLock = SRWLOCK_INIT;

// ---------------------------------------------------------------------------
// Settings

static void LoadSettings() {
    PCWSTR level = Wh_GetStringSetting(L"logLevel");
    InterlockedExchange(&g_detailed, (level && wcscmp(level, L"basic") == 0) ? 0 : 1);
    Wh_FreeStringSetting(level);
    InterlockedExchange(&g_fileLog, Wh_GetIntSetting(L"fileLog") != 0 ? 1 : 0);

    wchar_t path[MAX_PATH] = L"";
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"ProgramData", dir, MAX_PATH);
    if (n && n + 1 + ARRAYSIZE(kLogFileName) < MAX_PATH) {
        _snwprintf(path, MAX_PATH - 1, L"%s\\%s", dir, kLogFileName);
        path[MAX_PATH - 1] = 0;
    }
    AcquireSRWLockExclusive(&g_logLock);
    wcscpy(g_logPath, path);
    ReleaseSRWLockExclusive(&g_logLock);
    InterlockedExchange(&g_openFailLogged, 0);
}

// ---------------------------------------------------------------------------
// File log (call with g_logLock held)

// Drops the oldest part of the file. Copies the newest bytes to the front in
// small chunks, so it needs no heap memory.
static void TrimLogFile(HANDLE f, LONGLONG size) {
    LONGLONG src = size - kKeepLogBytes;
    if (src <= 0) return;
    char buf[4096];
    DWORD got = 0;
    LARGE_INTEGER pos;

    // Move the start to the next line break, so the file doesn't begin mid-line.
    pos.QuadPart = src;
    if (SetFilePointerEx(f, pos, nullptr, FILE_BEGIN) &&
        ReadFile(f, buf, sizeof(buf), &got, nullptr)) {
        for (DWORD i = 0; i < got; i++) {
            if (buf[i] == '\n') {
                src += i + 1;
                break;
            }
        }
    }

    LONGLONG dst = 0;
    while (src < size) {
        pos.QuadPart = src;
        if (!SetFilePointerEx(f, pos, nullptr, FILE_BEGIN)) return;
        if (!ReadFile(f, buf, sizeof(buf), &got, nullptr) || got == 0) break;
        pos.QuadPart = dst;
        if (!SetFilePointerEx(f, pos, nullptr, FILE_BEGIN)) return;
        DWORD put = 0;
        if (!WriteFile(f, buf, got, &put, nullptr) || put != got) return;
        src += got;
        dst += got;
    }
    pos.QuadPart = dst;
    if (SetFilePointerEx(f, pos, nullptr, FILE_BEGIN)) SetEndOfFile(f);
}

static void AppendToFile(const wchar_t* line) {
    if (!g_fileLog || !g_logPath[0]) return;

    HANDLE f = CreateFileW(g_logPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (InterlockedExchange(&g_openFailLogged, 1) == 0) {
            Wh_Log(L"Can't open the log file %s (error %lu). Logging to the Windhawk log only.",
                   g_logPath, err);
        }
        return;
    }

    LARGE_INTEGER size;
    if (GetFileSizeEx(f, &size) && size.QuadPart > kMaxLogBytes) {
        TrimLogFile(f, size.QuadPart);
    }

    SYSTEMTIME t;
    GetLocalTime(&t);
    int h12 = t.wHour % 12;
    if (h12 == 0) h12 = 12;
    wchar_t stamped[1400];
    int len = _snwprintf(stamped, 1399, L"%04d-%02d-%02d %d:%02d:%02d %s  %s\r\n",
                         t.wYear, t.wMonth, t.wDay, h12, t.wMinute, t.wSecond,
                         t.wHour < 12 ? L"AM" : L"PM", line);
    if (len < 0) len = 1399;
    char out[4200];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, stamped, len, out, sizeof(out), nullptr, nullptr);
    if (bytes > 0) {
        if (SetFilePointer(f, 0, nullptr, FILE_END) != INVALID_SET_FILE_POINTER) {
            DWORD written = 0;
            WriteFile(f, out, bytes, &written, nullptr);
        }
        // Push the data to disk now. The restart can kill the process right after.
        FlushFileBuffers(f);
    }
    CloseHandle(f);
}

static void Emit(const wchar_t* line) {
    Wh_Log(L"%s", line);
    AcquireSRWLockExclusive(&g_logLock);
    AppendToFile(line);
    ReleaseSRWLockExclusive(&g_logLock);
}

// ---------------------------------------------------------------------------
// Decoding helpers

static const wchar_t* BaseName(const wchar_t* path) {
    const wchar_t* s = wcsrchr(path, L'\\');
    return s ? s + 1 : path;
}

// Reads a string from the process without risking a crash on a bad pointer.
static void ReadWide(const void* p, wchar_t* out, size_t cap) {
    out[0] = 0;
    if (!p) {
        wcsncpy(out, L"(null)", cap - 1);
        out[cap - 1] = 0;
        return;
    }
    size_t toPage = 4096 - ((ULONG_PTR)p & 4095);
    size_t bytes = (cap - 1) * sizeof(wchar_t);
    if (bytes > toPage) bytes = toPage & ~(size_t)1;
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), p, out, bytes, &got) && got == 0) {
        wcsncpy(out, L"(unreadable)", cap - 1);
        out[cap - 1] = 0;
        return;
    }
    size_t n = got / sizeof(wchar_t);
    if (n >= cap) n = cap - 1;
    out[n] = 0;
}

static void ReadAnsi(const void* p, wchar_t* out, size_t cap) {
    out[0] = 0;
    if (!p) {
        wcsncpy(out, L"(null)", cap - 1);
        out[cap - 1] = 0;
        return;
    }
    char tmp[160];
    size_t cnt = cap - 1;
    if (cnt > sizeof(tmp)) cnt = sizeof(tmp);
    size_t toPage = 4096 - ((ULONG_PTR)p & 4095);
    if (cnt > toPage) cnt = toPage;
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), p, tmp, cnt, &got) && got == 0) {
        wcsncpy(out, L"(unreadable)", cap - 1);
        out[cap - 1] = 0;
        return;
    }
    size_t n = 0;
    while (n < got && tmp[n]) n++;
    int w = MultiByteToWideChar(CP_ACP, 0, tmp, (int)n, out, (int)cap - 1);
    out[w > 0 ? w : 0] = 0;
}

struct FlagName {
    DWORD bit;
    const wchar_t* name;
};

// Writes the names of the set bits, like "A|B". Unknown bits show as hex.
static void DecodeFlags(DWORD value, const FlagName* table, size_t count, wchar_t* out, size_t cap) {
    out[0] = 0;
    size_t used = 0;
    DWORD known = 0;
    for (size_t i = 0; i < count; i++) {
        if (table[i].bit && (value & table[i].bit) == table[i].bit) {
            known |= table[i].bit;
            int n = _snwprintf(out + used, cap - used - 1, L"%s%s", used ? L"|" : L"", table[i].name);
            if (n < 0) break;
            used += n;
        }
    }
    DWORD rest = value & ~known;
    if (rest && used + 14 < cap) {
        _snwprintf(out + used, cap - used - 1, L"%s0x%lX", used ? L"|" : L"", rest);
    } else if (!used) {
        wcsncpy(out, L"none", cap - 1);
    }
    out[cap - 1] = 0;
}

static const FlagName kEwxFlags[] = {
    {0x00000001, L"EWX_SHUTDOWN"},
    {0x00000002, L"EWX_REBOOT"},
    {0x00000004, L"EWX_FORCE"},
    {0x00000008, L"EWX_POWEROFF"},
    {0x00000010, L"EWX_FORCEIFHUNG"},
    {0x00000020, L"EWX_QUICKRESOLVE"},
    {0x00000040, L"EWX_RESTARTAPPS"},
    {0x00400000, L"EWX_HYBRID_SHUTDOWN"},
    {0x01000000, L"EWX_BOOTOPTIONS"},
};

static const FlagName kShutdownFlags[] = {
    {0x00000001, L"FORCE_OTHERS"},
    {0x00000002, L"FORCE_SELF"},
    {0x00000004, L"RESTART"},
    {0x00000008, L"POWEROFF"},
    {0x00000010, L"NOREBOOT"},
    {0x00000020, L"GRACE_OVERRIDE"},
    {0x00000040, L"INSTALL_UPDATES"},
    {0x00000080, L"RESTARTAPPS"},
    {0x00000100, L"SKIP_SVC_PRESHUTDOWN"},
    {0x00000200, L"HYBRID"},
    {0x00000400, L"RESTART_BOOTOPTIONS"},
    {0x00000800, L"SOFT_REBOOT"},
    {0x00001000, L"MOBILE_UI"},
    {0x00002000, L"ARSO"},
};

static const FlagName kReasonFlags[] = {
    {0x80000000, L"PLANNED"},
    {0x40000000, L"USER_DEFINED"},
    {0x08000000, L"DIRTY_UI"},
    {0x04000000, L"CLEAN_UI"},
    {0x02000000, L"DIRTY_PROBLEM_ID_REQUIRED"},
    {0x01000000, L"COMMENT_REQUIRED"},
};

// Writes "major=0xNN minor=0xNNNN flags=PLANNED|..." for a shutdown reason code.
static void DecodeReason(DWORD reason, wchar_t* out, size_t cap) {
    wchar_t fl[160];
    DecodeFlags(reason & 0xFF000000, kReasonFlags, ARRAYSIZE(kReasonFlags), fl, ARRAYSIZE(fl));
    _snwprintf(out, cap - 1, L"major=0x%02lX minor=0x%04lX %s",
               (reason >> 16) & 0xFF, reason & 0xFFFF, fl);
    out[cap - 1] = 0;
}

static const wchar_t* PowerActionName(DWORD v) {
    switch (v) {
        case 0: return L"PowerActionNone";
        case 1: return L"PowerActionReserved";
        case 2: return L"PowerActionSleep";
        case 3: return L"PowerActionHibernate";
        case 4: return L"PowerActionShutdown";
        case 5: return L"PowerActionShutdownReset";
        case 6: return L"PowerActionShutdownOff";
        case 7: return L"PowerActionWarmEject";
        case 8: return L"PowerActionDisplayOff";
    }
    return L"unknown";
}

static const wchar_t* PowerStateName(DWORD v) {
    switch (v) {
        case 0: return L"PowerSystemUnspecified";
        case 1: return L"PowerSystemWorking";
        case 2: return L"PowerSystemSleeping1";
        case 3: return L"PowerSystemSleeping2";
        case 4: return L"PowerSystemSleeping3";
        case 5: return L"PowerSystemHibernate";
        case 6: return L"PowerSystemShutdown";
    }
    return L"unknown";
}

static const wchar_t* ShutdownActionName(DWORD v) {
    switch (v) {
        case 0: return L"ShutdownNoReboot";
        case 1: return L"ShutdownReboot";
        case 2: return L"ShutdownPowerOff";
    }
    return L"unknown";
}

// ---------------------------------------------------------------------------
// Call log

struct CallRecord {
    const wchar_t* api;
    bool hasReason;
    DWORD reason;
    bool hasFlags;
    DWORD flags;
    wchar_t args[520];
};

// Finds the module that holds an address. Returns false if there isn't one.
static bool CallerModule(void* addr, wchar_t* name, size_t cap, unsigned long* offset) {
    HMODULE mod = nullptr;
    wchar_t path[MAX_PATH];
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)addr, &mod) || !mod) {
        return false;
    }
    if (!GetModuleFileNameW(mod, path, MAX_PATH)) return false;
    wcsncpy(name, BaseName(path), cap - 1);
    name[cap - 1] = 0;
    *offset = (unsigned long)((ULONG_PTR)addr - (ULONG_PTR)mod);
    return true;
}

static void LogCall(const CallRecord& c, void* retAddr) {
    wchar_t line[1300];
    size_t used = 0;
    auto add = [&](const wchar_t* fmt, auto... a) {
        if (used >= ARRAYSIZE(line) - 1) return;
        int n = _snwprintf(line + used, ARRAYSIZE(line) - used - 1, fmt, a...);
        if (n > 0) used += n;
        line[used] = 0;
    };
    line[0] = 0;

    bool detailed = g_detailed != 0;
    add(L"CALL %s proc=%s", c.api, detailed ? g_procPath : BaseName(g_procPath));
    if (detailed) add(L" pid=%lu tid=%lu", GetCurrentProcessId(), GetCurrentThreadId());
    if (c.hasReason) {
        wchar_t rs[260];
        DecodeReason(c.reason, rs, ARRAYSIZE(rs));
        add(L" reason=0x%08lX (%s)", c.reason, rs);
    } else {
        add(L" reason=n/a");
    }
    if (c.hasFlags) {
        add(L" flags=0x%08lX", c.flags);
    } else {
        add(L" flags=n/a");
    }

    wchar_t modName[MAX_PATH];
    unsigned long modOffset = 0;
    bool haveMod = CallerModule(retAddr, modName, ARRAYSIZE(modName), &modOffset);
    if (detailed) {
        add(L" args={%s}", c.args);
        if (haveMod) {
            add(L" ret_addr=%p caller=%s+0x%lX", retAddr, modName, modOffset);
        } else {
            add(L" ret_addr=%p caller=(unknown module)", retAddr);
        }
    } else if (haveMod) {
        // Basic mode still names the caller module, because it's the key clue.
        add(L" caller=%s", modName);
    }
    Emit(line);
}

static void LogResult(const wchar_t* api, ULONG_PTR value, const wchar_t* extra) {
    wchar_t line[400];
    if (g_detailed) {
        _snwprintf(line, ARRAYSIZE(line) - 1, L"RETURN %s pid=%lu tid=%lu result=0x%lX %s",
                   api, GetCurrentProcessId(), GetCurrentThreadId(), (unsigned long)value, extra);
    } else {
        _snwprintf(line, ARRAYSIZE(line) - 1, L"RETURN %s result=0x%lX", api, (unsigned long)value);
    }
    line[ARRAYSIZE(line) - 1] = 0;
    Emit(line);
}

#define CALLER() __builtin_return_address(0)

// ---------------------------------------------------------------------------
// Hooks. Each one logs, calls the original with the same arguments, and
// returns the original result unchanged.

typedef BOOL(WINAPI* ExitWindowsEx_t)(UINT, DWORD);
static ExitWindowsEx_t ExitWindowsEx_Original;
static BOOL WINAPI ExitWindowsEx_Hook(UINT flags, DWORD reason) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"ExitWindowsEx";
    c.hasReason = true;
    c.reason = reason;
    c.hasFlags = true;
    c.flags = flags;
    wchar_t fl[300];
    DecodeFlags(flags, kEwxFlags, ARRAYSIZE(kEwxFlags), fl, ARRAYSIZE(fl));
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1, L"uFlags=0x%X (%s), dwReason=0x%lX", flags, fl, reason);
    LogCall(c, ra);
    BOOL r = ExitWindowsEx_Original(flags, reason);
    DWORD err = GetLastError();
    LogResult(L"ExitWindowsEx", (ULONG_PTR)r, r ? L"(TRUE)" : L"(FALSE)");
    SetLastError(err);
    return r;
}

typedef BOOL(WINAPI* InitiateSystemShutdownExW_t)(LPWSTR, LPWSTR, DWORD, BOOL, BOOL, DWORD);
static InitiateSystemShutdownExW_t InitiateSystemShutdownExW_Original;
static BOOL WINAPI InitiateSystemShutdownExW_Hook(LPWSTR machine, LPWSTR message, DWORD timeout,
                                                  BOOL force, BOOL reboot, DWORD reason) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"InitiateSystemShutdownExW";
    c.hasReason = true;
    c.reason = reason;
    c.hasFlags = false;
    wchar_t m[64], msg[128];
    ReadWide(machine, m, ARRAYSIZE(m));
    ReadWide(message, msg, ARRAYSIZE(msg));
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1,
               L"machine=\"%s\", message=\"%s\", timeout=%lu, forceAppsClosed=%s, rebootAfterShutdown=%s, reason=0x%lX",
               m, msg, timeout, force ? L"TRUE" : L"FALSE", reboot ? L"TRUE" : L"FALSE", reason);
    LogCall(c, ra);
    BOOL r = InitiateSystemShutdownExW_Original(machine, message, timeout, force, reboot, reason);
    DWORD err = GetLastError();
    LogResult(L"InitiateSystemShutdownExW", (ULONG_PTR)r, r ? L"(TRUE)" : L"(FALSE)");
    SetLastError(err);
    return r;
}

typedef BOOL(WINAPI* InitiateSystemShutdownExA_t)(LPSTR, LPSTR, DWORD, BOOL, BOOL, DWORD);
static InitiateSystemShutdownExA_t InitiateSystemShutdownExA_Original;
static BOOL WINAPI InitiateSystemShutdownExA_Hook(LPSTR machine, LPSTR message, DWORD timeout,
                                                  BOOL force, BOOL reboot, DWORD reason) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"InitiateSystemShutdownExA";
    c.hasReason = true;
    c.reason = reason;
    c.hasFlags = false;
    wchar_t m[64], msg[128];
    ReadAnsi(machine, m, ARRAYSIZE(m));
    ReadAnsi(message, msg, ARRAYSIZE(msg));
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1,
               L"machine=\"%s\", message=\"%s\", timeout=%lu, forceAppsClosed=%s, rebootAfterShutdown=%s, reason=0x%lX",
               m, msg, timeout, force ? L"TRUE" : L"FALSE", reboot ? L"TRUE" : L"FALSE", reason);
    LogCall(c, ra);
    BOOL r = InitiateSystemShutdownExA_Original(machine, message, timeout, force, reboot, reason);
    DWORD err = GetLastError();
    LogResult(L"InitiateSystemShutdownExA", (ULONG_PTR)r, r ? L"(TRUE)" : L"(FALSE)");
    SetLastError(err);
    return r;
}

typedef BOOL(WINAPI* InitiateSystemShutdownW_t)(LPWSTR, LPWSTR, DWORD, BOOL, BOOL);
static InitiateSystemShutdownW_t InitiateSystemShutdownW_Original;
static BOOL WINAPI InitiateSystemShutdownW_Hook(LPWSTR machine, LPWSTR message, DWORD timeout,
                                                BOOL force, BOOL reboot) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"InitiateSystemShutdownW";
    c.hasReason = false;
    c.hasFlags = false;
    wchar_t m[64], msg[128];
    ReadWide(machine, m, ARRAYSIZE(m));
    ReadWide(message, msg, ARRAYSIZE(msg));
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1,
               L"machine=\"%s\", message=\"%s\", timeout=%lu, forceAppsClosed=%s, rebootAfterShutdown=%s",
               m, msg, timeout, force ? L"TRUE" : L"FALSE", reboot ? L"TRUE" : L"FALSE");
    LogCall(c, ra);
    BOOL r = InitiateSystemShutdownW_Original(machine, message, timeout, force, reboot);
    DWORD err = GetLastError();
    LogResult(L"InitiateSystemShutdownW", (ULONG_PTR)r, r ? L"(TRUE)" : L"(FALSE)");
    SetLastError(err);
    return r;
}

typedef DWORD(WINAPI* InitiateShutdownW_t)(LPWSTR, LPWSTR, DWORD, DWORD, DWORD);
static InitiateShutdownW_t InitiateShutdownW_Original;
static DWORD WINAPI InitiateShutdownW_Hook(LPWSTR machine, LPWSTR message, DWORD grace,
                                           DWORD flags, DWORD reason) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"InitiateShutdownW";
    c.hasReason = true;
    c.reason = reason;
    c.hasFlags = true;
    c.flags = flags;
    wchar_t m[64], msg[128], fl[400];
    ReadWide(machine, m, ARRAYSIZE(m));
    ReadWide(message, msg, ARRAYSIZE(msg));
    DecodeFlags(flags, kShutdownFlags, ARRAYSIZE(kShutdownFlags), fl, ARRAYSIZE(fl));
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1,
               L"machine=\"%s\", message=\"%s\", gracePeriod=%lu, shutdownFlags=0x%lX (%s), reason=0x%lX",
               m, msg, grace, flags, fl, reason);
    LogCall(c, ra);
    DWORD r = InitiateShutdownW_Original(machine, message, grace, flags, reason);
    DWORD err = GetLastError();
    LogResult(L"InitiateShutdownW", r, r == 0 ? L"(ERROR_SUCCESS)" : L"(error code)");
    SetLastError(err);
    return r;
}

typedef LONG(NTAPI* NtShutdownSystem_t)(DWORD);
static NtShutdownSystem_t NtShutdownSystem_Original;
static LONG NTAPI NtShutdownSystem_Hook(DWORD action) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"NtShutdownSystem";
    c.hasReason = false;
    c.hasFlags = true;
    c.flags = action;
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1, L"Action=%lu (%s)", action, ShutdownActionName(action));
    LogCall(c, ra);
    LONG r = NtShutdownSystem_Original(action);
    LogResult(L"NtShutdownSystem", (ULONG_PTR)(ULONG)r, L"(NTSTATUS)");
    return r;
}

typedef LONG(NTAPI* NtSetSystemPowerState_t)(DWORD, DWORD, ULONG);
static NtSetSystemPowerState_t NtSetSystemPowerState_Original;
static LONG NTAPI NtSetSystemPowerState_Hook(DWORD action, DWORD state, ULONG flags) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"NtSetSystemPowerState";
    c.hasReason = false;
    c.hasFlags = true;
    c.flags = flags;
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1,
               L"SystemAction=%lu (%s), LightestSystemState=%lu (%s), Flags=0x%lX",
               action, PowerActionName(action), state, PowerStateName(state), flags);
    LogCall(c, ra);
    LONG r = NtSetSystemPowerState_Original(action, state, flags);
    LogResult(L"NtSetSystemPowerState", (ULONG_PTR)(ULONG)r, L"(NTSTATUS)");
    return r;
}

typedef LONG(NTAPI* NtInitiatePowerAction_t)(DWORD, DWORD, ULONG, BOOLEAN);
static NtInitiatePowerAction_t NtInitiatePowerAction_Original;
static LONG NTAPI NtInitiatePowerAction_Hook(DWORD action, DWORD state, ULONG flags, BOOLEAN async) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"NtInitiatePowerAction";
    c.hasReason = false;
    c.hasFlags = true;
    c.flags = flags;
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1,
               L"SystemAction=%lu (%s), LightestSystemState=%lu (%s), Flags=0x%lX, Asynchronous=%s",
               action, PowerActionName(action), state, PowerStateName(state), flags,
               async ? L"TRUE" : L"FALSE");
    LogCall(c, ra);
    LONG r = NtInitiatePowerAction_Original(action, state, flags, async);
    LogResult(L"NtInitiatePowerAction", (ULONG_PTR)(ULONG)r, L"(NTSTATUS)");
    return r;
}

typedef BOOLEAN(WINAPI* SetSuspendState_t)(BOOLEAN, BOOLEAN, BOOLEAN);
static SetSuspendState_t SetSuspendState_Original;
static BOOLEAN WINAPI SetSuspendState_Hook(BOOLEAN hibernate, BOOLEAN force, BOOLEAN wakeupEventsDisabled) {
    void* ra = CALLER();
    CallRecord c{};
    c.api = L"SetSuspendState";
    c.hasReason = false;
    c.hasFlags = false;
    _snwprintf(c.args, ARRAYSIZE(c.args) - 1,
               L"bHibernate=%s, bForce=%s, bWakeupEventsDisabled=%s",
               hibernate ? L"TRUE" : L"FALSE", force ? L"TRUE" : L"FALSE",
               wakeupEventsDisabled ? L"TRUE" : L"FALSE");
    LogCall(c, ra);
    BOOLEAN r = SetSuspendState_Original(hibernate, force, wakeupEventsDisabled);
    DWORD err = GetLastError();
    LogResult(L"SetSuspendState", r, r ? L"(TRUE)" : L"(FALSE)");
    SetLastError(err);
    return r;
}

// ---------------------------------------------------------------------------
// Hook setup

struct HookTarget {
    const wchar_t* name;
    const wchar_t* modules[3];  // modules to try, in order
    const char* proc;
    void* hook;
    void** original;
};

static const HookTarget kTargets[] = {
    {L"ExitWindowsEx", {L"user32.dll", nullptr, nullptr}, "ExitWindowsEx",
     (void*)ExitWindowsEx_Hook, (void**)&ExitWindowsEx_Original},
    {L"InitiateSystemShutdownExW", {L"advapi32.dll", L"sechost.dll", L"kernelbase.dll"},
     "InitiateSystemShutdownExW", (void*)InitiateSystemShutdownExW_Hook,
     (void**)&InitiateSystemShutdownExW_Original},
    {L"InitiateSystemShutdownExA", {L"advapi32.dll", L"sechost.dll", L"kernelbase.dll"},
     "InitiateSystemShutdownExA", (void*)InitiateSystemShutdownExA_Hook,
     (void**)&InitiateSystemShutdownExA_Original},
    {L"InitiateSystemShutdownW", {L"advapi32.dll", L"sechost.dll", L"kernelbase.dll"},
     "InitiateSystemShutdownW", (void*)InitiateSystemShutdownW_Hook,
     (void**)&InitiateSystemShutdownW_Original},
    {L"InitiateShutdownW", {L"advapi32.dll", L"sechost.dll", L"kernelbase.dll"},
     "InitiateShutdownW", (void*)InitiateShutdownW_Hook, (void**)&InitiateShutdownW_Original},
    {L"NtShutdownSystem", {L"ntdll.dll", nullptr, nullptr}, "NtShutdownSystem",
     (void*)NtShutdownSystem_Hook, (void**)&NtShutdownSystem_Original},
    {L"NtSetSystemPowerState", {L"ntdll.dll", nullptr, nullptr}, "NtSetSystemPowerState",
     (void*)NtSetSystemPowerState_Hook, (void**)&NtSetSystemPowerState_Original},
    {L"NtInitiatePowerAction", {L"ntdll.dll", nullptr, nullptr}, "NtInitiatePowerAction",
     (void*)NtInitiatePowerAction_Hook, (void**)&NtInitiatePowerAction_Original},
    {L"SetSuspendState", {L"powrprof.dll", nullptr, nullptr}, "SetSuspendState",
     (void*)SetSuspendState_Hook, (void**)&SetSuspendState_Original},
};

static FARPROC ResolveApi(const HookTarget& t, const wchar_t** foundIn) {
    for (const wchar_t* modName : t.modules) {
        if (!modName) break;
        HMODULE m = GetModuleHandleW(modName);
        if (!m) m = LoadLibraryW(modName);
        if (!m) continue;
        FARPROC p = GetProcAddress(m, t.proc);
        if (p) {
            *foundIn = modName;
            return p;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Mod entry points

BOOL Wh_ModInit() {
    if (!GetModuleFileNameW(nullptr, g_procPath, MAX_PATH)) wcscpy(g_procPath, L"?");
    LoadSettings();

    wchar_t hooked[600] = L"";
    wchar_t skipped[600] = L"";
    size_t hookedLen = 0, skippedLen = 0;
    void* seen[ARRAYSIZE(kTargets)] = {};
    size_t seenCount = 0;

    for (const HookTarget& t : kTargets) {
        const wchar_t* foundIn = nullptr;
        FARPROC addr = ResolveApi(t, &foundIn);
        const wchar_t* why = nullptr;
        if (!addr) {
            why = L"not found";
        } else {
            for (size_t i = 0; i < seenCount; i++) {
                if (seen[i] == (void*)addr) why = L"same address as another hooked API";
            }
        }
        if (!why) {
            if (Wh_SetFunctionHook((void*)addr, t.hook, t.original)) {
                seen[seenCount++] = (void*)addr;
                int n = _snwprintf(hooked + hookedLen, ARRAYSIZE(hooked) - hookedLen - 1,
                                   L"%s%s(%s)", hookedLen ? L", " : L"", t.name, foundIn);
                if (n > 0) hookedLen += n;
                continue;
            }
            why = L"hook failed";
        }
        int n = _snwprintf(skipped + skippedLen, ARRAYSIZE(skipped) - skippedLen - 1,
                           L"%s%s(%s)", skippedLen ? L", " : L"", t.name, why);
        if (n > 0) skippedLen += n;
    }

    wchar_t line[1500];
    _snwprintf(line, ARRAYSIZE(line) - 1,
               L"INIT mod loaded in %s pid=%lu hooked=[%s] skipped=[%s] fileLog=%s",
               g_procPath, GetCurrentProcessId(), hooked[0] ? hooked : L"none",
               skipped[0] ? skipped : L"none",
               g_fileLog ? (g_logPath[0] ? g_logPath : L"no path") : L"off");
    line[ARRAYSIZE(line) - 1] = 0;
    Emit(line);
    return TRUE;
}

void Wh_ModUninit() {
    wchar_t line[400];
    _snwprintf(line, ARRAYSIZE(line) - 1, L"UNINIT mod unloading from %s pid=%lu", g_procPath,
               GetCurrentProcessId());
    line[ARRAYSIZE(line) - 1] = 0;
    Emit(line);
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}
