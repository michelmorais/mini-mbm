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
local task,started,calls
local function test()
 local E=api.state
 local pixels={};for i=1,65*33*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,65,33,3,'line_editor','/tmp/ime-line-editor-source.png'))
 api.openImage('/tmp/ime-line-editor-source.png')
 assert(api.action(function(p) Model.add(p,'rectangle',0,0,65,33) end));api.select(1,false)
 assert(api.areas.add(E,api.action,'line'));assert(E.project.regions[1].heightAreas[1].shape=='line')
 E.values.heightSource='manual';E.values.baseHeight=.8;assert(api.applyProperties())
 E.tool='area_line';E.polygon={};Canvas.sync(E)
 local function touch(fn,x,y)
  local t=Canvas.transform(E)
  fn(0,(t.x+x*64*t.scale)/E.camera2d.sx,(t.y+y*32*t.scaleY)/E.camera2d.sy)
 end
 for _,p in ipairs{{.2,.3},{.5,.6},{.8,.3}} do touch(onTouchDown,p[1],p[2]);touch(onTouchUp,p[1],p[2]) end
 assert(#E.polygon==3);assert(api.areas.finish(E,api.action))
 local function selected() return E.project.regions[1].heightAreas[E.areaIndex] end
 assert(selected().shape=='line' and #selected()==3)
 Canvas.sync(E)
 touch(onTouchDown,.5,.6);touch(onTouchMove,.5,.7);touch(onTouchUp,.5,.7)
 assert(math.abs(selected()[2].y-.7)<.001,'centerline handle did not move')
 assert(E.areaPoint==2)
 local a=E.draft.heightAreas[E.areaIndex];a.lineWidth=.2;assert(api.applyProperties())
 assert(selected().lineWidth==.2)
 assert(api.areas.change(E,api.action,'duplicate'));assert(#E.project.regions[1].heightAreas==3)
 assert(selected().lineWidth==.2 and selected().shape=='line')
 api.undo(false);assert(#E.project.regions[1].heightAreas==2)
 coroutine.yield();coroutine.yield();assert(calls==0,'line edit generated mesh implicitly')
 api.saveProject('/tmp/ime-line-editor.imesh');api.openProject('/tmp/ime-line-editor.imesh')
 assert(E.project.regions[1].heightAreas[2].lineWidth==.2)
 print('IMAGE MESH LINE EDITOR ADD / DRAW / HANDLES / WIDTH / DUPLICATE / UNDO / SAVE / IDLE OK')
end
function onInitScene()
 init();started=mbm.getTimeRun();calls=0
 local native=mbm.startImageMesh;mbm.startImageMesh=function(...) calls=calls+1;return native(...) end
 task=coroutine.create(test)
end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH LINE EDITOR FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>30 then print('IMAGE MESH LINE EDITOR FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
