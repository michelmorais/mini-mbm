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
local Preview=require 'image_mesh_height_preview'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local task,started,frames= nil,nil,0
local cancelRequested=false
local function settle(E)
 while E.heightJob or E.heightRequested do coroutine.yield() end
 assert(not E.heightError,E.heightError)
end
local function test()
 local E=api.state;local pixels={};for i=1,129*129*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,129,129,3,'map_editor_async','/tmp/ime-map-editor-source.png'))
 api.openImage('/tmp/ime-map-editor-source.png')
 assert(api.action(function(p) Model.add(p,'rectangle',0,0,129,129) end));api.select(1,false)
 E.heightView=2;api.updateHeightPreview();assert(E.heightJob and not E.heightObject)
 local frame=frames;settle(E);assert(frames>frame and E.heightObject)
 local old,path,builds=E.heightObject,E.heightPath,E.heightBuilds
 E.values.heightCurve=2;assert(api.applyProperties());assert(E.heightObject==old)
 api.updateHeightPreview();local obsolete=E.heightJob.path
 E.values.heightCurve=.5;assert(api.applyProperties());api.updateHeightPreview()
 assert(E.heightObject==old and E.heightJob)
 settle(E);assert(E.heightObject~=old and E.heightBuilds==builds+1)
 assert(not IO.exists(obsolete) and not IO.exists(path),'old map file leaked')
 local expected='/tmp/ime-map-editor-expected.png'
 assert(mbm.generateImageMeshMap(E.project.image.path,Model.options(E.project,E.project.regions[1]),expected))
 assert(mbm.readImagePixels(expected)==mbm.readImagePixels(E.heightPath),'stale map installed')
 old=E.heightObject
 E.values.heightImage='/tmp/missing-height-map-editor.png';assert(api.applyProperties());api.updateHeightPreview()
 while E.heightJob do coroutine.yield() end
 assert(E.heightError and E.heightObject==old and E.heightStale)
 E.values.heightImage='';assert(api.applyProperties());settle(E)
 old=E.heightObject;E.heightRequested=true;api.updateHeightPreview();local cancelled=E.heightJob.path
 cancelRequested=true;coroutine.yield();assert(not cancelRequested)
 while E.heightJob do coroutine.yield() end
 assert(E.heightObject==old and not IO.exists(cancelled))
 builds=E.heightBuilds;for i=1,5 do coroutine.yield() end;assert(E.heightBuilds==builds,'idle map rebuild')
 E.heightView=3;api.updateHeightPreview();settle(E);assert(E.heightBuilds==builds+1)
 E.heightRequested=true;api.updateHeightPreview();local replaced=E.heightJob.path
 api.openImage('/tmp/ime-map-editor-source.png');api.updateHeightPreview()
 while E.heightJob do coroutine.yield() end
 assert(not E.heightObject and not IO.exists(replaced),'project replacement kept old output')
 assert(api.action(function(p) Model.add(p,'rectangle',0,0,129,129) end));api.select(1,false);E.heightView=2
 E.heightRequested=true;api.updateHeightPreview();local closing=E.heightJob.path
 Preview.shutdown(E);assert(not E.heightJob and not E.heightObject and not IO.exists(closing))
 E.heightView=1
 print('IMAGE MESH MAP EDITOR FRAMES / PREVIOUS PREVIEW / STALE DISCARD / ERROR / RETRY / CANCEL / IDLE / OVERLAY / CLEANUP OK')
end
function onInitScene()
 init();started=mbm.getTimeRun();task=coroutine.create(test)
 local header=tImGui.CollapsingHeader
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_grooves_group') then tImGui.SetNextItemOpen(true) end
  return header(label,...)
 end
 local button=tImGui.Button
 tImGui.Button=function(label,...)
  local pressed=button(label,...)
  if cancelRequested and label==tLang.L('ime_cancel')..'##height_preview' then cancelRequested=false;return true end
  return pressed
 end
end
function onLoop(delta)
 loop(delta);frames=frames+1
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH MAP EDITOR FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>40 then print('IMAGE MESH MAP EDITOR FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
