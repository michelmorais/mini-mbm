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


-- Repaired indices must survive both OBJ interchange and an MSH save/load cycle.
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
    Cgal.setPath(assert(os.getenv('MBM_CGAL_EXECUTABLE')),false)
    local d=meshDebug:new();d:setType('mesh');d:setModeDraw('TRIANGLES');d:addFrame(3);d:addSubSet(1)
    assert(d:addVertex(1,1,{{x=0,y=0,z=0,u=0,v=0},{x=1,y=0,z=0,u=1,v=0},
        {x=0,y=1,z=0,u=0,v=1},{x=0,y=-1,z=0,u=0,v=-1},{x=0,y=0,z=1,u=0,v=0}}))
    assert(d:addIndex(1,1,{1,2,3,2,1,4,1,2,5}));d:setTexture(1,1,'#6080FFFF');d:addAnim('Static',1,1,1,0);d:removeNormals()
    local meshes={d}
    local sample=os.getenv('MBM_CGAL_REPAIR_MESH')
    if sample then
        mbm.addPath(sample:match('^(.*)[/\\]') or '.')
        local real=meshDebug:new();assert(real:load(sample));meshes[#meshes+1]=real
    end
    for i,source in ipairs(meshes) do
        local normals=i~=1
        local first=await(Cgal.startRepair(source,nil,1,65535,normals))
        if i==1 then assert(first.repair.repair_split_vertices>0) end
        local signature=fixture.signature(source)
        local path=tUtil.getTemporaryFilePath('.msh');paths[#paths+1]=path
        assert(source:save(path,false,false,true));meshDebug:fakeRelease(path)
        local reopened=meshDebug:new();assert(reopened:load(path))
        assert(fixture.signature(reopened)==signature,'MSH save/load changed vertices or indices')
        local second=await(Cgal.startRepair(reopened,nil,1,65535,normals))
        assert(second.repair.repair_split_vertices==0,'repeat repair split vertices again')
        assert(second.repair.repair_reversed_faces==0,'repeat repair reversed faces again')
        assert(fixture.signature(reopened)==signature,'repeat repair changed geometry')
        -- Planar must accept these separated indices without another repair pass.
        await(Cgal.start(reopened,nil,1,10,.05,65535,normals,false,nil,false))
        print('REPAIR_ROUNDTRIP mesh='..i..' first_split='..first.repair.repair_split_vertices..' second_split=0')
    end
    print('CGAL_REPAIR_ROUNDTRIP_OK')
end
function onInitScene() started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop()
    local ok,err=coroutine.resume(task)
    if not ok then print('CGAL_REPAIR_ROUNDTRIP_FAIL '..debug.traceback(task,tostring(err))) end
    if not ok or coroutine.status(task)=='dead' or mbm.getTimeRun()-started>60 then
        Cgal.shutdown()
        for _,path in ipairs(paths) do meshDebug:fakeRelease(path);os.remove(path) end
        mbm.quit()
    end
end
