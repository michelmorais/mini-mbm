# Facial Animation Phase 0: Blender Interchange and Initial Design

Status: **Synthetic interchange spike implemented and validated; runtime not implemented.**
Validated: **2026-10-23**, Linux, Blender **5.1.2**.
Parent: [Facial animation implementation plan](realtime-facial-animation-plan.md).

## 1. Decision and scope

Use **Blender `.blend` -> extended Blender helper -> mini-mbm** as the canonical first authoring
boundary. The experiment reuses `editor/blender_mesh_export.py` for base geometry, triangulation,
subset ordering, coordinate conversion and subset splitting. No direct glTF parser is needed.
The experimental extractor is in `src/test-lib/blender_facial_interchange_test.py`; its JSON is a
test artifact, not a public interchange protocol or a Mesh V11 section.

FBX and glTF 2.0 both preserve the fixture's target names, associated position deltas, sampled
weights and skin binding after explicit Action selection and time-origin normalization. This
supports later adapters through Blender, but is not a guarantee for arbitrary FBX/GLB assets.
The first canonical path uses `.blend` to retain authoring object names and avoid those adapters.
The ordinary Mesh Debug import still does not preserve editable facial targets or facial clips.

Faceit authors/bakes shape keys and can bake its control-rig animation to shape-key animation.
Those Blender datablocks are the intended input; the engine will not depend on Faceit at runtime.
See the primary [Faceit bake documentation](https://faceit-doc.readthedocs.io/en/latest/bake/) and
[control-rig documentation](https://faceit-doc.readthedocs.io/en/beta/control_rig/).
The fixture is generated with Blender APIs and **does not execute or validate the Faceit add-on**.
A baked production Faceit character remains a required integration check before advertising that
workflow as supported.

## 2. Reproduction and evidence

From the repository root:

```sh
blender --background --factory-startup --python-exit-code 1 \
  --python src/test-lib/blender_facial_interchange_test.py -- \
  --output-dir /tmp/mini-mbm-facial-spike
python3 src/test-lib/blender_mesh_export_test.py
```

Use a dedicated output directory: reruns replace the generated artifacts. No downloaded model or
Faceit installation is required. The generator creates:

- `facial-fixture.blend`, `.fbx`, `.glb`: a bent face patch with a head bone, animated skinning,
  three relative targets (`jawOpen`, `mouthSmileLeft`, `eyeBlinkRight`), two materials and a UV seam.
- `facial-helper-probe.json`: immutable base subsets, source-corner mapping, target deltas and
  a 24 fps, one-second clip sampled at 25 integer frames including both endpoints.
- `facial-fbx-probe.json`, `facial-glb-probe.json`: extracted re-imported positions and curves.
- `facial-interchange-report.json`: comparison results and Blender version.

The helper assertions and either interchange regression cause a nonzero Blender exit with
`--python-exit-code 1`. Route failures are written to the report before the final assertion.

| Check | Observed result |
|---|---|
| Source vertices / exported vertices | 9 / 24, with material and UV duplication |
| Targets / sampled weight vectors | 3 / 25 |
| Direct helper | PASS; repeated extraction and JSON read/write preserve payload and IDs |
| Frozen mapping through subset chunking | PASS; forced three-vertex chunks preserve triangle-corner records |
| Multiple targets before skinning | PASS against Blender's evaluated geometry, including zero-weight endpoints |
| Unsupported inputs | Absolute keys, non-Basis relative keys, masks, drivers and a subdivision modifier rejected |
| Extraction failure | Original Action, enabled armature modifier and scene frame restored |
| FBX round trip | PASS; maximum position/UV/weight/time component error `3.5762786865234375e-7` |
| GLB round trip | PASS; maximum position/UV/weight/time component error `2.384185791015625e-7` |
| Position/UV/weight/time comparison tolerance | `2e-5` |
| Normal change at individual target endpoints | Maximum component delta about `0.46658` |
| Existing exporter regression suite | 10 tests passed |

The animated reference uses nonidentity translation, rotation and uniform scale. The composition
check applies the **exported** deltas together, converts back to Blender world coordinates, applies
the head skin matrix, and compares with Blender evaluation. Skin-weight preservation in FBX/GLB is
checked for the presence of an armature and per-vertex groups; it is not a full numeric bone-track
or skin-weight round-trip test. There is no engine rendering, GPU parity or performance claim here.

Two import behaviors matter:

- Blender's default FBX import shifts this clip to frames 2-26. The comparison discovers the actual
  key range and normalizes it to seconds starting at zero instead of comparing wrong frames.
- glTF re-import stashes the shape Action separately from the skin Action. The comparison explicitly
  activates the sole facial Action and its action slot. It excludes hidden bone-display meshes.

The glTF manual also documents vertex splitting at UV and shading discontinuities:
[Blender glTF manual](https://docs.blender.org/manual/en/3.0/addons/import_export/scene_gltf2.html).
The experiment therefore compares re-imported corners by base position/UV and their own deltas,
never by assuming that source vertex indices survive a file-format round trip.

## 3. Implemented experimental contract

### Geometry and identity

`export_frame_subsets(..., capture_source_vertices=True)` adds `sourceObject`, `sourceVertex` and
`sourceLoop` to output vertex records and includes loop identity in its deduplication key.
Indices describe the **evaluated** Blender mesh. The spike first rejects all modifiers except
Armature, then temporarily disables Armature modifiers and zeros shape-key weights. Merely matching
vertex counts would not establish original vertex identity.

The base mapping is frozen once. Every target is evaluated into that mapping; its output is never
independently welded or triangulated. Source-corner identity prevents corners with equal base normals
but different target normals from accidentally merging. Existing subset splitting copies these
records, including provenance. The default exporter call emits no provenance and retains its
existing deduplication behavior. The new option is internal and is not exposed by the editor UI.

Blender world positions are rotated -90 degrees around X and reflected across X, giving
`engine = (-world.x, world.z, -world.y)`. Reflection also reverses triangle winding in the existing
helper. UVs are unchanged by this spike. Object translation affects base positions, not deltas;
object rotation and positive uniform scale affect both base geometry and target deltas. Shear,
negative and nonuniform object scale are rejected in this initial profile.

Mesh identity is a domain-separated stable hash of the object name; target identity hashes mesh
identity plus target name, using the existing helper's `stable_id`. JSON stores these 64-bit IDs as
decimal strings. Target names are unique within one Blender Key datablock. Indices are zero-based
within the experimental arrays. Renaming an object or target changes these experimental IDs;
cross-format object renaming is **not** treated as stable asset identity. Persistent identities and
collision checks belong in Phase 1's asset contract.

### Targets, normals and weights

The extractor accepts relative, unmuted, unmasked keys whose `relative_key` is Basis. Position
deltas are dense arrays in each object's frozen exported-vertex order. All visible mesh objects
must have shape keys in this intentionally narrow fixture profile. Finite values are checked;
sampled weights outside `[0,1]` are rejected. Experimental defaults are explicitly zero.

Normal deltas are collected for the direct smooth-shaded fixture without custom normals. Normal
transformation uses inverse transpose, followed by the same coordinate rotation/reflection.
The sizeable measured endpoint normal change supports retaining normal deltas for the first lit
runtime profile. The experiment does **not** validate hard edges, custom split normals, or blended
normal accuracy when several targets overlap. FBX/GLB comparisons explicitly omit normal deltas
because those importers create custom normals outside this profile.

### Animation and state

The extractor samples each mesh's active shape-key Action at explicit strictly increasing integer
frames. It computes seconds as `(frame - firstFrame) / (fps / fps_base)` and emits linear sampled
weight vectors. It uses Blender's evaluated weights rather than copying only FCurve control points.
Drivers and active NLA tracks require baking first. The fixture's curves are linear; no preservation
of arbitrary Bezier handles, constant discontinuities, subframe motion, multiple clips or NLA mixing
is claimed. Future import must expose sampling resolution and reject unsupported sources explicitly.

Sampling, temporary Action detachment, target evaluation and modifier changes run inside restoration
logic. The spike is a one-shot process; it introduces no editor `onLoop` or per-frame work.

## 4. Bounded Phase 1 design (not implemented)

### Asset and validation

Add optional versioned facial target and facial clip sections after the current V11 section 44;
45 and 46 are candidates, not yet allocated. Keep immutable target arrays, names, IDs and clips in
private cached-asset storage. Tie their vertex count and topology identity to the complete frame-0
stream after subset preparation. Persist mesh/target IDs in `.msh`; validate names and ID collisions
and keep IDs on ordinary load/save. Validate all sections transactionally before publishing an asset.

Start the reference implementation with dense position and normal deltas: the prototype proves that
association and avoids choosing a sparse encoding before representative measurements. The tiny
fixture is not evidence for general vertex/target capacity. Before accepting arbitrary payloads,
Phase 1 must set checked byte/count budgets, guard multiplication/offset overflow, and measure memory
and deformation cost on a representative head. Unsupported section versions, missing targets,
topology mismatches, duplicate names/IDs, invalid key ordering/durations and non-finite data fail.

The first lit profile requires normal deltas; absence produces an explicit unsupported-data report,
rather than silently lighting with unchanged base normals. Use normalized
`baseNormal + sum(weight * normalDelta)` before skeletal normal deformation, with a defined diagnostic
for a degenerate result. Confirm this approximation visually and numerically before claiming support.

Geometry operations that reorder, split or delete vertices must remap facial data with the same
mapping, or reject the operation while retaining the original asset. Existing optimization,
simplification, tangent generation and Blender export need this audit before facial `.msh` files
can safely enter normal editing workflows.

### Runtime and first backend

First rendering target: **Linux/OpenGL ES, CPU facial deformation, CPU LBS for combined skinning,
generated default material shaders**. Use this as the reference before extending other backends or
DQS. This scope does not change the accepted skeletal-only runtime contract.

Compute `bindPosition + sum(clampedWeight * positionDelta)` from immutable base data, then skin the
result once. Keep weights, clip cursor, overrides, dynamic buffers and dirty state private to each
`MESH` instance; never mutate cached geometry or shared skeleton poses. Reuse the existing CPU buffer
ownership pattern in `src/render/mesh.cpp`, without leaking new containers into core public headers.

Facial weight changes invalidate morph data; skeletal pose changes invalidate final skinned data.
An unchanged paused/idle instance performs no deformation or upload. Update bounds from final CPU
vertices whenever that buffer changes; ensure culling uses those bounds rather than the neutral
face. Reject incompatible custom vertex shaders and explicit unsupported GPU requests with a report;
`auto` may resolve to the supported combined CPU path with a stated reason.

### Controls to implement in Phase 2

Public C++/Lua method names remain unallocated. The initial semantic contract should be:

- Resolve by unique mesh-local name or validated target index; enumerate stable IDs and names.
- Use default weights for targets without clip tracks. An explicit manual override replaces the
  sampled value for that target; clearing it restores the clip/default value. Zero is a real override.
- Reject non-finite API weights, clamp finite weights to `[0,1]`, and do not normalize across targets.
- Maintain facial and skeletal clocks independently. Play restarts the selected facial clip; pause
  retains time; resume continues; stop clears the clip contribution while retaining manual overrides.
- Non-looping clips clamp at their duration; looping time wraps for positive duration. A zero-duration
  clip is constant. Seek clamps to the clip interval, speed is finite and nonnegative, and speed zero
  freezes advancement. Defer transitions until their blend semantics are implemented and tested.
- Report target/clip counts, requested/resolved execution, readiness, and the concrete rejection reason.

## 5. Completion boundary

The synthetic vertex-identity and animated-weight gate in Phase 0 has passed, so Phase 1 can start
with this bounded profile. Remaining integration acceptance includes a real baked Faceit character,
normal/seam fidelity, multiple meshes/clips, robust sampling, larger-asset budgets, `.msh` round trips,
engine/Lua controls, culling, instance isolation, editor integration and backend measurements.
No new runtime facial feature is shipped by this spike; `MBM_VERSION` remains unchanged until that
feature is delivered.
