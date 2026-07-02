--[[
--------------------------------------------------------------------------------
| mini-mbm smoke-test scene template                                          |
|                                                                              |
| Minimal Lua scene for agent-driven verification of an engine/Lua feature.   |
| Exits on its own after EXIT_AFTER_SECONDS so the process never blocks a     |
| test run waiting for a key press or window close.                          |
|                                                                              |
| Run with (see engine-testing skill for the full flag list):                |
|   ./mini-mbm --scene smoke-test.lua --disable_select_monitor --nosplash    |
--------------------------------------------------------------------------------
]]--

local script_dir = ...
if script_dir then mbm.addPath(script_dir) end

local EXIT_AFTER_SECONDS = 5
local start_time = nil

function onInitScene()
    mbm.setColor(20, 20, 40)
    print("info", "green", "smoke-test onInitScene: replace this body with the feature under test")

    -- --- Put the feature you're testing here ---
    -- local s = sprite:new("2dw", 0, 0)
    -- s:load("assets/sprites/player.spt")

    start_time = mbm.getTimeRun()
end

function onLoop(delta)
    -- --- Per-frame assertions / feature exercise go here ---

    if start_time and (mbm.getTimeRun() - start_time) >= EXIT_AFTER_SECONDS then
        print("info", "green", "smoke-test: EXIT_AFTER_SECONDS reached, quitting")
        mbm.quit()
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode("ESC") then
        mbm.quit()
    end
end
