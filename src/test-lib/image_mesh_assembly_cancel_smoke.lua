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
local task,started
local function waitTask(E) while E.meshTask do coroutine.yield() end end
local function absent(path) local f=io.open(path,'rb');if f then f:close();return false end;return true end
local function test()
 local E=api.state;local pixels={};for i=1,64*32*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,64,32,3,'assembly_cancel','/tmp/ime-assembly-cancel.png'))
 api.openImage('/tmp/ime-assembly-cancel.png')
 assert(api.action(function(p)
  p.defaults.columns=12;p.defaults.rows=12
  Model.add(p,'rectangle',0,0,32,32);Model.add(p,'rectangle',32,0,32,32)
  p.regions[2].overrides.simplify=true;p.regions[2].overrides.simplifyRatio=.8
 end))
 api.select(2,false);api.setEditMode(false);api.setAssembly(true);api.rebuild();waitTask(E)
 local a=E.assembly;assert(#a.items==2 and a.builds==1)
 local old=a.items;local revision=a.revision;local fit=E.fitDistance
 E.orbit.distance=321;api.camera();api.setWireframe(true)
 assert(api.action(function(p) p.defaults.relief=3 end));api.rebuild()
 while not E.simplifyAsset do assert(E.meshTask);coroutine.yield() end
 assert(a.items==old and a.pending and #a.pending.items==2 and E.previewStale)
 for _,item in ipairs(old) do assert(item.wireObject.visible and not item.preview.visible) end
 local paths={};for _,item in ipairs(a.pending.items) do paths[#paths+1]=item.previewPath end
 onKeyDown(mbm.getKeyCode('ESC'));waitTask(E)
 assert(E.generationCancelled and not E.generationFailure and a.items==old and a.revision==revision)
 assert(E.previewStale and not E.report and not a.pending and a.builds==1)
 assert(E.fitDistance==fit and E.orbit.distance==321)
 for _,path in ipairs(paths) do assert(absent(path),'cancel leaked temporary file') end
 -- Fail the second generation after the first staged preview has been loaded.
 local start=mbm.startImageMesh;local count=0
 mbm.startImageMesh=function(...)
  count=count+1
  if count==2 then
   paths={};for _,item in ipairs(a.pending.items) do paths[#paths+1]=item.previewPath end
   return nil,'injected assembly failure'
  end
  return start(...)
 end
 api.setEditMode(true);api.setEditMode(false);api.rebuild();waitTask(E)
 mbm.startImageMesh=start
 assert(E.generationFailure and not E.generationCancelled and a.items==old and a.builds==1)
 assert(not a.pending and E.previewStale and E.orbit.distance==321)
 for _,path in ipairs(paths) do assert(absent(path),'failure leaked temporary file') end
 api.setEditMode(true);api.setEditMode(false);api.rebuild();waitTask(E)
 assert(a.items~=old and a.builds==2 and not E.previewStale and not E.generationFailure)
 assert(E.report and E.orbit.distance==321)
 for _,item in ipairs(old) do assert(absent(item.previewPath),'replacement leaked old file') end
 local builds,layouts=a.builds,a.layouts
 for i=1,8 do coroutine.yield() end
 assert(a.builds==builds and a.layouts==layouts,'idle rebuild')
 print('ASSEMBLY CANCEL / FAILURE / PREVIOUS WIREFRAME / CAMERA / RETRY / CLEANUP / IDLE OK')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('ASSEMBLY CANCEL FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>45 then print('ASSEMBLY CANCEL FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
