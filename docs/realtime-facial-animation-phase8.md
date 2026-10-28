# Real-Time Facial Animation - Phase 8 minimum cross-platform runtime

Phase 8 is the minimum-functional engine milestone after the offline facial authoring workflow.
The goal is to validate the existing MSH runtime and editor workflow on Windows and macOS without
adding live capture or new authoring concepts.

## Scope

The acceptance asset contains:

- facial target deltas and normal deltas;
- generated facial bones and facial vertex weights;
- at least one authored facial clip, such as `sorrir`;
- at least one existing skeletal clip, such as `MeganWalk`;
- the same mesh topology and target order after save/reload.

The asset must load in MeshDebug, list both facial and skeletal clips, and play both systems at the
same time. Facial deformation remains morph-then-skin, with no duplicate head rendering or detached
facial geometry.

## Backend contract

| Platform | Required runtime path | Facial morph path for this milestone | GPU facial morph parity |
|---|---|---|---|
| Windows | Existing DirectX 9/11 paths selected by the build | GPU indexed deltas when capability checks pass, otherwise CPU fallback | Implemented |
| macOS | Existing Metal path | GPU indexed deltas with CPU fallback | Automated Apple M4 tests pass |
| Linux | Existing GLES2 path remains the regression reference | GPU indexed deltas when capability checks pass, otherwise CPU fallback | Already covered by the existing slice |

The Windows and macOS pass must not depend on GLES2 texture capabilities. The existing skeletal
backend for each platform remains responsible for skinning; macOS Metal, Windows DirectX 9/11 and ANGLE/OpenGLES
may use the indexed GPU facial path when their measured resources permit it, with the backend-neutral
CPU path as fallback.

## Windows acceptance result

**Windows manual runtime acceptance completed on 2026-10-26.** The Megan facial-plus-skeletal MSH was
loaded and reviewed in MeshDebug and the facial rig editor on the Windows paths used by the build:

| Windows path | Skeletal stage | Facial stage | Result |
|---|---|---|---|
| DirectX 11 | `gpu-skeletal-composite` | `gpu-indexed-deltas` | Pass |
| OpenGL ES through ANGLE | `gpu-skeletal-composite` | `gpu-indexed-deltas` | Pass |
| DirectX 9 | `gpu-skeletal-composite` | `cpu-reference` | Pass; GPU indexed morph is unavailable on the tested device because `floatTexture=no` |

The DirectX 9 CPU result is the documented capability fallback, not a facial animation failure.
The combined facial-plus-skeletal path is currently LBS-only: facial-target meshes remain on LBS,
while DQS continues to be available for meshes without facial targets. Facial morph plus rigid-DQS
composition is tracked as future work in the [implementation plan](realtime-facial-animation-plan.md#todo-compose-facial-morphs-with-dqs).

The Windows Release `libTest --facial-foundation-tests` also passed with zero failures, covering
facial validation, MSH round trips, authored clip persistence, and CPU deformation. The graphical
`--facial-runtime-test` still needs to be run on a session with an initialized graphics device to
automate seek, idle, reload, and replay checks; the non-interactive DirectX 9 session used for this
review could not create its hardware device.

## macOS automated result (2026-10-28)

Release build 7.474 on Apple M4 passed with `MTL_DEBUG_LAYER=1`:

- `testLib --facial-foundation-tests`: zero failures, including MSH round trips.
- `testLib --facial-runtime-test`: indexed and asynchronous VB-only geometry, morph plus LBS,
  clips, seek, reload, instance isolation, idle counters, and starting CPU skeletal playback after GPU morph rendering
  (the CPU skeletal policy is selected before load). GPU weight changes perform no geometry uploads.
- `testLib --metal-skeletal-parity-test`: shared production morph source and resource uploader
  checked against CPU results, including normals, tiled texture rows, subset offsets, zero/mixed/full
  weights and morph-before-LBS rotation/translation. Cases cover 5 vertices/2 targets and
  34,164 vertices with 6 and 52 targets; maximum facial error was `1.2e-7` (tolerance `1e-4`).
- The Meg facial editor project loaded with 34,164 vertices and 65 bones. At the saved face-origin
  X/Y, the old front-view ray selected hidden vertex 10788 (`z=-6.4198`); nearest-surface picking
  selects visible vertex 7224 (`z=15.9289`). The original project file was not rewritten.

Metal keeps the immutable RGBA32Float texture in private `BUFFER_SPECIFIC` storage. Vertex texture
6 holds deltas, buffer 22 holds per-instance weights, and buffer 23 holds the VB subset offset;
these do not overlap skeletal palette 19, normal-map slots 20-21, or lighting slots 4-18.
macOS probes RGBA32Float allocation and conservatively budgets 4096 x 4096 texels, up to 256 targets,
and 256 MiB per resource. Upload/shader/draw failures use the existing explicit CPU fallback statuses.
Specialized morph pipelines are kept out of the non-morph default shader cache. iOS activation is
not part of this acceptance pass.

An indicative synthetic timing sample at 34,164 vertices measured CPU morph means of 0.327 ms
(6 targets) and 1.535 ms (52 targets); GPU command submission plus synchronous completion measured
1.161 ms and 2.464 ms. These are different workloads: the GPU test also applies LBS and includes
submission/wait overhead. They are diagnostics, not evidence of a GPU speedup or a frame-time budget.

The full manual authored-asset acceptance checklist below remains open. Automated numerical tests
and editor loading do not establish anatomical quality or complete the Blender authoring workflow.

## Acceptance checklist

1. Configure and build the required Windows and macOS targets with the normal Lua, Mesh, and ImGui
   editor features enabled.
2. Open the generated MSH and verify facial target count, facial clip count, skeletal clip count,
   and the execution report.
3. Play the skeletal clip and the authored facial clip together. The head must follow the body and
   facial weights must change without a second facial draw.
4. Stop, seek, reload, and replay. The first facial key, skeletal bind/T-pose, loop behavior, and
   target names must remain deterministic.
5. Save the MSH again and reopen it. Facial targets, authored clips, skeletal sections, weights, and
   textures must remain present.
6. Exercise a mesh without GPU facial capability and verify that the report says CPU reference
   rather than silently disabling facial animation.
7. Run the editor with no live capture source and verify that idle frames do not rebuild facial data
   continuously.

## Explicit exclusions

OSC, webcam capture, Faceit runtime streaming, ARKit SDKs, and other vendor transports are not part
of Phase 8. They will be implemented in a separate capture PR. That PR must feed the validated
name-to-weight packet contract from [Phase 5](realtime-facial-animation-phase5.md) and may use the
deterministic replay adapter from [Phase 6](realtime-facial-animation-phase6.md) for regression.

Native Blender-free target sculpting is also optional future work. The current minimum path may use
the automated Blender target bootstrap, but a finished MSH does not require Blender at runtime.

## Handoff

The Windows portion is complete. Phase 8 as a whole is complete when the same authored asset passes
the checklist on macOS as well and each platform report shows either `gpu-indexed-deltas` or the
documented CPU fallback. DirectX GPU morph optimization is complete for the tested Windows paths;
live capture and native target authoring remain independent milestones.
