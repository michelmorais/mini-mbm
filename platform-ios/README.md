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
    -DPLAT=iOS -DUSE_LUA=1 \
    -DAUDIO=avfoundation \
    -DGAME_BUNDLE_ID=com.yourcompany.yourgame \
    -DGAME_NAME="My Game" \
    -DGAME_ASSETS_DIR=/path/to/your/game/assets \
    -DGAME_ICON_PNG=/path/to/your/icon-1024.png \
    -G Xcode
open "My Game.xcodeproj"   # name matches -DGAME_NAME; Signing & Capabilities → set Team → ⌘R
```

> **No Apple ID? Use the Simulator.**  In Xcode pick any **iPhone** simulator from the
> device dropdown at the top (instead of a physical device) and press **⌘R** — no signing
> or Apple ID required.  To deploy to a real device you need a free Apple ID
> (apple.com → Create Apple ID, no credit card needed); the provisioning profile expires
> after 7 days and is renewed automatically on the next ⌘R.

You can keep **two separate build directories** — one `make` dir for fast checks, one
`Xcode` dir for deployment.  They share the same source tree and CMake flags.

---

## Placing the Xcode project outside the engine repo

The build directory can live anywhere on disk — it does not have to be inside
`mini-mbm/`.  This is useful when each game lives in its own repository: you keep
the engine in one place and generate a per-game `.xcodeproj` inside the game repo.

```
~/mini-mbm/                  ← engine repo (shared, never edited per game)
~/tower-defense/             ← game repo
    assets/                  ← game assets
    ...
~/tower-defense-ios_xcode/   ← generated Xcode project (sibling dir, add to .gitignore)
```

```sh
# From inside your game repo — point cmake at the engine source
mkdir -p ~/tower-defense-ios_xcode && cd ~/tower-defense-ios_xcode
cmake ~/mini-mbm \
    -DPLAT=iOS -DUSE_LUA=1 \
    -DAUDIO=avfoundation \
    -DGAME_BUNDLE_ID=com.mini.mbm.tower-defense \
    -DGAME_NAME="Tower Defense Monster" \
    -DGAME_ASSETS_DIR=/Users/michel/tower-defense/assets \
    -DGAME_ICON_PNG=/Users/michel/tower-defense/propaganda/1024x1024-icon.png \
    -G Xcode
# The configure message prints the exact open command:
#   open "/Users/michel/tower-defense-ios_xcode/Tower Defense Monster.xcodeproj"
# Nothing is written to the game repo — only assets and the icon are copied.
```

> **Use an absolute path for `GAME_ASSETS_DIR`.**  CMake does not expand `~` in `-D`
> values when the path is quoted (e.g. `"-DGAME_ASSETS_DIR=~/…"`) — use the full
> path (`/Users/michel/…`) or the shell variable `$HOME` instead.

**What lands where:**

| Artifact | Location | Notes |
|---|---|---|
| `.xcodeproj` | `~/tower-defense-ios_xcode/` | Sibling dir — add to `.gitignore` |
| Compiled `.app` | Xcode Derived Data | Managed by Xcode, no action needed |
| `libcore_mbm.a` etc. | `~/mini-mbm/libs/release/…` | Engine artifacts — expected there |
| `platform-ios/Info.plist` | `~/mini-mbm/platform-ios/` | Generated file — already gitignored |
| `Assets.xcassets/AppIcon.appiconset/` | `platform-ios/Assets.xcassets/` | Populated by CMake at configure time from `GAME_ICON_PNG` |
| `flatten_icon.swift` | `platform-ios/` | Static helper script — composites alpha PNG onto white; called by CMake |
| `my-commands.m` | `platform-ios/` (engine repo only) | Built-in stub — no copy to the game repo |

Add `tower-defense-ios_xcode/` (or whatever sibling name you choose) to your
`.gitignore` at the parent level — it is a generated build directory and should
not be committed.  Alternatively, since it sits outside the game repo entirely,
it is simply not tracked by git at all.

---

## App icon

**Will the icon appear correctly on the App Store?**  
Yes. The same `actool_icon.sh` POST_BUILD script runs for both simulator and device
builds. It uses `$PLATFORM_NAME` (set by Xcode to `iphonesimulator` or `iphoneos`)
so actool always compiles for the right platform. When you archive and upload to
App Store Connect, the `Assets.car` and `CFBundleIcons` entries are already in the
bundle — Apple's servers use them directly. The 1024×1024 marketing icon you upload
in App Store Connect is a separate field and does not need to be in the bundle.

**Where to see the icon in Xcode:**  
In Xcode’s Project Navigator (**↘1** or **View → Navigators → Project**), look for
`Assets.xcassets` inside the `platform-ios` group. Click it to open Xcode’s
Asset Catalog editor — you will see the **AppIcon** set with your 1024×1024 image.  
After building, `Assets.car` (the compiled catalog) is visible in the `.app` bundle
under **Products** in the Project Navigator.

> If `Assets.xcassets` does not appear in the navigator, re-run cmake (it is added
> via `target_sources` at configure time) and then close/reopen the `.xcodeproj`.

Pass one of the two CMake flags at configure time:

| Flag | Description |
|---|---|
| `-DGAME_ICON_PNG=/path/to/icon-1024.png` | Single 1024×1024 PNG — CMake composites onto white and populates `platform-ios/Assets.xcassets/` |
| `-DGAME_ICON_DIR=/path/to/Assets.xcassets` | Pre-built asset catalog — `AppIcon.appiconset` is copied into `platform-ios/Assets.xcassets/` |

**`GAME_ICON_PNG` (recommended for most games):**  
Provide a 1024×1024 PNG. CMake:
1. Runs `platform-ios/flatten_icon.swift` (CoreGraphics/ImageIO) to composite the image onto a white background — removes any alpha that would make the icon invisible on the home screen.
2. Writes the opaque PNG + `Contents.json` into `platform-ios/Assets.xcassets/AppIcon.appiconset/` (source tree).
3. Adds a POST_BUILD script (`actool_icon.sh`) that compiles the catalog and merges `CFBundleIcons` into the bundle’s `Info.plist`.

```sh
cmake ~/mini-mbm ... -DGAME_ICON_PNG=/Users/michel/tower-defense/icon-1024.png -G Xcode
```

> **Source PNG with transparency is OK** — CMake composites it over white automatically.  
> **Use absolute paths** — CMake does not expand `~` inside `-D` values.

**`GAME_ICON_DIR` (advanced):**  
Point to an existing `Assets.xcassets` that contains an `AppIcon.appiconset`. CMake copies
the appiconset into `platform-ios/Assets.xcassets/` and registers the same POST_BUILD
actool phase.

```sh
cmake ~/mini-mbm ... -DGAME_ICON_DIR=/Users/michel/tower-defense/Assets.xcassets -G Xcode
```

**How it works (implementation notes):**

The icon catalog lives in `platform-ios/Assets.xcassets/` (source tree). CMake cannot
place `.xcassets` directories into Xcode's `PBXResourcesBuildPhase` — all three
mechanisms fail:

| CMake approach | Result |
|---|---|
| `target_sources` | PBXBuildFile created but assigned to no build phase |
| `MACOSX_PACKAGE_LOCATION ""` | ends up in `PBXCopyFilesBuildPhase` — actool not called |
| `RESOURCE` target property | directory silently omitted from the project |

The solution is a **POST_BUILD shell script** ("CMake PostBuild Rules" in the Xcode build
log). POST_BUILD fires _before_ Xcode's code-signing step, so the bundle is correctly
signed after actool writes `Assets.car` + the `CFBundleIcons` plist entries. The script:
1. Runs `xcrun actool --compile $BUILT_PRODUCTS_DIR/$WRAPPER_NAME …`
2. Writes a partial plist to a temp file
3. Merges `CFBundleIcons` into the bundle's `Info.plist` via `PlistBuddy`
4. Removes the temp file

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
mkdir -p ~/tower-defense-ios_xcode && cd ~/tower-defense-ios_xcode
cmake ~/mini-mbm \
    -DPLAT=iOS -DUSE_LUA=1 \
    -DAUDIO=avfoundation \
    -DGAME_BUNDLE_ID=com.mini.mbm.tower-defense \
    -DGAME_NAME="Tower Defense Monster" \
    -DGAME_ASSETS_DIR=~/tower-defense/assets \
    -G Xcode
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
