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
local Generation=require 'image_mesh_generation'
local Aliases=require 'image_mesh_texture_aliases'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local task,started
local notices={}
local show=tUtil.showMessage
local function wait(E) while E.meshTask do coroutine.yield() end end
local function rebuild(E) api.rebuild();wait(E) end
local function test()
 local E=api.state
 tLang.setLanguage('pt_br')
 tUtil.showMessage=function(message,duration)
  if message:find('Malha 3D',1,true) or message:find('malha 3D',1,true) then
   assert(not E.imageJob and not E.simplifyAsset,'Notification before final worker completion')
   notices[#notices+1]=message
  end
  return show(message,duration)
 end
 local pixels={};for i=1,32*32*3 do pixels[i]=128 end
 assert(mbm.createTexture(pixels,32,32,3,'notification_fixture','/tmp/ime-notification.png'))
 api.openImage('/tmp/ime-notification.png')
 assert(api.action(function(p)
  p.defaults.columns=12;p.defaults.rows=12;p.defaults.simplify=true;p.defaults.simplifyRatio=.7
  Model.add(p,'rectangle',0,0,32,32)
 end))
 api.select(1,false);api.setEditMode(false);rebuild(E)
 assert(#notices==1 and notices[1]:find('sucesso',1,true) and E.preview)
 assert(not tUtil.bWarnMessage)
 -- A late preview-write failure must not emit success for the completed geometry.
 local save=Aliases.savePreview
 local saves=0
 Aliases.savePreview=function(...)
  saves=saves+1
  if saves==2 then return false end
  return save(...)
 end
 E.dirty=true;rebuild(E);Aliases.savePreview=save
 assert(#notices==2 and notices[2]:find('Falha',1,true) and E.generationFailure)
 E.dirty=true;rebuild(E)
 assert(#notices==3 and notices[3]:find('sucesso',1,true))
 -- Reusing the same preview on return to 3D also restarts the identical message.
 api.setEditMode(true);api.setEditMode(false)
 assert(#notices==4 and notices[4]==notices[3])
 local builds=E.builds;local start=mbm.getTimeRun()
 while mbm.getTimeRun()-start<6.5 do coroutine.yield() end
 assert(#notices==4 and E.builds==builds,'Idle regenerated or repeated notification')
 assert(not tUtil.sMessageOverlay,'Notification did not expire')
 local generate=Generation.generate
 Generation.generate=function(owner)
  coroutine.yield();owner.generationCancelled=true
  return nil,'ime_generation_cancelled'
 end
 E.dirty=true;rebuild(E);Generation.generate=generate
 assert(#notices==5 and notices[5]:find('cancelada',1,true))
 local project=os.getenv('IMAGE_MESH_NOTIFICATION_PROJECT')
 if project then
  api.openProject(project)
  local id
  for _,r in ipairs(E.project.regions) do if r.name=='module_012' then id=r.id end end
  assert(id,'module_012 missing');api.select(id,false);api.setEditMode(false);rebuild(E)
  assert(E.preview and notices[#notices]:find('sucesso: module_012',1,true),E.status)
 end
 local shown=mbm.getTimeRun()
 while mbm.getTimeRun()-shown<3 do coroutine.yield() end
 print('IMAGE MESH NOTIFICATION PASS: final success / late failure / cancellation / repeated result / expiry / idle')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH NOTIFICATION FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>90 then print('IMAGE MESH NOTIFICATION FAIL timeout');mbm.quit() end
end
function onEndScene() tUtil.showMessage=show;finish() end
