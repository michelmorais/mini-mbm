# mini-mbm Lua Game — Copilot Instructions

## What is mini-mbm

mini-mbm is a lightweight, cross-platform 2D/3D game engine written in C++17. Games are written in **Lua 5.4** using the `mbm` namespace. This project is a game built on top of mini-mbm.

**This project contains only Lua scripts and asset files. The engine executable and shared libraries are provided externally.**

Run the game with:
```sh
./mini-mbm main.lua
```

---

## Project Structure Convention

```
<game-folder>/
├── .github/
│   └── copilot-instructions.md   ← Copilot context
├── AGENTS.md                     ← Codex context
├── main.lua                      ← entry point (or pass any .lua to the exe)
├── assets/                       ← sprites, fonts, textures, sounds, etc.
│   ├── sprites/
│   ├── fonts/
│   └── sounds/
└── scenes/                       ← additional scene Lua files (optional)
```

---

## Scene Lifecycle

The engine calls these Lua globals in your script. Define any you need:

| Function | When | Notes |
|---|---|---|
| `onInitScene()` | Once at load | Load assets, set camera, initialize state |
| `onLoop(delta)` | Every frame | `delta` = seconds since last frame. All game logic here. |
| `onTouchDown(key, x, y)` | Mouse/touch press | `x,y` in screen pixels |
| `onTouchUp(key, x, y)` | Mouse/touch release | |
| `onTouchMove(key, x, y)` | Mouse/touch move | |
| `onTouchZoom(zoom)` | Scroll wheel | `zoom` = +1 or -1 |
| `onKeyDown(key)` | Key pressed | `key` is int — use `mbm.getKeyCode("name")` |
| `onKeyUp(key)` | Key released | |
| `onKeyDownJoystick(player, key)` | Gamepad button | `player` = 0-based |
| `onKeyUpJoystick(player, key)` | Gamepad released | |
| `onMoveJoystick(player, lx, ly, rx, ry)` | Analog stick | Values in [-1, 1] |
| `onInfoDeviceJoystick(player, maxBtn, name, extra)` | Gamepad connected | |

**The per-frame callback is `onLoop(delta)`.**

---

## Coordinate Systems

Every render object constructor takes a coordinate-system string as first argument:

| String | System | Use for |
|---|---|---|
| `"2dw"` | 2D World | Game objects; origin at screen center; camera-affected |
| `"2ds"` | 2D Screen | HUD / UI; origin at top-left; no camera movement |
| `"3d"` | 3D World | 3D objects |

```lua
local wx, wy       = mbm.to2dw(sx, sy)       -- screen px → 2D world
local sx, sy       = mbm.to2ds(wx, wy)       -- 2D world → screen px
local wx, wy, wz   = mbm.to3d(sx, sy, depth) -- screen px → 3D world
```

---

## mbm Namespace — Full API Reference

### Scene & Control

```lua
mbm.quit()                          -- exit the application
mbm.pause()                         -- pause engine time (delta=0); scene loop/render continue, audio optional
mbm.resume()                        -- resume
mbm.loadScene("other.lua")          -- load a different scene file
mbm.getSceneName()                  -- → string: current scene filename
mbm.getFps(real?)                   -- → number: current FPS
mbm.getTimeRun()                    -- → number: seconds since engine start
mbm.setFakeFps(cycles, fps)         -- force fixed FPS (testing)
mbm.onErrorStop(bool)               -- halt on first Lua error if true
mbm.pauseAudioOnPauseGame(bool)     -- whether pause() also pauses audio
mbm.doCommands(cmd, param)          -- send native platform command
```

### Display

```lua
mbm.setColor(r, g, b)           -- background clear color (0.0-1.0)
mbm.enableClearScreen(bool)     -- toggle back-buffer clear each frame
mbm.refresh()                   -- force window redraw
mbm.getRealSizeScreen()         -- → w, h  (actual framebuffer pixels)
mbm.getSizeScreen()             -- → w, h  (logical/scaled size)
mbm.getDisplayMetrics()         -- → table with dpi/density
mbm.getObjectsRendered(type?)   -- → number of rendered objects
mbm.enableTextureFilter(bool)   -- bilinear texture filtering
```

### Camera

```lua
local cam = mbm.getCamera("2d")   -- or "3d"
cam.x, cam.y, cam.z = 0, 0, 0    -- position (read/write)
cam.fx, cam.fy, cam.fz = 0, 0, 0 -- focus/look-at (read/write)
cam.ux, cam.uy, cam.uz = 0, 1, 0 -- up vector (read/write)
cam:setPos(x, y, z?)
cam:getPos()          -- → vec3
cam:setFocus(x, y, z?)
cam:getFocus()        -- → vec3
```

### Asset Paths

```lua
mbm.addPath(path)           -- add directory to asset search list
mbm.getPathEngine()         -- → string: engine base path
mbm.getFullPath(file)       -- → string|nil: resolved full path
mbm.getAllPaths()            -- → table: all search paths
mbm.include("util.lua")     -- execute another Lua file (searched via paths)
```

### File System

```lua
mbm.existFile(name)                     -- → bool, fullPath|nil
mbm.listFiles(path, recursive?)         -- → table: {separator, {path, file...}, ...}
mbm.openFile(title, filter)             -- → string|nil  (native dialog)
mbm.openMultiFile(title, filter)        -- → table|nil
mbm.saveFile(title, filter)             -- → string|nil
mbm.openFolder(prompt?, default?)       -- → string|nil
mbm.compress(fileIn, fileOut?, level?)  -- → bool
mbm.decompress(fileIn, fileOut?)        -- → bool
```

### Input

```lua
mbm.getKeyCode("LEFT")      -- → int key code
mbm.getKeyCode("RIGHT")
mbm.getKeyCode("UP")
mbm.getKeyCode("DOWN")
mbm.getKeyCode("ESC")
mbm.getKeyCode("ENTER")
mbm.getKeyCode("SPACE")
mbm.getKeyCode("CTRL")    mbm.getKeyCode("ALT")    mbm.getKeyCode("SHIFT")
mbm.getKeyCode("A") -- ... "Z"
mbm.getKeyCode("0") -- ... "9"
mbm.getKeyCode("F1") -- ... "F12"
mbm.getKeyName(code)        -- → string: human-readable name
mbm.isCapitalKeyOn()        -- → bool
mbm.addOnTouch(obj, cb)     -- per-object touch callback
```

### Textures

```lua
mbm.createTexture(pixels, w, h, channels, name?, savePath?)  -- → string name | nil
mbm.existTexture(name)       -- → bool
mbm.loadTexture(file, alpha?) -- → textureInfo table
```

### Global Variables (persists across scene loads)

```lua
mbm.setGlobal(name, value)
mbm.getGlobal(name)     -- → value
mbm.clearGlobals()
```

### Shaders

```lua
mbm.getShaderList(detail?, filter?, min?, max?, code?)  -- → table
mbm.existShader(name)                                   -- → bool
mbm.addShader({
    name = "effect.ps",              -- .ps = pixel shader, .vs = vertex shader
    code = "void main(){...}",
    var  = { myAlpha = {1.0} },      -- uniform default value
    min  = { myAlpha = {0.0} },
    max  = { myAlpha = {1.0} },
})                                   -- → bool
mbm.getParticleShaderCode()          -- → string|nil (built-in particle GLSL)
mbm.sortShader()
```

### Dialogs

```lua
mbm.messageBox(title, msg)
mbm.inputBox(title, default?)       -- → string|nil
mbm.inputPassword(title)            -- → string|nil (masked)
mbm.colorDialog()                   -- → r, g, b  (0.0-1.0) | nil
```

### System Info

```lua
mbm.is("Windows")    -- → bool
mbm.is("Linux")
mbm.is("MacOs")
mbm.is("Android")
mbm.is("iOS")
mbm.get("version")          -- full version string
mbm.get("mbm")              -- engine version
mbm.get("lua")              -- Lua version
mbm.get("backend_engine")   -- "OpenGL ES", "Metal", "Directx9"
mbm.get("debug")            -- → bool
mbm.get("exe")              -- executable name
mbm.getIdiom()              -- → OS language: "en", "pt_br", etc.
mbm.getUserName()           -- → OS username
```

### Encryption

```lua
mbm.encrypt(fileIn, fileOut?, password?, iv?)   -- → bool
mbm.decrypt(fileIn, fileOut?, password?, iv?)   -- → bool
mbm.shuffle(msg, key)       -- Vigenère encrypt → string
mbm.undoShuffle(msg, key)   -- Vigenère decrypt → string
```

### Misc

```lua
mbm.setMinMaxWindowSize(minX, minY, maxX, maxY)
mbm.getSplash()                           -- → renderizable|nil
mbm.subscribe(pluginUserdata)             -- → int plugin index
mbm.executeInThread(func)                 -- run function in background thread
```

---

## Render Types — Constructors

```lua
-- Pattern: TypeName:new(coordType, x?, y?, z?)
local s  = sprite:new("2dw", 0, 0)
local m  = mesh:new("3d", 0, 0, -500)
local t  = texture:new("2dw", 100, 100)
local g  = gif:new("2dw", 0, 0)
local bg = backGround:new("2dw")
local p  = particle:new("2dw", 0, 0)
local sh = shape:new("2dw", 0, 0)
local ln = line:new("2dw", 0, 0)
-- font is different — no coord type, no position:
local fnt = font:new("roboto.fnt", height?, spaceW?, spaceH?, savePng?)
```

**Asset formats:**
- `sprite` → `.spt`
- `mesh` → `.msh`
- `texture` → `.png`, `.jpg`, `.bmp`, etc.
- `gif` → `.gif`
- `font` → `.fnt` (pre-parsed binary font, created with the font_maker editor) or `.ttf`/`.otf`/`.true-font` (runtime parsed by engine)
- `particle` → `.ptl` config file or configure manually
- `backGround` / `shape` / `line` → procedural or load from file

---

## Common Renderizable Methods

Every render object inherits all of these:

### Transform

```lua
obj:setPos(x, y, z?)        -- set world position
obj:getPos()                -- → vec3
obj:setAngle(ax, ay?, az?)  -- rotation in radians
obj:getAngle()              -- → vec3
obj:setScale(sx, sy?, sz?)  -- scale (1.0 = original)
obj:getScale()              -- → vec3
obj:move(vx, vy, vz?)       -- delta-scaled relative move
obj:rotate("z", radians)    -- delta-scaled rotation on axis "x","y","z"
```

### Direct Property Access (no method call)

```lua
obj.x, obj.y, obj.z         -- position
obj.sx, obj.sy, obj.sz      -- scale
obj.ax, obj.ay, obj.az      -- angle (radians)
obj.visible = false         -- hide/show (default true)
obj.alwaysRender = true     -- render even when off-screen
```

### Size & Collision

```lua
obj:getSize(considerScale?) -- → w, h  (or w, h, d for 3D)
obj:getAABB(update?)        -- → w, h  (AABB dimensions)
obj:isOnScreen()            -- → bool
obj:isLoaded()              -- → bool
obj:collide(other)          -- → bool (AABB vs AABB)
obj:collide(x, y)           -- → bool (AABB vs screen point)
obj:isOver(x, y)            -- → bool (point inside bounding rect)
obj:getPhysics()            -- → table of {type,x,y,z,width,height,...}
```

### Animation & Shaders

```lua
obj:setAnim("run")          -- play animation by name (or pass index)
obj:getAnim()               -- → name, index
obj:getTotalAnim()          -- → total animation count
obj:restartAnim()           -- restart from frame 1
obj:setPixelShader(name, vars?)   -- attach pixel shader
obj:setVertexShader(name, vars?)  -- attach vertex shader
obj:getShader()             -- → {name, var = {...}} table
obj:setBlend(src, dst, op?) -- blend mode via mbm.* constants
-- Depth: use obj.z property (no setOrder/getOrder methods)
-- obj.z = 10   -- higher z = renders on top
```

### Destroy

```lua
obj:destroy()   -- remove from scene immediately
obj = nil       -- also assign nil to prevent further access
```

---

## sprite — Load & Animate

```lua
local s = sprite:new("2dw", 0, 0)
s:load("hero.spt")          -- returns bool
s:setAnim("walk")           -- play animation by name
s.visible = false           -- hide
```

## mesh — 3D Model

```lua
local m = mesh:new("3d", 0, 0, -300)
m:load("enemy.msh")
m:setAngle(0, 0.5, 0)      -- face a direction
```

## texture — Image Quad

```lua
local t = texture:new("2dw", 200, 100)
t:load("coin.png")
t:setScale(2, 2)
```

## gif — Animated GIF

```lua
local g = gif:new("2ds", 10, 10)   -- screen-space HUD GIF
g:load("loading.gif")
```

## backGround — Background/Foreground

```lua
local bg = backGround:new("2dw")
bg:load("sky.jpg")
bg:setFront("clouds.png")       -- 2D foreground layer
-- bg:setFront3d("trees.mbm")   -- 3D foreground mesh
```

## font — Text Rendering

```lua
local fnt = font:new("ui.fnt")
local lbl = fnt:add("Hello!", 0, 200)  -- creates text object at (0, 200)
lbl.visible = true
lbl.text    = "Score: 999"          -- update text
lbl.x       = -100                  -- reposition

fnt:setSpace(charW, lineH)      -- adjust spacing
fnt:getHeight()                 -- → number
fnt:setSizeLetter(size)
```

## particle — Particle System

```lua
local fire = particle:new("2dw", 0, -100)
fire:load("fire.ptl")           -- from config file, OR configure:
fire:setMinOffset(  -5,  0, 0)
fire:setMaxOffset(   5, 10, 0)
fire:setMinDirection(-1,  5, 0)
fire:setMaxDirection( 1, 20, 0)
fire:setMinColor(255, 100,   0, 200)
fire:setMaxColor(255, 200, 100, 255)
fire:setMinLifeTime(0.3)
fire:setMaxLifeTime(0.8)
fire:setTotalParticle(100)
fire:add(0, 0, 0)              -- emit one particle at given position
fire:getTotalAlive()           -- → int
```

## shape — Procedural Geometry

```lua
local sh = shape:new("2dw", 0, 0)
-- vertices: flat list of x,y pairs
-- indices:  triangle list (1-based)
-- uvs:      flat list of u,v pairs matching vertices
sh:create(
    {0,0,  0,100,  100,100,  100,0},   -- 4 vertices (quad)
    {1,2,3,  1,3,4},                   -- 2 triangles
    {0,0,  0,1,  1,1,  1,0}            -- UVs
)
sh:setPixelShader("tinted.ps", {color={1,0,0,1}})
```

## line — Line Drawing

```lua
local ln = line:new("2dw", 0, 0)
ln:add({0,0, 100,0, 100,100, 0,100, 0,0})  -- closed square
ln:setColor(1.0, 1.0, 0.0)                  -- yellow
-- update a segment:
ln:set(1, {0,0, 200,0})
```

## tile — Tile Map

```lua
local tmap = tile:new("2dw")
tmap:load("level1.tmx")     -- Tiled TMX or engine format
-- Use the tiled plugin for advanced operations:
-- local tiled = require "tiled"
```

---

## vec2 / vec3 Math Objects

```lua
local v = vec2:new(x, y)
local v = vec3:new(x, y, z)

v.x, v.y, v.z             -- read/write components
v:get()                   -- → x, y, z
v:set(x, y, z?)
v:length()                -- → number
v:normalize()             -- → normalized vec copy
v:dot(other)              -- → number
v:cross(other)            -- → vec3 (3D only)
v:distance(other)         -- → number
local r = v + other       -- → new vec
local r = v - other
local r = v * scalar
local r = v / scalar
```

---

## mbm Constants

```lua
-- Animation status (obj.anim.status)
mbm.PAUSED            -- 0: stopped
mbm.GROWING           -- playing forward, stops at end
mbm.GROWING_LOOP      -- playing forward, loops
mbm.DECREASING        -- playing backward, stops
mbm.DECREASING_LOOP   -- playing backward, loops
mbm.RECURSIVE         -- ping-pong, stops
mbm.RECURSIVE_LOOP    -- ping-pong, loops

-- Blend factors (use with obj:setBlend)
mbm.DISABLE  mbm.ZERO  mbm.ONE
mbm.SRC_COLOR       mbm.INV_SRC_COLOR
mbm.SRC_ALPHA       mbm.INV_SRC_ALPHA
mbm.DEST_ALPHA      mbm.INV_DEST_ALPHA
mbm.DEST_COLOR      mbm.INV_DEST_COLOR

-- Blend equations (third arg to obj:setBlend)
mbm.ADD  mbm.SUBTRACT  mbm.REVERSE_SUBTRACT  mbm.MIN  mbm.MAX

-- Particle stages
mbm.STAGE_1  mbm.STAGE_2
```

---

## Plugin: box2d — 2D Physics

```lua
require "box2d"
local world = box2d:new()
world:setGravity(0, -10)

-- Bodies are linked to renderizables:
world:addStaticBody(ground_sprite)
world:addDynamicBody(player_sprite, density?, friction?, restitution?)
world:addKinematicBody(platform_sprite)

-- Physics advances automatically through the plugin's engine callback.
-- Use world:pause() and world:start() to control simulation.

-- Collision callbacks:
world:setContactListener(
    function(bodyA, bodyB, manifold) end,  -- onBegin
    function(bodyA, bodyB) end,            -- onEnd
    function(bodyA, bodyB, manifold) end,  -- onPreSolve  (optional)
    function(bodyA, bodyB, impulse)  end   -- onPostSolve (optional)
)

world:getWorldManifolds(body)  -- → contact manifold table
-- The world is released with its Lua scene/userdata; no explicit destroy method is registered.
```

---

## Plugin: ImGui — Immediate-Mode UI

```lua
local tImGui = require "ImGui"

-- ALL ImGui calls must be inside onLoop(delta):
function onLoop(delta)
    local open = tImGui.Begin("My Window", false,
        tImGui.Flags("ImGuiWindowFlags_MenuBar"))
    if open then
        -- Menu bar
        if tImGui.BeginMenuBar() then
            if tImGui.BeginMenu("File") then
                if tImGui.MenuItem("Quit") then mbm.quit() end
                tImGui.EndMenu()
            end
            tImGui.EndMenuBar()
        end

        -- Widgets
        tImGui.Text("Hello from ImGui!")
        local changed, val = tImGui.SliderFloat("Speed", speed, 0, 500)
        if changed then speed = val end
        if tImGui.Button("Reset") then speed = 100 end
        tImGui.SameLine()
        tImGui.Text("Speed: " .. tostring(speed))

        -- Color picker
        local c, r, g, b = tImGui.ColorEdit3("BG Color", bg_r, bg_g, bg_b)
        if c then bg_r, bg_g, bg_b = r, g, b end
    end
    tImGui.End()
end
```

**Window flags:** `NoMove`, `NoResize`, `NoTitleBar`, `NoScrollbar`,
`MenuBar`, `AlwaysAutoResize`, `NoBringToDisplayOnFocus`

**Layout helpers:**
```lua
tImGui.SetNextWindowPos(x, y, "Always"|"Once"|"Appearing")
tImGui.SetNextWindowSize(w, h)
tImGui.SameLine()  tImGui.Separator()  tImGui.Spacing()  tImGui.NewLine()
tImGui.Columns(n)  tImGui.NextColumn()
tImGui.PushItemWidth(w)  tImGui.PopItemWidth()
```

**Full widget list:**
`Text`, `TextColored`, `TextWrapped`, `Button`, `SmallButton`, `Checkbox`,
`SliderFloat`, `SliderInt`, `SliderFloat2`, `SliderFloat3`, `SliderFloat4`,
`DragFloat`, `DragInt`, `InputText`, `InputTextMultiline`, `InputFloat`, `InputInt`,
`ColorEdit3`, `ColorEdit4`, `ColorPicker3`, `ColorPicker4`,
`Combo`, `ListBox`, `TreeNode`, `TreePop`, `CollapsingHeader`,
`Image`, `ImageButton`, `OpenPopup`, `BeginPopup`, `EndPopup`,
`BeginPopupModal`, `BeginTable`, `TableSetupColumn`, `TableHeadersRow`,
`TableNextRow`, `TableSetColumnIndex`, `EndTable`

**Query last item:**
```lua
tImGui.IsItemHovered()
tImGui.IsItemClicked(btn?)
tImGui.IsItemActive()
tImGui.GetItemRectMin()   -- → x, y
tImGui.GetItemRectMax()   -- → x, y
tImGui.GetWindowSize()    -- → w, h
tImGui.GetWindowPos()     -- → x, y
tImGui.SetTooltip("text")
tImGui.PushID("id")  tImGui.PopID()
```

---

## Plugin: lsqlite3 — SQLite3

```lua
sqlite3 = require "lsqlite3"
local db = sqlite3.open("save.db")   -- or ":memory:"
db:execute("CREATE TABLE IF NOT EXISTS save (key TEXT, value TEXT)")
db:execute(string.format("INSERT INTO save VALUES ('%s','%s')", k, v))
for row in db:nrows("SELECT * FROM save") do
    print(row.key, row.value)
end
local stmt = db:prepare("SELECT value FROM save WHERE key = ?")
stmt:bind(1, "level")
for row in stmt:nrows() do print(row.value) end
stmt:finalize()
db:close()
```

---

## print() — Colored Output

```lua
print("Hello")                          -- plain white
print("info",  "green",  "Loaded!")     -- [INFO ] green
print("warn",  "yellow", "Low memory")  -- [WARN ] yellow
print("error", "red",    "Failed!")     -- [ERROR] red
print("line",  "message")               -- adds file:line prefix
-- Colors: "white","red","yellow","green","blue","magenta","cian"
```

---

## Common Patterns

### Pattern: Player with arrow key movement

```lua
local player
local speed = 300
local keys = {}

function onInitScene()
    player = sprite:new("2dw", 0, 0)
    player:load("player.spt")
end

function onLoop(delta)
    if keys[mbm.getKeyCode("LEFT")]  then player.x = player.x - speed * delta end
    if keys[mbm.getKeyCode("RIGHT")] then player.x = player.x + speed * delta end
    if keys[mbm.getKeyCode("UP")]    then player.y = player.y + speed * delta end
    if keys[mbm.getKeyCode("DOWN")]  then player.y = player.y - speed * delta end
end

function onKeyDown(k) keys[k] = true  end
function onKeyUp(k)   keys[k] = nil   end
```

### Pattern: Click-to-move (touch input)

```lua
function onTouchDown(key, sx, sy)
    if key == 0 then                         -- left button
        local wx, wy = mbm.to2dw(sx, sy)    -- screen → world
        player:setPos(wx, wy)
    end
end
```

### Pattern: Collision between two objects

```lua
function onLoop(delta)
    if player:collide(enemy) then
        player.visible = false
        mbm.loadScene("game-over.lua")
    end
end
```

### Pattern: HUD score counter (screen space)

```lua
local score = 0
local hud_font, hud_label

function onInitScene()
    hud_font  = font:new("ui.fnt")
    hud_label = hud_font:add("Score: 0", "2ds", -300, 250)  -- 2nd arg is the coord type, required
end

function onLoop(delta)
    hud_label.text = "Score: " .. tostring(score)
end
```

### Pattern: Camera follow player

```lua
function onLoop(delta)
    local cam = mbm.getCamera("2d")
    cam.x = player.x
    cam.y = player.y
end
```

### Pattern: Scene loading

```lua
-- Load another scene (transition to next level)
mbm.loadScene("level2.lua")

-- Pass data via globals
mbm.setGlobal("score", score)
mbm.loadScene("results.lua")
-- In results.lua:
-- local score = mbm.getGlobal("score")
```

### Pattern: Box2D physics

```lua
require "box2d"
local world, ground_body, player_body

function onInitScene()
    world = box2d:new()
    world:setGravity(0, -500)

    local ground = shape:new("2dw", 0, -250)
    ground:create({-400,0, -400,20, 400,20, 400,0}, {1,2,3, 1,3,4}, {0,0,0,1,1,1,1,0})
    world:addStaticBody(ground)

    local ball = sprite:new("2dw", 0, 200)
    ball:load("ball.spt")
    world:addDynamicBody(ball, 1.0, 0.3, 0.5)
end

-- No onLoop step is required; the plugin advances physics automatically.
```

---

## Pixel Shader Translation — GLSL ES / HLSL / MSL

### Pattern: multi-backend shader selection

Use `mbm.get('backend_engine')` to pick the right code at runtime.
Possible values: `'Directx9'`, `'Metal'`, or anything else (OpenGL ES).

```lua
local function getShaderCode()
    if mbm.get('backend_engine') == 'Directx9' then
        return [[ ... HLSL ... ]]
    elseif mbm.get('backend_engine') == 'Metal' then
        return [=[ ... MSL ... ]=]   -- MUST use [=[ ]=], NOT [[ ]]
    else
        return [[ ... GLSL ES ... ]]
    end
end

local tShader = { name = 'effect.ps', code = getShaderCode(), var = {}, min = {}, max = {} }
mbm.addShader(tShader)
```

---

### GLSL ES → HLSL (DirectX9)

| GLSL ES | HLSL |
|---|---|
| `uniform sampler2D TextureDiffuse;` | `sampler2D TextureDiffuse : register(s0);` |
| `uniform float x;` | `float x;` (global) |
| `uniform vec2 v;` | `float2 v;` (global) |
| `varying vec2 vTexCoord` | `float2 vTexCoord : TEXCOORD0;` in `PS_INPUT` struct |
| `texture2D(TextureDiffuse, uv)` | `tex2D(TextureDiffuse, uv)` |
| `gl_FragColor = color;` | `return color;` with `: COLOR0` return semantic |
| `vec2`, `vec4` | `float2`, `float4` |
| `void main()` | `float4 main(PS_INPUT input) : COLOR0` |

Full HLSL skeleton:

```hlsl
sampler2D TextureDiffuse : register(s0);
float     myUniform;
float2    myVec;

struct PS_INPUT { float2 vTexCoord : TEXCOORD0; };

float4 main(PS_INPUT input) : COLOR0
{
    float4 color = tex2D(TextureDiffuse, input.vTexCoord);
    // ... logic ...
    return color;
}
```

---

### GLSL ES → MSL (Metal)

#### Critical Lua gotcha
Metal attribute syntax `[[position]]`, `[[stage_in]]`, etc. contains `]]`
which terminates a Lua `[[ ]]` long string early.
**Always use `[=[ ]=]` for Metal shader strings.**

#### Engine entry point names
- Fragment: **`frag_main`** (required by the engine)
- Vertex:   **`vert_main`** (required by the engine)

#### How the engine compiles custom pixel shaders
The engine stores only the **fragment function** in the shader source string.
At compile time it **prepends** an auto-generated vertex preamble that includes
`#include <metal_stdlib>`, `using namespace metal;`, and the `VOut` struct.
**Do NOT include these yourself** — it causes duplicate-symbol errors.

#### Vertex input struct — use `VOut`, never define your own
The engine-generated preamble defines:
```metal
struct VOut {
    float4 pos [[position]];
    float2 uv;
};
```
The fragment function **must** use `VOut` (not `VertexOut` or any other name).
Access the texture coordinate as `in.uv`.

#### Uniforms — flat float array at `[[buffer(2)]]`
The engine packs all shader uniforms into a **single flat `float` array** and
binds it to `[[buffer(2)]]` (fragment) / `[[buffer(3)]]` (vertex).
**Do NOT declare a custom struct** — use `constant float* f [[buffer(2)]]`.

Uniforms are packed in **alphabetical order** of their Lua `var` table keys
(the engine uses `std::map`, which sorts keys lexicographically).

Example — `var = {ray={0.1}, center={0,0}, size_screen={w,h}}` is sorted as:
| Index | Name | Components |
|---|---|---|
| `f[0]`, `f[1]` | `center` | x, y |
| `f[2]` | `ray` | — |
| `f[3]`, `f[4]` | `size_screen` | x, y |

#### Texture sampling

```metal
texture2d<float> TextureDiffuse [[texture(0)]],
sampler          samp    [[sampler(0)]]
// usage:
float4 color = TextureDiffuse.sample(samp, in.uv);
```

#### Full MSL skeleton (fragment function only)

```metal
fragment float4 frag_main(VOut in [[stage_in]],
    texture2d<float> TextureDiffuse [[texture(0)]],
    sampler          samp    [[sampler(0)]],
    constant float*  f       [[buffer(2)]])
{
    float2 vTexCoord = in.uv;
    // Read uniforms by alphabetical index:
    // float2 center      = float2(f[0], f[1]);
    // float  ray         = f[2];
    // float2 size_screen = float2(f[3], f[4]);

    float4 color = TextureDiffuse.sample(samp, vTexCoord);
    // ... logic ...
    return color;
}
```

---

### Checklist when adding a new shader

- [ ] Wrap in `if not mbm.existShader('name.ps') then ... end`
- [ ] Use `[=[ ]=]` for the Metal shader string (never `[[ ]]`)
- [ ] Metal: provide **fragment function only** — no `#include`, no `using namespace`, no struct definitions
- [ ] Metal fragment entry point = `frag_main`
- [ ] Metal vertex input = `VOut in [[stage_in]]` (engine-defined struct, `in.uv` for tex coord)
- [ ] Metal uniforms = `constant float* f [[buffer(2)]]`; access by index in **alphabetical** key order
- [ ] HLSL uniforms declared as globals; input coord via `PS_INPUT` with `TEXCOORD0`
- [ ] GLSL ES uses `varying`, `uniform`, `texture2D()`, `gl_FragColor`
