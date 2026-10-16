# Same-App Window Cycler

Same-App Window Cycler is a Windhawk mod for Windows 11 that adds one shortcut. You press Alt and the backtick key, and you move to the next window of the app you're already in. It's for people who keep several windows of one program open, like a handful of browser windows or two Notepad files, and get tired of hunting for them in Alt+Tab. If you've used Cmd+Backtick on a Mac, it's the same idea.

In this doc, Backtick means the key above Tab on a US keyboard. The mod asks Windows for that key by its code, VK_OEM_3. We haven't checked where that key sits on other keyboard layouts.

## The problem

Alt+Tab shows every window from every app in one row. Say you've got six browser windows and a code editor open. To reach the right browser window you press Tab several times and read each thumbnail, and the code editor sits in the middle of the row the whole time. Windows has no key that stays inside one app.

## Install

You need Windhawk installed first. Then pick one of the two ways the repo describes.

From the Windhawk app:

1. Open Windhawk and choose Create a new mod.
2. Paste the contents of mods/same-app-window-cycler.wh.cpp.
3. Compile and enable the mod.

From a shell that runs as administrator:

```
windhawk-cli.exe mod install --file mods/same-app-window-cycler.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine.

To check that it's loaded, list your mods. A mod you installed from a file gets the ID local@same-app-window-cycler.

```
windhawk-cli.exe mod list
```

Then open two Notepad windows and press Alt+Backtick. The Windhawk log should show a line like Cycling 2 windows forward. If the mod couldn't claim the key, the log says Could not register the next-window hotkey, with an error number after it.

## How you use it

1. Open two or more windows of one program. Two Notepad windows will do.
2. Click one of them so it's in front.
3. Press Alt+Backtick. Another window of the same program comes to the front. Windows from other programs don't take part.
4. Press Alt+Backtick again. With two windows, you go back to the first one. With three or more, each press brings up the window that sits furthest back in the stack, so you go round all of them in turn.
5. Press Alt+Shift+Backtick to bring up the window right behind the front one. See the limits below for how this key behaves with three or more windows.
6. If a window you cycle to is minimized, it's restored and brought to the front. That happens only while Include minimized windows is on, which it is by default.
7. If the app has only one window that counts, nothing happens.

If you hold the keys down, the switch doesn't repeat. The mod registers both keys with Windows' no-repeat flag, so you get one switch per press.

If you change the modifier to Ctrl+Alt or Win, the same steps apply with that modifier in place of Alt. Shift still reverses.

## Settings

You'll find these in the mod's settings in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| modifier | alt | alt, ctrl_alt, win | The key you hold with Backtick. alt is Alt, ctrl_alt is Ctrl+Alt, win is the Windows key. The previous-window shortcut adds Shift to whichever one you pick. |
| includeMinimized | true | true, false | When on, minimized windows are part of the cycle and get restored when you land on them. When off, they're left out. |
| currentDesktopOnly | true | true, false | When on, windows on other virtual desktops are left out. When off, they're kept in the list. |

A change to the modifier takes effect right away. The mod drops its old keys and registers the new ones without a restart.

To set a value from a shell, use the form the repo's README gives:

```
windhawk-cli.exe mod settings set local@same-app-window-cycler "'modifier=ctrl_alt'"
```

## How it works under the hood

The mod runs inside explorer.exe and nowhere else. It doesn't hook any functions. When it loads, it starts one thread of its own, creates a hidden message-only window on that thread, and registers two system-wide hotkeys with RegisterHotKey. Hotkey 1 is the modifier plus VK_OEM_3. Hotkey 2 adds Shift.

When you press a hotkey, the mod reads the front window and finds the full path of the program file that owns it. Then it walks every top-level window with EnumWindows and keeps the ones that come from the same file path. The path match ignores letter case.

A window counts only if all of these hold:

- It has no owner window, so dialogs that belong to a main window are left out.
- It isn't a tool window, unless it's also marked as an app window.
- Its title isn't empty.
- It's visible, or it's minimized and Include minimized windows is on.
- It isn't cloaked by the desktop window manager. A window on another virtual desktop is cloaked, and it's let through only when Current virtual desktop only is off. Other cloaked windows are always left out.

EnumWindows returns windows from the top of the stack down, so the front window comes first. Forward picks the last window in that list, which is the one furthest back. Backward picks the second one. The mod restores the target if it's minimized, briefly attaches its input to the thread of the current front window, and calls BringWindowToTop and SetForegroundWindow. It writes one line to the Windhawk log each time it switches.

When you disable or remove the mod, it tells its thread to quit. The thread destroys the hidden window, which unregisters both hotkeys, and then the window class is removed. The mod waits up to 3 seconds for the thread to finish. If it hasn't finished by then, the mod closes its handle to the thread and unloads anyway. We haven't tested that case.

## Limits and known gaps

- Store apps share one frame host process, ApplicationFrameHost.exe, so they'd all look like one app. The mod does nothing when one of them is in front.
- The mod's own notes say a window that runs as administrator can't be switched to from Explorer.
- Windows only count as the same app when they come from the same program file at the same path. Two copies of one program installed in two folders won't cycle together.
- When File Explorer is in front, the mod only acts if that Explorer window itself counts as a normal window. The code has no special check for the desktop or the taskbar, and we haven't tested what it does when one of them is in front.
- The mod's own notes say Alt+Shift+Backtick goes the other way. In the code it always brings the second window in the stack to the front. With two windows that's the same as going forward. With three or more, pressing it over and over flips between the same two windows instead of walking the whole list backward.
- With Current virtual desktop only off, windows on other desktops are added to the cycle. We haven't tested what Windows does when the mod brings one of them to the front.
- The key is fixed in code as VK_OEM_3. Only the modifier can be changed in the settings.

## What's tested and what isn't

The README's Status section says there's no recorded live test for the window cycler. The tests it does record ran on other mods, on Windows 11 build 26300 with a single monitor. Everything in this doc comes from reading the code and the repo's text, not from a test run. The repo hasn't tried any of the mods on Windows 10.

## Troubleshooting

Nothing happens when you press the keys. Check the Windhawk log first. A line that starts with Could not register means Windows refused the key combination, and the error number after it tells you why. One possible cause, which we haven't checked, is another program that already holds the same combination. Pick a different modifier in the settings and try again.

Nothing happens, and the log has no error. The app in front may have only one window that counts. Untitled windows, dialogs with an owner, tool windows and hidden windows don't count. If it's a Store app, the mod skips it on purpose.

A minimized window gets skipped. Include minimized windows is probably off. Turn it back on.

A window on another virtual desktop never shows up. That's the default. Turning off Current virtual desktop only adds those windows to the cycle, but that path hasn't been tested.

## Uninstall

Open Windhawk, find the mod, and disable or delete it. From a shell that runs as administrator, you can do the same:

```
windhawk-cli.exe mod disable local@same-app-window-cycler --yes
windhawk-cli.exe mod remove local@same-app-window-cycler --yes
```

Both hotkeys are released as soon as the mod unloads.

## Use at your own risk

This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash or other harm it causes. It runs inside Windows apps, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
