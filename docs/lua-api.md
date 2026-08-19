# mini-mbm Lua API Reference

This document is the canonical reference for every Lua symbol exposed by the mini-mbm engine.  
Engine version: see `mbm.get("mbm")` at runtime.

---

## Table of Contents

1. [Scene Lifecycle Callbacks](#1-scene-lifecycle-callbacks)
2. [Coordinate Systems](#2-coordinate-systems)
3. [mbm Namespace](#3-mbm-namespace)
4. [Camera Object](#4-camera-object)
5. [Render Types — Constructors](#5-render-types--constructors)
6. [Common Renderizable Methods](#6-common-renderizable-methods)
7. [Render Types — Type-Specific Methods](#7-render-types--type-specific-methods)
8. [vec2 / vec3 Objects](#8-vec2--vec3-objects)
9. [mbm Constants](#9-mbm-constants)
10. [Plugin: box2d](#10-plugin-box2d)
11. [Plugin: box2dLiquidFun](#11-plugin-box2dliquidfun)
12. [Plugin: ImGui](#12-plugin-imgui)
13. [Plugin: lsqlite3](#13-plugin-lsqlite3)
14. [print() extensions](#14-print-extensions)

---

## 1. Scene Lifecycle Callbacks

The engine calls these global functions in your Lua script. Define the ones you need.

| Function | When Called | Notes |
|---|---|---|
| `onInitScene()` | Once when the scene first loads | Load assets, set camera, set up state here |
| `onLoop(delta)` | Every frame | `delta` = seconds since last frame (float). Main game loop. |
| `onTouchDown(key, x, y)` | Mouse button press / touch begin | `key` = button id; `x,y` in screen pixels |
| `onTouchUp(key, x, y)` | Mouse button release / touch end | Same coords as `onTouchDown` |
| `onTouchMove(key, x, y)` | Mouse cursor / finger move | Called while touch is held |
| `onTouchZoom(zoom)` | Mouse scroll wheel | `zoom` = +1 or -1 |
| `onKeyDown(key)` | Keyboard key pressed | `key` is an integer — use `mbm.getKeyCode("name")` |
| `onKeyUp(key)` | Keyboard key released | Same key codes as `onKeyDown` |
| `onKeyDownJoystick(player, key)` | Gamepad button pressed | `player` = 0-based player index |
| `onKeyUpJoystick(player, key)` | Gamepad button released | |
| `onMoveJoystick(player, lx, ly, rx, ry)` | Gamepad analog stick | Values in [-1, 1] |
| `onInfoDeviceJoystick(player, maxBtn, name, extra)` | Gamepad connected | `name` = device name string |

---

## 1b. Method Syntax: `:` vs `.`

Lua distinguishes between **method calls** (`:`) and **property access** (`.`):

```lua
-- Method call (`:`) — passes the object as implicit first argument:
obj:setPos(100, 200)     -- correct
obj:destroy()            -- correct

-- Property access (`.`) — reads or writes a field directly:
obj.x = 100              -- correct
obj.visible = false      -- correct

-- Common mistake: calling a method with `.` instead of `:`:
obj.setPos(100, 200)     -- WRONG — self is not passed
```

Rule of thumb: if the operation *does something*, use `:`; if you're *reading or writing a value*, use `.`.

---

## 2. Coordinate Systems

Every render object constructor takes a coordinate-system string as its first argument.

| String | System | Description |
|---|---|---|
| `"2dw"` | 2D World | Default. Origin typically at screen center. Camera-affected. |
| `"2ds"` | 2D Screen | Pixel-space. Origin at top-left. Ignores camera movement. Good for HUD. |
| `"3d"` | 3D World | Full perspective 3D space. |

**Conversion helpers:**

```lua
wx, wy       = mbm.to2dw(sx, sy)        -- screen pixels → 2D world coords
sx, sy       = mbm.to2ds(wx, wy)        -- 2D world coords → screen pixels
wx, wy, wz   = mbm.to3d(sx, sy, depth)  -- screen pixels → 3D world coords at given depth
ox,oy,oz,
dx,dy,dz     = mbm.getPickRay(sx, sy)   -- screen pixels → 3D pick ray (origin + unit direction)
```

**`mbm.to3d`'s `depth` is a literal distance traveled from the camera along the ray through
that screen pixel — NOT a target world Z coordinate.** It's easy to assume otherwise (nothing
in the name suggests it), but the underlying C++ makes it explicit: `depth=0` always returns
the camera's own position, for *any* screen pixel — confirmed empirically (two different pixels,
same `depth=0`, identical result) and in source: `DEVICE::transformeScreen2dToWorld3d_scaled`
(`device-common.cpp`) takes this parameter as `howFarZFromCamera` and computes
`out = rayOrigin + rayDir * howFarZFromCamera`, where `rayOrigin`/`rayDir` come from
`DEVICE::rayCast(sx, sy, ...)` — a real screen-ray reconstruction, camera-relative by
construction. Note `rayCast`'s own internal `rayDir` is **not** normalized (its magnitude grows
for pixels away from screen center, an artifact of how it falls out of the projection-matrix
math) — `mbm.to3d`'s `howFarZFromCamera` is therefore a scaling factor along a variable-length
vector, not a literal real-world distance except exactly at screen center. `mbm.getPickRay`
(below) normalizes this before returning it, specifically so it doesn't carry this gotcha into
new code. To reconstruct a full ray from `mbm.to3d` alone (e.g. to intersect a known plane),
sample at two different `depth` values and use their difference as the direction — see
`screenToWorldOnLayerPlane` in `editor/scene_editor3d.lua` for a worked example (still valid:
the *direction* it derives is correct even though neither sample point is a literal-distance
position on its own).

**`mbm.getPickRay(sx, sy)` returns a proper 3D pick ray for the given screen pixel: `ox,oy,oz`**
(the ray origin — always the camera's own current world position, for any pixel) **and
`dx,dy,dz`** (a normalized/unit direction). Wraps the engine's internal `DEVICE::rayCast`
(`device-common.cpp`), previously used only internally to back `mbm.to3d`. Use this instead of
guessing a `depth` for `mbm.to3d` when you actually need real ray-vs-object math (e.g. testing
against a known bounding box) — `obj:collide(x, y)` (§6.4) now does exactly this internally.
`DEVICE::rayCast` applies the `camera.scaleScreen2d` correction internally (since MBM_VERSION
6.32.0), so `mbm.getPickRay`, `mbm.to3d`, and `obj:collide`'s 3D ray/AABB path all agree on the
same screen point even when a game opts into design-resolution scaling via `-ew`/`-eh` — they
previously diverged whenever `scaleScreen2d != 1.0` (see `docs/future_investigation.md`).

---

## 3. mbm Namespace

All engine-level functions live in the global `mbm` table.

**Every color value anywhere in this API — `mbm.setColor`, `obj:setColor`, `line:setColor`,
the lighting functions (§3.16), `mbm.colorDialog`, and ImGui's `ColorEdit`/`ColorPicker`
widgets (§12) — is 0.0-1.0 per channel, never 0-255.** Several of these were previously
mis-documented as 0-255 (an easy assumption to carry over from other engines/APIs); values
outside `[0,1]` don't error, they silently clamp or saturate, so the mistake shows up as
"my light/tint/background is stuck white or black" rather than a crash.

### 3.1 Scene & Control

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.quit` | `()` | — | Exit the application |
| `mbm.pause` | `()` | — | Pause engine time (`delta=0`) and optionally audio; scene logic and rendering continue |
| `mbm.resume` | `()` | — | Resume a paused engine |
| `mbm.loadScene` | `(name: string)` | — | Load a new Lua scene file (relative path) |
| `mbm.getSceneName` | `()` | string | Name of the current scene file |
| `mbm.getFps` | `(real?: bool)` | number | Current FPS. `real=true` returns unsmoothed value. |
| `mbm.getTimeRun` | `()` | number | Seconds elapsed since engine started |
| `mbm.setFakeFps` | `(cycles: int, fps: int)` | — | Force a fixed FPS for deterministic testing |
| `mbm.onErrorStop` | `(stop: bool)` | — | If `true`, halt script on first Lua error |
| `mbm.pauseAudioOnPauseGame` | `(pause: bool)` | — | Whether to pause audio when `mbm.pause()` is called |
| `mbm.doCommands` | `(cmd: string, param: string)` | — | Send a native command to the platform layer |

### 3.2 Display & Rendering

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.getRealSizeScreen` | `()` | w, h | Actual window/framebuffer size in pixels |
| `mbm.getSizeScreen` | `()` | w, h | Logical/scaled back-buffer size |
| `mbm.getDisplayMetrics` | `()` | table | DPI and density info from the OS |
| `mbm.setColor` | `(r, g, b)` | — | Background clear color (0.0–1.0 each channel, not 0-255) |
| `mbm.enableClearScreen` | `(enable: bool)` | — | Toggle clearing the back-buffer each frame |
| `mbm.refresh` | `()` | — | Force a window redraw / resize event |
| `mbm.getObjectsRendered` | `(type?: string)` | number | Count of rendered objects; filter by type name |
| `mbm.enableTextureFilter` | `(enable: bool)` | — | Enable/disable bilinear texture filtering |

### 3.3 Camera

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.getCamera` | `(type: "2d"\|"3d")` | camera | Get the 2D or 3D camera object (see §4) |

### 3.4 Asset Paths

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.addPath` | `(path: string)` | — | Add a directory to the asset-search path list |
| `mbm.getPathEngine` | `()` | string | Engine installation directory |
| `mbm.getFullPath` | `(file: string)` | string\|nil | Resolve `file` against all search paths |
| `mbm.getAllPaths` | `()` | table | Array of all registered search-path strings |
| `mbm.include` | `(script: string)` | — | Execute another Lua file (searched via paths) |

### 3.5 File System

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.existFile` | `(name: string)` | bool, string\|nil | Whether file exists; second return is full path |
| `mbm.listFiles` | `(path: string, recursive?: bool)` | table | Directory listing. Returns `{separator, {path, file1, ...}, ...}` |
| `mbm.openFile` | `(title: string, filter: string)` | string\|nil | Show native open-file dialog |
| `mbm.openMultiFile` | `(title: string, filter: string)` | table\|nil | Show native multi-file open dialog |
| `mbm.saveFile` | `(title: string, filter: string)` | string\|nil | Show native save-file dialog |
| `mbm.openFolder` | `(prompt?: string, default?: string)` | string\|nil | Show native folder picker dialog |
| `mbm.compress` | `(fileIn: string, fileOut?: string, level?: int)` | bool | ZIP-compress a file (miniz) |
| `mbm.decompress` | `(fileIn: string, fileOut?: string)` | bool | ZIP-decompress a file |

### 3.6 Input

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.getKeyCode` | `(name: string)` | int | Convert a key name to its integer key code |
| `mbm.getKeyName` | `(code: int)` | string | Convert an integer key code to its name |
| `mbm.isCapitalKeyOn` | `()` | bool | Whether Caps Lock is active |
| `mbm.addOnTouch` | `(renderizable, callback)` | — | Register a per-object touch handler |

**Common key names (pass to `mbm.getKeyCode`):**  
`"ESC"`, `"ENTER"`, `"SPACE"`, `"BACKSPACE"`, `"TAB"`,  
`"LEFT"`, `"RIGHT"`, `"UP"`, `"DOWN"`,  
`"CTRL"`, `"ALT"`, `"SHIFT"`,  
`"A"` … `"Z"`, `"0"` … `"9"`,  
`"F1"` … `"F12"`,  
`"control"`, `"alt"`, `"shift"` (aliases accepted case-insensitively)

### 3.7 Coordinate Transforms

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.to2dw` | `(sx, sy)` | wx, wy | Screen px → 2D world coords |
| `mbm.to2ds` | `(wx, wy)` | sx, sy | 2D world coords → screen px |
| `mbm.to3d` | `(sx, sy, depth)` | wx, wy, wz | Screen px → 3D world coords at given camera-relative depth (see §2's note) |
| `mbm.getPickRay` | `(sx, sy)` | ox,oy,oz, dx,dy,dz | Screen px → 3D pick ray: origin (camera position) + normalized direction (see §2) |

### 3.8 Textures

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.createTexture` | `(pixels: table, w, h, channels, name?, savePath?)` | string\|nil | Create a texture from a raw pixel table (RGB or RGBA) |
| `mbm.existTexture` | `(name: string)` | bool | Whether a named texture is already loaded |
| `mbm.loadTexture` | `(file: string, alpha?: bool)` | textureInfo | Load a texture file and return info table |

### 3.9 Global Variables (cross-scene storage)

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.setGlobal` | `(name: string, value: any)` | — | Store a value that persists across scene loads |
| `mbm.getGlobal` | `(name: string)` | any | Retrieve a stored global value |
| `mbm.clearGlobals` | `()` | — | Clear all stored globals (except engine internals) |

### 3.10 Shaders

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.getShaderList` | `(detail?, filter?, min?, max?, code?)` | table | List registered shaders. All params optional booleans/strings. |
| `mbm.existShader` | `(name: string)` | bool | Whether a shader file is registered |
| `mbm.addShader` | `(shaderTable)` | bool | Register a new inline shader at runtime |
| `mbm.getParticleShaderCode` | `()` | string\|nil | Get the built-in particle pixel shader GLSL source |
| `mbm.sortShader` | `()` | — | Sort the shader list |

**`mbm.addShader` table format:**
```lua
mbm.addShader({
    name = "my_effect.ps",         -- must end in .ps (pixel) or .vs (vertex)
    code = "void main() { ... }",  -- GLSL source
    var  = { myUniform = {1.0, 0.0, 0.0} },  -- default values
    min  = { myUniform = {0.0, 0.0, 0.0} },  -- optional
    max  = { myUniform = {1.0, 1.0, 1.0} },  -- optional
})
```

### 3.11 Encryption & Security

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.encrypt` | `(fileIn, fileOut?, password?, iv?)` | bool | AES-encrypt a file |
| `mbm.decrypt` | `(fileIn, fileOut?, password?, iv?)` | bool | AES-decrypt a file |
| `mbm.shuffle` | `(msg: string, key: string)` | string | Vigenère-encrypt a string |
| `mbm.undoShuffle` | `(msg: string, key: string)` | string | Vigenère-decrypt a string |

### 3.12 Dialogs

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.messageBox` | `(title, msg)` | — | Native message dialog |
| `mbm.inputBox` | `(title, default?)` | string\|nil | Native text input dialog |
| `mbm.inputPassword` | `(title)` | string\|nil | Native password input (masked) |
| `mbm.colorDialog` | `(r?, g?, b?)` | r, g, b \| nil | Native color picker; optional default color and the returned r,g,b are all 0.0-1.0, not 0-255 |

### 3.13 System Info

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.is` | `(platform: string)` | bool | Platform check: `"Windows"`, `"Linux"`, `"MacOs"`, `"Android"`, `"iOS"` |
| `mbm.get` | `(what: string)` | string\|bool | Query engine info (see below) |
| `mbm.getIdiom` | `()` | string | OS language/idiom (`"en"`, `"pt_br"`, etc.) |
| `mbm.getUserName` | `()` | string | OS username of the current user |
| `mbm.showConsole` | `(show: bool)` | — | Show/hide the debug console window (Windows) |
| `mbm.getAzimute` | `()` | number | Device compass azimuth angle |

**`mbm.get(what)` accepted values:**  
`"version"` (full version string), `"mbm"`, `"lua"`, `"audio"`, `"opengl"`, `"directx"`,  
`"backend_engine"` / `"engine"`, `"exe"`, `"debug"`,  
`"USE_VR"`, `"USE_OPENGL_ES"`, `"USE_DIRECTX9"`, `"USE_METAL"`,  
`"USE_EDITOR_FEATURES"`,   
`"windows"`, `"linux"`, `"macos"`, `"android"`, `"ios"` (returns bool like `mbm.is`)

### 3.14 Plugins

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.subscribe` | `(pluginUserdata)` | int | Subscribe a plugin to engine callbacks; returns plugin index |

### 3.15 Misc

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.getSplash` | `()` | renderizable\|nil | Get the splash-screen renderizable (if active) |
| `mbm.executeInThread` | `(func)` | — | Execute a Lua function in a background thread (not Android) |
| `mbm.generateImageResourceHeaderFromPng` | `(pngFile, headerFile)` | bool | Convert PNG to a C++ `static-resource` header |
| `mbm.setMinMaxWindowSize` | `(minX, minY, maxX, maxY)` | — | Set window size constraints |

### 3.16 Lighting

Every function takes a `target` string identifying which render space the light state applies
to: `"3d"` or `"2dw"` (2D-screen space `"2ds"` has no lighting). Color/position/direction
arguments accept either a table (`{r,g,b,*a}` / `{x,y,z}`) or the components expanded inline —
both forms are shown below. Alpha (`a`) always defaults to `1.0` when omitted. All the setters
below return nothing on success and **raise a Lua error** (catchable with `pcall`) if `target`
or the arguments are invalid — they do not return `false`.

All colors accepted/returned by the functions below (ambient, directional, point-light colors)
are **0.0-1.0 per channel**, not 0-255 — values are clamped to `[0,1]` internally
(`device-common.cpp`'s `clampLightChannel`), so passing e.g. `255` silently saturates to `1.0`
rather than erroring.

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.setLightEnabled` | `(target, enabled: bool)` | — (errors on failure) | Enable/disable lighting for `target` |
| `mbm.resetLight` | `(target)` | — (errors on failure) | Reset `target`'s lighting (ambient/directional/point) to engine defaults |
| `mbm.setAmbientLight` | `(target, {r,g,b,*a})` or `(target, r,g,b,*a)` | — (errors on failure) | Set the ambient light color |
| `mbm.setDirectionalLight` | `(target, direction, color)` or `(target, x,y,z, r,g,b,*a)` | — (errors on failure) | Set directional light direction + color together |
| `mbm.setDirectionalLightDirection` | `(target, direction)` or `(target, x,y,z)` | — (errors on failure) | Set only the directional light's direction |
| `mbm.setDirectionalLightColor` | `(target, color)` or `(target, r,g,b,*a)` | — (errors on failure) | Set only the directional light's color |
| `mbm.setPointLight` | `(target, position, radius, color)` or `(target, x,y,z, radius, r,g,b,*a)` | — (errors on failure) | Set the (single, legacy) point light position/radius/color together |
| `mbm.setPointLightPosition` | `(target, position)` or `(target, x,y,z)` | — (errors on failure) | Set only the point light's position |
| `mbm.setPointLightRadius` | `(target, radius)` | — (errors on failure) | Set only the point light's radius |
| `mbm.setPointLightColor` | `(target, color)` or `(target, r,g,b,*a)` | — (errors on failure) | Set only the point light's color |
| `mbm.addPointLight` | `(target, position, radius, color)` or `(target, x,y,z, radius, r,g,b,*a)` | — (errors on failure) | Add a point light to `target`'s multi-light list (up to `getSupportedMaxLights`) |
| `mbm.clearPointLights` | `(target)` | — (errors on failure) | Remove every point light previously added via `addPointLight` |
| `mbm.setRequestedMaxLights` | `(target, requestedMaxLights: int)` | — (errors on failure) | Request how many point lights the shader should support; errors if it exceeds `getSupportedMaxLights` for the current backend |
| `mbm.getSupportedMaxLights` | `(target)` | int | Max point lights the current graphics backend can support for `target` |
| `mbm.getValidatedMaxLights` | `(target)` | int | The actually-validated max lights currently in effect for `target` |
| `mbm.setLightSelectionMode` | `(target, mode: string)` | — (errors on failure) | Set how per-object point lights are chosen when there are more lights than `getValidatedMaxLights`. Only `"per_object_nearest"` exists today |
| `mbm.getSelectedPointLights` | `(target, objectCenter, objectBoundingAABB)` | table | For one object at `objectCenter` with half-extents `objectBoundingAABB` (both `{x,y,z}`), returns the array (1-indexed) of point lights actually selected for it: `{sourceIndex, distanceToObjectCenter, position={x,y,z}, radius, color={r,g,b,a}}` per entry. `sourceIndex` is the 1-based index into the list `addPointLight` built |
| `mbm.getLightState` | `(target)` | table | Full lighting snapshot: `{enabled, target, requestedMaxLights, supportedMaxLights, validatedMaxLights, lightSelectionMode, ambientColor={r,g,b,a}, directionalColor={r,g,b,a}, directionalDirection={x,y,z}, pointColor={r,g,b,a}, pointPosition={x,y,z}, pointRadius, pointLights={...}}` |

```lua
-- Typical setup: ambient + directional "sun" light, plus a couple of point lights
-- (colors are 0.0-1.0 per channel, not 0-255 -- see note above)
mbm.setLightEnabled("3d", true)
mbm.setAmbientLight("3d", 0.12, 0.12, 0.16)
mbm.setDirectionalLight("3d", 0, -1, 0.3, 1.0, 0.98, 0.9)

mbm.setRequestedMaxLights("3d", 4)
mbm.addPointLight("3d", 100, 50, 0, 300, 1.0, 0.47, 0)   -- x,y,z, radius, r,g,b
mbm.addPointLight("3d", -200, 80, 50, 250, 0.31, 0.47, 1.0)

function onLoop(delta)
    -- Ask which of the point lights actually affect this specific object, e.g. for a
    -- forward-lit custom shader that only supports a handful of lights per draw call.
    local selected = mbm.getSelectedPointLights("3d", {x=player.x, y=player.y, z=player.z}, {x=50,y=50,z=50})
    for _, light in ipairs(selected) do
        -- light.position, light.radius, light.color, light.distanceToObjectCenter
    end
end
```

---

## 4. Camera Object

Obtained via `local cam = mbm.getCamera("2d")` or `mbm.getCamera("3d")`.

### Properties (read/write)

| Property | Type | Description |
|---|---|---|
| `cam.x`, `cam.y`, `cam.z` | number | Camera position in world space |
| `cam.fx`, `cam.fy`, `cam.fz` | number | Camera look-at / focus point |
| `cam.ux`, `cam.uy`, `cam.uz` | number | Camera up vector |

### Methods

| Method | Signature | Returns | Description |
|---|---|---|---|
| `cam:setPos` | `(x, y, z?)` | — | Set camera position |
| `cam:getPos` | `()` | vec3 | Get camera position |
| `cam:setFocus` | `(x, y, z?)` | — | Set look-at target |
| `cam:getFocus` | `()` | vec3 | Get look-at target |
| `cam:setUp` | `(x, y, z?)` | — | Set up vector |
| `cam:getUp` | `()` | vec3 | Get up vector |
| `cam:setAngleOfView` | `(degrees)` | — | 3D camera only. Field-of-view angle |
| `cam:setFar` | `(distance)` | — | 3D camera only. Far clip plane distance — objects beyond this are culled. **Default is only 1000**, easy to exceed in a normal 3D scene (was entirely undocumented before) |
| `cam:setNear` | `(distance)` | — | 3D camera only. Near clip plane distance |

`setFar`/`setNear`/`setAngleOfView` exist only on the 3D camera (`mbm.getCamera("3d")`) — the 2D
camera has no equivalent (it has no perspective/clipping planes). There is currently no `getFar`/
`getNear`/`getAngleOfView` getter exposed to Lua.

---

## 5. Render Types — Constructors

All constructors follow the pattern: `TypeName:new(coordType, x?, y?, z?)`.  
`coordType` is `"2dw"`, `"2ds"`, or `"3d"` (see §2).

| Global Name | Coord Types | Asset Format | Description |
|---|---|---|---|
| `sprite` | `2dw`, `2ds`, `3d` | `.spt` | Animated 2D/3D sprite |
| `mesh` | `3d` | `.msh` | 3D mesh |
| `texture` | `2dw`, `2ds`, `3d` | PNG, JPG, BMP, etc. | Plain textured quad |
| `gif` | `2dw`, `2ds` | `.gif` | Animated GIF |
| `backGround` | `2dw`, `3d` | various | Scrolling background or 3D backdrop |
| `font` | — | `.fnt` (pre-parsed binary) or `.ttf`/`.otf`/`.true-font` (runtime parsed) | Font renderer (see §7.4) |
| `particle` | `2dw`, `2ds`, `3d` | `.ptl` config file or configure manually | Particle emitter |
| `shape` | `2dw`, `2ds`, `3d` | procedural | Procedurally generated mesh |
| `line` | `2dw`, `2ds`, `3d` | procedural | Line-based geometry |
| `tile` | `2dw` | tile-map file | Tile map with layers |
| `render2texture` | — | — | Off-screen render target |
| `vec2` | — | — | 2D vector math object |
| `vec3` | — | — | 3D vector math object |

**Examples:**
```lua
-- 2D world sprite at origin
local player = sprite:new("2dw")
player:load("hero.spt")

-- HUD texture in screen space
local hudBg = texture:new("2ds", 20, 20)
hudBg:load("hud_bg.png")

-- 3D mesh
local cube = mesh:new("3d", 0, 0, -500)
cube:load("cube.msh")

-- Particle emitter
local sparks = particle:new("2dw", 0, 0, 0)
sparks:load("sparks.ptl")

-- Procedural rectangle shape
local box = shape:new("2dw", 100, 200)
box:create({0,0, 0,50, 50,50, 50,0}, {1,2,3, 1,3,4}, {0,0, 0,1, 1,1, 1,0})
```

---

## 6. Common Renderizable Methods

Every render object (sprite, mesh, texture, gif, backGround, particle, shape, line, tile, render2texture) inherits these methods.

### 6.1 Transform Methods

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:setPos` | `(x, y, z?)` or `(otherObj)` | — | Set world position. Can copy from another object. |
| `obj:getPos` | `()` | vec3 | Get world position |
| `obj:setAngle` | `(ax, ay?, az?)` or `(otherObj)` | — | Set rotation in radians |
| `obj:getAngle` | `()` | vec3 | Get rotation in radians |
| `obj:setScale` | `(sx, sy?, sz?)` or `(otherObj)` | — | Set scale (1.0 = original size) |
| `obj:getScale` | `()` | vec3 | Get scale |
| `obj:move` | `(vx, vy, vz?)` | — | Move relative to current position (delta-scaled) |
| `obj:rotate` | `(axis: string, radians)` | — | Rotate on axis `"x"`, `"y"`, or `"z"` (delta-scaled) |

> **Note:** `obj:move` and `obj:rotate` are automatically multiplied by `device.delta` so they are frame-rate independent.

### 6.2 Direct Property Access (no method call needed)

```lua
obj.x   = 100      -- position X
obj.y   = 200      -- position Y
obj.z   = 0        -- position Z
obj.sx  = 2.0      -- scale X
obj.sy  = 2.0      -- scale Y
obj.sz  = 1.0      -- scale Z
obj.ax  = 0        -- angle X (radians)
obj.ay  = 0        -- angle Y
obj.az  = 3.14     -- angle Z (radians)

obj.visible      = false  -- hide the object (default true)
obj.alwaysRender = true   -- render even when off-screen / outside frustum
obj.alwaysOnTop = true    -- render in the category's overlay sub-pass
obj.alwaysOnTopPriority = 1 -- higher populated layers appear above lower ones (default 0)
```

`alwaysOnTopPriority` only affects objects whose `alwaysOnTop` property is true. Priorities render
in ascending order, with depth cleared between populated layers. Objects at the same priority retain
ordinary depth behavior against each other. The property does not affect render-to-texture captures.

### 6.3 Size & Bounds

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:getSize` | `(considerScale?: bool)` | w, h [, d] | Object dimensions. 3D returns depth too. |
| `obj:getAABB` | `(update?: bool)` | w, h [, d] | Axis-aligned bounding box **size only** — does NOT imply the box is centered at `getPosition()`. Pass `true` to force recalc. |
| `obj:getAABBCenter` | `(update?: bool)` | x, y [, z] | The AABB's **true** geometric center in world space (since MBM_VERSION 6.9.0). Equals `getPosition()` when the object's geometry is centered on its own pivot (the common case); differs for anything anchored elsewhere (a mesh pivoted at its base/floor, a character pivoted at its feet, `font` text whose origin is alignment-driven, not centered). Pass `true` to force recalc. |
| `obj:isOnScreen` | `()` | bool | Whether the object is visible within the camera frustum |
| `obj:isLoaded` | `()` | bool | Whether the asset is fully loaded |

### 6.4 Collision

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:collide` | `(other, useAABB?: bool)` or `(x, y, useAABB?: bool)` | bool | AABB collision vs another object, or vs screen point. `useAABB` defaults to `true` (use `getAABB`); pass `false` to compare raw `getWidthHeight` dimensions instead. |
| `obj:isOver` | `(x, y)` | bool | Is screen point (x, y) inside the object's bounding rect |

**The `(x, y, useAABB)` 3-argument form was silently broken until MBM_VERSION 6.8.0** —
`onCheckCollisionBoundingBoxRenderizable` (`common-methods-lua.cpp`) only entered that code path
when Lua's argument count was exactly 3 (`self, x, y`), so passing a 4th `useAABB` argument
(`self, x, y, useAABB` — 4 args) missed that branch entirely and fell through to a generic
`luaL_error`, even though the code inside was clearly written to read a 4th argument. Fixed by
accepting both 3 and 4 argument counts here; if you need this form, upgrade past 6.8.0.

**`obj:collide(x, y)` on a 3D object does a real ray/AABB test** (`common-methods-lua.cpp`,
`onCheckCollisionBoundingBoxRenderizable`, since MBM_VERSION 6.8.0): it casts a proper
screen-space pick ray (`DEVICE::rayCast`) and intersects it against the object's world AABB
(`DEVICE::rayIntersectsAABB`, slab method). Before 6.8.0 this instead unprojected the screen
point using the object's own **raw world Z coordinate** as the `depth` passed to the same
primitive `mbm.to3d` uses (see §2) — which degenerated to the camera's own position, for *every*
screen pixel, for any object sitting near world Z=0 (e.g. anything left at its default placement
position), so the test could never register a hit no matter where you clicked. That's fixed now;
no workaround needed. See §2 for `mbm.getPickRay` if you need the same ray/AABB math directly
from Lua (e.g. against a box that isn't a renderizable's own AABB).

**`obj:collide` (every form — object-vs-object, screen point, 2D and 3D) now tests against each
object's true AABB center (`obj:getAABBCenter()`, §6.3), not just `getPosition()`** (since
MBM_VERSION 6.9.0). This replaces an older internal-only mechanism (`doOffsetIfText`) that
handled this correction for `font` objects alone, by temporarily mutating the object's live
position, running the check, then restoring it. The new mechanism is general (any renderizable
type can have a non-zero center offset, not just text) and doesn't touch `getPosition()` at all.
No Lua-visible behavior change for anything already centered on its own pivot — this only
changes results for objects where `getAABBCenter() != getPosition()`.

### 6.5 Physics

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:getPhysics` | `()` | table | Physics collision shapes: array of `{type, x, y, z, width?, height?, depth?, ray?}`. Types: `"cube"`, `"sphere"`, `"triangle"`, `"complex"`. |

### 6.6 Animation & Shaders

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:setAnim` | `(name: string\|index: int)` | — | Play animation by name or index |
| `obj:getAnim` | `(index?: int)` | name, index | Get current (or nth) animation name and index |
| `obj:addAnim` | `(name, type, frames, fps, startFrame?)` | — | Define a new animation on the object |
| `obj:getTotalAnim` | `()` | int | Number of animations defined |
| `obj:getTotalFrame` | `()` | int | Total frames in current animation |
| `obj:getIndexFrame` | `()` | int | Current frame index (1-based) |
| `obj:restartAnim` | `()` | — | Restart animation from frame 1 |
| `obj:isEndedAnim` | `()` | bool | Whether a non-looping animation has finished |
| `obj:onEndAnim` | `(callback)` | — | Call `callback(obj, animationName)` once when a non-looping frame, articulated, skeletal base, or skeletal layer clip ends through normal playback. Looping clips, pause, and an isolated seek do not emit completion. |
| `obj:onEndFx` | `(callback)` | — | Call `callback()` when shader effect ends |
| `obj:setTypeAnim` | `(type: int)` | — | Set animation loop type using `mbm.*` constants |
| `obj:forceEndAnimFx` | `()` | — | Immediately stop the current shader animation effect |
| `obj:setTexture` | `(textureName: string)` | bool | Replace the object's texture at runtime. `textureName` can also be a solid-color shorthand `"#RRGGBB"`/`"#RRGGBBAA"` hex string (alpha-last, e.g. `"#FF0000FF"` for opaque red) instead of a file path — generates a small solid-color texture on the fly |
| `obj:setColor` | `(r, g, b, a?)` | bool | Tint the object (0.0-1.0 per channel, not 0-255). Same underlying binding as `setTexture` (dispatches on argument type: a string sets a texture, numbers set a solid tint) — passing >1 values doesn't error, they just clamp/wrap like any float color channel |
| `obj:setPixelShader` | `(shaderName: string, varValues?: table)` | bool | Apply a pixel shader |
| `obj:setVertexShader` | `(shaderName: string, varValues?: table)` | bool | Apply a vertex shader |
| `obj:getShader` | `()` | table | Get current shader config (`name`, `var` values) |
| `obj:setBlend` | `(srcBlend, dstBlend, op?)` | — | Set blend mode using `mbm.*` blend constants |
| `obj:getBlend` | `()` | srcBlend, dstBlend, op | Get current blend mode |

#### Articulated playback (`mesh` and `sprite`)

These methods are available on loaded `.msh` and `.spt` objects whose asset contains articulated
Parts and clips. Playback state is local to the object instance even when several objects share the
same cached asset. See [Mesh V11 articulated parts and animation](mesh-v11-format.md#6f-section_articulated_parts-and-section_articulated_animation-payloads)
for the persisted asset model, and the [Articulated Animation guide](articulated-animation.md) for
composition, authoring, playback lifecycle, and examples.

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:playArticulatedAnimation` | `(name, priority?, blendDuration?, weight?)` | bool | Start/restart a clip. Defaults: priority `0`, immediate blend, weight `1` |
| `obj:getTotalArticulatedAnimations` | `()` | int | Number of articulated clips in the loaded asset |
| `obj:getArticulatedAnimationName` | `(index: int)` | string or nil | Name of the 1-based articulated clip, or `nil` when out of range |
| `obj:pauseArticulatedAnimation` | `(name)` | bool | Freeze an active clip and its blend progress |
| `obj:resumeArticulatedAnimation` | `(name)` | bool | Resume a paused active clip |
| `obj:disableArticulatedAnimation` | `(name)` | bool | Remove an active clip from pose composition |
| `obj:seekArticulatedAnimation` | `(name, time)` | bool | Move an active clip's playback position to the specified time, clamped to `0..duration` |
| `obj:getArticulatedAnimationTime` | `(name)` | number or nil | Current time for an active clip, or `nil` when it is inactive/unknown |

#### Canonical skeletal playback (`mesh`, GPU LBS/DQS profile)

These methods control type-43 skeletal clips on a loaded canonical type-41/42/43 `.msh`. Playback
state and the evaluated GPU palette belong to the individual `mesh` instance even when several
objects share the same cached asset and shader program. The surface supports one base clip plus one
optional transient layer in explicit Absolute or bind-relative Additive mode. Each clip has independent time and authored loop/clamp behavior;
global pause/resume freezes both, while the layer also has an independent pause. Local TRS poses are composed before one final LBS/DQS palette,
and the layer is never serialized into the mesh asset. Choose `"lbs"` (the engine default),
rigid `"dqs"`, or `"auto"` before `load`. Auto resolves once to DQS only when bind and every clip
contain unit scale; otherwise it resolves to LBS. Changing method after load returns `false`, because
the resolved method is part of the compiled default-shader variant. The Absolute layer supports a
linear timed fade and a per-instance non-negative playback-speed multiplier shared by base, layer,
and fade. Per-bone masks, completion callbacks, and Metal are delivered; transition curves/queues
and priorities remain future work. OpenGL ES, DirectX9, and Metal provide the current GPU paths. An
explicit opt-in CPU execution path is available for LBS only; it keeps the skinning method API
separate from execution and rejects CPU+DQS rather than silently switching. LBS compact normals
reject negative scale, shear, or non-uniform scale; rigid DQS rejects any scale/shear.

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:getTotalSkeletalAnimations` | `()` | int | Number of canonical skeletal clips |
| `obj:getSkeletalAnimationName` | `(index: int)` | string or nil | Name of the 1-based clip |
| `obj:getSkeletalAnimationDuration` | `(index: int)` | number or nil | Duration in seconds of the 1-based clip |
| `obj:setSkeletalSkinningMethod` | `(method: "auto"|"lbs"|"dqs")` | bool | Select requested policy before `load`; returns `false` after the mesh is loaded |
| `obj:getSkeletalSkinningMethod` | `()` | string | Requested method/policy |
| `obj:getResolvedSkeletalSkinningMethod` | `()` | string | Actual compiled method (`"lbs"` or `"dqs"`), or `"unresolved"` before an Auto mesh is loaded |
| `obj:setSkeletalExecutionPath` | `(path: "gpu"|"cpu")` | bool | Select execution path before `load`; default is `"gpu"`. CPU is explicit LBS-only and returns `false` with requested `"dqs"` |
| `obj:getSkeletalExecutionPath` | `()` | string | `"gpu"` or `"cpu"` |
| `obj:getSkeletalSkinningReport` | `()` | table | `requestedMethod`, `resolvedMethod`, `resolutionReason`, preparation `status`, required bones, resolved-method maximum bones per draw, `executionPath`, and `executionStatus` |
| `obj:playSkeletalAnimation` | `(name)` | bool | Start or restart one clip at time zero |
| `obj:crossFadeSkeletalAnimation` | `(name, duration)` | bool | Linearly transition from the active base pose to `name` starting at target time zero, then promote the target to the sole base clip. Duration must be finite and non-negative; zero is immediate play. A positive transition replaces any ordinary transient layer and uses an unmasked Absolute blend; requesting another cross-fade before promotion returns `false`. |
| `obj:pauseSkeletalAnimation` | `()` | bool | Freeze the active clip and palette |
| `obj:resumeSkeletalAnimation` | `()` | bool | Resume the active clip |
| `obj:stopSkeletalAnimation` | `()` | bool | Stop the active clip and restore bind-pose deformation |
| `obj:seekSkeletalAnimation` | `(time)` | bool | Seek the active clip, clamped to its duration |
| `obj:getSkeletalAnimationTime` | `()` | number or nil | Current time, or `nil` when inactive |
| `obj:setSkeletalAnimationPlaybackSpeed` | `(speed)` | bool | Set a finite non-negative per-instance multiplier shared by base clip, Absolute layer, and fade; `0` freezes temporal progress without changing pause state |
| `obj:getSkeletalAnimationPlaybackSpeed` | `()` | number | Current multiplier; defaults to `1` |
| `obj:playSkeletalAnimationAbsoluteLayer` | `(name, weight)` | bool | Start or replace the transient second clip at time zero with strict Absolute weight `0..1`; requires an active base clip |
| `obj:playSkeletalAnimationAdditiveLayer` | `(name, weight)` | bool | Start or replace the transient second clip in bind-relative Additive local-TRS mode with strict weight `0..1`; requires an active base clip |
| `obj:pauseSkeletalAnimationLayer` | `()` | bool | Freeze only the active layer's clip time and fade while the base may continue |
| `obj:resumeSkeletalAnimationLayer` | `()` | bool | Resume independently paused layer time and fade |
| `obj:isSkeletalAnimationLayerPaused` | `()` | bool | Whether an active layer is independently paused |
| `obj:stopSkeletalAnimationAbsoluteLayer` | `()` | bool | Remove the transient Absolute or Additive layer while preserving the base clip; the historical method name is shared by both modes |
| `obj:seekSkeletalAnimationAbsoluteLayer` | `(time)` | bool | Seek the active Absolute or Additive layer independently, clamped to its clip duration |
| `obj:setSkeletalAnimationAbsoluteLayerWeight` | `(weight)` | bool | Change the active Absolute or Additive layer's strict weight `0..1` |
| `obj:fadeSkeletalAnimationAbsoluteLayer` | `(targetWeight, duration)` | bool | Linearly animate the active Absolute or Additive layer from its current weight to a strict target `0..1`; duration must be non-negative, pause freezes progress, and reaching zero removes the layer |
| `obj:getSkeletalAnimationAbsoluteLayerWeight` | `()` | number or nil | Current evaluated Absolute or Additive layer weight, or `nil` when no layer is active |
| `obj:getSkeletalAnimationAbsoluteLayerTime` | `()` | number or nil | Current independent Absolute or Additive layer time, or `nil` when no layer is active |
| `obj:setSkeletalAnimationLayerBoneWeight` | `(boneId, weight)` | bool | Set one active layer-mask multiplier using a nonzero hexadecimal stable bone ID and strict weight `0..1`; `1` restores the compatibility default |
| `obj:setSkeletalAnimationLayerBoneWeights` | `(edits)` | bool | Atomically apply a nonempty array of `{boneId=hexString, weight=number}` edits; IDs must be unique and valid and every weight must be in `0..1`, otherwise the complete batch is rejected unchanged |
| `obj:getSkeletalAnimationLayerBoneWeight` | `(boneId)` | number or nil | Get one active layer-mask multiplier; returns `1` for an unmasked valid bone and no value when the layer or identity is invalid |
| `obj:clearSkeletalAnimationLayerMask` | `()` | bool | Atomically restore every active layer-mask multiplier to the all-ones compatibility default |
| `obj:getSkeletalAnimationPose` | `()` | table or nil | Copy the active player's final evaluated global pose as ordered `{boneId, parentIndex, globalMatrix}` records; `parentIndex` is one-based with `0` for a root, and inactive/authoring-palette players return `nil`. If automatic root motion is enabled, this is the neutralized final pose, not the raw motion pose. |
| `obj:getSkeletalBoneTransform` | `(boneName, space?)` | table or nil | Read one named bone from the final evaluated pose in `"model"` (default) or `"world"` space. Returns `{boneId, space, position={x,y,z}, angle={x,y,z}, rotation={x,y,z,w}, scale={x,y,z}, matrix={...16 values...}}`; `angle` is Euler XYZ in radians for `setAngle`, while `rotation` preserves the normalized quaternion. Unknown bones, inactive poses, singular transforms, and sheared transforms return `nil`. If automatic root motion is enabled, this reads the neutralized final pose. |
| `obj:getSkeletalRootMotionDelta` | `(boneName, space?)` | table or nil | Copy the named bone's raw translation delta between the two latest continuous evaluated poses as `{boneId, space, translation={x,y,z}}`. Space is `"model"` by default or `"world"`, where the current mesh rotation/scale is applied. Returns `nil` before the first advancing update and after pause, seek, direct play/stop, authoring-pose installation, or a loop wrap. The query does not consume the delta and continues to report the raw selected-bone delta even when automatic root motion is enabled. |
| `obj:getSkeletalSharingCompatibility` | `(otherMesh)` | table | Read-only preflight for evaluated-pose/palette sharing between two loaded meshes. Returns `{compatible:boolean, reason:string, boneCount:int}` plus mismatch fields such as `boneIndex`, `boneName`, `boneId`, `otherBoneId`, parent IDs/indices, `observedError`, and `tolerance` when applicable. Strict compatibility requires both meshes to have canonical skeletons, equal bone count, exact ordered stable bone IDs and names, equal parent indices and parent IDs, and equivalent local/global bind matrices within the engine's scale-aware skeletal matrix tolerance. Clips, weights, geometry, and `skeletonId` do not need to match. Reason codes are `compatible`, `missing_skeleton`, `bone_count_mismatch`, `bone_identity_mismatch`, `hierarchy_mismatch`, and `bind_transform_mismatch`. |
| `obj:enableSkeletalPoseSharing` | `(sourceMesh)` | bool | Make this loaded mesh a direct follower of another loaded compatible mesh. The follower keeps its own geometry, textures, render transform, and skin weights, but its skeletal draw uses the source mesh's already-evaluated per-instance palette. The call rejects self, unloaded source/follower, incompatible skeletons, follower chains, and converting a source that already has followers into a follower, without changing the existing sharing state. It does not start animation on the source and it does not copy palette data. |
| `obj:disableSkeletalPoseSharing` | `()` | bool | Disable follower pose sharing. Returns `true`; it is valid to call when already disabled. |
| `obj:getSkeletalPoseSharing` | `()` | table | Returns `{enabled:boolean, active:boolean, reason:string, sourceFile:string?}`. `enabled` means a source link exists. `active` means the source is still loaded, still compatible, has the same resolved skinning method, and currently has an active evaluated pose/palette. Reasons are `disabled`, `not_loaded`, `source_not_loaded`, `skinning_method_mismatch`, `source_pose_inactive`, `active`, or a compatibility reason if the relationship became invalid. Source or follower reload/release/destruction clears the link to avoid dangling pointers. When enabled but inactive, the follower renders without shared skeletal deformation rather than CPU-deforming vertices. |
| `obj:enableAutomaticSkeletalRootMotion` | `(boneName, applyRotation?)` | bool | Enable per-instance automatic root motion for a loaded mesh and valid canonical bone name. Each continuous advancing update applies that bone's raw translation delta to the mesh position in world space, then neutralizes that bone's local translation in the final evaluated/rendered pose so the motion is not doubled. When `applyRotation` is true, the same continuous update also applies the raw normalized-quaternion rotational delta to the mesh angle and neutralizes the selected bone's local rotation to bind in the final pose. The default/false path is translation-only for compatibility. Discontinuities invalidate history instead of moving or rotating the mesh. |
| `obj:disableAutomaticSkeletalRootMotion` | `()` | bool | Disable automatic root motion and invalidate the root-motion delta history for this instance |
| `obj:getAutomaticSkeletalRootMotionBone` | `()` | table or nil | Return `{name, boneId, applyRotation}` for the currently enabled automatic root-motion bone, or `nil` when disabled |
| `obj:setSkeletalAuthoringPalette` | `(method, palette, time, orderedBoneIds)` | bool, string or nil | Editor bridge: install an evaluated `"lbs"` or `"dqs"` palette as a paused in-memory pose after exact ordered-bone identity validation; failure returns a diagnostic reason |

```lua
local character = mesh:new("3d")
assert(character:setSkeletalSkinningMethod("auto"))
assert(character:load("character-walk.msh"))
local report = character:getSkeletalSkinningReport()
print(report.requestedMethod, report.resolvedMethod, report.resolutionReason)
assert(character:playSkeletalAnimation("Walk"))
assert(character:enableAutomaticSkeletalRootMotion("mixamorig:Hips"))
assert(character:crossFadeSkeletalAnimation("Run", 0.25))
assert(character:setSkeletalAnimationPlaybackSpeed(0.5))
assert(character:playSkeletalAnimationAbsoluteLayer("LookAround", 0.35))
-- To use bind-relative deltas instead:
-- assert(character:playSkeletalAnimationAdditiveLayer("LookAround", 0.35))
character:seekSkeletalAnimationAbsoluteLayer(0.2)
character:setSkeletalAnimationAbsoluteLayerWeight(0.5)
character:fadeSkeletalAnimationAbsoluteLayer(1.0, 0.25)
character:pauseSkeletalAnimationLayer()
character:resumeSkeletalAnimationLayer()
character:seekSkeletalAnimation(0.5)
character:pauseSkeletalAnimation()
character:resumeSkeletalAnimation()
character:stopSkeletalAnimationAbsoluteLayer()

-- Read the final hand pose each frame for an attachment such as a sword.
local hand = character:getSkeletalBoneTransform("mixamorig:RightHand", "world")
if hand then
    sword:setPos(hand.position.x, hand.position.y, hand.position.z)
    sword:setAngle(hand.angle.x, hand.angle.y, hand.angle.z)
    -- hand.rotation preserves quaternion XYZW; hand.matrix preserves the complete transform.
end

-- Raw root-motion deltas remain available as a non-consuming query, even when automatic
-- root motion is enabled.
local rootDelta = character:getSkeletalRootMotionDelta("mixamorig:Hips", "world")
if rootDelta then
    print(rootDelta.translation.x, rootDelta.translation.y, rootDelta.translation.z)
end
```

### 6.7 Depth / Ordering

Draw order (depth-sorting) is controlled via the `obj.z` property:

```lua
obj.z = -10   -- push further back (draws earlier, appears behind)
obj.z =  10   -- push forward   (draws later,  appears in front)
```

For 2D world objects in the same layer, higher `z` values render on top. For tile maps, each tile layer is rendered at a specific `z` depth — you can insert 3D or 2D mesh objects between layers by assigning a matching `z` value, enabling correct depth sorting (e.g., a character sprite walking behind trees by placing the character between two tile layers).

### 6.8 Destroy

```lua
obj:destroy()    -- removes from scene and frees memory immediately
obj = nil        -- also assign nil to prevent accidental access and allow Lua GC
```
Objects are also garbage-collected automatically when they go out of scope (via `__gc`).

---

## 7. Render Types — Type-Specific Methods

### 7.1 sprite

```lua
local s = sprite:new("2dw", x, y, z)
s:load("file.spt")   -- load sprite asset; returns bool
```
All common methods apply. Animation names come from the `.spt` file.

### 7.2 mesh

```lua
local m = mesh:new("3d", x, y, z)
m:load("file.mbm")   -- load mesh; returns bool
```

`mesh:loadAsync` is a background-thread-friendly equivalent of `load`: the file I/O and mesh
parsing happen on a worker thread, and the callback always fires later — from the engine's normal
per-frame update, never inline from the `loadAsync` call itself, not even when the file turns out
to already be cached. Do not assume the mesh is ready on the line right after calling it.

```lua
m:loadAsync("file.mbm", function(self_mesh, success)
    if success then
        self_mesh:setScale(1, 1, 1)
    else
        print("error", "red", "failed to load mesh")
    end
end)
```

### 7.3 texture

```lua
local t = texture:new("2dw", x, y)
t:load("image.png")  -- returns bool
```

### 7.4 font

Font is the only render type created differently — it does not take coordinates in `new`:

```lua
local fnt = font:new("roboto.fnt", height?, spaceW?, spaceH?, savePng?)
-- height     number  optional font height override
-- spaceW     number  optional space character width
-- spaceH     number  optional line height
-- savePng    bool    debug: save generated texture atlas to PNG

local label = fnt:add("Hello World", coordType?, x?, y?, z?)   -- returns a text object
-- coordType  string  "2dw"/"2ds"/"3d" (§2) -- REQUIRED to pass x/y/z at all; see warning below
-- label.x, label.y, label.z, label.visible, label.sx ...  (all common props)
label.text = "new text"       -- update text content

fnt:setSpace(w, h)            -- change spacing
fnt:getSpace()                -- returns w, h
fnt:getHeight()               -- font height in units
fnt:setLetterXDiff(dx)        -- per-letter X offset
fnt:setLetterYDiff(dy)        -- per-letter Y offset
fnt:setSizeLetter(size)
fnt:getSizeLetter()
fnt:getTexture()              -- returns texture name string
fnt:getTotal()                -- count of text objects created from this font
```

**`fnt:add`'s 2nd argument is a coordinate-type string, not `x`** (`onAddTextFontLua`,
`font-lua.cpp`): the C++ binding reads argument position 3 (the 2nd argument after `text`) via
`getTypeWordRenderizableLua` and only reads positions 4/5/6 as `x`/`y`/`z` if that 3rd Lua argument
was present — `fnt:add("score", -300, 250)` (2 numbers, no coord-type string, matching a pattern
copy-pasted from `game-template/main.lua`'s own comments) does **not** error, but silently drops
both numbers: `getTypeWordRenderizableLua` coerces `-300` to the string `"-300.0"` via Lua's
number-to-string rule, that doesn't match `"2ds"`, so it silently defaults to `is2dw = true` and
the label lands at `2dw` `(0,0)`, not at your intended position. Always pass the coordinate type:
`fnt:add("score", "2ds", -300, 250)`.

**A `font` object's child text objects (`fnt:add()`'s return value) do not keep their own parent
`font` alive, and become dangling the instant nothing in Lua references the parent anymore** —
confirmed with gdb (SIGSEGV in `TEXT_DRAW::setText` → `renderText` →
`ANIMATION_MANAGER::getIndexAnimation` on a freed `this`) after `fnt:add()`'s font was only a
`local` scoped inside `onInitScene()`; Lua's GC eventually collected it mid-session (not
immediately — this is why the crash looks delayed/nondeterministic), and `font`'s `__gc`
(`onDestroyFontLua`, `font-lua.cpp`) `delete`s the `FONT_DRAW` **and every `TEXT_DRAW` it created**
unconditionally. Keep the `font` object itself referenced (e.g. a top-level `local`, not one
scoped to `onInitScene()`) for as long as any text object it created is still in use — the
game-template's own `score_font`/`score_text` pattern already does this correctly; the trap is
specifically re-declaring the font as a function-local "just for setup."

### 7.5 gif

```lua
local g = gif:new("2dw", x, y)
g:load("anim.gif")   -- returns bool
```

### 7.6 backGround

```lua
local bg = backGround:new("2dw")
bg:load("sky.png")           -- or a mesh file
bg:setFront("overlay.png")   -- 2D foreground layer
bg:setFront3d("fg.mbm")      -- 3D foreground mesh
```

### 7.7 particle

```lua
local p = particle:new("2dw", x, y)
p:load("fire.ptl")       -- load from config file, or configure manually:

p:setMinOffset(x, y, z)   p:getMinOffset()
p:setMaxOffset(x, y, z)   p:getMaxOffset()
p:setMinDirection(x,y,z)  p:getMinDirection()
p:setMaxDirection(x,y,z)  p:getMaxDirection()
p:setMinColor(r,g,b,a)    p:getMinColor()
p:setMaxColor(r,g,b,a)    p:getMaxColor()
p:setMinSize(w, h)        p:getMinSize()
p:setMaxSize(w, h)        p:getMaxSize()
p:setMinSpeed(v)          p:getMinSpeed()
p:setMaxSpeed(v)          p:getMaxSpeed()
p:setMinLifeTime(t)       p:getMinLifeTime()
p:setMaxLifeTime(t)       p:getMaxLifeTime()

p:setTotalParticle(n)     -- pool size
p:getTotalParticle()      -- total created
p:getTotalAlive()         -- currently alive

p:setInvertedColor(bool)  p:getInvertedColor()
p:getTexture()            -- texture name
p:restartAnim()
p:add(x, y, z?)           -- emit a particle at position
p:addAnim(frame)          -- add animation frame
p:getStage(idx)           p:setStage(idx, cfg)
p:addStage(cfg)           p:getTotalStage()
p:getAriseTime()          p:setAriseTime(t)
p:getStageTime()          p:setStageTime(t)
```

### 7.8 shape

```lua
local sh = shape:new("2dw", x, y)  -- or shape:new("3d", x, y, z)

-- Named primitive (flat 2D-style shapes; still usable as flat markers/highlights in 3D):
sh:create("circle", width, height, numTriangles?, dynamic?, nickName?)      -- numTriangles default 18
sh:create("rectangle", width, height, numTriangles?, dynamic?, nickName?)   -- aliases: "quad"/"square"/"rect"
sh:create("triangle", width, height, numTriangles?, dynamic?, nickName?)
sh:create("triangle", {x1,y1, x2,y2, x3,y3}, dynamic?, nickName?)          -- explicit 2D triangle (flat 6-number table, no z)

-- Raw vertex list -- any shape, "x,y" pairs for a 2D shape / "x,y,z" triples for a 3D one:
sh:create(verticesFlat, uvsFlatOrNil, nickName?, modeDraw?, modeCullFace?, modeFrontFace?)

-- Indexed variants (separate vertex/index/uv buffers):
sh:createIndexed(vertices, indices, uvs, normals?)
sh:createDynamicIndexed(vertices, indices, uvs)  -- updatable each frame
sh:onRender(callback)  -- callback(sh) called every frame for dynamic update
```

Indexed shapes use 16-bit vertex indices and therefore support at most 65,535 distinct vertices,
but the triangle index list itself may contain more than 65,535 entries. The Lua binding validates
that complete list with a 32-bit loop counter; large triangle collections do not wrap the validator.

**Pitfall: `nickName` is a shared cache key, not a per-instance label or a "reload guard."** Every
`create*` variant above ultimately resolves its geometry through the engine's mesh manager, keyed
by `nickName` (or an auto-generated name if omitted) — **across every `shape` object in the
process**, not scoped to the one object you called `:create()` on. If you build a shape whose
vertex content actually changes over time (e.g. regenerating geometry from live/draggable input
each time it edits) and reuse the same literal `nickName` on every rebuild, every call after the
very first silently returns that first call's cached mesh — your new vertex data is discarded with
no error, and any other shape that happens to reuse the same name gets that same stale geometry
too. Confirmed directly in `src/render/shape-mesh.cpp` (`SHAPE_MESH::load` calls
`MESH_MANAGER::load(nickName, ...)`, a name-first cache lookup) and reproduced building Scene
Editor 3D's triangle object marker (`editor/scene_editor3d.lua`): reusing one fixed nickname across
every point-drag rebuild made the shape appear completely static, and made every triangle marker
in the scene look identical to whichever one was created first.
- **Safe to share a fixed `nickName`** only when the content is deliberately identical across every
  instance that uses it (e.g. one unit-size cube/sphere/quad, with each instance differentiated
  purely via `:setPos()`/`:setScale()` — the intended, efficient use of this cache). Otherwise give
  each rebuild a unique `nickName`, e.g. an index/counter baked into the string.
  `onCreateTriangleShapeMeshLua`'s own fallback (used whenever `nickName` is omitted for the 2D
  explicit-triangle form above) demonstrates the safe pattern directly: it derives the name FROM
  the actual point values (`"triangle_points:x1:y1:...:dynamic"`), so different content is
  guaranteed to produce a different, non-colliding name.

### 7.9 line

```lua
local ln = line:new("2dw", x, y)
ln:add({x1,y1, x2,y2, x3,y3})   -- add line strip vertices
ln:set({x1,y1, x2,y2}, idx)     -- update a specific segment (table FIRST, then index -- confirmed against line-mesh-lua.cpp)
ln:size()                         -- number of line segments
ln:setColor(r, g, b, a?)         -- line color (0.0-1.0 each channel, not 0-255)
ln:setPhysics(physicsTable)       -- attach a physics silhouette
ln:drawBounding(bool)             -- show bounding box
```

### 7.10 tile

```lua
local tmap = tile:new("2dw")
tmap:load("map.tmx")   -- Tiled TMX format or engine format
-- tile maps have layers accessible via tmap:getLayer(name)
-- use the tiled plugin (require "tiled") for advanced tile operations
```

Each tile layer is rendered at its own `z` depth. To mix sprites/meshes with tile layers (e.g., a character that walks behind foreground trees), assign the character sprite a `z` value between the two layer depths:

```lua
-- assume background layer z = -5, foreground (tree) layer z = 5
character.z = 0   -- renders after background, before trees
```

### 7.11 render2texture

```lua
local rt = render2texture:new(width, height, channels?)
-- After rendering, use rt as a texture name string:
someSprite:setTexture(rt:getName())
```

`rt:getCamera(type)` (`type` is `"2d"` or `"3d"`) returns a camera object independent of the main
scene camera (`mbm.getCamera`), with its own `setPos`/`getPos`/`setFocus`/`getFocus`/`setScale`/
`getScale`/`setAngle`/`getAngle`/`setUp`/`getUp`/`move`, plus (MBM_VERSION 6.24.0)
`setNear`/`getNear`/`setFar`/`getFar`:

```lua
local camRt = rt:getCamera('3d')
camRt:setPos(0, 100, -300)
camRt:setFocus(0, 0, 0)
camRt:setNear(0.1)   -- 3D: perspective near plane (default 0.1)
camRt:setFar(2000)   -- 3D: perspective far plane, objects beyond this are culled (default 1000)
```

For `rt:getCamera('2d')`, `setNear`/`setFar` instead drive the 2D camera's own orthographic depth
range (default -100/100), independent of the 3D pair above — each camera object always affects
only its own projection.

---

## 8. vec2 / vec3 Objects

```lua
local v2 = vec2:new(x, y)
local v3 = vec3:new(x, y, z)
```

### Properties (read/write)
```lua
v.x   v.y   v.z    -- component access
```

### Methods
```lua
v:set(x, y, z?)    -- set all components
v:get()            -- returns x, y, z

-- Arithmetic (returns new vec):
local r = v + other
local r = v - other
local r = v * scalar
local r = v / scalar

v:length()         -- magnitude
v:normalize()      -- returns normalized copy
v:dot(other)       -- dot product
v:cross(other)     -- cross product (vec3 only)
v:distance(other)  -- distance to another vector
```

---

## 9. mbm Constants

Access as `mbm.CONSTANT_NAME`.

### Animation States

| Constant | Description |
|---|---|
| `mbm.PAUSED` | Animation is paused |
| `mbm.GROWING` | Playing forward, stops at end |
| `mbm.GROWING_LOOP` | Playing forward, loops |
| `mbm.DECREASING` | Playing backward, stops at start |
| `mbm.DECREASING_LOOP` | Playing backward, loops |
| `mbm.RECURSIVE` | Ping-pong, stops |
| `mbm.RECURSIVE_LOOP` | Ping-pong, loops |

### Blend Equation Operators

| Constant | Value |
|---|---|
| `mbm.ADD` | 1 |
| `mbm.SUBTRACT` | 2 |
| `mbm.REVERSE_SUBTRACT` | 3 |
| `mbm.MIN` | 4 |
| `mbm.MAX` | 5 |

### Blend Factors

`mbm.DISABLE`, `mbm.ZERO`, `mbm.ONE`,  
`mbm.SRC_COLOR`, `mbm.INV_SRC_COLOR`,  
`mbm.SRC_ALPHA`, `mbm.INV_SRC_ALPHA`,  
`mbm.DEST_ALPHA`, `mbm.INV_DEST_ALPHA`,  
`mbm.DEST_COLOR`, `mbm.INV_DEST_COLOR`

### Stage Constants
`mbm.STAGE_1`, `mbm.STAGE_2`

---

## 10. Plugin: box2d

2D rigid-body physics. Load with:
```lua
local box2d = require "box2d"
```

```lua
local world = box2d:new()
world:setGravity(0, -10)
world:step(delta)                               -- advance physics (call in onLoop())

-- Add bodies (pass a renderizable as the shape source)
world:addDynamicBody(renderizable, density?, friction?, restitution?)
world:addStaticBody(renderizable)
world:addKinematicBody(renderizable)

-- Collision callbacks
world:setContactListener(
    onBegin,    -- function(bodyA, bodyB, manifold)
    onEnd,      -- function(bodyA, bodyB)
    onPreSolve, -- function(bodyA, bodyB, manifold)  optional
    onPostSolve -- function(bodyA, bodyB, impulse)   optional
)

world:getWorldManifolds(body)   -- returns contact manifold table
world:destroy()
```

---

## 11. Plugin: box2dLiquidFun

2D fluid simulation (LiquidFun). Load with:
```lua
local lf = require "box2dLiquidFun"
```

```lua
local world = lf:new()
world:setGravity(0, -10)
world:step(delta)

local fluid = world:createFluid(
    {type="rectangle", center={x=0, y=100, z=0}, width=200, height=100},
    {particleRadius=5, density=1.0, damping=0.2}
)
local fluidShader = fluid:getShader()   -- returns shader config table
world:destroy()
```

---

## 12. Plugin: ImGui

Dear ImGui immediate-mode UI. Load with:
```lua
local tImGui = require "ImGui"
```
**All ImGui calls must happen inside `onLoop(delta)`.** They are immediate-mode — call every frame.

### Common Pitfalls

These bindings return/expect a different shape or range than most ImGui-familiar code (or this
doc's older revisions) assumes. Each was found by tracing the actual C++ binding after a
real bug, not from reading the header alone — verify against `plugins/imGui/imgui-lua.cpp` if
in doubt rather than assuming a Dear ImGui C++ signature carries over as-is.

- **`Checkbox`** returns only the resulting value, not `(changed, value)`: `local v = tImGui.Checkbox(label, value)`. Detect a toggle yourself with `if v ~= value then ... end`.
- **`ColorEdit3` / `ColorEdit4` / `ColorPicker3` / `ColorPicker4`** take AND return a single `{r,g,b,*a}` **table** (0.0-1.0 per channel), never separate r,g,b[,a] numbers in either direction. Passing scalars throws `"Expected table [tRgb]"` — this crashed an editor mid-session because a table field (`color.r`) was passed instead of the table itself.
- **`Combo`**'s `currentIdx` argument and returned index are **1-based** (Lua array convention) — the binding does the `-1`/`+1` conversion against ImGui's native 0-based index internally. Passing a 0-based index (e.g. `i-1` from a manual lookup loop) renders the combo with nothing selected.
- **`ListBox`** does **not** do that conversion — its index is **0-based**, unlike `Combo`. The two widgets are inconsistent with each other; don't assume one from the other.
- **`GetWindowSize` / `GetWindowPos` / `GetItemRectMin` / `GetItemRectMax` / `GetItemRectSize`** each return a single `{x,y}` table, not two numbers. Use `GetWindowWidth()`/`GetWindowHeight()` if you just need plain numbers.
- **Don't use `IsAnyWindowHovered()`/`IsAnyItemHovered()` to decide whether a click/scroll/drag should reach your game scene instead of the UI.** Use **`GetWantCaptureMouse()`** (and `GetWantCaptureKeyboard()` for keyboard) instead. Three reasons, all hit in practice on the same editor, repeatedly, across several tabs before being root-caused:
  1. `IsWindowHovered(ImGuiHoveredFlags_AnyWindow)` (what `IsAnyWindowHovered()` wraps) is designed to be queried about a *specific* window from inside that window's `Begin()`/`End()` block; using it as a global "is the UI in front of the mouse anywhere" check is a repurposing of a per-window API, not its intended use. Dear ImGui's own header comment on `IsWindowHovered` says as much: *"If you are trying to check whether your mouse should be dispatched to Dear ImGui or to your app, you should use the `io.WantCaptureMouse` boolean for that!"*
  2. `WantCaptureMouse`/`WantCaptureKeyboard` additionally account for cases `IsAnyWindowHovered()` misses: an active drag started over a window but now outside its rect, or an open combo/popup list rendered outside its parent window's bounds.
  3. Engine-specific: mini-mbm's own input callbacks (`onTouchDown`, `onTouchMove`, `onTouchZoom`) fire from the platform event-dispatch loop **before** `onLoop()` runs for that frame — see `CORE_MANAGER::onLoop()` in `core-manager-common.cpp`: `plugin->onPrepare()` (which calls `ImGui::NewFrame()`) runs, then queued input events are dispatched to the scene, and only *afterward* does `this->logic()` call `onLoop()`, which is where every `Begin()`/window actually gets drawn this frame. `WantCaptureMouse` is computed by `NewFrame()` itself and is documented as valid to read immediately after it — exactly where these callbacks run. `IsAnyWindowHovered()` has no such guarantee at that point in the frame.

  ```lua
  function onTouchDown(key, x, y)
      if tImGui.GetWantCaptureMouse() then return end  -- click was for the UI, not the scene
      ...
  end
  ```

### Window Management

```lua
local open, clicked = tImGui.Begin(title, closeable?, flags?)
-- Returns: is_window_open (bool), was_close_button_clicked (bool)
tImGui.End()

-- Flags helper:
local f = tImGui.Flags("ImGuiWindowFlags_NoMove", "ImGuiWindowFlags_NoResize")
```

Common `ImGuiWindowFlags_*`:
`NoMove`, `NoResize`, `NoTitleBar`, `NoScrollbar`, `MenuBar`, `NoBringToDisplayOnFocus`, `AlwaysAutoResize`

### Layout

```lua
tImGui.SetNextWindowPos(x, y, cond?)    -- cond: "Always", "Once", "Appearing"
tImGui.SetNextWindowSize(w, h, cond?)
tImGui.SetNextWindowSizeConstraints(minW, minH, maxW, maxH)

tImGui.SameLine(offsetX?, spacing?)
tImGui.Separator()
tImGui.Spacing()
tImGui.NewLine()
tImGui.Columns(n?, id?, border?)
tImGui.NextColumn()
tImGui.PushItemWidth(w)
tImGui.PopItemWidth()
```

### Widgets

```lua
tImGui.Text("label")
tImGui.TextColored(r, g, b, a, "text")    -- 0.0–1.0 floats
tImGui.TextWrapped("long text ...")

local pressed = tImGui.Button("label", w?, h?)
local pressed = tImGui.SmallButton("label")
local v = tImGui.Checkbox("label", bool_value)   -- returns only the new/current value, not a separate changed flag
-- detect a toggle yourself: `if v ~= bool_value then ... end`

-- Sliders (return changed:bool, newValue)
local c, v  = tImGui.SliderFloat("label", value, min, max, fmt?)
local c, v  = tImGui.SliderInt("label", value, min, max)
local c, v1, v2    = tImGui.SliderFloat2("label", v1, v2, min, max)
local c, v1,v2,v3  = tImGui.SliderFloat3("label", v1, v2, v3, min, max)
local c, v1,v2,v3,v4 = tImGui.SliderFloat4("label", v1, v2, v3, v4, min, max)

-- Drag
local c, v = tImGui.DragFloat("label", value, speed?, min?, max?)
local c, v = tImGui.DragInt("label", value, speed?, min?, max?)

-- Input
local c, s = tImGui.InputText("label", str, maxLen?)
local c, s = tImGui.InputTextMultiline("label", str, maxLen?, w?, h?)
local c, v = tImGui.InputFloat("label", value, step?, stepFast?)
local c, v = tImGui.InputInt("label", value, step?, stepFast?)

-- Color
-- ColorEdit3/4 and ColorPicker3/4 take AND return a single {r,g,b,*a} table (0.0-1.0 per
-- channel), NOT separate r,g,b,[a] numbers in either direction. Passing/expecting scalars
-- here throws "Expected table [tRgb]" (ColorEdit4's crash mode when misused) or silently
-- misreads the result.
local c, tRgb  = tImGui.ColorEdit3("label", {r=1,g=1,b=1})            -- tRgb = {r,g,b}
local c, tRgba = tImGui.ColorEdit4("label", {r=1,g=1,b=1,a=1})        -- tRgba = {r,g,b,a}
local c, tRgb  = tImGui.ColorPicker3("label", {r=1,g=1,b=1})
local c, tRgba = tImGui.ColorPicker4("label", {r=1,g=1,b=1,a=1})

-- Combo / Listbox
-- Combo's currentIdx/returned idx are 1-BASED (Lua convention: pass/read them like a Lua
-- array index, e.g. `tItems[idx]`). The C binding does the -1/+1 conversion to ImGui's
-- native 0-based index internally, so index 0 (or any 0-based value) reads as "no selection"
-- and renders the combo box empty.
local c, idx = tImGui.Combo("label", currentIdx, {"item1","item2",...})   -- idx: 1-based
-- Combo and ListBox are consistent with each other here
local c, idx = tImGui.ListBox("label", currentIdx, {"item1","item2",...}, h?)   -- idx: 1-based

-- Tree / collapsing
local open = tImGui.TreeNode("label")
tImGui.TreePop()
local open = tImGui.CollapsingHeader("label", flags?)
```

### Menus

```lua
if tImGui.BeginMenuBar() then
    if tImGui.BeginMenu("File") then
        local pressed, checked = tImGui.MenuItem("Open", "Ctrl+O", false)
        tImGui.EndMenu()
    end
    tImGui.EndMenuBar()
end
```

### Popups & Modals

```lua
tImGui.OpenPopup("myPopup")
if tImGui.BeginPopup("myPopup") then
    -- contents
    tImGui.EndPopup()
end

tImGui.OpenPopup("myModal")
local open, closed = tImGui.BeginPopupModal("myModal", true)
if open then
    tImGui.EndPopup()
end
```

### Tables

```lua
if tImGui.BeginTable("id", numCols, flags?) then
    tImGui.TableSetupColumn("Col1")
    tImGui.TableSetupColumn("Col2")
    tImGui.TableHeadersRow()
    for _, row in ipairs(data) do
        tImGui.TableNextRow()
        tImGui.TableSetColumnIndex(0)
        tImGui.Text(row.name)
        tImGui.TableSetColumnIndex(1)
        tImGui.Text(row.value)
    end
    tImGui.EndTable()
end
```

### Image

```lua
tImGui.Image(textureName, w, h, u0?, v0?, u1?, v1?)
local pressed = tImGui.ImageButton(textureName, w, h)
```

### Utility

```lua
tImGui.IsItemHovered()       -- was last item hovered?
tImGui.IsItemClicked(btn?)   -- was last item clicked?
tImGui.IsItemActive()
tImGui.GetItemRectMin()      -- returns a single {x,y} table (top-left of last item), NOT two values
tImGui.GetItemRectMax()      -- returns a single {x,y} table (bottom-right), NOT two values
tImGui.GetItemRectSize()     -- returns a single {x,y} table, NOT two values
tImGui.SetTooltip("text")    -- tooltip on hover
tImGui.PushID("id")          tImGui.PopID()
tImGui.GetWindowSize()       -- returns a single {x,y} table, NOT two values (use GetWindowWidth()/GetWindowHeight() for plain numbers)
tImGui.GetWindowPos()        -- returns a single {x,y} table, NOT two values
tImGui.GetWindowWidth()      -- returns a single number
tImGui.GetWindowHeight()     -- returns a single number
tImGui.SetScrollHereY(0.5)   -- scroll to position

tImGui.GetWantCaptureMouse()      -- true if the UI wants this frame's mouse input (see Common Pitfalls above)
tImGui.GetWantCaptureKeyboard()   -- true if the UI wants this frame's keyboard input
tImGui.CaptureMouseFromApp(bool?)     -- manually override WantCaptureMouse for next frame (default true)
tImGui.CaptureKeyboardFromApp(bool?) -- manually override WantCaptureKeyboard for next frame (default true)
```

---

## 13. Plugin: lsqlite3

SQLite3 Lua bindings. Load with:
```lua
sqlite3 = require "lsqlite3"
```

```lua
local db = sqlite3.open(":memory:")     -- in-memory DB
local db = sqlite3.open("game.db")      -- file-based DB

db:execute("CREATE TABLE scores (name TEXT, score INTEGER)")
db:execute("INSERT INTO scores VALUES ('Alice', 1000)")

for row in db:nrows("SELECT * FROM scores ORDER BY score DESC") do
    print(row.name, row.score)
end

local stmt = db:prepare("SELECT * FROM scores WHERE name = ?")
stmt:bind(1, "Alice")
for row in stmt:nrows() do print(row.score) end
stmt:finalize()

db:close()
```

---

## 14. print() extensions

The engine overrides Lua's built-in `print()` with a colored terminal version.

```lua
print("info", "green", "message")      -- [INFO ] in green
print("warn", "yellow", "message")     -- [WARN ] in yellow
print("error", "red", "message")       -- [ERROR] in red
print("line", "message")               -- includes file:line prefix
print("white", "plain message")        -- explicit color

-- Colors: "white", "red", "yellow", "green", "blue", "magenta", "cian"
-- Tags:   "info", "warn", "error"
-- Special: "line" (adds file+line prefix)
-- Order: [tag] [line] [color] message   (each component is optional)
```

Standard `print(...)` (no tag) still works and prints white.

---

## 15. Mesh Debug authoring

The editor-only `meshDebug` object exposes Mesh V11 authoring operations. To reorder subsets inside
a frame, use:

```lua
local moved = meshD:moveSubsetUp(frame, subset)
```

`frame` and `subset` are one-based. The call returns `true` when the subset exchanged positions
with its predecessor, or `false` for the first subset or an invalid index. Articulated Part
occurrences are remapped automatically, so their stable IDs, tracks, pivots, and parent-child
relationships remain attached to the same geometry.

### Geometry scaling

```lua
meshD:scaleFrame(frame, sx, sy, sz [, subset])
```

`frame=0` and `subset=0` mean all frames/subsets. The operation modifies geometry only. It never
mutates the canonical bind skeleton; changing bind transforms requires an explicit canonical
skeleton-authoring operation rather than an implicit side effect of vertex scaling.

For a coordinate-unit change of a complete skeletal asset, use:

```lua
meshD:scaleSkeletalAsset(scale)
```

`scale` must be finite and greater than zero. The call transactionally scales every geometry
frame, canonical bind-local translations, bone radius/length metadata, translation values in every
skeletal clip key, and physics bounds; it recompiles inverse bind and validates skeleton/clips
before committing. Weights, rotations, normals, and local scale channels are unchanged. Failure
raises a Lua error without partially modifying the asset. Mesh Debug selects this operation
automatically when its Transform node applies a positive uniform scale to all frames and subsets of
a canonical skeletal mesh. Partial transforms remain geometry-only; negative or non-uniform
whole-asset skeletal scaling is rejected.

### Bind-pose diagnostics

```lua
local report = meshD:getSkeletonBindReport(includeDependencyImpact)
```

This read-only call inspects canonical section 41 directly. An asset without section 41 has no
skeletal bind report; there is no exploratory-skeleton compatibility fallback.
It returns a table with `canonical`, `valid`, `boneCount`, `diagnosticCount`, `animationClipCount`,
`maximumReconstructionError`, `maximumBindIdentityError`, `bones`, and `diagnostics`. Each compiled
bone contains its one-based `sourceIndex`, `name`, stable `boneId`/`parentBoneId` as 16-digit hex
strings, one-based `parentIndex` (`0` for a root), local translation/quaternion/scale, row-major
16-number `localBindMatrix`, `globalBindMatrix`, and `inverseGlobalBindMatrix`, plus
`hasNegativeScale` and `hasShear`. IDs are strings because Lua numbers cannot preserve every
`uint64` value exactly. Canonical bone entries also expose authoring metadata `radius`,
`length`, bone-local `tailOffset={x,y,z}`, and `hasExplicitTail`; false means that the transform
joint has no selectable bone segment. Version-1 assets also load in this state. The boolean
`connectedToParent` separately preserves the authoring constraint between this head and
the parent tail; it is not inferred merely from hierarchy or coincident positions.
Removal-impact fields are `childCount`, `weightPaletteReferenced`, `weightedVertexCount`, and
`animationTrackCount`.
Diagnostics contain `code`, one-based `sourceIndex`, `boneName`,
`observedError`, and `fatal`.
`includeDependencyImpact` defaults to true. Passing false leaves the weight/track impact fields at
zero/false and avoids their vertex scans, intended for high-frequency geometric editor previews;
it does not omit transforms, matrices, hierarchy, tails, or diagnostics.

The call never edits the skeleton and the report is a detached Lua snapshot. Mesh Debug and the
Skeletal Animation Editor use it for canonical bind inspection. The exploratory bone and
name-palette weight methods are no longer registered in Lua; only the canonical skeletal surface
is available.

```lua
meshD:renameSkeletalBone(oneBasedBoneIndex, newName)
```

The new name must be nonempty and unique. The operation copies and recompiles section 41, then
validates existing type-42 weights and type-43 animations before committing. Bone identity remains
its stable `boneId`, so weight palette references and animation-track targets survive rename and
save/reload without remapping. Failure raises a Lua error and preserves the previous skeleton.

```lua
meshD:reparentSkeletalBone(oneBasedBoneIndex, newParentIndex, preserveGlobalBind)
```

`newParentIndex=0` makes the bone a root; positive values are one-based bone indices.
`preserveGlobalBind` defaults to `true`, deriving a new local TRS so the selected bone does not jump.
Passing `false` preserves local TRS and lets its subtree move. Self-parenting, descendant cycles,
non-invertible parents, and preserve-global results requiring shear are rejected. The candidate is
stably reordered parent-first and sections 41–43 are revalidated before commit.

```lua
meshD:setSkeletalBoneBind(oneBasedBoneIndex,
    tx, ty, tz, qx, qy, qz, qw, sx, sy, sz, radius, length)
```

Replaces the selected bone's parent-relative canonical bind TRS and authoring radius/length.
The quaternion is normalized before storage. Quaternion zero, singular scale, non-finite values,
negative radius/length, or a candidate that fails complete section 41–43 validation raises a Lua
error without mutation. Bone and skeleton IDs remain unchanged. The operation deliberately moves
the edited bone and its descendants; it does not compensate child-local transforms. Moving the
head away from a parent tail clears `connectedToParent` rather than preserving a false constraint.

```lua
meshD:setSkeletalBoneTail(oneBasedBoneIndex, tailX, tailY, tailZ,
    hasExplicitTail, preserveOtherJoints)
```

Replaces the Bone Editor endpoint in bone-local bind space. `hasExplicitTail` defaults to true;
an explicit tail must differ from the head. Length metadata is recomputed, and every direct child
marked `connectedToParent` receives the same parent-local head translation atomically. When
`preserveOtherJoints` is omitted or true, the remaining joints are compensated so their global
bind transforms do not move; false retains ordinary hierarchical propagation. Bind orientation is
deliberately not inferred from this authoring geometry.

```lua
meshD:setSkeletalBoneHead(oneBasedBoneIndex, tx, ty, tz, preserveOtherJoints)
```

Moves the head using a parent-local translation while preserving an explicit tail in global bind
space. The operation clears this bone's own `connectedToParent` constraint; children connected to
its preserved tail remain attached. `preserveOtherJoints` defaults to true and compensates every
other joint in global bind space; false lets descendants follow their hierarchy. This is the
bind-authoring primitive used by direct head drag.

```lua
meshD:translateSkeletalBoneSegment(oneBasedBoneIndex, tx, ty, tz, preserveOtherJoints)
```

Moves an explicit bone segment to a new parent-local head translation while retaining its
bone-local tail, so both endpoints receive the same rigid displacement. Connected child heads
follow the moved tail. `preserveOtherJoints` defaults to true and compensates every joint outside
the manipulated segment/shared endpoint; false lets the descendant hierarchy follow normally.

```lua
meshD:setSkeletalBoneConnectedToParent(oneBasedBoneIndex, connected,
    preserveOtherJoints)
```

Explicitly connects or disconnects the bone head relative to its current parent. Disconnecting only
clears the constraint and moves nothing. Connecting requires an explicit parent tail, moves the head
to that tail, preserves the edited segment's global tail, and defaults to compensating every other
joint globally. This operation does not change hierarchy; reparenting remains a separate contract.

```lua
meshD:setSkeletalBoneRadius(oneBasedBoneIndex, radius, includeDescendants)
```

Atomically sets a finite positive authoring/picking radius on the selected bone. When
`includeDescendants` is true, every member of its descendant subtree receives the same value while
ancestors and other branches remain unchanged. Radius is visual/editor metadata and does not alter
canonical vertex weights.

```lua
local newBoneIndex = meshD:addSkeletalBone(parentIndex, name, tx, ty, tz, radius, length,
    hasExplicitTail, connectedToParent)
```

Adds one canonical bone. `parentIndex=0` creates a root; positive values select a one-based existing
parent. The returned index is one-based. The bone receives a new opaque nonzero stable ID, identity
rotation/scale, and the supplied parent-relative translation and nonnegative radius/length. Empty or
duplicate names, invalid parents, non-finite input, or a candidate that fails section 41–43 validation
raises a Lua error without mutation. Existing weight palettes and animation tracks are unchanged.
Both booleans are optional: `hasExplicitTail` defaults to true and `connectedToParent` to false.

```lua
local lastBoneIndex = meshD:addSkeletalBoneChain(parentIndex, namePrefix, count,
    stepX, stepY, stepZ, radius, length)
```

Atomically appends between 1 and 256 linked canonical bones to root (`parentIndex=0`) or an existing
one-based parent. Names are generated as `namePrefix1` through `namePrefixN`; each bone is parent of
the next and receives the same parent-relative translation step, identity rotation/scale, metadata,
and a new opaque stable ID. The returned one-based index identifies the last bone. Any duplicate
generated name, invalid input, ID exhaustion, or failed 41–43 validation rejects the entire chain.

```lua
local lastBoneIndex = meshD:extendSkeletalBoneTail(oneBasedBoneIndex, count, radius, length)
```

Atomically adds between 1 and 256 explicitly connected segments from the selected tail. The first
head uses the selected bone's exact tail; every new segment continues that tail's bone-local
direction with the requested length. Collision-free `Bone_N` labels and stable IDs are allocated,
and any validation failure rejects the complete extension rather than leaving a partial chain.

```lua
local mirroredRootIndex = meshD:mirrorSkeletalBoneSubtree(oneBasedBoneIndex,
    axis, namePrefix)
```

Atomically duplicates the selected bone and all descendants. `axis` is `1` for global X, `2` for Y,
or `3` for Z. Each global bind matrix is reflected across that origin plane by full matrix
conjugation; local TRS is then derived against the original external parent or newly mirrored parent.
Names become `namePrefix .. originalName`, metadata/hierarchy are copied, and every duplicate gets a
new stable ID. Existing weights remain unchanged, so mirrored bones begin without influences. The
first implementation rejects assets with canonical clips, duplicate names, singular parents, shear,
or failed dependency validation. The returned one-based index selects the mirrored subtree root.

```lua
meshD:initializeSkeletalSkeleton(rootName, tx, ty, tz, radius, length)
```

Creates canonical section 41 with one root bone on a loaded mesh that has no skeleton, weights, or
skeletal animations. The new skeleton and root receive stable nonzero IDs, identity rotation/scale,
and the supplied translation and nonnegative metadata. It deliberately does not create section 42
weights or section 43 clips. Existing skeletal data, an unloaded mesh, invalid values, or an empty
name reject without mutation.

```lua
meshD:removeSkeletalBone(oneBasedBoneIndex)
```

Strictly removes one unreferenced leaf while retaining at least one skeleton bone. Removal is
rejected if the bone has children, appears in the canonical weight palette, or is targeted by any
animation track. It never reparents descendants, rewrites palettes, redistributes weights, or
deletes tracks implicitly. The remaining section 41–43 candidate validates before commit; failure
raises a Lua error without mutation.

```lua
local pose = meshD:evaluateSkeletalAuthoringPose(clipIndex, time, method)
-- Optional final arguments override one sampled bone in local space:
-- boneIndex, tx,ty,tz, qx,qy,qz,qw, sx,sy,sz
```

Evaluates the current unsaved canonical clip state with `method` equal to `"lbs"` or `"dqs"`.
The detached result contains `bones` with local TRS and evaluated global matrices, the packed
`palette`, and `boneIds` in exact palette order. Pass all three relevant values to
`mesh:setSkeletalAuthoringPalette(method, pose.palette, time, pose.boneIds)`. The optional one-bone override is applied
after sampling and before hierarchy/global reconstruction. This is an editor-preview bridge; it
does not mutate keys or persist a pose. `boneIds` use the canonical 16-digit hexadecimal string
representation; converting them to 32-bit numbers destroys stable identity.

```lua
local created = meshD:commitSkeletalAuthoringKey(clipIndex, boneIndex, time, channelMask,
    tx,ty,tz, qx,qy,qz,qw, sx,sy,sz)
meshD:commitSkeletalAuthoringPose(clipIndex, time, {
    {hexBoneId, tx,ty,tz, qx,qy,qz,qw, sx,sy,sz},
    ...
})
```

Atomically commits an evaluated local pose to the selected canonical clip/bone. It creates the
bone track when absent, unions the requested T/R/S `channelMask`, and creates or updates the key at
`time`; `created` distinguishes insertion from update. The complete candidate animation validates
before replacement, so failure cannot leave a track or channel half-created.

`commitSkeletalAuthoringPose` is the complete-pose batch equivalent. Its detached input must contain
exactly one local TRS for every stable bone ID in the destination skeleton. It creates missing
tracks with T+R+S, unions T+R+S into existing tracks, and creates or updates one key per bone at
`time`. Duplicate, missing, or unknown identities, invalid transforms, and an out-of-range time
raise a Lua error without committing any bone. It returns `true` after one complete validation and
atomic replacement.

```lua
meshD:removeSkeletalBoneRemapped(oneBasedBoneIndex, replacementBoneIndex,
    discardAnimationTracks, reparentChildrenPreserveGlobal)
```

Removes a leaf while explicitly transferring its weight-palette reference to a different existing
bone. If both bones influence a vertex, their weights are summed and duplicate slots are compacted;
coverage remains normalized. Tracks targeting the removed identity are not retargeted because their
local transforms have different semantics: they are deleted only when `discardAnimationTracks` is
`true`, otherwise the operation rejects. With `reparentChildrenPreserveGlobal=true`, direct children
are moved to the removed bone's parent (or become roots) and receive newly derived local TRS that
preserves their global bind. For animated assets, each promoted child receives a full-TRS linear
track baked at time zero, clip end, and the union of authored key times from the child and removed
bone. Those authored samples preserve global pose; continuous interpolation between them may differ
from the original composed curves. Skeleton, weights, and animations commit together only after
complete validation, and a composition requiring shear rejects without mutation.

### Canonical skeletal-weight editing

```lua
local present = meshD:hasSkeletalVertexWeights()
local paletteSize = meshD:getTotalSkeletalWeightBones()
local initializedVertexCount = meshD:initializeSkeletalVertexWeights(oneBasedBoneIndex)
local removedVertexCount = meshD:removeSkeletalVertexWeights()
local removedBoneCount, removedWeightedVertexCount, removedClipCount =
    meshD:removeAllSkeletalData()
local name1, weight1, name2, weight2, name3, weight3, name4, weight4 =
    meshD:getSkeletalVertexWeight(vertexIndex)
meshD:setSkeletalVertexWeightsBatch({
    {vertexIndex, name1, weight1, name2, weight2, name3, weight3, name4, weight4},
    ...
})
```

These one-based vertex operations read and mutate canonical type-42 weights only. Bone names are
accepted for editor ergonomics but are resolved immediately through the canonical skeleton and
persisted as stable `boneId` palette entries. A set requires one to four unique, finite, positive
influences whose sum is one; unused slots are `nil, 0`. Unknown names, invalid sums, duplicate
influences, out-of-range vertices, or assets without matching type-41/type-42 sections raise a Lua
error without modifying the previous vertex. The getter returns eight values, using `nil, 0` for
unused slots, or a single `nil` for an invalid vertex. These methods never read, create, or update
the exploratory sections 11/40.

`setSkeletalVertexWeight(vertexIndex, name1, weight1, ..., name4, weight4)` remains registered as a
deprecated compatibility wrapper. New code should use `setSkeletalVertexWeightsBatch`, including
for one vertex, so validation and mutation always use the same atomic transaction boundary.

`setSkeletalVertexWeightsBatch` applies one or more unique one-based vertex edits through a single
detached type-42 candidate. Every row follows the scalar setter's influence contract. The complete
candidate validates before replacement, so an invalid row, duplicate vertex index, unknown bone,
or invalid final canonical collection raises a Lua error without applying any row. It returns
`true` after one atomic commit. This is the backend transaction boundary for a future Paint Weights
stroke; it does not implement brush math, picking, heatmaps, or Undo by itself.

`initializeSkeletalVertexWeights` is the explicit bootstrap for a locally authored skeleton that
does not yet have type-42 data. It creates complete frame-zero coverage and rigidly binds every
vertex to the selected existing bone with weight `1.0`, returning the number of initialized
vertices. It never guesses geometric influences and never replaces existing weights: an absent
canonical skeleton, invalid bone index, empty frame-zero geometry, existing type-42 data, or an
invalid resulting 41/42 contract raises a Lua error without mutation. Subsequent distribution and
blending belong to Paint Weights.

`removeSkeletalVertexWeights` removes the complete canonical type-42 section and returns its vertex
count. It preserves the type-41 skeleton and every type-43 animation clip, allowing weights to be
initialized again deliberately. An asset without canonical weights raises a Lua error without
mutation.

`removeAllSkeletalData` atomically removes the canonical skeleton, weights, and animation sections
41-43 and returns their bone, weighted-vertex, and clip counts. Clips cannot survive without the
skeleton identity they target. An asset without a canonical skeleton raises a Lua error without
mutation.

### Canonical skeletal-animation inspection

```lua
local clips = meshD:getSkeletalAnimationReport()
```

Returns a read-only snapshot of canonical type-43 data. Each clip contains hexadecimal `clipId`,
`name`, `duration`, `loop`, and `tracks`. Each track contains hexadecimal `boneId`, one-based
`boneIndex`, resolved `boneName`, numeric `channelMask` (`1` translation, `2` rotation, `4` scale),
and `keys`. Every key exposes `time`, local `translation`, quaternion `rotation`, local `scale`,
numeric `easing` (`0..5`), and the four normalized Cubic-Bezier control values in `bezier`.
The returned tables are detached copies for editor inspection; modifying them does not mutate the
mesh. An asset without type-43 clips returns an empty table.

```lua
local oneBasedClipIndex = meshD:addSkeletalClip(name, duration, loop)
meshD:updateSkeletalClip(oneBasedClipIndex, name, duration, loop)
meshD:removeSkeletalClip(oneBasedClipIndex)
```

These methods transactionally author the canonical clip container without editing tracks or keys.
Creation requires a canonical skeleton, allocates a new nonzero opaque `clipId`, and rejects empty
or duplicate names and non-finite/negative durations. Update preserves the stable ID and rejects a
duration that would place any existing key beyond the clip end. Removal deletes the selected clip
and all of its tracks/keys; removing the final clip clears type-43 storage entirely. Each operation
validates the complete canonical animation collection before commit and raises a Lua error without
mutation on failure.

```lua
local oneBasedTrackIndex = meshD:addSkeletalTrack(
    oneBasedClipIndex, oneBasedBoneIndex, channelMask)
meshD:updateSkeletalTrackChannels(oneBasedClipIndex, oneBasedTrackIndex, channelMask)
meshD:removeSkeletalTrack(oneBasedClipIndex, oneBasedTrackIndex)
```

Track authoring uses stable bone identity internally; one-based bone indices are resolved only at
the API boundary. `channelMask` must contain at least one of translation `1`, rotation `2`, or scale
`4`, and a clip may contain at most one track per bone. Because canonical type-43 forbids empty
tracks, creation inserts one key at time zero initialized from that bone's local bind TRS. Updating
the mask preserves all stored key values and revalidates them under the newly enabled channels.
Removal deletes the selected track and all its keys. All operations validate and commit atomically.

```lua
local oneBasedKeyIndex = meshD:addSkeletalKey(oneBasedClipIndex, oneBasedTrackIndex, time)
meshD:updateSkeletalKey(oneBasedClipIndex, oneBasedTrackIndex, oneBasedKeyIndex,
    time, tx, ty, tz, qx, qy, qz, qw, sx, sy, sz,
    easing, bezierX1, bezierY1, bezierX2, bezierY2)
meshD:removeSkeletalKey(oneBasedClipIndex, oneBasedTrackIndex, oneBasedKeyIndex)
meshD:moveSkeletalKeys(oneBasedClipIndex,
    {trackIndex1, keyIndex1, trackIndex2, keyIndex2, ...}, timeDelta)
meshD:duplicateSkeletalKeys(oneBasedClipIndex,
    {trackIndex1, keyIndex1, trackIndex2, keyIndex2, ...}, timeDelta)
meshD:pasteSkeletalKeys(oneBasedDestinationClipIndex, {
    {hexBoneId, channelMask, sourceTime,
     tx, ty, tz, qx, qy, qz, qw, sx, sy, sz,
     easing, bezierX1, bezierY1, bezierX2, bezierY2},
    ...
}, sourceMinimumTime, insertionTime)
meshD:insertSkeletalKeysRipple(oneBasedClipIndex,
    {trackIndex1, keyIndex1, trackIndex2, keyIndex2, ...}, insertionTime)
meshD:insertSkeletalEmptyTime(oneBasedClipIndex, insertionTime, duration)
local removedKeyCount = meshD:removeSkeletalTimeRange(
    oneBasedClipIndex, startTime, duration)
```

Insertion requires a unique time inside the clip, samples the current canonical clip first, and
stores the target bone's evaluated local TRS at that time; adding a key therefore does not alter the
existing curve by itself. Keys remain strictly time-ordered. Update may change time, TRS, easing
`0..5`, and Bezier controls; it normalizes a finite nonzero quaternion and atomically reorders the
key when its time changes. Duplicate times, invalid transforms, unsupported easing/Bezier values,
and times outside the clip reject without mutation. Removal is allowed only while the track retains
at least one key.
`moveSkeletalKeys` translates every referenced key by the same finite time delta. Pair indices refer
to the ordering before the move. Duplicate references, out-of-range results, or collisions reject
the candidate without mutation; affected tracks are sorted and the complete animation collection
validates once before one atomic commit.
`duplicateSkeletalKeys` uses the same pre-operation pair contract and copies complete key payloads
into their source tracks at shifted times. Existing keys remain untouched. Duplicate references,
out-of-range destinations, and collisions reject the entire candidate before commit.
`pasteSkeletalKeys` accepts detached complete key payloads, so the source clip no longer needs to
exist or remain unchanged. Each hexadecimal bone ID resolves against the destination skeleton. A
matching destination track must have the same T/R/S mask; an absent track is created with the copied
mask and pasted keys. Source times are offset so `sourceMinimumTime` lands at `insertionTime`.
Unknown bones, incompatible masks, invalid transforms, out-of-range times, duplicate destinations,
or any invalid complete animation candidate raise a Lua error and leave the destination untouched.
The method returns `true` on one atomic commit.
`insertSkeletalKeysRipple` requires a selection with a positive time span. It opens that amount of
space across every track by shifting keys at or after `insertionTime`, then copies the selected keys
with their earliest time aligned to the insertion point. The clip duration grows by the opened span
(plus the canonical numerical separation needed between endpoint keys). Duplicate references,
invalid results, and collisions reject the entire candidate atomically.
`insertSkeletalEmptyTime` creates no keys. It grows the clip by the positive finite `duration` and
shifts every existing key at or after `insertionTime` by exactly that duration across every track.
The complete result validates and commits atomically.
`removeSkeletalTimeRange` removes keys in the semi-open interval `[startTime, endTime)`, where the
end is clamped to the current clip duration. Surviving keys at or after the end shift left by the
effective removed duration and the clip shrinks equally. It returns the number of deleted keys and
rejects atomically if any track would become empty or the resulting canonical clip would be invalid.

### 15.1 Articulated-animation authoring

The object also exposes the Mesh V11 rigid-animation authoring API.
Indices returned by this API are one-based, matching the existing Mesh Debug methods. Part IDs are
nonzero, globally unique `uint64` identities within the asset; names are labels, may repeat, and are
not used as file references. Each frame/subset occurrence can have only one Part. Invalid mutations
raise a Lua error with the engine validation message; successful mutations return `true`, except
creation methods, which return their new one-based index, and the initialize/remove-all methods,
which return a count.

```lua
local partIndex = meshD:addArticulatedPart(
    partId, frame, subset, name,
    pivotX, pivotY, pivotZ,
    pivotQX, pivotQY, pivotQZ, pivotQW,
    parentPartId)

local added = meshD:initializeArticulatedParts() -- computes missing IDs and AABB-center pivots
local removedParts = meshD:removeArticulatedParts() -- removes parts and their tracks; clips remain
meshD:updateArticulatedPart(partIndex, name,
    pivotX, pivotY, pivotZ,
    pivotQX, pivotQY, pivotQZ, pivotQW,
    parentPartId)

local clipIndex = meshD:addArticulatedAnimation(name, duration, speed, priority, loop, blendMode)
meshD:removeArticulatedAnimation(clipIndex)
local trackIndex = meshD:addArticulatedTrack(clipIndex, partId, channelMask)
meshD:setArticulatedTrackChannels(clipIndex, trackIndex, channelMask)
meshD:addArticulatedKey(clipIndex, trackIndex, time,
    positionX, positionY, positionZ,
    rotationQX, rotationQY, rotationQZ, rotationQW,
    scaleX, scaleY, scaleZ)
meshD:setArticulatedKeyEuler(clipIndex, trackIndex, time,
    rotationEulerX, rotationEulerY, rotationEulerZ)
meshD:setArticulatedKeyEasing(clipIndex, trackIndex, keyIndex, easing)
meshD:setArticulatedKeyBezier(clipIndex, trackIndex, keyIndex, x1, y1, x2, y2)
```

Inspection return values are:

| Method | Returns |
|---|---|
| `getTotalArticulatedParts()` | Part count |
| `getArticulatedPart(index)` | `partId, frame, subset, name, pivotX, pivotY, pivotZ, pivotQX, pivotQY, pivotQZ, pivotQW, parentPartId`, or `nil` |
| `getTotalArticulatedAnimations()` | Clip count |
| `getArticulatedAnimationName(index)` | Name, or `nil` |
| `getArticulatedAnimation(index)` | `name, duration, speed, priority, loop, blendMode`, or `nil` |
| `getTotalArticulatedTracks(animation)` | Track count |
| `getArticulatedTrack(animation, track)` | `partId, channelMask, keyCount`, or `nil` |
| `getArticulatedKey(animation, track, key)` | `time`, position XYZ, quaternion XYZW, scale XYZ, `easing`, Bezier X1/Y1/X2/Y2, authored Euler XYZ, `hasEuler`; or `nil` |

Channel masks are `1` for position, `2` for rotation, and `4` for scale.
`setArticulatedTrackChannels(animation, track, channelMask)` updates the channels controlled by an
existing track and rejects zero or unknown mask bits. Disabling a channel does not erase its values
from the track's keys, so re-enabling it restores their influence. Adding another key for the same
track and time replaces the previous key. `getTotalArticulatedParts()`,
`getArticulatedPart(index)`, `getTotalArticulatedAnimations()`, and
`getArticulatedAnimationName(index)`, `getArticulatedAnimation(index)`,
`getTotalArticulatedTracks(animation)`, `getArticulatedTrack(animation, track)`, and
`getArticulatedKey(animation, track, key)` provide inspection. The editor can create tracks with
independent channel masks and add or replace keys. `updateArticulatedKey(animation, track, key,
time, ...)` moves and edits an existing key, consolidating a collision with another key at the
same time. `updateArticulatedAnimation(index, name, duration, speed, priority, loop, blendMode)` edits clip
metadata while preserving its tracks. `removeArticulatedKey(animation, track, key)` removes one
keyframe without changing the clip's manually editable duration. The clip duration is automatically
kept at least as large as the greatest key time; a requested shorter duration is clamped.
`removeArticulatedAnimation(index)` removes a clip and all its tracks and keys.
`removeArticulatedParts()` removes all parts/pivots and tracks referencing those parts; clips remain.
Mesh Debug displays key rotation as authored Euler degrees; the runtime converts those values to
the quaternion used for rendering. `getArticulatedKey` also returns the key easing mode, and
`setArticulatedKeyEasing` accepts `0` Linear, `1` Ease In, `2` Ease Out, `3` Ease In Out, or
`4` Smoothstep, or `5` Cubic Bezier. `getArticulatedKey` returns the four Bezier control values
after the easing mode, followed by the authored Euler X/Y/Z values and a boolean indicating whether
those Euler values are present. Existing return values keep their original order. Mesh Debug uses
the authored values instead of deriving Euler angles from the equivalent quaternion, preserving
inputs such as `359.99°` when the asset is reopened. `setArticulatedKeyBezier` changes the curve
controls; X is restricted to `0..1`, while Y may leave that range for overshoot. The easing on a
key controls the transition to the next key; the final key has no outgoing segment.
`parentPartId` establishes a same-frame parent relationship. A child inherits the parent's complete
transform and keeps its own local transform; self-parenting, missing parents, and cycles are rejected.

Clip composition modes are `0` for Absolute and `1` for Additive. `getArticulatedAnimation(index)`
returns this mode as its sixth result, after `loop`. Absolute is the default. Additive keys represent
deltas: neutral position is `(0,0,0)`, neutral Euler rotation is `(0,0,0)`, and neutral scale is
`(1,1,1)`.

Loaded `mesh` and `sprite` objects expose the same playback controls for `.msh` and `.spt` assets.
Multiple clips may be active. Absolute position, rotation, and scale are resolved independently:
the highest-priority Absolute clip that provides a channel wins that channel, and a newer clip wins
ties. This lets a lower-priority Absolute clip continue supplying channels omitted by the winner.
All active Additive clips are then applied in increasing priority/start order; the ordering matters
for local quaternion rotation composition.

`playArticulatedAnimation(name, priority, blendDuration, weight)` accepts an optional transition duration
in seconds. During that interval, each channel transitions from the already-composed pose of its
lower-priority/older candidates, or from the base transform when no fallback exists. This also
keeps overlapping crossfades continuous. Position and scale use linear interpolation and rotation
uses quaternion spherical interpolation. The default `blendDuration` is `0`, preserving immediate
playback. `weight` defaults to `1`, is clamped to `0..1`, and controls additive intensity; absolute
clips ignore it. Blend duration and weight are runtime state and are not persisted in the asset.
For an additive clip, blend duration fades its contribution from zero to the requested weight.
Authored Euler rotation is weighted before conversion to the final quaternion, preserving intended
full turns such as `0°..359°`; quaternion-only data falls back to spherical interpolation.

`pause` freezes both clip time and blend progress, `resume` continues them, and `disable` removes
the clip from evaluation. Playback state belongs to each renderizable instance: objects loading
the same cached asset share geometry, parts, and clip definitions, but do not share active clips,
time, pause state, priority, blend progress, additive weight, or seek position.

A non-looping articulated clip invokes the same callback registered by `obj:onEndAnim(callback)`
when it reaches its duration. The callback receives the renderizable object and the articulated
clip name. Looping clips do not emit this completion callback. If multiple active clips finish in
the same update, the callback is invoked once for each finished clip.

Non-looping skeletal base and transient layer clips use that same callback and clip-name argument.
They emit once when normal playback reaches the clamped end pose, including while the mesh is
culled. Replay or seeking backward rearms completion; looping clips, pause, and seek alone do not
emit it.

```lua
car:playArticulatedAnimation("wheel_spin", 10, 0.5, 0.75)
-- priority 10, 0.5-second fade, 75% weight when the clip is Additive
car:onEndAnim(function(object, animationName)
    print(animationName .. " finished")
end)
car:pauseArticulatedAnimation("wheel_spin")
car:resumeArticulatedAnimation("wheel_spin")
car:seekArticulatedAnimation("wheel_spin", 0.5)
local currentTime = car:getArticulatedAnimationTime("wheel_spin")
car:disableArticulatedAnimation("wheel_spin")

local icon = sprite:new("2dw")
icon:load("machine.spt")
icon:playArticulatedAnimation("gear_spin") -- priority 0, immediate transition
```

The runtime advances clip time with the engine's `device->delta`, preserving the engine time-scale
and frame-rate behavior. Easing is evaluated in the articulated track sampler before interpolating
position, rotation, and scale. Cubic Bezier solves its normalized-time X curve before evaluating Y.
Version-1 articulated sections default to Linear.
