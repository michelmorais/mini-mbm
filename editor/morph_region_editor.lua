--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Generic polygon-region authoring for any MSH-backed mesh.
]]--

tImGui=require 'ImGui'
tUtil=require 'editor_utils'
tLang=require 'lang.language'
local tTopology=require 'morph_surface_topology'
local tPolygon=require 'morph_surface_polygon'
local tRegions=require 'morph_surface_regions'
local tProject=require 'morph_surface_project'
local tOverlay=require 'morph_surface_overlay'

local E={project=tProject.new(),projectPath=nil,mesh=nil,meshDebug=nil,topology=nil,
    meshBounds=nil,modified=false,status='',statusError=false,nameDraft='region',
    selectedPoint=nil,regionDrag=nil,cameraDrag=nil,cameraPan=nil,overlayDirty=true,
    regionOverlay={},regionNames={},regionNamesDirty=true,selectedRegionStatus='missing',
    regionStatusDirty=true,camera={fx=0,fy=0,fz=0,distance=10,azimuth=0.45,elevation=0.22},
    pendingAction=nil,confirmOpen=false}

local function L(key) return tLang.L('mre_'..key) end

local function setStatus(message,isError)
    E.status=message or ''
    E.statusError=isError==true
    tUtil.sMessageOverlay=E.status
    tUtil.bWarnMessage=E.statusError
end

local function markChanged()
    E.modified=true
    E.overlayDirty=true
    E.regionStatusDirty=true
end

local function applyCamera()
    if not E.mesh then return end
    local camera=mbm.getCamera('3d')
    local c=E.camera
    local horizontal=math.cos(c.elevation)*c.distance
    camera:setPos(c.fx+math.sin(c.azimuth)*horizontal,
        c.fy+math.sin(c.elevation)*c.distance,
        c.fz+math.cos(c.azimuth)*horizontal)
    camera:setFocus(c.fx,c.fy,c.fz)
end

local function fitCamera(bounds)
    if not bounds then return end
    local c=E.camera
    c.fx=(bounds.minX+bounds.maxX)*0.5
    c.fy=(bounds.minY+bounds.maxY)*0.5
    c.fz=(bounds.minZ+bounds.maxZ)*0.5
    local dx,dy,dz=bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ
    c.distance=math.max(math.sqrt(dx*dx+dy*dy+dz*dz)*1.5,0.1)
    c.azimuth=0.45
    c.elevation=0.22
    applyCamera()
end

local function destroyMesh()
    tOverlay.destroy(E)
    if E.mesh then pcall(function() E.mesh:destroy() end) end
    if E.meshDebug then pcall(function() E.meshDebug:destroy() end) end
    E.mesh=nil
    E.meshDebug=nil
    E.topology=nil
    E.meshBounds=nil
    E.regionStatusDirty=true
end

local function releaseObject(object)
    if object then pcall(function() object:destroy() end) end
end

local function loadMesh(path,preserveProject)
    if type(path)~='string' or path=='' then return false end
    local directory=path:match('^(.*)[/\\][^/\\]+$')
    if directory then mbm.addPath(directory) end
    local candidate=mesh:new('3d')
    local loadOk,loaded=pcall(function() return candidate:load(path) end)
    if not loadOk or loaded==false then
        releaseObject(candidate)
        setStatus(string.format(L('mesh_load_failed'),path),true)
        return false
    end
    local inspector=meshDebug:new()
    local inspectOk,inspected=pcall(function() return inspector:load(path) end)
    if not inspectOk or inspected~=true then
        releaseObject(candidate)
        releaseObject(inspector)
        setStatus(L('mesh_inspect_failed'),true)
        return false
    end
    local topologyOk,topology=pcall(tTopology.inspect,inspector)
    if not topologyOk or not topology or not topology.bounds then
        releaseObject(candidate)
        releaseObject(inspector)
        setStatus(L('mesh_inspect_failed'),true)
        return false
    end
    pcall(function() candidate:stopSkeletalAnimation() end)
    candidate.alwaysRender=true
    candidate.visible=true
    destroyMesh()
    E.mesh=candidate
    E.meshDebug=inspector
    E.topology=topology
    E.meshBounds=topology.bounds
    E.project.meshPath=path
    if not preserveProject then
        E.project=tProject.new()
        E.project.meshPath=path
        E.projectPath=nil
        E.modified=false
        E.regionNamesDirty=true
    end
    if E.project.topologySignature=='' or not preserveProject then
        E.project.topologySignature=topology.signature
    end
    fitCamera(topology.bounds)
    E.overlayDirty=true
    E.regionStatusDirty=true
    setStatus(E.project.topologySignature==topology.signature and L('mesh_loaded') or
        L('topology_changed'),E.project.topologySignature~=topology.signature)
    return true
end

local function rebuildRegion(region)
    if not E.topology or not region then return false end
    region.derivedVertexWeights={}
    region.geometryFingerprint=''
    region.topologySignature=E.topology.signature
    local ok,errorCode=tRegions.rebuild(region,E.topology)
    markChanged()
    if not ok then
        setStatus(L('region_rebuild_'..tostring(errorCode)),true)
        return false
    end
    setStatus(string.format(L('region_rebuilt'),#region.derivedVertexWeights),false)
    return true
end

local function loadProject(path)
    local project,errorMessage=tProject.load(path)
    if not project then setStatus(tostring(errorMessage),true); return false end
    local previousProject,previousPath,previousModified,previousPoint=
        E.project,E.projectPath,E.modified,E.selectedPoint
    E.project=project
    E.projectPath=path
    E.modified=false
    E.selectedPoint=nil
    E.regionNamesDirty=true
    E.regionStatusDirty=true
    if project.meshPath~='' then
        if not loadMesh(project.meshPath,true) then
            E.project,E.projectPath,E.modified,E.selectedPoint=
                previousProject,previousPath,previousModified,previousPoint
            E.regionNamesDirty=true
            E.overlayDirty=true
            return false
        end
    else
        destroyMesh()
    end
    E.project.selectedRegion=math.max(1,math.min(#E.project.regions>0 and #E.project.regions or 1,
        E.project.selectedRegion or 1))
    E.overlayDirty=true
    setStatus(L('project_loaded'),false)
    return true
end

local function saveProject(path)
    path=path or E.projectPath
    if not path or path=='' then return false end
    if E.mesh and E.topology then
        E.project.topologySignature=E.project.topologySignature~='' and
            E.project.topologySignature or E.topology.signature
    end
    E.project.selectedRegion=E.project.selectedRegion or 1
    local saved,errorMessage=tProject.save(E.project,path,tUtil.save)
    if not saved then setStatus(string.format(L('project_save_failed'),tostring(errorMessage)),true); return false end
    E.projectPath=path
    E.modified=false
    setStatus(L('project_saved'),false)
    return true
end

local function saveProjectAs()
    local path=mbm.saveFile(E.projectPath or 'morph-project.morph.lua','*.morph.lua')
    if path and path~='' then return saveProject(path) end
    return false
end

local function performPending()
    local action=E.pendingAction
    E.pendingAction=nil
    E.confirmOpen=false
    if not action then return end
    if action.kind=='mesh' then
        loadMesh(action.path,false)
    elseif action.kind=='project' then
        loadProject(action.path)
    end
end

local function requestAction(kind,path)
    if E.modified then
        E.pendingAction={kind=kind,path=path}
        E.confirmOpen=true
        return
    end
    E.pendingAction={kind=kind,path=path}
    performPending()
end

local function openMeshDialog()
    local path=mbm.openFile(E.project.meshPath or '','*.msh')
    if path and path~='' then requestAction('mesh',path) end
end

local function openProjectDialog()
    local path=mbm.openFile(E.projectPath or '','*.morph.lua')
    if path and path~='' then requestAction('project',path) end
end

local function newRegionId()
    local suffix=1
    while true do
        local id='region_'..tostring(suffix)
        local exists=false
        for _,region in ipairs(E.project.regions) do
            if region.id==id then exists=true; break end
        end
        if not exists then return id end
        suffix=suffix+1
    end
end

local function addRegion()
    if not E.topology then setStatus(L('load_mesh_first'),true); return end
    local name=E.nameDraft:match('^%s*(.-)%s*$')
    if name=='' then name=string.format(L('region_default_name'),#E.project.regions+1) end
    local region=tRegions.new(name,'polygon',{})
    region.closed=false
    region.id=newRegionId()
    region.topologySignature=E.topology.signature
    E.project.regions[#E.project.regions+1]=region
    E.project.selectedRegion=#E.project.regions
    E.selectedPoint=nil
    E.regionNamesDirty=true
    markChanged()
    setStatus(L('region_created'),false)
end

local function selectedRegion()
    return E.project.regions[E.project.selectedRegion]
end

local function refreshRegionNames()
    if not E.regionNamesDirty then return end
    local names={}
    for _,region in ipairs(E.project.regions) do names[#names+1]=region.name end
    E.regionNames=names
    E.regionNamesDirty=false
end

local function containsAnchor(region,index,except)
    for pointIndex,point in ipairs(region.points or {}) do
        if pointIndex~=except and point.index==index then return true end
    end
    return false
end

local function pickPoint(x,y)
    if not E.topology then return false end
    local region=selectedRegion()
    if not region then return false end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,x,y)
    if not ok or not ox or not dx then return false end
    local origin,direction={x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz}
    local hit=tTopology.intersectRaySurface(E.topology,origin,direction)
    if not hit then return false end
    local resolved=tPolygon.resolvePoints(region,E.topology)
    local extent=math.sqrt((E.meshBounds.maxX-E.meshBounds.minX)^2+
        (E.meshBounds.maxY-E.meshBounds.minY)^2+(E.meshBounds.maxZ-E.meshBounds.minZ)^2)
    local radius=math.max(extent*0.024,0.001)
    local nearest,distance=tPolygon.nearestPoint(resolved,hit.point,radius)
    if nearest then
        E.selectedPoint=nearest
        E.regionDrag={pointIndex=nearest,changed=false}
        E.overlayDirty=true
        return true
    end
    local vertex=tTopology.nearestVertexOnSurfaceHit(E.topology,hit)
    if not vertex then return true end
    if containsAnchor(region,vertex.index) then return true end
    if region.closed then
        local edge,edgeDistance=tPolygon.nearestEdge(resolved,hit.point,true,radius)
        if not edge or edgeDistance>radius then return true end
        table.insert(region.points,edge+1,{index=vertex.index})
        E.selectedPoint=edge+1
        rebuildRegion(region)
    else
        region.points[#region.points+1]={index=vertex.index}
        E.selectedPoint=#region.points
        region.topologySignature=E.topology.signature
        region.derivedVertexWeights={}
        region.geometryFingerprint=''
        markChanged()
    end
    return true
end

local function updatePointDrag(x,y)
    local drag=E.regionDrag
    local region=selectedRegion()
    if not drag or not region or not E.topology then return end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,x,y)
    if not ok or not ox or not dx then return end
    local surfaceHit=tTopology.intersectRaySurface(E.topology,
        {x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz})
    local hit=tTopology.nearestVertexOnSurfaceHit(E.topology,surfaceHit)
    if not hit or containsAnchor(region,hit.index,drag.pointIndex) then return end
    local point=region.points[drag.pointIndex]
    if not point or point.index==hit.index then return end
    point.index=hit.index
    point.offset=nil
    drag.changed=true
    region.derivedVertexWeights={}
    region.geometryFingerprint=''
    E.overlayDirty=true
    E.regionStatusDirty=true
end

local function removeSelectedPoint()
    local region=selectedRegion()
    if not region or not E.selectedPoint then return end
    table.remove(region.points,E.selectedPoint)
    E.selectedPoint=nil
    if region.closed and #region.points<3 then region.closed=false end
    if region.closed then rebuildRegion(region)
    else
        region.derivedVertexWeights={}
        region.geometryFingerprint=''
        markChanged()
    end
end

local function deleteSelectedRegion()
    if not selectedRegion() then return end
    table.remove(E.project.regions,E.project.selectedRegion)
    E.project.selectedRegion=math.max(1,math.min(E.project.selectedRegion,#E.project.regions>0 and
        #E.project.regions or 1))
    E.selectedPoint=nil
    E.regionNamesDirty=true
    markChanged()
end

local function toggleClosed()
    local region=selectedRegion()
    if not region then return end
    if region.closed then
        region.closed=false
        region.derivedVertexWeights={}
        region.geometryFingerprint=''
        markChanged()
    elseif #region.points>=3 then
        region.closed=true
        rebuildRegion(region)
    else
        setStatus(L('need_three_points'),true)
    end
end

local function orbitCamera(x,y)
    if not E.cameraDrag then return end
    E.camera.azimuth=E.camera.azimuth+(E.cameraDrag.x-x)*0.008
    E.camera.elevation=math.max(-1.45,math.min(1.45,
        E.camera.elevation+(y-E.cameraDrag.y)*0.008))
    E.cameraDrag={x=x,y=y}
    applyCamera()
    E.overlayDirty=true
end

local function panCamera(x,y)
    if not E.cameraPan then return end
    local c=E.camera
    local scale=c.distance*0.001
    local dx=(E.cameraPan.x-x)*scale
    local dy=(y-E.cameraPan.y)*scale
    c.fx=c.fx+math.cos(c.azimuth)*dx
    c.fz=c.fz-math.sin(c.azimuth)*dx
    c.fy=c.fy+dy
    E.cameraPan={x=x,y=y}
    applyCamera()
end

local function drawConfirmPopup()
    if E.confirmOpen then tImGui.OpenPopup('##mreUnsaved') end
    if tImGui.BeginPopupModal('##mreUnsaved',false,0) then
        tImGui.TextWrapped(L('unsaved_changes'))
        if tImGui.Button(L('save_and_continue')) then
            local saved=false
            if E.projectPath then saved=saveProject(E.projectPath)
            else saved=saveProjectAs() end
            if saved then tImGui.CloseCurrentPopup(); performPending() end
        end
        tImGui.SameLine()
        if tImGui.Button(L('discard_and_continue')) then
            tImGui.CloseCurrentPopup()
            performPending()
        end
        tImGui.SameLine()
        if tImGui.Button(L('cancel')) then
            E.pendingAction=nil
            E.confirmOpen=false
            tImGui.CloseCurrentPopup()
        end
        tImGui.EndPopup()
    end
end

local function drawControls()
    tImGui.Begin(L('title')..'###MorphRegionEditor',true,0)
    if tImGui.Button(L('open_mesh')) then openMeshDialog() end
    tImGui.SameLine()
    if tImGui.Button(L('open_project')) then openProjectDialog() end
    tImGui.SameLine()
    if tImGui.Button(L('save')) then
        if E.projectPath then saveProject(E.projectPath) else saveProjectAs() end
    end
    tImGui.SameLine()
    if tImGui.Button(L('save_as')) then saveProjectAs() end
    if E.mesh then
        tImGui.Text(string.format(L('mesh_info'),E.project.meshPath,E.topology.vertexCount))
    else
        tImGui.TextWrapped(L('load_mesh_help'))
    end
    tImGui.Separator()
    tImGui.TextWrapped(L('interaction_help'))
    if E.topology and E.project.topologySignature==E.topology.signature then
        local changed,name=tImGui.InputText(L('new_region_name'),E.nameDraft)
        if changed then E.nameDraft=name end
        if tImGui.Button(L('add_region')) then addRegion() end
        if #E.project.regions>0 then
            refreshRegionNames()
            local names=E.regionNames
            local index=math.max(1,math.min(#names,E.project.selectedRegion))
            local selectionChanged,selected=tImGui.Combo(L('select_region'),index,names)
            if selectionChanged then
                E.project.selectedRegion=selected
                E.selectedPoint=nil
                E.overlayDirty=true
                E.regionStatusDirty=true
            end
            local region=selectedRegion()
            tImGui.Text(string.format(L('region_point_count'),#region.points))
            tImGui.Text(string.format(L('region_weight_count'),#region.derivedVertexWeights))
            tImGui.Text(L('region_status_'..E.selectedRegionStatus))
            if tImGui.Button(region.closed and L('open_contour') or L('close_contour')) then
                toggleClosed()
            end
            tImGui.SameLine()
            tImGui.BeginDisabled(not E.selectedPoint)
            if tImGui.Button(L('remove_point')) then removeSelectedPoint() end
            tImGui.EndDisabled()
            tImGui.SameLine()
            if tImGui.Button(L('delete_region')) then deleteSelectedRegion() end
            tImGui.BeginDisabled(not region.closed or #region.points<3)
            if tImGui.Button(L('rebuild_region')) then rebuildRegion(region) end
            tImGui.EndDisabled()
        end
    elseif E.topology then
        tImGui.TextWrapped(L('topology_changed'))
    end
    if E.status~='' then
        if E.statusError then tImGui.TextColored({r=1,g=0.35,b=0.3,a=1},E.status)
        else tImGui.TextWrapped(E.status) end
    end
    tImGui.End()
    drawConfirmPopup()
end

function onInitScene()
    mbm.setColor(0.12,0.12,0.14)
    local camera=mbm.getCamera('3d')
    camera:setFar(9999999)
    tUtil.setInitialWindowPositionLeft('MorphRegionEditor',0,25,390)
    local path=mbm.openFile('','*.msh')
    if path and path~='' then loadMesh(path,false) end
end

function onLoop()
    if E.regionStatusDirty then
        local region=selectedRegion()
        E.selectedRegionStatus=region and tRegions.status(region,E.topology) or 'missing'
        E.regionStatusDirty=false
    end
    if E.overlayDirty then
        if E.topology and E.project.topologySignature==E.topology.signature then
            tOverlay.rebuild(E)
        else
            tOverlay.destroy(E)
        end
        E.overlayDirty=false
    end
    drawControls()
    tUtil.showOverlayMessage()
end

function onTouchDown(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
    if key==0 then
        if not pickPoint(x,y) then
            E.cameraDrag={x=x,y=y}
        end
    elseif key==1 then
        E.cameraDrag={x=x,y=y}
    elseif key==2 then
        E.cameraPan={x=x,y=y}
    end
end

function onTouchMove(key,x,y)
    if E.regionDrag then updatePointDrag(x,y); return end
    if E.cameraDrag then orbitCamera(x,y); return end
    if E.cameraPan then panCamera(x,y) end
end

function onTouchUp(key)
    if key==0 and E.regionDrag then
        local drag=E.regionDrag
        E.regionDrag=nil
        local region=selectedRegion()
        if drag.changed and region and region.closed then rebuildRegion(region)
        elseif drag.changed then markChanged() end
        return
    end
    if key==0 or key==1 then E.cameraDrag=nil end
    if key==2 then E.cameraPan=nil end
end

function onTouchZoom(zoom)
    if tImGui.IsAnyWindowHovered() then return end
    E.camera.distance=math.max(0.01,E.camera.distance*(zoom>0 and 0.9 or 1.1))
    applyCamera()
end

function onKeyDown(key)
    if tImGui.GetWantCaptureKeyboard() then return end
    if key==mbm.getKeyCode('DELETE') then removeSelectedPoint() end
end

function onEndScene()
    destroyMesh()
end
