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
 assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
 assert(e.tSimplifyState.report and e.tSimplifyState.report.backend=='cgal',e.tSimplifyState.lastError)
 assert(e.meshDebug:getTotalIndex(1,1)==6)
 assert(e.meshDebug:getMaterialTexture(1,1,'normal')=='#8080FFFF')
 assert(e.meshDebug:getTexture(1,1)=='#6080FFFF')
 for _,v in ipairs(e.meshDebug:getVertex(1,1,1,e.meshDebug:getTotalVertex(1,1))) do assert(math.abs(v.u-v.x/8)<1e-6 and math.abs(v.v-v.y/8)<1e-6) end
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
 for _=1,5 do coroutine.yield() end
 print('CGAL EDITOR SMOKE OK: config/reduction/UV/material/subset/revert/cancel/failure')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 loop(delta)
 if tImGui.Begin('CGAL configuration smoke',false,0) then Cgal.panel() end
 tImGui.End()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,e=coroutine.resume(task);if not ok then print('CGAL EDITOR SMOKE FAIL '..debug.traceback(task,tostring(e)));mbm.quit() end
 if mbm.getTimeRun()-started>45 then print('CGAL EDITOR SMOKE FAIL timeout');mbm.quit() end
end
