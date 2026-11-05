--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
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
|------------------------------------------------------------------------------------------------------------------------|

   Interaction helpers for surface polygon editing.
]]--

local tPolygon=require 'facial_rig_polygon'
local M={}

function M.ensure(mask)
    if not mask then return nil end
    if type(mask.polygon)~='table' then
        mask.polygon={points={},closed=false,topologySignature='',derivedVertexWeights={}}
    end
    if type(mask.polygon.points)~='table' then mask.polygon.points={} end
    if type(mask.polygon.derivedVertexWeights)~='table' then mask.polygon.derivedVertexWeights={} end
    return mask.polygon
end

local function radius(state)
    local bounds=state.meshBounds
    local extent=bounds and math.sqrt((bounds.maxX-bounds.minX)^2+(bounds.maxY-bounds.minY)^2+
        (bounds.maxZ-bounds.minZ)^2) or 1
    return math.max(extent*0.055,0.002)
end

local function hasVertex(polygon,index,except)
    for position,point in ipairs(polygon.points or {}) do
        local pointIndex=type(point)=='table' and tonumber(point.index) or tonumber(point)
        if position~=except and pointIndex==index then return true end
    end
    return false
end

function M.begin(state,x,y,api)
    local mask=state.project.masks and state.project.masks[state.selectedMask]
    local polygon=mask and mask.representation=='polygon' and M.ensure(mask)
    if not polygon or not state.topology then return false end
    local hit=api.surfaceHit(x,y)
    if not hit then return false end
    local points=tPolygon.resolvePoints(polygon,state.topology)
    local pointIndex=tPolygon.nearestPoint(points,hit.point,radius(state))
    if pointIndex then
        state.maskPolygonSelectedPoint=pointIndex
        api.pushUndo()
        state.maskPolygonDrag={pointIndex=pointIndex,changed=false}
        return true
    end
    local origin,direction=api.pickRay(x,y)
    local vertexHit=origin and api.nearestVertex(state.topology,origin,direction)
    if not vertexHit or hasVertex(polygon,vertexHit.index) then return true end
    local edgeIndex=tPolygon.nearestEdge(points,hit.point,polygon.closed,radius(state))
    if polygon.closed and not edgeIndex then return true end
    api.pushUndo()
    local newPoint={index=vertexHit.index}
    if polygon.closed then
        table.insert(polygon.points,edgeIndex+1,newPoint)
        state.maskPolygonSelectedPoint=edgeIndex+1
    else
        polygon.points[#polygon.points+1]=newPoint
        state.maskPolygonSelectedPoint=#polygon.points
    end
    api.commit(mask)
    api.refresh()
    return true
end

function M.update(state,x,y,api)
    local drag=state.maskPolygonDrag
    if not drag then return false end
    local mask=state.project.masks and state.project.masks[state.selectedMask]
    local polygon=mask and mask.representation=='polygon' and M.ensure(mask)
    if not polygon then return false end
    local origin,direction=api.pickRay(x,y)
    local hit=origin and api.nearestVertex(state.topology,origin,direction)
    if not hit or hasVertex(polygon,hit.index,drag.pointIndex) then return false end
    local point=polygon.points[drag.pointIndex]
    if not point or tonumber(point.index)==hit.index then return false end
    point.index=hit.index
    drag.changed=true
    state.maskPolygonSelectedPoint=drag.pointIndex
    api.refresh()
    return true
end

function M.finish(state,api)
    local drag=state.maskPolygonDrag
    state.maskPolygonDrag=nil
    if not drag then return false end
    if not drag.changed then
        if #state.undoStack>0 then table.remove(state.undoStack) end
        return false
    end
    local mask=state.project.masks and state.project.masks[state.selectedMask]
    return api.commit(mask)
end

return M
