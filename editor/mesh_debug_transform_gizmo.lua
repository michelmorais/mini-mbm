--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
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

local AXES = {
    x={x=1,y=0,z=0,color={1,0.15,0.15,1}},
    y={x=0,y=1,z=0,color={0.15,1,0.2,1}},
    z={x=0,y=0,z=1,color={0.2,0.45,1,1}},
}

local function destroyObject(object)
    if object then pcall(function() object:destroy() end) end
end

function M.destroy(entry)
    local gizmo=entry and entry.tXformModernGizmo
    if not gizmo then return end
    for _,object in pairs(gizmo.axes or {}) do destroyObject(object) end
    entry.tXformModernGizmo=nil
end

local function rotatePoint(x,y,z,rx,ry,rz)
    if rx~=0 then
        local c,s=math.cos(rx),math.sin(rx)
        y,z=y*c-z*s,y*s+z*c
    end
    if ry~=0 then
        local c,s=math.cos(ry),math.sin(ry)
        x,z=x*c+z*s,-x*s+z*c
    end
    if rz~=0 then
        local c,s=math.cos(rz),math.sin(rz)
        x,y=x*c-y*s,x*s+y*c
    end
    return x,y,z
end

local function transformedBounds(minX,minY,minZ,maxX,maxY,maxZ,transform)
    transform=transform or {}
    local rx=math.rad(tonumber(transform.rx) or 0)
    local ry=math.rad(tonumber(transform.ry) or 0)
    local rz=math.rad(tonumber(transform.rz) or 0)
    local sx,sy,sz=tonumber(transform.sx) or 1,tonumber(transform.sy) or 1,
        tonumber(transform.sz) or 1
    local dx,dy,dz=tonumber(transform.dx) or 0,tonumber(transform.dy) or 0,
        tonumber(transform.dz) or 0
    local outMinX,outMinY,outMinZ,outMaxX,outMaxY,outMaxZ
    for _,x in ipairs({minX,maxX}) do
        for _,y in ipairs({minY,maxY}) do
            for _,z in ipairs({minZ,maxZ}) do
                local tx,ty,tz=rotatePoint(x,y,z,rx,ry,rz)
                tx,ty,tz=tx*sx+dx,ty*sy+dy,tz*sz+dz
                outMinX=not outMinX and tx or math.min(outMinX,tx)
                outMinY=not outMinY and ty or math.min(outMinY,ty)
                outMinZ=not outMinZ and tz or math.min(outMinZ,tz)
                outMaxX=not outMaxX and tx or math.max(outMaxX,tx)
                outMaxY=not outMaxY and ty or math.max(outMaxY,ty)
                outMaxZ=not outMaxZ and tz or math.max(outMaxZ,tz)
            end
        end
    end
    return outMinX,outMinY,outMinZ,outMaxX,outMaxY,outMaxZ
end

function M.rebuild(entry,bounds,transform,visible)
    if not visible or not bounds then M.destroy(entry); return end
    local minX,minY,minZ=tonumber(bounds.minX),tonumber(bounds.minY),tonumber(bounds.minZ)
    local maxX,maxY,maxZ=tonumber(bounds.maxX),tonumber(bounds.maxY),tonumber(bounds.maxZ)
    if not minX or not minY or not minZ or not maxX or not maxY or not maxZ then
        M.destroy(entry)
        return
    end
    minX,minY,minZ,maxX,maxY,maxZ=transformedBounds(
        minX,minY,minZ,maxX,maxY,maxZ,transform)
    local cx=(minX+maxX)*0.5
    local cy=(minY+maxY)*0.5
    local cz=(minZ+maxZ)*0.5
    local extent=math.max(maxX-minX,maxY-minY,maxZ-minZ)
    local length=math.max(extent * 0.33,0.05)
    local fingerprint=string.format('%.6g|%.6g|%.6g|%.6g',cx,cy,cz,length)
    local gizmo=entry.tXformModernGizmo or {axes={}}
    entry.tXformModernGizmo=gizmo
    if gizmo.fingerprint==fingerprint then return end
    for name,axis in pairs(AXES) do
        local points={0,0,0,axis.x*length,axis.y*length,axis.z*length,
            axis.x*length,axis.y*length,axis.z*length,
            axis.x*length*0.88,axis.y*length*0.88,axis.z*length*0.88}
        local object=gizmo.axes[name]
        if object then object:set(points,1) else
            object=line:new('3d',0,0,0); object:add(points); object.alwaysOnTop=true
            gizmo.axes[name]=object
        end
        object:setPos(cx,cy,cz)
        object:setColor(table.unpack(axis.color))
        object.visible=true
    end
    gizmo.origin={x=cx,y=cy,z=cz}
    gizmo.length=length
    gizmo.fingerprint=fingerprint
end

local function rayAxisParameter(sx,sy,origin,axis)
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local wx,wy,wz=ox-origin.x,oy-origin.y,oz-origin.z
    local dot=dx*axis.x+dy*axis.y+dz*axis.z
    local denom=1-dot*dot
    if math.abs(denom)<0.05 then return nil end
    return ((axis.x*wx+axis.y*wy+axis.z*wz)-dot*(dx*wx+dy*wy+dz*wz))/denom
end

local function raySegmentDistance(ox,oy,oz,dx,dy,dz,a,b,radius)
    local sx,sy,sz=b.x-a.x,b.y-a.y,b.z-a.z
    local lengthSquared=sx*sx+sy*sy+sz*sz
    if lengthSquared<1e-12 then return nil end
    local wx,wy,wz=ox-a.x,oy-a.y,oz-a.z
    local uv=dx*sx+dy*sy+dz*sz
    local uw=dx*wx+dy*wy+dz*wz
    local vw=sx*wx+sy*wy+sz*wz
    local denominator=lengthSquared-uv*uv
    local segmentT=math.abs(denominator)<1e-12 and -vw/lengthSquared or
        (vw-uv*uw)/denominator
    segmentT=math.max(0,math.min(1,segmentT))
    local px,py,pz=a.x+sx*segmentT,a.y+sy*segmentT,a.z+sz*segmentT
    local rayT=(px-ox)*dx+(py-oy)*dy+(pz-oz)*dz
    if rayT<0 then return nil end
    local qx,qy,qz=ox+dx*rayT,oy+dy*rayT,oz+dz*rayT
    local ex,ey,ez=px-qx,py-qy,pz-qz
    return ex*ex+ey*ey+ez*ez<=radius*radius and rayT or nil
end

function M.hitTest(entry,sx,sy)
    local gizmo=entry and entry.tXformModernGizmo
    if not gizmo or not gizmo.origin or not gizmo.length then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local radius=math.max(gizmo.length*0.08,0.001)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs(AXES) do
        if math.abs(dx*axis.x+dy*axis.y+dz*axis.z)<0.95 then
            local a={x=gizmo.origin.x+axis.x*gizmo.length*0.25,
                y=gizmo.origin.y+axis.y*gizmo.length*0.25,
                z=gizmo.origin.z+axis.z*gizmo.length*0.25}
            local b={x=gizmo.origin.x+axis.x*gizmo.length,
                y=gizmo.origin.y+axis.y*gizmo.length,
                z=gizmo.origin.z+axis.z*gizmo.length}
            local distance=raySegmentDistance(ox,oy,oz,dx,dy,dz,a,b,radius)
            if distance and distance<bestDistance then best,bestDistance=name,distance end
        end
    end
    return best
end

function M.beginDrag(entry,axisName,sx,sy,xf)
    local gizmo=entry and entry.tXformModernGizmo
    local axis=AXES[axisName]
    if not gizmo or not axis then return false end
    local parameter=rayAxisParameter(sx,sy,gizmo.origin,axis)
    if not parameter then return false end
    entry.tXformSubsetDrag={mode='gizmo_translate',axisName=axisName,axis=axis,
        origin=gizmo.origin,startParameter=parameter,initialDx=xf.dx,initialDy=xf.dy,
        initialDz=xf.dz,deltaX=0,deltaY=0,deltaZ=0}
    return true
end

function M.updateDrag(drag,sx,sy,snapStep)
    local parameter=rayAxisParameter(sx,sy,drag.origin,drag.axis)
    if not parameter then return nil end
    local amount=parameter-drag.startParameter
    if snapStep and snapStep>1e-9 then
        local units=amount/snapStep
        amount=(units>=0 and math.floor(units+0.5) or math.ceil(units-0.5))*snapStep
    end
    drag.deltaX,drag.deltaY,drag.deltaZ=drag.axis.x*amount,drag.axis.y*amount,drag.axis.z*amount
    return drag.deltaX,drag.deltaY,drag.deltaZ
end

function M.setDragOffset(entry,dx,dy,dz)
    local gizmo=entry and entry.tXformModernGizmo
    if not gizmo or not gizmo.origin then return end
    for _,object in pairs(gizmo.axes or {}) do
        object:setPos(gizmo.origin.x+dx,gizmo.origin.y+dy,gizmo.origin.z+dz)
    end
end

function M.snapFreeDelta(dx,dy,dz,snapAxes,snapStep)
    snapAxes=snapAxes or {}
    local constrained=snapAxes.x or snapAxes.y or snapAxes.z
    if constrained then
        dx=snapAxes.x and dx or 0; dy=snapAxes.y and dy or 0; dz=snapAxes.z and dz or 0
    end
    if snapStep and snapStep>1e-9 then
        local function snap(value)
            local units=value/snapStep
            return (units>=0 and math.floor(units+0.5) or math.ceil(units-0.5))*snapStep
        end
        dx,dy,dz=snap(dx),snap(dy),snap(dz)
    end
    return dx,dy,dz
end

return M
