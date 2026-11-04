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
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR   |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-------------------------------------------------------------------------------------------------------------------------|

   Rotation and pivot gizmo for generic Morph Editor targets.
]]--

-- The facial editor already owns the engine-independent ray/ring math. Keep the
-- generic viewport representation here so Morph Editor does not depend on a
-- facial project or on facial target records.
local tGizmoMath=require 'facial_rig_target_gizmo'
local M={}
for key,value in pairs(tGizmoMath) do M[key]=value end

local function copyPoint(point)
    return {x=point.x,y=point.y,z=point.z}
end

local function hideCollection(collection)
    for _,object in pairs(collection or {}) do object.visible=false end
end

function M.hide(gizmo)
    if not gizmo then return end
    hideCollection(gizmo.rings)
    hideCollection(gizmo.axes)
    if gizmo.center then gizmo.center.visible=false end
    gizmo.visible=false
    gizmo.drag=nil
end

function M.destroy(gizmo)
    if not gizmo then return end
    for _,object in pairs(gizmo.rings or {}) do object:destroy() end
    for _,object in pairs(gizmo.axes or {}) do object:destroy() end
    if gizmo.center then gizmo.center:destroy() end
    gizmo.rings={}
    gizmo.axes={}
    gizmo.center=nil
    gizmo.visible=false
    gizmo.drag=nil
end

local function crossPaths(size)
    return {
        {-size,0,0,size,0,0},
        {0,-size,0,0,size,0},
        {0,0,-size,0,0,size},
    }
end

function M.refresh(gizmo,pivot,extent,drag)
    if not gizmo or not pivot then return end
    gizmo.rings=gizmo.rings or {}
    gizmo.axes=gizmo.axes or {}
    extent=math.max(tonumber(extent) or 1,1e-6)
    local radius=math.max(extent*0.11,0.04)
    local length=radius*1.4
    local sizeChanged=gizmo.radius~=radius
    local colors={x={1,0.15,0.15},y={0.15,1,0.2},z={0.2,0.45,1}}
    for name,axis in pairs(M.axes) do
        local ring=gizmo.rings[name]
        if not ring then
            ring=line:new('3d',0,0,0)
            ring:add(M.ringPoints(name,radius))
            ring.alwaysOnTop=true
            ring.alwaysOnTopPriority=3
            ring.alwaysRender=true
            gizmo.rings[name]=ring
        elseif sizeChanged then
            ring:set(M.ringPoints(name,radius),1)
        end
        ring:setPos(pivot.x,pivot.y,pivot.z)
        ring:setColor(colors[name][1],colors[name][2],colors[name][3],
            drag and drag.kind=='ring' and drag.name~=name and 0.3 or 1)
        ring.visible=true

        local handle=gizmo.axes[name]
        if not handle then
            handle=line:new('3d',0,0,0)
            handle:add({0,0,0,axis.x*length,axis.y*length,axis.z*length})
            handle.alwaysOnTop=true
            handle.alwaysOnTopPriority=4
            handle.alwaysRender=true
            gizmo.axes[name]=handle
        elseif sizeChanged then
            handle:set({0,0,0,axis.x*length,axis.y*length,axis.z*length},1)
        end
        handle:setPos(pivot.x,pivot.y,pivot.z)
        handle:setColor(colors[name][1],colors[name][2],colors[name][3],
            drag and drag.kind=='axis' and drag.name~=name and 0.3 or 1)
        handle.visible=true
    end

    local centerSize=radius*0.1
    local cross=crossPaths(centerSize)
    if not gizmo.center then
        gizmo.center=line:new('3d',0,0,0)
        for _,path in ipairs(cross) do gizmo.center:add(path) end
        gizmo.center.alwaysOnTop=true
        gizmo.center.alwaysOnTopPriority=5
        gizmo.center.alwaysRender=true
        gizmo.center:setColor(1,0.85,0.1,1)
    elseif sizeChanged then
        for index,path in ipairs(cross) do gizmo.center:set(path,index) end
    end
    gizmo.center:setPos(pivot.x,pivot.y,pivot.z)
    gizmo.center.visible=true
    gizmo.origin=copyPoint(pivot)
    gizmo.radius=radius
    gizmo.length=length
    gizmo.visible=true
end

return M
