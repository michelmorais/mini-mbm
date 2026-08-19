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

local helper = require "blender_import_mode_helper"

local function options()
    return {
        bakeAnimation = true,
        animationClips = {
            { frameStart = 1, frameEnd = 10, sampleStep = 2 },
            { frameStart = 20, frameEnd = 24, sampleStep = 1 },
        },
    }
end

local skeletalScan = {
    skeletalCapability = {
        available = true,
        reason = 'Armature with bones and usable mesh skin weights found.',
        boneCount = 12,
        skinnedMeshCount = 1,
    },
}
local skeletalMode = helper.resolveImportMode(true, 'ready', skeletalScan)
assert(skeletalMode.mode == 'skeletal')
assert(helper.getAnimationToggleLabelKey(skeletalMode) == 'blender_import_animation_toggle_skeletal')
local skeletalEstimate = helper.estimateFrames(skeletalMode, options())
assert(skeletalEstimate.targetFrames == 1)
assert(skeletalEstimate.geometryFrames == 1)
assert(skeletalEstimate.skeletalKeySamples == 10)
assert(skeletalEstimate.bakedMeshFrames == 0)

local vbOnlyMode = helper.resolveImportMode(true, 'ready', skeletalScan, 'vb_only')
assert(vbOnlyMode.mode == 'baked')
assert(vbOnlyMode.skeletalAvailable == true)
assert(vbOnlyMode.fallbackExpected == true)
assert(vbOnlyMode.reasonCode == 'vb_only_no_canonical_weights')
assert(vbOnlyMode.reason:find('type%-42') ~= nil)
local vbOnlyEstimate = helper.estimateFrames(vbOnlyMode, options())
assert(vbOnlyEstimate.targetFrames == 10)
assert(vbOnlyEstimate.geometryFrames == 10)
assert(vbOnlyEstimate.bakedMeshFrames == 10)
assert(vbOnlyEstimate.skeletalKeySamples == 0)

local indexedMode = helper.resolveImportMode(true, 'ready', skeletalScan, 'fail')
assert(indexedMode.mode == 'skeletal')

local cacheFallbackScan = {
    skeletalCapability = {
        available = false,
        reason = 'Mesh Sequence Cache animation has no armature for real-time skeletal import.',
        armatureCount = 0,
        boneCount = 0,
        skinnedMeshCount = 0,
    },
}
local fallbackMode = helper.resolveImportMode(true, 'ready', cacheFallbackScan)
assert(fallbackMode.mode == 'baked')
assert(fallbackMode.fallbackExpected == true)
assert(helper.getAnimationToggleLabelKey(fallbackMode) == 'blender_import_animation_toggle_baked')
assert(fallbackMode.reason:find('Mesh Sequence Cache', 1, true) ~= nil)
local fallbackEstimate = helper.estimateFrames(fallbackMode, options())
assert(fallbackEstimate.targetFrames == 10)
assert(fallbackEstimate.bakedMeshFrames == 10)

local preferenceOffMode = helper.resolveImportMode(false, 'ready', skeletalScan)
assert(preferenceOffMode.mode == 'baked')
assert(preferenceOffMode.fallbackExpected == false)
assert(helper.estimateFrames(preferenceOffMode, options()).bakedMeshFrames == 10)

local unknownMode = helper.resolveImportMode(true, 'failed', nil)
assert(unknownMode.mode == 'unknown_preference')
assert(unknownMode.unknown == true)
assert(helper.getAnimationToggleLabelKey(unknownMode) == 'blender_import_animation_toggle_unknown')
local unknownEstimate = helper.estimateFrames(unknownMode, options())
assert(unknownEstimate.targetFrames == 10)
assert(unknownEstimate.unknown == true)
assert(helper.getAnimationToggleHelpKey() == 'blender_import_animation_toggle_help')

local missingCapabilityMode = helper.resolveImportMode(true, 'ready', {})
assert(missingCapabilityMode.mode == 'unknown_preference')
assert(missingCapabilityMode.unknown == true)
assert(missingCapabilityMode.fallbackExpected == false)
assert(missingCapabilityMode.capability.status == 'unknown')
assert(missingCapabilityMode.reason:find('metadata is missing', 1, true) ~= nil)

local rows = {
    { anim = { scanStatus = 'ready', scanData = skeletalScan } },
    { anim = { scanStatus = 'ready', scanData = cacheFallbackScan } },
    { anim = { scanStatus = 'failed', scanData = nil } },
}
local summary = helper.summarizeModes(rows, function() return options() end, true)
assert(summary.skeletalFiles == 1)
assert(summary.bakedFiles == 1)
assert(summary.fallbackFiles == 1)
assert(summary.unknownFiles == 1)
assert(summary.skeletalKeySamples == 10)
assert(summary.bakedMeshFrames == 10)

local vbOnlySummary = helper.summarizeModes(rows, function() return options() end, true, 'vb_only')
assert(vbOnlySummary.skeletalFiles == 0)
assert(vbOnlySummary.bakedFiles == 2)
assert(vbOnlySummary.fallbackFiles == 2)
assert(vbOnlySummary.unknownFiles == 1)
assert(vbOnlySummary.skeletalKeySamples == 0)
assert(vbOnlySummary.bakedMeshFrames == 20)

print('blender import mode helper tests passed')
