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
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Cached viewport outlines and handles for generic surface regions.
]]--

local tPolygon=require 'morph_surface_polygon'
local tShapes=require 'morph_surface_shapes'
local M={}
local COLORS={{0.1,0.85,1,1},{1,0.52,0.1,1},{0.7,0.45,1,1},
    {0.35,1,0.55,1},{1,0.8,0.15,1}}

local function destroy(object)
    if object then pcall(function() object:destroy() end) end
end

function M.destroy(state)
    for _,entry in pairs(state.regionOverlay or {}) do destroy(entry.line) end
    destroy(state.regionMarkers)
    destroy(state.regionShapeHandles)
    destroy(state.regionShapeCenter)
    state.regionOverlay={}
    state.regionMarkers=nil
    state.regionShapeHandles=nil
    state.regionShapeCenter=nil
end

local function appendSegment(target,a,b)
    target[#target+1]=a.x; target[#target+1]=a.y; target[#target+1]=a.z
    target[#target+1]=b.x; target[#target+1]=b.y; target[#target+1]=b.z
end

local function appendSquare(target,center,right,up,half)
    local corners={}
    for _,signs in ipairs({{-1,-1},{1,-1},{1,1},{-1,1}}) do
        corners[#corners+1]={x=center.x+right.x*half*signs[1]+up.x*half*signs[2],
            y=center.y+right.y*half*signs[1]+up.y*half*signs[2],
            z=center.z+right.z*half*signs[1]+up.z*half*signs[2]}
    end
    for index=1,4 do appendSegment(target,corners[index],corners[index%4+1]) end
end

local function appendCross(target,center,right,up,half)
    appendSegment(target,
        {x=center.x-right.x*half,y=center.y-right.y*half,z=center.z-right.z*half},
        {x=center.x+right.x*half,y=center.y+right.y*half,z=center.z+right.z*half})
    appendSegment(target,
        {x=center.x-up.x*half,y=center.y-up.y*half,z=center.z-up.z*half},
        {x=center.x+up.x*half,y=center.y+up.y*half,z=center.z+up.z*half})
end

local function updateLine(object,segments,color)
    if #segments==0 then
        destroy(object)
        return nil
    end
    if not object then
        object=line:new('3d')
        object.alwaysOnTop=true
    end
    object:set(segments,1)
    object:setColor(table.unpack(color))
    object.visible=true
    return object
end

function M.rebuild(state)
    if not state.topology then M.destroy(state); return end
    state.regionOverlay=state.regionOverlay or {}
    local activeIds={}
    for regionIndex,region in ipairs(state.project.regions or {}) do
        local id=region.id
        activeIds[id]=true
        local entry=state.regionOverlay[id] or {}
        state.regionOverlay[id]=entry
        local points=tPolygon.resolvePoints(region,state.topology)
        local segments={}
        local edgeCount=region.closed and #points or #points-1
        for index=1,edgeCount do
            local nextIndex=index%#points+1
            appendSegment(segments,points[index].position,points[nextIndex].position)
        end
        if #segments>0 then
            if not entry.line then
                entry.line=line:new('3d')
                entry.line.alwaysOnTop=true
            end
            entry.line:set(segments,1)
            entry.line:setColor(table.unpack(COLORS[(regionIndex-1)%#COLORS+1]))
            entry.line.visible=true
        else
            destroy(entry.line)
            entry.line=nil
        end
    end
    for id,entry in pairs(state.regionOverlay) do
        if not activeIds[id] then destroy(entry.line); state.regionOverlay[id]=nil end
    end

    local selected=state.project.regions[state.project.selectedRegion]
    if not selected then
        destroy(state.regionMarkers); state.regionMarkers=nil
        destroy(state.regionShapeHandles); state.regionShapeHandles=nil
        destroy(state.regionShapeCenter); state.regionShapeCenter=nil
        return
    end
    local points=tPolygon.resolvePoints(selected,state.topology)
    local markers={}
    local extent=state.meshBounds and math.sqrt(
        (state.meshBounds.maxX-state.meshBounds.minX)^2+
        (state.meshBounds.maxY-state.meshBounds.minY)^2+
        (state.meshBounds.maxZ-state.meshBounds.minZ)^2) or 1
    local half=math.max(extent*0.006,0.001)
    local right,up=tShapes.cameraBasis(state.camera)
    if tShapes.isParametric(selected) then
        local shapeHandles,shapeCenter={},{}
        for _,handle in ipairs(tShapes.handles(selected,state.topology,state.camera)) do
            if handle.kind=='center' then
                appendCross(shapeCenter,handle.position,right,up,half*1.45)
            else
                appendSquare(shapeHandles,handle.position,right,up,half*1.2)
            end
        end
        destroy(state.regionMarkers); state.regionMarkers=nil
        state.regionShapeHandles=updateLine(state.regionShapeHandles,shapeHandles,{1,1,1,1})
        state.regionShapeCenter=updateLine(state.regionShapeCenter,shapeCenter,{0.25,1,0.35,1})
        return
    end
    destroy(state.regionShapeHandles); state.regionShapeHandles=nil
    destroy(state.regionShapeCenter); state.regionShapeCenter=nil
    for index,point in ipairs(points) do
        local center=point.position
        appendSquare(markers,center,right,up,half)
        if index==state.selectedPoint then
            appendCross(markers,center,right,up,half*1.5)
        end
    end
    state.regionMarkers=updateLine(state.regionMarkers,markers,{0.15,0.9,1,1})
end

return M
