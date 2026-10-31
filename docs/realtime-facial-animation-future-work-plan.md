# Real-Time Facial Animation - Future Work

Status: **Implementation in progress. The native morph-target jaw authoring goal has a user-validated visible opening, and the edited project and MSH were saved. Expression/clip runtime acceptance and SDK integration remain open.**

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
- The Facial Rig Editor splits `Morph Targets` into Add, Edit, Paint, and Mirror modes. Add shows
  neutral-target creation fields and opens Edit after creation. Edit shows target metadata, batch
  operations, and duplicate/delete actions; Paint shows brush controls, vertex filters, and local
  undo/redo. Mirror keeps the mirroring inputs and diagnostics separate from Edit. Target selection,
  preview weight, and the displacement toggle are shared.
- The Convention selector offers `ARKit`, `FACS`, and `Manual`, with `Manual` as the default for
  targets created in this editor. The label is saved with the target but does not map channels or
  change deformation. Existing labels outside the list remain intact until explicitly changed.
  FACS describes facial movements as Action Units (AUs); see the
  [Paul Ekman Group FACS overview](https://www.paulekman.com/facial-action-coding-system/).
- Mirror Target can analyze an X-plane mirror and create a new target with mirrored position/normal
  deltas and pivot. The suggested name swaps recognized Left/Right or L/R suffixes; the name and
  plane are editable. Matching uses neutral vertex positions with an adjustable radius. The report
  shows matched vertices, active source vertices without partners, and ambiguous destinations.
  Exact matching creates directly. A limited approximate mode requires an explicit checkbox and
  permits at most 1% unmatched active vertices, 0.1% ambiguous destinations, and at least 80%
  matched vertices; the artist must inspect/correct that result. Analyze builds a temporary mesh
  copy with the proposed mirrored target. The preview shows its textured deformation at the chosen
  weight, or the mirrored displacement heat map when enabled. Changing the source or mirror
  parameters discards that preview. Only Create mirrored target mutates the editor candidate; the
  source target remains unchanged. In Mirror mode, enabling a guide selects a representative
  active source vertex; right clicking the mesh can select another neutral source vertex. The
  guides show a 3D line to its reflected position and three circles at the match radius.
  These optional guides update while dragging or typing Face plane X, or adjusting the match radius;
  the analysis itself still requires Analyze mirror. Selecting a target recalculates Face plane X
  from Face Origin (or mesh bounds) and resets the match-radius fraction to 0.005.
- Edit Target can inspect references in authored expressions and project clips. Renaming a target
  migrates named weights in both, including undo/redo of the rename. Deleting a referenced target
  requires a second click and clears those names from project expressions and clips after the
  MeshDebug operation succeeds. Duplicate and mirrored targets start without references.
- A Megan source-asset smoke test of `eyeBlinkLeft` with the X plane at 0 and a match radius equal to
  0.005 of the mesh extent matched 32,417 vertices, found 16 active source vertices without a
  partner and 3 ambiguous destinations. The limited approximate criteria were satisfied; adding
  the resulting dense target through MeshDebug succeeded in memory as target index 13. The source
  MSH was not modified. This verifies the candidate API path, not the artistic quality of the
  mirrored expression; visual review and an explicit project save/export remain necessary.
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
  target IDs and topology before replaying those edits. If the referenced MSH already contains the
  exact edited target catalog and deltas, the editor treats the patch as incorporated, removes the
  redundant patch in memory, and asks for a project save. Other source mismatches remain errors.
  The MSH stores target deltas and default weights, not the editor preview preference. Exporting a
  runtime MSH remains explicit.
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
In Masks / Regions, enter `jaw_lower` under New target mask name and choose Create Morph Target
selection mask. Build its visualization, then paint the lower jaw, chin, and lower lip into this
mask with the right mouse button. It has no rig role and does not change rig weight assignment.
Return to Morph Targets and choose `Mask: jaw_lower` under Vertex selection in Edit or Paint.
Switch to Paint before using the Move brush. The brush uses
the entered X/Y/Z offset with radius and strength; mouse drag only places strokes. Smooth blends
existing neighboring deltas and cannot create motion from a zero target. Edit can also start from
zero deltas with a masked batch Delta X/Y/Z; without a mask the batch edit affects the entire mesh.
Right click in Edit picks a neutral vertex for single-vertex selection and does not edit geometry.
Save the facial project to retain the sculpt session, then export a new MSH for runtime use.

For a clean asset with no imported targets, prepare a separate MSH candidate without facial targets;
creating a new project alone is insufficient. A jaw-opening rotation also needs a pivot near the
jaw hinge. In Edit Target, choose Rotate, right click a nearby neutral vertex, place the pivot there,
then refine its location with the 3D axes. Drag a rotation ring to set the angle and press Apply.
Restrict the vertex selection to the jaw mask before applying so the entire head does not rotate.
The mask is a binary vertex selection; use the optional Soft edge for the batch rotation and
localized Paint Move corrections where needed. Smooth averages nearby deltas and may close the
mouth gap, as the Megan trial demonstrated. Existing role masks continue to serve rig binding,
while target selection masks serve sculpting only.

The Masks / Regions worktree now has three modes: Rig roles for source-object and rig-role setup,
Manage masks for creation and metadata, and Edit selection for viewport painting and boundary-loop
selection. The mask list remains available in the latter two modes. Right-click mask edits are
active only in Edit selection; changing modes keeps the selected mask and restores the textured
mesh outside the edit mode. Entering Edit selection or switching masks there starts the shader
surface build when needed; vertex-only edits update the existing surface.

Masks / Regions now also has a Select open boundary loop checkbox. With it enabled, right click
near a mesh boundary to add or remove that entire closed loop using the current Add/Remove mode.
Changed vertices update the existing mask shader and selection counts immediately, without a
manual visualization rebuild. The brush radius defines the search distance. The clicked triangle
selects the connected surface first, because distinct loops can occupy the same 3D positions.
This detects edges used by one triangle; it does not detect quad edge loops in sealed topology,
split welded vertices, or create mouth interior geometry. A read-only inspection of the supplied
`Megan-weighted.msh` and saved `mandibula_inferior` mask found two coincident closed boundary loops
of 48 vertices each. Eleven vertices from each loop were already in that mask. Their anatomical
meaning still needs visual review.

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
below. The later project reload failure and its repair are recorded below.

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
export/reload check in Mesh Debug. The separate project reload issue is recorded below; it was a
stale editor patch, not missing MSH target persistence.

### Project reload after exported target (2026-10-31)

The user reported that Mesh Debug loaded `Megan-morph-target-test.msh` and displayed the newly
exported target, but reopening `Meg-facial-rig.lua` failed. The saved project was valid Lua and
pointed to that MSH; it still held a patch based on 12 targets. The MSH already contained all 13,
including `eyeBlinkRightDuplicated` with the same ID, metadata, position deltas, and normal deltas
as the patch. The old reload path rejected this as a source MSH mismatch. The editor now recognizes
that exact already-exported state without applying the patch twice, marks the project as changed,
and prompts a save to remove the redundant patch. A local engine test loaded the actual MSH and
saved/reloaded a repaired project copy in `/tmp`; reopening through the editor GUI remains to be
confirmed by the user.

### Manual validation - Megan jaw comparison (2026-10-31)

The user tried opening Megan's jaw in the skeletal animation editor as well as in the Facial Rig
Editor's Morph Targets workflow. A jaw-bone rotation was possible, but the user saw a vertex break
at the nape. The facial target edit did not show that break. The skeletal result could involve skin
weights, the bind pose, or another asset issue; its cause was not diagnosed. The user also found
that the lips do not open cleanly in either workflow because of how this mesh represents the mouth.
Whether the upper and lower lips actually share vertices, or merely have coincident vertices,
still needs a topology check before choosing an asset fix.

For this Megan asset, continue refining the morph-target `jawOpen` first. A skeletal jaw lane is an
optional later investigation if the morph result fails the needed poses; there is no current need
to spend time repainting skeletal weights merely to compare the two authoring paths. This is a
workflow choice from a visual trial, not acceptance of the final anatomical quality or proof that
skeletal deformation is generally unsuitable.

The user then saved `Megan-morph-target-test.msh` with the authored `jawOpenTest` target. That MSH
also contains `test-mandibula`, a diagnostic target that moves only the lower lips down on Y to
inspect the apparent upper/lower-lip attachment. The user considers the first Megan target test
complete. This records the asset and authoring result; the exact lip vertex-sharing relationship
was not reported. The later project-after-export reload failure is recorded above.

After using the maximum Soft edge setting for the jaw rotation, the user observed that the lips
open and saved both the facial project and the exported `Megan-morph-target-test.msh`. The saved
project now names that MSH as its mesh source and includes target-selection masks
`mandibula_inferior` and `test_lips`. The lower-lip corners still look stretched. This visual
result shows the mesh can produce a visible opening with the current morph workflow; it does not
by itself prove that no vertices are shared anywhere along the mouth. The next local cleanup is
to inspect the corner vertices and adjust the authored deltas there. The existing Paint Target
Smooth/Move brushes and a narrowly scoped target mask can be tried before adding another tool.

### Manual validation - jaw opening and lip cleanup (2026-10-31)

The user duplicated `jawOpenTest` into `jawOpenTest_copy`, then created the `labioInferior` target
mask from an open boundary loop. Paint Target Smooth pulled the lips together again in this trial;
the Move brush on the lower-lip selection opened them. The supplied screenshot shows a visible gap
between upper and lower lips. The user saved `Meg-facial-rig.lua` and exported
`Megan-morph-target-test.msh` again. A read-only check found the project references that MSH and
contains `jawOpenTest_copy` plus `labioInferior`; both files exist with updated timestamps.

This meets the initial goal of authoring a jaw-opening morph in the native Facial Rig Editor and
persisting the result without relying on Blender for the target edit. It does not establish the
quality of every preview weight or runtime clip, nor identify the exact lip topology. Smooth's
result here is a concrete reason to keep Move for localized lower-lip corrections and consider a
more controlled surface-aware smoothing tool later.

### Remaining quality checks for the Megan workflow

1. Inspect the authored target at weights 0, 0.5, and 1 in the Facial Rig Editor and Mesh Debug.
   Check the upper face, chin, neck, nape, and lip corners.
2. If a mouth-edge artifact remains, inspect topology to distinguish shared lip vertices from
   coincident but independent edges. A selection mask cannot change topology.
3. Reopen the saved facial project in the editor after the patch-reconciliation fix, save it once
   to remove the redundant patch, and confirm the target and mesh association in a fresh session.

The MSH write/read path for native targets has passed its first manual acceptance. The 3D pivot
tool is available and has been used for the jaw trial. The user validated a visible morph-only jaw
opening and saved the result. The broader expression/clip runtime milestone still needs a complete
playback check; further anatomical review can refine the asset without reopening the initial
authoring goal.

### Recommended order after the jaw comparison

1. The initial Megan target test is complete: `jawOpenTest` and the lower-lip diagnostic target
   were saved in `Megan-morph-target-test.msh`. Later quality checks can inspect the mouth-edge
   topology, compare weights 0, 0.5, and 1, and reopen the project after export. Keep mesh
   topology repair in the asset pipeline if the lips share vertices; a selection mask cannot
   separate them.
2. Improve target sculpt quality where the jaw trial exposed friction: a controllable soft edge
   for masked batch transforms is now available; next, show the textured draft before Apply and add
   a reliable normal-delta recompute/preserve choice. A more controlled smoothing tool may help
   corners; the current Smooth brush closed the lip gap in the Megan trial, while Move restored it.
3. Validate one complete morph-only expression and facial clip (`jawOpen`) through editor preview,
   MSH export, reload, and runtime playback. Then repeat with blink and an asymmetric expression.
4. Target mirroring and target-reference inspection are now available. Validate the mirrored
   result on the intended asset, especially when approximate matching is used. Revisit bone-driven
   expression authoring only for a demonstrated need; schedule OSC/vendor SDK capture after the
   native expression path is accepted.

## Why native authoring is needed

The Facial Rig Editor's `Expressions` worktree edits named weights for targets that are present in
the MSH. Morph target deltas are edited in the separate `Morph Targets` worktree. Existing targets
may originate in Blender Shape Keys or the optional Blender-assisted workflow. The MeshDebug
facial surface exposes target data for inspection and supports
project-authored facial clips. It now also has a first batch mutation API for adding, replacing,
and removing dense targets. Vertex selection, a first brush, and candidate preview now exist in
the editor; normal-delta recomputation remains to be implemented. Mirror analysis, visual preview,
and creation are available in their own mode, subject to neutral-geometry correspondence checks.

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

The 2026-10-31 manual comparison favors a morph-first approach for Megan because the skeletal
trial showed a nape break that the facial edit did not show. The cause of the skeletal break and
the lip topology are unverified. This narrows the immediate work without closing the diagnostic
or ruling out skeletal authoring for other assets.

The spike is complete when the desired result is measurable: the chin opens around the hinge,
upper-face landmarks stay stable, and the neck does not translate downward merely because the jaw
opens.

### Phase 1 - Native target data foundation

The first part of this phase is implemented. Continue extending `MESH_MBM_DEBUG` and its Lua
binding while keeping the facial asset PIMPL-owned.

- [x] create a target from the neutral frame, or duplicate an existing target;
- [x] rename it and edit its convention/default weight metadata;
- [x] read and write position deltas and optional normal deltas in batch form;
- [x] mirror a target across an editable X face plane while swapping recognized left/right target
  names; ambiguous or incomplete correspondence is diagnosed before creation;
- [x] remove a target transactionally, remap embedded clip weights, and synchronize the editor's
  named expression references;
- [x] validate finite values, vertex count/order, topology signature, target IDs/names, delta arrays,
  normal-delta presence, and the Mesh V11 target limit;
- [x] commit the entire candidate atomically, preserving the previous valid asset on failure.

The remaining Phase 1 work is further reference diagnostics for stale names in already imported
projects. Target-level expression/clip usage and migration on rename/delete are available. A
direct in-memory preview bridge remains a separate Phase 2 optimization.
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

Masked batch Translate/Rotate now has an optional Soft edge control from 0 to 8 edge rings. Zero
preserves the old hard selection. Selected vertices receive full influence, while adjacent vertices
on connected triangles receive a decreasing weight; disconnected surfaces at coincident positions
are not crossed. The same weights drive the rotation draft heat map and the committed deltas.
Reset-to-neutral and Paint do not use this control. It changes target deltas, not rig skin weights,
and cannot split vertices shared by both lips. The expanded selection is computed only when the
mask, topology, or ring count changes, not on each idle frame.

### Phase 3 - Jaw/head skeletal authoring lane

Add an explicit skeletal lane for expressions whose correct solution is a bone transform rather
than a vertex delta. For Megan, this lane is deferred while the morph-first jaw result is refined;
revisit it if that result fails the required poses or when another asset needs bone-driven motion.
It should use the canonical skeleton already bound by the Facial Rig Editor.

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
