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
|------------------------------------------------------------------------------------------------------------------------|
]]--

local M = {}

local function floorInt(value, defaultValue)
    local n = math.floor(tonumber(value or defaultValue) or defaultValue or 0)
    return n
end

function M.getBakedFrameCount(frameStart, frameEnd, sampleStep)
    local startFrame = floorInt(frameStart, 1)
    local endFrame = floorInt(frameEnd, startFrame)
    local step = math.max(1, floorInt(sampleStep, 1))
    local total = math.max(1, math.abs(endFrame - startFrame) + 1)
    return math.floor((total - 1) / step) + 1
end

function M.getSelectedSampleCount(options)
    options = options or {}
    if options.bakeAnimation and type(options.animationClips) == 'table' and #options.animationClips > 0 then
        local total = 0
        for i = 1, #options.animationClips do
            local clip = options.animationClips[i] or {}
            total = total + M.getBakedFrameCount(clip.frameStart, clip.frameEnd, clip.sampleStep)
        end
        return total
    end
    if options.bakeAnimation then
        return M.getBakedFrameCount(options.frameStart, options.frameEnd, options.sampleStep)
    end
    return 1
end

function M.normalizeSkeletalCapability(scanStatus, scanData)
    if scanStatus == 'ready' and type(scanData) == 'table' and type(scanData.skeletalCapability) == 'table' then
        local cap = scanData.skeletalCapability
        return {
            status = cap.available == true and 'available' or 'unavailable',
            available = cap.available == true,
            reason = tostring(cap.reason or ''),
            armatureCount = floorInt(cap.armatureCount, 0),
            boneCount = floorInt(cap.boneCount, 0),
            skinnedMeshCount = floorInt(cap.skinnedMeshCount, 0),
        }
    end
    if scanStatus == 'ready' then
        return {
            status = 'unknown',
            available = false,
            reason = 'Skeletal capability metadata is missing from this scan.',
            armatureCount = 0,
            boneCount = 0,
            skinnedMeshCount = 0,
        }
    end
    if scanStatus == 'failed' then
        return {
            status = 'unknown',
            available = false,
            reason = 'Blender scan failed.',
            armatureCount = 0,
            boneCount = 0,
            skinnedMeshCount = 0,
        }
    end
    return {
        status = 'unknown',
        available = false,
        reason = 'Blender scan has not completed.',
        armatureCount = 0,
        boneCount = 0,
        skinnedMeshCount = 0,
    }
end

local function normalizeLargeMeshMode(largeMeshMode)
    largeMeshMode = tostring(largeMeshMode or 'fail')
    if largeMeshMode == 'vb_only' then
        return 'vb_only'
    end
    return 'fail'
end

local function getSourceFrameCount(src)
    if type(src) ~= 'table' then return 1 end
    local startFrame = floorInt(src.frameStart, 1)
    local endFrame = floorInt(src.frameEnd, startFrame)
    return math.max(1, math.abs(endFrame - startFrame) + 1)
end

local function isSceneRangeSource(src)
    return type(src) == 'table' and tostring(src.kind or '') == 'scene_range'
end

local function isExplicitAnimationSource(src)
    if type(src) ~= 'table' then return false end
    local kind = tostring(src.kind or '')
    return kind == 'action' or kind == 'nla'
end

function M.isExplicitSkeletalAnimationSource(src)
    if not isExplicitAnimationSource(src) or getSourceFrameCount(src) <= 1 then
        return false
    end
    if src.hasSkeletalAnimation == true or src.hasArmatureAnimation == true or src.isCanonicalArmatureSource == true then
        return true
    end

    local objectType = tostring(src.objectType or src.type or ''):lower()
    if objectType == 'armature' then
        return true
    end

    local objectName = tostring(src.object or ''):lower()
    if objectName:find('armature', 1, true) ~= nil then
        return true
    end

    local reason = tostring(src.reason or ''):lower()
    return reason:find('armature', 1, true) ~= nil
end

function M.selectDefaultAnimationSourceIndices(scanData, preferSkeletal, preserveManualSelection)
    if preserveManualSelection then
        return {}, 'manual_preserved'
    end
    if not preferSkeletal or type(scanData) ~= 'table' or type(scanData.sources) ~= 'table' then
        return {}, 'none'
    end

    local cap = type(scanData.skeletalCapability) == 'table' and scanData.skeletalCapability or nil
    local skeletalExplicitlyUnavailable = cap and cap.available == false
    local selected = {}
    if not skeletalExplicitlyUnavailable then
        for i = 1, #scanData.sources do
            if M.isExplicitSkeletalAnimationSource(scanData.sources[i]) then
                selected[#selected + 1] = i
            end
        end
        if #selected > 0 then
            return selected, 'explicit_skeletal'
        end
    end

    for i = 1, #scanData.sources do
        local src = scanData.sources[i]
        if isSceneRangeSource(src) and getSourceFrameCount(src) > 1 and tostring(src.confidence or '') ~= 'low' then
            selected[1] = i
            return selected, 'scene_range_fallback'
        end
    end
    return {}, 'none'
end

function M.getVbOnlySkeletalFallbackReason()
    return 'VB-only large mesh mode cannot export canonical type-42 skin weights.'
end

function M.resolveImportMode(preferSkeletal, scanStatus, scanData, largeMeshMode)
    if not preferSkeletal then
        return {
            mode = 'baked',
            skeletalAvailable = false,
            fallbackExpected = false,
            unknown = false,
            reason = 'Skeletal preference is disabled.',
        }
    end

    local cap = M.normalizeSkeletalCapability(scanStatus, scanData)
    if cap.status == 'available' then
        if normalizeLargeMeshMode(largeMeshMode) == 'vb_only' then
            return {
                mode = 'baked',
                skeletalAvailable = true,
                fallbackExpected = true,
                unknown = false,
                reasonCode = 'vb_only_no_canonical_weights',
                reason = M.getVbOnlySkeletalFallbackReason(),
                capability = cap,
            }
        end
        return {
            mode = 'skeletal',
            skeletalAvailable = true,
            fallbackExpected = false,
            unknown = false,
            reason = cap.reason,
            capability = cap,
        }
    end
    if cap.status == 'unavailable' then
        return {
            mode = 'baked',
            skeletalAvailable = false,
            fallbackExpected = true,
            unknown = false,
            reason = cap.reason,
            capability = cap,
        }
    end
    return {
        mode = 'unknown_preference',
        skeletalAvailable = false,
        fallbackExpected = false,
        unknown = true,
        reason = cap.reason,
        capability = cap,
    }
end

function M.estimateFrames(modeInfo, options)
    modeInfo = modeInfo or {}
    local samples = M.getSelectedSampleCount(options)
    if modeInfo.mode == 'skeletal' then
        return {
            targetFrames = 1,
            geometryFrames = 1,
            bakedMeshFrames = 0,
            skeletalKeySamples = samples,
        }
    end
    if modeInfo.mode == 'unknown_preference' then
        return {
            targetFrames = samples,
            geometryFrames = samples,
            bakedMeshFrames = samples,
            skeletalKeySamples = samples,
            unknown = true,
        }
    end
    return {
        targetFrames = samples,
        geometryFrames = samples,
        bakedMeshFrames = samples,
        skeletalKeySamples = 0,
    }
end

function M.getAnimationToggleLabelKey(modeInfo)
    modeInfo = modeInfo or {}
    if modeInfo.mode == 'skeletal' then
        return 'blender_import_animation_toggle_skeletal'
    end
    if modeInfo.unknown then
        return 'blender_import_animation_toggle_unknown'
    end
    return 'blender_import_animation_toggle_baked'
end

function M.getAnimationToggleHelpKey()
    return 'blender_import_animation_toggle_help'
end

function M.getAnimationPresentationKeys(modeInfo)
    modeInfo = modeInfo or {}
    local out = {
        sourceFramesColumnKey = 'blender_anim_col_source_frames',
        sourceRangeHelpKey = 'blender_anim_source_range_help',
        frameStartKey = 'blender_import_source_frame_start',
        frameEndKey = 'blender_import_source_frame_end',
    }
    if modeInfo.mode == 'skeletal' then
        out.selectedClipsTitleKey = 'blender_anim_selected_skeletal_clips_title'
        out.sampleCountColumnKey = 'blender_anim_col_key_samples'
        return out
    end
    if modeInfo.unknown then
        out.selectedClipsTitleKey = 'blender_anim_selected_source_clips_title'
        out.sampleCountColumnKey = 'blender_anim_col_samples'
        return out
    end
    out.selectedClipsTitleKey = 'blender_anim_selected_baked_clips_title'
    out.sampleCountColumnKey = 'blender_anim_col_mesh_frames'
    return out
end

function M.summarizeModes(rows, getOptionsForRow, preferSkeletal, largeMeshMode)
    local out = {
        skeletalFiles = 0,
        bakedFiles = 0,
        fallbackFiles = 0,
        unknownFiles = 0,
        skeletalKeySamples = 0,
        bakedMeshFrames = 0,
    }
    for i = 1, #(rows or {}) do
        local row = rows[i]
        local anim = row and row.anim
        local modeInfo = M.resolveImportMode(preferSkeletal, anim and anim.scanStatus, anim and anim.scanData, largeMeshMode)
        local est = M.estimateFrames(modeInfo, getOptionsForRow(row))
        if modeInfo.mode == 'skeletal' then
            out.skeletalFiles = out.skeletalFiles + 1
        elseif modeInfo.unknown then
            out.unknownFiles = out.unknownFiles + 1
        else
            out.bakedFiles = out.bakedFiles + 1
        end
        if modeInfo.fallbackExpected then
            out.fallbackFiles = out.fallbackFiles + 1
        end
        if modeInfo.mode == 'skeletal' then
            out.skeletalKeySamples = out.skeletalKeySamples + (est.skeletalKeySamples or 0)
        elseif not modeInfo.unknown then
            out.bakedMeshFrames = out.bakedMeshFrames + (est.bakedMeshFrames or 0)
        end
    end
    return out
end

return M
