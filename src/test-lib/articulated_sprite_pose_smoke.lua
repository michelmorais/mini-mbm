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
local M=require 'articulated_sprite_model'
local IO=require 'articulated_sprite_io'
local stage,rendered,path=0
local export=IO.export
IO.export=function(project,...)
    rendered=M.copy(project)
    return export(project,...)
end
local selectable,drag,button=tImGui.Selectable,tImGui.DragFloat,tImGui.Button
tImGui.Selectable=function(label,...)
    local value=selectable(label,...)
    if ((stage==2 or stage==5) and label=='Spin##part2') or ((stage==4 or stage==9) and label=='Hammer##part1') then return true end
    if stage==9 and label=='0.350##key2' then return true end
    return value
end
tImGui.DragFloat=function(label,...)
    local changed,value=drag(label,...)
    if stage==6 and label==tLang.L('ase_time') then return true,0.5 end
    if stage==10 and label==tLang.L('ase_time') then return true,0.8 end
    if label==tLang.L('ase_key_rotation') then
        if stage==1 then return true,70 end
        if stage==3 then return true,20 end
        if stage==5 then return true,30 end
    end
    return changed,value
end
tImGui.Button=function(label,...)
    local value=button(label,...)
    if stage==4 and label==tLang.L('ase_record') then return true end
    if stage==11 and label==tLang.L('ase_move_key') then return true end
    if stage==12 and label==tLang.L('ase_delete_key') then return true end
    return value
end
local function key(project,id,time)
    for _,t in ipairs(project.clips[1].tracks) do
        if t.part==id then for _,k in ipairs(t.keys) do if math.abs(k.time-time)<0.00001 then return k end end end
    end
end
local function near(a,b) assert(math.abs(a-b)<0.001,tostring(a)..' ~= '..tostring(b)) end
function onInitScene()
    init()
    path=os.tmpname()..'.png'
    local pixels={}; for i=1,32*32*4 do pixels[i]=255 end
    mbm.createTexture(pixels,32,32,4,'pose_fixture',path)
    api.addImage(path); api.generate(false); api.generate(false)
    local E=api.state
    E.project.frames[1].parts[1].name='Hammer'; E.project.frames[1].parts[2].name='Spin'
    E.project.clips={{name='Hit',duration=1,speed=1,priority=0,loop=false,tracks={}}}
    M.key(E.project,1,1,0,{angle=0}); M.key(E.project,1,1,1,{angle=100})
    M.key(E.project,1,2,0,{angle=0})
    E.project.options.showSource=false; E.mode='animate'; E.selected=1; E.time=0.35
    E.history=M.history(E.project); api.syncPose(); api.rebuild()
end
local previousPreview,previousPose
function onLoop(delta)
    stage=stage+1
    local E=api.state
    if stage==5 then E.project.options.autoKey=true end
    loop(delta)
    if stage==1 then
        near(key(rendered,1,0.35).angle,70); assert(not key(E.project,1,0.35))
    elseif stage==2 then
        assert(E.selected==2); near(E.pose.angle,0)
        near(key(rendered,1,0.35).angle,70); assert(not key(rendered,2,0.35))
    elseif stage==3 then
        near(key(rendered,1,0.35).angle,70); near(key(rendered,2,0.35).angle,20)
        assert(not key(E.project,2,0.35))
    elseif stage==4 then
        near(key(E.project,1,0.35).angle,70)
        near(key(rendered,2,0.35).angle,20); assert(not key(E.project,2,0.35))
    elseif stage==5 then
        near(key(E.project,1,0.35).angle,70); near(key(E.project,2,0.35).angle,30)
        assert(not E.transient)
    elseif stage==6 then
        near(E.time,0.5); near(E.pose.angle,30)
        previousPreview,previousPose=E.preview,E.pose
    elseif stage==7 then
        assert(E.preview==previousPreview and E.pose==previousPose,'idle selection did continuous work')
        E.project=M.undo(E.history); E.dirty=true
    elseif stage==8 then
        assert(not key(E.project,2,0.35)); near(E.pose.angle,0)
        near(key(E.project,1,0.35).angle,70)
    elseif stage==9 then
        near(E.keyOrigin,0.35); near(E.pose.angle,70)
    elseif stage==10 then
        near(E.keyOrigin,0.35); near(E.time,0.8)
    elseif stage==11 then
        assert(not key(E.project,1,0.35)); near(key(E.project,1,0.8).angle,70)
    elseif stage==12 then
        assert(not key(E.project,1,0.8) and E.keyOrigin==nil)
    elseif stage==13 then
        os.remove(path)
        print('TIMELINE HAMMER / SPIN / DRAFTS / AUTO KEY / UNDO / MOVE / DELETE / IDLE OK')
        mbm.quit()
    end
end
