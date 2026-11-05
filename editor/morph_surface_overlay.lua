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
local SELECTED_COLOR={0.1,0.85,1,1}
local OTHER_COLOR={0.45,0.5,0.6,1}
local INACTIVE_SELECTED_COLOR={0.28,0.34,0.42,1}
local INACTIVE_OTHER_COLOR={0.22,0.26,0.32,1}
local MIRROR_SOURCE_COLOR={0.1,0.85,1,1}
local MIRROR_PREVIEW_COLOR={1,0.62,0.15,1}

local function destroy(object)
    if object then pcall(function() object:destroy() end) end
end

local function destroyCollection(collection)
    if not collection then return end
    if collection.destroy then
        destroy(collection)
        return
    end
    for _,object in ipairs(collection) do destroy(object) end
end

function M.destroy(state)
    for _,entry in pairs(state.regionOverlay or {}) do destroy(entry.line) end
    destroy(state.regionMirrorPreviewLine)
    state.regionMirrorPreviewLine=nil
    destroyCollection(state.regionMarkers)
    destroyCollection(state.regionShapeHandles)
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
    local left={x=center.x-right.x*half,y=center.y-right.y*half,z=center.z-right.z*half}
    local rightPoint={x=center.x+right.x*half,y=center.y+right.y*half,z=center.z+right.z*half}
    local bottom={x=center.x-up.x*half,y=center.y-up.y*half,z=center.z-up.z*half}
    local top={x=center.x+up.x*half,y=center.y+up.y*half,z=center.z+up.z*half}
    -- LINE_MESH is a line strip. Route through the center so the strip only
    -- draws segments belonging to the cross; two independent segments would
    -- create an unwanted diagonal between their endpoints.
    target[#target+1]=left.x; target[#target+1]=left.y; target[#target+1]=left.z
    target[#target+1]=center.x; target[#target+1]=center.y; target[#target+1]=center.z
    target[#target+1]=rightPoint.x; target[#target+1]=rightPoint.y; target[#target+1]=rightPoint.z
    target[#target+1]=center.x; target[#target+1]=center.y; target[#target+1]=center.z
    target[#target+1]=bottom.x; target[#target+1]=bottom.y; target[#target+1]=bottom.z
    target[#target+1]=top.x; target[#target+1]=top.y; target[#target+1]=top.z
end

local function updateLine(object,segments,color,priority)
    if #segments==0 then
        destroy(object)
        return nil
    end
    if not object then
        object=line:new('3d',0,0,0)
        object:add(segments)
    else
        object:set(segments,1)
    end
    object.alwaysOnTop=true
    object.alwaysRender=true
    object.alwaysOnTopPriority=priority or 1
    object:setColor(table.unpack(color))
    object.visible=true
    return object
end

local function updateLineCollection(collection,paths,color,priority)
    collection=collection or {}
    for index,path in ipairs(paths) do
        collection[index]=updateLine(collection[index],path,color,priority)
    end
    for index=#paths+1,#collection do destroy(collection[index]); collection[index]=nil end
    return collection
end

function M.rebuild(state)
    if not state.topology then M.destroy(state); return end
    if state.worktree==3 or state.worktree==4 then
        for _,entry in pairs(state.regionOverlay or {}) do destroy(entry.line) end
        state.regionOverlay={}
        destroy(state.regionMirrorPreviewLine)
        state.regionMirrorPreviewLine=nil
        destroyCollection(state.regionMarkers); state.regionMarkers=nil
        destroyCollection(state.regionShapeHandles); state.regionShapeHandles=nil
        destroy(state.regionShapeCenter); state.regionShapeCenter=nil
        return
    end
    state.regionOverlay=state.regionOverlay or {}
    local editingShapes=state.worktree==1
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
                entry.line=line:new('3d',0,0,0)
                entry.line:add(segments)
            else
                entry.line:set(segments,1)
            end
            -- Reapply overlay state on every rebuild. A line can survive a
            -- preview/worktree transition and must never fall back behind the mesh.
            entry.line.alwaysOnTop=true
            entry.line.alwaysRender=true
            local color
            local mirrorSource=state.worktree==2 and state.targetMode==5 and
                regionIndex==state.project.selectedRegion
            if editingShapes then
                color=regionIndex==state.project.selectedRegion and SELECTED_COLOR or OTHER_COLOR
            elseif mirrorSource then
                color=MIRROR_SOURCE_COLOR
            else
                color=regionIndex==state.project.selectedRegion and INACTIVE_SELECTED_COLOR or
                    INACTIVE_OTHER_COLOR
            end
            entry.line:setColor(table.unpack(color))
            entry.line.alwaysOnTopPriority=editingShapes and
                (regionIndex==state.project.selectedRegion and 2 or 1) or
                (mirrorSource and 4 or 1)
            entry.line.visible=true
        else
            destroy(entry.line)
            entry.line=nil
        end
    end
    for id,entry in pairs(state.regionOverlay) do
        if not activeIds[id] then destroy(entry.line); state.regionOverlay[id]=nil end
    end

    local mirrorCandidate=state.worktree==2 and state.targetMirrorCandidate or
        state.worktree==1 and state.shapeMode==4 and state.shapeMirrorCandidate
    local mirrorRegion=mirrorCandidate and mirrorCandidate.region
    local mirrorPoints=mirrorRegion and tPolygon.resolvePoints(mirrorRegion,state.topology) or {}
    local mirrorSegments={}
    local mirrorEdgeCount=mirrorRegion and mirrorRegion.closed and #mirrorPoints or #mirrorPoints-1
    for index=1,mirrorEdgeCount do
        local nextIndex=index%#mirrorPoints+1
        appendSegment(mirrorSegments,mirrorPoints[index].position,mirrorPoints[nextIndex].position)
    end
    state.regionMirrorPreviewLine=updateLine(state.regionMirrorPreviewLine,mirrorSegments,
        MIRROR_PREVIEW_COLOR,5)

    local selected=state.project.regions[state.project.selectedRegion]
    if not selected then
        destroyCollection(state.regionMarkers); state.regionMarkers=nil
        destroyCollection(state.regionShapeHandles); state.regionShapeHandles=nil
        destroy(state.regionShapeCenter); state.regionShapeCenter=nil
        return
    end
    local points=tPolygon.resolvePoints(selected,state.topology)
    if not editingShapes then
        destroyCollection(state.regionMarkers); state.regionMarkers=nil
        destroyCollection(state.regionShapeHandles); state.regionShapeHandles=nil
        destroy(state.regionShapeCenter); state.regionShapeCenter=nil
        return
    end
    local extent=state.meshBounds and math.sqrt(
        (state.meshBounds.maxX-state.meshBounds.minX)^2+
        (state.meshBounds.maxY-state.meshBounds.minY)^2+
        (state.meshBounds.maxZ-state.meshBounds.minZ)^2) or 1
    local handleScale=math.max(0.05,math.min(5,tonumber(state.handleScale) or 1))
    local half=math.max(extent*0.006*handleScale,1e-6)
    local right,up=tShapes.cameraBasis(state.camera)
    if tShapes.isParametric(selected) then
        local shapeHandlePaths,shapeCenter={},{ }
        for _,handle in ipairs(tShapes.handles(selected,state.topology,state.camera)) do
            if handle.kind=='center' then
                appendCross(shapeCenter,handle.position,right,up,half*1.45)
            else
                local path={}
                appendSquare(path,handle.position,right,up,half*1.2)
                shapeHandlePaths[#shapeHandlePaths+1]=path
            end
        end
        destroyCollection(state.regionMarkers); state.regionMarkers=nil
        state.regionShapeHandles=updateLineCollection(state.regionShapeHandles,shapeHandlePaths,{1,1,1,1},3)
        state.regionShapeCenter=updateLine(state.regionShapeCenter,shapeCenter,{0.25,1,0.35,1},4)
        return
    end
    destroyCollection(state.regionShapeHandles); state.regionShapeHandles=nil
    destroy(state.regionShapeCenter); state.regionShapeCenter=nil
    local markerPaths={}
    for index,point in ipairs(points) do
        local center=point.position
        local path={}
        appendSquare(path,center,right,up,half)
        markerPaths[#markerPaths+1]=path
        if index==state.selectedPoint then
            local cross={}
            appendCross(cross,center,right,up,half*1.5)
            markerPaths[#markerPaths+1]=cross
        end
    end
    state.regionMarkers=updateLineCollection(state.regionMarkers,markerPaths,SELECTED_COLOR,3)
end

return M
