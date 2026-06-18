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
is not OpenGL ES specific; it is engine/file-format data. The type should be renamed to a
platform-neutral name while preserving the binary mesh layout.

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

