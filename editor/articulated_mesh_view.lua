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
local edges={{1,2},{1,3},{1,5},{2,4},{2,6},{3,4},{3,7},{4,8},{5,6},{5,7},{6,8},{7,8}}
local function corners(b)
    return {{b.minX,b.minY,b.minZ},{b.maxX,b.minY,b.minZ},{b.minX,b.maxY,b.minZ},{b.maxX,b.maxY,b.minZ},
        {b.minX,b.minY,b.maxZ},{b.maxX,b.minY,b.maxZ},{b.minX,b.maxY,b.maxZ},{b.maxX,b.maxY,b.maxZ}}
end
function M.camera(E)
    local c=E.cam
    -- Keep the fitted mesh above the timeline while orbiting around its center.
    local offset=c.verticalOffset or 0
    local fx=c.fx+offset*math.sin(c.azimuth)*math.sin(c.elevation)
    local fy=c.fy-offset*math.cos(c.elevation)
    local fz=c.fz+offset*math.cos(c.azimuth)*math.sin(c.elevation)
    E.camera:setPos(fx+c.distance*math.cos(c.elevation)*math.sin(c.azimuth),
        fy+c.distance*math.sin(c.elevation),fz+c.distance*math.cos(c.elevation)*math.cos(c.azimuth))
    E.camera:setFocus(fx,fy,fz)
end
function M.fit(E)
    local b=E.bounds; if not b then return end
    E.cam.fx=(b.minX+b.maxX)/2; E.cam.fy=(b.minY+b.maxY)/2; E.cam.fz=(b.minZ+b.maxZ)/2
    E.extent=math.max(0.1,b.maxX-b.minX,b.maxY-b.minY,b.maxZ-b.minZ)
    E.cam.distance=E.extent*(E.showTimeline and 2.8 or 2.3)
    E.cam.verticalOffset=E.showTimeline and E.extent*0.45 or 0
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
    points={px-r,py,pz,px+r,py,pz,px,py-r,pz,px,py+r,pz,px,py,pz-r,px,py,pz+r}
    if E.pivotMarker then E.pivotMarker:set(points,1) else E.pivotMarker=line:new('3d'); E.pivotMarker:add(points); E.pivotMarker:setColor(1,0,1,1) end
    E.pivotMarker.visible=E.showPivot; E.pivotMarker.alwaysOnTop=true
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
function M.input(E)
    if tImGui.IsAnyWindowHovered() or tImGui.IsAnyItemActive() or tImGui.GetWantCaptureMouse() then E.cameraDrag=nil; return end
    local m=tImGui.GetMousePos()
    if tImGui.IsMouseClicked(0,false) then M.pick(E,m.x,m.y) end
    if tImGui.IsMouseClicked(1,false) or tImGui.IsMouseClicked(2,false) then
        E.cameraDrag={x=m.x,y=m.y,pan=tImGui.IsMouseDown(2)}
    end
    local drag=E.cameraDrag
    if drag then
        if not tImGui.IsMouseDown(1) and not tImGui.IsMouseDown(2) then E.cameraDrag=nil
        elseif drag.x~=m.x or drag.y~=m.y then
            local dx,dy=m.x-drag.x,m.y-drag.y; drag.x,drag.y=m.x,m.y
            if drag.pan then
                local r,u=E.camera:getNormal('R'),E.camera:getNormal('U'); local scale=E.cam.distance*0.0015
                E.cam.fx=E.cam.fx+(-dx*r.x+dy*u.x)*scale
                E.cam.fy=E.cam.fy+(-dx*r.y+dy*u.y)*scale
                E.cam.fz=E.cam.fz+(-dx*r.z+dy*u.z)*scale
            else E.cam.azimuth=E.cam.azimuth-dx*0.008; E.cam.elevation=math.max(-1.5,math.min(1.5,E.cam.elevation+dy*0.008)) end
            M.camera(E)
        end
    end
    local wheel=tImGui.GetZoom()
    if math.abs(wheel)>0.0001 then E.cam.distance=math.max(E.extent*0.02,math.min(E.extent*100,E.cam.distance*math.exp(-wheel*0.15))); M.camera(E) end
end
return M
