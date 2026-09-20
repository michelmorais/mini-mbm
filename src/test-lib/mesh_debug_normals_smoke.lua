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
    local apply=upvalue(showApplyAllWindow,'applyAllRecomputeNormalsBulk')
    assert(apply('mesh').success==1)
    for f=1,2 do for s=1,2 do
        local v=d:getVertex(f,s,1);assert(math.abs(v.nx-.6)<1e-6 and math.abs(v.nz-.8)<1e-6)
        assert(d:getVertex(f,s,3).nz==1)
    end end
    entry.modified=false;assert(apply('mesh').skipped==1);assert(not entry.modified)
    -- Save uses the same repair policy and does not silently recalculate.
    local save=upvalue(showApplyAllWindow,'applyAllSave')
    assert(save('mesh',true).success==1)
    local copy=meshDebug:new();assert(copy:load(entry.fileName));assert(math.abs(copy:getVertex(2,2,1).nx-.6)<1e-6)
    tApplyAllWin.normalMethod=2;assert(apply('mesh').success==1)
    for f=1,2 do for s=1,2 do assert(d:getVertex(f,s,1).nz==1) end end
    -- A failed file must not prevent processing other loaded targets.
    tLoadedMeshes={{info={type='mesh'},fileName='/tmp/failure.msh'},entry}
    local runner=upvalue(apply,'runApplyAllOperation')
    local summary=runner('mesh','failure isolation',function(e)
        if e~=entry then error('intentional fixture failure') end
        return 'success'
    end)
    assert(summary.failed==1 and summary.success==1)
    local plain=meshDebug:new();plain:setType('mesh');plain:addFrame(3);plain:addSubSet(1)
    plain:addVertex(1,1,{{x=0,y=0,z=0},{x=1,y=0,z=0},{x=0,y=1,z=0}})
    local geo=computeGeoNormalsForSubset(plain,1,1);assert(geo[1].z==1)
    tLoadedMeshes={entry};tApplyAllWin.open=true
    print('MESH DEBUG NORMALS FRAMES / SUBSETS / REPAIR / SAVE / UNIFORM OK')
end
local started
function onInitScene()
    local ok,err=pcall(run);if not ok then print('NORMALS SMOKE FAIL '..tostring(err));mbm.quit();return end
    started=mbm.getTimeRun()
end
function onLoop()
    if not started then return end
    showApplyAllWindow()
    tImGui.Begin('Single normal methods',false,0)
    tMeshNormals.draw(tImGui,tLang,tLoadedMeshes[1],'normalSingleTest')
    tImGui.End()
    if mbm.getTimeRun()-started>3 then print('MESH DEBUG NORMALS UI OK');mbm.quit() end
end
