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
mkdir -p build/macos_debug && cd build/macos_debug
cmake ../.. -DPLAT=MacOs -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.logicalcpu)
```

### Full-featured build (Lua 5.4 + all plugins + editors)

```sh
mkdir -p build/macos_debug && cd build/macos_debug
cmake ../.. \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.logicalcpu)
```

Equivalently via `cmake --build`:

```sh
cmake -B build/macos_debug \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build/macos_debug -j$(sysctl -n hw.logicalcpu)
```

### Release build

```sh
mkdir -p build/macos_release && cd build/macos_release
cmake ../.. \
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

## Platform Source Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ mode entry point — instantiates `GAME`, calls `initGraphics()` + `onLoop()` |
| `main-lua.cpp` | Lua mode entry point — instantiates `LUA_MANAGER`, loads a `.lua` scene |
| `my-scene.cpp` / `my-scene.h` | Example `SCENE` subclass — starting point for your own game |
