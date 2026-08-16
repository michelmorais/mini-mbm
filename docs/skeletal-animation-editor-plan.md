# Skeletal Animation Editor — Product and Migration Plan

Document version: **7.99**
Status: **Five active skeletal workflows implemented; Paint Weights visual foundation started; composition deferred**
Last updated: **2026-08-16**

## 1. Purpose

This document plans the evolution of the standalone **Skeletal Animation Editor** from its delivered
Skin Weight Lab into a sufficient local skeletal-animation tool. The objective is not to replace
Blender, Mixamo, or other DCC tools. Mini MBM should be able to:

1. inspect and repair imported skeletal assets;
2. create or adjust a skeleton locally when an external tool is unnecessary;
3. create and edit local skeletal animation clips;
4. import complete external skeleton/weight/animation content where practical;
5. preview the same LBS/DQS deformation path that the runtime will execute.

The implemented weight-authoring workflow is documented in the
[Skeletal Animation Editor guide](skeletal-animation-editor.md). Runtime deformation, bind-pose math,
backend delivery, and LBS/DQS correctness remain planned in the
[Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md).

## 2. Product shape: six exclusive worktrees

The editor is the product container. Its primary navigation contains six mutually exclusive
top-level worktrees; opening one closes the previous worktree:

1. **Bone Editor** — the ordinary-user surface for constructing and manipulating bind bones as
   head/tail joint pairs, with mouse-first interaction and only essential controls. Internally it
   maps to canonical joint TRS plus an explicit bone-local tail offset rather than introducing a
   second skeleton.
2. **Bind Pose Contract** — the advanced diagnostic surface for canonical hierarchy,
   local/global/inverse-bind data, and deterministic structural/numeric findings.
3. **Runtime Skeletal Preview** — select and play canonical clips through the actual runtime,
   inspect backend/method readiness, and compare synchronized LBS/DQS instances.
4. **Skin Weight Lab** — select vertices; inspect, normalize, rigid-bind, smooth, diagnose, and
   preserve stored weights. This workflow is already delivered and must be moved into a node without
   changing its accepted behavior.
5. **Create / Edit Animations** — active local authoring for clips, bone tracks, keys, easing,
   viewport TRS manipulation, playback, timeline editing, transactional clipboards, and Undo/Redo.
   Multi-clip composition remains explicitly deferred as described in Section 8.
6. **Paint Weights** — the intended primary visual weight-authoring workflow, beginning with direct
   brush interaction and remaining state-isolated from Skin Weight Lab. After Paint/Add and
   Erase/Subtract are stable, useful Lab operations migrate here under **Weight Tools** and
   **Repair / Diagnostics**. Skin Weight Lab remains intact as a reference and fallback until the
   replacement reaches explicit functional parity.

The loaded asset, viewport, camera, status, modified state, and **Show Mesh** control are shared.
Skeleton visualization is worktree-specific: Bind Pose Contract shows the bind skeleton
automatically; Skin Weight Lab owns its visibility/depth controls; Runtime Skeletal Preview hides
the bind-only gizmo until it can draw an evaluated skeleton for every preview instance. AABB
volumes, proximity capsules, analyzed markers, heatmaps,
transition diagnostics, and weight operations exist and render only while Skin Weight Lab is open.
Node-specific history and selection state may be preserved while hidden but must not leak behavior
or viewport artifacts into another worktree.

## 3. Problem

Skin Weight Lab can improve data that future skeletal deformation will consume, but Mini MBM still
cannot evaluate a bone pose, deform the mesh, author a skeletal clip, or preserve a complete imported
animation as a runtime resource. Meanwhile, Mesh Debug's **Bones** node appears close to a skeleton
editor but actually combines several different concerns:

- hierarchy and joint editing;
- viewport gizmos and bone visualization;
- FBX/Mixamo reconstruction heuristics;
- armature templates and coordinate conversion;
- envelope-binding parameters;
- rigid vertex binding;
- destructive whole-asset transforms and armature removal.

Copying that node would make export-era assumptions part of the runtime authoring model. Ignoring it
would discard proven interaction work. The plan therefore treats Bones as an audited reference from
which selected behavior may be reused, adapted, relocated, or retired.

## 4. Users and principal workflows

### Imported complete asset

The user imports a skeleton, weights, and one or more animation clips from Blender, Mixamo, or
another supported source; validates hierarchy and bind pose; previews LBS/DQS; optionally repairs
weights or clips; and saves a runtime-ready asset without losing valid imported data.

### Imported skeleton with local animation

The user imports or applies a skeleton, validates its bind pose, then creates new clips locally using
the Animation node and the same concepts already familiar from articulated animation.

### Local skeleton and local animation

The user starts from a mesh, creates a hierarchy and bind pose, authors weights in Skin Weight Lab,
creates clips, and validates them locally. This is a required eventual capability, but follows the
first milestone that validates imported/existing skeletons.

### Mixamo round-trip

The user may still remove an existing armature, send a mesh-only T-pose to Mixamo, import the
resulting skeleton/weights/animations, and continue locally. Mixamo-specific preparation belongs to
an interchange workflow and must not define the runtime skeleton model.

## 5. Decisions taken

1. Skeletal Animation Editor has six mutually exclusive worktrees: Bone Editor, Bind Pose Contract,
   Runtime Skeletal Preview, Skin Weight Lab, Create / Edit Animations, and Paint Weights.
2. Mesh Debug Bones is a reference implementation, not a module to copy wholesale.
3. Existing, trustworthy imported bind and animation data must be preserved by default.
4. Skeleton inspection and bind validation precede unrestricted local skeleton creation.
5. Local animation authoring is a product requirement, not only a debugging aid.
6. External complete-animation import remains a first-class workflow; local tools do not replace
   Blender or Mixamo.
7. Articulated animation supplies proven vocabulary and interaction patterns, but skeletal clips
   retain explicit bind, hierarchy, and vertex-deformation semantics.
8. Editor preview and runtime must use the same pose/deformation implementation after the shared
   path exists.
9. Weight editing remains owned by Skin Weight Lab even when initiated from another node.
10. Import/export conversion and runtime authoring are distinct responsibilities in the UI.

## 6. Audit of Mesh Debug Bones

This classification is a product boundary. Reuse still requires code-level review and extraction;
it does not authorize copying the large Lua block into the new editor.

| Bones capability | Destination | Decision / qualification |
|---|---|---|
| Skeleton line/joint rendering | Shared viewport | Reuse or extract; preserve scale-aware sizing and explicit depth behavior. |
| Bone picking, highlight, and joint dragging | Skeleton / Bind Pose | Reuse interaction lessons; distinguish bind editing from animated-pose manipulation. |
| Wide bone table | Skeleton / Bind Pose | Adapt around the final bind model; do not expose storage fields merely because they exist today. |
| Name and parent editing | Skeleton / Bind Pose | Adapt with cycle checks and impact reporting for weights and clips. |
| Add root / Add child | Skeleton / Bind Pose | Adapt for local skeleton authoring after bind contracts are validated. |
| Remove / cascade remove | Skeleton / Bind Pose | Adapt with explicit affected weights, descendants, tracks, and rollback. |
| X/Y/Z joint editing | Skeleton / Bind Pose | Redesign as an explicit bind-space operation; never confuse it with pose translation. |
| Length, tail, orientation, and Roll visualization | Skeleton / Bind Pose | Preserve the useful visualization; redefine against the canonical bind representation. |
| Recompute / Recompute All | Skeleton / Bind Pose utility | Rename and redesign as an explicit derivation heuristic, previewing affected bones and preserving valid imported orientation. It is not weight, bind-pose, or animation generation. |
| Ghost mesh while editing bones | Shared viewport | Reuse the visibility concept if it materially improves bind editing. |
| Whole skeleton Rotate/Scale/Translate bake | Asset conversion/interchange | Keep outside routine skeleton/pose editing; require coordinate-space semantics and mesh synchronization. |
| Up-axis conversion | Import/export | Do not migrate as a Skeleton-node editing control. |
| Humanoid/standard armature templates | Import/template workflow | Retain as optional starting content after template provenance and bind semantics are explicit. |
| Load/export armature Lua template | Import/template workflow | Adapt only if the template format remains compatible with the canonical skeleton model. |
| Mirror paired joint positions | Skeleton authoring utility | Candidate after naming/pairing rules are explicit; never infer center/left/right destructively without preview. |
| Radius / envelope controls | Import/export fallback | Do not treat as runtime deformation behavior when real stored weights exist. |
| Rigid Bind | Skin Weight Lab | Already superseded by the delivered weight-authoring workflow; do not duplicate. |
| Remove complete armature and weights | Asset reset/interchange | Preserve as an explicit destructive workflow with snapshot/confirmation; not a normal Skeleton edit. |
| Mixamo-oriented warnings and fallbacks | Import/export diagnostics | Keep only where they describe the active interchange path; do not present them as runtime invariants. |

## 7. Skeleton / Bind Pose node

### Initial scope

- Display the hierarchy as a navigable tree and in the 3D viewport.
- Select one bone consistently from the tree, table, viewport, Skin Weight Lab, or Animation node.
- Inspect local and derived global bind transforms without modifying imported values.
- Display joint, tail/axis, orientation/roll reference, parent, children, and affected-weight summary.
- Diagnose duplicate names or identities, missing parents, cycles, invalid transforms, zero/invalid
  lengths where meaningful, unknown weight references, and unsupported scale semantics.
- Evaluate the bind-pose identity invariant once the shared pose model exists.
- Explain whether a field is imported, authored, derived, or missing.

### Safe correction scope

- Rename and reparent with an impact preview.
- Move a bind joint with an explicit policy for mesh, descendants, inverse bind, weights, and clips.
- Adjust orientation/tail/roll without silently replacing valid imported information.
- Add/remove bones with affected-reference reporting and bounded Undo/Redo history.
- Offer derivation helpers only as previewable, named heuristics.

### Viewport-interaction refinement

Numeric local-TRS fields remain the precise and scriptable bind-correction surface. After the safe
mutation contracts above are complete, add modern direct manipulation without creating a second
mutation path:

- select bones and joints directly in the viewport with unambiguous hit targets and hierarchy sync;
- provide translation, rotation, and scale gizmos with explicit local/global coordinate modes;
- support constrained axis/plane dragging, snapping, numeric feedback, and visual hover/active state;
- preview continuously during a drag, but create one rollback snapshot and one canonical commit only
  when the gesture ends;
- allow Escape/right-click cancellation to restore the exact pre-drag state;
- route the final values through the same transactional local-bind API and 41–43 validation used by
  the numeric fields;
- keep bind manipulation visually and behaviorally distinct from future animation-pose gizmos.

This refinement is not a prerequisite for proving skeleton storage and referential integrity, but it
is required before the Skeleton / Bind Pose workflow is considered ergonomically complete.

### Later local-authoring scope

- Create a complete skeleton from an unrigged mesh.
- Add chains efficiently and mirror deliberate left/right structures.
- Define or capture the bind pose.
- Hand off to Skin Weight Lab for weight creation or repair.
- Save reusable templates without confusing mesh-specific bind data with a generic humanoid layout.

## 8. Animation node

The Animation node should echo the implemented articulated-animation workflow where meanings match:

- named clips;
- bone selection and per-bone tracks;
- position, rotation, and scale channels;
- keyframes, easing, timeline seek, play/pause, loop, and speed;
- clip priority, weight/fade, and Absolute/Additive composition;
- copy/paste or duplication of poses and tracks where safe;
- visible current time, duration, and active composition state.

Skeletal-specific requirements are additional:

- the bind pose is the immutable reference for clip evaluation unless the user explicitly enters
  Skeleton / Bind Pose editing;
- authored Euler controls may exist, but runtime interpolation and storage must follow the selected
  rotation contract;
- imported tracks must preserve timing, hierarchy targets, and transform semantics;
- missing or renamed bone targets must be diagnosed, not silently discarded;
- scale channels must report LBS/DQS compatibility and any selected fallback;
- preview must deform shared mesh vertices, not move independent mesh parts.

The initial Animation milestone may target clips on an existing validated skeleton. Full local
skeleton creation does not have to block timeline and clip work if the imported-skeleton contract is
already trustworthy.

### Deferred composition and blending

Multi-clip composition remains planned but is explicitly deferred while work proceeds on the Paint
Weights workflow. The delivered editor currently authors and previews one canonical clip at a time.
Priority, layer weight, fade in/out, Absolute/Additive semantics, per-bone masks, and a composed
runtime pose are not implemented and must not be inferred from the existing clip clipboard or pose
clipboard features. Before implementation resumes, consolidate the local-TRS blend order,
shortest-path quaternion policy, additive reference pose, scale compatibility, discontinuity rules,
and the boundary between transient player-layer state and persisted clip data. The final composed
pose must still produce one backend-neutral skeleton pose followed by one LBS/DQS palette build.

## 9. Cross-node rules

1. **Single asset context.** All six worktrees operate on the same loaded mesh, skeleton, weights,
   clips, selection, and modified state.
2. **Single selected bone.** Selection remains coherent when switching nodes; node-specific
   highlights may add context without creating contradictory selections.
3. **Explicit mode boundary.** Editing the bind skeleton and posing an animation are different
   modes with visually distinct state.
4. **Referential integrity.** Rename, remove, reparent, and wholesale skeleton replacement must
   account for weight palettes, clip targets, templates, and future runtime indices.
5. **Shared history boundary.** A destructive edit must identify which skeleton, weight, and clip
   data the snapshot restores.
6. **No silent conversion.** Coordinate-system, scale, DQS fallback, missing target, and heuristic
   reconstruction decisions are visible and reportable.
7. **Shared runtime preview.** Diagnostic-only drawing may precede runtime skinning, but final pose
   preview uses the same evaluator and LBS/DQS path as the engine.

## 10. Data and correctness gates

The normative Milestone 0 contracts live in the
[Real-Time Skinning Animation Plan](realtime-skinning-animation-plan.md#milestone-0-normative-contracts).
The editor must consume them rather than infer runtime meaning from Mesh Debug fields:

- canonical bone transforms are parent-relative local translation, normalized quaternion rotation,
  and three-component scale;
- global bind and inverse-global-bind are derived using Mini MBM's row-vector convention;
- every bone has a stable nonzero `uint64_t boneId`; names remain labels/interchange keys;
- tail offset, length, and radius are visualization/authoring metadata, not deformation
  transforms;
- skeletal clips are distinct bone-ID-targeted resources, even when easing/player services are
  shared with articulated animation;
- legacy version-1/2 skeleton globals and name-palette weights are temporary audit inputs only;
  they are not supported editor inputs or outputs in the delivered feature and are never silently
  rewritten;
- non-uniform, negative, singular, and shear-bearing transforms receive explicit capability or
  invalid diagnostics;
- bind/pose checks use the centralized numerical policy and report the worst observed error.

The remaining persistence/import gates are: embedded versus referenced resources, exact new binary
layout, FBX handedness conversion, FBX cluster-bind agreement, root motion/attachments/sharing, and
transactional rename/remove/reparent behavior. None may be inferred from current name-based editor
mutation behavior.

## 11. Delivery milestones

### Milestone 0 — Inventory and contracts

- Treat the completed Bones behavior/code inventory and dependency map in Section 6 as input, not a
  runtime model to copy.
- Implement the plan's M0.1 shared row-vector math and M0.2 immutable compiled skeleton.
- Use current global-Euler skeleton data only to validate canonical local-TRS conversion without
  mutating source assets; do not retain this as a delivered compatibility workflow.
- Implement M0.3 structural/bind/weight/scale diagnostics and inverse-bind validation.
- Define M0.4 bone-ID-targeted clip structs and pure deterministic sampling without timeline UI.
- Complete M0.5 persistence layout design before any skeleton/clip writer is implemented.
- Add M0.6 synthetic numeric fixtures and scale-1/scale-100 comparisons.
- Complete M0.7 FBX cluster-bind/handedness audit before promoting the rat to a normative fixture.

Exit evidence: a legacy skeleton compiles and round-trips global→local→global within tolerance, inverse bind
is identity at bind pose, invalid identities/references/transforms produce deterministic reports,
and a synthetic clip samples to expected local/global transforms. No Skeleton/Animation UI field
may be introduced with an undefined storage or runtime meaning. This evidence does not authorize a
permanent legacy API or reader.

### Milestone 1 — Exclusive worktree editor shell

- Add the canonical skeleton/weight readers and FBX import conversion required by the permanent
  editor data path. Do not build the shell around the temporary Mesh Debug bone representation.
- Introduce the six-worktree navigation defined in Section 2 and enforce mutual exclusion.
- Move the accepted Skin Weight Lab GUI/state into its node without behavior regression.
- Establish shared asset, viewport, camera, selection, status, and modified-state services.
- Show unavailable nodes with capability explanations while their data/runtime support is absent.
- Keep mesh visibility shared. Scope skeleton visualization by worktree and gate every selection
  volume, analyzed marker, heatmap, diagnostic overlay, and weight operation to Skin Weight Lab.

Exit: all accepted Skin Weight Lab tests still pass inside the new navigation.

### Milestone 2 — Skeleton inspection and bind validation

- Add hierarchy/tree and viewport inspection for an existing skeleton.
- Show source/derived bind information and structural diagnostics.
- Add identity bind validation against controlled fixtures when pose evaluation is available.
- Preserve imported data without writes in inspection mode.
- Read canonical sections only. Remove the temporary legacy bind-report bridge when equivalent
  canonical inspection is verified.

Exit: the rat and synthetic skeleton fixtures can be inspected and diagnosed reproducibly.

### Milestone 3 — Safe bind corrections

- Add audited rename, reparent, joint/orientation correction, add/remove, and derivation helpers.
- Report and preserve or deliberately remap weight references.
- Introduce a skeleton-aware snapshot/rollback boundary.
- Keep import/export-only conversion controls outside routine bind editing.

Exit: a corrected imported skeleton round-trips without unknown weight or hierarchy references.

### Delivery compatibility gate

Before the skeletal-animation feature is considered delivered:

- remove `SECTION_FRAME_SKINNED`/legacy skeletal-weight read-write support and the exploratory
  `meshDebug` bone mutation/report API;
- reject skeletal assets that contain only the exploratory sections instead of silently converting
  them;
- regenerate test and user skeletal `.msh` assets from source FBX into the canonical format;
- keep ordinary static `.msh` files with no skeletal sections valid.

### Milestone 4 — Local skeleton creation

- Create roots, children, and chains from an unrigged mesh.
- Add deliberate mirror/template helpers with preview.
- Capture/confirm bind pose and hand off to weight authoring.

Exit: a small synthetic character can be rigged entirely inside Mini MBM and passes bind identity.

### Milestone 5 — Clip data and import

- Persist named skeletal clips and bone-targeted tracks.
- Import at least one controlled external animation without losing hierarchy targets or timing.
- Diagnose unsupported channels and missing targets.

Exit: imported reference samples match expected local/global bone transforms within tolerance.

### Milestone 6 — Local Animation node

- Establish the authoring-pose contract before the graphical timeline: sample unsaved clip state at
  an arbitrary time, optionally override one bone's local TRS, rebuild globals/palette, and drive
  both the deformed preview mesh and evaluated skeleton from that same in-memory result.
- Add viewport picking and translation/rotation gizmos on top of that contract; define auto-key and
  explicit-key behavior before treating numeric TRS fields as anything beyond diagnostics.
- Deliver timeline, tracks, keyframes, easing, playback, loop, speed, and composition controls.
- Reuse articulated-animation interaction vocabulary where semantics match.
- Keep bind editing inaccessible while ordinary pose/keyframe editing is active.

Exit: a clip can be authored, saved, reopened, and sampled deterministically inside Mini MBM.

### Milestone 7 — Shared LBS/DQS preview

- Preview clips with the shared runtime pose evaluator and deformation implementation.
- The first GLES2 LBS slice is implemented in the existing Skin Weight Lab preview: canonical clip
  selection, play/restart, pause/resume, and bounded seek call the runtime `mesh` player directly.
  It deliberately adds no tracks, keyframes, or timeline and leaves the bind diagnostic gizmo static.
- Bind-pose restoration now stops the player instead of assuming the clip's zero-time pose is bind.
- LBS/rigid-DQS choice is exposed by rebuilding the preview with the selected method before load;
  the panel reports that method's preparation status, required bones, and measured per-draw capacity.
  Scale/shear rejection is explicit and there is no silent fallback.
- Auto is the editor preview default and displays both requested and resolved methods plus the
  one-time resolution reason; explicit DQS remains available for strict validation.
- Add backend capability selection/reporting when another runtime backend is delivered.
- Side-by-side pose-stress comparison is now available with synchronized runtime LBS/DQS instances,
  mirrored playback/seek/bind restoration, separate readiness reporting, and automatic reframing.
- The first numeric parity gate now compares two-bone LBS/DQS shader output with the CPU references
  through GLES2-compatible RGBA8 readback. The harness now consumes the same private deformation
  source generator as the production default shader. A real-asset subset is still required before
  declaring this milestone complete.
- Eight stable mixed-influence vertices from the Lorekeeper now pass CPU/GPU LBS and DQS position/
  normal comparison at a fixed authored clip time. Remaining Milestone-7 work is final acceptance
  review and any richer overlay/heat-map UX, not basic numeric or side-by-side runtime parity.

Exit: editor and runtime produce matching reference vertices/normals for the same clip and time.

### Milestone 8 — Mesh Debug migration

- Retain or relocate only genuinely mesh-debug/interchange operations.
- Remove duplicated Skeleton/weight authoring after the new workflows reach parity and fixtures pass.
- Convert useful armature-template concepts to the canonical model or remove them; do not retain
  legacy skeletal entry points for template compatibility.
- The first migration slice is complete: the Skeletal Animation Editor builds its bone list,
  hierarchy, global joint positions, radius, and length exclusively from a canonical bind-report
  snapshot. `getTotalBone/getBone` are no longer consulted and a legacy-only asset yields no editor
  skeleton rather than an implicit compatibility conversion.
- Skin Weight Lab now reads, analyzes, normalizes, rigid-binds, smooths, diagnoses, rolls back, and
  saves through the canonical type-42 surface. UI bone names resolve immediately to stable IDs;
  transactional validation rejects an invalid vertex without changing its previous record. It no
  longer calls the exploratory `has/get/setVertexWeight` surface. Mesh Debug and interchange code
  remain the outstanding consumers before legacy APIs/sections can be deleted.
- Mesh Debug no longer presents its legacy Bone node/window, legacy gizmo lifecycle, rigid binding,
  or single/batch removal of exploratory weights. Its Mesh Info weight summary now reads canonical
  type 42. The retired functions remain temporarily as unreachable code while serializers,
  exporters, bindings, and tests are audited for physical deletion.
- The public Lua deletion gate has started: exploratory `add/get/update/removeBone` and
  name-palette weight methods are no longer registered on `meshDebug`. FBX import already writes
  only canonical sections 41–43; its unused 11/40 constants/builders are isolated for deletion.
  C++ storage/parsers and the old Mesh Debug FBX-export implementation remain the next gate.
- Mesh Debug's reverse FBX export now serializes its joint hierarchy from the canonical bind
  report, including each bone's global bind matrix, and reads vertex groups exclusively through
  canonical type-42 methods. Blender reconstructs head, axis, and roll from the global matrix
  rather than exploratory global Euler fields. This removes the last active editor/interchange
  consumer of sections 11/40; only dead implementation and compatibility parsing remain.
- Active runtime/debug loaders now reject sections 11/40 and the Mesh Debug writer neither counts
  nor emits them. Canonical section 41 supplies skeleton metadata inspection. Payload structs,
  serializer symbols, unreachable C++/Lua functions, and test adapters remain for the final
  mechanical deletion pass, but there is no longer persisted compatibility behavior.
- `getSkeletonBindReport()` is now canonical-only in C++ as well: it no longer invokes the legacy
  compiler, carries no canonical/legacy mode flag, and returns no report for an asset without
  section 41. Runtime intermediate loading no longer owns scratch storage for 11/40.
- Removed `refreshSkeletonBindReport()` and its duplicate `COMPILED_SKELETON` cache. Bind summary,
  bone, name, and diagnostic getters now read the canonical compiled skeleton created at load time.
- Retired the pre-canonical test group that constructed `SKELETON_BONE_V11` and name-palette
  weights. The active suite now reaches hierarchy/identity/validation, weights, clips, corruption,
  scale, CPU skinning, GLES preparation, and save/reload solely through canonical fixtures.
- Removed the compiled/public representation of exploratory numeric types 11/40: enum members,
  payload structs and serializers, `MESH_MBM_DEBUG` bone/name-palette-weight APIs, PIMPL storage,
  legacy compiler/validator declarations, and Lua callbacks. Historical payload descriptions remain
  documentation only; affected skeletal assets must be regenerated from FBX.

Exit: there is one canonical implementation for each skeleton, weight, and animation responsibility.

### Milestone 9 — Paint Weights authoring

1. Deliver one minimal transactional **Paint/Add** stroke over the existing surface cursor and
   cached picking foundation. Radius, strength, and falloff are explicit; drag sampling must not
   leave gaps between frames.
2. A complete stroke previews or accumulates changes locally and commits one validated canonical
   type-42 batch on release. One stroke creates one Undo entry; cancellation or validation failure
   restores the exact pre-stroke state.
3. Preserve normalization and the canonical per-vertex influence limit deterministically without
   importer-name or anatomy assumptions. The selected bone gains influence while the remaining
   influences are redistributed according to an explicit policy.
4. After Paint/Add passes synthetic and real-asset deformation checks, add **Erase/Subtract** with
   the same transaction, normalization, Undo, and save/reload guarantees. **Implemented:** it
   reduces existing selected-bone influence, redistributes through normalization, ignores zero
   influence, and preserves a sole rigid influence rather than inventing a replacement bone.
5. Only after both brush directions are stable, migrate selected Skin Weight Lab operations into
   **Weight Tools** and **Repair / Diagnostics**. A later **Influence Distribution** view may report
   dominant-weight concentration, active influence count, and weak-weight contamination, but must
   not label a deformation good or bad without pose-stress evidence.

   **Started:** Weight Tools now includes whole-mesh Clean Weak Influences with a configurable
   threshold, strongest-influence preservation, deterministic normalization, one atomic batch, and
   one Undo entry. Normalize All and Limit Four are not duplicated because canonical type-42
   validation already enforces both invariants.

   Repair / Diagnostics now includes an optional Influence Distribution heatmap using a normalized
   concentration score derived from each vertex's dominant weight, plus aggregate raw dominant
   min/average/max and one-to-four active
   influence counts. It remains explicitly descriptive rather than a deformation-quality verdict,
   and disabling it restores the selected-bone heatmap.

   Weak Influence Contamination is also available as a mutually exclusive read-only view. It maps
   each vertex's sum of positive weights strictly below the shared Clean threshold and reports
   affected vertices, weak influence count, total weak mass, and maximum weak weight. Threshold
   changes refresh the diagnostic; only the explicit Clean action mutates data.

   Abrupt Weight Transitions now provides a fourth read-only view. It measures complete normalized
   weight-vector difference across triangle edges with bounded half-L1 distance, maps each vertex's
   maximum incident difference, and reports threshold-classified edges and unique vertices. Moving
   the threshold reclassifies cached edge distances without rebuilding heatmap geometry. Automatic
   smoothing of detected transitions is now available contextually: only classified vertices are
   edited, external neighbors stay fixed during configurable stable Jacobi passes, complete vectors
   normalize/limit deterministically, candidate influences remain inside each vertex's original
   one-ring neighborhood, and a half-L1 maximum-change cap prevents iteration count from producing
   an unbounded single repair. One atomic batch plus Undo entry reports edge counts before and after.

Exit: direct painting is continuous, deterministic, normalized, bounded by the runtime influence
contract, undoable per stroke, and persistent across save/reload.

Current delivery: items 1 and 4 plus the transaction core of items 2-3 are implemented. Paint/Add,
Erase/Subtract, and selected-bone Smooth use a
cached vertex BVH, bounded quarter-radius interpolation between ordinary surface hits, local per-stroke accumulation,
deterministic four-influence normalization, one atomic batch on release, one Undo snapshot, and Esc
cancellation. Interactive deformation quality remains; save/reload acceptance is closed by an
executable atomic-batch round-trip that verifies complete four-slot and two-slot edited records.

## 12. Validation fixtures and acceptance

The alien-rat bundle remains the primary real asset for hierarchy, weights, extreme proportions,
neck/shoulder deformation, rigid cavity, and Mixamo comparison. It is not sufficient by itself.
Additional small fixtures must isolate:

- one root and one child;
- multiple roots and branching hierarchy;
- add/remove/reparent and orphan-reference behavior;
- bind identity;
- antipodal quaternion inputs;
- uniform and non-uniform scale;
- one-bone rigid LBS/DQS equivalence;
- imported clip timing and coordinate conversion;
- locally authored clip save/reopen determinism.

Every milestone records source asset, operation, before/after data, preview/runtime method, backend,
expected tolerance, and observed result. Visual approval alone does not replace numeric bind/pose
checks; numeric checks alone do not replace a visual deformation pass on the rat.

## 13. Risks

| Risk | Impact | Mitigation direction |
|---|---|---|
| Copying Bones preserves export heuristics as runtime truth | Incorrect bind and animation model | Audit and classify; reuse interactions only behind explicit contracts. |
| Bind edits and pose edits look identical | Accidental destruction of the rest skeleton | Distinct nodes/modes, colors, labels, and snapshot boundaries. |
| Bone names act as identity everywhere | Rename/remap becomes fragile | Decide stable identity before clip persistence. |
| Imported content is silently normalized or rebuilt | High-quality external data is lost | Preserve by default; preview and report every conversion. |
| Local editor attempts DCC parity | Scope becomes unbounded | Target sufficient skeletal workflows, not modeling/constraint-suite replacement. |
| Animation UI duplicates articulated code and semantics drift | Two incompatible editors | Extract/share services only where semantics truly match. |
| DQS hides unsupported scale behavior | Preview/runtime disagreement | Detect, report, and require explicit method/fallback policy. |
| Worktrees own separate copies of asset state | Cross-worktree corruption and stale views | One asset context and explicit invalidation rules. |

## 14. Out of scope for the initial Skeleton/Animation deliveries

- general mesh modeling or topology editing;
- a Blender-equivalent constraint, IK, graph, or non-linear-animation suite;
- automatic anatomically correct rigging for arbitrary characters;
- automatic Mixamo marker placement or service integration;
- procedural tail physics in the first clip milestone;
- corrective blend shapes and muscle simulation;
- claiming all FBX animation features are supported before measured import coverage exists.

## 15. Open decisions

1. Embedded versus referenced skeleton and clip resources and their exact binary section layout.
2. First supported FBX animation-import scope after handedness and cluster-bind validation.
3. Root motion, attachment, multiple-root, and multi-mesh skeleton-sharing semantics.
4. Whether initial LBS accepts non-uniform scale before the final normal-transform path exists.
5. Exact shared service boundary with articulated-animation easing/player code.
6. Transactional rename/remove/reparent remapping and snapshot/undo scope across skeleton, weights,
   and clips.
7. Template versioning and compatibility with existing armature Lua files.
8. Which Mesh Debug interchange controls remain after migration.

## 16. Handoff readiness

Milestone 0 and the exclusive worktree shell are implemented. Skeleton mutation and clip persistence must
not outrun the data/correctness gates in Section 10. Each later milestone requires an executable
verification plan tied to both synthetic fixtures and the alien rat.

## 17. Change log

| Version | Date | Change |
|---|---|---|
| 7.99 | 2026-08-16 | Changed the safety overlay from red outlines to double-sided translucent filled faces and split protected-face, failed-sample, and seam counts onto separate GUI lines. |
| 7.98 | 2026-08-16 | Added a cached post-repair safety overlay. Red edges show unique faces that rejected the unrestricted candidate in at least one sampled pose; cyan crosses show synchronized coincident seam vertices. Counts distinguish unique faces, failed face/pose samples, seam vertices, and seam groups without per-frame reevaluation. |
| 7.97 | 2026-08-16 | Added topology-seam preservation to abrupt-transition repair. Mesh-scale coincident vertices are grouped only when their independent triangle neighborhoods share geometry; compatible copies receive one identical atomic weight candidate, while pre-existing weight conflicts remain unchanged and are reported. |
| 7.96 | 2026-08-16 | Added pose-aware surface preservation to abrupt-transition repair. Five deterministic LBS clip samples test incident triangles for severe area loss or orientation reversal; an unsafe batch is reduced by binary search and reports its final safety scale plus avoided face samples. |
| 7.95 | 2026-08-16 | Added shape-preserving safety to abrupt-transition repair: candidate bones are frozen to each vertex's original one-ring neighborhood, final half-L1 weight-vector change is capped independently of strength/iterations, and the GUI previews affected vertices plus the cap. |
| 7.94 | 2026-08-16 | Closed Paint Weights save/reload acceptance at its shared atomic type-42 boundary. The deterministic batch fixture now reloads and verifies every name/order/value/empty slot of separate four-influence and two-influence edited vertices. |
| 7.93 | 2026-08-16 | Added contextual Smooth Detected Transitions repair. Threshold-classified vertices receive configurable full-vector Jacobi smoothing against triangle neighbors with fixed external boundaries, deterministic normalization/four-influence limiting, one atomic batch/Undo entry, and before/after abrupt-edge reporting. |
| 7.92 | 2026-08-16 | Moved Show Skeleton to the first Paint Weights control. Clarified that the Abrupt Weight Transitions threshold classifies cached statistics only and intentionally does not recolor the raw difference heatmap. |
| 7.91 | 2026-08-16 | Made Target Bone contextual to Selected Bone Heatmap. Whole-weight diagnostics now hide the irrelevant selector and disable viewport bone picking, while retaining skeleton visibility and normal left-drag camera orbit. |
| 7.90 | 2026-08-16 | Added read-only Abrupt Weight Transitions diagnostics. Complete normalized weight vectors are compared across triangle edges with half-L1 distance; the heatmap shows each vertex's maximum difference, while a threshold classifies cached edges and affected vertices without geometry rebuilds. |
| 7.89 | 2026-08-16 | Moved Repair / Diagnostics to the first Paint Weights block so its primary mode selection determines all contextual sections immediately in the same GUI frame. |
| 7.88 | 2026-08-16 | Made the reorganized Paint Weights panel contextual. Brush controls/cursor/right-drag exist only in Selected Bone Heatmap; both diagnostics are read-only; Weak Contamination alone exposes its threshold and contextual Clean action after the statistics. |
| 7.87 | 2026-08-16 | Reorganized Paint Weights into Target/Skeleton, Brush Operations, Viewport Feedback, Repair/Diagnostics, Weight Tools, and History sections. Replaced mutually disabling diagnostic checkboxes with one three-option radio group and placed Clean controls after diagnostics. |
| 7.86 | 2026-08-16 | Added Weak Influence Contamination diagnostics tied to the Clean threshold. The read-only heatmap shows per-vertex weak-weight mass and reports affected vertices, weak influence count, total mass, and maximum weak weight; it is mutually exclusive with Influence Distribution. |
| 7.85 | 2026-08-16 | Started Paint Weights Repair / Diagnostics with optional Influence Distribution. The interpolated map shows each vertex's dominant normalized weight and reports min/average/max plus counts using one through four influences, without labeling deformation good or bad. |
| 7.84 | 2026-08-16 | Made Smooth effective on dense topology with 1-10 stable neighbor-average iterations per stroke, defaulting to three. Brush-operation radio buttons now occupy one line each for clearer selection. |
| 7.83 | 2026-08-16 | Added Smooth as a third transactional brush operation. It moves only the selected bone's weight toward the triangle-neighbor average under brush Strength/Falloff, proportionally redistributes remaining influences, normalizes, and commits one atomic batch plus one Undo entry. |
| 7.82 | 2026-08-16 | Started Paint Weights Weight Tools with whole-mesh Clean Weak Influences. A configurable threshold removes small weights while preserving each vertex's strongest influence, then deterministically renormalizes and commits one atomic batch plus one Undo entry. |
| 7.81 | 2026-08-16 | Replaced the Paint Weights brush-operation combo with directly visible Paint/Add and Erase/Subtract radio buttons. |
| 7.80 | 2026-08-16 | Added Erase/Subtract through the Paint/Add stroke transaction. It reduces existing selected-bone weights, renormalizes remaining influences, ignores zero-weight vertices, preserves sole rigid influences, and commits one atomic batch plus one Undo entry. |
| 7.79 | 2026-08-16 | Aligned Paint Weights mouse input with the rest of the editor: left-drag remains camera orbit and left-click selects visible bones, while right-drag exclusively performs Paint/Add. |
| 7.78 | 2026-08-16 | Implemented the first Paint/Add authoring stroke: radius/strength/falloff controls, vertex-BVH radius queries, bounded quarter-radius drag interpolation, deterministic normalized four-influence blending, one atomic type-42 batch and Undo entry per release, Esc cancellation, and post-commit heatmap refresh. |
| 7.77 | 2026-08-16 | Defined Paint Weights Milestone 9: transactional gap-free Paint/Add first, then Erase/Subtract, followed only later by selected Weight Tools and diagnostics. Reserved Influence Distribution as a secondary concentration/mixing diagnostic rather than a deformation-quality or rigidity verdict. |
| 7.76 | 2026-08-16 | Aligned Paint Weights viewport selection with canonical Bone Editor ownership: an explicit `head -> tail` segment is drawn and picked as its own bone, independent of importer names or anatomy; bones without explicit tails remain joint-selectable without guessed topology. |
| 7.75 | 2026-08-16 | Reduced Paint Weights CPU/geometry pressure: the continuous overlay now uses one vertex/UV per canonical vertex with 16-bit indexed drawing when possible and an explicit large-mesh non-indexed fallback. Cursor picking/recreation now requires actual pointer movement and is capped at 30 Hz. |
| 7.74 | 2026-08-16 | Replaced face-average heatmap buckets with true per-vertex interpolation. Paint Weights stores each selected-bone weight in the overlay UV stream, lets rasterization interpolate it across triangles, and maps the continuous value through a dedicated heatmap pixel shader. |
| 7.73 | 2026-08-16 | Added the non-destructive Paint Weights visual foundation: isolated state, panel/viewport bone selection, an opaque face-filled selected-bone heatmap that replaces the ordinary textured preview inside this worktree, independent skeleton visibility, cached frame-zero triangles, BVH-accelerated local raycast, and a surface-oriented configurable-radius cursor. Painting and weight mutation remain intentionally absent from this slice. |
| 7.72 | 2026-08-16 | Reframed Paint Weights as the primary visual authoring workflow and eventual successor to the standalone Skin Weight Lab experience. The Lab remains during incremental delivery; its useful batch and diagnostic operations migrate later under Weight Tools / Repair and Diagnostics after brush stability and parity. Began the non-visual foundation with atomic canonical weight batch mutation. |
| 7.71 | 2026-08-16 | Closed the session with a documentation drift audit. Current-state sections now identify Bone Editor, local animation authoring, timeline, viewport TRS, clipboards, and bounded Undo/Redo as delivered; only Paint Weights remains a reserved worktree, while multi-clip composition remains explicitly deferred. |
| 7.70 | 2026-08-16 | Marked multi-clip composition/blending as explicit deferred work while development moves to Paint Weights. Priority, weight/fade, Absolute/Additive, masks, persistence boundaries, and composed-pose math remain pending and are not implied by the delivered clip/pose clipboards. |
| 7.69 | 2026-08-16 | Added complete-skeleton pose copy/paste. A strict batch API requires exactly one stable identity/TRS per bone, creates or extends T+R+S tracks, creates/updates every playhead key on one candidate, and commits with one Undo or rejects all. The editor records source clip/time/count and includes localized tooltips. |
| 7.68 | 2026-08-16 | Added localized tooltips to selected-bone pose copy/paste, explaining evaluated temporary-pose capture, one-bone scope, T+R+S key creation/update, inherited descendant motion without child-key edits, stable identity, and the single Undo boundary. |
| 7.67 | 2026-08-16 | Placed selected-bone pose paste on its own GUI line and clarified that the operation authors one bone's local T+R+S at the destination playhead, not the complete character pose. |
| 7.66 | 2026-08-16 | Added a distinct selected-bone pose clipboard. It captures the evaluated local TRS, including temporary gizmo preview, resolves the stable bone ID after clip changes, and atomically commits T+R+S at the playhead through one existing authoring-key transaction and Undo entry. |
| 7.65 | 2026-08-16 | Extended the key clipboard across clips with detached full payloads. A new atomic backend/Lua operation maps stable bone IDs into the destination, requires equal masks on existing tracks, creates missing tracks without intermediate state, preserves relative timing/easing/Bezier/TRS, and rejects unknown bones, mask conflicts, bounds, collisions, or invalid candidates without mutation. |
| 7.64 | 2026-08-16 | Added the first transactional key clipboard: selected timeline keys copy as bone-ID/original-time identities, paste aligns them to the playhead through the atomic batch duplicate contract, repeated index resolution avoids stale indices, Ctrl+C/Ctrl+V respect active ImGui controls, and same-clip/range/source/collision gates are explicit. |
| 7.63 | 2026-08-16 | Replaced frozen/generic history text with operation-specific translation keys across skeleton, Bone Editor, weights, clips, tracks, keys, and timeline transactions. Undo/Redo menus and feedback now resolve descriptions in the editor's current language. |
| 7.62 | 2026-08-16 | Undo/Redo now resets the timed overlay interval for every operation, so localized or repeated feedback remains visible after the welcome timer has expired. |
| 7.61 | 2026-08-16 | Undo/Redo success feedback now uses the editor's timed global overlay in addition to persistent panel status, keeping the operation visible regardless of panel scroll, active worktree, detached timeline, or viewport focus. |
| 7.60 | 2026-08-16 | Replaced the single rollback slot with bounded 50-entry Undo/Redo snapshot stacks shared by every existing atomic commit boundary. Restoration covers canonical data plus core editor context and rebuilds dependent previews/reports; Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z, menus, worktree controls, redo invalidation, cap eviction, and owned-file cleanup are wired. Initial authoring/bone-drag descriptions are specific; remaining commands use a safe generic label. |
| 7.59 | 2026-08-16 | Runtime Skeletal Preview can now explicitly rebuild the real runtime mesh/player from a temporary snapshot of current unsaved canonical memory. Source labels distinguish saved file, current memory, and stale memory; later mutations mark staleness, and method/pose-stress rebuilding preserves the chosen memory source. |
| 7.58 | 2026-08-16 | Scale availability now follows the authoring preview's resolved method. Forced DQS and Auto-to-DQS disable Scale, LBS and Auto-to-LBS enable uniform scaling, and an incompatible method change safely returns the active tool to Move. |
| 7.57 | 2026-08-16 | Corrected Scale UX to the delivered backend contract. Compact GLES2 LBS normals support uniform scale only and rigid DQS supports none, so per-axis handles are hidden instead of failing during preview; the yellow proportional XYZ handle remains functional and non-uniform scale is reserved for an inverse-transpose normal-palette milestone. |
| 7.56 | 2026-08-16 | Scale adds a yellow bone-local diagonal handle for proportional XYZ multiplication. Animation now exposes Auto/LBS/DQS authoring-preview selection plus resolved method; immutable-method preview rebuilding uses a temporary snapshot of current unsaved canonical state rather than reloading the stale source asset. |
| 7.55 | 2026-08-16 | Completed the initial visual Scale pose tool. Bone-local X/Y/Z axes drive one positive local-scale component through the shared in-memory override evaluator; explicit commit and Auto Key author only channel S through the existing atomic snapshot/rollback path, while runtime DQS compatibility remains explicit. |
| 7.54 | 2026-08-16 | Replaced the fixed five-division ruler with adaptive `1/2/5 x 10^n` major ticks derived from visible duration and pixel width. Precision follows tick scale, grid lines span tracks, near-zero labels normalize to zero, and a hard cap bounds per-frame draw work. |
| 7.53 | 2026-08-16 | Added one-click temporal-snap presets for 24, 25, 30, 50, and 60 FPS. Each stores the exact reciprocal interval and enables snap, while the existing numeric control remains the arbitrary-rate path. |
| 7.52 | 2026-08-16 | Added optional configurable temporal snap, defaulting to `1/30 s`. Timeline seeking and dragged selection anchors quantize to the positive step, group deltas preserve relative timing, playhead-based interval operations inherit alignment, and canonical backend validation remains authoritative. |
| 7.51 | 2026-08-16 | Added Fit selection to frame selected key-time bounds with ten-percent padding per side. A single-time selection receives a small centered range, bounds clamp to the clip, pan state resets, and the operation remains purely navigational. |
| 7.50 | 2026-08-16 | Added a discoverable full-width horizontal-pan slider whenever the timeline is zoomed. It controls the same bounded visible range as middle-button drag, stays synchronized with zoom/framing, preserves range duration, and disappears when the complete clip is visible. |
| 7.49 | 2026-08-16 | Resolved the timeline navigation conflict on rigs with many tracks: horizontal zoom now requires Ctrl plus the mouse wheel, while the unmodified wheel remains dedicated to vertical track scrolling; middle-button pan is unchanged. |
| 7.48 | 2026-08-16 | Added the timeline horizontal-navigation foundation: clip-specific visible range, cursor-anchored wheel zoom, middle-button pan, full-clip framing, visible-range feedback, adaptive range labels, and one shared time/screen transform for ruler, keys, playhead, picking, drag, selection, and interval preview. |
| 7.47 | 2026-08-16 | Normalized unsupported typographic glyphs in editor-visible strings to ASCII-safe equivalents. Time ranges now use ` - `, angles use `deg`, truncated bone labels use `...`, and the shared language catalog no longer exposes risky dash, multiplication, or arrow glyphs. |
| 7.46 | 2026-08-16 | Completed transactional time-range removal. Explicit confirmation is tied to the currently previewed interval; mutation deletes keys in `[start,end)`, shifts later keys left, shrinks duration, returns the deletion count, blocks empty tracks, validates once, and creates one rollback entry. |
| 7.45 | 2026-08-16 | Began safe time removal with a non-mutating playhead-relative interval preview. The timeline shades the effective range and reports keys to delete plus canonical tracks that would become empty, establishing blockers before transactional removal is exposed. |
| 7.44 | 2026-08-16 | Added atomic empty-time insertion at the playhead with an editor duration control. It creates no keys, grows the clip, shifts every key at or after the insertion point across all tracks by the same duration, and commits through one rollback entry. |
| 7.43 | 2026-08-16 | Added atomic ripple insertion at the playhead. It grows the clip to open the selected time span across every track, shifts later keys with canonical endpoint separation, inserts complete selected payloads with relative timing intact, and rejects invalid candidates without partial mutation. |
| 7.42 | 2026-08-16 | Added atomic Duplicate at playhead for timeline selection. The earliest selected key aligns to the playhead, relative spacing/tracks/full payloads are preserved, and a candidate-copy backend insertion sorts affected tracks and rejects duplicate references, bounds errors, or collisions without partial copies. |
| 7.41 | 2026-08-16 | Timeline canvas now consumes the window's full remaining vertical area, revealing more tracks when resized. Empty-space drag draws a clipped rectangle and selects enclosed visible key markers on release; Ctrl makes it additive, while a stationary click still seeks. |
| 7.40 | 2026-08-16 | Added atomic multi-key timeline movement. A selected group previews one shared clamped delta and cross-checks unselected same-track keys; release passes pre-move track/key pairs to one candidate-copy backend mutation that sorts affected tracks and commits only after complete canonical validation. Lua API and rejection/round-trip tests cover the contract. |
| 7.39 | 2026-08-15 | Added non-mutating timeline multi-selection: normal click selects one key, Ctrl-click toggles keys across tracks, markers/count/clear action expose selection state, and clip/workspace/mutation transitions clear stale identities. Group dragging stays disabled until an atomic backend batch operation exists. |
| 7.38 | 2026-08-15 | Added timeline authoring playback over the shared in-memory evaluator: Play/Restart, Pause/Resume, Stop, `0.05x..4x` speed, loop/end handling, synchronized playhead/mesh/skeleton, and automatic pause before seek or transform editing. Playback never authors keys. |
| 7.37 | 2026-08-15 | Made timeline collisions perceptible: an eight-pixel capture zone snaps the dragged preview to another same-track key's exact time, and the larger invalid red marker renders after all ordinary markers so it cannot be occluded. Canonical `1e-6` time validation remains authoritative. |
| 7.36 | 2026-08-15 | Added horizontal timeline key dragging. Drag previews only marker/playhead time, clamps to clip bounds, marks same-track collisions red, and commits one backend-validated reorder plus rollback entry on release while preserving the complete key payload. |
| 7.35 | 2026-08-15 | Made the timeline freely movable/resizable after its initial panel-relative placement. Cached the detached animation report until a shared asset-report invalidation, removing per-frame reconstruction/allocation of every clip, track, and key that caused delayed CPU/GC growth while Animation sat idle. |
| 7.34 | 2026-08-15 | Timeline horizontal placement now begins at the live right edge of the resizable left worktree window and occupies only the remaining screen width, rather than covering the panel beneath it. |
| 7.33 | 2026-08-15 | Moved the timeline from the narrow worktree panel into a fixed full-width bottom window, widened bone/channel labels, and culled off-screen track/key draw submission using the child scroll range to remove the newly introduced per-frame CPU cost. |
| 7.32 | 2026-08-15 | Began the graphical timeline with a scrollable canonical-track canvas, time ruler, channel-labelled rows, selectable key markers, and synchronized playhead seeking. Key selection resolves the authored bone and exact key time; marker dragging remains deferred to its transactional collision policy. |
| 7.31 | 2026-08-15 | Added opt-in Auto Key for Animation pose gizmos. It defaults off; a genuinely moved drag commits exactly its T or R channel on release through the existing snapshot/rollback transaction, while clicks without movement create nothing and disabled mode remains temporary. |
| 7.30 | 2026-08-15 | Removed destructive per-mouse-event Animation skeleton rebuilding. The authoring viewport now keeps joint objects and parent-child line segments persistent and updates transforms/dynamic buffers in place together with the sampled palette; full rebuild remains reserved for structural changes. |
| 7.29 | 2026-08-15 | Fixed the Animation Move/Rotate controls to use the editor binding's numeric three-argument RadioButton contract; the unsupported boolean overload previously raised a Lua error and left the ImGui window unbalanced. |
| 7.28 | 2026-08-15 | Animation pose authoring adds explicit Move/Rotate tools and persistent local XYZ rotation rings. Ring drag writes only an in-memory normalized quaternion override, updates mesh and evaluated skeleton together, and requires an explicit R-channel key commit or discard. |
| 7.27 | 2026-08-15 | Made Move/Rotate visually distinct with a yellow camera-plane orbit guide centered on the fixed head and sized to the preserved visual length. Joint-radius authoring now clamps and validates a strictly positive mesh-relative minimum; zero rejects without mutation. |
| 7.26 | 2026-08-15 | Fixed segment rotation under scaled bind bases by preserving displayed global head-tail distance rather than raw local tail magnitude; the live label now reports visual length. Joint radius input is now a scale-proportional DragFloat. |
| 7.25 | 2026-08-15 | Added atomic joint-radius editing for the selected bone or complete descendant subtree. The control refreshes joint rendering/picking, preserves ancestors and other branches, supports rollback, and explicitly does not imply envelope generation or weight mutation. |
| 7.24 | 2026-08-15 | Fixed axis-gizmo direction and accidental capture. The closest ray/axis parameter sign now follows cursor/world direction; bone geometry has picking priority, only the outer handle is active, near-view-parallel axes are rejected, tolerance is narrower, and the active axis is visually isolated. |
| 7.23 | 2026-08-15 | Added selected-point global X/Y/Z handles and real incremental snapping. Axis-handle picking uses closest ray/axis motion and constrains only that gesture; the nonnegative Snap step quantizes displacement symmetrically around the gesture start, with zero retaining continuous movement. |
| 7.22 | 2026-08-15 | Added exact gesture cancellation for head, tail, segment move, and segment rotation. Esc/right-click reloads the pre-mouse-down complete asset snapshot, restores modified state and stable selection, rebuilds editor consumers, discards the temporary file, and creates no rollback-history entry. |
| 7.21 | 2026-08-15 | Bone-removal preview highlights the current weight-transfer target in green across joint, tail, and segment while retaining blue selection on the removal source. Combo changes update immediately and closing the removal section clears the preview. |
| 7.20 | 2026-08-15 | Migrated safe bone removal into Bone Editor: selected-bone impact counts, replacement selection, global-preserving child promotion, explicit track discard, last-bone blocking, confirmation, surviving-parent selection, and whole-asset rollback reuse the existing transactional backend. |
| 7.19 | 2026-08-15 | Added explicit connect/disconnect against the current parent tail. Connect snaps the head, preserves the selected global tail, optionally compensates every other joint, and rejects parents without explicit tails; disconnect clears only the constraint. Hierarchy changes remain a separate structural operation. |
| 7.18 | 2026-08-15 | Fixed direction-arrow trails/drift during rotation by retaining one line object and replacing line 1 points in place. The drag loop no longer destroys/recreates renderables, so prior frames cannot appear to accumulate toward -Z/-Y. |
| 7.17 | 2026-08-15 | Added a selected-segment direction arrow at the head joint. The short yellow always-on-top shaft and arrowhead follow head→tail continuously during manipulation, making endpoint rotation readable directly in the viewport. |
| 7.16 | 2026-08-15 | Added live selected-segment orientation feedback: normalized local direction, length, inclination from +Y, and XZ azimuth update during endpoint rotation. The UI explicitly leaves roll undefined and states that endpoint direction does not silently rewrite the canonical bind quaternion. |
| 7.15 | 2026-08-15 | Added an explicit Move/Rotate segment-drag mode. Rotation fixes the head, projects the pointer in the viewport, converts it through the bind basis, and normalizes the tail to its original bone-local length; connected joints, preservation policy, rollback, and global axis constraints remain coherent. |
| 7.14 | 2026-08-14 | Repeated tail extension now inherits the selected non-root segment length for the entire generated chain. Root extension alone uses the Bone length field, and the resolved value is shown beside the action. |
| 7.13 | 2026-08-14 | Added atomic repeated tail extension and global-axis drag constraints. A 1–256 count creates a connected directional chain in one validated commit; Snap X/Y/Z restricts joint and segment displacement to any selected axis combination, while no selection retains free camera-plane dragging. |
| 7.12 | 2026-08-14 | Completed direct segment translation: dragging the bone body moves head and tail rigidly without changing length/orientation, disconnects its former parent-head constraint when necessary, carries connected child heads, and applies the same optional global compensation policy to all other joints. |
| 7.11 | 2026-08-14 | Added the default-on Preserve other joints policy to bind dragging. It compensates descendant local TRS so joints outside the manipulated head/shared joint retain their global bind transforms; disabling it deliberately restores ordinary parent-to-subtree propagation. |
| 7.10 | 2026-08-14 | Added direct head dragging. Independent heads convert the camera-plane world target to parent-local translation while preserving their own global tail; connected logical-joint drag continues through the parent tail and now preserves every connected child's opposite global tail, reshaping adjacent segments correctly. |
| 7.9 | 2026-08-14 | Moved overlap cycling from mouse-down to click release. Mouse-down locks the already highlighted candidate; movement beyond three pixels drags that candidate without reselection, while a stationary release advances to the next nearest-depth overlap. This removes the select-one/drag-another conflict. |
| 7.8 | 2026-08-14 | Resolved overlapping endpoint ambiguity. Connected parent-tail/child-head members are one highlighted logical joint owned by the parent tail; coincident but disconnected endpoints remain separate and repeated clicks near the same pixel cycle deterministic nearest-depth candidates without requiring zoom. |
| 7.7 | 2026-08-14 | Removed tail-drag stalls on weighted meshes: mouse-move reports omit per-bone vertex/track impact scans, tail-only commits skip dependency validation whose identity inputs cannot change, and full gizmo reconstruction is capped near 30 Hz. A complete report and preview rebuild still run on release. |
| 7.6 | 2026-08-14 | Added direct tail dragging on a camera-facing plane. Each world hit is converted through the global bind basis to a new bone-local tail; length and connected child heads update atomically, while one rollback snapshot covers the full gesture. Moving a connected child head independently clears its connection constraint. |
| 7.5 | 2026-08-14 | Added Extend Selected Tail. It creates a child bone at the exact parent-local tail offset, persists `connectedToParent=true` in canonical skeleton section 41 version 3, commits through rollback, and selects the new segment. FBX import preserves Blender's own connection flag instead of inferring it from coincident endpoints. |
| 7.4 | 2026-08-14 | Split root authoring into Add Joint and Add Bone. A joint is a canonical hierarchy transform with `hasExplicitTail=false` and exposes only its head hit target; a bone additionally owns an explicit tail and selectable segment. Ordinary TRS/length editing no longer silently promotes a transform-only joint into a bone. |
| 7.3 | 2026-08-14 | Replaced the Bone Editor's incorrect universal local-+Y visualization assumption with an explicit canonical tail joint stored in bone-local bind space. Skeleton section 41 version 2 persists the tail offset and provenance; FBX import captures Blender's real world tail and converts it back to bone-local space. Version-1 canonical assets remain readable as transform-only joints until explicitly completed or reimported. |
| 7.2 | 2026-08-14 | Added distinct Bone Editor viewport selection for initial joint, final joint, and owned segment. Joint selection highlights one endpoint; segment selection highlights both endpoints and the segment; empty-space click clears selection and preserves orbit. This slice is deliberately non-mutating. |
| 7.1 | 2026-08-14 | Added explicit positive root length to the simplified Bone Editor. It defaults to 1 and defines the derived local +Y head-to-tail distance used by the standalone segment visualization. |
| 7.0 | 2026-08-14 | Split ordinary bind construction from advanced diagnostics by adding the Bone Editor worktree while retaining Bind Pose Contract unchanged. The first slice creates independent roots at numeric XYZ, derives a visible tail at local +Y with length 1, and renders the standalone head/tail pair plus segment. Extension and joint/segment mouse semantics remain the next slices. |
| 6.9 | 2026-08-14 | Added explicit commit for temporary translation. One atomic canonical operation finds/creates the bone track, unions the translation channel, creates or updates the key at authoring time, validates the entire animation candidate, and reports insertion versus update. The editor exposes a deliberate commit button with whole-asset rollback; mouse release remains non-persistent and Auto Key remains off. |
| 6.8 | 2026-08-14 | Added the first non-destructive transform gizmo over the selected evaluated bone. World XYZ translation handles use ray/axis dragging, convert world displacement through the inverse parent 3x3 basis into canonical local translation, and continuously reevaluate mesh, skeleton, and palette through the one-bone in-memory override. The pose is visibly temporary and discardable; releasing the mouse does not silently author a key. Rotation and commit/auto-key policy remain next. |
| 6.7 | 2026-08-14 | Added the first viewport-authoring interaction above the in-memory pose contract: a nearest-hit pick ray selects evaluated joints and parent-to-child bone segments by canonical index, updates the shared tree/track selection, and preserves empty-space camera orbit. Selection is non-mutating; translation/rotation gizmos remain next. |
| 6.6 | 2026-08-14 | Added the shared in-memory authoring-pose contract before graphical manipulation: unsaved clips evaluate at arbitrary time with an optional one-bone local override, rebuild hierarchy/global transforms and LBS/DQS palette, and drive both the paused runtime mesh and evaluated skeleton in the Animation worktree without save/reload. Viewport picking, gizmos, auto-key policy, and timeline remain next. |
| 6.5 | 2026-08-14 | Added transactional keyframe authoring. Insertion at a unique in-range time samples the pre-edit clip and captures the target bone's evaluated local TRS, preserving the curve; update edits time/T/Q/S/easing/Bezier, normalizes quaternion, and reorders atomically; confirmed removal retains the required final key. Duplicate/out-of-range times and invalid transforms reject without mutation. Inspector fields, rollback, strict ordering, numeric tests, and save-reload are covered; graphical timeline remains next. |
| 6.4 | 2026-08-14 | Added transactional per-bone track-container authoring. Creation filters duplicate targets, accepts nonempty T/R/S masks, resolves the selected bone to stable identity, and seeds a time-zero key from local bind TRS because canonical tracks cannot be empty. Channel edits preserve/revalidate key payloads; confirmed removal deletes the track and its keys. Independent expanded-track UI state, rollback, validation, and save-reload are covered; key editing/timeline remain next. |
| 6.3 | 2026-08-14 | Added the first transactional Animation-worktree mutations at the clip-container level. Empty clips receive opaque stable IDs; name/duration/loop edits preserve identity and validate all existing key times; duplicate names and duration truncation reject without mutation. Confirmed removal deletes the selected clip with its tracks/keys, clears type-43 when it was the last clip, and all actions share whole-asset rollback/save-reload. Track/key editing remains read-only. |
| 6.2 | 2026-08-14 | Closed the implemented Milestone-4/5 foundation and began Milestone 6 with a read-only canonical animation inspector. The Animation worktree now exposes clip ID/name/duration/loop, bone-resolved tracks, T/R/S masks, and key time/local quaternion TRS/easing/Bezier data through a detached C++/Lua report. Track selection synchronizes the editor bone index; timeline and destructive mutation remain deliberately disabled until their transaction model is added. |
| 6.1 | 2026-08-13 | Added the explicit local-rig handoff to Skin Weight Lab. With no type-42 section, a selected bone can initialize complete frame-zero coverage rigidly at weight 1.0 after an impact preview and confirmation. The validated transaction never guesses influences or overwrites existing weights, supports whole-asset rollback/save-reload, and opens the weight workspace for deliberate redistribution. |
| 6.0 | 2026-08-13 | Added deliberate atomic subtree mirroring for local rigging. The editor previews duplicate count, global X/Y/Z origin plane, generated-name prefix, and clip capability before confirmation. C++ reflects complete global bind matrices by conjugation, rederives locals against original/mirrored parents, preserves hierarchy/metadata, assigns new stable IDs, validates dependencies, selects the mirrored root, and supports rollback/save-reload. Existing weights are intentionally untouched and assets with clips remain blocked pending animation-aware mirroring. |
| 5.9 | 2026-08-13 | Added atomic local chain creation. From root or an existing parent, the editor creates 1–256 `prefix1..N` bones with uniform parent-relative translation step, identity rotation/scale, inherited metadata, unique opaque IDs, and each item parenting the next. All names/IDs and 41–43 dependencies validate before one commit; failure leaves no partial chain, success selects the final bone, and save/reload plus rollback are covered. |
| 5.8 | 2026-08-13 | Completed the audited Milestone-3 structural foundation and began Milestone 4 with initial local skeleton creation. A loaded nonskeletal mesh can create canonical section 41 with one root, stable IDs, identity rotation/scale, explicit metadata, AABB-base editor defaults, complete validation, save/reload, and rollback. No weights or clips are fabricated; subsequent bones reuse the delivered canonical add/edit/reparent/remove path. Mouse gizmos, adaptive track baking, chain/mirror helpers, and bind confirmation remain refinements/later Milestone-4 slices. |
| 5.7 | 2026-08-13 | Added deterministic animated parent-space conversion for child-bearing removal. Each promoted child composes its sampled local pose with the removed bone's local pose and receives a full-TRS linear track at clip boundaries plus the union of both authored key-time sets. Numeric coverage preserves global pose at authored samples; shear rejects atomically. Continuous interpolation between baked samples is explicitly not claimed identical, leaving adaptive resampling as refinement. Removed-bone tracks still require discard confirmation. |
| 5.6 | 2026-08-13 | Added the first explicit descendant-removal policy. On assets without canonical clips, direct children may be reparented to the removed bone's parent (or promoted to roots), with local TRS rederived to preserve each global bind and stable parent-first order restored before validation. Assets containing clips reject this policy because changing child parent space requires real track conversion; the editor and backend both expose that gate rather than altering motion silently. Weight transfer and whole-asset rollback remain atomic. |
| 5.5 | 2026-08-13 | Extended leaf removal with an explicit reference policy. A selected replacement bone receives the removed palette identity; existing overlapping vertex influences are summed and palette slots compacted without losing normalized coverage. Bone-local animation tracks are deliberately not retargeted and require a separate discard confirmation. Skeleton, weights, and clips validate and commit atomically with rollback; bones with children remain blocked pending a descendant policy. |
| 5.4 | 2026-08-13 | Added strict transactional canonical bone removal with an explicit impact report. Bind snapshots now expose child count, weight-palette presence, weighted-vertex count, and animation-track count. Only a confirmed unreferenced leaf may be removed while retaining at least one bone; descendants, palette entries, weights, and tracks are never implicitly remapped or discarded. The backend repeats all GUI gates, validates 41–43, and the editor selects the former parent and supports whole-asset rollback. |
| 5.3 | 2026-08-13 | Added transactional canonical bone creation as the next safe-correction slice. The editor accepts a unique name, root/existing parent, and parent-relative translation; identity rotation/scale plus inherited radius/length provide explicit editable defaults. C++ allocates a new opaque nonzero stable ID independent of name, appends parent-first, revalidates 41–43, preserves existing weights/tracks, returns/selects the new index, and integrates with whole-asset rollback. Chain/mirror tooling remains Milestone 4. |
| 5.2 | 2026-08-13 | Reserved direct viewport bone editing as an explicit editor-refinement phase after safe structural mutation. Numeric TRS remains the precise foundation; future mouse manipulation must add synchronized picking, local/global translation/rotation/scale gizmos, constrained drag/snapping/cancel, continuous preview, and exactly one transactional 41–43 commit/rollback boundary per completed gesture. Bind gizmos remain distinct from future animated-pose gizmos. |
| 5.1 | 2026-08-13 | Added transactional parent-relative bind correction for the selected bone: translation, quaternion rotation, scale, radius, and length. Quaternion input is normalized; zero quaternion, singular/non-finite transforms, negative metadata, and invalid 41–43 candidates reject without mutation. Stable IDs remain intact, subtree movement is explicit, runtime/gizmos rebuild after commit, and the shared whole-asset rollback covers the operation. |
| 5.0 | 2026-08-13 | Promoted the existing one-level file snapshot into a skeleton-aware rollback boundary shared by Bind Pose Contract and Skin Weight Lab. Rename/reparent stage a complete 41–43 asset snapshot and commit it to history only after successful mutation; rejection preserves the prior history entry. Revert now rebuilds canonical report, hierarchy, preview, gizmos, selections, and allowed-bone state rather than restoring only weight-lab data. |
| 4.9 | 2026-08-13 | Added transactional canonical reparent. The selected bone can target root or another bone with explicit preserve-global (default) or preserve-local policy. Self-parent/cycles, singular parents, and preserve-global shear are rejected; source bones are stably reordered parent-first and 41–43 revalidate before commit. Stable IDs preserve weights/tracks, and fixtures cover cycle rejection plus child-to-root global-bind preservation. |
| 4.8 | 2026-08-13 | Fixed large-rig hierarchy clipping: the expanded bind tree now lives in a bordered 300px child region with its own scrollbar, while Selected Bone details remain outside and accessible below it. A 67-bone expansion no longer appears to terminate at the detail-panel separator. |
| 4.7 | 2026-08-13 | Made bind-tree selection highlight the bone rather than only its joint. Each parent-to-child cylinder is now keyed by the child stable `boneId`; selecting a non-root colors both its joint and incoming segment cyan, while roots correctly highlight only their joint. Rename remains stable because visual lookup no longer depends on the editable name. |
| 4.6 | 2026-08-13 | Fixed the selected-bone rename field crash: this Lua ImGui binding owns its dynamic buffer and accepts `InputText(label, text, flags)`, so the erroneous C++-style buffer-size argument `128` was interpreted as an invalid flag and unwound past the editor window's `End()`. The field now passes explicit `ImGuiInputTextFlags_None`. |
| 4.5 | 2026-08-13 | Added the first canonical bind mutation: transactional selected-bone rename. C++ copies/recompiles section 41 and revalidates type-42 weights and type-43 animations before commit; empty/duplicate names fail without mutation. Stable `boneId` preserves weight palettes and track targets, with save/reload coverage. |
| 4.4 | 2026-08-13 | Began safe bind editing with the read-only navigation foundation: Bind Pose Contract now renders the canonical parent/child hierarchy as a real multi-root tree, marks nodes with diagnostics, selects by canonical array index/stable identity, highlights the selected joint in the gizmo, and shows one separate technical detail/matrix panel instead of expanding a flat list of every bone. |
| 4.3 | 2026-08-13 | Fixed ×100 canonical scaling on the real 67-bone Mixamo rig. Bind-identity validation now bounds float matrix-product roundoff from the inverse/global bind operands instead of comparing the cancellation result only against unit magnitude; the observed 1.52587891e-5 root residual is accepted without weakening structural validation. Mesh Debug now surfaces scale failures in its UI as well as the terminal. |
| 4.2 | 2026-08-13 | Added transactional positive-uniform skeletal asset scaling. Whole-mesh Mesh Debug scaling now updates all geometry frames, canonical bind translations, bone radius/length, clip translations, and physics bounds, recompiles inverse bind, validates before commit, and remains coherent for runtime preview and FBX export. Partial scaling stays geometry-only; negative/non-uniform complete skeletal scaling is rejected. |
| 4.1 | 2026-08-13 | Physically deleted the isolated `#if 0` section-11/40 implementations and fixtures. Also reduced `MESH_MBM_DEBUG::scaleFrame` to its geometry-only contract, removing the unused legacy skeleton-sync/error parameters and the corresponding Lua argument. |
| 4.0 | 2026-08-13 | Removed the compiled/public section-11/40 model: enum members, payload structs/serializers, Mesh Debug C++ APIs and PIMPL storage, Lua callbacks, and legacy compiler/validator declarations. Numeric IDs and layouts remain documentation-only history; canonical 41–43 are the sole skeletal contract. |
| 3.9 | 2026-08-13 | Retired the exploratory 11/40 fixture group from the active foundation suite. Equivalent and broader canonical tests remain active, leaving `compileLegacySkeleton`/`validateLegacyWeights` with no compiled consumer outside their own implementation. |
| 3.8 | 2026-08-13 | Removed `refreshSkeletonBindReport()` and the duplicate bind-report cache. Read-only bind getters now consume the canonical compiled skeleton validated during load directly. |
| 3.7 | 2026-08-13 | Removed the final bind-report compatibility fallback and runtime intermediate scratch storage for 11/40. Bind reports are unconditionally canonical and require section 41; the legacy compiler remains referenced only by pre-canonical test fixtures pending their conversion. |
| 3.6 | 2026-08-13 | Removed active read/write persistence for exploratory sections 11/40. Both runtime and debug loaders reject them, Mesh Debug save excludes them from section count and output, and generic mesh metadata inspection now recognizes canonical section 41. |
| 3.5 | 2026-08-13 | Corrected canonical reverse-FBX coordinates after real 67-bone testing exposed disconnected and misdirected bones. Reverse export now undoes the import reflection, restores triangle winding, and recovers bone axes/roll through full inverse matrix conjugation rather than transforming basis rows independently. All 67 source bone axes match within 1.28e-7. |
| 3.4 | 2026-08-13 | Migrated Mesh Debug → FBX export to canonical bind-report matrices and type-42 weights. Blender reconstructs global bone axis/roll directly from each global bind matrix, so the reverse interchange path no longer consumes sections 11/40. |
| 3.3 | 2026-08-13 | Retired the Mesh Debug Bone product surface and destructive legacy-weight removal controls. The node/window are no longer called, stale gizmos are cleaned defensively, and Mesh Info statistics now inspect canonical type-42 weights. Legacy implementation and persistence remain temporarily for the next physical-deletion gate. |
| 3.2 | 2026-08-12 | Migrated every Skin Weight Lab read/write path to canonical type-42 weights. New narrow C++/Lua operations resolve UI names to stable bone IDs, enforce four unique positive normalized influences, validate the complete canonical record transactionally, and never create sections 11/40. Synthetic mutation/read/rejection preservation joins save/reload and Lorekeeper as acceptance evidence. |
| 3.1 | 2026-08-12 | Began Milestone 8 by migrating the Skeletal Animation Editor's bone list and bind gizmo from legacy `getTotalBone/getBone` to canonical bind-report bones only. Lorekeeper-style type-41 assets now supply hierarchy and global joint positions without a compatibility copy; legacy-only assets intentionally supply no editor skeleton. The audit identifies Skin Weight Lab's exploratory name-palette weight API as the next deletion blocker. |
| 3.0 | 2026-08-12 | Made skeleton visualization worktree-specific. Bind Pose Contract displays the bind skeleton automatically; Skin Weight Lab owns its checkbox/depth preference; Runtime Skeletal Preview hides the bind-only gizmo because it cannot truthfully represent both evaluated LBS/DQS instances. Animated runtime skeleton gizmos remain future work. |
| 2.9 | 2026-08-12 | Fixed shared skeleton visibility so Skin Weight Lab's analysis, proximity, and target-bone highlight spheres obey both the global skeleton toggle and worktree scope, including when a highlight is rebuilt while the skeleton is hidden. |
| 2.8 | 2026-08-12 | Replaced the accumulated single Skin Weight Lab panel with five mutually exclusive worktrees. Mesh and skeleton visibility remain shared; AABB/proximity selection, analysis markers, heatmaps, diagnostics, and weight operations are both displayed and rendered only in Skin Weight Lab. Bind diagnostics and runtime preview have dedicated worktrees, while animation authoring and brush painting are explicit reserved destinations. |
| 2.7 | 2026-08-12 | Added read-only side-by-side pose-stress comparison: LBS left, rigid DQS right, using separate runtime instances with mirrored clip controls, per-frame time synchronization, shared bind restoration, method readiness/rejection feedback, and camera reframing. Lorekeeper load/seek/pause/sync/restore passed an isolated engine smoke; interactive checkbox ergonomics still require a human pass. |
| 2.6 | 2026-08-12 | Added fixed-time Lorekeeper numeric parity for eight deterministically selected mixed-influence vertices. Production-shared GLES LBS/DQS output matches CPU positions/normals within RGBA8-aware tolerances; the remaining Milestone-7 work is editor pose-stress/diagnostic UX. |
| 2.5 | 2026-08-12 | Removed copied GLSL from the numeric parity harness. Production default LBS/DQS shaders and the readback test now consume the same private declarations/deformation generator; both parity methods and the 23-bone production DQS compile/link regression pass. |
| 2.4 | 2026-08-12 | Began the Milestone-7 numeric acceptance gate with a real GLES RGBA8/readback comparison of two-bone LBS and rigid DQS positions/normals against CPU references. Both methods pass explicit quantization-aware tolerances; production shader-source sharing and real-asset coverage remain. |
| 2.3 | 2026-08-11 | Added Auto to the runtime preview. It resolves once before load to DQS for wholly rigid content or LBS when bind/clip scale exists, and displays the requested method, resolved method, and reason. Explicit DQS stays strict. |
| 2.2 | 2026-08-11 | Exposed the delivered per-instance LBS/rigid-DQS runtime choice in the preview. Changing method rebuilds the preview and selects its shader before load; the report uses the chosen method's per-draw capacity. No backend selector, timeline, destructive skeleton editing, or silent scale fallback was added. |
| 2.1 | 2026-08-11 | Added the separately cached GLES2 rigid-DQS shader and real driver compile/link coverage. The editor remains LBS-only until the next gate connects per-instance method selection and an authored DQS palette draw. |
| 2.0 | 2026-08-11 | Started the DQS prerequisite behind Milestone 7: a tested private rigid pose-to-dual-quaternion palette now packs two `vec4` values per bone and rejects non-rigid transforms. No editor selector is exposed before the corresponding runtime shader exists. Reworded the LBS capacity display as a per-mesh-draw device limit rather than an ambiguous fraction. |
| 1.9 | 2026-08-11 | Extended the shared runtime preview with explicit bind-pose restoration and visible GLES2 LBS readiness/capacity evidence. Restoration deactivates the player and identity palette rather than seeking to zero; no timeline, DQS selector, or destructive skeleton operation was added. |
| 1.8 | 2026-08-10 | Began Milestone 7 integration early, after the Phase-4 runtime prerequisite became available: the Skin Weight Lab's preview mesh now uses the actual per-instance GLES2 LBS player for clip selection, play/restart, pause/resume, and bounded seek. The bind gizmo stays static and no Milestone-6 timeline/authoring UI was introduced. DQS choice, backend reporting, pose stress, and parity fixtures remain. |
| 1.7 | 2026-08-10 | Mesh Debug's Bone node/window and gizmo now inspect canonical section 41 directly through the canonical-first bind snapshot. The view exposes hierarchy and bind metadata read-only and deliberately does not populate or enable the destructive legacy skeleton model. |
| 1.6 | 2026-08-10 | The direct Blender/FBX importer now emits canonical sections 41–43 directly from armature bind matrices, vertex groups, and sampled poses, with deterministic IDs, local quaternion TRS, strict influence coverage, and no legacy 11/40 output. The accepted reflection is applied atomically to geometry, winding, normals, bind, and poses. The 67-bone Mixamo walk now produces one REST bind frame plus 67×32 track keys; canonical load and save/reload passed. |
| 1.5 | 2026-08-10 | Added canonical 41–43 writer round-trip with validation before file creation, correct `sectionCount`, deterministic section order, and save/reload coverage. It writes only canonical state already owned by Mesh Debug and deliberately does not convert the temporary legacy skeleton/weight representation. FBX-to-canonical import remains the prerequisite for permanent editor work. |
| 1.4 | 2026-08-10 | Completed canonical read support with type 43 clips/tracks/keys and final section presence/identity validation in both loaders. The valid fixture proves order-independent resolution; mismatch target/easing fixtures prove deterministic rejection. Writer/import remains the next prerequisite for the permanent editor shell. |
| 1.3 | 2026-08-10 | Added canonical type-42 weights to both loaders with order-independent skeleton/topology resolution and strict ID, palette, four-influence, coverage, and normalization validation. The permanent editor still waits for type 43 plus the canonical writer/import path. |
| 1.2 | 2026-08-10 | Began Milestone 1's canonical data dependency with the type-41 reader in both real loaders and deterministic valid/invalid/duplicate fixtures. Canonical weights, clips, cross-section validation, writer/import, and the permanent three-node shell remain pending. |
| 1.1 | 2026-08-10 | Made canonical-only delivery normative: the Mesh Debug skeleton/weight representation and bind-report bridge are temporary audit scaffolding, not compatibility requirements. Canonical readers/import must precede permanent editor work; legacy skeletal APIs/sections are removed and affected assets regenerated from FBX before delivery. |
| 1.0 | 2026-08-10 | Added the first non-mutating post-Milestone-0 integration: an explicit read-only `meshDebug` snapshot boundary and Bind Pose Contract panel for stable IDs, canonical local TRS, local/global/inverse-bind matrices, numeric errors, and structural diagnostics. This is groundwork for the future three-node shell, not destructive skeleton editing, pose preview, or timeline UI. |
| 0.9 | 2026-08-10 | Registered the provisional 32-frame Mixamo walking source and its frames 1/16/32 audit for branch development. It complements rather than replaces the rat bind/weight/topology fixture and remains gated on provenance plus final handedness-converted expectations. |
| 0.8 | 2026-08-09 | Recorded M0.7's reproducible raw-cluster/rest-pose FBX audit and accepted the atomic handedness conversion contract. The rat is approved for bind/weight/topology evidence but disqualified as an animated-pose fixture because its action has zero sampled pose delta. No importer reflection or editor mutation was implemented. |
| 0.7 | 2026-08-09 | Completed M0.6's headless synthetic regression coverage for hierarchy, bind identity, weights, clip validation/sampling, easing, loop/clamp, antipodality, scale/shear, and normalized scale-1/scale-100 pose equivalence. No editor UI or asset mutation was introduced. |
| 0.6 | 2026-08-09 | Adopted M0.5's versioned persistence decision: new canonical skeleton, runtime weight, and skeletal animation sections linked by `skeletonId`, with readers and corruption tests required before any writer. Legacy editor sections retain their meaning and the new values remain reserved but unimplemented. |
| 0.5 | 2026-08-09 | Added M0.4's private skeletal clip/track/key validation and pure sampling foundation, including bind-local channel fallback, easing, antipodal quaternion interpolation, loop/clamp semantics, and local-to-global evaluation. This is not yet an editor timeline, player, persisted format, or deformation preview. |
| 0.4 | 2026-08-09 | Added the non-mutating M0.3 legacy-weight validation report, separating structural conversion failures from measured legacy editor-data quality findings. It resolves palette names only against an explicitly compiled skeleton and does not alter ordinary mesh loading or repair assets. |
| 0.3 | 2026-08-09 | Recorded the initial shared M0.1/M0.2 implementation: private explicit bind-pose conversion, stable IDs, local/global and inverse-bind validation, scale diagnostics, and synthetic fixtures. Legacy v1/v2 skeletons are not automatically promoted during ordinary runtime mesh loading. No destructive skeleton editing, clips, timeline, or deformation was added. |
| 0.2 | 2026-08-09 | Adopted the consolidated Milestone 0 contracts: row-vector local/global math, stable bone IDs, local quaternion TRS, derived inverse bind, scale diagnostics, distinct skeletal clips, legacy conversion, numerical fixtures, executable M0.1-M0.7 work packages, and the FBX handedness/cluster-bind gate. |
| 0.1 | 2026-08-09 | Initial plan: three-node product shape; Mesh Debug Bones audit boundary; Skeleton/Bind Pose and Animation scopes; local and imported workflows; shared contracts; staged migration; risks, gates, fixtures, and acceptance criteria. |
