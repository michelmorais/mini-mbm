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
rem Debug (whole solution)
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Debug /m /v:minimal

rem Release
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Release /m /v:minimal

rem Override backend
msbuild platform-msvs\mini-mbm.sln /p:Configuration=Debug /p:MbmBackend=OpenGLES /m /v:minimal

rem Build a single project
msbuild platform-msvs\core_mbm\core_mbm.vcxproj /p:Configuration=Debug /m
```

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

The writable save-file directory is set by `launch.bat` as
`%APPDATA%\<GameName>\` and exposed to Lua via `os.getenv("GAME_SAVE_DIR")`.

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
| `mingw32` | **32-bit (i686)** | `msvcrt` | **Recommended** — matches VS Win32, audiere works |
| `mingw64` | 64-bit (x86_64) | `msvcrt` | 64-bit builds with dsound audio |
| `ucrt64` | 64-bit (x86_64) | UCRT (Win 10+) | 64-bit modern builds with dsound audio |

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

#### CMake delivery flags

| Flag | Required? | Description |
|---|---|---|
| `-DGAME_ASSETS_DIR=C:\path\to\assets` | **Yes** (activates delivery) | Absolute path to your game assets folder. Must contain `main.lua`. |
| `-DGAME_NAME="My Game"` | No (default: `mini-mbm`) | Display name — used for the window title, EXE name, and installer. |
| `-DGAME_ICON_PNG=C:\path\to\icon.png` | No | Any-size PNG. ImageMagick (`convert`) converts it to `.ico` automatically if found in `PATH` or the standard install location. |
| `-DGAME_ICON_ICO=C:\path\to\icon.ico` | No | Supply a ready-made `.ico` directly (takes priority over `-DGAME_ICON_PNG`). |

#### Example build

```cmd
mkdir build
cd build

rem 32-bit build (recommended — use mingw32 environment, set PATH=C:\msys64\mingw32\bin;%PATH%)
cmake .. -G "MinGW Makefiles" ^
    -DPLAT=Windows -DUSE_ALL=1 -DAUDIO=audiere ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DGAME_NAME="Tower Defense Monster" ^
    -DGAME_ASSETS_DIR="C:\Users\miche\Documents\tower-defense\assets" ^
    -DGAME_ICON_PNG="C:\Users\miche\Documents\tower-defense\propaganda\1024x1024-icon.png"

rem 64-bit build (use mingw64 or ucrt64 environment — audiere not available for x64)
rem   -DAUDIO=dsound

mingw32-make -j%NUMBER_OF_PROCESSORS%    :: GameDir assembled automatically
mingw32-make nsis                        :: produces Tower_Defense_Monster-windows-setup.exe
mingw32-make msi                         :: produces Tower_Defense_Monster-windows.msi
mingw32-make zip                         :: produces Tower_Defense_Monster-windows.zip
```

After `make`, the staging folder is:

```
build\
    Tower_Defense_Monster.GameDir\
        Tower_Defense_Monster.exe
        d3dcompiler_47.dll   (or libEGL.dll + libGLESv2.dll for OpenGL ES)
        box2d.dll  ImGui.dll  ...   (plugin DLLs)
        Tower_Defense_Monster.ico
        launch.bat
        assets\
            main.lua
            ...
```

#### Packaging tools (all optional — `make` alone is sufficient for a portable folder)

| Tool | Target | Output | Download |
|---|---|---|---|
| [NSIS](https://nsis.sourceforge.io) | `make nsis` | `GameName-windows-setup.exe` | https://nsis.sourceforge.io |
| [WiX v4](https://wixtoolset.org) | `make msi` | `GameName-windows.msi` | `dotnet tool install --global wix` |
| [WiX v3](https://github.com/wixtoolset/wix3/releases) | `make msi` | `GameName-windows.msi` | GitHub releases |
| (built-in) | `make zip` | `GameName-windows.zip` | — no extra tools needed |

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
```

| Argument | Required? | Description |
|---|---|---|
| `%1` Game name | **Yes** | Display name — use quotes for names with spaces |
| `%2` Assets dir | **Yes** | Absolute path to game assets folder (must contain `main.lua`) |
| `%3` Config | No | `Debug` or `Release` (default: `Release`) |
| `%4` Icon `.ico` | No | Path to a Windows `.ico` file |

#### What the script produces

The script always assembles the staging folder first, then runs each packaging
step if the required tool is available:

```
<repo-root>\
    Tower_Defense_Monster.GameDir\      ← always produced
        Tower_Defense_Monster.exe
        audiere.dll  d3dcompiler_47.dll  ...  (runtime + plugin DLLs)
        Tower_Defense_Monster.ico
        launch.bat
        assets\
            main.lua
            ...
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
3. Copies runtime DLLs from `third-party\` (audiere, d3dcompiler, libEGL/libGLESv2)
4. Copies plugin DLLs from the bin output directory
5. Copies game assets to `GameDir\assets\`
6. Writes `launch.bat` (sets `GAME_SAVE_DIR=%APPDATA%\<GameName>\`)
7. Generates and runs NSIS (if `makensis` is in `PATH` or its default install location)
8. Generates the MSI via `cmake -P make-msi.cmake` (if `cmake` is in `PATH` and WiX is installed)
9. Creates a ZIP archive via PowerShell `Compress-Archive`

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
