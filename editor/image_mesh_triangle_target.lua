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
function M.panel(E,confirm)
    local available=Model.canSeparateDetail(E.values)
    tImGui.BeginDisabled(not available)
    E.values.normalMapAutomatic=tImGui.Checkbox(tLang.L('ime_target_enable'),E.values.normalMapAutomatic)
    if E.values.normalMapAutomatic then
        local changed,target=tImGui.InputInt(tLang.L('ime_normal_target'),E.values.normalMapTargetTriangles)
        if changed then E.values.normalMapTargetTriangles=math.max(2,math.min(100000,target)) end
        local separated=Model.canBakeSeparatedDetail(E.values)
        tImGui.TextWrapped(tLang.L(separated and 'ime_normal_automatic_help' or 'ime_target_geometry_help'))
        local result=E.report and E.report.detailSeparation
        if result then
            if separated then
                tImGui.TextWrapped(string.format(tLang.L('ime_normal_automatic_result'),result.radius,result.triangles,result.target))
            else
                tImGui.TextWrapped(string.format(tLang.L('ime_target_result'),result.triangles,result.target))
            end
            tImGui.TextWrapped(tLang.L(result.reached and 'ime_normal_target_met' or 'ime_normal_target_unmet'))
            local canConfirm=confirm~=nil and E.report.targetParameters~=nil and not E.meshTask and
                not E.editDefaults and E.draft~=nil and not E.draft.locked and not E.previewStale and
                not (E.assembly and E.assembly.enabled)
            tImGui.BeginDisabled(not canConfirm)
            if tImGui.Button(tLang.L('ime_target_confirm')) and canConfirm then confirm() end
            tImGui.EndDisabled()
            if tImGui.IsItemHovered() then require('image_mesh_help').tooltip(tLang.L('ime_target_confirm_help')) end
        end
    end
    tImGui.EndDisabled()
    if not available then tImGui.TextWrapped(tLang.L('ime_target_unavailable')) end
end
return M
