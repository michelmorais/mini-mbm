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
local Asset=require 'image_mesh_asset'
local Build=require 'image_mesh_build'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local task,started
local function centered(asset,label)
 local vertices=Asset.vertices(asset)
 for _,axis in ipairs{'x','y','z'} do
  local lo,hi=math.huge,-math.huge
  for _,v in ipairs(vertices) do lo=math.min(lo,v[axis]);hi=math.max(hi,v[axis]) end
  assert(math.abs(lo+hi)<1e-4,label..' off-center '..axis..': '..lo..', '..hi)
 end
end
local function saved(path,label)
 local asset=meshDebug:new();assert(asset:load(path));centered(asset,label)
end
local function wait(E) while E.meshTask do coroutine.yield() end end
local function test()
 local E=api.state
 local pixels={};for i=1,32*32 do pixels[#pixels+1]=128;pixels[#pixels+1]=128;pixels[#pixels+1]=128 end
 assert(mbm.createTexture(pixels,32,32,3,'center_fixture','/tmp/ime-center.png'))
 local p=Model.new('/tmp/ime-center.png',32,32)
 p.defaults.columns=12;p.defaults.rows=12;p.defaults.width=100;p.defaults.depth=20;p.defaults.relief=8
 local r=Model.add(p,'polygon',0,0,32,32,{{x=.55,y=.1},{x=.95,y=.1},{x=.85,y=.4},{x=.55,y=.35}})
 for _,mode in ipairs{'polygon','simplified','materials','normal','curved','voxel','rectangle','ellipse'} do
  r.overrides={};r.shape='polygon'
  if mode=='simplified' then r.overrides.simplify=true;r.overrides.simplifyRatio=.7
  elseif mode=='materials' then r.overrides.backSolid=true;r.overrides.sideMode='color'
  elseif mode=='normal' then r.overrides.reliefMode='normal'
  elseif mode=='curved' then r.shape='rectangle';r.overrides.heightSource='curved';r.overrides.curvedSymmetric=false
  elseif mode=='voxel' then r.overrides.voxelized=true
  elseif mode=='rectangle' or mode=='ellipse' then r.shape=mode end
  local asset,report=Build.generate(E,p,r,{beforeSimplify=function(source) centered(source,'original '..mode) end})
  assert(asset,tostring(report));centered(asset,mode)
  if mode=='polygon' or mode=='materials' then
   local raw=assert(mbm.generateImageMesh(p.image.path,Model.geometryOptions(Model.options(p,r))))
   local before,after=Asset.vertices(raw,true),Asset.vertices(asset)
   assert(#before==#after,'Centering changed vertex count')
   for i,v in ipairs(before) do
    for _,key in ipairs{'u','v','nx','ny','nz'} do assert(v[key]==after[i][key],'Centering changed '..key) end
    for _,axis in ipairs{'x','y','z'} do
     assert(math.abs(v[axis]-report.centerOffset[axis]-after[i][axis])<1e-4,'Inconsistent subset translation')
    end
   end
   for subset=1,raw:getTotalSubset(1) do
    local a,b=raw:getIndex(1,subset),asset:getIndex(1,subset)
    assert(#a==#b)
    for i,value in ipairs(a) do assert(value==b[i],'Centering changed winding/topology') end
   end
  end
  assert(asset:save('/tmp/ime-center-result.msh',false,false,true))
  saved('/tmp/ime-center-result.msh','export '..mode)
  print('CENTER CASE OK '..mode)
 end
 local project=os.getenv('IMAGE_MESH_CENTER_PROJECT')
 if project then
  api.openProject(project)
  local found
  for _,region in ipairs(E.project.regions) do if region.name=='wall-fac_004' then found=region.id end end
  assert(found,'wall-fac_004 missing');api.select(found,false)
 else
  api.openImage('/tmp/ime-center.png')
  assert(api.action(function(project)
   Model.add(project,'polygon',0,0,32,32,{{x=.55,y=.1},{x=.95,y=.1},{x=.85,y=.4},{x=.55,y=.35}})
  end));api.select(1,false)
 end
 api.setEditMode(false);api.rebuild();wait(E);assert(E.preview,E.status)
 saved(E.previewPath,'editor preview')
 api.exportOne('/tmp/ime-center-editor.msh',false);wait(E)
 saved('/tmp/ime-center-editor.msh','editor export')
 api.setWireframe(true);assert(E.wireObject)
 local builds=E.builds
 for i=1,12 do coroutine.yield() end
 assert(E.builds==builds,'idle rebuilt centered mesh')
 -- Render an orbit around the origin without rebuilding the geometry.
 local start=mbm.getTimeRun()
 while mbm.getTimeRun()-start<3 do
  E.orbit.azimuth=(mbm.getTimeRun()-start)*math.pi*2/3;api.camera();coroutine.yield()
 end
 assert(E.builds==builds,'Orbit rebuilt centered mesh')
 print('IMAGE MESH CENTER PASS')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
 loop(delta)
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH CENTER FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>90 then print('IMAGE MESH CENTER FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
