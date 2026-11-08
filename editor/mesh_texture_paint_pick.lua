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

local function normalizedVector(x, y, z)
    local length = math.sqrt(x * x + y * y + z * z)
    if length <= EPSILON then return nil end
    return {x = x / length, y = y / length, z = z / length}
end

--- Build an orthonormal tangent frame from a triangle's position and UV edges.
--- The frame is used by Surface 3D to give square dabs a stable local meaning.
function M.surfaceBrushBasis(triangle)
    if type(triangle) ~= 'table' or not validPoint(triangle.a) or
            not validPoint(triangle.b) or not validPoint(triangle.c) or
            type(triangle.uvA) ~= 'table' or type(triangle.uvB) ~= 'table' or
            type(triangle.uvC) ~= 'table' then
        return nil
    end
    local edge1x, edge1y, edge1z = triangle.b.x - triangle.a.x,
        triangle.b.y - triangle.a.y, triangle.b.z - triangle.a.z
    local edge2x, edge2y, edge2z = triangle.c.x - triangle.a.x,
        triangle.c.y - triangle.a.y, triangle.c.z - triangle.a.z
    local du1, dv1 = triangle.uvB.u - triangle.uvA.u, triangle.uvB.v - triangle.uvA.v
    local du2, dv2 = triangle.uvC.u - triangle.uvA.u, triangle.uvC.v - triangle.uvA.v
    local determinant = du1 * dv2 - du2 * dv1
    if math.abs(determinant) <= EPSILON then return nil end
    local tangent = normalizedVector(
        (edge1x * dv2 - edge2x * dv1) / determinant,
        (edge1y * dv2 - edge2y * dv1) / determinant,
        (edge1z * dv2 - edge2z * dv1) / determinant)
    local normal = normalizedNormal(edge1x, edge1y, edge1z, edge2x, edge2y, edge2z)
    if not tangent or not normal then return nil end
    local bitangentX, bitangentY, bitangentZ = cross(normal.x, normal.y, normal.z,
        tangent.x, tangent.y, tangent.z)
    local bitangent = normalizedVector(bitangentX, bitangentY, bitangentZ)
    if not bitangent then return nil end
    local rawBitangentX = (-edge1x * du2 + edge2x * du1) / determinant
    local rawBitangentY = (-edge1y * du2 + edge2y * du1) / determinant
    local rawBitangentZ = (-edge1z * du2 + edge2z * du1) / determinant
    if dot(bitangent.x, bitangent.y, bitangent.z,
            rawBitangentX, rawBitangentY, rawBitangentZ) < 0 then
        bitangent.x, bitangent.y, bitangent.z = -bitangent.x, -bitangent.y, -bitangent.z
    end
    return {tangent = tangent, bitangent = bitangent, normal = normal}
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

function M.closestPointOnTriangle(point, a, b, c)
    if not (validPoint(point) and validPoint(a) and validPoint(b) and validPoint(c)) then return nil end
    local abx, aby, abz = b.x - a.x, b.y - a.y, b.z - a.z
    local acx, acy, acz = c.x - a.x, c.y - a.y, c.z - a.z
    local apx, apy, apz = point.x - a.x, point.y - a.y, point.z - a.z
    local d1 = dot(abx, aby, abz, apx, apy, apz)
    local d2 = dot(acx, acy, acz, apx, apy, apz)
    if d1 <= 0 and d2 <= 0 then return {x=a.x,y=a.y,z=a.z}, {a=1,b=0,c=0} end

    local bpx, bpy, bpz = point.x - b.x, point.y - b.y, point.z - b.z
    local d3 = dot(abx, aby, abz, bpx, bpy, bpz)
    local d4 = dot(acx, acy, acz, bpx, bpy, bpz)
    if d3 >= 0 and d4 <= d3 then return {x=b.x,y=b.y,z=b.z}, {a=0,b=1,c=0} end

    local vc = d1 * d4 - d3 * d2
    if vc <= 0 and d1 >= 0 and d3 <= 0 then
        local value = d1 / (d1 - d3)
        return {x=a.x + value * abx, y=a.y + value * aby, z=a.z + value * abz},
            {a=1-value,b=value,c=0}
    end

    local cpx, cpy, cpz = point.x - c.x, point.y - c.y, point.z - c.z
    local d5 = dot(abx, aby, abz, cpx, cpy, cpz)
    local d6 = dot(acx, acy, acz, cpx, cpy, cpz)
    if d6 >= 0 and d5 <= d6 then return {x=c.x,y=c.y,z=c.z}, {a=0,b=0,c=1} end

    local vb = d5 * d2 - d1 * d6
    if vb <= 0 and d2 >= 0 and d6 <= 0 then
        local value = d2 / (d2 - d6)
        return {x=a.x + value * acx, y=a.y + value * acy, z=a.z + value * acz},
            {a=1-value,b=0,c=value}
    end

    local va = d3 * d6 - d5 * d4
    if va <= 0 and (d4 - d3) >= 0 and (d5 - d6) >= 0 then
        local value = (d4 - d3) / ((d4 - d3) + (d5 - d6))
        return {x=b.x + value * (c.x - b.x), y=b.y + value * (c.y - b.y),
            z=b.z + value * (c.z - b.z)}, {a=0,b=1-value,c=value}
    end

    local denominator = 1 / (va + vb + vc)
    local valueB, valueC = vb * denominator, vc * denominator
    return {
        x = a.x + abx * valueB + acx * valueC,
        y = a.y + aby * valueB + acy * valueC,
        z = a.z + abz * valueB + acz * valueC,
    }, {a=1-valueB-valueC,b=valueB,c=valueC}
end

local function barycentric2D(x, y, a, b, c)
    if type(a) ~= 'table' or type(b) ~= 'table' or type(c) ~= 'table' then return nil end
    local abx, aby = b.u - a.u, b.v - a.v
    local acx, acy = c.u - a.u, c.v - a.v
    local apx, apy = x - a.u, y - a.v
    local denominator = abx * acy - aby * acx
    if math.abs(denominator) <= EPSILON then return nil end
    local weightB = (apx * acy - apy * acx) / denominator
    local weightC = (abx * apy - aby * apx) / denominator
    return {a = 1 - weightB - weightC, b = weightB, c = weightC}
end

local function interpolatePoint(weights, a, b, c)
    return {
        x = weights.a * a.x + weights.b * b.x + weights.c * c.x,
        y = weights.a * a.y + weights.b * b.y + weights.c * c.y,
        z = weights.a * a.z + weights.b * b.z + weights.c * c.z,
    }
end

--- Rasterize a continuous surface-distance mask into texture-space samples.
--- Each texture texel is mapped back through the UV triangle to 3D, so the
--- result does not depend on one artificial dab per mesh triangle.
function M.surfaceSamplesAtPoint(center, radius, triangles, width, height, options)
    if not validPoint(center) or type(triangles) ~= 'table' then return {} end
    width, height = math.floor(tonumber(width) or 0), math.floor(tonumber(height) or 0)
    if width < 1 or height < 1 then return {} end
    radius = math.max(0, tonumber(radius) or 0)
    options = options or {}
    local hardness = math.max(0, math.min(1, tonumber(options.hardness) or 1))
    local shape = options.shape == 'square' and 'square' or 'circle'
    local tangent = options.tangent
    local bitangent = options.bitangent
    local maxSamples = math.max(1024, math.floor(tonumber(options.maxSamples) or 65536))
    local source = options.candidates or triangles
    local scaleX, scaleY = width - 1, height - 1
    local records, totalArea = {}, 0
    for _, entry in ipairs(source) do
        local triangle = entry.triangle or entry
        if type(triangle) == 'table' and validPoint(triangle.a) and validPoint(triangle.b) and
                validPoint(triangle.c) and triangle.uvA and triangle.uvB and triangle.uvC then
            local minU = math.min(triangle.uvA.u, triangle.uvB.u, triangle.uvC.u)
            local maxU = math.max(triangle.uvA.u, triangle.uvB.u, triangle.uvC.u)
            local minV = math.min(triangle.uvA.v, triangle.uvB.v, triangle.uvC.v)
            local maxV = math.max(triangle.uvA.v, triangle.uvB.v, triangle.uvC.v)
            local left = math.max(0, math.floor(minU * scaleX))
            local right = math.min(scaleX, math.ceil(maxU * scaleX))
            local top = math.max(0, math.floor(minV * scaleY))
            local bottom = math.min(scaleY, math.ceil(maxV * scaleY))
            if left <= right and top <= bottom then
                local area = (right - left + 1) * (bottom - top + 1)
                records[#records + 1] = {triangle = triangle, left = left, right = right,
                    top = top, bottom = bottom}
                totalArea = totalArea + area
            end
        end
    end
    if #records == 0 then return {} end
    local step = math.max(1, math.ceil(math.sqrt(totalArea / maxSamples)))
    local radiusSquared = radius * radius
    local innerRadius = radius * hardness
    local samples = {}
    for _, record in ipairs(records) do
        local triangle = record.triangle
        for y = record.top, record.bottom, step do
            for x = record.left, record.right, step do
                local sampleX = x + (step - 1) * .5
                local sampleY = y + (step - 1) * .5
                local weights = barycentric2D(sampleX / scaleX, sampleY / scaleY,
                    triangle.uvA, triangle.uvB, triangle.uvC)
                if weights and weights.a >= -EPSILON and weights.b >= -EPSILON and weights.c >= -EPSILON then
                    local position = interpolatePoint(weights, triangle.a, triangle.b, triangle.c)
                    local dx = position.x - center.x
                    local dy = position.y - center.y
                    local dz = position.z - center.z
                    local distanceSquared = dx * dx + dy * dy + dz * dz
                    local distance
                    if shape == 'square' and validPoint(tangent) and validPoint(bitangent) then
                        local localX = dot(dx, dy, dz, tangent.x, tangent.y, tangent.z)
                        local localY = dot(dx, dy, dz, bitangent.x, bitangent.y, bitangent.z)
                        distance = math.max(math.abs(localX), math.abs(localY))
                    else
                        distance = math.sqrt(distanceSquared)
                    end
                    if distance <= radius then
                        local amount = distance <= innerRadius and 1 or
                            (innerRadius >= radius and 0 or
                                math.max(0, 1 - (distance - innerRadius) / (radius - innerRadius)))
                        if amount > 0 then
                            samples[#samples + 1] = {
                                x = sampleX,
                                y = sampleY,
                                amount = amount,
                                radius = step > 1 and step * .5 or 0,
                            }
                        end
                    end
                end
            end
        end
    end
    return samples
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

local function sphereIntersectsBounds(center, radius, node)
    local distanceSquared = 0
    for _, axis in ipairs(BVH_AXES) do
        local value = center[axis]
        local low, high = node['min' .. axis:upper()], node['max' .. axis:upper()]
        if value < low then distanceSquared = distanceSquared + (low - value) ^ 2 end
        if value > high then distanceSquared = distanceSquared + (value - high) ^ 2 end
    end
    return distanceSquared <= radius * radius
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

function M.queryTrianglesSphere(center, radius, acceleration, triangles)
    radius = math.max(0, tonumber(radius) or 0)
    local radiusSquared = radius * radius
    local result = {}
    local function addCandidate(triangle, index)
        local closest = M.closestPointOnTriangle(center, triangle.a, triangle.b, triangle.c)
        if not closest then return end
        local dx, dy, dz = closest.x - center.x, closest.y - center.y, closest.z - center.z
        if dx * dx + dy * dy + dz * dz <= radiusSquared then
            result[#result + 1] = {triangle=triangle, index=index}
        end
    end
    if type(acceleration) ~= 'table' or not acceleration.root then
        for index, triangle in ipairs(triangles or {}) do
            addCandidate(triangle, index)
        end
        return result
    end
    local function visit(node)
        if not sphereIntersectsBounds(center, radius, node) then return end
        if node.first then
            for index = node.first, node.last do
                local entry = acceleration.entries[index]
                addCandidate(entry.triangle, entry.index)
            end
            return
        end
        visit(node.left)
        visit(node.right)
    end
    visit(acceleration.root)
    return result
end

return M
