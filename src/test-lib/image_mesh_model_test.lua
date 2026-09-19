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
local M=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local p=M.new('/tmp/source.png',1344,768)
M.validate(p)
local ids=M.grid(p,{columns=4,rows=3,marginX=40,marginY=27,gapX=100,gapY=36})
assert(#ids==12 and #p.regions==12)
M.validate(p)
local r=p.regions[1]; r.overrides.relief=4
assert(M.options(p,r).relief==4 and M.options(p,r).depth==20)
p.defaults.depth=30; assert(M.options(p,r).depth==30)
local h=M.history(); M.commit(h,p); local copy=M.copy(p); copy.regions[1].x=41
local old=M.undo(h,copy); assert(old.regions[1].x==40)
assert(M.undo(h,old,true).regions[1].x==41)
local polygon=M.fromPoints(p,{{x=0,y=0},{x=40,y=0},{x=40,y=20},{x=20,y=20},{x=20,y=40},{x=0,y=40}})
assert(polygon.w==41 and polygon.h==41)
assert(M.contains(M.outline(polygon),10,30)); assert(not M.contains(M.outline(polygon),30,30))
local ellipse=M.add(p,'ellipse',10,10,60,30); assert(#M.outline(ellipse)==48)
M.validate(p)
local bad=M.copy(p); bad.regions[1].x=1344; assert(not pcall(M.validate,bad))
bad=M.copy(p); bad.version=2; assert(not pcall(M.validate,bad))
bad=M.copy(p); bad.regions[1].overrides.columns=0; assert(not pcall(M.validate,bad))
assert(IO.relative('/tmp/source.png','/tmp/project.imesh')=='source.png')
assert(IO.resolve('source.png','/tmp/project.imesh')=='/tmp/source.png')
assert(IO.exportName({id=12,name='../evil/name'}):match('^012_') and not IO.exportName({id=12,name='../evil/name'}):find('/'))
for _,kind in ipairs({'rectangle','circle','ellipse','triangle','regular'}) do
    local r=M.primitive(p,kind,40,30,6,-100,10000)
    M.validate(p)
    assert(r.x==0 and r.y+r.h==p.image.height)
    if kind=='circle' then assert(r.w==r.h and M.options(p,r).height==M.options(p,r).width) end
    if kind=='triangle' then assert(#r.contour==3) end
    if kind=='regular' then assert(#r.contour==6) end
end
local count=#p.regions
assert(not pcall(M.primitive,p,'regular',40,30,2))
assert(not pcall(M.primitive,p,'rectangle',0,30,6))
assert(not pcall(M.primitive,p,'circle',900,30,6))
assert(#p.regions==count)
print('IMAGE MESH MODEL OK')
