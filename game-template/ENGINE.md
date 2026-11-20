# mini-mbm engine runtime

This file is shared by all developers and operating systems. Absolute engine
paths are intentionally kept out of this file because they are local to each
machine.

## Local engine path

If `ENGINE.local.md` exists, read it before running tests. It contains the
engine binary directory configured for the current developer and is ignored by
Git. If it is absent, copy `ENGINE.local.example.md` to `ENGINE.local.md` and
replace its placeholder with the local engine build directory.

The template copier creates `ENGINE.local.md` automatically for the machine
where the project is initialized. To configure a different machine, create its
own local file; do not append that machine's private path to this shared file.

The expected directory contains the Lua-enabled `mini-mbm` executable. Typical
locations are:

| Operating system | Example build directory |
| --- | --- |
| Linux | `/home/<user>/mini-mbm/bin/debug/linux_x86` |
| macOS | `/Users/<user>/mini-mbm/bin/debug/arm64` |
| Windows | `C:\path\to\mini-mbm\bin\debug\windows_x86` |

## Which runtime to use

Do not look for a system `lua`, `luajit`, or `lua5.4` when testing game code.
The engine embeds Lua and registers the `mbm` namespace and renderable tables.
Game/API checks must run through the matching `mini-mbm` binary.

The optional standalone `lua-5.4.1.exe` bundled beside the engine is suitable
only for pure Lua modules. It is not a substitute for testing scripts that use
`mbm`, `sprite`, `mesh`, `font`, plugins, or other engine-registered tables.

## Recommended checks

From the game project root, set `ENGINE_BIN` to the path in
`ENGINE.local.md`:

```sh
ENGINE_BIN="/path/to/mini-mbm/bin/<platform-build>"

# Fast structural check.
git diff --check

# Confirm that the configured engine exists.
test -x "$ENGINE_BIN/mini-mbm"

# Interactive run of the game's entry scene. Press Esc to leave it.
"$ENGINE_BIN/mini-mbm" --scene main.lua \
    --disable_select_monitor --nosplash -w 640 -h 480
```

On Windows, use the same path from `ENGINE.local.md` in PowerShell:

```powershell
$EngineBin = 'C:\path\to\mini-mbm\bin\debug\windows_x86'
& "$EngineBin\mini-mbm.exe" --scene main.lua --disable_select_monitor --nosplash -w 640 -h 480
```

For an unattended Lua smoke test, use a temporary scene that calls
`mbm.quit()` after a short `mbm.getTimeRun()` deadline, then launch it with the
same `mini-mbm` command and wrap it in `timeout -s KILL` as a hard backstop on
Unix. Always pass `--disable_select_monitor`; otherwise the monitor/script
picker can block an automated run. Add `mbm.addPath(...)` before loading
project assets.
