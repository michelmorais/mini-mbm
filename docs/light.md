# Mini MBM Lighting And Materials

This document explains the material model used by Mini MBM lighting and how it differs from
modern physically based rendering.

## Quick Mental Model (Read This First)

Skip this section if you already know Lambert/Blinn-Phong lighting — the rest of the document is
the precise reference. This section is the intuition-first version, for diagnosing whether
something you're seeing is a bug or expected behavior.

### The three ingredients of a lit pixel

Every lit surface point on screen is `ambient + diffuse + specular` added together:

- **Ambient** (`AmbientColor * MaterialAmbient`) — a flat, direction-less fill light. No angle, no
  position, just a multiply. Simulates "bounced light in the room" so shadowed areas aren't pure
  black. If you change nothing else, this is the color a fully-shadowed surface still shows.
- **Diffuse** (Lambertian: `max(dot(Normal, LightDirection), 0)`) — *how directly does this surface
  face the light?* Directly facing the light = 1 (full brightness); facing away = 0 (dark). This is
  the term that makes a sphere look round. It does **not** depend on where the camera is — moving
  only the camera must never change this term. If it does, that's a real bug (this is exactly what
  MBM_VERSION 6.11.0 fixed for 3D: `mvMatrix` wasn't actually in camera space, so diffuse quietly
  changed with camera orbit).
- **Specular** (Blinn-Phong: `pow(max(dot(Normal, HalfVector), 0), MaterialPower)`) — the shiny
  highlight. Unlike diffuse, this **does** depend on the camera (`HalfVector` averages "toward the
  light" and "toward the camera"). A specular highlight sliding around as you orbit the camera is
  correct, not a bug. A low `MaterialPower` (e.g. `1.0`) spreads this into a broad, soft-looking
  highlight that can be mistaken for a lighting-direction problem — see the checklist below.

### Where does the Normal come from?

This explains most surprises people run into:

1. **A real mesh with authored per-vertex normals** (a `.msh`, or a sprite/tile with normals) — the
   normal varies smoothly across the surface based on actual geometry. No surprises, standard
   Lambert/Blinn-Phong applies directly.
2. **A flat 2D sprite with no normal map** — there's no real surface to derive a normal from, so the
   engine used to fake `Normal = (0,0,1)` (dead-on facing the camera) and still ran it through the
   real diffuse dot-product against an in-plane point light. That combination produces meaningless,
   geometry-free artifacts (a tiny sharp "hotspot" instead of a soft glow) purely as an accident of
   whatever Z-offset the light happens to have. Fixed in the same round as the above: this case now
   skips the angle term and lights purely by distance (see `HasNormalMap` in Reserved Shader Inputs).
3. **A flat 2D sprite *with* a normal map** — a texture where pixel colors encode directions instead
   of colors, giving the illusion of bumps on a flat surface (e.g. `src/test-lib/box.spt` +
   `wooden-box_normal.png`). Here the engine samples a *real, different* normal per pixel and runs
   the *real* diffuse math against it. A light placed close and at a shallow angle will then
   legitimately produce a small, sharp, "sparkly" highlight where individual bumps catch the light —
   that is the entire point of a normal map, not a bug. Moving the light farther away, or adjusting
   its assumed "height" above the flat art, smooths this back into a soft glow. Whether a specific
   normal-map/light/height combination "looks right" is an art-tuning question, not an engine one.

   That "height" is the light's `z`. Mini MBM's 2D camera is left-handed: increasing `z` moves
   *away* from the camera (deeper into the screen), and a 2D world sprite sits at/near `z = 0` by
   default. So a light at a negative `z` sits in front of the sprite plane (closer to the camera)
   and one at a positive `z` sits behind it — think of `z` as how high the lamp hangs above the flat
   art, not as a draw-order value. Low/close (small `|z|`) gives the sharp, sparkly look above;
   higher/farther gives a soft, even glow.

Per-vertex normals on a sprite that never uses them for lighting (case 3 skips them entirely,
sampling the texture instead) can be flat-out wrong without ever being visible — this is exactly
what happened with `box.spt`: its own vertex normals were wrong, but nothing ever noticed because
that lighting path doesn't read them.

### Point light falloff (attenuation)

`attenuation = (1 - clamp(dist / radius, 0, 1))²`. Not true inverse-square (which never reaches
exactly zero) — a common game-engine approximation that guarantees the light contributes exactly
zero at `radius`, giving a clean bounded circle of effect instead of an infinite dim tail. `radius`
is "how far this light reaches" as a tuning knob, not a physical unit.

### Diagnostic checklist: is this a bug or expected?

1. **Isolate one light at a time.** With several lights active you're looking at a sum of all of
   them — hard to reason about. Temporarily `clearPointLights` down to one.
2. **Zero out `MaterialPower`** if you can, to kill specular and look at diffuse alone. Specular is
   the *only* term allowed to change when just the camera moves; if a "weird gradient" disappears
   when you zero it, that's what you were looking at.
3. **Check whether the mesh's normals actually point where you think.** Mesh Debug's Normals tree
   node (added in MBM_VERSION 6.12.0) draws every frame-1 vertex normal as a line, color-coded green
   ("OK", agrees with the geometric face normal from triangle winding) or red ("flipped", opposes
   it), with per-vertex and per-subset Flip/Recompute actions. A mesh reported as lighting from the
   "wrong side" (e.g. a floor only lighting up from underneath) usually means its authored normals
   are inverted — flip them there rather than fighting the light direction to compensate.
4. **Check for a normal map before blaming attenuation math.** `grep` the `.spt`/`.msh` for a
   `_normal.png`-style texture reference — a normal map changes the whole character of a point
   light's falloff (see above).
5. **Move only the camera, nothing else.** Diffuse and attenuation must stay pixel-identical; only
   specular is allowed to shift.
6. **When truly unsure, drop to the simplest possible asset**: a plain flat quad, no normal map,
   `MaterialPower = 0`. Confirm a point light looks like a plain soft radial glow there. If that's
   clean, whatever looked "wrong" on a fancier asset is almost always that asset's own material or
   normal data, not the engine.

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
- point-light list (see below) starts empty for both targets
- `requestedMaxLights` starts at the compiled supported cap
- `lightSelectionMode` starts at `per_object_nearest`

Color channels are clamped to `0..1`. Directional light directions are normalized by the engine. A
degenerate (near-zero-length) directional light direction is not an error: it silently falls back to
the default direction instead.

`mbm.resetLight(target)` resets *all* of the above for that target in one call: it disables
lighting, restores the default ambient/directional state, restores `requestedMaxLights` and
`lightSelectionMode` to their defaults, and clears the point-light list. There is also a C++-only
`resetAllLights()` that resets both targets at once; it is not currently exposed to Lua.

### Two Point-Light Storages

There are two independent point-light storages per target, and the naming is easy to misread:

- A single legacy point-light slot (`pointPosition`, `pointRadius`, `pointColor` in `LIGHT_STATE`).
  `mbm.setPointLight`, `mbm.setPointLightPosition`, `mbm.setPointLightRadius`, and
  `mbm.setPointLightColor` all mutate this one slot.
- A growable point-light list (`pointLights3D` / `pointLights2DW`). `mbm.addPointLight(...)` appends
  to this list (`push_back`, no cap enforced at registration time, no index returned). The only way
  to remove entries is `mbm.clearPointLights(target)`, which empties the whole list; there is no
  per-index remove or update.

`mbm.getSelectedPointLights(target, ...)` (see below) reads from the **list** when it is non-empty.
If the list is empty, it falls back to treating the legacy single slot as one synthetic candidate
light — but only if that slot was actually configured at least once (any of `setPointLight`/
`setPointLightPosition`/`setPointLightRadius`/`setPointLightColor` were called for this target).
Before MBM_VERSION 6.11.0's second point-light fix, this fallback fired unconditionally whenever the
list was empty, which meant *any* 3D mesh with lighting enabled and zero point lights configured
would silently pick up `LIGHT_STATE`'s bare struct defaults (`pointPosition=(0,0,128)`, `radius=512`,
white) as a phantom point light nobody asked for — harmless while 3D never consumed point-light
selection at all, but very much not harmless once it started to. So the legacy slot only matters for
selection on targets where `addPointLight` was never called *and* one of the legacy setters was.

## Current Implementation Scope

The current shipped lighting implementation covers:

- `3d` lighting
- `2dw` lighting
- target-specific ambient and directional light state
- per-target point-light lists (`mbm.addPointLight` / `mbm.clearPointLights`), independent from the
  legacy single point-light slot (`mbm.setPointLight` and friends) — see "Two Point-Light Storages"
  above
- per-object nearest-light selection, available on both `3d` and `2dw` targets
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

For the first multi-light implementation, the compiled supported cap defaults to `4` lights, but it
is now a build-time engine setting:

- CMake/Xcode generator builds can pass `-DSUPPORTED_MAX_LIGHTS=1..4`
- the standalone Visual Studio solution can set `MbmSupportedMaxLights=1..4`
- the default requested max also starts from that compiled supported cap
- the value is enforced by a `static_assert` to stay within `1..4`

So a game that knows it only needs `1`, `2`, or `3` supported lights may compile the engine with a
smaller fixed cap.

`getSupportedMaxLightsForActiveBackend()` currently has `#if`/backend scaffolding (DirectX 9, Metal,
OpenGL ES, fallback) but every branch returns the same compiled `SUPPORTED_MAX_LIGHTS` constant today
— there is no real per-backend cap differentiation yet, despite the function name suggesting
backend-specific values. Backend-specific caps are a possible future change, not current behavior.

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
- scan the target point-light list (or the legacy single slot as a fallback candidate when that list
  is empty — see "Two Point-Light Storages" above)
- discard lights whose `light.radius + objectRadius` does not reach the object center
- sort remaining candidates by distance to the object center (ties broken by source index)
- keep only the nearest validated `N` lights for that draw

`mbm.getSelectedPointLights(target, objectCenter, objectBoundingAABB)` returns a 1-indexed Lua array.
Each entry is a flattened table, not nested under a `pointLight` key:

```lua
{
    sourceIndex = 1,             -- 1-based index into the point-light list (converted from the
                                  -- internal 0-based index)
    distanceToObjectCenter = 42.0,
    position = { x = 0, y = 0, z = 0 },
    radius = 300,
    color = { r = 1, g = 0, b = 0, a = 1 },
}
```

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
- `DirectionalColor`: `vec4` / `float4`, the directional light's own color — kept separate from
  `LightColor[]` (below) specifically so a `3d` mesh's directional contribution and its point-light
  contributions don't clobber the same array slot when both are active at once (MBM_VERSION 6.11.0)
- `LightPositionView`: scalar one-light fallback or array base name `LightPositionView[0]`
- `LightRadius`: scalar one-light fallback or array base name `LightRadius[0]`
- `LightColor`: scalar one-light fallback or array base name `LightColor[0]` — for the array form,
  index-0-and-up are the currently *selected point lights* for this draw (see "Reserved Lighting
  Now Combines Directional + Point On `3d`" below), never the directional light
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
constant float4 &DirectionalColor  [[buffer(18)]]
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

### Contract: Classification Happens Once, At Creation Time

This is a real engine contract (verified against `getDefaultShaderModeForRenderizable()` in
`shader-fx.cpp` and its call sites), not just an implementation detail that happens to be true today:

```cpp
DEFAULT_SHADER_MODE getDefaultShaderModeForRenderizable(const RENDERIZABLE *renderizable) noexcept
{
    ...
    LIGHT_STATE lightState;
    if (getLightState(target, lightState) && lightState.enabled)
        return DEFAULT_SHADER_MODE_LIT;
    return DEFAULT_SHADER_MODE_UNLIT;
}
```

This function is called exactly once per default-shader FX build — never per-frame, and never again
later for an object that already has its FX built. The call sites are all one-shot setup paths:

- `ANIMATION_MANAGER::populateAnimationFromHeader()` — once per animation baked into a mesh/sprite/tile
  file, during `load()`/`loadAsync()`
- `ANIMATION_MANAGER::addAnimation()` — when a script adds a new animation programmatically
- the equivalent one-shot FX setup in `gif-view.cpp`, `line-mesh.cpp`, `particle.cpp`,
  `texture-view.cpp`, `render-2-texture.cpp`, `shape-mesh.cpp` (all primitive factories), and
  `steered_particle.cpp`

`setLightEnabled()` itself only flips a bool on `LIGHT_STATE`; it does not walk any list of existing
renderizables, so there is no code path that could retroactively reclassify them even in principle.

Concrete example of the behavior this produces:

```lua
-- lighting for '3d' is OFF here
for i = 1, 5 do
    local m = mesh:new('3d')
    m:load('crate.msh') -- classified default-unlit right now, permanently
end

mbm.setLightEnabled('3d', true)

for i = 1, 3 do
    local m = mesh:new('3d')
    m:load('crate.msh') -- classified default-lit right now, permanently
end

-- The first 5 meshes never react to ambient/directional/point light changes.
-- The last 3 meshes do, for the rest of their lifetime.
```

To make a previously-created object switch classification, it has to go through that same one-shot
build path again — e.g. call `load()` on it again (or add a fresh animation) after changing
`setLightEnabled()` for its target. There is no lighter-weight "just reclassify this object" API.

Custom shaders stay authoritative:

- the engine does not auto-merge lighting into a custom shader
- if a developer wants custom shader behavior plus engine lighting, that shader must explicitly
  provide support for the reserved light inputs

When classified as lit, the default shader uses the reserved material/light values described above.
When classified as unlit, it keeps the cheaper unlit path even if normals or UVs exist.

Canonical GLES2 skeletal meshes use variants of the same default vertex shader. LBS skinning is
applied to the bind position and normal before `mvpMatrix`/`mvMatrix`, so the lighting pipeline still
receives view-space `vPositionView` and `vNormalView` with the same meanings documented here. The
initial compact palette stores only three affine `vec4` values per bone; its normal transform is
therefore valid for rigid motion or uniform scale, not non-uniform scale/shear. Such animation must
be rejected until the renderer carries an inverse-transpose normal strategy. Custom vertex shaders
do not yet participate in canonical skinning and fail explicitly instead of silently drawing REST
pose. A separate rigid-DQS default-shader variant is compiled with two quaternion `vec4` values per
bone, performs per-vertex antipodal alignment and normalized/orthogonalized dual-quaternion blending,
and rotates normals through the blended real quaternion. A mesh instance selects LBS or rigid DQS
before loading; `auto` resolves once to DQS only when the bind and every clip use unit scale, falling
back visibly to LBS otherwise. The resolved choice compiles the matching default shader and uploads
the matching per-instance palette. It cannot be switched after load without rebuilding the instance.

Current default-lit shading behavior:

- ambient uses `AmbientColor * MaterialAmbient`
- diffuse uses `DirectionalColor * NdotL` (directional) plus, for `3d`, any selected point lights'
  `LightColor[i] * NdotL * attenuation` summed in — see below
- emissive adds `MaterialEmissive.rgb`
- specular uses `MaterialSpecular.rgb` and `MaterialPower`

The current specular term is a view-space Blinn-Phong style highlight:

- directional `3d` lighting uses the view-space light direction together with the current fragment
  view direction
- `3d` point lights (see below) each contribute their own specular term the same way, using their
  own view-space light vector
- `2dw` point-light shading uses the per-light view-space vector together with the current fragment
  view direction
- when `MaterialPower <= 0`, the default lit shaders contribute no specular highlight

### Reserved Lighting Now Combines Directional + Point On `3d` (MBM_VERSION 6.11.0)

Before 6.11.0, a `3d` mesh's built-in lit shader only ever evaluated its directional light —
`mbm.addPointLight('3d', ...)` correctly stored the light and `mbm.getSelectedPointLights('3d', ...)`
correctly returned it, but the reserved shader path never asked for a point-light-capable variant for
`3d`, so nearby point lights were entirely invisible on 3D meshes. Fixed by having the `3d` lit
fragment shader run its existing directional term **and** loop over the selected point lights in the
same pass (both accumulate into the same `light`/`specular` totals), instead of only ever picking one
or the other. `DirectionalColor` (see "Reserved Shader Inputs" above) exists specifically so the
directional and point contributions don't share — and clobber — the same `LightColor[0]` array slot.
`2dw` shading is unaffected: it was already point-light-only and stays that way.

This was fixed together with a related bug: the per-object matrix the vertex shader used to build
`vNormalView`/`vPositionView` (uploaded as the `mvMatrix` uniform) was the model matrix only, never
multiplied by the camera's view matrix, even though `LightDirectionView`/`LightPositionView` were
already correctly rotated into true view space. That mismatch made directional lighting appear to
change as the camera orbited a scene, and made 2D point-light attenuation subtly wrong whenever a 2D
camera panned. See `docs/new-backend-instructions.md`'s `mvMatrix` entry for the backend-facing
contract.

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
