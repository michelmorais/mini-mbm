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
local IO=require 'image_mesh_io'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local Graph=require 'image_mesh_curved_hierarchy'
local task,started,baseline,helpSeen
local function run()
 init()
 local path='/tmp/ime_interior_editor.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_interior_editor',path));api.openImage(path)
 local contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=.7,y=1},{x=.7,y=.3},{x=.3,y=.3},{x=.3,y=1},{x=0,y=1}}
 assert(api.action(function(p) Model.add(p,'polygon',0,0,65,65,contour) end));api.select(1,false)
 local E=api.state
 E.values.heightSource='curved';E.values.curvedInterior=true;E.values.columns=24;E.values.rows=24
 assert(api.applyProperties())
 assert(api.action(function(p)
  local r=p.regions[1];r.curvedNodes={};Model.curved.add(r,0,'target','polyline',8)
  Model.curved.seedInterior(r.curvedNodes[1],contour,r.holes)
 end))
 assert(Model.curved.geometry(E.draft.curvedNodes,contour,true))
 assert(Graph.edit(E,function(nodes)
  local n=nodes[1];n[1]={x=.15,y=.85};n[2]={x=.15,y=.15};n[3]={x=.85,y=.15};n[4]={x=.85,y=.85}
 end))
 assert(api.applyProperties())
 local original=Model.copy(E.draft.curvedNodes)
 assert(not Graph.edit(E,function(nodes) nodes[1][2]={x=.85,y=.85} end))
 assert(Model.curved.same(original,E.draft.curvedNodes) and E.graphEditError)
 E.graphEditError=nil
 assert(Graph.edit(E,function(nodes) nodes[1][1].y=.8 end));assert(api.applyProperties())
 api.undo(false);assert(E.draft.curvedNodes[1][1].y==.85)
 api.undo(true);assert(E.draft.curvedNodes[1][1].y==.8)
 api.saveProject('/tmp/ime_interior_editor.imesh');api.openProject('/tmp/ime_interior_editor.imesh');api.select(1,false)
 local o=Model.options(E.project,E.project.regions[1]);assert(o.curvedInterior and o.curvedNodes[1].shape=='polyline' and #o.curvedNodes[1]==4)
 E.tool='curved';E.curvedPanelOpen=true;E.curvedNode=1
 api.exportOne('/tmp/ime_interior_editor.msh');while E.meshTask do coroutine.yield() end
 assert(IO.exists('/tmp/ime_interior_editor.msh'),E.status)
 api.updateStatistics();while E.meshTask do coroutine.yield() end;assert(E.report,E.status)
 E.heightView=2;E.heightRequested=true
 repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError
 assert(E.heightObject,E.heightError)
 print('INTERIOR EDITOR POLYLINE / SEED / REJECTION / HISTORY / SAVE / MAP / EXPORT OK')
 started=mbm.getTimeRun()
end
function onInitScene()
 task=coroutine.create(run);local ok,e=coroutine.resume(task)
 if not ok then print('INTERIOR EDITOR FAIL '..tostring(e));mbm.quit() end
end
function onLoop(delta)
 local header,text=tImGui.CollapsingHeader,tImGui.TextWrapped
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_grooves_group') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 tImGui.TextWrapped=function(value,...)
  if value==tLang.L('ime_curved_interior_help') then helpSeen=true end
  return text(value,...)
 end
 loop(delta);tImGui.CollapsingHeader=header;tImGui.TextWrapped=text
 if coroutine.status(task)~='dead' then
  local ok,e=coroutine.resume(task)
  if not ok then print('INTERIOR EDITOR FAIL '..tostring(e));mbm.quit() end
  return
 end
 if not started then return end
 local E=api.state
 if not baseline and mbm.getTimeRun()-started>1 then baseline={E.builds,E.statisticsBuilds,E.heightBuilds,E.canvasBuilds} end
 if baseline and mbm.getTimeRun()-started>3 then
  assert(helpSeen,'interior guidance missing')
  assert(baseline[1]==E.builds and baseline[2]==E.statisticsBuilds and baseline[3]==E.heightBuilds and baseline[4]==E.canvasBuilds,'idle rebuild')
  print('INTERIOR EDITOR IMGUI / IDLE OK');mbm.quit()
 end
end
