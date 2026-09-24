# Coplanar mesh optimization — investigation and first milestone

Status: **integration decision accepted; not implemented**. Investigation against `547b569f`, 2026-09-24.
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

Deliver an optional **Optimize planar regions before QEM** stage in Mesh Debug's
existing simplification action, also available as **Planar only**. Expose three
mutually exclusive modes: **QEM** (default), **Planar + QEM**, and **Planar only**.
Preserve the current QEM path, parameters and failure behavior in the default mode.
The planar stage initially supports:

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

The M1 planar stage skips skeletal data (including weights without clips), multiple
geometry frames, articulated Parts/animation, and known vertex-deformation effects
before mutation, reporting the reason. The combined action can continue with the
existing QEM path for assets it already supports. Rejecting only nonempty `deformationDeltas` is insufficient: a weighted
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
| Optional prepass before QEM | **Accepted M1 choice.** Disabled by default; examines original geometry/attributes, then passes its validated candidate to the existing QEM reducer if needed. |
| Postpass after QEM | Not selected. It cannot certify equivalence to attributes already changed by QEM. |
| Planar-only operation | **Include in M1.** Expose the same certified stage as a user-selectable mode, with no subsequent QEM approximation. |

### Planar-only contract

- Run the same discovery, certification and fallback logic as the combined
  prepass, optimizing all eligible regions within the selected scope and work
  budgets. No target-ratio requirement and no implicit QEM fallback.
- Disable ratio and QEM-specific controls in this mode; retain their values for
  switching back. Expose only applicable planar options and scope controls.
- Leave unsupported assets/regions unchanged and report the reason. If nothing
  changes, do not replace the source, mark it modified or consume the previous undo.
- Preserve the planar certificate through final publication: no later QEM
  reconstruction or normal normalization. The achievable count follows the
  validated boundary and attribute constraints, not an arbitrary percentage.
- Use the same detached transaction, progress, cancellation, final physical-index
  checks, undo and Save As mechanisms as the combined action.

### Combined action contract

- With the option off, execute the existing QEM path without planar discovery,
  new eligibility restrictions or changes to existing success/failure behavior.
- Compute the absolute triangle target **once from the original selected scope**,
  retaining existing rounding and minimum-count rules. Never apply the user's
  ratio again to the smaller intermediate mesh. For example, 1,000 source faces
  at ratio 0.5 still targets 500 after a planar reduction to 700, not 350.
- If planar optimization reaches or passes that target, skip QEM. Certified whole
  regions may reduce below the requested target; report the achieved count.
- Otherwise run the existing QEM algorithm with the remaining absolute target.
  A planar no-op or unsupported region does not prevent QEM from running.
- Keep both stages inside one detached transaction, one progress/cancel lifecycle
  and one undo. QEM failure discards even successful planar work; never publish
  the intermediate mesh as a silent partial success. A fatal planar input/error
  also aborts; ordinary ineligibility is a reported skip, not a fatal failure.
- Preserve the immutable original and its provenance across both stages. Do not
  let an intermediate worker commit close the cancellation gate for the whole
  action; only final publication arbitrates cancel versus commit.
- The planar certificate applies **only to the intermediate result**. The final
  QEM result retains QEM's approximation semantics. Report stage counts/errors
  separately; do not label a QEM-modified result as attribute-equivalent to the
  original. A skipped QEM stage must not run QEM attribute normalization.

Implementation should add a private planar-region module beside `mesh-simplifier`
and an optional, backwards-compatible setting on the existing public/Lua workflow.
Names/signatures are to be finalized in implementation; no new callable method is
claimed here. Reuse instance-owned worker/state/progress/cancel and undo; keep
scratch data in `MESH_MBM_DEBUG::Impl` or private helpers. Expose no new containers,
backend handles or mutable internal geometry through public headers.

Keep legacy preflight unchanged when the option is off. When on, perform input
validation first, then evaluate stage-specific constraints against the appropriate
candidate. Count physical output vertices including seam/subset duplication before
final publication. Do not reject a potentially valid combined result solely using
a ratio estimate derived from the wrong stage, and do not bypass the existing
65,535-vertex output contract. If the current adapter cannot represent a large
intermediate candidate, retain the source and report the limit rather than narrow
indices or publish partial buffers.

The ratio and existing QEM controls remain available in modes that include QEM.
Keep full original-frame
context for planar validation even when selected-subset/virtual-frame QEM operates
on isolated geometry. Do not run the planar stage on the current virtual helper's
already stripped mesh. Unsupported scopes skip the planar stage explicitly without
removing existing QEM functionality. Cancellation, Esc, progress, undo and Save As
retain their transaction semantics. The panel displays cached reports only.

The planar report distinguishes source/intermediate counts, accepted/rejected
region counts, triangles removed, measured geometry/UV/normal bounds and stable
reasons (`animated`, `holes`, `attributes`, `topology`, `intersection`, `numeric`,
`work_limit`, `no_reduction`). The combined report also includes final counts and
whether QEM ran. QEM cost-derived errors remain distinct from certified planar
measurements; they must not be added together as a claimed global error bound.

## Acceptance matrix

The geometric/attribute assertions below test the planar stage against the original
input, not just `meshDebug:check()`. They do not assert unchanged attributes after
a subsequent QEM pass; combined-flow assertions follow separately.

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
| Skeletal bind pose with/without clips; morph frames; articulated Parts | Planar stage skipped with reason and original data unchanged at this stage; combined flow retains existing QEM support. |
| Non-indexed input; counts around the physical uint16 limit | Exact attribute joining only where eligible; checked output limits before publication; no narrowing overflow. |
| Cancel in each expensive phase; failure after one accepted region; repeated apply | Entire original/modified flag/undo preserved on failure or cancel; no partial results; retry works; late cancellation cannot report success after commit. |
| Fully ineligible mesh; already minimal mesh | Planar unchanged report; combined flow still attempts QEM under its existing contract. |
| Repeated run and idle editor | Deterministic geometry/report; second run no further reduction; idle loop performs no discovery, topology rebuild, serialization or geometry upload for this operation. |

Additional combined-flow acceptance gates:

- Planar-only mode: verify eligible reduction, unsupported/fully ineligible no-op,
  undo preservation on no-op, cancellation, save/reload and unchanged retained
  attributes. Verify QEM never runs, regardless of the saved ratio value.
- Switching among all three modes retains parameter values; the default QEM
  mode preserves legacy behavior and planar-only disables QEM-specific controls.
- Option disabled: existing static, skeletal, frame, Part, subset and virtual-frame
  fixtures retain their original results, errors and cancellation behavior.
- Option enabled: verify no-op prepass, partial reduction followed by QEM, and
  target already reached with QEM skipped. Assert targets use original counts,
  including subset batches and rounding; verify intermediate/final report counts.
- Cancel during either stage or fail QEM after a successful prepass: original,
  modified flag and previous undo remain intact. Retry and late-cancel races pass.
- Planar eligibility failure for animation or holes must not become a new QEM
  rejection. Compare the unchanged prepass input passed to QEM with the legacy path.
- Progress is monotonic across stages, all publication happens once, and disabled
  or idle editor frames never run planar discovery.

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
4. Expose the three modes in Lua and Mesh Debug with localized ASCII-safe text,
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
