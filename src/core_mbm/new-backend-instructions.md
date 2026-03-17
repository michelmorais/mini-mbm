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
cmake .. -DPLAT=Apple -DAUDIO=none
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
| Audio | `third-party/audiere-*`, `third-party/portaudio/` | **Yes** — pass `-DAUDIO=none` |
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

It is stored as `DEVICE::specificContextDevice` (forward-declared in `device.h`; the concrete
type lives in your `specific-<backend>.h`).

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
MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective);
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
- Use `less` depth comparison with depth writes enabled for normal 3D objects.
- Depth range is **[0, 1]** (see §5).

The engine also calls `DEVICE::setDephtTest(bool)` and `DEVICE::clearDepth()` — implement
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

`TEXTURE` uses a union field for the GPU handle:
```cpp
union { uint32_t idTexture; void* ptrTexture; };
```

- OpenGL ES uses `idTexture` (GLuint texture object).
- Metal/Vulkan/D3D12 use `ptrTexture` (store a retained pointer / descriptor, cast via
  `(__bridge_retained void*)` on Metal, via raw pointer on others).

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
2. `CORE_MANAGER::renderToTargets()` — iterate `device->lsObjectRenderToTarget`,
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
   events.  `backBufferWidth/Height` is always in **logical points** — the coordinate
   space the game scene uses.  The rendering-surface size (swapchain extent,
   `drawableSize`, EGL surface, etc.) uses *physical* pixels (`logical × scale`), but
   that is a separate value the game loop never reads directly.  Do **not** multiply
   input coordinates by the scale factor before comparing with `backBufferWidth/Height`.
3. Push events into `CORE_MANAGER::lsEvents` using `EVENT_KEY` structs.
4. Call `CORE_MANAGER::loop(singleLoop, doSwapBuffers)` to drive the game loop.

Mouse/touch Y origin: the engine origin is **bottom-left** (Y increases upward), matching
OpenGL convention.  Most desktop OSes put Y=0 at the top of the window.  Apply Y-flip:
```cpp
float ey = backBufferHeight - os_y;  // both in logical points — no scale factor
```

> **CRITICAL — `backBufferWidth/Height` = logical points, rendering surface = physical pixels**  
> Storing physical pixels (e.g. `width * retinaScale`) in `backBufferWidth/Height` makes
> the orthographic camera span physical pixels, so every 2-D object appears at *half*
> the expected size on a 2× Retina / HiDPI display compared with a non-Retina machine.  
> Rule of thumb:
> - `backBufferWidth/Height` — logical points (what the programmer thinks of as screen size)
> - `drawableSize` / swapchain extent — physical pixels = logical × `contentsScale` / DPI scale  
> - Input event coordinates — logical points (no multiplication needed on macOS/Windows)

> **CRITICAL — read the actual window size *after* the OS has shown and constrained the window**  
> On every desktop OS, if you request a window larger than the available screen area the OS
> silently shrinks it.  That shrink happens during or after `makeKeyAndOrderFront` /
> `ShowWindow` / `XMapWindow`, **not** when you create the window object.  Reading
> `contentView.bounds` (macOS) or the equivalent before the window is visible gives the
> *requested* size, not the *actual* size.  This creates a mismatch: `expectedScreen` is
> set once on the first `loop()` tick from `backBufferWidth/Height`, and if those values
> are wrong (e.g. 1 600 instead of 1 470) then `adjustScaleScreen2d()` computes a scale
> ≠ 1 every frame, offsetting every `is2dS` object.  
> Fix: pump the run-loop for ≥ 50 ms after showing the window, then read the actual
> bounds and assign both `backBufferWidth/Height` **and** the native surface size from them.

> **CRITICAL — `setProjectionMode` must only rebuild camera matrices**  
> `setProjectionMode` is called every frame from inside the render loop.  It must contain
> **only** a call to `updateCam` (or equivalent matrix rebuild).  Never reassign
> `backBufferWidth/Height` or the native surface size (e.g. `drawableSize`) from inside
> this function.  Doing so overwrites the values set by `initGraphics()` and
> `resetDeviceWithNewDimensions()` every frame, corrupting the coordinate system
> (sub-pixel gaps, wrong scale, lost Retina resolution).  Those values belong exclusively
> to the init path and the resize handler.

---

## 14. Milestone checklist

Implement features in this order to reach a testable state as early as possible:

- [x] **M1 — Window + clear screen**: `initGraphics`, `beginRender`, `endRender`,
      `swapBuffers`, background color.  Run testLib; a coloured window should appear.
- [x] **M2 — Textures**: `TEXTURE::loadFromData`, `TEXTURE::loadFromResourceData`,
      `TEXTURE::release`.  PNG images should decode and display.
- [x] **M3 — Shaders + static buffers**: `compileShader`, `loadBuffer(VB)`,
      `loadBuffer(IB)`, `render`.  3D meshes and 2D quads should draw correctly.
- [x] **M4 — Culling + depth**: apply `mode_cull_face` + `mode_front_face_direction` per
      draw call; attach depth buffer to render pass.  Meshes should stop showing inner faces.
- [x] **M5 — Dynamic buffers**: `loadBufferDynamic`, `updateDynamic`.
      Skinned meshes, line meshes, and text rendering require this.
- [x] **M6 — Particles**: `loadParticleBuffer`, `renderParticle(PARTICLE_CONTROL*)`.
- [x] **M7 — Render-to-texture**: `createTextureRenderTarget`, `renderToTargets`.
- [x] **M8 — Custom shaders**: `BASE_SHADER::addVar`, `BASE_SHADER::update`,
      `VAR_SHADER` constructor with backend handle.
  - ✅ `addVar`, `update`, `VAR_SHADER` constructor fully implemented for Metal.
  - ✅ Particles (`renderParticle`), steered particles (`FLUID_GROUP`), and
        dual-PSO blend modes (standard + additive) working.
  - ✅ Combined VS+PS compilation (`scale.vs` + `blend.ps`) working — the VS
        default `frag_main` is stripped and the PS `frag_main` appended.
  - ✅ **FVF attribute-index fix** (`patchVInStruct`): prewritten VS programs
        (`scale.vs`, `simple texture.vs`) hardcoded `uv [[attribute(1)]]`.
        For `FVF_POS_NOR_UV` meshes the Metal interleaved vertex descriptor
        places the normal at `[[attribute(1)]]` and UV at `[[attribute(2)]]`;
        the hardcoded index caused the vertex shader to read normal data as UV
        coordinates, sampling garbage texels.  `compileShader` now calls
        `patchVInStruct(vsStr, fvf)` to replace the `struct VIn` block with the
        FVF-correct attribute indices before pipeline compilation.
        *Note: OpenGL ES is unaffected because it binds each stream to a separate
        VBO and looks up `aTextCoord` by name, not by attribute index.*
  - ⚠️ **Known issue — `blend.ps + scale.vs` invisible when scale > 0.5**:
        observed on **both** Linux/OpenGL ES and macOS/Metal.  When all three
        `scale` components exceed ≈ 0.5 the rendered sprite becomes invisible;
        below 0.5 it is visible.  The same vertex shader combined with other
        pixel shaders (e.g. `bands.ps + scale.vs`) renders correctly even at
        very high scale values (verified at 6.67).  Root cause is under
        investigation; the most likely explanation is that `blend.ps` samples
        `sample1` (its second texture) which is not bound for the test sprite
        (`box.spt` has only one texture), causing the blend formula to produce
        degenerate or fully-transparent output under certain GPU/driver
        implementations.  Not a blocker for M8 completion.
- [x] **M9 — Fluid particles**: `renderParticle(FLUID_GROUP*)`.
- [x] **M10 — Utilities**: `saveAsPNG`, pixel-perfect filtering, HMD support.
  - ✅ `saveAsPNG`: implemented via `MTLBlitCommandEncoder` staging blit in
        `render-2-texture-metal.mm`.  Triggered by right mouse button in the test
        scene (`onTouchDown key==1`) when a `RENDER_2_TEXTURE` object is active.
  - ✅ **Pixel-perfect filtering**: implemented.  `SPECIFIC_AUX_CONTEXT_DEVICE`
        now holds two `MTLSamplerState` objects — `defaultSampler` (bilinear +
        clamp-to-edge for normal rendering) and `nearestSampler` (point filter +
        repeat, for tile-map rendering).  `disableFilteringForPixelPerfect()` sets
        `useNearestSampler = true` on the context; `enableFilteringAfterPixelPerfect()`
        clears it.  `getOrCreateSampler()` in `shader-metal.mm` lazily creates both
        samplers and returns the active one.  All `render()` / `renderDynamic()` /
        `renderParticle()` call sites use `getOrCreateSampler()`, so the switch is
        automatic with no per-draw-call overhead.  Fixes the black gap lines that
        appeared between tile-map tiles when bilinear filtering sampled across tile
        boundaries.
  - ✅ **HMD**: `HMD.cpp` is platform-agnostic and compiles for Metal without
        modification.  It is built on top of `RENDER_2_TEXTURE` (M7, already
        implemented), so no Metal-specific stubs are needed.  The class has not been
        exercised in the Metal test scene yet (it is driven via Lua in practice).

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
| `src/core_mbm/device-metal.mm` | Metal | `setProjectionMode`, `setDephtTest`, `clearDepth` |
| `src/core_mbm/shader-opengl_es.cpp` | OpenGL ES | Reference for culling, winding, uniform upload |
| `src/core_mbm/shader-directx9.cpp` | D3D9 | Reference for dynamic buffer lock/unlock pattern |
| `src/core_mbm/primitives.cpp` | All | `MatrixPerspectiveFovLH`, `MatrixLookAtLH`, `MatrixOrthoLH` |
| `include/core_mbm/draw-compatibility.h` | All | `CULL_MODE`, `FACE_DIRECTION`, `MODE_DRAW` enum values |
| `include/core_mbm/shader.h` | All | `BUFFER_GL`, `BASE_SHADER`, `SHADER`, `FVF_PROVIDE_BY_ENGINE` |
| `include/core_mbm/specific-metal.h` | Metal | Example `SPECIFIC_AUX_CONTEXT_DEVICE` struct layout |
