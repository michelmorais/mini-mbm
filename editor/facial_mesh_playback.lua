--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Facial target and clip preview for Mesh Debug. Runtime controls are deliberately kept here so
   the general mesh editor can inspect imported Faceit data without duplicating authoring logic.
]]--

local M = {}

local function safe(call, fn)
    local ok, result = call(fn)
    return ok and result or nil
end

local function showHelpTooltip(message)
    if not message or message=='' or not tImGui.IsItemHovered(0) then return end
    tImGui.SetNextWindowSizeConstraints({x=280,y=0},{x=440,y=360})
    tImGui.BeginTooltip()
    tImGui.TextWrapped(message)
    tImGui.EndTooltip()
end

-- Reports are small runtime snapshots; never cache readiness across shader fallback.
function M.drawReport(preview, call)
    local report = safe(call, function() return preview:getFacialAnimationReport() end)
    local skin = safe(call, function() return preview:getSkeletalSkinningReport() end)
    if type(report) ~= 'table' then return end
    local function indicator(label, path)
        local color = path == 'gpu' and {r=0.15,g=0.82,b=0.32,a=1} or
            path == 'cpu' and {r=1,g=0.55,b=0.10,a=1} or nil
        if color then
            local cursor = tImGui.GetCursorScreenPos()
            tImGui.Dummy({x=12,y=14})
            tImGui.AddCircleFilled({x=cursor.x+6,y=cursor.y+7},5,color,18)
            tImGui.SameLine()
        end
        tImGui.Text(label)
    end
    local morph = report.morphExecutionPath
    local morphPath = report.ready and (morph == 'gpu-indexed-deltas' and 'gpu' or
        morph == 'cpu-reference' and 'cpu') or nil
    indicator(string.format(tLang.L('fre_preview_morph_fmt'),
        morphPath and morphPath:upper() or (morph or 'none')), morphPath)
    if type(skin) == 'table' and (tonumber(skin.requiredBoneCount) or 0) > 0 then
        local skinReady = skin.executionStatus == 'gpu-ready' or
            skin.executionStatus == 'cpu-lbs-ready' or skin.executionStatus == 'cpu-dqs-ready'
        local path = report.ready and skinReady and (skin.resolvedExecutionPath or skin.executionPath) or nil
        indicator(string.format(tLang.L('fre_preview_skin_fmt'),
            (skin.resolvedMethod or 'unknown'):upper() .. ' / ' .. (path or 'unknown'):upper()), path)
    end
    tImGui.TextDisabled(string.format('%s: %s',
        tLang.L('mesh_debug_facial_status'), report.status or 'unknown'))
    if report.morphStatus and report.morphStatus ~= '' then tImGui.TextWrapped(report.morphStatus) end
    return report
end

local function refreshState(state, preview, call)
    state.preview = preview
    state.targets = {}
    state.weights = {}
    state.clips = {}
    state.targetSelected = 1
    state.clipSelected = 1
    state.failed = nil
    local totalTargets = safe(call, function() return preview:getTotalFacialTargets() end) or 0
    for index = 1, totalTargets do
        local name = safe(call, function() return preview:getFacialTargetName(index) end)
        if type(name) == 'string' and name ~= '' then
            state.targets[#state.targets + 1] = name
            state.weights[#state.weights + 1] = safe(call, function()
                return preview:getFacialWeight(index)
            end) or 0
        end
    end
    local totalClips = safe(call, function() return preview:getTotalFacialClips() end) or 0
    for index = 1, totalClips do
        local name = safe(call, function() return preview:getFacialClipName(index) end)
        if type(name) == 'string' and name ~= '' then
            state.clips[#state.clips + 1] = name
        end
    end
    state.report = safe(call, function() return preview:getFacialAnimationReport() end)
end

local function apply(state, fn, call)
    local ok, result = call(fn)
    state.failed = not ok or result == false
    state.report = safe(call, function() return state.preview:getFacialAnimationReport() end)
    if not state.failed and state.onChanged then state.onChanged() end
    return not state.failed
end

local function refreshWeights(state, preview, call)
    for index = 1, #state.weights do
        local weight = safe(call, function() return preview:getFacialWeight(index) end)
        if weight ~= nil then state.weights[index] = weight end
    end
end

function M.draw(entry, data, preview, ready, call, onChanged)
    local state = entry.facialPlayback
    if not preview then
        tImGui.TextDisabled(tLang.L('mesh_debug_facial_preview_unavailable'))
        return
    end
    if not state or state.preview ~= preview then
        state = {preview = nil, targets = {}, weights = {}, clips = {}, targetSelected = 1,
            clipSelected = 1, failed = nil}
        entry.facialPlayback = state
        refreshState(state, preview, call)
    end
    state.onChanged = onChanged
    if #state.targets == 0 and #state.clips == 0 then
        tImGui.TextDisabled(tLang.L('mesh_debug_facial_no_data'))
        return
    end

    if #state.targets > 0 then
        tImGui.Text(tLang.L('mesh_debug_facial_targets'))
        tImGui.PushItemWidth(250)
        local changed, selected = tImGui.Combo(tLang.L('mesh_debug_facial_target'), state.targetSelected, state.targets)
        if changed then state.targetSelected = selected end
        showHelpTooltip(tLang.L('mesh_debug_facial_target_tooltip'))
        local current = state.weights[state.targetSelected] or 0
        local weightChanged, weight = tImGui.SliderFloat(
            tLang.L('mesh_debug_facial_weight'), current, 0, 1, '%.3f')
        showHelpTooltip(tLang.L('mesh_debug_facial_weight_tooltip'))
        if weightChanged and ready then
            if apply(state, function()
                return preview:setFacialWeight(state.targetSelected, weight)
            end, call) then
                state.weights[state.targetSelected] = weight
            end
        end
        tImGui.PopItemWidth()
        if tImGui.Button(tLang.L('mesh_debug_facial_clear')) and ready then
            if apply(state, function() return preview:clearFacialWeight(state.targetSelected) end, call) then
                state.weights[state.targetSelected] = 0
            end
        end
        showHelpTooltip(tLang.L('mesh_debug_facial_clear_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('mesh_debug_facial_reset')) and ready then
            if apply(state, function() return preview:resetFacialWeights() end, call) then
                for index = 1, #state.weights do state.weights[index] = 0 end
            end
        end
        showHelpTooltip(tLang.L('mesh_debug_facial_reset_tooltip'))
    end

    if #state.clips > 0 then
        tImGui.Separator()
        tImGui.Text(tLang.L('mesh_debug_facial_clips'))
        tImGui.PushItemWidth(250)
        local changed, selected = tImGui.Combo(tLang.L('mesh_debug_facial_clip'), state.clipSelected, state.clips)
        if changed then state.clipSelected = selected end
        showHelpTooltip(tLang.L('mesh_debug_facial_clip_tooltip'))
        tImGui.PopItemWidth()
        tImGui.BeginDisabled(not ready)
        if tImGui.Button(tLang.L('ase_play')) then
            if apply(state, function() return preview:playFacialAnimation(state.clips[state.clipSelected]) end, call) then
                refreshWeights(state, preview, call)
            end
        end
        showHelpTooltip(tLang.L('mesh_debug_facial_clip_play_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('ase_pause')) then
            apply(state, function() return preview:pauseFacialAnimation() end, call)
        end
        showHelpTooltip(tLang.L('mesh_debug_facial_clip_pause_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_resume')) then
            apply(state, function() return preview:resumeFacialAnimation() end, call)
        end
        showHelpTooltip(tLang.L('mesh_debug_facial_clip_resume_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('ase_tl_stop')) then
            apply(state, function() return preview:stopFacialAnimation() end, call)
        end
        showHelpTooltip(tLang.L('mesh_debug_facial_clip_stop_tooltip'))
        tImGui.EndDisabled()
        local duration = safe(call, function() return preview:getFacialClipDuration(state.clipSelected) end) or 0
        local time = safe(call, function() return preview:getFacialAnimationTime() end)
        if time ~= nil then
            tImGui.Text(string.format('%s: %.3f / %.3f', tLang.L('mesh_debug_facial_time'), time, duration))
        end
    end

    state.report = M.drawReport(preview, call)
    if state.failed then
        tImGui.TextWrapped(tLang.L('mesh_debug_facial_failed'))
    end
    if not ready then
        tImGui.TextWrapped(tLang.L('articulated_save_to_preview'))
    end
    tImGui.TextWrapped(tLang.L('mesh_debug_facial_preview'))
end

return M
