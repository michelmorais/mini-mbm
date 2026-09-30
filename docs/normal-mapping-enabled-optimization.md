# Normal mapping in enabled builds: geometric variants and deferred upload

Initial delivery: 7.327. Shared CPU preparation: 7.328. This extends the completed build-switch implementation; it does
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
The DX9 SM3 and DX11 Debug baseline matrices are now validated below; Metal
acceptance and native performance measurements remain pending.
Linux measurements do not establish native-backend or mobile-device coverage.

## Next milestone: native backend regression matrix

Status: the complete DX9 SM3 and DX11 Debug x86 baseline matrices are validated
with 7.328 plus the DX9 compilation fixes below. macOS acceptance remains
**pending**. SM2 profile characterization is complete below: default lighting
exceeds the profile, while unlit rendering passes. DX11 single-batch COM creation
failure/retry is also validated across all eight Debug entries (details below).
Device-loss and performance acceptance remain pending.

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
- [ ] Compile and run the same eight entries on macOS Metal. Run macOS GLES
  separately if that backend is shipped.
- [x] Run the DX11 native resource test with the Windows Graphics Tools debug
  layer, including post-teardown live-object checks, in all eight entries.
- [ ] Run Metal with API validation enabled.
- [ ] Inspect native shader inputs/instructions and Metal pipeline identity using
  GPU captures. The Metal resource test does not introspect bound pipeline state.
- [x] Exercise controlled full EGL context destruction/recreation through the
  Linux/GLES production restore state machine, with pending and uploaded assets,
  shared instances, shader variants and pixel comparisons (details below).
- [ ] Validate abrupt driver resets/`EGL_CONTEXT_LOST`, in-flight async loads and
  native Windows/macOS loss handling. Controlled recreation does not prove those.
- [x] Add deterministic Linux/GLES failed-compile/link/program-creation and
  failed-upload retry coverage, including Debug and Release (details below).
- [x] Validate DX11 single-batch COM creation failure/retry in all eight Debug
  ON/OFF x 1..4-light entries, with debug-layer and post-teardown lifecycle checks.
- [x] Validate DX11 rollback/retry in a second batch of one subset and across
  two subsets, Debug ON/OFF at cap 2 (15-case suite, details below).
- [x] Extend the 15-case DX11 suite to light caps 1/3/4: all eight Debug ON/OFF
  entries pass, including native validation (full matrix below).
- [ ] Compare pixels after DX11 recovery. Extend failure injection to DX9/Metal,
  compiler/Map failures and actual device-loss scenarios.
- [ ] Record native measurements and evidence before claiming performance or
  native parity. No FPS or total-memory improvement is assumed.

### Prepared coverage

| Check | GLES | DX9 / DX11 | Metal |
|---|---|---|---|
| No derived allocation at load, no map, strength zero, 2dw | Native inspection | Native inspection | Native inspection |
| First effective use creates GPU buffers | Buffer queries | COM buffer descriptors | MTLBuffer lengths |
| Reassignment/repeated draws reuse resources | Buffer/program identity | Buffer/vertex-shader identity | Buffer identity |
| Geometric/mapped shader selection | Program and tangent input | Bound vertex shader | Visual comparisons; pipeline capture pending |
| Shared program cache | Geometric/mapped cache identity | DX9 identity; DX11 has no shared default-program cache | Capture pending |
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
GPU completion. It rejects recognized validation errors, but a clean log is not
proof that the layer activated: retain Xcode/Metal validation evidence as part of
the milestone. DX11 validation markers currently apply to the C++ resource test;
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

The scope remains the resource contracts described above, not pixel equality
during recovery or actual device loss. The next Windows milestone is **pixel
comparison after DX11 recovery**. Compiler/Map failures, failures after more
than two batches, DX9/Metal injection and Release profiling remain separate work.
