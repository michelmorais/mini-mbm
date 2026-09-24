# Coplanar mesh optimization — investigation and delivery milestones

Status: **delivery completed at M8 / 7.289.0; final acceptance audit completed**.
See [the final acceptance matrix and reproducible validation](mesh-coplanar-acceptance.md).

History: **M1 delivered in 7.280.0; M2 in 7.283.0; opt-in M3 in 7.284.0; M4 in 7.285.0; M5 in 7.286.0; M6 in 7.287.0; M7 in 7.288.0; M8 in 7.289.0**. Initial investigation
against `547b569f`, 2026-09-24. The design below was recorded before implementation;
this delivery record states the implemented scope and remaining validation limits.

## Final milestone: acceptance audit and closure (completed)

Recorded before work. Freeze feature scope at M8: no new algorithms or UI options.
Audit the original requirements against implementation and existing evidence;
exercise saved/imported repository assets through the native load/simplify/save
flow as well as existing editor, cancellation and geometry fixtures. Review strict
floating-point build assumptions and validate the private numerical helpers at
both native long-double and binary64 precision without claiming platform emulation.
Fix demonstrated issues only. Publish a final acceptance matrix, actual test commands
and results, retained limitations, and an explicit completed status. Reclassify
follow-up ideas as optional improvements driven by real assets, not unfinished
requirements. Non-GLES runtime and native mouse interaction may remain unexecuted
and must be called out; do not imply cross-platform runtime parity from source review.

Closure result: original requirements are mapped to implementation and evidence in
[Final acceptance](mesh-coplanar-acceptance.md). The complete GCC validation and
Clang structural validation pass, including binary64 helper checks and a real OBJ
parse/load/simplify/save/reload fixture. Existing engine assets retain their geometry
when ineligible; the OBJ reduces 256 -> 60 or 4 faces. No production-code correction
or feature-version bump was needed. Runtime platform/mouse/precision limits are
explicitly retained. All remaining feature ideas are optional follow-ups; this
Coplanar delivery has no further implementation milestone queued.

## Milestone 8: strict 3D obstacle separation (7.289.0)

Recorded before implementation. For exactly planar regions, add a sufficient
3D triangle-separation certificate after the existing full-projection test, only
for noncoplanar, nondegenerate obstacles. Test candidate face normals, obstacle
normal and edge-cross-edge directions. Any computed finite nonzero direction is
safe as a separation witness; no claim of complete triangle intersection solving.
Each obstacle must separate from every replacement triangle. Preserve prior
boundary-contact handling if the new certificate fails; work exhaustion rejects.

Bound projection arithmetic outward with nextafter after subtraction, multiplication
and addition, including a positive clearance guard of max(plane tolerance,
region scale * 1e-12), conservatively scaled by axis L1 norm. Reject nonfinite or
uncertain intervals. This avoids accepting a roundoff-only gap or reconstructing
unstable plane intersections. Keep the index, source geometry, attribute/animation
policies, public API and QEM unchanged. Count axis work against the existing budget.

Acceptance: spatially disjoint obstacles whose projections cover solid material,
including plane crossings confined to a hole and perpendicular pieces; reject real
crossings, boundary contact and near-contact. Cover winding, projection axes,
scale/translation, both scopes and boundary settings. Independently test the new
private predicate, interval enclosure, intersecting-pair symmetry and budget/cancel
interruption. Run engine topology/attributes, index, editor, QEM, curved and visual
regressions. No guarantee of accepting all separated geometry or speedup.

M8 validation: Linux Debug build and changed Lua syntax checks pass. The new
spatial engine fixture covers a triangle whose projection reaches solid material
but whose plane cut remains inside the hole, and a perpendicular triangle with
zero-area projection. Across three cyclic axis rotations, scales 1/1024, 1 and
1024 with translations, both windings/scopes and boundary modes, the surrounding
region reduces 120 -> 40 or 120 -> 8. Oriented obstacle faces and attributes remain
unchanged. Real plane crossings, inner-boundary contacts, degenerate obstacles and
approximately planar regions retain their originals. Prior obstacle rejection
fixtures were adjusted to retain actual intersections/uncertified contacts: projected
overlap alone is no longer grounds for rejection; authored boundary-edge contacts
continue through the existing allowed-contact policy.

The standalone `src/test-lib/unit/mesh-planar-separation-test.cpp` verifies 10,000
integer-reference interval enclosures and shared-vertex pairs, rounding/cancellation
arithmetic cases, transverse intersections, touching pairs, near-gap margins,
scaled/translated separations in all axes, nonfinite/degenerate inputs and interrupted
axis checks. It passes normal and AddressSanitizer/UndefinedBehaviorSanitizer builds
(leak detection disabled due to sandbox ptrace); a fast-math build confirms that the
new certificate disables itself. The unchanged index passes 1,200 exhaustive query
comparisons. Run the spatial predicate test with:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic src/test-lib/unit/mesh-planar-separation-test.cpp -o /tmp/planar-separation-test
/tmp/planar-separation-test
```

All existing coplanar, obstacle, hole, boundary, affine-normal, coordination-limit,
worker/editor cancellation/progress/undo, Image Mesh preview/export/project/idle,
QEM and curved regressions pass. Rendered comparisons pass and spatial-island
images were inspected; mean channel difference 0.063446 on 0..255, maximum 205
at isolated raster edges. No non-GLES runtime or native mouse interaction was
executed, and no full-pipeline test forces exhaustion specifically in the new 3D
branch. Interval safety assumes strict floating-point arithmetic; the guarded
fast-math configurations return to prior certificates. No general intersection
solver, deformation certificate or performance improvement is claimed.

## Milestone 7: projected separation of inclined obstacles (7.288.0)

Recorded before implementation. Extend strict projected triangle separation from
coplanar obstacles to inclined obstacles around an exactly planar region. If the
entire projected obstacle is strictly separated from every replacement triangle,
its 3D geometry cannot intersect the region, even when it crosses the seed plane
inside a hole or exterior notch. Reuse the existing half-plane certificate, area
epsilon, work budget and cancellation. Preserve existing coplanar box contacts
and noncoplanar boundary contacts. Overlapping/degenerate projections, uncertain
separation and approximate regions retain their previous conservative policy.
No arbitrary triangle-plane intersection reconstruction or contact relaxation.

Acceptance: crossing-plane and near-plane inclined islands inside holes/notches,
both obstacle windings, inclined region planes, selected/whole-frame scopes and
boundary reduction on/off. Obstacles projecting onto solid geometry or touching
its boundary must still reject; preserve obstacle oriented faces and attributes.
Run existing index, topology, attributes, QEM, curved, worker/editor cancellation
and visual regressions. Both generic editors inherit the native change without
new controls, idle work or changes to animation exclusions.

M7 validation: Linux Debug build and changed Lua syntax checks pass. Obstacle
fixtures now exercise zero, +/-1 and +/-1e-8 offsets from the region plane, for
holes/notches, both windings, horizontal/inclined region planes, both scopes and
boundary settings. Eligible holed regions reduce 120 -> 40 or 120 -> 8; concave
fixtures reach 40 or 10. Oriented obstacle faces and corner attributes remain
unchanged. Projected boundary contacts, overlapping/enclosing obstacles and
zero-area projections remain protected; near-planar regions retain the old guard.
Small offsets can round to zero on inclined float fixtures; horizontal fixtures
retain the nonzero near-plane offsets.

Existing coplanar, holes, boundary, affine-normal, index (1,200 exhaustive
comparisons), coordination-budget fallback, worker/editor cancellation/progress/
undo, Image Mesh preview/export/project/idle, QEM and curved regressions pass.
Rendered comparisons pass and inclined-island before/after images were inspected;
mean channel difference 0.062233 on the 0..255 scale, maximum 205 at isolated
raster edges. No non-GLES backend or native mouse interaction was executed, and
no dedicated test forces exhaustion inside the newly eligible obstacle branch.
The existing work budget includes all triangle-pair checks. Overlapping projections
remain deliberately conservative even when their 3D intersection could be absent.

## Milestone 6: indexed obstacle broad phase (7.287.0)

Recorded before implementation. Build a private balanced AABB hierarchy over source
and unselected-context triangle bounds once per runRegions invocation. Query only
the two retained projection axes, using the region boundary rectangle; intentionally
ignore the dropped axis so the broad phase is a conservative superset of the old
projected-box check even for approximate planes. Inclusive bounds retain contacts.
Feed candidates in original source/context order to the unchanged slab, contact
and strict-separation certificates. Never index progressively replaced geometry.

Index construction/query cancellation is mandatory. Query traversal shares the
region work budget; exhaustion rejects that region and publishes no partial region.
Keep the index and scratch state private, built only on request, no public settings
or persistent cache. The optional coordinated-boundary rerun builds its own index.
No promise of speedup for overlapping projections or small meshes.

Acceptance: independently compare indexed candidates against brute-force inclusive
box overlap for all projection axes, boundary contacts, degenerate boxes and
randomized inputs; deterministic ordering and cancellation. Run all coplanar,
obstacle, hole, attributes, editor, QEM and curved regression suites. Compare the
4,097-region fixture output and elapsed time before/after; report measurements
without treating a single timing as a general benchmark. Inspect rendered output.

M6 validation: Linux Debug build passes. The standalone structural test
`src/test-lib/unit/mesh-planar-index-test.cpp` compares 1,200 queries on 4,097
boxes against an independent exhaustive overlap test, for all three dropped axes,
including inclusive point/edge contacts and zero-extent boxes. It verifies sorted
candidate identity, interrupted construction/query cleanup, retry and empty input.
It visits 153,481 nodes/leaf entries versus 4,916,400 exhaustive box checks; these
are structural counts, not equivalent-cost CPU operations. Run with:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic src/test-lib/unit/mesh-planar-index-test.cpp -o /tmp/planar-index-test
/tmp/planar-index-test
```

The real-engine 4,097-region coordination-limit fixture retains exactly 8,194
result faces, 4,097 accepted regions and boundary fallback. One local before/after
wall-clock measurement (same Linux Debug executable configuration and fixture)
changed from 26.831 s to 0.970 s, including startup. This is not a general benchmark;
small meshes and overlapping projections may not improve. Build/query scratch is
linear, and partition/sort calls have cancellation checks around, not inside, them.
The existing work cap now includes index traversal, so marginal budget cases may
fall back differently. No spatial scratch becomes persistent or publicly exposed.

The structural test also passes AddressSanitizer/UndefinedBehaviorSanitizer with
leak detection disabled (LeakSanitizer cannot run under this sandbox's ptrace).
All existing coplanar, obstacle-separation, hole, boundary, affine-normal, worker/
editor cancellation/progress/undo, Image Mesh preview/export/project/idle, QEM and
curved suites pass. Rendered checker/lighting comparisons pass; island before/after
images were inspected (mean channel difference 0.063446 on 0..255, unchanged from
M5). No native mouse interaction or non-GLES runtime validation was performed.
No dedicated full-pipeline fixture forces exhaustion specifically during indexed
query; structural interruption and existing coordination-limit fallback are tested.

## Milestone 5: strict coplanar obstacle separation (7.286.0)

Recorded before implementation. Refine only the exact-plane, coplanar-obstacle
branch after the existing box/slab broad checks. Certify strict separation of an
obstacle from every replacement triangle using both triangles' edge half-planes
(the convex separating-axis test), with the region's area epsilon as an uncertainty
guard. Handle either obstacle winding. Touching, degenerate, overlapping or
uncertain pairs remain rejected by this new branch. Preserve the existing fast
box-boundary contacts and noncoplanar contact policy. Approximate regions retain
the old conservative behavior. Count each triangle-pair check against the existing
2,000,000-operation budget and cancellation checkpoints; no spatial index or
additional editor options/public storage are introduced.

Acceptance: islands strictly inside holes and exterior concave notches may now
coexist with a reduced region; verify selected and whole-frame scopes, reversed
winding, inclined geometry, boundary reduction on/off and unchanged obstacle
attributes/indices. Intersecting, enclosing, touching and near-planar cases retain
the original mesh. Run topology/attribute, worker/editor cancellation, QEM, curved
and rendering regressions. Both generic editors inherit the native improvement;
no idle work, deformation-policy or QEM change.

M5 validation: Linux Debug build and Lua syntax checks pass. The new obstacle
suite covers holes and exterior concave notches, horizontal/inclined regions,
both obstacle windings, selected-subset/whole-frame scope and boundary reduction
on/off. The holed region reduces 120 -> 40 or 120 -> 8 while its independent
island remains intact; the concave fixture reaches 40 or 10 faces. Overlapping,
enclosing, edge/point-touching and degenerate obstacles reject the transformation,
as does the near-planar fixture. Obstacle verification compares oriented faces
and corner attributes, allowing existing whole-frame physical vertex reordering.

Coplanar, hole, boundary, affine-normal, cancellation/progress/undo, Image Mesh
preview/export/project/idle, QEM and curved regressions pass. Rendered island
before/after images were inspected; mean channel difference is 0.06345 on the
0..255 scale (maximum 205 at isolated raster edges). Existing topology/coverage,
attribute and seam regressions still pass. No native mouse or non-GLES runtime
validation was performed. This adds no spatial acceleration and no guarantee
that every disjoint obstacle will certify; epsilon, work limits and the unchanged
noncoplanar contact policy may still conservatively reject valid regions. No
new-branch budget exhaustion or cancellation timing was specifically injected.
The existing 4,097-region coordination-budget fallback regression also passes.

## Milestone 4: exact affine normal fields (7.285.0)

Recorded before implementation. Extend the generic pass to exactly planar regions
with raw corner normals that satisfy one affine field in the seed-plane projection.
Require zero computed residual (no new normal tolerance), finite nonzero normals
strictly in the seed normal's open hemisphere, and test every source corner,
including aliases. Constant normals keep their existing path. Varying normals on
approximately planar geometry remain rejected. Curved-specific generation retains
its constant-normal policy. No QEM, editor option or public signature changes.

Standard GLES, DX9, DX11 and Metal lighting interpolates linearly transformed raw
normals and normalizes in the fragment shader. An identical affine raw field on an
identical plane therefore preserves its input throughout the region, not only at
the boundary. Custom nonlinear vertex shaders are outside this certificate.
Floating-point predicates remain conservative numerical certificates, not symbolic
exact arithmetic. Roundoff may reject valid fields; never add an epsilon to accept
normal residuals in this milestone.

Acceptance: dense affine-normal planes with/without boundary reduction, inclined
planes, holes, non-unit raw normals, interpolation samples and source values;
reject nonlinear/normalized fields, zero/cancelling normals and near-planar varying
fields. Run existing topology, seam, material, cancellation, editor, QEM and curved
regressions plus lit/checker rendered comparisons. Animation exclusions, work
limits and publication/cancellation remain unchanged. Record actual results below.

M4 validation: Linux Debug build and Lua syntax checks pass. Dense affine-normal
planes (horizontal and inclined) reduce 128 -> 30, or 128 -> 2 with coordinated
boundaries; holed grids reduce 120 -> 40 or 120 -> 8. Tests verify raw source
normals and interpolated values across every output triangle. Adjacent material
charts with distinct affine normal fields retain their separate values and reduce
to four faces total. Nonlinear/unit-normalized fields, zero/cancelling fields and
near-planar varying fields remain byte-for-byte unchanged.

Existing coplanar, holes, boundary, cancellation/undo, Image Mesh preview/export/
project/idle, QEM and curved-specific suites pass. Rendered checker plus directional
and point lighting comparisons pass; the affine-normal before/after images were
inspected. Mean channel difference is 0.0664 on the 0..255 scale, with isolated
raster-edge differences (maximum 205), not pixel identity. Other backends were
inspected in source only, not executed. Custom nonlinear vertex shaders and
non-affine normal approximation remain outside this milestone. No editor loop or
public API storage changed; both generic editors inherit the shared native pass.

## Milestone 3: coordinated straight-boundary reduction (7.284.0)

Recorded before implementation. Add opt-in `planarReduceBoundaries=false` to the
generic pipeline/API and both editors. Preserve legacy results when disabled.
Curved generation keeps authored locks/boundaries and does not enable this option.
Only exactly planar certified regions participate. Remove a sample only when its
3D position lies exactly inside its two collinear boundary neighbors, every source
face incident at that position belongs to a participating region, all aliases have
the same two geometric neighbors, at most two regions meet there, and no lock or
unselected surrounding triangle retains that position. Preserve material/UV/normal
charts independently; never weld physical seam attributes.

Collect certificates on the original input, coordinate decisions by exact position,
then retriangulate from the original source with the shared removal mask. Original
boundary unions remain identical; source attribute and obstacle certificates remain
mandatory. Verify both sides again. If any dependent region rejects its candidate,
discard the entire optional boundary stage and keep the completed M2 candidate.
Cancellation still publishes neither stage. Keep progress monotonic, no idle scans,
and cap coordination at 32,768 boundary entries / 4,096 certified regions. Expose
removed geometric sample count and boundary-stage fallback separately in reports.

Acceptance: dense rectangle 128 -> 2 when enabled (128 -> 30 when disabled),
straight holed loops, adjacent materials and UV charts, planar cap/wall junctions,
selected-scope contacts, nonplanar/attribute/lock protection, all-or-nothing fallback,
no geometric T-junctions, geometry/attribute coverage, deterministic no-op/repeat,
worker/editor cancellation/undo/export/project round-trip and rendered comparisons.
No approximate silhouette reduction, general curved-wall remeshing or deformation
certificate is included. Update docs, version and actual validation record.

M3 validation (Linux Debug / OpenGL ES): dense flat/inclined grids reduce from
128 to 2 faces, a rectangular hole from 120 to 8, two holes from 124 to 14,
and concave inner/outer boundaries to 10. Disabled mode retains 30/40-face results.
Tests cover repeat/no-op, source attribute values, dense coverage, geometric
T-junctions across every subset, material seams, same-subset UV aliases, a planar
cap/perpendicular wall (4 faces total), unselected/attribute-ineligible walls,
near-planar boundary protection, folded planar strips, failed-QEM rollback and
save/reload. Ineligible selected subsets may reorder physical vertices while
retaining their original oriented triangles and corner attributes.

Worker and Mesh Debug cancellation/retry/progress/undo tests pass with coordinated
holed boundaries. Image Mesh persists the option, defaults old projects to false,
retains the open-back fixture's independent quad contacts (94 faces), and reduces
its eligible closed flat box to 12 faces. Preview/export and idle-cache checks pass;
curved-specific fixture counts remain unchanged. The 4,097-region stress test
retains the completed 8,194-face interior-only result and reports boundary fallback.
Rendered checker/lighting comparisons pass and before/after images were inspected.

Remaining limits: no native mouse interaction or non-GLES backend validation;
no allocation-failure injection or forced dependency-certificate failure. The
coordination-cap fallback and transaction rollback/cancellation paths were exercised.
The original general intersection/numerical limits still apply. Curved generation
and unsupported animation remain outside this optional boundary stage.

## Milestone 2: certified hole-bearing regions (7.283.0)

Scope recorded before implementation: extend the shared coplanar pass to a single
simple outer loop and up to 16 disjoint, strictly internal holes. Retain every
boundary vertex/segment, winding, subset, UV and raw normal; no new vertices and
no coordinated boundary decimation in this milestone. Existing angle/distance and
attribute checks remain mandatory. Animated inputs remain excluded.

Reuse the existing generator's visible-bridge/ear-clipping approach conceptually,
with the generic pass's long-double projection, work budget and cancellation.
Validate every loop, nesting/orientation, Euler characteristic, visible bridges,
positive output faces, exact directed boundary and paired interior incidence.
Reject crossing or touching loops, uncertain bridges, incomplete triangulations,
obstacles and exhausted budgets without publishing a partial result. Bound work
by the existing 2,048 boundary-vertex and 2,000,000 operation limits, including
bridge discovery and output validation. A valid region with B retained boundary
vertices and H holes can reach B + 2H - 2 faces.

Acceptance: one/multiple holes, concave outer/inner loops, inclined/reversed/scaled
inputs, shared walls/materials, attribute rejection, nearby obstacles, deterministic
repeat/no-op, cancellation/undo/save/reload and both generic editor flows. Compare
coverage, directed boundaries, winding, intersections and attributes independently
of counts; render checker/lighting before and after. Preserve legacy disk/QEM and
specific curved regression results. Report actual coverage and remaining limits.

M2 validation on Linux Debug / OpenGL ES: build and numeric tests passed for a
single hole (120 -> 40 faces), two holes (124 -> 42), concave hole (122 -> 40),
concave exterior with a hole (88 -> 40), and 16 holes (768 -> 174). The 17-hole
fixture preserves its source and reports the work limit. Tests compare directed
boundaries, source attributes, orientation, area, dense coverage samples, edge
crossings and T-junctions. Inclined/reversed/translated/scaled and near-planar
fixtures, touching-loop/attribute/obstacle rejection, inner-wall subset retention,
determinism, repeat no-op and save/reload passed.

Real worker/Mesh Debug cancellation, retry, late cancel, prior undo and report
paths passed with a holed fixture. Image Mesh project/preview/export validation
removed 352 triangles from its holed fixture; idle-cache, legacy settings and
curved-specific checks also passed. Existing QEM and Image Mesh hole-generation
regressions passed. Checker renders of one/multiple and concave holes passed;
mean RGBA differences for one and two holes were 0.0671158 and 0.0475845 on a
0..255 scale (isolated raster edges reached 205), and images were inspected.

Limits: general spatial intersection solving, coordinated boundary decimation,
non-affine/variable-normal certificates and deformation remain outside M2.
Other backends, native mouse interaction, allocation-failure injection and exhaustive
numeric adversaries were not tested. The 2,000,000-operation budget may reject
otherwise valid regions; this is intentional conservative fallback.

## Follow-up: angular editor control (7.282.0)

Use a shared DragFloat in degrees for `planarAngle` (0..5, default 0.05), with
wrapped 420-pixel tooltips. Compare each original and replacement face normal to
the fixed seed plane, never only to the previously accepted neighbor. Preserve the
legacy dot threshold at the default. Keep `planarTolerance` as a separate advanced
distance safeguard (displayed as percentage of the domain diagonal); saved projects
retain their distance setting and receive the legacy angular default. Curved
plateaus remain exact. Append the angular parameter to both generic APIs, capture
it in workers, validate finite/range constraints, and test real geometric effect,
invalid input, editor forwarding, project persistence and unchanged defaults.

Validation on Linux Debug / OpenGL ES: build, angular/distance numeric fixtures,
invalid-input rollback, native/editor cancellation and undo, project save/default
migration, QEM regression and rendered comparisons passed. The angular fixture
retained 128 triangles at the legacy threshold, rejected a replacement exceeding
1 deg, and reached 30 at 5 deg with distance fraction 0.01. Mean rendered RGBA
difference was 0.0892944/255 (isolated raster-edge maximum 205). The shared degrees
widget and forced tooltip were rendered and visually inspected; native mouse drag
interaction and other render backends were not exercised.

## Follow-up: configurable plane distance (7.281.0)

Expose `planarTolerance` in the generic synchronous/asynchronous API and both
editors, default `1e-7`, finite range `[0, 0.01]`, relative to each connected subset
domain diagonal. Zero requires exact coplanarity. Preserve the fixed seed plane,
angular guard (configurable in 7.282.0), attributes, topology, obstacle checks and transactional publication.
The parameter limits distance to the plane, not the final surface deviation;
`planarMaximumError` remains the measured conservative surface-difference bound.
Show existing rejection counters and explain that small/non-saving candidates are
not counted as rejected regions. Persist Image Mesh settings with legacy defaults.
Curved generation retains exact plateaus and its existing height-error contract.
Validated on Linux Debug / OpenGL ES: shallow nonplanar fixture retains 128 faces
at the default and reaches 30 at `1e-4`; its reported geometry bound remains below
0.002 world units. Zero/exact mode, invalid numeric values without mutation,
attribute/hole/angular fallbacks at the maximum tolerance, asynchronous cancellation
and retry, Mesh Debug parameter forwarding/undo/report drawing, Image Mesh project
persistence/default migration/idle cache, and existing QEM and curved-relief smoke
tests passed. Rendered shallow before/after checker images were inspected; mean
RGBA difference was 0.070694 on a 0..255 scale (isolated edge maximum 205).
Native mouse editing and other render backends were not exercised.

## Delivery record

Implemented the shared private `mesh-planar.h` pass, optional native/Lua modes,
Mesh Debug and Image Mesh general selectors, and the curved generator's separate
Specific / Coplanar + Specific / Coplanar only modes. QEM remains the default and
`mesh-simplifier.cpp` itself is unchanged. No planar geometry scan runs in idle
editor frames. Mesh Debug's virtual grouping is QEM-only; other modes keep full
neighbor context and process selected subsets individually.

Differences from the initial algorithm sketch below:

- Plane distance is fixed at `1e-7 * L` for each original connected subset domain;
  raw normals must match exactly. UV affine residual is at most `5e-7`, bounding
  old/new interpolation difference by `1e-6`. The initial release used fixed conservative settings; 7.281.0 exposes
  the plane distance as described above, retaining the other guards.
- Geometry is bounded by both surfaces' distance from the same fixed plane, with
  the dominant-axis projection factor included. This is a sufficient uniform bound
  over the common projected domain; it does not require enumerating triangle
  overlaps. Boundaries, vertex links, disk topology and projected orientation are
  checked before deterministic ear clipping. The generator's specialized polygon
  helper remains untouched because its tolerances/coordinate contract differ.
- Whole-frame obstacles use conservative slab/projected-bounding-box rejection,
  with exact unchanged boundary contacts allowed. Valid regions may be rejected.
  There is no general intersection solver or spatial tree in this first delivery.
  Boundary size is capped at 2,048 and budgeted boundary/ear/obstacle work at
  2,000,000 operations per region.
- Curved generation only accepts exact horizontal plateaus covered by its existing
  constant-normal policy. It preserves authored locks and extrema, contributes
  zero initial height error, and leaves the specific reducer's original target and
  accumulated error scheme intact. Faceted/interior exclusions remain unchanged.
- Shader programs are not stored in the mesh simplifier's input. The certificate
  is for stored geometry/UVs/normals; arbitrary custom vertex deformation or
  procedural shader output cannot be certified by this operation.

Executed after implementation on Linux Debug / OpenGL ES:

- `mesh_coplanar_smoke.lua`: dense/inclined/concave grids, holes, non-affine interior
  UVs, varying normals, near-planar/bent inputs, both windings, non-indexed input,
  material boundaries, same-subset UV seams, preserved neighboring walls,
  protected interior contacts, original-count combined targets, failed-QEM rollback,
  no-op, multi-frame skip and save/reload; scaled/translated fixtures, a four-face
  fan reduced to two, nonmanifold rejection and compaction of a source buffer with
  65,536 vertices (including unused entries).
- `mesh_coplanar_cancel_smoke.lua`: native cancellation/retry/progress/late cancel;
  Mesh Debug cancellation, completed no-op keeping prior undo, restore and report UI.
- `image_mesh_coplanar_smoke.lua`: all general modes through preview, project save,
  export/reload and idle cache; legacy mode default; all curved modes and curved
  project settings. Curved fixture: 716 front faces -> 490 planar-only (226 removed),
  or -> 358 with the specific reducer, matching the original 0.5 target.
- `mesh_coplanar_visual_smoke.lua`: off-screen engine renders of flat, inclined,
  concave, material-boundary and neighboring-wall fixtures with checker UVs and
  configured directional/point lighting,
  identical oblique camera, nonblank-render assertion and pixel comparison.
  Mean RGBA differences were respectively 0.0660, 0.0550, 0.0629, 0.0304 and 0.0619 on a 0..255
  scale; isolated raster-edge differences reached 205. The images were inspected,
  not assumed equivalent solely from a passing mean-error threshold.
- Existing QEM, native/editor cancellation, Image Mesh general preview/export/batch,
  curved numeric/editor, and geometry-cache smoke tests passed. Existing modes
  retain their original fixture counts.

The wider acceptance matrix below remains a reference for follow-up coverage.
Automated native mouse interaction, other render backends, adversarial allocation
failure injection, exhaustive near-uint16 budgets and every proposed visual fixture
have not been verified here. Passing these fixtures is not a proof for arbitrary
meshes/shaders. The documented conservative fallbacks remain part of the contract.

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
existing simplification action and Image Mesh Editor's general simplification,
also available as **Planar only**. Expose three
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

### Editor coverage

The three modes must reach **all consumers of the engine's generic simplifier**,
not just Mesh Debug. The inspected Lua call sites currently are:

| Consumer | Integration points |
|---|---|
| Mesh Debug | `editor/mesh_debug.lua`: `simplifyAwait`, selected-subset/frame/virtual-frame orchestration and simplification panel. |
| Image Mesh Editor, general simplification | `editor/image_mesh_simplify.lua`: `M.apply` calls `asset:startSimplify`; `editor/image_mesh_editor.lua`: `generate` routes preview, requested statistics, single export, batch export and assembly generation through it. |

Use the same native implementation and mode semantics in both editors. Keep UI
labels/help consistent and localized; do not duplicate the planar algorithm in Lua.
Before delivery, repeat the call-site search for synchronous `simplify` and
asynchronous `startSimplify` to include any additional generic consumers.

For Image Mesh, preserve the existing simplification enable/disable switch.
When enabled, expose QEM (default), Planar + QEM and Planar only. Existing project
files without a mode retain their current behavior: disabled remains disabled,
enabled defaults to QEM. Extend `image_mesh_model.lua` defaults and validation,
project save/load, region option copying, cache invalidation and parameter-change
detection so mode changes cannot reuse geometry or reports from another mode.
Keep saved QEM parameters when switching to Planar only, but do not apply them.

Update `image_mesh_comparison.lua` and statistics to distinguish planar metrics
from QEM cost-derived errors; a planar-only result must not assume QEM report fields
exist. Compare the generated mesh before general simplification with the final
result. Route progress/cancellation through `image_mesh_generation.lua` and the
existing coroutine helper, preserving detached preview publication. Cancelled or
failed work must not replace the prior preview/comparison; do not publish or save
an intermediate planar mesh when the combined operation fails. Existing completed
batch outputs keep their existing semantics; cancellation stops remaining work.

Image Mesh's **specific curved-relief simplifier remains separate from QEM**,
but also gains Specific (default), Planar + Specific and Planar only modes.
Run the planar stage on generation topology before the specific reducer, keeping
boundary/control locks and original height samples. The original face count sets
the specific reducer target; carry the planar geometric error into its existing
accumulated error budget. The first implementation can require exact coplanarity
here so that the initial height error is zero. Neither this path nor its planar-only
mode implicitly invokes QEM. Keep the completed minimal-back optimization.

Other similarly named operations found during the audit are Blender importer
decimation, freehand contour simplification and articulated sprite contour
simplification. They do not call the engine's generic reducer and are not mode
consumers; do not replace their algorithms as part of this delivery.

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
The implemented Lua mode names are `qem`, `coplanar_qem` and `coplanar`;
see `docs/lua-api.md` for signatures and report fields. Reuse instance-owned worker/state/progress/cancel and undo; keep
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

- Run all three general modes through Mesh Debug and Image Mesh Editor. For Image
  Mesh cover preview, explicit statistics, original/result comparison, single and
  batch export, assembly generation, cancellation and retry. Check physical counts
  and geometry after save/reload, not only UI labels.
- Round-trip new Image Mesh project mode settings and load legacy projects with
  general simplification both enabled and disabled. Mode changes invalidate cached
  geometry/reports; idle frames do not trigger regeneration or planar scans.
- Re-run specific curved-relief and minimal-back fixtures. Their settings,
  generation routing, output and cancellation behavior must remain unchanged.
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
4. Expose the three modes in Lua, Mesh Debug and Image Mesh Editor's general
   simplification with localized ASCII-safe text, project compatibility, cached
   reports, existing undo/cancel flow and an explicit idle-loop audit. Cover every
   generation/export consumer and preserve the specific curved-relief workflow.
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

These were baseline results before implementation. New optimizer validation is
recorded in the delivery record above; the baseline alone makes no assertion about
coplanar equivalence.
