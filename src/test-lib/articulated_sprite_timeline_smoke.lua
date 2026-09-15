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

package.path='editor/?.lua;'..package.path
local api={}
assert(loadfile('editor/sprite_maker_articulated.lua'))(api)
local init,loop=onInitScene,onLoop
local Model=require 'articulated_sprite_model'
local Timeline=require 'articulated_sprite_timeline'
local IO=require 'articulated_sprite_io'
local stage,path,batch,preview=0
local warmup=0
local finished
local target,inside
local begin=tImGui.Begin
tImGui.Begin=function(label,...)
    if api.state.titles and label==api.state.titles.timeline then
        local w,h=mbm.getRealSizeScreen()
        tImGui.SetNextWindowPos({x=220,y=h*0.45})
        tImGui.SetNextWindowSize({x=w-230,y=h*0.5})
    end
    return begin(label,...)
end
local invisible,hover,mouse,clicked,down,released,endChild=tImGui.InvisibleButton,tImGui.IsItemHovered,tImGui.GetMousePos,
    tImGui.IsMouseClicked,tImGui.IsMouseDown,tImGui.IsMouseReleased,tImGui.EndChild
tImGui.InvisibleButton=function(label,...)
    local result=invisible(label,...)
    if label=='##ase_timeline' then
        inside=true
        local p,q=tImGui.GetItemRectMin(),tImGui.GetItemRectMax()
        local x0=p.x+math.min(170,(q.x-p.x)*0.35)
        local s=api.state.timeline
        local t=stage==4 and 0.4 or stage==5 and 0.4 or 0.2
        target={x=x0+(t-s.viewStart)/(s.viewEnd-s.viewStart)*(q.x-6-x0),
            y=p.y+24+((stage==7 and 2 or 1)-0.5)*24}
    end
    return result
end
tImGui.EndChild=function(...) inside=false; return endChild(...) end
tImGui.IsItemHovered=function(...) if inside then return stage>=3 and stage<=8 end; return hover(...) end
tImGui.GetMousePos=function(...) if inside and target then return target end; return mouse(...) end
tImGui.IsMouseClicked=function(k,...) if inside then return k==0 and (stage==3 or stage==6 or stage==7) end; return clicked(k,...) end
tImGui.IsMouseDown=function(k,...) if inside then return k==0 and (stage==3 or stage==4 or stage==6 or stage==7) end; return down(k,...) end
tImGui.IsMouseReleased=function(k,...) if inside then return k==0 and (stage==5 or stage==8) end; return released(k,...) end
local function near(a,b) assert(math.abs(a-b)<0.0001,tostring(a)..' ~= '..tostring(b)) end
local function clip() return api.state.project.clips[1] end
function onInitScene()
    init()
    path=os.tmpname()..'.png'
    local pixels={}; for i=1,32*32*4 do pixels[i]=255 end
    mbm.createTexture(pixels,32,32,4,'timeline_fixture',path)
    api.addImage(path); api.generate(false); api.generate(false)
    local E=api.state
    E.project.frames[1].parts[1].name='Hammer'; E.project.frames[1].parts[2].name='Spin'
    E.project.clips={{name='Hit',duration=2,speed=1,priority=0,loop=false,tracks={}}}
    for id=1,2 do Model.key(E.project,1,id,0.2,{angle=id*20}); Model.key(E.project,1,id,1.5,{sx=id}) end
    E.project.options.showSource=false; E.mode='animate'; E.selected=1; E.time=0
    E.history=Model.history(E.project); api.syncPose(); api.rebuild()
end
function onLoop(delta)
    if warmup<5 then warmup=warmup+1; loop(delta); return end
    stage=stage+1
    local E=api.state
    E.control=stage==7
    loop(delta)
    if stage==1 then batch=E.timeline.batch; preview=E.preview
    elseif stage==2 then assert(batch==E.timeline.batch and preview==E.preview,'idle rebuilt geometry or sprite')
    elseif stage==3 then assert(E.timeline.count==1 and E.selected==1); near(E.time,0.2)
    elseif stage==4 then near(clip().tracks[1].keys[1].time,0.2); assert(E.timeline.drag.moved)
    elseif stage==5 then near(clip().tracks[1].keys[1].time,0.4); near(E.time,0.4)
        E.project=Model.undo(E.history); E.dirty=true
    elseif stage==6 then near(clip().tracks[1].keys[1].time,0.2); assert(E.timeline.count==1)
    elseif stage==7 then assert(E.timeline.count==2 and E.selected==2,'Ctrl click failed')
    elseif stage==8 then
        assert(api.timelineCommand('copy')); Timeline.seek(E,0.8); assert(api.timelineCommand('paste'))
        for _,track in ipairs(clip().tracks) do assert(#track.keys==3); near(track.keys[2].time,0.8) end
        local file=os.tmpname()..'.spt'
        assert(IO.export(E.project,file))
        local imported=IO.import(file)
        assert(#imported.clips[1].tracks[1].keys==3 and #imported.clips[1].tracks[2].keys==3)
        os.remove(file)
        api.selectTimelineKey(clip().tracks[1].keys[2],false)
        api.selectTimelineKey(clip().tracks[2].keys[2],true)
        assert(api.timelineCommand('delete'))
        for _,track in ipairs(clip().tracks) do assert(#track.keys==2) end
        Timeline.seek(E,0.5); E.timeline.gap=0.3; assert(api.timelineCommand('insert')); near(clip().duration,2.3)
        E.timeline.removal=0.3; assert(api.timelineCommand('remove')); near(clip().duration,2)
        for _,track in ipairs(clip().tracks) do near(track.keys[2].time,1.5) end
    elseif stage==9 then api.rebuild(); batch=E.timeline.batch; preview=E.preview
    elseif stage==10 then assert(batch==E.timeline.batch and preview==E.preview,'idle after edits rebuilt geometry')
        os.remove(path)
        print('TIMELINE MARKER / GROUP DRAG / UNDO / CTRL SELECT / CLIPBOARD / SPT / DELETE / TIME / IDLE OK')
        finished=mbm.getTimeRun()
    elseif finished and mbm.getTimeRun()-finished>2 then mbm.quit() end
end
