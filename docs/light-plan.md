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
  and power fields.
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

Proposed names:

- `mbmLightEnabled`
- `mbmAmbientColor`
- `mbmLightDirectionView`
- `mbmLightColor`
- `mbmMaterialDiffuse`
- `mbmMaterialAmbient`

Keep existing names unchanged:

- `aPosition`
- `aNormal`
- `aTextCoord`
- `mvpMatrix`
- `mvMatrix`
- `sample0`
- `sample1`

### 5. Keep lighting opt-in at first

Recommended: yes.

Default behavior should remain visually compatible when no light is enabled. Enabling lighting
should be explicit from C++ and later Lua/editor APIs. This avoids unexpectedly darkening old games
that have normals in assets but were authored for unlit rendering.

## Proposed Runtime Model

Add a small light state object in core engine code, initially owned by `DEVICE` or a narrow
`LIGHT_MANAGER` reachable from `DEVICE`.

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
- Backend upload converts it to view space using the active view/model-view convention.
- If lighting is disabled, shaders render exactly as before.
- If lighting is enabled but geometry has no normals, shader output should fall back to unlit
  texture/color instead of failing.
- Material values initially come from `MATERIAL_GLES`, with sensible defaults when unavailable.

## Shader Contract

### OpenGL ES

Default vertex shader with normals should:

- read `aNormal`
- transform it using `mvMatrix`
- pass normalized view-space normal to the pixel shader

Default pixel shader should:

- sample `sample0` when UV exists
- compute `diffuse = max(dot(normalView, -mbmLightDirectionView), 0.0)`
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

### Milestone 1: Engine light state and API skeleton

- Add core C++ light state with no rendering behavior change when disabled.
- Add C++ accessors/mutators.
- Decide exact owner: `DEVICE` direct state vs. `LIGHT_MANAGER`.
- Do not add light back to `TYPE_CLASS`; the first implementation treats lights as scene state.

### Milestone 2: Backend uniform plumbing

- Add backend-specific lookup/upload for reserved light names.
- Upload only uniforms/constants that are active in the compiled shader.
- Keep CFG variable upload behavior unchanged.
- Add debug logging for missing optional light uniforms only when useful; avoid noisy warnings for
  shaders that intentionally do not use lighting.

### Milestone 3: Lit default shaders

- Update generated default shaders for normal FVF variants.
- Preserve old unlit output when lighting is disabled.
- Keep no-normal FVF variants unlit.
- Ensure `aNormal` is no longer optimized out when lighting is enabled and the default lit shader
  is selected.

### Milestone 4: Built-in lit shader resources

- Add explicit built-in lit shader entries for each backend if default-generation is not enough.
- Keep CFG variable names separate from reserved engine light names.
- Add examples for textured and untextured lit meshes.

### Milestone 5: Lua and editor exposure

- Add Lua API only after C++ backend behavior is validated.
- Candidate Lua API:
  - `mbm.setLightEnabled(bool)`
  - `mbm.setAmbientLight(r, g, b, a)`
  - `mbm.setDirectionalLight(dx, dy, dz, r, g, b, a)`
  - `mbm.getLightState()`
- Add editor controls later, likely in Scene Editor and Mesh Debug preview.

### Milestone 6: Expand lighting model

Only after the first path is proven:

- multiple directional lights
- point lights
- attenuation
- specular highlights using `MATERIAL_GLES::Specular` and `Power`
- emissive material
- normal maps
- shadow maps

## Open Questions

Ask and resolve these before implementation:

1. Should light state be global per `DEVICE`, per `SCENE`, or both?
   Recommended: per `SCENE` API backed by device upload state, so scene transitions can reset
   lights predictably.

2. Should light ever become a real `RENDERIZABLE` object?
   Recommended: no for the engine light itself. Add separate editor/debug gizmos later if picking
   or visualization needs them.

3. Should old content with normals become lit automatically?
   Recommended: no. Lighting should be opt-in to preserve compatibility.

4. Should custom shaders receive light uniforms automatically?
   Recommended: yes, but only when they declare the reserved names. Existing custom shaders should
   keep working unchanged.

5. Should normal maps use texture stage 1 later?
   Recommended: no. Stage 1 is already occupied by FX. Normal maps need an explicit future texture
   slot/material design.

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
