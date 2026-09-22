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
 local path='/tmp/ime-levels-source.png';local pixels={}
 for y=0,4 do for x=0,4 do for c=1,3 do pixels[#pixels+1]=math.min(255,x*64) end end end
 assert(mbm.createTexture(pixels,5,5,3,'ime_levels',path))
 local o={columns=4,rows=4,lockBorder=false,relief=8,depth=20,heightChannel='red',heightBlack=64/255,heightWhite=192/255,heightCurve=2}
 local function map()
  local ok,err=mbm.generateImageMeshMap(path,o,'/tmp/ime-levels-map.png');assert(ok,err)
  local bytes=assert(mbm.readImagePixels('/tmp/ime-levels-map.png'))
  return function(x) return bytes:byte((2*5+x)*4+1)/255 end
 end
 local m=map();close(m(0),0);close(m(1),0);close(m(2),.25);close(m(3),1);close(m(4),1)
 local asset,report=mbm.generateImageMesh(path,o);assert(asset,report)
 local found=false
 for _,v in ipairs(asset:getVertex(1,1,1,report.vertices)) do
  if math.abs(v.x)<.001 and math.abs(v.y)<.001 and v.z<0 then close(v.z,-12);found=true end
 end
 assert(found,'no center surface vertex')
 local job=assert(mbm.startImageMesh(path,o));o.heightCurve=.5
 while job:getStatus().state=='running' do coroutine.yield() end
 local other,result=job:takeResult();assert(other,result)
 local a,b=asset:getVertex(1,1,1,report.vertices),other:getVertex(1,1,1,result.vertices)
 assert(#a==#b);for i,v in ipairs(a) do assert(v.z==b[i].z) end
 close(map()(2),math.sqrt(.5))
 o.heightCurve=1;o.invert=true;close(map()(2),.5);close(map()(0),1);o.invert=false
 o.heightBlack=128/255;o.heightWhite=128/255;close(map()(1),0);close(map()(2),1)
 o.heightWhite=.1;local bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('levels'))
 o.heightBlack=0;o.heightWhite=1;o.heightCurve=0;bad,err=mbm.generateImageMesh(path,o);assert(not bad and err:find('levels'))
 o.heightCurve=2;o.heightSource='manual';o.baseHeight=.3;close(map()(2),.3)
 o.heightSource='image';o.heightImage=path;close(map()(2),(128/255)^2)
 o.heightImage=nil;o.heightCurve=nil;o.heightBlack=nil;o.heightWhite=nil;close(map()(2),128/255)
 local p=Model.new(path,5,5);local r=Model.add(p,'rectangle',0,0,5,5)
 r.overrides.heightCurve=2;r.overrides.heightBlack=.2;r.overrides.heightWhite=.8
 local history=Model.history();Model.commit(history,p);r.overrides.heightCurve=.5
 local old=Model.undo(history,p);assert(old.regions[1].overrides.heightCurve==2)
 p.presets={{name='levels',settings=Model.settings(Model.options(p,r))}}
 tImGui=require 'ImGui';local util=require 'editor_utils';IO.save(p,'/tmp/ime-levels.imesh',util.save)
 local loaded=IO.load('/tmp/ime-levels.imesh');assert(loaded.regions[1].overrides.heightCurve==.5)
 assert(loaded.presets[1].settings.heightBlack==.2)
 local invalid=Model.copy(p);invalid.defaults.heightWhite=.1;invalid.regions[1].overrides.heightWhite=nil
 assert(not pcall(Model.validate,invalid),'invalid inherited black/white accepted')
 print('IMAGE MESH LEVELS MAP / GEOMETRY / CURVE / INVERSION / EQUAL POINTS / VALIDATION / MANUAL / EXTERNAL / LEGACY / ASYNC / SAVE / PRESET / UNDO OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH LEVELS FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>30 then print('IMAGE MESH LEVELS FAIL timeout');mbm.quit() end
end
