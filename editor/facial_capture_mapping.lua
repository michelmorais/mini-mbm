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
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Stable source-name mapping for optional Faceit/ARKit-style live-capture producers.
   This module deliberately has no camera, network, or SDK dependency. It converts a
   named weight packet into the target names already present in a loaded mesh.
]]--

local M = {version = 1}

local ALIASES = {
    blinkl = 'eyeBlinkLeft', blinkr = 'eyeBlinkRight',
    eyeblinkl = 'eyeBlinkLeft', eyeblinkr = 'eyeBlinkRight',
    eyelookdownl = 'eyeLookDownLeft', eyelookdownr = 'eyeLookDownRight',
    eyelookinl = 'eyeLookInLeft', eyelookinr = 'eyeLookInRight',
    eyelookoutl = 'eyeLookOutLeft', eyelookoutr = 'eyeLookOutRight',
    eyelookupl = 'eyeLookUpLeft', eyelookupr = 'eyeLookUpRight',
    eyesquintl = 'eyeSquintLeft', eyesquintr = 'eyeSquintRight',
    eyewidel = 'eyeWideLeft', eyewider = 'eyeWideRight',
    browdownl = 'browDownLeft', browdownr = 'browDownRight',
    browouterupl = 'browOuterUpLeft', browouterupr = 'browOuterUpRight',
    mouthsmilel = 'mouthSmileLeft', mouthsmiler = 'mouthSmileRight',
    mouthfrownl = 'mouthFrownLeft', mouthfrownr = 'mouthFrownRight',
    mouthdimplel = 'mouthDimpleLeft', mouthdimpler = 'mouthDimpleRight',
    mouthstretchl = 'mouthStretchLeft', mouthstretchr = 'mouthStretchRight',
    mouthpressl = 'mouthPressLeft', mouthpressr = 'mouthPressRight',
    mouthupperupl = 'mouthUpperUpLeft', mouthupperupr = 'mouthUpperUpRight',
    mouthlowerdownl = 'mouthLowerDownLeft', mouthlowerdownr = 'mouthLowerDownRight',
    cheekpuffl = 'cheekPuff', cheekpuffr = 'cheekPuff',
    cheekpuffleft = 'cheekPuff', cheekpuffright = 'cheekPuff',
    cheekpuff_l = 'cheekPuff', cheekpuff_r = 'cheekPuff',
    nosesneerl = 'noseSneerLeft', nosesneerr = 'noseSneerRight',
}

-- Keep aliases ASCII and separator-free after normalization. The explicit entries
-- above cover Faceit side suffixes; canonical ARKit names resolve by exact key.
ALIASES.mouthdimplel = 'mouthDimpleLeft'
ALIASES.mouthdimpler = 'mouthDimpleRight'
ALIASES.cheekpuffl = 'cheekPuff'
ALIASES.cheekpuffr = 'cheekPuff'

local function normalizeName(name)
    if type(name) ~= 'string' then return nil end
    local trimmed = name:match('^%s*(.-)%s*$')
    if trimmed == '' then return nil end
    return trimmed:gsub('[%s_%.:%-%/%\\]+', ''):lower()
end

local function finiteNumber(value)
    local number = tonumber(value)
    if not number or number ~= number or math.abs(number) == math.huge then return nil end
    return number
end

local function append(list, value)
    list[#list + 1] = value
end

local function resolve(index, sourceName)
    local key = normalizeName(sourceName)
    if not key then return nil, nil, nil end
    local exact = index.targetsByKey[key] or index.canonicalTargetsByKey[key]
    if exact then return exact, 'exact', key end
    local canonical = ALIASES[key]
    if not canonical then return nil, nil, key end
    local canonicalKey = normalizeName(canonical)
    local target = index.targetsByKey[canonicalKey] or index.canonicalTargetsByKey[canonicalKey]
    if not target then return nil, 'alias-missing', key end
    return target, 'alias', key
end

function M.normalizeName(name)
    return normalizeName(name)
end

function M.buildTargetIndex(targetNames, options)
    assert(type(targetNames) == 'table', 'facial target names must be a table')
    options = options or {}
    local index = {
        targets = {},
        targetsByKey = {},
        canonicalTargetsByKey = {},
        requiredTargets = {},
        missingTargets = {},
        duplicateTargets = {},
        lastWeights = {},
        epsilon = math.max(finiteNumber(options.epsilon) or 0.0001, 0),
    }
    for position, name in ipairs(targetNames) do
        if type(name) == 'string' and name ~= '' then
            local key = normalizeName(name)
            if key and not index.targetsByKey[key] then
                index.targetsByKey[key] = name
                index.targets[#index.targets + 1] = name
                local canonical = ALIASES[key]
                if canonical then
                    local canonicalKey = normalizeName(canonical)
                    if canonicalKey and not index.canonicalTargetsByKey[canonicalKey] then
                        index.canonicalTargetsByKey[canonicalKey] = name
                    end
                end
            elseif key then
                append(index.duplicateTargets, {key = key, first = index.targetsByKey[key], duplicate = name})
            end
        elseif name ~= nil then
            append(index.duplicateTargets, {position = position, duplicate = name})
        end
    end
    if type(options.requiredTargets) == 'table' then
        for _, required in ipairs(options.requiredTargets) do
            local target = index.targetsByKey[normalizeName(required) or '']
            index.requiredTargets[#index.requiredTargets + 1] = required
            if not target then append(index.missingTargets, required) end
        end
    end
    return index
end

function M.build(mesh, options)
    assert(mesh and type(mesh.getTotalFacialTargets) == 'function', 'mesh facial target API is required')
    local names = {}
    local total = mesh:getTotalFacialTargets() or 0
    for index = 1, total do
        local name = mesh:getFacialTargetName(index)
        if name then names[#names + 1] = name end
    end
    return M.buildTargetIndex(names, options)
end

function M.resolveSourceName(index, sourceName)
    assert(type(index) == 'table' and type(index.targetsByKey) == 'table', 'invalid facial target index')
    local target, kind = resolve(index, sourceName)
    return target, kind
end

function M.apply(mesh, index, weights, options)
    assert(mesh and type(mesh.setFacialWeight) == 'function', 'mesh facial weight API is required')
    assert(type(index) == 'table' and type(index.targetsByKey) == 'table', 'invalid facial target index')
    assert(type(weights) == 'table', 'facial weights must be a name-to-number table')
    options = options or {}
    local epsilon = math.max(finiteNumber(options.epsilon) or index.epsilon or 0.0001, 0)
    local candidates = {}
    local result = {
        mapped = 0, changed = 0, unchanged = 0, clamped = 0, rejected = 0,
        failed = 0, unknownSource = {}, duplicateSource = {}, missingAliasTarget = {},
    }
    for sourceName, rawValue in pairs(weights) do
        local target, kind, key = resolve(index, sourceName)
        if not target then
            if kind == 'alias-missing' then append(result.missingAliasTarget, sourceName)
            else append(result.unknownSource, sourceName) end
        else
            local value = finiteNumber(rawValue)
            if not value then
                result.rejected = result.rejected + 1
            else
                if value < 0 then value = 0; result.clamped = result.clamped + 1 end
                if value > 1 then value = 1; result.clamped = result.clamped + 1 end
                local existing = candidates[target]
                local priority = kind == 'exact' and 2 or 1
                if existing then
                    append(result.duplicateSource, {target = target, first = existing.source, duplicate = sourceName})
                    if priority > existing.priority or (priority == existing.priority and tostring(sourceName) < tostring(existing.source)) then
                        candidates[target] = {value = value, source = sourceName, priority = priority, key = key}
                    end
                else
                    candidates[target] = {value = value, source = sourceName, priority = priority, key = key}
                end
            end
        end
    end
    for target, candidate in pairs(candidates) do
        result.mapped = result.mapped + 1
        local previous = index.lastWeights[target]
        if options.force or previous == nil or math.abs(previous - candidate.value) > epsilon then
            if mesh:setFacialWeight(target, candidate.value) then
                index.lastWeights[target] = candidate.value
                result.changed = result.changed + 1
            else
                result.failed = result.failed + 1
            end
        else
            result.unchanged = result.unchanged + 1
        end
    end
    if options.clearMissing then
        for _, target in ipairs(index.targets) do
            if candidates[target] == nil and index.lastWeights[target] ~= nil then
                if mesh:setFacialWeight(target, 0) then
                    index.lastWeights[target] = 0
                    result.changed = result.changed + 1
                else
                    result.failed = result.failed + 1
                end
            end
        end
    end
    return result
end

function M.reset(mesh, index)
    assert(mesh and type(mesh.resetFacialWeights) == 'function', 'mesh facial reset API is required')
    local ok = mesh:resetFacialWeights()
    if index then index.lastWeights = {} end
    return ok
end

return M
