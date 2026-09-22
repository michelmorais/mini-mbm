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
local D=require 'image_mesh_detect'
local function detect(cavities,alpha)
 local data={}
 for y=0,8 do for x=0,8 do
  local inside=x>0 and x<8 and y>0 and y<8 and not cavities[y*9+x]
  local c=inside and 200 or 89
  data[#data+1]=string.char(c,c,c,255)
 end end
 return D.trace(table.concat(data),9,9,{x=0,y=0,w=9,h=9},1,1,
  {mode=2,alpha=alpha,tolerance=10,color={r=89/255,g=89/255,b=89/255}},function() end)
end
local a=detect({},127)
-- Two enclosed cavities meeting diagonally must not invalidate the outer ring.
for _,alpha in ipairs{0,127,254} do
 local b=detect({[3*9+3]=true,[4*9+4]=true},alpha)
 assert(#a==4 and #b==#a)
 for i,p in ipairs(a) do assert(p.x==b[i].x and p.y==b[i].y) end
end
-- A diagonal opening from outside is separated into a simple boundary.
local bytes={};for _,v in ipairs{1,1,1,1,0,1,1,1,0} do bytes[#bytes+1]=string.char(200,200,200,v*255) end
local ok,err=pcall(D.trace,table.concat(bytes),3,3,{x=0,y=0,w=3,h=3},0,0,
 {mode=1,alpha=127},function() end)
assert(ok and require('image_mesh_holes_geometry').simple(err),'diagonal corner remained invalid')
print('DETECT INTERNAL CONTACTS / OPAQUE ALPHA / DIAGONAL CORNER SEPARATION OK')
