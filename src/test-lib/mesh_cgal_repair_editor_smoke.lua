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



-- Synthetic non-manifold fixture. Real saved meshes use the roundtrip smoke.
package.path='editor/?.lua;'..package.path
dofile('editor/mesh_debug.lua')
local Cgal=require 'mesh_cgal'
local Obj=require 'mesh_cgal_obj'
local modes=require 'mesh_simplify_modes'
local helper=dofile('src/test-lib/mesh_simplification_fixture.lua')
local init,loop=onInitScene,onLoop
local task,started
local temporary={}
local function await(entry)
    while entry.tSimplifyState.running do simplifyResume(entry);coroutine.yield() end
end
local function test()
    assert(Cgal.setRemeshPath(assert(os.getenv('MBM_CGAL_REMESH_EXECUTABLE')),false))
    assert(Cgal.setRepairPath(assert(os.getenv('MBM_CGAL_REPAIR_EXECUTABLE')),false))
    if os.getenv('MBM_CGAL_EXECUTABLE') then assert(Cgal.setPath(os.getenv('MBM_CGAL_EXECUTABLE'),false)) end
    local model=require 'image_mesh_model'
    assert(model.defaults.remeshRepairTopology==true and model.defaults.cgalRepairTopology==true and model.defaults.remeshTargetEnabled==nil)
    assert(model.defaults.remeshIterations==10 and model.defaults.remeshFeatureAngle==14.5)
    model.validateOptions({remeshRepairTopology=true,cgalRepairTopology=true,remeshIterations=50},false)
    assert(not pcall(model.validateOptions,{remeshIterations=51},false))
    assert(not pcall(model.validateOptions,{remeshRepairTopology=1},false))
    -- Exercise repair defaults and feature-angle precision.
    local button,drag=tImGui.SmallButton,tImGui.DragFloat
    local opened=tImGui.Begin('Remesh shortcuts test',false,0)
    local ok,err=pcall(function()
        assert(opened)
        local defaults={}
        modes.repairOption(defaults,'defaults')
        assert(defaults.remeshRepairTopology)
        modes.repairOption(defaults,'defaults-cgal','cgal');assert(defaults.cgalRepairTopology)
        defaults.remeshRepairTopology=false
        modes.repairOption(defaults,'explicit')
        assert(not defaults.remeshRepairTopology)
        tImGui.DragFloat=function(label,value,speed,minimum,maximum,format,flags)
            assert(label:find('remesh-feature-',1,true) and speed==.1 and format=='%.2f deg')
            drag(label,value,speed,minimum,maximum,format,flags)
            return true,14.55
        end
        local edge,iterations,angle,changed=modes.remeshSettings(.03,10,14.5,'precision')
        assert(edge==.03 and iterations==10 and angle==14.55 and changed)
    end)
    tImGui.SmallButton=button;tImGui.DragFloat=drag;tImGui.End();assert(ok,err)
    -- Identical attributes must not erase explicit topology splits on import.
    local objPath=tUtil.getTemporaryFilePath('.obj');temporary[#temporary+1]=objPath
    local f=assert(io.open(objPath,'w'))
    f:write('v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/1\nf 4/1 5/1 6/1\n');f:close()
    local data=Obj.read(objPath);assert(#data.groups[1].vertices==6)
    local path
    if not path then
        path=tUtil.getTemporaryFilePath('.msh');temporary[#temporary+1]=path
        local d=meshDebug:new();d:setType('mesh');d:setModeDraw('TRIANGLES');d:addFrame(3);d:addSubSet(1)
        assert(d:addVertex(1,1,{{x=0,y=0,z=0,u=0,v=0},{x=1,y=0,z=0,u=1,v=0},
            {x=0,y=1,z=0,u=0,v=1},{x=0,y=-1,z=0,u=0,v=-1},{x=0,y=0,z=1,u=0,v=0}}))
        assert(d:addIndex(1,1,{1,2,3,2,1,4,1,2,5}));d:setTexture(1,1,'#6080FFFF')
        d:setMaterialTexture(1,1,'normal','#8080FFFF');d:addAnim('Static',1,1,1,0);d:removeNormals()
        assert(d:save(path,false,false,true))
    end
    mbm.addPath(path:match('^(.*)[/\\]') or '.')
    assert(addMeshToTable(path))
    local e=tLoadedMeshes[#tLoadedMeshes];iSelectedMeshIndex=#tLoadedMeshes;e.sOpenNode='remesh'
    local original=helper.signature(e.meshDebug)
    local textureName=e.meshDebug:getTexture(1,1)
    local normalName=e.meshDebug:getMaterialTexture(1,1,'normal')
    e.tSimplifyState={scope='frame',selectedFrame=1,mode='remesh',ratio=.9,
        remeshEdgeLengthFraction=.2,remeshIterations=10,remeshFeatureAngle=14.5,remeshRepairTopology=false,
        remeshTargetEnabled=false,remeshTargetTriangles=20000}
    assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
    assert(e.tSimplifyState.lastError and e.tSimplifyState.lastError:find('OBJ is not an oriented manifold',1,true))
    assert(helper.signature(e.meshDebug)==original,'failure changed visible source')
    -- Explicit repair ignores the checkbox and does not remesh. Original
    -- corner attributes must survive, without the flat-normal vertex explosion.
    local sourceVertices=e.meshDebug:getVertex(1,1,1,e.meshDebug:getTotalVertex(1,1))
    local sourceIndices=e.meshDebug:getIndex(1,1)
    assert(simplifyRepairNow(e));await(e)
    local repaired=assert(e.tSimplifyState.report,e.tSimplifyState.lastError)
    assert(e.tSimplifyBackup.revertPrefix=='cgal_repair')
    assert(e.simplifyComparison.operation=='repair')
    assert(repaired.backend=='repair' and repaired.repair.repair_split_vertices>0)
    assert(repaired.sourceTriangleCount==repaired.resultTriangleCount)
    assert(e.meshDebug:getTexture(1,1)==textureName and e.meshDebug:getMaterialTexture(1,1,'normal')==normalName)
    local resultVertices=e.meshDebug:getVertex(1,1,1,e.meshDebug:getTotalVertex(1,1))
    local resultIndices=e.meshDebug:getIndex(1,1)
    assert(#sourceIndices==#resultIndices)
    for i,index in ipairs(sourceIndices) do
        local a,b=sourceVertices[index],resultVertices[resultIndices[i]]
        for _,key in ipairs({'x','y','z','u','v','nx','ny','nz'}) do assert(a[key]==b[key],'repair changed '..key) end
    end
    -- A repeated repair must not dirty a saved mesh or replace its undo/comparison.
    local repairedAsset,backup,comparison=e.meshDebug,e.tSimplifyBackup,e.simplifyComparison
    e.modified=false
    assert(simplifyRepairNow(e));await(e)
    assert(e.tSimplifyState.report.unchanged and e.modified==false)
    assert(e.meshDebug==repairedAsset and e.tSimplifyBackup==backup and e.simplifyComparison==comparison)
    e.modified=true
    assert(simplifyRepairNow(e));await(e)
    assert(e.modified==true and e.tSimplifyBackup==backup,'no-op cleared existing edits or undo')
    assert(simplifyRestoreBackup(e,iSelectedMeshIndex));assert(helper.signature(e.meshDebug)==original)
    assert(simplifyRepairNow(e));assert(simplifyCancel(e));await(e)
    assert(helper.signature(e.meshDebug)==original)
    e.tSimplifyState.remeshRepairTopology=true
    assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
    assert(e.tSimplifyBackup.revertPrefix=='cgal_remesh')
    local report=assert(e.tSimplifyState.report,e.tSimplifyState.lastError)
    assert(report.remesh.repair_enabled==1 and report.remesh.repair_split_vertices>0)
    assert(e.meshDebug:getTexture(1,1)==textureName and e.meshDebug:getMaterialTexture(1,1,'normal')==normalName)
    assert(e.meshDebug:check())
    print('REPAIR_RESULT '..report.sourceTriangleCount..' -> '..report.resultTriangleCount..' split='..report.remesh.repair_split_vertices)
    assert(simplifyRestoreBackup(e,iSelectedMeshIndex));assert(helper.signature(e.meshDebug)==original)
    assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));assert(simplifyCancel(e));await(e)
    assert(helper.signature(e.meshDebug)==original,'cancel changed visible source')
    if os.getenv('MBM_CGAL_EXECUTABLE') then
        e.tSimplifyState.mode='cgal';e.sOpenNode='simplification'
        e.tSimplifyState.cgalRepairTopology=false
        assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
        assert(e.tSimplifyState.lastError and e.tSimplifyState.lastError:find('OBJ is not an oriented manifold',1,true))
        assert(helper.signature(e.meshDebug)==original)
        e.tSimplifyState.cgalRepairTopology=true
        assert(simplifyApply(e,e.meshDebug,iSelectedMeshIndex));await(e)
        local planar=assert(e.tSimplifyState.report,e.tSimplifyState.lastError)
        assert(planar.cgal.repair_enabled==1 and planar.cgal.repair_split_vertices>0)
        assert(e.meshDebug:check())
        if e.tSimplifyBackup then assert(simplifyRestoreBackup(e,iSelectedMeshIndex)) end
        assert(helper.signature(e.meshDebug)==original)
    end
    for _,p in ipairs(temporary) do meshDebug:fakeRelease(p);os.remove(p) end
    print('CGAL REPAIR EDITOR SMOKE OK')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
    loop(delta)
    if coroutine.status(task)=='dead' then mbm.quit();return end
    local ok,e=coroutine.resume(task)
    if not ok then print('CGAL REPAIR EDITOR SMOKE FAIL '..debug.traceback(task,tostring(e)));mbm.quit() end
    if mbm.getTimeRun()-started>60 then print('CGAL REPAIR EDITOR SMOKE FAIL timeout');mbm.quit() end
end
