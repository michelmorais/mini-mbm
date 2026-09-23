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
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local task,started,baseline,heightBaseline,buildBaseline
local function close(a,b) assert(math.abs(a-b)<.0001,tostring(a)..' != '..tostring(b)) end
local function run()
 init()
 local path='/tmp/ime_curved_editor.png';local pixels={};for i=1,129*129*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,129,129,3,'ime_curved_editor',path));api.openImage(path)
 assert(api.action(function(p)
  local r=Model.add(p,'rectangle',0,0,129,129)
  r.heightEdits={{x=.5,y=.5,radius=.1,strength=1,height=0,mode='flatten'}}
  r.overrides.simplify=true
 end))
 api.select(1,false);local E=api.state
 E.values.heightSource='curved';E.values.curvedRadius=15;E.values.columns=8;E.values.rows=8
 assert(api.applyProperties());E.tool='curved';Canvas.sync(E)
 local function touch(fn,x,y)
  local t=Canvas.transform(E);fn(0,(t.x+x*128*t.scale)/E.camera2d.sx,(t.y+y*128*t.scaleY)/E.camera2d.sy)
 end
 local function options() return Model.options(E.project,E.project.regions[1]) end
 touch(onTouchDown,.5,.5);touch(onTouchMove,.55,.52);touch(onTouchUp,.55,.52)
 close(options().curvedX,.55);close(options().curvedY,.52)
 api.undo(false);close(options().curvedX,.5);api.undo(true);close(options().curvedX,.55)
 E.tool='curved';Canvas.sync(E)
 touch(onTouchDown,.7,.52);touch(onTouchMove,.75,.52);touch(onTouchUp,.75,.52);close(options().curvedRadius,20)
 -- Escape restores the project snapshot without a new history item.
 E.tool='curved';Canvas.sync(E)
 touch(onTouchDown,.55,.52);touch(onTouchMove,.56,.52)
 Canvas.cancel(E);close(options().curvedX,.55)
 -- Locked regions expose no interactive target handles.
 assert(api.action(function(p) p.regions[1].locked=true end));E.tool='curved';Canvas.sync(E)
 touch(onTouchDown,.55,.52);touch(onTouchMove,.56,.52);touch(onTouchUp,.56,.52);close(options().curvedX,.55)
 assert(api.action(function(p) p.regions[1].locked=false end));E.tool='curved'
 E.values.curvedTarget=9;Canvas.sync(E)
 touch(onTouchDown,.55,.52);touch(onTouchMove,.54,.52);touch(onTouchUp,.54,.52)
 close(options().curvedTarget,9);close(options().curvedX,.54)
 assert(#E.project.regions[1].heightEdits==1 and options().simplify)
 api.saveProject('/tmp/ime_curved_editor.imesh')
 local saved=IO.load('/tmp/ime_curved_editor.imesh');local o=Model.options(saved,saved.regions[1]);close(o.curvedRadius,20);close(o.curvedTarget,9)
 api.openProject('/tmp/ime_curved_editor.imesh');api.select(1,false)
 api.exportOne('/tmp/ime_curved_editor.msh');while E.meshTask do coroutine.yield() end
 assert(IO.exists('/tmp/ime_curved_editor.msh'),E.status)
 E.heightView=2;E.heightRequested=true
 repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError
 assert(E.heightObject,E.heightError)
 E.tool='curved';E.canvasDirty=true;Canvas.sync(E)
 started=mbm.getTimeRun()
 print('CURVED EDITOR MOVE / RESIZE / HISTORY / PERSISTENCE / EXPORT / MAP OK')
end
function onInitScene()
 task=coroutine.create(run);local ok,err=coroutine.resume(task)
 if not ok then print('CURVED EDITOR FAIL '..tostring(err));mbm.quit() end
end
function onLoop(delta)
 local header=tImGui.CollapsingHeader
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_grooves_group') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 loop(delta);tImGui.CollapsingHeader=header
 if coroutine.status(task)~='dead' then
  local ok,err=coroutine.resume(task)
  if not ok then print('CURVED EDITOR FAIL '..tostring(err));mbm.quit() end
  return
 end
 if not started then return end
 local E=api.state
 if not baseline and mbm.getTimeRun()-started>1 then baseline=E.canvasBuilds;heightBaseline=E.heightBuilds;buildBaseline=E.builds end
 if baseline and mbm.getTimeRun()-started>3 then
  assert(E.canvasBuilds==baseline and E.heightBuilds==heightBaseline and E.builds==buildBaseline,'idle editor rebuilds curved geometry')
  print('CURVED EDITOR IDLE / REAL IMGUI PANEL OK');mbm.quit()
 end
end
