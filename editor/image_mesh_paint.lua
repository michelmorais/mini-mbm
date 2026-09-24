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
local Canvas=require 'image_mesh_canvas'
local M={}
local modes={'raise','lower','flatten','smooth'}
local function L(k) return tLang.L('ime_paint_'..k) end
function M.state(E)
    E.paint=E.paint or {enabled=false,mode=1,radius=0.08,strength=0.15,height=0.5}
    return E.paint
end
function M.cancel(E) E.paintDrag=nil end
function M.destroy(E)
    M.cancel(E)
    if E.paint and E.paint.cursor then E.paint.cursor:destroy(); E.paint.cursor=nil end
end
local function point(E,x,y)
    local t=Canvas.transform(E)
    return (x-t.x)/t.scale,(y-t.y)/t.scaleY
end
local function inside(r,x,y,outline)
    return x>=r.x and x<=r.x+r.w-1 and y>=r.y and y<=r.y+r.h-1 and
        (r.shape=='rectangle' or Model.contains(outline or Model.outline(r,96),x,y))
end
local function dab(E,x,y)
    local drag=E.paintDrag; local r=drag.region
    if not inside(r,x,y,drag.outline) then return end
    if #drag.dabs+#(r.heightEdits or {})>=4096 then E.status=tLang.L('ime_paint_limit'); return end
    local settings=drag.settings
    drag.dabs[#drag.dabs+1]={x=(x-r.x)/math.max(1,r.w-1),y=(y-r.y)/math.max(1,r.h-1),
        mode=modes[settings.mode],radius=settings.radius,strength=settings.strength,height=settings.height}
end
function M.input(E,action,kind,sx,sy)
    local settings=M.state(E)
    if E.values and E.values.heightSource=='curved' and (not E.values.curvedPainting or E.values.curvedFaceted) then return false end
    if not settings.enabled or not E.editMode or E.editDefaults then return false end
    local r=Model.region(E.project,E.selected)
    if not r or r.locked then return false end
    local x,y=point(E,sx,sy)
    settings.x,settings.y=x,y
    settings.hover=inside(r,x,y)
    if kind=='down' then
        if not settings.hover then return false end
        E.paintDrag={region=r,outline=Model.outline(r,96),settings={mode=settings.mode,radius=settings.radius,
            strength=settings.strength,height=settings.height},dabs={},x=x,y=y}
        dab(E,x,y)
    elseif E.paintDrag then
        local drag=E.paintDrag
        local dx,dy=x-drag.x,y-drag.y
        local distance=math.sqrt(dx*dx+dy*dy)
        local step=math.max(0.5,drag.settings.radius*math.max(1,math.min(r.w,r.h)-1)*0.25)
        -- Fixed spacing avoids dependence on event frequency. Cap work for pathological input jumps.
        local count=math.min(4096,math.floor(distance/step))
        if distance>0 then
            for i=1,count do dab(E,drag.x+dx/distance*step*i,drag.y+dy/distance*step*i) end
            if count>0 then drag.x,drag.y=drag.x+dx/distance*step*count,drag.y+dy/distance*step*count end
        end
        if kind=='up' then
            E.paintDrag=nil
            if #drag.dabs>0 then
                action(function(project)
                    local region=assert(Model.region(project,r.id))
                    region.heightEdits=region.heightEdits or {}
                    for _,d in ipairs(drag.dabs) do region.heightEdits[#region.heightEdits+1]=d end
                end)
                E.heightRequested=true
            end
        end
    end
    return true
end
function M.panel(E,action,apply)
    if not E.draft or E.editDefaults or not E.editMode then return end
    if not tImGui.CollapsingHeader(L('title')) then return end
    local settings=M.state(E)
    local curved=E.values.heightSource=='curved'
    if curved then
        if E.values.curvedFaceted then settings.enabled=false;tImGui.TextWrapped(L('curved_faceted'));return end
        local active=tImGui.Checkbox(L('curved_enabled'),E.values.curvedPainting)
        if active~=E.values.curvedPainting then
            M.cancel(E);settings.enabled=false
            local previous=E.values.curvedPainting
            E.values.curvedPainting=active
            if not apply() then E.values.curvedPainting=previous;return end
            E.heightRequested=true
        end
        tImGui.TextWrapped(L('curved_help'))
        if not E.values.curvedPainting then settings.enabled=false;return end
        local region=Model.region(E.project,E.selected)
        local c=settings.curvedRange
        if not c or c.edge~=E.values.curvedEdge or c.target~=E.values.curvedTarget or c.nodes~=region.curvedNodes then
            local lo,hi=Model.curved.range({curvedEdge=E.values.curvedEdge,curvedTarget=E.values.curvedTarget,curvedNodes=region.curvedNodes})
            c={edge=E.values.curvedEdge,target=E.values.curvedTarget,nodes=region.curvedNodes,lo=lo,hi=hi}
            settings.curvedRange=c
        end
        tImGui.Text(string.format(L('curved_range'),c.lo,c.hi))
        if c.hi==c.lo then settings.enabled=false;tImGui.TextWrapped(L('curved_flat'));return end
    end
    local enabled=tImGui.Checkbox(L('enabled'),settings.enabled)
    if enabled~=settings.enabled then
        M.cancel(E)
        if enabled then
            if not apply() then return end
            E.heightView=2; E.heightRequested=true; E.tool='select'
        end
        settings.enabled=enabled
    end
    local edited,value=tImGui.Combo(L('brush'),settings.mode,{L('raise'),L('lower'),L('flatten'),L('smooth')})
    if edited then settings.mode=value end
    local region=Model.region(E.project,E.selected)
    local size=math.max(1,math.min(region.w,region.h)-1)
    local radiusChanged,radius=tImGui.SliderFloat(L('radius'),math.max(0.5,settings.radius*size),math.max(0.5,size*0.001),size,'%.1f')
    if radiusChanged then settings.radius=Model.clampNumber(radius/size,0.001,1,settings.radius) end
    for _,item in ipairs({{'strength',0.01,1},{'height',0,1}}) do
        local key=item[1]
        if key~='height' or settings.mode==3 then
            local changed,v=tImGui.SliderFloat(L(key),settings[key],item[2],item[3],'%.3f')
            if changed then settings[key]=Model.clampNumber(v,item[2],item[3],settings[key]) end
        end
    end
    local r=Model.region(E.project,E.selected)
    tImGui.Text(string.format(L('count'),#(r.heightEdits or {})))
    if not curved then tImGui.TextWrapped(L('help')) end
    if #(r.heightEdits or {})>0 and tImGui.Button(L('clear')) then
        action(function(p) Model.region(p,E.selected).heightEdits=nil end)
    end
end
function M.sync(E)
    local p=M.state(E)
    local allowed=not E.values or E.values.heightSource~='curved' or (E.values.curvedPainting and not E.values.curvedFaceted)
    local visible=allowed and p.enabled and p.hover and E.editMode and not E.editDefaults and E.selected~=0 and not E.panDrag and not tImGui.GetWantCaptureMouse()
    local r=visible and Model.region(E.project,E.selected)
    if not r or r.locked then if p.cursor then p.cursor.visible=false end; return end
    if not p.cursor then
        local vertices={}
        for i=0,64 do local a=i*math.pi/32;vertices[#vertices+1]=math.cos(a);vertices[#vertices+1]=math.sin(a) end
        p.cursor=line:new('2dw',0,0,-0.5);p.cursor:add(vertices);p.cursor:setColor(1,1,0.2)
    end
    local radius=math.max(0.5,p.radius*math.max(1,math.min(r.w,r.h)-1))*E.zoom
    local x,y=(p.x-E.project.image.width/2)*E.zoom,(E.project.image.height/2-p.y)*E.zoom
    if p.cx~=x or p.cy~=y or p.cr~=radius then
        p.cursor:setPos(x,y,-0.5);p.cursor:setScale(radius,radius);p.cx,p.cy,p.cr=x,y,radius
    end
    p.cursor.visible=true
end
return M
