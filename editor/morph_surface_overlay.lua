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
local M={}
local COLORS={{0.1,0.85,1,1},{1,0.52,0.1,1},{0.7,0.45,1,1},
    {0.35,1,0.55,1},{1,0.8,0.15,1}}

local function destroy(object)
    if object then pcall(function() object:destroy() end) end
end

function M.destroy(state)
    for _,entry in pairs(state.regionOverlay or {}) do destroy(entry.line) end
    destroy(state.regionMarkers)
    state.regionOverlay={}
    state.regionMarkers=nil
end

local function appendSegment(target,a,b)
    target[#target+1]=a.x; target[#target+1]=a.y; target[#target+1]=a.z
    target[#target+1]=b.x; target[#target+1]=b.y; target[#target+1]=b.z
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
        if not entry.line then
            entry.line=line:new('3d')
            entry.line.alwaysOnTop=true
        end
        local points=tPolygon.resolvePoints(region,state.topology)
        local segments={}
        local edgeCount=region.closed and #points or #points-1
        for index=1,edgeCount do
            local nextIndex=index%#points+1
            appendSegment(segments,points[index].position,points[nextIndex].position)
        end
        if #segments>0 then
            entry.line:set(segments,1)
            entry.line:setColor(table.unpack(COLORS[(regionIndex-1)%#COLORS+1]))
            entry.line.visible=true
        else
            entry.line.visible=false
        end
    end
    for id,entry in pairs(state.regionOverlay) do
        if not activeIds[id] then destroy(entry.line); state.regionOverlay[id]=nil end
    end

    local selected=state.project.regions[state.project.selectedRegion]
    if not selected then
        destroy(state.regionMarkers); state.regionMarkers=nil
        return
    end
    local points=tPolygon.resolvePoints(selected,state.topology)
    local markers={}
    local extent=state.meshBounds and math.sqrt(
        (state.meshBounds.maxX-state.meshBounds.minX)^2+
        (state.meshBounds.maxY-state.meshBounds.minY)^2+
        (state.meshBounds.maxZ-state.meshBounds.minZ)^2) or 1
    local half=math.max(extent*0.006,0.001)
    local azimuth=state.camera.azimuth or 0
    local elevation=state.camera.elevation or 0
    local right={x=math.cos(azimuth),y=0,z=-math.sin(azimuth)}
    local up={x=-math.sin(azimuth)*math.sin(elevation),y=math.cos(elevation),
        z=-math.cos(azimuth)*math.sin(elevation)}
    for index,point in ipairs(points) do
        local center=point.position
        local corners={}
        for _,signs in ipairs({{-1,-1},{1,-1},{1,1},{-1,1}}) do
            corners[#corners+1]={x=center.x+right.x*half*signs[1]+up.x*half*signs[2],
                y=center.y+right.y*half*signs[1]+up.y*half*signs[2],
                z=center.z+right.z*half*signs[1]+up.z*half*signs[2]}
        end
        for corner=1,4 do appendSegment(markers,corners[corner],corners[corner%4+1]) end
        if index==state.selectedPoint then
            local crossHalf=half*1.5
            appendSegment(markers,
                {x=center.x-right.x*crossHalf,y=center.y-right.y*crossHalf,z=center.z-right.z*crossHalf},
                {x=center.x+right.x*crossHalf,y=center.y+right.y*crossHalf,z=center.z+right.z*crossHalf})
            appendSegment(markers,
                {x=center.x-up.x*crossHalf,y=center.y-up.y*crossHalf,z=center.z-up.z*crossHalf},
                {x=center.x+up.x*crossHalf,y=center.y+up.y*crossHalf,z=center.z+up.z*crossHalf})
        end
    end
    if #markers==0 then destroy(state.regionMarkers); state.regionMarkers=nil; return end
    if not state.regionMarkers then
        state.regionMarkers=line:new('3d')
        state.regionMarkers.alwaysOnTop=true
    end
    state.regionMarkers:set(markers,1)
    state.regionMarkers:setColor(0.15,0.9,1,1)
    state.regionMarkers.visible=true
end

return M
