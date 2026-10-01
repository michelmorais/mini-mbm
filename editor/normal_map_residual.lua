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

-- Bake detail left after a height-field mesh has represented part of the relief.
-- Geometry is rasterized in the source UV domain; signed residuals stay floats.
local Height=require 'height_map_source'
local Generator=require 'normal_map_generator'
local M={}
function M.generate(image,vertices,indices,domain,options,tick)
    tick=tick or function() end
    local o=Height.settings(options)
    local input=Height.settings(o);input.blur=0
    local map=Height.build(image,input,nil,function(p) tick(p*.2) end)
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
        points[i]={x=coordinate(v.u,domain.imageWidth,domain.x),y=coordinate(v.v,domain.imageHeight,domain.y),z=v.z}
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
                local t={a=a,b=b,c=c,det=det,
                    left=math.max(1,math.ceil(math.min(a.x,b.x,c.x)-1e-6)),
                    right=math.min(w,math.floor(math.max(a.x,b.x,c.x)+1e-6))}
                starts[first]=starts[first] or {};table.insert(starts[first],t)
                ends[last+1]=ends[last+1] or {};table.insert(ends[last+1],t)
            end
        end
        if i%1536==1 then tick(.12) end
    end
    local active={}
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
                    surface[x]=math.max(surface[x] or -math.huge,z)
                end
                work=work+1
                if work%4096==0 then tick(.12+.43*(y-1)/h) end
            end
        end
        local row,alpha={},{}
        for x=1,w do
            local covered=surface[x]~=nil
            local residual=covered and (Height.value(map,x,y)*domain.scale-surface[x]) or 0
            row[x]=string.pack('<f',residual)
            alpha[x]=string.char(covered and map.alpha[y]:byte(x) or 0)
        end
        map.rows[y]=table.concat(row);map.alpha[y]=table.concat(alpha)
        tick(.12+.43*y/h)
    end
    -- Blur the difference, not the source: a matching surface stays neutral.
    Height.blur(map,math.floor(o.blur+.5),o.edge,function(p) tick(.55+p*.3) end)
    return Generator.fromMap(map,o,math.max(1,w-1)/domain.width*o.strength,
        math.max(1,h-1)/domain.height*o.strength,function(p) tick(.72+.28*p) end)
end
return M
