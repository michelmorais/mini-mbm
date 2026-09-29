# Build-time normal mapping and lighting specialization

Status: approved scope; implementation and verification pending.

The static 3D normal-mapping feature has already shipped. Its remaining work is
tracked in [Future Features](future-features.md#normal-mapping). This plan covers
the selected build-time optimization, not a new delivery of static normal mapping.

## Decisions

- Add `USE_NORMAL_MAPPING_3D`, enabled by default to preserve current behavior.
  CMake accepts `-DUSE_NORMAL_MAPPING_3D=OFF` (or `0`). Use a consistent numeric
  `0/1` compiler definition and value-based guards; an undefined macro in supported
  non-CMake builds must preserve the enabled default.
- Wire the same setting into Visual Studio, with a default-enabled
  `MbmUseNormalMapping3D` property and documented override. Validate propagation to
  affected targets and all supported platforms, including Android and iOS.
- Reuse `SUPPORTED_MAX_LIGHTS=1..4`, default `4`, and Visual Studio's existing
  `MbmSupportedMaxLights`. Do not add a second light-limit setting.
- The light limit is the compiled maximum of selected point lights per draw.
  The independent 3D directional light retains its existing behavior. A build
  configured for two point lights must generate arrays and loops capped at two.
- Preserve the runtime selection API and its validation. Runtime shader variants
  keyed by requested light count are outside this work.

## Disabled contract

| Area | `USE_NORMAL_MAPPING_3D=0` behavior |
|---|---|
| Default 3D lighting | Use geometric normals, including assets with an assigned normal map |
| Generated 3D shaders | Omit tangent inputs/varyings, mapping helpers, settings and normal-map sampling resources used exclusively by this feature |
| GPU resources | Do not create or upload derived normal-mapping geometry, tangent buffers, auxiliary declarations/layouts or fallback tangent buffers |
| Draw submission | Compile out feature-specific checks, bindings, settings uploads and alternate subset draws |
| Automatic CPU preparation | Skip preparation requested exclusively by 3D rendering, including runtime load/reload paths |
| Asset compatibility | Read, validate and preserve existing sections 14/15 and material references during supported load/save operations; malformed input must still fail validation |
| Explicit authoring | Keep explicit CPU preparation/import and persistence APIs available; MikkTSpace remains available for those operations |
| Lua/editor reporting | Expose the compiled rendering capability through a read-only query, distinct from assigned maps and stored tangents; explain geometric-normal fallback |
| 2D and custom shaders | Preserve existing 2dw normal mapping and explicit custom-shader texture semantics |

Keeping asset validation and authoring means this is not a promise to remove all
normal-map-related CPU code or persisted data from the binary/memory. Audit texture
loading separately: skip rendering-only 3D normal texture acquisition where the
usage is known, without removing stored references or breaking shared 2D/custom
shader usage. Do not globally disable the normal texture slot.

## Baseline checked against code

| Claim | Evidence | Verdict |
|---|---|---|
| Build light cap already exists | Root `CMakeLists.txt`, `include/core_mbm/light.h`, `platform-msvs/mbm-backend.props` | Reuse existing range, defaults and validation |
| Generated shaders already consume the compiled cap | `src/core_mbm/shader-opengl_es.cpp`, `shader-directx9.cpp`, `shader-directx11.cpp`, `shader-metal.mm` | Audit all eligible paths and correct omissions; do not assume a new performance gain |
| Normal-map CPU work mixes validation, automatic preparation and authoring | `src/core_mbm/mesh-manager.cpp`, `private/normal-map-asset.*`, `private/normal-map-preparation.*` | Separate render demand from preservation/explicit authoring |
| Build switch was previously only proposed | `docs/future-features.md` | Selected by this plan; not yet implemented |

## Implementation sequence

1. **Configuration and capability.** Introduce one consistent build contract,
   CMake help and Visual Studio property. Choose the query name after checking
   existing capability conventions; document its C++/Lua behavior. Preserve PIMPL
   boundaries and avoid public backend state or mutable configuration.
2. **Preparation and resource boundaries.** Trace synchronous/asynchronous load,
   reload, upload, settings changes and cleanup. Gate rendering-only preparation
   and resource storage in private interfaces. Retain validation and explicit
   authoring. Avoid scattered backend conditionals in `mesh-manager.cpp`.
3. **Backend shader and draw paths.** Apply the same contract in GLES, DX9 SM3,
   DX11 and Metal, including layouts, shader/cache keys, fallback resources and
   cleanup. Preserve DX9 SM2 fallback and skeletal behavior. Inspect generated
   shader text: C++ guards alone do not strip GLSL/HLSL/MSL strings.
4. **Light-cap audit.** Check generated default shaders, fixed/embedded lighting
   sources, normal-mapped and geometric paths, CPU upload sizes and constant-buffer
   layouts. Keep array lengths and loops consistent with the compiled cap without
   changing vec4 widths, matrix sizes or skeletal influence counts. Fix only actual
   omissions; arbitrary user shader source is not rewritten.
5. **Verification and delivery.** Execute the matrix below, record platform limits
   and measurements, update canonical documentation and bump `MBM_VERSION` when
   the implementation ships. Mark this plan complete only with verification evidence.

## Verification and acceptance

Read the `engine-testing` skill before building/running engine verification and
use the `doc-drift-check` skill for implementation-bound documentation updates.

- Cover both switch values and light caps 1, 2, 3 and 4 in isolated builds; the
  repository shares binary/library output directories, so avoid clobbering or
  accidentally comparing the same artifacts. Verify default and invalid settings.
- Inspect generated shaders and backend layouts for each combination: a disabled
  3D shader has no feature-specific normal-map interface, and a cap-two shader has
  no point-light slots/iterations above two. Inspect compiler resource/instruction
  output where tooling is available, preserving constant-buffer alignment.
- Reuse existing normal-map preparation/persistence C++ tests and asset, runtime,
  render, readback and authoring Lua tests. Adapt expectations for the disabled
  contract, and add focused checks for disabled resource/preparation paths and
  capability reporting. Verify synchronous and asynchronous load/save preservation.
- Exercise no map/no tangents, stored basis without a map, active map, zero strength,
  mixed subsets, shared assets and assignment/removal with a retained basis. Check
  geometric fallback when disabled, enabled visual parity, 2D normal mapping,
  custom shaders, skeletal fallback and DX9 profile fallback.
- Compare otherwise identical Release builds for binary size, shader compilation
  time/count, load time, CPU/GPU memory and draw/frame times. Separate normal-map
  switch comparisons from light-cap comparisons. Report measured changes without
  claiming zero overhead or guaranteed frame-rate gains.
- Execute native backend tests where available; report unavailable Windows/Apple
  validation explicitly. Source inspection does not substitute for native results.
- Update `docs/light.md`, `docs/lua-api.md`, build instructions and
  `platform-msvs/README.md`; update `docs/core-pimpl-status.md` if private boundaries
  change. Update `docs/mesh-v11-format.md` only if clarification is necessary; no
  binary format change is planned. Keep future work separate from shipped behavior.

## Exclusions

Per-material shader specialization in enabled builds, lazy GPU resources in enabled
builds, new skeletal/dynamic normal-mapping support, removal of CPU authoring,
runtime light-count shader variants and raising the maximum above four are separate
projects. Editor changes, if needed for capability reporting, must keep expensive
work out of per-frame callbacks and use ASCII-safe displayed punctuation.
