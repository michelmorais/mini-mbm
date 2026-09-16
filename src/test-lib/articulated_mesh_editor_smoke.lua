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
assert(loadfile('editor/mesh_maker_articulated.lua'))(api)
local init,loop=onInitScene,onLoop
local Model=require 'articulated_sprite_model'
local Pose=require 'articulated_mesh_pose'
local IO=require 'articulated_mesh_io'
local source='/home/michel/Downloads/Bocao.msh'
local output,start,ready,frames,previousPreview,previousBuilds
local builds=0; local build=IO.build
IO.build=function(...) builds=builds+1; return build(...) end
local function near(a,b) assert(math.abs(a-b)<0.001,tostring(a)..' ~= '..tostring(b)) end
local function geometry(d)
    local frames={}
    for f=1,d:getTotalFrame() do frames[f]={}; for s=1,d:getTotalSubset(f) do
        frames[f][s]={vertices=d:getVertex(f,s,1,d:getTotalVertex(f,s)),indices=d:getIndex(f,s)}
    end end
    return Model.encode(frames)
end
function onInitScene()
    local ok,err=pcall(function()
        init(); assert(api.load(source))
        local E=api.state; assert(#E.project.frames==1 and #E.project.frames[1].parts==12 and #E.project.clips==2)
        assert(E.preview.loadEditorPreview==nil and E.preview.loadMeshPreview==nil)
        local original=geometry(E.data)
        local bare=IO.build(E.base,E.project)
        for i=bare:getTotalArticulatedAnimations(),1,-1 do bare:removeArticulatedAnimation(i) end
        bare:removeArticulatedParts(); bare:initializeArticulatedParts()
        local initialized=IO.read(bare)
        assert(#initialized.frames[1].parts==12 and #initialized.clips==0,'initialize subsets')
        IO.apply(bare,initialized)
        assert(geometry(bare)==original,'initialization changed geometry')
        local p=E.project.frames[1].parts[5]; E.selected=p.id; E.time=0.4; Pose.sync(E)
        E.pose.euler={25,40,720}; E.pose.q=Pose.quaternion(E.pose.euler)
        api.record(); assert(api.rebuild())
        local c=E.project.clips[1]; local selected
        for _,t in ipairs(c.tracks) do if t.part==p.id then for _,k in ipairs(t.keys) do if math.abs(k.time-0.4)<0.001 then selected=k end end end end
        assert(selected); api.selectTimelineKey(selected,false); assert(api.timelineCommand('copy'))
        E.time=1.4; Pose.sync(E); assert(api.timelineCommand('paste')); api.history(false); api.history(true)
        local beforeEmptyRedo=Model.encode(E.project); api.history(true)
        assert(Model.encode(E.project)==beforeEmptyRedo,'empty redo changed project')
        assert(api.action(function() Model.reparent(E.project,1,4,3) end))
        output=os.tmpname()..'.msh'; assert(api.save(output))
        local d=meshDebug:new(); assert(d:load(output)); assert(geometry(d)==original,'geometry changed')
        local loaded=IO.read(d); assert(loaded.frames[1].parts[4].parent==3)
        local found=0
        for _,t in ipairs(loaded.clips[1].tracks) do if t.part==p.id then for _,k in ipairs(t.keys) do
            if math.abs(k.time-0.4)<0.001 or math.abs(k.time-1.4)<0.001 then near(k.euler[1],25); near(k.euler[2],40); near(k.euler[3],720); found=found+1 end
        end end end
        assert(found==2,'3D Euler keys or pasted keys lost')
        api.selectClip(2); E.selected=3; Pose.sync(E); api.rebuild()
        frames=0; ready=true; start=mbm.getTimeRun()
        print('ARTICULATED MESH LOAD / XYZ / EULER / HIERARCHY / CLIPBOARD / UNDO / SAVE / GEOMETRY OK')
    end)
    if not ok then print('ARTICULATED MESH INIT FAILED: '..tostring(err)); mbm.quit() end
end
function onLoop(delta)
    if not ready then return end
    loop(delta); frames=frames+1; local E=api.state
    if frames==5 then previousPreview=E.preview; previousBuilds=builds end
    if frames==10 then assert(E.preview==previousPreview and builds==previousBuilds,'idle rebuild'); start=mbm.getTimeRun(); print('ARTICULATED MESH VIEW READY') end
    if frames>=10 and mbm.getTimeRun()-start>6 then
        os.remove(output)
        print('ARTICULATED MESH UI / IDLE OK'); mbm.quit()
    end
end
