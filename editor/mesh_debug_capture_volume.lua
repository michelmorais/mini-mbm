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

-- Unit-space tests keep nonuniform Size consistent with the displayed volume.
local M = {}
-- Keep back-face culling enabled and provide the opposite winding explicitly.
-- FRONT_AND_BACK has different meanings across the engine's render backends.
function M.doubleSided(vertices)
    local count = #vertices
    for i = 1, count, 9 do
        for _, offset in ipairs({0, 6, 3}) do
            for component = 0, 2 do
                vertices[#vertices + 1] = vertices[i + offset + component]
            end
        end
    end
    return vertices
end
local function point(p, box)
    local lo, hi = box.aabbMin, box.aabbMax
    return {x=(2*p.x-lo.x-hi.x)/(hi.x-lo.x),
            y=(2*p.y-lo.y-hi.y)/(hi.y-lo.y),
            z=(2*p.z-lo.z-hi.z)/(hi.z-lo.z)}
end
local function dot(a,b) return a.x*b.x+a.y*b.y+a.z*b.z end
local function sub(a,b) return {x=a.x-b.x,y=a.y-b.y,z=a.z-b.z} end
local function cross(a,b)
    return {x=a.y*b.z-a.z*b.y,y=a.z*b.x-a.x*b.z,z=a.x*b.y-a.y*b.x}
end
local function segmentDistance(a,b)
    local d=sub(b,a)
    local length=dot(d,d)
    local t=length>0 and math.max(0,math.min(1,-dot(a,d)/length)) or 0
    local p={x=a.x+t*d.x,y=a.y+t*d.y,z=a.z+t*d.z}
    return dot(p,p)
end
local function triangleDistance(a,b,c)
    local ab,ac=sub(b,a),sub(c,a)
    local n=cross(ab,ac)
    local nn=dot(n,n)
    if nn>1e-24 then
        local d=dot(n,a)/nn
        local q={x=n.x*d,y=n.y*d,z=n.z*d}
        if dot(cross(ab,sub(q,a)),n)>=-1e-12 and
           dot(cross(sub(c,b),sub(q,b)),n)>=-1e-12 and
           dot(cross(sub(a,c),sub(q,c)),n)>=-1e-12 then
            return dot(q,q)
        end
    end
    return math.min(segmentDistance(a,b),segmentDistance(b,c),segmentDistance(c,a))
end
function M.contains(p,box)
    p=point(p,box)
    if box.volumeType=='sphere' then return dot(p,p)<=1+1e-12 end
    return math.abs(p.z)<=1+1e-12 and p.x*p.x+p.y*p.y<=1+1e-12
end
local function clip(poly,z,sign)
    local out={}
    local a=poly[#poly]
    if not a then return out end
    for _,b in ipairs(poly) do
        local ai,bi=sign*(a.z-z)<=0,sign*(b.z-z)<=0
        if ai~=bi then
            local t=(z-a.z)/(b.z-a.z)
            out[#out+1]={x=a.x+t*(b.x-a.x),y=a.y+t*(b.y-a.y),z=z}
        end
        if bi then out[#out+1]=b end
        a=b
    end
    return out
end
function M.intersects(a,b,c,box)
    a,b,c=point(a,box),point(b,box),point(c,box)
    if box.volumeType=='sphere' then return triangleDistance(a,b,c)<=1+1e-12 end
    -- Clip against the cylinder caps, then test the projected polygon against
    -- the unit disk. This also catches faces crossing the volume with no vertex inside.
    local poly=clip(clip({a,b,c},1,1),-1,-1)
    for _,p in ipairs(poly) do p.z=0 end
    if #poly==0 then return false end
    if #poly==1 then return dot(poly[1],poly[1])<=1+1e-12 end
    if #poly==2 then return segmentDistance(poly[1],poly[2])<=1+1e-12 end
    for i=2,#poly-1 do
        if triangleDistance(poly[1],poly[i],poly[i+1])<=1+1e-12 then return true end
    end
    return false
end
function M.geometry(kind,hw,hh,hd)
    local vertices,lines={},{}
    local function p(angle,z,r)
        return {x=hw*r*math.cos(angle),y=hh*r*math.sin(angle),z=hd*z}
    end
    local function append(out,v)
        out[#out+1]=v.x; out[#out+1]=v.y; out[#out+1]=v.z
    end
    local function tri(a,b,c) append(vertices,a); append(vertices,b); append(vertices,c) end
    local segments,rings=48,24
    if kind=='cylinder' then
        for i=0,segments-1 do
            local a,b=2*math.pi*i/segments,2*math.pi*(i+1)/segments
            local v1,v2,v3,v4=p(a,-1,1),p(b,-1,1),p(b,1,1),p(a,1,1)
            tri(v1,v2,v3); tri(v1,v3,v4)
            tri(p(0,-1,0),v2,v1); tri(p(0,1,0),v4,v3)
        end
    else
        for j=0,rings-1 do
            local a,b=-math.pi/2+math.pi*j/rings,-math.pi/2+math.pi*(j+1)/rings
            for i=0,segments-1 do
                local u,v=2*math.pi*i/segments,2*math.pi*(i+1)/segments
                local v1,v2=p(u,math.sin(a),math.cos(a)),p(v,math.sin(a),math.cos(a))
                local v3,v4=p(v,math.sin(b),math.cos(b)),p(u,math.sin(b),math.cos(b))
                if j>0 then tri(v1,v2,v3) end
                if j<rings-1 then tri(v1,v3,v4) end
            end
        end
    end
    for _,z in ipairs(kind=='cylinder' and {-1,1} or {-0.75,-0.5,0,0.5,0.75}) do
        local strip={}
        for i=0,segments do append(strip,p(2*math.pi*i/segments,z,kind=='cylinder' and 1 or math.sqrt(1-z*z))) end
        lines[#lines+1]=strip
    end
    for i=0,7 do
        local strip={}
        for j=0,rings do
            local a=-math.pi/2+math.pi*j/rings
            local z,r=math.sin(a),math.cos(a)
            if kind=='cylinder' then z=-1+2*j/rings; r=1 end
            append(strip,p(2*math.pi*i/8,z,r))
        end
        lines[#lines+1]=strip
    end
    return vertices,lines
end
return M
