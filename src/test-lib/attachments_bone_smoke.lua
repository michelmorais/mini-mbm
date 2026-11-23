--[[
-------------------------------------------------------------------------------------------------------------------------|
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
|------------------------------------------------------------------------------------------------------------------------|

   Attachments bone-source smoke test.
]]--

local attachments = require 'attachments'
local source
local target
local relation
local started
local checked = false

local function fail(message)
    error('attachments bone smoke failed: ' .. message)
end

local function closeEnough(a, b)
    return math.abs(a - b) < 0.01
end

function onInitScene()
    started = mbm.getTimeRun()
    mbm.addPath('src/test-lib')
    source = mesh:new('3d')
    target = mesh:new('3d')
    if not source:load('Lorekeeper-walk.msh') or not target:load('Lorekeeper-walk.msh') then
        fail('could not load skeletal fixtures')
    end
    local clip = source:getSkeletalAnimationName(1)
    if not clip or not source:playSkeletalAnimation(clip) then
        fail('could not start skeletal animation')
    end
end

function onLoop()
    if not relation then
        local bone = source:getSkeletalBoneTransform('mixamorig:RightHand', 'world')
        if bone then
            relation = assert(attachments.attach {
                source = source,
                target = target,
                sourceType = 'bone',
                sourceBone = 'mixamorig:RightHand',
                inherit = {position = true, rotation = true, scale = true},
                offset = {
                    position = {x = 0.25, y = 0, z = 0},
                    rotation = {x = 0, y = 0, z = 0},
                    scale = {x = 1, y = 1, z = 1},
                },
            })
            local state = relation:getState()
            if state.sourceType ~= 'bone' or state.sourceBone ~= 'mixamorig:RightHand' then
                fail('bone source state was not preserved')
            end
        end
    elseif not checked then
        local effective = assert(relation:getEffectivePose())
        local position = target:getPos()
        if not closeEnough(position.x, effective.position.x) or
                not closeEnough(position.y, effective.position.y) or
                not closeEnough(position.z, effective.position.z) then
            fail('target did not receive the evaluated bone pose')
        end
        checked = true
        mbm.quit()
    end
    if started and mbm.getTimeRun() - started > 5 then fail('timed out') end
end
