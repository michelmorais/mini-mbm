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
 while true do
  local status=job:getStatus()
  if status.state~='running' then
   assert(status.state=='completed',status.error)
   return job:takeResult()
  end
  coroutine.yield()
 end
end
local function test()
 local projectPath=os.getenv('IME_TEST_PROJECT')
 if projectPath then
  local project=IO.load(projectPath)
  for _,region in ipairs(project.regions) do
   local options=Model.options(project,region)
   local asset,report=wait(assert(mbm.startImageMesh(project.image.path,options)))
   assert(asset,report);assert(asset:check())
   assert(asset:save('/tmp/ime-large-project.msh',false,false,true))
   local loaded=meshDebug:new();assert(loaded:load('/tmp/ime-large-project.msh'));assert(loaded:check())
   assert(wait(assert(mbm.startImageMeshMap(project.image.path,options,'/tmp/ime-large-project-map.png',false))))
   print('USER PROJECT MESH / MAP / EXPORT OK '..#region.contour..' points')
  end
 end
 local pixels={};for i=1,65*65*4 do pixels[i]=255 end
 local path='/tmp/ime-large-source.png'
 assert(mbm.createTexture(pixels,65,65,4,'large_contour',path))
 local function ring(count,radius)
  local result={};for i=1,count do local a=2*math.pi*i/count
   result[i]={x=.5+radius*math.cos(a),y=.5+radius*math.sin(a)}
  end
  return result
 end
 local options={shape='polygon',contour=ring(374,.48),holes={ring(200,.1)},columns=2,rows=2,
  cropWidth=65,cropHeight=65,heightSource='manual',baseHeight=.5,sideInset=1}
 local asset,report=mbm.generateImageMesh(path,options);assert(asset,report);assert(asset:check())
 local result,asyncReport=wait(assert(mbm.startImageMesh(path,options)));assert(result and result:check())
 assert(asyncReport.vertices==report.vertices and asyncReport.triangles==report.triangles)
 assert(mbm.generateImageMeshMap(path,options,'/tmp/ime-large-sync.png',false))
 assert(wait(assert(mbm.startImageMeshMap(path,options,'/tmp/ime-large-async.png',false))))
 local contour,why=mbm.getImageMeshSideContour(options);assert(contour,why);assert(#contour==374)
 options.sideBandPerpendicular=true
 contour,why=mbm.getImageMeshSideContour(options);assert(contour,why);assert(#contour==748)
 options.contour=ring(4097,.48)
 assert(not pcall(mbm.generateImageMesh,path,options),'oversized contour accepted')
 options.contour=ring(374,.48);options.holes={ring(4097,.1)}
 assert(not pcall(mbm.startImageMesh,path,options),'oversized hole accepted')
 collectgarbage('collect')
 print('LARGE CONTOURS / HOLES / SYNC / ASYNC / MAPS / SIDE BUFFERS / LIMITS OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('LARGE CONTOURS FAIL '..debug.traceback(task,tostring(err)));mbm.quit() end
 if mbm.getTimeRun()-started>90 then print('LARGE CONTOURS FAIL timeout');mbm.quit() end
end
