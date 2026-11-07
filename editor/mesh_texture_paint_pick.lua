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
local EPSILON = 1e-8
local BVH_AXES = {'x', 'y', 'z'}

local function dot(ax, ay, az, bx, by, bz)
    return ax * bx + ay * by + az * bz
end

local function cross(ax, ay, az, bx, by, bz)
    return ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx
end

local function finite(value)
    return type(value) == 'number' and value == value and math.abs(value) < math.huge
end

local function validPoint(point)
    return type(point) == 'table' and finite(point.x) and finite(point.y) and finite(point.z)
end

local function normalizedNormal(ax, ay, az, bx, by, bz)
    local nx, ny, nz = cross(ax, ay, az, bx, by, bz)
    local length = math.sqrt(nx * nx + ny * ny + nz * nz)
    if length <= EPSILON then return nil end
    return {x = nx / length, y = ny / length, z = nz / length}
end

local function positionAt(origin, direction, distance)
    return {
        x = origin.x + direction.x * distance,
        y = origin.y + direction.y * distance,
        z = origin.z + direction.z * distance,
    }
end

--- Intersect a ray with one triangle using Moller-Trumbore.
--- Returns a hit with barycentric weights `a`, `b`, and `c`, or nil.
function M.intersectRayTriangle(origin, direction, a, b, c, cullBackface)
    if not validPoint(origin) or not validPoint(direction) or
            not validPoint(a) or not validPoint(b) or not validPoint(c) then
        return nil
    end

    local e1x, e1y, e1z = b.x - a.x, b.y - a.y, b.z - a.z
    local e2x, e2y, e2z = c.x - a.x, c.y - a.y, c.z - a.z
    local px, py, pz = cross(direction.x, direction.y, direction.z, e2x, e2y, e2z)
    local determinant = dot(e1x, e1y, e1z, px, py, pz)
    if cullBackface then
        if determinant <= EPSILON then return nil end
    elseif math.abs(determinant) <= EPSILON then
        return nil
    end

    local inverse = 1 / determinant
    local tx, ty, tz = origin.x - a.x, origin.y - a.y, origin.z - a.z
    local baryB = dot(tx, ty, tz, px, py, pz) * inverse
    if baryB < 0 or baryB > 1 then return nil end

    local qx, qy, qz = cross(tx, ty, tz, e1x, e1y, e1z)
    local baryC = dot(direction.x, direction.y, direction.z, qx, qy, qz) * inverse
    if baryC < 0 or baryB + baryC > 1 then return nil end

    local distance = dot(e2x, e2y, e2z, qx, qy, qz) * inverse
    if distance < 0 then return nil end

    local baryA = 1 - baryB - baryC
    return {
        distance = distance,
        barycentric = {a = baryA, b = baryB, c = baryC},
        position = positionAt(origin, direction, distance),
        normal = normalizedNormal(e1x, e1y, e1z, e2x, e2y, e2z),
    }
end

--- Interpolate a UV from a hit returned by intersectRayTriangle.
function M.interpolateUV(hit, uvA, uvB, uvC)
    if type(hit) ~= 'table' or type(hit.barycentric) ~= 'table' then return nil end
    if type(uvA) ~= 'table' or type(uvB) ~= 'table' or type(uvC) ~= 'table' then return nil end
    local weights = hit.barycentric
    if not finite(uvA.u) or not finite(uvA.v) or
            not finite(uvB.u) or not finite(uvB.v) or
            not finite(uvC.u) or not finite(uvC.v) then
        return nil
    end
    return {
        u = weights.a * uvA.u + weights.b * uvB.u + weights.c * uvC.u,
        v = weights.a * uvA.v + weights.b * uvB.v + weights.c * uvC.v,
    }
end

--- Pick the nearest triangle from an array of {a,b,c,uvA,uvB,uvC} records.
function M.pickTriangles(origin, direction, triangles, options)
    if type(triangles) ~= 'table' then return nil end
    options = options or {}
    local nearest = nil
    for index, triangle in ipairs(triangles) do
        if type(triangle) == 'table' then
            local hit = M.intersectRayTriangle(origin, direction,
                triangle.a, triangle.b, triangle.c, options.cullBackface == true)
            if hit and (not nearest or hit.distance < nearest.distance) then
                hit.triangleIndex = index
                hit.triangle = triangle
                if triangle.uvA and triangle.uvB and triangle.uvC then
                    hit.uv = M.interpolateUV(hit, triangle.uvA, triangle.uvB, triangle.uvC)
                end
                nearest = hit
            end
        end
    end
    return nearest
end

local function triangleEntry(triangle, index)
    local a, b, c = triangle.a, triangle.b, triangle.c
    local minX = math.min(a.x, b.x, c.x)
    local maxX = math.max(a.x, b.x, c.x)
    local minY = math.min(a.y, b.y, c.y)
    local maxY = math.max(a.y, b.y, c.y)
    local minZ = math.min(a.z, b.z, c.z)
    local maxZ = math.max(a.z, b.z, c.z)
    return {
        triangle = triangle,
        index = index,
        minX = minX, maxX = maxX, minY = minY, maxY = maxY, minZ = minZ, maxZ = maxZ,
        cx = (minX + maxX) * .5, cy = (minY + maxY) * .5, cz = (minZ + maxZ) * .5,
    }
end

local function sortRange(entries, first, last, axis)
    local left, right = first, last
    local pivot = entries[math.floor((first + last) * .5)][axis]
    while left <= right do
        while entries[left][axis] < pivot do left = left + 1 end
        while entries[right][axis] > pivot do right = right - 1 end
        if left <= right then
            entries[left], entries[right] = entries[right], entries[left]
            left, right = left + 1, right - 1
        end
    end
    if first < right then sortRange(entries, first, right, axis) end
    if left < last then sortRange(entries, left, last, axis) end
end

local function buildBvhNode(entries, first, last, leafSize)
    local entry = entries[first]
    local minX, maxX = entry.minX, entry.maxX
    local minY, maxY = entry.minY, entry.maxY
    local minZ, maxZ = entry.minZ, entry.maxZ
    for index = first + 1, last do
        entry = entries[index]
        minX, maxX = math.min(minX, entry.minX), math.max(maxX, entry.maxX)
        minY, maxY = math.min(minY, entry.minY), math.max(maxY, entry.maxY)
        minZ, maxZ = math.min(minZ, entry.minZ), math.max(maxZ, entry.maxZ)
    end
    local node = {minX=minX, maxX=maxX, minY=minY, maxY=maxY, minZ=minZ, maxZ=maxZ}
    if last - first + 1 <= leafSize then
        node.first, node.last = first, last
        return node
    end
    local extentX, extentY, extentZ = maxX - minX, maxY - minY, maxZ - minZ
    local axis = extentX >= extentY and extentX >= extentZ and 'cx' or
        extentY >= extentZ and 'cy' or 'cz'
    sortRange(entries, first, last, axis)
    local middle = math.floor((first + last) * .5)
    node.left = buildBvhNode(entries, first, middle, leafSize)
    node.right = buildBvhNode(entries, middle + 1, last, leafSize)
    return node
end

function M.buildBVH(triangles, leafSize)
    if type(triangles) ~= 'table' or #triangles == 0 then return nil end
    leafSize = math.max(2, math.floor(tonumber(leafSize) or 8))
    local entries = {}
    for index, triangle in ipairs(triangles) do
        if type(triangle) == 'table' and triangle.a and triangle.b and triangle.c then
            entries[#entries + 1] = triangleEntry(triangle, index)
        end
    end
    if #entries == 0 then return nil end
    return {entries=entries, root=buildBvhNode(entries, 1, #entries, leafSize)}
end

local function rayIntersectsBounds(origin, direction, node, maximumDistance)
    local minimum, maximum = 0, maximumDistance or math.huge
    for _, axis in ipairs(BVH_AXES) do
        local originValue, directionValue = origin[axis], direction[axis]
        local low, high = node['min' .. axis:upper()], node['max' .. axis:upper()]
        if math.abs(directionValue) < 1e-12 then
            if originValue < low or originValue > high then return false end
        else
            local inverse = 1 / directionValue
            local nearValue, farValue = (low - originValue) * inverse, (high - originValue) * inverse
            if nearValue > farValue then nearValue, farValue = farValue, nearValue end
            minimum, maximum = math.max(minimum, nearValue), math.min(maximum, farValue)
            if minimum > maximum then return false end
        end
    end
    return true
end

--- Pick using a lazily-built triangle BVH. The return shape matches pickTriangles().
function M.pickTrianglesBVH(origin, direction, acceleration, options)
    if type(acceleration) ~= 'table' or not acceleration.root then return nil end
    options = options or {}
    local nearest, maximumDistance = nil, math.huge
    local function visit(node)
        if not rayIntersectsBounds(origin, direction, node, maximumDistance) then return end
        if node.first then
            for index = node.first, node.last do
                local entry = acceleration.entries[index]
                local triangle = entry.triangle
                local hit = M.intersectRayTriangle(origin, direction, triangle.a, triangle.b, triangle.c,
                    options.cullBackface == true)
                if hit and hit.distance < maximumDistance then
                    hit.triangleIndex = entry.index
                    hit.triangle = triangle
                    if triangle.uvA and triangle.uvB and triangle.uvC then
                        hit.uv = M.interpolateUV(hit, triangle.uvA, triangle.uvB, triangle.uvC)
                    end
                    nearest, maximumDistance = hit, hit.distance
                end
            end
            return
        end
        visit(node.left)
        visit(node.right)
    end
    visit(acceleration.root)
    return nearest
end

function M.pickTrianglesAll(origin, direction, triangles, options)
    if type(triangles) ~= 'table' then return {} end
    options = options or {}
    local hits = {}
    for index, triangle in ipairs(triangles) do
        if type(triangle) == 'table' then
            local hit = M.intersectRayTriangle(origin, direction,
                triangle.a, triangle.b, triangle.c, options.cullBackface == true)
            if hit then
                hit.triangleIndex = index
                hit.triangle = triangle
                if triangle.uvA and triangle.uvB and triangle.uvC then
                    hit.uv = M.interpolateUV(hit, triangle.uvA, triangle.uvB, triangle.uvC)
                end
                hits[#hits + 1] = hit
            end
        end
    end
    table.sort(hits, function(left, right) return left.distance < right.distance end)
    return hits
end

function M.pickTrianglesBVHAll(origin, direction, acceleration, options)
    if type(acceleration) ~= 'table' or not acceleration.root then return {} end
    options = options or {}
    local hits = {}
    local function visit(node)
        if not rayIntersectsBounds(origin, direction, node) then return end
        if node.first then
            for index = node.first, node.last do
                local entry = acceleration.entries[index]
                local triangle = entry.triangle
                local hit = M.intersectRayTriangle(origin, direction, triangle.a, triangle.b, triangle.c,
                    options.cullBackface == true)
                if hit then
                    hit.triangleIndex = entry.index
                    hit.triangle = triangle
                    if triangle.uvA and triangle.uvB and triangle.uvC then
                        hit.uv = M.interpolateUV(hit, triangle.uvA, triangle.uvB, triangle.uvC)
                    end
                    hits[#hits + 1] = hit
                end
            end
            return
        end
        visit(node.left)
        visit(node.right)
    end
    visit(acceleration.root)
    table.sort(hits, function(left, right) return left.distance < right.distance end)
    return hits
end

return M
