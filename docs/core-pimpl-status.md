# Core MBM PIMPL Status

Updated: 2026-08-18

This document replaces the old milestone-style gap report. Its purpose is to describe the current PIMPL/header-hygiene status of `core_mbm`, the boundaries already established, and the rules for future work.

## Scope

There were two different goals in this branch:

1. Backend/OS isolation.
2. Optional strict-PIMPL / ABI / header-hygiene follow-up.

Those are no longer the same thing.

## Current Status

Backend/OS PIMPL is complete for the intended public engine boundary.

Public headers no longer expose normal class storage for concrete DirectX9, OpenGL ES, Metal, Win32, macOS, Android, or dummy backend implementation layouts. Backend-owned render handles and platform context storage now live behind `Impl`, `BackendData`, or private backend headers.

Selected ABI/header-hygiene cleanup is also complete for the main core/base surfaces that were worth migrating without redesigning the engine.

The remaining work is optional. It is not needed to claim backend/OS PIMPL success.

## Completed Boundaries

### Backend-owned state

- `TEXTURE` backend handle storage is hidden behind `BackendData`.
- `BUFFER_GL` backend-specific storage is hidden behind `BackendData`.
- `SHADER` backend-specific storage is hidden behind `BackendData`.
- `RENDERIZABLE_TO_TARGET` backend-specific storage is hidden behind private data.
- `DEVICE::specificContextDevice` is hidden behind `DEVICE::Impl`.

### Private backend headers

- Concrete backend layouts moved out of the public header surface and into `src/core_mbm/private/`.
- Public backend bridge headers now expose only forward declarations or narrow bridge APIs where needed.

### Core/base class layout cleanup

The following areas are already in the "treat as complete unless a bug/regression appears" state:

- `TEXTURE_MANAGER`
- `MESH_MANAGER` manager cache/fake-release state
- `ANIMATION_BACKUP`
- `EFFECT_SHADER`
- `ANIMATION_MANAGER`
- `ANIMATION`
- `SCENE`
- `CORE_MANAGER`
- `RENDERIZABLE`
- `RENDER_2_TEXTURE`
- `HMD` right-eye buffer storage
- `DEVICE` main accessor-backed state

`RENDERIZABLE::alwaysOnTopPriority` follows this completed boundary: its integer storage remains in
`RENDERIZABLE::Impl`; the public header exposes only narrow getter/setter methods.

### Low-risk render/header follow-up already done

- `TEXTURE_VIEW`, `GIF_VIEW`, and `BACKGROUND` private runtime state moved behind `Impl`.
- `CAMERA_TARGET` accessor prep is done.
- Main `CAMERA` projection/clip accessor prep is done, including 2D near/far accessors.
- `DEVICE` light runtime helper hygiene is done without reopening the public light API.
- Texture TTF public API no longer leaks `stbtt_aligned_quad`; it uses engine-owned `FONT_GLYPH_QUAD`.

## What This Document Says Is Still Open

Only the areas below remain intentionally visible or intentionally deferred.

| Area | Current status | Why it remains | Rule for future work |
|---|---|---|---|
| `CAMERA_TARGET` | Accessor prep is complete; direct value fields still exist. | It is a gameplay/Lua-facing value object, not backend leakage. | Only move storage behind private data if a breaking compatibility decision is explicitly accepted. |
| `TEXTURE` alpha property | Accessors exist; compatibility storage remains public. | This is a simple asset property, not backend leakage. | Only revisit if there is a deliberate compatibility decision to hide the storage too. |
| `TEXT_DRAW` / `FONT_DRAW` | Narrow accessor-prep is complete; storage remains direct. | Remaining state is layout/render working state or compatibility-facing bounds/restore context. | Do not continue with helper-only cleanup. Revisit only with an explicit font/layout or compatibility redesign. |
| `MESH_MBM` | Useful metadata and owned-physics helper slices are complete. | Remaining direct physics work is parser-local/shared-reader logic, not a clean object-layout target. | Stop here unless shared loader/serializer API redesign becomes an explicit goal. |
| `BUFFER_MESH` | Read-helper prep is complete; storage remains direct. | Remaining direct use is owner-side aggregate runtime builder/load/save behavior and backend-specific extraction/serialization. | Do not add more helper churn. Revisit only with a broader runtime-mesh builder/backend-debug-extraction redesign. |
| `MESH_MBM_DEBUG` | Lua/helper-prep slices are complete; storage remains direct. Its editor-only simplification worker, atomic progress/state, report, and error remain private in `Impl`. | Remaining direct use is debug-tool behavior, editing/storage mutation, and compatibility-facing payload/header state. | Revisit only with a broader debug-mesh editing/storage redesign. |
| `PLUGIN::onSubscribe(void *context, void *renderDevice)` | Still uses opaque backend/platform handles. | This is a plugin ABI/design issue, not current backend-header leakage. | Revisit only if plugin ABI versioning/stable wrapper design becomes a formal goal. |
| File-format structs such as `header-mesh.h` | Still public. | They describe serialized data and asset compatibility. | Do not PIMPL first. Revisit only during a file-format redesign. |

`MESH_MBM::getTotalArticulatedAnimations()` and `getArticulatedAnimationName()` are narrow,
read-only queries over existing `Impl`-owned clip metadata. They expose neither the clip container
nor mutable storage and therefore preserve the completed PIMPL boundary.

Runtime skeletal playback state also remains private. Automatic root-motion selection, raw pose
history, neutralized final pose history, and discontinuity invalidation live in
`SKELETAL_ANIMATION_PLAYER::Impl`; the public headers expose only narrow enable/disable/query and
copy-out methods.

## Stop Rules

Do not reopen this cleanup branch unless at least one of these is true:

- a public header starts exposing a concrete backend SDK type again;
- a public header starts exposing a backend-owned handle or platform/backend layout again;
- a real ABI/source-compatibility goal is approved for a specific area;
- a broader redesign is explicitly accepted for one of the deferred surfaces above.

If none of those are true, treat the current status as complete.

## Repo Rule For Future Core Work

When touching `src/core_mbm/` or `include/core_mbm/`:

- preserve the current PIMPL/header-hygiene direction;
- do not introduce new public mutable storage when the state can live in `Impl`, `BackendData`, or a private translation unit helper;
- do not re-expose backend handles or concrete backend/platform layout in public headers;
- if a function uses the same accessor-backed object more than once, store it once in a local variable or reference for that scope;
- do not use “strict PIMPL” as a reason to force redesign-shaped work into a small hygiene milestone.

## What "Redesign-Shaped" Means Here

Some remaining items are not blocked by missing accessors. They are blocked because the current direct state is part of the actual engine/tool model.

Examples:

- `MESH_MBM_DEBUG` is a debug editing model, not just leaked storage.
- `BUFFER_MESH` remaining direct use is part of coordinated runtime mesh building and backend extraction.
- parser-local mesh physics readers are temporary/shared parsing logic, not a clean public-storage leak.
- `TEXT_DRAW` / `FONT_DRAW` remaining direct state is text layout/render working state and compatibility-facing context.

That kind of work is not “next PIMPL milestone” work. It is architecture work.

## Future Work Policy

If someone resumes this area later, the valid approach is:

1. Pick one narrowly scoped surface only.
2. Confirm it is not redesign-shaped.
3. Add accessors/helpers only if they reduce real layout coupling.
4. Move storage only after focused scans are clean.
5. Update this document to reflect the new boundary.

If the work is redesign-shaped, write the redesign plan first instead of treating it as mechanical cleanup.

## Historical Note

The old `docs/core-pimpl-gap-report.md` milestone diary was retired because the branch is no longer tracking active gap burn-down. The useful output now is the current boundary/status, not the chronological milestone log.

`MESH_MBM_DEBUG::Impl` no longer contains the exploratory section-11 skeleton or section-40
name-palette weight storage. Their public structs/APIs, serializers, and enum members were removed
with that storage. Active v11 loaders reject numeric types 11/40 and writers never emit them;
runtime and editor skeletal paths consume only canonical sections 41–43.

The read-only bind-pose getters copy fixed value records directly from
`Impl::canonicalSkeleton.compiled`, which is produced and validated during load. There is no
separate refresh operation or duplicate compiled snapshot. Compiled vectors, maps, strings, and
lookup storage remain private.

The canonical type-41 reader stores its source records plus compiled hierarchy exclusively in
`MESH_MBM::Impl` or `MESH_MBM_DEBUG::Impl`. Runtime and debug parse paths share validation, while no
mutable canonical storage or lookup container is exposed through the public header.
The skeletal-sharing compatibility report follows the same boundary: runtime and debug meshes expose
only a narrow copy-out report over `Impl`-owned canonical skeletons. The first runtime pose-sharing
slice keeps the follower/source relationship on `MESH` renderizables and borrows only the source
instance's existing private `SKELETAL_ANIMATION_PLAYER::Impl` palette during draw; no palette vector,
mutable bind data, lookup map, cached asset state, or backend handle is exposed or retained across
the shader call.
The type-42 reader follows the same boundary: its stable-ID palette and per-vertex four-influence
records remain `Impl`-owned and are validated against the compiled type-41 skeleton and frame-1
topology before being retained.
Type-43 follows identically: canonical clips, tracks, keys, easing, and lookup validation remain
private in `Impl`; the public mesh headers expose neither the vectors nor mutable animation state.
The backend-neutral skeletal preparation cache follows that boundary as well. Resolved float bone-index/weight
streams, method-specific palette counts, and readiness status live only in `MESH_MBM::Impl` via
the private `skeletal-gpu-lbs.h` contract. No backend handle, mutable vector, or convenience accessor
was added to the public mesh header. Each instance's selected LBS/rigid-DQS method and evaluated
palette remain in `SKELETAL_ANIMATION_PLAYER::Impl`; public reports copy only scalar status/counts.
The requested Auto/LBS/DQS policy, resolved backend method, and static resolution-reason pointer are
also instance-private; Auto scans immutable canonical data before shader compilation.
The requested GPU/CPU/Auto execution policy follows the same shape. The public surface exposes only
scalar policy/report values; the one-shot resolution helper is private, and dynamic CPU buffers stay
owned by the `MESH` instance rather than leaking mesh/cache/backend state through headers.
A test-only private parity bridge copies canonical skeleton/weights/clips out of
`MESH_MBM_DEBUG::Impl` into an internal structure. It is a friend solely to preserve the PIMPL
boundary for numeric GLES tests: no mutable reference, backend handle, Lua binding, or public
container accessor is introduced.
Its uploaded vertex streams preserve the backend boundary: GLES2 bone-index/weight buffer handles
and per-subset arrays live only in private `BUFFER_SPECIFIC` storage and are created through the
private backend-neutral `skeletal-gpu-upload.h` bridge. OpenGL ES, DirectX9, DirectX11, and Metal
provide the same private upload symbol from backend translation units. `BUFFER_GL`'s public layout/API did not acquire
a graphics handle or a skeletal-data container.
The corresponding shader integration adds only backend-neutral palette size and method compile
parameters to the public `SHADER` operation. GLES attribute/uniform handles remain in private
`GLES_PS_VS`; DirectX 9 declarations, streams, constant-table handles, and palette state remain in
its private backend structures. Both default-program cache keys include skeletal method and palette
size without exposing cache or backend program identity.

`ARTICULATED_ANIMATION_PLAYER` follows the same boundary: its public class exposes only lifecycle
operations and an opaque `Impl`. Active clips, time, pause state, priority, crossfade
duration/progress, per-play additive weight, and tie-break sequence remain private in
`mesh-manager-impl.h`.
`MESH_MBM::Impl` retains only cache-safe asset data (parts, authored clips, geometry, and scratch
rendering storage); each `ANIMATION_MANAGER` instance owns a separate player, used by `MESH` and
`SPRITE`, so cached assets never leak playback state between renderizable instances.
Canonical skeletal playback follows the same ownership rule with its own opaque
`SKELETAL_ANIMATION_PLAYER`. Active clip index, time, pause state, stable-ID layer mask, evaluated
global transforms, and palette rows live
in the renderizable instance's `ANIMATION_MANAGER::Impl`; the cached `MESH_MBM::Impl` keeps only
validated type-41/42/43 asset data and GLES-ready immutable vertex inputs. Shader draw calls accept
a transient pointer/count for the owning instance's palette without retaining or exposing it. The
runtime-pose inspection API copies one bone's stable ID, parent index, and global matrix at a time.
The named-bone gameplay query resolves that same private evaluated pose and copies decomposed TRS
plus its matrix in model or renderizable-composed world space; neither query exposes a vector,
mutable storage, lookup container, palette pointer, or backend handle.
Root-motion extraction retains only the previous evaluated global matrices inside the opaque
per-instance player and copies one named bone's translation delta; the history vector and validity
state remain private and are invalidated at discontinuities.
