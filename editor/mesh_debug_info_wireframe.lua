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

local Wire=require 'image_mesh_wireframe'
local M={}
function M.hide(entry)
    local view=entry and entry.infoWireView
    if not view then return end
    if view.active and view.preview then view.preview.visible=view.mainVisible end
    view.active=false
    if view.wireObject then view.wireObject.visible=false end
end
function M.release(entry)
    if not entry then return end
    M.hide(entry)
    if entry.infoWireView then Wire.release(entry.infoWireView) end
    entry.infoWireView=nil
end
function M.set(entry,preview,enabled,safe)
    if not enabled then
        if entry.infoWireView then entry.infoWireView.enabled=false end
        M.hide(entry);return true
    end
    if not preview then return false end
    local view=entry.infoWireView
    if view and view.preview~=preview then M.release(entry);view=nil end
    if not view then view={preview=preview};entry.infoWireView=view end
    local ok,err=safe(function()
        assert(entry.meshDebug:getModeDraw()=='TRIANGLES',tLang.L('capture_requires_triangles'))
        Wire.ensure(view,entry.meshDebug)
        view.wireObject:setPos(0,0,0)
    end)
    if not ok then M.release(entry);entry.infoWireError=tostring(err);return false end
    entry.infoWireError=nil
    view.enabled=true
    return true
end
function M.sync(entry,active)
    local view=entry and entry.infoWireView
    if not view then return end
    if not active or not view.enabled then M.hide(entry);return end
    if not view.active then view.mainVisible=view.preview.visible;view.active=true end
    view.preview.visible=false
    view.wireObject.visible=true
end
function M.panel(entry,preview,available,safe)
    if not entry.info or entry.info.type~='mesh' then return end
    local view=entry.infoWireView
    tImGui.BeginDisabled(not available)
    local enabled=tImGui.Checkbox(tLang.L('md_info_wireframe'),view and view.enabled==true or false)
    if tImGui.IsItemHovered() then
        require('mesh_simplify_modes').tooltip(tLang.L('md_info_wireframe_help'))
    end
    tImGui.EndDisabled()
    if available and enabled~=(view and view.enabled==true or false) then
        if view then view.enabled=enabled end
        M.set(entry,preview,enabled,safe)
    end
    if entry.infoWireError then tImGui.TextWrapped(entry.infoWireError) end
end
return M
