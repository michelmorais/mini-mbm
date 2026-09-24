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
local Comparison=require 'image_mesh_comparison'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local task,started,baseline,injected=false,nil,nil,false
local calls,comparisonSeen=0,false
local function run()
 init()
 local path='/tmp/ime_curved_simplify_editor.png';local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 assert(mbm.createTexture(pixels,65,65,3,'ime_simplify_editor',path));api.openImage(path)
 assert(api.action(function(p) Model.add(p,'rectangle',0,0,65,65) end))
 api.select(1,false);local E=api.state
 E.values.heightSource='curved';E.values.curvedRadius=15;E.values.columns=8;E.values.rows=8
 assert(api.applyProperties());assert(not E.values.curvedSimplify)
 -- Exercise the real ImGui controls in the next frame, then Apply/history.
 started=mbm.getTimeRun()
 while not injected do coroutine.yield() end
 assert(E.values.curvedSimplify and E.values.curvedSimplifyRatio==.5 and E.values.curvedSimplifyError==.02)
 assert(api.applyProperties());api.undo(false);assert(not Model.options(E.project,E.project.regions[1]).curvedSimplify)
 api.undo(true);assert(Model.options(E.project,E.project.regions[1]).curvedSimplify)
 api.saveProject('/tmp/ime_curved_simplify_editor.imesh')
 local saved=IO.load('/tmp/ime_curved_simplify_editor.imesh');local o=Model.options(saved,saved.regions[1])
 assert(o.curvedSimplify and o.curvedSimplifyRatio==.5 and o.curvedSimplifyError==.02)
 api.openProject('/tmp/ime_curved_simplify_editor.imesh');api.select(1,false)
 api.exportOne('/tmp/ime_curved_simplify_editor.msh');while E.meshTask do coroutine.yield() end
 assert(IO.exists('/tmp/ime_curved_simplify_editor.msh'),E.status)
 api.updateStatistics();while E.meshTask do coroutine.yield() end
 assert(E.report.curvedResultTriangles<E.report.curvedSourceTriangles,E.status)
 assert(not E.report.simplification,'unsafe generic simplifier was invoked')
 E.heightView=2;E.heightRequested=true
 repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError
 assert(E.heightObject,E.heightError)
 print('CURVED SIMPLIFY EDITOR UI / HISTORY / SAVE / REOPEN / EXPORT / MAP OK')
 local originalCache=assert(E.generatedMesh.originalPath)
 local before=calls
 api.setEditMode(false);api.rebuild();while E.meshTask do coroutine.yield() end
 assert(calls==before,'comparison regenerated the statistics mesh')
 assert(E.comparison and not IO.exists(originalCache),E.status)
 local original=meshDebug:new();assert(original:load(E.comparison.previewPath))
 assert(#original:getIndex(1,1)/3==E.report.sourceTriangles)
 assert(E.report.sourceTriangles>E.report.triangles)
 api.setComparison(true);assert(E.compareSideBySide and E.preview.visible and E.comparison.preview.visible)
 api.setWireframe(true);assert(E.wireObject.visible and E.comparison.wireObject.visible)
 Comparison.visibility(E,false,true);assert(not E.comparison.wireObject.visible and E.wireObject.visible)
 Comparison.visibility(E,true,false);assert(E.comparison.wireObject.visible and not E.wireObject.visible)
 api.setComparison(false);assert(E.wireObject.visible and not E.comparison.wireObject.visible)
 Comparison.visibility(E,true,true);api.setWireframe(false)
 local distance=E.orbit.distance
 api.setComparison(true);api.setComparison(false);assert(E.orbit.distance==distance and calls==before)
 -- Direct regeneration must also capture the unsimplified original, without cache.
 api.setEditMode(true);E.values.curvedSimplifyRatio=.6;assert(api.applyProperties())
 assert(not E.generatedMesh)
 api.setEditMode(false);api.rebuild();while E.meshTask do coroutine.yield() end
 assert(E.comparison and calls==before+2,E.status)
 -- Cancelling the reference generation must never install a partial comparison.
 api.setEditMode(true);E.values.curvedSimplifyRatio=.55;assert(api.applyProperties())
 local start=mbm.startImageMesh
 mbm.startImageMesh=function(path,options)
  local job,err=start(path,options)
  if job and options.heightSource=='curved' and not options.curvedSimplify then job:cancel() end
  return job,err
 end
 api.setEditMode(false);api.rebuild();while E.meshTask do coroutine.yield() end
 assert(E.generationCancelled and (not E.comparison or not E.comparison.preview.visible))
 mbm.startImageMesh=start
 api.setEditMode(true);E.values.curvedSimplifyRatio=.6;assert(api.applyProperties())
 api.setEditMode(false);api.rebuild();while E.meshTask do coroutine.yield() end
 assert(E.comparison,E.status);api.setComparison(true)
 print('CURVED COMPARISON CACHE / DIRECT / COUNTS / VISIBILITY / WIREFRAME / CAMERA / CANCEL OK')
 started=mbm.getTimeRun()
end
function onInitScene()
 local start=mbm.startImageMesh
 mbm.startImageMesh=function(...) calls=calls+1;return start(...) end
 task=coroutine.create(run);local ok,e=coroutine.resume(task)
 if not ok then print('CURVED SIMPLIFY EDITOR FAIL '..tostring(e));mbm.quit() end
end
function onLoop(delta)
 local header,checkbox,slider=tImGui.CollapsingHeader,tImGui.Checkbox,tImGui.SliderFloat
 local text=tImGui.Text
 tImGui.Text=function(value,...) if value==tLang.L('ime_comparison') then comparisonSeen=true end;return text(value,...) end
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('simplify_geometry') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 if started and not injected then
  tImGui.Checkbox=function(label,...)
   local value=checkbox(label,...)
   if label==tLang.L('ime_curved_simplify') then return true end
   return value
  end
  tImGui.SliderFloat=function(label,...)
   local c,v=slider(label,...)
   if label==tLang.L('ime_curved_simplify_ratio') then return true,.5 end
   if label==tLang.L('ime_curved_simplify_error') then injected=true;return true,.02 end
   return c,v
  end
 end
 loop(delta);tImGui.CollapsingHeader=header;tImGui.Checkbox=checkbox;tImGui.SliderFloat=slider;tImGui.Text=text
 if coroutine.status(task)~='dead' then
  local ok,e=coroutine.resume(task)
  if not ok then print('CURVED SIMPLIFY EDITOR FAIL '..tostring(e));mbm.quit() end
  return
 end
 local E=api.state
 if not baseline and mbm.getTimeRun()-started>1 then baseline={E.builds,E.statisticsBuilds,E.heightBuilds,E.canvasBuilds} end
 if baseline and mbm.getTimeRun()-started>3 then
  assert(baseline[1]==E.builds and baseline[2]==E.statisticsBuilds and baseline[3]==E.heightBuilds and baseline[4]==E.canvasBuilds,'idle rebuild')
  assert(comparisonSeen,'curved comparison panel missing')
  print('CURVED SIMPLIFY EDITOR IDLE OK');mbm.quit()
 end
end
