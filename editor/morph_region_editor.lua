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
local tExpressions=require 'morph_surface_expressions'
local tClips=require 'morph_surface_clips'
local tOverlay=require 'morph_surface_overlay'
local tTargetPreview=require 'morph_surface_target_preview'
local tTargetBrush=require 'morph_surface_target_brush'
local tTargetGizmo=require 'morph_surface_target_gizmo'
local tTargetMirror=require 'morph_surface_target_mirror'
local tSurfaceNormals=require 'morph_surface_normals'
local tPreviewRuntime=require 'morph_preview_runtime'

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
    targetAvailabilityDirty=true,targetAvailability=false,
    camera={fx=0,fy=0,fz=0,distance=10,azimuth=0.45,elevation=0.22},
    pendingAction=nil,confirmOpen=false,newShapeType=1,newShapeFree=false,
    pendingSurfacePick=nil,surfaceAxis=3,shapeMode=1,handleScale=1,
    shapeMirrorAxis=1,shapeMirrorNameDraft='region_mirror',shapeMirrorNameSourceId=nil,
    shapeMirrorNeedsPrepare=false,shapeMirrorCandidate=nil,
    worktree=1,targetMode=1,targetNameDraft='target',targetNameSourceId=nil,targetDx=0,targetDy=0,targetDz=0,
    expressionMode=1,expressionNameDraft='expression',expressionDraft=nil,
    clipMode=1,clipNameDraft='clip',clipSelectedExpression=1,clipTime=0,clipKeySelected=1,clipPlaying=false,
    clipTimelineVisible=true,
    clipPreviewWeight=1,clipPreview={key=nil,target=nil,report=nil},
    previewMode=1,previewExpressionSelected=1,previewClipSelected=1,previewClipTime=0,
    previewClipPlaying=false,preview={key=nil,target=nil,report=nil},
    targetEditMode=1,targetRx=0,targetRy=0,targetRz=0,
    targetFeatherRings=0,targetFeatherCache=nil,targetEditDraft=nil,targetEditDraftKey=nil,
    targetMirrorAxis=1,targetMirrorTolerance=0.005,targetMirrorAllowApprox=false,
    targetMirrorNameDraft='target_mirror',targetMirrorNameTargetId=nil,
    targetMirrorNeedsPrepare=false,targetMirrorPlan=nil,targetMirrorCandidate=nil,
    targetBrushMode=1,targetBrushRadius=0.025,targetBrushStrength=0.5,targetBrushSmoothPasses=1,
    targetBrushSmoothProtectCreases=true,targetBrushSmoothAngle=60,
    exportNormalMode=1,exportNormalDraft=1,exportDialogOpen=false,
    targetBrushDx=0,targetBrushDy=-0.01,targetBrushDz=0,
    targetBrush={geometry=nil,geometryKey=nil,hoverHit=nil,hoverX=nil,hoverY=nil,stroke=nil},
    targetBrushHistory={undo={},redo={}},targetEditHistory={undo={},redo={}},controlDown=false,
    targetGizmo={rings={},axes={},center=nil,origin=nil,radius=nil,length=nil,targetId=nil,
        visible=false,dirty=true,drag=nil},
    targetPreviewWeight=1,targetPreviewVisible=true,targetPreviewDirty=true,targetPreview={},
    expressionPreview={key=nil,target=nil,report=nil},
    previewRuntime=tPreviewRuntime.new(),dqsSupported=false}

local targetStatus
local function L(key) return tLang.L('mre_'..key) end

local function setStatus(message,isError)
    E.status=message or ''
    E.statusError=isError==true
    if E.status=='' then
        tUtil.sMessageOverlay=false
        tUtil.bWarnMessage=false
        if tUtil.tTimerOverlay then tUtil.tTimerOverlay:stop() end
    elseif E.statusError then
        tUtil.showMessageWarn(E.status,5)
    else
        tUtil.showMessage(E.status,3)
    end
end

local function clearStatus()
    E.status=''
    E.statusError=false
    tUtil.sMessageOverlay=false
    tUtil.bWarnMessage=false
    if tUtil.tTimerOverlay then tUtil.tTimerOverlay:stop() end
end

local function clearTargetBrushHistory()
    E.targetBrushHistory.undo={}
    E.targetBrushHistory.redo={}
end

local function clearTargetEditHistory()
    E.targetEditHistory.undo={}
    E.targetEditHistory.redo={}
end

local function projectDisplayName()
    local path=type(E.projectPath)=='string' and E.projectPath or ''
    local name=path:match('([^/\\]+)$') or ''
    name=name:gsub('%.morph%.lua$','')
    if name=='' then return L('untitled_project') end
    return name
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
    E.targetAvailabilityDirty=true
    E.targetPreviewDirty=true
    E.targetFeatherCache=nil
    E.targetEditDraft=nil
    E.targetEditDraftKey=nil
    E.targetGizmo.dirty=true
    if E.expressionPreview then E.expressionPreview.key=nil end
    if E.clipPreview then E.clipPreview.key=nil end
    if E.preview then E.preview.key=nil end
    if E.targetBrush and not E.targetBrush.stroke then
        E.targetBrush.geometry=nil
        E.targetBrush.geometryKey=nil
    end
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
    preferences.shapeMirrorAxis=E.shapeMirrorAxis
    preferences.shapeMirrorNameDraft=E.shapeMirrorNameDraft
    preferences.worktree=E.worktree
    preferences.targetMode=E.targetMode
    preferences.targetNameSourceId=nil
    preferences.expressionMode=E.expressionMode
    preferences.expressionNameDraft=E.expressionNameDraft
    preferences.clipMode=E.clipMode
    preferences.clipNameDraft=E.clipNameDraft
    preferences.clipSelectedExpression=E.clipSelectedExpression
    preferences.clipKeySelected=E.clipKeySelected
    preferences.clipPreviewWeight=E.clipPreviewWeight
    preferences.clipTimelineVisible=E.clipTimelineVisible
    preferences.previewMode=E.previewMode
    preferences.previewExpressionSelected=E.previewExpressionSelected
    preferences.previewClipSelected=E.previewClipSelected
    preferences.previewComparisonMode=E.previewRuntime and E.previewRuntime.comparisonMode or 1
    preferences.previewSkeletalSelected=E.previewRuntime and E.previewRuntime.selectedSkeletal or 1
    preferences.previewWeight=E.targetPreviewWeight
    preferences.newShapeType=E.newShapeType
    preferences.newShapeFree=E.newShapeFree
    preferences.nameDraft=E.nameDraft
    preferences.targetNameDraft=E.targetNameDraft
    preferences.targetDx=E.targetDx
    preferences.targetDy=E.targetDy
    preferences.targetDz=E.targetDz
    preferences.targetEditMode=E.targetEditMode
    preferences.targetRx=E.targetRx
    preferences.targetRy=E.targetRy
    preferences.targetRz=E.targetRz
    preferences.targetFeatherRings=E.targetFeatherRings
    preferences.targetMirrorAxis=E.targetMirrorAxis
    preferences.targetMirrorTolerance=E.targetMirrorTolerance
    preferences.targetMirrorAllowApprox=E.targetMirrorAllowApprox
    preferences.targetMirrorNameDraft=E.targetMirrorNameDraft
    preferences.targetBrushMode=E.targetBrushMode
    preferences.targetBrushRadius=E.targetBrushRadius
    preferences.targetBrushStrength=E.targetBrushStrength
    preferences.targetBrushSmoothPasses=E.targetBrushSmoothPasses
    preferences.targetBrushSmoothProtectCreases=E.targetBrushSmoothProtectCreases
    preferences.targetBrushSmoothAngle=E.targetBrushSmoothAngle
    preferences.exportNormalMode=E.exportNormalMode
    preferences.targetBrushDx=E.targetBrushDx
    preferences.targetBrushDy=E.targetBrushDy
    preferences.targetBrushDz=E.targetBrushDz
    preferences.targetPreviewWeight=E.targetPreviewWeight
    preferences.targetPreviewVisible=E.targetPreviewVisible
    if E.mesh then
        local preview=E.targetPreview
        local shaderPreviewVisible=E.targetEditDraft==nil and E.targetPreviewVisible
        local visibleMesh=preview and preview.surface and
            (shaderPreviewVisible and preview.surface or preview.plainSurface) or E.mesh
        local nativePreviewActive=E.worktree==5 and E.previewRuntime and
            (E.previewRuntime.comparisonMode~=1 or E.previewRuntime.skeletalPlaying)
        if nativePreviewActive and E.previewRuntime.previewReady then
            visibleMesh=E.previewRuntime.previewMesh
        end
        preferences.meshVisible=visibleMesh.visible~=false
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
    E.shapeMode=math.max(1,math.min(4,math.floor(tonumber(preferences.shapeMode) or 1)))
    E.shapeMirrorAxis=math.max(1,math.min(3,math.floor(tonumber(preferences.shapeMirrorAxis) or 1)))
    E.shapeMirrorNameDraft=type(preferences.shapeMirrorNameDraft)=='string' and
        preferences.shapeMirrorNameDraft~='' and preferences.shapeMirrorNameDraft or 'region_mirror'
    local selectedShape=project.regions[project.selectedRegion]
    E.shapeMirrorNameSourceId=E.shapeMode==4 and selectedShape and selectedShape.id or nil
    E.shapeMirrorNeedsPrepare=E.shapeMode==4
    E.shapeMirrorCandidate=nil
    E.worktree=math.max(1,math.min(5,math.floor(tonumber(preferences.worktree) or 1)))
    E.targetMode=math.max(1,math.min(5,math.floor(tonumber(preferences.targetMode) or 1)))
    E.targetNameSourceId=nil
    E.expressionMode=math.max(1,math.min(3,math.floor(tonumber(preferences.expressionMode) or 1)))
    E.expressionNameDraft=type(preferences.expressionNameDraft)=='string' and
        preferences.expressionNameDraft~='' and preferences.expressionNameDraft or 'expression'
    E.expressionDraft=nil
    E.clipMode=math.max(1,math.min(3,math.floor(tonumber(preferences.clipMode) or 1)))
    E.clipNameDraft=type(preferences.clipNameDraft)=='string' and
        preferences.clipNameDraft~='' and preferences.clipNameDraft or 'clip'
    E.clipSelectedExpression=math.max(1,math.floor(tonumber(preferences.clipSelectedExpression) or 1))
    E.clipTime=0
    E.clipKeySelected=math.max(1,math.floor(tonumber(preferences.clipKeySelected) or 1))
    E.clipPlaying=false
    E.clipPreviewWeight=math.max(0,math.min(1,tonumber(preferences.clipPreviewWeight) or 1))
    E.clipTimelineVisible=preferences.clipTimelineVisible~=false
    E.clipPreview={key=nil,target=nil,report=nil}
    E.previewMode=math.max(1,math.min(2,math.floor(tonumber(preferences.previewMode) or 1)))
    E.previewExpressionSelected=math.max(1,math.floor(tonumber(preferences.previewExpressionSelected) or 1))
    E.previewClipSelected=math.max(1,math.floor(tonumber(preferences.previewClipSelected) or 1))
    E.previewClipTime=0
    E.previewClipPlaying=false
    E.preview={key=nil,target=nil,report=nil}
    E.previewRuntime=tPreviewRuntime.new()
    E.previewRuntime.comparisonMode=math.max(1,math.min(3,
        math.floor(tonumber(preferences.previewComparisonMode) or 1)))
    E.previewRuntime.selectedSkeletal=math.max(1,
        math.floor(tonumber(preferences.previewSkeletalSelected) or 1))
    E.newShapeType=math.max(1,math.min(3,math.floor(tonumber(preferences.newShapeType) or 1)))
    E.newShapeFree=preferences.newShapeFree==true
    E.nameDraft=type(preferences.nameDraft)=='string' and preferences.nameDraft~='' and
        preferences.nameDraft or 'region'
    E.targetNameDraft=type(preferences.targetNameDraft)=='string' and
        preferences.targetNameDraft~='' and preferences.targetNameDraft or 'target'
    E.targetDx=tonumber(preferences.targetDx) or 0
    E.targetDy=tonumber(preferences.targetDy) or 0
    E.targetDz=tonumber(preferences.targetDz) or 0
    E.targetEditMode=math.max(1,math.min(3,math.floor(tonumber(preferences.targetEditMode) or 1)))
    E.targetRx=tonumber(preferences.targetRx) or 0
    E.targetRy=tonumber(preferences.targetRy) or 0
    E.targetRz=tonumber(preferences.targetRz) or 0
    E.targetFeatherRings=math.max(0,math.min(8,math.floor(tonumber(preferences.targetFeatherRings) or 0)))
    E.targetMirrorAxis=math.max(1,math.min(3,math.floor(tonumber(preferences.targetMirrorAxis) or 1)))
    E.targetMirrorTolerance=math.max(0.00001,math.min(0.01,
        tonumber(preferences.targetMirrorTolerance) or 0.005))
    E.targetMirrorAllowApprox=preferences.targetMirrorAllowApprox==true
    E.targetMirrorNameDraft=type(preferences.targetMirrorNameDraft)=='string' and
        preferences.targetMirrorNameDraft~='' and preferences.targetMirrorNameDraft or 'target_mirror'
    local selectedTargetRecord=project.targets[project.selectedTarget]
    E.targetMirrorNameTargetId=E.targetMode==5 and selectedTargetRecord and selectedTargetRecord.id or nil
    E.targetMirrorNeedsPrepare=E.targetMode==5
    E.targetMirrorPlan=nil
    E.targetMirrorCandidate=nil
    E.targetFeatherCache=nil
    E.targetEditDraft=nil
    E.targetEditDraftKey=nil
    E.targetGizmo.dirty=true
    E.targetBrushMode=math.max(1,math.min(2,math.floor(tonumber(preferences.targetBrushMode) or 1)))
    E.targetBrushRadius=math.max(0.001,math.min(0.15,tonumber(preferences.targetBrushRadius) or 0.025))
    E.targetBrushStrength=math.max(0.01,math.min(1,tonumber(preferences.targetBrushStrength) or 0.5))
    E.targetBrushSmoothPasses=math.max(1,math.min(8,
        math.floor(tonumber(preferences.targetBrushSmoothPasses) or 1)))
    E.targetBrushSmoothProtectCreases=preferences.targetBrushSmoothProtectCreases~=false
    E.targetBrushSmoothAngle=math.max(5,math.min(180,
        tonumber(preferences.targetBrushSmoothAngle) or 60))
    E.exportNormalMode=math.max(1,math.min(2,math.floor(tonumber(preferences.exportNormalMode) or 1)))
    E.targetBrushDx=tonumber(preferences.targetBrushDx) or 0
    E.targetBrushDy=tonumber(preferences.targetBrushDy) or -0.01
    E.targetBrushDz=tonumber(preferences.targetBrushDz) or 0
    E.targetBrush.geometry=nil
    E.targetBrush.geometryKey=nil
    E.targetBrush.hoverHit=nil
    E.targetPreviewWeight=math.max(0,math.min(1,
        tonumber(preferences.targetPreviewWeight or preferences.previewWeight) or 1))
    E.targetPreviewVisible=preferences.targetPreviewVisible~=false
    E.expressionPreview={key=nil,target=nil,report=nil}
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
    tPreviewRuntime.destroy(E.previewRuntime,E.mesh,dpCall)
    tTargetPreview.destroy(E)
    tOverlay.destroy(E)
    if E.mesh then dpCall(function() E.mesh:destroy() end) end
    E.mesh=nil
    E.meshDebug=nil
    E.topology=nil
    E.meshBounds=nil
    E.targetBrush.stroke=nil
    E.targetBrush.geometry=nil
    E.targetBrush.geometryKey=nil
    E.targetBrush.hoverHit=nil
    E.targetBrush.hoverX=nil
    E.targetBrush.hoverY=nil
    E.targetBrush.hoverTime=nil
    clearTargetBrushHistory()
    clearTargetEditHistory()
    tTargetGizmo.destroy(E.targetGizmo)
    E.targetGizmo={rings={},axes={},center=nil,origin=nil,radius=nil,length=nil,targetId=nil,
        visible=false,dirty=true,drag=nil}
    E.targetPreviewDirty=true
    E.expressionPreview={key=nil,target=nil,report=nil}
    E.clipPreview={key=nil,target=nil,report=nil}
    E.preview={key=nil,target=nil,report=nil}
    E.clipTime=0
    E.clipKeySelected=1
    E.clipPlaying=false
    E.previewClipTime=0
    E.previewClipPlaying=false
    E.targetMirrorPlan=nil
    E.targetMirrorCandidate=nil
    E.targetMirrorNeedsPrepare=false
    E.shapeMirrorCandidate=nil
    E.shapeMirrorNeedsPrepare=false
    E.regionStatusDirty=true
    E.regionTableDirty=true
    E.targetAvailabilityDirty=true
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
        E.targetPreviewWeight=1
        E.targetPreviewVisible=true
        E.targetEditMode=1
        E.targetRx=0
        E.targetRy=0
        E.targetRz=0
        E.targetFeatherRings=0
        E.targetFeatherCache=nil
        E.targetEditDraft=nil
        E.targetEditDraftKey=nil
        E.targetMirrorAxis=1
        E.targetMirrorTolerance=0.005
        E.targetMirrorAllowApprox=false
        E.targetMirrorNameDraft='target_mirror'
        E.targetMirrorNameTargetId=nil
        E.targetMirrorNeedsPrepare=false
        E.targetMirrorPlan=nil
        E.targetMirrorCandidate=nil
        E.shapeMirrorAxis=1
        E.shapeMirrorNameDraft='region_mirror'
        E.shapeMirrorNameSourceId=nil
        E.shapeMirrorNeedsPrepare=false
        E.shapeMirrorCandidate=nil
        E.targetGizmo.dirty=true
        E.targetBrushMode=1
        E.targetBrushRadius=0.025
        E.targetBrushStrength=0.5
        E.targetBrushSmoothPasses=1
        E.targetBrushSmoothProtectCreases=true
        E.targetBrushSmoothAngle=60
        E.targetBrushDx=0
        E.targetBrushDy=-0.01
        E.targetBrushDz=0
        E.targetBrush.geometry=nil
        E.targetBrush.geometryKey=nil
        E.targetBrush.hoverHit=nil
        E.regionNamesDirty=true
        E.regionTableDirty=true
        E.targetAvailabilityDirty=true
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
    clearTargetBrushHistory()
    clearTargetEditHistory()
    E.selectedPoint=nil
    E.regionNamesDirty=true
    E.regionStatusDirty=true
    E.regionTableDirty=true
    E.targetAvailabilityDirty=true
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

local function exportMorphTargets()
    if not E.meshDebug or not E.topology then
        setStatus(L('export_msh_no_mesh'),true)
        return false
    end
    if #(E.project.targets or {})==0 then
        setStatus(L('export_msh_no_targets'),true)
        return false
    end
    for _,target in ipairs(E.project.targets) do
        if targetStatus(target)~='ready' then
            setStatus(string.format(L('export_msh_target_stale'),target.name or ''),true)
            return false
        end
    end
    local sourcePath=tUtil.getTemporaryFilePath('_morph_export_source.msh')
    local suggested=(E.project.meshPath or ''):gsub('%.msh$','')
    if suggested=='' then suggested='morph-targets' else suggested=suggested..'-morph-targets' end
    local outputPath=mbm.saveFile(suggested,'msh')
    if not outputPath or outputPath=='' then return false end
    local sourceFull=mbm.getFullPath(E.project.meshPath or '') or E.project.meshPath or ''
    local outputFull=mbm.getFullPath(outputPath) or outputPath
    if sourceFull~='' and outputFull==sourceFull then
        setStatus(L('export_msh_source_same'),true)
        return false
    end
    local copy
    local function cleanup()
        -- meshDebug userdata has no destroy method; dropping the reference lets Lua release it.
        copy=nil
        if sourcePath and sourcePath~='' then
            dpCall(function() meshDebug:fakeRelease(sourcePath) end)
            os.remove(sourcePath)
        end
    end
    local savedOk,saved=dpCall(function()
        return E.meshDebug:save(sourcePath,false,false,true)
    end)
    if not savedOk or saved~=true then
        cleanup()
        setStatus(L('export_msh_source_failed'),true)
        return false
    end
    copy=meshDebug:new()
    local loadedOk,loaded=dpCall(function() return copy:load(sourcePath) end)
    if not loadedOk or loaded~=true then
        cleanup()
        setStatus(L('export_msh_source_failed'),true)
        return false
    end
    dpCall(function() return copy:removeNormalMap() end)
    local dataOk,exportData=dpCall(function() return copy:getFacialExportData() end)
    if not dataOk or type(exportData)~='table' then
        cleanup()
        setStatus(L('export_msh_source_failed'),true)
        return false
    end
    local baseNormals=nil
    if E.exportNormalMode==2 then
        if exportData.hasNormals~=true then
            cleanup()
            setStatus(L('export_msh_no_normals'),true)
            return false
        end
        local normalsOk,normals=dpCall(function()
            return tSurfaceNormals.readBaseNormals(copy,E.topology)
        end)
        if not normalsOk or type(normals)~='table' then
            cleanup()
            setStatus(L('export_msh_normal_failed'),true)
            return false
        end
        baseNormals=normals
    end
    local existing={}
    for index,entry in ipairs(exportData.targets or {}) do
        if type(entry)=='table' and type(entry.name)=='string' and entry.name~='' then
            existing[entry.name]={index=index,normalDeltas=entry.normalDeltas}
        end
    end
    local vertexCount=#E.topology.vertices
    local function densePositions(target)
        local positions={}
        for index=1,vertexCount do positions[index]={x=0,y=0,z=0} end
        for _,delta in ipairs(target.deltas or {}) do
            local index=math.floor(tonumber(delta.index) or 0)
            if positions[index] then
                positions[index]={x=tonumber(delta.x) or 0,y=tonumber(delta.y) or 0,
                    z=tonumber(delta.z) or 0}
            end
        end
        return positions
    end
    for _,target in ipairs(E.project.targets) do
        local entry=existing[target.name]
        local positions=densePositions(target)
        local normalDeltas=entry and entry.normalDeltas or nil
        if E.exportNormalMode==2 then
            local normalsOk,computed=dpCall(function()
                return tSurfaceNormals.recomputeDeltas(E.topology,positions,baseNormals)
            end)
            if not normalsOk or type(computed)~='table' then
                cleanup()
                setStatus(string.format(L('export_msh_normal_target_failed'),target.name or ''),true)
                return false
            end
            normalDeltas=computed
        end
        local callOk,result
        if entry then
            callOk,result=dpCall(function()
                return copy:updateFacialTarget(entry.index,target.name,'Manual',0,positions,
                    normalDeltas)
            end)
        else
            callOk,result=dpCall(function()
                return copy:addFacialTarget(target.name,'Manual',0,positions,normalDeltas)
            end)
        end
        local targetSaved=result==true or type(result)=='number'
        if not callOk or not targetSaved then
            cleanup()
            setStatus(string.format(L('export_msh_target_failed'),target.name or ''),true)
            return false
        end
        if not entry and type(result)=='number' then
            existing[target.name]={index=result,normalDeltas=nil}
        end
    end
    local outputOk,outputSaved=dpCall(function()
        -- Preserve the source texture references; save() does not copy texture files.
        return copy:save(outputPath,false,false,false)
    end)
    cleanup()
    if not outputOk or outputSaved~=true then
        setStatus(L('export_msh_save_failed'),true)
        return false
    end
    setStatus(string.format(L('export_msh_done'),#E.project.targets,outputPath),false)
    return true
end

local function newProject()
    destroyMesh()
    E.project=tProject.new(tLang.getLanguage())
    E.projectPath=nil
    E.modified=false
    E.selectedPoint=nil
    E.regionDrag=nil
    E.cameraDrag=nil
    E.cameraPan=nil
    E.pendingSurfacePick=nil
    E.camera={fx=0,fy=0,fz=0,distance=10,azimuth=0.45,elevation=0.22}
    E.overlayDirty=true
    E.regionNamesDirty=true
    E.regionStatusDirty=true
    E.regionTableDirty=true
    E.targetAvailabilityDirty=true
    E.targetPreviewDirty=true
    E.shapeMirrorCandidate=nil
    E.targetMirrorPlan=nil
    E.targetMirrorCandidate=nil
    E.targetBrushHistory={undo={},redo={}}
    E.targetEditHistory={undo={},redo={}}
    restoreEditorPreferences(E.project)
    setStatus(L('new_project'),false)
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
    elseif action.kind=='new' then
        newProject()
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

local function uniqueRegionName(name)
    local base=(name or ''):match('^%s*(.-)%s*$')
    if base=='' then base=string.format(L('region_default_name'),#E.project.regions+1) end
    local candidate=base
    local suffix=2
    local function exists(value)
        for _,region in ipairs(E.project.regions or {}) do
            if region.name==value then return true end
        end
        return false
    end
    while exists(candidate) do
        candidate=base..'_'..tostring(suffix)
        suffix=suffix+1
    end
    return candidate
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
    local name=uniqueRegionName(E.nameDraft)
    local shapeType=shapeTypeFromIndex(E.newShapeType)
    local region=tRegions.new(name,'polygon',{})
    region.shape=tShapes.new(shapeType,shapeType=='polygon' and true or E.newShapeFree)
    region.closed=false
    region.id=newRegionId()
    region.topologySignature=E.topology.signature
    E.project.regions[#E.project.regions+1]=region
    E.project.selectedRegion=#E.project.regions
    E.shapeMode=2
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

local function suggestMirrorShapeName(region)
    if not region then return end
    local sourceName=(region.name or ''):match('^%s*(.-)%s*$')
    if sourceName=='' then sourceName='region' end
    E.shapeMirrorNameDraft=sourceName..'_mirror'
    E.shapeMirrorNameSourceId=region.id
end

local function prepareShapeMirrorCandidate()
    local source=selectedRegion()
    E.shapeMirrorCandidate=nil
    E.shapeMirrorNeedsPrepare=false
    if source and E.shapeMirrorNameSourceId~=source.id then
        suggestMirrorShapeName(source)
    end
    local sourcePoints=source and E.topology and tPolygon.resolvePoints(source,E.topology) or {}
    if not source or not E.topology or not source.closed or #sourcePoints<3 then
        E.overlayDirty=true
        setStatus(L('shape_mirror_source_not_ready'),true)
        return false
    end
    local mirrored,errorCode=tShapes.mirror(source,E.topology,E.shapeMirrorAxis,0)
    if not mirrored then
        E.overlayDirty=true
        setStatus(L('shape_mirror_failed')..': '..tostring(errorCode),true)
        return false
    end
    local preview=tRegions.new((source.name or 'region')..'_mirror_preview',source.kind,mirrored.points)
    preview.id='shape_mirror_preview'
    preview.closed=mirrored.closed
    preview.shape=mirrored.shape
    preview.topologySignature=E.topology.signature
    if source.falloff then
        preview.falloff={mode=source.falloff.mode,radius=source.falloff.radius,
            curve=source.falloff.curve}
    end
    local rebuilt,rebuildError=tRegions.rebuild(preview,E.topology)
    if not rebuilt then
        E.overlayDirty=true
        setStatus(L('shape_mirror_failed')..': '..tostring(rebuildError),true)
        return false
    end
    E.shapeMirrorCandidate={region=preview,sourceId=source.id,axis=E.shapeMirrorAxis}
    E.overlayDirty=true
    setStatus(L('shape_mirror_ready'),false)
    return true
end

local function createMirroredShape(region)
    local candidate=E.shapeMirrorCandidate
    if not region or not candidate or candidate.sourceId~=region.id or
            candidate.axis~=E.shapeMirrorAxis then
        setStatus(L('shape_mirror_invalid'),true)
        return false
    end
    local name=(E.shapeMirrorNameDraft or ''):match('^%s*(.-)%s*$')
    if name=='' then
        setStatus(L('shape_mirror_name_required'),true)
        return false
    end
    for _,entry in ipairs(E.project.regions or {}) do
        if entry.name==name then
            setStatus(L('shape_mirror_name_taken'),true)
            return false
        end
    end
    local mirroredRegion=candidate.region
    mirroredRegion.name=name
    mirroredRegion.id=newRegionId()
    local rebuilt,rebuildError=tRegions.rebuild(mirroredRegion,E.topology)
    if not rebuilt then
        setStatus(L('shape_mirror_failed')..': '..tostring(rebuildError),true)
        return false
    end
    E.project.regions[#E.project.regions+1]=mirroredRegion
    E.project.selectedRegion=#E.project.regions
    E.shapeMode=2
    E.selectedPoint=nil
    E.shapeMirrorCandidate=nil
    E.shapeMirrorNameSourceId=nil
    E.shapeMirrorNeedsPrepare=false
    E.regionNamesDirty=true
    markChanged()
    setStatus(string.format(L('shape_mirror_created'),name),false)
    return true
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

local function suggestTargetName(region)
    if not region or E.targetNameSourceId==region.id then return end
    local sourceName=(region.name or ''):match('^%s*(.-)%s*$')
    if sourceName=='' then sourceName='region' end
    E.targetNameDraft=sourceName..'_target'
    E.targetNameSourceId=region.id
end

local function targetById(id)
    for index,target in ipairs(E.project.targets or {}) do
        if target.id==id then return index,target end
    end
    return nil,nil
end

local function selectedExpression()
    return E.project.expressions[E.project.selectedExpression]
end

local function newExpressionId()
    local suffix=1
    while true do
        local id='expression_'..tostring(suffix)
        local exists=false
        for _,expression in ipairs(E.project.expressions or {}) do
            if expression.id==id then exists=true; break end
        end
        if not exists then return id end
        suffix=suffix+1
    end
end

local function uniqueExpressionName(name)
    local base=(name or ''):match('^%s*(.-)%s*$')
    if base=='' then base=string.format(L('expression_default_name'),#E.project.expressions+1) end
    local candidate=base
    local suffix=2
    local function exists(value)
        for _,expression in ipairs(E.project.expressions or {}) do
            if expression.name==value then return true end
        end
        return false
    end
    while exists(candidate) do
        candidate=base..'_'..tostring(suffix)
        suffix=suffix+1
    end
    return candidate
end

local function ensureExpressionDraft()
    if E.expressionMode~=1 then return nil end
    if not E.expressionDraft then
        E.expressionDraft=tExpressions.new('expression_preview','expression_preview')
    end
    E.expressionDraft.name=(E.expressionNameDraft or ''):match('^%s*(.-)%s*$')
    return E.expressionDraft
end

local function expressionPreviewExpression()
    if E.expressionMode==1 then return ensureExpressionDraft() end
    return selectedExpression()
end

local function expressionPreviewKey(expression)
    if not expression then return 'none' end
    local parts={expression.id or 'expression'}
    for _,entry in ipairs(expression.weights or {}) do
        parts[#parts+1]=entry.targetId..':'..string.format('%.6f',entry.weight or 0)
    end
    return table.concat(parts,'|')
end

local function expressionPreviewData()
    local expression=expressionPreviewExpression()
    if not expression then return nil,nil end
    local key=expressionPreviewKey(expression)
    if E.expressionPreview.key==key and E.expressionPreview.target then
        return E.expressionPreview.target,E.expressionPreview.report
    end
    local report=tExpressions.compose(expression,E.project.targets,targetStatus)
    local target={id=expression.id,name=expression.name,deltas={}}
    for index,delta in pairs(report.deltas or {}) do
        if math.abs(delta.x)>1e-8 or math.abs(delta.y)>1e-8 or math.abs(delta.z)>1e-8 then
            target.deltas[#target.deltas+1]={index=index,x=delta.x,y=delta.y,z=delta.z}
        end
    end
    table.sort(target.deltas,function(left,right) return left.index<right.index end)
    E.expressionPreview={key=key,target=target,report=report}
    return target,report
end

local function invalidateExpressionPreview()
    E.expressionPreview.key=nil
    E.expressionPreview.target=nil
    E.expressionPreview.report=nil
    E.targetPreviewDirty=true
end

local function setExpressionWeight(expression,target,value)
    if not expression or not target then return false end
    local before=tExpressions.getWeight(expression,target.id)
    local clamped=math.max(0,math.min(1,tonumber(value) or 0))
    if math.abs(before-clamped)<=1e-6 then return false end
    tExpressions.setWeight(expression,target.id,target.name,clamped)
    invalidateExpressionPreview()
    if E.expressionMode~=1 then markChanged() end
    return true
end

local function createExpression()
    local draft=ensureExpressionDraft()
    local name=uniqueExpressionName(E.expressionNameDraft)
    local _,report=expressionPreviewData()
    if not draft or not report or report.active==0 then
        setStatus(L('expression_requires_weight'),true)
        return false
    end
    if report.missing>0 or report.stale>0 then
        setStatus(L('expression_invalid_targets'),true)
        return false
    end
    local expression=tExpressions.new(name,newExpressionId())
    expression.weights=tExpressions.copyWeights(draft)
    E.project.expressions[#E.project.expressions+1]=expression
    E.project.selectedExpression=#E.project.expressions
    E.expressionMode=2
    E.expressionDraft=nil
    E.expressionNameDraft=name
    markChanged()
    invalidateExpressionPreview()
    setStatus(string.format(L('expression_created'),name),false)
    return true
end

local function deleteSelectedExpression()
    if not selectedExpression() then return false end
    table.remove(E.project.expressions,E.project.selectedExpression)
    E.project.selectedExpression=math.max(1,math.min(#E.project.expressions>0 and #E.project.expressions or 1,
        E.project.selectedExpression))
    E.expressionDraft=nil
    markChanged()
    invalidateExpressionPreview()
    setStatus(L('expression_deleted'),false)
    return true
end

local function selectExpression(index)
    if not E.project.expressions[index] then return end
    E.project.selectedExpression=index
    E.expressionDraft=nil
    E.expressionNameDraft=E.project.expressions[index].name or 'expression'
    invalidateExpressionPreview()
    E.modified=true
end

local function selectedClip()
    return E.project.clips and E.project.clips[E.project.selectedClip]
end

local function uniqueClipName(name)
    local base=(name or ''):match('^%s*(.-)%s*$')
    if base=='' then base=string.format(L('clip_default_name'),#(E.project.clips or {})+1) end
    local candidate=base
    local suffix=2
    local function exists(value)
        for _,clip in ipairs(E.project.clips or {}) do
            if clip.name==value then return true end
        end
        return false
    end
    while exists(candidate) do
        candidate=base..'_'..tostring(suffix)
        suffix=suffix+1
    end
    return candidate
end

local function newClipId()
    local suffix=1
    while true do
        local id='clip_'..tostring(suffix)
        local exists=false
        for _,clip in ipairs(E.project.clips or {}) do
            if clip.id==id then exists=true;break end
        end
        if not exists then return id end
        suffix=suffix+1
    end
end

local function clipPreviewExpression(clip)
    if not clip then return nil end
    return {id=clip.id,name=clip.name,weights=tClips.sample(clip,E.clipTime)}
end

local function clipPreviewData()
    local clip=selectedClip()
    if not clip then return nil,nil end
    local expression=clipPreviewExpression(clip)
    local key=table.concat({clip.id or 'clip',string.format('%.5f',E.clipTime),
        tostring(#(clip.keys or {}))},':')
    if E.clipPreview.key==key and E.clipPreview.target then
        return E.clipPreview.target,E.clipPreview.report
    end
    local report=tExpressions.compose(expression,E.project.targets,targetStatus)
    local target={id='clip_preview_'..tostring(clip.id),name=clip.name,deltas={}}
    for index,delta in pairs(report.deltas or {}) do
        if math.abs(delta.x)>1e-8 or math.abs(delta.y)>1e-8 or math.abs(delta.z)>1e-8 then
            target.deltas[#target.deltas+1]={index=index,x=delta.x,y=delta.y,z=delta.z}
        end
    end
    table.sort(target.deltas,function(left,right) return left.index<right.index end)
    E.clipPreview={key=key,target=target,report=report}
    return target,report
end

local function invalidateClipPreview()
    E.clipPreview.key=nil
    E.clipPreview.target=nil
    E.clipPreview.report=nil
    E.targetPreviewDirty=true
end

local function hasReadyExpression()
    if not E.topology or E.project.topologySignature~=E.topology.signature then return false end
    for _,expression in ipairs(E.project.expressions or {}) do
        local report=tExpressions.inspect(expression,E.project.targets,targetStatus)
        if report.active>0 and report.missing==0 and report.stale==0 then return true end
    end
    return false
end

local function selectClip(index)
    if not E.project.clips[index] then return end
    E.project.selectedClip=index
    E.clipNameDraft=E.project.clips[index].name or 'clip'
    E.clipTime=0
    E.clipKeySelected=1
    E.clipPlaying=false
    invalidateClipPreview()
    E.modified=true
end

local function createClip()
    local name=uniqueClipName(E.clipNameDraft)
    local clip=tClips.new(name,newClipId())
    E.project.clips=E.project.clips or {}
    E.project.clips[#E.project.clips+1]=clip
    E.project.selectedClip=#E.project.clips
    E.clipMode=2
    E.clipNameDraft=name
    E.clipTime=0
    E.clipPlaying=false
    markChanged()
    setStatus(string.format(L('clip_created'),name),false)
    return true
end

local function deleteSelectedClip()
    if not selectedClip() then return false end
    table.remove(E.project.clips,E.project.selectedClip)
    E.project.selectedClip=math.max(1,math.min(#E.project.clips>0 and #E.project.clips or 1,
        E.project.selectedClip))
    E.clipTime=0
    E.clipPlaying=false
    invalidateClipPreview()
    markChanged()
    setStatus(L('clip_deleted'),false)
    return true
end

local function insertClipKey(weights)
    local clip=selectedClip()
    if not clip then return false end
    tClips.insertKey(clip,E.clipTime,weights)
    for index,key in ipairs(clip.keys or {}) do
        if math.abs((key.time or 0)-E.clipTime)<0.000001 then E.clipKeySelected=index;break end
    end
    invalidateClipPreview()
    markChanged()
    return true
end

local function updateClipPlayback(delta)
    if E.worktree~=4 or not E.clipPlaying then return end
    local clip=selectedClip()
    if not clip then E.clipPlaying=false;return end
    E.clipTime=E.clipTime+math.max(tonumber(delta) or 0,0)
    if E.clipTime>clip.duration then
        if clip.loop then E.clipTime=E.clipTime%clip.duration
        else E.clipTime=clip.duration;E.clipPlaying=false end
    end
    invalidateClipPreview()
end

local function invalidateMorphPreview()
    E.preview.key=nil
    E.preview.target=nil
    E.preview.report=nil
    E.preview.source=nil
    E.targetPreviewDirty=true
end

local function morphPreviewData()
    local sourceId,sourceName,expression
    if E.previewMode==1 then
        local expressions=E.project.expressions or {}
        local index=math.max(1,math.min(#expressions,E.previewExpressionSelected))
        expression=expressions[index]
        if not expression then return nil,nil end
        E.previewExpressionSelected=index
        sourceId='expression_'..tostring(expression.id)
        sourceName=expression.name
    else
        local clips=E.project.clips or {}
        local index=math.max(1,math.min(#clips,E.previewClipSelected))
        local clip=clips[index]
        if not clip then return nil,nil end
        E.previewClipSelected=index
        sourceId='clip_'..tostring(clip.id)
        sourceName=clip.name
        expression={id=clip.id,name=clip.name,weights=tClips.sample(clip,E.previewClipTime)}
    end
    local key=table.concat({sourceId or 'preview',string.format('%.5f',E.previewClipTime),
        tostring(E.previewMode)},':')
    if E.preview.key==key and E.preview.target then
        return E.preview.target,E.preview.report,E.preview.source
    end
    local report=tExpressions.compose(expression,E.project.targets,targetStatus)
    local target={id='morph_preview_'..sourceId,name=sourceName,deltas={}}
    for index,delta in pairs(report.deltas or {}) do
        if math.abs(delta.x)>1e-8 or math.abs(delta.y)>1e-8 or math.abs(delta.z)>1e-8 then
            target.deltas[#target.deltas+1]={index=index,x=delta.x,y=delta.y,z=delta.z}
        end
    end
    table.sort(target.deltas,function(left,right) return left.index<right.index end)
    E.preview={key=key,target=target,report=report,source=expression}
    return target,report,expression
end

local function updateMorphPreviewPlayback(delta)
    if E.worktree~=5 or E.previewMode~=2 or not E.previewClipPlaying then return end
    local clip=(E.project.clips or {})[E.previewClipSelected]
    if not clip then E.previewClipPlaying=false;return end
    E.previewClipTime=E.previewClipTime+math.max(tonumber(delta) or 0,0)
    if E.previewClipTime>clip.duration then
        if clip.loop then E.previewClipTime=E.previewClipTime%clip.duration
        else E.previewClipTime=clip.duration;E.previewClipPlaying=false end
    end
    invalidateMorphPreview()
end

local function pushTargetBrushHistory(target,before,after)
    if not target or not before or not after then return end
    local history=E.targetBrushHistory
    history.undo[#history.undo+1]={targetId=target.id,before=before,after=after}
    while #history.undo>30 do table.remove(history.undo,1) end
    history.redo={}
end

local function restoreTargetBrushSnapshot(target,snapshot)
    if not target or type(snapshot)~='table' then return false end
    target.vertices={}
    for _,vertex in ipairs(snapshot.vertices or {}) do
        target.vertices[#target.vertices+1]={index=vertex.index,weight=vertex.weight}
    end
    target.deltas={}
    for _,delta in ipairs(snapshot.deltas or {}) do
        target.deltas[#target.deltas+1]={index=delta.index,x=delta.x,y=delta.y,z=delta.z}
    end
    target.pivot=snapshot.pivot and {x=snapshot.pivot.x,y=snapshot.pivot.y,z=snapshot.pivot.z} or nil
    return true
end

local function pushTargetEditHistory(target,before,after,kind)
    if not target or not before or not after then return end
    local history=E.targetEditHistory
    history.undo[#history.undo+1]={targetId=target.id,before=before,after=after,kind=kind or 'edit'}
    while #history.undo>30 do table.remove(history.undo,1) end
    history.redo={}
end

local function applyTargetEditHistory(redo)
    local history=E.targetEditHistory
    local source=redo and history.redo or history.undo
    local destination=redo and history.undo or history.redo
    local entry=source[#source]
    if not entry then return false end
    local index,target=targetById(entry.targetId)
    if not target then
        clearTargetEditHistory()
        setStatus(L('target_edit_history_invalid'),true)
        return false
    end
    local snapshot=redo and entry.after or entry.before
    if not restoreTargetBrushSnapshot(target,snapshot) then return false end
    table.remove(source)
    destination[#destination+1]=entry
    E.project.selectedTarget=index
    markChanged()
    setStatus(L(redo and 'target_edit_redo_done' or 'target_edit_undo_done'),false)
    return true
end

local function applyTargetBrushHistory(redo)
    local history=E.targetBrushHistory
    local source=redo and history.redo or history.undo
    local destination=redo and history.undo or history.redo
    local entry=source[#source]
    if not entry then return false end
    local index,target=targetById(entry.targetId)
    if not target then
        clearTargetBrushHistory()
        setStatus(L('target_brush_history_invalid'),true)
        return false
    end
    local snapshot=redo and entry.after or entry.before
    if not restoreTargetBrushSnapshot(target,snapshot) then return false end
    table.remove(source)
    destination[#destination+1]=entry
    E.project.selectedTarget=index
    markChanged()
    setStatus(L(redo and 'target_brush_redo_done' or 'target_brush_undo_done'),false)
    return true
end

local function targetRegion(target)
    if not target then return nil,nil end
    return regionIndexById(target.sourceRegionId)
end

targetStatus=function(target)
    local _,region=targetRegion(target)
    return tTargets.status(target,region,E.topology)
end

local function mirrorPreviewReady(target)
    local plan=E.targetMirrorPlan
    return E.targetMode==5 and target and plan and plan.sourceId==target.id and plan.result and
        E.targetMirrorCandidate and E.targetMirrorCandidate.target and
        targetStatus(target)=='ready'
end

local function targetPreviewReady(target)
    if E.worktree~=2 or not target or targetStatus(target)~='ready' then return false end
    if E.targetMode==2 or E.targetMode==4 then return true end
    return mirrorPreviewReady(target)
end

local function expressionPreviewReady()
    if E.worktree~=3 then return false end
    local _,report=expressionPreviewData()
    return report and report.active>0
end

local function restoreTargetPreviewDeltas()
    local target=E.worktree==3 and expressionPreviewData() or
        E.worktree==4 and clipPreviewData() or
        E.worktree==5 and morphPreviewData() or selectedTarget()
    if target and E.targetPreview.surface then
        tTargetPreview.setDeltas(E,target)
    end
end

local targetEditSelection

local function syncTargetEditDraft(target)
    local draft=nil
    local key=table.concat({target and target.id or 'none',tostring(E.targetMode),
        tostring(E.targetEditMode),tostring(E.targetFeatherRings)},':')
    if E.worktree==2 and targetPreviewReady(target) and E.targetMode==2 and
            (E.targetEditMode==1 or E.targetEditMode==2) then
        local pivot=tTargets.selectionPivot(target,E.topology.vertices,E.meshBounds)
        local rx,ry,rz=E.targetRx,E.targetRy,E.targetRz
        local dx,dy,dz=E.targetDx,E.targetDy,E.targetDz
        key=table.concat({key,string.format('%.5f',dx),string.format('%.5f',dy),
            string.format('%.5f',dz),string.format('%.5f',rx),string.format('%.5f',ry),
            string.format('%.5f',rz),string.format('%.5f',pivot.x),string.format('%.5f',pivot.y),
            string.format('%.5f',pivot.z)},':')
        local hasTransform=(E.targetEditMode==1 and
            (math.abs(dx)>1e-8 or math.abs(dy)>1e-8 or math.abs(dz)>1e-8)) or
            (E.targetEditMode==2 and
            (math.abs(rx)>1e-8 or math.abs(ry)>1e-8 or math.abs(rz)>1e-8))
        if hasTransform then
            local _,weights=targetEditSelection(target)
            draft={mode=E.targetEditMode==1 and 'translate' or 'rotate',weights=weights}
        end
        if E.targetEditMode==2 and draft then
            draft.pivot=pivot
            draft.basis=tTargets.rotationBasis(math.rad(rx),math.rad(ry),math.rad(rz))
        elseif E.targetEditMode==1 and draft then
            draft.dx,draft.dy,draft.dz=dx,dy,dz
        end
    end
    if E.targetEditDraftKey==key then return end
    E.targetEditDraftKey=key
    E.targetEditDraft=draft
    if E.targetPreview.surface then E.targetPreview.bufferDirty=true end
end

local function refreshTargetPreview()
    local target=selectedTarget()
    if E.worktree==3 then
        local expressionTarget=expressionPreviewData()
        if not expressionTarget or not expressionPreviewReady() then
            tTargetPreview.destroy(E)
            E.targetPreviewDirty=false
            return
        end
        tTargetPreview.destroy(E)
        local ok,errorCode=tTargetPreview.build(E,expressionTarget,E.targetPreviewWeight)
        E.targetPreviewDirty=false
        if not ok then
            setStatus(L('expression_preview_failed')..': '..tostring(errorCode),true)
        end
        return
    end
    if E.worktree==4 then
        local clipTarget,clipReport=clipPreviewData()
        if not clipTarget or not clipReport or clipReport.active==0 or clipReport.missing>0 or
                clipReport.stale>0 then
            tTargetPreview.destroy(E)
            E.targetPreviewDirty=false
            return
        end
        tTargetPreview.destroy(E)
        local ok,errorCode=tTargetPreview.build(E,clipTarget,E.clipPreviewWeight)
        E.targetPreviewDirty=false
        if not ok then setStatus(L('clip_preview_failed')..': '..tostring(errorCode),true) end
        return
    end
    if E.worktree==5 then
        if E.previewRuntime and E.previewRuntime.skeletalPlaying then
            if E.targetPreview.surface then tTargetPreview.destroy(E) end
            E.targetPreviewDirty=false
            return
        end
        if E.previewRuntime and E.previewRuntime.comparisonMode~=1 then
            if E.targetPreview.surface then tTargetPreview.destroy(E) end
            E.targetPreviewDirty=false
            return
        end
        local previewTarget,previewReport=morphPreviewData()
        if not previewTarget or not previewReport or previewReport.active==0 or previewReport.missing>0 or
                previewReport.stale>0 then
            tTargetPreview.destroy(E)
            E.targetPreviewDirty=false
            return
        end
        tTargetPreview.destroy(E)
        local ok,errorCode=tTargetPreview.build(E,previewTarget,E.targetPreviewWeight)
        E.targetPreviewDirty=false
        if not ok then setStatus(L('preview_failed')..': '..tostring(errorCode),true) end
        return
    end
    if not targetPreviewReady(target) then
        tTargetPreview.destroy(E)
        E.targetPreviewDirty=false
        return
    end
    local previewTarget=target
    if mirrorPreviewReady(target) then
        previewTarget=E.targetMirrorCandidate.target
        previewTarget.id=target.id
    end
    tTargetPreview.destroy(E)
    local ok,errorCode=tTargetPreview.build(E,previewTarget,E.targetPreviewWeight)
    E.targetPreviewDirty=false
    if not ok then
        setStatus(L('target_preview_failed')..': '..tostring(errorCode),true)
    end
end

local function syncTargetPreview()
    local target=selectedTarget()
    syncTargetEditDraft(target)
    if E.worktree==3 then
        local expressionTarget=expressionPreviewData()
        if not expressionTarget or not expressionPreviewReady() then
            if E.targetPreview.surface then tTargetPreview.destroy(E) end
            return
        end
        if E.targetPreviewDirty or E.targetPreview.targetId~=expressionTarget.id then
            refreshTargetPreview()
        end
        if E.targetPreview.surface then
            tTargetPreview.setWeight(E,E.targetPreviewWeight)
            tTargetPreview.setVisible(E,E.targetPreviewVisible)
            tTargetPreview.update(E)
        end
        return
    end
    if E.worktree==4 then
        local clipTarget,clipReport=clipPreviewData()
        if not clipTarget or not clipReport or clipReport.active==0 or clipReport.missing>0 or
                clipReport.stale>0 then
            if E.targetPreview.surface then tTargetPreview.destroy(E) end
            return
        end
        if E.targetPreviewDirty and E.targetPreview.surface and E.targetPreview.targetId==clipTarget.id then
            tTargetPreview.setDeltas(E,clipTarget)
            E.targetPreviewDirty=false
        elseif E.targetPreviewDirty or E.targetPreview.targetId~=clipTarget.id then
            refreshTargetPreview()
        end
        if E.targetPreview.surface then
            tTargetPreview.setWeight(E,E.clipPreviewWeight)
            tTargetPreview.setVisible(E,E.targetPreviewVisible)
            tTargetPreview.update(E)
        end
        return
    end
    if E.worktree==5 then
        local previewTarget,previewReport,previewSource=morphPreviewData()
        if not previewTarget or not previewReport or previewReport.active==0 or previewReport.missing>0 or
                previewReport.stale>0 then
            if E.targetPreview.surface then tTargetPreview.destroy(E) end
            E.targetPreviewDirty=false
            if E.previewRuntime and E.previewRuntime.previewReady then
                tPreviewRuntime.applyMorphWeights(E.previewRuntime,E.mesh,nil,E.project.targets,0,dpCall)
            end
            return
        end
        local nativeNeeded=E.previewRuntime and
            (E.previewRuntime.comparisonMode~=1 or E.previewRuntime.skeletalPlaying)
        if not nativeNeeded then
            if E.targetPreviewDirty and E.targetPreview.surface and
                    E.targetPreview.targetId==previewTarget.id then
                tTargetPreview.setDeltas(E,previewTarget)
                E.targetPreviewDirty=false
            elseif E.targetPreviewDirty or E.targetPreview.targetId~=previewTarget.id then
                refreshTargetPreview()
            end
            if E.targetPreview.surface then
                tTargetPreview.setWeight(E,E.targetPreviewWeight)
                tTargetPreview.setVisible(E,E.targetPreviewVisible)
                tTargetPreview.update(E)
            end
            return
        end
        local runtimeOk=true
        if E.targetPreviewDirty or not E.previewRuntime.previewReady then
            runtimeOk=tPreviewRuntime.ensure(E.previewRuntime,E.mesh,E.meshDebug,E.topology,
                E.project.targets,E.project.meshPath,dpCall)
        end
        if not runtimeOk and E.previewRuntime.lastError and
                E.previewRuntime.lastReportedError~=E.previewRuntime.lastError then
            setStatus(L(E.previewRuntime.lastError),true)
            E.previewRuntime.lastReportedError=E.previewRuntime.lastError
        end
        if runtimeOk then
            if E.targetPreview.surface then tTargetPreview.destroy(E) end
            local applied=tPreviewRuntime.applyMorphWeights(E.previewRuntime,E.mesh,previewSource,
                E.project.targets,E.targetPreviewWeight,dpCall)
            if applied then
                E.targetPreviewDirty=false
                return
            end
        end
        -- Keep the dynamic overlay as a fallback if the temporary runtime MSH cannot be built.
        if E.previewRuntime and E.previewRuntime.comparisonMode~=1 then
            if E.targetPreview.surface then tTargetPreview.destroy(E) end
            E.targetPreviewDirty=false
            return
        end
        if E.targetPreviewDirty and E.targetPreview.surface and
                E.targetPreview.targetId==previewTarget.id then
            tTargetPreview.setDeltas(E,previewTarget)
            E.targetPreviewDirty=false
        elseif E.targetPreviewDirty or E.targetPreview.targetId~=previewTarget.id then
            refreshTargetPreview()
        end
        if E.targetPreview.surface then
            tTargetPreview.setWeight(E,E.targetPreviewWeight)
            tTargetPreview.setVisible(E,E.targetPreviewVisible)
            tTargetPreview.update(E)
        end
        return
    end
    if not targetPreviewReady(target) then
        if E.targetPreview.surface then tTargetPreview.destroy(E) end
        return
    end
    if E.targetPreviewDirty or E.targetPreview.targetId~=target.id then
        refreshTargetPreview()
    end
    if E.targetPreview.surface then
        tTargetPreview.setWeight(E,E.targetPreviewWeight)
        -- A non-zero batch edit is intentionally shown with the original material
        -- so the artist can inspect the actual textured deformation before commit.
        tTargetPreview.setVisible(E,E.targetEditDraft==nil and E.targetPreviewVisible)
        tTargetPreview.update(E)
    end
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

local function suggestMirrorTargetName(target)
    if not target then return end
    local sourceName=(target.name or ''):match('^%s*(.-)%s*$')
    if sourceName=='' then sourceName='target' end
    E.targetMirrorNameDraft=sourceName..'_mirror'
    E.targetMirrorNameTargetId=target.id
end

local function selectTarget(index)
    local target=E.project.targets[index]
    if not target then return end
    suggestMirrorTargetName(target)
    if E.project.selectedTarget~=index then
        clearTargetBrushHistory()
        clearTargetEditHistory()
    end
    E.project.selectedTarget=index
    local regionIndex=targetRegion(target)
    if regionIndex then E.project.selectedRegion=regionIndex end
    E.selectedPoint=nil
    E.targetPreviewDirty=true
    E.targetBrush.geometry=nil
    E.targetBrush.geometryKey=nil
    E.targetBrush.hoverHit=nil
    E.targetFeatherCache=nil
    E.targetMirrorPlan=nil
    E.targetMirrorCandidate=nil
    E.targetMirrorNeedsPrepare=E.targetMode==5
    E.targetEditDraft=nil
    E.targetEditDraftKey=nil
    E.targetGizmo.dirty=true
    E.overlayDirty=true
    E.regionStatusDirty=true
    E.modified=true
end

local function targetMirrorExtent()
    if not E.meshBounds then return 1 end
    return math.max(E.meshBounds.maxX-E.meshBounds.minX,
        E.meshBounds.maxY-E.meshBounds.minY,E.meshBounds.maxZ-E.meshBounds.minZ,1e-6)
end

local function targetMirrorPlanMatches(target)
    local plan=E.targetMirrorPlan
    return target and plan and plan.sourceId==target.id and
        plan.name==((E.targetMirrorNameDraft or ''):match('^%s*(.-)%s*$')) and
        plan.axis==E.targetMirrorAxis and
        math.abs((plan.tolerance or 0)-E.targetMirrorTolerance)<=1e-12 and plan.result
end

local function prepareTargetMirrorCandidate(target)
    local sourceTarget=selectedTarget()
    local _,sourceRegion=targetRegion(target or sourceTarget)
    sourceTarget=target or sourceTarget
    if sourceTarget and E.targetMirrorNameTargetId~=sourceTarget.id then
        suggestMirrorTargetName(sourceTarget)
    end
    E.targetMirrorNeedsPrepare=false
    E.targetMirrorCandidate=nil
    E.targetMirrorPlan=nil
    if not sourceTarget or not sourceRegion or targetStatus(sourceTarget)~='ready' then
        E.targetPreviewDirty=true
        E.overlayDirty=true
        setStatus(L('target_mirror_source_not_ready'),true)
        return false
    end
    local mirroredShape,errorCode=tShapes.mirror(sourceRegion,E.topology,E.targetMirrorAxis,0)
    if not mirroredShape then
        E.targetPreviewDirty=true
        E.overlayDirty=true
        setStatus(L('target_mirror_polygon_failed')..': '..tostring(errorCode),true)
        return false
    end
    local analysis,analysisError=tTargetMirror.analyze(sourceTarget,E.topology.vertices,
        E.targetMirrorAxis,0,math.max(targetMirrorExtent()*E.targetMirrorTolerance,1e-7))
    if not analysis then
        E.targetPreviewDirty=true
        E.overlayDirty=true
        setStatus(L('target_mirror_invalid')..': '..tostring(analysisError),true)
        return false
    end
    local previewRegion=tRegions.new((sourceRegion.name or 'region')..'_mirror_preview',
        sourceRegion.kind,mirroredShape.points)
    previewRegion.id='mirror_preview'
    previewRegion.closed=mirroredShape.closed
    previewRegion.shape=mirroredShape.shape
    previewRegion.topologySignature=E.topology.signature
    if sourceRegion.falloff then
        previewRegion.falloff={mode=sourceRegion.falloff.mode,
            radius=sourceRegion.falloff.radius,curve=sourceRegion.falloff.curve}
    end
    local rebuilt,rebuildError=tRegions.rebuild(previewRegion,E.topology)
    if not rebuilt then
        E.targetPreviewDirty=true
        E.overlayDirty=true
        setStatus(L('target_mirror_polygon_failed')..': '..tostring(rebuildError),true)
        return false
    end
    local mirroredData=tTargetMirror.create(sourceTarget,analysis,E.targetMirrorAxis,0)
    if not mirroredData then
        E.targetPreviewDirty=true
        E.overlayDirty=true
        setStatus(L('target_mirror_invalid'),true)
        return false
    end
    local targetName=(E.targetMirrorNameDraft or ''):match('^%s*(.-)%s*$')
    local previewTarget={id=sourceTarget.id,name=targetName,
        sourceRegionId=previewRegion.id,topologySignature=E.topology.signature,
        sourceFingerprint=previewRegion.geometryFingerprint,vertices=mirroredData.vertices,
        deltas=mirroredData.deltas,pivot=mirroredData.pivot}
    tTargets.normalize(previewTarget,#E.topology.vertices)
    E.targetMirrorPlan={sourceId=sourceTarget.id,name=targetName,
        axis=E.targetMirrorAxis,tolerance=E.targetMirrorTolerance,result=analysis,
        candidateRegion=previewRegion,candidateTarget=previewTarget}
    E.targetMirrorCandidate={region=previewRegion,target=previewTarget,sourceId=sourceTarget.id}
    E.targetPreviewDirty=true
    E.overlayDirty=true
    setStatus(L('target_mirror_analyzed'),false)
    return true
end

local function analyzeTargetMirror(target)
    return prepareTargetMirrorCandidate(target)
end

local function createMirroredTarget(target)
    if not target or not targetMirrorPlanMatches(target) then return false end
    local plan=E.targetMirrorPlan
    local result=plan.result
    local approximateEligible=tTargetMirror.isApproximateEligible(result)
    local exact=result.unmatchedActive==0 and result.ambiguous==0
    if not exact and not (approximateEligible and E.targetMirrorAllowApprox==true) then
        setStatus(L('target_mirror_invalid'),true)
        return false
    end
    local name=plan.name or ''
    if name=='' then
        setStatus(L('target_mirror_name_required'),true)
        return false
    end
    for _,entry in ipairs(E.project.targets or {}) do
        if entry.name==name then
            setStatus(L('target_mirror_name_taken'),true)
            return false
        end
    end
    local candidate=E.targetMirrorCandidate
    if not candidate or not candidate.region or not candidate.target then
        setStatus(L('target_mirror_invalid'),true)
        return false
    end
    local _,sourceRegion=targetRegion(target)
    local regionName=uniqueRegionName((sourceRegion and sourceRegion.name or 'region')..'_mirror')
    local mirroredRegion=candidate.region
    mirroredRegion.name=regionName
    mirroredRegion.id=newRegionId()
    local rebuilt,rebuildError=tRegions.rebuild(mirroredRegion,E.topology)
    if not rebuilt then
        setStatus(L('target_mirror_polygon_failed')..': '..tostring(rebuildError),true)
        return false
    end
    local created=candidate.target
    created.id=newTargetId()
    created.name=name
    created.sourceRegionId=mirroredRegion.id
    created.topologySignature=E.topology.signature
    created.sourceFingerprint=mirroredRegion.geometryFingerprint
    created.mirrorSourceId=target.id
    created.mirrorAxis=E.targetMirrorAxis
    tTargets.normalize(created,#E.topology.vertices)
    E.project.regions[#E.project.regions+1]=mirroredRegion
    E.project.targets[#E.project.targets+1]=created
    E.project.selectedRegion=#E.project.regions
    E.project.selectedTarget=#E.project.targets
    E.targetMode=2
    E.targetEditMode=1
    E.regionNamesDirty=true
    E.selectedPoint=nil
    E.targetMirrorPlan=nil
    E.targetMirrorCandidate=nil
    E.targetMirrorNeedsPrepare=false
    E.targetPreviewDirty=true
    clearTargetBrushHistory()
    clearTargetEditHistory()
    markChanged()
    setStatus(string.format(L('target_mirror_committed'),regionName,name),false)
    return true
end

local drawTargetMirrorAxisControl

local function drawTargetMirrorControls(target)
    tImGui.Separator()
    tImGui.Text(L('target_mirror_title'))
    tImGui.TextWrapped(L('target_mirror_help'))
    tImGui.SetNextItemWidth(220)
    local nameChanged,name=tImGui.InputText(L('target_mirror_name')..'##mreTargetMirrorName',
        E.targetMirrorNameDraft)
    if nameChanged then
        E.targetMirrorNameDraft=name
        E.targetMirrorPlan=nil
        E.targetMirrorCandidate=nil
        E.targetMirrorNeedsPrepare=false
        E.targetPreviewDirty=true
        E.overlayDirty=true
        E.modified=true
    end
    tImGui.SetNextItemWidth(220)
    local toleranceChanged,tolerance=tImGui.SliderFloat(
        L('target_mirror_tolerance')..'##mreTargetMirrorTolerance',E.targetMirrorTolerance,
        0.00001,0.01,'%.5f')
    showHelpTooltip(L('target_mirror_tolerance_help'))
    if toleranceChanged then
        E.targetMirrorTolerance=math.max(0.00001,math.min(0.01,tolerance))
        E.targetMirrorPlan=nil
        E.targetMirrorCandidate=nil
        E.targetPreviewDirty=true
        E.overlayDirty=true
        E.modified=true
    end
    drawTargetMirrorAxisControl()
    tImGui.BeginDisabled(not target or targetStatus(target)~='ready')
    if tImGui.Button(L('target_mirror_analyze')..'##mreTargetMirrorAnalyze') then
        analyzeTargetMirror(target)
    end
    tImGui.EndDisabled()
    showHelpTooltip(L('target_mirror_analyze_help'))
    local plan=targetMirrorPlanMatches(target) and E.targetMirrorPlan or nil
    if plan and plan.result then
        local result=plan.result
        tImGui.TextWrapped(string.format(L('target_mirror_report'),result.matched,result.total,
            result.unmatchedActive,result.ambiguous))
        local approximateEligible=tTargetMirror.isApproximateEligible(result)
        if approximateEligible and (result.unmatchedActive>0 or result.ambiguous>0) then
            local allow=tImGui.Checkbox(L('target_mirror_allow_approx')..'##mreTargetMirrorApprox',
                E.targetMirrorAllowApprox)
            if allow~=E.targetMirrorAllowApprox then
                E.targetMirrorAllowApprox=allow
                E.modified=true
            end
            showHelpTooltip(L('target_mirror_allow_approx_help'))
        end
        local name=(E.targetMirrorNameDraft or ''):match('^%s*(.-)%s*$')
        local nameTaken=name==''
        for _,entry in ipairs(E.project.targets or {}) do
            if entry.name==name then nameTaken=true; break end
        end
        local exact=result.unmatchedActive==0 and result.ambiguous==0
        local canCreate=E.targetMirrorCandidate~=nil and not nameTaken and (exact or
            approximateEligible and E.targetMirrorAllowApprox==true)
        tImGui.BeginDisabled(not canCreate)
        if tImGui.Button(L('target_mirror_create')..'##mreTargetMirrorCreate') then
            createMirroredTarget(target)
        end
        tImGui.EndDisabled()
        showHelpTooltip(L('target_mirror_create_help'))
        if not canCreate then
            if nameTaken then
                tImGui.TextWrapped(L('target_mirror_name_taken'))
            elseif result.ambiguous>0 then
                tImGui.TextWrapped(L('target_mirror_ambiguous'))
            elseif result.unmatchedActive>0 then
                tImGui.TextWrapped(L('target_mirror_unmatched'))
            else
                tImGui.TextWrapped(L('target_mirror_approx_needed'))
            end
        end
    end
end

drawTargetMirrorAxisControl=function()
    if E.targetMode~=5 then return end
    if #(E.project.targets or {})==0 then return end
    local axes={L('target_mirror_axis_x'),L('target_mirror_axis_y'),L('target_mirror_axis_z')}
    tImGui.SetNextItemWidth(220)
    local axisChanged,axis=tImGui.Combo(L('target_mirror_axis')..'##mreTargetMirrorAxis',
        E.targetMirrorAxis,axes)
    showHelpTooltip(L('target_mirror_axis_help'))
    if axisChanged then
        E.targetMirrorAxis=math.max(1,math.min(3,axis))
        E.targetMirrorPlan=nil
        E.targetMirrorCandidate=nil
        E.targetPreviewDirty=true
        E.overlayDirty=true
        E.modified=true
    end
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
    suggestMirrorTargetName(target)
    E.project.targets[#E.project.targets+1]=target
    E.project.selectedTarget=#E.project.targets
    clearTargetBrushHistory()
    clearTargetEditHistory()
    E.targetMode=2
    E.targetPreviewDirty=true
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
    clearTargetBrushHistory()
    clearTargetEditHistory()
    markChanged()
    E.targetPreviewDirty=true
    setStatus(L('target_rebuilt'),false)
    return true
end

local function applyTargetTranslation()
    local target=selectedTarget()
    if not target or targetStatus(target)~='ready' then
        setStatus(L('target_not_ready'),true)
        return false
    end
    local indices,weights=targetEditSelection(target)
    local before=tTargets.copyTargetData(target)
    local changed=tTargets.applyTranslation(target,E.targetDx,E.targetDy,E.targetDz,indices,weights)
    if changed==0 then
        setStatus(L('target_no_translation'),false)
        return true
    end
    -- The entered translation is a pending operation, not a persistent offset.
    -- Clear it after commit so the live draft is not applied a second time.
    E.targetDx,E.targetDy,E.targetDz=0,0,0
    pushTargetEditHistory(target,before,tTargets.copyTargetData(target),'translate')
    clearTargetBrushHistory()
    markChanged()
    E.targetPreviewDirty=true
    setStatus(string.format(L('target_translation_applied'),changed),false)
    return true
end

local function resetSelectedTargetDeltas()
    local target=selectedTarget()
    if not target then return false end
    local before=tTargets.copyTargetData(target)
    if not tTargets.resetDeltas(target) then
        setStatus(L('target_already_neutral'),false)
        return true
    end
    pushTargetEditHistory(target,before,tTargets.copyTargetData(target),'reset')
    clearTargetBrushHistory()
    markChanged()
    setStatus(L('target_reset'),false)
    return true
end

local function applyTargetRotation()
    local target=selectedTarget()
    if not target or targetStatus(target)~='ready' or not E.topology then
        setStatus(L('target_not_ready'),true)
        return false
    end
    local pivot=tTargets.selectionPivot(target,E.topology.vertices,E.meshBounds)
    local indices,weights=targetEditSelection(target)
    local before=tTargets.copyTargetData(target)
    local changed=tTargets.applyRotation(target,E.topology.vertices,
        math.rad(E.targetRx),math.rad(E.targetRy),math.rad(E.targetRz),pivot,indices,weights)
    if changed==0 then
        setStatus(L('target_no_rotation'),false)
        return true
    end
    E.targetRx,E.targetRy,E.targetRz=0,0,0
    pushTargetEditHistory(target,before,tTargets.copyTargetData(target),'rotate')
    clearTargetBrushHistory()
    markChanged()
    setStatus(string.format(L('target_rotation_applied'),changed),false)
    return true
end

local function applySelectedTargetEdit()
    if E.targetEditMode==2 then return applyTargetRotation() end
    if E.targetEditMode==3 then return resetSelectedTargetDeltas() end
    return applyTargetTranslation()
end

local function deleteSelectedTarget()
    if not selectedTarget() then return false end
    table.remove(E.project.targets,E.project.selectedTarget)
    E.project.selectedTarget=math.max(1,math.min(#E.project.targets>0 and #E.project.targets or 1,
        E.project.selectedTarget))
    clearTargetBrushHistory()
    clearTargetEditHistory()
    E.overlayDirty=true
    markChanged()
    E.targetPreviewDirty=true
    setStatus(L('target_deleted'),false)
    return true
end

targetEditSelection=function(target)
    if not target or not E.topology then return {},{} end
    local key=tostring(target.id)..':'..tostring(E.targetFeatherRings)..':'..tostring(E.topology.signature)
    local cache=E.targetFeatherCache
    if cache and cache.key==key then return cache.indices,cache.weights end
    -- The polygon remains the hard authoring mask.  A previous edit may have
    -- persisted feather vertices into target.vertices so the exported target
    -- can contain them, but a later batch edit must start from the source
    -- region again instead of growing the selection from that old union.
    local selectionTarget=target
    local _,region=targetRegion(target)
    if region and #(region.derivedVertexWeights or {})>0 then
        selectionTarget={vertices=region.derivedVertexWeights}
    end
    local indices,weights=tTargets.featherSelection(E.topology,selectionTarget,E.targetFeatherRings)
    E.targetFeatherCache={key=key,indices=indices,weights=weights}
    return indices,weights
end

local function targetGizmoExtent()
    local bounds=E.meshBounds
    if not bounds then return 1 end
    return math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
end

local function refreshTargetGizmo()
    local gizmo=E.targetGizmo
    local target=E.worktree==2 and E.targetMode==2 and E.targetEditMode==2 and selectedTarget() or nil
    if not target or targetStatus(target)~='ready' or not E.mesh then
        if gizmo.visible then tTargetGizmo.hide(gizmo) end
        return
    end
    if gizmo.visible and not gizmo.dirty and gizmo.targetId==target.id then return end
    local pivot=tTargets.selectionPivot(target,E.topology.vertices,E.meshBounds)
    tTargetGizmo.refresh(gizmo,pivot,targetGizmoExtent(),gizmo.drag)
    gizmo.targetId=target.id
    gizmo.dirty=false
end

local function beginTargetGizmoDrag(x,y)
    local gizmo=E.targetGizmo
    if not gizmo.visible or not gizmo.origin then return false end
    local ok,ox,oy,oz,dx,dy,dz=dpCall(mbm.getPickRay,x,y)
    if not ok or not ox or not dx then return false end
    local origin,direction={x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz}
    local ring=tTargetGizmo.hitRing(origin,direction,gizmo.origin,gizmo.radius)
    if ring then
        local field=ring.name=='x' and 'targetRx' or ring.name=='y' and 'targetRy' or 'targetRz'
        gizmo.drag={kind='ring',name=ring.name,axis=ring.axis,startDirection=ring.direction,
            baseAngle=E[field] or 0,origin={x=gizmo.origin.x,y=gizmo.origin.y,z=gizmo.origin.z}}
        return true
    end
    local axisName=tTargetGizmo.hitAxis(origin,direction,gizmo.origin,gizmo.length,
        math.max(gizmo.radius*0.08,0.003))
    if not axisName then return false end
    local axis=tTargetGizmo.axes[axisName]
    local parameter=tTargetGizmo.axisParameter(origin,direction,gizmo.origin,axis)
    if not parameter then return false end
    gizmo.drag={kind='axis',name=axisName,axis=axis,startParameter=parameter,
        origin={x=gizmo.origin.x,y=gizmo.origin.y,z=gizmo.origin.z},moved=false}
    return true
end

local function updateTargetGizmoDrag(x,y)
    local drag=E.targetGizmo.drag
    if not drag then return false end
    local ok,ox,oy,oz,dx,dy,dz=dpCall(mbm.getPickRay,x,y)
    if not ok or not ox or not dx then return true end
    local origin,direction={x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz}
    if drag.kind=='axis' then
        local parameter=tTargetGizmo.axisParameter(origin,direction,drag.origin,drag.axis)
        if parameter then
            local amount=parameter-drag.startParameter
            if math.abs(amount)>1e-6 then
                local target=selectedTarget()
                if target then
                    target.pivot={x=drag.origin.x+drag.axis.x*amount,
                        y=drag.origin.y+drag.axis.y*amount,z=drag.origin.z+drag.axis.z*amount}
                    drag.moved=true
                    E.targetEditDraftKey=nil
                    E.targetGizmo.dirty=true
                    E.modified=true
                end
            end
        end
    else
        local vector=tTargetGizmo.ringDirection(origin,direction,drag.origin,drag.axis)
        if vector then
            local field=drag.name=='x' and 'targetRx' or drag.name=='y' and 'targetRy' or 'targetRz'
            E[field]=drag.baseAngle+tTargetGizmo.signedAngle(drag.axis,drag.startDirection,vector)
            E.targetEditDraftKey=nil
            E.modified=true
        end
    end
    return true
end

local function targetBrushRadius()
    return tTargetBrush.radius(E.meshBounds,E.targetBrushRadius)
end

local function ensureTargetBrushGeometry()
    local target=selectedTarget()
    if not target or not E.topology then return nil end
    local key=tostring(target.id)..':'..string.format('%.6f',E.targetPreviewWeight)
    local brush=E.targetBrush
    if brush.geometry and brush.geometryKey==key then
        local radius=targetBrushRadius()
        if not brush.geometry.spatial or math.abs((brush.geometry.spatialRadius or 0)-radius)>1e-8 then
            brush.geometry.spatial=tTargetBrush.buildSpatial(brush.geometry,radius)
            brush.geometry.spatialRadius=radius
        end
        return brush.geometry
    end
    local geometry=tTargetBrush.buildGeometry(E.topology,target,E.targetPreviewWeight)
    if not geometry then return nil end
    local radius=targetBrushRadius()
    geometry.spatial=tTargetBrush.buildSpatial(geometry,radius)
    geometry.spatialRadius=radius
    brush.geometry=geometry
    brush.geometryKey=key
    return geometry
end

local function targetBrushHit(x,y,geometry)
    local ok,ox,oy,oz,dx,dy,dz=dpCall(function() return mbm.getPickRay(x,y) end)
    if not ok or not ox or not dx then return nil end
    return tTargetBrush.hit(geometry,{x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz})
end

local function stampTargetBrush(x,y,stroke)
    local hit=targetBrushHit(x,y,stroke.geometry)
    if not hit then return false end
    local brush=E.targetBrush
    brush.hoverHit=hit
    brush.hoverX,brush.hoverY=x,y
    local radius=targetBrushRadius()
    local nearby=tTargetBrush.nearby(stroke.geometry,stroke.geometry.spatial,hit.point,radius,
        stroke.allowed)
    local changed=tTargets.brushStamp(stroke.edited,stroke.geometry.vertices,hit.point,hit.normal,
        radius,E.targetBrushStrength,{x=E.targetBrushDx,y=E.targetBrushDy,z=E.targetBrushDz},
        E.targetBrushMode==2,stroke.allowed,nearby,E.topology,E.targetBrushSmoothPasses,
        E.targetBrushSmoothProtectCreases,E.targetBrushSmoothAngle)
    stroke.changed=stroke.changed+changed
    stroke.lastX,stroke.lastY=x,y
    return true
end

local function beginTargetBrush(x,y)
    local target=selectedTarget()
    if not target or targetStatus(target)~='ready' or not E.topology then return false end
    local geometry=ensureTargetBrushGeometry()
    if not geometry then return false end
    local stroke={before=tTargets.copyTargetData(target),edited=tTargets.copyTargetData(target),
        changed=0,geometry=geometry,
        allowed=tTargetBrush.allowed(target),lastX=x,lastY=y}
    if not stroke.edited or not stampTargetBrush(x,y,stroke) then return false end
    E.targetBrush.stroke=stroke
    return true
end

local function finishTargetBrush()
    local stroke=E.targetBrush.stroke
    E.targetBrush.stroke=nil
    if not stroke then return false end
    if stroke.changed==0 then
        restoreTargetPreviewDeltas()
        setStatus(L('target_brush_no_change'),false)
        return false
    end
    local target=selectedTarget()
    if not target then
        restoreTargetPreviewDeltas()
        return false
    end
    target.deltas=stroke.edited.deltas
    tTargetPreview.setDeltas(E,target)
    E.targetBrush.geometry=nil
    E.targetBrush.geometryKey=nil
    markChanged()
    pushTargetBrushHistory(target,stroke.before,tTargets.copyTargetData(target))
    clearTargetEditHistory()
    setStatus(string.format(L('target_brush_applied'),stroke.changed),false)
    return true
end

local function stampTargetBrushAtRelease(x,y,stroke)
    if not stroke then return false end
    local lastX,lastY=stroke.lastX,stroke.lastY
    if lastX and lastY and math.abs(x-lastX)<=0.5 and math.abs(y-lastY)<=0.5 then
        return false
    end
    return stampTargetBrush(x,y,stroke)
end

local function updateTargetBrushHover()
    local brush=E.targetBrush
    if E.worktree~=2 or E.targetMode~=4 or brush.stroke or not targetPreviewReady(selectedTarget()) or
            tImGui.GetWantCaptureMouse() then
        if not brush.stroke then brush.hoverHit=nil end
        return
    end
    local mouse=tImGui.GetMousePos()
    if not mouse or not tonumber(mouse.x) or not tonumber(mouse.y) then return end
    local x,y=mouse.x,mouse.y
    if brush.hoverX and math.abs(x-brush.hoverX)<2 and brush.hoverY and
            math.abs(y-brush.hoverY)<2 and brush.hoverHit then return end
    local now=mbm.getTimeRun()
    if brush.hoverTime and now-brush.hoverTime<1/30 then return end
    brush.hoverTime=now
    local geometry=ensureTargetBrushGeometry()
    brush.hoverHit=targetBrushHit(x,y,geometry)
    brush.hoverX,brush.hoverY=x,y
end

local function drawTargetBrushCursor()
    local brush=E.targetBrush
    if E.worktree~=2 or E.targetMode~=4 or not brush.hoverHit or
            (not brush.stroke and tImGui.GetWantCaptureMouse()) then return end
    local mouse=tImGui.GetMousePos()
    if not mouse or not tonumber(mouse.x) or not tonumber(mouse.y) then return end
    local radius=math.max(6,math.min(96,(E.targetBrushRadius/0.025)*20))
    local color=E.targetBrushMode==2 and {r=0.25,g=0.75,b=1,a=0.9} or
        {r=1,g=0.75,b=0.2,a=0.9}
    tImGui.SetImDrawListToForeground(true)
    tImGui.AddCircle({x=mouse.x,y=mouse.y},radius,color,32,1.5)
    tImGui.SetImDrawListToForeground(false)
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
    E.camera.azimuth=E.camera.azimuth+(x-E.cameraDrag.x)*0.008
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

local function openExportDialog()
    E.exportNormalDraft=E.exportNormalMode
    E.exportDialogOpen=true
end

local function drawExportDialog()
    if E.exportDialogOpen then
        tImGui.OpenPopup('mreExportMshModal')
        E.exportDialogOpen=false
    end
    local flags=tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
    local opened=tImGui.BeginPopupModal(L('export_msh_dialog_title')..'###mreExportMshModal',false,flags)
    if not opened then return end
    tImGui.TextWrapped(L('export_msh_dialog_help'))
    tImGui.Separator()
    tImGui.SetNextItemWidth(260)
    local modeChanged,mode=tImGui.Combo(L('export_normal_mode')..'##mreExportNormalDraft',
        E.exportNormalDraft,{L('export_normal_preserve'),L('export_normal_recompute')})
    if modeChanged then E.exportNormalDraft=math.max(1,math.min(2,mode)) end
    showHelpTooltip(L('export_normal_help'))
    tImGui.Separator()
    if tImGui.Button(L('export_msh_start')..'##mreExportMshStart') then
        if E.exportNormalMode~=E.exportNormalDraft then
            E.exportNormalMode=E.exportNormalDraft
            E.modified=true
        end
        tImGui.CloseCurrentPopup()
        exportMorphTargets()
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('cancel')..'##mreExportMshCancel') then
        tImGui.CloseCurrentPopup()
    end
    tImGui.EndPopup()
end

local function showMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(L('new_project'),'Ctrl+N') then requestAction('new') end
        if tImGui.MenuItem(L('open_project'),'Ctrl+O') then openProjectDialog() end
        if tImGui.MenuItem(L('open_mesh')) then openMeshDialog() end
        tImGui.Separator()
        if tImGui.MenuItem(L('save'),'Ctrl+S',false,E.modified) then
            if E.projectPath then saveProject(E.projectPath) else saveProjectAs() end
        end
        if tImGui.MenuItem(L('save_as'),nil,false,true) then saveProjectAs() end
        local canExport=E.meshDebug~=nil and #(E.project.targets or {})>0
        if tImGui.MenuItem(L('export_msh'),nil,false,canExport) then openExportDialog() end
        tImGui.Separator()
        local canUndo=not E.targetBrush.stroke and #E.targetBrushHistory.undo>0
        local canRedo=not E.targetBrush.stroke and #E.targetBrushHistory.redo>0
        if tImGui.MenuItem(L('target_brush_undo'),'Ctrl+Z',false,canUndo) then
            applyTargetBrushHistory(false)
        end
        if tImGui.MenuItem(L('target_brush_redo'),'Ctrl+Y',false,canRedo) then
            applyTargetBrushHistory(true)
        end
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
        local displacementActive=E.targetPreview and E.targetPreview.surface and
            E.targetPreview.plainSurface~=nil
        local meshVisible=E.mesh and E.mesh.visible or false
        tImGui.BeginDisabled(displacementActive==true)
        local visible=tImGui.Checkbox(L('show_mesh')..'##mreShowMesh',meshVisible)
        tImGui.EndDisabled()
        if not displacementActive and E.mesh and visible~=meshVisible then
            E.mesh.visible=visible
            if E.targetPreview and E.targetPreview.surface then E.targetPreview.meshWasVisible=visible end
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
    local region=selectedRegion()
    suggestTargetName(region)
    return region
end

local function drawMorphTargets()
    local previousMode=E.targetMode
    E.targetMode=tImGui.RadioButton(L('target_mode_add')..'##mreTargetModeAdd',E.targetMode,1)
    E.targetMode=tImGui.RadioButton(L('target_mode_edit')..'##mreTargetModeEdit',E.targetMode,2)
    E.targetMode=tImGui.RadioButton(L('target_mode_paint')..'##mreTargetModePaint',E.targetMode,4)
    E.targetMode=tImGui.RadioButton(L('target_mode_mirror')..'##mreTargetModeMirror',E.targetMode,5)
    E.targetMode=tImGui.RadioButton(L('target_mode_remove')..'##mreTargetModeRemove',E.targetMode,3)
    local enteredMirror=E.targetMode==5 and previousMode~=5
    if E.targetMode~=previousMode then
        E.targetPreviewDirty=true
        if E.targetBrush.stroke then restoreTargetPreviewDeltas() end
        E.targetBrush.stroke=nil
        E.targetBrush.hoverHit=nil
        E.targetBrush.geometry=nil
        E.targetBrush.geometryKey=nil
        E.targetFeatherCache=nil
        E.targetEditDraft=nil
        E.targetEditDraftKey=nil
        E.targetMirrorPlan=nil
        E.targetMirrorCandidate=nil
        E.overlayDirty=true
        E.targetGizmo.dirty=true
        E.modified=true
    end
    if enteredMirror or (E.targetMode==5 and E.targetMirrorNeedsPrepare) then
        prepareTargetMirrorCandidate()
    end
    tImGui.TextWrapped(L('target_interaction_help'))
    tImGui.Separator()
    if not E.topology or E.project.topologySignature~=E.topology.signature then
        tImGui.TextWrapped(L('target_mesh_required'))
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
                if E.targetMode==2 or E.targetMode==4 then
                    local targetReady=targetStatus(target)=='ready'
                    tImGui.BeginDisabled(not targetReady)
                    tImGui.SetNextItemWidth(220)
                    local previewChanged,previewWeight=tImGui.SliderFloat(
                        L('target_preview_weight')..'##mreTargetPreviewWeight',
                        E.targetPreviewWeight,0,1,'%.2f')
                    tImGui.EndDisabled()
                    showHelpTooltip(L('target_preview_help'))
                    if previewChanged then
                        E.targetPreviewWeight=math.max(0,math.min(1,previewWeight))
                        tTargetPreview.setWeight(E,E.targetPreviewWeight)
                        E.targetBrush.geometry=nil
                        E.targetBrush.geometryKey=nil
                        E.modified=true
                    end
                    tImGui.BeginDisabled(not targetReady)
                    local displacementVisible=tImGui.Checkbox(
                        L('target_preview_displacement')..'##mreTargetPreviewDisplacement',
                        E.targetPreviewVisible)
                    tImGui.EndDisabled()
                    showHelpTooltip(L('target_preview_displacement_help'))
                    if E.targetEditDraft then
                        tImGui.TextWrapped(L('target_preview_edit_draft'))
                    end
                    if displacementVisible~=E.targetPreviewVisible then
                        E.targetPreviewVisible=displacementVisible
                        tTargetPreview.setVisible(E,E.targetPreviewVisible)
                        E.modified=true
                    end
                    tImGui.BeginDisabled(targetStatus(target)~='ready')
                    if tImGui.Button(L('target_rebuild')..'##mreTargetRebuild') then
                        rebuildSelectedTarget()
                    end
                    tImGui.EndDisabled()
                    tImGui.SetNextItemWidth(220)
                    local ringsChanged,rings=tImGui.SliderInt(
                        L('target_feather_rings')..'##mreTargetFeatherRings',E.targetFeatherRings,0,8)
                    if ringsChanged then
                        E.targetFeatherRings=math.max(0,math.min(8,rings))
                        E.targetFeatherCache=nil
                        E.modified=true
                    end
                    showHelpTooltip(L('target_feather_rings_help'))
                    tImGui.Separator()
                    if E.targetMode==2 then
                        tImGui.Text(L('target_edit_mode'))
                        local editMode=E.targetEditMode
                        editMode=tImGui.RadioButton(L('target_edit_translate')..'##mreTargetEditTranslate',editMode,1)
                        showHelpTooltip(L('target_edit_translate_help'))
                        editMode=tImGui.RadioButton(L('target_edit_rotate')..'##mreTargetEditRotate',editMode,2)
                        showHelpTooltip(L('target_edit_rotate_help'))
                        editMode=tImGui.RadioButton(L('target_edit_reset')..'##mreTargetEditReset',editMode,3)
                        showHelpTooltip(L('target_edit_reset_help'))
                        if editMode~=E.targetEditMode then
                            E.targetEditMode=editMode
                            E.targetEditDraft=nil
                            E.targetEditDraftKey=nil
                            E.targetGizmo.dirty=true
                            E.modified=true
                        end
                        if E.targetEditMode==1 then
                            tImGui.SetNextItemWidth(220)
                            local deltaChanged,delta=tImGui.DragFloat3(
                                L('target_translation')..'##mreTargetTranslation',
                                {E.targetDx,E.targetDy,E.targetDz},0.001,-math.huge,math.huge,'%.4f')
                            if deltaChanged then
                                E.targetDx, E.targetDy, E.targetDz=delta[1] or 0,delta[2] or 0,delta[3] or 0
                                E.modified=true
                            end
                            showHelpTooltip(L('target_translation_help'))
                        elseif E.targetEditMode==2 then
                            tImGui.SetNextItemWidth(220)
                            local rotationChanged,rotation=tImGui.DragFloat3(
                                L('target_rotation')..'##mreTargetRotation',
                                {E.targetRx,E.targetRy,E.targetRz},1,-math.huge,math.huge,'%.2f')
                            if rotationChanged then
                                E.targetRx,E.targetRy,E.targetRz=rotation[1] or 0,rotation[2] or 0,rotation[3] or 0
                                E.modified=true
                            end
                            showHelpTooltip(L('target_rotation_help'))
                            local pivot=tTargets.selectionPivot(target,E.topology.vertices,E.meshBounds)
                            tImGui.Text(string.format(L('target_pivot'),pivot.x,pivot.y,pivot.z))
                            if tImGui.Button(L('target_pivot_reset')..'##mreTargetPivotReset') then
                                target.pivot=nil
                                E.targetEditDraftKey=nil
                                E.targetGizmo.dirty=true
                                E.modified=true
                            end
                            showHelpTooltip(L('target_pivot_reset_help'))
                        else
                            tImGui.TextWrapped(L('target_edit_reset_help'))
                        end
                        if tImGui.Button(L('target_apply_edit')..'##mreTargetApplyEdit') then
                            applySelectedTargetEdit()
                        end
                        local editHistory=E.targetEditHistory
                        local canUndo=#editHistory.undo>0
                        local canRedo=#editHistory.redo>0
                        tImGui.SameLine()
                        tImGui.BeginDisabled(not canUndo)
                        if tImGui.Button(L('target_edit_undo')..'##mreTargetEditUndo',{x=120,y=0}) then
                            applyTargetEditHistory(false)
                        end
                        tImGui.EndDisabled()
                        showHelpTooltip(L('target_edit_undo_help'))
                        tImGui.SameLine()
                        tImGui.BeginDisabled(not canRedo)
                        if tImGui.Button(L('target_edit_redo')..'##mreTargetEditRedo',{x=120,y=0}) then
                            applyTargetEditHistory(true)
                        end
                        tImGui.EndDisabled()
                        showHelpTooltip(L('target_edit_redo_help'))
                    else
                        tImGui.Text(L('target_brush_title'))
                        local history=E.targetBrushHistory
                        local canUndo=not E.targetBrush.stroke and #history.undo>0
                        local canRedo=not E.targetBrush.stroke and #history.redo>0
                        tImGui.BeginDisabled(not canUndo)
                        if tImGui.Button(L('target_brush_undo')..'##mreTargetBrushUndo',{x=120,y=0}) then
                            applyTargetBrushHistory(false)
                        end
                        tImGui.EndDisabled()
                        showHelpTooltip(L('target_brush_undo_help'))
                        tImGui.SameLine()
                        tImGui.BeginDisabled(not canRedo)
                        if tImGui.Button(L('target_brush_redo')..'##mreTargetBrushRedo',{x=120,y=0}) then
                            applyTargetBrushHistory(true)
                        end
                        tImGui.EndDisabled()
                        showHelpTooltip(L('target_brush_redo_help'))
                        local brushMode=E.targetBrushMode
                        brushMode=tImGui.RadioButton(L('target_brush_move')..'##mreTargetBrushMove',brushMode,1)
                        brushMode=tImGui.RadioButton(L('target_brush_smooth')..'##mreTargetBrushSmooth',brushMode,2)
                        if brushMode~=E.targetBrushMode then
                            E.targetBrushMode=brushMode
                            E.modified=true
                        end
                        showHelpTooltip(L(E.targetBrushMode==1 and 'target_brush_move_help' or
                            'target_brush_smooth_help'))
                        tImGui.SetNextItemWidth(220)
                        local radiusChanged,radius=tImGui.SliderFloat(
                            L('target_brush_radius')..'##mreTargetBrushRadius',E.targetBrushRadius,0.001,0.15,'%.3f')
                        if radiusChanged then
                            E.targetBrushRadius=math.max(0.001,math.min(0.15,radius))
                            E.targetBrush.geometry=nil
                            E.targetBrush.geometryKey=nil
                            E.modified=true
                        end
                        showHelpTooltip(L('target_brush_radius_help'))
                        tImGui.SetNextItemWidth(220)
                        local strengthChanged,strength=tImGui.SliderFloat(
                            L('target_brush_strength')..'##mreTargetBrushStrength',E.targetBrushStrength,0.01,1,'%.2f')
                        if strengthChanged then E.targetBrushStrength=strength; E.modified=true end
                        showHelpTooltip(L('target_brush_strength_help'))
                        if E.targetBrushMode==1 then
                            tImGui.SetNextItemWidth(220)
                            local brushDeltaChanged,brushDelta=tImGui.DragFloat3(
                                L('target_brush_offset')..'##mreTargetBrushOffset',
                                {E.targetBrushDx,E.targetBrushDy,E.targetBrushDz},0.001,-math.huge,math.huge,'%.4f')
                            if brushDeltaChanged then
                                E.targetBrushDx=brushDelta[1] or 0
                                E.targetBrushDy=brushDelta[2] or 0
                                E.targetBrushDz=brushDelta[3] or 0
                                E.modified=true
                            end
                            showHelpTooltip(L('target_brush_offset_help'))
                        else
                            tImGui.SetNextItemWidth(220)
                            local passesChanged,passes=tImGui.SliderInt(
                                L('target_brush_smooth_passes')..'##mreTargetBrushSmoothPasses',
                                E.targetBrushSmoothPasses,1,8)
                            if passesChanged then
                                E.targetBrushSmoothPasses=math.max(1,math.min(8,passes))
                                E.modified=true
                            end
                            showHelpTooltip(L('target_brush_smooth_passes_help'))
                            local protectCreases=tImGui.Checkbox(
                                L('target_brush_smooth_protect_creases')..'##mreTargetBrushSmoothProtect',
                                E.targetBrushSmoothProtectCreases)
                            if protectCreases~=E.targetBrushSmoothProtectCreases then
                                E.targetBrushSmoothProtectCreases=protectCreases
                                E.modified=true
                            end
                            showHelpTooltip(L('target_brush_smooth_protect_creases_help'))
                            tImGui.BeginDisabled(not E.targetBrushSmoothProtectCreases)
                            tImGui.SetNextItemWidth(220)
                            local angleChanged,angle=tImGui.SliderFloat(
                                L('target_brush_smooth_angle')..'##mreTargetBrushSmoothAngle',
                                E.targetBrushSmoothAngle,5,180,'%.0f')
                            if angleChanged then
                                E.targetBrushSmoothAngle=math.max(5,math.min(180,angle))
                                E.modified=true
                            end
                            tImGui.EndDisabled()
                            showHelpTooltip(L('target_brush_smooth_angle_help'))
                        end
                        tImGui.TextWrapped(L('target_brush_interaction_help'))
                    end
                elseif E.targetMode==5 then
                    drawTargetMirrorControls(target)
                else
                    if tImGui.Button(L('target_delete')..'##mreTargetDelete') then
                        deleteSelectedTarget()
                    end
                end
            end
        end
    end
    drawTargetTable()
end

local function expressionReport(expression)
    if not expression then return nil end
    return tExpressions.inspect(expression,E.project.targets,targetStatus)
end

local function expressionStatusText(expression)
    local report=expressionReport(expression)
    if not report or report.active==0 then return L('expression_status_empty') end
    if report.missing>0 then return string.format(L('expression_status_missing'),report.missing) end
    if report.stale>0 then return string.format(L('expression_status_stale'),report.stale) end
    return L('expression_status_ready')
end

local function drawExpressionWeights(expression,editable)
    if not expression then
        tImGui.TextDisabled(L('no_expressions'))
        return
    end
    local targets=E.project.targets or {}
    if #targets==0 then
        tImGui.TextDisabled(L('no_targets'))
        return
    end
    tImGui.Separator()
    local fixedColumn=tImGui.Flags('ImGuiTableColumnFlags_WidthFixed')
    if tImGui.BeginTable('mreExpressionWeights',4,
            tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(L('expression_table_target'),fixedColumn,170)
        tImGui.TableSetupColumn(L('expression_table_weight'),fixedColumn,250)
        tImGui.TableSetupColumn(L('expression_table_vertices'),fixedColumn,75)
        tImGui.TableSetupColumn(L('expression_table_status'),fixedColumn,170)
        tImGui.TableHeadersRow()
        for index,target in ipairs(targets) do
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0); tImGui.Text(target.name or '')
            tImGui.TableSetColumnIndex(1)
            local weight=tExpressions.getWeight(expression,target.id)
            if editable then
                tImGui.SetNextItemWidth(235)
                local changed,value=tImGui.SliderFloat(
                    L('expression_weight')..'##mreExpressionWeight'..index,weight,0,1,'%.2f')
                if changed then setExpressionWeight(expression,target,value) end
            else
                tImGui.Text(string.format('%.2f',weight))
            end
            tImGui.TableSetColumnIndex(2); tImGui.Text(tostring(#(target.vertices or {})))
            tImGui.TableSetColumnIndex(3)
            tImGui.Text(targetStatusLabel(targetStatus(target)))
        end
        tImGui.EndTable()
    end
end

local function drawExpressionList()
    if #(E.project.expressions or {})==0 then
        tImGui.TextDisabled(L('no_expressions'))
        return
    end
    tImGui.Separator()
    if tImGui.BeginTable('mreExpressionTable',4,
            tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(L('expression_table_name'))
        tImGui.TableSetupColumn(L('expression_table_targets'))
        tImGui.TableSetupColumn(L('expression_table_status'))
        tImGui.TableSetupColumn(L('expression_table_select'))
        tImGui.TableHeadersRow()
        for index,expression in ipairs(E.project.expressions) do
            local report=expressionReport(expression)
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0); tImGui.Text(expression.name or '')
            tImGui.TableSetColumnIndex(1); tImGui.Text(tostring(report and report.active or 0))
            tImGui.TableSetColumnIndex(2); tImGui.Text(expressionStatusText(expression))
            tImGui.TableSetColumnIndex(3)
            if tImGui.Selectable(L('expression_select')..'##mreExpressionRow'..index,
                    index==E.project.selectedExpression) then
                selectExpression(index)
            end
        end
        tImGui.EndTable()
    end
end

local function drawExpressionNode()
    local previousMode=E.expressionMode
    E.expressionMode=tImGui.RadioButton(L('expression_mode_add')..'##mreExpressionModeAdd',E.expressionMode,1)
    E.expressionMode=tImGui.RadioButton(L('expression_mode_edit')..'##mreExpressionModeEdit',E.expressionMode,2)
    E.expressionMode=tImGui.RadioButton(L('expression_mode_remove')..'##mreExpressionModeRemove',E.expressionMode,3)
    if E.expressionMode~=previousMode then
        E.expressionDraft=nil
        if E.expressionMode==2 or E.expressionMode==3 then
            local expression=selectedExpression()
            E.expressionNameDraft=expression and expression.name or 'expression'
        end
        invalidateExpressionPreview()
        E.modified=true
    end
    tImGui.TextWrapped(L('expression_interaction_help'))
    tImGui.Separator()
    local expression
    if E.expressionMode==1 then
        expression=ensureExpressionDraft()
        tImGui.SetNextItemWidth(220)
        local changed,name=tImGui.InputText(L('expression_name')..'##mreExpressionNameAdd',E.expressionNameDraft)
        if changed then
            E.expressionNameDraft=name
            invalidateExpressionPreview()
        end
        drawExpressionWeights(expression,true)
        tImGui.BeginDisabled(not expression or #(E.project.targets or {})==0)
        if tImGui.Button(L('expression_create')..'##mreExpressionCreate') then createExpression() end
        tImGui.EndDisabled()
        showHelpTooltip(L('expression_create_help'))
        local _,report=expressionPreviewData()
        if report and (report.missing>0 or report.stale>0) then
            tImGui.TextWrapped(L('expression_invalid_targets'))
        end
    elseif E.expressionMode==2 then
        local expressions=E.project.expressions or {}
        if #expressions==0 then
            tImGui.TextDisabled(L('no_expressions'))
        else
            local names={}
            for _,entry in ipairs(expressions) do names[#names+1]=entry.name end
            tImGui.SetNextItemWidth(220)
            local changed,index=tImGui.Combo(L('expression_select')..'##mreExpressionSelect',
                math.max(1,math.min(#names,E.project.selectedExpression)),names)
            if changed then selectExpression(index) end
            expression=selectedExpression()
            if expression then
                tImGui.SetNextItemWidth(220)
                local nameChanged,name=tImGui.InputText(L('expression_name')..'##mreExpressionNameEdit',
                    expression.name or '')
                if nameChanged then
                    local trimmed=(name or ''):match('^%s*(.-)%s*$')
                    local taken=trimmed==''
                    for index,other in ipairs(expressions) do
                        if index~=E.project.selectedExpression and other.name==trimmed then taken=true; break end
                    end
                    if not taken then
                        expression.name=trimmed
                        E.expressionNameDraft=trimmed
                        markChanged()
                        invalidateExpressionPreview()
                    else
                        setStatus(L('expression_name_invalid'),true)
                    end
                end
                drawExpressionWeights(expression,true)
                local _,report=expressionPreviewData()
                if report and (report.missing>0 or report.stale>0) then
                    tImGui.TextWrapped(L('expression_invalid_targets'))
                end
            end
        end
    else
        local expressions=E.project.expressions or {}
        if #expressions==0 then
            tImGui.TextDisabled(L('no_expressions'))
        else
            local names={}
            for _,entry in ipairs(expressions) do names[#names+1]=entry.name end
            tImGui.SetNextItemWidth(220)
            local changed,index=tImGui.Combo(L('expression_select')..'##mreExpressionSelectRemove',
                math.max(1,math.min(#names,E.project.selectedExpression)),names)
            if changed then selectExpression(index) end
            expression=selectedExpression()
            if expression then
                tImGui.Text(string.format(L('expression_selected'),expression.name or ''))
                tImGui.BeginDisabled(false)
                if tImGui.Button(L('expression_delete')..'##mreExpressionDelete') then
                    deleteSelectedExpression()
                end
                tImGui.EndDisabled()
                showHelpTooltip(L('expression_delete_help'))
            end
        end
    end
    tImGui.SetNextItemWidth(220)
    local previewChanged,previewWeight=tImGui.SliderFloat(
        L('expression_preview_weight')..'##mreExpressionPreviewWeight',E.targetPreviewWeight,0,1,'%.2f')
    if previewChanged then
        E.targetPreviewWeight=math.max(0,math.min(1,previewWeight))
        tTargetPreview.setWeight(E,E.targetPreviewWeight)
        E.modified=true
    end
    showHelpTooltip(L('expression_preview_weight_help'))
    local _,previewReport=expressionPreviewData()
    if previewReport then
        tImGui.Text(string.format(L('expression_preview_summary'),previewReport.active,
            previewReport.missing,previewReport.stale))
    end
    tImGui.BeginDisabled(not previewReport or previewReport.active==0)
    local displacementVisible=tImGui.Checkbox(
        L('target_preview_displacement')..'##mreExpressionPreviewDisplacement',E.targetPreviewVisible)
    tImGui.EndDisabled()
    showHelpTooltip(L('target_preview_displacement_help'))
    if previewReport and previewReport.active>0 and displacementVisible~=E.targetPreviewVisible then
        E.targetPreviewVisible=displacementVisible
        tTargetPreview.setVisible(E,E.targetPreviewVisible)
        E.modified=true
    end
    drawExpressionList()
end

local function clipReport(clip)
    if not clip then return nil end
    return tClips.inspect(clip,E.project.targets,targetStatus)
end

local function drawClipKeys(clip)
    local keys=clip and clip.keys or {}
    if #keys==0 then
        tImGui.TextDisabled(L('clip_no_keys'))
        return
    end
    tImGui.Separator()
    if tImGui.BeginTable('mreClipKeys',4,
            tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(L('clip_key_column'))
        tImGui.TableSetupColumn(L('clip_time_column'))
        tImGui.TableSetupColumn(L('clip_weight_column'))
        tImGui.TableSetupColumn(L('clip_key_select_column'))
        tImGui.TableHeadersRow()
        for index,key in ipairs(keys) do
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0); tImGui.Text(L('clip_key')..' '..tostring(index))
            tImGui.TableSetColumnIndex(1); tImGui.Text(string.format('%.3f',key.time or 0))
            local count=0
            for _,entry in ipairs(key.weights or {}) do
                if (entry.weight or 0)>1e-8 then count=count+1 end
            end
            tImGui.TableSetColumnIndex(2); tImGui.Text(tostring(count))
            tImGui.TableSetColumnIndex(3)
            if tImGui.Selectable(L('clip_select_key')..'##mreClipKey'..tostring(index),
                    index==E.clipKeySelected) then
                E.clipKeySelected=index
                E.clipTime=math.max(0,math.min(clip.duration,key.time or 0))
                E.clipPlaying=false
                invalidateClipPreview()
            end
        end
        tImGui.EndTable()
    end
end

local function drawClipTimelineWindow(clip)
    if E.worktree~=4 or not E.clipTimelineVisible or not clip then return end
    local screenWidth,screenHeight=mbm.getRealSizeScreen()
    local windowHeight=280
    local left=math.max(0,math.min(390,screenWidth-420))
    tImGui.SetNextWindowPos({x=left,y=math.max(0,screenHeight-windowHeight)},tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x=math.max(420,screenWidth-left),y=windowHeight},tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSizeConstraints({x=420,y=210},
        {x=math.max(420,screenWidth),y=math.max(210,screenHeight)})
    local opened,closed=tImGui.Begin(L('clip_timeline_window')..'###MorphClipTimeline',true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if closed then
        E.clipTimelineVisible=false
        E.modified=true
    end
    if not opened then
        tImGui.End()
        return
    end
    tImGui.Text(string.format(L('clip_timeline_selection'),clip.name or ''))
    tImGui.SameLine()
    tImGui.Text(string.format(L('clip_timeline_key_selection'),E.clipKeySelected or 1))
    tImGui.TextWrapped(L('clip_timeline_help'))
    local names={}
    for _,expression in ipairs(E.project.expressions or {}) do names[#names+1]=expression.name end
    if #names>0 then
        E.clipSelectedExpression=math.max(1,math.min(#names,E.clipSelectedExpression))
        tImGui.SetNextItemWidth(280)
        local changed,index=tImGui.Combo(L('clip_expression_source')..'##mreClipExpressionSource',
            E.clipSelectedExpression,names)
        if changed then E.clipSelectedExpression=index;E.modified=true end
        showHelpTooltip(L('clip_expression_source_help'))
    else
        tImGui.TextDisabled(L('clip_requires_expression'))
    end
    local duration=math.max(tonumber(clip.duration) or 1,0.001)
    tImGui.SetNextItemWidth(-1)
    local timeChanged,time=tImGui.SliderFloat(L('clip_time')..'##mreClipTime',E.clipTime,0,duration,'%.3f s')
    showHelpTooltip(L('clip_time_help'))
    if timeChanged then
        E.clipTime=time
        E.clipPlaying=false
        invalidateClipPreview()
    end
    tImGui.BeginDisabled(#names==0)
    if tImGui.Button(L('clip_insert_expression_key')..'##mreClipInsertExpression',{x=230,y=0}) then
            local expression=E.project.expressions[E.clipSelectedExpression]
            if expression then
                local report=tExpressions.inspect(expression,E.project.targets,targetStatus)
                if report.missing>0 or report.stale>0 then
                    setStatus(L('clip_invalid_expression'),true)
                else
                    insertClipKey(tExpressions.copyWeights(expression))
                    setStatus(L('clip_key_inserted'),false)
                end
            end
    end
    tImGui.EndDisabled()
    showHelpTooltip(L('clip_insert_expression_key_help'))
    tImGui.SameLine()
    if tImGui.Button(L('clip_capture_key')..'##mreClipCapture',{x=190,y=0}) then
        local weights=tClips.sample(clip,E.clipTime)
        local previewWeight=math.max(0,math.min(1,E.clipPreviewWeight))
        if previewWeight<0.999999 then
            local scaled={}
            for _,entry in ipairs(weights) do
                scaled[#scaled+1]={targetId=entry.targetId,targetName=entry.targetName,
                    weight=entry.weight*previewWeight}
            end
            weights=scaled
        end
        insertClipKey(weights)
        setStatus(L('clip_key_captured'),false)
    end
    showHelpTooltip(L('clip_capture_key_help'))
    tImGui.SameLine()
    if tImGui.Button(L('clip_reset_weights')..'##mreClipResetWeights',{x=190,y=0}) then
        E.clipPreviewWeight=0
        tTargetPreview.setWeight(E,0)
        E.modified=true
        setStatus(L('clip_weights_reset'),false)
    end
    showHelpTooltip(L('clip_reset_weights_help'))
    tImGui.SameLine()
    if tImGui.Button(L('clip_play')..'##mreClipPlay',{x=110,y=0}) then
        E.clipPlaying=not E.clipPlaying
    end
    showHelpTooltip(L('clip_play_help'))
    tImGui.SameLine()
    if tImGui.Button(L('clip_stop')..'##mreClipStop',{x=90,y=0}) then
        E.clipPlaying=false
        E.clipTime=0
        E.clipKeySelected=1
        invalidateClipPreview()
    end
    showHelpTooltip(L('clip_stop_help'))
    drawClipKeys(clip)
    local keys=clip.keys or {}
    tImGui.BeginDisabled(#keys==0)
    if tImGui.Button(L('clip_remove_key')..'##mreClipRemoveKey',{x=170,y=0}) then
        if tClips.removeKey(clip,E.clipKeySelected) then
            E.clipKeySelected=math.max(1,math.min(E.clipKeySelected,#keys))
            E.clipTime=math.min(E.clipTime,clip.duration)
            invalidateClipPreview()
            markChanged()
            setStatus(L('clip_key_removed'),false)
        end
    end
    tImGui.EndDisabled()
    showHelpTooltip(L('clip_remove_key_help'))
    local report=clipReport(clip)
    if report then
        tImGui.Text(string.format(L('clip_preview_summary'),report.active,report.missing,report.stale))
        local validation=tClips.validate(clip)
        if not validation.ok then
            tImGui.TextWrapped(L('clip_invalid'))
            for _,message in ipairs(validation.errors) do tImGui.BulletText(message) end
        end
    end
    tImGui.End()
end

local function drawClipsNode()
    local previousMode=E.clipMode
    E.clipMode=tImGui.RadioButton(L('clip_mode_add')..'##mreClipModeAdd',E.clipMode,1)
    E.clipMode=tImGui.RadioButton(L('clip_mode_edit')..'##mreClipModeEdit',E.clipMode,2)
    E.clipMode=tImGui.RadioButton(L('clip_mode_remove')..'##mreClipModeRemove',E.clipMode,3)
    if E.clipMode~=previousMode then
        E.clipPlaying=false
        if E.clipMode==2 or E.clipMode==3 then
            local clip=selectedClip()
            E.clipNameDraft=clip and clip.name or 'clip'
        end
        E.modified=true
    end
    tImGui.TextWrapped(L('clip_interaction_help'))
    tImGui.Separator()
    local clips=E.project.clips or {}
    if E.clipMode==1 then
        tImGui.SetNextItemWidth(220)
        local changed,name=tImGui.InputText(L('clip_name')..'##mreClipNameAdd',E.clipNameDraft)
        if changed then E.clipNameDraft=name;E.modified=true end
        if tImGui.Button(L('clip_create')..'##mreClipCreate') then createClip() end
        showHelpTooltip(L('clip_create_help'))
    elseif #clips==0 then
        tImGui.TextDisabled(L('clip_no_clips'))
    else
        local names={}
        for _,clip in ipairs(clips) do names[#names+1]=clip.name end
        E.project.selectedClip=math.max(1,math.min(#names,E.project.selectedClip))
        tImGui.SetNextItemWidth(220)
        local changed,index=tImGui.Combo(L('clip_select')..'##mreClipSelect',E.project.selectedClip,names)
        if changed then selectClip(index) end
        local clip=selectedClip()
        if clip then
            if E.clipMode==3 then
                tImGui.Text(string.format(L('clip_selected'),clip.name or ''))
                if tImGui.Button(L('clip_delete')..'##mreClipDelete') then deleteSelectedClip() end
                showHelpTooltip(L('clip_delete_help'))
            else
                tImGui.SetNextItemWidth(220)
                local nameChanged,name=tImGui.InputText(L('clip_name')..'##mreClipNameEdit',clip.name or '')
                if nameChanged then
                    local trimmed=(name or ''):match('^%s*(.-)%s*$')
                    local taken=trimmed==''
                    for index,other in ipairs(clips) do
                        if index~=E.project.selectedClip and other.name==trimmed then taken=true;break end
                    end
                    if not taken then clip.name=trimmed;E.clipNameDraft=trimmed;markChanged()
                    else setStatus(L('clip_name_invalid'),true) end
                end
                tImGui.SetNextItemWidth(160)
                local durationChanged,duration=tImGui.DragFloat(L('clip_duration')..'##mreClipDuration',
                    clip.duration,0.01,0.001,0,'%.3f')
                if durationChanged then
                    tClips.setDuration(clip,duration)
                    E.clipTime=math.min(E.clipTime,clip.duration)
                    markChanged();invalidateClipPreview()
                end
                tImGui.SetNextItemWidth(160)
                local fpsChanged,fps=tImGui.DragFloat(L('clip_fps')..'##mreClipFps',clip.fps,0.1,1,240,'%.1f')
                if fpsChanged then clip.fps=math.max(fps,1);markChanged() end
                local loop=tImGui.Checkbox(L('clip_loop')..'##mreClipLoop',clip.loop==true)
                if loop~=clip.loop then clip.loop=loop;markChanged() end
                local interpolationLabels={L('clip_interpolation_step'),L('clip_interpolation_linear')}
                tImGui.SetNextItemWidth(180)
                local interpolationChanged,interpolation=tImGui.Combo(L('clip_interpolation')..'##mreClipInterpolation',
                    clip.interpolation==0 and 1 or 2,interpolationLabels)
                if interpolationChanged then clip.interpolation=interpolation==1 and 0 or 1;markChanged();invalidateClipPreview() end
                local timelineText=E.clipTimelineVisible and L('clip_timeline_hide') or L('clip_timeline_show')
                if tImGui.Button(timelineText..'##mreClipTimelineToggle') then
                    E.clipTimelineVisible=not E.clipTimelineVisible
                    E.modified=true
                end
                showHelpTooltip(L('clip_timeline_toggle_help'))
            end
        end
    end
    local clip=selectedClip()
    if clip and E.clipMode~=3 then
        local report=clipReport(clip)
        tImGui.SetNextItemWidth(220)
        local previewChanged,previewWeight=tImGui.SliderFloat(L('clip_preview_weight')..'##mreClipPreviewWeight',
            E.clipPreviewWeight,0,1,'%.2f')
        if previewChanged then
            E.clipPreviewWeight=math.max(0,math.min(1,previewWeight))
            tTargetPreview.setWeight(E,E.clipPreviewWeight)
            E.modified=true
        end
        showHelpTooltip(L('clip_preview_weight_help'))
        if report then
            tImGui.Text(string.format(L('clip_preview_summary'),report.active,report.missing,report.stale))
        end
        local visible=tImGui.Checkbox(L('target_preview_displacement')..'##mreClipPreviewDisplacement',E.targetPreviewVisible)
        showHelpTooltip(L('target_preview_displacement_help'))
        if visible~=E.targetPreviewVisible then
            E.targetPreviewVisible=visible
            tTargetPreview.setVisible(E,E.targetPreviewVisible)
            E.modified=true
        end
    end
end

local function drawPreviewNode()
    local previousMode=E.previewMode
    E.previewMode=tImGui.RadioButton(L('preview_mode_expression')..'##mrePreviewModeExpression',E.previewMode,1)
    local clips=E.project.clips or {}
    tImGui.BeginDisabled(#clips==0)
    E.previewMode=tImGui.RadioButton(L('preview_mode_clip')..'##mrePreviewModeClip',E.previewMode,2)
    tImGui.EndDisabled()
    if E.previewMode~=previousMode then
        E.previewClipPlaying=false
        invalidateMorphPreview()
        E.modified=true
    end
    tImGui.TextWrapped(L('preview_interaction_help'))
    tImGui.Separator()
    if E.previewMode==1 then
        local names={}
        for _,expression in ipairs(E.project.expressions or {}) do names[#names+1]=expression.name end
        if #names==0 then
            tImGui.TextDisabled(L('preview_no_expressions'))
        else
            E.previewExpressionSelected=math.max(1,math.min(#names,E.previewExpressionSelected))
            tImGui.SetNextItemWidth(260)
            local changed,index=tImGui.Combo(L('preview_expression')..'##mrePreviewExpression',
                E.previewExpressionSelected,names)
            if changed then
                E.previewExpressionSelected=index
                invalidateMorphPreview()
                E.modified=true
            end
            showHelpTooltip(L('preview_expression_help'))
        end
    else
        local names={}
        for _,clip in ipairs(clips) do names[#names+1]=clip.name end
        if #names==0 then
            tImGui.TextDisabled(L('preview_no_clips'))
        else
            E.previewClipSelected=math.max(1,math.min(#names,E.previewClipSelected))
            tImGui.SetNextItemWidth(260)
            local changed,index=tImGui.Combo(L('preview_clip')..'##mrePreviewClip',E.previewClipSelected,names)
            if changed then
                E.previewClipSelected=index
                E.previewClipTime=0
                E.previewClipPlaying=false
                invalidateMorphPreview()
                E.modified=true
            end
            showHelpTooltip(L('preview_clip_help'))
            local clip=clips[E.previewClipSelected]
            if clip then
                local duration=math.max(tonumber(clip.duration) or 1,0.001)
                tImGui.SetNextItemWidth(260)
                local timeChanged,time=tImGui.SliderFloat(L('preview_time')..'##mrePreviewTime',
                    E.previewClipTime,0,duration,'%.3f s')
                if timeChanged then
                    E.previewClipTime=time
                    E.previewClipPlaying=false
                    invalidateMorphPreview()
                end
                tImGui.NewLine()
                if tImGui.Button(L('preview_play')..'##mrePreviewPlay',{x=110,y=0}) then
                    E.previewClipPlaying=not E.previewClipPlaying
                end
                tImGui.SameLine()
                if tImGui.Button(L('preview_stop')..'##mrePreviewStop',{x=90,y=0}) then
                    E.previewClipPlaying=false
                    E.previewClipTime=0
                    invalidateMorphPreview()
                end
            end
        end
    end
    local previewTarget,report=morphPreviewData()
    tImGui.SetNextItemWidth(260)
    local weightChanged,weight=tImGui.SliderFloat(L('preview_weight')..'##mrePreviewWeight',
        E.targetPreviewWeight,0,1,'%.2f')
    if weightChanged then
        E.targetPreviewWeight=math.max(0,math.min(1,weight))
        tTargetPreview.setWeight(E,E.targetPreviewWeight)
        E.targetPreviewDirty=true
        E.modified=true
    end
    showHelpTooltip(L('preview_weight_help'))
    local visible=tImGui.Checkbox(L('preview_displacement')..'##mrePreviewDisplacement',E.targetPreviewVisible)
    showHelpTooltip(L('preview_displacement_help'))
    if visible~=E.targetPreviewVisible then
        E.targetPreviewVisible=visible
        local nativePreviewActive=E.previewRuntime and
            (E.previewRuntime.comparisonMode~=1 or E.previewRuntime.skeletalPlaying)
        if nativePreviewActive and E.previewRuntime.previewReady then
            -- Native runtime previews are textured directly; the displacement checkbox
            -- controls only the static overlay visualization.
            E.previewRuntime.previewVisible=true
        else
            tTargetPreview.setVisible(E,E.targetPreviewVisible)
        end
        E.modified=true
    end
    if report then
        tImGui.Text(string.format(L('preview_summary'),report.active,report.missing,report.stale))
        if report.missing>0 or report.stale>0 then
            tImGui.TextWrapped(L('preview_invalid'))
        elseif report.active==0 then
            tImGui.TextDisabled(L('preview_empty'))
        elseif previewTarget then
            tImGui.Text(L('preview_ready'))
        end
    end
    local function rebuildPreviewRuntime(mode)
        if mode~=1 and E.targetPreview.surface then tTargetPreview.destroy(E) end
        if mode~=1 then E.targetPreviewDirty=true end
        local ok,bounds,reason=tPreviewRuntime.rebuild(E.previewRuntime,E.mesh,E.meshDebug,E.topology,
            E.project.targets,E.project.meshPath,E.meshBounds,mode,E.dqsSupported,dpCall)
        if ok then
            fitCamera(bounds or E.meshBounds)
            E.targetPreviewDirty=true
            E.modified=true
        end
        return ok,bounds,reason
    end
    local function prepareSkeletalPlayback()
        if E.targetPreview.surface then tTargetPreview.destroy(E) end
        local _,_,previewSource=morphPreviewData()
        local runtimeOk=tPreviewRuntime.ensure(E.previewRuntime,E.mesh,E.meshDebug,E.topology,
            E.project.targets,E.project.meshPath,dpCall)
        if runtimeOk and E.previewRuntime.previewReady then
            tPreviewRuntime.applyMorphWeights(E.previewRuntime,E.mesh,previewSource,E.project.targets,
                E.targetPreviewWeight,dpCall)
            E.previewRuntime.previewVisible=true
            E.targetPreviewDirty=false
        elseif E.mesh then
            E.mesh.visible=true
            E.targetPreviewDirty=false
        end
    end
    local function finishSkeletalPlayback()
        if not E.previewRuntime.previewReady then E.targetPreviewDirty=true end
    end
    tPreviewRuntime.draw(E.previewRuntime,E.mesh,true,E.dqsSupported,dpCall,rebuildPreviewRuntime,setStatus,
        prepareSkeletalPlayback,finishSkeletalPlayback,function() E.modified=true end)
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
        if E.shapeMode==4 then
            E.shapeMirrorCandidate=nil
            E.shapeMirrorNeedsPrepare=true
        end
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

local function drawShapeMirrorControls(region)
    tImGui.Separator()
    tImGui.Text(L('shape_mirror_title'))
    tImGui.TextWrapped(L('shape_mirror_help'))
    tImGui.SetNextItemWidth(220)
    local nameChanged,name=tImGui.InputText(L('shape_mirror_name')..'##mreShapeMirrorName',
        E.shapeMirrorNameDraft)
    if nameChanged then
        E.shapeMirrorNameDraft=name
        E.modified=true
    end
    local axes={L('shape_mirror_axis_x'),L('shape_mirror_axis_y'),L('shape_mirror_axis_z')}
    tImGui.SetNextItemWidth(220)
    local axisChanged,axis=tImGui.Combo(L('shape_mirror_axis')..'##mreShapeMirrorAxis',
        E.shapeMirrorAxis,axes)
    showHelpTooltip(L('shape_mirror_axis_help'))
    if axisChanged then
        E.shapeMirrorAxis=math.max(1,math.min(3,axis))
        E.shapeMirrorCandidate=nil
        E.shapeMirrorNeedsPrepare=true
        E.overlayDirty=true
        E.modified=true
    end
    local candidate=E.shapeMirrorCandidate
    local nameValue=(E.shapeMirrorNameDraft or ''):match('^%s*(.-)%s*$')
    local nameTaken=nameValue==''
    for _,entry in ipairs(E.project.regions or {}) do
        if entry.name==nameValue then nameTaken=true; break end
    end
    local ready=candidate and candidate.sourceId==region.id and
        candidate.axis==E.shapeMirrorAxis and not nameTaken
    tImGui.BeginDisabled(not ready)
    if tImGui.Button(L('shape_mirror_create')..'##mreShapeMirrorCreate') then
        createMirroredShape(region)
    end
    tImGui.EndDisabled()
    showHelpTooltip(L('shape_mirror_create_help'))
    if nameValue=='' then
        tImGui.TextWrapped(L('shape_mirror_name_required'))
    elseif nameTaken then
        tImGui.TextWrapped(L('shape_mirror_name_taken'))
    elseif not candidate then
        tImGui.TextWrapped(L('shape_mirror_preview_pending'))
    end
end

local function hasReadyRegion()
    if not E.targetAvailabilityDirty then return E.targetAvailability end
    E.targetAvailability=false
    if not E.topology or E.project.topologySignature~=E.topology.signature then
        E.targetAvailabilityDirty=false
        return false
    end
    for _,region in ipairs(E.project.regions or {}) do
        if tRegions.status(region,E.topology)=='ready' then
            E.targetAvailability=true
            break
        end
    end
    E.targetAvailabilityDirty=false
    return E.targetAvailability
end

local function hasReadyTarget()
    if not E.topology or E.project.topologySignature~=E.topology.signature then return false end
    for _,target in ipairs(E.project.targets or {}) do
        if targetStatus(target)=='ready' then return true end
    end
    return false
end

local function selectWorktree(worktree)
    if worktree==2 and not hasReadyRegion() then
        setStatus(L('target_worktree_sequence_help'),true)
        return
    end
    if worktree==3 and not hasReadyTarget() then
        setStatus(L('expression_worktree_sequence_help'),true)
        return
    end
    if worktree==4 and not hasReadyExpression() then
        setStatus(L('clip_worktree_sequence_help'),true)
        return
    end
    if worktree==5 and not hasReadyExpression() then
        setStatus(L('preview_worktree_sequence_help'),true)
        return
    end
    if E.worktree==worktree then return end
    if E.previewRuntime and E.previewRuntime.skeletalPlaying then
        tPreviewRuntime.stop(E.previewRuntime,E.mesh,dpCall)
    end
    E.worktree=worktree
    E.selectedPoint=nil
    E.regionDrag=nil
    E.pendingSurfacePick=nil
    clearStatus()
    if E.targetBrush.stroke then restoreTargetPreviewDeltas() end
    E.targetBrush.stroke=nil
    E.targetBrush.hoverHit=nil
    E.targetBrush.hoverX=nil
    E.targetBrush.hoverY=nil
    E.targetBrush.hoverTime=nil
    E.targetPreviewDirty=true
    invalidateExpressionPreview()
    invalidateClipPreview()
    invalidateMorphPreview()
    E.clipPlaying=false
    E.previewClipPlaying=false
    if worktree~=1 then
        E.shapeMirrorCandidate=nil
        E.shapeMirrorNeedsPrepare=false
    elseif E.shapeMode==4 then
        E.shapeMirrorCandidate=nil
        E.shapeMirrorNeedsPrepare=true
    end
    E.modified=true
    E.overlayDirty=true
end

local function drawSurfaceShapes()
    local previousShapeMode=E.shapeMode
    E.shapeMode=tImGui.RadioButton(L('mode_add')..'##mreShapeModeAdd',E.shapeMode,1)
    E.shapeMode=tImGui.RadioButton(L('mode_edit')..'##mreShapeModeEdit',E.shapeMode,2)
    E.shapeMode=tImGui.RadioButton(L('mode_mirror')..'##mreShapeModeMirror',E.shapeMode,4)
    E.shapeMode=tImGui.RadioButton(L('mode_remove')..'##mreShapeModeRemove',E.shapeMode,3)
    local enteredMirror=E.shapeMode==4 and previousShapeMode~=4
    if E.shapeMode~=previousShapeMode then
        E.modified=true
        E.overlayDirty=true
        if E.shapeMode==4 then
            E.shapeMirrorNeedsPrepare=true
        else
            E.shapeMirrorCandidate=nil
            E.shapeMirrorNeedsPrepare=false
        end
    end
    if enteredMirror or (E.shapeMode==4 and E.shapeMirrorNeedsPrepare) then
        prepareShapeMirrorCandidate()
    end
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
        elseif E.shapeMode==4 then
            local region=drawSelectedRegionSelector()
            if region then drawShapeMirrorControls(region) end
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
end

local function drawWorktreeNode(worktree,label,content,enabled)
    local flags=E.worktree==worktree and tImGui.Flags('ImGuiTreeNodeFlags_Selected') or 0
    tImGui.SetNextItemOpen(E.worktree==worktree,tImGui.Flags('ImGuiCond_Always'))
    tImGui.BeginDisabled(enabled==false)
    local opened=tImGui.TreeNodeEx(label..'##mreWorktree'..tostring(worktree),flags)
    local clicked=tImGui.IsItemClicked()
    tImGui.EndDisabled()
    if clicked and enabled~=false then selectWorktree(worktree) end
    if opened then
        if E.worktree==worktree then content() end
        tImGui.TreePop()
    end
end

local function drawControls()
    tImGui.Begin(L('title')..'###MorphRegionEditor',true,0)
    if E.mesh then
        tImGui.Text(string.format(L('project_mesh_info'),projectDisplayName(),E.project.meshPath))
    else
        tImGui.TextWrapped(L('load_mesh_help'))
    end
    tImGui.Separator()
    tImGui.Text(L('worktrees'))
    drawWorktreeNode(1,L('worktree_surface_shapes'),drawSurfaceShapes,true)
    local targetsEnabled=hasReadyRegion()
    drawWorktreeNode(2,L('worktree_morph_targets'),drawMorphTargets,targetsEnabled)
    if not targetsEnabled then tImGui.TextDisabled(L('target_worktree_sequence_help')) end
    local expressionsEnabled=hasReadyTarget()
    drawWorktreeNode(3,L('worktree_expressions'),drawExpressionNode,expressionsEnabled)
    if not expressionsEnabled then tImGui.TextDisabled(L('expression_worktree_sequence_help')) end
    local clipsEnabled=hasReadyExpression()
    drawWorktreeNode(4,L('worktree_clips'),drawClipsNode,clipsEnabled)
    if not clipsEnabled then tImGui.TextDisabled(L('clip_worktree_sequence_help')) end
    local previewEnabled=hasReadyExpression()
    drawWorktreeNode(5,L('worktree_preview'),drawPreviewNode,previewEnabled)
    if not previewEnabled then tImGui.TextDisabled(L('preview_worktree_sequence_help')) end
    if E.status~='' then
        if E.statusError then tImGui.TextColored({r=1,g=0.35,b=0.3,a=1},E.status)
        else tImGui.TextWrapped(E.status) end
    end
    tImGui.End()
    drawConfirmPopup()
    drawExportDialog()
end

function onInitScene()
    mbm.setColor(0.12,0.12,0.14)
    E.dqsSupported=mbm.get('USE_OPENGL_ES') or mbm.get('USE_DIRECTX9') or
        mbm.get('USE_DIRECTX11') or mbm.get('USE_METAL')
    local camera=mbm.getCamera('3d')
    camera:setFar(9999999)
    tUtil.setInitialWindowPositionLeft('MorphRegionEditor',0,25,390)
    tUtil.sMessageOverlay=false
end

function onLoop(delta)
    updateClipPlayback(delta)
    updateMorphPreviewPlayback(delta)
    local nativePreviewActive=E.worktree==5 and E.previewRuntime and
        (E.previewRuntime.comparisonMode~=1 or E.previewRuntime.skeletalPlaying)
    tPreviewRuntime.update(E.previewRuntime,E.mesh,E.worktree==5,nativePreviewActive,E.meshBounds,dpCall)
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
    syncTargetPreview()
    refreshTargetGizmo()
    showMenu()
    syncLanguagePreference()
    drawControls()
    drawClipTimelineWindow(selectedClip())
    showCameraPanel()
    updateTargetBrushHover()
    drawTargetBrushCursor()
    tUtil.showOverlayMessage()
end

function onTouchDown(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
    if key==0 and E.worktree==2 and E.targetMode==2 and E.targetEditMode==2 and
            beginTargetGizmoDrag(x,y) then
        return
    end
    if key==1 and E.worktree==2 and E.targetMode==4 then
        beginTargetBrush(x,y)
        return
    end
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
    if E.targetBrush.stroke then
        local stroke=E.targetBrush.stroke
        local dx,dy=x-stroke.lastX,y-stroke.lastY
        if dx*dx+dy*dy>=16 then stampTargetBrush(x,y,stroke) end
        return
    end
    if key==0 and E.targetGizmo.drag then
        updateTargetGizmoDrag(x,y)
        return
    end
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
    if key==1 and E.targetMode==4 and E.targetBrush.stroke then
        stampTargetBrushAtRelease(x,y,E.targetBrush.stroke)
        finishTargetBrush()
        return
    end
    if key==0 and E.targetGizmo.drag then
        E.targetGizmo.drag=nil
        E.targetGizmo.dirty=true
        return
    end
    if key==1 and E.worktree==2 and E.targetMode==4 then return end
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
    if key==mbm.getKeyCode('control') then
        E.controlDown=true
        return
    end
    if tImGui.GetWantCaptureKeyboard() then return end
    if E.controlDown and key==mbm.getKeyCode('Z') and not tImGui.IsAnyItemActive() then
        applyTargetBrushHistory(false)
    elseif E.controlDown and key==mbm.getKeyCode('Y') and not tImGui.IsAnyItemActive() then
        applyTargetBrushHistory(true)
    elseif E.controlDown and key==mbm.getKeyCode('N') then
        requestAction('new')
    elseif E.controlDown and key==mbm.getKeyCode('O') then
        openProjectDialog()
    elseif E.controlDown and key==mbm.getKeyCode('S') then
        if E.projectPath then saveProject(E.projectPath) else saveProjectAs() end
    elseif E.worktree==1 and key==mbm.getKeyCode('DELETE') then
        removeSelectedPoint()
    end
end

function onKeyUp(key)
    if key==mbm.getKeyCode('control') then E.controlDown=false end
end

function onEndScene()
    destroyMesh()
end
