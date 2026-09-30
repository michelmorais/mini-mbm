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
-- CPU reference preview: independent from backend shader conventions. Relighting
-- consumes the existing normal raster; it never rebuilds height or normal maps.
function M.light(result,azimuth,elevation,enabled,tick)
    tick=tick or function() end
    local a,e=math.rad(azimuth),math.rad(elevation)
    local lx,ly,lz=math.cos(a)*math.cos(e),math.sin(a)*math.cos(e),math.sin(e)
    local rows={}
    for y=1,result.height do
        local row={}
        for x=1,result.width do
            local p=((y-1)*result.width+x-1)*4+1
            local r,g,b,alpha=result.bytes:byte(p,p+3)
            local nx,ny,nz=r/127.5-1,g/127.5-1,b/127.5-1
            if result.options.convention=='-Y' then ny=-ny end
            if not enabled then nx,ny,nz=0,0,1 end
            local length=math.sqrt(nx*nx+ny*ny+nz*nz)
            local light=.18+.82*math.max(0,(nx*lx+ny*ly+nz*lz)/math.max(1e-6,length))
            local dr,dg,db=result.diffuse:byte(p,p+2)
            row[x]=string.char(math.floor(dr*light+.5),math.floor(dg*light+.5),math.floor(db*light+.5),alpha)
        end
        rows[y]=table.concat(row);tick(y/result.height)
    end
    return table.concat(rows)
end
-- Four stable cache slots per editor; reload replaces their GPU storage.
function M.new(temporaryPath)
    local p={slots={}}
    function p:set(name,bytes,w,h)
        local slot=self.slots[name]
        if not slot then slot={path=temporaryPath('.png')};self.slots[name]=slot end
        assert(mbm.writeImagePixels(slot.path,bytes,w,h))
        if slot.info then assert(slot.info:reload(slot.path))
        else slot.info=assert(mbm.loadTexture(slot.path)) end
        return slot.info
    end
    function p:clear()
        for _,slot in pairs(self.slots) do
            if slot.info then slot.info:release() end
            os.remove(slot.path)
        end
        -- Retain slot names/handles, so opening another source does not grow the cache.
    end
    return p
end
return M
