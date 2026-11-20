# Local mini-mbm engine configuration

Copy this file to `ENGINE.local.md` and replace the path for the current
developer and operating system. `ENGINE.local.md` is ignored by Git.

## Linux

```text
Engine binary directory: /home/<user>/mini-mbm/bin/debug/linux_x86
```

## macOS

```text
Engine binary directory: /Users/<user>/mini-mbm/bin/debug/arm64
```

## Windows

```text
Engine binary directory: C:\path\to\mini-mbm\bin\debug\windows_x86
```

Use the directory containing `mini-mbm` (or `mini-mbm.exe` on Windows). The
standalone `lua-5.4.1.exe` is optional and is only for pure Lua checks; game
scripts must be tested through the engine.
