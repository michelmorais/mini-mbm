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

0. Lock the v11 binary layout (section/TLV table, texture-role encoding, index-width flag, reserved
   skinned-frame block id) — design review before any code lands. Draft layout proposal now exists
   in `docs/mesh-v11-format.md` (v0, pending review/sign-off, not yet implemented).
1. v11 section read/write helpers, reusing the `mesh-v8-io.cpp` little-endian primitive style.
2. `MESH_MBM` / `MESH_MBM_DEBUG` PIMPL split: `mesh-manager.h` stops transitively including
   `header-mesh.h`'s disk structs; only runtime-facing types stay public.
3. v11 writer (Mesh Debug / Sprite Maker save path) — the single source of truth for producing v11
   files.
4. v11 reader, synchronous path first (fully replaces the compressed-whole-file path for v11 files).
5. `mesh_deprecated` lib: extract the existing v1-v10 read code out of `core_mbm`; wire it as a
   standalone converter that calls the milestone-3 writer.
6. Async loading: background parse phase + main-thread GPU finish phase +
   `MESH_MANAGER::loadAsync`/`pumpAsyncLoads`.
7. Built-in shader resource cleanup: convert `shader-resource-opengl_es.cpp` /
   `-directx9.cpp` / `-metal.mm` built-ins to semantic `TEXTURE_ROLE` naming by default; keep legacy
   `sample0`/`sample1`/`sample2` only as the documented legacy naming profile for old custom shaders.
8. Per-blob compression (`DEFLATE`) for vertex/index/texture blobs as an opt-in save-time setting,
   fully replacing mandatory whole-file compression.
9. *(Reserved, not implemented)* skinned-frame block + GPU skinning — only after real demand is
   confirmed, not speculatively.

## Open Questions

- Where should the v11-to-`TEXTURE_ROLE` on-disk integer mapping live: in `header-mesh.h` referencing
  `shader.h`'s enum directly, or in a new small shared header that neither file owns exclusively?
- Default per-blob compression for new saves from Mesh Debug/Sprite Maker: on or off by default?
- Thread pool lifetime/ownership for milestone 6: private to `MESH_MANAGER::Impl`, or a small shared
  engine utility other systems (textures, audio) could reuse later without turning it into a full job
  system today?
- Should `MESH_MBM`/`MESH_MBM_DEBUG` class names change along with the format, or stay as-is since
  `.msh`/`.spt`/`.fnt`/`.ptl`/`.tile` already carry the type distinction at the extension level?
