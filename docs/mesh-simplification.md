# Mesh Simplification

Mini MBM provides two polygon-reduction workflows with different asset boundaries:

- Blender importer reduction for high-density static GLB/FBX sources before MSH subset chunking;
- atomic `meshDebug:simplify()` reduction for an existing MSH.

Both workflows retain the engine's `uint16_t` vertex-index contract. Simplification reduces the
existing topology; it is not a remesh operation that rebuilds and reprojects an arbitrary surface.

## Blender importer

`Reduce polygons before import` runs Blender's Collapse decimation on supported static visible mesh
objects before material bucketing and 65,535-vertex chunking. It never modifies the source GLB/FBX.
Armatures, skin weights, shape keys, mesh-cache animation, and animated mesh objects are rejected by
this importer option instead of being reduced incorrectly.

## MSH API

The Lua signatures, arguments, return fields, and failure behavior are defined in
[Lua API - Triangle simplification](lua-api.md#triangle-simplification). The native entry point is
`MESH_MBM_DEBUG::simplify()`. Editor-facing asynchronous work uses instance-owned
`startSimplify()`, `getSimplifyState()`, and `getSimplifyResult()` methods. Its thread and result
live in `MESH_MBM_DEBUG::Impl`; the runtime engine loop has no Mesh Debug pump or dependency.

The operation accepts indexed or non-indexed 3D triangle lists. It builds a detached candidate,
validates the complete result, and replaces the source only on success. A successful non-indexed
input becomes an indexed MSH while retaining the 16-bit index limit.

The same API internally selects the applicable path for:

- static geometry;
- canonical skeletons, weights, and animation clips;
- one selected non-skeletal geometry frame;
- one shared collapse sequence across compatible geometry frames;
- articulated Parts and their animation metadata.

Whole-frame reduction operates across material subsets while preserving their order and material
assignment. Articulated Parts remain separate topology domains because they can animate
independently. Subset-only reduction is also available; Mesh Debug coordinates multi-subset batches
in Lua and commits them atomically.

## Safety and quality

The simplifier uses edge-projected quadric-error collapse ranking and applies the same interpolation
factor to positions, normals, UVs, deformation samples, source contributions, and skin weights.
Canonical weights retain at most four normalized nonnegative influences.

Hard protections reject collapses that would violate:

- open-boundary vertex locking;
- manifold topology and triangle orientation;
- source/frame geometry bounds;
- the 16-bit index contract;
- material-subset and articulated-Part invariants;
- clearance between nearby disconnected subsets, including sampled deformations.

`Preserve details` is a default-on soft preference rather than a validity rule. It penalizes
collapses near sharp edges and strong normal variation so flatter regions are reduced first.

The report includes source/result counts, geometric and sampled-pose errors, achieved relative
error, structural validation counts, committed collapses, protected-candidate counters, detail
penalties, and inter-subset clearance rejections. Mesh Debug classifies relative error as `Good`
through 3%, `Attention` through 10%, and `Risky` above 10%; the numeric value remains available for
model-specific judgment.

Authored physics/collision geometry is preserved and is not regenerated automatically.

## Mesh Debug workflow

`Simplify Geometry` appears after Split Start Capture. The editor polls its detached working mesh
once per frame and displays a progress bar while the simplifier worker runs. It supports frame or checked-subset
scope, an optional virtual frame for selected subsets, compatible shared-frame collapses, rollback,
and Save As. No simplification work runs continuously while the editor is idle.

Split Capture supports canonical skeletal weights. It maps every rebuilt outside/captured vertex to
its source weight, reconstructs the frame-global weight order, validates the detached mesh, and only
then replaces the editor object. Skeleton hierarchy and animation clips remain available after
save/reload.

Deferred diagnostics and performance work is tracked in [Future Features](future-features.md).
