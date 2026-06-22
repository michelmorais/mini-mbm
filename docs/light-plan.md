# Mini MBM Light Plan

This document is now the lighting roadmap/history companion to `docs/light.md`.

Use:

- `docs/light.md` for the current engine behavior and public lighting/material contract
- `docs/light-plan.md` for milestone history, resolved design decisions, and deferred future work

The branch is now past the original early milestones. The main remaining future item is shadow-map
design/implementation, plus any later editor polish or optimization work.

## Goal

Introduce portable engine lighting that works consistently on DirectX 9, OpenGL ES, and Metal.

The first implementation should prove the complete cross-backend path with the smallest useful
feature set:

- ambient light
- one directional light
- diffuse Lambert lighting from existing mesh normals
- textured and untextured meshes
- no new texture-stage dependency

After that is stable, expand to 2D lighting, normal maps, point lights, specular/material controls,
editor support, Lua APIs, and scene serialization.

## Current State

### What already exists

- `FVF_PROVIDE_BY_ENGINE` already distinguishes `FVF_POS`, `FVF_POS_UV`, `FVF_POS_NOR`, and
  `FVF_POS_NOR_UV`.
- `BUFFER_GL::loadBuffer()` receives normal arrays on all active backends.
- OpenGL ES, DirectX 9, and Metal already upload/bind normal vertex data when the active shader
  asks for it.
- Mesh v8 already stores normal metadata through `hasNorText[0]`.
- Mesh headers already include `util::MATERIAL_GLES` with diffuse, ambient, specular, emissive,
  and power fields. The fields are useful for lighting, but the `GLES` suffix is misleading
  because this is an engine/file-format material, not an OpenGL ES material.
- Light was intentionally removed from `TYPE_CLASS`; new engine lights are scene state, not
  `RENDERIZABLE` objects.
- Shader code already uses engine-reserved names such as `aPosition`, `aNormal`, `aTextCoord`,
  `mvpMatrix`, `mvMatrix`, `sample0`, and `sample1`.

### What is missing

- No runtime light state exists.
- Default shaders pass or declare normals but do not use them for lighting.
- OpenGL ES can warn that `aNormal` was optimized out because current shaders do not consume it.
- DirectX 9 currently disables fixed-function lighting with `D3DRS_LIGHTING = false`.
- There is no shared engine contract for lighting uniforms across DirectX 9, OpenGL ES, and Metal.
- There is no Lua/editor/API surface for creating or editing lights.

### Texture-stage constraints

Current texture-stage meaning must be preserved:

- Stage 0: primary texture, per frame/subset.
- Stage 1: existing shader FX / secondary texture path, not per subset.

Lighting must not steal stage 1. The first lighting milestone should use only uniforms/constants.

Before adding more rendering state, audit texture-stage behavior:

- DirectX 9 binds stage 1 from `getTextureByStage(1, 0)`.
- Metal binds `sample1` from `getTextureByStage(1, 0)`.
- OpenGL ES index-buffer rendering binds stage 1 from `getTextureByStage(1, 0)`.
- OpenGL ES vertex-buffer rendering appears to bind stage 1 from `getTextureByStage(0, i)`,
  which should be reviewed and likely corrected.

DirectX 9 render-to-texture must also keep the sampler-unbind guard before `SetRenderTarget()`;
lighting or future shadow-map work must not remove or bypass it.

## Design Decisions

### 1. Use shader lighting, not DirectX fixed-function lighting

Recommended: yes.

DirectX 9 has fixed-function lighting support, but OpenGL ES and Metal paths are shader-centric.
Using fixed-function lighting would create backend divergence and bypass the existing shader/CFG
pipeline. The portable engine feature should be implemented in shaders and backend uniform upload.

### 2. Start with ambient plus one directional light

Recommended: yes.

This validates:

- normal loading
- model-view normal transforms
- shader uniform naming
- default shader generation
- platform parity
- material diffuse/ambient usage

It avoids mixing the first pass with light lists, attenuation, shadow maps, normal maps, asset-format
changes, or editor serialization.

The first shader should be Lambert diffuse plus ambient only, using mesh `MaterialAmbient` and
`MaterialDiffuse` immediately. Add emissive after the one-light path is working, then add specular
highlights with `MaterialSpecular` and `MaterialPower`.

The first backend proof can target renderables in `3d` coordinate mode, but the complete lighting
feature must also cover 2D. Mini MBM has three placement modes: `2ds`, `2dw`, and `3d`. Any
`RENDERIZABLE` class can be used as `3d`; lighting decisions follow the object's coordinate mode,
not only its concrete class.

### 3. Do not use texture stage 1 for lighting

Recommended: yes.

Stage 1 already means `sample1` for existing shader FX. The lighting contract should use
engine-owned uniforms. If normal maps are added later, introduce explicit material texture slots
or a deliberate stage-extension design instead of overloading the existing stage-1 convention.

Current structural constraints:

- MBM v8 stores only one primary texture name per subset in `HEADER_DESC_SUBSET::nameTexture`.
- Animation shader steps can store one stage-1/`sample1` texture through
  `HEADER_INFO_SHADER_STEP::lenTextureStage2`.
- `BUFFER_GL` currently has per-subset stage 0 and one shared stage 1 pointer; it does not model
  arbitrary material texture slots.
- A serializable material-texture-slot feature likely needs a new mesh format version after
  `CURRENT_VERSION_MBM_HEADER`, plus loader/saver/editor support for explicit per-frame-subset
  material texture slots. Normal maps should be the first new slot, not a one-off field.

### 4. Treat light uniforms as engine-owned, not CFG-owned

Recommended: yes.

CFG variables are user/shader-effect controls. Lighting needs consistent engine state across
default shaders and custom shaders. Reserve a small set of names that the engine uploads when the
compiled shader uses them.

Reserved names:

- `LightEnabled`
- `LightCount`
- `AmbientColor`
- `LightDirectionView`
- `LightColor`
- `MaterialDiffuse`
- `MaterialAmbient`
- `MaterialSpecular`
- `MaterialEmissive`
- `MaterialPower`

The engine should fill these automatically through the existing `SHADER::update()` /
`BASE_SHADER::update()` shader-variable update path when the active shader declares them.
Reserved names are exact and case-sensitive; do not add aliases.
In the one-light milestone, `LightEnabled` and `LightCount` are integer reserved uniforms.
`LightEnabled` uploads `1` when lighting is enabled for the target and `0` when disabled.
`LightCount` follows the same `1` or `0` value until multi-light expands it.
Public runtime state/API uses the name `directionalColor` for the directional light color. The
shader reserved name remains `LightColor` for the one-light shader contract.
Public runtime state/API uses `directionalDirection` for the directional light direction. Internal
state may store `directionalDirectionWorld`; the shader reserved uniform remains
`LightDirectionView` after backend view-space conversion.
Public `directionalDirection` has the same semantic in world space: it is the direction the light
travels, not the direction from a surface toward the light.
`LightDirectionView` means the direction the light travels in view space, not the direction from
the surface toward the light. Lambert diffuse therefore uses `-LightDirectionView`.
Keep both names even though they mirror each other in the one-light milestone: `LightEnabled` is
the explicit compatibility/on-off switch, while `LightCount` is the active-light count for
multi-light shaders.
Reserved light/material names are independent and optional. A shader may declare `LightCount`
without `LightEnabled`, or any other subset of reserved names; the engine uploads only the names
that are active in the compiled shader.
Custom shaders may declare reserved material names such as `MaterialSpecular`, `MaterialEmissive`,
and `MaterialPower` before default shaders consume them. If the compiled shader keeps those
uniforms/constants active, the engine should look them up and supply their material values.
CFG variables must not use reserved engine names. If a CFG declares a reserved name such as
`LightCount` or `MaterialDiffuse`, the loader/editor should reject the shader CFG with a clear
error; engine state owns those uniforms.
This applies to the full reserved list immediately, including material names that the first shader
does not consume yet.
Enforce this in the low-level shader CFG load/add-variable path so all entry points behave the
same way.
Keep the reserved-name list in one shared helper/table such as `isReservedShaderUniformName(name)`;
CFG parsing, editor/import checks, and backend upload must not duplicate the list.
Place this helper in shared shader core code, for example `shader-reserved-names.h/.cpp` or the
existing shader variable CFG module, not in any backend-specific file.

The current CFG shader-variable path is float-oriented. This lighting work should include scalar
`VAR_INT` support unless implementation proves unexpectedly risky. `VAR_INT` is for custom shader
variables declared in CFG. Engine-owned counters such as `LightCount` are reserved engine uniforms,
not CFG variables, but they can reuse the same backend integer upload capability.

Keep existing names unchanged:

- `aPosition`
- `aNormal`
- `aTextCoord`
- `mvpMatrix`
- `mvMatrix`
- `sample0`
- `sample1`

### 5. Keep lighting opt-in

Recommended: yes.

Default behavior should remain visually compatible when no light is enabled. Enabling lighting
is explicit per scene. This avoids unexpectedly darkening old games that have normals in assets
but were authored for unlit rendering.

Initial C++ API shape uses free functions in the `mbm` namespace, not new virtual/state methods on
`SCENE`. The target is an explicit lighting target/mode, not a boolean, because the engine has
`3d`, `2dw`, and `2ds` coordinate modes:

```cpp
mbm::setLightEnabled(mbm::LIGHT_TARGET_3D, true);
mbm::setAmbientLight(mbm::LIGHT_TARGET_3D, ...);
mbm::setDirectionalLightDirection(mbm::LIGHT_TARGET_3D, ...);
mbm::setDirectionalLightColor(mbm::LIGHT_TARGET_3D, ...);
mbm::setDirectionalLight(mbm::LIGHT_TARGET_3D, direction, color); // convenience
mbm::resetLight(mbm::LIGHT_TARGET_3D);
const mbm::LIGHT_STATE &state = mbm::getLightState(mbm::LIGHT_TARGET_3D);
```

C++ should use typed target constants/enums only and target-first argument order. Keep string
parsing in the Lua binding layer.

Initial Lua API shape mirrors the C++ namespace API and applies to the active script scene:

```lua
mbm.setLightEnabled('3d', true)
mbm.setAmbientLight('3d', {r=0.2, g=0.2, b=0.2, a=1.0})
mbm.setDirectionalLightDirection('3d', {x=0, y=-1, z=-1})
mbm.setDirectionalLightColor('3d', {r=1, g=1, b=1, a=1})
mbm.setDirectionalLight('3d', {x=0, y=-1, z=-1}, {r=1, g=1, b=1, a=1}) -- convenience
mbm.resetLight('3d')
local light = mbm.getLightState('3d')
```

Lua `mbm.getLightState(target)` should return a table copy:

```lua
{
    enabled = true,
    target = '3d',
    ambientColor = {r = 0.2, g = 0.2, b = 0.2, a = 1.0},
    directionalColor = {r = 1.0, g = 1.0, b = 1.0, a = 1.0},
    directionalDirection = {x = 0.0, y = -0.707, z = -0.707},
}
```

Do not expose per-field configured flags in Lua unless editor tooling later proves it needs them.

Lua color arguments should accept both named-field tables (`{r=1,g=1,b=1,a=1}`) and array-style
tables (`{1,1,1,1}`), matching existing binding patterns, then normalize internally to `COLOR`.
Lua direction arguments should accept both named-field tables (`{x=0,y=-1,z=-1}`) and array-style
tables (`{0,-1,-1}`), then normalize internally to `VEC3` and apply the zero-length fallback rule.
Lua light functions should use target-first argument order for consistency and simpler binding
validation. Lua setters may also accept separate numeric arguments for ergonomics. Use target-first
numeric forms to avoid ambiguity with optional alpha or future value shapes, such as
`mbm.setAmbientLight(target, r, g, b, a)` and
`mbm.setDirectionalLightDirection(target, x, y, z)`. Keep table arguments as the documented primary
form.

Lua target arguments must be exact strings: `'3d'` or `'2dw'`. Do not provide an implicit default
target and do not accept aliases. Future `'2ds'` support should be added only when that lighting
target is explicitly implemented.
Before dedicated 2D rendering exists, calls for target `'2dw'` should accept and store state, but
log a clear "`2dw` lighting rendering not implemented yet" warning/status instead of failing target
validation or changing setter return values.
Emit this temporary warning only once per scene/target to avoid noisy per-frame logs.
Lua light setters should follow existing namespace style: return no value on success and use
`lua_error_debug` for invalid arguments/targets.

### 6. Use the existing classic material model first

Recommended: yes.

See `docs/light.md` for the material-model explanation and the distinction between classic
materials and PBR materials.

The existing material fields cover the classic fixed-function/Phong-style material model:

- `Diffuse`
- `Ambient`
- `Specular`
- `Emissive`
- `Power`

That is enough for the first lighting implementation and for later specular highlights. Do not
introduce a PBR material model in the first lighting pass. Rename the C++ type from
`MATERIAL_GLES` to a platform-neutral engine name such as `MATERIAL`, while preserving the on-disk
mesh layout.

## Proposed Runtime Model

Add small active-scene light states to `DEVICE::Impl` without changing the `SCENE` public
signature. The engine already has one live scene at a time, so `mbm::` C++ functions and Lua
`mbm.*` wrappers can update the state for a target such as `3d` or `2dw`. Backend upload reads the
target state through `DEVICE` during
rendering. Lifetime remains scene-level because `CORE_MANAGER::logic()` resets it during scene
changes; it is storage in `DEVICE`, not a persistent device-global effect.

First-pass state:

```cpp
struct LIGHT_STATE
{
    bool  enabled;
    bool  hasAmbientColor;
    bool  hasDirectionalDirection;
    bool  hasDirectionalColor;
    COLOR ambientColor;
    VEC3  directionalDirectionWorld;
    COLOR directionalColor;
};
```

Default enabled-light values:

```cpp
AmbientColor = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
LightColor   = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
Direction    = normalize(VEC3(0.0f, -1.0f, -1.0f));
```

The default direction means light traveling downward and forward in world space.

Rules:

- Keep default ambient at `0.2`, not full white, because lighting is opt-in and should make
  directional shading visible by default.
- Directional direction is authored in world space.
- `setDirectionalLightDirection` and convenience `setDirectionalLight` normalize direction
  internally. A zero-length direction should fall back to the default direction.
- Light colors use normalized floats clamped to the `0.0..1.0` range.
- `AmbientColor.a` and `LightColor.a` are stored for API/type consistency but ignored by the
  first-pass lighting shader; lighting uses `.rgb`.
- `mbm::setLightEnabled(target, true)` initializes the target with the default light values if no
  light values were configured yet.
- Any light value setter marks that field as configured. Later `setLightEnabled(target, true)`
  fills only missing defaults and must not overwrite configured values.
- `mbm::setLightEnabled(target, false)` only disables the target; it does not erase configured
  ambient, color, or direction values.
- `setAmbientLight`, `setDirectionalLightDirection`, `setDirectionalLightColor`, and convenience
  `setDirectionalLight` only set values; they do not implicitly enable lighting.
- The combined `setDirectionalLight(target, direction, color)` is a convenience wrapper over the
  direction and color setters and marks both fields as configured.
- `mbm::resetLight(target)` clears the target back to disabled/default state and clears all
  configured flags.
- `setLightEnabled`, value setters, getters, and reset are target-specific. Changing `3d` does not
  implicitly affect `2dw` or future `2ds`.
- `getLightState('2dw')` returns the stored `2dw` state even before dedicated 2D rendering exists.
- C++ `mbm::getLightState(target)` returns a const reference. Mutation stays behind explicit
  setters so direction normalization/default initialization invariants remain centralized.
- Lua `mbm.getLightState(target)` returns a table copy for tools/tests, not a live mutable state
  reference.
- Material uniforms are sourced per subset when subset material data exists. If a renderable or
  subset has no material data, use fallback classic material defaults:
  `MaterialDiffuse = (1, 1, 1, 1)`, `MaterialAmbient = (1, 1, 1, 1)`,
  `MaterialSpecular = (0, 0, 0, 1)`, `MaterialEmissive = (0, 0, 0, 1)`,
  and `MaterialPower = 0`.
- The first backend proof lights objects in `3d` coordinate mode.
- In `3d`, only normal-capable vertex formats are lit. Objects without vertex normals render unlit
  exactly as before; do not synthesize runtime normals in the render path.
- 2D lighting is required as a follow-up feature. `2dw` lighting should be a dedicated point/radius
  light pipeline with normal maps, not just the 3D directional path applied to flat geometry. `2ds`
  should remain explicitly opt-in because HUD/UI/editor overlays often need predictable unlit
  rendering.
- In `2dw`, dedicated lighting may use a default flat normal `(0, 0, 1)` for objects without a
  normal map; normal maps upgrade the result with surface detail.
- 3D and 2D lighting use separate target states so they do not interfere with each other.
- Backend upload converts `directionalDirectionWorld` to `LightDirectionView` using only the view
  matrix rotation, ignoring translation, then normalizes the result.
- If lighting is disabled, shaders render exactly as before.
- If lighting is enabled but geometry has no normals, shader output should fall back to unlit
  texture/color instead of failing.
- Material values initially come from the existing material struct, with sensible defaults when
  unavailable.
- `CORE_MANAGER::logic()` should fully reset the active light state during scene changes before the
  new scene reaches `onInitScene()`, so each scene opts into its own lighting and does not inherit
  old configured values.

## Shader Contract

### OpenGL ES

Default vertex shader with normals should:

- read `aNormal`
- transform it using `mvMatrix`
- pass normalized view-space normal to the pixel shader

Default pixel shader should:

- sample `sample0` when UV exists
- multiply sampled RGB by `MaterialDiffuse.rgb`
- preserve final alpha as sampled alpha multiplied by `MaterialDiffuse.a`
- compute `diffuse = max(dot(normalView, -LightDirectionView), 0.0)`
- compute ambient contribution as `AmbientColor.rgb * MaterialAmbient.rgb`
- use the first-pass classic Lambert formula:
  `base = sample0.rgb * MaterialDiffuse.rgb`;
  `lit = base * (AmbientColor.rgb * MaterialAmbient.rgb + LightColor.rgb * diffuse)`
- clamp/saturate final RGB to `0.0..1.0`
- do not apply lighting to alpha
- for untextured material-only rendering, use `MaterialDiffuse.a` as final alpha

### DirectX 9

Default HLSL should mirror OpenGL ES:

- input semantic `NORMAL`
- output normal to pixel shader
- constants for light state
- texture `sample0 : register(s0)`
- preserve `sample1 : register(s1)` behavior for existing FX shaders

Do not enable `D3DRS_LIGHTING` for this feature.

### Metal

Default MSL generation should:

- include normal in `VOut` only for normal FVF variants
- transform normal with `mvMatrix`
- upload light constants through the same engine uniform path used by render calls
- keep `[[texture(0)]]` and `[[texture(1)]]` meaning unchanged

## Implementation Milestones

### Milestone 0: Audit and stabilize texture-stage behavior

- Confirm stage-0 and stage-1 behavior in `shader-opengl_es.cpp`, `shader-directx9.cpp`, and
  `shader-metal.mm`.
- Fix the OpenGL ES vertex-buffer stage-1 path if confirmed incorrect.
- Add a tiny regression scene or smoke script that uses `sample1` on a vertex-buffer object and
  an index-buffer object.
- Reconfirm DX9 render-to-texture sampler unbinding remains before `SetRenderTarget()`.
- Remove the `DEBUG_SHADER_D3D_MINIMIZE_ERROR` guarded update path from DirectX 9 shader rendering
  as part of making shader constant updates deterministic.

### Milestone 1: Scene light state and API skeleton

- Add active-scene light state in `DEVICE::Impl` with no rendering behavior change when disabled.
- Add C++ free functions in namespace `mbm`; do not change the `SCENE` class signature.
- Add Lua `mbm.*` wrappers that call the same active-scene light API.
- Include `resetLight(target)` for explicit target reset.
- Include a C++ const reference getter and Lua table-copy getter for tests/tools.
- Store separate target states for `3d` and `2dw` from the start.
- Define `LIGHT_TARGET_2DW` in milestone 1 for API stability, but keep `2dw` rendering disabled or
  explicitly not implemented until the dedicated 2D lighting milestone.
- Accept and store `2dw` light state before rendering is implemented, while logging a clear
  "`2dw` lighting rendering not implemented yet" warning/status without changing Lua setter return
  values.
- Emit the temporary `2dw` not-implemented warning only once per scene/target.
- Fully reset light during `CORE_MANAGER::logic()` scene-change flow before `onInitScene()` so new
  scenes do not inherit old configured light values.
- Do not add light back to `TYPE_CLASS`; the first implementation treats lights as scene state.

### Milestone 2: Material naming cleanup

- Rename `util::MATERIAL_GLES` to a platform-neutral name such as `util::MATERIAL`.
- Keep `MATERIAL_GLES` as a compatibility alias during the transition so existing code and tools do
  not break immediately.
- Preserve the existing material fields and on-disk mesh layout.
- Update mesh loading/saving, Mesh Debug Lua bindings, editor material UI, and docs.
- Treat material fields as reserved shader inputs through `MaterialDiffuse`, `MaterialAmbient`,
  `MaterialSpecular`, `MaterialEmissive`, and `MaterialPower`.
- Current runtime upload may source material from the active mesh material until per-subset material
  is threaded through the render path; still use the documented fallback classic material defaults
  when no material data is active.

### Milestone 3: Scalar shader integer variables

- Add scalar `VAR_INT` support to the shader variable type system.
- Extend CFG parsing using the existing shader-variable key structure:
  `[<shader-key>][int][<varName>] = min I max I default I`.
- Add `int` as the canonical type token in `SHADER_CFG::addVar(type, name, values)`. Do not add a
  new `name:type` or assignment-style syntax.
- Preserve every existing float, vector, and color variable behavior.
- Reject CFG variables whose name collides with a reserved engine light/material uniform in the
  low-level shader CFG load/add-variable path.
- Update `VAR_CFG` / `VAR_SHADER` storage or typed access so `VAR_INT` is represented as an
  integer value, not just a float variable with another enum name.
- Make `VAR_INT` follow the same min/max/default and animation behavior as existing scalar
  variables, with integer rounding/clamping defined in one shared helper.
- When animation produces a fractional intermediate `VAR_INT` value, round to the nearest integer,
  then clamp to the configured `min..max` range.
- Add backend upload support for integer uniforms/constants:
  - OpenGL ES: scalar integer uniform upload.
  - DirectX 9: scalar integer constant upload or a documented compatible mapping if the active
    shader profile requires one.
  - Metal: scalar integer uniform/buffer packing without breaking existing float packing.
  - Dummy backend: no-op behavior that still validates parsing/state.
- Update Lua/editor/plugin-helper shader variable paths if they expose or edit CFG variables.
- Keep this milestone scalar-only; do not add int vectors until a real shader contract needs them.

### Milestone 4: Backend uniform plumbing

- Add backend-specific lookup/upload for reserved light names through the existing
  `SHADER::update()` / `BASE_SHADER::update()` path.
- Track the current render light target during `CORE_MANAGER::render()`: `3d` objects receive the
  `3d` light state, `2dw` objects receive the stored `2dw` state, and `2ds` rendering disables
  light upload until a future explicit `2ds` target exists.
- Use the same shared reserved-name helper/table for CFG rejection and backend reserved-uniform
  upload decisions.
- Upload only uniforms/constants that are active in the compiled shader.
- Upload `LightEnabled = 1` for enabled targets and `LightEnabled = 0` for disabled targets when a
  shader declares it. Treat it as an integer uniform, not a shader `bool`.
- Upload `LightCount = 1` for enabled one-light targets and `LightCount = 0` for disabled targets
  when a shader declares it.
- Keep CFG variable upload behavior unchanged.
- Add debug logging for missing optional light uniforms only when useful; avoid noisy warnings for
  shaders that intentionally do not use lighting.
- Metal cannot query GLSL/HLSL-style named uniforms at draw time. Use fixed optional Metal argument
  buffer slots for the reserved names:
  - `LightEnabled` at `[[buffer(4)]]`
  - `LightCount` at `[[buffer(5)]]`
  - `AmbientColor` at `[[buffer(6)]]`
  - `LightDirectionView` at `[[buffer(7)]]`
  - `LightColor` at `[[buffer(8)]]`

### Milestone 5: Lit default shaders

- Update generated default shaders for normal FVF variants with ambient plus Lambert diffuse first.
- First implementation may use scene ambient/directional color directly until material/subset
  upload is completed; then fold mesh `MaterialAmbient` and `MaterialDiffuse` into this path.
- Preserve old unlit output when lighting is disabled.
- Keep no-normal FVF variants unlit.
- Do not synthesize normals for no-normal `3d` renderables at runtime.
- Lighting affects RGB only. Preserve alpha from the texture/object color.
- Ensure `aNormal` is no longer optimized out when lighting is enabled and the default lit shader
  is selected.

### Milestone 6: Built-in lit shader resources

- Add explicit built-in lit shader entries for each backend if default-generation is not enough.
- Add `lit textured.ps` and `lit solid.ps` as backend-portable built-in shader resources.
- Prefer pixel-shader-only built-ins for this milestone so the engine can keep using its
  FVF-matched default/generated vertex shader path automatically.
- Keep CFG variable names separate from reserved engine light names.
- Add examples for textured and untextured lit meshes.

### Milestone 7: One-light platform validation

- Validate one-light rendering on Linux/OpenGL ES.
- Validate one-light rendering on Windows/DirectX 9.
- Validate one-light rendering on macOS/Metal.
- Build the validation scene so the default direction `(0, -1, -1)` visibly lights normal-facing
  geometry.
- First validate with a tiny generated/debug mesh with known normals to isolate shader/backend
  behavior, then validate with an imported `.msh` mesh asset to cover current mesh loading,
  normals, and material data.
- Do not begin multi-light work until the one-light path is verified on all three backend families.

### Milestone 8: Dedicated 2D lighting and normal-map design

- Design dedicated 2D point/radius lighting for `2dw`.
- Require normal-map support for useful 2D lighting.
- Keep `2dw` multi-light selection per object rather than one global scene-wide first-`N` light
  list.
- Use per-object nearest-light selection as the first multi-light strategy:
  - consider only lights whose radius reaches the object
  - rank candidates by distance to the object center
  - keep the nearest validated `N` lights for that object
- Keep `2ds` lighting explicitly opt-in only.
- Design generic per-frame-subset material texture slots instead of a one-off normal-map field.
- Preserve compatibility by treating existing `HEADER_DESC_SUBSET::nameTexture` as the primary /
  diffuse slot.
- Store additional material textures as a counted list of typed slots, not as fixed fields.
- Runtime/file-format groundwork now exists for typed subset material texture slots in mesh version
  `v9` (`MATERIAL_TEXTURE_SLOT_VERSION_MBM_HEADER`), with unknown slot types skipped by recorded
  payload length.
- Include enough per-slot length information for loaders to skip unknown optional slot types.
- Store the counted slot list adjacent to each frame subset descriptor/data rather than in a global
  table.
- Match existing primary texture packaging behavior for material texture slots, including
  path-based vs embedded/compressed image handling after auditing the v8 save/load path.
- Implement the normal slot first.
- Reserve future slots such as specular, emissive, and mask without implementing them in the first
  normal-map pass.
- Decide whether the material-texture-slot path requires a new MBM header version after
  `CURRENT_VERSION_MBM_HEADER`.
- Extend `BUFFER_GL` texture storage beyond stage 0 plus shared stage 1 before using normal maps.
- Do not overload existing stage 1 / `sample1` FX texture with normal maps.
- Runtime now binds the first per-subset normal-map slot through stage / sampler `2`
  (`sample2`) so stage `1` / `sample1` remains reserved for the existing FX path.
- First runtime implementation now ships as one engine-managed `2dw` point/radius light with
  per-object shading, optional per-subset normal map from `sample2`, and flat-normal fallback
  `(0, 0, 1)` when no normal map exists. Expand that path first with validated multi-light
  selection before considering broader screen-space/light-buffer composition.
- Current `2dw` shading does not consume stored mesh/vertex normals. It uses the per-subset
  normal-map texture from `sample2` when present; otherwise it falls back to the flat normal
  `(0, 0, 1)`.
- Validated `2dw` behavior matrix:
  - normal map + mesh normals -> use `sample2` normal map
  - normal map + no mesh normals -> use `sample2` normal map
  - no normal map + mesh normals -> use flat fallback normal `(0, 0, 1)`
  - no normal map + no mesh normals -> use flat fallback normal `(0, 0, 1)`
- Mesh/vertex normals remain useful for `3d` lighting and asset/tooling, but they are currently
  informational only for the shipped `2dw` lighting path.

### Milestone 8.5: Texture role and shader naming cleanup

- Introduce semantic engine texture roles before multi-light work expands shader/backend binding
  state.
- Keep texture ownership separate from backend binding slots:
  - `TextureDiffuse`: primary per-frame/per-subset texture.
  - `TextureAnimationEffect`: per-animation shader-effect texture currently stored through the
    legacy `fileNameTextureStage2` / `textureOverrideStage2` path.
  - `TextureNormal`: per-frame/per-subset material normal-map texture.
  - `TextureSpecular`, `TextureEmissive`, and `TextureMask`: known/reserved material texture roles
    until their runtime binding and shader behavior are implemented.
- Do not move `TextureAnimationEffect` into material texture slots. It is animation-effect state,
  not surface material state.
- Make semantic texture names the default for new engine-generated and built-in shaders.
- Keep legacy `sample0`, `sample1`, and `sample2` only as a compatibility shader naming profile.
- Reject shaders that mix legacy texture names and semantic texture role names in the same source.
- Auto-detect shader texture naming profile from source, with an optional CFG declaration such as
  `[shader.ps][textureNaming] = semantic|legacy|none` for validation.
- Treat the optional CFG texture-naming declaration as a validation contract, not as a silent
  rewrite switch.
- First implementation keeps physical backend slots unchanged while centralizing the role mapping:
  - `TextureDiffuse` -> slot/register/index `0`.
  - `TextureAnimationEffect` -> slot/register/index `1`.
  - `TextureNormal` -> slot/register/index `2`.
- Bind role-specific fallback textures for supported missing roles:
  - missing `TextureDiffuse` -> white texture.
  - missing `TextureAnimationEffect` -> white texture.
  - missing `TextureNormal` -> flat normal texture `(0.5, 0.5, 1.0, 1.0)`.
- Fail clearly if a shader declares reserved-but-not-runtime-complete roles such as
  `TextureSpecular`, `TextureEmissive`, or `TextureMask` before their binding path exists.

### Milestone 9: Multi-light design

- Add multi-light support only after one-light validation is complete on OpenGL ES, DirectX 9, and
  Metal.
- Decide requested vs validated max light count, reserved array names, per-object light-selection
  strategy, DirectX 9 constant limits, Metal buffer layout, and fallback behavior before the full
  implementation.
- `LightCount` is already a reserved engine name. The multi-light milestone expands it beyond the
  one-light `0` or `1` contract.
- The first multi-light contract should remain explicit:
  - developer requests a max light count
  - engine validates that request against the active backend/profile
  - each object receives only the nearest validated `N` candidate lights that reach it
- The first backend-validation slice should reject unsupported requested caps immediately and expose
  query helpers for supported/validated max-light values before the shader-array upload work lands.
- Add a backend-agnostic selection helper and a Lua debug query before shader upload so the
  per-object nearest-light rule can be validated independently from GPU wiring.
- First shader-array upload slice should use:
  - `LightCount`
  - `LightPositionView[0]`
  - `LightRadius[0]`
  - `LightColor[0]`

### Milestone 10: Editor exposure

- Add editor controls after C++ and Lua behavior is validated.
- Likely first UI targets: Scene Editor and Mesh Debug preview.
- Mesh Debug should expose per-subset typed material texture slots, starting with the normal-map
  slot, so `2dw` lighting assets can be authored and smoke-tested before the full `2dw`
  light-buffer pipeline is finished.
- Sprite Maker must preserve typed per-frame/per-subset material texture slots, including the
  normal-map slot, during load/import/save even before it exposes dedicated authoring UI for those
  extra textures. Round-trip safety comes before editor polish here.
- Current status: substantially covered. Mesh Debug already exposes typed material texture slots and
  Sprite Maker already preserves typed per-frame/per-subset material texture slots during
  load/import/save. Any future work here is editor polish, not the next core-lighting milestone.

### Milestone 11: Expand lighting model

Only after the first path, 2D lighting design, and multi-light design are proven:

- Current shipped state already includes:
  - point lights
  - attenuation
  - emissive material
  - normal maps
- Current shipped state now also includes:
  - specular highlights using `MaterialSpecular` and `MaterialPower`
- Keep remaining shadow work separate and later:
  - shadow maps

## Remaining Work

- shadow-map design and scope freeze
- eventual shadow-map implementation after that design is accepted
- optional editor polish around lighting/material authoring
- optional performance optimizations such as runtime-selected narrower light shader variants
  (`1`/`2`/`3`/`4`-light variants) beyond the current coarse build-time `SUPPORTED_MAX_LIGHTS` cap

## Open Questions

Ask and resolve these before implementation:

1. Should light state be global per `DEVICE`, per `SCENE`, or both?
   Resolved: store active-scene light state in `DEVICE::Impl`, and reset it during
   `CORE_MANAGER::logic()` scene changes before `onInitScene()`.

2. Should light ever become a real `RENDERIZABLE` object?
   Resolved: no for the engine light itself. Add separate editor/debug gizmos later if picking
   or visualization needs them.

3. Should old content with normals become lit automatically?
   Resolved: no. Lighting is opt-in per scene to preserve compatibility.

4. Should custom shaders receive light uniforms automatically?
   Resolved: yes, but only when they declare the reserved names. Existing custom shaders should
   keep working unchanged.

5. Should milestone 1 include more than one light?
   Resolved: no. One ambient plus one directional light first; multi-light starts after one-light
   validation on OpenGL ES, DirectX 9, and Metal.

5a. Should 2D lighting replace the first 3D one-light milestone?
   Resolved: no. Keep the first implementation as 3D one-light Lambert lighting, then build 2D
   lighting on the validated shared infrastructure.

6. Should normal maps use texture stage 1 later?
   Recommended: no. Stage 1 is already occupied by FX. Normal maps need an explicit future texture
   slot/material design.

7. Does the existing material model cover milestone 1?
   Resolved: yes. `Diffuse`, `Ambient`, `Specular`, `Emissive`, and `Power` cover a classic
   material model. PBR material fields should be a separate future design, not part of the first
   lighting pass.

8. Should the first shader include specular?
   Resolved: no. Start with ambient plus Lambert diffuse. Add emissive and then specular after the
   one-light path is validated.

8a. Should the first Lambert pass use mesh material values?
   Resolved: yes. Use `MaterialAmbient` and `MaterialDiffuse` immediately.

8a.1. Should material uniforms be object-level or per-subset?
   Resolved: per-subset where subset material data exists. Use fallback classic material defaults
   when material data is missing: diffuse/ambient white, specular/emissive black, power `0`.

8a.2. How should `MaterialDiffuse` interact with `sample0`?
   Resolved: multiply sampled RGB by `MaterialDiffuse.rgb`, and compute final alpha as sampled
   alpha multiplied by `MaterialDiffuse.a`. White diffuse preserves old texture color.

8a.3. Should ambient use scene ambient only or material ambient too?
   Resolved: use both. Ambient contribution is `AmbientColor.rgb * MaterialAmbient.rgb`.

8a.4. What is the first-pass RGB lighting formula?
   Resolved: use classic Lambert-style modulation:
   `base = sample0.rgb * MaterialDiffuse.rgb`;
   `lit = base * (AmbientColor.rgb * MaterialAmbient.rgb + LightColor.rgb * NdotL)`.

8a.5. Should the first-pass lit RGB be clamped?
   Resolved: yes. Clamp/saturate final RGB to `0.0..1.0` until HDR/tone mapping is explicitly
   designed.

8a.6. Should lighting affect alpha?
   Resolved: no. Lighting affects RGB only. Alpha remains texture/material alpha so transparency
   behavior stays compatible with existing content.

8a.7. Should `AmbientColor.a` or `LightColor.a` affect the first lighting pass?
   Resolved: no. Store alpha for API/type consistency, but milestone 1 lighting uses only `.rgb`.

8b. What are the default enabled-light values?
   Resolved: ambient `(0.2, 0.2, 0.2, 1.0)`, white light `(1.0, 1.0, 1.0, 1.0)`, and normalized
   direction `(0.0, -1.0, -1.0)`.

8b.0. How should the default direction be interpreted?
   Resolved: as light traveling downward and forward in world space. Validation scenes should make
   that direction visibly light normal-facing geometry.

8b.0.1. What mesh asset should validation use?
   Resolved: use a tiny generated/debug mesh first, then validate an imported `.msh` mesh. `.mbm` is
   deprecated and should not be the primary validation asset extension.

8b.1. Should default ambient be full white for compatibility?
   Resolved: no. Keep default ambient at `0.2` because lighting is opt-in and directional shading
   should be visible by default.

8c. Should enabling lighting initialize defaults?
   Resolved: yes. `setLightEnabled(target, true)` should make lighting visibly usable by applying
   default values when the target has not been configured yet.

8d. Should disabling lighting erase configured values?
   Resolved: no. Disabling only sets `enabled=false`; scene transition reset or a future explicit
   reset API clears target state.

8d.1. Should light value setters implicitly enable lighting?
   Resolved: no. `setAmbientLight`, `setDirectionalLightDirection`,
   `setDirectionalLightColor`, and convenience `setDirectionalLight` only configure values. The
   explicit switch remains `setLightEnabled(target, true)`.

8d.2. Should setters mark a light target as configured before enabling?
   Resolved: yes, per field. Any light value setter marks that field as configured, so later
   `setLightEnabled(target, true)` fills only missing defaults and does not overwrite explicit
   values.

8d.3. Should light configuration use one target-level configured flag or per-field flags?
   Resolved: per-field flags. If the user sets only ambient before enabling, the engine should keep
   that ambient and still fill default light color/direction.

8d.4. Should directional light direction and color be set together or separately?
   Resolved: both forms, with clear primitives. Provide `setDirectionalLightDirection(target, ...)`
   and `setDirectionalLightColor(target, ...)` as the field-level setters, plus convenience
   `setDirectionalLight(target, direction, color)` that calls both and marks both fields configured.

8d.5. Should public state/API call the directional light color `directionalColor` or `lightColor`?
   Resolved: `directionalColor`. Public runtime state/API should describe the light type. The
   shader reserved uniform remains `LightColor` for the one-light shader contract.

8d.6. Should public state/API call the direction `directionalDirection` or just `direction`?
   Resolved: `directionalDirection`. The internal state may store `directionalDirectionWorld`, and
   the shader reserved uniform remains `LightDirectionView` after view-space conversion.

8d.7. How should world-space directional light direction become `LightDirectionView`?
   Resolved: transform with only the view matrix rotation, ignore translation, then normalize after
   conversion.

8d.8. What does `LightDirectionView` mean?
   Resolved: it is the direction the light travels in view space. Shader Lambert diffuse uses
   `-LightDirectionView` as the direction from the surface toward the light.

8d.9. What does public `directionalDirection` mean?
   Resolved: same semantic as `LightDirectionView`, but in world space. It is the direction the
   light travels, not the direction from a surface toward the light.

8e. Should there be an explicit reset API?
   Resolved: yes. Add `mbm::resetLight(target)` and Lua `mbm.resetLight(target)`.

8e.1. What exactly should `resetLight(target)` clear?
   Resolved: reset disables lighting, restores default values, and clears all per-field configured
   flags. Use `setLightEnabled(target, false)` when values should be preserved.

8e.2. Should scene transitions disable lighting or fully reset lighting?
   Resolved: fully reset. Scene transitions should clear configured values and flags before
   `onInitScene()` so a new scene never inherits old scene lighting.

8f. Should C++ expose mutable light state?
   Resolved: no for the first pass. Expose a const reference getter and keep mutation through
   explicit setters. Lua getter returns a table copy.

8g. Should directional-light setters normalize direction input?
   Resolved: yes. `setDirectionalLightDirection` and convenience `setDirectionalLight` normalize
   internally and fall back to the default direction for zero-length input.

8h. Should light colors support values above 1.0?
   Resolved: no for the first pass. Clamp light color inputs to `0.0..1.0`.

8i. Are reserved shader names case-sensitive?
   Resolved: yes. Use exact names only; do not add aliases.

9. Should lighting affect `2ds` or `2dw` objects?
   Resolved: yes, but not in the first backend proof. The complete feature must support 2D
   lighting. Prioritize `2dw`; keep `2ds` explicitly opt-in for HUD/UI/editor predictability.

9a. Should `2dw` start with 3D-style directional lighting on flat geometry?
   Resolved: no. The goal for `2dw` is dedicated point/radius lighting with normal maps.

9b. Should `2dw` lighting use per-object multi-light shader loops?
   Resolved: yes. The first multi-light path should keep selection per object rather than use one
   global scene-wide first-`N` light list.

9c. Should the 2D light buffer be a normal `RENDER_2_TEXTURE` object?
   Resolved: no for the first multi-light pass. Extend the current per-object shading path first.
   Revisit broader light-buffer or screen-space composition only after the bounded per-object
   selection model is proven.

9d. What happens when lighting is enabled for objects without normals?
   Resolved: target-specific behavior. In `3d`, no-normal objects remain unlit and the render path
   does not synthesize normals. In `2dw`, the dedicated lighting pipeline may use a default flat
   normal `(0, 0, 1)` when no normal map exists. `2ds` remains explicitly opt-in.

9e. How should `2dw` choose lights when a scene has more lights than the validated shader cap?
   Resolved: per object. Consider only lights whose radius reaches the object, sort candidates by
   distance to the object center, and keep the nearest validated `N`.

10. Should normal maps use texture stage 1?
   Resolved: no. Stage 1 is already occupied by FX. Normal maps need explicit material texture
   slots and may require a new MBM header version.

10a. Should the legacy FX texture become a material texture slot?
   Resolved: no. The old stage-1/`sample1` texture is per-animation shader-effect state, not
   per-subset material state. Name the semantic role `TextureAnimationEffect` and keep material
   slots for surface data such as diffuse, normal, specular, emissive, and mask.

10b. Should new shaders keep public names such as `sample0`, `sample1`, and `sample2`?
   Resolved: no for new engine-generated and built-in shaders. Use semantic names such as
   `TextureDiffuse`, `TextureAnimationEffect`, and `TextureNormal`. Keep `sample0`/`sample1`/
   `sample2` as a legacy compatibility profile for existing custom shaders.

10c. Can a shader mix legacy sample names and semantic texture role names?
   Resolved: no. Detect mixed naming in the same shader source and fail with a clear error. A shader
   must use one texture naming profile only.

10d. Should the first role-based implementation change physical backend slots?
   Resolved: no. Keep the first mapping compatible while centralizing it behind semantic roles:
   `TextureDiffuse` uses slot `0`, `TextureAnimationEffect` uses slot `1`, and `TextureNormal` uses
   slot `2`.

10e. Should shader texture naming profile be explicit or auto-detected?
   Resolved: auto-detect from shader source first. Add an optional CFG declaration such as
   `[shader.ps][textureNaming] = semantic|legacy|none` as a validation contract. If declaration and
   source disagree, fail clearly.

10f. Should arbitrary custom semantic texture roles be allowed?
   Resolved: no. Accept only known engine roles until the engine has ownership, binding, editor, and
   serialization semantics for additional roles.

10g. Which semantic texture roles are known initially?
   Resolved: `TextureDiffuse`, `TextureAnimationEffect`, `TextureNormal`, `TextureSpecular`,
   `TextureEmissive`, and `TextureMask`.

10h. Which semantic texture roles are bindable in the first pass?
   Resolved: bind `TextureDiffuse`, `TextureAnimationEffect`, and `TextureNormal`. Recognize and
   reserve `TextureSpecular`, `TextureEmissive`, and `TextureMask`, but fail if a shader declares
   them before their runtime binding path exists.

10i. What fallback textures should supported semantic roles use?
   Resolved: missing `TextureDiffuse` and `TextureAnimationEffect` bind a white texture. Missing
   `TextureNormal` binds a flat normal texture `(0.5, 0.5, 1.0, 1.0)`.

11. Should the light API use a boolean `is3d` target?
   Resolved: no. Use an explicit target/mode such as `LIGHT_TARGET_3D` in C++ and `'3d'` in Lua,
   leaving room for future `'2dw'` and explicit `'2ds'` support.

12. Should normal maps be a one-off field?
   Resolved: no. Plan generic per-frame-subset material texture slots. Implement normal maps first,
   and reserve room for specular, emissive, and mask texture slots later.

13. Should material texture slots be fixed fields?
   Resolved: no. Store additional material textures as a counted list of typed slots so future slot
   types can be added without another structural redesign.

14. Should loaders fail on unknown material texture slot types?
   Resolved: no for well-formed optional slots. Unknown slot types should be skipped when the record
   includes enough length information. Malformed or truncated slot records should still fail.

15. Where should per-subset material texture slots live in the file?
   Resolved: adjacent to each frame subset descriptor/data. Do not start with a separate global
   texture-slot table.

16. Should normal-map slots store filename only?
   Resolved: no. Material texture slots should match existing primary texture packaging semantics,
   including path-based or embedded/compressed image handling where the current MBM save/load path
   supports it.

17. Should 2D and 3D lighting share one light state?
   Resolved: no. Use separate target states under the same API family, for example `3d` and `2dw`.

17a. Should enabling one target implicitly affect other targets?
   Resolved: no. Light enable, setters, getters, and reset are target-specific. Enabling `3d` must
   not implicitly affect `2dw` or future `2ds`.

17b. How should Lua validate the light target argument?
   Resolved: accept only exact strings `'3d'` and `'2dw'`. Do not use a default target and do not
   accept aliases. Add `'2ds'` only if that target is explicitly implemented later.

17c. Should C++ APIs parse target strings too?
   Resolved: no. C++ uses typed target constants/enums only; Lua bindings own string parsing.

17c.1. Should C++ light APIs use target-first argument order?
   Resolved: yes. Use target-first C++ APIs such as
   `mbm::setLightEnabled(mbm::LIGHT_TARGET_3D, true)` so C++ and Lua stay aligned.

17d. Should `LIGHT_TARGET_2DW` exist before 2D lighting renders?
   Resolved: yes. Define the target/state early for API stability, but keep `2dw` rendering
   disabled or explicitly not implemented until the dedicated 2D lighting milestone.

17e. What happens if code enables `2dw` before 2D lighting rendering is implemented?
   Resolved: accept and store the target state, but log a clear "`2dw` lighting rendering not
   implemented yet" warning/status. Do not fail target validation and do not change Lua setter
   return values for this case.

17f. Should Lua light setters return boolean/status values?
   Resolved: follow existing namespace style. Lua light setters return no value on success and use
   `lua_error_debug` for invalid arguments or invalid target strings. Non-fatal states such as
   accepted-but-not-rendered `2dw` should log clearly rather than changing return shape.

17g. How often should the temporary `2dw` not-implemented warning log?
   Resolved: only once per scene/target, to avoid noisy logs when scripts update light state every
   frame.

17h. Should `getLightState('2dw')` work before 2D rendering exists?
   Resolved: yes. Since `2dw` calls are accepted and stored, getters should return the stored state
   for tools/tests even before dedicated 2D lighting renders.

17i. What should Lua `getLightState(target)` return?
   Resolved: a table copy with `enabled`, `target`, `ambientColor`, `directionalColor`, and
   `directionalDirection`. Keep per-field configured flags internal unless editor tooling later
   needs them.

17j. Which Lua color table shapes should light setters accept?
   Resolved: accept both named-field tables such as `{r=1,g=1,b=1,a=1}` and array-style tables such
   as `{1,1,1,1}`, matching existing Lua binding patterns. Normalize internally to `COLOR`.

17k. Which Lua direction table shapes should light setters accept?
   Resolved: accept both named-field tables such as `{x=0,y=-1,z=-1}` and array-style tables such
   as `{0,-1,-1}`. Normalize internally to `VEC3`, then apply zero-length fallback.

17l. Should Lua setters accept separate numeric arguments too?
   Resolved: yes, for ergonomics. Keep table arguments as the documented primary form, but allow
   target-first numeric forms such as `mbm.setAmbientLight(target, r, g, b, a)` and
   `mbm.setDirectionalLightDirection(target, x, y, z)`.

17m. Should the Lua numeric setter target argument be first or last?
   Resolved: first. Target-first numeric forms are easier to parse and avoid ambiguity with
   optional alpha or future variable-size value shapes.

17n. Should Lua table-form setters also be target-first?
   Resolved: yes. Use target-first argument order for all Lua light functions, including
   `mbm.setLightEnabled(target, enabled)`, `mbm.setAmbientLight(target, value)`,
   `mbm.setDirectionalLightDirection(target, value)`, `mbm.setDirectionalLightColor(target, value)`,
   `mbm.setDirectionalLight(target, direction, color)`, `mbm.resetLight(target)`, and
   `mbm.getLightState(target)`.

18. Should scalar integer shader variables be part of this feature?
   Resolved: yes, unless implementation proves unexpectedly risky. Add scalar `VAR_INT` support as
   a lighting milestone for custom shader controls and to prepare backend integer uniform upload.
   Future engine counters such as `LightCount` are reserved engine uniforms, not CFG variables.
   Keep the first pass scalar-only; defer int vectors until a shader contract needs them.

18a. What CFG syntax should `VAR_INT` use?
   Resolved: follow the existing parser structure. Shader variables are currently declared as
   `[shader][type][name] = min ... max ... default ...`, so scalar integer variables should use
   `[shader][int][name] = min I max I default I`.

18b. Should `VAR_INT` support the same min/max/default animation behavior as floats?
   Resolved: yes. Treat it like the existing scalar shader variable flow, but define integer
   rounding/clamping centrally so all backends, Lua/editor views, and saved animation data agree.

18c. How should animated `VAR_INT` fractional values be converted?
   Resolved: round to the nearest integer first, then clamp to the configured `min..max` range.

19. Should `LightCount` be reserved now?
   Resolved: yes. Reserve `LightCount` as an engine-owned shader name now, but define its real
   runtime behavior in the multi-light milestone. It is not a CFG variable.

19a. What should `LightCount` upload during the one-light milestone?
   Resolved: upload `1` when lighting is enabled for the target and `0` when lighting is disabled.

19b. Should `LightEnabled` be a float or integer reserved uniform?
   Resolved: integer. Upload `1` for enabled and `0` for disabled from the beginning. Do not use a
   shader `bool`; `int` is the clearer cross-backend contract.

19c. Do we need both `LightEnabled` and `LightCount`?
   Resolved: yes. They mirror each other in the one-light milestone, but they have different
   meaning. `LightEnabled` is the explicit compatibility/on-off switch; `LightCount` is the active
   light count for multi-light shaders.

19d. Must custom shaders declare both `LightEnabled` and `LightCount` together?
   Resolved: no. Reserved light/material names are independent and optional. The engine uploads
   whichever reserved names are active in the compiled shader.

19e. Can CFG variables use reserved engine names?
   Resolved: no. Reject those CFG declarations with a clear error; reserved names are engine-owned
   uniforms.

19f. Where should reserved-name CFG rejection happen?
   Resolved: in the low-level shader CFG load/add-variable path, not only in editor/import tooling,
   so all shader entry points follow the same rule.

19g. Should the reserved-name list be duplicated where needed?
   Resolved: no. Use one shared helper/table for reserved shader uniform names so CFG parsing,
   editor/import checks, and backend upload cannot drift.

19h. Where should the reserved-name helper live?
   Resolved: in shared shader core code, such as `shader-reserved-names.h/.cpp` or the existing
   shader variable CFG module, not in a backend-specific implementation.

19i. Should material reserved names be rejected from CFG before every material field is used?
   Resolved: yes. The whole reserved list is protected immediately, including material names that
   later milestones consume.

19j. Can custom shaders declare reserved material uniforms before default shaders use them?
   Resolved: yes. Reserved names are forbidden in CFG, but allowed as actual shader
   uniforms/constants. The engine should look up active reserved names in the compiled shader and
   supply their engine-owned values.

## Validation Checklist

- Linux/OpenGL ES build compiles.
- Windows/DirectX 9 build compiles.
- macOS/Metal build compiles.
- Existing unlit sprite/mesh scenes look unchanged when lighting is disabled.
- Mesh with normals visibly responds to directional light when lighting is enabled.
- Mesh without normals still renders.
- `sample1` FX shaders still work.
- DirectX 9 render-to-texture preview remains stable.
- No unexpected noisy `aNormal optimized out` warning for lit default shaders.
