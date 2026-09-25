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
dofile('editor/mesh_debug.lua')
local Cgal=require 'mesh_cgal'
local helper=dofile('src/test-lib/mesh_simplification_fixture.lua')
local init,loop=onInitScene,onLoop
local task,started
local openGeometryNodes={}
local function await(entry)
 while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
end
local function test()
 local executable=assert(os.getenv('MBM_CGAL_EXECUTABLE'))
 assert(Cgal.setPath(executable,true))
 package.loaded.mesh_cgal=nil;Cgal=require 'mesh_cgal';assert(Cgal.getPath()==executable)
 local d=helper.grid('flat');d:setTexture(1,1,'#6080FFFF');d:setMaterialTexture(1,1,'normal','#8080FFFF')
 assert(d:save('/tmp/cgal-editor-source.msh',false,false,true));assert(addMeshToTable('/tmp/cgal-editor-source.msh'))
 local e=tLoadedMeshes[#tLoadedMeshes];iSelectedMeshIndex=#tLoadedMeshes
 local original=helper.signature(e.meshDebug)
 for _,method in ipairs{'simplify','startSimplify'} do
  local ok=pcall(function() return e.meshDebug[method](e.meshDebug,.5,nil,1,true,0,'coplanar') end)
  assert(not ok,'native API accepted a removed algorithm')
 end
 assert(helper.signature(e.meshDebug)==original)
 e.sOpenNode='simplification';e.tSimplifyState={scope='frame',selectedFrame=1,ratio=.5,preserveDetails=true,
  boundaryCollapseThreshold=0,mode='cgal',planarAngle=5,planarTolerance=.01}
 -- Switching tree panels retains the chosen simplification method and never runs work.
 coroutine.yield()
 e.sOpenNode='remesh';coroutine.yield()
 assert(e.tSimplifyState.mode=='remesh' and not e.tSimplifyState.running)
 e.sOpenNode='simplification';coroutine.yield()
 assert(e.tSimplifyState.mode=='cgal' and helper.signature(e.meshDebug)==original)
 for _,node in ipairs{'audit','remesh','simplification','audit'} do
  e.sOpenNode=node;coroutine.yield()
  assert(#openGeometryNodes==1 and openGeometryNodes[1]==node,'geometry trees must be exclusive')
 end
 e.sOpenNode='simplification';coroutine.yield()

 assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
 assert(e.tSimplifyState.report and e.tSimplifyState.report.backend=='cgal',e.tSimplifyState.lastError)
 assert(e.meshDebug:getTotalIndex(1,1)==6)
 assert(e.meshDebug:getMaterialTexture(1,1,'normal')=='#8080FFFF')
 assert(e.meshDebug:getTexture(1,1)=='#6080FFFF')
 for _,v in ipairs(e.meshDebug:getVertex(1,1,1,e.meshDebug:getTotalVertex(1,1))) do assert(math.abs(v.u-v.x/8)<1e-6 and math.abs(v.v-v.y/8)<1e-6) end
 assert(simplifyRestoreBackup(e,iSelectedMeshIndex));assert(helper.signature(e.meshDebug)==original)
 e.tSimplifyState.mode='none'
 assert(not simplifyApply(e,e.meshDebug,iSelectedMeshIndex));assert(helper.signature(e.meshDebug)==original)
 e.tSimplifyState.mode='cgal_qem'
 assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
 assert(e.tSimplifyState.report.backend=='cgal_qem' and not e.tSimplifyState.report.qemRan)
 assert(e.meshDebug:getTotalIndex(1,1)==6)
 assert(simplifyRestoreBackup(e,iSelectedMeshIndex));assert(helper.signature(e.meshDebug)==original)
 assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));assert(simplifyCancel(e));await(e)
 assert(helper.signature(e.meshDebug)==original)
 Cgal.setPath('/tmp/missing-cgal-executable',false)
 assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e);assert(helper.signature(e.meshDebug)==original)
 assert(e.tSimplifyState.lastError)
 Cgal.setPath(executable,false)
 local bad=helper.grid('flat');local frozen=helper.signature(bad)
 local failed=assert(Cgal.start(bad,nil,1,61,.01));local failedStatus
 repeat failedStatus=failed:getSimplifyStatus();coroutine.yield() until failedStatus.state~='running'
 assert(failedStatus.state=='failed' and failedStatus.error:find('angle range',1,true),failedStatus.error)
 assert(helper.signature(bad)==frozen)
 for _,file in ipairs(failed.files) do assert(not io.open(file,'rb'),'temporary file leaked') end
 local cancelled=assert(Cgal.start(bad,nil,1,5,.01));cancelled:cancelSimplify()
 repeat failedStatus=cancelled:getSimplifyStatus();coroutine.yield() until failedStatus.state~='running'
 assert(failedStatus.state=='cancelled' and helper.signature(bad)==frozen)
 for _,file in ipairs(cancelled.files) do assert(not io.open(file,'rb'),'cancel leaked file') end
 local limited=assert(Cgal.start(bad,nil,1,5,.01,3))
 repeat failedStatus=limited:getSimplifyStatus();coroutine.yield() until failedStatus.state~='running'
 assert(failedStatus.state=='failed' and helper.signature(bad)==frozen,'vertex budget must reject before mutation')
 local bare=helper.grid('flat');bare:removeNormals()
 local bareJob=assert(Cgal.start(bare,nil,1,5,.01,nil,false))
 repeat failedStatus=bareJob:getSimplifyStatus();coroutine.yield() until failedStatus.state~='running'
 assert(failedStatus.state=='completed',failedStatus.error);assert(bare:save('/tmp/cgal-bare.msh',false,false,true));assert(not meshDebug:getInfo('/tmp/cgal-bare.msh').hasNormal,'normals unexpectedly introduced')
 local multi=helper.grid('flat');multi:copyFrameFrom(multi,1)
 local rejected=Cgal.start(multi,nil,1,5,.01);assert(not rejected)
 -- Subset isolation keeps untouched geometry, order and its material.
 local two=helper.grid('flat');two:copySubsetFrom(1,d,1,1);two:setTexture(1,2,'#FF0000FF')
 local job=assert(Cgal.start(two,2,1,5,.01));local status
 repeat status=job:getSimplifyStatus();coroutine.yield() until status.state~='running'
 assert(status.state=='completed',status.error);assert(two:getTotalIndex(1,1)==384 and two:getTotalIndex(1,2)==6)
 assert(two:getTexture(1,2)=='#FF0000FF')
 -- The combined target uses only the selected subset, and can cancel in QEM.
 local Pipeline=require 'mesh_simplify_pipeline'
 for _,cancel in ipairs{false,true} do
  local scoped=helper.grid('flat');local bent=helper.grid('bent')
  scoped:copySubsetFrom(1,bent,1,1)
  local combined=assert(Pipeline.start(scoped,'cgal_qem',.05,2,1,false,.25,0,0,nil,true))
  local result
  repeat
   result=combined:getSimplifyStatus()
   if cancel and combined.stage=='qem' and result.state=='running' then combined:cancelSimplify() end
   coroutine.yield()
  until result.state~='running'
  assert(scoped:getTotalIndex(1,1)==384,'combined modified an unselected subset')
  if cancel then assert(result.state=='cancelled',result.error)
  else
   assert(result.state=='completed',result.error)
   assert(result.report.qemRan and result.report.sourceTriangleCount==256)
   assert(scoped:getTotalIndex(1,2)/3<=6,'combined target used whole frame or intermediate count')
  end
 end
 -- The shared intermediate mesh fits, but final face normals can exceed the budget.
 local budgetMesh=helper.grid('bent')
 local budgetJob=assert(Pipeline.start(budgetMesh,'cgal_qem',.9,nil,1,true,0,0,0,40,true))
 local budgetStatus
 repeat budgetStatus=budgetJob:getSimplifyStatus();coroutine.yield() until budgetStatus.state~='running'
 assert(budgetMesh:getTotalIndex(1,1)/3<128,'budget test did not reach final reconstruction')
 assert(budgetStatus.state=='failed' and budgetStatus.error:find('vertex limit',1,true),
  'final normal reconstruction did not enforce the vertex budget')
 local remeshExecutable=os.getenv('MBM_CGAL_REMESH_EXECUTABLE')
 if remeshExecutable and remeshExecutable~='' then
  assert(Cgal.setRemeshPath(remeshExecutable,false))
  e.sOpenNode='remesh';e.tSimplifyState.mode='remesh';e.tSimplifyState.remeshEdgeLengthFraction=.06
  e.tSimplifyState.remeshIterations=2;e.tSimplifyState.remeshFeatureAngle=45
  assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
  assert(e.tSimplifyState.report and e.tSimplifyState.report.backend=='remesh',e.tSimplifyState.lastError)
  assert(e.tSimplifyState.report.resultTriangleCount>e.tSimplifyState.report.sourceTriangleCount)
  assert(simplifyRestoreBackup(e,iSelectedMeshIndex))
  for _,target in ipairs{80,500} do
   e.tSimplifyState.remeshTargetEnabled=true;e.tSimplifyState.remeshTargetTriangles=target
   assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
   local report=assert(e.tSimplifyState.report,e.tSimplifyState.lastError).remesh
   assert(report.target_triangles==target)
   local actual=e.meshDebug:getTotalIndex(1,1)/3
   local errorFraction=math.abs(actual-target)/target
   assert(math.abs(report.target_relative_error-errorFraction)<1e-9)
   assert(report.target_reached==(errorFraction<=.05 and 1 or 0))
   if target==500 then assert(errorFraction<=.05) else assert(actual<128) end
   assert(simplifyRestoreBackup(e,iSelectedMeshIndex));assert(helper.signature(e.meshDebug)==original)
  end
  e.tSimplifyState.remeshTargetEnabled=false
  assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)

  assert(simplifyRestoreBackup(e,iSelectedMeshIndex));assert(helper.signature(e.meshDebug)==original)
  assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));assert(simplifyCancel(e));await(e)
  assert(helper.signature(e.meshDebug)==original)

  local scopedTarget=helper.grid('flat');scopedTarget:copySubsetFrom(1,scopedTarget,1,1)
  assert(scopedTarget:save('/tmp/remesh-subset-target.msh',false,false,true))
  assert(addMeshToTable('/tmp/remesh-subset-target.msh'))
  local scopedEntry=tLoadedMeshes[#tLoadedMeshes]
  scopedEntry.tSimplifyState={scope='subsets',selectedFrame=1,selectedSubsets={[1]=true,[2]=true},ratio=.5,
      mode='remesh',remeshTargetEnabled=true,remeshTargetTriangles=500,
      remeshEdgeLengthFraction=.03,remeshIterations=3,remeshFeatureAngle=45}
  assert(simplifyApply(scopedEntry,scopedEntry.meshDebug,#tLoadedMeshes));await(scopedEntry)
  local scopedReport=assert(scopedEntry.tSimplifyState.report,scopedEntry.tSimplifyState.lastError).remesh
  assert(scopedReport.target_triangles==500 and scopedReport.target_reached==1)
  assert(scopedReport.target_result_triangles==scopedEntry.meshDebug:getTotalIndex(1,1)/3+scopedEntry.meshDebug:getTotalIndex(1,2)/3)
  assert(simplifyRestoreBackup(scopedEntry,#tLoadedMeshes))
  local remeshMesh=helper.grid('flat');remeshMesh:setTexture(1,1,'#6080FFFF')
  remeshMesh:setMaterialTexture(1,1,'normal','#8080FFFF')
    remeshMesh:setPhysics({{type='cube',center={x=0,y=0,z=0},half={x=.5,y=.5,z=.1}}})
  local remesh=assert(Cgal.startRemesh(remeshMesh,nil,1,.15,2,45,65535,true))
  local remeshStatus
  repeat remeshStatus=remesh:getSimplifyStatus();coroutine.yield() until remeshStatus.state~='running'
  assert(remeshStatus.state=='completed',remeshStatus.error)
  assert(remeshStatus.report.backend=='remesh' and remeshStatus.report.remesh)
  assert(remeshMesh:check() and remeshMesh:getTotalIndex(1,1)>0)
  assert(remeshMesh:getTexture(1,1)=='#6080FFFF')
  assert(remeshMesh:getMaterialTexture(1,1,'normal')=='#8080FFFF')
    local physics=remeshMesh:getPhysics();assert(#physics==1 and physics[1].type=='cube')
 end
 if os.getenv('MBM_CGAL_AUDIT_EXECUTABLE') then
  local AuditUI=require 'mesh_audit_ui'
  e.audit=e.audit or {}
  local snapshot=helper.signature(e.meshDebug)
  assert(AuditUI.start(e.audit,e.meshDebug))
  while e.audit.job do coroutine.yield() end
  assert(e.audit.status.report and e.audit.status.report.triangles==128,e.audit.status.error)
  assert(snapshot==helper.signature(e.meshDebug),'audit mutated Mesh Debug asset')
 end
 for _=1,5 do coroutine.yield() end
 print('CGAL EDITOR SMOKE OK: config/reduction/UV/material/subset/revert/cancel/failure')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 openGeometryNodes={}
 local tree=tImGui.TreeNodeEx
 tImGui.TreeNodeEx=function(...)
  local label,flags,id=...
  local opened=tree(...)
  if opened and id then
   if id:match('^audit%-mesh%-debug%-') then openGeometryNodes[#openGeometryNodes+1]='audit'
   elseif id:match('^remesh%-') then openGeometryNodes[#openGeometryNodes+1]='remesh'
   elseif id:match('^simplification%-') then openGeometryNodes[#openGeometryNodes+1]='simplification' end
  end
  return opened
 end
 loop(delta)
 tImGui.TreeNodeEx=tree
 if tImGui.Begin('CGAL configuration smoke',false,0) then
  local seen={}
  local button,input=tImGui.Button,tImGui.InputText
  local function check(label)
   assert(not seen[label],'Duplicate CGAL configuration ID: '..label)
   seen[label]=true
  end
  tImGui.Button=function(label,...) check(label);return button(label,...) end
  tImGui.InputText=function(label,...) check(label);return input(label,...) end
  local ok,err=pcall(Cgal.panel)
  tImGui.Button,tImGui.InputText=button,input
  if not ok then print('CGAL EDITOR SMOKE FAIL '..tostring(err));mbm.quit() end
 end
 tImGui.End()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,e=coroutine.resume(task);if not ok then print('CGAL EDITOR SMOKE FAIL '..debug.traceback(task,tostring(e)));mbm.quit() end
 if mbm.getTimeRun()-started>45 then print('CGAL EDITOR SMOKE FAIL timeout');mbm.quit() end
end
