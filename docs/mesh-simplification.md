# Mesh Simplification

The Coplanar delivery is complete through 7.289.0. See the
[final acceptance matrix, validation commands and limits](mesh-coplanar-acceptance.md).

Mini MBM provides two polygon-reduction workflows with different asset boundaries:

- Blender importer reduction for high-density static GLB/FBX sources before MSH subset chunking;
- atomic `meshDebug:simplify()` reduction for an existing MSH.

Both workflows retain the engine's `uint16_t` vertex-index contract. Simplification reduces the
existing topology; it is not a remesh operation that rebuilds and reprojects an arbitrary surface.
The QEM target-ratio range in the native MSH editors is 0.1% through 95%;
Coplanar-only does not use a target ratio. Before starting, editors compare
the estimated result vertex count with the 65,535 indexed-mesh limit. The native path repeats the
check after exact attribute joining and fails before edge-collapse passes when the conservative
logical estimate still cannot fit.
Mesh Debug exposes the ratio as a clamped drag field: drag to adjust it continuously or activate
text entry to type an exact value.
In the default strict mode it also derives a hard lower bound from locked open/seam boundaries. If
the boundary vertices alone exceed the indexed vertex limit, or their edges require more triangles
than requested, the job fails during preflight rather than running until no legal collapse remains.

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

- open-boundary vertex locking in the default strict mode;
- rejection of collapses that would pinch coincident attribute-seam edges together, even when their vertex indices differ;
- manifold topology and triangle orientation;
- source/frame geometry bounds;
- the 16-bit index contract;
- material-subset and articulated-Part invariants;
- clearance between nearby disconnected subsets, including sampled deformations.

`Preserve details` is a default-on soft preference rather than a validity rule. It penalizes
collapses near sharp edges and strong normal variation so flatter regions are reduced first.

`Boundary collapse threshold` is an opt-in geometric relaxation measured as a fraction of the
source bounding-box diagonal. Zero retains the strict behavior. A positive value permits only
short, clean open-boundary edges to collapse; boundary-to-interior edges, irregular boundaries,
orientation flips, and non-manifold results remain rejected. This is intended for assets dominated
by small disconnected pieces or duplicated seams that cannot otherwise make progress.

The report includes source/result counts, geometric and sampled-pose errors, achieved relative
error, structural validation counts, committed collapses, protected-candidate counters, detail
penalties, and inter-subset clearance rejections. Mesh Debug classifies relative error as `Good`
through 3%, `Attention` through 10%, and `Risky` above 10%; the numeric value remains available for
model-specific judgment.

These cost-derived diagnostics do not certify a bound over the complete source surface or
preservation of interior attribute interpolation after retriangulation. In particular, the current
Mesh Debug adapter reconstructs UVs and normals from per-subset source contributions; it does not
pass those attribute arrays into the private simplifier or certify a region-wide affine field.

Authored physics/collision geometry is preserved and is not regenerated automatically.

## Mesh Debug workflow

Since 7.290.0, **Simplification** is a separate Mesh Debug work tree, alongside
**Frame**. Frame retains frame/subset editing and Split Capture; method selection
and all simplification inputs, reports, cancel and undo live in Simplification.
The editor polls its detached working mesh
once per frame and displays a progress bar while the simplifier worker runs. It supports frame or checked-subset
scope, an optional virtual frame for selected subsets, compatible shared-frame collapses, rollback,
and Save As. No simplification work runs continuously while the editor is idle.

Since 7.261.0, the progress controls offer **Cancel**; **Esc** cancels the selected
mesh's active simplification. Cancellation discards the detached working mesh,
including any subsets already simplified in that operation, and preserves the
original mesh, its modified state, and the previous undo backup. Remaining subsets
are not processed. Parameters and rollback are disabled during simplification;
the user can apply again after cancellation completes. The worker cancellation is
cooperative, so an indivisible processing step may finish before it stops.

After a successful change, the Simplification tree offers original/result comparison
side by side, independent visibility, wireframe, vertex/triangle counts and fit-to-view.
The immutable result snapshot is paired with the existing pre-operation undo snapshot.
No-op, failed or cancelled operations retain the previous pair. Later geometry/settings
edits do not silently change the pair labelled "last simplification". Undo, a new
successful simplification, removing/clearing meshes or the Quit menu discards it.
Changing previews/selection releases display objects; comparison can be requested again
from the stored pair. Comparison is available for the selected mesh in 3D view.

The view uses static base geometry from the operation's reference frame (frame 1 for
shared-frame jobs), with normals, UVs, textures, material and culling copied. It does
not play skeletal/articulated deformation, frame animation or custom animated shader
effects. The panel explicitly describes that limit. Source/result offsets affect only
preview renderables, never mesh data, saving or exports. Geometry extraction, file
loading and wire generation occur on request; idle synchronization only updates
visibility. See [work tree implementation and validation](mesh-debug-simplification-worktree.md).

Split Capture supports canonical skeletal weights. It maps every rebuilt outside/captured vertex to
its source weight, reconstructs the frame-global weight order, validates the detached mesh, and only
then replaces the editor object. Skeleton hierarchy and animation clips remain available after
save/reload. [Automatic capture](mesh-debug-auto-capture.md) uses the same transaction and creates
one subset per detected island, with configurable connectivity, position tolerance and minimum
face count.

Deferred diagnostics and performance work is tracked in [Future Features](future-features.md).

## Coplanar modes (7.280.0)

Mesh Debug and Image Mesh Editor's general simplification share three modes:

- **QEM** (default): the existing reducer and parameters.
- **Coplanar + QEM**: validate/retriangulate eligible regions first, then run QEM
  only if necessary. The absolute target is computed from the original scope,
  never by applying the ratio again to the intermediate result.
- **Coplanar only**: no ratio target or QEM fallback. Reduce all eligible regions;
  an unchanged result preserves the native buffers and Mesh Debug's previous undo.

Both stages use one detached transaction and final commit gate. Failure in QEM
also discards successful coplanar work. Progress, cancellation, Esc and undo follow
that transaction. Coplanar measurements describe the intermediate surface; QEM's
subsequent approximation does not inherit its attribute-equivalence bound.

With boundary reduction disabled (the default), the implementation retains
**every** boundary segment and source vertex attribute. It handles connected static regions, including concave contours,
within one subset/attribute chart. Since 7.283.0, one outer loop with up to 16
strictly internal, disjoint holes is supported. Uncertified varying normals, non-affine UVs,
ambiguous topology/contacts and excessive work fall back unchanged. Skeletal,
articulated and multi-frame assets skip this stage; QEM remains available. Selected
subsets retain read-only whole-frame neighbor context. Mesh Debug's virtual-subset
batching is available only in QEM mode; the other modes process selected subsets
individually with the complete working mesh still present.

Region growth uses a fixed seed plane, distance at most `planarTolerance * L` (`L` is the
source connected subset domain's logical bounding-box diagonal) and a configurable angular threshold
(default dot `0.99999962`, approximately 0.05 degrees). Retained normals must be finite and nonzero. Constant raw normals retain their
existing behavior. Since 7.285.0, exactly planar generic regions also accept
varying raw normals matching one affine field at every source corner with zero
computed residual. All normals must lie strictly in the seed normal's open
hemisphere, preventing a zero interpolated normal. Approximately planar regions
and curved-specific generation still require identical raw normals. The certificate
preserves the interpolated field used by standard fragment lighting; custom
nonlinear vertex shaders are outside its scope. Numerical roundoff can reject
otherwise affine fields; no normal-error tolerance is introduced. UVs must match one affine field with residual at most `5e-7`
per component at **all** source corners. Twice the maximum residual bounds the
interpolated UV difference. The geometric bound compares both projected fields
to the same plane and includes the dominant-axis projection factor; this avoids
accumulating error while growing a region. No vertex positions are moved.

Since 7.281.0, `planarTolerance` is configurable in Mesh Debug and
Image Mesh general simplification: default `1e-7`, finite range `[0, 0.01]`, zero
for exact coplanarity. For example, `0.0001` allows plane distance up to 0.01% of
`L`. Distance and angle are independent guards; increasing one alone need not improve
reduction. The conservative surface error bound can reach `2 * sqrt(3) *
planarTolerance * L` and is reported separately. Increasing tolerance does not
permit seams, uncertified attributes, invalid holes or uncertain neighbor contacts to be ignored.
The report UI lists holes, attributes, topology, neighbors and work-limit rejection
counts. Small/no-saving candidates are not counted as rejected; zero rejected
regions does not imply that the entire mesh was eligible.

Since 7.282.0, **Coplanar angle** is the primary editor control, a DragFloat in
degrees (`planarAngle`, default 0.05, range 0..5). Both original and replacement
triangles are checked against the fixed seed plane, preventing chained angular
drift. **Advanced coplanar limits** retains the independent distance safeguard,
displayed as a percentage (0..1%, default 0.00001%) with DragFloat. This percentage
is divided by 100 for the stored/API `planarTolerance` fraction. Tooltips wrap at
420 pixels. Existing project settings retain their stored distance and receive the
legacy angular default. The specialized curved pass still accepts exact plateaus.

Projection, oriented edge/vertex links, simple nonintersecting boundary loops,
positive faces and the Euler characteristic with holes are checked before ear
clipping. Visible bridges join inner loops for triangulation without inserting
vertices; bridge endpoint IDs are reused. The candidate must retain the exact
directed boundary multiset (after any certified straight contraction), paired opposite interior edges and connected vertex
fans. Positive projected faces with this oriented boundary preserve domain winding
and coverage, including hole interiors. Uncertain segment contacts are rejected.
All boundary samples survive unless optional coordinated straight reduction certifies them. The pass rejects uncertain triangulation and
uses a conservative whole-frame obstacle test: separated bounding boxes/slabs are
safe; certain exact-plane boundary contacts are also accepted. Since 7.286.0,
exactly coplanar obstacles with overlapping projected bounds may be certified
strictly disjoint from every replacement triangle using both triangles' edge
half-planes. This allows separate islands inside holes or exterior concave notches.
The area epsilon protects uncertain separation; touching, degenerate and overlapping
pairs fail this new branch. Since 7.288.0, the same strict full-projection
certificate also accepts inclined obstacles, including pieces crossing the seed
plane entirely inside a hole or exterior notch. Disjoint projected domains imply
disjoint 3D geometry. This projected test alone does not certify degenerate or
overlapping projections; the previous noncoplanar boundary-contact checks remain.
This is not a general triangle-plane intersection certificate.
Since 7.289.0, a noncoplanar obstacle that fails full-projection separation may
instead certify a strict 3D gap from every replacement triangle. The private
predicate tries both face-normal and edge-cross-edge directions; outward-rounded
projection intervals prevent roundoff-only gaps. Its clearance guard is the greater
of the plane-distance tolerance and `1e-12 * L`, scaled conservatively by the axis
L1 norm. Axis tests share the existing work budget. Degenerate/uncertain pairs do
not gain an exemption, and existing boundary contacts remain eligible through the
previous fallback. This is a sufficient separation certificate, not a complete
intersection solver. Builds declaring fast floating-point math disable this new
certificate because its interval bounds require strict arithmetic. No new user
parameter is introduced, and QEM retains its existing behavior.
Approximately planar regions retain the previous
conservative obstacle policy.

Since 7.287.0, a private balanced AABB index over original selected and unselected
triangles replaces the per-region whole-frame scan. Queries use inclusive bounds
on the two projection axes and deliberately ignore the dropped axis; they cannot
remove candidates required by the previous projected-box check. Candidates retain
source/context order before the unchanged narrow certificates. Node/leaf visits
share the region work budget. Index construction and queries check cancellation;
standard-library partition/sort calls finish before the next checkpoint. Index
storage is linear in source/context triangle count, request-local, and rebuilt for
the optional coordinated-boundary rerun. No idle scans or persistent cache are
introduced. Highly overlapping projections may gain little and exhaust the budget;
large or uncertain neighborhoods can still reject valid regions.
Work is capped at 2,048 total boundary vertices, 16 holes and 2,000,000 budgeted
boundary/bridge/ear/certificate/obstacle operations per region, with cancellation
checkpoints. An eligible region with B retained boundary vertices and H holes
reaches B + 2H - 2 faces, using the final retained B. Touching/nested
loops, uncertain bridges and exhausted budgets preserve the original region.
`planarHoles` counts loop/bridge validation rejections, not successfully preserved
holes; work-limit and topology rejections retain their own counters. The certificate concerns stored geometry,
UVs and normals, not arbitrary procedural vertex effects or shader-specific output.

Reports separate planar accepted/rejected counts, removed faces, rejection categories,
geometry/UV bounds, `planarSkipped`, `qemRan` and `unchanged` from QEM diagnostics.
An 8x8 grid retains its 32 boundary segments and reduces from 128 to 30 faces by default;
with eligible straight-boundary reduction enabled it reaches two.
See the [design and validation record](mesh-coplanar-optimization-plan.md).

### Coordinated straight boundaries (7.284.0)

**Reduce straight boundaries** (`planarReduceBoundaries`, default false) is shared
by Mesh Debug and Image Mesh general simplification. It only removes exactly
collinear 3D samples from exactly planar certified charts. It never moves vertices,
merges attribute charts or changes the union of their boundary segments. All source
corners still participate in the original affine attribute certificate.

Decisions are coordinated by exact position, including aliases across UV/material
seams. Every incident source face must belong to a certified boundary region, with
the same two geometric neighbors on every side; at most two regions may meet.
Corners, locks, unselected surrounding vertices, uncertified regions and conflicting
segmentation protect the sample. Some generated side quads are separate charts;
their corners remain protected even when their geometry looks planar. Existing
planar faces with no interior reduction can participate in coordination, but regions
of fewer than three source triangles remain protected in this milestone.

The pass first completes the original interior-only candidate, then builds and
revalidates one coordinated candidate **from the original input**. It verifies all
dependent regions and keeps the interior-only candidate if any dependency or budget
fails. No partially coordinated borders are published. Limits are 4,096 certified
regions and 32,768 boundary entries, in addition to existing per-region budgets.
Cancellation discards the entire simplification; QEM still runs only after this
stage and retains the original absolute target. The option does not change QEM-only.

Reports add `planarBoundaryRemovedVertices` (geometric positions, counted once
across aliases) and `planarBoundaryFallback` (the optional stage retained the
completed interior-only result). Mesh Debug's selected-subset jobs retain unselected
neighbor samples; use the whole-frame scope to coordinate across subsets together.
General settings persist in Image Mesh projects, with false for older projects.
No discovery runs in idle frames. Approximate contour changes, varying-normal
certificates and general curved-wall remeshing remain outside this feature.

Image Mesh's specific curved-relief path offers **Curved relief**, **Coplanar +
curved relief**, and **Coplanar only** independently of QEM. Its prepass accepts
only exact horizontal plateaus compatible with the generator's constant-normal
policy, keeping controls and extrema locked. The initial height error remains zero;
subsequent specific simplification retains its original target and accumulated
error budget. Faceted/interior workflows keep their existing simplification
restrictions. The completed minimal-back generation optimization is unchanged.
