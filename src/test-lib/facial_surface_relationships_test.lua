--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Regression checks for explicit FACS controller relationships.
]]--

package.path='editor/?.lua;'..package.path
local links=require 'facial_surface_relationships'
local gizmo=require 'facial_surface_point_gizmo'
local surfaceEditor=require 'facial_surface_control_editor'
local projectModule=require 'facial_rig_project'

local topology={signature='fixture',vertices={
    {x=0,y=0,z=0},{x=1,y=0,z=0},{x=1,y=1,z=0},
    {x=-1,y=0,z=0},{x=0,y=0.2,z=0},{x=0.1,y=0.2,z=0},
    {x=-0.1,y=0.2,z=0},{x=-1,y=-1,z=0},{x=1,y=-1,z=0},
    {x=1,y=1,z=0},{x=-1,y=1,z=0}}}
local function control(id,groupId,kind,role,indices)
    local points={}
    for _,index in ipairs(indices) do points[#points+1]={index=index} end
    return {id=id,name=id,groupId=groupId,kind=kind,role=role,points=points,
        relationships={},enabled=true}
end
local controller=control('brow_left_controller','brow_left','point','controller',{1})
local area=control('brow_left_area','brow_left','polygon','area',{2,3})
local line=control('brow_left_action_line','brow_left','polyline','action_line',{5,6})
local other=control('brow_right_area','brow_right','polygon','area',{4})
local rightController=control('brow_right_controller','brow_right','point','controller',{4})
local nearby=control('cheek_left_action_line','cheek_left','polyline','action_line',{6,7})
controller.relationships={{controlId=area.id,weight=0.5},{controlId=line.id,weight=0}}
local project=projectModule.new()
project.surfaceControls={controller,area,line,other,rightController,nearby}
project.facialMasks={
    {id='brow_left',templateId='brow_left',controlIds={controller.id,area.id,line.id}},
    {id='brow_right',templateId='brow_right',controlIds={rightController.id,other.id}},
    {id='cheek_left',templateId='cheek_left',controlIds={nearby.id}}
}

local entries=links.dragEntries(controller,1,links.allEntries(project))
assert(#entries==2 and entries[1].control==controller and entries[2].control==area)
local candidates=links.candidates(controller,links.allEntries(project),topology,0.25)
assert(#candidates==1 and candidates[1].controlId==nearby.id)
assert(#controller.relationships==2)
local enclosure=control('forehead_area','forehead','polygon','area',{8,9,10,11})
enclosure.closed=true
assert(#links.candidates(controller,{{control=enclosure}},topology,0.1)==1)
local extraController=control('forehead_controller','forehead','point','controller',{5})
assert(#links.candidates(controller,{{control=extraController}},topology,0.5)==0)
assert(#links.candidates(controller,{{control=extraController}},topology,0.5,true)==1)
gizmo.translate(gizmo.snapshot(entries,topology),topology,{x=2,y=0,z=0})
assert(math.abs(gizmo.position(controller,1,topology).x-2)<1e-6)
assert(math.abs(gizmo.position(area,1,topology).x-2)<1e-6)
assert(math.abs(gizmo.position(line,1,topology).x-0)<1e-6)

controller.points[1].offset=nil
area.points[1].offset=nil
local state={topology=topology,undoStack={}}
local step,committed=0,0
local api={
    surfaceHit=function() return {point={x=0,y=0,z=0}} end,
    controlEntries=function() return links.allEntries(project) end,
    allEntries=function() return links.allEntries(project) end,
    radius=function() return 0.05 end,
    pickRay=function() return {x=0,y=0,z=1},{x=0,y=0,z=-1} end,
    nearestVertex=function()
        step=step+1
        return {point={x=step==1 and 0 or 2,y=0,z=0}}
    end,
    pushUndo=function() state.undoStack[#state.undoStack+1]=true end,
    refresh=function() end,
    commit=function() committed=committed+1; return true end
}
assert(surfaceEditor.begin(state,0,0,api))
assert(surfaceEditor.update(state,0,0,api))
assert(surfaceEditor.finish(state,api))
assert(math.abs(gizmo.position(controller,1,topology).x-2)<1e-6)
assert(math.abs(gizmo.position(area,1,topology).x-2)<1e-6)
assert(math.abs(gizmo.position(line,1,topology).x-0)<1e-6)
assert(committed==2)

assert(links.accept(controller,candidates))
assert(not links.accept(controller,candidates) and #controller.relationships==3)
assert(controller.relationships[1].weight==0.5)
assert(#rightController.relationships==0) -- independent side editing
assert(links.mirror(project,project.facialMasks[1]))
assert(rightController.relationships[1].controlId==other.id and
    rightController.relationships[1].weight==0.5)
assert(#rightController.relationships==1) -- no counterpart for cheek_left

local normalized=assert(projectModule.normalize(project))
assert(normalized.surfaceControls[1].relationships[1].weight==0.5)
assert(normalized.surfaceControls[5].relationships[1].controlId==other.id)
print('facial surface relationships: ok')
