# windhawk-mods

Custom [Windhawk](https://windhawk.net) mods for Windows 11.

## Mods
- `mods/offscreen-window-rescue.wh.cpp`: moves installer windows back onto the screen when they open outside the visible area.

## Install
1. Open Windhawk and create a new mod.
2. Paste the file contents and compile.

Or from an elevated shell:

```
windhawk-cli.exe mod install --file mods/offscreen-window-rescue.wh.cpp
```

## Status
Tested on Windows 11 build 26300 with a single monitor. A window forced to x=-3000 inside a test process moved back to the center. It has not been tested on a real MSI installer yet.

## License
MIT
