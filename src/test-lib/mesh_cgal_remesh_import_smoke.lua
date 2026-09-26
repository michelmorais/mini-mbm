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


-- Regression: real remesh output must fit after normals and survive MSH save/load.
package.path='editor/?.lua;'..package.path
tImGui=require 'ImGui'
tUtil=require 'editor_utils'
local Cgal=require 'mesh_cgal'
local fixture=dofile('src/test-lib/mesh_simplification_fixture.lua')
local task,started
local paths={}
local function await(job,err)
    assert(job,err)
    while true do
        local status=job:getSimplifyStatus()
        if status.state~='running' then assert(status.state=='completed',status.error);return status.report end
        coroutine.yield()
    end
end
local function test()
    Cgal.setRepairPath(assert(os.getenv('MBM_CGAL_REPAIR_EXECUTABLE')),false)
    Cgal.setRemeshPath(assert(os.getenv('MBM_CGAL_REMESH_EXECUTABLE')),false)
    local path=assert(os.getenv('MBM_CGAL_REPAIR_MESH'))
    mbm.addPath(path:match('^(.*)[/\\]') or '.')
    local mesh=meshDebug:new();assert(mesh:load(path))
    local texture=mesh:getTexture(1,1)
    local report=await(Cgal.startRemesh(mesh,nil,1,.03,10,14.5,65535,true,20000,true))
    assert(report.resultVertexCount<=65535 and report.resultTriangleCount<report.sourceTriangleCount)
    assert(mesh:getTexture(1,1)==texture and mesh:check())
    for subset=1,mesh:getTotalSubset(1) do
        for _,v in ipairs(mesh:getVertex(1,subset,1,mesh:getTotalVertex(1,subset))) do
            assert(math.abs(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz-1)<1e-4,'non-unit normal')
        end
    end
    local output=tUtil.getTemporaryFilePath('.msh');paths[#paths+1]=output
    assert(mesh:save(output,false,false,true));meshDebug:fakeRelease(output)
    local restored=meshDebug:new();assert(restored:load(output))
    assert(fixture.signature(restored)==fixture.signature(mesh),'saved remesh differs')
    print('REMESH_IMPORT_OK vertices='..report.resultVertexCount..' triangles='..report.resultTriangleCount..
        ' target_reached='..report.remesh.target_reached)
end

function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
    local ok,err=coroutine.resume(task)
    if not ok then print('REMESH_IMPORT_FAIL '..debug.traceback(task,tostring(err))) end
    local timedOut=mbm.getTimeRun()-started>60
    if timedOut then print('REMESH_IMPORT_FAIL timeout') end
    if not ok or coroutine.status(task)=='dead' or timedOut then
        Cgal.shutdown()
        for _,path in ipairs(paths) do meshDebug:fakeRelease(path);os.remove(path) end
        mbm.quit()
    end
end
