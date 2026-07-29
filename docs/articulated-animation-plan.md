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

The first runtime version treats all parts as independent; parent-child composition is reserved for a later milestone.

## Clips and tracks

- Clips are named and live in a namespace separate from frame-animation clips.
- Duplicate articulated clip names are rejected.
- Multiple articulated clips may be active at the same time.
- Each clip has an integer priority.
- Higher priority wins when clips control the same part. On equal priority, the most recently started clip wins.
- A clip can contain independent position, rotation, and scale channels.
- Missing channels inherit the subset base transform.
- Keyframe time is stored as `float`.
- Animation time advances using the engine's `delta` and existing playback semantics.
- Position and scale use linear interpolation.
- Rotation is stored/interpolated as quaternion and edited/displayed as Euler angles.
- A duplicate keyframe for the same part, channel, and time replaces the existing key.
- Clip duration grows automatically to the greatest key time but remains manually editable.
- Orphaned tracks are preserved, ignored by runtime, and reported in debug/editor.

## Playback API

`ANIMATION_MANAGER` remains the public manager. It will retain traditional animation behavior and gain articulated operations equivalent to:

```cpp
playArticulatedAnimation(name, priority)
pauseArticulatedAnimation(name)
resumeArticulatedAnimation(name)
disableArticulatedAnimation(name)
```

The C++/Lua API will expose playback and read-only inspection of clips and parts, including names, IDs, priorities, and current time. Keyframe authoring initially remains an editor responsibility.

Pause freezes clip time while preserving its last pose and priority. Disable removes the clip from composition. A separate `stopArticulatedAnimation()` is not planned initially.

Articulated clips are loaded inactive and start only after an explicit play request. They also work over a static frame when no traditional frame animation is active.

## Rendering

`MESH::render()` will:

1. Update/select the traditional frame, when present.
2. Evaluate active articulated clips.
3. Render subsets independently with their final transforms when articulated animation is active.

Without active articulated clips, the current static rendering path remains unchanged.

The common implementation belongs in `MESH_MBM`, so the initial behavior applies to the existing OpenGL, DirectX9, and Metal paths. Backend-specific performance optimizations are deferred.

## Binary format

Add two optional sections to Mesh V11:

```cpp
SECTION_ARTICULATED_PARTS
SECTION_ARTICULATED_ANIMATION
```

`SECTION_ARTICULATED_PARTS` stores part IDs, names, pivots, and future hierarchy data.

`SECTION_ARTICULATED_ANIMATION` stores named clips, priorities/default playback data, tracks, channel masks, key times, and transform values.

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

## Deferred items

- Parent-child transform composition in runtime/editor.
- Curves and easing.
- Backend-specific rendering optimizations.
- Sprite (`.spt`) support.
- Blending and per-property composition between clips.
- Runtime keyframe authoring from game code.
