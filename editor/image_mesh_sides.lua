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
local Help=require 'image_mesh_help'
local M={}
local function L(key) return tLang.L('ime_'..key) end
function M.available(E)
    if E.editDefaults or not E.draft then return false end
    return (E.draft.overrides.sideMode or E.project.defaults.sideMode or 'edge')=='band'
end
function M.contour(E)
    if not E.draft or (E.drag and E.drag.mode~='side_inset') then return end
    local cached=E.sideContour
    local region=cached and cached.region
    if not cached or cached.project~=E.project or cached.revision~=E.revision or cached.id~=E.selected then
        region=Model.region(E.project,E.selected)
        if not region then return end
        local options=Model.options(E.project,region);options.sideInset=1
        local first,limit,maximum=mbm.getImageMeshSideContour(options)
        cached={project=E.project,region=region,revision=E.revision,id=E.selected,perpendicular=options.sideBandPerpendicular,maximum=first and limit or maximum,error=not first and limit}
        if first then
            options.sideInset=limit
            local last,err=mbm.getImageMeshSideContour(options)
            if last then cached.first=first;cached.last=last else cached.error=err end
        end
        cached.handleIndex=1
        if cached.perpendicular and cached.first and cached.last then
            local longest=0
            for i,a in ipairs(cached.first) do
                local b=cached.last[i]
                local length=((b.x-a.x)*(region.w-1))^2+((b.y-a.y)*(region.h-1))^2
                if length>longest then longest=length;cached.handleIndex=i end
            end
        end
        E.sideContour=cached;E.sideQueries=(E.sideQueries or 0)+1
    end
    local width=region.overrides.sideInset or E.project.defaults.sideInset or 1
    if cached.first and cached.last and cached.width~=width then
        cached.width=width;cached.points={}
        if cached.perpendicular then
            local options=Model.options(E.project,region);options.sideInset=math.min(width,cached.maximum)
            cached.points=mbm.getImageMeshSideContour(options)
            E.sideQueries=(E.sideQueries or 0)+1
            return cached,region
        end
        local fraction=0
        if cached.maximum>1 then fraction=(math.min(width,cached.maximum)-1)/(cached.maximum-1) end
        for i,a in ipairs(cached.first) do local b=cached.last[i]
            cached.points[i]={x=a.x+(b.x-a.x)*fraction,y=a.y+(b.y-a.y)*fraction}
        end
    end
    return cached,region
end
function M.panel(E,apply,dpCall)
    if not tImGui.CollapsingHeader(L('side_group')) then return end
    local values=E.values
    local modes={'edge','color','repeat','band'}
    local index=1;for i,m in ipairs(modes) do if m==values.sideMode then index=i end end
    local changed,choice=tImGui.Combo(L('side_mode'),index,{L('side_edge'),L('side_color'),L('side_repeat'),L('side_band')})
    if changed then
        values.sideMode=modes[choice]
        if values.sideMode~='band' and E.tool=='side_band' then E.tool='select';E.canvasDirty=true end
    end
    if values.sideMode=='color' then
        local rgb=values.sideColor
        local c,color=tImGui.ColorEdit3(L('side_color'),{r=((rgb>>16)&255)/255,g=((rgb>>8)&255)/255,b=(rgb&255)/255})
        if c then
            local function byte(v) return math.floor(Model.clampNumber(v,0,1,0)*255+.5) end
            values.sideColor=(byte(color.r)<<16)|(byte(color.g)<<8)|byte(color.b)
        end
    elseif values.sideMode=='repeat' then
        if tImGui.Button(L('side_choose_texture')) then dpCall(function()
            local path=mbm.openFile(values.sideTexture,table.unpack(tUtil.supported_images))
            if path then values.sideTexture=path end
        end) end
        tImGui.TextWrapped(values.sideTexture~='' and tUtil.getShortName(values.sideTexture) or L('side_no_texture'))
        if tImGui.IsItemHovered() and values.sideTexture~='' then Help.tooltip(values.sideTexture) end
        if values.sideTexture~='' and tImGui.Button(L('side_use_source')) then values.sideTexture='' end
        for _,key in ipairs({'sideRepeatU','sideRepeatV'}) do
            local c,v=tImGui.InputFloat(L(key),values[key],.1,1,'%.2f')
            if c then values[key]=Model.clampOption(key,v,values[key]) end
        end
        tImGui.TextWrapped(L('side_repeat_help'))
    elseif values.sideMode=='band' then
        local changed,choice=tImGui.Combo(L('side_band_mapping'),values.sideBandPerpendicular and 2 or 1,
            {L('side_band_contour_mapping'),L('side_band_perpendicular')})
        if changed then values.sideBandPerpendicular=choice==2 end
        if tImGui.IsItemHovered() then Help.tooltip(L('side_band_perpendicular_help')) end
        values.sideBandInvert=tImGui.Checkbox(L('side_band_invert'),values.sideBandInvert or false)
        if tImGui.IsItemHovered() then Help.tooltip(L('side_band_invert_help')) end
        local cached
        if not E.editDefaults then cached=M.contour(E) end
        local maximum=cached and cached.maximum or 1000000
        local c,width=tImGui.InputFloat(L('side_inset'),values.sideInset,1,5,'%.2f')
        if c then values.sideInset=Model.clampNumber(width,1,math.max(1,maximum),values.sideInset) end
        if cached then
            tImGui.Text(string.format(L('side_limit'),maximum))
            if cached.error then tImGui.TextWrapped(cached.error) end
        end
        if E.editMode and not E.editDefaults and cached and cached.first and cached.last then
            local enabled=tImGui.Checkbox(L('side_edit'),E.tool=='side_band')
            if enabled~=(E.tool=='side_band') then
                if not enabled then E.tool='select';E.canvasDirty=true
                elseif apply() then
                    if E.paint then E.paint.enabled=false end
                    E.tool='side_band';E.heightView=1;E.canvasDirty=true
                end
            end
        end
        tImGui.TextWrapped(L(values.sideBandPerpendicular and 'side_band_perpendicular_help' or 'side_band_help'))
    end
end
function M.input(E,H,event,mx,my,origin,radius)
    if not M.available(E) then return false end
    local cached,region=M.contour(E)
    if not cached or not cached.points or cached.maximum<=1 then return false end
    local point=cached.points[cached.handleIndex or 1]
    local hx=region.x+point.x*(region.w-1);local hy=region.y+point.y*(region.h-1)
    local x=(mx-origin.x)/origin.scale;local y=(my-origin.y)/origin.scaleY
    local near=math.abs(x-hx)*origin.scale<=radius+6 and math.abs(y-hy)*origin.scaleY<=radius+6
    if event=='down' and near then
        local a,b=cached.first[cached.handleIndex or 1],cached.last[cached.handleIndex or 1]
        E.drag={mode='side_inset',before=Model.copy(E.project),id=region.id,x=x,y=y,width=cached.width,
            dx=(b.x-a.x)*(region.w-1)/(cached.maximum-1),dy=(b.y-a.y)*(region.h-1)/(cached.maximum-1),
            maximum=cached.maximum}
        local d=E.drag
        if cached.perpendicular then
            local length=math.sqrt(d.dx*d.dx+d.dy*d.dy)
            if length<1e-8 then E.drag=nil;return false end
            d.dx=d.dx/length;d.dy=d.dy/length
        end
        return true
    end
    local d=E.drag
    if not d or d.mode~='side_inset' then return false end
    if event=='move' then
        local delta=((x-d.x)*d.dx+(y-d.y)*d.dy)/(d.dx*d.dx+d.dy*d.dy)
        local width=math.max(1,math.min(d.maximum,d.width+delta))
        if math.abs(width-(region.overrides.sideInset or E.project.defaults.sideInset or 1))>.0001 then
            region.overrides.sideInset=width;d.changed=true;E.canvasDirty=true
        end
    elseif event=='up' then
        E.drag=nil
        if d.changed then H.commitDrag(d.before) end
        E.canvasDirty=true
    end
    return true
end
return M
