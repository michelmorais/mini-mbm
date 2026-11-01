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
surface control -> generated region -> authored morph target -> FACS Action Unit -> expression or clip
```

The existing MSH facial target and clip payload is already geometrically generic: it stores vertex
deltas and target weights, not facial anatomy. The first generic-morph milestones may reuse that
payload and existing runtime APIs while the editor-facing naming and project modules are extracted
from the facial-only workflow.

## Phase 1 - facial surface-control authoring and region generation

The authoring source is **Landmarks**, not **Masks / Regions**. After the face origin is positioned,
the editor loads a pre-defined facial-action template and the user adapts its controls directly on
the visible mesh:

| Kind | Intended use | Initial semantics |
| --- | --- | --- |
| polygon | Area influence, such as a brow, cheek, eyelid, or jaw area | Closed ordered surface anchors with editable contour and depth |
| polyline | Line of action, such as a lip border, furrow, or wrinkle | Open ordered surface anchors with editable contour and depth |
| point | Landmark or controller for a localized action | Explicit references to the polygons/polylines it controls |

The user must be able to select an action mask, drag its pre-authored surface anchors, adjust their
depth, edit contours, and inspect the related action lines in the same facial-mark authoring view.
The selected mask exposes its polygon, action-line polyline, group controller, and action-line
controller as one editable group.
All other masks remain visible as colored outlines, while only the selected mask exposes clickable
square anchors. The group controller moves its area and action lines together; the action-line
controller moves only its action lines.
The editor offers independent and synchronized side editing for paired facial masks; synchronized
editing mirrors the authored positions while retaining each side's mesh anchors. Stock influence
areas share one cyan color and stock action lines share one orange color across sides.
The face guide remains a placement aid and must not be confused with an authored control or a
legacy vertex mask.
The current catalog includes paired brows, eyes, cheeks, mouth corners, and nasolabial folds, plus
upper-lip, lower-lip, chin, jaw, and nose masks. Eye templates also expose an upper and lower
eyelid action line. New facial projects receive all 15 masks after face-origin placement. Saved
projects retain their authored catalog without automatic additions. Chin and jaw-hinge point
landmarks already exist as optional rig references; their surface influence masks are separate.
AU labels on these templates are semantic hints. The editor now
derives binary front-surface regions explicitly from closed area polygons, tracks source edits and
topology changes, and can copy a current region into an independent Morph Target selection mask.
Surface falloff, rig-weight application, Blender transport, and AU-to-target profiles remain
future milestones.

The initial point-controller model is explicit: a controller lists the controls it affects, with
optional per-control contribution values. An optional spatial-inference command may later suggest
related controls based on proximity, but the user must explicitly invoke and confirm it. Inference
must produce inspectable relationships and must not overwrite explicit links silently.

**Masks / Regions** is the derivation stage. It consumes authored controls and generates named
regions, vertex weights, and any legacy-compatible selections required by existing rig or target
workflows. A generated region records its source-control identity, source topology signature, and
generation state. Editing a source control marks dependent regions stale; regeneration is explicit
and must be reviewed before export or weight application.

Legacy vertex masks remain loadable and editable for old projects. They are not converted
automatically into polygons, polylines, or points. New projects start with surface-control
authoring; a legacy mask is an explicit compatibility choice.

The interim polygon-mask editor remains under **Masks / Regions** for compatibility. Generated
regions are stored separately from these masks, and copying one into a target-selection mask is
explicit; no automatic conversion or weight application occurs.

The first surface-anchor implementation may use canonical mesh vertices. A later extension may
anchor a point to a triangle plus barycentric coordinates when it must lie between existing vertices.
Derived weights are rebuildable data and are never the primary authoring source.

The bridge to Blender must eventually carry surface controls, generated regions, and their topology
identity. Existing legacy vertex lists remain available for compatibility exports.

Acceptance:

- old facial projects load and save without changing legacy mask semantics;
- a new project loads a facial-action template in the Landmarks worktree after the face origin is placed;
- a brow template contains a pre-authored polygon, action-line polyline, group controller, and
  action-line controller;
- paired eye, cheek, mouth-corner, and nasolabial templates use the same controls, palette, and
  synchronized side-editing behavior; lip, chin, jaw, and nose templates have editable area and
  action lines; saved projects retain their authored masks;
- polygon and polyline anchors can be edited with visible depth and contour feedback;
- point relationships are explicit, inspectable, and can control multiple surface controls;
- Masks / Regions can generate and refresh dependent regions without replacing the source controls;
- stale generated regions produce an actionable diagnostic and never apply weights silently;
- an explicit inference command can suggest point relationships without overwriting authored links.

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
