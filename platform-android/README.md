# Android Platform — Architecture Notes

## Prerequisites

Before running any cmake command you need three things installed on your machine:

### 1. Android Studio + SDK

Download from https://developer.android.com/studio and install it.
The installer places the Android SDK at `~/Android/Sdk` (Linux/macOS) automatically.

### 2. Android NDK r29

The NDK is the C++ compiler toolchain. Download it separately:
```sh
# Download NDK r29 (Linux x86_64)
cd ~
wget https://dl.google.com/android/repository/android-ndk-r29-linux.zip
unzip android-ndk-r29-linux.zip
# Result: ~/android-ndk-r29/
```
Or install it from Android Studio: **SDK Manager → SDK Tools → NDK (Side by side)**.

### 3. Ninja build system

```sh
sudo apt-get install ninja-build      # Ubuntu/Debian
# brew install ninja                  # macOS
```

### 4. Java 17 JDK

```sh
sudo apt-get install openjdk-17-jdk   # Ubuntu/Debian
```

---

## Quick Start — build your Lua game as an APK

This is the complete flow for a new Android developer.  Replace the paths with your own.

**Step 1 — tell your shell where the NDK lives** (must be done in every new terminal, or add to `~/.bashrc`):
```sh
export NDK_ROOT=~/android-ndk-r29
```

**Step 2 — create a build directory outside the engine repo and run cmake:**
```sh
mkdir -p ~/tower-defense-android && cd ~/tower-defense-android

# Using the dynamic libc++_shared.so (copied to the folder of application)
cmake ~/mini-mbm \
    -DPLAT=Android \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_NATIVE_API_LEVEL=24 \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=opensl \
    -DGAME_PACKAGE=com.mini.mbm.tower_defense \
    -DGAME_NAME="Tower Defense" \
    -DGAME_ICON_PNG=/home/michel/tower-defense/propaganda/1024x1024-icon.png \
    -DGAME_APP_DIR=~/tower-defense-android/android-studio \
    -DGAME_ASSETS_DIR=/home/michel/tower-defense/assets

# or with STL static
cmake ~/mini-mbm \
    -DPLAT=Android \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_NATIVE_API_LEVEL=24 \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=opensl \
    -DUSE_STL_STATIC=1 \
    -DGAME_PACKAGE=com.mini.mbm.tower_defense \
    -DGAME_NAME="Tower Defense" \
    -DGAME_ICON_PNG=/home/michel/tower-defense/propaganda/1024x1024-icon.png \
    -DGAME_APP_DIR=~/tower-defense-android/android-studio \
    -DGAME_ASSETS_DIR=/home/michel/tower-defense/assets
```

> **`GAME_ASSETS_DIR` must be an absolute path** — do not use `~` for this variable.

CMake will print the generated project location at the end:
```
  Android Studio project generated at:
    /home/michel/tower-defense-android/android-studio

  Open in Android Studio:
    studio "/home/michel/tower-defense-android/android-studio"
    (or: File → Open → select the folder above)

  Build from the command line:
    cd "/home/michel/tower-defense-android/android-studio" && ./gradlew assembleDebug
```

**Step 3 — build the APK:**

#### Debug build (development / testing)
```sh
cd ~/tower-defense-android/android-studio
./gradlew assembleDebug
```
Output: `app/build/outputs/apk/debug/app-debug.apk`

A debug APK is signed automatically with a throwaway debug key.  You can side-load it directly onto any device that has **USB debugging** enabled.  It includes debugger symbols, is not size-optimized, and cannot be uploaded to the Play Store.

#### Release build (distribution / Play Store)
```sh
cd ~/tower-defense-android/android-studio
./gradlew assembleRelease
```
Output: `app/build/outputs/apk/release/app-release-unsigned.apk`

A release APK must be **signed with your own keystore** before it can be installed or published. Create a keystore once (keep it safe — you need the same key for every update):
```sh
keytool -genkey -v -keystore ~/my-release-key.jks \
        -alias my-key -keyalg RSA -keysize 2048 -validity 10000
```
Then sign the APK:
```sh
# Sign
$ANDROID_HOME/build-tools/34.0.0/apksigner sign \
    --ks ~/my-release-key.jks \
    --out app-release.apk \
    app/build/outputs/apk/release/app-release-unsigned.apk
```

**Step 4 — install and run on a connected device** (USB debugging must be enabled on the device):
```sh
# Debug — install
adb install app/build/outputs/apk/debug/app-debug.apk

# Release — install (after signing)
adb install app-release.apk

# Launch from the command line (no need to tap the icon)
adb shell am start -n com.mini.mbm.tower_defense/com.mini.mbm.MbmActivity
```

**Step 5 — read log output in the terminal:**
```sh
# Show only your app's output (filters out all Android system noise)
adb logcat --pid=$(adb shell pidof -s com.example.tower_defense)
```

If the app crashes before `pidof` can catch it, use this instead — it captures the crash reason:
```sh
# Clear old logs first, then stream tagged output
adb logcat -c
adb logcat AndroidRuntime:E DEBUG:E mini-mbm:V *:S
```

| Tag | What it shows |
|---|---|
| `AndroidRuntime:E` | Java-level crashes and unhandled exceptions |
| `DEBUG:E` | Native crashes (signal/segfault, with backtrace) |
| `mini-mbm:V` | Engine log output (`__android_log_print` calls) |
| `*:S` | Silences everything else |

To see **all** output from your app including `LOGI`/`LOGW`/`LOGE` from the engine:
```sh
adb logcat -v time | grep -E "com.example.tower_defense|mini.mbm|FATAL|signal [0-9]"
```

To save a full log to a file for later inspection:
```sh
# Terminal 1 — start recording
adb logcat -c && adb logcat -v threadtime > /tmp/android.log

# Terminal 2 — launch the app
adb shell am start -n com.example.tower_defense/com.mini.mbm.MbmActivity

# After the crash, Ctrl+C in terminal 1, then search for the cause
grep -E "FATAL|Error|signal|backtrace|mini.mbm|lua" /tmp/android.log | head -80
```

> **gradle-wrapper.jar missing?**  If the automatic download failed, run once inside
> the generated folder:
> ```sh
> gradle wrapper --gradle-version 8.7
> ```

---

## Building from Android Studio (GUI)

After opening the project (**File → Open → select the `android-studio/` folder**) and clicking **Trust Project**:

#### Run on a device or emulator (debug)
1. Connect your phone via USB (or start an emulator from **Device Manager**).
2. Click the green **▶ Run** button in the toolbar (or press **Shift+F10**).
3. Android Studio builds a debug APK, installs it, and launches the app automatically.

#### Build an APK without running
- **Build → Build Bundle(s) / APK(s) → Build APK(s)**
- The output path is shown in a balloon notification bottom-right; click **locate** to open it in the file manager.

#### Switch between Debug and Release
- In the toolbar, click the **Build Variants** panel (bottom-left tab, or **View → Tool Windows → Build Variants**).
- Change `app` from `debug` to `release`.
- Then **Build → Build Bundle(s) / APK(s) → Build APK(s)**.

> Note: a release build from Android Studio also requires signing.  Use **Build → Generate Signed Bundle / APK** for a guided workflow that lets you pick your keystore.

---

## Alternative — build libraries with Make + manual Android Studio project

Instead of letting Gradle drive the native build, you can compile the shared libraries
yourself with `make` and copy them into a hand-crafted Android Studio project.  This is
the traditional workflow and gives you full control over the build and project layout.

### Step 1 — configure and build with CMake + Make

The manual workflow supports both **Lua** and **pure C++** game modes.

#### Lua mode (recommended for scripted games)

```sh
export NDK_ROOT=~/android-ndk-r29

mkdir -p ~/my-game-android && cd ~/my-game-android

cmake ~/mini-mbm \
    -DPLAT=Android \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_NATIVE_API_LEVEL=24 \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=opensl

make -j$(nproc)
```

In Lua mode the engine loads `main.lua` from the assets folder at runtime.  Your game
logic lives entirely in Lua scripts.

#### Pure C++ mode

```sh
export NDK_ROOT=~/android-ndk-r29

mkdir -p ~/my-game-android && cd ~/my-game-android

cmake ~/mini-mbm \
    -DPLAT=Android \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_NATIVE_API_LEVEL=24 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=opensl

make -j$(nproc)
```

Without `-DUSE_LUA=1`, the engine compiles `platform-android/my-scene.cpp` into the
library instead of the Lua wrapper.  Your game logic goes in `MY_SCENE::init()` and
`MY_SCENE::update()` — edit `my-scene.h` / `my-scene.cpp` before building.

> In both modes the output library is `libmini-mbm.so` and the rest of the steps below
> are identical.

> You can omit the `GAME_PACKAGE`, `GAME_NAME`, `GAME_APP_DIR` and `GAME_ASSETS_DIR`
> flags — they are only used for the automatic Gradle project generation.

When the build completes you will see a summary like this:

```
[100%] Built targets in ~/mini-mbm/bin/release/arm64-v8a:
--- Shared libraries / executables ---
total 16M
-rwxr-xr-x 1 user user 8.9M libc++_shared.so
-rwxr-xr-x 1 user user 2.2M libcore_mbm.so
-rwxr-xr-x 1 user user 1.3M ImGui.so
-rwxr-xr-x 1 user user 1.2M lsqlite3.so
-rwxr-xr-x 1 user user 873K libmini-mbm.so
-rwxr-xr-x 1 user user 585K box2dLiquidFun.so
-rwxr-xr-x 1 user user 381K box2d.so
-rwxr-xr-x 1 user user 324K liblua-5.4.1.so
-rwxr-xr-x 1 user user 128K plugin_helper.so
```

> If you used `-DUSE_STL_STATIC=1`, `libc++_shared.so` will **not** appear (the STL
> is linked statically into every library) and you do **not** need to copy it.

The exact set of `.so` files depends on the flags you passed to CMake (e.g. `USE_ALL`,
`USE_BOX2D`, `USE_IMGUI`, etc.).  All shared libraries that need to ship in the APK
are placed in the output directory.

### Step 2 — build for additional ABIs (optional)

To support both 64-bit and 32-bit devices, repeat the build for each ABI in a
**separate build directory**:

```sh
# arm64-v8a (already done above)
mkdir -p ~/my-game-android/arm64 && cd ~/my-game-android/arm64
cmake ~/mini-mbm -DPLAT=Android -DANDROID_ABI=arm64-v8a  ... && make -j$(nproc)

# x86_64 (emulators)
mkdir -p ~/my-game-android/x86_64 && cd ~/my-game-android/x86_64
cmake ~/mini-mbm -DPLAT=Android -DANDROID_ABI=x86_64     ... && make -j$(nproc)
```

Each ABI produces its libraries under `bin/release/<ABI>/`.

### Step 3 — create an Android Studio project

1. Open **Android Studio → File → New → New Project**.
2. Select **No Activity** (or **Empty Activity** — you will replace the activity class).
3. Set the **minimum SDK** to **API 24** (Android 7.0 Nougat) or higher.
4. Set the **language** to **Java** and finish creating the project.

### Step 4 — add MbmActivity.java

Copy the thin `NativeActivity` wrapper into your project's Java source tree.  The
package path **must** be `com/mini/mbm/`:

```sh
# From the root of your Android Studio project
mkdir -p app/src/main/java/com/mini/mbm
cp ~/mini-mbm/platform-android/MbmActivity.java \
   app/src/main/java/com/mini/mbm/
```

### Step 5 — copy the shared libraries into `jniLibs`

Create the `jniLibs` folder under `app/src/main` and copy **all** `.so` files from
the build output, preserving the ABI subfolder name:

```sh
# arm64-v8a
mkdir -p app/src/main/jniLibs/arm64-v8a
cp ~/mini-mbm/bin/release/arm64-v8a/*.so \
   app/src/main/jniLibs/arm64-v8a/

# x86_64 (if you built it)
mkdir -p app/src/main/jniLibs/x86_64
cp ~/mini-mbm/bin/release/x86_64/*.so \
   app/src/main/jniLibs/x86_64/
```

> **Important:** The ABI folder name (`arm64-v8a`, `x86_64`, etc.) must match exactly.
> Android loads native libraries from `jniLibs/<ABI>/` at runtime.
>
> If you did **not** use `-DUSE_STL_STATIC=1`, make sure `libc++_shared.so` is included
> — without it the app will crash immediately with an `UnsatisfiedLinkError`.

### Step 6 — add your game assets

Create the `assets` folder and copy your game files (Lua scripts, textures, audio, etc.):

```sh
mkdir -p app/src/main/assets
cp -r ~/my-game/assets/* app/src/main/assets/
```

The engine reads files from this folder at runtime via the Android `AAssetManager` API.
At minimum, a Lua-based game needs `main.lua` in the assets root.

### Step 7 — configure AndroidManifest.xml

Replace the generated `AndroidManifest.xml` with one that declares `MbmActivity` as a
`NativeActivity`.  Use the engine's template as reference:

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">

    <application
        android:allowBackup="true"
        android:label="@string/app_name"
        android:hasCode="true"
        android:icon="@mipmap/ic_launcher">

        <activity
            android:name="com.mini.mbm.MbmActivity"
            android:configChanges="orientation|screenSize|keyboardHidden"
            android:exported="true">

            <meta-data
                android:name="android.app.lib_name"
                android:value="mini-mbm" />

            <intent-filter>
                <action   android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
```

> The `android:value` in the `meta-data` tag must match the library name **without** the
> `lib` prefix and `.so` suffix.  For example, if your main library is `libmini-mbm.so`,
> the value is `mini-mbm`.

### Step 8 — remove externalNativeBuild from build.gradle

Since you are providing pre-built `.so` files, your `app/build.gradle` must **not**
contain an `externalNativeBuild` block.  If Android Studio generated one, delete it.
Make sure the `android` block includes the `jniLibs` source set:

```groovy
android {
    // ...
    sourceSets {
        main {
            jniLibs.srcDirs = ['src/main/jniLibs']
        }
    }
}
```

### Step 9 — build and run

Connect a device (or start an emulator) and click **▶ Run** in Android Studio, or build
from the command line:

```sh
cd ~/MyGame
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.example.mygame/com.mini.mbm.MbmActivity
```

### When to rebuild and re-copy

You only need to re-copy the `.so` files when:
- You change engine source code or CMake flags and rebuild with `make`.
- You update the NDK version.

Asset changes (Lua scripts, textures, audio) only require re-copying to `assets/` and
rebuilding the APK — no native recompilation needed.

---

## Assets folder

The assets live **outside** the generated project folder, at the path you passed to `-DGAME_ASSETS_DIR`.  They flow through the build in three stages:

| Stage | Path | Purpose |
|---|---|---|
| **Source** | `/home/michel/tower-defense/assets/` | Edit your Lua scripts and assets here |
| **Gradle staging** | `app/build/intermediates/assets/debug/mergeDebugAssets/` | Gradle copies here during build — do not edit |
| **APK** | `assets/` inside the APK | What the device reads at runtime via `AAssetManager` |

If you run `find . -name main.lua` in the generated project and see it under `build/intermediates/`, that is **correct** — it means Gradle successfully picked up your assets and they will be included in the APK.

**Always edit files in your source directory** (`/home/michel/tower-defense/assets/`).  The staging copy is recreated from scratch on every build.

> **Note:** assets are copied into the project at **cmake configure time**, not at build time.
> If you add, remove, or rename a Lua script, texture, or any other asset file you must
> **re-run cmake** so the copy is refreshed.  Just editing an existing file does not require
> re-running cmake — `./gradlew assembleDebug` will pick up the change on the next build
> because Gradle detects the modified timestamp.  This is the same behaviour as the iOS port.

### Viewing assets in Android Studio

Android Studio's default **"Android"** project view does not show external asset directories.  To browse them:

1. At the top of the **Project** panel, click the dropdown that says **"Android"**.
2. Switch to **"Project Files"** view.
3. Navigate to `app → src → main → assets` — this shows the files from your source directory.

You do not need to re-run cmake after editing asset files.  Just run `./gradlew assembleDebug` (or press **▶ Run** in Android Studio) and the updated files will be packaged into the next APK.

---

## Modern NativeActivity approach (current)

The Android target has been modernised to follow the same pattern as the iOS port:
**CMake generates the entire project**; no manual Android Studio setup is required.

| Old approach | New approach |
|---|---|
| 14-step manual Android Studio project | One `cmake` command generates everything |
| Java `GLSurfaceView` + JNI bridge | C++ `android_native_app_glue` + `NativeActivity` |
| Java `FileJniEngine` for asset I/O | NDK `AAssetManager` C API |
| `AudioManager` JNI calls | OpenSL ES — pure C++ (`-DAUDIO=opensl`, recommended) |
| `MY_GAME(JNIEnv*, jobject)` | `MY_GAME()` — no JNI args needed |

A single thin Java class (`MbmActivity.java`) is kept for device services (vibrate,
locale query) that still require a `Context`, but all game and engine logic is pure C++.

---

## Generating an Android Studio project

```sh
# Set NDK_ROOT before invoking CMake
export NDK_ROOT=~/android-ndk-r29

mkdir -p build/android_arm64 && cd build/android_arm64
cmake ../.. \
    -DPLAT=Android \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_NATIVE_API_LEVEL=24 \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=opensl
```

CMake will:
1. Configure the engine build for the specified ABI.
2. Emit all Gradle template files into `build/android_arm64/android-studio/`.
3. Download `gradle-wrapper.jar` automatically (requires internet access on first run).

Then open the generated project in Android Studio:
```sh
# Android Studio → File → Open → select the android-studio/ folder
# — or from the command line:
cd build/android_arm64/android-studio && ./gradlew assembleDebug
```

> **gradle-wrapper.jar missing?**  If the automatic download failed, run once inside
> the generated folder:
> ```sh
> gradle wrapper --gradle-version 8.7
> ```

---

## CMake variables for the Gradle project

| Variable | Default | Description |
|---|---|---|
| `GAME_PACKAGE` | `com.mini.mbm.game` | Java package name (application ID) |
| `GAME_NAME` | `mini-mbm` | App display name / project name |
| `GRADLE_VERSION` | `8.7` | Gradle wrapper version |
| `GRADLE_ABI_FILTERS` | `"arm64-v8a", "x86_64"` | Groovy abiFilters list |
| `ANDROID_SDK_ROOT` | `$ANDROID_HOME` or `$ANDROID_SDK_ROOT` | Path for `local.properties` |
| `GAME_APP_DIR` | `<build_dir>/android-studio` | Where generated project is written |
| `GAME_ASSETS_DIR` | _(empty)_ | Path to your game's assets folder — served directly into the APK |
| `GAME_ICON_PNG` | _(empty)_ | Path to a PNG icon — auto-resized to all mipmap densities at configure time |
| `GAME_ICON_DIR` | _(empty)_ | Path to a directory containing pre-built `mipmap-*/` subdirectories |

Override any of these with `-D` flags on the `cmake` command:
```sh
cmake ../.. -DPLAT=Android ... \
    -DGAME_PACKAGE=com.example.tower_defense \
    -DGAME_NAME="Tower Defense" \
    -DGAME_APP_DIR=~/tower-defense-android
```

---

## App icon

By default the generated APK uses the Android system placeholder icon.  Provide your
own icon in one of two ways:

### Option A — single PNG (auto-resized)

Pass the path to a square PNG (ideally 512×512 or larger) with `-DGAME_ICON_PNG`:

```sh
cmake ~/mini-mbm -DPLAT=Android ... \
    -DGAME_ICON_PNG=/path/to/icon.png \
    -DGAME_APP_DIR=~/tower-defense-android/android-studio
```

CMake calls `platform-android/resize_icon.py` at configure time, which generates
`mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png` into the project's `res/`
directory.

**Prerequisite — install one of:**
```sh
pip install pillow           # recommended (Pillow)
sudo apt-get install imagemagick   # fallback (ImageMagick)
```

You can also run the script standalone to verify output:
```sh
python3 platform-android/resize_icon.py /path/to/icon.png /tmp/test-res
ls /tmp/test-res/mipmap-*/ic_launcher.png
```

### Option B — pre-built mipmap directories

If you already have an Android `res/` directory with `mipmap-*` subfolders:

```sh
cmake ~/mini-mbm -DPLAT=Android ... \
    -DGAME_ICON_DIR=/path/to/res \
    -DGAME_APP_DIR=~/tower-defense-android/android-studio
```

`GAME_ICON_DIR` must contain at least one `mipmap-*/` subdirectory with an
`ic_launcher.png` file inside.

> `GAME_ICON_PNG` and `GAME_ICON_DIR` use the same variable names as the iOS port,
> so they can be passed to both platform builds from a shared build script.

---

## Placing the project outside the engine repo

The generated Gradle project can live anywhere — it does not belong inside the engine repo.
A typical multi-project layout:

```
~/mini-mbm/              ← engine repo (shared across games)
~/tower-defense/         ← game repo (assets, Lua scripts, C++ scenes)
~/tower-defense-android/ ← generated Android Studio project (add to .gitignore)
```

```sh
mkdir -p ~/tower-defense-android && cd ~/tower-defense-android
cmake ~/mini-mbm \
    -DPLAT=Android \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_NATIVE_API_LEVEL=24 \
    -DUSE_LUA=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=opensl \
    -DUSE_STL_STATIC=1 \
    -DGAME_PACKAGE=com.example.tower_defense \
    -DGAME_NAME="Tower Defense" \
    -DGAME_APP_DIR=~/tower-defense-android/android-studio \
    -DGAME_ASSETS_DIR=~/tower-defense/assets
```

---

## Audio backends

| `AUDIO=` | Description | Status |
|---|---|---|
| `opensl` | NDK OpenSL ES — pure C++, no Java audio calls | **Recommended** (API ≥ 21) |
| `none` | Disabled | Silently drops all audio calls |

To use OpenSL ES:
```sh
cmake ../.. -DPLAT=Android ... -DAUDIO=opensl
```

OpenSL ES limitations:
- `setPitch()` is a no-op (pitch shifting requires `SLPlaybackRateItf`, which is optional
  and not universally supported; return value is `true` to avoid breaking callers).
- `setPan()` stores the value but does not apply it (stereo position `SLStereoPositionItf`
  availability varies by device).
- Only formats natively supported by the device's OpenSL ES implementation will decode
  (WAV PCM is universally supported; OGG/MP3 support is device-dependent via MIME type).

---

## Build modes (Lua vs. pure C++)

| CMake flag | Entry point | Notes |
|---|---|---|
| `-DUSE_LUA=1` | `main-native-activity.cpp` (Lua path) | `mbm::LUA_MANAGER` runs `main.lua` |
| _(no flag)_ | `main-native-activity.cpp` + `my-scene.cpp` | Subclass `MY_SCENE`, override `init()` |

The `android_main()` NativeActivity entry point is the same file for both paths; the
`#ifdef USE_LUA` guard inside it selects the Lua manager or the `MY_GAME` C++ object.

---

## Key source files

| File | Purpose |
|---|---|
| `platform-android/main-native-activity.cpp` | `android_main()` entry; touch/key/lifecycle input; `android_command_handler` fn-ptr |
| `platform-android/my-scene.h` / `my-scene.cpp` | C++ scene — edit these for a custom game |
| `platform-android/MbmActivity.java` | Thin `NativeActivity` subclass; `OnDoCommands` bridge; vibrate / clipboard / URL / share |
| `platform-android/resize_icon.py` | Python script: resize a PNG to all Android mipmap densities |
| `platform-android/templates/` | Gradle project templates (`*.in` files) |
| `platform-android/templates/file_paths.xml` | FileProvider path config (copied to `res/xml/` at configure time) |
| `platform-android/gradle/` | Gradle wrapper scripts (configured at CMake time) |
| `src/lua-wrap/framework-android-lua.cpp` | Lua bindings for Android (`mbm.doCommands` → fn-ptr) |
| `src/core_mbm/specific-android.cpp` | NDK system integration (`AAssetManager`, `ALooper`) |
| `src/core_mbm/audio-opensl-android.cpp` | OpenSL ES audio backend (`AUDIO=opensl`, recommended) |

---

## Legacy files (kept for reference)

The following files remain in the repo as archive and are **not used by the NativeActivity build**:

| File | Notes |
|---|---|
| `platform-android/main.cpp` | Old C++ JNI entry point (replaced by `main-native-activity.cpp`) |
| `platform-android/main-lua.cpp` | Old Lua JNI entry point (replaced by `main-native-activity.cpp`) |
| `platform-android/scene-1.h` / `scene-1.cpp` | Old scene with `JNIEnv*` constructor |
| `platform-android/AndroidManifest.xml` | Old hand-crafted manifest |
| `platform-android/com/mini/mbm/FileJniEngine.java` | Lua file-dialog helper (optional — silently skipped if absent) |
| `platform-android/com/mini/mbm/KeyCodeJniEngine.java` | Lua key-mapping helper (optional — silently skipped if absent) |

### Removed files

The following JNI audio files were removed after the switch to OpenSL ES (`-DAUDIO=opensl`).
They are no longer needed and have been deleted from the repository:

- `AudioJniEngine.java`
- `AudioManagerJniEngine.java`
- `MusicJniEngine.java`
- `SoundJniEngine.java`
- `src/core_mbm/audio-jni-android.cpp`

---

## NDK version

Tested with **NDK r29** (`~/android-ndk-r29`).  Set `NDK_ROOT` before invoking CMake:
```sh
export NDK_ROOT=~/android-ndk-r29
```
