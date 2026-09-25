--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

]]--

package.path='editor/?.lua;'..package.path
dofile('editor/mesh_debug.lua')
local init,loop=onInitScene,onLoop
local entry,start,failed,builds,initialBuilds
local sourcePath='/tmp/mini-mbm-auto-capture-smoke.msh'
local exportPath='/tmp/mini-mbm-auto-capture-result.msh'
local function makeMesh(indexed,weighted)
    local d=meshDebug:new(); d:setType('mesh'); d:setModeDraw('TRIANGLES')
    d:addFrame(3)
    local function v(x,y) return {x=x,y=y,z=0,nx=0,ny=0,nz=1,u=x/100,v=y/100} end
    local vertices={v(0,0),v(20,0),v(20,20),v(0,20),v(60,0),v(80,0),v(60,20)}
    local indices={1,2,3,1,3,4,5,6,7}
    if not indexed then
        local expanded={}; for _,i in ipairs(indices) do expanded[#expanded+1]=vertices[i] end
        vertices=expanded
    end
    d:addSubSet(1); assert(d:addVertex(1,1,vertices))
    if indexed then assert(d:addIndex(1,1,indices)) end
    d:setTexture(1,1,'#FF0000FF')
    d:addAnim('Static',1,1,1,0)
    if weighted then
        d:initializeSkeletalSkeleton('root',0,0,0,1,1)
        d:addSkeletalBone(1,'child',60,0,0,1,1)
        d:initializeSkeletalVertexWeights(1)
        local edits={}
        for i,vertex in ipairs(vertices) do edits[#edits+1]={i,vertex.x>=60 and 'child' or 'root',1} end
        assert(d:setSkeletalVertexWeightsBatch(edits))
    end
    assert(d:check())
    return d
end
local function verify(d,expectedSubsets,weighted)
    assert(d:getTotalSubset(1)==expectedSubsets,'subset count')
    local totalIndices,global=0,0
    for s=1,d:getTotalSubset(1) do
        assert(d:getTexture(1,s)=='#FF0000FF','texture')
        totalIndices=totalIndices+d:getTotalIndex(1,s)
        for vi=1,d:getTotalVertex(1,s) do
            local v=d:getVertex(1,s,vi); global=global+1
            assert(math.abs(v.u-v.x/100)<0.00001 and math.abs(v.v-v.y/100)<0.00001,'UV')
            if weighted then
                local name,weight=d:getSkeletalVertexWeight(global)
                assert(name==(v.x>=60 and 'child' or 'root') and weight==1,'weight remap')
            end
        end
    end
    assert(totalIndices==9,'every source triangle must survive')
    assert(d:check())
end
local function tests()
    init()
    for _,indexed in ipairs({true,false}) do
        for _,minimum in ipairs({1,2,4}) do
            local d=makeMesh(indexed,true)
            local e={info={hasNormal=true},tSplitCaptures={}}
            local analysis=assert(splitCaptureAnalyze(e,d,nil,{mode='edges',tolerance=0,minFaces=minimum}))
            local result=analysis.resolved[1]
            assert(result.islands==2)
            assert(result.faces==(minimum==1 and 3 or (minimum==2 and 2 or 0)))
            if minimum<4 then
                assert(splitCaptureApply(e,d,result))
                verify(d,2,true)
                assert(d:save(exportPath,false,false))
                local reloaded=meshDebug:new(); assert(reloaded:load(exportPath)); verify(reloaded,2,true)
                meshDebug:fakeRelease(exportPath); os.remove(exportPath)
            else assert(d:getTotalSubset(1)==1) end
        end
    end
    -- Manual capture still combines all accepted islands into one subset.
    local d=makeMesh(true,false)
    local e={info={hasNormal=true}}
    local box={aabbMin={x=-1,y=-1,z=-1},aabbMax={x=100,y=100,z=1}}
    local manual=assert(splitCaptureAnalyze(e,d,box))
    assert(not manual.autoCapture and manual.resolved[1].subsets==1)
    assert(splitCaptureApply(e,d,manual.resolved[1])); verify(d,1,false)
    -- Excluded source subsets and prior captured signatures are respected.
    local excluded=assert(splitCaptureAnalyze({tCheckedRemove={[101]=false}},d,nil,{mode='edges',tolerance=0,minFaces=1}))
    assert(excluded.resolved[1].faces==0)
    local already=assert(splitCaptureAnalyze(e,d,nil,{mode='edges',tolerance=0,minFaces=1}))
    assert(already.resolved[1].faces==0)
    -- Full commit + revert, and reject a stale analysis without replacing the source mesh.
    d=makeMesh(true,true); assert(d:save(sourcePath,false,false))
    addMeshToTable(sourcePath); selectMeshIndex(1); entry=tLoadedMeshes[1]
    entry.sOpenNode='frameNode'; entry.tSplitCapture={active=false,initialized=false}; entry.tSplitCaptures={}
    local analysis=assert(splitCaptureAnalyze(entry,entry.meshDebug,nil,{mode='edges',tolerance=0,minFaces=1}))
    splitCaptureCommitAnalysis(entry,entry.meshDebug,1,entry.tSplitCapture,analysis.resolved[1])
    verify(entry.meshDebug,2,true); assert(entry.tSplitCaptureBackup)
    splitCaptureRevert(entry,1); assert(entry.meshDebug:getTotalSubset(1)==1)
    local unchanged=entry.meshDebug
    local stale=assert(splitCaptureAnalyze(entry,unchanged,nil,{mode='edges',tolerance=0,minFaces=1}))
    unchanged:translateFrame(1,1,0,0,1)
    splitCaptureCommitAnalysis(entry,unchanged,1,entry.tSplitCapture,stale.resolved[1])
    assert(entry.meshDebug==unchanged and entry.tSplitCaptureBackup==nil,'stale analysis')
    -- A backup can expand a relative texture path. Validate against the live source,
    -- not the serialized working copy, for the manual Start Capture -> Apply flow.
    d=makeMesh(true,false)
    mbm.addPath('src/test-lib/')
    d:setTexture(1,1,'Crate_1.png')
    local manualEntry={meshDebug=d,info={hasNormal=true},tSplitCapture={active=false}}
    local capture=assert(splitCaptureAnalyze(manualEntry,d,box))
    splitCaptureCommitAnalysis(manualEntry,d,1,manualEntry.tSplitCapture,capture.resolved[1])
    assert(manualEntry.meshDebug~=d and manualEntry.tSplitCaptureBackup,
        'manual capture rejected unchanged mesh after texture path serialization')
    assert(manualEntry.tSplitCapture.lastFaces==3)
    splitCaptureDiscardBackup(manualEntry)
    d=makeMesh(true,false)
    manualEntry={meshDebug=d,info={hasNormal=true},tSplitCapture={active=false}}
    capture=assert(splitCaptureAnalyze(manualEntry,d,box))
    d:setTexture(1,1,'Crate_1.png')
    splitCaptureCommitAnalysis(manualEntry,d,1,manualEntry.tSplitCapture,capture.resolved[1])
    assert(manualEntry.meshDebug==d and not manualEntry.tSplitCaptureBackup,
        'a real texture edit must invalidate manual capture')
    -- Render the real auto-capture result UI, with an idle-analysis counter.
    builds=0; local build=tMeshIslands.build
    tMeshIslands.build=function(...) builds=builds+1; return build(...) end
    entry.tSplitCapture.analysis=assert(splitCaptureAnalyze(entry,entry.meshDebug,nil,{mode='edges',tolerance=0,minFaces=1}))
    entry.tSplitCapture.analysis.showIslandCenters=true
    initialBuilds=builds
    bCameraMode3D=true
    iLastPreviewedIndex=0
    print('AUTO_CAPTURE_NATIVE_OK')
end
function onInitScene()
    start=mbm.getTimeRun()
    local ok,err=pcall(tests)
    if not ok then failed=err; print('AUTO_CAPTURE_FAIL '..tostring(err)); mbm.quit() end
end
function onLoop(delta)
    if failed then mbm.quit(); return end
    loop(delta)
    if mbm.getTimeRun()-start>4 then
        assert(builds==initialBuilds,'idle UI reran island detection')
        os.remove(sourcePath); os.remove(exportPath)
        print('AUTO_CAPTURE_EDITOR_OK'); mbm.quit()
    end
end
