# Real-Time Facial Animation - Implementation Plan

Status: **Phase 4 sparse CPU optimization and facial-plus-skeletal validation complete; GPU morph path not yet justified; FBX facial-curve round trip pending**
Last reviewed: **2026-10-23**

Phase 0 results, reproduction commands and the bounded first-runtime design are in
[Blender interchange and initial design](realtime-facial-animation-phase0.md). Phase 1 evidence,
the V11 payload contract, and CPU runtime validation are in
[Phase 1 asset and runtime slice](realtime-facial-animation-phase1.md). The generated Blender
5.1.2 fixture preserves vertex association and sampled weights through the helper, FBX and GLB.
The canonical initial route is `.blend` through the existing Blender helper. A real baked Faceit
character and custom-normal fidelity remain integration acceptance items for later phases. Phase 2
evidence and the Lua contract are in [Facial controls and playback](realtime-facial-animation-phase2.md).
The Phase 3 editor decision and implementation slice are in
[Blender import and Mesh Debug preview](realtime-facial-animation-phase3.md).
The first Phase 4 performance profile is in [CPU performance profile](realtime-facial-animation-phase4.md).

## 1. Purpose

Add a Blender and Faceit based workflow for authoring facial expressions and facial animation, then
play the resulting animation on mini-mbm meshes. The initial target is blend-shape animation
(Blender shape keys / morph targets), including clips driven by animated shape-key weights. This is
separate from the existing skeletal animation system: the two systems may be present on the same
mesh and must compose predictably.

Blender and Faceit are authoring and capture tools. The `.msh` asset and mini-mbm runtime remain the
delivery path for games. ARKit-compatible names are useful conventions for capture and retargeting,
but the runtime format must not require a specific vendor's target set.

## 2. Design Principles

- Preserve base geometry and store morph data as vertex deltas, rather than duplicating complete
  meshes for every expression.
- Keep immutable morph data with the cached mesh asset and keep current weights, clip time, and
  overrides on each `MESH` instance.
- Allow a face to combine several active targets at once, with deterministic weight clamping and
  clip/manual-control composition rules.
- Keep facial deformation backend-neutral at the asset and playback layers; optimize the render path
  per backend only after a correct reference path exists.
- Reuse Blender/FBX tooling where practical, but explicitly validate shape-key and animation support
  rather than assuming general FBX support includes facial deformers.
- Preserve compatibility for existing `.msh` files and reject malformed or unsupported facial data
  before exposing a partially loaded asset.

## 3. Proposed Delivery Order

### Phase 0: Interchange spike and bounded design

**Completed for the synthetic profile on 2026-10-23.** See the
[evidence and limitations](realtime-facial-animation-phase0.md#2-reproduction-and-evidence).

Use a small Blender fixture with a skinned face, a handful of shape keys, and one animated facial
clip. Confirm what the project's Blender/FBX boundary can reliably extract. Compare extending the
current Blender helper protocol with importing glTF 2.0 morph targets; select one canonical import
path and document coordinate, vertex-order, normal, naming, and animation-curve behavior.

Deliverable: a format/API/render design note, a reproducible fixture, and an explicit first-backend
scope. Do not begin runtime implementation until vertex identity and animated weights survive the
chosen interchange path.

### Phase 1: Asset representation and CPU reference deformation

**Completed for the bounded Linux OpenGL ES path on 2026-10-23.**

Extend Mesh V11 with optional, versioned facial sections for:

- A stable mesh/target identity, target name, and target count.
- Per-target position deltas and optional normal deltas associated with frame-0 vertex order.
- Named facial clips containing timestamped target weights and interpolation metadata.
- Optional default weights, normally zero, and any ARKit/FACS naming metadata needed for lookup.

Use new section type values after the existing V11 sections, including autoplay section 44. Keep
payloads bounded and validate counts, indices, identities, finite values, vertex topology, clip
durations, key ordering, and section relationships. Document whether target names are unique per
mesh and how absent normal deltas are handled. Readers and writers must preserve facial data during
ordinary load/save and relevant geometry operations.

Implement a CPU reference path that combines active deltas from immutable base geometry into
per-instance dynamic vertex data. Recompute only when weights or the base pose change. Define and
implement the deformation order for assets that have both morph targets and skeletal skinning;
default to morphing bind geometry before skinning. Include deformed bounds or a conservative bound
that keeps animated expressions visible to frustum culling.

Deliverable: a `.msh` round trip and an inspectable runtime report showing target and clip counts,
active execution path, and validation failures.

### Phase 2: Runtime and Lua controls

**Completed for per-instance controls on 2026-10-23.**

Add per-instance APIs to:

- Enumerate facial targets and clips, and read current target weights.
- Set, reset, and clear manual weights by stable target name or index.
- Play, pause, resume, stop, seek, and query facial clips.
- Report unsupported data or execution conditions without silently dropping expressions.

Specify interactions between manual weights and clip playback, clip loop/clamp behavior, playback
speed, and transitions before finalizing public names. Keep the API usable for ARKit-style live
weights as well as authored offline clips. Any realtime streaming transport is a later project.

Deliverable: a Lua example that blends multiple expressions and plays a facial clip on a mesh that
also uses skeletal animation.

### Phase 3: Blender/Faceit import and editor preview

Integrate the selected Blender interchange path into Mesh Debug's existing Blender import workflow.
Import shape-key deltas and weight curves transactionally, preserve existing skeletal data, and
report unsupported modifiers, topology changes, missing targets, and animation sources with clear
diagnostics. Export support round-trips facial targets and clips back to Blender's canonical
`.blend` interchange. FBX preserves the shape-key geometry in the tested Blender version, but its
facial animation curves are not restored on re-import; FBX curve round-trip remains a separate
acceptance item.

Add a focused preview surface to Mesh Debug, backed by a small companion module rather than adding
facial controls to the large skeletal authoring editor: target list, weight sliders, clip selection,
playback controls, and visible validation status. Keep scans, mesh rebuilds, uploads, and serialization
event-driven; an idle editor must not recompute facial deformation continuously.

Deliverable: edit/import, save, reload, preview, export, and re-import a fixture without losing
target identity or animation curves.

### Phase 4: Backend optimization and capture workflows

**Initial CPU profile and sparse target optimization completed on 2026-10-23.** The testLib benchmark
now measures small, medium, and large synthetic profiles and reports dense versus sparse median/p95
timings and dense delta memory. For 20,000 vertices, 52 targets, and 8 active targets, the Debug
reference measured approximately 8.992 ms dense versus 2.454 ms sparse. Targets decoded from MSH files now carry
indices for vertices with non-zero position or normal deltas; manually constructed fixtures retain
the dense compatibility path. This is evidence for a later decision, not a hardware-independent
budget. GPU morph evaluation remains deferred until representative game meshes or a target backend
show that the optimized CPU path is insufficient.

A local Fuzzy Fury fixture now provides that representative geometry: approximately 57.7k exported
vertices, six facial targets, and one clip. It is currently a procedural facial test fixture without
an armature; use it to measure the isolated CPU morph path before deciding whether a GPU path is
needed. A Megan Mixamo fixture now provides the composition test asset: 65 canonical bones, 34,164
facial vertices, six facial targets, one 211-frame facial clip, and the original 211-frame skeletal
clip. MeshDebug preview confirmed isolated and simultaneous facial-plus-skeletal playback. The
headless foundation suite now also verifies that facial and skeletal metadata and sections 41-43
survive load/save round trips in a representative combined asset.

The next milestone is facial clip interchange: preserve named facial curves through the Blender
`.blend`/MSH workflow, then document the remaining FBX curve limitation before evaluating a live
capture producer.

Measure the optimized CPU path on representative face meshes and target counts, then validate a
rigged facial-plus-skeletal asset. If the measured cost still requires it, add GPU morph evaluation
for explicitly selected backends, with CPU fallback where supported. Establish limits for active
targets, vertex counts, buffer memory, and shader resources; do not assume one backend's capacity
applies to all others.

After offline import and runtime controls are stable, evaluate ARKit-compatible live capture and
audio-driven generation such as Audio2Face as producers of named weights. Keep capture transport and
third-party runtime dependencies optional and outside the core asset contract.

Deliverable: backend parity evidence for enabled paths and a separately scoped capture integration
proposal if a concrete game workflow needs one.

## 4. Validation and Acceptance

- Static target data and animated curves survive Blender import -> `.msh` save -> load -> export ->
  re-import with stable names, vertex association, and weights within documented tolerances.
- Several targets blend together correctly; zero weights reproduce the base pose.
- Facial animation and skeletal animation compose in the documented order, on each mesh instance,
  without mutable state leaking through the shared asset cache.
- Existing non-facial `.msh` files continue loading, and facial data round-trips through supported
  mesh edits without silent loss.
- Invalid indices, non-finite values, topology mismatch, duplicate identities, and unsupported
  section versions fail with useful diagnostics and no partial asset mutation.
- Culling, lighting normals, pause/seek, reload, destruction, and multiple instances are covered.
- Performance is measured for the reference CPU path and every GPU path before declaring a backend
  supported.
- Editor work is dirty/event-driven; idle preview does not continuously rebuild or upload the
  deformed mesh.

## 5. Risks and Decisions to Resolve in Phase 0

- Blender FBX import/export may not preserve the required blend-shape curves consistently; compare
  that path with glTF 2.0 and choose a single canonical route for the first delivery.
- Mesh subset preparation, vertex splitting, optimization, and simplification can reorder or duplicate
  vertices; morph data must follow the same source-vertex mapping as base geometry.
- Position-only deltas are cheaper, while normal deltas improve lighting at expression extremes.
  Fixture results should determine whether normal deltas are required in the first format version.
- A general target set may be much larger than a typical face rig. Define sparse/dense storage and
  active-target limits from measurements, not assumptions.
- Existing custom vertex shaders have no established morph-target contract. Define explicit
  compatibility behavior before exposing facial assets to custom shaders.
- Facial clips, procedural/live weights, and manual overrides need a clear precedence rule to avoid
  surprising results when composed.

## 6. Project Integration

This plan is tracked independently from completed skeletal animation. Implementing facial animation
does not reopen or change the accepted skeletal runtime contract. When delivered, update the Mesh V11
format and Lua API documentation, the Blender import/export guide, editor documentation, and
`include/version/version.h`. Add completed capabilities to
[Real-Time Skeletal Animation and Editor](realtime-skeletal-animation.md) only where the shared
skeletal/facial composition contract belongs.
