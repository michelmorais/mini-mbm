# Core MBM PIMPL and Interface Boundaries

This document describes ownership and header boundaries in `core_mbm`. Backend/OS
isolation and strict ABI isolation are distinct: backend implementation storage is
private, while some gameplay values, authoring records, and serialized structures
remain intentionally visible. The engine does not require every value type to use
PIMPL.

## Backend and runtime ownership

Concrete backend layouts belong in `src/core_mbm/private/` or backend translation
units. Public bridge headers use forward declarations or narrow backend-neutral
operations rather than exposing SDK layouts.

| Surface | Ownership boundary |
|---|---|
| `TEXTURE`, `BUFFER_GL`, `SHADER` | Backend handles and implementation storage reside in private `BackendData` |
| `RENDERIZABLE_TO_TARGET` | Render-target backend state resides in private data |
| `DEVICE` | Platform context and accessor-backed runtime state reside in `Impl` |
| `TEXTURE_MANAGER`, `MESH_MANAGER` | Manager caches and release bookkeeping remain private |
| Animation, scene, and core-manager classes | Runtime implementation state is kept behind their private boundaries |
| `RENDERIZABLE`, `RENDER_2_TEXTURE`, `HMD` | Base runtime, render-target, and right-eye buffer state remain private |
| `TEXTURE_VIEW`, `GIF_VIEW`, `BACKGROUND` | Private runtime state resides in `Impl` |

Public APIs may expose narrow getters/setters and engine-owned value records.
For example, `RENDERIZABLE::alwaysOnTopPriority` has accessor methods with storage
in `Impl`; font glyph queries use `FONT_GLYPH_QUAD`, not `stbtt_aligned_quad`.

## Normal mapping: preparation, asset data and rendering

`src/core_mbm/private/normal-map-preparation.*` owns the CPU-only tangent preparation
contract. Input geometry, per-corner imported tangents, MikkTSpace callbacks and
prepared batches are private types. Each prepared vertex retains its source index;
batches use local 16-bit triangle indices without changing the source geometry.
Preparation does not query device state or allocate GPU resources.

`normal-map-asset.*` owns section serialization, source signatures and validation.
Prepared frames reside in the runtime and authoring `Impl`s and the CPU-only async
load intermediate. Loading and saving reuse valid persisted bases or prepare
missing ones when requested by a normal texture or explicit precomputation.
Automatic runtime preparation requires `USE_NORMAL_MAPPING_3D=1`; explicit
authoring/save preparation remains available in either build.
Private friend bridges copy prepared data during runtime-to-authoring extraction.
Frame copies/removals retain or reindex records; subset restructuring invalidates
them. Source changes are checked before saving.

Optional normal-map material settings reside in the same private asset state,
keyed by source frame/subset. `getNormalMapSettings` and `setNormalMapSettings`
expose validated scalar values, not containers. Defaults are +Y (`greenSign=1`)
and strength 1. Section 15 stores overrides independently of tangent data and
texture assignments. Changing settings does not invalidate tangents. Runtime
mutation follows the shared-asset material contract; extraction and editing
preserve associations, and merging incompatible settings fails without mutation.

`normal-map-upload.h` declares backend-neutral main-thread upload and settings
hooks. Static OpenGL ES, DirectX 9 SM3, DirectX 11 and Metal rendering keep
derived GPU buffers and draw settings in private `BUFFER_SPECIFIC::normalMapSubsets`.
`BUFFER_GL::BackendData` privately owns CPU staging for prepared static frames.
The first supported 3D draw with a texture, retained basis and nonzero strength
uploads the frame's derived batches; success frees staging geometry and its shared
preparation reference. Since 7.328, `MESH_MBM::Impl` owns an immutable shared
`ASSET_FRAMES`; staging aliases its frame preparation without duplicating batches.
The shared owner keeps pending data alive independently of the caller. Upload
failure retains the reference for retry; invalidation/release drops it. Runtime
extraction copies the map into mutable authoring state, preserving edit isolation. Frames never used with normal mapping retain staging
instead of GPU allocations. Small private per-subset settings and an active count avoid scanning
geometry or subsets for the no-map draw path. Texture/settings changes update
activity, including the legacy subset-zero texture fallback.
`SHADER::BackendData` owns a lazy mapped variant; the initial shader is geometric.
GLES/Metal cache keys include variant identity (DX9 already did); custom shader
source, skinning and 2dw keep their contracts. The private compile/render methods
hide variant selection without adding public flags, containers or handles.
After first use, GPU batches are retained until buffer release or invalidation so
texture removal/reassignment does not thrash allocations. Buffer release frees this
storage; dynamic source updates discard it. `normal-map-gles.h` contains the GLES
shader helper; `normal-map-hlsl.h` supplies the DirectX 11 fragment helper.
DirectX 11 owns derived vertex/index buffers per subset, releases partial uploads
through a temporary private owner, and preserves the source vertex layout for
readback. DirectX 9 likewise owns managed derived vertex/index buffers per subset;
`normal-map-hlsl9.h` supplies its SM3 fragment helper. Its private shader owns the
tangent declaration and constant zero tangent buffer. Source managed buffers are
readable for static authoring extraction; derived buffers stay write-only. Dynamic
write-only source extraction is explicitly rejected. Metal owns shared-storage
derived vertex, tangent and index buffers in the private backend structure,
publishing them only after all allocations succeed. Source
buffers remain independent for authoring readback. `normal-map-metal.h` supplies
MSL basis reconstruction; tangent reads use vertex buffer 20 and settings use
vertex/fragment buffer 21, outside lighting 4-18 and skeletal palette 19.
Source draws bind disabled settings and a zero tangent; no tangent reads occur.
Dynamic source updates discard the derived batches.
No backend handles or owned containers are exposed through public headers.

The rendering details above apply when `USE_NORMAL_MAPPING_3D=1` (default).
With `0`, private backend tangent storage, upload hooks and normal-map draw paths
are compiled out, and automatic preparation in the runtime loader is skipped.
The asset `Impl` still preserves and validates sections 14/15, and explicit authoring
preparation remains available. `core_mbm/render-features.h` exposes only the numeric
build contract and the read-only `isNormalMapping3DCompiled()` engine query; it
adds no mutable state or backend handles to public interfaces.

`normal-map-asset.*` also provides the private CPU `remapSkinWeights` bridge. It
collects canonical influences in each tangent batch's source-vertex order,
retaining skeleton identity, frame and bone palette. Its transient result is
published atomically without changing source weights or adding a serialized
section. Render backends do not yet consume these remapped batches for skeletal
normal mapping. The follow-up contract is tracked in
[Future Features](future-features.md#normal-mapping).

`MESH_MBM_DEBUG::hasNormalMapTangents()` exposes only constant-time
presence of retained preparation, without signature validation or regeneration.
It does not expose containers or introduce public mutable storage.

`MESH_MBM_DEBUG::removeNormalMap()` clears the private tangent/material caches and
normal texture slots across the asset, refusing mutation during simplification.
It introduces no storage or container exposure in the public API.

`MESH_MBM_DEBUG::prepareNormalMap` exposes an authoring operation without exposing
the cached representation. `NORMAL_MAP_CORNER` is borrowed input and
`NORMAL_MAP_REPORT` a copied result. Preparation publishes a validated candidate
atomically, preserves other valid subset bases and refuses to run while the
simplification worker is active. Lua forwards per-corner data to the same CPU
operation. Editor controls invoke preparation on explicit actions or after
geometry generation/simplification.

## Isolated editor previews

`MESH_MANAGER::loadUncached` returns an owned `std::unique_ptr<MESH_MBM>` without
registering the asset in the shared cache. It exposes ownership, not the cache
container or implementation storage. Ordinary runtime loading uses the shared cache.

`SPRITE_EDITOR_ACCESS` and `MESH_EDITOR_ACCESS`, declared in private render headers,
provide narrow friend bridges for `meshDebug:loadSpritePreview` and
`meshDebug:loadMeshPreview`. The renderizable privately owns its uncached asset.
Mesh initialization is shared by cached and uncached loading, including animation
and skeletal setup. Device restoration uses the corresponding private bridge.
These operations do not add public mutable cache accessors or backend handles.

## Skeletal and articulated animation

Cached meshes own validated canonical asset data: skeleton records and compiled
hierarchy, stable-ID skin palettes and influences, clips, tracks, keys, and lookup
structures. This data resides in `MESH_MBM::Impl` or `MESH_MBM_DEBUG::Impl`.
Public queries return scalar values or copy fixed records rather than exposing
vectors, maps, or mutable references.

The optional V11 autoplay target (kind and animation name) stays in those same
asset PIMPLs. Mesh Debug changes it through a validating setter; runtime
renderables read it through narrow getters and keep their playback state per
instance.

Each renderizable's animation manager owns its playback state. Opaque
`SKELETAL_ANIMATION_PLAYER` and `ARTICULATED_ANIMATION_PLAYER` implementations keep
clip selection, time, pause, blending, masks, evaluated transforms, palettes, and
root-motion history separate from cached assets. Instances sharing an asset do
not share mutable playback state implicitly.

Pose inspection and named-bone queries copy a bone's identity, transform, or TRS
into caller-owned results. Root motion keeps pose history and discontinuity
invalidation private and copies the requested delta. Explicit pose sharing borrows
an instance's private palette during drawing without exposing it as public storage.

Backend-neutral skeletal preparation caches and GPU upload bridges are private.
Bone-index/weight streams, palette readiness, selected LBS/DQS method, and GPU/CPU
execution policy do not expose backend buffers. Public reports contain scalar
policy/status/count values. Draw calls use transient palette inputs; backend
attribute/uniform handles, shader cache identity, and per-subset buffers remain
in backend-specific storage. Private parity-test bridges do not add public APIs.

## Image-based mesh authoring

[`image-mesh.h`](../include/core_mbm/image-mesh.h) exposes CPU authoring operations
through `IMAGE_MESH_OPTIONS`, `IMAGE_MESH_REPORT`, enums, and value records.
Requests contain scalar settings and borrowed immutable paths/point/brush/area
spans, not owned STL containers or runtime render state. Callers must keep borrowed
data alive for the operation.

Decoded pixels, floating-point height fields, painting snapshots, manual-area
rasterization, topology validation, triangulation, refinement, normals, UV seams,
and temporary geometry remain generator-local or in private image-mesh helpers.
Contour queries write to caller-provided buffers. Height-channel selection, tonal
curves, separate height images, back materials, and side mapping follow this same
request/result boundary.

The optional progress callback and context are borrowed, run on the generating
thread, and must remain valid for that invocation. Returning false requests
cancellation. Lua's asynchronous binding owns a snapshot of the request, including
its paths and arrays, alongside the CPU result, progress atomics, and worker in
private job state. Workers do not call Lua; owners join them before destruction.
Diagnostic-map generation uses the same ownership pattern. Graphics resources are
created on the main thread.

## Simplification

The editor simplification worker, progress, state, report, error, and cancellation
arbitration reside in `MESH_MBM_DEBUG::Impl`. Public operations start, query,
cancel, and collect work without exposing the worker or mutable state.
QEM options and report fields are scalar values. The editor's CGAL adapter
communicates with a separately configured executable using temporary OBJ files;
no CGAL library or process state is exposed through engine headers.

Prepared geometry buffers retain local ownership until commit. The cancellation
and commit gate ensures that an accepted native cancellation precedes replacement
of source buffers or canonical weights. Editor workflows can additionally discard
a completed but unpublished working copy. Neither path requires public containers
or backend accessors.

## Intentionally visible interfaces

These surfaces serve compatibility or data-model roles rather than representing
backend-layout leaks:

| Surface | Contract and maintenance rule |
|---|---|
| `CAMERA_TARGET` | Gameplay/Lua-facing value fields remain available alongside accessors. Hiding them requires an explicit compatibility decision. |
| `TEXTURE` alpha property | A simple asset property retains compatibility storage. Do not confuse it with backend handles. |
| `TEXT_DRAW` / `FONT_DRAW` | Direct layout/render working state and bounds/restore context require a deliberate font/layout redesign before broader encapsulation changes. |
| `MESH_MBM` physics readers | Parser-local/shared-reader logic is a loader/serializer concern, not a reason to expose new object storage. |
| `BUFFER_MESH` | Aggregate builder/load/save and backend extraction uses require coordinated API design rather than accessor proliferation. |
| `MESH_MBM_DEBUG` | Authoring/editing payload contracts are distinct from private worker and runtime state. Broader changes require an editing/storage design. |
| `PLUGIN::onSubscribe(void *context, void *renderDevice)` | Opaque handles are part of the plugin ABI; changing them requires a versioned plugin-interface design. |
| File-format records such as `header-mesh.h` | Serialized data contracts remain public. Change them through file-format design, not mechanical PIMPL conversion. |

## Rules for core changes

When modifying `src/core_mbm/` or `include/core_mbm/`:

- Keep new implementation state in `Impl`, `BackendData`, or private helpers.
- Do not expose backend SDK layouts, owned handles, cache containers, or mutable
  implementation references through public headers.
- Prefer operations and copy-out value reports to storage accessors.
- Keep asset data separate from per-instance playback and editor working copies.
- If the same accessor-backed object is used repeatedly in a function, obtain one
  local reference for that scope. Do not turn it into persistent cached state
  without an explicit lifetime design.
- Add helpers only when they reduce actual coupling. Preserve source compatibility
  unless a change is intentional and documented.
- Update this document when an ownership or public-interface boundary changes.

Revisit a boundary when a concrete backend type leaks into a public header, a
specific compatibility/ABI requirement calls for it, or an explicit subsystem
redesign needs it. Strict PIMPL alone is not a reason to redesign gameplay values,
file-format records, or authoring APIs.
