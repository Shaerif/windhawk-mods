# Windhawk mods for Windows 11: window management, focus, and clipboard fixes

A set of eight custom [Windhawk](https://windhawk.net) mods for Windows 11. Seven of them fix a single everyday annoyance: windows that open off screen, no way to cycle through an app's windows, focus that needs a click, tall title bars you can't fold away, apps that forget where you put them, a clipboard that freezes, and Copy as path when you only want the names of one or many files and folders. The eighth logs which Windows API call Windows Update uses when it restarts your PC. They're written in C++, they're MIT licensed, and you can install any of them on its own.

We build these because Windows 11 leaves small gaps that Mac and Linux desktops filled years ago. We'd rather patch a gap once, in a mod we can read and change, than live with it or install a large tweaking suite for one feature.

## Documentation
Each mod has its own guide in the docs folder. The [docs index](docs/README.md) lists all of them.

## Contents
- [Quick summary](#quick-summary)
- [Why we build these mods](#why-we-build-these-mods)
- [The mods](#the-mods)
- [Install](#install)
- [Screenshots](#screenshots)
- [Questions and answers](#questions-and-answers)
- [Mod reference](#mod-reference)
- [Status](#status)
- [Disclaimer](#disclaimer)
- [License](#license)

## Quick summary
| Mod | Problem it solves | Runs in | File |
| --- | --- | --- | --- |
| Off-Screen Window Rescue | A window opens outside the visible screen and you can't reach it | All apps except File Explorer and core system processes | `mods/offscreen-window-rescue.wh.cpp` |
| Same-App Window Cycler | No quick key to switch between windows of one app | explorer.exe | `mods/same-app-window-cycler.wh.cpp` |
| Focus Follows Mouse | Focus needs a click | explorer.exe | `mods/focus-follows-mouse.wh.cpp` |
| Title Bar Scroll Roll-Up | A window you want to fold out of the way | explorer.exe | `mods/titlebar-scroll-rollup.wh.cpp` |
| Window Rules | Apps open at a random spot, size, or opacity every time | All apps except File Explorer and core system processes | `mods/window-rules.wh.cpp` |
| Clipboard Hang Watchdog | Copy and paste stop working and you don't know why | explorer.exe | `mods/clipboard-hang-watchdog.wh.cpp` |
| Bulk Copy File and Folder Names | Copy as path gives full paths when you only want the names of one or many files and folders | explorer.exe | `mods/bulk-copy-file-and-folder-names.wh.cpp` |
| Restart API Logger | Windows Update restarts your PC and you can't tell which API call it used | MoUsoCoreWorker.exe and TrustedInstaller.exe | `mods/restart-api-logger.wh.cpp` |

## Why we build these mods
Windhawk lets you change how Windows behaves by injecting small pieces of code into running programs. It doesn't patch system files on disk. That makes it a good fit for narrow fixes, because a mod is one readable file and you can turn it off at any time.

We follow three rules when we write a mod:
- It does one job. If a fix needs a second feature, it becomes a second mod.
- It ships with settings and safe defaults. Anything that could end a program, like the clipboard watchdog, starts in log-only mode.
- It says what it can't do. Every section below lists its limits.

The first mod in this repo came from a real problem. A Windows installer opened its window outside the visible area and wouldn't come back. That's why the rescue mod exists, and why it checks each window once instead of fighting you every time you drag one near an edge.

## The mods

### Off-Screen Window Rescue
Off-Screen Window Rescue is a Windhawk mod for Windows 11 that moves windows back onto your screen when they open outside the visible area.

Problem: some windows open at coordinates that don't exist on your screen. Installers, old apps, and apps that remember a monitor you've unplugged do this. The window is running, but you can't see it or grab it.

What it does: when a normal window first appears, the mod checks how much of it is visible. If less than the limit you set is on screen, it moves the window back into the work area. It checks each window once, so you can still drag a window partly off the edge afterward.

Settings:
- minVisiblePercent (default 50): how much of the window must be visible before we leave it alone.
- logMoves (default on): writes each move to the Windhawk log.

Limits: it skips File Explorer and core system processes (csrss.exe, dwm.exe, winlogon.exe, LogonUI.exe, fontdrvhost.exe, sihost.exe, ShellHost.exe). You can add more @exclude lines in the Source code tab for any program you want to skip.

### Same-App Window Cycler
Same-App Window Cycler is a Windhawk mod that adds a shortcut to Windows 11 for switching between windows of the same app. You press Alt plus the backtick key, the key above Tab on a US keyboard.

Problem: Alt+Tab shows every window from every app. If you have six browser windows and a code editor, getting to the right browser window takes several presses.

What it does: press Alt plus the backtick key to move between the windows of the app you're using, like Cmd plus the backtick key on a Mac.

Settings:
- modifier (default Alt): Alt, Ctrl+Alt, or Win.
- includeMinimized (default on): minimized windows are part of the cycle.
- currentDesktopOnly (default on): only windows on the current virtual desktop.

### Focus Follows Mouse
Focus Follows Mouse is a Windhawk mod that gives keyboard focus to the window under your pointer on Windows 11, with no click.

Problem: on Windows, the window under your pointer doesn't get keyboard focus until you click it. Many Linux desktops don't work that way.

What it does: focuses the window under the mouse after a short delay. By default it doesn't raise the window, so a window you're reading stays on top while you type in the one underneath.

Settings:
- enabled (default on).
- delayMs (default 300): how long the pointer must rest on a window before focus moves.
- raiseWindow (default off): also bring the window to the front.

### Title Bar Scroll Roll-Up
Title Bar Scroll Roll-Up is a Windhawk mod that folds a Windows 11 window up to its title bar when you scroll the mouse wheel on it.

Problem: you want a window out of the way for a moment, but not minimized and not closed.

What it does: scroll the mouse wheel on a title bar and the window rolls up to its title bar. Scroll the other way and it comes back. The frame outline stays and the content is hidden.

Settings:
- enabled (default on).
- reverse (default off): swap the scroll directions.
- modifier (default none): require Ctrl, Shift, or Alt while you scroll.

### Window Rules
Window Rules is a Windhawk mod that sets position, size, monitor, always-on-top, and opacity for specific apps when their windows open.

Problem: some apps open in a different place and size every time, and Windows has no built-in way to pin them. KDE has had per-app window rules for a long time.

What it does: you write a rule for an app and the mod applies it when that app's window opens. A rule can set position, size, monitor, maximize, always-on-top, and opacity.

Rule fields:
- process: the program file name, for example notepad.exe. Required.
- title: optional, narrows the rule to windows with a matching title.
- monitor: 1 for the first monitor, 2 for the second, and so on. 0 keeps the current monitor.
- x, y, width, height: 0 leaves the value alone.
- maximize and alwaysOnTop: on or off.
- opacity: 10 to 100 percent. 0 leaves it alone.

Other settings: enabled and logRules, both on by default.

Limits: it skips File Explorer and the same core system processes as the rescue mod.

### Clipboard Hang Watchdog
Clipboard Hang Watchdog is a Windhawk mod that finds which program is blocking the Windows 11 clipboard and writes it to a log.

Problem: copy and paste stop working, sometimes for many seconds, and Windows doesn't tell you which program is responsible.

What it does: a background thread checks the clipboard every 500 ms. If a program holds the clipboard open longer than your threshold, or the clipboard owner stops responding, the mod writes the program name and process ID to a log. In endHolder mode it can also end that program once the hold passes the kill limit.

Settings:
- thresholdSeconds (default 5): how long a hold counts as a hang.
- action (default logOnly): logOnly or endHolder.
- killAfterSeconds (default 15): in endHolder mode, how long we wait before ending the program.
- maxLogKB (default 100, 0 means no limit): when the log passes this size, it's renamed to .old and a fresh log starts.
- protectedProcesses: programs the mod never ends. The default list is explorer.exe, dwm.exe, csrss.exe, winlogon.exe, svchost.exe, sihost.exe, services.exe, lsass.exe.

The log lives at %LOCALAPPDATA%\clipboard-hang-watchdog.log and uses 12-hour timestamps.

Limits: it finds a holder only if that program opened the clipboard with a window handle, or if it owns delayed-render data and has stopped responding. Windows waits up to 30 seconds for delayed rendering. A hang inside the Clipboard User Service (cbdhsvc) is a separate cause, and this mod can't see it.

### Bulk Copy File and Folder Names
Bulk Copy File and Folder Names is a Windhawk mod that copies the names of the files and folders you select in File Explorer, without their paths. It works on one item or on many at once, and a selection can mix files and folders.

Problem: Windows 11 has Copy as path (Ctrl+Shift+C), but it gives you full paths in quotes. If you only want the names, you have to trim each one by hand, and that's slow when there are dozens.

What it does: select one or more files and folders in File Explorer and press Ctrl+Shift+N. The names go to the clipboard as plain text, one per line. Press Ctrl+Alt+Shift+N to copy them without extensions. Folders keep their full name, so sub.folder stays sub.folder. a.tar.gz becomes a.tar, and a dotfile like .gitignore stays whole. The keys only work while a File Explorer window is in front, so other apps keep their own shortcuts.

You can also right-click a file, a folder, or a selection of many files and folders. The menu gets two entries, Copy name and Copy name without extension. They follow the same rules and settings as the keys. On Windows 11 they're in the classic menu, the one you get from Show more options or Shift+right-click. They don't show on empty space, on column headers, on the desktop, or in other apps.

Settings:
- withExtModifier (default Ctrl+Shift) and withExtKey (default N): the keys for names with extensions.
- noExtModifier (default Ctrl+Alt+Shift) and noExtKey (default N): the keys for names without extensions. A key can be a letter, a digit, or F1 to F24.
- separator (default newline): newline, comma, or space. A comma is followed by a space.
- quoteNames (default off): put each name in double quotes.
- contextMenu (default on): show the two right-click menu entries. The keys work either way.

Limits: Ctrl+Shift+N is also File Explorer's New folder shortcut. While the mod is on, that key copies names instead, so pick another key if you use New folder a lot. The keys and menu entries don't work on the desktop or in Open and Save dialogs. The Windows 11 compact menu can't take new entries from a mod. The menu labels are in English. Items that aren't files or folders, like drives in This PC, give their internal name. The mod doesn't change your files or the normal Copy command.

![The classic right-click menu in File Explorer with Copy name and Copy name without extension above Properties](docs/screenshots/bulk-copy-step-4-right-click-menu.png)

The full guide, with a screenshot for each step, is the [Bulk Copy File and Folder Names tutorial](docs/bulk-copy-file-and-folder-names.md).

### Restart API Logger
Restart API Logger is a Windhawk mod that records which Windows API call Windows Update uses to restart your PC, and which code made that call.

Problem: Windows Update sometimes restarts the PC on its own. The Windows event log tells you which program asked for the restart, but it doesn't tell you which API call was used.

What it does: the mod loads into the two Windows Update programs, MoUsoCoreWorker.exe and TrustedInstaller.exe. It hooks nine shutdown and restart APIs, among them ExitWindowsEx, InitiateShutdownW and NtShutdownSystem. For each call it writes a CALL line with the API name, the reason code and flags in hex, and the module that made the call. It writes that line before it passes the call on, and it writes a RETURN line if the program is still alive when the call comes back. Version 1 only logs. It doesn't block, delay or change any call.

Settings:
- logLevel (default detailed): basic or detailed. Detailed adds the full program path, the process and thread IDs, every argument, the return address, and the caller module with its offset.
- fileLog (default on): also appends each line to %ProgramData%\restart-api-logger.log, because a restart can end the program before the Windhawk log is saved.

Limits: it only sees calls from those two programs, and only the nine APIs it hooks. Windhawk protects some system processes, and it isn't verified that it can load into these two. The log file is capped at 256 KB, and the oldest lines are dropped past that.

The full guide is the [Restart API Logger doc](docs/restart-api-logger.md).

## Install
You need [Windhawk](https://windhawk.net) installed first. Then pick one way.

From the Windhawk app:
1. Open Windhawk and choose Create a new mod.
2. Paste the contents of the .wh.cpp file you want.
3. Compile and enable the mod.

From an elevated shell (run as administrator):

```
windhawk-cli.exe mod install --file mods/offscreen-window-rescue.wh.cpp --yes
```

Swap in any file from the mods folder. To change a setting from the shell:

```
windhawk-cli.exe mod settings set local@clipboard-hang-watchdog "'action=endHolder'"
```

## Screenshots
These are plain test windows on a blank backdrop.

Title bar roll-up. Scroll up on the title bar and the window rolls up. The frame outline stays but the content is hidden. Scroll down and it comes back.

![A test window before title bar roll-up](images/rollup-before.png)
![The same test window after rolling up](images/rollup-after.png)

Off-screen rescue. We opened a test window at x = -3000. The mod moved it to x = 1000, y = 630 on a 2560 by 1600 screen.

![A test window rescued after opening off screen](images/rescue-after.png)

Window rules. One rule matches a test window by app and title. The window opened at 60, 60 and was 380 by 220. The rule moved it to 500, 300, made it 560 by 340 and set opacity to 80.

![A test window placed and made translucent by a window rule](images/rules-after.png)

The window cycler, focus follows mouse, clipboard watchdog and restart logger have no screenshots because a still image can't show them.

## Questions and answers

### How do I fix a window that opened off screen in Windows 11?
Install Off-Screen Window Rescue in Windhawk. When a window first opens with less than 50 percent of it visible, the mod moves it back into the work area. You can change that limit with the minVisiblePercent setting. The mod checks each window once, when it appears.

### How do I switch between windows of the same app in Windows 11?
Install Same-App Window Cycler and press Alt plus the backtick key, the key above Tab on a US keyboard. It cycles through the windows of the app you're using, and it includes minimized windows by default. It works like Cmd plus the backtick key on a Mac.

### How do I get focus follows mouse in Windows 11?
Install Focus Follows Mouse. After the pointer rests on a window for 300 ms, that window gets keyboard focus. The mod doesn't bring the window to the front unless you turn on raiseWindow.

### How do I find which program is blocking the clipboard?
Install Clipboard Hang Watchdog. It checks the clipboard every 500 ms, and when a program holds it open longer than 5 seconds, the mod logs that program to %LOCALAPPDATA%\clipboard-hang-watchdog.log. It starts in log-only mode, so it doesn't end anything until you ask it to.

### How do I set a fixed size and position for an app in Windows 11?
Install Window Rules and add a rule with the program file name, for example notepad.exe. Then set x, y, width, and height. The rule applies each time that app opens a window.

### How do I roll a window up to its title bar in Windows 11?
Install Title Bar Scroll Roll-Up and scroll the mouse wheel on a window's title bar. The window rolls up to its title bar. Scroll the other way and it comes back.

### How do I copy only the file and folder names in File Explorer?
Install Bulk Copy File and Folder Names, select the files and folders, and press Ctrl+Shift+N. One item or many, it's the same key. You get the names with their extensions, one per line, ready to paste. Ctrl+Alt+Shift+N copies them without extensions. You can also right-click the selection and pick Copy name.

### Is it safe to run these mods?
Each mod is one source file you can read before you install it. Windhawk compiles it on your machine. The clipboard watchdog starts in log-only mode and never ends a program on the protected list.

### Do these mods work on Windows 10?
We only test on Windows 11. We haven't tried Windows 10, so we can't say.

### How do I remove a mod?
Open Windhawk, find the mod, and disable or delete it. From a shell, run one of these with the mod's ID:

```
windhawk-cli.exe mod disable <id> --yes
windhawk-cli.exe mod remove <id> --yes
```

### Can I use more than one mod?
Yes. They don't depend on each other.

### How do I stop a mod from touching one program?
Add an @exclude line with the program file name in the mod's Source code tab.

### My window is still off screen. What now?
Check that Off-Screen Window Rescue is enabled, then look at the Windhawk log. With logMoves on, each move is written there. The mod checks a window only when it first appears, so a window that was already off screen before you installed the mod stays where it is.

### My clipboard still hangs and the watchdog logs nothing.
That points at the Clipboard User Service (cbdhsvc) or a holder the mod can't see. This mod doesn't cover either case.

## Mod reference

- Repo contents: a collection of standalone Windhawk mods for Windows 11, written in C++.
- Language and format: each file in the mods folder ends in .wh.cpp and follows Windhawk's mod format, with a WindhawkMod metadata block at the top and a WindhawkModSettings block for settings.
- Install target: Windhawk on Windows 11. A mod isn't a standalone program.
- Install command: run this as administrator, with the mod's file name in place of name.

```
windhawk-cli.exe mod install --file mods/<name>.wh.cpp --yes
```

- Mod IDs: a locally installed mod gets the ID local@ followed by its file name without .wh.cpp, for example local@clipboard-hang-watchdog.
- Setting values: pass them in this form. Option settings use string keys.

```
windhawk-cli.exe mod settings set <id> "'key=value'"
```

- Build time: Windhawk compiles a mod in about 45 to 60 seconds on our machine.
- License: MIT. You can copy, change, and redistribute any mod.

| Mod ID | Version | Scope | Settings keys |
| --- | --- | --- | --- |
| offscreen-window-rescue | 0.2.0 | all apps except listed excludes | minVisiblePercent, logMoves |
| same-app-window-cycler | 0.1.0 | explorer.exe | modifier, includeMinimized, currentDesktopOnly |
| focus-follows-mouse | 0.1.0 | explorer.exe | enabled, delayMs, raiseWindow |
| titlebar-scroll-rollup | 0.1.0 | explorer.exe | enabled, reverse, modifier |
| window-rules | 0.1.1 | all apps except listed excludes | enabled, logRules, rules |
| clipboard-hang-watchdog | 0.1.0 | explorer.exe | thresholdSeconds, action, killAfterSeconds, maxLogKB, protectedProcesses |
| bulk-copy-file-and-folder-names | 0.3.0 | explorer.exe | withExtModifier, withExtKey, noExtModifier, noExtKey, separator, quoteNames, contextMenu |
| restart-api-logger | 0.1.0 | MoUsoCoreWorker.exe, TrustedInstaller.exe | logLevel, fileLog |

## Status
Tested on Windows 11 build 26300 with a single monitor. We checked the roll-up, the rescue mod, and window rules on plain test windows (see the screenshots). For the rescue mod, a window opened at x=-3000 stayed there with the mod off and moved to the center with the mod on. It hasn't been tested on a real MSI installer yet. The clipboard watchdog was tested for hold detection and for ending a holder, and the log trim, which moves an over-limit log to a .old file, passed a test with an 8-run overflow. Bulk copy file and folder names has one recorded live run on Windows 11. Its hotkeys and right-click entries were tested on a selection of 7 items, 2 folders and 5 files. Zip files, selections of more than 15 files, and other Windows builds haven't been tested. We have no recorded live test for the window cycler, focus follows mouse, or the restart API logger.

## Disclaimer
These mods come with no warranty of any kind, and you use them at your own risk. The author isn't liable for any damage, data loss, crash, or other harm they cause. A mod runs inside Windows apps, so test it on your own machine before you rely on it.

## License
MIT. The full text is in the [LICENSE](LICENSE) file. After the MIT terms, the file adds a Use at your own risk notice that says the mods come with no warranty or guarantee of any kind.
