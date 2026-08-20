--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
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
|------------------------------------------------------------------------------------------------------------------------|

   Skeletal Animation Editor — shared worktree shell
]]--

tImGui = require "ImGui"
tUtil = require "editor_utils"
tWearable = require "skeletal_runtime_wearable_helpers"
tMaskTopology = require "skeletal_mask_topology"
tArmatureTemplates = require "skeletal_armature_templates"
tAnimationImport = require "skeletal_animation_import"
tTutorials = require "skeletal_animation_tutorials"
tTutorialAssets = require "skeletal_animation_tutorial_assets"

local function getTemporaryMeshPath()
    return tUtil.getTemporaryFilePath('.msh')
end

local state = {
    fileName = nil,
    meshD = nil,
    preview = nil,
    comparisonPreview = nil,
    wearableFollowers = {},
    skeletalPreview = {clips={}, selected=1, method=1, execution=1, duration=0, playing=false, paused=false,
        poseStress=false, gpuCpuCompare=false, comparisonReady=false, absoluteLayerSelected=1,
        absoluteLayerDuration=0, absoluteLayerWeight=0.5, absoluteLayerActive=false,
        absoluteLayerFadeDuration=0.25, absoluteLayerMode=1, absoluteLayerPaused=false, speed=1,
        layerMaskWeights={},layerMaskSelected=1,layerMaskDescendants=true,
        layerMaskShowSkeleton=false,runtimePose=nil,previewX=0},
    runtimeLight={enabled=false,
        ambientColor={r=0.16,g=0.16,b=0.2,a=1},
        directionalColor={r=1,g=0.96,b=0.88,a=1},
        directionalDirection={x=-0.4,y=-0.8,z=0.5},orbit=nil},
    animationClipSelected = 1,
    animationEditClipId = nil,
    animationClipName = 'Clip',
    animationClipDuration = 1,
    animationClipLoop = true,
    animationNewClipName = 'Clip',
    animationNewClipDuration = 1,
    animationNewClipLoop = true,
    animationRemoveConfirmed = false,
    animationNewTrackTranslation = true,
    animationNewTrackRotation = true,
    animationNewTrackScale = false,
    animationTrackEdits = {},
    animationNewKeyTimes = {},
    animationKeyEdits = {},
    authoringTime = 0,
    authoringPose = nil,
    authoringPoseKey = nil,
    authoringOverride = nil,
    authoringActiveClip = nil,
    animationTransformTool = 1,
    animationAutoKey = false,
    animationTimelineTrackIndex = nil,
    animationTimelineKeyIndex = nil,
    animationTimelineDrag = nil,
    animationTimelineSelection = {},
    animationKeyClipboard = nil,
    animationBonePoseClipboard = nil,
    animationSkeletonPoseClipboard = nil,
    animationTimelineBox = nil,
    animationTimelineClip = nil,
    animationTimelineEmptyDuration = 0.1,
    animationTimelineRemovalDuration = 0.1,
    animationTimelineRemovalPreview = false,
    animationTimelineRemovalConfirmed = false,
    animationTimelineRemovalConfirmedStart = nil,
    animationTimelineRemovalConfirmedDuration = nil,
    animationTimelineViewStart = 0,
    animationTimelineViewEnd = nil,
    animationTimelineViewClipId = nil,
    animationTimelinePan = nil,
    animationTimelineSnapEnabled = false,
    animationTimelineSnapStep = 1/30,
    animationReport = nil,
    animationImport = {open=false,path='',sourceMesh=nil,sourceBind=nil,sourceClips={},
        sourceClip=1,newName='',analysis=nil,error=nil,keyCount=0,confirmed=false},
    runtimePreviewFromMemory = false,
    runtimePreviewMemoryDirty = false,
    animationPlayback = {playing=false,paused=false,speed=1},
    leftPanelRight = 440,
    translationGizmo = {axes={},boneIndex=nil,poseKey=nil,drag=nil},
    rotationGizmo = {rings={},origin=nil,radius=nil,drag=nil},
    scaleGizmo = {axes={},origin=nil,length=nil,drag=nil},
    boneEditorPosition = {x=0,y=0,z=0},
    boneEditorLength = 10,
    boneEditorExtendCount = 1,
    boneEditorPreserveOtherJoints = true,
    boneEditorSnapAxes = {x=false,y=false,z=false},
    boneEditorSnapStep = 0,
    boneEditorAxisGizmo = {axes={},origin=nil,length=nil},
    boneEditorSegmentTool = 1,
    boneEditorSelectedIndex = nil,
    boneEditorSelection = nil,
    boneEditorDrag = nil,
    boneEditorPendingCycle = nil,
    boneEditorOrientationIndicator = nil,
    boneEditorRemoveBoneId = nil,
    boneEditorRemoveReplacement = 1,
    boneEditorRemoveReparentChildren = false,
    boneEditorRemoveDiscardTracks = false,
    boneEditorRemoveConfirmed = false,
    boneEditorReorientTailsConfirmed = false,
    boneEditorInitializeWeightsConfirmed = false,
    boneEditorInitializeWeightsBoneId = nil,
    boneEditorAutomaticWeightsConfirmed = false,
    boneEditorAutomaticWeightIterations = 3,
    boneEditorRemoveWeightsConfirmed = false,
    boneEditorRemoveAllConfirmed = false,
    boneEditorRemovePreviewIndex = nil,
    boneEditorRadiusBoneId = nil,
    boneEditorRadius = 0,
    boneEditorRadiusSubtree = false,
    boneEditorRotationGuide = nil,
    armatureTemplateSelected = 1,
    armatureTemplateConfirmed = false,
    workspace = 'none',
    meshVisible = true,
    skeletonAlwaysOnTop = true,
    skeletonGizmo = {spheres={}, bones={}, referenceTails={}, referenceSegments={}},
    skeletonGizmoGeneration = 0,
    markersAlwaysOnTop = true,
    info = nil,
    bindReport = nil,
    bindTreeOpenAll = false,
    bindRenameBoneId = nil,
    bindRenameName = '',
    bindReparentBoneId = nil,
    bindParentChoice = 1,
    bindPreserveGlobal = true,
    bindEditBoneId = nil,
    bindEdit = {},
    bindAddBoneId = nil,
    bindAddName = '',
    bindAddParentChoice = 1,
    bindAddTranslation = {x=0,y=1,z=0},
    bindChainBoneId = nil,
    bindChainPrefix = '',
    bindChainCount = 3,
    bindChainStep = {x=0,y=1,z=0},
    bindMirrorBoneId = nil,
    bindMirrorPrefix = 'mirror_',
    bindMirrorAxis = 1,
    bindMirrorConfirmed = false,
    bindWeightsBoneId = nil,
    bindWeightsConfirmed = false,
    bindRemoveBoneId = nil,
    bindRemoveConfirmed = false,
    bindRemoveReplacement = 1,
    bindRemoveDiscardTracks = false,
    bindRemoveReparentChildren = false,
    bindInitialName = 'root',
    bindInitialTranslation = {x=0,y=0,z=0},
    bindInitialRadius = 0.1,
    bindInitialLength = 1,
    modified = false,
    showAdvancedDiagnostics = false,
    boneIndex = 1,
    aabb = nil,
    undoStack = {},
    redoStack = {},
    historyLimit = 50,
    shiftDown = false,
    restrictBones = false,
    allowedBones = {},
    allowedBonesHighlight = false,
    hoveredAllowedBone = nil,
    paint = {boneIndex=1,boneId=nil,radius=0.1,geometry=nil,heatmapLines={},cursor=nil,
        cursorHit=nil,lastSurfaceHit=nil,heatmapDirty=true,showSkeleton=true,heatmapGeneration=0,
        showBrushGradient=true,showBrushFootprint=false,brushFootprintGeneration=0,
        brushFootprintShape=nil,brushFootprintMarkers=nil,
        showVertexInspector=true,hoveredVertex=nil,hoveredVertexMarker=nil,
        inspectorPinned=false,inspectorClick=nil,
        inspectorSeamReport=nil,inspectorSeamMarkers=nil,
        inspectorSeamSyncConfirmed=false,
        globalSeamAudit=nil,globalSeamSyncConfirmed=false,
        inspectorGeometryReport=nil,inspectorGeometryOverlay=nil,
        inspectorGeometryTime=0,
        inspectorTopologyReport=nil,inspectorTopologyOverlay=nil,
        inspectorNormalReport=nil,
        normalRepairThreshold=30,normalRepairConfirmed=false,
        globalNormalAudit=nil,globalNormalRepairConfirmed=false,globalNormalMarkers=nil,
        normalSmoothAngle=30,globalNormalSmoothAudit=nil,
        globalNormalSmoothConfirmed=false,globalNormalSmoothMarkers=nil,
        exactSeamPositionAudit=nil,exactSeamPositionMarkers=nil,
        weightHealthSummary=nil,weightHealthSeamConfirmed=false,
        cursorLastX=nil,cursorLastY=nil,cursorLastUpdate=0,cursorPendingX=nil,cursorPendingY=nil,
        heatmapIndexed=false,strength=0.25,falloffMode=2,operationMode=1,
        rigidCoreRatio=0.6,
        connectedSurfaceOnly=true,
        restrictToHitSubset=false,
        maskVertices={},maskEditMode=0,maskRestrictBrush=false,maskMarkers=nil,
        maskTopologyRings=1,maskTopologyAcrossSeams=true,
        maskSmoothStrength=0.5,maskSmoothIterations=3,
        maskRigidTransitionRings=2,
        diagnosticsUseMask=false,
        aabbCapture={active=false,initialized=false,bounds=nil,result=nil,
            dragPlane=nil,dragOffset=nil,sensitivity=nil},
        aabbCaptureBox=nil,
        aabbCaptureAxisEdges={},aabbCaptureMinFaces={},aabbCaptureMaxFaces={},
        smoothIterations=3,cleanThreshold=0.01,visualizationMode=1,
        maximumInfluences=2,limitInfluencesConfirmed=false,
        abruptThreshold=0.35,abruptRepairStrength=0.5,abruptRepairIterations=3,
        abruptRepairMaxChange=0.2,
        safetyOverlayVisible=true,safetyFaceShape=nil,safetySeamMarkers=nil,safetyReport=nil,
        strokeSafetyOverlayVisible=true,strokeSafetyFaceShape=nil,strokeSafetyReport=nil,
        distributionStats=nil,weakStats=nil,abruptStats=nil,stroke=nil},
    topologyAdjacency = nil,
    coincidentSeams = nil,
    meshBounds = nil,
    status = nil,
    statusError = false,
    statusChanged = false,
    cam = {azimuth = 0.35, elevation = 0.25, distance = 5, fx = 0, fy = 0, fz = 0},
}

local camera3d
local mouseDown = false
local mouseX, mouseY = 0, 0
local noMoveFlag = 0

local function applyRuntimeLighting()
    local light=state.runtimeLight
    local enabled=state.workspace=='runtime' and light.enabled
    mbm.setLightEnabled('3d',enabled)
    if enabled then
        mbm.setAmbientLight('3d',light.ambientColor)
        mbm.setDirectionalLight('3d',light.directionalDirection,light.directionalColor)
    end
end
local cameraMove = {forward=0, right=0, vertical=0}

local function shouldShowSkeleton()
    return state.workspace=='bind' or state.workspace=='bone_editor' or
        state.workspace=='armature_template' or state.workspace=='animation' or
        (state.workspace=='runtime' and state.skeletalPreview.playing and
            state.skeletalPreview.layerMaskShowSkeleton) or
        (state.workspace=='paint' and state.paint.showSkeleton and
            not state.paint.aabbCapture.active)
end

local rebuildSkeletonVisuals
local rebuildPaintHeatmap
local poseSafeRepairScale
local rebuildPaintStrokeSafetyOverlay
local rebuildPaintCursor
local buildTopologyAdjacency
local buildCoincidentSeams
local queryPaintVertices
local readInfluenceMap
local showItemTooltip

local function safeCall(fn)
    local result = table.pack(pcall(fn))
    if not result[1] then
        state.status = tostring(result[2])
        state.statusError = true
        state.statusChanged = false
        return false
    end
    return true, table.unpack(result, 2, result.n)
end

local function setStatus(message, isError, isChanged)
    state.status = message
    state.statusError = isError == true
    state.statusChanged = not state.statusError and isChanged == true
end

local function showHistoryFeedback(message)
    setStatus(message,false)
    tUtil.bRightSide=true
    -- Reset the shared overlay timer instead of merely resuming it. Once the
    -- welcome message expires, start() alone can hide later feedback at once.
    -- Clearing the text also restarts identical consecutive history messages.
    tUtil.sMessageOverlay=false
    tUtil.showMessage(message,4.0)
end

local function shortName(path)
    return path and (path:match('([^/\\]+)$') or path) or ''
end

local function fileDir(path)
    return path and path:match('^(.*)[/\\]') or nil
end

local function destroyObject(object)
    if object then pcall(function() object:destroy() end) end
end

local function destroyRuntimeWearableEntry(entry)
    if entry and entry.preview then
        pcall(function() entry.preview:disableSkeletalPoseSharing() end)
    end
    destroyObject(entry and entry.preview)
end

function unloadRuntimeWearable(index)
    local entry=state.wearableFollowers[index]
    if not entry then return false end
    destroyRuntimeWearableEntry(entry)
    table.remove(state.wearableFollowers,index)
    return true
end

function unloadAllRuntimeWearables()
    for _,entry in ipairs(state.wearableFollowers) do
        destroyRuntimeWearableEntry(entry)
    end
    state.wearableFollowers={}
end

function mirrorRuntimeWearableTransforms()
    if not state.preview then return end
    local x=0
    if state.workspace=='runtime' and
            (state.skeletalPreview.poseStress or state.skeletalPreview.gpuCpuCompare) then
        local bounds=state.meshBounds
        local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
            bounds.maxZ-bounds.minZ) or 1
        x=-extent*0.65
    end
    for _,entry in ipairs(state.wearableFollowers) do
        if entry.preview then pcall(function() entry.preview:setPos(x,0,0) end) end
    end
end

function formatRuntimeWearableReport(report)
    return tWearable.formatCompatibilityReport(report,{
        unavailable=tLang.L('swl_wearable_compat_unavailable'),
        compatibilityFmt=tLang.L('swl_wearable_compat_fmt'),
        valid=tLang.L('swl_wearable_compat_valid'),
        invalid=tLang.L('swl_wearable_compat_invalid'),
        unknown=tLang.L('swl_wearable_compat_unknown'),
        bonesFmt=tLang.L('swl_wearable_compat_bones_fmt'),
        mismatchBoneFmt=tLang.L('swl_wearable_compat_mismatch_bone_fmt'),
        boneIdsFmt=tLang.L('swl_wearable_compat_bone_ids_fmt'),
        parentIndicesFmt=tLang.L('swl_wearable_compat_parent_indices_fmt'),
        bindErrorFmt=tLang.L('swl_wearable_compat_bind_error_fmt'),
    })
end

function loadRuntimeWearable(path)
    if not path or path=='' or not state.preview then return false end
    local entry=tWearable.newFollowerEntry(path)
    state.wearableFollowers[#state.wearableFollowers+1]=entry
    local dir=fileDir(path)
    if dir then mbm.addPath(dir) end
    local method=tWearable.primarySkinningMethod(state.preview)
    if not method then
        entry.status=tLang.L('swl_wearable_primary_method_unresolved')
        return false
    end
    local wearable=mesh:new('3d')
    local execution=tWearable.primaryExecutionPath(state.preview)
    if not wearable:setSkeletalSkinningMethod(method) or
            not wearable:setSkeletalExecutionPath(execution) or
            not wearable:load(path) then
        destroyObject(wearable)
        entry.status=string.format(tLang.L('swl_wearable_load_failed_fmt'),path)
        return false
    end
    local report=wearable:getSkeletalSharingCompatibility(state.preview)
    entry.compatibility=report
    if not report or not report.compatible then
        destroyObject(wearable)
        entry.status=formatRuntimeWearableReport(report)
        return false
    end
    if not wearable:enableSkeletalPoseSharing(state.preview) then
        destroyObject(wearable)
        entry.status=tLang.L('swl_wearable_share_failed')
        return false
    end
    entry.preview=wearable
    entry.status=formatRuntimeWearableReport(report)
    mirrorRuntimeWearableTransforms()
    applyWorkspaceVisibility()
    return true
end

local function clearPaintVisuals()
    for _,object in ipairs(state.paint.heatmapLines) do destroyObject(object) end
    destroyObject(state.paint.cursor)
    destroyObject(state.paint.brushFootprintShape)
    destroyObject(state.paint.brushFootprintMarkers)
    destroyObject(state.paint.hoveredVertexMarker)
    destroyObject(state.paint.inspectorSeamMarkers)
    destroyObject(state.paint.inspectorGeometryOverlay)
    destroyObject(state.paint.inspectorTopologyOverlay)
    destroyObject(state.paint.globalNormalMarkers)
    destroyObject(state.paint.globalNormalSmoothMarkers)
    destroyObject(state.paint.exactSeamPositionMarkers)
    destroyObject(state.paint.safetyFaceShape)
    destroyObject(state.paint.safetySeamMarkers)
    destroyObject(state.paint.strokeSafetyFaceShape)
    destroyObject(state.paint.maskMarkers)
    destroyObject(state.paint.aabbCaptureBox)
    for _,object in pairs(state.paint.aabbCaptureAxisEdges) do destroyObject(object) end
    for _,object in pairs(state.paint.aabbCaptureMinFaces) do destroyObject(object) end
    for _,object in pairs(state.paint.aabbCaptureMaxFaces) do destroyObject(object) end
    state.paint.heatmapLines={}
    state.paint.cursor=nil
    state.paint.brushFootprintShape=nil
    state.paint.brushFootprintMarkers=nil
    state.paint.hoveredVertex=nil
    state.paint.hoveredVertexMarker=nil
    state.paint.inspectorPinned=false
    state.paint.inspectorClick=nil
    state.paint.inspectorSeamReport=nil
    state.paint.inspectorSeamMarkers=nil
    state.paint.inspectorSeamSyncConfirmed=false
    state.paint.globalSeamAudit=nil
    state.paint.globalSeamSyncConfirmed=false
    state.paint.globalNormalAudit=nil
    state.paint.globalNormalRepairConfirmed=false
    destroyObject(state.paint.globalNormalMarkers)
    state.paint.globalNormalMarkers=nil
    state.paint.globalNormalSmoothAudit=nil
    state.paint.globalNormalSmoothConfirmed=false
    destroyObject(state.paint.globalNormalSmoothMarkers)
    state.paint.globalNormalSmoothMarkers=nil
    state.paint.exactSeamPositionAudit=nil
    state.paint.exactSeamPositionMarkers=nil
    state.paint.weightHealthSummary=nil
    state.paint.weightHealthSeamConfirmed=false
    state.paint.inspectorGeometryReport=nil
    state.paint.inspectorGeometryOverlay=nil
    state.paint.inspectorTopologyReport=nil
    state.paint.inspectorTopologyOverlay=nil
    state.paint.inspectorNormalReport=nil
    state.paint.globalNormalAudit=nil
    state.paint.globalNormalRepairConfirmed=false
    state.paint.globalNormalMarkers=nil
    state.paint.safetyFaceShape=nil
    state.paint.safetySeamMarkers=nil
    state.paint.safetyReport=nil
    state.paint.strokeSafetyFaceShape=nil
    state.paint.strokeSafetyReport=nil
    state.paint.maskMarkers=nil
    state.paint.aabbCaptureBox=nil
    state.paint.aabbCaptureAxisEdges={}
    state.paint.aabbCaptureMinFaces={}
    state.paint.aabbCaptureMaxFaces={}
    state.paint.aabbCapture={active=false,initialized=false,bounds=nil,result=nil,
        dragPlane=nil,dragOffset=nil,sensitivity=nil}
    state.paint.cursorHit=nil
    state.paint.lastSurfaceHit=nil
    state.paint.cursorLastX=nil
    state.paint.cursorLastY=nil
    state.paint.cursorPendingX=nil
    state.paint.cursorPendingY=nil
    if state.paint.stroke and state.paint.stroke.snapshot and state.paint.stroke.snapshot.path then
        pcall(os.remove,state.paint.stroke.snapshot.path)
    end
    state.paint.stroke=nil
end

local function appendPoint(coords, x, y, z)
    coords[#coords + 1] = x
    coords[#coords + 1] = y
    coords[#coords + 1] = z
end

local function clearRollback()
    for _,stack in ipairs({state.undoStack,state.redoStack}) do
        for _,entry in ipairs(stack or {}) do
            if entry.path then pcall(os.remove,entry.path) end
        end
    end
    state.undoStack={}
    state.redoStack={}
end

local function computeAABB(meshD)
    local minX, minY, minZ = math.huge, math.huge, math.huge
    local maxX, maxY, maxZ = -math.huge, -math.huge, -math.huge
    local total = 0
    local okS, subsets = safeCall(function() return meshD:getTotalSubset(1) end)
    if not okS then return nil end
    for subset = 1, subsets do
        local okV, vertices = safeCall(function() return meshD:getTotalVertex(1, subset) end)
        if okV then
            for vertex = 1, vertices do
                local okP, p = safeCall(function() return meshD:getVertex(1, subset, vertex) end)
                if okP and p then
                    minX, minY, minZ = math.min(minX, p.x), math.min(minY, p.y), math.min(minZ, p.z)
                    maxX, maxY, maxZ = math.max(maxX, p.x), math.max(maxY, p.y), math.max(maxZ, p.z)
                    total = total + 1
                end
            end
        end
    end
    if total == 0 then return nil end
    return {minX=minX, minY=minY, minZ=minZ, maxX=maxX, maxY=maxY, maxZ=maxZ, total=total}
end

local function applyCamera()
    local c = state.cam
    local x = c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth)
    local y = c.fy + c.distance * math.sin(c.elevation)
    local z = c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
    camera3d:setPos(x, y, z)
    camera3d:setFocus(c.fx, c.fy, c.fz)
end

local function frameCamera(bounds)
    if not bounds then return end
    state.cam.fx = (bounds.minX + bounds.maxX) * 0.5
    state.cam.fy = (bounds.minY + bounds.maxY) * 0.5
    state.cam.fz = (bounds.minZ + bounds.maxZ) * 0.5
    local dx, dy, dz = bounds.maxX - bounds.minX, bounds.maxY - bounds.minY, bounds.maxZ - bounds.minZ
    state.cam.distance = math.max(0.1, math.sqrt(dx * dx + dy * dy + dz * dz) * 1.4)
    applyCamera()
end

local function cameraPosition()
    local c = state.cam
    return c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth),
           c.fy + c.distance * math.sin(c.elevation),
           c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
end

local function updateCameraKeyboard(delta)
    if cameraMove.forward == 0 and cameraMove.right == 0 and cameraMove.vertical == 0 then return end
    if tImGui.GetWantCaptureKeyboard() then return end
    local speed = state.cam.distance * 0.8 * math.max(delta or 0, 0)
    local dx, dz = 0, 0
    if cameraMove.forward ~= 0 or cameraMove.right ~= 0 then
        local fw, rg = camera3d:getNormal('F'), camera3d:getNormal('R')
        local fwLen = math.sqrt(fw.x*fw.x + fw.z*fw.z)
        local rgLen = math.sqrt(rg.x*rg.x + rg.z*rg.z)
        if fwLen > 1e-6 and rgLen > 1e-6 then
            dx = (fw.x/fwLen*cameraMove.forward + rg.x/rgLen*cameraMove.right) * speed
            dz = (fw.z/fwLen*cameraMove.forward + rg.z/rgLen*cameraMove.right) * speed
        end
    end
    state.cam.fx = state.cam.fx + dx
    state.cam.fy = state.cam.fy + cameraMove.vertical * speed
    state.cam.fz = state.cam.fz + dz
    applyCamera()
end

local function rayPlaneHit(sx, sy, point, normal)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, sx, sy)
    if not ok then return nil end
    local denom = normal.x*dx + normal.y*dy + normal.z*dz
    if math.abs(denom) < 1e-6 then return nil end
    local distance = ((point.x-ox)*normal.x + (point.y-oy)*normal.y + (point.z-oz)*normal.z) / denom
    if distance < 0 then return nil end
    return ox+dx*distance, oy+dy*distance, oz+dz*distance
end

local function rayHitsAABB(sx, sy, b)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, sx, sy)
    if not ok then return false end
    local near, far = -math.huge, math.huge
    local function slab(origin, direction, minimum, maximum)
        if math.abs(direction) < 1e-9 then return origin >= minimum and origin <= maximum end
        local a, c = (minimum-origin)/direction, (maximum-origin)/direction
        if a > c then a,c = c,a end
        near, far = math.max(near,a), math.min(far,c)
        return near <= far
    end
    return slab(ox,dx,b.minX,b.maxX) and slab(oy,dy,b.minY,b.maxY) and
           slab(oz,dz,b.minZ,b.maxZ) and far >= 0
end

local function createSelectionBox(b, r, g, blue)
    local p = {
        {b.minX,b.minY,b.minZ}, {b.maxX,b.minY,b.minZ}, {b.maxX,b.maxY,b.minZ}, {b.minX,b.maxY,b.minZ},
        {b.minX,b.minY,b.maxZ}, {b.maxX,b.minY,b.maxZ}, {b.maxX,b.maxY,b.maxZ}, {b.minX,b.maxY,b.maxZ},
    }
    local edges = {1,2,2,3,3,4,4,1,5,6,6,7,7,8,8,5,1,5,2,6,3,7,4,8}
    local coords = {}
    for _, i in ipairs(edges) do
        appendPoint(coords, p[i][1], p[i][2], p[i][3])
    end
    local box = line:new('3d', 0, 0, 0)
    box:add(coords)
    box:setColor(r, g, blue, 1)
    box:setPos(0,0,0)
    return box
end

local function getBones()
    local bones = {}
    local report=state.bindReport
    if not report or report.canonical~=true or type(report.bones)~='table' then return bones end
    for index,bone in ipairs(report.bones) do
        local global=bone.globalBindMatrix or {}
        local parentIndex=bone.parentIndex or 0
        local parent=parentIndex>0 and report.bones[parentIndex] or nil
        bones[#bones+1]={
            index=index,
            sourceIndex=bone.sourceIndex,
            boneId=bone.boneId,
            name=bone.name,
            parentName=parent and parent.name or nil,
            parentIndex=parentIndex,
            parentBoneId=bone.parentBoneId,
            x=global[13] or 0,
            y=global[14] or 0,
            z=global[15] or 0,
            radius=bone.radius or 0,
            length=bone.length or 0,
            tailOffset=bone.tailOffset,
            hasExplicitTail=bone.hasExplicitTail==true,
            connectedToParent=bone.connectedToParent==true,
            childCount=bone.childCount or 0,
            weightedVertexCount=bone.weightedVertexCount or 0,
            animationTrackCount=bone.animationTrackCount or 0,
            weightPaletteReferenced=bone.weightPaletteReferenced==true,
            globalMatrix=global,
        }
    end
    return bones
end

local function getVisualBones()
    local bones=getBones()
    local posedBones=state.workspace=='animation' and state.authoringPose and
        state.authoringPose.bones or state.workspace=='runtime' and
        state.skeletalPreview.runtimePose or nil
    if type(posedBones)~='table' then
        if state.workspace=='runtime' then
            for _,bone in ipairs(bones) do bone.x=bone.x+(state.skeletalPreview.previewX or 0) end
        end
        return bones
    end
    for index,bone in ipairs(bones) do
        local posed=posedBones[index]
        local global=posed and posed.globalMatrix or nil
        if global and (not posed.boneId or posed.boneId==bone.boneId) then
            bone.x=global[13] or bone.x
            bone.y=global[14] or bone.y
            bone.z=global[15] or bone.z
            bone.globalMatrix=global
        end
        if state.workspace=='runtime' then bone.x=bone.x+(state.skeletalPreview.previewX or 0) end
    end
    return bones
end

local function getBoneEditorEndpoints(bone,extent)
    local matrix=bone.globalMatrix or {}
    local offset=bone.tailOffset or {x=0,y=math.max(bone.length or 0,0.001),z=0}
    local ox,oy,oz=offset.x or 0,offset.y or 0,offset.z or 0
    return {x=bone.x,y=bone.y,z=bone.z},
        {x=(matrix[13] or bone.x)+(matrix[1] or 1)*ox+(matrix[5] or 0)*oy+(matrix[9] or 0)*oz,
         y=(matrix[14] or bone.y)+(matrix[2] or 0)*ox+(matrix[6] or 1)*oy+(matrix[10] or 0)*oz,
         z=(matrix[15] or bone.z)+(matrix[3] or 0)*ox+(matrix[7] or 0)*oy+(matrix[11] or 1)*oz}
end

local function refreshBindReport(includeDependencyImpact)
    state.bindReport = nil
    state.animationReport = nil
    if not state.meshD then return end
    local ok, report = safeCall(function()
        return state.meshD:getSkeletonBindReport(includeDependencyImpact~=false)
    end)
    if ok then state.bindReport = report end
end

local function findBone(bones, name)
    for _, bone in ipairs(bones) do if bone.name == name then return bone end end
    return nil
end

local function unitSphereVerts(latSegments,lonSegments)
    latSegments,lonSegments=latSegments or 8,lonSegments or 12
    local vertices={}
    local function push(x,y,z)
        vertices[#vertices+1]=x
        vertices[#vertices+1]=y
        vertices[#vertices+1]=z
    end
    local function xyz(theta,phi)
        local sine=math.sin(theta)
        return sine*math.cos(phi),math.cos(theta),sine*math.sin(phi)
    end
    for latitude=0,latSegments-1 do
        local theta1,theta2=latitude/latSegments*math.pi,(latitude+1)/latSegments*math.pi
        for longitude=0,lonSegments-1 do
            local phi1,phi2=longitude/lonSegments*math.pi*2,(longitude+1)/lonSegments*math.pi*2
            local x1,y1,z1=xyz(theta1,phi1)
            local x2,y2,z2=xyz(theta1,phi2)
            local x3,y3,z3=xyz(theta2,phi1)
            local x4,y4,z4=xyz(theta2,phi2)
            push(x1,y1,z1); push(x3,y3,z3); push(x4,y4,z4)
            push(x1,y1,z1); push(x4,y4,z4); push(x2,y2,z2)
        end
    end
    return vertices
end

local function orientedCylinderVerts(dx,dy,dz,radiusTop,radiusBottom,segments)
    segments=segments or 8
    local height=math.sqrt(dx*dx+dy*dy+dz*dz)
    if height<0.0001 then return {} end
    local ax,ay,az=dx/height,dy/height,dz/height
    local sx,sy,sz=0,1,0
    if math.abs(ay)>0.999 then sx,sy,sz=1,0,0 end
    local rx,ry,rz=ay*sz-az*sy,az*sx-ax*sz,ax*sy-ay*sx
    local rightLength=math.sqrt(rx*rx+ry*ry+rz*rz)
    rx,ry,rz=rx/rightLength,ry/rightLength,rz/rightLength
    local fx,fy,fz=ry*az-rz*ay,rz*ax-rx*az,rx*ay-ry*ax
    local vertices={}
    local function push(u,v,w)
        vertices[#vertices+1]=u*rx+v*ax+w*fx
        vertices[#vertices+1]=u*ry+v*ay+w*fy
        vertices[#vertices+1]=u*rz+v*az+w*fz
    end
    for index=0,segments-1 do
        local a1,a2=index/segments*math.pi*2,(index+1)/segments*math.pi*2
        local x1b,z1b=math.cos(a1)*radiusBottom,math.sin(a1)*radiusBottom
        local x2b,z2b=math.cos(a2)*radiusBottom,math.sin(a2)*radiusBottom
        local x1t,z1t=math.cos(a1)*radiusTop,math.sin(a1)*radiusTop
        local x2t,z2t=math.cos(a2)*radiusTop,math.sin(a2)*radiusTop
        push(x1b,0,z1b); push(x2t,height,z2t); push(x2b,0,z2b)
        push(x1b,0,z1b); push(x1t,height,z1t); push(x2t,height,z2t)
    end
    return vertices
end

local function visualZ(z)
    return z==0 and 0.0001 or z
end

local function destroySkeletonVisuals(preserveTranslationGizmo)
    for _,object in pairs(state.skeletonGizmo.spheres) do destroyObject(object) end
    for _,object in pairs(state.skeletonGizmo.bones) do destroyObject(object) end
    for _,object in pairs(state.skeletonGizmo.referenceTails) do destroyObject(object) end
    for _,object in pairs(state.skeletonGizmo.referenceSegments) do destroyObject(object) end
    state.skeletonGizmo={spheres={},bones={},referenceTails={},referenceSegments={}}
    if not preserveTranslationGizmo then
        for _,object in pairs(state.translationGizmo.axes) do destroyObject(object) end
        state.translationGizmo={axes={},boneIndex=nil,poseKey=nil,drag=nil}
        for _,object in pairs(state.rotationGizmo.rings) do destroyObject(object) end
        state.rotationGizmo={rings={},origin=nil,radius=nil,drag=nil}
        for _,object in pairs(state.scaleGizmo.axes) do destroyObject(object) end
        state.scaleGizmo={axes={},origin=nil,length=nil,drag=nil}
    end
end

local function rebuildBoneEditorOrientationIndicator()
    local indicator=state.boneEditorOrientationIndicator
    if state.workspace~='bone_editor' or not state.boneEditorSelection then
        if indicator then indicator.visible=false end
        return
    end
    local bone=getBones()[state.boneEditorSelection.boneIndex]
    if not bone or not bone.hasExplicitTail then
        if indicator then indicator.visible=false end
        return
    end
    local head,tail=getBoneEditorEndpoints(bone,1)
    if not head or not tail then
        if indicator then indicator.visible=false end
        return
    end
    local dx,dy,dz=tail.x-head.x,tail.y-head.y,tail.z-head.z
    local segmentLength=math.sqrt(dx*dx+dy*dy+dz*dz)
    if segmentLength<=1e-6 then
        if indicator then indicator.visible=false end
        return
    end
    dx,dy,dz=dx/segmentLength,dy/segmentLength,dz/segmentLength
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(bone.radius or 0,extent*0.006,0.001)
    local indicatorLength=math.min(segmentLength*0.6,math.max(radius*4,extent*0.035))
    local rx,ry,rz=0,1,0
    if math.abs(dy)>0.9 then rx,ry,rz=1,0,0 end
    local px,py,pz=dy*rz-dz*ry,dz*rx-dx*rz,dx*ry-dy*rx
    local perpendicularLength=math.sqrt(px*px+py*py+pz*pz)
    if perpendicularLength<=1e-6 then
        if indicator then indicator.visible=false end
        return
    end
    px,py,pz=px/perpendicularLength,py/perpendicularLength,pz/perpendicularLength
    local tip={x=head.x+dx*indicatorLength,y=head.y+dy*indicatorLength,
        z=head.z+dz*indicatorLength}
    local arrowBack=indicatorLength*0.28
    local arrowWidth=indicatorLength*0.16
    local base={x=tip.x-dx*arrowBack,y=tip.y-dy*arrowBack,z=tip.z-dz*arrowBack}
    local points={head.x,head.y,head.z,tip.x,tip.y,tip.z,
        tip.x,tip.y,tip.z,
        base.x+px*arrowWidth,base.y+py*arrowWidth,base.z+pz*arrowWidth,
        tip.x,tip.y,tip.z,
        base.x-px*arrowWidth,base.y-py*arrowWidth,base.z-pz*arrowWidth}
    if indicator then
        indicator:set(points,1)
    else
        indicator=line:new('3d',0,0,0)
        indicator:add(points)
        indicator:setColor(1,0.8,0.05,1)
        indicator.alwaysOnTop=true
        state.boneEditorOrientationIndicator=indicator
    end
    indicator.visible=shouldShowSkeleton()
end

local function rebuildBoneEditorAxisGizmo()
    local gizmo=state.boneEditorAxisGizmo
    if state.workspace~='bone_editor' or not state.boneEditorSelection then
        for _,object in pairs(gizmo.axes) do object.visible=false end
        gizmo.origin=nil
        return
    end
    local bone=getBones()[state.boneEditorSelection.boneIndex]
    if not bone then return end
    local head,tail=getBoneEditorEndpoints(bone,1)
    local kind=state.boneEditorSelection.kind
    local origin=kind=='head' and head or (kind=='tail' or kind=='joint') and tail or
        (kind=='segment' and state.boneEditorSegmentTool==2) and head or
        {x=(head.x+tail.x)*0.5,y=(head.y+tail.y)*0.5,z=(head.z+tail.z)*0.5}
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local length=math.max(extent*0.1,0.05)
    local definitions={x={1,0,0,1,0.15,0.15},y={0,1,0,0.15,1,0.2},z={0,0,1,0.2,0.45,1}}
    local activeAxis=state.boneEditorDrag and state.boneEditorDrag.axisOverride or nil
    for name,definition in pairs(definitions) do
        -- Keep the line geometry local to its renderable. Supplying world-space
        -- vertices while leaving the object transform at zero made every
        -- always-on-top axis sort as if it lived at the scene origin.
        local points={0,0,0,definition[1]*length,definition[2]*length,
            definition[3]*length}
        local object=gizmo.axes[name]
        if object then object:set(points,1) else
            object=line:new('3d',0,0,0); object:add(points)
            object.alwaysOnTop=true; gizmo.axes[name]=object
        end
        object:setPos(origin.x,origin.y,origin.z)
        object:setColor(definition[4],definition[5],definition[6],
            activeAxis and (activeAxis==name and 1 or 0.25) or 1)
        object.visible=shouldShowSkeleton()
    end
    gizmo.origin=origin
    gizmo.length=length
end

local function rebuildBoneEditorRotationGuide()
    local guide=state.boneEditorRotationGuide
    local selection=state.boneEditorSelection
    if state.workspace~='bone_editor' or state.boneEditorSegmentTool~=2 or
            not selection or selection.kind~='segment' then
        if guide then guide.visible=false end
        return
    end
    local bone=getBones()[selection.boneIndex]
    if not bone or not bone.hasExplicitTail then
        if guide then guide.visible=false end
        return
    end
    local head,tail=getBoneEditorEndpoints(bone,1)
    local radius=math.sqrt((tail.x-head.x)^2+(tail.y-head.y)^2+(tail.z-head.z)^2)
    if radius<=1e-6 then if guide then guide.visible=false end return end
    local px,py,pz=cameraPosition()
    local nx,ny,nz=state.cam.fx-px,state.cam.fy-py,state.cam.fz-pz
    local normalLength=math.sqrt(nx*nx+ny*ny+nz*nz)
    if normalLength<=1e-6 then return end
    nx,ny,nz=nx/normalLength,ny/normalLength,nz/normalLength
    local rx,ry,rz=math.abs(ny)<0.9 and 0 or 1,math.abs(ny)<0.9 and 1 or 0,0
    local ux,uy,uz=ny*rz-nz*ry,nz*rx-nx*rz,nx*ry-ny*rx
    local uLength=math.sqrt(ux*ux+uy*uy+uz*uz)
    ux,uy,uz=ux/uLength,uy/uLength,uz/uLength
    local vx,vy,vz=ny*uz-nz*uy,nz*ux-nx*uz,nx*uy-ny*ux
    local points={}
    local segments=48
    for item=0,segments-1 do
        for _,angle in ipairs({item/segments*math.pi*2,(item+1)/segments*math.pi*2}) do
            points[#points+1]=head.x+(ux*math.cos(angle)+vx*math.sin(angle))*radius
            points[#points+1]=head.y+(uy*math.cos(angle)+vy*math.sin(angle))*radius
            points[#points+1]=head.z+(uz*math.cos(angle)+vz*math.sin(angle))*radius
        end
    end
    -- The skeleton is also rendered always-on-top. Pull these markers slightly
    -- toward the camera and extend the cross beyond the joint sphere so the
    -- opaque joint/cylinder cannot hide the rotation pivot and radius.
    local overlayOffset=math.max(radius*0.025,0.0005)
    local centerSize=math.max(radius*0.15,0.002)
    local centerX,centerY,centerZ=head.x-nx*overlayOffset,head.y-ny*overlayOffset,
        head.z-nz*overlayOffset
    local tailX,tailY,tailZ=tail.x-nx*overlayOffset,tail.y-ny*overlayOffset,
        tail.z-nz*overlayOffset
    for _,point in ipairs({
            centerX-ux*centerSize,centerY-uy*centerSize,centerZ-uz*centerSize,
            centerX+ux*centerSize,centerY+uy*centerSize,centerZ+uz*centerSize,
            centerX-vx*centerSize,centerY-vy*centerSize,centerZ-vz*centerSize,
            centerX+vx*centerSize,centerY+vy*centerSize,centerZ+vz*centerSize,
            centerX,centerY,centerZ,tailX,tailY,tailZ}) do
        points[#points+1]=point
    end
    if guide then guide:set(points,1) else
        guide=line:new('3d',0,0,0); guide:add(points); guide:setColor(1,0.85,0.05,1)
        guide.alwaysOnTop=true; state.boneEditorRotationGuide=guide
    end
    guide.visible=shouldShowSkeleton()
end

local function rebuildTranslationGizmo()
    local gizmo=state.translationGizmo
    if state.workspace~='animation' or not state.authoringPose then
        for _,object in pairs(gizmo.axes) do object.visible=false end
        gizmo.boneIndex=nil; gizmo.poseKey=nil; gizmo.origin=nil; gizmo.length=nil
        gizmo.drag=nil
        return
    end
    local bone=getVisualBones()[state.boneIndex]
    if not bone then
        for _,object in pairs(gizmo.axes) do object.visible=false end
        gizmo.boneIndex=nil; gizmo.poseKey=nil; gizmo.origin=nil; gizmo.length=nil
        return
    end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local length=math.max(extent*0.14,0.05)
    local axes={
        x={x=1,y=0,z=0,color={1,0.15,0.15,1}},
        y={x=0,y=1,z=0,color={0.15,1,0.2,1}},
        z={x=0,y=0,z=1,color={0.2,0.45,1,1}},
    }
    for name,axis in pairs(axes) do
        local object=gizmo.axes[name]
        local points={0,0,0,axis.x*length,axis.y*length,axis.z*length}
        if object then object:set(points,1) else
            object=line:new('3d',0,0,0)
            object:add(points)
            object.alwaysOnTop=true
            gizmo.axes[name]=object
        end
        object:setPos(bone.x,bone.y,bone.z)
        object:setColor(table.unpack(axis.color))
        object.visible=state.animationTransformTool==1
    end
    gizmo.boneIndex=state.boneIndex
    gizmo.poseKey=state.authoringPoseKey
    gizmo.origin={x=bone.x,y=bone.y,z=bone.z}
    gizmo.length=length
end

local function normalizedDirection(x,y,z)
    local length=math.sqrt(x*x+y*y+z*z)
    if length<=1e-8 then return nil end
    return {x=x/length,y=y/length,z=z/length}
end

local function rebuildRotationGizmo()
    local gizmo=state.rotationGizmo
    if state.workspace~='animation' or not state.authoringPose then
        for _,object in pairs(gizmo.rings) do object.visible=false end
        gizmo.origin=nil; gizmo.radius=nil; gizmo.axes=nil; gizmo.drag=nil
        return
    end
    local bone=getVisualBones()[state.boneIndex]
    if not bone then
        for _,object in pairs(gizmo.rings) do object.visible=false end
        gizmo.origin=nil; gizmo.radius=nil; gizmo.axes=nil
        return
    end
    local matrix=bone.globalMatrix or {}
    local axes={
        x=normalizedDirection(matrix[1] or 1,matrix[2] or 0,matrix[3] or 0),
        y=normalizedDirection(matrix[5] or 0,matrix[6] or 1,matrix[7] or 0),
        z=normalizedDirection(matrix[9] or 0,matrix[10] or 0,matrix[11] or 1),
    }
    if not axes.x or not axes.y or not axes.z then return end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(extent*0.11,0.04)
    local definitions={
        x={u=axes.y,v=axes.z,color={1,0.15,0.15,1}},
        y={u=axes.z,v=axes.x,color={0.15,1,0.2,1}},
        z={u=axes.x,v=axes.y,color={0.2,0.45,1,1}},
    }
    for name,definition in pairs(definitions) do
        local points={}
        for item=0,64 do
            local angle=item/64*math.pi*2
            points[#points+1]=(definition.u.x*math.cos(angle)+definition.v.x*math.sin(angle))*radius
            points[#points+1]=(definition.u.y*math.cos(angle)+definition.v.y*math.sin(angle))*radius
            points[#points+1]=(definition.u.z*math.cos(angle)+definition.v.z*math.sin(angle))*radius
        end
        local object=gizmo.rings[name]
        if object then object:set(points,1) else
            object=line:new('3d',0,0,0); object:add(points); object.alwaysOnTop=true
            gizmo.rings[name]=object
        end
        object:setPos(bone.x,bone.y,bone.z)
        object:setColor(table.unpack(definition.color))
        object.visible=state.animationTransformTool==2
    end
    gizmo.origin={x=bone.x,y=bone.y,z=bone.z}
    gizmo.radius=radius
    gizmo.axes=axes
end

local function rebuildScaleGizmo()
    local gizmo=state.scaleGizmo
    if state.workspace~='animation' or not state.authoringPose then
        for _,object in pairs(gizmo.axes) do object.visible=false end
        gizmo.origin=nil; gizmo.length=nil; gizmo.axesLocal=nil; gizmo.drag=nil
        return
    end
    local bone=getVisualBones()[state.boneIndex]
    if not bone then
        for _,object in pairs(gizmo.axes) do object.visible=false end
        gizmo.origin=nil; gizmo.length=nil; gizmo.axesLocal=nil
        return
    end
    local matrix=bone.globalMatrix or {}
    local axes={
        x=normalizedDirection(matrix[1] or 1,matrix[2] or 0,matrix[3] or 0),
        y=normalizedDirection(matrix[5] or 0,matrix[6] or 1,matrix[7] or 0),
        z=normalizedDirection(matrix[9] or 0,matrix[10] or 0,matrix[11] or 1),
    }
    if not axes.x or not axes.y or not axes.z then return end
    axes.uniform=normalizedDirection(axes.x.x+axes.y.x+axes.z.x,
        axes.x.y+axes.y.y+axes.z.y,axes.x.z+axes.y.z+axes.z.z)
    if not axes.uniform then return end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local length=math.max(extent*0.14,0.05)
    for _,object in pairs(gizmo.axes) do object.visible=false end
    local drawAxes={uniform=axes.uniform}
    local colors={uniform={1,0.85,0.05,1}}
    for name,axis in pairs(drawAxes) do
        local object=gizmo.axes[name]
        local points={0,0,0,axis.x*length,axis.y*length,axis.z*length,
            axis.x*length,axis.y*length,axis.z*length,
            axis.x*length*0.88,axis.y*length*0.88,axis.z*length*0.88}
        if object then object:set(points,1) else
            object=line:new('3d',0,0,0); object:add(points); object.alwaysOnTop=true
            gizmo.axes[name]=object
        end
        object:setPos(bone.x,bone.y,bone.z)
        object:setColor(table.unpack(colors[name]))
        object.visible=state.animationTransformTool==3
    end
    gizmo.origin={x=bone.x,y=bone.y,z=bone.z}
    gizmo.length=length
    gizmo.axesLocal=drawAxes
end

local function nextSkeletonNickname(prefix)
    state.skeletonGizmoGeneration=state.skeletonGizmoGeneration+1
    return prefix..state.skeletonGizmoGeneration
end

local function createBoneShape(x,y,z,vertices,nickname,r,g,b,a)
    local object=shape:new('3d',x,y,visualZ(z))
    object:create(vertices,nil,nextSkeletonNickname(nickname))
    object:setColor(r,g,b,a)
    object.visible=shouldShowSkeleton()
    object.alwaysOnTop=state.skeletonAlwaysOnTop
    object.alwaysOnTopPriority=1
    return object
end

local function updateSkeletonVisibility()
    for _,object in pairs(state.skeletonGizmo.spheres) do
        object.visible=shouldShowSkeleton()
    end
    for _,object in pairs(state.skeletonGizmo.bones) do object.visible=shouldShowSkeleton() end
    for _,object in pairs(state.skeletonGizmo.referenceTails) do
        object.visible=shouldShowSkeleton()
    end
    for _,object in pairs(state.skeletonGizmo.referenceSegments) do
        object.visible=shouldShowSkeleton()
    end
end

function applyWorkspaceVisibility()
    local paintWorkspace=state.workspace=='paint'
    local runtimeWorkspace=state.workspace=='runtime'
    rebuildBoneEditorOrientationIndicator()
    rebuildBoneEditorAxisGizmo()
    rebuildBoneEditorRotationGuide()

    if state.preview then
        -- Paint Weights' filled-face heatmap is a complete read-only surface copy. Hiding the
        -- textured preview there avoids z-fighting and color mixing; other worktrees keep it.
        state.preview.visible=(state.meshVisible or (paintWorkspace and
            state.paint.aabbCapture.active)) and not
            (paintWorkspace and #state.paint.heatmapLines>0 and
                not state.paint.aabbCapture.active)
        pcall(function()
            local x=0
            if runtimeWorkspace and
                    (state.skeletalPreview.poseStress or state.skeletalPreview.gpuCpuCompare) then
                local bounds=state.meshBounds
                local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                    bounds.maxZ-bounds.minZ) or 1
                x=-extent*0.65
            end
            state.preview:setPos(x,0,0)
        end)
    end
    if state.comparisonPreview then
        state.comparisonPreview.visible=state.meshVisible and runtimeWorkspace and
            (state.skeletalPreview.poseStress or state.skeletalPreview.gpuCpuCompare)
    end
    for _,entry in ipairs(state.wearableFollowers) do
        if entry.preview then
            entry.preview.visible=state.meshVisible and entry.visible and runtimeWorkspace
        end
    end
    mirrorRuntimeWearableTransforms()
    for _,marker in ipairs(state.paint.heatmapLines) do
        marker.visible=paintWorkspace and state.meshVisible and not state.paint.aabbCapture.active
    end
    if state.paint.cursor then
        state.paint.cursor.visible=paintWorkspace and state.meshVisible and
            not state.paint.aabbCapture.active
    end
    if state.paint.brushFootprintShape then
        state.paint.brushFootprintShape.visible=paintWorkspace and state.meshVisible and
            state.paint.visualizationMode==1 and state.paint.showBrushGradient and
            not state.paint.aabbCapture.active
    end
    if state.paint.brushFootprintMarkers then
        state.paint.brushFootprintMarkers.visible=paintWorkspace and state.meshVisible and
            state.paint.visualizationMode==1 and state.paint.showBrushFootprint and
            not state.paint.aabbCapture.active
    end
    if state.paint.maskMarkers then
        state.paint.maskMarkers.visible=paintWorkspace and state.meshVisible and
            not state.paint.aabbCapture.active
    end
    if state.paint.aabbCaptureBox then
        state.paint.aabbCaptureBox.visible=paintWorkspace and state.paint.aabbCapture.active
    end
    if not paintWorkspace or not state.paint.aabbCapture.active then
        for _,object in pairs(state.paint.aabbCaptureAxisEdges) do object.visible=false end
        for _,object in pairs(state.paint.aabbCaptureMinFaces) do object.visible=false end
        for _,object in pairs(state.paint.aabbCaptureMaxFaces) do object.visible=false end
    end
    if state.paint.hoveredVertexMarker then
        state.paint.hoveredVertexMarker.visible=paintWorkspace and state.meshVisible and
            state.showAdvancedDiagnostics and
            state.paint.showVertexInspector and
            not state.paint.aabbCapture.active
    end
    if state.paint.inspectorSeamMarkers then
        state.paint.inspectorSeamMarkers.visible=paintWorkspace and state.meshVisible and
            state.showAdvancedDiagnostics and
            state.paint.showVertexInspector and state.paint.inspectorPinned and
            not state.paint.aabbCapture.active
    end
    if state.paint.inspectorGeometryOverlay then
        state.paint.inspectorGeometryOverlay.visible=paintWorkspace and state.meshVisible and
            state.showAdvancedDiagnostics and
            state.paint.showVertexInspector and state.paint.inspectorPinned and
            not state.paint.aabbCapture.active
    end
    if state.paint.inspectorTopologyOverlay then
        state.paint.inspectorTopologyOverlay.visible=paintWorkspace and state.meshVisible and
            state.showAdvancedDiagnostics and
            state.paint.showVertexInspector and state.paint.inspectorPinned and
            not state.paint.aabbCapture.active
    end
    if state.paint.globalNormalMarkers then
        state.paint.globalNormalMarkers.visible=paintWorkspace and state.meshVisible and
            state.showAdvancedDiagnostics and
            not state.paint.aabbCapture.active
    end
    if state.paint.globalNormalSmoothMarkers then
        state.paint.globalNormalSmoothMarkers.visible=paintWorkspace and state.meshVisible and
            state.showAdvancedDiagnostics and
            not state.paint.aabbCapture.active
    end
    if state.paint.exactSeamPositionMarkers then
        state.paint.exactSeamPositionMarkers.visible=paintWorkspace and state.meshVisible and
            state.showAdvancedDiagnostics and
            not state.paint.aabbCapture.active
    end
    if state.paint.safetyFaceShape then
        state.paint.safetyFaceShape.visible=paintWorkspace and
            state.paint.visualizationMode==4 and state.paint.safetyOverlayVisible
    end
    if state.paint.safetySeamMarkers then
        state.paint.safetySeamMarkers.visible=paintWorkspace and
            state.paint.visualizationMode==4 and state.paint.safetyOverlayVisible
    end
    if state.paint.strokeSafetyFaceShape then
        state.paint.strokeSafetyFaceShape.visible=paintWorkspace and
            state.paint.visualizationMode==1 and state.paint.strokeSafetyOverlayVisible
    end
    local selectedBindBone=(state.workspace=='bind' or state.workspace=='animation' or
        (state.workspace=='paint' and state.paint.visualizationMode==1)) and
        getBones()[state.boneIndex] or (state.workspace=='runtime' and
        getBones()[state.skeletalPreview.layerMaskSelected] or nil) or (state.workspace=='bone_editor' and
        state.boneEditorSelectedIndex and getBones()[state.boneEditorSelectedIndex] or nil)
    local runtimeMaskByName={}
    if state.workspace=='runtime' then
        for _,bone in ipairs(getBones()) do runtimeMaskByName[bone.name]=bone end
    end
    local function runtimeMaskColor(bone)
        local weight=bone and (state.skeletalPreview.layerMaskWeights[bone.boneId] or 1) or 1
        if weight<0.5 then return 0.1,0.3+weight*1.4,1-weight*1.2,0.9 end
        return (weight-0.5)*1.8+0.1,1-(weight-0.5)*1.6,0.15,0.9
    end
    local removalPreviewBone=state.workspace=='bone_editor' and
        state.boneEditorRemovePreviewIndex and getBones()[state.boneEditorRemovePreviewIndex] or nil
    for name,object in pairs(state.skeletonGizmo.spheres) do
        local boneEditorSelected=state.workspace=='bone_editor' and state.boneEditorSelection and
            ((state.boneEditorSelection.kind=='head' and name==state.boneEditorSelection.boneName) or
             (state.boneEditorSelection.kind=='tail' and
                name==state.boneEditorSelection.boneName..'::tail') or
             (state.boneEditorSelection.kind=='joint' and
                state.boneEditorSelection.members and state.boneEditorSelection.members[name]) or
             (state.boneEditorSelection.kind=='segment' and
                (name==state.boneEditorSelection.boneName or
                 name==state.boneEditorSelection.boneName..'::tail')))
        if boneEditorSelected then
            object:setColor(0.1,0.85,1,1)
        elseif removalPreviewBone and (name==removalPreviewBone.name or
                name==removalPreviewBone.name..'::tail') then
            object:setColor(0.2,1,0.25,1)
        elseif selectedBindBone and name==selectedBindBone.name then
            object:setColor(0.1,0.85,1,1)
        elseif state.workspace=='runtime' then
            object:setColor(runtimeMaskColor(runtimeMaskByName[name]))
        else
            object:setColor(1,0,1,0.85)
        end
    end
    for boneId,object in pairs(state.skeletonGizmo.bones) do
        if state.workspace=='bone_editor' and state.boneEditorSelection and
                state.boneEditorSelection.kind=='segment' and
                boneId==state.boneEditorSelection.boneId then
            object:setColor(0.1,0.85,1,1)
        elseif removalPreviewBone and boneId==removalPreviewBone.boneId then
            object:setColor(0.2,1,0.25,1)
        elseif selectedBindBone and boneId==selectedBindBone.boneId then
            object:setColor(0.1,0.85,1,1)
        elseif state.workspace=='runtime' then
            local bone=nil
            for _,candidate in pairs(runtimeMaskByName) do
                if candidate.boneId==boneId then bone=candidate break end
            end
            object:setColor(runtimeMaskColor(bone))
        else
            object:setColor(1,0,1,0.75)
        end
    end
    updateSkeletonVisibility()
end

local function setWorkspace(workspace)
    -- The former Skin Weight Lab worktree is retired. Old in-session history snapshots created
    -- before its removal must return to the supported visual authoring workflow.
    if workspace=='weights' then workspace='paint' end
    if state.workspace==workspace then return end
    if state.workspace=='runtime' and workspace~='runtime' then
        pcall(function() if state.preview then state.preview:stopSkeletalAnimation() end end)
        pcall(function()
            if state.comparisonPreview then state.comparisonPreview:stopSkeletalAnimation() end
        end)
        local playback=state.skeletalPreview
        playback.playing=false
        playback.paused=false
        playback.absoluteLayerActive=false
        playback.absoluteLayerPaused=false
        playback.runtimePose=nil
        playback.comparisonReady=false
    end
    if state.workspace=='animation' and workspace~='animation' then
        pcall(function() if state.preview then state.preview:stopSkeletalAnimation() end end)
        state.authoringPose=nil
        state.authoringPoseKey=nil
        state.authoringOverride=nil
        state.authoringActiveClip=nil
        state.animationTimelineDrag=nil
        state.animationTimelineSelection={}
        state.animationTimelineBox=nil
        state.animationPlayback.playing=false
        state.animationPlayback.paused=false
    end
    state.workspace=workspace
    applyRuntimeLighting()
    if rebuildSkeletonVisuals then rebuildSkeletonVisuals() end
    if workspace=='paint' and state.paint.heatmapDirty then rebuildPaintHeatmap() end
    applyWorkspaceVisibility()
    if state.meshBounds then
        local bounds={}
        for key,value in pairs(state.meshBounds) do bounds[key]=value end
        if workspace=='runtime' and
                (state.skeletalPreview.poseStress or state.skeletalPreview.gpuCpuCompare) then
            local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                bounds.maxZ-bounds.minZ)
            local separation=extent*0.65
            bounds.minX=bounds.minX-separation
            bounds.maxX=bounds.maxX+separation
        end
        frameCamera(bounds)
    end
end

local function updateAllowedBoneColors()
    local selectedBindBone=state.workspace=='bind' and getBones()[state.boneIndex] or nil
    for name,object in pairs(state.skeletonGizmo.spheres) do
        if selectedBindBone and name==selectedBindBone.name then
            object:setColor(0.1,0.85,1,1)
        else
            object:setColor(1,0,1,0.85)
        end
    end
end

rebuildSkeletonVisuals=function()
    local translationDrag=state.translationGizmo.drag
    destroySkeletonVisuals(true)
    state.translationGizmo.drag=translationDrag
    local bones=getVisualBones()
    local byName={}
    for _,bone in ipairs(bones) do byName[bone.name]=bone end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    for _,bone in ipairs(bones) do
        local radius=math.max(bone.radius or 0,extent*0.006,0.001)
        local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
            'swl_bone_joint_',1,0,1,0.85)
        sphere:setScale(radius,radius,radius)
        state.skeletonGizmo.spheres[bone.name]=sphere
        if state.workspace=='animation' and bone.hasExplicitTail then
            local head,tailPoint=getBoneEditorEndpoints(bone,extent)
            local tail=createBoneShape(tailPoint.x,tailPoint.y,tailPoint.z,unitSphereVerts(),
                'swl_animation_reference_tail_',1,0,1,0.6)
            tail:setScale(radius,radius,radius)
            state.skeletonGizmo.referenceTails[bone.boneId]=tail
            local headVisualZ,tailVisualZ=visualZ(head.z),visualZ(tailPoint.z)
            local segment=line:new('3d',head.x,head.y,headVisualZ)
            segment:add({0,0,0,tailPoint.x-head.x,tailPoint.y-head.y,
                tailVisualZ-headVisualZ})
            segment:setColor(1,0,1,0.65)
            segment.visible=shouldShowSkeleton()
            segment.alwaysOnTop=state.skeletonAlwaysOnTop
            segment.alwaysOnTopPriority=1
            state.skeletonGizmo.referenceSegments[bone.boneId]=segment
        end
        if state.workspace=='bone_editor' or state.workspace=='paint' then
            if bone.hasExplicitTail then
                local head,tailPoint=getBoneEditorEndpoints(bone,extent)
                local tx,ty,tz=tailPoint.x,tailPoint.y,tailPoint.z
                if state.workspace=='bone_editor' then
                    local tail=createBoneShape(tx,ty,tz,unitSphereVerts(),
                        'swl_bone_tail_',1,0,1,0.85)
                    tail:setScale(radius,radius,radius)
                    state.skeletonGizmo.spheres[bone.name..'::tail']=tail
                end
                local dx,dy,dz=tx-bone.x,ty-bone.y,tz-bone.z
                local link=createBoneShape(bone.x,bone.y,bone.z,
                    orientedCylinderVerts(dx,dy,dz,radius*0.5,radius*0.5,8),
                    'swl_bone_own_link_',1,0,1,0.75)
                state.skeletonGizmo.bones[bone.boneId]=link
            end
        else
            local parent=bone.parentName and byName[bone.parentName]
            if parent then
            local dx,dy,dz=bone.x-parent.x,bone.y-parent.y,bone.z-parent.z
            local parentRadius=math.max(parent.radius or 0,extent*0.006,0.001)
            if dx*dx+dy*dy+dz*dz>0.000001 then
                local link
                if state.workspace=='animation' or state.workspace=='runtime' then
                    local parentVisualZ,boneVisualZ=visualZ(parent.z),visualZ(bone.z)
                    link=line:new('3d',parent.x,parent.y,parentVisualZ)
                    link:add({0,0,0,dx,dy,boneVisualZ-parentVisualZ})
                    link:setColor(1,0,1,0.9)
                    link.visible=shouldShowSkeleton()
                    link.alwaysOnTop=state.skeletonAlwaysOnTop
                    link.alwaysOnTopPriority=1
                else
                    link=createBoneShape(parent.x,parent.y,parent.z,
                        orientedCylinderVerts(dx,dy,dz,radius*0.5,parentRadius*0.5,8),
                        'swl_bone_link_',1,0,1,0.75)
                end
                -- A visual bone segment belongs to its child transform: parent joint -> child
                -- joint. Keying by the child's stable ID lets tree selection highlight the exact
                -- incoming segment even after rename or future hierarchy reordering.
                state.skeletonGizmo.bones[bone.boneId]=link
            end
            end
        end
    end
    rebuildTranslationGizmo()
    rebuildRotationGizmo()
    rebuildScaleGizmo()
end


local function updateAnimationSkeletonVisuals()
    if state.workspace~='animation' then return false end
    local bones=getVisualBones()
    local byName={}
    for _,bone in ipairs(bones) do byName[bone.name]=bone end
    for _,bone in ipairs(bones) do
        local sphere=state.skeletonGizmo.spheres[bone.name]
        if not sphere then return false end
        sphere:setPos(bone.x,bone.y,visualZ(bone.z))
        if bone.hasExplicitTail then
            local _,tailPoint=getBoneEditorEndpoints(bone,1)
            local tail=state.skeletonGizmo.referenceTails[bone.boneId]
            local segment=state.skeletonGizmo.referenceSegments[bone.boneId]
            if not tail or not segment then return false end
            local headVisualZ,tailVisualZ=visualZ(bone.z),visualZ(tailPoint.z)
            tail:setPos(tailPoint.x,tailPoint.y,tailVisualZ)
            segment:set({0,0,0,tailPoint.x-bone.x,tailPoint.y-bone.y,
                tailVisualZ-headVisualZ},1)
            segment:setPos(bone.x,bone.y,headVisualZ)
        end
        if bone.parentName then
            local parent=byName[bone.parentName]
            local link=state.skeletonGizmo.bones[bone.boneId]
            if not parent or not link then return false end
            local parentVisualZ,boneVisualZ=visualZ(parent.z),visualZ(bone.z)
            link:set({0,0,0,bone.x-parent.x,bone.y-parent.y,boneVisualZ-parentVisualZ},1)
            link:setPos(parent.x,parent.y,parentVisualZ)
        end
    end
    rebuildTranslationGizmo()
    rebuildRotationGizmo()
    rebuildScaleGizmo()
    updateSkeletonVisibility()
    return true
end

local function updateRuntimeSkeletonVisuals()
    local playback=state.skeletalPreview
    if state.workspace~='runtime' or not playback.playing or
            not playback.layerMaskShowSkeleton or not state.preview then return false end
    local pose=state.preview:getSkeletalAnimationPose()
    if type(pose)~='table' then return false end
    playback.runtimePose=pose
    local bones=getVisualBones()
    local byName={}
    for _,bone in ipairs(bones) do byName[bone.name]=bone end
    for _,bone in ipairs(bones) do
        local sphere=state.skeletonGizmo.spheres[bone.name]
        if not sphere then return false end
        sphere:setPos(bone.x,bone.y,visualZ(bone.z))
        if bone.parentName then
            local parent=byName[bone.parentName]
            local link=state.skeletonGizmo.bones[bone.boneId]
            if not parent or not link then return false end
            local parentVisualZ,boneVisualZ=visualZ(parent.z),visualZ(bone.z)
            link:set({0,0,0,bone.x-parent.x,bone.y-parent.y,boneVisualZ-parentVisualZ},1)
            link:setPos(parent.x,parent.y,parentVisualZ)
        end
    end
    applyWorkspaceVisibility()
    return true
end


local function invalidateAuthoringPose()
    state.authoringPoseKey=nil
end

local function clearAuthoringOverride()
    state.authoringOverride=nil
    state.translationGizmo.drag=nil
    state.rotationGizmo.drag=nil
    state.scaleGizmo.drag=nil
    invalidateAuthoringPose()
end

local function refreshAuthoringPose(clip)
    if not state.meshD or not state.preview or not clip then return false end
    local okMethod,method=safeCall(function()
        return state.preview:getResolvedSkeletalSkinningMethod()
    end)
    if not okMethod or (method~='lbs' and method~='dqs') then return false end
    local duration=math.max(0,clip.duration or 0)
    state.authoringTime=math.max(0,math.min(state.authoringTime or 0,duration))
    local key=string.format('%s:%d:%.9g:%s',clip.clipId or '?',
        state.animationClipSelected,state.authoringTime,method)
    if state.authoringPoseKey==key and state.authoringPose then return true end
    local override=state.authoringOverride
    local okPose,pose=safeCall(function()
        if override and override.clipIndex==state.animationClipSelected and
                math.abs(override.time-state.authoringTime)<1e-6 then
            local t,q,s=override.translation,override.rotation,override.scale
            return state.meshD:evaluateSkeletalAuthoringPose(state.animationClipSelected,
                state.authoringTime,method,override.boneIndex,t.x,t.y,t.z,
                q.x,q.y,q.z,q.w,s.x,s.y,s.z)
        end
        return state.meshD:evaluateSkeletalAuthoringPose(
            state.animationClipSelected,state.authoringTime,method)
    end)
    if not okPose or type(pose)~='table' then return false end
    local okApply,applied,reason=safeCall(function()
        return state.preview:setSkeletalAuthoringPalette(method,pose.palette,state.authoringTime,
            pose.boneIds)
    end)
    if not okApply or not applied then
        setStatus(string.format(tLang.L('swl_animation_pose_incompatible_fmt'),
            tostring(reason or 'unknown')),true)
        return false
    end
    state.authoringPose=pose
    state.authoringPoseKey=key
    if not updateAnimationSkeletonVisuals() then rebuildSkeletonVisuals() end
    applyWorkspaceVisibility()
    return true
end

local function pointSegmentDistanceSquared(p, a, b)
    local dx, dy, dz = b.x-a.x, b.y-a.y, b.z-a.z
    local lengthSquared = dx*dx + dy*dy + dz*dz
    if lengthSquared < 1e-12 then
        local x, y, z = p.x-a.x, p.y-a.y, p.z-a.z
        return x*x + y*y + z*z
    end
    local t = ((p.x-a.x)*dx + (p.y-a.y)*dy + (p.z-a.z)*dz) / lengthSquared
    t = math.max(0, math.min(1, t))
    local x, y, z = p.x-(a.x+dx*t), p.y-(a.y+dy*t), p.z-(a.z+dz*t)
    return x*x + y*y + z*z
end

local function raySphereDistance(ox,oy,oz,dx,dy,dz,cx,cy,cz,radius)
    local lx,ly,lz=cx-ox,cy-oy,cz-oz
    local projected=lx*dx+ly*dy+lz*dz
    if projected<0 then return nil end
    local perpendicular=lx*lx+ly*ly+lz*lz-projected*projected
    local radiusSquared=radius*radius
    if perpendicular>radiusSquared then return nil end
    return projected-math.sqrt(math.max(0,radiusSquared-perpendicular))
end

local function raySegmentDistance(ox,oy,oz,dx,dy,dz,a,b,radius)
    local vx,vy,vz=b.x-a.x,b.y-a.y,b.z-a.z
    local lengthSquared=vx*vx+vy*vy+vz*vz
    if lengthSquared<1e-12 then
        return raySphereDistance(ox,oy,oz,dx,dy,dz,a.x,a.y,a.z,radius)
    end
    local wx,wy,wz=ox-a.x,oy-a.y,oz-a.z
    local uv=dx*vx+dy*vy+dz*vz
    local uw=dx*wx+dy*wy+dz*wz
    local vw=vx*wx+vy*wy+vz*wz
    local denominator=lengthSquared-uv*uv
    local segmentT
    if math.abs(denominator)<1e-12 then segmentT=-vw/lengthSquared
    else segmentT=(vw-uv*uw)/denominator end
    segmentT=math.max(0,math.min(1,segmentT))
    local px,py,pz=a.x+vx*segmentT,a.y+vy*segmentT,a.z+vz*segmentT
    local rayT=(px-ox)*dx+(py-oy)*dy+(pz-oz)*dz
    if rayT<0 then return nil end
    local qx,qy,qz=ox+dx*rayT,oy+dy*rayT,oz+dz*rayT
    local ex,ey,ez=px-qx,py-qy,pz-qz
    if ex*ex+ey*ey+ez*ez>radius*radius then return nil end
    return rayT
end

local function hitTestBoneEditor(sx,sy)
    if state.workspace~='bone_editor' then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local bones=getBones()
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local pickRadius=math.max(extent*0.012,0.002)
    local candidates={}
    local seen={}
    local byName={}
    for index,bone in ipairs(bones) do bone.arrayIndex=index; byName[bone.name]=bone end
    local connectedChildren={}
    for _,bone in ipairs(bones) do
        if bone.connectedToParent and bone.parentName then
            connectedChildren[bone.parentName]=connectedChildren[bone.parentName] or {}
            connectedChildren[bone.parentName][#connectedChildren[bone.parentName]+1]=bone
        end
    end
    local function consider(distance,selection,key)
        if distance and not seen[key] then
            seen[key]=true
            selection.pickKey=key
            candidates[#candidates+1]={distance=distance,selection=selection,key=key}
        end
    end
    local function sharedJointSelection(parent)
        local members={[parent.name..'::tail']=true}
        for _,child in ipairs(connectedChildren[parent.name] or {}) do members[child.name]=true end
        return {kind='joint',boneIndex=parent.arrayIndex,boneId=parent.boneId,
            boneName=parent.name,members=members}
    end
    for index,bone in ipairs(bones) do
        local head,tail=getBoneEditorEndpoints(bone,extent)
        local headObject=state.skeletonGizmo.spheres[bone.name]
        local tailObject=state.skeletonGizmo.spheres[bone.name..'::tail']
        local headRadius,tailRadius=pickRadius,pickRadius
        if headObject then headRadius=math.max(headRadius,headObject:getScale().x or 0) end
        if tailObject then tailRadius=math.max(tailRadius,tailObject:getScale().x or 0) end
        local headSelection={kind='head',boneIndex=index,boneId=bone.boneId,boneName=bone.name}
        local headKey='head:'..tostring(bone.boneId)
        if bone.connectedToParent and bone.parentName and byName[bone.parentName] then
            local parent=byName[bone.parentName]
            headSelection=sharedJointSelection(parent)
            headKey='joint:'..tostring(parent.boneId)
        end
        consider(raySphereDistance(ox,oy,oz,dx,dy,dz,head.x,head.y,head.z,headRadius),
            headSelection,headKey)
        if bone.hasExplicitTail then
            local tailSelection={kind='tail',boneIndex=index,boneId=bone.boneId,boneName=bone.name}
            local tailKey='tail:'..tostring(bone.boneId)
            if connectedChildren[bone.name] then
                tailSelection=sharedJointSelection(bone)
                tailKey='joint:'..tostring(bone.boneId)
            end
            consider(raySphereDistance(ox,oy,oz,dx,dy,dz,tail.x,tail.y,tail.z,tailRadius),
                tailSelection,tailKey)
        end
    end
    for index,bone in ipairs(bones) do
        local head,tail=getBoneEditorEndpoints(bone,extent)
        if bone.hasExplicitTail then
            consider(raySegmentDistance(ox,oy,oz,dx,dy,dz,head,tail,pickRadius),
                {kind='segment',boneIndex=index,boneId=bone.boneId,boneName=bone.name},
                'segment:'..tostring(bone.boneId))
        end
    end
    table.sort(candidates,function(a,b)
        if math.abs(a.distance-b.distance)>1e-6 then return a.distance<b.distance end
        return a.key<b.key
    end)
    if #candidates==0 then state.boneEditorPendingCycle=nil return nil end
    local nearestDistance=candidates[1].distance
    for index=#candidates,2,-1 do
        if candidates[index].distance-nearestDistance>pickRadius*2 then table.remove(candidates,index) end
    end
    local currentIndex=nil
    local currentKey=state.boneEditorSelection and state.boneEditorSelection.pickKey
    if currentKey then
        for index,candidate in ipairs(candidates) do
            if candidate.key==currentKey then currentIndex=index break end
        end
    end
    local candidateIndex=currentIndex or 1
    state.boneEditorPendingCycle={candidates=candidates,currentIndex=candidateIndex,
        cycleOnRelease=currentIndex~=nil,x=sx,y=sy}
    return candidates[candidateIndex].selection
end

local function applyBoneEditorToolIntent(selection)
    if not selection or state.boneEditorSegmentTool~=2 or selection.kind=='segment' then
        return selection
    end
    local bone=getBones()[selection.boneIndex]
    if not bone or not bone.hasExplicitTail then return selection end
    -- In Rotate mode a joint hit identifies the segment that owns that joint.
    -- Keep the original pick key so overlapping-joint cycling remains stable.
    return {kind='segment',boneIndex=selection.boneIndex,boneId=bone.boneId,
        boneName=bone.name,pickKey=selection.pickKey}
end

local function hitTestTranslationAxis(sx,sy)
    local gizmo=state.translationGizmo
    if state.workspace~='animation' or state.animationTransformTool~=1 or
            not gizmo.origin or not gizmo.length then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(extent*0.007,0.001)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs({x={x=1,y=0,z=0},y={x=0,y=1,z=0},z={x=0,y=0,z=1}}) do
        local viewAlignment=math.abs(dx*axis.x+dy*axis.y+dz*axis.z)
        if viewAlignment<0.95 then
        local startPoint={x=gizmo.origin.x+axis.x*gizmo.length*0.35,
            y=gizmo.origin.y+axis.y*gizmo.length*0.35,
            z=gizmo.origin.z+axis.z*gizmo.length*0.35}
        local endpoint={x=gizmo.origin.x+axis.x*gizmo.length,
            y=gizmo.origin.y+axis.y*gizmo.length,z=gizmo.origin.z+axis.z*gizmo.length}
        local distance=raySegmentDistance(ox,oy,oz,dx,dy,dz,startPoint,endpoint,radius)
        if distance and distance<bestDistance then best,bestDistance=name,distance end
        end
    end
    return best
end

local function hitTestRotationRing(sx,sy)
    local gizmo=state.rotationGizmo
    if state.workspace~='animation' or state.animationTransformTool~=2 or
            not gizmo.origin or not gizmo.radius or not gizmo.axes then return nil end
    local tolerance=math.max(gizmo.radius*0.12,0.003)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs(gizmo.axes) do
        local wx,wy,wz=rayPlaneHit(sx,sy,gizmo.origin,axis)
        if wx then
            local vx,vy,vz=wx-gizmo.origin.x,wy-gizmo.origin.y,wz-gizmo.origin.z
            local radial=math.sqrt(vx*vx+vy*vy+vz*vz)
            local error=math.abs(radial-gizmo.radius)
            if error<=tolerance and error<bestDistance then
                local direction=normalizedDirection(vx,vy,vz)
                if direction then best={name=name,axis=axis,direction=direction}; bestDistance=error end
            end
        end
    end
    return best
end

local function hitTestScaleAxis(sx,sy)
    local gizmo=state.scaleGizmo
    if state.workspace~='animation' or state.animationTransformTool~=3 or
            not gizmo.origin or not gizmo.length or not gizmo.axesLocal then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(extent*0.01,0.0015)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs(gizmo.axesLocal) do
        if math.abs(dx*axis.x+dy*axis.y+dz*axis.z)<0.95 then
            local startPoint={x=gizmo.origin.x+axis.x*gizmo.length*0.3,
                y=gizmo.origin.y+axis.y*gizmo.length*0.3,
                z=gizmo.origin.z+axis.z*gizmo.length*0.3}
            local endpoint={x=gizmo.origin.x+axis.x*gizmo.length,
                y=gizmo.origin.y+axis.y*gizmo.length,
                z=gizmo.origin.z+axis.z*gizmo.length}
            local distance=raySegmentDistance(ox,oy,oz,dx,dy,dz,startPoint,endpoint,radius)
            if distance and distance<bestDistance then best,bestDistance=name,distance end
        end
    end
    return best
end

local function quaternionMultiply(a,b)
    return {x=a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        y=a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        z=a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        w=a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}
end

local function normalizedQuaternion(q)
    local length=math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w)
    if length<=1e-8 then return nil end
    return {x=q.x/length,y=q.y/length,z=q.z/length,w=q.w/length}
end

local function hitTestBoneEditorAxis(sx,sy)
    local gizmo=state.boneEditorAxisGizmo
    if state.workspace~='bone_editor' or not gizmo.origin or not gizmo.length then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(extent*0.014,0.002)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs({x={x=1,y=0,z=0},y={x=0,y=1,z=0},z={x=0,y=0,z=1}}) do
        local endpoint={x=gizmo.origin.x+axis.x*gizmo.length,
            y=gizmo.origin.y+axis.y*gizmo.length,z=gizmo.origin.z+axis.z*gizmo.length}
        local distance=raySegmentDistance(ox,oy,oz,dx,dy,dz,gizmo.origin,endpoint,radius)
        if distance and distance<bestDistance then best,bestDistance=name,distance end
    end
    return best
end

local function rayAxisParameter(sx,sy,origin,axis)
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local wx,wy,wz=ox-origin.x,oy-origin.y,oz-origin.z
    local parallel=dx*axis.x+dy*axis.y+dz*axis.z
    local rayProjection=dx*wx+dy*wy+dz*wz
    local axisProjection=axis.x*wx+axis.y*wy+axis.z*wz
    local denominator=1-parallel*parallel
    if math.abs(denominator)<0.05 then return nil end
    return (axisProjection-parallel*rayProjection)/denominator
end

local function worldDeltaToLocal(dx,dy,dz,parentMatrix)
    if not parentMatrix then return dx,dy,dz end
    local a,b,c=parentMatrix[1],parentMatrix[2],parentMatrix[3]
    local d,e,f=parentMatrix[5],parentMatrix[6],parentMatrix[7]
    local g,h,i=parentMatrix[9],parentMatrix[10],parentMatrix[11]
    local determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g)
    if math.abs(determinant)<1e-10 then return nil end
    local inv={
        (e*i-f*h)/determinant,(c*h-b*i)/determinant,(b*f-c*e)/determinant,
        (f*g-d*i)/determinant,(a*i-c*g)/determinant,(c*d-a*f)/determinant,
        (d*h-e*g)/determinant,(b*g-a*h)/determinant,(a*e-b*d)/determinant,
    }
    return dx*inv[1]+dy*inv[4]+dz*inv[7],
        dx*inv[2]+dy*inv[5]+dz*inv[8],dx*inv[3]+dy*inv[6]+dz*inv[9]
end

local function hitTestSkeletonBone(sx,sy)
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local bones=getVisualBones()
    local byName={}
    for _,bone in ipairs(bones) do byName[bone.name]=bone end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local segmentRadius=math.max(extent*0.012,0.001)
    local bestIndex,bestDistance=nil,math.huge
    for index,bone in ipairs(bones) do
        local sphere=state.skeletonGizmo.spheres[bone.name]
        local radius=segmentRadius
        if sphere then
            local scale=sphere:getScale()
            radius=math.max(radius,scale.x or 0)
        end
        local distance=raySphereDistance(ox,oy,oz,dx,dy,dz,bone.x,bone.y,bone.z,radius)
        local parent=bone.parentName and byName[bone.parentName] or nil
        local segmentDistance=parent and raySegmentDistance(ox,oy,oz,dx,dy,dz,parent,bone,
            segmentRadius) or nil
        if segmentDistance and (not distance or segmentDistance<distance) then distance=segmentDistance end
        if distance and distance<bestDistance then bestIndex,bestDistance=index,distance end
    end
    return bestIndex
end

local function hitTestBindBone(sx,sy)
    if state.workspace~='bind' then return nil end
    return hitTestSkeletonBone(sx,sy)
end

local function hitTestAuthoringBone(sx,sy)
    if state.workspace~='animation' or not state.authoringPose then return nil end
    return hitTestSkeletonBone(sx,sy)
end

local function hitTestPaintBone(sx,sy)
    if state.workspace~='paint' or not state.paint.showSkeleton or
            state.paint.aabbCapture.active then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local bones=getVisualBones()
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(extent*0.012,0.001)
    local bestIndex,bestDistance=nil,math.huge
    for index,bone in ipairs(bones) do
        local distance=raySphereDistance(ox,oy,oz,dx,dy,dz,bone.x,bone.y,bone.z,radius)
        local segmentDistance=nil
        if bone.hasExplicitTail then
            local head,tailPoint=getBoneEditorEndpoints(bone,extent)
            segmentDistance=raySegmentDistance(ox,oy,oz,dx,dy,dz,head,tailPoint,radius)
        end
        if segmentDistance and (not distance or segmentDistance<distance) then distance=segmentDistance end
        if distance and distance<bestDistance then bestIndex,bestDistance=index,distance end
    end
    return bestIndex
end

local function pointInsideAABB(p, b)
    return p.x >= b.minX and p.x <= b.maxX and p.y >= b.minY and p.y <= b.maxY and
           p.z >= b.minZ and p.z <= b.maxZ
end

local vertexMarkerGeneration=0

local function buildVertexMarkers(vertices, r, g, b, extent)
    if #vertices == 0 then return nil end
    local size = math.max(extent*0.006,0.001)
    local halfWidth=size*0.16
    local coords,step={},math.max(1,math.ceil(#vertices/500))
    local function addQuad(a,b,c,d)
        for _,point in ipairs({a,b,c,a,c,d,a,c,b,a,d,c}) do
            appendPoint(coords,point[1],point[2],point[3])
        end
    end
    for i=1,#vertices,step do
        local p=vertices[i].point
        -- Three orthogonal double-sided bars form one camera-independent 3D cross without
        -- line-strip connectors between its arms or neighboring vertices.
        addQuad({p.x-size,p.y-halfWidth,p.z},{p.x+size,p.y-halfWidth,p.z},
            {p.x+size,p.y+halfWidth,p.z},{p.x-size,p.y+halfWidth,p.z})
        addQuad({p.x-halfWidth,p.y-size,p.z},{p.x+halfWidth,p.y-size,p.z},
            {p.x+halfWidth,p.y+size,p.z},{p.x-halfWidth,p.y+size,p.z})
        addQuad({p.x-halfWidth,p.y,p.z-size},{p.x+halfWidth,p.y,p.z-size},
            {p.x+halfWidth,p.y,p.z+size},{p.x-halfWidth,p.y,p.z+size})
    end
    vertexMarkerGeneration=vertexMarkerGeneration+1
    local marks=shape:new('3d',0,0,0)
    if not marks:create(coords,nil,'skeletal_vertex_markers_'..vertexMarkerGeneration) then
        destroyObject(marks)
        return nil
    end
    marks:setColor(r,g,b,0.9); marks:setPos(0,0,0)
    marks.alwaysOnTop=state.markersAlwaysOnTop
    return marks
end

local function rebuildPaintMaskMarkers()
    destroyObject(state.paint.maskMarkers)
    state.paint.maskMarkers=nil
    local cache=state.paint.geometry
    if not cache then return end
    local vertices={}
    for index in pairs(state.paint.maskVertices) do
        if cache.vertices[index] then vertices[#vertices+1]=cache.vertices[index] end
    end
    table.sort(vertices,function(a,b) return a.globalIndex<b.globalIndex end)
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    state.paint.maskMarkers=buildVertexMarkers(vertices,1,0.45,0,extent)
    if state.paint.maskMarkers then
        state.paint.maskMarkers.alwaysRender=true
        state.paint.maskMarkers.alwaysOnTop=true
        state.paint.maskMarkers.alwaysOnTopPriority=0
        state.paint.maskMarkers.visible=state.workspace=='paint' and state.meshVisible
    end
end

local function applyHitSubsetToPaintMask(mode)
    local hit=state.paint.lastSurfaceHit
    local cache=state.paint.geometry
    if not hit or not hit.triangle or not cache then
        setStatus(tLang.L('swl_paint_mask_subset_no_hit'),false)
        return false
    end
    local subset=hit.triangle.subset
    if mode=='replace' then state.paint.maskVertices={} end
    for _,vertex in pairs(cache.vertices) do
        if vertex.subset==subset then
            if mode=='remove' then
                state.paint.maskVertices[vertex.globalIndex]=nil
            else
                state.paint.maskVertices[vertex.globalIndex]=true
            end
        end
    end
    rebuildPaintMaskMarkers()
    rebuildPaintCursor(hit)
    local count=0
    for _ in pairs(state.paint.maskVertices) do count=count+1 end
    setStatus(string.format(tLang.L('swl_paint_mask_subset_applied_fmt'),subset,count),false,true)
    return true
end

local paintAabbCaptureGeneration=0

local function rebuildPaintAabbCaptureBox()
    paintAabbCaptureGeneration=paintAabbCaptureGeneration+1
    destroyObject(state.paint.aabbCaptureBox)
    for _,object in pairs(state.paint.aabbCaptureAxisEdges) do destroyObject(object) end
    for _,object in pairs(state.paint.aabbCaptureMinFaces) do destroyObject(object) end
    for _,object in pairs(state.paint.aabbCaptureMaxFaces) do destroyObject(object) end
    state.paint.aabbCaptureBox=nil
    state.paint.aabbCaptureAxisEdges={}
    state.paint.aabbCaptureMinFaces={}
    state.paint.aabbCaptureMaxFaces={}
    local capture=state.paint.aabbCapture
    if not capture.active or not capture.bounds then return end
    local world=capture.bounds
    local cx,cy,cz=(world.minX+world.maxX)*0.5,(world.minY+world.maxY)*0.5,
        (world.minZ+world.maxZ)*0.5
    local hx,hy,hz=(world.maxX-world.minX)*0.5,(world.maxY-world.minY)*0.5,
        (world.maxZ-world.minZ)*0.5
    local b={minX=-hx,maxX=hx,minY=-hy,maxY=hy,minZ=-hz,maxZ=hz}
    state.paint.aabbCaptureBox=createSelectionBox(b,1,0.65,0)
    if state.paint.aabbCaptureBox then
        state.paint.aabbCaptureBox:setPos(cx,cy,cz)
        state.paint.aabbCaptureBox.alwaysRender=true
        state.paint.aabbCaptureBox.visible=state.workspace=='paint' and
            state.paint.aabbCapture.active
    end
    local corners={
        {b.minX,b.minY,b.maxZ},{b.minX,b.maxY,b.maxZ},
        {b.maxX,b.maxY,b.maxZ},{b.maxX,b.minY,b.maxZ},
        {b.minX,b.minY,b.minZ},{b.minX,b.maxY,b.minZ},
        {b.maxX,b.maxY,b.minZ},{b.maxX,b.minY,b.minZ},
    }
    local colors={x={1,0.1,0.8},y={0.1,1,1},z={0.8,1,0.1}}
    local edges={x={{1,4},{2,3},{5,8},{6,7}},
        y={{1,2},{4,3},{5,6},{8,7}},z={{1,5},{2,6},{3,7},{4,8}}}
    local faces={
        x={min={{5,6,2},{5,2,1}},max={{4,3,7},{4,7,8}}},
        y={min={{5,1,4},{5,4,8}},max={{2,6,7},{2,7,3}}},
        z={min={{5,8,7},{5,7,6}},max={{1,2,3},{1,3,4}}},
    }
    for _,axis in ipairs({'x','y','z'}) do
        local edgeLine=line:new('3d',0,0,0)
        for _,edge in ipairs(edges[axis]) do
            local coords={}
            for _,cornerIndex in ipairs(edge) do
                local p=corners[cornerIndex]
                appendPoint(coords,p[1],p[2],p[3])
            end
            edgeLine:add(coords)
        end
        edgeLine:setColor(colors[axis][1],colors[axis][2],colors[axis][3],1)
        edgeLine:setPos(cx,cy,cz)
        edgeLine.alwaysRender=true; edgeLine.alwaysOnTop=true
        edgeLine.alwaysOnTopPriority=1; edgeLine.visible=false
        state.paint.aabbCaptureAxisEdges[axis]=edgeLine
        for _,kind in ipairs({'min','max'}) do
            local coords={}
            for _,triangle in ipairs(faces[axis][kind]) do
                for _,cornerIndex in ipairs(triangle) do
                    local p=corners[cornerIndex]
                    appendPoint(coords,p[1],p[2],p[3])
                end
                -- Emit the opposite winding as well. This keeps the hover face visible from
                -- either side without relying on backend-specific cull-state overrides.
                for reverseIndex=#triangle,1,-1 do
                    local p=corners[triangle[reverseIndex]]
                    appendPoint(coords,p[1],p[2],p[3])
                end
            end
            local face=shape:new('3d',0,0,0)
            if face:create(coords,nil,'paint_aabb_hover_'..axis..'_'..kind..'_'..
                    paintAabbCaptureGeneration) then
                face:setColor(colors[axis][1],colors[axis][2],colors[axis][3],0.32)
                face:setPos(cx,cy,cz)
                face.alwaysRender=true; face.alwaysOnTop=true
                face.alwaysOnTopPriority=1; face.visible=false
                state.paint[kind=='min' and 'aabbCaptureMinFaces' or
                    'aabbCaptureMaxFaces'][axis]=face
            else
                destroyObject(face)
            end
        end
    end
end

local function movePaintAabbCaptureObjects()
    local b=state.paint.aabbCapture.bounds
    if not b then return end
    local cx,cy,cz=(b.minX+b.maxX)*0.5,(b.minY+b.maxY)*0.5,(b.minZ+b.maxZ)*0.5
    if state.paint.aabbCaptureBox then state.paint.aabbCaptureBox:setPos(cx,cy,cz) end
    for _,objects in ipairs({state.paint.aabbCaptureAxisEdges,
            state.paint.aabbCaptureMinFaces,state.paint.aabbCaptureMaxFaces}) do
        for _,object in pairs(objects) do object:setPos(cx,cy,cz) end
    end
end

local function setPaintAabbCaptureHover(kind,axis)
    for name,object in pairs(state.paint.aabbCaptureAxisEdges) do
        object.visible=axis==name and kind~=nil
    end
    for name,object in pairs(state.paint.aabbCaptureMinFaces) do
        object.visible=axis==name and (kind=='min' or kind=='size')
    end
    for name,object in pairs(state.paint.aabbCaptureMaxFaces) do
        object.visible=axis==name and (kind=='max' or kind=='size')
    end
end

local function setPaintAabbCaptureActive(active)
    local capture=state.paint.aabbCapture
    if active then
        local meshBounds=state.meshBounds
        if not capture.initialized and meshBounds then
            local cx=(meshBounds.minX+meshBounds.maxX)*0.5
            local cy=(meshBounds.minY+meshBounds.maxY)*0.5
            local cz=(meshBounds.minZ+meshBounds.maxZ)*0.5
            local hx=math.max((meshBounds.maxX-meshBounds.minX)*0.125,0.001)
            local hy=math.max((meshBounds.maxY-meshBounds.minY)*0.125,0.001)
            local hz=math.max((meshBounds.maxZ-meshBounds.minZ)*0.125,0.001)
            capture.bounds={minX=cx-hx,maxX=cx+hx,minY=cy-hy,maxY=cy+hy,
                minZ=cz-hz,maxZ=cz+hz}
            capture.initialized=true
        end
        if not capture.sensitivity then
            capture.sensitivity=math.max(meshBounds and math.max(
                meshBounds.maxX-meshBounds.minX,meshBounds.maxY-meshBounds.minY,
                meshBounds.maxZ-meshBounds.minZ)*0.0025 or 0.001,0.0001)
        end
        capture.active=true
        capture.result=nil
        capture.dragPlane,capture.dragOffset=nil,nil
        rebuildPaintCursor(nil)
        rebuildPaintAabbCaptureBox()
    else
        capture.active=false
        capture.dragPlane,capture.dragOffset=nil,nil
        rebuildPaintAabbCaptureBox()
        capture.result={}
        local capturedCount=0
        local cache=state.paint.geometry
        if cache and capture.bounds then
            for _,vertex in pairs(cache.vertices) do
                if pointInsideAABB(vertex.point,capture.bounds) then
                    capture.result[vertex.globalIndex]=true
                    capturedCount=capturedCount+1
                end
            end
        end
        setStatus(string.format(tLang.L('swl_paint_aabb_capture_ready_fmt'),capturedCount),false,true)
    end
    applyWorkspaceVisibility()
end

local function applyPaintAabbCaptureToMask(mode)
    local result=state.paint.aabbCapture.result
    if not result then return false end
    if mode=='replace' then state.paint.maskVertices={} end
    for index in pairs(result) do
        if mode=='remove' then
            state.paint.maskVertices[index]=nil
        else
            state.paint.maskVertices[index]=true
        end
    end
    rebuildPaintMaskMarkers()
    local count=0
    for _ in pairs(state.paint.maskVertices) do count=count+1 end
    setStatus(string.format(tLang.L('swl_paint_aabb_mask_applied_fmt'),count),false,true)
    applyWorkspaceVisibility()
    return true
end

local paintHeatmapShaderName='skeletal_paint_weight_heatmap_scope.ps'
local paintBrushFootprintShaderName='skeletal_paint_brush_footprint.ps'

local function ensurePaintHeatmapShader()
    if mbm.existShader(paintHeatmapShaderName) then return true end
    local code
    if mbm.get('USE_METAL') then
        code=[=[
        fragment float4 frag_main(VOut in [[stage_in]])
        {
            float t=clamp(in.uv.x,0.0f,1.0f);
            if(in.uv.y<0.5f)
                return float4(0.12f,0.13f,0.15f,1.0f);
            float3 c0=float3(0.10f,0.25f,1.00f);
            float3 c1=float3(0.00f,0.85f,1.00f);
            float3 c2=float3(0.10f,1.00f,0.25f);
            float3 c3=float3(1.00f,0.90f,0.00f);
            float3 c4=float3(1.00f,0.45f,0.00f);
            float3 c5=float3(1.00f,0.10f,0.00f);
            float3 color=t<0.2f ? mix(c0,c1,t/0.2f) :
                (t<0.4f ? mix(c1,c2,(t-0.2f)/0.2f) :
                (t<0.6f ? mix(c2,c3,(t-0.4f)/0.2f) :
                (t<0.8f ? mix(c3,c4,(t-0.6f)/0.2f) : mix(c4,c5,(t-0.8f)/0.2f))));
            return float4(color,1.0f);
        }
        ]=]
    elseif mbm.get('USE_DIRECTX9') then
        code=[[
        float3 heatColor(float value)
        {
            float t=saturate(value);
            float3 c0=float3(0.10,0.25,1.00);
            float3 c1=float3(0.00,0.85,1.00);
            float3 c2=float3(0.10,1.00,0.25);
            float3 c3=float3(1.00,0.90,0.00);
            float3 c4=float3(1.00,0.45,0.00);
            float3 c5=float3(1.00,0.10,0.00);
            if(t<0.2) return lerp(c0,c1,t/0.2);
            if(t<0.4) return lerp(c1,c2,(t-0.2)/0.2);
            if(t<0.6) return lerp(c2,c3,(t-0.4)/0.2);
            if(t<0.8) return lerp(c3,c4,(t-0.6)/0.2);
            return lerp(c4,c5,(t-0.8)/0.2);
        }

        float4 main(float2 texCoord : TEXCOORD0) : COLOR0
        {
            if(texCoord.y<0.5)
                return float4(0.12,0.13,0.15,1.0);
            return float4(heatColor(texCoord.x),1.0);
        }
        ]]
    else
        code=[[
        precision mediump float;
        varying vec2 vTexCoord;

        vec3 heatColor(float value)
        {
            float t=clamp(value,0.0,1.0);
            vec3 c0=vec3(0.10,0.25,1.00);
            vec3 c1=vec3(0.00,0.85,1.00);
            vec3 c2=vec3(0.10,1.00,0.25);
            vec3 c3=vec3(1.00,0.90,0.00);
            vec3 c4=vec3(1.00,0.45,0.00);
            vec3 c5=vec3(1.00,0.10,0.00);
            if(t<0.2) return mix(c0,c1,t/0.2);
            if(t<0.4) return mix(c1,c2,(t-0.2)/0.2);
            if(t<0.6) return mix(c2,c3,(t-0.4)/0.2);
            if(t<0.8) return mix(c3,c4,(t-0.6)/0.2);
            return mix(c4,c5,(t-0.8)/0.2);
        }

        void main()
        {
            if(vTexCoord.y<0.5)
                gl_FragColor=vec4(0.12,0.13,0.15,1.0);
            else
                gl_FragColor=vec4(heatColor(vTexCoord.x),1.0);
        }
        ]]
    end
    return mbm.addShader({name=paintHeatmapShaderName,code=code,var={},min={},max={}})
end

local function ensurePaintBrushFootprintShader()
    if mbm.existShader(paintBrushFootprintShaderName) then return true end
    local code
    if mbm.get('USE_METAL') then
        code=[=[
        fragment float4 frag_main(VOut in [[stage_in]])
        {
            float influence=clamp(in.uv.x,0.0f,1.0f);
            if(influence<=0.001f) discard_fragment();
            float3 color=in.uv.y<0.25f ? float3(0.10f,1.00f,0.25f) :
                (in.uv.y<0.75f ? float3(1.00f,0.12f,0.05f) :
                (in.uv.y<1.25f ? float3(0.00f,0.85f,1.00f) : float3(1.00f,0.75f,0.05f)));
            return float4(color,sqrt(influence)*0.65f);
        }
        ]=]
    elseif mbm.get('USE_DIRECTX9') then
        code=[[
        float4 main(float2 texCoord : TEXCOORD0) : COLOR0
        {
            float influence=saturate(texCoord.x);
            if(influence<=0.001) discard;
            float3 color=texCoord.y<0.25 ? float3(0.10,1.00,0.25) :
                (texCoord.y<0.75 ? float3(1.00,0.12,0.05) :
                (texCoord.y<1.25 ? float3(0.00,0.85,1.00) : float3(1.00,0.75,0.05)));
            return float4(color,sqrt(influence)*0.65);
        }
        ]]
    else
        code=[[
        precision mediump float;
        varying vec2 vTexCoord;

        void main()
        {
            float influence=clamp(vTexCoord.x,0.0,1.0);
            if(influence<=0.001) discard;
            vec3 color=vTexCoord.y<0.25 ? vec3(0.10,1.00,0.25) :
                (vTexCoord.y<0.75 ? vec3(1.00,0.12,0.05) :
                (vTexCoord.y<1.25 ? vec3(0.00,0.85,1.00) : vec3(1.00,0.75,0.05)));
            gl_FragColor=vec4(color,sqrt(influence)*0.65);
        }
        ]]
    end
    return mbm.addShader({name=paintBrushFootprintShaderName,code=code,var={},min={},max={}})
end

local function vertexWeightForBone(globalIndex,boneName)
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    if not ok then return 0,{} end
    local influences={{n1,w1},{n2,w2},{n3,w3},{n4,w4}}
    local weight=0
    for _,pair in ipairs(influences) do
        if pair[1]==boneName then weight=weight+(tonumber(pair[2]) or 0) end
    end
    return math.max(0,math.min(1,weight)),influences
end

local function buildPaintGeometryCache()
    if not state.meshD then return nil end
    local okMode,mode=safeCall(function() return state.meshD:getModeDraw() end)
    if not okMode or mode~='TRIANGLES' then return nil end
    local cache={vertices={},triangles={},incidentTriangles={}}
    local function addTriangle(a,b,c,subset)
        local triangle={a=a,b=b,c=c,subset=subset}
        local ap,bp,cp=a.point,b.point,c.point
        triangle.minX=math.min(ap.x,bp.x,cp.x); triangle.maxX=math.max(ap.x,bp.x,cp.x)
        triangle.minY=math.min(ap.y,bp.y,cp.y); triangle.maxY=math.max(ap.y,bp.y,cp.y)
        triangle.minZ=math.min(ap.z,bp.z,cp.z); triangle.maxZ=math.max(ap.z,bp.z,cp.z)
        triangle.cx=(ap.x+bp.x+cp.x)/3; triangle.cy=(ap.y+bp.y+cp.y)/3
        triangle.cz=(ap.z+bp.z+cp.z)/3
        cache.triangles[#cache.triangles+1]=triangle
        for _,vertex in ipairs({a,b,c}) do
            cache.incidentTriangles[vertex.globalIndex]=
                cache.incidentTriangles[vertex.globalIndex] or {}
            cache.incidentTriangles[vertex.globalIndex][#cache.incidentTriangles[vertex.globalIndex]+1]=
                triangle
        end
    end
    local okS,subsets=safeCall(function() return state.meshD:getTotalSubset(1) end)
    if not okS then return nil end
    local offset=0
    for subset=1,subsets do
        local okV,total=safeCall(function() return state.meshD:getTotalVertex(1,subset) end)
        total=okV and total or 0
        local subsetVertices={}
        for vertex=1,total do
            local okP,p=safeCall(function() return state.meshD:getVertex(1,subset,vertex) end)
            if okP and p then
                local entry={globalIndex=offset+vertex,subset=subset,localIndex=vertex,point=p}
                subsetVertices[vertex]=entry
                cache.vertices[offset+vertex]=entry
            end
        end
        local okI,indices=safeCall(function() return state.meshD:getIndex(1,subset) end)
        if okI and indices and #indices>=3 then
            for index=1,#indices-2,3 do
                local a,b,c=subsetVertices[indices[index]],subsetVertices[indices[index+1]],
                    subsetVertices[indices[index+2]]
                if a and b and c then addTriangle(a,b,c,subset) end
            end
        else
            for vertex=1,total-2,3 do
                local a,b,c=subsetVertices[vertex],subsetVertices[vertex+1],subsetVertices[vertex+2]
                if a and b and c then addTriangle(a,b,c,subset) end
            end
        end
        offset=offset+total
    end
    local function buildBvh(triangles)
        if #triangles==0 then return nil end
        local node={minX=math.huge,minY=math.huge,minZ=math.huge,
            maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge}
        for _,triangle in ipairs(triangles) do
            node.minX=math.min(node.minX,triangle.minX); node.maxX=math.max(node.maxX,triangle.maxX)
            node.minY=math.min(node.minY,triangle.minY); node.maxY=math.max(node.maxY,triangle.maxY)
            node.minZ=math.min(node.minZ,triangle.minZ); node.maxZ=math.max(node.maxZ,triangle.maxZ)
        end
        if #triangles<=12 then node.triangles=triangles return node end
        local dx,dy,dz=node.maxX-node.minX,node.maxY-node.minY,node.maxZ-node.minZ
        local axis=dx>=dy and dx>=dz and 'cx' or (dy>=dz and 'cy' or 'cz')
        table.sort(triangles,function(a,b) return a[axis]<b[axis] end)
        local middle=math.floor(#triangles/2)
        local left,right={},{}
        for index,triangle in ipairs(triangles) do
            if index<=middle then left[#left+1]=triangle else right[#right+1]=triangle end
        end
        node.left=buildBvh(left); node.right=buildBvh(right)
        return node
    end
    local function buildVertexBvh(vertices)
        if #vertices==0 then return nil end
        local node={minX=math.huge,minY=math.huge,minZ=math.huge,
            maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge}
        for _,vertex in ipairs(vertices) do
            local p=vertex.point
            node.minX=math.min(node.minX,p.x); node.maxX=math.max(node.maxX,p.x)
            node.minY=math.min(node.minY,p.y); node.maxY=math.max(node.maxY,p.y)
            node.minZ=math.min(node.minZ,p.z); node.maxZ=math.max(node.maxZ,p.z)
        end
        if #vertices<=32 then node.vertices=vertices return node end
        local dx,dy,dz=node.maxX-node.minX,node.maxY-node.minY,node.maxZ-node.minZ
        local axis=dx>=dy and dx>=dz and 'x' or (dy>=dz and 'y' or 'z')
        table.sort(vertices,function(a,b) return a.point[axis]<b.point[axis] end)
        local middle=math.floor(#vertices/2)
        local left,right={},{}
        for index,vertex in ipairs(vertices) do
            if index<=middle then left[#left+1]=vertex else right[#right+1]=vertex end
        end
        node.left=buildVertexBvh(left); node.right=buildVertexBvh(right)
        return node
    end
    cache.bvh=buildBvh(cache.triangles)
    local vertexCopy={}
    for _,vertex in pairs(cache.vertices) do vertexCopy[#vertexCopy+1]=vertex end
    cache.vertexBvh=buildVertexBvh(vertexCopy)
    state.paint.geometry=cache
    return cache
end

local function rayTriangleHit(ox,oy,oz,dx,dy,dz,triangle)
    local a,b,c=triangle.a.point,triangle.b.point,triangle.c.point
    local e1x,e1y,e1z=b.x-a.x,b.y-a.y,b.z-a.z
    local e2x,e2y,e2z=c.x-a.x,c.y-a.y,c.z-a.z
    local px,py,pz=dy*e2z-dz*e2y,dz*e2x-dx*e2z,dx*e2y-dy*e2x
    local determinant=e1x*px+e1y*py+e1z*pz
    if math.abs(determinant)<1e-9 then return nil end
    local inverse=1/determinant
    local tx,ty,tz=ox-a.x,oy-a.y,oz-a.z
    local u=(tx*px+ty*py+tz*pz)*inverse
    if u<0 or u>1 then return nil end
    local qx,qy,qz=ty*e1z-tz*e1y,tz*e1x-tx*e1z,tx*e1y-ty*e1x
    local v=(dx*qx+dy*qy+dz*qz)*inverse
    if v<0 or u+v>1 then return nil end
    local distance=(e2x*qx+e2y*qy+e2z*qz)*inverse
    if distance<=1e-7 then return nil end
    local nx=e1y*e2z-e1z*e2y
    local ny=e1z*e2x-e1x*e2z
    local nz=e1x*e2y-e1y*e2x
    local length=math.sqrt(nx*nx+ny*ny+nz*nz)
    if length<1e-12 then return nil end
    nx,ny,nz=nx/length,ny/length,nz/length
    if nx*dx+ny*dy+nz*dz>0 then nx,ny,nz=-nx,-ny,-nz end
    return {distance=distance,point={x=ox+dx*distance,y=oy+dy*distance,z=oz+dz*distance},
        normal={x=nx,y=ny,z=nz},triangle=triangle,u=u,v=v,w=1-u-v}
end

local function pickPaintSurface(sx,sy)
    if state.workspace~='paint' or not state.meshVisible then return nil end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,sx,sy)
    if not ok then return nil end
    local function hitsBounds(node,maxDistance)
        local near,far=0,maxDistance or math.huge
        for _,axis in ipairs({{'x',ox,dx},{'y',oy,dy},{'z',oz,dz}}) do
            local name,origin,direction=axis[1],axis[2],axis[3]
            local minimum=node['min'..name:upper()]
            local maximum=node['max'..name:upper()]
            if math.abs(direction)<1e-12 then
                if origin<minimum or origin>maximum then return false end
            else
                local a,b=(minimum-origin)/direction,(maximum-origin)/direction
                if a>b then a,b=b,a end
                near,far=math.max(near,a),math.min(far,b)
                if near>far then return false end
            end
        end
        return true
    end
    local nearest=nil
    local function visit(node)
        if not node or not hitsBounds(node,nearest and nearest.distance or math.huge) then return end
        if node.triangles then
            for _,triangle in ipairs(node.triangles) do
                local hit=rayTriangleHit(ox,oy,oz,dx,dy,dz,triangle)
                if hit and (not nearest or hit.distance<nearest.distance) then nearest=hit end
            end
        else
            visit(node.left); visit(node.right)
        end
    end
    visit(cache.bvh)
    return nearest
end

rebuildPaintCursor=function(hit)
    destroyObject(state.paint.cursor)
    destroyObject(state.paint.brushFootprintShape)
    destroyObject(state.paint.brushFootprintMarkers)
    if not state.paint.inspectorPinned then destroyObject(state.paint.hoveredVertexMarker) end
    state.paint.cursor=nil
    state.paint.brushFootprintShape=nil
    state.paint.brushFootprintMarkers=nil
    if not state.paint.inspectorPinned then
        state.paint.hoveredVertex=nil
        state.paint.hoveredVertexMarker=nil
    end
    state.paint.cursorHit=hit
    if hit then state.paint.lastSurfaceHit=hit end
    if not hit or state.workspace~='paint' then return end
    local n=hit.normal
    local rx,ry,rz=math.abs(n.y)<0.9 and 0 or 1,math.abs(n.y)<0.9 and 1 or 0,0
    local tx,ty,tz=ry*n.z-rz*n.y,rz*n.x-rx*n.z,rx*n.y-ry*n.x
    local tangentLength=math.sqrt(tx*tx+ty*ty+tz*tz)
    if tangentLength<1e-9 then return end
    tx,ty,tz=tx/tangentLength,ty/tangentLength,tz/tangentLength
    local bx,by,bz=n.y*tz-n.z*ty,n.z*tx-n.x*tz,n.x*ty-n.y*tx
    local radius=state.paint.radius
    local surfaceOffset=radius*0.004
    local coords,segments={},48
    for segment=0,segments-1 do
        local a0=segment*math.pi*2/segments
        local a1=(segment+1)*math.pi*2/segments
        local function point(angle)
            local cosine,sine=math.cos(angle)*radius,math.sin(angle)*radius
            return hit.point.x+tx*cosine+bx*sine+n.x*surfaceOffset,
                hit.point.y+ty*cosine+by*sine+n.y*surfaceOffset,
                hit.point.z+tz*cosine+bz*sine+n.z*surfaceOffset
        end
        appendPoint(coords,point(a0)); appendPoint(coords,point(a1))
    end
    if state.paint.visualizationMode==1 then
        local cursor=line:new('3d',0,0,0)
        cursor:add(coords); cursor:setColor(1,1,1,1); cursor:setPos(0,0,0)
        cursor.alwaysOnTop=true
        cursor.alwaysOnTopPriority=0
        state.paint.cursor=cursor
    end
    if state.showAdvancedDiagnostics and state.paint.showVertexInspector and
            not state.paint.inspectorPinned then
        local nearest,nearestDistance=nil,math.huge
        for _,vertex in ipairs({hit.triangle.a,hit.triangle.b,hit.triangle.c}) do
            local dx,dy,dz=vertex.point.x-hit.point.x,vertex.point.y-hit.point.y,
                vertex.point.z-hit.point.z
            local distance=dx*dx+dy*dy+dz*dz
            if not nearest or distance<nearestDistance or
                    (distance==nearestDistance and vertex.globalIndex<nearest.globalIndex) then
                nearest,nearestDistance=vertex,distance
            end
        end
        if nearest then
            local influences={}
            for name,weight in pairs(readInfluenceMap(nearest.globalIndex,false)) do
                influences[#influences+1]={name=name,weight=weight}
            end
            table.sort(influences,function(a,b)
                if a.weight==b.weight then return a.name<b.name end
                return a.weight>b.weight
            end)
            state.paint.hoveredVertex={globalIndex=nearest.globalIndex,subset=nearest.subset,
                point=nearest.point,influences=influences}
            local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
                state.meshBounds.maxY-state.meshBounds.minY,
                state.meshBounds.maxZ-state.meshBounds.minZ) or 1
            local markerSize=math.max(extent*0.008,0.001)
            local px=nearest.point.x+n.x*surfaceOffset
            local py=nearest.point.y+n.y*surfaceOffset
            local pz=nearest.point.z+n.z*surfaceOffset
            local markerCoords={}
            appendPoint(markerCoords,px+tx*markerSize,py+ty*markerSize,pz+tz*markerSize)
            appendPoint(markerCoords,px-tx*markerSize,py-ty*markerSize,pz-tz*markerSize)
            appendPoint(markerCoords,px,py,pz)
            appendPoint(markerCoords,px-bx*markerSize,py-by*markerSize,pz-bz*markerSize)
            appendPoint(markerCoords,px+bx*markerSize,py+by*markerSize,pz+bz*markerSize)
            state.paint.hoveredVertexMarker=line:new('3d',0,0,0)
            state.paint.hoveredVertexMarker:add(markerCoords)
            state.paint.hoveredVertexMarker:setColor(1,1,0,1)
            state.paint.hoveredVertexMarker:setPos(0,0,0)
            if state.paint.hoveredVertexMarker then
                state.paint.hoveredVertexMarker.alwaysOnTop=true
                state.paint.hoveredVertexMarker.alwaysOnTopPriority=0
                state.paint.hoveredVertexMarker.visible=state.meshVisible
            end
        end
    end
    if state.paint.visualizationMode~=1 then return end
    if state.paint.showBrushGradient then
        local faceVertices,uvs={},{}
        local operation=(state.paint.operationMode-1)*0.5
        local rings=10
        local function influenceAt(fraction)
            local falloff
            if state.paint.operationMode==4 then
                local core=math.max(0,math.min(0.95,state.paint.rigidCoreRatio))
                if fraction<=core then return 1 end
                falloff=1-(fraction-core)/math.max(1-core,1e-9)
            else
                falloff=1-fraction
            end
            falloff=math.max(0,math.min(1,falloff))
            if state.paint.falloffMode==2 then falloff=falloff*falloff*(3-2*falloff) end
            return state.paint.operationMode==4 and falloff or state.paint.strength*falloff
        end
        local function addDiscVertex(fraction,segment)
            local angle=segment*math.pi*2/segments
            local distance=radius*fraction
            local cosine,sine=math.cos(angle)*distance,math.sin(angle)*distance
            appendPoint(faceVertices,
                hit.point.x+tx*cosine+bx*sine+n.x*surfaceOffset,
                hit.point.y+ty*cosine+by*sine+n.y*surfaceOffset,
                hit.point.z+tz*cosine+bz*sine+n.z*surfaceOffset)
            uvs[#uvs+1]=influenceAt(fraction)
            uvs[#uvs+1]=operation
        end
        for ring=1,rings do
            local inner=(ring-1)/rings
            local outer=ring/rings
            for segment=0,segments-1 do
                local nextSegment=segment+1
                if ring==1 then
                    addDiscVertex(0,0)
                    addDiscVertex(outer,segment)
                    addDiscVertex(outer,nextSegment)
                else
                    addDiscVertex(inner,segment)
                    addDiscVertex(outer,segment)
                    addDiscVertex(outer,nextSegment)
                    addDiscVertex(inner,segment)
                    addDiscVertex(outer,nextSegment)
                    addDiscVertex(inner,nextSegment)
                end
            end
        end
        if #faceVertices>0 and ensurePaintBrushFootprintShader() then
            state.paint.brushFootprintGeneration=state.paint.brushFootprintGeneration+1
            local overlay=shape:new('3d',0,0,0)
            local nickname='paint_brush_footprint_'..state.paint.brushFootprintGeneration
            local created=overlay:create(faceVertices,uvs,nickname)
            local okShader,shader=pcall(function() return overlay:getShader() end)
            if created and okShader and shader and
                    shader:load(paintBrushFootprintShaderName,nil) then
                overlay:setPos(0,0,0); overlay.alwaysOnTop=true
                overlay.alwaysOnTopPriority=0
                overlay.visible=state.meshVisible
                state.paint.brushFootprintShape=overlay
            else
                destroyObject(overlay)
            end
        end
    end
    if state.paint.showBrushFootprint then
        local stroke=state.paint.stroke
        local lockedSubset=stroke and stroke.requiredSubset or nil
        local outsideLockedSubset=lockedSubset and hit.triangle.subset~=lockedSubset
        local requiredSubset=lockedSubset or
            (state.paint.restrictToHitSubset and hit.triangle.subset or nil)
        local candidates=outsideLockedSubset and {} or
            queryPaintVertices(hit.point,state.paint.radius,hit.triangle,requiredSubset)
        if state.paint.maskEditMode==0 and state.paint.maskRestrictBrush then
            local filtered={}
            for _,candidate in ipairs(candidates) do
                if state.paint.maskVertices[candidate.vertex.globalIndex] then
                    filtered[#filtered+1]=candidate
                end
            end
            candidates=filtered
        end
        local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
            state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
        local vertices={}
        for _,candidate in ipairs(candidates) do vertices[#vertices+1]=candidate.vertex end
        local markerVertices={}
        local markerSize=math.max(extent*0.006,0.001)
        local markerHalfWidth=markerSize*0.16
        local step=math.max(1,math.ceil(#vertices/500))
        local function addQuad(a,b,c,d)
            for _,point in ipairs({a,b,c,a,c,d,a,c,b,a,d,c}) do
                appendPoint(markerVertices,point.x,point.y,point.z)
            end
        end
        local function markerPoint(point,tangentScale,bitangentScale)
            return {x=point.x+tx*tangentScale+bx*bitangentScale+n.x*surfaceOffset,
                y=point.y+ty*tangentScale+by*bitangentScale+n.y*surfaceOffset,
                z=point.z+tz*tangentScale+bz*bitangentScale+n.z*surfaceOffset}
        end
        for index=1,#vertices,step do
            local point=vertices[index].point
            addQuad(markerPoint(point,-markerSize,-markerHalfWidth),
                markerPoint(point,markerSize,-markerHalfWidth),
                markerPoint(point,markerSize,markerHalfWidth),
                markerPoint(point,-markerSize,markerHalfWidth))
            addQuad(markerPoint(point,-markerHalfWidth,-markerSize),
                markerPoint(point,markerHalfWidth,-markerSize),
                markerPoint(point,markerHalfWidth,markerSize),
                markerPoint(point,-markerHalfWidth,markerSize))
        end
        if #markerVertices>0 then
            state.paint.brushFootprintGeneration=state.paint.brushFootprintGeneration+1
            local markers=shape:new('3d',0,0,0)
            if markers:create(markerVertices,nil,'paint_affected_vertices_'..
                    state.paint.brushFootprintGeneration) then
                markers:setColor(1,1,1,0.9)
                markers:setPos(0,0,0)
                markers.alwaysOnTop=true
                markers.alwaysOnTopPriority=0
                markers.visible=state.meshVisible
                state.paint.brushFootprintMarkers=markers
            else
                destroyObject(markers)
            end
        end
    end
end

local function updatePaintCursorHover()
    if state.workspace~='paint' or not state.paint.cursorPendingX then return end
    if state.paint.aabbCapture.active then
        state.paint.cursorPendingX,state.paint.cursorPendingY=nil,nil
        if state.paint.cursor then rebuildPaintCursor(nil) end
        return
    end
    if state.paint.visualizationMode~=1 and
            (not state.showAdvancedDiagnostics or not state.paint.showVertexInspector) then
        state.paint.cursorPendingX,state.paint.cursorPendingY=nil,nil
        if state.paint.cursor then rebuildPaintCursor(nil) end
        return
    end
    local now=mbm.getTimeRun()
    if now-state.paint.cursorLastUpdate<1/30 then return end
    local x,y=state.paint.cursorPendingX,state.paint.cursorPendingY
    state.paint.cursorPendingX,state.paint.cursorPendingY=nil,nil
    state.paint.cursorLastX,state.paint.cursorLastY=x,y
    state.paint.cursorLastUpdate=now
    rebuildPaintCursor(pickPaintSurface(x,y))
end

rebuildPaintHeatmap = function()
    for _,object in ipairs(state.paint.heatmapLines) do destroyObject(object) end
    state.paint.heatmapLines={}
    state.paint.heatmapDirty=false
    state.paint.heatmapIndexed=false
    state.paint.distributionStats=nil
    state.paint.weakStats=nil
    state.paint.abruptStats=nil
    if not state.meshD then return end
    local bones=getBones()
    local bone=bones[state.paint.boneIndex]
    if not bone then return end
    state.paint.boneId=bone.boneId
    state.boneIndex=state.paint.boneIndex
    -- This read-only view owns no heatmap geometry. Workspace visibility restores the ordinary
    -- textured preview underneath the already-cached persistent mask markers.
    if state.paint.visualizationMode==5 then return end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return end
    local weights={}
    local scopedDiagnostics=state.paint.diagnosticsUseMask and
        state.paint.visualizationMode~=1
    local function inDiagnosticScope(index)
        return not scopedDiagnostics or state.paint.maskVertices[index]==true
    end
    local scopedTotal=0
    for _,vertex in pairs(cache.vertices) do
        if inDiagnosticScope(vertex.globalIndex) then scopedTotal=scopedTotal+1 end
    end
    local distributionTotal,distributionMin,distributionMax=0,math.huge,0
    local distributionCounts,globalDistributionCounts={0,0,0,0},{0,0,0,0}
    local weakVertices,weakInfluences,weakTotal,weakMaximum=0,0,0,0
    local abruptValues,abruptEdges,abruptVertices,abruptMaximum,abruptRecords=nil,0,{},0,{}
    if state.paint.visualizationMode==4 then
        abruptValues={}
        local adjacency=buildTopologyAdjacency()
        local maps={}
        for _,vertex in pairs(cache.vertices) do
            maps[vertex.globalIndex]=readInfluenceMap(vertex.globalIndex,false)
        end
        local function distanceBetween(a,b)
            local names={}
            for name in pairs(a) do names[name]=true end
            for name in pairs(b) do names[name]=true end
            local total=0
            for name in pairs(names) do
                total=total+math.abs((a[name] or 0)-(b[name] or 0))
            end
            return math.min(1,total*0.5)
        end
        for index,neighbors in pairs(adjacency) do
            abruptValues[index]=abruptValues[index] or 0
            for neighbor in pairs(neighbors) do
                if index<neighbor and inDiagnosticScope(index) and
                        inDiagnosticScope(neighbor) then
                    local distance=distanceBetween(maps[index] or {},maps[neighbor] or {})
                    abruptValues[index]=math.max(abruptValues[index],distance)
                    abruptValues[neighbor]=math.max(abruptValues[neighbor] or 0,distance)
                    abruptMaximum=math.max(abruptMaximum,distance)
                    abruptRecords[#abruptRecords+1]={a=index,b=neighbor,distance=distance}
                    if distance>=state.paint.abruptThreshold then
                        abruptEdges=abruptEdges+1
                        abruptVertices[index]=true; abruptVertices[neighbor]=true
                    end
                end
            end
        end
    end
    for _,vertex in pairs(cache.vertices) do
        local inScope=inDiagnosticScope(vertex.globalIndex)
        local distributionDominant,distributionCount=0,0
        if state.paint.visualizationMode==2 then
            local _,influences=vertexWeightForBone(vertex.globalIndex,'')
            for _,pair in ipairs(influences) do
                local name,weight=pair[1],tonumber(pair[2]) or 0
                if name and weight>0 then
                    distributionDominant=math.max(distributionDominant,weight)
                    distributionCount=distributionCount+1
                end
            end
            if distributionCount>=1 and distributionCount<=4 then
                globalDistributionCounts[distributionCount]=
                    globalDistributionCounts[distributionCount]+1
            end
        end
        if not inScope then
            weights[vertex.globalIndex]=0
        elseif state.paint.visualizationMode==2 then
            weights[vertex.globalIndex]=math.max(0,math.min(1,
                (distributionDominant-0.25)/0.75))
            distributionTotal=distributionTotal+distributionDominant
            distributionMin=math.min(distributionMin,distributionDominant)
            distributionMax=math.max(distributionMax,distributionDominant)
            if distributionCount>=1 and distributionCount<=4 then
                distributionCounts[distributionCount]=distributionCounts[distributionCount]+1
            end
        elseif state.paint.visualizationMode==3 then
            local _,influences=vertexWeightForBone(vertex.globalIndex,'')
            local weakSum,weakCount=0,0
            for _,pair in ipairs(influences) do
                local name,weight=pair[1],tonumber(pair[2]) or 0
                if name and weight>0 and weight<state.paint.cleanThreshold then
                    weakSum=weakSum+weight
                    weakCount=weakCount+1
                    weakMaximum=math.max(weakMaximum,weight)
                end
            end
            weights[vertex.globalIndex]=math.max(0,math.min(1,
                weakSum/math.max(state.paint.cleanThreshold,1e-9)))
            if weakCount>0 then weakVertices=weakVertices+1 end
            weakInfluences=weakInfluences+weakCount
            weakTotal=weakTotal+weakSum
        elseif state.paint.visualizationMode==4 then
            weights[vertex.globalIndex]=abruptValues[vertex.globalIndex] or 0
        else
            weights[vertex.globalIndex]=vertexWeightForBone(vertex.globalIndex,bone.name)
        end
    end
    if state.paint.visualizationMode==2 then
        local total=scopedTotal
        state.paint.distributionStats={minimum=total>0 and distributionMin or 0,
            maximum=distributionMax,average=total>0 and distributionTotal/total or 0,
            counts=distributionCounts,globalCounts=globalDistributionCounts,total=total}
    elseif state.paint.visualizationMode==3 then
        state.paint.weakStats={vertices=weakVertices,influences=weakInfluences,
            totalWeight=weakTotal,maximumWeight=weakMaximum,total=scopedTotal}
    elseif state.paint.visualizationMode==4 then
        local affected=0
        for _ in pairs(abruptVertices) do affected=affected+1 end
        state.paint.abruptStats={edges=abruptEdges,vertices=affected,maximum=abruptMaximum,
            total=scopedTotal,records=abruptRecords}
    end
    local vertices,uvs,indices={},{},{}
    local useIndexed=#cache.vertices<=65535
    if useIndexed then
        for index=1,#cache.vertices do
            local entry=cache.vertices[index]
            appendPoint(vertices,entry.point.x,entry.point.y,entry.point.z)
            uvs[#uvs+1]=math.max(0,math.min(1,weights[entry.globalIndex] or 0))
            uvs[#uvs+1]=inDiagnosticScope(entry.globalIndex) and 1 or 0
        end
        for _,triangle in ipairs(cache.triangles) do
            indices[#indices+1]=triangle.a.globalIndex
            indices[#indices+1]=triangle.b.globalIndex
            indices[#indices+1]=triangle.c.globalIndex
        end
    else
        for _,triangle in ipairs(cache.triangles) do
            for _,entry in ipairs({triangle.a,triangle.b,triangle.c}) do
                appendPoint(vertices,entry.point.x,entry.point.y,entry.point.z)
                uvs[#uvs+1]=math.max(0,math.min(1,weights[entry.globalIndex] or 0))
                uvs[#uvs+1]=inDiagnosticScope(entry.globalIndex) and 1 or 0
            end
        end
    end
    state.paint.heatmapGeneration=state.paint.heatmapGeneration+1
    if #vertices>0 and ensurePaintHeatmapShader() then
        local marker=shape:new('3d',0,0,0)
        local nickname='paint_weight_surface_'..state.paint.heatmapGeneration
        local created=useIndexed and marker:createIndexed(vertices,indices,uvs) or
            marker:create(vertices,uvs,nickname)
        local okShader,shader=pcall(function() return marker:getShader() end)
        if created and okShader and shader and shader:load(paintHeatmapShaderName,nil) then
            marker:setPos(0,0,0)
            marker.alwaysOnTop=false
            marker.visible=state.workspace=='paint'
            state.paint.heatmapLines[1]=marker
            state.paint.heatmapIndexed=useIndexed
        else
            destroyObject(marker)
            setStatus(tLang.L('swl_paint_heatmap_shader_failed'),true)
        end
    end
    applyWorkspaceVisibility()
end

local rebuildRuntimePreviewFromMemory

local function rebuildPreview(sourcePath)
    if sourcePath==nil and state.modified and state.meshD and rebuildRuntimePreviewFromMemory then
        return rebuildRuntimePreviewFromMemory()
    end
    unloadAllRuntimeWearables()
    destroyObject(state.preview)
    destroyObject(state.comparisonPreview)
    state.preview = nil
    state.comparisonPreview = nil
    state.runtimePreviewFromMemory=sourcePath~=nil
    state.runtimePreviewMemoryDirty=false
    local playback=state.skeletalPreview
    playback.clips={}
    playback.selected=1
    playback.duration=0
    playback.playing=false
    playback.paused=false
    playback.comparisonReady=false
    playback.absoluteLayerSelected=1
    playback.absoluteLayerDuration=0
    playback.absoluteLayerActive=false
    playback.absoluteLayerPaused=false
    playback.runtimePose=nil
    sourcePath=sourcePath or state.fileName
    if not sourcePath then return end
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    local separation=extent*0.65
    local function loadRuntimePreview(method,x,executionOverride)
        applyRuntimeLighting()
        local preview=mesh:new('3d')
        if not preview:setSkeletalSkinningMethod(method) then preview:destroy(); return nil end
        local execution=executionOverride or (playback.execution==2 and 'gpu' or playback.execution==3 and 'cpu' or 'auto')
        if not preview:setSkeletalExecutionPath(execution) then preview:destroy(); return nil end
        if not preview:load(sourcePath) then preview:destroy(); return nil end
        preview:setSkeletalAnimationPlaybackSpeed(playback.speed)
        preview:setPos(x,0,0)
        preview.visible=state.meshVisible
        return preview
    end
    if playback.gpuCpuCompare then
        playback.execution=2
    end
    if playback.poseStress then
        playback.execution=2
        state.preview=loadRuntimePreview('lbs',-separation)
        playback.previewX=-separation
        state.comparisonPreview=loadRuntimePreview('dqs',separation)
        playback.comparisonReady=state.preview~=nil and state.comparisonPreview~=nil
    elseif playback.gpuCpuCompare then
        local method=playback.method==1 and 'auto' or playback.method==3 and 'dqs' or 'lbs'
        state.preview=loadRuntimePreview(method,-separation,'gpu')
        playback.previewX=-separation
        local report=state.preview and state.preview:getSkeletalSkinningReport() or nil
        local resolved=report and report.resolvedMethod or method
        state.comparisonPreview=loadRuntimePreview(resolved,separation,'cpu')
        playback.comparisonReady=state.preview~=nil and state.comparisonPreview~=nil
    else
        local method=playback.method==1 and 'auto' or playback.method==3 and 'dqs' or 'lbs'
        state.preview=loadRuntimePreview(method,0)
        playback.previewX=0
    end
    if state.preview then
        local total=state.preview:getTotalSkeletalAnimations()
        for index=1,total do
            playback.clips[index]=state.preview:getSkeletalAnimationName(index) or ('Clip '..index)
        end
    end
    applyWorkspaceVisibility()
end

rebuildRuntimePreviewFromMemory=function()
    if not state.meshD then return false end
    local temporaryPath=getTemporaryMeshPath()
    local okSaved,saved=safeCall(function()
        return state.meshD:save(temporaryPath,false,false)
    end)
    if not okSaved or not saved then
        pcall(os.remove,temporaryPath)
        return false
    end
    local selected=state.skeletalPreview.selected or 1
    rebuildPreview(temporaryPath)
    pcall(os.remove,temporaryPath)
    state.skeletalPreview.selected=math.max(1,
        math.min(selected,#state.skeletalPreview.clips>0 and #state.skeletalPreview.clips or 1))
    return state.preview~=nil
end

local function playSelectedSkeletalClip()
    local playback=state.skeletalPreview
    local name=playback.clips[playback.selected]
    if not state.preview or not name then return end
    if state.preview:playSkeletalAnimation(name) then
        playback.duration=state.preview:getSkeletalAnimationDuration(playback.selected) or 0
        playback.playing=true
        playback.paused=false
        if state.comparisonPreview then
            playback.comparisonReady=state.comparisonPreview:playSkeletalAnimation(name)
            if playback.comparisonReady then state.comparisonPreview:seekSkeletalAnimation(0) end
        end
        rebuildSkeletonVisuals()
        applyWorkspaceVisibility()
    end
end

local function playSelectedAbsoluteLayer()
    local playback=state.skeletalPreview
    local name=playback.clips[playback.absoluteLayerSelected]
    if not state.preview or not playback.playing or not name then return false end
    local function playLayer(preview)
        if playback.absoluteLayerMode==2 then
            return preview:playSkeletalAnimationAdditiveLayer(name,playback.absoluteLayerWeight)
        end
        return preview:playSkeletalAnimationAbsoluteLayer(name,playback.absoluteLayerWeight)
    end
    if not playLayer(state.preview) then
        return false
    end
    local maskEdits={}
    for boneId,weight in pairs(playback.layerMaskWeights) do
        maskEdits[#maskEdits+1]={boneId=boneId,weight=weight}
    end
    if #maskEdits>0 then state.preview:setSkeletalAnimationLayerBoneWeights(maskEdits) end
    playback.absoluteLayerDuration=
        state.preview:getSkeletalAnimationDuration(playback.absoluteLayerSelected) or 0
    playback.absoluteLayerActive=true
    playback.absoluteLayerPaused=false
    if state.comparisonPreview then
        playback.comparisonReady=playLayer(state.comparisonPreview)
        if playback.comparisonReady then
            state.comparisonPreview:seekSkeletalAnimationAbsoluteLayer(0)
            if #maskEdits>0 then
                state.comparisonPreview:setSkeletalAnimationLayerBoneWeights(maskEdits)
            end
        end
    end
    rebuildSkeletonVisuals()
    applyWorkspaceVisibility()
    return true
end

local function applyRuntimeLayerMaskEdits(edits)
    if #edits==0 or not state.skeletalPreview.absoluteLayerActive then return false end
    if not state.preview:setSkeletalAnimationLayerBoneWeights(edits) then return false end
    if state.comparisonPreview then
        state.comparisonPreview:setSkeletalAnimationLayerBoneWeights(edits)
    end
    for _,edit in ipairs(edits) do
        if edit.weight==1 then state.skeletalPreview.layerMaskWeights[edit.boneId]=nil
        else state.skeletalPreview.layerMaskWeights[edit.boneId]=edit.weight end
    end
    applyWorkspaceVisibility()
    return true
end

local function setRuntimeLayerMaskWeight(bone,weight)
    if not bone or not bone.boneId then return false end
    return applyRuntimeLayerMaskEdits({{boneId=bone.boneId,
        weight=math.max(0,math.min(1,weight))}})
end

local function showRuntimeLayerMask()
    local playback=state.skeletalPreview
    local bones=getBones()
    if #bones==0 then return end
    if not tImGui.TreeNode(tLang.L('swl_layer_mask')..'##swlLayerMask') then return end
    tImGui.TextWrapped(tLang.L('swl_layer_mask_help'))
    tImGui.BeginDisabled(not playback.absoluteLayerActive)
    if tImGui.Button(tLang.L('swl_layer_mask_all_zero')) then
        local edits={}
        for _,bone in ipairs(bones) do edits[#edits+1]={boneId=bone.boneId,weight=0} end
        applyRuntimeLayerMaskEdits(edits)
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_layer_mask_all_one')) then
        if state.preview:clearSkeletalAnimationLayerMask() then
            if state.comparisonPreview then state.comparisonPreview:clearSkeletalAnimationLayerMask() end
            playback.layerMaskWeights={}
        end
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_layer_mask_invert')) then
        local edits={}
        for _,bone in ipairs(bones) do
            edits[#edits+1]={boneId=bone.boneId,
                weight=1-(playback.layerMaskWeights[bone.boneId] or 1)}
        end
        applyRuntimeLayerMaskEdits(edits)
    end
    local children,roots={},{}
    for index,bone in ipairs(bones) do
        if bone.parentIndex and bone.parentIndex>0 then
            children[bone.parentIndex]=children[bone.parentIndex] or {}
            children[bone.parentIndex][#children[bone.parentIndex]+1]=index
        else roots[#roots+1]=index end
    end
    local function showBone(index)
        local bone=bones[index]
        local flags=playback.layerMaskSelected==index and
            tImGui.Flags('ImGuiTreeNodeFlags_Selected') or tImGui.Flags('ImGuiTreeNodeFlags_None')
        local open=tImGui.TreeNodeEx(string.format('%s  %.2f##swlMaskBone%d',bone.name,
            playback.layerMaskWeights[bone.boneId] or 1,index),flags)
        if tImGui.IsItemClicked() then
            playback.layerMaskSelected=index
            applyWorkspaceVisibility()
        end
        if open then
            for _,child in ipairs(children[index] or {}) do showBone(child) end
            tImGui.TreePop()
        end
    end
    for _,root in ipairs(roots) do showBone(root) end
    local selected=bones[playback.layerMaskSelected] or bones[1]
    local current=playback.layerMaskWeights[selected.boneId] or 1
    tImGui.PushItemWidth(190)
    local changed,weight=tImGui.SliderFloat(tLang.L('swl_layer_mask_weight'),current,0,1,'%.3f')
    tImGui.PopItemWidth()
    local descendants=tImGui.Checkbox(tLang.L('swl_layer_mask_descendants'),
        playback.layerMaskDescendants)
    playback.layerMaskDescendants=descendants
    if changed then
        local edits={}
        for index,bone in ipairs(bones) do
            local affected=index==playback.layerMaskSelected
            if not affected and descendants then
                local parent=bone.parentIndex
                while parent and parent>0 do
                    if parent==playback.layerMaskSelected then affected=true break end
                    parent=bones[parent] and bones[parent].parentIndex or 0
                end
            end
            if affected then edits[#edits+1]={boneId=bone.boneId,weight=weight} end
        end
        applyRuntimeLayerMaskEdits(edits)
    end
    local function selectedSubtreeEdits(weight)
        local edits={}
        for index,bone in ipairs(bones) do
            local affected=index==playback.layerMaskSelected
            local parent=bone.parentIndex
            while not affected and parent and parent>0 do
                if parent==playback.layerMaskSelected then affected=true break end
                parent=bones[parent] and bones[parent].parentIndex or 0
            end
            if affected then edits[#edits+1]={boneId=bone.boneId,weight=weight} end
        end
        return edits
    end
    if tImGui.Button(tLang.L('swl_layer_mask_subtree_zero')) then
        applyRuntimeLayerMaskEdits(selectedSubtreeEdits(0))
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_layer_mask_subtree_one')) then
        applyRuntimeLayerMaskEdits(selectedSubtreeEdits(1))
    end
    tImGui.EndDisabled()
    tImGui.TreePop()
end

local function stopAbsoluteLayer()
    local playback=state.skeletalPreview
    if not playback.absoluteLayerActive then return true end
    if not state.preview or not state.preview:stopSkeletalAnimationAbsoluteLayer() then return false end
    if state.comparisonPreview then state.comparisonPreview:stopSkeletalAnimationAbsoluteLayer() end
    playback.absoluteLayerActive=false
    playback.absoluteLayerPaused=false
    playback.runtimePose=nil
    rebuildSkeletonVisuals()
    applyWorkspaceVisibility()
    return true
end

function swlHasRuntimeComparison()
    local playback=state.skeletalPreview
    return playback.poseStress or playback.gpuCpuCompare
end

local function syncRuntimeComparisonPreview()
    local playback=state.skeletalPreview
    if state.workspace~='runtime' or not swlHasRuntimeComparison() or not playback.playing or
            not playback.comparisonReady or
            not state.preview or not state.comparisonPreview then return end
    local syncTolerance=0.0001
    local time=state.preview:getSkeletalAnimationTime()
    local comparisonTime=state.comparisonPreview:getSkeletalAnimationTime()
    if time and (not comparisonTime or math.abs(time-comparisonTime)>syncTolerance) then
        state.comparisonPreview:seekSkeletalAnimation(time)
    end
    if playback.absoluteLayerActive then
        local layerTime=state.preview:getSkeletalAnimationAbsoluteLayerTime()
        local comparisonLayerTime=state.comparisonPreview:getSkeletalAnimationAbsoluteLayerTime()
        if layerTime and (not comparisonLayerTime or
                math.abs(layerTime-comparisonLayerTime)>syncTolerance) then
            state.comparisonPreview:seekSkeletalAnimationAbsoluteLayer(layerTime)
        end
    end
end

local function frameRuntimeComparisonLayout()
    if not state.meshBounds then return end
    if not swlHasRuntimeComparison() then frameCamera(state.meshBounds); return end
    local bounds={}
    for key,value in pairs(state.meshBounds) do bounds[key]=value end
    local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
    local separation=extent*0.65
    bounds.minX=bounds.minX-separation
    bounds.maxX=bounds.maxX+separation
    frameCamera(bounds)
end

function showRuntimeWearableControls()
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_wearable_preview'))
    showItemTooltip(tLang.L('swl_wearable_preview_help'))
    if tImGui.Button(tLang.L('swl_wearable_add')..'##swlWearableAdd') then
        local last=state.wearableFollowers[#state.wearableFollowers]
        local path=mbm.openFile((last and last.path) or state.fileName or '', 'msh')
        if path then
            if loadRuntimeWearable(path) then
                setStatus(string.format(tLang.L('swl_wearable_loaded_fmt'),shortName(path)),false)
            else
                local entry=state.wearableFollowers[#state.wearableFollowers]
                setStatus((entry and entry.status) or tLang.L('swl_wearable_unavailable'),true)
            end
        end
    end
    tImGui.SameLine()
    tImGui.BeginDisabled(#state.wearableFollowers==0)
    if tImGui.Button(tLang.L('swl_wearable_remove_all')..'##swlWearableRemoveAll') then
        unloadAllRuntimeWearables()
        applyWorkspaceVisibility()
    end
    tImGui.EndDisabled()
    if #state.wearableFollowers==0 then
        tImGui.TextDisabled(tLang.L('swl_wearable_none'))
        return
    end
    local removeIndex=nil
    for index,entry in ipairs(state.wearableFollowers) do
        tImGui.PushID(index)
        tImGui.Separator()
        tImGui.TextDisabled(string.format(tLang.L('swl_wearable_file_fmt'),
            index,shortName(entry.path)))
        local visible=tImGui.Checkbox(tLang.L('swl_wearable_visible'),entry.visible)
        if visible~=entry.visible then
            entry.visible=visible
            applyWorkspaceVisibility()
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_wearable_remove')..'##swlWearableRemove') then
            removeIndex=index
        end
        if entry.status then
            if entry.preview then
                tImGui.TextWrapped(entry.status)
            else
                tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},entry.status)
            end
        else
            tImGui.TextDisabled(tLang.L('swl_wearable_unavailable'))
        end
        tImGui.PopID()
    end
    if removeIndex then
        unloadRuntimeWearable(removeIndex)
        applyWorkspaceVisibility()
    end
end

local function showSkeletalPreviewControls()
    if tTutorials.consumeFocus('runtime_preview') then tImGui.SetScrollHereY(0.15) end
    local playback=state.skeletalPreview
    local sourceKey=not state.runtimePreviewFromMemory and 'swl_runtime_source_file' or
        state.runtimePreviewMemoryDirty and 'swl_runtime_source_memory_stale' or
        'swl_runtime_source_memory'
    tImGui.TextDisabled(tLang.L(sourceKey))
    if tImGui.Button(tLang.L('swl_runtime_refresh_from_memory')..
            '##swlRuntimeRefreshMemory') then
        if rebuildRuntimePreviewFromMemory() then
            setStatus(tLang.L('swl_runtime_refreshed_from_memory'),false)
        end
    end
    local showSkeleton=tImGui.Checkbox(tLang.L('swl_layer_mask_show_skeleton'),
        playback.layerMaskShowSkeleton)
    if showSkeleton~=playback.layerMaskShowSkeleton then
        playback.layerMaskShowSkeleton=showSkeleton
        rebuildSkeletonVisuals()
        applyWorkspaceVisibility()
    end
    local poseStress=tImGui.Checkbox(tLang.L('swl_pose_stress_compare'),playback.poseStress)
    if poseStress~=playback.poseStress then
        playback.poseStress=poseStress
        if poseStress then
            playback.gpuCpuCompare=false
            playback.execution=2
        end
        if state.runtimePreviewFromMemory then rebuildRuntimePreviewFromMemory()
        else rebuildPreview() end
        frameRuntimeComparisonLayout()
    end
    local gpuCpuCompare=tImGui.Checkbox(tLang.L('swl_gpu_cpu_compare'),playback.gpuCpuCompare)
    if gpuCpuCompare~=playback.gpuCpuCompare then
        playback.gpuCpuCompare=gpuCpuCompare
        if gpuCpuCompare then
            playback.poseStress=false
            playback.execution=2
        end
        if state.runtimePreviewFromMemory then rebuildRuntimePreviewFromMemory()
        else rebuildPreview() end
        frameRuntimeComparisonLayout()
    end
    if playback.poseStress then
        tImGui.TextWrapped(tLang.L('swl_pose_stress_layout'))
    elseif playback.gpuCpuCompare then
        tImGui.TextWrapped(tLang.L('swl_gpu_cpu_layout'))
    end
    local methods={tLang.L('swl_skinning_auto'),tLang.L('swl_skinning_lbs'),tLang.L('swl_skinning_dqs')}
    tImGui.BeginDisabled(swlHasRuntimeComparison())
    tImGui.PushItemWidth(190)
    local methodChanged,method=tImGui.Combo(tLang.L('swl_skinning_method'),playback.method,methods,-1)
    tImGui.PopItemWidth()
    tImGui.EndDisabled()
    if methodChanged then
        playback.method=method
        if state.runtimePreviewFromMemory then rebuildRuntimePreviewFromMemory()
        else rebuildPreview() end
    end
    local lbsReport=nil
    if state.preview then
        lbsReport=state.preview:getSkeletalSkinningReport()
        tImGui.Text(string.format(tLang.L('swl_lbs_report_fmt'),
            (lbsReport.requestedMethod or 'unknown'):upper(),
            (lbsReport.resolvedMethod or 'unknown'):upper()))
        showItemTooltip(string.format(tLang.L('swl_skinning_reason_fmt'),
            lbsReport.resolutionReason or 'unknown'))
        local requiredBoneCount=lbsReport.requiredBoneCount or 0
        local resolvedMethod=lbsReport.resolvedMethod or 'unknown'
        local paletteBytesPerBone=resolvedMethod=='dqs' and 32 or 48
        tImGui.TextWrapped(string.format(tLang.L('swl_palette_usage_fmt'),
            resolvedMethod:upper(),requiredBoneCount,paletteBytesPerBone,
            requiredBoneCount*paletteBytesPerBone))
        showItemTooltip(tLang.L('swl_palette_usage_note'))
    end
    local executions={tLang.L('swl_execution_auto'),tLang.L('swl_execution_gpu'),tLang.L('swl_execution_cpu')}
    tImGui.BeginDisabled(swlHasRuntimeComparison())
    tImGui.PushItemWidth(190)
    local executionChanged,execution=tImGui.Combo(tLang.L('swl_execution_path'),playback.execution,executions,-1)
    tImGui.PopItemWidth()
    tImGui.EndDisabled()
    if executionChanged then
        playback.execution=execution
        if state.runtimePreviewFromMemory then rebuildRuntimePreviewFromMemory()
        else rebuildPreview() end
    end
    if not state.preview then
        tImGui.TextDisabled(tLang.L('swl_runtime_preview_unavailable'))
        return
    end
    local resolvedExecutionPath=lbsReport.resolvedExecutionPath or lbsReport.executionPath or 'gpu'
    drawExecutionStatusIndicator(resolvedExecutionPath)
    tImGui.SameLine()
    tImGui.TextWrapped(string.format(tLang.L('swl_execution_report_fmt'),
        lbsReport.requestedExecutionPath or lbsReport.executionPath or 'gpu',
        resolvedExecutionPath,lbsReport.executionStatus or 'unknown'))
    showItemTooltip(string.format(tLang.L('swl_execution_reason_fmt'),
        lbsReport.executionReason or 'unknown'))
    tImGui.TextWrapped(string.format(tLang.L('swl_lbs_capacity_fmt'),
        lbsReport.status or 'unknown',lbsReport.requiredBoneCount or 0,
        lbsReport.effectiveBoneCapacity or 0))
    showItemTooltip(tLang.L('swl_lbs_capacity_note'))
    local executionPath=resolvedExecutionPath
    if executionPath=='cpu' then
        showItemTooltip(tLang.L('swl_execution_cpu_note'))
    elseif executionPath=='gpu' then
        showItemTooltip(tLang.L('swl_execution_gpu_note'))
    end
    if swlHasRuntimeComparison() and state.comparisonPreview then
        local secondaryReport=state.comparisonPreview:getSkeletalSkinningReport()
        if playback.gpuCpuCompare then
            tImGui.Text(tLang.L('swl_gpu_cpu_primary_label'))
            drawExecutionStatusIndicator(lbsReport.resolvedExecutionPath or lbsReport.executionPath)
            tImGui.SameLine()
            tImGui.TextWrapped(string.format(tLang.L('swl_execution_report_fmt'),
                lbsReport.requestedExecutionPath or lbsReport.executionPath or 'unknown',
                lbsReport.resolvedExecutionPath or lbsReport.executionPath or 'unknown',
                lbsReport.executionStatus or 'unknown'))
            tImGui.Text(tLang.L('swl_gpu_cpu_secondary_label'))
            drawExecutionStatusIndicator(secondaryReport.resolvedExecutionPath or secondaryReport.executionPath)
            tImGui.SameLine()
            tImGui.TextWrapped(string.format(tLang.L('swl_execution_report_fmt'),
                secondaryReport.requestedExecutionPath or secondaryReport.executionPath or 'unknown',
                secondaryReport.resolvedExecutionPath or secondaryReport.executionPath or 'unknown',
                secondaryReport.executionStatus or 'unknown'))
            tImGui.Text(string.format(tLang.L('swl_gpu_cpu_reports'),
                lbsReport.executionStatus or 'unknown',
                secondaryReport.executionStatus or 'unknown'))
        else
            local dqsReport=secondaryReport
            tImGui.Text(string.format(tLang.L('swl_pose_stress_reports'),lbsReport.status or 'unknown',
                dqsReport.status or 'unknown'))
            if playback.playing and not playback.comparisonReady then
                tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_pose_stress_clip_rejected'))
            end
        end
    elseif swlHasRuntimeComparison() then
        tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},
            tLang.L(playback.gpuCpuCompare and 'swl_gpu_cpu_unavailable' or
                'swl_pose_stress_unavailable'))
    end
    showRuntimeWearableControls()
    if #playback.clips==0 then
        tImGui.TextDisabled(tLang.L('swl_no_skeletal_clips'))
        return
    end
    tImGui.PushItemWidth(190)
    local changed,selected=tImGui.Combo(tLang.L('swl_skeletal_clip'),playback.selected,
        playback.clips,-1)
    tImGui.PopItemWidth()
    if changed then
        playback.selected=selected
        playSelectedSkeletalClip()
    end
    if tImGui.Button(tLang.L('swl_play_restart')) then playSelectedSkeletalClip() end
    tImGui.SameLine()
    tImGui.BeginDisabled(not playback.playing)
    if tImGui.Button(tLang.L('swl_restore_bind_pose')) then
        if state.preview:stopSkeletalAnimation() then
            if state.comparisonPreview then state.comparisonPreview:stopSkeletalAnimation() end
            playback.playing=false
            playback.paused=false
            playback.absoluteLayerActive=false
            playback.absoluteLayerPaused=false
            playback.runtimePose=nil
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
        end
    end
    tImGui.EndDisabled()
    showItemTooltip(tLang.L('swl_bind_gizmo_note'),true)
    tImGui.SameLine()
    tImGui.BeginDisabled(not playback.playing)
    if tImGui.Button(playback.paused and tLang.L('swl_resume') or tLang.L('swl_pause')) then
        if playback.paused then
            if state.preview:resumeSkeletalAnimation() then playback.paused=false end
            if state.comparisonPreview then state.comparisonPreview:resumeSkeletalAnimation() end
        elseif state.preview:pauseSkeletalAnimation() then
            playback.paused=true
            if state.comparisonPreview then state.comparisonPreview:pauseSkeletalAnimation() end
        end
    end
    tImGui.EndDisabled()
    tImGui.PushItemWidth(110)
    local speedChanged,speed=tImGui.DragFloat(tLang.L('swl_animation_playback_speed')..
        '##swlRuntimeSpeed',playback.speed,0.05,0.05,4,'%.2fx')
    tImGui.PopItemWidth()
    if speedChanged then
        speed=math.max(0.05,math.min(4,speed))
        if state.preview:setSkeletalAnimationPlaybackSpeed(speed) then
            playback.speed=speed
            if state.comparisonPreview then
                state.comparisonPreview:setSkeletalAnimationPlaybackSpeed(speed)
            end
        end
    end
    showItemTooltip(tLang.L('swl_runtime_speed_help'))
    local time=state.preview:getSkeletalAnimationTime() or 0
    tImGui.PushItemWidth(240)
    local seekChanged,seekTime=tImGui.SliderFloat(tLang.L('swl_preview_time'),time,
        0,math.max(playback.duration,0.0001),'%.3f s')
    tImGui.PopItemWidth()
    if seekChanged and playback.playing then
        state.preview:seekSkeletalAnimation(seekTime)
        if state.comparisonPreview then state.comparisonPreview:seekSkeletalAnimation(seekTime) end
    end
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_absolute_layer'))
    showItemTooltip(tLang.L('swl_absolute_layer_help'))
    local runtimeLayerWeight=state.preview:getSkeletalAnimationAbsoluteLayerWeight()
    if playback.absoluteLayerActive and runtimeLayerWeight==nil then
        playback.absoluteLayerActive=false
        playback.absoluteLayerPaused=false
        playback.runtimePose=nil
        rebuildSkeletonVisuals()
        applyWorkspaceVisibility()
    elseif runtimeLayerWeight~=nil then
        playback.absoluteLayerWeight=runtimeLayerWeight
        playback.absoluteLayerPaused=state.preview:isSkeletalAnimationLayerPaused()
    end
    tImGui.BeginDisabled(not playback.playing)
    local layerModes={tLang.L('swl_layer_mode_absolute'),tLang.L('swl_layer_mode_additive')}
    tImGui.PushItemWidth(190)
    local layerModeChanged,layerMode=tImGui.Combo(tLang.L('swl_layer_mode'),
        playback.absoluteLayerMode,layerModes,-1)
    tImGui.PopItemWidth()
    if layerModeChanged then
        playback.absoluteLayerMode=layerMode
        if playback.absoluteLayerActive then playSelectedAbsoluteLayer() end
    end
    showItemTooltip(tLang.L('swl_layer_mode_help'))
    tImGui.PushItemWidth(190)
    local layerClipChanged,layerClip=tImGui.Combo(tLang.L('swl_absolute_layer_clip'),
        playback.absoluteLayerSelected,playback.clips,-1)
    tImGui.PopItemWidth()
    if layerClipChanged then
        playback.absoluteLayerSelected=layerClip
        if playback.absoluteLayerActive then playSelectedAbsoluteLayer() end
    end
    local layerEnabled=tImGui.Checkbox(tLang.L('swl_absolute_layer_enabled'),
        playback.absoluteLayerActive)
    if layerEnabled~=playback.absoluteLayerActive then
        if layerEnabled then playSelectedAbsoluteLayer() else stopAbsoluteLayer() end
    end
    tImGui.BeginDisabled(not playback.absoluteLayerActive)
    if tImGui.Button(playback.absoluteLayerPaused and
            tLang.L('swl_resume_layer') or tLang.L('swl_pause_layer')) then
        if playback.absoluteLayerPaused then
            if state.preview:resumeSkeletalAnimationLayer() then
                playback.absoluteLayerPaused=false
                if state.comparisonPreview then
                    state.comparisonPreview:resumeSkeletalAnimationLayer()
                end
            end
        elseif state.preview:pauseSkeletalAnimationLayer() then
            playback.absoluteLayerPaused=true
            if state.comparisonPreview then state.comparisonPreview:pauseSkeletalAnimationLayer() end
        end
    end
    showItemTooltip(tLang.L('swl_layer_pause_help'))
    tImGui.PushItemWidth(190)
    local weightChanged,layerWeight=tImGui.SliderFloat(tLang.L('swl_absolute_layer_weight'),
        playback.absoluteLayerWeight,0,1,'%.3f')
    tImGui.PopItemWidth()
    if weightChanged and state.preview:setSkeletalAnimationAbsoluteLayerWeight(layerWeight) then
        playback.absoluteLayerWeight=layerWeight
        if state.comparisonPreview then
            state.comparisonPreview:setSkeletalAnimationAbsoluteLayerWeight(layerWeight)
        end
    end
    local layerTime=state.preview:getSkeletalAnimationAbsoluteLayerTime() or 0
    tImGui.PushItemWidth(240)
    local layerSeekChanged,layerSeek=tImGui.SliderFloat(tLang.L('swl_absolute_layer_time'),
        layerTime,0,math.max(playback.absoluteLayerDuration,0.0001),'%.3f s')
    tImGui.PopItemWidth()
    if layerSeekChanged then
        state.preview:seekSkeletalAnimationAbsoluteLayer(layerSeek)
        if state.comparisonPreview then
            state.comparisonPreview:seekSkeletalAnimationAbsoluteLayer(layerSeek)
        end
    end
    tImGui.PushItemWidth(190)
    local fadeDurationChanged,fadeDuration=tImGui.DragFloat(
        tLang.L('swl_absolute_layer_fade_duration'),playback.absoluteLayerFadeDuration,
        0.01,0,10,'%.3f s')
    tImGui.PopItemWidth()
    if fadeDurationChanged then
        playback.absoluteLayerFadeDuration=math.max(0,math.min(10,fadeDuration))
    end
    if tImGui.Button(tLang.L('swl_absolute_layer_fade_base')) then
        if state.preview:fadeSkeletalAnimationAbsoluteLayer(
                0,playback.absoluteLayerFadeDuration) and state.comparisonPreview then
            state.comparisonPreview:fadeSkeletalAnimationAbsoluteLayer(
                0,playback.absoluteLayerFadeDuration)
        end
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_absolute_layer_fade_layer')) then
        if state.preview:fadeSkeletalAnimationAbsoluteLayer(
                1,playback.absoluteLayerFadeDuration) and state.comparisonPreview then
            state.comparisonPreview:fadeSkeletalAnimationAbsoluteLayer(
                1,playback.absoluteLayerFadeDuration)
        end
    end
    showItemTooltip(tLang.L('swl_absolute_layer_fade_help'))
    showRuntimeLayerMask()
    tImGui.EndDisabled()
    tImGui.EndDisabled()
end

local function loadMesh(path)
    if not path or path == '' then return false end
    local dir = fileDir(path)
    if dir then mbm.addPath(dir) end
    local meshD = meshDebug:new()
    if not meshD:load(path) then
        setStatus(string.format(tLang.L('swl_load_failed_fmt'), path), true)
        return false
    end
    setWorkspace('none')
    clearRollback()
    clearPaintVisuals()
    state.paint.maskVertices={}
    state.paint.maskEditMode=0
    state.paint.maskRestrictBrush=false
    destroySkeletonVisuals()
    state.fileName, state.meshD = path, meshD
    state.meshVisible = true
    state.info = meshDebug:getInfo(path)
    refreshBindReport()
    state.modified = false
    state.bindRenameBoneId=nil
    state.bindReparentBoneId=nil
    state.bindEditBoneId=nil
    state.bindAddBoneId=nil
    state.bindChainBoneId=nil
    state.bindMirrorBoneId=nil
    state.bindWeightsBoneId=nil
    state.bindRemoveBoneId=nil
    state.animationClipSelected=1
    state.animationEditClipId=nil
    state.animationRemoveConfirmed=false
    state.animationTrackEdits={}
    state.animationNewKeyTimes={}
    state.animationKeyEdits={}
    destroyObject(state.animationImport.sourceMesh)
    state.animationImport={open=false,path='',sourceMesh=nil,sourceBind=nil,sourceClips={},
        sourceClip=1,newName='',analysis=nil,error=nil,keyCount=0,confirmed=false}
    state.animationKeyClipboard=nil
    state.animationBonePoseClipboard=nil
    state.animationSkeletonPoseClipboard=nil
    state.boneEditorPosition={x=0,y=0,z=0}
    state.boneEditorLength=10
    state.boneEditorSelectedIndex=nil
    state.boneEditorSelection=nil
    state.boneEditorReorientTailsConfirmed=false
    state.boneEditorInitializeWeightsConfirmed=false
    state.boneEditorInitializeWeightsBoneId=nil
    state.boneEditorAutomaticWeightsConfirmed=false
    state.boneEditorRemoveWeightsConfirmed=false
    state.boneEditorRemoveAllConfirmed=false
    state.armatureTemplateSelected=1
    state.armatureTemplateConfirmed=false
    state.authoringTime=0
    state.authoringPose=nil
    state.authoringPoseKey=nil
    local bounds = computeAABB(meshD)
    local initialExtent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    state.bindInitialName='root'
    state.bindInitialTranslation={x=bounds and (bounds.minX+bounds.maxX)*0.5 or 0,
        y=bounds and bounds.minY or 0,z=bounds and (bounds.minZ+bounds.maxZ)*0.5 or 0}
    state.bindInitialRadius=math.max(initialExtent*0.02,0.001)
    state.bindInitialLength=math.max(initialExtent*0.1,0.001)
    state.boneIndex=1
    state.allowedBones={}
    state.allowedBonesHighlight=false
    state.hoveredAllowedBone=nil
    state.topologyAdjacency=nil
    state.coincidentSeams=nil
    state.paint.geometry=nil
    state.paint.boneIndex=1
    state.paint.boneId=nil
    state.paint.heatmapDirty=true
    for _,bone in ipairs(getBones()) do state.allowedBones[bone.name]=true end
    state.meshBounds = bounds
    state.aabb = bounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    state.paint.radius=math.max(extent*0.05,0.001)
    rebuildPreview()
    rebuildSkeletonVisuals()
    buildPaintGeometryCache()
    if state.workspace=='paint' then rebuildPaintHeatmap() end
    applyWorkspaceVisibility()
    frameCamera(bounds)
    setStatus(string.format(tLang.L('swl_loaded_fmt'), shortName(path)), false)
    return true
end

local function stageRollbackSnapshot(descriptionKey)
    local path = getTemporaryMeshPath()
    if not state.meshD:save(path, false, false) then return false end
    return {path=path,modified=state.modified,workspace=state.workspace,
        boneIndex=state.boneIndex,clipIndex=state.animationClipSelected,
        authoringTime=state.authoringTime,
        descriptionKey=descriptionKey or 'swl_history_operation'}
end

local function trimHistoryStack(stack)
    while #stack>(state.historyLimit or 50) do
        local removed=table.remove(stack,1)
        if removed and removed.path then pcall(os.remove,removed.path) end
    end
end

local function clearHistoryStack(stack)
    for _,entry in ipairs(stack) do
        if entry.path then pcall(os.remove,entry.path) end
    end
    for index=#stack,1,-1 do stack[index]=nil end
end

local function commitRollbackSnapshot(snapshot,descriptionKey)
    snapshot.descriptionKey=descriptionKey or snapshot.descriptionKey or 'swl_history_operation'
    state.undoStack[#state.undoStack+1]=snapshot
    trimHistoryStack(state.undoStack)
    clearHistoryStack(state.redoStack)
    state.animationReport = nil
    state.animationTimelineSelection = {}
    state.paint.heatmapDirty=true
    state.paint.globalSeamAudit=nil
    state.paint.globalSeamSyncConfirmed=false
    state.paint.weightHealthSummary=nil
    state.paint.weightHealthSeamConfirmed=false
    if state.runtimePreviewFromMemory then state.runtimePreviewMemoryDirty=true end
end

local function historyDescription(entry)
    if not entry then return '' end
    return entry.descriptionKey and tLang.L(entry.descriptionKey) or
        entry.description or tLang.L('swl_history_operation')
end

local function discardRollbackSnapshot(snapshot)
    if snapshot and snapshot.path then pcall(os.remove,snapshot.path) end
end

local function commitAuthoringOverride()
    local value=state.authoringOverride
    if not value or not state.authoringActiveClip then return false end
    local snapshot=stageRollbackSnapshot()
    if not snapshot then return false end
    local t,q,s=value.translation,value.rotation,value.scale
    local ok,created=safeCall(function()
        return state.meshD:commitSkeletalAuthoringKey(state.animationClipSelected,
            value.boneIndex,state.authoringTime,value.channelMask or 1,t.x,t.y,t.z,
            q.x,q.y,q.z,q.w,s.x,s.y,s.z)
    end)
    if not ok then discardRollbackSnapshot(snapshot); return false end
    local rotation=value.channelMask==2
    local scaling=value.channelMask==4
    commitRollbackSnapshot(snapshot,scaling and 'swl_history_scale_key' or
        rotation and 'swl_history_rotation_key' or 'swl_history_translation_key')
    state.modified=true
    clearAuthoringOverride()
    refreshBindReport()
    refreshAuthoringPose(state.authoringActiveClip)
    setStatus(tLang.L(scaling and (created and 'swl_animation_scale_key_created' or
        'swl_animation_scale_key_updated') or rotation and
        (created and 'swl_animation_rotation_key_created' or
        'swl_animation_rotation_key_updated') or (created and
        'swl_animation_translation_key_created' or
        'swl_animation_translation_key_updated')),false)
    return true
end

local function cancelBoneEditorDrag()
    local drag=state.boneEditorDrag
    if not drag or not drag.snapshot then return false end
    local restored=meshDebug:new()
    if not restored:load(drag.snapshot.path) then
        setStatus(tLang.L('swl_bone_editor_cancel_failed'),true)
        return false
    end
    state.meshD=restored
    state.modified=drag.snapshot.modified==true
    discardRollbackSnapshot(drag.snapshot)
    state.boneEditorDrag=nil
    state.boneEditorPendingCycle=nil
    refreshBindReport()
    local selectedIndex=nil
    for index,bone in ipairs(getBones()) do
        if bone.boneId==drag.boneId then selectedIndex=index break end
    end
    state.boneIndex=selectedIndex or 1
    state.boneEditorSelectedIndex=selectedIndex
    local bone=selectedIndex and getBones()[selectedIndex] or nil
    state.boneEditorSelection=bone and {kind=drag.mode=='head' and 'head' or
        drag.mode=='tail' and 'tail' or 'segment',boneIndex=selectedIndex,
        boneId=bone.boneId,boneName=bone.name} or nil
    rebuildPreview()
    rebuildSkeletonVisuals()
    applyWorkspaceVisibility()
    setStatus(tLang.L('swl_bone_editor_drag_cancelled'),false)
    return true
end

local function snapshotForRollback(descriptionKey)
    local snapshot=stageRollbackSnapshot(descriptionKey)
    if not snapshot then return false end
    commitRollbackSnapshot(snapshot)
    return true
end

local function blendedInfluences(globalIndex,targetName,alpha)
    if alpha >= 0.999999 then return {{name=targetName,weight=1}} end
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    local byName={}
    if ok then
        for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
            local name,weight=pair[1],tonumber(pair[2]) or 0
            if name and weight>0 and weight==weight and weight<math.huge and
                    (not state.restrictBones or state.allowedBones[name] or name==targetName) then
                byName[name]=(byName[name] or 0)+weight*(1-alpha)
            end
        end
    end
    byName[targetName]=(byName[targetName] or 0)+alpha
    local result={}
    for name,weight in pairs(byName) do
        if weight>0 then result[#result+1]={name=name,weight=weight} end
    end
    table.sort(result,function(a,b)
        if a.weight==b.weight then return a.name<b.name end
        return a.weight>b.weight
    end)
    while #result>4 do table.remove(result) end
    local sum=0
    for _,influence in ipairs(result) do sum=sum+influence.weight end
    if sum<=0 then return {{name=targetName,weight=1}} end
    for _,influence in ipairs(result) do influence.weight=influence.weight/sum end
    return result
end

readInfluenceMap=function(globalIndex,respectRestriction)
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    local result={}
    if not ok then return result end
    for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
        local name,weight=pair[1],tonumber(pair[2]) or 0
        if name and weight>0 and weight==weight and weight<math.huge and
                (respectRestriction==false or not state.restrictBones or state.allowedBones[name]) then
            result[name]=(result[name] or 0)+weight
        end
    end
    return result
end


local function normalizedInfluences(weightMap)
    local result={}
    for name,weight in pairs(weightMap) do
        if weight>0 and weight==weight and weight<math.huge then
            result[#result+1]={name=name,weight=weight}
        end
    end
    table.sort(result,function(a,b)
        if a.weight==b.weight then return a.name<b.name end
        return a.weight>b.weight
    end)
    while #result>4 do table.remove(result) end
    local sum=0
    for _,influence in ipairs(result) do sum=sum+influence.weight end
    if sum<=0 then return {} end
    for _,influence in ipairs(result) do influence.weight=influence.weight/sum end
    return result
end

queryPaintVertices=function(point,radius,seedTriangle,requiredSubset)
    local cache=state.paint.geometry
    if not cache or not cache.vertexBvh then return {} end
    if state.paint.connectedSurfaceOnly and seedTriangle then
        local adjacency=buildTopologyAdjacency()
        local seams=buildCoincidentSeams(adjacency)
        local distances,heap={},{}
        local function push(index,distance)
            heap[#heap+1]={index=index,distance=distance}
            local child=#heap
            while child>1 do
                local parent=math.floor(child/2)
                if heap[parent].distance<=distance then break end
                heap[child]=heap[parent]; child=parent
            end
            heap[child]={index=index,distance=distance}
        end
        local function pop()
            local root=heap[1]
            local last=table.remove(heap)
            if #heap>0 then
                local parent=1
                while parent*2<=#heap do
                    local child=parent*2
                    if child<#heap and heap[child+1].distance<heap[child].distance then
                        child=child+1
                    end
                    if heap[child].distance>=last.distance then break end
                    heap[parent]=heap[child]; parent=child
                end
                heap[parent]=last
            end
            return root
        end
        for _,vertex in ipairs({seedTriangle.a,seedTriangle.b,seedTriangle.c}) do
            local dx,dy,dz=vertex.point.x-point.x,vertex.point.y-point.y,vertex.point.z-point.z
            local distance=math.sqrt(dx*dx+dy*dy+dz*dz)
            if (not requiredSubset or vertex.subset==requiredSubset) and distance<=radius and
                    (not distances[vertex.globalIndex] or
                    distance<distances[vertex.globalIndex]) then
                distances[vertex.globalIndex]=distance
                push(vertex.globalIndex,distance)
            end
        end
        local result={}
        while #heap>0 do
            local item=pop()
            if item.distance==distances[item.index] then
                local vertex=cache.vertices[item.index]
                result[#result+1]={vertex=vertex,distance=item.distance}
                local function relax(neighbor)
                    local target=cache.vertices[neighbor]
                    if target and (not requiredSubset or target.subset==requiredSubset) then
                        local dx=target.point.x-vertex.point.x
                        local dy=target.point.y-vertex.point.y
                        local dz=target.point.z-vertex.point.z
                        local candidate=item.distance+math.sqrt(dx*dx+dy*dy+dz*dz)
                        if candidate<=radius and
                                (not distances[neighbor] or candidate<distances[neighbor]) then
                            distances[neighbor]=candidate
                            push(neighbor,candidate)
                        end
                    end
                end
                for neighbor in pairs(adjacency[item.index] or {}) do relax(neighbor) end
                for _,neighbor in ipairs(seams.byVertex[item.index] or {}) do
                    if neighbor~=item.index then relax(neighbor) end
                end
            end
        end
        return result
    end
    local result={}
    local radiusSquared=radius*radius
    local function boxDistanceSquared(node)
        local dx=point.x<node.minX and node.minX-point.x or
            (point.x>node.maxX and point.x-node.maxX or 0)
        local dy=point.y<node.minY and node.minY-point.y or
            (point.y>node.maxY and point.y-node.maxY or 0)
        local dz=point.z<node.minZ and node.minZ-point.z or
            (point.z>node.maxZ and point.z-node.maxZ or 0)
        return dx*dx+dy*dy+dz*dz
    end
    local function visit(node)
        if not node or boxDistanceSquared(node)>radiusSquared then return end
        if node.vertices then
            for _,vertex in ipairs(node.vertices) do
                local p=vertex.point
                local dx,dy,dz=p.x-point.x,p.y-point.y,p.z-point.z
                local distanceSquared=dx*dx+dy*dy+dz*dz
                if (not requiredSubset or vertex.subset==requiredSubset) and
                        distanceSquared<=radiusSquared then
                    result[#result+1]={vertex=vertex,distance=math.sqrt(distanceSquared)}
                end
            end
        else
            visit(node.left); visit(node.right)
        end
    end
    visit(cache.vertexBvh)
    return result
end

local function paintFalloff(distance,radius)
    local value=math.max(0,math.min(1,1-distance/math.max(radius,1e-9)))
    if state.paint.falloffMode==2 then return value*value*(3-2*value) end
    return value
end

local function stampPaintStroke(stroke,point,triangle)
    for _,candidate in ipairs(queryPaintVertices(point,state.paint.radius,triangle,
            stroke.requiredSubset)) do
        local alpha
        if stroke.operationMode==4 then
            local radius=math.max(state.paint.radius,1e-9)
            local fraction=candidate.distance/radius
            local core=math.max(0,math.min(0.95,stroke.rigidCoreRatio or 0.6))
            if fraction<=core then
                alpha=1
            else
                local transitionDistance=(fraction-core)/math.max(1-core,1e-9)
                alpha=paintFalloff(transitionDistance,1)
            end
        else
            alpha=state.paint.strength*paintFalloff(candidate.distance,state.paint.radius)
        end
        alpha=math.max(0,math.min(1,alpha))
        if alpha>0 and (stroke.maskEditMode~=0 or not state.paint.maskRestrictBrush or
                state.paint.maskVertices[candidate.vertex.globalIndex]) then
            local index=candidate.vertex.globalIndex
            local previous=stroke.alphas[index] or 0
            stroke.alphas[index]=stroke.operationMode==4 and math.max(previous,alpha) or
                1-(1-previous)*(1-alpha)
        end
    end
end

local function extendPaintStroke(hit)
    local stroke=state.paint.stroke
    if not stroke or not hit then return end
    if stroke.requiredSubset and hit.triangle.subset~=stroke.requiredSubset then
        stroke.lastPoint=nil
        stroke.lastTriangle=nil
        stroke.distanceSinceSample=0
        return
    end
    local previous=stroke.lastPoint
    local point=hit.point
    if previous then
        local dx,dy,dz=point.x-previous.x,point.y-previous.y,point.z-previous.z
        local distance=math.sqrt(dx*dx+dy*dy+dz*dz)
        if distance>1e-9 then
            local spacing=math.max(state.paint.radius*0.25,1e-6)
            local carried=stroke.distanceSinceSample or 0
            if math.floor((carried+distance)/spacing)>256 then
                spacing=distance/256
                carried=0
            end
            local sampleDistance=spacing-carried
            while sampleDistance<=distance+1e-9 do
                local t=math.min(1,sampleDistance/distance)
                local triangle=t<0.5 and stroke.lastTriangle or hit.triangle
                stampPaintStroke(stroke,{x=previous.x+dx*t,y=previous.y+dy*t,z=previous.z+dz*t},
                    triangle)
                sampleDistance=sampleDistance+spacing
            end
            stroke.distanceSinceSample=(carried+distance)%spacing
        end
    else
        stampPaintStroke(stroke,point,hit.triangle)
        stroke.distanceSinceSample=0
    end
    stroke.lastPoint={x=point.x,y=point.y,z=point.z}
    stroke.lastTriangle=hit.triangle
end

local function beginPaintStroke(hit)
    if not state.meshVisible then return false end
    local bone=getBones()[state.paint.boneIndex]
    if not hit or (state.paint.maskEditMode==0 and not bone) then return false end
    local snapshot=nil
    if state.paint.maskEditMode==0 then
        snapshot=stageRollbackSnapshot('swl_history_paint_add')
        if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    end
    state.paint.stroke={snapshot=snapshot,boneName=bone and bone.name or nil,
        boneId=bone and bone.boneId or nil,
        operationMode=state.paint.operationMode,smoothIterations=state.paint.smoothIterations,
        rigidCoreRatio=state.paint.rigidCoreRatio,
        maskEditMode=state.paint.maskEditMode,
        requiredSubset=state.paint.restrictToHitSubset and hit.triangle.subset or nil,
        alphas={},lastPoint=nil,distanceSinceSample=0}
    extendPaintStroke(hit)
    rebuildPaintCursor(hit)
    return true
end

local function cancelPaintStroke()
    local stroke=state.paint.stroke
    if not stroke then return false end
    if stroke.snapshot then discardRollbackSnapshot(stroke.snapshot) end
    state.paint.stroke=nil
    setStatus(tLang.L('swl_paint_stroke_cancelled'),false)
    return true
end

local function commitPaintStroke()
    local stroke=state.paint.stroke
    if not stroke then return false end
    state.paint.stroke=nil
    local indices={}
    for index,alpha in pairs(stroke.alphas) do
        if alpha>0 then indices[#indices+1]=index end
    end
    table.sort(indices)
    if stroke.maskEditMode~=0 then
        for _,index in ipairs(indices) do
            state.paint.maskVertices[index]=stroke.maskEditMode==1 and true or nil
        end
        rebuildPaintMaskMarkers()
        setStatus(string.format(tLang.L('swl_paint_mask_count_fmt'),
            (function() local count=0; for _ in pairs(state.paint.maskVertices) do count=count+1 end; return count end)()),false)
        return true
    end
    if #indices==0 then
        discardRollbackSnapshot(stroke.snapshot)
        return false
    end
    local edits={}
    local candidateMaps,editable={},{ }
    local adjacency=stroke.operationMode==3 and buildTopologyAdjacency() or nil
    local weightCache={}
    local function weightsFor(index)
        if not weightCache[index] then weightCache[index]=readInfluenceMap(index,false) end
        return weightCache[index]
    end
    local smoothTargets=nil
    if stroke.operationMode==3 then
        local current={}
        for _,index in ipairs(indices) do
            current[index]=weightsFor(index)[stroke.boneName] or 0
            for neighbor in pairs(adjacency[index] or {}) do
                if current[neighbor]==nil then
                    current[neighbor]=weightsFor(neighbor)[stroke.boneName] or 0
                end
            end
        end
        for _=1,math.max(1,stroke.smoothIterations or 1) do
            local nextValues={}
            for _,index in ipairs(indices) do
                local average,count=0,0
                for neighbor in pairs(adjacency[index] or {}) do
                    average=average+(current[neighbor] or 0)
                    count=count+1
                end
                local oldTarget=current[index] or 0
                nextValues[index]=count>0 and
                    oldTarget+(average/count-oldTarget)*stroke.alphas[index] or oldTarget
            end
            for index,value in pairs(nextValues) do current[index]=value end
        end
        smoothTargets=current
    end
    for _,index in ipairs(indices) do
        local before=weightsFor(index)
        local influences=nil
        if stroke.operationMode==3 then
            local oldTarget=before[stroke.boneName] or 0
            local newTarget=smoothTargets[index] or oldTarget
            if newTarget>oldTarget+1e-7 and oldTarget<1 then
                influences=blendedInfluences(index,stroke.boneName,
                    (newTarget-oldTarget)/(1-oldTarget))
            elseif newTarget<oldTarget-1e-7 then
                local otherWeight=1-oldTarget
                if otherWeight>1e-9 then
                    local smoothed={}
                    for name,weight in pairs(before) do
                        if name==stroke.boneName then
                            if newTarget>0 then smoothed[name]=newTarget end
                        else
                            smoothed[name]=weight*(1-newTarget)/otherWeight
                        end
                    end
                    influences=normalizedInfluences(smoothed)
                end
            end
        elseif stroke.operationMode==2 then
            local remaining={}
            for name,weight in pairs(before) do remaining[name]=weight end
            local targetWeight=remaining[stroke.boneName] or 0
            local otherWeight=0
            for name,weight in pairs(remaining) do
                if name~=stroke.boneName then otherWeight=otherWeight+weight end
            end
            if targetWeight>0 and otherWeight>0 then
                remaining[stroke.boneName]=targetWeight*(1-stroke.alphas[index])
                influences=normalizedInfluences(remaining)
            end
        else
            influences=blendedInfluences(index,stroke.boneName,stroke.alphas[index])
        end
        local afterTarget=0
        for _,influence in ipairs(influences or {}) do
            if influence.name==stroke.boneName then afterTarget=influence.weight break end
        end
        if influences and math.abs(afterTarget-(before[stroke.boneName] or 0))>1e-7 then
            local candidate={}
            for _,influence in ipairs(influences) do
                candidate[influence.name]=influence.weight
            end
            candidateMaps[index]=candidate
            editable[index]=true
            local row={index}
            for slot=1,4 do
                local influence=influences[slot]
                row[slot*2]=influence and influence.name or nil
                row[slot*2+1]=influence and influence.weight or 0
            end
            edits[#edits+1]=row
        end
    end
    if #edits==0 then
        discardRollbackSnapshot(stroke.snapshot)
        local noChangeKey=stroke.operationMode==4 and 'swl_paint_rigid_no_change' or
            stroke.operationMode==3 and 'swl_paint_smooth_no_change' or
            stroke.operationMode==2 and 'swl_paint_subtract_no_change' or
            'swl_paint_add_no_change'
        setStatus(tLang.L(noChangeKey),false)
        return false
    end
    local unsafeTriangles={}
    local strokeSafetyReport=nil
    local okDiagnostic,_,unsafeFaceSamples,diagnosticTriangles,checkedFaces,poseSamples,
        minimumAreaRatio,maximumOrientationDegrees,minimumNormalAlignment=safeCall(function()
            return poseSafeRepairScale(weightCache,candidateMaps,editable,true)
        end)
    if okDiagnostic and poseSamples>0 then
        unsafeTriangles=diagnosticTriangles or {}
        strokeSafetyReport={changedVertices=#edits,checkedFaces=checkedFaces,
            poseSamples=poseSamples,unsafeFaces=#unsafeTriangles,
            unsafeFaceSamples=unsafeFaceSamples,minimumAreaRatio=minimumAreaRatio,
            maximumOrientationDegrees=maximumOrientationDegrees,
            minimumNormalAlignment=minimumNormalAlignment}
    end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(stroke.snapshot)
        if ok then setStatus(tLang.L('swl_paint_stroke_failed'),true) end
        return false
    end
    local historyKey=stroke.operationMode==4 and 'swl_history_paint_rigid' or
        stroke.operationMode==3 and 'swl_history_paint_smooth' or
        stroke.operationMode==2 and 'swl_history_paint_subtract' or 'swl_history_paint_add'
    commitRollbackSnapshot(stroke.snapshot,historyKey)
    state.modified=true
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    rebuildPaintStrokeSafetyOverlay(unsafeTriangles,strokeSafetyReport)
    applyWorkspaceVisibility()
    local statusKey=stroke.operationMode==4 and 'swl_paint_rigid_applied_fmt' or
        stroke.operationMode==3 and 'swl_paint_smooth_applied_fmt' or
        stroke.operationMode==2 and 'swl_paint_subtract_applied_fmt' or
        'swl_paint_stroke_applied_fmt'
    setStatus(string.format(tLang.L(statusKey),#edits,stroke.boneName),false,true)
    return true
end

local function cleanPaintWeakInfluences(forceCompleteMesh)
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return false end
    local indices={}
    for index in pairs(cache.vertices) do
        if forceCompleteMesh or not state.paint.diagnosticsUseMask or
                state.paint.maskVertices[index] then
            indices[#indices+1]=index
        end
    end
    table.sort(indices)
    local edits={}
    for _,index in ipairs(indices) do
        local before=readInfluenceMap(index,false)
        local ordered=normalizedInfluences(before)
        if #ordered>0 then
            local kept={{name=ordered[1].name,weight=ordered[1].weight}}
            local removed=false
            for slot=2,#ordered do
                local influence=ordered[slot]
                if influence.weight>=state.paint.cleanThreshold then
                    kept[#kept+1]={name=influence.name,weight=influence.weight}
                else
                    removed=true
                end
            end
            if removed then
                local weightMap={}
                for _,influence in ipairs(kept) do weightMap[influence.name]=influence.weight end
                kept=normalizedInfluences(weightMap)
                local row={index}
                for slot=1,4 do
                    local influence=kept[slot]
                    row[slot*2]=influence and influence.name or nil
                    row[slot*2+1]=influence and influence.weight or 0
                end
                edits[#edits+1]=row
            end
        end
    end
    if #edits==0 then
        setStatus(tLang.L('swl_paint_clean_no_change'),false)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_clean_weights')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(snapshot)
        if ok then setStatus(tLang.L('swl_paint_clean_failed'),true) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_clean_weights')
    state.modified=true
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_clean_applied_fmt'),#edits,
        state.paint.cleanThreshold),false,true)
    return true
end

local function limitPaintMaximumInfluences()
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return false end
    local maximum=math.max(1,math.min(4,state.paint.maximumInfluences or 4))
    local indices={}
    for index in pairs(cache.vertices) do indices[#indices+1]=index end
    table.sort(indices)
    local edits={}
    for _,index in ipairs(indices) do
        local ordered=normalizedInfluences(readInfluenceMap(index,false))
        if #ordered>maximum then
            local keptMap={}
            for slot=1,maximum do
                local influence=ordered[slot]
                keptMap[influence.name]=influence.weight
            end
            local kept=normalizedInfluences(keptMap)
            local row={index}
            for slot=1,4 do
                local influence=kept[slot]
                row[slot*2]=influence and influence.name or nil
                row[slot*2+1]=influence and influence.weight or 0
            end
            edits[#edits+1]=row
        end
    end
    if #edits==0 then
        state.paint.limitInfluencesConfirmed=false
        setStatus(tLang.L('swl_paint_limit_influences_no_change'),false)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_limit_influences')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(snapshot)
        if ok then setStatus(tLang.L('swl_paint_limit_influences_failed'),true) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_limit_influences')
    state.paint.limitInfluencesConfirmed=false
    state.modified=true
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_limit_influences_applied_fmt'),#edits,
        maximum),false,true)
    return true
end

buildTopologyAdjacency=function()
    if state.topologyAdjacency then return state.topologyAdjacency end
    local adjacency={}
    local okS,subsets=safeCall(function() return state.meshD:getTotalSubset(1) end)
    if not okS then return adjacency end
    local offset=0
    local function connect(a,b)
        if a==b then return end
        adjacency[a]=adjacency[a] or {}; adjacency[b]=adjacency[b] or {}
        adjacency[a][b]=true; adjacency[b][a]=true
    end
    for subset=1,subsets do
        local okV,total=safeCall(function() return state.meshD:getTotalVertex(1,subset) end)
        total=okV and total or 0
        local okI,indices=safeCall(function() return state.meshD:getIndex(1,subset) end)
        if okI and indices and #indices>=3 then
            for i=1,#indices-2,3 do
                local a,b,c=offset+indices[i],offset+indices[i+1],offset+indices[i+2]
                connect(a,b); connect(b,c); connect(c,a)
            end
        else
            for vertex=1,total-2,3 do
                local a,b,c=offset+vertex,offset+vertex+1,offset+vertex+2
                connect(a,b); connect(b,c); connect(c,a)
            end
        end
        offset=offset+total
    end
    state.topologyAdjacency=adjacency
    return adjacency
end

buildCoincidentSeams=function(adjacency)
    if state.coincidentSeams then return state.coincidentSeams end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return {groups={},byVertex={}} end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local tolerance=math.max(extent*1e-6,1e-7)
    local function cell(point)
        return math.floor(point.x/tolerance),math.floor(point.y/tolerance),
            math.floor(point.z/tolerance)
    end
    local function cellKey(x,y,z) return x..':'..y..':'..z end
    local parent,buckets={},{ }
    local function root(index)
        local value=parent[index] or index
        while value~=(parent[value] or value) do value=parent[value] end
        while index~=(parent[index] or index) do
            local nextIndex=parent[index]; parent[index]=value; index=nextIndex
        end
        return value
    end
    local function unite(left,right)
        left,right=root(left),root(right)
        if left~=right then parent[right]=left end
    end
    for _,vertex in ipairs(cache.vertices) do
        local index=vertex.globalIndex
        parent[index]=index
        local x,y,z=cell(vertex.point)
        for dx=-1,1 do for dy=-1,1 do for dz=-1,1 do
            for _,other in ipairs(buckets[cellKey(x+dx,y+dy,z+dz)] or {}) do
                local point=cache.vertices[other].point
                local px,py,pz=point.x-vertex.point.x,point.y-vertex.point.y,
                    point.z-vertex.point.z
                if px*px+py*py+pz*pz<=tolerance*tolerance then unite(index,other) end
            end
        end end end
        local key=cellKey(x,y,z)
        buckets[key]=buckets[key] or {}; buckets[key][#buckets[key]+1]=index
    end
    local coincident={}
    for _,vertex in ipairs(cache.vertices) do
        local key=root(vertex.globalIndex)
        coincident[key]=coincident[key] or {}
        coincident[key][#coincident[key]+1]=vertex.globalIndex
    end
    local function sharesGeometricNeighbor(left,right)
        for leftNeighbor in pairs(adjacency[left] or {}) do
            local leftVertex=cache.vertices[leftNeighbor]
            for rightNeighbor in pairs(adjacency[right] or {}) do
                local rightVertex=cache.vertices[rightNeighbor]
                if leftVertex and rightVertex then
                    local x=leftVertex.point.x-rightVertex.point.x
                    local y=leftVertex.point.y-rightVertex.point.y
                    local z=leftVertex.point.z-rightVertex.point.z
                    if x*x+y*y+z*z<=tolerance*tolerance then return true end
                end
            end
        end
        return false
    end
    local groups,byVertex={},{}
    for _,bucket in pairs(coincident) do
        local remaining={}
        for _,index in ipairs(bucket) do remaining[index]=true end
        while next(remaining) do
            local seed=next(remaining)
            local accepted,queue={seed},{seed}; remaining[seed]=nil
            local position=1
            while position<=#queue do
                local member=queue[position]; position=position+1
                local additions={}
                for candidate in pairs(remaining) do
                    if sharesGeometricNeighbor(member,candidate) then
                        additions[#additions+1]=candidate
                    end
                end
                for _,candidate in ipairs(additions) do
                    remaining[candidate]=nil
                    accepted[#accepted+1]=candidate; queue[#queue+1]=candidate
                end
            end
            if #accepted>1 then
                groups[#groups+1]=accepted
                for _,index in ipairs(accepted) do byVertex[index]=accepted end
            end
        end
    end
    state.coincidentSeams={groups=groups,byVertex=byVertex,tolerance=tolerance}
    return state.coincidentSeams
end

function adjustPaintMaskTopology(mode)
    local previousCount=tMaskTopology.count(state.paint.maskVertices)
    if previousCount==0 then
        setStatus(tLang.L('swl_paint_mask_topology_empty'),false)
        return false
    end
    local adjacency=buildTopologyAdjacency()
    local seamByVertex={}
    if state.paint.maskTopologyAcrossSeams then
        seamByVertex=buildCoincidentSeams(adjacency).byVertex
    end
    local adjusted=tMaskTopology.adjust(state.paint.maskVertices,adjacency,seamByVertex,
        state.paint.maskTopologyRings,mode,state.paint.maskTopologyAcrossSeams)
    local adjustedCount=tMaskTopology.count(adjusted)
    if adjustedCount==previousCount then
        setStatus(tLang.L('swl_paint_mask_topology_no_change'),false)
        return false
    end
    state.paint.maskVertices=adjusted
    rebuildPaintMaskMarkers()
    rebuildPaintCursor(state.paint.cursorHit)
    setStatus(string.format(tLang.L(mode=='shrink' and
        'swl_paint_mask_topology_shrunk_fmt' or 'swl_paint_mask_topology_grown_fmt'),
        state.paint.maskTopologyRings,previousCount,adjustedCount),false,true)
    return true
end

local function influenceDistance(a,b)
    local names={}
    for name in pairs(a) do names[name]=true end
    for name in pairs(b) do names[name]=true end
    local total=0
    for name in pairs(names) do total=total+math.abs((a[name] or 0)-(b[name] or 0)) end
    return math.min(1,total*0.5)
end

local function rebuildPinnedSeamInspector()
    destroyObject(state.paint.inspectorSeamMarkers)
    state.paint.inspectorSeamMarkers=nil
    state.paint.inspectorSeamReport=nil
    state.paint.inspectorNormalReport=nil
    local selected=state.paint.hoveredVertex
    if not selected or not state.paint.inspectorPinned then return end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return end
    local seams=buildCoincidentSeams(buildTopologyAdjacency())
    local group=seams.byVertex[selected.globalIndex] or {selected.globalIndex}
    local members={}
    for _,index in ipairs(group) do
        local vertex=cache.vertices[index]
        if vertex then
            local weightMap=readInfluenceMap(index,false)
            members[#members+1]={globalIndex=index,subset=vertex.subset,point=vertex.point,
                localIndex=vertex.localIndex,
                weightMap=weightMap,influences=normalizedInfluences(weightMap)}
        end
    end
    table.sort(members,function(a,b) return a.globalIndex<b.globalIndex end)
    for _,member in ipairs(members) do
        if member.globalIndex==selected.globalIndex then
            selected.influences=member.influences
            break
        end
    end
    local maximumDivergence=0
    for left=1,#members-1 do
        for right=left+1,#members do
            maximumDivergence=math.max(maximumDivergence,
                influenceDistance(members[left].weightMap,members[right].weightMap))
        end
    end
    state.paint.inspectorSeamReport={members=members,
        maximumDivergence=maximumDivergence,tolerance=seams.tolerance or 0}
    if #members>1 then
        local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
            state.meshBounds.maxY-state.meshBounds.minY,
            state.meshBounds.maxZ-state.meshBounds.minZ) or 1
        state.paint.inspectorSeamMarkers=buildVertexMarkers(members,0,1,1,extent)
        if state.paint.inspectorSeamMarkers then
            state.paint.inspectorSeamMarkers.alwaysRender=true
            state.paint.inspectorSeamMarkers.alwaysOnTop=true
            state.paint.inspectorSeamMarkers.alwaysOnTopPriority=1
        end
    end
end

local function synchronizePinnedSeamWeights()
    local report=state.paint.inspectorSeamReport
    if not report or #report.members<=1 then
        setStatus(tLang.L('swl_paint_vertex_seam_sync_unavailable'),false)
        return false
    end
    local average={}
    for _,member in ipairs(report.members) do
        for name,weight in pairs(member.weightMap) do
            average[name]=(average[name] or 0)+weight/#report.members
        end
    end
    local influences=normalizedInfluences(average)
    if #influences==0 then
        setStatus(tLang.L('swl_paint_vertex_seam_sync_failed'),true)
        return false
    end
    local edits={}
    for _,member in ipairs(report.members) do
        local row={member.globalIndex}
        for slot=1,4 do
            local influence=influences[slot]
            row[slot*2]=influence and influence.name or nil
            row[slot*2+1]=influence and influence.weight or 0
        end
        edits[#edits+1]=row
    end
    local snapshot=stageRollbackSnapshot('swl_history_sync_seam_weights')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(snapshot)
        if ok then setStatus(tLang.L('swl_paint_vertex_seam_sync_failed'),true) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_sync_seam_weights')
    state.modified=true
    state.paint.inspectorSeamSyncConfirmed=false
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    rebuildPinnedSeamInspector()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_vertex_seam_sync_applied_fmt'),#edits),false,true)
    return true
end

local function analyzeGlobalSeamWeights()
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return nil end
    local seams=buildCoincidentSeams(buildTopologyAdjacency())
    local conflicts,vertices,maximumDivergence={},{},0
    for _,group in ipairs(seams.groups) do
        local maps,groupMaximum={ },0
        for _,index in ipairs(group) do maps[index]=readInfluenceMap(index,false) end
        for left=1,#group-1 do
            for right=left+1,#group do
                groupMaximum=math.max(groupMaximum,
                    influenceDistance(maps[group[left]],maps[group[right]]))
            end
        end
        if groupMaximum>1e-9 then
            conflicts[#conflicts+1]={indices=group,maps=maps,divergence=groupMaximum}
            maximumDivergence=math.max(maximumDivergence,groupMaximum)
            for _,index in ipairs(group) do vertices[index]=true end
        end
    end
    local vertexCount=0
    for _ in pairs(vertices) do vertexCount=vertexCount+1 end
    state.paint.globalSeamAudit={groups=conflicts,groupCount=#conflicts,
        vertexCount=vertexCount,totalGroups=#seams.groups,
        maximumDivergence=maximumDivergence,tolerance=seams.tolerance or 0}
    state.paint.globalSeamSyncConfirmed=false
    return state.paint.globalSeamAudit
end

local function synchronizeGlobalSeamWeights()
    local audit=state.paint.globalSeamAudit
    if not audit or audit.groupCount==0 then
        setStatus(tLang.L('swl_paint_global_seam_sync_no_change'),false)
        return false
    end
    local edits={}
    for _,group in ipairs(audit.groups) do
        local average={}
        for _,index in ipairs(group.indices) do
            for name,weight in pairs(group.maps[index]) do
                average[name]=(average[name] or 0)+weight/#group.indices
            end
        end
        local influences=normalizedInfluences(average)
        for _,index in ipairs(group.indices) do
            local row={index}
            for slot=1,4 do
                local influence=influences[slot]
                row[slot*2]=influence and influence.name or nil
                row[slot*2+1]=influence and influence.weight or 0
            end
            edits[#edits+1]=row
        end
    end
    local snapshot=stageRollbackSnapshot('swl_history_sync_all_seam_weights')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(snapshot)
        if ok then setStatus(tLang.L('swl_paint_global_seam_sync_failed'),true) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_sync_all_seam_weights')
    state.modified=true
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    analyzeGlobalSeamWeights()
    if state.paint.inspectorPinned then rebuildPinnedSeamInspector() end
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_global_seam_sync_applied_fmt'),
        audit.groupCount,#edits),false,true)
    return true
end

local function analyzePinnedDeformedGeometry()
    destroyObject(state.paint.inspectorGeometryOverlay)
    state.paint.inspectorGeometryOverlay=nil
    state.paint.inspectorGeometryReport=nil
    local selected=state.paint.hoveredVertex
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not selected or not state.paint.inspectorPinned or not cache then return false end
    local triangles=cache.incidentTriangles[selected.globalIndex] or {}
    if #triangles==0 then return false end
    local clipIndex=state.animationClipSelected or state.skeletalPreview.selected or 1
    local duration=state.preview:getSkeletalAnimationDuration(clipIndex) or 0
    local time=math.max(0,math.min(state.paint.inspectorGeometryTime or 0,duration))
    state.paint.inspectorGeometryTime=time
    local ok,pose=safeCall(function()
        return state.meshD:evaluateSkeletalAuthoringPose(clipIndex,time,'lbs')
    end)
    if not ok or not pose or not pose.palette then return false end
    local boneIndex={}
    for index,bone in ipairs(getBones()) do boneIndex[bone.name]=index-1 end
    local function deform(point,weights,vectorOnly)
        local x,y,z=0,0,0
        for name,weight in pairs(weights) do
            local index=boneIndex[name]
            if index then
                local first=index*12
                x=x+(point.x*pose.palette[first+1]+point.y*pose.palette[first+2]+
                    point.z*pose.palette[first+3]+(vectorOnly and 0 or pose.palette[first+4]))*weight
                y=y+(point.x*pose.palette[first+5]+point.y*pose.palette[first+6]+
                    point.z*pose.palette[first+7]+(vectorOnly and 0 or pose.palette[first+8]))*weight
                z=z+(point.x*pose.palette[first+9]+point.y*pose.palette[first+10]+
                    point.z*pose.palette[first+11]+(vectorOnly and 0 or pose.palette[first+12]))*weight
            end
        end
        return {x=x,y=y,z=z}
    end
    local function cross(a,b,c)
        local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
        local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
        return {x=uy*vz-uz*vy,y=uz*vx-ux*vz,z=ux*vy-uy*vx}
    end
    local function length(v) return math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z) end
    local function edgeLength(a,b)
        return math.sqrt((a.x-b.x)^2+(a.y-b.y)^2+(a.z-b.z)^2)
    end
    local minimumAreaRatio,minimumAlignment=math.huge,1
    local minimumEdgeRatio,maximumEdgeRatio=math.huge,0
    local collapsed,inverted=0,0
    local worstTriangles={}
    for _,triangle in ipairs(triangles) do
        local vertices={triangle.a,triangle.b,triangle.c}
        local after,weightMaps={},{}
        for slot,vertex in ipairs(vertices) do
            weightMaps[slot]=readInfluenceMap(vertex.globalIndex,false)
            after[slot]=deform(vertex.point,weightMaps[slot],false)
        end
        local bindArea=cross(vertices[1].point,vertices[2].point,vertices[3].point)
        local deformedArea=cross(after[1],after[2],after[3])
        local bindLength,deformedLength=length(bindArea),length(deformedArea)
        local areaRatio=bindLength>1e-12 and deformedLength/bindLength or 1
        local expected={x=0,y=0,z=0}
        for slot=1,3 do
            local transformed=deform(bindArea,weightMaps[slot],true)
            expected.x=expected.x+transformed.x
            expected.y=expected.y+transformed.y
            expected.z=expected.z+transformed.z
        end
        local expectedLength=length(expected)
        local alignment=1
        if deformedLength>1e-12 and expectedLength>1e-12 then
            alignment=math.max(-1,math.min(1,(deformedArea.x*expected.x+
                deformedArea.y*expected.y+deformedArea.z*expected.z)/
                (deformedLength*expectedLength)))
        end
        minimumAreaRatio=math.min(minimumAreaRatio,areaRatio)
        minimumAlignment=math.min(minimumAlignment,alignment)
        if areaRatio<0.25 then collapsed=collapsed+1 end
        if alignment<=0 then inverted=inverted+1 end
        for edge=1,3 do
            local nextEdge=edge%3+1
            local beforeLength=edgeLength(vertices[edge].point,vertices[nextEdge].point)
            if beforeLength>1e-12 then
                local ratio=edgeLength(after[edge],after[nextEdge])/beforeLength
                minimumEdgeRatio=math.min(minimumEdgeRatio,ratio)
                maximumEdgeRatio=math.max(maximumEdgeRatio,ratio)
            end
        end
        if areaRatio<0.5 or alignment<0.25 then worstTriangles[#worstTriangles+1]=triangle end
    end
    if minimumAreaRatio==math.huge then minimumAreaRatio=1 end
    if minimumEdgeRatio==math.huge then minimumEdgeRatio=1 end
    state.paint.inspectorGeometryReport={clipIndex=clipIndex,time=time,triangles=#triangles,
        minimumAreaRatio=minimumAreaRatio,minimumNormalAlignment=minimumAlignment,
        minimumEdgeRatio=minimumEdgeRatio,maximumEdgeRatio=maximumEdgeRatio,
        collapsed=collapsed,inverted=inverted,flagged=#worstTriangles}
    if #worstTriangles>0 then
        local coords={}
        for _,triangle in ipairs(worstTriangles) do
            for _,vertex in ipairs({triangle.a,triangle.b,triangle.c,
                    triangle.a,triangle.c,triangle.b}) do
                appendPoint(coords,vertex.point.x,vertex.point.y,vertex.point.z)
            end
        end
        local overlay=shape:new('3d',0,0,0)
        if overlay:create(coords,nil,'pinned_deformed_geometry_'..state.paint.heatmapGeneration) then
            overlay:setColor(1,0.55,0.05,0.55); overlay:setPos(0,0,0)
            overlay.alwaysOnTop=true; overlay.alwaysOnTopPriority=1
            state.paint.inspectorGeometryOverlay=overlay
        else
            destroyObject(overlay)
        end
    end
    applyWorkspaceVisibility()
    return true
end

local function analyzePinnedBindTopology()
    destroyObject(state.paint.inspectorTopologyOverlay)
    state.paint.inspectorTopologyOverlay=nil
    state.paint.inspectorTopologyReport=nil
    local selected=state.paint.hoveredVertex
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not selected or not state.paint.inspectorPinned or not cache then return false end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local tolerance=math.max(extent*1e-6,1e-7)
    local parent,buckets={},{ }
    local function root(index)
        local value=parent[index] or index
        while value~=(parent[value] or value) do value=parent[value] end
        while index~=(parent[index] or index) do
            local nextIndex=parent[index]; parent[index]=value; index=nextIndex
        end
        return value
    end
    local function unite(a,b)
        a,b=root(a),root(b)
        if a~=b then parent[b]=a end
    end
    local function key(x,y,z) return x..':'..y..':'..z end
    for _,vertex in ipairs(cache.vertices) do
        local index,point=vertex.globalIndex,vertex.point
        parent[index]=index
        local cx,cy,cz=math.floor(point.x/tolerance),math.floor(point.y/tolerance),
            math.floor(point.z/tolerance)
        for dx=-1,1 do for dy=-1,1 do for dz=-1,1 do
            for _,other in ipairs(buckets[key(cx+dx,cy+dy,cz+dz)] or {}) do
                local otherPoint=cache.vertices[other].point
                local x,y,z=point.x-otherPoint.x,point.y-otherPoint.y,point.z-otherPoint.z
                if x*x+y*y+z*z<=tolerance*tolerance then unite(index,other) end
            end
        end end end
        local cell=key(cx,cy,cz)
        buckets[cell]=buckets[cell] or {}; buckets[cell][#buckets[cell]+1]=index
    end
    local edgeMap={}
    local function addEdge(a,b,triangle)
        local ra,rb=root(a.globalIndex),root(b.globalIndex)
        if ra==rb then return end
        local edgeKey=ra<rb and ra..':'..rb or rb..':'..ra
        local record=edgeMap[edgeKey]
        if not record then
            record={count=0,a=a,b=b,rootA=ra,rootB=rb,triangle=triangle}
            edgeMap[edgeKey]=record
        end
        record.count=record.count+1
    end
    for _,triangle in ipairs(cache.triangles) do
        addEdge(triangle.a,triangle.b,triangle)
        addEdge(triangle.b,triangle.c,triangle)
        addEdge(triangle.c,triangle.a,triangle)
    end
    local boundaries={}
    for _,edge in pairs(edgeMap) do
        if edge.count==1 then boundaries[#boundaries+1]=edge end
    end
    local function pointSegmentDistance(point,a,b)
        local dx,dy,dz=b.x-a.x,b.y-a.y,b.z-a.z
        local lengthSquared=dx*dx+dy*dy+dz*dz
        local t=lengthSquared>1e-12 and ((point.x-a.x)*dx+(point.y-a.y)*dy+
            (point.z-a.z)*dz)/lengthSquared or 0
        t=math.max(0,math.min(1,t))
        local x,y,z=a.x+dx*t,a.y+dy*t,a.z+dz*t
        return math.sqrt((point.x-x)^2+(point.y-y)^2+(point.z-z)^2)
    end
    local selectedRoot=root(selected.globalIndex)
    local nearest,nearestDistance=nil,math.huge
    for _,edge in ipairs(boundaries) do
        local distance=(edge.rootA==selectedRoot or edge.rootB==selectedRoot) and 0 or
            pointSegmentDistance(selected.point,edge.a.point,edge.b.point)
        if distance<nearestDistance then nearest,nearestDistance=edge,distance end
    end
    local counterpart,counterpartGap=nil,math.huge
    local function distance(a,b)
        return math.sqrt((a.x-b.x)^2+(a.y-b.y)^2+(a.z-b.z)^2)
    end
    if nearest then
        for _,edge in ipairs(boundaries) do
            if edge~=nearest and edge.rootA~=nearest.rootA and edge.rootA~=nearest.rootB and
                    edge.rootB~=nearest.rootA and edge.rootB~=nearest.rootB then
                local direct=math.max(distance(nearest.a.point,edge.a.point),
                    distance(nearest.b.point,edge.b.point))
                local reversed=math.max(distance(nearest.a.point,edge.b.point),
                    distance(nearest.b.point,edge.a.point))
                local gap=math.min(direct,reversed)
                if gap<counterpartGap then counterpart,counterpartGap=edge,gap end
            end
        end
    end
    state.paint.inspectorTopologyReport={boundaryEdges=#boundaries,tolerance=tolerance,
        nearestDistance=nearest and nearestDistance or nil,
        counterpartGap=counterpart and counterpartGap or nil,
        nearPinned=nearest and nearestDistance<=extent*0.01 or false}
    if nearest then
        local coords={}
        appendPoint(coords,nearest.a.point.x,nearest.a.point.y,nearest.a.point.z)
        appendPoint(coords,nearest.b.point.x,nearest.b.point.y,nearest.b.point.z)
        if counterpart then
            appendPoint(coords,counterpart.a.point.x,counterpart.a.point.y,counterpart.a.point.z)
            appendPoint(coords,counterpart.b.point.x,counterpart.b.point.y,counterpart.b.point.z)
        end
        local overlay=line:new('3d',0,0,0)
        overlay:add(coords); overlay:setColor(1,0.2,0.05,1); overlay:setPos(0,0,0)
        overlay.alwaysOnTop=true; overlay.alwaysOnTopPriority=1
        state.paint.inspectorTopologyOverlay=overlay
    end
    applyWorkspaceVisibility()
    return true
end

local function analyzePinnedSeamNormals()
    state.paint.inspectorNormalReport=nil
    local seam=state.paint.inspectorSeamReport
    if not seam or #seam.members<=1 then return false end
    local clipIndex=state.animationClipSelected or state.skeletalPreview.selected or 1
    local duration=state.preview:getSkeletalAnimationDuration(clipIndex) or 0
    local time=math.max(0,math.min(state.paint.inspectorGeometryTime or 0,duration))
    local ok,pose=safeCall(function()
        return state.meshD:evaluateSkeletalAuthoringPose(clipIndex,time,'lbs')
    end)
    if not ok or not pose or not pose.palette then return false end
    local boneIndex={}
    for index,bone in ipairs(getBones()) do boneIndex[bone.name]=index-1 end
    local function normalized(x,y,z)
        local length=math.sqrt(x*x+y*y+z*z)
        if length<=1e-12 then return nil end
        return {x=x/length,y=y/length,z=z/length}
    end
    local function deformNormal(normal,weights)
        local x,y,z=0,0,0
        for name,weight in pairs(weights) do
            local index=boneIndex[name]
            if index then
                local first=index*12
                x=x+(normal.x*pose.palette[first+1]+normal.y*pose.palette[first+2]+
                    normal.z*pose.palette[first+3])*weight
                y=y+(normal.x*pose.palette[first+5]+normal.y*pose.palette[first+6]+
                    normal.z*pose.palette[first+7])*weight
                z=z+(normal.x*pose.palette[first+9]+normal.y*pose.palette[first+10]+
                    normal.z*pose.palette[first+11])*weight
            end
        end
        return normalized(x,y,z)
    end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    local normals={}
    for _,member in ipairs(seam.members) do
        local point=member.point
        local bind=point and normalized(point.nx or 0,point.ny or 0,point.nz or 0) or nil
        if bind then
            local gx,gy,gz=0,0,0
            for _,triangle in ipairs(cache and cache.incidentTriangles[member.globalIndex] or {}) do
                local a,b,c=triangle.a.point,triangle.b.point,triangle.c.point
                local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
                local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
                local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
                if nx*bind.x+ny*bind.y+nz*bind.z<0 then nx,ny,nz=-nx,-ny,-nz end
                gx,gy,gz=gx+nx,gy+ny,gz+nz
            end
            normals[#normals+1]={globalIndex=member.globalIndex,bind=bind,
                deformed=deformNormal(bind,member.weightMap),geometric=normalized(gx,gy,gz)}
        end
    end
    local function angle(a,b)
        if not a or not b then return 0 end
        local cosine=math.max(-1,math.min(1,a.x*b.x+a.y*b.y+a.z*b.z))
        return math.acos(cosine)*180/math.pi
    end
    local bindMaximum,deformedMaximum=0,0
    for left=1,#normals-1 do
        for right=left+1,#normals do
            bindMaximum=math.max(bindMaximum,angle(normals[left].bind,normals[right].bind))
            deformedMaximum=math.max(deformedMaximum,
                angle(normals[left].deformed,normals[right].deformed))
        end
    end
    local reference=nil
    for _,entry in ipairs(normals) do
        if entry.globalIndex==state.paint.hoveredVertex.globalIndex then reference=entry break end
    end
    reference=reference or normals[1]
    for _,entry in ipairs(normals) do
        entry.bindFromSelected=reference and angle(reference.bind,entry.bind) or 0
        entry.deformedFromSelected=reference and angle(reference.deformed,entry.deformed) or 0
        entry.geometricDifference=angle(entry.bind,entry.geometric)
    end
    state.paint.inspectorNormalReport={clipIndex=clipIndex,time=time,
        copies=#seam.members,normals=#normals,bindMaximum=bindMaximum,
        deformedMaximum=deformedMaximum,entries=normals,
        selectedIndex=reference and reference.globalIndex or 0}
    return true
end

local function printPinnedNormalReport()
    local report=state.paint.inspectorNormalReport
    if not report then return false end
    print(string.format('[skeletal-editor][normal-report] clip=%d time=%.6f copies=%d normals=%d bind-max-deg=%.6f deformed-max-deg=%.6f selected=%d',
        report.clipIndex,report.time,report.copies,report.normals,report.bindMaximum,
        report.deformedMaximum,report.selectedIndex))
    for _,entry in ipairs(report.entries or {}) do
        local geometric=entry.geometric
        print(string.format('[skeletal-editor][normal-copy] vertex=%d selected=%s bind=(%.6f,%.6f,%.6f) bind-angle-deg=%.6f deformed=(%.6f,%.6f,%.6f) deformed-angle-deg=%.6f geometric=(%s) stored-geometric-deg=%.6f',
            entry.globalIndex,entry.globalIndex==report.selectedIndex and 'yes' or 'no',
            entry.bind.x,entry.bind.y,entry.bind.z,entry.bindFromSelected,
            entry.deformed and entry.deformed.x or 0,entry.deformed and entry.deformed.y or 0,
            entry.deformed and entry.deformed.z or 0,entry.deformedFromSelected,
            geometric and string.format('%.6f,%.6f,%.6f',geometric.x,geometric.y,geometric.z) or
                'unavailable',entry.geometricDifference))
    end
    setStatus(tLang.L('swl_paint_normal_printed'),false)
    return true
end

local function repairPinnedIncompatibleNormals()
    local report=state.paint.inspectorNormalReport
    if not report then return false end
    local threshold=math.max(0,math.min(180,state.paint.normalRepairThreshold or 30))
    local affected={}
    for _,entry in ipairs(report.entries or {}) do
        if entry.geometric and entry.geometricDifference>threshold then affected[#affected+1]=entry end
    end
    if #affected==0 then
        state.paint.normalRepairConfirmed=false
        setStatus(tLang.L('swl_paint_normal_repair_no_change'),false)
        return false
    end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    local snapshot=stageRollbackSnapshot('swl_history_repair_incompatible_normals')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local applied=true
    for _,entry in ipairs(affected) do
        local vertex=cache and cache.vertices[entry.globalIndex]
        if not vertex then applied=false break end
        local data={}
        for key,value in pairs(vertex.point) do data[key]=value end
        data.nx,data.ny,data.nz=entry.geometric.x,entry.geometric.y,entry.geometric.z
        local ok=safeCall(function()
            return state.meshD:setVertex(1,vertex.subset,vertex.localIndex,data)
        end)
        if not ok then applied=false break end
    end
    if not applied then
        restoreHistoryEntry(snapshot)
        discardRollbackSnapshot(snapshot)
        setStatus(tLang.L('swl_paint_normal_repair_failed'),true)
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_repair_incompatible_normals')
    state.modified=true
    state.paint.normalRepairConfirmed=false
    local selectedIndex=state.paint.hoveredVertex and state.paint.hoveredVertex.globalIndex
    state.paint.geometry=nil
    cache=buildPaintGeometryCache()
    if selectedIndex and cache and cache.vertices[selectedIndex] then
        state.paint.hoveredVertex.point=cache.vertices[selectedIndex].point
    end
    rebuildPinnedSeamInspector()
    analyzePinnedSeamNormals()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_normal_repair_applied_fmt'),#affected,
        threshold),false,true)
    return true
end

local function analyzeGlobalNormalCompatibility()
    destroyObject(state.paint.globalNormalMarkers)
    state.paint.globalNormalMarkers=nil
    state.paint.globalNormalAudit=nil
    state.paint.globalNormalRepairConfirmed=false
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return nil end
    local threshold=math.max(0,math.min(180,state.paint.normalRepairThreshold or 30))
    local function normalized(x,y,z)
        local length=math.sqrt(x*x+y*y+z*z)
        if length<=1e-12 then return nil end
        return {x=x/length,y=y/length,z=z/length}
    end
    local function angle(a,b)
        local cosine=math.max(-1,math.min(1,a.x*b.x+a.y*b.y+a.z*b.z))
        return math.acos(cosine)*180/math.pi
    end
    local affected,usable,missing,maximum={ },0,0,0
    for _,vertex in ipairs(cache.vertices) do
        local point=vertex.point
        local stored=normalized(point.nx or 0,point.ny or 0,point.nz or 0)
        if stored then
            local gx,gy,gz=0,0,0
            for _,triangle in ipairs(cache.incidentTriangles[vertex.globalIndex] or {}) do
                local a,b,c=triangle.a.point,triangle.b.point,triangle.c.point
                local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
                local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
                local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
                if nx*stored.x+ny*stored.y+nz*stored.z<0 then nx,ny,nz=-nx,-ny,-nz end
                gx,gy,gz=gx+nx,gy+ny,gz+nz
            end
            local geometric=normalized(gx,gy,gz)
            if geometric then
                usable=usable+1
                local difference=angle(stored,geometric)
                maximum=math.max(maximum,difference)
                if difference>threshold then
                    affected[#affected+1]={globalIndex=vertex.globalIndex,
                        subset=vertex.subset,localIndex=vertex.localIndex,point=vertex.point,
                        geometric=geometric,difference=difference}
                end
            else
                missing=missing+1
            end
        else
            missing=missing+1
        end
    end
    state.paint.globalNormalAudit={affected=affected,affectedCount=#affected,
        usable=usable,missing=missing,maximumDifference=maximum,threshold=threshold}
    if #affected>0 then
        local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
            state.meshBounds.maxY-state.meshBounds.minY,
            state.meshBounds.maxZ-state.meshBounds.minZ) or 1
        state.paint.globalNormalMarkers=buildVertexMarkers(affected,1,0.45,0,extent)
        if state.paint.globalNormalMarkers then
            state.paint.globalNormalMarkers.alwaysRender=true
            state.paint.globalNormalMarkers.alwaysOnTop=true
            state.paint.globalNormalMarkers.alwaysOnTopPriority=1
        end
    end
    applyWorkspaceVisibility()
    return state.paint.globalNormalAudit
end

local function repairGlobalIncompatibleNormals()
    local audit=state.paint.globalNormalAudit
    if not audit or audit.affectedCount==0 then
        setStatus(tLang.L('swl_paint_global_normal_no_change'),false)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_repair_all_incompatible_normals')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    local applied=true
    for _,entry in ipairs(audit.affected) do
        local vertex=cache and cache.vertices[entry.globalIndex]
        if not vertex then applied=false break end
        local data={}
        for key,value in pairs(vertex.point) do data[key]=value end
        data.nx,data.ny,data.nz=entry.geometric.x,entry.geometric.y,entry.geometric.z
        local ok=safeCall(function()
            return state.meshD:setVertex(1,entry.subset,entry.localIndex,data)
        end)
        if not ok then applied=false break end
    end
    if not applied then
        restoreHistoryEntry(snapshot)
        discardRollbackSnapshot(snapshot)
        setStatus(tLang.L('swl_paint_global_normal_failed'),true)
        return false
    end
    local affectedCount,threshold=audit.affectedCount,audit.threshold
    commitRollbackSnapshot(snapshot,'swl_history_repair_all_incompatible_normals')
    state.modified=true
    state.paint.geometry=nil
    buildPaintGeometryCache()
    analyzeGlobalNormalCompatibility()
    if state.paint.inspectorPinned then
        local selectedIndex=state.paint.hoveredVertex.globalIndex
        local selected=state.paint.geometry and state.paint.geometry.vertices[selectedIndex]
        if selected then state.paint.hoveredVertex.point=selected.point end
        rebuildPinnedSeamInspector()
        analyzePinnedSeamNormals()
    end
    setStatus(string.format(tLang.L('swl_paint_global_normal_applied_fmt'),
        affectedCount,threshold),false,true)
    return true
end

local function analyzeGlobalCoincidentNormalSmoothing()
    destroyObject(state.paint.globalNormalSmoothMarkers)
    state.paint.globalNormalSmoothMarkers=nil
    state.paint.globalNormalSmoothAudit=nil
    state.paint.globalNormalSmoothConfirmed=false
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return nil end
    local limit=math.max(0,math.min(180,state.paint.normalSmoothAngle or 30))
    local seams=buildCoincidentSeams(buildTopologyAdjacency())
    local function normalized(x,y,z)
        local length=math.sqrt(x*x+y*y+z*z)
        if length<=1e-12 then return nil end
        return {x=x/length,y=y/length,z=z/length}
    end
    local function angle(a,b)
        local cosine=math.max(-1,math.min(1,a.x*b.x+a.y*b.y+a.z*b.z))
        return math.acos(cosine)*180/math.pi
    end
    local edits,groups={},0
    for _,seam in ipairs(seams.groups) do
        local remaining={}
        for _,index in ipairs(seam) do
            local vertex=cache.vertices[index]
            local point=vertex and vertex.point
            local normal=point and normalized(point.nx or 0,point.ny or 0,point.nz or 0)
            if normal then remaining[#remaining+1]={vertex=vertex,normal=normal} end
        end
        table.sort(remaining,function(a,b) return a.vertex.globalIndex<b.vertex.globalIndex end)
        while #remaining>0 do
            local component={table.remove(remaining,1)}
            local changed=true
            while changed do
                changed=false
                for candidate=#remaining,1,-1 do
                    local compatible=true
                    for _,member in ipairs(component) do
                        if angle(remaining[candidate].normal,member.normal)>limit then
                            compatible=false; break
                        end
                    end
                    if compatible then
                        component[#component+1]=table.remove(remaining,candidate)
                        changed=true
                    end
                end
            end
            if #component>1 then
                local x,y,z=0,0,0
                for _,member in ipairs(component) do
                    x,y,z=x+member.normal.x,y+member.normal.y,z+member.normal.z
                end
                local average=normalized(x,y,z)
                local componentChanged=false
                if average then
                    for _,member in ipairs(component) do
                        if angle(member.normal,average)>1e-4 then
                            edits[#edits+1]={globalIndex=member.vertex.globalIndex,
                                subset=member.vertex.subset,localIndex=member.vertex.localIndex,
                                point=member.vertex.point,normal=average}
                            componentChanged=true
                        end
                    end
                end
                if componentChanged then groups=groups+1 end
            end
        end
    end
    state.paint.globalNormalSmoothAudit={edits=edits,affectedCount=#edits,
        groupCount=groups,totalSeams=#seams.groups,angle=limit}
    if #edits>0 then
        local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
            state.meshBounds.maxY-state.meshBounds.minY,
            state.meshBounds.maxZ-state.meshBounds.minZ) or 1
        state.paint.globalNormalSmoothMarkers=buildVertexMarkers(edits,0.25,0.8,1,extent)
        if state.paint.globalNormalSmoothMarkers then
            state.paint.globalNormalSmoothMarkers.alwaysRender=true
            state.paint.globalNormalSmoothMarkers.alwaysOnTop=true
            state.paint.globalNormalSmoothMarkers.alwaysOnTopPriority=1
        end
    end
    applyWorkspaceVisibility()
    return state.paint.globalNormalSmoothAudit
end

local function smoothGlobalCoincidentNormals()
    local audit=state.paint.globalNormalSmoothAudit
    if not audit or audit.affectedCount==0 then
        setStatus(tLang.L('swl_paint_normal_smooth_no_change'),false)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_smooth_coincident_normals')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    local applied=true
    for _,entry in ipairs(audit.edits) do
        local vertex=cache and cache.vertices[entry.globalIndex]
        if not vertex then applied=false break end
        local data={}
        for key,value in pairs(vertex.point) do data[key]=value end
        data.nx,data.ny,data.nz=entry.normal.x,entry.normal.y,entry.normal.z
        local ok=safeCall(function()
            return state.meshD:setVertex(1,entry.subset,entry.localIndex,data)
        end)
        if not ok then applied=false break end
    end
    if not applied then
        restoreHistoryEntry(snapshot)
        discardRollbackSnapshot(snapshot)
        setStatus(tLang.L('swl_paint_normal_smooth_failed'),true)
        return false
    end
    local affectedCount,groupCount,angleLimit=audit.affectedCount,audit.groupCount,audit.angle
    commitRollbackSnapshot(snapshot,'swl_history_smooth_coincident_normals')
    state.modified=true
    state.paint.geometry=nil
    buildPaintGeometryCache()
    analyzeGlobalCoincidentNormalSmoothing()
    if state.paint.inspectorPinned then
        local selectedIndex=state.paint.hoveredVertex.globalIndex
        local selected=state.paint.geometry and state.paint.geometry.vertices[selectedIndex]
        if selected then state.paint.hoveredVertex.point=selected.point end
        rebuildPinnedSeamInspector()
        analyzePinnedSeamNormals()
    end
    setStatus(string.format(tLang.L('swl_paint_normal_smooth_applied_fmt'),
        affectedCount,groupCount,angleLimit),false,true)
    return true
end

local function analyzeExactCoincidentPositions()
    destroyObject(state.paint.exactSeamPositionMarkers)
    state.paint.exactSeamPositionMarkers=nil
    state.paint.exactSeamPositionAudit=nil
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return nil end
    local seams=buildCoincidentSeams(buildTopologyAdjacency())
    local affected,affectedLookup={},{}
    local nonzeroGroups,maximumDistance,minimumNonzero=0,0,math.huge
    local comparedCopies,comparedPairs=0,0
    local pinnedDistance,pinnedCopies,pinnedPairs=nil,nil,nil
    local pinnedIndex=state.paint.inspectorPinned and state.paint.hoveredVertex and
        state.paint.hoveredVertex.globalIndex or nil
    for _,group in ipairs(seams.groups) do
        comparedCopies=comparedCopies+#group
        comparedPairs=comparedPairs+(#group*(#group-1))/2
        local groupMaximum=0
        for left=1,#group-1 do
            local a=cache.vertices[group[left]] and cache.vertices[group[left]].point
            for right=left+1,#group do
                local b=cache.vertices[group[right]] and cache.vertices[group[right]].point
                if a and b then
                    local distance=math.sqrt((a.x-b.x)^2+(a.y-b.y)^2+(a.z-b.z)^2)
                    groupMaximum=math.max(groupMaximum,distance)
                end
            end
        end
        local containsPinned=false
        if pinnedIndex then
            for _,index in ipairs(group) do
                if index==pinnedIndex then containsPinned=true break end
            end
        end
        if containsPinned then
            pinnedDistance=groupMaximum
            pinnedCopies=#group
            pinnedPairs=(#group*(#group-1))/2
        end
        if groupMaximum>0 then
            nonzeroGroups=nonzeroGroups+1
            maximumDistance=math.max(maximumDistance,groupMaximum)
            minimumNonzero=math.min(minimumNonzero,groupMaximum)
            for _,index in ipairs(group) do
                if not affectedLookup[index] and cache.vertices[index] then
                    affectedLookup[index]=true
                    affected[#affected+1]=cache.vertices[index]
                end
            end
        end
    end
    if minimumNonzero==math.huge then minimumNonzero=0 end
    state.paint.exactSeamPositionAudit={totalGroups=#seams.groups,
        nonzeroGroups=nonzeroGroups,affectedVertices=#affected,
        comparedCopies=comparedCopies,comparedPairs=comparedPairs,
        minimumNonzero=minimumNonzero,maximumDistance=maximumDistance,
        tolerance=seams.tolerance or 0,pinnedDistance=pinnedDistance,
        pinnedCopies=pinnedCopies,pinnedPairs=pinnedPairs}
    if #affected>0 then
        local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
            state.meshBounds.maxY-state.meshBounds.minY,
            state.meshBounds.maxZ-state.meshBounds.minZ) or 1
        state.paint.exactSeamPositionMarkers=buildVertexMarkers(affected,1,0.1,0.1,extent)
        if state.paint.exactSeamPositionMarkers then
            state.paint.exactSeamPositionMarkers.alwaysRender=true
            state.paint.exactSeamPositionMarkers.alwaysOnTop=true
            state.paint.exactSeamPositionMarkers.alwaysOnTopPriority=1
        end
    end
    applyWorkspaceVisibility()
    return state.paint.exactSeamPositionAudit
end

local function analyzeWeightHealth()
    state.paint.weightHealthSummary=nil
    state.paint.weightHealthSeamConfirmed=false
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return nil end
    local knownBones={}
    for _,bone in ipairs(getBones()) do knownBones[bone.name]=true end
    local maps,counts={}, {0,0,0,0}
    local total,unweighted,dominantTotal,dominantMinimum,dominantMaximum=0,0,0,math.huge,0
    local invalidWeightVertices=0
    local weakVertices,weakInfluences,weakWeight,weakMaximum=0,0,0,0
    for _,vertex in ipairs(cache.vertices) do
        local index=vertex.globalIndex
        local map=readInfluenceMap(index,false)
        maps[index]=map
        local active,dominant,vertexWeakCount,vertexWeakWeight,sum,unknown=0,0,0,0,0,false
        for name,weight in pairs(map) do
            if weight>0 then
                active=active+1
                sum=sum+weight
                if not knownBones[name] then unknown=true end
                dominant=math.max(dominant,weight)
                if weight<state.paint.cleanThreshold then
                    vertexWeakCount=vertexWeakCount+1
                    vertexWeakWeight=vertexWeakWeight+weight
                    weakMaximum=math.max(weakMaximum,weight)
                end
            end
        end
        total=total+1
        if active==0 then
            unweighted=unweighted+1
        else
            if active<=4 then counts[active]=counts[active]+1 end
            dominantTotal=dominantTotal+dominant
            dominantMinimum=math.min(dominantMinimum,dominant)
            dominantMaximum=math.max(dominantMaximum,dominant)
        end
        if active==0 or active>4 or unknown or math.abs(sum-1)>1e-4 then
            invalidWeightVertices=invalidWeightVertices+1
        end
        if vertexWeakCount>0 then weakVertices=weakVertices+1 end
        weakInfluences=weakInfluences+vertexWeakCount
        weakWeight=weakWeight+vertexWeakWeight
    end
    local abruptEdges,abruptVertices,abruptMaximum=0,{},0
    for index,neighbors in pairs(buildTopologyAdjacency()) do
        for neighbor in pairs(neighbors) do
            if index<neighbor then
                local distance=influenceDistance(maps[index] or {},maps[neighbor] or {})
                abruptMaximum=math.max(abruptMaximum,distance)
                if distance>=state.paint.abruptThreshold then
                    abruptEdges=abruptEdges+1
                    abruptVertices[index]=true
                    abruptVertices[neighbor]=true
                end
            end
        end
    end
    local abruptVertexCount=0
    for _ in pairs(abruptVertices) do abruptVertexCount=abruptVertexCount+1 end
    local seamAudit=analyzeGlobalSeamWeights() or {groupCount=0,totalGroups=0,
        vertexCount=0,maximumDivergence=0}
    local weighted=total-unweighted
    state.paint.weightHealthSummary={
        canonicalValid=state.bindReport and state.bindReport.valid==true and
            invalidWeightVertices==0,
        total=total,unweighted=unweighted,invalidWeightVertices=invalidWeightVertices,
        counts=counts,
        dominantMinimum=weighted>0 and dominantMinimum or 0,
        dominantAverage=weighted>0 and dominantTotal/weighted or 0,
        dominantMaximum=dominantMaximum,
        weakVertices=weakVertices,weakInfluences=weakInfluences,
        weakWeight=weakWeight,weakMaximum=weakMaximum,
        cleanThreshold=state.paint.cleanThreshold,
        abruptEdges=abruptEdges,abruptVertices=abruptVertexCount,
        abruptMaximum=abruptMaximum,abruptThreshold=state.paint.abruptThreshold,
        seamGroups=seamAudit.groupCount,seamTotal=seamAudit.totalGroups,
        seamVertices=seamAudit.vertexCount,seamMaximum=seamAudit.maximumDivergence}
    return state.paint.weightHealthSummary
end

local function constrainedRepairWeights(original,candidate,allowedNames,maxChange)
    local filtered={}
    for name,weight in pairs(candidate or {}) do
        if allowedNames[name] and weight>0 and weight==weight and weight<math.huge then
            filtered[name]=weight
        end
    end
    local normalized=normalizedInfluences(filtered)
    if #normalized==0 then return original,0 end
    local normalizedMap={}
    for _,influence in ipairs(normalized) do normalizedMap[influence.name]=influence.weight end
    local distance=influenceDistance(original,normalizedMap)
    local limit=math.max(0,math.min(1,maxChange or 0))
    if distance<=limit+1e-9 then return normalizedMap,distance end
    local function blendAt(alpha)
        local blended={}
        for name,weight in pairs(original) do blended[name]=weight*(1-alpha) end
        for name,weight in pairs(normalizedMap) do
            blended[name]=(blended[name] or 0)+weight*alpha
        end
        local limited=normalizedInfluences(blended)
        local limitedMap={}
        for _,influence in ipairs(limited) do limitedMap[influence.name]=influence.weight end
        return limitedMap,influenceDistance(original,limitedMap)
    end
    local low,high=0,math.min(1,limit/distance)
    local bestMap,bestDistance=original,0
    for _=1,24 do
        local middle=(low+high)*0.5
        local trialMap,trialDistance=blendAt(middle)
        if trialDistance<=limit+1e-9 then
            low=middle; bestMap,bestDistance=trialMap,trialDistance
        else
            high=middle
        end
    end
    return bestMap,bestDistance
end

poseSafeRepairScale = function(original,candidates,editable,diagnosticOnly)
    if not state.meshD or not state.preview or not next(candidates) then
        return 1,0,{},0,0,1,0,1
    end
    local duration=state.preview:getSkeletalAnimationDuration(state.skeletalPreview.selected) or 0
    if duration<=0 then return 1,0,{},0,0,1,0,1 end
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return 1,0,{},0,0,1,0,1 end
    local incident,incidentLookup={},{}
    for index in pairs(editable) do
        for _,triangle in ipairs(cache.incidentTriangles[index] or {}) do
            if not incidentLookup[triangle] then
                incidentLookup[triangle]=true
                incident[#incident+1]=triangle
            end
        end
    end
    if #incident==0 then return 1,0,{},0,0,1,0,1 end
    local boneIndex={}
    for index,bone in ipairs(getBones()) do boneIndex[bone.name]=index-1 end
    local samples={0,duration*0.25,duration*0.5,duration*0.75,duration}
    local poses={}
    for _,time in ipairs(samples) do
        local ok,pose=safeCall(function()
            return state.meshD:evaluateSkeletalAuthoringPose(
                state.skeletalPreview.selected,time,'lbs')
        end)
        if not ok or not pose or not pose.palette then return 1,0,{},0,0,1,0,1 end
        poses[#poses+1]=pose.palette
    end
    local function originalMap(index)
        if not original[index] then original[index]=readInfluenceMap(index,false) end
        return original[index]
    end
    local function blendedMap(index,scale)
        local candidate=candidates[index]
        if not candidate then return originalMap(index) end
        if scale>=1 then return candidate end
        local mixed={}
        for name,weight in pairs(originalMap(index)) do mixed[name]=weight*(1-scale) end
        for name,weight in pairs(candidate) do
            mixed[name]=(mixed[name] or 0)+weight*scale
        end
        local result={}
        for _,influence in ipairs(normalizedInfluences(mixed)) do
            result[influence.name]=influence.weight
        end
        return result
    end
    local function deform(point,weights,palette)
        local x,y,z=0,0,0
        for name,weight in pairs(weights) do
            local index=boneIndex[name]
            if index then
                local first=index*12
                x=x+(point.x*palette[first+1]+point.y*palette[first+2]+
                    point.z*palette[first+3]+palette[first+4])*weight
                y=y+(point.x*palette[first+5]+point.y*palette[first+6]+
                    point.z*palette[first+7]+palette[first+8])*weight
                z=z+(point.x*palette[first+9]+point.y*palette[first+10]+
                    point.z*palette[first+11]+palette[first+12])*weight
            end
        end
        return {x=x,y=y,z=z}
    end
    local function deformVector(vector,weights,palette)
        local x,y,z=0,0,0
        for name,weight in pairs(weights) do
            local index=boneIndex[name]
            if index then
                local first=index*12
                x=x+(vector.x*palette[first+1]+vector.y*palette[first+2]+
                    vector.z*palette[first+3])*weight
                y=y+(vector.x*palette[first+5]+vector.y*palette[first+6]+
                    vector.z*palette[first+7])*weight
                z=z+(vector.x*palette[first+9]+vector.y*palette[first+10]+
                    vector.z*palette[first+11])*weight
            end
        end
        return {x=x,y=y,z=z}
    end
    local function areaVector(a,b,c)
        local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
        local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
        return {x=uy*vz-uz*vy,y=uz*vx-ux*vz,z=ux*vy-uy*vx}
    end
    local function safeAt(scale,countFailures,collectTriangles)
        local failures=0
        local failedTriangles,failedLookup={},{}
        local minimumAreaRatio=math.huge
        local maximumOrientationDegrees=0
        local minimumNormalAlignment=1
        for _,palette in ipairs(poses) do
            local beforeCache,afterCache={},{ }
            for _,triangle in ipairs(incident) do
                local before,after={},{}
                for slot,vertex in ipairs({triangle.a,triangle.b,triangle.c}) do
                    local index=vertex.globalIndex
                    if not beforeCache[index] then
                        beforeCache[index]=deform(vertex.point,originalMap(index),palette)
                        afterCache[index]=deform(vertex.point,blendedMap(index,scale),palette)
                    end
                    before[slot],after[slot]=beforeCache[index],afterCache[index]
                end
                local oldArea=areaVector(before[1],before[2],before[3])
                local newArea=areaVector(after[1],after[2],after[3])
                local bindArea=areaVector(triangle.a.point,triangle.b.point,triangle.c.point)
                local oldLength=math.sqrt(oldArea.x^2+oldArea.y^2+oldArea.z^2)
                local newLength=math.sqrt(newArea.x^2+newArea.y^2+newArea.z^2)
                if oldLength>1e-10 then
                    local orientation=oldArea.x*newArea.x+oldArea.y*newArea.y+oldArea.z*newArea.z
                    local areaRatio=newLength/oldLength
                    minimumAreaRatio=math.min(minimumAreaRatio,areaRatio)
                    if newLength>1e-10 then
                        local cosine=math.max(-1,math.min(1,orientation/(oldLength*newLength)))
                        maximumOrientationDegrees=math.max(maximumOrientationDegrees,
                            math.acos(cosine)*180/math.pi)
                    else
                        maximumOrientationDegrees=180
                    end
                    local expected,expectedBefore={x=0,y=0,z=0},{x=0,y=0,z=0}
                    for _,vertex in ipairs({triangle.a,triangle.b,triangle.c}) do
                        local transformed=deformVector(bindArea,
                            blendedMap(vertex.globalIndex,scale),palette)
                        local transformedBefore=deformVector(bindArea,
                            originalMap(vertex.globalIndex),palette)
                        expected.x=expected.x+transformed.x
                        expected.y=expected.y+transformed.y
                        expected.z=expected.z+transformed.z
                        expectedBefore.x=expectedBefore.x+transformedBefore.x
                        expectedBefore.y=expectedBefore.y+transformedBefore.y
                        expectedBefore.z=expectedBefore.z+transformedBefore.z
                    end
                    local expectedLength=math.sqrt(expected.x^2+expected.y^2+expected.z^2)
                    local expectedBeforeLength=math.sqrt(expectedBefore.x^2+
                        expectedBefore.y^2+expectedBefore.z^2)
                    local normalAlignment,normalAlignmentBefore=1,1
                    if newLength>1e-10 and expectedLength>1e-10 then
                        normalAlignment=(newArea.x*expected.x+newArea.y*expected.y+
                            newArea.z*expected.z)/(newLength*expectedLength)
                        normalAlignment=math.max(-1,math.min(1,normalAlignment))
                        minimumNormalAlignment=math.min(minimumNormalAlignment,normalAlignment)
                    end
                    if oldLength>1e-10 and expectedBeforeLength>1e-10 then
                        normalAlignmentBefore=(oldArea.x*expectedBefore.x+
                            oldArea.y*expectedBefore.y+oldArea.z*expectedBefore.z)/
                            (oldLength*expectedBeforeLength)
                    end
                    local introducedInversion=normalAlignmentBefore>0 and normalAlignment<=0
                    if newLength<oldLength*0.25 or introducedInversion then
                        failures=failures+1
                        if collectTriangles and not failedLookup[triangle] then
                            failedLookup[triangle]=true
                            failedTriangles[#failedTriangles+1]=triangle
                        end
                        if not countFailures then return false,failures end
                    end
                end
            end
        end
        if minimumAreaRatio==math.huge then minimumAreaRatio=1 end
        return failures==0,failures,failedTriangles,minimumAreaRatio,
            maximumOrientationDegrees,minimumNormalAlignment
    end
    local fullSafe,fullFailures,failedTriangles,minimumAreaRatio,maximumOrientationDegrees,
        minimumNormalAlignment=safeAt(1,true,true)
    if diagnosticOnly then
        return 1,fullFailures,failedTriangles,#incident,#poses,minimumAreaRatio,
            maximumOrientationDegrees,minimumNormalAlignment
    end
    if fullSafe then
        return 1,0,{},#incident,#poses,minimumAreaRatio,maximumOrientationDegrees,
            minimumNormalAlignment
    end
    local low,high=0,1
    for _=1,12 do
        local middle=(low+high)*0.5
        if safeAt(middle,false) then low=middle else high=middle end
    end
    return low,fullFailures,failedTriangles,#incident,#poses,minimumAreaRatio,
        maximumOrientationDegrees,minimumNormalAlignment
end

rebuildPaintStrokeSafetyOverlay = function(unsafeTriangles,report)
    destroyObject(state.paint.strokeSafetyFaceShape)
    state.paint.strokeSafetyFaceShape=nil
    state.paint.strokeSafetyReport=report
    local vertices={}
    for _,triangle in ipairs(unsafeTriangles or {}) do
        for _,entry in ipairs({triangle.a,triangle.b,triangle.c,
                triangle.a,triangle.c,triangle.b}) do
            appendPoint(vertices,entry.point.x,entry.point.y,entry.point.z)
        end
    end
    if #vertices>0 then
        local overlay=shape:new('3d',0,0,0)
        local nickname='paint_stroke_safety_faces_'..state.paint.heatmapGeneration
        if overlay:create(vertices,nil,nickname) then
            overlay:setColor(1,0.05,0.02,0.42)
            overlay:setPos(0,0,0)
            overlay.alwaysOnTop=true
            overlay.alwaysOnTopPriority=0
            state.paint.strokeSafetyFaceShape=overlay
        else
            destroyObject(overlay)
        end
    end
    applyWorkspaceVisibility()
end

local function rebuildPaintSafetyOverlay(unsafeTriangles,seamVertices,report)
    destroyObject(state.paint.safetyFaceShape)
    destroyObject(state.paint.safetySeamMarkers)
    state.paint.safetyFaceShape=nil
    state.paint.safetySeamMarkers=nil
    state.paint.safetyReport=report
    local vertices={}
    for _,triangle in ipairs(unsafeTriangles or {}) do
        for _,entry in ipairs({triangle.a,triangle.b,triangle.c,
                triangle.a,triangle.c,triangle.b}) do
            appendPoint(vertices,entry.point.x,entry.point.y,entry.point.z)
        end
    end
    if #vertices>0 then
        local overlay=shape:new('3d',0,0,0)
        local nickname='paint_safety_faces_'..state.paint.heatmapGeneration
        if overlay:create(vertices,nil,nickname) then
            overlay:setColor(1,0.05,0.02,0.38)
            overlay:setPos(0,0,0)
            state.paint.safetyFaceShape=overlay
        else
            destroyObject(overlay)
        end
    end
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    state.paint.safetySeamMarkers=buildVertexMarkers(seamVertices or {},0,1,1,extent)
    for _,object in ipairs({state.paint.safetyFaceShape,state.paint.safetySeamMarkers}) do
        if object then
            object.alwaysOnTop=state.markersAlwaysOnTop
            object.visible=state.workspace=='paint' and
                state.paint.visualizationMode==4 and state.paint.safetyOverlayVisible
        end
    end
end

local function repairPaintAbruptTransitions()
    local stats=state.paint.abruptStats
    if not stats or not stats.records then return false end
    local editable={}
    for _,record in ipairs(stats.records) do
        if record.distance>=state.paint.abruptThreshold then
            editable[record.a]=true; editable[record.b]=true
        end
    end
    local indices={}
    for index in pairs(editable) do indices[#indices+1]=index end
    table.sort(indices)
    if #indices==0 then
        setStatus(tLang.L('swl_paint_abrupt_repair_no_change'),false)
        return false
    end
    local adjacency=buildTopologyAdjacency()
    local current,original,allowedNames={},{},{}
    for _,index in ipairs(indices) do
        local weights=readInfluenceMap(index,false)
        current[index]=weights; original[index]=weights
        allowedNames[index]={}
        for name in pairs(weights) do allowedNames[index][name]=true end
        for neighbor in pairs(adjacency[index] or {}) do
            if not current[neighbor] then current[neighbor]=readInfluenceMap(neighbor,false) end
            for name in pairs(current[neighbor]) do allowedNames[index][name]=true end
        end
    end
    local strength=math.max(0,math.min(1,state.paint.abruptRepairStrength))
    for _=1,math.max(1,state.paint.abruptRepairIterations) do
        local nextWeights={}
        for _,index in ipairs(indices) do
            local average,count={},0
            for neighbor in pairs(adjacency[index] or {}) do
                count=count+1
                for name,weight in pairs(current[neighbor] or {}) do
                    average[name]=(average[name] or 0)+weight
                end
            end
            local mixed={}
            for name,weight in pairs(current[index] or {}) do
                mixed[name]=weight*(1-strength)
            end
            if count>0 then
                for name,weight in pairs(average) do
                    mixed[name]=(mixed[name] or 0)+weight/count*strength
                end
            end
            local normalized=normalizedInfluences(mixed)
            local weightMap={}
            for _,influence in ipairs(normalized) do weightMap[influence.name]=influence.weight end
            nextWeights[index]=weightMap
        end
        for index,weights in pairs(nextWeights) do current[index]=weights end
    end
    local candidateMaps={}
    local maximumAppliedChange=0
    for _,index in ipairs(indices) do
        local constrained,appliedChange=constrainedRepairWeights(original[index] or {},
            current[index] or {},allowedNames[index] or {},state.paint.abruptRepairMaxChange)
        maximumAppliedChange=math.max(maximumAppliedChange,appliedChange)
        if appliedChange>1e-7 then candidateMaps[index]=constrained end
    end
    local seams=buildCoincidentSeams(adjacency)
    local synchronizedSeams,conflictingSeams=0,0
    local seamVertices,seamVertexLookup={},{}
    for _,group in ipairs(seams.groups) do
        local touched=false
        for _,index in ipairs(group) do
            if candidateMaps[index] then touched=true break end
        end
        if touched then
            local compatible=true
            local base=original[group[1]] or readInfluenceMap(group[1],false)
            for _,index in ipairs(group) do
                original[index]=original[index] or readInfluenceMap(index,false)
                if influenceDistance(base,original[index])>1e-4 then compatible=false end
            end
            if compatible then
                local average,allowed={},{ }
                for _,index in ipairs(group) do
                    local source=candidateMaps[index] or original[index]
                    for name,weight in pairs(source) do
                        average[name]=(average[name] or 0)+weight/#group
                        allowed[name]=true
                    end
                end
                local common=select(1,constrainedRepairWeights(base,average,allowed,
                    state.paint.abruptRepairMaxChange))
                for _,index in ipairs(group) do
                    candidateMaps[index]=common
                    editable[index]=true
                    if not seamVertexLookup[index] then
                        seamVertexLookup[index]=true
                        seamVertices[#seamVertices+1]=(state.paint.geometry or
                            buildPaintGeometryCache()).vertices[index]
                    end
                end
                synchronizedSeams=synchronizedSeams+1
            else
                conflictingSeams=conflictingSeams+1
            end
        end
    end
    indices={}
    for index in pairs(candidateMaps) do indices[#indices+1]=index end
    table.sort(indices)
    local poseScale,protectedFaces,unsafeTriangles=
        poseSafeRepairScale(original,candidateMaps,editable)
    local edits={}
    maximumAppliedChange=0
    for _,index in ipairs(indices) do
        local candidate=candidateMaps[index]
        if candidate then
            local finalMap=candidate
            if poseScale<0.999999 then
                local mixed={}
                for name,weight in pairs(original[index] or {}) do
                    mixed[name]=weight*(1-poseScale)
                end
                for name,weight in pairs(candidate) do
                    mixed[name]=(mixed[name] or 0)+weight*poseScale
                end
                finalMap={}
                for _,influence in ipairs(normalizedInfluences(mixed)) do
                    finalMap[influence.name]=influence.weight
                end
            end
            local appliedChange=influenceDistance(original[index] or {},finalMap)
            maximumAppliedChange=math.max(maximumAppliedChange,appliedChange)
            local influences=normalizedInfluences(finalMap)
            if #influences>0 and
                    influenceDistance(original[index] or {},finalMap)>1e-7 then
                local row={index}
                for slot=1,4 do
                    local influence=influences[slot]
                    row[slot*2]=influence and influence.name or nil
                    row[slot*2+1]=influence and influence.weight or 0
                end
                edits[#edits+1]=row
            end
        end
    end
    if #edits==0 then
        setStatus(tLang.L('swl_paint_abrupt_repair_no_change'),false)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_repair_abrupt_weights')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(snapshot)
        if ok then setStatus(tLang.L('swl_paint_abrupt_repair_failed'),true) end
        return false
    end
    local beforeEdges=stats.edges
    commitRollbackSnapshot(snapshot,'swl_history_repair_abrupt_weights')
    state.modified=true
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    applyWorkspaceVisibility()
    local afterEdges=state.paint.abruptStats and state.paint.abruptStats.edges or 0
    rebuildPaintSafetyOverlay(unsafeTriangles,seamVertices,{poseScale=poseScale,
        unsafeFaceSamples=protectedFaces,unsafeFaces=#unsafeTriangles,
        synchronizedSeams=synchronizedSeams,seamVertices=#seamVertices,
        conflictingSeams=conflictingSeams})
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_abrupt_repaired_fmt'),#edits,beforeEdges,
        afterEdges,maximumAppliedChange,poseScale,protectedFaces,synchronizedSeams,
        conflictingSeams),false,true)
    return true
end

local function smoothPaintMaskFullVector()
    local indices,editable={},{}
    for index in pairs(state.paint.maskVertices) do
        if state.paint.geometry and state.paint.geometry.vertices[index] then
            indices[#indices+1]=index
            editable[index]=true
        end
    end
    table.sort(indices)
    if #indices==0 then
        setStatus(tLang.L('swl_paint_mask_smooth_empty'),false)
        return false
    end
    local adjacency=buildTopologyAdjacency()
    local seams=buildCoincidentSeams(adjacency)
    local original,current={},{ }
    for _,index in ipairs(indices) do
        original[index]=readInfluenceMap(index,false)
        current[index]=original[index]
    end
    local function eachMaskedNeighbor(index,callback)
        local seen={}
        for neighbor in pairs(adjacency[index] or {}) do
            if editable[neighbor] then seen[neighbor]=true; callback(neighbor) end
        end
        for _,neighbor in ipairs(seams.byVertex[index] or {}) do
            if neighbor~=index and editable[neighbor] and not seen[neighbor] then
                seen[neighbor]=true; callback(neighbor)
            end
        end
    end
    local strength=math.max(0,math.min(1,state.paint.maskSmoothStrength))
    for _=1,math.max(1,state.paint.maskSmoothIterations) do
        local nextWeights={}
        for _,index in ipairs(indices) do
            local average,count={},0
            eachMaskedNeighbor(index,function(neighbor)
                count=count+1
                for name,weight in pairs(current[neighbor] or {}) do
                    average[name]=(average[name] or 0)+weight
                end
            end)
            local mixed={}
            for name,weight in pairs(current[index] or {}) do
                mixed[name]=weight*(1-strength)
            end
            if count>0 then
                for name,weight in pairs(average) do
                    mixed[name]=(mixed[name] or 0)+weight/count*strength
                end
            else
                mixed=current[index] or {}
            end
            local normalized=normalizedInfluences(mixed)
            local weightMap={}
            for _,influence in ipairs(normalized) do weightMap[influence.name]=influence.weight end
            nextWeights[index]=weightMap
        end
        current=nextWeights
    end
    local candidateMaps={}
    for _,index in ipairs(indices) do
        if influenceDistance(original[index] or {},current[index] or {})>1e-7 then
            candidateMaps[index]=current[index]
        end
    end
    local poseScale,protectedFaces,unsafeTriangles=
        poseSafeRepairScale(original,candidateMaps,editable)
    local edits={}
    for _,index in ipairs(indices) do
        local candidate=candidateMaps[index]
        if candidate then
            local finalMap=candidate
            if poseScale<0.999999 then
                local mixed={}
                for name,weight in pairs(original[index] or {}) do
                    mixed[name]=weight*(1-poseScale)
                end
                for name,weight in pairs(candidate) do
                    mixed[name]=(mixed[name] or 0)+weight*poseScale
                end
                finalMap={}
                for _,influence in ipairs(normalizedInfluences(mixed)) do
                    finalMap[influence.name]=influence.weight
                end
            end
            local influences=normalizedInfluences(finalMap)
            if #influences>0 then
                local row={index}
                for slot=1,4 do
                    local influence=influences[slot]
                    row[slot*2]=influence and influence.name or nil
                    row[slot*2+1]=influence and influence.weight or 0
                end
                edits[#edits+1]=row
            end
        end
    end
    if #edits==0 then
        setStatus(tLang.L(poseScale<=1e-6 and 'swl_paint_mask_smooth_safety_blocked' or
            'swl_paint_mask_smooth_no_change'),false)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_smooth_mask_weights')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(snapshot)
        if ok then setStatus(tLang.L('swl_paint_mask_smooth_failed'),true) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_smooth_mask_weights')
    state.modified=true
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_mask_smoothed_fmt'),#edits,
        state.paint.maskSmoothIterations,poseScale,protectedFaces),false,true)
    return true
end

local function rigidBindPaintMask()
    local bone=getBones()[state.paint.boneIndex]
    if not bone then return false end
    local indices,editable={},{}
    for index in pairs(state.paint.maskVertices) do
        if state.paint.geometry and state.paint.geometry.vertices[index] then
            indices[#indices+1]=index; editable[index]=true
        end
    end
    table.sort(indices)
    if #indices==0 then
        setStatus(tLang.L('swl_paint_mask_rigid_empty'),false)
        return false
    end
    local adjacency=buildTopologyAdjacency()
    local seams=buildCoincidentSeams(adjacency)
    local function eachNeighbor(index,callback)
        local seen={}
        for neighbor in pairs(adjacency[index] or {}) do
            seen[neighbor]=true; callback(neighbor)
        end
        for _,neighbor in ipairs(seams.byVertex[index] or {}) do
            if neighbor~=index and not seen[neighbor] then
                seen[neighbor]=true; callback(neighbor)
            end
        end
    end
    local distance,queue={},{}
    for _,index in ipairs(indices) do
        local boundary=false
        eachNeighbor(index,function(neighbor)
            if not editable[neighbor] then boundary=true end
        end)
        if boundary then distance[index]=0; queue[#queue+1]=index end
    end
    local head=1
    while head<=#queue do
        local index=queue[head]; head=head+1
        eachNeighbor(index,function(neighbor)
            if editable[neighbor] and distance[neighbor]==nil then
                distance[neighbor]=distance[index]+1
                queue[#queue+1]=neighbor
            end
        end)
    end
    local rings=math.max(0,state.paint.maskRigidTransitionRings)
    local original,candidateMaps={},{ }
    for _,index in ipairs(indices) do
        local before=readInfluenceMap(index,false)
        original[index]=before
        local alpha=1
        if rings>0 and distance[index]~=nil then
            alpha=math.min(1,(distance[index]+1)/(rings+1))
        end
        local mixed={}
        for name,weight in pairs(before) do mixed[name]=weight*(1-alpha) end
        mixed[bone.name]=(mixed[bone.name] or 0)+alpha
        local result={}
        for _,influence in ipairs(normalizedInfluences(mixed)) do
            result[influence.name]=influence.weight
        end
        if influenceDistance(before,result)>1e-7 then candidateMaps[index]=result end
    end
    local poseScale,protectedSamples=poseSafeRepairScale(original,candidateMaps,editable)
    local edits={}
    for _,index in ipairs(indices) do
        local candidate=candidateMaps[index]
        if candidate then
            local finalMap=candidate
            if poseScale<0.999999 then
                local mixed={}
                for name,weight in pairs(original[index]) do
                    mixed[name]=weight*(1-poseScale)
                end
                for name,weight in pairs(candidate) do
                    mixed[name]=(mixed[name] or 0)+weight*poseScale
                end
                finalMap={}
                for _,influence in ipairs(normalizedInfluences(mixed)) do
                    finalMap[influence.name]=influence.weight
                end
            end
            local influences=normalizedInfluences(finalMap)
            if #influences>0 and influenceDistance(original[index],finalMap)>1e-7 then
                local row={index}
                for slot=1,4 do
                    local influence=influences[slot]
                    row[slot*2]=influence and influence.name or nil
                    row[slot*2+1]=influence and influence.weight or 0
                end
                edits[#edits+1]=row
            end
        end
    end
    if #edits==0 then
        setStatus(tLang.L(poseScale<=1e-6 and 'swl_paint_mask_rigid_safety_blocked' or
            'swl_paint_mask_rigid_no_change'),false)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_rigid_mask_weights')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(snapshot)
        if ok then setStatus(tLang.L('swl_paint_mask_rigid_failed'),true) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_rigid_mask_weights')
    state.modified=true
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_mask_rigid_applied_fmt'),#edits,bone.name,
        rings,poseScale,protectedSamples),false,true)
    return true
end

local function restoreHistoryEntry(entry)
    if not entry or not entry.path then return false end
    local restored = meshDebug:new()
    if not restored:load(entry.path) then
        setStatus(tLang.L('swl_revert_failed'), true)
        return false
    end
    state.meshD = restored
    clearPaintVisuals()
    state.topologyAdjacency=nil
    state.coincidentSeams=nil
    state.paint.geometry=nil
    state.paint.heatmapDirty=true
    state.animationReport=nil
    state.animationTimelineClip=nil
    state.animationTimelineSelection={}
    state.authoringPose=nil
    state.authoringPoseKey=nil
    state.authoringOverride=nil
    state.authoringActiveClip=nil
    state.modified = entry.modified==true
    state.workspace=entry.workspace=='weights' and 'paint' or (entry.workspace or state.workspace)
    state.boneIndex=entry.boneIndex or state.boneIndex
    state.animationClipSelected=entry.clipIndex or state.animationClipSelected
    state.authoringTime=entry.authoringTime or 0
    state.bindRenameBoneId=nil
    state.bindReparentBoneId=nil
    state.bindEditBoneId=nil
    refreshBindReport()
    local bones=getBones()
    state.boneIndex=math.max(1,math.min(state.boneIndex,#bones))
    if state.workspace=='paint' then state.paint.boneIndex=state.boneIndex end
    state.allowedBones={}
    for _,bone in ipairs(bones) do state.allowedBones[bone.name]=true end
    rebuildPreview(entry.path)
    buildPaintGeometryCache()
    rebuildPaintMaskMarkers()
    if state.workspace=='paint' then rebuildPaintHeatmap() end
    rebuildSkeletonVisuals()
    applyWorkspaceVisibility()
    return true
end

local function undoHistory()
    if #state.undoStack==0 then return end
    local current=stageRollbackSnapshot()
    if not current then setStatus(tLang.L('swl_snapshot_failed'),true); return end
    local entry=table.remove(state.undoStack)
    current.descriptionKey=entry.descriptionKey
    current.description=entry.description
    if restoreHistoryEntry(entry) then
        state.redoStack[#state.redoStack+1]=current
        trimHistoryStack(state.redoStack)
        pcall(os.remove,entry.path)
        showHistoryFeedback(string.format(tLang.L('swl_undone_fmt'),historyDescription(entry)))
    else
        state.undoStack[#state.undoStack+1]=entry
        discardRollbackSnapshot(current)
    end
end

local function redoHistory()
    if #state.redoStack==0 then return end
    local current=stageRollbackSnapshot()
    if not current then setStatus(tLang.L('swl_snapshot_failed'),true); return end
    local entry=table.remove(state.redoStack)
    current.descriptionKey=entry.descriptionKey
    current.description=entry.description
    if restoreHistoryEntry(entry) then
        state.undoStack[#state.undoStack+1]=current
        trimHistoryStack(state.undoStack)
        pcall(os.remove,entry.path)
        showHistoryFeedback(string.format(tLang.L('swl_redone_fmt'),historyDescription(entry)))
    else
        state.redoStack[#state.redoStack+1]=entry
        discardRollbackSnapshot(current)
    end
end

local function showRollbackControls(id)
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_history'))
    local undoEntry=state.undoStack[#state.undoStack]
    local redoEntry=state.redoStack[#state.redoStack]
    tImGui.BeginDisabled(undoEntry==nil)
    if tImGui.Button(string.format(tLang.L('swl_undo_fmt'),
            historyDescription(undoEntry))..'##undo'..id) then undoHistory() end
    tImGui.EndDisabled()
    tImGui.SameLine()
    tImGui.BeginDisabled(redoEntry==nil)
    if tImGui.Button(string.format(tLang.L('swl_redo_fmt'),
            historyDescription(redoEntry))..'##redo'..id) then redoHistory() end
    tImGui.EndDisabled()
end

local function saveTo(path)
    if not state.meshD or not path or path == '' then return false end
    local okCheck, valid, message = safeCall(function() return state.meshD:check() end)
    if not okCheck or not valid then
        setStatus(message or tLang.L('swl_check_failed'), true)
        return false
    end
    if state.meshD:save(path, false, false) then
        state.fileName = path
        state.modified = false
        rebuildPreview()
        setStatus(string.format(tLang.L('swl_saved_fmt'), shortName(path)), false)
        return true
    end
    setStatus(string.format(tLang.L('swl_save_failed_fmt'), path), true)
    return false
end

local function showMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(tLang.L('swl_open_mesh'), 'Ctrl+O') then
            local path = mbm.openFile(state.fileName or '', 'msh')
            if path then loadMesh(path) end
        end
        if tImGui.MenuItem(tLang.L('swl_save'), 'Ctrl+S', false, state.meshD ~= nil) then
            saveTo(state.fileName)
        end
        if tImGui.MenuItem(tLang.L('swl_save_as'), nil, false, state.meshD ~= nil) then
            local path = mbm.saveFile(state.fileName or 'skin-weight-lab.msh', 'msh')
            if path then saveTo(path) end
        end
        tImGui.Separator()
        if tImGui.MenuItem(tLang.L('menu_quit'), 'Ctrl+Q') then
            clearRollback()
            mbm.quit()
        end
        tImGui.EndMenu()
    end
    local openedTutorial=tTutorials.renderMenu(tImGui,tLang)
    if openedTutorial and openedTutorial.assetFactory=='worm_cylinder' then
        local path,err,info=tTutorialAssets.createWormCylinder(tUtil)
        if path and loadMesh(path) then
            setStatus(string.format(tLang.L('swl_tutorial_2_asset_loaded_fmt'),
                info.vertices,info.triangles),false,true)
        else
            setStatus(string.format(tLang.L('swl_tutorial_2_asset_failed_fmt'),
                tostring(err or path or 'unknown error')),true,true)
        end
    end
    if tImGui.BeginMenu(tLang.L('menu_edit')) then
        local undoEntry=state.undoStack[#state.undoStack]
        local redoEntry=state.redoStack[#state.redoStack]
        if tImGui.MenuItem(string.format(tLang.L('swl_undo_fmt'),
                historyDescription(undoEntry)),'Ctrl+Z',false,undoEntry~=nil) then
            undoHistory()
        end
        if tImGui.MenuItem(string.format(tLang.L('swl_redo_fmt'),
                historyDescription(redoEntry)),'Ctrl+Y',false,redoEntry~=nil) then
            redoHistory()
        end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        local pressed,showAdvanced=tImGui.MenuItem(
            tLang.L('swl_show_advanced_diagnostics'),nil,state.showAdvancedDiagnostics)
        if pressed then
            state.showAdvancedDiagnostics=showAdvanced
            if not showAdvanced then
                state.paint.inspectorClick=nil
                rebuildPaintCursor(nil)
            end
            applyWorkspaceVisibility()
        end
        tImGui.Separator()
        tLang.renderLanguageSubmenu()
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

showItemTooltip=function(text,allowWhenDisabled)
    local flags=allowWhenDisabled and
        tImGui.Flags('ImGuiHoveredFlags_AllowWhenDisabled') or 0
    if tImGui.IsItemHovered(flags) then
        tImGui.BeginTooltip()
        -- Tooltip windows have no reliable wrap width in this ImGui binding.
        -- Localized tooltip strings use explicit line breaks instead.
        tImGui.Text(text)
        tImGui.EndTooltip()
    end
end

drawExecutionStatusIndicator=function(executionPath)
    local path=executionPath or ''
    local color=nil
    local tooltipKey=nil
    if path=='gpu' then
        color={r=0.15,g=0.82,b=0.32,a=1}
        tooltipKey='swl_execution_gpu_note'
    elseif path=='cpu' then
        color={r=1.0,g=0.55,b=0.10,a=1}
        tooltipKey='swl_execution_cpu_note'
    end
    if not color then return end

    local radius=5
    local cursor=tImGui.GetCursorScreenPos()
    local yOffset=2
    tImGui.Dummy({x=radius*2+2,y=radius*2+4})
    tImGui.AddCircleFilled({x=cursor.x+radius+1,y=cursor.y+radius+yOffset},radius,color,18)
    if tooltipKey then
        showItemTooltip(tLang.L(tooltipKey))
    end
end

local function showWrappedColoredText(text,color)
    tImGui.PushStyleColor('ImGuiCol_Text',color)
    tImGui.TextWrapped(text)
    tImGui.PopStyleColor()
end

local function showWrappedDisabledText(text)
    tImGui.BeginDisabled(true)
    tImGui.TextWrapped(text)
    tImGui.EndDisabled()
end

local function showStatusMessage()
    if not state.status then return end
    tImGui.Separator()
    if state.statusError then
        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=0.3,b=0.2,a=1})
        tImGui.TextWrapped(state.status)
        tImGui.PopStyleColor()
    elseif state.statusChanged then
        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=0.82,b=0.2,a=1})
        tImGui.TextWrapped(state.status)
        tImGui.PopStyleColor()
    else
        tImGui.TextWrapped(state.status)
    end
end

local sectionTitleColor={r=0.25,g=0.80,b=1.0,a=1}

local function showSectionTitle(key)
    tImGui.TextColored(sectionTitleColor,tLang.L(key))
end

local function formatBindMatrix(matrix)
    if not matrix or #matrix ~= 16 then return tLang.L('swl_bind_matrix_unavailable') end
    return string.format('%.5g %.5g %.5g %.5g\n%.5g %.5g %.5g %.5g\n%.5g %.5g %.5g %.5g\n%.5g %.5g %.5g %.5g',
        table.unpack(matrix))
end

local function showSelectedBindBone(report)
    local bone=report.bones and report.bones[state.boneIndex] or nil
    if not bone then return end
    if state.bindRenameBoneId~=bone.boneId then
        state.bindRenameBoneId=bone.boneId
        state.bindRenameName=bone.name or ''
    end
    if state.bindReparentBoneId~=bone.boneId then
        state.bindReparentBoneId=bone.boneId
        state.bindParentChoice=(bone.parentIndex or 0)+1
        state.bindPreserveGlobal=true
    end
    if state.bindEditBoneId~=bone.boneId then
        local translation,rotation,scale=bone.localTranslation or {},bone.localRotation or {},bone.localScale or {}
        state.bindEditBoneId=bone.boneId
        state.bindEdit={tx=translation.x or 0,ty=translation.y or 0,tz=translation.z or 0,
            qx=rotation.x or 0,qy=rotation.y or 0,qz=rotation.z or 0,qw=rotation.w or 1,
            sx=scale.x or 1,sy=scale.y or 1,sz=scale.z or 1,
            radius=bone.radius or 0,length=bone.length or 0}
    end
    if state.bindAddBoneId~=bone.boneId then
        state.bindAddBoneId=bone.boneId
        state.bindAddName=(bone.name or 'bone')..'_child'
        state.bindAddParentChoice=state.boneIndex+1
        state.bindAddTranslation={x=0,y=(bone.length or 0)>0 and bone.length or 1,z=0}
    end
    if state.bindChainBoneId~=bone.boneId then
        state.bindChainBoneId=bone.boneId
        state.bindChainPrefix=(bone.name or 'bone')..'_chain_'
        state.bindChainCount=3
        state.bindChainStep={x=0,y=(bone.length or 0)>0 and bone.length or 1,z=0}
    end
    if state.bindMirrorBoneId~=bone.boneId then
        state.bindMirrorBoneId=bone.boneId
        state.bindMirrorPrefix='mirror_'
        state.bindMirrorAxis=1
        state.bindMirrorConfirmed=false
    end
    if state.bindWeightsBoneId~=bone.boneId then
        state.bindWeightsBoneId=bone.boneId
        state.bindWeightsConfirmed=false
    end
    if state.bindRemoveBoneId~=bone.boneId then
        state.bindRemoveBoneId=bone.boneId
        state.bindRemoveConfirmed=false
        state.bindRemoveDiscardTracks=false
        state.bindRemoveReparentChildren=false
        state.bindRemoveReplacement=(bone.parentIndex or 0)>0 and bone.parentIndex or
            (state.boneIndex==1 and 2 or 1)
    end
    tImGui.Separator()
    tImGui.Text(string.format('%s: %s',tLang.L('swl_source_bone'),bone.name or '?'))
    tImGui.Text(string.format('ID: %s  Parent: %s (%d)',bone.boneId or '?',
        bone.parentBoneId or '?',bone.parentIndex or 0))
    local translation=bone.localTranslation or {}
    local rotation=bone.localRotation or {}
    local scale=bone.localScale or {}
    tImGui.Text(string.format('T %.6g %.6g %.6g',translation.x or 0,
        translation.y or 0,translation.z or 0))
    tImGui.Text(string.format('Q %.6g %.6g %.6g %.6g',rotation.x or 0,
        rotation.y or 0,rotation.z or 0,rotation.w or 1))
    tImGui.Text(string.format('S %.6g %.6g %.6g',scale.x or 1,scale.y or 1,scale.z or 1))
    tImGui.Text(string.format('Radius %.6g  Length %.6g',bone.radius or 0,bone.length or 0))
    tImGui.PushItemWidth(190)
    local changed,newName=tImGui.InputText(tLang.L('swl_bone_name')..'##swlBindRename',
        state.bindRenameName,tImGui.Flags('ImGuiInputTextFlags_None'))
    tImGui.PopItemWidth()
    if changed then state.bindRenameName=newName end
    local trimmed=(state.bindRenameName or ''):match('^%s*(.-)%s*$')
    tImGui.BeginDisabled(trimmed=='' or trimmed==bone.name)
    if tImGui.Button(tLang.L('swl_apply_rename')..'##swlBindRenameApply') then
        local selectedBoneId=bone.boneId
        local snapshot=stageRollbackSnapshot()
        local ok=false
        if snapshot then
            ok=select(1,safeCall(function()
                return state.meshD:renameSkeletalBone(state.boneIndex,trimmed)
            end))
        else
            setStatus(tLang.L('swl_snapshot_failed'),true)
        end
        if ok then
            commitRollbackSnapshot(snapshot,'swl_bone_renamed')
            state.modified=true
            refreshBindReport()
            for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                if item.boneId==selectedBoneId then state.boneIndex=index break end
            end
            state.bindRenameBoneId=nil
            state.bindAddBoneId=nil
            state.bindRemoveConfirmed=false
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
            setStatus(tLang.L('swl_bone_renamed'),false)
        else
            discardRollbackSnapshot(snapshot)
        end
    end
    tImGui.EndDisabled()
    local parentNames={tLang.L('swl_root_no_parent')}
    for _,candidate in ipairs(report.bones or {}) do
        parentNames[#parentNames+1]=candidate.name or '?'
    end
    tImGui.PushItemWidth(190)
    local parentChanged,parentChoice=tImGui.Combo(tLang.L('swl_parent_bone'),
        state.bindParentChoice,parentNames,-1)
    tImGui.PopItemWidth()
    if parentChanged then state.bindParentChoice=parentChoice end
    local preserveGlobal=tImGui.Checkbox(tLang.L('swl_preserve_global_bind'),state.bindPreserveGlobal)
    if preserveGlobal~=state.bindPreserveGlobal then state.bindPreserveGlobal=preserveGlobal end
    local requestedParent=state.bindParentChoice-1 -- 0=root; otherwise one-based bone index
    local currentParent=bone.parentIndex or 0
    local selectsSelf=requestedParent==state.boneIndex
    tImGui.BeginDisabled(requestedParent==currentParent or selectsSelf)
    if tImGui.Button(tLang.L('swl_apply_reparent')..'##swlBindReparentApply') then
        local selectedBoneId=bone.boneId
        local snapshot=stageRollbackSnapshot()
        local ok=false
        if snapshot then
            ok=select(1,safeCall(function()
                return state.meshD:reparentSkeletalBone(state.boneIndex,requestedParent,
                    state.bindPreserveGlobal)
            end))
        else
            setStatus(tLang.L('swl_snapshot_failed'),true)
        end
        if ok then
            commitRollbackSnapshot(snapshot,'swl_bone_reparented')
            state.modified=true
            refreshBindReport()
            for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                if item.boneId==selectedBoneId then state.boneIndex=index break end
            end
            state.bindRenameBoneId=nil
            state.bindReparentBoneId=nil
            state.bindEditBoneId=nil
            state.bindRemoveConfirmed=false
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
            setStatus(tLang.L('swl_bone_reparented'),false)
        else
            discardRollbackSnapshot(snapshot)
        end
    end
    tImGui.EndDisabled()
    if selectsSelf then tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},
        tLang.L('swl_parent_self_invalid')) end
    if tImGui.TreeNode(tLang.L('swl_edit_local_bind')..'##swlEditLocalBind') then
        local fields={{'T X','tx'},{'T Y','ty'},{'T Z','tz'},
            {'Q X','qx'},{'Q Y','qy'},{'Q Z','qz'},{'Q W','qw'},
            {'S X','sx'},{'S Y','sy'},{'S Z','sz'},
            {tLang.L('swl_bone_radius'),'radius'},{tLang.L('swl_bone_length'),'length'}}
        tImGui.PushItemWidth(190)
        for _,field in ipairs(fields) do
            local edited,value=tImGui.InputFloat(field[1]..'##swlBindEdit'..field[2],
                state.bindEdit[field[2]],0,0,'%.6g',0)
            if edited then state.bindEdit[field[2]]=value end
        end
        tImGui.PopItemWidth()
        tImGui.TextWrapped(tLang.L('swl_local_bind_moves_subtree'))
        if tImGui.Button(tLang.L('swl_apply_bind_edit')..'##swlBindEditApply') then
            local selectedBoneId=bone.boneId
            local value=state.bindEdit
            local snapshot=stageRollbackSnapshot()
            local ok=false
            if snapshot then
                ok=select(1,safeCall(function()
                    return state.meshD:setSkeletalBoneBind(state.boneIndex,
                        value.tx,value.ty,value.tz,value.qx,value.qy,value.qz,value.qw,
                        value.sx,value.sy,value.sz,value.radius,value.length)
                end))
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot,'swl_bone_bind_updated')
                state.modified=true
                refreshBindReport()
                for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                    if item.boneId==selectedBoneId then state.boneIndex=index break end
                end
                state.bindEditBoneId=nil
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_bone_bind_updated'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_add_bone')..'##swlAddBone') then
        tImGui.PushItemWidth(190)
        local nameChanged,addName=tImGui.InputText(tLang.L('swl_bone_name')..'##swlAddBoneName',
            state.bindAddName,tImGui.Flags('ImGuiInputTextFlags_None'))
        if nameChanged then state.bindAddName=addName end
        local addParentChanged,addParent=tImGui.Combo(tLang.L('swl_parent_bone')..'##swlAddBoneParent',
            state.bindAddParentChoice,parentNames,-1)
        if addParentChanged then state.bindAddParentChoice=addParent end
        for _,field in ipairs({{'T X','x'},{'T Y','y'},{'T Z','z'}}) do
            local edited,value=tImGui.InputFloat(field[1]..'##swlAddBone'..field[2],
                state.bindAddTranslation[field[2]],0,0,'%.6g',0)
            if edited then state.bindAddTranslation[field[2]]=value end
        end
        tImGui.PopItemWidth()
        tImGui.TextWrapped(tLang.L('swl_add_bone_defaults'))
        local addTrimmed=(state.bindAddName or ''):match('^%s*(.-)%s*$')
        tImGui.BeginDisabled(addTrimmed=='')
        if tImGui.Button(tLang.L('swl_apply_add_bone')..'##swlAddBoneApply') then
            local snapshot=stageRollbackSnapshot()
            local ok,newIndex=false,nil
            if snapshot then
                ok,newIndex=safeCall(function()
                    local value=state.bindAddTranslation
                    return state.meshD:addSkeletalBone(state.bindAddParentChoice-1,addTrimmed,
                        value.x,value.y,value.z,bone.radius or 0,bone.length or 0)
                end)
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot,'swl_bone_added')
                state.modified=true
                refreshBindReport()
                state.boneIndex=newIndex
                state.bindRenameBoneId=nil
                state.bindReparentBoneId=nil
                state.bindEditBoneId=nil
                state.bindAddBoneId=nil
                state.bindRemoveConfirmed=false
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_bone_added'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        if tImGui.TreeNode(tLang.L('swl_add_chain')..'##swlAddChain') then
            tImGui.PushItemWidth(190)
            local prefixChanged,prefix=tImGui.InputText(tLang.L('swl_chain_prefix')..'##swlChainPrefix',
                state.bindChainPrefix,tImGui.Flags('ImGuiInputTextFlags_None'))
            if prefixChanged then state.bindChainPrefix=prefix end
            local countChanged,count=tImGui.InputInt(tLang.L('swl_chain_count')..'##swlChainCount',
                state.bindChainCount,1,1,tImGui.Flags('ImGuiInputTextFlags_None'))
            if countChanged then state.bindChainCount=math.max(1,math.min(256,count)) end
            for _,field in ipairs({{'T X','x'},{'T Y','y'},{'T Z','z'}}) do
                local edited,value=tImGui.InputFloat(field[1]..'##swlChain'..field[2],
                    state.bindChainStep[field[2]],0,0,'%.6g',0)
                if edited then state.bindChainStep[field[2]]=value end
            end
            tImGui.PopItemWidth()
            tImGui.TextWrapped(tLang.L('swl_add_chain_help'))
            local chainPrefix=(state.bindChainPrefix or ''):match('^%s*(.-)%s*$')
            tImGui.BeginDisabled(chainPrefix=='')
            if tImGui.Button(tLang.L('swl_apply_add_chain')..'##swlAddChainApply') then
                local snapshot=stageRollbackSnapshot()
                local ok,lastIndex=false,nil
                if snapshot then
                    ok,lastIndex=safeCall(function()
                        local step=state.bindChainStep
                        return state.meshD:addSkeletalBoneChain(state.bindAddParentChoice-1,
                            chainPrefix,state.bindChainCount,step.x,step.y,step.z,
                            bone.radius or 0,bone.length or 0)
                    end)
                else
                    setStatus(tLang.L('swl_snapshot_failed'),true)
                end
                if ok then
                    commitRollbackSnapshot(snapshot,'swl_chain_added')
                    state.modified=true
                    refreshBindReport()
                    state.boneIndex=lastIndex
                    state.bindRenameBoneId=nil
                    state.bindReparentBoneId=nil
                    state.bindEditBoneId=nil
                    state.bindAddBoneId=nil
                    state.bindChainBoneId=nil
                    state.bindRemoveConfirmed=false
                    rebuildPreview()
                    rebuildSkeletonVisuals()
                    applyWorkspaceVisibility()
                    setStatus(tLang.L('swl_chain_added'),false)
                else
                    discardRollbackSnapshot(snapshot)
                end
            end
            tImGui.EndDisabled()
            tImGui.TreePop()
        end
        tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_mirror_subtree')..'##swlMirrorSubtree') then
        local subtreeCount=0
        for index=1,#(report.bones or {}) do
            local cursor=index
            while cursor and cursor>0 do
                if cursor==state.boneIndex then subtreeCount=subtreeCount+1 break end
                cursor=report.bones[cursor] and report.bones[cursor].parentIndex or 0
            end
        end
        tImGui.Text(string.format(tLang.L('swl_mirror_subtree_count_fmt'),subtreeCount))
        tImGui.PushItemWidth(190)
        local prefixChanged,prefix=tImGui.InputText(tLang.L('swl_mirror_name_prefix')..'##swlMirrorPrefix',
            state.bindMirrorPrefix,tImGui.Flags('ImGuiInputTextFlags_None'))
        if prefixChanged then state.bindMirrorPrefix=prefix; state.bindMirrorConfirmed=false end
        local axes={'X','Y','Z'}
        local axisChanged,axis=tImGui.Combo(tLang.L('swl_mirror_global_axis'),state.bindMirrorAxis,axes,-1)
        if axisChanged then state.bindMirrorAxis=axis; state.bindMirrorConfirmed=false end
        tImGui.PopItemWidth()
        tImGui.TextWrapped(tLang.L('swl_mirror_subtree_help'))
        local clipBlocked=(report.animationClipCount or 0)>0
        if clipBlocked then
            tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_mirror_clips_blocked'))
        end
        local mirrorPrefix=(state.bindMirrorPrefix or ''):match('^%s*(.-)%s*$')
        tImGui.BeginDisabled(clipBlocked or mirrorPrefix=='')
        local confirmed=tImGui.Checkbox(tLang.L('swl_confirm_mirror_subtree'),state.bindMirrorConfirmed)
        if confirmed~=state.bindMirrorConfirmed then state.bindMirrorConfirmed=confirmed end
        tImGui.BeginDisabled(not state.bindMirrorConfirmed)
        if tImGui.Button(tLang.L('swl_apply_mirror_subtree')..'##swlMirrorApply') then
            local snapshot=stageRollbackSnapshot()
            local ok,newRoot=false,nil
            if snapshot then
                ok,newRoot=safeCall(function()
                    return state.meshD:mirrorSkeletalBoneSubtree(state.boneIndex,
                        state.bindMirrorAxis,mirrorPrefix)
                end)
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot,'swl_subtree_mirrored')
                state.modified=true
                refreshBindReport()
                state.boneIndex=newRoot
                state.bindRenameBoneId=nil
                state.bindReparentBoneId=nil
                state.bindEditBoneId=nil
                state.bindAddBoneId=nil
                state.bindChainBoneId=nil
                state.bindMirrorBoneId=nil
                state.bindRemoveConfirmed=false
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_subtree_mirrored'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    local hasWeightsOk,hasWeights=safeCall(function()
        return state.meshD:hasSkeletalVertexWeights()
    end)
    if hasWeightsOk and not hasWeights and
            tImGui.TreeNode(tLang.L('swl_initialize_weights')..'##swlInitializeWeights') then
        local vertexCount=state.meshBounds and state.meshBounds.total or 0
        tImGui.TextWrapped(string.format(tLang.L('swl_initialize_weights_help_fmt'),
            vertexCount,bone.name or '?'))
        local confirmed=tImGui.Checkbox(tLang.L('swl_confirm_initialize_weights'),
            state.bindWeightsConfirmed)
        if confirmed~=state.bindWeightsConfirmed then state.bindWeightsConfirmed=confirmed end
        tImGui.BeginDisabled(not state.bindWeightsConfirmed or vertexCount==0)
        if tImGui.Button(tLang.L('swl_initialize_and_open_weights')..'##swlInitializeWeightsApply') then
            local snapshot=stageRollbackSnapshot()
            local ok,affected=false,nil
            if snapshot then
                ok,affected=safeCall(function()
                    return state.meshD:initializeSkeletalVertexWeights(state.boneIndex)
                end)
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot,'swl_history_initialize_weights')
                state.modified=true
                refreshBindReport()
                state.bindWeightsBoneId=nil
                state.allowedBones={}
                for _,item in ipairs(getBones()) do state.allowedBones[item.name]=true end
                rebuildPreview()
                rebuildSkeletonVisuals()
                setWorkspace('paint')
                applyWorkspaceVisibility()
                setStatus(string.format(tLang.L('swl_weights_initialized_fmt'),affected,bone.name),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_remove_bone')..'##swlRemoveBone') then
        tImGui.Text(string.format(tLang.L('swl_remove_bone_impact_fmt'),bone.childCount or 0,
            bone.weightedVertexCount or 0,bone.animationTrackCount or 0))
        if bone.weightPaletteReferenced then
            tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_remove_weight_palette_blocked'))
        end
        local hasReferences=bone.weightPaletteReferenced==true or (bone.animationTrackCount or 0)>0
        local hasChildren=(bone.childCount or 0)>0
        local blocked=#(report.bones or {})<=1
        tImGui.TextWrapped(blocked and tLang.L('swl_remove_bone_blocked') or
            ((hasReferences or hasChildren) and tLang.L('swl_remove_bone_remap_policy') or
                tLang.L('swl_remove_bone_strict_policy')))
        local replacementNames,replacementIndices={},{}
        for index,candidate in ipairs(report.bones or {}) do
            if index~=state.boneIndex then
                replacementNames[#replacementNames+1]=candidate.name or '?'
                replacementIndices[#replacementIndices+1]=index
            end
        end
        if (hasReferences or hasChildren) and #replacementNames>0 then
            local replacementChoice=1
            for choice,index in ipairs(replacementIndices) do
                if index==state.bindRemoveReplacement then replacementChoice=choice break end
            end
            tImGui.PushItemWidth(190)
            local replacementChanged,newChoice=tImGui.Combo(tLang.L('swl_remove_remap_target'),
                replacementChoice,replacementNames,-1)
            tImGui.PopItemWidth()
            if replacementChanged then
                state.bindRemoveReplacement=replacementIndices[newChoice]
                state.bindRemoveConfirmed=false
            end
        end
        if hasChildren then
            local reparent=tImGui.Checkbox(tLang.L('swl_reparent_children_preserve_global'),
                state.bindRemoveReparentChildren)
            if reparent~=state.bindRemoveReparentChildren then
                state.bindRemoveReparentChildren=reparent
                state.bindRemoveConfirmed=false
            end
            if (report.animationClipCount or 0)>0 then
                tImGui.TextWrapped(tLang.L('swl_remove_children_tracks_baked'))
            end
        end
        if (bone.animationTrackCount or 0)>0 then
            local discard=tImGui.Checkbox(tLang.L('swl_discard_removed_bone_tracks'),
                state.bindRemoveDiscardTracks)
            if discard~=state.bindRemoveDiscardTracks then
                state.bindRemoveDiscardTracks=discard
                state.bindRemoveConfirmed=false
            end
        end
        local actionBlocked=blocked or ((bone.animationTrackCount or 0)>0 and
            not state.bindRemoveDiscardTracks) or (hasChildren and
            not state.bindRemoveReparentChildren)
        tImGui.BeginDisabled(actionBlocked)
        local confirmed=tImGui.Checkbox(tLang.L('swl_confirm_remove_bone'),state.bindRemoveConfirmed)
        if confirmed~=state.bindRemoveConfirmed then state.bindRemoveConfirmed=confirmed end
        tImGui.BeginDisabled(not state.bindRemoveConfirmed)
        if tImGui.Button(tLang.L('swl_apply_remove_bone')..'##swlRemoveBoneApply') then
            local previousParentId=bone.parentBoneId
            local snapshot=stageRollbackSnapshot()
            local ok=false
            if snapshot then
                ok=select(1,safeCall(function()
                    if hasReferences or hasChildren then
                        return state.meshD:removeSkeletalBoneRemapped(state.boneIndex,
                            state.bindRemoveReplacement,state.bindRemoveDiscardTracks,
                            state.bindRemoveReparentChildren)
                    end
                    return state.meshD:removeSkeletalBone(state.boneIndex)
                end))
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot,'swl_bone_removed')
                state.modified=true
                refreshBindReport()
                state.boneIndex=1
                for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                    if item.boneId==previousParentId then state.boneIndex=index break end
                end
                state.bindRenameBoneId=nil
                state.bindReparentBoneId=nil
                state.bindEditBoneId=nil
                state.bindAddBoneId=nil
                state.bindRemoveBoneId=nil
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_bone_removed'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    if bone.hasNegativeScale then tImGui.TextColored({r=1,g=0.65,b=0.1,a=1},
        tLang.L('swl_bind_negative_scale')) end
    if bone.hasShear then tImGui.TextColored({r=1,g=0.3,b=0.25,a=1},
        tLang.L('swl_bind_shear')) end
    if tImGui.TreeNode(tLang.L('swl_bind_local_matrix')..'##selectedLocal') then
        tImGui.Text(formatBindMatrix(bone.localBindMatrix)); tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_bind_global_matrix')..'##selectedGlobal') then
        tImGui.Text(formatBindMatrix(bone.globalBindMatrix)); tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_bind_inverse_matrix')..'##selectedInverse') then
        tImGui.Text(formatBindMatrix(bone.inverseGlobalBindMatrix)); tImGui.TreePop()
    end
end

local function showBindBoneHierarchy(report)
    local bones=report.bones or {}
    local children={}
    local roots={}
    local diagnosticsByBone={}
    for _,diagnostic in ipairs(report.diagnostics or {}) do
        local index=tonumber(diagnostic.sourceIndex) or 0
        diagnosticsByBone[index]=diagnosticsByBone[index] or {}
        diagnosticsByBone[index][#diagnosticsByBone[index]+1]=diagnostic
    end
    for index,bone in ipairs(bones) do
        local parentIndex=tonumber(bone.parentIndex) or 0
        if parentIndex>0 and parentIndex<=#bones and parentIndex~=index then
            children[parentIndex]=children[parentIndex] or {}
            children[parentIndex][#children[parentIndex]+1]=index
        else
            roots[#roots+1]=index
        end
    end

    -- Structural reachability must not depend on which TreeNode happens to be expanded this frame.
    -- Otherwise valid children hidden under a collapsed parent look "unvisited" to the malformed-data
    -- fallback below and are incorrectly rendered again as parallel roots.
    local structurallyReachable={}
    local function markSubtree(index)
        if structurallyReachable[index] then return end
        structurallyReachable[index]=true
        for _,childIndex in ipairs(children[index] or {}) do markSubtree(childIndex) end
    end
    for _,rootIndex in ipairs(roots) do markSubtree(rootIndex) end

    local rendered={}
    local function showNode(index)
        if rendered[index] then return end
        rendered[index]=true
        local bone=bones[index]
        local findings=diagnosticsByBone[index]
        local prefix=findings and '! ' or ''
        local label=string.format('%s%s##swlHierarchyBone%d',prefix,bone.name or '?',index)
        local flags=state.boneIndex==index and tImGui.Flags('ImGuiTreeNodeFlags_Selected') or
            tImGui.Flags('ImGuiTreeNodeFlags_None')
        if state.bindTreeOpenAll then
            tImGui.SetNextItemOpen(true,tImGui.Flags('ImGuiCond_Always'))
        end
        if findings then tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'),
            {r=1,g=0.45,b=0.2,a=1}) end
        local open=tImGui.TreeNodeEx(label,flags)
        local clicked=tImGui.IsItemClicked()
        if findings then tImGui.PopStyleColor(1) end
        if clicked then
            state.boneIndex=index
            applyWorkspaceVisibility()
        end
        if open then
            for _,childIndex in ipairs(children[index] or {}) do showNode(childIndex) end
            tImGui.TreePop()
        end
    end
    for _,rootIndex in ipairs(roots) do showNode(rootIndex) end
    -- Defensive display for genuinely malformed/orphaned snapshots; valid descendants remain hidden
    -- with their collapsed parent instead of being flattened into this root level.
    for index=1,#bones do if not structurallyReachable[index] then showNode(index) end end
    state.bindTreeOpenAll=false
end

local function showBindPoseDiagnostics()
    if tTutorials.consumeFocus('bind_hierarchy') then tImGui.SetScrollHereY(0.15) end
    local report=state.bindReport
    if not report then
        tImGui.TextDisabled(tLang.L('swl_bind_report_unavailable'))
        tImGui.TextWrapped(tLang.L('swl_initialize_skeleton_help'))
        tImGui.PushItemWidth(190)
        local nameChanged,name=tImGui.InputText(tLang.L('swl_bone_name')..'##swlInitialRootName',
            state.bindInitialName,tImGui.Flags('ImGuiInputTextFlags_None'))
        if nameChanged then state.bindInitialName=name end
        for _,field in ipairs({{'T X','x'},{'T Y','y'},{'T Z','z'}}) do
            local changed,value=tImGui.InputFloat(field[1]..'##swlInitialRoot'..field[2],
                state.bindInitialTranslation[field[2]],0,0,'%.6g',0)
            if changed then state.bindInitialTranslation[field[2]]=value end
        end
        local radiusChanged,radius=tImGui.InputFloat(tLang.L('swl_bone_radius')..'##swlInitialRadius',
            state.bindInitialRadius,0,0,'%.6g',0)
        if radiusChanged then state.bindInitialRadius=radius end
        local lengthChanged,length=tImGui.InputFloat(tLang.L('swl_bone_length')..'##swlInitialLength',
            state.bindInitialLength,0,0,'%.6g',0)
        if lengthChanged then state.bindInitialLength=length end
        tImGui.PopItemWidth()
        local trimmed=(state.bindInitialName or ''):match('^%s*(.-)%s*$')
        tImGui.BeginDisabled(trimmed=='' or state.bindInitialRadius<0 or state.bindInitialLength<0)
        if tImGui.Button(tLang.L('swl_initialize_skeleton')..'##swlInitializeSkeleton') then
            local snapshot=stageRollbackSnapshot()
            local ok=false
            if snapshot then
                local value=state.bindInitialTranslation
                ok=select(1,safeCall(function()
                    return state.meshD:initializeSkeletalSkeleton(trimmed,value.x,value.y,value.z,
                        state.bindInitialRadius,state.bindInitialLength)
                end))
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot,'swl_skeleton_initialized')
                state.modified=true
                refreshBindReport()
                state.boneIndex=1
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_skeleton_initialized'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        showRollbackControls('swlInitializeRevert')
        return
    end
    local color=report.valid and {r=0.25,g=0.9,b=0.35,a=1} or {r=1,g=0.3,b=0.25,a=1}
    tImGui.TextColored(color,report.valid and tLang.L('swl_bind_valid') or tLang.L('swl_bind_invalid'))
    tImGui.Text(string.format(tLang.L('swl_bind_error_summary_fmt'),
        report.maximumReconstructionError or 0,report.maximumBindIdentityError or 0,
        report.diagnosticCount or 0))
    if tImGui.Button(tLang.L('swl_refresh')..'##swlBindRefresh') then refreshBindReport() end
    if report.diagnostics and #report.diagnostics>0 and
            tImGui.TreeNode(tLang.L('swl_bind_diagnostics')..'##swlBindDiagnostics') then
        for _,diagnostic in ipairs(report.diagnostics) do
            local label=string.format('%s: %s [%d] error=%.7g',
                diagnostic.fatal and tLang.L('swl_fatal') or tLang.L('swl_warning'),
                diagnostic.code or '?',diagnostic.sourceIndex or 0,diagnostic.observedError or 0)
            tImGui.TextWrapped(label..((diagnostic.boneName and diagnostic.boneName~='') and
                (' - '..diagnostic.boneName) or ''))
        end
        tImGui.TreePop()
    end
    if report.bones and tImGui.TreeNode(string.format('%s (%d)##swlBindBones',
            tLang.L('swl_bind_compiled_bones'),#report.bones)) then
        if tImGui.Button(tLang.L('swl_expand_all')..'##swlBindExpandAll') then
            state.bindTreeOpenAll=true
        end
        tImGui.TextDisabled(tLang.L('swl_hierarchy_scroll_hint'))
        tImGui.BeginChild('##swlBindHierarchyScroll',{x=0,y=300},true)
        showBindBoneHierarchy(report)
        tImGui.EndChild()
        showSelectedBindBone(report)
        tImGui.TreePop()
    end
    showRollbackControls('swlBindRevert')
end

local function openWorkspaceNode(key,label,id)
    local selected=state.workspace==key
    tImGui.SetNextItemOpen(selected,tImGui.Flags('ImGuiCond_Always'))
    local open=tImGui.TreeNodeEx(label,0,id)
    if tImGui.IsItemClicked() and not selected then setWorkspace(key) end
    return open
end

local function showSharedVisualization()
    showSectionTitle('swl_shared_visualization')
    local meshVisible=tImGui.Checkbox(tLang.L('swl_show_mesh'),state.meshVisible)
    if meshVisible~=state.meshVisible then
        state.meshVisible=meshVisible
        applyWorkspaceVisibility()
    end
end

local function showPaintWeights()
    local bones=getBones()
    if #bones==0 then
        tImGui.TextWrapped(tLang.L('swl_paint_requires_skeleton'))
        return
    end
    local okWeights,hasWeights=safeCall(function() return state.meshD:hasSkeletalVertexWeights() end)
    if not okWeights or not hasWeights then
        tImGui.TextWrapped(tLang.L('swl_paint_requires_weights'))
        return
    end
    showSectionTitle('swl_paint_skeleton_view')
    local meshVisible=tImGui.Checkbox(tLang.L('swl_show_mesh'),state.meshVisible)
    if meshVisible~=state.meshVisible then
        state.meshVisible=meshVisible
        applyWorkspaceVisibility()
    end
    local showSkeleton=tImGui.Checkbox(tLang.L('swl_show_skeleton'),state.paint.showSkeleton)
    if showSkeleton~=state.paint.showSkeleton then
        state.paint.showSkeleton=showSkeleton
        applyWorkspaceVisibility()
    end
    tImGui.Separator()
    showSectionTitle('swl_paint_repair_diagnostics')
    tImGui.Text(tLang.L('swl_paint_health_title'))
    if tImGui.Button(tLang.L('swl_paint_health_analyze')) then
        analyzeWeightHealth()
    end
    showItemTooltip(tLang.L('swl_paint_health_help'))
    local health=state.paint.weightHealthSummary
    if health then
        local canonicalKey=health.canonicalValid and 'swl_paint_health_canonical_valid' or
            'swl_paint_health_canonical_invalid'
        local canonicalColor=health.canonicalValid and {r=0.25,g=0.9,b=0.35,a=1} or
            {r=1,g=0.3,b=0.25,a=1}
        showWrappedColoredText(tLang.L(canonicalKey),canonicalColor)
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_health_coverage_fmt'),
            health.total,health.unweighted,health.invalidWeightVertices,
            health.counts[1],health.counts[2],
            health.counts[3],health.counts[4]))
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_health_dominant_fmt'),
            health.dominantMinimum,health.dominantAverage,health.dominantMaximum))
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_health_weak_fmt'),
            health.weakVertices,health.weakInfluences,health.weakWeight,
            health.weakMaximum,health.cleanThreshold))
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_health_abrupt_fmt'),
            health.abruptEdges,health.abruptVertices,health.abruptMaximum,
            health.abruptThreshold))
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_health_seam_fmt'),
            health.seamGroups,health.seamTotal,health.seamVertices,health.seamMaximum))
        if health.canonicalValid and health.unweighted==0 and health.weakVertices==0 and
                health.seamGroups==0 then
            showWrappedColoredText(tLang.L('swl_paint_health_mechanical_clear'),
                {r=0.25,g=0.9,b=0.35,a=1})
        else
            showWrappedColoredText(tLang.L('swl_paint_health_mechanical_attention'),
                {r=1,g=0.7,b=0.15,a=1})
        end
        if health.abruptEdges>0 then
            showWrappedColoredText(tLang.L('swl_paint_health_pose_review'),
                {r=1,g=0.7,b=0.15,a=1})
        else
            showWrappedDisabledText(tLang.L('swl_paint_health_no_abrupt'))
        end
        if health.weakVertices>0 or health.seamGroups>0 then
            tImGui.Separator()
            tImGui.Text(tLang.L('swl_paint_health_known_repairs'))
            if health.weakVertices>0 then
                if tImGui.Button(tLang.L('swl_paint_health_clean_weak')) then
                    if cleanPaintWeakInfluences(true) then analyzeWeightHealth() end
                end
                showItemTooltip(tLang.L('swl_paint_health_clean_weak_help'))
            end
            if health.seamGroups>0 then
                state.paint.weightHealthSeamConfirmed=tImGui.Checkbox(
                    tLang.L('swl_paint_health_confirm_seams'),
                    state.paint.weightHealthSeamConfirmed)
                tImGui.BeginDisabled(not state.paint.weightHealthSeamConfirmed)
                if tImGui.Button(tLang.L('swl_paint_health_sync_seams')) then
                    if synchronizeGlobalSeamWeights() then analyzeWeightHealth() end
                end
                tImGui.EndDisabled()
                showItemTooltip(tLang.L('swl_paint_health_sync_seams_help'),true)
            end
        else
            tImGui.Separator()
            showWrappedDisabledText(tLang.L('swl_paint_health_no_known_repairs'))
        end
    end
    tImGui.Separator()
    local previousVisualizationMode=state.paint.visualizationMode
    state.paint.visualizationMode=tImGui.RadioButton(tLang.L('swl_paint_show_selected'),
        state.paint.visualizationMode,1)
    state.paint.visualizationMode=tImGui.RadioButton(tLang.L('swl_paint_show_distribution'),
        state.paint.visualizationMode,2)
    state.paint.visualizationMode=tImGui.RadioButton(tLang.L('swl_paint_show_weak'),
        state.paint.visualizationMode,3)
    state.paint.visualizationMode=tImGui.RadioButton(tLang.L('swl_paint_show_abrupt'),
        state.paint.visualizationMode,4)
    state.paint.visualizationMode=tImGui.RadioButton(tLang.L('swl_paint_show_mask_original'),
        state.paint.visualizationMode,5)
    if state.paint.visualizationMode~=previousVisualizationMode then
        state.paint.limitInfluencesConfirmed=false
        state.paint.heatmapDirty=true
        rebuildPaintHeatmap()
        rebuildPaintCursor(nil)
        applyWorkspaceVisibility()
    end
    if state.paint.visualizationMode>=2 and state.paint.visualizationMode<=4 then
        local diagnosticMask=tImGui.Checkbox(tLang.L('swl_paint_diagnostics_use_mask'),
            state.paint.diagnosticsUseMask)
        if diagnosticMask~=state.paint.diagnosticsUseMask then
            state.paint.diagnosticsUseMask=diagnosticMask
            state.paint.heatmapDirty=true
            rebuildPaintHeatmap()
        end
        tImGui.TextWrapped(tLang.L('swl_paint_diagnostics_use_mask_help'))
    end
    if state.paint.visualizationMode==2 then
        tImGui.TextWrapped(tLang.L('swl_paint_distribution_help'))
        local stats=state.paint.distributionStats
        if stats then
            tImGui.Text(string.format(tLang.L('swl_paint_distribution_stats_fmt'),
                stats.minimum,stats.average,stats.maximum))
            tImGui.Text(string.format(tLang.L('swl_paint_distribution_counts_fmt'),
                stats.counts[1],stats.counts[2],stats.counts[3],stats.counts[4]))
        end
        tImGui.Separator()
        showSectionTitle('swl_paint_weight_tools')
        tImGui.PushItemWidth(240)
        local maximumChanged,maximum=tImGui.SliderInt(
            tLang.L('swl_paint_maximum_influences'),state.paint.maximumInfluences,1,4)
        tImGui.PopItemWidth()
        showItemTooltip(tLang.L('swl_paint_limit_influences_help'))
        if maximumChanged then
            state.paint.maximumInfluences=maximum
            state.paint.limitInfluencesConfirmed=false
        end
        local affected=0
        if stats and stats.globalCounts then
            for count=(state.paint.maximumInfluences or 4)+1,4 do
                affected=affected+(stats.globalCounts[count] or 0)
            end
        end
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_limit_influences_preview_fmt'),affected,
            state.paint.maximumInfluences or 4))
        state.paint.limitInfluencesConfirmed=tImGui.Checkbox(
            tLang.L('swl_paint_confirm_limit_influences'),state.paint.limitInfluencesConfirmed)
        tImGui.BeginDisabled(affected==0 or not state.paint.limitInfluencesConfirmed)
        if tImGui.Button(tLang.L('swl_paint_limit_influences_apply')) then
            limitPaintMaximumInfluences()
        end
        tImGui.EndDisabled()
    elseif state.paint.visualizationMode==3 then
        tImGui.TextWrapped(tLang.L('swl_paint_weak_help'))
        tImGui.PushItemWidth(240)
        local thresholdChanged,cleanThreshold=tImGui.SliderFloat(
            tLang.L('swl_paint_clean_threshold'),state.paint.cleanThreshold,0.0001,0.25,'%.4f')
        tImGui.PopItemWidth()
        if thresholdChanged then
            state.paint.cleanThreshold=cleanThreshold
            state.paint.weightHealthSummary=nil
            state.paint.heatmapDirty=true
            rebuildPaintHeatmap()
        end
        local stats=state.paint.weakStats
        if stats then
            tImGui.Text(string.format(tLang.L('swl_paint_weak_stats_fmt'),stats.vertices,
                stats.total,stats.influences))
            tImGui.Text(string.format(tLang.L('swl_paint_weak_weight_fmt'),stats.totalWeight,
                stats.maximumWeight,state.paint.cleanThreshold))
        end
        tImGui.Separator()
        showSectionTitle('swl_paint_weight_tools')
        tImGui.TextWrapped(tLang.L('swl_paint_clean_help'))
        if tImGui.Button(tLang.L('swl_paint_clean_apply')) then cleanPaintWeakInfluences() end
    elseif state.paint.visualizationMode==4 then
        tImGui.TextWrapped(tLang.L('swl_paint_abrupt_help'))
        tImGui.PushItemWidth(240)
        local thresholdChanged,abruptThreshold=tImGui.SliderFloat(
            tLang.L('swl_paint_abrupt_threshold'),state.paint.abruptThreshold,0.01,1,'%.2f')
        tImGui.PopItemWidth()
        if thresholdChanged then
            state.paint.abruptThreshold=abruptThreshold
            state.paint.weightHealthSummary=nil
            local stats=state.paint.abruptStats
            if stats and stats.records then
                local edges,vertices=0,{}
                for _,record in ipairs(stats.records) do
                    if record.distance>=abruptThreshold then
                        edges=edges+1; vertices[record.a]=true; vertices[record.b]=true
                    end
                end
                local affected=0
                for _ in pairs(vertices) do affected=affected+1 end
                stats.edges,stats.vertices=edges,affected
            end
        end
        local stats=state.paint.abruptStats
        if stats then
            tImGui.Text(string.format(tLang.L('swl_paint_abrupt_stats_fmt'),stats.edges,
                stats.vertices,stats.total))
            tImGui.Text(string.format(tLang.L('swl_paint_abrupt_max_fmt'),stats.maximum,
                state.paint.abruptThreshold))
        end
        tImGui.Separator()
        showSectionTitle('swl_paint_weight_tools')
        tImGui.TextWrapped(tLang.L('swl_paint_abrupt_repair_help'))
        tImGui.PushItemWidth(240)
        local strengthChanged,repairStrength=tImGui.SliderFloat(
            tLang.L('swl_paint_abrupt_repair_strength'),state.paint.abruptRepairStrength,
            0.01,1,'%.2f')
        tImGui.PopItemWidth()
        if strengthChanged then state.paint.abruptRepairStrength=repairStrength end
        tImGui.PushItemWidth(240)
        local iterationsChanged,repairIterations=tImGui.SliderInt(
            tLang.L('swl_paint_abrupt_repair_iterations'),state.paint.abruptRepairIterations,1,10)
        tImGui.PopItemWidth()
        if iterationsChanged then state.paint.abruptRepairIterations=repairIterations end
        tImGui.PushItemWidth(240)
        local maxChangeChanged,maxChange=tImGui.SliderFloat(
            tLang.L('swl_paint_abrupt_repair_max_change'),state.paint.abruptRepairMaxChange,
            0.01,1,'%.2f')
        tImGui.PopItemWidth()
        if maxChangeChanged then state.paint.abruptRepairMaxChange=maxChange end
        if stats then
            tImGui.Text(string.format(tLang.L('swl_paint_abrupt_repair_preview_fmt'),
                stats.vertices,state.paint.abruptRepairMaxChange))
        end
        if tImGui.Button(tLang.L('swl_paint_abrupt_repair_apply')) then
            repairPaintAbruptTransitions()
        end
        local safetyReport=state.paint.safetyReport
        tImGui.BeginDisabled(safetyReport==nil)
        local overlayVisible=tImGui.Checkbox(tLang.L('swl_paint_safety_overlay'),
            state.paint.safetyOverlayVisible)
        if overlayVisible~=state.paint.safetyOverlayVisible then
            state.paint.safetyOverlayVisible=overlayVisible
            applyWorkspaceVisibility()
        end
        tImGui.EndDisabled()
        if safetyReport then
            tImGui.TextWrapped(tLang.L('swl_paint_safety_legend'))
            tImGui.Text(string.format(tLang.L('swl_paint_safety_report_fmt'),
                safetyReport.unsafeFaces,safetyReport.unsafeFaceSamples,
                safetyReport.seamVertices,safetyReport.synchronizedSeams))
        end
    elseif state.paint.visualizationMode==5 then
        tImGui.TextWrapped(tLang.L('swl_paint_show_mask_original_help'))
    end
    if state.paint.visualizationMode==1 then
        tImGui.Separator()
        showSectionTitle('swl_paint_target_only')
        local names={}
        for _,bone in ipairs(bones) do names[#names+1]=bone.name end
        state.paint.boneIndex=math.max(1,math.min(state.paint.boneIndex,#bones))
        tImGui.PushItemWidth(240)
        local changed,boneIndex=tImGui.Combo(tLang.L('swl_paint_target_bone'),
            state.paint.boneIndex,names,-1)
        tImGui.PopItemWidth()
        if changed then
            state.paint.boneIndex=boneIndex
            state.paint.heatmapDirty=true
            rebuildPaintHeatmap()
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
        end
    end
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    if state.paint.visualizationMode==1 then
        tImGui.Separator()
        showSectionTitle('swl_paint_brush_section')
        if tTutorials.consumeFocus('paint_mask') then tImGui.SetScrollHereY(0.2) end
        local capture=state.paint.aabbCapture
        local captureActive=tImGui.Checkbox(tLang.L('swl_paint_aabb_capture_start'),
            capture.active)
        if captureActive~=capture.active then
            setPaintAabbCaptureActive(captureActive)
        end
        if capture.active and capture.bounds then
            local b=capture.bounds
            local reference=state.meshBounds
            local automaticSensitivity=math.max(reference and math.max(
                reference.maxX-reference.minX,reference.maxY-reference.minY,
                reference.maxZ-reference.minZ)*0.0025 or 0.001,0.0001)
            tImGui.PushItemWidth(200)
            local sensitivityChanged,sensitivity=tImGui.InputFloat(
                tLang.L('swl_paint_aabb_sensitivity'),capture.sensitivity or automaticSensitivity,
                automaticSensitivity*0.1,automaticSensitivity,'%.6f',0)
            tImGui.PopItemWidth()
            if tImGui.Button(tLang.L('swl_reset_auto')..'##paintAabbSensitivity') then
                capture.sensitivity=automaticSensitivity
            elseif sensitivityChanged then
                capture.sensitivity=math.max(sensitivity,0.000001)
            end
            local dragSpeed=math.max(capture.sensitivity or automaticSensitivity,0.000001)
            local hoverKind,hoverAxis=nil,nil
            tImGui.PushItemWidth(200)
            local minXChanged,newMinX=tImGui.DragFloat(tLang.L('swl_paint_aabb_min_x'),
                b.minX,dragSpeed,-1000000,1000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='min','x' end
            local minYChanged,newMinY=tImGui.DragFloat(tLang.L('swl_paint_aabb_min_y'),
                b.minY,dragSpeed,-1000000,1000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='min','y' end
            local minZChanged,newMinZ=tImGui.DragFloat(tLang.L('swl_paint_aabb_min_z'),
                b.minZ,dragSpeed,-1000000,1000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='min','z' end
            local maxXChanged,newMaxX=tImGui.DragFloat(tLang.L('swl_paint_aabb_max_x'),
                b.maxX,dragSpeed,-1000000,1000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='max','x' end
            local maxYChanged,newMaxY=tImGui.DragFloat(tLang.L('swl_paint_aabb_max_y'),
                b.maxY,dragSpeed,-1000000,1000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='max','y' end
            local maxZChanged,newMaxZ=tImGui.DragFloat(tLang.L('swl_paint_aabb_max_z'),
                b.maxZ,dragSpeed,-1000000,1000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='max','z' end
            local sx,sy,sz=b.maxX-b.minX,b.maxY-b.minY,b.maxZ-b.minZ
            local sxChanged,newSx=tImGui.DragFloat(tLang.L('swl_paint_aabb_size_x'),
                sx,dragSpeed,0.0001,2000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='size','x' end
            local syChanged,newSy=tImGui.DragFloat(tLang.L('swl_paint_aabb_size_y'),
                sy,dragSpeed,0.0001,2000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='size','y' end
            local szChanged,newSz=tImGui.DragFloat(tLang.L('swl_paint_aabb_size_z'),
                sz,dragSpeed,0.0001,2000000,'%.4f')
            if tImGui.IsItemHovered(0) then hoverKind,hoverAxis='size','z' end
            tImGui.PopItemWidth()
            if minXChanged or minYChanged or minZChanged or maxXChanged or maxYChanged or
                    maxZChanged or sxChanged or syChanged or szChanged then
                b.minX=minXChanged and math.min(newMinX,b.maxX-0.0001) or b.minX
                b.minY=minYChanged and math.min(newMinY,b.maxY-0.0001) or b.minY
                b.minZ=minZChanged and math.min(newMinZ,b.maxZ-0.0001) or b.minZ
                b.maxX=maxXChanged and math.max(newMaxX,b.minX+0.0001) or b.maxX
                b.maxY=maxYChanged and math.max(newMaxY,b.minY+0.0001) or b.maxY
                b.maxZ=maxZChanged and math.max(newMaxZ,b.minZ+0.0001) or b.maxZ
                if sxChanged then
                    local center=(b.minX+b.maxX)*0.5; local half=math.max(newSx,0.0001)*0.5
                    b.minX,b.maxX=center-half,center+half
                end
                if syChanged then
                    local center=(b.minY+b.maxY)*0.5; local half=math.max(newSy,0.0001)*0.5
                    b.minY,b.maxY=center-half,center+half
                end
                if szChanged then
                    local center=(b.minZ+b.maxZ)*0.5; local half=math.max(newSz,0.0001)*0.5
                    b.minZ,b.maxZ=center-half,center+half
                end
                rebuildPaintAabbCaptureBox()
            end
            setPaintAabbCaptureHover(hoverKind,hoverAxis)
            tImGui.TextWrapped(tLang.L('swl_paint_aabb_capture_active_help'))
        elseif capture.result then
            local capturedCount=0
            for _ in pairs(capture.result) do capturedCount=capturedCount+1 end
            tImGui.Text(string.format(tLang.L('swl_paint_aabb_capture_result_fmt'),capturedCount))
            if tImGui.Button(tLang.L('swl_paint_aabb_replace')) then
                applyPaintAabbCaptureToMask('replace')
            end
            if tImGui.Button(tLang.L('swl_paint_aabb_add')) then
                applyPaintAabbCaptureToMask('add')
            end
            if tImGui.Button(tLang.L('swl_paint_aabb_remove')) then
                applyPaintAabbCaptureToMask('remove')
            end
        end
        tImGui.BeginDisabled(capture.active)
        tImGui.Text(tLang.L('swl_paint_mask_mode'))
        local previousMaskMode=state.paint.maskEditMode
        state.paint.maskEditMode=tImGui.RadioButton(tLang.L('swl_paint_mask_off'),
            state.paint.maskEditMode,0)
        state.paint.maskEditMode=tImGui.RadioButton(tLang.L('swl_paint_mask_add'),
            state.paint.maskEditMode,1)
        state.paint.maskEditMode=tImGui.RadioButton(tLang.L('swl_paint_mask_remove'),
            state.paint.maskEditMode,2)
        if state.paint.maskEditMode~=previousMaskMode then
            rebuildPaintCursor(state.paint.cursorHit)
        end
        local maskCount=0
        for _ in pairs(state.paint.maskVertices) do maskCount=maskCount+1 end
        tImGui.Text(string.format(tLang.L('swl_paint_mask_count_fmt'),maskCount))
        local restrictMask=tImGui.Checkbox(tLang.L('swl_paint_mask_restrict'),
            state.paint.maskRestrictBrush)
        if restrictMask~=state.paint.maskRestrictBrush then
            state.paint.maskRestrictBrush=restrictMask
            rebuildPaintCursor(state.paint.cursorHit)
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_paint_mask_clear')) then
            state.paint.maskVertices={}
            rebuildPaintMaskMarkers()
            rebuildPaintCursor(state.paint.cursorHit)
        end
        tImGui.TextWrapped(tLang.L('swl_paint_mask_help'))
        tImGui.PushItemWidth(240)
        local topologyRingsChanged,topologyRings=tImGui.SliderInt(
            tLang.L('swl_paint_mask_topology_rings'),state.paint.maskTopologyRings,1,10)
        tImGui.PopItemWidth()
        if topologyRingsChanged then state.paint.maskTopologyRings=topologyRings end
        local acrossSeams=tImGui.Checkbox(tLang.L('swl_paint_mask_topology_seams'),
            state.paint.maskTopologyAcrossSeams)
        if acrossSeams~=state.paint.maskTopologyAcrossSeams then
            state.paint.maskTopologyAcrossSeams=acrossSeams
        end
        tImGui.BeginDisabled(maskCount==0)
        if tImGui.Button(tLang.L('swl_paint_mask_topology_grow')) then
            adjustPaintMaskTopology('grow')
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_paint_mask_topology_shrink')) then
            adjustPaintMaskTopology('shrink')
        end
        tImGui.EndDisabled()
        tImGui.TextWrapped(tLang.L('swl_paint_mask_topology_help'))
        tImGui.BeginDisabled(state.paint.lastSurfaceHit==nil or not state.meshVisible)
        if tImGui.Button(tLang.L('swl_paint_mask_subset_replace')) then
            applyHitSubsetToPaintMask('replace')
        end
        if tImGui.Button(tLang.L('swl_paint_mask_subset_add')) then
            applyHitSubsetToPaintMask('add')
        end
        if tImGui.Button(tLang.L('swl_paint_mask_subset_remove')) then
            applyHitSubsetToPaintMask('remove')
        end
        tImGui.EndDisabled()
        tImGui.TextWrapped(tLang.L('swl_paint_mask_subset_help'))
        tImGui.PushItemWidth(240)
        local maskStrengthChanged,maskStrength=tImGui.SliderFloat(
            tLang.L('swl_paint_mask_smooth_strength'),state.paint.maskSmoothStrength,0,1,'%.2f')
        tImGui.PopItemWidth()
        if maskStrengthChanged then state.paint.maskSmoothStrength=maskStrength end
        tImGui.PushItemWidth(240)
        local maskIterationsChanged,maskIterations=tImGui.SliderInt(
            tLang.L('swl_paint_mask_smooth_iterations'),state.paint.maskSmoothIterations,1,10)
        tImGui.PopItemWidth()
        if maskIterationsChanged then state.paint.maskSmoothIterations=maskIterations end
        tImGui.BeginDisabled(maskCount==0 or state.paint.maskEditMode~=0)
        if tImGui.Button(tLang.L('swl_paint_mask_smooth_apply')) then
            smoothPaintMaskFullVector()
        end
        tImGui.EndDisabled()
        tImGui.TextWrapped(tLang.L('swl_paint_mask_smooth_help'))
        tImGui.PushItemWidth(240)
        local transitionChanged,transitionRings=tImGui.SliderInt(
            tLang.L('swl_paint_mask_rigid_transition_rings'),
            state.paint.maskRigidTransitionRings,0,10)
        tImGui.PopItemWidth()
        if transitionChanged then state.paint.maskRigidTransitionRings=transitionRings end
        tImGui.BeginDisabled(maskCount==0 or state.paint.maskEditMode~=0)
        if tImGui.Button(tLang.L('swl_paint_mask_rigid_apply')) then
            rigidBindPaintMask()
        end
        tImGui.EndDisabled()
        tImGui.TextWrapped(tLang.L('swl_paint_mask_rigid_help'))
        tImGui.TextWrapped(tLang.L(state.paint.operationMode==4 and 'swl_paint_rigid_help' or
            state.paint.operationMode==3 and 'swl_paint_smooth_help' or
            state.paint.operationMode==2 and 'swl_paint_subtract_help' or 'swl_paint_add_help'))
        tImGui.Text(tLang.L('swl_paint_operation'))
        tImGui.BeginDisabled(state.paint.maskEditMode~=0)
        local previousOperation=state.paint.operationMode
        state.paint.operationMode=tImGui.RadioButton(tLang.L('swl_paint_operation_add'),
            state.paint.operationMode,1)
        state.paint.operationMode=tImGui.RadioButton(tLang.L('swl_paint_operation_subtract'),
            state.paint.operationMode,2)
        state.paint.operationMode=tImGui.RadioButton(tLang.L('swl_paint_operation_smooth'),
            state.paint.operationMode,3)
        state.paint.operationMode=tImGui.RadioButton(tLang.L('swl_paint_operation_rigid'),
            state.paint.operationMode,4)
        tImGui.EndDisabled()
        if state.paint.operationMode~=previousOperation then
            rebuildPaintCursor(state.paint.cursorHit)
        end
        tImGui.PushItemWidth(240)
        local radiusChanged,radius=tImGui.SliderFloat(tLang.L('swl_paint_brush_radius'),
            state.paint.radius,math.max(extent*0.002,0.0001),math.max(extent*0.5,0.001),'%.4g')
        tImGui.PopItemWidth()
        if radiusChanged then
            state.paint.radius=radius
            rebuildPaintCursor(state.paint.cursorHit)
        end
        local connectedOnly=tImGui.Checkbox(tLang.L('swl_paint_connected_surface_only'),
            state.paint.connectedSurfaceOnly)
        if connectedOnly~=state.paint.connectedSurfaceOnly then
            state.paint.connectedSurfaceOnly=connectedOnly
            rebuildPaintCursor(state.paint.cursorHit)
        end
        tImGui.TextWrapped(tLang.L('swl_paint_connected_surface_help'))
        local restrictSubset=tImGui.Checkbox(tLang.L('swl_paint_restrict_hit_subset'),
            state.paint.restrictToHitSubset)
        if restrictSubset~=state.paint.restrictToHitSubset then
            state.paint.restrictToHitSubset=restrictSubset
            rebuildPaintCursor(state.paint.cursorHit)
        end
        tImGui.TextWrapped(tLang.L('swl_paint_restrict_hit_subset_help'))
        local showGradient=tImGui.Checkbox(tLang.L('swl_paint_show_brush_gradient'),
            state.paint.showBrushGradient)
        if showGradient~=state.paint.showBrushGradient then
            state.paint.showBrushGradient=showGradient
            rebuildPaintCursor(state.paint.cursorHit)
        end
        local showFootprint=tImGui.Checkbox(tLang.L('swl_paint_show_brush_vertices'),
            state.paint.showBrushFootprint)
        if showFootprint~=state.paint.showBrushFootprint then
            state.paint.showBrushFootprint=showFootprint
            rebuildPaintCursor(state.paint.cursorHit)
        end
        if state.paint.operationMode~=4 then
            tImGui.PushItemWidth(240)
            local strengthChanged,strength=tImGui.SliderFloat(tLang.L('swl_paint_brush_strength'),
                state.paint.strength,0.01,1,'%.2f')
            tImGui.PopItemWidth()
            if strengthChanged then
                state.paint.strength=strength
                rebuildPaintCursor(state.paint.cursorHit)
            end
        else
            tImGui.PushItemWidth(240)
            local coreChanged,core=tImGui.SliderFloat(tLang.L('swl_paint_rigid_core'),
                state.paint.rigidCoreRatio,0,0.95,'%.2f')
            tImGui.PopItemWidth()
            if coreChanged then
                state.paint.rigidCoreRatio=core
                rebuildPaintCursor(state.paint.cursorHit)
            end
        end
        if state.paint.operationMode==3 then
            tImGui.PushItemWidth(240)
            local iterationsChanged,smoothIterations=tImGui.SliderInt(
                tLang.L('swl_paint_smooth_iterations'),state.paint.smoothIterations,1,10)
            tImGui.PopItemWidth()
            if iterationsChanged then state.paint.smoothIterations=smoothIterations end
        end
        local falloffNames={tLang.L('swl_falloff_linear'),tLang.L('swl_falloff_smooth')}
        tImGui.PushItemWidth(240)
        local falloffChanged,falloffMode=tImGui.Combo(tLang.L('swl_falloff'),
            state.paint.falloffMode,falloffNames,-1)
        tImGui.PopItemWidth()
        if falloffChanged then
            state.paint.falloffMode=falloffMode
            rebuildPaintCursor(state.paint.cursorHit)
        end
        tImGui.EndDisabled()
    end
    if state.showAdvancedDiagnostics then
    tImGui.Separator()
    showWrappedColoredText(tLang.L('swl_paint_advanced_diagnostics_notice'),
        {r=1,g=0.75,b=0.15,a=1})
    showSectionTitle('swl_paint_viewport_feedback')
    local showVertexInspector=tImGui.Checkbox(tLang.L('swl_paint_vertex_inspector'),
        state.paint.showVertexInspector)
    if showVertexInspector~=state.paint.showVertexInspector then
        state.paint.showVertexInspector=showVertexInspector
        if not showVertexInspector then
            state.paint.inspectorPinned=false
            destroyObject(state.paint.inspectorSeamMarkers)
            state.paint.inspectorSeamMarkers=nil
            state.paint.inspectorSeamReport=nil
            state.paint.inspectorSeamSyncConfirmed=false
            destroyObject(state.paint.inspectorGeometryOverlay)
            state.paint.inspectorGeometryOverlay=nil
            state.paint.inspectorGeometryReport=nil
            destroyObject(state.paint.inspectorTopologyOverlay)
            state.paint.inspectorTopologyOverlay=nil
            state.paint.inspectorTopologyReport=nil
            state.paint.inspectorNormalReport=nil
        end
        rebuildPaintCursor(state.paint.cursorHit)
        applyWorkspaceVisibility()
    end
    tImGui.TextWrapped(tLang.L('swl_paint_vertex_inspector_help'))
    if state.paint.showVertexInspector and state.paint.inspectorPinned then
        tImGui.TextColored({r=1,g=0.8,b=0.15,a=1},tLang.L('swl_paint_vertex_inspector_pinned'))
        if tImGui.Button(tLang.L('swl_paint_vertex_inspector_clear_pin')) then
            state.paint.inspectorPinned=false
            destroyObject(state.paint.inspectorSeamMarkers)
            state.paint.inspectorSeamMarkers=nil
            state.paint.inspectorSeamReport=nil
            state.paint.inspectorSeamSyncConfirmed=false
            destroyObject(state.paint.inspectorGeometryOverlay)
            state.paint.inspectorGeometryOverlay=nil
            state.paint.inspectorGeometryReport=nil
            destroyObject(state.paint.inspectorTopologyOverlay)
            state.paint.inspectorTopologyOverlay=nil
            state.paint.inspectorTopologyReport=nil
            state.paint.inspectorNormalReport=nil
            rebuildPaintCursor(state.paint.cursorHit)
            applyWorkspaceVisibility()
        end
    end
    local hoveredVertex=state.paint.hoveredVertex
    if state.paint.showVertexInspector and hoveredVertex then
        tImGui.Text(string.format(tLang.L('swl_paint_vertex_inspector_header_fmt'),
            hoveredVertex.globalIndex,hoveredVertex.subset,#hoveredVertex.influences))
    elseif state.paint.showVertexInspector then
        tImGui.TextDisabled(tLang.L('swl_paint_vertex_inspector_header_empty'))
    end
    if state.paint.showVertexInspector then
        for slot=1,4 do
            local influence=hoveredVertex and hoveredVertex.influences[slot] or nil
            if influence then
                tImGui.Text(string.format(tLang.L('swl_paint_vertex_influence_fmt'),
                    influence.name,influence.weight))
            else
                tImGui.TextDisabled(string.format(
                    tLang.L('swl_paint_vertex_influence_empty_fmt'),slot))
            end
        end
    end
    tImGui.Separator()
    showSectionTitle('swl_paint_advanced_pinned')
    local seamReport=state.paint.inspectorSeamReport
    if state.paint.showVertexInspector and state.paint.inspectorPinned and seamReport then
        tImGui.Separator()
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_vertex_seam_summary_fmt'),
            #seamReport.members,seamReport.maximumDivergence,seamReport.tolerance))
        if #seamReport.members<=1 then
            tImGui.TextDisabled(tLang.L('swl_paint_vertex_seam_none'))
        else
            tImGui.TextWrapped(tLang.L('swl_paint_vertex_seam_help'))
            for _,member in ipairs(seamReport.members) do
                local selected=member.globalIndex==hoveredVertex.globalIndex and ' *' or ''
                tImGui.Text(string.format(tLang.L('swl_paint_vertex_seam_member_fmt'),
                    member.globalIndex,member.subset,selected))
                for _,influence in ipairs(member.influences) do
                    tImGui.BulletText(string.format(tLang.L('swl_paint_vertex_influence_fmt'),
                        influence.name,influence.weight))
                end
            end
            local confirmed=tImGui.Checkbox(
                tLang.L('swl_paint_vertex_seam_sync_confirm'),
                state.paint.inspectorSeamSyncConfirmed)
            if confirmed~=state.paint.inspectorSeamSyncConfirmed then
                state.paint.inspectorSeamSyncConfirmed=confirmed
            end
            tImGui.BeginDisabled(not state.paint.inspectorSeamSyncConfirmed or
                seamReport.maximumDivergence<=1e-9)
            if tImGui.Button(tLang.L('swl_paint_vertex_seam_sync_apply')) then
                synchronizePinnedSeamWeights()
            end
            tImGui.EndDisabled()
            showItemTooltip(tLang.L('swl_paint_vertex_seam_sync_help'))
        end
    end
    if state.paint.showVertexInspector and state.paint.inspectorPinned then
        local geometryClip=state.animationClipSelected or state.skeletalPreview.selected or 1
        local geometryDuration=state.preview:getSkeletalAnimationDuration(geometryClip) or 0
        state.paint.inspectorGeometryTime=math.max(0,
            math.min(state.paint.inspectorGeometryTime or 0,geometryDuration))
        tImGui.PushItemWidth(240)
        local geometryTimeChanged,geometryTime=tImGui.SliderFloat(
            tLang.L('swl_paint_geometry_time'),state.paint.inspectorGeometryTime,
            0,math.max(geometryDuration,0),'%.3f s')
        tImGui.PopItemWidth()
        if geometryTimeChanged then
            state.paint.inspectorGeometryTime=geometryTime
            destroyObject(state.paint.inspectorGeometryOverlay)
            state.paint.inspectorGeometryOverlay=nil
            state.paint.inspectorGeometryReport=nil
            state.paint.inspectorNormalReport=nil
        end
        tImGui.TextDisabled(string.format(tLang.L('swl_paint_geometry_duration_fmt'),
            geometryClip,geometryDuration))
        if tImGui.Button(tLang.L('swl_paint_geometry_analyze')) then
            analyzePinnedDeformedGeometry()
        end
        showItemTooltip(tLang.L('swl_paint_geometry_analyze_help'))
        local geometryReport=state.paint.inspectorGeometryReport
        if geometryReport then
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_geometry_context_fmt'),
                geometryReport.clipIndex,geometryReport.time,geometryReport.triangles))
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_geometry_area_fmt'),
                geometryReport.minimumAreaRatio,geometryReport.collapsed,
                geometryReport.minimumNormalAlignment,geometryReport.inverted))
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_geometry_edges_fmt'),
                geometryReport.minimumEdgeRatio,geometryReport.maximumEdgeRatio,
                geometryReport.flagged))
            if geometryReport.collapsed>0 or geometryReport.inverted>0 then
                tImGui.TextColored({r=1,g=0.7,b=0.15,a=1},
                    tLang.L('swl_paint_geometry_risk'))
            else
                tImGui.TextDisabled(tLang.L('swl_paint_geometry_no_local_failure'))
            end
        end
        if tImGui.Button(tLang.L('swl_paint_topology_analyze')) then
            analyzePinnedBindTopology()
        end
        showItemTooltip(tLang.L('swl_paint_topology_analyze_help'))
        local topologyReport=state.paint.inspectorTopologyReport
        if topologyReport then
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_topology_summary_fmt'),
                topologyReport.boundaryEdges,topologyReport.tolerance))
            if topologyReport.nearestDistance then
                tImGui.TextWrapped(string.format(tLang.L('swl_paint_topology_nearest_fmt'),
                    topologyReport.nearestDistance))
            end
            if topologyReport.counterpartGap then
                tImGui.TextWrapped(string.format(tLang.L('swl_paint_topology_pair_fmt'),
                    topologyReport.counterpartGap))
            end
            if topologyReport.boundaryEdges==0 then
                tImGui.TextDisabled(tLang.L('swl_paint_topology_closed'))
            elseif topologyReport.nearPinned then
                tImGui.TextColored({r=1,g=0.7,b=0.15,a=1},
                    tLang.L('swl_paint_topology_open_near_pin'))
            else
                tImGui.TextDisabled(tLang.L('swl_paint_topology_open_elsewhere'))
            end
        end
        if tImGui.Button(tLang.L('swl_paint_normal_analyze')) then
            analyzePinnedSeamNormals()
        end
        showItemTooltip(tLang.L('swl_paint_normal_analyze_help'))
        local normalReport=state.paint.inspectorNormalReport
        if normalReport then
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_normal_context_fmt'),
                normalReport.clipIndex,normalReport.time,normalReport.normals,
                normalReport.copies))
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_normal_angles_fmt'),
                normalReport.bindMaximum,normalReport.deformedMaximum))
            if tImGui.Button(tLang.L('swl_paint_normal_print')) then
                printPinnedNormalReport()
            end
            showItemTooltip(tLang.L('swl_paint_normal_print_help'))
            tImGui.PushItemWidth(240)
            local thresholdChanged,threshold=tImGui.SliderFloat(
                tLang.L('swl_paint_normal_repair_threshold'),
                state.paint.normalRepairThreshold,0,180,'%.1f deg')
            tImGui.PopItemWidth()
            if thresholdChanged then
                state.paint.normalRepairThreshold=threshold
                state.paint.normalRepairConfirmed=false
                state.paint.globalNormalAudit=nil
                state.paint.globalNormalRepairConfirmed=false
                destroyObject(state.paint.globalNormalMarkers)
                state.paint.globalNormalMarkers=nil
            end
            local affected=0
            for _,entry in ipairs(normalReport.entries or {}) do
                if entry.geometric and
                        entry.geometricDifference>state.paint.normalRepairThreshold then
                    affected=affected+1
                end
            end
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_normal_repair_preview_fmt'),
                affected,normalReport.normals))
            local confirmed=tImGui.Checkbox(tLang.L('swl_paint_normal_repair_confirm'),
                state.paint.normalRepairConfirmed)
            if confirmed~=state.paint.normalRepairConfirmed then
                state.paint.normalRepairConfirmed=confirmed
            end
            tImGui.BeginDisabled(not state.paint.normalRepairConfirmed or affected==0)
            if tImGui.Button(tLang.L('swl_paint_normal_repair_apply')) then
                repairPinnedIncompatibleNormals()
            end
            tImGui.EndDisabled()
            showItemTooltip(tLang.L('swl_paint_normal_repair_help'))
            for _,entry in ipairs(normalReport.entries or {}) do
                local selected=entry.globalIndex==normalReport.selectedIndex and ' *' or ''
                if tImGui.TreeNode(string.format(tLang.L('swl_paint_normal_entry_fmt'),
                        entry.globalIndex,selected)..'##swlNormalEntry'..entry.globalIndex) then
                    tImGui.TextWrapped(string.format(tLang.L('swl_paint_normal_bind_xyz_fmt'),
                        entry.bind.x,entry.bind.y,entry.bind.z,entry.bindFromSelected))
                    if entry.deformed then
                        tImGui.TextWrapped(string.format(
                            tLang.L('swl_paint_normal_deformed_xyz_fmt'),entry.deformed.x,
                            entry.deformed.y,entry.deformed.z,entry.deformedFromSelected))
                    end
                    if entry.geometric then
                        tImGui.TextWrapped(string.format(
                            tLang.L('swl_paint_normal_geometric_fmt'),entry.geometric.x,
                            entry.geometric.y,entry.geometric.z,entry.geometricDifference))
                    else
                        tImGui.TextDisabled(tLang.L('swl_paint_normal_geometric_missing'))
                    end
                    tImGui.TreePop()
                end
            end
            if normalReport.normals<normalReport.copies then
                tImGui.TextDisabled(tLang.L('swl_paint_normal_missing'))
            elseif normalReport.bindMaximum>5 or normalReport.deformedMaximum>5 then
                tImGui.TextColored({r=1,g=0.7,b=0.15,a=1},
                    tLang.L('swl_paint_normal_discontinuous'))
            else
                tImGui.TextDisabled(tLang.L('swl_paint_normal_continuous'))
            end
        end
    end
    tImGui.Separator()
    showSectionTitle('swl_paint_advanced_weight_seams')
    tImGui.Text(tLang.L('swl_paint_global_seam_title'))
    if tImGui.Button(tLang.L('swl_paint_global_seam_analyze')) then
        analyzeGlobalSeamWeights()
    end
    showItemTooltip(tLang.L('swl_paint_global_seam_analyze_help'))
    local globalSeamAudit=state.paint.globalSeamAudit
    if globalSeamAudit then
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_global_seam_summary_fmt'),
            globalSeamAudit.groupCount,globalSeamAudit.totalGroups,
            globalSeamAudit.vertexCount,globalSeamAudit.maximumDivergence,
            globalSeamAudit.tolerance))
        if globalSeamAudit.groupCount==0 then
            tImGui.TextDisabled(tLang.L('swl_paint_global_seam_clean'))
        else
            local confirmed=tImGui.Checkbox(tLang.L('swl_paint_global_seam_sync_confirm'),
                state.paint.globalSeamSyncConfirmed)
            if confirmed~=state.paint.globalSeamSyncConfirmed then
                state.paint.globalSeamSyncConfirmed=confirmed
            end
            tImGui.BeginDisabled(not state.paint.globalSeamSyncConfirmed)
            if tImGui.Button(tLang.L('swl_paint_global_seam_sync_apply')) then
                synchronizeGlobalSeamWeights()
            end
            tImGui.EndDisabled()
            showItemTooltip(tLang.L('swl_paint_global_seam_sync_help'))
        end
    end
    tImGui.Separator()
    showSectionTitle('swl_paint_advanced_normals')
    tImGui.Text(tLang.L('swl_paint_global_normal_title'))
    tImGui.PushItemWidth(240)
    local globalThresholdChanged,globalThreshold=tImGui.SliderFloat(
        tLang.L('swl_paint_normal_repair_threshold')..'##globalNormalThreshold',
        state.paint.normalRepairThreshold,0,180,'%.1f deg')
    tImGui.PopItemWidth()
    if globalThresholdChanged then
        state.paint.normalRepairThreshold=globalThreshold
        state.paint.normalRepairConfirmed=false
        state.paint.globalNormalAudit=nil
        state.paint.globalNormalRepairConfirmed=false
        destroyObject(state.paint.globalNormalMarkers)
        state.paint.globalNormalMarkers=nil
    end
    if tImGui.Button(tLang.L('swl_paint_global_normal_analyze')) then
        analyzeGlobalNormalCompatibility()
    end
    showItemTooltip(tLang.L('swl_paint_global_normal_analyze_help'))
    local globalNormalAudit=state.paint.globalNormalAudit
    if globalNormalAudit then
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_global_normal_summary_fmt'),
            globalNormalAudit.affectedCount,globalNormalAudit.usable,
            globalNormalAudit.missing,globalNormalAudit.maximumDifference,
            globalNormalAudit.threshold))
        if globalNormalAudit.affectedCount==0 then
            tImGui.TextDisabled(tLang.L('swl_paint_global_normal_clean'))
        else
            local confirmed=tImGui.Checkbox(tLang.L('swl_paint_global_normal_confirm'),
                state.paint.globalNormalRepairConfirmed)
            if confirmed~=state.paint.globalNormalRepairConfirmed then
                state.paint.globalNormalRepairConfirmed=confirmed
            end
            tImGui.BeginDisabled(not state.paint.globalNormalRepairConfirmed)
            if tImGui.Button(tLang.L('swl_paint_global_normal_apply')) then
                repairGlobalIncompatibleNormals()
            end
            tImGui.EndDisabled()
            showItemTooltip(tLang.L('swl_paint_global_normal_apply_help'))
        end
    end
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_paint_normal_smooth_title'))
    tImGui.PushItemWidth(240)
    local smoothAngleChanged,smoothAngle=tImGui.SliderFloat(
        tLang.L('swl_paint_normal_smooth_angle'),state.paint.normalSmoothAngle,
        0,180,'%.1f deg')
    tImGui.PopItemWidth()
    if smoothAngleChanged then
        state.paint.normalSmoothAngle=smoothAngle
        state.paint.globalNormalSmoothAudit=nil
        state.paint.globalNormalSmoothConfirmed=false
        destroyObject(state.paint.globalNormalSmoothMarkers)
        state.paint.globalNormalSmoothMarkers=nil
    end
    if tImGui.Button(tLang.L('swl_paint_normal_smooth_analyze')) then
        analyzeGlobalCoincidentNormalSmoothing()
    end
    showItemTooltip(tLang.L('swl_paint_normal_smooth_analyze_help'))
    local smoothAudit=state.paint.globalNormalSmoothAudit
    if smoothAudit then
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_normal_smooth_summary_fmt'),
            smoothAudit.affectedCount,smoothAudit.groupCount,smoothAudit.totalSeams,
            smoothAudit.angle))
        if smoothAudit.affectedCount==0 then
            tImGui.TextDisabled(tLang.L('swl_paint_normal_smooth_clean'))
        else
            local confirmed=tImGui.Checkbox(tLang.L('swl_paint_normal_smooth_confirm'),
                state.paint.globalNormalSmoothConfirmed)
            if confirmed~=state.paint.globalNormalSmoothConfirmed then
                state.paint.globalNormalSmoothConfirmed=confirmed
            end
            tImGui.BeginDisabled(not state.paint.globalNormalSmoothConfirmed)
            if tImGui.Button(tLang.L('swl_paint_normal_smooth_apply')) then
                smoothGlobalCoincidentNormals()
            end
            tImGui.EndDisabled()
            showItemTooltip(tLang.L('swl_paint_normal_smooth_apply_help'))
        end
    end
    tImGui.Separator()
    showSectionTitle('swl_paint_advanced_source_geometry')
    tImGui.Text(tLang.L('swl_paint_exact_position_title'))
    if tImGui.Button(tLang.L('swl_paint_exact_position_analyze')) then
        analyzeExactCoincidentPositions()
    end
    showItemTooltip(tLang.L('swl_paint_exact_position_help'))
    local positionAudit=state.paint.exactSeamPositionAudit
    if positionAudit then
        tImGui.TextWrapped(string.format(tLang.L('swl_paint_exact_position_summary_fmt'),
            positionAudit.nonzeroGroups,positionAudit.totalGroups,
            positionAudit.comparedCopies,positionAudit.comparedPairs,
            positionAudit.affectedVertices,positionAudit.minimumNonzero,
            positionAudit.maximumDistance,positionAudit.tolerance))
        if positionAudit.pinnedDistance~=nil then
            tImGui.TextWrapped(string.format(tLang.L('swl_paint_exact_position_pinned_fmt'),
                positionAudit.pinnedCopies,positionAudit.pinnedPairs,
                positionAudit.pinnedDistance))
        end
        if positionAudit.nonzeroGroups==0 then
            tImGui.TextDisabled(tLang.L('swl_paint_exact_position_identical'))
        else
            tImGui.TextColored({r=1,g=0.7,b=0.15,a=1},
                tLang.L('swl_paint_exact_position_nonzero'))
        end
    end
    end
    local strokeSafetyReport=state.paint.strokeSafetyReport
    tImGui.BeginDisabled(strokeSafetyReport==nil)
    local strokeSafetyVisible=tImGui.Checkbox(tLang.L('swl_paint_stroke_safety_overlay'),
        state.paint.strokeSafetyOverlayVisible)
    if strokeSafetyVisible~=state.paint.strokeSafetyOverlayVisible then
        state.paint.strokeSafetyOverlayVisible=strokeSafetyVisible
        applyWorkspaceVisibility()
    end
    tImGui.EndDisabled()
    if strokeSafetyReport then
        tImGui.TextWrapped(tLang.L('swl_paint_stroke_safety_legend'))
        tImGui.Text(string.format(tLang.L('swl_paint_stroke_safety_report_fmt'),
            strokeSafetyReport.changedVertices,strokeSafetyReport.checkedFaces,
            strokeSafetyReport.poseSamples,strokeSafetyReport.unsafeFaces,
            strokeSafetyReport.unsafeFaceSamples,strokeSafetyReport.minimumAreaRatio,
            strokeSafetyReport.maximumOrientationDegrees,
            strokeSafetyReport.minimumNormalAlignment))
    end
    showWrappedDisabledText(tLang.L('swl_heatmap_legend'))
    local geometry=state.paint.geometry
    if geometry then
        tImGui.Text(string.format(tLang.L('swl_paint_geometry_fmt'),
            #geometry.vertices,#geometry.triangles))
        if #state.paint.heatmapLines>0 then
            showWrappedDisabledText(tLang.L(state.paint.heatmapIndexed and
                'swl_paint_indexed_heatmap' or 'swl_paint_nonindexed_heatmap'))
        end
    end
    if state.paint.visualizationMode~=1 then
        showWrappedDisabledText(tLang.L('swl_paint_diagnostic_read_only'))
    elseif state.paint.cursorHit then
        local p=state.paint.cursorHit.point
        tImGui.Text(string.format(tLang.L('swl_paint_hit_fmt'),p.x,p.y,p.z,
            state.paint.cursorHit.triangle.subset))
    else
        showWrappedDisabledText(tLang.L('swl_paint_no_hit'))
    end
    showRollbackControls('Paint')
end

local skeletalEasingNames={'Linear','Ease In','Ease Out','Ease In Out','Smoothstep','Cubic Bezier'}

local function skeletalChannelLabel(mask)
    local channels={}
    if (mask & 1)~=0 then channels[#channels+1]='T' end
    if (mask & 2)~=0 then channels[#channels+1]='R' end
    if (mask & 4)~=0 then channels[#channels+1]='S' end
    return table.concat(channels,'+')
end

local function skeletalKeyFloat(label,id,value)
    tImGui.PushItemWidth(130)
    local changed,result=tImGui.InputFloat(label..'##'..id,value,0,0,'%.6g',
        tImGui.Flags('ImGuiInputTextFlags_None'))
    tImGui.PopItemWidth()
    return changed and result or value
end

local function timelineSelectionId(trackIndex,keyIndex)
    return tostring(trackIndex)..':'..tostring(keyIndex)
end

local function timelineSelectionCount()
    local count=0
    for _,selected in pairs(state.animationTimelineSelection) do
        if selected then count=count+1 end
    end
    return count
end

local function collectTimelineSelection(clip)
    local references={}
    local minimumTime,maximumTime=math.huge,-math.huge
    for trackIndex,track in ipairs(clip.tracks or {}) do
        for keyIndex,key in ipairs(track.keys or {}) do
            if state.animationTimelineSelection[timelineSelectionId(trackIndex,keyIndex)] then
                references[#references+1]=trackIndex
                references[#references+1]=keyIndex
                minimumTime=math.min(minimumTime,key.time or 0)
                maximumTime=math.max(maximumTime,key.time or 0)
            end
        end
    end
    return references,minimumTime,maximumTime
end

local function copyTimelineSelection()
    local clip=state.animationTimelineClip
    if state.workspace~='animation' or not clip then return false end
    local items={}
    local minimumTime,maximumTime=math.huge,-math.huge
    for trackIndex,track in ipairs(clip.tracks or {}) do
        for keyIndex,key in ipairs(track.keys or {}) do
            if state.animationTimelineSelection[timelineSelectionId(trackIndex,keyIndex)] then
                local time=key.time or 0
                local t=key.translation or {}
                local q=key.rotation or {}
                local s=key.scale or {}
                local b=key.bezier or {}
                items[#items+1]={track.boneId,track.channelMask or 0,time,
                    t.x or 0,t.y or 0,t.z or 0,q.x or 0,q.y or 0,q.z or 0,q.w or 1,
                    s.x or 1,s.y or 1,s.z or 1,key.easing or 0,
                    b.x1 or 0,b.y1 or 0,b.x2 or 1,b.y2 or 1}
                minimumTime=math.min(minimumTime,time)
                maximumTime=math.max(maximumTime,time)
            end
        end
    end
    if #items==0 then return false end
    state.animationKeyClipboard={clipId=clip.clipId,clipName=clip.name,items=items,
        minimumTime=minimumTime,maximumTime=maximumTime}
    local message=string.format(tLang.L('swl_animation_timeline_keys_copied_fmt'),#items)
    setStatus(message,false)
    tUtil.bRightSide=true
    tUtil.sMessageOverlay=false
    tUtil.showMessage(message,4.0)
    return true
end

local function pasteTimelineClipboardAtPlayhead()
    local clip=state.animationTimelineClip
    local clipboard=state.animationKeyClipboard
    if state.workspace~='animation' or not clip or not clipboard then return false end
    local destinationEnd=(state.authoringTime or 0)+
        (clipboard.maximumTime-clipboard.minimumTime)
    if destinationEnd>(clip.duration or 0)+1e-6 then
        setStatus(tLang.L('swl_animation_timeline_clipboard_out_of_range'),true)
        return false
    end
    local snapshot=stageRollbackSnapshot()
    local pasted=snapshot and select(1,safeCall(function()
        return state.meshD:pasteSkeletalKeys(state.animationClipSelected,clipboard.items,
            clipboard.minimumTime,state.authoringTime or 0)
    end)) or false
    if not pasted then
        if snapshot then discardRollbackSnapshot(snapshot) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_paste_keys')
    state.modified=true
    state.animationTimelineSelection={}
    state.animationTimelineTrackIndex=nil
    state.animationTimelineKeyIndex=nil
    clearAuthoringOverride()
    local count=#(clipboard.items or {})
    setStatus(string.format(tLang.L('swl_animation_timeline_keys_pasted_fmt'),count),false)
    return true
end

local function copySelectedBonePose()
    local bone=getBones()[state.boneIndex]
    local posed=state.authoringPose and state.authoringPose.bones and
        state.authoringPose.bones[state.boneIndex] or nil
    if state.workspace~='animation' or not bone or not posed then return false end
    local t=posed.localTranslation or {}
    local q=posed.localRotation or {}
    local s=posed.localScale or {}
    state.animationBonePoseClipboard={boneId=bone.boneId,boneName=bone.name,
        translation={x=t.x or 0,y=t.y or 0,z=t.z or 0},
        rotation={x=q.x or 0,y=q.y or 0,z=q.z or 0,w=q.w or 1},
        scale={x=s.x or 1,y=s.y or 1,z=s.z or 1}}
    local message=string.format(tLang.L('swl_animation_bone_pose_copied_fmt'),bone.name or '?')
    setStatus(message,false)
    tUtil.bRightSide=true
    tUtil.sMessageOverlay=false
    tUtil.showMessage(message,4.0)
    return true
end

local function pasteSelectedBonePoseAtPlayhead()
    local clipboard=state.animationBonePoseClipboard
    local clip=state.animationTimelineClip
    if state.workspace~='animation' or not clipboard or not clip then return false end
    local boneIndex=nil
    for index,bone in ipairs(getBones()) do
        if bone.boneId==clipboard.boneId then boneIndex=index break end
    end
    if not boneIndex then
        setStatus(tLang.L('swl_animation_bone_pose_missing'),true)
        return false
    end
    local t,q,s=clipboard.translation,clipboard.rotation,clipboard.scale
    local snapshot=stageRollbackSnapshot()
    local committed=snapshot and select(1,safeCall(function()
        return state.meshD:commitSkeletalAuthoringKey(state.animationClipSelected,boneIndex,
            state.authoringTime or 0,7,t.x,t.y,t.z,q.x,q.y,q.z,q.w,s.x,s.y,s.z)
    end)) or false
    if not committed then
        if snapshot then discardRollbackSnapshot(snapshot) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_paste_bone_pose')
    state.modified=true
    state.boneIndex=boneIndex
    clearAuthoringOverride()
    refreshBindReport()
    refreshAuthoringPose(clip)
    setStatus(string.format(tLang.L('swl_animation_bone_pose_pasted_fmt'),
        clipboard.boneName or '?'),false)
    return true
end

local function copySkeletonPose()
    local bones=getBones()
    local posedBones=state.authoringPose and state.authoringPose.bones or nil
    if state.workspace~='animation' or #bones==0 or not posedBones or
            #posedBones~=#bones then return false end
    local items={}
    for index,bone in ipairs(bones) do
        local posed=posedBones[index]
        if not posed then return false end
        local t=posed.localTranslation or {}
        local q=posed.localRotation or {}
        local s=posed.localScale or {}
        items[#items+1]={bone.boneId,t.x or 0,t.y or 0,t.z or 0,
            q.x or 0,q.y or 0,q.z or 0,q.w or 1,s.x or 1,s.y or 1,s.z or 1}
    end
    state.animationSkeletonPoseClipboard={items=items,boneCount=#items,
        clipName=state.animationTimelineClip and state.animationTimelineClip.name or '?',
        sourceTime=state.authoringTime or 0}
    local message=string.format(tLang.L('swl_animation_skeleton_pose_copied_fmt'),#items)
    setStatus(message,false)
    tUtil.bRightSide=true
    tUtil.sMessageOverlay=false
    tUtil.showMessage(message,4.0)
    return true
end

local function pasteSkeletonPoseAtPlayhead()
    local clipboard=state.animationSkeletonPoseClipboard
    local clip=state.animationTimelineClip
    if state.workspace~='animation' or not clipboard or not clip then return false end
    if clipboard.boneCount~=#getBones() then
        setStatus(tLang.L('swl_animation_skeleton_pose_incompatible'),true)
        return false
    end
    local snapshot=stageRollbackSnapshot()
    local committed=snapshot and select(1,safeCall(function()
        return state.meshD:commitSkeletalAuthoringPose(state.animationClipSelected,
            state.authoringTime or 0,clipboard.items)
    end)) or false
    if not committed then
        if snapshot then discardRollbackSnapshot(snapshot) end
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_paste_skeleton_pose')
    state.modified=true
    clearAuthoringOverride()
    refreshBindReport()
    refreshAuthoringPose(clip)
    setStatus(string.format(tLang.L('swl_animation_skeleton_pose_pasted_fmt'),
        clipboard.boneCount),false)
    return true
end

local function timelineRemovalImpact(clip)
    local startTime=math.max(0,math.min(clip.duration or 0,state.authoringTime or 0))
    local endTime=math.min(clip.duration or 0,
        startTime+math.max(0,state.animationTimelineRemovalDuration or 0))
    local removedKeys,emptyTracks=0,0
    for _,track in ipairs(clip.tracks or {}) do
        local trackRemoved=0
        for _,key in ipairs(track.keys or {}) do
            local time=key.time or 0
            if time+1e-6>=startTime and time<endTime-1e-6 then
                trackRemoved=trackRemoved+1
            end
        end
        removedKeys=removedKeys+trackRemoved
        if trackRemoved>0 and trackRemoved==#(track.keys or {}) then
            emptyTracks=emptyTracks+1
        end
    end
    return {startTime=startTime,endTime=endTime,duration=endTime-startTime,
        removedKeys=removedKeys,emptyTracks=emptyTracks}
end

local function snapTimelineTime(time,duration)
    local value=math.max(0,math.min(duration or time,time or 0))
    local step=state.animationTimelineSnapStep or 0
    if state.animationTimelineSnapEnabled and step>1e-6 then
        value=math.floor(value/step+0.5)*step
        value=math.max(0,math.min(duration or value,value))
    end
    return value
end

local function niceTimelineTickStep(rawStep)
    local safe=math.max(rawStep or 0,1e-9)
    local power=10^math.floor(math.log(safe)/math.log(10))
    local normalized=safe/power
    local factor=normalized<=1 and 1 or normalized<=2 and 2 or normalized<=5 and 5 or 10
    return factor*power
end

local function timelineTickFormat(step)
    if step>=1 then return '%.2f' end
    if step>=0.1 then return '%.2f' end
    if step>=0.01 then return '%.3f' end
    if step>=0.001 then return '%.4f' end
    return '%.6f'
end

local function commitTimelineKeyDrag(drag)
    if not drag or not drag.moved or drag.invalid then return false end
    local snapshot=stageRollbackSnapshot()
    local references={}
    for _,member in ipairs(drag.members or {}) do
        references[#references+1]=member.trackIndex
        references[#references+1]=member.keyIndex
    end
    local updated=snapshot and select(1,safeCall(function()
        return state.meshD:moveSkeletalKeys(state.animationClipSelected,references,drag.delta)
    end)) or false
    if not updated then
        if snapshot then discardRollbackSnapshot(snapshot) end
        setStatus(tLang.L('swl_animation_timeline_key_move_failed'),true)
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_move_keys')
    state.modified=true
    state.animationKeyEdits={}
    state.animationTimelineTrackIndex=nil
    state.animationTimelineKeyIndex=nil
    state.authoringTime=drag.previewTime
    clearAuthoringOverride()
    setStatus(#drag.members>1 and string.format(
        tLang.L('swl_animation_timeline_keys_moved_fmt'),#drag.members) or
        tLang.L('swl_animation_timeline_key_moved'),false)
    return true
end

local function showSkeletalTimeline(clip)
    local tracks=clip.tracks or {}
    local duration=math.max(clip.duration or 0,0.0001)
    local rulerHeight,rowHeight=24,24
    local contentRows=math.max(1,#tracks)
    local outerAvailable=tImGui.GetContentRegionAvail()
    local viewportHeight=math.max(100,outerAvailable.y or 220)
    tImGui.BeginChild('##swlSkeletalTimelineScroll',{x=0,y=viewportHeight},true)
    local available=tImGui.GetContentRegionAvail()
    local width=math.max(220,available.x or 220)
    local labelWidth=190
    local height=rulerHeight+contentRows*rowHeight+4
    tImGui.InvisibleButton('##swlSkeletalTimeline',{x=width,y=height})
    local minimum=tImGui.GetItemRectMin()
    local maximum=tImGui.GetItemRectMax()
    local x0,x1=minimum.x+labelWidth,maximum.x-6
    local timelineWidth=math.max(1,x1-x0)
    if state.animationTimelineViewClipId~=clip.clipId then
        state.animationTimelineViewClipId=clip.clipId
        state.animationTimelineViewStart=0
        state.animationTimelineViewEnd=duration
        state.animationTimelinePan=nil
    end
    local viewStart=math.max(0,math.min(duration,state.animationTimelineViewStart or 0))
    local viewEnd=math.max(viewStart+0.0001,
        math.min(duration,state.animationTimelineViewEnd or duration))
    if viewEnd>duration then viewEnd=duration end
    if viewEnd-viewStart<0.0001 then
        viewStart=math.max(0,math.min(viewStart,duration-0.0001))
        viewEnd=math.min(duration,viewStart+0.0001)
    end
    local viewDuration=math.max(0.0001,viewEnd-viewStart)
    local function timeToX(time)
        return x0+timelineWidth*((time or 0)-viewStart)/viewDuration
    end
    local function xToTime(x)
        return viewStart+(x-x0)/timelineWidth*viewDuration
    end
    local activeDrag=state.animationTimelineDrag
    local selectionBox=state.animationTimelineBox
    local hovered=tImGui.IsItemHovered(0)
    if hovered and state.controlDown then
        local wheel=tImGui.GetZoom()
        if math.abs(wheel)>0.0001 then
            local mouse=tImGui.GetMousePos()
            local anchor=math.max(viewStart,math.min(viewEnd,xToTime(mouse.x)))
            local minimumView=math.min(duration,0.001)
            local newDuration=math.max(minimumView,
                math.min(duration,viewDuration*math.exp(-wheel*0.2)))
            local fraction=(anchor-viewStart)/viewDuration
            viewStart=anchor-newDuration*fraction
            viewStart=math.max(0,math.min(duration-newDuration,viewStart))
            viewEnd=viewStart+newDuration
        end
    end
    if hovered and tImGui.IsMouseClicked(2,false) then
        state.animationTimelinePan={start=viewStart,duration=viewDuration}
    end
    if state.animationTimelinePan and tImGui.IsMouseDown(2) then
        local delta=tImGui.GetMouseDragDelta(2,0)
        local pan=state.animationTimelinePan
        viewStart=pan.start-(delta.x or 0)/timelineWidth*pan.duration
        viewStart=math.max(0,math.min(duration-pan.duration,viewStart))
        viewEnd=viewStart+pan.duration
    elseif state.animationTimelinePan then
        state.animationTimelinePan=nil
    end
    state.animationTimelineViewStart=viewStart
    state.animationTimelineViewEnd=viewEnd
    viewDuration=math.max(0.0001,viewEnd-viewStart)
    if selectionBox and tImGui.IsMouseDown(0) then
        local mouse=tImGui.GetMousePos()
        selectionBox.currentX=math.max(minimum.x,math.min(maximum.x,mouse.x))
        selectionBox.currentY=math.max(minimum.y,math.min(maximum.y,mouse.y))
        selectionBox.moved=(selectionBox.currentX-selectionBox.startX)^2+
            (selectionBox.currentY-selectionBox.startY)^2>16
    end
    if activeDrag and tImGui.IsMouseDown(0) then
        local mouse=tImGui.GetMousePos()
        local requestedTime=snapTimelineTime(xToTime(mouse.x),duration)
        activeDrag.delta=math.max(-activeDrag.minimumTime,
            math.min(duration-activeDrag.maximumTime,requestedTime-activeDrag.anchorTime))
        activeDrag.previewTime=activeDrag.anchorTime+activeDrag.delta
        activeDrag.moved=math.abs(mouse.x-activeDrag.startX)>2
        activeDrag.invalid=false
        for _,member in ipairs(activeDrag.members) do
            local track=tracks[member.trackIndex]
            for keyIndex,key in ipairs(track and track.keys or {}) do
                if not activeDrag.memberIds[timelineSelectionId(member.trackIndex,keyIndex)] then
                    local otherTime=key.time or 0
                    local movedTime=member.originalTime+activeDrag.delta
                    local movedX=timeToX(movedTime)
                    local otherX=timeToX(otherTime)
                    if math.abs(movedX-otherX)<=8 or
                            math.abs(otherTime-movedTime)<=1.0e-6 then
                        activeDrag.delta=otherTime-member.originalTime
                        activeDrag.previewTime=activeDrag.anchorTime+activeDrag.delta
                        activeDrag.invalid=true
                        break
                    end
                end
            end
            if activeDrag.invalid then break end
        end
        if activeDrag.moved then
            state.authoringTime=activeDrag.previewTime
            clearAuthoringOverride()
            refreshAuthoringPose(clip)
        end
    end
    tImGui.AddRectFilled(minimum,maximum,{r=0.055,g=0.065,b=0.085,a=1},3,0)
    tImGui.AddLine({x=x0,y=minimum.y},{x=x0,y=maximum.y},
        {r=0.35,g=0.38,b=0.45,a=1},1)
    local tickStep=niceTimelineTickStep(viewDuration/math.max(1,timelineWidth/90))
    local tickFormat=timelineTickFormat(tickStep)
    local tickTime=math.ceil((viewStart-1e-9)/tickStep)*tickStep
    local tickCount=0
    local tickPositions={}
    while tickTime<=viewEnd+tickStep*1e-5 and tickCount<200 do
        local tx=timeToX(tickTime)
        if tx>=x0-1 and tx<=x1+1 then
            tickPositions[#tickPositions+1]=tx
            tImGui.AddLine({x=tx,y=minimum.y},{x=tx,y=minimum.y+rulerHeight},
                {r=0.28,g=0.3,b=0.36,a=0.65},1)
            tImGui.AddText({x=tx+2,y=minimum.y+3},{r=0.75,g=0.78,b=0.85,a=1},
                string.format(tickFormat,math.abs(tickTime)<tickStep*1e-5 and 0 or tickTime))
        end
        tickTime=tickTime+tickStep
        tickCount=tickCount+1
    end
    local markerPositions={}
    local ordinaryMarkers={}
    local draggedMarkers={}
    local scrollY=tImGui.GetScrollY()
    local firstVisible=math.max(1,math.floor(math.max(0,scrollY-rulerHeight)/rowHeight)+1)
    local lastVisible=math.min(#tracks,
        math.ceil(math.max(0,scrollY+viewportHeight-rulerHeight)/rowHeight)+1)
    for trackIndex=firstVisible,lastVisible do
        local track=tracks[trackIndex]
        local y=minimum.y+rulerHeight+(trackIndex-0.5)*rowHeight
        if trackIndex%2==0 then
            tImGui.AddRectFilled({x=minimum.x,y=y-rowHeight*0.5},
                {x=maximum.x,y=y+rowHeight*0.5},{r=0.08,g=0.09,b=0.115,a=1},0,0)
        end
        local label=(track.boneName or '?')..' ['..skeletalChannelLabel(track.channelMask or 0)..']'
        if #label>30 then label=label:sub(1,27)..'...' end
        tImGui.AddText({x=minimum.x+4,y=y-7},{r=0.8,g=0.82,b=0.88,a=1},label)
        tImGui.AddLine({x=x0,y=y},{x=x1,y=y},{r=0.22,g=0.24,b=0.3,a=1},1)
        markerPositions[trackIndex]={}
        for keyIndex,key in ipairs(track.keys or {}) do
            local member=activeDrag and activeDrag.memberIds[
                timelineSelectionId(trackIndex,keyIndex)] or nil
            local dragged=member~=nil
            local displayTime=dragged and (member.originalTime+activeDrag.delta) or (key.time or 0)
            local kx=timeToX(displayTime)
            local selected=state.animationTimelineSelection[
                timelineSelectionId(trackIndex,keyIndex)]==true
            if dragged and kx>=x0 and kx<=x1 then
                draggedMarkers[#draggedMarkers+1]={x=kx,y=y,invalid=activeDrag.invalid}
            elseif kx>=x0 and kx<=x1 then
                ordinaryMarkers[#ordinaryMarkers+1]={x=kx,y=y,selected=selected}
            end
            if kx>=x0 and kx<=x1 then
                markerPositions[trackIndex][keyIndex]={x=kx,y=y,key=key,track=track}
            end
        end
    end
    for _,tx in ipairs(tickPositions) do
        tImGui.AddLine({x=tx,y=minimum.y+rulerHeight},{x=tx,y=maximum.y},
            {r=0.28,g=0.3,b=0.36,a=0.42},1)
    end
    if state.animationTimelineRemovalPreview then
        local impact=timelineRemovalImpact(clip)
        local rangeX0=math.max(x0,math.min(x1,timeToX(impact.startTime)))
        local rangeX1=math.max(x0,math.min(x1,timeToX(impact.endTime)))
        if rangeX1>rangeX0 then
            tImGui.AddRectFilled({x=rangeX0,y=minimum.y},{x=rangeX1,y=maximum.y},
                {r=1,g=0.25,b=0.12,a=0.16},0,0)
            tImGui.AddLine({x=rangeX0,y=minimum.y},{x=rangeX0,y=maximum.y},
                {r=1,g=0.45,b=0.2,a=0.9},1.5)
            tImGui.AddLine({x=rangeX1,y=minimum.y},{x=rangeX1,y=maximum.y},
                {r=1,g=0.45,b=0.2,a=0.9},1.5)
        end
    end
    for _,marker in ipairs(ordinaryMarkers) do
        tImGui.AddCircleFilled({x=marker.x,y=marker.y},marker.selected and 6 or 4,
            marker.selected and {r=1,g=0.75,b=0.1,a=1} or
            {r=0.75,g=0.35,b=0.9,a=1},12)
    end
    for _,draggedMarker in ipairs(draggedMarkers) do
        tImGui.AddCircleFilled({x=draggedMarker.x,y=draggedMarker.y},7,
            draggedMarker.invalid and {r=1,g=0.08,b=0.08,a=1} or
                {r=1,g=0.75,b=0.1,a=1},14)
    end
    if selectionBox and selectionBox.moved then
        local bx0,bx1=math.min(selectionBox.startX,selectionBox.currentX),
            math.max(selectionBox.startX,selectionBox.currentX)
        local by0,by1=math.min(selectionBox.startY,selectionBox.currentY),
            math.max(selectionBox.startY,selectionBox.currentY)
        tImGui.AddRectFilled({x=bx0,y=by0},{x=bx1,y=by1},
            {r=0.15,g=0.55,b=1,a=0.16},0,0)
        tImGui.AddRect({x=bx0,y=by0},{x=bx1,y=by1},
            {r=0.25,g=0.7,b=1,a=1},0,0,1.5)
    end
    local playheadX=timeToX(state.authoringTime or 0)
    if playheadX>=x0 and playheadX<=x1 then
        tImGui.AddLine({x=playheadX,y=minimum.y},{x=playheadX,y=maximum.y},
            {r=1,g=0.25,b=0.15,a=1},2)
    end
    if tImGui.IsItemHovered(0) and tImGui.IsMouseClicked(0,false) then
        if state.animationPlayback.playing then state.animationPlayback.paused=true end
        local mouse=tImGui.GetMousePos()
        local row=math.floor((mouse.y-(minimum.y+rulerHeight))/rowHeight)+1
        local nearestIndex,nearestDistance=nil,math.huge
        for keyIndex,marker in pairs(markerPositions[row] or {}) do
            local distance=math.sqrt((mouse.x-marker.x)^2+(mouse.y-marker.y)^2)
            if distance<nearestDistance then nearestIndex,nearestDistance=keyIndex,distance end
        end
        if nearestIndex and nearestDistance<=9 then
            local marker=markerPositions[row][nearestIndex]
            local selectionId=timelineSelectionId(row,nearestIndex)
            local wasSelected=state.animationTimelineSelection[selectionId]==true
            if state.controlDown then
                state.animationTimelineSelection[selectionId]=
                    not state.animationTimelineSelection[selectionId] or nil
            elseif not wasSelected or timelineSelectionCount()<=1 then
                state.animationTimelineSelection={[selectionId]=true}
            end
            state.animationTimelineTrackIndex=row
            state.animationTimelineKeyIndex=nearestIndex
            if marker.track.boneIndex then state.boneIndex=marker.track.boneIndex end
            state.authoringTime=marker.key.time or 0
            if not state.controlDown and state.animationTimelineSelection[selectionId] then
                local members,memberIds={},{}
                local minimumTime,maximumTime=math.huge,-math.huge
                for trackIndex,track in ipairs(tracks) do
                    for keyIndex,key in ipairs(track.keys or {}) do
                        local id=timelineSelectionId(trackIndex,keyIndex)
                        if state.animationTimelineSelection[id] then
                            local member={trackIndex=trackIndex,keyIndex=keyIndex,
                                originalTime=key.time or 0}
                            members[#members+1]=member; memberIds[id]=member
                            minimumTime=math.min(minimumTime,member.originalTime)
                            maximumTime=math.max(maximumTime,member.originalTime)
                        end
                    end
                end
                state.animationTimelineDrag={members=members,memberIds=memberIds,
                    anchorTime=marker.key.time or 0,previewTime=marker.key.time or 0,
                    minimumTime=minimumTime,maximumTime=maximumTime,delta=0,
                    startX=mouse.x,moved=false,invalid=false}
            end
        elseif mouse.x>=x0 then
            state.animationTimelineBox={startX=mouse.x,startY=mouse.y,
                currentX=mouse.x,currentY=mouse.y,moved=false,additive=state.controlDown}
        end
        clearAuthoringOverride()
        refreshAuthoringPose(clip)
    end
    if activeDrag and tImGui.IsMouseReleased(0) then
        if activeDrag.moved and activeDrag.invalid then
            state.authoringTime=activeDrag.anchorTime
            clearAuthoringOverride()
            refreshAuthoringPose(clip)
            setStatus(tLang.L('swl_animation_timeline_key_collision'),true)
        elseif activeDrag.moved then
            commitTimelineKeyDrag(activeDrag)
        end
        state.animationTimelineDrag=nil
    end
    if selectionBox and tImGui.IsMouseReleased(0) then
        if selectionBox.moved then
            if not selectionBox.additive then state.animationTimelineSelection={} end
            local bx0,bx1=math.min(selectionBox.startX,selectionBox.currentX),
                math.max(selectionBox.startX,selectionBox.currentX)
            local by0,by1=math.min(selectionBox.startY,selectionBox.currentY),
                math.max(selectionBox.startY,selectionBox.currentY)
            for trackIndex,markers in pairs(markerPositions) do
                for keyIndex,marker in pairs(markers) do
                    if marker.x>=bx0 and marker.x<=bx1 and marker.y>=by0 and marker.y<=by1 then
                        state.animationTimelineSelection[
                            timelineSelectionId(trackIndex,keyIndex)]=true
                    end
                end
            end
            state.animationTimelineTrackIndex=nil
            state.animationTimelineKeyIndex=nil
        else
            state.animationTimelineSelection=selectionBox.additive and
                state.animationTimelineSelection or {}
            state.animationTimelineTrackIndex=nil; state.animationTimelineKeyIndex=nil
            state.authoringTime=snapTimelineTime(xToTime(selectionBox.startX),duration)
            clearAuthoringOverride()
            refreshAuthoringPose(clip)
        end
        state.animationTimelineBox=nil
    end
    tImGui.EndChild()
end

local function showSkeletalTimelineWindow()
    if state.workspace~='animation' or not state.animationTimelineClip then return end
    local screenWidth,screenHeight=mbm.getRealSizeScreen()
    local windowHeight=280
    local left=math.max(0,math.min(state.leftPanelRight or 440,screenWidth-220))
    tImGui.SetNextWindowPos({x=left,y=math.max(0,screenHeight-windowHeight)},
        tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x=math.max(220,screenWidth-left),y=windowHeight},
        tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSizeConstraints({x=420,y=180},
        {x=math.max(420,screenWidth),y=math.max(180,screenHeight)})
    local opened=tImGui.Begin(tLang.L('swl_animation_timeline')..'##swlTimelineWindow',false,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if opened then
        local playback=state.animationPlayback
        local selectedCount=timelineSelectionCount()
        tImGui.Text(string.format(tLang.L('swl_animation_timeline_selection_fmt'),selectedCount))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_animation_timeline_fit_clip')..
                '##swlTimelineFitClip') then
            state.animationTimelineViewStart=0
            state.animationTimelineViewEnd=state.animationTimelineClip.duration
            state.animationTimelinePan=nil
        end
        if selectedCount>0 then
            tImGui.SameLine()
            if tImGui.Button(tLang.L('swl_animation_timeline_fit_selection')..
                    '##swlTimelineFitSelection') then
                local _,minimumTime,maximumTime=
                    collectTimelineSelection(state.animationTimelineClip)
                local duration=math.max(state.animationTimelineClip.duration or 0,0.0001)
                local span=math.max(0,maximumTime-minimumTime)
                local viewDuration
                if span<=1e-6 then
                    viewDuration=math.min(duration,math.max(0.001,duration*0.05))
                else
                    viewDuration=math.min(duration,math.max(0.001,span*1.2))
                end
                local center=(minimumTime+maximumTime)*0.5
                local viewStart=math.max(0,
                    math.min(duration-viewDuration,center-viewDuration*0.5))
                state.animationTimelineViewStart=viewStart
                state.animationTimelineViewEnd=viewStart+viewDuration
                state.animationTimelinePan=nil
            end
        end
        tImGui.SameLine()
        tImGui.TextDisabled(string.format(tLang.L('swl_animation_timeline_visible_range_fmt'),
            state.animationTimelineViewStart or 0,
            state.animationTimelineViewEnd or state.animationTimelineClip.duration or 0))
        if selectedCount>0 then
            if tImGui.Button(tLang.L('swl_animation_timeline_copy_selection')..
                    '##swlTimelineCopy') then
                copyTimelineSelection()
            end
        else
            tImGui.BeginDisabled(true)
            tImGui.Button(tLang.L('swl_animation_timeline_copy_selection')..
                '##swlTimelineCopy')
            tImGui.EndDisabled()
        end
        tImGui.SameLine()
        local clipboard=state.animationKeyClipboard
        tImGui.BeginDisabled(clipboard==nil)
        if tImGui.Button(tLang.L('swl_animation_timeline_paste_at_playhead')..
                '##swlTimelinePaste') then
            pasteTimelineClipboardAtPlayhead()
        end
        tImGui.EndDisabled()
        if clipboard then
            tImGui.SameLine()
            tImGui.TextDisabled(string.format(tLang.L(
                'swl_animation_timeline_clipboard_fmt'),#(clipboard.items or {}),
                clipboard.clipName or '?'))
        end
        if selectedCount>0 then
            tImGui.SameLine()
            if tImGui.Button(tLang.L('swl_animation_timeline_clear_selection')..
                    '##swlTimelineClearSelection') then
                state.animationTimelineSelection={}
                state.animationTimelineTrackIndex=nil
                state.animationTimelineKeyIndex=nil
            end
            local references,minimumTime,maximumTime=
                collectTimelineSelection(state.animationTimelineClip)
            local duplicateDelta=(state.authoringTime or 0)-minimumTime
            local duplicateBlocked=#references==0 or math.abs(duplicateDelta)<=1e-6 or
                maximumTime+duplicateDelta>(state.animationTimelineClip.duration or 0)+1e-6
            tImGui.SameLine()
            tImGui.BeginDisabled(duplicateBlocked)
            if tImGui.Button(tLang.L('swl_animation_timeline_duplicate_at_playhead')..
                    '##swlTimelineDuplicate') then
                local snapshot=stageRollbackSnapshot()
                local duplicated=snapshot and select(1,safeCall(function()
                    return state.meshD:duplicateSkeletalKeys(state.animationClipSelected,
                        references,duplicateDelta)
                end)) or false
                if duplicated then
                    commitRollbackSnapshot(snapshot,'swl_animation_timeline_duplicate_at_playhead')
                    state.modified=true
                    clearAuthoringOverride()
                    setStatus(string.format(tLang.L(
                        'swl_animation_timeline_keys_duplicated_fmt'),selectedCount),false)
                elseif snapshot then discardRollbackSnapshot(snapshot) end
            end
            tImGui.EndDisabled()
            local selectionSpan=maximumTime-minimumTime
            local rippleBlocked=selectionSpan<=1e-6
            tImGui.SameLine()
            tImGui.BeginDisabled(rippleBlocked)
            if tImGui.Button(tLang.L('swl_animation_timeline_insert_ripple')..
                    '##swlTimelineRipple') then
                local snapshot=stageRollbackSnapshot()
                local inserted=snapshot and select(1,safeCall(function()
                    return state.meshD:insertSkeletalKeysRipple(state.animationClipSelected,
                        references,state.authoringTime)
                end)) or false
                if inserted then
                    commitRollbackSnapshot(snapshot,'swl_animation_timeline_insert_ripple')
                    state.modified=true
                    clearAuthoringOverride()
                    setStatus(string.format(tLang.L(
                        'swl_animation_timeline_keys_inserted_fmt'),selectedCount,selectionSpan),false)
                elseif snapshot then discardRollbackSnapshot(snapshot) end
            end
            tImGui.EndDisabled()
        end
        tImGui.PushItemWidth(110)
        local gapChanged,gapDuration=tImGui.DragFloat(
            tLang.L('swl_animation_timeline_empty_duration')..'##swlTimelineEmptyDuration',
            state.animationTimelineEmptyDuration,0.01,0.001,60,'%.3f s')
        tImGui.PopItemWidth()
        if gapChanged then
            state.animationTimelineEmptyDuration=math.max(0.001,math.min(60,gapDuration))
        end
        tImGui.SameLine()
        local gapBlocked=(state.animationTimelineEmptyDuration or 0)<=1e-6
        tImGui.BeginDisabled(gapBlocked)
        if tImGui.Button(tLang.L('swl_animation_timeline_insert_empty')..
                '##swlTimelineInsertEmpty') then
            local snapshot=stageRollbackSnapshot()
            local inserted=snapshot and select(1,safeCall(function()
                return state.meshD:insertSkeletalEmptyTime(state.animationClipSelected,
                    state.authoringTime,state.animationTimelineEmptyDuration)
            end)) or false
            if inserted then
                commitRollbackSnapshot(snapshot,'swl_animation_timeline_insert_empty')
                state.modified=true
                clearAuthoringOverride()
                setStatus(string.format(tLang.L(
                    'swl_animation_timeline_empty_inserted_fmt'),
                    state.animationTimelineEmptyDuration),false)
            elseif snapshot then discardRollbackSnapshot(snapshot) end
        end
        tImGui.EndDisabled()
        local removalPreview=tImGui.Checkbox(
            tLang.L('swl_animation_timeline_preview_removal'),
            state.animationTimelineRemovalPreview)
        if removalPreview~=state.animationTimelineRemovalPreview then
            state.animationTimelineRemovalPreview=removalPreview
            state.animationTimelineRemovalConfirmed=false
        end
        tImGui.SameLine()
        tImGui.PushItemWidth(110)
        local removalChanged,removalDuration=tImGui.DragFloat(
            tLang.L('swl_animation_timeline_removal_duration')..'##swlTimelineRemovalDuration',
            state.animationTimelineRemovalDuration,0.01,0.001,60,'%.3f s')
        tImGui.PopItemWidth()
        if removalChanged then
            state.animationTimelineRemovalDuration=math.max(0.001,math.min(60,removalDuration))
            state.animationTimelineRemovalConfirmed=false
        end
        if state.animationTimelineRemovalPreview then
            local impact=timelineRemovalImpact(state.animationTimelineClip)
            if state.animationTimelineRemovalConfirmed and
                    (math.abs((state.animationTimelineRemovalConfirmedStart or -1)-
                        impact.startTime)>1e-6 or
                     math.abs((state.animationTimelineRemovalConfirmedDuration or -1)-
                        impact.duration)>1e-6) then
                state.animationTimelineRemovalConfirmed=false
            end
            tImGui.TextColored(impact.emptyTracks>0 and {r=1,g=0.4,b=0.2,a=1} or
                    {r=1,g=0.75,b=0.2,a=1},string.format(
                tLang.L('swl_animation_timeline_removal_impact_fmt'),impact.startTime,
                impact.endTime,impact.removedKeys,impact.emptyTracks))
            if impact.emptyTracks>0 then
                tImGui.TextWrapped(tLang.L('swl_animation_timeline_removal_blocked_tracks'))
            end
            local confirmed=tImGui.Checkbox(
                tLang.L('swl_animation_timeline_confirm_removal'),
                state.animationTimelineRemovalConfirmed)
            if confirmed~=state.animationTimelineRemovalConfirmed then
                state.animationTimelineRemovalConfirmed=confirmed
                state.animationTimelineRemovalConfirmedStart=confirmed and impact.startTime or nil
                state.animationTimelineRemovalConfirmedDuration=confirmed and impact.duration or nil
            end
            tImGui.SameLine()
            local removalBlocked=not state.animationTimelineRemovalConfirmed or
                impact.duration<=1e-6 or impact.emptyTracks>0
            tImGui.BeginDisabled(removalBlocked)
            if tImGui.Button(tLang.L('swl_animation_timeline_remove_interval')..
                    '##swlTimelineRemoveInterval') then
                local snapshot=stageRollbackSnapshot()
                local removed=snapshot and select(2,safeCall(function()
                    return state.meshD:removeSkeletalTimeRange(state.animationClipSelected,
                        impact.startTime,impact.duration)
                end)) or nil
                if removed~=nil then
                    commitRollbackSnapshot(snapshot,'swl_animation_timeline_remove_interval')
                    state.modified=true
                    state.animationTimelineRemovalConfirmed=false
                    clearAuthoringOverride()
                    setStatus(string.format(tLang.L(
                        'swl_animation_timeline_interval_removed_fmt'),
                        impact.duration,removed),false)
                elseif snapshot then discardRollbackSnapshot(snapshot) end
            end
            tImGui.EndDisabled()
        end
        local clipDuration=math.max(state.animationTimelineClip.duration or 0,0)
        local visibleStart=math.max(0,state.animationTimelineViewStart or 0)
        local visibleEnd=math.min(clipDuration,state.animationTimelineViewEnd or clipDuration)
        local visibleDuration=math.max(0,visibleEnd-visibleStart)
        if clipDuration>0 and visibleDuration<clipDuration-1e-6 then
            tImGui.Text(tLang.L('swl_animation_timeline_horizontal_pan'))
            tImGui.PushItemWidth(-1)
            local panChanged,panStart=tImGui.SliderFloat('##swlTimelineHorizontalPan',
                visibleStart,0,math.max(0,clipDuration-visibleDuration),'%.3f s')
            tImGui.PopItemWidth()
            if panChanged then
                state.animationTimelineViewStart=panStart
                state.animationTimelineViewEnd=panStart+visibleDuration
                state.animationTimelinePan=nil
            end
        end
        local snapEnabled=tImGui.Checkbox(tLang.L('swl_animation_timeline_snap'),
            state.animationTimelineSnapEnabled)
        if snapEnabled~=state.animationTimelineSnapEnabled then
            state.animationTimelineSnapEnabled=snapEnabled
        end
        tImGui.SameLine()
        tImGui.PushItemWidth(120)
        local snapChanged,snapStep=tImGui.DragFloat(
            tLang.L('swl_animation_timeline_snap_step')..'##swlTimelineSnapStep',
            state.animationTimelineSnapStep,0.001,0.000001,10,'%.6f s')
        tImGui.PopItemWidth()
        if snapChanged then
            state.animationTimelineSnapStep=math.max(0.000001,math.min(10,snapStep))
        end
        tImGui.TextDisabled(tLang.L('swl_animation_timeline_snap_presets'))
        for _,fps in ipairs({24,25,30,50,60}) do
            tImGui.SameLine()
            if tImGui.Button(string.format('%d FPS##swlTimelineSnap%d',fps,fps)) then
                state.animationTimelineSnapStep=1/fps
                state.animationTimelineSnapEnabled=true
            end
        end
        tImGui.TextDisabled(tLang.L('swl_animation_timeline_navigation_help'))
        tImGui.TextDisabled(tLang.L('swl_animation_timeline_box_help'))
        if tImGui.Button(tLang.L('swl_play_restart')..'##swlTimelinePlay') then
            playback.playing=true; playback.paused=false; state.authoringTime=0
            clearAuthoringOverride()
            refreshAuthoringPose(state.animationTimelineClip)
        end
        tImGui.SameLine()
        tImGui.BeginDisabled(not playback.playing)
        if tImGui.Button(tLang.L(playback.paused and 'swl_resume' or 'swl_pause')..
                '##swlTimelinePause') then playback.paused=not playback.paused end
        tImGui.EndDisabled()
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_stop')..'##swlTimelineStop') then
            playback.playing=false; playback.paused=false; state.authoringTime=0
            clearAuthoringOverride()
            refreshAuthoringPose(state.animationTimelineClip)
        end
        tImGui.SameLine()
        tImGui.PushItemWidth(110)
        local speedChanged,speed=tImGui.DragFloat(tLang.L('swl_animation_playback_speed')..
            '##swlTimelineSpeed',playback.speed,0.05,0.05,4,'%.2fx')
        tImGui.PopItemWidth()
        if speedChanged then playback.speed=math.max(0.05,math.min(4,speed)) end
        showSkeletalTimeline(state.animationTimelineClip)
    end
    tImGui.End()
end

local function updateAuthoringPlayback(delta)
    local playback=state.animationPlayback
    local clip=state.animationTimelineClip
    if state.workspace~='animation' or not clip or not playback.playing or playback.paused then return end
    local duration=math.max(clip.duration or 0,0)
    if duration<=0 then playback.playing=false; playback.paused=false; return end
    local time=(state.authoringTime or 0)+math.max(0,delta or 0)*playback.speed
    if time>=duration then
        if clip.loop then time=time%duration
        else time=duration; playback.playing=false; playback.paused=false end
    end
    state.authoringTime=time
    invalidateAuthoringPose()
    refreshAuthoringPose(clip)
end

local function showSkeletalAnimationInspection()
    tImGui.TextWrapped(tLang.L('swl_animation_inspection_help'))
    local clips=state.animationReport
    if type(clips)~='table' then
        local ok,report=safeCall(function() return state.meshD:getSkeletalAnimationReport() end)
        clips=ok and type(report)=='table' and report or {}
        state.animationReport=clips
    end
    if tImGui.Button(tLang.L('swl_animation_import_msh')..'##swlAnimationImportMsh') then
        state.animationImport.open=true
    end
    showItemTooltip(tLang.L('swl_animation_import_msh_help'))
    if #clips==0 then
        state.animationTimelineClip=nil
        tImGui.TextDisabled(tLang.L('swl_animation_no_clips'))
        state.animationEditClipId=nil
        if state.authoringPose then
            state.authoringPose=nil
            state.authoringPoseKey=nil
            pcall(function() state.preview:stopSkeletalAnimation() end)
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
        end
    else
        local names={}
        for index,clip in ipairs(clips) do names[index]=clip.name or ('Clip '..index) end
        state.animationClipSelected=math.max(1,math.min(state.animationClipSelected,#clips))
        tImGui.PushItemWidth(190)
        local changed,selected=tImGui.Combo(tLang.L('swl_skeletal_clip'),
            state.animationClipSelected,names,-1)
        tImGui.PopItemWidth()
        if changed then
            state.animationClipSelected=selected
            state.animationEditClipId=nil
            state.authoringTime=0
            state.animationTimelineTrackIndex=nil
            state.animationTimelineKeyIndex=nil
            state.animationTimelineDrag=nil
            state.animationTimelineSelection={}
            state.animationTimelineBox=nil
            state.animationPlayback.playing=false
            state.animationPlayback.paused=false
            clearAuthoringOverride()
        end
        local clip=clips[state.animationClipSelected]
        state.animationTimelineClip=clip
        state.authoringActiveClip=clip
        if state.animationEditClipId~=clip.clipId then
            state.animationEditClipId=clip.clipId
            state.animationClipName=clip.name or ''
            state.animationClipDuration=clip.duration or 0
            state.animationClipLoop=clip.loop==true
            state.animationRemoveConfirmed=false
        end
        tImGui.PushItemWidth(190)
        local timeChanged,time=tImGui.SliderFloat('Tempo da pose##swlAuthoringTime',
            state.authoringTime,0,math.max(clip.duration or 0,0),'%.3f s')
        tImGui.PopItemWidth()
        if timeChanged then
            if state.animationPlayback.playing then state.animationPlayback.paused=true end
            state.authoringTime=time
            clearAuthoringOverride()
        end
        refreshAuthoringPose(clip)
        local selectedPoseBone=getBones()[state.boneIndex]
        tImGui.BeginDisabled(not selectedPoseBone or not state.authoringPose)
        if tImGui.Button(tLang.L('swl_animation_copy_bone_pose')..
                '##swlCopyBonePose') then
            copySelectedBonePose()
        end
        showItemTooltip(tLang.L('swl_animation_copy_bone_pose_tooltip'),true)
        tImGui.EndDisabled()
        tImGui.BeginDisabled(state.animationBonePoseClipboard==nil)
        if tImGui.Button(tLang.L('swl_animation_paste_bone_pose')..
                '##swlPasteBonePose') then
            pasteSelectedBonePoseAtPlayhead()
        end
        showItemTooltip(tLang.L('swl_animation_paste_bone_pose_tooltip'),true)
        tImGui.EndDisabled()
        if state.animationBonePoseClipboard then
            tImGui.TextDisabled(string.format(tLang.L('swl_animation_bone_pose_clipboard_fmt'),
                state.animationBonePoseClipboard.boneName or '?'))
        end
        tImGui.Separator()
        tImGui.BeginDisabled(not state.authoringPose)
        if tImGui.Button(tLang.L('swl_animation_copy_skeleton_pose')..
                '##swlCopySkeletonPose') then
            copySkeletonPose()
        end
        showItemTooltip(tLang.L('swl_animation_copy_skeleton_pose_tooltip'),true)
        tImGui.EndDisabled()
        tImGui.BeginDisabled(state.animationSkeletonPoseClipboard==nil)
        if tImGui.Button(tLang.L('swl_animation_paste_skeleton_pose')..
                '##swlPasteSkeletonPose') then
            pasteSkeletonPoseAtPlayhead()
        end
        showItemTooltip(tLang.L('swl_animation_paste_skeleton_pose_tooltip'),true)
        tImGui.EndDisabled()
        if state.animationSkeletonPoseClipboard then
            tImGui.TextDisabled(string.format(tLang.L(
                'swl_animation_skeleton_pose_clipboard_fmt'),
                state.animationSkeletonPoseClipboard.boneCount,
                state.animationSkeletonPoseClipboard.clipName or '?',
                state.animationSkeletonPoseClipboard.sourceTime or 0))
        end
        local authoringMethods={tLang.L('swl_skinning_auto'),tLang.L('swl_skinning_lbs'),
            tLang.L('swl_skinning_dqs')}
        tImGui.PushItemWidth(190)
        local authoringMethodChanged,authoringMethod=tImGui.Combo(
            tLang.L('swl_animation_preview_skinning_method'),
            state.skeletalPreview.method,authoringMethods,-1)
        tImGui.PopItemWidth()
        if authoringMethodChanged then
            local temporaryPath=getTemporaryMeshPath()
            local okSaved,saved=safeCall(function()
                return state.meshD:save(temporaryPath,false,false)
            end)
            if okSaved and saved then
                state.skeletalPreview.method=authoringMethod
                state.skeletalPreview.poseStress=false
                state.skeletalPreview.gpuCpuCompare=false
                clearAuthoringOverride()
                state.authoringPose=nil
                rebuildPreview(temporaryPath)
                pcall(os.remove,temporaryPath)
                refreshAuthoringPose(clip)
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
            else
                pcall(os.remove,temporaryPath)
            end
        end
        local resolvedAuthoringMethod=nil
        if state.preview then
            local okResolved,resolved=safeCall(function()
                return state.preview:getResolvedSkeletalSkinningMethod()
            end)
            if okResolved then
                resolvedAuthoringMethod=resolved
                tImGui.TextDisabled(string.format(
                    tLang.L('swl_animation_preview_resolved_method_fmt'),
                    tostring(resolved or 'unknown'):upper()))
            end
        end
        local previousTransformTool=state.animationTransformTool
        state.animationTransformTool=tImGui.RadioButton(
            tLang.L('swl_animation_tool_move')..'##swlAnimationMove',
            state.animationTransformTool,1)
        showItemTooltip(tLang.L('swl_animation_viewport_select_help'))
        tImGui.SameLine()
        state.animationTransformTool=tImGui.RadioButton(
            tLang.L('swl_animation_tool_rotate')..'##swlAnimationRotate',
            state.animationTransformTool,2)
        showItemTooltip(tLang.L('swl_animation_viewport_select_help'))
        tImGui.SameLine()
        tImGui.BeginDisabled(resolvedAuthoringMethod~='lbs')
        state.animationTransformTool=tImGui.RadioButton(
            tLang.L('swl_animation_tool_scale')..'##swlAnimationScale',
            state.animationTransformTool,3)
        showItemTooltip(tLang.L(resolvedAuthoringMethod=='lbs' and
            'swl_animation_scale_uniform_only' or 'swl_animation_scale_requires_lbs'),true)
        tImGui.EndDisabled()
        if state.animationTransformTool==3 and resolvedAuthoringMethod~='lbs' then
            state.animationTransformTool=1
        end
        if previousTransformTool~=state.animationTransformTool then
            state.translationGizmo.drag=nil; state.rotationGizmo.drag=nil
            state.scaleGizmo.drag=nil
            rebuildTranslationGizmo(); rebuildRotationGizmo(); rebuildScaleGizmo()
        end
        state.animationAutoKey=tImGui.Checkbox(
            tLang.L('swl_animation_auto_key')..'##swlAnimationAutoKey',state.animationAutoKey)
        showItemTooltip(tLang.L('swl_animation_auto_key_help'))
        if state.authoringOverride then
            local rotationOverride=state.authoringOverride.channelMask==2
            local scaleOverride=state.authoringOverride.channelMask==4
            local commitKey=scaleOverride and 'swl_animation_commit_scale_key' or
                rotationOverride and 'swl_animation_commit_rotation_key' or
                'swl_animation_commit_translation_key'
            tImGui.TextColored({r=1,g=0.75,b=0.15,a=1},
                tLang.L('swl_animation_temporary_pose'))
            tImGui.TextColored({r=1,g=0.75,b=0.15,a=1},string.format(
                tLang.L('swl_animation_temporary_pose_commit_help_fmt'),tLang.L(commitKey)))
            if tImGui.Button(tLang.L(commitKey)..'##swlCommitAuthoringKey') then
                commitAuthoringOverride()
            end
            if tImGui.Button(tLang.L('swl_animation_discard_temporary_pose')..
                    '##swlDiscardTemporaryPose') then
                clearAuthoringOverride()
                refreshAuthoringPose(clip)
            end
        end
        tImGui.TextWrapped(string.format(tLang.L('swl_animation_clip_summary_fmt'),
            clip.duration or 0,#(clip.tracks or {}),clip.loop and tLang.L('swl_yes') or tLang.L('swl_no'),
            clip.clipId or '?'))
        tImGui.PushItemWidth(190)
        local nameChanged,name=tImGui.InputText(tLang.L('swl_animation_clip_name')..'##swlClipName',
            state.animationClipName,tImGui.Flags('ImGuiInputTextFlags_None'))
        if nameChanged then state.animationClipName=name end
        local durationChanged,duration=tImGui.InputFloat(tLang.L('swl_animation_duration')..'##swlClipDuration',
            state.animationClipDuration,0.01,0.1,'%.6g',tImGui.Flags('ImGuiInputTextFlags_None'))
        if durationChanged then state.animationClipDuration=duration end
        tImGui.PopItemWidth()
        state.animationClipLoop=tImGui.Checkbox(tLang.L('swl_animation_loop')..'##swlClipLoop',
            state.animationClipLoop)
        local trimmed=(state.animationClipName or ''):match('^%s*(.-)%s*$')
        tImGui.BeginDisabled(trimmed=='' or (state.animationClipDuration or -1)<0)
        if tImGui.Button(tLang.L('swl_animation_apply_clip')..'##swlClipApply') then
            local snapshot=stageRollbackSnapshot()
            local applied=false
            if snapshot then
                applied=select(1,safeCall(function() return state.meshD:updateSkeletalClip(
                    state.animationClipSelected,trimmed,state.animationClipDuration,state.animationClipLoop) end))
            end
            if applied then
                commitRollbackSnapshot(snapshot,'swl_animation_clip_updated'); state.modified=true; refreshBindReport()
                state.animationEditClipId=nil
                clearAuthoringOverride()
                setStatus(tLang.L('swl_animation_clip_updated'),false)
            elseif snapshot then discardRollbackSnapshot(snapshot) end
        end
        tImGui.EndDisabled()
        state.animationRemoveConfirmed=tImGui.Checkbox(
            tLang.L('swl_animation_confirm_remove_clip')..'##swlClipRemoveConfirm',
            state.animationRemoveConfirmed)
        tImGui.BeginDisabled(not state.animationRemoveConfirmed)
        if tImGui.Button(tLang.L('swl_animation_remove_clip')..'##swlClipRemove') then
            local snapshot=stageRollbackSnapshot()
            local removed=false
            if snapshot then removed=select(1,safeCall(function()
                return state.meshD:removeSkeletalClip(state.animationClipSelected) end)) end
            if removed then
                commitRollbackSnapshot(snapshot,'swl_animation_clip_removed'); state.modified=true; refreshBindReport()
                state.animationClipSelected=math.max(1,state.animationClipSelected-1)
                state.animationEditClipId=nil; state.animationRemoveConfirmed=false
                state.authoringTime=0; clearAuthoringOverride()
                setStatus(tLang.L('swl_animation_clip_removed'),false)
            elseif snapshot then discardRollbackSnapshot(snapshot) end
        end
        tImGui.EndDisabled()
        local existingBoneIds={}
        for _,track in ipairs(clip.tracks or {}) do existingBoneIds[track.boneId]=true end
        local availableBoneNames,availableBoneIndices={},{}
        for index,bone in ipairs((state.bindReport and state.bindReport.bones) or {}) do
            if not existingBoneIds[bone.boneId] then
                availableBoneNames[#availableBoneNames+1]=bone.name or ('Bone '..index)
                availableBoneIndices[#availableBoneIndices+1]=index
            end
        end
        if #availableBoneNames>0 then
            local availableChoice=1
            for choice,boneIndex in ipairs(availableBoneIndices) do
                if boneIndex==state.boneIndex then availableChoice=choice; break end
            end
            local selectedAvailableBone=availableBoneIndices[availableChoice]
            tImGui.PushItemWidth(190)
            local boneChanged,newChoice=tImGui.Combo(tLang.L('swl_source_bone')..'##swlNewTrackBone',
                availableChoice,availableBoneNames,-1)
            tImGui.PopItemWidth()
            if boneChanged then
                availableChoice=newChoice
                selectedAvailableBone=availableBoneIndices[newChoice]
                state.boneIndex=selectedAvailableBone
                clearAuthoringOverride()
            end
            state.animationNewTrackTranslation=tImGui.Checkbox(
                tLang.L('swl_animation_translation')..'##swlNewTrackT',state.animationNewTrackTranslation)
            tImGui.SameLine()
            state.animationNewTrackRotation=tImGui.Checkbox(
                tLang.L('swl_animation_rotation')..'##swlNewTrackR',state.animationNewTrackRotation)
            tImGui.SameLine()
            state.animationNewTrackScale=tImGui.Checkbox(
                tLang.L('swl_animation_scale')..'##swlNewTrackS',state.animationNewTrackScale)
            local newMask=(state.animationNewTrackTranslation and 1 or 0)+
                (state.animationNewTrackRotation and 2 or 0)+(state.animationNewTrackScale and 4 or 0)
            tImGui.TextDisabled(tLang.L('swl_animation_track_bind_key_help'))
            tImGui.BeginDisabled(newMask==0)
            if tImGui.Button(tLang.L('swl_animation_add_track')..'##swlTrackAdd') then
                local snapshot=stageRollbackSnapshot()
                local added=false
                if snapshot then added=select(1,safeCall(function() return state.meshD:addSkeletalTrack(
                    state.animationClipSelected,selectedAvailableBone,newMask) end)) end
                if added then
                    commitRollbackSnapshot(snapshot,'swl_animation_track_added'); state.modified=true; refreshBindReport()
                    state.animationTrackEdits={}
                    state.animationKeyEdits={}; state.animationNewKeyTimes={}
                    clearAuthoringOverride()
                    setStatus(tLang.L('swl_animation_track_added'),false)
                elseif snapshot then discardRollbackSnapshot(snapshot) end
            end
            tImGui.EndDisabled()
        end
        tImGui.BeginChild('##swlCanonicalTracks',{x=0,y=360},true)
        for trackIndex,track in ipairs(clip.tracks or {}) do
            local label=string.format(tLang.L('swl_animation_track_fmt'),track.boneName or '?',
                #(track.keys or {}),skeletalChannelLabel(track.channelMask or 0))
            if tImGui.TreeNode(label..'##swlTrack'..trackIndex) then
                if tImGui.IsItemClicked() and track.boneIndex then
                    state.boneIndex=track.boneIndex
                    clearAuthoringOverride()
                end
                local trackEditKey=(clip.clipId or '?')..':'..(track.boneId or '?')
                local trackEdit=state.animationTrackEdits[trackEditKey]
                if not trackEdit then
                    trackEdit={translation=((track.channelMask or 0)&1)~=0,
                        rotation=((track.channelMask or 0)&2)~=0,
                        scale=((track.channelMask or 0)&4)~=0,removeConfirmed=false}
                    state.animationTrackEdits[trackEditKey]=trackEdit
                end
                tImGui.Text(string.format('%s: %s',tLang.L('swl_stable_id'),track.boneId or '?'))
                tImGui.Text(string.format(tLang.L('swl_animation_channels_fmt'),
                    skeletalChannelLabel(track.channelMask or 0)))
                trackEdit.translation=tImGui.Checkbox(
                    tLang.L('swl_animation_translation')..'##swlTrackT'..trackIndex,
                    trackEdit.translation)
                tImGui.SameLine()
                trackEdit.rotation=tImGui.Checkbox(
                    tLang.L('swl_animation_rotation')..'##swlTrackR'..trackIndex,
                    trackEdit.rotation)
                tImGui.SameLine()
                trackEdit.scale=tImGui.Checkbox(
                    tLang.L('swl_animation_scale')..'##swlTrackS'..trackIndex,
                    trackEdit.scale)
                local editedMask=(trackEdit.translation and 1 or 0)+
                    (trackEdit.rotation and 2 or 0)+(trackEdit.scale and 4 or 0)
                tImGui.BeginDisabled(editedMask==0 or editedMask==(track.channelMask or 0))
                if tImGui.Button(tLang.L('swl_animation_apply_track_channels')..
                        '##swlTrackApply'..trackIndex) then
                    local snapshot=stageRollbackSnapshot()
                    local updated=false
                    if snapshot then updated=select(1,safeCall(function()
                        return state.meshD:updateSkeletalTrackChannels(state.animationClipSelected,
                            trackIndex,editedMask) end)) end
                    if updated then
                        commitRollbackSnapshot(snapshot,'swl_animation_track_updated'); state.modified=true; refreshBindReport()
                        state.animationTrackEdits={}
                        state.animationKeyEdits={}
                        clearAuthoringOverride()
                        setStatus(tLang.L('swl_animation_track_updated'),false)
                    elseif snapshot then discardRollbackSnapshot(snapshot) end
                end
                tImGui.EndDisabled()
                trackEdit.removeConfirmed=tImGui.Checkbox(
                    tLang.L('swl_animation_confirm_remove_track')..'##swlTrackRemoveConfirm'..trackIndex,
                    trackEdit.removeConfirmed)
                tImGui.BeginDisabled(not trackEdit.removeConfirmed)
                if tImGui.Button(tLang.L('swl_animation_remove_track')..'##swlTrackRemove'..trackIndex) then
                    local snapshot=stageRollbackSnapshot()
                    local removed=false
                    if snapshot then removed=select(1,safeCall(function()
                        return state.meshD:removeSkeletalTrack(state.animationClipSelected,trackIndex) end)) end
                    if removed then
                        commitRollbackSnapshot(snapshot,'swl_animation_track_removed'); state.modified=true; refreshBindReport()
                        state.animationTrackEdits={}
                        state.animationKeyEdits={}; state.animationNewKeyTimes={}
                        clearAuthoringOverride()
                        setStatus(tLang.L('swl_animation_track_removed'),false)
                    elseif snapshot then discardRollbackSnapshot(snapshot) end
                end
                tImGui.EndDisabled()
                local newKeyId=(clip.clipId or '?')..':'..(track.boneId or '?')
                local newKeyTime=state.animationNewKeyTimes[newKeyId]
                if newKeyTime==nil then newKeyTime=math.min(clip.duration or 0,
                    ((track.keys or {})[#(track.keys or {})] and
                     (track.keys or {})[#(track.keys or {})].time or 0)+0.1) end
                newKeyTime=skeletalKeyFloat(tLang.L('swl_animation_new_key_time'),
                    'swlNewKeyTime'..trackIndex,newKeyTime)
                state.animationNewKeyTimes[newKeyId]=newKeyTime
                tImGui.TextDisabled(tLang.L('swl_animation_key_sample_help'))
                tImGui.BeginDisabled(newKeyTime<0 or newKeyTime>(clip.duration or 0))
                if tImGui.Button(tLang.L('swl_animation_add_key')..'##swlKeyAdd'..trackIndex) then
                    local snapshot=stageRollbackSnapshot()
                    local added=false
                    if snapshot then added=select(1,safeCall(function()
                        return state.meshD:addSkeletalKey(state.animationClipSelected,trackIndex,newKeyTime) end)) end
                    if added then
                        commitRollbackSnapshot(snapshot,'swl_animation_key_added'); state.modified=true
                        state.animationKeyEdits={}; state.animationNewKeyTimes={}
                        clearAuthoringOverride()
                        setStatus(tLang.L('swl_animation_key_added'),false)
                    elseif snapshot then discardRollbackSnapshot(snapshot) end
                end
                tImGui.EndDisabled()
                for keyIndex,key in ipairs(track.keys or {}) do
                    local easing=skeletalEasingNames[(key.easing or 0)+1] or '?'
                    if tImGui.TreeNode(string.format(tLang.L('swl_animation_key_fmt'),
                            keyIndex,key.time or 0,easing)..'##swlKey'..trackIndex..'-'..keyIndex) then
                        local p,q,s=key.translation or {},key.rotation or {},key.scale or {}
                        local keyEditId=newKeyId..':'..keyIndex..':'..string.format('%.9g',key.time or 0)
                        local edit=state.animationKeyEdits[keyEditId]
                        if not edit then
                            edit={time=key.time or 0,tx=p.x or 0,ty=p.y or 0,tz=p.z or 0,
                                qx=q.x or 0,qy=q.y or 0,qz=q.z or 0,qw=q.w or 1,
                                sx=s.x or 1,sy=s.y or 1,sz=s.z or 1,easing=(key.easing or 0)+1,
                                x1=key.bezier and key.bezier.x1 or 0,
                                y1=key.bezier and key.bezier.y1 or 0,
                                x2=key.bezier and key.bezier.x2 or 1,
                                y2=key.bezier and key.bezier.y2 or 1,removeConfirmed=false}
                            state.animationKeyEdits[keyEditId]=edit
                        end
                        local fieldId='swlKey'..trackIndex..'-'..keyIndex
                        edit.time=skeletalKeyFloat(tLang.L('swl_animation_new_key_time'),fieldId..'Time',edit.time)
                        for _,field in ipairs({{'T X','tx'},{'T Y','ty'},{'T Z','tz'},
                                {'Q X','qx'},{'Q Y','qy'},{'Q Z','qz'},{'Q W','qw'},
                                {'S X','sx'},{'S Y','sy'},{'S Z','sz'}}) do
                            edit[field[2]]=skeletalKeyFloat(field[1],fieldId..field[2],edit[field[2]])
                        end
                        tImGui.PushItemWidth(190)
                        local easingChanged,easingChoice=tImGui.Combo(
                            tLang.L('swl_animation_easing')..'##'..fieldId..'Easing',
                            edit.easing,skeletalEasingNames,-1)
                        tImGui.PopItemWidth()
                        if easingChanged then edit.easing=easingChoice end
                        if edit.easing==6 then
                            for _,field in ipairs({{'Bezier X1','x1'},{'Bezier Y1','y1'},
                                    {'Bezier X2','x2'},{'Bezier Y2','y2'}}) do
                                edit[field[2]]=skeletalKeyFloat(field[1],fieldId..field[2],edit[field[2]])
                            end
                        end
                        tImGui.BeginDisabled(edit.time<0 or edit.time>(clip.duration or 0))
                        if tImGui.Button(tLang.L('swl_animation_apply_key')..'##'..fieldId..'Apply') then
                            local snapshot=stageRollbackSnapshot()
                            local updated=false
                            if snapshot then updated=select(1,safeCall(function()
                                return state.meshD:updateSkeletalKey(state.animationClipSelected,trackIndex,keyIndex,
                                    edit.time,edit.tx,edit.ty,edit.tz,edit.qx,edit.qy,edit.qz,edit.qw,
                                    edit.sx,edit.sy,edit.sz,edit.easing-1,edit.x1,edit.y1,edit.x2,edit.y2) end)) end
                            if updated then
                                commitRollbackSnapshot(snapshot,'swl_animation_key_updated'); state.modified=true
                                state.animationKeyEdits={}
                                clearAuthoringOverride()
                                setStatus(tLang.L('swl_animation_key_updated'),false)
                            elseif snapshot then discardRollbackSnapshot(snapshot) end
                        end
                        tImGui.EndDisabled()
                        if #(track.keys or {})<=1 then
                            tImGui.TextDisabled(tLang.L('swl_animation_last_key_required'))
                        else
                            edit.removeConfirmed=tImGui.Checkbox(
                                tLang.L('swl_animation_confirm_remove_key')..'##'..fieldId..'Confirm',
                                edit.removeConfirmed)
                            tImGui.BeginDisabled(not edit.removeConfirmed)
                            if tImGui.Button(tLang.L('swl_animation_remove_key')..'##'..fieldId..'Remove') then
                                local snapshot=stageRollbackSnapshot()
                                local removed=false
                                if snapshot then removed=select(1,safeCall(function()
                                    return state.meshD:removeSkeletalKey(state.animationClipSelected,
                                        trackIndex,keyIndex) end)) end
                                if removed then
                                    commitRollbackSnapshot(snapshot,'swl_animation_key_removed'); state.modified=true
                                    state.animationKeyEdits={}; state.animationNewKeyTimes={}
                                    clearAuthoringOverride()
                                    setStatus(tLang.L('swl_animation_key_removed'),false)
                                elseif snapshot then discardRollbackSnapshot(snapshot) end
                            end
                            tImGui.EndDisabled()
                        end
                        tImGui.TreePop()
                    end
                end
                tImGui.TreePop()
            end
        end
        tImGui.EndChild()
    end
    if tTutorials.consumeFocus('animation_clip') then tImGui.SetScrollHereY(0.8) end
    tImGui.Separator()
    tImGui.PushItemWidth(190)
    local newNameChanged,newName=tImGui.InputText(tLang.L('swl_animation_clip_name')..'##swlNewClipName',
        state.animationNewClipName,tImGui.Flags('ImGuiInputTextFlags_None'))
    if newNameChanged then state.animationNewClipName=newName end
    local newDurationChanged,newDuration=tImGui.InputFloat(
        tLang.L('swl_animation_duration')..'##swlNewClipDuration',state.animationNewClipDuration,
        0.01,0.1,'%.6g',tImGui.Flags('ImGuiInputTextFlags_None'))
    if newDurationChanged then state.animationNewClipDuration=newDuration end
    tImGui.PopItemWidth()
    state.animationNewClipLoop=tImGui.Checkbox(tLang.L('swl_animation_loop')..'##swlNewClipLoop',
        state.animationNewClipLoop)
    local newTrimmed=(state.animationNewClipName or ''):match('^%s*(.-)%s*$')
    tImGui.BeginDisabled(newTrimmed=='' or (state.animationNewClipDuration or -1)<0)
    if tImGui.Button(tLang.L('swl_animation_add_clip')..'##swlClipAdd') then
        local snapshot=stageRollbackSnapshot()
        local added,newIndex=false,nil
        if snapshot then added,newIndex=safeCall(function() return state.meshD:addSkeletalClip(
            newTrimmed,state.animationNewClipDuration,state.animationNewClipLoop) end) end
        if added then
            commitRollbackSnapshot(snapshot,'swl_animation_clip_added'); state.modified=true; refreshBindReport()
            state.animationClipSelected=newIndex or (#clips+1); state.animationEditClipId=nil
            state.authoringTime=0; clearAuthoringOverride()
            setStatus(tLang.L('swl_animation_clip_added'),false)
        elseif snapshot then discardRollbackSnapshot(snapshot) end
    end
    tImGui.EndDisabled()
    showRollbackControls('swlAnimationRevert')
end

-- Deliberately global: the main editor chunk is at Lua 5.4's 200-local compilation ceiling.
function swlShowAnimationImportWindow()
    local import=state.animationImport
    if not import.open or not state.meshD then return end
    tImGui.SetNextWindowSize({x=610,y=430},tImGui.Flags('ImGuiCond_Once'))
    local flags=tImGui.Flags('ImGuiWindowFlags_NoCollapse')
    local opened=tImGui.Begin(tLang.L('swl_animation_import_window')..'##swlAnimationImportWindow',
        false,flags)
    if opened then
        tImGui.TextWrapped(tLang.L('swl_animation_import_help'))
        if tImGui.Button(tLang.L('swl_animation_import_choose')..'##swlAnimationImportChoose') then
            local path=mbm.openFile(import.path~='' and import.path or state.fileName or '','msh')
            if path then
                destroyObject(import.sourceMesh)
                import.sourceMesh=nil; import.path=path; import.sourceClips={}; import.analysis=nil
                import.error=nil; import.sourceClip=1; import.keyCount=0; import.confirmed=false
                local source=meshDebug:new()
                local loaded,loadResult=safeCall(function() return source:load(path) end)
                if loaded and loadResult then
                    import.sourceMesh=source
                    local bindOk,sourceBind=safeCall(function()
                        return source:getSkeletonBindReport(false)
                    end)
                    local clipsOk,sourceClips=safeCall(function()
                        return source:getSkeletalAnimationReport()
                    end)
                    if bindOk and clipsOk and type(sourceClips)=='table' and #sourceClips>0 then
                        import.sourceBind=sourceBind; import.sourceClips=sourceClips
                        import.analysis,import.error=tAnimationImport.analyze(state.bindReport,sourceBind)
                        local clip=sourceClips[1]
                        import.newName=tAnimationImport.defaultClipName(path,clip.name)
                        for _,track in ipairs(clip.tracks or {}) do
                            import.keyCount=import.keyCount+#(track.keys or {})
                        end
                    else
                        import.error='invalid_source_animation'
                    end
                else
                    destroyObject(source)
                    import.error='load_failed'
                end
            end
        end
        tImGui.SameLine()
        tImGui.TextDisabled(import.path~='' and shortName(import.path) or
            tLang.L('swl_animation_import_no_source'))
        if import.analysis then
            local sourcePrefix=import.analysis.sourcePrefix~='' and import.analysis.sourcePrefix or
                tLang.L('swl_animation_import_no_prefix')
            local targetPrefix=import.analysis.targetPrefix~='' and import.analysis.targetPrefix or
                tLang.L('swl_animation_import_no_prefix')
            tImGui.TextWrapped(string.format(tLang.L('swl_animation_import_mapping_fmt'),
                import.analysis.boneCount,sourcePrefix,targetPrefix,import.analysis.heightRatio or 1))
            local names={}
            for index,clip in ipairs(import.sourceClips) do names[index]=clip.name or ('Clip '..index) end
            tImGui.PushItemWidth(330)
            local clipChanged,sourceClip=tImGui.Combo(tLang.L('swl_animation_import_source_clip'),
                import.sourceClip,names,-1)
            tImGui.PopItemWidth()
            if clipChanged then
                import.sourceClip=sourceClip; import.confirmed=false; import.keyCount=0
                local clip=import.sourceClips[sourceClip]
                import.newName=tAnimationImport.defaultClipName(import.path,clip and clip.name)
                for _,track in ipairs(clip and clip.tracks or {}) do
                    import.keyCount=import.keyCount+#(track.keys or {})
                end
            end
            local clip=import.sourceClips[import.sourceClip]
            if clip then
                tImGui.TextWrapped(string.format(tLang.L('swl_animation_import_clip_fmt'),
                    clip.duration or 0,#(clip.tracks or {}),import.keyCount))
            end
            tImGui.PushItemWidth(330)
            local nameChanged,newName=tImGui.InputText(
                tLang.L('swl_animation_import_name')..'##swlAnimationImportName',import.newName,
                tImGui.Flags('ImGuiInputTextFlags_None'))
            tImGui.PopItemWidth()
            if nameChanged then import.newName=newName; import.confirmed=false end
            import.confirmed=tImGui.Checkbox(tLang.L('swl_animation_import_confirm')..
                '##swlAnimationImportConfirm',import.confirmed)
            local trimmed=(import.newName or ''):match('^%s*(.-)%s*$')
            local duplicate=false
            for _,targetClip in ipairs(state.animationReport or {}) do
                if targetClip.name==trimmed then duplicate=true break end
            end
            if duplicate then tImGui.TextColored({r=1,g=0.4,b=0.3,a=1},
                tLang.L('swl_animation_import_duplicate')) end
            tImGui.BeginDisabled(not clip or trimmed=='' or duplicate or not import.confirmed)
            if tImGui.Button(tLang.L('swl_animation_import_apply')..'##swlAnimationImportApply') then
                local payload,payloadError=tAnimationImport.buildPayload(import.analysis,clip)
                if not payload then
                    setStatus(string.format(tLang.L('swl_animation_import_failed_fmt'),
                        tostring(payloadError)),true)
                else
                    local snapshot=stageRollbackSnapshot('swl_animation_import_history')
                    local imported,newIndex=false,nil
                    if snapshot then imported,newIndex=safeCall(function()
                        local index=state.meshD:addSkeletalClip(trimmed,clip.duration,clip.loop==true)
                        state.meshD:pasteSkeletalKeys(index,payload,0,0)
                        return index
                    end) end
                    if imported then
                        commitRollbackSnapshot(snapshot,'swl_animation_import_history')
                        state.modified=true; refreshBindReport()
                        state.animationClipSelected=newIndex; state.animationEditClipId=nil
                        state.animationTimelineClip=nil; state.authoringTime=0
                        clearAuthoringOverride(); import.confirmed=false; import.open=false
                        local previewRefreshed=rebuildRuntimePreviewFromMemory()
                        setStatus(string.format(tLang.L(previewRefreshed and
                            'swl_animation_imported_fmt' or
                            'swl_animation_imported_preview_failed_fmt'),trimmed),
                            not previewRefreshed)
                    elseif snapshot then
                        restoreHistoryEntry(snapshot)
                        discardRollbackSnapshot(snapshot)
                    end
                end
            end
            tImGui.EndDisabled()
        elseif import.error then
            tImGui.TextColored({r=1,g=0.4,b=0.3,a=1},string.format(
                tLang.L('swl_animation_import_incompatible_fmt'),tostring(import.error)))
        end
        tImGui.Separator()
        if tImGui.Button(tLang.L('swl_animation_import_close')..'##swlAnimationImportClose') then
            import.open=false
        end
    end
    tImGui.End()
end

local function nextSimpleBoneName()
    local used={}
    for _,bone in ipairs(getBones()) do used[bone.name]=true end
    local index=1
    while used['Bone_'..index] do index=index+1 end
    return 'Bone_'..index
end

local function generateAutomaticBoneWeights()
    local bones=getBones()
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if #bones==0 or not cache or #cache.vertices==0 then return false end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local minimumRadius=math.max(extent*0.015,1e-5)
    local segments={}
    for _,bone in ipairs(bones) do
        local head,tail=getBoneEditorEndpoints(bone,extent)
        segments[#segments+1]={name=bone.name,head=head,tail=tail,
            radius=math.max(bone.radius or 0,minimumRadius)}
    end
    local function distanceToSegment(point,segment)
        local ax,ay,az=segment.head.x,segment.head.y,segment.head.z
        local bx,by,bz=segment.tail.x,segment.tail.y,segment.tail.z
        local dx,dy,dz=bx-ax,by-ay,bz-az
        local lengthSquared=dx*dx+dy*dy+dz*dz
        local t=lengthSquared>1e-12 and ((point.x-ax)*dx+(point.y-ay)*dy+
            (point.z-az)*dz)/lengthSquared or 0
        t=math.max(0,math.min(1,t))
        local x,y,z=ax+dx*t,ay+dy*t,az+dz*t
        local px,py,pz=point.x-x,point.y-y,point.z-z
        return math.sqrt(px*px+py*py+pz*pz)
    end
    local function trimAndNormalize(map)
        local ranked={}
        for name,value in pairs(map) do
            if value>1e-12 then ranked[#ranked+1]={name=name,value=value} end
        end
        table.sort(ranked,function(a,b)
            return a.value==b.value and a.name<b.name or a.value>b.value
        end)
        local result,total={},0
        for index=1,math.min(4,#ranked) do total=total+ranked[index].value end
        if total<=1e-12 then return result end
        for index=1,math.min(4,#ranked) do
            result[ranked[index].name]=ranked[index].value/total
        end
        return result
    end
    local weights={}
    for index,vertex in ipairs(cache.vertices) do
        local scores={}
        for _,segment in ipairs(segments) do
            local scaled=distanceToSegment(vertex.point,segment)/segment.radius
            scores[segment.name]=1/((0.25+scaled)*(0.25+scaled))
        end
        weights[index]=trimAndNormalize(scores)
    end
    local adjacency=buildTopologyAdjacency()
    for _=1,state.boneEditorAutomaticWeightIterations do
        local nextWeights={}
        for index,current in ipairs(weights) do
            local mixed={}
            for name,value in pairs(current) do mixed[name]=(mixed[name] or 0)+value*0.6 end
            local neighbors,count=adjacency[index],0
            if neighbors then for _ in pairs(neighbors) do count=count+1 end end
            if count>0 then
                local scale=0.4/count
                for neighbor in pairs(neighbors) do
                    for name,value in pairs(weights[neighbor] or {}) do
                        mixed[name]=(mixed[name] or 0)+value*scale
                    end
                end
            else
                for name,value in pairs(current) do mixed[name]=(mixed[name] or 0)+value*0.4 end
            end
            nextWeights[index]=trimAndNormalize(mixed)
        end
        weights=nextWeights
    end
    local synchronizedSeams=0
    local seams=buildCoincidentSeams(adjacency)
    for _,group in ipairs(seams.groups) do
        local average={}
        for _,index in ipairs(group) do
            for name,value in pairs(weights[index] or {}) do
                average[name]=(average[name] or 0)+value/#group
            end
        end
        average=trimAndNormalize(average)
        for _,index in ipairs(group) do
            local copy={}
            for name,value in pairs(average) do copy[name]=value end
            weights[index]=copy
        end
        synchronizedSeams=synchronizedSeams+1
    end
    local edits={}
    for index,map in ipairs(weights) do
        local ranked={}
        for name,value in pairs(map) do ranked[#ranked+1]={name=name,value=value} end
        table.sort(ranked,function(a,b)
            return a.value==b.value and a.name<b.name or a.value>b.value
        end)
        local row={[1]=index}
        for slot=1,4 do
            local influence=ranked[slot]
            row[2+(slot-1)*2]=influence and influence.name or nil
            row[3+(slot-1)*2]=influence and influence.value or 0
        end
        edits[#edits+1]=row
    end
    local snapshot=stageRollbackSnapshot()
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local initialized=select(1,safeCall(function()
        return state.meshD:initializeSkeletalVertexWeights(1)
    end))
    local applied=initialized and select(1,safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end))
    if not applied then
        restoreHistoryEntry(snapshot)
        discardRollbackSnapshot(snapshot)
        setStatus(tLang.L('swl_bone_editor_automatic_weights_failed'),true)
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_generate_automatic_weights')
    state.modified=true
    state.boneEditorAutomaticWeightsConfirmed=false
    refreshBindReport(); rebuildPreview(); buildPaintGeometryCache()
    state.paint.heatmapDirty=true; rebuildSkeletonVisuals(); applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_bone_editor_automatic_weights_applied_fmt'),
        #edits,#bones,state.boneEditorAutomaticWeightIterations,synchronizedSeams),false,true)
    return true
end

applySelectedArmatureTemplate=function(templateOverride)
    local template=templateOverride or tArmatureTemplates.items[state.armatureTemplateSelected]
    local fitted,fitError=tArmatureTemplates.fit(template,state.meshBounds)
    if not fitted then
        setStatus(tLang.L(fitError=='invalid_bounds' and 'swl_armature_template_invalid_bounds' or
            'swl_armature_template_invalid'),true)
        return false
    end
    local snapshot=stageRollbackSnapshot('swl_history_apply_armature_template')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    local callOk,applied,applyResult,adaptedHeight=pcall(tArmatureTemplates.apply,state.meshD,template,
        state.meshBounds)
    if not callOk or not applied then
        restoreHistoryEntry(snapshot)
        discardRollbackSnapshot(snapshot)
        local reason=callOk and applyResult or applied
        setStatus(tostring(reason or tLang.L('swl_armature_template_invalid')),true)
        return false
    end
    commitRollbackSnapshot(snapshot,'swl_history_apply_armature_template')
    state.modified=true
    state.armatureTemplateConfirmed=false
    state.boneIndex=1
    state.boneEditorSelectedIndex=nil
    state.boneEditorSelection=nil
    clearPaintVisuals()
    state.topologyAdjacency=nil
    state.coincidentSeams=nil
    state.paint.geometry=nil
    state.paint.heatmapDirty=true
    refreshBindReport()
    state.allowedBones={}
    for _,bone in ipairs(getBones()) do state.allowedBones[bone.name]=true end
    rebuildPreview()
    buildPaintGeometryCache()
    rebuildSkeletonVisuals()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_armature_template_applied_fmt'),template.label,applyResult,
        adaptedHeight or 0),false)
    return true
end

extractCurrentArmature=function()
    local report=state.meshD and state.meshD:getSkeletonBindReport(false) or nil
    local label=state.fileName and tUtil.getBaseFileName(state.fileName) or 'Extracted Armature'
    local template,extractError=tArmatureTemplates.fromReport(report,label)
    if not template then setStatus(tostring(extractError or
        tLang.L('swl_armature_extract_failed')),true); return false end
    local defaultPath=(state.fileName or 'armature'):gsub('%.[^./\\]+$','')..'.lua'
    local path=mbm.saveFile(defaultPath,'lua')
    if not path then return false end
    local saved,saveError=tArmatureTemplates.saveFile(path,template)
    if not saved then setStatus(tostring(saveError or
        tLang.L('swl_armature_extract_failed')),true); return false end
    setStatus(string.format(tLang.L('swl_armature_extracted_fmt'),#template.bones,path),false)
    return true
end

importArmatureFile=function()
    local path=mbm.openFile(state.fileName or '', 'lua')
    if not path then return false end
    local template,loadError=tArmatureTemplates.loadFile(path)
    if not template then setStatus(tostring(loadError or
        tLang.L('swl_armature_import_failed')),true); return false end
    return applySelectedArmatureTemplate(template)
end

showArmatureTemplate=function()
    tImGui.TextWrapped(tLang.L('swl_armature_template_help'))
    tImGui.PushItemWidth(230)
    local changed,selected=tImGui.Combo(tLang.L('swl_armature_template_select'),
        state.armatureTemplateSelected,tArmatureTemplates.labels,-1)
    tImGui.PopItemWidth()
    if changed then
        state.armatureTemplateSelected=selected
        state.armatureTemplateConfirmed=false
    end
    local template=tArmatureTemplates.items[state.armatureTemplateSelected]
    if template then
        tImGui.TextWrapped(string.format(tLang.L('swl_armature_template_summary_fmt'),
            template.label,#template.bones))
    end
    local bones=getBones()
    if #bones>0 then
        tImGui.TextColored({r=1,g=0.55,b=0.15,a=1},
            string.format(tLang.L('swl_armature_template_replace_fmt'),#bones))
    end
    state.armatureTemplateConfirmed=tImGui.Checkbox(
        tLang.L('swl_armature_template_confirm'),state.armatureTemplateConfirmed)
    tImGui.BeginDisabled(not state.armatureTemplateConfirmed or not template)
    if tImGui.Button(tLang.L('swl_armature_template_apply')) then
        applySelectedArmatureTemplate()
    end
    tImGui.EndDisabled()
    tImGui.Separator()
    tImGui.TextWrapped(tLang.L('swl_armature_file_help'))
    tImGui.BeginDisabled(#bones==0)
    if tImGui.Button(tLang.L('swl_armature_extract')) then extractCurrentArmature() end
    tImGui.EndDisabled()
    tImGui.SameLine()
    tImGui.BeginDisabled(not state.armatureTemplateConfirmed)
    if tImGui.Button(tLang.L('swl_armature_import')) then importArmatureFile() end
    tImGui.EndDisabled()
    tImGui.Separator()
    tImGui.TextWrapped(tLang.L('swl_armature_template_next_steps'))
    if #getBones()>0 and tImGui.Button(tLang.L('swl_armature_template_open_bone_editor')) then
        setWorkspace('bone_editor')
    end
    showRollbackControls('swlArmatureTemplateRevert')
end

local function showBoneEditor()
    if tTutorials.consumeFocus('bone_create') then tImGui.SetScrollHereY(0.15) end
    local previousRemovePreview=state.boneEditorRemovePreviewIndex
    state.boneEditorRemovePreviewIndex=nil
    tImGui.TextWrapped(tLang.L('swl_bone_editor_help'))
    tImGui.Separator()
    if tImGui.TreeNode(tLang.L('swl_bone_editor_reorient_tails_section')..
            '##swlBoneEditorReorientTails') then
        tImGui.TextWrapped(tLang.L('swl_bone_editor_reorient_tails_help'))
        state.boneEditorReorientTailsConfirmed=tImGui.Checkbox(
            tLang.L('swl_bone_editor_reorient_tails_confirm')..
                '##swlBoneEditorReorientTailsConfirm',
            state.boneEditorReorientTailsConfirmed)
        tImGui.BeginDisabled(not state.boneEditorReorientTailsConfirmed or #getBones()==0)
        if tImGui.Button(tLang.L('swl_bone_editor_reorient_tails_apply')..
                '##swlBoneEditorReorientTailsApply') then
            local snapshot=stageRollbackSnapshot()
            local callOk,operationOk,count=false,false,0
            if snapshot then
                callOk,operationOk,count=safeCall(function()
                    return tArmatureTemplates.reorientVisualTails(state.meshD,getBones())
                end)
            end
            if callOk and operationOk then
                commitRollbackSnapshot(snapshot,'swl_bone_editor_reorient_tails_history')
                state.boneEditorReorientTailsConfirmed=false
                state.modified=true
                refreshBindReport()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(string.format(tLang.L('swl_bone_editor_reorient_tails_applied_fmt'),
                    tonumber(count) or 0),false)
            elseif snapshot then discardRollbackSnapshot(snapshot) end
        end
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    tImGui.Separator()
    state.boneEditorPreserveOtherJoints=tImGui.Checkbox(
        tLang.L('swl_bone_editor_preserve_other_joints')..'##swlBonePreserveJoints',
        state.boneEditorPreserveOtherJoints)
    tImGui.TextWrapped(tLang.L('swl_bone_editor_preserve_other_joints_help'))
    tImGui.Text(tLang.L('swl_bone_editor_axis_constraint'))
    for axisIndex,axis in ipairs({'x','y','z'}) do
        if axisIndex>1 then tImGui.SameLine() end
        state.boneEditorSnapAxes[axis]=tImGui.Checkbox(
            'Snap '..axis:upper()..'##swlBoneSnap'..axis,state.boneEditorSnapAxes[axis])
    end
    tImGui.TextWrapped(tLang.L('swl_bone_editor_axis_constraint_help'))
    tImGui.PushItemWidth(90)
    local snapChanged,snapStep=tImGui.InputFloat(tLang.L('swl_bone_editor_snap_step')..
        '##swlBoneSnapStep',state.boneEditorSnapStep,0,0,'%.6g',
        tImGui.Flags('ImGuiInputTextFlags_None'))
    tImGui.PopItemWidth()
    if snapChanged then state.boneEditorSnapStep=math.max(0,snapStep) end
    tImGui.TextWrapped(tLang.L('swl_bone_editor_snap_step_help'))
    tImGui.Text(tLang.L('swl_bone_editor_segment_tool'))
    local previousSegmentTool=state.boneEditorSegmentTool
    state.boneEditorSegmentTool=tImGui.RadioButton(
        tLang.L('swl_bone_editor_segment_move')..'##swlBoneSegmentMove',
        state.boneEditorSegmentTool,1)
    tImGui.SameLine()
    state.boneEditorSegmentTool=tImGui.RadioButton(
        tLang.L('swl_bone_editor_segment_rotate')..'##swlBoneSegmentRotate',
        state.boneEditorSegmentTool,2)
    if previousSegmentTool~=state.boneEditorSegmentTool then applyWorkspaceVisibility() end
    tImGui.TextWrapped(tLang.L('swl_bone_editor_segment_tool_help'))
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_bone_editor_new_head_position'))
    tImGui.PushItemWidth(190)
    for _,field in ipairs({{'X','x'},{'Y','y'},{'Z','z'}}) do
        local changed,value=tImGui.InputFloat(field[1]..'##swlBoneEditor'..field[2],
            state.boneEditorPosition[field[2]],0,0,'%.6g',
            tImGui.Flags('ImGuiInputTextFlags_None'))
        if changed then state.boneEditorPosition[field[2]]=value end
    end
    local lengthChanged,length=tImGui.InputFloat(tLang.L('swl_bone_editor_length')..
        '##swlBoneEditorLength',state.boneEditorLength,0,0,'%.6g',
        tImGui.Flags('ImGuiInputTextFlags_None'))
    if lengthChanged then state.boneEditorLength=length end
    tImGui.PopItemWidth()
    tImGui.TextWrapped(tLang.L('swl_bone_editor_new_head_position_help'))
    local function addRootItem(hasExplicitTail)
        local snapshot=stageRollbackSnapshot()
        local ok,newIndex=false,nil
        local position=state.boneEditorPosition
        local name=nextSimpleBoneName()
        local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
            state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
        local radius=math.max(extent*0.012,0.01)
        local length=hasExplicitTail and state.boneEditorLength or 0
        if snapshot then
            if #getBones()==0 then
                ok=select(1,safeCall(function()
                    return state.meshD:initializeSkeletalSkeleton(name,position.x,position.y,position.z,
                        radius,length,hasExplicitTail)
                end))
                newIndex=ok and 1 or nil
            else
                ok,newIndex=safeCall(function()
                    return state.meshD:addSkeletalBone(0,name,position.x,position.y,position.z,
                        radius,length,hasExplicitTail,false)
                end)
            end
        end
        if ok then
            commitRollbackSnapshot(snapshot,hasExplicitTail and 'swl_bone_editor_added' or
                'swl_bone_editor_joint_added')
            state.modified=true
            refreshBindReport()
            state.boneEditorSelectedIndex=nil
            state.boneEditorSelection=nil
            state.boneIndex=newIndex or 1
            rebuildPreview()
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
            setStatus(tLang.L(hasExplicitTail and 'swl_bone_editor_added' or
                'swl_bone_editor_joint_added'),false)
        elseif snapshot then discardRollbackSnapshot(snapshot) end
    end
    if tImGui.Button(tLang.L('swl_bone_editor_add_joint')..'##swlBoneEditorAddJoint') then
        addRootItem(false)
    end
    tImGui.SameLine()
    tImGui.BeginDisabled(not state.boneEditorLength or state.boneEditorLength<=0)
    if tImGui.Button(tLang.L('swl_bone_editor_add')..'##swlBoneEditorAdd') then
        addRootItem(true)
    end
    tImGui.EndDisabled()
    local selectedTail=state.boneEditorSelection and
        (state.boneEditorSelection.kind=='tail' or state.boneEditorSelection.kind=='joint') and
        getBones()[state.boneEditorSelection.boneIndex] or nil
    local inheritedExtensionLength=selectedTail and selectedTail.parentName and
        selectedTail.length and selectedTail.length>0 and selectedTail.length or nil
    local extensionLength=inheritedExtensionLength or state.boneEditorLength
    tImGui.BeginDisabled(not selectedTail or not extensionLength or extensionLength<=0)
    tImGui.PushItemWidth(65)
    local countChanged,extendCount=tImGui.InputInt('##swlBoneEditorExtendCount',
        state.boneEditorExtendCount,1,1,tImGui.Flags('ImGuiInputTextFlags_None'))
    if countChanged then state.boneEditorExtendCount=math.max(1,math.min(256,extendCount)) end
    tImGui.PopItemWidth()
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_bone_editor_extend')..'##swlBoneEditorExtend') then
        local snapshot=stageRollbackSnapshot()
        local ok,newIndex=false,nil
        if snapshot then
            local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
                state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
            local radius=math.max(selectedTail.radius or 0,extent*0.012,0.01)
            ok,newIndex=safeCall(function()
                return state.meshD:extendSkeletalBoneTail(state.boneEditorSelection.boneIndex,
                    state.boneEditorExtendCount,radius,extensionLength)
            end)
        end
        if ok then
            commitRollbackSnapshot(snapshot,'swl_bone_editor_extended')
            state.modified=true
            refreshBindReport()
            state.boneEditorSelectedIndex=newIndex
            state.boneIndex=newIndex
            local bone=getBones()[newIndex]
            state.boneEditorSelection=bone and {kind='segment',boneIndex=newIndex,
                boneId=bone.boneId,boneName=bone.name} or nil
            rebuildPreview()
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
            setStatus(tLang.L('swl_bone_editor_extended'),false)
        elseif snapshot then discardRollbackSnapshot(snapshot) end
    end
    tImGui.EndDisabled()
    if selectedTail then
        showWrappedDisabledText(string.format(tLang.L(inheritedExtensionLength and
            'swl_bone_editor_inherited_length' or 'swl_bone_editor_configured_length'),
            extensionLength or 0))
    end
    showWrappedDisabledText(tLang.L('swl_bone_editor_root_note'))
    if state.boneEditorSelection then
        local selectionKey=state.boneEditorSelection.kind=='segment' and
            'swl_bone_editor_selected_segment' or state.boneEditorSelection.kind=='joint' and
            'swl_bone_editor_selected_joint' or state.boneEditorSelection.kind=='tail' and
            'swl_bone_editor_selected_tail' or 'swl_bone_editor_selected_head'
        tImGui.TextWrapped(string.format(tLang.L(selectionKey),state.boneEditorSelection.boneName))
        local selectedBone=getBones()[state.boneEditorSelection.boneIndex]
        local headPoint,tailPoint=nil,nil
        if selectedBone then
            headPoint,tailPoint=getBoneEditorEndpoints(selectedBone,1)
            tImGui.Separator()
            tImGui.Text(tLang.L('swl_bone_editor_joint_positions'))
            tImGui.Text(string.format(tLang.L('swl_bone_editor_head_position_fmt'),
                headPoint.x,headPoint.y,headPoint.z))
            if selectedBone.hasExplicitTail then
                tImGui.Text(string.format(tLang.L('swl_bone_editor_tail_position_fmt'),
                    tailPoint.x,tailPoint.y,tailPoint.z))
            end
        end
        local tail=selectedBone and selectedBone.hasExplicitTail and selectedBone.tailOffset or nil
        if tail then
            local tx,ty,tz=tail.x or 0,tail.y or 0,tail.z or 0
            local length=math.sqrt(tx*tx+ty*ty+tz*tz)
            if length>1e-6 then
                local nx,ny,nz=tx/length,ty/length,tz/length
                local inclination=math.deg(math.acos(math.max(-1,math.min(1,ny))))
                local azimuth=math.deg(math.atan(nx,nz))
                tImGui.Separator()
                tImGui.Text(tLang.L('swl_bone_editor_segment_orientation'))
                tImGui.Text(string.format(tLang.L('swl_bone_editor_segment_direction_fmt'),
                    nx,ny,nz))
                tImGui.Text(string.format(tLang.L('swl_bone_editor_segment_angles_fmt'),
                    inclination,azimuth))
                local worldLength=math.sqrt((tailPoint.x-headPoint.x)^2+
                    (tailPoint.y-headPoint.y)^2+(tailPoint.z-headPoint.z)^2)
                tImGui.Text(string.format(tLang.L('swl_bone_editor_segment_length_fmt'),worldLength))
                tImGui.TextWrapped(tLang.L('swl_bone_editor_segment_roll_note'))
            end
        end
        if selectedBone and selectedBone.parentName then
            local parentBone=findBone(getBones(),selectedBone.parentName)
            tImGui.Separator()
            tImGui.Text(string.format(tLang.L('swl_bone_editor_current_parent_fmt'),
                selectedBone.parentName))
            local wantConnected=not selectedBone.connectedToParent
            tImGui.BeginDisabled(wantConnected and (not parentBone or not parentBone.hasExplicitTail))
            if tImGui.Button(tLang.L(wantConnected and 'swl_bone_editor_connect_parent' or
                    'swl_bone_editor_disconnect_parent')..'##swlBoneEditorConnection') then
                local snapshot=stageRollbackSnapshot()
                local ok=snapshot and select(1,safeCall(function()
                    return state.meshD:setSkeletalBoneConnectedToParent(
                        state.boneEditorSelection.boneIndex,wantConnected,
                        state.boneEditorPreserveOtherJoints)
                end)) or false
                if ok then
                    commitRollbackSnapshot(snapshot,wantConnected and 'swl_bone_editor_connected' or
                        'swl_bone_editor_disconnected')
                    state.modified=true
                    refreshBindReport()
                    rebuildPreview()
                    rebuildSkeletonVisuals()
                    applyWorkspaceVisibility()
                    setStatus(tLang.L(wantConnected and 'swl_bone_editor_connected' or
                        'swl_bone_editor_disconnected'),false)
                elseif snapshot then discardRollbackSnapshot(snapshot) end
            end
            tImGui.EndDisabled()
            if wantConnected and (not parentBone or not parentBone.hasExplicitTail) then
                tImGui.TextWrapped(tLang.L('swl_bone_editor_parent_without_tail'))
            end
        end
        if selectedBone then
            if state.boneEditorRadiusBoneId~=selectedBone.boneId then
                state.boneEditorRadiusBoneId=selectedBone.boneId
                state.boneEditorRadius=selectedBone.radius or 0
                state.boneEditorRadiusSubtree=false
            end
            if tImGui.TreeNode(tLang.L('swl_bone_editor_joint_radius')..
                    '##swlBoneEditorRadius') then
                tImGui.PushItemWidth(110)
                local extent=state.meshBounds and math.max(
                    state.meshBounds.maxX-state.meshBounds.minX,
                    state.meshBounds.maxY-state.meshBounds.minY,
                    state.meshBounds.maxZ-state.meshBounds.minZ) or 1
                local minimumRadius=math.max(extent*0.0001,0.000001)
                state.boneEditorRadius=math.max(minimumRadius,state.boneEditorRadius or 0)
                local changed,radius=tImGui.DragFloat(tLang.L('swl_bone_editor_radius')..
                    '##swlBoneEditorRadiusValue',state.boneEditorRadius,
                    math.max(extent*0.001,0.0001),minimumRadius,2000000,'%.6g')
                tImGui.PopItemWidth()
                if changed then state.boneEditorRadius=math.max(minimumRadius,radius) end
                state.boneEditorRadiusSubtree=tImGui.Checkbox(
                    tLang.L('swl_bone_editor_radius_subtree')..'##swlBoneEditorRadiusSubtree',
                    state.boneEditorRadiusSubtree)
                tImGui.TextWrapped(tLang.L('swl_bone_editor_radius_help'))
                if tImGui.Button(tLang.L('swl_bone_editor_apply_radius')..
                        '##swlBoneEditorRadiusApply') then
                    local snapshot=stageRollbackSnapshot()
                    local ok=snapshot and select(1,safeCall(function()
                        return state.meshD:setSkeletalBoneRadius(
                            state.boneEditorSelection.boneIndex,state.boneEditorRadius,
                            state.boneEditorRadiusSubtree)
                    end)) or false
                    if ok then
                        commitRollbackSnapshot(snapshot,'swl_bone_editor_radius_applied')
                        state.modified=true
                        refreshBindReport()
                        rebuildSkeletonVisuals()
                        applyWorkspaceVisibility()
                        setStatus(tLang.L('swl_bone_editor_radius_applied'),false)
                    elseif snapshot then discardRollbackSnapshot(snapshot) end
                end
                tImGui.TreePop()
            end
        end
        if selectedBone then
            if state.boneEditorRemoveBoneId~=selectedBone.boneId then
                state.boneEditorRemoveBoneId=selectedBone.boneId
                state.boneEditorRemoveReplacement=selectedBone.parentIndex>0 and
                    selectedBone.parentIndex or (state.boneEditorSelection.boneIndex==1 and 2 or 1)
                state.boneEditorRemoveReparentChildren=false
                state.boneEditorRemoveDiscardTracks=false
                state.boneEditorRemoveConfirmed=false
            end
            if tImGui.TreeNode(tLang.L('swl_remove_bone')..'##swlBoneEditorRemove') then
                local bones=getBones()
                local hasReferences=selectedBone.weightPaletteReferenced or
                    selectedBone.animationTrackCount>0
                local hasChildren=selectedBone.childCount>0
                local blocked=#bones<=1
                tImGui.Text(string.format(tLang.L('swl_remove_bone_impact_fmt'),
                    selectedBone.childCount,selectedBone.weightedVertexCount,
                    selectedBone.animationTrackCount))
                tImGui.TextWrapped(blocked and tLang.L('swl_remove_bone_blocked') or
                    ((hasReferences or hasChildren) and tLang.L('swl_remove_bone_remap_policy') or
                        tLang.L('swl_remove_bone_strict_policy')))
                local replacementNames,replacementIndices={},{}
                for index,candidate in ipairs(bones) do
                    if index~=state.boneEditorSelection.boneIndex then
                        replacementNames[#replacementNames+1]=candidate.name
                        replacementIndices[#replacementIndices+1]=index
                    end
                end
                if (hasReferences or hasChildren) and #replacementNames>0 then
                    local choice=1
                    for item,index in ipairs(replacementIndices) do
                        if index==state.boneEditorRemoveReplacement then choice=item break end
                    end
                    tImGui.PushItemWidth(190)
                    local changed,newChoice=tImGui.Combo(tLang.L('swl_remove_remap_target')..
                        '##swlBoneEditorRemoveTarget',choice,replacementNames,-1)
                    tImGui.PopItemWidth()
                    if changed then
                        state.boneEditorRemoveReplacement=replacementIndices[newChoice]
                        state.boneEditorRemoveConfirmed=false
                    end
                    state.boneEditorRemovePreviewIndex=state.boneEditorRemoveReplacement
                    tImGui.TextWrapped(tLang.L('swl_bone_editor_remove_target_highlight'))
                end
                if hasChildren then
                    local reparent=tImGui.Checkbox(tLang.L('swl_reparent_children_preserve_global')..
                        '##swlBoneEditorRemoveChildren',state.boneEditorRemoveReparentChildren)
                    if reparent~=state.boneEditorRemoveReparentChildren then
                        state.boneEditorRemoveReparentChildren=reparent
                        state.boneEditorRemoveConfirmed=false
                    end
                end
                if selectedBone.animationTrackCount>0 then
                    local discard=tImGui.Checkbox(tLang.L('swl_discard_removed_bone_tracks')..
                        '##swlBoneEditorRemoveTracks',state.boneEditorRemoveDiscardTracks)
                    if discard~=state.boneEditorRemoveDiscardTracks then
                        state.boneEditorRemoveDiscardTracks=discard
                        state.boneEditorRemoveConfirmed=false
                    end
                end
                local actionBlocked=blocked or (selectedBone.animationTrackCount>0 and
                    not state.boneEditorRemoveDiscardTracks) or (hasChildren and
                    not state.boneEditorRemoveReparentChildren)
                tImGui.BeginDisabled(actionBlocked)
                local confirmed=tImGui.Checkbox(tLang.L('swl_confirm_remove_bone')..
                    '##swlBoneEditorRemoveConfirm',state.boneEditorRemoveConfirmed)
                if confirmed~=state.boneEditorRemoveConfirmed then
                    state.boneEditorRemoveConfirmed=confirmed
                end
                tImGui.BeginDisabled(not state.boneEditorRemoveConfirmed)
                if tImGui.Button(tLang.L('swl_apply_remove_bone')..'##swlBoneEditorRemoveApply') then
                    local previousParentId=selectedBone.parentBoneId
                    local snapshot=stageRollbackSnapshot()
                    local ok=snapshot and select(1,safeCall(function()
                        if hasReferences or hasChildren then
                            return state.meshD:removeSkeletalBoneRemapped(
                                state.boneEditorSelection.boneIndex,
                                state.boneEditorRemoveReplacement,
                                state.boneEditorRemoveDiscardTracks,
                                state.boneEditorRemoveReparentChildren)
                        end
                        return state.meshD:removeSkeletalBone(state.boneEditorSelection.boneIndex)
                    end)) or false
                    if ok then
                        commitRollbackSnapshot(snapshot,'swl_bone_removed')
                        state.modified=true
                        refreshBindReport()
                        state.boneIndex=1
                        for index,candidate in ipairs(getBones()) do
                            if candidate.boneId==previousParentId then state.boneIndex=index break end
                        end
                        local replacement=getBones()[state.boneIndex]
                        state.boneEditorSelectedIndex=replacement and state.boneIndex or nil
                        state.boneEditorSelection=replacement and {kind=replacement.hasExplicitTail and
                            'segment' or 'head',boneIndex=state.boneIndex,boneId=replacement.boneId,
                            boneName=replacement.name} or nil
                        state.boneEditorRemoveBoneId=nil
                        rebuildPreview()
                        rebuildSkeletonVisuals()
                        applyWorkspaceVisibility()
                        setStatus(tLang.L('swl_bone_removed'),false)
                    elseif snapshot then discardRollbackSnapshot(snapshot) end
                end
                tImGui.EndDisabled()
                tImGui.EndDisabled()
                tImGui.TreePop()
            end
        end
    end
    tImGui.Separator()
    local tutorialWeightsFocus=tTutorials.consumeFocus('bone_weights')
    if tutorialWeightsFocus then
        tImGui.SetNextItemOpen(true,tImGui.Flags('ImGuiCond_Always'))
    end
    if tImGui.TreeNode(tLang.L('swl_bone_editor_weight_asset_actions')..
            '##swlBoneEditorWeightAssetActions') then
        if tutorialWeightsFocus then tImGui.SetScrollHereY(0.65) end
        local bones=getBones()
        local targetIndex=state.boneEditorSelection and state.boneEditorSelection.boneIndex or
            math.max(1,math.min(state.boneIndex,#bones))
        local selected=bones[targetIndex]
        if #bones>0 then
            local names={}
            for _,bone in ipairs(bones) do names[#names+1]=bone.name end
            tImGui.PushItemWidth(210)
            local changed,newIndex=tImGui.Combo(tLang.L('swl_target_bone')..
                '##swlBoneEditorInitializeTarget',targetIndex,names,-1)
            tImGui.PopItemWidth()
            if changed or not state.boneEditorSelection then
                targetIndex=changed and newIndex or targetIndex
                selected=bones[targetIndex]
                state.boneIndex=targetIndex
                state.boneEditorSelectedIndex=targetIndex
                state.boneEditorSelection=selected and {
                    kind=selected.hasExplicitTail and 'segment' or 'head',boneIndex=targetIndex,
                    boneId=selected.boneId,boneName=selected.name} or nil
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
            end
        end
        local selectedBoneId=selected and selected.boneId or nil
        if state.boneEditorInitializeWeightsBoneId~=selectedBoneId then
            state.boneEditorInitializeWeightsBoneId=selectedBoneId
            state.boneEditorInitializeWeightsConfirmed=false
        end
        local weightsOk,hasWeights=safeCall(function()
            return state.meshD:hasSkeletalVertexWeights()
        end)
        hasWeights=weightsOk and hasWeights
        if not hasWeights then
            tImGui.TextWrapped(tLang.L('swl_bone_editor_initialize_weights_help'))
            tImGui.BeginDisabled(#bones==0)
            state.boneEditorInitializeWeightsConfirmed=tImGui.Checkbox(
                tLang.L('swl_confirm_initialize_weights')..'##swlBoneEditorInitializeConfirm',
                state.boneEditorInitializeWeightsConfirmed)
            tImGui.BeginDisabled(not state.boneEditorInitializeWeightsConfirmed)
            if tImGui.Button(tLang.L('swl_bone_editor_initialize_weights')..
                    '##swlBoneEditorInitializeWeights') then
                local snapshot=stageRollbackSnapshot()
                local ok,affected=false,nil
                if snapshot then
                    ok,affected=safeCall(function()
                        return state.meshD:initializeSkeletalVertexWeights(
                            targetIndex)
                    end)
                end
                if ok then
                    commitRollbackSnapshot(snapshot,'swl_history_initialize_weights')
                    state.modified=true
                    state.boneEditorInitializeWeightsConfirmed=false
                    refreshBindReport(); rebuildPreview(); buildPaintGeometryCache()
                    state.paint.heatmapDirty=true; rebuildSkeletonVisuals(); applyWorkspaceVisibility()
                    setStatus(string.format(tLang.L('swl_weights_initialized_fmt'),affected,
                        selected.name),false)
                elseif snapshot then discardRollbackSnapshot(snapshot) end
            end
            tImGui.EndDisabled()
            tImGui.EndDisabled()
            if not selected then tImGui.TextDisabled(tLang.L('swl_bone_editor_select_bone_first')) end
            tImGui.Separator()
            tImGui.TextWrapped(tLang.L('swl_bone_editor_automatic_weights_help'))
            tImGui.PushItemWidth(100)
            local iterationsChanged,iterations=tImGui.InputInt(
                tLang.L('swl_bone_editor_automatic_iterations')..
                '##swlBoneEditorAutomaticIterations',state.boneEditorAutomaticWeightIterations,
                1,1,tImGui.Flags('ImGuiInputTextFlags_None'))
            tImGui.PopItemWidth()
            if iterationsChanged then
                state.boneEditorAutomaticWeightIterations=math.max(0,math.min(12,iterations))
                state.boneEditorAutomaticWeightsConfirmed=false
            end
            state.boneEditorAutomaticWeightsConfirmed=tImGui.Checkbox(
                tLang.L('swl_bone_editor_confirm_automatic_weights')..
                '##swlBoneEditorAutomaticConfirm',state.boneEditorAutomaticWeightsConfirmed)
            tImGui.BeginDisabled(#bones==0 or not state.boneEditorAutomaticWeightsConfirmed)
            if tImGui.Button(tLang.L('swl_bone_editor_generate_automatic_weights')..
                    '##swlBoneEditorGenerateAutomatic') then
                generateAutomaticBoneWeights()
            end
            tImGui.EndDisabled()
        else
            tImGui.TextWrapped(tLang.L('swl_bone_editor_remove_weights_help'))
            state.boneEditorRemoveWeightsConfirmed=tImGui.Checkbox(
                tLang.L('swl_bone_editor_confirm_remove_weights')..
                '##swlBoneEditorRemoveWeightsConfirm',state.boneEditorRemoveWeightsConfirmed)
            tImGui.BeginDisabled(not state.boneEditorRemoveWeightsConfirmed)
            if tImGui.Button(tLang.L('swl_bone_editor_remove_weights')..
                    '##swlBoneEditorRemoveWeights') then
                local snapshot=stageRollbackSnapshot()
                local ok,affected=false,nil
                if snapshot then
                    ok,affected=safeCall(function()
                        return state.meshD:removeSkeletalVertexWeights()
                    end)
                end
                if ok then
                    commitRollbackSnapshot(snapshot,'swl_history_remove_all_weights')
                    state.modified=true
                    state.boneEditorRemoveWeightsConfirmed=false
                    clearPaintVisuals(); state.paint.geometry=nil; state.paint.heatmapDirty=true
                    refreshBindReport(); rebuildPreview(); buildPaintGeometryCache()
                    rebuildSkeletonVisuals(); applyWorkspaceVisibility()
                    setStatus(string.format(tLang.L('swl_bone_editor_weights_removed_fmt'),affected),false)
                elseif snapshot then discardRollbackSnapshot(snapshot) end
            end
            tImGui.EndDisabled()
        end
        tImGui.Separator()
        local clipsOk,clips=safeCall(function() return state.meshD:getSkeletalAnimationReport() end)
        local clipCount=clipsOk and #(clips or {}) or 0
        tImGui.TextWrapped(string.format(tLang.L('swl_bone_editor_remove_all_impact_fmt'),
            #bones,hasWeights and (state.meshBounds and state.meshBounds.total or 0) or 0,clipCount))
        state.boneEditorRemoveAllConfirmed=tImGui.Checkbox(
            tLang.L('swl_bone_editor_confirm_remove_all')..'##swlBoneEditorRemoveAllConfirm',
            state.boneEditorRemoveAllConfirmed)
        tImGui.BeginDisabled(#bones==0 or not state.boneEditorRemoveAllConfirmed)
        if tImGui.Button(tLang.L('swl_bone_editor_remove_all')..'##swlBoneEditorRemoveAll') then
            local snapshot=stageRollbackSnapshot()
            local ok,boneCount,vertexCount,removedClipCount=false,nil,nil,nil
            if snapshot then
                ok,boneCount,vertexCount,removedClipCount=safeCall(function()
                    return state.meshD:removeAllSkeletalData()
                end)
            end
            if ok then
                commitRollbackSnapshot(snapshot,'swl_history_remove_all_skeletal_data')
                state.modified=true
                state.boneEditorRemoveAllConfirmed=false
                state.boneEditorSelectedIndex=nil; state.boneEditorSelection=nil; state.boneIndex=1
                clearPaintVisuals(); state.paint.geometry=nil; state.paint.heatmapDirty=true
                refreshBindReport(); rebuildPreview(); buildPaintGeometryCache()
                rebuildSkeletonVisuals(); applyWorkspaceVisibility()
                setStatus(string.format(tLang.L('swl_bone_editor_all_removed_fmt'),boneCount,
                    vertexCount,removedClipCount),false)
            elseif snapshot then discardRollbackSnapshot(snapshot) end
        end
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    if previousRemovePreview~=state.boneEditorRemovePreviewIndex then
        applyWorkspaceVisibility()
    end
    showRollbackControls('swlBoneEditorRevert')
end

local function showPanel()
    local _, screenH = mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=0,y=22}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x=440,y=math.max(500,screenH-27)}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSizeConstraints({x=330,y=320}, {x=900,y=math.max(320,screenH-27)})
    local opened = tImGui.Begin(tLang.L('swl_title'), false, noMoveFlag)
    local panelPosition=tImGui.GetWindowPos()
    local panelSize=tImGui.GetWindowSize()
    state.leftPanelRight=(panelPosition.x or 0)+(panelSize.x or 440)
    if opened then
        tImGui.Text(tLang.L('swl_workspace'))
        tImGui.Separator()
        if not state.meshD then
            tImGui.TextWrapped(tLang.L('swl_welcome'))
            if tImGui.Button(tLang.L('swl_open_mesh')) then
                local path = mbm.openFile('', 'msh')
                if path then loadMesh(path) end
            end
        else
            tImGui.Text(shortName(state.fileName) .. (state.modified and ' *' or ''))
            local bones = getBones()
            local okW, hasWeights = safeCall(function() return state.meshD:hasSkeletalVertexWeights() end)
            tImGui.Text(string.format(tLang.L('swl_summary_fmt'), state.aabb and state.aabb.total or 0,
                #bones, okW and hasWeights and tLang.L('swl_yes') or tLang.L('swl_no')))
            showStatusMessage()
            if state.workspace~='paint' then
                showSharedVisualization()
                tImGui.Separator()
            end
            tImGui.Text(tLang.L('swl_workspaces'))
            if openWorkspaceNode('armature_template',tLang.L('swl_armature_template_workspace'),
                    '##swlArmatureTemplateWorkspace') then
                showArmatureTemplate()
                tImGui.TreePop()
            end
            if openWorkspaceNode('bone_editor',tLang.L('swl_bone_editor_workspace'),
                    '##swlBoneEditorWorkspace') then
                showBoneEditor()
                tImGui.TreePop()
            end
            if openWorkspaceNode('bind',tLang.L('swl_bind_pose_contract'),'##swlBindPoseContract') then
                showBindPoseDiagnostics()
                tImGui.TreePop()
            end
            if openWorkspaceNode('runtime',tLang.L('swl_runtime_preview'),'##swlRuntimePreview') then
                showSkeletalPreviewControls()
                tImGui.TreePop()
            end
            if openWorkspaceNode('animation',tLang.L('swl_animation_workspace'),
                    '##swlAnimationWorkspace') then
                showSkeletalAnimationInspection()
                tImGui.TreePop()
            end
            if openWorkspaceNode('paint',tLang.L('swl_paint_weights_workspace'),
                    '##swlPaintWeightsWorkspace') then
                showPaintWeights()
                tImGui.TreePop()
            end
        end
        if not state.meshD then showStatusMessage() end
    end
    tImGui.End()
end

local function showCameraPanel()
    local screenW = mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=math.max(0,screenW-315),y=25}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x=310,y=410}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSizeConstraints({x=270,y=260}, {x=600,y=800})
    local opened = tImGui.Begin(tLang.L('camera_panel') .. ' 3D##swlCamera', false,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if opened then
        if tUtil.drawOrbitGizmo(state.cam, {size=120}) then applyCamera() end
        tImGui.Separator()
        local px,py,pz = cameraPosition()
        tImGui.PushItemWidth(105)
        tImGui.Text(tLang.L('cam_position'))
        local xChanged,nx = tImGui.InputFloat('X##swlCamPX',px,0,0,'%.3f',0)
        tImGui.SameLine()
        local yChanged,ny = tImGui.InputFloat('Y##swlCamPY',py,0,0,'%.3f',0)
        local zChanged,nz = tImGui.InputFloat('Z##swlCamPZ',pz,0,0,'%.3f',0)
        if xChanged or yChanged or zChanged then
            nx,ny,nz = xChanged and nx or px, yChanged and ny or py, zChanged and nz or pz
            local dx,dy,dz = nx-state.cam.fx,ny-state.cam.fy,nz-state.cam.fz
            local distance = math.sqrt(dx*dx+dy*dy+dz*dz)
            if distance > 1e-6 then
                state.cam.distance = distance
                state.cam.elevation = math.asin(math.max(-1,math.min(1,dy/distance)))
                state.cam.azimuth = math.atan(dx,dz)
                applyCamera()
            end
        end
        tImGui.Text(tLang.L('cam_focus'))
        local step = math.max(state.cam.distance*0.0025,0.0001)
        local fxChanged,nfx = tImGui.DragFloat('X##swlCamFX',state.cam.fx,step,0,0,'%.3f')
        tImGui.SameLine()
        local fyChanged,nfy = tImGui.DragFloat('Y##swlCamFY',state.cam.fy,step,0,0,'%.3f')
        local fzChanged,nfz = tImGui.DragFloat('Z##swlCamFZ',state.cam.fz,step,0,0,'%.3f')
        tImGui.PopItemWidth()
        if fxChanged then state.cam.fx=nfx end
        if fyChanged then state.cam.fy=nfy end
        if fzChanged then state.cam.fz=nfz end
        if fxChanged or fyChanged or fzChanged then applyCamera() end
        if tImGui.Button(tLang.L('reset_camera')) then frameCamera(state.meshBounds) end
        tImGui.TextDisabled(tLang.L('cam_hint_3d'))
        tImGui.TextDisabled(tLang.L('cam_hint_keyboard'))
        tImGui.TextDisabled('Scroll: zoom  |  Drag AABB: move')
    end
    tImGui.End()
end

local function resetRuntimeLight()
    state.runtimeLight={enabled=false,
        ambientColor={r=0.16,g=0.16,b=0.2,a=1},
        directionalColor={r=1,g=0.96,b=0.88,a=1},
        directionalDirection={x=-0.4,y=-0.8,z=0.5},orbit=nil}
    state.runtimeLight.orbit=tUtil.orbitFromDir(state.runtimeLight.directionalDirection)
    applyRuntimeLighting()
end

local function rebuildRuntimePreviewForLightChange()
    if not state.meshD then return end
    if state.runtimePreviewFromMemory then rebuildRuntimePreviewFromMemory()
    else rebuildPreview() end
end

local function showRuntimeLightWindow()
    if state.workspace~='runtime' then return end
    local screenW=mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=math.max(0,screenW-475),y=500},
        tImGui.Flags('ImGuiCond_Once'))
    local flags=tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize','ImGuiWindowFlags_NoCollapse')
    local opened=tImGui.Begin(tLang.L('swl_runtime_light_window')..'##swlRuntimeLight',false,flags)
    if opened then
        local light=state.runtimeLight
        local enabled=tImGui.Checkbox(tLang.L('swl_runtime_lighting'),light.enabled)
        if enabled~=light.enabled then
            light.enabled=enabled
            applyRuntimeLighting()
            rebuildRuntimePreviewForLightChange()
        end
        showItemTooltip(tLang.L('swl_runtime_lighting_help'))
        local colorFlags=tImGui.Flags('ImGuiColorEditFlags_NoInputs')
        tImGui.Text(tLang.L('ambient'))
        tImGui.SameLine()
        local ambientChanged,ambient=tImGui.ColorEdit4('##swlRuntimeAmbient',
            light.ambientColor,colorFlags)
        if ambientChanged and ambient then
            light.ambientColor=ambient
            if light.enabled then mbm.setAmbientLight('3d',ambient) end
        end
        tImGui.Text(tLang.L('directional_color'))
        tImGui.SameLine()
        local directionalChanged,directional=tImGui.ColorEdit4('##swlRuntimeDirectional',
            light.directionalColor,colorFlags)
        if directionalChanged and directional then
            light.directionalColor=directional
            if light.enabled then mbm.setDirectionalLightColor('3d',directional) end
        end
        tImGui.Text(tLang.L('direction_label'))
        light.orbit=light.orbit or tUtil.orbitFromDir(light.directionalDirection)
        if tUtil.drawOrbitGizmo(light.orbit,{size=110}) then
            light.directionalDirection=tUtil.dirFromOrbit(light.orbit)
            if light.enabled then
                mbm.setDirectionalLightDirection('3d',light.directionalDirection.x,
                    light.directionalDirection.y,light.directionalDirection.z)
            end
        end
        tImGui.TextDisabled(string.format('x=%.3f',light.directionalDirection.x))
        tImGui.TextDisabled(string.format('y=%.3f',light.directionalDirection.y))
        tImGui.TextDisabled(string.format('z=%.3f',light.directionalDirection.z))
        if tImGui.Button(tLang.L('reset_light')..'##swlRuntimeLightReset') then
            resetRuntimeLight()
            rebuildRuntimePreviewForLightChange()
        end
        tImGui.TextWrapped(tLang.L('swl_runtime_light_scope'))
    end
    tImGui.End()
end

function onInitScene()
    camera3d = mbm.getCamera('3d')
    camera3d:setFar(9999999)
    noMoveFlag = tImGui.Flags('ImGuiWindowFlags_NoCollapse')
    tUtil.sMessageOverlay = tLang.L('swl_welcome')
    tUtil.bRightSide = true
    tUtil.tTimerOverlay:start()
    mbm.setColor(0.08, 0.09, 0.12)
    state.runtimeLight.orbit=tUtil.orbitFromDir(state.runtimeLight.directionalDirection)
    applyRuntimeLighting()
    applyCamera()
end

function onLoop(delta)
    updateCameraKeyboard(delta)
    updatePaintCursorHover()
    updateAuthoringPlayback(delta)
    showMenu()
    showPanel()
    local tutorialNavigation=tTutorials.renderWindow(tImGui,tLang,state.leftPanelRight,
        state.meshD~=nil)
    if tutorialNavigation then
        setWorkspace(tutorialNavigation.workspace)
        tTutorials.requestFocus(tutorialNavigation.focus)
        setStatus(tLang.L(tutorialNavigation.statusKey),false,true)
    end
    showCameraPanel()
    showRuntimeLightWindow()
    swlShowAnimationImportWindow()
    showSkeletalTimelineWindow()
    syncRuntimeComparisonPreview()
    if state.workspace=='runtime' and state.skeletalPreview.playing and
            state.skeletalPreview.layerMaskShowSkeleton then
        if not updateRuntimeSkeletonVisuals() then rebuildSkeletonVisuals() end
    end
    tUtil.showOverlayMessage()
end

function onEndScene()
    unloadAllRuntimeWearables()
    destroyObject(state.preview)
    destroyObject(state.comparisonPreview)
    destroyObject(state.animationImport.sourceMesh)
    clearPaintVisuals()
    destroySkeletonVisuals()
    clearRollback()
end

function onTouchDown(key, x, y)
    if key==1 and state.workspace=='paint' and state.paint.aabbCapture.active then return end
    if key==0 and state.workspace=='paint' and state.paint.aabbCapture.active and
            not tImGui.GetWantCaptureMouse() then
        local capture=state.paint.aabbCapture
        local b=capture.bounds
        if b and rayHitsAABB(x,y,b) then
            local px,py,pz=cameraPosition()
            local nx,ny,nz=state.cam.fx-px,state.cam.fy-py,state.cam.fz-pz
            local length=math.sqrt(nx*nx+ny*ny+nz*nz)
            if length>1e-6 then nx,ny,nz=nx/length,ny/length,nz/length end
            local center={x=(b.minX+b.maxX)*0.5,y=(b.minY+b.maxY)*0.5,
                z=(b.minZ+b.maxZ)*0.5}
            local wx,wy,wz=rayPlaneHit(x,y,center,{x=nx,y=ny,z=nz})
            if wx then
                capture.dragPlane={point=center,normal={x=nx,y=ny,z=nz}}
                capture.dragOffset={x=center.x-wx,y=center.y-wy,z=center.z-wz}
                return
            end
        end
    end
    if key==1 and state.workspace=='bone_editor' and state.boneEditorDrag then
        cancelBoneEditorDrag()
        return
    end
    if key==1 and state.workspace=='paint' and state.paint.visualizationMode==1 and
            not tImGui.GetWantCaptureMouse() then
        local hit=pickPaintSurface(x,y)
        if hit then beginPaintStroke(hit) end
        return
    end
    if key == 0 and not tImGui.GetWantCaptureMouse() then
        if state.workspace=='paint' and state.paint.visualizationMode~=1 and
                state.showAdvancedDiagnostics and state.paint.showVertexInspector then
            state.paint.inspectorClick={startX=x,startY=y,moved=false}
        end
        if state.workspace=='paint' and state.paint.visualizationMode==1 then
            local selectedBone=hitTestPaintBone(x,y)
            if selectedBone then
                state.paint.boneIndex=selectedBone
                state.paint.heatmapDirty=true
                rebuildPaintHeatmap()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                return
            end
        end
        if state.workspace=='bone_editor' then
            local selection=applyBoneEditorToolIntent(hitTestBoneEditor(x,y))
            local axisOverride=not selection and hitTestBoneEditorAxis(x,y) or nil
            if axisOverride then selection=state.boneEditorSelection end
            if axisOverride then state.boneEditorPendingCycle=nil end
            state.boneEditorSelection=selection
            state.boneEditorSelectedIndex=selection and selection.boneIndex or nil
            if selection then
                state.boneIndex=selection.boneIndex
                applyWorkspaceVisibility()
                if selection.kind=='head' or selection.kind=='tail' or selection.kind=='joint' or
                        selection.kind=='segment' then
                    local bone=getBones()[selection.boneIndex]
                    local head,tail=getBoneEditorEndpoints(bone,1)
                    local rotatingSegment=selection.kind=='segment' and
                        state.boneEditorSegmentTool==2
                    local dragPoint=selection.kind=='head' and head or rotatingSegment and head or
                        selection.kind=='segment' and {x=(head.x+tail.x)*0.5,
                            y=(head.y+tail.y)*0.5,z=(head.z+tail.z)*0.5} or tail
                    local parent=nil
                    if bone.parentName then
                        for _,candidate in ipairs(getBones()) do
                            if candidate.name==bone.parentName then parent=candidate break end
                        end
                    end
                    local px,py,pz=cameraPosition()
                    local nx,ny,nz=state.cam.fx-px,state.cam.fy-py,state.cam.fz-pz
                    local normalLength=math.sqrt(nx*nx+ny*ny+nz*nz)
                    if normalLength>1e-6 then
                        nx,ny,nz=nx/normalLength,ny/normalLength,nz/normalLength
                        local wx,wy,wz=rayPlaneHit(x,y,dragPoint,{x=nx,y=ny,z=nz})
                        local snapshot=wx and stageRollbackSnapshot() or nil
                        if snapshot then
                            state.boneEditorDrag={boneIndex=selection.boneIndex,
                                boneId=selection.boneId,boneName=selection.boneName,
                                mode=selection.kind=='head' and 'head' or
                                    rotatingSegment and 'rotate_segment' or
                                    selection.kind=='segment' and 'segment' or 'tail',parent=parent,
                                head={x=bone.x,y=bone.y,z=bone.z},
                                tail={x=tail.x,y=tail.y,z=tail.z},
                                worldTailLength=math.sqrt((tail.x-bone.x)^2+
                                    (tail.y-bone.y)^2+(tail.z-bone.z)^2),
                                globalMatrix=bone.globalMatrix,point=dragPoint,
                                startWorldHit=axisOverride and {x=dragPoint.x,y=dragPoint.y,
                                    z=dragPoint.z} or {x=wx,y=wy,z=wz},
                                axisOverride=axisOverride,
                                startAxisParameter=axisOverride and rayAxisParameter(x,y,
                                    state.boneEditorAxisGizmo.origin,
                                    axisOverride=='x' and {x=1,y=0,z=0} or
                                    axisOverride=='y' and {x=0,y=1,z=0} or
                                    {x=0,y=0,z=1}) or nil,
                                plane={point=dragPoint,normal={x=nx,y=ny,z=nz}},snapshot=snapshot,
                                moved=false,lastVisualTime=0,startX=x,startY=y}
                            return
                        end
                    end
                end
                return
            end
            applyWorkspaceVisibility()
        end
        if state.workspace=='bind' then
            local selectedBone=hitTestBindBone(x,y)
            if selectedBone then
                state.boneIndex=selectedBone
                applyWorkspaceVisibility()
                return
            end
        end
        local ring=hitTestRotationRing(x,y)
        if ring and state.authoringPose and state.authoringPose.bones[state.boneIndex] then
            if state.animationPlayback.playing then state.animationPlayback.paused=true end
            local posed=state.authoringPose.bones[state.boneIndex]
            local t,q,s=posed.localTranslation,posed.localRotation,posed.localScale
            state.rotationGizmo.drag={axisName=ring.name,axis=ring.axis,
                startDirection=ring.direction,
                origin={x=state.rotationGizmo.origin.x,y=state.rotationGizmo.origin.y,
                    z=state.rotationGizmo.origin.z},
                baseTranslation={x=t.x,y=t.y,z=t.z},
                baseRotation={x=q.x,y=q.y,z=q.z,w=q.w},
                scale={x=s.x,y=s.y,z=s.z},moved=false}
            return
        end
        local axisName=hitTestTranslationAxis(x,y)
        if axisName and state.authoringPose and state.authoringPose.bones[state.boneIndex] then
            if state.animationPlayback.playing then state.animationPlayback.paused=true end
            local axes={x={x=1,y=0,z=0},y={x=0,y=1,z=0},z={x=0,y=0,z=1}}
            local axis=axes[axisName]
            local parameter=rayAxisParameter(x,y,state.translationGizmo.origin,axis)
            local posed=state.authoringPose.bones[state.boneIndex]
            if parameter and posed then
                local t,q,s=posed.localTranslation,posed.localRotation,posed.localScale
                state.translationGizmo.drag={axisName=axisName,axis=axis,startParameter=parameter,
                    origin={x=state.translationGizmo.origin.x,y=state.translationGizmo.origin.y,
                        z=state.translationGizmo.origin.z},
                    baseTranslation={x=t.x,y=t.y,z=t.z},
                    rotation={x=q.x,y=q.y,z=q.z,w=q.w},scale={x=s.x,y=s.y,z=s.z},moved=false}
                return
            end
        end
        local scaleAxisName=hitTestScaleAxis(x,y)
        if scaleAxisName and state.authoringPose and
                state.authoringPose.bones[state.boneIndex] then
            if state.animationPlayback.playing then state.animationPlayback.paused=true end
            local axis=state.scaleGizmo.axesLocal[scaleAxisName]
            local parameter=rayAxisParameter(x,y,state.scaleGizmo.origin,axis)
            local posed=state.authoringPose.bones[state.boneIndex]
            if parameter and posed then
                local t,q,s=posed.localTranslation,posed.localRotation,posed.localScale
                state.scaleGizmo.drag={axisName=scaleAxisName,axis=axis,
                    startParameter=parameter,length=state.scaleGizmo.length,
                    origin={x=state.scaleGizmo.origin.x,y=state.scaleGizmo.origin.y,
                        z=state.scaleGizmo.origin.z},
                    translation={x=t.x,y=t.y,z=t.z},
                    rotation={x=q.x,y=q.y,z=q.z,w=q.w},
                    baseScale={x=s.x,y=s.y,z=s.z},moved=false}
                return
            end
        end
        local selectedBone=hitTestAuthoringBone(x,y)
        if selectedBone then
            state.boneIndex=selectedBone
            clearAuthoringOverride()
            refreshAuthoringPose(state.authoringActiveClip)
            return
        end
        mouseDown, mouseX, mouseY = true, x, y
    end
end

function onTouchMove(key, x, y)
    local inspectorClick=state.paint.inspectorClick
    if inspectorClick and (x-inspectorClick.startX)^2+(y-inspectorClick.startY)^2>9 then
        inspectorClick.moved=true
    end
    local capture=state.paint.aabbCapture
    if state.workspace=='paint' and capture.active and capture.dragPlane and
            capture.dragOffset then
        local wx,wy,wz=rayPlaneHit(x,y,capture.dragPlane.point,capture.dragPlane.normal)
        if wx then
            local b,o=capture.bounds,capture.dragOffset
            local cx,cy,cz=(b.minX+b.maxX)*0.5,(b.minY+b.maxY)*0.5,
                (b.minZ+b.maxZ)*0.5
            local nx,ny,nz=wx+o.x,wy+o.y,wz+o.z
            local dx,dy,dz=nx-cx,ny-cy,nz-cz
            b.minX,b.maxX=b.minX+dx,b.maxX+dx
            b.minY,b.maxY=b.minY+dy,b.maxY+dy
            b.minZ,b.maxZ=b.minZ+dz,b.maxZ+dz
            capture.dragPlane.point={x=nx,y=ny,z=nz}
            movePaintAabbCaptureObjects()
        end
        return
    end
    if state.workspace=='paint' then
        if state.paint.stroke then
            local hit=pickPaintSurface(x,y)
            if hit then
                extendPaintStroke(hit)
                rebuildPaintCursor(hit)
            end
            return
        elseif tImGui.GetWantCaptureMouse() then
            state.paint.cursorPendingX,state.paint.cursorPendingY=nil,nil
            if state.paint.cursor then rebuildPaintCursor(nil) end
        else
            local moved=not state.paint.cursorLastX or
                math.abs(x-state.paint.cursorLastX)>=1 or math.abs(y-state.paint.cursorLastY)>=1
            if moved then state.paint.cursorPendingX,state.paint.cursorPendingY=x,y end
        end
    end
    local drag=state.translationGizmo.drag
    local rotationDrag=state.rotationGizmo.drag
    local scaleDrag=state.scaleGizmo.drag
    local boneDrag=state.boneEditorDrag
    if state.workspace=='bone_editor' and boneDrag then
        local screenDx,screenDy=x-boneDrag.startX,y-boneDrag.startY
        local wx,wy,wz=nil,nil,nil
        if screenDx*screenDx+screenDy*screenDy>9 then
            wx,wy,wz=rayPlaneHit(x,y,boneDrag.plane.point,boneDrag.plane.normal)
        end
        if wx then
            if boneDrag.axisOverride and boneDrag.startAxisParameter then
                local axis=boneDrag.axisOverride=='x' and {x=1,y=0,z=0} or
                    boneDrag.axisOverride=='y' and {x=0,y=1,z=0} or {x=0,y=0,z=1}
                local axisOrigin=boneDrag.mode=='rotate_segment' and boneDrag.tail or boneDrag.point
                local parameter=rayAxisParameter(x,y,axisOrigin,axis)
                if parameter then
                    local amount=parameter-boneDrag.startAxisParameter
                    wx,wy,wz=axisOrigin.x+axis.x*amount,
                        axisOrigin.y+axis.y*amount,axisOrigin.z+axis.z*amount
                end
            end
            if boneDrag.mode=='segment' then
                wx=boneDrag.head.x+(wx-boneDrag.startWorldHit.x)
                wy=boneDrag.head.y+(wy-boneDrag.startWorldHit.y)
                wz=boneDrag.head.z+(wz-boneDrag.startWorldHit.z)
            end
            local snap=boneDrag.axisOverride and {
                x=boneDrag.axisOverride=='x',y=boneDrag.axisOverride=='y',
                z=boneDrag.axisOverride=='z'} or state.boneEditorSnapAxes
            local constrained=snap.x or snap.y or snap.z
            if constrained then
                local baseline=boneDrag.mode=='segment' and boneDrag.head or
                    boneDrag.mode=='rotate_segment' and boneDrag.tail or boneDrag.point
                wx=snap.x and wx or baseline.x
                wy=snap.y and wy or baseline.y
                wz=snap.z and wz or baseline.z
            end
            local snapStep=state.boneEditorSnapStep or 0
            if snapStep>1e-9 then
                local baseline=boneDrag.mode=='segment' and boneDrag.head or
                    boneDrag.mode=='rotate_segment' and boneDrag.tail or boneDrag.point
                local function snapped(value,start)
                    local units=(value-start)/snapStep
                    units=units>=0 and math.floor(units+0.5) or math.ceil(units-0.5)
                    return start+units*snapStep
                end
                wx,wy,wz=snapped(wx,baseline.x),snapped(wy,baseline.y),snapped(wz,baseline.z)
            end
            local lx,ly,lz
            if boneDrag.mode=='rotate_segment' then
                lx,ly,lz=worldDeltaToLocal(wx-boneDrag.head.x,wy-boneDrag.head.y,
                    wz-boneDrag.head.z,boneDrag.globalMatrix)
                local directionLength=lx and math.sqrt(lx*lx+ly*ly+lz*lz) or 0
                local matrix=boneDrag.globalMatrix or {}
                local worldX=lx and ((matrix[1] or 1)*lx+(matrix[5] or 0)*ly+
                    (matrix[9] or 0)*lz) or 0
                local worldY=lx and ((matrix[2] or 0)*lx+(matrix[6] or 1)*ly+
                    (matrix[10] or 0)*lz) or 0
                local worldZ=lx and ((matrix[3] or 0)*lx+(matrix[7] or 0)*ly+
                    (matrix[11] or 1)*lz) or 0
                local worldDirectionLength=math.sqrt(worldX*worldX+worldY*worldY+
                    worldZ*worldZ)
                if directionLength>1e-6 and worldDirectionLength>1e-6 and
                        boneDrag.worldTailLength>1e-6 then
                    local scale=boneDrag.worldTailLength/worldDirectionLength
                    lx,ly,lz=lx*scale,ly*scale,lz*scale
                else lx=nil end
            elseif boneDrag.mode=='head' or boneDrag.mode=='segment' then
                local parent=boneDrag.parent
                if parent then
                    lx,ly,lz=worldDeltaToLocal(wx-parent.x,wy-parent.y,wz-parent.z,
                        parent.globalMatrix)
                else lx,ly,lz=wx,wy,wz end
            else
                lx,ly,lz=worldDeltaToLocal(wx-boneDrag.head.x,wy-boneDrag.head.y,
                    wz-boneDrag.head.z,boneDrag.globalMatrix)
            end
            if lx then
                local ok=safeCall(function()
                    if boneDrag.mode=='head' then
                        return state.meshD:setSkeletalBoneHead(boneDrag.boneIndex,lx,ly,lz,
                            state.boneEditorPreserveOtherJoints)
                    elseif boneDrag.mode=='segment' then
                        return state.meshD:translateSkeletalBoneSegment(boneDrag.boneIndex,lx,ly,lz,
                            state.boneEditorPreserveOtherJoints)
                    elseif boneDrag.mode=='rotate_segment' then
                        return state.meshD:setSkeletalBoneTail(boneDrag.boneIndex,lx,ly,lz,true,
                            state.boneEditorPreserveOtherJoints)
                    end
                    return state.meshD:setSkeletalBoneTail(boneDrag.boneIndex,lx,ly,lz,true,
                        state.boneEditorPreserveOtherJoints)
                end)
                if ok then
                    boneDrag.moved=true
                    state.modified=true
                    local now=mbm.getTimeRun()
                    if now-boneDrag.lastVisualTime>=0.033 then
                        boneDrag.lastVisualTime=now
                        refreshBindReport(false)
                        rebuildSkeletonVisuals()
                        applyWorkspaceVisibility()
                    end
                end
            end
        end
    elseif state.workspace=='animation' and rotationDrag then
        local wx,wy,wz=rayPlaneHit(x,y,rotationDrag.origin,rotationDrag.axis)
        if wx then
            local current=normalizedDirection(wx-rotationDrag.origin.x,
                wy-rotationDrag.origin.y,wz-rotationDrag.origin.z)
            if current then
                local start=rotationDrag.startDirection
                local cx=start.y*current.z-start.z*current.y
                local cy=start.z*current.x-start.x*current.z
                local cz=start.x*current.y-start.y*current.x
                local sinAngle=rotationDrag.axis.x*cx+rotationDrag.axis.y*cy+
                    rotationDrag.axis.z*cz
                local cosAngle=math.max(-1,math.min(1,start.x*current.x+
                    start.y*current.y+start.z*current.z))
                local half=math.atan(sinAngle,cosAngle)*0.5
                local localAxis=rotationDrag.axisName=='x' and {x=1,y=0,z=0} or
                    rotationDrag.axisName=='y' and {x=0,y=1,z=0} or {x=0,y=0,z=1}
                local sine=math.sin(half)
                local delta={x=localAxis.x*sine,y=localAxis.y*sine,
                    z=localAxis.z*sine,w=math.cos(half)}
                local q=normalizedQuaternion(quaternionMultiply(rotationDrag.baseRotation,delta))
                if q then
                    state.authoringOverride={clipIndex=state.animationClipSelected,
                        time=state.authoringTime,boneIndex=state.boneIndex,channelMask=2,
                        translation=rotationDrag.baseTranslation,rotation=q,
                        scale=rotationDrag.scale}
                    invalidateAuthoringPose()
                    if refreshAuthoringPose(state.authoringActiveClip) then
                        rotationDrag.moved=true
                    end
                end
            end
        end
    elseif state.workspace=='animation' and scaleDrag and state.scaleGizmo.origin then
        local parameter=rayAxisParameter(x,y,scaleDrag.origin,scaleDrag.axis)
        if parameter then
            local factor=math.max(0.001,
                1+(parameter-scaleDrag.startParameter)/math.max(scaleDrag.length,0.0001))
            local scale={x=scaleDrag.baseScale.x,y=scaleDrag.baseScale.y,
                z=scaleDrag.baseScale.z}
            if scaleDrag.axisName=='uniform' then
                scale.x=scaleDrag.baseScale.x*factor
                scale.y=scaleDrag.baseScale.y*factor
                scale.z=scaleDrag.baseScale.z*factor
            else
                scale[scaleDrag.axisName]=scaleDrag.baseScale[scaleDrag.axisName]*factor
            end
            state.authoringOverride={clipIndex=state.animationClipSelected,
                time=state.authoringTime,boneIndex=state.boneIndex,channelMask=4,
                translation=scaleDrag.translation,rotation=scaleDrag.rotation,scale=scale}
            invalidateAuthoringPose()
            if refreshAuthoringPose(state.authoringActiveClip) then scaleDrag.moved=true end
        end
    elseif state.workspace=='animation' and drag and state.translationGizmo.origin then
        local parameter=rayAxisParameter(x,y,drag.origin,drag.axis)
        if parameter then
            local amount=parameter-drag.startParameter
            local wx,wy,wz=drag.axis.x*amount,drag.axis.y*amount,drag.axis.z*amount
            local bindBone=state.bindReport and state.bindReport.bones and
                state.bindReport.bones[state.boneIndex] or nil
            local parentIndex=bindBone and bindBone.parentIndex or 0
            local parentPose=parentIndex>0 and state.authoringPose and
                state.authoringPose.bones[parentIndex] or nil
            local lx,ly,lz=worldDeltaToLocal(wx,wy,wz,parentPose and parentPose.globalMatrix)
            if lx then
                state.authoringOverride={clipIndex=state.animationClipSelected,
                    time=state.authoringTime,boneIndex=state.boneIndex,channelMask=1,
                    translation={x=drag.baseTranslation.x+lx,y=drag.baseTranslation.y+ly,
                        z=drag.baseTranslation.z+lz},rotation=drag.rotation,scale=drag.scale}
                invalidateAuthoringPose()
                if refreshAuthoringPose(state.authoringActiveClip) then drag.moved=true end
            end
        end
    elseif mouseDown and not tImGui.GetWantCaptureMouse() then
        state.cam.azimuth = state.cam.azimuth + (x-mouseX) * 0.008
        state.cam.elevation = math.max(-1.45, math.min(1.45, state.cam.elevation + (y-mouseY) * 0.008))
        mouseX, mouseY = x, y
        applyCamera()
    end
end

function onTouchUp(key, x, y)
    if key==0 and state.paint.aabbCapture then
        state.paint.aabbCapture.dragPlane=nil
        state.paint.aabbCapture.dragOffset=nil
    end
    if key==1 and state.workspace=='paint' then
        if state.paint.stroke then
            local hit=pickPaintSurface(x,y)
            if hit then extendPaintStroke(hit) end
            commitPaintStroke()
            rebuildPaintCursor(hit)
        end
        return
    end
    if key == 0 then
        local inspectorClick=state.paint.inspectorClick
        if inspectorClick and not inspectorClick.moved and state.workspace=='paint' and
                state.paint.visualizationMode~=1 and state.showAdvancedDiagnostics and
                state.paint.showVertexInspector then
            local hit=pickPaintSurface(x,y)
            if hit then
                state.paint.inspectorPinned=false
                rebuildPaintCursor(hit)
                state.paint.inspectorPinned=state.paint.hoveredVertex~=nil
                state.paint.inspectorSeamSyncConfirmed=false
                destroyObject(state.paint.inspectorGeometryOverlay)
                state.paint.inspectorGeometryOverlay=nil
                state.paint.inspectorGeometryReport=nil
                destroyObject(state.paint.inspectorTopologyOverlay)
                state.paint.inspectorTopologyOverlay=nil
                state.paint.inspectorTopologyReport=nil
                state.paint.inspectorNormalReport=nil
                rebuildPinnedSeamInspector()
                applyWorkspaceVisibility()
            end
        end
        state.paint.inspectorClick=nil
        local boneDrag=state.boneEditorDrag
        local completedAuthoringDrag=(state.translationGizmo.drag and
            state.translationGizmo.drag.moved) or (state.rotationGizmo.drag and
            state.rotationGizmo.drag.moved) or (state.scaleGizmo.drag and
            state.scaleGizmo.drag.moved)
        local completedBoneDrag=boneDrag and boneDrag.moved
        if boneDrag then
            if boneDrag.moved then
                commitRollbackSnapshot(boneDrag.snapshot,'swl_history_bone_drag')
                rebuildPreview()
                refreshBindReport()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L(boneDrag.mode=='head' and 'swl_bone_editor_head_moved' or
                    boneDrag.mode=='segment' and 'swl_bone_editor_segment_moved' or
                    boneDrag.mode=='rotate_segment' and 'swl_bone_editor_segment_rotated' or
                    'swl_bone_editor_tail_moved'),false)
            else discardRollbackSnapshot(boneDrag.snapshot) end
            state.boneEditorDrag=nil
        end
        local pendingCycle=state.boneEditorPendingCycle
        if pendingCycle and not completedBoneDrag and pendingCycle.cycleOnRelease and
                #pendingCycle.candidates>1 then
            local nextIndex=(pendingCycle.currentIndex%#pendingCycle.candidates)+1
            local selection=applyBoneEditorToolIntent(pendingCycle.candidates[nextIndex].selection)
            state.boneEditorSelection=selection
            state.boneEditorSelectedIndex=selection.boneIndex
            state.boneIndex=selection.boneIndex
            applyWorkspaceVisibility()
        end
        state.boneEditorPendingCycle=nil
        if state.workspace=='animation' and state.animationAutoKey and
                completedAuthoringDrag and state.authoringOverride then
            commitAuthoringOverride()
        end
        state.translationGizmo.drag=nil
        state.rotationGizmo.drag=nil
        state.scaleGizmo.drag=nil
        mouseDown = false
    end
end

function onTouchZoom(zoom)
    if tImGui.GetWantCaptureMouse() then return end
    state.cam.distance = math.max(0.01, state.cam.distance * (1 - zoom * 0.15))
    applyCamera()
end

function onKeyDown(key)
    if key==mbm.getKeyCode('ESC') and state.workspace=='paint' and state.paint.stroke then
        cancelPaintStroke()
    elseif key==mbm.getKeyCode('ESC') and state.workspace=='bone_editor' and
            state.boneEditorDrag then
        cancelBoneEditorDrag()
    elseif key == mbm.getKeyCode('control') then
        state.controlDown = true
    elseif key == mbm.getKeyCode('shift') then
        state.shiftDown = true
    elseif state.controlDown and key == mbm.getKeyCode('C') and
            state.workspace=='animation' and not tImGui.IsAnyItemActive() then
        copyTimelineSelection()
    elseif state.controlDown and key == mbm.getKeyCode('V') and
            state.workspace=='animation' and not tImGui.IsAnyItemActive() then
        pasteTimelineClipboardAtPlayhead()
    elseif state.controlDown and key == mbm.getKeyCode('Z') then
        if state.shiftDown then redoHistory() else undoHistory() end
    elseif state.controlDown and key == mbm.getKeyCode('Y') then
        redoHistory()
    elseif state.controlDown and key == mbm.getKeyCode('O') then
        local path = mbm.openFile(state.fileName or '', 'msh')
        if path then loadMesh(path) end
    elseif state.controlDown and key == mbm.getKeyCode('S') then
        saveTo(state.fileName)
    elseif state.controlDown and key == mbm.getKeyCode('Q') then
        clearRollback()
        mbm.quit()
    elseif not state.controlDown and key == mbm.getKeyCode('W') then cameraMove.forward=1
    elseif not state.controlDown and key == mbm.getKeyCode('S') then cameraMove.forward=-1
    elseif not state.controlDown and key == mbm.getKeyCode('A') then cameraMove.right=-1
    elseif not state.controlDown and key == mbm.getKeyCode('D') then cameraMove.right=1
    elseif key == mbm.getKeyCode('pageup') then cameraMove.vertical=1
    elseif key == mbm.getKeyCode('pagedown') then cameraMove.vertical=-1
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then state.controlDown = false
    elseif key == mbm.getKeyCode('shift') then state.shiftDown = false
    elseif key == mbm.getKeyCode('W') or key == mbm.getKeyCode('S') then cameraMove.forward=0
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('D') then cameraMove.right=0
    elseif key == mbm.getKeyCode('pageup') or key == mbm.getKeyCode('pagedown') then cameraMove.vertical=0
    end
end
