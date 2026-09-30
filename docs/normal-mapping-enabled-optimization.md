# Normal mapping in enabled builds: geometric variants and deferred upload

Initial delivery: 7.327. Shared CPU preparation: 7.328. This extends the completed build-switch implementation; it does
not change `USE_NORMAL_MAPPING_3D` or `SUPPORTED_MAX_LIGHTS`.

## Current scope and next step (2026-09-30)

**Windows functional validation is closed for DX9 SM3/DX11 Debug x86. Further
test expansion is deferred. The macOS Metal Debug arm64 baseline is also
validated: 72/72 steps, API validation and targeted captures**, as recorded in
[the Metal milestone](#completed-macos-milestone-metal-baseline-and-captures-2026-09-30). Completed milestones below remain the evidence
history; their earlier next-step proposals do not override this scope decision.

Windows evidence includes the full ON/OFF x 1..4 baseline matrices, SM2
characterization, numerical skeletal parity, DX11 debug/lifecycle checks, the
15-case creation-failure/pixel matrix at all caps, and the 20-case suite including
`Map` at cap 2. Keep those tests; no new Windows fault matrix is required for
functional closure. Release performance and actual device-loss recovery are not
claimed.

### Bounded macOS handoff (completed below)

1. On a Mac with a graphical session and Metal support, read `engine-testing`
   and record macOS, CPU/GPU, architecture, Xcode/SDK and source revision. This
   Windows handoff did not execute native Metal tests; the native results follow below.
2. Build Debug `testLib` and `mini-mbm` with `PLAT=MacOs`, `USE_METAL=1`,
   `USE_LUA=1`, `USE_TEXTURE_MISSING_DIALOG=0`, normal mapping ON/OFF and light
   cap 2. Use separate build trees and snapshot each entry's binaries/libraries
   before building the next, since CMake output paths are shared.
3. Run the existing [matrix entry procedure](#run-one-matrix-entry) with
   `--backend metal --require-native-validation`; preserve reports, logs and
   visual images, plus evidence that Metal API validation is active. The runner
   supports neither `--dx11-failure` nor `--skeletal-parity` for Metal.
4. Resolve failures in the existing baseline, then complete ON/OFF at caps 1, 3
   and 4. Perform the already-planned targeted Metal capture to inspect shader
   inputs and geometric/mapped pipeline selection/cache identity.
5. Close this milestone when all eight baseline entries pass, API validation
   evidence and the targeted capture are recorded, and any platform limitations
   are explicit. Do not add new fault-injection suites to reach this boundary.

macOS GLES is a separate conditional follow-up only if that backend is shipped.
The bounded Metal Release measurement baseline is now recorded [below](#completed-macos-milestone-release-measurement-baseline-2026-09-30):
480 timed samples, separate validation, CPU/GPU timings and memory observations.
Representative game workloads and native instruction/register analysis remain
follow-up work; no FPS or combined-memory improvement is assumed.

### Deferred robustness backlog (not baseline acceptance blockers)

- Extend the 20-case DX11 suite, including `Map`, to caps 1/3/4.
- Broaden compiler/upload/Map fault injection on DX9/Metal or beyond the existing
  DX11 cases; test failures after more than two batches and skeletal buffers.
- Exercise actual device loss/driver resets and in-flight async loads on native
  backends. Existing controlled EGL recreation does not establish these results.

Resume these only through an explicit scope decision or a concrete defect that
requires a focused regression. They are not automatic next milestones.

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
  its shared preparation reference. The asset's original preparation remains available for
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
map: staging holds positions/normals/UVs. Since 7.328 it shares the asset's
immutable preparation instead of copying tangent batches. Assets without
prepared bases allocate no staging. This is a GPU allocation/upload optimization,
not a guarantee of reduced combined CPU/GPU memory or higher FPS.

The first effective mapped draw can incur compilation and upload latency. It
uploads all prepared subsets of that frame, and mixed draws may switch programs
more often. Per-subset upload/eviction and reducing remaining staging geometry remain
separate refinements, to be driven by measurements on representative assets.

## Validation

Linux Release/GLES, two point lights: visual comparisons cover indexed and
non-indexed geometry, strength/convention, removal, mixed subsets, reflected and
nonuniform transforms, reserved/custom-VS shaders, point lights, HUD and 2dw.
The preparation/persistence suites and runtime readback cover CPU/file contracts.
With `USE_NORMAL_MAPPING_3D=0`, preparation, persistence, resource inspection,
runtime/GC and visual fallback/2dw tests also pass. The initial delivery reran
cap 2. The follow-up below completes all eight Linux/GLES configurations for
this optimization; the older build-switch matrix remains separate evidence.
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

DirectX 9/11 and Metal follow the same private selection/staging contract.
The DX9 SM3, DX11 and macOS Metal Debug baseline matrices are validated below.
Bounded Metal Release measurements are recorded below; representative workloads
and other native backend profiling remain pending.
Linux measurements do not establish native-backend or mobile-device coverage.

## Next milestone: native backend regression matrix

Status: the complete DX9 SM3 and DX11 Debug x86 baseline matrices are validated
with 7.328 plus the DX9 compilation fixes below. The macOS Metal Debug arm64
baseline also passes all eight entries, including native pipeline checks and captures. SM2 profile characterization is complete below: default lighting
exceeds the profile, while unlit rendering passes. DX11 single-batch COM creation
failure/retry is also validated across all eight Debug entries (details below).
Windows and the bounded macOS Metal functional baselines are now validated. See [current scope](#current-scope-and-next-step-2026-09-30) for the
completion boundary. Device-loss testing is deferred robustness work, and
performance measurements remain a separate workstream.

- [x] Keep the GLES native resource inspection command and implement the same
  `--normal-map-lazy-resource-test` entry point for DX9, DX11 and Metal.
- [x] Wire the native tests into CMake and Visual Studio. Metal compiles the test
  as Objective-C++ with ARC and submits an offscreen color/depth render pass.
- [x] Provide a Python 3 runner with isolated fixtures/images, per-suite logs,
  JSON results, timeouts, nonzero-exit checks, required PASS sentinels and rejection
  of FAIL/runtime/recognized driver diagnostics. Verify the requested build and
  backend against both testLib and the Lua engine before render tests.
- [x] Complete the Linux/GLES baseline matrix for the enabled-build optimization:
  all eight configurations and 72 runner steps pass (see results below).
- [x] Compile and run all eight `USE_NORMAL_MAPPING_3D=0/1` x
  `SUPPORTED_MAX_LIGHTS=1/2/3/4` entries on Windows DX11 Debug x86: 72/72 steps.
- [x] Compile and run the same eight entries on Windows DX9 SM3 Debug x86:
  72/72 steps, with explicit SM3 capability/profile checks.
- [x] Characterize strict DX9 SM2 at all eight entries: verify the default-lighting
  capacity failure and actual SM2 unlit bytecode/pixels without mapping resources.
  This does not establish lit rendering or real SM2 hardware support.
- [x] Run native numerical skeletal parity on DX9/DX11 with normal mapping ON/OFF
  and all light caps: 64/64 synthetic/Lorekeeper LBS/DQS cases pass. This does not
  test normal-map assignment to production animated meshes.
- [x] Compile and run the same eight entries on macOS Metal Debug arm64: 72/72 steps.
  macOS GLES remains separate, conditional on shipping that backend.
- [x] Run the DX11 native resource test with the Windows Graphics Tools debug
  layer, including post-teardown live-object checks, in all eight entries.
- [x] Run Metal with API validation enabled; require the activation marker.
- [x] Capture native Metal ON/OFF at cap 2, inspect captured MSL inputs and verify
  actual encoder pipeline selection/reuse/cache identity in all eight entries.
- [ ] Inspect native machine instructions/resource statistics during the separate
  performance workstream; captured MSL is not GPU disassembly.
- [x] Exercise controlled full EGL context destruction/recreation through the
  Linux/GLES production restore state machine, with pending and uploaded assets,
  shared instances, shader variants and pixel comparisons (details below).
- [x] Add deterministic Linux/GLES failed-compile/link/program-creation and
  failed-upload retry coverage, including Debug and Release (details below).
- [x] Validate DX11 single-batch COM creation failure/retry in all eight Debug
  ON/OFF x 1..4-light entries, with debug-layer and post-teardown lifecycle checks.
- [x] Validate DX11 rollback/retry in a second batch of one subset and across
  two subsets, Debug ON/OFF at cap 2 (15-case suite, details below).
- [x] Extend the 15-case DX11 suite to light caps 1/3/4: all eight Debug ON/OFF
  entries pass, including native validation (full matrix below).
- [x] Compare pixels after DX11 recovery in all 15 cases, Debug ON/OFF at cap 2.
- [x] Extend recovery pixel comparisons to caps 1/3/4: all eight Debug ON/OFF
  entries pass, including contrast checks and native validation (matrix below).
- [x] Validate DX11 constant-buffer `Map` failure/retry in Debug ON/OFF at cap 2,
  including second-subset failures, retained buffers and pixel comparison.
- [x] Record a bounded native Metal Release measurement baseline across ON/OFF
  and caps 1..4, separating validation from CPU/GPU timing and memory observations.
  This does not establish FPS, total-memory improvement or representative game performance.

### Prepared coverage

| Check | GLES | DX9 / DX11 | Metal |
|---|---|---|---|
| No derived allocation at load, no map, strength zero, 2dw | Native inspection | Native inspection | Native inspection |
| First effective use creates GPU buffers | Buffer queries | COM buffer descriptors | MTLBuffer lengths |
| Reassignment/repeated draws reuse resources | Buffer/program identity | Buffer/vertex-shader identity | Buffer/pipeline identity |
| Geometric/mapped shader selection | Program and tangent input | Bound vertex shader | Encoder pipeline observation, captured MSL, visual comparisons |
| Shared program cache | Geometric/mapped cache identity | DX9 identity; DX11 has no shared default-program cache | Encoder pipeline identity |
| Shader restore, no-basis fallback, dynamic invalidation | Automated | Automated | Submission/resource checks |
| Visual IB/VB, mixed subsets, strength/sign/transforms, custom fallback | Lua suite | Lua suite | Lua suite |
| Async loading/GC, persistence, readback | CPU/Lua suites | CPU/Lua suites | CPU/Lua suites |

“Prepared” describes source coverage, not a native pass. The DirectX/Metal tests
must first compile and execute on those systems; failures there are findings,
not reasons to weaken assertions. Expected malformed-asset errors in persistence
and runtime suites are allowed; their own PASS/FAIL sentinels decide the result.

### Run one matrix entry

Build **both** testLib (Visual Studio project `libTest`) and the Lua-enabled
mini-mbm executable with matching configuration and libraries. Disable the
missing-texture picker (`USE_TEXTURE_MISSING_DIALOG=0`). Python 3 is required;
no third-party Python package is needed. A working graphical desktop is required.
Use a new output directory for every run:

```sh
python3 src/test-lib/run-normal-map-tests.py \
  --test-lib /absolute/path/testLib --engine /absolute/path/mini-mbm \
  --backend gles --normal 1 --lights 2 --output /tmp/normal-gles-on-2
```

Windows example (PowerShell; replace binary paths with your build outputs):

```powershell
py -3 src/test-lib/run-normal-map-tests.py --test-lib C:/mbm/bin/testLib.exe --engine C:/mbm/bin/mini-mbm.exe --backend dx11 --normal 1 --lights 2 --require-native-validation --output C:/mbm-results/dx11-on-2
```

For DX9 use `--backend dx9` and omit `--require-native-validation`. Use a Debug
DX11 build for that switch: Release is deliberately rejected if it cannot emit
both debug-layer and resource-lifecycle validation success markers.

macOS example:

```sh
python3 src/test-lib/run-normal-map-tests.py \
  --test-lib /absolute/path/testLib --engine /absolute/path/mini-mbm \
  --backend metal --normal 1 --lights 2 --require-native-validation \
  --output /tmp/normal-metal-on-2
```

The runner enables `MTL_DEBUG_LAYER=1` for Metal and waits for native resource-test
GPU completion. With `--require-native-validation`, the Metal resource step
also requires `Metal API Validation Enabled` and `NORMAL MAP METAL PIPELINES PASS`.
A clean log alone is not proof that the layer activated. DX11 validation markers currently apply to the C++ resource test;
the Lua visual suite does not expose its own info-queue validation hook.

Pass repeated `--library-dir PATH` options if DLL/shared libraries live elsewhere.
The runner prepends these and binary directories to loader paths, invokes binaries
directly without a shell, and writes `report.json`, logs and visual PNGs. It does
not build or switch flags: build each matrix entry first and snapshot its binaries
and libraries, since engine build directories share output locations. A timeout
is a failed test, not a skipped case. Review all failed logs before continuing.

The baseline runner executes build identity, preparation, persistence, Lua build
configuration, resources, async runtime/GC, readback and visual IB/VB. On
DX9 it additionally runs the SM2 profile characterization (ten steps total).
The SM2 step requires the known compiler capacity diagnostic as well as its
unlit bytecode/pixel/resource success markers; unexpected compiler errors fail it.
On Linux/GLES it also runs controlled context recreation automatically (ten baseline
steps, eleven with optional fault injection). Optional `--skeletal-parity` adds
the four-case native DX9/DX11 suite as one runner step; `-SkeletalParity` forwards
it from the Windows build script. Editor (ImGui),
abrupt device loss and native Windows/macOS lifecycle tests remain separate;
they are not implicitly covered by a baseline PASS.

### Infrastructure verification on Linux

The runner completed all nine baseline steps for GLES Release at cap 2 with
normal mapping ON and OFF (18 successful suite invocations). Its negative checks
rejected a mismatched light cap and a real child-process timeout. Four Python
unit tests cover success-marker requirements, failed exits, failure/driver
messages overriding PASS, timeout, and expected malformed-asset diagnostics.
Run them with `python3 src/test-lib/test-normal-map-runner.py`.

MSBuild project XML parses successfully. This does **not** compile its C++ sources.
No native DX9, DX11 or Metal compile/run is claimed. DirectX resource tests convert
the fixture's source vertex storage to writable storage before calling
`updateDynamic`; this avoids testing a dynamic-update API with static buffers.
The derived normal-map resources remain intact until the production invalidation
path is exercised.

Local reports for this preparation run are in `/tmp/mbm-normal-native-on-2` and
`/tmp/mbm-normal-native-off-2`; these are ephemeral artifacts, not repository
fixtures. Repeat the commands above on each target OS and retain their reports.


## Completed milestone: full Linux/GLES optimization matrix

Validated the source at commit `a5cb0125f0517d389d8e98acc1806878b14f5302`
(engine 7.327), independently of the earlier build-switch matrix. All eight
Release configurations compiled and all **72 runner steps passed**:

| `USE_NORMAL_MAPPING_3D` | `SUPPORTED_MAX_LIGHTS` | Runner steps | Result |
|---|---|---|---|
| 0 | 1 | 9/9 | PASS |
| 0 | 2 | 9/9 | PASS |
| 0 | 3 | 9/9 | PASS |
| 0 | 4 | 9/9 | PASS |
| 1 | 1 | 9/9 | PASS |
| 1 | 2 | 9/9 | PASS |
| 1 | 3 | 9/9 | PASS |
| 1 | 4 | 9/9 | PASS |

Each entry ran build identity, preparation, persistence, Lua build configuration,
native lazy-resource inspection, runtime/async/GC, readback and both indexed and
non-indexed visual comparisons. No engine or test changes were necessary.
This is baseline coverage, not an additional skeletal/editor/lifecycle matrix.

Environment: Linux 6.1.0-53-amd64, GCC 12.2.0, CMake 3.25.1, OpenGL ES 3.2
Mesa 22.3.6 on Intel UHD Graphics 630 (CFL GT2). Flags common to every entry:
`PLAT=Linux`, `CMAKE_BUILD_TYPE=Release`, `USE_LUA=1`, `AUDIO=none`,
`USE_TEXTURE_MISSING_DIALOG=0`. Each combination was reconfigured and built
sequentially with eight build jobs, then tested using its own copied executables
and libraries. Build warnings about existing `strncpy` calls were present;
this is not a warning-free-build claim.

For every enabled light cap, the three-vertex resource fixture reported three
active geometric attributes versus four mapped attributes, and derived GPU
payload growing from zero to 150 bytes only on effective first use. Disabled
builds passed geometric fallback and preserved 2dw visual assertions. These are
resource/behavior checks, not frame-time or aggregate-memory benchmarks.

Artifacts are retained locally under `/tmp/mbm-enabled-normal-matrix`:
`matrix.json` aggregates all results; each `<normal>-<lights>/` contains
configure/build logs, `CMakeCache.txt`, copied `bin/`, `sha256.json`, and
`tests/report.json`, suite logs, fixtures and visual PNGs. Artifacts in `/tmp`
are temporary. The pre-run shared Release binaries/libraries were restored
byte-for-byte; the normal Debug build configuration was not changed.

To reproduce, configure and build each pair using the flags above, snapshot the
binaries/libraries before building the next pair, and invoke
`src/test-lib/run-normal-map-tests.py --backend gles --normal <0|1> --lights <1..4>`
with the snapshot's `--test-lib` and `--engine`, and a new `--output` directory.
The complete single-entry invocation is documented above. A passing result from
one light cap must not substitute for another entry.

The failure/retry and controlled Linux context-recreation milestones are
completed below, followed by a reproducible synthetic Linux benchmark. Abrupt
device loss, representative-asset profiling and Windows/macOS native acceptance
remain pending.


## Completed milestone: Linux/GLES failure and retry

Initial delivery: 7.327. Shared CPU preparation: 7.328.1. `testLib --normal-map-failure-test` uses an explicitly loaded,
test-only ELF interposer to fail vertex compilation, fragment compilation,
program creation, linking, vertex upload and index upload. Index-upload failure
is repeated before a successful retry. The test checks:

- Failed draws return false and retain the original geometric shader.
- Failed shader attempts release temporary shaders/programs without uploading.
- A failed upload publishes no batches and deletes both temporary buffers;
  `glIsShader`, `glIsProgram` and `glIsBuffer` confirm actual object deletion.
- Strength zero still permits geometric rendering after failures.
- A retry reuses retained CPU staging and the successfully compiled mapped
  shader; twenty warm draws afterward perform no new compilation/allocation/upload.
- With the feature compiled out, mapped draws reach none of the injection points.

The initial regressions found two production bugs, now fixed:

| Failure | Before | Fix |
|---|---|---|
| `glCreateProgram()` returns zero | Both compiled shaders leaked | Delete both shader objects before returning failure |
| Vertex upload fails in Debug | A diagnostic `GLBindBuffer` wrapper consumed the GL error before the transaction check, allowing publication | Use raw GL calls inside the upload transaction so the error reaches its rollback decision |

No production fault switches, exported engine APIs or PIMPL state were added.
The injector is built only with `MBM_BUILD_GLES_FAULT_TESTS=ON` on Linux/GLES;
it is not linked to core_mbm or testLib and has an effect only with `LD_PRELOAD`.
Controls and counters are private to the test library and used on the GL thread.

Reproduce (cap 2 shown):

```sh
cmake -S . -B build/normal_faults -DPLAT=Linux -DCMAKE_BUILD_TYPE=Release \
  -DUSE_LUA=1 -DAUDIO=none -DUSE_TEXTURE_MISSING_DIALOG=0 \
  -DUSE_NORMAL_MAPPING_3D=1 -DSUPPORTED_MAX_LIGHTS=2 \
  -DMBM_BUILD_GLES_FAULT_TESTS=ON
cmake --build build/normal_faults --target testLib mini-mbm -j8
python3 src/test-lib/run-normal-map-tests.py \
  --test-lib bin/release/linux_x86/testLib --engine bin/release/linux_x86/mini-mbm \
  --backend gles --normal 1 --lights 2 --output /tmp/normal-recovery-on \
  --gles-fault-library bin/release/linux_x86/mbm-normal-map-faults.so
```

The optional runner flag adds the `recovery` step and loads the interposer
only into that step. Build identity, ordinary resources and Lua/visual tests run
without injection. Missing injector symbols are a failure, not a skip. For the
standalone command, generate fixtures with the persistence suite and set
`MBM_NORMAL_MAP_FIXTURE_DIR` and `LD_PRELOAD` explicitly. Use matching libraries,
an active display and an external timeout as for other native tests.

Scope: Release ON/OFF baseline plus recovery and the Debug ON recovery test,
all at cap 2. The earlier eight-entry/72-step baseline matrix remains historical
evidence for 7.327; this patch does not claim a new full matrix. Expected shader
compiler/linker diagnostics appear in Debug logs and are not unexpected errors;
require the test's final PASS and zero exit code.

The upload fixture has one batch. Upload errors are real `GL_INVALID_VALUE`
errors induced by a negative buffer size; program-creation failure simulates a
zero return. These test the failure contracts without exhausting GPU memory.
They do not simulate physical OOM, context loss, errors in later batches, or
DirectX/Metal failure semantics. Those remain separate requirements. Controlled
context recreation for pending and uploaded assets is covered by the subsequent
Linux milestone below.


Validation evidence: all ten runner steps passed in Release ON and OFF (20
steps total), and the dedicated Debug ON recovery test passed. Running the test
without the interposer correctly returned failure. Reports are retained locally
at `/tmp/mbm-fault-release-on-final` and `/tmp/mbm-fault-release-off-final`;
`/tmp/mbm-fault-debug-before.log` and `/tmp/mbm-fault-debug-after.log` capture the
Debug regression before and after the upload fix. `/tmp/mbm-fault-before.log`
records the initial program-creation leak. These paths are ephemeral evidence.
The original shared Debug testLib/core library were restored; shared Release
outputs contain the corrected enabled build at cap 2.


## Completed milestone: controlled Linux/GLES context recreation

`testLib --normal-map-context-test` runs two complete context replacement cycles
through `CORE_MANAGER::onStopCoreManager()` and `onLostDevice()`. This is the same
state machine used by `forceRestore()`, driven with a bounded step count and an
external process timeout. It destroys the old EGL context and creates a new one;
it does not merely clear shader instances or recreate the window surface.

The fixture uses persisted materials/tangent bases and a static animation so
real `MESH` instances participate in production object restoration. It contains:

- A detail-mapped asset rendered before the first context replacement.
- A separate mapped asset whose first upload is delayed until after both cycles.
- A second instance sharing the first asset and a geometric asset without a map.
- An unmanaged GL buffer sentinel absent from the replacement context before
  asset restoration. This confirms a fresh, unshared GL namespace rather than
  merely reloading managed assets; it is not a driver memory-leak measurement.

Assertions cover shared asset identity, preserved object position, empty derived
GPU batches immediately after restore, correct lazy upload, valid mapped versus
geometric shader programs, successful draws through the production object's
shader, and a clean final GL/EGL error state. Controlled 64x64 probe draws read
pixels from the current framebuffer: images must contain visible geometry,
mapping must change the image only in enabled builds, and images after each
restore must exactly match their respective pre-loss baselines. Camera/light
space is fixed for these comparisons. The pending asset's late first draw must
also match the mapped baseline.

The test is part of the runner by default on Linux/GLES; no fault interposer is
needed for this step. The existing single-entry runner commands apply unchanged.
For a standalone execution after generating persistence fixtures:

```sh
MBM_NORMAL_MAP_FIXTURE_DIR=/tmp/normal-fixtures \
LD_LIBRARY_PATH="$PWD/bin/release/linux_x86" \
  timeout -s KILL 40 bin/release/linux_x86/testLib --normal-map-context-test
```

Use an active graphical display, matching binaries/libraries, and
`USE_TEXTURE_MISSING_DIALOG=0`. The command must exit zero and emit
`NORMAL MAP CONTEXT PASS`; pixel mismatches, invisible geometry, stale resources,
restore failure or timeout are failures, not skipped cases.

Validation at cap 2: all eleven integrated runner steps passed in Release ON and
OFF (22 total, including optional fault injection); the dedicated context test
also passed in Debug ON. No production code changes were needed. The earlier
1..4-light matrix remains evidence for its recorded revision, not a claim that
this new test ran at all caps. Reports are retained locally at
`/tmp/mbm-context-release-on-verified` and `/tmp/mbm-context-release-off-verified`;
`/tmp/mbm-context-debug-detail.log` contains the Debug result. These paths are
ephemeral. Original Debug binaries were restored; Release outputs retain the
updated enabled test harness at cap 2.

Limits: this is controlled desktop EGL recreation, not an actual GPU reset,
Android pause/resume, or `EGL_CONTEXT_LOST` fault injection. It uses persisted
asset state, not unsaved material/geometry edits or in-flight asynchronous loads.
Scene/plugin-specific restore callbacks and DirectX/Metal lifecycle behavior
require their own coverage. Reproducible Linux measurements follow below; native
and abrupt-loss acceptance remain pending.


## Completed milestone: reproducible Linux/GLES measurements

`gles-normal-map-benchmark.cpp` extends testLib with fixture generation and a
bounded benchmark. `run-normal-map-benchmark.py` compares prebuilt enabled and
disabled snapshots, using identical fixture files and a fresh process for each
sample. It randomizes sample order with a recorded seed, retains raw logs and
SHA-256 hashes of fixtures/binaries, and writes samples and summaries to
`report.json`, including partial results when a run fails.

The six cases are `plain` (no tangent basis), `retained` (persisted tangents,
no map), `zero` (map with zero strength), `mapped` (both subsets mapped), `mixed`
(one mapped subset), and `removed` (both mapped on first draw, maps removed before
warm draws). All use a static, non-indexed grid with two subsets and directional
lighting, drawn into a 128x128 viewport. They exercise production load/material/
shader paths, with resource-contract checks and a visible-pixel sanity check.
The existing regression suites provide the stronger image/behavior comparisons.

Metrics, in microseconds unless named bytes/RSS:

- `load_sync_us`: asset load plus `glFinish`. Fixtures persist tangents but no
  normal textures; map assignment/texture creation is timed separately as
  `material_sync_us`. Fixture generation is outside measured processes.
- `geometric_compile_sync_us`: explicit geometric shader compilation, separate
  from first draw. `first_submit_us` and `first_sync_us` capture the first draw
  before/after `glFinish`, including any lazy mapped shader compilation/upload.
- `warm_submit_us` and `warm_sync_us`: per-draw wall times from blocks of draws
  after 16 warmup draws. Submission may include driver backpressure; synchronized
  time includes CPU work and waiting. Neither is an isolated GPU timer or FPS.
- `rss_before/loaded/first/warm`: process resident bytes from `/proc/self/statm`.
  RSS includes driver and allocator state; deltas cannot isolate engine staging,
  and retained allocator pages do not establish a resource leak.
- `source_gpu_bytes` and `derived_before/first/warm`: sums of unique live GL buffer
  payload sizes queried with `GL_BUFFER_SIZE`, outside timed draws. These exclude
  textures, programs and driver overhead, and are not physical GPU memory usage.

Use Release builds with the same options except `USE_NORMAL_MAPPING_3D`, at the
same `SUPPORTED_MAX_LIGHTS`. Reconfigure CMake after adding the new source. Copy
`testLib` and its matching `libcore_mbm.so` into separate ON/OFF directories before
building the next configuration: the project's build trees share output paths.
Ensure each library was actually relinked for its configuration; a newer output
from another build tree can otherwise appear up to date. Run on an active display:

```sh
python3 src/test-lib/run-normal-map-benchmark.py \
  --enabled /tmp/normal-on/testLib --disabled /tmp/normal-off/testLib \
  --lights 2 --grids 32 128 --repeats 5 --draws 32 --blocks 8 \
  --output /tmp/normal-benchmark
```

The output directory must be new. The runner enforces build identity, process
exit, PASS markers, sample identity and complete warm blocks; timeout, diagnostic
or resource-contract failures fail the run. It removes `LD_PRELOAD` and disables
Mesa's shader disk cache by default (`--allow-mesa-disk-cache` opts back in).
OS file caches are not flushed: this is not cold-disk I/O. Summaries report
median, nearest-rank p95 and range across independent processes; warm metrics
first take each process's block median. With five repetitions, p95 is simply the
maximum sample. There are no timing thresholds on a shared desktop.

This compares ON/OFF in the current implementation, not before/after revisions
of the optimization. Native backends, real game assets, skeletal/dynamic meshes,
GPU timestamp/frame profiling and isolated CPU allocation accounting remain
pending. No FPS or combined-memory reduction is claimed.

### Recorded Linux result (7.327.1 baseline)

All **120 samples passed**: ON/OFF x grids 32/128 (6,144/98,304 vertices) x six
cases x five processes, eight blocks of 32 warm draws. Release, light cap 2,
`USE_LUA=1`, `AUDIO=none`, missing-texture dialog disabled; Mesa 22.3.6, Intel UHD
Graphics 630, OpenGL ES 3.2. Existing runner unit checks also passed. This is
measurement coverage at cap 2, not a repeat of the earlier four-cap matrix.
No production changes were needed; Debug outputs were untouched and Release
outputs restored to the enabled build after snapshotting OFF.

For the larger grid, medians across the five processes:

| Build | Case | Load ms | First synchronized draw ms | Warm submission us/draw | Warm synchronized us/draw | Derived buffer MiB |
|---|---|---:|---:|---:|---:|---:|
| OFF | plain | 30.52 | 1.68 | 37.05 | 1105.52 | 0.00 |
| OFF | retained | 52.22 | 14.04 | 37.86 | 1228.93 | 0.00 |
| OFF | zero | 48.15 | 2.17 | 36.48 | 1261.05 | 0.00 |
| OFF | mapped | 48.34 | 1.10 | 36.35 | 1144.22 | 0.00 |
| OFF | mixed | 59.39 | 12.40 | 40.43 | 1211.26 | 0.00 |
| OFF | removed | 50.32 | 7.69 | 38.09 | 1279.93 | 0.00 |
| ON | plain | 29.49 | 6.75 | 37.60 | 1284.11 | 0.00 |
| ON | retained | 46.00 | 13.66 | 36.50 | 1205.18 | 0.00 |
| ON | zero | 55.66 | 8.91 | 36.31 | 1152.48 | 0.00 |
| ON | mapped | 53.62 | 57.15 | 40.13 | 1589.05 | 4.69 |
| ON | mixed | 50.95 | 62.72 | 52.01 | 1504.64 | 4.69 |
| ON | removed | 40.78 | 51.18 | 37.73 | 1230.21 | 4.69 |

The source buffer payload is 3 MiB in every larger-grid case. Derived buffers
start at zero for every sample. ON `mapped`, `mixed` and `removed` allocate
4,915,200 bytes at first use and retain them through warm draws. `mixed` therefore
confirms frame-wide upload rather than per-subset upload, and `removed` confirms
retention rather than eviction. ON `plain`, `retained` and `zero`, and every OFF
case, keep zero derived bytes. The smaller grid follows the same contract
(196,608 source bytes and 307,200 derived bytes when active).

For `retained` on the larger grid, median load RSS growth is about 10.64 MiB ON
versus 5.70 MiB OFF; `plain` is about 3.4 MiB in both. This observation is consistent
with retained preparation/staging costs, but process RSS does not isolate those
allocations. Warm submission medians for geometric cases are close here; noisy
first-use times and shared-desktop scheduling do not justify an FPS claim or a
small percentage improvement. Use the report's ranges and fresh measurements on
target hardware before making performance decisions.

Raw logs, sample records, hashes and distribution summaries are retained locally
in `/tmp/mbm-normal-benchmark-release/report.json` and adjacent files; these are
ephemeral artifacts. Re-run the command above to obtain a new baseline. The next
Linux optimization candidate is reducing unused CPU staging or using per-subset
upload/eviction, with these measurements and regression tests as a baseline.


## Completed milestone: shared immutable CPU preparation (7.328)

Runtime `MESH_MBM::Impl` now owns immutable prepared asset frames through a private
shared owner. `stageStatic` retains an alias to the frame's `PREPARED` instead of
copying its tangent batches, source-vertex mappings and indices. Source geometry
is still staged for late upload. This changes neither serialized sections nor
public API/header ownership boundaries.

The alias keeps its owner alive until upload succeeds or the buffer is invalidated
or released. Failed uploads preserve the reference and geometry for retry.
Successful upload drops the staging reference; the asset retains preparation for
readback. Extraction into `MESH_MBM_DEBUG` makes an independent mutable copy, so
authoring edits cannot mutate runtime preparation. Context recreation rebuilds
staging from the reloaded asset. There is no new ownership operation per draw;
sharing happens during load and ends during upload/invalidation.

The GLES resource test now explicitly checks reference sharing, survival after the
caller releases its reference, release after successful upload, and release when
pending geometry is invalidated. Existing tests cover late map assignment,
retry, extraction/editing, shared instances, dynamic invalidation and context
recreation. The shared implementation serves all backends; DirectX/Metal still
require native compilation and execution, and this does not extend the earlier
four-light-cap evidence to the new revision.

For the synthetic 98,304-vertex fixture, avoiding the duplicate arrays removes
2,162,688 bytes (2.0625 MiB) of preparation payload from pending staging: 4 bytes
of source mapping + 16 bytes of tangent + 2 bytes of index per vertex in this
particular fixture, excluding container/allocator overhead. The 3 MiB source
geometry staging remains until upload. Other meshes can have different prepared
vertex/index counts; this is not a universal bytes-per-source-vertex formula.
Per-subset upload/eviction and further geometry staging reduction remain future
work. Derived GPU allocation and shading behavior are unchanged.


Validation: Release Linux/GLES at cap 2 passed all eleven integrated runner steps
in ON and OFF (22 total, including injected upload failure/retry and controlled
context recreation), the four runner unit checks, and all 120 benchmark samples.
For the larger `retained` fixture, median load RSS growth was 8.61 MiB ON versus
5.69 MiB OFF; the earlier 7.327.1 baseline recorded 10.64 MiB ON. This is a
comparison of separate desktop runs, not a controlled timing or exact allocation
measurement. The deterministic saving is the removed preparation payload above.
No-map derived buffer payload remains zero; active/mixed/removed larger fixtures
still retain 4,915,200 derived GPU bytes, as before.

Local reports: `/tmp/mbm-shared-staging-on/report.json`,
`/tmp/mbm-shared-staging-off/report.json` and
`/tmp/mbm-shared-staging-benchmark/report.json` (ephemeral). Reproduce using the
existing suite and benchmark commands, after building matching 7.328 snapshots.
Debug outputs were untouched; shared Release outputs were restored to ON, cap 2.


## First Windows milestone: DX11 Debug ON/OFF at cap 2 (2026-09-30)

Validated engine 7.328 at source commit
`05db41c1e730119556af11ff2c0e021e3bc48340` using Visual Studio 2026
(MSVC v145), Debug x86, DirectX11 feature level 11_0, PortAudio and Python 3.14.5.
Both `MbmUseNormalMapping3D=1` and `0`, with `MbmSupportedMaxLights=2`, compiled
and passed all nine integrated runner steps: **18/18 passed**. The missing-texture
picker was disabled through the command-line `MbmCoreFeatureDefines` override.
No production engine changes were required.

Both entries passed build identity, CPU preparation, persistence, Lua capability,
native resources, async runtime/GC, readback and indexed/non-indexed visual tests.
The native resource tests emitted both DX11 debug-layer and post-teardown
resource-lifecycle success markers. These markers apply to that C++ test; the
Lua visual suite still has no separate info-queue validation hook.

The enabled three-vertex fixture had zero derived buffer payload before effective
use and 150 bytes after use, and passed resource reuse, shader selection/restore,
late assignment and dynamic invalidation assertions. The disabled entry passed
the geometric fallback and no-derived-upload contract. Both preserved 2dw mapping.
Enabled indexed/non-indexed images had a mean detail difference of 1.9323 from
the baseline, and zero difference after strength zero or map removal. These are
regression metrics, not performance measurements.

`platform-msvs/run-normal-map-tests.ps1` now builds and verifies one entry in a
new output directory. It isolates executable/library outputs and per-project
intermediates using an imported test-only MSBuild property file, preserving the
usual development outputs and local backend preferences. Debug DX11 automatically
requires native validation markers. The four Python runner unit checks also passed.

From the repository root, reproduce with new output paths:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 2 -Output build/normal-windows/dx11-on-2-new
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 0 -Lights 2 -Output build/normal-windows/dx11-off-2-new
```

Local evidence is retained in `build/normal-windows/dx11-on-2/results/report.json`
and `build/normal-windows/dx11-off-2/results/report.json`, with adjacent suite
logs and PNGs; their parent directories retain build logs and isolated binaries.
These ignored build artifacts are local evidence, not committed fixtures.

This first milestone covered the Windows DX11 baseline and its Debug validation
check at cap 2 only. Caps 1/3/4 are completed in the subsequent milestone below.
The DX9 matrix/SM2 fallback, Windows skeletal parity,
native failure injection/device loss, shader instruction/capture analysis and
Release performance measurements remain pending, as does macOS acceptance.
The full native-matrix checklist above is intentionally still open.


## Completed Windows milestone: full DX11 Debug matrix (2026-09-30)

Engine 7.328 now passes all eight DX11 Debug x86 configurations, with normal
mapping ON/OFF and light caps 1..4: **72/72 integrated runner steps passed**.
The six new entries at caps 1/3/4 contributed 54 passes at commit
`1a2acbff8fb8fc62df295460664c3ca771410638`. The cap-2 results are the 18 passes
from the first Windows milestone above, not newly executed tests. The commits
differ only in documentation and the PowerShell runner; engine/test sources are
unchanged. All eight entries' executable/DLL hashes were verified against their
retained manifests before combining the evidence. No production changes were
required, and normal development outputs were untouched.

| `MbmUseNormalMapping3D` | `MbmSupportedMaxLights` | Steps | DX11 resource debug/lifecycle checks |
|---|---|---|---|
| 0 | 1 | 9/9 PASS | PASS |
| 0 | 2 | 9/9 PASS (previous milestone) | PASS |
| 0 | 3 | 9/9 PASS | PASS |
| 0 | 4 | 9/9 PASS | PASS |
| 1 | 1 | 9/9 PASS | PASS |
| 1 | 2 | 9/9 PASS (previous milestone) | PASS |
| 1 | 3 | 9/9 PASS | PASS |
| 1 | 4 | 9/9 PASS | PASS |

Each entry verifies C++/Lua build identity, CPU preparation, persistence, native
lazy resources, async runtime/GC, readback and indexed/non-indexed visual behavior.
Lua build checks validate both 3d/2dw light caps, reject requests above the cap,
and inspect reserved shader loop/array lengths and normal-map source removal.
The native resource test passes with both required debug-layer and post-teardown
lifecycle markers in every entry. This remains resource-test validation, not
info-queue coverage for the Lua visual scenes or shader instruction analysis.

Toolchain/configuration matches the first milestone: Visual Studio 2026/MSVC
v145, Debug x86, DX11 feature level 11_0, PortAudio, Python 3.14.5 and no
missing-texture picker. Reproduce a fresh complete matrix from the repository root:

```powershell
foreach ($normal in 0, 1) {
    foreach ($lights in 1, 2, 3, 4) {
        powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal $normal -Lights $lights -Output "build/normal-windows/recheck-dx11-$normal-$lights"
        if ($LASTEXITCODE -ne 0) { throw "DX11 entry failed: normal=$normal lights=$lights" }
    }
}
```

Output directories must be new. Local reports are
`build/normal-windows/dx11-{on,off}-{1,2,3,4}/results/report.json`, with suite logs,
PNG comparisons, build logs and binary hash manifests alongside. The consolidated
local `build/normal-windows/dx11-matrix-summary.json` records all eight entries,
source revisions and validation/hash checks. These are ignored local artifacts.

The subsequent milestone below completes the DX9 SM3 matrix. DX9 SM2 fallback, native skeletal
parity, failure injection/device loss, GPU capture/instruction analysis, Release
measurements and macOS acceptance remain separate outstanding work. This Debug
matrix establishes regression coverage, not an FPS or memory-performance gain.


## Completed Windows milestone: full DX9 SM3 Debug matrix (2026-09-30)

All eight DX9 SM3 Debug x86 entries now pass: **72/72 integrated runner steps**,
normal mapping ON/OFF at light caps 1..4. Validation used base commit
`404092d313eb822620c705901e9b834e5f711e15` (engine 7.328) plus the two production
compilation fixes and the test-only SM3 assertion delivered with this milestone.
Visual Studio 2026/MSVC v145, the existing DirectX June 2010 SDK, PortAudio,
Python 3.14.5 and the disabled missing-texture picker match the isolated Windows
runner setup. No public API, ownership boundary or asset format changed.

Native compilation exposed two previously unverified DX9 defects:

| Defect | Fix | Verification |
|---|---|---|
| A `#if` inside the argument of the `FAILED` macro in the indexed vertex-declaration path failed MSVC preprocessing | Select the declaration in a local pointer before the macro call; retain lazy fallback lookup | All eight native builds and indexed visual/resource tests pass |
| `SPECIFIC_AUX_CONTEXT_DEVICE::release()` reset `normalMapUnsupportedReported` even when its field was compiled out | Guard the reset with the same `USE_NORMAL_MAPPING_3D` condition as the field | All four OFF builds and suites pass |

The native resource suite now explicitly requires device support for SM3 and
selected `vs_3_0`/`ps_3_0` profiles, emitting
`NORMAL MAP DX9 PROFILES PASS vs_3_0 ps_3_0`. This check runs in both ON and OFF
entries. It does not exercise forced SM2 fallback or inspect shader instructions.

| `MbmUseNormalMapping3D` | `MbmSupportedMaxLights` | Steps | SM3 profiles |
|---|---|---|---|
| 0 | 1 | 9/9 PASS | PASS |
| 0 | 2 | 9/9 PASS | PASS |
| 0 | 3 | 9/9 PASS | PASS |
| 0 | 4 | 9/9 PASS | PASS |
| 1 | 1 | 9/9 PASS | PASS |
| 1 | 2 | 9/9 PASS | PASS |
| 1 | 3 | 9/9 PASS | PASS |
| 1 | 4 | 9/9 PASS | PASS |

Every entry passed C++/Lua build identity, preparation, persistence, native
resources, async runtime/GC, readback and indexed/non-indexed visual tests.
The enabled resource fixture reports zero derived payload before use and 150
bytes after first use; shader/cache identity, reuse, late assignment, shader
restore and dynamic invalidation checks pass. Disabled builds retain geometric
3D shading and 2dw mapping. At cap 2, enabled indexed images have mean detail
difference 1.8088 and zero difference after strength zero or map removal.
These are regression checks, not performance measurements. Unlike the DX11
resource suite, this DX9 run does not claim debug-layer/live-object validation.

Reproduce from the repository root with new output directories:

```powershell
foreach ($normal in 0, 1) {
    foreach ($lights in 1, 2, 3, 4) {
        powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx9 -Normal $normal -Lights $lights -Output "build/normal-windows/recheck-dx9-$normal-$lights"
        if ($LASTEXITCODE -ne 0) { throw "DX9 entry failed: normal=$normal lights=$lights" }
    }
}
```

Local evidence: `build/normal-windows/dx9-matrix-summary.json` lists all eight
reports and verified binary hashes. Entry directories are
`dx9-{on,off}-{1,2,3,4}` beneath that directory; final ON cap-1/2 reports use
`results-verified/report.json`, and the other six use `results/report.json`.
Logs, PNGs and binaries remain adjacent. The initial cap-2 build failures are
preserved as `build-before-fix.log` in their respective entries. The retained
`dx9-validation.patch` and `dx9-source-hashes.json` identify the tested changes.
All artifacts are ignored local evidence; normal development outputs were untouched.

The subsequent milestone below characterizes DX9 SM2. Native
skeletal parity, failure injection/device loss, shader capture/instruction
analysis and Release performance measurements remain pending, as does macOS
acceptance. The earlier DX11 matrix is retained as its recorded evidence; this
DX9-only production fix does not claim a new DX11 matrix run.


## Completed Windows milestone: SM2 profile characterization (2026-09-30)

The fallback has now been measured rather than inferred: **default geometric
lighting does not compile under strict `ps_2_0` at any light cap 1..4**. The
profile guard removes the tangent path, but the remaining lighting shader itself
exceeds SM2 capacity. There is no automatic replacement with an unlit shader.
Functional SM2 lighting would require a separately designed reduced lighting
model; this milestone does not introduce one or change production behavior.

| Light cap | ON and OFF compiler result for default lighting | Separate unlit shader |
|---|---|---|
| 1 | X5608/X5609: 106 arithmetic slots versus 64 allowed | SM2 bytecode/pixels/resources PASS |
| 2 | X5608/X5609: 163 arithmetic slots versus 64 allowed | SM2 bytecode/pixels/resources PASS |
| 3 | X4505: temporary register limit exceeded | SM2 bytecode/pixels/resources PASS |
| 4 | X4505: temporary register limit exceeded | SM2 bytecode/pixels/resources PASS |

The old `--directx9-normal-map-shader-test` selected SM2 after compiling SM3
without clearing the default-program cache, whose key does not include profiles.
It could reuse SM3 bytecode. That test now clears the cache before the switch.
The new `--normal-map-sm2-test` also clears it, sets the profiles through the
existing API and restores them on exit. It checks bytecode version tokens from
both actual shader objects, not merely the selected profile strings.

The test loads a persisted prepared triangle, verifies no mapping interface or
derived buffers, and renders the separate unlit shader into a 64x64 DX9 target.
Readback must contain visible geometry. Assigning a normal map at nonzero strength,
repeated draws, strength zero and removal must preserve the baseline pixels and
leave derived buffers, tangent declaration, fallback tangent buffer and mapping
constants absent. The lit shader's expected compile failure is checked separately;
no lit pixel parity is claimed when that shader cannot compile.

All eight ON/OFF x 1..4-light Debug x86 configurations passed the expanded runner:
**80/80 steps**, comprising 72 repeated SM3 baseline steps and eight SM2 checks.
Five Python runner unit tests passed, including rejection of missing capacity
diagnostics and unexpected compiler errors even alongside a known capacity error.
SM2 PASS means the capacity limitation and unlit contract were verified, not that
lighting succeeded. No production engine changes were needed.

Validation used base commit `e4f94d4bf64c3b643c27e77a913c397571c18722`, engine
7.328, with the test/runner changes in this milestone, Visual Studio 2026/MSVC
v145 and the existing DX9 SDK. This forces SM2 compilation on an SM3-capable
device; hardware caps are not falsified. It does not validate an old SM2 GPU,
2dw lighting under SM2, arbitrary custom SM2 shaders, device loss, or profile
changes with live cached shaders in production.

The existing PowerShell matrix commands above now execute the extra DX9 step
automatically. For just the new test, first generate persistence fixtures, then:

```powershell
$env:MBM_NORMAL_MAP_FIXTURE_DIR = 'C:/path/to/results/fixtures'
# The integrated Python runner supplies a process timeout and validates diagnostics.
& 'C:/path/to/libTest.exe' --normal-map-sm2-test
```

Local final reports are
`build/normal-windows/dx9-{on,off}-{1,2,3,4}/results-sm2-final/report.json`.
The consolidated `build/normal-windows/dx9-sm2-summary.json` records all eight
results and their capacity diagnostics. Per-entry `binary-hashes-sm2.json`
identifies the refreshed test binaries; earlier manifests describe the historical
SM3 run. Old reports are retained. Normal development outputs were untouched.

The subsequent milestone below completes native numerical skeletal parity with
the normal-mapping switch ON/OFF. Native failure/device-loss, Release profiling
and macOS acceptance remain outstanding.


## Completed Windows milestone: native skeletal parity (2026-09-30)

All **16 DX9/DX11 Debug x86 configurations** (normal mapping ON/OFF x light caps
1..4 per backend) pass the native GPU/CPU parity suite: **64/64 cases**. Each
entry captures GPU positions and normals for synthetic LBS/DQS (two vertices
per case) and Lorekeeper LBS/DQS (eight selected vertices at 37% of the first
clip). The capture shaders use the shared production deformation generators;
the CPU reference and RGBA8 encoding/comparison are provided by the shared suite.

The reported values below were identical at the log's seven-decimal precision
across all 16 entries. Position errors are in fixture units; tolerances include
RGBA8 quantization and are not assertions of bit-exact CPU/GPU equality.

| Fixture | Method | Max position error | Position tolerance | Max normal error | Normal tolerance |
|---|---|---:|---:|---:|---:|
| synthetic | LBS | 0.0021373 | 0.0072246 | 0.0039216 | 0.0098431 |
| synthetic | DQS | 0.0022525 | 0.0075061 | 0.0039216 | 0.0098431 |
| Lorekeeper | LBS | 0.1977425 | 0.3710776 | 0.0035972 | 0.0098431 |
| Lorekeeper | DQS | 0.1922569 | 0.3715830 | 0.0039960 | 0.0098431 |

The integrated runner also repeated the prior regressions: **168/168 steps**
passed (88 DX9, including eight SM2 characterization steps; 80 DX11). DX11
debug-layer and post-teardown lifecycle success markers were required for both
the resource and skeletal-parity steps in every DX11 entry. Five Python runner
unit tests passed. DX9 does not claim equivalent debug-layer/live-object coverage.

`--skeletal-parity` adds this optional suite to the Python runner. It requires
all four fixture/method markers, the aggregate four-case PASS, a zero exit code
and no recognized failure/driver diagnostic. `-SkeletalParity` forwards the
option from `platform-msvs/run-normal-map-tests.ps1`. Existing prebuilt snapshots
from the Windows matrices were used after verifying every executable/DLL hash;
no engine or C++ test changes were necessary. Repository revision was
`0660aa5a350e4b554b41464cb4836c6b40250d67` plus the runner/documentation changes
for this milestone. Toolchain/runtime remain the recorded 7.328 Debug x86 setup.

For a fresh build and run from the repository root (repeat for each backend,
normal-mapping value and light cap, always with a new output directory):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 2 -SkeletalParity -Output build/normal-windows/recheck-skeletal-dx11-on-2
```

For matching prebuilt executables, add `--skeletal-parity` to the existing Python
invocation; retain `--require-native-validation` for DX11 Debug. Each child has
the runner's timeout and its own log. Missing Lorekeeper/case output is a failure,
not a skip.

Local evidence is consolidated in `build/normal-windows/skeletal-matrix-summary.json`,
with all 16 reports at `{dx9,dx11}-{on,off}-{1,2,3,4}/results-skeletal/report.json`
under that directory. `skeletal-parity.log` retains all measured errors, selected
vertices, clip/time and DX11 validation markers. The summary links the previously
verified binary manifests. Earlier reports and development outputs were untouched.

Scope: sampled numerical deformation in capture shaders, not full animation,
automatic CPU/GPU execution policy, lighting/material parity of production mesh
draws or tangent-space normal mapping on skinned geometry. The latter remains
outside the static normal-mapping feature. The next Windows milestone is native
failure-and-retry coverage, beginning with DX11 (completed at cap 2 below);
device loss, Release profiling and macOS acceptance remain separate outstanding work.

## Completed Windows milestone: DX11 resource failure and retry (2026-09-30)

DX11 **Debug x86 ON/OFF at light cap 2** now validates native resource-creation
failure and recovery. The final integrated runs passed **22/22 steps**, including
the existing numerical skeletal parity, runtime/readback and visual IB/VB suites.
The recovery suite passed all 11 cases in each build. ON injects `E_OUTOFMEMORY`
at the following creation calls; OFF verifies that none of these mapped creation
calls is made and no injected failure is reached:

| Resource | Injected call in the first mapped draw |
|---|---|
| Vertex shader | `CreateVertexShader`, first call |
| Pixel shader | `CreatePixelShader`, first call |
| Input layout | `CreateInputLayout`, first call |
| Default / nearest sampler | `CreateSamplerState`, calls 1 / 2 |
| Matrix / light constant buffer | `CreateBuffer`, calls 1 / 2 |
| Normal settings / zero tangent | `CreateBuffer`, calls 3 / 4 |
| Derived vertex / index buffer | `CreateBuffer`, calls 5 / 6 |

`src/test-lib/normal-map-directx11-failure-tests.cpp` uses a synchronous,
test-only forwarding `ID3D11Device` proxy. It temporarily replaces the internal
device pointer during each case and restores it before shader/engine teardown.
No fault controls or public API were added to the engine. Every case uses a
separate prepared mesh fixture and compiles its geometric shader before injection.

For ON, a failed draw must return false, leave derived batches unpublished and
release any partial derived buffers. Shader-creation failures must also release
their temporary constant/zero-tangent buffers. Test-only COM private-data tokens
track buffer lifetime with independent counters per case; they hold no device or
resource references. Shader and sampler objects may be interned by the runtime,
so whole-pipeline leak checking uses the existing post-teardown DX11 validator.
The initial probe exposed a test-counter error caused by delayed release of a
previous case's bound buffers; per-case counters fixed that accounting without
changing engine code.

Setting strength to zero after failure must still select the original geometric
shader without creating resources. The upload cases then fail a second time at
the derived index buffer: partial storage must be discarded again, while the
compiled mapped variant survives. Removing injection must allow upload using
retained preparation, creating only the two missing derived buffers. Twenty
subsequent mapped draws per ON case must create no new pipeline/buffer resources.
Debug-layer and post-teardown lifecycle success markers were required for
resources, skeletal parity and recovery in both final runs. Five runner unit
tests also passed.

Reproduce from the repository root, using a fresh output directory per run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 2 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-recovery-on-2
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 0 -Lights 2 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-recovery-off-2
```

For matching prebuilt executables, the Python runner accepts `--dx11-failure`;
retain `--require-native-validation` for Debug. It requires all 11 named case
markers, the aggregate PASS for the requested ON/OFF value, zero exit status and
no recognized failure/driver diagnostic. Other backends reject this option.

Local final evidence is in
`build/normal-windows/recovery-dx11-{on,off}-2/results-verified/report.json` and
`recovery.log`, with build metadata/logs beside them. Updated executable/DLL hashes
are in `binary-hashes-recovery.json`; the original `results/` reports and manifests
remain the initial pre-correction evidence. The combined summary is
`build/normal-windows/dx11-recovery-summary.json`. Toolchain is VS 2026/MSVC v145,
Windows SDK 10.0.28000.0, Python 3.14.5, engine 7.328; no production change or version
bump was needed.

Scope is COM creation failure for the static, single-batch mapped path. This does
not simulate an actual memory shortage, HLSL compiler failure, `Map` failure,
multi-batch rollback or real device loss, nor compare pixels during recovery.
The remaining light caps are covered by the following milestone. DX9/Metal fault
injection and Release profiling remain separate work.

## Completed Windows milestone: full DX11 recovery matrix (2026-09-30)

DX11 resource failure/retry now covers every **Debug x86 ON/OFF x light cap
1/2/3/4** entry. Six fresh isolated builds at caps 1, 3 and 4 passed **66/66
integrated runner steps**. Combined with the verified cap-2 evidence above, the
matrix passes **88/88 steps** across eight configurations:

| Light cap | ON steps | OFF steps | Evidence |
|---|---:|---:|---|
| 1 | 11/11 | 11/11 | New builds and runs |
| 2 | 11/11 | 11/11 | Prior final runs; reports and binary hashes rechecked |
| 3 | 11/11 | 11/11 | New builds and runs |
| 4 | 11/11 | 11/11 | New builds and runs |

Each entry requires all 11 recovery case markers: **44 ON cases** exercise
injected COM creation failures, cleanup and retry; **44 OFF cases** verify that
mapped resource creation is absent. The integrated suites also cover numerical
skeletal parity, CPU preparation/persistence, Lua configuration/runtime/readback,
native lazy resources and visual IB/VB regression. Debug-layer and post-teardown
lifecycle success markers were verified for resources, skeletal parity and
recovery in all eight entries. No engine or test-code changes were necessary;
the engine version remains 7.328.

New runs use revision `b442db3bc4a1577e66eecb53a95eeb9e1feb46ba`, VS 2026/MSVC
v145, Windows SDK 10.0.28000.0 and Python 3.14.5. The cap-2 harness/runner source
hashes still match after accounting for CRLF-to-LF normalization of the proxy
header; its executable/DLL hashes match exactly. The cap-2 steps were not rerun.

Reproduce each new entry with the existing PowerShell command, setting `-Normal`
to 0 or 1 and `-Lights` to 1, 3 or 4; always use a fresh output directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 4 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-recovery-on-4
```

New reports/logs/PNGs are at
`build/normal-windows/recovery-dx11-{on,off}-{1,3,4}/results/`, beside `build.json`,
`build.log` and `binary-hashes.json`. The combined evidence index is
`build/normal-windows/dx11-recovery-matrix-summary.json`; it links all eight
reports and manifests and distinguishes the six new runs from the two prior
cap-2 runs. All recorded executable/DLL hashes were checked during consolidation.

This matrix covers static single-batch COM resource creation. The following
milestone adds DX11 multi-batch rollback/retry at cap 2. Compiler/Map failures,
actual device loss, DX9/Metal injection and Release performance acceptance remain
outstanding.

## Completed Windows milestone: DX11 multi-batch rollback and retry (2026-09-30)

The DX11 recovery suite now has **15 cases**, validated in fresh **Debug x86
ON/OFF builds at light cap 2**. Both runs passed all 15 cases and **22/22
integrated runner steps**, including the existing numerical skeletal parity,
runtime/readback and visual IB/VB suites. Debug-layer and post-teardown lifecycle
success markers were required for resources, skeletal parity and recovery.
The five Python runner unit tests also passed. No production changes were needed;
engine version remains 7.328.

Four new cases extend the 11-case suite:

| Fixture | Failure target | First draw `CreateBuffer` ordinal | Retry ordinal |
|---|---|---:|---:|
| One subset, two derived batches | Second batch vertex buffer | 7 | 3 |
| One subset, two derived batches | Second batch index buffer | 8 | 4 |
| Two subsets, one derived batch each | Second subset vertex buffer | 7 | 3 |
| Two subsets, one derived batch each | Second subset index buffer | 8 | 4 |

The partition fixture is built through `MESH_MBM_DEBUG` authoring/import/save:
65,538 non-indexed source vertices exceed the 16-bit derived-batch limit and
produce two batches in a single subset. Its 21,846 repeated triangles are kept
subpixel to avoid excessive overdraw; this is resource coverage, not a pixel
comparison. The second fixture reuses `author-subsets.msh`, whose two subsets
already carry independently prepared bases. Both are loaded through the normal
mesh manager before drawing.

ON cases count successful derived-buffer creations before each failure: two
buffers must have been created before a second-batch vertex failure, or three
before an index failure. After each failed draw, all derived-buffer lifetime
tokens must be released and no subset/batch may be published. Strength zero must
still draw with the original geometric shader without resource creation. A
second injected upload failure verifies rollback again with the mapped shader
already cached. Removing injection must create all four derived buffers, publish
the expected batch/subset distribution and preserve the total corner count.
Native buffer descriptors verify expected vertex/index storage sizes. Twenty
subsequent draws per ON case must create no new tracked resources. OFF cases
require successful geometric rendering with no mapped creation calls or injected
failure reached, including both larger fixtures.

`--dx11-failure` / `-Dx11Failure` now require all 15 named case markers and the
15-case aggregate PASS. An older 11-case binary cannot satisfy the updated runner.
The prior eight-entry matrix remains evidence for the original 11 cases only;
the expanded 15-case matrix is recorded in the following milestone.

Reproduce with a new output directory for each ON/OFF value:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 2 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-multibatch-on-2
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 0 -Lights 2 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-multibatch-off-2
```

Local evidence is under `build/normal-windows/multibatch-dx11-{on,off}-2/`:
`results/report.json`, `results/recovery.log`, remaining suite logs/PNGs, build
metadata/logs and `binary-hashes.json`. The evidence index is
`build/normal-windows/dx11-multibatch-summary.json`. Toolchain remains VS 2026/MSVC
v145, Windows SDK 10.0.28000.0 and Python 3.14.5. Earlier artifacts are unchanged.

Scope is static upload rollback across two batches/subsets, including repeated
failure, retry and warm reuse. Buffer contents/pixels during recovery, failures
after more than two batches, compiler/Map failures and actual device loss are
not covered here. The following milestone extends this 15-case suite to caps
**1, 3 and 4**, ON/OFF. DX9/Metal injection and Release profiling remain separate
work.

## Completed Windows milestone: full DX11 multi-batch matrix (2026-09-30)

The expanded **15-case** suite now passes all **eight DX11 Debug x86 ON/OFF x
light cap 1/2/3/4** entries. Six fresh isolated builds at caps 1, 3 and 4 passed
**66/66 integrated steps**. Together with the verified prior cap-2 runs, this
gives **88/88 steps** and **120/120 recovery-suite cases**:

| Light cap | ON cases | OFF cases | Integrated steps | Evidence |
|---|---:|---:|---:|---|
| 1 | 15/15 | 15/15 | 22/22 | New builds and runs |
| 2 | 15/15 | 15/15 | 22/22 | Prior runs; reports, sources and binaries verified |
| 3 | 15/15 | 15/15 | 22/22 | New builds and runs |
| 4 | 15/15 | 15/15 | 22/22 | New builds and runs |

The **60 ON cases** exercise injection, rollback, retry and warm reuse; the
**60 OFF cases** verify successful geometric draws without mapped resource
creation or injected failures being reached. This includes second-batch vertex
and index failures within one subset and across two subsets at every light cap.
Each entry also runs CPU preparation/persistence, Lua build configuration,
native lazy resources, numerical skeletal parity, runtime/readback and visual
IB/VB regression. Debug-layer and post-teardown lifecycle success markers were
checked for resources, skeletal parity and recovery in all eight entries.

No engine or test-code changes were required. New runs use revision
`3f96b72fe123ec68a55aaeeb8acb3f1cd36ebbf0`, engine 7.328, VS 2026/MSVC v145,
Windows SDK 10.0.28000.0 and Python 3.14.5. The cap-2 test/proxy/runner source
hashes and executable/DLL hashes still match exactly; its 22 steps were not rerun.
All eight binary manifests were verified during consolidation.

Reproduce each entry with the existing command, varying `-Normal` (0/1) and
`-Lights` (1/3/4), always choosing a fresh output directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 4 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-multibatch-on-4
```

New evidence is under `build/normal-windows/multibatch-dx11-{on,off}-{1,3,4}/`:
`results/report.json`, per-suite logs/PNGs, `build.json`, `build.log` and
`binary-hashes.json`. The combined index is
`build/normal-windows/dx11-multibatch-matrix-summary.json`; it links all eight
reports/manifests and distinguishes the six new runs from the prior cap-2 runs.
Earlier artifacts remain unchanged.

This matrix covers resource contracts, not pixel equality during recovery or
actual device loss. The following milestone adds pixel comparison at cap 2.
Compiler/Map failures, failures after more than two batches, DX9/Metal injection
and Release profiling remain separate work.

## Completed Windows milestone: DX11 recovery pixel comparison (2026-09-30)

All **15 recovery cases** now include native offscreen pixel comparisons,
validated in **Debug x86 ON/OFF at light cap 2**. The final runs passed **30/30
pixel cases and 22/22 integrated steps**, including the prior resource contracts,
skeletal parity, runtime/readback and visual IB/VB suites. Debug-layer and
post-teardown lifecycle validation passed for resources, skeletal parity and
recovery in both builds. Five Python runner unit tests passed. No production
changes were needed; engine version remains 7.328.

The test creates a 64x64 `R8G8B8A8_UNORM` target and CPU-readable staging texture
before installing the fault proxy. A separate asset copy and shader produce
geometric and mapped references without injection. Directional and ambient light
are explicitly configured. The capture aligns the camera's light-space view
with the identity geometry matrices and restores it, along with the previous
render target, viewport, depth, blend and rasterizer state, on exit.

The first ON run correctly failed the contrast assertion: camera view state
had not been updated by the normal frame loop, so both references showed only
ambient lighting. Aligning the test's coordinate spaces fixed the capture; the
contrast requirement was retained. The initial reports remain separate evidence.

The partition fixture now exposes one triangle from each of its two batches in
separate image halves; the remaining repeated triangles stay subpixel. The
two-subset fixture is generated with two separated triangles, replacing the
overlapping `author-subsets.msh` fixture for this suite. Reference visibility is
required globally and in both halves for the multi-batch cases. ON must also
show a mapped/geometric difference in each populated half; OFF must show none.

| Cases | Visible reference pixels | ON mapped/geometric changed pixels | OFF changed pixels |
|---|---:|---:|---:|
| Original 11 single-batch cases | 496 per case | 496 | 0 |
| Four two-batch/two-subset cases | 520 per case | 520 | 0 |

For ON, the initial failed draw and the repeated failed upload must leave the
cleared target untouched. Strength-zero fallback must match the geometric
reference. Successful retry and the draw after twenty warm draws must match the
mapped reference, while retaining the existing resource-reuse assertions. For
OFF, the draw with injection armed must match the geometric reference without
reaching a mapped creation call. Comparisons cover all **4,096 RGBA pixels**
exactly, with no tolerance, between draws on the same device. This does not claim
bit-identical results across different GPUs/backends.

The runner now requires all 15 `NORMAL MAP RECOVERY PIXELS ... PASS` markers in
addition to the resource-case markers and aggregate PASS. Older resource-only
binaries cannot satisfy the updated runner. Reproduce using fresh directories:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 2 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-pixels-on-2
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 0 -Lights 2 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-pixels-off-2
```

Final local reports are
`build/normal-windows/pixels-dx11-{on,off}-2/results-verified/report.json`.
`recovery.log` records visibility/contrast counts and native validation markers;
`fixtures/pixels-*-{reference,recovered}.ppm` provides 15 RGB image pairs per
build. The in-process comparison includes alpha; the PPM artifacts contain RGB.
Final executable/DLL hashes are in `binary-hashes-pixels.json` and the evidence
index is `build/normal-windows/dx11-recovery-pixels-summary.json`. Initial reports
and binary manifests are preserved. Toolchain remains VS 2026/MSVC v145, Windows
SDK 10.0.28000.0 and Python 3.14.5.

The following milestone extends these pixel comparisons to caps **1, 3 and 4**,
ON/OFF. The earlier full matrices establish resource coverage only. Compiler/Map
failures, actual device loss, DX9/Metal injection and Release profiling remain
separate work.

## Completed Windows milestone: full DX11 recovery pixel matrix (2026-09-30)

Recovery pixel comparison now passes every **DX11 Debug x86 ON/OFF x light cap
1/2/3/4** entry. Six fresh isolated builds at caps 1, 3 and 4 passed **90/90 pixel
cases and 66/66 integrated steps**. With the verified prior cap-2 runs, the full
matrix passes **120/120 pixel cases and 88/88 integrated steps**:

| Light cap | ON pixel cases | OFF pixel cases | Integrated steps | Evidence |
|---|---:|---:|---:|---|
| 1 | 15/15 | 15/15 | 22/22 | New builds and runs |
| 2 | 15/15 | 15/15 | 22/22 | Prior final runs; reports, sources and binaries verified |
| 3 | 15/15 | 15/15 | 22/22 | New builds and runs |
| 4 | 15/15 | 15/15 | 22/22 | New builds and runs |

Every entry requires all 15 resource and pixel case markers, the aggregate PASS,
zero exit status and no recognized failure/driver diagnostic. The in-process
comparison covers all 4,096 RGBA pixels. All **120 reference/recovered RGB PPM
pairs** also have matching SHA256 hashes. Across every cap, ON retains 496
visible/changed pixels in each single-batch case and 520 in each two-batch/subset
case; OFF retains the same visibility with zero mapped/geometric changes.
These contrast counts compare geometric and mapped references, not recovery
errors: recovered images match their references exactly.

The integrated runs include preparation/persistence, Lua build configuration,
native lazy resources, numerical skeletal parity, runtime/readback and visual
IB/VB regression. Debug-layer and post-teardown lifecycle success markers were
verified for resources, skeletal parity and recovery in all eight entries.
No engine or test-code changes were required; engine version remains 7.328.

New runs use revision `6346451bd553b4118339b685a8468e4ea025de8e`, VS 2026/MSVC
v145, Windows SDK 10.0.28000.0 and Python 3.14.5. The cap-2 test/proxy/runner
source hashes and executable/DLL hashes match the previous milestone exactly.
Its 30 pixel cases and 22 steps were not rerun. All eight binary manifests were
checked during consolidation; older artifacts remain unchanged.

Reproduce each new entry by varying `-Normal` (0/1) and `-Lights` (1/3/4), always
using a fresh output directory:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 4 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-pixels-on-4
```

New evidence is at `build/normal-windows/pixels-dx11-{on,off}-{1,3,4}/`:
`results/report.json`, `results/recovery.log`, PPM pairs under `results/fixtures/`,
remaining logs/PNGs, `build.json`, `build.log` and `binary-hashes.json`. The combined
index is `build/normal-windows/dx11-recovery-pixels-matrix-summary.json`; it links
all eight reports/manifests and records each case's visibility, contrast and
matching PPM hash, distinguishing new runs from prior cap-2 evidence.

This completes pixel acceptance for the existing static COM creation-failure
suite on this Windows device, not cross-GPU bit equality or device-loss handling.
DX11 constant-buffer `Map` failure and retry follows below.
Compiler failures, failures after more than two batches, DX9/Metal injection and
Release profiling remain separate work.

## Completed Windows milestone: DX11 Map failure and retry (2026-09-30)

Fresh **DX11 Debug x86 ON/OFF builds at light cap 2** pass the expanded **20-case
recovery suite: 40/40 recovery/pixel cases and 22/22 integrated steps**. Native
debug-layer and post-teardown resource-lifecycle validation passed. The five
runner unit tests also pass. No production changes were needed; engine version
remains 7.328.

A scoped, test-only `ID3D11DeviceContext` forwarding proxy injects
`E_OUTOFMEMORY` before the selected native constant-buffer `Map` call. The
reserved static pipeline distinguishes matrix (128 bytes), normal settings
(16 bytes) and the larger lighting buffer. Staging readback maps are forwarded.
The wrapper restores the engine context pointer before leaving the case and
checks that every successful map has exactly one unmap, with none outstanding.

| New case | ON | OFF |
|---|---|---|
| Matrix buffer | Two injected failures; empty failed draws | Same |
| First-subset lighting | Two injected failures; empty failed draws | Same |
| First-subset normal settings | Two injected failures; empty failed draws | No normal-settings map; complete geometric image |
| Second-subset lighting | Two injected failures; only first subset visible | Same |
| Second-subset normal settings | Two injected failures; only first subset visible | No normal-settings map; complete geometric image |

Unlike creation-failure rollback, these failures occur after a complete derived
upload. Tests verify that uploaded buffers remain published with unchanged native
identities, repeated failures create no resources, and strength-zero fallback,
retry and 20 warm draws reuse the retained allocation. The second-subset fixture
has disjoint image halves, so failed-draw readback checks the first subset's exact
pixels and an empty second half. This captures partial rendering rather than
claiming transactional draw rollback.

Fallback matches an independent geometric reference; retry and the final warm
draw match the independent mapped reference in all 4,096 RGBA pixels. ON has
496 visible/changed pixels for each single-subset case and 520 for each
two-subset case; OFF has the same visibility and zero mapped/geometric contrast.
All **40 reference/recovered RGB PPM pairs** also have matching SHA256 hashes.
The original 15 creation-failure cases were rerun in both builds.

`--dx11-failure` / `-Dx11Failure` now require 20 resource markers, 20 pixel
markers, five `Map` markers with the expected injection counts, and the 20-case
aggregate. Rebuild `libTest`; an older binary cannot satisfy the updated runner.

Reproduce both configurations with fresh output directories, varying `-Normal`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File platform-msvs/run-normal-map-tests.ps1 -Backend dx11 -Normal 1 -Lights 2 -Dx11Failure -SkeletalParity -Output build/normal-windows/recheck-map-on-2
```

Evidence: `build/normal-windows/map-dx11-{on,off}-2/` contains build metadata/logs,
verified `binary-hashes.json`, `results/report.json`, `results/recovery.log`,
PPM pairs under `results/fixtures/`, and the remaining integrated logs/PNGs.
`build/normal-windows/dx11-map-summary.json` links both reports/manifests and
records source hashes, pixel hashes and per-case injection counts. Builds use
base revision `dfa5260f7b13be147aab954dd70e0e7350b2286b` plus this test/runner
change, VS 2026/MSVC v145, Windows SDK 10.0.28000.0 and Python 3.14.5.

This proves deterministic constant-buffer failure/retry on this Windows device;
it does not simulate real memory exhaustion, device removal, compiler failure,
skeletal buffer failures or cross-GPU image equality. Extending the **20-case
suite to caps 1/3/4, ON/OFF** is deferred by the
[scope decision](#current-scope-and-next-step-2026-09-30); native macOS Metal
evaluation is next. Earlier full matrices establish 15-case creation-failure
coverage only. Broader fault injection and actual device loss remain in the
robustness backlog; Release profiling remains separate planned work.

## Completed macOS milestone: Metal baseline and captures (2026-09-30)

All eight **Metal Debug arm64 ON/OFF x light cap 1/2/3/4** entries pass the
existing nine-step baseline: **72/72 integrated steps**. Every graphical step
(build configuration, resources, runtime, readback and both visual suites) logs
`Metal API Validation Enabled`, with no recognized validation failure. The five
Python runner unit tests pass. This is native execution on an Apple M4, not a
host-only shader-generator check.

| Light cap | ON steps | OFF steps | Native pipeline checks | GPU capture |
|---|---:|---:|---|---|
| 1 | 9/9 | 9/9 | Passed both | Not requested |
| 2 | 9/9 | 9/9 | Passed both | ON and OFF |
| 3 | 9/9 | 9/9 | Passed both | Not requested |
| 4 | 9/9 | 9/9 | Passed both | Not requested |

Host: MacBook Air / Apple M4 (10 CPU cores, 10 GPU cores, 16 GB), macOS 26.6.2
(25G83), Xcode 26.6 (17F113), macOS SDK 26.5, CMake 4.2.0 and Python 3.9.6.
Base revision is `acaf93acc522d1e65ac97638ef95bc7574fe7f1e` plus the CMake/test/runner
changes in this milestone. Engine version remains 7.328; no production rendering
code, public API or ownership boundary changed.

The first clean configuration exposed a build defect: the native resource test
requested `LANGUAGE OBJCXX` without enabling that language. The root CMake file
now enables Objective-C++ for Apple Metal, requests C++17 and preserves the
optimization/debug flags previously used to compile `.mm` files through the CXX
rule. A sandboxed graphical attempt returned a null Metal device; native runs
were repeated with GPU/desktop access. Initial failed evidence is retained and
is excluded from the matrix totals.

The Metal resource test now forwards encoder calls through a test-only observer
that records the actual `setRenderPipelineState` argument. The existing shader
identity assertions therefore also cover Metal: geometric rendering without a
map or at zero strength, distinct mapped selection only in ON builds, map
removal/reassignment, twenty repeated draws, sharing across shader instances,
restore, an asset without prepared basis and dynamic invalidation. Calls still
reach the native validation encoder; no engine PSO state was made public.
ON reports **0 -> 150 bytes** of derived GPU payload on first effective use,
with unchanged buffer identities on warm draws. This excludes driver/allocator
overhead and is not a total-memory or performance measurement.

Optional `MBM_NORMAL_MAP_METAL_CAPTURE=/absolute/path/new.gputrace`, together
with `MTL_CAPTURE_ENABLED=1`, records this production submission through
`MTLCaptureManager`. Capture errors fail the test; RAII stops the capture after
GPU completion, including early-return cleanup. The cap-2 captures each contain
one submitted frame. Their captured MSL sources show:

- ON has geometric and mapped variants. Only mapped MSL has tangent buffer 20,
  normal-settings buffer 21 and `mbmMappedNormal`.
- OFF has only the geometric variant. Both builds retain the existing 2dw
  texture-normal path; absence of the 3D interface does not remove that feature.
- Both lighting loops have the literal cap 2 in every captured source.
- **The ON/OFF geometric sources are not identical.** ON includes the existing
  inverse-transpose normal transform; OFF retains the earlier direct transform.
  This is the sole textual difference between these captured geometric sources.
  The baseline checks each build's fallback behavior, not arbitrary ON/OFF pixel
  equality under nonuniform scaling. That cross-build transform difference is
  recorded, not changed or treated as a proven equivalence by this milestone.

The capture inspection extracts complete MSL source streams from this Xcode
trace and hashes the trace files and sources. It does not disassemble GPU machine
instructions or measure register pressure. Pipeline identity evidence comes from
the native encoder observer, not an inference from shader source names.

Reproduce each entry using a separate build tree, varying `ON`/`OFF` and cap:

```sh
cmake -S . -B build/normal-metal/recheck-on-2 -DPLAT=MacOs -DUSE_METAL=1 \
  -DUSE_LUA=1 -DUSE_TEXTURE_MISSING_DIALOG=0 -DCMAKE_BUILD_TYPE=Debug \
  -DUSE_NORMAL_MAPPING_3D=ON -DSUPPORTED_MAX_LIGHTS=2
cmake --build build/normal-metal/recheck-on-2 --target testLib mini-mbm -j 8
MTL_CAPTURE_ENABLED=1 MBM_NORMAL_MAP_METAL_CAPTURE="$PWD/build/normal-metal/recheck-on-2/native.gputrace" \
  python3 src/test-lib/run-normal-map-tests.py \
  --test-lib "$PWD/bin/debug/arm64/testLib" --engine "$PWD/bin/debug/arm64/mini-mbm" \
  --backend metal --normal 1 --lights 2 --require-native-validation \
  --output "$PWD/build/normal-metal/recheck-on-2/results"
```

Snapshot `testLib`, `mini-mbm` and their dylibs before building the next entry:
CMake build trees share output locations. Both capture and result paths must be
new. GPU access and a graphical session are required.

Local evidence: `build/normal-metal/{on,off}-{1,2,3,4}/` contains `binaries/`,
`binary-hashes.json`, `build.json`, configure/build logs and `results-final/`
(reports, suite logs, fixtures and IB/VB PNGs). The index is
`build/normal-metal/matrix-summary.json`; `host.json` and `source-hashes.json`
record provenance. Cap-2 entries also contain `acceptance.gputrace` and extracted
`captured-shaders/`; `capture-inspection.json` records their hashes and findings.
The local `run-matrix.py` and `inspect-captures.py` retain the orchestration and
capture extraction used for this evidence. Artifacts are local, not committed.

This closes the bounded Metal functional baseline on this M4. macOS GLES,
iOS, native skeletal parity, actual device loss/fault injection and cross-device
pixel equality are not covered. The known ON/OFF transform difference above
limits cross-build parity claims. Release CPU/GPU profiling and native instruction
statistics remain separate planned work; no FPS improvement is claimed.

## Completed macOS milestone: Release measurement baseline (2026-09-30)

The benchmark now supports native macOS Metal through the existing `testLib`
entry points and Python runner. Fixture generation was extracted unchanged from
the GLES benchmark into `normal-map-benchmark-fixtures.cpp`; Linux/GLES keeps its
existing draw/timing implementation. No production rendering behavior or public
API changed; engine version remains 7.328.

### Method and measurement boundaries

The study uses eight isolated Release arm64 ON/OFF x cap 1/2/3/4 builds. Each cap
compares both switches across six cases (`plain`, `retained`, `zero`, `mapped`,
`mixed`, `removed`) and grids 32/128 (6,144/98,304 vertices). Five independent
processes per combination give 120 timed samples per cap. Each process performs
one first-use draw, 16 warmup draws and eight blocks of 32 draws into a 128x128
offscreen target. No presentation, vsync or pixel readback is inside timed blocks.
Visibility is checked before and after warm draws; the regression suite supplies
stronger image comparisons. Geometry is opaque, identity-transformed, with depth
and culling disabled. These are repeated draws of one synthetic asset, not frames
of a game.

All caps use the **same one selected point light plus a directional light**.
A spatial owner supplies the plane's bounds to production per-object light
selection, and the benchmark asserts the selected count before measuring. A
null owner would silently select zero lights; that cannot satisfy this benchmark.
Changing the compiled cap therefore does not change the active-light workload.
This is not a saturated 1/2/3/4-light cost comparison. Each cap's ON/OFF job order
is shuffled with seed 2026. Caps run sequentially (2, 1, 3, 4), so cross-cap
comparisons are also subject to time/thermal/scheduling drift.

The runner separates `--validate-metal` correctness runs from measurements.
Timed processes set `MTL_DEBUG_LAYER=0`, `MTL_SHADER_VALIDATION=0` and
`MTL_CAPTURE_ENABLED=0`, remove preload/injection settings and require the Release
marker. Validation runs require `Metal API Validation Enabled`; timed runs reject
that activation marker. Every sample must contain complete, finite, positive GPU
timestamps and all warm blocks, matching build/workload identity, visible output
and the expected deferred-allocation contract. Malformed measurements are rejected
before summarization. OS/driver Metal shader caching is uncontrolled; each engine
process is fresh, but these are not cold-driver compilation or cold-disk results.

Metrics retain the existing JSON names where applicable:

- `load_sync_us` and `material_sync_us`: synchronous host calls for asset load and
  material setup. Metal creates shared source buffers and writes texture data
  synchronously here; these timings do not include a separate GPU command buffer.
- `geometric_compile_sync_us`: the explicit geometric shader compile/cache call,
  including creation of its blend variants when uncached. This does not count
  compiler invocations or distinguish driver cache hits.
- `first_submit_us`, `first_sync_us`: CPU wall time for pass creation, draw,
  encoder end and commit, before/after `waitUntilCompleted`. First use can include
  lazy mapped shader compilation and derived upload. `removal_us` is host time
  spent removing map assignments before warming the `removed` case.
- `first_gpu_us`, `warm_gpu_us`: completed command buffer `GPUEndTime - GPUStartTime`,
  divided by draw count. The span includes clear/store/pass overhead and GPU
  scheduling within that command buffer, not just isolated vertex/fragment work.
- `warm_submit_us`, `warm_sync_us`: the same CPU wall intervals over a warm block,
  divided by 32. They exclude scene traversal, gameplay and presentation, and
  must not be converted into application FPS.
- `rss_*`: task resident bytes from `mach_task_basic_info`; `metal_allocated_*`:
  `MTLDevice.currentAllocatedSize`; `source_gpu_bytes` / `derived_*`: unique
  `MTLBuffer.length` payload totals. RSS and Metal allocations overlap on unified
  memory and must not be added. Neither isolates CPU preparation allocations;
  buffer lengths exclude allocator padding, textures, pipelines and driver state.

Summaries use the median of each process's warm blocks, then median, nearest-rank
p95 and min/max across five processes. With five samples, p95 is the maximum.
There are no timing acceptance thresholds on this shared desktop. ON `mapped`
and OFF `mapped` perform different shading work; this compares current build
switches, not a historical before/after implementation of the optimization.

### Recorded Metal result

All **480 timed samples passed**, alongside **288 separate benchmark validation
samples**, **72/72 integrated Release regression steps** and ten Python unit
tests (five benchmark, five regression runner). The timed samples run without
API validation/capture; the other results establish correctness with API
validation active. Binary/source hashes and fixture equality across caps were
checked during consolidation.

Host: Apple M4, macOS 26.6.2 (25G83), **Xcode 27.0 (27A266a), SDK 27.0**, CMake
4.2.0, Python 3.9.6. This toolchain differs from the earlier Debug acceptance;
that is why Release regressions were rerun. All eight builds use `-O3`,
`USE_LUA=1`, Metal, AVFoundation audio and the missing-texture dialog disabled.
Source baseline: `31d7f08e` plus this benchmark/test/runner change. Version 7.328.

For the larger grid at cap 2, medians across five processes (us/draw):

| Case | OFF CPU submit | ON CPU submit | OFF GPU span | ON GPU span | ON derived MiB after first use |
|---|---:|---:|---:|---:|---:|
| plain | 2.33 | 2.21 | 59.94 | 82.02 | 0 |
| retained | 2.56 | 2.42 | 95.80 | 91.39 | 0 |
| zero | 2.52 | 2.35 | 63.30 | 84.10 | 0 |
| mapped | 2.37 | 2.57 | 84.63 | 101.74 | 4.6875 |
| mixed | 2.37 | 2.87 | 152.12 | 108.14 | 4.6875 |
| removed | 2.50 | 2.67 | 66.70 | 106.97 | 4.6875 |

GPU dispersion is material: cap-2 `retained` process block-medians range from
64.70 to 220.19 us OFF and 64.79 to 128.82 us ON. Even OFF cases with equivalent
geometric work vary substantially. Do not read the table as a stable speedup or
slowdown percentage. CPU submissions are much closer, with overlapping ranges;
this shared-desktop study does not resolve small timing improvements. The full
four-cap distributions are preserved in the report, without claiming a monotonic
performance benefit from lowering the compiled light cap.

The allocation results are deterministic across every cap and sample:

- Original buffer payload is 196,608 bytes (grid 32) or 3,145,728 bytes (grid 128).
- Derived payload starts at zero in every case. All OFF cases, and ON `plain`,
  `retained`, `zero`, remain at zero through the last warm block.
- ON `mapped`, `mixed`, `removed` allocate 307,200 or 4,915,200 derived bytes on
  first use and retain them through warm draws. Mapping only one subset still
  uploads both; removing maps does not evict them. These observations identify
  per-subset upload/eviction as a possible future optimization, not a change made
  by this study.

At cap 2, `libcore_mbm.dylib` is 3,141,504 bytes ON versus 3,121,776 bytes OFF:
19,728 fewer file bytes in OFF. This is library file size, not loaded/resident
memory. For `retained` at the larger grid, median load RSS growth is 16.45 MiB ON
versus 16.56 MiB OFF; `currentAllocatedSize` progresses from about 0.61 to 3.73
MiB after loading, then 4.19 MiB after drawing in both builds. These overlapping
observations do not prove a total-memory reduction or isolate CPU staging.
Earlier Linux memory/timing numbers must not be transferred to this Metal device.

Reproduce using matching Release snapshots (build flags as in the Metal baseline,
with `CMAKE_BUILD_TYPE=Release`). Run correctness separately, then measurements:

```sh
python3 src/test-lib/run-normal-map-benchmark.py \
  --backend metal --enabled /absolute/on-2/testLib --disabled /absolute/off-2/testLib \
  --lights 2 --point-lights 1 --grids 32 128 --draws 32 --blocks 8 \
  --validate-metal --repeats 3 --output /tmp/metal-benchmark-validation-2
python3 src/test-lib/run-normal-map-benchmark.py \
  --backend metal --enabled /absolute/on-2/testLib --disabled /absolute/off-2/testLib \
  --lights 2 --point-lights 1 --grids 32 128 --draws 32 --blocks 8 \
  --repeats 5 --output /tmp/metal-benchmark-measurements-2
```

Each snapshot needs `testLib` and its matching `libcore_mbm.dylib`. Use new output
directories and repeat with matching cap-1/3/4 binaries. The backend defaults to
`gles` for existing Linux commands; nonzero `--point-lights` and `--validate-metal`
are Metal-only. Five focused benchmark-reader unit checks are available through
`python3 src/test-lib/test-normal-map-benchmark.py`.

Local evidence is under `build/normal-metal-release/`: per-entry binaries,
SHA-256 manifests, build metadata/logs and `regression-final/`; per-cap
`validation-N/` and `measurements-N/` contain raw logs, fixture hashes, per-process
samples and distribution summaries. `study-summary.json` joins all evidence;
`timings.csv` exposes the distributions for further analysis. `host.json`,
`source-hashes.json`, `run-study.py` and `summarize-study.py` retain provenance and
orchestration. These artifacts are local, not committed. Shared Release outputs
were restored to ON / cap 4; Debug outputs were not rebuilt.

This completes the bounded synthetic Metal measurement baseline. GPU machine
instruction/register counts, isolated CPU allocation attribution, representative
game assets, skeletal/dynamic workloads and historical before/after profiling
remain outside this result. The earlier ON/OFF nonuniform-normal-transform
limitation also remains; this benchmark uses identity transforms. No FPS or
combined-memory improvement is claimed.
