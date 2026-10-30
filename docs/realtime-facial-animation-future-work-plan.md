# Real-Time Facial Animation - Future Work

Status: **Implementation in progress; the facial runtime and basic native target authoring are available. A manual target has been exported and verified in Mesh Debug.**

The implemented facial runtime contract is documented with the engine state in:

- [Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md#facial-composition-on-the-same-mesh)
- [Mesh V11 format](mesh-v11-format.md#facial-sections-45-46-version-1)
- [Lua API - Facial targets and clips](lua-api.md#facial-targets-and-clips-mesh)

This document separates the current implementation from the remaining work. The first Phase 1
batch-authoring APIs, bounded target brush, and candidate preview are available; fuller sculpting,
bone-driven expressions, and SDK integration remain follow-up work. The former **Native Target
Authoring** item is expanded below into a concrete native facial expression authoring plan.

## Current implementation snapshot

The first native authoring slice is available on this branch:

- `MESH_MBM_DEBUG::addFacialTarget`, `updateFacialTarget`, and `removeFacialTarget` mutate a
  candidate facial asset transactionally and validate it against frame-0 geometry.
- The Lua `meshDebug` binding exposes those operations using dense `{x, y, z}` position and
  optional normal-delta arrays. Target addition appends a zero weight to existing facial clip
  keys; removal deletes the corresponding weight and protects the last target when clips remain.
- The Facial Rig Editor splits `Morph Targets` into Add, Edit, and Paint modes. Add shows only
  neutral-target creation fields and opens Edit after creation. Edit shows target metadata, batch
  operations, and duplicate/delete actions; Paint shows only brush controls and the shared target
  selection, preview, vertex filter, and local undo/redo controls.
- The Convention selector offers `ARKit`, `FACS`, and `Manual`, with `Manual` as the default for
  targets created in this editor. The label is saved with the target but does not map channels or
  change deformation. Existing labels outside the list remain intact until explicitly changed.
  FACS describes facial movements as Action Units (AUs); see the
  [Paul Ekman Group FACS overview](https://www.paulekman.com/facial-action-coding-system/).
- The worktree now applies explicit translation, rotation-around-pivot, and reset operations to
  all vertices or to an existing facial mask, with a local undo/redo stack for those target edits.
- Edit Target rotation shows X/Y/Z rings and pivot translation axes in the 3D viewport, following
  the skeletal animation editor's gizmo interaction. Left dragging a ring fills the matching
  rotation angle; left dragging an axis repositions the target's pivot. Right click can select a
  neutral vertex and a button can place the pivot there. The pivot is project metadata keyed by
  stable target ID. The displacement overlay updates the draft deformation during a ring drag,
  before Apply; pressing Apply writes the rotated deltas to the MeshDebug candidate.
- The worktree uses the same viewport controls as Masks: left drag orbits and middle drag pans.
  Right click picks a neutral vertex in Edit; right drag paints move/smooth deltas in Paint with
  radius, strength, and optional mask filtering. Add leaves right click inactive. Each paint stroke
  commits once on release and enters the same local undo/redo history.
- A contextual Ctrl indicator appears while Ctrl is held in Face Position or Landmarks, where
  Ctrl + left click changes placement. Ctrl + right click has no distinct action in Morph Targets
  or Masks, so these worktrees do not show the indicator.
- After a committed target edit, the viewport loads a temporary MSH candidate once and displays
  the selected target at the chosen preview weight. The editor project stores the selected target
  ID and preview weight when saved. It also stores changed/new targets as sparse delta overrides
  against the source MSH and records deleted target IDs. Reopening the project validates the source
  target IDs and topology before replaying those edits. The MSH stores target deltas and default
  weights, not the editor preview preference. Exporting a runtime MSH remains explicit.
- An optional displacement heat map uses the same dynamic indexed surface and pixel shader as the
  mask visualization. Its color intensity follows the selected target's per-vertex delta magnitude;
  it rebuilds when the target or geometry changes and updates its vertex/UV buffers for draft
  rotations. While enabled, it replaces the textured mesh. Changing Preview weight switches to
  the textured mesh and keeps the selected weight; the overlay toggle can enable the heat map
  again, including at weight zero, where it shows neutral geometry. The Viewport Facial Show mesh
  checkbox controls the desired visibility of either surface; hiding it leaves the heat map
  option selected, so showing the mesh again restores the heat map.

### Manual authoring trial

A new project does not clear targets already embedded in the opened MSH: the project stores editor
metadata and target edits relative to that MSH, while the source MSH stores its original dense
target deltas. To test native authoring without changing an imported expression, keep the current
weighted MSH open, choose Add, enter a unique target name such as `jawOpen_manual_test`, and create
a zero target. The editor selects it and opens Edit with preview weight 1 and the textured mesh.
Switch to Paint and select a facial mask before using the Move brush. The brush uses
the entered X/Y/Z offset with radius and strength; mouse drag only places strokes. Smooth blends
existing neighboring deltas and cannot create motion from a zero target. Edit can also start from
zero deltas with a masked batch Delta X/Y/Z; without a mask the batch edit affects the entire mesh.
Right click in Edit picks a neutral vertex for single-vertex selection and does not edit geometry.
Save the facial project to retain the sculpt session, then export a new MSH for runtime use.

For a clean asset with no imported targets, prepare a separate MSH candidate without facial targets;
creating a new project alone is insufficient. A jaw-opening rotation also needs a pivot near the
jaw hinge. In Edit Target, choose Rotate, right click a nearby neutral vertex, place the pivot there,
then refine its location with the 3D axes. Drag a rotation ring to set the angle and press Apply.
Restrict the vertex selection to a jaw mask before applying so the entire head does not rotate.

There is still no ARKit/Faceit/vendor SDK adapter. Faceit remains a workflow reference for the
future separation of shape channels from head/bone channels, not a runtime dependency.

### Manual validation - Edit Target (2026-10-30)

The `jawOpen_test_manual` trial confirmed the Edit Target workflow on the current test mesh:

- Applying Delta Z = 1 moved the target by one unit at preview weight 1; changing the preview
  weight scaled the visible displacement.
- Applying rotation around X with the `main_face` vertex mask changed only the selected region.
- Saving the facial project and reopening the editor restored the translation and rotation.
- Applying Reset to neutral with either all vertices or a mask affected the selected vertices in
  memory. Closing without saving that second edit restored the last saved translation and rotation.

This validates batch edits, mask selection, preview weight response, and project persistence for
Edit Target. Paint Target was tested separately below. Runtime MSH export was subsequently tested
as described below; the anatomical quality of `jawOpen` remains unverified.

### Manual validation - Paint Target (2026-10-30)

The same target was painted with Move using X = 1. The user observed the local deformation vary
with preview weight and confirmed that Undo and Redo reversed and restored the stroke. This
validates the basic brush and local edit history. Export was subsequently tested as described
below. Reopening the facial project after the export and checking that its mesh path and authored
target state remain consistent has not yet been reported.

### Manual validation - exported MSH in Mesh Debug (2026-10-30)

The user exported the edited Megan asset to
`/home/michel/Downloads/face-tests/Megan-morph-target-test.msh` and opened that file in **Mesh
Debug**. The authored target was present and its deformation worked there. This validates the
manual target export and reload path for that asset. It does not establish that the deformation
opens the jaw anatomically, that the neck stays fixed, or that every target and facial clip has
the intended content.

At the time of this check, the project file `Meg-facial-rig.lua` still named `Megan-weighted.msh`
as its source mesh. Export updates the project mesh path in editor memory; saving the project
afterward persists that association. The MSH target payload has already passed a user-visible
export/reload check in Mesh Debug. Saving and reopening the project after export remains a separate
editor-session check, not a missing API for MSH persistence.

### Next acceptance step for the Megan workflow

1. Try the 3D pivot gizmo on the Megan jaw, place it near the mandibular hinge, and compare the
   edited target at preview weights 0, 0.5, and 1 in Mesh Debug and the Facial Rig Editor.
2. Inspect the exported `jawOpen` deformation and the jaw/neck skeleton and skin weights. Confirm
   whether the neck movement comes from target deltas, skinning, the jaw pivot, or a combination.
3. Author and review a jaw opening around the mandibular hinge, with upper-face and neck landmarks
   stable. Decide from that result whether a morph target alone is sufficient or the skeletal jaw
   lane in Phase 3 is required.

The MSH write/read path for native targets has passed its first manual acceptance. The 3D pivot
tool is now available for the next quality pass; its interaction and the anatomical result still
need user validation. The broader native expression authoring milestone remains open until the jaw
result meets the Phase 0 anatomical criteria.

## Why native authoring is needed

The Facial Rig Editor's `Expressions` worktree edits named weights for targets that are present in
the MSH. Morph target deltas are edited in the separate `Morph Targets` worktree. Existing targets
may originate in Blender Shape Keys or the optional Blender-assisted workflow. The MeshDebug
facial surface exposes target data for inspection and supports
project-authored facial clips. It now also has a first batch mutation API for adding, replacing,
and removing dense targets. Vertex selection, a first brush, and candidate preview now exist in
the editor; target mirroring and normal-delta recomputation remain to be implemented.

This distinction matters for the supplied Megan test asset (`Meg-facial-rig.lua` ->
`Megan-weighted.msh`). The runtime evaluates the asset's facial and skeletal data; the authoring
tools can now edit a poor automatically generated `jawOpen` target. A complete workflow must let the
artist inspect the bind pose, choose the intended deformation, and verify that jaw motion rotates
around the mandibular hinge without dragging the neck. The fix may be a sculpted morph target, a
skeletal jaw correction, or a controlled combination of both; the editor must make that choice
visible instead of hiding it behind automatic target generation.

The proposed interaction is inspired by Faceit's Live Link Face importer: keep shape animation and
head/bone animation as distinct channels, allow region filtering, and make replacement/mixing and
timing decisions explicit. See the [Faceit Live Link Face importer documentation](https://faceit-doc.readthedocs.io/en/latest/mocap_importers/#live-link-face).

## Verified current baseline

| Area | Available today | Boundary relevant to native authoring |
| --- | --- | --- |
| Mesh V11 | Section 45 stores dense position/optional normal deltas; section 46 stores target-weight clips | The target section is validated against frame-0 topology and is limited to 256 targets; batch target edits now commit transactionally |
| Runtime composition | Facial morphing is applied before skeletal LBS/DQS | A target can follow the body pose, but a facial clip does not currently carry a skeletal pose |
| MeshDebug | Can inspect target metadata/deltas, preview weights/clips, append validated facial clips, save V11, and batch-edit targets | Vertex deltas commit through dense batch arrays; the editor stages strokes and reloads a temporary candidate after commit |
| Facial Rig Editor | Has landmarks, masks, facial skeleton binding, target-weight expressions, batch/brush target editing, candidate preview, clips, undo/redo, and MSH export | Expressions contain named target weights only; jaw/head bone pose authoring remains absent |
| Skeletal authoring | The engine/editor already has canonical bone and skeletal clip authoring APIs | The facial expression workflow does not yet compose a jaw/head bone pose with facial target weights as one authored expression |

## Native Target Authoring - native facial authoring plan

This is technically feasible with the current architecture. The lowest-risk path is to add target
mutation and sculpt preview to MeshDebug first, then add bone-driven corrective expressions only
after the Megan jaw diagnostic identifies whether the asset needs a morph, a jaw rotation, or both.

The recommended first deliverable is a Blender-free authoring path for one loaded, topology-stable
MSH. It should use the current MeshDebug candidate and the existing Facial Rig Editor worktrees,
not create a parallel facial asset model.

### Phase 0 - Megan diagnostic spike

Use the supplied Megan asset as the acceptance fixture before implementing a general sculpt tool.

1. Load the weighted MSH and record its facial target catalog, geometry signature, frame-0 vertex
   order, skeleton bind report, jaw/neck bone names, and relevant vertex weights.
2. Preview the current `jawOpen` target at controlled weights and identify whether the bad motion is
   caused by the imported target geometry, the jaw/neck skin weights, the bone bind/pivot, or a
   combination.
3. Author a temporary jaw-bone rotation around the existing hinge and compare it with a temporary
   morph-only correction. Keep the two results separate so the editor can show which deformation is
   responsible for the neck movement.
4. Decide whether the first production `jawOpen` asset should be morph-only, skeletal-only, or a
   paired corrective expression. This is an acceptance decision, not an implementation assumption.

The spike is complete when the desired result is measurable: the chin opens around the hinge,
upper-face landmarks stay stable, and the neck does not translate downward merely because the jaw
opens.

### Phase 1 - Native target data foundation

The first part of this phase is implemented. Continue extending `MESH_MBM_DEBUG` and its Lua
binding while keeping the facial asset PIMPL-owned.

- [x] create a target from the neutral frame, or duplicate an existing target;
- [x] rename it and edit its convention/default weight metadata;
- [x] read and write position deltas and optional normal deltas in batch form;
- mirror a target across the project face plane while swapping recognized left/right target names;
- [x] remove a target transactionally, remap embedded clip weights, and synchronize the editor's
  named expression references;
- [x] validate finite values, vertex count/order, topology signature, target IDs/names, delta arrays,
  normal-delta presence, and the Mesh V11 target limit;
- [x] commit the entire candidate atomically, preserving the previous valid asset on failure.

The remaining Phase 1 work is mirror support and finer-grained expression-reference diagnostics.
Preview currently writes a temporary MSH on committed edits; a direct in-memory bridge could
remove that temporary round-trip later.

The API should be designed for large copy-outs and batch edits. It must not expose vectors,
containers, or mutable implementation storage through public headers. Existing read-only export
helpers may be reused, but a setter should not be assembled from one Lua call per vertex.

The runtime export path saves a new MSH and does not overwrite the source asset implicitly. The
project file stores editor metadata, named expressions, and sparse target edits relative to the
source MSH, so sculpt sessions survive before runtime export. The exported MSH persists the dense
target payload for the engine.

### Phase 2 - Target sculpting worktree

Add a target-authoring mode beside the current weight sliders. The viewport should reuse the
existing mesh, camera, masks, landmarks, face guide, undo/redo, and dirty-flag conventions.

The minimum useful sculpt tool is:

- target list with new, duplicate, rename, delete, mirror, reset-to-neutral, and preview actions;
- vertex or surface picking that maps back to the canonical frame-0 vertex order;
- a bounded brush with at least move/grab and smooth modes, adjustable radius and strength;
- region/mask filters for brows, eyes, cheeks, nose, mouth, and tongue, matching the user's
  existing facial mask concepts and the Faceit workflow;
- explicit position-delta and normal-delta handling, including a mode to recompute normals from the
  deformed preview and a mode to preserve authored normal deltas;
- target-local preview at weights 0, 0.5, and 1, plus simultaneous preview with skeletal skinning;
- changed-data indicators and transactional save/export feedback.

The continuous editor loop must only rebuild the sculpt surface, spatial index, preview buffers,
and diagnostics when the mesh, target, brush, mask, or camera input changes. Idle frames must not
rescan all vertices, allocate dense arrays, or rewrite target data.

The first implementation slices are the mask-scoped batch delta editor, neutral-surface vertex
picking, move/smooth brush with bounded falloff, a per-target 3D rotation pivot gizmo, and
on-change temporary candidate preview. Rotation-ring dragging updates the draft angle and the
dynamic displacement surface at a bounded cadence; the MeshDebug candidate changes on Apply.
The first surface build batches triangle indices, then draft rotations update its vertex/UV
buffers without recreating the shape. A new neutral target starts with preview weight 1 and all
vertices selected so the draft can be seen immediately. The target brush now picks and selects
vertices on the morph-deformed preview surface, while committing deltas against canonical frame-0
vertex indices. A stroke holds its starting surface until mouse release; the next stroke uses
the newly committed deformation. Batch transforms and brush strokes accumulate on one target.
Reset-to-neutral intentionally clears deltas for the selected vertices. Remaining work includes
continuous textured draft preview, reliable normal-delta recomputation, region presets beyond
project masks, and a direct in-memory preview bridge.

### Phase 3 - Jaw/head skeletal authoring lane

Add an explicit skeletal lane for expressions whose correct solution is a bone transform rather
than a vertex delta. This lane is essential for the Megan `jawOpen` investigation and should use
the canonical skeleton already bound by the Facial Rig Editor.

- select a facial bone or body `Head`/`Neck` bone and display its bind pivot and influence context;
- provide a rotation gizmo with hinge-aware jaw defaults, plus optional translation only when the
  author explicitly enables it;
- preview the bone pose with neutral facial weights and with the selected morph target;
- record the pose as a skeletal key or as an expression-local corrective pose;
- keep head rotation/location separate from face shapes, as in the Faceit workflow, with explicit
  replace/mix semantics when importing or recording animation later.

The editor may initially export paired facial and skeletal clips with a shared name and timing.
Before exposing one runtime `playExpression` operation, define the missing composition contract:
how a paired expression is identified, how its two clocks/loop policies are synchronized, how
manual target overrides interact with the bone pose, and what happens when one side is absent.
Do not bake a bone rotation into dense morph deltas as a shortcut; that would make the result depend
on the current skinning pose and would undermine the existing morph-then-skin contract.

### Phase 4 - Expressions and clips

Extend the current expression model without replacing it:

- retain the existing target-weight map for morph-only expressions;
- add optional skeletal pose channels for expressions that need jaw/head corrections;
- support region filters, mirror rules, neutral reset, and notes/diagnostics;
- let a clip sample both channels when the runtime/export contract is ready;
- preserve the current facial clip export for morph-only projects;
- validate that all referenced targets and bones still exist before export.

The first end-to-end expression should be `jawOpen`, followed by `blink`, brow raise/lower,
mouth smile/frown, and one asymmetric left/right expression. These cases exercise hinge rotation,
normal deltas, mirror naming, masks, and mixed morph/skeletal composition.

### Phase 5 - Validation and acceptance

Add coverage at three levels:

1. **Model tests:** target creation, duplicate/rename/delete, mirroring, clamping, invalid topology,
   normal-delta modes, undo/redo, and expression reference cleanup.
2. **Native round-trip tests:** save a candidate MSH, reload it, compare mesh ID, geometry
   signature, target identity/order, all position/normal deltas, skeletal data, and existing clips.
3. **Visual/runtime tests:** run the Megan fixture through the same preview path used by a game and
   verify `jawOpen`, blink, brows, and an asymmetric expression at neutral, half, and full weight.
   The jaw case must demonstrate rotation around the intended hinge without the neck translating
   with the chin.

Every failed validation must leave the last valid in-memory candidate and source MSH untouched.
Backend acceptance should use the existing CPU reference and the available GPU morph paths; native
authoring must not introduce a renderer-specific deformation result.

## Explicit non-goals for the first milestone

- automatic ARKit/FACS solving or webcam capture;
- replacing Blender for all mesh creation, retopology, or skin-weight painting;
- baking a skeletal pose into a facial target without a deliberate authoring decision;
- changing Mesh V11 sections 45/46 merely to make the first sculpt UI possible;
- custom shader or normal-map support for facial assets;
- a full Faceit feature clone before the Megan jaw case is solved and tested.

## Live capture integration

Add optional transport adapters for OSC first, followed by webcam or vendor SDK integrations such
as ARKit. Reuse the existing named-weight mapping and deterministic replay contracts without
changing Mesh V11.

The work should define connection state, packet timeout, neutral reset, calibration, target
diagnostics, and optional recording to a facial clip. Transport code should remain outside the core
asset format and should not change the existing manual-weight or clip-playback API.

## Native target authoring

Add an optional Blender-free workflow for creating, naming, duplicating, mirroring, deleting, and
sculpting dense facial target deltas. It should provide undo/redo, topology validation, normal-delta
handling, target preview, and transactional Mesh V11 export.

The workflow should reuse the existing Expressions and Clips worktrees rather than introduce a
second animation model.

## Additional platform acceptance

Extend the existing indexed-resource contract, capability checks, and CPU fallback rules to iOS
Metal and any other supported platform that does not yet have facial runtime acceptance. Platform
work must preserve the current facial sections and Lua control contract.

## Compatibility extensions

Custom vertex/pixel shaders and normal-mapped facial assets remain outside the current facial
contract. Any future support for those combinations must define their shader inputs, material
interactions, validation rules, and CPU/GPU fallback behavior before changing the runtime contract.

When future work changes a binary section or Lua API, update the corresponding format/API
documentation and `include/version/version.h` in the same change.
