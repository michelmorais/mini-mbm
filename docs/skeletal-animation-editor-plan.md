# Skeletal Animation Editor — Product and Migration Plan

Document version: **0.9**
Status: **Product contracts consolidated; shared Milestone 0.1–0.7 foundation audited**
Last updated: **2026-08-09**

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

## 2. Product shape: three nodes

The editor is the product container. Its primary navigation will contain exactly three sibling
nodes:

1. **Skin Weight Lab** — select vertices; inspect, normalize, rigid-bind, smooth, diagnose, and
   preserve stored weights. This workflow is already delivered and must be moved into a node without
   changing its accepted behavior.
2. **Skeleton / Bind Pose** — inspect hierarchy and bind data, diagnose structural or transform
   problems, safely adjust an imported skeleton, and eventually author a skeleton locally.
3. **Animation** — create/import clips, edit bone tracks and keyframes, use a timeline, compose and
   preview poses, and validate the result through runtime LBS/DQS.

Deformation preview and backend capabilities are shared services or panels used by these nodes, not
additional top-level nodes. File operations, viewport, camera, skeleton selection, diagnostics, and
history should also be shared where their meanings are identical.

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

1. Skeletal Animation Editor has three primary nodes: Skin Weight Lab, Skeleton / Bind Pose, and
   Animation.
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
- Add/remove bones with affected-reference reporting and one-level rollback at minimum.
- Offer derivation helpers only as previewable, named heuristics.

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

## 9. Cross-node rules

1. **Single asset context.** All three nodes operate on the same loaded mesh, skeleton, weights,
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
- tail, length, and radius are visualization/authoring metadata, not deformation transforms;
- skeletal clips are distinct bone-ID-targeted resources, even when easing/player services are
  shared with articulated animation;
- legacy version-1/2 skeleton globals and name-palette weights are compiled and diagnosed without
  silent rewriting;
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
- Convert current global Euler skeleton data to canonical local TRS without mutating source assets.
- Implement M0.3 structural/bind/weight/scale diagnostics and inverse-bind validation.
- Define M0.4 bone-ID-targeted clip structs and pure deterministic sampling without timeline UI.
- Complete M0.5 persistence layout design before any skeleton/clip writer is implemented.
- Add M0.6 synthetic numeric fixtures and scale-1/scale-100 comparisons.
- Complete M0.7 FBX cluster-bind/handedness audit before promoting the rat to a normative fixture.

Exit: a legacy skeleton compiles and round-trips global→local→global within tolerance, inverse bind
is identity at bind pose, invalid identities/references/transforms produce deterministic reports,
and a synthetic clip samples to expected local/global transforms. No Skeleton/Animation UI field
may be introduced with an undefined storage or runtime meaning.

### Milestone 1 — Three-node editor shell

- Introduce Skin Weight Lab, Skeleton / Bind Pose, and Animation navigation.
- Move the accepted Skin Weight Lab GUI/state into its node without behavior regression.
- Establish shared asset, viewport, camera, selection, status, and modified-state services.
- Show unavailable nodes with capability explanations while their data/runtime support is absent.

Exit: all accepted Skin Weight Lab tests still pass inside the new navigation.

### Milestone 2 — Skeleton inspection and bind validation

- Add hierarchy/tree and viewport inspection for an existing skeleton.
- Show source/derived bind information and structural diagnostics.
- Add identity bind validation against controlled fixtures when pose evaluation is available.
- Preserve imported data without writes in inspection mode.

Exit: the rat and synthetic skeleton fixtures can be inspected and diagnosed reproducibly.

### Milestone 3 — Safe bind corrections

- Add audited rename, reparent, joint/orientation correction, add/remove, and derivation helpers.
- Report and preserve or deliberately remap weight references.
- Introduce a skeleton-aware snapshot/rollback boundary.
- Keep import/export-only conversion controls outside routine bind editing.

Exit: a corrected imported skeleton round-trips without unknown weight or hierarchy references.

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

- Deliver timeline, tracks, keyframes, easing, playback, loop, speed, and composition controls.
- Reuse articulated-animation interaction vocabulary where semantics match.
- Keep bind editing inaccessible while ordinary pose/keyframe editing is active.

Exit: a clip can be authored, saved, reopened, and sampled deterministically inside Mini MBM.

### Milestone 7 — Shared LBS/DQS preview

- Preview clips with the shared runtime pose evaluator and deformation implementation.
- Expose LBS/DQS choice, scale restrictions/fallbacks, normals, and backend capability information.
- Add pose-stress comparison and bind-pose restoration.

Exit: editor and runtime produce matching reference vertices/normals for the same clip and time.

### Milestone 8 — Mesh Debug migration

- Retain or relocate only genuinely mesh-debug/interchange operations.
- Remove duplicated Skeleton/weight authoring after the new workflows reach parity and fixtures pass.
- Keep compatibility entry points or migration notes for existing armature templates as needed.

Exit: there is one canonical implementation for each skeleton, weight, and animation responsibility.

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
| Three nodes own separate copies of asset state | Cross-node corruption and stale views | One asset context and explicit invalidation rules. |

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

Milestone 0 is ready for technical investigation. Implementation of the three-node shell may proceed
once shared editor state boundaries are identified, but Skeleton mutation and clip persistence must
not outrun the data/correctness gates in Section 10. Each later milestone requires an executable
verification plan tied to both synthetic fixtures and the alien rat.

## 17. Change log

| Version | Date | Change |
|---|---|---|
| 0.9 | 2026-08-10 | Registered the provisional 32-frame Mixamo walking source and its frames 1/16/32 audit for branch development. It complements rather than replaces the rat bind/weight/topology fixture and remains gated on provenance plus final handedness-converted expectations. |
| 0.8 | 2026-08-09 | Recorded M0.7's reproducible raw-cluster/rest-pose FBX audit and accepted the atomic handedness conversion contract. The rat is approved for bind/weight/topology evidence but disqualified as an animated-pose fixture because its action has zero sampled pose delta. No importer reflection or editor mutation was implemented. |
| 0.7 | 2026-08-09 | Completed M0.6's headless synthetic regression coverage for hierarchy, bind identity, weights, clip validation/sampling, easing, loop/clamp, antipodality, scale/shear, and normalized scale-1/scale-100 pose equivalence. No editor UI or asset mutation was introduced. |
| 0.6 | 2026-08-09 | Adopted M0.5's versioned persistence decision: new canonical skeleton, runtime weight, and skeletal animation sections linked by `skeletonId`, with readers and corruption tests required before any writer. Legacy editor sections retain their meaning and the new values remain reserved but unimplemented. |
| 0.5 | 2026-08-09 | Added M0.4's private skeletal clip/track/key validation and pure sampling foundation, including bind-local channel fallback, easing, antipodal quaternion interpolation, loop/clamp semantics, and local-to-global evaluation. This is not yet an editor timeline, player, persisted format, or deformation preview. |
| 0.4 | 2026-08-09 | Added the non-mutating M0.3 legacy-weight validation report, separating structural conversion failures from measured legacy editor-data quality findings. It resolves palette names only against an explicitly compiled skeleton and does not alter ordinary mesh loading or repair assets. |
| 0.3 | 2026-08-09 | Recorded the initial shared M0.1/M0.2 implementation: private explicit bind-pose conversion, stable IDs, local/global and inverse-bind validation, scale diagnostics, and synthetic fixtures. Legacy v1/v2 skeletons are not automatically promoted during ordinary runtime mesh loading. No destructive skeleton editing, clips, timeline, or deformation was added. |
| 0.2 | 2026-08-09 | Adopted the consolidated Milestone 0 contracts: row-vector local/global math, stable bone IDs, local quaternion TRS, derived inverse bind, scale diagnostics, distinct skeletal clips, legacy conversion, numerical fixtures, executable M0.1-M0.7 work packages, and the FBX handedness/cluster-bind gate. |
| 0.1 | 2026-08-09 | Initial plan: three-node product shape; Mesh Debug Bones audit boundary; Skeleton/Bind Pose and Animation scopes; local and imported workflows; shared contracts; staged migration; risks, gates, fixtures, and acceptance criteria. |
