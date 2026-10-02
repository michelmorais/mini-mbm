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

package.path='editor/?.lua;'..package.path
local Split=require 'image_mesh_normal_material'
local Asset=require 'image_mesh_asset'
local function surface(asset)
    local vertices,indices=Asset.geometry(asset)
    local triangles={}
    for i=1,#indices,3 do
        local corners={}
        for j=i,i+2 do
            local v=vertices[indices[j]]
            corners[#corners+1]=string.format('%.9g/%.9g/%.9g/%.9g/%.9g',v.x,v.y,v.z,v.u,v.v)
        end
        -- Preserve winding as well as positions/UVs.
        triangles[#triangles+1]=table.concat(corners,';')
    end
    table.sort(triangles)
    return table.concat(triangles,'\n')
end
local function test()
    for _,scale in ipairs{.001,1,1000} do
        local source=meshDebug:new();source:setType('mesh');source:addFrame(3);source:addSubSet(1)
        local vertices,indices={},{}
        local function vertex(x,y,z)
            vertices[#vertices+1]={x=x*scale,y=y*scale,z=z*scale,nx=0,ny=0,nz=1,u=x/100+.5,v=y/100+.5}
            return #vertices
        end
        local function triangle(a,b,c)
            indices[#indices+1]=a;indices[#indices+1]=b;indices[#indices+1]=c
        end
        -- Genuine steep front and flat back must retain their materials.
        triangle(vertex(-10,-10,2),vertex(10,-10,2000),vertex(0,10,2))
        triangle(vertex(-10,-10,-10),vertex(0,10,-10),vertex(10,-10,-10))
        -- A slanted wall fan with a refined relief edge, like a minimal-back
        -- image mesh. Native float32 storage makes its XY projection nonzero.
        local rear=vertex(-49.9492416,-49.7461929,-10)
        local previous
        for i=0,32 do
            local t=i/32
            local current=vertex(-49.9492416+t*99.2893447,-49.7461929-t*.4060898,12+math.sin(i))
            if previous then triangle(rear,previous,current) end
            previous=current
        end
        assert(source:addVertex(1,1,vertices));assert(source:addIndex(1,1,indices))
        assert(source:setTexture(1,1,'#FFFFFFFF'))
        local before=surface(source)
        local result,subsets=Split.split(source,{sideMode='band'})
        assert(result:getTotalSubset(1)==3)
        assert(#result:getIndex(1,1)==3,'Wall triangles leaked into front')
        assert(#result:getIndex(1,2)==3,'Wall triangles leaked into back')
        assert(#result:getIndex(1,3)==96,'Missing wall triangles')
        assert(#subsets==2 and subsets[1]==1 and subsets[2]==3)
        assert(surface(result)==before,'Split changed geometry, winding or UVs')
        assert(result:prepareNormalMap(1,1,'preserve'))
        assert(result:prepareNormalMap(1,3,'generate'))
        assert(surface(result)==before,'Tangents changed geometry')
    end
    print('IMAGE MESH NORMAL MATERIAL PASS')
end
local task
function onInitScene() task=coroutine.create(test) end
function onLoop()
    if coroutine.status(task)=='dead' then mbm.quit();return end
    local ok,err=coroutine.resume(task)
    if not ok then print('IMAGE MESH NORMAL MATERIAL FAIL '..tostring(err));mbm.quit() end
end
