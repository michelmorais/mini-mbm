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
local IO=require 'image_mesh_io'
local Geometry=require 'image_mesh_holes_geometry'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local task,started,baseline,helpSeen
local function close(a,b) assert(math.abs(a-b)<.0001,tostring(a)..' != '..tostring(b)) end
local function run()
 init()
 local path='/tmp/ime_curved_holes_editor.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_curved_holes_editor',path));api.openImage(path)
 assert(api.action(function(p) Model.add(p,'rectangle',0,0,65,65) end));api.select(1,false)
 local E=api.state
 E.values.heightSource='curved';E.values.curvedRadius=15;E.values.columns=8;E.values.rows=8
 E.values.curvedSimplify=true
 assert(api.applyProperties())
 assert(api.action(function(p) local r=p.regions[1];Model.curved.convert(r,Model.options(p,r)) end))
 local targets=Model.copy(E.project.regions[1].curvedNodes)
 assert(api.holes.add(E,api.action,2));E.tool='holes';Canvas.sync(E)
 local function hole() return E.project.regions[1].holes[1] end
 local function touch(fn,x,y)
  local t=Canvas.transform(E);fn(0,(t.x+x*64*t.scale)/E.camera2d.sx,(t.y+y*64*t.scaleY)/E.camera2d.sy)
 end
 touch(onTouchDown,.5,.5);touch(onTouchMove,.53,.5);touch(onTouchUp,.53,.5)
 close(Geometry.bounds(hole()).x,.53)
 api.undo(false);close(Geometry.bounds(hole()).x,.5)
 api.undo(true);close(Geometry.bounds(hole()).x,.53)
 assert(Model.curved.same(targets,E.project.regions[1].curvedNodes),'hole drag changed virtual targets')
 local b=Geometry.bounds(hole())
 touch(onTouchDown,b.x+b.rx,b.y);touch(onTouchMove,b.x+b.rx+.02,b.y);touch(onTouchUp,b.x+b.rx+.02,b.y)
 close(Geometry.bounds(hole()).rx,b.rx+.02)
 local previous=Model.copy(hole())
 touch(onTouchDown,.53,.5);touch(onTouchMove,-.1,.5);touch(onTouchUp,-.1,.5)
 assert(Model.curved.same(previous,hole()),'invalid hole move was accepted')
 assert(api.holes.add(E,api.action,1));assert(#E.project.regions[1].holes==2)
 api.saveProject('/tmp/ime_curved_holes_editor.imesh')
 local saved=IO.load('/tmp/ime_curved_holes_editor.imesh')
 assert(#saved.regions[1].holes==2 and Model.options(saved,saved.regions[1]).curvedSimplify)
 assert(Model.curved.same(targets,saved.regions[1].curvedNodes))
 api.openProject('/tmp/ime_curved_holes_editor.imesh');api.select(1,false)
 api.exportOne('/tmp/ime_curved_holes_editor.msh');while E.meshTask do coroutine.yield() end
 assert(IO.exists('/tmp/ime_curved_holes_editor.msh'),E.status)
 api.updateStatistics();while E.meshTask do coroutine.yield() end
 assert(E.report and E.report.curvedSourceTriangles,E.status)
 E.heightView=2;E.heightRequested=true
 repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError
 assert(E.heightObject,E.heightError)
 E.tool='holes';E.holeIndex=1;E.canvasDirty=true;Canvas.sync(E)
 started=mbm.getTimeRun()
 print('CURVED HOLES EDITOR MOVE / RESIZE / INVALID / HISTORY / SAVE / REOPEN / EXPORT / MAP OK')
end
function onInitScene()
 task=coroutine.create(run);local ok,e=coroutine.resume(task)
 if not ok then print('CURVED HOLES EDITOR FAIL '..tostring(e));mbm.quit() end
end
function onLoop(delta)
 local header,text=tImGui.CollapsingHeader,tImGui.TextWrapped
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_holes_title') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 tImGui.TextWrapped=function(value,...)
  if value==tLang.L('ime_holes_curved_help') then helpSeen=true end
  return text(value,...)
 end
 loop(delta);tImGui.CollapsingHeader=header;tImGui.TextWrapped=text
 if coroutine.status(task)~='dead' then
  local ok,e=coroutine.resume(task)
  if not ok then print('CURVED HOLES EDITOR FAIL '..tostring(e));mbm.quit() end
  return
 end
 if not started then return end
 local E=api.state
 if not baseline and mbm.getTimeRun()-started>1 then baseline={E.builds,E.statisticsBuilds,E.heightBuilds,E.canvasBuilds} end
 if baseline and mbm.getTimeRun()-started>3 then
  assert(helpSeen,'missing curved holes guidance')
  assert(baseline[1]==E.builds and baseline[2]==E.statisticsBuilds and baseline[3]==E.heightBuilds and baseline[4]==E.canvasBuilds,'idle rebuild')
  print('CURVED HOLES EDITOR IMGUI / IDLE OK');mbm.quit()
 end
end
