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
DLLs (audio engine, OpenGL ES emulation, Lua, etc.) next to the output executable:

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
