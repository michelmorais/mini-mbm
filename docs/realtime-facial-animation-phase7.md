# Real-Time Facial Animation - Phase 7 dedicated facial rig authoring editor

The project's primary next step is a standalone facial rig authoring editor. It follows the
workflow users recognize from Faceit while keeping the project state and output under mini-mbm
control. Faceit remains an interoperability reference and an optional Blender-side tool; the
engine does not need to embed the Faceit addon.

Faceit describes a similar sequence: register target objects and expressions, place landmarks,
generate an armature from the landmarks, bind geometry, and then edit expressions. Its documentation
also distinguishes main face, eyes, teeth, tongue, and other geometry during binding:

- https://faceit-doc.readthedocs.io/en/latest/mocap_setup/
- https://faceit-doc.readthedocs.io/en/latest/rigging/
- https://faceit-doc.readthedocs.io/en/latest/expressions/

## Compatibility boundary

Compatibility is defined at the data and workflow boundaries:

1. **Names and weights** - ARKit-style names, stable target identities, finite weights, and the
   existing MSH facial sections.
2. **Facial parts** - semantic roles for the main face, eyes, teeth, tongue, brows, hair, and
   optional accessories. A role can refer to a whole object or to a vertex mask.
3. **Landmarks and rig description** - named points, parent relationships, bind transforms, and
   bone identities that can be sent through the existing Blender skeleton bridge.
4. **Expressions and clips** - authored targets, corrective edits, mirrored pairs, and sampled
   named clips.

This is workflow compatibility with Faceit, not compatibility with Faceit's private Blender
database or internal operators. A project authored in the mini-mbm editor should be exportable to
Blender for continued work, and a Blender/Faceit asset should remain importable when it contains
the supported names, objects, shape keys, and animation data.

## What the mask means

The setup mask is a preparation and binding mask. It identifies which vertices or objects belong to
each facial role. It is separate from a runtime expression weight:

- region masks answer **which geometry is allowed to be affected**;
- the face reference origin answers **where the facial rig is placed**;
- landmarks answer **where facial features are placed relative to that origin**;
- expression weights answer **how much a target is active at runtime**.

The first implementation should prefer stable frame-0 vertex indices and object references. It
must reject masks whose topology no longer matches the registered mesh instead of silently
assigning weights to a different vertex order. Each role uses a registered source-object catalog:
the role table selects `unassigned` or one of the known object names, while a small registration
field adds an exact Blender object name when the imported mesh does not expose that catalog yet.
Blender exports now write the visible mesh-object names to a sidecar named
`<mesh>_mbm_objects.lua`; the facial editor merges that catalog into the project and still keeps
manual entry and common naming suggestions for assets that were exported without the sidecar.

## Planned editor flow

### Setup

- Load a mesh or Blender-backed project.
- Register one or more facial objects.
- Assign object roles and inspect the base pose.
- Create, rename, validate, and save region masks.

Initial roles are main face, left/right eye, upper/lower teeth, tongue, brows, nose, mouth, hair,
and optional accessory. Roles remain extensible so a creature such as Fuzzy Fury can use a tongue
or muzzle without pretending it is a human face.

### Landmarks

- Place and edit bilateral and midline landmarks.
- Snap to selected vertices or move in the 3D viewport.
- Validate required landmark groups before generation.
- Save landmark coordinates in mesh-local space with the source geometry identity.

The minimum first landmark set covers face bounds, eye centers and corners, nose bridge/tip,
mouth corners, upper/lower lip, chin, jaw hinge, and brow anchors. The exact generated bone
layout is a later milestone and must not be hidden inside the landmark editor.

### Rig generation and binding

The editor will generate a canonical rig description and pass it through the existing Blender
bridge where Blender-side armature and weight operations are the most compatible. The editor must
show a preview and a validation report before committing the result. Re-generation must preserve
the project masks, landmarks, expression definitions, and explicitly authored corrections.

The initial binding strategy is conservative:

- eyes use dedicated eye roles and can be assigned to eye bones;
- teeth use upper/lower rigid roles;
- tongue uses a dedicated role with optional bone chains;
- the main face uses the generated facial skeleton and weights;
- hair and accessories use explicit opt-in roles rather than accidental proximity weights.

### Expressions and export

After a rig can be previewed, the editor will create or import ARKit-compatible targets, support
mirroring and corrective edits, and sample clips into the existing facial runtime format. The
current Mesh Debug facial playback module remains the shared review surface until the authoring
editor has its own expression panel.

## Architecture and editor pattern

The facial editor is a separate Lua entry point, but its application shell follows
`editor/skeletal_animation_editor.lua`. This keeps the two tools familiar and lets them share
engine conventions without turning the skeletal editor into a collection of unrelated panels:

- the same launcher registration and localized application label;
- the same `tImGui`/`tUtil` setup, scene lifecycle, state-table organization, and workspace switching;
- the same viewport, camera navigation, selection, gizmo, status, diagnostics, and dirty-state
  conventions;
- the same event-driven save/load approach for editor state and generated Lua project files.

The facial editor keeps its own file and project schema. It is not a new workspace in the skeletal
animation editor, because the workflows have different primary data and validation rules. Its
workspaces are limited to facial authoring:

1. **Setup** - load the mesh, register objects, assign facial roles, and inspect the base pose.
2. **Face position** - place the facial reference wireframe on the mesh and move its origin with
   the X/Y/Z gizmo. This worktree owns the reference transform independently from landmarks.
3. **Landmarks** - place, edit, snap, and validate facial landmarks in the 3D viewport. The face
   combo contains only landmarks; the face bounds near the forehead and chin area are reference
   boundaries, and the chin landmark is separate.
4. **Masks/regions** - create and validate stable vertex or object regions after the facial reference
   points are established.
5. **Rig** - preview and validate the generated facial rig and binding description.
6. **Expressions** - author, mirror, correct, and inspect facial targets and static poses.
7. **Clips** - author temporal facial clips from expressions and target weights.
8. **Preview** - review target weights, facial clips, and skeletal/facial composition.

Skeletal-only features remain outside this editor: bone animation timelines, skeletal animation
layers, general skin-weight painting, and skeletal playback controls. A narrowly scoped shared
routine is allowed when it is required to bind facial geometry, but it must not expose the whole
skeletal workflow in the facial UI.

The proposed modules are:

- `facial_rig_editor.lua` - lifecycle, viewport, facial workspaces, and dirty-state orchestration;
- `facial_rig_project.lua` - project schema, role/mask/landmark validation, and save/load model;
- `facial_rig_landmarks.lua` - landmark placement and geometric validation;
- `facial_rig_blender_bridge.lua` - transactional Blender export/import of rig descriptions;
- `facial_capture_mapping.lua` and `facial_capture_stream.lua` - downstream named-weight mapping and
  deterministic replay utilities.

Shared viewport and mesh utilities may be reused. Per-frame code must only draw UI or update an
actively edited gizmo; loading, mask scans, Blender exports, serialization, and rig generation
must be dirty or explicitly requested operations.

## Delivered first slice: editor shell and setup

The first implementation slice now provides:

- `editor/facial_rig_editor.lua` as a standalone launcher entry following the skeletal editor shell;
- `editor/facial_rig_project.lua` with versioned project state, role/mask/landmark persistence, and
  validation;
- setup and masks/regions workspaces with mesh loading, role registration, a source-object combo
  per role, and stable project-side mask records;
- a landmarks workspace with the required landmark schema, local coordinates, placed state, and
  validation feedback;
- rig, expressions, and preview workspaces reserved for the next authoring slices;
- Linux, macOS, and Windows-dev launcher registration plus English and Portuguese labels.

The editor currently loads `.msh` assets for the initial setup workflow. Blender/GLB import remains
the established Mesh Debug path; the facial editor now exports its canonical rig description to a
transactionally written Lua document for the Blender bridge.

The Blender bridge is optional for the mini-mbm runtime. A finished game only needs the facial data
stored in the `.msh` asset and the runtime target/clip APIs; it does not load the bridge Lua file or
start Blender. The bridge is required only for the current Blender handoff and preview workflow,
until native mesh binding and weight authoring are implemented in the facial editor.

## Delivered second slice: landmark viewport and binding data

The second implementation slice now provides:

- a frame-0 topology signature containing ordered vertex positions, subset boundaries, and subset index data;
- stale-project and stale-mask diagnostics when the loaded mesh no longer matches the saved topology;
- persistent landmark vertex associations when a landmark is snapped to the viewport;
- always-on-top landmark cross markers with selection feedback in the 3D viewport;
- the shared Mesh Debug orbit gizmo in the facial camera panel, including axis snap views;
- separate camera commands for framing the complete mesh, the facial landmark region, and the selected landmark, with face framing centered on the face origin and front-aligned;
- a human-face initial wireframe calibrated from the mapped Megan landmark project;
- a TreeNode workspace navigator with one facial worktree open at a time;
- fixed-size authoring and camera panels so localized labels and input fields keep a stable layout;
- localized landmark names, side labels, and validation diagnostics in English and Portuguese;
- a dedicated Face position worktree that anchors the facial reference wireframe to a mesh point such as the nose;
- editable face-reference scale around the origin, with placed landmarks scaled together;
- an always-on-top 3D facial wireframe with independent contour, eye, brow, nose, mouth, and jaw paths,
  selectable joints, and X/Y/Z translation axes;
- landmark coordinates stored relative to the face reference origin while preserving mesh-local positions;
- independent and synchronized landmark side editing, with synchronized sides mirrored across the nose X position and persisted in the facial project;
- click-to-select placed landmarks, Ctrl-click snapping to the nearest visible mesh vertex, and empty-space orbiting;
- Ctrl+Z/Ctrl+Y project history for facial placement, mask, role, and landmark edits;
- project files preserve the selected facial side-editing mode and preferred editor language;
- add/remove mask editing by nearest vertex in the Masks / Regions workspace;
- a review-only binding description summarizing enabled roles, masks, landmarks, and source topology;
- regression coverage for topology inspection, ray picking, project validation, and binding summaries.

## Delivered third slice: canonical rig preview

The rig workspace now turns the validated authoring data into a deterministic, review-only rig
description. It includes a face root, eye, brow, nose, jaw, and mouth bones whenever the required
landmark groups are available, plus explicit role bindings and mask references. The generated bones
are drawn in the 3D viewport with stable colors and are rebuilt only when project data changes.
The preview remains read-only: it does not create weights, alter the mesh, or export an armature.
Optional landmarks produce warnings and omit only the affected bone; missing core eye or mouth
landmarks keep the rig preview pending.

The landmark slice remains authoring data only. It does not generate automatic weights or runtime
expression targets; the following rig-preview slice adds read-only generated bones.

## Delivered fourth slice: Blender bridge interchange

The Rig workspace now exports `mini-mbm-facial-rig-bridge-1`, a Lua interchange document containing
the mesh path, topology identity, canonical bones, role bindings, masks, and a review report. The
writer uses a temporary file and an atomic rename so an interrupted export does not replace the
last valid document. Export remains read-only and does not create weights, alter the mesh, or run
Blender automatically.

Automatic weight generation, full Faceit rig parity, live capture, and production-quality facial
expressions remain outside this slice.

## Delivered fifth slice: Blender armature preview importer

`editor/facial_rig_blender_import.py` consumes the bridge document without executing Lua. It
accepts the generated hexadecimal Lua numeric format, resolves the serializer's shared-table
references, converts the engine's Y-up coordinates to Blender's Z-up convention, and creates a
`FacialRigPreview` armature. The existing `.blend` scene remains available when Blender is opened
with `-b existing.blend`; the importer stores topology and binding reports as armature metadata.
This importer is preview-only and never writes vertex groups or changes mesh geometry.

Example:

```sh
blender -b megan.blend --python editor/facial_rig_blender_import.py -- \
  --bridge facial-rig-meg-blender-bridge.lua \
  --output megan-facial-rig-preview.blend
```

The importer is also exposed through the Blender UI operator described below.

## Delivered sixth slice: Blender UI operator

`editor/facial_rig_blender_addon.py` registers a `Mini MBM Facial` panel in Blender's 3D View
sidebar. The panel opens a file picker for the bridge document, imports the preview into the
currently open `.blend`, reports bone and vertex-count checks, and displays the stored role
warnings. It uses the same Y-up to Z-up conversion as the headless importer and leaves saving to
the user. When the source `.blend` already contains an armature, the preview inherits its object
scale; this keeps engine coordinates aligned with Blender files whose source armature uses a
uniform scale such as `0.01`. The operator is still preview-only: it does not assign weights or
edit mesh geometry.

The panel also checks each bridge role against the names currently present in Blender. A missing
object is reported explicitly, rather than being guessed from similar names; this is needed for
assets such as Megan where the Blender objects and the merged MSH source use different names.

The next milestone is a mesh-aware review of facial role objects and the loaded `.msh` topology.
After that review, an explicit, opt-in weight assignment design can be implemented. This order keeps
object-name or scale mismatches from silently producing weights for the wrong vertex order.

In the Blender panel, `Engine X rotation = 90` is an import conversion setting, not a request to
rotate the resulting armature object afterward. Click **Import Facial Rig Bridge** after selecting
the bridge, then use Blender's normal **Save As** to save the `.blend`. Leave the armature object
rotation at zero unless the viewport review proves that the source asset uses a different axis
convention.

## Delivered seventh slice: Blender workflow launcher

The Rig workspace now keeps the exported bridge path and offers **Open Blender facial workflow**.
The button finds the `.blend` next to the loaded `.msh` when possible, otherwise asks for the source
`.blend`, detects Blender through the shared CLI wrapper, and starts Blender with
`editor/facial_rig_blender_addon.py -- --bridge <exported-bridge.lua>`. The addon imports that bridge
once Blender has finished loading, selects the visible `FacialRigPreview` armature, and keeps the
result and any error in the `Mini MBM` panel. The file-picker button remains available for manual
imports, and the user saves the resulting `.blend` manually. The button tooltip still documents the
manual eight-step procedure, including the `X rotation = 90` conversion setting.

This launcher does not assign weights. The Blender panel remains a preview-only armature import;
weights or facial targets visible after a Mesh Debug round trip come from the source skeletal asset
or the existing Mesh Debug import path, not from this launcher.

## Delivered eighth slice: mesh-aware binding review

The Rig workspace now includes a read-only binding review based on the loaded MSH topology. It
reports each enabled role's source mode, mask vertex count, invalid or stale mask data, and
cross-role vertex overlaps. Object-only roles are explicitly marked as having an unverified vertex
count because a merged MSH does not retain Blender object boundaries. This review does not assign
weights; it is the gate before designing an explicit weight-generation operation.

## Delivered ninth slice: explicit weight assignment plan

The Rig workspace now derives a deterministic, review-only weight assignment plan from the generated
bones, enabled roles, and binding review. The plan records the policy for each role (facial skeleton,
rigid, bilateral, or manual opt-in), candidate bones, masked vertex counts, and blockers. Object-only
roles remain blocked because a merged MSH does not expose their original Blender vertex boundaries.
The plan is descriptive and does not mutate the mesh; the next implementation can add an explicit
confirmation and transactional assignment operation once the source vertices are available.

## Delivered tenth slice: explicit rigid mask assignment

The Rig workspace now validates a safe first assignment operation against the loaded MeshDebug
asset. It accepts only explicit vertex masks or verified source-object maps, existing canonical
bones, and rigid role policies such as eyes and teeth. The operation builds a detached batch and sends it through the atomic
`setSkeletalVertexWeightsBatch` API; facial-skeleton, bilateral, unverified object-only, missing-bone, stale,
and unweighted cases remain blocked with a visible reason unless a later milestone adds their
dedicated policy. Applying changes memory only. A separate
Save As action writes a new MSH file, leaving the original asset untouched.

This slice intentionally does not infer Blender object boundaries, generate heat-map weights, or
create the canonical facial bones.

## Delivered eleventh slice: Blender source-vertex map

The direct Blender MSH exporter now writes `vertexMap` in `<mesh>_mbm_objects.lua`. It records the
flattened one-based MSH vertex indices for each visible Blender mesh object, preserving the source
boundary after material subsets are merged and split. The sidecar also stores the topology identity;
the facial editor imports this map into the project, validates its identity and every index against
the loaded topology, and marks object-only roles as verified when the map is present. Verified object
roles can enter the rigid assignment plan; old sidecars without a map remain safely blocked.

The map is metadata for validation and explicit assignment. It does not guess facial weights or
replace the need for canonical facial bones for the main face, brows, mouth, or other articulated
roles.

Facial authoring metadata remains in the project Lua file. A binary MSH stores geometry, canonical
bones, weights, and facial animation data, but it does not carry the editor's face origin, landmark,
mask, or role tables. Opening a derived MSH while a project with the same topology is active keeps
that project state; opening the MSH alone starts without those authoring tables.

When a Blender object sidecar with `vertexMap` is unavailable, the Masks / Regions workspace now
offers an explicit **Generate main face mask from landmarks** action. It selects the loaded MSH
vertices inside the placed landmark bounds, records the result as a normal topology-checked mask,
and leaves review and manual refinement to the user before weight assignment.

## Delivered twelfth slice: canonical facial skeleton materialization

The Rig workspace now has an explicit **Canonical facial skeleton** operation. It compares the
generated facial rig with the loaded MeshDebug canonical skeleton, reports existing and missing
bones, appends only missing generated bones, and reparents them while preserving their global bind
transforms. The operation keeps existing weights and animation tracks intact and removes newly
added leaf bones if any later step fails. It changes memory only; **Save modified facial MSH as...** writes
the result to a new file.

This slice creates the canonical bone identities required by the next weight-authoring milestone.
It does not infer facial influences or overwrite existing bone weights.

## Delivered thirteenth slice: facial weight preview and assignment

The Rig workspace now builds a read-only weight preview for the verified `main_face` role. It reads
the loaded MSH vertex positions once when the review becomes dirty, scores each vertex against the
generated facial bone segments, keeps up to four nearest influences, and normalizes their weights.
Rigid roles continue to use their explicit one-bone masks. The **Apply facial weight preview** button
sends the complete detached edit batch through `setSkeletalVertexWeightsBatch`; no vertex is changed
until the button is pressed, and **Save modified facial MSH as...** remains the separate persistence
step.

The first policy is intentionally deterministic and reviewable. It is a binding baseline for the
Megan asset; later milestones can add region-specific falloffs and artist overrides without changing
the transaction boundary.

### Topology-order safety for generated masks

The **Generate main face mask from landmarks** action now reads the canonical global topology
snapshot already used by landmark placement. This keeps mask indices aligned with the flattened
subset order and prevents generated masks from containing out-of-range or repeated vertex indices.
The read-only binding review also keeps its overlap sets separate from the serialized mask array,
so opening Rig cannot mutate valid vertex indices into boolean values.

The Rig diagnostics and binding summary are cached behind a dirty flag. Large topology scans and
the facial weight preview run when the project or mesh changes, not once per render frame while the
Rig workspace is idle.

## Delivered fourteenth slice: expression authoring

The Expressions worktree is now an authoring surface for facial targets already present in the
loaded MSH. It discovers target names and conventions once after a mesh change, exposes bounded
0..1 weight sliders, and applies changed values directly to the preview mesh without scanning or
re-uploading the full facial geometry every frame.

Named expressions are stored in the facial project as target-name-to-weight poses. The editor can
create a pose, capture the current target weights, apply a saved pose for visual review, rename it,
and mirror recognized `Left`/`Right`, `.L`/`.R`, and `_l`/`_r` target pairs. Validation rejects duplicate
pose names and references to targets that are not present in the loaded mesh. Mesh deltas remain in
the MSH asset; the project stores only authoring metadata and weights.

The Expressions worktree now exposes explicit `New expression`, `Edit expression`, and `Delete
expression` modes. New mode accepts only a new name and creates a new pose from the current weights;
it cannot overwrite an existing pose. Edit mode exposes the existing-pose combo, applies the selected
pose to the mesh when selection changes, and persists changes only through the explicit Update action.
Delete mode hides target editing and exposes only deletion of the selected pose.

This slice intentionally does not edit vertex deltas or write facial clips. The Preview worktree
consumes these named poses and imported clips; deterministic capture replay remains a later input
integration step.

## Delivered fifteenth slice: expression and clip preview

The Preview worktree now applies a selected saved expression to the visible mesh, reports the number
of active targets in that pose, and provides an explicit reset action that does not alter saved
project data. It also reuses the existing facial playback surface for imported facial clips,
including clip selection, play/pause/resume/stop, playback time, and runtime execution status.

Preview operations are event-driven: changing a pose or pressing Apply updates target weights once;
the editor does not rebuild facial data while idle. The project remains the source of saved
expressions, while MSH remains the source of target deltas and clips. Preview also exposes the
available skeletal clips and their play/pause/resume/stop controls, so a clip such as `MeganWalk`
can be reviewed together with a facial expression or facial clip instead of leaving the character
in its bind T-pose.

An individual target is a reusable geometry channel (for example `browUp`); an expression is a
named static combination of target weights. Start/end times belong to a clip, not to the static
expression pose.

## Delivered sixteenth slice: facial clip authoring timeline

The Clips worktree now provides a project-side timeline for authored facial animation. A clip stores
its name, duration, FPS, loop flag, interpolation mode, and ordered target-weight keys. The editor
can create and rename clips, move a time cursor, insert a key from a saved expression, capture the
current mesh weights, remove keys, scrub the result on the visible mesh, and play the authored clip
while the editor is open. Validation requires an ordered key at time zero and an end key at the clip
duration, matching the runtime facial clip contract.

This project representation is transport-independent. Exporting a clip to Blender as a Shape Key
Action is optional interoperability, not a runtime requirement. Native MSH clip serialization is a
separate follow-up slice; until it is implemented, imported MSH clips remain available in Preview
and authored project clips can be reviewed directly in the editor.

## Delivered seventeenth slice: dedicated facial clip timeline window

Facial clip authoring now follows the existing animation-editor layout more closely. Clip metadata
and expression-source selection remain in the Clips worktree, while time scrubbing, playback,
expression-key insertion, weight capture, key selection, key removal, and validation live in a
dedicated resizable timeline window anchored to the bottom of the screen. Closing that window does
not discard the clip; it can be reopened from the Clips worktree. The interpolation selector keeps
a fixed width so its value remains readable independently of the main panel width.

## Delivered eighteenth slice: project facial clip preview

The Preview worktree now lists clips authored in the facial project separately from facial clips
already embedded in the loaded MSH. Project clips can be selected, scrubbed, played, stopped, and
validated directly against the visible mesh; selecting one starts at time zero. This preview uses
the saved project target weights and does not require exporting a new MSH. MSH export remains gated
by the Rig weight-assignment flow because it materializes mesh skeleton and weight changes, while
project clip authoring remains safe metadata until a native MSH or Blender Shape Key export is
explicitly requested.

## Delivered nineteenth slice: explicit facial export destinations

The File menu now opens an export panel that distinguishes the three supported destinations:
the facial project Lua (which preserves expressions and authored clips without materializing weights),
the optional Blender bridge Lua (which carries authoring and topology interchange data), and a
runtime MSH (which requires the facial skeleton and weight assignment to have been applied in Rig).
The runtime MSH option remains disabled until that materialization is complete, and the panel states
that authored project clips are not embedded in MSH by the current native writer.

The runtime MSH page also exposes the prerequisite operations directly: it inspects the canonical
facial skeleton, adds missing facial bones, rebuilds the weight-assignment plan, applies the facial
weight preview, and only then enables Save runtime MSH as. The user no longer needs to open the Rig
worktree to materialize these requirements.

### Full-body skeleton compatibility

When an existing full-body skeleton exposes a body bone named `Head` (including common names such
as `mixamorig2:Head`), the facial skeleton binding attaches `face_root` to that bone while
preserving the generated global bind transform. This keeps facially weighted vertices moving with
the animated character body. Head-only assets without a suitable body anchor continue to use a
standalone facial root.
When the body anchor is required but not yet repaired, the weight-assignment action remains
blocked so a full-body mesh cannot be saved with a detached facial head.
