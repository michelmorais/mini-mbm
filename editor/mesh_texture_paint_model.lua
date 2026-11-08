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

local function uvPointKey(point)
    return string.format('%.9f,%.9f', point.u, point.v)
end

local function uvEdgeKey(first, second)
    local left, right = uvPointKey(first), uvPointKey(second)
    if left > right then left, right = right, left end
    return left .. '|' .. right
end

function M.buildUvBoundaryEdges(triangles)
    assert(type(triangles) == 'table', 'triangles must be a table')
    local edges = {}
    local function add(first, second)
        if not (first and second and finite(first.u) and finite(first.v) and
                finite(second.u) and finite(second.v)) then return end
        local key = uvEdgeKey(first, second)
        local entry = edges[key]
        if entry then
            entry.count = entry.count + 1
        else
            edges[key] = {count = 1, a = {u = first.u, v = first.v},
                b = {u = second.u, v = second.v}}
        end
    end
    for _, triangle in ipairs(triangles) do
        add(triangle.uvA, triangle.uvB)
        add(triangle.uvB, triangle.uvC)
        add(triangle.uvC, triangle.uvA)
    end
    local result = {}
    for _, edge in pairs(edges) do
        if edge.count == 1 then
            result[#result + 1] = {a = edge.a, b = edge.b}
        end
    end
    table.sort(result, function(left, right)
        if left.a.u ~= right.a.u then return left.a.u < right.a.u end
        if left.a.v ~= right.a.v then return left.a.v < right.a.v end
        if left.b.u ~= right.b.u then return left.b.u < right.b.u end
        return left.b.v < right.b.v
    end)
    return result
end

local function islandVertexKey(triangle, vertex, uv)
    return string.format('%s|%.9f,%.9f,%.9f|%.9f,%.9f', tostring(triangle.subset or 0),
        vertex.x, vertex.y, vertex.z, uv.u, uv.v)
end

--- Build connected UV islands from triangles that share the same mesh vertex and UV.
--- A UV seam naturally gets different keys and therefore starts a new island.
function M.buildUvIslands(triangles)
    assert(type(triangles) == 'table', 'triangles must be a table')
    local parent, rank, owners = {}, {}, {}
    for index = 1, #triangles do parent[index], rank[index] = index, 0 end
    local function find(index)
        while parent[index] ~= index do
            parent[index] = parent[parent[index]]
            index = parent[index]
        end
        return index
    end
    local function union(left, right)
        left, right = find(left), find(right)
        if left == right then return end
        if rank[left] < rank[right] then left, right = right, left end
        parent[right] = left
        if rank[left] == rank[right] then rank[left] = rank[left] + 1 end
    end
    for index, triangle in ipairs(triangles) do
        local vertices = {{triangle.a, triangle.uvA}, {triangle.b, triangle.uvB},
            {triangle.c, triangle.uvC}}
        for _, item in ipairs(vertices) do
            local vertex, uv = item[1], item[2]
            if validVertex(vertex) and uv and finite(uv.u) and finite(uv.v) then
                local key = islandVertexKey(triangle, vertex, uv)
                if owners[key] then union(index, owners[key]) else owners[key] = index end
            end
        end
    end
    local rootToIsland, islands, islandByTriangle = {}, {}, {}
    for index = 1, #triangles do
        local root = find(index)
        local island = rootToIsland[root]
        if not island then
            island = #islands + 1
            rootToIsland[root] = island
            islands[island] = {id = island, triangleIndices = {}}
        end
        islandByTriangle[index] = island
        islands[island].triangleIndices[#islands[island].triangleIndices + 1] = index
    end
    return {count = #islands, islands = islands, islandByTriangle = islandByTriangle}
end

local function uvCross(first, second, third)
    return (second.u - first.u) * (third.v - first.v) -
        (second.v - first.v) * (third.u - first.u)
end

local function uvPointInsideStrict(point, triangle)
    local first = uvCross(triangle.uvA, triangle.uvB, point)
    local second = uvCross(triangle.uvB, triangle.uvC, point)
    local third = uvCross(triangle.uvC, triangle.uvA, point)
    local epsilon = 1e-9
    return (first > epsilon and second > epsilon and third > epsilon) or
        (first < -epsilon and second < -epsilon and third < -epsilon)
end

local function uvProperSegmentsIntersect(first, second, third, fourth)
    local ab = uvCross(first, second, third)
    local ac = uvCross(first, second, fourth)
    local cd = uvCross(third, fourth, first)
    local ce = uvCross(third, fourth, second)
    local epsilon = 1e-9
    return ((ab > epsilon and ac < -epsilon) or (ab < -epsilon and ac > epsilon)) and
        ((cd > epsilon and ce < -epsilon) or (cd < -epsilon and ce > epsilon))
end

local function uvTrianglesOverlap(first, second)
    local firstPoints = {first.uvA, first.uvB, first.uvC}
    local secondPoints = {second.uvA, second.uvB, second.uvC}
    local function bounds(points)
        local minU, maxU = points[1].u, points[1].u
        local minV, maxV = points[1].v, points[1].v
        for index = 2, #points do
            minU, maxU = math.min(minU, points[index].u), math.max(maxU, points[index].u)
            minV, maxV = math.min(minV, points[index].v), math.max(maxV, points[index].v)
        end
        return minU, maxU, minV, maxV
    end
    local minU1, maxU1, minV1, maxV1 = bounds(firstPoints)
    local minU2, maxU2, minV2, maxV2 = bounds(secondPoints)
    local epsilon = 1e-9
    if maxU1 <= minU2 + epsilon or maxU2 <= minU1 + epsilon or
            maxV1 <= minV2 + epsilon or maxV2 <= minV1 + epsilon then
        return false
    end
    for _, point in ipairs(firstPoints) do
        if uvPointInsideStrict(point, second) then return true end
    end
    for _, point in ipairs(secondPoints) do
        if uvPointInsideStrict(point, first) then return true end
    end
    local firstEdges = {{first.uvA, first.uvB}, {first.uvB, first.uvC}, {first.uvC, first.uvA}}
    local secondEdges = {{second.uvA, second.uvB}, {second.uvB, second.uvC}, {second.uvC, second.uvA}}
    for _, firstEdge in ipairs(firstEdges) do
        for _, secondEdge in ipairs(secondEdges) do
            if uvProperSegmentsIntersect(firstEdge[1], firstEdge[2], secondEdge[1], secondEdge[2]) then
                return true
            end
        end
    end
    local centerFirst = {
        u = (first.uvA.u + first.uvB.u + first.uvC.u) / 3,
        v = (first.uvA.v + first.uvB.v + first.uvC.v) / 3,
    }
    local centerSecond = {
        u = (second.uvA.u + second.uvB.u + second.uvC.u) / 3,
        v = (second.uvA.v + second.uvB.v + second.uvC.v) / 3,
    }
    return uvPointInsideStrict(centerFirst, second) or uvPointInsideStrict(centerSecond, first)
end

function M.countUvOverlaps(triangles)
    assert(type(triangles) == 'table', 'triangles must be a table')
    local gridSize, pairLimit = 24, 10000
    local cells, tested = {}, {}
    local function cellKey(x, y) return x .. ':' .. y end
    local function cellCoordinate(value)
        return math.max(0, math.min(gridSize - 1, math.floor(value * gridSize)))
    end
    local function addTriangle(index, triangle)
        local points = {triangle.uvA, triangle.uvB, triangle.uvC}
        if not (points[1] and points[2] and points[3]) then return end
        local minU, maxU = points[1].u, points[1].u
        local minV, maxV = points[1].v, points[1].v
        for pointIndex = 2, #points do
            minU, maxU = math.min(minU, points[pointIndex].u), math.max(maxU, points[pointIndex].u)
            minV, maxV = math.min(minV, points[pointIndex].v), math.max(maxV, points[pointIndex].v)
        end
        for y = cellCoordinate(minV), cellCoordinate(maxV) do
            for x = cellCoordinate(minU), cellCoordinate(maxU) do
                local key = cellKey(x, y)
                if not cells[key] then cells[key] = {} end
                cells[key][#cells[key] + 1] = index
            end
        end
    end
    for index, triangle in ipairs(triangles) do addTriangle(index, triangle) end

    local overlapCount, testedPairs = 0, 0
    for _, cell in pairs(cells) do
        for firstIndex = 1, #cell - 1 do
            for secondIndex = firstIndex + 1, #cell do
                local left, right = cell[firstIndex], cell[secondIndex]
                if left > right then left, right = right, left end
                local pairKey = left .. ':' .. right
                if not tested[pairKey] then
                    tested[pairKey] = true
                    testedPairs = testedPairs + 1
                    if uvTrianglesOverlap(triangles[left], triangles[right]) then
                        overlapCount = overlapCount + 1
                    end
                    if testedPairs >= pairLimit then
                        return {count = overlapCount, truncated = true}
                    end
                end
            end
        end
    end
    return {count = overlapCount, truncated = false}
end

return M
