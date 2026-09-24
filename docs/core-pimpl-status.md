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
arbitration remain in `MESH_MBM_DEBUG::Impl`. Public operations start, query, cancel,
and collect work without exposing the worker or mutable state. The 7.280.0
`MESH_SIMPLIFY_MODE` and added report fields are scalar values. Planar adjacency,
corner attributes, region certificates and scratch candidates live in the private
`mesh-planar.h` helper; the optional prepass shares the same final commit gate.
Image Mesh's curved mode and planar count also remain scalar options/report data.
The 7.284.0 boundary option and counters are scalar; coordinated certificates,
position-alias contacts and masks remain temporary private helper state.

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
