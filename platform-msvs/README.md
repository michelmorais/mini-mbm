# Windows Platform — Visual Studio Project Notes

The Windows port uses a pre-configured **Visual Studio 2022** solution located at
`platform-msvs/mini-mbm.sln`. All project files and third-party libraries are
checked into the repository — no additional CMake configure step is needed.

> For cross-compilation via CMake + MinGW on Windows, see the CMake build
> instructions in the [Building the Engine](../README.md#building-the-engine)
> section of the main README. The notes below apply to the Visual Studio workflow.

---

## Prerequisites

| Tool | Minimum version | Notes |
|---|---|---|
| **Visual Studio 2022** | 17.x | Community edition is sufficient |
| **Desktop development with C++ workload** | — | Installs MSVC, Windows SDK, and DirectX headers |

---

## Opening the Solution

```cmd
start platform-msvs\mini-mbm.sln
```

Or open **Visual Studio → File → Open → Project/Solution…** and browse to
`platform-msvs/mini-mbm.sln`.

---

## Solution Projects

All projects live under `platform-msvs/` and build into the shared `bin/` and
`libs/` output directories at the repo root.

| Project name | Directory | Description |
|---|---|---|
| **mini_mbm** | `mini-mbm/` | Main executable — Windows entry point, links the engine library and all enabled plugins |
| **mini-mbm-lib** | `mini-mbm-lib/` | Core engine static library (rendering loop, Lua manager, plugin registration) |
| **mini-mbm-launcher** | `mini-mbm-launcher/` | Launcher executable with script browser dialog — shown when the engine starts without a scene argument |
| **mini-mbm-dev** | `mini-mbm-dev/` | Development / editor build — includes all plugins and the built-in Lua editors |
| **core_mbm** | `core_mbm/` | Foundational engine library (device, scene, input, camera, animation, audio) |
| **lua5.4** | `lua5.4/` | Lua 5.4.1 static library |
| **lsqlite3** | `lsqlite3/` | SQLite3 Lua bindings (used by the Asset Packager) |
| **ImGui** | `imGui/` | Dear ImGui plugin with DirectX 9, OpenGL 3, and Win32 backends — powers all built-in editors |
| **box2d** | `box2d/` | Box2D 2.4.1 physics plugin |
| **box2dLiquidFun** | `box2d-liquid-fun/` | LiquidFun 2.3.1 fluid simulation plugin |
| **bullet3d** | `bullet2.8/` | Bullet 2.84 3D physics plugin |
| **tilemap** | `tilemap/` | Tiled tile-map plugin |
| **tiny_obj_loader** | `tiny-obj-loader/` | Tiny OBJ mesh loader library |
| **plugin-helper** | `plugin-helper/` | Shared utilities for plugin development (Lua shader bindings, class identification) |
| **distribution** | `distribution/` | Asset packaging DLL — AES-128-CBC encrypted SQLite archive library |
| **distribution_exe** | `distribution-exe/` | `distribution.exe` CLI — packs/extracts `.asset` files; called by `package-game.bat` |
| **libTest** | `libTest/` | Unit and integration test library |

---

## Selecting the Graphics Backend

The backend is controlled by `platform-msvs/mbm-backend.props`, which is imported
by every project in the solution. Edit the `<MbmBackend>` property to switch:

```xml
<!-- DirectX 9 (default for Debug|Win32) -->
<MbmBackend>DirectX9</MbmBackend>

<!-- OpenGL ES 2.0 (default for all other configurations) -->
<MbmBackend>OpenGLES</MbmBackend>
```

Default assignments:

| Configuration | Default backend |
|---|---|
| `Debug\|Win32` | **DirectX 9** |
| `Release\|Win32` | OpenGL ES |
| `Debug\|x64` | OpenGL ES |
| `Release\|x64` | OpenGL ES |

You can override without editing the file by passing `/p:MbmBackend=DirectX9` (or
`OpenGLES`) on the MSBuild command line (see below).

The same property sheet also defines the lighting compile-time cap:

```xml
<MbmSupportedMaxLights>4</MbmSupportedMaxLights>
```

Accepted values are `1..4`. This maps to the engine preprocessor define
`SUPPORTED_MAX_LIGHTS`, which controls the compiled supported light count used by
the default light-capable shaders and staging arrays. You can override it from
MSBuild with `/p:MbmSupportedMaxLights=2`.

---

## Build Configurations

The solution exposes four configurations:

| Configuration | Platform | Notes |
|---|---|---|
| `Debug` | `Win32` (x86) | Debug symbols, no optimization. DirectX 9 backend by default. |
| `Release` | `Win32` (x86) | Full optimization. OpenGL ES backend by default. |
| `Debug` | `x64` | Debug symbols. OpenGL ES backend by default. |
| `Release` | `x64` | Full optimization. OpenGL ES backend by default. |

---

## Building from the Command Line (MSBuild)

```cmd
rem Debug (whole solution, x86)
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Debug /p:Platform=x86 /m /v:minimal

rem Release
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Release /p:Platform=x86 /m /v:minimal

rem Override backend
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Release /p:Platform=x86 /p:MbmBackend=DirectX9 /m /v:minimal

rem Build a single project
msbuild platform-msvs\core_mbm\core_mbm.vcxproj /p:Configuration=Debug /p:Platform=x86 /m
```

> **Note:** Always specify `/p:Platform=x86`. The solution platform names are `x86` and `x64` (not `Win32`). Only 32-bit portaudio binaries are bundled; the x64 platform configuration is not fully supported.

---

## Skeletal Numeric Parity Tests

`libTest` provides native encoded/readback comparison against the shared CPU references for both
Windows graphics backends. Build the selected backend and run its matching command from the output
directory:

```cmd
rem DirectX 9 (Debug|Win32 default)
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Debug /p:Platform=x86 /p:MbmBackend=DirectX9 /m /v:minimal
cd platform-msvs\Debug
libTest.exe --directx9-skeletal-parity-test

rem OpenGL ES through ANGLE (Release|Win32 default)
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Release /p:Platform=x86 /p:MbmBackend=OpenGLES /m /v:minimal
cd platform-msvs\Release
libTest.exe --gles-skeletal-parity-test
```

Each command exercises synthetic and Lorekeeper LBS/DQS positions and normals. All four cases must
report `PASS`, the suite must end with `cases=4 PASS`, and the process must exit with status zero.
The Windows OpenGL ES result is specifically ANGLE coverage and does not replace native OpenGL ES
coverage on Linux.

---

## Post-Build: Copying DLLs

After the first successful build, run `copy-dlls.bat` to copy the required runtime
DLLs (OpenGL ES emulation, Lua, etc.) next to the output executable:

> **Note:** DirectSound (`-DAUDIO=dsound`) uses a Windows system DLL — no audio
> file needs to be copied.

```cmd
cd platform-msvs
copy-dlls.bat Debug
copy-dlls.bat Release
```

The script sources DLLs from the `third-party/` tree. Re-run it whenever you add
a new plugin or switch the audio/graphics backend.

---

## Output Locations

| Artifact | Path |
|---|---|
| Debug executable | `bin\debug\windows_x86\mini-mbm.exe` |
| Release executable | `bin\release\windows_x86\mini-mbm.exe` |
| Debug static libraries | `libs\debug\windows_x86\` |
| Release static libraries | `libs\release\windows_x86\` |

---

## Game Delivery — Distribution Packages

Two packaging paths are available. Both produce the same staging folder
(`<GameName>.GameDir\`) containing the renamed executable, runtime DLLs, game
assets, and a `launch.bat`. From that folder you can optionally produce a
one-file NSIS installer (`.exe`), a Windows Installer package (`.msi` via WiX),
or a portable ZIP archive.

The writable save-file directory (`%APPDATA%\<GameName>\`) is computed by the
delivery binary at startup and exposed to Lua via `mbm.doCommands('get_save_dir')` —
the same API used on Linux and macOS.

---

## Building Just the Engine with CMake / MinGW

Use this when you want to develop or test the engine itself — no game assets or
packaging needed.  The build outputs `mini-mbm.exe` plus `mini-mbm-dev.exe` (the editor launcher) and
all plugin `.dll` files into `bin\debug\windows_x86\` or `bin\release\windows_x86\`.

MinGW runtime DLLs (`libgcc_s_*.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`),
the audio backend DLL (`portaudio_x86.dll`), the graphics backend DLLs
(`D3DCompiler_47.dll`; `libEGL.dll` + `libGLESv2.dll` for OpenGL ES), and the
`editor\` folder of Lua scripts are all copied automatically next to the
executables by CMake POST_BUILD steps, so no manual copying is required.

```cmd
mkdir build
cd build

cmake .. -G "MinGW Makefiles" ^
    -DPLAT=Windows -DUSE_ALL=1 ^
    -DAUDIO=portaudio ^
    -DCMAKE_BUILD_TYPE=Release

mingw32-make -j%NUMBER_OF_PROCESSORS%
```

For a Debug build:

```cmd
mkdir build
cd build

cmake .. -G "MinGW Makefiles" ^
    -DPLAT=Windows -DUSE_ALL=1 ^
    -DAUDIO=portaudio ^
    -DCMAKE_BUILD_TYPE=Debug

mingw32-make -j%NUMBER_OF_PROCESSORS%
```

To select the graphics backend explicitly, add one of:

| Flag | Backend |
|---|---|
| `-DUSE_DIRECTX9=1` | DirectX 9 (adds `D3DCompiler_47.dll`) |
| `-DUSE_OPENGL_ES=1` | OpenGL ES emulation (adds `libEGL.dll`, `libGLESv2.dll`) |

After the build, run the engine directly from the repo root:

```cmd
rem Run a specific Lua script
bin\release\windows_x86\mini-mbm.exe editor\scene_editor2d.lua

rem Open the dev launcher (ImGui tool selector — same as the MSVS mini-mbm-dev project)
bin\release\windows_x86\mini-mbm-dev.exe
```

> **Tip:** To also copy the `editor\` folder and the third-party runtime DLLs
> (portaudio, gles) to the output directory for a self-contained run from the
> bin folder, use `copy-dlls.bat` as described in the [Post-Build: Copying DLLs](#post-build-copying-dlls)
> section above — it is the same script used for Visual Studio builds.

---

## Game Delivery — Distribution Packages

---

### Path A — CMake / MinGW (recommended for CI and scripted builds)

Mirrors the Linux AppDir and iOS Xcode packaging workflows. Pass the same three
flags and the build assembles `GameDir\` automatically after `make`.

#### Prerequisites — MinGW toolchain

CMake needs `gcc`, `g++`, and `mingw32-make` on `PATH` before you run
`cmake -G "MinGW Makefiles"`. The easiest way to get them on Windows is via
**MSYS2** ([https://www.msys2.org](https://www.msys2.org)).

> **32-bit vs 64-bit:** `mingw64` and `ucrt64` produce **64-bit (x86_64)** binaries.
> `mingw32` produces **32-bit (i686)** binaries, matching Visual Studio's `Win32`
> platform target. The engine and all bundled third-party libraries are validated
> against the 32-bit build; use `mingw32` unless you have a specific reason to
> build 64-bit.

After installing MSYS2 to `C:\msys64`, open the **MSYS2** shell and install the
toolchain into whichever environment you need:

| Environment | Architecture | Runtime | Recommended for |
|---|---|---|---|
| `mingw32` | **32-bit (i686)** | `msvcrt` | Fully supported |
| `mingw64` | 64-bit (x86_64) | `msvcrt` | 64-bit builds |
| `ucrt64` | 64-bit (x86_64) | UCRT (Win 10+) | 64-bit modern builds |

```bash
# mingw32 environment (32-bit — recommended)
pacman -S mingw-w64-i686-gcc mingw-w64-i686-make mingw-w64-i686-cmake

# — OR — mingw64 environment (64-bit, msvcrt)
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-cmake

# — OR — ucrt64 environment (64-bit, UCRT)
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-cmake
```

Then add the corresponding `bin\` directory to your **system** `PATH` (once,
permanently) via *System Properties → Environment Variables → Path*:

| Environment chosen | Directory to add to PATH |
|---|---|
| `mingw32` | `C:\msys64\mingw32\bin` |
| `mingw64` | `C:\msys64\mingw64\bin` |
| `ucrt64` | `C:\msys64\ucrt64\bin` |

Or add it for the current terminal session only:

```cmd
rem mingw32 (32-bit — recommended)
set PATH=C:\msys64\mingw32\bin;%PATH%

rem mingw64 (64-bit)
set PATH=C:\msys64\mingw64\bin;%PATH%

rem ucrt64 (64-bit)
set PATH=C:\msys64\ucrt64\bin;%PATH%
```

Verify the setup:

```cmd
gcc --version
mingw32-make --version
cmake --version
```

All three must print a version number before the `cmake -G "MinGW Makefiles"` step
will succeed.

> **Note on the `mingw32-make` name:** The "32" is historical — this executable is
> present in **all** MSYS2 environments including `mingw64` and `ucrt64`. It is not
> related to the target architecture; the compiler determines whether the output
> binary is 32-bit or 64-bit.

#### CMake delivery flags

| Flag | Required? | Description |
|---|---|---|
| `-DGAME_ASSETS_DIR=C:\path\to\assets` | **Yes** (activates delivery) | Absolute path to your game assets folder. Must contain `main.lua`. |
| `-DGAME_NAME="My Game"` | No (default: `mini-mbm`) | Display name — used for the window title, EXE name, and installer. |
| `-DGAME_ASSETS_PASSWORD=secret` | No | If set, assets are AES-128-CBC encrypted (PBKDF2-HMAC-SHA256, 100 000 iterations). Omit for unencrypted packing. |
| `-DGAME_ICON_PNG=C:\path\to\icon.png` | No | Any-size PNG. ImageMagick (`convert`) converts it to `.ico` automatically if found in `PATH` or the standard install location. |
| `-DGAME_ICON_ICO=C:\path\to\icon.ico` | No | Supply a ready-made `.ico` directly (takes priority over `-DGAME_ICON_PNG`). |

#### Example build

```cmd
mkdir build
cd build

rem Build (portaudio is the default audio backend for Windows)
cmake .. -G "MinGW Makefiles" ^
    -DPLAT=Windows -DUSE_ALL=1 -DAUDIO=portaudio ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DUSE_DIRECTX9=1 ^
    -DGAME_NAME="Tower Defense Monster" ^
    -DGAME_ASSETS_DIR="C:\Users\miche\Documents\tower-defense\assets" ^
    -DGAME_ICON_PNG="C:\Users\miche\Documents\tower-defense\propaganda\1024x1024-icon.png"

rem using backend OpenGlEs
rem The required DLLs (libEGL.dll, libGLESv2.dll, d3dcompiler_47.dll) are
rem bundled in the repo at third-party\gles\bin — no external download needed.

cmake .. -G "MinGW Makefiles" ^
    -DPLAT=Windows -DUSE_ALL=1 -DAUDIO=dsound ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DUSE_OPENGL_ES=1  ^
    -DGAME_NAME="Tower Defense Monster" ^
    -DGAME_ASSETS_DIR="C:\Users\miche\Documents\tower-defense\assets" ^
    -DGAME_ICON_PNG="C:\Users\miche\Documents\tower-defense\propaganda\1024x1024-icon.png"


rem Enabling steam plugin + asset encryption
rem IMPORTANT: Use forward slashes in -DSTEAMWORKS_SDK_PATH on MinGW.
rem Backslashes are misinterpreted by ld.exe as escape sequences and will
rem cause a "cannot find steam_api.lib: No such file or directory" link error.

cmake .. -G "MinGW Makefiles" ^
    -DPLAT=Windows -DUSE_ALL=1 -DAUDIO=portaudio ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DUSE_OPENGL_ES=1  ^
    -DUSE_STEAM=1 ^
    -DSTEAMWORKS_SDK_PATH="C:/Program Files (x86)/steamworks_sdk_165/sdk" ^
    -DGAME_NAME="Tower Defense Monster" ^
    -DGAME_ASSETS_DIR="C:\Users\miche\Documents\tower-defense\assets" ^
    -DGAME_ASSETS_PASSWORD="agasdOyu86555" ^
    -DGAME_ICON_PNG="C:\Users\miche\Documents\tower-defense\propaganda\1024x1024-icon.png"

mingw32-make -j%NUMBER_OF_PROCESSORS%    :: GameDir assembled automatically
mingw32-make nsis                        :: produces Tower_Defense_Monster-windows-setup.exe
mingw32-make msi                         :: produces Tower_Defense_Monster-windows.msi
mingw32-make zip                         :: produces Tower_Defense_Monster-windows.zip
```

After `mingw32-make`, the staging folder is:

```
build\
    Tower_Defense_Monster.GameDir\
        Tower_Defense_Monster.exe
        distribution.dll              (asset library — required at runtime)
        d3dcompiler_47.dll   (or libEGL.dll + libGLESv2.dll for OpenGL ES)
        box2d.dll  ImGui.dll  ...   (plugin DLLs)
        Tower_Defense_Monster.ico
        launch.bat
        assets\
            Tower_Defense_Monster.asset   ← packed (+ optionally encrypted) archive
```

#### Packaging tools (all optional — `mingw32-make` alone is sufficient for a portable folder)

| Tool | Target | Output | Download |
|---|---|---|---|
| [NSIS](https://nsis.sourceforge.io) | `mingw32-make nsis` | `GameName-windows-setup.exe` | https://nsis.sourceforge.io |
| [WiX v4](https://wixtoolset.org) | `mingw32-make msi` | `GameName-windows.msi` | `dotnet tool install --global wix` |
| [WiX v3](https://github.com/wixtoolset/wix3/releases) | `mingw32-make msi` | `GameName-windows.msi` | GitHub releases |
| (built-in) | `mingw32-make zip` | `GameName-windows.zip` | — no extra tools needed |

If a packaging tool is not found, the target prints the download URL and exits
cleanly; the `GameDir\` folder is always ready regardless.

---

### Path B — Visual Studio solution (`package-game.bat`)

For interactive development builds, run `package-game.bat` from `platform-msvs\`
after building the solution.

#### Prerequisites

| Tool | Required for | Download |
|---|---|---|
| Visual Studio 2022 (built solution) | Everything | — |
| `cmake` in `PATH` | MSI generation | https://cmake.org/download/ |
| [NSIS](https://nsis.sourceforge.io) | `*-windows-setup.exe` | https://nsis.sourceforge.io |
| [WiX v4](https://wixtoolset.org) or [WiX v3](https://github.com/wixtoolset/wix3/releases) | `*-windows.msi` | `dotnet tool install --global wix` |
| PowerShell (built into Windows) | `*-windows.zip` | — |

#### Usage

```cmd
cd platform-msvs

rem Basic usage
package-game.bat "Tower Defense Monster" C:\Users\miche\tower-defense\assets Release

rem With an icon
package-game.bat "Tower Defense Monster" C:\Users\miche\tower-defense\assets Release C:\Users\miche\tower-defense\icon.ico

rem With an icon and asset encryption password
package-game.bat "Tower Defense Monster" C:\Users\miche\tower-defense\assets Release C:\Users\miche\tower-defense\icon.ico mysecret
```

| Argument | Required? | Description |
|---|---|---|
| `%1` Game name | **Yes** | Display name — use quotes for names with spaces |
| `%2` Assets dir | **Yes** | Absolute path to game assets folder (must contain `main.lua`) |
| `%3` Config | No | `Debug` or `Release` (default: `Release`) |
| `%4` Icon `.ico` | No | Path to a Windows `.ico` file |
| `%5` Password | No | If supplied, assets are AES-128-CBC encrypted via `distribution.exe` |

#### What the script produces

The script always assembles the staging folder first, then runs each packaging
step if the required tool is available:

```
<repo-root>\
    Tower_Defense_Monster.GameDir\      ← always produced
        Tower_Defense_Monster.exe
        distribution.dll              (asset library — required at runtime)
        d3dcompiler_47.dll  ...  (runtime + plugin DLLs)
        Tower_Defense_Monster.ico
        launch.bat
        assets\
            Tower_Defense_Monster.asset   ← packed (+ optionally encrypted) archive
    Tower_Defense_Monster-windows-setup.exe  ← NSIS (if makensis found)
    Tower_Defense_Monster-windows.msi        ← WiX  (if cmake + WiX found)
    Tower_Defense_Monster-windows.zip        ← ZIP  (PowerShell, always)
```

All three installers perform a **per-user install** (no UAC prompt):
- **NSIS** installs to `%LOCALAPPDATA%\Programs\<GameName>\`
- **MSI** installs to `%LOCALAPPDATA%\Programs\<GameName>\`, with Start Menu + Desktop shortcuts and an *Uninstall* entry in Settings → Apps
- **ZIP** is a portable folder — extract anywhere and run `launch.bat`

#### Steps performed

1. Reads the built `mini-mbm.exe` from `bin\debug|release\windows_x86\`
2. Renames it to `<GameName>.exe` inside `<GameName>.GameDir\`
3. Copies runtime DLLs from `third-party\` (d3dcompiler, libEGL/libGLESv2)
4. Copies `distribution.dll` from the bin output directory
5. Copies plugin DLLs from the bin output directory
6. Packs game assets into `GameDir\assets\<GameName>.asset` using `distribution.exe` (encrypted if password supplied)
7. Writes `launch.bat` (launches `<GameName>.exe` with the correct working directory)
8. Generates and runs NSIS (if `makensis` is in `PATH` or its default install location)
9. Generates the MSI via `cmake -P make-msi.cmake` (if `cmake` is in `PATH` and WiX is installed)
10. Creates a ZIP archive via PowerShell `Compress-Archive`

#### MSI generation details (`make-msi.cmake`)

The MSI is produced by the standalone script `platform-msvs/make-msi.cmake`,
which can also be called directly without going through `package-game.bat`:

```cmd
cmake -DGAME_NAME="Tower Defense Monster" ^
      -DGAME_DIR="C:\path\to\Tower_Defense_Monster.GameDir" ^
      -DOUTPUT_DIR="C:\path\to\output" ^
      -DICON_ICO="C:\path\to\icon.ico" ^
      -P platform-msvs\make-msi.cmake
```

| Parameter | Required? | Description |
|---|---|---|
| `-DGAME_NAME` | **Yes** | Display name |
| `-DGAME_DIR` | **Yes** | Path to the populated `GameDir\` folder |
| `-DOUTPUT_DIR` | No | Where to write the `.msi` (default: same directory as the script) |
| `-DICON_ICO` | No | Path to a `.ico` file for the installer dialog |

The script detects WiX v3.x (any version) or v4 automatically. If neither is
found it prints download instructions and exits cleanly.

> **Tip:** Use absolute paths. The script does not expand environment variables
> inside path arguments on all Windows configurations.
