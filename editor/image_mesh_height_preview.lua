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
local M={}
local function releasePreview(E)
    if E.heightObject then E.heightObject:destroy(); E.heightObject=nil end
    if E.heightPath then os.remove(E.heightPath); E.heightPath=nil end
    E.heightScale=nil; E.heightRegion=nil; E.heightResultKey=nil; E.heightStale=nil
end
local function cancel(E)
    if E.heightJob and not E.heightJob.discard then
        E.heightJob.discard=true; E.heightJob.job:cancel()
    end
end
function M.invalidate(E)
    cancel(E); E.heightRequested=true; E.heightError=nil;E.heightStale=true
end
function M.destroy(E)
    M.invalidate(E);releasePreview(E);E.heightKey=nil
end
function M.shutdown(E)
    M.destroy(E)
    if E.heightJob then
        E.heightJob.job:close() -- Only scene teardown waits for an unfinished worker.
        os.remove(E.heightJob.path);E.heightJob=nil
    end
end
function M.build(E)
    local region=Model.copy(E.draft)
    region.overrides={}
    for k in pairs(Model.defaults) do region.overrides[k]=E.values[k] end
    local candidate=Model.copy(E.project); candidate.regions={region}
    Model.validate(candidate)
    local path=tUtil.getTemporaryFilePath('.png')
    local job,message=mbm.startImageMeshMap(E.project.image.path,Model.options(E.project,region),path,E.heightView==3)
    if not job then os.remove(path);error(message,0) end
    E.heightJob={job=job,path=path,region=region,key=E.heightKey,source=E.project.image.path,id=E.selected}
    E.heightError=nil
end
local function install(E,pending)
    assert(pending.job:takeResult())
    local object=texture:new('2dw');pending.object=object
    assert(object:load(pending.path))
    object:setSize(pending.region.w,pending.region.h)
    releasePreview(E)
    E.heightObject=object;pending.object=nil
    E.heightPath=pending.path;E.heightRegion=pending.region
    E.heightSourcePath=pending.source;E.heightSelected=pending.id;E.heightResultKey=pending.key
    E.heightStale=nil
    E.heightBuilds=(E.heightBuilds or 0)+1
end
function M.sync(E,protectedCall)
    local key=E.revision..':'..E.selected..':'..E.heightView
    if E.heightKey~=key then E.heightKey=key;M.invalidate(E) end
    local visible=E.editMode and E.heightView~=1 and E.draft~=nil and not E.drag
    if E.heightJob and (E.heightRequested or not visible or E.paintDrag) then
        if not visible or E.paintDrag then E.heightRequested=true end
        cancel(E)
    end
    local pending=E.heightJob
    if pending then
        local status=pending.job:getStatus();pending.progress=status.progress
        if status.state~='running' then
            local installed=false
            if not pending.discard and pending.key==key and visible then
                if status.state=='completed' then
                    installed=protectedCall(install,E,pending)
                    if not installed then E.heightError=E.status end
                elseif status.state=='failed' then E.heightError=status.error end
            end
            if pending.object then pending.object:destroy() end
            pending.job:close()
            if not installed then os.remove(pending.path) end
            E.heightJob=nil
        end
    end
    if visible and E.heightRequested and not E.paintDrag and not E.heightJob then
        E.heightStale=true;E.heightRequested=false
        local ok=protectedCall(M.build,E)
        if not ok then E.heightError=E.status end
    end
    if E.heightObject then
        E.heightObject.visible=visible and E.heightSelected==E.selected and E.heightSourcePath==E.project.image.path
        if E.heightScale~=E.zoom then
            local r=E.heightRegion
            E.heightObject:setScale(E.zoom,E.zoom)
            E.heightObject:setPos((r.x+r.w/2-E.project.image.width/2)*E.zoom,
                (E.project.image.height/2-r.y-r.h/2)*E.zoom,0.5)
            E.heightScale=E.zoom
        end
    end
end
function M.panel(E)
    if E.heightView==1 then return end
    if E.heightJob then
        tImGui.Text(tLang.L('ime_height_processing'))
        tImGui.ProgressBar(E.heightJob.progress or 0,{x=0,y=0})
        if tImGui.Button(tLang.L('ime_cancel')..'##height_preview') then
            E.heightRequested=false;cancel(E)
        end
    end
    if E.heightObject and E.heightObject.visible and (E.heightStale or E.heightJob or E.heightRequested or E.heightResultKey~=E.heightKey or E.heightError) then
        tImGui.TextWrapped(tLang.L('ime_height_previous'))
    end
    if E.heightError then tImGui.TextWrapped(E.heightError) end
end
return M
