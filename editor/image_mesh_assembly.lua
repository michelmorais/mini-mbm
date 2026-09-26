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

local Asset=require 'image_mesh_asset'
local Wire=require 'image_mesh_wireframe'
local Model=require 'image_mesh_model'
local TextureAliases=require 'image_mesh_texture_aliases'
local M={}
local function L(k) return tLang.L('ime_assembly_'..k) end
function M.state(E)
    if not E.assembly then E.assembly={enabled=false,columns=3,gapX=0,gapY=0,slots={},items={}} end
    return E.assembly
end
local function slots(E)
    local a=M.state(E)
    if a.order then return a end
    a.order={};a.nextSlot=0;a.configRevision=0
    for i,r in ipairs(E.project.regions) do
        a.order[#a.order+1]=r.id;a.nextSlot=math.max(a.nextSlot,r.id)
        a.slots[r.id]={regionId=r.id,column=(i-1)%a.columns,row=math.floor((i-1)/a.columns),z=0,visible=true}
    end
    a.selectedSlot=a.slots[E.selected] and E.selected or a.order[1]
    return a
end
local function invalidate(E,a)
    a.configRevision=a.configRevision+1;E.dirty=true;M.sync(E)
end
function M.add(E,regionId)
    if E.meshTask or not Model.region(E.project,regionId) then return false end
    local a=slots(E);a.nextSlot=a.nextSlot+1
    local index=#a.order
    a.slots[a.nextSlot]={regionId=regionId,column=index%a.columns,row=math.floor(index/a.columns),z=0,visible=true}
    a.order[#a.order+1]=a.nextSlot;a.selectedSlot=a.nextSlot
    invalidate(E,a);return true
end
function M.choose(E,id,regionId)
    if E.meshTask or not Model.region(E.project,regionId) then return false end
    local a=slots(E);local slot=a.slots[id]
    if not slot then return false end
    if slot.regionId~=regionId then slot.regionId=regionId;invalidate(E,a) end
    return true
end
function M.remove(E,id)
    if E.meshTask then return false end
    local a=slots(E)
    for i,key in ipairs(a.order) do if key==id then
        table.remove(a.order,i);a.slots[id]=nil;a.selectedSlot=a.order[math.min(i,#a.order)]
        invalidate(E,a);return true
    end end
    return false
end
local function releaseItems(items)
    for _,item in ipairs(items) do
        Wire.release(item)
        if item.preview then meshDebug:loadMeshPreview(item.preview,nil);item.preview:destroy() end
        if item.previewPath then os.remove(item.previewPath) end
    end
end
function M.release(E)
    local a=E.assembly;if not a then return end
    releaseItems(a.items)
    if a.pending then releaseItems(a.pending.items);a.pending=nil end
    a.items={};a.revision=nil
end
function M.sync(E)
    local a=E.assembly;if not a then return end
    if a.enabled then E.previewStale=#a.items>0 and (a.revision~=E.revision or a.builtConfig~=a.configRevision) or nil end
    for _,item in ipairs(a.items) do
        local visible=a.enabled and not E.editMode and item.slot.visible
        item.preview.visible=visible and not E.wireframe
        if item.wireObject then item.wireObject.visible=visible and E.wireframe end
    end
end
function M.layout(E)
    local a=M.state(E);local w,h=0,0
    for _,item in ipairs(a.items) do w=math.max(w,item.width);h=math.max(h,item.height) end
    local loX,hiX,loY,hiY=math.huge,-math.huge,math.huge,-math.huge
    for _,item in ipairs(a.items) do
        item.x=item.slot.column*(w+a.gapX);item.y=-item.slot.row*(h+a.gapY)
        loX=math.min(loX,item.x-item.width/2);hiX=math.max(hiX,item.x+item.width/2)
        loY=math.min(loY,item.y-item.height/2);hiY=math.max(hiY,item.y+item.height/2)
    end
    if #a.items==0 then return end
    local cx,cy=(loX+hiX)/2,(loY+hiY)/2
    local depth=0
    for _,item in ipairs(a.items) do
        item.x=item.x-cx-item.centerX;item.y=item.y-cy-item.centerY
        item.z=(item.curvedFlat and 0 or -item.depth/2)+item.slot.z
        item.preview:setPos(item.x,item.y,item.z)
        if item.wireObject then item.wireObject:setPos(item.x,item.y,item.z) end
        depth=math.max(depth,item.depth+item.relief+math.abs(item.slot.z))
    end
    E.fitDistance=math.max(hiX-loX,hiY-loY,depth)*2.7
    a.layouts=(a.layouts or 0)+1
end
function M.ensureWire(E)
    local a=M.state(E)
    for _,item in ipairs(a.items) do Wire.ensure(item) end
    M.layout(E)
end
function M.build(E,generate,dpCall,camera)
    local a=slots(E)
    for i=#a.order,1,-1 do
        local id=a.order[i]
        if not Model.region(E.project,a.slots[id].regionId) then
            a.slots[id]=nil;table.remove(a.order,i);a.configRevision=a.configRevision+1
        end
    end
    if not a.slots[a.selectedSlot] then a.selectedSlot=a.order[1] end
    E.dirty=false
    if a.revision==E.revision and a.builtConfig==a.configRevision then
        for _,item in ipairs(a.items) do if item.id==E.selected then E.report=item.report end end
        M.sync(E);return
    end
    local staged={items={},slots=Model.copy(a.slots),gapX=a.gapX,gapY=a.gapY}
    local staging={assembly=staged}
    a.pending=staged
    local statistics={}
    local assets={}
    local ok=dpCall(function()
        for _,id in ipairs(a.order) do
            local slot=staged.slots[id]
            local region=assert(Model.region(E.project,slot.regionId))
            local item={id=region.id,instanceId=id,slot=slot,previewPath=tUtil.getTemporaryFilePath('.msh')}
            staged.items[#staged.items+1]=item
            local cached=assets[region.id]
            if not cached then
                local asset,report=generate(region)
                cached={asset=asset,report=report};assets[region.id]=cached
            end
            local asset,report=cached.asset,cached.report
            item.report=report;statistics[region.id]={report=report}
            local vertices=Asset.vertices(asset)
            local x0,x1,y0,y1=math.huge,-math.huge,math.huge,-math.huge
            for _,v in ipairs(vertices) do x0=math.min(x0,v.x);x1=math.max(x1,v.x);y0=math.min(y0,v.y);y1=math.max(y1,v.y) end
            item.width=x1-x0;item.height=y1-y0;item.centerX=(x0+x1)/2;item.centerY=(y0+y1)/2
            local o=Model.options(E.project,region)
            if o.heightSource=='curved' then
                local lo,hi=Model.curved.range(o)
                item.depth=lo;item.relief=hi-lo
                item.curvedFlat=o.curvedNodes~=nil and not o.curvedSymmetric
            else item.depth=o.depth;item.relief=o.relief end
            assert(TextureAliases.savePreview(asset,item.previewPath,E.path or E.project.image.path),tLang.L('ime_export_failed'))
            item.preview=mesh:new('3d');item.preview.visible=false
            assert(meshDebug:loadMeshPreview(item.preview,item.previewPath),tLang.L('ime_preview_failed'))
            item.preview.alwaysRender=true;item.preview.visible=false
            if E.wireframe then Wire.ensure(item,asset) end
        end
        M.layout(staging)
    end)
    a.pending=nil
    if ok then
        releaseItems(a.items)
        a.items=staged.items;a.slots=staged.slots;a.revision=E.revision;a.builtConfig=a.configRevision
        a.builds=(a.builds or 0)+1;a.layouts=(a.layouts or 0)+1
        E.fitDistance=staging.fitDistance;E.statistics=statistics
        E.report=statistics[E.selected] and statistics[E.selected].report
        E.generationFailure=nil
        if a.fitPending then E.orbit.fx=0;E.orbit.fy=0;E.orbit.fz=0;E.orbit.distance=E.fitDistance or E.orbit.distance;camera();a.fitPending=false end
        E.status=L('ready')
    else
        releaseItems(staged.items)
        E.report=nil
        E.generationFailure=not E.generationCancelled and E.status or nil
    end
    M.sync(E)
end
function M.arrange(E)
    local a=M.state(E)
    for i,id in ipairs(a.order or {}) do
        local slot=a.slots[id]
        slot.column=(i-1)%a.columns;slot.row=math.floor((i-1)/a.columns)
    end
    M.layout(E);M.sync(E)
end
function M.panel(E,toggle,camera)
    if E.editMode then return end
    if not tImGui.CollapsingHeader(L('title')) then return end
    local a=M.state(E)
    local enabled=tImGui.Checkbox(L('enabled'),a.enabled)
    if enabled~=a.enabled then toggle(enabled) end
    if not a.enabled then return end
    a=slots(E)
    local changed,value=tImGui.InputInt(L('columns'),a.columns,1,1)
    if changed then a.columns=Model.clampNumber(value,1,64,a.columns,true);M.arrange(E) end
    local layout=false
    for _,axis in ipairs{'X','Y'} do
        local key='gap'..axis
        local c,v=tImGui.InputFloat(L('gap')..' '..axis,a[key],1,10,'%.2f')
        if c then a[key]=Model.clampNumber(v,0,1000000,a[key]);layout=true end
    end
    if a.choicesRevision~=E.revision or a.choicesConfig~=a.configRevision then
        a.moduleNames={};a.moduleIds={};a.instanceNames={}
        for i,r in ipairs(E.project.regions) do a.moduleNames[i]=r.name;a.moduleIds[i]=r.id end
        for i,id in ipairs(a.order) do
            local r=Model.region(E.project,a.slots[id].regionId)
            a.instanceNames[i]=i..': '..(r and r.name or '?')
        end
        a.choicesRevision=E.revision;a.choicesConfig=a.configRevision
    end
    local selected=1
    for i,id in ipairs(a.order) do if id==a.selectedSlot then selected=i;break end end
    if #a.order>0 then
        local c,v=tImGui.Combo(L('object')..'##ime_assembly_instance',selected,a.instanceNames)
        if c then a.selectedSlot=a.order[v] end
    end
    local slot=a.slots[a.selectedSlot]
    local source=1
    local sourceId=slot and slot.regionId or a.addRegion or E.selected
    for i,id in ipairs(a.moduleIds) do if id==sourceId then source=i;break end end
    if #a.moduleIds>0 then
        local c,v=tImGui.Combo(L('module')..'##ime_assembly_source',source,a.moduleNames)
        if c then
            source=v
            if slot then M.choose(E,a.selectedSlot,a.moduleIds[v]) else a.addRegion=a.moduleIds[v] end
        end
    end
    if tImGui.Button(L('add')) then M.add(E,a.moduleIds[source]) end
    if slot then
        tImGui.SameLine()
        if tImGui.Button(L('remove')) then M.remove(E,a.selectedSlot);slot=nil end
    end
    if slot then
        tImGui.Separator();tImGui.Text(L('selected'))
        for _,key in ipairs{'column','row'} do
            local c,v=tImGui.InputInt(L(key),slot[key],1,1)
            if c then slot[key]=Model.clampNumber(v,0,1024,slot[key],true);layout=true end
        end
        local c,v=tImGui.InputFloat(L('depth'),slot.z,1,10,'%.2f')
        if c then slot.z=Model.clampNumber(v,-1000000,1000000,slot.z);layout=true end
        local visible=tImGui.Checkbox(L('visible'),slot.visible)
        if visible~=slot.visible then slot.visible=visible;M.sync(E) end
    end
    if layout then M.layout(E) end
    if tImGui.Button(L('arrange')) then M.arrange(E) end
    tImGui.SameLine()
    if tImGui.Button(L('fit')) then E.orbit.fx=0;E.orbit.fy=0;E.orbit.fz=0;E.orbit.distance=E.fitDistance or E.orbit.distance;camera() end
    tImGui.TextWrapped(L('help'))
end
return M
