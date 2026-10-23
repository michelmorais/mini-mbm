# Real-Time Facial Animation - Phase 5 live capture naming contract

This slice defines the producer boundary before adding a webcam, OSC, or vendor SDK dependency.
Capture systems send a Lua table whose keys are source target names and whose values are finite
weights. editor/facial_capture_mapping.lua resolves those names against the targets of one loaded
mesh and calls the existing mesh:setFacialWeight(name, value) API.

## Mapping rules

Mapping.build(mesh, options) snapshots the mesh target names and returns a reusable index. Names
are compared after trimming, lowercasing, and removing spaces, underscores, dots, colons, slashes,
and hyphens. Canonical ARKit-style names therefore resolve directly, while conservative Faceit
side-suffix forms such as EyeBlink_L, EyeBlink_R, mouthSmile_L, and mouthSmile_R resolve to
their Left/Right target names. Unknown names are reported instead of being guessed.

options.requiredTargets is optional. When supplied, names absent from the mesh are listed in
index.missingTargets; this is useful for deciding whether a capture profile is compatible before
starting it. options.epsilon defaults to 0.0001.

~~~lua
local Mapping = require 'facial_capture_mapping'
local index = Mapping.build(face, {
    requiredTargets = {'jawOpen', 'eyeBlinkLeft', 'eyeBlinkRight'},
})

local result = Mapping.apply(face, index, {
    jawOpen = 0.35,
    EyeBlink_L = 0.8,
    EyeBlink_R = 0.1,
})
if result.failed > 0 or #result.missingAliasTarget > 0 then
    -- Stop or mark the capture source as incompatible.
end
~~~

Mapping.apply clamps values to [0,1], rejects non-finite values, and applies the exact source
name when both an exact key and an alias target are present. It reports mapped, changed,
unchanged, clamped, rejected, unknownSource, duplicateSource, and failed. The optional
clearMissing = true mode resets previously applied targets that are absent from a sparse packet;
the default keeps them unchanged, which is safer for producers that send deltas.

Mapping.reset(mesh, index) clears the runtime manual overrides and the cached values. The index
must be rebuilt after the mesh asset changes or its facial target list is replaced.

## Validation

The pure Lua contract test runs with the bundled interpreter:

~~~sh
bin/debug/linux_x86/lua-5.4.1.exe src/test-lib/facial_capture_mapping_test.lua
~~~

It covers canonical names, Faceit side aliases, required-target diagnostics, exact-name
precedence, clamping, non-finite rejection, sparse updates, clearing missing values, and mesh
discovery. No capture device or network service is required.

This milestone deliberately does not claim a live capture producer. The deterministic file-backed
producer and replay lifecycle are defined in Phase 6; a future OSC or webcam adapter can produce
the same packet table without changing this mapping contract.
