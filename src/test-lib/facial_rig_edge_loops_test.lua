--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|
]]--

package.path='editor/?.lua;'..package.path
local Loops=require 'facial_rig_edge_loops'
local function point(x,y,z) return {x=x,y=y,z=z} end

local ring={signature='ring',vertices={
    point(-2,-2,0),point(2,-2,0),point(2,2,0),point(-2,2,0),
    point(-0.5,-0.5,0),point(0.5,-0.5,0),point(0.5,0.5,0),point(-0.5,0.5,0)},triangles={}}
for index=1,4 do
    local nextIndex=index%4+1
    ring.triangles[#ring.triangles+1]={index,nextIndex,index+4}
    ring.triangles[#ring.triangles+1]={nextIndex,nextIndex+4,index+4}
end
local cache=Loops.build(ring)
assert(#cache.loops==2)
local inner=assert(Loops.nearest(cache,point(0.5,0,0),0.8))
assert(#inner==4 and inner[1]==5 and inner[4]==8)
assert(not Loops.nearest(cache,point(0,0,3),0.8))

local duplicate={signature='coincident',vertices={},triangles={}}
for _,vertex in ipairs(ring.vertices) do
    duplicate.vertices[#duplicate.vertices+1]=point(vertex.x,vertex.y,vertex.z)
end
for _,vertex in ipairs(ring.vertices) do
    duplicate.vertices[#duplicate.vertices+1]=point(vertex.x,vertex.y,vertex.z)
end
for _,triangle in ipairs(ring.triangles) do
    duplicate.triangles[#duplicate.triangles+1]=triangle
    duplicate.triangles[#duplicate.triangles+1]={triangle[1]+8,triangle[2]+8,triangle[3]+8}
end
local coincident=Loops.build(duplicate)
local lower=assert(Loops.nearest(coincident,point(0.5,0,0),0.8,{10,14,13}))
assert(#lower==4 and lower[1]==13 and lower[4]==16,
    'clicked triangle must disambiguate coincident boundary loops')

local sealed={signature='sealed',vertices={point(0,0,0),point(1,0,0),point(0,1,0),point(0,0,1)},
    triangles={{1,2,3},{1,4,2},{2,4,3},{1,3,4}}}
assert(#Loops.build(sealed).loops==0)
assert(not Loops.nearest(Loops.build(sealed),point(0,0,0),1))

local branch={signature='branch',vertices={point(0,0,0),point(1,0,0),point(0,1,0),
    point(-1,0,0),point(0,-1,0)},triangles={{1,2,3},{1,4,5}}}
local found,reason=Loops.nearest(Loops.build(branch),point(0,0,0),0.1)
assert(not found and reason=='open_chain')

print('FACIAL RIG EDGE LOOPS OK')
