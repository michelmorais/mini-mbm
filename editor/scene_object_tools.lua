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
| the Software.                                                                                                        |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Ray picking and translation helpers for Scene Editor 3D.

]]--

local M = {}
local function raySphereDistance(ox,oy,oz,dx,dy,dz,cx,cy,cz,radius)
    local lx,ly,lz=cx-ox,cy-oy,cz-oz
    local projected=lx*dx+ly*dy+lz*dz
    if projected<0 then return nil end
    local perpendicular=lx*lx+ly*ly+lz*lz-projected*projected
    local radiusSquared=radius*radius
    if perpendicular>radiusSquared then return nil end
    return projected-math.sqrt(math.max(0,radiusSquared-perpendicular))
end

local function raySegmentDistance(ox,oy,oz,dx,dy,dz,a,b,radius)
    local vx,vy,vz=b.x-a.x,b.y-a.y,b.z-a.z
    local lengthSquared=vx*vx+vy*vy+vz*vz
    if lengthSquared<1e-12 then
        return raySphereDistance(ox,oy,oz,dx,dy,dz,a.x,a.y,a.z,radius)
    end
    local wx,wy,wz=ox-a.x,oy-a.y,oz-a.z
    local uv=dx*vx+dy*vy+dz*vz
    local uw=dx*wx+dy*wy+dz*wz
    local vw=vx*wx+vy*wy+vz*wz
    local denominator=lengthSquared-uv*uv
    local segmentT
    if math.abs(denominator)<1e-12 then segmentT=-vw/lengthSquared
    else segmentT=(vw-uv*uw)/denominator end
    segmentT=math.max(0,math.min(1,segmentT))
    local px,py,pz=a.x+vx*segmentT,a.y+vy*segmentT,a.z+vz*segmentT
    local rayT=(px-ox)*dx+(py-oy)*dy+(pz-oz)*dz
    if rayT<0 then return nil end
    local qx,qy,qz=ox+dx*rayT,oy+dy*rayT,oz+dz*rayT
    local ex,ey,ez=px-qx,py-qy,pz-qz
    if ex*ex+ey*ey+ez*ez>radius*radius then return nil end
    return rayT
end


M.raySphereDistance = raySphereDistance
M.raySegmentDistance = raySegmentDistance
M.axes = {x={x=1,y=0,z=0,color={1,0.15,0.15,1}},
    y={x=0,y=1,z=0,color={0.15,1,0.2,1}},
    z={x=0,y=0,z=1,color={0.2,0.45,1,1}}}

function M.pointRadius(options)
    return math.max(0.05, math.min(options.fGridCellWidthX, options.fGridCellDepthZ) * 0.08)
end

local geometryId = 0
function M.nextGeometryId()
    geometryId = geometryId + 1
    return geometryId
end

-- Same closest ray/axis parameter and parallel guard as the skeletal translation gizmo.
function M.axisParameter(sx, sy, origin, axis)
    local ox,oy,oz,dx,dy,dz = mbm.getPickRay(sx,sy)
    local wx,wy,wz = ox-origin.x,oy-origin.y,oz-origin.z
    local parallel = dx*axis.x+dy*axis.y+dz*axis.z
    local denominator = 1-parallel*parallel
    if denominator < 0.05 then return nil end
    return (axis.x*wx+axis.y*wy+axis.z*wz-parallel*(dx*wx+dy*wy+dz*wz))/denominator
end

function M.planeHit(sx, sy, origin, normal)
    local ox,oy,oz,dx,dy,dz = mbm.getPickRay(sx,sy)
    local denominator = dx*normal.x+dy*normal.y+dz*normal.z
    if math.abs(denominator) < 1e-6 then return nil end
    local t = ((origin.x-ox)*normal.x+(origin.y-oy)*normal.y+(origin.z-oz)*normal.z)/denominator
    if t < 0 then return nil end
    return ox+dx*t, oy+dy*t, oz+dz*t
end

-- Double-sided Moller-Trumbore intersection for the actual marker surface.
function M.rayTriangleDistance(ox,oy,oz,dx,dy,dz,a,b,c)
    local ex,ey,ez = b.x-a.x,b.y-a.y,b.z-a.z
    local fx,fy,fz = c.x-a.x,c.y-a.y,c.z-a.z
    local px,py,pz = dy*fz-dz*fy,dz*fx-dx*fz,dx*fy-dy*fx
    local det = ex*px+ey*py+ez*pz
    if math.abs(det) < 1e-9 then return nil end
    local tx,ty,tz = ox-a.x,oy-a.y,oz-a.z
    local u = (tx*px+ty*py+tz*pz)/det
    if u < 0 or u > 1 then return nil end
    local qx,qy,qz = ty*ez-tz*ey,tz*ex-tx*ez,tx*ey-ty*ex
    local v = (dx*qx+dy*qy+dz*qz)/det
    if v < 0 or u+v > 1 then return nil end
    local distance = (fx*qx+fy*qy+fz*qz)/det
    if distance >= 0 then return distance end
end

function M.translate(object, x, y, z)
    local dx,dy,dz = x-object.x,y-object.y,z-object.z
    if dx == 0 and dy == 0 and dz == 0 then return false end
    if (object.type == 'line' or object.type == 'triangle') and object.points then
        for _,p in ipairs(object.points) do p.x,p.y,p.z = p.x+dx,p.y+dy,p.z+dz end
    end
    object.x,object.y,object.z = x,y,z
    return true
end

return M
