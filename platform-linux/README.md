# Linux Platform — Build & Development Notes

## Prerequisites

Install the required packages before building:

```sh
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install \
    build-essential \
    cmake \
    libx11-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    portaudio19-dev       # required only for -DAUDIO=portaudio
```

CMake ≥ 3.25.1 is required. If your distro ships an older version, install it from
[cmake.org](https://cmake.org/download/) or via `pip install cmake`.

The **Audiere** audio backend (`-DAUDIO=audiere`) is bundled in
`third-party/audiere-1.9.4/` and does not require a system package.

---

## Quick Start

Clone the repository and choose a build configuration:

```sh
git clone git@github.com:michelmorais/mini-mbm.git mini-mbm
cd mini-mbm
```

### Minimal build (C++ only, no Lua)

```sh
mkdir -p build/linux_debug && cd build/linux_debug
cmake ../.. -DPLAT=Linux -DCMAKE_BUILD_TYPE=Debug -DAUDIO=audiere
make -j$(nproc)
```

### Full-featured build (Lua 5.4 + all plugins + editors)

```sh
mkdir -p build/linux_debug && cd build/linux_debug
cmake ../.. \
    -DPLAT=Linux \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=audiere \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE
make -j$(nproc)
```

`-DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE` generates `compile_commands.json` at the
build root, which enables accurate code completion in `clangd`-powered editors
(VS Code with the C/C++ or clangd extension, Neovim, etc.).

### Release build

```sh
mkdir -p build/linux_release && cd build/linux_release
cmake ../.. \
    -DPLAT=Linux \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=audiere \
    -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Output Locations

| Artifact | Path |
|---|---|
| Debug binary | `bin/debug/linux_x86/mini-mbm` |
| Release binary | `bin/release/linux_x86/mini-mbm` |
| Debug libraries | `libs/debug/linux_x86/` |
| Release libraries | `libs/release/linux_x86/` |

Run the engine from the **repo root** so it can find editor scripts, shaders, and
assets at their default relative paths:

```sh
# Launch the launcher dialog (lists all built-in editors):
./bin/debug/linux_x86/mini-mbm

# Launch with a specific Lua script:
./bin/debug/linux_x86/mini-mbm --scene path/to/my_game.lua

# Launch a built-in editor directly:
./bin/debug/linux_x86/mini-mbm --scene editor/sprite_maker.lua
./bin/debug/linux_x86/mini-mbm --scene editor/scene_editor2d.lua
```

---

## Audio Backends

| Backend | CMake flag | Notes |
|---|---|---|
| **Audiere** | `-DAUDIO=audiere` | Bundled in `third-party/audiere-1.9.4/`. Supports OGG, MP3, WAV, FLAC, and tracker formats. Recommended default on Linux. |
| **PortAudio** | `-DAUDIO=portaudio` | Bundled in `third-party/portaudio/`. Requires `portaudio19-dev`. Lower latency for WAV playback. |
| **None** | `-DAUDIO=none` | Silent build — no audio dependency required. |

---

## Render Backend

On Linux the engine uses **OpenGL ES 2.0** via EGL + X11. This is selected
automatically when `-DPLAT=Linux` is set — no extra CMake flag is needed.

The platform entry point is `platform-linux/main.cpp` (C++ mode) or
`platform-linux/main-lua.cpp` (Lua mode).

---

## Switching from GCC to Clang

```sh
# Install Clang
sudo apt-get install clang

# Update system alternatives
sudo update-alternatives --set cc  /usr/bin/clang
sudo update-alternatives --set c++ /usr/bin/clang++

# Rebuild with a fresh build directory
mkdir -p build/linux_clang && cd build/linux_clang
cmake ../.. -DPLAT=Linux -DUSE_ALL=1 -DAUDIO=audiere -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Or pass the compiler directly without touching system alternatives:

```sh
cmake ../.. \
    -DPLAT=Linux \
    -DUSE_ALL=1 \
    -DAUDIO=audiere \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug
```

---

## Static Analysis

A `.clang-tidy.sh` helper script is provided in this directory. It runs
`clang-tidy` over the Linux platform sources using the `compile_commands.json`
produced by a CMake configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE`.

```sh
# From this directory (after a cmake configure with EXPORT_COMPILE_COMMANDS):
cd platform-linux
./clang-tidy.sh
```

---

## Game Delivery — AppDir and AppImage

The Linux build supports the same per-game packaging workflow as iOS and Android.
Pass three CMake flags from **outside** the engine repo and the build produces a
portable **AppDir** folder automatically after `make`. An optional `make appimage`
step wraps it into a single self-contained `.AppImage` file.

### CMake delivery flags

| Flag | Required? | Description |
|---|---|---|
| `-DGAME_ASSETS_DIR=/path/to/assets` | **Yes** (activates delivery) | Absolute path to your game's assets folder. Must contain `main.lua`. |
| `-DGAME_NAME="My Game"` | No (default: `mini-mbm`) | Display name — sets the window title and names the output files. |
| `-DGAME_ICON_PNG=/path/to/icon.png` | No | Any-size PNG. Copied as-is and renamed to match the desktop entry. |

> **Use absolute paths.** CMake does not expand `~` inside double-quoted `-D` values.
> Use `$HOME` or the full path (`/home/michel/…`) instead.

### Building outside the engine repo

The build directory can live anywhere — it does not have to be inside `mini-mbm/`.
This mirrors the iOS workflow: keep the engine in one place and generate a per-game
output directory beside the game repo.

```
~/mini-mbm/                  ← engine repo (shared, never edited per game)
~/tower-defense/             ← game repo
    assets/                  ← game assets (must contain main.lua)
    icon.png
~/tower-defense-linux/       ← generated build dir (add to .gitignore)
    Tower_Defense_Monster.AppDir/
    Tower_Defense_Monster-x86_64.AppImage   ← after make appimage
```

```sh
mkdir -p ~/tower-defense-linux && cd ~/tower-defense-linux
cmake ~/mini-mbm \
    -DPLAT=Linux \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=audiere \
    -DCMAKE_BUILD_TYPE=Release \
    -DGAME_NAME="Tower Defense Monster" \
    -DGAME_ASSETS_DIR=/home/michel/tower-defense/assets \
    -DGAME_ICON_PNG=/home/michel/tower-defense/propaganda/1024x1024-icon.png
make -j$(nproc)      # assembles AppDir automatically after linking
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
make appimage        # wraps AppDir into .AppImage (appimagetool required)
```

### What gets generated

**At `cmake` configure time** (before any compilation):

| Artifact | Location | Notes |
|---|---|---|
| `AppRun` | `AppDir/` | Executable launcher script — sets `LD_LIBRARY_PATH`, launches `mini-mbm` with `--name` and `--scene assets/main.lua` |
| `Tower_Defense_Monster.desktop` | `AppDir/` | XDG desktop entry required by the AppImage spec |
| `Tower_Defense_Monster.png` | `AppDir/` | Copy of `GAME_ICON_PNG` renamed to match the `Icon=` entry (if provided) |

**After `make`** (POST_BUILD):

| Artifact | Location | Notes |
|---|---|---|
| `mini-mbm` binary | `AppDir/usr/bin/` | The compiled engine executable |
| `*.so` plugins | `AppDir/usr/lib/` | All shared-library plugins (box2d, ImGui, bullet, etc.) |
| Game assets | `AppDir/assets/` | Full copy of `GAME_ASSETS_DIR` |

### Running without AppImage

The AppDir is a standalone, runnable directory without any additional tooling:

```sh
./Tower_Defense_Monster.AppDir/AppRun
# or pass extra engine flags:
./Tower_Defense_Monster.AppDir/AppRun --width 1280 --height 720
```

### Writable save directory

The AppImage filesystem is **read-only** — any file the game tries to create inside
`/tmp/.mount_*/` will fail. `AppRun` handles this automatically: it creates a
persistent writable directory under `$HOME/.local/share/<GameName>/` and exports it
as the `GAME_SAVE_DIR` environment variable before launching the engine.

```
~/.local/share/Tower_Defense_Monster/   ← writable, survives AppImage updates
    tower-defense.data
    ...
```

In your Lua save/load code, read the path from the environment variable instead of
using a hardcoded or relative path:

```lua
-- Use the writable save dir injected by AppRun.
-- Falls back to "" (current dir) when running outside an AppImage during development.
local save_dir = os.getenv("GAME_SAVE_DIR")
save_dir = save_dir and (save_dir .. "/") or ""

local save_file = save_dir .. "tower-defense.data"
```

Use `save_file` wherever you read or write persistent game data. This works
transparently in all three contexts:

| Context | `GAME_SAVE_DIR` value | Save file location |
|---|---|---|
| AppImage (distributed) | `~/.local/share/Tower_Defense_Monster` | `~/.local/share/Tower_Defense_Monster/tower-defense.data` |
| AppDir (direct run) | `~/.local/share/Tower_Defense_Monster` | same |
| Development (engine repo) | _(not set)_ | `tower-defense.data` next to the script |

After changing `AppRun` (by re-running cmake) you must rebuild and repackage:

```sh
cd ~/tower-defense-linux
cmake ~/mini-mbm [same flags as before]
make -j$(nproc)
make appimage
```

### Bundled audio libraries (Audiere)

Audiere discovers its audio backend at runtime via `dlopen()`. On modern Linux,
`/dev/dsp` (OSS) is gone — Audiere falls back to ALSA (`libasound.so.2`).

`libasound` is **not bundled** in the AppImage. Bundling it would break audio on
Ubuntu and other PipeWire-based distros: ALSA on those systems routes through
`libasound_module_pcm_pipewire.so` and config files in `/usr/share/alsa/` that live
on the host, not inside the AppImage. The system `libasound2` (a standard package
present on all Debian/Ubuntu machines) handles this correctly on its own.

If audio fails completely (e.g. no ALSA device at all), the engine logs
`audio disabled, game will run silently` and continues running without crashing.

**What gets bundled in `AppDir/usr/lib/`:**

| Library | Purpose |
|---|---|
| `libaudiere-1.10.1.so` | Audiere audio engine |
| `libcore_mbm.so` | Engine core |
| `liblua-5.4.1.so` | Lua scripting |
| `plugin_helper.so`, `box2d.so`, `ImGui.so`, … | Gameplay plugins |

### Building the AppImage

`appimagetool` is searched in two places:

1. `$PATH` (system-wide install)
2. `<build_dir>/appimagetool-x86_64.AppImage` (local download — no install needed)

If neither is found, `make appimage` prints the download URL and exits cleanly —
the AppDir is still fully usable.

```sh
# Option A — system install
sudo apt-get install appimagetool    # if available in your distro
make appimage

# Option B — local download (no root required)
cd ~/tower-defense-linux
wget -q https://github.com/AppImage/AppImageKit/releases/latest/download/appimagetool-x86_64.AppImage
make appimage
# Output: Tower_Defense_Monster-x86_64.AppImage
```

### Inspecting AppImage contents

Extract the SquashFS without running the app, then browse the result:

```sh
cd /tmp && rm -rf ai_check && mkdir ai_check && cd ai_check
/path/to/Tower_Defense_Monster-x86_64.AppImage --appimage-extract 2>/dev/null

# Overall structure (excluding the large assets/ subtree)
find squashfs-root -not -path "*/assets/*" | sort

# Bundled shared libraries
ls -lh squashfs-root/usr/lib/

# Game assets
ls squashfs-root/assets/

# Launcher script
cat squashfs-root/AppRun
```

Expected layout:

```
squashfs-root/
├── AppRun                          ← launcher script (chmod +x)
├── Tower_Defense_Monster.desktop   ← XDG desktop entry
├── Tower_Defense_Monster.png       ← app icon
├── .DirIcon -> Tower_Defense_Monster.png
├── assets/                         ← full copy of GAME_ASSETS_DIR
│   ├── main.lua
│   └── ...
└── usr/
    ├── bin/
    │   └── mini-mbm                ← engine binary
    └── lib/
        ├── libaudiere-1.10.1.so
        ├── libcore_mbm.so
        ├── liblua-5.4.1.so
        ├── plugin_helper.so
        ├── box2d.so
        ├── ImGui.so
        └── ...
```

### CMake status messages

When delivery is configured, cmake prints:

```
-- Linux game name   : Tower Defense Monster
-- Linux assets dir  : /home/michel/tower-defense/assets
-- Linux AppDir      : /home/michel/tower-defense-linux/Tower_Defense_Monster.AppDir
-- After 'make': run  /home/michel/tower-defense-linux/Tower_Defense_Monster.AppDir/AppRun
-- AppImage (opt.)   : cd /home/michel/tower-defense-linux && make appimage
```

---

## Platform Source Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ mode entry point — instantiates `GAME`, calls `initGraphics()` + `onLoop()` |
| `main-lua.cpp` | Lua mode entry point — instantiates `LUA_MANAGER`, loads a `.lua` scene |
| `my-scene.cpp` / `my-scene.h` | Example `SCENE` subclass — starting point for your own game |
| `.clang-tidy.sh` | Helper script to run `clang-tidy` with project compile commands |
