# Mini MBM Mesh v11 Plan

This document is the scoping/design companion for the deliberate mesh-format compatibility break
discussed alongside `docs/light-plan.md` (which already covers the related texture-role / shader
naming cleanup, milestone 8.5).

Use:

- `include/core_mbm/header-mesh.h` / `include/core_mbm/mesh-manager.h` for the current on-disk and
  runtime mesh types.
- `docs/light-plan.md` for the texture-role (`mbm::TEXTURE_ROLE`) and shader-naming contract this
  plan must reuse, not duplicate.
- This document for the v11 format break: what's in scope, what's deliberately deferred, and why.

## Goal

Replace the legacy MESH_MBM binary format (versions 1-10, `CURRENT_VERSION_MBM_HEADER ==
TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER`) with a new version 11 designed for the engine as it
exists today, not as it existed in the Android-APK-size-constrained era the original format was
built for. Old assets remain readable, but only through an explicit, offline migration step, not
through a runtime legacy path baked into the shipped engine.

## Findings that motivate the break (kept here for traceability)

- **Whole-file compression is structurally bad, not just slow.** `open_decompressed_mesh_file()`
  (`src/core_mbm/mesh-manager.cpp:110`) decompresses the *entire* file to a single process-global
  temp file path (`util::getDecompressModelFileName()`, `src/core_mbm/file-util.cpp:1023`, backed by
  `CR_DEFINE_STATIC_LOCAL_ARG`) before any parsing happens. This means: a synchronous disk
  round-trip on every load, a shared static filename that is not safe for concurrent loads, full
  DEFLATE cost paid even for metadata-only reads (`MESH_MBM_DEBUG::getInfo`), and double compression
  of texture blobs that are already individually compressed via `miniz` (`HEADER_IMG::lenght`).
- **No PIMPL hygiene on the mesh types.** `MESH_MANAGER` already does this correctly (`struct Impl;
  std::unique_ptr<Impl> impl;`), but `MESH_MBM` / `MESH_MBM_DEBUG` expose on-disk types directly in
  the public `mesh-manager.h` (`util::HEADER_MESH`, `util::INFO_ANIMATION`, `util::INFO_DRAW_MODE`,
  and transitively every `*_DISK_V8/V9/V10` struct via the `header-mesh.h` include).
- **16-bit indices are a real ceiling for dense meshes.** `HEADER_FRAME_DISK_V8::sizeIndexBuffer`
  and `getIndexArray()` are hardwired to `uint16_t`, capping a frame/subset at 65535 vertices.
- **32/64-bit-safe serialization is already solved since v8** — `mesh-v8-io.cpp` reads/writes
  field-by-field through explicit little-endian helpers (`readI32LE`, `writeF32LE`, ...), never
  struct-blits. v11 keeps this pattern as-is; it does not need fixing.
- **Texture-role plumbing already exists at the runtime/shader layer** but not at the file-format
  layer. `mbm::TEXTURE_ROLE` (`include/core_mbm/shader.h:74`) is fully wired through
  `SHADER_TEXTURE_NAMING` profile detection, backend binding (`shader-opengl_es.cpp`,
  `shader-directx9.cpp`, `shader-metal.mm`), and fallback textures
  (`TEXTURE_MANAGER::getFallbackTexture`). The file format still encodes its own, differently-valued
  `util::MATERIAL_TEXTURE_SLOT_TYPE` (`header-mesh.h:225`: `NORMAL=1, SPECULAR=2, EMISSIVE=3,
  MASK=4`), and the built-in shader source strings in `shader-resource-opengl_es.cpp` /
  `shader-resource-directx9.cpp` / `shader-resource-metal.mm` still hardcode `sample0`/`sample1`/
  `sample2` literally. Two parallel enums for the same concept is exactly the kind of drift the
  reserved-name helper pattern in `light-plan.md` was meant to prevent.
- **The `.mbm` extension is already effectively deprecated.** `MESH_MBM_DEBUG::getValidExtension()`
  (`mesh-manager.cpp:1579`) only accepts `.MBM` when `MBM_ENABLE_MESH_LEGACY_V7` is defined, which
  defaults to **off**. Current mesh types already use `.msh` (3D mesh), `.spt` (sprite), `.fnt`
  (font), `.ptl` (particle), `.tile` (tile map). The extension-per-type split is done; what's left to
  break is the *binary layout* those extensions point to, not their names.

## Scope Decisions (resolved 2026-06-25)

### 1. `mesh_deprecated` is an offline-only migration tool, not a runtime-linked lib

A shipped game built against the v11 engine never opens a v1-v10 file. `mesh_deprecated` exists
purely to let a developer recover/convert assets from old projects before rebuilding against the
new engine.

Implications:

- `MESH_MANAGER` in the shipped runtime only knows how to read/write v11. No version-branch, no
  `MBM_ENABLE_MESH_LEGACY_V7`-style flag, inside the hot load path of `core_mbm` going forward.
- `mesh_deprecated` becomes its own CMake target, seeded from the code currently gated by
  `MBM_ENABLE_MESH_LEGACY_V7` plus the existing v8/v9/v10 read paths in `mesh-manager.cpp` and the
  `deprecated_mbm::INFO_SPRITE` types. It is read-only: it parses old formats and re-serializes
  through the *same* v11 writer that Mesh Debug / Sprite Maker use for authoring, so there is one
  writer for v11, not two.
- Packaging: linked only into an offline converter tool / editor "Import legacy mesh" action, never
  into the game runtime target.

### 2. Texture roles get one shared source of truth

`mbm::TEXTURE_ROLE` (`shader.h`) is promoted to the only definition of "what kind of texture is
this." v11's on-disk per-subset texture-slot encoding serializes `mbm::TEXTURE_ROLE` values (or a
stable byte mapping that round-trips 1:1 to them) instead of inventing or keeping a second,
differently-numbered `util::MATERIAL_TEXTURE_SLOT_TYPE`.

This also pulls the built-in shader source cleanup (milestone 8.5 in `light-plan.md`) into the same
delivery window as v11: v11 is what finally lets a subset declare `TextureNormal` / `TextureSpecular`
/ `TextureEmissive` / `TextureMask` as first-class slots instead of the bolted-on v9 slot list, so
converting the built-in shaders to semantic naming by default should ship alongside it rather than
as a separate, later pass.

### 4. `TEXTURE_ROLE` is defined in a new minimal shared header, not in `shader.h` directly

`shader.h` is a heavy include (`particle-control.h`, forward decls of `TEXTURE`/`RENDERIZABLE`,
`<unordered_map>`, ...) — appropriate for runtime/rendering code, wrong for `header-mesh.h`, which is
deliberately lean (`stdint.h`, `<vector>`, `<string>`, `primitives.h`, `core-exports.h`) so that
on-disk format types stay usable by tools (`mesh_deprecated`, a standalone validator) without pulling
in shader/texture/particle machinery.

`mbm::TEXTURE_ROLE` moves out of `shader.h` into a new dependency-free header (e.g.
`include/core_mbm/texture-role.h`); both `shader.h` and `header-mesh.h` include that header. This
keeps Scope Decision 2's "one enum, one definition" intact while keeping `header-mesh.h`'s include
graph unchanged.

### 5. `UBER_IMG` decouples from the legacy mesh v8 header, not kept as a `core_mbm` dependency

Milestone 5's extraction surfaced an unrelated coupling: `mbm::UBER_IMG` (`uber-image.cpp`/`.h`), a
standalone compressed-texture-blob format used by `texture-manager*.cpp` — **not a mesh format at
all** — reads/writes its file header using `util::readHeaderV8`/`writeHeaderV8`/`readHeaderImgV8`/
`writeHeaderImgV8`, the same `util::HEADER`/`HEADER_IMG` structs the legacy v1-v10 mesh format uses
(`header.typeApp == "img uberimg"`, hardcoded `version == 1`). This is the *only* reason those four
functions still live in `core_mbm` after milestone 5's extraction — `saveV11`/`loadV11` never call
them.

Decision: `mesh_deprecated` keeps `readHeaderV8`/`writeHeaderV8`/`readHeaderImgV8`/`writeHeaderImgV8`
(needed to *read* old `.uberimg` files during migration, same as any other legacy asset). Going
forward, `core_mbm`'s `UBER_IMG` gets its own small, self-contained header — not the legacy mesh v8
`HEADER`/`HEADER_IMG` layout — so `core_mbm` ends up with zero v1-v10 format dependencies, not "zero
except this one unrelated texture format." This is **not implemented yet** — tracked as milestone
5.5 below — `core_mbm` currently still carries the four functions for `UBER_IMG`'s sake.

### 3. Async loading ships for real in v11, not just designed for

No thread pool or background-loading primitive exists anywhere in the engine today (confirmed by
search). This is new ground for the codebase, so the design below stays deliberately small and
purpose-built rather than a general job system — that's a separate, much larger conversation if it's
ever needed beyond mesh loading.

## File Format Design Principles

- **Sectioned / TLV layout.** Precedent already exists (`EXTRA_HEADER` since v6,
  `MATERIAL_TEXTURE_SLOT_HEADER_DISK_V9` with `payloadSizeInBytes` letting unknown slots be skipped).
  v11 generalizes this: every block beyond a minimal fixed header is `{type, length, payload}`.
  Benefits: future block types are always skippable without another version bump; a skeleton/bone
  frame type can be added later as a new block type instead of a v12; tools can read only the blocks
  they need (e.g. editor metadata preview without parsing geometry).
- **No mandatory whole-file compression.** Compression becomes a per-blob attribute on the heavy
  payloads only (vertex/index buffers, embedded texture pixel data), each blob tagged with its own
  method (`NONE`, `DEFLATE`, ...). The loader decompresses straight from the open stream into memory
  — never through a second temp file on disk, and never twice (no compressing an already-compressed
  texture blob again at a container level).
- **32-bit indices as an explicit, opt-in block variant.** Keep 16-bit as the default/smaller choice
  for the common case (sprites, fonts, particles are almost always small quads); allow 32-bit for
  dense 3D meshes.
- **Reserve, don't implement, a skinned-frame block type.** Per the overengineering concern already
  agreed on: today's implicit static frame stays the only implemented kind. A `FRAME_KIND_SKINNED`
  (or similar) block type is reserved in the format's type table now, so bones can be added later as
  a new optional block without forcing a v12 just to make room.
- **Keep the v8 serialization style.** Field-by-field little-endian read/write helpers
  (`mesh-v8-io.cpp`'s pattern), no struct blitting, continues unchanged into v11's section readers.

## `mesh_deprecated` (legacy import lib/tool)

- Understands v1 (`INITIAL_VERSION_MBM_HEADER`) through v10
  (`TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER`), including the pre-v8
  `deprecated_mbm::INFO_SPRITE` path currently gated by `MBM_ENABLE_MESH_LEGACY_V7`.
- Not linked into the shipped game runtime target.
- Single responsibility: parse old bytes into the same intermediate representation the v11 writer
  consumes, then call that writer. No second, parallel "legacy save" path.
- Once extracted, `MBM_ENABLE_MESH_LEGACY_V7` and the `#if defined(MBM_ENABLE_MESH_LEGACY_V7)` blocks
  inside `MESH_MBM` / `MESH_MBM_DEBUG` are deleted from `core_mbm` — the code physically moves, it
  doesn't just get a permanent flag.

## Async Loading (v11)

Two-phase split, chosen specifically because GPU resource creation in this engine is tied to backend
context/device ownership (OpenGL ES context affinity, DirectX 9 device-thread rules), so it cannot
move to a worker thread, while file I/O and decompression/parsing can.

1. **Background-thread phase** (I/O + decompress + section parsing): produces a self-contained,
   GPU-untouched intermediate structure in CPU memory (vertex/index/material data). No calls into
   `BUFFER_GL`, `TEXTURE_MANAGER` GPU paths, or any backend module from this thread.
2. **Main-thread finish phase**: consumes the intermediate structure, creates GPU buffers/textures
   through the existing backend paths, and publishes the finished `MESH_MBM*`.

Proposed API shape: `MESH_MANAGER::loadAsync(fileName) -> handle`, plus a per-frame
`MESH_MANAGER::pumpAsyncLoads()` called from a known point in the `CORE_MANAGER` frame loop —
finished loads are handed back on the main thread, not via a callback fired directly from the worker
thread.

`MESH_MANAGER::Impl`'s internal registry needs a thread-safe hand-off (mutex-guarded completion
queue) between phase 1 and phase 2; the existing map of loaded meshes stays main-thread-only, exactly
as it is today.

Worker concurrency: start with a small fixed-size pool (1-2 threads) sized for asset loading. This is
intentionally not a general engine-wide job system — scope it to mesh loading only until a second
concrete use case shows up.

## Milestones (draft)

0. **Closed 2026-06-25.** Lock the v11 binary layout (section/TLV table, texture-role encoding,
   index-width flag, reserved skinned-frame block id). Layout is in `docs/mesh-v11-format.md`
   (v1, locked); `TEXTURE_ROLE` header location decided in Scope Decision 4 above. Not yet
   implemented — that's milestone 1.
1. **Closed 2026-06-25.** v11 section read/write helpers, reusing the `mesh-v8-io.cpp` little-endian
   primitive style (`mesh-v11-io.h`/`.cpp`).
2. **Closed 2026-06-25.** `MESH_MBM` / `MESH_MBM_DEBUG` PIMPL split: `mesh-manager.h` stops
   transitively including `header-mesh.h`'s disk structs; only runtime-facing types stay public.
3. **Core slice closed 2026-06-25** (`MESH_MBM_DEBUG::saveV11`): `SECTION_MATERIAL_TRANSFORM`,
   `SECTION_FRAME_STATIC` (path-referenced textures only), `SECTION_DETAIL_PHYSICS`,
   `SECTION_EXTRA_PATHS` — covers 3D/sprite meshes fully. `SECTION_ANIMATION`+FX closed 2026-06-26,
   see milestone 10. Still open: the `SECTION_DETAIL_FONT`/`PARTICLE`/`TILE` payloads; `saveV11`
   still rejects FONT/PARTICLE/TILE_MAP meshes until that follow-up pass lands.
   Not yet wired into any caller (`MESH_MBM_DEBUG::saveDebug()` and its two call sites are untouched).
4. **Standalone core slice closed 2026-06-25** (`MESH_MBM_DEBUG::loadV11`): reads back exactly what
   `saveV11` writes (material+transform, static frames, physics, paths). Not wired into
   `loadDebugImpl`/`MESH_MBM::loadImpl`/`open_decompressed_mesh_file` - no v11 magic-sniffing dispatch
   yet, so this milestone's "fully replaces the compressed-whole-file path" goal is still open and
   deferred to a later, explicit pass (once `saveV11` is reachable from a real save action). Rejects
   (does not silently skip) any section type outside the core slice.
5. `mesh_deprecated` lib: extract the existing v1-v10 read code out of `core_mbm`; wire it as a
   standalone converter that calls the milestone-3 writer. **Phase A closed 2026-06-26**
   (`MESH_MBM::loadV11`): the runtime-class mirror of `MESH_MBM_DEBUG::loadV11`, needed because
   `MESH_MANAGER::load()` - the sole loading path for every renderable type (MESH/SPRITE/PARTICLE/
   FONT/BACKGROUND/TILE) - had no v11 reader at all until then. **Phase B1 closed 2026-06-26**: all
   v1-v10 read/write code (mesh-v8-io.cpp's non-physics primitives, mesh-manager-legacy.cpp,
   deprecated.h/.cpp, the `MBM_ENABLE_MESH_LEGACY_V7` flag and every `#if` site) extracted out of
   `core_mbm` into a new standalone `mesh_deprecated` CMake target (never linked into the runtime -
   offline-only, per Scope Decision 1). `core_mbm` now only understands v11: `MESH_MBM::load()` /
   `MESH_MANAGER::load()` call `loadV11` directly (no more `loadImpl`/legacy dispatch), and
   `MESH_MBM_DEBUG::saveDebug`/`loadDebug` are gone - the two real call sites
   (`mesh-debug-lua.cpp`, `plugins/tiled/tile_editor.cpp`) now call `saveV11`/`loadV11`. Confirmed,
   accepted breakage: `saveV11` rejects animated/FONT/PARTICLE/TILE_MAP meshes, so
   `font_maker.lua`/`particle_editor.lua`/`tile_editor.cpp` can no longer save those types (new or
   old) until a later milestone gives v11 full section coverage - this branch is isolated and will be
   tested before merge, per the user's explicit decision. `MESH_MBM_DEBUG::getInfo`/`getType` (static
   file-peek functions) rewritten to parse the v11 file header only, no legacy fallback.
   Verified: clean `core_mbm` + full project build (`mini-mbm`/`tilemap`/`testLib`/`mesh_deprecated`),
   plus a real dynamic smoke test via `mini-mbm` with a live GL context (X11/`DISPLAY` available in
   this sandbox) - save+reload a plain mesh through `saveV11`/`loadV11`, confirmed FONT-type save
   fails cleanly (not a crash), and confirmed the runtime `MESH::load()` path
   (`MESH_MBM::loadV11`, including the real `BUFFER_GL` GPU upload) loads correctly. **Phase B2
   closed 2026-06-26**: `mesh_deprecated::convertLegacyMeshToV11`'s real v8-v10 parsing body
   (`src/mesh_deprecated/mesh-deprecated.cpp`), replacing the not-implemented placeholder. Scoped to
   exactly what `saveV11` can persist: static (non-animated) 3D/SPRITE/USER/TEXTURE/SHAPE meshes,
   v8-v10 only, path-referenced subset textures only - genuinely new code built entirely on
   `MESH_MBM_DEBUG`'s public API (no `impl->` access available outside `core_mbm`), not a move of the
   deleted `loadDebugImpl`. Reads via the already-ported `mesh-v8-io-legacy.cpp` primitives in the
   original on-disk order (header → extra headers (discarded, `saveV11` re-derives its own
   `SECTION_EXTRA_PATHS`) → draw mode → physics detail (pushed into `INFO_PHYSICS`'s public owning
   vectors) → mesh header → animation headers → per-frame subset headers + geometry), then calls
   `saveV11(..., compress=false, ...)` once at the end - never a partial output file. Rejects, with a
   specific message each: pre-v8 versions, FONT/PARTICLE/TILE_MAP types, embedded (`#u`) or
   solid-color (`#M`/`#RRGGBBAA`) subset textures, and real animation. Two non-obvious findings drove
   the design: (1) the old writer always injected a synthetic `"default"`-named, no-shader-effect
   animation header into every file that had zero real animations, so `totalAnimation >= 1` for
   virtually all real files - a naive "reject any animation" check would block 100% of static-mesh
   imports, so exactly that single synthetic shape is detected and discarded, anything else is
   rejected as real animation; (2) `MESH_MBM_DEBUG::addVertex` unconditionally forces
   `hasNorText[0] = HAS_NOR_IN_FILE` on every call (it always allocates a normal array), so
   `setHasNormal`/`setHasTexture` must be called *after* the whole frame/subset/vertex loop, not
   before, or they'd be silently clobbered back. Also handles: `addVertex`/`addIndex`'s own
   auto-contiguous vertex/index placement means the legacy file's `vertexStart` is only needed to
   rebase frame-global index values back to subset-local before calling `addIndex` (which re-adds the
   *new* vertexStart itself) - not for positioning vertices, which the public API derives from call
   order automatically. `HAS_TEX_FIRST_FRAME` shares frame 0's UV array into later frames via a small
   cache, matching the format's own "first frame only" convention.
   Verified: clean `mesh_deprecated` + full project build; a real dynamic test via two temporary
   throwaway executables (a fixture generator hand-writing legacy v9 bytes via the preserved
   `mesh-v8-io-legacy.cpp` write-side primitives + `mbm::MINIZ::compressFile`, since `core_mbm` itself
   can no longer produce legacy-format files; and a conversion driver) - converted a static v9 fixture
   (2 frames, indexed subset, path texture, one material texture slot, a CUBE and a SPHERE physics
   shape, the synthetic `"default"` animation entry) and confirmed, after reloading the v11 output via
   `MESH_MBM_DEBUG::loadV11`, byte-exact position/texture/material-slot/index/physics round-trip;
   separately confirmed clean rejection (not a crash, no partial output) for a real-2-animation
   fixture, a pre-v8 fixture, and a FONT-typed fixture; confirmed no leftover decompressed temp file
   after any rejected conversion. All temporary executables, fixtures, and the temporary
   `CMakeLists.txt` entries used to build them were removed afterward.
5.5. **Closed 2026-06-26.** Decoupled `UBER_IMG` from the legacy mesh v8 header (Scope Decision 5).
   `uber-image.cpp` now has its own private, self-contained `UberImgHeaderV1` (4-byte `"UBIM"` tag +
   version/depth/channel/hasAlpha/width/height/lenght, read/written via `util::le_io` primitives
   directly, no struct shared with mesh files) — `load`/`loadFromFileOpened`/`save` all use it.
   `readHeaderV8`/`writeHeaderV8`/`readHeaderImgV8`/`writeHeaderImgV8` deleted from `core_mbm`'s
   `mesh-v8-io.h`/`.cpp` (they remain only in `mesh_deprecated`, for reading old `.uberimg` files
   during migration — not wired up there yet, since `mesh_deprecated` has no UBER_IMG-aware caller
   either, this is just keeping the primitives available for Phase B2 if ever needed).
   `core_mbm` now has zero v1-v10 format code left anywhere, including outside the mesh subsystem
   proper. Verified: clean `core_mbm` + full project build, plus a standalone round-trip test (save
   a 64x64 RGB image, reload it, compare metadata and pixel data byte-for-byte) confirming the new
   format works correctly — this is a real behavior change to an actively-used texture format
   (`texture-manager.cpp`'s `.uberimg` loader), not just an internal refactor, so it got its own
   dynamic verification.
6. **Closed 2026-06-26.** Async loading: background parse phase + main-thread GPU finish phase +
   `MESH_MANAGER::loadAsync`/`pumpAsyncLoads`. `MESH_MBM::loadV11` was split into a pure-CPU
   `parse_v11_intermediate` free function (file I/O + v11 section parsing into a new
   `mbm::MESH_LOAD_INTERMEDIATE_V11`, safe on a worker thread - no `BUFFER_GL`/`TEXTURE_MANAGER`/
   `addPath`/`EnablePixelPerfectTexture` calls anywhere in it) and `MESH_MBM::finishLoadFromIntermediate`
   (the main-thread-only GPU-finish half: texture loads, `BUFFER_GL` upload, `addPath`, global-state
   mutation). `loadV11` itself is now just `parse` then `finish`, sharing 100% of its logic with the
   async path. `MESH_MANAGER::Impl` gained a lazily-started fixed 2-thread worker pool (job queue +
   completion queue, each behind its own mutex; workers block on a condition variable when idle) -
   `loadAsync(fileName, callback)` pushes a job (or, on a cache hit, a pre-completed result - the
   callback always fires from `pumpAsyncLoads()`, on the main thread, never inline, so callers never
   have to special-case "sometimes synchronous"); `pumpAsyncLoads()` drains completed jobs, does the
   GPU-finish work, and dispatches callbacks. `~MESH_MANAGER()` signals and joins the pool before its
   existing cache-release loop runs. Wired into the real frame loop:
   `CORE_MANAGER::update()` calls `pumpAsyncLoads()` once per frame, right after `logic()`. Per the
   approved scope, no Lua bindings and no render call site (`mesh.cpp`/`sprite.cpp`/etc.) switched to
   use it - they keep calling synchronous `load()` exactly as today; this is live, working
   infrastructure with no callers yet.
   Non-obvious fix needed along the way: `mbm::INFO_PHYSICS` has a user-declared destructor (see
   milestone-5 notes), so it has no implicit move ctor - that silently made
   `MESH_LOAD_INTERMEDIATE_V11` (which embeds an `INFO_PHYSICS`) fall back to copy semantics, which
   is impossible once it also holds non-copyable `unique_ptr`-based frame buffers, surfacing as a
   `vector::push_back` "copy ctor is implicitly deleted" build error from deep inside
   `MESH_MANAGER::Impl`'s completion queue. Fixed by giving `MESH_LOAD_INTERMEDIATE_V11` an explicit
   `noexcept` move ctor/assignment that moves `infoPhysics`'s four owning vectors individually
   (mirrors `finishLoadFromIntermediate`'s own ownership-transfer code) and deletes copy.
   Verified: clean `core_mbm` + full project build; a real dynamic test via `testLib` with a live GL
   context (`DISPLAY=:1`) - built a throwaway triangle mesh in memory via `MESH_MBM_DEBUG`, saved it
   (legacy `.msh` fixtures in `src/test-lib/` predate v11 and can't exercise `loadV11`, so a fresh
   file was required), then exercised: cold `loadAsync` (real worker-thread parse + main-thread GPU
   finish, success), a forced-fresh sync `load()` after `fakeRelease` (confirms the `loadV11`
   refactor didn't regress the synchronous path), a cache-hit `loadAsync` on the same file (confirms
   it still defers to `pumpAsyncLoads` rather than firing inline, and returns the same cached
   pointer), a missing-file `loadAsync` (confirms a clean `success=false`, no crash), and a `SIGTERM`
   mid-flight (confirms `stopAndJoinWorkers()` doesn't hang or crash on shutdown). All temporary test
   code and the throwaway mesh file were removed afterward.
7. **Closed 2026-06-26.** Built-in shader resource cleanup: converted `shader-resource-opengl_es.cpp`/
   `-directx9.cpp`/`-metal.mm` built-ins to semantic `TEXTURE_ROLE` naming by default; legacy
   `sample0`/`sample1`/`sample2` remains fully supported as the naming profile for old custom shaders.
   **Key finding: almost all the runtime infrastructure already existed** -
   `SHADER_TEXTURE_NAMING`/`getTextureRoleShaderName`/`detectShaderTextureNamingProfile` (`shader.h`/
   `.cpp`) already mapped both conventions and ran detection on every shader load
   (`BASE_SHADER::loadShader`, built-in or custom, no special-casing), and the runtime texture-binding
   code (`shader-opengl_es.cpp`/`shader-directx9.cpp`) already looked up uniforms via each shader's own
   *detected* naming profile rather than a hardcoded name. So this milestone touched zero binding/
   detection code - it was a **pure identifier rename** inside existing built-in shader source strings:
   `sample0`→`TextureDiffuse`, `sample1`→`TextureAnimationEffect`, `sample2`→`TextureNormal`, applied
   to each platform's `buildLitTexturedPixelShader*()` (the default lit/textured shader), the
   `resourceShader[]` catalogue (~40-50 post-processing/effect shaders per platform), and
   `getParticlePSCode()`/`getSteeredParticlePSCode()`. One real identifier collision was found and
   preserved: `shader-resource-opengl_es.cpp`'s `"sketch.ps"` declares unrelated local
   `const vec2 sample1/sample2` offset constants (its own texture is `Image`, not a `TEXTURE_ROLE`
   uniform) - confirmed as the *only* such collision across all three platforms (the DirectX9 port uses
   an array instead, the Metal port already renamed its offsets to `k_s1`/`k_s2`/`k_s3`), excluded from
   the rename, and verified byte-identical to the original afterward. Also updated, per explicit user
   scope confirmation (AskUserQuestion - "update them too" over "leave as-is"): the Lua-shipped example/
   template shaders (`editor/shaders/shader_cfg.lua`'s 6-shader library, `editor/shader_editor.lua`'s
   new-custom-shader template) and one in-editor tooltip string (`editor/lang/language.lua`) describing
   the old `sample2` name.
   Verified: whole-word grep audit confirmed zero remaining `sample[012]` identifiers outside the 5
   preserved `sketch.ps` lines; clean `core_mbm` + full project build; a real dynamic test via
   `testLib`/`DISPLAY=:1` compiling, through the real GLSL compiler, the default lit-textured shader
   (paired with a matching test vertex shader providing `vNormalView`/`vPositionView`/`vTexCoord`,
   since `resourceShader[]` ships no generic paired `.vs` for it - it's meant to be selected alongside
   a user-authored one), two `resourceShader[]` post-processing entries (`pie.ps` single-texture,
   `blend.ps` dual-texture), `sketch.ps` itself (confirming the preserved offsets still compile
   correctly), and an inline legacy custom shader literally using `sample0` (confirming
   `detectShaderTextureNamingProfile`'s fallback still works after the built-ins changed) - all
   compiled cleanly with the expected naming profile detected. Two pre-existing, unrelated issues were
   discovered and explicitly *not* fixed (out of this milestone's scope, confirmed present
   byte-for-byte in a pre-rename backup): `getParticlePSCode()` has a malformed ternary
   (`color.rgb ? texColor.rgb` with no `:` false-branch) that's always failed to compile;
   `detectShaderTextureNamingProfile`'s plain identifier-text scan has always mis-flagged `sketch.ps`
   as `LEGACY_SAMPLE` (it matches the literal `sample1`/`sample2` text in the unrelated offset
   constants) even though the shader doesn't use either as a texture - harmless since no actual binding
   lookup for those names ever succeeds or is needed by that shader. All temporary test code was
   removed afterward.
8. **Closed 2026-06-26.** Per-blob compression (`DEFLATE`) for `SECTION_FRAME_STATIC` as an opt-in
   save-time setting (off by default). The on-disk format and codec were already in place since
   milestone 0/1 - `SECTION_HEADER_V11`'s `compression`/`uncompressedLength`/`compressedLength`/
   `crc32Value` fields, and `util::writeSectionV11`/`readSectionV11` (`mesh-v11-io.cpp`) already fully
   implemented DEFLATE via `mbm::MINIZ`. The actual gap: `MESH_MBM_DEBUG::saveV11` exclusively used
   `util::writeSectionV11Streamed`, which hardcoded `SECTION_COMPRESSION_NONE` and said so in its own
   doc comment ("milestone 8's job") - `writeSectionV11`'s compression path was dead code from
   `saveV11`'s perspective. Fixed by having `writeSectionV11Streamed` delegate its header+payload
   write to `writeSectionV11` (once the streamed payload is read back into memory for `crc32Value`
   anyway, it's the same shape `writeSectionV11` already takes) instead of duplicating the
   NONE-only logic, then truncating the file to the new (possibly shorter, if compressed) end via a
   new small cross-platform `truncateFileToCurrentPosition` helper (`ftruncate`+`fileno` on
   POSIX, `_chsize`+`_fileno` on Windows) - needed because the payload was originally streamed to
   disk uncompressed before compression could shrink it, leaving stale tail bytes otherwise.
   `saveV11` gained a new `compress` bool parameter, wired into the `SECTION_FRAME_STATIC` per-frame
   loop only (the one section large enough for compression to matter - material transform/extra
   paths/physics stay uncompressed). Threaded through both real call sites
   (`mesh-debug-lua.cpp`'s `meshD:save(file, calcNormal, calcUV, compress)` - new optional 5th Lua
   arg, defaults to `false` - and `tile_editor.cpp`, which passes `false` explicitly, no behavior
   change). Reader side needed zero changes - `readSectionV11`/`skipSectionPayloadV11` were already
   compression-agnostic. (`TEXTURE_REF_V11`'s `EMBEDDED_COMPRESSED` variant still has no writer per
   milestone 3's scope, so "texture blobs" from this milestone's original one-liner isn't reachable
   yet - out of scope for this pass.)
   Verified: clean `core_mbm` + full project build; a real dynamic test via `testLib` with a live GL
   context (`DISPLAY=:1`) - built a throwaway 32x32 grid mesh (1024 vertices, 1922 triangles; big
   and repetitive enough to actually compress, unlike a 3-vertex triangle - milestone 5.5 hit that
   exact trap with too-small test data), saved it once uncompressed and once with `compress=true`,
   confirmed the compressed file was 27% the size of the plain one, then reloaded both via
   `MESH_MBM_DEBUG::loadV11` and confirmed byte-for-byte identical position/uv/index data against
   each other and the original input. All temporary test code and throwaway files were removed
   afterward.
9. *(Reserved, not implemented)* skinned-frame block + GPU skinning — only after real demand is
   confirmed, not speculatively.
10. **Closed 2026-06-26.** Real `SECTION_ANIMATION`+FX support in `saveV11`/`loadV11`, unblocking
    `mesh_legacy_converter` on real animated sprites. Triggered by running the converter against the
    user's actual production assets (`tower-defense/sprite/*.spt`): the large majority have genuine
    multi-animation data (named frame ranges) that Phase B2's narrow single-placeholder heuristic
    correctly rejected as "real animation, not yet supported."
    `docs/mesh-v11-format.md` §6b now specifies the payload: `ANIMATION_HEADER_V11` (name, frame
    range, time, type, blend state, `hasFx`) plus an optional `FX_HEADER_V11` (blend operation, an
    optional FX texture reusing `TEXTURE_REF_V11`, and up to a PS/VS `SHADER_STEP_V11` pair - each a
    shader *name* reference, e.g. `"transparent.ps"`, plus a `SHADER_VAR_V11` array of per-variable
    type tag + min/max). `saveV11` no longer rejects animated meshes; both it and `loadV11` gained a
    `SECTION_ANIMATION` branch (`mesh-manager.cpp`), built on `MESH_MBM_DEBUG::appendAnimationHeader`/
    `getAnimationHeader`/`getTotalAnimationHeaders` (already public, already used by
    `mesh-debug-lua.cpp`). Confirmed with the user this lands in `core_mbm` (not isolated inside
    `mesh_deprecated`) so the data is actually readable by something, even though wiring it into real
    in-game playback (`MESH_MBM::loadV11` → `ANIMATION_MANAGER::populateAnimationFromHeader`,
    resolving a stored shader name into a compiled/bound shader) is an explicit, separate, later
    follow-up - out of scope here.
    `mesh_deprecated::convertLegacyMeshToV11` (`mesh-deprecated.cpp`) replaced its reject-unless-
    synthetic-placeholder animation gate with real import logic, reverse-engineered from the
    recovered pre-deletion writer: for `version >= TEXTURE_ANIMATION_EFFECT_VERSION_MBM_HEADER`, the
    animation-level FX texture-effect header is read *before* the PS/VS step pair, not after (an
    earlier diagnostic that assumed the opposite order produced a clean `anim[0]` but corrupted
    `anim[1]` onward); and a shader step's variable count is `sizeArrayVarInBytes / 4`, not the raw
    byte count (`INFO_SHADER_DATA`'s own constructor already divides by 4 - the diagnostic initially
    didn't). Diagnosing all 59 real `.spt` files this way found ~40 with plain frame-range animations
    and no real shader effect (the legacy `hasShaderEffect` flag is unconditionally set to 1 by the
    old writer regardless, a writer quirk not real data) and ~9 with a real effect referencing the
    engine's own named built-in shader catalogue (`transparent.ps`/`blend.ps`/`pie.ps`/`wave.ps`/etc -
    the same catalogue milestone 7 renamed) plus variable values - never an arbitrary external shader
    file, never a populated `lenTextureStage2` (a second texture per step - explicitly out of scope,
    rejected with a clear message, since no real file uses it).
    A second, unrelated pre-existing bug was found and fixed along the way, discovered only because
    real sprite geometry (not Phase B2's synthetic fixture) finally exercised it:
    `mesh-deprecated.cpp`'s `readFrame` read a frame's vertex/normal/uv buffers *before* its index
    buffer, but the legacy on-disk order (confirmed against the recovered
    `read_frame_geometry`/`load_from_separated_buffers_common`) is index buffer first, vertex data
    after, whenever `typeBuffer == "IB"`. Every real indexed sprite subset was silently reading
    garbage indices until this was reordered to match.
    Verified: clean `core_mbm`/`mesh_deprecated`/`mesh_legacy_converter` build; a temporary round-trip
    test (`MESH_MBM_DEBUG` with one plain animation via `addAnimation` and one with a PS shader effect
    + 1 variable built via `appendAnimationHeader`, `saveV11`, `loadV11`, field-by-field comparison)
    passed; `mesh_legacy_converter` run against all 59 real `tower-defense/sprite/*.spt` files - all 59
    now convert successfully (previously: 0, blocked at "animated meshes are not supported yet"); a
    reload-and-dump check on several converted outputs (`enemy_1.spt`'s 4 animations, `blast.spt`'s PS
    `transparent.ps` effect, `add-support.spt`'s single no-op placeholder) confirmed names/ranges/
    types/shader names/variable values match the original legacy source exactly. All temporary
    diagnostic tooling and `CMakeLists.txt` entries were removed afterward.
11. **Closed 2026-06-26.** Wired milestone 10's `SECTION_ANIMATION`+FX data into the real runtime load
    path - the explicit follow-up milestone 10 deferred. Triggered immediately: converting a real
    sprite in place and previewing it via `mesh_debug.lua` failed with "loadV11 does not support this
    section type yet" - that error comes from `parse_v11_intermediate` (mesh-manager.cpp), the
    pure-CPU function shared by `MESH_MBM::loadV11` and the async load path (milestone 6), which
    milestone 10 never touched (it only extended the offline `MESH_MBM_DEBUG` class).
    Turned out to be a small, mechanical gap, not a new design problem: `MESH_MBM::Impl` already had
    its own `INFO_ANIMATION infoAnimation` field (`MESH_MBM::release()` already calls
    `infoAnimation.release()`), `MESH_MBM::getTotalAnimations()`/`getAnimationHeader()` already
    existed and worked, and `ANIMATION_MANAGER::populateAnimationFromHeader`/
    `populateTextureAnimationEffectFromMesh` were already real, active call sites in
    `SPRITE::load()`/`MESH::load()`/`background.cpp`/`font.cpp`/`tile.cpp` - they'd simply never had
    real data to consume. Most surprising: the shader-by-name resolution flagged during milestone 10's
    planning as "a new capability we'd need to build" already existed too -
    `populateAnimationFromHeader` already calls `device->getShaderConfig().getShader(fileNameShader)`
    on exactly the `INFO_FX`/`INFO_SHADER_DATA` shapes milestone 10 already populates.
    So the fix was three steps: (1) added `util::INFO_ANIMATION infoAnimation` to
    `mbm::MESH_LOAD_INTERMEDIATE_V11`, extending its existing custom move ctor/assignment (the same
    pattern already used for `infoPhysics`, since `INFO_ANIMATION` also has a user-declared destructor
    and thus no implicit move) to move `lsHeaderAnim` too; (2) extracted the `SECTION_ANIMATION`
    parsing logic milestone 10 wrote inline inside `MESH_MBM_DEBUG::loadV11` into a shared
    `parse_animation_section_v11` free function, used by both it and a new
    `SECTION_ANIMATION` case in `parse_v11_intermediate`; (3) in `MESH_MBM::finishLoadFromIntermediate`,
    moved the parsed `infoAnimation.lsHeaderAnim` into `impl->infoAnimation.lsHeaderAnim`, next to the
    existing `infoPhysics` transfer. No changes needed anywhere else.
    Verified: clean `core_mbm` + full project build; a real dynamic test via `testLib`/`DISPLAY=:1`
    (temporary code in `my-scene-test.cpp::onInitScene`, removed after) - loaded a converted
    `enemy_1.spt` (4 plain animations) and `blast.spt` (a real PS shader effect) through the actual
    `mbm::SPRITE::load()` runtime path (not just the debug API): `getTotalAnimation()`/names/frame
    ranges matched the source exactly, and `blast.spt`'s `transparent.ps` effect resolved and loaded
    successfully (`FX::getCodePS()` non-empty, no "Shader ... not found at cfg shader list!" error) -
    confirming the shader-name-resolution path genuinely needed zero new code. All temporary test code
    and converted fixtures were removed afterward.

## Open Questions

(Resolved for milestone 0: `TEXTURE_ROLE` header location — see Scope Decision 4. Remaining items
below belong to later milestones.)

- Default per-blob compression for new saves from Mesh Debug/Sprite Maker: on or off by default?
- Thread pool lifetime/ownership for milestone 6: private to `MESH_MANAGER::Impl`, or a small shared
  engine utility other systems (textures, audio) could reuse later without turning it into a full job
  system today?
- Should `MESH_MBM`/`MESH_MBM_DEBUG` class names change along with the format, or stay as-is since
  `.msh`/`.spt`/`.fnt`/`.ptl`/`.tile` already carry the type distinction at the extension level?
