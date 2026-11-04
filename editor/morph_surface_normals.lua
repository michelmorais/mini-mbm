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

   Normal calculation for explicit generic morph-target export.
]]--

local M={}

local function finite(value,default)
    local number=tonumber(value)
    if not number or number~=number or number==math.huge or number==-math.huge then
        return default or 0
    end
    return number
end

local function vector(value)
    return {x=finite(value and value.x),y=finite(value and value.y),z=finite(value and value.z)}
end

local function cross(a,b)
    return {x=a.y*b.z-a.z*b.y,y=a.z*b.x-a.x*b.z,z=a.x*b.y-a.y*b.x}
end

local function subtract(a,b)
    return {x=a.x-b.x,y=a.y-b.y,z=a.z-b.z}
end

local function length(value)
    return math.sqrt(value.x*value.x+value.y*value.y+value.z*value.z)
end

local function normalized(value,fallback)
    local magnitude=length(value)
    if magnitude<=1e-8 then return vector(fallback) end
    return {x=value.x/magnitude,y=value.y/magnitude,z=value.z/magnitude}
end

local function add(sum,value)
    sum.x=sum.x+value.x
    sum.y=sum.y+value.y
    sum.z=sum.z+value.z
end

function M.readBaseNormals(meshDebug,topology)
    if not meshDebug or not topology or type(topology.refs)~='table' then
        return nil,'missing_topology'
    end
    local result={}
    for index,ref in ipairs(topology.refs) do
        local vertex=meshDebug:getVertex(1,ref.subset,ref.vertex)
        if type(vertex)~='table' then return nil,'missing_vertex' end
        local normal={x=finite(vertex.nx),y=finite(vertex.ny),z=finite(vertex.nz)}
        if length(normal)<=1e-8 then return nil,'missing_normals' end
        result[index]=normalized(normal)
    end
    return result
end

function M.recomputeDeltas(topology,positions,baseNormals)
    if not topology or type(topology.vertices)~='table' or type(topology.triangles)~='table' or
            type(positions)~='table' or type(baseNormals)~='table' then
        return nil,'missing_normal_input'
    end
    local sums={}
    for index=1,#topology.vertices do sums[index]={x=0,y=0,z=0} end
    for _,triangle in ipairs(topology.triangles) do
        local a,b,c=topology.vertices[triangle[1]],topology.vertices[triangle[2]],topology.vertices[triangle[3]]
        if a and b and c and positions[triangle[1]] and positions[triangle[2]] and positions[triangle[3]] then
            local face=cross(subtract(positions[triangle[2]],positions[triangle[1]]),
                subtract(positions[triangle[3]],positions[triangle[1]]))
            if length(face)>1e-8 then
                add(sums[triangle[1]],face)
                add(sums[triangle[2]],face)
                add(sums[triangle[3]],face)
            end
        end
    end
    local result={}
    for index=1,#topology.vertices do
        local base=normalized(baseNormals[index])
        local deformed=normalized(sums[index],base)
        result[index]={x=deformed.x-base.x,y=deformed.y-base.y,z=deformed.z-base.z}
    end
    return result
end

return M
