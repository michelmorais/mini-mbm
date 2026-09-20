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
dofile('editor/mesh_debug.lua')
local init=onInitScene
local editorLoop=onLoop
local surfaceCalls,initialSurfaceCalls=0,0
local surfaces=tMeshNormals.surfaces
tMeshNormals.surfaces=function(...) surfaceCalls=surfaceCalls+1;return surfaces(...) end
local function upvalue(fn,name)
    for i=1,100 do local n,v=debug.getupvalue(fn,i);if n==name then return v end;if not n then break end end
    error('missing callback '..name)
end
local function run()
    init()
    local d=meshDebug:new();d:setType('mesh');d:setModeDraw('TRIANGLES')
    for f=1,2 do
        d:addFrame(3)
        for s=1,2 do
            d:addSubSet(f)
            local v={{x=0,y=0,z=0,nx=.6,ny=0,nz=.8,u=0,v=0},
                {x=1,y=0,z=0,nx=0,ny=0,nz=1,u=1,v=0},
                {x=0,y=1,z=0,nx=0,ny=0,nz=0,u=0,v=1}}
            assert(d:addVertex(f,s,v));assert(d:addIndex(f,s,{1,2,3}));d:setTexture(f,s,'#FFFFFFFF')
        end
    end
    d:addAnim('Static',1,2,1,0)
    local entry={meshDebug=d,info={type='mesh',hasNormal=true,animation=1},fileName='/tmp/mesh-debug-normal-policy.msh'}
    tLoadedMeshes={entry};tApplyAllWin.normalMethod=1
    local prepare=upvalue(showApplyAllWindow,'applyAllRecomputeNormalsBulk')
    local review=tMeshNormals.preview
    -- Staging must never mutate authored data, including existing unsaved changes.
    entry.modified=true
    assert(prepare('mesh').success==1)
    assert(entry.meshDebug==d and d:getVertex(1,1,3).nz==0)
    assert(review.entry(entry).meshDebug:getVertex(1,1,3).nz==1)
    local temporary=review.pending.paths[1]
    assert(review.finish(false) and not review.pending)
    assert(entry.modified and entry.meshDebug==d and d:getVertex(1,1,3).nz==0)
    assert(not io.open(temporary,'rb'),'cancel leaked temporary file')
    -- The single-vertex and subset controls use the same transaction with narrower scope.
    entry.normalMethod=2
    assert(review.begin({{entry=entry,index=1}},entry,1,2,1,nil,pcall).success==1)
    local candidate=review.entry(entry).meshDebug
    assert(candidate:getVertex(1,2,1).nz==1 and candidate:getVertex(2,2,1).nx>.5)
    assert(candidate:getVertex(1,1,1).nx>.5)
    assert(review.finish(false))
    assert(review.begin({{entry=entry,index=1}},entry,1,2,nil,nil,pcall).success==1)
    candidate=review.entry(entry).meshDebug
    assert(candidate:getVertex(1,2,3).nz==1 and candidate:getVertex(2,2,3).nz==0)
    assert(review.finish(false))
    -- Batch preparation failure leaves even already-prepared targets untouched.
    local bad={info={type='mesh',hasNormal=true},fileName='/tmp/invalid-normal-source.msh'}
    assert(review.begin({{entry=entry,index=1},{entry=bad,index=2}},entry,nil,nil,nil,nil,pcall).failed==1)
    assert(not review.pending and entry.meshDebug==d and d:getVertex(1,1,3).nz==0)
    local another={meshDebug=d,info=entry.info,fileName='/tmp/normal-review-second.msh'}
    tLoadedMeshes={entry,another}
    assert(review.begin({{entry=entry,index=1},{entry=another,index=2}},entry,nil,nil,nil,nil,pcall).success==2)
    assert(#review.pending.items==2 and another.meshDebug==d)
    assert(review.finish(false));tLoadedMeshes={entry}
    local function apply(kind)
        local result=prepare(kind)
        if review.pending then assert(review.finish(true));d=entry.meshDebug end
        return result
    end
    assert(apply('mesh').success==1)
    for f=1,2 do for s=1,2 do
        local v=d:getVertex(f,s,1);assert(math.abs(v.nx-.6)<1e-6 and math.abs(v.nz-.8)<1e-6)
        assert(d:getVertex(f,s,3).nz==1)
    end end
    entry.modified=false;assert(apply('mesh').skipped==1);assert(not entry.modified)
    -- Save uses the same repair policy and does not silently recalculate.
    local saveOperation=upvalue(showApplyAllWindow,'applyAllSave')
    os.remove(entry.fileName)
    assert(saveOperation('mesh',true).success==1)
    assert(not io.open(entry.fileName,'rb'),'save ran before confirmation')
    assert(review.finish(false))
    assert(not io.open(entry.fileName,'rb'),'cancel wrote the authored file')
    local function save(kind,normals)
        local result=saveOperation(kind,normals)
        if review.pending then assert(review.finish(true));d=entry.meshDebug end
        return result
    end
    assert(save('mesh',true).success==1)
    local copy=meshDebug:new();assert(copy:load(entry.fileName));assert(math.abs(copy:getVertex(2,2,1).nx-.6)<1e-6)
    tApplyAllWin.normalMethod=2;assert(apply('mesh').success==1)
    for f=1,2 do for s=1,2 do assert(d:getVertex(f,s,1).nz==1) end end
    -- A failed file must not prevent processing other loaded targets.
    tLoadedMeshes={{info={type='mesh'},fileName='/tmp/failure.msh'},entry}
    local runner=upvalue(saveOperation,'runApplyAllOperation')
    local summary=runner('mesh','failure isolation',function(e)
        if e~=entry then error('intentional fixture failure') end
        return 'success'
    end)
    assert(summary.failed==1 and summary.success==1)
    local plain=meshDebug:new();plain:setType('mesh');plain:addFrame(3);plain:addSubSet(1)
    plain:addVertex(1,1,{{x=0,y=0,z=0},{x=1,y=0,z=0},{x=0,y=1,z=0}})
    local geo=computeGeoNormalsForSubset(plain,1,1);assert(geo[1].z==1)
    tLoadedMeshes={entry};tApplyAllWin.normalMethod=3;tApplyAllWin.normalSurfaceAngle=25
    for f=1,2 do for subset=1,2 do
        local v=d:getVertex(f,subset,1);v.nx=.6;v.nz=.8;d:setVertex(f,subset,1,v)
    end end
    assert(apply('mesh').success==1)
    for f=1,2 do for subset=1,2 do assert(d:getVertex(f,subset,1).nz==1) end end
    assert(save('mesh',true).success==1)
    entry.modified=true;iSelectedMeshIndex=1;iLastPreviewedIndex=0
    updatePreviewMesh();assert(tPreviewMesh,'isolated mesh preview failed')
    print('MESH DEBUG SURFACES ALL FRAMES / SUBSETS / SAVE / PREVIEW OK')
    -- Leave a real pending preview on screen, with original and candidate separated.
    tApplyAllWin.normalMethod=2
    local v=d:getVertex(1,1,1);v.nx=.6;v.nz=.8;d:setVertex(1,1,1,v)
    entry.cam3d={fx=0,fy=0,fz=0,distance=100,azimuth=0,elevation=0}
    assert(prepare('mesh').success==1)
    updatePreviewMesh();assert(tPreviewMesh)
    review.pending.original=true;iLastPreviewedIndex=0;updatePreviewMesh();assert(tPreviewMesh)
    review.pending.original=false;iLastPreviewedIndex=0;updatePreviewMesh()
    assert(entry.meshDebug==d and math.abs(d:getVertex(1,1,1).nx-.6)<1e-6)
    tApplyAllWin.open=true
    print('MESH DEBUG NORMALS FRAMES / SUBSETS / REPAIR / SAVE / UNIFORM OK')
end
local started
function onInitScene()
    local ok,err=pcall(run);if not ok then print('NORMALS SMOKE FAIL '..tostring(err));mbm.quit();return end
    initialSurfaceCalls=surfaceCalls;started=mbm.getTimeRun()
end
function onLoop()
    if not started then return end
    editorLoop(0.016)
    if mbm.getTimeRun()-started>3 then
        assert(surfaceCalls==initialSurfaceCalls,'surface reconstruction repeated while idle')
        assert(tMeshNormals.preview.finish(false));print('MESH DEBUG NORMALS PREVIEW / CANCEL / CONFIRM / UI / IDLE OK');mbm.quit()
    end
end
