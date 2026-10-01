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

local root='/tmp/imesh-frequency-smoke'
local function vertices(asset) return asset:getVertex(1,1,1,asset:getTotalVertex(1,1)) end
local function test()
    assert(mbm.createDirectories(root));mbm.addPath(root)
    local n=9
    local pixels={}
    for y=0,n-1 do for x=0,n-1 do
        local value=64+x*16+(y%2)*48
        pixels[#pixels+1]=string.char(value,value,value,255)
    end end
    local source=root..'/source.png'
    assert(mbm.writeImagePixels(source,table.concat(pixels),n,n))
    local o={width=8,height=8,depth=2,relief=8,columns=8,rows=8,lockBorder=false,
        separateFront=true,backOpen=true,geometryBlurRadius=0}
    local raw=assert(mbm.generateImageMesh(source,o))
    assert(mbm.generateImageMeshMap(source,o,root..'/full.png',false))
    o.geometryBlurRadius=2
    local filtered=assert(mbm.generateImageMesh(source,o))
    assert(mbm.generateImageMeshMap(source,o,root..'/still-full.png',false))
    assert(mbm.readImagePixels(root..'/full.png')==mbm.readImagePixels(root..'/still-full.png'),'Geometry filter changed normal-map height source')
    local different=false
    local a,b=vertices(raw),vertices(filtered)
    assert(#a==#b,'Fixed grid changed topology')
    for i,v in ipairs(b) do
        local x,y=math.floor(v.u*n),math.floor(v.v*n)
        local sum=0
        for yy=y-2,y+2 do for xx=x-2,x+2 do
            local sx,sy=math.max(0,math.min(n-1,xx)),math.max(0,math.min(n-1,yy))
            sum=sum+64+sx*16+(sy%2)*48
        end end
        local actual=(-v.z-1)/8
        assert(math.abs(actual-sum/(25*255))<1e-5,'Filtered geometry differs from box reference')
        assert(v.x==a[i].x and v.y==a[i].y and v.u==a[i].u and v.v==a[i].v,'Filter changed XY/UV')
        different=different or math.abs(v.z-a[i].z)>.001
    end
    assert(different,'Geometry filter was ignored')
    o.lockBorder=true
    local locked=assert(mbm.generateImageMesh(source,o))
    for _,v in ipairs(vertices(locked)) do
        if math.abs(v.x)==4 or math.abs(v.y)==4 then assert(math.abs(v.z+1)<1e-5,'Locked border moved') end
    end
    -- Hidden RGB must not bleed into opaque samples.
    pixels={}
    for y=0,n-1 do for x=0,n-1 do
        local hidden=x>=3 and x<=5 and y>=3 and y<=5
        pixels[#pixels+1]=hidden and string.char(255,255,255,0) or string.char(80,80,80,255)
    end end
    local alpha=root..'/alpha.png';assert(mbm.writeImagePixels(alpha,table.concat(pixels),n,n))
    o.lockBorder=false
    local masked=assert(mbm.generateImageMesh(alpha,o))
    for _,v in ipairs(vertices(masked)) do assert(math.abs((-v.z-1)/8-80/255)<1e-5,'Hidden RGB leaked into geometry') end
    -- Finishing is filtered too; a constant manual field remains constant.
    o.heightSource='manual';o.baseHeight=.4
    local manual=assert(mbm.generateImageMesh(source,o))
    for _,v in ipairs(vertices(manual)) do assert(math.abs((-v.z-1)/8-.4)<1e-5) end
    o.heightAreas={{mode='raise',height=.4,{x=.375,y=.375},{x=.625,y=.375},{x=.625,y=.625},{x=.375,y=.625}}}
    local painted=assert(mbm.generateImageMesh(source,o))
    local hi=0
    for _,v in ipairs(vertices(painted)) do hi=math.max(hi,(-v.z-1)/8) end
    assert(hi>.4 and hi<.8,'Finishing was discarded or not filtered')
    o.heightAreas=nil;o.heightSource='image'
    o.holes={{{x=.3,y=.3},{x=.7,y=.3},{x=.7,y=.7},{x=.3,y=.7}}}
    pixels={}
    for y=0,n-1 do for x=0,n-1 do
        local value=(x>=3 and x<=5 and y>=3 and y<=5) and 255 or 80
        pixels[#pixels+1]=string.char(value,value,value,255)
    end end
    local holeSource=root..'/hole.png';assert(mbm.writeImagePixels(holeSource,table.concat(pixels),n,n))
    local holed=assert(mbm.generateImageMesh(holeSource,o))
    for _,v in ipairs(vertices(holed)) do assert(math.abs((-v.z-1)/8-80/255)<1e-5,'Hole pixels leaked into geometry') end
    o.holes=nil
    o.followImage=true;o.columns=4;o.rows=4;o.heightTolerance=.05;o.geometryBlurRadius=0
    local detailed,detailedReport=mbm.generateImageMesh(source,o);assert(detailed,detailedReport)
    o.geometryBlurRadius=3
    local coarse,coarseReport=mbm.generateImageMesh(source,o);assert(coarse,coarseReport)
    assert(coarseReport.triangles<detailedReport.triangles,'Filtered field did not reduce adaptive detail')
    print('FREQUENCY TRIANGLES',detailedReport.triangles,coarseReport.triangles)
    for _,case in ipairs{{geometryBlurRadius=33},{geometryBlurRadius=2,voxelized=true},
        {geometryBlurRadius=2,twoLevels=true},{geometryBlurRadius=2,heightSource='curved',backOpen=false}} do
        local options={};for k,v in pairs(o) do options[k]=v end
        for k,v in pairs(case) do options[k]=v end
        local result,message=mbm.generateImageMesh(source,options)
        assert(not result and type(message)=='string','Unsupported separation silently accepted')
    end
    print('IMAGE MESH FREQUENCY PASS')
end
function onInitScene()
    local ok,err=pcall(test)
    if not ok then print('IMAGE MESH FREQUENCY FAIL '..tostring(err)) end
    mbm.quit()
end
function onLoop() mbm.quit() end
