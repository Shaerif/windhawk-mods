# Docs

There's one step-by-step tutorial per mod. Each one covers install, the steps with a screenshot where one was captured, the settings behind the result, limits, what's been tested, and removal. The numbered red badge on a screenshot matches the step it belongs to.

| Tutorial | What it does | Screenshots | Recorded live test |
| --- | --- | --- | --- |
| [Bulk Copy File and Folder Names](bulk-copy-file-and-folder-names.md) | Copy the names of the files and folders you select in File Explorer, without their paths, with a hotkey or the right-click menu. | 4, steps 1 to 4 | One run on 7 items |
| [Clipboard Hang Watchdog](clipboard-hang-watchdog.md) | Find the program that holds the clipboard open or blocks it and log it. It starts in log-only mode and can end the program if you turn that on. | 1, step 1 | Hold detection, ending a holder, log trim |
| [Focus Follows Mouse](focus-follows-mouse.md) | Give keyboard focus to the window under the pointer with no click, without bringing it to the front unless you ask. | 1, step 1 | One run with the defaults |
| [Off-Screen Window Rescue](offscreen-window-rescue.md) | Move a window back onto the screen when it first opens outside the visible work area. | 1, step 1, no badge | One test window at x = -3000 |
| [Restart API Logger](restart-api-logger.md) | Log which Windows API Windows Update calls to restart the PC, and who called it. Version 1 only logs and blocks nothing. | 1, step 1 | Loads and hooks only, no restart logged yet |
| [Same-App Window Cycler](same-app-window-cycler.md) | Press Alt plus the backtick key (the key above Tab on a US keyboard) to cycle through the windows of the app you're using. | 3, steps 1 to 3 | One run on three test windows |
| [Title Bar Scroll Roll-Up](titlebar-scroll-rollup.md) | Scroll the mouse wheel on a title bar to roll the window up to its title bar and back. | 2, steps 1 and 2, no badge | One test window |
| [Window Rules](window-rules.md) | Give an app a set position, size, monitor, always-on-top flag or opacity each time its window opens. | 1, step 1 | Two test windows, one below a blank rule |

The README's Status section has the full record of what's been tested. Anything a tutorial doesn't tie to a recorded test comes from reading the code.
