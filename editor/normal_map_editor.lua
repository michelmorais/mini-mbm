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

local testApi=...
tImGui=require 'ImGui'
tUtil=require 'editor_utils'
local Height=require 'height_map_source'
local Generator=require 'normal_map_generator'
local Preview=require 'normal_map_preview'
local Project=require 'normal_map_project'
local Panel=require 'normal_map_panel'
local E={options=Height.settings(),azimuth=45,elevation=45,enabled=true,builds=0,uploads=0}
local function L(key) return tLang.L('nmg_'..key) end
local function dpCall(fn,...)
    local res=table.pack(pcall(fn,...))
    if not res[1] then E.status=tostring(res[2]);print('[normal_map_editor] '..E.status) end
    return table.unpack(res,1,res.n)
end
local function cancelJobs()
    for _,key in ipairs{'job','lightJob','exportJob'} do
        if E[key] then E[key]:cancel();E[key]=nil end
    end
end
function E.invalidate()
    cancelJobs();E.status=nil;E.pending=true;E.due=mbm.getTimeRun()+.15
end
function E.open(path,options)
    local bytes,w,h=mbm.readImagePixels(path)
    assert(bytes,w)
    local image=Height.image(bytes,w,h)
    local settings=Height.settings(options or E.options)
    cancelJobs();E.preview:clear();E.result=nil;E.source=path;E.image=image;E.options=settings
    E.status=nil;E.invalidate()
end
function E.export(path)
    assert(E.image,L('open_first'))
    local snapshot=Height.settings(E.options)
    if E.exportJob then E.exportJob:cancel() end
    E.exportPath=path
    E.exportJob=Generator.start(E.image,snapshot)
    E.status=L('exporting')
end
local function relight()
    if E.lightJob then E.lightJob:cancel() end
    if not E.result then return end
    local result,azimuth,elevation,enabled=E.result,E.azimuth,E.elevation,E.enabled
    E.lightJob=Generator.job(function(tick) return Preview.light(result,azimuth,elevation,enabled,tick) end)
end
function E.update()
    if E.pending and E.image and mbm.getTimeRun()>=E.due then
        E.pending=false;E.job=Generator.start(E.image,E.options,384)
    end
    if E.job then
        E.job:step()
        if E.job.state=='completed' then
            local result=E.job.result;E.job=nil
            E.preview:set('original',result.diffuse,result.width,result.height)
            E.preview:set('height',result.heightBytes,result.width,result.height)
            E.preview:set('normal',result.bytes,result.width,result.height)
            E.result=result;E.builds=E.builds+1;E.uploads=E.uploads+3;relight()
        elseif E.job.state=='failed' then E.status=E.job.error;E.job=nil end
    end
    if E.lightJob then
        E.lightJob:step()
        if E.lightJob.state=='completed' then
            E.preview:set('lit',E.lightJob.result,E.result.width,E.result.height)
            E.uploads=E.uploads+1;E.lightJob=nil
        elseif E.lightJob.state=='failed' then E.status=E.lightJob.error;E.lightJob=nil end
    end
    if E.exportJob then
        E.exportJob:step()
        if E.exportJob.state=='completed' then
            local result=E.exportJob.result;E.exportJob=nil
            assert(mbm.writeImagePixels(E.exportPath,result.bytes,result.width,result.height))
            E.status=L('exported')..': '..E.exportPath
        elseif E.exportJob.state=='failed' then E.status=E.exportJob.error;E.exportJob=nil end
    end
end
function onInitScene()
    E.preview=Preview.new(tUtil.getTemporaryFilePath)
    E.flags=tImGui.Flags('ImGuiWindowFlags_NoMove')
    E.titles={controls='nmg_controls',preview='nmg_preview'}
    E.checker=tUtil.createAlphaPattern(384,384,16,{r=90,g=90,b=90},{r=130,g=130,b=130})
    if E.checker then E.checkerInfo=mbm.loadTexture(E.checker) end
    -- Standard origin helpers, hidden for this image-only workspace.
    E.axes={line:new('2dw'),line:new('2dw')}
    E.axes[1]:add({-1000,0,1000,0});E.axes[1]:setColor(1,0,0)
    E.axes[2]:add({0,-1000,0,1000});E.axes[2]:setColor(0,1,0)
    for _,axis in ipairs(E.axes) do axis.visible=false end
    tUtil.sMessageOverlay=L('welcome')
end
local function openImage()
    local path=mbm.openFile(E.source or '',table.unpack(tUtil.supported_images))
    if path then E.open(path);E.projectPath=nil end
end
local function openProject()
    local path=mbm.openFile(E.projectPath or '','normalmap')
    if path then local source,options=Project.load(path);E.open(source,options);E.projectPath=path end
end
local function saveProject()
    local path=mbm.saveFile(E.projectPath or 'project.normalmap','normalmap')
    if path then
        if path:sub(-10):lower()~='.normalmap' then path=path..'.normalmap' end
        Project.save(path,E.source,E.options);E.projectPath=path;E.status=L('saved')
    end
end
local function exportImage()
    local path=mbm.saveFile('normal.png','png')
    if path then
        if path:sub(-4):lower()~='.png' then path=path..'.png' end
        assert(path~=E.source,L('source_overwrite'))
        E.export(path)
    end
end
local function controls()
    if tImGui.Button(L('open')) then dpCall(openImage) end
    tImGui.SameLine()
    if tImGui.Button(L('open_project')) then dpCall(openProject) end
    tImGui.BeginDisabled(not E.image)
    if tImGui.Button(L('save_project')) then dpCall(saveProject) end
    tImGui.SameLine()
    if tImGui.Button(L('export')) then dpCall(exportImage) end
    if E.image then tImGui.TextWrapped(E.source..' ('..E.image.width..' x '..E.image.height..')') end
    tImGui.Separator()
    if Panel.draw(tImGui,L,E.options) then E.invalidate() end
    if tImGui.Button(L('regenerate')) then E.invalidate() end
    tImGui.EndDisabled()
    tImGui.Separator()
    tImGui.SetNextItemWidth(145)
    local changed,value=tImGui.SliderFloat(L('azimuth'),E.azimuth,-180,180,'%.0f')
    if changed then E.azimuth=value;relight() end
    tImGui.SetNextItemWidth(145)
    changed,value=tImGui.SliderFloat(L('elevation'),E.elevation,5,90,'%.0f')
    if changed then E.elevation=value;relight() end
    value=tImGui.Checkbox(L('enabled'),E.enabled)
    if value~=E.enabled then E.enabled=value;relight() end
    tImGui.TextWrapped(L('preview_help'))
    local job=E.exportJob or E.job or E.lightJob
    if job then
        tImGui.ProgressBar(job.progress,{x=0,y=0})
        if tImGui.Button(L('cancel')) then cancelJobs();E.pending=false;E.status=L('cancelled') end
    end
    if E.pending or E.job then tImGui.TextWrapped(L('pending')) end
    if E.status then tImGui.TextWrapped(E.status) end
end
local function previews()
    if not E.result then tImGui.TextWrapped(L('open_first'));return end
    local available=tImGui.GetContentRegionAvail()
    local size=math.max(32,math.min(512,(available.x-30)/2,(available.y-60)/2))
    local w,h=E.result.width,E.result.height
    local scale=size/math.max(w,h)
    if tImGui.BeginTable('normal_map_images',2,0) then
        for i,name in ipairs{'original','height','normal','lit'} do
            if i%2==1 then tImGui.TableNextRow() end
            tImGui.TableNextColumn();tImGui.Text(L(name))
            local slot=E.preview.slots[name]
            if slot and slot.info and slot.info:isLoaded() then
                -- Checker behind transparent pixels; same cursor origin for both draws.
                local pos=tImGui.GetCursorPos()
                if E.checkerInfo then tImGui.Image(E.checkerInfo,{x=w*scale,y=h*scale}) end
                tImGui.SetCursorPos(pos)
                tImGui.Image(slot.info,{x=w*scale,y=h*scale})
            end
        end
        tImGui.EndTable()
    end
end
function onLoop(delta)
    dpCall(E.update)
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu(tLang.L('menu_options')) then
            tLang.renderLanguageSubmenu();tImGui.EndMenu()
        end
        if tImGui.MenuItem(tLang.L('menu_quit')) then mbm.quit() end
        tImGui.EndMainMenuBar()
    end
    tUtil.setInitialWindowPositionLeft(E.titles.controls,0,0,400)
    local opened=tImGui.Begin(L('controls')..'##'..E.titles.controls,false,E.flags)
    if opened then dpCall(controls) end
    tImGui.End()
    tUtil.setInitialWindowPositionRight(E.titles.preview,0,0,math.max(300,mbm.getRealSizeScreen()-400))
    opened=tImGui.Begin(L('preview')..'##'..E.titles.preview,false,E.flags)
    if opened then dpCall(previews) end
    tImGui.End()
    tUtil.showOverlayMessage()
end
function onEndScene()
    cancelJobs()
    if E.preview then E.preview:clear() end
    if E.checkerInfo then E.checkerInfo:release() end
    if E.checker then os.remove(E.checker) end
    for _,axis in ipairs(E.axes or {}) do axis:destroy() end
    E.image=nil;E.result=nil
end
-- Global-callback scenes must not return a table: the launcher interprets it as
-- a class-style scene and would skip the global onInitScene callback.
if type(testApi)=='table' then testApi.state=E end
