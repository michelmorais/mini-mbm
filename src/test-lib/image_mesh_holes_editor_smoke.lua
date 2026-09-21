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
local Presets=require 'image_mesh_presets'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started,baseline
local function close(a,b) assert(math.abs(a-b)<.00001,string.format('expected %.6f, got %.6f',b,a)) end
local function run()
 init()
 local pixels={};for i=1,64*64 do pixels[#pixels+1]=80;pixels[#pixels+1]=150;pixels[#pixels+1]=220 end
 assert(mbm.createTexture(pixels,64,64,3,'holes_editor','/tmp/ime_holes_editor.png'))
 api.openImage('/tmp/ime_holes_editor.png')
 assert(api.action(function(p) local r=Model.add(p,'rectangle',0,0,64,64);r.overrides.columns=6;r.overrides.rows=6 end))
 api.select(1,false)
 local E=api.state
 assert(api.holes.add(E,api.action,1));E.tool='holes';Canvas.sync(E)
 local function touch(fn,x,y)
  local t=Canvas.transform(E)
  fn(0,(t.x+x*63*t.scale)/E.camera2d.sx,(t.y+y*63*t.scaleY)/E.camera2d.sy)
 end
 -- Consecutive Add operations must place and select distinct editable holes.
 assert(api.holes.add(E,api.action,1));assert(E.holeIndex==2)
 assert(api.holes.add(E,api.action,2));assert(E.holeIndex==3)
 assert(#E.project.regions[1].holes==3);Model.validate(E.project)
 local vertex=Model.copy(E.project.regions[1].holes[3][1])
 touch(onTouchDown,vertex.x,vertex.y)
 touch(onTouchMove,vertex.x+.005,vertex.y)
 touch(onTouchUp,vertex.x+.005,vertex.y)
 close(E.project.regions[1].holes[3][1].x,vertex.x+.005)
 local Geometry=require 'image_mesh_holes_geometry'
 local function current() return E.project.regions[1].holes[3] end
 assert(current().preserveShape and Geometry.isEllipse(current()))
 local b=Geometry.bounds(current())
 touch(onTouchDown,b.x,b.y+b.ry)
 touch(onTouchMove,b.x,b.y+b.ry+.01)
 touch(onTouchUp,b.x,b.y+b.ry+.01)
 local resized=Geometry.bounds(current());close(resized.rx,b.rx);close(resized.ry,b.ry+.01)
 touch(onTouchDown,resized.x+resized.rx,resized.y+resized.ry)
 touch(onTouchMove,resized.x+resized.rx-.02,resized.y+resized.ry-.02)
 touch(onTouchUp,resized.x+resized.rx-.02,resized.y+resized.ry-.02)
 local circle=Geometry.bounds(current());close(circle.rx,circle.ry);assert(Geometry.isEllipse(current()))
 assert(api.holes.setPreserve(E,api.action,false));assert(not current().preserveShape)
 api.saveProject('/tmp/ime_holes_resize.imesh')
 local resizedProject=IO.load('/tmp/ime_holes_resize.imesh')
 assert(resizedProject.regions[1].holes[3].primitive=='ellipse')
 assert(resizedProject.regions[1].holes[3].preserveShape==false)
 assert(api.holes.setPreserve(E,api.action,true));assert(current().preserveShape)
 api.saveProject('/tmp/ime_holes_resize.imesh')
 assert(IO.load('/tmp/ime_holes_resize.imesh').regions[1].holes[3].preserveShape==true)
 api.undo(false);api.undo(false);api.undo(false);api.undo(false)
 api.undo(false);api.undo(false);api.undo(false)
 assert(#E.project.regions[1].holes==1)
 api.undo(true);assert(#E.project.regions[1].holes==2)
 api.undo(false)
 print('HOLES CONSECUTIVE ADD / SELECTION / WIDTH / HEIGHT / CIRCLE / PRESERVE / PERSISTENCE / UNDO / REDO OK')
 touch(onTouchDown,.4,.4);touch(onTouchMove,.35,.35);touch(onTouchUp,.35,.35)
 close(E.project.regions[1].holes[1][1].x,.35)
 api.undo(false);close(E.project.regions[1].holes[1][1].x,.4)
 api.undo(true);close(E.project.regions[1].holes[1][1].x,.35)
 touch(onTouchDown,.5,.5);touch(onTouchMove,.55,.5);touch(onTouchUp,.55,.5)
 close(E.project.regions[1].holes[1][1].x,.4)
 touch(onTouchDown,.4,.35);touch(onTouchMove,-.1,.35);touch(onTouchUp,-.1,.35)
 close(E.project.regions[1].holes[1][1].x,.4) -- invalid drag rejected
 touch(onTouchDown,.4,.35);touch(onTouchMove,.3,.3);Canvas.cancel(E);api.select(1,false)
 close(E.project.regions[1].holes[1][1].x,.4)
 E.tool='hole_draw'
 for _,p in ipairs{{.1,.1},{.25,.1},{.25,.25},{.1,.25}} do touch(onTouchDown,p[1],p[2]);touch(onTouchUp,p[1],p[2]) end
 assert(api.holes.finish(E,api.action));assert(#E.project.regions[1].holes==2)
 assert(api.action(function(p) Presets.store(p,'Keep holes',E.values) end))
 assert(api.action(function(p) Presets.apply(p,1,{[1]=true},false) end));assert(#E.project.regions[1].holes==2)
 api.saveProject('/tmp/ime_holes_editor.imesh')
 local saved=IO.load('/tmp/ime_holes_editor.imesh');assert(#saved.regions[1].holes==2)
 api.openProject('/tmp/ime_holes_editor.imesh');api.select(1,false)
 assert(#E.project.regions[1].holes==2)
 assert(api.exportOne('/tmp/ime_holes_editor.msh'))
 api.setEditMode(false);api.rebuild();api.setWireframe(true);assert(E.wireObject)
 api.setWireframe(false);api.setEditMode(true);E.tool='holes';E.holeIndex=1;E.canvasDirty=true;Canvas.sync(E)
 started=mbm.getTimeRun()
 print('HOLES EDITOR / DRAW / VERTEX / MOVE / INVALID / CANCEL / HISTORY / PRESETS / SAVE / EXPORT OK')
end
function onInitScene() local ok,err=xpcall(run,debug.traceback);if not ok then print('HOLES EDITOR FAIL '..tostring(err));mbm.quit() end end
function onLoop(delta)
 if not started then return end
 local header=tImGui.CollapsingHeader
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_holes_title') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 loop(delta);tImGui.CollapsingHeader=header
 if not baseline and mbm.getTimeRun()-started>1 then baseline=api.state.canvasBuilds end
 if mbm.getTimeRun()-started>4 then
  assert(baseline==api.state.canvasBuilds,'idle holes redraw')
  print('HOLES EDITOR GUI / IDLE OK');mbm.quit()
 end
end
