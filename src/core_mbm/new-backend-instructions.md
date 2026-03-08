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
2. Translate pointer/touch coordinates from OS units to **physical pixels** before pushing
   events.  On high-DPI displays (Retina/HiDPI) the OS often supplies logical points;
   multiply by the display scale factor before comparing with `backBufferWidth/Height`.
3. Push events into `CORE_MANAGER::lsEvents` using `EVENT_KEY` structs.
4. Call `CORE_MANAGER::loop(singleLoop, doSwapBuffers)` to drive the game loop.

Mouse/touch Y origin: the engine origin is **bottom-left** (Y increases upward), matching
OpenGL convention.  Most desktop OSes put Y=0 at the top of the window.  Apply Y-flip:
```cpp
float ey = backBufferHeight - (os_y * scale);
```

---

## 14. Milestone checklist

Implement features in this order to reach a testable state as early as possible:

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
- [x] **M7 — Render-to-texture**: `createTextureRenderTarget`, `renderToTargets`.
- [ ] **M8 — Custom shaders**: `BASE_SHADER::addVar`, `BASE_SHADER::update`,
      `VAR_SHADER` constructor with backend handle.
- [ ] **M9 — Fluid particles**: `renderParticle(FLUID_GROUP*)`.
- [ ] **M10 — Utilities**: `saveAsPNG`, pixel-perfect filtering, HMD support.

---

## 15. Key source files for reference

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
