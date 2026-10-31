# Real-Time Facial Animation - Future Work

The current facial runtime, Mesh V11 format, Lua API, and native authoring workflow are described in:

- [Facial Rig Editor](facial-rig-editor.md)
- [Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md#facial-composition-on-the-same-mesh)
- [Mesh V11 format](mesh-v11-format.md#facial-sections-45-46-version-1)
- [Lua API - Facial targets and clips](lua-api.md#facial-targets-and-clips-mesh)

The native morph-target authoring workflow is available. The items below are the remaining work;
they are independent extensions unless a dependency is stated explicitly.

## Sculpt quality and feedback

- Add an explicit choice to recompute normal deltas from the deformed surface or preserve authored
  normal deltas. Check both modes at neutral, partial, and full target weight.
- Show the textured deformation while a batch transform or pivot rotation is being adjusted, before
  Apply. Preserve the current behavior in which Apply commits the target deltas.
- Improve control over mouth-corner and other narrow-region corrections. Evaluate surface-aware
  smoothing that preserves an intentional lip opening; do not assume a selection mask can separate
  vertices that share one mesh index.
- Evaluate face-region selection presets and a direct in-memory candidate preview bridge. These are
  workflow and performance improvements, not prerequisites for saving a target.

Acceptance: an artist can inspect the intended deformation and its normals before committing it,
then export and reload the same result without changing vertex order or topology. Idle editor frames
must not rebuild the sculpt surface, serialize an MSH, or scan the full vertex set without an input
change.

## Expression and clip acceptance

- Author one morph-only `jawOpen` expression and a facial clip using its named target weight.
  Verify editor preview, project save/reload, MSH export/reload, and runtime playback.
- Repeat with a blink and an asymmetric expression. Check target references after rename, delete,
  duplicate, and mirror operations.
- Inspect target results at weights 0, 0.5, and 1. For jaw opening, inspect the lip corners, chin,
  neck, nape, and upper face. If a gap cannot be formed, inspect the mesh topology and repair the
  source asset where vertices are genuinely shared.

Acceptance: the exported MSH retains target identities, deltas, defaults, clip keys, and existing
skeletal data. A fresh runtime instance plays the clips with the documented morph-then-skin
composition. Invalid project references are reported before export.

## Imported-project reference diagnostics

Extend project-load and export diagnostics for stale target names or IDs in expressions and clips
from previously authored projects. Keep the existing target-level reference inspection and
rename/delete migration. Report which expression or clip holds each unresolved reference and
provide an explicit repair path; do not silently redirect it to another target.

Acceptance: an imported project with missing target references gives actionable diagnostics and
cannot export an apparently valid clip with the wrong target mapping.

## Optional bone-driven facial expressions

Add this only when an asset needs a jaw or head bone pose in the facial expression workflow. Reuse
the canonical bound skeleton and keep facial weights and skeletal transforms as distinct channels.
Provide a visible pivot/influence context, bone rotation preview, and deliberate translation control.

Before a paired expression or a single playback command is exposed, define how facial and skeletal
clips are identified, synchronized, looped, mixed, and reset. Preserve morph-only expression and
clip behavior. Do not bake a bone rotation into morph deltas: the existing runtime applies morphing
before skinning.

Acceptance: a paired expression reproduces the same result after project and MSH round-trips, and
manual facial weight overrides have a documented interaction with its bone pose.

## Live capture and SDK integration

Add optional capture adapters outside the core asset format. An OSC adapter can establish the
transport contract before a vendor SDK integration. Define connection state, packet timeout,
calibration, neutral reset, target-name diagnostics, and optional recording to a facial clip. Keep
shape animation and head/bone animation distinct, with explicit replacement or mixing rules.
The [Faceit Live Link Face workflow](https://faceit-doc.readthedocs.io/en/latest/mocap_importers/#live-link-face)
is a workflow reference, not an engine dependency.

Acceptance: capture can drive the existing named facial weights, recover to neutral after timeout,
and record/replay deterministic clip data without a Mesh V11 format change.

## Platform and material compatibility

- Complete facial runtime acceptance on supported platforms without a documented GPU/CPU morph
  verification, including iOS Metal. Preserve the CPU reference and fallback behavior.
- Define shader inputs, lighting interactions, validation, and fallback behavior before enabling
  custom shader or normal-mapped facial meshes.

Acceptance: equivalent facial weights and skeletal poses produce the same supported deformation
contract across the accepted backends. When a future change alters a binary section or Lua API,
update the format/API reference and `include/version/version.h` in the same change.
