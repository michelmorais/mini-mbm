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
| 1 | Lua/prefab skeletal-sharing workflows | Keeps body/wearable composition in game code while reusing the existing transient runtime sharing |
| 2 | Editor refinements | Keeps optional authoring aids separate and driven by concrete editor needs |
| 3 | Lightweight secondary motion | Keeps optional appendage motion bounded above the accepted skeletal pose contract |

## 3. Deferred Projects

### 3.1 Lua/prefab skeletal-sharing workflows

Keep body/wearable relationships outside the engine's persisted mesh format and ownership model.
Provide optional Lua examples or a game-level prefab convention that reconstructs direct transient
followers with the existing skeletal-pose-sharing API after the source and followers are loaded.

Acceptance: direct source/follower setup remains explicit, compatibility failures are reported,
reload and destruction are handled safely by the game workflow, and compatible followers do not
duplicate pose evaluation or palette copies.

### 3.2 Editor refinements

Candidate independent tools are animation-aware subtree mirroring, protected/exclusion volumes, and
welded diagnostic topology.

Acceptance: every tool is transactional and Undoable, expensive analysis is event/dirty-driven, idle
editor cost remains bounded, and each addition has reproducible fixtures for its concrete authoring
case.

### 3.3 Lightweight secondary motion

Keep Velocity Skinning only as a research reference and possible source of ideas, not as a planned
feature or compatibility target. A small procedural secondary-bone chain for a tail, hair, or a
similar appendage may be evaluated independently when a concrete game needs it. Keep that candidate
bounded to a simple deterministic spring/damping model with explicit reset and pause behavior; do
not turn it into a general soft-body system, collision solver, muscle simulation, or complex physics
framework.

Acceptance: a secondary-motion feature must remain optional, bounded in state and per-frame cost,
stable across pause/reset and fixed or variable frame steps, and must not destabilize ordinary bind,
playback, root motion, or LBS/DQS skinning. Backend modernization policy belongs to
[Implementing a New Rendering Backend](new-backend-instructions.md#project-direction-and-complexity-gate),
not to the skeletal-animation roadmap.

## 4. Planning Rule

Starting one project does not imply starting the others. A future merge should move only the selected
project into its own implementation plan and remove it from this deferred list after its capability
is delivered and documented in
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md).
