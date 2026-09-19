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

local Model=require 'image_mesh_model'
local M={}
local function clamp(v,lo,hi) return math.max(lo,math.min(hi,v)) end
function M.cancel(E)
    if E.drag then E.project=E.drag.before or E.project end
    E.drag=nil; E.polygon={}; E.outlines=nil; E.canvasDirty=true
end
function M.destroy(E)
    for _,object in ipairs(E.sceneLines or {}) do object:destroy() end
    E.sceneLines={}
    if E.imageObject then E.imageObject:destroy(); E.imageObject=nil end
    if E.checkerObject then E.checkerObject:destroy(); E.checkerObject=nil end
    E.canvasPath=nil; E.canvasDirty=true
end
function M.zoom(E,value)
    E.zoom=clamp(value,0.02,32)
end
function M.fit(E)
    if not E.texture then return end
    local w,h=E.project.image.width,E.project.image.height
    M.zoom(E,math.min(math.max(50,E.screenW-E.sidebar-E.rightbar-40)/w,math.max(50,E.screenH-65)/h))
    E.camera2d:setPos(-(E.sidebar-E.rightbar)/2/E.camera2d.sx,12.5/E.camera2d.sy)
end
function M.transform(E)
    local c=E.camera2d
    local w,h=E.project.image.width,E.project.image.height
    local transform=E.canvasTransform or {}
    transform.x=E.screenW/2+(-w/2*E.zoom-c.x)*c.sx
    transform.y=E.screenH/2-(h/2*E.zoom-c.y)*c.sy
    transform.scale=E.zoom*c.sx
    E.canvasTransform=transform
    return E.canvasTransform
end
function M.sync(E)
    local visible=E.editMode and E.texture~=nil
    if E.imageObject then E.imageObject.visible=visible end
    if E.checkerObject then E.checkerObject.visible=visible end
    if E.canvasVisible~=visible then
        for _,object in ipairs(E.sceneLines or {}) do object.visible=visible end
        E.canvasVisible=visible
    end
    if not visible then return end
    local w,h=E.project.image.width,E.project.image.height
    local scale=E.zoom*E.camera2d.sx
    M.transform(E)
    local zoomed=E.canvasScale~=scale
    if E.canvasPath~=E.project.image.path then
        M.destroy(E)
        E.imageObject=texture:new('2dw'); assert(E.imageObject:load(E.project.image.path))
        if E.checkerPath then E.checkerObject=texture:new('2dw'); assert(E.checkerObject:load(E.checkerPath)) end
        E.imageObject:setSize(w,h); E.imageObject:setPos(0,0,1)
        if E.checkerObject then E.checkerObject:setSize(w,h); E.checkerObject:setPos(0,0,2) end
        E.canvasPath=E.project.image.path
    end
    if zoomed or E.canvasDirty then
        E.imageObject:setScale(E.zoom,E.zoom)
        if E.checkerObject then E.checkerObject:setScale(E.zoom,E.zoom) end
    end
    if not E.canvasDirty and E.outlines and not zoomed then return end
    E.canvasDirty=false; E.canvasScale=scale
    for _,object in ipairs(E.sceneLines or {}) do object:destroy() end
    E.sceneLines={}; E.canvasBuilds=(E.canvasBuilds or 0)+1
    local function draw(points,closed,selected)
        if #points<2 then return end
        local vertices={}
        for _,p in ipairs(points) do vertices[#vertices+1]=p.x-w/2; vertices[#vertices+1]=h/2-p.y end
        if closed then vertices[#vertices+1]=vertices[1]; vertices[#vertices+1]=vertices[2] end
        local object=line:new('2dw',0,0,0); object:add(vertices); object:setScale(E.zoom,E.zoom)
        if selected then object:setColor(1,0.7,0.1) else object:setColor(0.2,0.8,1) end
        E.sceneLines[#E.sceneLines+1]=object
    end
    local function handle(px,py)
        local d=4/scale
        draw({{x=px-d,y=py-d},{x=px+d,y=py-d},{x=px+d,y=py+d},{x=px-d,y=py+d}},true,true)
    end
    if not E.outlines then
        E.outlines={}
        for _,r in ipairs(E.project.regions) do E.outlines[#E.outlines+1]={id=r.id,points=Model.outline(r)} end
    end
    for _,r in ipairs(E.outlines) do
        draw(r.points,true,E.selection[r.id])
        if r.id==E.selected then
            local region=Model.region(E.project,r.id)
            if region.shape=='polygon' then for _,p in ipairs(r.points) do handle(p.x,p.y) end
            else handle(region.x+region.w-1,region.y+region.h-1) end
        end
    end
    local d=E.drag
    if d and d.mode=='create' then
        local r={shape=E.tool,x=math.min(d.x,d.cx),y=math.min(d.y,d.cy),w=math.abs(d.cx-d.x)+1,h=math.abs(d.cy-d.y)+1}
        draw(Model.outline(r),true,true)
    end
    if #E.polygon>0 then
        local points=Model.copy(E.polygon)
        if E.cursor then points[#points+1]=E.cursor end
        draw(points,false,true)
    end
end
function M.input(E,H,event,mx,my)
    if not E.editMode or not E.texture or not E.canvasTransform then return end
    local origin=M.transform(E); local scale=origin.scale
    local hovered=mx>=origin.x and my>=origin.y and mx<origin.x+E.project.image.width*scale and my<origin.y+E.project.image.height*scale
    local x=clamp((mx-origin.x)/scale,0,E.project.image.width-1)
    local y=clamp((my-origin.y)/scale,0,E.project.image.height-1)
    local imagePoints=E.outlines or {}
    E.cursor={x=x,y=y}
    if E.drag or #E.polygon>0 or event=='down' then E.canvasDirty=true end
    if hovered and event=='down' then
        if E.tool=='polygon' then
            local last=E.polygon[#E.polygon]
            if #E.polygon<128 and (not last or (last.x-x)^2+(last.y-y)^2>1) then E.polygon[#E.polygon+1]={x=x,y=y} end
        elseif E.tool=='rectangle' or E.tool=='ellipse' then
            E.drag={mode='create',x=x,y=y,cx=x,cy=y}
        else
            local selected=Model.region(E.project,E.selected)
            local mode,index,id
            if selected then
                if selected.shape=='polygon' then
                    for i,p in ipairs(Model.outline(selected)) do if ((p.x-x)^2+(p.y-y)^2)*scale*scale<81 then mode='point';index=i;id=selected.id;break end end
                elseif ((selected.x+selected.w-1-x)^2+(selected.y+selected.h-1-y)^2)*scale*scale<100 then mode='resize';id=selected.id end
            end
            if not id then for i=#imagePoints,1,-1 do local r=imagePoints[i]
                if Model.contains(r.points,x,y) then id=r.id; mode='move'; break end
            end end
            if id then
                H.select(id,E.control)
                if not E.control then E.drag={mode=mode,index=index,id=id,x=x,y=y,cx=x,cy=y,before=Model.copy(E.project),region=Model.copy(Model.region(E.project,id))} end
            end
        end
    end
    local d=E.drag
    if d and event=='move' and (d.cx~=x or d.cy~=y) then
        d.cx=x; d.cy=y
        if d.mode~='create' then
            local r=Model.region(E.project,d.id); local before=d.region
            if d.mode=='move' then
                r.x=clamp(math.floor(before.x+x-d.x+0.5),0,E.project.image.width-r.w)
                r.y=clamp(math.floor(before.y+y-d.y+0.5),0,E.project.image.height-r.h)
            elseif d.mode=='resize' then
                r.w=clamp(math.floor(x-r.x+1.5),1,E.project.image.width-r.x)
                r.h=clamp(math.floor(y-r.y+1.5),1,E.project.image.height-r.y)
            else r.contour[d.index]={x=clamp((x-r.x)/math.max(1,r.w-1),0,1),y=clamp((y-r.y)/math.max(1,r.h-1),0,1)} end
            E.outlines=nil; d.changed=true
        end
    end
    if d and event=='up' then
        E.drag=nil
        if d.mode=='create' then
            if math.abs(d.cx-d.x)>=2 and math.abs(d.cy-d.y)>=2 then
                H.action(function(p)
                    local x0,y0=math.floor(math.min(d.x,d.cx)),math.floor(math.min(d.y,d.cy))
                    local r=Model.add(p,E.tool,x0,y0,math.floor(math.max(d.x,d.cx))-x0+1,math.floor(math.max(d.y,d.cy))-y0+1)
                    E.selected=r.id; E.selection={[r.id]=true}
                end)
            end
        elseif d.changed then H.commitDrag(d.before) end
    end
end
return M
