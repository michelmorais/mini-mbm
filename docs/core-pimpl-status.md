# Core MBM PIMPL Status

Updated: 2026-06-22

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
| `MESH_MBM_DEBUG` | Lua/helper-prep slices are complete; storage remains direct. | Remaining direct use is debug-tool behavior, editing/storage mutation, and compatibility-facing payload/header state. | Revisit only with a broader debug-mesh editing/storage redesign. |
| `PLUGIN::onSubscribe(void *context, void *renderDevice)` | Still uses opaque backend/platform handles. | This is a plugin ABI/design issue, not current backend-header leakage. | Revisit only if plugin ABI versioning/stable wrapper design becomes a formal goal. |
| File-format structs such as `header-mesh.h` | Still public. | They describe serialized data and asset compatibility. | Do not PIMPL first. Revisit only during a file-format redesign. |

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

`MESH_MBM_DEBUG::Impl::skeleton` (`std::vector<util::SKELETON_BONE_V11>`, `SECTION_FRAME_SKINNED` persistence for `mesh_debug.lua`'s Bones node round-trip, added alongside `addBone`/`getBone`/`getTotalBone`) follows the standard Impl-only rule from "Repo Rule For Future Core Work" below — noted here explicitly so a future reader doesn't have to re-derive that this was a deliberate, rule-compliant addition rather than an oversight. `MESH_MBM::Impl` was deliberately NOT given an equivalent field (no runtime skinning consumer exists); the shared parse path merely tolerates and discards the section for that class.

`updateBone`/`removeBone` (added for `mesh_debug.lua`'s general-purpose Bones editor node) are pure `MESH_MBM_DEBUG` methods operating only on the existing `impl->skeleton` field above — no new header-visible state, so the PIMPL boundary itself doesn't move. `updateBone`'s reparent path calls a small anonymous-namespace helper, `resortSkeletonParentFirst` (`src/core_mbm/mesh-manager.cpp`), kept as a private translation-unit function rather than a class method per the same rule, since it's pure vector-reordering logic with no need to touch `Impl` directly beyond the vector reference it's passed.

`ARTICULATED_ANIMATION_PLAYER` follows the same boundary: its public class exposes only lifecycle
operations and an opaque `Impl`. Active clips, time, pause state, priority, and tie-break sequence
remain private in `mesh-manager-impl.h`. `MESH_MBM::Impl` retains only cache-safe asset data
(parts, authored clips, geometry, and scratch rendering storage); each `ANIMATION_MANAGER`
instance owns a separate player, used by `MESH` and `SPRITE`, so cached assets never leak playback
state between renderizable instances.
