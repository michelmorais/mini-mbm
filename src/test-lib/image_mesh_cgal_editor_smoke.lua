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
local Cgal=require 'mesh_cgal'
local init,loop=onInitScene,onLoop
local task,started
local function await() while api.state.meshTask do coroutine.yield() end end
local function test()
 assert(Cgal.setPath(assert(os.getenv('MBM_CGAL_EXECUTABLE')),false))
 local E=api.state
 assert(api.openProject(assert(os.getenv('MBM_CGAL_PROJECT'))));api.select(E.project.regions[1].id)
 E.values.simplify=true;E.values.simplifyMode='cgal';E.values.planarAngle=5;E.values.planarTolerance=.01
 assert(api.applyProperties());assert(api.saveProject('/tmp/cgal-integrated.imesh'))
 local saved=IO.load('/tmp/cgal-integrated.imesh');assert(Model.options(saved,saved.regions[1]).simplifyMode=='cgal')
 api.setEditMode(false);api.rebuild();await()
 assert(E.report and E.report.simplification and E.report.simplification.backend=='cgal',E.status)
 assert(E.report.sourceTriangles==12570 and E.report.triangles==6882,'unexpected triangle count '..tostring(E.report.triangles))
 local builds=E.builds;for _=1,5 do coroutine.yield() end;assert(E.builds==builds,'idle rebuilt')
 api.exportOne('/tmp/cgal-integrated.msh');await()
 local d=meshDebug:new();assert(d:load('/tmp/cgal-integrated.msh'));assert(d:check())
 for _,ratio in ipairs{.6,.4,.046} do
  E.values.simplifyMode='cgal_qem';E.values.simplifyRatio=ratio;E.values.simplifyBoundary=0
  assert(api.applyProperties());api.rebuild();await()
  local report=assert(E.report and E.report.simplification,E.status)
  assert(report.backend=='cgal_qem' and report.sourceTriangleCount==12570,E.status)
  assert(report.qemRan==(ratio<.6),'wrong QEM stage decision')
  assert(report.resultTriangleCount<=6882 and E.preview,'combined result missing')
  if ratio==.6 then assert(report.resultTriangleCount==6882,'ratio applied twice')
  else assert(report.resultTriangleCount==math.floor(12570*ratio),'original triangle target not reached') end
  print('COMBINED ratio '..ratio..': '..report.sourceTriangleCount..' -> '..report.resultTriangleCount)
 end
 -- The completed combined mesh must restore face normals after QEM.
 api.exportOne('/tmp/cgal-combined.msh');await()
 local final=meshDebug:new();assert(final:load('/tmp/cgal-combined.msh'));assert(final:check())
 for subset=1,final:getTotalSubset(1) do
  local vertices=final:getVertex(1,subset,1,final:getTotalVertex(1,subset))
  local indices=final:getIndex(1,subset)
  for i=1,#indices,3 do
   local a,b,c=vertices[indices[i]],vertices[indices[i+1]],vertices[indices[i+2]]
   local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
   local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
   local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
   local length=math.sqrt(nx*nx+ny*ny+nz*nz)
   for _,v in ipairs{a,b,c} do
    assert((nx*v.nx+ny*v.ny+nz*v.nz)/length>.999,'final face normals were not restored')
   end
  end
 end
 assert(api.saveProject('/tmp/cgal-combined.imesh'))
 local combined=IO.load('/tmp/cgal-combined.imesh')
 assert(Model.options(combined,combined.regions[1]).simplifyMode=='cgal_qem')
 -- QEM-only must not invoke the external worker, including after a combined preview.
 local start=Cgal.start
 Cgal.start=function() error('QEM-only invoked CGAL') end
 E.values.simplifyMode='qem'
 assert(api.applyProperties());api.rebuild();await()
 assert(E.report and E.report.triangles==578 and not E.report.simplification.cgal,E.status)
 Cgal.start=start
 E.values.simplify=false;E.values.simplifyMode='none'
 assert(api.applyProperties());api.rebuild();await()
 assert(E.report and not E.report.simplification and E.report.triangles==12570,E.status)
 assert(api.saveProject('/tmp/cgal-disabled.imesh'))
 local disabled=IO.load('/tmp/cgal-disabled.imesh')
 local options=Model.options(disabled,disabled.regions[1])
 assert(not options.simplify and options.simplifyMode=='none')
 assert(options.planarAngle==5 and options.simplifyRatio==.046,'disabled methods lost parameters')
 local remeshExecutable=os.getenv('MBM_CGAL_REMESH_EXECUTABLE')
 if remeshExecutable and remeshExecutable~='' then
  assert(Cgal.setRemeshPath(remeshExecutable,false))
  E.values.simplify=false;E.values.simplifyMode='none';E.values.remesh=true
  E.values.remeshEdgeLengthFraction=.05;E.values.remeshIterations=2;E.values.remeshFeatureAngle=45
  assert(api.applyProperties());api.rebuild();await()
  assert(E.status:find('self-intersections',1,true),'unsafe result was not rejected: '..tostring(E.status))
  -- Keep all source edges on this detailed relief to avoid smoothing across thin features.
  E.values.remeshFeatureAngle=0
  assert(api.applyProperties());api.rebuild();await()
  assert(E.report and E.report.remesh,E.status)
  assert(E.report.remesh.source_triangles==12570 and E.report.remesh.result_triangles>0)
  assert(E.report.triangles==E.report.remesh.result_triangles)
  local grid=dofile('src/test-lib/mesh_simplification_fixture.lua').grid('flat')
  local targetReport={triangles=128,vertices=81}
  require('image_mesh_simplify').apply({},grid,{remesh=true,remeshTargetEnabled=true,remeshTargetTriangles=500,
      remeshEdgeLengthFraction=.03,remeshIterations=3,remeshFeatureAngle=45,maxVertices=65535},targetReport)
  assert(targetReport.remesh.target_triangles==500 and targetReport.remesh.target_reached==1)
  assert(targetReport.triangles==grid:getTotalIndex(1,1)/3)
  E.values.remeshTargetEnabled=true;E.values.remeshTargetTriangles=3000
  assert(api.applyProperties());assert(api.saveProject('/tmp/cgal-target.imesh'))
  local targetProject=IO.load('/tmp/cgal-target.imesh')
  local targetOptions=Model.options(targetProject,targetProject.regions[1])
  assert(targetOptions.remeshTargetEnabled and targetOptions.remeshTargetTriangles==3000)

 end
 if os.getenv('MBM_CGAL_AUDIT_EXECUTABLE') then
  local AuditUI=require 'mesh_audit_ui'
  E.audit=E.audit or {}
  local path,builds=E.previewPath,E.builds
  assert(AuditUI.start(E.audit,path))
  while E.audit.job do coroutine.yield() end
  assert(E.audit.status.report and E.audit.status.report.triangles==E.report.triangles,E.audit.status.error)
  assert(E.previewPath==path and E.builds==builds,'audit rebuilt image mesh')
 end
 print('IMAGE CGAL EDITOR SMOKE OK: individual/combined/disabled/persistence')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,e=coroutine.resume(task);if not ok then print('IMAGE CGAL EDITOR SMOKE FAIL '..tostring(e));mbm.quit() end
 if mbm.getTimeRun()-started>60 then print('IMAGE CGAL EDITOR SMOKE FAIL timeout');mbm.quit() end
end
