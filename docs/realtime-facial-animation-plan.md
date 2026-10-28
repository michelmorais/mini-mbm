# Real-Time Facial Animation - Current State and Future Deliveries

Status: **The minimum facial animation workflow is implemented for Linux, Windows, and macOS.**

This document is a snapshot of the current engine. It intentionally does not describe the sequence
of implementation work. Future items below are independent delivery proposals and are not required
to use the current offline facial animation workflow.

## Current engine contract

The runtime delivery asset is a Mesh V11 `.msh`. Blender is an optional authoring tool; a finished
asset does not require Blender at runtime.

Facial data is optional and remains compatible with meshes that do not contain it. The facial asset
uses these Mesh V11 sections:

- `SECTION_FACIAL_TARGETS` (45): target identity, name, convention, position deltas, and optional
  normal deltas in frame-0 vertex order.
- `SECTION_FACIAL_CLIPS` (46): named facial clips with duration, loop mode, interpolation, and
  timestamped target weights.

The loader validates target and clip identities, topology signatures, vertex counts, finite values,
weight ranges, key ordering, section relationships, and payload limits before committing an asset.
Ordinary save/reload preserves facial sections together with skeletal sections and textures.

## Runtime capabilities already available

Each loaded mesh instance supports:

- target enumeration and target information by index or name;
- manual facial weight set, clear, and reset;
- facial clip play, pause, resume, stop, seek, and playback speed;
- facial animation reports with target/clip counts and explicit execution paths;
- simultaneous facial morphing and skeletal animation using morph-then-skin composition;
- per-instance state without mutating the shared cached mesh asset;
- conservative visibility bounds and an editor `alwaysRender` path for close authoring views.

The Lua runtime exposes the same controls through the `mesh` API. MeshDebug also exposes facial
metadata inspection and validated project-clip insertion/removal for export.

## Supported rendering paths

| Platform | Skeletal path | Facial morph path | Fallback |
|---|---|---|---|
| Linux OpenGL ES | Existing skeletal backend | Indexed GPU deltas when measured resources and shaders pass | CPU reference |
| Windows DirectX 11 | Existing skeletal backend | Indexed GPU deltas when capability checks pass | CPU reference |
| Windows DirectX 9 | Existing skeletal backend | Indexed GPU deltas when the device supports the required vertex texture path | CPU reference |
| macOS Metal | Existing skeletal backend | Indexed GPU deltas using the measured float texture path | CPU reference |

The report never labels an incomplete GPU upload or shader as ready. A capability or shader failure
selects the CPU reference path explicitly. Facial-target meshes currently require LBS for skeletal
composition; facial morph plus rigid DQS is not implemented.

## Facial authoring editor

`editor/facial_rig_editor.lua` is a dedicated editor following the skeletal editor's worktree and
viewport conventions. Its current worktrees are:

1. Setup and object roles.
2. Face position and scale.
3. Facial landmarks, including independent or synchronized side editing.
4. Masks and regions with generated shader visualization and brush editing.
5. Facial rig preview, canonical facial bones, binding review, and weight assignment.
6. Expressions based on target weights.
7. Facial clips with a timeline, keys, interpolation, capture, and preview.
8. Preview of expressions, project clips, imported MSH clips, and skeletal clips together.

The editor persists project metadata in Lua. The MSH stores runtime geometry, targets, clips,
skeleton data, and weights. Project clips can be embedded into the exported MSH by an option that
is enabled by default. Existing runtime clips with the same name are replaced; unrelated clips are
preserved.

## Blender interoperability

The Blender bridge and automated workflow are optional authoring paths. They support:

- rig preview and armature handoff;
- assisted starter target generation from project landmarks and masks;
- target and clip import through MeshDebug;
- skeletal animation preservation during the automated target round trip;
- coordinate, axis, and texture conversion used by the existing MeshDebug Blender workflow.

The generated targets are starter shapes and require visual review. Full Faceit internal project
compatibility is not claimed. Compatibility means shared conventions for names, landmarks, regions,
weights, Shape Keys, Actions, and MSH interchange.

## Capture boundary

The engine contains a transport-independent named-weight mapping contract in
`editor/facial_capture_mapping.lua` and a deterministic file replay adapter in
`editor/facial_capture_stream.lua`. They validate names, aliases, finite values, ranges, sparse
updates, ordering, looping, and reset behavior.

There is currently no built-in OSC receiver, webcam capture, Faceit runtime stream, ARKit SDK, or
other vendor transport. Those sources are intentionally external to the core asset format and are
future deliveries.

## Current minimum Linux delivery

The Linux delivery must provide the same offline contract already accepted on Windows and macOS:

- build the engine and editor with Lua, Mesh, and ImGui enabled;
- load a MSH containing facial targets, facial clips, facial weights, and a skeletal clip;
- play facial and skeletal clips together in MeshDebug and the facial editor;
- preserve targets, clips, skeleton data, weights, and textures through save/reload;
- report GPU indexed morphing when the OpenGL ES capability path is ready and CPU fallback otherwise;
- keep idle editor frames event-driven without repeated topology scans or uploads.

This is the minimum feature delivery. It does not require live capture, native target sculpting, or
facial DQS.

## Future deliveries

### Live capture integration

Implement OSC first as a separate PR, using the existing named-weight mapping and replay contracts.
Webcam and vendor SDK adapters can follow without changing Mesh V11. The delivery should define
connection state, packet timeout, neutral reset, calibration, target diagnostics, and optional
recording to a facial clip.

### Native target authoring

Add an optional Blender-free path for creating, naming, duplicating, mirroring, deleting, and
sculpting dense facial target deltas. It must provide undo/redo, topology validation, normal-delta
handling, target preview, and transactional MSH export. This must reuse the existing Expressions and
Clips worktrees rather than introduce a second animation model.

### Facial morph plus DQS composition

Define and implement facial morphing with rigid dual-quaternion skinning in the CPU reference and
all supported GPU paths. Add capability reporting, parity tests, and editor comparison coverage.
Until then, facial-target meshes remain on LBS.

### Additional platform acceptance

Evaluate iOS Metal and any other supported platform using the same indexed-resource contract,
capability checks, and CPU fallback rules. Platform work must not change the MSH facial sections or
the Lua control contract.

## Validation requirements

The maintained test suite must continue to cover:

- facial target and clip V11 round trips, including compressed and uncompressed output;
- malformed section rejection without partial asset mutation;
- sparse and dense CPU deformation parity;
- GPU capability, upload, shader, and CPU fallback reporting;
- simultaneous facial and skeletal playback;
- instance isolation, pause/seek/stop/reload behavior, and culling visibility;
- Lua mapping/replay contract behavior;
- idle editor performance and event-driven rebuilds.

When a future delivery changes a binary section or Lua API, update the corresponding format/API
documentation and `include/version/version.h` in the same change.
