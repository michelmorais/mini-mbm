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
local Assembly=require 'image_mesh_assembly'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local task,started,calls
local native=mbm.startImageMesh
mbm.startImageMesh=function(...) calls=(calls or 0)+1;return native(...) end
local function wait(E) while E.meshTask do coroutine.yield() end end
local function rebuild(E) api.rebuild();wait(E) end
local function test()
 local E=api.state;local pixels={};for i=1,32*32*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,32,32,3,'assembly_instances','/tmp/ime-assembly-instances.png'))
 api.openImage('/tmp/ime-assembly-instances.png')
 assert(api.action(function(p)
  p.defaults.width=40;p.defaults.columns=4;p.defaults.rows=4
  Model.add(p,'rectangle',0,0,32,32)
 end))
 api.select(1,false);api.setEditMode(false);api.setAssembly(true);rebuild(E)
 local a=E.assembly;assert(#a.items==1)
 local before=calls
 assert(Assembly.add(E,1));local copyId=a.selectedSlot;rebuild(E)
 assert(#a.items==2 and #E.project.regions==1,'copy changed project modules')
 assert(calls==before+1,'duplicate regenerated the same module more than once')
 assert(a.items[1].id==1 and a.items[2].id==1)
 assert(a.items[1].preview~=a.items[2].preview and a.items[1].previewPath~=a.items[2].previewPath)
 assert(math.abs(a.items[2].x-a.items[1].x-40)<.001,'copies not side by side')
 a.slots[copyId].z=5;Assembly.layout(E)
 assert(math.abs(a.items[2].z-a.items[1].z-5)<.001)
 api.setWireframe(true);a.slots[copyId].visible=false;Assembly.sync(E)
 assert(a.items[1].wireObject.visible and not a.items[2].wireObject.visible,'copy visibility coupled')
 a.slots[copyId].visible=true;api.setWireframe(false)
 assert(api.action(function(p) local r=Model.add(p,'rectangle',0,0,32,32);r.overrides.width=20 end))
 assert(Assembly.choose(E,copyId,2));rebuild(E)
 assert(a.items[1].id==1 and a.items[2].id==2 and math.abs(a.items[2].width-20)<.001)
 local builds,layouts,generated=a.builds,a.layouts,calls
 for i=1,12 do coroutine.yield() end
 assert(a.builds==builds and a.layouts==layouts and calls==generated,'idle regenerated instances')
 assert(api.action(function(p) table.remove(p.regions,2) end));rebuild(E)
 assert(#a.items==1 and #a.order==1,'deleted module left a dangling preview object')
 assert(Assembly.remove(E,a.order[1]));rebuild(E);assert(#a.items==0)
 assert(Assembly.add(E,1));rebuild(E);assert(#a.items==1)
 api.setAssembly(false);rebuild(E);assert(E.preview)
 print('ASSEMBLY INSTANCES / SINGLE MODULE / SOURCE / SHARED GENERATION / POSITION / WIRE / DELETE / IDLE OK')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 local header=tImGui.CollapsingHeader
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_assembly_title') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 loop(delta);tImGui.CollapsingHeader=header
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('ASSEMBLY INSTANCES FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>35 then print('ASSEMBLY INSTANCES FAIL timeout');mbm.quit() end
end
function onEndScene() mbm.startImageMesh=native;finish() end
