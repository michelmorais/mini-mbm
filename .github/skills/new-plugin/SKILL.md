---
name: new-plugin
description: 'Step-by-step workflow for creating a new mini-mbm Lua plugin (shared library). Use when: adding a new plugin, creating a new box2d-style Lua binding, adding a new plugin-helper-backed feature, wiring a new plugin into the CMake build, implementing luaopen_ entry points, implementing the PLUGIN callback interface, creating a tilemap-style or ImGui-style plugin.'
---

# New Plugin Skill — mini-mbm

## When to Use

Invoke this skill whenever the task is to create a **new plugin** for the mini-mbm engine — a shared library (`.so` / `.dll`) or iOS static archive that exposes a Lua API via `require "myplugin"`.

---

## 1. Understand the Plugin Contract

Every mini-mbm plugin must fulfil **all three** of these contracts:

### 1a. Lua entry points (required in every plugin)

```cpp
// myplugin-lua.h
extern "C" MY_PLUGIN_API int luaopen_myplugin  (lua_State *lua);
extern "C" MY_PLUGIN_API int luaopen_libmyplugin(lua_State *lua);
```

- Both names are required: `luaopen_<name>` for `require "myplugin"` and `luaopen_lib<name>` for `require "libmyplugin"`.
- **`luaopen_libmyplugin` must always delegate to `luaopen_myplugin`** — never duplicate the body.
- For pure-Lua plugins (§1e) the entry point calls `luaL_newlib` and returns the table. For PLUGIN-type plugins it calls `plugin_doSubscribe` (see §1d) and returns the plugin table.

### 1b. Export macro pattern (copy from existing plugin)

```cpp
// myplugin-lua.h
#ifndef MYPLUGIN_IMPORTER_H
#define MYPLUGIN_IMPORTER_H

#if defined (__GNUC__)
  #define MY_PLUGIN_API  __attribute__ ((__visibility__("default")))
#elif defined (WIN32)
  #ifdef MY_PLUGIN_BUILD_DLL
    #define MY_PLUGIN_API  __declspec(dllexport)
  #else
    #define MY_PLUGIN_API  __declspec(dllimport)
  #endif
#endif

extern "C" {
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}

extern "C" MY_PLUGIN_API int luaopen_myplugin  (lua_State *lua);
extern "C" MY_PLUGIN_API int luaopen_libmyplugin(lua_State *lua);

#endif // !MYPLUGIN_IMPORTER_H
```

- GCC/Clang: `__attribute__ ((__visibility__("default")))`
- MSVC/Windows: `__declspec(dllexport)` when `MY_PLUGIN_BUILD_DLL` is defined, `__declspec(dllimport)` otherwise. Add `#pragma comment(lib, "lua5.4.lib")` if needed (see ImGui plugin).
- Use a **unique** macro name per plugin to avoid ODR issues with other plugins.

### 1c. PLUGIN callback interface (optional — only if the plugin needs engine hooks)

If the plugin needs per-frame `onLoop()`, input events, or render hooks, inherit from `PLUGIN`:

```cpp
// include/core_mbm/plugin-callback.h  (already in the repo — do NOT copy)
class PLUGIN {
public:
    PLUGIN() noexcept = default;
    virtual ~PLUGIN() = default;
    virtual void onSubscribe         (int width, int height, void *context, void *renderDevice) = 0;
    virtual void onResizeWindow      (int width, int height) = 0;
    virtual void onTouchDown         (int key, float x, float y) = 0;
    virtual void onTouchUp           (int key, float x, float y) = 0;
    virtual void onTouchMove         (int key, float x, float y) = 0;
    virtual void onTouchZoom         (float zoom) = 0;
    virtual void onKeyDown           (int key) = 0;
    virtual void onKeyUp             (int key) = 0;
    virtual void onDoubleClick       (float x, float y, int key) = 0;
    virtual void onKeyDownJoystick   (int player, int key) = 0;
    virtual void onKeyUpJoystick     (int player, int key) = 0;
    virtual void onMoveJoystick      (int player, float lx, float ly, float rx, float ry) = 0;
    virtual void onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo) = 0;
    virtual void onPrepare           () = 0; // once before render loop starts
    virtual void onLoop              (float delta) = 0;
    virtual void onRender            () = 0;
    virtual void onDestroy           () = 0;
};
```

Include: `#include <core_mbm/plugin-callback.h>`

### 1d. PLUGIN_IDENTIFIER — userdata type guard

Every plugin that exposes Lua userdata must declare a `PLUGIN_IDENTIFIER` so the engine can validate that a userdata object belongs to this plugin before calling into it:

```cpp
// myplugin-lua.cpp  (module scope — one per plugin, not per class)
static int PLUGIN_IDENTIFIER = 1;   // initial value; overwritten at load time by plugin_stamp_userdata
```

**Registration** — call once inside `onNewMyPluginLua`, after creating the userdata and before `lua_rawseti`:

```cpp
// One line replaces the ~12-line _usertype_plugin metatable block:
mbm::plugin_stamp_userdata(lua, &PLUGIN_IDENTIFIER);
```

This applies the engine's `_usertype_plugin` metatable to the userdata at the top of the stack _and_ writes the runtime plugin-identifier into `PLUGIN_IDENTIFIER`.

**Validation** — at the top of every Lua C callback that receives plugin userdata:

```cpp
int onSomeMethodLua(lua_State *lua)
{
    // Extracts and validates a PLUGIN_IDENTIFIER-stamped userdata from table at stack index 1.
    MY_PLUGIN **pp = static_cast<MY_PLUGIN **>(
        mbm::plugin_check_userdata(lua, 1, 1, PLUGIN_IDENTIFIER, "MyPlugin"));
    MY_PLUGIN *self = *pp;
    // ... proceed safely
}
```

`plugin_check_userdata` calls `luaL_error` on type mismatch — it never returns `nullptr` on success.

### 1e. Alternative: Pure Lua library (no PLUGIN interface)

If your plugin only needs to expose a set of **stateless utility functions** to Lua — no per-frame updates, no engine backend access, no render hooks, no persistent C++ state per scene — you do **not** need to inherit from `PLUGIN` at all.

**When to choose this path:**
- The plugin wraps a standalone library (parser, codec, algorithm) with no connection to the render loop.
- There is no `onLoop`, `onSubscribe`, or `onDestroy` work to do.
- You do not need to allocate per-scene C++ objects or hold Lua registry refs across frames.

**Real example:** `modules/obj_importer_lua/` — parses `.obj` mesh files and returns Lua tables. Loaded with `require "tiny_obj_loader"`.

#### File layout (minimal — 2 files)

```
plugins/myplugin/
├── CMakeLists.txt          # Only links LUA_LIBRARY — no core_mbm, no plugin_helper
├── myplugin-lua.h          # Export macro + luaopen_ declarations (same as §1b)
└── myplugin-lua.cpp        # luaL_Reg table + luaopen_ implementations
```

No `myplugin-wrap.h`, no `myplugin-class-lua.cpp`, no `PLUGIN_IDENTIFIER` needed.

#### Implementation pattern

```cpp
// myplugin-lua.cpp
#include "myplugin-lua.h"
#include <some_third_party_lib.h>   // whatever you're wrapping

// ---- Lua C function --------------------------------------------------------

static int onDoThingLua(lua_State *lua)
{
    const char *input = luaL_checkstring(lua, 1);
    // ... call third-party lib, push results onto Lua stack ...
    lua_pushboolean(lua, 1);
    return 1;
}

// ---- Function table --------------------------------------------------------

static const luaL_Reg mylib[] =
{
    { "doThing",  onDoThingLua },
    { NULL,       NULL         }
};

// ---- Entry points ----------------------------------------------------------

// luaopen_ returns the function table directly as the require result.
// Contrast with the PLUGIN style, which returns true (boolean) after
// subscribing to the engine — the API lives on mbm.* or a created object.
int luaopen_myplugin(lua_State *lua)
{
    luaL_newlib(lua, mylib);
    return 1;   // the table is the require result; caller does: local mp = require "myplugin"
}

int luaopen_libmyplugin(lua_State *lua)
{
    return luaopen_myplugin(lua);
}
```

#### Lua usage

```lua
local mp = require "myplugin"   -- mp IS the table returned by luaL_newlib
mp.doThing("hello")
```

Compare with the full PLUGIN style where `require` returns `true` and the real API is accessed through engine-managed objects.

#### CMakeLists.txt differences

The pure-library CMakeLists is simpler — it does **not** link `core_mbm` or `plugin_helper`:

```cmake
set(CMAKE_SHARED_LIBRARY_PREFIX "")

if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    add_library(${MYPLUGIN_LIBRARY} STATIC ${SOURCES})
else()
    add_library(${MYPLUGIN_LIBRARY} SHARED ${SOURCES})
endif()

# Only Lua is needed — no core_mbm, no plugin_helper
if (NOT ${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    target_link_libraries(${MYPLUGIN_LIBRARY} PRIVATE ${LUA_LIBRARY})
endif()
```

#### Decision guide

| Need | Use |
|---|---|
| `onLoop`, `onSubscribe`, `onDestroy`, input events, render access | Full `PLUGIN` interface (§1c) |
| Userdata objects managed by the engine | Full `PLUGIN` interface + `PLUGIN_IDENTIFIER` (§1d) |
| Stateless utility functions, pure data parsing/conversion | Pure Lua library — `luaL_newlib` (§1e) |

---

## 2. PLUGIN Lifecycle — Pitfalls and Rules

Understanding the lifetime of a PLUGIN instance is critical. Getting it wrong causes crashes or double-initialization bugs that are hard to reproduce.

### 2a. How the engine calls PLUGIN methods

```
require "myplugin"          → luaopen_myplugin()
                              → registerClassMyPlugin()    (builds Lua table)
                              → new MY_PLUGIN()            (allocates instance)
                              → mbm.subscribe(table)       (engine stores pointer)
                              → onSubscribe()              (first callback — initialize resources)

per-frame                   → onLoop(delta)
                            → onRender()

scene ends                  → onDestroy()                  (release/close resources)
                              (instance may not be deleted immediately — see §2c)

next scene, require again   → luaopen_myplugin() is called again from scratch
                              → new MY_PLUGIN() → onSubscribe() → ...
```

### 2b. Process-lifetime vs scene-lifetime resources

**The most dangerous pitfall:** some external libraries must be initialized **once per process** and must not be shut down between scenes. If `onDestroy` shuts them down, and the next scene immediately re-initializes them, you may hit:

- Double-init crashes (library not designed for it)
- Broken callbacks that were registered before shutdown
- Memory corruption from partially-torn-down global state

**Rule:** if the underlying library has a process-global init/shutdown API (e.g. `SteamAPI_InitEx` / `SteamAPI_Shutdown`, a GPU context, a network layer), treat it as a **process-lifetime resource**:

```cpp
// Global flag — defined in myplugin-lua.cpp, declared extern in myplugin-impl.h
bool g_myPluginGlobalReady = false;

void MY_PLUGIN::onSubscribe(int, int, void *, void *) override
{
    if (!g_myPluginGlobalReady)
    {
        // Initialize only on the very first load — never again for this process.
        if (MyLib_Init() == SUCCESS)
        {
            g_myPluginGlobalReady = true;
            // Register shutdown via atexit so it runs once at process exit,
            // regardless of how many scenes load/unload this plugin.
            std::atexit([] { MyLib_Shutdown(); });
        }
        else
        {
            ERROR_LOG("MyLib_Init failed.");
        }
    }
    m_bReady = g_myPluginGlobalReady;
}

void MY_PLUGIN::onDestroy() override
{
    // Do NOT call MyLib_Shutdown() here.
    // Only release per-scene state: cancel async requests, release Lua refs,
    // free scene-scoped allocations, set m_bReady = false.
    m_bReady = false;
    m_pendingRequests.clear();   // destructors release Lua registry refs
}
```

**Per-scene resources** (textures, file handles, Lua callback refs, etc.) are safe to release in `onDestroy`.

### 2c. The deferred-destruction race

The engine may enqueue scene destruction rather than executing it synchronously. This means:

- `onDestroy` of the **old** scene's plugin instance may fire **after** `onSubscribe` of the **new** scene's instance.
- Both instances may be alive simultaneously for a short window.

**Consequence:** if `onDestroy` shuts down a global library and `onSubscribe` tries to initialize it again in the same frame, the result is undefined.

Using the `g_myPluginGlobalReady` flag (§2b) eliminates this race entirely: `onSubscribe` only calls `MyLib_Init` once, and `onDestroy` never touches the global state.

### 2d. The singleton pointer

Plugins that expose a Lua API where callback functions need access to engine state (pending async requests, Lua state, etc.) use a module-level singleton pointer:

```cpp
// myplugin-lua.cpp
static MY_PLUGIN *g_myPluginInstance = nullptr;

namespace mbm {
    MY_PLUGIN *getMyPluginInstance() noexcept { return g_myPluginInstance; }
}
```

Set it inside `luaopen_myplugin` when creating the instance, and **do not** clear it in `onDestroy` — any Lua functions that fire during the frame will still read the pointer. The pointer is overwritten the next time `require "myplugin"` is called in a new scene.

### 2e. Summary table

| Resource type | Initialize in | Shut down in | Example |
|---|---|---|---|
| Process-global library | `onSubscribe` (guarded by `g_*GlobalReady`) | `std::atexit` lambda | Steam, network layer |
| Per-scene allocations | `onSubscribe` | `onDestroy` | Textures, file handles |
| Async request lists | populated during `onLoop` | `onDestroy` (clear vector, destructors release refs) | `CCallResult`, network requests |
| Lua registry refs | acquired in Lua callbacks | Released in async result handler or `onDestroy` | `luaL_ref` / `luaL_unref` |
| Singleton pointer | set in `luaopen_` | Never cleared (overwritten on next load) | `g_myPluginInstance` |

---

## 3. File Layout `plugins/<myplugin>/`. Minimum required files:

```
plugins/myplugin/
├── CMakeLists.txt          # Build definition (see template in §3)
├── myplugin-lua.h          # Export macro + luaopen_ declarations
├── myplugin-lua.cpp        # luaopen_ implementations → calls registerClassMyPlugin()
├── myplugin-wrap.h         # C++ wrapper class for underlying library (optional)
├── myplugin-wrap.cpp       # Wrapper implementation (optional)
├── myplugin-class-lua.h    # Per-class Lua binding declarations (optional, can split)
└── myplugin-class-lua.cpp  # Per-class Lua binding implementations (optional)
```

**Naming conventions** (follow exactly):
- File names: `kebab-case` (e.g., `my-plugin-wrap.cpp`)
- Classes: `ALL_CAPS` (e.g., `MY_PLUGIN_WRAP`)
- Methods: `camelCase`
- Lua C callbacks: `on<Action>Lua` (e.g., `onCreateMyPluginLua`)
- Lua entry point: `luaopen_myplugin` / `luaopen_libmyplugin`

---

## 4. CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION ${CMAKE_VERSION_MAJOR} FATAL_ERROR)
cmake_policy(SET CMP0054 NEW)
project(${MYPLUGIN_LIBRARY})           # variable set in root CMakeLists.txt

set(CORE_MBM_INCLUDES     ${CMAKE_SOURCE_DIR}/include)
set(PLUGIN_HELPER_INC     ${CMAKE_SOURCE_DIR}/plugins)
set(THIRD_PARTY           ${CMAKE_SOURCE_DIR}/third-party)
set(LUA_INCLUDE           ${THIRD_PARTY}/lua-${LUA_LIB_VERSION})
set(MYPLUGIN_SRC          ${CMAKE_SOURCE_DIR}/plugins/myplugin)

# Optional: third-party library headers
# set(MYLIB_PATH    ${CMAKE_SOURCE_DIR}/third-party/mylib-x.y.z)
# set(MYLIB_INCLUDE ${MYLIB_PATH}/include)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-variable")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-class-memaccess")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-strict-aliasing -Wno-unused-but-set-variable -Wno-maybe-uninitialized")
endif()

# Optional: also glob third-party sources: "${MYLIB_PATH}/*.cpp"
file(GLOB_RECURSE SOURCES "${MYPLUGIN_SRC}/*.cpp")

include_directories(
    ${LUA_INCLUDE}
    ${CORE_MBM_INCLUDES}
    ${PLUGIN_HELPER_INC}
    ${CORE_MBM_INCLUDES}/core_mbm
    # ${MYLIB_INCLUDE}   # add if wrapping a third-party lib
)

set(CMAKE_SHARED_LIBRARY_PREFIX "")   # CRITICAL: removes "lib" prefix → loads as "myplugin.so" not "libmyplugin.so"

# iOS must be STATIC (App Store sandbox forbids user-space dylibs)
if (${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    add_library(${MYPLUGIN_LIBRARY} STATIC ${SOURCES})
else()
    add_library(${MYPLUGIN_LIBRARY} SHARED ${SOURCES})
endif()

# On iOS all libraries are STATIC archives absorbed into mini-mbm at final link.
# Lua / core_mbm / plugin_helper are provided by mini-mbm — skip to avoid duplicate-library warnings.
if (NOT ${ENGINE_TARGET_PLATFORM} STREQUAL "iOS")
    target_link_libraries(${MYPLUGIN_LIBRARY} PRIVATE ${LUA_LIBRARY})
    target_link_libraries(${MYPLUGIN_LIBRARY} PRIVATE ${CORE_MBM_LIBRARY})
    target_link_libraries(${MYPLUGIN_LIBRARY} PRIVATE ${PLUGIN_HELPER_LIBRARY})
    # target_link_libraries(${MYPLUGIN_LIBRARY} PRIVATE mylib)   # add if wrapping a third-party lib
endif()
```

**Key rules:**
- `project(${MYPLUGIN_LIBRARY})` — uses the variable name set by the root `CMakeLists.txt`
- `set(CMAKE_SHARED_LIBRARY_PREFIX "")` — **always required** to strip the `lib` prefix
- iOS branch uses `STATIC`, all other platforms use `SHARED`
- iOS branch **does not** call `target_link_libraries` for Lua/core_mbm/plugin_helper

---

## 5. Wire the Plugin into the Root Build

Edit **`CMakeLists.txt`** (root) in **exactly** four places:

### 4a. Library name variable (near line 510, in the `#libraries name` block)

```cmake
set(MYPLUGIN_LIBRARY    myplugin)   # matches project() argument in plugin's CMakeLists.txt
```

### 4b. Library source path variable (near line 500, in the `#library folder` block)

```cmake
set(MYPLUGIN_LUA_LIB    ${PLUGINS}/myplugin)
```

### 4c. Feature flag default (near line 548, in the `# Default all plugin flags` block)

```cmake
if(NOT DEFINED USE_MYPLUGIN)
    set(USE_MYPLUGIN 0)
endif()
```

### 4d. Enable under `USE_ALL` (near line 575)

```cmake
if(USE_ALL)
    # ... existing entries ...
    set(USE_MYPLUGIN 1)
endif()
```

### 4e. `add_subdirectory` under the `#plugins` block (near line 989)

```cmake
# inside "if (USE_LUA)" → "else" branch (non-Android):
if(USE_MYPLUGIN)
    add_subdirectory(${MYPLUGIN_LUA_LIB}    ${MYPLUGIN_LUA_LIB})
endif()
```

For **Android**, add the same block inside the `if(PLAT_LOWER STREQUAL "android")` branch, plus add a link workaround in `src/CMakeLists.txt`:

```cmake
# src/CMakeLists.txt — Android link workaround
if(USE_MYPLUGIN)
    message(STATUS "USE_MYPLUGIN is enabled, ${MYPLUGIN_LIBRARY} for ANDROID will be linked as workaround")
    target_link_libraries(mini-mbm ${MYPLUGIN_LIBRARY})
endif()
```

For **iOS**, add the same static-link block in `src/CMakeLists.txt` (near line 370):

```cmake
# src/CMakeLists.txt — iOS static link
if(USE_MYPLUGIN)
    message(STATUS "USE_MYPLUGIN is enabled — linking statically for iOS")
    target_link_libraries(mini-mbm ${MYPLUGIN_LIBRARY})
endif()
```

And add the `-DUSE_MYPLUGIN` compile definition (in the iOS `add_definitions` block in `src/CMakeLists.txt`):

```cmake
if(USE_MYPLUGIN)
    add_definitions(-DUSE_MYPLUGIN)
endif()
```

---

## 6. Entry Point Implementation

### For pure-Lua plugins (§1e)

```cpp
// myplugin-lua.cpp
#include "myplugin-lua.h"
#include <some_third_party_lib.h>

static int onDoThingLua(lua_State *lua) { /* ... */ return 1; }

static const luaL_Reg mylib[] = { { "doThing", onDoThingLua }, { NULL, NULL } };

int luaopen_myplugin(lua_State *lua)     { luaL_newlib(lua, mylib); return 1; }
int luaopen_libmyplugin(lua_State *lua)  { return luaopen_myplugin(lua); }
```

### For full PLUGIN-interface plugins

```cpp
// myplugin-lua.cpp
#include "myplugin-lua.h"
#include "myplugin-class-lua.h"   // declares registerClassMyPlugin + MY_PLUGIN
#include <plugin-helper/plugin-helper.h>

static int PLUGIN_IDENTIFIER       = 1;
static MY_PLUGIN *g_myPluginInst   = nullptr;   // singleton; see §2d

// Factory registered under registerClassMyPlugin:
static int onNewMyPluginLua(lua_State *lua)
{
    lua_settop(lua, 0);

    mbm::registerClassMyPlugin(lua);   // builds the luaL_newlib function table (left on stack)

    auto **udata   = static_cast<MY_PLUGIN **>(lua_newuserdata(lua, sizeof(MY_PLUGIN *)));
    MY_PLUGIN *that = new MY_PLUGIN();
    *udata          = that;
    g_myPluginInst  = that;

    mbm::plugin_stamp_userdata(lua, &PLUGIN_IDENTIFIER);   // applies _usertype_plugin metatable

    lua_rawseti(lua, -2, 1);   // table[1] = userdata

    const int index_plugin = lua_gettop(lua);
    mbm::plugin_doSubscribe(lua, index_plugin, "myplugin");   // subscribes; luaL_error on fail

    return 1;
}

// registerClassMyPlugin uses plugin_register_factory internally:
void mbm::registerClassMyPlugin(lua_State *lua)
{
    luaL_Reg factory[] = { {"new", onNewMyPluginLua}, {"__gc", onDestroyMyPluginLua}, {nullptr, nullptr} };
    mbm::plugin_register_factory(lua, "myplugin", "_mbmMyPlugin", factory);
}

int luaopen_myplugin(lua_State *lua)
{
    mbm::registerClassMyPlugin(lua);   // registers global factory + auto-creates instance
    return onNewMyPluginLua(lua);
}

int luaopen_libmyplugin(lua_State *lua) { return luaopen_myplugin(lua); }   // always delegate
```

---

## 7. What plugin-helper Provides

`plugins/plugin-helper/` is a **shared library** (`plugin_helper`) that all plugins link against. It provides:

### Plugin infrastructure helpers (use these — do NOT hand-roll)
```cpp
// Applies _usertype_plugin metatable to userdata at stack top; writes runtime id to *plugin_id.
void plugin_stamp_userdata(lua_State *lua, int *plugin_id);

// Calls mbm.doSubscribe(plugin_table) at index_plugin; cleans stack on success; luaL_error on fail.
void plugin_doSubscribe(lua_State *lua, int index_plugin, const char *plugin_name);

// Creates global factory table: luaL_newmetatable → luaL_setfuncs → lua_setglobal → lua_settop(lua,0).
void plugin_register_factory(lua_State *lua, const char *global_name, const char *metatable_name, const luaL_Reg *factory_methods);

// Extracts a PLUGIN_IDENTIFIER-stamped userdata (T**) from table[rawi]; luaL_error on mismatch.
void *plugin_check_userdata(lua_State *lua, int rawi, int indexTable, int plugin_id, const char *type_name);
```

### Engine-type userdata validation (for L_USER_TYPE_* based types)
```cpp
void *lua_check_userType(lua_State *lua, int rawi, int indexTable, L_USER_TYPE expectedType);
void *lua_get_userType_no_throw(lua_State *lua, int rawi, int indexTable, L_USER_TYPE expectedType);
```

### Renderizable extraction from Lua tables
```cpp
RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, int rawi, int indexTable);
RENDERIZABLE *getRenderizableNoThrowFromRawTable(lua_State *lua, int rawi, int indexTable);
ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, int rawi, int indexTable);
FX *getFxFromRawTable(lua_State *lua, int rawi, int indexTable);
```

### Lua error and logging
```cpp
void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...);
int  lua_error_debug(lua_State *lua, const char *format, ...);
int  errorLuaPushFalse(lua_State *lua, const char *msg);
void printStack(lua_State *lua, const char *fileName, unsigned int numLine);
```

### Table field extraction helpers
```cpp
void getFieldPrimaryFromTable(lua_State *lua, int indexTable, const char *fieldName, int LUA_TYPE, void *ptrRet);
void getFieldIntegerFromTable(lua_State *lua, int indexTable, const char *fieldName, int *ptrRet);
void getFieldUnsignedFromTable(lua_State *lua, int indexTable, const char *fieldName, uint32_t *ptrRet);
void getFieldUnsignedShortFromTable(lua_State *lua, int indexTable, const char *fieldName, unsigned short int *ptrRet);
void getFieldSignedShortFromTable(lua_State *lua, int indexTable, const char *fieldName, short int *ptrRet);
void getFieldUnsigned8FromTable(lua_State *lua, int indexTable, const char *fieldName, uint8_t *ptrRet);
void getFloat2FieldTableFromTable(lua_State *lua, int indexTable, const char *fieldNameTable, const char *field1, const char *field2, float *out1, float *out2);
```

### Array helpers
```cpp
void getArrayFloatFromTable(lua_State *lua, int index, float *out, uint32_t size);
void getArrayFromTablePixels(lua_State *lua, int index, uint8_t *out, uint32_t size);
void getArrayXYZ_FromTable(lua_State *lua, int index, std::vector<VEC3> &xyz_out);
void getArrayXYZ_noZ_FromTable(lua_State *lua, int index, std::vector<VEC3> &xyz_out);
void getArrayUintFromTable(lua_State *lua, int index, uint16_t *out, uint32_t size);
void get2ArrayFromTableWithField(lua_State *lua, int index, float *out, uint32_t size, const char *field_a, const char *field_b);
void get3ArrayFromTableWithField(lua_State *lua, int index, float *out, uint32_t size, const char *field_a, const char *field_b, const char *field_c);
```

### Dynamic variable / RENDERIZABLE variable access
```cpp
int getDynamicVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
int getVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
int setDynamicVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
int setVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
```

### Physics helpers
```cpp
int onSetPhysicsFromTableLua(lua_State *lua, int indexTable, INFO_PHYSICS *infoPhysicsOut);
int onSetPhysicsFromTableLuaToLineMesh(lua_State *lua, INFO_PHYSICS *infoPhysics, LINE_MESH *lineMesh);
```

**Include path:** `#include <plugin-helper/plugin-helper.h>`
**Link:** Variable `${PLUGIN_HELPER_LIBRARY}` (resolves to `plugin_helper`)

---

## 8. Include Patterns

```cpp
// Engine / core headers (angle-bracket, relative to include/)
#include <core_mbm/scene.h>
#include <core_mbm/device.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/plugin-callback.h>
#include <render/sprite.h>
#include <plugin-helper/plugin-helper.h>

// Lua (always in extern "C" block)
extern "C" {
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}
```

---

## 9. C++ Coding Conventions (Mandatory)

- **License block**: MIT license in box-drawing-character frame at top of every `.h` and `.cpp`
- **Include guards**: `#ifndef FOO_H` / `#define FOO_H` (not `#pragma once`)
- **Indentation**: 4 spaces (no tabs)
- **Pointer style**: `TYPE *varName` (space before `*`, not after)
- **Classes/structs**: `ALL_CAPS`
- **Methods**: `camelCase`
- **File names**: `kebab-case`
- **C++ standard**: C++17 (`noexcept` on constructors, `override` on virtuals, `static_cast<>` not C-style casts)
- **Logging**: Use `INFO_LOG()`, `ERROR_LOG()`, `ERROR_AT()` macros from `<core_mbm/util-interface.h>`

---

## 10. Build Command to Test

```sh
# Linux Debug — full feature build
mkdir -p build/linux_debug && cd build/linux_debug
cmake ../.. -DPLAT=Linux -DUSE_ALL=1 -DMBM_ENABLE_MESH_LEGACY_V7=1 \
            -DAUDIO=portaudio -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Or use the VS Code task: **Build mini-mbm (Linux Debug)**.

The plugin `.so` will be output to `bin/debug/linux_x86/myplugin.so`.

---

## 11. Lua Usage Pattern (for plugin consumers)

```lua
local myplugin = require "myplugin"   -- loads myplugin.so / myplugin.dll
-- or
local myplugin = require "libmyplugin"

-- The luaopen_ function returns true (boolean pushed in entry point)
-- The actual API is registered on the mbm namespace or a returned table
```

---

## 12. Real Examples to Reference

| Plugin | Path | Style | Notes |
|--------|------|-------|-------|
| tiny_obj_loader | [modules/obj_importer_lua/](../../../modules/obj_importer_lua/) | Pure Lua library (§1e) | `luaL_newlib` — no PLUGIN interface, parses OBJ files |
| box2d (simplest) | [plugins/box2d/](../../../plugins/box2d/) | Full PLUGIN | Physics — wraps third-party lib |
| tiled (no third-party) | [plugins/tiled/](../../../plugins/tiled/) | Full PLUGIN | Inlines plugin-helper sources directly instead of linking |
| steam | [plugins/steam/](../../../plugins/steam/) | Full PLUGIN | Process-lifetime init guard (`g_steamGlobalReady`) — lifecycle §2 |
| imGui (complex) | [plugins/imGui/](../../../plugins/imGui/) | Full PLUGIN | Backend selection, Metal `.mm`, release/debug source exclusion |
| plugin-helper | [plugins/plugin-helper/](../../../plugins/plugin-helper/) | — | Shared helper library — not a Lua plugin itself |

---

## 13. Checklist

**Full PLUGIN interface (engine hooks needed):**
- [ ] `plugins/myplugin/CMakeLists.txt` — project uses `${MYPLUGIN_LIBRARY}`, has `set(CMAKE_SHARED_LIBRARY_PREFIX "")`, iOS uses `STATIC`
- [ ] `plugins/myplugin/myplugin-lua.h` — unique export macro, `extern "C"` Lua includes, both `luaopen_` signatures
- [ ] `plugins/myplugin/myplugin-lua.cpp` — both entry points call same `registerClass*` function and `return 1`
- [ ] Root `CMakeLists.txt` — `MYPLUGIN_LIBRARY` name set, `MYPLUGIN_LUA_LIB` path set, `USE_MYPLUGIN` default = 0, added to `USE_ALL` block, `add_subdirectory` under plugins section
- [ ] `src/CMakeLists.txt` — Android link workaround block added, iOS static link block added, `-DUSE_MYPLUGIN` definition added for both Android and iOS
- [ ] MIT license header in every new file
- [ ] Include guards (not `#pragma once`) in every new header
- [ ] Plugin links `${PLUGIN_HELPER_LIBRARY}`, `${LUA_LIBRARY}`, `${CORE_MBM_LIBRARY}` (non-iOS only)
- [ ] Build tested with `make -j$(nproc)` or VS Code build task

**Pure Lua library (no engine hooks — §1e):**
- [ ] `luaopen_` uses `luaL_newlib(lua, mylib)` and returns 1 (the table, not `true`)
- [ ] CMakeLists only links `${LUA_LIBRARY}` — no `core_mbm`, no `plugin_helper`
- [ ] No `PLUGIN_IDENTIFIER`, no `PLUGIN` inheritance, no `mbm::registerClass*`
- [ ] `require "myplugin"` in Lua assigns the returned table directly (e.g. `local mp = require "myplugin"`)
