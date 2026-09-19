--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

package.path = 'editor/?.lua;' .. package.path
local Islands = require 'mesh_debug_islands'
local function vertex(x,y,z) return {x=x,y=y,z=z or 0,u=x,v=y} end
-- A quad split across duplicate UV/normal seam vertices, plus a disconnected triangle.
local v={vertex(0,0),vertex(1,0),vertex(1,1),vertex(0,0),vertex(1,1),vertex(0,1),
    vertex(5,0),vertex(6,0),vertex(5,1)}
local tris={{1,2,3},{4,5,6},{7,8,9}}
assert(#Islands.build(tris,v,'indices',0)==3)
for _,mode in ipairs({'vertices','edges'}) do
    local islands=Islands.build(tris,v,mode,0)
    assert(#islands==2 and #islands[1]==2 and islands[1][1]==tris[1])
end
-- A single contact point joins vertex-connected islands, but not edge-connected ones.
local contact={vertex(0,0),vertex(1,0),vertex(0,1),vertex(-1,0),vertex(0,-1)}
assert(#Islands.build({{1,2,3},{1,4,5}},contact,'vertices',0)==1)
assert(#Islands.build({{1,2,3},{1,4,5}},contact,'edges',0)==2)
-- Spatial neighbor checks cross cell boundaries and use Euclidean distance, not cell identity.
v[4].x=0.009; v[5].x=1.009
assert(#Islands.build(tris,v,'edges',0.008)==3)
assert(#Islands.build(tris,v,'edges',0.01)==2)
assert(v[4].x==0.009 and v[5].x==1.009,'connectivity must not weld input geometry')
-- Signed zero is identical; nearby but distinct positions are separate at tolerance zero.
v[4].x=-0.0; v[5].x=1
assert(#Islands.build(tris,v,'edges',0)==2)
v[4].x=1e-10
assert(#Islands.build(tris,v,'edges',0)==3)
-- Increasing tolerance may merge components, but must not split an existing surface
-- when both endpoints of a short real edge enter the same tolerance cluster.
local previous=math.huge
for _, tolerance in ipairs({0,0.01,0.5,1,2,10}) do
    local count=#Islands.build(tris,v,'edges',tolerance)
    assert(count<=previous,'edge tolerance fragmented connected triangles')
    previous=count
end
assert(#Islands.build({}, {}, 'edges',0)==0)
assert(not pcall(Islands.build,tris,v,'edges',-1))
assert(not pcall(Islands.build,tris,v,'edges',math.huge))
-- Connectivity is transitive, including distinct vertices inside one tolerance chain.
local chain={vertex(0,0),vertex(0,10),vertex(0,20),vertex(0.09,0),vertex(10,10),vertex(10,20),
    vertex(0.18,0),vertex(20,10),vertex(20,20)}
assert(#Islands.build(tris,chain,'vertices',0.1)==1)
-- Same-size components retain source-face ordering for stable subset output.
local separated=Islands.build(tris,chain,'indices',0)
for i=1,3 do assert(separated[i][1]==tris[i]) end
-- A many-triangle fan exercises high-valence topology without rescanning incident-face lists.
local fanVertices={vertex(0,0)}; local fan={}
for i=1,10000 do
    fanVertices[#fanVertices+1]=vertex(i,1)
    fanVertices[#fanVertices+1]=vertex(i,2)
    fan[#fan+1]={1,#fanVertices-1,#fanVertices}
end
assert(#Islands.build(fan,fanVertices,'indices',0)==1)
print('Mesh Debug island detection OK')
