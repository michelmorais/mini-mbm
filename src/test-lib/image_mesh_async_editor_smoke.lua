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
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local task,started,frames,cancelButton,sawPanel
local function waitTask() while api.state.meshTask do coroutine.yield() end end
local function test()
 local E=api.state
 local pixels={};for i=1,257*257*3 do pixels[i]=100 end
 assert(mbm.createTexture(pixels,257,257,3,'async_editor','/tmp/ime-async-editor.png'))
 api.openImage('/tmp/ime-async-editor.png')
 assert(api.action(function(p)
  local r=Model.add(p,'rectangle',0,0,257,257)
  r.overrides.heightSource='manual';r.overrides.followImage=true
  r.heightAreas={{{x=.2,y=.2},{x=.8,y=.2},{x=.8,y=.8},{x=.2,y=.8},height=.9,transition=.01,name='Area',shape='rectangle',enabled=true}}
 end))
 api.select(1,false);api.setEditMode(false)
 local before=frames;api.rebuild();assert(E.imageJob);waitTask()
 assert(E.preview and frames>before+1 and sawPanel,E.status)
 local preview,path,builds=E.preview,E.previewPath,E.builds
 assert(api.action(function(p) p.regions[1].heightAreas[1].height=.7 end))
 api.rebuild();assert(E.imageJob)
 cancelButton=true;waitTask()
 assert(not cancelButton and E.generationCancelled and E.preview==preview and E.previewPath==path and E.builds==builds)
 assert(E.previewStale and not E.generatedMesh and not E.generationFailure)
 for _=1,5 do coroutine.yield() end
 assert(E.builds==builds and not E.meshTask,'cancelled generation restarted automatically')
 api.setEditMode(true);api.updateStatistics();assert(E.imageJob);onKeyDown(mbm.getKeyCode('ESC'));waitTask()
 assert(E.generationCancelled and not E.statistics[1] and not E.generatedMesh)
 api.updateStatistics();waitTask();assert(E.generatedMesh and E.report)
 api.setEditMode(false);api.rebuild();waitTask();assert(E.preview~=preview and not E.previewStale)
 -- Cancel before any export can overwrite an existing file.
 local output='/tmp/ime-async-cancelled.msh';local f=assert(io.open(output,'wb'));f:write('keep me');f:close()
 api.exportOne(output,true);assert(E.imageJob);api.generation.cancel(E);waitTask()
 f=assert(io.open(output,'rb'));assert(f:read('*a')=='keep me');f:close()
 api.openImage('/tmp/ime-async-editor.png')
 assert(not E.preview and not E.imageJob and not E.generatedMesh)
 print('IMAGE MESH ASYNC EDITOR FRAMES / PROGRESS WINDOW / CANCEL BUTTON / PREVIOUS PREVIEW / RETRY / EXPORT SAFETY / CLEANUP OK')
end
function onInitScene()
 init();started=mbm.getTimeRun();frames=0
 local button=tImGui.Button;tImGui.Button=function(label,...)
  local pressed=button(label,...)
  if cancelButton and label==tLang.L('ime_cancel') then cancelButton=false;return true end
  return pressed
 end
 local begin=tImGui.Begin;tImGui.Begin=function(label,...) if label==tLang.L('ime_generation_title') then sawPanel=true end;return begin(label,...) end
 task=coroutine.create(test)
end
function onLoop(delta)
 frames=frames+1;loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH ASYNC EDITOR FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>40 then print('IMAGE MESH ASYNC EDITOR FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
