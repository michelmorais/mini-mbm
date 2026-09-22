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
local task,started,cancelButton
local function waitTask(E) while E.meshTask do coroutine.yield() end end
local function waitSimplify(E) while not E.simplifyAsset do assert(E.meshTask,'simplification did not start');coroutine.yield() end end
local function test()
 local E=api.state;local pixels={};for i=1,65*65*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,65,65,3,'simplify_cancel_editor','/tmp/ime-simplify-cancel.png'))
 api.openImage('/tmp/ime-simplify-cancel.png');assert(api.action(function(p) Model.add(p,'rectangle',0,0,65,65) end));api.select(1,false)
 api.setEditMode(false);api.rebuild();waitTask(E);local previous=assert(E.preview)
 E.values.simplify=true;E.values.simplifyRatio=.7;assert(api.applyProperties());api.rebuild();waitSimplify(E)
 onKeyDown(mbm.getKeyCode('ESC'));waitTask(E)
 assert(E.generationCancelled and E.preview==previous and E.previewStale and not E.generationFailure)
 api.setEditMode(true);api.setEditMode(false);api.rebuild();waitTask(E);assert(E.preview~=previous and E.report.simplification,'retry failed')
 local output='/tmp/ime-simplify-cancel-export.msh';local f=assert(io.open(output,'wb'));f:write('keep');f:close()
 api.exportOne(output);waitSimplify(E);cancelButton=true;waitTask(E)
 assert(not cancelButton and E.generationCancelled)
 f=assert(io.open(output,'rb'));assert(f:read('*a')=='keep');f:close()
 api.setEditMode(true)
 assert(api.action(function(p) Model.add(p,'rectangle',0,0,33,33) end))
 local directory='/tmp/ime-simplify-cancel-batch'
 os.remove(directory..'/module_001.msh');os.remove(directory..'/module_002.msh')
 api.beginBatch(directory);api.batchStep();waitSimplify(E);api.generation.cancel(E);waitTask(E)
 assert(not E.batch,'cancel did not stop batch')
 assert(not io.open(directory..'/module_001.msh','rb') and not io.open(directory..'/module_002.msh','rb'))
 assert(not E.simplifyAsset and not E.simplifyProgress)
 print('IMAGE MESH SIMPLIFY CANCEL ESC / BUTTON / PREVIOUS PREVIEW / RETRY / EXPORT SAFETY / BATCH / CLEANUP OK')
end
function onInitScene()
 init();started=mbm.getTimeRun();task=coroutine.create(test)
 local button=tImGui.Button
 tImGui.Button=function(label,...)
  local pressed=button(label,...)
  if cancelButton and label==tLang.L('ime_cancel') then cancelButton=false;return true end
  return pressed
 end
end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH SIMPLIFY CANCEL FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>40 then print('IMAGE MESH SIMPLIFY CANCEL FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
