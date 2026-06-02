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
wx, wy, wz   = mbm.to3d(sx, sy, depth)  -- screen pixels → 3D world coords
```

---

## 3. mbm Namespace

All engine-level functions live in the global `mbm` table.

### 3.1 Scene & Control

| Function | Signature | Returns | Description |
|---|---|---|---|
| `mbm.quit` | `()` | — | Exit the application |
| `mbm.pause` | `()` | — | Pause engine (stops `logic` calls and audio) |
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
| `mbm.setColor` | `(r, g, b)` | — | Background clear color (0–255 each channel) |
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
| `mbm.to3d` | `(sx, sy, depth)` | wx, wy, wz | Screen px → 3D world coords at given depth |

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
| `mbm.colorDialog` | `()` | r, g, b \| nil | Native color picker; returns 0-255 values |

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
`"USE_EDITOR_FEATURES"`, `"MBM_ENABLE_MESH_LEGACY_V7"`,  
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
| `cam:setUp` | `()` | vec3 | Get up vector |

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
```

### 6.3 Size & Bounds

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:getSize` | `(considerScale?: bool)` | w, h [, d] | Object dimensions. 3D returns depth too. |
| `obj:getAABB` | `(update?: bool)` | w, h [, d] | Axis-aligned bounding box. Pass `true` to force recalc. |
| `obj:isOnScreen` | `()` | bool | Whether the object is visible within the camera frustum |
| `obj:isLoaded` | `()` | bool | Whether the asset is fully loaded |

### 6.4 Collision

| Method | Signature | Returns | Description |
|---|---|---|---|
| `obj:collide` | `(other)` or `(x, y)` | bool | AABB collision vs another object, or vs screen point |
| `obj:isOver` | `(x, y)` | bool | Is screen point (x, y) inside the object's bounding rect |

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
| `obj:onEndAnim` | `(callback)` | — | Call `callback()` when animation ends |
| `obj:onEndFx` | `(callback)` | — | Call `callback()` when shader effect ends |
| `obj:setTypeAnim` | `(type: int)` | — | Set animation loop type using `mbm.*` constants |
| `obj:forceEndAnimFx` | `()` | — | Immediately stop the current shader animation effect |
| `obj:setTexture` | `(textureName: string)` | bool | Replace the object's texture at runtime |
| `obj:setColor` | `(r, g, b, a?)` | — | Tint the object (0–255 per channel) |
| `obj:setPixelShader` | `(shaderName: string, varValues?: table)` | bool | Apply a pixel shader |
| `obj:setVertexShader` | `(shaderName: string, varValues?: table)` | bool | Apply a vertex shader |
| `obj:getShader` | `()` | table | Get current shader config (`name`, `var` values) |
| `obj:setBlend` | `(srcBlend, dstBlend, op?)` | — | Set blend mode using `mbm.*` blend constants |
| `obj:getBlend` | `()` | srcBlend, dstBlend, op | Get current blend mode |

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

local label = fnt:add("Hello World", x, y, z?)   -- returns a text object
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
local sh = shape:new("2dw", x, y)
-- Indexed shape: vertices, indices, UVs
sh:create(vertices, indices, uvs)
-- Example: triangle
sh:create({0,0, 0,100, 100,0},  -- flat vertex list (x,y pairs)
          {1,2,3},               -- index list (1-based)
          {0,0, 0,1, 1,0})       -- UV list

-- Or create with normals/depth for 3D:
sh:createIndexed(vertices, indices, uvs, normals?)
sh:createDynamicIndexed(vertices, indices, uvs)  -- updatable each frame
sh:onRender(callback)  -- callback(sh) called every frame for dynamic update
```

### 7.9 line

```lua
local ln = line:new("2dw", x, y)
ln:add({x1,y1, x2,y2, x3,y3})   -- add line strip vertices
ln:set(idx, {x1,y1, x2,y2})     -- update a specific segment
ln:size()                         -- number of line segments
ln:setColor(r, g, b, a?)         -- line color (0-255)
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
local changed, v = tImGui.Checkbox("label", bool_value)

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
local c, r,g,b     = tImGui.ColorEdit3("label", r, g, b)          -- 0.0–1.0
local c, r,g,b,a   = tImGui.ColorEdit4("label", r, g, b, a)
local c, r,g,b     = tImGui.ColorPicker3("label", r, g, b)
local c, r,g,b,a   = tImGui.ColorPicker4("label", r, g, b, a)

-- Combo / Listbox
local c, idx = tImGui.Combo("label", currentIdx, {"item1","item2",...})
local c, idx = tImGui.ListBox("label", currentIdx, {"item1","item2",...}, h?)

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
tImGui.GetItemRectMin()      -- returns x, y (top-left of last item)
tImGui.GetItemRectMax()      -- returns x, y (bottom-right)
tImGui.GetItemRectSize()     -- returns w, h
tImGui.SetTooltip("text")    -- tooltip on hover
tImGui.PushID("id")          tImGui.PopID()
tImGui.GetWindowSize()       -- returns w, h
tImGui.GetWindowPos()        -- returns x, y
tImGui.SetScrollHereY(0.5)   -- scroll to position
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
