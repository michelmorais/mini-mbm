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

package.path='editor/?.lua;editor/lang/?.lua;'..package.path
tImGui=require 'ImGui'; tLang=require 'language'
local Playback=require 'articulated_mesh_playback'
local data,preview,path,stage,reads= nil,nil,nil,0,0
local entry={}; local played,paused,stopped=0,0,0
local button=tImGui.Button
tImGui.Button=function(label,...)
    local pressed=button(label,...)
    if (stage==1 or stage==4 or stage==5) and label==tLang.L('ase_play') then return true end
    if stage==2 and label==tLang.L('ase_pause') then return true end
    if stage==3 and label==tLang.L('ase_tl_stop') then return true end
    return pressed
end
function onInitScene()
    mbm.addPath('/home/michel/Downloads')
    data=meshDebug:new(); assert(data:load('/home/michel/Downloads/Bocao.msh'))
    for s=1,data:getTotalSubset(1) do data:setTexture(1,s,'/home/michel/Downloads/bocao.png') end
    path=os.tmpname()..'.msh'; assert(data:save(path,false,false))
    preview=mesh:new('3d'); assert(meshDebug:loadMeshPreview(preview,path)); preview.visible=false
    local get=data.getTotalArticulatedAnimations
    data.getTotalArticulatedAnimations=function(...) reads=reads+1; return get(...) end
    local play,pause,stop=preview.playArticulatedAnimation,preview.pauseArticulatedAnimation,preview.disableArticulatedAnimation
    preview.playArticulatedAnimation=function(...) played=played+1; return play(...) end
    preview.pauseArticulatedAnimation=function(...) paused=paused+1; return pause(...) end
    preview.disableArticulatedAnimation=function(...) stopped=stopped+1; return stop(...) end
end
function onLoop()
    stage=stage+1
    if stage==4 then entry.articulatedPlayback.selected=2 end
    local ok,err
    if tImGui.Begin('Playback smoke',false,0) then
        ok,err=pcall(function() Playback.draw(entry,data,preview,stage~=5,function(fn) return fn() end) end)
    end
    tImGui.End(); assert(ok,err)
    if stage==5 then
        assert(played==2 and paused==1 and stopped==6,'playback controls failed')
        assert(reads==1,'clip list scanned every frame')
        assert(data:getTotalArticulatedParts()==12 and #entry.articulatedPlayback.clips==2)
        meshDebug:loadMeshPreview(preview,nil); preview:destroy(); os.remove(path)
        print('MESH DEBUG READ-ONLY PLAYBACK / CLIP CACHE / DISABLED CONTROLS OK'); mbm.quit()
    end
end
