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
local task,started
local function close(a,b) assert(math.abs(a-b)<.006,tostring(a)..' != '..tostring(b)) end
local function test()
 local path='/tmp/ime-channels.png';local rgb='/tmp/ime-channels-rgb.png'
 local pixels,opaque={},{}
 for i=1,81 do
  for _,v in ipairs{51,102,153,204} do pixels[#pixels+1]=v end
  for _,v in ipairs{51,102,153} do opaque[#opaque+1]=v end
 end
 assert(mbm.createTexture(pixels,9,9,4,'ime_channels',path))
 assert(mbm.createTexture(opaque,9,9,3,'ime_channels_rgb',rgb))
 local options={columns=2,rows=2,lockBorder=false,relief=10}
 local function sample()
  local output='/tmp/ime-channels-map.png'
  local ok,err=mbm.generateImageMeshMap(path,options,output);assert(ok,err)
  local bytes=assert(mbm.readImagePixels(output));return bytes:byte((4*9+4)*4+1)/255
 end
 for _,entry in ipairs{{'luminance',.37192},{'red',.2},{'green',.4},{'blue',.6},{'alpha',.8}} do
  options.heightChannel=entry[1]
  local asset,report=mbm.generateImageMesh(path,options);assert(asset,report)
  close(report.minHeight,entry[2]*10);close(report.maxHeight,entry[2]*10);close(sample(),entry[2])
  assert(asset:getTexture(1,1):find('ime%-channels.png'),'channel replaced color texture')
  local job=assert(mbm.startImageMesh(path,options));options.heightChannel='red'
  while job:getStatus().state=='running' do coroutine.yield() end
  local other,result=job:takeResult();assert(other,result);close(result.maxHeight,entry[2]*10)
 end
 options.heightChannel=nil;close(sample(),.37192)
 options.heightChannel='green';options.invert=true;close(sample(),.6)
 options.invert=false;options.twoLevels=true;options.grooveThreshold=.5;options.grooveTransition=.1
 options.heightChannel='red';close(sample(),0)
 options.heightChannel='blue';close(sample(),1)
 options.twoLevels=false;options.heightSource='manual';options.baseHeight=.3;close(sample(),.3)
 options.heightSource='mixed';options.heightChannel='red'
 options.heightAreas={{{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=0,y=1},height=.7,transition=0}}
 close(sample(),.7)
 options.heightAreas=nil;options.heightSource='image';options.heightChannel='alpha'
 local asset,report=mbm.generateImageMesh(rgb,options);assert(asset,report);close(report.minHeight,10)
 options.heightChannel='invalid';assert(not pcall(mbm.generateImageMesh,path,options))
 local p=Model.new(path,9,9);local r=Model.add(p,'rectangle',0,0,9,9)
 p.defaults.heightChannel='green';r.overrides.heightChannel='alpha';Model.validate(p)
 local history=Model.history();Model.commit(history,p);r.overrides.heightChannel='blue'
 local before=Model.undo(history,p);assert(Model.options(before,before.regions[1]).heightChannel=='alpha')
 local after=Model.undo(history,before,true);assert(Model.options(after,after.regions[1]).heightChannel=='blue')
 p.presets={{name='channel',settings=Model.settings(Model.options(p,r))}}
 tImGui=require 'ImGui'
 local util=require 'editor_utils';assert(IO.save(p,'/tmp/ime-channels.imesh',util.save))
 local loaded=IO.load('/tmp/ime-channels.imesh');assert(Model.options(loaded,loaded.regions[1]).heightChannel=='blue')
 assert(loaded.presets[1].settings.heightChannel=='blue')
 loaded.defaults.heightChannel=nil;loaded.regions[1].overrides.heightChannel=nil
 assert(Model.options(loaded,loaded.regions[1]).heightChannel=='luminance')
 print('IMAGE MESH CHANNELS RGB / ALPHA / DEFAULT / INVERT / GROOVES / MANUAL / MIXED / ASYNC SNAPSHOT / SAVE / PRESET / UNDO OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH CHANNELS FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>30 then print('IMAGE MESH CHANNELS FAIL timeout');mbm.quit() end
end
