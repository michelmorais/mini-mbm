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
local Presets=require 'image_mesh_presets'
local task,started
local function close(a,b) assert(math.abs(a-b)<.012,tostring(a)..' != '..tostring(b)) end
local function test()
 local source='/tmp/ime-height-color.png';local map='/tmp/ime-height-data.png'
 local color,data={},{}
 for i=1,81 do for _,v in ipairs{200,50,10} do color[#color+1]=v end end
 for y=0,2 do for x=0,2 do for c=1,3 do data[#data+1]=x==0 and 0 or x==1 and 128 or 255 end end end
 assert(mbm.createTexture(color,9,9,3,'height_color',source))
 assert(mbm.createTexture(data,3,3,3,'height_data',map))
 local o={x=2,y=2,cropWidth=5,cropHeight=5,columns=4,rows=4,lockBorder=false,relief=10,heightImage=map}
 local function mesh()
  local asset,report=mbm.generateImageMesh(source,o);assert(asset,report)
  assert(asset:getTexture(1,1):find('ime%-height%-color.png'),'height map replaced color texture')
  return asset,report
 end
 local function samples()
  local out='/tmp/ime-height-diagnostic.png'
  local ok,err=mbm.generateImageMeshMap(source,o,out);assert(ok,err)
  local pixels,w,h=mbm.readImagePixels(out);assert(w==5 and h==5)
  return pixels:byte(1)/255,pixels:byte(17)/255
 end
 local asset,r=mesh();close(r.minHeight,128/255*5);close(r.maxHeight,(128/255+1)*5)
 local l,h=samples();close(l,128/255/2);close(h,(128/255+1)/2)
 o.heightImageToRegion=true;asset,r=mesh();close(r.minHeight,0);close(r.maxHeight,10)
 l,h=samples();close(l,0);close(h,1)
 o.invert=true;l,h=samples();close(l,1);close(h,0);o.invert=false
 local job=assert(mbm.startImageMesh(source,o));o.heightImage='/tmp/nonexistent-height-map.png';o.heightImageToRegion=false
 while job:getStatus().state=='running' do coroutine.yield() end
 local result,report=job:takeResult();assert(result,report);close(report.minHeight,0);close(report.maxHeight,10)
 local bad,err=mbm.generateImageMesh(source,o);assert(not bad and err:find('Height image'))
 o.heightSource='manual';o.baseHeight=.3;asset,r=mesh();close(r.maxHeight,3)
 o.heightSource='mixed';o.heightImage=map
 o.heightAreas={{{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=0,y=1},height=.7,transition=0}}
 l,h=samples();close(l,.7);close(h,.7)
 o.heightAreas=nil;o.heightSource='image';o.heightImage=''
 asset,r=mesh();close(r.maxHeight,(.2126*200+.7152*50+.0722*10)/255*10)
 local p=Model.new(source,9,9);local region=Model.add(p,'rectangle',2,2,5,5)
 region.overrides.heightImage=map;region.overrides.heightImageToRegion=true
 p.defaults.heightImage=map;p.presets={{name='height map',settings=Model.settings(Model.options(p,region))}}
 tImGui=require 'ImGui';local util=require 'editor_utils'
 IO.save(p,'/tmp/ime-height-map.imesh',util.save)
 local saved=assert(loadfile('/tmp/ime-height-map.imesh'))();assert(saved.regions[1].overrides.heightImage=='ime-height-data.png')
 local loaded=IO.load('/tmp/ime-height-map.imesh');assert(loaded.defaults.heightImage==map)
 assert(loaded.regions[1].overrides.heightImage==map and loaded.regions[1].overrides.heightImageToRegion)
 assert(loaded.presets[1].settings.heightImage==map)
 Presets.save(p.presets[1],'/tmp/ime-height-map.preset',util.save)
 local preset=Presets.load('/tmp/ime-height-map.preset');assert(preset.settings.heightImage==map and preset.settings.heightImageToRegion)
 local history=Model.history();Model.commit(history,p);region.overrides.heightImage=''
 local previous=Model.undo(history,p);assert(previous.regions[1].overrides.heightImage==map)
 print('IMAGE MESH HEIGHT IMAGE ALIGNMENT / RESAMPLING / TEXTURE / MAP / INVERT / ASYNC / MISSING / MANUAL / MIXED / PERSISTENCE / PRESET / UNDO OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH HEIGHT IMAGE FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>30 then print('IMAGE MESH HEIGHT IMAGE FAIL timeout');mbm.quit() end
end
