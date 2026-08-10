# Bones, Armatures, and FBX in Mini MBM

This document explains skeletal animation (rigging/skinning) in game engines generally, how FBX
represents it, and precisely what Mini MBM has and does not have today. It exists because this
whole feature area was built up over many sessions chasing a single symptom (Mixamo animations
"melting" a re-exported character) whose real root cause turned out to be several layers deep — the
"Quick Mental Model" and "Pitfalls" sections below exist so the next person (human or AI) doesn't
have to re-derive any of that from scratch.

Region-based weight editing, rigid-core/falloff workflows, diagnostics, and validated usage are
documented in the [Skeletal Animation Editor guide](skeletal-animation-editor.md). The future
bind-pose pipeline and runtime LBS/DQS delivery are tracked in
[`realtime-skinning-animation-plan.md`](realtime-skinning-animation-plan.md).

Rigid subset animation is a separate implemented feature documented in
[`articulated-animation.md`](articulated-animation.md). It should not be confused with bone-weighted
skeletal deformation.

## Quick Mental Model (Read This First)

Skip this section if you already know how bones/skinning work in a modern engine — the rest of the
document is the precise reference.

**Mini MBM's status in one sentence:** the engine stores everything a real skeletal-animation system
needs (a bind-pose bone hierarchy, and real per-vertex bone weights), but nothing in the renderer
ever *reads* that data to deform a mesh. It is 100% editor/diagnostic + FBX round-trip data. Mini
MBM's deforming animation model remains **static frame swapping** — a flipbook of complete,
pre-baked vertex snapshots (`SECTION_FRAME_STATIC`, repeated per frame), while articulated animation
moves rigid subsets. The private Milestone-0 foundation can explicitly derive inverse bind and
sample a synthetic skeletal pose for validation, but there is no runtime skeletal player,
bone-matrix shader upload, or vertex deformation consumer.

Why store bones/weights at all, then? Because Mixamo (and every other rigged-character pipeline)
needs them to work at all — a character with no skeleton can't be animated by anything, and a
character with a skeleton but no *real* per-vertex weights gets Mixamo's own auto-rigger's geometric
guess instead of the mesh's actual intended deformation. Mini MBM's bones/weights feature exists so
`editor/mesh_debug.lua` can be a faithful *pass-through*: import a rigged FBX, let the user inspect
and tweak the skeleton, export a rigged FBX back out — all without the engine itself ever having to
understand skinning. This is why every relevant format section (`SECTION_FRAME_SKINNED`,
`SECTION_VERTEX_SKIN_WEIGHTS`) is explicitly documented as "never consulted by rendering" — that is
the actual, deliberate design, not an oversight.

## How Real-Time Skeletal Animation Works (Other Engines)

This section describes the general, industry-standard model (Unity, Unreal, Godot, and FBX itself
all implement some variation of this) — not Mini MBM, which is covered afterward.

### The rig: a hierarchy of bones

A **skeleton** (rig, armature — these are all the same concept under different names in different
tools: Blender calls it an Armature, Maya/Unreal/FBX call it a Skeleton, Unity calls it an
Avatar+bone hierarchy) is a tree of **bones** (joints). Each bone has:

- a **parent** (or none, for the root)
- a **local transform** relative to that parent: translation, rotation, and (usually) scale
- often a **length** or a **tail position**, purely for visualizing where the bone "points" —
  functionally this is cosmetic; only the joint's own position/rotation actually drives deformation

A bone's **world transform** (where it actually sits in space) is computed by walking from the root
down to that bone, composing local transforms along the way:

```text
boneWorld = parentWorld * boneLocal
```

This is **forward kinematics** — exactly the same math a robot arm's joint chain uses. There is no
shortcut; every bone's world transform depends on every ancestor's.

### Bind pose vs. animated pose

The **bind pose** (rest pose, T-pose) is the skeleton's position *at the moment the mesh was rigged*
— literally the pose the artist was looking at when they painted vertex weights. This pose is what
gets exported once, and never changes.

The **animated pose** is the skeleton's position *right now*, driven by whatever animation clip is
playing. Every bone's world transform is recomputed every frame from the current animated local
transforms via the same forward-kinematics formula above.

### The skinning formula (why an "inverse bind matrix" exists)

A vertex is authored once, in bind pose. To deform it correctly under the *current* animated pose,
for each bone influencing that vertex the engine needs to answer: "where would this vertex be if the
skeleton moved from its bind pose to its current pose?" That requires undoing the bind pose first,
then applying the current pose:

```text
skinnedVertex = Σ (weight_i * boneCurrentWorld_i * inverse(boneBindWorld_i)) * bindVertex
```

That expression uses the common column-vector notation. Mini MBM's matrices transform row vectors
(`vertex * matrix`), so the planned runtime contract reverses the written product:
`inverse(boneBindGlobal) * boneCurrentGlobal`, with child globals composed as
`boneLocal * parentGlobal`. The normative foundation and legacy-global conversion are specified in
the [Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md#milestone-0-normative-contracts).

`inverse(boneBindWorld_i)` is the **inverse bind matrix** — captured once, at bind time, per bone.
Multiplying it by the vertex first transforms the vertex out of world space into that bone's own
*local* space (relative to wherever that bone was at bind time); then `boneCurrentWorld_i` moves it
back out into world space, but following the bone's *current* position instead. This is called
**Linear Blend Skinning (LBS)**, and it is *the* standard technique — the "up to 4 bone influences
per vertex, weights summing to 1.0" convention (which Mini MBM's own
`SECTION_VERTEX_SKIN_WEIGHTS` already follows, see below) exists specifically because this formula
is a weighted sum, and real-time engines cap the influence count for a bounded, GPU-friendly cost.

### GPU vs. CPU skinning

- **GPU skinning** (what every modern real-time engine does): every bone's current
  `world * inverseBind` matrix is uploaded once per frame as a small array (a "bone matrix palette",
  either a uniform array or a texture for very large skeletons), and the *vertex shader* does the
  weighted-sum math per vertex, every frame, on the GPU. This is why influence count is capped — a
  fixed-size shader array needs a compile-time bound (the exact same reason `SUPPORTED_MAX_LIGHTS` is
  a compile-time cap in Mini MBM's own lighting system, see `docs/light.md`).
- **CPU skinning**: the same math, done once per frame on the CPU, writing deformed positions back
  into a vertex buffer. Simpler to implement, much more expensive at scale — mostly seen in older or
  very constrained engines, or as a fallback for skeletons too large/exotic for a GPU path.

Mini MBM implements **neither**. There is no code path in `src/render/` or any backend
(`shader-opengl_es.cpp`, `shader-directx9.cpp`, Metal) that uploads a bone matrix palette or performs
either of these computations.

### Animation clips and retargeting

An animation clip is, per bone, a set of **keyframed curves** (translation/rotation/scale over time
— rotation is almost always stored as a **quaternion** in real engines specifically to avoid gimbal
lock during interpolation, not Euler angles). Playing an animation means sampling/interpolating each
bone's curves at the current time to get that bone's *local* transform for this frame, then running
forward kinematics as above.

**Retargeting** is applying an animation clip authored for one skeleton onto a *different* skeleton
with a compatible bone hierarchy (same bone names/topology, different proportions). This is exactly
what Mixamo does: every character gets rigged onto the same "Standard Skeleton" bone-name
convention (`mixamorig:Hips`, `mixamorig:Spine`, ...), so any of Mixamo's animation library can drive
any Mixamo-rigged character regardless of that character's actual body proportions.

## FBX: Why It's the Standard, and How It Represents a Skeleton

FBX (Autodesk) is the de facto interchange format for rigged/animated 3D content between DCC tools
(Blender, Maya, 3ds Max) and game pipelines (Unity, Unreal, Mixamo, and this engine's own Blender
scripts) — not because it's technically the best-designed format, but because virtually every tool
in this space can read and write it, so it's the path of least friction for round-tripping a rigged
character between tools that otherwise share nothing.

### Bones are just scene-graph nodes

FBX has one generic concept for "anything positioned in a 3D scene": a **node**, with a local
translation/rotation/scale relative to its parent node — exactly the mesh objects, cameras, and
lights in the same scene use. A **bone is simply a node** with a special node-attribute
(`FbxSkeleton`) tagging it as a skeleton joint rather than a mesh or camera. There is no separate
"bone" data structure in FBX's own object model — structurally, a skeleton is just an ordinary
parent-child hierarchy of empties, the same way Blender's own Armature editing works (an Armature is
a collection of `EditBone`s, each with a parent, and Blender's bone `head`/`tail`/`roll` is just a
friendlier way to express a position + orientation).

This is *why* Mini MBM's own `SKELETON_BONE_V11` (`include/core_mbm/header-mesh.h`) mirrors this
almost exactly:

```cpp
struct SKELETON_BONE_V11 // one per bone; parentName empty ("") marks the root
{
    std::string name;
    std::string parentName; // must equal the `name` of a SKELETON_BONE_V11 already emitted earlier
    float x, y, z;          // bone position, same coordinate convention as the caller's mesh
    float radius;           // authoring-time gizmo marker size only, not skinning-relevant
    float rotX, rotY, rotZ; // Euler degrees, engine's own X-then-Y-then-Z composition order
    float scaleX, scaleY, scaleZ;
    float length;           // bone extent along its own local +Y axis (Blender's head->tail convention)
};
```

Name-based parent references (not indices) mirror how both Blender and FBX identify a bone: by name,
not by array position — this is also why Mini MBM's own `SECTION_VERTEX_SKIN_WEIGHTS` (below)
references bones by name too, not by index into this array (see Pitfalls).

This describes the current editor/round-trip format, not the planned runtime identity contract.
Runtime bones will use stable nonzero `uint64_t` IDs; names remain labels and legacy/interchange
lookup keys so rename and clip targeting do not depend on vector position or display text.

One real divergence from most engines: Mini MBM stores rotation as **Euler XYZ degrees**, not a
quaternion. This is fine for a *bind pose* (a single static orientation, no interpolation ever
happens on it) but would need converting to/from quaternions if this data were ever consumed for
real animation blending — Euler angles interpolate badly (gimbal lock, and shortest-path ambiguity)
and are essentially never used for runtime animation curves in real engines, only for one-shot
authoring-time values like this.

### The skin deformer: separate from the skeleton itself

FBX represents "which bones influence which vertices, and how much" as a *separate* object attached
to the mesh: an `FbxSkin`, containing one `FbxCluster` per influencing bone. Each cluster holds:

- the list of vertex indices it influences, and their weights (the actual weight-painting data)
- a `TransformLink` matrix — the bone's own world transform *at bind time* (this is where the
  inverse bind matrix comes from, per the skinning formula above)

This separation — skeleton (a hierarchy of nodes) vs. skin (a separate weight-and-vertex-index
attachment) — is exactly why Mini MBM's own format keeps `SECTION_FRAME_SKINNED` (the skeleton) and
`SECTION_VERTEX_SKIN_WEIGHTS` (the weights) as two independent, optional sections rather than one
combined blob. It also explains a real pitfall this session hit hard — see "Weights are independent
of the skeleton" below.

### Animation curves

FBX stores keyframed animation as `FbxAnimCurve`s — one curve per animated *property channel*
(a bone node's translation X, rotation Y, etc.), using the exact same generic curve mechanism any
other animated property (a camera's field of view, a light's intensity) would use. There's nothing
skeleton-specific about FBX's animation storage at all — a "walk cycle" is just a bundle of curves
that happen to target bone nodes' transform channels.

## What Mini MBM Has Today

> **Delivery decision (2026-08-10):** the sections and Mesh Debug bone API described in this
> “today” section are exploratory implementation, retained temporarily for audit only. The delivered
> skeletal-animation feature uses canonical sections 41–43 exclusively. It will not retain a legacy
> skeletal reader/writer/converter mode; affected `.msh` assets must be regenerated from source FBX.
> Static meshes without skeletal sections are unaffected.

### `SECTION_FRAME_SKINNED` — bind-pose bone hierarchy

One optional bundled section per mesh (docs/mesh-v11-format.md §6e). A flat list of
`SKELETON_BONE_V11` entries, parent-before-child order. Authored via `editor/mesh_debug.lua`'s Bones
node — either hand-built (`meshD:addBone(...)`), captured from a real Blender import
(`editor/blender_mesh_export.py`'s `extract_armature_joints`), or stamped onto a mesh from a saved
**Armature Template** (see below). `MESH_MBM_DEBUG::addBone/getBone/updateBone/removeBone` — the
*editor* mesh class. **`MESH_MBM` — the runtime class every actual game loads meshes through — has
no bone accessors at all.** Ordinary loading parses and discards this legacy editor/interchange
section. The version-6.52.0 foundation provides a private explicit conversion utility, but does not
automatically reinterpret v1/v2 data as a runtime skeleton. There is still no animated-pose or
deformation consumer and no public mutable skeleton surface.

The same private foundation also provides an explicit, non-mutating weight-validation report for
conversion tooling. It resolves the legacy name palette against a deliberately compiled skeleton,
separates structural reference errors from quality findings such as partial coverage or a non-unit
sum, and is not invoked by ordinary `MESH_MBM` loading.

### `SECTION_VERTEX_SKIN_WEIGHTS` — real per-vertex bone weights

One optional bundled section per mesh (docs/mesh-v11-format.md §6g), tied specifically to
`SECTION_FRAME_STATIC` frame 1's own vertex topology (weights are a bind-pose property; they don't
vary per frame, only bone *transforms* would, and this engine has none). Fixed at 4 influences per
vertex (`VERTEX_BONE_WEIGHT_V11`: `paletteIndex[4]` + `weight[4]`), matching the industry-standard
LBS convention described above. Bones are referenced by a small **per-section name palette** — not
by raw index into `SECTION_FRAME_SKINNED`'s own array, and not shared with it at all (see Pitfalls).
Captured on import from a real rig's `vertex_groups` (inline inside
`editor/blender_mesh_export.py`'s `export_frame_subsets`, gated by its own `capture_weights`
parameter — itself set from `--include-bones` — no separate named function); also writable
directly in the editor via `mesh_debug.lua`'s Bones-window **Rigid Bind** tool (`setVertexWeight`,
weight 1.0 to one bone — for a prop that shouldn't deform, e.g. a sword welded to a hand, instead
of leaving `ARMATURE_ENVELOPE`'s distance-based guess to decide). Consumed on export
(`editor/blender_mesh_skeleton_export.py`'s `apply_stored_vertex_weights_override`) as a per-vertex
override pass layered on top of `bind_mesh_to_armature`'s envelope binding, not a mesh-wide
replacement of it — see the Editor Round-Trip Pipeline section below.

### What is explicitly missing for real ("dynamic frame") skeletal animation

None of the following exist anywhere in this codebase:

- **No skinning-palette consumer.** The private Milestone 0 conversion utility can derive inverse
  global bind matrices when invoked explicitly, but ordinary V11 mesh loading does not promote its
  legacy skeleton and no current pose or GPU/CPU deformation path consumes them.
- **No bone matrix palette upload.** No shader input, uniform, or buffer slot analogous to
  `docs/light.md`'s `LightColor[]`/`MaterialDiffuse` reserved names exists for bone matrices, in any
  backend (OpenGL ES, DirectX 9, Metal).
- **No pose evaluation / forward kinematics at runtime.** Nothing walks `SECTION_FRAME_SKINNED`'s
  hierarchy to compute per-bone world transforms for "the current frame" — because there is no
  concept of an animated pose distinct from the bind pose to begin with.
- **No keyframed bone animation clips.** `SECTION_ANIMATION` (the engine's real, working animation
  system) stores/plays sequences of *entire pre-baked static frames*, not per-bone transform curves.
  Playing an "animation" in Mini MBM means swapping which whole `SECTION_FRAME_STATIC` snapshot is
  currently displayed — the same mechanism a 2D sprite sheet uses, just extended to full 3D vertex
  data per frame instead of a 2D UV rect. This is fundamentally different from, and much less
  flexible than, real skeletal animation: there's no blending between clips, no retargeting, and the
  file size scales with (vertex count × frame count) rather than (bone count × keyframe count) —
  frame swapping is dramatically more expensive to store for a long/detailed animation.
- **No GPU or CPU skinning code path**, per the "GPU vs. CPU skinning" section above — stated
  explicitly in this exact wording in both `SECTION_FRAME_SKINNED`'s and
  `SECTION_VERTEX_SKIN_WEIGHTS`'s own format-doc sections, since it's the single most important
  scoping fact about this whole feature area.

## The Editor Round-Trip Pipeline (What Actually Exists)

This is the real, working feature: a faithful **import → inspect/edit → export** loop through
`editor/mesh_debug.lua`, entirely for producing FBX files real DCC tools and Mixamo can consume —
never for anything the engine itself renders differently.

```text
   Blender/Mixamo FBX                 mesh_debug.lua (editor)                  Blender/Mixamo FBX
  (real bind pose +      --import-->  SECTION_FRAME_SKINNED +   --export-->   (real bind pose +
   real vertex weights)                SECTION_VERTEX_SKIN_                    real vertex weights,
                                        WEIGHTS, inspectable/                   re-derived from the
                                        editable in the Bones                   same stored data)
                                        node/window
```

- **Import** (`editor/blender_mesh_export.py`, headless Blender): `extract_armature_joints` walks
  the first `ARMATURE` object's rest-pose bones (BFS from roots, so parent always precedes child);
  `export_frame_subsets`'s weight-capture pass (gated on `--include-bones`) reads each source mesh
  object's own `vertex_groups`, applies `vertex_group_limit_total(4)` +
  `vertex_group_normalize_all` (the same cap/normalize convention the export-side envelope fallback
  already used), and attaches up to 4 `(boneName, weight)` pairs to each captured vertex.
- **Inspect/edit** (`editor/mesh_debug.lua`'s Bones node/window): a plain table of bones
  (name/parent/x/y/z/radius/length/roll + a Highlight checkbox), DragFloat-editable, plus bake
  Rotate/Scale/Translate for the whole skeleton, an **Armature Template** system (see below), and
  Mesh Info's own read-only weight summary (weighted-vertex count, bones referenced, avg/max
  influences). A `length ≤ ~1e-6` bone (e.g. one added via "+ Add Bone"/"+ Add Child Bone", which
  never carry real orientation data) is flagged inline with a warning — `length > EPS` is the same
  "real orientation data available" sentinel `has_orientation()` uses below, so this warning is
  telling the user exactly when that fallback is about to kick in — and a per-bone **Recompute**
  button bakes the same position-topology heuristic the exporter would otherwise silently apply
  (see `compute_tail` just below) into real, further-editable `rotX/Y/Z`/`length` — preserving
  whichever roll this bone already had (see Pitfalls: "Recompute must preserve roll, not reset it")
  — with a single **Roll** field for twisting around that computed axis afterward. A **Rigid Bind**
  section writes real weight 1.0 to one bone for a chosen
  set of vertices (picked either by proximity to that bone's own segment, reusing its `radius`, or
  by an existing material subset) — for a prop that shouldn't deform under `ARMATURE_ENVELOPE`'s
  geometric guess. **Remove Armature** deletes the complete bone hierarchy and all persisted vertex
  weights together, leaving a clean mesh-only T-pose for Mixamo's auto-rigger; it requires an
  explicit confirmation because the operation is destructive until the mesh is reloaded.
  The optional **Sync Left/Right Drag** mode pairs bilateral joints by the naming conventions used
  by the built-in armatures (`Left`/`Right` or `.l`/`.r`). In X/Y drag mode it copies Y and mirrors
  X onto the paired joint; in Z/Y mode it copies both Y and Z so hands/feet move in the same depth
  direction, preserving the paired joint's hidden axis. Center or otherwise unpaired bones are
  moved normally without a second edit.
  Each drag plane has an inline radio selection: **X/Y**, **X**, or **Y** for the front plane and
  **Z/Y**, **Z**, or **Y** for the side plane. The two-axis choice is the default; a single-axis
  choice keeps the other visible coordinate locked even if the camera has been orbited.
- **Export** (`editor/blender_mesh_skeleton_export.py`, headless Blender): `build_armature`
  reconstructs real Blender edit-bones from the stored data (using the stored `rotX/Y/Z`/`length`
  directly when present — `length > EPS` is the "real orientation data available" sentinel — falling
  back to a position-topology heuristic, `compute_tail`, only for bones with none, e.g. hand-authored
  ones with no Blender-import provenance, unless the editor's own Recompute button already baked a
  real value in). Binding then always runs `bind_mesh_to_armature`'s `ARMATURE_ENVELOPE` geometric
  fallback first, for the whole mesh, plus its several targeted cleanup passes (see Pitfalls); if
  ANY vertex carries real/rigid-bound stored weight data, `apply_stored_vertex_weights_override` then
  overrides just those specific vertices' groups from the stored data afterward — a per-vertex
  override layered on top of envelope binding, not a mesh-wide either/or (see Pitfalls: "Weights are
  independent of the skeleton" for why the earlier either/or design silently zeroed the rest of a
  character whenever only a prop bone had real weights).

### Scaling geometry and its skeleton

The skeleton uses the same coordinate space as mesh vertices; its positions are not normalized.
Mesh Debug's Transform node may therefore synchronize a positive uniform whole-mesh scale with the
global skeleton. That bake scales joint positions, radius, and length, while preserving per-bone
`scaleX/Y/Z`: changing coordinate units is not a local bone-scale transform. A frame-only,
subset-only, negative, or non-uniform operation cannot faithfully update the one global rest
skeleton and is not synchronized. The FBX exporter currently reconstructs Blender edit bones from
position, `rotX/Y/Z`, length, and radius; although `scaleX/Y/Z` travel through the intermediate
JSON, they are not consumed when constructing the FBX armature.

### Armature Templates — reusable named skeletons

`editor/mesh_debug.lua`'s `ARMATURE_TEMPLATES` (currently one entry, `ARMATURE_STANDARD_SKELETON_65`
— a real 33-bone Mixamo rig extracted verbatim from a user-rigged reference character) lets a user
stamp a *complete, real* skeleton (full position/rotation/scale/length per bone) onto an arbitrary
target mesh, fit by **uniform scale + reposition only** (`applyArmatureTemplate`) — never per-axis
stretching, which would corrupt the stored `rotX/Y/Z` (a real 3D direction, only meaningful under
uniform scaling). `exportArmatureToFile`/`loadArmatureFromFile` let a user save a hand-fitted
skeleton (after manually nudging bone positions to match a *specific* mesh's own limb proportions,
since a uniform scale-fit alone only gets the reference's proportions, not the target's) and reuse it
on other meshes without a source-code change.

## Pitfalls and Lessons Learned

Real bugs found and fixed while building this feature, kept here so nobody re-discovers them the
hard way. Roughly chronological.

### Envelope binding is a *geometric guess*, and guesses have artifacts

When no real weights exist, Blender's `ARMATURE_ENVELOPE` binding (distance-from-bone-segment) is
used instead of heat-map (`ARMATURE_AUTO`), because heat-map was found to fail **completely** (100%
unweighted vertices, on two different real rigs) whenever `build_mesh`'s single combined mesh
included non-manifold/disconnected islands (teeth, eyelashes, a flat "head mask" overlay) — a very
common shape for a Mixamo download. Envelope binding has no such requirement, but is purely
distance-based, which creates its own artifacts, each with a real, targeted fix:

- **Envelope radius must come from world-space bone length, not `bone.length`.** Blender's
  `EditBone.length` is measured in the armature's own *unscaled rest space* and silently ignores the
  armature object's own scale — a real rig with `armature_obj.scale == 0.01` produced envelope
  radii ~100x too large. Fix: derive radius from `(world_tail - world_head).length`, not
  `bone.length`.
- **A "root motion" bone contaminates nearby real joints.** A top-level, single-child bone that
  exists purely to connect world origin to the first real body joint (Mixamo's own `root` → `hips`
  convention) geometrically overlaps the hip region and gets real envelope weight there, even though
  it's never meant to deform anything — a vertex weighted to it doesn't move the way its
  real-joint-weighted neighbors do, reading as "melting"/detached geometry during animation. Fix:
  detect this specific topology (`_compute_non_deforming_bone_names` — top-level *and* exactly one
  child; deliberately NOT applied down ordinary single-child chains like upperleg→lowerleg→foot,
  which are real sequential joints) and strip/renormalize any weight already assigned to it.
- **Left/right crosstalk near the body midline.** A thin sliver of vertices near the center line
  (inner thigh, groin) can pick up real envelope weight from *both* the left and right leg
  simultaneously, since both are physically close there. Fix
  (`_resolve_left_right_crosstalk`): drop the weaker side's weight and renormalize.
- **A vertex an envelope simply never reaches stays fully unweighted**, which (per the LBS formula
  above) means it doesn't move *at all* under animation — visually far worse than an imperfect
  weight. Fix (`_assign_nearest_bone_to_unweighted`): rigidly assign weight 1.0 to whichever bone
  segment (via `mathutils.geometry.intersect_point_line`) the vertex is geometrically closest to.

### Multi-child bones have no single "obvious" tail direction

A naive `tail = first_child_position` is order-dependent on however the source data happened to list
children, and was confirmed (via direct side-by-side comparison against a real Mixamo file) to
visibly misrepresent joints with several children — e.g. a hand's tail pointing straight at one
specific finger instead of a neutral hand direction. Fix: a multi-child, *non-root* bone continues
the direction the bone *arrived from* at its own length (the same formula already used for genuine
leaf bones); only a multi-child *root* bone (no incoming direction to continue) still uses
first-child direction, separately verified to already match real rigs' own convention there.

### A parent/tail off-by-one silently misrepresents *every* bone at once

An early version of `build_armature` set `child_bone.head = parent's own position` instead of the
child's own position — shifting every bone in the hierarchy by one level versus the source rig's own
`head = self, tail = child` convention. This is *not* the same failure mode as the multi-child issue
above (which only affects branching joints) — it silently corrupts the entire skeleton's proportions
at once, and was the confirmed cause of severe Mixamo retargeting artifacts (arms crossing,
distorted shoulders) on a real re-exported character. Lesson: always verify a from-scratch skeleton
reconstruction against a *known-real* source rig's own head/tail values on a handful of bones
directly, not just "does it look roughly right."

### Embedded/packed textures have a meaningless recorded source path

An FBX with packed/embedded textures (routine for a Mixamo download) still carries a *recorded*
source path in its own metadata — pointing at wherever the original artist's own machine had the
file, e.g. `/home/app/mixamo-mini/tmp/skins_<uuid>.fbm/...`. Trusting that path instead of the
already-loaded pixel data leaves every texture reference pointing nowhere real. Fix: detect a
packed/inaccessible-path image and re-save its actual pixel data to a real file next to the output,
threaded through both the import and export scripts.

### Flat shading defeats vertex deduplication on re-export

`build_mesh` originally never called `shade_smooth()`, so Blender defaulted every polygon to
independent per-face normals — meaning the *next* re-export's own dedup key (which includes the
normal, `editor/blender_mesh_export.py`'s `export_frame_subsets`) found almost zero shared vertices
even though the topology hadn't changed at all: a real 15,882-vertex character round-tripped into
86,640 vertices (`face_count * 3`, i.e. zero sharing), blowing past the 65,535 uint16-index-buffer
limit on the *next* re-import. One `mesh_data.shade_smooth()` call after `from_pydata()` fixed it —
this isn't byte-identical to the source mesh's original normals (this format never stored normals to
begin with, only position/UV), but it restores the vertex-sharing a smooth-shaded mesh is expected to
have.

### Weights are independent of the skeleton — including when they *shouldn't* be

`SECTION_VERTEX_SKIN_WEIGHTS` deliberately references bones by name, in its own separate palette,
specifically so ordinary bone edits (rename, reorder, add/remove a bone or two) never silently
corrupt existing weight data. This is correct for *ordinary* edits — but applying an **Armature
Template** replaces the *entire* skeleton with a different rig's bone names outright. If old weight
data isn't cleared too, every one of its palette names becomes orphaned (referencing bones that no
longer exist anywhere in the mesh's own skeleton at all) — and because export's own
"real weights present?" check only looks at *whether* weight data exists, not whether it's still
valid, it trusted the orphaned data completely and skipped the envelope-binding fallback that would
otherwise have produced *correct* fresh weights. The result, confirmed via direct testing: an
exported FBX whose vertex groups don't match any bone in its own armature — every vertex gets zero
real deform weight (the mesh is invisible) while the skeleton, structurally valid on its own, still
animates fine, which is a very confusing combination to debug from the Mixamo side alone. Fix:
`applyArmatureTemplate` clears existing vertex weights itself whenever it replaces the skeleton.
**Lesson:** any future code path that does a *wholesale* skeleton replacement (not just editing
individual bones) must treat existing weight data as invalidated, even though the format's own
by-name independence is otherwise a deliberate, correct design choice.

### `MESH_MANAGER`'s nickname is a shared cache key, not a per-instance reload guard

Unrelated to skinning specifically, but hit repeatedly while building the Bones node's own 3D gizmo:
`shape:create(vertices, uv, nickName)` caches the underlying mesh resource by `nickName` in
`MESH_MANAGER` — *every* `SHAPE_MESH` instance created with the same nickname shares that **one**
underlying resource, including things that aren't obviously "geometry": `setColor()` in this engine
converts the RGBA into a hex string and routes through the exact same code path as `setTexture()`
(`onSetTextureAnimationLua`), so two shape instances sharing a nickname also silently share
*material/texture* state, not just vertex data — confirmed directly: two spheres given the same
nickname and different colors both read back the identical, *last-set* color via
`getMaterialTexture()`. A fixed/shared nickname is only safe when the underlying vertex data is
*always byte-identical* across every use (e.g. a unit sphere, scaled afterward via `setScale`) —
anything that varies per-instance (including just its *color*) needs a genuinely unique nickname per
instance, not just per distinct geometry.

### `DEVICE::addRenderizable` silently overwrites `position.z == 0.0` on 3D objects

Also hit building the Bones node's gizmo, not skinning-specific, but worth flagging here since it's
exactly the kind of thing that looks like a skinning/skeleton bug from the outside: any 3D object
created with `z` exactly `0.0` gets that `z` silently replaced with an ever-incrementing internal
"z order control" counter (`ORDER_RENDER::getNextZOrderControl3d()`, `+0.01` per call) — a
convenience for objects that don't care about their own depth, not something a deliberately-placed
gizmo wants. A humanoid rig routinely has several bones sitting exactly on the character's own
sagittal (`z=0`) plane (root, hips, spine, chest, head), and rebuilding a gizmo on every interaction
(e.g. every Highlight-checkbox click) meant every rebuild of a `z==0` bone silently drifted further
along `+Z` — this is literally the bug the user remembered as "the Z being overwritten when we
change the bones positions." Fix: nudge any exactly-zero world Z by a visually negligible epsilon
(`0.0001`) before it ever reaches `shape:new`, at the call site — not the shared engine mechanism,
which other, unrelated 2D sorting code intentionally still relies on.

### A precomputed section count silently truncates a whole file

`MESH_MBM_DEBUG::saveV11`'s `fileHeader.sectionCount` was a hand-maintained sum of conditionally-
present sections, computed *before* the new `SECTION_VERTEX_SKIN_WEIGHTS` section existed — adding
the section's own conditional `write` call without also updating this count meant every file saved
with real weights present silently claimed one fewer section than it actually wrote. Both real
loaders (`parse_v11_intermediate`, `MESH_MBM_DEBUG::loadV11`) trust this count exactly — they stop
reading after that many sections, so the *last* section actually written (`SECTION_FRAME_STATIC`,
the mesh's own geometry) was silently never read at all. The failure mode was maximally confusing:
no parse error anywhere, just a downstream shader/animation-setup failure with no obvious connection
to section counts — root-caused only by hex-dumping both a working and a broken file's section
headers side by side and noticing the last one was simply missing. **Lesson:** any new *conditionally
written* section needs its own line in *every* place a section count gets computed, not just its own
`write`/`read` pair — there is no single source of truth for "how many sections does this file have"
in this codebase, it's reconstructed independently in more than one place.

### Both real loaders actually hard-fail on an unrecognized section type

The format's own long-standing doc comment claimed "an unrecognized/never-written type is always a
structural no-op for every reader" — checked directly against the code while adding
`SECTION_VERTEX_SKIN_WEIGHTS` and found **false** for both real content loaders
(`parse_v11_intermediate`, `MESH_MBM_DEBUG::loadV11`) — both have an explicit `else { error }`
branch on an unrecognized `type`. Only the lightweight `MESH_MBM_DEBUG::getInfo` file-probe actually
skips unknown types gracefully. Practical consequence: any new section type needs *explicit*
handling added to both real loaders (even if that handling is "parse into a scratch field and
discard," as `SECTION_VERTEX_SKIN_WEIGHTS` does for the runtime path) before it's safe to write to
disk at all — and an *older*, already-compiled engine binary will still hard-fail on a file carrying
a section type it doesn't know, with no graceful fallback. This is accepted as consistent with how
`SECTION_FRAME_SKINNED` itself was originally rolled out, not something retroactively fixed.

### A tree-node's own collapse can silently disable its cleanup logic

Not a data-format bug, but a real UI lifecycle trap: `showBonesNode`'s gizmo destroy/rebuild logic
only runs while that specific mesh's own top-level tree entry is expanded — and a loaded mesh's own
tree entry only stays expanded while it is the *selected* mesh
(`showMeshTreeWindow`'s `SetNextItemOpen(isSelected, ...)`). The instant a different mesh becomes
selected, the previous mesh's entry collapses and `showBonesNode` simply **stops being called at
all** for it — even though that mesh's own `sOpenNode`/`bBonesWasOpen` state still says "open."
Confirmed directly: switching selection between two loaded meshes while the first one's Bones node
was left open (never explicitly re-clicked closed) left both meshes' 3D bone gizmos visible
simultaneously, since the first mesh's own close-transition code never got a chance to run. **Lesson
(general, not bones-specific):** per-entry cleanup logic that lives *inside* a conditionally-rendered
UI block cannot be trusted to run on every state transition that matters — if "no longer selected"
is one of those transitions, it needs an unconditional per-frame sweep (`sweepStaleBoneGizmos`, run
every frame regardless of which tree entries happen to be expanded) as a second, independent
enforcement point, not just the natural open/close transition inside the collapsible UI itself.

### Rotation isn't self-cancelling; UV inversion is

A smaller one, but easy to get backwards: applying the *same* UV-invert flag on both import and
export cancels out (`1 - (1 - x) == x`), so export's own default invert flags should match import's
defaults exactly. Rotation does **not** work this way — undoing an import rotation requires the
*negated* angle, not the same one applied twice. Getting this backwards for either produces a subtly
wrong (not obviously broken) result: rotation would compound instead of cancel, or UV would flip
twice unnecessarily.

### `radius` and `length` are never preserved across a round-trip — they're re-derived every import

A user round-tripped a character through Mixamo (upload FBX with a hand-added "sword" bone → apply
an animation there → download the animated FBX → re-import into `mesh_debug.lua`) and found the
sword bone's `radius` had grown substantially compared to what they'd originally set. Root cause,
confirmed by reading the import path directly: `editor/blender_mesh_export.py`'s
`extract_armature_joints` (`blender_mesh_export.py:1145`) computes radius fresh on **every** import —
`radius = max(0.001, (world_tail - world_head).length * 0.15)` — always 15% of *that specific file's*
own bone length, never copied forward from any previous mesh_debug session. So "radius changed" is
really just a symptom: `length` changed somewhere in the round-trip (most likely Mixamo doing
*something* internally with the one bone it has no built-in meaning for — "Bone 24" isn't part of
its own 65-bone `mixamorig:` map — though its exact internal behavior is outside this codebase and
wasn't independently verified). **This is not a skinning bug**: neither field ever drives animation
(only keyframed transforms do, see the Quick Mental Model) or the *result* of a Rigid Bind (real
stored per-vertex weights, once written, aren't touched by a `radius`/`length` change on re-import) —
it only matters again if that mesh is re-exported and some of its vertices still need
`ARMATURE_ENVELOPE`'s geometric fallback.

**When a user should actually change `radius`:** only for a bone with no real per-vertex weights
(i.e. still relying on the envelope fallback). Increase it when a bone should own a wide/long span
of geometry and nearby vertices are visibly being claimed by the wrong neighbor, or not deforming at
all (envelope never reached them — see `_assign_nearest_bone_to_unweighted` above). Keep it small for
closely-spaced bones (fingers, a prop tucked into a hand) to avoid the same crosstalk
`_resolve_left_right_crosstalk` exists to clean up elsewhere. For Rigid Bind's own Proximity mode,
radius only sizes the *search* (which vertices count as "the prop") — once Applied, the resulting
weights are exact and radius stops mattering for that bone.

### `tImGui.Checkbox` returns only one value, not `(changed, value)`

Unlike `Combo`/`DragFloat` (which push both a "did it change" bool and the new value),
`onCheckboxImGuiLua` (`plugins/imGui/imgui-lua.cpp:4463`) pushes a single value: the checkbox's
resulting boolean, full stop. Every checkbox already in `mesh_debug.lua` correctly follows the
single-return idiom (`newVal = tImGui.Checkbox(label, oldVal); if newVal ~= oldVal then ... end`) —
one new checkbox this session instead assumed the two-value `(changed, value)` shape, so its second
variable was always `nil`. Concretely: `rb.mode = wantSubsetMode and 'subset' or 'proximity'` with
`wantSubsetMode` always `nil` evaluates to `'proximity'` unconditionally, so the checkbox visually
"snapped back" unchecked every frame no matter what the user clicked — confirmed via direct user
testing ("the checkbox does not hold"). **Lesson:** always check a new `tImGui.Checkbox` call against
an existing one in the same file before assuming its return arity.

### `pushResponsiveItemWidth`'s `min_width` is a floor, not a cap

`tUtil.pushResponsiveItemWidth(min_width, label_reserve)` (`editor/editor_utils.lua:47`, built on
`getResponsiveItemWidth:33`) returns `max(min_width, available_width - reserve)` — so on a wide
window, passing e.g. `200` does **not** cap the widget at 200px, it becomes a widget that fills
nearly the entire remaining width, since `available_width - reserve` is larger. Two real symptoms
this session, same root cause: (1) a combo box built with this helper for a should-stay-modest
bone/subset picker stretched to nearly the full bottom-docked window, crowding a checkbox placed
`SameLine()` right after it off the visible edge — fixed by switching to a plain, fixed
`tImGui.PushItemWidth(140)`; (2) a warning marker (`!`) placed `SameLine()` *after* a
`pushResponsiveItemWidth`-sized `DragFloat` got silently clipped past the table column's own
boundary, since the DragFloat had already consumed the entire column — fixed by drawing the marker
*before* the DragFloat instead, so `GetContentRegionAvail()` (which the helper reads) already
reflects the space the marker consumed by the time the DragFloat sizes itself. **Lesson:** use this
helper only for a field that's *meant* to expand and fill its row/column; for anything that must stay
a fixed, modest size regardless of window width, use `tImGui.PushItemWidth(pixels)` directly.

### A three-way "smallest magnitude wins" axis pick flips on ordinary floating-point noise

The Roll field's "zero roll" reference (`canonicalRollAxis`, `editor/mesh_debug.lua:134`) originally
picked whichever world axis (X/Y/Z) had the smallest `|component|` against the bone's aim direction,
via a three-way `<=` comparison chain. For an aim of exactly `(0,1,0)` this reliably picked world Z
(`aax=0, aaz=0`, tie broken toward Z). But an aim *decoded back* from stored `rotX/Y/Z` for that same
bone came back as `(~1e-7, 1, ~1e-7)` — mathematically identical, but with `aaz` a hair larger than
`aax` due to ordinary trig rounding — which failed the same tie check and fell through to world X
instead, a full 90° away. The result: `currentRollDeg` decoded a value 90° off from what
`eulerFromAimAndRoll` had just encoded, confirmed by a direct round-trip test
(`eulerFromAimAndRoll(0,1,0,179)` then `currentRollDeg` on the result returned `89`, not `179`).
Fix: replaced the three-way magnitude race with a **wide-margin threshold** (world Y as reference
unless `|ay| > 0.9`, in which case world X) — comparing against a fixed, well-separated constant
instead of two near-zero magnitudes against *each other* is immune to this class of noise, since the
real boundary case (an aim actually near-parallel to Y) sits nowhere close to 0.9. **Lesson:** never
pick a discrete code path by comparing two values that can both independently be "computed zero" —
if one is real user data and the other is decoded/reconstructed, floating-point noise can and will
disagree about which is smaller.

### Recompute must preserve roll, not reset it

The per-bone Recompute button originally called `eulerFromAimAndRoll(ax, ay, az, 0)` — hardcoding
roll to the canonical reference on every use. This is fine for a bone that never had real orientation
data to begin with (length was already 0), but a direct user test on an *already-good*, real
Blender-imported bone (the skeleton root, length 0.30, a real authored roll around -90°) showed
Recompute silently discarding that roll down to 0 — real data loss, since Recompute's actual purpose
is fixing a *missing* aim/length, not overwriting a *good* one just because the button was clicked.
Fix: decode the bone's current roll first (`currentRollDeg(b.rotX, b.rotY, b.rotZ)`, relative to its
OLD aim) and reapply that same angle to the newly-computed aim, in both the per-row Recompute and the
Bones-node **Recompute All** batch action — length updates to the real geometric distance to the
bone's child (or continuation from its parent), aim direction updates to match, but roll rides along
unchanged unless the user deliberately adjusts the Roll field afterward. This also made it safe to add
a "recompute even if Length is already set" option to Recompute All, for the "applied a
borrowed/Mixamo armature template, then manually dragged joints to fit this specific mesh" workflow
— `applyArmatureTemplate` only does a uniform scale + reposition (see above), and repositioning a
joint via the X/Y/Z drag fields never updates its Length/rotation on its own, so a template's bones
can end up with a real but geometrically stale Length; forcing Recompute across all of them is no
longer destructive to roll the way it would have been before this fix.

## Audited Handedness Contract: Left/Right Import Fix Not Yet Implemented

Unlike everything in Pitfalls above, the implementation remains **not fixed**. Milestone 0.7 has,
however, resolved the conversion contract and added reproducible FBX evidence so the eventual fix no
longer depends on a visual guess.

**Symptom, confirmed via a controlled user test:** a Mixamo character with an asymmetric prop on the
head (visually on the character's right side, facing the camera, both on the Mixamo website and in a
fresh/untouched Blender import of the same source FBX) shows that same prop on the character's
**left** side once imported through `editor/blender_mesh_export.py` into `mesh_debug.lua`. Camera
angle was explicitly controlled for (character facing the viewer in all three), and two independent,
trusted references (Mixamo's own web viewer and a plain Blender import with no mini-mbm involvement)
agree with each other and disagree with mini-mbm — ruling out "which way is the character facing" as
the explanation and pointing at the import pipeline itself.

**Root cause, confirmed by direct code inspection:**

- Mini MBM's own camera/rendering math is **left-handed** — `src/core_mbm/camera.cpp:193-195` builds
  the engine's view/projection matrices with `MatrixLookAtLH`/`MatrixPerspectiveFovLH` specifically
  (right-handed variants of these same functions exist in `primitives.cpp` but aren't used for the
  camera).
- Blender (and FBX, and therefore anything downloaded from Mixamo) is natively **right-handed**.
- `editor/blender_mesh_export.py`'s vertex export copies `world_pos.x/y/z` straight from Blender with
  no conversion at all (its own comment near `extract_armature_joints`, ~line 1099, states this
  explicitly: "no axis conversion"; the vertex-write code itself, `export_frame_subsets` ~line
  695-697, confirms it). The *only* transform ever applied on import is a genuine **rotation**
  (`rotate_point_deg`, driven by `--angle-x/y/z`), used to reconcile Blender's Z-up convention with
  this engine's own Y-up convention.

**Why a rotation can't fix this:** a rotation is a "proper" transform (determinant +1) — it changes
which physical direction each axis *points*, but it mathematically cannot change whether the
coordinate system as a whole is left- or right-handed (that's an invariant of any pure rotation).
Only an actual reflection (negating exactly one axis) converts one handedness to the other. Since no
such reflection exists anywhere in this pipeline, data authored in Blender's right-handed space is
handed to mini-mbm's left-handed renderer completely unconverted — the up-axis rotation fixes "which
way is up," but the model still renders as a mirror image of its Blender/Mixamo appearance. This is
invisible on roughly-symmetric geometry (a humanoid body looks the same either way) and only becomes
obvious with an asymmetric detail, exactly as the test above found.

**Scope:** this isn't specific to one mesh or one prop — it would affect the left/right orientation of
*everything* ever imported through `editor/blender_mesh_export.py`, silently, for any asset with a
genuinely asymmetric shape or a left/right-specific bone name (`.l`/`.r`) that a viewer might rely on
matching a specific physical side.

**Why this needs care, not a one-line patch, when someone picks it up:** negating a single axis to
correct the handedness also **flips triangle winding order** (every face would start rendering as
back-facing/culled under this engine's own culling convention) unless index order within each
triangle is also swapped to compensate. The fix needs to consistently cover: vertex positions,
vertex normals, bone positions, *and* bone rotations (`rotX/Y/Z` — a Euler triple encodes a
handedness-dependent rotation too, not just a position) — doing only some of these would trade this
bug for a worse, more confusing one (e.g. correct positions but inverted normals, or a correctly
mirrored mesh with a now-incorrect skeleton).

### Accepted conversion contract

The existing default import rotation maps Blender `(x,y,z)` to Mini MBM `(x,z,-y)`: Blender +Z
becomes engine +Y and Blender -Y becomes engine +Z. Preserving those established up and forward
directions leaves X as the reflection axis. The accepted right-handed→left-handed mapping is:

```text
position_engine = (-x_blender, z_blender, -y_blender)
normal_engine   = (-nx_blender, nz_blender, -ny_blender)
```

Because this mapping has determinant `-1`, each triangle must swap one index pair to preserve front
faces under the existing culling convention. UVs are unchanged. Tangents, when runtime support is
added, use the same linear transform and must update their handedness sign.

Let `C` be the column-vector matrix for that mapping. Blender rest/pose/cluster matrices are
converted as `M_engine_column = C * M_blender_column * inverse(C)`, then transposed at the engine
boundary because Mini MBM evaluates row vectors. This applies equally to bind and animated bone
matrices; reflecting only translations or Euler angles is invalid. The importer fix must land as one
atomic positions/normals/winding/bones/animation change with controlled asymmetric fixtures.

### Reproducible M0.7 evidence

Run:

```sh
blender -b --factory-startup --python src/test-lib/skeletal-fbx-bind-audit.py -- \
  src/test-lib/T-BONE-rato-from-mixamo.fbx \
  src/test-lib/T-BONE-rato-from-mixamo-fbx-audit.json
```

The checked report is tied to FBX SHA-256
`d9d99bec7286aaca94700526ce7f78e0fc23acd5101931edf88144ab38af143c` and Blender 5.1.2. It reads
34 raw FBX clusters through Blender's bundled parser before scene import discards `Transform` and
`TransformLink`, then compares selected link matrices with the 41-bone imported rest armature. The
maximum selected comparison error is `7.63e-6`; REST bone/pose comparison error is zero. The mesh
has 36,149 imported vertices and 51,794 triangles, with a positive first-triangle geometric versus
loop-normal dot (`0.986664612`).

The sole action, `Armature|mixamo.com|Layer0` at frames 1–2, has zero selected-bone matrix delta.
Consequently this asset is a bind/weight/topology fixture only. It cannot validate animated FBX
sampling; a genuinely animated source remains required for that later acceptance test.

For work on the current feature branch, `src/test-lib/human-from-mixamo-walking.fbx` supplies that
missing animated source. Its checked audit records a 32-frame Mixamo walk with selected-bone matrix
delta `0.316282972`; frames 1, 16, and 32 are the provisional comparison samples. It has 67 bones,
114 raw clusters across its skinned objects, and maximum selected cluster/rest error `4.5776e-5`.
The source is provisional until provenance/licensing and post-handedness expected matrices are
recorded; do not silently substitute it for the rat's bind/topology baselines.
