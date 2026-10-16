# Restart API Logger tutorial

This guide sets up the Restart API Logger mod for Windhawk and shows you how to read its log before and after a Windows Update restart. When you're done, the next time Windows Update restarts your PC on its own, one log file tells you which Windows API call it used and which code made that call. It doesn't stop the restart. Version 1 only logs. It blocks nothing, delays nothing and changes nothing. The screenshot shows the real log file from our test PC on Windows 11 build 26300. That file has no restart in it yet, so the steps after the first one have no screenshot.

## Contents
- [What it fixes](#what-it-fixes)
- [Before you start](#before-you-start)
- [Install](#install)
- [Use it in four steps](#use-it-in-four-steps)
- [The result and the settings behind it](#the-result-and-the-settings-behind-it)
- [Every message the mod writes](#every-message-the-mod-writes)
- [Basic and detailed lines](#basic-and-detailed-lines)
- [Settings](#settings)
- [How it works under the hood](#how-it-works-under-the-hood)
- [Limits and troubleshooting](#limits-and-troubleshooting)
- [What's tested and what isn't](#whats-tested-and-what-isnt)
- [Wrap-up](#wrap-up)
- [Uninstall](#uninstall)
- [Use at your own risk](#use-at-your-own-risk)

## What it fixes
Windows Update sometimes restarts the PC on its own. The Windows event log tells you which program asked for the restart, but it doesn't tell you which API call was used. Windows has several APIs that shut down or restart the PC, and the event log doesn't say which one ran.

This mod sits inside the two Windows Update programs, MoUsoCoreWorker.exe and TrustedInstaller.exe, and writes down every shutdown or restart call they make through the nine APIs it hooks.

## Before you start
You need three things.

- Windows 11. The repo's mods are only tested on Windows 11, and nobody has tried this one on Windows 10.
- Windhawk, from [windhawk.net](https://windhawk.net).
- The file mods/restart-api-logger.wh.cpp from this repo.

The mod can only log a restart when Windows Update does one. On our test PC, two days of INIT lines went by with no restart logged.

## Install
The repo describes two ways to add the mod as a local mod.

From the Windhawk app:
1. Open Windhawk and choose Create a new mod.
2. Paste the whole contents of restart-api-logger.wh.cpp.
3. Compile and enable the mod.

From a shell you've opened as administrator:

```
windhawk-cli.exe mod install --file mods/restart-api-logger.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine.

To check that it's installed, list your mods and look for local@restart-api-logger, which is the id a local install gets.

```
windhawk-cli.exe mod list
```

Leave both settings at their defaults. Step 1 shows you how to see that Windhawk loaded the mod into the two programs.

## Use it in four steps
The number matches the red badge in the corner of the screenshot.

### 1. Open the log file and find the INIT line
Open this file in a text editor. %ProgramData% stands for the ProgramData folder, which is C:\ProgramData on our test PC.

```
%ProgramData%\restart-api-logger.log
```

When the mod loads into one of the two programs, it writes one line whose message starts with INIT. In the file, INIT comes right after the date and time. The same line also goes to the Windhawk log.

![The restart logger log file in a plain text window with two INIT lines, both from MoUsoCoreWorker.exe, each listing nine hooked APIs, skipped=[none] and the log file path](screenshots/restart-api-logger-step-1-log-file.png)

These were the last two lines of the real file when the screenshot was taken. Both are INIT lines from MoUsoCoreWorker.exe, and each lists nine hooked APIs with the DLL they came from, skipped=[none] and the log file path.

An INIT line has the program's path, its process ID, the APIs it hooked and the DLL each one came from, the APIs it skipped and why, and where the log file goes.

The gotcha is a missing INIT line. If you never see one for TrustedInstaller.exe or MoUsoCoreWorker.exe, Windhawk didn't load the mod into that program. Windhawk protects some system processes. On our test PC it did load the mod into both.

### 2. Leave the mod on until Windows Update restarts your PC
Keep the mod enabled and use your PC as usual. Nothing shows on screen when it's working.

No screenshot for this step. There's nothing to see while the mod waits.

The gotcha is the number of INIT lines. Each time Windhawk loads the mod into a new copy of one of the two programs, it writes a new INIT line. The two lines in step 1 have different process IDs, 64700 and 74396, and the file on our test PC held 64 INIT lines by the evening of October 16, 2026.

### 3. Read the CALL line after the restart
Once the PC is back up, open the log file again. A restart can end the program before the Windhawk log is saved, so the file is the place to look. Find the CALL lines. Each one names the API that was called, the program that called it, the reason code and flags in hex, and the module that made the call. In detailed mode, the code builds the line in this shape, with the parts in angle brackets filled in.

```
CALL <API> proc=<program path> pid=<id> tid=<id> reason=0x<hex> (<reason parts>) flags=0x<hex> args={<arguments>} ret_addr=<address> caller=<module>+0x<offset>
```

In basic mode the line keeps the API, proc with the file name only, reason, flags and caller=<module>, and leaves out the rest.

No screenshot for this step. The log on our test PC has no CALL line yet, because no restart has been logged.

The gotcha is the setting for the file. If Also write a log file in ProgramData is off, the CALL line only went to the Windhawk log, and a restart can cut that off.

### 4. Look for a RETURN line
Check the line right after each CALL line. A RETURN line with the result is there only if the program was still alive when the call came back. In detailed mode it reads RETURN, the API, pid=, tid=, result=0x and a short note like (TRUE) or (NTSTATUS). In basic mode it's only RETURN, the API and result=0x.

No screenshot for this step, for the same reason as step 3.

The gotcha is reading a missing RETURN line as a failure. A restart can end the program before the RETURN line is written. The CALL line is already on disk by then, because the mod writes it and pushes it to disk before it passes the call on.

## The result and the settings behind it
When the screenshot was taken, the file on our test PC held 60 lines from October 15 and 16, 2026. All 60 were INIT lines. 53 came from MoUsoCoreWorker.exe and 7 from TrustedInstaller.exe. A later read the same evening found 64 lines, still all INIT, 56 from MoUsoCoreWorker.exe and 8 from TrustedInstaller.exe. Every one listed all nine APIs as hooked and said skipped=[none]. There was no CALL line, so the file hasn't caught a restart.

The INIT lines show that Also write a log file in ProgramData was on, because each ends in fileLog=C:\ProgramData\restart-api-logger.log. They can't show the Log detail setting. An INIT line looks the same in basic and detailed mode.

## Every message the mod writes

| Message starts with | What it means |
| --- | --- |
| INIT | The mod loaded into a program. It lists the hooked APIs, the skipped APIs with a reason, and the log file path, or off when the file log is off. |
| CALL | A hooked API was called. The mod writes this line before it passes the call on. |
| RETURN | The real API came back. The line has the result. |
| UNINIT | The mod is unloading from a program. |
| Can't open the log file | The mod couldn't open its file. This line goes to the Windhawk log only. |

Each line in the file starts with the date and a 12-hour time, for example 2026-10-16 3:07:42 PM, then two spaces and the message.

## Basic and detailed lines

| Detail | Basic | Detailed |
| --- | --- | --- |
| API name | yes | yes |
| Program | file name only | full path |
| Process and thread IDs | no | yes |
| Reason code, in hex, with the major and minor parts and the named reason flags | yes | yes |
| Flags, in hex | yes | yes |
| Every argument, with flag names decoded and text arguments read out | no | yes |
| Return address | no | yes |
| Caller module | name only, left out when no module holds the return address | name and offset, or (unknown module) |
| Result | yes | yes, with process and thread IDs and a short note |

For an API that has no reason code or no flags, the line says n/a in that spot. The INIT line is the same in both modes and always has the full program path.

## Settings
You'll find these in the mod's Settings tab in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Name in Windhawk | Default | Allowed values | Effect |
| --- | --- | --- | --- | --- |
| logLevel | Log detail | detailed | basic (Basic (process, API, reason, flags)) or detailed (Detailed (everything)). Any value other than basic counts as detailed. | basic logs the program, API, reason, flags and caller module. detailed logs everything in the table above. |
| fileLog | Also write a log file in ProgramData | on | on, off | On also appends each line to %ProgramData%\restart-api-logger.log. Off writes to the Windhawk log only. |

The mod reads its settings again when you change them, and the next line it writes uses the new values. The hooks themselves are set once, when the mod loads. To change a setting from an elevated shell, pass the key and value like this.

```
windhawk-cli.exe mod settings set local@restart-api-logger "'logLevel=basic'"
```

## How it works under the hood
The mod loads only into MoUsoCoreWorker.exe and TrustedInstaller.exe. When it loads, it looks for each of these APIs in the DLLs listed, in that order. If a DLL isn't loaded yet, the mod loads it.

| API | Looked for in |
| --- | --- |
| ExitWindowsEx | user32.dll |
| InitiateSystemShutdownExW | advapi32.dll, sechost.dll, kernelbase.dll |
| InitiateSystemShutdownExA | advapi32.dll, sechost.dll, kernelbase.dll |
| InitiateSystemShutdownW | advapi32.dll, sechost.dll, kernelbase.dll |
| InitiateShutdownW | advapi32.dll, sechost.dll, kernelbase.dll |
| NtShutdownSystem | ntdll.dll |
| NtSetSystemPowerState | ntdll.dll |
| NtInitiatePowerAction | ntdll.dll |
| SetSuspendState | powrprof.dll |

In the two INIT lines in step 1, every API was found in the first DLL on its list.

An API that isn't found is skipped with the reason not found. If two names point to the same code, the mod hooks the first one and skips the other with the reason same address as another hooked API. If Windhawk can't set a hook, the reason is hook failed.

Every hook works the same way. It builds the CALL line, writes it to the Windhawk log, appends it to the file, pushes the file to disk and closes it. Then it calls the real function with the same arguments. When that returns, it logs a RETURN line and hands back the real result unchanged. For the APIs that report errors through the Windows last-error value, it puts that value back as it was, too.

Text arguments, such as the machine name and the message, are read with a safe memory read. A missing pointer shows as (null) and a bad one shows as (unreadable). The caller module is the module that holds the return address. If no module holds it, a detailed line says caller=(unknown module), and a basic line leaves the caller field out.

The file is capped at 256 KB. When it's bigger than that, the mod keeps about the newest 192 KB before it writes the next line. It cuts at a line break, so the file doesn't start in the middle of a line. When the mod unloads, it writes the UNINIT line. It doesn't delete the file.

## Limits and troubleshooting
These come from the code and the mod's own readme.

Limits.
- Version 1 only logs. It doesn't block, delay or change any call.
- It loads into two programs only. A restart that another program asks for isn't logged.
- It only sees the nine APIs listed above. A restart that goes another way isn't logged.
- Windhawk protects some system processes. On our test PC it loaded the mod into both programs, but no other PC has been checked.
- A restart can end the program before the RETURN line is written. The CALL line is already on disk by then.
- When the file passes 256 KB, the oldest lines are dropped.

There's no INIT line for either program. Windhawk didn't load the mod into it. The mod can't log anything in a program it isn't loaded in.

The INIT line lists APIs under skipped. Square brackets wrap the whole skipped list, and the reason sits in round brackets right after each name, like NtShutdownSystem(not found). Not found means the mod found no export by that name in the DLLs listed for it above, after it loaded any of them that weren't loaded yet. The mod still logs the APIs it did hook.

The log says Can't open the log file. The mod logs to the Windhawk log only. It tries the file again on every line but reports the failure only once, until you change a setting.

The INIT line says fileLog=no path. The mod couldn't build a path from the ProgramData variable, so it has no file to write.

The PC restarted but there's no CALL line. Check for an INIT line from before the restart first. If there is one, the restart didn't go through a hooked API in that program.

## What's tested and what isn't
The README's Status section records what the log file on our test PC shows. On Windows 11 build 26300, Windhawk loaded the mod into MoUsoCoreWorker.exe and TrustedInstaller.exe again and again on October 15 and 16, 2026, and every INIT line hooked all nine APIs. The file has no CALL line, so the repo doesn't record a restart that was logged with it. Steps 2 to 4, the CALL and RETURN lines, the basic level and the file trim all follow from reading the code. The repo says the mods are tested only on Windows 11, and Windows 10 hasn't been tried.

## Wrap-up
You installed the mod and found its INIT lines in the log file, which show it loaded into the Windows Update programs with all nine hooks in place. After the next restart by Windows Update, the CALL line in the same file names the API and the module that called it. Version 1 stops there and leaves the restart alone.

## Uninstall
Open Windhawk, find the mod, and disable it or remove it. From an elevated shell:

```
windhawk-cli.exe mod disable local@restart-api-logger --yes
windhawk-cli.exe mod remove local@restart-api-logger --yes
```

The mod doesn't delete its log file, so it stays in %ProgramData%.

## Use at your own risk
This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash, or other harm it causes. The mod loads into Windows Update programs, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
