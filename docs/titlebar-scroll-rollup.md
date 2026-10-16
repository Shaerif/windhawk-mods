# Title Bar Scroll Roll-Up

Title Bar Scroll Roll-Up is a Windhawk mod for Windows 11 that folds a window up to its title bar when you scroll the mouse wheel on that title bar. Scroll the other way and the window comes back. It's for anyone who wants a window out of the way for a moment but doesn't want to minimize it or close it. KDE and older Linux desktops have had this for years, and Windows doesn't have it at all.

## The problem

Sometimes a window covers what you need, but you still want it on screen. Maybe it's a chat or a log you'll look at again in a minute. If you minimize it, it goes to the taskbar and you lose track of where it was. If you move it, you have to drag it back later. Windows gives you nothing in between, so the window either covers your work or it's gone.

## Install

You need Windhawk installed first. Then pick one of the two ways the repo describes.

From the Windhawk app:

1. Open Windhawk and choose Create a new mod.
2. Paste the contents of mods/titlebar-scroll-rollup.wh.cpp.
3. Compile and enable the mod.

From a shell that runs as administrator:

```
windhawk-cli.exe mod install --file mods/titlebar-scroll-rollup.wh.cpp --yes
```

A compile takes about 45 to 60 seconds on our machine, so give it time.

To check that it's loaded, list your mods. A mod you installed from a file gets the ID local@titlebar-scroll-rollup.

```
windhawk-cli.exe mod list
```

The mod doesn't log anything when it works. It writes one line to the Windhawk log only if it fails to start, and that line begins with Could not install the mouse hook. So the quickest check is to try it on a window, as below.

## How you use it

1. Open an ordinary window, for example Notepad. It shouldn't be maximized, minimized or full screen.
2. Put the pointer on the window's title bar. Pick an empty part of it, away from the minimize, maximize and close buttons.
3. Scroll the wheel up. The window rolls up to its title bar. The frame outline stays where it was, and the content below the title bar is hidden.
4. With the pointer still on the title bar, scroll the wheel down. The window comes back as it was.
5. Scroll in the body of the window, and nothing changes. The app gets that scroll as usual.

If you turn on Reverse the scroll direction, scroll down rolls up and scroll up brings the window back. If you set Hold a key while scrolling to Ctrl, Shift or Alt, the mod only acts while you hold that key. Without it, the scroll goes to the app as usual.

A rolled-up window keeps its spot on the taskbar. Its taskbar thumbnail still shows the full window.

## Settings

You'll find these in the mod's settings in Windhawk. The keys and defaults come from the settings block in the mod file.

| Key | Default | Allowed values | Effect |
| --- | --- | --- | --- |
| enabled | true | true, false | Turns the roll-up on or off. When it's off, every title bar scroll goes to the app. |
| reverse | false | true, false | Swaps the directions. Off means scroll up rolls up. On means scroll down rolls up. |
| modifier | none | none, ctrl, shift, alt | A key you must hold while you scroll. none means no key is needed. |

Changes take effect right away. The mod reads the settings again as soon as you save them.

To set a value from a shell, use the form the repo's README gives:

```
windhawk-cli.exe mod settings set local@titlebar-scroll-rollup "'modifier=ctrl'"
```

## How it works under the hood

The mod runs inside explorer.exe and nowhere else. It doesn't hook any functions inside apps. When it loads, it starts one thread of its own and installs a low-level mouse hook on that thread with SetWindowsHookEx and WH_MOUSE_LL. The hook only looks at vertical wheel events.

For each wheel event, the mod checks these things in order:

1. The roll-up is enabled and the modifier key, if you set one, is held down.
2. There's a window under the pointer. The mod moves up to its top-level window.
3. The pointer is within the top 100 pixels of that window. The 100 is scaled for the window's DPI.
4. The window answers a hit test that says the point is its caption. The mod sends WM_NCHITTEST and waits at most 40 milliseconds. If the app is hung, the mod gives up on that scroll.

A roll-up doesn't resize the window. The mod sets a window region with SetWindowRgn that's as wide as the window and as tall as the title bar. It works out that height from the caption height for the window's DPI, plus the gap between the window's outer edge and its visible frame as the desktop window manager reports it, plus one pixel. The app keeps its real size, so apps with a minimum size still work. The mod keeps a list of the windows it rolled up, and it drops entries for windows that have closed.

When the mod acts on a scroll, it swallows that scroll so the app doesn't see it. When it doesn't act, it passes the scroll on.

When you disable or remove the mod, its thread removes the mouse hook. Then it clears the region on every window it rolled up, so all of them come back.

## Limits and known gaps

- Maximized, minimized and full screen windows are skipped. A window counts as full screen when it covers its whole monitor.
- Only windows with a standard caption style are rolled up. Child windows and tool windows are skipped.
- A window that already has a region of its own, for example one with a custom shape, is skipped.
- The mod only acts where the app reports the point as caption. The title bar buttons report something else, so scrolling on them does nothing. An app that draws its own title bar takes part only if it reports caption for that spot.
- The mod's own notes say windows that run as administrator can't be changed from Explorer, so they're skipped.
- A scroll down on a window the mod didn't roll up does nothing to it and goes to the app.
- If you turn Roll up on title bar scroll off, windows that are already rolled up stay that way. While it's off, you can't scroll them back down either.
- Horizontal scrolling isn't handled.

## What's tested and what isn't

The repo's status note says the mods were tested on Windows 11 build 26300 with a single monitor. The roll-up was checked on plain test windows. Scroll up on the title bar rolled the window up, the frame outline stayed and the content was hidden. Scroll down brought it back.

The repo doesn't record a test on real apps, on more than one monitor, with Reverse or a modifier key, or on Windows 10. Treat those as untested.

## Troubleshooting

A scroll on the title bar does nothing. Check that the window isn't maximized and doesn't fill the whole screen. Move the pointer off the title bar buttons. If you set a modifier key, hold it while you scroll. If none of that helps, look in the Windhawk log for Could not install the mouse hook. That line means the mod couldn't start its hook.

It works in one app and not in another. The second app probably draws its own title bar and doesn't report that spot as caption, or it runs as administrator, or it uses a window region already. The mod skips all three.

A window stays rolled up and won't come back. You may have turned the roll-up off, or changed the modifier, after you rolled it up. Turn it back on, or hold the new modifier, and scroll down on the title bar. You can also disable the mod in Windhawk. That brings every rolled-up window back.

## Uninstall

Open Windhawk, find the mod, and disable or delete it. From a shell that runs as administrator, you can do the same:

```
windhawk-cli.exe mod disable local@titlebar-scroll-rollup --yes
windhawk-cli.exe mod remove local@titlebar-scroll-rollup --yes
```

Every window the mod rolled up comes back when it unloads.

## Use at your own risk

This mod comes with no warranty of any kind, and you use it at your own risk. The author isn't liable for any damage, data loss, crash or other harm it causes. It runs inside Windows apps, so test it on your own machine before you rely on it. The full terms are in the [LICENSE](../LICENSE) file.
