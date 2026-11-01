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

   Explicitly derived facial regions. Surface controls remain the authoring source.
]]--

local tPolygon=require 'facial_rig_polygon'
local M={}

local function finite(value)
    return type(value)=='number' and value==value and value~=math.huge and value~=-math.huge
end

local function controlById(project,id)
    for _,control in ipairs(project and project.surfaceControls or {}) do
        if control.id==id then return control end
    end
end

local function areaForGroup(project,group)
    for _,id in ipairs(group and group.controlIds or {}) do
        local control=controlById(project,id)
        if control and control.role=='area' and control.kind=='polygon' and control.closed then
            return control
        end
    end
end

local function fingerprint(control,faceScale)
    if not control then return '' end
    local parts={control.id or '',control.kind or '',control.closed and '1' or '0',
        control.topologySignature or '',string.format('%.17g',control.depthOffset or 0),
        string.format('%.17g',tonumber(faceScale) or 1)}
    for _,point in ipairs(control.points or {}) do
        local offset=type(point)=='table' and point.offset or nil
        parts[#parts+1]=string.format('%d:%.17g:%.17g:%.17g',
            tonumber(type(point)=='table' and point.index or point) or 0,
            offset and offset.x or 0,offset and offset.y or 0,offset and offset.z or 0)
    end
    return table.concat(parts,'|')
end
M.fingerprint=fingerprint

local function onEdge(x,y,a,b)
    local dx,dy=b.x-a.x,b.y-a.y
    local cross=(x-a.x)*dy-(y-a.y)*dx
    if math.abs(cross)>1e-8 then return false end
    local along=(x-a.x)*dx+(y-a.y)*dy
    return along>=-1e-8 and along<=dx*dx+dy*dy+1e-8
end

local function insidePolygon(x,y,polygon)
    local inside=false
    local previous=polygon[#polygon]
    for _,current in ipairs(polygon) do
        if onEdge(x,y,previous,current) then return true end
        if (current.y>y)~=(previous.y>y) and
                x<(previous.x-current.x)*(y-current.y)/(previous.y-current.y)+current.x then
            inside=not inside
        end
        previous=current
    end
    return inside
end

local function validContour(polygon)
    local minX,minY,maxX,maxY=math.huge,math.huge,-math.huge,-math.huge
    local twiceArea=0
    for index,point in ipairs(polygon) do
        local nextPoint=polygon[index%#polygon+1]
        twiceArea=twiceArea+point.x*nextPoint.y-nextPoint.x*point.y
        minX,minY=math.min(minX,point.x),math.min(minY,point.y)
        maxX,maxY=math.max(maxX,point.x),math.max(maxY,point.y)
    end
    return math.abs(twiceArea)>math.max((maxX-minX)*(maxY-minY)*1e-6,1e-16)
end

-- Front-facing XY projection with a local depth slab. The anchor subset keeps
-- hair, eyes, and other separate mesh parts out of the generated selection.
local function deriveWeights(topology,control,faceScale)
    local points=tPolygon.resolvePoints(control,topology)
    if #points<3 then return nil,'too_few_points' end
    local polygon,minZ,maxZ={},math.huge,-math.huge
    for _,point in ipairs(points) do
        polygon[#polygon+1]={x=point.position.x,y=point.position.y}
        -- The mesh anchors identify the skin layer. The artist may lift the
        -- contour in Z for editing without moving the underlying selection.
        local base=topology.vertices[point.index]
        minZ=math.min(minZ,base.z)
        maxZ=math.max(maxZ,base.z)
    end
    if not validContour(polygon) then return nil,'invalid_contour' end
    local firstRef=topology.refs and topology.refs[points[1].index]
    local subset=firstRef and firstRef.subset
    local tolerance=math.max((tonumber(faceScale) or 1)*0.18,(maxZ-minZ)*0.6,1e-5)
    local weights,seen={},{}
    for index,vertex in ipairs(topology.vertices or {}) do
        local ref=topology.refs and topology.refs[index]
        if (not subset or ref and ref.subset==subset) and
                vertex.z>=minZ-tolerance and vertex.z<=maxZ+tolerance and
                insidePolygon(vertex.x,vertex.y,polygon) then
            weights[#weights+1]={index=index,weight=1}
            seen[index]=true
        end
    end
    for _,point in ipairs(points) do
        local ref=topology.refs and topology.refs[point.index]
        if not seen[point.index] and (not subset or ref and ref.subset==subset) then
            weights[#weights+1]={index=point.index,weight=1}
            seen[point.index]=true
        end
    end
    table.sort(weights,function(a,b) return a.index<b.index end)
    if #weights==0 then return nil,'empty' end
    return weights
end

function M.regionForGroup(project,group)
    for _,region in ipairs(project and project.generatedRegions or {}) do
        if region.sourceGroupId==group.id then return region end
    end
end

function M.status(project,topology,region)
    if not region then return 'missing' end
    local control=controlById(project,region.sourceControlId)
    if not control then return 'missing_source' end
    local sourceGroup
    for _,group in ipairs(project and project.facialMasks or {}) do
        if group.id==region.sourceGroupId then sourceGroup=group; break end
    end
    if not sourceGroup or areaForGroup(project,sourceGroup)~=control then return 'stale' end
    if not topology or region.topologySignature~=topology.signature or
            control.topologySignature~='' and control.topologySignature~=topology.signature then
        return 'topology_mismatch'
    end
    if region.sourceFingerprint~=fingerprint(control,
            project.faceOrigin and project.faceOrigin.scale) then return 'stale' end
    if #(region.vertexWeights or {})==0 then return 'empty' end
    return 'ready'
end

function M.copyVertices(project,topology,region)
    local status=M.status(project,topology,region)
    if status~='ready' then return nil,status end
    local vertices={}
    for _,item in ipairs(region.vertexWeights) do
        if not finite(item.index) or item.index%1~=0 or item.index<1 or
                item.index>#(topology.vertices or {}) or not finite(item.weight) or
                item.weight<=0 or item.weight>1 then return nil,'invalid_vertices' end
        vertices[#vertices+1]=item.index
    end
    return vertices
end

function M.generate(project,topology,group)
    if not project or not topology or not group then return nil,'missing_source' end
    local control=areaForGroup(project,group)
    if not control then return nil,'missing_area' end
    if control.topologySignature~='' and control.topologySignature~=topology.signature then
        return nil,'topology_mismatch'
    end
    local weights,errorCode=deriveWeights(topology,control,
        project.faceOrigin and project.faceOrigin.scale)
    if not weights then return nil,errorCode end
    project.generatedRegions=project.generatedRegions or {}
    local region=M.regionForGroup(project,group)
    if not region then
        region={id=(group.id or control.id)..'_region'}
        project.generatedRegions[#project.generatedRegions+1]=region
    end
    region.name=group.name or group.id or control.id
    region.sourceGroupId=group.id
    region.sourceControlId=control.id
    region.topologySignature=topology.signature or ''
    region.sourceFingerprint=fingerprint(control,project.faceOrigin and project.faceOrigin.scale)
    region.vertexWeights=weights
    return region
end

function M.normalize(project)
    local normalized={}
    for _,region in ipairs(type(project.generatedRegions)=='table' and project.generatedRegions or {}) do
        if type(region)=='table' and type(region.sourceControlId)=='string' and
                region.sourceControlId~='' and type(region.sourceGroupId)=='string' then
            region.id=type(region.id)=='string' and region.id~='' and region.id or
                region.sourceGroupId..'_region'
            region.name=type(region.name)=='string' and region.name~='' and region.name or region.id
            region.topologySignature=type(region.topologySignature)=='string' and
                region.topologySignature or ''
            region.sourceFingerprint=type(region.sourceFingerprint)=='string' and
                region.sourceFingerprint or ''
            local weights,seen={},{}
            for _,item in ipairs(type(region.vertexWeights)=='table' and region.vertexWeights or {}) do
                local index=type(item)=='table' and tonumber(item.index) or tonumber(item)
                local weight=type(item)=='table' and tonumber(item.weight) or 1
                if finite(index) and index>=1 and index%1==0 and finite(weight) and
                        weight>0 and weight<=1 and not seen[index] then
                    weights[#weights+1]={index=index,weight=weight}
                    seen[index]=true
                end
            end
            table.sort(weights,function(a,b) return a.index<b.index end)
            region.vertexWeights=weights
            normalized[#normalized+1]=region
        end
    end
    project.generatedRegions=normalized
    return project
end

return M
