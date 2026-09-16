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
local clicked, selection, selectedMode, selectedWeight
local noop = function() end
tLang = {L = function(key) return key end}
tImGui = {
    TextDisabled=noop, TextWrapped=noop, BeginDisabled=noop, EndDisabled=noop,
    PushItemWidth=noop, PopItemWidth=noop, SameLine=noop, EndCombo=noop,
    BeginCombo=function() return true end,
    Selectable=function(label) return selection and label:find('artPlay' .. selection, 1, true) ~= nil end,
    Combo=function(label, current)
        local value = selection
        if label=='mesh_debug_skeletal_mode' then value=selectedMode end
        return value ~= nil, value or current
    end,
    SliderFloat=function(_, current) return selectedWeight ~= nil, selectedWeight or current end,
    Button=function(label) return clicked == label end,
}
local function call(fn) return true, fn() end
local data = {
    getTotalArticulatedAnimations=function() return 2 end,
    getArticulatedAnimation=function(_, i) return 'clip'..i, 1, 1, 0 end,
}
for _, kind in ipairs({'skeletal'}) do
    local module = require(kind .. '_mesh_playback')
    local active, paused, plays, reads = nil, false, 0, 0
    local baseTime, layer, layerPaused = 0, nil, false
    local preview = {
        getSkeletalAnimationTime=function() if active then return baseTime end end,
        getSkeletalAnimationAbsoluteLayerWeight=function() return layer and layer.weight end,
        playSkeletalAnimationAbsoluteLayer=function(_, name, weight) layer={name=name, weight=weight, mode='absolute'}; return true end,
        playSkeletalAnimationAdditiveLayer=function(_, name, weight) layer={name=name, weight=weight, mode='additive'}; return true end,
        pauseSkeletalAnimationLayer=function() layerPaused=true; return true end,
        resumeSkeletalAnimationLayer=function() layerPaused=false; return true end,
        stopSkeletalAnimationAbsoluteLayer=function() layer=nil; return true end,
        setSkeletalAnimationAbsoluteLayerWeight=function(_, weight) layer.weight=weight; return true end,
        getTotalSkeletalAnimations=function() reads=reads+1; return 2 end,
        getSkeletalAnimationName=function(_, i) return 'clip'..i end,
        playSkeletalAnimation=function(_, name) active=name; paused=false; plays=plays+1; return true end,
        pauseSkeletalAnimation=function() paused=true; return true end,
        stopSkeletalAnimation=function() active=nil; return true end,
        disableArticulatedAnimation=function(_, name) if active==name then active=nil end; return true end,
    }
    preview.playArticulatedAnimation = preview.playSkeletalAnimation
    preview.pauseArticulatedAnimation = function(_, name) assert(name==active); paused=true; return true end
    local entry = {}
    local function draw(button, selected, ready)
        clicked, selection = button, selected
        if kind=='articulated' then module.draw(entry,data,preview,ready~=false,call)
        else module.draw(entry,preview,ready~=false,call) end
    end
    selectedMode=2
    draw('ase_play')
    assert(not active and not layer, 'layer started without a base')
    selectedMode=1
    draw('ase_play')
    selectedMode=nil
    assert(active=='clip1' and plays==1)
    draw(nil,2)
    assert(active=='clip1' and plays==1, 'selection interrupted playback')
    draw('ase_pause')
    assert(paused, 'pause must target the playing clip, not the selection')
    draw('ase_play')
    assert(active=='clip2' and not paused and plays==2)
    active=nil -- Simulate natural completion; selection must not restart it.
    draw(nil,1)
    assert(active==nil and plays==2)
    draw('ase_play',nil,false)
    assert(plays==2, 'disabled preview started playback')
    draw('ase_play')
    draw('ase_tl_stop')
    assert(active==nil)
    for _=1,100 do draw() end
    if kind=='skeletal' then assert(reads==1, 'idle UI rescanned clips') end
    draw('ase_play',1)
    baseTime=0.75
    selectedMode=2
    draw('ase_play',2)
    assert(active=='clip1' and baseTime==0.75 and layer.name=='clip2' and layer.mode=='absolute')
    selectedMode=3
    draw(nil,1)
    assert(layer.mode=='absolute', 'mode selection changed active layer')
    draw('ase_play')
    assert(active=='clip1' and baseTime==0.75 and layer.mode=='additive')
    selectedWeight=0.25
    draw()
    assert(layer.weight==0.25 and baseTime==0.75)
    selectedWeight=nil
    paused=false
    draw('ase_pause')
    assert(layerPaused and not paused)
    draw('swl_resume')
    assert(not layerPaused)
    draw('ase_tl_stop')
    assert(not layer and active=='clip1' and baseTime==0.75)
    draw('ase_play',nil,false)
    assert(not layer)
    selectedMode=nil

end
do
    local module = require 'articulated_mesh_playback'
    local active, entry = {}, {}
    local preview = {
        playArticulatedAnimation=function(_, name) active[name]={time=0}; return true end,
        pauseArticulatedAnimation=function(_, name) if active[name] then active[name].paused=true end; return true end,
        disableArticulatedAnimation=function(_, name) active[name]=nil; return true end,
    }
    local function draw(button, selected, ready)
        clicked, selection = button, selected
        module.draw(entry,data,preview,ready~=false,call)
    end
    draw('ase_play',1)
    active.clip1.time=0.5
    draw(nil,2)
    draw('ase_play')
    assert(active.clip1.time==0.5 and active.clip2.time==0, 'play interrupted previous clip')
    draw('ase_pause')
    assert(active.clip2.paused and not active.clip1.paused)
    draw('ase_tl_stop')
    assert(active.clip1 and not active.clip2, 'stop affected other clips')
    active.clip1=nil -- Completed clips must not restart on selection.
    draw(nil,1)
    assert(not active.clip1)
    draw('ase_play',nil,false)
    assert(not active.clip1)
end
print('MESH DEBUG PLAYBACK TEST OK')
