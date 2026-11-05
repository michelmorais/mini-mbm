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
    local triangles={}
    for _,indices in ipairs(topology.triangles or {}) do
        local a,b,c=vertices[indices[1]],vertices[indices[2]],vertices[indices[3]]
        if a and b and c then
            triangles[#triangles+1]={a=a,b=b,c=c,indices=indices,
                minX=math.min(a.x,b.x,c.x),maxX=math.max(a.x,b.x,c.x),
                minY=math.min(a.y,b.y,c.y),maxY=math.max(a.y,b.y,c.y),
                minZ=math.min(a.z,b.z,c.z),maxZ=math.max(a.z,b.z,c.z),
                cx=(a.x+b.x+c.x)/3,cy=(a.y+b.y+c.y)/3,cz=(a.z+b.z+c.z)/3}
        end
    end
    local function buildBvh(entries)
        if #entries==0 then return nil end
        local node={minX=math.huge,minY=math.huge,minZ=math.huge,
            maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge}
        for _,triangle in ipairs(entries) do
            node.minX=math.min(node.minX,triangle.minX); node.maxX=math.max(node.maxX,triangle.maxX)
            node.minY=math.min(node.minY,triangle.minY); node.maxY=math.max(node.maxY,triangle.maxY)
            node.minZ=math.min(node.minZ,triangle.minZ); node.maxZ=math.max(node.maxZ,triangle.maxZ)
        end
        if #entries<=12 then node.triangles=entries; return node end
        local dx,dy,dz=node.maxX-node.minX,node.maxY-node.minY,node.maxZ-node.minZ
        local axis=dx>=dy and dx>=dz and 'cx' or (dy>=dz and 'cy' or 'cz')
        table.sort(entries,function(left,right) return left[axis]<right[axis] end)
        local middle=math.floor(#entries/2)
        local left,right={},{}
        for index,triangle in ipairs(entries) do
            if index<=middle then left[#left+1]=triangle else right[#right+1]=triangle end
        end
        node.left=buildBvh(left); node.right=buildBvh(right)
        return node
    end
    return {vertices=vertices,triangles=topology.triangles or {},rayTriangles=triangles,
        bvh=buildBvh(triangles),topology=topology}
end

function M.hit(geometry,origin,direction)
    if not geometry or not origin or not direction then return nil end
    local bvh=geometry.bvh
    if not bvh then return tTopology.intersectRaySurface(geometry,origin,direction) end
    local directionLength=math.sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z)
    if directionLength<=1e-8 then return nil end
    local ray={x=direction.x/directionLength,y=direction.y/directionLength,
        z=direction.z/directionLength}
    local function boundsHit(node,maxDistance)
        local near,far=0,maxDistance or math.huge
        for _,axis in ipairs({{'x',origin.x,ray.x},{'y',origin.y,ray.y},{'z',origin.z,ray.z}}) do
            local name,value,delta=axis[1],axis[2],axis[3]
            local minimum=node['min'..name:upper()]
            local maximum=node['max'..name:upper()]
            if math.abs(delta)<1e-12 then
                if value<minimum or value>maximum then return false end
            else
                local a,b=(minimum-value)/delta,(maximum-value)/delta
                if a>b then a,b=b,a end
                near,far=math.max(near,a),math.min(far,b)
                if near>far then return false end
            end
        end
        return true
    end
    local function triangleHit(triangle)
        local a,b,c=triangle.a,triangle.b,triangle.c
        local e1x,e1y,e1z=b.x-a.x,b.y-a.y,b.z-a.z
        local e2x,e2y,e2z=c.x-a.x,c.y-a.y,c.z-a.z
        local px,py,pz=ray.y*e2z-ray.z*e2y,ray.z*e2x-ray.x*e2z,ray.x*e2y-ray.y*e2x
        local determinant=e1x*px+e1y*py+e1z*pz
        if math.abs(determinant)<1e-9 then return nil end
        local inverse=1/determinant
        local tx,ty,tz=origin.x-a.x,origin.y-a.y,origin.z-a.z
        local u=(tx*px+ty*py+tz*pz)*inverse
        if u<0 or u>1 then return nil end
        local qx,qy,qz=ty*e1z-tz*e1y,tz*e1x-tx*e1z,tx*e1y-ty*e1x
        local v=(ray.x*qx+ray.y*qy+ray.z*qz)*inverse
        if v<0 or u+v>1 then return nil end
        local distance=(e2x*qx+e2y*qy+e2z*qz)*inverse
        if distance<=1e-7 then return nil end
        local nx=e1y*e2z-e1z*e2y
        local ny=e1z*e2x-e1x*e2z
        local nz=e1x*e2y-e1y*e2x
        local length=math.sqrt(nx*nx+ny*ny+nz*nz)
        if length<1e-12 then return nil end
        nx,ny,nz=nx/length,ny/length,nz/length
        if nx*ray.x+ny*ray.y+nz*ray.z>0 then nx,ny,nz=-nx,-ny,-nz end
        return {distance=distance,point={x=origin.x+ray.x*distance,
            y=origin.y+ray.y*distance,z=origin.z+ray.z*distance},
            normal={x=nx,y=ny,z=nz},triangle=triangle.indices}
    end
    local nearest=nil
    local function visit(node)
        if not node or not boundsHit(node,nearest and nearest.distance or math.huge) then return end
        if node.triangles then
            for _,triangle in ipairs(node.triangles) do
                local hit=triangleHit(triangle)
                if hit and (not nearest or hit.distance<nearest.distance) then nearest=hit end
            end
            return
        end
        visit(node.left); visit(node.right)
    end
    visit(bvh)
    return nearest
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
