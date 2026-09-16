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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

]]--

package.path = 'editor/?.lua;' .. package.path
tImGui = require 'ImGui'
tLang = require 'lang.language'
local playback = require 'skeletal_mesh_playback'
local preview, stage, started = nil, 0, nil
local entry = {}
local click
local realButton = tImGui.Button
tImGui.Button = function(label, ...)
    local pressed = realButton(label, ...)
    return label == click or pressed
end
local function call(fn)
    local ok, value = pcall(fn)
    assert(ok, value)
    return ok, value
end
function onInitScene()
    mbm.addPath('src/test-lib')
    preview = mesh:new('3d')
    assert(preview:load('Lorekeeper-walk.msh'))
    assert(preview:getTotalSkeletalAnimations() > 0)
    started = mbm.getTimeRun()
end
function onLoop()
    stage = stage + 1
    click = nil
    local state = entry.skeletalPlayback
    if stage == 1 then click = tLang.L('ase_play') end
    if stage == 2 then
        assert(preview:seekSkeletalAnimation(0.1))
        assert(preview:setSkeletalAnimationPlaybackSpeed(0))
        state.mode = 2
        state.selected = math.min(2, #state.clips)
        click = tLang.L('ase_play')
    end
    if stage == 3 then
        assert(preview:getSkeletalAnimationAbsoluteLayerWeight() == 0.5)
        assert(math.abs(preview:getSkeletalAnimationTime() - 0.1) < 0.00001)
        state.mode = 3
        click = tLang.L('ase_play')
    end
    if stage == 4 then
        assert(preview:getSkeletalAnimationAbsoluteLayerTime() == 0)
        assert(math.abs(preview:getSkeletalAnimationTime() - 0.1) < 0.00001)
        click = tLang.L('ase_pause')
    end
    if stage == 5 then
        assert(preview:isSkeletalAnimationLayerPaused())
        click = tLang.L('swl_resume')
    end
    if stage == 6 then
        assert(not preview:isSkeletalAnimationLayerPaused())
        click = tLang.L('ase_tl_stop')
    end
    if stage == 7 then
        assert(preview:getSkeletalAnimationAbsoluteLayerWeight() == nil)
        assert(math.abs(preview:getSkeletalAnimationTime() - 0.1) < 0.00001)
        state.mode = 1
        click = tLang.L('ase_tl_stop')
    end
    local ok, err = true, nil
    if tImGui.Begin('Mesh Debug skeletal layer smoke', false, 0) then
        ok, err = pcall(function() playback.draw(entry, preview, true, call) end)
    end
    tImGui.End()
    assert(ok, err)
    assert(not entry.skeletalPlayback.failed, 'playback command rejected')
    if stage == 7 then
        assert(preview:getSkeletalAnimationTime() == nil)
        print('SKELETAL PREVIEW ABSOLUTE / ADDITIVE / INDEPENDENT LAYER CONTROLS OK')
        mbm.quit()
    end
    assert(mbm.getTimeRun() - started < 5, 'smoke test deadline exceeded')
end
