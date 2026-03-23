# iOS Platform — Architecture Notes

## Build generators: `make` vs `-G Xcode`

Two CMake generators are supported for iOS, serving different purposes:

| Generator | Command | Use case |
|---|---|---|
| `make` (default) | `cmake .. -DPLAT=iOS …` | Fast compile check, CI pipelines — produces a plain unsigned binary |
| Xcode | `cmake .. -DPLAT=iOS … -G Xcode` | Device/simulator deployment — produces a signed `.xcodeproj` |

The `make` generator cross-compiles for ARM64 via `xcrun` and produces a valid
`mini-mbm.app` bundle with all assets, but the binary is **unsigned** and cannot be
installed on a device.  It is useful for verifying the build compiles cleanly.

The **Xcode generator** is required whenever you want to:
- Run on a physical device (requires a signed `.ipa`)
- Launch in the iOS Simulator (Xcode selects the right arch automatically)
- Use Xcode's debugger, Instruments, or crash symbolication

```sh
# Xcode project — generated once, rebuilt by Xcode thereafter
mkdir -p build/ios_xcode && cd build/ios_xcode
cmake ../.. \
    -DPLAT=iOS -DUSE_LUA=1 -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=avfoundation \
    -DGAME_BUNDLE_ID=com.yourcompany.yourgame \
    -DGAME_NAME="My Game" \
    -DGAME_ASSETS_DIR=/path/to/your/game/assets \
    -G Xcode
open "My Game.xcodeproj"   # name matches -DGAME_NAME; Signing & Capabilities → set Team → ⌘R
```

You can keep **two separate build directories** — one `make` dir for fast checks, one
`Xcode` dir for deployment.  They share the same source tree and CMake flags.

---

## Build modes (Lua vs. pure C++)

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

### Separate game repositories

The engine repo and a game repo are independent.  A typical layout:

```
~/mini-mbm/          ← engine repo (this repo)
~/tower-defense/     ← game repo (separate, not part of the engine)
  assets/
    main.lua
    sprites/
    meshes/
  docs/
  README.md
  …
```

Three CMake variables let you point the engine build at a specific game without touching
any CMake file:

| Variable | Default | Purpose |
|---|---|---|
| `-DGAME_BUNDLE_ID` | `com.mini.mbm.mini-mbm` | iOS App Store bundle identifier |
| `-DGAME_NAME` | `mini-mbm` | Display name shown on the device home screen |
| `-DGAME_ASSETS_DIR` | _(not set — no copy)_ | Absolute path to the game's asset folder |

Example configure command for Tower Defense:

```sh
mkdir -p build/ios_towerdefense && cd build/ios_towerdefense
cmake ../.. \
    -DPLAT=iOS \
    -DUSE_LUA=1 \
    -DAUDIO=avfoundation \
    -DGAME_BUNDLE_ID=com.mini.mbm.towerdefense \
    -DGAME_NAME="Tower Defense" \
    -DGAME_ASSETS_DIR=/Users/michel/tower-defense/assets
make -j$(sysctl -n hw.logicalcpu)
```

What each variable does under the hood:

* **`GAME_BUNDLE_ID` / `GAME_NAME`** — CMake runs `configure_file` on
  `platform-ios/Info.plist.in`, substituting `@IOS_BUNDLE_ID@` and `@IOS_APP_NAME@`,
  and writes the result to the build directory.  The `Info.plist.in` template is the
  *only* file you need to look at if you want to add custom plist keys (e.g. required
  device capabilities, privacy strings).

* **`GAME_ASSETS_DIR`** — a `POST_BUILD` command copies the entire directory tree into
  `mini-mbm.app/assets/` after every successful build.  Only the declared path is
  copied; the rest of the game repo (docs, READMEs, scripts, …) is ignored
  automatically.  You never need to edit any CMake file between games.

The engine locates assets at runtime because `MetalViewController viewDidLoad` already
passes `[[NSBundle mainBundle] resourcePath]` as `--addPath`, which resolves to the
bundle root.  From Lua you reference files with a path relative to that root:

```lua
local spr = mbm.newSprite("assets/sprites/player.spr")
```

> **Manual copy (quick iteration):** You can also copy files directly into the `.app`
> after building — it is a plain directory on disk:
> ```sh
> cp -r ~/tower-defense/assets \
>        bin/release/iphoneos_arm64/mini-mbm.app/assets/
> ```
> The app is re-signed on device deployment anyway, so this is safe during development.

---

## ARC (Automatic Reference Counting)

All Objective-C/Objective-C++ source files under `platform-ios/` are compiled with
`-fobjc-arc`.  This is set in `src/CMakeLists.txt` via
`set_source_files_properties(… PROPERTIES COMPILE_FLAGS "-fobjc-arc")`.

With ARC active:
* `[super dealloc]` must **not** be called explicitly — the compiler inserts it.
* Manual `retain`/`release`/`autorelease` calls are forbidden.
* C++ `delete` for non-ObjC pointers (e.g. `delete s_game`) is still required and valid.
