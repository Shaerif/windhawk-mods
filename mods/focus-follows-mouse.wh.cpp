// ==WindhawkMod==
// @id              focus-follows-mouse
// @name            Focus Follows Mouse
// @description     Focus the window under the mouse without clicking. Optionally keep it from jumping to the front
// @version         0.1.0
// @author          Shaerif
// @include         explorer.exe
// @compilerOptions -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Focus Follows Mouse
Linux desktops can focus the window under the mouse without a click. Windows 11
has a weak version of this hidden in Accessibility settings. It always pulls the
window to the front.

This mod turns on the built-in Windows hover focus and gives you control over it.

## What it does
- Turns on "activate a window by hovering over it".
- Lets you pick the hover delay.
- Lets you keep the hovered window where it is in the stack. It gets keyboard
  focus but does not cover the windows around it. This is the main reason to use
  the mod.

## How it works
The mod uses three Windows system settings: active window tracking, the
tracking delay, and the raise-on-hover flag. It saves your current values when it
loads and puts them back when it unloads. It does not write to the registry.
Windows resets these values at the next sign in, and the mod sets them again each
time Explorer starts.

## Limits
- If you change these options in Windows Settings while the mod runs, the mod
  keeps its own values until you change a mod setting or reload it.
- Windows decides which windows count. Menus and the taskbar behave as Windows
  decides.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- enabled: true
  $name: Focus the window under the mouse
- delayMs: 300
  $name: Hover delay (milliseconds)
- raiseWindow: false
  $name: Bring the window to the front when it gets focus
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#ifndef SPI_GETACTIVEWINDOWTRACKING
#define SPI_GETACTIVEWINDOWTRACKING 0x1000
#define SPI_SETACTIVEWINDOWTRACKING 0x1001
#endif
#ifndef SPI_GETACTIVEWNDTRKZORDER
#define SPI_GETACTIVEWNDTRKZORDER 0x100C
#define SPI_SETACTIVEWNDTRKZORDER 0x100D
#endif
#ifndef SPI_GETACTIVEWNDTRKTIMEOUT
#define SPI_GETACTIVEWNDTRKTIMEOUT 0x2002
#define SPI_SETACTIVEWNDTRKTIMEOUT 0x2003
#endif

struct {
    bool enabled;
    int delayMs;
    bool raiseWindow;
} g_settings;

struct Saved {
    bool valid = false;
    BOOL tracking = FALSE;
    BOOL zorder = TRUE;
    DWORD timeout = 0;
} g_saved;

static void LoadSettings() {
    g_settings.enabled = Wh_GetIntSetting(L"enabled") != 0;
    g_settings.delayMs = Wh_GetIntSetting(L"delayMs");
    if (g_settings.delayMs < 0) g_settings.delayMs = 0;
    if (g_settings.delayMs > 5000) g_settings.delayMs = 5000;
    g_settings.raiseWindow = Wh_GetIntSetting(L"raiseWindow") != 0;
}

static void SaveOriginalOnce() {
    if (g_saved.valid) return;
    BOOL tracking = FALSE;
    BOOL zorder = TRUE;
    DWORD timeout = 0;
    bool ok = SystemParametersInfoW(SPI_GETACTIVEWINDOWTRACKING, 0, &tracking, 0) &&
              SystemParametersInfoW(SPI_GETACTIVEWNDTRKZORDER, 0, &zorder, 0) &&
              SystemParametersInfoW(SPI_GETACTIVEWNDTRKTIMEOUT, 0, &timeout, 0);
    if (!ok) {
        Wh_Log(L"Could not read the current hover focus settings");
        return;
    }
    g_saved.tracking = tracking;
    g_saved.zorder = zorder;
    g_saved.timeout = timeout;
    g_saved.valid = true;
    Wh_Log(L"Saved original: tracking=%d zorder=%d timeout=%lu", tracking, zorder, timeout);
}

static void RestoreOriginal() {
    if (!g_saved.valid) return;
    SystemParametersInfoW(SPI_SETACTIVEWINDOWTRACKING, 0, (PVOID)(INT_PTR)g_saved.tracking, 0);
    SystemParametersInfoW(SPI_SETACTIVEWNDTRKZORDER, 0, (PVOID)(INT_PTR)g_saved.zorder, 0);
    SystemParametersInfoW(SPI_SETACTIVEWNDTRKTIMEOUT, 0, (PVOID)(UINT_PTR)g_saved.timeout, 0);
    Wh_Log(L"Restored original hover focus settings");
}

static void Apply() {
    if (!g_settings.enabled) {
        RestoreOriginal();
        return;
    }
    SaveOriginalOnce();
    if (!g_saved.valid) return;
    SystemParametersInfoW(SPI_SETACTIVEWNDTRKZORDER, 0,
                          (PVOID)(INT_PTR)(g_settings.raiseWindow ? TRUE : FALSE), 0);
    SystemParametersInfoW(SPI_SETACTIVEWNDTRKTIMEOUT, 0, (PVOID)(UINT_PTR)g_settings.delayMs, 0);
    SystemParametersInfoW(SPI_SETACTIVEWINDOWTRACKING, 0, (PVOID)(INT_PTR)TRUE, 0);
    Wh_Log(L"Applied: delay=%d raise=%d", g_settings.delayMs, g_settings.raiseWindow ? 1 : 0);
}

BOOL Wh_ModInit() {
    LoadSettings();
    Apply();
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    Apply();
}

void Wh_ModUninit() {
    RestoreOriginal();
}
