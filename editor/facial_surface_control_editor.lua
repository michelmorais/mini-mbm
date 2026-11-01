--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit     |
| persons to whom the Software is furnished to do so, subject to the following conditions:                              |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Surface-control interaction helpers for the Facial Rig Editor.
]]--

local tSurface=require 'facial_rig_polygon'
local M={}

function M.ensure(control)
    if not control then return nil end
    control.kind=control.kind=='polyline' and 'polyline' or control.kind=='point' and 'point' or 'polygon'
    control.closed=control.kind=='polygon' and control.closed~=false or false
    if type(control.points)~='table' then control.points={} end
    if type(control.relationships)~='table' then control.relationships={} end
    if control.kind~='point' then control.relationships={} end
    return control
end

local function nearestCandidate(entries,topology,point,maxDistance)
    local best=nil
    for _,entry in ipairs(entries or {}) do
        local control=M.ensure(entry.control)
        local points=tSurface.resolvePoints(control,topology)
        local pointIndex,distance=tSurface.nearestPoint(points,point,maxDistance)
        local priority=control.role=='controller' and 0 or 1
        if pointIndex and (not best or distance<best.distance or
                math.abs(distance-best.distance)<=1e-6 and priority<best.priority) then
            best={entry=entry,pointIndex=pointIndex,distance=distance,priority=priority}
        end
    end
    return best
end

local function nearestVertexToPosition(topology,target,used,sourceIndex)
    local bestIndex,bestDistance=nil,math.huge
    local sourceRef=topology.refs and topology.refs[sourceIndex]
    local sourceSubset=sourceRef and sourceRef.subset
    for index,vertex in ipairs(topology.vertices or {}) do
        local ref=topology.refs and topology.refs[index]
        if not used[index] and (not sourceSubset or ref and ref.subset==sourceSubset) then
            local dx,dy,dz=vertex.x-target.x,vertex.y-target.y,vertex.z-target.z
            local distance=dx*dx+dy*dy+dz*dz
            if distance<bestDistance then bestIndex,bestDistance=index,distance end
        end
    end
    return bestIndex
end

local function copyPosition(point)
    return {x=point.x,y=point.y,z=point.z}
end

local function snapshotEntries(entries,topology)
    local result={}
    for _,entry in ipairs(entries or {}) do
        local control=M.ensure(entry.control)
        local points={}
        for index,point in ipairs(control.points or {}) do
            local vertexIndex=type(point)=='table' and tonumber(point.index) or tonumber(point)
            local position=vertexIndex and topology.vertices[vertexIndex]
            if position then points[#points+1]={pointIndex=index,index=vertexIndex,position=copyPosition(position)} end
        end
        result[#result+1]={control=control,index=entry.index,points=points}
    end
    return result
end

function M.begin(state,x,y,api)
    if not state.topology then return false end
    local hit=api.surfaceHit(x,y)
    if not hit then return false end
    local entries=api.controlEntries and api.controlEntries(state) or {}
    local candidate=nearestCandidate(entries,state.topology,hit.point,api.radius(state))
    if not candidate then return false end
    local control=candidate.entry.control
    state.selectedSurfaceControl=candidate.entry.index
    state.selectedSurfaceControlPoint=candidate.pointIndex
    api.pushUndo()
    if control.role=='controller' then
        state.surfaceControlDrag={mode='group',controller=control,controllerPoint=candidate.pointIndex,
            baseControllerIndex=control.points[candidate.pointIndex].index,
            baseControllerPosition=copyPosition(state.topology.vertices[control.points[candidate.pointIndex].index]),
            members=snapshotEntries(entries,state.topology),changed=false}
    else
        state.surfaceControlDrag={mode='point',control=control,pointIndex=candidate.pointIndex,changed=false}
    end
    return true
end

function M.update(state,x,y,api)
    local drag=state.surfaceControlDrag
    if not drag then return false end
    local origin,direction=api.pickRay(x,y)
    local hit=origin and api.nearestVertex(state.topology,origin,direction)
    if not hit then return false end
    if drag.mode=='group' then
        local delta={x=hit.point.x-drag.baseControllerPosition.x,
            y=hit.point.y-drag.baseControllerPosition.y,z=hit.point.z-drag.baseControllerPosition.z}
        for _,member in ipairs(drag.members or {}) do
            local used={}
            for _,basePoint in ipairs(member.points) do
                local target={x=basePoint.position.x+delta.x,y=basePoint.position.y+delta.y,
                    z=basePoint.position.z+delta.z}
                local index=nearestVertexToPosition(state.topology,target,used,basePoint.index)
                if index then
                    used[index]=true
                    member.control.points[basePoint.pointIndex].index=index
                    member.control.points[basePoint.pointIndex].offset=nil
                end
            end
        end
    else
        local control=drag.control
        local point=control.points[drag.pointIndex]
        if not point or tonumber(point.index)==hit.index and not point.offset then return false end
        point.index=hit.index
        point.offset=nil
    end
    drag.changed=true
    state.selectedSurfaceControlPoint=drag.controllerPoint or drag.pointIndex
    api.refresh()
    return true
end

function M.finish(state,api)
    local drag=state.surfaceControlDrag
    state.surfaceControlDrag=nil
    if not drag then return false end
    if not drag.changed then
        if #state.undoStack>0 then table.remove(state.undoStack) end
        return false
    end
    if drag.mode=='group' then
        for _,member in ipairs(drag.members or {}) do api.commit(member.control) end
        return true
    end
    return api.commit(drag.control)
end

return M
