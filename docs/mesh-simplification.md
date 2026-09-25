# Mesh Simplification

Mesh Debug and Image Mesh Editor offer native **QEM** reduction and optional
**Coplanar (CGAL external)** reduction. Blender import decimation and the Image
Mesh curved-relief simplifier are separate workflows.

## Native QEM API

`MESH_MBM_DEBUG::simplify()` and `MESH_SIMPLIFY_REPORT` are declared in
[`mesh-manager.h`](../include/core_mbm/mesh-manager.h). Lua:

```lua
local report, err = meshD:simplify(ratio, subset, frame, preserveDetails,
    boundaryCollapseThreshold)
```

Arguments after `ratio` are optional. `subset` is one-based; nil selects the
complete frame. `frame` defaults to 1; zero requests a shared collapse sequence
across compatible geometry frames. Ratios must be finite and strictly between
zero and one; editor controls restrict them to 0.1% through 95%.
See [Lua API](lua-api.md#triangle-simplification) for the complete contract.

Input may be indexed or nonindexed. The indexed result must fit the engine's
65,535-vertex frame limit. Subsets retain order and materials; articulated Parts
remain separate topology domains. Authored physics is preserved, not regenerated.

### Transactions and asynchronous work

Simplification builds a detached candidate and commits only after validation.
Failures or accepted cancellation leave the original geometry intact.

```lua
local started, err = workingMesh:startSimplify(0.5, nil, 1, true, 0)
local status = workingMesh:getSimplifyStatus()
local accepted = workingMesh:cancelSimplify()
```

Poll active work from the editor loop. Status includes `state` (`idle`, `running`,
`completed`, `failed`, `cancelled`) and progress in 0..1; completion includes
`report`, failure includes `error`. Do not read/save/mutate the working instance
while running. Publish it only after completion. Cancellation is cooperative;
sorting/allocation can finish before the next checkpoint. Destruction requests
cancellation and joins the worker. C++ exposes `startSimplify`,
`getSimplifyState`, `getSimplifyResult`, `cancelSimplify`; worker state is private.

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
attribute interpolation.

## External CGAL backend

The executable, build files, standalone tests and licenses live in
[mbm-cgal](https://github.com/michelmorais/mbm-cgal), independently of the engine.
In either editor, use **Options > CGAL executable**, choose the executable and
click **Save path**. The shared preference is plain text in `.mini-mbm-cgal-path`
inside `APPDATA` (Windows), otherwise `HOME`. It is read once per editor session;
restart an already open second editor after changing it elsewhere.
`MBM_CGAL_CONFIG` overrides the preference file for isolated tests.

Select **Coplanar (CGAL external)**. Angle is 0..60 degrees (default 0.05); distance
is 0..10% of the exported input diagonal (default 0.00001%). These control CGAL
region/corner detection. UV tolerance is fixed at `1e-6`. Ratios, Preserve Details
and QEM boundary-collapse thresholds do not apply to this backend.

The editor exports temporary OBJ/MTL, invokes the configured process without a
shell, and imports only on exit 0 with a valid report. Cancellation, a five-minute
timeout, nonzero exit, invalid output or vertex-budget overflow leaves the visible
mesh unchanged. Files are cleaned after work/scene shutdown. CGAL provides no
numerical progress estimate; the editor keeps its cancel control available.

Supported input is a single-frame triangle mesh without skeletal data or
articulated parts. UV seams, subset order/IDs, diffuse and normal/specular/emissive/
mask textures are retained. Geometry is replaced in the working asset, retaining
asset-level material, animation effects, winding/culling and physics. Existing
normals are reconstructed as face normals; authored smooth shading is not retained.
Meshes without normals remain without them. Results must fit 65,535 frame vertices
and Image Mesh's configured vertex budget. Sampled geometry error is not a
certified surface bound. CGAL is not linked into the engine or selected through
the native `meshDebug:simplify` API.

## Mesh Debug workflow

**Simplification** is a separate work tree alongside **Frame**, with method,
frame/checked-subset scope, applicable backend controls, progress, reports,
cancellation and revert. Virtual-subset batching is QEM-only. CGAL selected subsets
are exported and processed individually; whole-frame scope exports all subsets.
Widely separated islands increase CGAL's distance scale; select individual subsets
or lower the distance tolerance when needed.

Processing starts only on request. The editor polls active work and publishes a
complete detached mesh. Cancel or Esc discards the working copy, including subsets
already processed in a batch, preserving the original and previous undo backup.
Parameters and rollback are disabled during processing.

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

General simplification offers QEM and CGAL. Settings persist with project/default/
region options and apply to preview, comparison, statistics and export. The external
executable path is a machine preference, not part of the `.imesh` project.

The separate **Simplify curved relief** workflow removes interior vertices under
its original dense-source height-error budget, retaining controls, extrema and
authored boundaries. Faceted/interior generation keeps its simplification
restrictions. Eligible flat-back optimization is part of generation and does not
require invoking general simplification.

See [Image Mesh Editor](image-mesh-editor.md) for generation and export controls.

## Blender importer

**Reduce polygons before import** uses Blender Collapse decimation on supported
static visible mesh objects before material bucketing and 65,535-vertex chunking.
It never modifies the source GLB/FBX. Armatures, skin weights, shape keys, mesh-cache
animation and animated mesh objects are rejected by this importer option. Imported
meshes can subsequently use the generic simplifier within its asset policy.

## Integration tests

`mesh_cgal_editor_smoke.lua` covers shared configuration, UV/material transfer,
subset isolation, reduction, revert, cancellation, invalid executables, process
errors, vertex budgets and unsupported input. Set `MBM_CGAL_EXECUTABLE` to the
independently built executable and `MBM_CGAL_CONFIG` to a temporary preference path.
`image_mesh_cgal_editor_smoke.lua` additionally uses `MBM_CGAL_PROJECT` for the
measured authored fixture, checking preview/export and idle cache reuse.
Standalone geometry/UV/protocol tests run in the mbm-cgal repository.
