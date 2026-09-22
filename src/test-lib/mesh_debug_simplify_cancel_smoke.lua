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
local init=onInitScene
local task,started
local function wait(entry)
 while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
end
local function source()
 local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CW');d:addFrame(3)
 local indices={3,1,5,3,5,2,3,2,6,3,6,1,4,5,1,4,2,5,4,6,2,4,1,6}
 for s=1,2 do
  d:addSubSet(1)
  local v={{x=1,y=0,z=0},{x=-1,y=0,z=0},{x=0,y=1,z=0},{x=0,y=-1,z=0},{x=0,y=0,z=1},{x=0,y=0,z=-1}}
  for _,p in ipairs(v) do p.x=p.x+s*4 end
  assert(d:addVertex(1,s,v));assert(d:addIndex(1,s,indices));d:setTexture(1,s,'#FFFFFFFF')
 end
 d:addAnim('Static',1,1,1,0);assert(d:check());return d
end
local function test()
 for _,mode in ipairs{'frame','subsets','virtual'} do
  local d=source()
  local entry={meshDebug=d,info={type='mesh'},modified=true,tPendingOps={},
   tSimplifyState={scope=mode=='frame' and 'frame' or 'subsets',virtualFrame=mode=='virtual',
    selectedSubsets={[1]=true,[2]=true},selectedFrame=1,ratio=.5,preserveDetails=true,boundaryCollapseThreshold=0}}
  local previous={path='/tmp/debug-cancel-previous-'..mode..'.msh'}
  assert(d:save(previous.path,false,false,true));entry.tSimplifyBackup=previous
  assert(simplifyApply(entry,d,0));assert(entry.tSimplifyState.activeMesh)
  if mode=='frame' then
   local button=tImGui.Button;local clicked=false
   tImGui.Button=function(label,...)
    local pressed=button(label,...)
    if label:find('##simplifyCancel-',1,true) then clicked=true;return true end
    return pressed
   end
   tImGui.Begin('Simplification cancellation test',false,0)
   showSimplifyGeometry(entry,d,0,1,{})
   tImGui.End();tImGui.Button=button
   assert(clicked and entry.tSimplifyState.cancelRequested)
  elseif mode=='subsets' then
   while entry.tSimplifyState.progress<.5 do
    simplifyResume(entry);coroutine.yield()
   end
   assert(entry.tSimplifyState.running,'second subset was not pending')
   tLoadedMeshes={entry};iSelectedMeshIndex=1
   onKeyDown(mbm.getKeyCode('ESC'))
   assert(entry.tSimplifyState.cancelRequested)
   tLoadedMeshes={};iSelectedMeshIndex=0
  else assert(simplifyCancel(entry)) end
  wait(entry)
  assert(entry.meshDebug==d and entry.modified and entry.tSimplifyBackup==previous)
  assert(not entry.tSimplifyState.report and not entry.tSimplifyState.lastError and not entry.tSimplifyState.activeMesh)
  assert(d:getTotalIndex(1,1)==24 and d:getTotalIndex(1,2)==24)
  assert(not simplifyCancel(entry))
  assert(simplifyApply(entry,d,0));wait(entry)
  assert(entry.meshDebug~=d and entry.tSimplifyState.report and not entry.tSimplifyState.lastError)
  assert(entry.tSimplifyState.report.resultTriangleCount<16)
  simplifyDiscardBackup(entry)
 end
 print('MESH DEBUG CANCEL FRAME / SUBSETS / VIRTUAL / ORIGINAL / UNDO / RETRY OK')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('MESH DEBUG CANCEL FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>40 then print('MESH DEBUG CANCEL FAIL timeout');mbm.quit() end
end
