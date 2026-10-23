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

The development Linux run on 2026-10-23 measured approximately 1.3 ms, 9.6 ms, and 19.1 ms median
for the three profiles. The medium profile uses about 24.96 MB of dense position and normal delta
storage. These measurements establish the comparison point for future changes and do not claim a
GPU capacity or a frame-rate guarantee.

The reference loop already skips the vertex pass for zero-weight targets. A follow-up two-pass
active-target experiment did not show a measurable improvement in the medium profile in the same
debug build, so it was not retained. The next CPU optimization should target a measurable source of
work rather than duplicate this existing guard.

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

The CPU reference remains the default path. A GPU morph implementation should be proposed only when
a representative game mesh, capture workload, or target backend demonstrates that CPU deformation is
the limiting cost. The next optimization milestone should retain this command and compare CPU/GPU
results against the same target weights and output tolerance.
