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
local task,started
local function wait(job)
 local frames,last=0,0
 while true do
  local s=job:getStatus();assert(s.progress>=last and s.progress<=1);last=s.progress
  if s.state~='running' then return s,frames end
  frames=frames+1;coroutine.yield()
 end
end
local function test()
 local pixels={};for y=0,128 do for x=0,128 do for c=1,3 do pixels[#pixels+1]=(x*3+y*5)%256 end end end
 local path='/tmp/ime-async-source.png';assert(mbm.createTexture(pixels,129,129,3,'async_source',path))
 local options={shape='ellipse',heightSource='manual',baseHeight=.25,followImage=true,
  heightAreas={{{x=.2,y=.2},{x=.8,y=.2},{x=.8,y=.8},{x=.2,y=.8},height=.8,transition=.015}},
  heightEdits={{x=.5,y=.5,radius=.08,height=.1,strength=1,mode='flatten'}},
  sideMode='color',sideColor=0x123456,backSolid=true,backColor=0x998877}
 local expected,report=mbm.generateImageMesh(path,options);assert(expected,report)
 local job=assert(mbm.startImageMesh(path,options))
 local early=job:takeResult();assert(not early)
 local busy,why=mbm.startImageMesh(path,{});assert(not busy and why:find('running'))
 options.heightAreas[1][1].x=.9;options.heightAreas[1].height=0;options.baseHeight=1
 local state,frames=wait(job);assert(state.state=='completed',state.error);assert(frames>0)
 local asset,result=job:takeResult();assert(asset,result)
 assert(result.vertices==report.vertices and result.triangles==report.triangles)
 local a,b=Asset.vertices(asset),Asset.vertices(expected);assert(#a==#b)
 for i,p in ipairs(a) do for _,key in ipairs{'x','y','z','nx','ny','nz','u','v'} do assert(p[key]==b[i][key],key) end end
 for i=1,asset:getTotalSubset(1) do assert(asset:getTexture(1,i)==expected:getTexture(1,i)) end
 assert(job:getStatus().state=='consumed' and not job:takeResult())
 local complex={shape='polygon',contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=0,y=1}},
  holes={{{x=.1,y=.1},{x=.18,y=.1},{x=.18,y=.18},{x=.1,y=.18}}},
  heightSource='manual',baseHeight=.3,sideMode='repeat',sideTexture=path,backExternal=true,backTexture=path}
 local reference,referenceReport=mbm.generateImageMesh(path,complex);assert(reference,referenceReport)
 job=assert(mbm.startImageMesh(path,complex));complex.contour[1].x=.9;complex.holes[1][1].x=.8;complex.sideTexture='missing';complex.backTexture='missing'
 state=wait(job);assert(state.state=='completed',state.error)
 asset,result=job:takeResult();assert(result.triangles==referenceReport.triangles)
 a,b=Asset.vertices(asset),Asset.vertices(reference);assert(#a==#b)
 for i,p in ipairs(a) do for _,key in ipairs{'x','y','z','nx','ny','nz','u','v'} do assert(p[key]==b[i][key],key) end end
 for i=1,asset:getTotalSubset(1) do assert(asset:getTexture(1,i)==reference:getTexture(1,i)) end
 options.heightAreas=nil
 job=assert(mbm.startImageMesh(path,options));job:cancel();state=wait(job)
 assert(state.state=='cancelled' and not job:takeResult())
 options.width=-1;job=assert(mbm.startImageMesh(path,options));state=wait(job)
 assert(state.state=='failed' and state.error and not job:takeResult())
 options.width=100
 job=assert(mbm.startImageMesh(path,options));job=nil;collectgarbage('collect')
 job=assert(mbm.startImageMesh(path,options));state=wait(job);assert(state.state=='completed')
 print('IMAGE MESH ASYNC PARITY / SNAPSHOT / FRAME YIELDS / PROGRESS / CANCEL / FAILURE / GC / RETRY OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH ASYNC FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>30 then print('IMAGE MESH ASYNC FAIL timeout');mbm.quit() end
end
