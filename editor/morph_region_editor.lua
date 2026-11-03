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
local tShapes=require 'morph_surface_shapes'
local tProject=require 'morph_surface_project'
local tTargets=require 'morph_surface_targets'
local tOverlay=require 'morph_surface_overlay'

local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[morph_region_editor] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end

local E={project=tProject.new(tLang.getLanguage()),projectPath=nil,mesh=nil,meshDebug=nil,topology=nil,
    meshBounds=nil,modified=false,status='',statusError=false,nameDraft='region',
    selectedPoint=nil,regionDrag=nil,cameraDrag=nil,cameraPan=nil,overlayDirty=true,
    regionOverlay={},regionNames={},regionNamesDirty=true,selectedRegionStatus='missing',
    regionStatusDirty=true,regionTableRows={},regionTableDirty=true,
    camera={fx=0,fy=0,fz=0,distance=10,azimuth=0.45,elevation=0.22},
    pendingAction=nil,confirmOpen=false,newShapeType=1,newShapeFree=false,
    pendingSurfacePick=nil,surfaceAxis=3,shapeMode=1,handleScale=1,
    worktree=1,targetMode=1,targetNameDraft='target',targetDx=0,targetDy=0,targetDz=0}

local function L(key) return tLang.L('mre_'..key) end

local function setStatus(message,isError)
    E.status=message or ''
    E.statusError=isError==true
    tUtil.sMessageOverlay=E.status
    tUtil.bWarnMessage=E.statusError
end

local function showHelpTooltip(message)
    if not message or message=='' or not tImGui.IsItemHovered(0) then return end
    tImGui.SetNextWindowSizeConstraints({x=280,y=0},{x=440,y=360})
    tImGui.BeginTooltip()
    tImGui.TextWrapped(message)
    tImGui.EndTooltip()
end

local function markChanged()
    E.modified=true
    E.overlayDirty=true
    E.regionStatusDirty=true
    E.regionTableDirty=true
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

local function saveEditorPreferences()
    local preferences=type(E.project.preferences)=='table' and E.project.preferences or {}
    preferences.handleScale=E.handleScale
    preferences.surfaceAxis=E.surfaceAxis
    preferences.shapeMode=E.shapeMode
    preferences.worktree=E.worktree
    preferences.targetMode=E.targetMode
    preferences.newShapeType=E.newShapeType
    preferences.newShapeFree=E.newShapeFree
    preferences.nameDraft=E.nameDraft
    preferences.targetNameDraft=E.targetNameDraft
    preferences.targetDx=E.targetDx
    preferences.targetDy=E.targetDy
    preferences.targetDz=E.targetDz
    if E.mesh then
        preferences.meshVisible=E.mesh.visible~=false
    elseif preferences.meshVisible==nil then
        preferences.meshVisible=true
    end
    preferences.camera={
        fx=E.camera.fx,fy=E.camera.fy,fz=E.camera.fz,
        distance=E.camera.distance,azimuth=E.camera.azimuth,
        elevation=E.camera.elevation,
    }
    E.project.language=tLang.getLanguage()
    E.project.preferences=preferences
end

local function restoreEditorPreferences(project)
    local preferences=project.preferences or {}
    E.handleScale=tonumber(preferences.handleScale) or 1
    E.surfaceAxis=math.max(1,math.min(3,math.floor(tonumber(preferences.surfaceAxis) or 3)))
    E.shapeMode=math.max(1,math.min(3,math.floor(tonumber(preferences.shapeMode) or 1)))
    E.worktree=math.max(1,math.min(2,math.floor(tonumber(preferences.worktree) or 1)))
    E.targetMode=math.max(1,math.min(3,math.floor(tonumber(preferences.targetMode) or 1)))
    E.newShapeType=math.max(1,math.min(3,math.floor(tonumber(preferences.newShapeType) or 1)))
    E.newShapeFree=preferences.newShapeFree==true
    E.nameDraft=type(preferences.nameDraft)=='string' and preferences.nameDraft~='' and
        preferences.nameDraft or 'region'
    E.targetNameDraft=type(preferences.targetNameDraft)=='string' and
        preferences.targetNameDraft~='' and preferences.targetNameDraft or 'target'
    E.targetDx=tonumber(preferences.targetDx) or 0
    E.targetDy=tonumber(preferences.targetDy) or 0
    E.targetDz=tonumber(preferences.targetDz) or 0
    if E.mesh then E.mesh.visible=preferences.meshVisible~=false end
    local camera=preferences.camera
    if type(camera)=='table' then
        E.camera.fx=tonumber(camera.fx) or E.camera.fx
        E.camera.fy=tonumber(camera.fy) or E.camera.fy
        E.camera.fz=tonumber(camera.fz) or E.camera.fz
        E.camera.distance=math.max(tonumber(camera.distance) or E.camera.distance,0.1)
        E.camera.azimuth=tonumber(camera.azimuth) or E.camera.azimuth
        E.camera.elevation=math.max(-1.45,math.min(1.45,
            tonumber(camera.elevation) or E.camera.elevation))
        applyCamera()
    end
    tLang.setLanguage(project.language)
end

local function syncLanguagePreference()
    local language=tLang.getLanguage()
    if E.project.language==language then return end
    E.project.language=language
    E.modified=true
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

local function selectedRegionBounds()
    local region=E.project.regions[E.project.selectedRegion]
    if not region or not E.topology then return nil end
    local points=tPolygon.resolvePoints(region,E.topology)
    if #points==0 then return nil end
    local bounds={minX=math.huge,minY=math.huge,minZ=math.huge,
        maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge}
    for _,point in ipairs(points) do
        bounds.minX=math.min(bounds.minX,point.position.x)
        bounds.minY=math.min(bounds.minY,point.position.y)
        bounds.minZ=math.min(bounds.minZ,point.position.z)
        bounds.maxX=math.max(bounds.maxX,point.position.x)
        bounds.maxY=math.max(bounds.maxY,point.position.y)
        bounds.maxZ=math.max(bounds.maxZ,point.position.z)
    end
    local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
    local padding=math.max(extent*0.18,0.01)
    bounds.minX=bounds.minX-padding; bounds.maxX=bounds.maxX+padding
    bounds.minY=bounds.minY-padding; bounds.maxY=bounds.maxY+padding
    bounds.minZ=bounds.minZ-padding; bounds.maxZ=bounds.maxZ+padding
    return bounds
end

local function frameRegionCamera()
    fitCamera(selectedRegionBounds() or E.meshBounds)
end

local function placeShapeAtViewportCenter(region)
    if not region or not E.topology then return false end
    local ok,width,height=dpCall(mbm.getSizeScreen)
    if not ok or not width or not height then return false end
    local rayOk,ox,oy,oz,dx,dy,dz=dpCall(mbm.getPickRay,width*0.5,height*0.5)
    if not rayOk or not ox or not dx then return false end
    local hit=tTopology.intersectRaySurface(E.topology,{x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz})
    if not hit then return false end
    local extent=math.sqrt((E.meshBounds.maxX-E.meshBounds.minX)^2+
        (E.meshBounds.maxY-E.meshBounds.minY)^2+(E.meshBounds.maxZ-E.meshBounds.minZ)^2)
    tShapes.seed(region,hit.point,E.topology,E.camera,extent)
    return true
end

local function destroyMesh()
    tOverlay.destroy(E)
    if E.mesh then dpCall(function() E.mesh:destroy() end) end
    if E.meshDebug then dpCall(function() E.meshDebug:destroy() end) end
    E.mesh=nil
    E.meshDebug=nil
    E.topology=nil
    E.meshBounds=nil
    E.regionStatusDirty=true
    E.regionTableDirty=true
end

local function releaseObject(object)
    if object then dpCall(function() object:destroy() end) end
end

local function loadMesh(path,preserveProject)
    if type(path)~='string' or path=='' then return false end
    local directory=path:match('^(.*)[/\\][^/\\]+$')
    if directory then mbm.addPath(directory) end
    local candidate=mesh:new('3d')
    local loadOk,loaded=dpCall(function() return candidate:load(path) end)
    if not loadOk or loaded==false then
        releaseObject(candidate)
        setStatus(string.format(L('mesh_load_failed'),path),true)
        return false
    end
    local inspector=meshDebug:new()
    local inspectOk,inspected=dpCall(function() return inspector:load(path) end)
    if not inspectOk or inspected~=true then
        releaseObject(candidate)
        releaseObject(inspector)
        setStatus(L('mesh_inspect_failed'),true)
        return false
    end
    local topologyOk,topology=dpCall(tTopology.inspect,inspector)
    if not topologyOk or not topology or not topology.bounds then
        releaseObject(candidate)
        releaseObject(inspector)
        setStatus(L('mesh_inspect_failed'),true)
        return false
    end
    dpCall(function() candidate:stopSkeletalAnimation() end)
    candidate.alwaysRender=true
    candidate.visible=true
    destroyMesh()
    E.mesh=candidate
    E.meshDebug=inspector
    E.topology=topology
    E.meshBounds=topology.bounds
    E.project.meshPath=path
    if not preserveProject then
        E.project=tProject.new(tLang.getLanguage())
        E.project.meshPath=path
        E.projectPath=nil
        E.modified=false
        E.regionNamesDirty=true
        E.regionTableDirty=true
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

local function axisKey(axisIndex)
    return axisIndex==1 and 'x' or axisIndex==2 and 'y' or 'z'
end

local function axisBounds(bounds,axis)
    if axis=='x' then return bounds.minX,bounds.maxX end
    if axis=='y' then return bounds.minY,bounds.maxY end
    return bounds.minZ,bounds.maxZ
end

local function snapSelectedShapeToSurface(region)
    if not region or not E.topology or not E.meshBounds or #region.points==0 then
        return false,'missing'
    end
    local axis=axisKey(E.surfaceAxis)
    local minAxis,maxAxis=axisBounds(E.meshBounds,axis)
    local span=math.max(E.meshBounds.maxX-E.meshBounds.minX,
        E.meshBounds.maxY-E.meshBounds.minY,E.meshBounds.maxZ-E.meshBounds.minZ,1e-4)
    local resolved=tPolygon.resolvePoints(region,E.topology)
    if #resolved~=#region.points then return false,'invalid' end
    local rawByIndex={}
    for _,raw in ipairs(region.points) do rawByIndex[tonumber(raw.index)]=raw end
    local changes={}
    local changed=false
    for _,resolvedPoint in ipairs(resolved) do
        local raw=rawByIndex[resolvedPoint.index]
        local vertex=E.topology.vertices[resolvedPoint.index]
        if not raw or not vertex then return false,'invalid' end
        local positiveOrigin={x=resolvedPoint.position.x,y=resolvedPoint.position.y,
            z=resolvedPoint.position.z}
        positiveOrigin[axis]=maxAxis+span*2
        local negativeOrigin={x=resolvedPoint.position.x,y=resolvedPoint.position.y,
            z=resolvedPoint.position.z}
        negativeOrigin[axis]=minAxis-span*2
        local positiveDirection={x=0,y=0,z=0}; positiveDirection[axis]=-1
        local negativeDirection={x=0,y=0,z=0}; negativeDirection[axis]=1
        local positiveHit=tTopology.intersectRaySurface(E.topology,positiveOrigin,positiveDirection)
        local negativeHit=tTopology.intersectRaySurface(E.topology,negativeOrigin,negativeDirection)
        local targetHit
        if positiveHit and negativeHit then
            local positiveDistance=math.abs(positiveHit.point[axis]-resolvedPoint.position[axis])
            local negativeDistance=math.abs(negativeHit.point[axis]-resolvedPoint.position[axis])
            targetHit=positiveDistance<=negativeDistance and positiveHit or negativeHit
        else
            targetHit=positiveHit or negativeHit
        end
        if not targetHit then return false,'missing_surface' end
        local target={x=resolvedPoint.position.x,y=resolvedPoint.position.y,
            z=resolvedPoint.position.z}
        target[axis]=targetHit.point[axis]
        local offset={x=target.x-vertex.x,y=target.y-vertex.y,z=target.z-vertex.z}
        local old=raw.offset or {}
        if math.abs(offset.x-(tonumber(old.x) or 0))>1e-8 or
                math.abs(offset.y-(tonumber(old.y) or 0))>1e-8 or
                math.abs(offset.z-(tonumber(old.z) or 0))>1e-8 then changed=true end
        changes[#changes+1]={raw=raw,offset=offset}
    end
    if not changed then return true,'unchanged' end
    for _,change in ipairs(changes) do
        local offset=change.offset
        if math.abs(offset.x)<=1e-8 and math.abs(offset.y)<=1e-8 and math.abs(offset.z)<=1e-8 then
            change.raw.offset=nil
        else
            change.raw.offset=offset
        end
    end
    region.derivedVertexWeights={}
    region.geometryFingerprint=''
    markChanged()
    return true,'changed'
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
    E.regionTableDirty=true
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
    restoreEditorPreferences(E.project)
    E.project.selectedRegion=math.max(1,math.min(#E.project.regions>0 and #E.project.regions or 1,
        E.project.selectedRegion or 1))
    E.overlayDirty=true
    setStatus(L('project_loaded'),false)
    return true
end

local function saveProject(path)
    path=path or E.projectPath
    if not path or path=='' then return false end
    saveEditorPreferences()
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
    elseif action.kind=='quit' then
        mbm.quit()
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

local function shapeTypeFromIndex(index)
    return index==1 and 'square' or index==2 and 'ellipse' or 'polygon'
end

local function shapeTypeIndex(region)
    local kind=tShapes.type(region)
    return kind=='square' and 1 or kind=='ellipse' and 2 or 3
end

local function addRegion()
    if not E.topology then setStatus(L('load_mesh_first'),true); return end
    local name=E.nameDraft:match('^%s*(.-)%s*$')
    if name=='' then name=string.format(L('region_default_name'),#E.project.regions+1) end
    local shapeType=shapeTypeFromIndex(E.newShapeType)
    local region=tRegions.new(name,'polygon',{})
    region.shape=tShapes.new(shapeType,shapeType=='polygon' and true or E.newShapeFree)
    region.closed=false
    region.id=newRegionId()
    region.topologySignature=E.topology.signature
    E.project.regions[#E.project.regions+1]=region
    E.project.selectedRegion=#E.project.regions
    E.selectedPoint=nil
    E.regionNamesDirty=true
    markChanged()
    if placeShapeAtViewportCenter(region) then
        markChanged()
        setStatus(L('shape_seeded'),false)
    else
        setStatus(L('region_created'),false)
    end
end

local function changeSelectedShape(region,shapeType,isFree)
    if not region or not E.topology then return false end
    local changed=tShapes.change(region,shapeType,isFree,E.topology,E.camera)
    if changed then
        markChanged()
        E.selectedPoint=nil
        E.overlayDirty=true
        E.regionStatusDirty=true
    end
    return changed
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

local function refreshRegionTable()
    if not E.regionTableDirty then return end
    local rows={}
    for _,region in ipairs(E.project.regions or {}) do
        local kind=tShapes.type(region)
        rows[#rows+1]={name=region.name or '',
            type=kind=='square' and L('shape_square') or kind=='ellipse' and L('shape_ellipse') or
                L('shape_polygon'),
            mode=tShapes.isFree(region) and L('shape_free') or L('shape_locked'),
            points=#(region.points or {}),vertices=#(region.derivedVertexWeights or {}),
            status=tRegions.status(region,E.topology)}
    end
    E.regionTableRows=rows
    E.regionTableDirty=false
end

local function regionIndexById(id)
    for index,region in ipairs(E.project.regions or {}) do
        if region.id==id then return index,region end
    end
    return nil,nil
end

local function selectedTarget()
    return E.project.targets[E.project.selectedTarget]
end

local function targetRegion(target)
    if not target then return nil,nil end
    return regionIndexById(target.sourceRegionId)
end

local function targetStatus(target)
    local _,region=targetRegion(target)
    return tTargets.status(target,region,E.topology)
end

local function newTargetId()
    local suffix=1
    while true do
        local id='target_'..tostring(suffix)
        local exists=false
        for _,target in ipairs(E.project.targets or {}) do
            if target.id==id then exists=true; break end
        end
        if not exists then return id end
        suffix=suffix+1
    end
end

local function uniqueTargetName(name)
    local base=name~='' and name or string.format(L('target_default_name'),#E.project.targets+1)
    local candidate=base
    local suffix=2
    local function exists(value)
        for _,target in ipairs(E.project.targets or {}) do
            if target.name==value then return true end
        end
        return false
    end
    while exists(candidate) do
        candidate=base..'_'..tostring(suffix)
        suffix=suffix+1
    end
    return candidate
end

local function selectTarget(index)
    local target=E.project.targets[index]
    if not target then return end
    E.project.selectedTarget=index
    local regionIndex=targetRegion(target)
    if regionIndex then E.project.selectedRegion=regionIndex end
    E.selectedPoint=nil
    E.overlayDirty=true
    E.regionStatusDirty=true
    E.modified=true
end

local function createTargetFromRegion()
    local region=selectedRegion()
    if not region then setStatus(L('target_source_required'),true); return false end
    local status=tRegions.status(region,E.topology)
    if status~='ready' then
        setStatus(L('target_source_not_ready'),true)
        return false
    end
    local name=uniqueTargetName((E.targetNameDraft or ''):match('^%s*(.-)%s*$'))
    local target=tTargets.new(name,region,newTargetId())
    E.project.targets[#E.project.targets+1]=target
    E.project.selectedTarget=#E.project.targets
    E.targetMode=2
    markChanged()
    setStatus(string.format(L('target_created'),name),false)
    return true
end

local function rebuildSelectedTarget()
    local target=selectedTarget()
    local _,region=targetRegion(target)
    if not target or not region then setStatus(L('target_source_required'),true); return false end
    if tRegions.status(region,E.topology)~='ready' then
        setStatus(L('target_source_not_ready'),true)
        return false
    end
    local ok,errorCode=tTargets.rebuildFromRegion(target,region)
    if not ok then
        setStatus(L('target_rebuild_failed')..': '..tostring(errorCode),true)
        return false
    end
    markChanged()
    setStatus(L('target_rebuilt'),false)
    return true
end

local function applyTargetTranslation()
    local target=selectedTarget()
    if not target or targetStatus(target)~='ready' then
        setStatus(L('target_not_ready'),true)
        return false
    end
    local changed=tTargets.applyTranslation(target,E.targetDx,E.targetDy,E.targetDz)
    if changed==0 then
        setStatus(L('target_no_translation'),false)
        return true
    end
    markChanged()
    setStatus(string.format(L('target_translation_applied'),changed),false)
    return true
end

local function resetSelectedTargetDeltas()
    local target=selectedTarget()
    if not target then return false end
    if not tTargets.resetDeltas(target) then
        setStatus(L('target_already_neutral'),false)
        return true
    end
    markChanged()
    setStatus(L('target_reset'),false)
    return true
end

local function deleteSelectedTarget()
    if not selectedTarget() then return false end
    table.remove(E.project.targets,E.project.selectedTarget)
    E.project.selectedTarget=math.max(1,math.min(#E.project.targets>0 and #E.project.targets or 1,
        E.project.selectedTarget))
    E.overlayDirty=true
    markChanged()
    setStatus(L('target_deleted'),false)
    return true
end

local function containsAnchor(region,index,except)
    for pointIndex,point in ipairs(region.points or {}) do
        if pointIndex~=except and point.index==index then return true end
    end
    return false
end

local function pickPoint(x,y,allowSurfaceAuthoring)
    if not E.topology then return false end
    local region=selectedRegion()
    if not region then return false end
    local ok,ox,oy,oz,dx,dy,dz=dpCall(mbm.getPickRay,x,y)
    if not ok or not ox or not dx then return false end
    local origin,direction={x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz}
    local resolved=tPolygon.resolvePoints(region,E.topology)
    local extent=math.sqrt((E.meshBounds.maxX-E.meshBounds.minX)^2+
        (E.meshBounds.maxY-E.meshBounds.minY)^2+(E.meshBounds.maxZ-E.meshBounds.minZ)^2)
    local markerRadius=math.max(extent*0.007,0.001)
    local surfaceRadius=math.max(extent*0.024,0.001)
    if #region.points>0 and tShapes.isParametric(region) then
        local handle=tShapes.nearestHandleOnRay(tShapes.handles(region,E.topology,E.camera),
            origin,direction,markerRadius*1.15)
        if handle then
            E.regionDrag={mode=handle.kind=='center' and 'shape_center' or 'shape_handle',
                handleId=handle.id,base=tShapes.parameters(region,E.topology,E.camera),changed=false}
            E.overlayDirty=true
            return true
        end
    end
    if #region.points>0 and not tShapes.isParametric(region) then
        local pointHandles={}
        for index,point in ipairs(resolved) do
            pointHandles[#pointHandles+1]={id=index,position=point.position}
        end
        local handle=tShapes.nearestHandleOnRay(pointHandles,origin,direction,markerRadius*1.15)
        if handle then
            local selected=resolved[handle.id]
            E.selectedPoint=handle.id
            E.regionDrag={mode='point',pointIndex=handle.id,
                basePosition=selected and {x=selected.position.x,y=selected.position.y,z=selected.position.z},
                changed=false}
            E.overlayDirty=true
            return true
        end
    end
    if not allowSurfaceAuthoring then return false end
    local hit=tTopology.intersectRaySurface(E.topology,origin,direction)
    if not hit then return false end
    if #region.points==0 and tShapes.type(region)~='polygon' then
        tShapes.seed(region,hit.point,E.topology,E.camera,extent)
        E.selectedPoint=nil
        markChanged()
        setStatus(L('shape_seeded'),false)
        return true
    end
    if tShapes.isParametric(region) then
        return false
    end
    local vertex=tTopology.nearestVertexOnSurfaceHit(E.topology,hit)
    if not vertex then return true end
    if containsAnchor(region,vertex.index) then return true end
    if region.closed then
        local edge,edgeDistance=tPolygon.nearestEdge(resolved,hit.point,true,surfaceRadius)
        if not edge or edgeDistance>surfaceRadius then return true end
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
    local ok,ox,oy,oz,dx,dy,dz=dpCall(mbm.getPickRay,x,y)
    if not ok or not ox or not dx then return end
    if drag.mode=='shape_center' or drag.mode=='shape_handle' then
        -- Surface picking scans every triangle. Shape manipulation is a
        -- camera-plane operation, so keep this path constant-time per event.
        local planeHit=tShapes.intersectCameraPlane({x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz},
            drag.base and drag.base.center,E.camera)
        if not planeHit or not drag.base then return end
        local changed
        if drag.mode=='shape_center' then
            changed=tShapes.applyCenterDrag(region,drag.base,planeHit,E.topology)
        else
            changed=tShapes.applyResize(region,drag.base,drag.handleId,planeHit,E.topology)
        end
        if changed then
            drag.changed=true
            E.overlayDirty=true
            E.regionStatusDirty=true
        end
        return
    end
    local point=region.points[drag.pointIndex]
    if not point or not drag.basePosition then return end
    -- Keep free-anchor dragging interactive. A triangle-ray intersection scans
    -- the complete mesh and is too expensive for every mouse-move event. The
    -- anchor moves on the camera plane while dragging and is snapped to the
    -- mesh once when the mouse button is released.
    local planeHit=tShapes.intersectCameraPlane({x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz},
        drag.basePosition,E.camera)
    if not planeHit then return end
    local vertex=E.topology.vertices[point.index]
    if not vertex then return end
    point.offset={x=planeHit.x-vertex.x,y=planeHit.y-vertex.y,z=planeHit.z-vertex.z}
    drag.changed=true
    region.derivedVertexWeights={}
    region.geometryFingerprint=''
    E.overlayDirty=true
    E.regionStatusDirty=true
end

local function snapDraggedPoint(drag,x,y,region)
    if not drag or drag.mode~='point' or not drag.changed or
            type(x)~='number' or type(y)~='number' then return end
    local ok,ox,oy,oz,dx,dy,dz=dpCall(mbm.getPickRay,x,y)
    if not ok or not ox or not dx then return end
    local surfaceHit=tTopology.intersectRaySurface(E.topology,
        {x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz})
    local hit=tTopology.nearestVertexOnSurfaceHit(E.topology,surfaceHit)
    if not hit or containsAnchor(region,hit.index,drag.pointIndex) then return end
    local point=region.points[drag.pointIndex]
    if not point then return end
    point.index=hit.index
    point.offset=nil
end

local function removeSelectedPoint()
    local region=selectedRegion()
    if not region or not E.selectedPoint then return end
    table.remove(region.points,E.selectedPoint)
    E.selectedPoint=nil
    if region.closed and #region.points<3 then region.closed=false end
    -- Removing an anchor invalidates the previous vertex selection, but must
    -- not scan the complete mesh from inside a click/key event. The explicit
    -- Rebuild selection button performs that expensive operation.
    region.derivedVertexWeights={}
    region.geometryFingerprint=''
    markChanged()
    if region.closed then setStatus(L('selection_needs_rebuild'),false) end
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
    if tShapes.isParametric(region) then
        setStatus(L('shape_always_closed'),true)
        return
    end
    if region.closed then
        region.closed=false
        region.derivedVertexWeights={}
        region.geometryFingerprint=''
        markChanged()
    elseif #region.points>=3 then
        region.closed=true
        region.derivedVertexWeights={}
        region.geometryFingerprint=''
        markChanged()
        setStatus(L('selection_needs_rebuild'),false)
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
    E.modified=true
end

local function panCamera(x,y)
    if not E.cameraPan then return end
    local c=E.camera
    local scale=c.distance*0.001
    local dx=(E.cameraPan.x-x)*scale
    local dy=(y-E.cameraPan.y)*scale
    c.fx=c.fx-math.cos(c.azimuth)*dx
    c.fz=c.fz+math.sin(c.azimuth)*dx
    c.fy=c.fy+dy
    E.cameraPan={x=x,y=y}
    applyCamera()
    E.modified=true
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

local function showMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(L('open_mesh')) then openMeshDialog() end
        if tImGui.MenuItem(L('open_project')) then openProjectDialog() end
        tImGui.Separator()
        if tImGui.MenuItem(L('save'),nil,false,E.modified) then
            if E.projectPath then saveProject(E.projectPath) else saveProjectAs() end
        end
        if tImGui.MenuItem(L('save_as'),nil,false,true) then saveProjectAs() end
        tImGui.Separator()
        if tImGui.MenuItem(tLang.L('menu_quit')) then requestAction('quit') end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        tLang.renderLanguageSubmenu()
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

local function showCameraPanel()
    tUtil.setInitialWindowPositionRight('MorphRegionCamera',0,25,330)
    tImGui.SetNextWindowSize({x=330,y=300},tImGui.Flags('ImGuiCond_Always'))
    tImGui.SetNextWindowSizeConstraints({x=330,y=300},{x=330,y=300})
    local opened=tImGui.Begin(L('camera')..'##MorphRegionCamera',true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if opened then
        local canFrameRegion=selectedRegionBounds()~=nil
        tImGui.BeginDisabled(not E.meshBounds)
        if tImGui.Button(L('frame_mesh')..'##mreFrameMesh') then
            fitCamera(E.meshBounds)
            E.modified=true
        end
        tImGui.SameLine()
        tImGui.BeginDisabled(not canFrameRegion)
        if tImGui.Button(L('frame_region')..'##mreFrameRegion') then
            frameRegionCamera()
            E.modified=true
        end
        tImGui.EndDisabled()
        tImGui.EndDisabled()
        tImGui.Separator()
        local meshVisible=E.mesh and E.mesh.visible or false
        local visible=tImGui.Checkbox(L('show_mesh')..'##mreShowMesh',meshVisible)
        if E.mesh and visible~=meshVisible then
            E.mesh.visible=visible
            E.modified=true
        end
        tImGui.Separator()
        if tUtil.drawOrbitGizmo(E.camera,{size=110}) then
            applyCamera()
            E.overlayDirty=true
            E.modified=true
        end
        tImGui.Separator()
        tImGui.Text(string.format(L('camera_distance_fmt'),E.camera.distance))
        tImGui.TextWrapped(L('camera_help'))
    end
    tImGui.End()
end

local function drawRegionTable()
    refreshRegionTable()
    if #E.regionTableRows==0 then return end
    tImGui.Separator()
    if tImGui.BeginTable('mreRegionTable',6,
            tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(L('table_name'))
        tImGui.TableSetupColumn(L('table_type'))
        tImGui.TableSetupColumn(L('table_mode'))
        tImGui.TableSetupColumn(L('table_points'))
        tImGui.TableSetupColumn(L('table_vertices'))
        tImGui.TableSetupColumn(L('table_status'))
        tImGui.TableHeadersRow()
        for _,row in ipairs(E.regionTableRows) do
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0); tImGui.Text(row.name)
            tImGui.TableSetColumnIndex(1); tImGui.Text(row.type)
            tImGui.TableSetColumnIndex(2); tImGui.Text(row.mode)
            tImGui.TableSetColumnIndex(3); tImGui.Text(tostring(row.points))
            tImGui.TableSetColumnIndex(4); tImGui.Text(tostring(row.vertices))
            tImGui.TableSetColumnIndex(5); tImGui.Text(L('region_status_'..row.status))
        end
        tImGui.EndTable()
    end
end

local function targetStatusLabel(status)
    return L('target_status_'..tostring(status))
end

local function drawTargetTable()
    if #(E.project.targets or {})==0 then
        tImGui.TextDisabled(L('no_targets'))
        return
    end
    tImGui.Separator()
    if tImGui.BeginTable('mreTargetTable',5,
            tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(L('target_table_name'))
        tImGui.TableSetupColumn(L('target_table_source'))
        tImGui.TableSetupColumn(L('target_table_vertices'))
        tImGui.TableSetupColumn(L('target_table_deltas'))
        tImGui.TableSetupColumn(L('target_table_status'))
        tImGui.TableHeadersRow()
        for index,target in ipairs(E.project.targets) do
            local _,region=targetRegion(target)
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0)
            if tImGui.Selectable((target.name or '')..'##mreTargetRow'..index,
                    index==E.project.selectedTarget) then selectTarget(index) end
            tImGui.TableSetColumnIndex(1); tImGui.Text(region and region.name or '-')
            tImGui.TableSetColumnIndex(2); tImGui.Text(tostring(#(target.vertices or {})))
            tImGui.TableSetColumnIndex(3); tImGui.Text(tostring(tTargets.deltaCount(target)))
            tImGui.TableSetColumnIndex(4); tImGui.Text(targetStatusLabel(targetStatus(target)))
        end
        tImGui.EndTable()
    end
end

local function drawTargetSourceSelector()
    if #(E.project.regions or {})==0 then
        tImGui.TextDisabled(L('no_shapes'))
        return nil
    end
    refreshRegionNames()
    local index=math.max(1,math.min(#E.regionNames,E.project.selectedRegion))
    tImGui.SetNextItemWidth(220)
    local changed,selected=tImGui.Combo(L('target_source_region')..'##mreTargetSourceRegion',
        index,E.regionNames)
    if changed then
        E.project.selectedRegion=selected
        E.selectedPoint=nil
        E.overlayDirty=true
        E.regionStatusDirty=true
        E.modified=true
    end
    return selectedRegion()
end

local function drawMorphTargets()
    if not tImGui.TreeNodeEx(L('worktree_morph_targets')..'##mreMorphTargets',
            tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then return end
    local previousMode=E.targetMode
    E.targetMode=tImGui.RadioButton(L('target_mode_add')..'##mreTargetModeAdd',E.targetMode,1)
    tImGui.SameLine()
    E.targetMode=tImGui.RadioButton(L('target_mode_edit')..'##mreTargetModeEdit',E.targetMode,2)
    tImGui.SameLine()
    E.targetMode=tImGui.RadioButton(L('target_mode_remove')..'##mreTargetModeRemove',E.targetMode,3)
    if E.targetMode~=previousMode then E.modified=true end
    tImGui.TextWrapped(L('target_interaction_help'))
    tImGui.Separator()
    if not E.topology or E.project.topologySignature~=E.topology.signature then
        tImGui.TextWrapped(L('target_mesh_required'))
        tImGui.TreePop()
        return
    end
    if E.targetMode==1 then
        tImGui.SetNextItemWidth(220)
        local changed,name=tImGui.InputText(L('target_name')..'##mreTargetName',E.targetNameDraft)
        if changed then E.targetNameDraft=name; E.modified=true end
        local region=drawTargetSourceSelector()
        local sourceReady=region and tRegions.status(region,E.topology)=='ready'
        if region then tImGui.Text(targetStatusLabel(region and
            ('source_'..tRegions.status(region,E.topology)) or 'missing_source')) end
        tImGui.BeginDisabled(not sourceReady)
        if tImGui.Button(L('target_create')..'##mreTargetCreate') then createTargetFromRegion() end
        tImGui.EndDisabled()
        showHelpTooltip(L('target_create_help'))
    else
        local target=selectedTarget()
        if #(E.project.targets or {})==0 then
            tImGui.TextDisabled(L('no_targets'))
        else
            refreshRegionNames()
            local names={}
            for _,entry in ipairs(E.project.targets) do names[#names+1]=entry.name end
            local targetIndex=math.max(1,math.min(#names,E.project.selectedTarget))
            tImGui.SetNextItemWidth(220)
            local changed,index=tImGui.Combo(L('target_select')..'##mreTargetSelect',targetIndex,names)
            if changed then selectTarget(index); target=selectedTarget() end
            if target then
                local _,region=targetRegion(target)
                tImGui.Text(string.format(L('target_source_info'),region and region.name or '-'))
                tImGui.Text(string.format(L('target_vertex_count'),#(target.vertices or {})))
                tImGui.Text(string.format(L('target_delta_count'),tTargets.deltaCount(target)))
                tImGui.Text(targetStatusLabel(targetStatus(target)))
                if E.targetMode==2 then
                    tImGui.BeginDisabled(targetStatus(target)~='ready')
                    if tImGui.Button(L('target_rebuild')..'##mreTargetRebuild') then
                        rebuildSelectedTarget()
                    end
                    tImGui.EndDisabled()
                    tImGui.Separator()
                    tImGui.SetNextItemWidth(220)
                    local deltaChanged,delta=tImGui.DragFloat3(L('target_translation')..'##mreTargetTranslation',
                        {E.targetDx,E.targetDy,E.targetDz},0.001,-math.huge,math.huge,'%.4f')
                    if deltaChanged then
                        E.targetDx, E.targetDy, E.targetDz=delta[1] or 0,delta[2] or 0,delta[3] or 0
                        E.modified=true
                    end
                    if tImGui.Button(L('target_apply_translation')..'##mreTargetApplyTranslation') then
                        applyTargetTranslation()
                    end
                    tImGui.SameLine()
                    if tImGui.Button(L('target_reset_deltas')..'##mreTargetReset') then
                        resetSelectedTargetDeltas()
                    end
                    showHelpTooltip(L('target_translation_help'))
                else
                    if tImGui.Button(L('target_delete')..'##mreTargetDelete') then
                        deleteSelectedTarget()
                    end
                end
            end
        end
    end
    drawTargetTable()
    tImGui.TreePop()
end

local function drawSelectedRegionSelector()
    if #E.project.regions==0 then
        tImGui.TextDisabled(L('no_shapes'))
        return nil
    end
    tImGui.Text(L('selected_shape'))
    refreshRegionNames()
    local names=E.regionNames
    local index=math.max(1,math.min(#names,E.project.selectedRegion))
    tImGui.SetNextItemWidth(220)
    local selectionChanged,selected=tImGui.Combo(L('select_region')..'##mreSelectedShape',index,names)
    if selectionChanged then
        E.project.selectedRegion=selected
        E.selectedPoint=nil
        E.overlayDirty=true
        E.regionStatusDirty=true
        E.modified=true
    end
    return selectedRegion()
end

local function drawSelectedShapeEditControls(region,shapeTypes)
    local selectedShapeIndex=shapeTypeIndex(region)
    tImGui.SetNextItemWidth(220)
    local selectedShapeChanged,newShapeIndex=tImGui.Combo(L('shape_type')..'##mreSelectedShapeType',
        selectedShapeIndex,shapeTypes)
    if selectedShapeChanged then
        local newType=shapeTypeFromIndex(newShapeIndex)
        local keepFree=newType=='polygon' or
            (tShapes.type(region)~='polygon' and tShapes.isFree(region))
        changeSelectedShape(region,newType,keepFree)
    end
    local shapeIsFree=tShapes.isFree(region)
    tImGui.BeginDisabled(tShapes.type(region)=='polygon')
    local selectedFree=tImGui.Checkbox(L('shape_free')..'##mreSelectedShapeFree',shapeIsFree)
    tImGui.EndDisabled()
    if tShapes.type(region)~='polygon' and selectedFree~=shapeIsFree then
        changeSelectedShape(region,tShapes.type(region),selectedFree)
    end
    tImGui.Text(string.format(L('region_point_count'),#region.points))
    tImGui.Text(string.format(L('region_weight_count'),#region.derivedVertexWeights))
    tImGui.Text(L('region_status_'..E.selectedRegionStatus))
    tImGui.Separator()
    local surfaceAxes={L('surface_axis_x'),L('surface_axis_y'),L('surface_axis_z')}
    tImGui.SetNextItemWidth(150)
    local surfaceAxisChanged,surfaceAxis=tImGui.Combo(L('surface_axis')..'##mreSurfaceAxis',
        E.surfaceAxis,surfaceAxes)
    if surfaceAxisChanged then
        E.surfaceAxis=surfaceAxis
        E.modified=true
    end
    showHelpTooltip(L('surface_snap_help'))
    tImGui.BeginDisabled(not E.topology or #region.points==0)
    if tImGui.Button(L('surface_snap')..'##mreSurfaceSnap') then
        local ok,result=snapSelectedShapeToSurface(region)
        if not ok then setStatus(L('surface_snap_failed'),true)
        elseif result=='unchanged' then setStatus(L('surface_snap_already'),false)
        else setStatus(L('surface_snapped'),false) end
    end
    tImGui.EndDisabled()
    tImGui.BeginDisabled(tShapes.isParametric(region))
    if tImGui.Button((region.closed and L('open_contour') or L('close_contour'))..'##mreContourToggle') then
        toggleClosed()
    end
    tImGui.EndDisabled()
    tImGui.SameLine()
    tImGui.BeginDisabled(not E.selectedPoint)
    if tImGui.Button(L('remove_point')..'##mreRemovePoint') then removeSelectedPoint() end
    tImGui.EndDisabled()
    tImGui.BeginDisabled(not region.closed or #region.points<3)
    if tImGui.Button(L('rebuild_region')..'##mreRebuildSelection') then rebuildRegion(region) end
    tImGui.EndDisabled()
end

local function drawControls()
    tImGui.Begin(L('title')..'###MorphRegionEditor',true,0)
    if E.mesh then
        tImGui.Text(string.format(L('mesh_info'),E.project.meshPath,E.topology.vertexCount))
    else
        tImGui.TextWrapped(L('load_mesh_help'))
    end
    tImGui.Separator()
    if tImGui.TreeNodeEx(L('worktrees')..'##mreWorktrees',
            tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
        local previousWorktree=E.worktree
        E.worktree=tImGui.RadioButton(L('worktree_surface_shapes')..'##mreWorktreeShapes',E.worktree,1)
        tImGui.SameLine()
        E.worktree=tImGui.RadioButton(L('worktree_morph_targets')..'##mreWorktreeTargets',E.worktree,2)
        if E.worktree~=previousWorktree then
            E.regionDrag=nil
            E.pendingSurfacePick=nil
            E.modified=true
            E.overlayDirty=true
        end
        tImGui.TreePop()
    end
    if E.worktree==1 and tImGui.TreeNodeEx(L('worktree_surface_shapes')..'##mreSurfaceShapes',
            tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
    local previousShapeMode=E.shapeMode
    E.shapeMode=tImGui.RadioButton(L('mode_add')..'##mreShapeModeAdd',E.shapeMode,1)
    tImGui.SameLine()
    E.shapeMode=tImGui.RadioButton(L('mode_edit')..'##mreShapeModeEdit',E.shapeMode,2)
    tImGui.SameLine()
    E.shapeMode=tImGui.RadioButton(L('mode_remove')..'##mreShapeModeRemove',E.shapeMode,3)
    if E.shapeMode~=previousShapeMode then E.modified=true end
    tImGui.TextWrapped(L('interaction_help'))
    tImGui.Separator()
    tImGui.SetNextItemWidth(220)
    local handleScaleChanged,handleScale=tImGui.DragFloat(L('handle_size')..'##mreHandleSize',
        E.handleScale,0.05,0.05,5.0,'%.2f')
    if handleScaleChanged then
        E.handleScale=math.max(0.05,math.min(5.0,handleScale))
        E.overlayDirty=true
        E.modified=true
    end
    showHelpTooltip(L('handle_size_help'))
    tImGui.Separator()
    if E.topology and E.project.topologySignature==E.topology.signature then
        tImGui.SetNextItemWidth(220)
        local shapeTypes={L('shape_square'),L('shape_ellipse'),L('shape_polygon')}
        if E.shapeMode==1 then
            local changed,name=tImGui.InputText(L('new_region_name')..'##mreNewShapeName',E.nameDraft)
            if changed then
                E.nameDraft=name
                E.modified=true
            end
            tImGui.SetNextItemWidth(220)
            local shapeChanged,shapeIndex=tImGui.Combo(L('new_shape_type')..'##mreNewShapeType',E.newShapeType,shapeTypes)
            if shapeChanged then
                E.newShapeType=shapeIndex
                if shapeIndex==3 then E.newShapeFree=false end
                E.modified=true
            end
            tImGui.BeginDisabled(E.newShapeType==3)
            local free=tImGui.Checkbox(L('shape_free')..'##mreNewShapeFree',E.newShapeFree)
            if free~=E.newShapeFree then
                E.newShapeFree=free
                E.modified=true
            end
            tImGui.EndDisabled()
            showHelpTooltip(L('shape_free_help'))
            if tImGui.Button(L('add_region')..'##mreAddShape') then addRegion() end
        elseif E.shapeMode==2 then
            local region=drawSelectedRegionSelector()
            if region then drawSelectedShapeEditControls(region,shapeTypes) end
        else
            local region=drawSelectedRegionSelector()
            if region then
                if tImGui.Button(L('delete_region')..'##mreDeleteShape') then deleteSelectedRegion() end
            end
        end
        if E.shapeMode~=1 then drawRegionTable() end
    elseif E.topology then
        tImGui.TextWrapped(L('topology_changed'))
    end
    tImGui.TreePop()
    end
    if E.worktree==2 then drawMorphTargets() end
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
    tUtil.sMessageOverlay=false
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
    showMenu()
    syncLanguagePreference()
    drawControls()
    showCameraPanel()
    tUtil.showOverlayMessage()
end

function onTouchDown(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
    if key==0 then
        if E.worktree==1 and pickPoint(x,y,false) then
            return
        end
        if E.worktree==1 then
            E.pendingSurfacePick={x=x,y=y}
        end
        E.cameraDrag={x=x,y=y,startX=x,startY=y,moved=false}
    elseif key==1 then
        E.cameraDrag={x=x,y=y,startX=x,startY=y,moved=false}
    elseif key==2 then
        E.cameraPan={x=x,y=y}
    end
end

function onTouchMove(key,x,y)
    if E.regionDrag then updatePointDrag(x,y); return end
    if E.cameraDrag then
        if E.pendingSurfacePick then
            local moved=math.abs(x-E.cameraDrag.startX)>2 or math.abs(y-E.cameraDrag.startY)>2
            if not moved then return end
            E.pendingSurfacePick=nil
            E.cameraDrag.moved=true
        end
        orbitCamera(x,y)
        return
    end
    if E.cameraPan then panCamera(x,y) end
end

function onTouchUp(key,x,y)
    if key==0 and E.regionDrag then
        local drag=E.regionDrag
        local region=selectedRegion()
        if region and drag.mode=='point' then snapDraggedPoint(drag,x,y,region) end
        E.regionDrag=nil
        if drag.changed and region and region.closed then rebuildRegion(region)
        elseif drag.changed then markChanged() end
        return
    end
    if key==0 and E.worktree==1 and E.pendingSurfacePick then
        local drag=E.cameraDrag
        local pending=E.pendingSurfacePick
        E.pendingSurfacePick=nil
        if drag and not drag.moved then
            pickPoint(x or pending.x,y or pending.y,true)
        end
    end
    if key==0 or key==1 then E.cameraDrag=nil end
    if key==2 then E.cameraPan=nil end
end

function onTouchZoom(zoom)
    if tImGui.IsAnyWindowHovered() then return end
    E.camera.distance=math.max(0.01,E.camera.distance*(zoom>0 and 0.9 or 1.1))
    applyCamera()
    E.modified=true
end

function onKeyDown(key)
    if tImGui.GetWantCaptureKeyboard() then return end
    if E.worktree==1 and key==mbm.getKeyCode('DELETE') then removeSelectedPoint() end
end

function onEndScene()
    destroyMesh()
end
