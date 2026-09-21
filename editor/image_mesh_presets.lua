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
local IO=require 'image_mesh_io'
local M={}
local function L(key) return tLang.L('ime_'..key) end
local function cleanName(name)
    assert(type(name)=='string','ime_invalid_name')
    name=name:match('^%s*(.-)%s*$')
    assert(#name>0 and #name<=128 and not name:find('%c'),'ime_invalid_name')
    return name
end
function M.store(project,name,values,replace)
    name=cleanName(name)
    local preset={name=name,settings=Model.settings(values)}
    local list=project.presets or {}
    for i,p in ipairs(list) do
        if p.name==name then
            assert(replace,'ime_preset_duplicate')
            list[i]=preset; project.presets=list; return i
        end
    end
    assert(#list<128,'ime_preset_limit')
    list[#list+1]=preset; project.presets=list
    return #list
end
function M.rename(project,index,name)
    name=cleanName(name)
    local list=assert(project.presets,'ime_preset_invalid')
    for i,p in ipairs(list) do assert(i==index or p.name~=name,'ime_preset_duplicate') end
    assert(list[index],'ime_preset_invalid').name=name
end
function M.apply(project,index,selection,defaults)
    local preset=assert((project.presets or {})[index],'ime_preset_invalid')
    local settings=Model.settings(preset.settings)
    if defaults then project.defaults=settings; return end
    local count=0
    for _,r in ipairs(project.regions) do
        if selection[r.id] then
            r.overrides=Model.copy(settings)
            count=count+1
        end
    end
    assert(count>0,'ime_select_region')
end
function M.save(preset,path,serialize)
    Model.validatePresets({preset})
    local lines={}
    local settings=Model.settings(preset.settings)
    for _,key in ipairs{'sideTexture','backTexture'} do
        if settings[key]~='' then settings[key]=IO.relative(settings[key],path) end
    end
    serialize('preset',{version=1,name=preset.name,settings=settings},lines)
    local data=table.concat(lines,'\n')..'\nreturn preset\n'
    assert(#data<=65536,'ime_preset_invalid')
    local f,err=io.open(path,'wb'); assert(f,err)
    local ok,writeError=f:write(data); local closed,closeError=f:close()
    assert(ok,writeError); assert(closed,closeError)
end
function M.load(path)
    local f,err=io.open(path,'rb'); assert(f,err)
    local data=f:read(65537); f:close()
    assert(data and #data<=65536,'ime_preset_invalid')
    local fn,loadError=load(data,'@'..path,'t',{}); assert(fn,loadError)
    local value=fn()
    assert(type(value)=='table' and value.version==1,'ime_preset_invalid')
    Model.validatePresets({value})
    local settings=Model.settings(value.settings)
    for _,key in ipairs{'sideTexture','backTexture'} do
        if settings[key]~='' then settings[key]=IO.resolve(settings[key],path) end
    end
    return {name=cleanName(value.name),settings=settings}
end
function M.draw(E,action,dpCall)
    if not tImGui.CollapsingHeader(L('presets')) then return end
    E.presetUI=E.presetUI or {index=1,name=''}
    local ui=E.presetUI
    -- Rebuild names only after an action, undo, or project replacement.
    if ui.project~=E.project then
        ui.project=E.project; ui.names={}
        for _,p in ipairs(E.project.presets or {}) do ui.names[#ui.names+1]=p.name end
        ui.index=math.max(1,math.min(ui.index,#ui.names))
    end
    tImGui.TextWrapped(L('preset_help'))
    local edited,name=tImGui.InputText(L('preset_name'),ui.name)
    if edited then ui.name=name end
    local function notify(key)
        E.status=L(key); tUtil.showMessage(E.status,4)
        tUtil.tTimerOverlay:set(4); tUtil.tTimerOverlay:restart()
    end
    if tImGui.Button(L('preset_create')) then
        local index
        if action(function(p) index=M.store(p,ui.name,E.values,false) end) then ui.index=index; notify('preset_saved') end
    end
    tImGui.SameLine()
    if tImGui.Button(L('preset_import')) then dpCall(function()
        local path=mbm.openFile('','imeshpreset'); if not path then return end
        local preset=M.load(path); local index
        if action(function(p) index=M.store(p,preset.name,preset.settings,false) end) then
            ui.index=index; ui.name=preset.name; notify('preset_imported')
        end
    end) end
    local preset=(E.project.presets or {})[ui.index]
    if preset then
        -- An action above may have changed the list; refresh on the next frame.
        if ui.project==E.project then
            tImGui.SetNextItemWidth(180)
            local change,index=tImGui.Combo(L('preset_list'),ui.index,ui.names)
            if change then ui.index=index; preset=E.project.presets[index]; ui.name=preset.name end
        end
        if tImGui.Button(L(E.editDefaults and 'preset_apply_defaults' or 'preset_apply')) then
            if action(function(p) M.apply(p,ui.index,E.selection,E.editDefaults) end) then notify('preset_applied') end
        end
        if tImGui.Button(L('preset_update')) then
            if action(function(p) M.store(p,preset.name,E.values,true) end) then notify('preset_saved') end
        end
        if tImGui.Button(L('preset_rename')) then
            action(function(p) M.rename(p,ui.index,ui.name) end)
        end
        tImGui.SameLine()
        if tImGui.Button(L('preset_delete')) then
            if action(function(p) table.remove(p.presets,ui.index) end) then notify('preset_deleted') end
        end
        if tImGui.Button(L('preset_export')) then dpCall(function()
            local filename=preset.name:gsub('[^%w_-]','_')..'.imeshpreset'
            local path=mbm.saveFile(filename,'imeshpreset')
            if path then M.save(preset,path,tUtil.save); notify('preset_exported') end
        end) end
    end
end
return M
