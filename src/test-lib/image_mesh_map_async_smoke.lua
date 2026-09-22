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
local function wait(job)
 local count,progress=0,0
 while true do
  local s=job:getStatus();assert(s.progress>=progress);progress=s.progress
  if s.state~='running' then return s,count end
  count=count+1;coroutine.yield()
 end
end
local function test()
 local pixels={};for y=0,128 do for x=0,256 do for c=1,3 do pixels[#pixels+1]=(x+y)%256 end end end
 local path='/tmp/ime-map-async-source.png';assert(mbm.createTexture(pixels,257,129,3,'map_async',path))
 for _,overlay in ipairs{false,true} do
  local o={shape='ellipse',smoothPasses=2,heightSource='mixed',heightCurve=2,
   heightAreas={{{x=.2,y=.3},{x=.8,y=.7},shape='line',lineWidth=.1,height=.2,transition=.02}}}
  assert(mbm.generateImageMeshMap(path,o,'/tmp/ime-map-sync.png',overlay))
  local job=assert(mbm.startImageMeshMap(path,o,'/tmp/ime-map-async.png',overlay))
  assert(not job:takeResult());local busy=mbm.startImageMeshMap(path,o,'/tmp/ime-map-busy.png');assert(not busy)
  o.heightCurve=.5;o.heightAreas[1].lineWidth=.5
  local meshJob=assert(mbm.startImageMesh(path,{columns=2,rows=2}))
  local status,frames=wait(job);assert(status.state=='completed',status.error);assert(frames>0)
  assert(job:takeResult() and not job:takeResult());assert(job:getStatus().state=='consumed')
  assert(mbm.readImagePixels('/tmp/ime-map-sync.png')==mbm.readImagePixels('/tmp/ime-map-async.png'),'map pixels changed')
  wait(meshJob);meshJob:close();job:close();job:close()
 end
 local job=assert(mbm.startImageMeshMap(path,{smoothPasses=4},'/tmp/ime-map-cancel.png'));job:cancel()
 assert(wait(job).state=='cancelled');job:close();os.remove('/tmp/ime-map-cancel.png')
 job=assert(mbm.startImageMeshMap('/tmp/no-image-async-map.png',{},'/tmp/ime-map-failed.png'))
 assert(wait(job).state=='failed');job:close()
 job=assert(mbm.startImageMeshMap(path,{},'/tmp/ime-map-close.png'));job:close();os.remove('/tmp/ime-map-close.png')
 job=assert(mbm.startImageMeshMap(path,{},'/tmp/ime-map-retry.png'));assert(wait(job).state=='completed');job:close()
 print('IMAGE MESH MAP ASYNC PIXEL PARITY / SNAPSHOT / FRAMES / PROGRESS / MESH CONCURRENCY / CANCEL / ERROR / CLOSE / RETRY OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('IMAGE MESH MAP ASYNC FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>40 then print('IMAGE MESH MAP ASYNC FAIL timeout');mbm.quit() end
end
