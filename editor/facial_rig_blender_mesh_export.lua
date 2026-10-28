--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE|
| ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                  |
|------------------------------------------------------------------------------------------------------------------------|

    Minimal Mesh Debug-compatible dump used by the facial editor's fully automated Blender round
    trip. It intentionally writes the same JSON contract consumed by blender_mesh_skeleton_export.py
    and uses the engine's canonical Y-up data. The caller supplies the output path and runs this
    only on an explicit user action; no per-frame scans are performed.
]]--

local M = {}
local tMeshExport = require "mesh_debug_export_helper"

local function invoke(object, method, ...)
    local args = table.pack(...)
    local result = table.pack(pcall(function()
        return object[method](object, table.unpack(args, 1, args.n))
    end))
    if not result[1] then return false, result[2] end
    return true, table.unpack(result, 2, result.n)
end

local function jsonStr(value)
    local text = tostring(value or '')
    text = text:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n')
        :gsub('\r', '\\r'):gsub('\t', '\\t')
    return '"' .. text .. '"'
end

local function resolveTexture(path)
    if not path or path == '' then return nil end
    local candidate = mbm.getFullPath(path) or path
    local file = io.open(candidate, 'rb')
    if not file then return nil end
    file:close()
    return candidate
end

local function writeMatrix(file, matrix)
    local values = {}
    for index = 1, 16 do values[index] = string.format('%.9g', matrix[index] or 0) end
    file:write('[' .. table.concat(values, ', ') .. ']')
end

local function writeBones(file, meshDebug)
    local ok, report = invoke(meshDebug, 'getSkeletonBindReport')
    if not ok or type(report) ~= 'table' then
        file:write('  "canonicalSkeleton": false,\n  "joints": [],\n')
        return {}, false
    end
    local bones = report.canonical == true and (report.bones or {}) or {}
    file:write('  "canonicalSkeleton": ', report.canonical == true and 'true' or 'false', ',\n')
    file:write('  "joints": [\n')
    for index, bone in ipairs(bones) do
        local parent = nil
        if (bone.parentIndex or 0) > 0 and bones[bone.parentIndex] then
            parent = bones[bone.parentIndex].name
        end
        file:write(string.format('    { "name": %s, "parent": %s, "x": %.9g, "y": %.9g, "z": %.9g, ',
            jsonStr(bone.name), parent and jsonStr(parent) or 'null',
            bone.globalBindMatrix and bone.globalBindMatrix[13] or 0,
            bone.globalBindMatrix and bone.globalBindMatrix[14] or 0,
            bone.globalBindMatrix and bone.globalBindMatrix[15] or 0))
        file:write(string.format('"radius": %.9g, "length": %.9g, "globalBindMatrix": ',
            bone.radius or 0, bone.length or 0))
        writeMatrix(file, bone.globalBindMatrix or {})
        file:write(index < #bones and ' },\n' or ' }\n')
    end
    file:write('  ],\n')
    return bones, report.canonical == true
end

local function writeAnimations(file, meshDebug, bones)
    local ok, animations = invoke(meshDebug, 'getSkeletalAnimationReport')
    if not ok or type(animations) ~= 'table' then animations = {} end
    local sampleRate, maxDuration = 30, 0
    for _, clip in ipairs(animations) do
        maxDuration = math.max(maxDuration, tonumber(clip.duration) or 0)
        for _, track in ipairs(clip.tracks or {}) do
            local previous = nil
            for _, key in ipairs(track.keys or {}) do
                local time = tonumber(key.time)
                if previous and time and time > previous then
                    sampleRate = math.max(sampleRate, math.min(120, math.floor(1 / (time - previous) + 0.5)))
                end
                previous = time
            end
        end
    end
    if maxDuration > 0 and maxDuration * sampleRate > 10000 then
        sampleRate = math.max(1, math.floor(10000 / maxDuration))
    end
    file:write(string.format('  "animations": { "sampleRate": %d, "clips": [\n', sampleRate))
    for clipIndex, clip in ipairs(animations) do
        local duration = math.max(0, tonumber(clip.duration) or 0)
        local samples = math.ceil(duration * sampleRate)
        file:write(string.format('    { "name": %s, "duration": %.9g, "loop": %s, "samples": [\n',
            jsonStr(clip.name), duration, clip.loop and 'true' or 'false'))
        for sampleIndex = 0, samples do
            local time = math.min(duration, sampleIndex / sampleRate)
            local okPose, pose = invoke(meshDebug, 'evaluateSkeletalAuthoringPose', clipIndex, time, 'lbs')
            if not okPose or type(pose) ~= 'table' or type(pose.bones) ~= 'table' or #pose.bones ~= #bones then
                return false, string.format('failed to sample skeletal clip %s at %.4f', tostring(clip.name), time)
            end
            file:write(string.format('      { "time": %.9g, "globalMatrices": [', time))
            for boneIndex, bone in ipairs(pose.bones) do
                writeMatrix(file, bone.globalMatrix or {})
                if boneIndex < #pose.bones then file:write(', ') end
            end
            file:write(sampleIndex < samples and '] },\n' or '] }\n')
        end
        file:write(clipIndex < #animations and '    ] },\n' or '    ] }\n')
    end
    file:write('  ] },\n')
    return true
end

function M.write(meshDebug, path)
    assert(meshDebug and path and path ~= '', 'mesh export requires a mesh and output path')
    local okFrames, frameCount = invoke(meshDebug, 'getTotalFrame')
    if not okFrames or (tonumber(frameCount) or 0) < 1 then return false, 'mesh has no geometry frame' end
    local file = io.open(path, 'wb')
    if not file then return false, 'could not create Blender mesh dump: ' .. tostring(path) end
    local previousLocale = os.setlocale and os.setlocale(nil, 'numeric') or nil
    if os.setlocale then os.setlocale('C', 'numeric') end
    local function abort(message)
        file:close(); os.remove(path)
        if os.setlocale and previousLocale then os.setlocale(previousLocale, 'numeric') end
        return false, message
    end

    file:write('{\n')
    local bones = writeBones(file, meshDebug)
    local okAnimations, animationError = writeAnimations(file, meshDebug, bones)
    if not okAnimations then return abort(animationError) end
    file:write('  "mesh": { "vertices": [\n')
    local okSubsets, subsetCount = invoke(meshDebug, 'getTotalSubset', 1)
    subsetCount = okSubsets and tonumber(subsetCount) or 0
    local firstVertex, totalVertices = true, 0
    local subsetIndices = {}
    local hasWeights = select(2, invoke(meshDebug, 'hasSkeletalVertexWeights')) == true
    for subset = 1, subsetCount do
        local okVertices, vertexCount = invoke(meshDebug, 'getTotalVertex', 1, subset)
        vertexCount = okVertices and tonumber(vertexCount) or 0
        for vertex = 1, vertexCount do
            local okVertex, point = invoke(meshDebug, 'getVertex', 1, subset, vertex)
            if not okVertex or type(point) ~= 'table' then return abort('could not read mesh vertex') end
            if not firstVertex then file:write(',\n') end
            firstVertex = false
            local weightText = ''
            if hasWeights then
                local values = table.pack(invoke(meshDebug, 'getSkeletalVertexWeight', totalVertices + vertex))
                if values[1] == true and values[2] then
                    local names, weights = {}, {}
                    for slot = 2, 8, 2 do
                        if values[slot] then
                            names[#names + 1] = jsonStr(values[slot])
                            weights[#weights + 1] = string.format('%.6f', values[slot + 1] or 0)
                        end
                    end
                    if #names > 0 then
                        weightText = ', "boneNames": [' .. table.concat(names, ', ') ..
                            '], "weights": [' .. table.concat(weights, ', ') .. ']'
                    end
                end
            end
            file:write(string.format('    { "x": %.9g, "y": %.9g, "z": %.9g, "u": %.9g, "v": %.9g%s }',
                point.x or 0, point.y or 0, point.z or 0, point.u or 0, point.v or 0, weightText))
        end
        local okIndices, indices = invoke(meshDebug, 'getIndex', 1, subset)
        local globalIndices, indexError = tMeshExport.buildGlobalTriangleIndices(
            okIndices and indices or nil, vertexCount, totalVertices)
        if not globalIndices then return abort(indexError or 'invalid mesh triangle list') end
        subsetIndices[subset] = globalIndices
        totalVertices = totalVertices + vertexCount
    end
    file:write('\n  ], "subsets": [\n')
    for subset = 1, subsetCount do
        local okTexture, texture = invoke(meshDebug, 'getTexture', 1, subset)
        local materialTextures = {}
        for _, role in ipairs({'normal', 'specular', 'emissive', 'mask'}) do
            local okRole, rolePath = invoke(meshDebug, 'getMaterialTexture', 1, subset, role)
            local resolved = okRole and resolveTexture(rolePath) or nil
            if resolved then materialTextures[#materialTextures + 1] = jsonStr(role) .. ': ' .. jsonStr(resolved) end
        end
        file:write(string.format('    { "indices": [%s], "texture": %s, "materialTextures": {%s} }',
            table.concat(subsetIndices[subset] or {}, ', '),
            okTexture and resolveTexture(texture) and jsonStr(resolveTexture(texture)) or 'null',
            table.concat(materialTextures, ', ')))
        file:write(subset < subsetCount and ',\n' or '\n')
    end
    file:write('  ] }\n}\n')
    file:close()
    if os.setlocale and previousLocale then os.setlocale(previousLocale, 'numeric') end
    return true
end

return M
