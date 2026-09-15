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

-- A test may supply an output table via loadfile()(api). Production dofile()
-- must return nil so the launcher invokes the global scene callbacks.
local testApi = ...
tImGui = require 'ImGui'
tUtil = require 'editor_utils'
local G=require 'articulated_sprite_geometry'
local Model=require 'articulated_sprite_model'
local IO=require 'articulated_sprite_io'
local Pose=require 'articulated_sprite_pose'
local E={project=Model.new(),frame=1,image=1,selected=0,clip=1,time=0,playing=false,
    rect={x=0,y=0,w=100,h=100},kind='rectangle',budget=2,threshold=16,
    form={inner=0.5,dx=0,dy=0,linked=true,circular=true},textures={},alpha={},dirty=false,poseDirty=false,
    pose={x=0,y=0,z=0,angle=0,sx=1,sy=1},mode='setup',editContour=false,contour=1,point=1,
    status='',preview=nil,ghosts={},scale=1,showParts=true,showTimeline=true}
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then
        print('[articulated_sprite_editor] '..tostring(result[2]))
        E.status=tostring(result[2])
        tUtil.showMessageWarn(E.status,8)
    end
    return table.unpack(result,1,result.n)
end
local function L(key) return tLang.L('ase_'..key) end
local function tooltip(key)
    if tImGui.IsItemHovered(0) then tImGui.SetTooltip(L(key..'_tip')) end
end
-- Preserve every binding return value, and attach help before another widget
-- replaces ImGui's last item. Only the hovered item resolves its help text.
local function widget(method,key,...)
    local label=L(key)
    if E.sourceControls and (method=='SliderInt' or method=='DragFloat') then
        tImGui.Text(label)
        label='##'..key
    end
    local a,b=tImGui[method](label,...)
    tooltip(key)
    return a,b
end
local function refreshTitles()
    E.titles={source=L('source_title')..'###ase_sources',
        canvas=L('canvas_title')..'###ase_canvas',
        parts=L('parts_title')..'###ase_parts',
        timeline=L('timeline_title')..'###ase_timeline'}
end
local function selected() return Model.part(E.project,E.frame,E.selected) end
local function commit()
    E.geometryRevision=(E.geometryRevision or 0)+1
    if E.mouseDown then E.pendingHistory=true
    else Model.commit(E.history,E.project) end
    E.dirty=true; E.changed=true
end
local function action(fn)
    local before=Model.copy(E.project)
    local ok=dpCall(fn)
    if ok then commit() else E.project=before end
    return ok
end
local function button(key,fn)
    if widget('Button',key) then dpCall(fn) end
end
local function field(key,object,name,step,min,max)
    local changed,value=widget('DragFloat',key,object[name] or 0,step or 1,min or -100000,max or 100000,'%.3f')
    if changed then object[name]=value end
    return changed
end
local function check(key,object,name)
    local previous=not not object[name]
    local value=widget('Checkbox',key,previous)
    object[name]=value
    return value~=previous
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
    if #E.project.frames[E.frame].parts==0 then destroyPreview(); return true end
    local path=os.tmpname()
    os.remove(path); path=path..'.spt'
    local renderProject=E.project
    local previewFrame=E.frame
    for fi=#E.project.frames,1,-1 do
        if #E.project.frames[fi].parts==0 then
            if renderProject==E.project then renderProject=Model.copy(E.project) end
            Model.removeFrame(renderProject,fi)
            if fi<E.frame then previewFrame=previewFrame-1 end
        end
    end
    if E.transient and E.mode=='animate' and E.project.clips[E.clip] then
        renderProject=Model.copy(renderProject)
        for id,pose in pairs(E.drafts or {}) do
            if Model.part(E.project,E.frame,id) then Model.key(renderProject,E.clip,id,E.time,pose) end
        end
    end
    local obj,ghosts=nil,{}
    local previous,previousGhosts,previousPath,previousFrame=E.preview,E.ghosts,E.previewPath,E.previewFrame
    local ok=dpCall(function()
        IO.export(renderProject,path)
        obj=sprite:new('2dw')
        assert(obj:loadEditorPreview(path),'preview_load_failed')
        for i=1,(E.project.options.onion and 2 or 0) do
            local ghost=sprite:new('2dw')
            ghosts[i]=ghost
            assert(ghost:loadEditorPreview(path),'ghost_load_failed')
            ghost:setColor(i==1 and 0.3 or 0.9,0.5,i==1 and 0.9 or 0.3,0.18)
            ghost.visible=false
        end
        E.preview,E.ghosts,E.previewPath,E.previewFrame=obj,ghosts,path,previewFrame
        applyPose()
    end)
    if not ok then
        E.preview,E.ghosts,E.previewPath,E.previewFrame=previous,previousGhosts,previousPath,previousFrame
        if obj then obj:loadEditorPreview(nil); obj:destroy() end
        for _,ghost in ipairs(ghosts) do ghost:loadEditorPreview(nil); ghost:destroy() end
        os.remove(path)
        return false
    end
    -- Release the previous version only after every new resource and pose is ready.
    if previous then previous:loadEditorPreview(nil); previous:destroy() end
    for _,ghost in ipairs(previousGhosts) do ghost:loadEditorPreview(nil); ghost:destroy() end
    if previousPath then os.remove(previousPath) end
    return true
end
local function loadProject(project,path)
    destroyPreview()
    Pose.clear(E); E.poseContext=nil
    E.project=project; E.path=path; E.frame=1; E.image=1; E.selected=0; E.clip=1
    E.time=0; E.playing=false; E.textures={}; E.alpha={}; E.history=Model.history(project)
    E.dirty=true; E.changed=false; E.editContour=false; E.contourDrag=nil; E.overlayCache=nil
end
local function selectClip(index)
    Pose.clear(E); E.poseContext=nil
    E.clip=index; E.time=0; E.poseDirty=true; E.transient=false; E.keyOrigin=nil
    E.pose={x=0,y=0,z=0,angle=0,sx=1,sy=1}
    local clip=E.project.clips[index]
    if clip and clip.frame and E.frame~=clip.frame then
        E.frame=clip.frame; E.selected=0; E.dirty=true
    end
end
local function importScml(path)
    local project=require('articulated_sprite_scml').import(path)
    -- Validate all source textures before replacing the current editor project.
    for _,img in ipairs(project.images) do
        local info=assert(mbm.loadTexture(img.path),'missing_image: '..img.path)
        assert(info:getWidth()==img.width and info:getHeight()==img.height,'SCML image dimensions: '..img.path)
    end
    project.options.showSource=false
    loadProject(project)
    E.mode='animate'; E.changed=true
    E.status=string.format(L('scml_loaded'),#project.clips,#project.images)
    tUtil.showMessage(E.status,8)
    selectClip(1)
end
local function addImage(paths)
    paths=paths or mbm.openMultiFile(nil,'*.png','*.jpg','*.bmp','*.tga')
    if not paths then return end
    if type(paths)=='string' then paths={paths} end
    if #paths==0 then return end
    -- Load the batch before changing the project, then record one undo action.
    local images={}
    for _,path in ipairs(paths) do
        local info=assert(mbm.loadTexture(path),'missing_image: '..path)
        images[#images+1]={path=mbm.getFullPath(path) or path,
            width=info:getWidth(),height=info:getHeight()}
    end
    local ok=action(function()
        for _,img in ipairs(images) do E.project.images[#E.project.images+1]=img end
        E.image=#E.project.images
        E.project.options.showSource=true
    end)
    if ok then
        local img=images[#images]
        E.selectionStart=nil
        E.rect={x=0,y=0,w=img.width,h=img.height}
    end
end
local function retriangulate(p)
    assert(not p.componentCount or #G.components(p.rings)==p.componentCount,'contour_changes_components')
    local alphaMode=p.recipe.kind=='alpha' or p.recipe.alpha
    local vertices,indices=G.triangulate(p.rings,alphaMode and 2 or (p.recipe.budget or E.budget))
    p.vertices=vertices; p.indices=indices; p.manual=true
end
local function finishContourDrag(cancel)
    local d=E.contourDrag
    E.contourDrag=nil; E.dragPoint=false
    if not d then return end
    local point=d.part.rings[d.contour][d.point]
    if point.x==d.x and point.y==d.y then return end
    if cancel then
        E.geometryRevision=(E.geometryRevision or 0)+1
        point.x,point.y=d.x,d.y
        return
    end
    local ok=dpCall(retriangulate,d.part)
    if ok then commit() else
        point.x,point.y=d.x,d.y
        E.geometryRevision=(E.geometryRevision or 0)+1
    end
end
local function startContourDrag(p,contour,point,x,y)
    finishContourDrag(true)
    local v=p.rings[contour][point]
    E.contourDrag={part=p,contour=contour,point=point,x=v.x,y=v.y,mouseX=x,mouseY=y}
    E.dragPoint=true
end
local function moveContourPoint(x,y,scale)
    local d=E.contourDrag
    if not d then return end
    local dx,dy=x-d.mouseX,y-d.mouseY
    -- A click selects a point. Small mouse jitter must not edit geometry.
    if not d.moved and (dx*dx+dy*dy)*scale*scale<4 then return end
    d.moved=true
    local point=d.part.rings[d.contour][d.point]
    local x,y=d.x+dx,d.y+dy
    if point.x~=x or point.y~=y then
        point.x,point.y=x,y
        E.geometryRevision=(E.geometryRevision or 0)+1
    end
end
local function closeContourEditor()
    finishContourDrag(true)
    E.editContour=false; E.selectionStart=nil; E.overlayCache=nil
end
local function generate(replace)
    finishContourDrag(true)
    local img=assert(E.project.images[E.image],'missing_image')
    local alphaMode=E.kind=='alpha' or E.useAlpha
    local rings=G.form(E.kind,E.rect,alphaMode and 64 or E.budget,E.form)
    if alphaMode then
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
        local vertices,indices=G.triangulate(group,alphaMode and 2 or E.budget)
        generated[#generated+1]={rings=group,vertices=vertices,indices=indices}
    end
    local ok=action(function()
        for _,g in ipairs(generated) do
            local recipe={kind=E.kind,rect=Model.copy(E.rect),budget=E.budget,form=Model.copy(E.form),
                alpha=E.useAlpha,threshold=E.threshold}
            if replace then
                local p=assert(selected(),'missing_part')
                assert(not p.imported,'imported_regenerate_requires_source_mapping')
                p.rings=g.rings; p.componentCount=#G.components(g.rings); p.vertices=g.vertices; p.indices=g.indices; p.recipe=recipe; p.image=E.image; p.manual=false
            else
                local p=Model.addPart(E.project,E.frame,E.image,g.rings,g.vertices,g.indices,recipe)
                p.componentCount=#G.components(g.rings)
                p.name=L('part')..' '..p.id
                E.selected=p.id
            end
        end
        if replace then assert(rebuild(),E.status) end
    end)
    if ok and replace then
        E.dirty=false; E.contour=1; E.point=1; E.overlayCache=nil
    end
end
local function record()
    if E.playing then E.playing=false; E.poseDirty=true end
    if action(function() Model.key(E.project,E.clip,E.selected,E.time,E.pose) end) then
        Pose.recorded(E)
        E.status=string.format(L('key_recorded'),selected().name,E.time)
    end
end
local function menu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if widget('MenuItem','new') then E.pendingNew=true end
        if widget('MenuItem','open') then
            dpCall(function() local path=mbm.openFile(E.path,'*.asprite'); if path then loadProject(IO.load(path),path) end end)
        end
        if widget('MenuItem','save') then
            dpCall(function()
                local path=mbm.saveFile(E.path,'*.asprite')
                if path then IO.save(E.project,path); E.path=path; E.changed=false; E.status=L('saved') end
            end)
        end
        if widget('MenuItem','import') then
            dpCall(function() local path=mbm.openFile(nil,'*.spt'); if path then loadProject(IO.import(path)) end end)
        end
        if widget('MenuItem','import_scml') then
            dpCall(function() local path=mbm.openFile(nil,'*.scml'); if path then importScml(path) end end)
        end
        if widget('MenuItem','export') then
            dpCall(function() local path=mbm.saveFile(nil,'*.spt'); if path then IO.export(E.project,path); E.status=L('saved') end end)
        end
        if widget('MenuItem','add_image') then dpCall(addImage) end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(L('edit')) then
        if widget('MenuItem','undo','Ctrl+Z') then
            local p=Model.undo(E.history); if p then E.project=p; E.dirty=true; E.changed=true end
        end
        if widget('MenuItem','redo','Ctrl+Y') then
            local p=Model.redo(E.history); if p then E.project=p; E.dirty=true; E.changed=true end
        end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        check('show_source',E.project.options,'showSource')
        check('show_parts',E,'showParts')
        check('show_timeline',E,'showTimeline')
        check('copy_images',E.project.options,'copyImages')
        if check('move_windows',E,'moveWindows') then E.flags=E.moveWindows and 0 or E.noMoveFlag end
        if tImGui.BeginMenu(tLang.L('menu_language')) then
            if tImGui.MenuItem(tLang.L('lang_english'),nil,tLang.current=='en') then
                tLang.setLanguage('en'); refreshTitles()
            end
            tooltip('language')
            if tImGui.MenuItem(tLang.L('lang_portuguese_br'),nil,tLang.current=='pt_br') then
                tLang.setLanguage('pt_br'); refreshTitles()
            end
            tooltip('language')
            tImGui.EndMenu()
        end
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end
local function sourcePanel()
    tUtil.setInitialWindowPositionLeft(E.titles.source,0,0,220)
    local opened=tImGui.Begin(E.titles.source,true,E.flags)
    if opened then
        tImGui.PushItemWidth(135)
        button('add_image',addImage)
        for i,img in ipairs(E.project.images) do
            if tImGui.Selectable(img.path:match('[^/\\]+$')..'##image'..i,i==E.image) then
                E.image=i; E.project.options.showSource=true
            end
            tooltip('select_source')
        end
        tImGui.PopItemWidth()
    end
    tImGui.End()
end
local function sourceCanvas()
    if not E.project.options.showSource then closeContourEditor(); return end
    if not E.editContour then finishContourDrag(true) end
    local width,height=mbm.getRealSizeScreen()
    local first=tImGui.Flags('ImGuiCond_FirstUseEver')
    -- Reference layout from imgui.ini at 1920 x 1020: (225,23), 1383 x 775.
    -- FirstUseEver preserves the user's saved placement and subsequent resizing.
    tImGui.SetNextWindowPos({x=width*225/1920,y=math.max(tImGui.GetMainMenuBarHeight(),height*23/1020)},first)
    tImGui.SetNextWindowSize({x=width*1383/1920,y=height*775/1020},first)
    tImGui.SetNextWindowSizeConstraints({x=math.min(520,width),y=math.min(320,height)},{x=width,y=height})
    tImGui.SetNextWindowBgAlpha(1)
    local opened,closed=tImGui.Begin(E.titles.canvas,true,E.flags)
    if closed then
        E.project.options.showSource=false; closeContourEditor()
        tImGui.End(); return
    end
    if opened then
        local img=E.project.images[E.image]
        if img then
            local avail=tImGui.GetContentRegionAvail()
            local imageWidth=math.max(100,math.min(avail.x-250,(avail.y-8)*img.width/img.height))
            tImGui.BeginChild('##source_image',{x=imageWidth,y=0},false)
            local scale=math.min(imageWidth/img.width,math.max(100,avail.y-8)/img.height)
            local origin=tImGui.GetCursorScreenPos()
            tImGui.Image(texture(E.image),{x=img.width*scale,y=img.height*scale})
            tooltip('source_canvas')
            local mouse=tImGui.GetMousePos()
            local x,y=(mouse.x-origin.x)/scale,(mouse.y-origin.y)/scale
            if tImGui.IsItemHovered(0) and tImGui.IsMouseClicked(0,false) then
                if E.editContour then
                    local p=selected(); local ring=p and p.image==E.image and p.rings[E.contour]
                    if ring then
                        local distance=64/(scale*scale)
                        for i,v in ipairs(ring) do
                            local d=(v.x-x)^2+(v.y-y)^2
                            if d<distance then E.point=i; distance=d end
                        end
                        if distance<64/(scale*scale) then startContourDrag(p,E.contour,E.point,x,y) end
                    end
                else E.selectionStart={x=x,y=y} end
            end
            if E.selectionStart and tImGui.IsMouseDown(0) then
                local a=E.selectionStart
                x=math.max(0,math.min(img.width,x)); y=math.max(0,math.min(img.height,y))
                E.rect={x=math.min(a.x,x),y=math.min(a.y,y),w=math.max(1,math.abs(x-a.x)),h=math.max(1,math.abs(y-a.y))}
            end
            if E.dragPoint and tImGui.IsMouseDown(0) then
                moveContourPoint(x,y,scale)
            end
            if tImGui.IsMouseReleased(0) then
                E.selectionStart=nil
                finishContourDrag(false)
            end
            local r=E.rect
            tImGui.AddRect({x=origin.x+r.x*scale,y=origin.y+r.y*scale},
                {x=origin.x+(r.x+r.w)*scale,y=origin.y+(r.y+r.h)*scale},{r=1,g=0.8,b=0,a=1})
            local p=selected()
            if p and p.image==E.image then
                local function screen(v)
                    return {x=origin.x+v.x*scale,y=origin.y+v.y*scale}
                end
                local dragging=E.contourDrag and E.contourDrag.moved or false
                local cache=E.overlayCache
                if not cache or cache.part~=p or cache.vertices~=p.vertices or
                    cache.indices~=p.indices or cache.revision~=E.geometryRevision or
                    cache.x~=origin.x or cache.y~=origin.y or cache.scale~=scale or
                    cache.edit~=E.editContour or cache.drag~=dragging then
                    cache={part=p,vertices=p.vertices,indices=p.indices,revision=E.geometryRevision,
                        x=origin.x,y=origin.y,scale=scale,edit=E.editContour,drag=dragging}
                    cache.batch=tImGui.CreateGeometryBatch(function()
                        -- Tessellate the overlay once per geometry/view change.
                        if E.editContour and not dragging and not p.imported then
                            local edges={}
                            for j=1,#p.indices,3 do
                                for k=0,2 do
                                    local a,b=p.indices[j+k],p.indices[j+(k+1)%3]
                                    if a>b then a,b=b,a end
                                    local key=a..':'..b
                                    if not edges[key] then
                                        edges[key]=true
                                        tImGui.AddLine(screen(p.vertices[a]),screen(p.vertices[b]),{r=0,g=0.8,b=1,a=0.65},1)
                                    end
                                end
                            end
                        end
                        for _,ring in ipairs(p.rings) do
                            for i,a in ipairs(ring) do
                                local b=ring[i%#ring+1]
                                local from,to=screen(a),screen(b)
                                tImGui.AddLine(from,to,{r=0,g=0,b=0,a=1},5)
                                tImGui.AddLine(from,to,{r=0,g=1,b=0.5,a=1},2)
                            end
                        end
                        if E.editContour then
                            for _,ring in ipairs(p.rings) do
                                for _,v in ipairs(ring) do
                                    local point=screen(v)
                                    tImGui.AddCircleFilled(point,4,{r=0,g=0,b=0,a=1},12)
                                    tImGui.AddCircleFilled(point,2,{r=1,g=1,b=1,a=1},12)
                                end
                            end
                        end
                    end)
                    E.overlayCache=cache
                end
                tImGui.AddGeometryBatch(cache.batch)
                if E.editContour then
                    local ring=p.rings[E.contour]
                    local active=ring and ring[E.point]
                    if active then
                        local point=screen(active)
                        tImGui.AddCircleFilled(point,7,{r=0,g=0,b=0,a=1},12)
                        tImGui.AddCircleFilled(point,5,{r=1,g=0,b=1,a=1},12)
                    end
                end
            end
            tImGui.EndChild()
            tImGui.SameLine()
            tImGui.BeginChild('##source_controls',{x=0,y=0},false)
            E.sourceControls=true
            tImGui.PushItemWidth(120)
            tImGui.Text(L('cut_shape'))
            tImGui.PushItemWidth(math.max(120,tImGui.GetContentRegionAvail().x-8))
            local shapeOpen=tImGui.BeginCombo('##cut_shape',L(E.kind))
            tooltip('cut_shape')
            if shapeOpen then
                for _,kind in ipairs({'rectangle','circle','capsule','ring','alpha'}) do
                    if widget('Selectable',kind,E.kind==kind) then
                        E.kind=kind; E.budget=kind=='rectangle' and 2 or 12
                    end
                end
                tImGui.EndCombo()
            end
            tImGui.PopItemWidth()
            button('select_full_image',function()
                E.selectionStart=nil
                E.rect={x=0,y=0,w=img.width,h=img.height}
            end)
            check('use_alpha',E,'useAlpha')
            local alphaMode=E.kind=='alpha' or E.useAlpha
            if not alphaMode then
                local changed,value=widget('SliderInt','budget',E.budget,E.kind=='circle' and 5 or 2,512)
                if changed then E.budget=value end

            end
            field('simplification',E,'tolerance',0.1,0,10)
            field('x',E.rect,'x'); field('y',E.rect,'y'); field('width',E.rect,'w',1,1,100000); field('height',E.rect,'h',1,1,100000)
            if E.kind=='capsule' then
                check('linked',E.form,'linked')
                check('circular',E.form,'circular')
                field('top',E.form,'top',0.5,0.1,E.rect.h/2)
                if not E.form.linked then field('bottom',E.form,'bottom',0.5,0.1,E.rect.h/2) end
            elseif E.kind=='ring' then
                local innerMax=G.constrainRing(E.rect,E.form,alphaMode and 64 or E.budget)
                field('inner',E.form,'inner',0.01,0.01,innerMax)
                -- Also clamp typed values; DragFloat limits alone do not
                -- constrain all keyboard entry paths in ImGui.
                E.form.inner=math.max(0.01,math.min(innerMax,E.form.inner))
                local _,xMax=G.constrainRing(E.rect,E.form,alphaMode and 64 or E.budget)
                field('hole_x',E.form,'dx',1,-xMax,xMax)
                E.form.dx=math.max(-xMax,math.min(xMax,E.form.dx))
                local _,_,yMax=G.constrainRing(E.rect,E.form,alphaMode and 64 or E.budget)
                field('hole_y',E.form,'dy',1,-yMax,yMax)
                E.form.dy=math.max(-yMax,math.min(yMax,E.form.dy))
            end
            local change,threshold=widget('SliderInt','threshold',E.threshold,0,254)
            if change then E.threshold=threshold end
            check('preserve_holes',E.project.options,'preserveHoles')
            check('split_regions',E.project.options,'splitRegions')
            button('generate',function() generate(false) end)
            if p and not p.imported then button('regenerate',function() generate(true) end) end
            tImGui.PopItemWidth()
            E.sourceControls=false
            tImGui.EndChild()
        else
            tImGui.Text(L('welcome'))
        end
    end
    tImGui.End()
end
local function properties()
    if not E.showParts then return end
    tUtil.setInitialWindowPositionRight(E.titles.parts,0,0,310)
    local opened,closed=tImGui.Begin(E.titles.parts,true,E.flags)
    if closed then E.showParts=false end
    if opened then
        tImGui.PushItemWidth(135)
        local changed,frame=widget('SliderInt','frame',E.frame,1,#E.project.frames)
        if changed then
            E.frame=frame; E.selected=0; E.dirty=true
            for i,clip in ipairs(E.project.clips) do
                if clip.frame==frame then selectClip(i); break end
            end
        end
        button('add_frame',function() action(function() E.project.frames[#E.project.frames+1]={parts={}}; E.frame=#E.project.frames; E.selected=0 end) end)
        if #E.project.frames>1 then
            button('delete_frame',function()
                finishContourDrag(true)
                action(function()
                    Model.removeFrame(E.project,E.frame)
                    E.frame=math.min(E.frame,#E.project.frames)
                    E.selected=0; E.contour=1; E.point=1; E.overlayCache=nil
                    E.playing=false; E.time=0; E.transient=false; E.keyOrigin=nil
                    E.worldDrag=nil; E.pose={x=0,y=0,z=0,angle=0,sx=1,sy=1}
                end)
            end)
        end
        local parts=E.project.frames[E.frame].parts
        for _,p in ipairs(parts) do
            if tImGui.Selectable(p.name..'##part'..p.id,E.selected==p.id) then
                E.selected=p.id; E.image=p.image; E.contour=1; E.point=1
                if p.recipe and p.recipe.rect then
                    E.kind=p.recipe.kind; E.rect=Model.copy(p.recipe.rect); E.budget=p.recipe.budget
                    E.form=Model.copy(p.recipe.form); E.useAlpha=p.recipe.alpha; E.threshold=p.recipe.threshold or 0
                end
            end
            tooltip('select_part')
        end
        local p,index=selected()
        if p then
            local changed,name=widget('InputText','name',p.name)
            if changed then p.name=name; commit() end
            tImGui.Text(L('triangles')..': '..#p.indices/3)
            local parentOpen=tImGui.BeginCombo(L('parent'),p.parent==0 and L('none') or tostring(p.parent))
            tooltip('parent')
            if parentOpen then
                if widget('Selectable','none',p.parent==0) then action(function() Model.reparent(E.project,E.frame,p.id,0) end) end
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
                local oldX,oldY,oldAngle=p.x,p.y,p.angle
                local modified=false
                for _,entry in ipairs({{'x','x'},{'y','y'},{'z','z'},{'rotation','angle'},{'scale_x','sx'},{'scale_y','sy'}}) do
                    if field(entry[1],p,entry[2],0.1) then modified=true end
                end
                p.pivot.x=p.pivot.x+p.x-oldX; p.pivot.y=p.pivot.y+p.y-oldY
                if p.angle~=oldAngle then
                    local angle=p.angle
                    p.angle=oldAngle
                    Model.rotatePart(p,angle)
                end
                check('move_pivot',E,'movePivot')
                if field('pivot_x',p.pivot,'x',0.1) then modified=true end
                if field('pivot_y',p.pivot,'y',0.1) then modified=true end
                if modified then commit() end
            end
            if check('edit_contour',E,'editContour') and E.editContour then
                E.image=p.image
                E.project.options.showSource=true
            end
            if p.imported then
                E.point=math.max(1,math.min(E.point,#p.vertices))
                local _,v=widget('SliderInt','vertex',E.point,1,#p.vertices); E.point=v
                local point=p.vertices[v]
                local modified=field('vertex_x',point,'x',0.1)
                if field('vertex_y',point,'y',0.1) then modified=true end
                if field('uv_u',point,'u',0.001) then modified=true end
                if field('uv_v',point,'v',0.001) then modified=true end
                if modified then commit() end
            end
            if E.editContour and #p.rings>0 then
                E.contour=math.max(1,math.min(E.contour,#p.rings))
                local _,c=widget('SliderInt','contour',E.contour,1,#p.rings); E.contour=c
                local ring=p.rings[c]
                E.point=math.max(1,math.min(E.point,#ring))
                local _,v=widget('SliderInt','point',E.point,1,#ring); E.point=v
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
local function mouseWorld(mouse)
    -- ImGui reports physical window pixels. mbm.to2dw expects coordinates
    -- already scaled by the engine's input pipeline, so do not pass them raw.
    local width,height=mbm.getRealSizeScreen()
    return E.camera.x+(mouse.x-width/2)/E.camera.sx,
        E.camera.y+(height/2-mouse.y)/E.camera.sy
end
local function worldInput()
    local hovered=tImGui.IsAnyWindowHovered()
    if hovered or tImGui.IsAnyItemActive() or tImGui.GetWantCaptureMouse() then
        -- A DragFloat can keep mouse ownership after the cursor leaves its
        -- window. Cancel scene gestures instead of resuming them underneath UI.
        E.pan=nil; E.worldDrag=nil; E.zoomRequest=nil
        return
    end
    if E.zoomRequest then
        if not hovered then
            local factor=math.max(0.1,math.min(10,E.camera.sx*1.1^E.zoomRequest))
            local width,height=mbm.getRealSizeScreen()
            local x,y=E.camera.x,E.camera.y
            -- sx/sy alone are overwritten by adjustScaleScreen2d next frame.
            E.camera:scaleToScreen(width/factor,height/factor,'xy')
            E.camera:setPos(x,y)
        end
        E.zoomRequest=nil
    end
    local mouse=tImGui.GetMousePos()
    if not hovered and tImGui.IsMouseClicked(0,false) then
        E.worldDrag=nil; E.pan=nil
        local x,y=mouseWorld(mouse)
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
                Pose.sync(E)
                E.worldDrag={x=x,y=y,px=p.x,py=p.y,pivotX=p.pivot.x,pivotY=p.pivot.y,
                    keyX=E.pose.x or 0,keyY=E.pose.y or 0,pivotOnly=E.movePivot}
                break
            end
        end
        if not E.worldDrag then
            E.pan={button=0,x=mouse.x,y=mouse.y,cx=E.camera.x,cy=E.camera.y}
        end
    end
    if E.worldDrag and tImGui.IsMouseDown(0) then
        local x,y=mouseWorld(mouse)
        local d,p=E.worldDrag,selected()
        if p and (x~=d.lastX or y~=d.lastY) then
            d.lastX,d.lastY=x,y
            if E.mode=='setup' then
                if not d.pivotOnly then p.x,p.y=d.px+x-d.x,d.py+y-d.y end
                p.pivot.x,p.pivot.y=d.pivotX+x-d.x,d.pivotY+y-d.y
                commit()
            else
                E.pose.x,E.pose.y=d.keyX+x-d.x,d.keyY+y-d.y
                if E.project.options.autoKey then record() else Pose.stage(E) end
            end
        end
    end
    if tImGui.IsMouseReleased(0) then E.worldDrag=nil end
    if not hovered and not E.worldDrag and not E.pan and tImGui.IsMouseClicked(2,false) then
        E.pan={button=2,x=mouse.x,y=mouse.y,cx=E.camera.x,cy=E.camera.y}
    end
    if E.pan and tImGui.IsMouseDown(E.pan.button) then
        E.camera:setPos(E.pan.cx+(E.pan.x-mouse.x)/E.camera.sx,E.pan.cy+(mouse.y-E.pan.y)/E.camera.sy)
    else E.pan=nil end
end
local function timeline()
    Pose.sync(E)
    if not E.showTimeline then return end
    local width,height=mbm.getRealSizeScreen()
    local first=tImGui.Flags('ImGuiCond_FirstUseEver')
    -- Reference layout from imgui.ini at 1920 x 1020: (222,796), 1385 x 201.
    tImGui.SetNextWindowPos({x=width*222/1920,y=height*796/1020},first)
    tImGui.SetNextWindowSize({x=width*1385/1920,y=height*201/1020},first)
    tImGui.SetNextWindowSizeConstraints({x=math.min(120,width),y=math.min(80,height)},{x=width,y=height})
    local opened,closed=tImGui.Begin(E.titles.timeline,true,E.flags)
    if closed then E.showTimeline=false end
    if opened then
        if widget('Selectable','setup',E.mode=='setup') then E.mode='setup'; E.playing=false; E.poseDirty=true end
        tImGui.SameLine()
        if widget('Selectable','animate',E.mode=='animate') then E.mode='animate'; E.poseDirty=true end
        Pose.sync(E)
        local activePart=selected()
        tImGui.Text(L('selected_part')..': '..(activePart and activePart.name or L('none')))
        check('auto_key',E.project.options,'autoKey')
        if check('onion',E.project.options,'onion') then E.dirty=true end
        for i,clip in ipairs(E.project.clips) do
            if tImGui.Selectable(clip.name..'##clip'..i,E.clip==i) then selectClip(i) end
            tooltip('select_clip')
        end
        button('add_clip',function() action(function()
            E.project.clips[#E.project.clips+1]={name=L('clip')..' '..(#E.project.clips+1),duration=1,speed=1,priority=0,loop=true,blend=0,tracks={}}
            E.clip=#E.project.clips; E.mode='animate'; Pose.sync(E)
        end) end)
        local clip=E.project.clips[E.clip]
        if clip then
            button('delete_clip',function() action(function()
                table.remove(E.project.clips,E.clip); E.clip=1; E.time=0; E.playing=false
            end) end)
            if E.project.clips[E.clip]~=clip then tImGui.End(); return end
            tImGui.PushItemWidth(100)
            tImGui.PushItemWidth(180)
            if E.renameClip~=clip or E.renameSource~=clip.name then
                E.renameClip=clip; E.renameSource=clip.name; E.renameDraft=clip.name
            end
            local changed,name=widget('InputText','clip_name',E.renameDraft)
            tImGui.PopItemWidth()
            if changed then E.renameDraft=name end
            tImGui.SameLine()
            local newName=E.renameDraft:match('^%s*(.-)%s*$')
            tImGui.BeginDisabled(newName=='' or newName==clip.name)
            button('rename_clip',function()
                if newName=='' or newName==clip.name then return end
                for _,other in ipairs(E.project.clips) do
                    assert(other==clip or other.name~=newName,L('clip_name_exists'))
                end
                action(function()
                    -- Keep the named base-frame selector of imported clips in sync.
                    for _,anim in ipairs(E.project.frameAnimations or {}) do
                        if clip.frame and anim[1]==clip.name and anim[2]==clip.frame and anim[3]==clip.frame then
                            anim[1]=newName
                        end
                    end
                    clip.name=newName
                end)
            end)
            tImGui.EndDisabled()
            if field('duration',clip,'duration',0.05,0.01,3600) then commit() end
            if field('speed',clip,'speed',0.05,0.01,100) then commit() end
            local priorityChanged,priority=widget('SliderInt','priority',clip.priority or 0,-20,20)
            if priorityChanged then clip.priority=priority; commit() end
            local wasAdditive=clip.blend==1
            local additive=widget('Checkbox','additive',wasAdditive)
            if additive~=wasAdditive then clip.blend=additive and 1 or 0; commit() end
            if check('loop',clip,'loop') then commit() end
            -- Keep scrubbing wide; properties and key values use compact fields.
            tImGui.PushItemWidth(math.max(100,tImGui.GetContentRegionAvail().x-90))
            local moved,time=widget('SliderFloat','time',E.time,0,clip.duration)
            tImGui.PopItemWidth()
            if moved then E.time=time; E.poseDirty=true; Pose.sync(E) end
            button(E.playing and 'pause' or 'play',function()
                Pose.clear(E); E.playing=not E.playing; E.mode='animate'; E.poseDirty=true
            end)
            if selected() then
                local modified=false
                for _,entry in ipairs({{'key_x','x'},{'key_y','y'},{'key_rotation','angle'},{'key_scale_x','sx'},{'key_scale_y','sy'}}) do
                    if field(entry[1],E.pose,entry[2],0.1) then modified=true end
                end
                tImGui.PushItemWidth(160)
                if widget('BeginCombo','easing',L('easing_'..(E.pose.easing or 0))) then
                    for easing=0,5 do
                        if tImGui.Selectable(L('easing_'..easing),(E.pose.easing or 0)==easing) then
                            E.pose.easing=easing; modified=true
                        end
                    end
                    tImGui.EndCombo()
                end
                tImGui.PopItemWidth()
                if E.pose.easing==5 then
                    E.pose.bezier=E.pose.bezier or {0.25,0.25,0.75,0.75}
                    for i=1,4 do
                        if field('bezier_'..i,E.pose.bezier,i,0.01,0,1) then modified=true end
                    end
                end
                if modified then
                    if E.project.options.autoKey then record() else Pose.stage(E) end
                end
                if E.transient then tImGui.TextWrapped(L('pending_poses')) end
                button('record',record)
                if E.keyOrigin then
                    button('move_key',function()
                        local ok=action(function()
                            local original
                            for _,track in ipairs(clip.tracks) do
                                if track.part==E.selected then
                                    for i=#track.keys,1,-1 do
                                        if math.abs(track.keys[i].time-E.keyOrigin)<0.0001 then
                                            original=Model.copy(track.keys[i]); table.remove(track.keys,i)
                                        end
                                    end
                                end
                            end
                            assert(original,'missing_key')
                            Model.key(E.project,E.clip,E.selected,E.time,original); E.keyOrigin=E.time
                        end)
                        if ok then Pose.recorded(E) end
                    end)
                end
                for _,track in ipairs(clip.tracks) do
                    if track.part==E.selected then
                        for ki,k in ipairs(track.keys) do
                            if tImGui.Selectable(string.format('%.3f##key%d',k.time,ki),math.abs(k.time-E.time)<0.0001) then
                                E.time=k.time; Pose.sync(E)
                                if E.drafts and E.drafts[E.selected] then Pose.recorded(E); E.dirty=true end
                                E.pose=Pose.sample(clip,E.selected,E.time)
                                E.keyOrigin=k.time; E.poseDirty=true
                            end
                            tooltip('select_key')
                        end
                        button('delete_key',function()
                            local ok=action(function()
                                for ki=#track.keys,1,-1 do if math.abs(track.keys[ki].time-E.time)<0.0001 then table.remove(track.keys,ki) end end
                            end)
                            if ok then Pose.recorded(E); E.keyOrigin=nil end
                        end)
                    end
                end
            end
            tImGui.PopItemWidth()
        end
        tImGui.Text(E.status)
    end
    tImGui.End()
end
function onInitScene()
    E.camera=mbm.getCamera('2d')
    E.noMoveFlag=tImGui.Flags('ImGuiWindowFlags_NoMove'); E.flags=E.noMoveFlag
    refreshTitles()
    E.history=Model.history(E.project)
    E.pivotMarker=line:new('2dw',0,0,-1)
    E.pivotMarker:add({-8,0,8,0}); E.pivotMarker:add({0,-8,0,8}); E.pivotMarker:setColor(1,0,1)
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
    Pose.sync(E)
    menu(); sourcePanel(); properties(); timeline(); sourceCanvas(); worldInput()
    Pose.sync(E)
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
    E.zoomRequest=(E.zoomRequest or 0)+zoom
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
if type(testApi)=='table' then
    testApi.importScml=importScml
    testApi.selectClip=selectClip
    testApi.syncPose=function() Pose.sync(E) end
    testApi.stagePose=function() Pose.stage(E) end
    testApi.record=record
    testApi.state=E
    testApi.addImage=addImage
    testApi.generate=generate
    testApi.rebuild=rebuild
    testApi.loadProject=loadProject
    testApi.worldInput=worldInput
    testApi.check=check
    testApi.refreshTitles=refreshTitles
    testApi.startContourDrag=startContourDrag
    testApi.moveContourPoint=moveContourPoint
    testApi.finishContourDrag=finishContourDrag
end
