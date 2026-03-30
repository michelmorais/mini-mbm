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

## Platform Source Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ mode entry point — instantiates `GAME`, calls `initGraphics()` + `loop()` |
| `main-lua.cpp` | Lua mode entry point — instantiates `LUA_MANAGER`, loads a `.lua` scene |
| `my-scene.cpp` / `my-scene.h` | Example `SCENE` subclass — starting point for your own game |
| `.clang-tidy.sh` | Helper script to run `clang-tidy` with project compile commands |
