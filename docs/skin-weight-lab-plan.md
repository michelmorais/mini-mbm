# Skin Weight Lab — Product Discovery and Delivery Plan

Document version: **0.26**
Status: **Current editor validation in progress; rigid cavity and normalization milestones approved**
Last updated: **2026-08-09**

## 1. Purpose

This document is the versioned reference for a proposed Skin Weight Lab inside a future standalone
**Real-Time Skinning Editor**. Mesh Debug remains the source of proven interaction patterns and the
temporary home of existing bone/weight tools, but is not the intended permanent home of this
workflow. This document records the problem, intended user workflow, phased scope, validation
criteria, risks, decisions, hypotheses, and open questions before implementation begins.

The companion [Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md) records the
runtime and editor architecture needed to support both Linear Blend Skinning (LBS) and Dual
Quaternion Skinning (DQS). The plans are deliberately linked: authoring weights without a
trustworthy deformation preview is incomplete, while runtime skinning without usable authoring and
diagnostics is difficult to validate.

The plan comes from a concrete character: a stylized alien rat with a large head and torso, almost
no visible neck, short legs, long arms, a tail, and a hollow rectangular abdominal cavity that must
remain rigid. Mixamo can produce a usable humanoid rig for the character, but the resulting skin
weights still show neck tearing and deformation of the abdominal cavity. Its standard humanoid
skeleton also has no tail chain.

This document deliberately separates confirmed requirements from ideas that still require hands-on
user testing. A listed hypothesis is not an implementation commitment.

## 2. Current State and Confirmed Facts

- Mini MBM persists an editor-only bind-pose skeleton in `SECTION_FRAME_SKINNED`.
- Mini MBM persists up to four named bone influences per frame-1 vertex in
  `SECTION_VERTEX_SKIN_WEIGHTS`.
- The runtime renderer does not perform skeletal deformation. Bones and weights exist for editor
  diagnostics and Blender/FBX round trips; runtime animation remains pre-baked frame animation.
- Mesh Debug can add, edit, recompute, and remove bones.
- Recompute/Recompute All updates bone direction, orientation, and length. It does not generate or
  smooth vertex weights.
- Mesh Debug's Rigid Bind writes weight `1.0` to one bone for selected vertices. Its current
  selection is by subset or proximity to a bone segment.
- Split Capture already provides an interactive, resizable AABB and several face-selection modes:
  Face Center, Entire Face, Any Vertex, and Intersecting Face. It also has preview, island filtering,
  explicit Apply, and one-level rollback concepts worth reusing.
- Blender/FBX import can preserve weights authored by Mixamo or another DCC tool. FBX export first
  creates envelope fallback weights for the whole mesh and then applies persisted weights as
  authoritative per-vertex overrides.
- The reference rat armature is stored in
  `src/test-lib/T-BONE-rato-from-mixamo_armature.lua`. It contains a 41-bone humanoid Mixamo
  hierarchy and no tail bones.
- Mesh Debug already contains an articulated-animation workflow with hierarchical parts and pivots,
  clips, position/rotation/scale tracks, quaternion runtime rotation, easing, a timeline, looping,
  priority, and Absolute/Additive composition. These are useful product and interaction references,
  but articulated parts and skeletal vertex deformation remain different data models. See the
  [Articulated Animation guide](articulated-animation.md).

## 3. Problem Statement

Positioning a skeleton correctly is necessary but insufficient for good deformation. The editor
does not currently provide a practical workflow to inspect, select, diagnose, smooth, or locally
replace imported skin weights.

The user can see a deformation problem in an animated FBX, but cannot answer these questions inside
the current tools:

- Which vertices have missing, invalid, distant, or abruptly changing influences?
- Which vertices will be affected before an edit is applied?
- Can one local region be corrected without destroying good Mixamo weights elsewhere?
- Can a mechanically rigid region remain rigid while nearby organic geometry transitions smoothly?
- Can a hand-authored tail chain receive progressive weights without regenerating the entire body?

## 4. Desired Outcome

The standalone Real-Time Skinning Editor should support a safe, visual, region-based workflow for
improving existing skin weights without requiring a full re-rig and without silently replacing good
imported data. Skin Weight Lab is one workspace of that editor, alongside bind-pose, animation,
preview, and backend-capability workspaces planned in the companion document.

Success means the user can:

1. import a Mixamo-rigged character with its real weights;
2. identify a problematic region visually;
3. select vertices without changing or splitting geometry;
4. preview a proposed weight operation;
5. apply it only to the intended vertices;
6. undo the last applied weight operation;
7. export to FBX and validate the result in Mixamo or Blender;
8. repeat the loop without losing unrelated good weights.

## 5. Actors and Responsibilities

### Primary user

The editor user positions bones, selects regions, chooses intended bone behavior, reviews the
preview, applies changes, and validates exported animation externally.

### Real-Time Skinning Editor

The standalone editor must expose understandable selection, diagnostics, weight operations,
warnings, preview, and rollback. Its Skin Weight Lab workspace owns this plan's workflow. It must
not claim that a heuristic result is anatomically correct.

### Mesh Debug

Mesh Debug remains a behavioral reference and possible migration source for bone editing, Split
Capture selection, and articulated-animation interaction. The new editor should reuse shared data
and services where practical instead of copying an already large Lua implementation.

### Blender/FBX bridge

The existing bridge remains responsible for importing/exporting the armature and weights. It is an
external dependency for animated validation and may later provide optional heavy calculations.

### Mixamo

Mixamo remains an external auto-rigging and animation service. Its output is test input, not a
deterministic dependency controlled by Mini MBM.

## 6. Product Principles

1. **Preserve before regenerating.** Imported Mixamo/Blender weights remain unchanged outside the
   explicit selection.
2. **Preview before mutation.** Expensive analysis and destructive edits occur only on explicit
   commands, never every frame merely because a panel is open.
3. **Selection is not cutting.** Reusing Split Capture's volume must not create subsets, duplicate
   geometry, or change topology.
4. **Rigid core and flexible boundary are different concepts.** A rigid cavity must remain rigid;
   any falloff belongs outside its protected core.
5. **Diagnostics report suspicion, not truth.** Stylized anatomy and rigid props legitimately violate
   rules that would be correct for ordinary organic skin.
6. **Local repair before global replacement.** Full weight regeneration is a later, explicitly
   destructive fallback.
7. **User testing is a delivery gate.** A phase is not considered successful merely because its
   calculations complete without errors.
8. **Prepare LBS and DQS together.** Persisted weights, bind-pose validation, animation sampling,
   diagnostics, and editor state must not assume only one deformation method.
9. **Backend limits are capabilities, not global product limits.** A constrained OpenGL ES backend
   may require a smaller palette, partitioning, or an explicit unsupported result; it must not force
   omission of LBS/DQS from better-capable backends or cause a silent method switch.

## 7. Core Domain Concepts

### Weight set

Up to four `(bone name, weight)` influences for one frame-1 vertex. Used weights should be
non-negative, reference existing bones, and normally sum to approximately `1.0`.

### Selection volume

An editable AABB used to identify vertices or faces. It borrows interaction and visualization from
Split Capture but produces only a temporary selection.

### Rigid core

Vertices that must all receive weight `1.0` from one selected bone. Example: every surface of the
hollow rectangular abdominal cavity.

### Transition shell

An optional adjustable region outside the rigid core. Changes are progressively weaker toward the
outer boundary while the core remains fully rigid.

### Weight discontinuity

A large difference between the weight sets of geometrically adjacent vertices. It is a diagnostic
signal, not automatically an error.

### Pose stress test

A diagnostic preview that temporarily rotates a chosen bone and estimates or displays likely
stretching/tearing without persisting an animation or changing the bind pose.

### Skinning method

The deformation method selected for preview or runtime: LBS or DQS. The weight authoring model is
shared, but results can differ. The selected method and any fallback must always be visible to the
user; backend capability handling must not silently change it.

## 8. Main User Flow

1. Open a mesh containing a skeleton in the standalone **Real-Time Skinning Editor** and select the
   **Skin Weight Lab** workspace.
2. Choose a selection method.
3. Position the selection volume or choose a subset/bone-proximity source.
4. Click **Analyze Selection**.
5. Review the highlighted vertices/faces and the affected vertex count.
6. Choose an operation and its inputs.
7. Review a non-destructive result preview and warnings.
8. Click **Apply** or **Cancel**.
9. If necessary, use **Revert Last Weight Operation**.
10. Save/export the mesh and validate animation externally.

When the runtime preview foundation exists, the same flow should allow comparing LBS and DQS using
the same pose, clip, camera, and weight data.

Changing selection geometry or relevant operation inputs invalidates the cached analysis. The user
must analyze again before Apply becomes available.

## 9. Selection Requirements

### Initial selection methods

- AABB/box selection, reusing the Split Capture interaction model.
- Existing material subset.
- Proximity to a selected bone segment.

### AABB inclusion modes

- Face Center.
- Entire Face.
- Any Vertex.
- Intersecting Face.
- Direct Vertex Inside Volume, if user testing shows face-derived selection is too broad for weight
  editing.

### Selection behavior

- Show selected vertices/faces before any weight modification.
- Report selected vertex count and affected subsets.
- Do not split, duplicate, reorder, or remove vertices.
- Use frame 1's vertex indexing, matching `SECTION_VERTEX_SKIN_WEIGHTS`.
- Handle duplicate position entries created by UV seams or hard-normal edges predictably. Whether
  coincident entries are selected together remains an open decision.
- Island filtering should be available if direct reuse from Split Capture remains understandable in
  user testing.

## 10. Proposed Weight Operations

### 10.1 Rigid Bind

Set every selected core vertex to exactly one chosen bone with weight `1.0`, removing other
influences from those vertices.

Primary use cases:

- hollow abdominal cavity;
- weapons and accessories;
- eyes or mechanical pieces;
- rigid segments of a stylized character.

### 10.2 Blend Bone Into Selection

Introduce or strengthen one chosen bone while proportionally retaining existing influences. Final
weights are normalized and limited to four influences.

The exact meaning of the user input is not decided. Candidates include target weight, additive
strength, and replacement strength.

### 10.3 Smooth Selected Weights

Reduce abrupt changes by blending a vertex's weights with geometrically adjacent vertices. The user
must be able to restrict the allowed bones so smoothing a neck cannot accidentally introduce arm,
jaw, or opposite-side influences.

Potential inputs, pending user validation:

- number of smoothing passes;
- strength per pass;
- allowed bone set;
- locked bones or locked vertices;
- whether the selection boundary is fixed or participates in smoothing.

### 10.4 Remove Bone Influence

Remove a selected bone from selected vertices and redistribute/normalize the remaining weights.
The operation must warn if a vertex would become completely unweighted.

### 10.5 Normalize and Limit

Normalize selected vertices and retain at most four strongest influences. This is both a standalone
repair and a mandatory finalization step for operations that can create or change influences.

### 10.6 Full Regeneration

Regenerate weights for the whole mesh or a broad selection. This is explicitly outside the first
delivery because the appropriate algorithm and Blender dependency are unresolved. If introduced,
it must preserve a restorable snapshot and clearly distinguish generated approximation from
authored/imported data.

## 11. Rigid Core and Falloff

The cavity use case requires two independently visible regions:

```text
outer selection boundary
┌───────────────────────────────────┐
│ transition shell                  │
│   ┌───────────────────────────┐   │
│   │ rigid core                │   │
│   │ target bone weight = 1.0  │   │
│   └───────────────────────────┘   │
│ progressively weaker adjustment  │
└───────────────────────────────────┘
```

Rules:

- A zero-width transition shell means a hard rigid selection.
- Increasing shell width must not soften vertices inside the rigid core.
- Existing weights outside the outer boundary remain byte-for-byte unchanged.
- The falloff curve should initially offer a small understandable set: Linear and Smooth.
- Preview must visually distinguish rigid core, transition shell, and unaffected vertices.
- The behavior when the shell reaches unrelated/disconnected geometry is an open question; island
  filtering may be the answer.

## 12. Diagnostic Scope

### Phase-one integrity diagnostics

- vertex has no effective influence;
- total weight is not approximately `1.0`;
- negative or non-finite weight;
- referenced bone name does not exist in the current skeleton;
- more than four effective influences before finalization;
- left/right bone mixture on the same vertex, using configurable naming conventions;
- bone influence is suspiciously distant from its segment.

### Transition diagnostics

- compare adjacent vertices' weight distributions;
- highlight large discontinuities;
- allow the user to focus analysis on a selected region and selected allowed bones;
- avoid labeling rigid-core boundaries as errors without user context.

The discontinuity metric and default threshold are hypotheses requiring tests against the rat's
neck and at least one ordinary humanoid mesh.

### Pose stress test — later phase

Proposed flow:

1. choose one bone;
2. choose one or more test axes and an angle range;
3. preview positive and negative rotation;
4. color vertices/triangles by estimated stretch or deformation severity;
5. restore the unchanged bind pose when the preview closes.

Before the shared runtime foundation exists, this is an editor-only diagnostic and must not be
described as runtime skeletal animation. Once that foundation exists, the editor should use the
canonical LBS/DQS deformation path rather than maintain a second approximation. In either case the
bind pose must be restored unchanged when the preview closes.

## 13. Use-Case Acceptance Scenarios

### Initial rat fixture set

The following files under `src/test-lib/` form the initial versioned study set:

| File | Initial role |
|---|---|
| `T-BONE-rato-from-mixamo.fbx` | Mixamo reference containing the skinned rat, humanoid hierarchy, skin clusters, and an animation stack; external baseline for visible deformation and round-trip comparison. |
| `T-BONE-rato-from-mixamo.msh` | Mini MBM representation used to inspect which bones/weights survived conversion and exercise editor operations without requiring a new import for every test. |
| `T-BONE-rato-from-mixamo_armature.lua` | Independently loadable 41-bone armature reference for hierarchy, bind placement, remove/reload, and bone-editing tests. |
| `Image_0.jpg`, `Image_1.jpg`, `Image_3.jpg`, `normal.png` | Material textures referenced by the FBX; keep visual/material comparisons reproducible and prevent missing-texture noise from being confused with deformation errors. |

This is a baseline asset bundle, not yet a complete automated fixture. Before Phase 1 exits, record:

- the exact FBX animation/clip name and representative timestamps;
- screenshots or video of the unedited neck and cavity failures;
- the expected material-to-texture mapping after FBX and `.msh` loading;
- whether `.msh` weights match the FBX clusters closely enough for before/after comparison;
- hashes or another immutable fixture identity if these files will be replaced during experiments;
- which generated exports are disposable results and must not overwrite this baseline.

### Alien-rat neck

Given imported Mixamo weights and the `Spine2`, `Neck`, and `Head` bones, the user can select the
neck region, restrict smoothing to those bones, preview the affected vertices, apply smoothing, and
export without changing weights on the arms, face extremities, abdomen, or legs.

Success is measured externally using at least:

- head turn left/right;
- head tilt;
- torso bend combined with head rotation;
- visual absence or meaningful reduction of face separation/tearing.

### Hollow abdominal cavity

The user can select all cavity-wall vertices, rigid-bind them to one torso bone, optionally apply a
transition shell outside the cavity, and export while the rectangular interior retains its shape
during torso animation.

The cavity's rigid faces must not be smoothed merely because falloff is enabled.

The first Mixamo cavity test validates the central Phase-1/Phase-2 behavior: after an AABB Rigid
Bind, the hollow rectangular abdominal core becomes visibly rigid during animation. Two boundary
tests also establish the current interaction limit. With zero transition width, the hard weight
boundary produces visible separation along lateral faces. With an overly broad uniform transition,
the target influence reaches unrelated upper-body geometry and can deform the chin. These are the
expected opposite extremes of the current algorithm: the rigid core works, while authoring the
flexible boundary precisely is still difficult.

This milestone is therefore accepted as functional. The next cavity-focused work is selective
transition authoring, not a replacement of Rigid Bind. The preferred investigation order is:

1. independently adjustable transition widths for `-X`, `+X`, `-Y`, `+Y`, `-Z`, and `+Z`;
2. per-face enable/disable controls so only chosen AABB faces emit falloff;
3. an exclusion/protected volume whose vertices cannot be modified by the operation;
4. topology-ring expansion as an alternative to world-space distance, with disconnected-island
   protection.

Version 6.50.0 implements that first refinement. Each of the six faces (`-X`, `+X`, `-Y`, `+Y`,
`-Z`, `+Z`) now has independent enablement and width. A vertex outside a disabled/zero-width face
is excluded from the shell; at edges and corners, every crossed face must permit the vertex and the
largest normalized face distance controls the existing Linear/Smooth falloff. The cyan AABB remains
the unchanged rigid core, while the orange preview box shows the asymmetric outer limit. A
protected volume and topology rings remain follow-up options if per-face control is insufficient
on the rat mesh. Mixamo validation approved the refinement: the cavity remained rigid, the lateral
break diminished, and the transition no longer reached the chin when its upward face was constrained.
A horizontal break elsewhere on the twisting torso remains visible and is not attributed to the
rigid box without a selection-boundary diagnostic.

### Tail

The user can add a parented tail chain manually and assign progressively changing weights along the
tail using region operations. Export must preserve the custom bones and weights.

Mixamo animations are not expected to animate custom tail bones. Tail animation/physics is a
separate future concern.

## 14. Phased Delivery

### Implemented first slice (6.45.0)

`editor/realtime_skinning_editor.lua` establishes the standalone editor and its first Skin Weight
Lab workspace. It currently provides:

- `.msh` load, save, and save-as;
- cached explicit analysis using direct vertex-in-AABB, material subset, or nearest-bone proximity;
- visual AABB and capped selected-vertex markers;
- selected-region counts for missing weights, non-normalized sums, and unknown bone references;
- Rigid Bind to one selected bone;
- one-level rollback backed by a complete temporary mesh snapshot;
- English and Brazilian Portuguese UI.

This is not the complete Phase 1. Selection is direct-vertex based rather than the planned face
inclusion modes, and LBS/DQS deformation preview is not present. Apply is intentionally limited to
frame-1 weights, matching the persisted weight section.

The 6.45.1 interaction pass adds direct viewport dragging for the complete AABB while preserving
explicit Min/Max controls for each axis. Numeric drag sensitivity is derived from the loaded mesh
bounds, but is exposed as an editor-local value so the user can trade precision for speed when the
automatic result is unsuitable. Changing sensitivity alone neither moves the AABB nor invalidates
its cached analysis; the Auto action restores the bounds-derived value. Size X/Y/Z
controls provide symmetric per-axis resizing around the current center by moving the negative and
positive faces together. The editor panel is resizable, and a separate 3D camera panel exposes
orbit, position, focus, reset, WASD horizontal movement, Page Up/Down elevation, and wheel zoom.
Direct per-face viewport resizing remains a later gizmo enhancement; Min/Max and symmetric Size
fields are the current precise sizing mechanisms.

The 6.46.0 Phase-2 slice adds an adjustable outer transition shell around the AABB rigid core.
Core vertices receive weight `1.0` from the target bone; shell vertices blend that target into
their existing frame-1 influences using Linear or Smooth falloff. The result is sorted, limited to
four influences, and normalized. Vertices outside the orange outer boundary remain unchanged.
The preview distinguishes the red rigid core from orange transition vertices, reports both counts,
and the existing one-level snapshot restores the complete pre-operation weight state.

This first Phase-2 slice deliberately uses a uniform AABB shell and geometric distance. It does not
yet provide topology/adjacency smoothing, a weight heat map, per-face inclusion, or LBS/DQS posed
deformation preview; these remain later milestones.

The initial Phase-3 heatmap now uses six conventional cold-to-hot bands over the analyzed region,
from blue (`0`) through cyan, green, yellow, and orange to red (`1`). Its analysis bone is
independent from the rigid-bind target bone. An optional allowed-bone list
also diagnoses influence references outside that list and filters them from transition-shell
results during Apply; the target bone is always retained. Filtered results continue to be limited
to four influences and normalized. The operation remains protected by the existing snapshot
rollback. Topology/adjacency smoothing and posed LBS/DQS preview are still future work.

The 6.48.0 Phase-3 continuation adds configurable local topology smoothing. For `TRIANGLES`
meshes, each iteration reads a stable weight snapshot, averages one-ring triangle neighbors, and
mixes that average by the selected strength before limiting to four normalized influences. When an
AABB transition shell exists, only the shell is editable and the rigid core remains unchanged;
otherwise the complete analyzed selection is editable. Neighbors outside the selection may guide
the boundary, but their own weights are never written. Allowed-bone filtering, one-level rollback,
and outside-region preservation remain active.

This implementation follows index connectivity inside each subset. It deliberately does not cross
subset boundaries or geometric seams represented by duplicated vertices; a later welded-position
adjacency option may address those cases. Posed LBS/DQS preview remains future work.

The 6.49.0 Phase-3 diagnostics slice compares each selected triangle edge using half the L1
distance between its two complete weight vectors. This produces a normalized difference in
`[0,1]`: `0` means identical influences and `1` means fully disjoint weight assignments. An
adjustable threshold reports abrupt-edge count, unique affected-vertex count, and maximum observed
difference; affected vertices receive magenta always-on-top markers. The diagnostic reads stored
weights without allowed-bone filtering and never modifies the mesh. It shares a cached per-mesh
adjacency with smoothing, so repeated operations do not rebuild triangle connectivity. Analysis
and abrupt-transition overlays have independent visibility controls; each control remains visibly
disabled until its corresponding marker set exists.

The follow-up targeted repair stores the diagnostic's actual affected-vertex set rather than only
its counts. **Smooth Detected Transitions** edits exactly those magenta vertices, while their
one-ring triangle neighbors participate as read-only averaging inputs. It reuses the existing
Strength and Iterations controls, allowed-bone filtering, four-influence normalization, and
one-level rollback snapshot. After the write, selection analysis and abrupt-transition diagnosis
run again automatically, reporting before/after abrupt-edge and affected-vertex counts and
refreshing the magenta overlay from the new weights.

The first explicit targeted-repair validation used threshold `0.35`, Strength `0.15`, and one
iteration on the rat neck. The diagnosis selected 396 magenta vertices from 372 abrupt edges;
all 396 were smoothed, none were skipped, and automatic re-diagnosis reduced abrupt edges from
`372` to `333` and affected vertices from `396` to `358`. One-level rollback restored the exact
pre-operation result. The retained output is
`src/test-lib/T-BONE-rato-suavizado-scale-100-from-mixamo.msh`; reopening that specific saved file
remains an optional persistence confirmation, while general save/reopen persistence is already
approved separately.

The standalone **Normalize and Limit** cleanup operates on every vertex in the current explicit
analysis. It drops non-positive/non-finite weights, merges duplicate bone names, keeps the four
strongest effective influences, and normalizes their sum to `1.0` without applying the optional
allowed-bone filter. Vertices with no effective influence remain unchanged and are reported as
skipped rather than receiving an invented bone. The complete operation is protected by the same
one-level rollback snapshot.

The operation now performs a preflight comparison and writes only vertices that actually require
cleanup; already-valid vertices are not rewritten. Its report remains visible directly below the
button and separates analyzed, corrected, already valid, skipped-without-influence, and failed
counts. This makes a healthy Mixamo mesh an explicit no-op rather than misleadingly reporting every
selected vertex as normalized. The controlled invalid-weight fixture initially reports three
non-normalized sums. Cleanup corrects 182 vertices: the three deliberately invalid sums plus 179
pre-existing named slots carrying weight zero. A second analysis reports zero non-normalized
vertices, and a second cleanup reports 0 corrected / 36,149 already valid, confirming idempotence.

The editor panel now presents the workflow as three numbered, visually separated blocks:
**1. Visualization**, **2. Selection and Analysis**, and **3. Operation**. The Operation block
starts with an explicit action selector and displays only the controls relevant to Inspect
Transitions, Rigid Bind, Normalize and Limit, Smooth Selection, or Repair Detected Transitions.
The heatmap bone therefore remains an inspection input in block 2 and is shown only while the
weight heatmap is enabled, while the single target bone exists only for Rigid Bind. Bone-proximity
selection has its own explicitly named selection bone: it defines the geometric segment/radius and
can be highlighted with an orange joint marker. Its selection radius is editor-local, initialized
to 10% of the largest mesh extent, and does not modify the radius persisted in the skeleton. An
orange wireframe capsule previews the exact point-to-segment distance envelope. By default every
vertex inside this capsule is eligible; an optional nearest-segment filter restricts ownership to
vertices for which the selected bone segment is nearer than every other segment. The heatmap bone independently chooses which
stored influence the colors inspect and can be highlighted with its yellow marker. Leaving the
proximity method clears its orange highlight; disabling the heatmap hides its bone input and clears
its yellow highlight. Smoothing and transition repair use the optional set of allowed bones;
they never interpret the rigid-bind target as an implicit smoothing destination.

If allowed-bone filtering leaves a vertex without any effective influence during smoothing, that
vertex remains unchanged and is counted as skipped. Assigning it silently to a target bone would
turn a neighborhood-average operation into a hidden rigid bind and could introduce precisely the
neck discontinuity the operation is meant to reduce.

Changing the allowed-bone set is an operation parameter and does not invalidate the analyzed
geometric selection. The editor refreshes the disallowed-influence count in place. Abrupt-transition
diagnostics also remain valid because they intentionally inspect the complete stored weight vectors,
independently of the operation's allowed-bone filter. The restriction list provides Allow All and Clear All
actions, plus an optional viewport highlight: checked bones appear in cyan and the hovered list
item temporarily appears in orange. Restriction highlights are cleared whenever the selected
operation changes, so operation-specific visualization cannot leak into another workflow.

The first recorded neck test used the rat's head base, short neck, and a small upper-torso region,
with Strength `0.25`, one iteration, and allowed influences selected by inspecting each plausible
bone's heatmap. Spine1, Spine2, Neck, Head, LeftShoulder, and RightShoulder were retained because
the shoulder bones also showed meaningful yellow/red influence inside the AABB. Abrupt edges fell
from `461` to `372`, affected vertices from `479` to `405`, and maximum difference from `1.000` to
`0.870`. The generated neck-test `.msh` and `.fbx` were intentionally removed after the experiment;
the measurements remain historical evidence rather than permanent fixtures. Blender inspection of
the temporary FBX confirmed the same 41-bone armature, 36,149 vertices, and 51,794
polygons as the source, with zero unweighted vertices, zero non-normalized vertices, and no vertex
above four effective influences. This is a successful integrity/continuity result, not yet proof
of a finished neck correction. Mixamo comparison showed less tearing during head rotation, but
including arm influences introduced visible stretching; the experiment was closed without adopting
a final corrected neck asset.

Like smoothing, the diagnostic is limited to `TRIANGLES`, index connectivity within each subset,
and edges whose two endpoints belong to the analyzed selection. It detects weight discontinuity,
not actual posed deformation; pose stress remains a separate LBS/DQS milestone.

### Current validation status (2026-08-09)

The following table distinguishes implemented code from behavior actually exercised by the user.
“Approved” means the observed result matches the current milestone; it does not imply that every
future refinement or malformed-input branch has been tested.

| Area | Status | Evidence / remaining qualification |
|---|---|---|
| Load, Save As, close/reopen persistence | **Approved** | Edited weights and skeleton survive reopening. |
| One-level rollback | **Approved** | Weight edits restore the preceding snapshot. |
| AABB placement, symmetric sizing, dragging, and 100× camera interaction | **Approved** | Used repeatedly on the rat for neck and cavity selection. |
| Bone-proximity selection | **Approved** | Retest confirmed the scale-aware independent radius, orange capsule, bone switching/cache invalidation, and optional nearest-segment filter behave coherently on the 100× rat. |
| Analysis-bone heatmap | **Approved** | Spine1, Spine2, Neck, Head, shoulders, and arms were inspected independently. |
| Allowed-bone restriction workflow | **Approved for smoothing** | Analysis remains valid while choosing allowed bones; the list controlled the neck experiments. |
| Allowed-bone visualization | **Approved** | Persistent cyan selection, temporary orange hover, Allow All/Clear All, and operation-change cleanup were confirmed. Changing operation disables the highlight and clears viewport colors while intentionally preserving the allowed-bone selection. |
| Abrupt-transition diagnosis | **Approved** | Neck baseline `461 / 479 / 1.000`; after smoothing `372 / 405 / 0.870`. |
| Smooth Detected Transitions | **Approved** | At threshold `0.35`, Strength `0.15`, one iteration: 396/396 magenta vertices written, 0 skipped, abrupt edges `372 → 333`, affected vertices `396 → 358`, and rollback confirmed. |
| Local smoothing of a complete selection | **Partially validated** | Reduced the neck tear, but including arm influences introduced stretching. The experiment was stopped without accepting a final neck asset. |
| Rigid Bind | **Approved in Mixamo** | The hollow abdominal core remained visibly rigid during body animation. |
| Uniform transition shell | **Superseded** | Zero width broke lateral faces; excessive uniform width reached the chin. This motivated per-face control. |
| Six independent AABB transition faces | **Approved in Mixamo for the cavity** | Prevented upward reach into the chin and reduced the lateral break while preserving the rigid core. A separate horizontal torso break remains. |
| Per-face transition edge cases | **Approved** | Confirmed one enabled face, unequal opposite widths, all faces disabled, enabled width zero, a permitted two-face corner, a corner blocked by a disabled crossed face, and analysis invalidation after edits. |
| Material-subset selection | **Approved only for a single-subset mesh** | The rat's sole frame-1 subset selected all 36,149 vertices; multi-subset isolation is not yet tested. |
| Normalize and Limit | **Approved** | Controlled fixture: 182 first-pass cleanups (`3` invalid sums + `179` named zero weights), then `0` corrected / `36,149` already valid on the second pass. |
| Normalize report | **Approved** | Local analyzed/corrected/already-valid/skipped/failed counts were visible and consistent with the audit. |
| FBX export after weight editing | **Approved structurally and in Mixamo** | 41 bones, 36,149 vertices, 51,794 polygons, no unweighted/non-normalized vertices, and at most four effective influences in the inspected export. |

The following behavior is implemented but still needs an explicit validation pass:

1. **Multi-subset selection:** use a mesh with at least two material subsets and prove isolation.
2. **Normalize exceptional branches:** test a vertex with no effective influence and a controlled
   read/write failure if a safe fixture can represent one; ordinary and idempotent paths are done.
3. **Original-scale interaction regression:** repeat camera, numeric drag, AABB picking, markers,
   and skeleton alignment on the unscaled rat rather than only the 100× working fixture.
4. **Meshes without bones or without stored weights:** confirm useful diagnostics, disabled actions,
   and absence of crashes.

The following items remain future milestones rather than missing tests of current behavior:

- selection-boundary diagnosis comparing internal vertices with immediate external neighbors;
- protected/exclusion volumes and topology-ring transition expansion;
- welded/coincident-vertex adjacency across UV or subset seams;
- tail-chain weighting and exported custom-tail preservation;
- pose-stress preview and real LBS/DQS deformation preview/runtime skinning.

### Phase 0 — Test assets and baseline

- Preserve the initial `src/test-lib/T-BONE-rato-from-mixamo.*` bundle and its four textures as the
  first versioned baseline. Keep the FBX, `.msh`, and armature Lua roles distinct.
- Confirm and record the animation stack/clip and problematic timestamps present in the FBX.
- Record screenshots/video and the exact problem regions before editing.
- Record whether the neck geometry is welded or consists of disconnected/duplicate surfaces.
- Identify which torso bone should own the rigid cavity.

Exit criterion: the deformation failures can be reproduced consistently.

### Phase 1 — Standalone editor shell, region selection, and rigid correction

- Standalone Real-Time Skinning Editor shell with a Skin Weight Lab workspace.
- Load/save integration that preserves the existing mesh, skeleton, and weight data.
- Reuse or extract proven Mesh Debug camera, bone visualization, and selection behaviors without
  duplicating the entire Mesh Debug implementation.
- AABB selection preview without topology mutation.
- Subset and bone-proximity selection where practical.
- Rigid Bind to one bone.
- Normalize/limit validation.
- Explicit Analyze, Apply, Cancel, and one-level Revert.
- Selected/affected counts and warnings.

Primary validation: abdominal cavity.

### Phase 2 — Transition shell and local blending

- Inner rigid core plus adjustable outer shell.
- Linear and Smooth falloff.
- Blend a selected bone into existing weights.
- Preserve all weights outside the outer boundary.
- Refine the currently uniform shell with per-face enablement and independent face widths, based on
  the validated cavity test. **Implemented in 6.50.0 and approved in the cavity Mixamo test.** Evaluate
  protected exclusion volumes and topology rings afterward.

Primary validation: cavity-to-abdomen boundary and tail segments.

### Phase 3 — Local smoothing and diagnostics

- Allowed-bone restriction.
- Configurable local smoothing.
- Integrity diagnostics.
- Adjacency-based transition heat map.

Primary validation: alien-rat neck.

### Phase 4 — Pose stress preview

- Controlled temporary bone rotations.
- Visual deformation/stress heat map.
- Guaranteed restoration of the unchanged bind pose.

Primary validation: neck turns and torso/head combinations.

### Phase 5 — Heavy regeneration investigation

- Evaluate full-mesh and selected-region algorithms.
- Re-evaluate Blender Automatic Weights failures on disconnected/non-manifold content.
- Compare envelope, heat-map, voxel/geodesic, and other feasible strategies using real fixtures.
- Decide whether heavy generation belongs inside the standalone editor, in the Blender bridge, or
  remains an external DCC workflow.

This phase is research, not a promised feature.

### Companion runtime milestones — LBS and DQS

Runtime deformation, clip playback, and backend shader delivery are tracked in
[Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md). Skin Weight Lab phases may
start before runtime skinning, but the pose-stress preview should converge on that shared path as
soon as it is trustworthy. Bind-pose validation, influence limits, method selection, and diagnostic
fixtures must therefore be designed as shared contracts rather than editor-only assumptions.

## 15. Safety and Non-Functional Requirements

- Weight analysis must be cached and invalidated when geometry, skeleton, selection, or relevant
  operation parameters change.
- Opening the panel must not trigger full-mesh work every frame.
- Large meshes must remain interactively navigable while positioning the selection volume.
- Apply must be disabled when its preview is stale.
- A weight edit must mark the mesh modified and invalidate cached weight statistics and previews.
- Operations must never silently leave a vertex unweighted.
- Unknown bone references must be reported before export or deliberately repaired by the user.
- All generated results must use at most four normalized influences per vertex.
- The active preview method (LBS or DQS) and backend limitations must be explicit.
- The editor must never silently replace DQS with LBS, or LBS with DQS, because a backend limit was
  reached. It may offer an explained fallback that the user explicitly accepts.
- Undo scope must be stated accurately. The initial requirement is one-level rollback for the last
  weight operation, not a general editor-wide history.
- Canceling an analysis or closing the tool before Apply must leave mesh data unchanged.
- English and Brazilian Portuguese UI text must be added together.
- Documentation and `MBM_VERSION` must be updated when a phase ships.

## 16. Dependencies and Constraints

- Frame-1 vertex topology is the authoritative indexing space for persisted weights.
- UV seams and hard normals may create multiple stored vertex entries at the same position.
- Current Mesh Debug access supports reading and writing weights per vertex, but does not expose a
  ready-made high-level smoothing or adjacency service.
- Split Capture logic is a behavioral reference, but its apply path changes geometry and therefore
  must not be reused for weight application.
- A reliable pose preview may require bind/inverse-bind calculations that Mini MBM does not
  currently perform.
- Basic DQS represents rigid rotation and translation; non-uniform scale and shear require a
  documented fallback or a later two-phase extension. The editor must not silently discard them.
- DQS blending requires quaternion antipodality correction before accumulation and normalization
  afterward; otherwise equivalent rotations can cancel and produce unstable deformation.
- OpenGL ES 2 has a tighter guaranteed vertex-uniform budget than the other current backends. Bone
  palette limits and capability reporting must be measured rather than inferred from desktop Linux.
- The standalone editor should consume shared skeleton, weight, pose, and deformation services. A
  large copy of Mesh Debug logic would create two implementations that drift.
- Mixamo behavior and generated weights are external and may vary between uploads.
- Automatic heat-map weighting has previously failed on combined meshes containing disconnected,
  open, or thin surfaces. Heavy regeneration must not assume it is a universal solution.

## 17. Risks

| Risk | Impact | Mitigation direction |
|---|---|---|
| Volume selects hidden or unrelated geometry | Wrong vertices receive weights | Preview, island filter, selection modes, explicit Apply |
| Smoothing contaminates a region with unrelated bones | New deformations appear | Allowed-bone list and locked influences |
| Falloff softens the cavity itself | Rectangular cavity bends | Separate rigid core from external transition shell |
| UV-seam duplicates receive inconsistent weights | Visible cracks along seams | Decide and test coincident-vertex grouping |
| Full regeneration destroys good Mixamo weights | Whole-character regression | Keep it out of early phases; snapshot and explicit warning later |
| Diagnostic threshold produces false positives | User loses trust in heat map | Adjustable thresholds and “suspicious,” not “wrong,” language |
| Pose preview becomes an incomplete animation subsystem | Excess complexity and misleading results | Phase-gate it, validate math separately, then converge on the shared runtime path |
| Standalone editor duplicates Mesh Debug internals | Fixes and behavior drift between tools | Extract shared services and migrate incrementally |
| Quaternion signs are blended without antipodality correction | DQS collapses or flips unpredictably | Canonical reference sign, correction fixture, normalization checks |
| DQS silently ignores non-uniform scale | Preview differs from authored animation | Detect scale/shear and expose fallback or unsupported state |
| GLES palette limits differ by device | Works on one platform and fails on another | Capability report, explicit palette limit, partitioning investigation |
| Custom tail bones appear static under Mixamo clips | User expects automatic tail motion | State scope clearly; animation/physics remains separate |
| Combined/non-manifold meshes defeat automatic algorithms | Heavy calculation produces unusable output | Local tools first; compare algorithms against real fixtures |

## 18. Decisions Taken

1. The work will be documented and delivered incrementally rather than implemented as one global
   “Regenerate All” button.
2. Existing imported weights should be preserved outside an explicit user selection.
3. The Split Capture box interaction is the preferred starting point for spatial selection, but
   weight editing must not cut or otherwise mutate topology.
4. Rigid-core weighting and transition smoothing are separate behaviors.
5. Manual tail-bone creation is already acceptable; tail weighting belongs in this tool, while tail
   animation does not.
6. User validation with the alien rat is part of each phase's completion criteria.
7. Skin Weight Lab will be planned as a workspace in a standalone Real-Time Skinning Editor, not as
   another permanent expansion of Mesh Debug.
8. The data and preview contracts will accommodate LBS and DQS from the initial design, even though
   the methods may ship in different milestones.
9. The existing articulated-animation editor is the interaction and domain-language reference for
   clips, hierarchy, pivots, tracks, timeline, playback, and composition. Its part-animation format
   will not be treated as if it were skeletal skinning data.

## 19. Hypotheses to Validate

1. Face-derived AABB selection is precise enough for weight editing without a dedicated vertex
   selection mode.
2. **Rejected by the first cavity test:** one uniform outer falloff shell is not sufficiently
   controllable. Zero width creates a hard lateral break, while a width large enough to soften that
   boundary can reach unrelated geometry such as the chin. Per-face control is the next hypothesis.
3. Adjacency smoothing restricted to `Spine2`, `Neck`, and `Head` will materially improve the rat's
   neck deformation.
4. A simple weight-discontinuity metric correlates with visible tearing.
5. Coincident stored vertices should normally be edited as one logical position group.
6. One-level rollback is sufficient for the first delivery.
7. The cavity can be owned rigidly by one existing torso bone rather than needing a dedicated bone.
8. DQS may be a useful preferred preview/runtime method on constrained GLES2 devices because a
   rigid dual-quaternion palette uses fewer vertex-uniform vectors per bone than common matrix
   palettes. This remains conditional on measured limits, reserved uniforms, and scale semantics.
9. Linux/GLES can be the first implementation and feedback platform while the shared contracts stay
   backend-neutral; Metal and DirectX validation should occur at explicit milestones rather than
   only after a Linux-specific design reaches a dead end.
10. A standalone editor can reuse enough shared Mesh Debug services to avoid duplicating its large
    Lua codebase.

## 20. Open Questions

These questions do not block saving this discovery document. They must be answered before their
respective phase is implemented.

### Before Phase 1

1. Which FBX animation/clip and timestamps will be the canonical deformation cases?
2. Is the cavity already a separate material subset, or must AABB selection isolate it?
3. Which bone should own the cavity: `Spine`, `Spine1`, `Spine2`, or a new dedicated bone?
4. Should coincident vertices at UV/normal seams always be selected together?
5. Should the first version edit only the currently selected mesh, with no Apply All equivalent?
6. What is the final editor name and which existing Mesh Debug responsibilities migrate into it?
7. Which functions become shared services, and which remain intentionally owned by Mesh Debug?

### Before Phase 2

8. Should falloff be defined by an absolute world-space thickness, a percentage of box size, or
   both?
9. Should the transition shell grow only outward, or should the user be able to define separate
   inner and outer volumes?
10. For Blend Bone, should the main input mean target weight, additive strength, or interpolation
   percentage?

### Before Phase 3

11. How will the user choose allowed bones: multi-select list, selected bone plus parent/children, or
   a named preset?
12. Should smoothing keep the outer selection boundary fixed by default?
13. What diagnostic threshold is useful on the rat without overwhelming the display?

### Before Phase 4

14. At what milestone must the editor preview stop using diagnostic-only deformation and use the
    canonical runtime LBS/DQS path?
15. Should LBS/DQS comparison be side-by-side, a toggle using one camera/pose, or both?

## 21. User Test Protocol per Phase

Each test round should record:

- source mesh and source weights;
- selected region and selection mode;
- selected target/allowed bones;
- operation parameters;
- before/after affected vertex counts;
- screenshots of the editor preview;
- exported FBX result;
- animation and pose used for validation;
- observed improvement, regression, or ambiguity;
- decision: accept, adjust defaults, redesign, or revert.

The document version should be incremented when testing changes requirements or phase boundaries.

## 22. Out of Scope for the Initial Delivery

- Shipping runtime skeletal animation in the initial Skin Weight Lab delivery. Its future design and
  integration are explicitly in scope of the companion plan.
- Automatic animation of custom tail bones from standard Mixamo clips.
- General-purpose Blender-style weight painting with brush input.
- A full multi-level editor undo/redo system.
- Guaranteed anatomically correct automatic weights for arbitrary meshes.
- Modifying Mixamo's service, marker interpretation, or animation library.
- Animation of the sphere intended to float inside the cavity.

## 23. Handoff Readiness

Phase 1 can move to technical design after the Phase-1 questions are answered and the canonical rat
fixture is reproducible. The handoff must also define the boundary between the standalone editor and
shared Mesh Debug/engine services. Later phases remain discovery items until their preceding phase
has been tested by the user.

The technical handoff for each phase should identify the smallest reusable selection/preview pieces,
the weight-data snapshot boundary, cache invalidation triggers, and an executable verification plan.
Those implementation decisions are intentionally not prescribed by this discovery document.

## 24. Change Log

| Version | Date | Change |
|---|---|---|
| 0.26 | 2026-08-09 | Approved all six AABB transition-face edge cases: one-sided and asymmetric widths, all-disabled and zero-width behavior, permitted two-face corners, disabled crossed-face blocking, and analysis invalidation after edits. |
| 0.25 | 2026-08-09 | Approved allowed-bone visualization: persistent cyan selection, orange hover, list actions, and visual cleanup when changing operations; clarified that the allowed-bone selection itself is intentionally preserved. |
| 0.24 | 2026-08-09 | Exposed the bounds-derived AABB numeric-drag sensitivity as an editable value, with a compact Auto reset; changing sensitivity alone does not alter selection geometry or invalidate analysis. |
| 0.23 | 2026-08-08 | Approved bone-proximity selection after the scale-aware radius/capsule retest confirmed coherent selection, bone-change invalidation, and optional nearest-segment filtering on the 100× rat. |
| 0.22 | 2026-08-08 | Recorded the inconclusive one-vertex Neck proximity test, replaced the stored bone radius with a scale-aware editor-local selection radius, added an exact orange capsule preview, and made nearest-segment ownership an optional default-off filter. Functional retesting remains pending. |
| 0.21 | 2026-08-08 | Disambiguated the proximity-selection bone from the heatmap-inspection bone; added an orange proximity-joint highlight, made the heatmap bone conditional on the heatmap checkbox, and recorded highlight cleanup when either context is left. Functional proximity validation remains pending. |
| 0.20 | 2026-08-08 | Approved Smooth Detected Transitions with the 396-vertex targeted neck test, automatic `372 → 333` edge / `396 → 358` vertex re-diagnosis, zero skipped vertices, confirmed rollback, and a retained smoothed `.msh` result. |
| 0.19 | 2026-08-08 | Consolidated approved, partial, pending, and future validation status; approved per-face cavity transition and Normalize idempotence; recorded the 182-cleanup breakdown; corrected removed neck fixtures and listed the remaining editor test matrix. |
| 0.18 | 2026-08-08 | Added a persistent local Normalize and Limit report with analyzed/corrected/already-valid/skipped/failed counts, and changed cleanup to avoid rewriting vertices that are already valid. |
| 0.17 | 2026-08-08 | Implemented independent enablement and width for all six AABB transition faces, asymmetric outer preview, crossed-face blocking, and normalized edge/corner falloff; retained protected volumes and topology rings as future work pending Mixamo validation. |
| 0.16 | 2026-08-08 | Accepted the abdominal rigid-core Mixamo milestone; recorded hard-boundary lateral separation and over-broad-transition chin deformation; prioritized selective per-face transition widths/enables, with protected volumes and topology-ring expansion as follow-ups. |
| 0.15 | 2026-08-08 | Recorded the first neck-smoothing result and exported-FBX integrity inspection; added allowed-bone guidance based on per-bone heatmap inspection and clarified that AABB selects vertices rather than joint positions. |
| 0.14 | 2026-08-08 | Kept geometric analysis and raw-weight transition diagnostics valid while editing allowed bones, added in-place disallowed-count refresh, Allow All/Clear All actions, persistent cyan allowed-bone highlighting, temporary orange hover highlighting, and operation-change highlight cleanup. |
| 0.13 | 2026-08-08 | Reorganized the editor into numbered Visualization, Selection and Analysis, and Operation blocks with colored titles and contextual action controls; made the rigid target exclusive to Rigid Bind; removed smoothing's implicit rigid-target fallback and added skipped-vertex reporting. |
| 0.12 | 2026-08-07 | Added standalone analyzed-selection Normalize and Limit with invalid-weight cleanup, duplicate merging, strongest-four normalization, skipped-unweighted reporting, and rollback; added symmetric center-preserving Size X/Y/Z controls for the AABB. |
| 0.11 | 2026-08-07 | Added targeted smoothing of diagnosed magenta vertices with shared strength/iteration controls, allowed-bone filtering, rollback, automatic re-analysis/re-diagnosis, and before/after reporting; corrected the heatmap record to six bands and an independent analysis bone. |
| 0.10 | 2026-08-07 | Added independent, default-enabled analysis/abrupt overlay visibility controls that stay disabled until their marker data exists, plus clearer separation before rigid apply. |
| 0.9 | 2026-08-06 | Added abrupt-transition diagnostics using normalized half-L1 edge distance, threshold/count/max reporting, magenta affected-vertex markers, and cached adjacency shared with smoothing. |
| 0.8 | 2026-08-06 | Recorded local one-ring topology smoothing with strength/iteration controls, Jacobi snapshots, rigid-core protection, allowed-bone filtering, four-weight normalization, rollback, and duplicate-seam limitations. |
| 0.7 | 2026-08-06 | Recorded the first Phase-3 slice: target-bone weight heatmap, optional allowed-bone restriction, disallowed-reference diagnostics, and restriction-aware normalized blending. |
| 0.6 | 2026-08-06 | Recorded the first Phase-2 slice: adjustable AABB transition shell, Linear/Smooth falloff, normalized four-influence blending, separate core/shell preview, outside-region preservation, and rollback verification on the 100× rat fixture. |
| 0.5 | 2026-08-06 | Recorded the resizable layout, compact selection combo, scale-aware AABB Min/Max controls, direct AABB viewport dragging, and orbit/position/focus camera controls. |
| 0.4 | 2026-08-06 | Recorded the first Phase-1 implementation slice: standalone editor, three cached selection modes, integrity counts, visual markers, rigid binding, save flow, and snapshot rollback, with remaining Phase-1 gaps stated explicitly. |
| 0.3 | 2026-08-06 | Registered the initial versioned rat fixture bundle under `src/test-lib/`, assigned each file a study role, and added baseline characterization requirements. |
| 0.2 | 2026-08-06 | Reframed Skin Weight Lab as a workspace in a standalone Real-Time Skinning Editor; linked the LBS/DQS runtime plan; added bind/preview, antipodality, scale, backend-capability, articulated-animation reference, and migration decisions. |
| 0.1 | 2026-08-05 | Initial discovery: region selection, rigid core/falloff, local smoothing, diagnostics, pose stress preview, tail scope, phased delivery, and user-test gates. |
