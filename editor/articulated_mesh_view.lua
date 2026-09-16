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

local Pose=require 'articulated_mesh_pose'
local Model=require 'articulated_sprite_model'
local M={}
local function unitSphereVerts(latSegments, lonSegments)
    latSegments = latSegments or 8
    lonSegments = lonSegments or 12
    local function toXYZ(theta, phi)
        local s = math.sin(theta)
        return s * math.cos(phi), math.cos(theta), s * math.sin(phi)
    end
    local verts = {}
    local function push(x, y, z) table.insert(verts, x); table.insert(verts, y); table.insert(verts, z) end
    for i = 0, latSegments - 1 do
        local theta1 = (i / latSegments) * math.pi
        local theta2 = ((i + 1) / latSegments) * math.pi
        for j = 0, lonSegments - 1 do
            local phi1 = (j / lonSegments) * math.pi * 2
            local phi2 = ((j + 1) / lonSegments) * math.pi * 2
            local x1, y1, z1 = toXYZ(theta1, phi1)
            local x2, y2, z2 = toXYZ(theta1, phi2)
            local x3, y3, z3 = toXYZ(theta2, phi1)
            local x4, y4, z4 = toXYZ(theta2, phi2)
            push(x1, y1, z1); push(x3, y3, z3); push(x4, y4, z4)
            push(x1, y1, z1); push(x4, y4, z4); push(x2, y2, z2)
        end
    end
    return verts
end

local edges={{1,2},{1,3},{1,5},{2,4},{2,6},{3,4},{3,7},{4,8},{5,6},{5,7},{6,8},{7,8}}
local function corners(b)
    return {{b.minX,b.minY,b.minZ},{b.maxX,b.minY,b.minZ},{b.minX,b.maxY,b.minZ},{b.maxX,b.maxY,b.minZ},
        {b.minX,b.minY,b.maxZ},{b.maxX,b.minY,b.maxZ},{b.minX,b.maxY,b.maxZ},{b.maxX,b.maxY,b.maxZ}}
end
function M.camera(E)
    local c=E.cam
    E.camera:setPos(c.fx+c.distance*math.cos(c.elevation)*math.sin(c.azimuth),
        c.fy+c.distance*math.sin(c.elevation),c.fz+c.distance*math.cos(c.elevation)*math.cos(c.azimuth))
    E.camera:setFocus(c.fx,c.fy,c.fz)
end
function M.reset(E)
    E.cameraDrag=nil; E.cam.azimuth=0.3; E.cam.elevation=0.3
    M.fit(E)
end
function M.fit(E)
    local b=E.bounds; if not b then return end
    E.cam.fx=(b.minX+b.maxX)/2; E.cam.fy=(b.minY+b.maxY)/2; E.cam.fz=(b.minZ+b.maxZ)/2
    E.extent=math.max(0.1,b.maxX-b.minX,b.maxY-b.minY,b.maxZ-b.minZ)
    E.cam.distance=E.extent*(E.showTimeline and 2.8 or 2.3)
    -- Apply layout compensation once, not as an angle-dependent moving orbit target.
    local offset=E.showTimeline and E.extent*0.45 or 0
    E.cam.fx=E.cam.fx+offset*math.sin(E.cam.azimuth)*math.sin(E.cam.elevation)
    E.cam.fy=E.cam.fy-offset*math.cos(E.cam.elevation)
    E.cam.fz=E.cam.fz+offset*math.cos(E.cam.azimuth)*math.sin(E.cam.elevation)
    M.camera(E)
end
function M.destroy(E)
    if E.outline then E.outline:destroy(); E.outline=nil end
    if E.pivotMarker then E.pivotMarker:destroy(); E.pivotMarker=nil end
    E.viewSignature=nil
end
function M.update(E)
    if not E.bounds then return end
    local signature=table.concat({E.geometryRevision,E.frame,E.selected,E.mode,E.time,E.poseRevision or 0},':')
    if E.viewSignature==signature then return end
    E.viewSignature=signature
    local transform=Pose.transforms(E); E.transform=transform
    local p=Model.part(E.project,E.frame,E.selected)
    if not p then
        if E.outline then E.outline.visible=false end
        if E.pivotMarker then E.pivotMarker.visible=false end
        return
    end
    local b=E.subsetBounds[E.frame][p.subset]
    local c=corners(b); for i,v in ipairs(c) do c[i]={transform(p.id,table.unpack(v))} end
    local points={}
    for _,edge in ipairs(edges) do for _,i in ipairs(edge) do for _,v in ipairs(c[i]) do points[#points+1]=v end end end
    if E.outline then E.outline:set(points,1) else E.outline=line:new('3d'); E.outline:add(points); E.outline:setColor(0.1,0.8,1,1) end
    E.outline.visible=E.showOutline; E.outline.alwaysOnTop=true
    local px,py,pz=transform(p.id,table.unpack(p.pivot)); local r=E.extent*0.012
    if not E.pivotMarker then
        E.pivotMarker=shape:new('3d')
        E.pivotMarker:create(unitSphereVerts(),nil,'articulated_mesh_pivot_orange')
        E.pivotMarker:setColor(1,0.5,0,1)
        E.pivotMarker.alwaysOnTop=true
    end
    E.pivotMarker:setPos(px,py,pz)
    E.pivotMarker:setScale(r,r,r)
    E.pivotMarker.visible=E.showPivot
end
function M.pick(E,x,y)
    if not E.transform then return end
    local ox,oy,oz,dx,dy,dz=mbm.getPickRay(x,y)
    local best,distance=nil,math.huge
    for _,p in ipairs(E.project.frames[E.frame].parts) do
        local c=corners(E.subsetBounds[E.frame][p.subset]); local lo={math.huge,math.huge,math.huge}; local hi={-math.huge,-math.huge,-math.huge}
        for _,v in ipairs(c) do local w={E.transform(p.id,table.unpack(v))}; for i=1,3 do lo[i]=math.min(lo[i],w[i]); hi[i]=math.max(hi[i],w[i]) end end
        local first,last=0,math.huge; local origin,dir={ox,oy,oz},{dx,dy,dz}
        for i=1,3 do
            if math.abs(dir[i])<1e-8 then if origin[i]<lo[i] or origin[i]>hi[i] then last=-1 end
            else local a,b=(lo[i]-origin[i])/dir[i],(hi[i]-origin[i])/dir[i]; first=math.max(first,math.min(a,b)); last=math.min(last,math.max(a,b)) end
        end
        if last>=first and first<distance then best,distance=p.id,first end
    end
    if best then E.selected=best; Pose.sync(E) end
end
local function captured()
    return tImGui.IsAnyWindowHovered() or tImGui.IsAnyItemActive() or tImGui.GetWantCaptureMouse()
end
function M.pointerDown(E,key,x,y)
    if captured() or not E.bounds then E.cameraDrag=nil; return end
    if key==1 then M.pick(E,x,y)
    elseif key==0 or key==2 then E.cameraDrag={x=x,y=y,key=key} end
end
function M.pointerUp(E,key)
    if E.cameraDrag and E.cameraDrag.key==key then E.cameraDrag=nil end
end
function M.pointerMove(E,x,y)
    if captured() then E.cameraDrag=nil; return end
    local drag=E.cameraDrag
    if not drag or (drag.x==x and drag.y==y) then return end
    local dx,dy=x-drag.x,y-drag.y; drag.x,drag.y=x,y
    if drag.key==2 then
        -- Mesh Debug convention: horizontal camera-right on XZ, vertical on world Y.
        local scale=E.cam.distance*0.001
        E.cam.fx=E.cam.fx+dx*math.cos(E.cam.azimuth)*scale
        E.cam.fy=E.cam.fy+dy*scale
        E.cam.fz=E.cam.fz-dx*math.sin(E.cam.azimuth)*scale
    else
        E.cam.azimuth=E.cam.azimuth-dx*0.005
        E.cam.elevation=math.max(-math.pi*0.49,math.min(math.pi*0.49,E.cam.elevation+dy*0.005))
    end
    M.camera(E)
end
function M.input(E)
    if captured() then E.cameraDrag=nil; return end
    if not E.bounds then return end
    local wheel=tImGui.GetZoom()
    if math.abs(wheel)>0.0001 then E.cam.distance=math.max(E.extent*0.02,math.min(E.extent*100,E.cam.distance*math.exp(-wheel*0.15))); M.camera(E) end
end
return M
