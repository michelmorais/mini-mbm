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

-- Fixed XY samples of the front (+Z) surface, evaluated only during generation.
local Asset=require 'image_mesh_asset'
local M={}
function M.sample(asset,reference)
    local vertices,indices=Asset.geometry(asset)
    local grid=reference
    if not grid then
        local x0,y0,x1,y1=math.huge,math.huge,-math.huge,-math.huge
        for _,v in ipairs(vertices) do
            x0=math.min(x0,v.x);x1=math.max(x1,v.x);y0=math.min(y0,v.y);y1=math.max(y1,v.y)
        end
        grid={x=x0,y=y0,dx=(x1-x0)/128,dy=(y1-y0)/128,size=128}
    end
    local values={}
    local n=grid.size
    if grid.dx<=0 or grid.dy<=0 then return {values=values,grid=grid} end
    for i=1,#indices,3 do
        local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
        local det=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y)
        if det>1e-12 then
            local x0=math.max(0,math.ceil((math.min(a.x,b.x,c.x)-grid.x)/grid.dx-.5))
            local x1=math.min(n-1,math.floor((math.max(a.x,b.x,c.x)-grid.x)/grid.dx-.5))
            local y0=math.max(0,math.ceil((math.min(a.y,b.y,c.y)-grid.y)/grid.dy-.5))
            local y1=math.min(n-1,math.floor((math.max(a.y,b.y,c.y)-grid.y)/grid.dy-.5))
            for y=y0,y1 do for x=x0,x1 do
                local px,py=grid.x+(x+.5)*grid.dx,grid.y+(y+.5)*grid.dy
                local u=((b.y-c.y)*(px-c.x)+(c.x-b.x)*(py-c.y))/det
                local v=((c.y-a.y)*(px-c.x)+(a.x-c.x)*(py-c.y))/det
                if u>=-1e-7 and v>=-1e-7 and u+v<=1.0000001 then
                    local z=u*a.z+v*b.z+(1-u-v)*c.z
                    local key=y*n+x+1
                    if not values[key] or z>values[key] then values[key]=z end
                end
            end end
        end
    end
    return {values=values,grid=grid}
end
function M.compare(reference,candidate)
    local ref,got=reference.values,candidate.values
    local n=reference.grid.size
    local lo,hi,clo,chi=math.huge,-math.huge,math.huge,-math.huge
    local square,maxError,count,missing,slope,change=0,0,0,0,0,0
    for i,z in pairs(ref) do
        lo=math.min(lo,z);hi=math.max(hi,z)
        local value=got[i]
        if not value then missing=missing+1 else
            clo=math.min(clo,value);chi=math.max(chi,value)
            local e=math.abs(value-z);square=square+e*e;maxError=math.max(maxError,e);count=count+1
            for direction=1,2 do
                local j=i+n
                if direction==1 then j=(i-1)%n<n-1 and i+1 or 0 end
                if ref[j] and got[j] then
                    slope=slope+math.abs(ref[j]-z)
                    change=change+math.abs((got[j]-value)-(ref[j]-z))
                end
            end
        end
    end
    local scale=math.max(hi-lo,math.max(reference.grid.dx,reference.grid.dy)*1e-4,1e-6)
    local result={rms=math.sqrt(square/math.max(1,count))/scale,max=maxError/scale,
        slope=change/math.max(slope,scale),depth=math.max(0,chi-clo)/scale,missing=missing,range=hi-lo}
    result.accepted=missing==0 and count>0 and result.rms<=.1 and result.slope<=.75 and (hi-lo<1e-5 or result.depth>=.75)
    result.score=result.rms+result.slope*.1
    return result
end
return M
