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
local task,started,baseline,injected=false,nil,nil,false
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
 started=mbm.getTimeRun()
end
function onInitScene()
 task=coroutine.create(run);local ok,e=coroutine.resume(task)
 if not ok then print('CURVED SIMPLIFY EDITOR FAIL '..tostring(e));mbm.quit() end
end
function onLoop(delta)
 local header,checkbox,slider=tImGui.CollapsingHeader,tImGui.Checkbox,tImGui.SliderFloat
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
 loop(delta);tImGui.CollapsingHeader=header;tImGui.Checkbox=checkbox;tImGui.SliderFloat=slider
 if coroutine.status(task)~='dead' then
  local ok,e=coroutine.resume(task)
  if not ok then print('CURVED SIMPLIFY EDITOR FAIL '..tostring(e));mbm.quit() end
  return
 end
 local E=api.state
 if not baseline and mbm.getTimeRun()-started>1 then baseline={E.builds,E.statisticsBuilds,E.heightBuilds,E.canvasBuilds} end
 if baseline and mbm.getTimeRun()-started>3 then
  assert(baseline[1]==E.builds and baseline[2]==E.statisticsBuilds and baseline[3]==E.heightBuilds and baseline[4]==E.canvasBuilds,'idle rebuild')
  print('CURVED SIMPLIFY EDITOR IDLE OK');mbm.quit()
 end
end
