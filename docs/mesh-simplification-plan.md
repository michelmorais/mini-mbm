# Mesh Simplification Plan

## Purpose

Add polygon reduction without changing mini-mbm's 16-bit vertex-index contract. The work has two
related, but separate, entry points:

1. Reduce large static GLB/FBX assets in Blender before the importer creates MSH subsets.
2. Provide one `MESH_MBM_DEBUG::simplify()` API for an existing MSH, with internal paths that
   preserve static, skeletal, and animated data as applicable.

`remesh` is reserved for a future operation that rebuilds topology and reprojects attributes.
The planned operation is decimation/simplification derived from the existing topology.

## Invariants

- Engine and editor index buffers remain `uint16_t`; this work does not add 32-bit runtime indices.
- Material boundaries remain semantic subset boundaries.
- Technical chunks created only to satisfy the 65,535-vertex limit are produced after reduction.
- A failed or unsupported simplification must not partially mutate or overwrite the input asset.
- Canonical weights remain frame-zero, frame-global, one record per exported vertex, with at most
  four finite nonnegative influences normalized to one.
- Skeleton identities, hierarchy, clips, and animation tracks are not silently removed.
- UV seams, hard normals, material boundaries, and open boundaries must have explicit preservation
  policies rather than incidental behavior.
- Results must be deterministic for the same input and options.

## Stage 1: Blender Pre-import Reduction

The Blender importer gets an opt-in "Reduce polygons before import" option. Reduction runs after
Blender imports the source and before `export_frame_subsets()` triangulates, separates materials,
deduplicates loop attributes, and calls `split_subset_for_uint16_indices()`.

Initial scope:

- static visible mesh objects only;
- Blender Decimate modifier in Collapse mode;
- ratio-based input, with the UI showing the approximate retained percentage;
- no armatures, skin weights, shape keys, mesh-cache animation, animated mesh objects, or baked
  geometry clips;
- reject unsupported scenes with an actionable error instead of ignoring the option;
- do not alter the source GLB/FBX on disk.

The imported Blender scene is a disposable conversion workspace, so the exporter may add temporary
modifiers there. The exporter still reads only evaluated meshes and writes the MSH after successful
validation.

Later Blender-importer extensions may add a global target-face budget, per-object allocation,
symmetry, and carefully validated skeletal decimation. They are not part of Stage 1.

## Stage 2: `MESH_MBM_DEBUG::simplify()`

Status: static indexed simplification is implemented in MBM_VERSION 7.178.0. Canonical skeletal
weights and animation preservation are implemented in MBM_VERSION 7.179.0. Animation-aware
pose-sampled quality is implemented in MBM_VERSION 7.180.0. Articulated animation and multi-frame
geometry remain later slices.

The public surface is one method. Asset inspection selects private implementation paths; callers do
not choose separate static/skinned/frame-animation methods.

The implementation should:

1. Build a detached candidate topology.
2. Simplify indexed triangles with an explicit error metric and preservation options.
3. Produce an old-to-new vertex mapping.
4. Rebuild positions, indices, normals, UVs, and subsets while preserving authored physics metadata.
5. If canonical weights exist, merge influences, retain the strongest four, and normalize them.
6. If geometry animation has multiple frames, apply one compatible collapse sequence to every
   frame or reject the operation.
7. Validate the complete candidate with the existing MSH and skeletal validators.
8. Commit atomically only after every validation succeeds.

Pose sampling uses a deterministic budget of at most 24 poses distributed across at most 24 clips.
Collapse ranking combines bind-pose quadric error with the maximum difference between the sampled
animated displacement of the endpoints. The report exposes sampling coverage and separate bind-pose
and pose-space errors; assets without clips continue to report bind-pose-only quality.

The unconstrained QEM optimum is projected onto its source edge segment before a collapse. This
prevents an otherwise valid quadratic solution from creating a distant spike outside the source
geometry. A final finite-coordinate and source-bounds check is also applied to the complete detached
candidate before commit; the bounds check is a defensive validator, not the primary correction.
Each collapse must also satisfy the manifold link condition. Candidates within one batch may not
touch the same triangle, so orientation checks performed separately cannot interfere when the batch
is committed. If these topology and orientation constraints prevent reaching an aggressive target,
the operation fails atomically instead of creating non-manifold edges or apparent holes.
The detached result is scanned once more before output compaction, and any edge referenced by more
than two triangles rejects the operation as a defensive invariant check.

Whole-frame simplification uses one virtual topology across all material subsets. Vertices with
bit-identical positions may be paired across different subsets so a material seam is treated as an
interior topological edge, but coincident vertices inside the same subset remain separate to retain
authored UV and hard-normal seams. Every triangle carries its original subset identifier through
the collapse sequence. The detached result is then expanded back into contiguous per-subset vertex
and index ranges, with normals, UVs, and canonical weights blended in the target subset's attribute
domain. No material assignment is merged, and the final frame still must fit the existing 16-bit
vertex-index contract. The reduction target is calculated once for the complete frame rather than
rounded independently per subset; removing every triangle from any subset rejects the operation
atomically.

## Mesh Debug Editor Integration

The editor entry is implemented under the existing Frame node because that area already owns
structural frame/subset operations. MBM_VERSION 7.181.0 introduced the explicit `Entire mesh`
scope, target ratio, source-to-target triangle counts, skeletal quality notice, simplification
report, existing `Save As` flow, and rollback. MBM_VERSION 7.184.0 adds an explicit one-subset scope
for one-frame meshes. It does not reuse the visibility/removal checkboxes as an implicit target:
the simplification panel names the selected subset directly. Unselected subsets retain identical
rendered vertex attributes, triangle order, textures, and canonical weights; frame-global buffers
and weights are remapped and validated before the atomic commit. A later backend/UI slice may add
multi-frame targeting once one compatible collapse sequence can be applied across every frame.

Authored collision geometry is intentionally not regenerated by simplification. A future explicit
"recalculate visual bounds" operation may replace only auto-generated bounds, but simplification
must not guess whether existing `INFO_PHYSICS` entries are disposable.

## Test Assets

- `/home/michel/Downloads/rp_sophia_animated_003_idling_FBX/rp_sophia_animated_003_idling.msh`
  validates canonical skeleton, weights, and idle animation preservation.
- `/home/michel/Downloads/From-mixamo-Walking.msh` validates a second skeletal source and weight
  distribution.
- `/home/michel/Downloads/Meshy_AI_Grumblechain_0828120116_texture.glb` is the high-density static
  Blender-import stress test and remains GLB so reduction occurs before MSH chunking.

## Acceptance Criteria

### Blender Stage 1

- The option is disabled by default and does not change existing imports.
- A ratio strictly greater than zero and at most one reaches Blender unchanged.
- Reduction happens before material bucketing and `uint16_t` chunking.
- Static GLB/FBX output has fewer triangles at ratios below one and remains loadable as MSH.
- Material textures and extra texture roles survive the operation.
- Unsupported skeletal or animated inputs fail before output replacement with a clear reason.
- Cancellation, timeout, debug logging, and atomic output behavior continue to work.

### MSH Simplification

- Triangle reduction reaches the requested target within a documented tolerance.
- All indices, subset ranges, weights, animation references, and physics bounds validate.
- Skeletal test assets load and animate after simplification with no invalid weights.
- The original object remains unchanged on any failure.
- Idle editor frames perform no simplification or other expensive repeated work.
