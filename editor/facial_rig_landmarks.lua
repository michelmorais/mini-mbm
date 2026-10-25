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
    local result={vertices={},refs={},triangles={},bounds={minX=math.huge,minY=math.huge,minZ=math.huge,
        maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge},signature='',vertexCount=0}
    local hash=2166136261
    local subsetCount=meshDebugObject:getTotalSubset(1)
    hash=hashCount(hash,subsetCount)
    for subset=1,subsetCount do
        local count=meshDebugObject:getTotalVertex(1,subset)
        hash=hashCount(hash,count)
        local indices=meshDebugObject:getIndex(1,subset)
        local globalOffset=#result.vertices
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
        if type(indices)=='table' and #indices>=3 then
            for index=1,#indices-2,3 do
                local a,b,c=tonumber(indices[index]),tonumber(indices[index+1]),tonumber(indices[index+2])
                if a and b and c and a>=1 and b>=1 and c>=1 and
                        a<=count and b<=count and c<=count then
                    result.triangles[#result.triangles+1]={globalOffset+a,globalOffset+b,globalOffset+c}
                end
            end
        elseif count>=3 then
            for vertex=1,count-2,3 do
                result.triangles[#result.triangles+1]={globalOffset+vertex,globalOffset+vertex+1,globalOffset+vertex+2}
            end
        end
    end
    result.vertexCount=#result.vertices
    if result.vertexCount==0 then result.bounds=nil end
    result.signature=string.format('%08x:%d',hash,result.vertexCount)
    return result
end

local function dot(a,b)
    return a.x*b.x+a.y*b.y+a.z*b.z
end

local function cross(a,b)
    return {x=a.y*b.z-a.z*b.y,y=a.z*b.x-a.x*b.z,z=a.x*b.y-a.y*b.x}
end

local function subtract(a,b)
    return {x=a.x-b.x,y=a.y-b.y,z=a.z-b.z}
end

local function addScaled(a,b,scale)
    return {x=a.x+b.x*scale,y=a.y+b.y*scale,z=a.z+b.z*scale}
end

-- Intersect a viewport/world ray against the inspected topology. This is an
-- explicit authoring operation, so a linear triangle scan is preferable to a
-- continuously maintained spatial index: it avoids idle CPU work and keeps
-- the result deterministic for a single snap click.
function M.intersectRaySurface(topology, origin, direction, allowedVertices)
    if not topology or not topology.vertices or not topology.triangles or
            not origin or not direction then return nil end
    local directionLength=math.sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z)
    if directionLength<=1e-8 then return nil end
    local ray={x=direction.x/directionLength,y=direction.y/directionLength,z=direction.z/directionLength}
    local epsilon=1e-7
    local bestDistance=math.huge
    local best=nil
    for _,triangle in ipairs(topology.triangles) do
        local ia,ib,ic=triangle[1],triangle[2],triangle[3]
        if (not allowedVertices) or
                (allowedVertices[ia] and allowedVertices[ib] and allowedVertices[ic]) then
            local a,b,c=topology.vertices[ia],topology.vertices[ib],topology.vertices[ic]
            if a and b and c then
                local edge1,edge2=subtract(b,a),subtract(c,a)
                local pvec=cross(ray,edge2)
                local determinant=dot(edge1,pvec)
                if math.abs(determinant)>epsilon then
                    local inverse=1/determinant
                    local tvec=subtract(origin,a)
                    local u=dot(tvec,pvec)*inverse
                    if u>=-epsilon and u<=1+epsilon then
                        local qvec=cross(tvec,edge1)
                        local v=dot(ray,qvec)*inverse
                        if v>=-epsilon and u+v<=1+epsilon then
                            local distance=dot(edge2,qvec)*inverse
                            if distance>=0 and distance<bestDistance then
                                local normal=cross(edge1,edge2)
                                local normalLength=math.sqrt(dot(normal,normal))
                                if normalLength>epsilon then
                                    normal={x=normal.x/normalLength,y=normal.y/normalLength,z=normal.z/normalLength}
                                    -- Orient the normal toward the ray origin so
                                    -- positive offsets move outward from the surface.
                                    if dot(normal,ray)>0 then
                                        normal={x=-normal.x,y=-normal.y,z=-normal.z}
                                    end
                                else
                                    normal={x=0,y=0,z=0}
                                end
                                bestDistance=distance
                                best={point=addScaled(origin,ray,distance),normal=normal,
                                    distance=distance,triangle=triangle}
                            end
                        end
                    end
                end
            end
        end
    end
    return best
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
