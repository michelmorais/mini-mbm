--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
local init,loop,finish=onInitScene,onLoop,onEndScene
local Gen=require 'mesh_debug_normal_generator'
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local task,started
local function wait() repeat coroutine.yield() until not Gen.busy() end
local function commit(entry,index)
    return function(asset,info,project) return applyGeneratedNormalMap(entry,index,asset,info,project) end
end
local function test()
    local root=(os.getenv('TEMP') or os.getenv('TMPDIR') or '/tmp'):gsub('\\','/')..'/mesh-debug-normal-generation'
    assert(mbm.createDirectories(root));mbm.addPath(root)
    local bytes={}
    for y=0,15 do for x=0,15 do local v=x*16;bytes[#bytes+1]=string.char(v,v,v,255) end end
    local source=root..'/height.png';assert(mbm.writeImagePixels(source,table.concat(bytes),16,16))
    local asset=meshDebug:new();asset:setType('mesh');asset:setModeDraw('TRIANGLES')
    for f=1,2 do
        asset:addFrame(3)
        for s=1,2 do
            asset:addSubSet(f)
            assert(asset:addVertex(f,s,{{x=0,y=0,z=0,nx=0,ny=0,nz=1,u=0,v=0},
                {x=1,y=0,z=0,nx=0,ny=0,nz=1,u=1,v=0},
                {x=0,y=1,z=0,nx=0,ny=0,nz=1,u=0,v=1}}))
            assert(asset:addIndex(f,s,{1,2,3}));asset:setTexture(f,s,source)
        end
    end
    asset:addAnim('Static',1,2,1,0)
    local path=root..'/input.msh';assert(asset:save(path,false,false,true));assert(addMeshToTable(path))
    local entry=tLoadedMeshes[#tLoadedMeshes];local index=#tLoadedMeshes
    local state=Gen.state(entry);state.source=source;state.frame=1;state.subset=1
    state.settings.strength=3;state.settings.convention='-Y'
    local original=entry.meshDebug
    assert(Gen.start(entry,commit(entry,index)));assert(entry.meshDebug==original);wait()
    assert(not state.error,state.error);assert(entry.meshDebug~=original and entry.modified)
    assert(entry.meshDebug:getMaterialTexture(1,1,'normal'))
    assert(not entry.meshDebug:getMaterialTexture(1,2,'normal') and not entry.meshDebug:getMaterialTexture(2,1,'normal'))
    local convention,strength=entry.meshDebug:getNormalMapSettings(1,1);assert(convention=='-Y' and strength==1)
    assert(entry.meshDebug:getVertex(1,1,2).x==1 and entry.meshDebug:getTotalIndex(1,1)==3)
    assert(entry.meshDebug:prepareNormalMap(1,1,'preserve').reused)
    assert(IO.exists(state.lastPNG))
    assert(transformRestoreUndo(entry,index));assert(not entry.meshDebug:getMaterialTexture(1,1,'normal'))
    original=entry.meshDebug
    assert(Gen.start(entry,commit(entry,index)));Gen.cancel();wait()
    assert(entry.meshDebug==original and #state.sessions==1,'Cancelled job published an asset')
    state.source=root..'/missing.png'
    assert(Gen.start(entry,commit(entry,index)));wait()
    assert(state.error and entry.meshDebug==original,'Failed job changed the mesh')
    state.source=source;state.frame=0;state.subset=0
    assert(Gen.start(entry,commit(entry,index)));wait();assert(not state.error,state.error)
    for f=1,2 do for s=1,2 do assert(entry.meshDebug:getMaterialTexture(f,s,'normal')) end end
    -- Build an Image Mesh project through its actual worktree, then edit its normal parameters.
    local project=Model.new(source,16,16)
    project.defaults.columns=4;project.defaults.rows=4;project.defaults.relief=3
    local region=Model.add(project,'rectangle',0,0,16,16)
    local projectPath=root..'/source.imesh';assert(IO.save(project,projectPath,tUtil.save))
    assert(addImageMeshProject(projectPath))
    local owner=tImageMeshProjects[#tImageMeshProjects]
    assert(tImageMeshWorktree.ensure(owner))
    repeat coroutine.yield() until owner.status~='generating' and owner.status~='queued'
    assert(owner.status=='ready',owner.error)
    local child=owner.meshEntries[1];assert(child)
    local childIndex=tMeshEntryIndex[child]
    local draft=Gen.state(child)
    draft.values.reliefMode='normal';draft.values.normalMapResidual=true;draft.values.normalMapBasis=true
    draft.values.normalMapStrength=2;draft.values.normalMapBlur=1
    local before=child.meshDebug
    assert(Gen.start(child,commit(child,childIndex)));wait();assert(not draft.error,draft.error)
    assert(child.meshDebug~=before and child.meshDebug:getMaterialTexture(1,1,'normal'))
    local updated=Model.options(owner.project,Model.region(owner.project,child.imageMeshRegionId))
    assert(updated.reliefMode=='normal' and updated.normalMapResidual and updated.normalMapStrength==2)
    local saved=root..'/edited.imesh';assert(IO.save(owner.project,saved,tUtil.save))
    assert(IO.load(saved).regions[1].overrides.normalMapStrength==2)
    assert(IO.load(projectPath).defaults.reliefMode=='geometry','Source project was overwritten')
    assert(transformRestoreUndo(child,childIndex))
    assert(Model.options(owner.project,Model.region(owner.project,child.imageMeshRegionId)).reliefMode=='geometry')
    assert(draft.values.reliefMode=='geometry')
    -- Exercise both panels and idle frames without starting generation.
    local calls=0;local start=mbm.startImageMesh
    mbm.startImageMesh=function(...) calls=calls+1;return start(...) end
    for i=1,8 do
        local open=tImGui.Begin('Normal generation panels',false,0)
        if open then
            Gen.panel(entry,index,commit(entry,index),function() return false end)
            tImGui.Separator()
            Gen.panel(child,childIndex,commit(child,childIndex),function() return false end)
        end
        tImGui.End();coroutine.yield()
    end
    mbm.startImageMesh=start;assert(calls==0 and not Gen.busy(),'Idle panel started generation')
    local generatedPNG=state.lastPNG
    local savedMesh=root..'/generated.msh';assert(entry.meshDebug:save(savedMesh,false,false,true))
    local reloaded=meshDebug:new();assert(reloaded:load(savedMesh))
    assert(reloaded:getMaterialTexture(1,1,'normal') and reloaded:prepareNormalMap(1,1,'preserve').reused)
    local paths={}
    for _,session in ipairs(state.sessions) do for _,path in ipairs(session.paths) do paths[#paths+1]=path end end
    -- Removing the entry must release its private files, including earlier undo generations.
    removeMeshFromTable(index)
    assert(not IO.exists(generatedPNG),'Generated PNG leaked after removal')
    for _,path in ipairs(paths) do assert(not IO.exists(path),'Temporary mesh leaked after removal') end
    print('MESH DEBUG NORMAL GENERATOR PASS')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
    loop(delta)
    if coroutine.status(task)=='dead' then mbm.quit();return end
    local ok,err=coroutine.resume(task)
    if not ok then print('MESH DEBUG NORMAL GENERATOR FAIL '..tostring(err));mbm.quit() end
    if mbm.getTimeRun()-started>60 then print('MESH DEBUG NORMAL GENERATOR FAIL timeout');mbm.quit() end
end
function onEndScene() finish() end
