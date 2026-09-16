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

-- Read-only runtime preview. Cache clip names until the preview instance changes.
local M = {}

function M.draw(entry, preview, ready, call)
    if not preview or not preview.getTotalSkeletalAnimations then
        tImGui.TextDisabled(tLang.L('mesh_debug_skeletal_no_clips'))
        return
    end
    local state = entry.skeletalPlayback
    if not state or state.preview ~= preview then
        state = {preview = preview, clips = {}, selected = 1, mode = 1, weight = 0.5}
        entry.skeletalPlayback = state
        for i = 1, preview:getTotalSkeletalAnimations() do
            state.clips[i] = preview:getSkeletalAnimationName(i)
        end
    end
    if #state.clips == 0 then
        tImGui.TextDisabled(tLang.L('mesh_debug_skeletal_no_clips'))
        return
    end
    local function apply(fn)
        local ok, result = call(fn)
        state.failed = not ok or result == false
        return not state.failed
    end
    tImGui.BeginDisabled(not ready)
    tImGui.PushItemWidth(220)
    local changed, selected = tImGui.Combo(tLang.L('swl_skeletal_clip'), state.selected, state.clips)
    if changed and ready then state.selected = selected end
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
