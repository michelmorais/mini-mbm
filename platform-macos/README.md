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
    -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.logicalcpu)
```

Equivalently via `cmake --build`:

```sh
cmake -B build \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

### Release build

```sh
mkdir -p build && cd build
cmake .. \
    -DPLAT=MacOs \
    -DUSE_ALL=1 \
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

### Optional Metal skeletal numeric parity test

On a macOS machine or CI runner with an available Metal GPU and graphical session, build
`testLib` and run the native RGBA8 skeletal readback suite from the repository root:

```sh
cmake --build build/macos_debug --target testLib -j$(sysctl -n hw.logicalcpu)
MTL_DEBUG_LAYER=1 ./bin/debug/arm64/testLib --metal-skeletal-parity-test
```

The command exercises the shared synthetic and Lorekeeper LBS/DQS cases through the production
Metal deformation source. All four cases must report `PASS`, the process must exit with status zero,
and any Metal API validation message must be treated as a test failure. This test is optional for
build-only runners because Metal device creation is unavailable without GPU access.

### Automated Metal, window, and input tests

Two additional graphical-session tests cover the macOS render-target readback path and window
resize state:

```sh
MTL_DEBUG_LAYER=1 ./bin/debug/arm64/testLib --metal-render-to-texture-test
MTL_DEBUG_LAYER=1 ./bin/debug/arm64/testLib --macos-resize-test
MTL_DEBUG_LAYER=1 ./bin/debug/arm64/testLib --macos-input-test
MTL_DEBUG_LAYER=1 ./bin/debug/arm64/testLib --macos-close-test
```

The render-target test validates known RGBA pixels after a Metal offscreen clear and GPU readback.
The resize test requests a 960x640 logical content area and verifies the engine resize callback,
the `CAMetalLayer` backing scale, and its pixel-sized drawable. The input test posts deterministic
AppKit events and verifies a normal key, Shift, Control, Option, Command, Caps Lock state,
three mouse buttons, movement, both scroll directions, double-click, and logical coordinates. The
close test exercises the window delegate and normal engine teardown. All commands exit
automatically; any Metal API validation message or nonzero process status is a failure.

The resize harness validates the current display's scale. Moving a live window between displays
with different backing scales, native fullscreen transitions, and multi-monitor placement still
require manual checks because CI cannot synthesize the physical display topology.

### Manual window/input acceptance checklist

Run a Debug build with `MTL_DEBUG_LAYER=1` and verify the following on real hardware:

- Hold and combine Shift, Control, Option, and Command; each press and release must arrive once.
- Toggle Caps Lock twice and confirm text/input code observes the enabled and disabled states.
- Drag with left, right, and an additional mouse button; verify motion remains in logical window
  coordinates on Retina displays. Test both discrete-wheel and trackpad scrolling.
- Resize continuously, minimize and restore, then enter and leave native fullscreen. Rendering must
  resume at the correct size without flicker, stretched frames, or Metal validation messages.
- With two displays, move the window between them. When their backing scales differ, the drawable
  pixel size must update while the engine's logical dimensions and pointer coordinates stay stable.
- Close through the title-bar button, Command-Q, and `mbm.quit()`. Each path must return normally
  without a crash, hang, Metal validation message, or audio/plugin callback after teardown.

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

### Current AVFoundation limitations

- Volume, pan, play, pause, resume, stop, looping, and end-of-stream notification are implemented.
- Pitch changes are not implemented; `setPitch()` returns failure and `getPitch()` returns `1.0`.
- Seeking is not implemented; `setPosition()` returns failure. Use `stop()` followed by `play()` to
  restart from the beginning.
- `getLength()` currently reports the decoded frame count, not milliseconds. Do not use it as a
  duration in milliseconds until that API contract is corrected.

---

## Launching the Editor Tools

After a build with `-DUSE_ALL=1`, launch the engine without arguments to open the
**launcher dialog** listing all built-in Lua editors:

```sh
./bin/debug/arm64/mini-mbm
```

Or launch a specific editor directly:

```sh
./bin/debug/arm64/mini-mbm --scene editor/sprite_maker.lua
./bin/debug/arm64/mini-mbm --scene editor/scene_editor2d.lua
./bin/debug/arm64/mini-mbm --scene editor/font_maker.lua
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
    -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.logicalcpu)
```

---

## Local CMake Bundle Prototype — .app and .dmg

### Supported workflow policy

CMake is used for local development, builds, and automated engine tests on macOS. It is **not**
the release-packaging workflow. Distribution is performed through Xcode: create the application
archive there, then use Xcode's distribution workflow for Developer ID or Mac App Store export,
signing, and submission as appropriate. A game built with Mini-MBM has already been released using
this Xcode workflow.

The available paths therefore have different roles:

| Workflow | Status |
|---|---|
| Xcode project and Archive/Distribute workflow | Required for distribution; proven in production for an existing game and its configuration |
| CMake/Makefile (`PLAT=MacOs`) | Supported for local development and automated tests |
| CMake bundle helpers (`GAME_ASSETS_DIR`, `macapp`, `macdmg`) | Local prototype/diagnostic output only; not a supported release artifact |
| CMake-generated Xcode project with `MAS_DELIVERY=1` | Project-generation helper; final Archive, signing, validation, and distribution still happen in Xcode |

> **Important - current CMake standard-delivery limitation:** the generated `.app` copies the
> engine executable and `distribution.dylib`, but does not yet copy all runtime libraries and Lua
> plugin `.dylib` files. The executable also retains the build-tree `LC_RPATH`. Consequently, do
> not treat this CMake-generated bundle as portable or ready for signing/notarization until its
> dependencies use bundle-relative install names (for example,
> `@executable_path/../Frameworks`) and all required libraries are embedded. This limitation does
> not apply to the separate Xcode release workflow.

The normal Makefile build also inherits the active SDK's deployment target unless
`CMAKE_OSX_DEPLOYMENT_TARGET` is supplied. This matters when using a local CMake binary for
compatibility testing, but it does not define the release policy: configure the deployment target
and architectures in the Xcode project used to archive the game. The current CMake output layout
is `arm64`, and it does not produce an Intel or universal binary automatically.

The macOS build contains a per-game packaging path via the same `-DGAME_ASSETS_DIR`
workflow as Linux (AppImage) and Windows (GameDir). The build produces a
**`.app` bundle skeleton** automatically after `make`. An optional
`make macdmg` step wraps it into a **`.dmg`** disk image. Creating the disk image does not by itself
make the incomplete bundle portable or ready for distribution.

### How it works

Assets are packed into an AES-128-CBC encrypted SQLite archive (`.asset` file)
by the bundled `distribution` tool. At launch, the engine extracts the archive
to a temporary directory, loads `main.lua`, then cleans up on exit. The
`distribution.dylib` runtime library is bundled inside `Contents/Frameworks/`.

### Local CMake bundle flags

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

### Troubleshooting `macdmg`

If `make macdmg` fails with:

```text
cp: /dev/disk...: Not a directory
```

you are likely using an older generated `make_macdmg.sh` that parses
`hdiutil` output and may resolve a device path instead of a mount directory.

This repository now generates a robust script that uses an explicit mountpoint.
Re-run CMake to regenerate the script, then run `make macdmg` again:

```sh
cmake ~/mini-mbm \
    -DPLAT=MacOs \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DCMAKE_BUILD_TYPE=Release \
    -DGAME_NAME="Tower Defense Monster" \
    -DGAME_ASSETS_DIR=/Users/michel/tower-defense/assets \
    -DGAME_ASSETS_PASSWORD=mysecret \
    -DGAME_ICON_PNG=/Users/michel/tower-defense/propaganda/1024x1024-icon.png
make macdmg
```

### No release from this CMake output

Do not sign, notarize, upload, or ship the `.app`/`.dmg` produced by these Make targets. They exist
for local inspection of the asset-packing and bundle-assembly logic. Use the game's Xcode project
for all release artifacts. In Xcode, verify the deployment target, architectures, embedded runtime
libraries, signing team, entitlements, hardened runtime, and archive contents before selecting
**Distribute App**.

### CMake status messages

When delivery is configured, CMake prints:

```
-- macOS game name   : Tower Defense Monster
-- macOS assets dir  : /Users/michel/tower-defense/assets
-- macOS .app bundle : /Users/michel/tower-defense-macos/Tower_Defense_Monster.app
```

---

## Mac App Store Xcode Project Helper (`-DMAS_DELIVERY=1`)

This CMake option generates an Xcode project and prepares its targets/resources. It does not make
CMake the distribution tool: open the generated project in Xcode and use **Product -> Archive** and
**Distribute App** for the actual release workflow.

### Monitor / Resolution / Fullscreen Launcher

On every launch the engine presents a small **startup dialog** (before the game
window opens) where the player can choose:

- **Monitor** — any connected display; particularly useful for multi-monitor
  setups
- **Resolution** — resolutions that fit the selected monitor (common presets +
  native); defaults to 1920 × 1080 on first run
- **Full Screen** — checked by default; when enabled, the window covers the
  selected monitor without a title bar

The choice is remembered in `NSUserDefaults` inside the App Sandbox container, so
the player's preference is restored on the next launch without prompting.

This matches the industry standard for macOS/PC games (Unity, Unreal Engine, and
SDL-based titles all display an equivalent launcher on startup).  On first run
the dialog defaults to **full screen on the primary monitor** — the most common
expectation for a purchased App Store game.

This mode produces an Xcode project suitable for archiving and submitting to the
**Mac App Store**.  Assets are embedded directly in the `.app` bundle; no
`.asset` archive extraction happens at runtime.  All plugins are linked
statically so no user-space dylibs are loaded (required by the App Store sandbox).

### Requirements

- Xcode installed (not just the command-line tools)
- An Apple Developer account enrolled in the **Mac App Store** program
- An **App Store Distribution** certificate in your keychain (not *Developer ID*)
- App record created in App Store Connect with the matching Bundle ID

### Configure (Xcode generator only)

```sh
mkdir -p ~/tower-defense-mas && cd ~/tower-defense-mas
cmake ~/mini-mbm \
    -G Xcode \
    -DPLAT=MacOs \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMAS_DELIVERY=1 \
    -DMAS_BUNDLE_ID=com.mini.mbm.tower-defense \
    -DMAS_APP_NAME="Tower Defense Monsters" \
    -DGAME_ASSETS_DIR=/Users/michel/tower-defense/assets \
    -DGAME_ICON_PNG=/Users/michel/tower-defense/propaganda/1024x1024-icon.png \
    -DCMAKE_BUILD_TYPE=Release
```

| Flag | Required | Description |
|---|---|---|
| `-DMAS_DELIVERY=1` | yes | Activates App Store delivery mode |
| `-DMAS_BUNDLE_ID=...` | yes | Bundle ID registered in App Store Connect |
| `-DMAS_APP_NAME=...` | no | Display name (defaults to `GAME_NAME` or `mini-mbm`) |
| `-DGAME_ASSETS_DIR=...` | recommended | Assets folder to embed in the bundle |
| `-DGAME_ICON_PNG=...` | recommended | 1024×1024 PNG icon |

### What the build system does

1. Sets `main-lua-mas.mm` as the entry point — uses `NSBundle` to locate
   `Contents/Resources/assets/` at runtime.
2. Generates `platform-macos/Info.plist` from `Info.plist.in` with your bundle
   ID and app name substituted.
3. Sets `MACOSX_BUNDLE TRUE` and all required `XCODE_ATTRIBUTE_*` properties
   (code-sign entitlements, `INSTALL_PATH`, `SKIP_INSTALL NO`, deployment target
   12.0, App Icon asset catalog name).
4. Copies all files from `GAME_ASSETS_DIR` into
   `Contents/Resources/assets/` (preserving directory structure).
5. Runs `sips` to generate all required macOS icon sizes and writes an
   `Assets.xcassets/AppIcon.appiconset/Contents.json` for Xcode to compile.
6. Statically links all enabled plugins (ImGui, Box2D, Bullet, lsqlite3, Tiled)
   into the executable — no dylib loading at runtime.
7. Wires `platform-macos/mini-mbm.entitlements` for App Sandbox compliance.

### Archive and upload (Xcode UI)

```sh
open mini-mbm.xcodeproj   # opens Xcode
```

Then in Xcode:

1. Select the **mini-mbm** scheme and **Any Mac** as destination.
2. Set the signing team: *Signing & Capabilities → Team*.
3. **Product → Archive** — builds a Release archive.
4. **Distribute App → App Store Connect → Upload**.

Command-line packaging/upload recipes are intentionally not documented as a supported release
path. Use Xcode's Archive organizer so the game's signing, entitlements, embedded content, and
distribution method are reviewed together.

### Entitlements (`platform-macos/mini-mbm.entitlements`)

The provided entitlements file enables the App Sandbox, read-only user-selected file access, and
outbound networking. Review it before submission and remove capabilities the game does not need;
add any other application-specific capabilities explicitly.

```xml
<key>com.apple.security.network.client</key>
<true/>   <!-- currently enabled; remove if the game does not need networking -->
```

### Important differences from the local CMake prototype

| | Local CMake prototype (`GAME_ASSETS_DIR`) | Xcode/Mac App Store (`MAS_DELIVERY`) |
|---|---|---|
| Release status | Not supported for distribution | Archive and distribute through Xcode |
| Asset delivery | `.asset` archive extracted to temp dir | Embedded in bundle |
| Plugin linking | SHARED `.dylib` loaded via `require` | STATIC, linked into binary |
| Signing/notarization | Not performed from this prototype | Managed by the selected Xcode distribution workflow |
| Entry point | `main-lua-delivery.cpp` | `main-lua-mas.mm` |
| Generator | Makefile | **Xcode only** |

### Persistent, tamper-resistant save files

#### Why save location matters in MAS mode

Inside the App Sandbox, `mbm.getPathEngine()` returns a path that points into
or near the read-only bundle, and the temporary directory that is used to stage
assets is **cleaned up on exit**.  Any progress stored there is lost the next
time the player opens the game.

The correct persistent location is **`Application Support`**:

```
~/Library/Containers/<BundleID>/Data/Library/Application Support/<AppName>/
```

The sandbox remaps the familiar `~/Library/Application Support/<AppName>/`
automatically.  This directory survives app restarts, system reboots, and OS
updates.  No extra entitlement is required to write there.

#### How to get the save directory from Lua

All delivery entry points (`main-lua-delivery.cpp` on Linux and macOS,
`main-lua-mas.mm` for MAS, `main-mingw-delivery.cpp` on Windows) expose the
persistent save directory through the same native command:

```lua
-- Returns the persistent writable directory for save files (no trailing slash).
-- Works on Linux, macOS (delivery and MAS), and Windows delivery builds.
-- Returns "" when running outside a delivery build (development mode).
local save_dir = mbm.doCommands('get_save_dir')
```

The call creates the directory if it does not exist and returns the absolute
path without a trailing slash.

| Platform | Directory returned |
|---|---|
| Linux AppImage | `$GAME_SAVE_DIR` or `~/.local/share/<GameName>` |
| macOS delivery | `~/Library/Application Support/<GameName>` |
| macOS MAS | Sandbox-mapped Application Support directory |
| Windows delivery | `%APPDATA%\<GameName>` |
| Development | _(empty string — CWD fallback)_ |

#### Recommended save/load pattern with encryption

Storing save data as plain Lua source (e.g. a sequence of `mbm.setGlobal()`
calls) makes it trivially editable by the player.  `mbm.encrypt` / `mbm.decrypt`
(available when built with `-DUSE_ALL=1`) apply AES-128-CBC encryption.  The
recommended pattern:

```lua
local SAVE_FILE = "mygame.data"

local function get_save_path()
    local dir = mbm.doCommands('get_save_dir')
    if dir and #dir > 0 then return dir .. "/" .. SAVE_FILE end
    return mbm.getPathEngine(SAVE_FILE)  -- fallback for development
end

-- Save
local function saveProgress(tData)
    local path    = get_save_path()
    local pathTmp = path .. ".tmp"
    local fp = io.open(pathTmp, "w")
    if not fp then return end
    for k, v in pairs(tData) do
        fp:write(string.format("mbm.setGlobal('%s',%s)\n", k, tostring(v)))
    end
    fp:close()
    -- Encrypt the temp file into the real save file, then remove the plaintext
    if not mbm.encrypt(pathTmp, path) then
        os.rename(pathTmp, path)   -- fallback: keep unencrypted if encrypt fails
    else
        os.remove(pathTmp)
    end
end

-- Load
local function loadProgress()
    local path    = get_save_path()
    local pathTmp = path .. ".tmp"
    if mbm.decrypt(path, pathTmp) then
        mbm.include(pathTmp)
        os.remove(pathTmp)
    else
        mbm.include(path)   -- first run or unencrypted legacy file
    end
end
```

#### Data flow summary

```
saveProgress()
    → writes plain Lua to <save_path>.tmp
    → mbm.encrypt(<save_path>.tmp, <save_path>)   ← AES-128-CBC ciphertext on disk
    → os.remove(<save_path>.tmp)                  ← no plaintext trace

loadProgress()
    → mbm.decrypt(<save_path>, <save_path>.tmp)   ← decrypts to temp
    → mbm.include(<save_path>.tmp)                ← executes Lua, sets globals
    → os.remove(<save_path>.tmp)                  ← no plaintext trace
```

#### What this achieves

| Threat | Protection |
|---|---|
| Player opens save file in a text editor | AES-128-CBC ciphertext — unreadable without the key |
| Player deletes the temp dir | Irrelevant — file lives in Application Support |
| App reinstall / update | File survives — container is preserved by macOS |
| Player uninstalls the app | Container (including saves) is removed by macOS |
| Player clears container via System Settings | Save is lost — same as any macOS app |

> **Note on `GAME_ASSETS_PASSWORD`:** This flag only affects the `.asset` archive
> used by the local CMake prototype (`GAME_ASSETS_DIR`). It has no effect in MAS mode
> — assets are plain files embedded in the bundle.  The App Store protects the
> entire package with FairPlay DRM at download time.

---

## Platform Source Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ mode entry point — instantiates `GAME`, calls `initGraphics()` + `onLoop()` |
| `main-lua.cpp` | Lua mode entry point — instantiates `LUA_MANAGER`, loads a `.lua` scene |
| `main-lua-delivery.cpp` | Delivery entry point — extracts `.asset`, then calls `mbm::onLoop()` |
| `main-lua-mas.mm` | Mac App Store entry point — loads assets from bundle via `NSBundle` |
| `Info.plist.in` | CMake template for MAS bundle plist (`@MAS_BUNDLE_ID@`, `@MAS_APP_NAME@`) |
| `mini-mbm.entitlements` | App Sandbox entitlements required for Mac App Store submission |
| `my-scene.cpp` / `my-scene.h` | Example `SCENE` subclass — starting point for your own game |
