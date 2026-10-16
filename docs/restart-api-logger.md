# Restart API Logger

Restart API Logger is a Windhawk mod for Windows 11 that records which Windows API call Windows Update uses to restart your PC, and which code made that call. It doesn't stop the restart. It's for you if Windows Update restarts your PC on its own and you want to know exactly how it does it. Version 1 only logs. It blocks nothing, delays nothing and changes nothing.

## The problem

Windows Update sometimes restarts the PC on its own. The Windows event log tells you which program asked for the restart, but it doesn't tell you which API call was used. Windows has several APIs that shut down or restart the PC, and the event log doesn't say which one ran.

This mod sits inside the two Windows Update programs, MoUsoCoreWorker.exe and TrustedInstaller.exe, and writes down every shutdown or restart call they make.

## Install

You need [Windhawk](https://windhawk.net) installed first. Then pick one way.

From the Windhawk app:

1. Open Windhawk and choose Create a new mod.
2. Paste the contents of mods/restart-api-logger.wh.cpp.
3. Compile and enable the mod.

From an elevated shell (run as administrator):

```
windhawk-cli.exe mod install --file mods/restart-api-logger.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine. A locally installed copy gets the ID local@restart-api-logger.

### Check that it's loaded

When the mod loads into one of the two programs, it writes one line whose message starts with INIT. In the Windhawk log the line starts with INIT. In the log file it starts with the date and time, and INIT comes right after them. The line has the program's path, its process ID, the APIs it hooked and the DLL each one came from, the APIs it skipped and why, and where the log file goes. If you never see that line for TrustedInstaller.exe or MoUsoCoreWorker.exe, Windhawk didn't load the mod into that program. Windhawk protects some system processes. On our test PC it did load the mod into both, as the log below shows.

You can also list your mods from an elevated shell and look for local@restart-api-logger:

```
windhawk-cli.exe mod list
```

### What the log file looks like

This screenshot shows the last two lines of the real log file on our test PC, opened in a plain text window on a solid backdrop.

![Two INIT lines from the restart logger log file, both from MoUsoCoreWorker.exe, each with nine hooked APIs and skipped=[none]](screenshots/restart-api-logger-step-1-log-file.png)

Both are INIT lines from MoUsoCoreWorker.exe. Each one lists the nine APIs as hooked, says skipped=[none] and gives the log file path. When we checked, the file held 60 lines from October 15 and 16, 2026. 53 were INIT lines from MoUsoCoreWorker.exe and 7 were INIT lines from TrustedInstaller.exe. It had no CALL line yet, so it hasn't caught a restart.

## How you use it

The mod runs in the background. There's nothing to click.

1. Install and enable the mod. Leave both settings at their defaults.
2. Look for the INIT line in the Windhawk log or in the log file. In the file, INIT comes after the date and time. The file lives here:

```
%ProgramData%\restart-api-logger.log
```

3. Leave the mod on until Windows Update restarts your PC.
4. After the restart, open the log file. A restart can kill the program before the Windhawk log is saved, so the file is the place to look.
5. Find the CALL lines. Each one names the API that was called, the program that called it, the reason code and flags in hex, and the module that made the call.
6. Look for a RETURN line after it. It's there only if the program was still alive when the call came back.

These are the messages the mod writes:

| Message starts with | What it means |
| --- | --- |
| INIT | The mod loaded into a program. It lists the hooked APIs, the skipped APIs with a reason, and the log file path, or off when the file log is off. |
| CALL | A hooked API was called. The mod writes this line before it passes the call on. |
| RETURN | The real API came back. The line has the result. |
| UNINIT | The mod is unloading from a program. |
| Can't open the log file | The mod couldn't open its file. This line goes to the Windhawk log only. |

Each line in the file starts with the date and a 12-hour time, for example 2026-10-16 3:07:42 PM, then two spaces and the message.

### Basic and detailed lines

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

For an API that has no reason code or no flags, the line says n/a in that spot.

## Settings

| Key | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| logLevel | detailed | basic or detailed. Any value other than basic counts as detailed. | basic logs the program, API, reason, flags and caller module. detailed logs everything in the table above. |
| fileLog | on | On or off. | On also appends each line to %ProgramData%\restart-api-logger.log. Off writes to the Windhawk log only. |

The mod reads its settings again when you change them, and the next line it writes uses the new values. The hooks themselves are set once, when the mod loads. To change a setting from the shell, pass the key and value like this:

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

An API that isn't found is skipped with the reason not found. If two names point to the same code, the mod hooks the first one and skips the other with the reason same address as another hooked API. If Windhawk can't set a hook, the reason is hook failed.

Every hook works the same way. It builds the CALL line, writes it to the Windhawk log, appends it to the file, pushes the file to disk and closes it. Then it calls the real function with the same arguments. When that returns, it logs a RETURN line and hands back the real result unchanged. For the APIs that report errors through the Windows last-error value, it puts that value back as it was, too.

Text arguments, such as the machine name and the message, are read with a safe memory read. A missing pointer shows as (null) and a bad one shows as (unreadable). The caller module is the module that holds the return address. If no module holds it, a detailed line says caller=(unknown module), and a basic line leaves the caller field out.

The file is capped at 256 KB. When it's bigger than that, the mod keeps about the newest 192 KB before it writes the next line. It cuts at a line break, so the file doesn't start in the middle of a line. When the mod unloads, it writes the UNINIT line. It doesn't delete the file.

## Limits and known gaps

- Version 1 only logs. It doesn't block, delay or change any call.
- It loads into two programs only. A restart that another program asks for isn't logged.
- It only sees the nine APIs listed above. A restart that goes another way isn't logged.
- Windhawk protects some system processes. On our test PC it loaded the mod into both programs, but no other PC has been checked.
- A restart can end the program before the RETURN line is written. The CALL line is already on disk by then.
- When the file passes 256 KB, the oldest lines are dropped.

## What's tested and what isn't

The README's Status section records what the log file on our test PC shows. On Windows 11 build 26300, Windhawk loaded the mod into MoUsoCoreWorker.exe and TrustedInstaller.exe again and again on October 15 and 16, 2026, and every INIT line hooked all nine APIs. The file has no CALL line, so the repo doesn't record a restart that was logged with it. The repo says the mods are tested only on Windows 11, and Windows 10 hasn't been tried.

## Troubleshooting

There's no INIT line for either program. Windhawk didn't load the mod into it. The mod can't log anything in a program it isn't loaded in.

The INIT line lists APIs under skipped. Square brackets wrap the whole skipped list, and the reason sits in round brackets right after each name, like NtShutdownSystem(not found). Not found means the mod found no export by that name in the DLLs listed for it above, after it loaded any of them that weren't loaded yet. The mod still logs the APIs it did hook.

The log says Can't open the log file. The mod logs to the Windhawk log only. It tries the file again on every line but reports the failure only once, until you change a setting.

The INIT line says fileLog=no path. The mod couldn't build a path from the ProgramData variable, so it has no file to write.

The PC restarted but there's no CALL line. Check for an INIT line from before the restart first. If there is one, the restart didn't go through a hooked API in that program.

## Uninstall

Open Windhawk, find the mod, and disable or delete it. From an elevated shell you can run one of these:

```
windhawk-cli.exe mod disable local@restart-api-logger --yes
windhawk-cli.exe mod remove local@restart-api-logger --yes
```

The mod doesn't delete its log file, so it stays in %ProgramData%.

## Use at your own risk

This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash, or other harm it causes. The mod loads into Windows Update programs, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
