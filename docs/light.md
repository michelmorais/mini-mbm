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
- later specular highlights

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
- Do not add PBR fields such as metallic, roughness, ambient occlusion, or normal-map material slots.

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

Lua target strings are exact: only `'3d'` and `'2dw'` are accepted for now. The `2dw` state is
accepted and stored, but dedicated 2D lighting rendering is not implemented yet; enabling `2dw`
lighting logs that status once per scene. New scene loading resets all light state.

Default values:

- `enabled = false`
- `ambientColor = (0.2, 0.2, 0.2, 1.0)`
- `directionalColor = (1.0, 1.0, 1.0, 1.0)`
- `directionalDirection = normalized (0.0, -0.707, -0.707)`

Color channels are clamped to `0..1`. Directional light directions are normalized by the engine.

## Intentional Multi-Light Limitation

The future multi-light path is intentionally designed around a validated maximum light count, not
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
- compile/use shader variants that match the validated maximum
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

For the first multi-light implementation, the compiled supported cap is intentionally fixed at `4`
lights on the active backend. This keeps the next shader-array layout explicit while the real
multi-light upload path is still being added.

For the Lua selection query, `objectBoundingAABB` is the full object AABB size, not half extents.
That matches `RENDERIZABLE::getBoundingAABB()`.

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

Built-in default shaders stay unlit unless the active vertex format provides normals. When normals
exist and target lighting is enabled, the default shader applies ambient plus directional diffuse
lighting to RGB using `MaterialDiffuse` and `MaterialAmbient`, and preserves alpha as
texture-alpha times `MaterialDiffuse.a`. Objects without normals keep their previous unlit output
even when scene lighting is enabled.

## Built-in Lit Shader Resources

The engine now exposes two explicit built-in lit pixel shaders on every active backend:

- `lit textured.ps`
- `lit solid.ps`

They use only reserved engine inputs such as `LightEnabled`, `AmbientColor`, `LightDirectionView`,
`LightColor`, `MaterialDiffuse`, and `MaterialAmbient`. Their CFG entries therefore contain no
user-editable variables.

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

Current reserved slot ids are:

- `MATERIAL_TEXTURE_SLOT_NORMAL`
- `MATERIAL_TEXTURE_SLOT_SPECULAR`
- `MATERIAL_TEXTURE_SLOT_EMISSIVE`
- `MATERIAL_TEXTURE_SLOT_MASK`

The slot list is stored adjacent to each subset descriptor. Unknown slot types are skipped by
recorded payload length so older/newer tools can coexist more safely. The render path groundwork is
now in place; normal-map consumption is still a follow-up step.
