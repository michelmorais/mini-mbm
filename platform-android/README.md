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

cmake ~/mini-mbm \
    -DPLAT=Android \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_NATIVE_API_LEVEL=24 \
    -DUSE_LUA=1 \
    -DUSE_ALL=1 \
    -DMBM_ENABLE_MESH_LEGACY_V7=1 \
    -DAUDIO=jni \
    -DUSE_STL_STATIC=1 \
    -DGAME_PACKAGE=com.mini.mbm.tower_defense \
    -DGAME_NAME="Tower Defense" \
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

## Assets folder

The assets live **outside** the generated project folder, at the path you passed to `-DGAME_ASSETS_DIR`.  They flow through the build in three stages:

| Stage | Path | Purpose |
|---|---|---|
| **Source** | `/home/michel/tower-defense/assets/` | Edit your Lua scripts and assets here |
| **Gradle staging** | `app/build/intermediates/assets/debug/mergeDebugAssets/` | Gradle copies here during build — do not edit |
| **APK** | `assets/` inside the APK | What the device reads at runtime via `AAssetManager` |

If you run `find . -name main.lua` in the generated project and see it under `build/intermediates/`, that is **correct** — it means Gradle successfully picked up your assets and they will be included in the APK.

**Always edit files in your source directory** (`/home/michel/tower-defense/assets/`).  The staging copy is recreated from scratch on every build.

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
| `AudioManager` JNI calls | `AUDIO=jni` (backward compat) or `AUDIO=opensl` |
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
    -DAUDIO=jni
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

Override any of these with `-D` flags on the `cmake` command:
```sh
cmake ../.. -DPLAT=Android ... \
    -DGAME_PACKAGE=com.example.tower_defense \
    -DGAME_NAME="Tower Defense" \
    -DGAME_APP_DIR=~/tower-defense-android
```

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
    -DAUDIO=jni \
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
| `jni` | Java `SoundPool` / `MediaPlayer` via JNI (default) | Stable, backward compatible |
| `opensl` | NDK OpenSL ES — pure C++, no Java audio calls | Available (API ≥ 21) |
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
| `platform-android/main-native-activity.cpp` | `android_main()` entry; touch/key/lifecycle input |
| `platform-android/my-scene.h` / `my-scene.cpp` | C++ scene — edit these for a custom game |
| `platform-android/MbmActivity.java` | Thin `NativeActivity` subclass; vibrate / locale helpers |
| `platform-android/templates/` | Gradle project templates (`*.in` files) |
| `platform-android/gradle/` | Gradle wrapper scripts (configured at CMake time) |
| `src/core_mbm/specific-android.cpp` | NDK system integration (`AAssetManager`, `ALooper`) |
| `src/core_mbm/audio-jni-android.cpp` | JNI audio backend (AUDIO=jni) |
| `src/core_mbm/audio-opensl-android.cpp` | OpenSL ES audio backend (AUDIO=opensl) |

---

## Legacy files (kept for reference)

The following files remain in the repo as archive and are **not used by the build**:

| File | Notes |
|---|---|
| `platform-android/main.cpp` | Old C++ JNI entry point (replaced by `main-native-activity.cpp`) |
| `platform-android/main-lua.cpp` | Old Lua JNI entry point (replaced by `main-native-activity.cpp`) |
| `platform-android/scene-1.h` / `scene-1.cpp` | Old scene with `JNIEnv*` constructor |
| `platform-android/AndroidManifest.xml` | Old hand-crafted manifest |
| `platform-android/com/` | 12 Java classes for the old JNI bridge |

---

## NDK version

Tested with **NDK r29** (`~/android-ndk-r29`).  Set `NDK_ROOT` before invoking CMake:
```sh
export NDK_ROOT=~/android-ndk-r29
```
