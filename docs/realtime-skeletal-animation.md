# Real-Time Skeletal Animation and Editor

Status: **Implemented and validated on OpenGL ES, DirectX 9, and Metal**
Last verified: **2026-08-19**

## 1. Scope

Mini MBM supports real-time bone animation for canonical `.msh` meshes. A skeletal asset stores one
rest geometry, a bind hierarchy, per-vertex bone influences, and bone-local animation tracks. At
runtime the engine evaluates a pose per mesh instance and deforms positions and normals with Linear
Blend Skinning (LBS) or rigid Dual Quaternion Skinning (DQS).

This system is distinct from:

- baked/static mesh animation, which stores complete geometry frames;
- articulated animation, which moves rigid mesh subsets;
- vertex-cache or Mesh Sequence Cache animation, which has no required skeleton to evaluate.

The Skeletal Animation Editor authors, inspects, repairs, and previews the same canonical data and
runtime deformation paths used by a game.

## 2. Canonical Asset Data

Mesh V11 stores the runtime contract in three linked sections:

| Section | Contents |
|---|---|
| `SECTION_SKELETAL_SKELETON` (41) | Parent-first bind hierarchy, stable nonzero bone IDs, local translation, normalized quaternion rotation, scale, and authoring tail/connectivity metadata |
| `SECTION_SKELETAL_WEIGHTS` (42) | One record per rest-pose vertex, up to four positive normalized influences, referencing stable bone IDs through a palette |
| `SECTION_SKELETAL_ANIMATION` (43) | Named clips containing bone-ID-targeted local translation, quaternion rotation, and scale tracks |

The loader validates section relationships, hierarchy ordering, stable identities, weights, tracks,
finite values, and bind transforms before making the asset available to the runtime. Global bind and
inverse-bind matrices are derived internally. Multiple `parentIndex = -1` roots are supported as
independent hierarchies.

The complete byte layout and validation rules are documented in
[Mesh V11 format](mesh-v11-format.md#6h-canonical-skeletal-runtime-persistence-design-and-implementation-status).

## 3. Pose Evaluation and Skinning

Each loaded `MESH` instance owns its playback state, evaluated local/global pose, root-motion
history, composition state, and final palette. Cached mesh resources do not share mutable player
time or pose state.

The engine uses row-vector skeletal math:

```text
globalPose       = localPose * parentGlobalPose
skinningMatrix   = inverseGlobalBind * globalPose
```

The evaluated pose is backend-neutral. LBS and DQS consume the same pose and weights; only palette
construction and vertex deformation differ.

### LBS

LBS stores three `float4` rows per bone. Its palette costs 48 bytes per bone per draw:

```text
LBS palette bytes = boneCount * 3 * 4 * sizeof(float)
```

The compact GPU profile supports rigid and positive-uniform-scale transforms. CPU LBS uses the
reference deformation path and inverse-transpose normal handling. Unsupported GPU transform or
capacity cases can therefore resolve to CPU when execution policy is `auto` and the CPU path is
valid.

### Rigid DQS

DQS stores a real and dual quaternion, two `float4` values per bone. Its palette costs 32 bytes per
bone per draw:

```text
DQS palette bytes = boneCount * 2 * 4 * sizeof(float)
```

DQS performs per-vertex quaternion hemisphere alignment before blending. It is restricted to rigid
bind and clip transforms: authored scale or shear is rejected rather than silently discarded.

### Method policy

The requested skinning method is selected before loading:

- `auto` selects rigid DQS when the complete bind and all clips are DQS-compatible; otherwise it
  selects LBS and records the reason;
- `lbs` requires LBS;
- `dqs` requires rigid DQS and reports incompatible scale explicitly.

Method selection never changes per clip or per draw.

## 4. GPU and CPU Execution

The requested execution path is also selected before loading:

- `auto` is the default, prefers GPU for the resolved method, and falls back to CPU when GPU cannot
  support that method and CPU deformation is available;
- `gpu` explicitly requires the GPU path;
- `cpu` explicitly requires CPU deformation.

CPU LBS and rigid DQS deform immutable rest geometry into per-instance dynamic vertex buffers. The
runtime report exposes requested/resolved method, requested/resolved execution path, preparation
status, reason, bone usage/limit, and execution readiness.

### GPU palette capacity

The engine reserves eight vertex-shader vectors for non-skeletal scene data. LBS consumes three
vectors per bone and DQS consumes two. Every backend also obeys the engine-wide operational ceiling
of 1,024 bones per draw; a smaller measured backend/device limit remains authoritative.

| Backend | Palette transport and effective limit |
|---|---|
| OpenGL ES | Measured vertex-uniform vectors minus the engine reserve, divided by 3 for LBS or 2 for DQS, capped at 1,024 |
| DirectX 9 | Measured Shader Model 3 vertex constants minus the engine reserve, divided by 3 or 2, capped at 1,024 |
| Metal | Buffer-backed palette capacity derived from `maxBufferLength`, capped operationally at 1,024 |

A report such as `lbs-bones=88/1024` means that the loaded draw uses 88 bones and the effective
limit for that method on the active backend is 1,024. `status=ready` describes the shared canonical
GPU input; method-specific support must still be read from the LBS and DQS usage/limit fields.

Generated default vertex shaders implement skeletal deformation before lighting on all three
backends. Fragment-only custom shaders preserve generated skeletal deformation. A custom vertex
shader has no canonical skeletal input contract and is rejected explicitly for a skeletal mesh.

## 5. Playback and Composition

The runtime and Lua surface support:

- clip enumeration, duration lookup, play/restart, pause, resume, stop, seek, and current time;
- finite nonnegative per-instance playback speed;
- authored loop policy;
- one-shot `onEndAnim(mesh, clipName)` delivery for non-looping base and layer completion;
- direct linear cross-fade from the active base clip to another base clip;
- one transient second clip in Absolute or bind-relative Additive mode;
- independent layer time, seek, pause, weight, and linear fade;
- stable-ID per-bone layer-mask multipliers from 0 through 1;
- transactional pose replacement: invalid mutations preserve the prior valid pose and palette.

The base plus optional layer is composed in local TRS, after which the hierarchy and one final
LBS/DQS palette are rebuilt. Layer state and layer masks are per-instance runtime state and are not
serialized into the mesh.

The complete Lua signatures and return values are documented in
[Lua API - Canonical skeletal playback](lua-api.md#canonical-skeletal-playback-mesh-gpucpu-lbsdqs-profile).

## 6. Gameplay Bone Transforms and Root Motion

`mesh:getSkeletalBoneTransform(name, space)` returns a copy of a named bone from the final evaluated
base-plus-layer pose. Model and renderizable-composed world space are supported. The result contains
position, engine-order Euler XYZ radians, normalized quaternion rotation, scale, and matrix. The
Euler value is directly consumable by engine angle APIs; the quaternion preserves the lossless
evaluated rotation.

`mesh:getSkeletalRootMotionDelta(name, space)` exposes the non-consuming translation delta between
continuous evaluated poses. Pause, seek, direct clip replacement, stop, authoring poses, and loop
wraps invalidate history instead of emitting a teleport delta.

Automatic root motion can apply the selected bone's continuous translation to the mesh transform
and neutralize that local translation in the rendered pose. Optional rotation applies the analogous
normalized-quaternion delta and neutralizes the selected local rotation. Translation-only is the
default.

## 7. Pose Sharing

A compatible loaded mesh can become a direct follower of another loaded mesh and use the source's
already-evaluated palette without copying it. This supports separate body/clothing meshes sharing a
pose while retaining independent geometry, weights, textures, and render transforms.

Compatibility requires matching ordered stable bone identities, names, hierarchy, and equivalent
bind transforms. The resolved skinning method and execution path must also match while sharing is
active. Self-sharing, follower chains, unloaded objects, and incompatible skeletons are rejected.
Reload, release, or destruction unlinks relationships safely.

Pose-sharing links are runtime-only. They do not transfer ownership of the source player or
serialize a body/wearable relationship.

## 8. Blender and FBX Interchange

Mesh Debug's Blender importer can prefer real-time skeletal animation when a selected armature has
bones and usable matching skin weights. It then writes one REST geometry and canonical sections
41-43 instead of duplicating a complete mesh for every sampled frame.

The import scan discovers explicit Armature Actions and compatible NLA sources and selects all
explicit skeletal sources by default. Scene Range is used only as a fallback when no explicit source
exists. If canonical skeletal import is unavailable, the UI reports the reason and uses baked/static
frames; users can still explicitly request one static frame.

Mesh Debug's FBX export reconstructs the canonical armature, weights, and one Blender Action per
canonical clip. Animation samples are transferred as deltas from the canonical bind to Blender's
reconstructed rest pose, preserving scaled-armature animation through tested MSH -> FBX -> MSH
round-trips.

The detailed coordinate, bind, skin-cluster, and interchange contracts are documented in
[Bones, Armatures, and FBX](bones-armatures-and-fbx.md).

## 9. Skeletal Animation Editor

The standalone editor contains five mutually exclusive worktrees:

| Worktree | Capabilities |
|---|---|
| Bone Editor | Create and edit joints/bones, hierarchy, head/tail/connectivity, bind transforms, chains, mirroring where compatible, referenced removal, and transactional Undo/Redo |
| Bind Pose Contract | Inspect canonical hierarchy, stable IDs, local/global/inverse-bind transforms, validation findings, and bind skeleton selection |
| Runtime Skeletal Preview | Run the actual backend player, select LBS/DQS and GPU/CPU policies, compare LBS against DQS or GPU against CPU, inspect evaluated skeleton/masks, root motion, and transient wearable followers |
| Create / Edit Animations | Create/edit clips, tracks and T/R/S keys; manipulate poses; use easing, timeline selection/drag/duplicate/ripple/time editing, playback, clipboards, and Undo/Redo |
| Paint Weights | Paint/add/replace/rigid brushes, masks and regional tools, smoothing, influence limiting/cleanup, seam and normal diagnostics/repair, pose-safety checks, heatmaps, and persistent canonical type-42 output |

The editor preview uses the same runtime pose evaluation and deformation path as a game. Secondary
comparison instances are synchronized only when their playback clocks drift; idle editor paths use
dirty flags, change detection, caching, and throttling for expensive work.

The complete user workflow is documented in the
[Skeletal Animation Editor guide](skeletal-animation-editor.md).

## 10. Validated Runtime Evidence

| Platform/backend | Recorded evidence |
|---|---|
| Linux/OpenGL ES | Mesa OpenGL ES 3.2, 12,216 vertices and 88 bones: LBS `88/1024`, 4,224 bytes; DQS `88/1024`, 2,816 bytes; both methods visually accepted |
| Windows/DirectX 9 | The 88-bone reference exceeds the measured 82-bone LBS limit and uses Auto CPU fallback; it fits the 124-bone DQS limit and runs DQS on GPU; GPU and CPU execution were visually accepted. The native encoded/readback parity harness passes synthetic and Lorekeeper LBS/DQS positions and normals against the shared CPU references |
| Windows/OpenGL ES/ANGLE | The same 88-bone reference reports LBS and DQS `88/1024`; GPU and CPU execution were visually accepted. Native RGBA8 readback passes all four shared synthetic/Lorekeeper LBS/DQS parity cases on OpenGL ES 3.0 through ANGLE's Direct3D 11 backend |
| macOS/Metal | Apple M4 production-path LBS/DQS validation passed with the committed Lorekeeper; an 88-bone, 13,111-vertex real mesh also ran Auto-resolved GPU LBS at `88/1024` with a 4,224-byte palette; native RGBA8 readback passes all four shared synthetic/Lorekeeper LBS/DQS parity cases with Metal API validation enabled |

Deterministic foundation tests cover canonical validation, sampling, easing, hierarchy evaluation,
bind identity, LBS/DQS reference deformation, antipodality, scale rejection, composition, masks,
root motion, execution policy, sharing compatibility, and the shared skeletal-parity case contract.
The parity suite builds backend-neutral synthetic and real-asset LBS/DQS inputs, CPU references,
RGBA8 encoding, tolerances, comparison, and reporting once. The OpenGL ES, DirectX 9, and native
Metal capture backends execute all four shared cases and read encoded GPU positions/normals for
comparison. DirectX 9 and Metal use the same generated LBS/DQS deformation source as their
production default shaders.

## 11. Current Capability Boundaries

- GPU LBS uses the compact rigid/positive-uniform-scale normal profile; non-uniform GPU normal
  deformation has no inverse-transpose palette profile.
- DQS is rigid and does not approximate authored scale or shear.
- A draw is limited to the active backend capacity and the 1,024-bone operational ceiling; there is
  no palette partitioning across draws.
- Composition contains one transient layer over one base clip; there is no serialized layer graph,
  arbitrary layer stack, priority queue, or non-linear-animation system.
- Pose sharing is direct, runtime-only, and non-persistent; there are no follower chains or shared
  serialized skeleton/clip resource objects.
- The engine does not retarget clips between different skeletons or proportions.
- Bone-subtree mirroring does not mirror animation tracks and is therefore restricted to assets
  without clips.
- Paint Weights has no protected/exclusion volumes, welded diagnostic topology, or automatic
  high-cost whole-mesh weight generator.
- The editor is not a Blender-equivalent IK, constraint, graph, topology-modeling, automatic-rigging,
  corrective-shape, muscle, or procedural-tail suite.
- FBX support covers the measured armature, weights, Actions/NLA, and canonical round-trip workflow;
  it is not a claim that every FBX animation/deformer feature is supported.
- Velocity Skinning, compute skinning, and a new modern OpenGL/Vulkan renderer are not part of the
  implemented skeletal capability.
- Numeric encoded/readback CPU/GPU comparison is automated for OpenGL ES, DirectX 9, and Metal.

Optional projects outside this accepted capability are tracked separately in the
[Deferred Work Plan](realtime-skeletal-animation-future-work-plan.md).
