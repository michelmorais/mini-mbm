---
name: new-platform-port
description: "Add a new target platform to mini-mbm. Use when porting to a new OS/device, adding a new PLAT= value, creating platform-<name>/ files, wiring CMakeLists.txt, or understanding how Linux/macOS/Android/iOS platform sources differ."
---

# New Platform Port — mini-mbm

## When to Use

- Adding a new `-DPLAT=<Name>` target to mini-mbm
- Creating a `platform-<name>/` directory with the required source files
- Wiring the new platform into `CMakeLists.txt` (root) and `src/CMakeLists.txt`
- Understanding how existing platforms (Linux, macOS, Android, iOS) differ
- Choosing the correct render backend, audio backend, and compiler flags for a platform

## How the Platform Selection Works

Two CMake files control everything:

### Root `CMakeLists.txt`
- The user passes `-DPLAT=<Name>`; the file lower-cases it to `PLAT_LOWER`
- A long `if/elseif/else` chain sets these variables per platform:
  - `ENGINE_TARGET_PLATFORM` — string key used throughout `src/CMakeLists.txt`
  - `CMAKE_SYSTEM_NAME` — CMake's OS detection override
  - `FOLDER_ARCHITECTURE` — subdirectory under `bin/` and `libs/`
  - `AUDIO` — default audio backend
  - `USE_VR` / `DISABLE_VR` — VR feature toggle
  - `CMAKE_SUCCESS_SET_PLAT = 1` — guards the rest of the build
- A second block auto-selects the render backend from `CMAKE_SYSTEM_NAME` (Linux/Android → `USE_OPENGL_ES`, Darwin/iOS → `USE_METAL`, Windows → `USE_DIRECTX9`)

### `src/CMakeLists.txt`
- Reads `ENGINE_TARGET_PLATFORM` (and `USE_LUA`) to set source variables:
  - `MAIN` — entry-point `.cpp` / `.mm` file
  - `SCENE_CPP` — `my-scene.cpp` (C++ mode only)
  - `LIB_MINIMBM_LUA_PLATFORM_SRC` — platform launcher from `src/mini-mbm-lib/` (Lua mode only)
  - `IOS_PLATFORM_SOURCES` — extra `.mm` files (iOS only)
- Then a second chain of `if/elseif` blocks calls `add_executable` or `add_library` with those variables and the correct `target_link_libraries`

## Platform Quick-Reference

See [platform file lists](./references/platform-files.md) for the exact files each platform requires.

| Platform | PLAT= | ENGINE_TARGET_PLATFORM | Output type | Render | Audio | VR |
|---|---|---|---|---|---|---|
| Linux | `Linux` | `Linux` | `add_executable` | OpenGL ES | portaudio | on |
| macOS | `MacOs` | `MacOs` | `add_executable` | Metal | AVFoundation | on |
| Android | `Android` | `Android` | `add_library SHARED` | OpenGL ES | OpenSL | off |
| iOS | `iOS` | `iOS` | `add_executable` (bundle) | Metal | AVFoundation | off |
| Windows | `Windows` | `Windows` | `add_executable` | DirectX 9 | portaudio | on |

### Extra link libraries per platform

| Platform | Extra `target_link_libraries` |
|---|---|
| Linux | `Xrandr` |
| macOS | `-framework Cocoa` |
| Android | `android log EGL GLESv2 android_native_app_glue` (with `--whole-archive` for glue) |
| iOS | `-framework UIKit -framework Metal -framework AVFoundation -framework AudioToolbox -framework Foundation` |
| Windows | (system libs via `#pragma comment(lib,…)` in MSVC; nothing extra in MinGW) |

### Platform-specific defines

| Platform | Compiler defines added |
|---|---|
| Linux | `USE_OPENGL_ES`, `USE_VR`, `LUA_COMPAT_ALL`, `LUA_ANSI` |
| macOS | `USE_METAL`, `USE_VR`, `LUA_COMPAT_ALL`, `LUA_ANSI` |
| Android | `ANDROID`, `USE_OPENGL_ES`, `LUA_COMPAT_ALL`, `LUA_ANSI` |
| iOS | `USE_METAL`, `DISABLE_VR`, `MBM_PLATFORM_IOS`, `LUA_COMPAT_ALL`, `LUA_ANSI` |
| Windows | `WIN32`, `_WINDOWS`, `USE_DIRECTX9`, `USE_VR` |

### Plugins on restricted platforms (Android & iOS)
Android (since API 24) and iOS (App Store sandbox) **cannot load shared libraries at runtime**.
All plugin libraries (`box2d`, `ImGui`, `lsqlite3`, …) must be **statically linked**.
`src/CMakeLists.txt` has dedicated blocks for both platforms that call `add_definitions(-DUSE_<PLUGIN>)` and add each `target_link_libraries(mini-mbm ${PLUGIN_LIBRARY})` unconditionally.
On iOS, `box2d` and `box2d-liquid-fun` share object code and **cannot coexist**; when `-DUSE_ALL=1` is passed, `USE_BOX2D_LIQUID_FUN` is forced to 0.

## Step-by-Step: Adding a New Platform

### Step 1 — Create `platform-<newplat>/`

```
platform-<newplat>/
├── my-scene.h           # MY_SCENE : mbm::SCENE + GAME : mbm::CORE_MANAGER
├── my-scene.cpp         # Implement all virtual methods
├── main.cpp             # C++ mode entry point
├── main-lua.cpp         # Lua mode entry point  (if platform supports Lua launcher)
└── README.md            # Prerequisites + build commands
```

Use the templates in [assets/](./assets/):
- [my-scene.h template](./assets/my-scene-h.template)
- [my-scene.cpp template](./assets/my-scene-cpp.template)
- [main.cpp template](./assets/main-cpp.template)

**Required overrides in `MY_SCENE`.** Every platform must implement all of these pure virtuals from `mbm::SCENE`:

```cpp
void startLoading();          // called by engine before async asset load begins
void endLoading();            // called by engine when async load completes
void onInitScene();           // load assets, set up scene (first frame)
void onLoop();                // per-frame game logic
void onResizeWindow();        // handle window/viewport resize
void onFinalizeScene();       // cleanup before scene is destroyed
// Input events:
void onTouchDown(int key, float x, float y);
void onTouchUp(int key, float x, float y);
void onTouchMove(int key, float x, float y);
void onTouchZoom(float zoom);
void onKeyDown(int key);
void onKeyUp(int key);
void onKeyDownJoystick(int player, int key);
void onKeyUpJoystick(int player, int key);
void onMoveJoystick(int player, float lx, float ly, float rx, float ry);
void onInfoDeviceJoystick(int player, int maxBtn, const char *name, const char *extra);
```

> `startLoading` and `endLoading` are called by the engine's async loading system. In the simplest case they can be `INFO_LOG("...")` stubs, but they must be present — the base class declares them as pure virtual.

For ObjC++ platforms (macOS-like), use `.mm` extensions and add `-fobjc-arc` to those source files in CMake.
For native-activity platforms (Android-like), there is no `int main()`; use the NativeActivity + `android_native_app_glue` pattern with JNI entry points.

### Step 2 — Create `src/mini-mbm-lib/mini-mbm-lib-<NewPlat>.cpp`

This file is only needed in **Lua mode** (`-DUSE_LUA=1`). It provides the platform-specific part of the mini-mbm launcher library (window creation, event pump, etc.). Model it on the closest existing file:
- `mini-mbm-lib-Linux.cpp` — for POSIX desktops
- `mini-mbm-lib-MacOs.mm` — for Cocoa / ObjC++ desktops
- `mini-mbm-lib-iOS.mm` — for UIKit / Metal mobile

### Step 3 — Edit root `CMakeLists.txt`

In the `if(PLAT_LOWER STREQUAL …) / elseif / else` chain (after the `iOS` block and before the final `else(message(SEND_ERROR…))`), add:

```cmake
elseif(PLAT_LOWER STREQUAL "newplat")

    set(CMAKE_SYSTEM_NAME       "NewPlatOsName")   # e.g. "Linux", "Darwin", "Windows"
    set(ENGINE_TARGET_PLATFORM  "NewPlat")          # used in src/CMakeLists.txt
    set(FOLDER_ARCHITECTURE     "newplat_arch")     # e.g. "arm64", "linux_x86"
    set(CMAKE_SUCCESS_SET_PLAT   1)

    # VR toggle
    if(NOT USE_VR AND NOT DISABLE_VR)
        set(USE_VR 1)                               # or set(DISABLE_VR 1) if not supported
        add_definitions(-DUSE_VR)
    endif()

    # Audio default
    if(NOT AUDIO)
        set(AUDIO portaudio)                        # choose: portaudio|avfoundation|opensl|none
    endif()

    # Optional platform defines
    add_definitions(-DMBM_PLATFORM_NEWPLAT)
```

Also add the render backend default in the second `if(CMAKE_SYSTEM_NAME …)` chain:

```cmake
elseif(CMAKE_SYSTEM_NAME STREQUAL "NewPlatOsName")
    message(STATUS " Defaulting to USE_OPENGL_ES backend...")   # or USE_METAL etc.
    set(USE_OPENGL_ES 1)
    add_definitions(-DUSE_OPENGL_ES)
```

### Step 4 — Edit `src/CMakeLists.txt`

#### 4a — In the `if(USE_LUA)` block (Lua mode source selection)

After the iOS `elseif` and before `else(message(SEND_ERROR…))`:

```cmake
elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "NewPlat")
    set(MAIN                        ${CMAKE_SOURCE_DIR}/platform-newplat/main-lua.cpp)
    set(LIB_MINIMBM_LUA_HEADER      ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib)
    set(LIB_MINIMBM_LUA_PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/mini-mbm-lib/mini-mbm-lib-NewPlat.cpp)
    # For ObjC++ platforms add:
    # set_source_files_properties(${LIB_MINIMBM_LUA_PLATFORM_SRC}
    #     PROPERTIES COMPILE_FLAGS "-fobjc-arc")
```

#### 4b — In the `else()` block (C++ mode source selection)

```cmake
elseif(${ENGINE_TARGET_PLATFORM} STREQUAL "NewPlat")
    set(MAIN      ${CMAKE_SOURCE_DIR}/platform-newplat/main.cpp)
    set(SCENE_CPP ${CMAKE_SOURCE_DIR}/platform-newplat/my-scene.cpp)
```

#### 4c — In the `add_executable` / `add_library` chain

Add a new `elseif` for the final build step. For a normal desktop platform producing an executable:

```cmake
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "NewPlat")

    if (USE_LUA)
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES}
                            ${SCENE_CPP}
                            ${LIB_MINIMBM_LUA_SRC}
                            ${LIB_MINIMBM_LUA_PLATFORM_SRC})
    else()
        add_executable(mini-mbm ${MAIN}
                            ${SOURCES}
                            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
                            ${BULLET3D_WRAP_SOURCES}
                            ${TINYFILEDIALOGS_SOURCES}
                            ${SCENE_CPP})
    endif()

    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    # Add any platform system libraries:
    # target_link_libraries(mini-mbm SomePlatformLib)
```

For a **shared-library** platform (like Android), replace `add_executable` with `add_library(mini-mbm SHARED …)`.

### Step 5 — Build and Test

```sh
mkdir -p build/newplat_debug && cd build/newplat_debug
cmake ../.. \
    -DPLAT=NewPlat \
    -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

For the full-featured build:

```sh
cmake ../.. \
    -DPLAT=NewPlat \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=<backend> \
    -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Key Differences Between Existing Platforms

See [platform-files reference](./references/platform-files.md) and [cmake-integration reference](./references/cmake-integration.md) for full details.

### Entry Point Patterns

| Mode | Linux/macOS/Windows | Android | iOS |
|---|---|---|---|
| C++ | `int main() { GAME g; g.initGraphics("…"); g.onLoop(false,true); }` | JNI `MiniMbmEngine_init(…)` + `onLoop(singleLoop=true, swapBuffers=false)` | `UIApplicationMain(…)` → `MetalViewController::viewDidLoad` |
| Lua | `mbm::onLoop()` via mini-mbm-lib | `mbm::onLoop()` via NativeActivity glue | `UIApplicationMain(…)` → MetalViewController Lua path |

### Source Language

| Platform | Language | Extension | ARC |
|---|---|---|---|
| Linux | C++ | `.cpp` | n/a |
| macOS | C++ / ObjC++ | `.cpp` / `.mm` | `-fobjc-arc` on `.mm` |
| Android | C++ | `.cpp` | n/a |
| iOS | ObjC++ | `.mm` | `-fobjc-arc` on all platform files |
| Windows | C++ | `.cpp` | n/a |

### `GAME` vs `MY_GAME` class name

- Linux, macOS, iOS: class named `GAME : public mbm::CORE_MANAGER`
- Android: class named `MY_GAME : public mbm::CORE_MANAGER` (with `MY_GAME(JNIEnv*, jobject)` constructor)

### `my-scene.h` identity fields

Android's `my-scene.h` adds:
```cpp
mbm::TEXTURE_VIEW* texBox;                          // extra member
const char* getSceneName() noexcept { return "my-scene-android"; }
```
and `MY_GAME` constructor takes `(JNIEnv* env, jobject obj)`.

All other platforms' `my-scene.h` files are structurally identical.
