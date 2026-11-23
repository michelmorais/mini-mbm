--[[
-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------|

   Attachments aim/look-at smoke test.
]]--

local attachments = require 'attachments'
local source
local target
local relation
local started
local checked = false

local function fail(message)
    error('attachments aim smoke failed: ' .. message)
end

local function closeEnough(a, b)
    return math.abs(a - b) < 0.02
end

function onInitScene()
    started = mbm.getTimeRun()
    mbm.addPath('src/test-lib')
    source = mesh:new('3d')
    target = mesh:new('3d')
    if not source:load('Lorekeeper-walk.msh') or not target:load('Lorekeeper-walk.msh') then
        fail('could not load mesh fixtures')
    end
    source:setPos(0, 0, 0)
    target:setPos(0, 0, 20)
end

function onLoop()
    if not relation then
        local created, errorMessage = attachments.attach {
            source = source,
            target = target,
            inherit = {position = false, rotation = true, scale = false},
            aimAtSource = true,
            offset = {
                position = {x = 0, y = 0, z = 0},
                rotation = {x = 0, y = 0, z = 0},
                scale = {x = 1, y = 1, z = 1},
            },
        }
        if not created then fail(errorMessage or 'could not create aim attachment') end
        relation = created
    elseif not checked then
        local angle = target:getAngle()
        if not angle or not closeEnough(math.abs(angle.x), math.pi) or
                not closeEnough(math.abs(angle.z), math.pi) then
            fail(string.format('unexpected look-at angle: %.4f, %.4f, %.4f', angle.x, angle.y, angle.z))
        end
        checked = true
        mbm.quit()
    end
    if started and mbm.getTimeRun() - started > 5 then fail('timed out') end
end
