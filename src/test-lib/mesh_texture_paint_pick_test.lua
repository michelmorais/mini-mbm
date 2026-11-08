--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
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
local Pick = require 'mesh_texture_paint_pick'

local function near(actual, expected, epsilon)
    assert(math.abs(actual - expected) <= (epsilon or 1e-6),
        string.format('expected %.9f, got %.9f', expected, actual))
end

local origin = {x = 0, y = 0, z = 2}
local direction = {x = 0, y = 0, z = -1}
local a = {x = -1, y = -1, z = 0}
local b = {x = 1, y = -1, z = 0}
local c = {x = 0, y = 1, z = 0}

local hit = assert(Pick.intersectRayTriangle(origin, direction, a, b, c))
near(hit.distance, 2)
near(hit.position.x, 0)
near(hit.position.y, 0)
near(hit.position.z, 0)
near(hit.barycentric.a + hit.barycentric.b + hit.barycentric.c, 1)
near(hit.normal.x, 0)
near(hit.normal.y, 0)
near(hit.normal.z, 1)

local uv = assert(Pick.interpolateUV(hit,
    {u = 0, v = 1}, {u = 1, v = 1}, {u = .5, v = 0}))
near(uv.u, .5)
near(uv.v, .5)

local triangles = {
    {a = a, b = b, c = c,
     uvA = {u = 0, v = 1}, uvB = {u = 1, v = 1}, uvC = {u = .5, v = 0}},
    {a = {x = -2, y = -2, z = -2}, b = {x = 2, y = -2, z = -2},
     c = {x = 0, y = 2, z = -2}},
}
local nearest = assert(Pick.pickTriangles(origin, direction, triangles))
assert(nearest.triangleIndex == 1)
near(nearest.uv.u, .5)
local accelerated = assert(Pick.pickTrianglesBVH(origin, direction, Pick.buildBVH(triangles)))
assert(accelerated.triangleIndex == 1)
near(accelerated.uv.u, .5)
local allHits = Pick.pickTrianglesBVHAll(origin, direction, Pick.buildBVH(triangles))
assert(#allHits == 2 and allHits[1].triangleIndex == 1 and allHits[2].triangleIndex == 2,
    'all-hit BVH picking must return every surface in distance order')
local allLinearHits = Pick.pickTrianglesAll(origin, direction, triangles)
assert(#allLinearHits == 2 and allLinearHits[1].triangleIndex == 1 and allLinearHits[2].triangleIndex == 2,
    'all-hit linear picking must return every surface in distance order')
local closest, closestBarycentric = Pick.closestPointOnTriangle({x=0,y=0,z=1}, a, b, c)
near(closest.x, 0)
near(closest.y, 0)
near(closest.z, 0)
near(closestBarycentric.a + closestBarycentric.b + closestBarycentric.c, 1)
local nearby = Pick.queryTrianglesSphere({x=0,y=0,z=.1}, .2, Pick.buildBVH(triangles))
assert(#nearby == 1 and nearby[1].index == 1, 'surface query must return only nearby triangles')

local surfaceTriangles = {
    {a={x=-1,y=-1,z=0}, b={x=1,y=-1,z=0}, c={x=1,y=1,z=0},
     uvA={u=0,v=0}, uvB={u=1,v=0}, uvC={u=1,v=1}},
    {a={x=-1,y=-1,z=0}, b={x=1,y=1,z=0}, c={x=-1,y=1,z=0},
     uvA={u=0,v=0}, uvB={u=1,v=1}, uvC={u=0,v=1}},
}
local surfaceSamples = Pick.surfaceSamplesAtPoint({x=0,y=0,z=0}, .55,
    surfaceTriangles, 9, 9, {hardness=1, maxSamples=4096})
local surfaceSampleByPixel = {}
for _, sample in ipairs(surfaceSamples) do
    assert(sample.radius == 0, 'continuous surface raster must not create triangle dabs')
    surfaceSampleByPixel[math.floor(sample.x + .5) .. ':' .. math.floor(sample.y + .5)] = true
end
assert(surfaceSampleByPixel['4:4'], 'surface raster must include the hit texel')
assert(surfaceSampleByPixel['3:4'] and surfaceSampleByPixel['5:4'] and
    surfaceSampleByPixel['4:3'] and surfaceSampleByPixel['4:5'],
    'surface raster must produce a continuous neighborhood across a UV diagonal')

local basis = assert(Pick.surfaceBrushBasis(surfaceTriangles[1]))
local squareSurfaceSamples = Pick.surfaceSamplesAtPoint({x=0,y=0,z=0}, .55,
    surfaceTriangles, 9, 9, {hardness=1, shape='square', tangent=basis.tangent,
        bitangent=basis.bitangent, maxSamples=4096})
local squareSampleByPixel = {}
for _, sample in ipairs(squareSurfaceSamples) do
    squareSampleByPixel[math.floor(sample.x + .5) .. ':' .. math.floor(sample.y + .5)] = true
end
assert(squareSampleByPixel['2:2'],
    'square surface raster must include a corner inside the tangent-space square')
assert(not surfaceSampleByPixel['2:2'],
    'circular surface raster must exclude the same diagonal corner')
local function countSamples(samples)
    local unique = {}
    for _, sample in ipairs(samples) do
        unique[math.floor(sample.x + .5) .. ':' .. math.floor(sample.y + .5)] = true
    end
    local count = 0
    for _ in pairs(unique) do count = count + 1 end
    return count
end
assert(countSamples(squareSurfaceSamples) > countSamples(surfaceSamples),
    'square surface raster must cover more texels than the circle')

local backA = {x = -1, y = -1, z = 0}
local backB = {x = 0, y = 1, z = 0}
local backC = {x = 1, y = -1, z = 0}
assert(Pick.intersectRayTriangle(origin, direction, backA, backB, backC))
assert(not Pick.intersectRayTriangle(origin, direction, backA, backB, backC, true),
    'backface culling accepted a reversed triangle')

assert(not Pick.intersectRayTriangle(origin, direction,
    {x = 0, y = 0, z = 0}, {x = 1, y = 0, z = 0}, {x = 2, y = 0, z = 0}),
    'degenerate triangle intersected')
assert(not Pick.intersectRayTriangle(origin, direction,
    {x = 5, y = 5, z = 0}, {x = 6, y = 5, z = 0}, {x = 5, y = 6, z = 0}),
    'missed triangle intersected')

print('MESH TEXTURE PAINT PICK TESTS PASS')
