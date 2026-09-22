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

local M=dofile('editor/mesh_debug_normals.lua')
local g={x=0,y=0,z=1}
assert(M.select({nx=.6,ny=0,nz=.8},g,1)==nil)
local x,y,z=M.select({nx=3,ny=0,nz=4},g,1);assert(x==.6 and z==.8)
for _,v in ipairs({{nx=0,ny=0,nz=0},{nx=0,ny=0,nz=-1},{nx=0/0,ny=0,nz=1},{nx=math.huge,ny=0,nz=1}}) do
 local x,y,z=M.select(v,g,1);assert(x==0 and y==0 and z==1)
end
local x,y,z=M.select({nx=.6,ny=0,nz=.8},g,2);assert(z==1)
assert(M.select({nx=0,ny=0,nz=0},nil,1)==nil)
print('NORMAL POLICY OK')

-- A broad plateau adjacent to a narrow inclined wall: geometry, not stored
-- normals or triangle count, determines the dominant patch at the seam.
local v={{x=0,y=0,z=0},{x=10,y=0,z=0},{x=10,y=10,z=0},{x=0,y=10,z=0},
    {x=-1,y=0,z=-2},{x=-1,y=10,z=-2}}
local ids={1,2,3,1,3,4,1,4,6,1,6,5}
local n=M.surfaces(v,ids,25)
assert(n[1].z==1 and n[4].z==1 and n[1].x==0)
assert(n[5].x<-.8 and n[5].z>0)
local joined=M.surfaces(v,ids,85);assert(joined[1].x<0 and joined[1].z<1)
local rotated={}
for i,p in ipairs(v) do rotated[i]={x=p.x,y=-p.z,z=p.y} end
local nr=M.surfaces(rotated,ids,25)
for i,a in pairs(n) do
    local b=nr[i];assert(math.abs(a.x-b.x)<1e-12 and math.abs(-a.z-b.y)<1e-12 and math.abs(a.y-b.z)<1e-12)
end
assert(next(M.surfaces(v,{1,1,1},25))==nil)
assert(M.select({nx=0,ny=0,nz=1},{x=1,y=0,z=0},3)==1)
assert(M.label(3)=='normal_method_surfaces')
-- Equal-size perpendicular patches blend rather than choosing by face order.
local cubeCorner={{x=0,y=0,z=0},{x=1,y=0,z=0},{x=0,y=1,z=0},{x=0,y=0,z=1}}
local equal=M.surfaces(cubeCorner,{1,2,3,1,4,2},25)
assert(math.abs(equal[1].y-equal[1].z)<1e-12)
print('SURFACE PATCHES / ANGLE / ROTATION / DEGENERATE / TIES OK')
