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

-- Read-only runtime preview. Cache authored clip names until the Mesh Debug object changes.
local M = {}

local function drawExecutionReport(preview, ready, call)
    if not preview then return end
    local ok, report = call(function() return preview:getSkeletalSkinningReport() end)
    if not ok or type(report) ~= 'table' or (tonumber(report.requiredBoneCount) or 0) == 0 then return end
    local path = report.resolvedExecutionPath or report.executionPath
    local status = report.executionStatus
    local color
    if ready and path == 'gpu' and status == 'gpu-ready' then
        color = {r=0.15,g=0.82,b=0.32,a=1}
    elseif ready and path == 'cpu' and (status == 'cpu-lbs-ready' or status == 'cpu-dqs-ready') then
        color = {r=1,g=0.55,b=0.10,a=1}
    end
    if color then
        local cursor = tImGui.GetCursorScreenPos()
        tImGui.Dummy({x=12,y=14})
        tImGui.AddCircleFilled({x=cursor.x+6,y=cursor.y+7},5,color,18)
        tImGui.SameLine()
    end
    tImGui.Text(tLang.L('swl_execution_path') .. ': ' .. (path or 'unknown'):upper() ..
        ' / ' .. (report.resolvedMethod or 'unknown'):upper())
    if not color then tImGui.TextDisabled(status or 'unknown') end
end

function M.draw(entry, data, preview, ready, call, onEdit)
    drawExecutionReport(preview, ready, call)
    local state = entry.skeletalPlayback
    if not state or state.data ~= data then
        state = {data = data, clips = {}, selected = 1, mode = 1, weight = 0.5}
        entry.skeletalPlayback = state
        local ok, clips = call(function() return data:getSkeletalAnimationReport() end)
        if ok and type(clips) == 'table' then
            for _, clip in ipairs(clips) do
                if type(clip.name) == 'string' then
                    state.clips[#state.clips + 1] = clip.name
                end
            end
        end
    end
    if #state.clips == 0 then
        tImGui.TextDisabled(tLang.L('mesh_debug_skeletal_no_clips'))
        return
    end
    tImGui.PushItemWidth(220)
    local changed, selected = tImGui.Combo(tLang.L('swl_skeletal_clip'), state.selected, state.clips)
    if changed then state.selected = selected end
    tImGui.PopItemWidth()
    local clipName = state.clips[state.selected]
    local autoplay = data:isAutoplayAnimation('skeletal', clipName)
    local newAutoplay = tImGui.Checkbox(
        tLang.L('autoplay_animation_on_load') .. '##skeletalAutoplay', autoplay)
    if newAutoplay ~= autoplay then
        local ok, updated = call(function()
            if newAutoplay then return data:setAutoplayAnimation('skeletal', clipName) end
            return data:setAutoplayAnimation('', '')
        end)
        if ok and updated and onEdit then onEdit() end
    end
    if not preview or not preview.getTotalSkeletalAnimations then
        tImGui.TextDisabled(tLang.L('mesh_debug_skeletal_preview_unavailable'))
        return
    end
    local function apply(fn)
        local ok, result = call(fn)
        state.failed = not ok or result == false
        return not state.failed
    end
    tImGui.BeginDisabled(not ready)
    tImGui.PushItemWidth(220)
    local modeChanged, mode = tImGui.Combo(tLang.L('mesh_debug_skeletal_mode'), state.mode,
        {tLang.L('mesh_debug_skeletal_base'), tLang.L('swl_layer_mode_absolute'), tLang.L('swl_layer_mode_additive')})
    if modeChanged and ready then state.mode = mode end
    local isLayer = state.mode ~= 1
    local hasBase = preview:getSkeletalAnimationTime() ~= nil
    if isLayer then
        local weightChanged, weight = tImGui.SliderFloat(tLang.L('swl_absolute_layer_weight'), state.weight, 0, 1, '%.3f')
        if weightChanged and ready then
            if preview:getSkeletalAnimationAbsoluteLayerWeight() == nil or
                apply(function() return preview:setSkeletalAnimationAbsoluteLayerWeight(weight) end) then
                state.weight = weight
            end
        end
    end
    tImGui.PopItemWidth()
    tImGui.BeginDisabled(isLayer and not hasBase)
    local canControl = ready and (not isLayer or hasBase)
    if tImGui.Button(tLang.L('ase_play')) and canControl then
        apply(function()
            local name = state.clips[state.selected]
            if state.mode == 2 then return preview:playSkeletalAnimationAbsoluteLayer(name, state.weight) end
            if state.mode == 3 then return preview:playSkeletalAnimationAdditiveLayer(name, state.weight) end
            return preview:playSkeletalAnimation(name)
        end)
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('ase_pause')) and canControl then
        apply(function()
            if isLayer then return preview:pauseSkeletalAnimationLayer() end
            return preview:pauseSkeletalAnimation()
        end)
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_resume')) and canControl then
        apply(function()
            if isLayer then return preview:resumeSkeletalAnimationLayer() end
            return preview:resumeSkeletalAnimation()
        end)
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('ase_tl_stop')) and canControl then
        apply(function()
            if isLayer then return preview:stopSkeletalAnimationAbsoluteLayer() end
            return preview:stopSkeletalAnimation()
        end)
    end
    tImGui.EndDisabled()
    tImGui.EndDisabled()
    if isLayer and not hasBase then tImGui.TextWrapped(tLang.L('mesh_debug_skeletal_requires_base')) end
    if state.failed then tImGui.TextWrapped(tLang.L('mesh_debug_skeletal_failed')) end
    tImGui.TextWrapped(tLang.L('mesh_debug_skeletal_layer_help'))
    if not ready then tImGui.TextWrapped(tLang.L('articulated_save_to_preview')) end
    tImGui.TextWrapped(tLang.L('mesh_debug_skeletal_preview'))
end

return M
