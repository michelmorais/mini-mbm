# Articulated Animation

Document version: **1.1**
Status: **Implemented**
Last updated: **2026-08-17**

## 1. Purpose

This guide explains Mini MBM's implemented articulated-animation system: what it is, when to use
it, how parts, pivots, clips, tracks, composition, and playback behave, and how to author and run it.

For API signatures, see [Lua API](lua-api.md#articulated-playback-mesh-and-sprite). For the exact
binary layout, see [Mesh V11 format](mesh-v11-format.md#6f-section_articulated_parts-and-section_articulated_animation-payloads).

## 2. Choose the Correct Animation Model

Mini MBM has distinct animation models:

| Model | What changes | Best suited to |
|---|---|---|
| Frame animation | Selects complete pre-baked geometry frames | Vertex animation, baked deformations, legacy animated meshes |
| Articulated animation | Applies rigid transforms to independent material subsets | Vehicles, doors, wheels, mechanisms, modular characters |
| Skeletal skinning | Blends each vertex through bone weights | Organic characters and deformable joints |

Articulated animation is implemented for Mesh (`.msh`) and Sprite (`.spt`) assets. It is not
skeletal skinning: it does not read skin weights, calculate inverse-bind matrices, or deform a
single subset's vertices differently from one another. Every vertex in an animated subset receives
the same final part transform.

This makes it ideal for rigid components. It cannot create a smooth shoulder, neck, elbow, or tail
bend unless those regions are already divided into visibly rigid subsets.

## 3. Mental Model

```text
asset
├── frame animation (optional base frame selection)
├── articulated parts
│   └── one persistent Part for a frame/subset occurrence
│       ├── pivot position and orientation
│       └── optional parent Part in the same frame
└── articulated clips
    └── tracks targeting Part IDs
        ├── Position
        ├── Rotation
        └── Scale

renderizable instance (Mesh or Sprite)
└── independent active-clip state
    ├── time / paused / ended
    ├── runtime priority and start order
    ├── blend progress
    └── additive weight
```

Authored parts and clips belong to the cached asset. Active playback belongs to each Mesh or Sprite
instance, so two objects can share one asset while playing different clips or times.

Frame animation and articulated animation can coexist. Frame animation selects the base frame;
articulated animation then transforms the Parts belonging to that frame.

## 4. Parts, Subsets, and Pivots

A Part associates articulated identity and pivot information with one `(frame, subset)` occurrence.
Its persisted properties are:

- globally unique, nonzero `partId` within the asset;
- frame index and subset index;
- editable display name;
- pivot position and quaternion orientation;
- optional `parentPartId`.

`partId`, not the name, is the stable identity used by tracks. Names are labels and may repeat. A
frame/subset occurrence can have at most one Part.

### Pivot

Position, rotation, and scale are evaluated around the Part's pivot. Pivot orientation changes the
local axes used by the animated rotation. A wheel, for example, normally needs its pivot centered on
the axle and its pivot orientation aligned with the desired rotation axis.

Moving the pivot does not move or edit the underlying vertices. It changes the transform applied
during articulated rendering.

### Hierarchy

A Part may reference a parent Part in the same frame. The child applies its local transform first
and then inherits the complete parent hierarchy. Parent links across frames and cycles are invalid.

Typical examples:

- vehicle body → door;
- crane base → arm → hook;
- torso subset → rigid head subset;
- machine body → wheel subsets.

Deleting Parts also removes tracks that target those Part IDs. Clips themselves remain available.

## 5. Clips, Tracks, and Keys

An articulated clip stores:

- unique clip name;
- duration in seconds;
- playback speed;
- default editor/runtime metadata priority;
- loop flag;
- composition mode: Absolute or Additive;
- zero or more tracks.

Runtime `playArticulatedAnimation()` receives the effective priority explicitly; its Lua default is
`0`. The persisted default priority is used by the authoring/preview workflow and is not implicitly
substituted when Lua omits the argument.

Each track targets one `partId` and enables one or more independent channels:

- Position (`1`);
- Rotation (`2`);
- Scale (`4`).

A key contains values for P/R/S, but only channels enabled by the track participate in composition.
Duplicate key time within a track replaces the existing key. Keys are evaluated in time order.

### Interpolation and easing

Position and scale interpolate linearly after applying the selected easing curve. Rotation supports
two paths:

- when both surrounding keys contain authored Euler degrees, Euler values are interpolated and then
  converted to a quaternion, preserving authored turns such as `0° → 359°`;
- otherwise, normalized shortest-path quaternion interpolation is used, including sign alignment
  for equivalent antipodal quaternions.

The easing stored on a key controls the segment from that key to the next key:

| Value | Mode |
|---:|---|
| 0 | Linear |
| 1 | Ease In |
| 2 | Ease Out |
| 3 | Ease In Out |
| 4 | Smoothstep |
| 5 | Cubic Bezier |

Cubic Bezier uses control points `(x1, y1)` and `(x2, y2)`. X values stay within `0..1`; Y values
may leave that range to create overshoot.

## 6. Absolute and Additive Composition

Composition is resolved independently for Position, Rotation, and Scale. A clip can therefore own
one channel while another active clip supplies a missing channel.

### Absolute

Absolute clips describe a target pose. Candidates are applied from lower to higher priority; when
priorities match, the more recently started clip is applied later. At full blend, the later
candidate wins that channel. During its initial blend, it transitions from the pose accumulated so
far toward its target.

`blendDuration` controls this initial transition. The `weight` argument does not attenuate an
Absolute clip in the current implementation.

### Additive

Additive clips are composed after the Absolute result:

- Position adds an offset from zero.
- Rotation composes a delta from identity.
- Scale multiplies by a value relative to one.

The effective additive influence is:

```text
clamped weight * initial blend progress
```

`weight` and `blendDuration` therefore both affect Additive clips. Weight is clamped to `0..1`, and
negative blend duration becomes zero.

Additive candidates are also ordered by priority and start order. Because additive transforms are
composed sequentially, rotation order can affect the result.

## 7. Playback Lifecycle

`playArticulatedAnimation(name, priority, blendDuration, weight)` activates a clip. Playing an
already active name restarts it at time zero, refreshes its start order, resets blend progress, and
clears paused/ended state.

- **Pause** freezes clip time and blend progress while preserving its current pose.
- **Resume** continues a paused active clip.
- **Seek** moves an active clip to a time clamped to `0..duration` and clears ended state.
- **Disable** removes the clip from composition.

Looping clips wrap using their duration. A non-looping clip stops at its duration, invokes the
existing animation-end callback once, and remains active holding its final pose. Call Disable when
that final pose should stop participating.

A duration of zero stays at time zero. A negative authored speed behaves as zero at runtime.

## 8. Lua Usage

The same playback methods are available on loaded Mesh and Sprite objects:

```lua
local vehicle = mesh:new("3d")
assert(vehicle:load("vehicle.msh"))

for i = 1, vehicle:getTotalArticulatedAnimations() do
    print(i, vehicle:getArticulatedAnimationName(i))
end

-- Absolute door pose, priority 10, 0.25-second initial blend.
assert(vehicle:playArticulatedAnimation("OpenDoor", 10, 0.25, 1.0))

-- Additive vibration authored in the asset, half influence.
assert(vehicle:playArticulatedAnimation("EngineVibration", 20, 0.1, 0.5))

vehicle:onEndAnim(function(object, animationName)
    print("finished", animationName)
    -- Optional: remove a non-looping clip instead of holding its final pose.
    object:disableArticulatedAnimation(animationName)
end)
```

Important API details:

- animation-name enumeration is 1-based in Lua;
- playback/control methods return `true` on success and `false` for an unknown/inactive clip;
- `getArticulatedAnimationName(index)` returns a string or `nil`;
- `getArticulatedAnimationTime(name)` returns a number only while the clip is active, otherwise
  `nil`;
- Pause, Resume, Seek, and Disable operate only on active clips;
- `onEndAnim` receives `(object, animationName)` for non-looping frame animations and articulated
  clips.

See the [Lua API reference](lua-api.md#articulated-playback-mesh-and-sprite) for the complete method
table.

## 9. Authoring in Mesh Debug

Open a `.msh` or `.spt` asset and expand the **Articulated Animation** node. The current workflow is:

1. initialize or select Parts for frame/subset occurrences;
2. position and orient each pivot;
3. assign parent Parts where hierarchy is needed;
4. create a named clip and configure duration, speed, priority, loop, and blend mode;
5. select a Part and create a track with the required P/R/S channels;
6. move the timeline to the target time;
7. enter transform values and explicitly add/update keys;
8. choose easing and, for Cubic Bezier, edit its control points;
9. save the asset before relying on runtime preview;
10. use Play, Pause, Resume, Disable, and timeline seek in the preview.

The editor previews several active clips simultaneously, which is necessary for validating
priority, crossfade, and Absolute/Additive composition. Only one clip is edited at a time.

Part selection is frame-aware. When editing a hierarchy, confirm that parent and child Parts belong
to the same frame. Orphaned tracks can be preserved in data but are ignored by runtime until their
target Part exists again.

## 10. Rendering and Performance Model

Articulated rendering reuses the loaded static vertex and index buffers. It draws subsets
independently and supplies each animated subset's final matrix. It does not create a transformed
CPU-side vertex copy per frame and does not upload a deformed vertex buffer.

Consequences:

- geometry inside one subset remains rigid;
- a model with many independently animated subsets can require more draw calls;
- materials and subset boundaries are part of the authoring design;
- an inactive asset follows the existing static/frame-animation path;
- the implementation works through the engine's OpenGL ES, DirectX 9, Metal, and dummy shader
  paths that support subset selection.

## 11. Common Pitfalls

| Symptom | Likely cause | Direction |
|---|---|---|
| Part rotates around the wrong point | Pivot position is incorrect | Move the pivot to the mechanical joint |
| Part rotates on the wrong axis | Pivot orientation is misaligned | Orient the pivot axes before authoring rotation keys |
| Child does not follow the intended parent | Parent is missing, invalid, or in another frame | Reassign a valid same-frame parent and check for cycles |
| A clip has no visible effect | Track targets another Part, has no keys, or lacks the intended channel | Inspect Part ID, key count, and P/R/S mask |
| `weight` does not weaken an Absolute clip | Weight is currently applied to Additive composition | Use priority/blend design or author an Additive clip |
| Finished clip keeps its pose | Non-looping clips remain active at their final time | Call `disableArticulatedAnimation()` |
| Smooth organic joint still looks segmented | Articulated animation transforms rigid subsets | Use skeletal skinning/weights when available or bake frame animation |
| Full turn takes the short path | Keys lack authored Euler data | Author rotation through the editor so Euler intent is persisted |
| Different object instances are at different times | Playback is intentionally instance-local | Start/seek each instance explicitly |

## 12. Persistence and Compatibility

Mesh V11 stores articulated data in two optional sections:

- `SECTION_ARTICULATED_PARTS` (`12`);
- `SECTION_ARTICULATED_ANIMATION` (`13`).

Assets without these sections remain valid and use the normal static/frame-animation path. The
format stores authored Parts, clips, tracks, keys, easing, and blend mode. Active playback time,
runtime priority, start order, blend progress, and runtime weight are per-instance state and are not
serialized.

Do not use this guide as the byte-level format specification. See
[Mesh V11 format §6f](mesh-v11-format.md#6f-section_articulated_parts-and-section_articulated_animation-payloads)
for field order, types, versions, and loader invariants.

## 13. Relationship to Skeletal Skinning

The implemented Skeletal Animation Editor reuses established vocabulary and interaction patterns
for hierarchy, clips, P/R/S tracks, easing, timeline, blend, and Absolute/Additive composition.
The underlying deformation remains different:

- articulated animation targets Parts and applies one rigid matrix per subset;
- skeletal animation targets bones and blends each vertex using skin weights through LBS or DQS.

The systems share suitable playback concepts without conflating Part IDs, bone identities, pivots,
bind poses, or file sections. See
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md).

## 14. Source-of-Truth Map

| Concern | Reference |
|---|---|
| User workflow and behavioral explanation | This guide |
| Lua signatures and return values | [Lua API](lua-api.md#articulated-playback-mesh-and-sprite) |
| Binary layout and persisted invariants | [Mesh V11 format](mesh-v11-format.md#6f-section_articulated_parts-and-section_articulated_animation-payloads) |
| Skeletal LBS/DQS relationship | [Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md) |
| Weight authoring | [Skeletal Animation Editor](skeletal-animation-editor.md) |

## 15. Change Log

| Version | Date | Change |
|---|---|---|
| 1.1 | 2026-08-17 | Updated the source-of-truth link label now that skeletal LBS/DQS is implemented on GLES rather than wholly future work. |
| 1.0 | 2026-08-06 | Initial implementation-backed guide covering model selection, Parts/pivots, hierarchy, clips/tracks/keys, easing, composition, playback, Lua, Mesh Debug authoring, rendering, persistence, and future skeletal-skinning boundaries. |
