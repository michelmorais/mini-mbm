# Attachments

This document describes the attachment editor, the `attachments` Lua plugin, the
definition exported by the editor, and the runtime transform contract.

## Runtime plugin

The plugin is loaded from Lua with:

```lua
local attachments = require "attachments"
```

It is enabled with `USE_ATTACHMENTS=1`. `USE_ALL=1` enables it together with the
other optional Lua features. Desktop builds load the shared plugin; static
platform builds link the same module into the application.

The plugin reads the source pose and writes the inherited components of the
target during `PLUGIN::onLoop`. It does not load assets, discover scene objects,
or require a Lua callback every frame. The game supplies the source and target
objects explicitly.

### Embedded loading on mobile platforms

Android and iOS builds cannot depend on the normal runtime Lua shared-library
lookup for every plugin. The engine therefore links the selected plugin into
the application and registers its Lua entry point in
`src/lua-wrap/require_embedded.cpp`.

When `USE_ATTACHMENTS=1` is enabled for Android, iOS, or the macOS App Store
profile, that loader includes `attachments-lua.h` and maps both module names to
the same entry point:

```text
attachments     -> luaopen_attachments
libattachments  -> luaopen_attachments
```

`framework-lua.cpp` installs a wrapper around Lua's `require`. It first asks the
embedded loader for the module and falls back to the normal Lua loader when the
module is not embedded. Application Lua code therefore keeps the same form on
all supported platforms:

```lua
local attachments = require "attachments"
```

To embed another plugin, its platform build must define the corresponding
`USE_*` symbol, link the plugin target into the application, include the plugin
Lua header in `require_embedded.cpp`, and map its module name to its exported
`luaopen_*` function. Plugins keep the standard shared-library path on desktop
builds.

## Editor

The authoring tool is `editor/attachments_editor.lua`. It is independent from
`interaction_animation_editor.lua`, although it follows the same general editor
workflow.

An editor project is saved as a `.att.lua` file. It contains the runtime
definition and an `editor` table with preview-only data such as mesh paths,
preview poses, selection flags, the selected language, and timeline state. The
runtime exporter omits the `editor` table.

The editor provides:

- source and multiple target objects;
- root, skeletal bone, skeletal socket, articulated-part, and static-frame
  source modes;
- independent position, rotation, and scale inheritance per target;
- local offsets and a target-local virtual pivot;
- look-at, yaw/pitch limits, spring, blend, influence, and state profiles;
- viewport selection, move/rotate gizmos, relation lines, origin markers, and
  an XZ reference plane;
- attachment graph validation and visualization;
- a Timeline panel for animated source previews and static-frame offsets;
- `Run -> Run attachment preview` and `Run -> Save preview as...`;
- `File -> Export require` for the runtime module.

The editor uses runtime labels for source and target names. These names are not
mesh filenames and are resolved by the game binding table.

### Multiple targets and chains

One project has one root source and one or more targets. Every target stores its
own source label, offset, target pivot, inheritance mask, and constraint
settings. A target may use the root source or another target as its source.
Chains such as the following are supported:

```text
character -> hand_attachment -> sword -> effect
```

When a target follows another target, the runtime relation uses the target's
root transform. Bone, socket, articulated-part, and static-frame source modes
apply to relations driven directly by the root source mesh.

The graph panel reports empty or duplicate names, missing sources, self-links,
and cycles. These errors block preview execution and require correction before
runtime export.

### Skeletal bones and sockets

For a skeletal bone source, the editor provides a `Select bone` viewport mode.
It frames the source mesh, hides the other preview actor, loads the bind/T-pose
skeleton, and allows the user to select the bone directly in the viewport.

Sockets are authored in the Skeletal Animation Editor, in the Bone Editor
worktree. A socket has a name, a parent bone, and local translation, rotation,
and scale. The socket viewport mode displays socket markers and allows their
local position and rotation to be edited with the XYZ gizmo and rotation rings.
The socket marker also displays its local orientation with colored axes.

Sockets are stored in optional Mesh V11 section 47. They do not modify skinning
weights or animation tracks. Mesh Debug FBX export writes sockets as
bone-parented Blender Empty objects, and FBX import reconstructs them as
sockets.

To use a socket in an attachment:

1. Save the socket in the Skeletal Animation Editor.
2. Load the mesh as the attachment source.
3. Select `Socket transform` in the Attachment panel.
4. Select the socket name.
5. Configure the target offset and inheritance, then capture the offset.

At runtime, the plugin evaluates the named socket from the current skeletal
pose. A bone is the structural joint itself; a socket is an art-directed local
frame that can be placed and oriented without changing the skeleton.

### Articulated parts

An articulated source is a mesh composed of named rigid subsets and articulated
clips. It does not use skeletal bones or skin weights. The editor provides a
`Select part` viewport mode that frames the source mesh and selects the subset
under the cursor. The selected subset pivot is used as the attachment anchor.

Each target can select a different articulated subset of the same source mesh.
The source mesh must contain the selected part and the active articulated clip
when the runtime relation is created.

### Static-frame animation

For `sourceType = "frame"`, the source mesh plays a static-frame animation and
the target has an offset for each authored frame. The Timeline can select,
pause, play, and step through the frames. `Capture offset` stores the current
target pose for the current frame.

The editor option `Preserve target transform on uncaptured frames` carries the
previous target pose into a newly selected frame and creates its offset. This
is an authoring convenience. The exported runtime module only contains the
resulting `frameOffsets` table.

## Exported `require` module

`File -> Export require` writes a flat Lua module in the same directory as the
project. For a project named `character.att.lua`, the default output is:

```text
character-attachment.lua
```

The game loads the module with:

```lua
local runtime = require "character-attachment"

local relations, errorMessage = runtime.attach {
    character = characterMesh,
    hat = hatMesh,
    propeller = propellerMesh,
}
assert(relations, errorMessage)
```

The exported module contains only runtime data and an `attach(bindings)`
function. The binding keys must match the source and target names authored in
the editor. It returns one attachment object per target, or `nil, errorMessage`
if a binding or runtime relation is invalid. If creation of a later relation
fails, relations already created by that call are detached before the error is
returned.

The generated module requires the plugin itself:

```lua
local attachments = require "attachments"
```

The editor preview uses the same exported definition and plugin API in a
separate engine instance.

## Definition format

The exported definition has one root source and a target collection:

```lua
{
    source = "character",
    sourceType = "root", -- root, bone, socket, articulated_part, or frame
    frameAnimation = "run", -- required only for sourceType = "frame"
    influenceEnabled = true,
    initialState = "equipped",
    states = {
        equipped = {
            influenceEnabled = true,
            blend = {enabled = true, weight = 1},
        },
        released = {influenceEnabled = false},
    },
    targets = {
        {
            name = "hat",
            source = "character",
            sourceBone = "Head",       -- bone source
            sourceSocket = "HatMount", -- socket source
            sourcePart = "hat_mount",  -- articulated-part source
            inherit = {position = true, rotation = true, scale = false},
            aimAtSource = false,
            aimPoint = {x = 0, y = 0, z = 0},
            aimLimits = {
                enabled = false,
                reference = "dynamic", -- dynamic or captured
                yawMin = -math.pi,
                yawMax = math.pi,
                pitchMin = -math.pi * 0.5,
                pitchMax = math.pi * 0.5,
            },
            spring = {
                enabled = false,
                position = true,
                rotation = true,
                stiffness = 120,
                damping = 24,
            },
            blend = {
                enabled = false,
                position = true,
                rotation = true,
                scale = true,
                weight = 1,
            },
            offset = {
                position = {x = 0, y = 1.8, z = 0},
                rotation = {x = 0, y = 0, z = 0},
                scale = {x = 1, y = 1, z = 1},
                rotationMatrix = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
            },
            targetPivot = {x = 0, y = 0, z = 0},
            frameOffsets = {}, -- only for sourceType = "frame"
        },
    },
}
```

The editor writes `frameAnimation` at the definition level. The generated
module passes that value to the direct plugin API as `sourceAnimation`.
`frameOffsets` uses positive, one-based animation frame numbers. A frame without
an entry uses the target's base `offset`.

## Transform contract

Offsets are stored in the local coordinate frame of the selected source. The
runtime applies the relation as follows:

- inherited position uses the source position plus the source rotation applied
  to the local offset;
- inherited source scale also scales the local position offset;
- inherited rotation composes the local offset rotation with the source
  rotation;
- inherited scale multiplies source scale by offset scale;
- the target pivot is a point in target local space and is subtracted from the
  resulting target position after target rotation and scale are applied;
- a component whose inheritance flag is false is left at the target's current
  value.

Angles use radians. The runtime composes rotations as matrices and converts to
Euler XYZ radians only when writing the renderizable transform. The optional
`rotationMatrix` emitted by the editor is the authoritative local offset
rotation when present. The composition follows the engine's row-vector matrix
convention.

`aimAtSource` makes the target's local +Z axis point toward the current source
position or toward `aimPoint` expressed in source-local coordinates. It requires
rotation inheritance. `aimLimits` clamps yaw and pitch in radians. `dynamic`
uses the current source frame; `captured` uses the reference matrix captured by
the editor or by `captureOffset()`.

`blend` interpolates the selected inherited components between the current
target pose and the attachment pose. Its weight is in the range 0..1. Rotation
blending uses the shortest angular path.

`spring` applies damped position and rotation following for the enabled
components. `stiffness` and `damping` must be finite and non-negative. Spring
integration is reset when the attachment state or relevant configuration
changes.

State profiles contain `influenceEnabled` and optional partial `blend` values.
`initialState` selects the profile at creation, and `setState()` changes the
active profile at runtime.

## Direct Lua API

The direct API creates one relation at a time:

```lua
local relation, errorMessage = attachments.attach {
    id = "character_hat",
    source = characterMesh,
    target = hatMesh,
    sourceType = "bone",
    sourceBone = "Head",
    inherit = {position = true, rotation = true, scale = false},
    offset = {
        position = {x = 0, y = 0.4, z = 0},
        rotation = {x = 0, y = 0, z = 0},
        scale = {x = 1, y = 1, z = 1},
    },
}
assert(relation, errorMessage)
```

For a socket use `sourceType = "socket"` and `sourceSocket`. For an articulated
part use `sourceType = "articulated_part"` and `sourcePart`. For a static-frame
source use `sourceType = "frame"`, `sourceAnimation`, and optional
`frameOffsets`.

The attachment object exposes:

| Method | Result | Description |
|---|---|---|
| `detach(options?)` | boolean | Detaches the relation. `preserveWorldPose` defaults to `true`. |
| `isActive()` | boolean | Reports whether the relation is active. |
| `getId()` | string | Returns the attachment identifier. |
| `getOffset()` | table | Returns offset vectors, matrix, and target pivot. |
| `setOffset(offset)` | boolean or `nil, error` | Changes the local offset and pivot. |
| `setInherit(inherit)` | boolean or `nil, error` | Changes the inheritance mask after validation. |
| `getBlend()` | table | Returns the current blend settings. |
| `setBlend(blend)` | boolean or `nil, error` | Updates blend settings. |
| `setState(name)` | boolean or `nil, error` | Selects a configured state profile. |
| `getStateName()` | string | Returns the active state name. |
| `getStates()` | table | Returns configured state names. |
| `setInfluenceEnabled(enabled)` | boolean or `nil, error` | Suspends or resumes target writes. |
| `isInfluenceEnabled()` | boolean | Reports whether target writes are enabled. |
| `captureOffset()` | boolean or `nil, error` | Captures the current target pose relative to the current source pose. |
| `getEffectivePose()` | table or `nil, error` | Returns the currently calculated target pose. |
| `getState()` | table | Returns status, source settings, offsets, constraints, and effective pose. |

`detach()` preserves the current world pose by default. With
`{preserveWorldPose = false}`, it restores the target pose recorded when the
relation was created.

## Validation and lifecycle

`attachments.attach` rejects:

- missing or invalid source and target renderizables;
- source and target from different scenes;
- source and target referring to the same renderizable;
- unsupported or malformed source types;
- missing or unevaluated bones, sockets, articulated parts, or frame
  animations;
- invalid vectors, matrices, limits, spring values, or blend weights;
- duplicate target component ownership;
- duplicate attachment identifiers;
- dependency cycles.

Active relations are resolved in a stable topological order. This allows an
attachment target to be the source of another attachment. A relation that
becomes invalid during the update loop is deactivated and no longer writes to
its target.

The plugin retains Lua references to the source and target tables while a
relation is active. Scene teardown deactivates all relations and releases those
references. Destroyed or invalidated renderizables are detected during
resolution and invalidate their relations without leaving dangling references.

The plugin owns only the components selected by the inheritance mask. It does
not arbitrate writes made by unrelated gameplay, physics, or animation code;
the game must avoid external writers that compete for the same target
components.
