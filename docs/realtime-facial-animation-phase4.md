# Real-Time Facial Animation - Phase 4 CPU performance profile

The first Phase 4 slice establishes a repeatable CPU reference measurement before adding a backend-
specific morph shader.

## Benchmark command

Build the Linux debug test binary, then run:

```sh
./bin/debug/linux_x86/testLib --facial-performance-benchmark
```

The command performs three synthetic profiles sequentially:

| Profile | Vertices | Targets | Active targets |
| --- | ---: | ---: | ---: |
| small | 5,000 | 16 | 4 |
| medium | 20,000 | 52 | 8 |
| large | 40,000 | 52 | 8 |

Each profile warms the reference deformation path, records twelve samples, and prints median and
p95 milliseconds plus dense position/normal delta memory. The command returns success when the
reference path produces the expected vertex count; it intentionally does not enforce a universal
time threshold because this is hardware and build dependent.

The development Linux Debug run on 2026-10-23 measured the following dense versus sparse medians:

| Profile | Dense median | Sparse median | Dense p95 | Sparse p95 |
| --- | ---: | ---: | ---: | ---: |
| small | 1.282 ms | 0.464 ms | 1.490 ms | 0.513 ms |
| medium | 8.992 ms | 2.454 ms | 11.018 ms | 2.548 ms |
| large | 17.270 ms | 4.867 ms | 17.690 ms | 5.255 ms |

The synthetic sparse profiles keep about one eighth of each target's vertices active. The medium
profile still uses about 24.96 MB of dense position and normal delta storage; the sparse index adds
only one 32-bit entry per changed vertex. These measurements are hardware and build dependent and
do not claim a GPU capacity or a frame-rate guarantee.

The reference loop already skips the vertex pass for zero-weight targets. This milestone adds a
runtime sparse index for targets decoded from MSH files: only vertices with a non-zero position or
normal delta are accumulated. Hand-built targets retain the dense compatibility path, and output
normalization still scans the complete output buffer so untouched normals remain valid.

## Representative high-poly fixture

The Fuzzy Fury GLB was imported into Blender and prepared as a local facial test fixture. The
generated `.blend` contains 57,746 vertices, six Basis-relative targets (`jawOpen`, `mouthClose`,
`mouthSmile`, `tongueOut`, `eyeBlink`, and `browUp`) and one 25-frame facial clip. Its exported MSH
contains facial sections 45 and 46 and stays within the 16-bit frame vertex limit. The original GLB
has no morph targets or armature, so this fixture isolates facial deformation and is not a production
Faceit rig yet.

Facial export now keeps `sourceVertex` and `sourceLoop` associations on emitted vertices without
forcing the loop index into the deduplication key. This preserves stable target mapping while
allowing high-poly meshes whose deduplicated vertex count remains below 65,535.

## Decision

The sparse CPU reference remains the parity oracle for morph evaluation. The GPU composition slice
is implemented for the existing OpenGL ES, DirectX 11, DirectX 9, and macOS Metal skeletal backends. Each GPU
morph variant samples indexed position/normal deltas and its GPU LBS variant applies the skeletal
palette in the same vertex stage, preserving morph-then-skin without a second draw. The CPU path
remains available for parity checks and safe fallback when a resource or shader prerequisite is
unavailable.
The benchmark command still compares dense and sparse morph output against the same target weights;
both paths must retain the same output tolerance.

The GPU composition report remains exposed as `gpu-skeletal-composite` for the skeletal stage. The
indexed path moves dense position and normal deltas into a measured floating-point texture, indexes
them with a canonical vertex-ID stream, and sends current target weights to the vertex shader. The
morph report is `gpu-indexed-deltas` with `gpu-morph-ready` while this path is active on GLES2,
DirectX 11, DirectX 9, or macOS Metal. Metal stores indexed deltas in a private `RGBA32Float`
texture and keeps per-instance weights in the vertex stage; capability, layout, upload, and shader
failures use the CPU reference path.
Custom vertex shaders, normal-map facial materials, and DQS facial composition remain unsupported.
If texture creation or shader specialization fails, the renderer switches back to the CPU reference
path and reports the reason instead of dropping the mesh.

The runtime reports the skeletal and morph stages separately. `morphExecutionPath` is
`gpu-indexed-deltas` when the backend texture/shader path is active and `cpu-reference` after a safe
fallback. `morphStatus` distinguishes `gpu-morph-ready`, capability/layout failures, upload
failures, and shader failures. If initialization cannot measure the backend's texture limits or
floating-point resource support, the report still says `gpu-morph-capability-unmeasured` or
`gpu-morph-resource-unavailable`. MeshDebug and the Lua report show this explicitly. DirectX 9
reports CPU fallback on devices without vertex-shader 3.0 or floating-point vertex textures.

The GPU morph layout planner is covered independently from rendering. It tiles a target's vertex
deltas across the measured texture width, reserves a second row block for normal deltas, and rejects
layouts whose total height exceeds the device limit. The Megan fixture (34,164 vertices, six targets,
position and normal deltas) produces a 4,096 x 108 layout on the representative test capability.
The GLES2, DirectX 11, DirectX 9, and Metal upload and shader paths consume this layout without
changing mesh topology or creating a second draw.

The Fuzzy Fury fixture provides that representative high-poly measurement: its 57,746-vertex,
six-target deformation measured approximately 20.7 ms dense and 5.9 ms sparse per evaluation in
the current Linux Debug build. The Debug result is unsuitable as a frame-rate estimate, but the
rough 3.5x reduction confirms that the dense vertex pass was a significant cost. Its active index
contains about 29,156 changed vertices across the six targets. An optimized standalone profile
remains useful for later backend comparisons. The preview clip intentionally overlaps several
targets, so its visible result is a composite rather than a single-target isolation test.

## Facial-plus-skeletal fixture

The Mixamo Megan FBX was prepared as a second fixture with six procedural facial targets on the
body mesh (`jawOpen`, `mouthSmile`, `mouthPucker`, `eyeBlinkLeft`, `eyeBlinkRight`, and `browUp`).
Its exported MSH preserves 65 canonical bones, the original 211-frame skeletal clip, 34,164 facial
vertices, and a 211-frame facial clip. The FBX custom normals were cleared in the preparation blend,
allowing all six targets to carry position and normal deltas; the facial renderer continues to
exclude normal-map slots.

MeshDebug preview confirmed that the Megan skeletal clip and facial clip play together. The
headless facial foundation suite now loads a representative composition fixture, checks canonical
skeletal and facial metadata, and compares facial sections plus skeletal sections 41-43 after both
compressed and uncompressed save/reload round trips. This protects the combined asset contract
without making the test depend on a window or a manually selected file.

The facial clip interchange milestone is also covered by
`src/test-lib/blender_facial_curve_roundtrip_test.py`. It reloads the generated Blender project
and checks both linear and step Actions; this caught and fixed an explicit-zero interpolation bug
in the reverse exporter. The same regression exports and reimports FBX, confirming that the
MeshDebug-owned custom-property payload restores the clips after Blender drops the original
Actions. External FBX files without that payload remain geometry-only for facial animation.
