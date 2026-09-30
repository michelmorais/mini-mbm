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

DirectX 9/11 and Metal follow the same private selection/staging contract but
require native compilation, driver validation and performance measurements.
Linux measurements do not establish native-backend or mobile-device coverage.

## Next milestone: native backend regression matrix

Status: test infrastructure prepared; native Windows/macOS compilation and execution
are **pending**. This milestone validates 7.327 and must not be inferred complete
from the earlier build-switch matrix.

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
- [ ] Compile and run all eight `USE_NORMAL_MAPPING_3D=0/1` x
  `SUPPORTED_MAX_LIGHTS=1/2/3/4` entries on Windows DX9 SM3, Windows DX11 and
  macOS Metal. Run macOS GLES separately if that backend is shipped.
- [ ] Run DX11 Debug with the Windows Graphics Tools debug layer, including
  post-teardown live-object checks; run Metal with API validation enabled.
- [ ] Inspect native shader inputs/instructions and Metal pipeline identity using
  GPU captures. The Metal resource test does not introspect bound pipeline state.
- [ ] Add and execute full device/context loss and recreation tests, including
  pending staging and already-uploaded assets. Shader `onRestore()` alone is not
  proof of device-loss recovery.
- [ ] Add deterministic failed-upload/failed-compile retry coverage. Current
  suites do not inject driver allocation/compilation failures.
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
configuration, resources, async runtime/GC, readback and visual IB/VB. Editor
(ImGui), skeletal parity and full device-loss tests are separate follow-ups, not
implicitly covered by a baseline PASS.

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

The next Linux milestone is deterministic failed-compile/failed-upload retry
coverage; full context loss/recreation and reproducible performance measurements
remain separate pending work. Windows/macOS native acceptance remains pending.
