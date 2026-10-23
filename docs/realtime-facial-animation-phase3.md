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
- Mesh Debug exposes target sliders, clear/reset controls, clip playback controls, time, execution
  status, and validation state. Controls update only on user interaction; idle frames do not rebuild
  or serialize the mesh.

## Scope boundary

The stream/intermediate Lua exporter does not author facial sections; the normal direct-MSH import
path is the supported route. Facial edits that reorder or resize frame-0 geometry remain rejected by
the runtime. Direct Blender -> MSH import is the supported acceptance path in this slice. MSH ->
Blender shape-key export, authoring new dense targets, and editing facial curves inside mini-mbm
remain future work.

## Validation

The pure exporter tests cover the V11 geometry signature, signed-zero canonicalization, and target
and clip payload headers. Lua preflight covers the new playback module and the existing Mesh Debug
script. Blender 5.1 fixture execution remains the integration check when Blender is available.
