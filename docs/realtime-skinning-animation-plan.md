# Real-Time Skinning Animation — LBS and DQS Plan

Document version: **0.5**
Status: **Weight-authoring Phase 2 started; runtime skinning not implemented**
Last updated: **2026-08-06**

## 1. Purpose

This document is the versioned reference for adding skeletal animation and real-time vertex
skinning to Mini MBM. It plans Linear Blend Skinning (LBS) and Dual Quaternion Skinning (DQS) from
the initial design so that file data, pose evaluation, diagnostics, editor workflows, and backend
interfaces do not accidentally encode only one method.

The companion [Skin Weight Lab Plan](skin-weight-lab-plan.md) owns weight authoring, local repair,
and validation. Both are intended to live in a standalone **Real-Time Skinning Editor** rather than
continue inflating Mesh Debug.

This is a planning document, not a promise that LBS and DQS ship simultaneously. It separates
confirmed facts, decisions, hypotheses, and open questions.

## 2. Current State and Confirmed Facts

- Mini MBM persists an editor-only bind-pose skeleton and up to four named influences per frame-1
  vertex.
- The runtime currently renders pre-baked mesh frames; it does not evaluate a bone hierarchy and
  deform vertices from skin weights per frame.
- Current backends include OpenGL ES 2, DirectX 9, and Metal. Basic skeletal skinning does not by
  itself require replacing these APIs, but their buffer, shader, and palette limits differ.
- Linux currently exercises the OpenGL ES path and is a practical first development platform.
- Mesh Debug already implements articulated animation with hierarchical parts/pivots, named clips,
  position/rotation/scale tracks, quaternion runtime rotation, easing, timeline controls, loop,
  speed, priority, weight/fade, and Absolute/Additive composition.
- Articulated animation moves mesh parts. Skeletal skinning instead evaluates bones and blends
  transformed vertex positions/normals. The existing workflow is a reference, not an interchangeable
  storage format.
- `docs/articulated-animation.md` never existed in Git history. A similarly named implementation
  plan, `docs/articulated-animation-plan.md`, existed from commit `e3f41bf` until it was removed in
  `729c193` after the feature was refined. The broken Lua API link introduced during that transition
  was repaired and an implementation-backed [Articulated Animation guide](articulated-animation.md)
  now documents the current feature.

### Initial rat study bundle

The first versioned input set is under `src/test-lib/`:

| File(s) | Runtime-skinning study role |
|---|---|
| `T-BONE-rato-from-mixamo.fbx` | Source-of-comparison for imported bind transforms, skin clusters, hierarchy, animation stack, and animated reference poses. |
| `T-BONE-rato-from-mixamo.msh` | Candidate Mini MBM-side skeleton/weight baseline; first confirm which imported data survived conversion before using it for CPU/GPU comparisons. |
| `T-BONE-rato-from-mixamo_armature.lua` | 41-bone hierarchy/placement reference for bind-pose reconstruction and import consistency checks. |
| `Image_0.jpg`, `Image_1.jpg`, `Image_3.jpg`, `normal.png` | Stable material inputs so geometry, normal, and deformation comparisons are not obscured by missing resources. |

These files are suitable for discovery and visual baseline work now. They become runtime acceptance
fixtures only after expected clip names/timestamps, bind-pose values, selected reference vertices,
expected deformed results/tolerances, and asset identity are recorded. Additional synthetic fixtures
remain necessary for antipodality, single-bone equivalence, and non-uniform-scale behavior.

## 3. Problem and Objective

The engine needs a single trustworthy pose pipeline that can feed multiple deformation methods and
multiple graphics backends. Without that separation, an editor-only preview can disagree with the
runtime, GLES limits can leak into the whole product design, and DQS can be added later only through
duplicated animation logic.

The objective is:

1. evaluate clips and a bone hierarchy once on the CPU;
2. produce a backend-neutral skinning palette from the evaluated pose and bind pose;
3. deform the same mesh through a selectable LBS or DQS path;
4. make capability limits and fallbacks explicit;
5. reuse the exact deformation path in the editor preview and runtime;
6. validate results against CPU references and cross-backend fixtures.

## 4. Product and Architecture Principles

1. **One pose, multiple deformation methods.** Clip sampling and hierarchy evaluation must not be
   duplicated between LBS and DQS.
2. **Bind pose is an invariant.** Evaluating the bind pose must reproduce the undeformed mesh within
   a defined numeric tolerance.
3. **No silent fallback.** If the requested method, palette size, or transform semantics are not
   supported, report it. An explained user-selected fallback is acceptable.
4. **Do not design the product around the weakest backend.** Backend limits affect capabilities and
   delivery milestones, not whether stronger backends may support the feature.
5. **Linux first does not mean GLES-shaped architecture.** Linux/GLES is suitable for early feedback,
   while shared CPU contracts and serialized data remain backend-neutral.
6. **Editor and runtime share the deformation implementation.** Temporary diagnostic prototypes may
   precede it, but must not become a permanent second skinning system.
7. **LBS is the reference baseline; DQS is a quality option.** DQS addresses rotational volume loss,
   not bad topology, wrong weights, or every scaling problem.

## 5. Shared Conceptual Model

### Bind pose

The authored rest transforms of the skeleton and the mesh positions associated with them. Each bone
requires a stable global bind transform and inverse global bind transform.

For bone `i`, the skin transform is conceptually:

```text
skin_i = current_global_i * inverse(bind_global_i)
```

At the bind pose, `current_global_i == bind_global_i`, so every `skin_i` must be identity. A failure
here indicates hierarchy, transform-order, coordinate-system, or imported-bind data problems before
weights are considered.

### Animated pose

Local bone transforms sampled from one or more clips and composed through the hierarchy into current
global transforms. Composition semantics include loop, speed, priority, blend weight/fade, and
Absolute/Additive behavior, subject to the decisions in Section 9.

### Influences

Each vertex has up to four bone indices and normalized weights. Names may remain useful in authoring
and interchange, but runtime data needs validated indices into a stable skeleton/palette mapping.

### Deformation boundary

CPU responsibilities include clip sampling, composition, hierarchy evaluation, bind validation, and
palette construction. GPU responsibilities include per-vertex influence blending and position/normal
deformation. A CPU deformation reference exists for tests and diagnostics, not as the intended
high-volume runtime path.

## 6. Linear Blend Skinning

LBS blends positions transformed by each influencing bone's skin matrix. It is the initial reference
because it is straightforward to inspect, compare on CPU/GPU, and use as a fallback for transforms
that basic DQS cannot represent.

Known tradeoff: matrix blending can lose volume around large twists or bends, producing the familiar
“candy-wrapper” collapse. Better weights help but do not eliminate the mathematical limitation.

A compact rigid/affine palette commonly uses three `vec4` values per bone; a full `mat4` uses four.
The final representation remains a technical-design decision because normal transformation and scale
support affect the choice.

## 7. Dual Quaternion Skinning

DQS blends rigid bone transformations as dual quaternions, commonly two `vec4` values per bone. It
generally preserves volume better than LBS under rotation and is therefore especially relevant to
the rat's nearly invisible neck, shoulders, wrists, and tail.

### Antipodality invariant

Quaternion `q` and `-q` represent the same rotation but cancel if naively added. Before accumulating
weighted dual quaternions, each influence must be sign-aligned with a deterministic reference
quaternion. The blended result must then be normalized. Tests must include equivalent transforms
encoded with opposite signs.

This correction is part of DQS correctness, not an optional visual enhancement.

### Scale and shear boundary

Basic DQS represents rigid rotation and translation. It does not faithfully represent arbitrary
non-uniform scale or shear. The first implementation must detect unsupported transform content and
either:

- explicitly use LBS for that mesh/clip after user or application selection;
- reject DQS with a useful diagnostic; or
- later implement a documented two-phase method that handles scale separately.

Silently dropping scale is not acceptable. Corrective shapes and bulge preservation beyond basic
DQS are later research topics.

## 8. Backend Strategy

| Backend | Initial planning concern | Direction |
|---|---|---|
| OpenGL ES 2 (Linux/Android) | Vertex-uniform capacity is tightly constrained and device limits must be queried. Other scene uniforms reduce the space available to the bone palette. | Start development and measurement here; expose the effective palette limit; investigate partitioning or another transport only when fixtures require it. |
| Metal (macOS/iOS) | Palette transport and synchronization through buffers; shader parity with the reference implementation. | Use an explicit buffer-backed palette and validate after the shared CPU model stabilizes. |
| DirectX 9 (Windows) | Shader-model instruction/constant budgets and vertex declaration integration. | Validate an explicit supported shader profile and palette capacity; do not assume desktop hardware removes DX9 limits. |

### Platform policy

- Development may begin on Linux because it offers the fastest current engine feedback loop.
- Metal and DirectX are explicit delivery milestones, not emergency destinations used only after a
  GLES implementation fails.
- A GLES device with insufficient capacity must fail clearly or use an explicitly selected strategy;
  it must not disable skeletal animation globally for Metal/DirectX builds.
- “Always offer DQS on GLES” is not yet a decision. Its two-`vec4` rigid palette is attractive, but
  effective limits, shader instructions, scale semantics, and real devices must be measured.
- Likewise, LBS must not be avoided on GLES by assumption. Small skeletons may fit comfortably.
- The capability report should include requested method, supported method(s), effective bone count,
  palette transport, relevant transform restrictions, and reason for rejection/fallback.

Open questions include palette partitioning, texture-based palettes where available, CPU fallback
for very small/diagnostic cases, and minimum supported bone counts per platform.

## 9. Relationship to Articulated Animation

The existing articulated system provides a useful product vocabulary and proven interaction model:

- hierarchy, parts/pivots, and named clips;
- position, quaternion rotation, and scale channels;
- authored Euler angles converted to quaternion runtime data;
- easing and timeline seek/playback;
- loop, speed, priority, weight, and fade;
- Absolute and Additive composition.

The new editor should visibly echo these concepts where their meanings match. A user familiar with
articulated clips should not need to relearn timeline and composition controls for skeletal clips.

However, reuse must happen at the appropriate layer. Skeletal targets are bones, bind transforms are
mandatory, and the evaluated pose deforms shared vertices through weights. The plan must separately
decide which clip/player structures can become shared services, which semantics are merely aligned,
and which file sections remain distinct.

## 10. Standalone Editor Shape

The proposed Real-Time Skinning Editor should eventually contain these workspaces:

1. **Skeleton and Bind Pose** — hierarchy, joints, parentage, local/global transforms, inverse-bind
   validation, add/remove bones, and bind-pose diagnostics.
2. **Skin Weight Lab** — selection, rigid regions, transitions, smoothing, diagnostics, regeneration
   research, and rollback as specified in its companion plan.
3. **Clips and Timeline** — clip list, bone tracks, P/R/S channels, easing, timeline playback/seek,
   loop, speed, and composition controls aligned with articulated animation.
4. **Deformation Preview** — LBS/DQS selection or comparison, stress poses, normals, heat maps, and
   explicit scale/fallback diagnostics.
5. **Backend Capabilities** — current backend, available methods, effective palette size, active
   transport, warnings, and reproducible diagnostic export.

The standalone editor is a product decision. Its internal code-sharing boundary is not yet decided.
The preferred direction is shared engine/editor services with incremental migration, not a copy of
Mesh Debug's large Lua implementation.

## 11. Delivery Plan

### Phase 0 — Fixtures, documentation, and invariants

- Preserve and characterize the initial `src/test-lib/T-BONE-rato-from-mixamo.*` bundle and textures.
- Derive rat bind-pose and animated-pose comparison points without modifying the source baseline.
- Add separate minimal humanoid, rigid-cavity, tail, antipodal-quaternion, single-bone, and
  non-uniform-scale fixtures where the rat bundle cannot isolate one invariant.
- Document coordinate conventions, transform order, bind data, weight/index mapping, and tolerances.
- Keep the dedicated articulated-animation guide, Lua API, and normative Mesh V11 format references
  synchronized as the shared animation vocabulary evolves.

Exit: bind-pose and expected-deformation fixtures are reproducible.

### Phase 1 — Standalone editor foundation and bind validation

- Establish the editor shell and shared mesh/skeleton access boundary.
- Visualize local/global bind transforms and inverse-bind results.
- Diagnose invalid hierarchy, missing bind data, cycles, unknown influences, and non-identity bind
  deformation.

Implementation note: version 6.45.0 adds `editor/realtime_skinning_editor.lua` with the first Skin
Weight Lab workspace and reuses the existing narrow `meshDebug` data API. It does not yet implement
bind/inverse-bind evaluation, clip playback, LBS, or DQS; those phase boundaries remain unchanged.
Version 6.46.0 adds local rigid-core/transition-shell weight blending to that workspace. This is
authoring data for future runtime skinning, not pose evaluation or an LBS/DQS deformation preview.

### Phase 2 — Shared pose evaluation

- Sample P/R/S tracks and compose the hierarchy on CPU.
- Decide concrete reuse/alignment with articulated clip, easing, timeline, priority, and
  Absolute/Additive semantics.
- Produce a backend-neutral evaluated pose.

### Phase 3 — CPU references

- Implement test-oriented CPU LBS and rigid DQS references.
- Add antipodality, normalization, scale-detection, normal, and bind-pose tests.
- Establish expected cross-method results for single-bone rigid motion.

### Phase 4 — Linux/OpenGL ES runtime and editor preview

- Implement measured palette capability reporting.
- Add GPU LBS and DQS incrementally against CPU references.
- Use the same runtime deformation in the editor preview.
- Validate the rat and small skeletons before investigating palette expansion.

### Phase 5 — Metal validation and delivery

- Implement buffer-backed palettes and shader parity.
- Compare reference vertices/normals and visual fixtures with Linux results.

### Phase 6 — DirectX 9 validation and delivery

- Select and document the supported shader profile and effective palette capacity.
- Compare reference vertices/normals and visual fixtures with other backends.

### Phase 7 — Runtime animation surface

- Expose clip playback, seek, loop, speed, priority, weight/fade, and composition through the engine
  and appropriate Lua API.
- Define resource ownership, instance state, and multi-mesh skeleton sharing.

### Phase 8 — Advanced research

- Palette partitioning and larger skeleton transport.
- Two-phase DQS for scale/stretch.
- Corrective shapes, bulge compensation, and optional CPU/compute alternatives where appropriate.
- Tail procedural animation/physics integration.

## 12. Verification and Acceptance Invariants

- Bind-pose evaluation reproduces original positions and normals within documented tolerance.
- One bone at weight `1.0` produces equivalent rigid LBS and DQS results.
- Effective weights are finite, non-negative, limited to four, and normalized.
- An antipodal DQS fixture produces the same result whether an equivalent input quaternion is stored
  as `q` or `-q`.
- Unsupported non-uniform scale/shear is never silently discarded.
- GPU output is compared with CPU reference vertices/normals for every backend.
- LBS/DQS comparison uses the same mesh, weights, evaluated pose, camera, and time.
- A capability failure states the method, required/effective palette size, backend, and reason.
- Runtime and editor preview use the same deformation path once Phase 4 is complete.
- The rat tests include head turn/tilt, torso plus head, cavity rigidity, shoulders, wrists, and tail.

## 13. Risks

| Risk | Impact | Mitigation direction |
|---|---|---|
| Wrong bind or coordinate convention | Every animation deforms incorrectly | Identity bind invariant and imported fixture comparisons |
| DQS antipodality omitted | Flips, cancellation, or collapsed vertices | Deterministic sign alignment and dedicated fixture |
| DQS marketed as a universal quality fix | Bad weights/topology remain broken | Method comparison plus weight/topology diagnostics |
| Scale semantics differ between methods | Clips change shape or lose authored intent | Detect content; explicit method restriction/fallback; two-phase research |
| GLES limit becomes a global architecture limit | Better backends lose functionality | Capability-driven backends and backend-neutral shared model |
| Linux-only success hides mobile GLES constraints | Device-specific failure | Query/report limits and test representative Android devices |
| Editor duplicates runtime math | Preview and game disagree | Shared deformation path and CPU reference suite |
| Articulated and skeletal formats are conflated | Bind/weight semantics become fragile | Reuse concepts/services selectively; keep explicit data boundaries |
| Mesh Debug code is copied into a second large editor | Long-term drift and maintenance cost | Extract shared services and migrate in phases |

## 14. Decisions Taken

1. LBS and DQS are planned from the initial data and architecture design.
2. LBS is the correctness/reference baseline; DQS is a selectable quality method, not a replacement
   for weight authoring.
3. Skin Weight Lab and skeletal animation belong in a standalone Real-Time Skinning Editor.
4. Editor preview and runtime must converge on one deformation implementation.
5. Bind-pose identity, DQS antipodality correction, and explicit scale handling are required
   correctness contracts.
6. Backend limitations are reported as capabilities and must not silently select another method.
7. Linux/GLES may lead implementation, with Metal and DirectX represented by planned milestones.
8. Existing articulated animation is the UX/domain reference, while skeletal data remains distinct.

## 15. Hypotheses to Validate

1. DQS's smaller rigid palette makes it a useful preferred option on some GLES2 devices.
2. Four influences per vertex provide an acceptable quality/performance boundary for the target
   characters.
3. Current articulated clip/player concepts can be extracted or generalized without destabilizing
   pre-baked and articulated animation.
4. Linux-first work can establish shared contracts without accidentally depending on GLES-specific
   shader representations.
5. The standalone editor can reuse camera, selection, timeline, and visualization services rather
   than duplicating Mesh Debug.
6. CPU reference deformation is fast enough for tests and selected-region diagnostics.

## 16. Open Questions

### Data and animation

1. Should skeletal clips extend existing mesh animation resources or use a distinct resource type?
2. Which articulated player semantics are shared exactly, and which are only UX-aligned?
3. What are the precise local-transform order, handedness, quaternion convention, and bind storage?
4. How are root motion, multiple roots, attachments, and skeleton sharing represented?
5. Are bone scale and shear permitted in the first runtime release?

### Backend capability

6. What minimum bone count must each backend guarantee after reserving other vertex uniforms?
7. Should meshes be partitioned by palette when they exceed that count?
8. Is any texture/buffer palette alternative viable on the actual GLES2 device baseline?
9. When DQS cannot represent a clip's scale, is fallback selected per mesh, clip, pose, or draw?
10. Should the runtime ever auto-fallback if an application explicitly opts in, or always fail?

### Editor and delivery

11. What is the editor's final public name?
12. Which Mesh Debug capabilities migrate, remain, or become shared modules?
13. Is LBS/DQS comparison a toggle, split view, overlay/heat map, or all three?
14. Which rat/animation files may be committed as canonical fixtures?
15. Which articulated clip/player concepts should become shared services instead of remaining only
    aligned at the UX level?

## 17. Out of Scope for the Initial Runtime Delivery

- Guaranteed high-quality automatic weights for arbitrary meshes.
- Brush-based Blender-equivalent weight painting.
- Automatic animation of custom tail bones from standard Mixamo clips.
- Corrective blend shapes and muscle simulation.
- Compute-shader skinning as a required baseline.
- Unlimited skeleton sizes on every backend.
- Silent approximation of unsupported transforms.

## 18. Technical References

- Kavan et al., [Skinning with Dual Quaternions](https://users.cs.utah.edu/~ladislav/kavan07skinning/kavan07skinning.html).
- Ladislav Kavan et al., [Skinning with Dual Quaternions — overview, limitations, paper, and reference code](https://users.cs.utah.edu/~ladislav/dq/index.html). This is the practical reference for GPU-oriented DQS, its relationship to LBS, antipodality/flipping concerns, and the optional two-phase treatment of scale and shear.
- Le and Hodgins, [Real-time Skeletal Skinning with Optimized Centers of Rotation](https://binh.graphics/papers/2016s-cor/). This is a later comparison and possible research direction that targets both LBS candy-wrapper artifacts and DQS bulging while retaining the existing weights/animation pipeline.
- Khronos, [OpenGL ES 2.0 specification](https://registry.khronos.org/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf).
- Microsoft, [Shader Model 2 (Direct3D 9)](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-2-0).
- Apple, [Metal resource fundamentals](https://developer.apple.com/documentation/metal/resource_fundamentals).

References constrain later technical design; measured Mini MBM backend behavior and project fixtures
remain required before choosing palette sizes or fallbacks.

## 19. Change Log

| Version | Date | Change |
|---|---|---|
| 0.5 | 2026-08-06 | Recorded the editor's Phase-2 rigid-core/transition-shell authoring slice while explicitly preserving bind evaluation, pose evaluation, and LBS/DQS preview as future runtime work. |
| 0.4 | 2026-08-06 | Recorded the standalone editor's first implemented Skin Weight Lab slice while retaining bind validation and all runtime LBS/DQS work as future milestones. |
| 0.3 | 2026-08-06 | Added the practical DQS reference page and Optimized Centers of Rotation paper as study references for DQS implementation, scale/shear, and artifacts not fully addressed by either LBS or DQS. |
| 0.2 | 2026-08-06 | Registered the initial rat study bundle, defined what is still required before it becomes a runtime acceptance fixture, and corrected the historical articulated-document finding. |
| 0.1 | 2026-08-06 | Initial plan: shared pose model, bind-pose invariant, LBS/DQS roles, antipodality, scale boundary, backend policy, standalone editor, articulated-animation relationship, milestones, and validation gates. |
