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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Surface morph-target brush math and cached viewport queries.
]]--

local tTopology=require 'morph_surface_topology'
local M={}

local function finite(value,default)
    local number=tonumber(value)
    if not number or number~=number or number==math.huge or number==-math.huge then return default or 0 end
    return number
end

function M.radius(bounds,fraction)
    if not bounds then return 0 end
    local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
    return math.max(extent*math.max(0,finite(fraction,0.025)),0.0001)
end

function M.buildGeometry(topology,target,weight)
    if not topology or not target then return nil end
    local vertices={}
    local deltas={}
    for _,delta in ipairs(target.deltas or {}) do
        local index=tonumber(delta.index)
        if index then deltas[math.floor(index)]=delta end
    end
    weight=finite(weight,1)
    for index,base in ipairs(topology.vertices or {}) do
        local delta=deltas[index]
        vertices[index]={x=base.x+(delta and finite(delta.x) or 0)*weight,
            y=base.y+(delta and finite(delta.y) or 0)*weight,
            z=base.z+(delta and finite(delta.z) or 0)*weight}
    end
    return {vertices=vertices,triangles=topology.triangles or {},topology=topology}
end

function M.hit(geometry,origin,direction)
    if not geometry then return nil end
    return tTopology.intersectRaySurface(geometry,origin,direction)
end

function M.buildSpatial(geometry,radius)
    if not geometry or not geometry.vertices then return nil end
    local cellSize=math.max(tonumber(radius) or 0,1e-5)
    local cells={}
    local function coordinate(value) return math.floor(value/cellSize) end
    for index,vertex in ipairs(geometry.vertices) do
        local key=coordinate(vertex.x)..':'..coordinate(vertex.y)..':'..coordinate(vertex.z)
        cells[key]=cells[key] or {}
        cells[key][#cells[key]+1]=index
    end
    return {cellSize=cellSize,cells=cells}
end

function M.nearby(geometry,spatial,point,radius,allowed)
    local result={}
    if not geometry or not spatial or not point then return result end
    local cellSize=spatial.cellSize
    local minX,maxX=math.floor((point.x-radius)/cellSize),math.floor((point.x+radius)/cellSize)
    local minY,maxY=math.floor((point.y-radius)/cellSize),math.floor((point.y+radius)/cellSize)
    local minZ,maxZ=math.floor((point.z-radius)/cellSize),math.floor((point.z+radius)/cellSize)
    local seen={}
    for x=minX,maxX do
        for y=minY,maxY do
            for z=minZ,maxZ do
                local cell=spatial.cells[x..':'..y..':'..z]
                for _,index in ipairs(cell or {}) do
                    if not seen[index] and (not allowed or allowed[index]) then
                        seen[index]=true
                        result[#result+1]=index
                    end
                end
            end
        end
    end
    return result
end

function M.allowed(target)
    local result={}
    for _,vertex in ipairs(target and target.vertices or {}) do
        result[vertex.index]=math.max(0,math.min(1,finite(vertex.weight,1)))
    end
    return result
end

return M
