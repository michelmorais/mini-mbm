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

local M = {}

local function defaultCall(fn, ...)
    return true, fn(...)
end

local function finite(value)
    return type(value) == 'number' and value == value and math.abs(value) < math.huge
end

local function validVertex(vertex)
    return type(vertex) == 'table' and finite(vertex.x) and finite(vertex.y) and
        finite(vertex.z) and finite(vertex.u) and finite(vertex.v)
end

local function copyVertex(vertex)
    return {
        x = vertex.x, y = vertex.y, z = vertex.z,
        u = vertex.u, v = vertex.v,
        nx = vertex.nx, ny = vertex.ny, nz = vertex.nz,
    }
end

local function read(call, fn, ...)
    local ok, value, extra = call(fn, ...)
    if not ok then return nil, value end
    return value, extra
end

local function buildTriangles(vertices, indices)
    local triangles = {}
    local function addTriangle(indexA, indexB, indexC)
        local a, b, c = vertices[indexA], vertices[indexB], vertices[indexC]
        if not (a and b and c) then return false end
        triangles[#triangles + 1] = {
            a = a, b = b, c = c,
            uvA = {u = a.u, v = a.v},
            uvB = {u = b.u, v = b.v},
            uvC = {u = c.u, v = c.v},
        }
        return true
    end

    if type(indices) == 'table' and #indices > 0 then
        if #indices % 3 ~= 0 then return nil, 'index buffer is not divisible by three' end
        for i = 1, #indices, 3 do
            if not addTriangle(indices[i], indices[i + 1], indices[i + 2]) then
                return nil, 'index buffer references a missing vertex'
            end
        end
    else
        if #vertices % 3 ~= 0 then return nil, 'triangle list is not divisible by three' end
        for i = 1, #vertices, 3 do
            addTriangle(i, i + 1, i + 2)
        end
    end
    return triangles
end

--- Snapshot one frame/subset from a meshDebug object for CPU picking.
--- `call` is the editor's dpCall-compatible protected-call wrapper.
function M.snapshotSubset(meshDebugObject, frame, subset, call)
    assert(meshDebugObject, 'meshDebug object is required')
    frame = math.max(1, math.floor(tonumber(frame) or 1))
    subset = math.max(1, math.floor(tonumber(subset) or 1))
    call = call or defaultCall

    local totalVertices, errorMessage = read(call,
        function() return meshDebugObject:getTotalVertex(frame, subset) end)
    if not totalVertices then return nil, errorMessage or 'could not read vertex count' end
    totalVertices = math.floor(tonumber(totalVertices) or 0)
    if totalVertices <= 0 then return nil, 'subset has no vertices' end

    local rawVertices
    rawVertices, errorMessage = read(call,
        function() return meshDebugObject:getVertex(frame, subset, 1, totalVertices) end)
    if not rawVertices then return nil, errorMessage or 'could not read vertices' end
    if type(rawVertices) ~= 'table' or #rawVertices ~= totalVertices then
        return nil, 'vertex read returned an unexpected array' end

    local vertices = {}
    for i, vertex in ipairs(rawVertices) do
        if not validVertex(vertex) then return nil, 'vertex has invalid position or UV' end
        vertices[i] = copyVertex(vertex)
    end

    local indices
    indices, errorMessage = read(call,
        function() return meshDebugObject:getIndex(frame, subset) end)
    if errorMessage then return nil, errorMessage end
    if type(indices) ~= 'table' then indices = nil end
    local triangles, triangleError = buildTriangles(vertices, indices)
    if not triangles then return nil, triangleError end

    local texture, textureError = read(call,
        function() return meshDebugObject:getTexture(frame, subset) end)
    if textureError then return nil, textureError end

    return {
        frame = frame,
        subset = subset,
        vertices = vertices,
        indices = indices,
        triangles = triangles,
        texture = texture,
    }
end

return M
