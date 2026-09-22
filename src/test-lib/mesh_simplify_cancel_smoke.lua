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

local task,started
local function wait(asset)
 while true do local s=asset:getSimplifyStatus();if s.state~='running' then return s end;coroutine.yield() end
end
local function test()
 local pixels={};for i=1,65*65*3 do pixels[i]=128 end
 local path='/tmp/simplify-cancel-source.png';assert(mbm.createTexture(pixels,65,65,3,'simplify_cancel',path))
 local asset,report=mbm.generateImageMesh(path,{columns=60,rows=60,lockBorder=false});assert(asset,report)
 local vertices=asset:getVertex(1,1,1,report.vertices);local indices=asset:getIndex(1,1)
 assert(not asset:cancelSimplify())
 assert(asset:startSimplify(.5));assert(asset:cancelSimplify(),'request was too late')
 local status=wait(asset);assert(status.state=='cancelled',status.state)
 assert(asset:getTotalVertex(1,1)==#vertices)
 for i,v in ipairs(asset:getVertex(1,1,1,#vertices)) do
  for _,k in ipairs{'x','y','z','nx','ny','nz','u','v'} do assert(v[k]==vertices[i][k],'cancel changed '..k) end
 end
 local current=asset:getIndex(1,1);assert(#current==#indices);for i,v in ipairs(current) do assert(v==indices[i]) end
 assert(asset:startSimplify(.8));status=wait(asset);assert(status.state=='completed',status.error)
 assert(not asset:cancelSimplify(),'late cancellation accepted after commit')
 assert(status.report.resultTriangleCount<report.triangles)
 assert(asset:startSimplify(.8));status=wait(asset);assert(status.state=='completed',status.error)
 print('MESH SIMPLIFY CANCEL STATE / ORIGINAL VERTICES NORMALS UVS INDICES / RETRY / COMMIT RACE OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
 if coroutine.status(task)=='dead' then mbm.quit();return end
 local ok,err=coroutine.resume(task)
 if not ok then print('MESH SIMPLIFY CANCEL FAIL '..tostring(err));mbm.quit() end
 if mbm.getTimeRun()-started>45 then print('MESH SIMPLIFY CANCEL FAIL timeout');mbm.quit() end
end
