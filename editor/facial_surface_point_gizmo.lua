--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   3D selection and translation of facial surface-control anchors.
]]--

local M={}
M.axes={x={x=1,y=0,z=0,color={1,0.18,0.18,1}},
    y={x=0,y=1,z=0,color={0.18,1,0.22,1}},
    z={x=0,y=0,z=1,color={0.22,0.48,1,1}}}

local function position(control,point,topology)
    local raw=control and control.points and control.points[point]
    local index=type(raw)=='table' and tonumber(raw.index) or tonumber(raw)
    local vertex=index and topology and topology.vertices and topology.vertices[index]
    if not vertex then return nil end
    local offset=type(raw)=='table' and raw.offset or nil
    return {x=vertex.x+(offset and offset.x or 0),
        y=vertex.y+(offset and offset.y or 0),
        z=vertex.z+(offset and offset.z or 0)+(control.depthOffset or 0)}
end
M.position=position

local function rayPoint(origin,direction,point)
    local vx,vy,vz=point.x-origin.x,point.y-origin.y,point.z-origin.z
    local along=vx*direction.x+vy*direction.y+vz*direction.z
    if along<0 then return nil end
    local dx,dy,dz=vx-direction.x*along,vy-direction.y*along,vz-direction.z*along
    return math.sqrt(dx*dx+dy*dy+dz*dz),along
end

local function raySegment(origin,direction,a,b)
    local vx,vy,vz=b.x-a.x,b.y-a.y,b.z-a.z
    local lengthSquared=vx*vx+vy*vy+vz*vz
    if lengthSquared<1e-12 then return rayPoint(origin,direction,a) end
    local wx,wy,wz=origin.x-a.x,origin.y-a.y,origin.z-a.z
    local uv=direction.x*vx+direction.y*vy+direction.z*vz
    local uw=direction.x*wx+direction.y*wy+direction.z*wz
    local vw=vx*wx+vy*wy+vz*wz
    local denominator=lengthSquared-uv*uv
    local t=math.abs(denominator)<1e-12 and -vw/lengthSquared or (vw-uv*uw)/denominator
    t=math.max(0,math.min(1,t))
    local closest={x=a.x+vx*t,y=a.y+vy*t,z=a.z+vz*t}
    local distance,along=rayPoint(origin,direction,closest)
    return distance,along,t
end

-- Points take precedence over outlines. Clicking a segment selects its nearer endpoint.
function M.pick(entries,topology,origin,direction,pointRadius,lineRadius)
    local bestPoint,bestLine
    for _,entry in ipairs(entries or {}) do
        local control=entry.control
        local points={}
        for index=1,#(control.points or {}) do
            local point=position(control,index,topology)
            points[index]=point
            if point then
                local distance,along=rayPoint(origin,direction,point)
                if distance and distance<=pointRadius and
                        (not bestPoint or distance<bestPoint.distance or
                            distance==bestPoint.distance and along<bestPoint.along) then
                    bestPoint={entry=entry,pointIndex=index,distance=distance,along=along}
                end
            end
        end
        local edgeCount=control.closed and #points or #points-1
        for index=1,edgeCount do
            local nextIndex=index%#points+1
            local a,b=points[index],points[nextIndex]
            if a and b then
                local distance,along,t=raySegment(origin,direction,a,b)
                if distance and distance<=lineRadius and
                        (not bestLine or distance<bestLine.distance or
                            distance==bestLine.distance and along<bestLine.along) then
                    bestLine={entry=entry,pointIndex=t<=0.5 and index or nextIndex,
                        distance=distance,along=along}
                end
            end
        end
    end
    return bestPoint or bestLine
end

function M.pickAxis(origin,direction,pivot,length,radius)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs(M.axes) do
        local alignment=math.abs(direction.x*axis.x+direction.y*axis.y+direction.z*axis.z)
        if alignment<0.95 then
            local a={x=pivot.x+axis.x*length*0.3,y=pivot.y+axis.y*length*0.3,
                z=pivot.z+axis.z*length*0.3}
            local b={x=pivot.x+axis.x*length,y=pivot.y+axis.y*length,z=pivot.z+axis.z*length}
            local distance=raySegment(origin,direction,a,b)
            if distance and distance<=radius and distance<bestDistance then
                best,bestDistance=name,distance
            end
        end
    end
    return best
end

function M.axisParameter(origin,direction,pivot,axis)
    local wx,wy,wz=origin.x-pivot.x,origin.y-pivot.y,origin.z-pivot.z
    local parallel=direction.x*axis.x+direction.y*axis.y+direction.z*axis.z
    local denominator=1-parallel*parallel
    if denominator<0.05 then return nil end
    local rayProjection=direction.x*wx+direction.y*wy+direction.z*wz
    local axisProjection=axis.x*wx+axis.y*wy+axis.z*wz
    return (axisProjection-parallel*rayProjection)/denominator
end

function M.dragEntries(control,pointIndex,groupEntries)
    if not control or not pointIndex then return {} end
    if control.kind=='point' and control.role=='controller' then
        return groupEntries or {}
    end
    return {{control=control,pointIndex=pointIndex}}
end

function M.snapshot(entries,topology)
    local result={}
    for _,entry in ipairs(entries or {}) do
        local points={}
        for index=1,#(entry.control.points or {}) do
            if not entry.pointIndex or index==entry.pointIndex then
                local resolved=position(entry.control,index,topology)
                if resolved then points[#points+1]={index=index,position=resolved} end
            end
        end
        result[#result+1]={control=entry.control,points=points}
    end
    return result
end

function M.translate(snapshot,topology,delta)
    for _,entry in ipairs(snapshot or {}) do
        for _,point in ipairs(entry.points) do
            local base=point.position
            local raw=entry.control.points[point.index]
            local vertex=raw and topology.vertices[raw.index]
            if base and vertex then
                raw.offset={x=base.x+delta.x-vertex.x,
                    y=base.y+delta.y-vertex.y,
                    z=base.z+delta.z-vertex.z-(entry.control.depthOffset or 0)}
            end
        end
    end
end

-- Plan first so a failed raycast cannot leave half a shape moved. Restrict
-- intersections to the anchor's mesh subset to avoid hair and eye surfaces.
function M.planSurfaceSnap(control,topology,bounds,intersect)
    if not control or not topology or not topology.vertices or not bounds then return nil end
    local first=control.points and control.points[1]
    local firstIndex=type(first)=='table' and first.index or first
    local firstRef=topology.refs and topology.refs[firstIndex]
    local subset=firstRef and firstRef.subset
    local allowed
    if subset then
        allowed={}
        for index,ref in ipairs(topology.refs) do
            if ref.subset==subset then allowed[index]=true end
        end
    end
    local extent=math.max(bounds.maxZ-bounds.minZ,1)
    local front=bounds.maxZ+extent*2
    local plan={offsets={},changed=math.abs(control.depthOffset or 0)>1e-8}
    for index,raw in ipairs(control.points or {}) do
        local vertex=topology.vertices[raw.index]
        local world=position(control,index,topology)
        if not vertex or not world then return nil end
        local old=raw.offset or {}
        local x,y=old.x or 0,old.y or 0
        local hit=intersect(topology,{x=world.x,y=world.y,z=front},
            {x=0,y=0,z=-1},allowed)
        local targetZ=hit and hit.point.z
        if not targetZ and math.abs(x)<1e-8 and math.abs(y)<1e-8 then
            targetZ=vertex.z
        end
        if not targetZ then return nil end
        local z=targetZ-vertex.z
        if math.abs(z-(old.z or 0))>1e-8 then plan.changed=true end
        plan.offsets[index]=math.abs(x)+math.abs(y)+math.abs(z)>1e-8 and
            {x=x,y=y,z=z} or false
    end
    return plan
end

function M.applySurfaceSnap(control,plan)
    for index,offset in ipairs(plan.offsets) do
        control.points[index].offset=offset or nil
    end
    control.depthOffset=0
end

return M
