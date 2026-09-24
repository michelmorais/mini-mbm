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
local helper={};assert(loadfile('src/test-lib/mesh_coplanar_smoke.lua'))(helper)
local init=onInitScene
local task,started
local function wait(d)
 local previous=0
 while true do local s=d:getSimplifyStatus();assert(s.progress>=previous,'progress regressed');previous=s.progress
  if s.state~='running' then return s end;coroutine.yield() end
end
local function test()
 for _,mode in ipairs{'coplanar','coplanar_qem'} do
  local d=helper.grid('hole');local before=helper.signature(d)
  assert(d:startSimplify(.5,nil,nil,true,0,mode,.01,5,true));assert(d:cancelSimplify())
  local status=wait(d);assert(status.state=='cancelled',status.error)
  assert(helper.signature(d)==before,'native cancellation changed source')
  assert(d:startSimplify(.5,nil,nil,true,0,mode,.01,5,true));status=wait(d)
  assert(status.state=='completed' and status.progress==1,status.error)
  assert(status.report.resultTriangleCount==8 and not d:cancelSimplify())
 end
 local d=helper.grid('hole')
 local entry={meshDebug=d,info={type='mesh'},modified=false,tPendingOps={},
  tSimplifyState={scope='frame',selectedFrame=1,ratio=.5,preserveDetails=true,boundaryCollapseThreshold=0,mode='coplanar',planarTolerance=.01,planarAngle=5,planarReduceBoundaries=true}}
 assert(simplifyApply(entry,d,0));assert(simplifyCancel(entry))
 while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
 assert(entry.meshDebug==d and not entry.modified and not entry.tSimplifyBackup)
 assert(simplifyApply(entry,d,0));while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
 assert(entry.tSimplifyState.report.planarRegions==1 and entry.tSimplifyState.report.planarBoundaryRemovedVertices>0 and entry.meshDebug~=d)
 local reduced,backup=entry.meshDebug,entry.tSimplifyBackup
 assert(simplifyApply(entry,reduced,0));while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
 assert(entry.tSimplifyState.report.unchanged and entry.meshDebug==reduced and entry.tSimplifyBackup==backup)
 tImGui.Begin('Coplanar report',false,0);showSimplifyGeometry(entry,reduced,0,1,{});tImGui.End()
 assert(simplifyRestoreBackup(entry,0));assert(entry.meshDebug:getTotalIndex(1,1)==360 and not entry.modified)
 print('COPLANAR CANCEL / RETRY / PROGRESS / LATE CANCEL / EDITOR NOOP UNDO / REPORT OK')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,e=coroutine.resume(task);if not ok then print('COPLANAR CANCEL FAIL '..tostring(e));mbm.quit() end
 if mbm.getTimeRun()-started>30 then print('COPLANAR CANCEL FAIL timeout');mbm.quit() end
end
