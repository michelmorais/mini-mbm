# Bones, Armatures, and FBX in Mini MBM

This document describes the current skeletal-animation and FBX interchange contract. The runtime
contract is canonical Mesh V11; Blender is the interchange tool used by Mesh Debug for FBX import
and export.

For the runtime API, backend behavior, playback, skinning method selection, and editor surface,
see [Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md). For the binary
sections, see [Mesh V11 format](mesh-v11-format.md#6h-canonical-skeletal-runtime-persistence).

## Current skeletal asset

A real-time skeletal `.msh` contains one bind/rest geometry and three linked canonical sections:

- **Section 41**: parent-first hierarchy with stable bone IDs, local bind translation, normalized
  quaternion rotation, scale, and authoring tail/connectivity metadata.
- **Section 42**: one weight record per bind vertex, with up to four positive, normalized
  influences referencing the section-41 bone palette.
- **Section 43**: named clips containing local translation, quaternion rotation, and scale tracks
  addressed by stable bone ID.

The runtime derives global bind and inverse-bind matrices from section 41. It evaluates local
tracks into global poses using row-vector composition:

```text
globalPose     = localPose * parentGlobalPose
skinningMatrix = inverseGlobalBind * globalPose
```

LBS and rigid DQS consume the same canonical pose. The requested method and execution path are
resolved when the mesh loads; they do not change per clip or per frame.

## FBX import through Blender

Mesh Debug launches Blender headlessly through `editor/blender_mesh_importer.lua` and
`editor/blender_mesh_export.py`. The exporter accepts Blender-importable FBX and GLB sources. The
Mesh Debug preference **Prefer real-time skeletal animation when available** selects the canonical
path when all of the following are true:

1. the selected scene armature has bones;
2. a visible mesh is controlled by that armature through an Armature modifier; and
3. the mesh has usable matching vertex-group weights.

The exporter-selected armature is the first armature in the imported Blender scene. The canonical
path writes one REST geometry frame plus sections 41, 42, and 43. It does not duplicate complete
mesh frames for each sampled skeletal key.

### Actions and clip selection

The import scan discovers skeletal sources from:

- the active Armature Action;
- detached `bpy.data.actions` whose curves target bones of the selected armature; and
- compatible Armature NLA strips.

Explicitly selected sources are sampled as separate canonical clips. The Blender scene range is
used only when no explicit skeletal source is available. Each sampled clip keeps its source frame
range and sample step; sampled keys are stored as local T/R/S tracks in section 43.

### Bind reference and object transforms

The bind geometry and bind skeleton are evaluated from the same reference state. At the canonical
bind frame, the exporter:

- captures the imported mesh and armature object transforms;
- temporarily disables object Actions and NLA evaluation;
- evaluates armatures in REST pose;
- restores the captured object transforms;
- extracts the bind hierarchy and exports the single REST geometry frame; and
- restores Blender's animation state before sampling clips.

This keeps object-level transforms from being applied inconsistently between the mesh and the
skeleton. When an FBX clip carries root motion in the armature/object transform, that motion is
included once while the clip's pose tracks are sampled. It is not baked into the REST geometry a
second time.

### Weights and fallback modes

During canonical export, vertex groups are limited to the four strongest influences and normalized
to a sum of one. The invalid-weight policy is configurable:

- **Stop and explain** reports incompatible meshes and affected vertex counts;
- **Skip incompatible objects** omits affected mesh objects; and
- **Keep all objects using baked frames** uses the geometry-frame fallback.

If no usable armature and weights are available, the importer uses baked/static geometry frames.
The `vb_only` large-mesh mode is also a geometry fallback and intentionally does not emit canonical
section-42 weights.

## Coordinate and material conversion

Mesh Debug's default FBX import settings use V inversion and a `-90` degree X rotation. The
canonical Blender-to-Mini-MBM mapping is:

```text
position_engine = (-x_blender,  z_blender, -y_blender)
normal_engine   = (-nx_blender, nz_blender, -ny_blender)
```

Because this mapping includes a reflection, the exporter reverses each triangle's winding while
leaving UV coordinates unchanged. Bone bind and animated pose matrices use the same coordinate
change by conjugation before being converted to Mini MBM's row-vector convention. Positions,
normals, triangle winding, bind matrices, and animation tracks therefore remain in one consistent
coordinate space.

Packed or embedded FBX textures are extracted from Blender's loaded image data when necessary and
written beside the generated `.msh`. Recorded source paths from the originating machine are not
required to resolve the material.

## MSH to FBX export

`editor/blender_mesh_skeleton_export.py` reconstructs a Blender armature from the canonical
sections, restores the bind hierarchy and weights, and creates one Blender Action per section-43
clip. The generated FBX can therefore be opened in Blender or uploaded to services such as Mixamo,
then imported back through the canonical Blender path with multiple skeletal clips preserved.

Mini MBM loop metadata is not a standard FBX playback contract. On re-import, clip looping follows
the importer/runtime default unless the resulting MSH is configured separately.

## Skeletal sockets

The direct FBX-to-MSH importer recognizes optional Blender Empty objects as named skeletal sockets.
An Empty is exported when it is parented to the selected armature with `parent_type = "BONE"` and
either has the custom property `mini_mbm_socket = true` or its name starts with `MBM_SOCKET_`.
The socket name comes from `mini_mbm_socket_name` when present; otherwise the `MBM_SOCKET_` prefix
is removed. Its transform is stored relative to the bind pose of the parent bone in optional MSH
V11 section 47. Existing FBX files without these objects and existing MSH files are unchanged.

Sockets do not alter weights or animation tracks. Runtime code can enumerate them with
`mesh:getSkeletalSocketReport()` and query an evaluated pose with
`mesh:getSkeletalSocketTransform(name, "world")`; the attachments plugin accepts the same point as
`sourceType = "socket"` and `sourceSocket = name`.
The reverse MSH-to-FBX exporter recreates section-47 sockets as Blender Empty objects with the same
bone parent and local bind transform. Mesh Debug also exposes socket creation/editing in its
Skeletal panel using local bind-space TRS; direct viewport picking and a socket gizmo remain a
separate editor increment.

## Runtime and editor boundaries

Canonical skeletal data is consumed by the real-time runtime and the Skeletal Animation Editor.
Static-frame animation, articulated rigid-subset animation, vertex-cache animation, and facial
shape-key animation are separate capabilities and are not interchangeable with section-41/42/43
skinning.

The complete runtime methods, CPU/GPU execution policies, root-motion API, pose sharing, and
validation rules are maintained in
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md).
