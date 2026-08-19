# Agent Instructions — mini-mbm

## What is mini-mbm

Mini MBM is a **lightweight, cross-platform 2D/3D game engine** written in C++17, licensed under MIT. It targets **Windows, Linux, macOS, Android, and iOS** and exposes an optional **Lua 5.4 scripting layer**. Primary focus is 2D game development; 3D is supported. Shipped games: *Tower Defense Monster* and *Spaceship Combat* (both on Google Play).

Key features:
- Multiple render backends: **OpenGL ES** (Linux/Android/macOS fallback), **DirectX 9** (Windows), **Metal** (macOS/iOS)
- Audio backends: **PortAudio** (Linux/Windows), **AVFoundation** (macOS/iOS), **OpenSL** (Android)
- Asset types: sprite (`.spt`), mesh (`.mbm`), texture, GIF, background, font, particle, tile map, shape mesh, line mesh, render-to-texture, HMD/VR
- Plugin architecture — optional features compiled as shared libraries loaded at runtime
- Lua 5.4 scripting with the `mbm` namespace; ImGui-based editor tools written in Lua

---

## Agent Context Files

- `AGENTS.md` is the shared Codex/ChatGPT project context.
- `CLAUDE.md` is a symbolic link to `AGENTS.md`, so Claude Code receives the same instructions.
- `.github/copilot-instructions.md` is the Copilot copy of this file and should stay synchronized with `AGENTS.md`.
- Keep critical invariants and project-wide rules in this file; move long task workflows into repo-local skills when they are Codex-specific.

## Codex Skills

Repo-local Codex skills live in `.agents/skills/`.

- Use `engine-testing` before running `testLib`, `mini-mbm`, Lua scripts, or render/backend verification.
- Use `lua-game` when scaffolding or editing standalone Lua games.
- Use `new-plugin` when creating Lua/C++ plugins or plugin-helper-backed Lua bindings.
- Use `new-editor-tool` when adding ImGui/Lua editor tools under `editor/`.
- Use `new-platform-port` when adding a new `PLAT=` target or platform directory.
- Use `doc-drift-check` when touching docs tied to implementation behavior, especially `docs/lua-api.md`, `docs/mesh-v11-format.md`, `docs/light.md`, and `docs/bones-armatures-and-fbx.md`.
- Use `grill-me` when asked to stress-test a plan or design before implementation.

---

## Build System

Minimum CMake version: **3.25.1**

### Build command pattern
```sh
mkdir -p build/<dir> && cd build/<dir>
cmake ../.. -DPLAT=<platform> [options] -DCMAKE_BUILD_TYPE=Debug|Release
make -j$(nproc)           # Linux / macOS
mingw32-make -j...        # Windows MinGW
```

### Required flag
| Flag | Values | Notes |
|---|---|---|
| `-DPLAT` | `Linux` `Windows` `MacOs` `Android` `iOS` | **Always required** |

### Important optional flags
| Flag | Effect |
|---|---|
| `-DUSE_ALL=1` | Enable common full-feature set (Lua, VR, Box2D, LiquidFun, ImGui, lSQLite3, Tiled); does not enable Steam or Bullet3D |
| `-DUSE_LUA=1` | Embed Lua 5.4 scripting |
| `-DUSE_VR=1` | Enable VR class (default on Linux/Windows, off on Android) |
| `-DAUDIO=portaudio\|avfoundation\|opensl\|none` | Audio backend. `portaudio` is the default for Linux/Windows; `avfoundation` for macOS/iOS; `opensl` for Android |
| `-DUSE_BOX2D=1` | Box2D 2.4.1 physics plugin |
| `-DUSE_BOX2D_LIQUID_FUN=1` | LiquidFun 2.3.1 fluid physics plugin |
| `-DUSE_BULLET3D=1` | Bullet 2.84 3D physics plugin |
| `-DUSE_IMGUI=1` | Dear ImGui plugin (required for editor tools) |
| `-DUSE_LSQLITE3=1` | SQLite3 Lua bindings |
| `-DUSE_TILEMAP=1` | Optional `plugins/tiled/` Lua plugin (`tilemap` library) for tilemap editor tooling; core `TILE` render type is built independently |
| `-DUSE_STEAM=1` | Steamworks Lua plugin; requires `-DSTEAMWORKS_SDK_PATH=/path/to/steamworks_sdk` |

### Output directories (set by root CMakeLists.txt)
| Build type | Binaries | Libraries |
|---|---|---|
| Debug | `bin/debug/` | `libs/debug/` |
| Release | `bin/release/` | `libs/release/` |

### Recommended build directories
- `build/linux_debug/`, `build/linux_release/`
- `build/macos_debug/`, `build/macos_release/`
- `build/mingw_debug/`, `build/mingw_release/`

### Windows Visual Studio
Pre-configured solution: `platform-msvs/mini-mbm.sln` (Visual Studio 2022 Community). No CMake step needed.

---

## Directory Layout

```
mini-mbm/
├── CMakeLists.txt            Root CMake — sets PLAT, output dirs, delegates to src/
├── src/                      Engine source code
│   ├── CMakeLists.txt        Main build orchestrator — selects platform main, links all libs
│   ├── core_mbm/             Core engine library (device, scene, input, camera, animation, audio)
│   ├── render/               Renderable type implementations (sprite, mesh, font, particle, tile…)
│   ├── lua-wrap/             Lua bindings for all engine types (mbm namespace)
│   ├── mini-mbm-lib/         Thin launcher library (mini-mbm-lib.cpp + platform variants)
│   ├── platform/             Platform-specific backend (OpenGL ES, DirectX, Metal, EGL)
│   └── test-lib/             Interactive test executable with all render types
├── include/                  Public headers (also consumed by plugins)
│   ├── core_mbm/             Core engine headers (scene.h, device.h, renderizable.h, …)
│   ├── render/               Renderable type headers (sprite.h, mesh.h, font.h, tile.h, …)
│   ├── lua-interface/        Lua binding headers (lua-wrap/, render-table/)
│   ├── platform/             Platform mismatch / abstraction headers
│   ├── miniz-wrap/           Miniz compression wrapper
│   ├── static-resource/      Embedded binary resources
│   └── version/              version.h
├── platform-linux/           Linux entry point: main.cpp, main-lua.cpp, my-scene.h/cpp
├── platform-macos/           macOS entry point: main.cpp, main-lua.cpp, my-scene.h/mm
├── platform-android/         Android entry point: main.cpp (JNI), my-scene.h/cpp
├── platform-ios/             iOS entry point: main-lua.mm, AppDelegate, MBMMetalView, my-scene.h/mm
├── platform-msvs/            Windows Visual Studio solution + project files
├── plugins/                  Optional plugin shared libraries
│   ├── plugin-helper/        Shared Lua helper utilities used by all plugins
│   ├── box2d/                Box2D 2.4.1 Lua bindings
│   ├── box2d-liquid-fun-lua/ LiquidFun 2.3.1 Lua bindings
│   ├── bullet3d/             Bullet 2.84 Lua bindings
│   ├── imGui/                Dear ImGui plugin (powers all editor tools)
│   └── tiled/                Tiled-map plugin with tile_editor
├── editor/                   Lua-based editor tools (scene_editor2d, sprite_maker, shader_editor…)
├── modules/                  Dynamically-loadable Lua modules (tiny_obj_loader, …)
├── third-party/              Bundled third-party libraries (lua-5.4.1, bullet, box2d, stb…)
├── bin/                      Built executables (debug/ or release/ subdirs)
└── libs/                     Built static libraries (debug/ or release/ subdirs)
```

---

## Scene Pattern (C++ mode)

Every C++ game creates two classes in `my-scene.h` / `my-scene.cpp`:

### `MY_SCENE : public mbm::SCENE`
Implement these virtual methods:
```cpp
void onInitScene();           // load assets, set up camera
void onLoop();                // per-frame logic (called every loop)
void startLoading();          // called before async load
void endLoading();            // called when loading completes
void onResizeWindow();        // handle window resize
void onFinalizeScene();       // cleanup before scene ends

// Input
void onTouchDown(int key, float x, float y);
void onTouchUp(int key, float x, float y);
void onTouchMove(int key, float x, float y);
void onTouchZoom(float zoom);
void onKeyDown(int key);
void onKeyUp(int key);
void onKeyDownJoystick(int player, int key);
void onKeyUpJoystick(int player, int key);
void onMoveJoystick(int player, float lx, float ly, float rx, float ry);
void onInfoDeviceJoystick(int player, int maxNumberButton,
                          const char* strDeviceName, const char* extraInfo);
```

### `GAME : public mbm::CORE_MANAGER`
```cpp
class GAME : public mbm::CORE_MANAGER {
public:
    MY_SCENE myScene;
    bool existScene(const int idScene) override;
    GAME();
    virtual ~GAME();
};
```

### `main()` pattern
```cpp
int main() {
    GAME game;
    const bool singleLoop = true, doSwapBuffers = false;
    if (game.initGraphics("Window Title"))
        return game.onLoop(singleLoop, doSwapBuffers);
    return -1;
}
```

### Accessing the device / camera
```cpp
mbm::DEVICE* device = mbm::DEVICE::getInstance();
mbm::CAMERA &camera = device->getCamera();
camera.position = mbm::VEC3(0, 280, -900);
camera.focus    = mbm::VEC3(0, 280, 0);
```

### Debug path helper
```cpp
util::addPath(__FILE__); // adds file's directory to asset search paths
```

---

## Plugin Architecture

Plugins are **shared libraries** (`.so` on Linux/Android/macOS, `.dll` on Windows) or **static archives** on iOS.

### Plugin structure
```
plugins/<name>/
    <name>-lua.h        // exports luaopen_<name> and luaopen_lib<name>
    <name>-lua.cpp      // calls mbm::registerClass<Name>(lua)
    <class>-wrap.h/.cpp // C++ wrapper around third-party lib
    CMakeLists.txt
```

### Plugin Lua entry point
Each plugin exports two equivalent C functions (to support both `require "<name>"` and `require "lib<name>"`):
```cpp
extern "C" MY_API int luaopen_<name>(lua_State* lua);
extern "C" MY_API int luaopen_lib<name>(lua_State* lua);
```

### Plugin C++ callback interface (`include/core_mbm/plugin-callback.h`)
Any plugin that needs engine hooks inherits from `PLUGIN`:
```cpp
class PLUGIN {
    virtual void onSubscribe(int width, int height, void* context, void* renderDevice) = 0;
    virtual void onResizeWindow(int width, int height) = 0;
    virtual void onTouchDown(int key, float x, float y) = 0;
    virtual void onTouchUp(int key, float x, float y) = 0;
    virtual void onTouchMove(int key, float x, float y) = 0;
    virtual void onTouchZoom(float zoom) = 0;
    virtual void onKeyDown(int key) = 0;
    virtual void onKeyUp(int key) = 0;
    virtual void onDoubleClick(float x, float y, int key) = 0;
    virtual void onKeyDownJoystick(int player, int key) = 0;
    virtual void onKeyUpJoystick(int player, int key) = 0;
    virtual void onMoveJoystick(int player, float lx, float ly, float rx, float ry) = 0;
    virtual void onInfoDeviceJoystick(int player, int maxNumberButton,
                                      const char* strDeviceName, const char* extraInfo) = 0;
    virtual void onPrepare() = 0;  // once before render loop starts
    virtual void onLoop(float delta) = 0;
    virtual void onRender() = 0;
    virtual void onDestroy() = 0;
};
```

### Plugin-helper library (`plugins/plugin-helper/`)
All plugins link against `plugin-helper`. It provides:
- `lua_check_userType()` / `lua_get_userType_no_throw()` — validate Lua userdata
- `getRenderizableFromRawTable()` — extract `RENDERIZABLE*` from Lua table
- `lua_error_debug()` / `lua_print_line()` — Lua-aware error/logging
- `USER_DATA_RENDER_LUA`, `USER_DATA_SCENE_LUA` — Lua callback ref containers
- Shader helpers: `onLoadNewShaderLua`, `onSetPixelShaderLua`, etc.
- `PLUGIN_HELPER_API` export macro (dllexport/visibility default)
- **Plugin infrastructure helpers** (use these instead of hand-rolling):
  - `plugin_stamp_userdata(lua, &PLUGIN_IDENTIFIER)` — applies `_usertype_plugin` metatable to userdata at stack top; reads/creates the runtime plugin id
  - `plugin_doSubscribe(lua, index_plugin, name)` — calls `mbm.doSubscribe`; cleans stack on success; `luaL_error` on failure
  - `plugin_register_factory(lua, global_name, metatable_name, luaL_Reg*)` — one-liner for the `luaL_newmetatable → luaL_setfuncs → lua_setglobal → lua_settop` factory pattern
  - `plugin_check_userdata(lua, rawi, indexTable, plugin_id, type_name)` — extracts and validates a `PLUGIN_IDENTIFIER`-stamped userdata; `luaL_error` on mismatch

### CMake library naming
`set(CMAKE_SHARED_LIBRARY_PREFIX "")` — plugins have no `lib` prefix.

---

## Core API Reference

### Render types (in `include/render/`)
| Class | Header | Description |
|---|---|---|
| `SPRITE` | `render/sprite.h` | 2D/3D sprite with animation |
| `MESH` | `render/mesh.h` | 3D mesh (`.msh` format) |
| `TEXTURE_VIEW` | `render/texture-view.h` | Plain texture quad |
| `BACKGROUND` | `render/background.h` | Background/foreground mesh or texture |
| `TEXT_DRAW` / `FONT_DRAW` | `render/font.h` | Text rendering |
| `PARTICLE` | `render/particle.h` | Particle system |
| `STEERED_PARTICLE` | `render/steered_particle.h` | Directed particles |
| `TILE` | `render/tile.h` | Tile map with layers (`TILE_LAYER`, `TILE_OBJ`) |
| `LINE_MESH` | `render/line-mesh.h` | Procedural line geometry |
| `SHAPE_MESH` | `render/shape-mesh.h` | Procedural shape geometry |
| `RENDER_2_TEXTURE` | `render/render-2-texture.h` | Render-to-texture target |
| `GIF_VIEW` | `render/gif-view.h` | Animated GIF |
| `HMD` | `render/HMD.h` | VR/HMD rendering |

### Core headers (in `include/core_mbm/`)
| Header | Key types |
|---|---|
| `scene.h` | `SCENE`, `CONTROL_SCENE` |
| `core-manager.h` | `CORE_MANAGER` (main game loop) |
| `device.h` | `DEVICE` singleton (camera, back buffer, ordering) |
| `renderizable.h` | `RENDERIZABLE` base class, `TYPE_CLASS` enum |
| `animation.h` | `ANIMATION_MANAGER`, `ANIMATION`, `EFFECT_SHADER` |
| `shader.h` | `FVF_PROVIDE_BY_ENGINE`, `TYPE_ANIMATION`, `BUFFER_GL` |
| `audio.h` | `AUDIO`, `AUDIO_MANAGER` |
| `physics.h` | `PHYSICS` abstract base, `INFO_PHYSICS` |
| `plugin-callback.h` | `PLUGIN` interface |
| `primitives.h` | `VEC2`, `VEC3`, `MATRIX`, `COLOR`, `CUBE`, `SPHERE`, etc. |
| `camera.h` | `CAMERA` (2D ortho + 3D perspective) |
| `util-interface.h` | `util::addPath()`, logging macros |
| `log-util.h` | `INFO_LOG`, `ERROR_LOG`, `ERROR_AT` macros |
| `dynamic-var.h` | `DYNAMIC_VAR` — engine-level key-value store |
| `texture-manager.h` | `TEXTURE`, `TEXTURE_MANAGER` |
| `mesh-manager.h` | `MESH_MBM`, `MESH_MANAGER` |

### `TYPE_CLASS` enum values
`TYPE_CLASS_MESH`, `TYPE_CLASS_SPRITE`, `TYPE_CLASS_TEXTURE`, `TYPE_CLASS_BACKGROUND`, `TYPE_CLASS_GIF`, `TYPE_CLASS_TEXT`, `TYPE_CLASS_PRIMITIVE`, `TYPE_CLASS_SHAPE_MESH`, `TYPE_CLASS_LINE_MESH`, `TYPE_CLASS_PARTICLE`, `TYPE_CLASS_STEERED_PARTICLE`, `TYPE_CLASS_RENDER_2_TEX`, `TYPE_CLASS_TILE`, `TYPE_CLASS_TILE_OBJ`, `TYPE_CLASS_TILE_LAYER`

---

## Coding Conventions

### Naming
- **Namespace**: `mbm` for all engine code; `util` for utility free functions
- **Classes / structs**: `ALL_CAPS` (e.g., `SCENE`, `DEVICE`, `CORE_MANAGER`, `TEXTURE_VIEW`, `MY_SCENE`)
- **Methods**: `camelCase` (e.g., `initGraphics`, `getIdScene`, `isOnFrustum`)
- **Lua C callbacks**: `on<Action>Lua` (e.g., `onLoadSpriteLua`, `onSetPixelShaderLua`)
- **File names**: `kebab-case` (e.g., `core-manager.h`, `my-scene.cpp`, `plugin-helper.h`)
- **Platform-specific defines**: `ANDROID`, `_WIN32`, `__linux__`, `USE_METAL`, `USE_OPENGL_ES`, `USE_DIRECTX9`

### C++ style
- **`API_IMPL`** macro on all public engine API methods (resolves to `__attribute__((visibility("default")))` or `__declspec(dllexport/import)`)
- **`noexcept`** on constructors and non-throwing functions
- **`override`** keyword on all virtual overrides
- **`static_cast<>`** — no C-style casts
- **Include guards** `#ifndef SOMETHING_H` / `#define SOMETHING_H` — not `#pragma once`
- **Indentation**: 4 spaces (no tabs)
- **Pointer style**: `TYPE *varName` (space before `*`, not after)
- **Accessor reuse**: if a function uses the same getter/setter-backed object more than once, store it in a local variable or reference for that function scope (e.g., `CAMERA &camera = device->getCamera();`). Do not cache accessor-returned engine state as persistent object/member state unless ownership/lifetime is explicitly designed for it.
- **Flat control flow with preserved performance**: keep new code's control flow as flat as practical. Prefer guard clauses and explicit mutually exclusive `if`/`else` branches over deep nesting or chained ternaries, while preserving behavior and performance, especially in hot paths. Do not flatten code by duplicating work, adding allocations, or introducing extra per-frame checks.
- **Editor continuous-loop audit**: whenever adding or editing any editor, inspect every path reachable from `onLoop(delta)` and other per-frame callbacks for redundant continuous work that can unnecessarily increase CPU/GPU usage. ImGui drawing and genuinely frame-dependent updates may remain per-frame, but asset loading, rebuilding, serialization, animation `seek`, buffer uploads, allocations, full-data scans, and other expensive operations must run only when their inputs change, when explicitly requested, or when a measured cadence requires them. Prefer dirty flags, cached results, change detection, event-driven updates, or throttling, and verify that an idle editor does not continuously perform expensive work.
- **Lua C functions**: always `extern "C"` with export macro; return `int`, take `lua_State *lua`
- **License block**: MIT license in box-drawing-character frame at top of every file

### Includes
Engine sources use angle-bracket includes relative to their `include_directories`:
```cpp
#include <core_mbm/scene.h>
#include <core_mbm/device.h>
#include <render/sprite.h>
#include <plugin-helper/plugin-helper.h>
```
Platform scenes use:
```cpp
#include "my-scene.h"
```
Lua headers always wrapped in `extern "C" { #include <lua.h> #include <lualib.h> #include <lauxlib.h> }`.

### Logging macros (from `core_mbm/util-interface.h` / `log-util.h`)
```cpp
INFO_LOG("message %s", str);
ERROR_LOG("error %s", str);
ERROR_AT(lineNum, fileName, "message");
```

---

## Lua Scripting

- Engine loads `main.lua` (or argument-specified script) via `SCENE_SCRIPT`
- Scene lifecycle in Lua: `onInitScene()`, `onLoop(delta)`, touch/key callbacks
  > Note: `startLoading()` and `endLoading()` are C++ pure virtuals only — they have no Lua callback equivalent.
- Lua API namespace: `mbm.*` (e.g., `mbm.loadScene(string)`, `mbm.getFps()`, `mbm.quit()`)
- Plugins loaded via `require "box2d"` / `require "ImGui"` etc.
- Modules loaded via `require "tiny_obj_loader"` (from `modules/`)
- `mbm.doCommands(cmd, param)` → bridges to `OnDoNativeCommand` C++ callback

---

## Game Development

### Game Project Template

When creating a new standalone Lua game project (e.g., `/home/michel/tower-defense`):

1. Copy `game-template/.github/copilot-instructions.md` → `<game-folder>/.github/copilot-instructions.md`.
   VS Code Copilot auto-loads it, giving the agent full mini-mbm Lua API knowledge without engine source being present.
2. Copy `game-template/AGENTS.md` → `<game-folder>/AGENTS.md`.
   Codex auto-loads it as the game project's repository instructions.
3. Copy `game-template/main.lua` as the entry-point script for the game.
4. Run the game: `./mini-mbm main.lua` (or `./bin/debug/linux_x86/mini-mbm main.lua`).

### Key Resources

| Resource | Path | Purpose |
|---|---|---|
| Lua API reference (canonical) | `docs/lua-api.md` | Full API: all `mbm.*` functions, render types, plugins, constants |
| Game project template | `game-template/` | Starting point for new standalone game folders |
| Game copilot context | `game-template/.github/copilot-instructions.md` | Self-contained API context for game projects |
| Game Codex context | `game-template/AGENTS.md` | Self-contained Codex context for game projects |
| Scaffolding skill | `.agents/skills/lua-game/SKILL.md` | Invoke when scaffolding a new game from the engine repo |

### Lua API Quick Summary

- **Lifecycle callbacks**: `onInitScene()`, `onLoop(delta)`, `onKeyDown(key)`, `onKeyUp(key)`, `onTouchDown(key,x,y)`, `onTouchUp(key,x,y)`, `onTouchMove(key,x,y)`, `onTouchZoom(zoom)`, `onKeyDownJoystick(player,key)`, `onMoveJoystick(player,lx,ly,rx,ry)`
- **Namespace**: `mbm.*` — ~60 functions for scene control, display, camera, asset paths, file system, input, coordinate transforms, shaders, dialogs, encryption, and system info
- **Render type constructors** (globals): `sprite`, `mesh`, `texture`, `gif`, `backGround`, `font`, `particle`, `shape`, `line`, `tile`, `render2texture`, `vec2`, `vec3`
- **Coordinate type strings**: `"2dw"` (2D world), `"2ds"` (2D screen/HUD), `"3d"` (3D world)
- **Plugins**: `require "box2d"`, `require "ImGui"` (returns as `tImGui`), `require "lsqlite3"`, `require "box2dLiquidFun"`

---

## Third-Party Libraries (bundled in `third-party/`)

| Library | Version | Use |
|---|---|---|
| lua | 5.4.1 | Scripting engine |
| portaudio | — | Audio (Linux/Windows) |
| box2d | 2.4.1 | 2D physics |
| box2d-liquid-fun | 2.3.1 | 2D fluid simulation |
| bullet | 2.84 | 3D physics |
| stb | — | Image loading helpers |
| lodepng | — | PNG loading |
| miniz | — | ZIP/zlib compression |
| rapidxml | — | XML parsing |
| lsqlite3 | — | SQLite3 Lua bindings |
| tinyfiledialogs | — | Native file dialogs |
| plusaes | — | AES encryption |
| wave | — | WAV audio format |
| dirent | 1.13 | Windows `dirent.h` shim |

---

## Platform Notes

### Linux
- Requires: `libx11-dev`, `libgles2-mesa-dev`, `libegl1-mesa-dev`
- Optional: `portaudio19-dev` (if `-DAUDIO=portaudio`)
- Default audio: `-DAUDIO=portaudio`

### macOS
- Requires: Xcode CLI tools (`xcode-select --install`), CMake via Homebrew
- Metal backend is the default on macOS; OpenGL ES is still selectable for the X11 compatibility path

### Android
- Requires: Android Studio, NDK r29, Ninja, Java 17 JDK
- `export NDK_ROOT=~/android-ndk-r29` before cmake
- Build with: `-DPLAT=Android -DANDROID_ABI=arm64-v8a -DANDROID_NATIVE_API_LEVEL=24`
- JNI bridge: `platform-android/main.cpp` implements `MiniMbmEngine_*` JNI functions
- Audio: `-DAUDIO=opensl`

### iOS
- Two CMake generators: plain `make` (unsigned, CI checks) and `-G Xcode` (device/simulator deploy)
- iOS platform sources (`.mm`) use `-fobjc-arc`
- Audio: `-DAUDIO=avfoundation`
- All plugins are `STATIC` on iOS (no dynamic loading)

### Windows (Visual Studio)
- Solution: `platform-msvs/mini-mbm.sln`, no CMake needed
- Key projects: `mini_mbm` (exe), `mini-mbm-lib` (launcher lib), `core_mbm` (engine lib), `lua5.4`, `lsqlite3`, `ImGui`, `box2d`, `box2dLiquidFun`, `bullet3d`
- CMake + MinGW also supported; audio: `-DAUDIO=portaudio`

---

## Important Constraints and Patterns

- **CMake ≥ 3.25.1** is required; `cmake_policy(SET CMP0054 NEW)` is set everywhere
- **`-DUSE_ALL=1`** is the recommended common full-featured build flag — it enables Lua, VR, Box2D, LiquidFun, ImGui, lSQLite3, and Tiled; Steam and Bullet3D remain explicit
- **ImGui-visible text must use ASCII-safe punctuation and symbols.** The current editor font atlas supports Portuguese letters/accents, but do not put typographic glyphs such as `—`, `–`, `…`, `°`, `×`, or `→` in labels, tooltips, status messages, or dynamically rendered text; they may appear as `?`. Use `-`, `...`, `deg`, `x`, and `->` respectively. Comments and non-rendered documentation are not constrained by this rule.
- **When touching `src/core_mbm/` or `include/core_mbm/`, preserve the current PIMPL/header-hygiene direction** — do not introduce new public mutable storage, backend handles, STL container exposure, or convenience accessors that leak `Impl`-owned state into public headers just to ship a feature. If a feature needs new internal state, prefer keeping it in `Impl`/`BackendData` or private translation-unit helpers, and update `docs/core-pimpl-status.md` when the work changes that boundary.
- **When shipping a new feature, update `include/version/version.h`** — bump `MBM_VERSION` to reflect the new functionality, following the versioning notes already documented in that header.
- **Plugin naming**: `CMAKE_SHARED_LIBRARY_PREFIX ""` removes the `lib` prefix so plugins load as `box2d.so` not `libbox2d.so`
- **iOS plugins are STATIC** — do not link Lua/core_mbm inside them (absorbed by final link)
- **`util::addPath(__FILE__)`** is a debug trick — add the source file's directory to the asset search path so relative image paths work from the IDE
- **`existScene(int idScene)`** must be overridden in `GAME` to return whether a given scene index exists
- **`DEVICE::getInstance()`** is a singleton; never hold it across frames as a member — call `getInstance()` each time
- **`enableRender`** member on `RENDERIZABLE` controls per-object visibility without destroying the object
- **Render-to-texture ownership**: do not use `RENDERIZABLE::isRender2Texture` as proof that an object is in a specific `RENDER_2_TEXTURE`. It only says the object participates in render-to-texture somewhere; `addObject2Render()` / `removeObject2Render()` must inspect that target's own 2D/3D object list.
- **DirectX9 render-to-texture binding hazard**: before `SetRenderTarget()` binds a texture surface as an output target, unbind texture samplers with `SetTexture(stage, nullptr)`. DX9 does not safely allow the same texture to remain bound as input while being used as a render target; leaving stale sampler bindings can cause invisible render-to-texture previews, invalid device state, or crashes after running for a while.
- **Sprite Maker animation preview**: render-to-texture previews compose many transparent frame/subset quads. Keep deterministic z ordering for preview parts so equal-depth transparent quads do not blank or flicker in copied/mirrored animations. DirectX9 can require refreshing the render-target object list for cached dynamic preview textures; do not compensate by changing the shared part order.
- Lua plugin entry points support both `luaopen_<name>` and `luaopen_lib<name>` for compatibility with different `require` mechanisms
