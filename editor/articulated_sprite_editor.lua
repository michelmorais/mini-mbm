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

tImGui = require 'ImGui'
tUtil = require 'editor_utils'
local G=require 'articulated_sprite_geometry'
local Model=require 'articulated_sprite_model'
local IO=require 'articulated_sprite_io'
local E={project=Model.new(),frame=1,image=1,selected=0,clip=1,time=0,playing=false,
    rect={x=0,y=0,w=100,h=100},kind='rectangle',budget=2,threshold=16,
    form={inner=0.5,dx=0,dy=0,linked=true,circular=true},textures={},alpha={},dirty=false,poseDirty=false,
    pose={x=0,y=0,z=0,angle=0,sx=1,sy=1},mode='setup',editContour=false,contour=1,point=1,
    status='',preview=nil,ghosts={},scale=1}
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then
        print('[articulated_sprite_editor] '..tostring(result[2]))
        E.status=tostring(result[2])
    end
    return table.unpack(result,1,result.n)
end
local function L(key) return tLang.L('ase_'..key) end
local function selected() return Model.part(E.project,E.frame,E.selected) end
local function commit()
    if E.mouseDown then E.pendingHistory=true
    else Model.commit(E.history,E.project) end
    E.dirty=true; E.changed=true
end
local function action(fn)
    local before=Model.copy(E.project)
    local ok=dpCall(fn)
    if ok then commit() else E.project=before end
end
local function button(key,fn)
    if tImGui.Button(L(key)) then dpCall(fn) end
end
local function field(key,object,name,step,min,max)
    local changed,value=tImGui.DragFloat(L(key),object[name] or 0,step or 1,min or -100000,max or 100000,'%.3f')
    if changed then object[name]=value end
    return changed
end
local function check(key,object,name)
    local changed,value=tImGui.Checkbox(L(key),object[name])
    if changed then object[name]=value end
    return changed
end
local function texture(index)
    local img=E.project.images[index]
    if not img then return end
    if not E.textures[img.path] then E.textures[img.path]=assert(mbm.loadTexture(img.path),'missing_image') end
    return E.textures[img.path]
end
local function destroyPreview()
    if E.preview then E.preview:loadEditorPreview(nil); E.preview:destroy(); E.preview=nil end
    for _,obj in ipairs(E.ghosts) do obj:loadEditorPreview(nil); obj:destroy() end
    if E.previewPath then os.remove(E.previewPath); E.previewPath=nil end
    E.ghosts={}
end
local function applyPose()
    if not E.preview then return end
    E.preview:setAnim('__frame_'..(E.previewFrame or E.frame))
    for _,obj in ipairs(E.ghosts) do obj:setAnim('__frame_'..(E.previewFrame or E.frame)); obj.visible=false end
    for _,clip in ipairs(E.project.clips) do
        E.preview:disableArticulatedAnimation(clip.name)
        for _,obj in ipairs(E.ghosts) do obj:disableArticulatedAnimation(clip.name) end
    end
    local clip=E.project.clips[E.clip]
    if E.mode~='animate' or not clip then return end
    assert(E.preview:playArticulatedAnimation(clip.name))
    E.preview:seekArticulatedAnimation(clip.name,E.time)
    if not E.playing then E.preview:pauseArticulatedAnimation(clip.name) end
    for i,obj in ipairs(E.ghosts) do
        if E.project.options.onion then
            obj.visible=true
            obj:playArticulatedAnimation(clip.name)
            obj:seekArticulatedAnimation(clip.name,math.max(0,math.min(clip.duration,E.time+(i==1 and -1 or 1)/12)))
            obj:pauseArticulatedAnimation(clip.name)
        end
    end
end
local function rebuild()
    E.dirty=false
    if #E.project.frames[E.frame].parts==0 then destroyPreview(); return end
    local path=os.tmpname()
    os.remove(path); path=path..'.spt'
    local renderProject=E.project
    E.previewFrame=E.frame
    for fi=#E.project.frames,1,-1 do
        if #E.project.frames[fi].parts==0 then
            if renderProject==E.project then renderProject=Model.copy(E.project) end
            table.remove(renderProject.frames,fi)
            if fi<E.frame then E.previewFrame=E.previewFrame-1 end
        end
    end
    if E.transient and E.mode=='animate' and selected() and E.project.clips[E.clip] then
        renderProject=Model.copy(renderProject)
        Model.key(renderProject,E.clip,E.selected,E.time,E.pose)
    end
    local ok=dpCall(IO.export,renderProject,path)
    if not ok then os.remove(path); return end
    local obj=sprite:new('2dw')
    if not obj:loadEditorPreview(path) then obj:destroy(); os.remove(path); error('preview_load_failed') end
    destroyPreview(); E.preview=obj; E.previewPath=path
    for i=1,(E.project.options.onion and 2 or 0) do
        local ghost=sprite:new('2dw')
        assert(ghost:loadEditorPreview(path),'ghost_load_failed')
        ghost:setColor(i==1 and 0.3 or 0.9,0.5,i==1 and 0.9 or 0.3,0.18)
        ghost.visible=false; E.ghosts[i]=ghost
    end
    applyPose()
end
local function loadProject(project,path)
    destroyPreview()
    E.project=project; E.path=path; E.frame=1; E.image=1; E.selected=0; E.clip=1
    E.time=0; E.playing=false; E.textures={}; E.alpha={}; E.history=Model.history(project)
    E.dirty=true; E.changed=false
end
local function addImage(path)
    path=path or mbm.openFile(nil,'*.png','*.jpg','*.bmp','*.tga')
    if not path then return end
    local info=assert(mbm.loadTexture(path),'missing_image')
    action(function()
        E.project.images[#E.project.images+1]={path=mbm.getFullPath(path) or path,
            width=info:getWidth(),height=info:getHeight()}
        E.image=#E.project.images
    end)
    E.rect={x=0,y=0,w=info:getWidth(),h=info:getHeight()}
end
local function generate(replace)
    local img=assert(E.project.images[E.image],'missing_image')
    local rings=G.form(E.kind,E.rect,E.budget,E.form)
    if E.kind=='alpha' or E.useAlpha then
        if not E.alpha[img.path] then
            local bytes,w,h=mbm.readPngAlpha(img.path)
            assert(bytes,w); assert(w==img.width and h==img.height,'alpha_dimensions_mismatch')
            E.alpha[img.path]=bytes
        end
        rings=G.alphaContours(E.alpha[img.path],img.width,img.height,E.rect,rings,E.threshold,
            E.project.options.preserveHoles)
    end
    rings=G.simplify(rings,E.tolerance or 0)
    local groups=(not replace and E.project.options.splitRegions) and G.components(rings) or {rings}
    local generated={}
    for _,group in ipairs(groups) do
        if E.kind=='alpha' or E.useAlpha then group=G.fitBudget(group,E.budget) end
        local vertices,indices=G.triangulate(group,E.budget)
        generated[#generated+1]={rings=group,vertices=vertices,indices=indices}
    end
    action(function()
        for _,g in ipairs(generated) do
            local recipe={kind=E.kind,rect=Model.copy(E.rect),budget=E.budget,form=Model.copy(E.form),
                alpha=E.useAlpha,threshold=E.threshold}
            if replace then
                local p=assert(selected(),'missing_part')
                assert(not p.imported,'imported_regenerate_requires_source_mapping')
                p.rings=g.rings; p.componentCount=#G.components(g.rings); p.vertices=g.vertices; p.indices=g.indices; p.recipe=recipe; p.image=E.image
            else
                local p=Model.addPart(E.project,E.frame,E.image,g.rings,g.vertices,g.indices,recipe)
                p.componentCount=#G.components(g.rings)
                p.name=L('part')..' '..p.id
                E.selected=p.id
            end
        end
    end)
end
local function retriangulate(p)
    assert(not p.componentCount or #G.components(p.rings)==p.componentCount,'contour_changes_components')
    local vertices,indices=G.triangulate(p.rings,p.recipe.budget or E.budget)
    p.vertices=vertices; p.indices=indices; p.manual=true
end
local function record()
    E.transient=false
    action(function() Model.key(E.project,E.clip,E.selected,E.time,E.pose) end)
end
local function menu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(L('new')) then E.pendingNew=true end
        if tImGui.MenuItem(L('open')) then
            dpCall(function() local path=mbm.openFile(E.path,'*.asprite'); if path then loadProject(IO.load(path),path) end end)
        end
        if tImGui.MenuItem(L('save')) then
            dpCall(function()
                local path=mbm.saveFile(E.path,'*.asprite')
                if path then IO.save(E.project,path); E.path=path; E.changed=false; E.status=L('saved') end
            end)
        end
        if tImGui.MenuItem(L('import')) then
            dpCall(function() local path=mbm.openFile(nil,'*.spt'); if path then loadProject(IO.import(path)) end end)
        end
        if tImGui.MenuItem(L('export')) then
            dpCall(function() local path=mbm.saveFile(nil,'*.spt'); if path then IO.export(E.project,path); E.status=L('saved') end end)
        end
        if tImGui.MenuItem(L('add_image')) then dpCall(addImage) end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(L('edit')) then
        if tImGui.MenuItem(L('undo'),'Ctrl+Z') then
            local p=Model.undo(E.history); if p then E.project=p; E.dirty=true; E.changed=true end
        end
        if tImGui.MenuItem(L('redo'),'Ctrl+Y') then
            local p=Model.redo(E.history); if p then E.project=p; E.dirty=true; E.changed=true end
        end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        check('show_source',E.project.options,'showSource')
        check('copy_images',E.project.options,'copyImages')
        if check('move_windows',E,'moveWindows') then E.flags=E.moveWindows and 0 or E.noMoveFlag end
        if tImGui.MenuItem('English') then tLang.setLanguage('en') end
        if tImGui.MenuItem('Portugues') then tLang.setLanguage('pt_br') end
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end
local function sourcePanel()
    if not E.project.options.showSource then return end
    tUtil.setInitialWindowPositionLeft(E.titles.source,0,0,340)
    local opened=tImGui.Begin(E.titles.source,true,E.flags)
    if opened then
        tImGui.PushItemWidth(135)
        button('add_image',addImage)
        for i,img in ipairs(E.project.images) do
            if tImGui.Selectable(img.path:match('[^/\\]+$')..'##image'..i,i==E.image) then E.image=i end
        end
        local img=E.project.images[E.image]
        if img then
            local avail=tImGui.GetContentRegionAvail()
            local scale=math.min((avail.x or 300)/img.width,350/img.height)
            local origin=tImGui.GetCursorScreenPos()
            tImGui.Image(texture(E.image),{x=img.width*scale,y=img.height*scale})
            local mouse=tImGui.GetMousePos()
            local x,y=(mouse.x-origin.x)/scale,(mouse.y-origin.y)/scale
            if tImGui.IsItemHovered(0) and tImGui.IsMouseClicked(0,false) then
                if E.editContour then
                    local p=selected(); local ring=p and p.rings[E.contour]
                    if ring then
                        local distance=64/(scale*scale)
                        for i,v in ipairs(ring) do
                            local d=(v.x-x)^2+(v.y-y)^2
                            if d<distance then E.point=i; distance=d; E.dragPoint=true end
                        end
                        if E.dragPoint then E.beforeDrag=Model.copy(E.project) end
                    end
                else E.selectionStart={x=x,y=y} end
            end
            if E.selectionStart and tImGui.IsMouseDown(0) then
                local a=E.selectionStart
                x=math.max(0,math.min(img.width,x)); y=math.max(0,math.min(img.height,y))
                E.rect={x=math.min(a.x,x),y=math.min(a.y,y),w=math.max(1,math.abs(x-a.x)),h=math.max(1,math.abs(y-a.y))}
            end
            if E.dragPoint and tImGui.IsMouseDown(0) then
                local p=selected(); p.rings[E.contour][E.point]={x=x,y=y}
            end
            if tImGui.IsMouseReleased(0) then
                E.selectionStart=nil
                if E.dragPoint then
                    E.dragPoint=false
                    local ok=dpCall(retriangulate,selected())
                    if ok then commit() else E.project=E.beforeDrag end
                    E.beforeDrag=nil
                end
            end
            local r=E.rect
            tImGui.AddRect({x=origin.x+r.x*scale,y=origin.y+r.y*scale},
                {x=origin.x+(r.x+r.w)*scale,y=origin.y+(r.y+r.h)*scale},{r=1,g=0.8,b=0,a=1})
            local p=selected()
            if p and p.image==E.image then
                for _,ring in ipairs(p.rings) do
                    for i,a in ipairs(ring) do
                        local b=ring[i%#ring+1]
                        tImGui.AddLine({x=origin.x+a.x*scale,y=origin.y+a.y*scale},
                            {x=origin.x+b.x*scale,y=origin.y+b.y*scale},{r=0,g=1,b=0.5,a=1},2)
                    end
                end
            end
            for _,kind in ipairs({'rectangle','circle','capsule','ring','alpha'}) do
                if tImGui.Selectable(L(kind),E.kind==kind) then E.kind=kind; E.budget=kind=='rectangle' and 2 or 12 end
            end
            local changed,value=tImGui.SliderInt(L('budget'),E.budget, E.kind=='circle' and 5 or 2,512)
            if changed then E.budget=value end
            field('simplification',E,'tolerance',0.1,0,10)
            field('x',E.rect,'x'); field('y',E.rect,'y'); field('width',E.rect,'w',1,1,100000); field('height',E.rect,'h',1,1,100000)
            if E.kind=='capsule' then
                check('linked',E.form,'linked')
                check('circular',E.form,'circular')
                field('top',E.form,'top',0.5,0.1,E.rect.h/2)
                if not E.form.linked then field('bottom',E.form,'bottom',0.5,0.1,E.rect.h/2) end
            elseif E.kind=='ring' then
                field('inner',E.form,'inner',0.01,0.01,0.99)
                field('hole_x',E.form,'dx'); field('hole_y',E.form,'dy')
            end
            check('use_alpha',E,'useAlpha')
            local change,threshold=tImGui.SliderInt(L('threshold'),E.threshold,0,254)
            if change then E.threshold=threshold end
            check('preserve_holes',E.project.options,'preserveHoles')
            check('split_regions',E.project.options,'splitRegions')
            button('generate',function() generate(false) end)
            if p and not p.imported then button('regenerate',function() generate(true) end) end
        end
        tImGui.PopItemWidth()
    end
    tImGui.End()
end
local function properties()
    tUtil.setInitialWindowPositionRight(E.titles.parts,0,0,310)
    local opened=tImGui.Begin(E.titles.parts,true,E.flags)
    if opened then
        tImGui.PushItemWidth(135)
        local changed,frame=tImGui.SliderInt(L('frame'),E.frame,1,#E.project.frames)
        if changed then E.frame=frame; E.selected=0; E.dirty=true end
        button('add_frame',function() action(function() E.project.frames[#E.project.frames+1]={parts={}}; E.frame=#E.project.frames; E.selected=0 end) end)
        local parts=E.project.frames[E.frame].parts
        for _,p in ipairs(parts) do
            if tImGui.Selectable(p.name..'##part'..p.id,E.selected==p.id) then
                E.selected=p.id; E.image=p.image; E.contour=1; E.point=1
                if p.recipe and p.recipe.rect then
                    E.kind=p.recipe.kind; E.rect=Model.copy(p.recipe.rect); E.budget=p.recipe.budget
                    E.form=Model.copy(p.recipe.form); E.useAlpha=p.recipe.alpha; E.threshold=p.recipe.threshold or 0
                end
            end
        end
        local p,index=selected()
        if p then
            local changed,name=tImGui.InputText(L('name'),p.name)
            if changed then p.name=name; commit() end
            tImGui.Text(L('triangles')..': '..#p.indices/3)
            if tImGui.BeginCombo(L('parent'),p.parent==0 and L('none') or tostring(p.parent)) then
                if tImGui.Selectable(L('none'),p.parent==0) then action(function() Model.reparent(E.project,E.frame,p.id,0) end) end
                for _,parent in ipairs(parts) do
                    if parent.id~=p.id and tImGui.Selectable(parent.name..'##parent'..parent.id,p.parent==parent.id) then
                        action(function() Model.reparent(E.project,E.frame,p.id,parent.id) end)
                    end
                end
                tImGui.EndCombo()
            end
            button('up',function() if index>1 then action(function() parts[index],parts[index-1]=parts[index-1],parts[index] end) end end)
            tImGui.SameLine()
            button('down',function() if index<#parts then action(function() parts[index],parts[index+1]=parts[index+1],parts[index] end) end end)
            button('duplicate',function() action(function() E.selected=Model.duplicate(E.project,E.frame,p.id,false).id end) end)
            button('duplicate_animation',function() action(function() E.selected=Model.duplicate(E.project,E.frame,p.id,true).id end) end)
            button('delete',function() action(function() Model.remove(E.project,E.frame,p.id,false); E.selected=0 end) end)
            button('delete_hierarchy',function() action(function() Model.remove(E.project,E.frame,p.id,true); E.selected=0 end) end)
            if E.mode=='setup' then
                local oldX,oldY=p.x,p.y
                local modified=false
                for _,entry in ipairs({{'x','x'},{'y','y'},{'z','z'},{'rotation','angle'},{'scale_x','sx'},{'scale_y','sy'}}) do
                    if field(entry[1],p,entry[2],0.1) then modified=true end
                end
                p.pivot.x=p.pivot.x+p.x-oldX; p.pivot.y=p.pivot.y+p.y-oldY
                check('move_pivot',E,'movePivot')
                if field('pivot_x',p.pivot,'x',0.1) then modified=true end
                if field('pivot_y',p.pivot,'y',0.1) then modified=true end
                if modified then commit() end
            end
            check('edit_contour',E,'editContour')
            if p.imported then
                E.point=math.max(1,math.min(E.point,#p.vertices))
                local _,v=tImGui.SliderInt(L('vertex'),E.point,1,#p.vertices); E.point=v
                local point=p.vertices[v]
                local modified=field('vertex_x',point,'x',0.1)
                if field('vertex_y',point,'y',0.1) then modified=true end
                if field('uv_u',point,'u',0.001) then modified=true end
                if field('uv_v',point,'v',0.001) then modified=true end
                if modified then commit() end
            end
            if E.editContour and #p.rings>0 then
                E.contour=math.max(1,math.min(E.contour,#p.rings))
                local _,c=tImGui.SliderInt(L('contour'),E.contour,1,#p.rings); E.contour=c
                local ring=p.rings[c]
                E.point=math.max(1,math.min(E.point,#ring))
                local _,v=tImGui.SliderInt(L('point'),E.point,1,#ring); E.point=v
                button('add_point',function() action(function()
                    local a,b=ring[v],ring[v%#ring+1]
                    table.insert(ring,v+1,{x=(a.x+b.x)/2,y=(a.y+b.y)/2}); retriangulate(p)
                end) end)
                button('remove_point',function() action(function()
                    assert(#ring>3,'minimum_three_points'); table.remove(ring,v); E.point=1; retriangulate(p)
                end) end)
            end
        end
        tImGui.PopItemWidth()
    end
    tImGui.End()
end
local function worldInput()
    local hovered=tImGui.IsAnyWindowHovered()
    if E.zoomRequest then
        if not hovered then
            local factor=math.max(0.1,math.min(10,E.camera.sx*(1-E.zoomRequest*0.1)))
            E.camera:setScale(factor,factor)
        end
        E.zoomRequest=nil
    end
    local mouse=tImGui.GetMousePos()
    if not hovered and tImGui.IsMouseClicked(0,false) then
        local x,y=mbm.to2dw(mouse.x,mouse.y)
        local parts=E.project.frames[E.frame].parts
        for i=#parts,1,-1 do
            local p=parts[i]
            local hit=false
            for j=1,#p.indices,3 do
                local triangle={}
                for k=0,2 do triangle[k+1]=Model.vertex(p,p.vertices[p.indices[j+k]],E.project.images[p.image]) end
                if G.contains(triangle,x,y) then hit=true; break end
            end
            if hit then
                E.selected=p.id; E.image=p.image
                E.worldDrag={x=x,y=y,px=p.x,py=p.y,pivotX=p.pivot.x,pivotY=p.pivot.y,
                    keyX=E.pose.x or 0,keyY=E.pose.y or 0,pivotOnly=E.movePivot}
                break
            end
        end
    end
    if E.worldDrag and tImGui.IsMouseDown(0) then
        local x,y=mbm.to2dw(mouse.x,mouse.y)
        local d,p=E.worldDrag,selected()
        if p and (x~=d.lastX or y~=d.lastY) then
            d.lastX,d.lastY=x,y
            if E.mode=='setup' then
                if not d.pivotOnly then p.x,p.y=d.px+x-d.x,d.py+y-d.y end
                p.pivot.x,p.pivot.y=d.pivotX+x-d.x,d.pivotY+y-d.y
                commit()
            else
                E.pose.x,E.pose.y=d.keyX+x-d.x,d.keyY+y-d.y
                if E.project.options.autoKey then record() else E.transient=true; E.dirty=true end
            end
        end
    end
    if tImGui.IsMouseReleased(0) then E.worldDrag=nil end
    if not hovered and tImGui.IsMouseClicked(2,false) then
        E.pan={x=mouse.x,y=mouse.y,cx=E.camera.x,cy=E.camera.y}
    end
    if E.pan and tImGui.IsMouseDown(2) then
        E.camera:setPos(E.pan.cx+(E.pan.x-mouse.x)*E.camera.sx,E.pan.cy+(mouse.y-E.pan.y)*E.camera.sy)
    else E.pan=nil end
end
local function timeline()
    tUtil.setInitialWindowPositionDown(E.titles.timeline,340,0.27,310)
    local opened=tImGui.Begin(E.titles.timeline,true,E.flags)
    if opened then
        if tImGui.Selectable(L('setup'),E.mode=='setup') then E.mode='setup'; E.playing=false; E.poseDirty=true end
        tImGui.SameLine()
        if tImGui.Selectable(L('animate'),E.mode=='animate') then E.mode='animate'; E.poseDirty=true end
        check('auto_key',E.project.options,'autoKey')
        if check('onion',E.project.options,'onion') then E.dirty=true end
        for i,clip in ipairs(E.project.clips) do
            if tImGui.Selectable(clip.name..'##clip'..i,E.clip==i) then E.clip=i; E.time=0; E.poseDirty=true end
        end
        button('add_clip',function() action(function()
            E.project.clips[#E.project.clips+1]={name=L('clip')..' '..(#E.project.clips+1),duration=1,speed=1,priority=0,loop=true,blend=0,tracks={}}
            E.clip=#E.project.clips; E.mode='animate'
        end) end)
        local clip=E.project.clips[E.clip]
        if clip then
            button('delete_clip',function() action(function()
                table.remove(E.project.clips,E.clip); E.clip=1; E.time=0; E.playing=false
            end) end)
            if E.project.clips[E.clip]~=clip then tImGui.End(); return end
            local changed,name=tImGui.InputText(L('clip_name'),clip.name)
            if changed then clip.name=name; commit() end
            if field('duration',clip,'duration',0.05,0.01,3600) then commit() end
            if field('speed',clip,'speed',0.05,0.01,100) then commit() end
            local priorityChanged,priority=tImGui.SliderInt(L('priority'),clip.priority or 0,-20,20)
            if priorityChanged then clip.priority=priority; commit() end
            local blendChanged,blend=tImGui.Checkbox(L('additive'),clip.blend==1)
            if blendChanged then clip.blend=blend and 1 or 0; commit() end
            if check('loop',clip,'loop') then commit() end
            local moved,time=tImGui.SliderFloat(L('time'),E.time,0,clip.duration)
            if moved then E.time=time; E.poseDirty=true; if E.transient then E.transient=false; E.dirty=true end end
            button(E.playing and 'pause' or 'play',function() E.playing=not E.playing; E.mode='animate'; E.poseDirty=true end)
            if selected() then
                local modified=false
                for _,entry in ipairs({{'key_x','x'},{'key_y','y'},{'key_rotation','angle'},{'key_scale_x','sx'},{'key_scale_y','sy'}}) do
                    if field(entry[1],E.pose,entry[2],0.1) then modified=true end
                end
                if modified then
                    if E.project.options.autoKey then record()
                    else E.transient=true; E.dirty=true end
                end
                local easingChanged,easing=tImGui.SliderInt(L('easing'),E.pose.easing or 0,0,5)
                if easingChanged then E.pose.easing=easing end
                tImGui.Text(L('easing_help'))
                if E.pose.easing==5 then
                    E.pose.bezier=E.pose.bezier or {0.25,0.25,0.75,0.75}
                    for i=1,4 do field('bezier_'..i,E.pose.bezier,i,0.01,0,1) end
                end
                button('record',record)
                if E.keyOrigin then
                    button('move_key',function() action(function()
                        for _,track in ipairs(clip.tracks) do
                            if track.part==E.selected then
                                for i=#track.keys,1,-1 do
                                    if math.abs(track.keys[i].time-E.keyOrigin)<0.0001 then table.remove(track.keys,i) end
                                end
                            end
                        end
                        Model.key(E.project,E.clip,E.selected,E.time,E.pose); E.keyOrigin=E.time
                    end) end)
                end
                for _,track in ipairs(clip.tracks) do
                    if track.part==E.selected then
                        for ki,k in ipairs(track.keys) do
                            if tImGui.Selectable(string.format('%.3f##key%d',k.time,ki),math.abs(k.time-E.time)<0.0001) then
                                E.time=k.time; E.keyOrigin=k.time; E.pose=Model.copy(k)
                                E.pose.angle=k.euler and k.euler[3] or k.angle or (k.q and 2*math.atan(k.q[3],k.q[4])*180/math.pi) or 0
                                E.pose.q=nil; E.pose.euler=nil; E.poseDirty=true
                            end
                        end
                        button('delete_key',function() action(function()
                            for ki=#track.keys,1,-1 do if math.abs(track.keys[ki].time-E.time)<0.0001 then table.remove(track.keys,ki) end end
                        end) end)
                    end
                end
            end
        end
        tImGui.Text(E.status)
    end
    tImGui.End()
end
function onInitScene()
    E.camera=mbm.getCamera('2d')
    E.noMoveFlag=tImGui.Flags('ImGuiWindowFlags_NoMove'); E.flags=E.noMoveFlag
    E.titles={source=L('source_title'),parts=L('parts_title'),timeline=L('timeline_title')}
    E.history=Model.history(E.project)
    E.pivotMarker=line:new('2dw',0,0,-1)
    E.pivotMarker:add({-8,0,8,0}); E.pivotMarker:add({0,-8,0,8}); E.pivotMarker:setColor(1,0.8,0)
    E.pivotMarker.visible=false
    tUtil.sMessageOverlay=L('welcome')
    tLineCenterX=line:new('2dw',0,0,50); tLineCenterX:add({-99999,0,99999,0}); tLineCenterX:setColor(1,0,0)
    tLineCenterY=line:new('2dw',0,0,50); tLineCenterY:add({0,-99999,0,99999}); tLineCenterY:setColor(0,1,0)
    local path=tUtil.createAlphaPattern(1024,768,32,{r=210,g=210,b=210},{r=160,g=160,b=160})
    if path then
        E.background=_G.texture:new('2dw')
        E.background:load(path); E.background.z=99
    end
end
function onLoop(delta)
    E.frame=math.max(1,math.min(E.frame,#E.project.frames))
    E.clip=math.max(1,math.min(E.clip,#E.project.clips))
    E.mouseDown=tImGui.IsMouseDown(0)
    menu(); sourcePanel(); properties(); timeline(); worldInput()
    if E.pendingHistory and not tImGui.IsMouseDown(0) then
        E.pendingHistory=false; Model.commit(E.history,E.project)
    end
    if E.pendingNew then
        E.pendingNew=false
        tImGui.OpenPopup(L('discard'))
    end
    local opened=tImGui.BeginPopupModal(L('discard'),false,E.flags)
    if opened then
        tImGui.Text(L('discard_help'))
        button('new',function() loadProject(Model.new()); tImGui.CloseCurrentPopup() end)
        tImGui.SameLine(); button('cancel',function() tImGui.CloseCurrentPopup() end)
        tImGui.EndPopup()
    end
    local p=selected()
    E.pivotMarker.visible=p~=nil and E.mode=='setup'
    if p and (E.markerX~=p.pivot.x or E.markerY~=p.pivot.y) then
        E.markerX,E.markerY=p.pivot.x,p.pivot.y; E.pivotMarker:setPos(E.markerX,E.markerY)
    end
    E.rebuildClock=(E.rebuildClock or 0)+delta
    if E.dirty and not E.dragPoint and (not E.mouseDown or E.rebuildClock>=0.08) then
        E.rebuildClock=0; dpCall(rebuild)
    end
    if E.poseDirty then E.poseDirty=false; dpCall(applyPose) end
    if E.playing and E.preview then
        local clip=E.project.clips[E.clip]
        if clip then
            E.time=E.preview:getArticulatedAnimationTime(clip.name) or E.time
            if E.project.options.onion then
                -- Ghost poses update at 12 Hz while playing, never through asset rebuilds.
                E.ghostClock=(E.ghostClock or 0)+delta
                if E.ghostClock>=1/12 then
                    E.ghostClock=0
                    for i,obj in ipairs(E.ghosts) do
                        obj:seekArticulatedAnimation(clip.name,math.max(0,math.min(clip.duration,E.time+(i==1 and -1 or 1)/12)))
                    end
                end
            end
        end
    end
    tUtil.showOverlayMessage()
end
function onTouchDown(key,x,y)
    E.mouse={x=x,y=y,key=key}
    E.pendingTouch=key==0
end
function onTouchMove(key,x,y)
    -- Actual world interaction is deferred to onLoop so ImGui capture is current.
    E.cursor={x=x,y=y}
end
function onTouchUp(key,x,y) E.pendingTouch=false; E.mouse=nil end
function onTouchZoom(zoom)
    E.zoomRequest=zoom
end
function onKeyDown(key)
    if key==mbm.getKeyCode('control') then E.control=true end
    if E.control and key==mbm.getKeyCode('Z') then
        local p=Model.undo(E.history); if p then E.project=p; E.dirty=true end
    elseif E.control and key==mbm.getKeyCode('Y') then
        local p=Model.redo(E.history); if p then E.project=p; E.dirty=true end
    end
end
function onEndScene() destroyPreview() end
function onKeyUp(key) if key==mbm.getKeyCode('control') then E.control=false end end
-- Exposed only to the engine smoke scene; production callbacks remain above.
return {state=E,addImage=addImage,generate=generate,rebuild=rebuild,loadProject=loadProject}
