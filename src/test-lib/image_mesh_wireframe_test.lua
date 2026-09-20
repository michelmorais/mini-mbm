--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]

package.path='editor/?.lua;'..package.path
local Wire=require 'image_mesh_wireframe'
local strips={}
line={new=function() return {
    add=function(_,p) strips[#strips+1]=p; return #strips end,
    setColor=function() end,
    destroy=function() end,
} end}
-- Two triangles sharing duplicated positions, plus a disconnected triangle.
local vertices={}
for _,p in ipairs({{0,0},{1,0},{1,1},{0,0},{1,1},{0,1},{3,0},{4,0},{3,1}}) do
    vertices[#vertices+1]={x=p[1],y=p[2],z=0}
end
local asset={getTotalVertex=function() return #vertices end,
    getVertex=function() return vertices end,
    getIndex=function() return {1,2,3,4,5,6,7,8,9} end}
local state={preview={},wireframe=true,editMode=false,dirty=false}
Wire.ensure(state,asset); Wire.sync(state)
assert(state.wireObject.visible and not state.preview.visible)
local expected={['0,0/1,0']=true,['1,0/1,1']=true,['0,0/1,1']=true,
    ['0,1/1,1']=true,['0,0/0,1']=true,['3,0/4,0']=true,['3,1/4,0']=true,['3,0/3,1']=true}
local seen={}
for _,p in ipairs(strips) do
    for i=1,#p-3,3 do
        local a,b=p[i]..','..p[i+1],p[i+3]..','..p[i+4]
        if a>b then a,b=b,a end
        local key=a..'/'..b
        assert(expected[key],'spurious wire edge: '..key); seen[key]=true
    end
end
for key in pairs(expected) do assert(seen[key],'missing wire edge: '..key) end
local count=#strips; Wire.ensure(state,asset)
assert(#strips==count and state.wireBuilds==1)
Wire.release(state); assert(not state.wireObject)
print('IMAGE MESH WIREFRAME EDGES / DISCONNECTED COMPONENTS / CACHE OK')
