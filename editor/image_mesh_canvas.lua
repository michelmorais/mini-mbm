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
function M.invalidate(E) E.overlay=nil end
function M.draw(E,H)
    if not E.texture then tImGui.Text(tLang.L('ime_open_help')); return end
    local available=tImGui.GetContentRegionAvail()
    local scale=math.min(math.max(100,available.x)/E.project.image.width,math.max(100,available.y)/E.project.image.height)*E.zoom
    local size={x=E.project.image.width*scale,y=E.project.image.height*scale}
    if E.checker then local start=tImGui.GetCursorScreenPos(); tImGui.AddImage(E.checker,start,{x=start.x+size.x,y=start.y+size.y}) end
    tImGui.Image(E.texture,size)
    local origin=tImGui.GetItemRectMin(); local mouse=tImGui.GetMousePos(); local hovered=tImGui.IsItemHovered(0)
    E.canvasTransform=E.canvasTransform or {}
    E.canvasTransform.x=origin.x; E.canvasTransform.y=origin.y; E.canvasTransform.scale=scale
    local x=clamp((mouse.x-origin.x)/scale,0,E.project.image.width-1)
    local y=clamp((mouse.y-origin.y)/scale,0,E.project.image.height-1)
    local imagePoints=E.outlines
    if not imagePoints then
        imagePoints={}; for _,r in ipairs(E.project.regions) do imagePoints[#imagePoints+1]={id=r.id,points=Model.outline(r)} end
        E.outlines=imagePoints
    end
    if not E.overlay or E.overlay.x~=origin.x or E.overlay.y~=origin.y or E.overlay.scale~=scale then
        E.overlay={x=origin.x,y=origin.y,scale=scale,regions={}}
        for _,r in ipairs(imagePoints) do local points={}
            for _,p in ipairs(r.points) do points[#points+1]={x=origin.x+p.x*scale,y=origin.y+p.y*scale} end
            E.overlay.regions[#E.overlay.regions+1]={id=r.id,points=points}
        end
    end
    for _,r in ipairs(E.overlay.regions) do
        local color=E.selection[r.id] and {r=1,g=0.7,b=0.1,a=1} or {r=0.2,g=0.8,b=1,a=0.9}
        tImGui.AddPolyline(r.points,color,true,2)
        if r.id==E.selected then
            local region=Model.region(E.project,r.id)
            if region.shape=='polygon' then
                for _,point in ipairs(r.points) do tImGui.AddCircleFilled(point,4,color,12) end
            else local p=r.points[1]; local q={x=origin.x+(region.x+region.w-1)*scale,y=origin.y+(region.y+region.h-1)*scale}
                tImGui.AddRectFilled({x=q.x-4,y=q.y-4},{x=q.x+4,y=q.y+4},color)
            end
        end
    end
    if hovered and tImGui.IsMouseClicked(0,false) then
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
    if d and tImGui.IsMouseDown(0) and (d.cx~=x or d.cy~=y) then
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
            E.outlines=nil; E.overlay=nil; d.changed=true
        end
    end
    if d and d.mode=='create' then
        tImGui.AddRect({x=origin.x+math.min(d.x,d.cx)*scale,y=origin.y+math.min(d.y,d.cy)*scale},
            {x=origin.x+math.max(d.x,d.cx)*scale,y=origin.y+math.max(d.y,d.cy)*scale},{r=1,g=0.7,b=0.1,a=1})
    end
    if d and not tImGui.IsMouseDown(0) then
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
    if #E.polygon>0 then
        local points={}; for _,p in ipairs(E.polygon) do points[#points+1]={x=origin.x+p.x*scale,y=origin.y+p.y*scale} end
        points[#points+1]={x=origin.x+x*scale,y=origin.y+y*scale}
        tImGui.AddPolyline(points,{r=1,g=0.6,b=0.1,a=1},false,2)
    end
end
return M
