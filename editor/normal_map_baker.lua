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

-- Project a physical height-field normal into the actual interpolated render basis.
-- Pure Lua: corners follow expanded triangle draw order, including UV seams.
local Height=require 'height_map_source'
local M={}
local function unit(x,y,z)
    local length=math.sqrt(x*x+y*y+z*z)
    if length<1e-8 then return 0,0,1 end
    return x/length,y/length,z/length
end
local function byte(v) return math.floor(math.max(0,math.min(255,127.5+127.5*v))+.5) end
-- Mirrors the renderer: normalize N, orthogonalize T, then B = cross(N,T)*sign.
function M.project(a,b,c,ta,tb,tc,u,v,dx,dy,dz,strength,green)
    local w=1-u-v
    local nx,ny,nz=unit(a.nx*w+b.nx*u+c.nx*v,a.ny*w+b.ny*u+c.ny*v,a.nz*w+b.nz*u+c.nz*v)
    local tx,ty,tz=ta.x*w+tb.x*u+tc.x*v,ta.y*w+tb.y*u+tc.y*v,ta.z*w+tb.z*u+tc.z*v
    local sign=ta.sign*w+tb.sign*u+tc.sign*v
    local dot=nx*tx+ny*ty+nz*tz
    tx,ty,tz=tx-nx*dot,ty-ny*dot,tz-nz*dot
    if math.abs(sign)<.5 or tx*tx+ty*ty+tz*tz<1e-8 then return 0,0,1 end
    tx,ty,tz=unit(tx,ty,tz);sign=sign<0 and -1 or 1
    local bx,by,bz=(ny*tz-nz*ty)*sign,(nz*tx-nx*tz)*sign,(nx*ty-ny*tx)*sign
    dx,dy,dz=unit(nx+(dx-nx)*strength,ny+(dy-ny)*strength,nz+(dz-nz)*strength)
    return tx*dx+ty*dy+tz*dz,(bx*dx+by*dy+bz*dz)*green,nx*dx+ny*dy+nz*dz
end
function M.generate(image,vertices,indices,corners,domain,options,tick)
    tick=tick or function() end
    local o=Height.settings(options)
    assert(#corners==#indices,'One tangent is required per triangle corner')
    local map=Height.build(image,o,nil,function(p) tick(p*.2) end)
    local w,h=map.width,map.height
    assert(domain.width>0 and domain.height>0 and domain.scale>=0,'Invalid residual dimensions')
    assert(#indices%3==0,'Residual indices must contain triangles')
    -- Mesh UVs come from float32. Recover pixel centers near integer boundaries
    -- instead of losing a full outer row to a few ulps of atlas conversion.
    local function coordinate(uv,size,origin)
        local value=uv*size-origin+.5
        local rounded=math.floor(value+.5)
        if math.abs(value-rounded)<=size*1e-7 then return rounded end
        return value
    end
    local points={}
    for i,v in ipairs(vertices) do
        points[i]={x=coordinate(v.u,domain.imageWidth,domain.x),y=coordinate(v.v,domain.imageHeight,domain.y),z=v.z,vertex=v}
        if i%1024==0 then tick(.11) end
    end
    local starts,ends={},{}
    for i=1,#indices,3 do
        local a,b,c=points[indices[i]],points[indices[i+1]],points[indices[i+2]]
        assert(a and b and c,'Invalid residual triangle')
        local det=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x)
        if math.abs(det)>1e-12 then
            local first=math.max(1,math.ceil(math.min(a.y,b.y,c.y)-1e-6))
            local last=math.min(h,math.floor(math.max(a.y,b.y,c.y)+1e-6))
            if first<=last then
                local t={a=a,b=b,c=c,det=det,index=i,ta=corners[i],tb=corners[i+1],tc=corners[i+2],
                    left=math.max(1,math.ceil(math.min(a.x,b.x,c.x)-1e-6)),
                    right=math.min(w,math.floor(math.max(a.x,b.x,c.x)+1e-6))}
                starts[first]=starts[first] or {};table.insert(starts[first],t)
                ends[last+1]=ends[last+1] or {};table.insert(ends[last+1],t)
            end
        end
        if i%1536==1 then tick(.12) end
    end
    local active,rows={},{}
    local green=o.convention=='-Y' and -1 or 1
    local function slope(x,y,axis)
        local count=axis=='x' and w or h
        local center=axis=='x' and x or y
        local l,r=Height.coordinate(center-1,count,o.edge),Height.coordinate(center+1,count,o.edge)
        local function alpha(i) return axis=='x' and map.alpha[y]:byte(i) or map.alpha[i]:byte(x) end
        local distance=o.edge=='repeat' and 2 or r-l
        if alpha(l)==0 then l=center;distance=distance-1 end
        if alpha(r)==0 then r=center;distance=distance-1 end
        if distance<=0 then return 0 end
        if axis=='x' then return (Height.value(map,r,y)-Height.value(map,l,y))/distance end
        return (Height.value(map,x,r)-Height.value(map,x,l))/distance
    end
    for y=1,h do
        for _,t in ipairs(ends[y] or {}) do active[t]=nil end
        for _,t in ipairs(starts[y] or {}) do active[t]=true end
        local surface={}
        local work=0
        for t in pairs(active) do
            local a,b,c=t.a,t.b,t.c
            for x=t.left,t.right do
                local u=((x-a.x)*(c.y-a.y)-(y-a.y)*(c.x-a.x))/t.det
                local v=((b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x))/t.det
                if u>=-1e-6 and v>=-1e-6 and u+v<=1.000001 then
                    local z=a.z+u*(b.z-a.z)+v*(c.z-a.z)
                    -- Topmost surface wins at overlapping/discontinuous UV edges.
                    local old=surface[x]
                    if not old or z>old.z+1e-7 or (math.abs(z-old.z)<=1e-7 and t.index<old.triangle.index) then
                        surface[x]={z=z,triangle=t,u=u,v=v}
                    end
                end
                work=work+1
                if work%4096==0 then tick(.12+.88*(y-1)/h) end
            end
        end
        local row={}
        for x=1,w do
            local sample=surface[x]
            local alpha=sample and map.alpha[y]:byte(x) or 0
            local mx,my,mz=0,0,1
            if alpha>0 then
                local dx=slope(x,y,'x')*math.max(1,w-1)*domain.scale/domain.width
                local dy=slope(x,y,'y')*math.max(1,h-1)*domain.scale/domain.height
                local nx,ny,nz=unit(dx,dy,1)
                local t=sample.triangle
                mx,my,mz=M.project(t.a.vertex,t.b.vertex,t.c.vertex,t.ta,t.tb,t.tc,
                    sample.u,sample.v,nx,ny,nz,o.strength,green)
            end
            row[x]=string.char(byte(mx),byte(my),byte(mz),alpha)
            if x%1024==0 then tick(.12+.88*(y-1)/h) end
        end
        rows[y]=table.concat(row)
        tick(.12+.88*y/h)
    end
    return {bytes=table.concat(rows),width=w,height=h,options=o}
end
return M
