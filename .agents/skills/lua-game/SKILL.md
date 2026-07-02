---
name: lua-game
description: "Step-by-step workflow for creating or scaffolding a new mini-mbm Lua game project. Use when: creating a new game project folder, scaffolding main.lua, asking how to use the Lua API for game logic, understanding the scene lifecycle, using box2d physics, ImGui menus, or font/sprite/particle APIs in a game context."
---

# Lua Game — mini-mbm

## When to Use

- Scaffolding a new game project (directory, `main.lua`, `.github/copilot-instructions.md`, `AGENTS.md`, `CLAUDE.md`)
- Answering "how do I do X in mini-mbm Lua?"
- Implementing game features: sprites, physics, HUD, input, particles, shaders
- Helping someone work in a **standalone game folder** that has no engine source

---

## Architecture Overview

A mini-mbm game project is a standalone folder containing only Lua scripts and asset files.
The engine binary and shared libraries (`box2d.so`, `ImGui.so`, etc.) are external.

```
<game-folder>/
├── .github/
│   └── copilot-instructions.md    ← API context auto-loaded by Copilot
├── AGENTS.md                      ← API context auto-loaded by Codex
├── CLAUDE.md                      ← symlink to AGENTS.md; auto-loaded by Claude Code
├── main.lua                       ← entry point
├── assets/
│   ├── sprites/                   ← .spt files
│   ├── fonts/                     ← bitmap font files
│   ├── textures/                  ← PNG, JPG, BMP
│   └── sounds/                    ← WAV, OGG, MP3
└── scenes/                        ← additional scene Lua files (optional)
```

Run with:
```sh
./mini-mbm main.lua
```

The `.github/copilot-instructions.md` and `AGENTS.md` files are the **game-project API context** for Copilot and Codex. They should be copied from `game-template/.github/copilot-instructions.md` and `game-template/AGENTS.md` in this engine repo. `CLAUDE.md` is a symlink to the game folder's own `AGENTS.md` — not a copy — so Claude Code sees the same content without a second file to keep in sync; `game-template/CLAUDE.md` itself follows the same pattern (symlinked to `game-template/AGENTS.md`) for anyone opening `game-template/` directly.

---

## Step 1 — Create the Game Project

When users ask to create a new game project at a path like `/home/michel/my-game`:

1. Create the directory structure above.
2. Copy `game-template/.github/copilot-instructions.md` to `<game-folder>/.github/copilot-instructions.md`.
3. Copy `game-template/AGENTS.md` to `<game-folder>/AGENTS.md`.
4. In `<game-folder>`, run `ln -s AGENTS.md CLAUDE.md` (do **not** copy — a symlink keeps Claude Code's context identical to Codex's without a second file to drift).
5. Copy `game-template/main.lua` to `<game-folder>/main.lua`.
6. Customize `main.lua` for the specific game concept.

---

## Step 2 — Scaffold main.lua

All game scripts follow this pattern:

```lua
-- Asset path: add directory of this script
local script_dir = ...
if script_dir then mbm.addPath(script_dir) end

-- State variables
local player = nil
local KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_ESC = 0,0,0,0,0
local speed = 300
local keys_held = {}

function onInitScene()
    KEY_LEFT  = mbm.getKeyCode("LEFT")
    KEY_RIGHT = mbm.getKeyCode("RIGHT")
    KEY_UP    = mbm.getKeyCode("UP")
    KEY_DOWN  = mbm.getKeyCode("DOWN")
    KEY_ESC   = mbm.getKeyCode("ESC")

    mbm.setColor(20, 20, 40)          -- background

    player = sprite:new("2dw", 0, 0)
    player:load("assets/sprites/player.spt")
end

function onLoop(delta)
    if keys_held[KEY_LEFT]  then player.x = player.x - speed * delta end
    if keys_held[KEY_RIGHT] then player.x = player.x + speed * delta end
    if keys_held[KEY_UP]    then player.y = player.y + speed * delta end
    if keys_held[KEY_DOWN]  then player.y = player.y - speed * delta end
end

function onKeyDown(k) keys_held[k] = true  end
function onKeyUp(k)   keys_held[k] = nil   end
function onTouchDown(key, x, y) end
function onTouchUp(key, x, y)   end
function onTouchMove(key, x, y) end
```

---

## Step 3 — Key Patterns Reference

### Lifecycle

| Callback | Purpose |
|---|---|
| `onInitScene()` | Load assets, init state — called once |
| `onLoop(delta)` | Per-frame — movement, AI, physics step |
| `onKeyDown(key)` / `onKeyUp(key)` | Keyboard input |
| `onTouchDown(key,x,y)` etc. | Mouse/touch input (x,y in screen pixels) |

### Coordinate Systems

- `"2dw"` — 2D world space (default for game objects). Origin at screen center.
- `"2ds"` — 2D screen space (HUD/UI). Origin at top-left.
- `"3d"` — 3D perspective.
- Helpers: `mbm.to2dw(sx,sy)`, `mbm.to2ds(wx,wy)`, `mbm.to3d(sx,sy,depth)`

### Asset Loading

Always load in `onInitScene()`, never in `onLoop()`.

```lua
-- Sprite
local s = sprite:new("2dw", x, y)
s:load("file.spt")             -- animation names defined in .spt

-- Texture
local t = texture:new("2dw", x, y)
t:load("image.png")

-- Font + text
local fnt  = font:new("ui.fnt")
local lbl  = fnt:add("Hello", x, y)
lbl.text   = "new text"         -- update any time

-- Particle
local p = particle:new("2dw", x, y)
p:load("fire.ptl")              -- or configure with setMin/Max* methods
```

### Common Object Properties (all render types)

```lua
obj.x, obj.y, obj.z          -- position (direct assignment)
obj.sx, obj.sy                -- scale
obj.ax, obj.ay, obj.az        -- angle in radians
obj.visible = false           -- show/hide
obj.alwaysRender = true       -- draw even off-screen

obj:setPos(x, y)
obj:setScale(sx, sy)
obj:setAngle(ax, ay, az)
obj:move(vx, vy)              -- delta-scaled (frame-rate independent)
obj:rotate("z", radians)      -- delta-scaled
obj:collide(other)            -- → bool  AABB vs AABB
obj:collide(x, y)             -- → bool  AABB vs screen point
obj:getSize()                 -- → w, h
obj:getAABB()                 -- → w, h
obj:isOnScreen()              -- → bool
obj:setAnim("animName")       -- play animation by name (or pass index)
obj:getAnim()                 -- → name, index
obj:getTotalAnim()            -- → total animation count
obj:restartAnim()             -- restart from frame 1
obj:destroy()                 -- remove from scene, then assign nil
```

### Camera

```lua
local cam = mbm.getCamera("2d")
-- Follow player:
function onLoop(delta)
    cam.x = player.x
    cam.y = player.y
end
```

### Scene Transitions

```lua
-- Simple transition
mbm.loadScene("level2.lua")

-- Pass data via globals
mbm.setGlobal("score", current_score)
mbm.loadScene("game-over.lua")
-- In game-over.lua: local s = mbm.getGlobal("score")
```

### Box2D Physics

```lua
local box2d = require "box2d"
local world

function onInitScene()
    world = box2d:new()
    world:setGravity(0, -500)
    world:addStaticBody(ground_sprite)
    world:addDynamicBody(player_sprite, 1.0, 0.3, 0.5)
    world:setContactListener(
        function(a, b) print("collision begin") end,  -- onBegin
        function(a, b) print("collision end") end     -- onEnd
    )
end

function onLoop(delta) world:step(delta) end
```

### ImGui for in-game menus / debug

```lua
local tImGui = require "ImGui"

function onLoop(delta)
    if tImGui.Begin("Pause Menu") then
        if tImGui.Button("Resume") then paused = false end
        if tImGui.Button("Quit")   then mbm.quit() end
    end
    tImGui.End()
end
```

---

## Step 4 — Checklist for New Game Project

- [ ] Game folder created
- [ ] `.github/copilot-instructions.md` copied from `game-template/`
- [ ] `AGENTS.md` copied from `game-template/`
- [ ] `CLAUDE.md` symlinked to `AGENTS.md` (`ln -s AGENTS.md CLAUDE.md`), not copied
- [ ] `main.lua` created with `onInitScene`, `onLoop`, key callbacks
- [ ] `mbm.addPath(script_dir)` at top so assets resolve correctly
- [ ] `mbm.setColor(r,g,b)` sets background
- [ ] All `sprite:new` / `font:new` / etc. done inside `onInitScene`
- [ ] `onLoop(delta)` uses `delta` for movement speed (frame-rate independent)
- [ ] Key codes resolved once in `onInitScene` via `mbm.getKeyCode()`

---

## Full API Reference

The complete API reference lives in `docs/lua-api.md` in this engine repo.  
Topics covered there but not repeated here:

- Full `mbm.*` namespace (~60 functions): shaders, file system, encryption, dialogs, textures
- All render type constructors and their type-specific methods
- `vec2`/`vec3` math object API
- All `mbm.*` constants (blend modes, animation states)
- Plugin APIs: box2d, box2dLiquidFun, ImGui (full widget list), lsqlite3
- `print()` colored output extension

---

## Common Mistakes to Avoid

| Mistake | Correct Approach |
|---|---|
| Calling `sprite:new()` inside `onLoop()` | Call in `onInitScene()` only |
| Hard-coding key integers | Use `mbm.getKeyCode("ESC")` |
| Moving with fixed pixel amounts | Multiply by `delta` for frame-rate independence |
| Using `"3"` as coord type | Use `"3d"` (the string `"3"` also works but is ambiguous) |
| Calling ImGui outside `onLoop()` | All ImGui calls must be inside `onLoop(delta)` |
| Using `obj:anim()` | The correct method is `obj:setAnim(name)` or `obj:setAnim(index)` |
| Using `obj:checkCollision()` | The correct method is `obj:collide(other)` or `obj:collide(x,y)` |
| Using `obj:isOnFrustum()` | The correct method is `obj:isOnScreen()` |
