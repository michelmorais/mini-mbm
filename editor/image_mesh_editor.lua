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

local testApi=...
tImGui=require 'ImGui'
tUtil=require 'editor_utils'
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local Canvas=require 'image_mesh_canvas'
local E={project=Model.new(),history=Model.history(),selected=0,selection={},tool='select',zoom=1,
    polygon={},revision=0,builds=0,modified=false,grid={columns=4,rows=3,marginX=0,marginY=0,gapX=0,gapY=0},
    orbit={azimuth=math.pi-0.4,elevation=0.25,distance=300},status='',point=1}
local function L(key) return tLang.L('ime_'..key) end
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then E.status=tLang.L(tostring(result[2]):match('(ime_[%w_]+)$') or tostring(result[2])); print('[image_mesh_editor] '..E.status) end
    return table.unpack(result,1,result.n)
end
local function releasePreview()
    if E.preview then
        E.target:remove(E.preview); meshDebug:loadMeshPreview(E.preview,nil); E.preview:destroy(); E.preview=nil
    end
    if E.previewPath then os.remove(E.previewPath); E.previewPath=nil end
end
local function syncDraft()
    local r=Model.region(E.project,E.selected)
    E.draft=r and Model.copy(r) or nil
    E.values=Model.copy(E.editDefaults and E.project.defaults or (r and Model.options(E.project,r) or E.project.defaults))
    E.point=1
end
local function changed()
    E.revision=E.revision+1; E.modified=true; E.dirty=true; E.outlines=nil; E.overlay=nil; E.report=nil
    if E.preview then E.preview.visible=false end
    syncDraft()
end
local function selectRegion(id,extend)
    if E.drag then return end
    if extend then E.selection[id]=not E.selection[id] else E.selection={[id]=true} end
    E.selected=id
    if not E.selection[id] then E.selected=0; for _,r in ipairs(E.project.regions) do if E.selection[r.id] then E.selected=r.id; break end end end
    E.dirty=true; E.report=nil; E.polygon={}; syncDraft()
    if E.preview then E.preview.visible=false end
end
local function action(fn)
    if E.drag then return false end
    local before=E.project; local selected,selection=E.selected,Model.copy(E.selection); local candidate=Model.copy(before)
    local ok=dpCall(function() fn(candidate); Model.validate(candidate) end)
    if not ok then E.selected=selected; E.selection=selection; return false end
    Model.commit(E.history,before); E.project=candidate; changed(); return true
end
local function commitDrag(before)
    local ok=dpCall(Model.validate,E.project)
    if ok then Model.commit(E.history,before); changed() else E.project=before; E.outlines=nil; E.overlay=nil; syncDraft() end
end
local function history(redo)
    if E.drag then E.project=E.drag.before or E.project; E.drag=nil end
    local project=Model.undo(E.history,E.project,redo)
    if not project then return end
    E.project=project
    if not Model.region(project,E.selected) then E.selected=project.regions[1] and project.regions[1].id or 0 end
    E.selection={[E.selected]=true}; E.polygon={}; changed()
end
local function camera()
    local c=E.orbit
    E.previewCamera:setPos(c.distance*math.cos(c.elevation)*math.sin(c.azimuth),c.distance*math.sin(c.elevation),c.distance*math.cos(c.elevation)*math.cos(c.azimuth))
    E.renderFrames=2
    E.previewCamera:setFocus(0,0,0)
    E.previewCamera:setFar(math.max(2000,c.distance*10))
end
local function generate(region)
    local asset,report=mbm.generateImageMesh(E.project.image.path,Model.options(E.project,region))
    assert(asset,report); return asset,report
end
local function rebuild()
    if not E.dirty or E.drag then return end
    E.dirty=false; releasePreview()
    local r=Model.region(E.project,E.selected); if not r or not E.texture then return end
    local path=tUtil.getTemporaryFilePath('.msh'); local object
    local ok=dpCall(function()
        local asset,report=generate(r)
        assert(asset:save(path,false,false,true),L('export_failed'))
        object=mesh:new('3d'); assert(meshDebug:loadMeshPreview(object,path),L('preview_failed'))
        object.alwaysRender=true
        assert(E.target:add(object),L('preview_failed'))
        E.preview=object; E.previewPath=path; E.report=report; E.builds=E.builds+1
        local o=Model.options(E.project,r); E.orbit.distance=math.max(o.width,o.height,o.depth+o.relief)*2.7; camera()
        E.status=L('preview_ready')
    end)
    if not ok then
        if object then E.target:remove(object); meshDebug:loadMeshPreview(object,nil); object:destroy() end
        E.preview=nil; E.previewPath=nil; os.remove(path)
    end
end
local function loadTexture(path)
    assert(IO.exists(path),L('missing_image'))
    mbm.addPath(IO.directory(path))
    local texture=assert(mbm.loadTexture(path),L('invalid_image'))
    assert(texture.width*texture.height<=16777216,L('invalid_image'))
    return texture
end
local function install(project,path,texture)
    releasePreview(); E.project=project; E.path=path; E.texture=texture; E.history=Model.history()
    E.selected=project.regions[1] and project.regions[1].id or 0; E.selection={[E.selected]=true}
    E.polygon={}; E.drag=nil; E.missing=nil; E.zoom=1; changed(); E.modified=false
end
local function openImage(path)
    local texture=loadTexture(path)
    install(Model.new(path,texture.width,texture.height),nil,texture)
    E.status=L('open_help'); return true
end
local function openProject(path)
    local project=IO.load(path)
    if not IO.exists(project.image.path) then E.missing={project=project,path=path}; E.status=L('missing_image'); return false end
    local texture=loadTexture(project.image.path)
    assert(texture.width==project.image.width and texture.height==project.image.height,L('image_size_changed'))
    install(project,path,texture); return true
end
local function relink(path)
    local pending=assert(E.missing)
    local texture=loadTexture(path)
    assert(texture.width==pending.project.image.width and texture.height==pending.project.image.height,L('image_size_changed'))
    pending.project.image.path=path; install(pending.project,pending.path,texture); E.modified=true
end
local function saveProject(path)
    IO.save(E.project,path,tUtil.save); E.path=path; E.modified=false; E.status=L('saved'); return true
end
local function exportOne(path)
    local r=assert(Model.region(E.project,E.selected),L('select_region'))
    local asset=generate(r); assert(asset:save(path,false,false,true),L('export_failed'))
    E.status=L('exported')..' '..path; return true
end
local function beginBatch(directory)
    assert(#E.project.regions>0,L('select_region'))
    E.batch={directory=directory,index=1,completed=0,failures={},project=Model.copy(E.project)}
end
local function batchStep()
    local batch=E.batch; if not batch then return end
    local region=batch.project.regions[batch.index]
    if not region then
        E.status=string.format(L('batch_done'),batch.completed,#batch.failures)
        if #batch.failures>0 then E.status=E.status..' '..table.concat(batch.failures,'; ') end
        E.batch=nil; return
    end
    local ok,err=dpCall(function()
        local asset,message=mbm.generateImageMesh(batch.project.image.path,Model.options(batch.project,region)); assert(asset,message)
        local path=batch.directory..'/'..IO.exportName(region)
        -- Do not silently overwrite an earlier export; the user chooses a fresh folder.
        assert(not IO.exists(path),L('file_exists')..' '..path)
        assert(asset:save(path,false,false,true),L('export_failed'))
    end)
    if ok then batch.completed=batch.completed+1 else batch.failures[#batch.failures+1]=region.name..': '..tostring(err) end
    batch.index=batch.index+1
end
local function requestReplace(fn)
    if E.modified then E.pending=fn; tImGui.OpenPopup('ime_discard') else dpCall(fn) end
end
local function finishPolygon()
    local points=E.polygon
    if action(function(p) local r=Model.fromPoints(p,points); E.selected=r.id; E.selection={[r.id]=true} end) then E.polygon={} end
end
local function applyProperties()
    local draft,values=E.draft,Model.copy(E.values)
    action(function(p)
        local settings={}; for k in pairs(Model.defaults) do settings[k]=values[k] end
        if E.editDefaults then p.defaults=settings; return end
        for _,r in ipairs(p.regions) do if E.selection[r.id] then
            r.overrides={}; for k,v in pairs(settings) do if v~=p.defaults[k] then r.overrides[k]=v end end
        end end
        local r=assert(Model.region(p,E.selected),L('select_region'))
        r.name=draft.name; r.x=draft.x; r.y=draft.y; r.w=draft.w; r.h=draft.h; r.shape=draft.shape; r.contour=Model.copy(draft.contour)
    end)
end
local function menu()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu(tLang.L('menu_file')) then
            if tImGui.MenuItem(L('open_image')) then requestReplace(function() local path=mbm.openFile(E.project.image.path,table.unpack(tUtil.supported_images)); if path then openImage(path) end end) end
            if tImGui.MenuItem(L('open_project')) then requestReplace(function() local path=mbm.openFile(E.path or '', 'imesh'); if path then openProject(path) end end) end
            if tImGui.MenuItem(L('save'),'Ctrl+S') then dpCall(function() local path=E.path or mbm.saveFile('project.imesh','imesh'); if path then saveProject(path) end end) end
            if tImGui.MenuItem(L('save_as')) then dpCall(function() local path=mbm.saveFile(E.path or 'project.imesh','imesh'); if path then saveProject(path) end end) end
            if tImGui.MenuItem(L('export_selected')) then dpCall(function() local path=mbm.saveFile('module.msh','msh'); if path then exportOne(path) end end) end
            if tImGui.MenuItem(L('export_all')) then dpCall(function() local path=mbm.openFolder(L('export_folder')); if path then beginBatch(path) end end) end
            if tImGui.MenuItem(tLang.L('menu_quit')) then requestReplace(mbm.quit) end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(L('edit')) then
            if tImGui.MenuItem(L('undo'),'Ctrl+Z') then history(false) end
            if tImGui.MenuItem(L('redo'),'Ctrl+Y') then history(true) end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(tLang.L('menu_options')) then tLang.renderLanguageSubmenu(); tImGui.EndMenu() end
        tImGui.Text(E.modified and '*' or ''); tImGui.EndMainMenuBar()
    end
    if tImGui.BeginPopupModal('ime_discard',true,E.flags.auto) then
        tImGui.Text(L('discard'))
        if tImGui.Button(L('continue')) then local fn=E.pending; E.pending=nil; tImGui.CloseCurrentPopup(); if fn then dpCall(fn) end end
        tImGui.SameLine(); if tImGui.Button(L('cancel')) then E.pending=nil; tImGui.CloseCurrentPopup() end
        tImGui.EndPopup()
    end
end
local function window(key,x,y,w,h)
    tImGui.SetNextWindowPos({x=x,y=y},E.flags.first)
    tImGui.SetNextWindowSize({x=w,y=h},E.flags.first)
    return tImGui.Begin(L(key)..'###'..E.titles[key],false,0)
end
local function regionsPanel()
    if window('regions',0,25,260,E.screenH-25) then
        tImGui.PushItemWidth(105)
        if E.missing then
            tImGui.Text(L('missing_image'))
            if tImGui.Button(L('relink')) then dpCall(function() local path=mbm.openFile('',table.unpack(tUtil.supported_images)); if path then relink(path) end end) end
            tImGui.SameLine(); if tImGui.Button(L('cancel')) then E.missing=nil end
        end
        if E.texture then
            tImGui.Text(tUtil.getShortName(E.project.image.path))
            if tImGui.CollapsingHeader(L('grid')) then
                for _,key in ipairs({'columns','rows','marginX','marginY','gapX','gapY'}) do
                    local edit,value=tImGui.InputInt(L('grid_'..key),E.grid[key]); if edit then E.grid[key]=value end
                end
                if tImGui.Button(L('create_grid')) then action(function(p) local ids=Model.grid(p,E.grid); E.selected=ids[1]; E.selection={}; for _,id in ipairs(ids) do E.selection[id]=true end end) end
            end
            if tImGui.Button(L('duplicate')) then action(function(p)
                local originals=Model.copy(p.regions); local selection=E.selection; E.selection={}; local first
                for _,r in ipairs(originals) do if selection[r.id] then local n=Model.add(p,r.shape,r.x,r.y,r.w,r.h,r.contour)
                    n.overrides=Model.copy(r.overrides); n.name=r.name:sub(1,123)..'_copy'; first=first or n.id; E.selected=first; E.selection[n.id]=true
                end end
            end) end
            tImGui.SameLine()
            if tImGui.Button(L('delete')) then action(function(p)
                for i=#p.regions,1,-1 do if E.selection[p.regions[i].id] then table.remove(p.regions,i) end end
                E.selected=p.regions[1] and p.regions[1].id or 0; E.selection={[E.selected]=true}
            end) end
            tImGui.Text(L('selection_help'))
            for _,r in ipairs(E.project.regions) do
                if tImGui.Selectable(r.name..'##region'..r.id,E.selection[r.id] or false) then selectRegion(r.id,E.control) end
            end
        else tImGui.Text(L('open_help')) end
        if E.batch then
            tImGui.ProgressBar((E.batch.index-1)/#E.batch.project.regions,{x=-1,y=0},L('exporting'))
            if tImGui.Button(L('cancel')) then E.status=L('cancelled'); E.batch=nil end
        end
        tImGui.PopItemWidth()
    end
    tImGui.End()
end
local function imagePanel()
    if window('image',260,25,E.middleW,E.screenH-25) then
        local names={L('select'),L('rectangle'),L('ellipse'),L('polygon')}; local tools={'select','rectangle','ellipse','polygon'}
        local index=1; for i,name in ipairs(tools) do if name==E.tool then index=i end end
        local changedTool,value=tImGui.Combo(L('tool'),index,names)
        if changedTool then E.tool=tools[value]; E.polygon={} end
        local edited,zoom=tImGui.SliderFloat(L('zoom'),E.zoom,0.25,4); if edited then E.zoom=zoom end
        tImGui.TextWrapped(L('canvas_help'))
        if E.tool=='polygon' then
            if tImGui.Button(L('finish_polygon')) then finishPolygon() end
            tImGui.SameLine(); if tImGui.Button(L('cancel')) then E.polygon={} end
        end
        if tImGui.BeginChild('ime_canvas',{x=0,y=0},true,E.flags.scroll) then
            Canvas.draw(E,{action=action,select=selectRegion,commitDrag=commitDrag})
        end
        tImGui.EndChild()
    end
    tImGui.End()
end
local function propertiesPanel()
    if window('properties',260+E.middleW,25,E.rightW,E.screenH*0.51) then
        tImGui.PushItemWidth(math.max(90,tImGui.GetContentRegionAvail().x*0.42))
        local edit,defaults=tImGui.Checkbox(L('edit_defaults'),E.editDefaults or false)
        if edit then E.editDefaults=defaults; syncDraft() end
        if E.draft or E.editDefaults then
            if not E.editDefaults then
                local d=E.draft
                local edited,value=tImGui.InputText(L('name'),d.name); if edited then d.name=value end
                for _,key in ipairs({'x','y','w','h'}) do local c,v=tImGui.InputInt(L('crop_'..key),d[key]); if c then d[key]=v end end
                local shapes={'rectangle','ellipse','polygon'}; local idx=d.shape=='rectangle' and 1 or (d.shape=='ellipse' and 2 or 3)
                local c,v=tImGui.Combo(L('shape'),idx,{L('rectangle'),L('ellipse'),L('polygon')})
                if c then d.shape=shapes[v]; if d.shape=='polygon' and not d.contour then d.contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=0,y=1}} end end
                if d.shape=='polygon' and tImGui.CollapsingHeader(L('points')) then
                    local change,index=tImGui.SliderInt(L('point'),E.point,1,#d.contour); if change then E.point=index end
                    local point=d.contour[E.point]
                    for _,axis in ipairs({'x','y'}) do local modified,n=tImGui.InputFloat(axis,point[axis]); if modified then point[axis]=n end end
                    if tImGui.Button(L('add_point')) and #d.contour<128 then local nextPoint=d.contour[E.point%#d.contour+1]
                        table.insert(d.contour,E.point+1,{x=(point.x+nextPoint.x)/2,y=(point.y+nextPoint.y)/2}); E.point=E.point+1
                    end
                    tImGui.SameLine(); if tImGui.Button(L('remove_point')) and #d.contour>3 then table.remove(d.contour,E.point); E.point=math.min(E.point,#d.contour) end
                end
            end
            for _,key in ipairs({'width','height','depth','relief','borderWidth'}) do
                local c,v=tImGui.InputFloat(L(key),E.values[key]); if c then E.values[key]=v end
            end
            for _,key in ipairs({'columns','rows','ellipseSegments','maxVertices','maxTriangles'}) do
                local c,v=tImGui.InputInt(L(key),E.values[key]); if c then E.values[key]=v end
            end
            for _,key in ipairs({'invert','lockBorder'}) do local c,v=tImGui.Checkbox(L(key),E.values[key]); if c then E.values[key]=v end end
            if tImGui.Button(L('apply')) then applyProperties() end
            if not E.editDefaults then
                tImGui.SameLine(); if tImGui.Button(L('inherit')) then action(function(p) for _,r in ipairs(p.regions) do if E.selection[r.id] then r.overrides={} end end end) end
            end
        else tImGui.Text(L('select_region')) end
        tImGui.PopItemWidth()
    end
    tImGui.End()
end
local function previewPanel()
    if window('preview',260+E.middleW,30+E.screenH*0.51,E.rightW,E.screenH*0.49-30) then
        if E.report then tImGui.Text(string.format(L('counts'),E.report.vertices,E.report.triangles)) end
        tImGui.TextWrapped(E.status)
        tImGui.Text(L('preview_help'))
        tImGui.SetNextItemWidth(math.max(90,tImGui.GetContentRegionAvail().x*0.5))
        local c,v=tImGui.SliderFloat(L('light'),E.light,0,1)
        if c then E.light=v; mbm.setDirectionalLightColor('3d',v,v,v); E.renderFrames=2 end
        local available=tImGui.GetContentRegionAvail(); local side=math.max(32,math.min(available.x,available.y))
        if E.preview then
            tImGui.Image(E.targetTexture,{x=side,y=side},E.previewUV0,E.previewUV1)
            local hover=tImGui.IsItemHovered(0); local mouse=tImGui.GetMousePos()
            if hover and tImGui.IsMouseClicked(0,false) then E.orbitDrag={x=mouse.x,y=mouse.y} end
            if E.orbitDrag and tImGui.IsMouseDown(0) then
                local dx,dy=mouse.x-E.orbitDrag.x,mouse.y-E.orbitDrag.y
                if dx~=0 or dy~=0 then E.orbit.azimuth=E.orbit.azimuth-dx*0.01; E.orbit.elevation=math.max(-1.5,math.min(1.5,E.orbit.elevation+dy*0.01)); camera() end
                E.orbitDrag=mouse
            else E.orbitDrag=nil end
            if hover then local wheel=tImGui.GetZoom(); if wheel~=0 then E.orbit.distance=math.max(0.01,E.orbit.distance*math.exp(-wheel*0.12)); camera() end end
        end
    end
    tImGui.End()
end
function onInitScene()
    local w,h=mbm.getSizeScreen(); E.screenW=w; E.screenH=h; E.middleW=math.max(280,(w-260)*0.55); E.rightW=math.max(280,w-260-E.middleW)
    E.flags={first=tImGui.Flags('ImGuiCond_FirstUseEver'),auto=tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize'),scroll=tImGui.Flags('ImGuiWindowFlags_HorizontalScrollbar')}
    E.titles={regions='ime_regions',image='ime_image',properties='ime_properties',preview='ime_preview'}
    mbm.setColor(0.1,0.12,0.15); mbm.setLightEnabled('3d',true); mbm.setAmbientLight('3d',0.35,0.35,0.35)
    E.light=0.7; mbm.setDirectionalLight('3d',0.4,-0.5,1,E.light,E.light,E.light)
    local flipPreview=mbm.get('USE_OPENGL_ES')
    E.previewUV0={x=0,y=flipPreview and 1 or 0}; E.previewUV1={x=1,y=flipPreview and 0 or 1}
    E.target=render2texture:new('2ds'); local ok,_,texture=E.target:create(640,640,true)
    assert(ok and texture,L('preview_failed')); E.targetTexture=texture; E.target:enableFrame(false); E.target:setColor(0.10,0.12,0.15,1)
    E.previewCamera=E.target:getCamera('3d'); camera()
    E.originX=line:new('2dw'); E.originX:add({-1000,0,1000,0}); E.originX:setColor(1,0,0); E.originX.visible=false
    E.originY=line:new('2dw'); E.originY:add({0,-1000,0,1000}); E.originY:setColor(0,1,0); E.originY.visible=false
    local checker=tUtil.createAlphaPattern(256,256,16,{r=70,g=70,b=70},{r=100,g=100,b=100})
    if checker then E.checker=mbm.loadTexture(checker) end
    syncDraft(); tUtil.sMessageOverlay=L('welcome')
end
function onLoop(delta)
    menu(); regionsPanel(); imagePanel(); propertiesPanel(); previewPanel()
    if E.key and not tImGui.GetWantCaptureKeyboard() then
        if E.control and E.key==mbm.getKeyCode('Z') then history(false)
        elseif E.control and E.key==mbm.getKeyCode('Y') then history(true)
        elseif E.control and E.key==mbm.getKeyCode('S') then dpCall(function() local path=E.path or mbm.saveFile('project.imesh','imesh'); if path then saveProject(path) end end)
        elseif E.key==mbm.getKeyCode('ESC') then E.polygon={}; if E.drag then E.project=E.drag.before or E.project; E.drag=nil; E.outlines=nil; E.overlay=nil end end
    end
    E.key=nil; rebuild(); batchStep()
    E.target.visible=(E.renderFrames or 0)>0
    E.renderFrames=math.max(0,(E.renderFrames or 0)-1)
    tUtil.showOverlayMessage()
end
function onKeyDown(key)
    if key==mbm.getKeyCode('control') then E.control=true else E.key=key end
end
function onKeyUp(key) if key==mbm.getKeyCode('control') then E.control=false end end
function onEndScene() releasePreview(); if E.target then E.target:destroy(); E.target=nil end end
if type(testApi)=='table' then
    testApi.state=E; testApi.openImage=openImage; testApi.openProject=openProject; testApi.saveProject=saveProject
    testApi.action=action; testApi.select=selectRegion; testApi.rebuild=rebuild; testApi.undo=history
    testApi.exportOne=exportOne; testApi.beginBatch=beginBatch; testApi.batchStep=batchStep; testApi.relink=relink
    testApi.applyProperties=applyProperties; testApi.finishPolygon=finishPolygon
end
