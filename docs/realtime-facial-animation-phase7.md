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

## Architecture

The editor will be a separate Lua entry point, not a new workspace inside the skeletal animation
editor:

- facial_rig_editor.lua - lifecycle, viewport, setup panels, and dirty-state orchestration;
- facial_rig_project.lua - project schema, role/mask/landmark validation, and save/load model;
- facial_rig_landmarks.lua - landmark placement and geometric validation;
- facial_rig_blender_bridge.lua - transactional Blender export/import of rig descriptions;
- facial_capture_mapping.lua and facial_capture_stream.lua - downstream named-weight mapping and
  deterministic replay utilities.

Shared viewport and mesh utilities may be reused. Per-frame code must only draw UI or update an
actively edited gizmo; loading, mask scans, Blender exports, serialization, and rig generation
must be dirty or explicitly requested operations.

## Next implementation milestone: setup and landmarks

The first implementation slice will provide:

1. a saved facial project state;
2. object/role registration;
3. stable vertex-region masks;
4. landmark placement and editing;
5. topology and required-landmark validation;
6. a reviewable setup report for the Fuzzy Fury and Megan fixtures.

It will not yet claim automatic binding, full Faceit rig parity, live capture, or production-quality
facial expressions. Those depend on the setup data being inspectable and stable first.
