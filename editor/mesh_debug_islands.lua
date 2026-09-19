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

local function sets()
    local parent, rank = {}, {}
    local function root(i)
        if not parent[i] then parent[i], rank[i] = i, 0 end
        local r = i
        while parent[r] ~= r do r = parent[r] end
        while parent[i] ~= i do local nextIndex = parent[i]; parent[i] = r; i = nextIndex end
        return r
    end
    local function join(a, b)
        a, b = root(a), root(b)
        if a == b then return end
        if rank[a] < rank[b] then a, b = b, a end
        parent[b] = a
        if rank[a] == rank[b] then rank[a] = rank[a] + 1 end
    end
    return root, join
end

local function positionKey(x, y, z)
    -- Normalize signed zero; retain double precision instead of rounding into grid cells.
    return string.format('%.17g:%.17g:%.17g', x == 0 and 0 or x, y == 0 and 0 or y, z == 0 and 0 or z)
end

-- mode: indices, vertices (positions), or edges (position-paired edges).
-- Tolerance affects connectivity only: original vertices/UVs/normals/weights are never welded.
function M.build(triangles, vertices, mode, tolerance)
    mode, tolerance = mode or 'edges', tolerance or 0
    assert(mode == 'indices' or mode == 'vertices' or mode == 'edges', 'Invalid island connectivity mode')
    assert(tolerance >= 0 and tolerance < math.huge, 'Invalid island tolerance')
    local vertexRoot, joinVertices = sets()
    if mode ~= 'indices' then
        local seen, exact, cells = {}, {}, {}
        for _, triangle in ipairs(triangles) do
            for _, vi in ipairs(triangle) do
                if not seen[vi] then
                    seen[vi] = true
                    local v = assert(vertices[vi], 'Invalid triangle vertex')
                    local key = positionKey(v.x, v.y, v.z)
                    local duplicate = exact[key]
                    if duplicate then
                        joinVertices(vi, duplicate)
                    else
                        exact[key] = vi
                        if tolerance > 0 then
                            local cx, cy, cz = math.floor(v.x/tolerance), math.floor(v.y/tolerance), math.floor(v.z/tolerance)
                            for x = -1, 1 do
                                for y = -1, 1 do
                                    for z = -1, 1 do
                                        for _, neighbor in ipairs(cells[positionKey(cx+x,cy+y,cz+z)] or {}) do
                                            local other = vertices[neighbor]
                                            local dx, dy, dz = (v.x-other.x)/tolerance, (v.y-other.y)/tolerance, (v.z-other.z)/tolerance
                                            if dx*dx + dy*dy + dz*dz <= 1 then joinVertices(vi, neighbor) end
                                        end
                                    end
                                end
                            end
                            local cell = positionKey(cx,cy,cz)
                            cells[cell] = cells[cell] or {}
                            cells[cell][#cells[cell]+1] = vi
                        end
                    end
                end
            end
        end
    end
    local faceRoot, joinFaces = sets()
    local owners = {}
    for ti, triangle in ipairs(triangles) do
        local ids = {}
        for k, vi in ipairs(triangle) do ids[k] = mode == 'indices' and vi or vertexRoot(vi) end
        for k = 1, 3 do
            local token = ids[k]
            if mode == 'edges' then
                local other = ids[k % 3 + 1]
                local a, b = vertices[triangle[k]], vertices[triangle[k % 3 + 1]]
                if a.x == b.x and a.y == b.y and a.z == b.z then token = nil
                else
                    -- A tolerance cluster can contain both endpoints of a real edge.
                    -- Keep that edge token: dropping it fragments already connected surfaces
                    -- as tolerance grows, instead of only joining nearby components.
                    token = math.min(token,other) .. ':' .. math.max(token,other)
                end
            end
            if token then
                if owners[token] then joinFaces(ti, owners[token]) else owners[token] = ti end
            end
        end
    end
    local groups, islands, first = {}, {}, {}
    for ti, triangle in ipairs(triangles) do
        local root = faceRoot(ti)
        if not groups[root] then
            groups[root] = {}; islands[#islands+1] = groups[root]; first[groups[root]] = ti
        end
        local group = groups[root]; group[#group+1] = triangle
    end
    table.sort(islands, function(a,b)
        if #a == #b then return first[a] < first[b] end
        return #a > #b
    end)
    return islands
end

return M
