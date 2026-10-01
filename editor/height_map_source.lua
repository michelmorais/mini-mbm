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

local M={}
M.defaults={channel='luminance',black=0,white=1,curve=1,invert=false,blur=0,strength=1,convention='+Y',edge='clamp'}
function M.settings(input)
    input=input or {}
    local o={}
    for k,v in pairs(M.defaults) do if input[k]~=nil then o[k]=input[k] else o[k]=v end end
    assert(({luminance=true,r=true,g=true,b=true,a=true})[o.channel],'Invalid height channel')
    assert(o.convention=='+Y' or o.convention=='-Y','Invalid normal convention')
    assert(o.edge=='clamp' or o.edge=='repeat','Invalid edge mode')
    for k,range in pairs{black={0,1},white={0,1},curve={.1,8},blur={0,32},strength={0,16}} do
        local v=o[k];assert(type(v)=='number' and v==v and v>=range[1] and v<=range[2],'Invalid '..k)
    end
    assert(o.white>o.black,'White must be greater than black')
    assert(type(o.invert)=='boolean','Invalid inversion')
    return o
end
function M.image(bytes,w,h)
    assert(math.type(w)=='integer' and math.type(h)=='integer' and w>0 and h>0 and w<=16384 and h<=16384 and w*h<=16777216,'Invalid dimensions')
    assert(type(bytes)=='string' and #bytes==w*h*4,'Invalid RGBA bytes')
    return {bytes=bytes,width=w,height=h}
end
function M.coordinate(x,n,edge)
    if edge=='repeat' then return (x-1)%n+1 end
    return math.max(1,math.min(n,x))
end
-- Rows of packed floats keep large rasters out of Lua number tables.
function M.value(map,x,y,edge)
    x=M.coordinate(x,map.width,edge);y=M.coordinate(y,map.height,edge)
    return (string.unpack('<f',map.rows[y],(x-1)*4+1))
end
function M.build(image,o,limit,tick)
    assert(limit==nil or (math.type(limit)=='integer' and limit>0),'Invalid preview limit')
    tick=tick or function() end
    local scale=math.min(1,(limit or math.max(image.width,image.height))/math.max(image.width,image.height))
    local w,h=math.max(1,math.floor(image.width*scale)),math.max(1,math.floor(image.height*scale))
    local map={width=w,height=h,rows={},alpha={},diffuse={}}
    local channel=({r=1,g=2,b=3,a=4})[o.channel]
    for y=1,h do
        local row,alpha,diffuse={},{},{}
        local sy=math.min(image.height-1,math.floor((y-.5)*image.height/h))
        for x=1,w do
            local sx=math.min(image.width-1,math.floor((x-.5)*image.width/w))
            local p=(sy*image.width+sx)*4+1
            local r,g,b,a=image.bytes:byte(p,p+3)
            local v=channel and image.bytes:byte(p+channel-1) or (.2126*r+.7152*g+.0722*b)
            v=math.max(0,math.min(1,(v/255-o.black)/(o.white-o.black)))^o.curve
            if o.invert then v=1-v end
            row[x]=string.pack('<f',v);alpha[x]=string.char(a);diffuse[x]=string.char(r,g,b,a)
        end
        map.rows[y]=table.concat(row);map.alpha[y]=table.concat(alpha);map.diffuse[y]=table.concat(diffuse)
        tick(.25*y/h)
    end
    M.blur(map,math.floor(o.blur*scale+.5),o.edge,tick)
    return map
end
-- Also accepts signed physical heights, used by residual normal-map baking.
function M.blur(map,radius,edge,tick)
    local w,h=map.width,map.height
    tick=tick or function() end
    -- Alpha-weighted separable box filter; invisible RGB never bleeds into the height.
    if radius>0 then
        local sums,weights={},{}
        for y=1,h do
            local sr,wr={},{}
            local sum,weight=0,0
            local function sample(x)
                x=M.coordinate(x,w,edge)
                local a=map.alpha[y]:byte(x)/255
                return M.value(map,x,y,edge)*a,a
            end
            for x=1-radius,1+radius do local v,a=sample(x);sum=sum+v;weight=weight+a end
            for x=1,w do
                sr[x]=string.pack('<f',sum);wr[x]=string.pack('<f',weight)
                local v,a=sample(x-radius);sum=sum-v;weight=weight-a
                v,a=sample(x+radius+1);sum=sum+v;weight=weight+a
            end
            sums[y]=table.concat(sr);weights[y]=table.concat(wr);tick(.25+.15*y/h)
        end
        local totals,counts={},{}
        local function accumulate(y,sign)
            y=M.coordinate(y,h,edge)
            for x=1,w do
                totals[x]=(totals[x] or 0)+sign*string.unpack('<f',sums[y],(x-1)*4+1)
                counts[x]=(counts[x] or 0)+sign*string.unpack('<f',weights[y],(x-1)*4+1)
            end
        end
        for y=1-radius,1+radius do accumulate(y,1) end
        for y=1,h do
            local row={}
            for x=1,w do
                local v=counts[x]>1e-6 and totals[x]/counts[x] or M.value(map,x,y,edge)
                row[x]=string.pack('<f',v)
            end
            map.rows[y]=table.concat(row)
            accumulate(y-radius,-1);accumulate(y+radius+1,1);tick(.4+.15*y/h)
        end
    end
    return map
end
return M
