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

   Surface polygon helpers for the Facial Rig Editor.
]]--

local M={}

local function pointDistanceSquared(a,b)
    local dx,dy,dz=a.x-b.x,a.y-b.y,a.z-b.z
    return dx*dx+dy*dy+dz*dz
end

local function dot(a,b) return a.x*b.x+a.y*b.y+a.z*b.z end

local function subtract(a,b) return {x=a.x-b.x,y=a.y-b.y,z=a.z-b.z} end

local function cross(a,b)
    return {x=a.y*b.z-a.z*b.y,y=a.z*b.x-a.x*b.z,z=a.x*b.y-a.y*b.x}
end

local function length(value) return math.sqrt(dot(value,value)) end

local function normalize(value)
    local size=length(value)
    if size<=1e-8 then return nil end
    return {x=value.x/size,y=value.y/size,z=value.z/size}
end

function M.positionWithDepth(topology,index,position,depth)
    local point=position or topology and topology.vertices and topology.vertices[index]
    if not point then return nil end
    local amount=tonumber(depth) or 0
    if math.abs(amount)<=1e-8 or not topology or type(topology.triangles)~='table' then
        return {x=point.x,y=point.y,z=point.z}
    end
    local normal={x=0,y=0,z=0}
    for _,triangle in ipairs(topology.triangles) do
        if triangle[1]==index or triangle[2]==index or triangle[3]==index then
            local first=topology.vertices[triangle[1]]
            local second=topology.vertices[triangle[2]]
            local third=topology.vertices[triangle[3]]
            if first and second and third then
                local faceNormal=cross(subtract(second,first),subtract(third,first))
                normal.x=normal.x+faceNormal.x
                normal.y=normal.y+faceNormal.y
                normal.z=normal.z+faceNormal.z
            end
        end
    end
    normal=normalize(normal)
    if not normal then return {x=point.x,y=point.y,z=point.z} end
    return {x=point.x+normal.x*amount,y=point.y+normal.y*amount,z=point.z+normal.z*amount}
end

local function copyPoint(point)
    return {x=tonumber(point and point.x) or 0,y=tonumber(point and point.y) or 0,
        z=tonumber(point and point.z) or 0}
end

function M.resolvePoints(polygon,topology)
    local result,seen={},{}
    if type(polygon)~='table' or not topology or type(topology.vertices)~='table' then return result end
    for _,rawPoint in ipairs(polygon.points or {}) do
        local index=type(rawPoint)=='table' and tonumber(rawPoint.index) or tonumber(rawPoint)
        index=index and math.floor(index) or nil
        local point=index and topology.vertices[index]
        if point and not seen[index] then
            seen[index]=true
            result[#result+1]={index=index,position=copyPoint(point)}
        end
    end
    return result
end

function M.pointDistanceSquared(a,b) return pointDistanceSquared(a,b) end

function M.nearestPoint(points,point,maxDistance)
    local best,bestDistance=nil,(tonumber(maxDistance) or math.huge)^2
    for index,candidate in ipairs(points or {}) do
        local distance=pointDistanceSquared(candidate.position,point)
        if distance<=bestDistance then
            best,bestDistance=index,distance
        end
    end
    return best,best and math.sqrt(bestDistance) or nil
end

local function distanceToSegmentSquared(point,a,b)
    local edge=subtract(b,a)
    local edgeLengthSquared=dot(edge,edge)
    if edgeLengthSquared<=1e-12 then return pointDistanceSquared(point,a) end
    local amount=dot(subtract(point,a),edge)/edgeLengthSquared
    amount=math.max(0,math.min(1,amount))
    local closest={x=a.x+edge.x*amount,y=a.y+edge.y*amount,z=a.z+edge.z*amount}
    return pointDistanceSquared(point,closest)
end

function M.nearestEdge(points,point,closed,maxDistance)
    local count=#(points or {})
    if count<2 then return nil end
    local edgeCount=closed and count or count-1
    local best,bestDistance=nil,(tonumber(maxDistance) or math.huge)^2
    for index=1,edgeCount do
        local nextIndex=index%count+1
        local distance=distanceToSegmentSquared(point,points[index].position,points[nextIndex].position)
        if distance<=bestDistance then
            best,bestDistance=index,distance
        end
    end
    return best,best and math.sqrt(bestDistance) or nil
end

local function polygonBasis(points)
    local origin=points[1].position
    local normal={x=0,y=0,z=0}
    for index=1,#points do
        local current=points[index].position
        local nextPoint=points[index%#points+1].position
        normal.x=normal.x+(current.y-nextPoint.y)*(current.z+nextPoint.z)
        normal.y=normal.y+(current.z-nextPoint.z)*(current.x+nextPoint.x)
        normal.z=normal.z+(current.x-nextPoint.x)*(current.y+nextPoint.y)
    end
    normal=normalize(normal)
    if not normal then
        for index=2,#points-1 do
            normal=normalize(cross(subtract(points[index].position,origin),
                subtract(points[index+1].position,origin)))
            if normal then break end
        end
    end
    if not normal then return nil end
    local axis=normalize(subtract(points[2].position,origin))
    if not axis then return nil end
    local tangent=normalize(cross(normal,axis))
    if not tangent then return nil end
    return origin,axis,tangent,normal
end

local function project(point,origin,axis,tangent)
    local relative=subtract(point,origin)
    return dot(relative,axis),dot(relative,tangent)
end

local function pointInPolygon(x,y,polygon)
    local inside=false
    local previous=polygon[#polygon]
    for _,current in ipairs(polygon) do
        local crosses=(current.y>y)~=(previous.y>y)
        if crosses and x<(previous.x-current.x)*(y-current.y)/(previous.y-current.y)+current.x then
            inside=not inside
        end
        previous=current
    end
    return inside
end

function M.deriveVertexWeights(topology,points,closed)
    if not closed or #points<3 or not topology or not topology.vertices then return {} end
    local origin,axis,tangent,normal=polygonBasis(points)
    if not origin then return {} end
    local projected={}
    for _,point in ipairs(points) do
        local x,y=project(point.position,origin,axis,tangent)
        projected[#projected+1]={x=x,y=y}
    end
    local extent=0
    for _,point in ipairs(topology.vertices) do
        extent=math.max(extent,length(subtract(point,origin)))
    end
    local planeTolerance=math.max(extent*0.035,1e-5)
    local result,seen={},{}
    for index,point in ipairs(topology.vertices) do
        local distanceFromPlane=math.abs(dot(subtract(point,origin),normal))
        if distanceFromPlane<=planeTolerance then
            local x,y=project(point,origin,axis,tangent)
            if pointInPolygon(x,y,projected) then
                result[#result+1]={index=index,weight=1}
                seen[index]=true
            end
        end
    end
    for _,point in ipairs(points) do
        if not seen[point.index] then result[#result+1]={index=point.index,weight=1} end
    end
    table.sort(result,function(left,right) return left.index<right.index end)
    return result
end

function M.rebuild(polygon,topology)
    if type(polygon)~='table' then return false end
    local points=M.resolvePoints(polygon,topology)
    polygon.points={}
    for _,point in ipairs(points) do polygon.points[#polygon.points+1]={index=point.index} end
    polygon.derivedVertexWeights=M.deriveVertexWeights(topology,points,polygon.closed==true)
    polygon.topologySignature=topology and topology.signature or ''
    return true
end

return M
