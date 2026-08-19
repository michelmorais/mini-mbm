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

local M = {}

local function valueOrUnknown(value, labels)
    if value == nil or value == '' then return labels.unknown or 'unknown' end
    return tostring(value)
end

function M.formatCompatibilityReport(report, labels)
    labels = labels or {}
    if type(report) ~= 'table' then
        return labels.unavailable or 'Compatibility: unavailable'
    end
    local lines = {
        string.format(labels.compatibilityFmt or 'Compatibility: %s (%s)',
            report.compatible and (labels.valid or 'valid') or (labels.invalid or 'invalid'),
            valueOrUnknown(report.reason, labels)),
    }
    if report.boneCount then
        lines[#lines + 1] = string.format(labels.bonesFmt or 'Bones: %d', report.boneCount)
    end
    if report.boneIndex then
        local boneName = report.boneName and (' "' .. report.boneName .. '"') or ''
        lines[#lines + 1] = string.format(labels.mismatchBoneFmt or 'Mismatch bone: %d%s',
            report.boneIndex, boneName)
    end
    if report.boneId or report.otherBoneId then
        lines[#lines + 1] = string.format(labels.boneIdsFmt or 'Bone IDs: %s vs %s',
            valueOrUnknown(report.boneId, labels), valueOrUnknown(report.otherBoneId, labels))
    end
    if report.parentIndex or report.otherParentIndex then
        lines[#lines + 1] = string.format(labels.parentIndicesFmt or 'Parent indices: %s vs %s',
            valueOrUnknown(report.parentIndex, labels), valueOrUnknown(report.otherParentIndex, labels))
    end
    if report.observedError or report.tolerance then
        lines[#lines + 1] = string.format(labels.bindErrorFmt or 'Bind error: %s (tolerance %s)',
            valueOrUnknown(report.observedError, labels), valueOrUnknown(report.tolerance, labels))
    end
    return table.concat(lines, '\n')
end

function M.primarySkinningMethod(primary)
    if not primary then return nil end
    local resolved = primary.getResolvedSkeletalSkinningMethod and
        primary:getResolvedSkeletalSkinningMethod() or nil
    if resolved == 'lbs' or resolved == 'dqs' then return resolved end
    local requested = primary.getSkeletalSkinningMethod and primary:getSkeletalSkinningMethod() or nil
    if requested == 'lbs' or requested == 'dqs' then return requested end
    return nil
end

return M
