# Off-Screen Window Rescue tutorial

This guide sets up the Off-Screen Window Rescue mod for Windhawk and walks you through one rescue. When you're done, a window that opens outside the visible screen gets moved back to the middle of your screen the moment it appears. It's for you if an installer, an old app, or an app that remembers a monitor you've since unplugged keeps opening where you can't reach it. The mod checks each window once, when it first appears, so it won't fight you when you drag a window near an edge later. The screenshot comes from a live run on Windows 11 build 26300, on a test window that opened at x = -3000.

## Contents
- [What it fixes](#what-it-fixes)
- [Before you start](#before-you-start)
- [Install](#install)
- [Use it in three steps](#use-it-in-three-steps)
- [The result and the settings behind it](#the-result-and-the-settings-behind-it)
- [Which windows it checks](#which-windows-it-checks)
- [Settings](#settings)
- [How it works under the hood](#how-it-works-under-the-hood)
- [Limits and troubleshooting](#limits-and-troubleshooting)
- [What's tested and what isn't](#whats-tested-and-what-isnt)
- [Wrap-up](#wrap-up)
- [Uninstall](#uninstall)
- [Use at your own risk](#use-at-your-own-risk)

## What it fixes
Some windows open at coordinates that don't exist on your screen. The window is running and you can see it in the taskbar, but you can't see it on screen or grab it with the mouse. Installers do this often. The first mod in this repo came from that exact problem. A Windows installer opened its window outside the visible area and wouldn't come back.

This mod moves a window like that back into view, once, when it first shows itself.

## Before you start
You need three things.

- Windows 11. The repo's mods are only tested on Windows 11, and nobody has tried this one on Windows 10.
- Windhawk, from [windhawk.net](https://windhawk.net).
- The file mods/offscreen-window-rescue.wh.cpp from this repo.
## Install
The repo describes two ways to add the mod as a local mod.

From the Windhawk app:
1. Open Windhawk and choose Create a new mod.
2. Paste the whole contents of offscreen-window-rescue.wh.cpp.
3. Compile and enable the mod.

From a shell you've opened as administrator:

```
windhawk-cli.exe mod install --file mods/offscreen-window-rescue.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine.

To check that it loaded, list your mods and look for local@offscreen-window-rescue, which is the id a local install gets.

```
windhawk-cli.exe mod list
```

The mod doesn't write a line when it loads. It only logs when it moves a window, and only while Log moves is on. It's on by default.

## Use it in three steps
The number matches the red badge in the corner of the screenshot.

### 1. Open the program whose window lands off screen
Start the program as you normally would. With the default Minimum visible share of 50, if less than half of its window is inside the work area of the nearest monitor, the mod moves the window to the center of that work area. The work area is the part of the screen the taskbar doesn't cover.

![The whole 2560 by 1600 screen, blank except for one test window in the middle in a red box. Its title says opened at x = -3000, now at 900, 580. Its text says it opened at -3000, 300, size 760 x 440, and measured now: position 900, 580, size 760 x 440, screen 2560 x 1600](screenshots/offscreen-window-rescue-step-1-after-rescue.png)

The picture is the whole 2560 by 1600 screen with a plain backdrop over it. The test window opened at -3000, 300, which is off the left edge. Right after it appeared, it read its own spot back from Windows and wrote it into its title and its text. It's at 900, 580 and still 760 by 440. For a window that size, that's the middle of a 2560 by 1600 screen.

There's no before screenshot. The window was off the screen then, so a capture of the screen would only show an empty backdrop and wouldn't prove anything.

The gotcha is a window that's already open. The mod checks a window only when it first appears. A window that was already off screen before you installed the mod stays where it is until you close the program and start it again.

### 2. Read the move in the Windhawk log
Look in the Windhawk log. Each move gets one line in this shape.

```
[ShowWindow] moved <window handle> from (left,top,right,bottom) visible=N% to (x,y)
```

The part in brackets is the call that showed the window, and it's ShowWindow, SetWindowPos or CreateWindowExW. The four numbers after from are the window's old edges. visible is the share of the window that was inside the work area, and the last pair is the new top left corner.

No screenshot for this step. The test record doesn't include a capture of the Windhawk log.

The gotcha is that the line only shows up while Log moves is on. If you turned it off, the mod still moves windows but says nothing.

### 3. Drag the window partly off the edge
Move the window wherever you like, even halfway off the screen. The mod has already checked it, so it leaves it there.

No screenshot for this step. No capture was made of it, and the rescue test didn't record a drag.

The gotcha is a program that hides its window and shows it again. The mod remembers each window it has checked, so it doesn't check that window a second time.

## The result and the settings behind it
The test in the README's Status section ran on Windows 11 build 26300 with a single monitor. A test window opened at x = -3000. It stayed there with the mod off. With the mod on, it moved to the center of the screen, and the README's screenshot notes give that spot as x = 1000, y = 630 on a 2560 by 1600 screen.

The screenshot in step 1 comes from a later run on 2026-10-16. The installed settings were minVisiblePercent 50 and logMoves on, read with mod settings get. A 760 by 440 test window opened at -3000, 300 and measured 900, 580 right after it appeared. That run didn't repeat the check with the mod off.

The record doesn't list the settings. A window at x = -3000 has none of its area on the screen, so it's below any Minimum visible share you can set, the default of 50 included.

## Which windows it checks
A window is checked only if all of these hold.

- It's a top-level window with a full title bar.
- It isn't a tool window or a window that refuses activation.
- It isn't minimized or maximized.

Menus, tooltips, overlays and fullscreen games are left alone that way.

The mod measures how much of the window's area lies inside the work area of the nearest monitor. If that share is below Minimum visible share, it shrinks the window to fit the work area if it's too big, centers it there, and moves it without changing its place in the stack or activating it.

## Settings
You'll find these in the mod's Settings tab in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Name in Windhawk | Default | Allowed values | Effect |
| --- | --- | --- | --- | --- |
| minVisiblePercent | Minimum visible share (percent) | 50 | 1 to 100. A value below 1 counts as 1 and a value above 100 counts as 100. | The mod moves a window when less than this percent of its area is inside the work area of the nearest monitor. |
| logMoves | Log moves | on | on, off | On writes a line to the Windhawk log each time a window is moved. |

The mod reads its settings again when you change them. To change one from an elevated shell, pass the key and value like this.

```
windhawk-cli.exe mod settings set local@offscreen-window-rescue "'minVisiblePercent=70'"
```

## How it works under the hood
The mod loads into every program except File Explorer and these core system processes: csrss.exe, dwm.exe, winlogon.exe, LogonUI.exe, fontdrvhost.exe, sihost.exe and ShellHost.exe.

Inside each program it hooks three Windows calls.

- ShowWindow, when a window is shown with SW_SHOW, SW_SHOWNORMAL, SW_SHOWNA, SW_SHOWNOACTIVATE, SW_RESTORE or SW_SHOWDEFAULT.
- SetWindowPos, when the call carries SWP_SHOWWINDOW.
- CreateWindowExW, when a window is created already visible and has no parent.

Each hook first lets the real call run. Then it checks whether the window counts, as Which windows it checks describes. The mod keeps a list of the windows it has already checked, and a window is measured only the first time it's seen.

When the list grows past 4096 windows, it's cleared and starts again. The mod has no unload code of its own, and windows it has moved stay where it put them.

## Limits and troubleshooting
These come from the code and the mod's own readme.

Limits.
- It checks a window only when it first appears. A window that was already off screen before you installed the mod stays where it is.
- It skips File Explorer and the core system processes listed above. You can add more @exclude lines in the Source code tab for any program you want it to skip.
- It skips windows without a full title bar, tool windows, minimized windows and maximized windows.
- It measures against one monitor, the one nearest the window. A window split across two monitors counts only the part on that monitor.
- It watches three calls. A window that becomes visible some other way isn't checked.
- Once the list of checked windows passes 4096 and is cleared, a window that's shown again can be checked a second time.

My window is still off screen. Check that the mod is enabled, then look at the Windhawk log. With Log moves on, each move is written there. The mod checks a window only when it first appears, so a window that was already off screen before you installed it stays where it is. Close that program and open it again.

The window was moved but it's still partly off screen. The mod leaves a window alone once at least Minimum visible share of it is visible. Raise that number if you want it to act sooner.

A program's window never moves. If the program is File Explorer or one of the excluded system processes, the mod doesn't load into it. A window without a full title bar, a tool window, or a maximized or minimized window is skipped too.

The mod moved a window you didn't want moved. Add an @exclude line with that program's file name in the mod's Source code tab, or lower Minimum visible share.

## What's tested and what isn't
The README's Status section records a test on Windows 11 build 26300 with a single monitor. A test window opened at x = -3000 stayed there with the mod off and moved to the center with the mod on. The screenshot in step 1 comes from a later run with the mod on.

It hasn't been tested on a real MSI installer yet. The repo records no test with more than one monitor, no test of a drag after the rescue, and no capture of the log line. Windows 10 hasn't been tried. Those follow from reading the code.

## Wrap-up
You installed the mod, opened a program whose window landed off screen, and found it moved to the center of the work area. The log line told you where it was and where it went. After that first check, the mod leaves the window alone.

## Uninstall
Open Windhawk, find the mod, and disable it or remove it. From an elevated shell:

```
windhawk-cli.exe mod disable local@offscreen-window-rescue --yes
windhawk-cli.exe mod remove local@offscreen-window-rescue --yes
```

Windows the mod has already moved stay where they are.

## Use at your own risk
This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash, or other harm it causes. The mod runs inside almost every app on your PC, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
