# Mesh Simplification

Mini MBM provides QEM reduction and conservative coplanar retriangulation for
existing 3D triangle-list meshes. Mesh Debug and Image Mesh Editor share the
generic simplifier. Blender import decimation and the Image Mesh curved-relief
simplifier are separate workflows.

## API and modes

The native API is `MESH_MBM_DEBUG::simplify()` with `MESH_SIMPLIFY_MODE` and
`MESH_SIMPLIFY_REPORT`, declared in
[`mesh-manager.h`](../include/core_mbm/mesh-manager.h). The Lua API is:

```lua
local report, err = meshD:simplify(ratio, subset, frame, preserveDetails,
    boundaryCollapseThreshold, mode, planarTolerance, planarAngle,
    planarReduceBoundaries)
```

Arguments after `ratio` are optional. See
[Lua API - Triangle simplification](lua-api.md#triangle-simplification) for the
complete argument, report and error reference.

| Mode | Lua value | Behavior |
|---|---|---|
| QEM (default) | `"qem"` | Quadric-error edge collapses with topology, attribute-seam and deformation protections |
| Coplanar + QEM | `"coplanar_qem"` | Retriangulate eligible regions, then run QEM only if needed to reach the target derived from the original triangle count |
| Coplanar only | `"coplanar"` | Reduce eligible regions without a target ratio or QEM fallback; `ratio` may be `nil` |

```lua
-- Coplanar only, whole first frame, default tolerances, retain boundary samples.
local report, err = meshD:simplify(nil, nil, 1, true, 0, "coplanar")

-- Coplanar before QEM, targeting half the original faces, with straight boundaries.
local report, err = meshD:simplify(0.5, nil, 1, true, 0,
    "coplanar_qem", 1e-7, 0.05, true)
```

`subset` is a one-based subset index; `nil` selects the complete frame.
`frame` defaults to 1; zero requests a shared QEM collapse sequence across
compatible geometry frames. QEM ratios must be finite and strictly between zero
and one; editor controls restrict them to 0.1% through 95%.

The input may be indexed or nonindexed. A changed result is indexed and must fit
the engine's 65,535-vertex limit. Subsets retain their order and material assignment;
articulated Parts remain separate topology domains. Authored physics/collision
geometry is preserved, not regenerated.

### Transactions and asynchronous work

Both stages build a detached candidate and share a final commit gate. A combined
failure discards both stages. A coplanar-only no-op leaves native buffers intact;
a region that cannot be certified retains its original triangulation.

For large meshes, use an instance-owned worker on a detached working mesh:

```lua
local started, err = workingMesh:startSimplify(0.5, nil, 1, true, 0,
    "coplanar_qem", 1e-7, 0.05, false)
-- Poll from the editor loop while work is active:
local status = workingMesh:getSimplifyStatus()
-- On user cancellation:
local accepted = workingMesh:cancelSimplify()
```

Status includes `state` (`idle`, `running`, `completed`, `failed`, `cancelled`)
and `progress` in 0..1; completion includes `report`, failure/cancellation includes
`error`. Do not read, save or mutate the working instance while it is running.
Publish it only after completion. Accepted native cancellation precedes buffer
replacement and preserves the source; keep polling until terminal status.
Cancellation is cooperative, so sorting/allocation or another indivisible step
may finish before the next checkpoint. Destruction requests cancellation and joins
the worker. C++ exposes `startSimplify`, `getSimplifyState`, `getSimplifyResult`
and `cancelSimplify`; worker state remains private to the instance.

## Coplanar modes

The coplanar stage accepts connected static regions within a subset/attribute
chart, including inclined planes, concave contours and one outer loop with up to
16 strictly internal, disjoint holes. It skips assets with canonical skeletal data,
articulated Parts or multiple geometry frames; coplanarity in one pose does not
certify equivalence during deformation. QEM remains available for those assets.

Every boundary segment and retained corner attribute is preserved by default.
The optional straight-boundary operation below can remove certified collinear
samples. Regions are not assumed to become two triangles: with `B` retained
boundary vertices and `H` holes, an accepted triangulation has `B + 2H - 2` faces.
For example, an 8x8 grid has 128 faces and 32 boundary vertices: it reduces to 30
faces with boundaries retained, or two if straight-boundary removal is eligible.
No reduction does not imply that the input is globally minimal.

### Tolerances and attributes

| Setting | Default | Range and meaning |
|---|---:|---|
| `planarTolerance` | `1e-7` | Finite 0..0.01; distance to a fixed seed plane as a fraction of the original connected subset domain's bounding-box diagonal `L`. Zero requires exact coplanarity. |
| `planarAngle` | `0.05` | Finite 0..5 degrees; source and replacement face normals must satisfy the angle against the fixed seed plane. |
| `planarReduceBoundaries` | `false` | Enable coordinated removal of exact straight-boundary samples. |

Distance and angle apply together; increasing one alone need not increase
reduction. The seed plane and domain scale remain fixed while grouping faces,
preventing accumulated pairwise drift. No vertex positions move. The conservative
surface error bound can reach `2 * sqrt(3) * planarTolerance * L`; the report
provides the bound for accepted regions, separately from the input distance limit.

Preserving only boundary attributes is insufficient. The stage checks **all source
corners** against their projected interpolation fields:

- UVs must match one affine field with residual at most `5e-7` per component.
  Twice the maximum residual bounds the interpolated UV difference.
- Present raw normals must be finite and nonzero. Exactly planar generic regions
  accept constant normals or one affine raw-normal field with zero computed
  residual. All normals must lie in the reference seed corner normal's open
  hemisphere, preventing a zero interpolated normal.
- Approximately planar regions and the specialized curved generator require
  identical raw normals. Normal seams and non-affine fields are not relaxed by
  the geometric tolerance; roundoff may reject an otherwise affine field.

These certificates concern stored geometry, UVs, raw normals and their
interpolation. Arbitrary nonlinear procedural vertex/shader effects are outside
the contract. A subsequent QEM stage does not inherit coplanar attribute or
surface-error bounds.

### Topology, neighbors and limits

The candidate must preserve orientation, boundary coverage, paired interior edges
and connected vertex fans. Hole bridges reuse existing vertices. Ambiguous,
touching or nested loops, uncertain triangulations, degeneracy and invalid contacts
retain the original region. Material subsets and attribute charts are not merged.
Selected-subset processing keeps read-only whole-frame neighbor context to avoid
new cracks, T-junctions and intersections with surrounding surfaces.

Obstacle checks accept certified separation or supported existing boundary
contacts. Exactly planar regions can also certify strict projected separation
from coplanar or inclined obstacles, or a strict 3D gap from noncoplanar obstacles.
The 3D test uses outward-rounded intervals and a clearance guard based on
`max(planarTolerance * L, 1e-12 * L)`. It is a sufficient separation certificate,
not a complete intersection solver. Approximately planar regions use more
conservative obstacle checks. Uncertain contacts remain unchanged.

The request-local obstacle index reduces neighborhood searches without dropping
inclusive projected candidates. Heavily overlapping neighborhoods may still
exhaust the budget. No spatial discovery runs in idle editor frames.

| Limit | Value |
|---|---:|
| Total outer/inner boundary vertices per region | 2,048 |
| Internal holes per region | 16 |
| Budgeted boundary/bridge/triangulation/certificate/obstacle operations per region | 2,000,000 |
| Certified regions for boundary coordination | 4,096 |
| Boundary entries for coordination | 32,768 |

Exhausted limits cause conservative fallback. Floating-point precision can change
acceptance and face counts; bit-identical cross-platform output is not promised.
The interval-based 3D separation certificate requires strict arithmetic and
disables itself in builds declaring fast floating-point math. This does not make
the complete simplifier safe under arbitrary fast-math flags.

### Coordinated straight boundaries

`planarReduceBoundaries` removes only exactly collinear 3D samples from exactly
planar certified charts. It neither moves vertices nor changes the union of
boundary segments. All original corners still participate in attribute checks.

Removal decisions are coordinated by exact position, including aliases across
UV/material seams. Every incident face must belong to a certified region with the
same two geometric neighbors on each side; at most two regions may meet. Corners,
locks, unselected neighbors, uncertified regions and conflicting segmentation
protect the sample. Regions with fewer than three source triangles remain
protected. Separate side quads can therefore prevent removal even when they look
planar.

The operation first builds the interior-only result, then constructs and validates
a coordinated candidate from the original input. If any dependency or coordination
budget fails, it retains the completed interior-only result. No partially coordinated
boundary is published. Use whole-frame scope to coordinate across subsets;
selected-subset jobs preserve unselected neighbor samples. The option has no effect
in QEM-only mode or the specialized curved-relief boundary policy.

### Reports

| Fields | Meaning |
|---|---|
| `sourceVertexCount`, `resultVertexCount`, `sourceTriangleCount`, `resultTriangleCount` | Source and final counts |
| `unchanged`, `qemRan`, `planarSkipped` | No geometry change, QEM execution, or coplanar exclusion by asset policy |
| `planarRegions`, `planarRejectedRegions`, `planarRemovedTriangles` | Accepted/rejected regions and removed faces |
| `planarHoles`, `planarAttributes`, `planarTopology`, `planarSurroundings`, `planarWorkLimit` | Rejection categories; `planarHoles` counts loop/bridge failures, not preserved holes |
| `planarMaximumError`, `planarMaximumUvError` | Coplanar-stage geometry and UV bounds |
| `planarBoundaryRemovedVertices`, `planarBoundaryFallback` | Removed geometric samples counted once across aliases; fallback to the interior-only result |

Small or non-reducing candidates are not counted as rejected regions. Zero
rejections does not mean the entire mesh was eligible. QEM diagnostics remain
separate from coplanar measurements.

## QEM safety and quality

QEM ranks edge-projected quadric-error collapses. Positions, deformation samples,
source contributions and skin weights use the same interpolation factor. The
Mesh Debug adapter reconstructs per-subset UVs and normals from source
contributions. Canonical weights retain at most four normalized nonnegative
influences. QEM supports static, skeletal, selected/shared geometry-frame and
articulated meshes with their corresponding protections.

Hard protections cover manifold topology, orientation, source/frame bounds,
material/Part domains, the index limit, coincident attribute-seam pinching and
clearance between disconnected subsets, including sampled deformations.
Open-boundary vertices are locked by default. Conservative indexed-size and
locked-boundary lower bounds can reject an impossible target before collapses.
Other protections may also prevent reaching the requested target; failure is atomic.

`preserveDetails` defaults to true and adds a soft penalty near sharp edges and
strong normal variation. `boundaryCollapseThreshold` defaults to zero. A positive
value relaxes only short, clean open-boundary edges, measured relative to the
source bounding-box diagonal; irregular boundaries, boundary-to-interior edges,
flips and non-manifold results remain rejected.

Reports include counts, collapses, geometric/sampled-pose and relative errors,
structural validation and protected-candidate counters. Mesh Debug labels relative
error Good through 3%, Attention through 10%, and Risky above 10%. These cost-derived
QEM diagnostics do not certify a bound over the complete surface or region-wide
attribute interpolation; the coplanar stage performs its own independent checks.

## Mesh Debug workflow

**Simplification** is a separate work tree alongside **Frame**. It contains method,
frame/checked-subset scope, QEM/coplanar inputs, progress, reports, cancellation
and undo. Virtual-subset batching is available only for QEM; coplanar modes process
selected subsets individually while retaining the complete working mesh as context.
The angle uses DragFloat in degrees. Plane distance appears directly below it as
0..1% of the diagonal (default 0.00001%), converted to the API fraction by dividing
by 100. Tooltips wrap their text.

Processing starts only on request. The editor polls active work and publishes a
complete detached mesh. Cancel or Esc discards the working copy, including subsets
already processed in the batch, and preserves the original, modified state and
previous undo backup. Parameters and rollback are disabled during processing.

### Comparison and wireframe

The Simplification tree offers a side-by-side comparison of the last successful
operation, independent original/result visibility, wireframe, counts and fit-to-view.
An immutable result snapshot is paired with the pre-operation undo snapshot.
No-op, failed or cancelled operations preserve the previous pair; later edits do
not silently change it. Undo, a new successful operation, removal, Clear All or
the Quit menu discards it. Changing selection/preview releases display objects;
comparison can be requested again from the stored pair.

Comparison is available for the selected mesh in 3D. It shows static base geometry
from the operation's reference frame (frame 1 for shared-frame jobs), retaining
normals, UVs, textures, material and culling. It does not play skeletal/articulated
deformation, frame animation or custom animated shader effects. Offsets affect
only display objects, never saved/exported geometry. Geometry loading and wire
construction happen on request; idle synchronization updates visibility only.

**Mesh Info** also offers an independent Wireframe option for all subsets of
frame 1 of a 3D triangle-list mesh, indexed or nonindexed. It replaces the filled
preview with static base-geometry edges while Info is active. Leaving Info restores
the filled preview; returning reuses cached lines. Edits, preview reload, selection
changes, removal and Clear All release the cache.

## Image Mesh Editor

General simplification uses the same QEM, Coplanar + QEM and Coplanar-only modes.
Settings persist with project/default/region options and apply to preview,
comparison and export. The angle uses degrees; the independent distance percentage
is under **Advanced coplanar limits**. Straight-boundary reduction defaults to false.

The separate specific-relief workflow offers **Curved relief**, **Coplanar + curved
relief** and **Coplanar only**. Its coplanar stage accepts exact horizontal plateaus
compatible with constant normals, keeping controls, extrema and authored boundaries
locked. Its initial height error is zero; subsequent curved simplification retains
the original target and accumulated height-error budget. General coplanar distance
and straight-boundary options do not relax this policy. Faceted/interior generation
retains its simplification restrictions. Eligible flat-back optimization is part
of generation and does not require invoking generic simplification.

See [Image Mesh Editor](image-mesh-editor.md) for generation, project and export controls.

## Blender importer

**Reduce polygons before import** uses Blender Collapse decimation on supported
static visible mesh objects before material bucketing and 65,535-vertex chunking.
It never modifies the source GLB/FBX. Armatures, skin weights, shape keys, mesh-cache
animation and animated mesh objects are rejected by this importer option. Imported
meshes can subsequently use the generic simplifier within its asset policy.
