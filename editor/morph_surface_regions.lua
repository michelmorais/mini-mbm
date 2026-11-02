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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Generic surface-region data and polygon selection, independent of facial projects.
]]--

local tSurfacePolygon=require 'morph_surface_polygon'
local M={}

local function finite(value)
    return type(value)=='number' and value==value and value~=math.huge and value~=-math.huge
end

local function fingerprint(region)
    local parts={region.name or '',region.kind or '',region.closed and '1' or '0',
        region.topologySignature or ''}
    local falloff=region.falloff or {}
    parts[#parts+1]=falloff.mode or 'surface'
    parts[#parts+1]=string.format('%.17g',tonumber(falloff.radius) or 2)
    parts[#parts+1]=falloff.curve or 'smooth'
    for _,point in ipairs(region.points or {}) do
        local offset=point.offset or {}
        parts[#parts+1]=string.format('%d:%.17g:%.17g:%.17g',tonumber(point.index) or 0,
            tonumber(offset.x) or 0,tonumber(offset.y) or 0,tonumber(offset.z) or 0)
    end
    return table.concat(parts,'|')
end
M.fingerprint=fingerprint

function M.new(name,kind,points)
    local region={name=type(name)=='string' and name or '',
        kind=kind=='polyline' and 'polyline' or kind=='point' and 'point' or 'polygon',
        closed=kind~='polyline' and kind~='point',topologySignature='',points={},
        derivedVertexWeights={},falloff={mode='surface',radius=2,curve='smooth'},
        geometryFingerprint=''}
    for _,raw in ipairs(type(points)=='table' and points or {}) do
        local index=type(raw)=='table' and tonumber(raw.index) or tonumber(raw)
        if finite(index) and index>=1 and index%1==0 then
            local anchor={index=index}
            local offset=type(raw)=='table' and raw.offset or nil
            if type(offset)=='table' and finite(tonumber(offset.x)) and
                    finite(tonumber(offset.y)) and finite(tonumber(offset.z)) then
                anchor.offset={x=tonumber(offset.x),y=tonumber(offset.y),z=tonumber(offset.z)}
            end
            region.points[#region.points+1]=anchor
        end
    end
    return region
end

function M.normalize(region,vertexCount)
    if type(region)~='table' then return nil,'invalid_region' end
    region.name=type(region.name)=='string' and region.name or ''
    region.kind=region.kind=='polyline' and 'polyline' or
        region.kind=='point' and 'point' or 'polygon'
    region.closed=region.kind=='polygon' and region.closed~=false
    region.topologySignature=type(region.topologySignature)=='string' and
        region.topologySignature or ''
    region.falloff=type(region.falloff)=='table' and region.falloff or {}
    region.falloff.mode='surface'
    region.falloff.radius=math.max(0,tonumber(region.falloff.radius) or 2)
    region.falloff.curve=type(region.falloff.curve)=='string' and region.falloff.curve or 'smooth'
    local points,seen={},{}
    for _,raw in ipairs(type(region.points)=='table' and region.points or {}) do
        local index=type(raw)=='table' and tonumber(raw.index) or tonumber(raw)
        if finite(index) and index>=1 and index%1==0 and
                (not vertexCount or index<=vertexCount) and not seen[index] then
            local anchor={index=index}
            local offset=type(raw)=='table' and raw.offset or nil
            if type(offset)=='table' and finite(tonumber(offset.x)) and
                    finite(tonumber(offset.y)) and finite(tonumber(offset.z)) then
                anchor.offset={x=tonumber(offset.x),y=tonumber(offset.y),z=tonumber(offset.z)}
            end
            points[#points+1]=anchor
            seen[index]=true
        end
    end
    region.points=points
    local weights,seenWeights={},{}
    for _,raw in ipairs(type(region.derivedVertexWeights)=='table' and
            region.derivedVertexWeights or {}) do
        local index=type(raw)=='table' and tonumber(raw.index) or tonumber(raw)
        local weight=type(raw)=='table' and tonumber(raw.weight) or 1
        if finite(index) and index>=1 and index%1==0 and
                (not vertexCount or index<=vertexCount) and finite(weight) and
                weight>0 and weight<=1 and not seenWeights[index] then
            weights[#weights+1]={index=index,weight=weight}
            seenWeights[index]=true
        end
    end
    table.sort(weights,function(a,b) return a.index<b.index end)
    region.derivedVertexWeights=weights
    region.geometryFingerprint=type(region.geometryFingerprint)=='string' and
        region.geometryFingerprint or ''
    return region
end

function M.rebuild(region,topology)
    if not region or not topology or type(topology.vertices)~='table' then
        return false,'missing_topology'
    end
    M.normalize(region,#topology.vertices)
    if region.kind~='polygon' or not region.closed then return false,'polygon_required' end
    local points=tSurfacePolygon.resolvePoints(region,topology)
    if #points<3 then return false,'too_few_points' end
    local weights=tSurfacePolygon.deriveVertexWeights(topology,points,true)
    if #weights==0 then return false,'empty_region' end
    region.derivedVertexWeights=weights
    region.topologySignature=topology.signature or ''
    region.geometryFingerprint=fingerprint(region)
    return true
end

function M.status(region,topology)
    if not region then return 'missing' end
    if not topology then return 'missing_topology' end
    if region.kind~='polygon' or not region.closed or #(region.points or {})<3 then
        return 'invalid_region'
    end
    if #tSurfacePolygon.resolvePoints(region,topology)<#region.points then
        return 'invalid_anchors'
    end
    if region.topologySignature~=topology.signature then return 'topology_mismatch' end
    if region.geometryFingerprint~=fingerprint(region) then return 'stale' end
    if #(region.derivedVertexWeights or {})==0 then return 'empty' end
    return 'ready'
end

function M.copyVertices(region,topology)
    local status=M.status(region,topology)
    if status~='ready' then return nil,status end
    local result={}
    for _,item in ipairs(region.derivedVertexWeights) do
        if not finite(item.index) or item.index%1~=0 or item.index<1 or
                item.index>#topology.vertices or not finite(item.weight) or
                item.weight<=0 or item.weight>1 then return nil,'invalid_weights' end
        result[#result+1]=item.index
    end
    return result
end

return M
