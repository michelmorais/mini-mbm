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

-- Shared editor UI for native, external and combined simplification.
local M={}
function M.enabled(mode,method)
    return mode=='cgal_qem' or (mode or 'qem')==method
end
function M.setEnabled(mode,method,enabled)
    local cgal=M.enabled(mode,'cgal')
    local qem=M.enabled(mode,'qem')
    if method=='cgal' then cgal=enabled else qem=enabled end
    if cgal and qem then return 'cgal_qem' end
    if cgal then return 'cgal' end
    if qem then return 'qem' end
    return 'none'
end
function M.cgalBlock(mode,tolerance,angle,id)
    local enabled=tImGui.Checkbox('CGAL##cgal-'..id,M.enabled(mode,'cgal'))
    if tImGui.IsItemHovered() then M.tooltip(tLang.L('cgal_method_tooltip')) end
    mode=M.setEnabled(mode,'cgal',enabled)
    tImGui.BeginDisabled(not enabled)
    tolerance,angle=M.cgalSettings(tolerance,angle,id)
    tImGui.TextWrapped(tLang.L('cgal_help'))
    tImGui.EndDisabled()
    if enabled and require('mesh_cgal').getPath()=='' then tImGui.TextWrapped(tLang.L('cgal_missing')) end
    return mode,tolerance,angle
end
function M.qemCheckbox(mode,id)
    tImGui.Separator()
    local enabled=tImGui.Checkbox('QEM##qem-'..id,M.enabled(mode,'qem'))
    if tImGui.IsItemHovered() then M.tooltip(tLang.L('qem_method_tooltip')) end
    mode=M.setEnabled(mode,'qem',enabled)
    if mode=='cgal_qem' then tImGui.TextWrapped(tLang.L('cgal_qem_help')) end
    return mode
end
function M.tooltip(text)
    if not tImGui.BeginTooltip() then return end
    tImGui.PushTextWrapPos(420)
    tImGui.Text(text)
    tImGui.PopTextWrapPos()
    tImGui.EndTooltip()
end
function M.cgalSettings(tolerance,angle,id)
    tolerance=tolerance or .05
    angle=angle or 10
    local flags=tImGui.Flags('ImGuiSliderFlags_AlwaysClamp')
    tImGui.SetNextItemWidth(240)
    local changed,result=tImGui.DragFloat(tLang.L('cgal_angle')..'##'..id,
        angle,.1,0,60,'%.3f deg',flags)
    if tImGui.IsItemHovered() then M.tooltip(tLang.L('cgal_angle_help')) end
    if changed and result==result then angle=math.max(0,math.min(60,result)) end
    tImGui.SetNextItemWidth(240)
    changed,result=tImGui.DragFloat(tLang.L('cgal_distance')..'##'..id,
        tolerance*100,.001,0,10,'%.6f %%',flags)
    if tImGui.IsItemHovered() then M.tooltip(tLang.L('cgal_distance_help')) end
    if changed and result==result then tolerance=math.max(0,math.min(.1,result/100)) end
    return tolerance,angle
end
function M.report(report)
    if not report then return end
    if report.cgal then
        tImGui.TextWrapped(string.format(tLang.L('cgal_report'),report.cgal.regions,report.cgal.sampled_error_fraction*100))
    end
    if report.backend=='cgal_qem' and report.qemRan then tImGui.TextWrapped(tLang.L('cgal_qem_report')) end
    if report.unchanged then tImGui.TextWrapped(tLang.L('simplify_unchanged')) end
end
return M
