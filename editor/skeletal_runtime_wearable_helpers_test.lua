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
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|
]]--

local helper = require "skeletal_runtime_wearable_helpers"

local compatible = helper.formatCompatibilityReport({
    compatible=true,
    reason='compatible',
    boneCount=23,
})
assert(compatible:find('Compatibility: valid %(compatible%)', 1, false) ~= nil)
assert(compatible:find('Bones: 23', 1, false) ~= nil)

local mismatch = helper.formatCompatibilityReport({
    compatible=false,
    reason='bone_identity_mismatch',
    boneCount=23,
    boneIndex=7,
    boneName='Arm',
    boneId='0x1',
    otherBoneId='0x2',
})
assert(mismatch:find('Compatibility: invalid %(bone_identity_mismatch%)', 1, false) ~= nil)
assert(mismatch:find('Mismatch bone: 7 "Arm"', 1, false) ~= nil)
assert(mismatch:find('Bone IDs: 0x1 vs 0x2', 1, false) ~= nil)

local primary = {
    getResolvedSkeletalSkinningMethod=function() return 'dqs' end,
    getSkeletalSkinningMethod=function() return 'auto' end,
    getSkeletalExecutionPath=function() return 'cpu' end,
}
assert(helper.primarySkinningMethod(primary) == 'dqs')
assert(helper.primaryExecutionPath(primary) == 'cpu')

local fallbackPrimary = {
    getResolvedSkeletalSkinningMethod=function() return 'unresolved' end,
    getSkeletalSkinningMethod=function() return 'lbs' end,
    getSkeletalExecutionPath=function() return 'bogus' end,
}
assert(helper.primarySkinningMethod(fallbackPrimary) == 'lbs')
assert(helper.primaryExecutionPath(fallbackPrimary) == 'gpu')
assert(helper.primaryExecutionPath(nil) == 'gpu')

local entry = helper.newFollowerEntry('/tmp/hat.msh')
assert(entry.path == '/tmp/hat.msh')
assert(entry.visible == true)
assert(entry.preview == nil)

local followers = {
    entry,
    helper.newFollowerEntry('/tmp/cape.msh'),
    helper.newFollowerEntry('/tmp/boots.msh'),
}
followers[2].preview = {}
followers[3].preview = false
assert(helper.countLoadedFollowers(followers) == 2)
assert(helper.countLoadedFollowers(nil) == 0)

print('skeletal wearable helper tests passed')
