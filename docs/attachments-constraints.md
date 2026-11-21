# Attachments and constraints

Document version: **1.5**
Status: **Phase 4 runtime export and binding bridge implemented; sockets pending**
Last updated: **2026-11-21**

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

## Transform contract

The source can be the `root` transform of a renderizable object or the evaluated world transform of
a named skeletal bone on a mesh. An offset is stored in the selected source's local space:

```text
world position = source position + source rotation * (source scale * offset position)
world rotation = source rotation * offset rotation
world scale    = source scale * offset scale
```

The source scale in the position expression is used only when `inherit.scale` is enabled. The
source rotation orients the position offset whenever position inheritance is enabled, even when
rotation inheritance is disabled. Position, rotation, and scale are written independently; a
component whose inheritance flag is false is left untouched. Bone sources require an active,
evaluated skeletal pose; sockets are not supported because the mesh format has no socket contract
yet.

Angles use radians, matching `RENDERIZABLE::getAngle()` and the regular Lua renderizable API.
Composition uses matrices internally and converts back to Euler XYZ radians only when writing the
engine transform. Non-uniform scale is supported because the offset position is scaled before
source rotation. Sheared transforms are not part of this contract.

## Lua API

```lua
local attachment, err = attachments.attach {
    id = "hat_on_character", -- optional; generated when omitted
    source = character,
    target = hat,
    sourceType = "bone", -- "root" or "bone"; defaults to "root"
    sourceBone = "mixamorig:RightHand", -- required when sourceType is "bone"
    inherit = {
        position = true,
        rotation = true,
        scale = false,
    },
    offset = {
        position = {x = 0, y = 1.8, z = 0},
        rotation = {x = 0, y = 0, z = 0}, -- radians
        scale = {x = 1, y = 1, z = 1},
    },
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

Lua references to the source and target tables keep them alive while an attachment is active. An
explicit renderizable destroy invalidates its `USER_DATA_RENDER_LUA` table reference; the next update
detects that state and disables the attachment. Scene teardown disables all relations and releases
their references.

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

## Phase 2 lifecycle guarantees

The engine invokes plugin `onLoop` after scene logic, deferred main-thread work, physics, and audio,
and invokes rendering only afterward. Attachment resolution therefore reads the final source pose
for that frame and writes inherited target components before the render pass. Scene transitions call
`onDestroy` before the plugin list is cleared; the plugin disables all relations, releases registry
references, and leaves surviving Lua attachment objects inert instead of retaining a dangling plugin
pointer. Invalidated renderizables are detected during the next resolution pass.

## Future phases

Sockets, IK, aim constraints, spring/damping, physics integration, cross-scene attachments, and a
general scene graph remain out of scope. Socket support requires a mesh-format/API contract.
