# Articulated Animation Plan

## Objective

Add rigid/articulated animation for `.msh` assets. This feature transforms independent mesh subsets and is not skeletal skinning.

The first target is a solid machine such as a car: one frame containing a body and wheel subsets, with each wheel independently rotatable.

## Core model

- Articulated animation is additive to traditional frame animation.
- Frame animation selects the base frame; articulated animation transforms subsets of that frame.
- `ANIMATION` will distinguish content kind from playback state:
  - `ANIMATION_KIND_FRAME`
  - `ANIMATION_KIND_ARTICULATED`
- Existing `TYPE_ANIMATION` states, including `TYPE_ANIMATION_PAUSED`, remain responsible for playback behavior.
- The base transform and geometry of a subset are preserved.
- An animated transform is evaluated relative to the subset pivot.

## Parts and identity

Each articulated part will have:

- Persistent `uint64_t partId`, generated once and never reused after deletion.
- Optional editable name. Names may repeat in different frames.
- Local pivot position and orientation.
- Optional `parentPartId`, persisted for future hierarchy support.

`partId` is the runtime and file-format identity. Names are labels only. If a name is duplicated in the active frame, debug mode reports the conflict and the first match is used for name lookup.

Parent-child composition is supported at runtime. A child inherits the complete transform of its
parent, while retaining its own local transform and mesh-space pivot. Parent links must stay within
the same frame and cycles are rejected.

## Clips and tracks

- Clips are named and live in a namespace separate from frame-animation clips.
- Duplicate articulated clip names are rejected.
- Multiple articulated clips may be active at the same time.
- Each clip has an integer priority.
- Priority is resolved independently for position, rotation, and scale. On equal priority, the most
  recently started clip that supplies the channel wins.
- A clip can contain independent position, rotation, and scale channels.
- A missing channel may come from a lower-priority active clip; the subset base transform is used
  only when no active clip supplies that channel.
- Keyframe time is stored as `float`.
- Animation time advances using the engine's `delta` and existing playback semantics.
- Position and scale use linear interpolation.
- Rotation keeps a quaternion for rendering, while authored Euler degrees are also persisted.
  Runtime interpolation uses the authored Euler values
  so transitions such as 0° to 359° preserve the intended full turn. The articulated-animation
  section is still optional; meshes without it remain unchanged.
- A duplicate keyframe for the same part, channel, and time replaces the existing key.
- Clip duration grows automatically to the greatest key time but remains manually editable.
- Orphaned tracks are preserved, ignored by runtime, and reported in debug/editor.

## Playback API

`ANIMATION_MANAGER` remains the public manager. It will retain traditional animation behavior and gain articulated operations equivalent to:

```cpp
playArticulatedAnimation(name, priority, blendDuration, weight)
pauseArticulatedAnimation(name)
resumeArticulatedAnimation(name)
disableArticulatedAnimation(name)
```

The C++/Lua API will expose playback and read-only inspection of clips and parts, including names, IDs, priorities, and current time. Keyframe authoring initially remains an editor responsibility.

Pause freezes clip time while preserving its last pose and priority. Disable removes the clip from composition. A separate `stopArticulatedAnimation()` is not planned initially.

Articulated clips are loaded inactive and start only after an explicit play request. They also work over a static frame when no traditional frame animation is active.
When a non-looping clip reaches its duration, it emits the existing animation-end callback once,
using the articulated clip name. Looping clips do not emit this callback. Multiple clips ending in
the same update each emit their own completion event.

## Rendering

`MESH::render()` will:

1. Update/select the traditional frame, when present.
2. Evaluate active articulated clips.
3. Render the articulated pose when articulated animation is active.

Without active articulated clips, the current static rendering path remains unchanged.

The initial implementation used `MESH_MBM::renderArticulatedDynamic()` to transform a CPU-side
working copy. `MESH_MBM::renderArticulatedStatic()` is now the active path for `.msh`: it reuses
the loaded static vertex/index buffers and renders each subset independently, applying its final
matrix before that subset's draw call. The `SHADER::render()` API keeps the original all-subsets
behavior and accepts an optional subset selector, implemented by OpenGL ES, DirectX9, Metal, and
the dummy backend. No transformed vertex buffer is duplicated or uploaded. The dynamic method is
retained as a fallback/reference path while the static path receives runtime validation.

## Binary format

Add two optional sections to Mesh V11:

```cpp
SECTION_ARTICULATED_PARTS
SECTION_ARTICULATED_ANIMATION
```

`SECTION_ARTICULATED_PARTS` stores part IDs, names, pivots, and future hierarchy data.

`SECTION_ARTICULATED_ANIMATION` stores named clips, priorities/default playback data, Absolute or
Additive composition mode, tracks, channel masks, key times, transform values, and the per-segment
easing mode. Linear is the default; Smoothstep, Cubic Bezier with two editable control points, and
the other basic ease modes are evaluated by the runtime sampler. Playback weight remains
instance-local and is not serialized.

Both sections are omitted when there is no corresponding data. Existing `.msh` files without either section remain valid and follow the current static/frame-animation path.

## Mesh Debug Editor

Add an `Articulated Animation` node while renaming the existing animation area to `Frame Animation`.

The editor will provide:

- Named clip creation and selection.
- Simultaneous clip preview with priority resolution.
- One clip edited at a time.
- Timeline scrubbing that immediately evaluates the pose.
- Play/pause controls using the engine delta behavior.
- Visual subset and pivot gizmos.
- Explicit `Add Keyframe` confirmation.
- Independent Position, Rotation, and Scale checkboxes.
- Automatic duration extension with manual duration editing.
- Debug warnings for duplicate names and orphaned tracks.

## Milestones

1. Add part metadata and optional V11 serialization.
2. Extend `ANIMATION`/`ANIMATION_MANAGER` with articulated kind and playback state handling.
3. Add articulated clip evaluation, priority resolution, and transform math.
4. Add per-subset rendering in `MESH_MBM` and preserve the static fast path.
5. Add Mesh Debug Editor authoring and preview workflow.
6. Expose C++/Lua playback and inspection APIs.
7. Validate old/new file compatibility and update version/documentation.
8. Integrate `MESH_MBM::renderArticulatedStatic()` using static buffers and per-subset draw calls;
   validate rotations, scales, materials, culling, and 2D/3D behavior in the editor and runtime.
9. Move active clip playback state from the cached `MESH_MBM` asset into one opaque player per
   renderizable instance, while continuing to share geometry and authored clip data.
10. Reuse the per-instance player and static per-subset render path for Sprite (`.spt`).
11. Add optional runtime crossfade between clips and resolve position, rotation, and scale
    independently by priority. Each channel composes candidates from the base transform through
    increasing priority/start order, keeping overlapping crossfades continuous. A zero duration
    preserves immediate playback.
12. Add persisted Absolute/Additive composition modes. Absolute clips establish the per-channel
    base pose; active additive clips then contribute weighted position offsets, local quaternion
    rotation deltas, and scale multipliers. Runtime weight is per play invocation, while blend
    duration fades additive influence from zero to that weight. Authored Euler deltas are weighted
    before quaternion conversion so full-turn intent remains available.

## Deferred items

- GPU-side per-subset transform storage in shaders.
- Runtime keyframe authoring from game code.
