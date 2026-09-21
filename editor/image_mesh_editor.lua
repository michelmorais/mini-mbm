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
local Help=require 'image_mesh_help'
local IO=require 'image_mesh_io'
local Portable=require 'image_mesh_portable'
local Canvas=require 'image_mesh_canvas'
local Diagnostics=require 'image_mesh_diagnostics'
local Wire=require 'image_mesh_wireframe'
local HeightPreview=require 'image_mesh_height_preview'
local Paint=require 'image_mesh_paint'
local Presets=require 'image_mesh_presets'
local BackUv=require 'image_mesh_back_uv'
local Asset=require 'image_mesh_asset'
local GeometryCache=require 'image_mesh_cache'
local Sides=require 'image_mesh_sides'
local Holes=require 'image_mesh_holes'
local Areas=require 'image_mesh_areas'
local Freehand=require 'image_mesh_freehand'
local Auto=require 'image_mesh_auto'
local Simplify=require 'image_mesh_simplify'
local Comparison=require 'image_mesh_comparison'
local Assembly=require 'image_mesh_assembly'
local E={project=Model.new(),history=Model.history(),selected=0,selection={},tool='select',zoom=1,
    primitive={kind='rectangle',w=64,h=64,sides=6},editMode=true,wireframe=false,heightView=1,sidebar=370,rightbar=310,polygon={},statistics={},revision=0,builds=0,modified=false,grid={columns=4,rows=3,marginX=0,marginY=0,gapX=0,gapY=0},
    orbit={fx=0,fy=0,fz=0,azimuth=0.3,elevation=0.3,distance=300},status='',point=1}
local function L(key) return tLang.L('ime_'..key) end
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then E.status=tLang.L(tostring(result[2]):match('(ime_[%w_]+)$') or tostring(result[2])); print('[image_mesh_editor] '..E.status) end
    return table.unpack(result,1,result.n)
end
local function releasePreview()
    Assembly.release(E)
    Comparison.release(E)
    E.generationFailure=nil
    Wire.release(E)
    if E.preview then
        meshDebug:loadMeshPreview(E.preview,nil); E.preview:destroy(); E.preview=nil
    end
    if E.previewPath then os.remove(E.previewPath); E.previewPath=nil end
end
local function syncDraft()
    E.canvasDirty=true
    local r=Model.region(E.project,E.selected)
    E.draft=r and Model.copy(r) or nil
    E.values=Model.copy(E.editDefaults and E.project.defaults or (r and Model.options(E.project,r) or E.project.defaults))
    for k,v in pairs(Model.optionalDefaults) do if E.values[k]==nil then E.values[k]=v end end
    E.values.preserveAspect=E.values.preserveAspect~=false
    if (E.tool=='back_uv' and not BackUv.available(E)) or (E.tool=='side_band' and not Sides.available(E)) then E.tool='select' end
    E.holeIndex=math.max(1,math.min(E.holeIndex or 1,r and #(r.holes or {}) or 0))
    E.areaIndex=math.max(1,math.min(E.areaIndex or 1,r and #(r.heightAreas or {}) or 0))
    E.point=1
end
local function changed()
    E.statisticsRequested=nil
    GeometryCache.clear(E)
    E.statistics={}
    HeightPreview.destroy(E)
    E.generationFailure=nil
    E.revision=E.revision+1; E.modified=true; E.dirty=true; E.outlines=nil; E.report=nil
    Comparison.sync(E); Assembly.sync(E)
    syncDraft()
end
local function selectRegion(id,extend)
    E.statisticsRequested=nil
    Paint.cancel(E)
    if E.drag then return end
    if extend then E.selection[id]=not E.selection[id] else E.selection={[id]=true} end
    E.selected=id
    if E.tool=='back_uv' or E.tool=='side_band' then E.tool='select' end
    if not E.selection[id] then E.selected=0; for _,r in ipairs(E.project.regions) do if E.selection[r.id] then E.selected=r.id; break end end end
    E.generationFailure=nil
    HeightPreview.destroy(E); E.heightRequested=true
    E.dirty=true; E.report=nil; E.polygon={}; E.stroke=nil; E.autoTask=nil; E.autoSource=nil; E.canvasDirty=true; E.editDefaults=false; syncDraft()
    Comparison.sync(E); Assembly.sync(E)
end
local function action(fn)
    if E.drag then return false end
    local before=E.project; local selected,selection=E.selected,Model.copy(E.selection); local candidate=Model.copy(before)
    local ok=dpCall(function() fn(candidate); Model.ensureBackCrops(candidate); Model.validate(candidate) end)
    if not ok then E.selected=selected; E.selection=selection; return false end
    Model.commit(E.history,before); E.project=candidate; changed(); return true
end
local function commitDrag(before)
    local ok=dpCall(Model.validate,E.project)
    if ok then Model.commit(E.history,before); changed() else E.project=before; E.outlines=nil; syncDraft() end
end
local function history(redo)
    if E.paintDrag then Paint.cancel(E); return end
    if E.stroke or E.autoTask then Canvas.cancel(E); return end
    if E.drag then E.project=E.drag.before or E.project; E.drag=nil end
    local project=Model.undo(E.history,E.project,redo)
    if not project then return end
    E.project=project
    if not Model.region(project,E.selected) then E.selected=project.regions[1] and project.regions[1].id or 0 end
    E.selection={[E.selected]=true}; E.polygon={}; E.stroke=nil; changed()
end
local function camera()
    local c=E.orbit
    -- Shift the camera parallel to its view plane so the module is centered
    -- in the scene area beside the sidebar. Match CAMERA::updateCam's current
    -- projection conversion (cot(angle/2) is passed as the perspective angle).
    local halfHeight=c.distance*math.tan(0.5/math.tan(math.rad(55)))
    local offset=halfHeight*(E.sidebar-E.rightbar)/E.screenH
    local ox,oz=math.cos(c.azimuth)*offset,-math.sin(c.azimuth)*offset
    E.previewCamera:setPos(c.fx+ox+c.distance*math.cos(c.elevation)*math.sin(c.azimuth),c.fy+c.distance*math.sin(c.elevation),c.fz+oz+c.distance*math.cos(c.elevation)*math.cos(c.azimuth))

    E.previewCamera:setFocus(c.fx+ox,c.fy,c.fz+oz)
    E.previewCamera:setFar(math.max(2000,c.distance*10))
end
local function generationError(region,message)
    local details={}
    for kind,count,limit in tostring(message):gmatch('(%a+) >= (%d+), limit (%d+)') do
        local label=kind=='vertices' and L('maxVertices') or L('maxTriangles')
        details[#details+1]=string.format(L('budget_exceeded'),label,tonumber(count),tonumber(limit))
    end
    if #details>0 then
        return region.name..': '..table.concat(details,'\n')..'\n'..L('budget_reduce')
    end
    return region.name..': '..tostring(message)
end
local function generate(region,project,keepOriginal,cacheOriginal)
    project=project or E.project
    local options=Model.options(project,region)
    local asset,report=mbm.generateImageMesh(project.image.path,options)
    if not asset then error(generationError(region,report),0) end
    -- Match Mesh Debug's +Z front view. Rotate positions AND authored normals
    -- by 180 degrees around Y, preserving UVs, winding and smooth/hard edges.
    local vertices=Asset.vertices(asset,true)
    if keepOriginal and options.simplify then Comparison.capture(E,asset,vertices) end
    if cacheOriginal and options.simplify then GeometryCache.original(E,asset) end
    Simplify.apply(E,asset,options,report)
    return asset,report
end
local function compactCount(value)
    if value<1000 then return tostring(value) end
    local divisor,suffix=1000,'K'
    if value>=999950 then divisor,suffix=1000000,'M' end
    return string.format('%.1f',value/divisor):gsub('%.0$','')..suffix
end
local function updateStatisticsImpl()
    if not E.editMode or E.drag or not E.texture or E.selected==0 then return end
    local cached=E.statistics[E.selected]
    if not cached then
        local region=Model.region(E.project,E.selected)
        if not region then return end
        GeometryCache.begin(E,region.id)
        local ok,asset,report=dpCall(generate,region,nil,false,true)
        if ok then GeometryCache.finish(E,asset,report) else GeometryCache.clear(E) end
        cached=ok and {report=report} or {error=E.status}
        E.statistics[E.selected]=cached
        E.statisticsBuilds=(E.statisticsBuilds or 0)+1
    end
    E.report=cached.report; E.generationFailure=cached.error
end
local function updateStatistics(requested)
    if E.meshTask or E.paintDrag or (E.paint and E.paint.enabled and not requested) or not E.editMode or E.drag or not E.texture or E.selected==0 then return end
    if E.statistics[E.selected] then return updateStatisticsImpl() end
    if not requested then return end
    E.statisticsRequested=nil
    return Simplify.run(E,updateStatisticsImpl)
end
local function rebuildImpl()
    if not E.dirty or E.drag or E.editMode then return end
    if E.assembly and E.assembly.enabled then return Assembly.build(E,generate,dpCall,camera) end
    E.dirty=false; releasePreview()
    local r=Model.region(E.project,E.selected); if not r or not E.texture then return end
    local path=tUtil.getTemporaryFilePath('.msh'); local object
    local ok=dpCall(function()
        local asset,report
        local cached=GeometryCache.get(E,r.id)
        if cached then
            asset,report=cached.asset,cached.report
            if cached.originalPath then
                local original=meshDebug:new(); assert(original:load(cached.originalPath),L('preview_failed'))
                Comparison.capture(E,original,Asset.vertices(original))
            end
        else asset,report=generate(r,nil,true) end
        assert(asset:save(path,false,false,true),L('export_failed'))
        object=mesh:new('3d'); assert(meshDebug:loadMeshPreview(object,path),L('preview_failed'))
        object.alwaysRender=true
        E.preview=object; E.previewPath=path; E.report=report; E.statistics[r.id]={report=report}; E.builds=E.builds+1
        Comparison.layout(E,asset)
        local o=Model.options(E.project,r)
        E.fitDistance=math.max(o.width,o.height,o.depth+o.relief)*2.7
        E.singleFitDistance=E.fitDistance
        -- Rebuilding the same module must not disturb the user's comparison view.
        if E.viewRegion~=r.id then E.orbit.distance=E.fitDistance; camera() end
        if E.wireframe then Wire.ensure(E,asset) end
        Comparison.sync(E); Assembly.sync(E)
        E.viewRegion=r.id
        E.status=L('preview_ready')
    end)
    GeometryCache.clear(E)
    if not ok then
        E.generationFailure=E.status
        Comparison.release(E)
        Wire.release(E)
        if object then meshDebug:loadMeshPreview(object,nil); object:destroy() end
        E.preview=nil; E.previewPath=nil; os.remove(path)
    end
end
local function rebuild()
    if E.meshTask or not E.dirty or E.drag or E.editMode then return end
    return Simplify.run(E,rebuildImpl)
end
local function loadTexture(path)
    assert(IO.exists(path),L('missing_image'))
    mbm.addPath(IO.directory(path))
    local texture=assert(mbm.loadTexture(path),L('invalid_image'))
    assert(texture.width*texture.height<=16777216,L('invalid_image'))
    return texture
end
local function install(project,path,texture)
    Paint.destroy(E); if E.paint then E.paint.enabled=false end
    releasePreview(); E.assembly=nil; Canvas.destroy(E); E.sideContour=nil; E.project=project; E.path=path; E.texture=texture; E.history=Model.history()
    if E.tool=='back_uv' or E.tool=='side_band' then E.tool='select' end
    E.selected=project.regions[1] and project.regions[1].id or 0; E.selection={[E.selected]=true}
    E.polygon={}; E.stroke=nil; E.autoTask=nil; E.autoSource=nil; E.drag=nil; E.missing=nil; E.zoom=1; E.viewRegion=nil
    E.primitive.w=math.max(2,math.floor(project.image.width/4)); E.primitive.h=math.max(2,math.floor(project.image.height/4))
    Canvas.fit(E); changed(); E.modified=false
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
local applyProperties
local function draftChanged()
    if not E.editDefaults and not E.draft then return false end
    local region=Model.region(E.project,E.selected)
    local current=E.editDefaults and E.project.defaults or Model.options(E.project,region)
    for key in pairs(Model.defaults) do
        local expected=current[key]
        if expected==nil then expected=Model.optionalDefaults[key] end
        if key~='maxTriangles' and not (key=='height' and E.values.preserveAspect) and E.values[key]~=expected then return true end
    end
    if E.editDefaults then return false end
    for _,key in ipairs({'name','x','y','w','h','shape'}) do if E.draft[key]~=region[key] then return true end end
    if Areas.different(E.draft.heightAreas,region.heightAreas) then return true end
    local ca,cb=E.draft.backCrop,region.backCrop
    if (ca==nil)~=(cb==nil) then return true end
    if ca then for _,key in ipairs({'x','y','w','h'}) do if ca[key]~=cb[key] then return true end end end
    local a,b=E.draft.contour or {},region.contour or {}
    if #a~=#b then return true end
    for i,p in ipairs(a) do if p.x~=b[i].x or p.y~=b[i].y then return true end end
    return false
end
local function saveProject(path)
    -- Save the settings currently displayed, even if Apply was not pressed yet.
    if draftChanged() and not applyProperties() then return false end
    IO.save(E.project,path,tUtil.save); E.path=path; E.modified=false
    E.status=L('saved')..' '..tUtil.getShortName(path)
    tUtil.showMessage(E.status,4)
    -- Saving the same file again must restart the confirmation duration too.
    tUtil.tTimerOverlay:set(4); tUtil.tTimerOverlay:restart()
    return true
end
local function exportOneImpl(path,portable,crop)
    local r=assert(Model.region(E.project,E.selected),L('select_region'))
    local asset=generate(r)
    if portable then Portable.save(asset,path,dpCall,crop) else assert(asset:save(path,false,false,true),L('export_failed')) end
    E.status=L('exported')..' '..path; return true
end
local function exportOne(path,portable)
    return Simplify.run(E,exportOneImpl,path,portable,E.portableCrop or false)
end
local function beginBatch(directory,portable)
    assert(#E.project.regions>0,L('select_region'))
    E.batch={portableTextures={},portableCrop=E.portableCrop or false,portable=portable,directory=directory,index=1,completed=0,failures={},project=Model.copy(E.project)}
end
local function batchStepImpl()
    local batch=E.batch; if not batch then return end
    local region=batch.project.regions[batch.index]
    if not region then
        E.status=string.format(L('batch_done'),batch.completed,#batch.failures)
        if #batch.failures>0 then E.status=E.status..' '..table.concat(batch.failures,'; ') end
        E.batch=nil; return
    end
    local ok,err=dpCall(function()
        local asset=generate(region,batch.project)
        local path=batch.directory..'/'..IO.exportName(region)
        if not batch.portable then assert(not IO.exists(path),L('file_exists')..' '..path) end
        if batch.portable then Portable.save(asset,path,dpCall,batch.portableCrop,batch.portableTextures) else assert(asset:save(path,false,false,true),L('export_failed')) end
    end)
    if ok then batch.completed=batch.completed+1 else batch.failures[#batch.failures+1]=region.name..': '..tostring(err) end
    batch.index=batch.index+1
end
local function batchStep()
    if E.meshTask or not E.batch then return end
    return Simplify.run(E,batchStepImpl)
end
local function requestReplace(fn)
    if E.modified then E.pending=fn; tImGui.OpenPopup('ime_discard') else dpCall(fn) end
end
local function finishPolygon()
    local points=E.polygon
    if action(function(p) local r=Model.fromPoints(p,points); E.selected=r.id; E.selection={[r.id]=true} end) then E.polygon={};E.stroke=nil;if E.tool=='freehand' or E.tool=='auto_contour' then E.tool='select' end end
end
applyProperties=function()
    local draft,values=E.draft,Model.copy(E.values)
    values.maxTriangles=2*values.maxVertices
    return action(function(p)
        local settings={}; for k in pairs(Model.defaults) do settings[k]=values[k] end
        if E.editDefaults then p.defaults=settings; return end
        for _,r in ipairs(p.regions) do if E.selection[r.id] then
            r.overrides={}; for k,v in pairs(settings) do if k=='backExternal' or k=='backSolid' or k=='backOpen' or k=='backRemap' or k=='backRelief' or v~=p.defaults[k] then r.overrides[k]=v end end
        end end
        local r=assert(Model.region(p,E.selected),L('select_region'))
        r.name=draft.name; r.x=draft.x; r.y=draft.y; r.w=draft.w; r.h=draft.h; r.shape=draft.shape; r.contour=Model.copy(draft.contour); r.backCrop=Model.copy(draft.backCrop); r.holes=Model.copy(draft.holes); r.heightAreas=Model.copy(draft.heightAreas)
    end)
end
local function menu()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu(tLang.L('menu_file')) then
            if tImGui.MenuItem(L('open_image')) then requestReplace(function() local path=mbm.openFile(E.project.image.path,table.unpack(tUtil.supported_images)); if path then openImage(path) end end) end
            if tImGui.MenuItem(L('open_project')) then requestReplace(function() local path=mbm.openFile(E.path or '', 'imesh'); if path then openProject(path) end end) end
            if tImGui.MenuItem(L('save'),'Ctrl+S') then dpCall(function() local path=E.path or mbm.saveFile('project.imesh','imesh'); if path then saveProject(path) end end) end
            if tImGui.MenuItem(L('save_as')) then dpCall(function() local path=mbm.saveFile(E.path or 'project.imesh','imesh'); if path then saveProject(path) end end) end
            if tImGui.MenuItem(L('export_selected')) then dpCall(function()
                local region=assert(Model.region(E.project,E.selected),L('select_region'))
                local path=mbm.saveFile(string.format('module_%03d.msh',region.id),'msh')
                if path then exportOne(path) end
            end) end
            if tImGui.MenuItem(L('export_all')) then dpCall(function() local path=mbm.openFolder(L('export_folder')); if path then beginBatch(path) end end) end
            tImGui.Separator()
            E.portableCrop=tImGui.Checkbox(L('export_portable_crop'),E.portableCrop or false)
            if tImGui.IsItemHovered() then tImGui.SetTooltip(L('export_portable_crop_help')) end
            if tImGui.MenuItem(L('export_portable_selected')) then dpCall(function()
                local region=assert(Model.region(E.project,E.selected),L('select_region'))
                local path=mbm.saveFile(string.format('module_%03d.msh',region.id),'msh')
                if path then exportOne(path,true) end
            end) end
            if tImGui.MenuItem(L('export_portable_all')) then dpCall(function()
                local path=mbm.openFolder(L('export_folder')); if path then beginBatch(path,true) end
            end) end
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
local function setEditMode(enabled)
    if E.editMode==enabled then return end
    Paint.cancel(E); Canvas.cancel(E); syncDraft(); E.orbitDrag=nil; E.panDrag=nil
    E.editMode=enabled
    Comparison.sync(E); Assembly.sync(E)
    if E.heightObject then E.heightObject.visible=enabled and E.heightView~=1 end
    Canvas.sync(E)
end
local function setComparison(sideBySide)
    if Comparison.select(E,sideBySide) then camera() end
end
local function setWireframe(enabled)
    if E.wireframe==enabled then return end
    if enabled and E.preview and not E.dirty then
        local ok=dpCall(Comparison.ensureWire,E)
        if not ok then Wire.release(E); if E.comparison then Wire.release(E.comparison) end; return end
    end
    if enabled and E.assembly and E.assembly.enabled then
        if not dpCall(Assembly.ensureWire,E) then return end
    end
    E.wireframe=enabled; Comparison.sync(E); Assembly.sync(E)
end
local function setAssembly(enabled)
    local a=Assembly.state(E)
    if a.enabled==enabled then return end
    if enabled then
        a.singleOrbit=Model.copy(E.orbit)
        releasePreview();a.enabled=true;a.fitPending=true
    else
        Assembly.release(E);a.enabled=false
        if a.singleOrbit then E.orbit=Model.copy(a.singleOrbit);camera() end
        E.viewRegion=E.selected
    end
    E.dirty=true;Assembly.sync(E)
end
local function addPrimitive()
    if not E.texture or not E.editMode then return false end
    local spec=E.primitive
    Paint.cancel(E); Paint.state(E).enabled=false
    Canvas.cancel(E)
    local o=Canvas.transform(E)
    local cx=((E.sidebar+E.screenW-E.rightbar)/2-o.x)/o.scale
    local cy=((E.screenH+25)/2-o.y)/o.scaleY
    local ok=action(function(p)
        local r=Model.primitive(p,spec.kind,spec.w,spec.h,spec.sides,cx,cy)
        E.selected=r.id; E.selection={[r.id]=true}; E.editDefaults=false
    end)
    if ok then E.tool='select'; E.status=L('primitive_added') end
    return ok
end
local function primitivePanel()
    tImGui.Text(L('add_primitive'))
    local types={'rectangle','circle','ellipse','triangle','regular'}
    local names={L('rectangle'),L('circle'),L('ellipse'),L('triangle'),L('regular')}
    local spec=E.primitive; local index=1
    for i,kind in ipairs(types) do if kind==spec.kind then index=i end end
    local changed,value=tImGui.Combo(L('primitive_type'),index,names)
    if changed then
        spec.kind=types[value]
        if spec.kind=='circle' then spec.w=math.min(spec.w,E.project.image.width,E.project.image.height) end
    end
    local maxWidth=spec.kind=='circle' and math.min(E.project.image.width,E.project.image.height) or E.project.image.width
    changed,value=tImGui.InputInt(L(spec.kind=='circle' and 'diameter' or 'crop_w')..'##new',spec.w,1,10)
    if changed then spec.w=Model.clampNumber(value,math.min(2,maxWidth),maxWidth,spec.w,true) end
    if spec.kind~='circle' then
        changed,value=tImGui.InputInt(L('crop_h')..'##new',spec.h,1,10)
        if changed then spec.h=Model.clampNumber(value,math.min(2,E.project.image.height),E.project.image.height,spec.h,true) end
    end
    if spec.kind=='regular' then
        changed,value=tImGui.SliderInt(L('sides'),spec.sides,3,32)
        if changed then spec.sides=Model.clampNumber(value,3,32,spec.sides,true) end
    end
    if tImGui.Button(L('add_primitive')..'##add') then addPrimitive() end
    tImGui.Separator()
end
local function propertiesPanel()
    tImGui.PushItemWidth(math.max(90,tImGui.GetContentRegionAvail().x*0.42))
    local defaults=tImGui.Checkbox(L('edit_defaults'),E.editDefaults or false)
    if defaults~=(E.editDefaults or false) then E.editDefaults=defaults; syncDraft() end
    if E.draft or E.editDefaults then
        if not E.editDefaults then
            local d=E.draft
            local edited,value=tImGui.InputText(L('name'),d.name); if edited then d.name=value end
        end
        tImGui.Separator()
        Presets.draw(E,action,dpCall)
        if not E.editDefaults then
            local d=E.draft
            if tImGui.CollapsingHeader(L('crop_group')) then
            for _,key in ipairs({'x','y','w','h'}) do
                local horizontal=key=='x' or key=='w'
                local size=horizontal and E.project.image.width or E.project.image.height
                local position=key=='x' or key=='y'
                local lo=position and 0 or 1
                local hi=position and size-d[horizontal and 'w' or 'h'] or size-d[horizontal and 'x' or 'y']
                local c,v=tImGui.InputInt(L('crop_'..key),d[key],1,10)
                if c then d[key]=Model.clampNumber(v,lo,hi,d[key],true) end
            end
            local shapes={'rectangle','ellipse','polygon'}; local idx=d.shape=='rectangle' and 1 or (d.shape=='ellipse' and 2 or 3)
            local c,v=tImGui.Combo(L('shape'),idx,{L('rectangle'),L('ellipse'),L('polygon')})
            if c then d.shape=shapes[v]; if d.shape=='polygon' and not d.contour then d.contour={{x=0,y=0},{x=1,y=0},{x=1,y=1},{x=0,y=1}} end end
            if d.shape=='polygon' and tImGui.CollapsingHeader(L('points')) then
                local change,index=tImGui.SliderInt(L('point'),E.point,1,#d.contour); if change then E.point=Model.clampNumber(index,1,#d.contour,E.point,true) end
                local point=d.contour[E.point]
                for _,axis in ipairs({'x','y'}) do local modified,n=tImGui.InputFloat(axis,point[axis],0.01,0.1,'%.3f'); if modified then point[axis]=Model.clampNumber(n,0,1,point[axis]) end end
                if tImGui.Button(L('add_point')) and #d.contour<128 then local nextPoint=d.contour[E.point%#d.contour+1]
                    table.insert(d.contour,E.point+1,{x=(point.x+nextPoint.x)/2,y=(point.y+nextPoint.y)/2}); E.point=E.point+1
                end
                tImGui.SameLine(); if tImGui.Button(L('remove_point')) and #d.contour>3 then table.remove(d.contour,E.point); E.point=math.min(E.point,#d.contour) end
            end
            end
        end
        if tImGui.CollapsingHeader(L('grooves_group')) then
            Areas.modePanel(E)
            local manual=E.values.heightSource=='manual'
            local imagePreview=E.editMode and not E.editDefaults
            if imagePreview then
                if manual and E.heightView==3 then E.heightView=2 end
                local views={L('original_image'),L('height_map')}
                if not manual then views[3]=L('groove_overlay') end
                local change,view=tImGui.Combo(L('height_view'),E.heightView,views)
                if change then E.heightView=view end
                Help.show('view_'..E.heightView)
            end
            Areas.panel(E,action,function() if draftChanged() then return applyProperties() end return true end)
            tImGui.Separator()
            local original=imagePreview and E.heightView==1
            local map=imagePreview and E.heightView==2
            local overlay=imagePreview and E.heightView==3
            local geometry=not map and not overlay
            if geometry then E.values.followImage=tImGui.Checkbox(L('followImage'),E.values.followImage); Help.show('adaptive') end
            if not manual and not overlay then E.values.twoLevels=tImGui.Checkbox(L('twoLevels'),E.values.twoLevels); Help.show('twoLevels') end
            if not manual then E.values.invert=tImGui.Checkbox(L('invert'),E.values.invert); Help.show('invert') end
            for _,key in ipairs({'grooveThreshold','grooveTransition','heightTolerance'}) do
                local visible=(key=='grooveThreshold' and (overlay or E.values.twoLevels or (geometry and E.values.followImage))) or
                    (key=='grooveTransition' and not overlay and E.values.twoLevels) or
                    (key=='heightTolerance' and geometry and E.values.followImage)
                if visible and not original and (not manual or key=='heightTolerance') then
                    local lo=key=='grooveThreshold' and 0 or 0.001
                    local c,v=tImGui.SliderFloat(L(key),E.values[key],lo,1)
                    Help.show(key)
                    if c then E.values[key]=Model.clampOption(key,v,E.values[key]) end
                end
            end
            if not original and not manual then
                local c,v=tImGui.SliderInt(L('smoothPasses'),E.values.smoothPasses,0,4)
                Help.show('smoothPasses')
                if c then E.values.smoothPasses=Model.clampOption('smoothPasses',v,E.values.smoothPasses) end
            end
            local help='grooves_help'
            if map then help='height_map_help' elseif overlay then help='groove_overlay_help' end
            tImGui.TextWrapped(L(help))
            if imagePreview then
                if E.heightView~=1 and tImGui.Button(L('preview_adjustments')) then E.heightRequested=true end
                if E.heightError then tImGui.TextWrapped(E.heightError) end
            end
        end
        Holes.panel(E,action,function() if draftChanged() then return applyProperties() end return true end)
        Paint.panel(E,action,function() if draftChanged() then return applyProperties() end return true end)
        if tImGui.CollapsingHeader(L('volume_group')) then
        E.values.preserveAspect=tImGui.Checkbox(L('preserveAspect'),E.values.preserveAspect)
        if E.values.preserveAspect and E.draft and not E.editDefaults then
            E.values.height=E.values.width*math.max(1,E.draft.h-1)/math.max(1,E.draft.w-1)
        end
        for _,key in ipairs({'width','height','depth','relief','borderWidth'}) do
            if key=='height' and E.values.preserveAspect then
                if not E.editDefaults then tImGui.Text(L(key)..': '..string.format('%.3f',E.values.height)) end
            else
                local step=key=='borderWidth' and 0.05 or 0.1
                local c,v=tImGui.InputFloat(L(key),E.values[key],step,step*10,'%.3f'); if c then E.values[key]=Model.clampOption(key,v,E.values[key]) end
                if key=='relief' or key=='borderWidth' or key=='depth' then Help.show(key) end
            end
        end
        E.values.lockBorder=tImGui.Checkbox(L('lockBorder'),E.values.lockBorder)
        Help.show('lockBorder')
        end
        BackUv.panel(E,function() if draftChanged() then return applyProperties() end return true end,dpCall)
        Sides.panel(E,function() if draftChanged() then return applyProperties() end return true end,dpCall)
        if tImGui.CollapsingHeader(L('resolution_group')) then
        for _,key in ipairs({'columns','rows','ellipseSegments'}) do
            local c,v=tImGui.InputInt(L(key),E.values[key],1,10); if c then E.values[key]=Model.clampOption(key,v,E.values[key]) end
        end
        local c,v=tImGui.InputInt(L('maxVertices'),E.values.maxVertices)
        if c then E.values.maxVertices=Model.clampOption('maxVertices',v,E.values.maxVertices) end
        tImGui.TextWrapped(L('vertex_budget_help'))
        tImGui.Text(string.format(L('triangle_budget_auto'),2*E.values.maxVertices))
        end
        if tImGui.CollapsingHeader(tLang.L('simplify_geometry')) then
            E.values.simplify=tImGui.Checkbox(L('simplify_after'),E.values.simplify)
            if E.values.simplify then
                local c,v=tImGui.DragFloat(tLang.L('simplify_ratio'),E.values.simplifyRatio,0.001,0.001,0.95,'%.3f',tImGui.Flags('ImGuiSliderFlags_AlwaysClamp'))
                if c then E.values.simplifyRatio=Model.clampOption('simplifyRatio',v,E.values.simplifyRatio) end
                if E.report and not E.editDefaults then
                    local source=E.report.sourceTriangles or E.report.triangles
                    tImGui.Text(string.format(tLang.L('simplify_estimate_fmt'),source,math.max(1,math.floor(source*E.values.simplifyRatio))))
                    if tImGui.IsItemHovered() then tImGui.SetTooltip(L('simplify_estimate_hint')) end
                else tImGui.TextWrapped(L('simplify_estimate_pending')) end
                E.values.simplifyDetails=tImGui.Checkbox(tLang.L('simplify_preserve_details'),E.values.simplifyDetails)
                if tImGui.IsItemHovered() then tImGui.SetTooltip(tLang.L('simplify_preserve_details_tooltip')) end
                c,v=tImGui.SliderFloat(tLang.L('simplify_boundary_threshold'),E.values.simplifyBoundary,0,0.25,'%.3f')
                if c then E.values.simplifyBoundary=Model.clampOption('simplifyBoundary',v,E.values.simplifyBoundary) end
                if tImGui.IsItemHovered() then tImGui.SetTooltip(tLang.L('simplify_boundary_threshold_tooltip')) end
                tImGui.TextWrapped(L('simplify_help'))
            end
            if E.report and E.report.simplification then
                tImGui.Text(string.format(tLang.L('simplify_success_fmt'),E.report.sourceTriangles,E.report.triangles))
                if E.comparison and not E.editMode and not E.dirty then
                    tImGui.Separator(); tImGui.Text(L('comparison'))
                    local sideBySide=tImGui.Checkbox(L('comparison_side_by_side'),E.compareSideBySide)
                    if sideBySide~=E.compareSideBySide then dpCall(setComparison,sideBySide) end
                    if E.compareSideBySide then
                        local originalOnLeft=math.cos(E.orbit.azimuth)<0
                        local source=E.comparison
                        local original=tImGui.Checkbox(L(originalOnLeft and 'comparison_show_original_left' or 'comparison_show_original_right')..'###ime_show_original',source.showOriginal)
                        local simplified=tImGui.Checkbox(L(originalOnLeft and 'comparison_show_simplified_right' or 'comparison_show_simplified_left')..'###ime_show_simplified',source.showSimplified)
                        if original~=source.showOriginal or simplified~=source.showSimplified then Comparison.visibility(E,original,simplified) end
                    end
                    setWireframe(tImGui.Checkbox(L('wireframe')..'##comparison',E.wireframe))
                    tImGui.Text(string.format(L('comparison_counts'),E.report.sourceVertices,E.report.vertices,
                        E.report.sourceTriangles,E.report.triangles,100*(1-E.report.triangles/E.report.sourceTriangles)))
                    tImGui.Text(string.format(L('comparison_error'),E.report.simplification.maximumGeometricError,
                        E.report.simplification.maximumRelativeError*100))
                    if tImGui.IsItemHovered() then tImGui.SetTooltip(L('comparison_error_help')) end
                    tImGui.TextWrapped(L('comparison_export'))
                end
            end
        end
        if tImGui.Button(L('apply')) then applyProperties() end
        if not E.editDefaults then
            tImGui.SameLine(); if tImGui.Button(L('inherit')) then action(function(p) for _,r in ipairs(p.regions) do if E.selection[r.id] then r.overrides={} end end end) end
        end
    else tImGui.Text(L('select_region')) end
    tImGui.PopItemWidth()
end
local function regionsPanel()
    tImGui.SetNextWindowPos({x=0,y=25},E.flags.always)
    tImGui.SetNextWindowSize({x=E.sidebar,y=E.screenH-25},E.flags.always)
    if tImGui.Begin(L('regions')..'###ime_regions',false,E.flags.fixed) then
        tImGui.PushItemWidth(135)
        local value=tImGui.Checkbox(L('edit_mode'),E.editMode)
        if value~=E.editMode then setEditMode(value) end
        if E.editMode then
            local names={L('select'),L('rectangle'),L('ellipse'),L('polygon'),L('freehand_title'),L('auto_title'),L('pan')}; local tools={'select','rectangle','ellipse','polygon','freehand','auto_contour','pan'}
            if BackUv.available(E) then names[#names+1]=L('back_edit'); tools[#tools+1]='back_uv' end
            if Sides.available(E) then names[#names+1]=L('side_edit');tools[#tools+1]='side_band' end
            if E.tool=='auto_background' then names[#names+1]=L('auto_pick');tools[#tools+1]=E.tool end
            if Areas.active(E) then names[#names+1]=tLang.L('ime_areas_title');tools[#tools+1]=E.tool end
            if E.tool=='hole_freehand' then names[#names+1]=L('freehand_hole');tools[#tools+1]=E.tool end
            if E.tool=='holes' or E.tool=='hole_draw' then names[#names+1]=L('holes_'..(E.tool=='holes' and 'edit' or 'draw'));tools[#tools+1]=E.tool end
            local index=1; for i,name in ipairs(tools) do if name==E.tool then index=i end end
            if E.paint and E.paint.enabled then names[#names+1]=L('paint_title'); index=#names end
            tImGui.SetNextItemWidth(210)
            local modified,tool=tImGui.Combo(L('tool'),index,names)
            if modified and tools[tool] then Paint.cancel(E); Paint.state(E).enabled=false; Canvas.cancel(E); E.tool=tools[tool]; if E.tool=='back_uv' or E.tool=='side_band' then E.heightView=1 end end
            if E.tool=='auto_contour' or E.tool=='auto_background' then Auto.panel(E,finishPolygon,function() Holes.finish(E,action) end) end
            if E.tool=='freehand' then Freehand.panel(E,finishPolygon,function() Canvas.cancel(E) end) end
            if E.tool=='polygon' then
                if tImGui.Button(L('finish_polygon')) then finishPolygon() end
                tImGui.SameLine(); if tImGui.Button(L('cancel')) then Canvas.cancel(E) end
            end
            if tImGui.Button(L('fit_image')) then Canvas.fit(E) end
            tImGui.TextWrapped(L((E.tool=='holes' or E.tool=='hole_draw') and 'holes_help' or E.tool=='side_band' and 'side_canvas_help' or (E.tool=='back_uv' and 'back_canvas_help' or (E.paint and E.paint.enabled and 'paint_canvas_help' or 'canvas_help'))))
        else
            tImGui.TextWrapped(L('preview_help'))
            setWireframe(tImGui.Checkbox(L('wireframe'),E.wireframe))
        end
        Assembly.panel(E,setAssembly,camera)
        if E.draft then
            if E.report and not E.drag then
                tImGui.TextWrapped(string.format(L('faces_compact'),E.draft.name,compactCount(E.report.triangles)))
                if tImGui.IsItemHovered() then
                    tImGui.BeginTooltip(); tImGui.Text(string.format(L('counts'),E.report.vertices,E.report.triangles)); tImGui.EndTooltip()
                end
            elseif not E.generationFailure then tImGui.Text(L(E.meshTask and 'faces_calculating' or 'faces_pending')) end
            if E.editMode and not E.report and not E.drag and not E.paintDrag then
                if tImGui.Button(L('faces_calculate')) then
                    if not draftChanged() or applyProperties() then E.statisticsRequested=true end
                end
                if tImGui.IsItemHovered() then tImGui.SetTooltip(L('faces_calculate_help')) end
            end
        end
        if E.generationFailure then tImGui.TextWrapped(E.generationFailure) end
        tImGui.Separator()
        if E.missing then
            tImGui.Text(L('missing_image'))
            if tImGui.Button(L('relink')) then dpCall(function() local path=mbm.openFile('',table.unpack(tUtil.supported_images)); if path then relink(path) end end) end
            tImGui.SameLine(); if tImGui.Button(L('cancel')) then E.missing=nil end
        end
        if E.texture then
            if E.editMode then primitivePanel() end
            tImGui.Text(tUtil.getShortName(E.project.image.path))
            if tImGui.CollapsingHeader(L('grid')) then
                for _,key in ipairs({'columns','rows','marginX','marginY','gapX','gapY'}) do
                    local count=key=='columns' or key=='rows'
                    local hi=count and 64 or E.project.image[key:sub(-1)=='X' and 'width' or 'height']
                    local edit,value=tImGui.InputInt(L('grid_'..key),E.grid[key],1,10)
                    if edit then E.grid[key]=Model.clampNumber(value,count and 1 or 0,hi,E.grid[key],true) end
                end
                if tImGui.Button(L('create_grid')) then action(function(p) local ids=Model.grid(p,E.grid); E.selected=ids[1]; E.selection={}; for _,id in ipairs(ids) do E.selection[id]=true end end) end
            end
            if tImGui.Button(L('duplicate')) then action(function(p)
                local originals=Model.copy(p.regions); local selection=E.selection; E.selection={}; local first
                for _,r in ipairs(originals) do if selection[r.id] then local n=Model.add(p,r.shape,r.x,r.y,r.w,r.h,r.contour)
                    n.overrides=Model.copy(r.overrides); n.heightEdits=Model.copy(r.heightEdits); n.heightAreas=Model.copy(r.heightAreas); n.holes=Model.copy(r.holes); n.backCrop=Model.copy(r.backCrop); n.name=r.name:sub(1,123)..'_copy'; first=first or n.id; E.selected=first; E.selection[n.id]=true
                end end
            end) end
            tImGui.SameLine()
            if tImGui.Button(L('delete')) then action(function(p)
                for i=#p.regions,1,-1 do if E.selection[p.regions[i].id] then table.remove(p.regions,i) end end
                E.selected=p.regions[1] and p.regions[1].id or 0; E.selection={[E.selected]=true}
            end) end
            tImGui.Text(L('selection_help'))
            if tImGui.BeginChild('ime_region_list',{x=0,y=130},true,0) then
            for _,r in ipairs(E.project.regions) do
                if tImGui.Selectable(r.name..'##region'..r.id,E.selection[r.id] or false) then selectRegion(r.id,E.control) end
                if r.id==E.selected then
                    local mode=E.editDefaults and (r.overrides.heightSource or E.project.defaults.heightSource or 'image') or E.values.heightSource
                    Help.show('mode_'..mode)
                end
            end
            end
            tImGui.EndChild()
            tImGui.Separator(); propertiesPanel()
        else tImGui.Text(L('open_help')) end
        if E.simplifyProgress then tImGui.ProgressBar(E.simplifyProgress,{x=-1,y=0},string.format(tLang.L('simplify_progress_fmt'),E.simplifyProgress*100)) end
        if E.status~=E.generationFailure then tImGui.TextWrapped(E.status) end
        if E.batch then
            tImGui.ProgressBar((E.batch.index-1)/#E.batch.project.regions,{x=-1,y=0},L('exporting'))
            if tImGui.Button(L('cancel')) then E.status=L('cancelled'); E.batch=nil end
        end
        tImGui.PopItemWidth()
    end
    tImGui.End()
end
function onInitScene()
    E.screenW,E.screenH=mbm.getRealSizeScreen()
    E.flags={always=tImGui.Flags('ImGuiCond_Always'),auto=tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize'),
        fixed=tImGui.Flags('ImGuiWindowFlags_NoMove','ImGuiWindowFlags_NoResize','ImGuiWindowFlags_NoCollapse')}
    mbm.setColor(0.1,0.12,0.15); mbm.setLightEnabled('3d',true); mbm.setAmbientLight('3d',0.35,0.35,0.35)
    E.light=0.7; mbm.setDirectionalLight('3d',-0.4,-0.5,-1,E.light,E.light,E.light)
    E.camera2d=mbm.getCamera('2d'); mbm.setLightEnabled('2dw',false)
    E.previewCamera=mbm.getCamera('3d'); E.previewCamera:setAngleOfView(110); camera()
    local checker=tUtil.createAlphaPattern(256,256,16,{r=70,g=70,b=70},{r=100,g=100,b=100})
    E.checkerPath=checker
    Diagnostics.init(E); syncDraft(); tUtil.sMessageOverlay=L('welcome')
end
function onLoop(delta)
    if E.autoTask then dpCall(Auto.resume,E) end
    if E.meshTask then dpCall(Simplify.resume,E) end
    tImGui.BeginDisabled(E.meshTask~=nil or E.paintDrag~=nil); menu(); tImGui.EndDisabled()
    tImGui.BeginDisabled(E.meshTask~=nil or E.paintDrag~=nil)
    regionsPanel(); Diagnostics.draw(E,{camera=camera,setMode=setEditMode,fit=Canvas.fit,zoom=Canvas.zoom})
    tImGui.EndDisabled()
    if not E.meshTask and E.key and not tImGui.GetWantCaptureKeyboard() then
        if E.control and E.key==mbm.getKeyCode('Z') then history(false)
        elseif E.control and E.key==mbm.getKeyCode('Y') then history(true)
        elseif E.control and E.key==mbm.getKeyCode('S') and not E.paintDrag then dpCall(function() local path=E.path or mbm.saveFile('project.imesh','imesh'); if path then saveProject(path) end end)
        elseif E.key==mbm.getKeyCode('ESC') then Paint.cancel(E); Canvas.cancel(E); syncDraft() end
    end
    E.key=nil; rebuild(); updateStatistics(E.statisticsRequested); batchStep()
    Canvas.sync(E)
    HeightPreview.sync(E,dpCall)
    Paint.sync(E)
    tUtil.showOverlayMessage()
end
function onKeyDown(key)
    if key==mbm.getKeyCode('control') then E.control=true else E.key=key end
end
function onKeyUp(key) if key==mbm.getKeyCode('control') then E.control=false end end
local handlers={action=action,select=selectRegion,commitDrag=commitDrag,detect=function(...)
    local ok,handled=dpCall(Auto.input,E,...);return ok and handled
end}
local function sceneInput(x,y)
    -- The lower-right area is scene space when the light window is hidden.
    -- Let ImGui reject actual windows/popups instead of blocking an entire column.
    return x>=0 and x<E.screenW and y>=25 and y<E.screenH and not tImGui.GetWantCaptureMouse()
end
function onTouchDown(key,x,y)
    if E.meshTask then return end
    -- Scene callbacks arrive divided by the camera scale; UI and hit testing use framebuffer pixels.
    x=x*E.camera2d.sx; y=y*E.camera2d.sy
    if not sceneInput(x,y) then return end
    if E.editMode then
        if key==0 and E.tool~='pan' then
            local handled
            if Areas.active(E) and draftChanged() and not applyProperties() then return end
            if Paint.state(E).enabled and not E.editDefaults then
                if draftChanged() and not applyProperties() then return end
                handled=Paint.input(E,action,'down',x,y)
            else handled=Canvas.input(E,handlers,'down',x,y) end
            if (E.tool=='select' or E.tool=='back_uv' or E.tool=='side_band' or E.tool=='holes' or E.tool=='height_areas') and not handled then E.panDrag={x=x,y=y} end
        elseif key==0 then E.panDrag={x=x,y=y}
        elseif key==1 or key==2 then E.panDrag={x=x,y=y} end
    elseif key==0 then E.orbitDrag={x=x,y=y} end
end
function onTouchMove(key,x,y)
    if E.meshTask then return end
    -- Scene callbacks arrive divided by the camera scale; UI and hit testing use framebuffer pixels.
    x=x*E.camera2d.sx; y=y*E.camera2d.sy
    if E.panDrag then
        local c=E.camera2d
        c:setPos(c.x-(x-E.panDrag.x)/c.sx,c.y+(y-E.panDrag.y)/c.sy)
        E.panDrag={x=x,y=y}
    elseif E.orbitDrag then
        E.orbit.azimuth=E.orbit.azimuth-(x-E.orbitDrag.x)*0.01
        E.orbit.elevation=math.max(-1.5,math.min(1.5,E.orbit.elevation+(y-E.orbitDrag.y)*0.01))
        E.orbitDrag={x=x,y=y}; camera()
    elseif E.paintDrag or (E.editMode and Paint.state(E).enabled and sceneInput(x,y)) then Paint.input(E,action,'move',x,y)
    elseif E.drag or sceneInput(x,y) then Canvas.input(E,handlers,'move',x,y) end
end
function onTouchUp(key,x,y)
    -- Scene callbacks arrive divided by the camera scale; UI and hit testing use framebuffer pixels.
    x=x*E.camera2d.sx; y=y*E.camera2d.sy
    if key==0 then
        if E.paintDrag then Paint.input(E,action,'up',x,y) else Canvas.input(E,handlers,'up',x,y) end
        E.orbitDrag=nil; E.panDrag=nil
    end
    if key==1 or key==2 then E.panDrag=nil end
end
function onTouchZoom(zoom)
    if E.meshTask then return end
    if tImGui.GetWantCaptureMouse() then return end
    if E.editMode then
        local mouse=tImGui.GetMousePos()
        if sceneInput(mouse.x,mouse.y) then Canvas.zoom(E,E.zoom*math.exp(zoom*0.12),mouse.x,mouse.y) end
    else E.orbit.distance=math.max(0.01,E.orbit.distance*math.exp(-zoom*0.12)); camera() end
end
function onResizeWindow()
    E.screenW,E.screenH=mbm.getRealSizeScreen(); E.canvasDirty=true; camera()
end
function onEndScene() GeometryCache.clear(E); Paint.destroy(E); HeightPreview.destroy(E); releasePreview(); Canvas.destroy(E) end
if type(testApi)=='table' then
    testApi.auto=Auto; testApi.freehand=Freehand; testApi.paint=Paint; testApi.paintInput=function(kind,x,y) return Paint.input(E,action,kind,x,y) end
    testApi.areas=Areas
    testApi.holes=Holes
    testApi.setAssembly=setAssembly
    testApi.setComparison=setComparison
    testApi.state=E; testApi.openImage=openImage; testApi.openProject=openProject; testApi.saveProject=saveProject
    testApi.action=action; testApi.select=selectRegion; testApi.rebuild=rebuild; testApi.undo=history
    testApi.exportOne=exportOne; testApi.beginBatch=beginBatch; testApi.batchStep=batchStep; testApi.relink=relink
    testApi.updateHeightPreview=function() HeightPreview.sync(E,dpCall) end; testApi.updateStatistics=function() return updateStatistics(true) end; testApi.compactCount=compactCount; testApi.setWireframe=setWireframe; testApi.addPrimitive=addPrimitive; testApi.camera=camera; testApi.fit=Canvas.fit; testApi.setEditMode=setEditMode; testApi.applyProperties=applyProperties; testApi.finishPolygon=finishPolygon
end
