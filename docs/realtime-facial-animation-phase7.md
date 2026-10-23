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
- landmarks answer **where the facial rig is placed**;
- expression weights answer **how much a target is active at runtime**.

The first implementation should prefer stable frame-0 vertex indices and object references. It
must reject masks whose topology no longer matches the registered mesh instead of silently
assigning weights to a different vertex order.

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
2. **Masks/regions** - create and validate stable vertex or object regions.
3. **Landmarks** - place, edit, snap, and validate facial landmarks in the 3D viewport.
4. **Rig** - preview and validate the generated facial rig and binding description.
5. **Expressions** - author, mirror, correct, and inspect facial targets and clips.
6. **Preview** - review target weights, facial clips, and deterministic capture replay.

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
- setup and masks/regions workspaces with mesh loading, role registration, object assignment, and
  stable project-side mask records;
- a landmarks workspace with the required landmark schema, local coordinates, placed state, and
  validation feedback;
- rig, expressions, and preview workspaces reserved for the next authoring slices;
- Linux, macOS, and Windows-dev launcher registration plus English and Portuguese labels.

The editor currently loads `.msh` assets for the initial setup workflow. Blender/GLB import remains
the established Mesh Debug and Blender bridge path until the facial editor's bridge workspace is
implemented.

## Delivered second slice: landmark viewport and binding data

The second implementation slice now provides:

- a frame-0 topology signature containing ordered vertex positions, subset boundaries, and subset index data;
- stale-project and stale-mask diagnostics when the loaded mesh no longer matches the saved topology;
- persistent landmark vertex associations when a landmark is snapped to the viewport;
- always-on-top landmark cross markers with selection feedback in the 3D viewport;
- the shared Mesh Debug orbit gizmo in the facial camera panel, including axis snap views;
- click-to-select placed landmarks and click-to-snap the selected landmark to the nearest visible mesh vertex;
- add/remove mask editing by nearest vertex in the Masks / Regions workspace;
- a review-only binding description summarizing enabled roles, masks, landmarks, and source topology;
- regression coverage for topology inspection, ray picking, project validation, and binding summaries.

This slice remains authoring data only. It does not generate facial bones, automatic weights, or
runtime expression targets.

## Next implementation milestone: landmark viewport editing and binding data

The delivered landmark slice moves the next implementation milestone to rig preview and binding:

1. generate a canonical facial rig description from validated landmarks;
2. preview generated facial bones and role bindings without committing weights;
3. validate role masks and landmark groups against the generated rig;
4. produce reviewable setup reports for the Fuzzy Fury and Megan fixtures.

It will not yet claim automatic weight generation, full Faceit rig parity, live capture, or
production-quality facial expressions. Those depend on the generated rig description being stable
and reviewable first.
