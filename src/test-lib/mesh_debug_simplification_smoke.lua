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
local View=require 'mesh_debug_simplification'
local helper=dofile('src/test-lib/mesh_simplification_fixture.lua')
local initialize=onInitScene
local job,stage,rendered,entry
local function safe(fn,...)
 local values=table.pack(pcall(fn,...));if not values[1] then print('COMPARISON ERROR '..tostring(values[2])) end
 return table.unpack(values,1,values.n)
end
local function operation()
 local d=helper.grid('hole');d:setTexture(1,1,'#6080FFFF')
 assert(d:save('/tmp/md-simplify-source.msh',false,false,true))
 assert(addMeshToTable('/tmp/md-simplify-source.msh'))
 entry=tLoadedMeshes[#tLoadedMeshes];iSelectedMeshIndex=#tLoadedMeshes
 entry.sOpenNode='simplification';entry.tSimplifyState={scope='frame',selectedFrame=1,ratio=.5,preserveDetails=true,
  boundaryCollapseThreshold=0,mode='qem'}
 assert(simplifyApply(entry,entry.meshDebug,iSelectedMeshIndex))
 while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
 updatePreviewMesh()
 local InfoWire=require 'mesh_debug_info_wireframe'
 assert(InfoWire.set(entry,tPreviewMesh,true,safe))
 local infoView=entry.infoWireView;local infoWire=infoView.wireObject
 for _=1,10 do
  tPreviewMesh.visible=true;InfoWire.sync(entry,true)
  assert(infoWire.visible and not tPreviewMesh.visible and infoView.wireBuilds==1)
 end
 InfoWire.sync(entry,false);assert(not infoWire.visible and tPreviewMesh.visible)
 assert(InfoWire.set(entry,tPreviewMesh,false,safe))
 InfoWire.sync(entry,true);assert(not infoWire.visible and tPreviewMesh.visible)
 assert(InfoWire.set(entry,tPreviewMesh,true,safe));assert(entry.infoWireView.wireObject==infoWire)
 stage='info';rendered=0
 for _=1,8 do coroutine.yield() end
 entry.infoTestRT:destroy();entry.infoTestRT=nil
 InfoWire.release(entry);assert(not entry.infoWireView and tPreviewMesh.visible)
 stage=nil
 assert(entry.simplifyComparison and entry.simplifyComparison.triangles==entry.tSimplifyState.report.resultTriangleCount)
 assert(View.ensure(entry,safe));local view=entry.simplifyComparisonView
 local original=meshDebug:new();assert(original:load(view.original.previewPath));assert(original:getTotalIndex(1,1)==360)
 local result=meshDebug:new();assert(result:load(view.result.previewPath));assert(result:getTotalIndex(1,1)==entry.tSimplifyState.report.resultTriangleCount*3 and result:getTotalIndex(1,1)<360)
 local frozen=helper.signature(entry.meshDebug)
 view.enabled=true;View.fit(entry,applyCam3d);View.sync(entry,tPreviewMesh,true)
 assert(not tPreviewMesh.visible and view.original.preview.visible and view.result.preview.visible)
 assert(view.original.x~=view.result.x and helper.signature(entry.meshDebug)==frozen)
 View.wire(entry,true,safe);View.sync(entry,tPreviewMesh,true)
 assert(view.wire and not view.original.preview.visible and view.original.wireObject.visible and view.result.wireObject.visible)
 local wireA,wireB=view.original.wireObject,view.result.wireObject
 for _=1,10 do tPreviewMesh.visible=true;View.sync(entry,tPreviewMesh,true);assert(not tPreviewMesh.visible);View.wire(entry,true,safe);assert(View.ensure(entry,safe)) end
 assert(view.original.wireBuilds==1 and view.result.wireBuilds==1 and view.original.wireObject==wireA and view.result.wireObject==wireB)
 View.sync(entry,tPreviewMesh,false);assert(tPreviewMesh.visible and not wireA.visible and not wireB.visible)
 View.sync(entry,tPreviewMesh,true)
 view.showOriginal=false;view.dirty=true;View.sync(entry,tPreviewMesh,true);assert(not wireA.visible and wireB.visible)
 view.showOriginal=true;view.dirty=true;View.sync(entry,tPreviewMesh,true)
 local record=entry.simplifyComparison
 assert(simplifyApply(entry,entry.meshDebug,iSelectedMeshIndex));assert(simplifyCancel(entry))
 while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
 assert(entry.simplifyComparison==record)
 entry.tSimplifyState.mode='qem';entry.tSimplifyState.ratio=.001
 assert(simplifyApply(entry,entry.meshDebug,iSelectedMeshIndex))
 while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
 assert(entry.simplifyComparison==record and helper.signature(entry.meshDebug)==frozen)
 entry.tSimplifyState.mode='qem'
 stage='render';rendered=0
 for _=1,12 do coroutine.yield() end
 entry.testRT:destroy();entry.testRT=nil
 assert(simplifyRestoreBackup(entry,iSelectedMeshIndex))
 assert(not entry.simplifyComparison and not entry.simplifyComparisonView and entry.meshDebug:getTotalIndex(1,1)==360)
 assert(io.open(record.path,'rb')==nil)
 -- Repeat with QEM and verify selection/preview destruction releases renderables,
 -- while retaining the immutable result for a later comparison.
 entry.tSimplifyState.mode='qem';entry.tSimplifyState.ratio=.75
 assert(simplifyApply(entry,entry.meshDebug,iSelectedMeshIndex))
 while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
 assert(entry.simplifyComparison and entry.tSimplifyState.report.qemRan)
 updatePreviewMesh();assert(View.ensure(entry,safe));local path=entry.simplifyComparison.path
 destroyPreviewMesh();assert(not entry.simplifyComparisonView and entry.simplifyComparison.path==path)
 simplifyDiscardBackup(entry);assert(not entry.simplifyComparison and io.open(path,'rb')==nil)
 -- Reference-frame selection must survive snapshot extraction on multiframe meshes.
 local multi=meshDebug:new();multi:setType('mesh');multi:setModeFrontFace('CCW')
 for frame=1,2 do
  local _,vertices,indices=helper.grid('flat')
  for _,v in ipairs(vertices) do v.x=v.x+(frame-1)*64 end
  multi:addFrame(3);multi:addSubSet(frame)
  assert(multi:addVertex(frame,1,vertices));assert(multi:addIndex(frame,1,indices))
 end
 multi:addAnim('Frames',1,2,1,0)
 local other={meshDebug=multi,info={type='mesh'},modified=false,tPendingOps={},
  tSimplifyState={scope='frame',selectedFrame=2,ratio=.75,preserveDetails=true,boundaryCollapseThreshold=0,mode='qem'}}
 assert(simplifyApply(other,multi,0))
 while other.tSimplifyState.running do simplifyResume(other);coroutine.yield() end
 assert(other.simplifyComparison.frame==2 and View.ensure(other,safe))
 assert(other.simplifyComparisonView.original.lo[1]==64 and other.simplifyComparisonView.result.lo[1]==64)
 assert(other.meshDebug:getTotalIndex(1,1)==384,'unselected frame changed')
 simplifyDiscardBackup(other)
 print('MESH DEBUG SIMPLIFICATION WORKTREE / COMPARISON / WIREFRAME / IDLE / CANCEL / UNDO / CLEANUP OK')
end
function onInitScene()
 initialize();job=coroutine.create(operation)
end
function onLoop()
 local ok,e=coroutine.resume(job)
 if not ok then print('MESH DEBUG SIMPLIFICATION FAIL '..tostring(e));mbm.quit();return end
 if stage=='info' then
  local InfoWire=require 'mesh_debug_info_wireframe'
  entry.sOpenNode='meshinfo'
  tImGui.Begin('Mesh Info wireframe test',false,0)
  local drawn,why=safe(showMeshOptions,entry,iSelectedMeshIndex)
  tImGui.End();assert(drawn,why)
  InfoWire.sync(entry,true);rendered=rendered+1
  assert(entry.infoWireView.wireObject.visible and not tPreviewMesh.visible)
  if rendered==1 then
   local rt=render2texture:new('2ds');assert(rt:create(512,512,true,'md-info-wireframe'))
   assert(rt:add(entry.infoWireView.wireObject))
   local camera=rt:getCamera('3d');camera:setPos(4,4,-15);camera:setFocus(4,4,0)
   camera:setNear(.1);camera:setFar(100);entry.infoTestRT=rt
  elseif rendered==6 then assert(entry.infoTestRT:save('/tmp/md-info-wireframe.png')) end
 end
 if stage=='render' and entry.simplifyComparisonView then
  tImGui.SetNextWindowSize({x=420,y=650},tImGui.Flags('ImGuiCond_Always'))
  tImGui.Begin('Simplification test',false,0)
  local drawn,why=safe(function()
   if rendered==0 then
    local settings=showSimplifyGeometry;local calls=0
    showSimplifyGeometry=function(...) calls=calls+1;return settings(...) end
    entry.sOpenNode='frameNode';showFrameNode(entry,entry.meshDebug,iSelectedMeshIndex)
    assert(calls==0,'Frame still contains simplification')
    entry.sOpenNode='simplification'
    View.draw(entry,entry.meshDebug,iSelectedMeshIndex,showSimplifyGeometry,safe,applyCam3d,true)
    showSimplifyGeometry=settings;assert(calls==1,'Simplification tree missing settings')
   else showMeshOptions(entry,iSelectedMeshIndex) end
  end)
  tImGui.End();assert(drawn,why)
  View.sync(entry,tPreviewMesh,true);rendered=rendered+1
  if rendered==3 then
   local rt=render2texture:new('2ds');assert(rt:create(512,256,true,'md-simplify-comparison'))
   local view=entry.simplifyComparisonView
   assert(rt:add(view.original.wireObject));assert(rt:add(view.result.wireObject))
   local camera=rt:getCamera('3d');camera:setPos(0,4,-35);camera:setFocus(0,4,0);camera:setNear(.1);camera:setFar(100)
   entry.testRT=rt
  elseif rendered==6 then assert(entry.testRT:save('/tmp/md-simplify-comparison.png'))
  elseif rendered==7 then
   local view=entry.simplifyComparisonView
   entry.testRT:remove(view.original.wireObject);entry.testRT:remove(view.result.wireObject)
   View.wire(entry,false,safe);View.sync(entry,tPreviewMesh,true)
   assert(entry.testRT:add(view.original.preview));assert(entry.testRT:add(view.result.preview))
  elseif rendered==10 then assert(entry.testRT:save('/tmp/md-simplify-comparison-filled.png')) end
 end
 if coroutine.status(job)=='dead' then mbm.quit() end
end
