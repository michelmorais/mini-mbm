# Real-Time Facial Animation - Phase 1

Phase 1 adds a bounded facial asset contract to Mesh V11 and a CPU reference deformation path for
the first Linux OpenGL ES runtime profile. It is intentionally separate from the later Lua/editor
controls and from backend-specific GPU morph optimization.

## Delivered

- Mesh V11 section types 45 (`SECTION_FACIAL_TARGETS`) and 46 (`SECTION_FACIAL_CLIPS`), both version 1.
- Transactional loading and saving with CRC/envelope validation, UTF-8 names, finite-value checks,
  duplicate identity checks, topology signatures, section budgets, and malformed-payload rejection.
- Dense frame-0 position deltas with optional normal deltas, plus sampled clip weights using step or
  linear interpolation metadata.
- Per-instance target weights, deformation buffers, conservative animated bounds, counters, and a
  `FACIAL_ANIMATION_REPORT` for runtime inspection.
- Composition with the existing CPU linear-blend skeletal path: morph bind geometry first, then skin.
- Explicit rejection of geometry edits that could reorder or resize the frame-0 vertex contract.

## Validation

From the repository root:

```sh
cmake -S . -B build -DPLAT=Linux -DUSE_TEXTURE_MISSING_DIALOG=0
cmake --build build --target testLib -j4
./bin/debug/linux_x86/testLib --skeletal-foundation-tests
./bin/debug/linux_x86/testLib --facial-foundation-tests
```

The facial foundation suite covers indexed and vertex-buffer fixtures, compressed and uncompressed
round trips, duplicate/orphan/version/signature/truncation failures, transactional load retention,
unsupported geometry edits, zero and simultaneous target deformation, normal validation, and a
20,000-vertex/52-target/8-active benchmark. The existing skeletal foundation suite remains green.

The interactive `--facial-runtime-test` also covers two-instance isolation, idle recomputation,
deformation counters, weight changes, seek/reload, and extraction. It requires a working X11
display; in the headless validation environment it stopped before scene initialization because
display `:1` was unavailable.

## Bounded limitations

The first runtime path is Linux OpenGL ES with triangle meshes and dynamic CPU vertex uploads. It
rejects explicit GPU skeletal execution, dual-quaternion skinning, normal-map material assets, and
custom vertex shaders for facial meshes. Geometry signatures cover positions, normals, indices,
vertex order, and counts; UV/subset remapping is deferred with the rest of the geometry-edit work.
Facial clips are persisted and reported; playback controls and Lua enumeration are delivered in
[Phase 2](realtime-facial-animation-phase2.md). Import/editor integration remains Phase 3.
