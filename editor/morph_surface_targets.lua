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

local function copyPoint(value)
    return {x=finite(value and value.x,0),y=finite(value and value.y,0),
        z=finite(value and value.z,0)}
end

local function validPoint(value)
    return type(value)=='table' and value.x==value.x and value.y==value.y and value.z==value.z and
        value.x~=math.huge and value.x~=-math.huge and value.y~=math.huge and value.y~=-math.huge and
        value.z~=math.huge and value.z~=-math.huge
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
    target.pivot=validPoint(target.pivot) and copyPoint(target.pivot) or nil
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
    local result={vertices={},deltas={},pivot=validPoint(target.pivot) and copyPoint(target.pivot) or nil}
    for _,vertex in ipairs(target.vertices or {}) do
        result.vertices[#result.vertices+1]={index=vertex.index,weight=vertex.weight}
    end
    for _,delta in ipairs(target.deltas or {}) do
        result.deltas[#result.deltas+1]={index=delta.index,x=delta.x,y=delta.y,z=delta.z}
    end
    return result
end

local adjacencyCache=setmetatable({}, {__mode='k'})

local function buildAdjacency(topology)
    local adjacency=adjacencyCache[topology]
    if adjacency then return adjacency end
    adjacency={}
    for _,triangle in ipairs(topology and topology.triangles or {}) do
        local a,b,c=triangle[1],triangle[2],triangle[3]
        if topology.vertices[a] and topology.vertices[b] and topology.vertices[c] then
            for edge=1,3 do
                local first=triangle[edge]
                local second=triangle[edge%3+1]
                if first~=second then
                    adjacency[first]=adjacency[first] or {}
                    adjacency[second]=adjacency[second] or {}
                    adjacency[first][second]=true
                    adjacency[second][first]=true
                end
            end
        end
    end
    adjacencyCache[topology]=adjacency
    return adjacency
end

function M.featherSelection(topology,target,rings)
    rings=math.max(0,math.min(8,math.floor(finite(rings,0))))
    local weights,frontier={},{}
    for _,vertex in ipairs(target and target.vertices or {}) do
        local index=tonumber(vertex.index)
        if index then
            weights[index]=math.max(weights[index] or 0,math.max(0,math.min(1,finite(vertex.weight,1))))
            frontier[#frontier+1]=index
        end
    end
    if rings>0 and topology then
        local adjacency=buildAdjacency(topology)
        local visited={}
        for index in pairs(weights) do visited[index]=true end
        for ring=1,rings do
            local nextFrontier={}
            local t=ring/(rings+1)
            local influence=1-t*t*(3-2*t)
            for _,index in ipairs(frontier) do
                for neighbor in pairs(adjacency[index] or {}) do
                    if not visited[neighbor] then
                        visited[neighbor]=true
                        weights[neighbor]=influence
                        nextFrontier[#nextFrontier+1]=neighbor
                    end
                end
            end
            frontier=nextFrontier
            if #frontier==0 then break end
        end
    end
    local indices={}
    for index,weight in pairs(weights) do
        if weight>0 and (not topology or topology.vertices[index]) then indices[#indices+1]=index end
    end
    table.sort(indices)
    return indices,weights
end

function M.ensureSelection(target,indices,weights)
    if not target or not indices then return end
    local byIndex={}
    for _,vertex in ipairs(target.vertices or {}) do byIndex[vertex.index]=vertex.weight end
    for _,rawIndex in ipairs(indices) do
        local index=tonumber(rawIndex)
        local weight=weights and weights[index] or 1
        if index and weight and weight>0 then
            byIndex[index]=math.max(byIndex[index] or 0,math.min(1,weight))
        end
    end
    target.vertices={}
    for index,weight in pairs(byIndex) do
        target.vertices[#target.vertices+1]={index=index,weight=weight}
    end
    table.sort(target.vertices,function(left,right) return left.index<right.index end)
end

-- Apply one local brush stamp to a target candidate. The caller owns the hit
-- testing and supplies only vertices belonging to the target selection.
function M.brushStamp(target,baseVertices,point,normal,radius,strength,offset,smooth,allowed,indices,
    topology,smoothPasses)
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
    local changed=0
    if smooth then
        local adjacency=topology and buildAdjacency(topology) or nil
        smoothPasses=math.max(1,math.min(8,math.floor(tonumber(smoothPasses) or 1)))
        for _=1,smoothPasses do
            local nextPositions={}
            for _,entry in ipairs(affected) do
                local index=entry.index
                local old=positions[index] or {x=0,y=0,z=0}
                local average={x=old.x,y=old.y,z=old.z}
                local sum=1
                if adjacency then
                    for neighbor in pairs(adjacency[index] or {}) do
                        local neighborWeight=allowed and allowed[neighbor]
                        if neighborWeight then
                            local delta=positions[neighbor] or {x=0,y=0,z=0}
                            average.x=average.x+delta.x*neighborWeight
                            average.y=average.y+delta.y*neighborWeight
                            average.z=average.z+delta.z*neighborWeight
                            sum=sum+neighborWeight
                        end
                    end
                else
                    for _,other in ipairs(affected) do
                        local delta=positions[other.index] or {x=0,y=0,z=0}
                        average.x=average.x+delta.x*other.weight
                        average.y=average.y+delta.y*other.weight
                        average.z=average.z+delta.z*other.weight
                        sum=sum+other.weight
                    end
                end
                average.x,average.y,average.z=average.x/sum,average.y/sum,average.z/sum
                local nextWeight=entry.weight
                nextPositions[index]={x=old.x+(average.x-old.x)*nextWeight,
                    y=old.y+(average.y-old.y)*nextWeight,z=old.z+(average.z-old.z)*nextWeight}
            end
            for index,value in pairs(nextPositions) do positions[index]=value end
        end
        for _,entry in ipairs(affected) do
            if nonZero(positions[entry.index] or {x=0,y=0,z=0}) then changed=changed+1 end
        end
    else
        for _,entry in ipairs(affected) do
            local old=positions[entry.index] or {x=0,y=0,z=0}
            local weight=entry.weight
            local nextPosition={x=old.x+(offset.x or 0)*weight,
                y=old.y+(offset.y or 0)*weight,z=old.z+(offset.z or 0)*weight}
            if math.abs(nextPosition.x-old.x)>1e-8 or math.abs(nextPosition.y-old.y)>1e-8 or
                    math.abs(nextPosition.z-old.z)>1e-8 then changed=changed+1 end
            positions[entry.index]=nextPosition
        end
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

function M.applyTranslation(target,dx,dy,dz,indices,weights)
    if type(target)~='table' then return 0 end
    if indices then M.ensureSelection(target,indices,weights) end
    local byIndex=copyDeltasByIndex(target.deltas)
    local changed=0
    dx,dy,dz=finite(dx,0),finite(dy,0),finite(dz,0)
    local selection=indices
    if not selection then selection=target.vertices end
    for _,entry in ipairs(selection or {}) do
        local index=type(entry)=='table' and entry.index or tonumber(entry)
        local sourceWeight=type(entry)=='table' and entry.weight or weights and weights[index] or 1
        local delta=byIndex[index] or {x=0,y=0,z=0}
        local weight=finite(sourceWeight,1)
        delta={x=delta.x+dx*weight,y=delta.y+dy*weight,z=delta.z+dz*weight}
        if nonZero(delta) then
            byIndex[index]=delta
            changed=changed+1
        else
            byIndex[index]=nil
        end
    end
    target.deltas={}
    for index,delta in pairs(byIndex) do
        target.deltas[#target.deltas+1]={index=index,x=delta.x,y=delta.y,z=delta.z}
    end
    table.sort(target.deltas,function(left,right) return left.index<right.index end)
    return changed
end

local function rotateVector(value,rx,ry,rz)
    local x,y,z=value.x,value.y,value.z
    local cx,sx=math.cos(rx),math.sin(rx)
    y,z=y*cx-z*sx,y*sx+z*cx
    local cy,sy=math.cos(ry),math.sin(ry)
    x,z=x*cy+z*sy,-x*sy+z*cy
    local cz,sz=math.cos(rz),math.sin(rz)
    x,y=x*cz-y*sz,x*sz+y*cz
    return {x=x,y=y,z=z}
end

function M.rotationBasis(rx,ry,rz)
    return {x=rotateVector({x=1,y=0,z=0},rx,ry,rz),
        y=rotateVector({x=0,y=1,z=0},rx,ry,rz),
        z=rotateVector({x=0,y=0,z=1},rx,ry,rz)}
end

function M.rotatedDelta(base,delta,pivot,basis)
    local x,y,z=base.x+delta.x-pivot.x,base.y+delta.y-pivot.y,base.z+delta.z-pivot.z
    return {
        x=pivot.x+x*basis.x.x+y*basis.y.x+z*basis.z.x-base.x,
        y=pivot.y+x*basis.x.y+y*basis.y.y+z*basis.z.y-base.y,
        z=pivot.z+x*basis.x.z+y*basis.y.z+z*basis.z.z-base.z,
    }
end

function M.selectionPivot(target,baseVertices,bounds)
    if validPoint(target and target.pivot) then return copyPoint(target.pivot) end
    local x,y,z,total=0,0,0,0
    for _,vertex in ipairs(target and target.vertices or {}) do
        local base=baseVertices and baseVertices[vertex.index]
        if base then
            local weight=math.max(0,finite(vertex.weight,1))
            x,y,z=x+base.x*weight,y+base.y*weight,z+base.z*weight
            total=total+weight
        end
    end
    if total>0 then return {x=x/total,y=y/total,z=z/total} end
    if bounds then
        return {x=(bounds.minX+bounds.maxX)*0.5,y=(bounds.minY+bounds.maxY)*0.5,
            z=(bounds.minZ+bounds.maxZ)*0.5}
    end
    return {x=0,y=0,z=0}
end

function M.applyRotation(target,baseVertices,rx,ry,rz,pivot,indices,weights)
    if type(target)~='table' or type(baseVertices)~='table' then return 0 end
    if indices then M.ensureSelection(target,indices,weights) end
    local byIndex=copyDeltasByIndex(target.deltas)
    local basis=M.rotationBasis(rx,ry,rz)
    pivot=pivot or {x=0,y=0,z=0}
    local changed=0
    local selection=indices
    if not selection then selection=target.vertices end
    for _,entry in ipairs(selection or {}) do
        local index=type(entry)=='table' and tonumber(entry.index) or tonumber(entry)
        local base=index and baseVertices[index]
        if index and base then
            local old=byIndex[index] or {x=0,y=0,z=0}
            local rotated=M.rotatedDelta(base,old,pivot,basis)
            local sourceWeight=type(entry)=='table' and entry.weight or weights and weights[index] or 1
            local weight=math.max(0,math.min(1,finite(sourceWeight,1)))
            local next={x=old.x+(rotated.x-old.x)*weight,
                y=old.y+(rotated.y-old.y)*weight,z=old.z+(rotated.z-old.z)*weight}
            if math.abs(next.x-old.x)>1e-8 or math.abs(next.y-old.y)>1e-8 or
                    math.abs(next.z-old.z)>1e-8 then changed=changed+1 end
            if nonZero(next) then byIndex[index]=next else byIndex[index]=nil end
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
