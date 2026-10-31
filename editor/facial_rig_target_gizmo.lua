--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR         |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        |
|-------------------------------------------------------------------------------------------------------------------------|

   Facial target rotation-pivot model and pick math. A pivot is editor metadata keyed by the
   stable target ID; only the resulting vertex deltas are exported to MSH.
]]--

local M={}

M.axes={x={x=1,y=0,z=0},y={x=0,y=1,z=0},z={x=0,y=0,z=1}}

local function finite(value)
    return type(value)=='number' and value==value and value~=math.huge and value~=-math.huge
end

local function copyPoint(point)
    return {x=point.x,y=point.y,z=point.z}
end

function M.normalizePivots(pivots)
    local result={}
    if type(pivots)~='table' then return result end
    for id,point in pairs(pivots) do
        if type(id)=='string' and id~='' and type(point)=='table' and
                finite(point.x) and finite(point.y) and finite(point.z) then
            result[id]=copyPoint(point)
        end
    end
    return result
end

function M.remapPivots(pivots,idMap)
    idMap=idMap or {}
    local result={}
    for id,point in pairs(pivots or {}) do
        result[idMap[id] or id]=copyPoint(point)
    end
    return result
end

function M.defaultPivot(project,bounds)
    local face=project and project.faceOrigin
    if face and face.placed and face.position and
            finite(face.position.x) and finite(face.position.y) and finite(face.position.z) then
        return copyPoint(face.position)
    end
    if not bounds then return {x=0,y=0,z=0} end
    return {x=((bounds.minX or 0)+(bounds.maxX or 0))*0.5,
        y=((bounds.minY or 0)+(bounds.maxY or 0))*0.5,
        z=((bounds.minZ or 0)+(bounds.maxZ or 0))*0.5}
end

function M.pivot(project,id,bounds)
    local point=project and project.morphTargetPivots and project.morphTargetPivots[id]
    if point and finite(point.x) and finite(point.y) and finite(point.z) then return copyPoint(point) end
    return M.defaultPivot(project,bounds)
end

function M.setPivot(project,id,point)
    if not project or type(id)~='string' or id=='' or not point or
            not finite(point.x) or not finite(point.y) or not finite(point.z) then return false end
    project.morphTargetPivots=project.morphTargetPivots or {}
    project.morphTargetPivots[id]=copyPoint(point)
    return true
end

function M.ringPoints(name,radius)
    local axes=M.axes
    local u,v=name=='x' and axes.y or name=='y' and axes.z or axes.x,
        name=='x' and axes.z or name=='y' and axes.x or axes.y
    local points={}
    for item=0,64 do
        local angle=item/64*math.pi*2
        local c,s=math.cos(angle)*radius,math.sin(angle)*radius
        points[#points+1]=u.x*c+v.x*s
        points[#points+1]=u.y*c+v.y*s
        points[#points+1]=u.z*c+v.z*s
    end
    return points
end

function M.axisParameter(origin,direction,pivot,axis)
    local wx,wy,wz=origin.x-pivot.x,origin.y-pivot.y,origin.z-pivot.z
    local parallel=direction.x*axis.x+direction.y*axis.y+direction.z*axis.z
    local rayProjection=direction.x*wx+direction.y*wy+direction.z*wz
    local axisProjection=axis.x*wx+axis.y*wy+axis.z*wz
    local denominator=1-parallel*parallel
    if math.abs(denominator)<0.05 then return nil end
    return (axisProjection-parallel*rayProjection)/denominator
end

function M.ringDirection(origin,direction,pivot,axis)
    local denominator=direction.x*axis.x+direction.y*axis.y+direction.z*axis.z
    if math.abs(denominator)<1e-8 then return nil end
    local distance=((pivot.x-origin.x)*axis.x+(pivot.y-origin.y)*axis.y+
        (pivot.z-origin.z)*axis.z)/denominator
    if distance<0 then return nil end
    local x=origin.x+direction.x*distance-pivot.x
    local y=origin.y+direction.y*distance-pivot.y
    local z=origin.z+direction.z*distance-pivot.z
    local length=math.sqrt(x*x+y*y+z*z)
    if length<1e-8 then return nil end
    return {x=x/length,y=y/length,z=z/length},length
end

function M.hitRing(origin,direction,pivot,radius)
    local best,bestError=nil,math.huge
    local tolerance=math.max(radius*0.12,0.003)
    for name,axis in pairs(M.axes) do
        local vector,radial=M.ringDirection(origin,direction,pivot,axis)
        local error=radial and math.abs(radial-radius) or math.huge
        if error<=tolerance and error<bestError then
            best,bestError={name=name,axis=axis,direction=vector},error
        end
    end
    return best
end

function M.hitAxis(origin,direction,pivot,length,radius)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs(M.axes) do
        local alignment=math.abs(direction.x*axis.x+direction.y*axis.y+direction.z*axis.z)
        if alignment<0.95 then
            local sx,sy,sz=pivot.x+axis.x*length*0.25,pivot.y+axis.y*length*0.25,
                pivot.z+axis.z*length*0.25
            local vx,vy,vz=axis.x*length*0.75,axis.y*length*0.75,axis.z*length*0.75
            local wx,wy,wz=origin.x-sx,origin.y-sy,origin.z-sz
            local uv=direction.x*vx+direction.y*vy+direction.z*vz
            local uw=direction.x*wx+direction.y*wy+direction.z*wz
            local vw=vx*wx+vy*wy+vz*wz
            local lengthSquared=vx*vx+vy*vy+vz*vz
            local denominator=lengthSquared-uv*uv
            local segment=math.abs(denominator)<1e-12 and -vw/lengthSquared or
                (vw-uv*uw)/denominator
            segment=math.max(0,math.min(1,segment))
            local px,py,pz=sx+vx*segment,sy+vy*segment,sz+vz*segment
            local rayDistance=(px-origin.x)*direction.x+(py-origin.y)*direction.y+
                (pz-origin.z)*direction.z
            if rayDistance>=0 then
                local ex,ey,ez=px-origin.x-direction.x*rayDistance,
                    py-origin.y-direction.y*rayDistance,pz-origin.z-direction.z*rayDistance
                local distance=math.sqrt(ex*ex+ey*ey+ez*ez)
                if distance<=radius and distance<bestDistance then
                    best,bestDistance=name,distance
                end
            end
        end
    end
    return best
end

function M.signedAngle(axis,startDirection,currentDirection)
    local start,current=startDirection,currentDirection
    local cx=start.y*current.z-start.z*current.y
    local cy=start.z*current.x-start.x*current.z
    local cz=start.x*current.y-start.y*current.x
    local sine=axis.x*cx+axis.y*cy+axis.z*cz
    local cosine=math.max(-1,math.min(1,start.x*current.x+start.y*current.y+start.z*current.z))
    return math.deg(math.atan(sine,cosine))
end

return M
