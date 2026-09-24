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

-- Shared presentation only; algorithms and mode validation live in the engine.
local M={}
function M.select(value,curved,id)
    local modes=curved and {'specific','coplanar_specific','coplanar'} or {'qem','coplanar_qem','coplanar'}
    local labels={tLang.L(curved and 'simplify_mode_specific' or 'simplify_mode_qem'),
        tLang.L(curved and 'simplify_mode_coplanar_specific' or 'simplify_mode_coplanar_qem'),
        tLang.L('simplify_mode_coplanar')}
    local index=1
    for i,v in ipairs(modes) do if value==v then index=i end end
    local changed,selected=tImGui.Combo(tLang.L('simplify_mode')..'##'..id,index,labels,-1)
    return modes[changed and selected or index]
end
function M.tooltip(text)
    if not tImGui.BeginTooltip() then return end
    tImGui.PushTextWrapPos(420)
    tImGui.Text(text)
    tImGui.PopTextWrapPos()
    tImGui.EndTooltip()
end
-- Keep distance and angle independent; changing units must not change saved geometry.
function M.planarSettings(tolerance,angle,id,reduceBoundaries)
    tolerance=tolerance or 1e-7
    angle=angle or .05
    local flags=tImGui.Flags('ImGuiSliderFlags_AlwaysClamp')
    tImGui.SetNextItemWidth(240)
    local changed,result=tImGui.DragFloat(tLang.L('simplify_planar_angle')..'##'..id,
        angle,.01,0,5,'%.3f deg',flags)
    if tImGui.IsItemHovered() then M.tooltip(tLang.L('simplify_planar_angle_help')) end
    if changed and result==result then angle=math.max(0,math.min(5,result)) end
    if tImGui.CollapsingHeader(tLang.L('simplify_planar_advanced')..'##'..id) then
        tImGui.SetNextItemWidth(240)
        changed,result=tImGui.DragFloat(tLang.L('simplify_planar_tolerance')..'##'..id,
            tolerance*100,.00001,0,1,'%.6f %%',flags)
        if tImGui.IsItemHovered() then M.tooltip(tLang.L('simplify_planar_tolerance_help')) end
        if changed and result==result then tolerance=math.max(0,math.min(.01,result/100)) end
    end
    reduceBoundaries=tImGui.Checkbox(tLang.L('simplify_planar_boundaries')..'##'..id,reduceBoundaries==true)
    if tImGui.IsItemHovered() then M.tooltip(tLang.L('simplify_planar_boundaries_help')) end
    return tolerance,angle,reduceBoundaries
end
function M.report(report)
    if not report then return end
    if report.planarRegions and (report.planarRegions>0 or report.planarRejectedRegions>0 or report.planarSkipped) then
        tImGui.TextWrapped(string.format(tLang.L('simplify_planar_report'),report.planarRegions,
            report.planarRemovedTriangles,report.planarRejectedRegions,report.planarMaximumError,report.planarMaximumUvError))
        tImGui.TextWrapped(string.format(tLang.L('simplify_planar_rejections'),
            report.planarHoles or 0,report.planarAttributes or 0,report.planarTopology or 0,
            report.planarSurroundings or 0,report.planarWorkLimit or 0))
        if report.planarSkipped then tImGui.TextWrapped(tLang.L('simplify_planar_skipped')) end
    end
    if (report.planarBoundaryRemovedVertices or 0)>0 then
        tImGui.TextWrapped(string.format(tLang.L('simplify_planar_boundaries_report'),report.planarBoundaryRemovedVertices))
    end
    if report.planarBoundaryFallback then tImGui.TextWrapped(tLang.L('simplify_planar_boundaries_fallback')) end
    if report.unchanged then
        tImGui.TextWrapped(tLang.L('simplify_unchanged'))
        tImGui.TextWrapped(tLang.L('simplify_planar_no_gain'))
    end
end
return M
