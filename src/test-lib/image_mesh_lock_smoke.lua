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
local started,baseline
local function run()
 init()
 local pixels={};for i=1,256*256*3 do pixels[i]=180 end
 local path='/tmp/ime_locked.png';assert(mbm.createTexture(pixels,256,256,3,'ime_locked',path))
 api.openImage(path)
 assert(api.action(function(p)
  Model.add(p,'rectangle',70,70,70,70)
  Model.add(p,'rectangle',30,30,190,190)
 end))
 api.select(2,false)
 local E=api.state
 E.values.relief=9;E.draft.name='Pending name'
 assert(api.setRegionLocked(true))
 assert(E.values.relief==9 and E.draft.name=='Pending name','locking discarded pending edits')
 assert(E.project.regions[2].locked and E.draft.locked)
 api.undo(false);assert(not E.project.regions[2].locked)
 api.undo(true);assert(E.project.regions[2].locked)
 api.fit(E);Canvas.sync(E)
 local capture=tImGui.GetWantCaptureMouse;tImGui.GetWantCaptureMouse=function() return false end
 local function xy(x,y)
  local t=Canvas.transform(E)
  return (t.x+x*t.scale)/E.camera2d.sx,(t.y+y*t.scaleY)/E.camera2d.sy
 end
 local function down(x,y) local sx,sy=xy(x,y);onTouchDown(0,sx,sy);return sx,sy end
 local function up(sx,sy) onTouchUp(0,sx,sy) end
 -- Locked topmost region must not hide an editable region underneath.
 local x,y=down(100,100);assert(E.selected==1 and E.drag and E.drag.id==1);up(x,y)
 -- Selecting from the list still exposes all fields, but scene handles stay inert.
 api.select(2,false);Canvas.sync(E)
 local before=Model.copy(E.project.regions[2])
 x,y=down(219,219);assert(E.panDrag and not E.drag,'locked resize handle is active');up(x,y)
 x,y=down(180,180);assert(E.panDrag and not E.drag,'locked region intercepted empty scene')
 onTouchMove(0,x+15,y+10);up(x+15,y+10)
 assert(E.project.regions[2].x==before.x and E.project.regions[2].w==before.w)
 -- Module creation is still available over the locked image area.
 E.tool='rectangle';x,y=down(160,160)
 local ex,ey=xy(190,190);onTouchMove(0,ex,ey);up(ex,ey)
 assert(#E.project.regions==3 and not E.project.regions[3].locked)
 api.select(2,false);Canvas.sync(E)
 local t=Canvas.transform(E);local sx,sy=t.x+100*t.scale,t.y+100*t.scaleY
 local H={select=function() error('unexpected selection') end,action=api.action,commitDrag=function() error('unexpected edit') end}
 for _,tool in ipairs{'holes','hole_draw','hole_freehand','height_areas','area_draw','area_freehand','area_line','side_band','back_uv'} do
  E.tool=tool
  assert(not Canvas.input(E,H,'down',sx,sy),'locked tool '..tool)
  assert(not E.drag and not E.stroke)
 end
 E.tool='select';api.paint.state(E).enabled=true
 assert(not api.paintInput('down',sx,sy));assert(not E.paintDrag)
 api.paint.state(E).enabled=false
 -- Numeric panel editing remains valid while locked.
 E.values.depth=31;assert(api.applyProperties())
 assert(Model.options(E.project,E.project.regions[2]).depth==31 and E.project.regions[2].locked)
 assert(Model.options(E.project,E.project.regions[2]).locked==nil,'editor lock leaked into generation options')
 assert(api.saveProject('/tmp/ime_locked.imesh'));api.openProject('/tmp/ime_locked.imesh')
 assert(E.project.regions[2].locked);api.select(2,false)
 assert(api.setRegionLocked(false));Canvas.sync(E)
 x,y=down(205,180);assert(E.drag and E.drag.id==2,'unlock did not restore picking');up(x,y)
 local old=Model.copy(E.project);for _,r in ipairs(old.regions) do r.locked=nil end
 IO.save(old,'/tmp/ime_locked_legacy.imesh',tUtil.save)
 local loaded=IO.load('/tmp/ime_locked_legacy.imesh');for _,r in ipairs(loaded.regions) do assert(r.locked==false) end
 local invalid=Model.copy(loaded);invalid.regions[1].locked='yes';assert(not pcall(Model.validate,invalid))
 tImGui.GetWantCaptureMouse=capture
 api.select(2,false);api.setRegionLocked(true)
 print('LOCKED MODULE PICK THROUGH / HANDLES / PAINT / SCOPED TOOLS / CREATE / PANEL / UNDO / SAVE OK')
 started=mbm.getTimeRun()
end
function onInitScene()
 local ok,err=xpcall(run,debug.traceback)
 if not ok then print('LOCKED MODULE FAIL '..tostring(err));mbm.quit() end
end
function onLoop(delta)
 if not started then return end
 loop(delta)
 local E=api.state
 if mbm.getTimeRun()-started>2 and not baseline then baseline=E.canvasBuilds end
 if mbm.getTimeRun()-started>4 then
  assert(E.canvasBuilds==baseline,'idle rebuild')
  print('LOCKED MODULE GUI / IDLE OK');mbm.quit()
 end
end
