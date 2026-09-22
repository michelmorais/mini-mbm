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
local function clamp(v,lo,hi) return math.max(lo,math.min(hi,v)) end
function M.available(E)
    if E.editDefaults or not E.draft then return false end
    return Model.backMode(E.project,E.draft)=='remap'
end
function M.region(E,r)
    local mirror=r.overrides.backMirror
    if mirror==nil then mirror=E.project.defaults.backMirror end
    return Model.backRegion(r,mirror)
end
function M.panel(E,apply,dpCall)
    if not tImGui.CollapsingHeader(L('back_group')) then return end
    local v=E.values
    local index=1
    if v.backExternal then index=6 elseif v.backSolid then index=5 elseif v.backRelief then index=2 elseif v.backOpen then index=3 elseif v.backRemap then index=4 end
    local changed,choice=tImGui.Combo(L('back_geometry'),index,{L('back_flat'),L('back_copy'),L('back_open'),L('back_remap'),L('back_solid'),L('back_external')})
    if changed then
        v.backRelief=choice==2; v.backOpen=choice==3; v.backRemap=choice==4; v.backSolid=choice==5; v.backExternal=choice==6
        if choice~=4 and E.tool=='back_uv' then E.tool='select'; E.canvasDirty=true end
    end
    if not v.backOpen and not v.backSolid then v.backMirror=tImGui.Checkbox(L('back_mirror'),v.backMirror) end
    if v.backExternal then
        if tImGui.Button(L('back_choose_texture')) then dpCall(function()
            local path=mbm.openFile(v.backTexture,table.unpack(tUtil.supported_images))
            if path then v.backTexture=path end
        end) end
        tImGui.TextWrapped(v.backTexture~='' and tUtil.getShortName(v.backTexture) or L('side_no_texture'))
        if tImGui.IsItemHovered() and v.backTexture~='' then Help.tooltip(v.backTexture) end
        if v.backTexture~='' and tImGui.Button(L('side_use_source')) then v.backTexture='' end
        tImGui.TextWrapped(L('back_external_help'))
        return
    end
    if v.backSolid then
        local rgb=v.backColor
        local changed,color=tImGui.ColorEdit3(L('back_solid'),{r=((rgb>>16)&255)/255,g=((rgb>>8)&255)/255,b=(rgb&255)/255})
        if changed then
            local function byte(n) return math.floor(Model.clampNumber(n,0,1,0)*255+.5) end
            v.backColor=(byte(color.r)<<16)|(byte(color.g)<<8)|byte(color.b)
        end
        tImGui.TextWrapped(L('back_solid_help'))
        return
    end
    if v.backRemap and not E.editDefaults then
        local d=E.draft
        d.backCrop=d.backCrop or {x=d.x,y=d.y,w=d.w,h=d.h}
        local crop=d.backCrop
        for _,key in ipairs({'x','y','w','h'}) do
            local horizontal=key=='x' or key=='w'
            local size=horizontal and E.project.image.width or E.project.image.height
            local position=key=='x' or key=='y'
            local limit=position and size-crop[horizontal and 'w' or 'h'] or size-crop[horizontal and 'x' or 'y']
            local c,n=tImGui.InputInt(L('crop_'..key)..'##back',crop[key],1,10)
            if c then crop[key]=Model.clampNumber(n,position and 0 or 1,limit,crop[key],true) end
        end
        if E.editMode then
            local editing=tImGui.Checkbox(L('back_edit'),E.tool=='back_uv')
            if editing~=(E.tool=='back_uv') then
                if not editing then E.tool='select'; E.canvasDirty=true
                elseif apply() then
                    if E.paint then E.paint.enabled=false end
                    E.tool='back_uv'; E.heightView=1; E.canvasDirty=true
                end
            end
        end
        if tImGui.Button(L('back_reset_crop')) then
            d=E.draft; d.backCrop={x=d.x,y=d.y,w=d.w,h=d.h}
        end
        tImGui.TextWrapped(L('back_remap_help'))
    else tImGui.TextWrapped(L(v.backOpen and 'back_open_help' or 'back_help')) end
end
function M.input(E,H,event,mx,my,origin,radius)
    if not M.available(E) then return false end
    local r=Model.region(E.project,E.selected)
    if not r then return false end
    local crop=M.region(E,r)
    local x=(mx-origin.x)/origin.scale
    local y=(my-origin.y)/origin.scaleY
    local near=math.abs((crop.x+crop.w-1-x)*origin.scale)<=radius+6 and
        math.abs((crop.y+crop.h-1-y)*origin.scaleY)<=radius+6
    local inside=Model.contains(Model.outline(crop),x,y)
    if event=='down' and (near or inside) then
        E.drag={mode=near and 'back_resize' or 'back_move',id=r.id,x=x,y=y,
            before=Model.copy(E.project),region=crop}
        E.canvasDirty=true
        return true
    end
    local d=E.drag
    if not d or (d.mode~='back_resize' and d.mode~='back_move') then return false end
    if event=='move' then
        local before=d.region
        local nextCrop={x=before.x,y=before.y,w=before.w,h=before.h}
        if d.mode=='back_move' then
            nextCrop.x=clamp(math.floor(before.x+x-d.x+.5),0,E.project.image.width-before.w)
            nextCrop.y=clamp(math.floor(before.y+y-d.y+.5),0,E.project.image.height-before.h)
        else
            nextCrop.w=clamp(math.floor(before.w+x-d.x+.5),1,E.project.image.width-before.x)
            nextCrop.h=clamp(math.floor(before.h+y-d.y+.5),1,E.project.image.height-before.y)
        end
        local current=r.backCrop or r
        if nextCrop.x~=current.x or nextCrop.y~=current.y or nextCrop.w~=current.w or nextCrop.h~=current.h then
            r.backCrop=nextCrop; d.changed=true; E.canvasDirty=true
        end
    elseif event=='up' then
        E.drag=nil
        if d.changed then H.commitDrag(d.before) end
        E.canvasDirty=true
    end
    return true
end
return M
