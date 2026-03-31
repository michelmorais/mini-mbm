# CMake Integration Reference

## Root `CMakeLists.txt` — Platform Block Structure

The file (at repo root) has one large `if/elseif/else` chain on `PLAT_LOWER`.
Each platform block **must** set these five things:

```cmake
elseif(PLAT_LOWER STREQUAL "<plat_lower>")
    set(CMAKE_SYSTEM_NAME          "<CmakeOsName>")   # used to auto-select render backend
    set(ENGINE_TARGET_PLATFORM     "<PlatKey>")        # key string used in src/CMakeLists.txt
    set(FOLDER_ARCHITECTURE        "<arch_subdir>")    # e.g. linux_x86, arm64, iphoneos_arm64
    set(CMAKE_SUCCESS_SET_PLAT      1)                 # REQUIRED — without this the build aborts
    # Audio default (set only if NOT already set by user):
    if(NOT AUDIO)
        set(AUDIO <backend>)                           # audiere|portaudio|avfoundation|opensl|none
    endif()
    # VR toggle:
    if(NOT USE_VR AND NOT DISABLE_VR)
        set(USE_VR 1)                                  # or  set(DISABLE_VR 1)
        add_definitions(-DUSE_VR)
    endif()
```

### Existing platform blocks (order in file)

```
if     android   → CMAKE_SYSTEM_NAME=Android, ENGINE_TARGET_PLATFORM=Android, ABI handling
elseif windows   → CMAKE_SYSTEM_NAME=Windows, ENGINE_TARGET_PLATFORM=Windows
elseif linux     → CMAKE_SYSTEM_NAME=Linux,   ENGINE_TARGET_PLATFORM=Linux
elseif macos     → CMAKE_SYSTEM_NAME=Darwin,  ENGINE_TARGET_PLATFORM=MacOs
elseif ios       → CMAKE_SYSTEM_NAME=iOS (CACHE FORCE), ENGINE_TARGET_PLATFORM=iOS
else             → SEND_ERROR "Platform unknown"
```

**Insert your new `elseif` before the final `else`.**

### Render-backend auto-select block (also in root CMakeLists.txt)

After the PLAT block, there is a second `if/elseif` chain that reads `CMAKE_SYSTEM_NAME`:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")         → USE_OPENGL_ES
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")   → USE_DIRECTX9
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")    → USE_METAL
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")   → USE_OPENGL_ES
elseif(${PLAT} STREQUAL "Android")             → USE_OPENGL_ES  (fallback)
elseif(${PLAT} STREQUAL "Linux")               → USE_OPENGL_ES  (fallback)
elseif(${PLAT} STREQUAL "Windows")             → USE_DIRECTX9   (fallback)
elseif(${PLAT} STREQUAL "MacOs")               → USE_METAL      (fallback)
elseif(${PLAT} STREQUAL "iOS")                 → USE_METAL      (fallback)
```

If your new platform uses a unique `CMAKE_SYSTEM_NAME`, add `elseif` here too. If you reuse
an existing OS name (e.g. "Linux") the existing entry already covers it.

---

## `src/CMakeLists.txt` — Source Selection Blocks

### Block A: Lua mode source selection (`if(USE_LUA)`)

Reads `ENGINE_TARGET_PLATFORM`. Must set:
- `MAIN` — path to entry-point `.cpp` / `.mm`
- `LIB_MINIMBM_LUA_PLATFORM_SRC` — path to `src/mini-mbm-lib/mini-mbm-lib-<Plat>.*`
- `LIB_MINIMBM_LUA_HEADER` — `${CMAKE_SOURCE_DIR}/src/mini-mbm-lib` (same for all desktop platforms)

For ObjC++ files, add immediately after setting the variable:

```cmake
set_source_files_properties(${LIB_MINIMBM_LUA_PLATFORM_SRC}
    PROPERTIES COMPILE_FLAGS "-fobjc-arc")
```

iOS also sets `IOS_PLATFORM_SOURCES` (the AppDelegate / MetalView / ViewController `.mm` trio)
and applies `-fobjc-arc` to all of them.

### Block B: C++ mode source selection (`else()` after `if(USE_LUA)`)

Must set:
- `MAIN` — entry-point path
- `SCENE_CPP` — `platform-<name>/my-scene.cpp` path

### Block C: `add_executable` / `add_library` build block

One `elseif` per platform. The standard desktop pattern:

```cmake
elseif (${ENGINE_TARGET_PLATFORM} STREQUAL "NewPlat")

    if (USE_LUA)
        add_executable(mini-mbm
            ${MAIN}
            ${SOURCES}                          # lua-wrap/ glob (when USE_LUA)
            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
            ${BULLET3D_WRAP_SOURCES}
            ${TINYFILEDIALOGS_SOURCES}
            ${SCENE_CPP}                        # empty in Lua mode
            ${LIB_MINIMBM_LUA_SRC}             # mini-mbm-lib/mini-mbm-lib.cpp
            ${LIB_MINIMBM_LUA_PLATFORM_SRC})   # mini-mbm-lib/mini-mbm-lib-<Plat>.*
    else()
        add_executable(mini-mbm
            ${MAIN}
            ${SOURCES}
            ${BOX2D_LIQUID_FUN_WRAP_SOURCES}
            ${BULLET3D_WRAP_SOURCES}
            ${TINYFILEDIALOGS_SOURCES}
            ${SCENE_CPP})
    endif()

    target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
    # target_link_libraries(mini-mbm <platform-system-libs>)
```

#### Android — shared library variant

```cmake
add_library(mini-mbm SHARED ${MAIN} ${SOURCES} … ${SCENE_CPP})

target_include_directories(mini-mbm PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY})
target_link_libraries(mini-mbm
    -Wl,--whole-archive android_native_app_glue -Wl,--no-whole-archive)
target_link_libraries(mini-mbm android log EGL GLESv2)
```

`--whole-archive` is required to keep `ANativeActivity_onCreate` (called by the OS, not our code) from being dead-stripped.

#### iOS — bundle variant

```cmake
add_executable(mini-mbm
    ${MAIN} ${IOS_PLATFORM_SOURCES} ${SOURCES} …
    ${LIB_MINIMBM_LUA_SRC} ${LIB_MINIMBM_LUA_PLATFORM_SRC})

set_target_properties(mini-mbm PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/platform-ios/Info.plist
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${GAME_BUNDLE_ID}")

target_link_libraries(mini-mbm ${CORE_MBM_LIBRARY}
    "-framework UIKit" "-framework Metal"
    "-framework AVFoundation" "-framework AudioToolbox"
    "-framework Foundation")
```

### Block D: Plugin static-link workarounds (Android & iOS)

After Block C, `src/CMakeLists.txt` has two separate `if(ENGINE_TARGET_PLATFORM STREQUAL …)` blocks that iterate over all enabled plugin flags and call `target_link_libraries` statically. If your new platform also requires static plugin linking, add an analogous block.

---

## Variable Reference

| Variable | Set by | Consumed by |
|---|---|---|
| `PLAT` | User (-DPLAT=) | root CMakeLists.txt |
| `PLAT_LOWER` | root CMakeLists.txt | root CMakeLists.txt |
| `ENGINE_TARGET_PLATFORM` | root CMakeLists.txt | src/CMakeLists.txt |
| `CMAKE_SYSTEM_NAME` | root CMakeLists.txt | CMake, render-backend block |
| `FOLDER_ARCHITECTURE` | root CMakeLists.txt | bin/ and libs/ output dirs |
| `CMAKE_SUCCESS_SET_PLAT` | root CMakeLists.txt | Guards `project()` and the rest |
| `AUDIO` | root CMakeLists.txt (default) or user | src/core_mbm/CMakeLists.txt audio selection |
| `USE_VR` / `DISABLE_VR` | root CMakeLists.txt | VR class inclusion |
| `MAIN` | src/CMakeLists.txt (platform block) | `add_executable` source list |
| `SCENE_CPP` | src/CMakeLists.txt (platform block) | `add_executable` source list (C++ mode) |
| `LIB_MINIMBM_LUA_SRC` | src/CMakeLists.txt (USE_LUA block) | `add_executable` source list |
| `LIB_MINIMBM_LUA_PLATFORM_SRC` | src/CMakeLists.txt (platform block) | `add_executable` source list |
| `IOS_PLATFORM_SOURCES` | src/CMakeLists.txt (iOS block) | `add_executable` source list |
| `SOURCES` | src/CMakeLists.txt `file(GLOB_RECURSE … lua-wrap/)` | `add_executable` source list |
| `CORE_MBM_LIBRARY` | root CMakeLists.txt (`core_mbm`) | `target_link_libraries` |

---

## Checklist for a New Platform

- [ ] `elseif(PLAT_LOWER STREQUAL …)` added in root CMakeLists.txt platform chain
- [ ] All five mandatory variables set (`CMAKE_SYSTEM_NAME`, `ENGINE_TARGET_PLATFORM`, `FOLDER_ARCHITECTURE`, `CMAKE_SUCCESS_SET_PLAT=1`, `AUDIO` default)
- [ ] Render backend registered in the `CMAKE_SYSTEM_NAME` auto-select block
- [ ] `elseif` added in `src/CMakeLists.txt` Lua-mode source block (sets `MAIN`, `LIB_MINIMBM_LUA_PLATFORM_SRC`, `LIB_MINIMBM_LUA_HEADER`)
- [ ] `elseif` added in `src/CMakeLists.txt` C++-mode source block (sets `MAIN`, `SCENE_CPP`)
- [ ] `elseif` added in `src/CMakeLists.txt` build block (`add_executable`/`add_library` + `target_link_libraries`)
- [ ] `platform-<name>/` directory exists with `my-scene.h`, `my-scene.cpp`, `main.cpp`
- [ ] If Lua mode: `src/mini-mbm-lib/mini-mbm-lib-<NewPlat>.cpp` exists
- [ ] If plugins need static linking: dedicated plugin-link block added in `src/CMakeLists.txt`
