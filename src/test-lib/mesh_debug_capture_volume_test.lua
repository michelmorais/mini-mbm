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
local volume=require 'mesh_debug_capture_volume'
local function p(x,y,z) return {x=x,y=y,z=z} end
for _,kind in ipairs({'sphere','cylinder'}) do
    local box={volumeType=kind,aabbMin=p(-1,-1,-1),aabbMax=p(1,1,1)}
    assert(volume.contains(p(0,0,0),box))
    assert(volume.contains(p(1,0,0),box))
    assert(not volume.contains(p(0.9,0.9,0),box))
    assert(not volume.contains(p(0,0,1.01),box))
    assert(volume.intersects(p(-3,-3,0),p(3,-3,0),p(0,3,0),box),'crossing face')
    assert(not volume.intersects(p(2,0,0),p(3,1,0),p(3,-1,0),box),'outside')
    assert(volume.intersects(p(1,-2,0),p(1,2,0),p(2,0,0),box),'tangent')
    assert(volume.intersects(p(-2,0,0),p(2,0,0),p(2,0,0),box),'degenerate segment')
    assert(not volume.intersects(p(2,0,0),p(2,0,0),p(2,0,0),box),'degenerate point')
    assert(not volume.intersects(p(-3,-3,1.1),p(3,-3,1.1),p(0,3,1.1),box),'beyond cap')
    assert(volume.intersects(p(0,0,-3),p(0,0,3),p(3,0,0),box),'vertical crossing')
    assert(volume.contains(p(0.8,0,0.8),box)==(kind=='cylinder'),'sphere versus cylinder')
    local shifted={volumeType=kind,aabbMin=p(8,17,26),aabbMax=p(12,23,34)}
    assert(volume.contains(p(12,20,30),shifted),'translated nonuniform size')
    assert(not volume.contains(p(12.1,20,30),shifted))
    local vertices,lines=volume.geometry(kind,2,3,4)
    assert(#vertices%9==0 and #lines>0)
    for i=1,#vertices,3 do
        local x,y,z=vertices[i]/2,vertices[i+1]/3,vertices[i+2]/4
        assert(math.abs(x)<=1.00001 and math.abs(y)<=1.00001 and math.abs(z)<=1.00001)
        if kind=='sphere' then assert(math.abs(x*x+y*y+z*z-1)<1e-6) end
    end
end
print('CAPTURE VOLUME TEST OK')
