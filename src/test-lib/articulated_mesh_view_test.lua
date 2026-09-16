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
local View=require 'articulated_mesh_view'
local captures={window=false,active=false,want=false}
tImGui={IsAnyWindowHovered=function() return captures.window end,
    IsAnyItemActive=function() return captures.active end,
    GetWantCaptureMouse=function() return captures.want end,GetZoom=function() return 0 end}
local calls=0
local E={cam={},bounds={minX=-1,minY=-2,minZ=-3,maxX=1,maxY=2,maxZ=3},showTimeline=true,
    camera={setPos=function(self,x,y,z) calls=calls+1; self.position={x,y,z} end,
    setFocus=function(self,x,y,z) self.focus={x,y,z} end,
    getNormal=function(_,axis) return axis=='R' and {x=1,y=0,z=0} or {x=0,y=1,z=0} end}}
local function near(a,b) assert(math.abs(a-b)<0.0001,tostring(a)..' ~= '..tostring(b)) end
View.reset(E); local distance=E.cam.distance; local initial={E.cam.fx,E.cam.fy,E.cam.fz}
local picked=0; View.pick=function(_,x,y) assert(x==10 and y==20); picked=picked+1 end
View.pointerDown(E,0,10,20); View.pointerMove(E,30,30)
near(E.cam.azimuth,0.2); near(E.cam.elevation,0.35); assert(picked==0)
View.pointerUp(E,0); local before=calls; View.pointerMove(E,50,50); assert(calls==before)
View.pointerDown(E,1,10,20); View.pointerMove(E,30,30); assert(picked==1 and calls==before)
for key in pairs(captures) do
    View.pointerDown(E,0,0,0); captures[key]=true
    View.pointerMove(E,100,100); assert(calls==before and not E.cameraDrag)
    View.pointerDown(E,1,10,20); assert(picked==1)
    captures[key]=false
end
View.pointerDown(E,2,0,0); View.pointerMove(E,10,10); assert(E.cam.fx~=0 and E.cam.fy~=0)
E.cam.distance=1; View.reset(E)
near(E.cam.azimuth,0.3); near(E.cam.elevation,0.3); near(E.cam.fx,initial[1]); near(E.cam.fy,initial[2]); near(E.cam.fz,initial[3]); near(E.cam.distance,distance)
-- Vertical pan must not change X/Z or zoom, even with an inclined camera.
local x,z=E.cam.fx,E.cam.fz
View.pointerDown(E,2,0,0); View.pointerMove(E,0,100); View.pointerUp(E,2)
near(E.cam.fx,x); near(E.cam.fz,z); near(E.cam.distance,distance)
near(E.cam.fy,initial[2]+100*distance*0.001)
local focus={E.cam.fx,E.cam.fy,E.cam.fz}
View.pointerDown(E,0,0,0)
for i=1,30 do
    View.pointerMove(E,i*7,i*2)
    local squared=0
    for axis=1,3 do near(E.camera.focus[axis],focus[axis]); squared=squared+(E.camera.position[axis]-focus[axis])^2 end
    near(math.sqrt(squared),distance)
end
View.pointerUp(E,0)
local idle=calls; View.input(E); View.pointerMove(E,0,0); assert(calls==idle)
print('ARTICULATED MESH LEFT ORBIT / RIGHT PICK / GUI CAPTURE / RESET / FIXED FOCUS / PAN / IDLE OK')
