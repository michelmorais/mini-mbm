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

local Height=require 'height_map_source'
local M={}
local function byte(v) return math.floor(math.max(0,math.min(255,v))+.5) end
function M.generate(image,options,limit,tick)
    local o=Height.settings(options)
    tick=tick or function() end
    local map=Height.build(image,o,limit,tick)
    -- Strength 1 means a height range of 1/32 of the shorter image side.
    local amplitude=math.max(1,math.min(map.width,map.height)-1)*o.strength/32
    return M.fromMap(map,o,amplitude,amplitude,tick)
end
-- Signed float maps and independent physical scales are useful to mesh bakers.
function M.fromMap(map,o,amplitudeX,amplitudeY,tick)
    tick=tick or function() end
    local rows,heights={},{}
    local w,h=map.width,map.height
    local left,right={},{}
    for x=1,w do left[x]=Height.coordinate(x-1,w,o.edge);right[x]=Height.coordinate(x+1,w,o.edge) end
    local function unpackRow(y)
        local values={};local row=map.rows[y]
        for x=1,w do values[x]=string.unpack('<f',row,(x-1)*4+1) end
        return values
    end
    local current=unpackRow(1)
    local above=unpackRow(Height.coordinate(0,h,o.edge))
    local first=current
    local sign=o.convention=='-Y' and -1 or 1
    for y=1,h do
        local nextY=Height.coordinate(y+1,h,o.edge)
        local below=nextY==1 and first or (nextY==y and current or unpackRow(nextY))
        local upAlpha=map.alpha[Height.coordinate(y-1,h,o.edge)]
        local downAlpha=map.alpha[nextY]
        local centerAlpha=map.alpha[y]
        local row,heightRow={},{}
        for x=1,w do
            local center=current[x]
            local alpha=centerAlpha:byte(x)
            local nx,ny=0,0
            if alpha>0 then
                local l,r=left[x],right[x]
                local lv=centerAlpha:byte(l)==0 and center or current[l]
                local rv=centerAlpha:byte(r)==0 and center or current[r]
                local uv=upAlpha:byte(x)==0 and center or above[x]
                local dv=downAlpha:byte(x)==0 and center or below[x]
                nx=-(rv-lv)*amplitudeX*.5
                -- Input rows go down; tangent-space +Y points up.
                ny=(dv-uv)*amplitudeY*.5*sign
            end
            local length=math.sqrt(nx*nx+ny*ny+1)
            row[x]=string.char(byte(127.5+127.5*nx/length),byte(127.5+127.5*ny/length),byte(127.5+127.5/length),alpha)
            local v=byte(center*255);heightRow[x]=string.char(v,v,v,alpha)
        end
        rows[y]=table.concat(row);heights[y]=table.concat(heightRow)
        above=current;current=below;tick(.55+.45*y/h)
    end
    return {bytes=table.concat(rows),heightBytes=table.concat(heights),diffuse=table.concat(map.diffuse),width=w,height=h,options=o}
end
-- Cooperative jobs do no work while idle. Each step has a CPU-time budget.
function M.job(fn)
    local job={state='running',progress=0}
    job.thread=coroutine.create(function() return fn(function(p) coroutine.yield(p) end) end)
    function job:cancel()
        if self.thread then coroutine.close(self.thread);self.thread=nil end
        self.state='cancelled';self.result=nil
    end
    function job:step(seconds)
        if self.state~='running' then return end
        local deadline=os.clock()+(seconds or .006)
        repeat
            local ok,value=coroutine.resume(self.thread)
            if not ok then self.state='failed';self.error=value;self.thread=nil;return end
            if coroutine.status(self.thread)=='dead' then
                self.state='completed';self.result=value;self.progress=1;self.thread=nil;return
            end
            self.progress=value
        until os.clock()>=deadline
    end
    return job
end
function M.start(image,options,limit)
    local snapshot=Height.settings(options)
    return M.job(function(tick) return M.generate(image,snapshot,limit,tick) end)
end
return M
