--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

local M = {}

local function wholeMesh(xf)
    return (xf.frame or 0) == 0 and (xf.subset or 0) == 0
end

local ratioAxes = {'', 'X', 'Y', 'Z'}
local ratioFields = {'', 'targetWidth', 'targetHeight', 'targetDepth'}
local ratioSizes = {'', 'width', 'height', 'depth'}

function M.ratioAxis(xf)
    return ratioAxes[xf.keepRatio or 1]
end

function M.syncRatio(xf, bounds)
    local mode = xf.keepRatio or 1
    if mode == 1 or not bounds then return end
    local size = bounds[ratioSizes[mode]]
    if size <= 1e-7 then return end
    local factor = xf[ratioFields[mode]] / size
    for i = 2, 4 do
        if i ~= mode then xf[ratioFields[i]] = bounds[ratioSizes[i]] * factor end
    end
end

function M.drawRatio(imgui, lang, xf, id, bounds)
    local changed, mode = imgui.Combo(lang.L('transform_keep_ratio') .. '##' .. id,
        xf.keepRatio or 1, {lang.L('none'), lang.L('width'), lang.L('height'), lang.L('depth')}, -1)
    if changed then xf.keepRatio = mode end
    -- Bounds are cached by the caller. No vertex scan or mesh mutation in the idle loop.
    if changed or xf.ratioBounds ~= bounds then
        xf.ratioBounds = bounds
        M.syncRatio(xf, bounds)
    end
end

function M.drawInvert(imgui, lang, xf, id)
    imgui.Separator()
    xf.invertX = imgui.Checkbox(lang.L('transform_invert_x') .. '##' .. id, xf.invertX or false)
    imgui.SameLine()
    xf.invertY = imgui.Checkbox(lang.L('transform_invert_y') .. '##' .. id, xf.invertY or false)
    imgui.SameLine()
    xf.invertZ = imgui.Checkbox(lang.L('transform_invert_z') .. '##' .. id, xf.invertZ or false)
    imgui.BeginDisabled(not (xf.invertX or xf.invertY or xf.invertZ))
    local apply = imgui.Button(lang.L('transform_apply_invert') .. '##' .. id)
    imgui.EndDisabled()
    return apply
end

-- Reflection changes handedness for one or three axes. Keep the original draw/cull
-- mode and repair only the selected triangles; changing global front-face state would
-- invert every untouched subset too. All work here runs only on Apply.
local function invertGeometry(meshD, xf)
    local sx, sy, sz = xf.invertX and -1 or 1, xf.invertY and -1 or 1, xf.invertZ and -1 or 1
    local reverse = sx * sy * sz < 0
    local mode = meshD:getModeDraw()
    if reverse and (mode == 'TRIANGLE_STRIP' or mode == 'TRIANGLE_FAN') then
        error('transform_invert_requires_triangles', 0)
    end
    local frame, subset = xf.frame or 0, xf.subset or 0
    local firstFrame, lastFrame = frame, frame
    local targets = {}
    -- Validate/read topology before mutation. A vertex swap on non-indexed skeletal
    -- geometry would detach the vertex from its skin influences, which Lua cannot swap.
    local report = meshD:getSkeletonBindReport(false)
    local skeletal = report and report.canonical and (report.boneCount or 0) > 0
    if skeletal and wholeMesh(xf) then error('bones_uniform_positive_scale_required', 0) end
    if frame == 0 then firstFrame, lastFrame = 1, meshD:getTotalFrame() end
    for f = firstFrame, lastFrame do
        local firstSubset, lastSubset = subset, subset
        if subset == 0 then firstSubset, lastSubset = 1, meshD:getTotalSubset(f) end
        for sub = firstSubset, lastSubset do
            local count = meshD:getTotalVertex(f, sub)
            local indices = reverse and mode == 'TRIANGLES' and meshD:getIndex(f, sub) or nil
            if reverse and mode == 'TRIANGLES' and not indices and skeletal then
                error('transform_invert_skin_order', 0)
            end
            targets[#targets+1] = {frame=f, subset=sub, count=count, indices=indices}
        end
    end
    for _, target in ipairs(targets) do
        local f, sub, count = target.frame, target.subset, target.count
        -- Preserve UVs and authored smooth/hard normals rather than recomputing them.
        for v = 1, count do
            local vertex = meshD:getVertex(f, sub, v)
            vertex.x, vertex.y, vertex.z = vertex.x*sx, vertex.y*sy, vertex.z*sz
            vertex.nx, vertex.ny, vertex.nz = vertex.nx*sx, vertex.ny*sy, vertex.nz*sz
            meshD:setVertex(f, sub, v, vertex)
        end
        if reverse and mode == 'TRIANGLES' then
            if target.indices then
                local indices = target.indices
                for i = 1, #indices-2, 3 do indices[i+1], indices[i+2] = indices[i+2], indices[i+1] end
                if #indices > 0 then meshD:addIndex(f, sub, indices) end
            else
                for i = 1, count-2, 3 do
                    local b, c = meshD:getVertex(f, sub, i+1), meshD:getVertex(f, sub, i+2)
                    meshD:setVertex(f, sub, i+1, c)
                    meshD:setVertex(f, sub, i+2, b)
                end
            end
        end
    end
end

-- Read the canonical report only for explicit operations, never in the editor loop.
-- Dependency analysis is unnecessary for a whole-asset transform decision.
function M.apply(meshD, operation, xf)
    if operation == 'invert' then
        if not (xf.invertX or xf.invertY or xf.invertZ) then return end
        return invertGeometry(meshD, xf)
    end
    local frame, subset = xf.frame or 0, xf.subset or 0
    local combined = operation == 'combined'
    local rotate = operation == 'rotate' or (combined and (xf.rx ~= 0 or xf.ry ~= 0 or xf.rz ~= 0))
    local translate = operation == 'translate' or (combined and (xf.dx ~= 0 or xf.dy ~= 0 or xf.dz ~= 0))
    local scale = operation == 'scale' or (combined and (xf.sx ~= 1 or xf.sy ~= 1 or xf.sz ~= 1))
    local centralize = operation == 'centralize' or operation == 'centralizeItself'
    -- centralize uses subset as an anchor but moves every subset in the selected frames.
    local whole = wholeMesh(xf) or (operation == 'centralize' and frame == 0)
    local skeletal = false
    if whole then
        local report = meshD:getSkeletonBindReport(false)
        skeletal = report ~= nil and report.canonical == true and (report.boneCount or 0) > 0
    end
    -- Preflight the entire combined operation before touching any geometry or skeletal data.
    if skeletal and (rotate or translate or centralize) then
        error('mesh_debug_skeletal_transform_blocked', 0)
    end
    if skeletal and scale then
        local sx, sy, sz = xf.sx, xf.sy, xf.sz
        local tolerance = math.max(1, math.abs(sx), math.abs(sy), math.abs(sz)) * 0.000001
        if sx ~= sx or sy ~= sy or sz ~= sz or math.abs(sx) == math.huge or
                math.abs(sy) == math.huge or math.abs(sz) == math.huge or
                sx <= 0 or sy <= 0 or sz <= 0 or math.abs(sx-sy) > tolerance or math.abs(sx-sz) > tolerance then
            error('bones_uniform_positive_scale_required', 0)
        end
    end
    if rotate then meshD:rotateFrame(frame, xf.rx, xf.ry, xf.rz, subset) end
    if scale then
        if skeletal then meshD:scaleSkeletalAsset(xf.sx)
        else meshD:scaleFrame(frame, xf.sx, xf.sy, xf.sz, subset) end
    end
    if translate then meshD:translateFrame(frame, xf.dx, xf.dy, xf.dz, subset) end
    if centralize then meshD[operation](meshD, frame, subset) end
end

return M
