// ==WindhawkMod==
// @id              copy-file-names
// @name            Copy File Names
// @description     Press Ctrl+Shift+N in File Explorer to copy the names of the selected files, not the files
// @version         0.1.0
// @author          Shaerif
// @include         explorer.exe
// @compilerOptions -luser32 -lole32 -loleaut32 -luuid
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Copy File Names
Windows has Copy as path (Ctrl+Shift+C). It gives you full paths in quotes. Often
you only want the names. This mod copies the names of the selected files and
folders as plain text, one per line.

## Keys
- Ctrl+Shift+N copies the names with their extensions.
- Ctrl+Alt+Shift+N copies the names without extensions. Folders keep their full
  name. `a.tar.gz` becomes `a.tar`. A name that starts with a dot, like
  `.gitignore`, stays whole.

The keys only work while a File Explorer window is in front. You can change both
keys in the settings.

## Settings
- Separator: a new line, a comma, or a space between names.
- Quote names: put each name in double quotes.

## Limits
- Ctrl+Shift+N is also File Explorer's New folder key. While the mod is on, it
  copies names instead. Pick another key in the settings if you need New folder.
- The keys don't work on the desktop or in file dialogs. Only File Explorer windows
  count.
- Items that aren't files, like drives in This PC, give their internal name.
- If nothing is selected, nothing happens.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- withExtModifier: ctrl_shift
  $name: Modifiers for copy names with extension
  $options:
  - ctrl_shift: Ctrl+Shift
  - ctrl_alt: Ctrl+Alt
  - ctrl_alt_shift: Ctrl+Alt+Shift
  - alt_shift: Alt+Shift
  - win_shift: Win+Shift
- withExtKey: N
  $name: Key for copy names with extension
  $description: A letter, a digit, or F1 to F24
- noExtModifier: ctrl_alt_shift
  $name: Modifiers for copy names without extension
  $options:
  - ctrl_shift: Ctrl+Shift
  - ctrl_alt: Ctrl+Alt
  - ctrl_alt_shift: Ctrl+Alt+Shift
  - alt_shift: Alt+Shift
  - win_shift: Win+Shift
- noExtKey: N
  $name: Key for copy names without extension
  $description: A letter, a digit, or F1 to F24
- separator: newline
  $name: Separator
  $options:
  - newline: New line
  - comma: Comma
  - space: Space
- quoteNames: false
  $name: Put each name in double quotes
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <exdisp.h>
#include <servprov.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <string>
#include <vector>

#define WM_APP_RELOAD (WM_APP + 1)
#define WM_APP_QUIT (WM_APP + 2)

enum {
    kHotkeyWithExt = 1,
    kHotkeyNoExt = 2,
};

static const UINT_PTR kTimerId = 1;
static const UINT kTimerMs = 100;

// SID_STopLevelBrowser, defined here so the mod doesn't depend on which GUIDs
// the import libraries carry.
static const GUID kSidTopLevelBrowser = {
    0x4C96BE40, 0x915C, 0x11CF, {0x99, 0xD3, 0x00, 0xAA, 0x00, 0x4A, 0xE8, 0x37}};

struct {
    UINT withExtModifiers;
    UINT withExtKey;
    UINT noExtModifiers;
    UINT noExtKey;
    std::wstring separator;
    bool quoteNames;
} g_settings;

static HANDLE g_thread = nullptr;
static DWORD g_threadId = 0;
static HWND g_msgWnd = nullptr;
static HANDLE g_readyEvent = nullptr;

// Hotkeys are only registered while a File Explorer window is in front, so the
// same keys keep working in other apps.
static bool g_keysWanted = false;
static bool g_withExtRegistered = false;
static bool g_noExtRegistered = false;
static bool g_loggedRegisterError = false;
static bool g_copying = false;

static UINT ParseModifiers(PCWSTR s, UINT fallback) {
    if (!s) return fallback;
    if (wcscmp(s, L"ctrl_shift") == 0) return MOD_CONTROL | MOD_SHIFT;
    if (wcscmp(s, L"ctrl_alt") == 0) return MOD_CONTROL | MOD_ALT;
    if (wcscmp(s, L"ctrl_alt_shift") == 0) return MOD_CONTROL | MOD_ALT | MOD_SHIFT;
    if (wcscmp(s, L"alt_shift") == 0) return MOD_ALT | MOD_SHIFT;
    if (wcscmp(s, L"win_shift") == 0) return MOD_WIN | MOD_SHIFT;
    return fallback;
}

static UINT ParseKey(PCWSTR s, UINT fallback) {
    if (!s || !*s) return fallback;
    if (!s[1]) {
        wchar_t c = towupper(s[0]);
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) return (UINT)c;
        return fallback;
    }
    if (towupper(s[0]) == L'F') {
        int n = _wtoi(s + 1);
        if (n >= 1 && n <= 24) return VK_F1 + (n - 1);
    }
    return fallback;
}

static UINT ReadModifiers(PCWSTR name, UINT fallback) {
    PCWSTR s = Wh_GetStringSetting(name);
    UINT m = ParseModifiers(s, fallback);
    Wh_FreeStringSetting(s);
    return m | MOD_NOREPEAT;
}

static UINT ReadKey(PCWSTR name, UINT fallback) {
    PCWSTR s = Wh_GetStringSetting(name);
    UINT k = ParseKey(s, fallback);
    Wh_FreeStringSetting(s);
    return k;
}

static void LoadSettings() {
    g_settings.withExtModifiers = ReadModifiers(L"withExtModifier", MOD_CONTROL | MOD_SHIFT);
    g_settings.withExtKey = ReadKey(L"withExtKey", 'N');
    g_settings.noExtModifiers =
        ReadModifiers(L"noExtModifier", MOD_CONTROL | MOD_ALT | MOD_SHIFT);
    g_settings.noExtKey = ReadKey(L"noExtKey", 'N');

    PCWSTR sep = Wh_GetStringSetting(L"separator");
    if (sep && wcscmp(sep, L"comma") == 0) g_settings.separator = L", ";
    else if (sep && wcscmp(sep, L"space") == 0) g_settings.separator = L" ";
    else g_settings.separator = L"\r\n";
    Wh_FreeStringSetting(sep);

    g_settings.quoteNames = Wh_GetIntSetting(L"quoteNames") != 0;
}

static bool IsExplorerWindow(HWND hWnd) {
    if (!hWnd) return false;
    wchar_t cls[64];
    if (!GetClassNameW(hWnd, cls, ARRAYSIZE(cls))) return false;
    return wcscmp(cls, L"CabinetWClass") == 0;
}

static void UnregisterKeys(HWND hWnd) {
    if (g_withExtRegistered) UnregisterHotKey(hWnd, kHotkeyWithExt);
    if (g_noExtRegistered) UnregisterHotKey(hWnd, kHotkeyNoExt);
    g_withExtRegistered = false;
    g_noExtRegistered = false;
}

static void RegisterKeys(HWND hWnd) {
    UnregisterKeys(hWnd);
    g_withExtRegistered = RegisterHotKey(hWnd, kHotkeyWithExt, g_settings.withExtModifiers,
                                         g_settings.withExtKey) != 0;
    if (!g_withExtRegistered && !g_loggedRegisterError) {
        Wh_Log(L"Could not register the copy-with-extension hotkey (error %lu)", GetLastError());
    }
    g_noExtRegistered = RegisterHotKey(hWnd, kHotkeyNoExt, g_settings.noExtModifiers,
                                       g_settings.noExtKey) != 0;
    if (!g_noExtRegistered && !g_loggedRegisterError) {
        Wh_Log(L"Could not register the copy-without-extension hotkey (error %lu)", GetLastError());
    }
    if (!g_withExtRegistered || !g_noExtRegistered) g_loggedRegisterError = true;
}

static void UpdateKeys(HWND hWnd) {
    bool wanted = IsExplorerWindow(GetForegroundWindow());
    if (wanted == g_keysWanted) return;
    g_keysWanted = wanted;
    if (wanted) RegisterKeys(hWnd);
    else UnregisterKeys(hWnd);
}

// Finds the shell browser of the active tab in the given File Explorer window.
// The caller releases the result.
static IShellBrowser* FindShellBrowser(HWND fg) {
    IShellWindows* windows = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_IShellWindows,
                                (void**)&windows)) ||
        !windows) {
        return nullptr;
    }

    long count = 0;
    if (FAILED(windows->get_Count(&count))) count = 0;

    // Each tab is its own entry, and all tabs share the frame window. The first
    // tab child of the frame is the active one.
    HWND activeTab = FindWindowExW(fg, nullptr, L"ShellTabWindowClass", nullptr);

    IShellBrowser* best = nullptr;
    int bestRank = 0;
    for (long i = 0; i < count && bestRank < 3; i++) {
        VARIANT index;
        VariantInit(&index);
        index.vt = VT_I4;
        index.lVal = i;
        IDispatch* disp = nullptr;
        if (windows->Item(index, &disp) != S_OK || !disp) {
            if (disp) disp->Release();
            continue;
        }

        IWebBrowserApp* app = nullptr;
        if (SUCCEEDED(disp->QueryInterface(IID_IWebBrowserApp, (void**)&app)) && app) {
            SHANDLE_PTR frame = 0;
            if (SUCCEEDED(app->get_HWND(&frame)) && (HWND)(LONG_PTR)frame == fg) {
                IServiceProvider* sp = nullptr;
                if (SUCCEEDED(app->QueryInterface(IID_IServiceProvider, (void**)&sp)) && sp) {
                    IShellBrowser* browser = nullptr;
                    if (SUCCEEDED(sp->QueryService(kSidTopLevelBrowser, IID_IShellBrowser,
                                                   (void**)&browser)) &&
                        browser) {
                        HWND tab = nullptr;
                        if (FAILED(browser->GetWindow(&tab))) tab = nullptr;
                        int rank = 1;
                        if (activeTab && tab == activeTab) rank = 3;
                        else if (tab && IsWindowVisible(tab)) rank = 2;
                        if (rank > bestRank) {
                            if (best) best->Release();
                            best = browser;
                            bestRank = rank;
                        } else {
                            browser->Release();
                        }
                    }
                    sp->Release();
                }
            }
            app->Release();
        }
        disp->Release();
    }
    windows->Release();
    return best;
}

static void StripExtension(std::wstring& name) {
    size_t dot = name.find_last_of(L'.');
    // A dot at the start is a dotfile, not an extension.
    if (dot != std::wstring::npos && dot > 0) name.resize(dot);
}

static bool ReadSelectedNames(IShellBrowser* browser, bool withExt,
                              std::vector<std::wstring>& names) {
    IShellView* view = nullptr;
    if (FAILED(browser->QueryActiveShellView(&view)) || !view) return false;

    bool ok = false;
    IFolderView2* folderView = nullptr;
    if (SUCCEEDED(view->QueryInterface(IID_IFolderView2, (void**)&folderView)) && folderView) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(folderView->GetSelection(FALSE, &items)) && items) {
            DWORD count = 0;
            if (FAILED(items->GetCount(&count))) count = 0;
            for (DWORD i = 0; i < count; i++) {
                IShellItem* item = nullptr;
                if (FAILED(items->GetItemAt(i, &item)) || !item) continue;
                PWSTR display = nullptr;
                // The parsing name is the real file name. It keeps the extension
                // even when File Explorer hides extensions.
                if (SUCCEEDED(item->GetDisplayName(SIGDN_PARENTRELATIVEPARSING, &display)) &&
                    display) {
                    std::wstring name(display);
                    CoTaskMemFree(display);
                    if (!withExt) {
                        // A zip file reports itself as a folder too, but it's
                        // also a stream. Only real folders keep their full name.
                        SFGAOF attrs = 0;
                        if (FAILED(item->GetAttributes(SFGAO_FOLDER | SFGAO_STREAM, &attrs))) {
                            attrs = 0;
                        }
                        bool isFolder = (attrs & SFGAO_FOLDER) && !(attrs & SFGAO_STREAM);
                        if (!isFolder) StripExtension(name);
                    }
                    if (!name.empty()) names.push_back(std::move(name));
                }
                item->Release();
            }
            items->Release();
            ok = true;
        }
        folderView->Release();
    }
    view->Release();
    return ok;
}

static bool SetClipboardText(HWND owner, const std::wstring& text) {
    // Another program may hold the clipboard for a moment, so try a few times.
    bool opened = false;
    for (int i = 0; i < 5 && !opened; i++) {
        opened = OpenClipboard(owner) != 0;
        if (!opened) Sleep(20);
    }
    if (!opened) {
        Wh_Log(L"Could not open the clipboard (error %lu)", GetLastError());
        return false;
    }

    bool ok = false;
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (mem) {
        void* p = GlobalLock(mem);
        if (p) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(mem);
            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, mem)) {
                // The clipboard owns the memory now.
                mem = nullptr;
                ok = true;
            }
        }
        if (mem) GlobalFree(mem);
    }
    CloseClipboard();
    return ok;
}

static void CopyNames(HWND hWnd, bool withExt) {
    HWND fg = GetForegroundWindow();
    if (!IsExplorerWindow(fg)) return;

    // COM calls can pump messages, so a second key press could land here
    // while the first one is still running.
    if (g_copying) return;
    g_copying = true;

    std::vector<std::wstring> names;
    IShellBrowser* browser = FindShellBrowser(fg);
    if (browser) {
        if (!ReadSelectedNames(browser, withExt, names)) {
            Wh_Log(L"Could not read the selection from this window");
        }
        browser->Release();
    } else {
        Wh_Log(L"Could not find the shell view for this window");
    }

    if (!names.empty()) {
        std::wstring text;
        for (size_t i = 0; i < names.size(); i++) {
            if (i > 0) text += g_settings.separator;
            if (g_settings.quoteNames) text += L'"';
            text += names[i];
            if (g_settings.quoteNames) text += L'"';
        }
        if (SetClipboardText(hWnd, text)) {
            Wh_Log(L"Copied %zu names %s", names.size(),
                   withExt ? L"with extensions" : L"without extensions");
        }
    }

    g_copying = false;
}

static LRESULT CALLBACK MsgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == kTimerId) UpdateKeys(hWnd);
            return 0;
        case WM_HOTKEY:
            if (wParam == kHotkeyWithExt) CopyNames(hWnd, true);
            else if (wParam == kHotkeyNoExt) CopyNames(hWnd, false);
            return 0;
        case WM_APP_RELOAD:
            // Drop the old keys. The next timer tick registers the new ones if
            // File Explorer is in front.
            UnregisterKeys(hWnd);
            g_keysWanted = false;
            g_loggedRegisterError = false;
            LoadSettings();
            UpdateKeys(hWnd);
            return 0;
        case WM_APP_QUIT:
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hWnd, kTimerId);
            UnregisterKeys(hWnd);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static DWORD WINAPI ThreadProc(LPVOID) {
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comReady = SUCCEEDED(hrCom);
    if (!comReady) Wh_Log(L"CoInitializeEx failed (0x%08lX)", (unsigned long)hrCom);

    WNDCLASSW wc{};
    wc.lpfnWndProc = MsgWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WindhawkCopyFileNamesWnd";
    RegisterClassW(&wc);

    HWND wnd = nullptr;
    if (comReady) {
        wnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                              wc.hInstance, nullptr);
        if (wnd) SetTimer(wnd, kTimerId, kTimerMs, nullptr);
        else Wh_Log(L"Could not create the message window (error %lu)", GetLastError());
    }
    g_msgWnd = wnd;
    SetEvent(g_readyEvent);

    if (wnd) {
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    g_msgWnd = nullptr;
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (comReady) CoUninitialize();
    return 0;
}

BOOL Wh_ModInit() {
    LoadSettings();
    g_readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_readyEvent) return FALSE;
    g_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, &g_threadId);
    if (!g_thread) {
        CloseHandle(g_readyEvent);
        g_readyEvent = nullptr;
        return FALSE;
    }
    WaitForSingleObject(g_readyEvent, 3000);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    HWND wnd = g_msgWnd;
    if (wnd) PostMessageW(wnd, WM_APP_RELOAD, 0, 0);
}

void Wh_ModUninit() {
    if (g_thread) {
        // Post to the window, not the thread. A thread message can be lost while
        // a COM call runs its own message loop.
        HWND wnd = g_msgWnd;
        if (wnd) PostMessageW(wnd, WM_APP_QUIT, 0, 0);
        else PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_thread, 5000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_readyEvent) {
        CloseHandle(g_readyEvent);
        g_readyEvent = nullptr;
    }
}
