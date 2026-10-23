# Real-Time Facial Animation - Phase 2

Phase 2 adds per-instance controls for the facial data persisted by Phase 1. The cached mesh keeps
immutable target and clip arrays; each `MESH` owns its current weights, manual override mask, active
clip, time, pause state, speed, and evaluation counters.

## Delivered

- Target enumeration by 1-based index, stable ID, name, convention, and default weight.
- Manual weights by index or unique name, finite-value validation, `[0,1]` clamping, one-target
  clearing, and reset of all overrides.
- Clip enumeration by name and duration.
- Play, pause, resume, stop, seek, and non-negative playback speed controls.
- Step and linear sampled-key evaluation with exact duration clamping and loop wrapping.
- Deterministic composition: defaults -> sampled clip weights -> manual overrides.
- Runtime report fields for active clip, time, duration, speed, readiness, evaluation/upload counts,
  execution path, and status.
- Lua bindings documented in [Lua API](lua-api.md#facial-targets-and-clips-mesh-phase-2).

Stopping a clip removes only its sampled contribution and retains manual overrides. Clearing an
override restores the current clip value, or the target default when no clip is active. Playback
state is isolated between instances sharing one cached mesh asset.

## Validation

The existing facial foundation suite now also validates sampled linear interpolation and duration
clamping. `testLib` and the full `mini-mbm` Lua launcher both compile with the new public surface;
`git diff --check` is clean. The interactive runtime fixture exercises instance isolation and the
control transitions, but requires a working X11 display and cannot start in the headless validation
environment when display `:1` is unavailable.

## Scope boundary

The Lua surface is available for loaded `mesh` objects. Name enumeration returns `nil` for invalid
indices, while mutating controls return `false` for unloaded meshes or unknown targets/clips. Clip
transitions and blending between facial clips remain future work. Blender/Faceit import, the Mesh
Debug preview, and `.blend` reverse export are covered by
[Phase 3](realtime-facial-animation-phase3.md); reliable FBX facial-curve transport remains future
work.
