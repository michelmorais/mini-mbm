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
local Asset=require 'image_mesh_asset'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local task,started,calls
local function waitTask() while api.state.meshTask do coroutine.yield() end end
local function equal(a,b)
 assert(#a==#b)
 for i,p in ipairs(a) do for _,k in ipairs{'x','y','z','nx','ny','nz','u','v'} do
  assert(math.abs(p[k]-b[i][k])<1e-6,'cached geometry changed: '..k)
 end end
end
local function test()
 local E=api.state
 local pixels={};for y=0,64 do for x=0,64 do for c=1,3 do pixels[#pixels+1]=80+x+y end end end
 assert(mbm.createTexture(pixels,65,65,3,'cache_source','/tmp/ime-cache-source.png'))
 api.openImage('/tmp/ime-cache-source.png')
 assert(api.action(function(p) local r=Model.add(p,'rectangle',0,0,65,65);r.overrides.columns=12;r.overrides.rows=12 end))
 api.select(1,false)
 for _,simplify in ipairs{false,true} do
  api.setEditMode(true);E.values.simplify=simplify;E.values.simplifyRatio=.7;assert(api.applyProperties())
  local before=calls
  api.updateStatistics();waitTask()
  assert(calls==before+1 and E.generatedMesh and E.generatedMesh.asset)
  local vertices=Asset.vertices(E.generatedMesh.asset)
  local original=E.generatedMesh.originalPath
  assert((original~=nil)==simplify)
  local expected=E.report.triangles
  local source=E.report.sourceTriangles
  api.setEditMode(false);api.rebuild();waitTask()
  assert(calls==before+1,'3D regenerated statistics mesh')
  assert(E.preview and E.report.triangles==expected and not E.generatedMesh,E.status)
  local saved=meshDebug:new();assert(saved:load(E.previewPath));equal(vertices,Asset.vertices(saved))
  if simplify then
   assert(E.comparison and not IO.exists(original),'original cache leak')
   local raw=meshDebug:new();assert(raw:load(E.comparison.previewPath));assert(#raw:getIndex(1,1)/3==source)
   api.setComparison(true);assert(E.compareSideBySide)
  end
  api.setWireframe(true);assert(E.wireObject);api.setWireframe(false)
  local camera=Model.copy(E.orbit)
  api.setEditMode(true);api.setEditMode(false);api.rebuild();waitTask()
  assert(calls==before+1,'mode toggle regenerated mesh')
  assert(E.orbit.distance==camera.distance and E.orbit.azimuth==camera.azimuth)
  api.setEditMode(true)
  E.values.relief=E.values.relief+1;assert(api.applyProperties());assert(not E.generatedMesh)
  api.updateStatistics();waitTask();assert(calls==before+2)
  original=E.generatedMesh.originalPath
  api.undo(false);assert(not E.generatedMesh and (not original or not IO.exists(original)))
  api.updateStatistics();waitTask();assert(calls==before+3)
  local cache=E.generatedMesh
  api.exportOne('/tmp/ime-cache-export.msh',true);waitTask()
  assert(calls==before+4 and E.generatedMesh==cache,'export shared mutable cache')
  equal(Asset.vertices(cache.asset),vertices)
 end
 -- A cache from another module is never used, even when its report is retained.
 api.setEditMode(true)
 assert(api.action(function(p) local r=Model.add(p,'rectangle',0,0,32,32);r.overrides.columns=4;r.overrides.rows=4 end))
 api.select(1,false);api.updateStatistics();waitTask()
 local old=E.generatedMesh.originalPath
 api.select(2,false);api.updateStatistics();waitTask()
 assert(E.generatedMesh.id==2 and (not old or not IO.exists(old)))
 api.select(1,false);local before=calls
 api.setEditMode(false);api.rebuild();waitTask();assert(calls==before+1)
 api.setEditMode(true)
 assert(api.action(function(p) p.regions[1].overrides.maxVertices=1 end))
 api.updateStatistics();waitTask();assert(E.generationFailure and not E.generatedMesh)
 api.openImage('/tmp/ime-cache-source.png');assert(not E.generatedMesh)
 print('IMAGE MESH CACHE REUSE / COMPARISON / GEOMETRY / CAMERA / HISTORY / EXPORT ISOLATION / EVICTION / FAILURE OK')
end
function onInitScene()
 init();started=mbm.getTimeRun();calls=0
 local generate=mbm.startImageMesh
 mbm.startImageMesh=function(...) calls=calls+1;return generate(...) end
 task=coroutine.create(test)
end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH CACHE FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>35 then print('IMAGE MESH CACHE FAIL timeout');mbm.quit() end
end
function onEndScene() finish();assert(not api.state.generatedMesh) end
