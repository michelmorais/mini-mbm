# Mini MBM Interaction Animation Runtime

Interaction animation coordinates a sequence involving two or more actors. It
is useful for authored reactions such as a character using an object, a hit
reaction, or a synchronized exchange between meshes.

The workflow has two parts:

1. Create and export a definition with the Interaction Animation Editor.
2. Load that definition in the game and bind its mesh actors to live `mesh`
   objects with the `interaction_animation_runtime` plugin.

The runtime is included whenever the Lua build is enabled. On platforms where
plugins are embedded, the engine resolves the same module through its embedded
`require` loader.

## Basic usage

```lua
local definition = require "my_interaction"
local runtime = require "interaction_animation_runtime"

local interaction = runtime.start {
    definition = definition,
    actors = {
        actor1 = machineMesh,
        actor2 = characterMesh,
    },
    callbacks = {
        onStart = function(instance) end,
        onEvent = function(instance, event, index) end,
        onFinish = function(instance) end,
        onUpdate = function(instance, delta)
            -- Optional synchronization of objects outside the actor list.
        end,
    },
}
```

`runtime.start(config)` creates and starts an interaction instance. The engine
advances it automatically through the plugin loop; the game must not call an
`update` method every frame.

Every mesh actor required by the definition must be supplied in `actors`. The
keys may use the exported slot name, such as `actor1`, or its underscore alias,
such as `actor_1`. Placeholders used by the editor are virtual and do not need a
runtime mesh object.

## Instance control

```lua
interaction:restart()       -- restore the captured base state and replay
interaction:stop()          -- stop, restore the base state and finish
local done = interaction:isFinished()
local pose = interaction:getActorPose("actor1")
interaction:rebaseToAnchor() -- optional transform rebasing
```

The `onEvent` callback receives the instance, the exported event table and its
one-based event index. Events are dispatched at lifecycle or event boundaries,
not once per interpolation step.

## Authoring reference

The project format, actor slots, timeline, transform/bezier/play/notify/
visibility events and the editor workflow are documented in
`interaction-animation-editor.md`. The complete Lua API contract is also in
the Interaction Animation Runtime section of `lua-api.md`.

This feature is independent of Box2D and Bullet. It changes the transforms and
animation state of bound meshes; collision and gameplay rules remain the
responsibility of the game.
