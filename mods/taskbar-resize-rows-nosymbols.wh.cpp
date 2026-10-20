// ==WindhawkMod==
// @id              taskbar-resize-rows-nosymbols
// @name            Taskbar Resize and Rows Without Symbols
// @description     A taller, multi row Windows 11 taskbar with a top edge you can drag. Uses XAML diagnostics and Win32 only, no PDB symbols.
// @version         0.1.7
// @author          Shaerif
// @license         GPL-3.0
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lcomctl32 -lgdi32 -lole32 -loleaut32 -lruntimeobject -lshcore
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Resize and Rows Without Symbols

A rewrite of the draggable multirow taskbar idea that doesn't look up any
function by its debug symbol. It keeps working when Microsoft hasn't yet
published PDB files for a new taskbar build.

## How it works
- The taskbar UI is reached through XAML diagnostics, the same public
  interface the Windows 11 Taskbar Styler mod uses (code based on ExplorerTAP
  from TranslucentTB).
- The taskbar frame gets a new height. Explorer reads the frame height when it
  sizes the taskbar window, so the window follows.
- An invisible grip inside the top edge of the taskbar lets you drag the
  taskbar taller or shorter in whole rows. The row count is remembered. The
  grip draws nothing. The mouse cursor turns into a resize arrow over it.
- With "Wrap taskbar buttons into rows" on, the taskbar buttons flow into the
  rows and the notification area icons wrap into the same number of rows. The
  clock, Wi-Fi, sound and battery don't change. It's experimental and off
  by default.

## Limits
- Only a horizontal taskbar on the primary monitor is resized. Secondary
  taskbars may follow, with no guarantee.
- Only one program can use XAML diagnostics in explorer at a time. This mod
  can clash with Windows 11 Taskbar Styler, TranslucentTB or ExplorerBlurMica
  when they run at the same time.

## Credits
Three projects shaped this mod.

- andkon wrote Draggable, All-in-One Multirow Taskbar & Systray for Windows 11,
  under GPL-3.0. That mod is the behavior reference: a taskbar you drag taller
  in whole rows, with buttons and tray icons that wrap into the rows. This mod
  takes no code from it. The two share only Windows API boilerplate and helpers
  that both took from m417z's mods, such as the helper that runs code on the
  taskbar thread. Links: https://github.com/andkondev and https://andkon.dev/
- m417z wrote Windows 11 Taskbar Styler, under GPL-3.0. The XAML diagnostics
  code in this mod, the part that reaches the taskbar's XAML tree without
  symbols, is adapted from it. The message that makes explorer re-read the
  taskbar height and the ABM_QUERYPOS fix for secondary taskbars follow m417z's
  Taskbar height and icon size mod. Links: https://github.com/m417z and
  https://m417z.com/
- The Styler's XAML diagnostics code is itself based on ExplorerTAP from the
  TranslucentTB project, under GPL-3.0, so that's where this part began. Link:
  https://github.com/TranslucentTB/TranslucentTB/tree/develop/ExplorerTAP

Because it adapts GPL-3.0 code, this mod is GPL-3.0. The other mods in the repo
are MIT.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- RowHeight: 48
  $name: Row height
  $description: Height of one taskbar row in pixels at 100% scaling. Windows uses 48.
- Rows: 1
  $name: Rows
  $description: The row count to start with. A drag on the top edge overrides it, and the dragged value is remembered.
- MinRows: 1
  $name: Minimum rows
- MaxRows: 4
  $name: Maximum rows
- IconSize: 0
  $name: Taskbar button icon size
  $description: Icon size in pixels at 100% scaling. 0 keeps the Windows size.
- MultirowLayout: false
  $name: Wrap taskbar buttons into rows (experimental)
  $description: Moves taskbar buttons that don't fit on the first row to the rows below and wraps the notification area icons into the same rows. Off by default because it's experimental.
- EdgeResizeEnabled: true
  $name: Drag the top edge to resize
- EdgeResizeGripPixels: 8
  $name: Grip height
  $description: Height of the drag area at the top edge, in pixels at 100% scaling.
- DebugLog: false
  $name: Debug log file
  $description: Writes a log and a dump of the taskbar layout to the wh-work\multirow-test folder inside your temp folder. Use it only to find a problem.
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <Unknwn.h>
#include <combaseapi.h>
#include <ocidl.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <xamlom.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <deque>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

#undef GetCurrentTime

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/base.h>

namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxm = winrt::Windows::UI::Xaml::Media;

////////////////////////////////////////////////////////////////////////////////
// Settings and shared state

struct {
    int rowHeight;
    int rows;
    int minRows;
    int maxRows;
    int iconSize;
    bool multirowLayout;
    bool edgeResizeEnabled;
    int edgeResizeGripPixels;
    bool debugLog;
} g_settings;

std::atomic<int> g_effectiveRows{1};
std::atomic<bool> g_unloading{false};
std::atomic<bool> g_tapInjected{false};

constexpr double kDefaultFrameHeight = 48.0;

int ClampInt(int value, int low, int high) {
    if (high < low) {
        high = low;
    }
    return std::min(std::max(value, low), high);
}

void LoadSettings() {
    g_settings.rowHeight = ClampInt(Wh_GetIntSetting(L"RowHeight"), 24, 200);
    g_settings.minRows = ClampInt(Wh_GetIntSetting(L"MinRows"), 1, 10);
    g_settings.maxRows =
        ClampInt(Wh_GetIntSetting(L"MaxRows"), g_settings.minRows, 10);
    g_settings.rows = ClampInt(Wh_GetIntSetting(L"Rows"), g_settings.minRows,
                               g_settings.maxRows);
    g_settings.iconSize = ClampInt(Wh_GetIntSetting(L"IconSize"), 0, 128);
    g_settings.multirowLayout = Wh_GetIntSetting(L"MultirowLayout") != 0;
    g_settings.edgeResizeEnabled = Wh_GetIntSetting(L"EdgeResizeEnabled") != 0;
    g_settings.edgeResizeGripPixels =
        ClampInt(Wh_GetIntSetting(L"EdgeResizeGripPixels"), 2, 32);
    g_settings.debugLog = Wh_GetIntSetting(L"DebugLog") != 0;

    int rows = g_settings.rows;
    int dragged = Wh_GetIntValue(L"EdgeResizeRows", 0);
    if (dragged > 0) {
        rows = dragged;
    }
    g_effectiveRows =
        ClampInt(rows, g_settings.minRows, g_settings.maxRows);
}

double TargetFrameHeight() {
    return static_cast<double>(g_effectiveRows.load() * g_settings.rowHeight);
}

////////////////////////////////////////////////////////////////////////////////
// Debug log file. Only written when DebugLog is on.

std::mutex g_fileLogMutex;

std::wstring DebugFilePath(PCWSTR fileName) {
    WCHAR temp[MAX_PATH];
    DWORD length = GetTempPath(ARRAYSIZE(temp), temp);
    if (!length || length >= ARRAYSIZE(temp)) {
        return L"";
    }
    std::wstring dir = std::wstring(temp) + L"wh-work";
    CreateDirectory(dir.c_str(), nullptr);
    dir += L"\\multirow-test";
    CreateDirectory(dir.c_str(), nullptr);
    return dir + L"\\" + fileName;
}

void AppendToFile(PCWSTR fileName, PCWSTR text) {
    std::wstring path = DebugFilePath(fileName);
    if (path.empty()) {
        return;
    }
    SYSTEMTIME time;
    GetLocalTime(&time);
    WCHAR prefix[64];
    _snwprintf(prefix, ARRAYSIZE(prefix) - 1, L"%02d:%02d:%02d.%03d t%u ",
               time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
               GetCurrentThreadId());
    prefix[ARRAYSIZE(prefix) - 1] = L'\0';
    std::wstring line = std::wstring(prefix) + text + L"\r\n";
    int size = WideCharToMultiByte(CP_UTF8, 0, line.c_str(),
                                   static_cast<int>(line.size()), nullptr, 0,
                                   nullptr, nullptr);
    if (size <= 0) {
        return;
    }
    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()),
                        utf8.data(), size, nullptr, nullptr);
    std::lock_guard<std::mutex> lock(g_fileLogMutex);
    HANDLE file = CreateFile(path.c_str(), FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written;
    WriteFile(file, utf8.data(), size, &written, nullptr);
    CloseHandle(file);
}

// Logs to the Windhawk log and, with DebugLog on, to mod-log.txt.
void LogLine(PCWSTR format, ...) {
    WCHAR text[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf(text, ARRAYSIZE(text) - 1, format, args);
    va_end(args);
    text[ARRAYSIZE(text) - 1] = L'\0';
    Wh_Log(L"%s", text);
    if (g_settings.debugLog) {
        AppendToFile(L"mod-log.txt", text);
    }
}

bool IsResizeActive() {
    return !g_unloading &&
           TargetFrameHeight() != kDefaultFrameHeight;
}

HMODULE GetCurrentModuleHandle() {
    HMODULE module;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           L"", &module)) {
        return nullptr;
    }
    return module;
}

HWND FindCurrentProcessTaskbarWnd() {
    HWND hTaskbarWnd = nullptr;
    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            DWORD dwProcessId;
            WCHAR className[32];
            if (GetWindowThreadProcessId(hWnd, &dwProcessId) &&
                dwProcessId == GetCurrentProcessId() &&
                GetClassName(hWnd, className, ARRAYSIZE(className)) &&
                _wcsicmp(className, L"Shell_TrayWnd") == 0) {
                *reinterpret_cast<HWND*>(lParam) = hWnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&hTaskbarWnd));
    return hTaskbarWnd;
}

HWND GetTaskbarUiWnd() {
    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (!hTaskbarWnd) {
        return nullptr;
    }
    return FindWindowEx(hTaskbarWnd, nullptr,
                        L"Windows.UI.Composition.DesktopWindowContentBridge",
                        nullptr);
}

using RunFromWindowThreadProc_t = void(WINAPI*)(PVOID parameter);

bool RunFromWindowThread(HWND hWnd,
                         RunFromWindowThreadProc_t proc,
                         PVOID procParam) {
    static const UINT runFromWindowThreadRegisteredMsg =
        RegisterWindowMessage(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);

    struct RUN_FROM_WINDOW_THREAD_PARAM {
        RunFromWindowThreadProc_t proc;
        PVOID procParam;
    };

    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (dwThreadId == 0) {
        return false;
    }

    if (dwThreadId == GetCurrentThreadId()) {
        proc(procParam);
        return true;
    }

    HHOOK hook = SetWindowsHookEx(
        WH_CALLWNDPROC,
        [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (nCode == HC_ACTION) {
                const CWPSTRUCT* cwp = (const CWPSTRUCT*)lParam;
                if (cwp->message == runFromWindowThreadRegisteredMsg) {
                    RUN_FROM_WINDOW_THREAD_PARAM* param =
                        (RUN_FROM_WINDOW_THREAD_PARAM*)cwp->lParam;
                    param->proc(param->procParam);
                }
            }
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        },
        nullptr, dwThreadId);
    if (!hook) {
        return false;
    }

    RUN_FROM_WINDOW_THREAD_PARAM param;
    param.proc = proc;
    param.procParam = procParam;
    SendMessage(hWnd, runFromWindowThreadRegisteredMsg, 0, (LPARAM)&param);

    UnhookWindowsHookEx(hook);
    return true;
}

// Asks explorer to size the taskbar again. This is the message the
// Taskbar height and icon size mod sends for the same purpose. Posted, so it
// never runs inside a XAML layout pass.
void RequestTaskbarRecalc(bool force = false) {
    static std::atomic<ULONGLONG> lastTick{0};
    ULONGLONG now = GetTickCount64();
    if (!force && now - lastTick.load() < 200) {
        return;
    }
    lastTick = now;

    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (hTaskbarWnd) {
        PostMessage(hTaskbarWnd, WM_SETTINGCHANGE, SPI_SETLOGICALDPIOVERRIDE,
                    0);
    }
}

bool IsTaskbarHorizontal(HWND hTaskbarWnd) {
    RECT rect{};
    if (!hTaskbarWnd || !GetWindowRect(hTaskbarWnd, &rect)) {
        return false;
    }
    return (rect.right - rect.left) > (rect.bottom - rect.top);
}

////////////////////////////////////////////////////////////////////////////////
// XAML element tracking. Everything in this section runs on the UI thread that
// owns the element, which is the thread XAML diagnostics reports it on.

enum class ElementKind {
    Frame,     // Taskbar.TaskbarFrame
    Inner,     // fixed height parts inside the frame
    Repeater,  // TaskbarFrameRepeater, the taskbar button list
    Icon,      // TaskListButton > IconPanel > Icon
    TrayHost,  // NotificationAreaIcons, the notification area icon list
    TrayItems, // the ItemsControl that holds the notification area icons
};

struct SavedValue {
    wux::DependencyProperty property{nullptr};
    wf::IInspectable localValue{nullptr};
};

struct TrackedElement {
    InstanceHandle handle = 0;
    ElementKind kind = ElementKind::Inner;
    winrt::weak_ref<wux::FrameworkElement> element;
    std::vector<SavedValue> saved;
    int64_t heightToken = 0;
    int64_t maxHeightToken = 0;
    winrt::event_token layoutUpdatedToken{};
    // Repeater and TrayItems.
    double baseWidth = 0;
    double baseFrameWidth = 0;
    bool rowsApplied = false;
    // TrayItems only: rows and item size of the WrapGrid that was set.
    int trayRows = 0;
    double trayItemHeight = 0;
    // Inner only: a notification area parent, sized only while the tray
    // icons wrap into rows.
    bool trayParent = false;
};

struct RowTransform {
    winrt::weak_ref<wux::UIElement> element;
    wuxm::TranslateTransform transform{nullptr};
};

// A deque keeps references valid when tracking adds entries during a pass.
thread_local std::deque<TrackedElement> g_tracked;
thread_local std::vector<RowTransform> g_rowTransforms;
thread_local bool g_selfSetting = false;
thread_local bool g_inArrangeRows = false;

std::mutex g_threadsMutex;
std::vector<DWORD> g_trackedThreads;

void RememberCurrentThread() {
    DWORD threadId = GetCurrentThreadId();
    std::lock_guard<std::mutex> lock(g_threadsMutex);
    if (std::find(g_trackedThreads.begin(), g_trackedThreads.end(),
                  threadId) == g_trackedThreads.end()) {
        g_trackedThreads.push_back(threadId);
    }
}

TrackedElement* FindTracked(InstanceHandle handle) {
    for (auto& entry : g_tracked) {
        if (entry.handle == handle) {
            return &entry;
        }
    }
    return nullptr;
}

TrackedElement* FindTrackedByElement(wux::FrameworkElement const& element) {
    for (auto& entry : g_tracked) {
        auto tracked = entry.element.get();
        if (tracked && tracked == element) {
            return &entry;
        }
    }
    return nullptr;
}

std::wstring ClassNameOf(wf::IInspectable const& object) {
    try {
        return std::wstring(winrt::get_class_name(object));
    } catch (...) {
        return L"";
    }
}

bool IsInsideTaskbarFrame(wux::DependencyObject element) {
    for (int depth = 0; element && depth < 16; depth++) {
        element = wuxm::VisualTreeHelper::GetParent(element);
        if (element && ClassNameOf(element) == L"Taskbar.TaskbarFrame") {
            return true;
        }
    }
    return false;
}

bool IsBinding(wf::IInspectable const& value) {
    if (!value) {
        return false;
    }
    std::wstring name = ClassNameOf(value);
    return name.find(L"BindingExpression") != std::wstring::npos;
}

// Saves the local value the first time the mod is about to change a property,
// so that the exact original can be put back on unload. A property that holds a
// binding is never touched.
bool SaveOnce(TrackedElement& entry,
              wux::FrameworkElement const& element,
              wux::DependencyProperty const& property) {
    for (auto& saved : entry.saved) {
        if (saved.property == property) {
            return !IsBinding(saved.localValue);
        }
    }
    SavedValue saved;
    saved.property = property;
    saved.localValue = element.ReadLocalValue(property);
    bool ok = !IsBinding(saved.localValue);
    entry.saved.push_back(std::move(saved));
    return ok;
}

void UpdateSaved(TrackedElement& entry,
                 wux::FrameworkElement const& element,
                 wux::DependencyProperty const& property) {
    for (auto& saved : entry.saved) {
        if (saved.property == property) {
            saved.localValue = element.ReadLocalValue(property);
            return;
        }
    }
}

void RestoreSaved(TrackedElement& entry, wux::FrameworkElement const& element) {
    g_selfSetting = true;
    for (auto& saved : entry.saved) {
        try {
            if (IsBinding(saved.localValue)) {
                continue;
            }
            if (!saved.localValue ||
                saved.localValue == wux::DependencyProperty::UnsetValue()) {
                element.ClearValue(saved.property);
            } else {
                element.SetValue(saved.property, saved.localValue);
            }
        } catch (...) {
            Wh_Log(L"Restore error %08X", winrt::to_hresult());
        }
    }
    g_selfSetting = false;
    entry.saved.clear();
}

void SetDoubleIfDifferent(wux::FrameworkElement const& element,
                          wux::DependencyProperty const& property,
                          double value) {
    auto current = winrt::unbox_value_or<double>(element.GetValue(property),
                                                 std::nan(""));
    if (current == value) {
        return;
    }
    g_selfSetting = true;
    element.SetValue(property, winrt::box_value(value));
    g_selfSetting = false;
}

void ApplyHeight(TrackedElement& entry) {
    auto element = entry.element.get();
    if (!element) {
        return;
    }

    double target = TargetFrameHeight();
    auto heightProperty = wux::FrameworkElement::HeightProperty();
    auto maxHeightProperty = wux::FrameworkElement::MaxHeightProperty();

    bool trayRowsOn = g_settings.multirowLayout && g_effectiveRows > 1;
    if (!IsResizeActive() || (entry.trayParent && !trayRowsOn)) {
        RestoreSaved(entry, element);
        return;
    }

    // The frame always gets the height, it's what explorer reads back. Inner
    // parts only get it when Windows gave them a fixed height.
    bool setHeight = entry.kind == ElementKind::Frame ||
                     !std::isnan(element.Height());
    if (setHeight && SaveOnce(entry, element, heightProperty)) {
        SetDoubleIfDifferent(element, heightProperty, target);
    }
    if (element.MaxHeight() < target &&
        SaveOnce(entry, element, maxHeightProperty)) {
        SetDoubleIfDifferent(element, maxHeightProperty,
                             std::numeric_limits<double>::infinity());
    }
}

void ApplyIcon(TrackedElement& entry) {
    auto element = entry.element.get();
    if (!element) {
        return;
    }
    if (g_unloading || g_settings.iconSize <= 0) {
        RestoreSaved(entry, element);
        return;
    }
    double size = g_settings.iconSize;
    auto widthProperty = wux::FrameworkElement::WidthProperty();
    auto heightProperty = wux::FrameworkElement::HeightProperty();
    if (SaveOnce(entry, element, widthProperty)) {
        SetDoubleIfDifferent(element, widthProperty, size);
    }
    if (SaveOnce(entry, element, heightProperty)) {
        SetDoubleIfDifferent(element, heightProperty, size);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Phase 2: rows. The button list keeps its own layout. It's given room for all
// rows side by side (a negative right margin, as the old mod did) and the
// buttons past the first row width are then moved down with a render transform.
// A render transform doesn't change layout, so this can't start a layout loop.

RowTransform* FindRowTransform(wux::UIElement const& element) {
    for (auto& item : g_rowTransforms) {
        auto tracked = item.element.get();
        if (tracked && tracked == element) {
            return &item;
        }
    }
    return nullptr;
}

void SetRowOffset(wux::UIElement const& element, double dx, double dy) {
    RowTransform* item = FindRowTransform(element);
    if (!item) {
        if (dx == 0 && dy == 0) {
            return;
        }
        // The getter returns a default transform for every element, so only
        // a local value means Windows set its own transform here. Leave that
        // alone.
        auto local =
            element.ReadLocalValue(wux::UIElement::RenderTransformProperty());
        if (local && local != wux::DependencyProperty::UnsetValue()) {
            return;
        }
        RowTransform created;
        created.element = winrt::make_weak(element);
        created.transform = wuxm::TranslateTransform();
        element.RenderTransform(created.transform);
        g_rowTransforms.push_back(std::move(created));
        item = &g_rowTransforms.back();
    } else if (element.RenderTransform() != item->transform) {
        return;
    }
    if (item->transform.X() != dx) {
        item->transform.X(dx);
    }
    if (item->transform.Y() != dy) {
        item->transform.Y(dy);
    }
}

void ClearRowTransforms() {
    for (auto& item : g_rowTransforms) {
        try {
            auto element = item.element.get();
            if (element && element.RenderTransform() == item.transform) {
                element.ClearValue(wux::UIElement::RenderTransformProperty());
            }
        } catch (...) {
        }
    }
    g_rowTransforms.clear();
}

void UndoRows(TrackedElement& entry) {
    ClearRowTransforms();
    if (entry.rowsApplied) {
        if (auto element = entry.element.get()) {
            RestoreSaved(entry, element);
        }
        entry.rowsApplied = false;
    }
    entry.baseWidth = 0;
    entry.baseFrameWidth = 0;
}

wux::FrameworkElement FindFrameAncestor(wux::DependencyObject element) {
    for (int depth = 0; element && depth < 16; depth++) {
        element = wuxm::VisualTreeHelper::GetParent(element);
        if (element && ClassNameOf(element) == L"Taskbar.TaskbarFrame") {
            return element.try_as<wux::FrameworkElement>();
        }
    }
    return nullptr;
}

// The notification area is a sibling of the taskbar frame in the same XAML
// root on current builds.
wux::FrameworkElement FindTrayFrameNear(wux::FrameworkElement const& frame) {
    auto parent = wuxm::VisualTreeHelper::GetParent(frame);
    if (!parent) {
        return nullptr;
    }
    int count = wuxm::VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; i++) {
        auto child = wuxm::VisualTreeHelper::GetChild(parent, i);
        if (ClassNameOf(child) == L"SystemTray.SystemTrayFrame") {
            return child.try_as<wux::FrameworkElement>();
        }
    }
    return nullptr;
}

void ArrangeRows(TrackedElement& entry) {
    auto repeater = entry.element.get();
    if (!repeater) {
        return;
    }

    int rows = g_effectiveRows;
    if (g_unloading || !g_settings.multirowLayout || rows <= 1) {
        UndoRows(entry);
        return;
    }

    auto frame = FindFrameAncestor(repeater);
    if (!frame) {
        return;
    }

    double frameWidth = frame.ActualWidth();
    if (entry.baseWidth > 0 && std::fabs(frameWidth - entry.baseFrameWidth) > 1) {
        // The taskbar changed width. Measure the one row width again from the
        // original margin on the next pass.
        UndoRows(entry);
        return;
    }

    auto marginProperty = wux::FrameworkElement::MarginProperty();
    if (entry.baseWidth <= 0) {
        wf::Rect slot =
            wux::Controls::Primitives::LayoutInformation::GetLayoutSlot(
                repeater);
        wux::Thickness margin = repeater.Margin();
        double width = slot.Width - margin.Left - margin.Right;
        if (!(width > 50)) {
            return;
        }
        entry.baseWidth = width;
        entry.baseFrameWidth = frameWidth;
    }

    // The button list spans the whole frame and the notification area is
    // drawn over its right part. A row ends where the notification area
    // starts, as the old mod's "width without extent" did. The notification
    // area gets narrower when its icons wrap, so this is read on every pass.
    double rowWidth = entry.baseWidth;
    if (auto trayFrame = FindTrayFrameNear(frame)) {
        try {
            double trayLeft =
                trayFrame.TransformToVisual(frame).TransformPoint({0, 0}).X;
            double repeaterLeft =
                repeater.TransformToVisual(frame).TransformPoint({0, 0}).X;
            double width = trayLeft - repeaterLeft;
            if (width > 50 && width < rowWidth) {
                rowWidth = width;
            }
        } catch (...) {
        }
    }

    double rowHeight = g_settings.rowHeight;
    if (!SaveOnce(entry, repeater, marginProperty) ||
        !SaveOnce(entry, repeater, wux::FrameworkElement::HeightProperty()) ||
        !SaveOnce(entry, repeater,
                  wux::FrameworkElement::VerticalAlignmentProperty())) {
        return;
    }
    entry.rowsApplied = true;

    g_selfSetting = true;
    if (repeater.Height() != rowHeight) {
        repeater.Height(rowHeight);
    }
    if (repeater.VerticalAlignment() != wux::VerticalAlignment::Top) {
        repeater.VerticalAlignment(wux::VerticalAlignment::Top);
    }
    g_selfSetting = false;

    struct Item {
        wux::FrameworkElement element{nullptr};
        wf::Rect slot;
    };
    std::vector<Item> items;
    int count = wuxm::VisualTreeHelper::GetChildrenCount(repeater);
    for (int i = 0; i < count; i++) {
        auto child = wuxm::VisualTreeHelper::GetChild(repeater, i)
                         .try_as<wux::FrameworkElement>();
        if (!child || child.Visibility() != wux::Visibility::Visible) {
            continue;
        }
        wf::Rect slot =
            wux::Controls::Primitives::LayoutInformation::GetLayoutSlot(child);
        // Recycled buttons are parked far outside the list.
        if (slot.Width <= 0 || slot.X < -1000) {
            SetRowOffset(child, 0, 0);
            continue;
        }
        items.push_back({child, slot});
    }

    std::sort(items.begin(), items.end(), [](Item const& a, Item const& b) {
        return a.slot.X < b.slot.X;
    });

    double x = 0;
    int row = 0;
    // Space left empty at the end of each full row, because the next button
    // didn't fit there.
    double rowEndGaps = 0;
    std::vector<int> perRow(rows + 1, 0);
    for (auto& item : items) {
        double width = item.slot.Width;
        if (x > 0 && x + width > rowWidth + 0.5) {
            if (row + 1 < rows) {
                rowEndGaps += rowWidth - x;
            }
            row++;
            x = 0;
        }
        if (row >= rows) {
            // More buttons than rows can hold, leave the rest where Windows
            // put them.
            SetRowOffset(item.element, 0, 0);
            perRow[rows]++;
            continue;
        }
        SetRowOffset(item.element, x - item.slot.X, row * rowHeight);
        perRow[row]++;
        x += width;
    }

    // The button list is made exactly as wide as the rows can hold, so Windows
    // itself shrinks or collapses the buttons that don't fit and no button
    // lands past the screen edge. Buttons can change width with the
    // space they get, so the changes per second are capped to rule out a
    // layout loop.
    // Windows keeps the part of the list under the notification area free,
    // so the list gets the extra rows on top of its one row width.
    double extraWidth = (rows - 1) * rowWidth - rowEndGaps;
    double wantedRight = std::min(0.0, -extraWidth);
    wux::Thickness margin = repeater.Margin();
    if (std::fabs(margin.Right - wantedRight) > 1) {
        static thread_local ULONGLONG windowStart = 0;
        static thread_local int changes = 0;
        ULONGLONG now = GetTickCount64();
        if (now - windowStart > 1000) {
            windowStart = now;
            changes = 0;
        }
        if (changes < 8) {
            changes++;
            margin.Right = wantedRight;
            g_selfSetting = true;
            repeater.Margin(margin);
            g_selfSetting = false;
        } else if (changes == 8) {
            changes++;
            LogLine(L"Button list width changes paused for a second");
        }
    }

    // Logged only when the result changes, LayoutUpdated runs often.
    static thread_local std::wstring lastSummary;
    std::wstring summary = L"Button rows " + std::to_wstring(rows) +
                           L", row width " +
                           std::to_wstring(static_cast<int>(rowWidth)) +
                           L", buttons per row";
    for (int i = 0; i <= rows; i++) {
        summary += (i == rows ? L", left over " : L" ") +
                   std::to_wstring(perRow[i]);
    }
    if (summary != lastSummary) {
        lastSummary = summary;
        LogLine(L"%s", summary.c_str());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Notification area rows. The icon list is an ItemsControl. Its panel is
// replaced by a WrapGrid that fills the rows column by column, as the old mod
// did. Restoring puts the original panel value back.

void TrackElement(InstanceHandle handle,
                  wux::FrameworkElement const& element,
                  ElementKind kind);

// Elements found by walking the tree have no diagnostics handle. The object
// address is unique while the element lives, and the top bit keeps it apart
// from real handles.
InstanceHandle PseudoHandle(wux::FrameworkElement const& element) {
    return static_cast<InstanceHandle>(
               reinterpret_cast<uintptr_t>(winrt::get_abi(element))) |
           (1ull << 63);
}

wux::FrameworkElement FindItemsControl(wux::FrameworkElement const& host) {
    if (host.try_as<wuxc::ItemsControl>()) {
        return host;
    }
    std::vector<std::pair<wux::DependencyObject, int>> queue;
    queue.emplace_back(host, 0);
    for (size_t i = 0; i < queue.size(); i++) {
        auto node = queue[i].first;
        int depth = queue[i].second;
        if (depth >= 6) {
            continue;
        }
        int count = wuxm::VisualTreeHelper::GetChildrenCount(node);
        for (int c = 0; c < count; c++) {
            auto child = wuxm::VisualTreeHelper::GetChild(node, c);
            if (child.try_as<wuxc::ItemsControl>()) {
                if (auto element = child.try_as<wux::FrameworkElement>()) {
                    return element;
                }
            }
            queue.emplace_back(child, depth + 1);
        }
    }
    return nullptr;
}

double MeasureTrayItemWidth(wuxc::ItemsControl const& itemsControl) {
    auto panel = itemsControl.ItemsPanelRoot();
    if (!panel) {
        return 0;
    }
    double width = 0;
    int count = wuxm::VisualTreeHelper::GetChildrenCount(panel);
    for (int i = 0; i < count; i++) {
        auto child = wuxm::VisualTreeHelper::GetChild(panel, i)
                         .try_as<wux::FrameworkElement>();
        if (child && child.Visibility() == wux::Visibility::Visible) {
            width = std::max(width, child.ActualWidth());
        }
    }
    return width;
}

wuxc::ItemsPanelTemplate LoadWrapGridTemplate(int rows,
                                              int itemWidth,
                                              int itemHeight) {
    WCHAR xaml[512];
    _snwprintf(xaml, ARRAYSIZE(xaml) - 1,
               L"<ItemsPanelTemplate "
               L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/"
               L"presentation\"><WrapGrid Orientation=\"Vertical\" "
               L"MaximumRowsOrColumns=\"%d\" ItemWidth=\"%d\" "
               L"ItemHeight=\"%d\" VerticalAlignment=\"Center\"/>"
               L"</ItemsPanelTemplate>",
               rows, itemWidth, itemHeight);
    xaml[ARRAYSIZE(xaml) - 1] = L'\0';
    try {
        return wux::Markup::XamlReader::Load(xaml)
            .as<wuxc::ItemsPanelTemplate>();
    } catch (...) {
        LogLine(L"WrapGrid template failed %08X", winrt::to_hresult());
        return nullptr;
    }
}

// Parents of the icon list that Windows gave a fixed one row height get the
// full taskbar height, otherwise the second row would be cut off.
void TrackTrayAncestors(wux::FrameworkElement const& element) {
    double target = TargetFrameHeight();
    wux::DependencyObject node = element;
    for (int depth = 0; node && depth < 12; depth++) {
        node = wuxm::VisualTreeHelper::GetParent(node);
        auto parent = node ? node.try_as<wux::FrameworkElement>() : nullptr;
        if (!parent) {
            break;
        }
        std::wstring className = ClassNameOf(parent);
        bool fixedHeight =
            !std::isnan(parent.Height()) && parent.Height() < target;
        if (fixedHeight || parent.MaxHeight() < target) {
            InstanceHandle handle = PseudoHandle(parent);
            if (!FindTracked(handle)) {
                LogLine(L"Tray parent %s #%s height %.1f max %.1f gets %.1f",
                        className.c_str(), std::wstring(parent.Name()).c_str(),
                        parent.Height(), parent.MaxHeight(), target);
                TrackElement(handle, parent, ElementKind::Inner);
                if (TrackedElement* added = FindTracked(handle)) {
                    added->trayParent = true;
                }
            }
        }
        if (className == L"SystemTray.SystemTrayFrame") {
            break;
        }
    }
}

void ApplyTrayRows(TrackedElement& entry) {
    auto element = entry.element.get();
    if (!element) {
        return;
    }
    auto itemsControl = element.try_as<wuxc::ItemsControl>();
    if (!itemsControl) {
        return;
    }

    int rows = g_effectiveRows;
    if (g_unloading || !g_settings.multirowLayout || rows <= 1) {
        if (entry.rowsApplied) {
            RestoreSaved(entry, element);
            entry.rowsApplied = false;
            entry.trayRows = 0;
            entry.trayItemHeight = 0;
            LogLine(L"Tray rows undone");
        }
        return;
    }

    if (entry.baseWidth <= 0) {
        // The width of one icon as Windows laid it out. Not known before the
        // first layout pass, LayoutUpdated calls this again.
        double width = MeasureTrayItemWidth(itemsControl);
        if (!(width > 4)) {
            return;
        }
        entry.baseWidth = width;
    }

    double itemHeight = std::floor(TargetFrameHeight() / rows);
    if (!SaveOnce(entry, element, wuxc::ItemsControl::ItemsPanelProperty()) ||
        !SaveOnce(entry, element, wux::FrameworkElement::HeightProperty()) ||
        !SaveOnce(entry, element,
                  wux::FrameworkElement::VerticalAlignmentProperty())) {
        return;
    }

    if (entry.trayRows != rows || entry.trayItemHeight != itemHeight) {
        auto itemsPanel = LoadWrapGridTemplate(
            rows, static_cast<int>(std::ceil(entry.baseWidth)),
            static_cast<int>(itemHeight));
        if (!itemsPanel) {
            return;
        }
        g_selfSetting = true;
        itemsControl.ItemsPanel(itemsPanel);
        g_selfSetting = false;
        entry.rowsApplied = true;
        entry.trayRows = rows;
        entry.trayItemHeight = itemHeight;
        LogLine(L"Tray rows %d, icon slot %.0fx%.0f", rows, entry.baseWidth,
                itemHeight);
        TrackTrayAncestors(element);
    }

    g_selfSetting = true;
    SetDoubleIfDifferent(element, wux::FrameworkElement::HeightProperty(),
                         rows * itemHeight);
    if (element.VerticalAlignment() != wux::VerticalAlignment::Center) {
        element.VerticalAlignment(wux::VerticalAlignment::Center);
    }
    g_selfSetting = false;
}

void ApplyTrayHost(TrackedElement& entry) {
    if (g_unloading || !g_settings.multirowLayout || g_effectiveRows <= 1) {
        return;
    }
    auto host = entry.element.get();
    if (!host) {
        return;
    }
    auto items = FindItemsControl(host);
    if (!items) {
        return;
    }
    InstanceHandle handle = PseudoHandle(items);
    if (!FindTracked(handle)) {
        LogLine(L"Tray icon list found: %s #%s", ClassNameOf(items).c_str(),
                std::wstring(items.Name()).c_str());
        TrackElement(handle, items, ElementKind::TrayItems);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Layout dump for troubleshooting. Written with DebugLog on, a few seconds
// after the taskbar connects, the settings change or a drag ends.

std::atomic<ULONGLONG> g_dumpDueTick{0};

void RequestDump(DWORD delayMs) {
    if (g_settings.debugLog) {
        g_dumpDueTick = GetTickCount64() + delayMs;
    }
}

void DumpElement(std::wstring& out,
                 wux::DependencyObject const& node,
                 wux::UIElement const& root,
                 int depth,
                 int& budget) {
    if (budget <= 0 || depth > 30) {
        return;
    }
    if (auto element = node.try_as<wux::FrameworkElement>()) {
        budget--;
        double x = std::nan("");
        double y = std::nan("");
        try {
            auto point =
                element.TransformToVisual(root).TransformPoint({0, 0});
            x = point.X;
            y = point.Y;
        } catch (...) {
        }
        auto margin = element.Margin();
        WCHAR line[768];
        _snwprintf(line, ARRAYSIZE(line) - 1,
                   L"%*s%s #%s at %.1f,%.1f size %.1fx%.1f h=%.1f vis=%d "
                   L"m=%.0f,%.0f,%.0f,%.0f%s\r\n",
                   depth * 2, L"", ClassNameOf(element).c_str(),
                   std::wstring(element.Name()).c_str(), x, y,
                   element.ActualWidth(), element.ActualHeight(),
                   element.Height(),
                   element.Visibility() == wux::Visibility::Visible ? 1 : 0,
                   margin.Left, margin.Top, margin.Right, margin.Bottom,
                   element.RenderTransform() ? L" rt" : L"");
        line[ARRAYSIZE(line) - 1] = L'\0';
        out += line;
    }
    int count = wuxm::VisualTreeHelper::GetChildrenCount(node);
    for (int i = 0; i < count; i++) {
        DumpElement(out, wuxm::VisualTreeHelper::GetChild(node, i), root,
                    depth + 1, budget);
    }
}

// Renders the XAML tree of the taskbar to a 32 bit BMP file. The taskbar
// window may be hidden by auto-hide, where screen capture only sees black.
// Composition brushes such as the acrylic background aren't part of the
// render, so empty areas come out black.
winrt::fire_and_forget WriteSnapshot(wux::UIElement root, std::wstring fileName) {
    try {
        wuxm::Imaging::RenderTargetBitmap bitmap;
        co_await bitmap.RenderAsync(root);
        auto pixels = co_await bitmap.GetPixelsAsync();
        int width = bitmap.PixelWidth();
        int height = bitmap.PixelHeight();
        DWORD imageSize = static_cast<DWORD>(width) * height * 4;
        if (width <= 0 || height <= 0 || pixels.Length() < imageSize) {
            LogLine(L"Snapshot empty %dx%d", width, height);
            co_return;
        }
        BITMAPFILEHEADER fileHeader{};
        BITMAPINFOHEADER infoHeader{};
        fileHeader.bfType = 0x4D42;
        fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(infoHeader);
        fileHeader.bfSize = fileHeader.bfOffBits + imageSize;
        infoHeader.biSize = sizeof(infoHeader);
        infoHeader.biWidth = width;
        infoHeader.biHeight = -height;
        infoHeader.biPlanes = 1;
        infoHeader.biBitCount = 32;
        infoHeader.biCompression = BI_RGB;
        std::wstring path = DebugFilePath(fileName.c_str());
        HANDLE file = CreateFile(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                 nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                 nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            co_return;
        }
        DWORD written;
        WriteFile(file, &fileHeader, sizeof(fileHeader), &written, nullptr);
        WriteFile(file, &infoHeader, sizeof(infoHeader), &written, nullptr);
        WriteFile(file, pixels.data(), imageSize, &written, nullptr);
        CloseHandle(file);
        LogLine(L"Snapshot written to %s, %dx%d", fileName.c_str(), width,
                height);
    } catch (...) {
        LogLine(L"Snapshot error %08X", winrt::to_hresult());
    }
}

void WINAPI DumpForCurrentThread(PVOID) {
    try {
        std::vector<wux::UIElement> roots;
        for (auto& entry : g_tracked) {
            if (entry.kind != ElementKind::Frame &&
                entry.kind != ElementKind::TrayHost) {
                continue;
            }
            auto element = entry.element.get();
            auto xamlRoot = element ? element.XamlRoot() : nullptr;
            auto content = xamlRoot ? xamlRoot.Content() : nullptr;
            if (content &&
                std::find(roots.begin(), roots.end(), content) == roots.end()) {
                roots.push_back(content);
            }
        }
        for (auto& root : roots) {
            SYSTEMTIME time;
            GetLocalTime(&time);
            WCHAR fileName[64];
            _snwprintf(fileName, ARRAYSIZE(fileName) - 1,
                       L"xaml-dump-%02d%02d%02d-t%u.txt", time.wHour,
                       time.wMinute, time.wSecond, GetCurrentThreadId());
            fileName[ARRAYSIZE(fileName) - 1] = L'\0';
            WCHAR header[160];
            _snwprintf(header, ARRAYSIZE(header) - 1,
                       L"rows %d, multirow %d, row height %d\r\n",
                       g_effectiveRows.load(), g_settings.multirowLayout,
                       g_settings.rowHeight);
            header[ARRAYSIZE(header) - 1] = L'\0';
            std::wstring out = header;
            int budget = 4000;
            DumpElement(out, root, root, 0, budget);
            AppendToFile(fileName, out.c_str());
            LogLine(L"Layout dump written to %s", fileName);
            std::wstring snapshotName = fileName;
            snapshotName.replace(0, 9, L"snapshot");
            snapshotName.replace(snapshotName.size() - 4, 4, L".bmp");
            WriteSnapshot(root, snapshotName);
        }
    } catch (...) {
        LogLine(L"Dump error %08X", winrt::to_hresult());
    }
}

////////////////////////////////////////////////////////////////////////////////
// Tracking and callbacks

void OnTrackedPropertyChanged(wux::DependencyObject const& sender,
                              wux::DependencyProperty const& property) {
    if (g_selfSetting || g_unloading) {
        return;
    }
    try {
        auto element = sender.try_as<wux::FrameworkElement>();
        if (!element) {
            return;
        }
        TrackedElement* entry = FindTrackedByElement(element);
        if (!entry) {
            return;
        }
        // Windows set a new value. Remember it as the value to restore, then
        // put the mod's value back.
        UpdateSaved(*entry, element, property);
        ApplyHeight(*entry);
    } catch (...) {
        Wh_Log(L"Property callback error %08X", winrt::to_hresult());
    }
}

void TrackElement(InstanceHandle handle,
                  wux::FrameworkElement const& element,
                  ElementKind kind) {
    if (FindTracked(handle)) {
        return;
    }

    RememberCurrentThread();

    TrackedElement entry;
    entry.handle = handle;
    entry.kind = kind;
    entry.element = winrt::make_weak(element);

    if (kind == ElementKind::Frame || kind == ElementKind::Inner) {
        auto callback = wux::DependencyPropertyChangedCallback(
            &OnTrackedPropertyChanged);
        entry.heightToken = element.RegisterPropertyChangedCallback(
            wux::FrameworkElement::HeightProperty(), callback);
        entry.maxHeightToken = element.RegisterPropertyChangedCallback(
            wux::FrameworkElement::MaxHeightProperty(), callback);
    } else if (kind == ElementKind::Repeater) {
        entry.layoutUpdatedToken = element.LayoutUpdated(
            [handle](wf::IInspectable const&, wf::IInspectable const&) {
                if (g_inArrangeRows) {
                    return;
                }
                g_inArrangeRows = true;
                try {
                    if (TrackedElement* tracked = FindTracked(handle)) {
                        ArrangeRows(*tracked);
                    }
                } catch (...) {
                    Wh_Log(L"Rows error %08X", winrt::to_hresult());
                }
                g_selfSetting = false;
                g_inArrangeRows = false;
            });
    } else if (kind == ElementKind::TrayItems) {
        entry.layoutUpdatedToken = element.LayoutUpdated(
            [handle](wf::IInspectable const&, wf::IInspectable const&) {
                if (g_inArrangeRows) {
                    return;
                }
                g_inArrangeRows = true;
                try {
                    if (TrackedElement* tracked = FindTracked(handle)) {
                        ApplyTrayRows(*tracked);
                    }
                } catch (...) {
                    Wh_Log(L"Tray rows error %08X", winrt::to_hresult());
                }
                g_selfSetting = false;
                g_inArrangeRows = false;
            });
    }

    g_tracked.push_back(std::move(entry));
    TrackedElement& added = g_tracked.back();

    if (kind == ElementKind::Frame || kind == ElementKind::Inner) {
        ApplyHeight(added);
        if (kind == ElementKind::Frame) {
            RequestTaskbarRecalc();
            RequestDump(4000);
        }
    } else if (kind == ElementKind::Icon) {
        ApplyIcon(added);
    } else if (kind == ElementKind::TrayHost) {
        ApplyTrayHost(added);
    } else if (kind == ElementKind::TrayItems) {
        ApplyTrayRows(added);
    }
}

void UntrackEntry(TrackedElement& entry, bool restore) {
    auto element = entry.element.get();
    if (!element) {
        return;
    }
    try {
        if (entry.heightToken) {
            element.UnregisterPropertyChangedCallback(
                wux::FrameworkElement::HeightProperty(), entry.heightToken);
        }
        if (entry.maxHeightToken) {
            element.UnregisterPropertyChangedCallback(
                wux::FrameworkElement::MaxHeightProperty(),
                entry.maxHeightToken);
        }
        if (entry.layoutUpdatedToken) {
            element.LayoutUpdated(entry.layoutUpdatedToken);
        }
    } catch (...) {
        Wh_Log(L"Untrack error %08X", winrt::to_hresult());
    }
    entry.heightToken = 0;
    entry.maxHeightToken = 0;
    entry.layoutUpdatedToken = {};
    if (restore) {
        if (entry.kind == ElementKind::Repeater) {
            UndoRows(entry);
        }
        RestoreSaved(entry, element);
        entry.rowsApplied = false;
    }
}

void ForgetHandle(InstanceHandle handle) {
    for (auto it = g_tracked.begin(); it != g_tracked.end(); ++it) {
        if (it->handle == handle) {
            UntrackEntry(*it, /*restore=*/false);
            if (it->kind == ElementKind::Repeater) {
                ClearRowTransforms();
            }
            g_tracked.erase(it);
            return;
        }
    }
}

// Re-applies the current settings to everything tracked on this thread.
void WINAPI ApplyAllForCurrentThread(PVOID) {
    try {
        for (auto& entry : g_tracked) {
            switch (entry.kind) {
                case ElementKind::Frame:
                case ElementKind::Inner:
                    ApplyHeight(entry);
                    break;
                case ElementKind::Icon:
                    ApplyIcon(entry);
                    break;
                case ElementKind::Repeater:
                    if (auto element = entry.element.get()) {
                        element.InvalidateMeasure();
                    }
                    break;
                case ElementKind::TrayHost:
                case ElementKind::TrayItems:
                    break;
            }
        }
        // TrayHost can add new entries, so the tray goes in a second pass
        // by index.
        for (size_t i = 0; i < g_tracked.size(); i++) {
            if (g_tracked[i].kind == ElementKind::TrayHost) {
                ApplyTrayHost(g_tracked[i]);
            } else if (g_tracked[i].kind == ElementKind::TrayItems) {
                ApplyTrayRows(g_tracked[i]);
            }
        }
    } catch (...) {
        Wh_Log(L"Apply error %08X", winrt::to_hresult());
    }
}

void WINAPI RestoreAllForCurrentThread(PVOID) {
    for (auto& entry : g_tracked) {
        try {
            UntrackEntry(entry, /*restore=*/true);
        } catch (...) {
            Wh_Log(L"Restore error %08X", winrt::to_hresult());
        }
    }
    ClearRowTransforms();
    g_tracked.clear();
}

// Classifies an element reported by XAML diagnostics.
void OnElementAdded(InstanceHandle handle,
                    wux::FrameworkElement const& element,
                    PCWSTR type) {
    std::wstring_view typeName = type ? type : L"";
    if (typeName == L"Taskbar.TaskbarFrame") {
        TrackElement(handle, element, ElementKind::Frame);
        return;
    }

    std::wstring name(element.Name());
    if (name == L"NotificationAreaIcons") {
        TrackElement(handle, element, ElementKind::TrayHost);
        return;
    }

    bool innerCandidate = name == L"RootGrid" ||
                          name == L"BackgroundControl" ||
                          name == L"BackgroundFill" ||
                          typeName == L"Taskbar.TaskbarBackground";
    bool repeaterCandidate = name == L"TaskbarFrameRepeater";
    bool iconCandidate = name == L"Icon";
    if (!innerCandidate && !repeaterCandidate && !iconCandidate) {
        return;
    }

    if (iconCandidate) {
        auto parent = wuxm::VisualTreeHelper::GetParent(element)
                          .try_as<wux::FrameworkElement>();
        if (!parent || parent.Name() != L"IconPanel" ||
            !IsInsideTaskbarFrame(element)) {
            return;
        }
        TrackElement(handle, element, ElementKind::Icon);
        return;
    }

    if (!IsInsideTaskbarFrame(element)) {
        return;
    }

    TrackElement(handle, element,
                 repeaterCandidate ? ElementKind::Repeater
                                   : ElementKind::Inner);
}

////////////////////////////////////////////////////////////////////////////////
// XAML diagnostics. Adapted from Windows 11 Taskbar Styler, which is based on
// ExplorerTAP from TranslucentTB. No symbols are involved: the entry point is
// the exported InitializeXamlDiagnosticsEx function of Windows.UI.Xaml.dll.

// See the Styler for the full story: the composition diagnostics corrupt the
// heap when several UI threads add visuals, so they're reported as disabled
// while AdviseVisualTreeChange reads the setting.
thread_local bool g_reportCompositionDiagAsDisabled;

class VisualTreeWatcher
    : public winrt::implements<VisualTreeWatcher,
                               IVisualTreeServiceCallback2,
                               winrt::non_agile> {
   public:
    VisualTreeWatcher(winrt::com_ptr<IUnknown> site);
    void UnadviseVisualTreeChange();

   private:
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(
        ParentChildRelation relation,
        VisualElement element,
        VisualMutationType mutationType) override;
    HRESULT STDMETHODCALLTYPE
    OnElementStateChanged(InstanceHandle element,
                          VisualElementState elementState,
                          LPCWSTR context) noexcept override;

    winrt::com_ptr<IXamlDiagnostics> m_xamlDiagnostics = nullptr;
};

winrt::com_ptr<VisualTreeWatcher> g_visualTreeWatcher;

VisualTreeWatcher::VisualTreeWatcher(winrt::com_ptr<IUnknown> site)
    : m_xamlDiagnostics(site.as<IXamlDiagnostics>()) {
    // Calling AdviseVisualTreeChange from this thread can hang, the Styler
    // calls it from a new thread for that reason.
    HANDLE thread = CreateThread(
        nullptr, 0,
        [](LPVOID lpParam) -> DWORD {
            auto watcher = reinterpret_cast<VisualTreeWatcher*>(lpParam);
            auto service =
                watcher->m_xamlDiagnostics.as<IVisualTreeService3>();
            g_reportCompositionDiagAsDisabled = true;
            HRESULT hr = service->AdviseVisualTreeChange(watcher);
            g_reportCompositionDiagAsDisabled = false;
            watcher->Release();
            if (FAILED(hr)) {
                Wh_Log(L"AdviseVisualTreeChange failed %08X", hr);
            }
            return 0;
        },
        this, 0, nullptr);
    if (thread) {
        AddRef();
        CloseHandle(thread);
    }
}

void VisualTreeWatcher::UnadviseVisualTreeChange() {
    HRESULT hr = m_xamlDiagnostics.as<IVisualTreeService3>()
                     ->UnadviseVisualTreeChange(this);
    if (FAILED(hr)) {
        Wh_Log(L"UnadviseVisualTreeChange failed %08X", hr);
    }
}

HRESULT VisualTreeWatcher::OnVisualTreeChange(ParentChildRelation,
                                              VisualElement element,
                                              VisualMutationType mutationType)
try {
    if (g_unloading) {
        return S_OK;
    }

    if (mutationType == Add) {
        // Cheap filter on the type and name before any runtime object work.
        std::wstring_view type = element.Type ? element.Type : L"";
        std::wstring_view name = element.Name ? element.Name : L"";
        bool interesting = type == L"Taskbar.TaskbarFrame" ||
                           type == L"Taskbar.TaskbarBackground" ||
                           name == L"RootGrid" ||
                           name == L"BackgroundControl" ||
                           name == L"BackgroundFill" ||
                           name == L"TaskbarFrameRepeater" ||
                           name == L"NotificationAreaIcons" ||
                           (name == L"Icon" && g_settings.iconSize > 0);
        if (!interesting) {
            return S_OK;
        }

        wf::IInspectable inspectable;
        winrt::check_hresult(m_xamlDiagnostics->GetIInspectableFromHandle(
            element.Handle,
            reinterpret_cast<::IInspectable**>(winrt::put_abi(inspectable))));
        if (auto frameworkElement =
                inspectable.try_as<wux::FrameworkElement>()) {
            OnElementAdded(element.Handle, frameworkElement, element.Type);
        }
    } else if (mutationType == Remove) {
        ForgetHandle(element.Handle);
    }

    return S_OK;
} catch (...) {
    Wh_Log(L"OnVisualTreeChange error %08X", winrt::to_hresult());
    // Returning an error stops further reports, always return success.
    return S_OK;
}

HRESULT VisualTreeWatcher::OnElementStateChanged(InstanceHandle,
                                                 VisualElementState,
                                                 LPCWSTR) noexcept {
    return S_OK;
}

// {6E1B3C52-8F0D-4B7A-9C21-5D3E7A4F9B10}, unique to this mod so it can't
// collide with the Styler's TAP.
static constexpr CLSID CLSID_TaskbarRowsTAP = {
    0x6e1b3c52,
    0x8f0d,
    0x4b7a,
    {0x9c, 0x21, 0x5d, 0x3e, 0x7a, 0x4f, 0x9b, 0x10}};

class TaskbarRowsTAP : public winrt::implements<TaskbarRowsTAP,
                                                IObjectWithSite,
                                                winrt::non_agile> {
   public:
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown* pUnkSite) override try {
        if (g_visualTreeWatcher) {
            g_visualTreeWatcher->UnadviseVisualTreeChange();
            g_visualTreeWatcher = nullptr;
        }
        site.copy_from(pUnkSite);
        if (site) {
            // InitializeXamlDiagnosticsEx loaded this dll once more.
            FreeLibrary(GetCurrentModuleHandle());
            g_visualTreeWatcher = winrt::make_self<VisualTreeWatcher>(site);
        }
        return S_OK;
    } catch (...) {
        return winrt::to_hresult();
    }

    HRESULT STDMETHODCALLTYPE GetSite(REFIID riid,
                                      void** ppvSite) noexcept override {
        return site.as(riid, ppvSite);
    }

   private:
    winrt::com_ptr<IUnknown> site;
};

template <class T>
struct SimpleFactory
    : winrt::implements<SimpleFactory<T>, IClassFactory, winrt::non_agile> {
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter,
                                             REFIID riid,
                                             void** ppvObject) override try {
        if (pUnkOuter) {
            return CLASS_E_NOAGGREGATION;
        }
        *ppvObject = nullptr;
        return winrt::make<T>().as(riid, ppvObject);
    } catch (...) {
        return winrt::to_hresult();
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override {
        return S_OK;
    }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdll-attribute-on-redeclaration"

__declspec(dllexport) _Use_decl_annotations_ STDAPI
    DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) try {
    if (rclsid == CLSID_TaskbarRowsTAP) {
        *ppv = nullptr;
        return winrt::make<SimpleFactory<TaskbarRowsTAP>>().as(riid, ppv);
    }
    return CLASS_E_CLASSNOTAVAILABLE;
} catch (...) {
    return winrt::to_hresult();
}

__declspec(dllexport) _Use_decl_annotations_ STDAPI DllCanUnloadNow() {
    return winrt::get_module_lock() ? S_FALSE : S_OK;
}

#pragma clang diagnostic pop

using PFN_INITIALIZE_XAML_DIAGNOSTICS_EX =
    decltype(&InitializeXamlDiagnosticsEx);

HRESULT InjectTAP() noexcept {
    HMODULE module = GetCurrentModuleHandle();
    if (!module) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    WCHAR location[MAX_PATH];
    switch (GetModuleFileName(module, location, ARRAYSIZE(location))) {
        case 0:
        case ARRAYSIZE(location):
            return HRESULT_FROM_WIN32(GetLastError());
    }

    HMODULE wux = LoadLibraryEx(L"Windows.UI.Xaml.dll", nullptr,
                                LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!wux) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto ixde = reinterpret_cast<PFN_INITIALIZE_XAML_DIAGNOSTICS_EX>(
        GetProcAddress(wux, "InitializeXamlDiagnosticsEx"));
    if (!ixde) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Each XAML thread has its own connection name, try them in turn. Same
    // approach as the Styler and ExplorerTAP.
    HRESULT hr = E_FAIL;
    for (int i = 0; i < 10000; i++) {
        WCHAR connectionName[256];
        wsprintf(connectionName, L"VisualDiagConnection%d", i + 1);
        hr = ixde(connectionName, GetCurrentProcessId(), L"", location,
                  CLSID_TaskbarRowsTAP, nullptr);
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
            break;
        }
    }
    return hr;
}

void InjectTAPOnce() {
    if (g_tapInjected.exchange(true)) {
        return;
    }
    HRESULT hr = InjectTAP();
    if (FAILED(hr)) {
        Wh_Log(L"InjectTAP failed %08X", hr);
        // Leave the flag set: a failing injection isn't retried in a loop.
    }
}

////////////////////////////////////////////////////////////////////////////////
// Running work on every UI thread that holds tracked elements.

HWND FindAnyWindowOfThread(DWORD threadId) {
    HWND found = nullptr;
    EnumThreadWindows(
        threadId,
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            *reinterpret_cast<HWND*>(lParam) = hWnd;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&found));
    return found;
}

void RunOnTrackedThreads(RunFromWindowThreadProc_t proc) {
    std::vector<DWORD> threads;
    {
        std::lock_guard<std::mutex> lock(g_threadsMutex);
        threads = g_trackedThreads;
    }
    for (DWORD threadId : threads) {
        HWND hWnd = FindAnyWindowOfThread(threadId);
        if (!hWnd || !RunFromWindowThread(hWnd, proc, nullptr)) {
            Wh_Log(L"Can't reach UI thread %u", threadId);
        }
    }
}

void ApplyRowsEverywhere(bool forceRecalc = false) {
    RunOnTrackedThreads(ApplyAllForCurrentThread);
    RequestTaskbarRecalc(forceRecalc);
}

////////////////////////////////////////////////////////////////////////////////
// Phase 3: the drag grip. A thin topmost strip inside the edge of the taskbar
// that faces the desktop. It never draws anything: alpha 1 on a black
// background, set once when the window is created. Dragging it changes the row
// count in whole rows. All coordinates are physical pixels.

// Alpha 1 keeps the window invisible but still lets it take mouse input.
// Alpha 0 would make it click-through.
constexpr BYTE kGripAlpha = 1;
// The grip only shows after the taskbar has kept the same place, fully on
// screen, for this long. The auto-hide slide never shows it for a moment.
constexpr ULONGLONG kGripSettleMs = 300;
// Something else (explorer or another mod) can raise the taskbar above the
// grip. The grip raises itself back at most this often, so the two never
// fight on every tick.
constexpr ULONGLONG kGripRaiseGapMs = 1000;
constexpr UINT kGripTimerMs = 100;

HANDLE g_gripThread;
std::atomic<DWORD> g_gripThreadId{0};
std::atomic<HWND> g_gripWnd{nullptr};
bool g_dragging;
int g_dragStartY;
int g_dragStartRows;
bool g_dragTaskbarAtBottom;
bool g_gripHover;

struct TaskbarPlacement {
    HWND hWnd = nullptr;
    RECT rect{};
    RECT monitorRect{};
    bool atBottom = true;
    UINT dpi = 96;
};

bool GetTaskbarPlacement(TaskbarPlacement* placement) {
    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (!hTaskbarWnd || !IsWindowVisible(hTaskbarWnd) ||
        !IsTaskbarHorizontal(hTaskbarWnd)) {
        return false;
    }
    RECT rect{};
    GetWindowRect(hTaskbarWnd, &rect);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    HMONITOR monitor = MonitorFromWindow(hTaskbarWnd, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfo(monitor, &monitorInfo)) {
        return false;
    }
    placement->hWnd = hTaskbarWnd;
    placement->rect = rect;
    placement->monitorRect = monitorInfo.rcMonitor;
    placement->atBottom =
        rect.bottom >= monitorInfo.rcMonitor.bottom - 1 ||
        rect.top > (monitorInfo.rcMonitor.top + monitorInfo.rcMonitor.bottom) / 2;
    placement->dpi = GetDpiForWindow(hTaskbarWnd);
    if (!placement->dpi) {
        placement->dpi = 96;
    }
    return true;
}

bool IsFullscreenWindowOnTaskbarMonitor(HWND hTaskbarWnd) {
    HWND foreground = GetForegroundWindow();
    // With auto-hide the work area is the whole screen, so a maximized window
    // covers the monitor too. That isn't a full screen app.
    if (!foreground || foreground == hTaskbarWnd || IsZoomed(foreground)) {
        return false;
    }
    WCHAR className[32];
    if (GetClassName(foreground, className, ARRAYSIZE(className)) &&
        (_wcsicmp(className, L"Progman") == 0 ||
         _wcsicmp(className, L"WorkerW") == 0 ||
         // Start, Search and other shell flyouts. They cover the monitor
         // while the taskbar stays on screen.
         _wcsicmp(className, L"Windows.UI.Core.CoreWindow") == 0)) {
        return false;
    }
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (!GetMonitorInfo(MonitorFromWindow(hTaskbarWnd, MONITOR_DEFAULTTONEAREST),
                        &monitorInfo)) {
        return false;
    }
    RECT rect{};
    if (!GetWindowRect(foreground, &rect)) {
        return false;
    }
    return rect.left <= monitorInfo.rcMonitor.left &&
           rect.top <= monitorInfo.rcMonitor.top &&
           rect.right >= monitorInfo.rcMonitor.right &&
           rect.bottom >= monitorInfo.rcMonitor.bottom;
}

bool IsTaskbarAutoHide(HWND hTaskbarWnd) {
    APPBARDATA abd{sizeof(abd)};
    abd.hWnd = hTaskbarWnd;
    return (SHAppBarMessage(ABM_GETSTATE, &abd) & ABS_AUTOHIDE) != 0;
}

// Only the grip thread calls this. The state is logged when it changes, so the
// timer doesn't flood the log.
void SetGripState(HWND hGripWnd, bool shown, PCWSTR reason) {
    static int lastShown = -1;
    static PCWSTR lastReason = nullptr;
    if (lastShown != static_cast<int>(shown) || lastReason != reason) {
        lastShown = shown;
        lastReason = reason;
        LogLine(L"Grip %s: %s", shown ? L"shown" : L"hidden", reason);
    }
    if (!shown && IsWindowVisible(hGripWnd)) {
        ShowWindow(hGripWnd, SW_HIDE);
    }
}

// True when the grip is above the taskbar in the z-order.
bool IsGripAboveTaskbar(HWND hGripWnd, HWND hTaskbarWnd) {
    for (HWND hWnd = GetWindow(hGripWnd, GW_HWNDNEXT); hWnd;
         hWnd = GetWindow(hWnd, GW_HWNDNEXT)) {
        if (hWnd == hTaskbarWnd) {
            return true;
        }
    }
    return false;
}

void UpdateGripWindowPosition(HWND hGripWnd) {
    if (g_unloading || !g_settings.edgeResizeEnabled) {
        SetGripState(hGripWnd, false, L"disabled");
        return;
    }
    TaskbarPlacement placement;
    if (!GetTaskbarPlacement(&placement)) {
        SetGripState(hGripWnd, false, L"no visible horizontal taskbar");
        return;
    }
    // A full screen window must stay that way for kGripSettleMs before the
    // grip hides, so a short focus change doesn't hide and show it.
    static ULONGLONG fullscreenSince = 0;
    if (IsFullscreenWindowOnTaskbarMonitor(placement.hWnd)) {
        ULONGLONG now = GetTickCount64();
        if (!fullscreenSince) {
            fullscreenSince = now;
        }
        if (now - fullscreenSince >= kGripSettleMs) {
            SetGripState(hGripWnd, false, L"full screen window");
        }
        return;
    }
    fullscreenSince = 0;

    // An auto-hidden taskbar leaves a 2 px strip on screen that explorer uses
    // to bring it back. The grip mustn't cover that strip, so it only shows
    // while the whole taskbar is on screen. A slide out hides it once.
    RECT& r = placement.rect;
    static RECT settleRect{};
    static ULONGLONG settleSince = 0;
    RECT visible{};
    if (!IntersectRect(&visible, &r, &placement.monitorRect) ||
        visible.bottom - visible.top < r.bottom - r.top - 1) {
        SetRectEmpty(&settleRect);
        SetGripState(hGripWnd, false,
                     IsTaskbarAutoHide(placement.hWnd)
                         ? L"auto-hide, taskbar not fully shown"
                         : L"taskbar not fully on screen");
        return;
    }

    // The taskbar is fully on screen. It must keep the same place for
    // kGripSettleMs before the grip shows or moves, so the end of a slide or
    // a height change never shows, hides or moves it on every frame. A grip
    // that's already shown stays put meanwhile.
    ULONGLONG now = GetTickCount64();
    if (!EqualRect(&settleRect, &r)) {
        settleRect = r;
        settleSince = now;
    }
    if (now - settleSince < kGripSettleMs) {
        return;
    }

    // The grip sits inside the taskbar, on its top edge for a bottom taskbar.
    // SetWindowPos only runs when something changed: the place, the
    // visibility, or the z-order against the taskbar. Raising is rate limited.
    int grip = MulDiv(g_settings.edgeResizeGripPixels, placement.dpi, 96);
    int top = placement.atBottom ? r.top : r.bottom - grip;
    RECT wanted{r.left, top, r.right, top + grip};
    RECT current{};
    GetWindowRect(hGripWnd, &current);
    bool samePlace = EqualRect(&current, &wanted) != FALSE;
    bool shown = IsWindowVisible(hGripWnd) != FALSE;
    bool above = IsGripAboveTaskbar(hGripWnd, placement.hWnd);
    static ULONGLONG lastRaise = 0;
    bool raise = !shown || (!above && now - lastRaise >= kGripRaiseGapMs);
    if (!samePlace || raise) {
        UINT flags = SWP_NOACTIVATE;
        if (samePlace) {
            flags |= SWP_NOMOVE | SWP_NOSIZE;
        }
        if (!shown) {
            flags |= SWP_SHOWWINDOW;
        }
        if (raise) {
            lastRaise = now;
        } else {
            flags |= SWP_NOZORDER;
        }
        SetWindowPos(hGripWnd, HWND_TOPMOST, wanted.left, wanted.top,
                     wanted.right - wanted.left, grip, flags);
        LogLine(L"Grip placed at %d,%d,%d,%d (moved %d, was hidden %d, "
                L"raised %d)",
                wanted.left, wanted.top, wanted.right, wanted.bottom,
                !samePlace, !shown, raise && shown);
    }
    SetGripState(hGripWnd, true, L"taskbar on screen");
}

void FinishDrag(HWND hWnd, PCWSTR reason) {
    if (!g_dragging) {
        return;
    }
    g_dragging = false;
    if (GetCapture() == hWnd) {
        ReleaseCapture();
    }
    int rows = g_effectiveRows.load();
    Wh_Log(L"Drag end (%s), rows %d, was %d", reason, rows, g_dragStartRows);
    Wh_SetIntValue(L"EdgeResizeRows", rows);
    Wh_SetIntValue(L"EdgeResizeRowsSetting", g_settings.rows);
    // Once more without the throttle, so the last row change during the drag
    // always reaches the XAML height and explorer.
    ApplyRowsEverywhere(/*forceRecalc=*/true);
    UpdateGripWindowPosition(hWnd);
    RequestDump(2500);
}

LRESULT CALLBACK GripWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hWnd, 1, kGripTimerMs, nullptr);
            return 0;

        case WM_TIMER:
            if (!g_dragging) {
                UpdateGripWindowPosition(hWnd);
            }
            if (ULONGLONG due = g_dumpDueTick; due && GetTickCount64() >= due) {
                g_dumpDueTick = 0;
                RunOnTrackedThreads(DumpForCurrentThread);
            }
            return 0;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_SETCURSOR:
            SetCursor(LoadCursor(nullptr, IDC_SIZENS));
            return TRUE;

        case WM_LBUTTONDOWN: {
            POINT pt{};
            GetCursorPos(&pt);
            TaskbarPlacement placement;
            if (!GetTaskbarPlacement(&placement)) {
                Wh_Log(L"Mouse down at %d,%d ignored, no taskbar", pt.x, pt.y);
                return 0;
            }
            g_dragging = true;
            g_dragStartY = pt.y;
            g_dragStartRows = g_effectiveRows;
            g_dragTaskbarAtBottom = placement.atBottom;
            SetCapture(hWnd);
            Wh_Log(L"Drag start at %d,%d, rows %d, taskbar %d..%d, dpi %u",
                   pt.x, pt.y, g_dragStartRows, placement.rect.top,
                   placement.rect.bottom, placement.dpi);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!g_dragging) {
                // Proves the grip gets mouse input. Logged once per hover.
                if (!g_gripHover) {
                    g_gripHover = true;
                    TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hWnd, 0};
                    TrackMouseEvent(&tme);
                    Wh_Log(L"Mouse entered the grip at %d,%d",
                           GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                }
                return 0;
            }
            TaskbarPlacement placement;
            if (!GetTaskbarPlacement(&placement)) {
                return 0;
            }
            POINT pt{};
            GetCursorPos(&pt);
            // Cursor and taskbar positions are both physical pixels, so the
            // row height is scaled by the taskbar DPI as well.
            int delta = g_dragTaskbarAtBottom ? g_dragStartY - pt.y
                                              : pt.y - g_dragStartY;
            int rowPixels = MulDiv(g_settings.rowHeight, placement.dpi, 96);
            if (rowPixels <= 0) {
                return 0;
            }
            int steps = static_cast<int>(
                std::lround(static_cast<double>(delta) / rowPixels));
            int rows = ClampInt(g_dragStartRows + steps, g_settings.minRows,
                                g_settings.maxRows);
            Wh_Log(L"Drag move y %d, delta %d px, row %d px, rows %d", pt.y,
                   delta, rowPixels, rows);
            if (rows != g_effectiveRows) {
                g_effectiveRows = rows;
                Wh_Log(L"Rows changed to %d, height %d", rows,
                       static_cast<int>(TargetFrameHeight()));
                ApplyRowsEverywhere();
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            g_gripHover = false;
            return 0;

        case WM_LBUTTONUP:
            Wh_Log(L"Mouse up");
            FinishDrag(hWnd, L"button up");
            return 0;

        case WM_CAPTURECHANGED:
            if (g_dragging && reinterpret_cast<HWND>(lParam) != hWnd) {
                FinishDrag(hWnd, L"capture lost");
            }
            return 0;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

DWORD WINAPI GripThreadProc(void*) {
    // Same coordinate space as the taskbar window and GetCursorPos.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASS wc{};
    wc.lpfnWndProc = GripWndProc;
    wc.hInstance = GetCurrentModuleHandle();
    wc.hCursor = LoadCursor(nullptr, IDC_SIZENS);
    // A stock brush, never deleted. At alpha 1 it leaves no visible mark.
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = L"WindhawkTaskbarRowsGrip";
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        wc.lpszClassName, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
        wc.hInstance, nullptr);
    if (!hWnd) {
        Wh_Log(L"Grip window creation failed %u", GetLastError());
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 0;
    }

    // Set once here and never changed afterwards.
    SetLayeredWindowAttributes(hWnd, 0, kGripAlpha, LWA_ALPHA);
    Wh_Log(L"Grip window created");
    g_gripWnd = hWnd;
    UpdateGripWindowPosition(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_gripWnd = nullptr;
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

void StartGrip() {
    if (g_gripThread || !g_settings.edgeResizeEnabled) {
        return;
    }
    DWORD threadId = 0;
    g_gripThread =
        CreateThread(nullptr, 0, GripThreadProc, nullptr, 0, &threadId);
    g_gripThreadId = threadId;
}

void StopGrip() {
    if (!g_gripThread) {
        return;
    }
    HWND hWnd = g_gripWnd;
    if (hWnd) {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
    } else if (DWORD threadId = g_gripThreadId) {
        PostThreadMessage(threadId, WM_QUIT, 0, 0);
    }
    if (WaitForSingleObject(g_gripThread, 5000) != WAIT_OBJECT_0) {
        Wh_Log(L"Grip thread didn't stop in time");
    }
    CloseHandle(g_gripThread);
    g_gripThread = nullptr;
    g_gripThreadId = 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function hooks. All of them are exported Windows functions found with
// GetProcAddress, none needs a symbol.

using RegOpenKeyExW_t = decltype(&RegOpenKeyExW);
RegOpenKeyExW_t RegOpenKeyExW_Original;
LSTATUS WINAPI RegOpenKeyExW_Hook(HKEY hKey,
                                  LPCWSTR lpSubKey,
                                  DWORD ulOptions,
                                  REGSAM samDesired,
                                  PHKEY phkResult) {
    LSTATUS result = RegOpenKeyExW_Original(hKey, lpSubKey, ulOptions,
                                            samDesired, phkResult);
    if (result == ERROR_SUCCESS || !g_reportCompositionDiagAsDisabled ||
        hKey != HKEY_LOCAL_MACHINE || !lpSubKey ||
        _wcsicmp(lpSubKey, L"Software\\Microsoft\\XAML\\Debug") != 0) {
        return result;
    }
    // Read only: hand out a key that exists so the value query below is made.
    return RegOpenKeyExW_Original(HKEY_LOCAL_MACHINE, L"Software\\Microsoft",
                                  ulOptions, samDesired, phkResult);
}

using RegQueryValueExW_t = decltype(&RegQueryValueExW);
RegQueryValueExW_t RegQueryValueExW_Original;
LSTATUS WINAPI RegQueryValueExW_Hook(HKEY hKey,
                                     LPCWSTR lpValueName,
                                     LPDWORD lpReserved,
                                     LPDWORD lpType,
                                     LPBYTE lpData,
                                     LPDWORD lpcbData) {
    if (!g_reportCompositionDiagAsDisabled || !lpValueName ||
        _wcsicmp(lpValueName, L"DisableCompositionDiag") != 0) {
        return RegQueryValueExW_Original(hKey, lpValueName, lpReserved, lpType,
                                         lpData, lpcbData);
    }
    if (lpType) {
        *lpType = REG_DWORD;
    }
    if (lpData && (!lpcbData || *lpcbData < sizeof(DWORD))) {
        if (lpcbData) {
            *lpcbData = sizeof(DWORD);
        }
        return ERROR_MORE_DATA;
    }
    if (lpData) {
        *reinterpret_cast<DWORD*>(lpData) = 1;
    }
    if (lpcbData) {
        *lpcbData = sizeof(DWORD);
    }
    return ERROR_SUCCESS;
}

// Secondary taskbars are placed with ABM_QUERYPOS. The Taskbar height and icon
// size mod adjusts the same call.
using SHAppBarMessage_t = decltype(&SHAppBarMessage);
SHAppBarMessage_t SHAppBarMessage_Original;
UINT_PTR WINAPI SHAppBarMessage_Hook(DWORD dwMessage, PAPPBARDATA pData) {
    UINT_PTR ret = SHAppBarMessage_Original(dwMessage, pData);
    if (dwMessage != ABM_QUERYPOS || !ret || !pData || !IsResizeActive()) {
        return ret;
    }
    WCHAR className[32];
    if (!GetClassName(pData->hWnd, className, ARRAYSIZE(className)) ||
        _wcsicmp(className, L"Shell_SecondaryTrayWnd") != 0) {
        return ret;
    }
    UINT dpi = GetDpiForWindow(pData->hWnd);
    if (!dpi) {
        return ret;
    }
    int height = MulDiv(static_cast<int>(TargetFrameHeight()), dpi, 96);
    if (pData->uEdge == ABE_BOTTOM) {
        pData->rc.top = pData->rc.bottom - height;
    } else if (pData->uEdge == ABE_TOP) {
        pData->rc.bottom = pData->rc.top + height;
    }
    return ret;
}

// After an explorer restart the taskbar is created after the mod loads. The
// XAML island of the taskbar is the DesktopWindowContentBridge child of
// Shell_TrayWnd. Same trigger the Styler uses.
using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t CreateWindowExW_Original;
HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle,
                                 LPCWSTR lpClassName,
                                 LPCWSTR lpWindowName,
                                 DWORD dwStyle,
                                 int X,
                                 int Y,
                                 int nWidth,
                                 int nHeight,
                                 HWND hWndParent,
                                 HMENU hMenu,
                                 HINSTANCE hInstance,
                                 PVOID lpParam) {
    HWND hWnd = CreateWindowExW_Original(dwExStyle, lpClassName, lpWindowName,
                                         dwStyle, X, Y, nWidth, nHeight,
                                         hWndParent, hMenu, hInstance, lpParam);
    if (!hWnd || !hWndParent || g_unloading || g_tapInjected) {
        return hWnd;
    }
    WCHAR className[64];
    if (GetClassName(hWnd, className, ARRAYSIZE(className)) &&
        _wcsicmp(className,
                 L"Windows.UI.Composition.DesktopWindowContentBridge") == 0 &&
        GetClassName(hWndParent, className, ARRAYSIZE(className)) &&
        _wcsicmp(className, L"Shell_TrayWnd") == 0) {
        Wh_Log(L"Taskbar XAML island created, connecting");
        InjectTAPOnce();
    }
    return hWnd;
}

////////////////////////////////////////////////////////////////////////////////
// Windhawk entry points

BOOL Wh_ModInit() {
    Wh_Log(L">");

    LoadSettings();

    // Each hook is optional. If one can't be set, the part that needs it stays
    // off and the mod still loads.

    if (!WindhawkUtils::SetFunctionHook(CreateWindowExW, CreateWindowExW_Hook,
                                        &CreateWindowExW_Original)) {
        Wh_Log(L"CreateWindowExW hook failed");
    }

    HMODULE kernelBase = GetModuleHandle(L"kernelbase.dll");
    if (kernelBase) {
        auto pRegOpenKeyExW =
            (RegOpenKeyExW_t)GetProcAddress(kernelBase, "RegOpenKeyExW");
        auto pRegQueryValueExW =
            (RegQueryValueExW_t)GetProcAddress(kernelBase, "RegQueryValueExW");
        if (pRegOpenKeyExW && pRegQueryValueExW) {
            WindhawkUtils::SetFunctionHook(pRegOpenKeyExW, RegOpenKeyExW_Hook,
                                           &RegOpenKeyExW_Original);
            WindhawkUtils::SetFunctionHook(pRegQueryValueExW,
                                           RegQueryValueExW_Hook,
                                           &RegQueryValueExW_Original);
        }
    }

    HMODULE shell32 =
        LoadLibraryEx(L"shell32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (shell32) {
        auto pSHAppBarMessage =
            (SHAppBarMessage_t)GetProcAddress(shell32, "SHAppBarMessage");
        if (pSHAppBarMessage) {
            WindhawkUtils::SetFunctionHook(pSHAppBarMessage,
                                           SHAppBarMessage_Hook,
                                           &SHAppBarMessage_Original);
        }
    }

    return TRUE;
}

void Wh_ModAfterInit() {
    Wh_Log(L">");
    LogLine(L"Loaded " WH_MOD_VERSION L": rows %d (effective %d), multirow %d, row height %d",
            g_settings.rows, g_effectiveRows.load(), g_settings.multirowLayout,
            g_settings.rowHeight);

    if (GetTaskbarUiWnd()) {
        Wh_Log(L"Taskbar XAML island found, connecting");
        InjectTAPOnce();
    }

    StartGrip();
}

void Wh_ModBeforeUninit() {
    Wh_Log(L">");
    g_unloading = true;
    StopGrip();
}

void Wh_ModUninit() {
    Wh_Log(L">");

    g_unloading = true;
    StopGrip();

    if (g_visualTreeWatcher) {
        g_visualTreeWatcher->UnadviseVisualTreeChange();
        g_visualTreeWatcher = nullptr;
    }

    RunOnTrackedThreads(RestoreAllForCurrentThread);

    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (hTaskbarWnd) {
        PostMessage(hTaskbarWnd, WM_SETTINGCHANGE, SPI_SETLOGICALDPIOVERRIDE,
                    0);
    }
}

void Wh_ModSettingsChanged() {
    Wh_Log(L">");

    int oldRowsSetting = g_settings.rows;
    LoadSettings();
    if (g_settings.rows != oldRowsSetting) {
        // A new Rows value in the settings wins over the dragged one.
        g_effectiveRows = g_settings.rows;
        Wh_SetIntValue(L"EdgeResizeRows", 0);
    }

    LogLine(L"Settings: rows %d (effective %d), multirow %d, row height %d",
            g_settings.rows, g_effectiveRows.load(), g_settings.multirowLayout,
            g_settings.rowHeight);

    if (g_settings.edgeResizeEnabled) {
        StartGrip();
    } else {
        StopGrip();
    }

    ApplyRowsEverywhere(/*forceRecalc=*/true);
    RequestDump(2500);
}
