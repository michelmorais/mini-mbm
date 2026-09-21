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
local Model=require 'image_mesh_model'
local Canvas=require 'image_mesh_canvas'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local task,started,calls,request
local function test()
 local E=api.state
 local pixels={};for i=1,65*65*3 do pixels[i]=100 end
 assert(mbm.createTexture(pixels,65,65,3,'statistics_source','/tmp/ime-statistics-source.png'))
 api.openImage('/tmp/ime-statistics-source.png')
 assert(api.action(function(p) Model.add(p,'rectangle',0,0,65,65) end));api.select(1,false)
 assert(api.areas.add(E,api.action,'rectangle'));E.values.heightSource='manual';assert(api.applyProperties())
 E.heightView=1;Canvas.sync(E)
 local function touch(fn,x,y)
  local t=Canvas.transform(E);fn(0,(t.x+x*64*t.scale)/E.camera2d.sx,(t.y+y*64*t.scaleY)/E.camera2d.sy)
 end
 for i=1,3 do
  coroutine.yield()
  touch(onTouchDown,.5+(i-1)*.02,.5)
  touch(onTouchMove,.5+i*.02,.5)
  touch(onTouchUp,.5+i*.02,.5)
  coroutine.yield();assert(calls==0,'drag triggered generation for face count')
 end
 assert(not E.report and not E.generatedMesh)
 request=true;coroutine.yield()
 while E.meshTask do coroutine.yield() end
 assert(not request and calls==1 and E.report and E.generatedMesh,'Calculate faces did not generate/cache')
 local triangles=E.report.triangles
 api.setEditMode(false);api.rebuild()
 assert(calls==1 and E.preview and E.report.triangles==triangles,'3D did not reuse explicit count')
 api.setEditMode(true)
 E.values.relief=E.values.relief+1;assert(api.applyProperties());coroutine.yield()
 assert(calls==1 and not E.report,'Apply in editing generated statistics automatically')
 api.setEditMode(false);api.rebuild();while E.meshTask do coroutine.yield() end;assert(calls==2 and E.preview,'3D failed to generate changed geometry')
 print('IMAGE MESH STATISTICS DRAG / IDLE / EXPLICIT BUTTON / CACHE / APPLY / 3D OK')
end
function onInitScene()
 init();started=mbm.getTimeRun();calls=0
 local native=mbm.startImageMesh;mbm.startImageMesh=function(...) calls=calls+1;return native(...) end
 local button=tImGui.Button;tImGui.Button=function(label,...)
  local pressed=button(label,...)
  if request and label==tLang.L('ime_faces_calculate') then request=false;return true end
  return pressed
 end
 task=coroutine.create(test)
end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH STATISTICS FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>20 then print('IMAGE MESH STATISTICS FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
