# Real-Time Facial Animation - Phase 3

Phase 3 connects Blender shape keys to Mesh V11 facial sections and adds an event-driven preview
surface to Mesh Debug.

## Editor decision

The preview lives in `editor/facial_mesh_playback.lua`, mounted as a **Facial animation** node in
`editor/mesh_debug.lua`. The skeletal animation editor remains focused on bones, pose authoring,
and skeletal timelines. A separate facial authoring editor is deferred until the runtime has an
editing contract for dense deltas and facial keys; the preview module can be reused by that editor
later.

## Delivered in this slice

- The direct Blender `.blend`/FBX/GLB import command accepts `--include-facial`, a convention label,
  and optional normal-delta extraction.
- Relative Basis shape keys are exported as dense frame-0 position deltas, with optional smooth
  normal deltas. The exporter freezes the same source-object, source-vertex, and source-loop
  association used to build the frame, then computes the V11 geometry signature over the final
  vertex and index order.
- Shape-key Actions are sampled into linear facial clips. Explicit import clip ranges are honored;
  otherwise the active shape-key Action becomes one clip. The final requested frame is always
  sampled so the clip has the V11-required endpoint key.
- Facial sections are written transactionally with the direct MSH output. Vertex-buffer-only mode,
  decimation, absolute/masked shape keys, unsupported modifiers, topology changes, and custom
  normals when normal deltas are requested fail with diagnostics. Animated facial clips require a
  canonical skeletal asset (or a static facial frame); the legacy multi-frame baked mesh path is
  rejected because V11 facial data is tied to one frame-0 topology.
- Facial direct exports omit normal-map texture slots automatically. The current facial renderer
  uses the CPU morph path and rejects prepared 3D normal-map state; retaining a normal slot would
  make the runtime prepare tangents during load and reject the otherwise valid facial asset. Static
  exports keep normal-map slots unchanged.
- Mesh Debug exposes target sliders, clear/reset controls, clip playback controls, time, execution
  status, and validation state. Controls update only on user interaction; idle frames do not rebuild
  or serialize the mesh.
- Mesh Debug can export its loaded facial asset to the existing Blender skeleton-export JSON bridge.
  The Blender side creates one Basis plus one shape key per target, stores stable facial metadata on
  the mesh datablock, and writes one linear or step Action per facial clip. `.blend` output retains
  the shape keys and Actions for continued authoring.
- The Blender fixture now runs the direct `.blend -> MSH` path headlessly and parses the emitted V11
  sections 45/46. It verifies the post-split vertex count, target names and ids, non-zero dense
  deltas, and the sampled clip key count without opening the Mesh Debug window.
- `src/test-lib/blender_facial_curve_roundtrip_test.py` now reloads a generated `.blend` in Blender
  headlessly and verifies facial target metadata, multiple named Actions, sampled keyframes, and
  both linear and step interpolation. Explicit V11 interpolation value `0` is preserved as a
  Blender `CONSTANT` curve instead of being coerced to linear.

## Scope boundary

The stream/intermediate Lua exporter does not author facial sections; the normal direct-MSH import
path is the supported route. Facial edits that reorder or resize frame-0 geometry remain rejected by
the runtime. Direct Blender -> MSH import and Mesh Debug -> `.blend` export are the supported
acceptance paths in this slice. FBX export preserves shape-key geometry in the tested Blender
version, but FBX facial Actions are not restored on re-import; reliable FBX curve transport,
authoring new dense targets, and editing facial curves inside mini-mbm remain future work.

## Validation

The pure exporter tests cover the V11 geometry signature, signed-zero canonicalization, and target
and clip payload headers. Lua preflight covers the new playback module and the existing Mesh Debug
script. Blender 5.1 fixture execution covers `.blend` shape-key/action creation; FBX geometry is
present after re-import, and the direct MSH acceptance route is checked from the same fixture while
facial curves are intentionally recorded as a remaining limitation for FBX.

Run the reverse-curve regression with:

```sh
blender -b --factory-startup --python src/test-lib/blender_facial_curve_roundtrip_test.py -- \
    --output-dir /tmp/mini-mbm-facial-curve-roundtrip
```
