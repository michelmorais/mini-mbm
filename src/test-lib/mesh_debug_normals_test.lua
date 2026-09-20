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
