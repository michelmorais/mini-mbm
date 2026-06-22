# Mini MBM Lighting And Materials

This document explains the material model used by Mini MBM lighting and how it differs from
modern physically based rendering.

## Classic Material

Mini MBM currently has a classic material model in mesh data:

```cpp
struct MATERIAL
{
    mbm::COLOR Diffuse;
    mbm::COLOR Ambient;
    mbm::COLOR Specular;
    mbm::COLOR Emissive;
    float      Power;
};
```

Older code may still call this type `MATERIAL_GLES`. That name is misleading because the material
is not OpenGL ES specific; it is engine/file-format data. The engine now uses the platform-neutral
name `MATERIAL` and keeps `MATERIAL_GLES` as a compatibility alias while preserving the binary mesh
layout.

These fields match the classic fixed-function OpenGL/DirectX and Phong/Blinn-Phong material model:

- `Diffuse`: the main visible material color under direct light.
- `Ambient`: the material color multiplied by scene ambient light.
- `Specular`: the color of shiny highlights.
- `Emissive`: color the material emits by itself, independent of lights.
- `Power`: shininess/specular exponent; higher values create tighter highlights.

A classic lighting shader usually combines these terms like this:

```text
finalColor =
    MaterialEmissive
  + AmbientColor * MaterialAmbient
  + LightColor * MaterialDiffuse * max(dot(N, L), 0)
  + LightColor * MaterialSpecular * specularTerm(MaterialPower)
```

The first Mini MBM lighting implementation should use this classic model. It is enough for:

- ambient light
- directional diffuse light
- emissive material contribution
- specular highlights

## Other Material Models

### Unlit

An unlit material ignores lights completely. The object renders its texture or color directly.
Sprites, UI, debug geometry, fonts, and editor overlays often stay unlit.

### Lambert

Lambert lighting is diffuse-only lighting:

```text
diffuse = max(dot(N, L), 0)
```

It uses the surface normal and light direction but has no shiny/specular highlight. This is a good
first step for validating normals and light constants.

### Phong / Blinn-Phong

Phong and Blinn-Phong add specular highlights to diffuse lighting. This is the classic material
model supported by Mini MBM's current material fields.

## PBR Material

PBR means physically based rendering. It is not a built-in material type provided by DirectX 9,
OpenGL ES, or Metal. It is mostly shader math plus material conventions, texture slots, and
lighting/environment data.

Common PBR material fields are:

- `BaseColor` / `Albedo`
- `Metallic`
- `Roughness`
- `AmbientOcclusion`
- `NormalMap`
- `Emissive`

PBR usually also needs:

- tangent and bitangent vertex data for normal maps
- linear/gamma color-space handling
- image-based lighting or reflection probes
- environment maps
- tone mapping or HDR-style render flow for best results

DirectX 9 and OpenGL ES can run shader code that approximates PBR, but they do not provide PBR as a
native feature. DirectX 9 fixed-function lighting maps to the classic material model, not PBR.

## Mini MBM Direction

For the first lighting pass:

- Use the existing classic material fields.
- Keep old unlit behavior when lighting is disabled.
- Add engine-reserved shader names for classic material values:
  - `MaterialDiffuse`
  - `MaterialAmbient`
  - `MaterialSpecular`
  - `MaterialEmissive`
  - `MaterialPower`
- Do not add PBR fields such as metallic, roughness, or ambient occlusion.

PBR should be treated as a separate future rendering/material-system design, not as part of the
first light feature.

## Runtime Light State

Lighting is opt-in and target-specific. The first API skeleton stores independent state for `3d`
and `2dw` without changing the `SCENE` class signature.

C++:

```cpp
mbm::setLightEnabled(mbm::LIGHT_TARGET_3D, true);
mbm::setAmbientLight(mbm::LIGHT_TARGET_3D, mbm::COLOR(0.2f, 0.2f, 0.2f, 1.0f));
mbm::setDirectionalLight(mbm::LIGHT_TARGET_3D,
                         mbm::VEC3(0.0f, -0.707f, -0.707f),
                         mbm::COLOR(1.0f, 1.0f, 1.0f, 1.0f));
mbm::resetLight(mbm::LIGHT_TARGET_3D);
```

Lua:

```lua
mbm.setLightEnabled('3d', true)
mbm.setAmbientLight('3d', {r = 0.2, g = 0.2, b = 0.2, a = 1.0})
mbm.setDirectionalLight('3d',
    {x = 0.0, y = -0.707, z = -0.707},
    {r = 1.0, g = 1.0, b = 1.0, a = 1.0})
local light = mbm.getLightState('3d')
```

Lua target strings are exact: only `'3d'` and `'2dw'` are accepted for now. Both targets now have
engine lighting support. New scene loading resets all light state.

Default values:

- `enabled = false`
- `ambientColor = (0.2, 0.2, 0.2, 1.0)`
- `directionalColor = (1.0, 1.0, 1.0, 1.0)`
- `directionalDirection = normalized (0.0, -0.707, -0.707)`

Color channels are clamped to `0..1`. Directional light directions are normalized by the engine.

## Current Implementation Scope

The current shipped lighting implementation covers:

- `3d` lighting
- `2dw` lighting
- target-specific ambient and directional light state
- per-target point-light lists
- per-object nearest-light selection for `2dw`
- typed per-subset material texture slots, with the normal-map slot consumed at runtime
- default lit/unlit shader classification for engine-generated shaders
- built-in lit pixel shaders
- diffuse, emissive, and specular material contribution

Current intentional exclusions or limitations:

- `2ds` lighting is not implemented
- shadow maps are not implemented
- reserved material texture roles `TextureSpecular`, `TextureEmissive`, and `TextureMask` are known
  but not runtime-bindable yet
- custom shaders receive engine lighting only when they explicitly declare the reserved inputs
- runtime material upload currently comes from the active mesh/subset material path; broader
  per-renderable material ownership is still limited by existing render-path wiring

## Intentional Multi-Light Limitation

The current multi-light path is intentionally designed around a validated maximum light count, not
an unbounded runtime list.

Reason:

- shader arrays need a fixed upper bound
- DirectX 9 constant/register limits are real
- a shared engine contract across OpenGL ES, DirectX 9, and Metal needs a bounded layout

So the engine design should distinguish:

- requested light capacity: what the game asks for
- validated light capacity: what the active backend/profile can support safely
- active light count: how many of those validated slots are currently used at runtime

In practice, the multi-light pipeline should:

- accept a developer-requested max light count
- validate that count against the active backend/profile
- fail clearly when the request exceeds supported limits
- compile against a bounded supported maximum
- upload `LightCount` as the current active runtime count within that validated capacity

This is an intentional limitation, not a temporary weakness. It keeps the shader contract explicit
and avoids pretending that every backend can support an arbitrary number of simultaneous lights.

For `2dw`, the intended selection model is per-object nearest lights, not one global scene-wide
first-`N` list. That means a scene may contain many visible lights overall while each object still
shades against only a small validated subset.

Planned first selection rule:

- consider only lights whose radius reaches the object
- rank candidates by distance to the object center
- keep the nearest validated `N` lights for that object

This is the expected mechanism behind scenes that appear to have many simultaneous 2D lights. The
engine does not need to promise that every sprite uses every light; it needs to choose a bounded
set of relevant lights per object.

Current Milestone 9 groundwork exposes two target-specific configuration values:

- requested max light count
- light selection mode

It also stores a target-specific point-light list that future multi-light selection will read from.

The current implementation slice also exposes backend-cap validation:

- `mbm.setRequestedMaxLights(target, n)` now rejects unsupported values immediately
- `mbm.getSupportedMaxLights(target)` reports the active backend compiled cap
- `mbm.getValidatedMaxLights(target)` reports the currently accepted validated cap
- `mbm.getLightState(target)` now also includes `supportedMaxLights` and `validatedMaxLights`
- `mbm.getSelectedPointLights(target, objectCenter, objectBoundingAABB)` returns the nearest
  validated point lights whose radius reaches that object

For the first multi-light implementation, the compiled supported cap defaults to `4` lights on the
active backend, but it is now a build-time engine setting:

- CMake/Xcode generator builds can pass `-DSUPPORTED_MAX_LIGHTS=1..4`
- the standalone Visual Studio solution can set `MbmSupportedMaxLights=1..4`
- the default requested max also starts from that compiled supported cap

So a game that knows it only needs `1`, `2`, or `3` supported lights may compile the engine with a
smaller fixed cap.

The requested max light count is a per-target runtime selection limit, not a registration limit:

- `mbm.addPointLight(...)` may still register more lights than the requested max
- `mbm.setRequestedMaxLights(target, 2)` means each draw on that target keeps at most the nearest
  validated `2` lights for that object
- the other registered lights remain in the target list and may still affect other objects whose
  nearest-light selection chooses them

So with `per_object_nearest`, requested max `2` means: "for this draw, send only the 2 nearest
relevant lights to the shader and ignore the others for this object".

For the Lua selection query, `objectBoundingAABB` is the full object AABB size, not half extents.
That matches `RENDERIZABLE::getBoundingAABB()`.

The current nearest-light selection algorithm is:

- derive an approximate object radius from the object AABB
- scan the target point-light list
- discard lights whose `light.radius + objectRadius` does not reach the object center
- sort remaining candidates by distance to the object center
- keep only the nearest validated `N` lights for that draw

This means performance cost is primarily driven by:

- how many objects are using a light-capable shader
- how many point lights exist on that target
- the validated max light count

The engine does not maintain a second explicit list of objects that receive light. The shader path
defines that:

- unlit default shader objects do not consume selected lights
- lit default shader objects do
- custom shaders only receive light if they explicitly implement the reserved light inputs

So the nearest-light selection work matters only for draws that actually use a light-capable shader
path.

Today the generated shader source still uses the compiled supported cap, not the requested max. In
practice that means:

- shader arrays are compiled with the backend supported cap
- the shader loop is compiled against that supported cap
- runtime `LightCount` stops the loop after the selected light count for that draw

So lowering the requested max from `4` to `2` reduces the selected/uploaded lights for that draw,
but it does not currently generate a smaller shader variant.

## Reserved Shader Inputs

The engine uploads these reserved light names automatically when the active shader declares them:

- `LightEnabled`: integer, `1` when target lighting is enabled, otherwise `0`
- `LightCount`: integer active-light count for the current draw within the validated capacity
- `AmbientColor`: `vec4` / `float4`
- `LightDirectionView`: `vec3` / `float3`, direction the light travels in view space
- `LightPositionView`: scalar one-light fallback or array base name `LightPositionView[0]`
- `LightRadius`: scalar one-light fallback or array base name `LightRadius[0]`
- `LightColor`: scalar one-light fallback or array base name `LightColor[0]`
- `MaterialDiffuse`: `vec4` / `float4`
- `MaterialAmbient`: `vec4` / `float4`
- `MaterialSpecular`: `vec4` / `float4`
- `MaterialEmissive`: `vec4` / `float4`
- `MaterialPower`: `float`

OpenGL ES and DirectX 9 look up these names directly as optional uniforms/constants. Metal shaders
use fixed optional argument buffer slots with the same argument names:

```metal
constant int    &LightEnabled      [[buffer(4)]]
constant int    &LightCount        [[buffer(5)]]
constant float4 &AmbientColor      [[buffer(6)]]
constant float3 &LightDirectionView [[buffer(7)]]
constant float4 *LightColor        [[buffer(8)]]
constant float4 &MaterialDiffuse   [[buffer(9)]]
constant float4 &MaterialAmbient   [[buffer(10)]]
constant float4 &MaterialSpecular  [[buffer(11)]]
constant float4 &MaterialEmissive  [[buffer(12)]]
constant float  &MaterialPower     [[buffer(13)]]
constant float3 *LightPositionView [[buffer(15)]]
constant float  *LightRadius       [[buffer(16)]]
```

Custom shader CFG variables cannot use these reserved names; they are owned by the engine.
Current runtime upload sources material from the active mesh material. Per-subset material upload
is still a future expansion once that data is threaded through the render path.

## Default Lit Shader Behavior

For renderizables without a custom shader, the built-in default shader is classified at load/compile
time, per animation/FX:

- `default-unlit`
- `default-lit`

Classification rule:

- if target lighting is disabled when that default-shader animation is created, it gets the unlit
  default shader
- if target lighting is enabled when that default-shader animation is created, it gets the light-
  capable default shader

This applies to both `3d` and `2dw`.

Important consequences:

- changing `setLightEnabled(target, ...)` later does not reclassify already-loaded default-shader
  objects
- a scene may intentionally contain a mix of lit-default and unlit-default objects
- runtime light enable/disable still affects the uniforms seen by lit-default shaders, but not
  which shader class was chosen at load time

Custom shaders stay authoritative:

- the engine does not auto-merge lighting into a custom shader
- if a developer wants custom shader behavior plus engine lighting, that shader must explicitly
  provide support for the reserved light inputs

When classified as lit, the default shader uses the reserved material/light values described above.
When classified as unlit, it keeps the cheaper unlit path even if normals or UVs exist.

Current default-lit shading behavior:

- ambient uses `AmbientColor * MaterialAmbient`
- diffuse uses `LightColor * NdotL`
- emissive adds `MaterialEmissive.rgb`
- specular uses `MaterialSpecular.rgb` and `MaterialPower`

The current specular term is a view-space Blinn-Phong style highlight:

- directional `3d` lighting uses the view-space light direction together with the current fragment
  view direction
- `2dw` point-light shading uses the per-light view-space vector together with the current fragment
  view direction
- when `MaterialPower <= 0`, the default lit shaders contribute no specular highlight

`MaterialPower` is the shininess exponent, not a linear strength slider. The current shaders use a
term equivalent to `pow(max(dot(normal, halfDir), 0), MaterialPower)`.

Practical consequence:

- higher `MaterialPower` -> narrower, tighter highlight
- lower `MaterialPower` -> broader, softer highlight

So values above `1` can make the visible highlight look smaller, which is expected. Very low values
such as `0.1` can make larger regions appear white or opaque-looking because the broad specular term
adds across more pixels before the final lit color is clamped to `0..1`.

Use the two material controls with different intent:

- `MaterialPower` controls highlight size/focus
- `MaterialSpecular` controls highlight color/intensity

Reasonable first-pass ranges:

- `1..4`: broad highlight
- `8..16`: medium highlight
- `32+`: tight highlight

## Built-in Lit Shader Resources

The engine now exposes two explicit built-in lit pixel shaders on every active backend:

- `lit textured.ps`
- `lit solid.ps`

They use only reserved engine inputs such as `LightEnabled`, `AmbientColor`, `LightDirectionView`,
`LightColor`, `MaterialDiffuse`, `MaterialAmbient`, `MaterialSpecular`, `MaterialEmissive`, and
`MaterialPower`. Their CFG entries therefore contain no user-editable variables.

Typical usage is to load only the pixel shader and let the engine provide the FVF-matched default
vertex shader automatically:

```lua
local fx = myMesh:getShader()
fx:load("lit textured.ps")
```

Use `lit textured.ps` for normal-capable geometry that also has UVs and a stage-0 texture. Use
`lit solid.ps` for normal-capable geometry that should light from material color only.

These built-ins require a vertex format with normals. They are intended for `FVF_POS_NOR_UV` and
`FVF_POS_NOR` style meshes; objects without normals should keep using the unlit/default path.

## Material Texture Slots

Mesh version `v9` adds typed per-subset material texture slots alongside the existing primary
diffuse texture reference in `HEADER_DESC_SUBSET::nameTexture`.

Mesh version `v10` stores `TextureAnimationEffect` once at the animation FX level instead of
duplicating that path through the legacy PS/VS stage-1 records.

Current reserved slot ids are:

- `MATERIAL_TEXTURE_SLOT_NORMAL`
- `MATERIAL_TEXTURE_SLOT_SPECULAR`
- `MATERIAL_TEXTURE_SLOT_EMISSIVE`
- `MATERIAL_TEXTURE_SLOT_MASK`

The slot list is stored adjacent to each subset descriptor. Unknown slot types are skipped by
recorded payload length so older/newer tools can coexist more safely.

Current runtime binding behavior:

- `TextureDiffuse` stays on slot/register/index `0`
- `TextureAnimationEffect` stays on slot/register/index `1`
- `TextureNormal` uses slot/register/index `2`

Current `2dw` normal-map behavior:

- when a per-subset normal map exists, `2dw` lighting samples it from `TextureNormal`
- when no normal map exists, `2dw` lighting falls back to the flat normal `(0, 0, 1)`
- current `2dw` lighting does not consume stored mesh/vertex normals; the normal-map texture or the
  flat fallback drives the lighting normal

Fallback textures:

- missing `TextureDiffuse` -> white texture
- missing `TextureAnimationEffect` -> white texture
- missing `TextureNormal` -> flat normal texture `(0.5, 0.5, 1.0, 1.0)`
