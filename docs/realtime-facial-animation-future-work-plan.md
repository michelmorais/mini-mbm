# Real-Time Facial Animation - Future Work

The current facial runtime, Mesh V11 format, Lua API, and native authoring workflow are described in:

- [Facial Rig Editor](facial-rig-editor.md)
- [Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md#facial-composition-on-the-same-mesh)
- [Mesh V11 format](mesh-v11-format.md#facial-sections-45-46-version-1)
- [Lua API - Facial targets and clips](lua-api.md#facial-targets-and-clips-mesh)

The native morph-target authoring workflow is available. The items below are the remaining work;
they are independent extensions unless a dependency is stated explicitly.

## Expansion decisions: semantic facial actions and generic morph regions

The project will not depend on a paid Live Capture SDK. Capture adapters remain optional and
external to the core asset format. An open transport such as OSC, recorded files, or another
adapter can be evaluated later without changing the authoring model.

FACS is an authoring and semantic layer, not a muscle simulator. An Action Unit describes an
observable facial action associated with one or more muscles or muscle groups. The editor must
therefore map an authored Action Unit to one or more real morph targets, with explicit weights,
side, intensity range, response curve, and optional secondary target contributions. The `FACS`
target convention remains a label for compatibility; it must not imply that the runtime solves
muscle anatomy or automatically generates deformation.

The generic deformation model is:

```text
surface region -> authored morph target -> FACS Action Unit -> expression or clip
```

The existing MSH facial target and clip payload is already geometrically generic: it stores vertex
deltas and target weights, not facial anatomy. The first generic-morph milestones may reuse that
payload and existing runtime APIs while the editor-facing naming and project modules are extracted
from the facial-only workflow.

## Phase 1 - dual mask authoring in the Facial Rig Editor

The current facial editor will initially support two mask editing modes selected by a radio button:

| Mode | Authoring representation | Compatibility |
| --- | --- | --- |
| Legacy vertex mask | Explicit list of canonical frame-0 vertex indices | Existing projects and existing rig/target workflows remain valid |
| Surface polygon mask | Editable closed polygon with draggable points anchored to the assigned mesh surface | New authoring path; derived vertex weights are generated from the polygon |

The radio button selects which representation is edited. It must not silently convert a legacy mask
to a polygon or a polygon to a legacy mask. A project may contain both types, but each mask has one
authoritative representation and an explicit type. Existing legacy masks load as legacy masks.

The polygon path should support the following initial operations:

- create a polygon by placing points on the visible mesh surface;
- drag existing points, insert a point on an edge, remove a point, and close the polygon;
- preview the polygon boundary and its derived influence on the mesh;
- apply binary or soft vertex weights using a documented falloff;
- invalidate the derived weights when the polygon or source topology changes;
- undo and redo the complete polygon edit as one authoring operation where appropriate.

For the first implementation, polygon points may be anchored to canonical mesh vertices. A later
extension may anchor a point to a triangle plus barycentric coordinates when a point must lie
between existing vertices. The serialized polygon must retain the source topology signature and
the derived weights must be treated as rebuildable data, not as the primary authoring source.

The bridge to Blender must carry the complete polygon definition and its derived vertex weights.
For parity, it should also carry the explicit vertex list when a legacy mask is exported as a
region. The current binding description only reports mask counts, so the bridge schema must be
extended before Blender can consume either representation as a precise region. Existing editor and
rig-binding behavior for legacy vertex masks must remain unchanged during that extension.

Acceptance:

- old facial projects load and save without changing legacy mask semantics;
- the radio button clearly identifies the active mask representation;
- editing one representation does not mutate the other representation implicitly;
- a polygon can be used as a target-selection region and as a Blender authoring region;
- stale topology produces an actionable diagnostic and never applies weights silently;
- a polygon's preview and derived weights are idle-cached and rebuilt only after an input change.

## Phase 2 - generic surface regions and Morph Editor

Extract the reusable parts of facial target authoring into a generic morph-authoring core. A generic
region should be usable by any MSH-backed object, not only a face. Its conceptual data is:

```lua
region = {
    name = "brow_left",
    kind = "polygon", -- point, polyline, or polygon
    topologySignature = "...",
    points = { ... }, -- authoritative surface anchors
    derivedVertexWeights = { ... }, -- rebuildable result
    falloff = { mode = "surface", radius = 2, curve = "smooth" }
}
```

The initial generic editor should reuse target catalog, transforms, painting, smoothing, masking,
and mirroring without requiring facial landmarks, facial roles, or FACS names. A `polygon` is an
area influence, a `polyline` is a line-of-action influence, and a `point` is a localized control.
Concave polygons must be supported for selection; triangulation used for display must not change
the meaning of the influence field.

The facial editor should consume this generic core while retaining its facial-specific worktrees.
The runtime format remains unchanged in this phase; the semantic distinction belongs to project
metadata and editor modules until a runtime semantic API is proven necessary.

Acceptance: a non-facial MSH can create, preview, save, reload, export, and play a morph target
using the same region tools, without requiring face origin, facial landmarks, or facial roles.

## Phase 3 - FACS authoring profiles

Add an editor catalog for Action Units and authorable profiles. An Action Unit profile should define:

- semantic name and optional left/right side;
- normalized intensity range;
- one or more output target references;
- per-output weight, response curve, and optional clamp;
- optional secondary contributions for neighboring facial regions;
- diagnostics when a referenced target or region is missing.

Example:

```lua
actionUnits = {
    {
        name = "AU1_inner_brow_raise_L",
        side = "left",
        outputs = {
            {target = "brow_inner_raise_L", weight = 1.0},
            {target = "forehead_tension_L", weight = 0.2}
        }
    }
}
```

FACS profiles are evaluated into ordinary target weights before expression or clip export. The
first implementation must preserve the existing runtime composition and MSH format. If multiple
profiles affect the same target, the project must define the composition rule explicitly (for
example additive with saturation, maximum, or priority); it must not rely on accidental floating
point accumulation.

Acceptance: an artist can author a brow, eye, or mouth Action Unit, inspect its primary and
secondary target effects, combine it with another Action Unit, and export the result as the existing
named target/clip data. Missing references block export with an actionable diagnostic.

## Phase 4 - open capture adapters

Keep capture adapters independent from the target asset. An adapter produces normalized Action Unit
or named-target values, and the same FACS profiles used by the editor resolve those values to mesh
weights. OSC and recorded data are valid first adapters; no vendor SDK is required.

The adapter contract must define connection state, packet timeout, neutral reset, calibration,
target/action diagnostics, replacement versus mixing, and deterministic recording/replay. A timeout
must recover to the documented neutral or fallback state without leaving stale facial weights active.

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
