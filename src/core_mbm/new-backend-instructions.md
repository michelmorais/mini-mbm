# Mini MBM — Implementing a New Rendering Backend

This document explains everything a new backend implementor (Vulkan, PlayStation, Nintendo,
Xbox GDK, etc.) needs to know about engine conventions, existing patterns, and the lessons
learned while porting to Metal.

---

## 1. Repository layout

```
include/core_mbm/     ← Public headers (shared by ALL backends, never modify for one backend)
src/core_mbm/         ← Backend-specific .cpp / .mm source files live here
src/render/           ← Higher-level rendering helpers (also needs backend .mm/.cpp files)
third-party/          ← Lodepng, miniz, stb (platform-neutral, link as-is)
```

Each backend is selected by a preprocessor define, e.g.:
- `USE_OPENGL_ES` — OpenGL ES 2.0 (Linux, Android, legacy Apple)
- `USE_DIRECTX9`  — Direct3D 9 (Windows MSVS)
- `USE_METAL`     — Apple Metal (macOS / iOS)
- `USE_DUMMY_BACK_END_ENGINE` — stub template for new backends (start here)

---

## 2. Getting started

### Initial build

Before doing anything else, perform a clean build to verify the toolchain is working.
The following command removes all stale build artefacts, then configures and builds for
the Apple/Metal target without audio:

```bash
git clean -xdf
mkdir build && cd build
cmake .. -DPLAT=MacOs -DAUDIO=none
make -j8
cd ..
```

`git clean -xdf` is important: it removes all previously compiled `.o` files before
starting on a new backend.  Stale object files from a prior backend can survive an
incremental build and be linked silently, producing confusing link errors or subtly wrong
behaviour that is very hard to diagnose.  Always do a clean build when switching backends
or after adding new source files.

`-DAUDIO=none` removes one layer of complexity from the initial port.  Add audio back
once the renderer is stable.

### What to implement first (and what to skip)

The engine has several optional subsystems.  Focus exclusively on the core C++ rendering
pipeline for the initial port and skip everything listed below until it is stable:

| Subsystem | Where it lives | Skip initially? |
|---|---|---|
| Lua scripting | `src/lua-wrap/` — C++ ↔ Lua bindings for every engine class | **Yes** |
| Audio | `third-party/portaudio/` | **Yes** — pass `-DAUDIO=none` |
| ImGui plugin | `plugins/imGui/` — in-engine editors | **Yes** |
| Box2D / Bullet / tiled / other plugins | `plugins/` subdirectories | **Yes** |

#### Lua bindings

Every engine class that is exposed to Lua has a corresponding binding file in
`src/lua-wrap/`.  Those bindings call the same C++ API you are implementing — once the
C++ side is solid, Lua support is a separate additive step that requires no changes to
backend files.  Implement and validate the C++ rendering side first, then add Lua
exposure.

#### ImGui plugin and editors

The engine ships with an ImGui-based editor suite (`plugins/imGui/`) that is used for
in-engine tooling (scene editor, shader editor, particle editor, texture packer, etc.).
The editors are Lua scripts in `editor/` that drive the ImGui C++ plugin.  They require
a fully working render backend **and** Lua integration before they can run.  Do not
attempt to build or test the editors during the initial port.

### Git workflow — add new files immediately, do not push

Whenever you **create** a new backend source file, `git add` it straight away:

```bash
git add src/core_mbm/specific-vulkan.cpp src/core_mbm/device-vulkan.cpp  # etc.
```

Why this matters: CMake generates build rules from the source file list at configure
time.  An untracked file can be silently omitted from an incremental build, leaving the
previous backend's stale `.o` linked instead of your new code.  Adding every new file to
the Git index ensures Make/Ninja always regenerates rules from the correct sources.

**Only do `git add` for new (untracked) files.**  Do not stage modifications to
already-tracked files — this keeps `git diff` / `git difftool` clean so the owner can
review exactly what changed.  You can verify which files are untracked with:

```bash
git status --short   # ?? = untracked (needs git add); M = modified (leave unstaged)
```

**Do not `git push` until the backend compiles cleanly and the project owner has reviewed
the new files.**  Keep the commits local until they have been inspected.

---

## 3. Files you must implement

Copy the nine dummy files and rename them for your backend:

| Dummy file | What it implements |
|---|---|
| `specific-dummy.cpp` | `SPECIFIC_AUX_CONTEXT_DEVICE` (device/window context struct) |
| `device-dummy.cpp` | `DEVICE` — init/quit, projection mode, depth test, pixel-perfect |
| `core-manager-dummy.cpp` | `CORE_MANAGER` — window creation, event loop, render loop |
| `shader-dummy.cpp` | `BUFFER_GL` + `BASE_SHADER` + `SHADER` — GPU buffers and draw calls |
| `shader-var-cfg-dummy.cpp` | `VAR_SHADER` constructor/destructor |
| `shader-resource-dummy.cpp` | `SHADER_RESOURCE_MANAGER` (shader source table) |
| `texture-manager-dummy.cpp` | `TEXTURE` — GPU texture upload/release |
| `blend-dummy.cpp` | `BLEND` — blend state |
| `mesh-manager-dummy.cpp` | `MESH_MBM_DEBUG::fillInSubsetDebug` |

Additionally in `src/render/`:

| Dummy file | What it implements |
|---|---|
| `render-2-texture-dummy.cpp` | `RENDER_2_TEXTURE` / `RENDERIZABLE_TO_TARGET` — off-screen render |

All dummy files use `REMINDER_TODO` macros to mark unimplemented functions.  Remove each
`REMINDER_TODO` as you implement the function.

### CMakeLists.txt

Add a new platform/backend branch in `src/core_mbm/CMakeLists.txt` by analogy with the
`Apple AND USE_METAL` block.  Wire up your framework/library link targets.

If your backend uses a language extension (Objective-C++, HLSL toolchain, etc.) set
`COMPILE_FLAGS` on those source files via `set_source_files_properties`.

---

## 4. The `SPECIFIC_AUX_CONTEXT_DEVICE` struct

This is the single object that owns everything backend-specific at the device level:
- The GPU device handle
- The swap-chain / surface
- Per-frame command encoder / command buffer
- Shared state (default sampler, depth-stencil state, etc.)

It is stored behind `DEVICE::Impl`. Backend code should access it through
`DEVICE::getSpecificContextDevice()` and update ownership through the private
`DEVICE::setSpecificContextDevice()` helper.

The struct **must** provide:
```cpp
void release(bool wasDeviceLost) noexcept; // called on device reset and shutdown
```

**Important:** any GPU object stored as an Objective-C `id<>` (Metal) or COM pointer (D3D)
must be properly retained/released.  On Metal every `.mm` file that stores `id<>` across
autoreleasepool boundaries must be compiled with `-fobjc-arc`.

---

## 5. Matrix convention — CRITICAL

The engine uses its own `MATRIX` type (see `include/core_mbm/primitives.h`):

```cpp
struct MATRIX {
    union {
        struct { float _11,_12,_13,_14, _21,…,_44; };
        float m[4][4];
        float p[16];   // ← flat float array, passed to GPU shaders
    };
};
```

### Memory layout

`MATRIX` is **row-major**, identical to `D3DMATRIX`.  Rows are stored contiguously:
`p[0..3]` = row 0 (_11 _12 _13 _14), `p[4..7]` = row 1, etc.

All geometry throughout the engine follows the **left-handed, row-vector convention**:
```
v_transformed = v × M     (row vector on the left)
MVP = modelView × projection
```

In code this looks like:
```cpp
MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->getCamera().matrixPerspective);
```

### Passing matrices to the GPU

**Direct3D 9** — cast `MATRIX*` to `D3DMATRIX*` (identical layout) and call `SetTransform`:
```cpp
const D3DMATRIX* mv = reinterpret_cast<const D3DMATRIX*>(&camera.matrixView);
pd3dDevice->SetTransform(D3DTS_VIEW, mv);
```

**OpenGL ES** — transpose by passing `GL_TRUE` to `glUniformMatrix4fv`,
or pre-transpose the matrix before upload.

**Metal / MSL** — pass `MATRIX::p` directly via `setVertexBytes`.  MSL `float4x4` is
column-major, so it automatically reads the same bytes as the transpose of your row-major
matrix.  In the vertex shader write:
```metal
out.pos = uniforms.mvpMatrix * float4(in.pos, 1.0);  // M * v (column-vector convention)
```
Because `M_MSL = M_engine^T`, this is mathematically equivalent to `v × M_engine`.
**No explicit transpose is needed — just pass `.p` and use `M * v` in the shader.**

**Vulkan / other** — Vulkan's GLSL uses column-major like Metal.  Same rule applies:
pass `MATRIX::p` as-is and use `M * v` in the vertex shader.  Additionally, Vulkan NDC
maps depth to [0, 1] (same as Metal and D3D, unlike OpenGL's [-1, 1]).

### Depth range (NDC)

The engine uses `MatrixPerspectiveFovLH` which produces a **[0, 1]** depth range
(D3D-style).  Backends that expect this range (Metal, Vulkan/VK_EXT_depth_zero_to_one,
D3D) work directly.  An OpenGL ES backend must either:
- Use `MatrixPerspectiveFovRH` (which produces [-1, 1]), or
- Remap depth in the projection matrix (scale `z` by `0.5`, bias by `0.5`).

### Camera matrices used for rendering

```cpp
camera.matrixPerspective    // 3D: view × projection combined
camera.matrixPerspective2d  // 2D: ortho view × orthographic projection combined
camera.matrixView           // 3D: view only (for lighting / normals)
camera.matrixProj           // 3D: projection only
```

---

## 6. Culling and winding order

Engine mesh files store culling and winding state per buffer as GL-style enum values:

```cpp
// BUFFER_GL fields:
uint32_t mode_cull_face;            // CULL_FRONT=0x0404, CULL_BACK=0x0405, CULL_FRONT_AND_BACK=0x0408
uint32_t mode_front_face_direction; // CW=0x0900, CCW=0x0901
```

These are defined in `include/core_mbm/draw-compatibility.h`:
```cpp
enum CULL_MODE     : uint32_t { CULL_FRONT=0x0404, CULL_BACK=0x0405, CULL_FRONT_AND_BACK=0x0408 };
enum FACE_DIRECTION: uint32_t { CW=0x0900, CCW=0x0901 };
```

**You must translate these values to your backend's equivalents every draw call.**  The
default for all standard 3D mesh assets is `CULL_BACK` + `CW` (clockwise front-faces).

Example translation table:

| Engine value | OpenGL ES | Metal | D3D 9 | Vulkan |
|---|---|---|---|---|
| `CULL_FRONT` (0x0404) | `GL_FRONT` | `MTLCullModeFront` | `D3DCULL_CW` | `VK_CULL_MODE_FRONT_BIT` |
| `CULL_BACK` (0x0405) | `GL_BACK` | `MTLCullModeBack` | `D3DCULL_CCW` | `VK_CULL_MODE_BACK_BIT` |
| `CULL_FRONT_AND_BACK` (0x0408) | `GL_FRONT_AND_BACK` | `MTLCullModeBack`* | — | `VK_CULL_MODE_FRONT_AND_BACK` |
| `CW` (0x0900) | `GL_CW` | `MTLWindingClockwise` | — | `VK_FRONT_FACE_CLOCKWISE` |
| `CCW` (0x0901) | `GL_CCW` | `MTLWindingCounterClockwise` | — | `VK_FRONT_FACE_COUNTER_CLOCKWISE` |

*Metal has no true FRONT_AND_BACK — use `MTLCullModeNone` or handle via blending.

**Forgetting to apply culling** will cause closed meshes to draw their inner faces, making
them appear hollow or incorrectly lit.

---

## 7. Depth buffer

A depth buffer is **required for correct 3D rendering**; without it back-faces win over
front-faces in submission order.

- Create a depth texture / renderbuffer matching the backbuffer dimensions every frame
  (or create once and resize on window resize).
- Attach it to the render pass with a clear value of `1.0` (far plane).
- Use **less-or-equal** depth comparison with depth writes enabled for normal 3D objects.
  This matches `GLDepthFunc(GL_LEQUAL)` used on all OpenGL ES platforms.  Using strict
  `less` breaks tile maps: transparent bricks write Z to the depth buffer; the next brick
  at the identical Z (same layer, overlapping pixel) fails the strict test and is
  discarded, showing a white/clear hole instead of the blended result.  See §A4.
- Depth range is **[0, 1]** (see §5).

The engine also calls `DEVICE::setDepthTest(bool)` and `DEVICE::clearDepth()` — implement
these to enable/disable the depth test and clear the depth buffer on demand.

On Metal the depth format of the `MTLRenderPipelineState` must match the format declared in
the render pass descriptor (`MTLPixelFormatDepth32Float`).  A mismatch causes a GPU error
at draw time even if the pipeline compiled without errors.

---

## 8. Vertex formats (`FVF_PROVIDE_BY_ENGINE`)

The engine selects one of four interleaved vertex layouts per mesh:

```cpp
enum class FVF_PROVIDE_BY_ENGINE {
    FVF_NONE,        // invalid — compileShader must return false
    FVF_POS,         // float3 position                        (12 bytes/vertex)
    FVF_POS_UV,      // float3 position + float2 uv            (20 bytes/vertex)
    FVF_POS_NOR,     // float3 position + float3 normal        (24 bytes/vertex)
    FVF_POS_NOR_UV   // float3 position + float3 normal + uv   (32 bytes/vertex)
};
```

`BUFFER_GL::fvf` is set automatically by `loadBuffer()`.  Your `compileShader()` receives
this value and must build/select a shader variant that matches the input layout.

For backends that pre-compile shaders (SPIR-V, DXIL) you will need a variant per FVF.
For backends that use runtime-compiled shaders (MSL, HLSL via D3DCompile) you can generate
the source string at compile time based on the FVF.

---

## 9. Buffer lifecycle

| Method | Called when |
|---|---|
| `BUFFER_GL::loadBuffer(VB, …)` | Static vertex-only meshes (lines, 2D quads) |
| `BUFFER_GL::loadBuffer(IB, …)` | Static indexed meshes (`.msh` 3D models, sprites) |
| `BUFFER_GL::loadBufferDynamic(…)` | Skinned / procedural meshes — creates writeable vertex buffer + static index buffer |
| `BUFFER_GL::updateDynamic(…)` | Called every frame to rewrite vertex data into the buffer created by `loadBufferDynamic` |
| `BUFFER_GL::loadParticleBuffer()` | Particle system — 6-index quad IB `{0,1,2,2,1,3}` shared for all particles |

All functions call `release()` first, so they are safe to call multiple times.

For `loadBufferDynamic` + `updateDynamic` the key insight is that the vertex buffer must be
CPU-writable every frame.  On Metal use `MTLResourceStorageModeShared`; on Vulkan use a
host-visible + host-coherent memory type; on D3D use `D3DUSAGE_DYNAMIC | D3DPOOL_DEFAULT`
and `Lock/Unlock`.

---

## 10. Shader uniforms (`MetalUniforms` / uniform struct)

Each `SHADER::render()` call must upload at minimum:

```cpp
struct Uniforms {
    float mvp[16];   // SHADER::mvpMatrix.p  — model-view-projection
    float mv[16];    // SHADER::modelView.p  — model-view (for lighting)
    float color[4];  // per-object tint color (r,g,b,a); default = {1,1,1,1}
};
```

The `color` field should be read from the pixel shader's `VAR_SHADER` named `"color"` when
present:
```cpp
const VAR_SHADER* cv = pShader ? pShader->getVarByName("color") : nullptr;
if (cv) memcpy(color, cv->current, sizeof(color));
else    color = {1,1,1,1};
```

Custom shader variables (`VAR_SHADER`) are stored in `BASE_SHADER::lsVar` with types
`VAR_FLOAT` / `VAR_VECTOR2` / `VAR_VECTOR` / `VAR_COLOR_RGB` / `VAR_COLOR_RGBA` (sizes 1–4
floats).  You can extend the uniform struct or use a separate uniform buffer per variable.

`BASE_SHADER::update(void* ptrShaderSpecific)` is the hook for uploading all custom
variables.  However, on Metal everything is pushed inline at draw time so this is a no-op
there — choose the approach that fits your backend.

---

## 11. Texture upload

`TEXTURE` currently keeps a compatibility union for the GPU handle:
```cpp
union { uint32_t idTexture; void* ptrTexture; };
```

- New backend/core code should use `getBackendTextureId()` / `setBackendTextureId()` /
  `getBackendTextureIdAddress()` for OpenGL-style integer handles.
- New backend/core code should use `getBackendTexturePointer()` /
  `setBackendTexturePointer()` / `getBackendTexturePointerAddress()` for pointer-backed
  handles.
- OpenGL ES uses the texture-id helper path (GLuint texture object).
- Metal/Vulkan/D3D12 use the pointer helper path (store a retained pointer / descriptor, cast via
  `(__bridge_retained void*)` on Metal, via raw pointer on others).

> **CRITICAL — 64-bit platforms: always use `ptrTexture`, never `idTexture`**  
> On 32-bit platforms `sizeof(void*) == sizeof(uint32_t)` so both union members alias
> cleanly.  On **64-bit** platforms (macOS, Linux x86_64, Windows x64) `sizeof(void*)` is
> 8 bytes, but `sizeof(uint32_t)` is only 4 bytes.  Storing a Metal/Vulkan/D3D12 pointer
> in `ptrTexture` and then reading it back through `idTexture` silently truncates the
> upper 32 bits, yielding a garbage or null value that will crash when used as a pointer.
>
> - **OpenGL ES (any bitness):** use the texture-id helpers exclusively.  `GLGenTextures`
>   and `GLDeleteTextures` take a `GLuint*` (4 bytes), so use `getBackendTextureIdAddress()`.
>   Widening `idTexture`
>   to 64 bits would corrupt those calls because the GL functions only write/read 4 bytes.
>   The union is intentionally split by backend for this reason — never widen `idTexture`.
> - **Metal / Vulkan / D3D12 on 64-bit:** use the texture-pointer helpers everywhere.
>   Never read or compare `idTexture` in these backends, not even for null checks.
>
> **Real crash (macOS/Metal, tilemap plugin):** a code path checked `texture->idTexture`
> to test whether a Metal texture was loaded.  The Metal texture had a valid non-null
> `ptrTexture`, but the lower 32 bits of the pointer happened to be zero, so the equality
> test returned `true` and the texture was treated as missing — causing a null dereference
> one call later.  The fix: use `getBackendTexturePointer()` exclusively in all Metal
> code paths.

`TEXTURE::release()` must free the GPU texture when called.  `loadFromData()` receives a
decoded RGBA byte buffer + dimensions; upload it to the GPU and store the handle.
`loadNativeEngine()` may return `nullptr` to let the common code decode PNG via lodepng
first and then call `loadFromData()`.

---

## 12. Render-to-texture

`RENDER_2_TEXTURE` renders a sub-scene to an off-screen texture.  The texture is then used
as a regular 2D object in the main scene.

Required steps:
1. `createTextureRenderTarget(w, h)` — create an off-screen color texture +
   depth texture, store in `SPECIFIC_AUX_CONTEXT_DEVICE` or via `specificConfig`.
2. `CORE_MANAGER::renderToTargets()` — iterate render targets with
   `device->getTotalRenderTargets()` and `device->getRenderTarget(index)`,
   begin a secondary render pass for each target, render its object list, end the pass.
3. The result texture is then available for sampling in the main pass.

> **CRITICAL — call `setTextureCapabilities()` inside `initGraphics()`**  
> `TEXTURE_MANAGER::maxTextureSize` is initialized to `0`.  Every
> `createTextureRenderTarget()` call checks `width > maxTextureSize` before
> allocating — if you forget this call, every render target will fail silently.
> Call it at the end of your backend's `initGraphics()` with the GPU's real
> limits, e.g.:
> ```cpp
> mbm::TEXTURE_MANAGER* tm = mbm::TEXTURE_MANAGER::getInstance();
> tm->setTextureCapabilities(16384, 16384, 16384); // Metal: 16 K on all families
> ```
> Other backends (OpenGL ES, DirectX9) already do this — do not forget it.

> **UV orientation — display quad (`fillvertexQuad`)**  
> When the captured texture is displayed as a 2-D quad, the V coordinate must
> match the render-target's row origin:
> - **OpenGL ES** — FBO row 0 is at the *bottom* → `uvOriginBottomLeft = false`
> - **Metal / DirectX9** — texture row 0 is at the *top* → `uvOriginBottomLeft = true`  
> See `RENDER_2_TEXTURE::fillvertexQuad()` in `render-2-texture.cpp` —
> add your backend's define to the existing `#if defined(USE_DIRECTX9) || defined(USE_METAL)` guard.

---

## 13. Platform event loop (`handleEventFromWindow`)

`CORE_MANAGER::handleEventFromWindow()` is backend-specific.  It must:

1. Process OS events (window messages, input callbacks).
2. Translate pointer/touch coordinates from OS units to **logical points** before pushing
   events.  `DEVICE::getBackBufferWidth()` / `DEVICE::getBackBufferHeight()` always return **logical points** — the coordinate
   space the game scene uses.  The rendering-surface size (swapchain extent,
   `drawableSize`, EGL surface, etc.) uses *physical* pixels (`logical × scale`), but
   that is a separate value the game loop never reads directly.  Do **not** multiply
   input coordinates by the scale factor before comparing with the backbuffer size.
3. Push events through the `CORE_MANAGER::onTouch*`, `onKey*`, `onResizeWindow`,
   `onMoveWindow`, and joystick callback methods. These methods enqueue `EVENT_KEY`
   internally; do not access the event queue storage directly.
4. Call `CORE_MANAGER::onLoop(singleLoop, doSwapBuffers)` to drive the game loop.

Mouse/touch Y origin: the engine origin is **bottom-left** (Y increases upward), matching
OpenGL convention.  Most desktop OSes put Y=0 at the top of the window.  Apply Y-flip:
```cpp
float ey = device->getBackBufferHeight() - os_y;  // both in logical points — no scale factor
```

> **CRITICAL — backbuffer size = logical points, rendering surface = physical pixels**
> Storing physical pixels (e.g. `width * retinaScale`) with `DEVICE::setBackBufferSize()` makes
> the orthographic camera span physical pixels, so every 2-D object appears at *half*
> the expected size on a 2× Retina / HiDPI display compared with a non-Retina machine.  
> Rule of thumb:
> - `DEVICE::getBackBufferWidth()` / `DEVICE::getBackBufferHeight()` — logical points (what the programmer thinks of as screen size)
> - `drawableSize` / swapchain extent — physical pixels = logical × `contentsScale` / DPI scale  
> - Input event coordinates — logical points (no multiplication needed on macOS/Windows)

> **CRITICAL — read the actual window size *after* the OS has shown and constrained the window**  
> On every desktop OS, if you request a window larger than the available screen area the OS
> silently shrinks it.  That shrink happens during or after `makeKeyAndOrderFront` /
> `ShowWindow` / `XMapWindow`, **not** when you create the window object.  Reading
> `contentView.bounds` (macOS) or the equivalent before the window is visible gives the
> *requested* size, not the *actual* size.  This creates a mismatch: `expectedScreen` is
> set once on the first `onLoop()` tick from the backbuffer size, and if those values
> are wrong (e.g. 1 600 instead of 1 470) then `adjustScaleScreen2d()` computes a scale
> ≠ 1 every frame, offsetting every `is2dS` object.  
> Fix: pump the run-loop for ≥ 50 ms after showing the window, then read the actual
> bounds and assign both `DEVICE::setBackBufferSize()` **and** the native surface size from them.

> **CRITICAL — `setProjectionMode` must only rebuild camera matrices**  
> `setProjectionMode` is called every frame from inside the render loop.  It must contain
> **only** a call to `updateCam` (or equivalent matrix rebuild).  Never reassign
> the backbuffer size or the native surface size (e.g. `drawableSize`) from inside
> this function.  Doing so overwrites the values set by `initGraphics()` and
> `resetDeviceWithNewDimensions()` every frame, corrupting the coordinate system
> (sub-pixel gaps, wrong scale, lost Retina resolution).  Those values belong exclusively
> to the init path and the resize handler.

---

## 14. Milestone checklist

Implement features in this order to reach a testable state as early as possible.
For each milestone, check the Metal implementation in `src/core_mbm/shader-metal.mm`
and `src/core_mbm/texture-manager-metal.mm` as a concrete reference.

- [ ] **M1 — Window + clear screen**: `initGraphics`, `beginRender`, `endRender`,
      `swapBuffers`, background color.  Run testLib; a coloured window should appear.
- [ ] **M2 — Textures**: `TEXTURE::loadFromData`, `TEXTURE::loadFromResourceData`,
      `TEXTURE::release`.  PNG images should decode and display.
- [ ] **M3 — Shaders + static buffers**: `compileShader`, `loadBuffer(VB)`,
      `loadBuffer(IB)`, `render`.  3D meshes and 2D quads should draw correctly.
- [ ] **M4 — Culling + depth**: apply `mode_cull_face` + `mode_front_face_direction` per
      draw call; attach depth buffer to render pass.  Meshes should stop showing inner faces.
- [ ] **M5 — Dynamic buffers**: `loadBufferDynamic`, `updateDynamic`.
      Skinned meshes, line meshes, and text rendering require this.
- [ ] **M6 — Particles**: `loadParticleBuffer`, `renderParticle(PARTICLE_CONTROL*)`.
- [ ] **M7 — Render-to-texture**: `createTextureRenderTarget`, `renderToTargets`.
- [ ] **M8 — Custom shaders**: `BASE_SHADER::addVar`, `BASE_SHADER::update`,
      `VAR_SHADER` constructor with backend handle.  See §15 for the shader catalogue
      and §A2 for Metal-specific notes (PSO variants, FVF attribute patching).
- [ ] **M9 — Fluid particles**: `renderParticle(FLUID_GROUP*)`.
- [ ] **M10 — Utilities**: `saveAsPNG`, pixel-perfect filtering, HMD support.
      `HMD.cpp` is platform-agnostic and builds on M7.

---

## 15. Built-in shader catalogue — CFG format and reserved names

### CFG triple format

`getShaderEngineBuiltIn()` (implemented in `shader-resource-<backend>.cpp/mm`) returns a
`const char**` array organised in **groups of three** null-terminated strings:

```
"<name>.(ps|vs)",          // 1 – filename key used to find the shader
"<shader source code>",    // 2 – full GLSL / MSL source text
"<cfg description>\n",     // 3 – variable declarations (see below)
```

A `nullptr, nullptr, nullptr` sentinel terminates the list.  The CFG string is
**100 % backend-independent** — the same text is used by all backends; never change it
for a single backend.

**Variable declaration syntax** (in the CFG string, third element of each triple):

```
[<shader-key>] = <name>.(ps|vs)                           // required header line
[<shader-key>][float][varName]       = min V max V default V
[<shader-key>][vector2][varName]     = min X Y max X Y default X Y
[<shader-key>][rgb][varName]         = min R G B max R G B default R G B
[<shader-key>][rgba][varName]        = min R G B A max R G B A default R G B A
```

Supported types: `float`, `vector2`, `rgb` (vec3 float), `rgba` (vec4 float).  Use
`>= 0.5` / `< 0.5` in shader code for boolean behaviour — no separate bool type.

---

### Reserved names — do NOT rename across backends

`SHADER::compileShader` (in `shader-opengl_es.cpp`) scans the shader source text for
these exact strings to decide which uniform/attribute handles to look up.  Your backend
**must** honour the same names so the rest of the engine can find them:

| Name | Kind | GLSL binding | Metal convention |
|---|---|---|---|
| `aPosition` | vertex attribute | `attribute vec4 aPosition` | `[[attribute(0)]]` (float3) |
| `aNormal` | vertex attribute | `attribute vec3 aNormal` | `[[attribute(1)]]` if present |
| `aTextCoord` | vertex attribute | `attribute vec2 aTextCoord` | `[[attribute(1)]]` or `[[attribute(2)]]` if normal present |
| `mvpMatrix` | uniform | `uniform mat4 mvpMatrix` | `Uniforms.mvp` at `[[buffer(1)]]` |
| `mvMatrix` | uniform | `uniform mat4 mvMatrix` | `Uniforms.mv` at `[[buffer(1)]]` |
| `sample0` | sampler | `uniform sampler2D sample0` | `[[texture(0)]]` + `[[sampler(0)]]` |
| `sample1` | sampler | `uniform sampler2D sample1` | `[[texture(1)]]` + `[[sampler(0)]]` |
| `color` | uniform (optional) | `uniform vec4 color` | custom uniforms struct / `VAR_SHADER` |

The engine never passes these names to `glGetUniformLocation` / equivalent at draw
time; they are only searched once during `compileShader` to cache handles.  In a Metal
backend they map to fixed buffer/texture slots that the render() methods hard-code.

---

### GLSL → MSL translation rules

When translating the built-in catalogue to MSL:

| GLSL | MSL |
|---|---|
| `precision mediump float;` | *omit* (Metal has no precision qualifiers) |
| `uniform sampler2D sample0` | `texture2d<float> sample0 [[texture(0)]]` + `sampler samp [[sampler(0)]]` |
| `texture2D(sample0, uv)` | `sample0.sample(samp, uv)` |
| `varying vec2 vTexCoord` | pass-through in vertex-out struct: `float2 uv;` |
| `gl_FragColor = c` | `return c;` (fragment function returns `float4`) |
| `gl_Position = ...` | `out.pos = ...; return out;` |
| `discard;` | `discard_fragment();` |
| `attribute vec4 aPosition` | `float3 pos [[attribute(0)]]` |
| `attribute vec2 aTextCoord` | `float2 uv [[attribute(N)]]` (N = 1 or 2, see table above) |
| `uniform float x` | field in custom `FragUniforms` struct at `[[buffer(2)]]` |
| `uniform vec2 x` | `float2 x` in `FragUniforms` |
| `uniform vec3 / rgb` | `float3 x` in `FragUniforms` |
| `uniform vec4 / rgba` | `float4 x` in `FragUniforms` |
| `vec2 / vec3 / vec4 / mat4` | `float2 / float3 / float4 / float4x4` |
| `atan(y, x)` | `atan2(y, x)` |
| `mod(a, b)` | `fmod(a, b)` |
| `mix(a, b, t)` | `mix(a, b, t)` *(same)* |
| `int loop_var; for(; cond;)` | `int` OK but `uint` preferred; Metal supports `for` |
| `xlat_lib_sincos(angle, s, c)` | `s = sin(angle); c = cos(angle);` (inline, Metal has no `sincos`) |

**Metal buffer slot conventions used by this engine:**

| Slot | Content |
|---|---|
| `[[buffer(0)]]` | vertex data (position / normal / uv interleaved or separate) |
| `[[buffer(1)]]` | `Uniforms` struct: `{ float4x4 mvp; float4x4 mv; float4 color; }` |
| `[[buffer(2)]]` | custom `FragUniforms` struct (per-shader; produced from `VAR_SHADER` list) |

**Custom uniforms (M8):** each built-in shader that has `[type][varName]` lines in
its CFG string needs a matching `struct FragUniforms` in its MSL source.  The fields
must appear **in the same order** as the CFG variable declarations so that
`VAR_SHADER::ptrHandleVar` (which stores a byte offset into this struct) works
correctly.  `BASE_SHADER::update()` in the Metal backend writes all current values
into a stack `FragUniforms` buffer and calls `setFragmentBytes:length:atIndex:2`.

**Prewritten VS programs and FVF attribute indices:** `scale.vs` and `simple texture.vs`
are stored as complete MSL programs with a hardcoded `struct VIn` that maps
`uv` to `[[attribute(1)]]`.  This is only correct for `FVF_POS_UV`; for
`FVF_POS_NOR_UV` the interleaved buffer places normal at slot 1 and UV at slot 2.
`compileShader` must call `patchVInStruct(vsStr, fvf)` to rewrite the `struct VIn`
block with the FVF-correct attribute indices.  OpenGL ES is immune because it uses
separate VBOs per stream and binds `aTextCoord` by name.

**`blend.ps` requires two textures:** `blend.ps` always samples both `sample0` and
`sample1`.  When only one texture is bound (common for single-texture sprites) the
behaviour of the unbound sampler is implementation-defined and can produce a
degenerate alpha value, making the object invisible.  When using `blend.ps` ensure
a second texture is always bound, even if it is a 1×1 white placeholder.  This is
a `blend.ps`-specific constraint; pixel shaders that use only `sample0` (e.g.
`bands.ps`, `font.ps`) are unaffected.

---

## 16. Key source files for reference

| File | Backend | Notes |
|---|---|---|
| `src/core_mbm/shader-metal.mm` | Metal | Complete implementation of all buffer + shader methods |
| `src/core_mbm/core-manager-metal.mm` | Metal | `beginRender` with depth buffer, `renderToTargets` stub |
| `src/core_mbm/core-manager-metal-macos.mm` | Metal/macOS | Event loop, Y-flip, Retina scale |
| `src/core_mbm/texture-manager-metal.mm` | Metal | Texture upload using lodepng decode path |
| `src/core_mbm/device-metal.mm` | Metal | `setProjectionMode`, `setDepthTest`, `clearDepth` |
| `src/core_mbm/shader-opengl_es.cpp` | OpenGL ES | Reference for culling, winding, uniform upload |
| `src/core_mbm/shader-directx9.cpp` | D3D9 | Reference for dynamic buffer lock/unlock pattern |
| `src/core_mbm/primitives.cpp` | All | `MatrixPerspectiveFovLH`, `MatrixLookAtLH`, `MatrixOrthoLH` |
| `include/core_mbm/draw-compatibility.h` | All | `CULL_MODE`, `FACE_DIRECTION`, `MODE_DRAW` enum values |
| `include/core_mbm/shader.h` | All | `BUFFER_GL`, `BASE_SHADER`, `SHADER`, `FVF_PROVIDE_BY_ENGINE` |
| `include/core_mbm/specific-metal.h` | Metal | Example `SPECIFIC_AUX_CONTEXT_DEVICE` struct layout |

---

## 17. Plugin porting notes (ImGui, Tiled, Box2D and others)

Plugins live under `plugins/` and are loaded as shared libraries or compiled directly into
the engine depending on build flags.  Build with `-DUSE_ALL=1` to include all of them, or
enable individual plugins with `-DUSE_IMGUI=1`, `-DUSE_TILED=1`, etc.  Plugins call the
same C++ API as the rest of the engine — if the core render backend is solid, most plugin
code will work without modification.  The exceptions documented below were all encountered
during the macOS/Metal port.

### Shared library file extensions by platform

The engine loads plugins at runtime by filename.  The file extension varies by OS:

| Platform | Extension | Notes |
|---|---|---|
| Windows | `.dll` | Dynamic-link library |
| Linux / Android | `.so` | Shared object |
| macOS / iOS | `.dylib` | Dynamic library (distinct from `.so`) |
| PlayStation / Nintendo / Xbox GDK | vendor-specific | Consult the platform SDK |

When the engine or a Lua script constructs a plugin filename at runtime (e.g.
`"box2d" .. ext`), make sure the correct extension is used for the target OS.  CMake
exposes `CMAKE_SHARED_LIBRARY_SUFFIX` which resolves to the right value automatically
if you set plugin output names without a hardcoded extension.

### Plugin lifecycle — `onPrepare`, `onLoop`, `onRender`

Every plugin implements the `PLUGIN_INTERFACE` with three per-frame callbacks:

| Callback | Timing | Metal encoder state |
|---|---|---|
| `onPrepare()` | Before `beginRender()` | No encoder active (`currentEncoder == nil`) |
| `onLoop(delta)` | After `onPrepare()`, before `beginRender()` | No encoder active |
| `onRender()` | After all scene objects render, before `endRender()` | Encoder **is** active |

Any plugin code that needs to submit draw calls or bind GPU resources **must** run in
`onRender()`.  Code that allocates GPU resources (creating `MTLBuffer`, uploading textures)
does **not** need the encoder — it can run in `onPrepare()` or `onLoop()` safely.

### ImGui plugin (`plugins/imGui/`)

**`ImGui_Metal_NewFrame()` timing:**  
The Metal ImGui backend's `NewFrame()` reads `currentPassDescriptor` (set by
`beginRender()`) to obtain the render-target's `sampleCount`.  Calling it before
`beginRender()` gives a nil descriptor, which yields `sampleCount = 0`, and the GPU
rejects the pipeline state at draw time with a validation error.  
*Fix:* call `ImGui_Metal_NewFrame()` inside `onRender()`, after `beginRender()` has
already run.  Do **not** put it in `onPrepare()`.

**Retina / HiDPI `DisplayFramebufferScale`:**  
ImGui needs to know the ratio of physical pixels to logical points so it can render text
and geometry at the correct density.  If `io.DisplayFramebufferScale` is left at its
default `{1, 1}`, every glyph and widget will render at half the intended size on a 2×
Retina display.  
*Fix:* inside `ImGui_Metal_NewFrame()` (or `onRender()` before `ImGui::Render()`),
compute the scale from the actual drawable texture:
```cpp
ImGuiIO& io = ImGui::GetIO();
id<MTLTexture> colorTex = currentPassDescriptor.colorAttachments[0].texture;
if (io.DisplaySize.x > 0)
    io.DisplayFramebufferScale = ImVec2(
        colorTex.width  / io.DisplaySize.x,
        colorTex.height / io.DisplaySize.y);
```

**ImGui scale flicker on mouse leave (macOS):**  
The macOS event handler (`NSEventTypeAppKitDefined`) fired on every window event including
mouse activity.  If that handler computes a new window size by multiplying logical bounds
by `backingScaleFactor` (physical pixels) and then passes that to `onResizeWindow()`, the
backbuffer dimensions oscillate between physical and logical pixels every time the mouse
enters or leaves the window — causing ImGui (and all 2D objects) to flicker between the
correct size and half-size.  
*Fix:* in the resize event handler use logical points only (do **not** multiply by
`backingScaleFactor`).  Let `resetDeviceWithNewDimensions()` handle the physical drawable
size internally via `contentsScale`.  See §13 for the general rule.

**Lua `onFrame` callback timing:**  
The ImGui Lua integration calls `ImGui::NewFrame()` in `onPrepare()` and `ImGui::Render()`
in `onRender()`.  Lua code (button callbacks, editor logic) therefore executes between
these two calls — i.e. during the frame but **before** `beginRender()`.  This means Lua
callbacks can safely allocate GPU resources (upload textures, create vertex buffers) but
should not attempt to issue draw calls.

### Tiled / tilemap plugin (`plugins/tiled/`)

**`idTexture` vs `ptrTexture` crash on 64-bit (segfault on macOS):**  
The tilemap plugin's "create tile set" button triggered a segfault when clicked on macOS.
The crash was caused by plugin code reading `texture->idTexture` (a `uint32_t`) to check
whether a Metal texture was loaded.  On the 64-bit macOS build the Metal texture pointer
was stored in `ptrTexture` (8 bytes); its lower 32 bits happened to be zero, so the
`idTexture == 0` check incorrectly concluded the texture was missing and proceeded to
dereference a null pointer.  
*Fix:* use `texture->getBackendTexturePointer() != nullptr` for Metal (or any pointer-backed
backend) everywhere in plugin code.  See §11 for the full explanation of the compatibility
handle layout.

**`TILE_EDITOR::renderTileSet()` missing null guard:**  
After a tile set is successfully created (at least one entry in `tile_sets`), the next
render frame calls `this->getAnimation(0)` inside `renderTileSet()` without checking the
return value for null.  If `createAnim()` fails (e.g. shader not found) the animation list
is empty and the subsequent dereference crashes.  Always guard:
```cpp
ANIMATION* anim = this->getAnimation(0);
if (anim == nullptr) return false;  // guard required
```

### Box2D / Bullet / other physics plugins (`plugins/box2d/`, `plugins/bullet3d/`)

These plugins are pure CPU physics; they have no GPU dependencies and port without
modification.  The only requirement is that the engine core compiles cleanly for your
platform (64-bit struct sizes, endianness) before enabling them.

### General plugin checklist for a new 64-bit backend

- [ ] Replace every `texture->idTexture` check with
      `texture->getBackendTexturePointer() != nullptr` in plugin code that runs on a
      pointer-backed backend.
- [ ] Verify `onPrepare()` / `onRender()` split: GPU resource creation in `onPrepare()`,
      draw calls in `onRender()` (encoder active).
- [ ] Any backend-specific `NewFrame()` call (ImGui, etc.) must be deferred to
      `onRender()` if it needs an active render pass descriptor.
- [ ] `DisplayFramebufferScale` (or equivalent HiDPI scale) must be set explicitly;
      default `{1, 1}` produces half-size UI on Retina / HiDPI displays.
- [ ] Event-handler resize paths must use **logical points**, not physical pixels, when
      updating the backbuffer size. See §13.

---

## 18. 3D → 2dw depth separation

### Problem

The engine renders frames in three sequential passes (see `core-manager-common.cpp`):

1. **3D pass** — perspective projection, depth test on, depth write on.
2. **2dw pass** — orthographic projection, depth test on, depth write on.
   Objects are pre-sorted back-to-front by `prepareRender2d()` (painter's algorithm).
3. **2ds pass** — screen-space UI, depth test off.

Without an explicit depth-buffer clear between the 3D and 2dw passes, the perspective
depth values written during the 3D pass remain in the depth buffer when the 2dw pass
begins.  The orthographic projection maps Z differently from the perspective projection
(e.g. a wall mesh at depth ≈ 0.1 in perspective may overlap the same depth region as a
2dw sprite), so some 2dw objects fail the depth test against 3D geometry even though they
should be fully visible.  The result is incorrect overlap between 2dw objects — objects
whose back-to-front order is correct per their Z value appear to be occluded by other 2dw
objects (or by invisible 3D geometry remnants in the depth buffer).

The same root cause also means that `setDepthTest(false)` **must** be functional for the
2ds pass; if it is a no-op, 2ds screen-space UI elements can be occluded by 2dw objects
behind them.

### Fix — three parts

#### Part 1 — clear depth between 3D and 2dw passes (`core-manager-common.cpp`)

Call `device->clearDepth()` immediately after `setProjectionMode(false)` and before the
first `setDepthTest(true)` / 2dw draw loop:

```cpp
device->setProjectionMode(false, device->getBackBufferWidth(), device->getBackBufferHeight());
device->setTotalObjectsIsRendering2D(0);
// Clear the depth buffer so 3D perspective depth values do not occlude 2dw
// objects whose depth comes from the orthographic projection.
device->clearDepth();
device->setDepthTest(true);
for (auto ptrRender : lsRender2dw)
    ...
```

`clearDepth()` must clear **depth only** — colour must be preserved so the 3D scene is
not erased.

#### Part 2 — depth-only clear on each backend

**OpenGL ES (`device-opengl_es.cpp`):**
```cpp
void DEVICE::clearDepth()
{
    // Depth only — colour intentionally preserved (3D scene must not be erased).
    GLClearDepthf(1.0f);
    GLClear(GL_DEPTH_BUFFER_BIT);   // NOT GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
}
```

**Direct3D 9 (`device-directx9.cpp`):**
```cpp
void DEVICE::clearDepth()
{
    auto* ctx = getSpecificContextDevice();
    ctx->pd3dDevice->Clear(0, NULL,
        D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,  // NOT D3DCLEAR_TARGET
        D3DCOLOR_XRGB(0,0,0), 1.0f, 0);
}
```

**Metal (`device-metal.mm`) — encoder restart pattern:**  
Metal does not support clearing a single attachment in the middle of a render pass.  The
only way to change load/store actions mid-frame is to end the current encoder and start a
new one.  To preserve the 3D scene in the colour attachment, open the new pass with
`MTLLoadActionLoad` for colour and `MTLLoadActionClear` for depth:

```objc
void DEVICE::clearDepth()
{
    auto* ctx = getSpecificContextDevice();
    if (!ctx->currentEncoder || !ctx->currentCommandBuffer) return;

    [ctx->currentEncoder endEncoding];
    ctx->currentEncoder = nil;

    MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
    // Colour: load existing pixels (3D scene)
    desc.colorAttachments[0].texture     = ctx->currentPassDescriptor.colorAttachments[0].texture;
    desc.colorAttachments[0].loadAction  = MTLLoadActionLoad;
    desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    // Depth: clear to 1.0 (far plane)
    desc.depthAttachment.texture     = ctx->depthTexture;
    desc.depthAttachment.loadAction  = MTLLoadActionClear;
    desc.depthAttachment.clearDepth  = 1.0;
    desc.depthAttachment.storeAction = MTLStoreActionDontCare;

    ctx->currentEncoder = [ctx->currentCommandBuffer renderCommandEncoderWithDescriptor:desc];
    ctx->currentEncoder.label = @"MBM Encoder (2D)";
    ctx->currentPassDescriptor = desc;
}
```

#### Part 3 — implement `setDepthTest` on Metal (`device-metal.mm` + `specific-metal.h`)

Metal bakes the depth-stencil state into the pipeline (`MTLDepthStencilState`).  Unlike
OpenGL or D3D you cannot toggle depth testing with a single API call — you must pre-build
two states and switch between them at draw time.

1. Add a flag to `SPECIFIC_AUX_CONTEXT_DEVICE` (`specific-metal.h`):
   ```cpp
   // Tracks whether depth testing is currently enabled (toggled by DEVICE::setDepthTest)
   bool depthTestEnabled = true;
   ```

2. Implement `setDepthTest`:
   ```objc
   void DEVICE::setDepthTest(const bool enable)
   {
       auto* ctx = getSpecificContextDevice();
       ctx->depthTestEnabled = enable;
   }
   ```

3. In `SHADER::render()` and `SHADER::renderDynamic()` (`shader-metal.mm`), select the
   appropriate pre-built depth state before each draw call:
   ```objc
   [enc setDepthStencilState: ctx->depthTestEnabled
       ? getOrCreateDepthStencilState(ctx)   // less comparison + depth write
       : getOrCreateNoDepthState(ctx)];      // always pass + no write (2ds / particles)
   ```

   Both helper functions (`getOrCreateDepthStencilState` and `getOrCreateNoDepthState`)
   already exist in `shader-metal.mm` — just call the right one based on the flag.

### Summary of files changed

| File | Change |
|---|---|
| `src/core_mbm/core-manager-common.cpp` | Added `device->clearDepth()` between 3D and 2dw loops |
| `src/core_mbm/device-metal.mm` | Implemented `clearDepth()` (encoder restart) and `setDepthTest()` |
| `include/core_mbm/specific-metal.h` | Added `bool depthTestEnabled = true` to context struct |
| `src/core_mbm/shader-metal.mm` | `render()` and `renderDynamic()` select depth state via `depthTestEnabled` |
| `src/core_mbm/device-opengl_es.cpp` | `clearDepth()` — removed `GL_COLOR_BUFFER_BIT` |
| `src/core_mbm/device-directx9.cpp` | `clearDepth()` — removed `D3DCLEAR_TARGET` |

---

---

# Appendix A — macOS / Metal implementation notes

This appendix documents macOS- and Metal-specific behaviour.  A Vulkan, PlayStation, or
other backend implementor can skip this section entirely during the initial port.

---

## A1. macOS modifier-key events (Shift, Control, Option, Command)

### Problem

On macOS, modifier keys (Shift, Control, Option/Alt, Command, Caps Lock) do **not** produce
`NSEventTypeKeyDown` / `NSEventTypeKeyUp` events.  Instead, the OS fires a single
`NSEventTypeFlagsChanged` event whenever the combined modifier-key state changes.  If the
event loop only handles `NSEventTypeKeyDown` and `NSEventTypeKeyUp`, all modifier keys are
silently invisible to the engine — game logic that checks
`XK_Shift_L`, `XK_Control_L`, etc. will never fire.

### Fix — handle `NSEventTypeFlagsChanged` in `handleEventFromWindow`

Track the previous modifier-flag state (static local, reset to 0 at startup) and diff
it against the current state on every `FlagsChanged` event.  For each bit that toggled on,
fire `onKeyDown`; for each bit that toggled off, fire `onKeyUp`.

```objc
// At the top of handleEventFromWindow() — persists across calls:
static NSEventModifierFlags previousModifierFlags = 0;

// Inside the switch(event.type):
case NSEventTypeFlagsChanged:
{
    NSEventModifierFlags cur =
        event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
    NSEventModifierFlags prev = previousModifierFlags;
    previousModifierFlags = cur;

    auto dispatchMod = [&](NSEventModifierFlags flag, int keyCode)
    {
        bool wasDown = (prev & flag) != 0;
        bool isDown  = (cur  & flag) != 0;
        if      (!wasDown && isDown)  this->onKeyDown(keyCode);
        else if ( wasDown && !isDown) this->onKeyUp(keyCode);
    };

    dispatchMod(NSEventModifierFlagShift,    0xFFE1); // XK_Shift_L
    dispatchMod(NSEventModifierFlagControl,  0xFFE3); // XK_Control_L
    dispatchMod(NSEventModifierFlagOption,   0xFFE9); // XK_Alt_L   (Option ⌥)
    dispatchMod(NSEventModifierFlagCommand,  0xFFEB); // XK_Super_L (Command ⌘)
    dispatchMod(NSEventModifierFlagCapsLock, 0xFFE5); // XK_Caps_Lock
}
break;
```

`NSEventModifierFlagDeviceIndependentFlagsMask` strips hardware-specific bits so the
comparison is stable across keyboards.

### Key-code table

The XK constants used above match those defined in `src/lua-wrap/framework-apple-lua.cpp`
and consumed by the Lua key-name table:

| macOS flag | XK constant | Lua key name |
|---|---|---|
| `NSEventModifierFlagShift` | `0xFFE1` (`XK_Shift_L`) | `"shift"` |
| `NSEventModifierFlagControl` | `0xFFE3` (`XK_Control_L`) | `"control"` |
| `NSEventModifierFlagOption` | `0xFFE9` (`XK_Alt_L`) | `"alt"` |
| `NSEventModifierFlagCommand` | `0xFFEB` (`XK_Super_L`) | `"windows"` (matches DX9) |
| `NSEventModifierFlagCapsLock` | `0xFFE5` (`XK_Caps_Lock`) | `"caps lock"` |

> **Note on regular key events and modifiers:**
> `translateMacKeyCode()` uses `[event charactersIgnoringModifiers]` to retrieve the base
> key code, which is correct — letter keys always produce uppercase ASCII regardless of
> Shift state, matching the convention used on Windows and Linux.  Because
> `NSEventTypeFlagsChanged` fires **before** the following `NSEventTypeKeyDown`, the
> engine already knows Shift/Control/etc. are held by the time the regular key event
> arrives.

### File changed

| File | Change |
|---|---|
| `src/core_mbm/core-manager-metal-macos.mm` | Added `static NSEventModifierFlags previousModifierFlags = 0` + `NSEventTypeFlagsChanged` case |

---

## A2. Blend state on Metal

### Problem

Metal bakes the blend equation and blend factors into `MTLRenderPipelineState` at compile
time — there is no `glBlendFunc` / `SetRenderState` equivalent that can change blending
per draw call.  The engine's `RENDER_STATE::set(BLEND_STATE)` and `FX::setBlendOp()` are
called at draw time; on the Metal stub they were no-ops, so all objects rendered with the
same hardcoded `standardPSO` (alpha blend) regardless of what the scene requested.

### Solution — two PSO variants per compiled shader

`compileShader()` in `shader-metal.mm` already builds a `MBMPSOPair` holding:

| PSO | Blend equation | Equivalent GL call |
|---|---|---|
| `standardPSO` | `src_alpha × src + (1−src_alpha) × dst` | `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` |
| `additivePSO` | `src_alpha × src + 1 × dst` | `glBlendFunc(GL_SRC_ALPHA, GL_ONE)` — `BLEND_ONE` |

This covers the two modes used by all current engine content.

### Implementation — `currentBlendState` field

1. **`include/core_mbm/specific-metal.h`** — add a field to `SPECIFIC_AUX_CONTEXT_DEVICE`:
   ```cpp
   // Tracks the destination blend factor requested via RENDER_STATE::set().
   // 2 = BLEND_ONE (additive); all others = standard alpha.
   int currentBlendState = 2; // BLEND_ONE matches OpenGL default: GL_SRC_ALPHA, GL_ONE
   ```

2. **`src/core_mbm/blend-metal.mm`** — `RENDER_STATE::set()` stores the enum value:
   ```cpp
   void RENDER_STATE::set(const BLEND_STATE blendState) const noexcept
   {
       auto* dev = mbm::DEVICE::getInstance();
       auto* context = dev ? dev->getSpecificContextDevice() : nullptr;
       if (context)
           context->currentBlendState = static_cast<int>(blendState);
   }
   ```

3. **`src/core_mbm/shader-metal.mm`** — `SHADER::render()` and `renderDynamic()` select
   the PSO before each draw:
   ```objc
   // BLEND_ONE (2) = additive; all others = standard alpha blend.
   [enc setRenderPipelineState:(ctx->currentBlendState == 2)
       ? pair.additivePSO : pair.standardPSO];
   ```

### Blend equations (`setBlendOp` / `setBlendDefaultOp`)

Both PSOs compile with `MTLBlendOperationAdd`.  `FX::setBlendOp()` values 2–5
(SUBTRACT, REVERSE SUBTRACT, MIN, MAX) would require additional PSO variants compiled
per shader.  They are not built currently — these three cases remain as documented no-ops
and continue to render with the ADD equation.  If a scene uses subtract/min/max blending,
add a third PSO variant to `MBMPSOPair` and select it via an extended `currentBlendState`
check in `render()`.

### Files changed

| File | Change |
|---|---|
| `include/core_mbm/specific-metal.h` | Added `int currentBlendState = 2` to context struct |
| `src/core_mbm/blend-metal.mm` | `RENDER_STATE::set()` stores blend state; `setBlendOp()` documented no-op |
| `src/core_mbm/shader-metal.mm` | `render()` and `renderDynamic()` select PSO via `currentBlendState` |

**M8 additional notes (Metal):**
- `VAR_SHADER` fields must appear **in CFG declaration order** so byte-offset handles computed by `VAR_SHADER::ptrHandleVar` are correct.
- Prewritten VS programs (`scale.vs`, `simple texture.vs`) hardcode `uv [[attribute(1)]]`. For `FVF_POS_NOR_UV` meshes the normal sits at slot 1 and UV at slot 2 — call `patchVInStruct(vsStr, fvf)` in `compileShader` to rewrite the `struct VIn` block.
- **Known issue — `blend.ps + scale.vs` invisible when scale > 0.5** (seen on both OpenGL ES and Metal): `blend.ps` always samples `sample1`; if only one texture is bound the blend formula collapses to transparent. Always bind a 1×1 white placeholder when using `blend.ps` with a single-texture object.

---

## A3. Mesh debug readback on Metal (`fillInSubsetDebug`)

### Background

`MESH_MBM_DEBUG::fillInSubsetDebug()` is called by `loadDebugFromMemory()` to copy
vertex and index data from GPU buffers back to CPU memory for the in-engine mesh
debug/editor visualisation (collision shapes, bone overlays, UV display).  On OpenGL ES
this requires the `GL_OES_mapbuffer` extension to map VBOs.  On Direct3D 9 it is not
implemented at all.

### Metal advantage — Shared storage mode

All Metal vertex and index buffers in this engine are created with
`MTLResourceStorageModeShared`.  On Apple Silicon (and on Intel Macs with unified memory)
the CPU and GPU share the same physical memory — `MTLBuffer.contents` is a valid, always-
accessible CPU pointer.  No blit command, staging buffer, or synchronisation fence is
needed to read back vertex data.

### Implementation

```objc
const uint8_t* raw = static_cast<const uint8_t*>(bs->vertexBuffer.contents);
```

The interleaved layout (built by `buildInterleavedVB` in `shader-metal.mm`) is:

| FVF | Stride | Layout |
|---|---|---|
| `FVF_POS` | 12 bytes | `float3 pos` |
| `FVF_POS_UV` | 20 bytes | `float3 pos, float2 uv` |
| `FVF_POS_NOR` | 24 bytes | `float3 pos, float3 normal` |
| `FVF_POS_NOR_UV` | 32 bytes | `float3 pos, float3 normal, float2 uv` |

`fillInSubsetDebug` walks the buffer with the correct stride and writes separate
`pBuffer->position`, `pBuffer->normal`, and `pBuffer->uv` arrays that the debug system
expects.

### Special cases handled

| Case | How handled |
|---|---|
| Index Buffer meshes (3D models) | Copies indices from `bs->indexBuffer.contents`; scans each subset to find `maxIndex`, derives `vertexCount` |
| Vertex Buffer meshes (2D quads, lines) | Reads from `bs->vertexBuffer.contents` directly |
| Dynamic shapes (fonts, skinned meshes) | Uses CPU-side `infoShape->dynamicVertex` / `dynamicUV` arrays — no GPU readback needed |
| Font letter offsets | Applies per-frame `letterDiffX` / `letterDiffY` offsets after copying position data |

### File changed

| File | Change |
|---|---|
| `src/core_mbm/mesh-manager-metal.mm` | Full implementation replacing the `return false` stub |

---

# Appendix B — Lua / editor layer notes

This appendix documents issues in the Lua editor scripts and the `mbm.*` Lua API.  These
are not backend implementation concerns — they apply once the C++ render backend is solid
and Lua integration is enabled.

---

## B1. Render-to-texture UV flip in Lua editors (top-origin vs bottom-origin backends)

### Problem

In the editor Lua scripts, the `render2texture` result is displayed by a `shape` quad whose
UVs were only flipped for `USE_DIRECTX9`.  Metal and Vulkan store render-target row 0 at
the **top** (same convention as DirectX9) — not at the bottom like OpenGL ES — so the
packed texture / animation preview appeared **upside-down** on macOS.

### Root cause

`RENDER_2_TEXTURE::fillvertexQuad()` already handles the flip correctly for the C++ side
(see the `#elif defined(USE_DIRECTX9) || defined(USE_METAL)` block in `render-2-texture.cpp`).
However the Lua-side quads that _display_ the render-to-texture result had a separate guard
that only checked `USE_DIRECTX9`.

### Fix pattern

Every place in an editor script that builds quad UVs (or passes `bFlipV`) based on
`mbm.get('USE_DIRECTX9')` must also include `mbm.get('USE_METAL')`:

```lua
-- BEFORE (broken on Metal)
if mbm.get('USE_DIRECTX9') then
    tUv = {0,1, 0,0, 1,1, 1,0}
else
    tUv = {0,0, 0,1, 1,0, 1,1}
end

-- AFTER (correct for Metal + DirectX9)
if mbm.get('USE_DIRECTX9') or mbm.get('USE_METAL') then
    tUv = {0,1, 0,0, 1,1, 1,0}
else
    tUv = {0,0, 0,1, 1,0, 1,1}
end
```

The same applies to boolean `bFlipV` flags passed to `tImGui.Image(...)`.

### Files changed

| File | Location | Change |
|---|---|---|
| `editor/texture_packer.lua` | `adjustTextureSize()` — quad UV for display shape | Added `or mbm.get('USE_METAL')` |
| `editor/sprite_maker.lua` | `getOrCreateShapeForAnimImage()` — animation preview quad UV | Added `or mbm.get('USE_METAL')` |
| `editor/sprite_maker.lua` | `addDynamicTextureToImGuiImage()` — `bFlipV` for ImGui image | Added `or mbm.get('USE_METAL')` |

### Rule for future editors

When a Lua editor script renders a `render2texture` result via a `shape:createIndexed` quad
or `tImGui.Image`, the UV/flip condition must be:

```lua
if mbm.get('USE_DIRECTX9') or mbm.get('USE_METAL') then
    -- V=0 at top: DirectX9, Metal, Vulkan (VK_KHR_maintenance1 default)
else
    -- V=0 at bottom: OpenGL ES
end
```

> If you add a new top-origin backend (e.g. Vulkan), add its `mbm.get('USE_VULKAN')`
> check to every such condition in the editor scripts.

---

## B2. Particle editor shader — use `mbm.getParticleShaderCode()` instead of hardcoded GLSL

### Problem

`particle_editor.lua::addShaderParticle()` and the shader preview in `showParticleOptions()` previously hardcoded 30 lines of OpenGL ES GLSL:

```lua
code = [[
precision mediump float;
uniform vec4 color;
...
gl_FragColor = outColor;
]]
```

This raw GLSL is passed to `mbm.addShader()` which stores it verbatim and later feeds it to `compileShader()`. On Metal, `compileShader()` calls `[MTLDevice newLibraryWithSource:]` expecting MSL — the GLSL causes a compile error and the particle effect renders with no shader.

### Fix

A new C++ binding `mbm.getParticleShaderCode()` was added to `src/lua-wrap/framework-lua.cpp`. It calls `getParticlePSCode()` — the same platform-specific function already used by `PARTICLE::loadParticleShader()` — and returns the complete fragment shader template for the active backend.

The template uses two placeholders (identical to the C++ substitution logic in `particle.cpp`):

| Placeholder | Meaning |
|---|---|
| `?` | Arithmetic operator (`+`, `-`, `/`, `*`) |
| `#` | Injection point for optional extra GLSL/MSL code (remove by replacing with `''`) |

#### C++ binding (`src/lua-wrap/framework-lua.cpp`)

```cpp
#include <core_mbm/shader-resource.h>   // getParticlePSCode()

int onGetParticleShaderCode(lua_State *lua)
{
    const char *code = getParticlePSCode();
    if (code)
        lua_pushstring(lua, code);
    else
        lua_pushnil(lua);
    return 1;
}
// Registered in luaL_Reg table as: {"getParticleShaderCode", onGetParticleShaderCode}
```

#### `addShaderParticle()` pattern (`editor/particle_editor.lua`)

```lua
function addShaderParticle()
    local baseTemplate = mbm.getParticleShaderCode()
    for i = 1, 4 do
        local code = baseTemplate:gsub('%?', tShaderByOperator[i].op, 1):gsub('#', '', 1)
        local tShaderParticle = { name = tShaderByOperator[i].name, code = code, ... }
        mbm.addShader(tShaderParticle)
    end
end
```

**Important Lua `gsub` notes:**
- Use `'%?'` (not `'?'`) — `?` is a magic pattern character in Lua and must be escaped with `%`.
- When substituting user-supplied content that may contain `%` characters (e.g. GLSL comments like `// 50% blend`), use a function to avoid Lua interpreting `%1`, `%2` etc. in the replacement string:
  ```lua
  :gsub('#', function() return sAddCode end, 1)
  ```

#### Additional code text box — hidden on Metal

The `InputTextMultiline` that lets users inject custom GLSL is meaningless on Metal (the engine ignores it; shader compilation uses MSL). It is hidden with:

```lua
if not mbm.get('USE_METAL') then
    -- show InputTextMultiline
else
    tImGui.TextDisabled("(Custom shader code not available on Metal)")
end
```

#### Rule for future editors

Never hardcode GLSL in Lua editor scripts. For particle shaders always call `mbm.getParticleShaderCode()` and substitute the operator placeholder `?` before passing the result to `mbm.addShader()`.

---

## B3. `mbm.is()` / `mbm.get()` — adding a new platform string

When you add a new backend, also add its platform name to `onIs` and `onGet` in
`src/lua-wrap/framework-lua.cpp` so that `mbm.is('yourplatform')` works in Lua.

### Bug history — `macos` was missing, `linux` returned `true` on macOS

The original `linux` branch used:

```cpp
#if defined __linux__ || defined(__APPLE__) && !defined(ANDROID)
```

This made `mbm.is('linux')` return `true` on macOS and `mbm.is('macos')` always
return `false`.  Fixed during the Metal port.

### Pattern — `onIs` branch for a new platform

```cpp
// linux: strictly __linux__ without Apple
else if (strcasecmp(what, "linux") == 0)
{
#if defined __linux__ && !defined(__APPLE__)
    lua_pushboolean(lua, 1);
#else
    lua_pushboolean(lua, 0);
#endif
}
// NEW: macos
else if (strcasecmp(what, "macos") == 0)
{
#if defined(__APPLE__) && !defined(ANDROID)
    lua_pushboolean(lua, 1);
#else
    lua_pushboolean(lua, 0);
#endif
}
```

#### `onGet` platform block — add `macos`, fix per-platform matching

```cpp
if (strcasecmp(what, "windows") == 0 || strcasecmp(what, "android") == 0 ||
    strcasecmp(what, "linux") == 0   || strcasecmp(what, "macos") == 0)
{
#if defined _WIN32
    lua_pushboolean(lua, strcasecmp(what, "windows") == 0 ? 1 : 0);
#elif defined ANDROID
    lua_pushboolean(lua, strcasecmp(what, "android") == 0 ? 1 : 0);
#elif defined __linux__ && !defined(__APPLE__)
    lua_pushboolean(lua, strcasecmp(what, "linux") == 0 ? 1 : 0);
#elif defined(__APPLE__)
    lua_pushboolean(lua, strcasecmp(what, "macos") == 0 ? 1 : 0);
#else
    lua_pushboolean(lua, 0);
#endif
}
```

### Known platform strings (case-insensitive)

| String | Macro | Notes |
|---|---|---|
| `windows` | `_WIN32` | |
| `android` | `ANDROID` | |
| `linux` | `__linux__ && !__APPLE__` | |
| `macos` | `__APPLE__ && !ANDROID && !MBM_PLATFORM_IOS` | **Added Metal port session** |
| `ios` | `MBM_PLATFORM_IOS` | **Added iOS port session** |

### Editors updated

All 8 editors (`particle_editor`, `texture_packer`, `sprite_maker`, `shader_editor`, `asset_packager`, `physic_editor`, `mesh_debug`, `scene_editor2d`) had their About menu browser-open blocks updated to include `elseif mbm.is('macos') then os.execute('open "URL"')` in the same session.

---

# Appendix C — iOS / Metal implementation notes

This appendix documents the iOS port of the engine (Metal backend, UIKit app model).

---

## C1. Architecture overview

iOS uses the same Metal render backend as macOS, but replaces the Cocoa/NSWindow
application model with UIKit/UIView:

| Concern | macOS | iOS |
|---|---|---|
| Window | `NSWindow` | `UIWindow` + `UIView` |
| Event loop | `NSApplication` run loop + `handleEventFromWindow` | `UIApplicationMain` + `CADisplayLink` |
| Metal surface | `CAMetalLayer` in `NSView` | `CAMetalLayer` as backing layer of `MBMMetalView` |
| Frame tick | `LUA_MANAGER::run()` blocking loop | `CADisplayLink` → `onLoop(true, true)` per frame |
| File dialog | `dialog-util-macos.mm` (tinyfiledialogs) | `dialog-util-ios.mm` (stub, returns `nullptr`) |
| Launcher dialog | `mini-mbm-lib-MacOs.mm` (Cocoa panel) | `mini-mbm-lib-iOS.mm` (stub, returns `false`) |

### Key difference: non-blocking `run()`

On iOS, `UIApplicationMain` owns the main thread and cannot be blocked.  `LUA_MANAGER::run()`
is guarded in `manager-lua.cpp`:

```cpp
#if (defined _WIN32 || defined __linux__ || defined __APPLE__) && !defined ANDROID && !defined MBM_PLATFORM_IOS
    onLoop(false, true);   // blocking desktop loop
#endif
```

After `run()` returns immediately, `MetalViewController` starts a `CADisplayLink` that
calls `onLoop(true, true)` once per display refresh (60 or 120 Hz).

---

## C2. CAMetalLayer handoff

`DEVICE::initializeSpecificContext()` constructs a fresh `SPECIFIC_AUX_CONTEXT_DEVICE`,
which clears any previously stored layer pointer.  To survive this reset, the layer pointer
is stored in a file-scoped static before `initializeSceneLua()` is called and read back
inside `initGraphics()`:

```objc
// core-manager-metal-ios.mm
static CAMetalLayer* s_pendingMetalLayer = nil;

extern "C" void mbm_ios_setMetalLayer(CAMetalLayer* layer) {
    s_pendingMetalLayer = layer;
}

void CORE_MANAGER::initGraphics() {
    // initializeSpecificContext() was called inside initializeSceneLua()
    auto* ctx = this->device->getSpecificContextDevice();
    ctx->metalLayer = s_pendingMetalLayer;   // read back after context reset
    ...
}
```

`MetalViewController` calls `mbm_ios_setMetalLayer(_metalView.metalLayer)` before
`initializeSceneLua()`.

---

## C3. `specific-metal.h` conditional fields

`NSWindow` does not exist on iOS.  The struct uses `TARGET_OS_IOS` to switch:

```objc
#include <TargetConditionals.h>
#if TARGET_OS_IOS
    #import <UIKit/UIKit.h>    // UIView
#else
    #import <Cocoa/Cocoa.h>    // NSWindow
#endif

// Inside SPECIFIC_AUX_CONTEXT_DEVICE:
#if TARGET_OS_IOS
    UIView*   metalView      = nil;
#else
    NSWindow* window         = nil;
    id        windowDelegate = nil;
#endif
```

---

## C4. Touch input

Touch events arrive via `UIResponder` overrides on `MetalViewController`.  Each
`UITouch*` pointer is mapped to a stable integer finger index via a static map:

```cpp
static std::map<UITouch*, int> s_touchMap;
static int s_nextTouchID = 0;
```

`touchesBegan` assigns a new ID; `touchesEnded/Cancelled` removes the entry.  The (x, y)
coordinates are taken from `[touch locationInView:self.view]` in **points** (logical),
then multiplied by `self.view.contentScaleFactor` to get physical pixels matching the
engine's backbuffer dimensions.

The engine receives `onTouchDown(id, x, y)` / `onTouchMove(id, x, y)` / `onTouchUp(id, x, y)`.

---

## C5. Build prerequisites

- Xcode 15 or later with the iOS SDK
- Apple Developer account (free tier allows device testing via USB)
- `cmake` 3.21+; Ninja or Xcode generator

```bash
# Generate an Xcode project targeting iOS (arm64 device)
cmake -B build/ios \
      -DPLAT=iOS \
      -DUSE_ALL=1 \
      -DMBM_ENABLE_MESH_LEGACY_V7=1 \
      -DCMAKE_BUILD_TYPE=Debug \
      -G Xcode

# Build from command line (requires code-signing environment variables or
# DEVELOPMENT_TEAM set in CMakeLists.txt)
xcodebuild -project build/ios/mini-mbm.xcodeproj \
           -scheme mini-mbm \
           -destination "generic/platform=iOS" \
           -configuration Debug \
           build
```

> **Tip:** Open `build/ios/mini-mbm.xcodeproj` in Xcode and set the Team + Bundle
> Identifier under *Signing & Capabilities* before the first build.

---

## C6. Bundle layout

Lua scripts and assets must be placed inside the app bundle so they are accessible via
`[[NSBundle mainBundle] resourcePath]`.  `MetalViewController` passes
`--addPath <resourcePath>` and `--scene main.lua` to `LUA_MANAGER` so the engine finds
the entry script at `Resources/main.lua`.

CMake copies files listed under `RESOURCE` properties of the target into the `.app`
bundle automatically when building with the Xcode generator.

---

## C7. Plugin notes

All plugins are **statically linked** on iOS (App Store requirement — no dynamic loading).
Each plugin is added via `add_definitions(-DUSE_<PLUGIN>)` in
`src/CMakeLists.txt` so `require_embedded.cpp` maps `require("box2d")` (etc.) to the
statically-linked init functions.

The same pattern is used for Android; see the `BUILD_ANDROID` block in
`src/CMakeLists.txt` for reference.

---

## C8. Orientation

`MetalViewController` returns `UIInterfaceOrientationMaskLandscape` from
`supportedInterfaceOrientations`, matching the `UISupportedInterfaceOrientations` keys in
`Info.plist` (LandscapeLeft + LandscapeRight).  To add portrait support remove both
restrictions.

---

## A4. Tile-map alpha transparency — depth comparison must be `LessEqual`

### Problem

When a tile map layer contains bricks that overlap (e.g. a wide/tall brick that extends
into an adjacent cell), transparent (alpha < 1) pixels from the first brick are written to
the depth buffer.  The second brick occupies exactly the same Z value (same layer, same
orthographic depth).  With a strict `Less` depth test the second brick's fragments at those
overlapping pixels **fail** (equal is not less) and are discarded entirely.  The result is
a white or clear rectangle wherever two semi-transparent bricks overlap — only visible on
Metal/iOS/macOS because all OpenGL ES platforms (Linux, Windows, Android) already use
`GL_LEQUAL`.

### Root cause

`getOrCreateDepthStencilState()` in `src/core_mbm/shader-metal.mm` created the
`MTLDepthStencilState` with `MTLCompareFunctionLess` instead of
`MTLCompareFunctionLessEqual`.

This affects **both macOS and iOS** — they share the same `shader-metal.mm`.

### Fix

In `src/core_mbm/shader-metal.mm`, `getOrCreateDepthStencilState()`:

```objc
// Before (incorrect — breaks overlapping same-Z tile bricks with alpha)
dsd.depthCompareFunction = MTLCompareFunctionLess;

// After (correct — matches GL_LEQUAL on all OpenGL ES platforms)
dsd.depthCompareFunction = MTLCompareFunctionLessEqual;
```

### Rule for new backends

Always use **less-or-equal** (LEQUAL / `<=`) as the default depth comparison function.
Tile map layers render multiple bricks at the exact same Z; a strict `Less` function
discards any brick that shares depth with a previously rendered brick in the same
pass.
