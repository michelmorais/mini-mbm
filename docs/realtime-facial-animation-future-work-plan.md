# Real-Time Facial Animation - Future Work

The current facial runtime, Mesh V11 format, Lua API, and native authoring workflow are described in:

- [Facial Rig Editor](facial-rig-editor.md)
- [Morph Editor](morph-editor.md)
- [Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md#facial-composition-on-the-same-mesh)
- [Mesh V11 format](mesh-v11-format.md#facial-sections-45-46-version-1)
- [Lua API - Facial targets and clips](lua-api.md#facial-targets-and-clips-mesh)

The Facial Rig Editor can author facial morph targets. The generic Morph Editor implementation now
reaches milestone 2.5, including non-facial surface regions, target sculpting, mirroring, smoothing,
runtime export validation, the complete manual coverage pass, and generic named expressions. Generic
region and project modules remain reusable for future editor tools; capture expansion remains a
separate future milestone.

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

In the **FACS Mask** workflow, the authoring source is **Landmarks**, not **Masks / Regions**. After
the face origin is positioned, the editor loads a pre-defined facial-action template and the user
adapts its controls directly on the visible mesh. The separate **Vertex mask** workflow
retains classic landmark and vertex-mask authoring:

| Kind | Intended use | Initial semantics |
| --- | --- | --- |
| polygon | Area influence, such as a brow, cheek, eyelid, or jaw area | Closed ordered surface anchors with editable contour and depth |
| point | Group controller for one or more influence areas | Explicit references to the polygons it moves |

The user must be able to select an action mask, drag its pre-authored surface anchors, adjust their
depth and edit contours in the same facial-mark authoring view. The selected mask exposes its
influence polygon and group controller as one editable group.
All other masks remain visible as colored outlines, while only the selected mask exposes clickable
square anchors. The group controller moves its area and any explicitly linked influence polygons
together.
The editor offers independent and synchronized side editing for paired facial masks; synchronized
editing mirrors the authored positions while retaining each side's mesh anchors. Stock influence
areas share one cyan color across sides, and group controllers share one green color.
The face guide remains a placement aid and must not be confused with an authored control or a
legacy vertex mask.
The current catalog includes paired brows, eyes, cheeks, mouth corners, and nasolabial folds, plus
upper-lip, lower-lip, chin, jaw, and nose masks. New facial projects receive all 15 masks after face-origin placement. Saved
projects retain their serialized controls: authored template shapes are not added, recolored, or
reconciled on load. Templates are seeded only for an empty FACS catalog. Any future serialized
template-layout change must bump the project schema; files with an older schema are rejected and
must be recreated. Chin and jaw-hinge point landmarks already exist as optional rig references;
their surface influence masks are separate.
AU labels on these templates are semantic hints. The editor derives binary front-surface regions
explicitly from closed area polygons, tracks source edits and topology changes, and can copy a
current region into an independent Morph Target selection mask. Surface falloff, automatic
rig-weight assignment, and Blender transport remain future milestones. AU-to-target profile
authoring is described in Phase 3.

The FACS region workspace separates target operations into **Create Target** and **Update Target**.
The latter exposes the target-mask selector and can regenerate the region and replace the selected
target mask in one action; creating a target never changes an existing target mask implicitly.

**Phase 1 controller relationships are implemented.** Each point controller stores direct links
to controls and a contribution weight from 0 to 1. The Landmarks/FACS panel displays the links;
the user can change their weights, remove links, or add them through a review list. Gizmo and
surface drag move linked controls by the controller displacement multiplied by each link's weight.
Stock template links remain at weight 1. A link to another controller moves that controller point
only; its own links are not traversed. All edits are undoable and saved in the facial project.
The optional **Suggest nearby** command finds unlinked influence polygons near the selected
controller in the face X/Y plane. It proposes same-side or unpaired controls, displays candidates
for review, and changes the project only after **Apply selected links**. Existing authored links
are preserved. Synchronized side mode mirrors link IDs and weights where counterpart controls
exist; independent mode leaves the opposite side unchanged.

**Masks / Regions** is the derivation stage. It consumes authored controls and generates named
regions, vertex weights, and any legacy-compatible selections required by existing rig or target
workflows. A generated region records its source-control identity, source topology signature, and
generation state. Editing a source control marks dependent regions stale; regeneration is explicit
and must be reviewed before export or weight application.

New projects select one workflow in **Setup**. **Vertex mask** uses classic landmark,
Rig-role, and vertex-mask authoring; **FACS Mask** uses surface-action templates and derived
regions. The saved choice is fixed for that project. The workspace tree includes Rig only in the
classic path and FACS Action Unit Profiles only in the FACS path. **Masks / Regions** also filters
its controls: classic projects show Rig role management and classic masks, while FACS projects show
generated regions and target masks. Morph Targets, Expressions, and Clips are shared. Project
schema 21 stores the workflow and rejects earlier project schemas; users must recreate those
projects.

Classic vertex masks are retained as a supported authoring representation in the **Vertex mask**
workflow. They are not converted automatically into FACS surface controls.

The **Vertex mask** workflow edits classic vertex masks only. Surface polygon editing belongs only
to **FACS facial masks**, the FACS version of the **Landmarks / Marcos faciais** workspace. It is the
source authoring step for influence polygons and controllers. **Masks / Regions**
has no representation selector: it generates FACS regions from those authored polygons and edits
only the resulting vertex selections. Copying one into a target-selection mask is explicit, with no
automatic conversion or weight application.

The first surface-anchor implementation may use canonical mesh vertices. A later extension may
anchor a point to a triangle plus barycentric coordinates when it must lie between existing vertices.
Derived weights are rebuildable data and are never the primary authoring source.

The bridge to Blender must eventually carry surface controls, generated regions, and their topology
identity. Existing legacy vertex lists remain available for compatibility exports.

Acceptance:

- a new project requires one workflow choice in Setup before saving or opening its workspaces;
- a project keeps its selected workflow, shows Rig only in the classic path and FACS Profiles only
  in the FACS path, and filters Masks / Regions to that workflow;
- the FACS Action Unit reference button appears in Setup only after the FACS workflow is selected;
- a FACS project loads a facial-action template in the Landmarks worktree after the face origin is placed;
- a brow template contains a pre-authored influence polygon and group controller;
- paired eye, cheek, mouth-corner, and nasolabial templates use the same controls, palette, and
  synchronized side-editing behavior; lip, chin, jaw, and nose templates have editable influence
  areas; loading a saved project does not upgrade its FACS templates;
- polygon anchors can be edited with visible depth and contour feedback;
- point relationships are explicit, inspectable, and drive weighted movement of multiple surface controls;
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
    falloff = { mode = "surface", radius = 2, curve = "smooth" },
    shape = { type = "square", free = false, segments = 16 }
}
```

The generic Morph Editor may author a parametric `square` or `ellipse` shape before it becomes a
free contour. A locked shape preserves its closed contour while it is resized; its uniform handle
keeps a square square, while width and height handles can produce a rectangle. `free = true` means
that its surface anchors can be edited independently. A `polygon` remains the unconstrained shape
representation.

The initial generic editor should reuse target catalog, transforms, painting, smoothing, masking,
and mirroring without requiring facial landmarks, facial roles, or FACS names. A `polygon` is an
area influence, a `polyline` is a line-of-action influence, and a `point` is a localized control.
Concave polygons must be supported for selection; triangulation used for display must not change
the meaning of the influence field.

The facial editor should consume this generic core while retaining its facial-specific worktrees.
The runtime format remains unchanged in this phase; the semantic distinction belongs to project
metadata and editor modules until a runtime semantic API is proven necessary.

**Milestone 2.1 - generic polygon-region core.** Introduce a reusable region record and lifecycle
that contains a name, polygon kind, canonical surface anchors, topology signature, rebuildable
vertex selection, and falloff settings. Its geometry API must not require face origin, facial
roles, or FACS identifiers, and must support concave contours. This milestone establishes the
data and geometry layer; it does not yet add a separate Morph Editor worktree. The current FACS
region generator keeps its face-specific front-depth and subset rules until the generic editor
can supply equivalent surface constraints. The reusable module is
`editor/morph_surface_regions.lua`; facial generated-region records now also carry its generic
anchors and derived-weight field while retaining their face-specific source validation. At this
stage, polygon selections remain binary; falloff settings are stored for the later target workflow
but are not applied yet.
The generic implementation is split across `morph_surface_topology.lua`,
`morph_surface_polygon.lua`, and `morph_surface_regions.lua`; the facial modules retain
compatibility wrappers for facial-only callers.

**Milestone 2.2 - non-facial surface-shape authoring (implemented).** The Morph Editor has its
first worktree, `Surface shapes`, with a shape-aware interaction model instead of exposing only a
free polygon. New regions can be Square, Circle / ellipse, or Free polygon. Locked Square and
Ellipse shapes retain their parametric form while the center controller moves the complete shape
and the resize handles change its dimensions. The Free flag converts either template into an
independently editable contour. Shape anchors and derived selections are persisted in Morph Editor
project schema 4. The worktree is organized into Add, Edit, Mirror, and Remove modes. Mirror mode
previews a reflected copy of the selected shape across an X/Y/Z origin plane and creates it only
after explicit confirmation, leaving the source available for comparison. The viewport
provides mesh/region framing, mesh visibility, camera distance, navigation help, and the orbit
gizmo. Selector size, language, worktree mode, shape defaults, camera state, and other authoring
preferences are persisted with the project. Selection data is currently binary; falloff and
morph-target editing remain for milestone 2.3. See
[Morph Editor](morph-editor.md).

**Milestone 2.3 - morph workflow integration (implemented).** The Morph Editor now has a separate,
sequential `Morph targets` worktree node after `Surface shapes`. It creates an independent target selection from a ready surface region,
retains the source region fingerprint and topology identity, explicitly rebuilds the target
selection after source changes, and supports weighted translation, rotation around a persisted
source-selection pivot, reset-to-neutral operations, and soft edge rings over triangle adjacency.
Edit mode
now uses paired dynamic in-memory surfaces: a shader heat map and a textured/default-material geometry
view, selected by the displacement checkbox, with a neutral-to-full contribution slider. A non-zero
Translate or Rotate draft temporarily uses the textured/default-material view and applies the draft
only in memory until Apply, including soft selection weights and the current pivot. Paint target
mode now applies Move or Smooth strokes to selected target vertices on right-drag, updates a direct
working copy while the stroke is active, and commits the working copy only when the stroke ends;
the source MSH remains unchanged. The Morph Editor deliberately does not preview the candidate
deformation during the stroke, matching the facial editor workflow and keeping consecutive brush
increments predictable.
Mirror target now reflects
the selected source polygon and target across the origin plane of a selected X/Y/Z axis, reports
unmatched/ambiguous matches, and presents a temporary polygon and target candidate. The explicit
Create action commits a separate polygon and target with mirrored selection, position deltas, and
pivot when present. The analyzed candidate is previewed through the existing in-memory target
surface without changing the source target. The target record is generic and remains project data;
the explicit MSH export action now copies the source mesh, materializes the generic sparse targets
as dense position deltas, replaces same-name targets, and leaves the authoring source unchanged.
Paint Smooth now supports configurable surface-neighbor passes based on triangle adjacency while
respecting the target selection boundary. Smooth also protects sharp surface creases by comparing
the normals of the incident triangles, with an adjustable angle for low-poly meshes. This avoids
blending across likely lip openings or other narrow folds while retaining an explicit opt-out for
intentional cross-crease smoothing. Runtime reload and combined skeletal playback have been
validated with the exported MSH. Export now also offers explicit preservation of existing normal
deltas or recomputation from each deformed triangle surface; both policies were visually checked
after runtime reload.
Keep derived selections rebuildable and require explicit regeneration after anchor or topology
changes.

Acceptance: a non-facial MSH can create, preview, save, reload, export, and play a morph target
using the same region tools, without requiring face origin, facial landmarks, or facial roles.
This acceptance has been validated with MeshDebug and a runtime mesh instance, including combined
skeletal playback.

**Milestone 2.4 - Morph Editor coverage and hardening (implemented and validated).** Closed the remaining authoring
coverage with a repeatable validation matrix for locked and free Square / Ellipse / Free polygon
shapes, Add / Edit / Mirror / Remove modes, surface fitting, region rebuilds, target translation,
rotation, painting, smoothing, soft edge rings, undo/redo, project reload, and MSH export. Verify
that inactive worktrees do not retain visible selectors or modify the active mesh, and that camera
orbit, pan, shortcuts, selector sizing, language, and other persisted preferences remain consistent.
The matrix must include a non-facial mesh and a facial mesh, partial target weights, combined
skeletal playback, and both normal-delta export policies.

Acceptance: each Morph Editor operation has a documented manual case with its expected viewport,
project, and exported-MSH result; repeated strokes do not double-apply; idle frames do not rebuild
geometry; and reopening the project preserves the authored shapes, targets, preferences, and
selection state without requiring the Facial Rig Editor. This acceptance was validated manually with
the Megan facial mesh, including target preview, painting, smoothing, mirrored shapes and targets,
project reload, MSH export, MeshDebug playback, combined skeletal playback, and the two normal-delta
export policies. The generic workflow remains independent of facial landmarks and FACS metadata.

**Milestone 2.5 - generic Morph expressions (implemented and validated).** Add the sequential `Expressions`
worktree after `Morph targets`. An expression is a named generic pose composed only from normalized
weights referencing Morph Targets by stable target ID; it does not evaluate FACS Action Units,
profiles, anatomy, or muscle relationships. Add mode edits an in-memory draft and creates a named
expression explicitly. Edit mode changes the selected expression's name and target weights, while
Remove mode deletes only the expression. The viewport composes all valid target deltas in memory and
shows the result through the existing neutral-to-full preview weight without rewriting the source
MSH or materializing an export. Missing or stale target references are reported instead of being
silently discarded. Expressions are persisted in Morph Editor project schema 6, including the
selected worktree, expression mode, and name draft preferences. Expression export remains out of
scope until generic Morph clip export is implemented.

Acceptance: a generic Morph Editor project can create, preview, edit, save, reload, and remove a
named multi-target expression; each target weight is clamped to `0..1`; the source targets and MSH
remain unchanged; and an invalid target reference is visible in the expression status.

**Milestone 2.6 - generic Morph clips (implemented).** Add the sequential `Clips` worktree after
`Expressions`. A clip stores ordered keys with stable Morph Target IDs and normalized weights; it
does not store FACS metadata and does not require a facial mesh. Add mode creates a named clip with
neutral keys at time zero and at the end of its initial duration. Edit mode can insert a saved
Expression or capture the current sampled Morph pose at the current time, change duration, frame rate, looping, and
Step/Linear interpolation, and preview the sampled composition through the existing in-memory
Morph overlay. The clip timeline is displayed in a separate window, following the Facial Rig
Editor interaction pattern and exposes Insert expression key, Capture current key, Reset Morph
weights, Play, and Stop in the same order. Remove mode deletes only the selected clip. During playback, the preview updates
the existing dynamic surface buffers instead of rebuilding temporary geometry every frame. Missing
or stale target references are reported and prevent a complete clip preview. Clips are persisted
in Morph Editor project schema 6 and are not exported to MSH in this milestone.

Acceptance: a generic Morph Editor project can create, edit, save, reload, and remove a clip;
insert Expression and neutral keys; sample a clip at zero, partial, and full times; play a looping
and a non-looping clip; and preserve stable target references. The source MSH, Morph Targets, and
Expressions remain unchanged. Invalid target references are visible before preview or later export.

## Phase 3 - FACS authoring profiles

**Milestone 3.1 - project Action Unit profiles (implemented).** The Facial Rig Editor now has a
FACS Profiles workspace persisted in facial project
schema 21. A profile defines:

- semantic name and optional left/right side;
- a standardized facial Action Unit code, shown with its action name;
- normalized intensity range;
- one or more output target references;
- per-output weight, response curve, and optional clamp;
- optional secondary contributions for neighboring facial regions;
- diagnostics when a referenced target is missing or a saved Action Unit is outside the standard
  facial AU catalog.

The workspace lets the user create and edit profiles, map outputs to the current mesh's facial
targets, set each profile's normalized input range and preview intensity, and choose additive
saturation or maximum contribution when profiles share a target. The optional secondary flag
records which output is a neighboring-region contribution; it does not change the weight formula.
Missing target references block preview and expression capture. Applying the composition writes
ordinary target weights to the current mesh, and a composed-target table shows each final weight
and its primary/secondary role. **Capture as expression** stores those weights in the existing
expression model, which can then be used by the existing clip and MSH export workflows.
The FACS profile metadata remains in the editor project; no new runtime or MSH payload was added.

The current profile evaluator intentionally maps explicitly authored Action Units to existing
targets. It does not infer mappings from facial polygons and does not simulate
muscle anatomy. Profiles can be authored without a loaded mesh, but target mapping and evaluation
require one. The Action Unit picker lists the 44 single facial AUs with their standard action names.
The reference window marks which AUs have an editor surface-mask template; other standardized AUs
can still be profiled with manually mapped targets.

In the FACS workflow, the workspace order is **FACS Masks**, **Masks / Regions**, **Morph Targets**,
**FACS Action Unit Profiles**, **Expressions**, then **Clips**. Adapt surface masks, generate
regions if reusable vertex selections are useful, create or import morph targets, map targets to
Action Units in profiles, capture an expression, and use it in clips. Rig is hidden in this
workflow; choose **Vertex mask** in a separate project for bone and weight authoring.

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
implementation preserves the existing runtime composition and MSH format. If multiple profiles
affect the same target, the project defines the composition rule explicitly; evaluation follows
profile and output list order for additive saturation and uses maximum contribution for the
alternative rule.

Phase completion acceptance: an artist can author a brow, eye, or mouth Action Unit, inspect its
primary and secondary target effects, combine it with another Action Unit, and carry the result
through the existing expression/clip and MSH export workflow. Missing references block preview or
export with an actionable diagnostic. Runtime visual verification was completed with combined AU
profiles, facial clips, MSH export, and MeshDebug playback.

## Phase 4 - open capture adapters

Keep capture adapters independent from the target asset. An adapter produces normalized Action Unit
or named-target values, and the same FACS profiles used by the editor resolve those values to mesh
weights. OSC and recorded data are valid first adapters; no vendor SDK is required.

The adapter contract must define connection state, packet timeout, neutral reset, calibration,
target/action diagnostics, replacement versus mixing, and deterministic recording/replay. A timeout
must recover to the documented neutral or fallback state without leaving stale facial weights active.

**Milestone 4.1 - deterministic recorded replay (implemented and visually validated).** The Facial Rig Editor has an
optional **Open capture** workspace in FACS projects. It loads a newline-delimited JSON replay in
which each packet contains a non-negative `timestamp` and a `weights` object. Source names are
normalized and mapped to the loaded mesh's existing facial targets; common ARKit/Faceit-style
aliases are accepted, values are clamped to `0..1`, and unknown or ambiguous names are reported
without aborting the packet. Replay is session state, not project or MSH data.

Packets are sparse updates: names present in a packet replace their current weights, while omitted
names retain their previous values. A producer clears one target by sending `0`; the editor clears
all targets on explicit reset or, by default, when replay stops, reaches EOF, or leaves the capture
workspace. This is the current replacement-plus-persistence contract; full snapshot packets are
also valid when the producer includes every target on each line.

The workspace supports start, stop, reset, looping, packet counters, mapping diagnostics, and a
neutral reset when the file ends, the replay is stopped, or the user changes worktrees. The final
packet remains visible until the next time step confirms EOF, avoiding a one-frame disappearance.
The replay decoder is dependency-free and deterministic, so it is also the reference contract for
future OSC or webcam adapters. This first adapter accepts named-target values; AU-to-profile
evaluation remains the next capture milestone.

Acceptance: a recorded replay can drive existing facial targets, expose unknown-name diagnostics,
loop deterministically, and return to neutral at EOF without changing the project schema or MSH
format.

## Sculpt quality and feedback

- Implemented an explicit export choice to recompute normal deltas from the deformed surface or
  preserve authored normal deltas. Verify both modes at neutral, partial, and full target weight.
- Implemented the textured draft preview while a batch transform or pivot rotation is being adjusted,
  before Apply. Preserve the current behavior in which Apply commits the target deltas. Verify the
  preview at partial weight and with soft edge rings before moving on to narrow-region sculpt quality.
- Implemented surface-aware smoothing for mouth-corner and other narrow-region corrections. Smooth
  can protect sharp triangle creases by normal angle, preserving likely lip openings while leaving
  an explicit opt-out. A shared mesh vertex is still one vertex; this does not split topology or
  pretend a selection mask can separate coincident surfaces.
- Evaluate face-region selection presets. Brush strokes intentionally follow the facial editor
  workflow: they accumulate in a working copy and refresh the target preview only when released;
  presets remain a workflow improvement and are not prerequisites for saving a target.

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
- Keep custom shader and normal-mapped facial composition out of the current runtime contract:
  facial loads use geometric normals and discard optional normal-map inputs in memory, while
  derived facial exports omit those sections. A future facial-plus-normal-map implementation must
  define shader inputs, lighting interactions, validation, and fallback behavior before enabling
  composition.

Acceptance: equivalent facial weights and skeletal poses produce the same supported deformation
contract across the accepted backends. When a future change alters a binary section or Lua API,
update the format/API reference and `include/version/version.h` in the same change.
