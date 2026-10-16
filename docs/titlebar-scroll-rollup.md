# Title Bar Scroll Roll-Up tutorial

This guide sets up the Title Bar Scroll Roll-Up mod for Windhawk and walks you through one roll-up and back. When you're done, you can scroll the mouse wheel on a window's title bar to fold the window up to that bar, and scroll the other way to bring it back. It's for anyone who wants a window out of the way for a moment but doesn't want to minimize it or close it. KDE and older Linux desktops can roll a window up like this, and Windows doesn't have it at all. The two screenshots come from the roll-up test in the README's Status section, on a plain test window called Demo window.

## Contents
- [What it fixes](#what-it-fixes)
- [Before you start](#before-you-start)
- [Install](#install)
- [Use it in four steps](#use-it-in-four-steps)
- [The result and the settings behind it](#the-result-and-the-settings-behind-it)
- [Reverse and the hold key](#reverse-and-the-hold-key)
- [Settings](#settings)
- [How it works under the hood](#how-it-works-under-the-hood)
- [Limits and troubleshooting](#limits-and-troubleshooting)
- [What's tested and what isn't](#whats-tested-and-what-isnt)
- [Wrap-up](#wrap-up)
- [Uninstall](#uninstall)
- [Use at your own risk](#use-at-your-own-risk)

## What it fixes
Sometimes a window covers what you need, but you still want it on screen. Maybe it's a chat or a log you'll look at again in a minute. If you minimize it, it goes to the taskbar and you lose track of where it was. If you move it, you have to drag it back later. Windows gives you nothing in between, so the window either covers your work or it's gone.

This mod adds the in-between state. The window stays where it is, and only its title bar shows.

## Before you start
You need three things.

- Windows 11. The repo's mods are only tested on Windows 11, and nobody has tried this one on Windows 10.
- Windhawk, from [windhawk.net](https://windhawk.net).
- The file mods/titlebar-scroll-rollup.wh.cpp from this repo.

A mouse with a scroll wheel helps. The mod only reacts to vertical wheel events.

## Install
The repo describes two ways to add the mod as a local mod.

From the Windhawk app:
1. Open Windhawk and choose Create a new mod.
2. Paste the whole contents of titlebar-scroll-rollup.wh.cpp.
3. Compile and enable the mod.

From a shell you've opened as administrator:

```
windhawk-cli.exe mod install --file mods/titlebar-scroll-rollup.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine.

To check that it loaded, list your mods and look for local@titlebar-scroll-rollup, which is the id a local install gets.

```
windhawk-cli.exe mod list
```

The mod doesn't log anything when it works. If it can't install its mouse hook, it writes one line to the Windhawk log that begins with Could not install the mouse hook, followed by an error number. If it can't start its own thread, it writes nothing and tells Windhawk that it failed to load.

## Use it in four steps
The two screenshots below are older captures from the README and carry no step badge. They belong to steps 1 and 2.

### 1. Put the pointer on an empty part of the title bar
Open an ordinary window that isn't maximized, minimized or full screen. Rest the pointer on its title bar, away from the minimize, maximize and close buttons.

![A test window called Demo window with all of its text in view, before the roll-up](../images/rollup-before.png)

The whole Demo window is in view, with its title bar and its two lines of text.

The gotcha is the title bar buttons. They don't report themselves as caption, so a scroll on them does nothing.

### 2. Scroll the wheel up
Scroll up once. The window rolls up to its title bar.

![The same test window after a scroll up on its title bar. The title bar is still there, and below it only the frame outline is left, with the backdrop showing through](../images/rollup-after.png)

The title bar stays and the content is gone. The frame outline is still there, and you can see the backdrop through it.

The gotcha is that the window keeps its real size. The mod clips it instead of resizing it, so the frame outline you see is the window's full size. The mod's own notes say the taskbar still lists a rolled-up window and its thumbnail shows the full window. No recorded test checked the thumbnail.

### 3. Scroll the wheel down
Keep the pointer on the title bar and scroll down. The window comes back as it was.

No screenshot for this step. The test recorded that scroll down brought the window back, but no image of that moment was kept.

The gotcha is a window the mod didn't roll up. A scroll down on it does nothing to the window and goes to the app as usual.

### 4. Scroll in the body of the window
Move the pointer below the title bar and scroll. Nothing changes, and the app gets the scroll as usual.

No screenshot for this step. Nothing changes on screen, so a still image would prove nothing.

The gotcha is the size of the area the mod watches. It looks at the top 100 pixels of the window, scaled for the window's DPI, and then asks the app whether the point is caption. A point lower than that band never counts, even if the app reports it as caption.

## The result and the settings behind it
The test in the README's Status section ran on Windows 11 build 26300 with a single monitor, on plain test windows. Scroll up on the title bar rolled the window up. The frame outline stayed and the content was hidden. Scroll down brought it back.

The record doesn't list the settings. Scroll up rolled the window up, and that's what the mod does with reverse off, which is the default. The defaults are enabled on, reverse off and modifier none.

## Reverse and the hold key
If you turn on Reverse the scroll direction, scroll down rolls up and scroll up brings the window back.

If you set Hold a key while scrolling to Ctrl, Shift or Alt, the mod only acts while you hold that key. Without it, the scroll goes to the app as usual. A still image can't show a key you hold, so there's no screenshot for this, and no recorded test covers either setting.

## Settings
You'll find these in the mod's Settings tab in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Name in Windhawk | Default | Allowed values | Effect |
| --- | --- | --- | --- | --- |
| enabled | Roll up on title bar scroll | on | on, off | Turns the roll-up on or off. When it's off, every title bar scroll goes to the app. |
| reverse | Reverse the scroll direction | off | on, off | Swaps the directions. Off means scroll up rolls up. On means scroll down rolls up. |
| modifier | Hold a key while scrolling | none | none (No key needed), ctrl (Ctrl), shift (Shift), alt (Alt) | A key you must hold while you scroll. none means no key is needed. |

Changes take effect right away. The mod reads the settings again as soon as you save them.

To set a value from an elevated shell, use the form the repo's README gives.

```
windhawk-cli.exe mod settings set local@titlebar-scroll-rollup "'modifier=ctrl'"
```

## How it works under the hood
The mod runs inside explorer.exe and nowhere else. It doesn't hook any functions inside apps. When it loads, it starts one thread of its own and installs a low-level mouse hook on that thread with SetWindowsHookEx and WH_MOUSE_LL. The hook only looks at vertical wheel events.

For each wheel event, the mod checks these things in order.

1. The roll-up is enabled and the modifier key, if you set one, is held down.
2. There's a window under the pointer. The mod moves up to its top-level window.
3. The pointer is within the top 100 pixels of that window. The 100 is scaled for the window's DPI.
4. The window answers a hit test that says the point is its caption. The mod sends WM_NCHITTEST and waits at most 40 milliseconds. If the app is hung, the mod gives up on that scroll.

A roll-up doesn't resize the window. The mod sets a window region with SetWindowRgn that's as wide as the window and as tall as the title bar. It works out that height from the caption height for the window's DPI, plus the gap between the window's outer edge and its visible frame as the desktop window manager reports it, plus one pixel. The app keeps its real size, so apps with a minimum size still work. The mod keeps a list of the windows it rolled up, and it drops entries for windows that have closed.

When the mod acts on a scroll, it swallows that scroll so the app doesn't see it. When it doesn't act, it passes the scroll on.

When you disable or remove the mod, its thread removes the mouse hook. Then it clears the region on every window it rolled up, so all of them come back.

## Limits and troubleshooting
These come from the code and the mod's own readme.

Limits.
- Maximized, minimized and full screen windows are skipped. A window counts as full screen when it covers its whole monitor.
- Only windows with a standard caption style are rolled up. Child windows and tool windows are skipped.
- A window that already has a region of its own, for example one with a custom shape, is skipped.
- The mod only acts where the app reports the point as caption. An app that draws its own title bar takes part only if it reports caption for that spot.
- The mod's own notes say windows that run as administrator can't be changed from Explorer, so they're skipped.
- If you turn Roll up on title bar scroll off, windows that are already rolled up stay that way. While it's off, you can't scroll them back down either.
- Horizontal scrolling isn't handled.

A scroll on the title bar does nothing. Check that the window isn't maximized and doesn't fill the whole screen. Move the pointer off the title bar buttons. If you set a modifier key, hold it while you scroll. If none of that helps, look in the Windhawk log for Could not install the mouse hook. That line means the mod couldn't start its hook.

It works in one app and not in another. The second app probably draws its own title bar and doesn't report that spot as caption, or it runs as administrator, or it uses a window region already. The mod skips all three.

A window stays rolled up and won't come back. You may have turned the roll-up off, or changed the modifier, after you rolled it up. Turn it back on, or hold the new modifier, and scroll down on the title bar. You can also disable the mod in Windhawk. That brings every rolled-up window back.

## What's tested and what isn't
The README's Status section records a check of the roll-up on plain test windows, on Windows 11 build 26300 with a single monitor. Scroll up on the title bar rolled the window up, the frame outline stayed and the content was hidden. Scroll down brought it back. Steps 1 and 2 show that test.

The repo doesn't record a test on real apps, on more than one monitor, with Reverse or a modifier key, of the taskbar thumbnail, or on Windows 10. Those follow from reading the code and the mod's notes.

## Wrap-up
You installed the mod, scrolled up on a title bar to roll a window up, and scrolled down to bring it back. The window kept its place the whole time. By the code, scrolls in the window body and on the title bar buttons go to the app as usual.

## Uninstall
Open Windhawk, find the mod, and disable it or remove it. From an elevated shell:

```
windhawk-cli.exe mod disable local@titlebar-scroll-rollup --yes
windhawk-cli.exe mod remove local@titlebar-scroll-rollup --yes
```

Every window the mod rolled up comes back when it unloads.

## Use at your own risk
This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash or other harm it causes. It runs inside Windows apps, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
