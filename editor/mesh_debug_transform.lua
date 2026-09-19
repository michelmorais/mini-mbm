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

-- Read the canonical report only for explicit operations, never in the editor loop.
-- Dependency analysis is unnecessary for a whole-asset transform decision.
function M.apply(meshD, operation, xf)
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
