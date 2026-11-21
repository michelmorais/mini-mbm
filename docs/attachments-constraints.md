# Attachments and constraints

Document version: **1.0**
Status: **Phase 1 implemented**
Last updated: **2026-11-21**

This document defines the first runtime contract for root-transform attachments. It is independent
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

Phase 1 supports only the `root` transform of existing renderizable objects. An offset is stored in
the source's local space:

```text
world position = source position + source rotation * (source scale * offset position)
world rotation = source rotation * offset rotation
world scale    = source scale * offset scale
```

The source scale in the position expression is used only when `inherit.scale` is enabled. The
source rotation orients the position offset whenever position inheritance is enabled, even when
rotation inheritance is disabled. Position, rotation, and scale are written independently; a
component whose inheritance flag is false is left untouched.

Angles use radians, matching `RENDERIZABLE::getAngle()` and the regular Lua renderizable API.
Composition uses matrices internally and converts back to Euler XYZ radians only when writing the
engine transform. Non-uniform scale is supported for this root-transform composition because the
offset position is scaled before source rotation. Skeletal bones, sockets, and sheared transforms
are not part of this contract.

## Lua API

```lua
local attachment, err = attachments.attach {
    id = "hat_on_character", -- optional; generated when omitted
    source = character,
    target = hat,
    sourceType = "root", -- optional; this is the only phase 1 value
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
cycles, duplicate component ownership on one target, and unsupported source types. It returns
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
| `getState()` | table | Returns active/invalid status, inheritance flags, and offset. |

`detach()` preserves the target's current pose by default. Passing
`{preserveWorldPose = false}` restores the target pose captured immediately before `attach`.
Destroying a source, target, or scene deactivates the relation and releases its Lua references.

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

`editor/attachments_editor.lua` is an independent, definition-only authoring skeleton. Its saved
`.att.lua` file contains source/target labels, inheritance flags, and the local offset; it does not
store runtime objects. The editor has its own state and does not require or modify
`interaction_animation_editor.lua` or `interaction_animation_runtime`.

The editor can edit and visualize a relation with preview markers, capture a preview offset, and
save/load the definition. Loading a runtime scene and resolving labels to actual renderizables is
deliberately left as the next authoring integration step.

## Future phases

Bones, sockets, skeletal-pose sampling, IK, aim constraints, spring/damping, physics integration,
cross-scene attachments, and a general scene graph remain out of scope for Phase 1.
