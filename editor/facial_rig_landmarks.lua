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

   Facial rig landmark and topology helpers.
]]--

local M = {}

local function hashByte(hash, byte)
    local mixed=math.tointeger(hash) ~ math.tointeger(byte & 0xff)
    return math.tointeger((mixed * 16777619) % 4294967296)
end

local function hashNumber(hash, value)
    local quantized = math.floor((tonumber(value) or 0) * 100000 + 0.5)
    for shift = 0, 24, 8 do hash = hashByte(hash, (quantized >> shift) & 0xff) end
    return hash
end

local function hashCount(hash, value)
    value = math.max(0, math.floor(tonumber(value) or 0))
    for shift = 0, 24, 8 do hash = hashByte(hash, (value >> shift) & 0xff) end
    return hash
end

local function pointDistanceSquared(a, b)
    local dx,dy,dz=a.x-b.x,a.y-b.y,a.z-b.z
    return dx*dx+dy*dy+dz*dz
end

function M.inspect(meshDebugObject)
    local result={vertices={},refs={},bounds={minX=math.huge,minY=math.huge,minZ=math.huge,
        maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge},signature='',vertexCount=0}
    local hash=2166136261
    local subsetCount=meshDebugObject:getTotalSubset(1)
    hash=hashCount(hash,subsetCount)
    for subset=1,subsetCount do
        local count=meshDebugObject:getTotalVertex(1,subset)
        hash=hashCount(hash,count)
        local indices=meshDebugObject:getIndex(1,subset)
        if type(indices)=='table' then
            hash=hashCount(hash,#indices)
            for _,index in ipairs(indices) do hash=hashCount(hash,index) end
        else
            hash=hashCount(hash,0)
        end
        for vertex=1,count do
            local point=meshDebugObject:getVertex(1,subset,vertex)
            if point then
                local copy={x=tonumber(point.x) or 0,y=tonumber(point.y) or 0,z=tonumber(point.z) or 0}
                result.vertices[#result.vertices+1]=copy
                result.refs[#result.refs+1]={subset=subset,vertex=vertex}
                result.bounds.minX=math.min(result.bounds.minX,copy.x)
                result.bounds.minY=math.min(result.bounds.minY,copy.y)
                result.bounds.minZ=math.min(result.bounds.minZ,copy.z)
                result.bounds.maxX=math.max(result.bounds.maxX,copy.x)
                result.bounds.maxY=math.max(result.bounds.maxY,copy.y)
                result.bounds.maxZ=math.max(result.bounds.maxZ,copy.z)
                hash=hashNumber(hash,copy.x); hash=hashNumber(hash,copy.y); hash=hashNumber(hash,copy.z)
            end
        end
    end
    result.vertexCount=#result.vertices
    if result.vertexCount==0 then result.bounds=nil end
    result.signature=string.format('%08x:%d',hash,result.vertexCount)
    return result
end

function M.nearestVertex(topology, origin, direction, maxDistance)
    if not topology or not topology.vertices or not origin or not direction then return nil end
    local bestIndex,bestDistance,bestDepth=nil,math.huge,math.huge
    local directionLength=math.sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z)
    if directionLength<=1e-8 then return nil end
    local dx,dy,dz=direction.x/directionLength,direction.y/directionLength,direction.z/directionLength
    local limitSquared=(tonumber(maxDistance) or math.huge)^2
    for index,point in ipairs(topology.vertices) do
        local ox,oy,oz=point.x-origin.x,point.y-origin.y,point.z-origin.z
        local depth=ox*dx+oy*dy+oz*dz
        if depth>=0 then
            local hit={x=origin.x+dx*depth,y=origin.y+dy*depth,z=origin.z+dz*depth}
            local distance=pointDistanceSquared(point,hit)
            if distance<=limitSquared and (distance<bestDistance or
                    distance==bestDistance and depth<bestDepth) then
                bestIndex,bestDistance,bestDepth=index,distance,depth
            end
        end
    end
    return bestIndex and {index=bestIndex,point=topology.vertices[bestIndex],distance=math.sqrt(bestDistance)} or nil
end

function M.nearestLandmark(landmarks, origin, direction, maxDistance)
    if not landmarks or not origin or not direction then return nil end
    local directionLength=math.sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z)
    if directionLength<=1e-8 then return nil end
    local dx,dy,dz=direction.x/directionLength,direction.y/directionLength,direction.z/directionLength
    local limitSquared=(tonumber(maxDistance) or math.huge)^2
    local bestIndex,bestDistance,bestDepth=nil,math.huge,math.huge
    for index,landmark in ipairs(landmarks) do
        local point=landmark.placed and landmark.position
        if point then
            local ox,oy,oz=point.x-origin.x,point.y-origin.y,point.z-origin.z
            local depth=ox*dx+oy*dy+oz*dz
            if depth>=0 then
                local hx,hy,hz=origin.x+dx*depth,origin.y+dy*depth,origin.z+dz*depth
                local px,py,pz=point.x-hx,point.y-hy,point.z-hz
                local distance=px*px+py*py+pz*pz
                if distance<=limitSquared and (distance<bestDistance or
                        distance==bestDistance and depth<bestDepth) then
                    bestIndex,bestDistance,bestDepth=index,distance,depth
                end
            end
        end
    end
    return bestIndex and {index=bestIndex,landmark=landmarks[bestIndex],distance=math.sqrt(bestDistance)} or nil
end

function M.pointForVertex(topology, index)
    return topology and topology.vertices and topology.vertices[tonumber(index) or 0]
end

return M
