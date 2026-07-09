# Default Shader Program Cache (Windows-only 400-mesh fill freeze)

Status: **implemented** (see `MBM_VERSION` 6.26.0 in `include/version/version.h` for the shipped
entry). Written up separately here because the diagnostic path (several disproven hypotheses before
the real cause) and the per-backend ownership design are worth preserving for whoever touches
`SHADER::compileShader` next.

## The bug report

Scene Editor 3D's "fill layer with selected mesh" (`editor/scene_editor3d.lua`,
`fillActiveLayerWithMesh`) places one already-loaded mesh into every cell of the active layer's grid
-- 400 cells for the default 20x20 -- synchronously, in a single Lua call triggered by one right-click
menu action. On Windows this froze the editor for ~20-30 seconds. On Linux and macOS the identical
operation, same scene, same mesh, took under half a second. Windows-only, reproducible, large enough
to matter.

## Hypotheses tried and disproven, in order

Worth recording explicitly, since each of these looked plausible and each turned out wrong -- a
future session hitting a similar "Windows only" report should not have to re-walk this path blind.

1. **Repeated disk I/O per placement.** Disproven by reading the code, not guessing:
   `MESH_MANAGER::load()` (`mesh-manager.cpp`) is a `std::unordered_map<std::string, MESH_MBM*>`
   cache keyed by base filename -- the 2nd through 400th placement of the same file is a pure hash
   lookup, zero disk access, on every platform.
2. **`mbm.setLightEnabled` cost.** Disproven: `device-common.cpp`'s implementation just flips a bool
   on `LIGHT_STATE`, O(1), documented as such in `docs/light.md`.
3. **MSVC Debug-CRT / unoptimized build.** The most tempting hypothesis, and the one first pursued:
   the MSVS solution's `Debug|Win32` configuration uses `/MDd` (debug CRT + checked STL iterators,
   `_ITERATOR_DEBUG_LEVEL=2`), `Optimization=Disabled`, *and* (per `platform-msvs/mbm-backend.props`)
   defaults to the DirectX9 backend instead of OpenGL ES -- three plausible slowdowns bundled into
   one config, versus CMake's `Release`-by-default on Linux/macOS. **Disproven by direct user
   measurement**: rebuilding clean in MSVS `Release|Win32` (optimized CRT, OpenGL ES backend) still
   took 21 seconds. Whatever this was, it wasn't the CRT or missing optimizations.
4. **The real cause**, found by tracing the actual call graph from `fillActiveLayerWithMesh` through
   `addPlacedMesh` (`bSync=true` path) -> `placeMeshSync` -> `MESH::load` -> (cache hit, fast) ->
   `ANIMATION_MANAGER::populateAnimationsFromMesh` (`animation.cpp`) -> `populateAnimationFromHeader`,
   which does `new ANIMATION()` **per placed instance** and, inside that, calls
   `fx.shader.compileShader(...)` -- also per instance, unconditionally.

## Root cause

`SHADER::compileShader` (`shader-opengl_es.cpp`, `shader-directx9.cpp`, `shader-metal.mm` --
structurally identical across all three) deterministically regenerates the engine's built-in default
shader source from a small, bounded set of flags:

- `fvf` (`FVF_PROVIDE_BY_ENGINE`: derived from the mesh's vertex layout -- position/normal/UV
  presence)
- `useReservedLightScaffolding` (`SHADER::shouldCompileReservedLightDefault()`, itself derived once
  from the renderizable's type via `getDefaultShaderModeForRenderizable`, not from any live/toggle
  state)
- `canUsePointLight2D` (OpenGL ES only -- `useReservedLightScaffolding && vShader == nullptr`; the
  DirectX9 and Metal default-source generators have no equivalent branch)

...then unconditionally does a real compiler/driver call every time: `glCompileShader` +
`glCreateProgram` + `glLinkProgram` (GL), `D3DXCompileShader` + `CreatePixelShader` +
`CreateVertexShader` (DX9), or `newLibraryWithSource:` + PSO compilation (Metal). The only existing
"already done" guard in any of the three (`if (gles_shaderSpecific->programObject) return true;` and
equivalents) is **per-instance** -- every new placed `MESH` gets a brand-new `ANIMATION`/`FX`/`SHADER`
object with its own fresh backend-specific struct, so the guard never actually skips anything across
the 400 placements of one fill operation. 400 placements of the same mesh -> 400 full compiles of
byte-identical generated source.

## Why this was Windows-only

The C++ logic above is identical on every platform -- the *cost per redundant compile* is what
differs:

- **Linux** (`core-manager-opengl_es-x11.cpp`): native GLSL ES driver compiler (Mesa or vendor),
  sub-millisecond for this trivial generated source. 400 redundant compiles are imperceptible.
- **macOS** (`core-manager-metal-macos.mm`): native Metal, `newLibraryWithSource:` + PSO compile is
  also fast for this size of shader, and doesn't touch the GLES/DirectX9 code paths at all.
- **Windows**: `core-manager-opengl_es-windows.cpp` runs OpenGL ES through **ANGLE**
  (`third-party/gles/EGL/egl.h` + `GLES2/gl2.h`, ANGLE's own headers), which translates GLSL ES ->
  HLSL and invokes the real D3D shader compiler (`d3dcompiler_47.dll`) on every compile+link --
  routinely tens of milliseconds per call, even for a trivial shader. The DirectX9 backend
  (`shader-directx9.cpp`, active on `Debug|Win32` per `mbm-backend.props`, and confirmed by the user
  to reproduce the same stall when manually selected) calls `D3DXCompileShader` directly, same order
  of cost. 400 x ~50ms lands almost exactly on the ~20s measured in both configurations.

## Fix: a process-lifetime cache of compiled default programs

Scoped narrowly and deliberately:

- **Only the pure-default-shader-pair path** (`SHADER::usesPureDefaultShaderPair()`, i.e. no custom
  `.cfg` effect shader loaded via `pShader`/`vShader`) is cached. The custom-effect path's source
  determinism per (mesh, animation index) was never verified during this investigation, and it's not
  the path `fillActiveLayerWithMesh` exercises -- left untouched in all three backends.
- **Cache key** is the flag tuple above -- `(fvf, useReservedLightScaffolding[, canUsePointLight2D])`
  -- packed into a small int; `FVF_PROVIDE_BY_ENGINE` has 5 values, so the whole keyspace is at most
  20 entries. Entries are populated lazily and never evicted during normal operation, mirroring the
  existing lifetime philosophy of `MESH_MANAGER::Impl::lsMeshes` / `TEXTURE_MANAGER::Impl::lsTextures`
  (both are the same shape of "string/int-keyed manager cache, alive until teardown" already
  established in this codebase).

### Ownership differs per backend, and that's intentional, not an inconsistency

- **OpenGL ES** (`shader-opengl_es.cpp`): a raw `GLuint programObject` inside `GLES_PS_VS` has no
  reference count of its own. `GLES_PS_VS` gained one new field, `bool isSharedProgram`, checked
  inside `GLES_PS_VS::release()` (the single choke point already called from the destructor,
  `SHADER::onRestore()`, and `SHADER::releaseShader()`) to skip `GLDeleteProgram` when true. Every
  instance that ends up pointing at a cached program -- **including the very first one that compiled
  it** -- gets `isSharedProgram = true`, because ownership conceptually transfers to the cache the
  moment an entry exists; the compiling instance is not special.
- **DirectX9** (`shader-directx9.cpp`) and **Metal** (`shader-metal.mm`): their backend-specific
  objects are already reference-counted by their respective platforms (D3D9 shader/constant-table
  objects are COM/`IUnknown`; Metal's `MBMPSOPair` is a toll-free-bridged Objective-C object managed
  via `__bridge_retained`/`CFRelease`, per the existing `SHADER::~SHADER()`). No new field was needed
  in either backend's per-instance struct: the cache simply takes one extra `AddRef()` /
  `__bridge_retained` reference of its own right after a successful compile, and every instance that
  later shares that entry (again, including the original compiler) takes its own independent share
  the same way. Each instance's existing, **completely unmodified** `Release()`/`CFRelease()`
  teardown path stays correct regardless of destruction order, because the cache's own reference is
  what keeps the object alive once all sharing instances are gone.

### Invalidation on GL/device context loss

`CORE_MANAGER::onLostDevice()` (`core-manager-common.cpp`) tears down and recreates the entire
graphics context via `ReleaseGraphics()` + `initGraphics()`. Every previously compiled program id
becomes invalid the moment that happens, and a freshly created context is free to reuse the same
numeric/opaque id for a *completely different* program -- a stale cache entry would silently corrupt
rendering rather than just fail loudly. Every existing `RENDERIZABLE` already re-runs
`onRestoreDevice()` -> eventually `SHADER::onRestore()` for its own per-instance state after a
restore; that self-healing does **not** reach the new cross-instance cache, so a new
`SHADER::clearDefaultProgramCache()` static method was added (`include/core_mbm/shader.h`), with a
real per-backend implementation in all three backends (clears the map; for DX9/Metal, also
releases/`CFRelease`s each cached reference first, since those *do* own a real reference; the GL
version just clears the map, since the context tearing down already invalidated the raw ids and
there's nothing meaningful left to "release"). `shader-dummy.cpp` keeps a no-op stub. Called once
from `onLostDevice`, immediately after `ReleaseGraphics()` and before the per-object restore loop
runs.

## Files touched

- `src/core_mbm/shader-opengl_es.cpp` -- cache, `compileShader` fast path/store, `release()` guard,
  real `clearDefaultProgramCache()`.
- `src/core_mbm/private/specific-opengl_es-shader.h` -- `GLES_PS_VS::isSharedProgram`.
- `src/core_mbm/shader-directx9.cpp` -- cache, `compileShader` fast path/store (AddRef-based), real
  `clearDefaultProgramCache()`.
- `src/core_mbm/shader-metal.mm` -- cache, `compileShader` fast path/store
  (`__bridge_retained`-based), real `clearDefaultProgramCache()`.
- `src/core_mbm/shader-dummy.cpp` -- no-op `clearDefaultProgramCache()` stub, for build parity only.
- `include/core_mbm/shader.h` -- `SHADER::clearDefaultProgramCache()` declaration.
- `src/core_mbm/core-manager-common.cpp` -- one call site in `onLostDevice`.

## Explicitly out of scope

- **Custom `.cfg` effect-shader path** (`pShader`/`vShader` non-null) in all three backends -- source
  determinism per (mesh, animation index) was never verified; left uncached.
- Nothing else was left out backend-wise this time -- OpenGL ES, DirectX9, and Metal all received the
  cache (Metal per explicit request, even though no macOS complaint exists yet, for full parity across
  the three backends this engine ships).

## Verification

- **Windows, OpenGL ES backend** (user-verified): fill-layer on the default 20x20 grid dropped from
  ~21s to under 0.5s.
- **Windows, DirectX9 backend** (`Debug|Win32`, user-verified the *bug* independently by manually
  switching backends before this fix landed there): same ~20s+ stall reproduced, confirming the same
  root cause applied; fixed by the same change, not yet re-timed by the user as of this writing.
- **Metal**: not run (no macOS environment in this session) -- ported by code-reading/mirroring the
  DirectX9 refcounting approach (Metal's `MBMPSOPair` retain/release semantics are structurally the
  same as D3D9 COM `AddRef`/`Release`), self-reviewed line-by-line, not compiled or executed.
- **Linux**: not run in this session either (no compiler available); self-reviewed only. See
  `sleepy-cooking-brooks.md` (this session's plan file) for the originally-intended Linux verification
  checklist (fill-layer correctness, a second distinct mesh/FVF combination hitting a different cache
  key, removing the first-placed instance without breaking the others, and a device-lost/restore
  cycle if one is reachable interactively).
