# Real-Time Facial Animation - Implementation Plan

Status: **Phase 7 Rig actions and facial playback comparison slice complete; runtime, Blender interchange, and capture naming foundations are complete**
Last reviewed: **2026-10-24**

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
The Phase 5 producer contract is in [Live capture naming contract](realtime-facial-animation-phase5.md).
The Phase 6 producer contract is in [Deterministic capture replay](realtime-facial-animation-phase6.md).
The Phase 7 editor design is in [Facial rig authoring editor](realtime-facial-animation-phase7.md).

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
budget. GPU morph evaluation is enabled for the measured OpenGL ES 2 backend after the representative
Megan/Fuzzy Fury profiles established the texture capacity and layout contract. Other backends keep
the CPU reference until they provide an equivalent indexed resource.

A local Fuzzy Fury fixture now provides that representative geometry: approximately 57.7k exported
vertices, six facial targets, and one clip. It is currently a procedural facial test fixture without
an armature; use it to measure the isolated CPU morph path before deciding whether a GPU path is
needed. A Megan Mixamo fixture now provides the composition test asset: 65 canonical bones, 34,164
facial vertices, six facial targets, one 211-frame facial clip, and the original 211-frame skeletal
clip. MeshDebug preview confirmed isolated and simultaneous facial-plus-skeletal playback. The
headless foundation suite now also verifies that facial and skeletal metadata and sections 41-43
survive load/save round trips in a representative combined asset.

The facial clip interchange milestone is complete: named facial curves are preserved through the
Blender `.blend`/MSH workflow. The route is covered by a headless Blender 5.1 regression that
reloads the generated file and checks target names, metadata, clip Actions, timestamps, weights,
and both linear and step interpolation.

MeshDebug-owned FBX exports now carry a serialized facial clip payload in mesh custom properties.
When Blender's FBX importer drops the original Shape Key Actions, the direct MSH importer restores
clips from that payload. Arbitrary external FBX files without this metadata still provide target
geometry only; the transport-independent naming contract is defined in Phase 5 below.

### Phase 5: Live capture naming contract

**Completed for the transport-independent mapping slice on 2026-10-23.**

The editor module editor/facial_capture_mapping.lua defines the boundary for an optional
Faceit/ARKit-compatible producer. It discovers the exact target names in a loaded mesh, resolves
canonical names and conservative Faceit side-suffix aliases, clamps finite incoming weights, and
reports unknown names, duplicate source names, missing required targets, rejected values, and
failed runtime assignments. It caches the last applied values and skips assignments within an
epsilon, so a later capture loop does not upload unchanged weights every frame.

The module has no camera, network, or vendor SDK dependency. A future OSC, webcam, or native
capture adapter can feed a name-to-weight table into the same contract without changing the .msh
format or the core renderer. Phase 6 supplies a deterministic file-backed producer; a live transport
remains a separate integration milestone.

### Phase 6: Deterministic capture replay adapter

**Completed for the file-backed packet and replay slice on 2026-10-23.**

editor/facial_capture_stream.lua now accepts a strict line-oriented JSON packet format with a
timestamp, optional sequence, and named finite weights. It provides a deterministic replay clock,
ordered packet validation, looping, end-of-stream state, and optional reset-on-disconnect behavior.
The adapter calls the Phase 5 mapping contract and remains independent of camera, network, and
vendor SDKs.

This gives the editor and runtime a reproducible capture source for regression tests and manual
facial-plus-skeletal review. The next transport milestone can implement OSC or webcam input by
producing the same validated packet table.

Measure the optimized CPU path on representative face meshes and target counts, then validate a
rigged facial-plus-skeletal asset. The first GPU composition slice reuses the existing GPU LBS shader
for the skeletal stage, avoiding a second CPU deformation pass while preserving the morph-then-skin
contract. The GLES2 morph slice now uploads indexed position/normal deltas to a float texture and
samples them in the vertex shader with per-frame weights. It is selected only after capability and
layout validation; failed uploads or shader specialization fall back to CPU. Establish limits for
active targets, vertex counts, buffer memory, and shader resources; do not assume one backend's
capacity applies to all others.

The current GLES2 runtime exposes both stages in `getFacialAnimationReport`: the skeletal stage may
be `gpu-skeletal-composite`, while the morph stage reports `gpu-indexed-deltas` and
`gpu-morph-ready` when the texture/shader path is active. Fallback statuses are explicit, so a
partial or failed GPU path is not reported as complete facial GPU evaluation.
At graphics initialization, GLES2 now measures vertex texture units, maximum texture size, and
floating-point texture support. These prerequisites are recorded independently from implementation
readiness so a future indexed-delta shader can be enabled only after parity tests pass.

### Phase 7: Dedicated facial rig authoring editor

**The first shell/setup slice is implemented; the remaining rig and expression work is staged below.**

The project will provide a standalone facial rig editor whose application shell follows the
existing skeletal animation editor. It will reuse shared viewport, serialization, mesh inspection,
selection, gizmo, and launcher conventions while keeping a separate facial project state and
separate Lua entry point. The skeletal animation editor remains focused on bones, animation
timelines, skeletal masks, and skeletal playback. Mesh Debug keeps the compact facial
playback/import preview.

The facial editor will expose only facial workspaces: setup and object roles, face position, landmarks,
masks/regions, facial rig preview/binding, expressions, facial clips, and preview. Skeletal timelines,
general skin painting, and unrelated skeletal controls stay out of its UI; a small shared binding
helper is allowed only where facial geometry needs it.

The workflow is intentionally compatible with Faceit concepts without depending on Faceit internal
files: register facial objects, place a face reference origin in its dedicated worktree, position landmarks independently or with synchronized sides around the nose, assign semantic regions, generate a facial rig,
bind geometry, author expressions, and export ARKit-compatible named targets and clips. Faceit
compatibility means compatible names, regions, landmarks, weights, and Blender/MSH interchange;
it does not mean reproducing Blender add-on internals.

The Blender bridge is an optional authoring interchange artifact. The mini-mbm runtime consumes the
facial sections, target weights, and clips stored in `.msh`; it does not require the bridge Lua file
or Blender to load or play a finished asset. While native binding is still being implemented, the
bridge remains the handoff used for Blender-side armature and weight work.

The Expressions worktree now authors named target-weight poses from existing MSH facial targets,
including bounded preview sliders, capture/apply, side mirroring, rename, persistence, and validation.
The Clips worktree now authors temporal facial clips from expression poses and target-weight keys.
The Preview worktree reviews saved poses, imported facial clips, authored project clips, and skeletal clips together,
while GPU morph parity for
DirectX and Metal remains a separate backend milestone; those
backends keep the CPU reference path until an equivalent indexed resource is implemented.

Exporting authored clips to Blender Shape Key Actions is optional interoperability. The native MSH
serialization of authored clips is a separate runtime milestone; Blender is not required to play
finished assets.

The Rig worktree exposes the two materialization actions before its reports: add missing canonical
facial bones (or repair the facial root parent) and apply the verified facial weight assignment.
Saving the derived MSH is enabled only after both operations. Read-only diagnostics are grouped
separately from the optional Blender bridge/workflow controls.

Preview controls document the distinction between skeletal clips, project-authored facial clips,
individual target overrides, saved expressions, and facial clips embedded in the MSH. The face
origin click remains the non-destructive anchor for the landmark reference guide; individual
landmarks still require explicit confirmation on a new mesh.

The facial editor keeps its loaded mesh on the engine `alwaysRender` path so close-up authoring is
not lost to frustum culling. The optional CPU comparison mesh uses the same temporary override and
clears it when the comparison is removed.

Landmark authoring also provides an explicit surface-depth snap along the face Z axis, with
main-face mask preference and a signed axis offset. This is a placement aid, not an automatic
validation step; anatomical pivots such as jaw hinges and eye centers remain manually adjustable.
An optional anatomical reference mode supplies conservative per-landmark offsets and can be
disabled whenever an asset needs explicit manual placement.
Depth solving is axis-aligned in mesh space (default `Z`) and independent of viewport camera
rotation.

The Landmarks worktree also includes a labeled reference guide for all canonical points, drawn with
Lua and ImGui primitives. It is available as a compact guide beside the selected landmark and as an
optional centered, closable large view for close inspection. The guide does not alter the project and
has no external image or texture dependency.

Capture remains downstream of this editor. The file-backed replay and named-weight contract are
producer tests; OSC, webcam, and SDK adapters must drive a prepared rig rather than define the
rigging workflow.

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
