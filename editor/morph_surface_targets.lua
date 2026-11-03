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

   Generic morph-target records built from surface-region selections.
]]--

local tRegions=require 'morph_surface_regions'
local M={}

local function finite(value,default)
    local number=tonumber(value)
    if not number or number~=number or number==math.huge or number==-math.huge then
        return default or 0
    end
    return number
end

local function copyDelta(value)
    return {x=finite(value and value.x,0),y=finite(value and value.y,0),
        z=finite(value and value.z,0)}
end

local function nonZero(value)
    return math.abs(value.x)>1e-8 or math.abs(value.y)>1e-8 or math.abs(value.z)>1e-8
end

local function copyDeltasByIndex(deltas)
    local result={}
    for _,raw in ipairs(type(deltas)=='table' and deltas or {}) do
        if type(raw)=='table' then
            local index=tonumber(raw.index)
            if index and index>=1 and index%1==0 then result[index]=copyDelta(raw) end
        end
    end
    return result
end

function M.new(name,region,id)
    local target={id=type(id)=='string' and id or '',name=type(name)=='string' and name or '',
        sourceRegionId=region and region.id or '',topologySignature=region and region.topologySignature or '',
        sourceFingerprint=region and region.geometryFingerprint or '',vertices={},deltas={}}
    if region then M.rebuildFromRegion(target,region) end
    return target
end

function M.normalize(target,vertexCount)
    if type(target)~='table' then return nil,'invalid_target' end
    target.id=type(target.id)=='string' and target.id or ''
    target.name=type(target.name)=='string' and target.name or ''
    target.sourceRegionId=type(target.sourceRegionId)=='string' and target.sourceRegionId or ''
    target.topologySignature=type(target.topologySignature)=='string' and target.topologySignature or ''
    target.sourceFingerprint=type(target.sourceFingerprint)=='string' and target.sourceFingerprint or ''
    local vertices,seen={},{}
    for _,raw in ipairs(type(target.vertices)=='table' and target.vertices or {}) do
        local index=type(raw)=='table' and tonumber(raw.index) or tonumber(raw)
        local weight=type(raw)=='table' and finite(raw.weight,1) or 1
        if index and index>=1 and index%1==0 and (not vertexCount or index<=vertexCount) and
                weight>0 and weight<=1 and not seen[index] then
            vertices[#vertices+1]={index=math.floor(index),weight=weight}
            seen[index]=true
        end
    end
    table.sort(vertices,function(left,right) return left.index<right.index end)
    local deltas,deltaSeen={},{}
    for _,raw in ipairs(type(target.deltas)=='table' and target.deltas or {}) do
        local index=type(raw)=='table' and tonumber(raw.index) or nil
        if index and index>=1 and index%1==0 and (not vertexCount or index<=vertexCount) and
                seen[index] and not deltaSeen[index] then
            local delta=copyDelta(raw)
            if nonZero(delta) then
                deltas[#deltas+1]={index=math.floor(index),x=delta.x,y=delta.y,z=delta.z}
            end
            deltaSeen[index]=true
        end
    end
    table.sort(deltas,function(left,right) return left.index<right.index end)
    target.vertices=vertices
    target.deltas=deltas
    return target
end

function M.rebuildFromRegion(target,region)
    if type(target)~='table' or type(region)~='table' then return false,'missing_region' end
    local oldDeltas=copyDeltasByIndex(target.deltas)
    target.sourceRegionId=region.id or ''
    target.topologySignature=region.topologySignature or ''
    target.sourceFingerprint=region.geometryFingerprint or ''
    target.vertices={}
    target.deltas={}
    for _,raw in ipairs(region.derivedVertexWeights or {}) do
        local index=tonumber(raw.index)
        if index and index>=1 and index%1==0 then
            local weight=math.max(0,math.min(1,finite(raw.weight,1)))
            if weight>0 then
                target.vertices[#target.vertices+1]={index=math.floor(index),weight=weight}
                local delta=oldDeltas[math.floor(index)]
                if delta and nonZero(delta) then
                    target.deltas[#target.deltas+1]={index=math.floor(index),x=delta.x,y=delta.y,z=delta.z}
                end
            end
        end
    end
    table.sort(target.vertices,function(left,right) return left.index<right.index end)
    table.sort(target.deltas,function(left,right) return left.index<right.index end)
    return true
end

function M.status(target,region,topology)
    if not target then return 'missing_target' end
    if not region then return 'missing_source' end
    if not topology then return 'missing_topology' end
    if target.topologySignature~=topology.signature then return 'topology_mismatch' end
    local regionStatus=tRegions.status(region,topology)
    if regionStatus~='ready' then return 'source_'..regionStatus end
    if target.sourceRegionId~=region.id then return 'source_changed' end
    if target.sourceFingerprint~=region.geometryFingerprint then return 'source_stale' end
    if #(target.vertices or {})==0 then return 'empty_target' end
    return 'ready'
end

function M.deltaCount(target)
    return #(target and target.deltas or {})
end

function M.copyTargetData(target)
    if not target then return nil end
    local result={vertices={},deltas={}}
    for _,vertex in ipairs(target.vertices or {}) do
        result.vertices[#result.vertices+1]={index=vertex.index,weight=vertex.weight}
    end
    for _,delta in ipairs(target.deltas or {}) do
        result.deltas[#result.deltas+1]={index=delta.index,x=delta.x,y=delta.y,z=delta.z}
    end
    return result
end

-- Apply one local brush stamp to a target candidate. The caller owns the hit
-- testing and supplies only vertices belonging to the target selection.
function M.brushStamp(target,baseVertices,point,normal,radius,strength,offset,smooth,allowed,indices)
    if not target or not baseVertices or not point or not normal or radius<=0 or strength<=0 then return 0 end
    local positions=copyDeltasByIndex(target.deltas)
    local affected={}
    local radiusSquared=radius*radius
    local nx,ny,nz=normal.x,normal.y,normal.z
    for _,index in ipairs(indices or {}) do
        local base=baseVertices[index]
        local selectionWeight=allowed and allowed[index] or 1
        if base and selectionWeight then
            local x,y,z=base.x-point.x,base.y-point.y,base.z-point.z
            local normalDistance=x*nx+y*ny+z*nz
            local tangentSquared=x*x+y*y+z*z-normalDistance*normalDistance
            if tangentSquared<=radiusSquared and math.abs(normalDistance)<=radius*0.45 then
                local distance=math.sqrt(math.max(0,tangentSquared))/radius
                local falloff=(1-distance*distance)^2*strength*selectionWeight
                if falloff>1e-6 then affected[#affected+1]={index=index,weight=falloff} end
            end
        end
    end
    if #affected==0 then return 0 end
    local average={x=0,y=0,z=0}
    if smooth then
        local sum=0
        for _,entry in ipairs(affected) do
            local delta=positions[entry.index] or {x=0,y=0,z=0}
            average.x=average.x+delta.x*entry.weight
            average.y=average.y+delta.y*entry.weight
            average.z=average.z+delta.z*entry.weight
            sum=sum+entry.weight
        end
        if sum>0 then
            average.x,average.y,average.z=average.x/sum,average.y/sum,average.z/sum
        end
    end
    local changed=0
    for _,entry in ipairs(affected) do
        local old=positions[entry.index] or {x=0,y=0,z=0}
        local weight=entry.weight
        local nextPosition
        if smooth then
            nextPosition={x=old.x+(average.x-old.x)*weight,
                y=old.y+(average.y-old.y)*weight,z=old.z+(average.z-old.z)*weight}
        else
            nextPosition={x=old.x+(offset.x or 0)*weight,
                y=old.y+(offset.y or 0)*weight,z=old.z+(offset.z or 0)*weight}
        end
        if math.abs(nextPosition.x-old.x)>1e-8 or math.abs(nextPosition.y-old.y)>1e-8 or
                math.abs(nextPosition.z-old.z)>1e-8 then changed=changed+1 end
        positions[entry.index]=nextPosition
    end
    target.deltas={}
    for index,delta in pairs(positions) do
        if nonZero(delta) then
            target.deltas[#target.deltas+1]={index=index,x=delta.x,y=delta.y,z=delta.z}
        end
    end
    table.sort(target.deltas,function(left,right) return left.index<right.index end)
    return changed
end

function M.applyTranslation(target,dx,dy,dz)
    if type(target)~='table' then return 0 end
    local byIndex=copyDeltasByIndex(target.deltas)
    local changed=0
    dx,dy,dz=finite(dx,0),finite(dy,0),finite(dz,0)
    for _,vertex in ipairs(target.vertices or {}) do
        local delta=byIndex[vertex.index] or {x=0,y=0,z=0}
        local weight=finite(vertex.weight,1)
        delta={x=delta.x+dx*weight,y=delta.y+dy*weight,z=delta.z+dz*weight}
        if nonZero(delta) then
            byIndex[vertex.index]=delta
            changed=changed+1
        else
            byIndex[vertex.index]=nil
        end
    end
    target.deltas={}
    for index,delta in pairs(byIndex) do
        target.deltas[#target.deltas+1]={index=index,x=delta.x,y=delta.y,z=delta.z}
    end
    table.sort(target.deltas,function(left,right) return left.index<right.index end)
    return changed
end

function M.resetDeltas(target)
    if not target or #(target.deltas or {})==0 then return false end
    target.deltas={}
    return true
end

return M
