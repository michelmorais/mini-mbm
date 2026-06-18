# Mini MBM Light Plan

## Goal

Introduce portable engine lighting that works consistently on DirectX 9, OpenGL ES, and Metal.

The first implementation should prove the complete cross-backend path with the smallest useful
feature set:

- ambient light
- one directional light
- diffuse Lambert lighting from existing mesh normals
- textured and untextured meshes
- no new texture-stage dependency

After that is stable, expand to point lights, specular/material controls, normal maps, editor
support, Lua APIs, and scene serialization.

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

It avoids mixing the first pass with light lists, attenuation, shadow maps, normal maps, or editor
serialization.

### 3. Do not use texture stage 1 for lighting

Recommended: yes.

Stage 1 already means `sample1` for existing shader FX. The lighting contract should use
engine-owned uniforms. If normal maps are added later, introduce explicit material texture slots
or a deliberate stage-extension design instead of overloading the existing stage-1 convention.

### 4. Treat light uniforms as engine-owned, not CFG-owned

Recommended: yes.

CFG variables are user/shader-effect controls. Lighting needs consistent engine state across
default shaders and custom shaders. Reserve a small set of names that the engine uploads when the
compiled shader uses them.

Reserved names:

- `LightEnabled`
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
`SCENE`:

```cpp
mbm::setLightEnabled(true);
mbm::setAmbientLight(...);
mbm::setDirectionalLight(...);
```

Initial Lua API shape mirrors the C++ namespace API and applies to the active script scene:

```lua
mbm.setLightEnabled(true)
mbm.setAmbientLight(...)
mbm.setDirectionalLight(...)
```

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

Add a small active-scene light state to `DEVICE::Impl` without changing the `SCENE` public
signature. The engine already has one live scene at a time, so `mbm::` C++ functions and Lua
`mbm.*` wrappers can update that state. Backend upload reads the state through `DEVICE` during
rendering. Lifetime remains scene-level because `CORE_MANAGER::logic()` resets it during scene
changes; it is storage in `DEVICE`, not a persistent device-global effect.

First-pass state:

```cpp
struct LIGHT_STATE
{
    bool  enabled;
    COLOR ambientColor;
    VEC3  directionalDirectionWorld;
    COLOR directionalColor;
};
```

Rules:

- Directional direction is authored in world space.
- Light colors use normalized floats in the `0.0..1.0` range.
- Backend upload converts it to view space using the active view/model-view convention.
- If lighting is disabled, shaders render exactly as before.
- If lighting is enabled but geometry has no normals, shader output should fall back to unlit
  texture/color instead of failing.
- Material values initially come from the existing material struct, with sensible defaults when
  unavailable.
- `CORE_MANAGER::logic()` should reset/disable the active light state during scene changes before
  the new scene reaches `onInitScene()`, so each scene opts into its own lighting.

## Shader Contract

### OpenGL ES

Default vertex shader with normals should:

- read `aNormal`
- transform it using `mvMatrix`
- pass normalized view-space normal to the pixel shader

Default pixel shader should:

- sample `sample0` when UV exists
- compute `diffuse = max(dot(normalView, -LightDirectionView), 0.0)`
- combine ambient and diffuse
- preserve alpha from the primary texture or material color

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
- Reset/disable light during `CORE_MANAGER::logic()` scene-change flow before `onInitScene()`.
- Do not add light back to `TYPE_CLASS`; the first implementation treats lights as scene state.

### Milestone 2: Material naming cleanup

- Rename `util::MATERIAL_GLES` to a platform-neutral name such as `util::MATERIAL`.
- Preserve the existing material fields and on-disk mesh layout.
- Update mesh loading/saving, Mesh Debug Lua bindings, editor material UI, and docs.
- Treat material fields as reserved shader inputs through `MaterialDiffuse`, `MaterialAmbient`,
  `MaterialSpecular`, `MaterialEmissive`, and `MaterialPower`.

### Milestone 3: Backend uniform plumbing

- Add backend-specific lookup/upload for reserved light names through the existing
  `SHADER::update()` / `BASE_SHADER::update()` path.
- Upload only uniforms/constants that are active in the compiled shader.
- Keep CFG variable upload behavior unchanged.
- Add debug logging for missing optional light uniforms only when useful; avoid noisy warnings for
  shaders that intentionally do not use lighting.

### Milestone 4: Lit default shaders

- Update generated default shaders for normal FVF variants.
- Preserve old unlit output when lighting is disabled.
- Keep no-normal FVF variants unlit.
- Ensure `aNormal` is no longer optimized out when lighting is enabled and the default lit shader
  is selected.

### Milestone 5: Built-in lit shader resources

- Add explicit built-in lit shader entries for each backend if default-generation is not enough.
- Keep CFG variable names separate from reserved engine light names.
- Add examples for textured and untextured lit meshes.

### Milestone 6: One-light platform validation

- Validate one-light rendering on Linux/OpenGL ES.
- Validate one-light rendering on Windows/DirectX 9.
- Validate one-light rendering on macOS/Metal.
- Do not begin multi-light work until the one-light path is verified on all three backend families.

### Milestone 7: Multi-light design

- Add multi-light support only after one-light validation is complete on OpenGL ES, DirectX 9, and
  Metal.
- Decide max light count, reserved array names, shader-loop strategy, DirectX 9 constant limits,
  Metal buffer layout, and fallback behavior before implementation.
- Candidate future names:
  - `LightCount`
  - `LightDirectionView[0]`
  - `LightPositionView[0]`
  - `LightColor[0]`

### Milestone 8: Editor exposure

- Add editor controls after C++ and Lua behavior is validated.
- Likely first UI targets: Scene Editor and Mesh Debug preview.

### Milestone 9: Expand lighting model

Only after the first path and multi-light design are proven:

- point lights
- attenuation
- specular highlights using `MaterialSpecular` and `MaterialPower`
- emissive material
- normal maps
- shadow maps

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

6. Should normal maps use texture stage 1 later?
   Recommended: no. Stage 1 is already occupied by FX. Normal maps need an explicit future texture
   slot/material design.

7. Does the existing material model cover milestone 1?
   Resolved: yes. `Diffuse`, `Ambient`, `Specular`, `Emissive`, and `Power` cover a classic
   material model. PBR material fields should be a separate future design, not part of the first
   lighting pass.

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
