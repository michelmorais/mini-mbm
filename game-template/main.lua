--[[
--------------------------------------------------------------------------------
| mini-mbm Lua Game Template                                                   |
|                                                                              |
| Copy this file to your game project and rename it main.lua (or any name     |
| you pass on the command line).                                               |
|                                                                              |
| Run with the mini-mbm executable:                                            |
|   ./mini-mbm main.lua                                                        |
--------------------------------------------------------------------------------
]]--

-- ---------------------------------------------------------------------------
-- Asset path setup
-- Add the folder containing this script (and your assets) to the search path.
-- ---------------------------------------------------------------------------
local script_dir = ...  -- engine passes the script directory as vararg
if script_dir then
    mbm.addPath(script_dir)
end

mbm.addPath("assets")  -- add "assets" subfolder to search path for loading assets
mbm.addPath("scenes")  -- add "scenes" subfolder to search path for loading scenes
mbm.addPath("scenes/fonts") -- add "scenes/fonts" subfolder to search path for loading fonts
mbm.addPath("scenes/sprites") -- add "scenes/sprites" subfolder to search path for loading sprites
mbm.addPath("scenes/tilesets") -- add "scenes/tilesets" subfolder to search path for loading tilesets
mbm.addPath("scenes/textures") -- add "scenes/textures" subfolder to search path for loading textures
mbm.addPath("scenes/sounds") -- add "scenes/sounds" subfolder to search path for loading sounds

-- ---------------------------------------------------------------------------
-- State — declare all your game objects here so they are accessible from
-- every lifecycle callback.
-- ---------------------------------------------------------------------------
local player     = nil    -- sprite
local background = nil    -- backGround
local score_font = nil    -- font
local score_text = nil    -- text object from font:add()
local score      = 0

local KEY_LEFT   = 0
local KEY_RIGHT  = 0
local KEY_UP     = 0
local KEY_DOWN   = 0
local KEY_ESCAPE = 0
local MOVE_SPEED = 300    -- pixels per second


-- ---------------------------------------------------------------------------
-- onInitScene()
-- Called ONCE when the scene first loads.
-- Load all assets, configure the camera, and set up initial state here.
-- ---------------------------------------------------------------------------
function onInitScene()
    -- Resolve key codes once at startup
    KEY_LEFT   = mbm.getKeyCode("LEFT")
    KEY_RIGHT  = mbm.getKeyCode("RIGHT")
    KEY_UP     = mbm.getKeyCode("UP")
    KEY_DOWN   = mbm.getKeyCode("DOWN")
    KEY_ESCAPE = mbm.getKeyCode("ESC")

    -- Background clear color (R, G, B — 0.0..1.0)
    mbm.setColor(30 / 255, 30 / 255, 50 / 255)

    -- --- Background ---
    -- background = backGround:new("2dw")
    -- background:load("sky.png")

    -- --- Player sprite ---
    -- player = sprite:new("2dw", 0, 0)
    -- player:load("player.spt")
    -- player:setScale(1.5, 1.5)

    -- --- HUD font (screen-space so it is not affected by camera movement) ---
    -- score_font must stay referenced for as long as score_text is used (a top-level `local`,
    -- same as here) -- once nothing in Lua references the font object anymore, its __gc deletes
    -- it AND every text object it created, even ones still referenced elsewhere (real crash,
    -- see docs/lua-api.md section 7.4).
    -- score_font = font:new("font.fnt")
    -- score_text = score_font:add("Score: 0", "2ds", -300, 250)   -- top-left area
    -- score_text.visible = true

    print("info", "green", "onInitScene complete")
end


-- ---------------------------------------------------------------------------
-- onLoop(delta)
-- Called EVERY FRAME.
-- delta = seconds since last frame (use it to make movement frame-rate
-- independent, e.g.  obj:move(speed * delta, 0)).
-- ---------------------------------------------------------------------------

-- Track which keys are currently held
local keys_held = {}

function onLoop(delta)
    -- Example: move player on held arrow keys
    if player then
        if keys_held[KEY_LEFT]  then player.x = player.x - MOVE_SPEED * delta end
        if keys_held[KEY_RIGHT] then player.x = player.x + MOVE_SPEED * delta end
        if keys_held[KEY_UP]    then player.y = player.y + MOVE_SPEED * delta end
        if keys_held[KEY_DOWN]  then player.y = player.y - MOVE_SPEED * delta end
    end

    -- Example: update HUD text
    -- if score_text then
    --     score_text.text = "Score: " .. tostring(score)
    -- end
end


-- ---------------------------------------------------------------------------
-- Input callbacks
-- ---------------------------------------------------------------------------

function onKeyDown(key)
    keys_held[key] = true
    if key == KEY_ESCAPE then
        mbm.quit()
    end
end

function onKeyUp(key)
    keys_held[key] = nil
end

-- Touch / mouse events — x, y in screen pixels; key = button index (0 = left)
function onTouchDown(key, x, y)
    -- Convert to world coords if needed:
    -- local wx, wy = mbm.to2dw(x, y)
end

function onTouchUp(key, x, y)
end

function onTouchMove(key, x, y)
end

function onTouchZoom(zoom)
    -- zoom = +1 (scroll up) or -1 (scroll down)
end

-- Gamepad callbacks
function onKeyDownJoystick(player_idx, key)
end

function onKeyUpJoystick(player_idx, key)
end

function onMoveJoystick(player_idx, lx, ly, rx, ry)
    -- lx, ly, rx, ry in [-1, 1]
end

function onInfoDeviceJoystick(player_idx, maxBtn, deviceName, extra)
    print("Joystick connected:", deviceName, "buttons:", maxBtn)
end
