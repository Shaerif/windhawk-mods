# Off-Screen Window Rescue

Off-Screen Window Rescue is a Windhawk mod for Windows 11 that moves windows back onto your screen when they open outside the visible area. It's for you if an installer, an old app, or an app that remembers a monitor you've since unplugged keeps opening where you can't reach it. The mod checks each window once, when it first appears, so it won't fight you when you drag a window near an edge later.

## The problem

Some windows open at coordinates that don't exist on your screen. The window is running and you can see it in the taskbar, but you can't see it on screen or grab it with the mouse. Installers do this often. The first mod in this repo came from that exact problem: a Windows installer opened its window outside the visible area and wouldn't come back.

## Install

You need [Windhawk](https://windhawk.net) installed first. Then pick one way.

From the Windhawk app:

1. Open Windhawk and choose Create a new mod.
2. Paste the contents of mods/offscreen-window-rescue.wh.cpp.
3. Compile and enable the mod.

From an elevated shell (run as administrator):

```
windhawk-cli.exe mod install --file mods/offscreen-window-rescue.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine. A locally installed copy gets the ID local@offscreen-window-rescue.

### Check that it's loaded

The mod doesn't write a line when it loads. It only logs when it moves a window, and only while logMoves is on. To see that it's installed and enabled, list your mods from an elevated shell and look for local@offscreen-window-rescue:

```
windhawk-cli.exe mod list
```

## How you use it

The mod runs in the background inside your apps. There's nothing to click.

1. Install and enable the mod. Leave logMoves on.
2. Open the program whose window keeps landing off screen.
3. If less than the minimum visible percent of its window (default 50) is inside the work area of the nearest monitor, the mod moves it to the center of that monitor. The work area is the part of the screen the taskbar doesn't cover.
4. Look in the Windhawk log. Each move gets one line. It starts with the name of the Windows call that showed the window, in square brackets, then the word moved and the window handle. After that come the old left, top, right and bottom edges, the visible share as a percent, and the new top-left corner.
5. Drag the window partly off the edge if you want to. The mod has already checked it, so it leaves it there.

## Settings

| Key | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| minVisiblePercent | 50 | 1 to 100. A value below 1 counts as 1 and a value above 100 counts as 100. | The mod moves a window when less than this percent of its area is inside the work area of the nearest monitor. |
| logMoves | on | On or off. | On writes a line to the Windhawk log each time a window is moved. |

The mod reads its settings again when you change them. To change one from the shell, pass the key and value like this:

```
windhawk-cli.exe mod settings set local@offscreen-window-rescue "'minVisiblePercent=70'"
```

## How it works under the hood

The mod loads into every program except File Explorer and these core system processes: csrss.exe, dwm.exe, winlogon.exe, LogonUI.exe, fontdrvhost.exe, sihost.exe and ShellHost.exe.

Inside each program it hooks three Windows calls:

- ShowWindow, when a window is shown, restored, or shown with its default state
- SetWindowPos, when the call carries the flag that shows the window
- CreateWindowExW, when a window is created already visible and has no parent

Each hook first lets the real call run. Then it checks whether the window counts. A window counts if it's a top-level window with a full title bar, it isn't a tool window or a window that refuses activation, and it isn't minimized or maximized. Menus, tooltips, overlays and fullscreen games are left alone that way.

The mod keeps a list of the windows it has already checked. A window is measured only the first time it's seen. The mod works out how much of the window's area lies inside the work area of the nearest monitor. If that share is below minVisiblePercent, it shrinks the window to fit the work area if it's too big, centers it there, and moves it without changing its place in the stack or activating it.

When the list grows past 4096 windows, it's cleared and starts again. The mod has no unload code of its own, and windows it has moved stay where it put them.

## Limits and known gaps

- It checks a window only when it first appears. A window that was already off screen before you installed the mod stays where it is.
- It skips File Explorer and the core system processes listed above. You can add more @exclude lines in the Source code tab for any program you want it to skip.
- It skips windows without a full title bar, tool windows, minimized windows and maximized windows.
- It measures against one monitor, the one nearest the window. A window split across two monitors counts only the part on that monitor.
- It watches three calls. A window that becomes visible some other way isn't checked.
- Once the list of checked windows passes 4096 and is cleared, a window that's shown again can be checked a second time.

## What's tested and what isn't

The README's Status section records a test of this mod on Windows 11 build 26300 with a single monitor. A test window opened at x = -3000 stayed there with the mod off. With the mod on, it moved to the center of the screen, which the README's screenshot gives as x = 1000, y = 630 on a 2560 by 1600 screen.

It hasn't been tested on a real MSI installer yet. The repo records no test with more than one monitor, and Windows 10 hasn't been tried.

## Troubleshooting

My window is still off screen. Check that the mod is enabled, then look at the Windhawk log. With logMoves on, each move is written there. The mod checks a window only when it first appears, so a window that was already off screen before you installed it stays where it is. Close that program and open it again.

The window was moved but it's still partly off screen. The mod leaves a window alone once at least minVisiblePercent of it is visible. Raise that number if you want it to act sooner.

A program's window never moves. If the program is File Explorer or one of the excluded system processes, the mod doesn't load into it. A window without a full title bar, a tool window, or a maximized or minimized window is skipped too.

The mod moved a window I didn't want moved. Add an @exclude line with that program's file name in the mod's Source code tab, or lower minVisiblePercent.

## Uninstall

Open Windhawk, find the mod, and disable or delete it. From an elevated shell you can run one of these:

```
windhawk-cli.exe mod disable local@offscreen-window-rescue --yes
windhawk-cli.exe mod remove local@offscreen-window-rescue --yes
```

Windows the mod has already moved stay where they are.

## Use at your own risk

This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash, or other harm it causes. The mod runs inside almost every app on your PC, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
