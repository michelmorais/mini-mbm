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
local task,started,baseline,injected,helpSeen
local function run()
 init()
 local path='/tmp/ime_facets_editor.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_facets_editor',path));api.openImage(path)
 assert(api.action(function(p) Model.add(p,'ellipse',0,0,65,65) end));api.select(1,false)
 local E=api.state
 E.values.heightSource='curved';E.values.curvedRadius=15;E.values.curvedTarget=35
 E.values.curvedSimplify=true
 assert(api.applyProperties());assert(not E.values.curvedFaceted)
 started=mbm.getTimeRun()
 while not injected do coroutine.yield() end
 assert(E.values.curvedFaceted and E.values.curvedFacetSectors==12 and E.values.curvedFacetRings==2)
 assert(api.applyProperties());api.undo(false);assert(not Model.options(E.project,E.project.regions[1]).curvedFaceted)
 api.undo(true);assert(Model.options(E.project,E.project.regions[1]).curvedFaceted)
 api.saveProject('/tmp/ime_facets_editor.imesh')
 local saved=IO.load('/tmp/ime_facets_editor.imesh');local o=Model.options(saved,saved.regions[1])
 assert(o.curvedFaceted and o.curvedFacetSectors==12 and o.curvedFacetRings==2 and o.curvedSimplify)
 api.openProject('/tmp/ime_facets_editor.imesh');api.select(1,false)
 api.exportOne('/tmp/ime_facets_editor.msh');while E.meshTask do coroutine.yield() end
 assert(IO.exists('/tmp/ime_facets_editor.msh'),E.status)
 api.updateStatistics();while E.meshTask do coroutine.yield() end
 assert(E.report and E.report.vertices==E.report.triangles*3,E.status)
 assert(not E.report.curvedSourceTriangles,'faceting was simplified')
 E.heightView=2;E.heightRequested=true
 repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError
 assert(E.heightObject,E.heightError)
 print('FACETS EDITOR UI / HISTORY / SAVE / REOPEN / EXPORT / MAP OK')
 assert(api.action(function(p)
  local r=p.regions[1];Model.curved.convert(r,Model.options(p,r))
  Model.curved.add(r,1,'target','rectangle',12)
  r.curvedNodes[1].profile='bezier';r.curvedNodes[1].bezier1=1;r.curvedNodes[1].bezier2=0
 end))
 api.undo(false);assert(not E.project.regions[1].curvedNodes)
 api.undo(true);assert(#E.project.regions[1].curvedNodes==2)
 E.tool='curved';E.curvedPanelOpen=true;E.curvedNode=1
 api.saveProject('/tmp/ime_facet_chain_editor.imesh')
 api.openProject('/tmp/ime_facet_chain_editor.imesh');api.select(1,false)
 local chain=E.project.regions[1].curvedNodes
 assert(#chain==2 and chain[1].profile=='bezier' and chain[2].thickness==12)
 assert(Model.options(E.project,E.project.regions[1]).curvedFaceted)
 E.tool='curved';E.curvedPanelOpen=true;E.curvedNode=1
 api.exportOne('/tmp/ime_facet_chain_editor.msh');while E.meshTask do coroutine.yield() end
 assert(IO.exists('/tmp/ime_facet_chain_editor.msh'),E.status)
 api.updateStatistics();while E.meshTask do coroutine.yield() end
 assert(E.report and E.report.vertices==E.report.triangles*3,E.status)
 E.heightView=2;E.heightRequested=true
 repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError
 assert(E.heightObject,E.heightError)
 print('FACETS EDITOR CHAIN / CONVERSION / HISTORY / PROFILE PRESERVATION / SAVE / MAP / EXPORT OK')
 started=mbm.getTimeRun()
end
function onInitScene()
 task=coroutine.create(run);local ok,e=coroutine.resume(task)
 if not ok then print('FACETS EDITOR FAIL '..tostring(e));mbm.quit() end
end
function onLoop(delta)
 local header,checkbox,input,text=tImGui.CollapsingHeader,tImGui.Checkbox,tImGui.InputInt,tImGui.TextWrapped
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_grooves_group') or label==tLang.L('simplify_geometry') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 tImGui.TextWrapped=function(value,...)
  if value==tLang.L('ime_facets_help') then helpSeen=true end
  return text(value,...)
 end
 if started and not injected then
  tImGui.Checkbox=function(label,...)
   local value=checkbox(label,...)
   if label==tLang.L('ime_facets_enabled') then return true end
   return value
  end
  tImGui.InputInt=function(label,...)
   local c,v=input(label,...)
   if label=='##ime_facets_sectors' then return true,12 end
   if label=='##ime_facets_rings' then injected=true;return true,2 end
   return c,v
  end
 end
 loop(delta);tImGui.CollapsingHeader=header;tImGui.Checkbox=checkbox;tImGui.InputInt=input;tImGui.TextWrapped=text
 if coroutine.status(task)~='dead' then
  local ok,e=coroutine.resume(task)
  if not ok then print('FACETS EDITOR FAIL '..tostring(e));mbm.quit() end
  return
 end
 if not started then return end
 local E=api.state
 if not baseline and mbm.getTimeRun()-started>1 then baseline={E.builds,E.statisticsBuilds,E.heightBuilds,E.canvasBuilds} end
 if baseline and mbm.getTimeRun()-started>3 then
  assert(helpSeen,'facet guidance missing')
  assert(baseline[1]==E.builds and baseline[2]==E.statisticsBuilds and baseline[3]==E.heightBuilds and baseline[4]==E.canvasBuilds,'idle rebuild')
  print('FACETS EDITOR IMGUI / IDLE OK');mbm.quit()
 end
end
