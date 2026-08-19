# Real-Time Skeletal Animation - Deferred Work Plan

Status: **Optional follow-up projects; not required for the accepted runtime/editor delivery**
Last reviewed: **2026-08-19**

## 1. Purpose

This document tracks only skeletal-animation capabilities that were deliberately left outside the
completed runtime and editor delivery. It is not a continuation history and does not reopen accepted
OpenGL ES, DirectX 9, Metal, CPU/GPU, LBS/DQS, editor, or FBX round-trip work.

Each area below is an independent project. Before implementation, it should receive a bounded design,
platform scope, fixtures, performance budget, and versioned acceptance criteria.

## 2. Recommended Order

| Order | Project | Reason |
|---|---|---|
| 1 | Multi-layer composition and persistence | Extends the existing base-plus-one-layer contract used by games |
| 2 | Persistent shared skeletal resources | Formalizes body/wearable ownership beyond transient direct followers |
| 3 | Skeleton-to-skeleton retargeting | Adds a new animation-data transformation boundary and needs dedicated fixtures |
| 4 | Extended scale and palette support | Requires shader/palette contract changes and backend-specific measurement |
| 5 | Editor and FBX refinements | Should be driven by concrete authoring assets rather than DCC feature parity |
| 6 | Renderer modernization and secondary deformation | Separate research/architecture work, not a skinning correctness requirement |

## 3. Deferred Projects

### 3.1 Multi-layer composition

Generalize the current base plus one transient Absolute/Additive layer into an explicitly bounded
layer stack. Define priority, ordering, queues, transition curves, masks, pause/time ownership, and
serialization separately from canonical source clips.

Acceptance: deterministic composition order, bounded runtime cost, transactional mutations, stable
save/reload semantics, Lua/editor coverage, and no extra skinning draw per layer.

### 3.2 Persistent skeletal sharing

Define reusable skeleton/clip resource ownership and persisted body/wearable relationships beyond
the current direct runtime follower. Decide whether follower chains remain forbidden and how source
advancement order is guaranteed across scenes and asynchronous loading.

Acceptance: explicit lifetime ownership, compatibility/version rules, deterministic load order,
safe reload/destruction, and no duplicated pose evaluation or palette copy for compatible followers.

### 3.3 Animation retargeting

Add an explicit offline/editor operation for applying a clip to a different compatible skeleton.
Define bone mapping, bind-space correction, proportion handling, missing/extra bones, root motion,
scale policy, and error reporting. Do not treat FBX round-trip bind reconstruction as retargeting.

Acceptance: source and destination remain independent assets; named reference poses and clips match
expected global transforms within tolerance; unsupported mappings fail without partial mutation.

### 3.4 Extended deformation and palette capacity

Evaluate these independent additions:

- inverse-transpose GPU normal palettes for non-uniform LBS scale;
- two-phase or another explicit DQS scale/stretch model;
- palette partitioning or another transport for skeletons above the effective per-draw limit;
- buffer/texture-backed palettes or compute deformation where a backend justifies them.

Acceptance: no silent scale/shear approximation, measured memory/draw cost, CPU reference parity,
and unchanged canonical clip/weight semantics unless a new format version is explicitly required.

### 3.5 Editor and interchange refinements

Candidate independent tools are animation-aware subtree mirroring, protected/exclusion volumes,
topology-ring expansion, welded diagnostic topology, bounded automatic weight generation, richer
pose-stress/antipodality diagnostics, and custom-tail authoring.

FBX coverage should expand only through named source fixtures that demonstrate a missing armature,
Action/NLA, deformer, or transform case. The objective is measured interoperability, not universal
FBX or Blender feature parity.

Acceptance: every tool is transactional and Undoable, expensive analysis is event/dirty-driven, idle
editor cost remains bounded, and import/export additions have reproducible round-trip fixtures.

### 3.6 Renderer modernization and secondary deformation

Evaluate modern OpenGL versus Vulkan as a renderer-wide decision. Velocity Skinning, corrective
shapes, bulge compensation, muscle effects, and procedural/physical tail motion remain optional
secondary deformation projects built above the accepted LBS/DQS pose contract.

Acceptance: these features remain capability-driven and optional; legacy OpenGL ES and DirectX 9 do
not define modern limits, and no secondary effect destabilizes ordinary bind, playback, or skinning.

## 4. Planning Rule

Starting one project does not imply starting the others. A future merge should move only the selected
project into its own implementation plan and remove it from this deferred list after its capability
is delivered and documented in
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md).
