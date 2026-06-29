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
    **Follow-up fix found immediately after, same milestone**: converting a real sprite in place and
    opening it in `mesh_debug.lua`'s editor showed frames but no animations - a *different* bug from
    the one above. `MESH_MBM_DEBUG::getInfo(fileNamePath, ...)` (mesh-manager.cpp) - the static
    "peek a v11 file's info without fully loading it" function `mesh_debug.lua`'s `"getInfo"` Lua
    binding uses to populate its editor-side `info.animation`/`info.frame` cache - only ever read the
    *first* section and unconditionally `break`'d out of the section loop right after, regardless of
    how many sections actually existed. It never read `SECTION_ANIMATION` (or even counted
    `SECTION_FRAME_STATIC`), so `totalAnimation` (and `totalFrames`) were always 0 for any v11 file -
    a pre-existing gap since milestone 4, just never user-visible until an editor workflow that
    actually depends on `getInfo` (rather than a full `loadV11`) needed real animation data. Confirmed
    via `MESH_MBM_DEBUG::loadV11` directly that the converted file's `SECTION_ANIMATION` data was
    always correct - this was purely a stale lightweight-peek function, not a data-loss or
    conversion bug. Fixed by removing the unconditional `break` and adding counters for
    `SECTION_ANIMATION`/`SECTION_FRAME_STATIC` in the existing per-section loop, so `getInfo` now
    reports accurate counts without needing a full load. Verified via a temporary diagnostic calling
    the exact same overload `mesh_debug.lua`'s Lua binding calls: a converted `archer-1.spt` now
    reports `totalFrames=12 totalAnimation=4`, matching its real content (4 named animations:
    `stand-right`/`attack-right`/`stand-left`/`attack-left`). Temporary diagnostic removed afterward.

- **Milestone 12 (closed 2026-06-27): real `SECTION_DETAIL_FONT`/`SECTION_DETAIL_PARTICLE` support.**
  `saveV11`/`mesh_legacy_converter` rejected any FONT or PARTICLE mesh outright (same deferred-gap
  shape `SECTION_ANIMATION` had before milestone 10) - hit converting a real production particle file
  (`star-explode.ptl`). Scoped to FONT+PARTICLE only (both small: `STAGE_PARTICLE` is a flat 70-byte
  struct with no strings; FONT's detail block is just name/spacing/letter metadata - glyph geometry
  already lives in ordinary `SECTION_FRAME_STATIC` frames). TILE_MAP stays deferred (explicit user
  decision) - a much bigger structure (full 2D tile grid per layer, object/property lists), its own
  future milestone. Format: `SECTION_DETAIL_PARTICLE` bundles all stages into one section
  (`uint16_t stageCount` + that many `STAGE_PARTICLE` blobs, same field order as the existing
  `writeStageParticleV8`/`readStageParticleV8`); `SECTION_DETAIL_FONT` bundles a header
  (name/spaceX/spaceY/heightLetter/letterCount) + that many `DETAIL_LETTER`-shaped entries, one
  section per file (not "repeated, one per item" like `SECTION_ANIMATION` - these have no per-item
  optional sub-blocks, so bundling like `SECTION_DETAIL_PHYSICS` already does is simpler). Applied the
  lesson from the `SECTION_ANIMATION` split (milestones 10/11 - splitting "debug class" from "runtime
  class" into two passes caused a real user-visible gap in between, found and fixed same-day as a
  follow-up to milestone 11): both `MESH_MBM_DEBUG` (saveV11/loadV11) and the real runtime class
  (`MESH_MBM` via `parse_v11_intermediate`/`finishLoadFromIntermediate`) were wired together in this
  one milestone, no separate follow-up needed. New shared parsing helpers
  `parse_particle_detail_section_v11`/`parse_font_detail_section_v11` (mesh-manager.cpp, alongside the
  existing `parse_animation_section_v11`) are called from both `MESH_MBM_DEBUG::loadV11` and
  `parse_v11_intermediate`, matching that precedent exactly. `MESH_LOAD_INTERMEDIATE_V11` gained a
  `void *extraInfo` field (trivially movable, unlike `infoPhysics`/`infoAnimation` - but still needs
  explicit cleanup in the intermediate's destructor, guarded by `typeMe`, so an abandoned/failed load
  doesn't leak). On the `mesh_deprecated` importer side, FONT/PARTICLE detail blocks turned out to live
  at the exact same legacy stream position as physics shapes - `readPhysicsDetail`'s inner
  `DETAIL_MESH.type` dispatch (1-4 = CUBE/SPHERE/CUBE_COMPLEX/TRIANGLE) gained `case 5` (FONT, via the
  already-present-but-previously-unused `readDetailHeaderFontV8`/`readDetailLetterV8`) and `case 6`
  (PARTICLE, via the already-present `readStageParticleV8`), both installing their result via
  `MESH_MBM_DEBUG::replaceDetailInfo` - the same generic mutation API `tile_editor.cpp` already uses
  for TILE_MAP. Found and fixed one real ordering hazard along the way: `meshDebug.setMeshType(typeMe)`
  was being called *after* `readPhysicsDetail` in the original code - moved it earlier so a FONT/
  PARTICLE detail pointer installed via `replaceDetailInfo` is never momentarily paired with the wrong
  (still-default) `typeMe`, which would have made `deleteExtraInfo()` misinterpret the pointer if an
  unrelated failure further down destroyed `meshDebug` first. Verified: clean build; converted the
  real `star-explode.ptl` (PARTICLE) and a real `Calistoga-Regular-50.fnt` (FONT, from
  `tower-defense/sprite/`) end-to-end through `mesh_legacy_converter`, confirmed via a temporary
  diagnostic (`MESH_MBM_DEBUG::loadV11` + `getDetailInfo()`) that all stage/letter data round-tripped
  correctly (1 particle stage with real field values; 225 font letters with correct widths/heights/
  frame indices); then confirmed both load correctly through the real runtime path too (`mbm::PARTICLE
  ::load`/`mbm::FONT_DRAW::loadFont`, which both internally call `MESH_MANAGER::load` →
  `getInfoParticle()`/`getInfoFont()`) via a temporary `testLib`/`DISPLAY=:1` dynamic test. All
  temporary diagnostics/test code removed afterward.

- **Milestone 13 (closed 2026-06-27): real `SECTION_DETAIL_TILE` support (TILE_MAP).** Last deferred
  mesh type - closes out the mesh-v11 migration's full type coverage. `util::BTILE_INFO` (header-
  mesh.h) bundles a `BTILE_HEADER_MAP` (grid/background metadata), a `BTILE_LAYER*` array (each with
  its own `BTILE_INDEX_TILE*` grid + a `float offset[3]`), a `BTILE_BRICK_INFO*` array (one per
  distinct brick - rotation/flip/original-index; brick *geometry* is ordinary `SECTION_FRAME_STATIC`
  frames, same relationship FONT glyphs have to their frames), and `std::vector<BTILE_OBJ*>`/
  `std::vector<BTILE_PROPERTY*>` (Tiled-style collision shapes and key/value metadata).
  `SECTION_DETAIL_TILE` bundles all of it into one section (header → bricks → per-layer
  header+grid → objects → properties), same convention as `SECTION_DETAIL_PARTICLE`/`FONT`, reusing
  every existing v8 struct's field layout verbatim (only `background_texture[62]` became a
  length-prefixed string, matching every other v11 string field). Wired `MESH_MBM_DEBUG` and the real
  runtime class `MESH_MBM` together in this one pass (the milestone-10/11 lesson, applied again for
  milestone 12 and now this one).
  **Found and fixed a real bug via recovered pre-deletion history, not guesswork**: initially assumed
  (wrongly, by analogy with FONT/PARTICLE) that `mesh_deprecated`'s `readPhysicsDetail` should wrap the
  whole TILE read in `for (j < detail.totalBounding)`. Real files broke immediately (garbage object/
  property data) because `detail.totalBounding` for TILE actually piggy-backs `layerCount` as a sanity
  check, not a repeat count - confirmed by recovering the actual pre-milestone-5 deleted core_mbm
  loader via `git log --all -p` (it read the whole tile block exactly once, validated
  `layerCount == detail.totalBounding`, and advanced the outer dispatch counter by a hardcoded `+= 1`
  - same hardcoded `+=1`, it turns out, for FONT and PARTICLE too, which my existing milestone-12 code
  had been getting away with only because `detail.totalBounding` happened to equal 1 in every file
  tested so far). Fixed by introducing a `consumed` variable (defaults to `detail.totalBounding` for
  the physics-shape cases 1-4, explicitly set to `1` for FONT/PARTICLE/TILE) and replacing the loop's
  blanket `i += detail.totalBounding` with `i += consumed`. The same history recovery also revealed
  `BTILE_LAYER::offset[3]` *is* read from legacy files after all (via the generic `readFloat3ArrayV8`,
  missed by an earlier grep scoped to "Tile/BTile/Brick" names only) - so the importer now reads it
  for real instead of defaulting to zero. Verified: clean build; converted the real legacy fixture
  `src/test-lib/tile-map-test.tile` (10x6 grid, 2 layers, 27 distinct bricks) end-to-end through
  `mesh_legacy_converter`, confirmed via a temporary diagnostic that map/brick/layer/cell data
  round-tripped correctly; a separate synthetic round-trip (`saveV11`→`loadV11`, since the real fixture
  had no Tiled objects/properties to exercise that path) confirmed objects (name/type/points) and
  properties (owner/name/value/type) and the per-layer `offset` all round-trip exactly; then confirmed
  the real runtime path too (`mbm::TILE::load`, which internally calls `MESH_MANAGER::load` →
  `parse_v11_intermediate`/`finishLoadFromIntermediate`) via a temporary `testLib`/`DISPLAY=:1` dynamic
  test. All temporary diagnostics/test code removed afterward.
- **Mesh-v11 migration now has full mesh-type coverage** - every `util::TYPE_MESH_*` value `saveV11`/
  `loadV11` need to handle is implemented. Only milestone 9 (skinned-frame block + GPU skinning)
  remains explicitly reserved/not-implemented by design, picked up only if real demand shows up.

- **Milestone 14 (closed 2026-06-27): renamed `CURRENT_VERSION_MBM_HEADER` →
  `LEGACY_HEADER_VERSION`.** Pure cleanup, no behavior change. The old name made it look like a v11
  versioning bug; it's actually the legacy in-memory `util::HEADER::version` field for the v1-v10
  on-disk format, unrelated to the real v11 file version (`FILE_HEADER_V11::formatVersion =
  MBM_V11_FORMAT_VERSION = 11`, correct everywhere it's written). Updated `header-mesh.h`'s
  `#define` + comment and the 4 call sites in `header-mesh.cpp`/`mesh-manager.cpp`. Confirmed no
  other references anywhere in the tree; full project build passes.

- **Milestone 15 (closed 2026-06-27): removed the per-section `tmpfile()` staging.**
  `stage_payload_as_tmpfile` called `tmpfile()` once per v11 *section* (~10 call sites - one per
  frame/animation/detail block, so 50+ times for a typical mesh), each doing a full
  write+rewind+read round-trip through the OS filesystem instead of reading the in-memory
  `payload` byte vector already held. Real cost beyond raw overhead: MSVC's `tmpfile()` defaults
  to creating the file in the root of the current drive, which fails for non-admin Windows users.
  Replaced with `MEM_CURSOR_V11` (`include/core_mbm/header-mesh.h` - a non-owning `{data, size,
  pos}` view, declared there rather than the internal `mesh-io-primitives.h` because
  `MESH_MBM_DEBUG`'s class declaration in the public `mesh-manager.h` needs the type visible).
  Added `MEM_CURSOR_V11` overloads of the six low-level primitives (`readBytes`/`readI16LE`/
  `readU16LE`/`readI32LE`/`readU32LE`/`readF32LE`) in `mesh-io-primitives.h` alongside the existing
  `FILE*` ones. Confirmed by direct research (not guesswork) that every payload-level reader in
  `mesh-v11-io.h`/`mesh-v8-io.h` (the latter is SECTION_DETAIL_PHYSICS's v8-layout reuse) does pure
  sequential forward reads only - no seeking - and is *only ever* called against a staged section
  buffer, never the real on-disk file (the envelope readers - `readFileHeaderV11`/
  `readSectionHeaderV11`/`readSectionV11`/`skipSectionPayloadV11` - are the only ones touching the
  real `FILE*`, and stayed unchanged). This meant every payload-level read function could have its
  signature swapped directly from `FILE*` to `MEM_CURSOR_V11&` with no template/dual-mode needed -
  simpler than the originally-scoped plan. `stage_payload_as_tmpfile` became
  `stage_payload_as_cursor`, just wrapping the already-owned payload vector (`{payload.data(),
  payload.size(), 0}`) - no allocation, no syscall. This also deleted a real latent bug: every one
  of the ~10 call sites unconditionally called `fclose(tmp)` *after* the parse helper returned, but
  several of those helpers could already fail partway through and call `log_util::onFailed(tmp,
  ...)` internally, which itself calls `fclose` on a non-null handle - a double-close on the error
  path. Removing the FILE* entirely removes the whole hazard class, not just the syscall cost.
  Found one piece of genuinely dead code along the way: `MESH_MBM::readTriangleDetailCompat` has
  zero callers anywhere in the codebase (its sibling `MESH_MBM_DEBUG::readDebugTriangleDetailCompat`
  is the one actually used) - left in place (just signature-converted so it still compiles) rather
  than deleted, since removing dead code wasn't this milestone's scope; worth a follow-up cleanup.
  Verified: full project build clean (no warnings); converted real legacy fixtures for all 5 mesh
  types (`.msh`, `.spt`, `.ptl`, `.fnt`, `.tile` - pulled from `tower-defense`'s git history since
  the live copies had already been migrated to v11 by earlier milestones) through
  `mesh_legacy_converter`; then loaded all 5 converted v11 files through the real runtime classes
  (`mbm::MESH`/`mbm::SPRITE`/`mbm::PARTICLE`/`mbm::TILE`/`mbm::FONT_DRAW`, all via
  `MESH_MANAGER::load` → `parse_v11_intermediate`/`finishLoadFromIntermediate`, the
  `MEM_CURSOR_V11`-based path) via a temporary `testLib`/`DISPLAY=:1` dynamic test - all 5 reported
  `OK`. Temporary test code and `/tmp` fixtures removed afterward.

- **Milestone 16 (real 32-bit index support): deferred, parked in backlog.** Scoping work
  revealed the real surface area is much bigger than the original plan assumed. The plan only
  accounted for load/save (`mesh-manager.cpp`'s `FRAME_HEADER_V11` read/write) and the GPU
  upload/draw layer (~25-35 call sites across `shader-opengl_es.cpp`/`shader-directx9.cpp`,
  hardcoded `uint16_t`/`GL_UNSIGNED_SHORT`/`D3DFMT_INDEX16`). What it missed:
  `BUFFER_MESH_DEBUG::indexBuffer` (`uint16_t*`) is also the backbone of the entire in-memory
  mesh-*editing* API used by `mesh_debug.lua`/`sprite_maker.lua` - `addVertex`/`addIndex`/
  `insertSubset`/`removeSubset`/`mergeBuffer` and index-rebasing-on-merge, ~30 call sites in
  `mesh-manager.cpp`, several with `uint16_t`-wraparound-sensitive casts (e.g.
  `static_cast<uint16_t>(vertexStart)` when rebasing indices after a merge) that would silently
  corrupt data above 65535 vertices - exactly the case 32-bit indices exist to support. Doing this
  properly means retrofitting all of that editing logic too, not just load/save/render - a
  milestone-sized effort on its own, not a few-hour pass. User chose to defer rather than narrow
  the scope or commit to the bigger effort right now - revisit as its own properly-scoped milestone
  if real demand shows up. The research above (storage/GPU-upload/draw-call findings) stays valid
  for whenever this gets picked back up.

- **Milestone 17 (closed 2026-06-27): decoupled the animation-effect (FX) texture from per-subset
  role dispatch.** Real investigation (not the plan's original guess) found the issue was smaller
  and more localized than scoped: the *write* side (`mbm::bindTextureAnimationEffect`/
  `FX::bindTextureAnimationEffect`, `shader-fx.cpp`) already stores the FX texture exactly once
  per mesh buffer (`setTextureByStage(tex, fxSlot, /*subset=*/0)`), called once per frame-buffer
  from each render type's update logic (`sprite.cpp`/`mesh.cpp`/`tile.cpp`/etc.) - never per-subset.
  DirectX9's draw functions (`shader-directx9.cpp`) already bound it correctly too -
  `bindTextureRoleD3D(pd3dDevice, pBufferId, 0, TEXTURE_ROLE_ANIMATION_EFFECT)` called once, before
  each per-subset loop, at all 4 draw sites. The actual problem was narrower: **OpenGL ES**
  (`shader-opengl_es.cpp`) called the equivalent `bindTextureRoleOpenGlEs(pBufferId, i,
  TEXTURE_ROLE_ANIMATION_EFFECT, ...)` *inside* all 4 per-subset loops (redundantly rebinding the
  identical texture to the same sampler once per subset - wasteful, not incorrect), and both
  backends' generic per-role GET helpers (`getBoundTextureForRoleOpenGlEs`/`getBoundTextureForRoleD3D`)
  carried a `role == TEXTURE_ROLE_ANIMATION_EFFECT ? 0u : subsetIndex` override that duplicated the
  same "force subset 0" logic the call sites already handle explicitly. Fixed by moving the 4
  OpenGL ES bind calls to once-before-each-loop (matching the DirectX9 pattern that already
  existed), and removing the now-redundant ternary from both backends' GET helpers (callers always
  pass subset 0 for FX already, so the override added nothing). Left `BUFFER_GL::getTextureByStage`'s
  own internal "stage>0, missing per-subset entry, fall back to subset 0" logic untouched - it's
  unreachable for FX now (every FX lookup already passes subset 0 directly, so the fallback branch
  is never reached) but may still be load-bearing for NORMAL/SPECULAR/EMISSIVE/MASK roles when a
  specific subset lacks an entry for that role, which is out of this milestone's scope to verify or
  change. This was a code-quality/structural-clarity fix, not a rendering bug fix - the explicit
  `subsetIndex=0` already in place everywhere made the *result* correct before this milestone, just
  via duplicated logic in 3 places instead of one. **User explicitly chose to skip milestone 16 and
  prioritize this one** (raised via `AskUserQuestion` after milestone 16's scope ballooned).
  Verified: clean build (zero warnings); a real FX-equipped sprite (`blast.spt`, pulled from
  `tower-defense`'s git history, 1 animation with a real `transparent.ps` shader effect) converted
  through `mesh_legacy_converter` and loaded through the real runtime path
  (`mbm::SPRITE::load`/`setAnimationByIndex`) via a temporary `testLib`/`DISPLAY=:1` dynamic test -
  loaded OK, animation set OK, no errors during the render loop. Temporary test code and `/tmp`
  fixtures removed afterward.

- **Milestone 17 follow-up (closed 2026-06-27): deleted the dead `textureOverrideStage2` union
  alias.** While reviewing `shader-fx.h` post-milestone-17, found `FX::textureAnimationEffect`/
  `FX::textureOverrideStage2` declared as a union of two identically-typed `TEXTURE*` members, the
  second commented "Legacy alias kept for compatibility." Grepped the whole codebase - zero callers
  of `textureOverrideStage2` anywhere. Since v11 already breaks backward compatibility on purpose,
  keeping a compatibility shim nothing references serves no one; collapsed the union to a single
  plain `TEXTURE *textureAnimationEffect;` member. Verified: clean build, zero warnings.

- **Milestone 18 (new, scoped not started): build real FX/shader-effect editing in the editor.**
  While fixing milestone 17's editor note, found it pointed users to "the Animations node" for
  setting the FX texture - that node (`mesh_debug.lua:4900-5033`) has no such option, never did.
  Wider check found **zero editor UI for the FX/shader-effect system at all**: no shader picker, no
  texture picker, no shader-vars editor, nothing (`grep` for `loadNewShader`/`shader_effect`/
  `effectShader` in `mesh_debug.lua` returns nothing). Today `FX`/`EFFECT_SHADER` (`shader-fx.h`,
  `animation.h`) can only be configured from C++ game code at runtime (`anim->getFx().loadNewShader(...)`,
  direct `textureAnimationEffect = tex` assignment - confirmed call sites in `animation.cpp`,
  `particle.cpp`, `font.cpp`, `steered_particle.cpp`). The editor's Lua binding layer
  (`mesh-debug-lua.cpp`) only *reads* `FX` data to round-trip it on save (`fillEffect`,
  `appendAnimationHeader`) - there's no `setFx`/`loadEffect`/`setAnimEffectTexture` entry in the
  `MESH_MBM_DEBUG` method table (`mesh-debug-lua.cpp:1850-1915`) at all. So a real fix needs two
  layers, not just a UI:
  1. **New Lua bindings** in `mesh-debug-lua.cpp`, following the existing `onSetMaterialTextureNameMeshDebugLua`-
     style pattern (plain `luaL_Reg` C functions), to expose at minimum: load a shader effect
     (`EFFECT_SHADER::loadEffect`, needs shader file + code + `TYPE_ANIMATION` + time), set/get the
     FX texture (`FX::textureAnimationEffect`), set/get shader vars (`VAR_SHADER` min/max table,
     `FX::setVarPShader`/`setMaxVarPShader`/etc.), set blend op (`FX::setBlendOp`/`setBlendDefaultOp`),
     and disable/clear an effect (`ANIMATION::getFx()`-equivalent debug-side access doesn't exist
     yet either - check whether `MESH_MBM_DEBUG` even holds a per-animation `FX` today or whether
     this also needs a debug-side `ANIMATION`/`FX` storage addition).
  2. **New UI** inside the Animations node's per-animation `TreeNodeEx` block
     (`mesh_debug.lua:4965-5010`, alongside name/frames/time/type): shader file picker (ps/vs),
     texture picker for `textureAnimationEffect` (reuse the same browse-button pattern as the
     Texture node), a vars table editor, and a blend-op combo.
  Real feature-sized work (new Lua API surface + new UI), not a cleanup - scoped here as its own
  milestone rather than folded into 17 or attempted ad hoc, per `AskUserQuestion` with the user:
  chose "dead alias now + scope a real milestone" over a quick note-only fix. Pick up as its own
  pass with its own plan/review when prioritized.

- **Milestone 19 (closed 2026-06-28): FX-texture editing in both editor entry points, plus
  `shader_editor.lua` full 6-role texture coverage.** Narrower than milestone 18's broader
  shader-code/vars/blend-op scope (which stays separately backlogged) - this milestone only closes
  the FX *texture* gap, in both places the user wanted it: the Animations node and the Texture
  node's stage-1 branch (which previously dead-ended with a note pointing at a feature that didn't
  exist). Also closed a parallel gap in the separate live shader-preview tool, `shader_editor.lua`,
  which could only ever set one texture role (FX/"stage 2") on its preview mesh.
  - **Debug-class storage** (`mesh-manager.h`/`.cpp`): added `MESH_MBM_DEBUG::getAnimationEffectTexture`/
    `setAnimationEffectTexture(animIndex, fileName)`. Storage already existed
    (`util::INFO_ANIMATION::INFO_HEADER_ANIM::effectShader`, lazily null) and save/load already
    round-tripped it whenever present - this milestone added the first read/write path to it.
    Clearing (`fileName=""`) blanks just the texture field via
    `INFO_FX::setTextureAnimationEffectFileName(nullptr)`, only freeing the whole `effectShader`
    object if no PS/VS shader data is set either - so clearing the FX texture never silently
    destroys a configured shader effect sharing the same slot.
  - **Lua bindings** (`mesh-debug-lua.cpp`): added `getFxTexture`/`setFxTexture(animIndex, fileName)`
    on `MESH_MBM_DEBUG`, same style as the existing `getMaterialTexture`/`setMaterialTexture`.
  - **`mesh_debug.lua` UI**: Animations node now has an FX-texture row (current value, Browse,
    Set, Clear) per animation. The Texture node's stage-1 branch (both the per-mesh copy and the
    "apply to all" copy) now shows a real animation selector + filename/Browse/Set/Clear instead
    of a dead-end note, calling the same `setFxTexture`/`getFxTexture` API - one mechanism, two
    UI entry points, no duplicated logic. The "apply to all" variant uses a plain numeric
    animation-index input (consistent with that panel's existing frame/subset numeric-input style)
    rather than a name combo, since animation names/counts can differ across the multiple meshes
    an "apply to all" operation targets.
  - **Runtime API** (`mesh-manager.h`/`.cpp`, `animation-lua.cpp`, `common-methods-lua.cpp`): added
    `MESH_MBM::getMaterialTexture`/`setMaterialTexture(frame, subset, role, fileName, hasAlpha)` -
    a generic per-(frame,subset,role) runtime setter using `BUFFER_GL::setTextureByStage`/
    `getTextureByStage` directly (NOT built on `ANIMATION_MANAGER::setTexture`, which turned out
    not to be a generic-enough building block - it special-cases stage 0 as "whole animation" and
    any other stage as "just the FX texture field", never calling `setTextureByStage` for
    non-diffuse roles). Clearing (`fileName=""`) now correctly calls `setTextureByStage(nullptr,
    stage, subset)` instead of silently no-op'ing (the pre-existing sibling `MESH_MBM::setTexture`
    has this same silent-no-op gap on empty filename, left as-is since fixing it was out of scope).
    Exposed to Lua as `setMaterialTexture(role, filename[, alpha=true[, subset=0[, frame=current]]])`
    / `getMaterialTexture(role[, subset=0[, frame=current]])` on the runtime renderizable wrapper
    (registered in `regAnimationsMethods`, `common-methods-lua.cpp`), with `role` accepting a
    string ("diffuse"/"normal"/"specular"/"emissive"/"mask"/"fx") or numeric `TEXTURE_ROLE` via a
    small new parser - deliberately NOT reusing the debug-side `parseMaterialTextureSlotType`,
    which parses into a different legacy enum (`util::MATERIAL_TEXTURE_SLOT_TYPE`) with different
    numeric values than `mbm::TEXTURE_ROLE`. The existing numeric `setTexture(filename, alpha,
    stage)` stays untouched for back-compat.
  - **`shader_editor.lua` UI**: replaced the old single-role "Texture Stage 2" panel with a
    "Material Textures" panel covering all 6 roles (diffuse, normal, specular, emissive, mask, fx),
    each with current-texture display + asset-list Set + Clear, all calling the new
    `setMaterialTexture`/`getMaterialTexture` - including migrating the FX row onto the new API
    too (replacing the old asymmetric get-via-`shader:getTextureStage2()`/set-via-
    `mesh:setTexture(file,true,2)` pair) for one consistent call pattern across all 6 roles.
  - **Role-design conclusion** (the user's "revisit the role design" ask): no redesign was made.
    `TEXTURE_ROLE`'s 6 fixed values were confirmed sufficient - the original human+weapon
    per-subset-texturing concern that kicked off this whole design review (session start) is
    already correctly served by the existing per-subset DIFFUSE + material-texture-slot system;
    the only real conflation was FX-via-per-subset-dispatch, already fixed in milestone 17. See
    Open Questions below for the one related gap intentionally left unfixed (dormant
    `SUBSET_EXTRA_SLOT_V11` extensibility).
  - Verified: clean build (zero warnings); Lua syntax-checked all 3 edited `.lua` files; a direct
    C++ round-trip test (temporary, reverted after) via `MESH_MBM_DEBUG` - load a real v11 `.msh`,
    `addAnimation`, `setAnimationEffectTexture`, `saveV11`, reload into a fresh instance,
    `getAnimationEffectTexture` confirmed the texture path survived the round-trip, then
    `setAnimationEffectTexture(idx, "")` confirmed clearing works - all via `testLib`/`DISPLAY=:1`.
  - **Post-close fix (2026-06-28): the runtime `setMaterialTexture('fx', ...)` path (used by
    `shader_editor.lua`'s new Material Textures panel) didn't actually work** - user-reported via
    "I set one image for FX texture however I do not see updated in the editor". Root cause: FX is
    refreshed into `BUFFER_GL`'s per-stage texture map from `mbm::FX::textureAnimationEffect` on
    *every draw call* (`FX::bindTextureAnimationEffect`, `shader-fx.cpp`, called from ~12 per-draw
    sites - this is the exact mechanism milestone 17 made correct and explicit). The original
    `MESH_MBM::setMaterialTexture` wrote straight to `BUFFER_GL::setTextureByStage` for every role
    including FX - for the 4 real per-subset material roles that's correct and persists, but for
    FX it got silently overwritten back to whatever `textureAnimationEffect` already was (still
    null) on the very next render call, so the UI's "set" appeared to do nothing. This was the
    same risk the milestone 19 planning notes had already identified in `ANIMATION_MANAGER::
    setTexture`'s existing FX branch (`anim->getFx().textureAnimationEffect = newTex`, bypassing
    `setTextureByStage` entirely) and consciously decided not to reuse for being "not generic
    enough" - that decision was right for the 4 material roles but wrong to extend to FX without
    a special case. Fixed in `animation-lua.cpp`'s `onSetMaterialTextureAnimationLua`/
    `onGetMaterialTextureAnimationLua`: when `role == TEXTURE_ROLE_ANIMATION_EFFECT`, assign/read
    `anim->getFx().textureAnimationEffect` directly instead of going through `MESH_MBM::
    setMaterialTexture`/`getMaterialTexture` - mirrors `ANIMATION_MANAGER::setTexture`'s
    already-proven FX-branch pattern. The 4 real per-subset roles (normal/specular/emissive/mask)
    and diffuse are unaffected, still routed through `setTextureByStage` as before. Verified via a
    temporary `testLib` test: set `anim->getFx().textureAnimationEffect` the same way the fixed Lua
    binding now does, read it back - confirmed it persists (whereas before the fix, the equivalent
    `setTextureByStage`-based write would have been invisible to any subsequent draw call).

- **Milestone 20 (closed 2026-06-28): wired up SPECULAR/EMISSIVE/MASK runtime texture binding
  across all 4 render backends, and removed legacy `sample0`/`sample1`/`sample2` shader-naming
  support from the live engine entirely.** Triggered by a user question ("if a custom shader uses
  sample0/sample1, will it work?") that surfaced two gaps: (1) `TEXTURE_ROLE_SPECULAR`/`EMISSIVE`/
  `MASK` could be stored via milestone 19's `setMaterialTexture` but no backend actually bound them
  to a GPU sampler - all 4 backends explicitly *rejected* shader compilation if a semantic-named
  shader declared any of these 3 samplers; (2) legacy `sample0/1/2` naming was a fully-supported
  live convention, which the user wants gone per this whole migration's "v11 breaks compatibility
  on purpose, leave the code better" philosophy. Scoped via `AskUserQuestion`: delete the enum
  value completely (not just make it unreachable); bundle the SPECULAR/EMISSIVE/MASK fix into the
  same pass since both touch the same per-backend code; and - after the user corrected an initial
  wrong assumption that `mesh_deprecated` should gain shader-source-rewriting capability - confirmed
  legacy meshes only ever store a shader's *filename*, never its source text, so **no
  `mesh_deprecated` changes were needed or made**; that tool is untouched.
  - **Wiring SPECULAR/EMISSIVE/MASK** (`shader-opengl_es.cpp`, `shader-directx9.cpp`,
    `shader-metal.mm`): each backend's existing DIFFUSE/NORMAL resolve-and-bind pattern was
    extended to the 3 new roles, but only at the draw sites that already bind NORMAL (not every
    DIFFUSE site - NORMAL is never bound at particle draw sites in any backend, so SPECULAR/
    EMISSIVE/MASK follow that same footprint, not a broader one):
    - GLES: added `samplerHandle3/4/5` next to the existing 0/1/2, resolved at compile time the
      same way; bind calls added at `render()`'s 2 per-subset branches + `renderDynamic()`'s 2 -
      4 sites total.
    - DirectX9: **no new handle fields needed** - investigation found `samplerHandle0/1/2` are
      resolved at compile time but never actually consumed by `bindTextureRoleD3D` (HLSL's static
      `register(sN)` binding model needs no existence check, unlike GLSL's uniform-location
      lookup) - they're vestigial, pre-existing dead storage, left as-is (out of scope to clean up
      now). Just added `bindTextureRoleD3D(..., TEXTURE_ROLE_SPECULAR/EMISSIVE/MASK)` calls.
      Site count corrected during implementation from an initial (wrong) plan estimate of 2: D3D9
      actually binds NORMAL at **4** sites - `render()`'s 2 branches AND both `renderParticle`
      overloads (`renderDynamic` itself has no bind loop of its own; it delegates via `return
      render(...)` and inherits the fix for free) - so SPECULAR/EMISSIVE/MASK went to all 4.
    - Metal: has no resolve-then-bind handle concept at all - hardcodes fixed `atIndex:` values
      per role with no shader-declares-this-role gating. Added 3 more unconditional
      `setFragmentTexture:...atIndex:3/4/5` calls at the same 4 sites GLES uses. Also fixed a
      leftover redundant `role == ANIMATION_EFFECT ? 0 : subsetIndex` ternary in
      `getBoundTextureForRoleMetal` (dead - the FX call site already explicitly passes
      `subsetIndex=0`) - the same pattern milestone 17 cleaned up in GLES/D3D9, just missed there
      since Metal wasn't in that milestone's research scope.
    - Dummy backend: no real per-subset binding exists for *any* role (its render functions are
      all no-op stubs) - nothing to add beyond removing its rejection guard.
  - **Removed the now-obsolete rejection guards** in all 4 backends ("...declares a reserved
    semantic texture role without runtime binding support") - the gap they guarded against no
    longer exists.
  - **Extended `TEXTURE_MANAGER::getFallbackTexture`** (`texture-manager.cpp`) to cover SPECULAR/
    EMISSIVE/MASK (previously `nullptr` for these 3 - only DIFFUSE/ANIMATION_EFFECT/NORMAL had
    fallbacks). Found during plan review: without this, a subset whose shader declares e.g.
    `TextureSpecular` but has no specular texture actually assigned (a normal, expected case) would
    pass `nullptr` into Metal's `setFragmentTexture:nil atIndex:N` for a slot the shader actively
    samples from - a real API validation risk in debug builds, not just a visual quirk (GLES/D3D9
    null-guard gracefully, Metal doesn't). Added black fallbacks for specular/emissive, opaque
    white for mask, via the engine's existing `"#AARRGGBB"` synthetic-color-texture convention.
  - **Removed `SHADER_TEXTURE_NAMING_LEGACY_SAMPLE` from the enum** (`shader.h`) and all its case
    sites in `shader.cpp` (`getTextureRoleShaderName`, `parseShaderTextureNaming`,
    `getShaderTextureNamingName`, `detectShaderTextureNamingProfile`). Confirmed via grep these
    were the *only* 2 files referencing the enum value by name - no backend or `shader-cfg.cpp`
    referenced it directly, they all go through the naming-detection/lookup functions generically,
    so deleting it from one place cut off legacy support engine-wide. A shader using only
    `sample0`/`sample1`/`sample2` now detects as `SHADER_TEXTURE_NAMING_NONE` (no role resolved/
    bound, not an error) rather than a recognized-but-deprecated convention.
  - **Found and removed additional now-dead code while implementing** (not in the original plan,
    surfaced once `detectShaderTextureNamingProfile` could no longer return `MIXED_INVALID` at
    all): the `MIXED_INVALID` rejection branch in `BASE_SHADER::loadShader` (`shader.cpp`), in
    `SHADER_CFG::validateTextureNamingProfile` (`shader-cfg.cpp`), and in all 4 backends'
    `compileShader` - all 6 were fed directly by `detectShaderTextureNamingProfile`'s return value,
    which can now only ever be `NONE` or `SEMANTIC_ROLE`, making these checks unreachable rather
    than merely "misleadingly worded" as initially assumed. Deleted rather than reworded. Also
    deleted `mergeShaderTextureNamingProfiles` (`shader.cpp`/`shader.h`) - confirmed zero callers
    anywhere in the codebase, genuinely dead. `MIXED_INVALID` itself stays in the enum - still
    reachable via `parseShaderTextureNaming`'s fallthrough for an unrecognized CFG `textureNaming`
    string, whose existing error message was already naming-neutral (never said "legacy"), so no
    rewording was needed there. In the Dummy backend specifically, removing the naming check also
    orphaned its entire surrounding default-shader-source setup block (computed only to feed that
    check) - removed that too, down to the bare `pShader`/`vShader` assignment + `REMINDER_TODO`.
  - Verified: clean build (zero warnings) of every backend this Linux dev environment compiles
    (GLES, DirectX9 - via its stub path, Dummy; Metal's `.mm` is Apple-gated out of this build,
    verified by careful code review/context-matching instead of compilation). Real `testLib`/
    `DISPLAY=:1` test (temporary, reverted after): a `SHAPE_MESH` quad with a custom shader
    declaring `TextureSpecular`/`TextureEmissive`/`TextureMask` samplers compiled successfully (no
    longer rejected); `setMaterialTexture`/`getMaterialTexture` round-tripped for all 3 roles;
    `detectShaderTextureNamingProfile` on legacy-only (`sample0`) source returned `NONE`, not an
    error; `getFallbackTexture` returned non-null for all 3 roles.

- **Milestone 21 (closed 2026-06-29): removed dead legacy-version branches from the live v11
  engine; relocated `util::HEADER`/`EXTRA_HEADER` and the old `*_VERSION_MBM_HEADER` constants to
  `mesh_deprecated`.** Triggered by the user reviewing `MESH_MBM_DEBUG::loadV11` and noticing it
  still threaded a `util::HEADER headerMain` (the old v1-v10 in-memory header) alongside the real
  `util::FILE_HEADER_V11`, and still branched on old version constants
  (`DETAIL_MESH_VERSION_MBM_HEADER`, `MODE_DRAW_VERSION_MBM_HEADER`,
  `MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER`). Investigation confirmed `core_mbm`'s only load path
  (`loadV11`/`parse_v11_intermediate`) never reads a real `util::HEADER` from disk - both
  synthesized a fake one hardcoded to `LEGACY_HEADER_VERSION` (10, the old max), making every
  comparison against it a compile-time-constant in practice: `headerMain.version ==
  DETAIL_MESH_VERSION_MBM_HEADER` (10==3) always false, `fileVersion >= MODE_DRAW_VERSION_MBM_HEADER`
  (10>=5) always true, `impl->headerMain.version >= MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER`
  (10>=9) always true. Verified the one loose thread (whether `release()`'s memset-to-0 of
  `headerMain` could ever reach a `saveV11` while still zeroed) - every `release()` call site is
  immediately followed, same function, by either destruction or the version being re-set; no live
  path leaves it stale.
  - **Collapsed the dead branches** in `mesh-manager.cpp`: `read_detail_mesh_section`'s
    `DETAIL_MESH_VERSION_MBM_HEADER`-gated preamble-skip and `'H'`-type check (both unreachable -
    dropped the now-unused `headerMain` parameter too); the `else`/`readTriangleLegacyNoPosV8`
    branch in `MESH_MBM_DEBUG::readDebugTriangleDetailCompat`, the free-function equivalent
    `read_triangle_detail_v11`, and `MESH_MBM::readTriangleDetailCompat` (the last of which turned
    out to already have **zero callers** - `MESH_MBM::loadV11` fully delegates to
    `parse_v11_intermediate`/`finishLoadFromIntermediate`, which use the free-function path instead
    - deleted outright as a pre-existing, unrelated dead-code find); the
    `MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER` save-path gate around populating a subset's
    `extraSlots` (now unconditional, since v11 always supports material texture slots).
  - **Replaced `impl->headerMain` (a full `util::HEADER`)** in `MESH_MBM_DEBUG::Impl`
    (`mesh-manager-impl.h`) with just the 2 fields it ever read (`backBufferWidth`,
    `backBufferHeight`) plus a new `formatVersion` field. Found in the process: `.name`/`.typeApp`/
    `.magic`/`.reserved`/`.extraHeader` were write-only everywhere in `core_mbm` - never read back -
    pure historical cruft; their only producers (`get_type_app_from_mesh_type`,
    `MBM_HEADER_NAME_MBM` writes) were deleted too once orphaned.
  - **Fixed a real (small) bug**: `MESH_MBM_DEBUG::getFileVersion()` (shown in the editor as the
    "Loaded version" row, `mesh_debug.lua`) used to always return the hardcoded `10` for every v11
    mesh, never the real file's format version. Per the user's direction, it now returns
    `impl->formatVersion`, populated from the real `FILE_HEADER_V11::formatVersion` on `loadV11`
    and defaulting to `MBM_V11_FORMAT_VERSION` (already `11`, no new constant needed) for
    not-yet-saved/in-memory meshes (`loadDebugFromMemory`, fresh construction, `release()`) - "the
    latest version while creating/editing it via mesh_debug," per the user.
  - **Relocated `util::HEADER`, `EXTRA_HEADER`, the 10 `*_VERSION_MBM_HEADER` defines,
    `LEGACY_HEADER_VERSION`, `MBM_DEPRECATED_DETAIL_TYPE_SCRIPT`/`_SHADER`, and
    `MBM_HEADER_NAME_MBM`/`MBM_TYPE_APP_*`/the two compare-length defines** from the shared
    `include/core_mbm/header-mesh.h` into `src/mesh_deprecated/mesh-v8-io-legacy.h`/`.cpp` -
    confirmed via grep these had zero remaining `core_mbm` references after the above, and are
    genuinely still needed by `mesh_deprecated`, which reads a *real* `util::HEADER` off disk and
    branches on real, varying v8-v10 file versions (legitimate, untouched logic). Dropped `API_IMPL`
    from `HEADER`/`EXTRA_HEADER` (matching this header's existing no-export convention - the whole
    module is internal-only) and the dead, zero-caller `HEADER(const char*, int32_t)` overload
    found along the way. Same precedent this header already documents for
    `header-mesh-legacy-disk.h`'s structs (other legacy on-disk types relocated for the same
    reason, milestone 5).
  - Verified: clean build of `core_mbm`, `mesh_deprecated`, `mesh_legacy_converter`, and the full
    project (zero warnings). Real `testLib` test (temporary, reverted after): a fresh
    `MESH_MBM_DEBUG` reports `getFileVersion() == 11`; loading the real v11 fixture `Crate.msh`
    (inspected its raw bytes directly - `formatVersion` byte is `0x0b` = 11) also reports `11`.
    Ran `mesh_legacy_converter` on the real legacy fixture `Crate_old.msh` end-to-end - converted
    successfully to a valid v11 file, confirming the relocated `mesh_deprecated` code path still
    works.

## Open Questions

(Resolved for milestone 0: `TEXTURE_ROLE` header location — see Scope Decision 4. Remaining items
below belong to later milestones.)

- Default per-blob compression for new saves from Mesh Debug/Sprite Maker: on or off by default?
- Thread pool lifetime/ownership for milestone 6: private to `MESH_MANAGER::Impl`, or a small shared
  engine utility other systems (textures, audio) could reuse later without turning it into a full job
  system today?
- Should `MESH_MBM`/`MESH_MBM_DEBUG` class names change along with the format, or stay as-is since
  `.msh`/`.spt`/`.fnt`/`.ptl`/`.tile` already carry the type distinction at the extension level?
- `SUBSET_EXTRA_SLOT_V11` already stores an arbitrary role byte per subset, fully wired through
  v11 save/load - but `getTextureRoleShaderName`/`getTextureRoleBackendSlot` (`shader.cpp`) are
  hardcoded switches over the 6 known `TEXTURE_ROLE` values, so that storage-level extensibility
  is dormant: nothing can actually consume a 7th role at runtime today. No concrete need for one
  has come up (milestone 19's role-design review confirmed the 6 existing roles are sufficient) -
  backlog only; generalize the dispatch if/when a real 7th-role need shows up.
- A pre-existing, unrelated live-mesh-to-debug-class conversion path (`mesh-manager.cpp:~1721-1766`)
  unconditionally rebuilds `effectShader` gated only on `getCurrentShader()` being set, which would
  drop a texture-only FX (no shader code) if that conversion ever ran after milestone 19's new
  FX-texture setter populated one. Confirmed milestone 19's own UI never reaches this path (its
  `setFxTexture`/`getFxTexture` calls go straight to `MESH_MBM_DEBUG`, not through this conversion
  routine), so it's not a regression from this milestone - flagged here in case some other,
  not-yet-found Lua entry point reaches it.
- Milestone 20 removed legacy `sample0`/`sample1`/`sample2` shader-naming support from the live
  engine with no migration tooling - by design (the user explicitly chose this; `mesh_deprecated`
  only ever carries a referenced shader's filename, never its source text, so there was nothing to
  build a translator on top of). The handful of real legacy-named shader files known to exist (a
  personal library outside any repo, not the shipped game) are now unsupported until manually
  hand-edited to semantic naming - a few lines of work if/when actually needed, intentionally not
  automated.
