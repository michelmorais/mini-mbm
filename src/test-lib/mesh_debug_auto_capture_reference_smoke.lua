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
local start,failed,entry,initialBuilds,builds,boxGeneration
local function vertexKey(v)
    local values={v.x,v.y,v.z,v.nx or 0,v.ny or 0,v.nz or 0,v.u or 0,v.v or 0}
    for i,value in ipairs(values) do if value==0 then values[i]=0 end end
    return string.pack('<ffffffff',table.unpack(values))
end
local function groups(d)
    assert(d:getTotalFrame()==1,'reference fixture must have one frame')
    local result={}
    for subset=1,d:getTotalSubset(1) do
        local vertices=d:getVertex(1,subset,1,d:getTotalVertex(1,subset))
        local indices=d:getIndex(1,subset)
        local triangles={}
        for i=1,#indices,3 do
            local corners={vertexKey(vertices[indices[i]]),vertexKey(vertices[indices[i+1]]),vertexKey(vertices[indices[i+2]])}
            table.sort(corners)
            triangles[#triangles+1]=table.concat(corners)
        end
        table.sort(triangles)
        result[#result+1]={key=table.concat(triangles),faces=#triangles,subset=subset}
    end
    return result
end
local function compare(d,expected)
    local actual=groups(d); assert(#actual==6,'expected six subsets')
    local used={}
    for _,a in ipairs(actual) do
        local found
        for i,e in ipairs(expected) do
            if not used[i] and a.key==e.key then found=i; used[i]=true; break end
        end
        assert(found,'subset does not match any manual subset: '..a.subset..', faces='..a.faces)
        print('REFERENCE_MATCH',a.subset,expected[found].subset,a.faces)
    end
    return actual
end
function onInitScene()
    start=mbm.getTimeRun()
    local ok,err=pcall(function()
        init()
        local base=assert(os.getenv('MBM_AUTO_SPLIT_FIXTURE_DIR'),'Set MBM_AUTO_SPLIT_FIXTURE_DIR to the fixture folder')..'/'
        local output=assert(os.getenv('MBM_AUTO_SPLIT_OUTPUT'),'Set MBM_AUTO_SPLIT_OUTPUT to a new output mesh path')
        mbm.addPath(base)
        local reference=meshDebug:new(); assert(reference:load(base..'n1-v01-ee31480b-msh01-manual-splited.msh'))
        local expected=groups(reference); assert(#expected==6)
        addMeshToTable(base..'n1-v01-ee31480b-msh01.msh'); selectMeshIndex(1)
        entry=tLoadedMeshes[1]; assert(entry.meshDebug:getTotalSubset(1)==1)
        local sp={active=false,initialized=false,autoOptions={mode='vertices',tolerance=3,minFaces=1}}
        entry.tSplitCapture=sp; entry.tSplitCaptures={}
        local analysis=assert(splitCaptureAnalyze(entry,entry.meshDebug,nil,sp.autoOptions))
        assert(analysis.resolved[1].subsets==6 and analysis.resolved[1].faces==2861)
        splitCaptureCommitAnalysis(entry,entry.meshDebug,1,sp,analysis.resolved[1])
        assert(entry.tSplitCaptureBackup,'capture must be committed')
        compare(entry.meshDebug,expected)
        assert(entry.meshDebug:save(output,false,false),'save output')
        local reloaded=meshDebug:new(); assert(reloaded:load(output)); compare(reloaded,expected)
        assert(reloaded:check())
        splitCaptureRevert(entry,1); assert(entry.meshDebug:getTotalSubset(1)==1)
        -- Keep the result UI visible without committing the user's source file.
        builds=0; local build=tMeshIslands.build
        tMeshIslands.build=function(...) builds=builds+1; return build(...) end
        sp.analysis=assert(splitCaptureAnalyze(entry,entry.meshDebug,nil,sp.autoOptions))
        assert(sp.analysis.showIslandBoxes == true, 'auto capture must enable boxes')
        local resolved = sp.analysis.resolved[1]
        updateSplitCaptureIslandMarkers(entry,1,sp.analysis,resolved)
        assert(#entry.tSplitCaptureIslandBoxes==6 and #entry.tSplitCaptureIslandMarkers==0)
        for i,box in ipairs(entry.tSplitCaptureIslandBoxes) do
            local bounds=resolved.islandMarkers[i]
            assert(box.width==bounds.width and box.height==bounds.height and box.depth==bounds.depth)
            local pos=box.tShape:getPos()
            assert(pos.x==bounds.x and pos.y==bounds.y and pos.z==bounds.z)
            assert(next(box.tAxisEdgeLines)==nil and next(box.tAxisFaceShapes)==nil,'read-only box overlays')
        end
        local previous=entry.tSplitCaptureIslandBoxes[1]
        sp.analysis.showIslandBoxes=false
        updateSplitCaptureIslandMarkers(entry,1,sp.analysis,resolved)
        assert(entry.tSplitCaptureIslandBoxes==nil and previous.tShape==nil,'disable boxes')
        sp.analysis.showIslandCenters=true
        updateSplitCaptureIslandMarkers(entry,1,sp.analysis,resolved)
        assert(#entry.tSplitCaptureIslandMarkers==6 and #entry.tSplitCaptureIslandBoxes==0,'independent centers')
        sp.analysis.showIslandCenters=false; sp.analysis.showIslandBoxes=true
        updateSplitCaptureIslandMarkers(entry,1,sp.analysis,resolved)
        assert(#entry.tSplitCaptureIslandBoxes==6,'restore boxes')
        boxGeneration=entry.iSplitCaptureIslandMarkerGeneration
        updateSplitCaptureIslandMarkers(entry,1,sp.analysis,resolved)
        assert(entry.iSplitCaptureIslandMarkerGeneration==boxGeneration,'idle boxes rebuilt')
        initialBuilds=builds; entry.sOpenNode='frameNode'
        bCameraMode3D=true; iLastPreviewedIndex=0
        entry.cam3d={azimuth=0,elevation=0,distance=320,fx=0,fy=0,fz=0}
        print('AUTO_CAPTURE_REFERENCE_OK '..output)
    end)
    if not ok then failed=err; print('AUTO_CAPTURE_REFERENCE_FAIL '..tostring(err)); mbm.quit() end
end
function onLoop(delta)
    if failed then mbm.quit(); return end
    loop(delta)
    if mbm.getTimeRun()-start>5 then
        assert(builds==initialBuilds,'idle UI rebuilt islands')
        assert(entry.iSplitCaptureIslandMarkerGeneration==boxGeneration,'idle UI rebuilt boxes')
        destroySplitCaptureIslandMarkers(entry)
        assert(entry.tSplitCaptureIslandBoxes==nil,'cleanup boxes')
        print('AUTO_CAPTURE_REFERENCE_UI_OK'); mbm.quit()
    end
end
