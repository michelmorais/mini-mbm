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

   Facial polygon compatibility layer over generic surface polygon geometry.
]]--

local tSurfacePolygon=require 'morph_surface_polygon'
local M={}
for name,operation in pairs(tSurfacePolygon) do M[name]=operation end

local function subtract(a,b) return {x=a.x-b.x,y=a.y-b.y,z=a.z-b.z} end
local function cross(a,b)
    return {x=a.y*b.z-a.z*b.y,y=a.z*b.x-a.x*b.z,z=a.x*b.y-a.y*b.x}
end
local function dot(a,b) return a.x*b.x+a.y*b.y+a.z*b.z end

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
    local normalLength=math.sqrt(dot(normal,normal))
    if normalLength<=1e-8 then return {x=point.x,y=point.y,z=point.z} end
    normal={x=normal.x/normalLength,y=normal.y/normalLength,z=normal.z/normalLength}
    return {x=point.x+normal.x*amount,y=point.y+normal.y*amount,z=point.z+normal.z*amount}
end

function M.positionWithFaceDepth(position,depth)
    if not position then return nil end
    return {x=position.x,y=position.y,z=position.z+(tonumber(depth) or 0)}
end

return M
