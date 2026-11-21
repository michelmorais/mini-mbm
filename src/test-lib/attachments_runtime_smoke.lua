--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|-------------------------------------------------------------------------------------------------------------------------|

   Attachments runtime smoke test.

   Run with a Lua-enabled engine build and the attachments plugin enabled:
   mini-mbm --scene src/test-lib/attachments_runtime_smoke.lua --disable_select_monitor --nosplash
]]--

local attachments = require 'attachments'
local source = shape:new('2dw', 0, 0)
local target = shape:new('2dw', 0, 0)
local chain = shape:new('2dw', 0, 0)
source:create('rectangle', 20, 20)
target:create('rectangle', 10, 10)
chain:create('rectangle', 5, 5)

local first = assert(attachments.attach {
    id = 'smoke_first', source = source, target = target,
    inherit = {position = true, rotation = true, scale = true},
    offset = {
        position = {x = 2, y = 0, z = 0},
        rotation = {x = 0, y = 0, z = 0},
        scale = {x = 2, y = 2, z = 2},
    },
})
local chainAttachment = assert(attachments.attach {
    id = 'smoke_chain', source = target, target = chain,
    inherit = {position = true, rotation = false, scale = false},
    offset = {position = {x = 1, y = 0, z = 0}},
})
assert(not attachments.attach {source = source, target = chain, inherit = {position = true}})
assert(not attachments.attach {source = target, target = source, inherit = {position = true}})

local started = nil
local complete = false
local destroyed = false

function onInitScene()
    started = mbm.getTimeRun()
    chain:setScale(7, 8, 9)
    chain:setAngle(0, 0, 0.25)
    source:setScale(3, 3, 3)
    source:setAngle(0, 0, math.pi / 2)
    source:setPos(10, 20)
end

function onLoop()
    if not complete then
        assert(math.abs(target.x - 10) < 0.01)
        assert(math.abs(target.y - 26) < 0.01)
        assert(math.abs(target.sx - 6) < 0.01)
        assert(math.abs(chain.x - 10) < 0.01)
        assert(math.abs(chain.y - 27) < 0.01)
        assert(math.abs(chain.sx - 7) < 0.01)
        assert(math.abs(chain.az - 0.25) < 0.01)
        local effective = assert(first:getEffectivePose())
        assert(math.abs(effective.position.x - 10) < 0.01)
        assert(math.abs(effective.position.y - 26) < 0.01)
        assert(math.abs(effective.scale.x - 6) < 0.01)
        local state = first:getState()
        assert(state.effectivePose and math.abs(state.effectivePose.position.y - 26) < 0.01)
        local x, y, z = target.x, target.y, target.z
        assert(first:detach())
        assert(math.abs(target.x - x) < 0.01)
        assert(math.abs(target.y - y) < 0.01)
        assert(math.abs(target.z - z) < 0.01)
        complete = true
    elseif not destroyed then
        chain:destroy()
        destroyed = true
    else
        assert(not chainAttachment:isActive())
        mbm.quit()
    end
    if started and mbm.getTimeRun() - started > 2 then mbm.quit() end
end
