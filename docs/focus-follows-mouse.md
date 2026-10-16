# Focus Follows Mouse tutorial

This guide sets up the Focus Follows Mouse mod for Windhawk and walks you through your first hover with no click. When you're done, the window under your pointer gets keyboard focus after a short rest, and by default it stays where it is in the stack. So the window you're reading stays on top while you type in the one underneath. It's for you if you've used a Linux desktop that works this way, or if you often type into one window while you read another. The screenshot comes from one live run on Windows 11 build 26300 with the default settings, on two plain test windows called Front window and Back window.

## Contents
- [What it fixes](#what-it-fixes)
- [Before you start](#before-you-start)
- [Install](#install)
- [Set up two windows](#set-up-two-windows)
- [Use it in three steps](#use-it-in-three-steps)
- [The result and the settings behind it](#the-result-and-the-settings-behind-it)
- [Settings](#settings)
- [How it works under the hood](#how-it-works-under-the-hood)
- [Limits and troubleshooting](#limits-and-troubleshooting)
- [What's tested and what isn't](#whats-tested-and-what-isnt)
- [Wrap-up](#wrap-up)
- [Uninstall](#uninstall)
- [Use at your own risk](#use-at-your-own-risk)

## What it fixes
On Windows, the window under your pointer doesn't get keyboard focus until you click it. The click also brings that window to the front, and it can cover the window you were reading.

Windows 11 has a weak version of hover focus hidden in its Accessibility settings. The mod's own notes say it always pulls the hovered window to the front. This mod turns on that same built-in Windows feature and gives you control over the delay and over whether the window comes to the front.

## Before you start
You need three things.

- Windows 11. The repo's mods are only tested on Windows 11, and nobody has tried this one on Windows 10.
- Windhawk, from [windhawk.net](https://windhawk.net).
- The file mods/focus-follows-mouse.wh.cpp from this repo.

## Install
The repo describes two ways to add the mod as a local mod.

From the Windhawk app:
1. Open Windhawk and choose Create a new mod.
2. Paste the whole contents of focus-follows-mouse.wh.cpp.
3. Compile and enable the mod.

From a shell you've opened as administrator:

```
windhawk-cli.exe mod install --file mods/focus-follows-mouse.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine.

To check that it loaded, list your mods and look for local@focus-follows-mouse, which is the id a local install gets.

```
windhawk-cli.exe mod list
```

The mod runs inside explorer.exe. When it loads with Focus the window under the mouse on, it writes two lines to the Windhawk log. The first one starts with Saved original and records the values Windows had before, as tracking, zorder and timeout. The numbers in it depend on your PC. The second one records what the mod set, and with the defaults it reads like this.

```
Applied: delay=300 raise=0
```

## Set up two windows
You need two windows that overlap a little, so one sits partly behind the other.

1. Open two windows and move them so they overlap.
2. Click the one you want in front.

There's no screenshot of this setup on its own. Step 1 shows the same two windows.

## Use it in three steps
The number matches the red badge in the corner of the screenshot.

### 1. Type while the pointer rests on the back window
Move the pointer onto the part of the back window you can still see, leave it there for a moment without a click, and type. With the defaults the pointer has to rest for 300 ms.

![Two overlapping test windows. Back window shows the words typed here inside a red box, and Front window still sits on top of its top right corner](screenshots/focus-follows-mouse-step-1-typing-into-back-window.png)

The words typed here are in Back window, and Front window still covers Back window's top right corner.

Back window's title text is dark and Front window's is gray, which is how Windows 11 marks the active window. The red arrow marks the spot where the pointer rested. The screenshot doesn't show the pointer itself.

The gotcha is the rest time. Windows moves focus only after the pointer has rested for the delay, so keys you type sooner should still go to the window that had focus before. That follows from how the delay works, and no recorded test checked it.

### 2. Change the hover delay
Open the mod's settings and set Hover delay (milliseconds) to a value you like, from 0 to 5000. The mod applies it as soon as you save. The Applied line in the log shows the new delay.

No screenshot for this step. A delay can't be seen in a still image, and no recorded test used a delay other than 300.

The gotcha is the range. A value below 0 counts as 0, and a value above 5000 counts as 5000.

### 3. Turn on Bring the window to the front when it gets focus
Switch it on if you want the hovered window to come to the front too. The Applied line then ends in raise=1.

No screenshot for this step. No recorded test turned this setting on.

The gotcha is what you lose. With it on, the window you hover covers the one you were reading, which is the same as the built-in Accessibility option.

## The result and the settings behind it
The live run in step 1 used these values.

| Setting | Value |
| --- | --- |
| Focus the window under the mouse (enabled) | on |
| Hover delay (milliseconds) (delayMs) | 300 |
| Bring the window to the front when it gets focus (raiseWindow) | off |

The run made Front window the active window first. Then the pointer moved onto the part of Back window you can see and stayed there for 1.5 seconds, with no click. Then the run typed the words typed here.

The words went into Back window, and Windows still reported Front window above Back window in the stack. So the hover gave Back window focus, and Back window didn't come to the front.

## Settings
You'll find these in the mod's Settings tab in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Name in Windhawk | Default | Allowed values | Effect |
| --- | --- | --- | --- | --- |
| enabled | Focus the window under the mouse | on | on, off | On turns hover focus on. Off puts back the Windows values the mod saved the first time it applied its values with enabled on. |
| delayMs | Hover delay (milliseconds) | 300 | 0 to 5000 milliseconds. A value below 0 counts as 0 and a value above 5000 counts as 5000. | How long the pointer has to rest on a window before it gets focus. |
| raiseWindow | Bring the window to the front when it gets focus | off | on, off | On also brings the focused window to the front. Off gives it focus and leaves it where it is in the stack. |

The mod applies a changed setting as soon as you save it. To change one from an elevated shell, pass the key and value like this.

```
windhawk-cli.exe mod settings set local@focus-follows-mouse "'delayMs=500'"
```

## How it works under the hood
The mod loads only into explorer.exe, but it doesn't hook anything there. It changes three Windows system settings through SystemParametersInfo.

- Active window tracking, which is the hover focus switch itself.
- The tracking delay, set from delayMs.
- The raise-on-hover flag, set from raiseWindow.

These are Windows system settings, not Explorer settings, which is why the mod can live in explorer.exe. We haven't tested whether the change reaches every app's windows.

The first time it applies its values, the mod reads and saves the three values Windows had. When you turn enabled off, or when the mod unloads, it writes those saved values back. It doesn't write to the registry. The mod's own notes say Windows resets these values at the next sign in, and the mod sets them again each time Explorer starts.

## Limits and troubleshooting
These come from the code and the mod's own readme.

Limits.
- If you change these options in Windows Settings while the mod runs, the mod keeps its own values until you change a mod setting or reload it.
- When the mod unloads, it puts back the values it saved the first time it applied its values with enabled on. If enabled was never on, it saved nothing and puts nothing back. A change you made in Windows Settings in between is replaced by those older values.
- Windows decides which windows count. Menus and the taskbar behave as Windows decides.
- If the mod can't read the current values when it first applies its own, it doesn't change anything.

Focus doesn't move when you hover. Look in the Windhawk log for the Applied line. If you see Could not read the current hover focus settings instead, the mod couldn't read the Windows values and it left everything as it was. Check that Focus the window under the mouse is on, too.

The window still jumps to the front. Check that Bring the window to the front when it gets focus is off. The mod writes it into the Applied line as raise=0 or raise=1.

Focus moves too fast or too slow. Change Hover delay (milliseconds). Anything past 5000 counts as 5000.

You changed hover focus in Windows Settings and the mod changed it back. That's expected while the mod runs. It keeps its own values until you change a mod setting or reload it.

## What's tested and what isn't
The README's Status section records one live run of focus follows mouse on Windows 11 build 26300 with a single monitor, the one in step 1. A hover with no click gave focus to a window behind another one. The typed text went into it, and Windows still reported the front window above it in the stack.

That run used two test windows, the default delay of 300 ms and raiseWindow off. raiseWindow on, other delays, real apps such as a browser, and the restore of the saved values when the mod unloads haven't been tested. Those follow from reading the code. The repo also says the mods are tested only on Windows 11, and Windows 10 hasn't been tried.

## Wrap-up
You installed the mod, checked the Applied line in the log, and typed into a window you only pointed at. The window in front stayed in front. The delay and the raise switch are the two settings that change that behavior.

## Uninstall
Open Windhawk, find the mod, and disable it or remove it. From an elevated shell:

```
windhawk-cli.exe mod disable local@focus-follows-mouse --yes
windhawk-cli.exe mod remove local@focus-follows-mouse --yes
```

When the mod unloads, it writes the saved Windows values back and logs Restored original hover focus settings.

## Use at your own risk
This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash, or other harm it causes. The full terms are in the [LICENSE](../LICENSE) file.
