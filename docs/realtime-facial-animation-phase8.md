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
| macOS | Existing Metal path | CPU reference | Deferred |
| Linux | Existing GLES2 path remains the regression reference | GPU indexed deltas when capability checks pass, otherwise CPU fallback | Already covered by the existing slice |

The Windows and macOS pass must not depend on GLES2 texture capabilities. The existing skeletal
backend for each platform remains responsible for skinning; Windows DirectX 9/11 and ANGLE/OpenGLES
may use the indexed GPU facial path when their measured resources permit it, with the backend-neutral
CPU path as fallback.

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

Phase 8 is complete when the same authored asset passes the checklist on Windows and macOS and the
platform reports show either `gpu-indexed-deltas` or the documented CPU fallback. DirectX/Metal GPU
morph optimization is complete for the Windows DirectX 9/11 paths; live capture and native target
authoring remain independent milestones.
