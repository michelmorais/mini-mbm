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
local task,started
local function await() while api.state.meshTask do coroutine.yield() end end
local function test()
 local E=api.state
 local pixels={};for i=1,65*65*3 do pixels[i]=150 end
 local path='/tmp/ime-coplanar.png';assert(mbm.createTexture(pixels,65,65,3,'ime-coplanar',path))
 assert(api.openImage(path));assert(api.action(function(p) Model.add(p,'rectangle',0,0,65,65) end));api.select(1)
 E.values.columns=8;E.values.rows=8;E.values.relief=0;E.values.backOpen=true;E.values.simplify=true
 E.values.simplifyMode='coplanar';assert(api.applyProperties())
 api.setEditMode(false);api.rebuild();await()
 assert(E.report.simplification.planarRegions==1,E.status)
 assert(E.report.triangles==94 and not E.report.simplification.qemRan)
 local builds=E.builds;for i=1,5 do coroutine.yield() end;assert(E.builds==builds,'idle rebuilt')
 for _,mode in ipairs{'qem','coplanar_qem','coplanar'} do
  api.setEditMode(true);E.values.simplifyMode=mode;E.values.planarTolerance=1e-4;E.values.planarAngle=1;E.values.simplifyRatio=.5;assert(api.applyProperties())
  assert(api.saveProject('/tmp/ime-coplanar.imesh'));local saved=IO.load('/tmp/ime-coplanar.imesh')
  assert(Model.options(saved,saved.regions[1]).simplifyMode==mode)
  assert(Model.options(saved,saved.regions[1]).planarTolerance==1e-4)
  assert(Model.options(saved,saved.regions[1]).planarAngle==1)
  api.setEditMode(false);api.rebuild();await();assert(E.preview and E.report.simplification,E.status)
  assert(E.report.simplification.qemRan==(mode=='qem'))
  api.exportOne('/tmp/ime-coplanar-'..mode..'.msh');await()
  local restored=meshDebug:new();assert(restored:load('/tmp/ime-coplanar-'..mode..'.msh'))
  assert(restored:getTotalIndex(1,1)/3==E.report.triangles)
 end
 api.setEditMode(true);E.values.planarReduceBoundaries=true;assert(api.applyProperties())
 assert(api.saveProject('/tmp/ime-coplanar-boundary.imesh'))
 local boundaryProject=IO.load('/tmp/ime-coplanar-boundary.imesh')
 assert(Model.options(boundaryProject,boundaryProject.regions[1]).planarReduceBoundaries)
 api.setEditMode(false);api.rebuild();await()
 -- Open-back generation emits separate side quads, whose corners protect contacts.
 assert(E.report.simplification.planarBoundaryRemovedVertices==0 and E.report.triangles==94,E.status)
 -- The minimal flat-back generator has continuous planar side charts that qualify.
 api.setEditMode(true);E.values.backOpen=false;assert(api.applyProperties())
 api.setEditMode(false);api.rebuild();await()
 assert(E.report.simplification.planarBoundaryRemovedVertices>0,E.status)
 assert(E.report.triangles==12,'closed planar box '..E.report.triangles)
 print('IMAGE MESH COPLANAR BOUNDARY OK triangles='..E.report.triangles)
 api.exportOne('/tmp/ime-coplanar-boundary.msh');await()
 local boundaryReload=meshDebug:new();assert(boundaryReload:load('/tmp/ime-coplanar-boundary.msh'))
 assert(boundaryReload:getTotalIndex(1,1)/3==E.report.triangles)
 local boundaryBuilds=E.builds;for i=1,5 do coroutine.yield() end;assert(E.builds==boundaryBuilds)
 api.setEditMode(true)
 assert(api.action(function(p) p.regions[1].holes={{{x=.375,y=.375},{x=.625,y=.375},{x=.625,y=.625},{x=.375,y=.625}}} end))
 api.select(1);E.values.simplifyMode='coplanar';assert(api.applyProperties())
 api.setEditMode(false);api.rebuild();await()
 assert(E.report.simplification.planarRegions>0 and E.report.simplification.planarRemovedTriangles>0,E.status)
 assert(not E.report.simplification.qemRan)
 assert(api.saveProject('/tmp/ime-coplanar-hole.imesh'))
 local savedHole=IO.load('/tmp/ime-coplanar-hole.imesh');assert(#savedHole.regions[1].holes==1)
 api.exportOne('/tmp/ime-coplanar-hole.msh');await()
 local restoredHole=meshDebug:new();assert(restoredHole:load('/tmp/ime-coplanar-hole.msh'))
 assert(restoredHole:getTotalIndex(1,1)/3==E.report.triangles)
 print('IMAGE MESH COPLANAR HOLE OK removed='..E.report.simplification.planarRemovedTriangles)
 api.setEditMode(true);assert(api.action(function(p) p.regions[1].holes={} end));api.select(1)
 local legacy=Model.copy(E.project);legacy.defaults.simplifyMode=nil;legacy.regions[1].overrides.simplifyMode=nil
 legacy.defaults.planarAngle=nil;legacy.regions[1].overrides.planarAngle=nil
 legacy.defaults.planarReduceBoundaries=nil;legacy.regions[1].overrides.planarReduceBoundaries=nil
 legacy.defaults.planarTolerance=nil;legacy.regions[1].overrides.planarTolerance=nil
 assert(Model.options(legacy,legacy.regions[1]).simplifyMode=='qem')
 assert(Model.options(legacy,legacy.regions[1]).planarTolerance==1e-7)
 assert(Model.options(legacy,legacy.regions[1]).planarAngle==.05)
 assert(Model.options(legacy,legacy.regions[1]).planarReduceBoundaries==false)
 local node={{x=.2,y=.2},{x=.8,y=.2},{x=.8,y=.8},{x=.2,y=.8},parent=0,role='target',thickness=8}
 local o={heightSource='curved',columns=16,rows=16,width=100,height=100,curvedEdge=1,curvedTarget=8,
  curvedNodes={node},heightTolerance=.015,curvedSimplify=true,curvedSimplifyRatio=.5,curvedSimplifyError=.01}
 for _,mode in ipairs{'specific','coplanar_specific','coplanar'} do
  o.curvedSimplifyMode=mode
  local d,r=mbm.generateImageMesh(path,o);assert(d,r);assert(d:check())
  print('CURVED MODE '..mode..' '..r.curvedSourceTriangles..' -> '..r.curvedResultTriangles..' planar '..r.curvedPlanarRemovedTriangles)
  assert(r.curvedMaximumError<=o.curvedSimplifyError)
  if mode~='specific' then assert(r.curvedPlanarRemovedTriangles>0,'curved planar made no progress') end
  if mode=='coplanar' then assert(r.curvedMaximumError==0) end
 end
 api.setEditMode(true);E.values.heightSource='curved';E.values.curvedSimplify=true
 E.values.curvedSimplifyMode='coplanar';assert(api.applyProperties());assert(api.saveProject('/tmp/ime-coplanar-curved.imesh'))
 local saved=IO.load('/tmp/ime-coplanar-curved.imesh');assert(Model.options(saved,saved.regions[1]).curvedSimplifyMode=='coplanar')
 api.setEditMode(false);api.rebuild();await();assert(not E.report.simplification,'QEM was called for curved relief')
 print('IMAGE MESH COPLANAR MODES / PREVIEW / SAVE / EXPORT / LEGACY / CURVED / IDLE OK')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,e=coroutine.resume(task)
 if not ok then print('IMAGE MESH COPLANAR FAIL '..tostring(e));mbm.quit() end
 if mbm.getTimeRun()-started>45 then print('IMAGE MESH COPLANAR FAIL timeout');mbm.quit() end
end
