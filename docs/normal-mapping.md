# Static 3D normal mapping

Static 3D normal mapping supports OpenGL ES, DirectX 9 SM3, DirectX 11 and Metal.
It combines a build-time switch, lighting shaders specialized for a fixed maximum
point-light count, geometric/mapped shader variants and deferred GPU upload.
Desktop validation covers Linux/GLES, Windows DX9 SM3/DX11 and macOS Metal within
the [validation scope](#validation-scope) below.

## Build configuration

| Setting | Default | Contract |
|---|---|---|
| CMake `USE_NORMAL_MAPPING_3D` | `ON` | Emits numeric `USE_NORMAL_MAPPING_3D=0/1`; accepts `OFF` or `0` to disable |
| CMake `SUPPORTED_MAX_LIGHTS` | `4` | Integer 1..4; specializes generated and reserved lighting shaders |
| MSBuild `MbmUseNormalMapping3D` | `1` | Same numeric rendering switch |
| MSBuild `MbmSupportedMaxLights` | `4` | Same compiled point-light limit |

`include/core_mbm/render-features.h` defaults the rendering switch to `1` when
not supplied and rejects values other than `0/1`. Guards test its value, not
merely whether the macro exists. Build all participating targets with consistent
settings. CMake and Visual Studio propagate the definitions to their targets.

`mbm::isNormalMapping3DCompiled()` and Lua `mbm.isNormalMapping3DCompiled()` report
the linked engine's compiled capability. This is read-only and distinct from
whether an asset has tangents, a normal texture or an active mapped draw.

The light cap limits selected point lights per draw; the independent 3D
directional light is not counted against it. Generated arrays and loops use the
compiled cap. Runtime light selection/request APIs remain available but do not
recompile shaders for each requested count. Arbitrary custom shader source is
not rewritten. See [Lighting](light.md) for light selection and material semantics.

Example Linux build:

```sh
cmake -S . -B build/normal-check -DPLAT=Linux -DCMAKE_BUILD_TYPE=Release \
  -DUSE_LUA=1 -DAUDIO=none -DUSE_TEXTURE_MISSING_DIALOG=0 \
  -DUSE_NORMAL_MAPPING_3D=ON -DSUPPORTED_MAX_LIGHTS=2
cmake --build build/normal-check --target testLib mini-mbm -j8
```

For Metal use `-DPLAT=MacOs -DUSE_METAL=1` and the platform's audio configuration.
For Visual Studio use `/p:MbmUseNormalMapping3D=0` or `1` and
`/p:MbmSupportedMaxLights=1` through `4`; see the [Windows build guide](../platform-msvs/README.md).
Build directories share binary/library output locations. Snapshot matching
executables and libraries before building another configuration, and ensure
outputs are relinked for the selected settings.

## Disabled rendering contract

With `USE_NORMAL_MAPPING_3D=0`:

- Default static 3D lighting uses geometric normals even when a normal texture
  is assigned. Generated shaders omit the feature's tangent interface, mapping
  helpers, settings and 3D sampling path.
- Derived tangent geometry, auxiliary mapping layouts/declarations and fallback
  tangent resources are not created. Feature-specific bindings and draw paths
  are compiled out.
- Automatic preparation needed exclusively by 3D rendering is skipped during
  runtime load/reload. Explicit CPU preparation/import remains available.
- Asset loading still validates and preserves tangent/material sections 14/15
  and material references. Invalid data still fails validation. Authoring and
  persistence support, including MikkTSpace, remains available.
- Existing 2dw normal mapping and explicit custom-shader texture semantics are
  preserved. Texture acquisition remains shared because a runtime asset can be
  used in those paths; the normal texture slot is not globally disabled.

The switch does not remove every normal-map-related CPU operation, persisted
byte or texture allocation. Mesh Debug reports the compiled rendering capability
separately from stored asset data. Its normal-map removal operation removes
prepared tangents, normal texture references and normal-map properties; save the
edited asset to persist that change. Other material roles remain independent.

## Enabled rendering and ownership

Default/reserved static lighting starts with a geometric shader. A supported 3D
draw selects a lazy mapped variant only for a subset with a normal texture,
retained tangent basis and nonzero strength. Mixed subsets use their respective
variants. No-map draws use cached activity and perform no geometry scan or
normal-map upload. Shader caches distinguish geometric and mapped variants;
DX11 does not have a shared default-program cache.

Prepared static frames retain source positions/normals/UVs in private CPU staging.
The staging aliases immutable preparation owned by `MESH_MBM::Impl`, sharing
batches instead of copying them. Assets without a prepared basis have no such
staging. The first effective mapped draw uploads **all prepared batches of the
frame**, including any prepared but inactive subsets. Subsets without prepared
batches are not included.

Upload is transactional: failure leaves staging available for retry and does
not publish partially created derived buffers. A failed lazy shader compile or
upload fails that draw; it does not silently substitute geometric lighting.
On success, staging geometry and its shared preparation reference are released.
The asset retains preparation for readback; extraction to `MESH_MBM_DEBUG`
produces an independent mutable copy. Public headers expose neither these
containers nor backend handles. See [private ownership](core-pimpl-status.md#normal-mapping-preparation-asset-data-and-rendering).

After upload, GPU batches remain cached through normal texture removal,
reassignment and strength changes. Removal or zero strength selects geometric
shading. Buffer release or dynamic source invalidation discards derived resources
and staging. Assigning a map without a retained basis uses geometric fallback;
this path does not automatically regenerate tangents after edits. Material changes
retain the engine's shared-asset semantics.

Frame-wide upload and buffer retention are the supported policy. Per-subset
upload/eviction is not a delivery requirement: when all prepared subsets use a
map, it would send the same data while adding partial-upload and retention states.
Reconsider it only with representative assets demonstrating material unused
prepared geometry and a relevant resource cost.

## Backend limits

- **DirectX 9:** static mapping requires `vs_3_0` and `ps_3_0`. Unsupported profiles
  select the geometric-lighting path and emit a diagnostic. Strict SM2 default
  lighting exceeds instruction limits at caps 1/2 and temporary-register limits
  at caps 3/4; that shader fails compilation. There is no automatic unlit fallback.
  The separate unlit shader works with SM2 bytecode and no derived mapping
  resources. Profile characterization does not establish real SM2 hardware support.
- **Metal:** enabled-build geometric shaders use an inverse-transpose normal
  transform; disabled builds use a direct transform. Arbitrary ON/OFF pixel
  equivalence under nonuniform scale is not established. The synthetic benchmark
  uses identity transforms and does not exercise this difference.
- **Skeletal/dynamic geometry:** static tangent preparation and numerical skinning
  parity do not establish skeletal normal-map material/render integration.
  Dynamic edits invalidate derived mapping resources. Custom vertex shaders and
  unsupported draw paths retain their geometric fallback contracts.
- **Memory and latency:** unused prepared frames retain CPU staging; first mapped
  use can compile a shader and upload geometry. Mixed materials can add shader
  switches. Deferred upload does not guarantee lower combined CPU/GPU memory,
  zero overhead or higher FPS.

Related contracts: [material texture slots](light.md#material-texture-slots),
[Lua material settings](lua-api.md#normal-map-material-settings),
[mesh tangent section](mesh-v11-format.md#optional-section_normal_map_tangents-14-section-version-1).

## Validation scope

| Backend / target | Validated configuration | Coverage |
|---|---|---|
| Linux / Mesa GLES | Release ON/OFF x caps 1..4; shared-preparation, retry and context checks at cap 2, with focused Debug checks | Baseline suites, resource/program inspection, controlled EGL recreation and fault/retry checks |
| Windows / DX9 SM3 | Debug x86 ON/OFF x caps 1..4 | Baseline suites, native resource/shader identity, SM2 characterization and LBS/DQS numerical parity |
| Windows / DX11 | Debug x86 ON/OFF x caps 1..4 | Baseline suites, debug-layer/post-teardown checks, LBS/DQS numerical parity, 15 creation-failure/recovery pixel cases; expanded 20-case suite including `Map` at cap 2 |
| macOS / Metal | Apple M4 arm64, Debug and Release ON/OFF x caps 1..4 | Baseline suites with API validation, encoder pipeline identity/cache checks, ON/OFF cap-2 captures and a separate Release measurement suite |

Baseline suites cover build identity, preparation/persistence, Lua capability,
no-map/zero-strength/2dw allocation behavior, first use, late assignment/removal,
resource reuse, no-basis fallback, shader restore, dynamic invalidation, async
loading/GC, readback and indexed/non-indexed visual scenes. Visual checks include
mixed subsets, strength/sign, transforms and custom-shader fallback.

GLES fault injection covers shader/program creation and upload failure/retry.
Controlled EGL recreation covers pending/uploaded assets, shared instances and
pixel comparisons through the production restore state machine. DX11 recovery
covers single-batch and second-batch/second-subset rollback, retained resources,
retry and fault-free versus recovered pixels; `Map` scenarios include matrices,
lighting and normal settings. These are not actual GPU reset/device-loss tests.

The accepted desktop scope does not establish Android/iOS, macOS GLES, all GPU
models, Windows Release performance, arbitrary cross-build pixel equality,
broader compiler/resource fault coverage or in-flight async device-loss recovery.
Numerical skeletal parity is distinct from animated normal mapping.

## Run regression tests

Build matching `testLib` (Visual Studio project `libTest`) and Lua-enabled
`mini-mbm`, with the missing-texture dialog disabled. Use a graphical session
and a new output directory. The runner verifies identity, required success
markers, exit codes, timeouts and recognized failure/driver diagnostics, and
writes JSON, logs, fixtures and visual images. It does not build or switch flags.
Expected malformed-asset diagnostics are allowed only by the relevant suite.

```sh
python3 src/test-lib/run-normal-map-tests.py \
  --test-lib /absolute/path/testLib --engine /absolute/path/mini-mbm \
  --backend gles --normal 1 --lights 2 --output /tmp/normal-gles-on-2
```

Use `--backend dx9`, `dx11` or `metal` for native targets. Add repeated
`--library-dir PATH` when matching DLLs/shared libraries live elsewhere.
Run all ON/OFF x cap 1..4 entries in isolated snapshots for a full baseline.

- DX11 Debug: add `--require-native-validation` for debug-layer and lifecycle
  markers; Release cannot satisfy that requirement. `--dx11-failure` adds the
  20-case resource/pixel suite. Its all-cap coverage must not be inferred from
  the cap-2 result. `--skeletal-parity` adds native DX9/DX11 deformation checks.
- Windows build wrapper: `platform-msvs/run-normal-map-tests.ps1 -Backend dx11
  -Normal 1 -Lights 2 -Configuration Debug -Output C:/mbm-results/dx11-on-2`;
  optional `-Dx11Failure` and `-SkeletalParity` forward those suites.
- Metal: add `--require-native-validation`. The runner enables the API layer and
  requires both `Metal API Validation Enabled` and `NORMAL MAP METAL PIPELINES PASS`.
  Set `MTL_CAPTURE_ENABLED=1` and `MBM_NORMAL_MAP_METAL_CAPTURE` to an absolute new
  `.gputrace` path for a native resource-test capture. Captured MSL is not GPU
  machine-code/register analysis.
- Linux/GLES: controlled context recreation is included. Configure
  `-DMBM_BUILD_GLES_FAULT_TESTS=ON` and pass `--gles-fault-library` pointing to
  `mbm-normal-map-faults.so` to add the test-only interposer suite.

DX11 validation markers apply to the C++ resource/recovery/parity tests;
Lua visual tests have no dedicated info-queue hook. Editor UI, abrupt loss and
native full device recreation are not implied by a baseline PASS.

## Reproducible measurements

`src/test-lib/run-normal-map-benchmark.py` compares matching ON/OFF Release
snapshots for GLES or Metal, with fresh processes and seeded randomized case
order. Cases are `plain`, `retained`, `zero`, `mapped`, `mixed` and `removed`.
Fixtures use two subsets and grids 32/128 (6,144/98,304 vertices). The first draw
is measured separately from 16 warmup draws and eight blocks of 32 warm draws.
The runner preserves raw samples, binary/fixture hashes and distribution summaries.

```sh
python3 src/test-lib/run-normal-map-benchmark.py \
  --backend gles --enabled /absolute/on/testLib --disabled /absolute/off/testLib \
  --lights 2 --grids 32 128 --repeats 5 --draws 32 --blocks 8 \
  --output /tmp/normal-benchmark
```

Each snapshot needs its matching `libcore_mbm.so` or `libcore_mbm.dylib` beside
`testLib`. For Metal use `--backend metal --point-lights 1`, and run correctness
separately with `--validate-metal --repeats 3` into another output directory.
Timed Metal runs require Release and disable validation/capture. Metal keeps one
selected point light plus a directional light for this command across caps;
GLES uses directional lighting. These are not saturated light-count comparisons.

Load/material setup, geometric compilation, first submission/completion and warm
submission/completion are distinct CPU wall intervals. GLES completion uses
`glFinish`; it provides no isolated GPU timer. Metal additionally records completed
command-buffer GPU spans, including pass overhead. Neither measures game FPS.

Memory metrics distinguish process RSS, unique buffer payload sizes and Metal
`currentAllocatedSize`. They exclude or combine different driver/allocator costs;
RSS and Metal allocations overlap on unified memory and must not be added.
For these fixtures, source buffers contain 196,608 or 3,145,728 bytes. Derived
payload starts at zero; ON `mapped`/`mixed`/`removed` retain 307,200 or 4,915,200
bytes after first use, while OFF and ON `plain`/`retained`/`zero` remain at zero.
These are payload sizes, not total physical GPU memory.

The runner disables Mesa's shader disk cache unless `--allow-mesa-disk-cache` is
set. OS file caches and Metal driver caches are not controlled. Summaries use
per-process warm-block medians, then median, nearest-rank p95 and range across
processes; with five samples p95 equals the maximum. No timing thresholds or
small percentage improvements are inferred from shared-desktop measurements.

Runner-only checks require no graphical execution:

```sh
python3 src/test-lib/test-normal-map-runner.py
python3 src/test-lib/test-normal-map-benchmark.py
```

## MeshDebug generation panel

**Generate normal map** is separate from material/tangent preparation.
Image Mesh entries reuse the project's source, residual compensation and generation
settings. Ordinary mesh entries require an explicit height image and frame/subset
selection; generation is additive and leaves geometry unchanged. Generated pixel
strength is independent of runtime material strength (set to 1 on application).
See [the generator manual](normal-map-editor.md#geração-no-meshdebug) for staging,
Undo, project persistence and temporary texture ownership.
