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
local Assembly=require 'image_mesh_assembly'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local started,phase,baseline,pose,oldPaths
local function close(a,b) assert(math.abs(a-b)<.0001,tostring(a)..' ~= '..tostring(b)) end
function onInitScene()
 init()
 local pixels={};for y=0,31 do for x=0,95 do
  pixels[#pixels+1]=x*2;pixels[#pixels+1]=y*7;pixels[#pixels+1]=180
 end end
 assert(mbm.createTexture(pixels,96,32,3,'assembly_fixture','/tmp/ime_assembly.png'))
 api.openImage('/tmp/ime_assembly.png')
 assert(api.action(function(p)
  p.defaults.width=40;p.defaults.depth=8;p.defaults.relief=2;p.defaults.columns=8;p.defaults.rows=8
  for i=1,3 do
   local r=Model.add(p,'rectangle',(i-1)*32,0,32,32)
   r.overrides.depth=6+i*2
  end
  p.regions[2].overrides.simplify=true;p.regions[2].overrides.simplifyRatio=.95
  p.regions[3].overrides.sideMode='color';p.regions[3].overrides.backSolid=true
 end))
 api.select(1,false);api.setEditMode(false);api.setAssembly(true)
 started=mbm.getTimeRun();phase=1
end
local function check()
 local E=api.state;local a=E.assembly
 if E.meshTask or E.dirty then return end
 if phase==1 then
  assert(#a.items==3 and a.builds==1 and not E.preview,'assembly did not replace single preview')
  for _,item in ipairs(a.items) do assert(item.preview.visible);close(item.z+item.depth/2,0) end
  a.gapX=5;Assembly.layout(E);close(a.items[2].x-a.items[1].x,45)
  a.slots[2].row=1;a.slots[2].column=0;a.slots[2].z=3;Assembly.layout(E)
  close(a.items[2].x,a.items[1].x);close(a.items[2].y-a.items[1].y,-40)
  close(a.items[2].z+a.items[2].depth/2,3)
  api.setWireframe(true)
  for _,item in ipairs(a.items) do assert(item.wireObject.visible and not item.preview.visible) end
  a.slots[2].visible=false;Assembly.sync(E);assert(not a.items[2].wireObject.visible)
  api.setWireframe(false);a.slots[2].visible=true;Assembly.sync(E)
  api.select(2,false);phase=2
 elseif phase==2 then
  assert(a.builds==1,'selection rebuilt all geometry')
  E.orbit.distance=345;E.orbit.azimuth=.72;api.camera()
  pose=Model.copy(E.orbit);oldPaths={}
  for _,item in ipairs(a.items) do oldPaths[#oldPaths+1]=item.previewPath end
  assert(api.action(function(p) p.regions[1].overrides.relief=3 end));phase=3
 elseif phase==3 then
  assert(a.builds==2)
  for k,v in pairs(pose) do close(E.orbit[k],v) end
  for _,path in ipairs(oldPaths) do local f=io.open(path,'rb');if f then f:close();error('old preview file leaked') end end
  api.setEditMode(true)
  for _,item in ipairs(a.items) do assert(not item.preview.visible) end
  api.setEditMode(false)
  for _,item in ipairs(a.items) do assert(item.preview.visible) end
  a.columns=3;Assembly.arrange(E)
  baseline={builds=a.builds,layouts=a.layouts,time=mbm.getTimeRun()};phase=4
 elseif phase==4 and mbm.getTimeRun()-baseline.time>2 then
  assert(a.builds==baseline.builds and a.layouts==baseline.layouts,'idle assembly work')
  local original=Model.copy(E.project)
  api.setAssembly(false);assert(#a.items==0)
  assert(E.project.regions[2].overrides.simplify==original.regions[2].overrides.simplify)
  phase=5
 elseif phase==5 then
  assert(E.preview and E.preview.visible,'single preview not restored')
  print('ASSEMBLY / GRID / DEPTH / SIMPLIFY / MATERIALS / WIRE / CAMERA / RELEASE / IDLE OK')
  mbm.quit()
 end
end
function onLoop(delta)
 local header=tImGui.CollapsingHeader
 tImGui.CollapsingHeader=function(label,...)
  if label==tLang.L('ime_assembly_title') then tImGui.SetNextItemOpen(true,0) end
  return header(label,...)
 end
 loop(delta);tImGui.CollapsingHeader=header
 local ok,err=pcall(check)
 if not ok then print('ASSEMBLY FAIL '..tostring(err));mbm.quit() end
 if started and mbm.getTimeRun()-started>20 then print('ASSEMBLY TIMEOUT');mbm.quit() end
end
