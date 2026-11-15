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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|
]]--

local M = {}
local EPSILON = 0.00001

local function pointValues(p)
    return p.x or 0, p.y or 0, p.z or 0
end

local function cross(ax, ay, az, bx, by, bz)
    return ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx
end

local function dot(ax, ay, az, bx, by, bz)
    return ax * bx + ay * by + az * bz
end

local function projectedCoordinates(nx, ny, nz, x, y, z)
    local anx, any, anz = math.abs(nx), math.abs(ny), math.abs(nz)
    if anx >= any and anx >= anz then
        return y, z
    end
    if any >= anz then
        return x, z
    end
    return x, y
end

local function polygonForPlane(points, plane, indices)
    local projected = {}
    for _, index in ipairs(indices) do
        local x, y, z = pointValues(points[index])
        local u, v = projectedCoordinates(plane.nx, plane.ny, plane.nz, x, y, z)
        projected[#projected + 1] = {u=u, v=v, index=index}
    end
    table.sort(projected, function(a, b)
        if math.abs(a.u - b.u) > EPSILON then
            return a.u < b.u
        end
        if math.abs(a.v - b.v) > EPSILON then
            return a.v < b.v
        end
        return a.index < b.index
    end)

    local function cross2(a, b, c)
        return (b.u - a.u) * (c.v - a.v) - (b.v - a.v) * (c.u - a.u)
    end

    local lower = {}
    for _, item in ipairs(projected) do
        while #lower >= 2 and cross2(lower[#lower - 1], lower[#lower], item) <= EPSILON do
            table.remove(lower)
        end
        lower[#lower + 1] = item
    end
    local upper = {}
    for index = #projected, 1, -1 do
        local item = projected[index]
        while #upper >= 2 and cross2(upper[#upper - 1], upper[#upper], item) <= EPSILON do
            table.remove(upper)
        end
        upper[#upper + 1] = item
    end

    local polygon = {}
    for index = 1, #lower - 1 do polygon[#polygon + 1] = lower[index].index end
    for index = 1, #upper - 1 do polygon[#polygon + 1] = upper[index].index end
    if #polygon < 3 then
        return nil
    end

    local p1x, p1y, p1z = pointValues(points[polygon[1]])
    local p2x, p2y, p2z = pointValues(points[polygon[2]])
    local p3x, p3y, p3z = pointValues(points[polygon[3]])
    local ux, uy, uz = p2x - p1x, p2y - p1y, p2z - p1z
    local vx, vy, vz = p3x - p1x, p3y - p1y, p3z - p1z
    local fnx, fny, fnz = cross(ux, uy, uz, vx, vy, vz)
    if dot(fnx, fny, fnz, plane.nx, plane.ny, plane.nz) < 0 then
        local reversed = {}
        for index = #polygon, 1, -1 do reversed[#reversed + 1] = polygon[index] end
        polygon = reversed
    end
    return polygon
end

-- Computes the actual convex hull topology for the small point sets used by the editors.
-- The native runtime accepts a point cloud and computes its own Bullet hull; the preview must
-- therefore use the same hull instead of connecting points in their storage order.
function M.hull(points)
    local pointCount = #points
    if pointCount < 3 then
        return {faces={}, edges={}}
    end

    local centerX, centerY, centerZ = 0, 0, 0
    for _, p in ipairs(points) do
        local x, y, z = pointValues(p)
        centerX, centerY, centerZ = centerX + x, centerY + y, centerZ + z
    end
    centerX, centerY, centerZ = centerX / pointCount, centerY / pointCount, centerZ / pointCount

    local planes = {}
    for i = 1, pointCount - 2 do
        local ax, ay, az = pointValues(points[i])
        for j = i + 1, pointCount - 1 do
            local bx, by, bz = pointValues(points[j])
            local ux, uy, uz = bx - ax, by - ay, bz - az
            for k = j + 1, pointCount do
                local cx, cy, cz = pointValues(points[k])
                local vx, vy, vz = cx - ax, cy - ay, cz - az
                local nx, ny, nz = cross(ux, uy, uz, vx, vy, vz)
                local length = math.sqrt(nx * nx + ny * ny + nz * nz)
                if length > EPSILON then
                    local hasPositive, hasNegative = false, false
                    for other = 1, pointCount do
                        if other ~= i and other ~= j and other ~= k then
                            local px, py, pz = pointValues(points[other])
                            local side = dot(nx, ny, nz, px - ax, py - ay, pz - az)
                            if side > EPSILON then hasPositive = true end
                            if side < -EPSILON then hasNegative = true end
                            if hasPositive and hasNegative then break end
                        end
                    end
                    if not (hasPositive and hasNegative) then
                        local centerSide = dot(nx, ny, nz, centerX - ax, centerY - ay, centerZ - az)
                        if centerSide > EPSILON then
                            nx, ny, nz = -nx, -ny, -nz
                        elseif math.abs(centerSide) <= EPSILON then
                            -- When all points are coplanar, the centroid cannot orient the plane.
                            -- Canonicalize the normal so the same polygon is not emitted twice with
                            -- opposite windings.
                            if nx < -EPSILON
                                    or (math.abs(nx) <= EPSILON and ny < -EPSILON)
                                    or (math.abs(nx) <= EPSILON and math.abs(ny) <= EPSILON and nz < 0) then
                                nx, ny, nz = -nx, -ny, -nz
                            end
                        end
                        nx, ny, nz = nx / length, ny / length, nz / length
                        local planeD = -(nx * ax + ny * ay + nz * az)
                        local duplicate = false
                        for _, plane in ipairs(planes) do
                            if math.abs(plane.nx - nx) < EPSILON
                                    and math.abs(plane.ny - ny) < EPSILON
                                    and math.abs(plane.nz - nz) < EPSILON
                                    and math.abs(plane.d - planeD) < EPSILON * math.max(1, math.abs(planeD)) then
                                duplicate = true
                                break
                            end
                        end
                        if not duplicate then
                            planes[#planes + 1] = {nx=nx, ny=ny, nz=nz, d=planeD}
                        end
                    end
                end
            end
        end
    end

    local faces, edges, edgeSet = {}, {}, {}
    for _, plane in ipairs(planes) do
        local coplanar = {}
        for index = 1, pointCount do
            local x, y, z = pointValues(points[index])
            if math.abs(plane.nx * x + plane.ny * y + plane.nz * z + plane.d) < EPSILON * 4 then
                coplanar[#coplanar + 1] = index
            end
        end
        local polygon = polygonForPlane(points, plane, coplanar)
        if polygon then
            for index = 2, #polygon - 1 do
                faces[#faces + 1] = {polygon[1], polygon[index], polygon[index + 1]}
            end
            for index = 1, #polygon do
                local nextIndex = (index % #polygon) + 1
                local a, b = polygon[index], polygon[nextIndex]
                local key = a < b and (a .. ':' .. b) or (b .. ':' .. a)
                if not edgeSet[key] then
                    edgeSet[key] = true
                    edges[#edges + 1] = {a, b}
                end
            end
        end
    end
    return {faces=faces, edges=edges}
end

function M.edgePoints(points, hull)
    hull = hull or M.hull(points)
    local result = {}
    for _, edge in ipairs(hull.edges) do
        local a, b = points[edge[1]], points[edge[2]]
        local ax, ay, az = pointValues(a)
        local bx, by, bz = pointValues(b)
        result[#result + 1] = ax; result[#result + 1] = ay; result[#result + 1] = az
        result[#result + 1] = bx; result[#result + 1] = by; result[#result + 1] = bz
    end
    return result, hull
end

return M
