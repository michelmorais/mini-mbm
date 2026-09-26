# Mesh Simplification

Mesh Debug and Image Mesh Editor offer native **QEM** reduction and optional
**Coplanar (CGAL external)** reduction, plus sequential **CGAL + QEM**. Blender import decimation and the Image
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
In either editor, use **Options > CGAL executable** and choose the folder containing
`mbm-cgal-planar`, `mbm-cgal-remesh`, `mbm-cgal-repair` and `mbm-cgal-audit`
(with `.exe` on Windows). A table shows each file found in green; missing tools
remain unavailable without preventing use of the others. This checks file presence,
not execution permissions or binary compatibility. **Refresh** rescans after an
installation change. There are no filesystem scans during idle drawing.
The shared folder preference is `.mini-mbm-cgal-folder` inside `APPDATA` (Windows),
otherwise `HOME`; `MBM_CGAL_FOLDER_CONFIG` overrides that file for tests.
It takes priority over legacy individual paths. Without a saved folder, legacy
paths remain usable and the panel initially suggests a folder from them.
Select that folder to persist it. Restart another open editor to reload changes.
Legacy `MBM_CGAL_CONFIG` still overrides the individual planar preference file.

Enable the **CGAL** checkbox. Angle is 0..60 degrees (default 10); distance
is 0..10% of the exported input diagonal (default 5%). These control CGAL
region/corner detection. UV tolerance is fixed at `1e-6`. Ratios, Preserve Details
and QEM boundary-collapse thresholds do not apply to this backend.

The editor exports temporary OBJ/MTL, invokes the configured process without a
shell, and imports only on exit 0 with a valid report. Cancellation, a five-minute
timeout, nonzero exit, invalid output or vertex-budget overflow leaves the visible
mesh unchanged. Files are cleaned after work/scene shutdown. CGAL provides no
numerical progress estimate; the editor keeps its cancel control available.

Both CGAL workers require an oriented manifold. The editor preserves mesh vertex
indices in OBJ and passes `--preserve-topology` to repair, planar and Remesh.
Coincident positions are not merged: explicit splits, including UV/normal seams
and saved repairs, remain separate. Non-manifold connections or inconsistent
face orientation can reject input before
reduction starts; changing the triangle target cannot resolve that rejection.
The editor identifies this topology error separately from executable failures
and directs the user to the mesh audit. The editor enables topology repair by default for CGAL and Remesh;
the CLI workers require the explicit flag described below.

Supported input is a single-frame triangle mesh without skeletal data or
articulated parts. UV seams, subset order/IDs, diffuse and normal/specular/emissive/
mask textures are retained. Geometry is replaced in the working asset, retaining
asset-level material, animation effects, winding/culling and physics. Existing
normals are reconstructed as face normals for planar reduction; Remesh uses
connected smooth fans as described below. Authored normals are not transferred.
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

General simplification offers independent CGAL and QEM checkboxes. Settings persist with project/default/
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
To include isotropic Remesh, also set `MBM_CGAL_REMESH_EXECUTABLE` to
`mbm-cgal-remesh`; the optional branch verifies a completed remesh and retained
textures/material roles and physics shapes, plus apply/revert/cancel and triangle-count increases.
`image_mesh_cgal_editor_smoke.lua` additionally uses `MBM_CGAL_PROJECT` for the
measured authored fixture, checking preview/export and idle cache reuse. With
`MBM_CGAL_REMESH_EXECUTABLE`, it also verifies the post-generation Remesh stage, rejection of new self-intersections,
and a successful retry with all source edges constrained.
Standalone geometry/UV/protocol tests run in the mbm-cgal repository.

## Combined CGAL + QEM

Both editors offer **CGAL + QEM** (`cgal_qem`). The external CGAL pass runs
first. If its result exceeds the target triangle count, native QEM processes
that result. The ratio refers to the original selected geometry, not the
intermediate CGAL mesh; QEM is skipped when CGAL already meets the target.
Native topology and boundary constraints can prevent reaching the target.
Angle and distance control CGAL; ratio, preserve-details and boundary controls
apply to QEM. Selected subsets retain independent targets.

Both stages use a disposable working mesh, with one final application and one
undo operation. Cancellation or failure discards the working result. CGAL UV
preservation applies to its intermediate output: QEM may further change UVs
and geometry. CGAL sampled error describes the first stage; native QEM quality
metrics describe changes from that intermediate mesh, not total pipeline error.
No extra processing runs while the editor is idle.

The GUI groups the CGAL checkbox and its angle/distance parameters first, then
a separator and the QEM checkbox with its ratio/details/boundary parameters.
Unchecked methods retain their settings in disabled controls. Both checked
select the combined pipeline; neither checked disables simplification (and
disables Apply in Mesh Debug). Image Mesh projects retain the existing mode
representation, with `none` for neither method, and `simplify=false` when off.

The combined pipeline retains CGAL's shared position/UV indices during QEM.
Face normals are reconstructed only after both geometric passes finish (also
when QEM is skipped). Splitting vertices for face normals before QEM would
create artificial open boundaries and could reject otherwise feasible targets.
UV seams remain separate; the configured boundary threshold is not overridden.
The final reconstruction rechecks the vertex budget before publishing the mesh.

## Isotropic Remesh

`mbm-cgal-remesh` is a separate optional offline worker, not a simplification
backend. Mesh Debug and Image Mesh expose it as **Remesh** with a target edge
length, iteration count and feature angle. It may increase triangle count and
does not target an exact polygon budget. The worker accepts static triangulated
OBJ, constrains UV/material charts, open borders and detected sharp edges, then
reports sampled deviation and topology checks. UVs are transferred from the
closest point in the corresponding source chart; shading normals are
reconstructed and authored physics shapes are copied to the remeshed asset.
For each subset, the importer averages area-weighted normals through consistently
oriented edges shared by exactly two faces within the feature-angle threshold.
It shares normals only within connected fans of existing vertex indices; it does
not weld positions or bridge UV/material seams, boundaries or non-manifold edges.
Sharper edges retain separate normals. This replaces the previous flat-normal
expansion, which could exceed 65,535 vertices even when the worker result fit.
Inputs without normals remain without them; the final vertex-budget check remains
mandatory. The shading can therefore differ from the previous faceted result.
Skeletal weights and animation are not transferred by this OBJ contract, and
such inputs are rejected. Remesh does not perform semantic/quad retopology. The
editor applies the result as one reversible operation only after worker and
geometry-budget checks succeed.

New self-intersections, changes in component count or closedness reject the result
with an explicit diagnostic. Isotropic remeshing does not guarantee an
intersection-free result for arbitrary settings on thin or detailed surfaces.
Lower the target edge length or feature angle when needed; a feature angle of
`0` constrains all source edges and limits smoothing. Vertex limits still apply.

## Read-only Mesh Audit

The **Analyze mesh / Analisar malha** panel reports a stored-frame-1 snapshot
without changing geometry. Configure `mbm-cgal-audit` in the CGAL options;
analysis is explicit and supports cancellation and JSON export. See
[Mesh Audit](mesh-audit.md) for the public Lua API and diagnostic limitations.

### Remesh controls

Mesh Debug and Image Mesh use target edge length directly (fraction of the
processed geometry bounding-box diagonal, range 0.002..0.25, default 0.03).
The triangle-count checkbox, count field, presets, automatic conversion and
per-subset triangle-budget allocation have been removed. Iterations, feature
angle and optional topology repair remain available. Results report achieved
counts without a triangle-count goal or tolerance warning.

Old Image Mesh projects with `remeshTargetEnabled` and `remeshTargetTriangles`
remain readable, but those fields are ignored by editor processing. The saved
edge length is used, or its default if absent. No geometry scan or conversion is
performed while loading or drawing the controls.

For compatibility with direct integrations, the low-level
`mesh_cgal.startRemesh` ninth argument and external worker `--target-triangles`
remain supported; neither editor nor `mesh_simplify_pipeline` enables them.

### Topology preparation for CGAL and Remesh

**Repair topology before processing / Reparar topologia antes de processar**
appears first in the operation settings and defaults to on for CGAL and Remesh.
Explicit saved false values remain false. It invokes the configured
`mbm-cgal-repair` first, then passes its temporary OBJ to the selected worker
with `--preserve-topology`. The same repair executable powers **Repair now**.
The repair worker
orients the triangle soup and duplicates vertices at non-manifold connections,
preserving all nondegenerate source triangles, positions, corner UVs and materials
during this preparation. It may open seams or split connected components. It does
not fill holes, remove degenerate faces, or resolve self-intersections; degenerate
faces still fail. Remeshing can subsequently move vertices and change triangles.

The result report includes the number of split vertices and reversed faces.
Topology comparisons (components, closedness, self-intersections) use the repaired
input as the baseline. Existing source self-intersections can remain; a successful
result is not a certificate of intersection-free geometry. Imported OBJ vertex
identities remain distinct even when position and UV match, including during
normal reconstruction, subsequent exports and MSH save/load. Repeating repair
on unchanged repaired geometry must report zero new splits and reversed faces.

`mesh_cgal.startRemesh` accepts an optional tenth boolean `repairTopology`;
`mesh_simplify_pipeline.start` accepts `repairTopology` in Remesh settings.
Image Mesh saves `remeshRepairTopology` and `cgalRepairTopology` with
project/default/region options. The pipeline passes `repairTopology` to both
CGAL and Remesh. The low-level `mesh_cgal.start` accepts an eleventh argument
`repairTopology` after its internal Remesh settings slot; absent flags retain
the strict worker behavior for direct API callers.
The original asset is committed only after successful processing; cancellation,
failure and revert retain their existing behavior. A pure planar CGAL result
that does not reduce triangles is reported unchanged and is not imported (including
its preparation splits); this avoids reconstructing normals for discarded output.
CGAL + QEM still imports its intermediate topology for the QEM stage.

`src/test-lib/mesh_cgal_repair_editor_smoke.lua` checks the repair-disabled
failure, repair-enabled import, material retention, backup restore, cancellation,
shortcut state changes and preservation of explicit OBJ vertex splits. Set
`MBM_CGAL_REMESH_EXECUTABLE` and `MBM_CGAL_REPAIR_EXECUTABLE` to the workers.
Real-mesh save/load coverage lives in the separate roundtrip smoke described below.

### Remesh defaults and iteration limits

New editor settings use 10 iterations (range 1..50), feature angle 14.5 degrees,
repair enabled and edge-length fraction 0.03. Saved iteration, angle and length
settings remain intact. The feature angle uses DragFloat at 0.1 degree per drag
step with two decimal places; Ctrl+click permits exact numeric entry. Smaller
feature angles constrain more edges. More iterations can improve regularity but
also increase smoothing and time; they do not guarantee a smaller triangle count
or better preservation of details. Worker CLI defaults for iterations and feature
angle match the editor; repair remains an explicit CLI flag.

### Standalone topology repair

The separate external `mbm-cgal-repair` executable performs only the topology
preparation, using the same routine as both workers:

```sh
mbm-cgal-repair source.obj repaired.obj repair-report.txt
mbm-cgal-planar repaired.obj reduced.obj 10 .05 .000001 planar-report.txt --preserve-topology
```

The CLI consumes attributed triangular OBJ, not MSH, and does not transfer authored
normals. Positions, triangle count, corner UVs and materials survive repair.
`--preserve-topology` tells the downstream reader to respect separate OBJ vertex
indices instead of welding them back together; it is available on planar and
Remesh after all positional arguments. Strict mesh validation still applies.
The repair executable also accepts this flag when processing an already repaired
OBJ. The editor always uses this flag, including for standalone repair, and
exports distinct source vertex indices. Raw CLI calls still weld by default. There is no new standalone repair tree in the GUI.

The 31,091-triangle `i2-v01-c6097637-msh01.msh` sample has 24,491 indexed
render vertices. Preserving those indices requires zero splits and zero face
reversals. Earlier results reporting 22 splits first welded the mesh to 15,553
positions; that preparation recreated the non-manifold connections on each run.
Those historical counts and reduction measurements describe a different input
connectivity. Preserving render seams can constrain reduction/remeshing further.
Audit still analyzes exact-position-welded geometry and can therefore report
connections that are absent from the indexed surface used for processing.

### Repair executable configuration and immediate repair

**Options > CGAL executable** discovers repair in the shared folder and lists it
alongside the other tools. `mesh_cgal.getRepairPath()` resolves that selection.
The legacy `setRepairPath(path,persist)` API and `.mini-mbm-cgal-repair-path`
preference (overridden by `MBM_CGAL_REPAIR_CONFIG`) remain supported when no
shared folder is configured.

In Mesh Debug, **Repair now / Reparar agora** sits below the repair checkbox;
a separator ends the repair group. It works independently of the checkbox and
selected reduction method, using the current frame/subset scope. It requires a
static, single-frame triangle mesh and no queued edits. It does not simplify or
remesh. It uses the existing asynchronous progress, cancel, comparison, backup
and revert machinery; failure leaves the visible asset untouched. A repair
report with zero split vertices and zero reversed faces is a no-op: the editor
keeps the original mesh buffers, modified flag, undo backup and comparison. It
explicitly reports that no changes were applied and no save is required for that
operation; previously pending edits still require saving. Changed repairs report
their topology changes. Comparison headings identify the operation that produced
the stored result (repair, remesh or simplification), even after a later no-op.

`mesh_cgal.startRepair(asset,subset,frame,maxVertices,hasNormals)` exposes the
same worker. Its `repair` result contains split-vertex and reversed-face counts.
The OBJ repair worker preserves face order and exact positions. The Lua importer
uses that correspondence to retain source corner UVs and authored normals, flipping
normal signs for reversed faces and preserving explicit topology splits. This
avoids rebuilding a separate flat normal at every corner of a smooth source.
The CLI itself still does not write normals. Material roles and physics use the
same import path as CGAL/Remesh. The Image Mesh editor's checkbox uses the same
configured preparation worker; the manual repair button belongs to Mesh Debug.

### Repair regression checks

The engine repair smoke verifies immediate repair, no-op handling, source corner
attributes, undo and comparison identification.
`src/test-lib/mesh_cgal_repair_roundtrip_smoke.lua` verifies a genuinely
non-manifold fixture, MSH save/load with identical vertex/index data, zero
changes on repeat repair, and planar processing without another repair.
Set `MBM_CGAL_REPAIR_EXECUTABLE` and `MBM_CGAL_EXECUTABLE` to the workers;
optional `MBM_CGAL_REPAIR_MESH` adds a real mesh (the source is never overwritten).


`src/test-lib/mesh_cgal_normals_smoke.lua` checks smooth/sharp edges, disconnected
seams, non-manifold edges, corner attributes and unit normals with bundled Lua.
`src/test-lib/mesh_cgal_remesh_import_smoke.lua` runs the real engine using
`MBM_CGAL_REPAIR_MESH`, `MBM_CGAL_REPAIR_EXECUTABLE` and
`MBM_CGAL_REMESH_EXECUTABLE`, checks the vertex limit and normals, then saves and
reloads a temporary result without changing the source. The 31,091-triangle
sample, at 20,000 target / 10 iterations / 14.5 feature angle, imports as 33,251
render vertices and 22,781 triangles. The target remains unmet (13.905% over);
fixing normal reconstruction does not alter the CGAL triangle result.


Mesh3DGen uses the same folder table via
`mesh_cgal.panel(gui, translate, notifications)`. Arguments are optional for
mini-mbm's own editors; external clients can supply their own ImGui/language and
notification functions without changing global editor state. Its local Remesh
uses edge length, 10 iterations and 14.5 feature angle, plus the separate repair
worker by default. Immediate repair creates a separate `repaired` variant only
when topology changes; no-op runs create no variant. Its remote provider Remesh
controls are independent.
