--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
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
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

]]--

local M = {}
local transforms = require 'mesh_debug_transform'
local normals = require 'mesh_debug_normals'
local axes={'x','y','z'}

function M.mul(a,b)
    local c={}
    for i=0,2 do for j=1,3 do
        c[i*3+j]=a[i*3+1]*b[j]+a[i*3+2]*b[3+j]+a[i*3+3]*b[6+j]
    end end
    return c
end
function M.transpose(a) return {a[1],a[4],a[7],a[2],a[5],a[8],a[3],a[6],a[9]} end
function M.scale(x,y,z) return {x,0,0,0,y,0,0,0,z} end
function M.rotation(x,y,z)
    local cx,sx,cy,sy,cz,sz=math.cos(x),math.sin(x),math.cos(y),math.sin(y),math.cos(z),math.sin(z)
    -- Column-vector equivalent of engine MatrixTranslationRotationScale: Rx * Ry * Rz * S.
    return {cy*cz,-cy*sz,sy, cx*sz+sx*sy*cz,cx*cz-sx*sy*sz,-sx*cy,
        sx*sz-cx*sy*cz,sx*cz+cx*sy*sz,cx*cy}
end
function M.angles(r)
    local y=math.asin(math.max(-1,math.min(1,r[3])))
    if math.abs(math.cos(y))>1e-7 then return math.atan(-r[6],r[9]),y,math.atan(-r[2],r[1]) end
    return math.atan(r[8],r[5]),y,0
end
function M.inverse(a)
    local d=a[1]*(a[5]*a[9]-a[6]*a[8])-a[2]*(a[4]*a[9]-a[6]*a[7])+a[3]*(a[4]*a[8]-a[5]*a[7])
    if not (math.abs(d)>1e-12 and math.abs(d)<math.huge) then error('scene3d_singular_transform',0) end
    return {(a[5]*a[9]-a[6]*a[8])/d,(a[3]*a[8]-a[2]*a[9])/d,(a[2]*a[6]-a[3]*a[5])/d,
        (a[6]*a[7]-a[4]*a[9])/d,(a[1]*a[9]-a[3]*a[7])/d,(a[3]*a[4]-a[1]*a[6])/d,
        (a[4]*a[8]-a[5]*a[7])/d,(a[2]*a[7]-a[1]*a[8])/d,(a[1]*a[5]-a[2]*a[4])/d}
end
function M.vector(a,x,y,z)
    return a[1]*x+a[2]*y+a[3]*z,a[4]*x+a[5]*y+a[6]*z,a[7]*x+a[8]*y+a[9]*z
end
function M.key(values)
    local out={}
    for i,v in ipairs(values) do out[i]=string.format('%.6f',math.abs(v)<.0000005 and 0 or v) end
    return table.concat(out,',')
end
function M.dominant(items,field)
    local counts,best={},nil
    local function key(item)
        if field=='r' then return M.key(M.rotation(table.unpack(item.r))) end
        return M.key(item[field])
    end
    for _,item in ipairs(items) do
        local k=key(item);counts[k]=(counts[k] or 0)+1
    end
    -- First occurrence wins a tie, including when another group reached its count first.
    local count=0
    for _,item in ipairs(items) do
        local n=counts[key(item)]
        if n>count then count=n;best=item end
    end
    return best
end
function M.bounds(d)
    local lo,hi={math.huge,math.huge,math.huge},{-math.huge,-math.huge,-math.huge}
    local count=0
    for s=1,d:getTotalSubset(1) do for v=1,d:getTotalVertex(1,s) do
        local p=d:getVertex(1,s,v)
        for axis,key in ipairs(axes) do lo[axis]=math.min(lo[axis],p[key]);hi[axis]=math.max(hi[axis],p[key]) end
        count=count+1
    end end
    if count==0 then error('scene3d_empty_geometry',0) end
    return {width=hi[1]-lo[1],height=hi[2]-lo[2],depth=hi[3]-lo[3],
        cx=(hi[1]+lo[1])/2,cy=(hi[2]+lo[2])/2,cz=(hi[3]+lo[3])/2}
end
function M.resetPhysics(d)
    local b=M.bounds(d)
    d:setPhysics({{type='cube',x=b.cx,y=b.cy,z=b.cz,width=b.width,height=b.height,depth=b.depth}})
end
function M.uniformNormals(d,f,s)
    local count=d:getTotalVertex(f,s)
    if count==0 then return {} end
    local vertices=d:getVertex(f,s,1,count)
    local indices=d:getIndex(f,s)
    if not indices then indices={};for i=1,count do indices[i]=i end end
    local sums={}
    for i=1,#indices-2,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
        local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
        local x,y,z=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
        local length=math.sqrt(x*x+y*y+z*z)
        if length>0 then for j=i,i+2 do
            local id=indices[j];local n=sums[id] or {x=0,y=0,z=0};sums[id]=n
            n.x,n.y,n.z=n.x+x/length,n.y+y/length,n.z+z/length
        end end
    end
    for id,n in pairs(sums) do
        local length=math.sqrt(n.x*n.x+n.y*n.y+n.z*n.z)
        if length>0 then n.x,n.y,n.z=n.x/length,n.y/length,n.z/length else sums[id]=nil end
    end
    return sums
end
function M.normals(d,state,missing)
    if d:getModeDraw()~='TRIANGLES' then error('scene3d_triangles_required',0) end
    if missing then d:addNormals() end
    for f=1,d:getTotalFrame() do for s=1,d:getTotalSubset(f) do
        local geo=normals.geometry(d,f,s,state,M.uniformNormals)
        for v=1,d:getTotalVertex(f,s) do
            local p=d:getVertex(f,s,v)
            local x,y,z=normals.select(p,geo[v],missing and (state.normalMethod or 1)==1 and 2 or state.normalMethod)
            if x then p.nx,p.ny,p.nz=x,y,z;d:setVertex(f,s,v,p) end
        end
    end end
end
function M.bake(d,matrix,scale)
    if scale then
        -- Includes canonical skeleton and articulated translation/pivot handling.
        transforms.apply(d,'scale',{sx=scale[1],sy=scale[2],sz=scale[3]})
    else
        local report=d:getSkeletonBindReport(false)
        if report and report.canonical and (report.boneCount or 0)>0 then error('mesh_debug_skeletal_transform_blocked',0) end
        if d:getTotalArticulatedParts()>0 then error('scene3d_articulated_rotation_blocked',0) end
    end
    local normalMatrix=M.transpose(M.inverse(matrix))
    for f=1,d:getTotalFrame() do for s=1,d:getTotalSubset(f) do for v=1,d:getTotalVertex(f,s) do
        local p=d:getVertex(f,s,v)
        if not scale then p.x,p.y,p.z=M.vector(matrix,p.x,p.y,p.z) end
        if p.nx then
            local x,y,z=M.vector(normalMatrix,p.nx,p.ny,p.nz)
            local n=math.sqrt(x*x+y*y+z*z)
            if n>0 then p.nx,p.ny,p.nz=x/n,y/n,z/n end
        end
        d:setVertex(f,s,v,p)
    end end end
    M.resetPhysics(d)
end
return M
