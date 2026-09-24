# Coplanar mesh optimization — investigation and first milestone

Status: **proposed, not implemented**. Investigation against `547b569f`, 2026-09-24.
This records the design before implementation. It does not change the existing
`simplify()` contract or announce a shipped feature.

## Findings in the current implementation

| Area | Existing behavior and implication |
|---|---|
| [Private simplifier](../src/core_mbm/private/mesh-simplifier.cpp), `simplify()` | Edge-projected quadric-error collapses, deterministic cost tie-breaking, boundary locking, link checks including position aliases, orientation checks, detail penalties and local inter-subset clearance checks. No connected planar-region extraction or contour retriangulation. A planar quadric does not choose a minimum planar triangulation. |
| [Private input/output](../src/core_mbm/private/mesh-simplifier.h) | Positions, optional normals/UVs, triangle groups, deformation samples and source contributions. No attribute-equivalence certificate or region error budget. |
| [Mesh Debug adapter](../src/core_mbm/mesh-manager.cpp), `MESH_MBM_DEBUG::simplify()` | Builds logical positions and groups, but **does not populate `input.normals` or `input.uvs`**. Within a non-indexed subset it joins exact source attributes; across subsets it may join positions in the same topology domain despite different attributes. Normals/UVs/weights are reconstructed per subset from `sourceContributions`. A new validator cannot treat the private input's absent attributes as proof that the source has none. |
| Same adapter, reconstruction | Normalizes reconstructed normals, including single-source contributions. A preservation-oriented operation must copy retained source attributes directly instead of inheriting normalization or averaging through this path. |
| Same adapter, animation | Selected geometry frames, shared frame collapses, canonical weights and up to 24 sampled animation poses are supported by the existing reducer. Samples are not a proof for all possible skeletal poses or interpolated animation times. |
| Same adapter, preflight/publication | Checks draw mode, index/subset ranges, logical vertex estimate, locked boundary floor, physical `uint16_t` limits and output bounds. Allocates detached results before an atomic cancellation/commit gate. These mechanisms are reusable; the ratio-derived estimates are specific to collapse reduction. |
| [Mesh Debug editor](../editor/mesh_debug.lua), `simplifyApplyCoroutine()` | Saves a pending backup, operates on a detached mesh, publishes the complete result and then replaces undo. Supports frame/subset/virtual-frame scope. The virtual-frame helper removes other geometry; it cannot provide the full neighbor context needed for a new planar safety check. |
| Same editor, `onLoop()` / `simplifyResume()` | Resumes active jobs; an absent coroutine returns immediately. Region discovery must be started by an explicit action, never by drawing the panel or polling progress. |
| [Image Mesh generation](../src/core_mbm/image-mesh.cpp) | Already selects eligible minimal backs and rebuilds adjoining side strips. This relies on generated contour/height/UV knowledge unavailable for arbitrary imported meshes. Preserve this completed workflow. |
| [Polygon helper](../src/core_mbm/private/image-mesh-topology.cpp), `triangulatePolygon()` / `minimalBack()` | Existing ear clipping and hole-bridging internals are relevant prior art. The public private-helper entry accepts a single polygon of Image Mesh points and uses generator-specific tolerances. It is not yet a generic, certified planar-region triangulator. |
| [Curved relief simplifier](../src/core_mbm/private/image-mesh-curved-simplify.h) | Already compares piecewise-linear height fields at triangle-overlap vertices and carries error bounds across local replacements. Reuse that validation principle where appropriate; do not route generic meshes through this height-field operation. |

The current QEM report is not a Hausdorff bound or an attribute-interpolation
certificate. Its maximum geometric error comes from committed quadric costs;
orientation checks and edge incidence checks alone do not establish absence of
all triangle intersections. The new operation needs its own validation, rather
than interpreting the current quality labels as a proof.

## Milestone 1: conservative static-region retriangulation

Deliver an explicit **Optimize planar regions** operation in Mesh Debug's
simplification panel, independent of the target-ratio slider. Initially support:

- Static, single-frame 3D triangle-list meshes, indexed or non-indexed.
- Complete-frame or selected-subset scope, always retaining read-only access to
  the **whole original frame** for junction and intersection validation.
- Connected, consistently oriented, manifold disk regions with a simple outer
  loop; convex and concave contours are eligible.
- One material/subset and one continuous attribute chart per region.
- Removal of interior vertices only; retain every boundary vertex and directed
  boundary segment, even collinear ones. Do not move retained vertices.
- UVs certified against one affine field over the region; constant, finite,
  nonzero stored normals for the first delivery, or consistently absent normals.
  Preserve raw retained normal values, not just their normalized directions.

Regions with holes are detected and left unchanged in M1; holes must never be
filled. Likewise preserve regions with non-affine UVs, varying normals, ambiguous
connectivity, invalid projections, insufficient reduction or exhausted work
budgets. This deliberately limits reduction rather than weakening preservation.
Disconnected eligible regions may still be optimized in the same transaction.

With `B` retained boundary vertices, an eligible disk can reach `B - 2` triangles.
An 8-by-8-cell grid has 128 source triangles and 32 boundary segments: M1's target
is 30 triangles, **not two**. A rectangle with only its four corners on the
boundary and many interior samples can reach two. Minimality is conditional on
the retained constraints, not a guarantee for an entire arbitrary mesh.

Do not merge subsets, renumber their semantic identities, delete a subset, weld
UV/normal seams, remove authored collision data or change material/texture state.
Unknown vertex-deformation effects or attributes that cannot be validated make
the affected asset ineligible. Geometry/UV/normal equivalence does not prove
equivalence for arbitrary vertex shaders, per-vertex procedural effects, or
wireframe rendering. The supported rendering contract must be documented and
checked at the adapter/editor boundary before exposing the operation as safe.

### Animation policy

M1 rejects skeletal data (including weights without clips), multiple geometry
frames, articulated Parts/animation, and known vertex-deformation effects before
mutation. Rejecting only nonempty `deformationDeltas` is insufficient: a weighted
mesh with no sampled clip still deforms. Do not optimize the current preview pose
and silently bake it. Existing QEM simplification remains independently available
with its existing behavior.

Rigid per-Part optimization, shared-frame certificates and deformable surfaces
are later milestones. Even affine weights alone do not certify LBS equivalence:
blending varying weights with varying positions introduces products; DQS needs
its own argument. A finite pose sample set is regression evidence, not proof.

## Region discovery and proof obligations

1. **Snapshot and validate.** Preserve provenance at triangle corners: original
   frame/subset/vertex plus raw position, UV and normal. Distinguish index
   adjacency from exact-position geometric adjacency. Never weld by distance to
   manufacture connectivity. Retain context from unselected subsets. Reject
   non-finite input and out-of-range references before geometry calculations.
2. **Build constrained adjacency once.** Crossing requires a shared manifold
   edge with opposite directed incidence, equal subset/domain and compatible
   attribute chart. Seams, open edges, material boundaries and nonmanifold or
   ambiguous contacts are barriers. Check vertex links as well as edge counts;
   a bow-tie vertex can pass a two-faces-per-edge test.
3. **Grow against a fixed plane.** Select a deterministic, well-conditioned seed
   triangle, establish a local origin and double-precision projection basis.
   Require all added vertices to satisfy the same plane-distance bound and all
   faces to satisfy the angular bound. Do not chain neighbor-normal comparisons
   or update the reference plane so successive small bends accumulate unnoticed.
   Recheck the entire completed region against the fixed reference.
4. **Extract the boundary.** Recover directed loops from region incidence.
   Validate closure, winding, unique traversal, vertex links and simple projected
   segments. Verify disk topology and positive projected face orientation. More
   than one loop, touching loops, duplicate/coincident ambiguous sheets,
   projected overlap, or uncertain predicates cause fallback. Signed area alone
   cannot detect compensating overlaps and gaps.
5. **Certify attributes throughout the interior.** Fit UV components to an affine
   field using well-conditioned reference coordinates, then check **every source
   corner**, including removed interior vertices. If each old/new vertex residual
   is at most half the accepted UV error, barycentric interpolation bounds each
   field by that residual and their difference by the full budget over the same
   projected domain. Check unwrapped UVs; a modulo-one comparison is invalid for
   interpolation and texture derivatives. Require constant raw normals for M1.
   A UV perturbation at an interior vertex must cause rejection even if all
   boundary attributes match.
6. **Triangulate the constrained loop.** Start from the existing ear-clipping
   implementation, extracting a private geometry helper if useful. Do not import
   Image Mesh options or height fields. Adapt precision, orientation, cancellation
   and collinear-boundary handling; reject an incomplete or ambiguous result.
   No Steiner vertices, boundary simplification or arbitrary diagonal fan.
7. **Validate against the immutable source.** Check candidate face orientation,
   positive area, exact directed boundary multiset, edge/vertex manifoldness,
   projected coverage, absence of overlapping triangles and strict count
   reduction. Validate stored float geometry, not just double intermediates.
   For a nonzero plane-distance budget, compare original and replacement
   piecewise-linear heights at **all vertices of their projected overlap**,
   including edge intersections. Do not test only corners/centroids or compare
   each pass to its immediate predecessor. Validate near-plane face-normal
   deviation as well as distance; small heights can still produce steep slivers.
8. **Validate surroundings.** Preserve all exact-position boundary incidences,
   including duplicated seams and contacts in unselected subsets. A retained
   boundary prevents many T-junctions but is not a global intersection proof.
   Use a spatial broad phase followed by triangle/segment tests against other
   original and replacement regions, including contacts at otherwise removable
   interior vertices. Reject new crossings, overlaps, unmatched contacts and
   numerically uncertain cases. Nearby geometry matters even in the same subset.
9. **Prepare and publish.** Copy rejected regions from the source without
   reconstructing their attributes. Retained vertices use direct source copies
   and one-to-one provenance. Compact only after acceptance, preserving physical
   duplication required by seams/subsets. Check final physical vertex/index
   limits, metadata and counts before the existing commit gate. On cancellation,
   validation failure or allocation failure, publish nothing.

For uncertain near-degenerate orientation/intersection predicates, either use a
certified robust predicate or reject the region. Adding an arbitrary epsilon to
an ear test is not a proof. Input defects must not be silently repaired.

### Tolerances and work limits

Keep independent, explicit budgets for plane distance (mesh units), face angle
(degrees), source-to-result geometric error (mesh units), UV error (UV units),
normal variation and numeric predicate uncertainty. Do not reuse
`boundaryCollapseThreshold` or the QEM quality percentage.

Proposed initial strict defaults for evaluation: plane distance and geometric
error at most `1e-6 * L`, face-angle limit `0.05 deg`, UV error `1e-6` per component,
and constant-normal residual `1e-6` per component. `L` is a fixed source component
diagonal, never a changing candidate extent or the size of unrelated geometry.
These are proposed numerical budgets, **not validated product defaults**. Test
translated, rotated and widely scaled inputs before fixing them in the API.
If floating-point uncertainty exceeds the requested budget, fall back rather
than silently relaxing it. Reject negative/non-finite options.

M1 does not offer a broad approximate-flattening slider: differences larger than
the strict budget remain unchanged. Supporting useful larger near-coplanar
tolerances requires the full overlay/intersection checks above and separate
visual acceptance; angular similarity alone never enables it.

Use one shared source adjacency/spatial index, stable traversal and bounded
per-region scratch storage. Before shipping, set explicit limits for boundary
size and validation work from measurements; report `work_limit` fallback rather
than running unbounded quadratic/cubic ear/overlap checks. Never silently split
a region and then apply independent error budgets that accumulate. Add cooperative
checkpoints inside adjacency construction, growth, triangulation, overlap and
neighbor checks, and before final allocation/publication.

## Integration decision

| Integration | Decision |
|---|---|
| Automatic prepass before QEM | Defer. Later collapses can invalidate the planar/attribute certificate; QEM failure would also discard a useful planar reduction when the requested ratio is unreachable. |
| Automatic postpass after QEM | Defer. It certifies only the already changed surface, may be less eligible, and cannot recover source UV detail lost by the earlier operation. |
| Explicit operation in the same panel | **M1 choice.** Has a clear source, independent tolerances and no impossible target-ratio requirement. Users can run QEM separately, but its result must not inherit the planar guarantee. |

Implementation should add a private planar-region module beside `mesh-simplifier`,
a distinct public operation/report with scalar options or a value-only options
type, and a corresponding Lua entry point. Names/signatures are to be finalized
in implementation; no new callable method is claimed here. Reuse the instance's
worker/state/progress/cancel ownership and undo transaction, not a second runtime
engine pump. Keep worker data in `MESH_MBM_DEBUG::Impl`; expose no new containers,
backend handles or mutable internal geometry through public headers.

Do not pass a synthetic reduction ratio to reuse QEM preflight. Count accepted
physical vertices directly, including per-subset/seam duplicates, before enforcing
the existing 65,535-vertex contract. Inputs above that indexed limit may succeed
only if the complete validated indexed output fits. Otherwise preserve the source.

The UI action reuses selected-subset controls but must not use the current helper
that discards unselected geometry. Disable shared-frame/virtual-frame collapse
options and ratio controls for this operation. Cancellation, Esc, progress, undo
and Save As retain their transaction semantics. An all-ineligible/no-reduction
result returns an explicit unchanged report and must not replace the mesh, mark
it modified or consume the previous undo. The panel displays cached reports only.

The report distinguishes source/result counts, accepted/rejected region counts,
triangles removed, measured geometry/UV/normal bounds, and stable rejection reasons
(`animated`, `holes`, `attributes`, `topology`, `intersection`, `numeric`,
`work_limit`, `no_reduction`). QEM collapse counts and cost-derived errors must
not be reused to describe these measurements.

## Acceptance matrix

All assertions compare against the original input, not just `meshDebug:check()`.

| Fixture | Required M1 result |
|---|---|
| Dense rectangular grid, affine UVs/constant normals | Strict face reduction; 8x8 cells: 128 to 30 faces with all 32 boundary segments retained. No edge contains an unconnected retained boundary vertex. |
| Four-corner boundary with many interior samples | Two triangles, unchanged corner attributes and orientation. |
| Inclined planes, both windings; small/large scale and translation | Same eligibility/count under representable transforms; finite bounded errors; uncertainty causes unchanged fallback. |
| Concave L/notched contour, collinear boundary samples | `B - 2` faces where certified; no triangle outside the source footprint or across the notch; all boundary segments survive. |
| One/multiple holes, touching rings | Hole-bearing region unchanged, exact indices/attributes preserved; no filled holes. An independent disk in the same mesh may still reduce. |
| UV seam, including coincident duplicated vertices | Seam remains a barrier with both attribute values retained. No weld or new geometric incidence. |
| Interior UV distortion with matching boundary UVs | Region unchanged; interior affine certificate fails. Include a diagonal-change case whose maximal interpolation discrepancy occurs at an edge intersection. |
| Hard-normal seam and smooth varying normals | Seam remains; varying-normal region unchanged. Non-unit constant normals are not silently normalized. |
| Adjacent materials/subsets, identical or different textures | Per-region material identity and subset order unchanged; shared segmentation remains matched. Same behavior for subset-only selection with neighbors retained for validation. |
| Near-coplanar noise and a chain of small bends | Acceptance only within fixed-plane **and** source/result budgets; no accumulated drift. Above-budget regions remain unchanged. |
| Planar cap joined to curved wall; duplicated seam junction | Wall bytes/attributes unchanged; every cap boundary segment matches the original junction, no T-junction or crack. |
| Nearby sheets, crossings, contacts at interior vertices | No new intersection or disconnected contact. Reject uncertain candidate, including same-material neighbors. |
| Nonmanifold edges, bow-ties, duplicate/reversed faces, zero-area faces, NaN/Inf | Deterministic fallback or input error before publication; no repair disguised as optimization. |
| Skeletal bind pose with/without clips; morph frames; articulated Parts | Explicit unsupported result and original geometry/animation metadata unchanged. |
| Non-indexed input; counts around the physical uint16 limit | Exact attribute joining only where eligible; checked output limits before publication; no narrowing overflow. |
| Cancel in each expensive phase; failure after one accepted region; repeated apply | Entire original/modified flag/undo preserved on failure or cancel; no partial results; retry works; late cancellation cannot report success after commit. |
| Fully ineligible mesh; already minimal mesh | Successful unchanged report; no undo replacement or modified flag. |
| Repeated run and idle editor | Deterministic geometry/report; second run no further reduction; idle loop performs no discovery, topology rebuild, serialization or geometry upload for this operation. |

For eligible fixtures, validate area/coverage, winding, connected components,
boundary incidence, no new intersections and attribute bounds, as well as counts.
Save/reload MSH and repeat comparisons. Test cancellation through the real worker
and through the editor, not only a private callback stub.

Visual acceptance must use the engine renderer: before/after with a UV checker,
directional and point lighting, silhouette/oblique views and boundary wireframe.
Capture identical-camera images for dense, inclined, concave, material-seam and
cap/wall fixtures; compare coverage and shaded output within documented raster
tolerance and inspect differences. Wireframe itself intentionally changes inside
the region. Screenshots supplement numeric certificates; they do not prove them.
Record the exercised backend and do not claim untested backend parity.

## Implementation sequence and exit gates

1. Add source/provenance fixtures and independent coverage/attribute/topology
   checks; reproduce current behavior and adversarial cases before adding the pass.
2. Implement private static-region discovery and certification with deterministic
   fallback reasons and cancellable bounded work; adapt/extract polygon helper
   only after its precision/collinear behavior is covered by tests.
3. Add adapter reconstruction by source identity, whole-frame context, immutable
   fallback, physical-limit checks and atomic worker publication. Cover no-op,
   cancellation races, allocation/validation failure and save/reload.
4. Expose the operation in Lua and Mesh Debug with localized ASCII-safe text,
   cached reports, existing undo/cancel flow and an explicit idle-loop audit.
5. Run the full acceptance matrix and visual comparisons. Update
   `docs/mesh-simplification.md`, `docs/lua-api.md`, `docs/core-pimpl-status.md`
   if the public/internal boundary changes, and `include/version/version.h`
   when shipping the feature. Wire any new translation units into CMake and
   Visual Studio project files as applicable.

Do not mark M1 delivered until these gates pass. General hole triangulation,
boundary decimation coordinated across neighboring regions, non-affine attribute
constraints, varying-normal certificates, larger approximate tolerances and
animated equivalence remain separate follow-up milestones.

## Baseline validation

The existing tests inspected are
`src/test-lib/mesh_simplify_smoke.lua`,
`src/test-lib/mesh_simplify_cancel_smoke.lua`, and
`src/test-lib/mesh_debug_simplify_cancel_smoke.lua`.
They exercise static/non-indexed/frame/skeletal/Part reduction, save/reload and
worker/editor cancellation. They do not provide the coplanar acceptance matrix
above. Planned coplanar numeric/visual checks must not be confused with completed
validation.

Executed on Linux/OpenGL ES using the local X11 display after rebuilding the
current Debug target (`cmake --build build --target mini-mbm -j 4`), with
`USE_TEXTURE_MISSING_DIALOG=0` in the existing build cache:

| Check | Observed result |
|---|---|
| `mesh_simplify_smoke.lua` | Passed with `SIMPLIFY SMOKETEST OK`; static octahedron 8 to 4 faces; skeletal fixture 10,738 to 8,590 faces. The test intentionally adds a vertex to an indexed subset and emits the existing index-invalidation diagnostic; no assertion failed. |
| `mesh_simplify_cancel_smoke.lua` | Passed: original positions/normals/UVs/indices, cancellation, retry and late-cancel/commit behavior. |
| `mesh_debug_simplify_cancel_smoke.lua` | Passed: frame, selected subsets and virtual frame; cancel/undo/original preservation and retry. |
| Temporary grid experiment | Created vertices `(x,y,slope*x)` for integer `x,y` in `0..8`, two CCW triangles per cell, affine UVs `(x/8,y/8)` and constant normals. Both `slope=0` and `slope=0.5`: ratio 0.5 produced 64 faces/49 vertices; ratio 0.25 produced 32 faces/33 vertices; ratio 0.1 failed on topology constraints and retained 128 faces/81 vertices. This demonstrates existing reduction, not the proposed 30-face result. |

Launch pattern for the three repository fixtures (each owns its exit condition):

```sh
timeout -s KILL 60 ./bin/debug/linux_x86/mini-mbm \
  --scene src/test-lib/mesh_simplify_smoke.lua \
  --disable_select_monitor --nosplash -w 320 -h 240
```

Repeat with the two cancellation fixture filenames; use a 500x400 window for the
editor fixture. Inspect each test's explicit success sentinel, not only process
exit status. The sandbox initially could not connect to X11; the same launch
worked with approved local-display access. The preexisting binary was older than
the source version, so results above were rerun after rebuilding.

No before/after rendering comparison for a new optimizer has been performed:
there is no new optimizer yet. Concavity, holes, attribute certificates,
near-coplanar acceptance and junction/intersection preservation remain the
implementation acceptance gates above, not conclusions from these baseline tests.
