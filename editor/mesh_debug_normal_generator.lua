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

local Model=require 'image_mesh_model'
local Normal=require 'image_mesh_normal_map'
local Height=require 'height_map_source'
local Generator=require 'normal_map_generator'
local Author=require 'normal_map_authoring'
local M={}
local active
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[mesh_debug_normal_generator] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end
local function L(key) return tLang.L('mdng_'..key) end
function M.state(entry)
    if not entry.normalGenerator then
        local owner=entry.imageMeshProjectOwner
        local values=owner and Model.options(owner.project,assert(Model.region(owner.project,entry.imageMeshRegionId))) or nil
        entry.normalGenerator={values=values,settings=Height.settings(),source='',frame=0,subset=0,sessions={}}
    end
    return entry.normalGenerator
end
local function cleanup(session)
    Normal.shutdown(session.worker)
    if session.texture then session.texture:release() end
    if session.png then os.remove(session.png) end
    for _,path in ipairs(session.paths) do meshDebug:fakeRelease(path);os.remove(path) end
end
local function temp(session)
    local path=tUtil.getTemporaryFilePath('.msh');session.paths[#session.paths+1]=path;return path
end
function M.start(entry,commit)
    assert(not active,L('busy'))
    local state=M.state(entry)
    -- Check other editor jobs only on this explicit action, never during idle drawing.
    for _,other in ipairs(tLoadedMeshes or {}) do
        assert(not (other.tSimplifyState or {}).running,L('busy'))
    end
    for _,project in ipairs(tImageMeshProjects or {}) do
        assert(project.status~='generating' and project.status~='cancelling' and project.status~='queued',L('busy'))
    end
    assert(not (entry.tSimplifyState or {}).running,L('busy'))
    local owner=entry.imageMeshProjectOwner
    assert(not owner or (owner.status~='generating' and owner.status~='cancelling' and owner.status~='queued'),L('busy'))
    local session={worker={},paths={},entry=entry,state=state,commit=commit,sourceAsset=entry.meshDebug}
    local ok,err=dpCall(function()
        if owner then
            session.project=Model.copy(owner.project)
            local region=assert(Model.region(session.project,entry.imageMeshRegionId))
            for key in pairs(Model.normalMapDefaults) do region.overrides[key]=state.values[key] end
            Model.validate(session.project)
        else
            assert(state.source~='',L('choose_source'))
            session.source=state.source;session.settings=Height.settings(state.settings)
            session.frame=state.frame;session.subset=state.subset
            local selection=Author.readSettings(entry.meshDebug,state.frame,state.subset)
            assert(not selection.invalid and #selection.targets>0,tLang.L('nm_settings_empty'))
            session.targets=selection.targets
            local path=temp(session);assert(entry.meshDebug:save(path,false,false,true))
            session.asset=meshDebug:new();assert(session.asset:load(path))
        end
    end)
    if not ok then cleanup(session);error(err,0) end
    state.error=nil
    session.task=coroutine.create(function()
        local asset,report
        if session.project then
            local region=assert(Model.region(session.project,entry.imageMeshRegionId))
            asset,report=require('image_mesh_build').generate(session.worker,session.project,region)
            assert(asset,report)
        else
            local bytes,w,h=mbm.readImagePixels(session.source);assert(bytes,w)
            local job=Generator.start(Height.image(bytes,w,h),session.settings)
            session.worker.normalJob=job
            repeat
                assert(not session.cancelled,'ime_generation_cancelled')
                job:step(.006);session.worker.generationProgress=job.progress
                coroutine.yield()
            until job.state~='running'
            assert(job.state=='completed',job.error)
            session.png=tUtil.getTemporaryFilePath('.png')
            assert(mbm.writeImagePixels(session.png,job.result.bytes,job.result.width,job.result.height))
            mbm.addPath(session.png:match('^(.*)[/\\]') or '.')
            session.texture=assert(mbm.loadTexture(session.png))
            session.worker.normalJob=nil
            asset=session.asset
            for _,target in ipairs(session.targets) do
                assert(not session.cancelled,'ime_generation_cancelled')
                assert(asset:setMaterialTexture(target.frame,target.subset,'normal',session.png))
                assert(asset:setNormalMapSettings(target.frame,target.subset,session.settings.convention,1))
                local prepared,err=asset:prepareNormalMap(target.frame,target.subset,'preserve');assert(prepared,err)
                coroutine.yield()
            end
        end
        assert(not session.cancelled,'ime_generation_cancelled')
        local path=temp(session)
        assert(require('image_mesh_texture_aliases').savePreview(asset,path,entry.fileName))
        local info=assert(meshDebug:getInfo(path))
        assert(entry.meshDebug==session.sourceAsset,L('stale'))
        assert(commit(asset,info,session.project),L('apply_failed'))
        -- Keep owned textures for existing materials/Undo, not job closures or whole meshes.
        local resources={normalResources=session.worker.normalResources}
        for _,record in pairs(resources.normalResources or {}) do record.geometry=nil;record.options=nil end
        state.sessions[#state.sessions+1]={worker=resources,paths=session.paths,png=session.png,texture=session.texture}
        state.report=report;state.lastPNG=session.png
        state.applied=true
    end)
    active=session
    return true
end
function M.busy() return active~=nil end
function M.cancel()
    if not active then return end
    active.cancelled=true
    require('image_mesh_generation').cancel(active.worker)
    Normal.cancelWork(active.worker)
end
function M.advance()
    local session=active
    if not session then return end
    local ok,err=coroutine.resume(session.task)
    if not ok or coroutine.status(session.task)=='dead' then
        active=nil
        if not ok then
            if not session.cancelled then session.state.error=tostring(err);print('[mesh_debug_normal_generator] '..tostring(err)) end
            cleanup(session)
        end
    end
end
function M.progress()
    if not active then return end
    local open=tImGui.Begin(L('title'),false,0)
    if open then
        tImGui.ProgressBar(active.worker.simplifyProgress or active.worker.generationProgress or 0,{x=320,y=0})
        if tImGui.Button(tLang.L('cancel')) then M.cancel() end
    end
    tImGui.End()
end
function M.dispose(entry)
    if active and active.entry==entry then
        M.cancel();coroutine.close(active.task);cleanup(active);active=nil
    end
    local state=entry.normalGenerator
    if state then for _,session in ipairs(state.sessions) do cleanup(session) end end
    entry.normalGenerator=nil
end
function M.panel(entry,id,commit,undo)
    local state=M.state(entry)
    local owner=entry.imageMeshProjectOwner
    if owner then
        tImGui.TextWrapped(L('project_help'))
        Normal.settings(state)
        if tImGui.TreeNode(tLang.L('ime_target_group')) then
            require('image_mesh_triangle_target').panel(state);tImGui.TreePop()
        end
    else
        tImGui.TextWrapped(L('mesh_help'))
        if tImGui.Button(L('choose_source')..'##ngsource'..id) then
            local path=mbm.openFile(state.source,'png','jpg','jpeg','bmp','tga')
            if path then state.source=path end
        end
        tImGui.TextWrapped(state.source)
        require('normal_map_panel').draw(tImGui,function(k) return tLang.L('nmg_'..k) end,state.settings,false)
        local changed,value=tImGui.InputInt(tLang.L('nm_frame')..'##ngf'..id,state.frame)
        if changed then state.frame=math.max(0,math.min(entry.meshDebug:getTotalFrame(),value)) end
        changed,value=tImGui.InputInt(tLang.L('nm_subset')..'##ngs'..id,state.subset)
        if changed then state.subset=math.max(0,value) end
        tImGui.TextWrapped(tLang.L('nm_all_hint'))
    end
    tImGui.BeginDisabled(M.busy() or (entry.tSimplifyState or {}).running==true)
    if tImGui.Button(L('generate')..'##ngapply'..id) then
        local ok,err=dpCall(M.start,entry,commit);if not ok then state.error=tostring(err) end
    end
    tImGui.SameLine()
    local restored=false
    if tImGui.Button(tLang.L('undo')..'##ngundo'..id) then restored=undo() end
    if owner and tImGui.Button(L('save_project')..'##ngsave'..id) then
        local path=mbm.saveFile(owner.path,'imesh')
        if path then
            local ok,err=dpCall(require('image_mesh_io').save,owner.project,path,tUtil.save)
            if not ok then state.error=tostring(err) end
        end
    end
    if state.lastPNG and tImGui.Button(L('export_png')..'##ngpng'..id) then
        local suggestion=require('normal_map_project').suggestedPaths(state.source)
        local path=mbm.saveFile(suggestion,'png')
        if path then
            local ok,err=dpCall(function()
                local bytes,w,h=mbm.readImagePixels(state.lastPNG);assert(bytes,w)
                assert(mbm.writeImagePixels(path,bytes,w,h))
            end)
            if not ok then state.error=tostring(err) end
        end
    end
    tImGui.EndDisabled()
    if restored then return true end
    if state.error then tImGui.TextWrapped(state.error) end
    if state.applied then tImGui.TextWrapped(L('applied')) end
end
return M
