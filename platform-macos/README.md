# macOS Platform — Build & Development Notes

## Prerequisites

### Xcode Command Line Tools (required)

```sh
xcode-select --install
```

This installs `clang`, `clang++`, `make`, and the macOS SDK. If the full Xcode
IDE is already installed, the CLI tools are included automatically.

### CMake ≥ 3.25.1

```sh
brew install cmake     # via Homebrew (recommended)
# or: pip install cmake
```

[Homebrew](https://brew.sh) is the easiest way to install build tools on macOS.

---

## Quick Start

Clone the repository and choose a build configuration:

```sh
git clone git@github.com:michelmorais/mini-mbm.git mini-mbm
cd mini-mbm
```

### Minimal build (C++ only, no Lua)

```sh
mkdir -p build && cd build
cmake .. -DPLAT=MacOs -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.logicalcpu)
```

### Full-featured build (Lua 5.4 + all plugins + editors)

```sh
mkdir -p build && cd build
cmake .. \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.logicalcpu)
```

Equivalently via `cmake --build`:

```sh
cmake -B build \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

### Release build

```sh
mkdir -p build && cd build
cmake .. \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.logicalcpu)
```

---

## Render Backend — Metal

The **Metal** backend is selected automatically when `-DPLAT=MacOs` is set
(`USE_METAL=1` is set internally by CMake). No extra flag is required.

The macOS Metal implementation uses an `NSWindow` + `CAMetalLayer` setup.

> The OpenGL ES backend is **not available** on macOS — Metal is the only render
> backend used on this platform.

---

## Audio Backend — AVFoundation

**AVFoundation** is the default audio backend on macOS and is selected
automatically (no `-DAUDIO=` flag needed).

| Format | Notes |
|---|---|
| WAV | Recommended for sound effects — zero decode latency |
| AIFF / CAF / AU | Native Apple formats, decoded by AVFoundation |
| MP3 | Hardware-decoded |
| AAC / M4A | Recommended for long background music |
| OGG Vorbis | Decoded via the bundled `stb_vorbis` (no extra dependency needed) |
| OGG Opus | Not supported — engine automatically retries with a `.wav` fallback |

> **OGG Opus note:** If an `.ogg` file encoded with the Opus codec is requested,
> the engine detects this (via the `OpusHead` stream header) and retries with a
> `.wav` file of the same base name in the same directory. Keep both `.ogg` and
> `.wav` versions of your sounds to stay compatible with both Android and macOS.

---

## Launching the Editor Tools

After a build with `-DUSE_ALL=1`, launch the engine without arguments to open the
**launcher dialog** listing all built-in Lua editors:

```sh
./bin/debug/macos/mini-mbm
```

Or launch a specific editor directly:

```sh
./bin/debug/macos/mini-mbm --scene editor/sprite_maker.lua
./bin/debug/macos/mini-mbm --scene editor/scene_editor2d.lua
./bin/debug/macos/mini-mbm --scene editor/font_maker.lua
```

---

## Separate Game Repository

The build directory can live anywhere on disk — it does not have to be inside the
engine repo. This is useful when each game lives in its own repository:

```sh
mkdir -p ~/my-game-macos && cd ~/my-game-macos
cmake ~/mini-mbm \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.logicalcpu)
```

---

## Game Delivery — .app Bundle and .dmg

The macOS build supports per-game packaging via the same `-DGAME_ASSETS_DIR`
workflow as Linux (AppImage) and Windows (GameDir). The build produces a
self-contained **`.app` bundle** automatically after `make`. An optional
`make macdmg` step wraps it into a distributable **`.dmg`** disk image.

### How it works

Assets are packed into an AES-128-CBC encrypted SQLite archive (`.asset` file)
by the bundled `distribution` tool. At launch, the engine extracts the archive
to a temporary directory, loads `main.lua`, then cleans up on exit. The
`distribution.dylib` runtime library is bundled inside `Contents/Frameworks/`.

### CMake delivery flags

| Flag | Required? | Description |
|---|---|---|
| `-DGAME_ASSETS_DIR=/path/to/assets` | **Yes** (activates delivery) | Absolute path to your game's assets folder. Must contain `main.lua`. |
| `-DGAME_NAME="My Game"` | No (default: `mini-mbm`) | Display name — sets the window title and `.app` name. |
| `-DGAME_ASSETS_PASSWORD=secret` | No | If set, assets are AES-128-CBC encrypted (PBKDF2-HMAC-SHA256, 100 000 iterations). Omit for unencrypted packing. |
| `-DGAME_ICON_PNG=/path/to/icon.png` | No | Any-size PNG. Converted to `.icns` via `sips` + `iconutil` automatically if those tools are available (they are on any macOS with Xcode CLI tools). |

> **Use absolute paths.** CMake does not expand `~` inside double-quoted `-D` values.
> Use `$HOME` or the full path (`/Users/yourname/…`) instead.

### Building outside the engine repo

```sh
mkdir -p ~/tower-defense-macos && cd ~/tower-defense-macos
cmake ~/mini-mbm \
    -DPLAT=MacOs \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DCMAKE_BUILD_TYPE=Release \
    -DGAME_NAME="Tower Defense Monster" \
    -DGAME_ASSETS_DIR=/Users/michel/tower-defense/assets \
    -DGAME_ASSETS_PASSWORD=mysecret \
    -DGAME_ICON_PNG=/Users/michel/tower-defense/propaganda/1024x1024-icon.png
make -j$(sysctl -n hw.logicalcpu)   # .app assembled automatically after linking
make macdmg                          # wraps .app into a .dmg (requires hdiutil)
```

### What gets generated

**At `cmake` configure time:**

| Artifact | Location | Notes |
|---|---|---|
| `Info.plist` | `Tower_Defense_Monster.app/Contents/` | Bundle metadata (name, identifier, executable) |
| `Tower_Defense_Monster.icns` | `Contents/Resources/` | Generated from `GAME_ICON_PNG` via `sips`/`iconutil` (if PNG supplied) |
| `make_macdmg.sh` | `<build_dir>/` | Shell script used by `make macdmg` |

**After `make` (POST_BUILD):**

| Artifact | Location | Notes |
|---|---|---|
| `Tower_Defense_Monster` binary | `Contents/MacOS/` | Renamed engine executable |
| `distribution.dylib` | `Contents/Frameworks/` | Asset library — extracts `.asset` at launch |
| `Tower_Defense_Monster.asset` | `Contents/Resources/assets/` | Packed (and optionally encrypted) archive of `GAME_ASSETS_DIR` |

### .app bundle layout

```
Tower_Defense_Monster.app/
└── Contents/
    ├── Info.plist
    ├── MacOS/
    │   └── Tower_Defense_Monster       ← engine binary
    ├── Frameworks/
    │   └── distribution.dylib          ← asset library
    └── Resources/
        ├── Tower_Defense_Monster.icns  ← app icon (if GAME_ICON_PNG supplied)
        └── assets/
            └── Tower_Defense_Monster.asset   ← packed game archive
```

### Make targets

| Target | Command | Description |
|---|---|---|
| _(default)_ | `make` | Compiles the engine and assembles the `.app` bundle |
| `macapp` | `make macapp` | Synonym for the default target — rebuilds and reassembles the `.app` |
| `macdmg` | `make macdmg` | Wraps the `.app` into a compressed `.dmg` using `hdiutil` (requires macOS) |

### CMake status messages

When delivery is configured, CMake prints:

```
-- macOS game name   : Tower Defense Monster
-- macOS assets dir  : /Users/michel/tower-defense/assets
-- macOS .app bundle : /Users/michel/tower-defense-macos/Tower_Defense_Monster.app
```

---

## Platform Source Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ mode entry point — instantiates `GAME`, calls `initGraphics()` + `onLoop()` |
| `main-lua.cpp` | Lua mode entry point — instantiates `LUA_MANAGER`, loads a `.lua` scene |
| `main-lua-delivery.cpp` | Delivery entry point — extracts `.asset`, then calls `mbm::onLoop()` |
| `my-scene.cpp` / `my-scene.h` | Example `SCENE` subclass — starting point for your own game |
