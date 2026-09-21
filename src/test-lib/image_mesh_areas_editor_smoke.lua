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
local started,baseline,heightBaseline
local function close(a,b) assert(math.abs(a-b)<.00001,tostring(a)..' != '..tostring(b)) end
local function run()
 init()
 local path='/tmp/ime_areas_editor.png';local pixels={};for i=1,129*129*3 do pixels[i]=100 end
 assert(mbm.createTexture(pixels,129,129,3,'ime_areas_editor',path))
 api.openImage(path)
 assert(api.action(function(p) local r=Model.add(p,'rectangle',0,0,129,129);r.overrides.columns=8;r.overrides.rows=8;r.overrides.lockBorder=false end))
 api.select(1,false);local E=api.state;local A=api.areas
 assert(A.add(E,api.action,'rectangle'));assert(E.values.heightSource=='mixed')
 E.values.heightSource='manual';E.values.baseHeight=.2;assert(api.applyProperties())
 Canvas.sync(E)
 local function touch(fn,x,y)
  local t=Canvas.transform(E);fn(0,(t.x+x*128*t.scale)/E.camera2d.sx,(t.y+y*128*t.scaleY)/E.camera2d.sy)
 end
 local function area() return E.project.regions[1].heightAreas[E.areaIndex] end
 touch(onTouchDown,.5,.5);touch(onTouchMove,.55,.55);touch(onTouchUp,.55,.55);close(area()[1].x,.35)
 touch(onTouchDown,.75,.75);touch(onTouchMove,.8,.85);touch(onTouchUp,.8,.85);close(area()[3].x,.8);close(area()[3].y,.85)
 api.undo(false);close(area()[3].x,.75);api.undo(true);close(area()[3].x,.8)
 E.draft.heightAreas[1].height=.9;E.draft.heightAreas[1].name='Moldura'
 -- Touch must commit pending area properties before moving.
 touch(onTouchDown,.55,.55);touch(onTouchMove,.56,.55);touch(onTouchUp,.56,.55);close(area().height,.9)
 assert(A.add(E,api.action,'ellipse'));assert(#area()==32)
 assert(A.change(E,api.action,'duplicate'));assert(E.areaIndex==3)
 assert(A.change(E,api.action,'up'));assert(E.areaIndex==2)
 assert(A.change(E,api.action,'remove'));assert(#E.project.regions[1].heightAreas==2)
 E.tool='area_draw';E.polygon={}
 for _,p in ipairs{{.1,.1},{.2,.1},{.2,.2},{.1,.2}} do touch(onTouchDown,p[1],p[2]);touch(onTouchUp,p[1],p[2]) end
 assert(A.finish(E,api.action));assert(area().shape=='polygon')
 touch(onTouchDown,.1,.1);touch(onTouchMove,.08,.08);touch(onTouchUp,.08,.08);close(area()[1].x,.08)
 E.tool='area_freehand';touch(onTouchDown,.05,.7)
 for _,p in ipairs{{.2,.7},{.2,.9},{.05,.9},{.05,.7}} do touch(onTouchMove,p[1],p[2]) end
 touch(onTouchUp,.05,.7);assert(api.freehand.ready(E));assert(A.finish(E,api.action))
 E.draft.heightAreas[4].enabled=false;assert(api.applyProperties())
 api.saveProject('/tmp/ime_areas_editor.imesh');local saved=IO.load('/tmp/ime_areas_editor.imesh')
 assert(#saved.regions[1].heightAreas==4 and not saved.regions[1].heightAreas[4].enabled)
 assert(saved.regions[1].heightAreas[1].name=='Moldura');assert(Model.options(saved,saved.regions[1]).heightSource=='manual')
 api.openProject('/tmp/ime_areas_editor.imesh');api.select(1,false)
 assert(api.exportOne('/tmp/ime_areas_editor.msh'))
 E.heightView=2;E.heightRequested=true;api.updateHeightPreview();assert(E.heightObject,E.status)
 E.tool='height_areas';E.areaIndex=1;E.canvasDirty=true;Canvas.sync(E)
 started=mbm.getTimeRun()
 print('HEIGHT AREAS EDITOR MOVE / RESIZE / VERTEX / FREEHAND / ORDER / HISTORY / PERSISTENCE / EXPORT OK')
end
function onInitScene() local ok,err=xpcall(run,debug.traceback);if not ok then print('HEIGHT AREAS EDITOR FAIL '..tostring(err));mbm.quit() end end
function onLoop(delta)
 if not started then return end
 local header=tImGui.CollapsingHeader
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_grooves_group') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 loop(delta);tImGui.CollapsingHeader=header
 if not baseline and mbm.getTimeRun()-started>1 then baseline=api.state.canvasBuilds;heightBaseline=api.state.heightBuilds end
 if mbm.getTimeRun()-started>4 then
  assert(baseline==api.state.canvasBuilds and heightBaseline==api.state.heightBuilds,'idle area rebuild')
  print('HEIGHT AREAS EDITOR GUI / IDLE OK');mbm.quit()
 end
end
