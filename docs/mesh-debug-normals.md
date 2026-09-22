# Normal Processing in Mesh Debug

Mesh Debug offers three normal-processing methods. The normal table defaults to
repair and applies the selected method to an individual vertex or the whole subset
in frame 1. The **Apply to all** window has an independent method selection for
batch operations across frames and subsets.

## Methods

| Method | Behavior |
|---|---|
| Repair invalid normals | Preserves valid authored vectors, normalizes recoverable lengths, and replaces invalid directions when a geometric reference is available |
| Recompute - preserve surfaces | Infers connected surface regions and prioritizes dominant regions at shared vertices |
| Recompute - uniform smoothing | Replaces normals with the normalized average of adjacent unit face normals |

### Repair invalid normals

A vector is preserved exactly when it is finite, nonzero, within 0.001 of unit
length, and points in a direction compatible with the geometric average (positive
dot product). Compatible vectors with incorrect lengths are normalized without
changing their direction. Zero, nonfinite, or reversed vectors are replaced with
the geometric average when a usable reference exists.

Without a geometric reference, finite nonzero vectors can still have their length
repaired; unrecoverable vectors remain unchanged. Degenerate triangles do not
provide useful directions. Orientation is derived from winding and does not prove
that a vector points outward from an arbitrary solid; the calculation does not
flip its result based on the declared CW/CCW mode.

Repair does not recreate the plateau classification used by the Image Mesh Editor.
If a file has already been recomputed with different but valid normals, repair
cannot automatically recover its previous shading.

### Recompute while preserving surfaces

Faces connected by shared edges belong to the same region when neighboring face
normals differ by no more than the chosen angle. The default is 25 degrees; the
range is 1–89 degrees.

At a vertex shared by different regions, the region with the largest total area
receives priority. The result averages unit normals of incident faces belonging
to that region. Regions with at least 95% of the largest region's area contribute
together, avoiding arbitrary preferences at symmetric corners.

This is a geometric inference. It uses neither existing normals, a height map,
nor a preferred world axis. Region growth follows local continuity, so a curved
surface can form one region even if its endpoints have very different orientations.
Large angles may merge a wall with a plateau; small angles may fragment curved
surfaces. Dominant area is a heuristic, not a semantic top/bottom classification.

Regions do not join across edges shared by more than two faces. Without shared
indices, including non-indexed triangle lists, connectivity is not inferred from
coincident positions. The operation does not weld vertices or UVs, split seams,
or add vertices. Positions, UVs, indices, and counts remain unchanged. Corners
without a dominant region receive blended normals.

Use the preview under several light directions before confirming. This method
can recover useful surface shading but cannot guarantee reconstruction of artistic
normals or the output of a specialized generator.

### Uniform smoothing

Uniform smoothing averages adjacent unit face normals and normalizes the result.
It replaces authored normals and can change the appearance of meshes from the
Image Mesh Editor, whose plateau normals receive specialized treatment.

Both indexed and non-indexed triangle lists are supported. Vertices are not
merged by position.

## Reversible preview

All three methods prepare isolated copies of the current mesh, including unsaved
edits. The review window shows the method, the number of normals to change, and
the proposed meshes. For a batch, select the target from the list. **Show original**
switches between the authored mesh and its proposal; camera and lighting remain
available. Other editing controls are unavailable during review.

- **Confirm** accepts all listed proposals after checking that the targets are
  still the same. Entries are marked modified, without saving implicitly.
- **Cancel** or closing the window discards proposals, preserving the original
  mesh, previous edits, and modified state.
- **Confirm and save** is required for the combined processing-and-saving command.

Temporary files are removed after confirmation, cancellation, preparation failure,
or scene exit. A preparation failure discards all proposals. Ordinary processing
reports when no changes are needed; the combined save workflow still offers a
review when the number of changed normals is zero.

The preview uses isolated editor loading so temporary files do not reuse a mesh
from the shared render cache. Frame filtering is supported. Copies and computation
are created on request; switching targets or original/proposal invalidates the
preview, without continuously recalculating normals or rebuilding meshes while idle.

## Batch operations and saving

**Apply to all** processes every frame and subset of the selected asset type.
Preserved/unchanged normals do not mark an entry modified. Ordinary processing
skips entries without normals. **Add normals** creates them only where missing,
avoiding accidental replacement of existing vectors.

**Save all (selected normal method)** prepares copies for review. It creates
missing normals and applies the chosen method; Repair uses uniform smoothing when
there are no authored vectors to preserve. Assets not using TRIANGLES are skipped.
Only **Confirm and save** writes files, with the engine's implicit normal
recomputation disabled. Ordinary saving preserves the current vectors.

Save failures are reported per item and do not stop the remaining batch. A failed
save retains the accepted in-memory changes so saving can be retried. Only targets
included in the review are saved.

Method selection is temporary session state and does not itself modify geometry.
Both method selectors have a fixed width. Angle changes do not run surface grouping;
grouping runs when the processing operation is requested. Geometric status and
visualization use a separate cache. Manual component editing, inversion, addition,
and removal of normals retain their own workflows.

## Implementation and verification

Surface grouping uses face/edge data and disjoint sets for connected components.
It does not run while drawing the method selector. The algorithms and preview
helpers live in the editor's normal-processing modules.

- `src/test-lib/mesh_debug_normals_test.lua` covers repair, normalization,
  NaN/infinity, orientation, and uniform replacement.
- `src/test-lib/mesh_debug_normals_smoke.lua` covers multiple frames/subsets,
  authored normals, idempotence, save/reload, non-indexed geometry, batch failures,
  review isolation, cancellation, temporary-file cleanup, and idle behavior.

See [Image Mesh Editor](image-mesh-editor.md#geometry-normals-and-budgets) for
relief-specific normals and [Mesh Simplification](mesh-simplification.md) for
operations that change geometry.
