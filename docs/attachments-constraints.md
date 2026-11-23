# Attachments and constraints

Document version: **2.4**
Status: **Phase 7 look-at limits, editable aim points, and spring smoothing implemented on top of the validated socket pipeline**
Last updated: **2026-11-23**

This document defines the runtime contract for root-transform and skeletal-bone attachments. It is independent
from `interaction_animation_runtime`; the interaction plugin does not create, update, or own
attachments.

## Runtime and Lua module

The optional C++ plugin is named `attachments` and is loaded with:

```lua
local attachments = require "attachments"
```

The CMake option is `USE_ATTACHMENTS=1`. `USE_ALL=1` enables it. The iOS and macOS App Store
profiles link the plugin statically; desktop builds load `attachments.so`, `attachments.dylib`, or
`attachments.dll` from the normal Lua C module path.

## Validation status

The current editor and `attachments` plugin were manually validated with the rat and propeller
project. The skeletal-bone attachment, target pivot, animation preview, target rotation follow,
socket authoring, and socket viewport gizmos were exercised. The complete interchange path was
also validated: open the mesh in Mesh Debug, export to FBX, apply an animation in Mixamo, download
the FBX, import it again through Mesh Debug, and reopen it in the Skeletal Animation Editor. The
authored sockets remained available after that round-trip.

## Transform contract

The source can be the `root` transform of a renderizable object or the evaluated world transform of
a named skeletal bone or socket on a mesh. An offset is stored in the selected source's local space:

```text
attachment position = source position + (source scale * offset position) * source rotation
target origin = attachment position - (target scale * targetPivot) * target rotation
world rotation = offset rotationMatrix * source rotation
world scale    = source scale * offset scale
```

The source scale in the position expression is used only when `inherit.scale` is enabled. The
source rotation orients the position offset whenever position inheritance is enabled, even when
rotation inheritance is disabled. Position, rotation, and scale are written independently; a
component whose inheritance flag is false is left untouched. Bone sources require an active,
evaluated skeletal pose; socket queries resolve to the evaluated pose or to the canonical bind
pose when playback is inactive. Sockets are named points stored in optional MSH V11 section 47;
they do not change skinning weights or animation tracks. `targetPivot` is an optional point in the
target mesh's local space. Its zero value preserves
the original behavior, where the target renderizable origin is the attachment point. A non-zero
value lets a base, socket-like point inside a mesh remain attached while the target origin moves
around it.

Angles use radians, matching `RENDERIZABLE::getAngle()` and the regular Lua renderizable API.
Composition uses matrices internally and converts back to Euler XYZ radians only when writing the
engine transform. The optional `offset.rotationMatrix` is the authoritative 3x3 source-local
rotation captured by the editor and runtime capture API; older definitions without it derive the
same matrix from `offset.rotation`. For bone sources, the evaluated bone matrix is normalized by
its evaluated scale before composition, avoiding an intermediate Euler reconstruction during
animation. Non-uniform
scale is supported because the offset position is scaled before source rotation. Sheared transforms
are not part of this contract.

The composition follows the engine's row-vector convention. Capturing an offset therefore stores
`target rotation * inverse(source rotation)`, and applying it composes the source-local frame before
the evaluated source frame. Reversing this order can make a rotation appear on a different axis
once a skeletal animation changes the bone pose, even when the bind/T-pose looks correct.

## Lua API

```lua
local attachment, err = attachments.attach {
    id = "hat_on_character", -- optional; generated when omitted
    source = character,
    target = hat,
    sourceType = "socket", -- "root", "bone", or "socket"; defaults to "root"
    sourceSocket = "RightHandGrip", -- required when sourceType is "socket"
    inherit = {
        position = true,
        rotation = true,
        scale = false,
    },
    aimAtSource = false, -- optional; target +Z points toward the source aim point
    aimPoint = {x = 0, y = 0, z = 0}, -- optional source-local point used by Look at
    aimLimits = { -- optional; only used when aimAtSource is true
        enabled = false,
        reference = "dynamic", -- "dynamic" or "captured"
        yawMin = -math.pi,
        yawMax = math.pi,
        pitchMin = -math.pi / 2,
        pitchMax = math.pi / 2,
    },
    spring = { -- optional; disabled by default
        enabled = false,
        position = true,
        rotation = true,
        stiffness = 120,
        damping = 24,
    },
    offset = {
        position = {x = 0, y = 1.8, z = 0},
        rotation = {x = 0, y = 0, z = 0}, -- radians
        scale = {x = 1, y = 1, z = 1},
    },
    targetPivot = {x = 0, y = 0, z = 0}, -- target-local point kept on the attachment
}
assert(attachment, err)
```

`attach` rejects invalid or destroyed renderizables, cross-scene references, self-attachments,
cycles, duplicate component ownership on one target, missing or unevaluated bones, and unsupported
source types. It returns
`nil, errorMessage` for those validation failures.

An attachment object provides:

| Method | Result | Meaning |
|---|---|---|
| `detach(options?)` | boolean | Deactivates the relation. `preserveWorldPose` defaults to `true`. |
| `isActive()` | boolean | Whether the relation is active in the current scene. |
| `getId()` | string | Stable attachment identifier. |
| `getOffset()` | table | Returns `position`, `rotation`, and `scale` tables. |
| `setOffset(offset)` | boolean | Changes the local offset without changing ownership. |
| `setInherit(inherit)` | boolean or `nil,error` | Changes the component mask after conflict/cycle validation. |
| `captureOffset()` | boolean or `nil,error` | Captures the current target pose relative to the current source pose. |
| `getEffectivePose()` | table or `nil,error` | Returns the target pose currently computed by the attachment. |
| `getState()` | table | Returns active/invalid status, source type/name, inheritance flags, and offset. |

`detach()` preserves the target's current pose by default. Passing
`{preserveWorldPose = false}` restores the target pose captured immediately before `attach`.
`getEffectivePose()` and `getState().effectivePose` expose `position`, `rotation` (radians), and
`scale` after applying the inheritance mask; components that are not inherited retain the target's
current values. Destroying a source, target, or scene deactivates the relation and releases its Lua
references.

## Frame order and ownership

The plugin resolves attachments in `PLUGIN::onLoop`, after scene logic, deferred main-thread work,
physics, and audio, and before rendering. It therefore reads externally controlled source poses in
the same frame and writes the target before the render pass. It does not use a Lua callback to apply
the essential transform.

Attachments form a directed graph from source to target. Active relations are resolved with a
stable topological order so chains such as `character -> hand -> sword -> effect` work. A relation
that would introduce a cycle is rejected. Two active attachments cannot own the same position,
rotation, or scale component of one target. The first implementation cannot observe arbitrary
gameplay or physics writes made outside the attachment plugin; it guarantees only that attachment
relations do not conflict with one another and that unowned components are not overwritten.

Lua references to the source and target tables keep them alive while an attachment is active. The
attachment keeps these references itself; `USER_DATA_RENDER_LUA::ref_MeAsTable` is not required for
ordinary renderizable construction. Scene teardown disables all relations and releases their
references.

## Authoring boundary

`editor/attachments_editor.lua` is an independent, definition-only authoring tool. Its saved
`.att.lua` file contains source/target labels, inheritance flags, and the local offset. It may also
contain an `editor` block with mesh paths, actor kinds, and preview poses; that block is ignored by
the runtime plugin. The editor has its own state and does not require or modify
`interaction_animation_editor.lua` or `interaction_animation_runtime`.

The source and target strings are runtime binding labels, not mesh filenames. The editor does not
discover actors from a runtime scene. Instead, its `Export runtime` action writes a self-contained
Lua module that keeps these labels and resolves them against an explicit binding table supplied by
the game.

For example, if the editor exports `character_weapon.runtime.lua`, the game can use:

```lua
local runtime = require "character_weapon.runtime"
local relation, errorMessage = runtime.attach {
    character = characterMesh,
    weapon = weaponMesh,
}
assert(relation, errorMessage)
```

The generated module validates the binding names, builds the `attachments.attach` specification,
and returns the plugin's relation/error result. It contains only runtime fields; editor-only mesh
paths, preview poses, selection state, and language settings are not exported.

Its UX follows the interaction animation editor: a file/options menu, an objects panel, an
attachment-properties panel, a camera panel, and a lower preview panel. The viewport uses a 3D
source/target placeholder or mesh pair, XYZ origins, an XZ reference plane, orbit/pan camera
controls, editable source and target authoring poses, mesh loading through the standard file picker,
offset capture, and interaction-style move/rotate gizmos for the selected actor. Bone source mode
adds a read-only selected-bone display and evaluates the selected bind-pose bone through the loaded
source mesh preview. Loading a runtime scene and discovering renderizables remains outside the
editor; the exported module receives those objects through the game's binding table. Each preview actor also has an editor-only
viewport-selection toggle to avoid ambiguous picking when the source and target overlap. When the
source type is `bone`, the editor provides a `Select bone` mode. That mode frames the complete source
mesh in the viewport, hides the target and relation helpers, loads the canonical bind skeleton in
bind/T-pose, and draws large joint spheres and bone segments over the mesh. The user selects the
source bone directly in the viewport; the selected bone name is displayed as read-only editor state
and is saved with the definition. Leaving the mode restores the previous camera and target preview.
There is no free-text bone authoring field.
The Attachment panel also exposes a target-local virtual pivot. The viewport draws that point and
the relation line ends there; zero keeps the mesh origin as the target point.
The Attachments Editor also exposes a `Run` menu with `Save preview as...` and `Run attachment
preview`. The preview is a standalone Lua driver containing the saved source and target asset
references, transform setup, selected skeletal animation, and the same attachment specification.
For a bone source, a selected skeletal clip is required because the runtime contract requires an
active evaluated pose. The `Timeline` panel provides the clip selector, Play, Pause, Resume, and
Stop controls; while a clip is playing, the attached target is refreshed from the evaluated source
bone each frame. It also includes a timeline slider and one-frame step controls. Capture and reset
actions are available in the Objects panel when the target is selected; save and export remain in
the File menu.
Definition version 2 uses the same row-major bind-matrix orientation as the runtime. Loading a
version 1 bone definition migrates its captured offset from the corrected bind pose before the
next save, avoiding a legacy 180-degree orientation error.

### Socket workflow and source choice

Sockets are authored in the **Skeletal Animation Editor**, not in the Attachment Editor. Open the
source `.msh` in the Skeletal Animation Editor, enter **Bone Editor**, open **Sockets**, select the
bone, and add or edit a named socket. The viewport can be put into **Edit sockets in viewport**
mode: bone picking is disabled, socket markers are selectable, and the XYZ gizmo moves the socket
in the bone's local bind space. The rotation rings edit the socket's local quaternion; the socket
marker displays its effective local orientation with colored X/Y/Z axes. Save the mesh before
using it as an attachment source.

The socket is persisted in optional Mesh V11 section 47 (`SECTION_SKELETAL_SOCKETS`). Each record
contains a stable socket ID, a unique name, the referenced bone ID, and local translation,
quaternion rotation, and scale. Socket data is independent of vertex weights and animation tracks.
Mesh Debug's FBX exporter writes sockets as bone-parented Blender Empty objects, and the importer
reconstructs them as canonical sockets. This is why the validated MSH -> FBX -> Mixamo -> FBX ->
MSH workflow preserves sockets.

To consume the socket in the Attachment Editor:

1. Load the mesh containing the socket as the source actor.
2. In the Attachment panel, select **Socket transform**.
3. Choose the socket name from the source socket combo box.
4. Configure position, rotation, and scale inheritance, then capture or edit the attachment
   offset and target pivot as usual.
5. Save the `.att.lua` definition or export the runtime module.

The editor obtains the socket catalog from `getSkeletalSocketReport()` and previews the evaluated
socket with `getSkeletalSocketTransform(name, "world")`. The exported runtime definition stores
`sourceType = "socket"` and `sourceSocket = "Name"`; it does not copy the socket into the
attachment file. The game must therefore bind a mesh that still contains that named socket. At
runtime the attachments plugin reads the evaluated socket pose every frame, exactly as it reads an
evaluated bone pose, and applies the configured inheritance mask and local offset to the target.

Use a **bone source** when the attachment should follow the structural joint itself and the bone
origin/orientation is already the desired anchor. Use a **socket source** when the attachment needs
a named, art-directed point and orientation on or near a bone.

| Source | Advantages | Tradeoffs |
|---|---|---|
| Bone | No extra anchor metadata; every canonical skeleton bone is immediately available; follows the rig's structural joint directly | The bone origin may be inside the mesh or have an inconvenient orientation; each prop needs its own attachment offset; rig edits can change the meaning of the anchor; moving the bone changes the skeleton and can affect deformation |
| Socket | Dedicated local translation, rotation, and scale; can sit at a hand grip, muzzle, hat base, weapon mount, or other art-directed point; multiple sockets can share one bone; moving the socket does not alter weights, bone bind transforms, or animation tracks | Must be authored and kept under a stable name; a missing or renamed socket invalidates the attachment; it still requires a skeletal mesh with an evaluated pose; it is unnecessary for static or articulated meshes that only need a root transform |

Both choices are evaluated from the same animated source mesh and use the same attachment offset,
inheritance flags, and target pivot. A socket is therefore not a replacement for a bone: it is a
stable, editable attachment frame layered on top of the skeleton.

### Look-at constraint

The first additional constraint is `aimAtSource`. When enabled, the target's local +Z axis points
from the target position toward the current source position. The target must inherit rotation;
the constraint uses a world-up reference and falls back to the previous target rotation when the
source and target positions coincide. Position, scale, socket selection, and target pivot keep
their normal attachment semantics. The editor exposes this as **Look at source** and the runtime
Lua definition uses `aimAtSource = true`.

When a constrained target should not turn freely, `aimLimits.enabled = true` clamps the look-at
direction in the attachment frame. `yawMin` and `yawMax` limit the horizontal angle around the
frame's local Y axis, while `pitchMin` and `pitchMax` limit the vertical angle. All four values
are radians. With `reference = "dynamic"`, the frame follows the current source socket or bone;
with `reference = "captured"`, the editor stores a frame at `captureOffset()` and keeps the
angular reference fixed during playback. The same calculation is used by the editor Timeline and
by the runtime plugin, so the preview and exported execution keep the same orientation.

The optional `spring` table smooths inherited position and rotation with a damped spring. It is
disabled by default. `stiffness` controls how quickly the target follows the effective pose and
`damping` reduces oscillation; both values are non-negative and use engine time units. Position
and rotation can be enabled independently.

`aimPoint` is an optional point in the source's local space. When `aimAtSource` is enabled, the
target points toward this point after the source rotation and scale are applied. The default zero
vector preserves the original behavior of aiming at the source origin. This point changes only the
look-at direction; attachment position continues to use the normal source offset and target pivot.
In the Attachment Editor, the point is shown as a pink cube with the same 10% mesh-relative size as
the yaw/pitch handles. Select **Move aim point** to activate the XYZ gizmo and edit the point in
the viewport.

This is intentionally a source-facing constraint, not a general third-actor look-at graph yet.
The next constraint stages can add blending without changing the basic
attachment contract.

## Phase 2 lifecycle guarantees

The engine invokes plugin `onLoop` after scene logic, deferred main-thread work, physics, and audio,
and invokes rendering only afterward. Attachment resolution therefore reads the final source pose
for that frame and writes inherited target components before the render pass. Scene transitions call
`onDestroy` before the plugin list is cleared; the plugin disables all relations, releases registry
references, and leaves surviving Lua attachment objects inert instead of retaining a dangling plugin
pointer. Invalidated renderizables are detected during the next resolution pass.

## Future phases

Socket authoring inside Mesh Debug, IK, blending, physics integration,
cross-scene attachments, and a general scene graph remain future work. Socket consumption already
has the MSH/API/runtime contract described above, and socket authoring is implemented in the
Skeletal Animation Editor.
