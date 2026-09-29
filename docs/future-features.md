# Future Features

This file tracks intentional follow-up work that should not be mixed into an active backend delivery.

## Scope decisions

Mini MBM prioritizes lightweight assets and a small engine implementation.
The following are deliberate exclusions, not deferred features:

- **32-bit mesh indices will not be implemented.** Meshes must fit the supported
  16-bit addressing limits; assets requiring larger indexed geometry must be
  divided or simplified. The MSH `indexWidth` field does not promise 32-bit support.
- **Asynchronous texture loading will not be implemented.** There is no planned
  `TEXTURE_MANAGER::loadAsync` decode/upload pipeline. Existing asynchronous mesh
  loading remains supported; texture-only particle/background paths retain their
  synchronous work behind callback-shaped APIs.

## Mesh simplification follow-up

Current API behavior and capabilities are documented in
[Mesh Simplification](mesh-simplification.md). Follow-up work should be driven by
real assets encountered during project development.

### Visual diagnostics

- add a Mesh Debug overlay for open boundaries, high-error regions, elongated faces, disconnected
  components, and inter-subset clearance protections;
- let the user focus or frame a reported region without stripping skeletal weights;
- provide a copyable diagnostic summary with stable frame/subset identifiers;
- keep diagnostic generation explicit or cached so idle editor frames do not rescan geometry.

### Performance and progress

- reuse or incrementally update spatial acceleration structures across collapse passes;
- reduce temporary allocations and repeated candidate/triangle set construction;
- add a broad phase for deformation-sample clearance checks and avoid redundant pose evaluations;
- measure Debug and Release behavior on high-density static, skeletal, and layered meshes;
- preserve the existing simplifier progress, cancellation and atomic publication when adding new
  processing stages; add checkpoints and measure cancellation latency inside expensive stages.

## Explicit blend-state API

`BLEND_DISABLE` is a legacy and misleading name. Its established behavior on DirectX 9,
DirectX 11, OpenGL ES, and Metal use the engine's default alpha composition
(`SRC_ALPHA`, `INV_SRC_ALPHA`), not disabled blending.

After the DirectX 11 backend delivery:

- introduce an explicit name such as `BLEND_ALPHA` for the current behavior;
- retain `BLEND_DISABLE` as a deprecated compatibility alias during migration;
- add a separately named opaque/no-blend mode if the engine needs true disabled blending;
- audit Lua constants, mesh serialization, editors, plugins, and shipped games before changing
  any public enum exposure;
- do not renumber the existing serialized blend values.

## Normal mapping

Current contracts are documented in [Lighting](light.md#material-texture-slots),
[Lua API](lua-api.md#normal-map-material-settings), [MSH sections](mesh-v11-format.md#optional-section_normal_map_tangents-14-section-version-1)
and [PIMPL boundaries](core-pimpl-status.md#normal-mapping-preparation-asset-data-and-rendering).
Static 3D normal mapping is implemented in OpenGL ES, DirectX 9 SM3, DirectX 11
and Metal. The work below extends that capability; it is not a condition for the
static delivery or a claim that these paths already work.

### Investigate overhead when normal mapping is unused

This is a performance concern to investigate, not a measured regression or an
approved implementation. The lightweight goal includes assets and applications
that never use normal maps.

Without a requested basis, preparation skips MikkTSpace and does not create
derived geometry buffers. However, eligible static lighting shaders include
normal-mapping support even for assets without tangents. Draw-time checks,
disabled settings, auxiliary buffers/bindings and shader inputs can remain.
The disabled fragment path returns the geometric normal before sampling the map;
this does not establish zero CPU/GPU overhead. Prepared assets without a texture
also retain derived data and GPU allocations.

Compare these options before choosing a design:

| Option | Potential benefit | Tradeoff / limit |
|---|---|---|
| Engine build define, e.g. `USE_NORMAL_MAPPING_3D` (proposed name only) | Compile out 3D normal-map rendering integration for applications that do not use it | Requires a separate engine build; enabling the feature still leaves unused-path costs in mixed scenes |
| Shader compile define / separate geometric and normal-mapped variants | Omit tangent inputs and calculations from shaders used by materials without the effect | Requires matching vertex layouts, variant/cache keys and switching when textures or basis availability change; does not by itself remove CPU checks or allocations |
| Lazy derived GPU resources | Avoid upload/memory cost until a prepared asset actually uses a map | Introduces first-use work and resource lifetime/synchronization decisions |

A C++ preprocessor define does not automatically remove code from generated
GLSL/HLSL/MSL strings. A build option must control backend source generation,
resource creation and draw integration consistently. Keep specialization in the
backend/private interfaces rather than scattering backend conditionals through
`mesh-manager.cpp`. The build option and per-material shader variants can coexist;
they solve different scopes of unused work.

If a build option is selected, define its default, CMake/Visual Studio/platform
wiring and the disabled contract before implementation. Distinguish disabling
3D rendering from removing CPU authoring, MikkTSpace or MSH section parsing.
Specify asset loading, validation, preservation on save, Lua/editor capability
reporting and geometric-normal fallback for sections 14/15. Preserve existing
2dw normal mapping unless a separate scope decision explicitly changes it.

Measure otherwise identical Release builds/scenes: no map/no tangents, prepared
basis without a texture, active map, strength zero and mixed subsets. Compare
binary size, load/compile time, shader/cache counts, CPU/GPU memory, draw CPU time
and GPU frame time across GLES, DX9 SM3, DX11 and Metal. Inspect compiled shader
resource/instruction usage instead of assuming the driver removes all disabled
work. Include late assignment/removal with a retained basis and shared assets.
Choose the solution from measured benefit versus code/cache/build complexity;
no define or new shader variant is implemented by this investigation entry.

### Skeletal deformation and dynamic geometry

Moving, rotating or scaling a whole static object already transforms its tangent
basis. Dynamic geometry needs additional handling:

| Situation | Follow-up contract |
|---|---|
| CPU/GPU LBS and DQS skinning | Deform normals and tangents with the reference-pose basis; preserve orientation, skin weights and palette associations |
| Vertex/normal edits by code, physics or a deformer | Update the affected basis when its inputs change |
| UV/topology edits | Invalidate derived batches and source remapping, then rebuild before consuming them |
| Morph targets/blend shapes, if added | Define how normals/tangents follow the deformation; do not presume existing morph support |
| Geometry-frame animation | Verify per-frame prepared bases, switching and cache invalidation; reuse persisted bases and generate only missing required data |
| A normal texture assigned after loading an asset without a basis | Prepare/upload once on demand; assigning a map to an already prepared static frame remains supported |

- Integrate the existing private CPU `normal_map::remapSkinWeights` helper into
  derived skeletal buffers. Duplicated seam vertices must retain canonical
  influences, skeleton identity, frame association and bone palette.
- Reuse reference-pose tangents during skeletal animation; do not run MikkTSpace
  each frame. Respect existing LBS/DQS scale/palette restrictions and verify CPU/GPU
  agreement, orthogonality, reflected transforms and degenerate fallbacks.
- Keep source authoring geometry separate from derived render batches. Selection,
  physics, extraction and export must continue using source indices. Preserve
  16-bit batch partitioning without truncating indices or losing influences.
- Dynamic source updates currently discard derived GPU batches. Define their
  regeneration before enabling the effect again; extend the same contract to all
  backends through private backend-neutral hooks.
- Cover pause/resume, animation/frame changes, edits followed by save/export,
  async loading, preview reload, late map assignment/removal and shared assets.
  Changes only to strength or convention must not regenerate tangents.
- Keep normal maps and tangents optional. Consider deferring GPU allocation for
  prepared assets without a map, measuring the memory/loading tradeoff and keeping
  later assignment reliable. No full-mesh scans or geometry uploads in an idle loop.

### Import, texture handling and editor tools

- Extend external importers, including the direct Blender producer, to supply
  per-corner tangents through the existing explicit import/recalculate contract.
  Preserve valid imported bases compatible with the bake; do not silently replace
  them after UV/topology postprocessing. Compare using fixed triangulation,
  normals and known tangent-space conventions rather than promising identical
  final images across engines.
- Add convention/strength controls to Image Mesh Editor with project persistence,
  reload and Undo/Redo. Mesh Debug already exposes these settings.
- Distinguish assigned maps, effective 3D normal mapping and fallback in editor
  diagnostics; add temporary with/without-map preview without changing saved data.
- Audit role-aware texture caching, linear sampling, packed-channel decoding,
  compression and vector-appropriate mipmaps before supporting additional normal
  texture encodings. Avoid sRGB treatment of vector data and conflicts when an
  image is reused in multiple roles.
- Any extension of the 3D convention/strength properties to `2dw` requires an
  explicit behavior decision; the current 2dw equations do not consume them.
- PBR, parallax, displacement and consumption of specular/AO/emissive maps are
  separate material/rendering projects, not implicit parts of normal mapping.

### Additional validation and platform coverage

- Validate Android GLES2, iOS and additional GPUs; desktop backend support does
  not establish device-specific coverage. Exercise real device/context loss and
  restoration, not only ordinary asset reload.
- Evaluate DX9 SM2 lighting separately. Static normal mapping requires SM3;
  selecting the geometric fallback does not prove that the multi-light shader
  fits SM2. Measure actual shader budgets and distinguish compiler-profile tests
  from physical-device validation.
- Compare load time, CPU/GPU memory and frame cost, separating preparation/upload
  from shading. Use deterministic images with tolerances, not cross-GPU byte
  equality or metrics captured with different cameras/lights.
- Retain coverage for no map, neutral/detail maps, strengths 0/1/>1, +Y/-Y
  equivalents, mixed subsets, mirrored UVs, negative/nonuniform scale, degenerate
  bases, IB/VB, custom shaders, unlit/HUD and 2dw regressions.
- Use the existing `normal-map-render-test.lua` and `normal-map-asset-test.lua`
  scenes for visual checks; preparation/persistence testLib suites and runtime,
  authoring and readback scenes cover CPU/file/API contracts. Require explicit
  PASS markers and graphics validation where available, not exit status alone.

## Mesh format and loader follow-up

The current format contract is in [Mesh V11](mesh-v11-format.md).

- Consider vertex quantization and optional build/provenance metadata only with
  explicit versioned section contracts; preserve the fixed header and zeroed
  reserved fields.
- Consider author-controlled per-subset alpha flags instead of the writer's
  current `{1,0,0,0}` value if an asset workflow needs them.
- Extend shader-effect authoring beyond the existing FX texture controls: PS/VS
  names, animation type/time, blend operation and named uniform ranges. Uniform
  names require shader compilation/reflection rather than positional disk data.
- Broaden live async-loader validation across renderizable types, including
  genuinely asynchronous execution, GC safety before completion and wrong-type/
  missing-file failures. Mesh normal-map loading already has sync/async coverage.
