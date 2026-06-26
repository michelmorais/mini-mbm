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
   `SECTION_EXTRA_PATHS` — covers 3D/sprite meshes fully. Still open within this milestone:
   `SECTION_ANIMATION`+FX and the `SECTION_DETAIL_FONT`/`PARTICLE`/`TILE` payloads; `saveV11`
   explicitly rejects animated and FONT/PARTICLE/TILE_MAP meshes until that follow-up pass lands.
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
   (writing `mesh_deprecated::convertLegacyMeshToV11`'s actual v1-v10 parsing body, currently a
   not-implemented placeholder) is deferred** - it's genuinely new code (populating a
   `MESH_MBM_DEBUG` via its public API, since the old `impl->` access isn't visible outside
   `core_mbm`), not a move, and deserves its own focused pass.
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
7. Built-in shader resource cleanup: convert `shader-resource-opengl_es.cpp` /
   `-directx9.cpp` / `-metal.mm` built-ins to semantic `TEXTURE_ROLE` naming by default; keep legacy
   `sample0`/`sample1`/`sample2` only as the documented legacy naming profile for old custom shaders.
8. Per-blob compression (`DEFLATE`) for vertex/index/texture blobs as an opt-in save-time setting,
   fully replacing mandatory whole-file compression.
9. *(Reserved, not implemented)* skinned-frame block + GPU skinning — only after real demand is
   confirmed, not speculatively.

## Open Questions

(Resolved for milestone 0: `TEXTURE_ROLE` header location — see Scope Decision 4. Remaining items
below belong to later milestones.)

- Default per-blob compression for new saves from Mesh Debug/Sprite Maker: on or off by default?
- Thread pool lifetime/ownership for milestone 6: private to `MESH_MANAGER::Impl`, or a small shared
  engine utility other systems (textures, audio) could reuse later without turning it into a full job
  system today?
- Should `MESH_MBM`/`MESH_MBM_DEBUG` class names change along with the format, or stay as-is since
  `.msh`/`.spt`/`.fnt`/`.ptl`/`.tile` already carry the type distinction at the extension level?
