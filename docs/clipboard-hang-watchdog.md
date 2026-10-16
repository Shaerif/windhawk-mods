# Clipboard Hang Watchdog tutorial

This guide sets up the Clipboard Hang Watchdog mod for Windhawk and shows you how to read what it finds. When you're done, the next time copy and paste stop working you can open one log file and see which program was to blame. It starts in log-only mode, so it won't end anything until you tell it to. The screenshot shows the real log file from our test PC on Windows 11 build 26300. No hang was staged for this guide. The HELD OPEN lines in it come from an earlier hold test.

## Contents
- [What it fixes](#what-it-fixes)
- [Before you start](#before-you-start)
- [Install](#install)
- [Use it in four steps](#use-it-in-four-steps)
- [The result and the settings behind it](#the-result-and-the-settings-behind-it)
- [Every message the mod writes](#every-message-the-mod-writes)
- [Settings](#settings)
- [How it works under the hood](#how-it-works-under-the-hood)
- [Limits and troubleshooting](#limits-and-troubleshooting)
- [What's tested and what isn't](#whats-tested-and-what-isnt)
- [Wrap-up](#wrap-up)
- [Uninstall](#uninstall)
- [Use at your own risk](#use-at-your-own-risk)

## What it fixes
Windows lets only one program hold the clipboard open at a time. If a program opens it and never closes it, every other app is locked out. You press Ctrl+C and nothing is copied, or you press Ctrl+V and the app freezes. Sometimes that lasts for many seconds.

There's a second way it goes wrong. The program that owns the copied data can stop responding. Other programs then wait for it when they paste, and the mod's own notes say they wait up to 30 seconds. In both cases Windows doesn't tell you which program is responsible.

This mod watches for both cases and writes the program's name, process ID and window title to a log.

## Before you start
You need three things.

- Windows 11. The repo's mods are only tested on Windows 11, and nobody has tried this one on Windows 10.
- Windhawk, from [windhawk.net](https://windhawk.net).
- The file mods/clipboard-hang-watchdog.wh.cpp from this repo.

Any plain text editor will open the log file.

## Install
The repo describes two ways to add the mod as a local mod.

From the Windhawk app:
1. Open Windhawk and choose Create a new mod.
2. Paste the whole contents of clipboard-hang-watchdog.wh.cpp.
3. Compile and enable the mod.

From a shell you've opened as administrator:

```
windhawk-cli.exe mod install --file mods/clipboard-hang-watchdog.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine.

To check that it loaded, list your mods and look for local@clipboard-hang-watchdog, which is the id a local install gets.

```
windhawk-cli.exe mod list
```

The mod runs inside explorer.exe. When it loads, it writes one line to the Windhawk log and to its own log file.

```
Watchdog started (report after 5s, action 1)
```

The first number is your threshold in seconds. Action 1 means log only and action 2 means log and end the holder.

## Use it in four steps
The number matches the red badge in the corner of the screenshot.

### 1. Open the log file
Open this file in a text editor. %LOCALAPPDATA% stands for your local app data folder.

```
%LOCALAPPDATA%\clipboard-hang-watchdog.log
```

![The watchdog log file in a plain text window, with four HELD OPEN lines, each one followed by a Clipboard released line, and then six Watchdog started lines](screenshots/clipboard-hang-watchdog-step-1-log-file.png)

These are the first 14 lines of the real file. Four HELD OPEN lines each name powershell.exe, a process ID and the window ClipHoldTest, and six Watchdog started lines show action 1.

Each line starts with the date and a 12-hour time, then two spaces and the message text. The same message text goes to the Windhawk log too.

The gotcha is an empty or missing file. The mod writes its first line as soon as it loads, so a missing file usually means it isn't running. If Windows can't give the mod the LOCALAPPDATA path, it writes to the Windhawk log only.

### 2. Read the HELD OPEN or OWNER NOT RESPONDING line
When copy and paste stop working, give it a few seconds. With the default settings a hold is logged once it has lasted 5 seconds. Then look for the newest line whose message text starts with HELD OPEN or OWNER NOT RESPONDING. It names the program, its process ID and the title of its window.

No screenshot for this step. No hang was staged for this guide. Step 1 already shows four real HELD OPEN lines.

The gotcha is a hang with no line at all. Windows reports a holder only when it opened the clipboard with a window handle. A program that opened it with no window isn't logged, and the mod can't name it.

### 3. Wait for the released line
Once the hang clears, a later line says Clipboard released after, with the full length of the hold in seconds. For a program that stopped responding, the later line is Clipboard owner responding again.

No screenshot for this step. Step 1 shows four Clipboard released lines from the hold test, at 7 and 8 seconds.

The gotcha is the number. HELD OPEN gives the time when the mod reported the hold, and Clipboard released gives the full length. In step 1 the holds were reported at 5 seconds and lasted 7 or 8.

### 4. Let it end the holder, if you accept the risk
Switch Action to Log and end the holding program. From an elevated shell, that's this command.

```
windhawk-cli.exe mod settings set local@clipboard-hang-watchdog "'action=endHolder'"
```

After that, when a program keeps the clipboard open for 15 seconds, which is the default for Seconds before the holding program is ended, the mod ends it and logs an Ended line. It won't end anything on the protected list.

No screenshot for this step. The README records a test of ending a holder, but no image of it was kept.

The gotcha is lost work. When the mod ends a program, that program's unsaved work is gone. That's why this is off by default.

## The result and the settings behind it
The six Watchdog started lines in step 1 show the mod loading with the defaults, report after 5s and action 1. The HELD OPEN lines were reported at 5 seconds, which matches the default threshold. The four HELD OPEN lines come from the hold test that the README's Status section describes. In that test a window called ClipHoldTest, from powershell.exe, held the clipboard open on purpose.

| Line | What it shows |
| --- | --- |
| HELD OPEN 5s by powershell.exe (pid 12208) window "ClipHoldTest" | The hold passed the 5 second threshold |
| Clipboard released after 8s | That hold ended after 8 seconds in all |
| Watchdog started (report after 5s, action 1) | The mod loaded with a 5 second threshold, in log-only mode |

The other three holds look the same with other process IDs, and one of them lasted 7 seconds.

## Every message the mod writes

| Message text starts with | What it means |
| --- | --- |
| Watchdog started | The mod loaded. It shows the threshold and the action number. |
| HELD OPEN | A window kept the clipboard open past the threshold. You get the seconds, program name, process ID and window title. |
| Clipboard released after | The hold that was logged has ended. You get its total length in seconds. |
| OWNER NOT RESPONDING for | The program that owns the clipboard data has stopped responding for longer than the threshold. You get the seconds, program name, process ID and window title. |
| Clipboard owner responding again | After a reported owner, a later check found no owner that isn't responding. The owner may have recovered, or the clipboard may have a new owner or none, for example after a new copy. |
| Ended | In endHolder mode the mod ended the holding program. The line ends with done or failed. |
| Not ending | The mod left the holder alone, and the line says protected program. That happens when the holder is on the protected list, has process ID 0, or is the explorer.exe the mod runs in. |
| Could not open | Windows didn't let the mod open the holder to end it. The line has the Windows error number. |

## Settings
You'll find these in the mod's Settings tab in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Name in Windhawk | Default | Allowed values | Effect |
| --- | --- | --- | --- | --- |
| thresholdSeconds | Seconds before a hold is reported | 5 | Whole seconds, 1 or more. A value under 1 counts as 1. | How long a hold, or an owner that isn't responding, lasts before it's logged. |
| action | Action | logOnly | logOnly (Log only) or endHolder (Log and end the holding program). Any other value counts as logOnly. | logOnly writes to the log. endHolder writes to the log and ends the program that holds the clipboard open. |
| killAfterSeconds | Seconds before the holding program is ended | 15 | Whole seconds. A value lower than thresholdSeconds is raised to match it. | In endHolder mode, how long a hold lasts before the mod ends the program. It has no effect in logOnly mode. |
| maxLogKB | Log size limit in KB (0 = no limit) | 100 | Whole kilobytes. 0 or less means no limit. | When the log file is bigger than this, the mod renames it to clipboard-hang-watchdog.log.old before its next write and starts a new log. An older .old file is replaced. |
| protectedProcesses | Protected programs (comma separated) | explorer.exe,dwm.exe,csrss.exe,winlogon.exe,svchost.exe,sihost.exe,services.exe,lsass.exe | Program file names, separated by commas. Case and spaces around a name don't matter. | Programs the mod never ends. |

The mod reads its settings again whenever you change them, so you don't need to restart anything. A settings change doesn't write a new Watchdog started line. That line only comes when the mod loads.

## How it works under the hood
The mod loads only into explorer.exe. It starts one background thread, and that thread checks the clipboard every 500 ms.

Each check asks Windows two things. The first is which window has the clipboard open, through GetOpenClipboardWindow. The second is which window owns the clipboard data, through GetClipboardOwner, and whether that window has stopped responding, through IsHungAppWindow. The timer for a hold starts the first time the mod sees that window. If a different window takes over, the timer starts again.

To name a program, the mod reads the full path of its process and keeps the file name. If it can't read the path, the name shows as a question mark.

Only the held-open case can lead to ending a program. The mod ends it with TerminateProcess. It never ends process ID 0, the explorer.exe it runs in, or anything on the protected list. An owner that stops responding is only logged, in either mode.

Every line goes to the Windhawk log and is appended to the log file as UTF-8. When the mod unloads, it tells the thread to stop and waits up to 3 seconds for it. It doesn't delete the log file.

## Limits and troubleshooting
These come from the code and the mod's own readme.

Limits.
- Windows reports a holder only when it opened the clipboard with a window handle. A program that opened it with no window isn't logged at all, and the mod can't name it.
- An owner of copied data is caught only when Windows marks its window as not responding.
- If the Windows Clipboard User Service (cbdhsvc) is stuck, the mod can't see it.
- The mod checks every 500 ms and counts in whole seconds. A hold shorter than one check can slip past it.
- A program the mod ends loses its unsaved work.
- The log keeps one backup. Each time the log passes the size limit, the old backup is replaced.

Your clipboard hangs and the log shows nothing. That points at the Clipboard User Service or at a holder that opened the clipboard with no window. The mod covers neither case. The mod's own notes say signing out and back in resets that service, and that the signs of a stuck service are a frozen Win+V history and new copies that don't appear.

There's no log file. Check that the mod is enabled. The file path comes from the LOCALAPPDATA variable, and if Windows can't give the mod that path, it writes to the Windhawk log only.

The mod didn't end the program. Check that Action is endHolder and that the hold lasted at least Seconds before the holding program is ended. Check the protected list too. A Not ending line means the program is on it, has process ID 0, or is the explorer.exe the mod runs in. An owner that stops responding is never ended.

The log says Could not open or Ended with failed. Windows refused the request to end that program. The Could not open line carries the error number Windows gave.

The program name is a question mark. The mod couldn't read that program's file path. The process ID in the same line still points to it.

## What's tested and what isn't
The README's Status section records these tests on Windows 11 build 26300 with a single monitor. The clipboard watchdog was tested for hold detection and for ending a holder. The log trim, which moves an over-limit log to a .old file, passed a test with an 8-run overflow. The HELD OPEN lines in step 1 come from the hold test.

The repo records no separate test of the check for an owner that stops responding, and no image of the endHolder test. Windows 10 hasn't been tried.

## Wrap-up
You installed the mod, opened its log file, and read the lines a real hold left behind. A HELD OPEN line names the program, and the released line after it tells you how long the hold lasted. The mod won't end a holder until you switch Action yourself.

## Uninstall
Open Windhawk, find the mod, and disable it or remove it. From an elevated shell:

```
windhawk-cli.exe mod disable local@clipboard-hang-watchdog --yes
windhawk-cli.exe mod remove local@clipboard-hang-watchdog --yes
```

The mod doesn't delete its log file or the .old backup. Both stay in your local app data folder.

## Use at your own risk
This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash, or other harm it causes. The endHolder setting ends programs, and their unsaved work is lost. The full terms are in the [LICENSE](../LICENSE) file.
