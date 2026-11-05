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
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Generic neutral-mesh matching for morph-target mirroring.
]]--

local M={}

local function finite(value,default)
    local number=tonumber(value)
    if not number or number~=number or number==math.huge or number==-math.huge then
        return default or 0
    end
    return number
end

local function nonZero(delta)
    return delta and (math.abs(finite(delta.x,0))>1e-8 or
        math.abs(finite(delta.y,0))>1e-8 or math.abs(finite(delta.z,0))>1e-8)
end

local function axisField(axis)
    return axis==1 and 'x' or axis==2 and 'y' or 'z'
end

local function distanceSquared(first,second)
    local dx,dy,dz=first.x-second.x,first.y-second.y,first.z-second.z
    return dx*dx+dy*dy+dz*dz
end

local function cellCoordinate(value,size)
    return math.floor(value/size)
end

local function cellKey(x,y,z)
    return x..':'..y..':'..z
end

local function buildSpatialIndex(vertices,size)
    local index={}
    for vertex,point in ipairs(vertices) do
        if type(point)=='table' then
            local key=cellKey(cellCoordinate(point.x,size),cellCoordinate(point.y,size),
                cellCoordinate(point.z,size))
            local bucket=index[key]
            if not bucket then bucket={}; index[key]=bucket end
            bucket[#bucket+1]=vertex
        end
    end
    return index
end

local function findCandidates(vertices,index,point,radius)
    local cell=radius
    local cx,cy,cz=cellCoordinate(point.x,cell),cellCoordinate(point.y,cell),
        cellCoordinate(point.z,cell)
    local radiusSquared=radius*radius
    local candidates={}
    for dx=-1,1 do
        for dy=-1,1 do
            for dz=-1,1 do
                local bucket=index[cellKey(cx+dx,cy+dy,cz+dz)]
                for _,vertex in ipairs(bucket or {}) do
                    local candidate=vertices[vertex]
                    local distance=distanceSquared(candidate,point)
                    if distance<=radiusSquared then
                        candidates[#candidates+1]={index=vertex,distance=distance}
                    end
                end
            end
        end
    end
    table.sort(candidates,function(left,right)
        if left.distance==right.distance then return left.index<right.index end
        return left.distance<right.distance
    end)
    return candidates
end

local function copyDelta(delta)
    return {x=finite(delta and delta.x,0),y=finite(delta and delta.y,0),
        z=finite(delta and delta.z,0)}
end

local function deltaMap(target)
    local result={}
    for _,delta in ipairs(target and target.deltas or {}) do
        local index=tonumber(delta.index)
        if index and index>=1 and index%1==0 then result[index]=copyDelta(delta) end
    end
    return result
end

local function validPoint(point)
    return type(point)=='table' and tonumber(point.x) and tonumber(point.y) and tonumber(point.z)
end

-- Analyze only the vertices selected by the source target. The neutral mesh is
-- indexed once per analysis, so this operation never runs in the editor loop.
function M.analyze(target,vertices,axisOrPlane,planeOrRadius,radiusArgument)
    local axis,plane,radius
    if radiusArgument==nil then
        axis,plane,radius=1,tonumber(axisOrPlane) or 0,tonumber(planeOrRadius)
    else
        axis,plane,radius=math.floor(tonumber(axisOrPlane) or 1),tonumber(planeOrRadius) or 0,
            tonumber(radiusArgument)
    end
    if type(target)~='table' or type(vertices)~='table' or #vertices==0 or
            axis<1 or axis>3 or type(plane)~='number' or plane~=plane or
            type(radius)~='number' or radius<=0 then
        return nil,'invalid'
    end
    if #(target.vertices or {})==0 then return nil,'empty_target' end
    local index=buildSpatialIndex(vertices,radius)
    local deltas=deltaMap(target)
    local result={mapping={},matched=0,unmatched=0,ambiguous=0,total=#target.vertices,
        totalActive=0,unmatchedActive=0,vertexCount=#target.vertices}
    local destinationOwner={}
    local ambiguityDistance=math.max(radius*radius*1e-5,1e-12)
    for _,entry in ipairs(target.vertices) do
        local source=tonumber(entry.index)
        local point=source and vertices[source]
        local matched=nil
        local ambiguous=false
        if point then
            local reflected={x=point.x,y=point.y,z=point.z}
            local field=axisField(axis)
            reflected[field]=2*plane-reflected[field]
            local candidates=findCandidates(vertices,index,reflected,radius)
            if #candidates>0 then
                matched=candidates[1].index
                for candidateIndex=2,#candidates do
                    if candidates[candidateIndex].distance<=candidates[1].distance+ambiguityDistance then
                        ambiguous=true
                        break
                    end
                end
                local owner=destinationOwner[matched]
                if owner and owner~=source then
                    matched=nil
                    ambiguous=true
                else
                    destinationOwner[matched]=source
                end
            end
        end
        if matched then
            result.mapping[source]=matched
            result.matched=result.matched+1
            if ambiguous then result.ambiguous=result.ambiguous+1 end
        else
            result.unmatched=result.unmatched+1
            if ambiguous then result.ambiguous=result.ambiguous+1 end
        end
        if nonZero(deltas[source]) then
            result.totalActive=result.totalActive+1
            if not matched then result.unmatchedActive=result.unmatchedActive+1 end
        end
    end
    return result
end

function M.isApproximateEligible(result)
    if type(result)~='table' or result.vertexCount<=0 then return false end
    return result.matched>=result.vertexCount*0.8 and
        result.unmatchedActive<=math.max(1,result.totalActive*0.01) and
        result.ambiguous<=math.max(1,result.vertexCount*0.001)
end

function M.create(target,analysis,axisOrPlane,planeArgument)
    local axis,plane
    if planeArgument==nil then
        axis,plane=1,tonumber(axisOrPlane) or 0
    else
        axis,plane=math.floor(tonumber(axisOrPlane) or 1),tonumber(planeArgument) or 0
    end
    if type(target)~='table' or type(analysis)~='table' or
            type(analysis.mapping)~='table' or axis<1 or axis>3 then return nil,'invalid' end
    local deltas=deltaMap(target)
    local verticesByIndex={}
    local mirrored={vertices={},deltas={},pivot=nil}
    for _,entry in ipairs(target.vertices or {}) do
        local source=tonumber(entry.index)
        local destination=source and analysis.mapping[source]
        if destination then
            local weight=math.max(0,math.min(1,finite(entry.weight,1)))
            local previous=verticesByIndex[destination]
            if not previous or weight>previous then verticesByIndex[destination]=weight end
            local delta=deltas[source]
            if delta and nonZero(delta) then
                local reflectedDelta=copyDelta(delta)
                reflectedDelta[axisField(axis)]=-reflectedDelta[axisField(axis)]
                mirrored.deltas[#mirrored.deltas+1]={index=destination,x=reflectedDelta.x,
                    y=reflectedDelta.y,z=reflectedDelta.z}
            end
        end
    end
    for index,weight in pairs(verticesByIndex) do
        mirrored.vertices[#mirrored.vertices+1]={index=index,weight=weight}
    end
    table.sort(mirrored.vertices,function(left,right) return left.index<right.index end)
    table.sort(mirrored.deltas,function(left,right) return left.index<right.index end)
    if validPoint(target.pivot) then
        mirrored.pivot={x=finite(target.pivot.x,0),y=finite(target.pivot.y,0),
            z=finite(target.pivot.z,0)}
        local field=axisField(axis)
        mirrored.pivot[field]=2*plane-mirrored.pivot[field]
    end
    return mirrored
end

return M
