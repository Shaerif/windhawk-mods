# Window Rules

Window Rules is a Windhawk mod for Windows 11 that gives an app a fixed position, size, monitor, always-on-top flag or opacity when its window opens. You write a rule for a program, and the mod applies it the first time each of that program's windows appears. It's for people who open the same apps every day and keep dragging them back to the same spot. KDE has had per-app window rules for a long time, and Windows has nothing built in.

## The problem

Some apps open in a different place and at a different size every time. Others always open on the wrong monitor, or right on top of the window you're working in. You move the window, you resize it, and the next day you do it again. Windows doesn't let you pin an app to a place or a size, and it has no setting to keep one app above the rest.

## Install

You need Windhawk installed first. Then pick one of the two ways the repo describes.

From the Windhawk app:

1. Open Windhawk and choose Create a new mod.
2. Paste the contents of mods/window-rules.wh.cpp.
3. Compile and enable the mod.

From a shell that runs as administrator:

```
windhawk-cli.exe mod install --file mods/window-rules.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine.

To check that it's loaded, list your mods. A mod you installed from a file gets the ID local@window-rules.

```
windhawk-cli.exe mod list
```

The mod ships with one blank rule, so it does nothing until you fill that rule in. Once you've added a rule and opened a matching window, the Windhawk log shows a line like [ShowWindow] applied a rule to window, followed by the window's handle. That line shows up only while Log applied rules is on, and it's on by default.

## How you use it

1. Open the mod's settings in Windhawk.
2. In Rules, fill in the first rule, the blank one the mod ships with. Don't leave it blank above a new rule, because the mod stops reading the list at a fully blank rule. The section More than one rule below has the details.
3. Type the program file name in Process, for example notepad.exe.
4. Fill in the fields you want. Leave the rest at 0 or off.
5. Save the settings.
6. Close the app if it's open, then start it again. The new window opens and the rule is applied to it.

Rules only apply to windows that open after you save. A window that's already open stays where it is.

### The rule fields

Each rule has ten fields. These are the exact keys from the mod's settings block.

| Field | Type | Default | What it does |
| --- | --- | --- | --- |
| process | text | empty | The program file name, for example notepad.exe. Required. It's matched against the file name of the running program, without its folder, and letter case doesn't matter. Include the .exe part. |
| title | text | empty | Optional. Text the window title must contain. Case doesn't matter. Leave it empty and the rule matches any window of that program. |
| monitor | number | 0 | 1 for the first monitor, 2 for the second, and so on. 0 keeps the monitor the window is on. A number with no monitor behind it also falls back to the window's own monitor. |
| x | number | 0 | Pixels from the left edge of the monitor's work area. |
| y | number | 0 | Pixels from the top edge of the monitor's work area. |
| width | number | 0 | Width in pixels. 0 keeps the window's own width. |
| height | number | 0 | Height in pixels. 0 keeps the window's own height. |
| maximize | on or off | off | When on, the window is maximized after the other fields are applied. |
| alwaysOnTop | on or off | off | When on, the window is kept above windows that aren't set to stay on top. |
| opacity | number | 0 | 10 to 100 percent. Any other value, 0 included, leaves the opacity alone. |

The work area is the part of the monitor the taskbar doesn't cover.

### When the window moves

The mod moves a window only if the rule sets a monitor above 0, an x or y that isn't 0, or a width or height above 0. If it does move the window, x and y are always used as offsets from the top left corner of the work area. So an x and y of 0 put the window right in that corner. The repo README describes 0 as leaving x and y alone, but in the code that's only true when nothing else in the rule asks for a move.

Before the move, the mod fits the window inside the work area:

- A width or height bigger than the work area is cut down to the work area.
- If the window would run past the right or bottom edge, it's pushed back in.
- A negative x or y is moved up to the left or top edge.

A rule can't place a window off screen.

### A worked example

Say you want Notepad windows with notes in the title to open 100 pixels from the left, 80 pixels from the top, at 900 by 700, kept on top and a little see-through. Here's that rule as the first item of the rules list, in the same nested shape the mod's settings block uses for its default rule:

```
- rules:
  - - process: notepad.exe
    - title: notes
    - monitor: 0
    - x: 100
    - y: 80
    - width: 900
    - height: 700
    - maximize: false
    - alwaysOnTop: true
    - opacity: 90
```

In the Windhawk settings screen you type the same values into the fields of one rule.

Here's what each field does in this rule:

- process notepad.exe limits the rule to Notepad. The mod loads this rule only inside notepad.exe.
- title notes means a Notepad window only matches if its title contains the text notes anywhere, in any letter case. Notes.txt matches, and so does keynotes.txt. Untitled doesn't.
- monitor 0 keeps the window on whichever monitor it opened on.
- x 100 and y 80 put the top left corner 100 pixels right of and 80 pixels below the corner of that monitor's work area.
- width 900 and height 700 set the size. On a small screen, the size is cut down to fit the work area.
- maximize false leaves the window at that size.
- alwaysOnTop true keeps it above your other windows.
- opacity 90 makes it 90 percent opaque, so a little of what's behind it shows through.

### More than one rule

The mod reads up to 200 rules. For each program it keeps only the rules whose process matches it, in the order you wrote them. When a window opens, the first of those rules that matches its title wins, and the rest are ignored. A rule without a title matches every window of its program, so a rule for the same program below it never gets a turn.

The mod ships with one fully blank rule at the top of the list. Fill in that first rule, at index 0, rather than adding a new rule below it. Here's how the mod reads the list. A rule with an empty process, an empty title, and monitor, width and opacity all at 0 ends the list. The mod stops there and doesn't read any rule below it. A rule with an empty process but a title, a monitor, a width or an opacity set is skipped, and the mod goes on to the next rule. If you leave the first rule blank and add yours below it, the mod stops at the blank one and never loads yours.

A rule that has only a process and nothing else matches, but it doesn't change anything.

## Settings

You'll find these in the mod's settings in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| enabled | true | true, false | Turns all rules on or off. When it's off, no rule is applied. |
| logRules | true | true, false | Writes a line to the Windhawk log each time a rule is applied to a window. |
| rules | one empty rule | a list of rules, up to 200 are read | The rules themselves. See the rule fields above. |

Changes take effect for windows that open after you save. The mod reads the rules again as soon as the settings change.

## How it works under the hood

The mod runs inside every process except File Explorer and core system processes. The excluded ones are explorer.exe, csrss.exe, dwm.exe, winlogon.exe, LogonUI.exe, fontdrvhost.exe, sihost.exe and ShellHost.exe.

When it loads inside a program, it reads that program's file name and keeps only the rules for it. Then it hooks three functions with Windhawk:

- ShowWindow, when a window is shown with one of these commands: SW_SHOW, SW_SHOWNORMAL, SW_SHOWNA, SW_SHOWNOACTIVATE, SW_RESTORE or SW_SHOWDEFAULT.
- SetWindowPos, when the call carries SWP_SHOWWINDOW.
- CreateWindowExW, when the new window has WS_VISIBLE and no parent.

Each hook calls the original function first and then checks the window. A window is a candidate only if it's a top-level window with a caption, and isn't a tool window or a window that can't be activated. The mod remembers each candidate it has seen, so it checks a window once. Then it reads the window title, finds the first matching rule and applies it in this order: the move and resize with SetWindowPos, always on top with HWND_TOPMOST, opacity as a layered window with SetLayeredWindowAttributes, and last the maximize with ShowWindow.

The mod has no unload step of its own. When you disable it, Windhawk removes the hooks, and windows keep whatever the rules already set.

## Limits and known gaps

- Rules apply once, when a window first appears. If you move the window afterward, it stays where you put it.
- The title is read at that first moment. If an app sets its real title later, a rule with a title may not match. The mod has already marked the window as seen, so it won't try again.
- A window that sets its own transparency keeps it. The opacity field skips any window that's already layered.
- The opacity field doesn't apply 1 to 9 or anything above 100.
- Monitor numbers follow the order Windows lists the displays in. The repo hasn't checked that this order matches the numbers in Display settings.
- Only the first 511 characters of a window title are compared.
- The mod's own notes say windows that run as administrator can't be changed by a normal process.
- File Explorer and the core system processes listed above are excluded for stability. You can add more @exclude lines in the mod's source if you want it to skip other programs.
- After the mod has seen more than 2048 windows inside one program, it clears its list of seen windows. A window that shows itself again after that can get its rule a second time.

## What's tested and what isn't

The README's Status section records a check of Window Rules on a plain test window, on Windows 11 build 26300 with a single monitor. One rule matched the test window by app and title. The window opened at 60, 60 at 380 by 220. The rule moved it to 500, 300, made it 560 by 340 and set its opacity to 80.

The repo doesn't record a test of the monitor field, the maximize field or always on top, a test with more than one monitor, a test on real apps, or a test on Windows 10. Treat those as untested.

## Troubleshooting

A rule doesn't apply. Check the process name first. It must be the file name of the program with .exe at the end, not a folder path and not the name shown in the Start menu. Then check that Enable rules is on. If the rule has a title, check that the window's title contains that text when the window first opens.

You changed a rule and the open window didn't move. Rules only apply when a window first appears. Close the app and open it again.

The window went to the top left corner. Your rule sets a width, a height or a monitor, and x and y are 0. Whenever the window moves, x and y count from the corner of the work area. Set them to the offset you want.

The window isn't where you asked. The mod fits every window inside the work area, so a size or offset that would run past the edge gets pulled back in.

The log says a rule was applied but nothing changed. That line means a rule matched the window. A rule with only a process, or with opacity on a window that's already layered, matches without changing anything.

Opacity doesn't change. The value has to be between 10 and 100, and the window can't already be a layered window.

## Uninstall

Open Windhawk, find the mod, and disable or delete it. From a shell that runs as administrator, you can do the same:

```
windhawk-cli.exe mod disable local@window-rules --yes
windhawk-cli.exe mod remove local@window-rules --yes
```

Windows that are already open keep the position, size, on-top state and opacity the rules gave them. Close and reopen an app to get its normal behavior back.

## Use at your own risk

This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash or other harm it causes. It runs inside Windows apps, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
