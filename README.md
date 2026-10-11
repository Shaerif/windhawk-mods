# windhawk-mods

Custom [Windhawk](https://windhawk.net) mods for Windows 11.

## Mods
- `mods/offscreen-window-rescue.wh.cpp`: moves any normal window back onto the screen when it first opens outside the visible area. It runs in all apps except File Explorer and core system processes. It checks each window once, so you can still drag windows partly off the edge.

## Install
1. Open Windhawk and create a new mod.
2. Paste the file contents and compile.

Or from an elevated shell:

```
windhawk-cli.exe mod install --file mods/offscreen-window-rescue.wh.cpp
```

## Status
Tested on Windows 11 build 26300 with a single monitor. A window opened at x=-3000 stayed there with the mod off and moved to the center with the mod on. It has not been tested on a real MSI installer yet.

## License
MIT
