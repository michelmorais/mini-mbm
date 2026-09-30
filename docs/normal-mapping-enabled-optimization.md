# Normal mapping in enabled builds: geometric variants and deferred upload

Delivery: 7.327. This extends the completed build-switch implementation; it does
not change `USE_NORMAL_MAPPING_3D` or `SUPPORTED_MAX_LIGHTS`.

## Behavior

- Default/reserved static lighting compiles a geometric variant first. It has no
  3D tangent input, mapped-normal helper or mapping settings. Existing 2dw sampling
  remains available, and arbitrary custom shaders are not rewritten.
- A supported 3D draw selects a mapped variant only when the subset has a normal
  texture, a retained basis and nonzero strength. Variant creation is lazy;
  default-program caches distinguish geometric and mapped programs.
- Mixed subsets select their respective variants. No-map frames use a cached
  active-subset count; texture and strength changes update that count. There is
  no geometry scan, shader compilation or upload on an idle no-map draw.
- Prepared static frames retain private CPU staging until first effective use.
  Upload publishes the frame's derived buffers and frees staging geometry and
  its copied preparation. The asset's original preparation remains available for
  extraction and serialization.
- After first use, GPU batches stay cached through texture removal/reassignment
  or strength changes. Removal/zero strength switches shading to geometric;
  buffer release or dynamic geometry changes discard resources and staging.
- Source geometry, authoring indices, shared-asset material semantics, skeletal
  fallback, custom-VS fallback, 2dw and the disabled-build contract are preserved.
  Late assignment to an asset with **no prepared basis** still uses geometric
  fallback; this delivery does not introduce automatic regeneration after edits.

## Tradeoffs

Deferred upload can increase CPU memory for a prepared frame that never uses a
map: staging holds positions/normals/UVs and a copy of preparation. Assets without
prepared bases allocate no staging. This is a GPU allocation/upload optimization,
not a guarantee of reduced combined CPU/GPU memory or higher FPS.

The first effective mapped draw can incur compilation and upload latency. It
uploads all prepared subsets of that frame, and mixed draws may switch programs
more often. Per-subset upload/eviction and eliminating staging duplication remain
separate refinements, to be driven by measurements on representative assets.

## Validation

Linux Release/GLES, two point lights: visual comparisons cover indexed and
non-indexed geometry, strength/convention, removal, mixed subsets, reflected and
nonuniform transforms, reserved/custom-VS shaders, point lights, HUD and 2dw.
The preparation/persistence suites and runtime readback cover CPU/file contracts.
With `USE_NORMAL_MAPPING_3D=0`, preparation, persistence, resource inspection,
runtime/GC and visual fallback/2dw tests also pass. This delivery reran cap 2;
the earlier 1..4-light matrix remains evidence for the separate build-switch
delivery, not a new full matrix of these changes.
The runtime suite also exposed a pre-existing Lua coroutine lifetime issue: a
pending mesh load rooted its callback and mesh table but not the initiating
thread. The binding now retains that thread until completion. A forced-GC
regression covers a completed coroutine, callback extraction/save and release
of the thread reference afterward.

`testLib --normal-map-lazy-resource-test` inspects real GLES programs and buffers.
Set `MBM_NORMAL_MAP_FIXTURE_DIR` to the output of
`testLib --normal-map-persistence-tests`. The test covers no-map/zero-strength/2dw
before first use, late assignment, resource reuse, cache separation, shader
restore and dynamic invalidation. The three-vertex fixture reports three active
attributes for the geometric shader versus four for the mapped shader, and zero
derived GPU payload bytes before use versus 150 after use (allocator/driver
overhead excluded).

The GLES LBS/DQS parity suite also passes. This verifies the existing skeletal
fallback, not skeletal normal mapping. ASan investigation reported no memory
access violation; UBSan reported existing unaligned accesses in bundled miniz,
so this is not a claim of a completely clean sanitizer run. The sanitizer run
disabled leak detection and the duplicate tinyfiledialogs global registration
check; neither suppression disables address-access checks.

DirectX 9/11 and Metal follow the same private selection/staging contract but
require native compilation, driver validation and performance measurements.
Linux measurements do not establish native-backend or mobile-device coverage.
