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
local A=require 'mesh_audit'
local UI=require 'mesh_audit_ui'
local fixture=dofile('src/test-lib/mesh_simplification_fixture.lua')
tImGui=require 'ImGui'
local Lang=require 'lang.language'
local task,started
local uiState={}
local function await(job)
 local status
 repeat status=job:getStatus();coroutine.yield() until status.state~='running'
 return status
end
local function test()
 A.setPath(assert(os.getenv('MBM_CGAL_AUDIT_EXECUTABLE')),false)
 local asset=fixture.grid('flat')
 local before=fixture.signature(asset)
 local job=assert(A.startMesh(asset));local report=assert(await(job).report)
 assert(report.triangles==128 and report.surface_area==64 and report.boundary_edges==32)
 assert(report.valid_polygon_mesh and not report.has_self_intersections)
 assert(report.self_intersections_status=='completed')
 assert(before==fixture.signature(asset),'audit modified source')
 for _,p in ipairs(job.files) do assert(not io.open(p),'temporary file leaked') end
 assert(A.decodeReport(job.json).triangles==128)
 local skip=assert(A.startMesh(asset,{selfIntersections=false}));report=assert(await(skip).report)
 assert(report.self_intersections_status=='not_requested' and report.has_self_intersections==nil)
 local cancel=assert(A.startMesh(asset));cancel:cancel()
 assert(cancel:getStatus().state=='cancelled' and fixture.signature(asset)==before)
 local failed=assert(A.startFile('/tmp/audit-nonexistent-fixture.obj'));local status=await(failed)
 assert(status.state=='failed' and status.error:find('cannot read',1,true),status.error)
 assert(UI.start(uiState,asset))
 while uiState.job do UI.update();coroutine.yield() end
 assert(uiState.status.report.triangles==128)
 for _=1,5 do coroutine.yield() end
 assert(fixture.signature(asset)==before)
 print('MESH AUDIT SMOKE OK: read-only snapshot / JSON / skip / cancel / failure / UI')
end
function onInitScene()started=mbm.getTimeRun();task=coroutine.create(test)end
function onLoop(delta)
 UI.update()
 if tImGui.Begin('Audit smoke',false,0) then
  tImGui.SetNextItemOpen(true,tImGui.Flags('ImGuiCond_Always'))
  UI.draw(uiState,uiState.source,'smoke',tImGui,Lang.L,false)
 end
 tImGui.End()
 local ok,err=coroutine.resume(task)
 if not ok then print('MESH AUDIT SMOKE FAIL '..tostring(err)) end
 if not ok or coroutine.status(task)=='dead' or mbm.getTimeRun()-started>20 then
  if mbm.getTimeRun()-started>20 then print('MESH AUDIT SMOKE FAIL timeout') end
  UI.shutdown();A.shutdown();mbm.quit()
 end
end
function onEndScene() UI.shutdown();A.shutdown() end
