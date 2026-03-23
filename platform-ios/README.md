# iOS Platform — Architecture Notes

## Build modes

The iOS target supports two build modes, selected at CMake configure time:

| CMake flag | Engine path | Entry point |
|---|---|---|
| `-DUSE_LUA=1` | Lua scripted | `main-lua.mm` → `MetalViewController.mm` (Lua path) |
| _(no flag)_ | Pure C++ | `main-lua.mm` → `MetalViewController.mm` (C++ path) + `my-scene.mm` |

In both cases `UIApplicationMain` (in `main-lua.mm`) drives the app lifecycle. All engine
initialisation happens in `MetalViewController viewDidLoad`.

The preprocessor symbol `USE_LUA` (defined by CMake when `-DUSE_LUA=1`) guards the two
paths inside `MetalViewController.mm`:

```objc
#ifdef USE_LUA
    // creates mbm::LUA_MANAGER, calls initializeSceneLua + run()
#else
    // creates GAME (see my-scene.h/mm), calls initGraphics()
#endif
```

---

## Why all plugins are compiled as STATIC on iOS

On Linux and macOS the engine plugins (`ImGui`, `box2d`, `bullet3d`, etc.) are built as
**shared libraries** (`.dylib` / `.so`).  On iOS they are forced to **static archives**
(`.a`).

Apple's App Store sandbox policy forbids bare `.dylib` files inside an `.app` bundle unless
they are wrapped in a proper `.framework` bundle **and** embedded + code-signed.  For a
single-app game there is no practical benefit to the extra complexity, so the simpler and
universally accepted approach is to link everything statically into the main executable.

Consequences of static linking on iOS:

* All plugin object code is merged into the single `mini-mbm` executable.
* Plugin `target_link_libraries` declarations are wrapped in `if (NOT iOS)` guards in each
  plugin's `CMakeLists.txt`.  This is necessary because static library dependencies
  propagate to the consumer's linker command even when marked `PRIVATE` — omitting the
  guard would cause the linker to see each library listed twice and emit
  `ld: warning: ignoring duplicate libraries`.
* `box2d` and `box2d-liquid-fun` share large amounts of object code (`b2World`, `b2Body`,
  …) and cannot both be linked into the same binary.  The root `CMakeLists.txt` detects
  this combination on iOS and automatically disables `USE_BOX2D_LIQUID_FUN` with a
  warning.  To use liquid-fun instead, pass `-DUSE_BOX2D=0 -DUSE_BOX2D_LIQUID_FUN=1`
  explicitly (do not use `-DUSE_ALL=1`).

---

## Dynamic frameworks — when they *would* be useful

A `.framework` bundle can contain either a static archive or a dynamic library.  Apple
allows dynamic frameworks in App Store apps under one condition: the framework must be
**embedded** inside `YourApp.app/Frameworks/` and signed with your team identity.

This would only be worth the extra complexity if:

* The app has **extensions** (Share Extension, Today Widget, Keyboard Extension, etc.)
  that need to share the same library code — embedding one framework is smaller than
  statically linking the same code twice.
* You want to ship a **pre-built SDK** so that a third party can drop in a
  `.framework` without recompiling the engine.

For a straightforward game with a single app target the static approach is simpler,
requires no per-library code signing, and produces a smaller binary overall (no
dynamic-loader overhead).

---

## Game assets

Library type (static vs. dynamic) has no effect on how assets are distributed.  Lua
scripts, textures, sprite binaries, mesh binaries, and any other data files are placed
in the **app bundle's resource directory** at build time and located at runtime via
`NSBundle`:

```objc
NSString* resourcePath = [[NSBundle mainBundle] resourcePath];
// → e.g. /var/containers/Bundle/…/mini-mbm.app/
```

This path is already passed to the engine as `--addPath` inside `MetalViewController
viewDidLoad` (Lua path), and the engine's `util::addPath` / `DEVICE::addPath` API
accepts it for the pure C++ path.

The resulting app bundle structure:

```
mini-mbm.app/
  mini-mbm          ← executable (engine + all static libs merged in)
  Info.plist
  main.lua          ← Lua entry point (if USE_LUA)
  assets/
    player.spr      ← sprite binary (produced by sprite_maker tool)
    level1.msh      ← mesh binary   (produced by mesh editor)
    ui.png          ← texture
    …
```

### Copying assets — no CMake changes required per game

You do **not** need to modify any CMake file for each new game.  The build system adds a
single post-build command (once, in `src/CMakeLists.txt`) that copies an entire
conventional `assets/` folder into the bundle every time you build:

```cmake
# In src/CMakeLists.txt — added once, works for every game
add_custom_command(TARGET mini-mbm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_BUNDLE_CONTENT_DIR:mini-mbm>/assets"
    COMMENT "Copying game assets into app bundle"
)
```

The workflow per game is then identical to Android:

1. Place your files under `<repo>/assets/` (Lua scripts, textures, sprites, meshes, …).
2. Build normally — the post-build step copies everything automatically.
3. No CMake edits needed.

The engine locates assets at runtime because `MetalViewController viewDidLoad` already
passes `[[NSBundle mainBundle] resourcePath]` as `--addPath`, which resolves to the
bundle's root.  From Lua you reference files relatively:

```lua
local spr = mbm.newSprite("assets/player.spr")
```

> **Manual copy (quick iteration):** You can also just copy files directly into the
> `.app` directory after building — on macOS/iOS simulator it is a plain directory:
> ```sh
> cp -r assets/ bin/release/iphoneos_arm64/mini-mbm.app/assets/
> ```
> For device deployment the `.app` is re-signed anyway by Xcode/`xcodebuild`, so this
> works fine during development.

---

## ARC (Automatic Reference Counting)

All Objective-C/Objective-C++ source files under `platform-ios/` are compiled with
`-fobjc-arc`.  This is set in `src/CMakeLists.txt` via
`set_source_files_properties(… PROPERTIES COMPILE_FLAGS "-fobjc-arc")`.

With ARC active:
* `[super dealloc]` must **not** be called explicitly — the compiler inserts it.
* Manual `retain`/`release`/`autorelease` calls are forbidden.
* C++ `delete` for non-ObjC pointers (e.g. `delete s_game`) is still required and valid.
