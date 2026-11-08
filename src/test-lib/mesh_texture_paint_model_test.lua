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

package.path = 'editor/?.lua;' .. package.path
local Model = require 'mesh_texture_paint_model'

local function mock(vertices, indices)
    return {
        getTotalVertex = function() return #vertices end,
        getVertex = function(_, _, _, first, count)
            assert(first == 1)
            local result = {}
            for i = first, first + count - 1 do result[#result + 1] = vertices[i] end
            return result
        end,
        getIndex = function() return indices end,
        getTexture = function() return 'face-base.png' end,
    }
end

local vertices = {
    {x = -1, y = -1, z = 0, u = 0, v = 1},
    {x = 1, y = -1, z = 0, u = 1, v = 1},
    {x = 0, y = 1, z = 0, u = .5, v = 0},
    {x = -1, y = -1, z = 1, u = 0, v = 1},
    {x = 1, y = -1, z = 1, u = 1, v = 1},
    {x = 0, y = 1, z = 1, u = .5, v = 0},
}

local indexed = assert(Model.snapshotSubset(mock(vertices, {1, 2, 3, 4, 6, 5}), 2, 3))
assert(indexed.frame == 2 and indexed.subset == 3)
assert(#indexed.vertices == 6 and #indexed.indices == 6 and #indexed.triangles == 2)
assert(indexed.triangles[2].c.z == 0 + 1)
assert(indexed.texture == 'face-base.png')

local unindexed = assert(Model.snapshotSubset(mock(vertices, nil)))
assert(#unindexed.triangles == 2)

local invalid = {
    getTotalVertex = function() return 3 end,
    getVertex = function() return {
        {x = 0, y = 0, z = 0, u = 0, v = 0},
        {x = 1, y = 0, z = 0, u = 1, v = 0},
        {x = 0, y = 1, z = 0, u = 0 / 0, v = 1},
    } end,
}
local failed, errorMessage = Model.snapshotSubset(invalid)
assert(not failed and errorMessage:find('invalid position or UV', 1, true))

local boundaries = Model.buildUvBoundaryEdges({
    {uvA = {u=0, v=0}, uvB = {u=1, v=0}, uvC = {u=1, v=1}},
    {uvA = {u=0, v=0}, uvB = {u=1, v=1}, uvC = {u=0, v=1}},
})
assert(#boundaries == 4, 'shared UV edges must not be highlighted as island boundaries')

local islandTriangles = {
    {subset=1, a={x=0,y=0,z=0,u=0,v=0}, b={x=1,y=0,z=0,u=1,v=0},
        c={x=1,y=1,z=0,u=1,v=1}, uvA={u=0,v=0}, uvB={u=1,v=0}, uvC={u=1,v=1}},
    {subset=1, a={x=0,y=0,z=0,u=0,v=0}, b={x=1,y=1,z=0,u=1,v=1},
        c={x=0,y=1,z=0,u=0,v=1}, uvA={u=0,v=0}, uvB={u=1,v=1}, uvC={u=0,v=1}},
    {subset=1, a={x=2,y=0,z=0,u=0,v=0}, b={x=3,y=0,z=0,u=1,v=0},
        c={x=2,y=1,z=0,u=0,v=1}, uvA={u=0,v=0}, uvB={u=1,v=0}, uvC={u=0,v=1}},
}
local islands = Model.buildUvIslands(islandTriangles)
assert(islands.count == 2 and islands.islandByTriangle[1] == islands.islandByTriangle[2] and
    islands.islandByTriangle[1] ~= islands.islandByTriangle[3],
    'UV island connectivity must follow shared position and UV vertices')

local noOverlap = Model.countUvOverlaps({
    {uvA = {u=0, v=0}, uvB = {u=1, v=0}, uvC = {u=1, v=1}},
    {uvA = {u=0, v=1}, uvB = {u=0, v=0}, uvC = {u=-1, v=0}},
})
assert(noOverlap.count == 0, 'triangles that only touch at an edge must not overlap')
local overlap = Model.countUvOverlaps({
    {uvA = {u=0, v=0}, uvB = {u=1, v=0}, uvC = {u=0, v=1}},
    {uvA = {u=.2, v=.2}, uvB = {u=.6, v=.2}, uvC = {u=.2, v=.6}},
})
assert(overlap.count == 1, 'nested UV triangles must be reported as overlapping')

print('MESH TEXTURE PAINT MODEL TESTS PASS')
