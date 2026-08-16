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

local state = {
    fileName = nil,
    meshD = nil,
    preview = nil,
    comparisonPreview = nil,
    skeletalPreview = {clips={}, selected=1, method=1, duration=0, playing=false, paused=false,
        poseStress=false, comparisonReady=false},
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
    runtimePreviewFromMemory = false,
    runtimePreviewMemoryDirty = false,
    animationPlayback = {playing=false,paused=false,speed=1},
    leftPanelRight = 440,
    translationGizmo = {axes={},boneIndex=nil,poseKey=nil,drag=nil},
    rotationGizmo = {rings={},origin=nil,radius=nil,drag=nil},
    scaleGizmo = {axes={},origin=nil,length=nil,drag=nil},
    boneEditorPosition = {x=0,y=0,z=0},
    boneEditorLength = 1,
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
    boneEditorRemovePreviewIndex = nil,
    boneEditorRadiusBoneId = nil,
    boneEditorRadius = 0,
    boneEditorRadiusSubtree = false,
    boneEditorRotationGuide = nil,
    workspace = 'weights',
    meshVisible = true,
    skeletonVisible = true,
    skeletonAlwaysOnTop = true,
    skeletonGizmo = {spheres={}, bones={}},
    skeletonGizmoGeneration = 0,
    analysisBoneHighlight = false,
    analysisBoneHighlightSphere = nil,
    proximityBoneHighlight = false,
    proximityBoneHighlightSphere = nil,
    targetBoneHighlight = false,
    targetBoneHighlightSphere = nil,
    markersAlwaysOnTop = true,
    analysisMarkersVisible = true,
    abruptMarkersVisible = true,
    boundaryMarkersVisible = true,
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
    normalizeReport = nil,
    operationMode = 1, -- 1 inspect, 2 rigid, 3 normalize, 4 smooth, 5 repair abrupt
    selectionMode = 1, -- 1 AABB, 2 subset, 3 bone proximity
    subsetIndex = 1,
    boneIndex = 1,
    proximityRadius = 1,
    proximityNearestOnly = false,
    proximityCapsule = nil,
    analysisBoneIndex = 1,
    targetBoneIndex = 1,
    aabb = nil,
    aabbDragSensitivity = 0.001,
    analysis = nil,
    analysisDirty = true,
    undoStack = {},
    redoStack = {},
    historyLimit = 50,
    shiftDown = false,
    selectionLines = nil,
    transitionLines = nil,
    heatmapLines = {},
    selectionBox = nil,
    transitionBox = nil,
    shellFaces = {
        minX={enabled=true,width=0}, maxX={enabled=true,width=0},
        minY={enabled=true,width=0}, maxY={enabled=true,width=0},
        minZ={enabled=true,width=0}, maxZ={enabled=true,width=0},
    },
    falloffMode = 2, -- 1 linear, 2 smooth
    heatmapEnabled = true,
    restrictBones = false,
    allowedBones = {},
    allowedBonesHighlight = false,
    hoveredAllowedBone = nil,
    smoothStrength = 0.5,
    smoothIterations = 1,
    abruptThreshold = 0.35,
    abruptDiagnostics = nil,
    abruptLines = nil,
    boundaryLines = nil,
    paint = {boneIndex=1,boneId=nil,radius=0.1,geometry=nil,heatmapLines={},cursor=nil,
        cursorHit=nil,heatmapDirty=true,showSkeleton=true,heatmapGeneration=0,
        cursorLastX=nil,cursorLastY=nil,cursorLastUpdate=0,cursorPendingX=nil,cursorPendingY=nil,
        heatmapIndexed=false,strength=0.25,falloffMode=2,operationMode=1,
        smoothIterations=3,cleanThreshold=0.01,visualizationMode=1,
        distributionStats=nil,stroke=nil},
    topologyAdjacency = nil,
    meshBounds = nil,
    aabbDragging = false,
    aabbDragPlane = nil,
    aabbDragOffset = nil,
    status = nil,
    statusError = false,
    cam = {azimuth = 0.35, elevation = 0.25, distance = 5, fx = 0, fy = 0, fz = 0},
}

local camera3d
local mouseDown = false
local mouseX, mouseY = 0, 0
local noMoveFlag = 0
local cameraMove = {forward=0, right=0, vertical=0}

local function isWeightLabWorkspace()
    return state.workspace == 'weights'
end

local function shouldShowSkeleton()
    return state.workspace=='bind' or state.workspace=='bone_editor' or state.workspace=='animation' or
        (state.workspace=='paint' and state.paint.showSkeleton) or
        (isWeightLabWorkspace() and state.skeletonVisible)
end

local rebuildSkeletonVisuals
local rebuildPaintHeatmap
local buildTopologyAdjacency

local function safeCall(fn)
    local result = table.pack(pcall(fn))
    if not result[1] then
        state.status = tostring(result[2])
        state.statusError = true
        return false
    end
    return true, table.unpack(result, 2, result.n)
end

local function setStatus(message, isError)
    state.status = message
    state.statusError = isError == true
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

local function clearPaintVisuals()
    for _,object in ipairs(state.paint.heatmapLines) do destroyObject(object) end
    destroyObject(state.paint.cursor)
    state.paint.heatmapLines={}
    state.paint.cursor=nil
    state.paint.cursorHit=nil
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

local function clearSelectionVisuals()
    destroyObject(state.selectionLines)
    destroyObject(state.transitionLines)
    destroyObject(state.selectionBox)
    destroyObject(state.transitionBox)
    destroyObject(state.proximityCapsule)
    destroyObject(state.abruptLines)
    destroyObject(state.boundaryLines)
    for _, object in ipairs(state.heatmapLines) do destroyObject(object) end
    state.selectionLines = nil
    state.transitionLines = nil
    state.selectionBox = nil
    state.transitionBox = nil
    state.proximityCapsule = nil
    state.abruptLines = nil
    state.boundaryLines = nil
    state.heatmapLines = {}
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

local function invalidateAnalysis()
    state.analysis = nil
    state.abruptDiagnostics = nil
    state.analysisDirty = true
    destroyObject(state.selectionLines)
    destroyObject(state.transitionLines)
    destroyObject(state.abruptLines)
    destroyObject(state.boundaryLines)
    for _, object in ipairs(state.heatmapLines) do destroyObject(object) end
    state.selectionLines = nil
    state.transitionLines = nil
    state.abruptLines = nil
    state.boundaryLines = nil
    state.heatmapLines = {}
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

local function rebuildSelectionBox()
    destroyObject(state.selectionBox)
    destroyObject(state.transitionBox)
    state.selectionBox, state.transitionBox = nil, nil
    if not state.meshD or state.selectionMode ~= 1 or not state.aabb then return end
    state.selectionBox = createSelectionBox(state.aabb, 0, 1, 1)
    local b,faces=state.aabb,state.shellFaces
    local outer={
        minX=b.minX-(faces.minX.enabled and faces.minX.width or 0),
        maxX=b.maxX+(faces.maxX.enabled and faces.maxX.width or 0),
        minY=b.minY-(faces.minY.enabled and faces.minY.width or 0),
        maxY=b.maxY+(faces.maxY.enabled and faces.maxY.width or 0),
        minZ=b.minZ-(faces.minZ.enabled and faces.minZ.width or 0),
        maxZ=b.maxZ+(faces.maxZ.enabled and faces.maxZ.width or 0),
    }
    if outer.minX<b.minX or outer.maxX>b.maxX or outer.minY<b.minY or
            outer.maxY>b.maxY or outer.minZ<b.minZ or outer.maxZ>b.maxZ then
        state.transitionBox = createSelectionBox({
            minX=outer.minX,minY=outer.minY,minZ=outer.minZ,
            maxX=outer.maxX,maxY=outer.maxY,maxZ=outer.maxZ,
        }, 1, 0.65, 0)
    end
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
    if state.workspace~='animation' or not state.authoringPose or
            type(state.authoringPose.bones)~='table' then return bones end
    for index,bone in ipairs(bones) do
        local posed=state.authoringPose.bones[index]
        local global=posed and posed.globalMatrix or nil
        if global then
            bone.x=global[13] or bone.x
            bone.y=global[14] or bone.y
            bone.z=global[15] or bone.z
            bone.globalMatrix=global
        end
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

local function rebuildProximityCapsule()
    destroyObject(state.proximityCapsule)
    state.proximityCapsule=nil
    if not state.meshD or state.selectionMode~=3 then return end
    local bones=getBones()
    local bone=bones[state.boneIndex]
    if not bone then return end
    local parent=findBone(bones,bone.parentName) or bone
    local ax,ay,az=bone.x,bone.y,bone.z
    local bx,by,bz=parent.x,parent.y,parent.z
    local dx,dy,dz=bx-ax,by-ay,bz-az
    local length=math.sqrt(dx*dx+dy*dy+dz*dz)
    local ux,uy,uz
    if length>1e-8 then ux,uy,uz=dx/length,dy/length,dz/length else ux,uy,uz=0,1,0 end
    local rx,ry,rz=math.abs(uy)<0.9 and 0 or 1,math.abs(uy)<0.9 and 1 or 0,0
    local vx,vy,vz=uy*rz-uz*ry,uz*rx-ux*rz,ux*ry-uy*rx
    local vLength=math.sqrt(vx*vx+vy*vy+vz*vz)
    vx,vy,vz=vx/vLength,vy/vLength,vz/vLength
    local wx,wy,wz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
    local radius=math.max(tonumber(state.proximityRadius) or 0,0)
    local coords,segments={},24
    local function point(cx,cy,cz,angle)
        local c,s=math.cos(angle)*radius,math.sin(angle)*radius
        return cx+vx*c+wx*s,cy+vy*c+wy*s,cz+vz*c+wz*s
    end
    local function appendCircle(cx,cy,cz,p1x,p1y,p1z,p2x,p2y,p2z)
        for index=0,segments-1 do
            local a1,a2=index*math.pi*2/segments,(index+1)*math.pi*2/segments
            local c1,s1=math.cos(a1)*radius,math.sin(a1)*radius
            local c2,s2=math.cos(a2)*radius,math.sin(a2)*radius
            appendPoint(coords,cx+p1x*c1+p2x*s1,cy+p1y*c1+p2y*s1,cz+p1z*c1+p2z*s1)
            appendPoint(coords,cx+p1x*c2+p2x*s2,cy+p1y*c2+p2y*s2,cz+p1z*c2+p2z*s2)
        end
    end
    for _,center in ipairs({{ax,ay,az},{bx,by,bz}}) do
        appendCircle(center[1],center[2],center[3],vx,vy,vz,wx,wy,wz)
        appendCircle(center[1],center[2],center[3],ux,uy,uz,vx,vy,vz)
        appendCircle(center[1],center[2],center[3],ux,uy,uz,wx,wy,wz)
    end
    for index=0,7 do
        local angle=index*math.pi*2/8
        local x1,y1,z1=point(ax,ay,az,angle)
        local x2,y2,z2=point(bx,by,bz,angle)
        appendPoint(coords,x1,y1,z1); appendPoint(coords,x2,y2,z2)
    end
    local capsule=line:new('3d',0,0,0)
    capsule:add(coords)
    capsule:setColor(1,0.55,0,1)
    capsule:setPos(0,0,0)
    capsule.alwaysOnTop=true
    state.proximityCapsule=capsule
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
    state.skeletonGizmo={spheres={},bones={}}
    if not preserveTranslationGizmo then
        for _,object in pairs(state.translationGizmo.axes) do destroyObject(object) end
        state.translationGizmo={axes={},boneIndex=nil,poseKey=nil,drag=nil}
        for _,object in pairs(state.rotationGizmo.rings) do destroyObject(object) end
        state.rotationGizmo={rings={},origin=nil,radius=nil,drag=nil}
        for _,object in pairs(state.scaleGizmo.axes) do destroyObject(object) end
        state.scaleGizmo={axes={},origin=nil,length=nil,drag=nil}
    end
    destroyObject(state.analysisBoneHighlightSphere)
    state.analysisBoneHighlightSphere=nil
    destroyObject(state.proximityBoneHighlightSphere)
    state.proximityBoneHighlightSphere=nil
    destroyObject(state.targetBoneHighlightSphere)
    state.targetBoneHighlightSphere=nil
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
    return object
end

local function updateSkeletonVisibility()
    local weightWorkspace=isWeightLabWorkspace()
    local analyzedBone=weightWorkspace and state.analysisBoneHighlight and
        getBones()[state.analysisBoneIndex] or nil
    local targetBone=weightWorkspace and state.targetBoneHighlight and
        getBones()[state.targetBoneIndex] or nil
    local proximityBone=weightWorkspace and state.selectionMode==3 and state.proximityBoneHighlight and
        getBones()[state.boneIndex] or nil
    for name,object in pairs(state.skeletonGizmo.spheres) do
        -- Highlight spheres replace their regular joints instead of occupying the same surface
        -- in the same always-on-top depth pass.
        local highlighted=(analyzedBone and name==analyzedBone.name) or
            (targetBone and name==targetBone.name) or
            (proximityBone and name==proximityBone.name)
        object.visible=shouldShowSkeleton() and not highlighted
    end
    for _,object in pairs(state.skeletonGizmo.bones) do object.visible=shouldShowSkeleton() end
end

local function applyWorkspaceVisibility()
    local weightWorkspace=isWeightLabWorkspace()
    local paintWorkspace=state.workspace=='paint'
    local runtimeWorkspace=state.workspace=='runtime'
    local analysisVisible=weightWorkspace and state.analysisMarkersVisible
    rebuildBoneEditorOrientationIndicator()
    rebuildBoneEditorAxisGizmo()
    rebuildBoneEditorRotationGuide()

    if state.preview then
        -- Paint Weights' filled-face heatmap is a complete read-only surface copy. Hiding the
        -- textured preview there avoids z-fighting and color mixing; other worktrees keep it.
        state.preview.visible=state.meshVisible and not
            (paintWorkspace and #state.paint.heatmapLines>0)
        pcall(function()
            local x=0
            if runtimeWorkspace and state.skeletalPreview.poseStress then
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
            state.skeletalPreview.poseStress
    end
    if state.selectionBox then state.selectionBox.visible=weightWorkspace end
    if state.transitionBox then state.transitionBox.visible=weightWorkspace end
    if state.proximityCapsule then state.proximityCapsule.visible=weightWorkspace end
    if state.selectionLines then state.selectionLines.visible=analysisVisible end
    if state.transitionLines then state.transitionLines.visible=analysisVisible end
    for _,marker in ipairs(state.heatmapLines) do marker.visible=analysisVisible end
    for _,marker in ipairs(state.paint.heatmapLines) do
        marker.visible=paintWorkspace and state.meshVisible
    end
    if state.paint.cursor then state.paint.cursor.visible=paintWorkspace and state.meshVisible end
    if state.abruptLines then
        state.abruptLines.visible=weightWorkspace and state.abruptMarkersVisible
    end
    if state.boundaryLines then
        state.boundaryLines.visible=weightWorkspace and state.boundaryMarkersVisible
    end
    if state.analysisBoneHighlightSphere then
        state.analysisBoneHighlightSphere.visible=shouldShowSkeleton() and weightWorkspace and
            state.analysisBoneHighlight
    end
    if state.proximityBoneHighlightSphere then
        state.proximityBoneHighlightSphere.visible=shouldShowSkeleton() and weightWorkspace and
            state.proximityBoneHighlight
    end
    if state.targetBoneHighlightSphere then
        state.targetBoneHighlightSphere.visible=shouldShowSkeleton() and weightWorkspace and
            state.targetBoneHighlight
    end
    local selectedBindBone=(state.workspace=='bind' or state.workspace=='animation' or
        state.workspace=='paint') and
        getBones()[state.boneIndex] or (state.workspace=='bone_editor' and
        state.boneEditorSelectedIndex and getBones()[state.boneEditorSelectedIndex] or nil)
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
        elseif weightWorkspace and name==state.hoveredAllowedBone then
            object:setColor(1,0.45,0.05,1)
        elseif weightWorkspace and state.allowedBonesHighlight and state.allowedBones[name] then
            object:setColor(0.1,0.85,1,0.95)
        elseif selectedBindBone and name==selectedBindBone.name then
            object:setColor(0.1,0.85,1,1)
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
        else
            object:setColor(1,0,1,0.75)
        end
    end
    updateSkeletonVisibility()
end

local function setWorkspace(workspace)
    if state.workspace==workspace then return end
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
    state.aabbDragging=false
    state.aabbDragPlane=nil
    state.aabbDragOffset=nil
    if rebuildSkeletonVisuals then rebuildSkeletonVisuals() end
    if workspace=='paint' and state.paint.heatmapDirty then rebuildPaintHeatmap() end
    applyWorkspaceVisibility()
    if state.meshBounds then
        local bounds={}
        for key,value in pairs(state.meshBounds) do bounds[key]=value end
        if workspace=='runtime' and state.skeletalPreview.poseStress then
            local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                bounds.maxZ-bounds.minZ)
            local separation=extent*0.65
            bounds.minX=bounds.minX-separation
            bounds.maxX=bounds.maxX+separation
        end
        frameCamera(bounds)
    end
end

local function rebuildProximityBoneHighlight()
    destroyObject(state.proximityBoneHighlightSphere)
    state.proximityBoneHighlightSphere=nil
    if state.selectionMode~=3 or not state.proximityBoneHighlight then
        updateSkeletonVisibility()
        return
    end
    local bone=getBones()[state.boneIndex]
    if not bone then
        updateSkeletonVisibility()
        return
    end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(bone.radius or 0,extent*0.018,0.001)
    local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
        'swl_proximity_bone_highlight_',1,0.45,0.05,0.95)
    sphere:setScale(radius*1.9,radius*1.9,radius*1.9)
    sphere.visible=shouldShowSkeleton() and isWeightLabWorkspace()
    sphere.alwaysOnTop=true
    state.proximityBoneHighlightSphere=sphere
    updateSkeletonVisibility()
end

local function updateAllowedBoneColors()
    local selectedBindBone=state.workspace=='bind' and getBones()[state.boneIndex] or nil
    for name,object in pairs(state.skeletonGizmo.spheres) do
        if selectedBindBone and name==selectedBindBone.name then
            object:setColor(0.1,0.85,1,1)
        elseif not isWeightLabWorkspace() then
            object:setColor(1,0,1,0.85)
        elseif name==state.hoveredAllowedBone then
            object:setColor(1,0.45,0.05,1)
        elseif state.allowedBonesHighlight and state.allowedBones[name] then
            object:setColor(0.1,0.85,1,0.95)
        else
            object:setColor(1,0,1,0.85)
        end
    end
end

local function rebuildAnalysisBoneHighlight()
    destroyObject(state.analysisBoneHighlightSphere)
    state.analysisBoneHighlightSphere=nil
    if not state.analysisBoneHighlight then
        updateSkeletonVisibility()
        return
    end
    local bone=getBones()[state.analysisBoneIndex]
    if not bone then
        updateSkeletonVisibility()
        return
    end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(bone.radius or 0,extent*0.018,0.001)
    local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
        'swl_analysis_bone_highlight_',1,1,0,0.95)
    sphere:setScale(radius*1.6,radius*1.6,radius*1.6)
    sphere.visible=shouldShowSkeleton() and isWeightLabWorkspace()
    sphere.alwaysOnTop=true
    state.analysisBoneHighlightSphere=sphere
    updateSkeletonVisibility()
end

local function rebuildTargetBoneHighlight()
    destroyObject(state.targetBoneHighlightSphere)
    state.targetBoneHighlightSphere=nil
    if not state.targetBoneHighlight then
        updateSkeletonVisibility()
        return
    end
    local bone=getBones()[state.targetBoneIndex]
    if not bone then
        updateSkeletonVisibility()
        return
    end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(bone.radius or 0,extent*0.018,0.001)
    local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
        'swl_target_bone_highlight_',0.1,1,0.2,0.9)
    sphere:setScale(radius*1.75,radius*1.75,radius*1.75)
    sphere.visible=shouldShowSkeleton() and isWeightLabWorkspace()
    sphere.alwaysOnTop=true
    state.targetBoneHighlightSphere=sphere
    updateSkeletonVisibility()
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
                if state.workspace=='animation' then
                    link=line:new('3d',parent.x,parent.y,parent.z)
                    link:add({0,0,0,dx,dy,dz})
                    link:setColor(1,0,1,0.9)
                    link.visible=shouldShowSkeleton()
                    link.alwaysOnTop=state.skeletonAlwaysOnTop
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
    rebuildAnalysisBoneHighlight()
    rebuildProximityBoneHighlight()
    rebuildTargetBoneHighlight()
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
        if bone.parentName then
            local parent=byName[bone.parentName]
            local link=state.skeletonGizmo.bones[bone.boneId]
            if not parent or not link then return false end
            link:set({0,0,0,bone.x-parent.x,bone.y-parent.y,bone.z-parent.z},1)
            link:setPos(parent.x,parent.y,parent.z)
        end
    end
    rebuildTranslationGizmo()
    rebuildRotationGizmo()
    rebuildScaleGizmo()
    updateSkeletonVisibility()
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

local function hitTestAuthoringBone(sx,sy)
    if state.workspace~='animation' or not state.authoringPose then return nil end
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

local function hitTestPaintBone(sx,sy)
    if state.workspace~='paint' then return nil end
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

local function collectVertices()
    local vertices = {}
    local okS, subsets = safeCall(function() return state.meshD:getTotalSubset(1) end)
    if not okS then return vertices end
    local offset = 0
    for subset = 1, subsets do
        local okV, total = safeCall(function() return state.meshD:getTotalVertex(1, subset) end)
        total = okV and total or 0
        for vertex = 1, total do
            local okP, p = safeCall(function() return state.meshD:getVertex(1, subset, vertex) end)
            if okP and p then
                vertices[#vertices+1] = {globalIndex=offset+vertex, subset=subset, point=p}
            end
        end
        offset = offset + total
    end
    return vertices
end

local function pointInsideAABB(p, b)
    return p.x >= b.minX and p.x <= b.maxX and p.y >= b.minY and p.y <= b.maxY and
           p.z >= b.minZ and p.z <= b.maxZ
end

local function hasTransitionShell()
    for _,face in pairs(state.shellFaces) do
        if face.enabled and face.width>0 then return true end
    end
    return false
end

local function transitionAlpha(p, b)
    if pointInsideAABB(p, b) then return 1, 'core' end
    local faces=state.shellFaces
    local distances={
        p.x<b.minX and {'minX',b.minX-p.x} or (p.x>b.maxX and {'maxX',p.x-b.maxX} or nil),
        p.y<b.minY and {'minY',b.minY-p.y} or (p.y>b.maxY and {'maxY',p.y-b.maxY} or nil),
        p.z<b.minZ and {'minZ',b.minZ-p.z} or (p.z>b.maxZ and {'maxZ',p.z-b.maxZ} or nil),
    }
    local t=0
    for _,distance in pairs(distances) do
        local face=faces[distance[1]]
        if not face.enabled or face.width<=0 then return nil end
        t=math.max(t,distance[2]/face.width)
    end
    if t >= 1 then return nil end
    if state.falloffMode == 2 then t = t*t*(3-2*t) end
    return math.max(0,math.min(1,1-t)), 'shell'
end

local function buildVertexMarkers(vertices, r, g, b, extent)
    if #vertices == 0 then return nil end
    local size = math.max(extent*0.006,0.001)
    local coords,step={},math.max(1,math.ceil(#vertices/500))
    for i=1,#vertices,step do
        local p=vertices[i].point
        appendPoint(coords,p.x-size,p.y,p.z); appendPoint(coords,p.x+size,p.y,p.z)
        appendPoint(coords,p.x,p.y-size,p.z); appendPoint(coords,p.x,p.y+size,p.z)
    end
    local marks=line:new('3d',0,0,0)
    marks:add(coords); marks:setColor(r,g,b,1); marks:setPos(0,0,0)
    marks.alwaysOnTop=state.markersAlwaysOnTop
    return marks
end

local function buildEdgeLines(edges,r,g,b)
    if #edges==0 then return nil end
    local coords={}
    for _,edge in ipairs(edges) do
        appendPoint(coords,edge[1].point.x,edge[1].point.y,edge[1].point.z)
        appendPoint(coords,edge[2].point.x,edge[2].point.y,edge[2].point.z)
    end
    local lines=line:new('3d',0,0,0)
    lines:add(coords)
    lines:setColor(r,g,b,1)
    lines:setPos(0,0,0)
    lines.alwaysOnTop=state.markersAlwaysOnTop
    return lines
end

local heatmapColors = {
    {0.1,0.25,1}, {0,0.85,1}, {0.1,1,0.25}, {1,0.9,0}, {1,0.45,0}, {1,0.1,0},
}

local paintHeatmapShaderName='skeletal_paint_weight_heatmap.ps'

local function ensurePaintHeatmapShader()
    if mbm.existShader(paintHeatmapShaderName) then return true end
    return mbm.addShader({name=paintHeatmapShaderName,code=[[
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
            gl_FragColor=vec4(heatColor(vTexCoord.x),1.0);
        }
    ]],var={},min={},max={}})
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

local function rebuildAnalysisMarkers(core,shell,extent)
    destroyObject(state.selectionLines); destroyObject(state.transitionLines)
    for _,object in ipairs(state.heatmapLines) do destroyObject(object) end
    state.selectionLines,state.transitionLines=nil,nil
    state.heatmapLines={}
    if not state.heatmapEnabled then
        state.selectionLines=buildVertexMarkers(core,1,0,1,extent)
        state.transitionLines=buildVertexMarkers(shell,0.9,0.9,0.9,extent)
        if state.selectionLines then state.selectionLines.visible=state.analysisMarkersVisible end
        if state.transitionLines then state.transitionLines.visible=state.analysisMarkersVisible end
        return
    end
    local buckets={{},{},{},{},{},{}}
    for _,vertex in ipairs(state.analysis.vertices) do
        local index=math.min(6,math.floor((vertex.targetWeight or 0)*6)+1)
        buckets[index][#buckets[index]+1]=vertex
    end
    for index,vertices in ipairs(buckets) do
        local color=heatmapColors[index]
        local marker=buildVertexMarkers(vertices,color[1],color[2],color[3],extent)
        if marker then
            marker.visible=state.analysisMarkersVisible
            state.heatmapLines[#state.heatmapLines+1]=marker
        end
    end
end

local function buildPaintGeometryCache()
    if not state.meshD then return nil end
    local okMode,mode=safeCall(function() return state.meshD:getModeDraw() end)
    if not okMode or mode~='TRIANGLES' then return nil end
    local cache={vertices={},triangles={}}
    local function addTriangle(a,b,c,subset)
        local triangle={a=a,b=b,c=c,subset=subset}
        local ap,bp,cp=a.point,b.point,c.point
        triangle.minX=math.min(ap.x,bp.x,cp.x); triangle.maxX=math.max(ap.x,bp.x,cp.x)
        triangle.minY=math.min(ap.y,bp.y,cp.y); triangle.maxY=math.max(ap.y,bp.y,cp.y)
        triangle.minZ=math.min(ap.z,bp.z,cp.z); triangle.maxZ=math.max(ap.z,bp.z,cp.z)
        triangle.cx=(ap.x+bp.x+cp.x)/3; triangle.cy=(ap.y+bp.y+cp.y)/3
        triangle.cz=(ap.z+bp.z+cp.z)/3
        cache.triangles[#cache.triangles+1]=triangle
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
                local entry={globalIndex=offset+vertex,subset=subset,point=p}
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
    if state.workspace~='paint' then return nil end
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

local function rebuildPaintCursor(hit)
    destroyObject(state.paint.cursor)
    state.paint.cursor=nil
    state.paint.cursorHit=hit
    if not hit or state.workspace~='paint' then return end
    local n=hit.normal
    local rx,ry,rz=math.abs(n.y)<0.9 and 0 or 1,math.abs(n.y)<0.9 and 1 or 0,0
    local tx,ty,tz=ry*n.z-rz*n.y,rz*n.x-rx*n.z,rx*n.y-ry*n.x
    local tangentLength=math.sqrt(tx*tx+ty*ty+tz*tz)
    if tangentLength<1e-9 then return end
    tx,ty,tz=tx/tangentLength,ty/tangentLength,tz/tangentLength
    local bx,by,bz=n.y*tz-n.z*ty,n.z*tx-n.x*tz,n.x*ty-n.y*tx
    local radius=state.paint.radius
    local coords,segments={},32
    for segment=0,segments-1 do
        local a0=segment*math.pi*2/segments
        local a1=(segment+1)*math.pi*2/segments
        local function point(angle)
            local cosine,sine=math.cos(angle)*radius,math.sin(angle)*radius
            return hit.point.x+tx*cosine+bx*sine+n.x*radius*0.002,
                hit.point.y+ty*cosine+by*sine+n.y*radius*0.002,
                hit.point.z+tz*cosine+bz*sine+n.z*radius*0.002
        end
        appendPoint(coords,point(a0)); appendPoint(coords,point(a1))
    end
    local cursor=line:new('3d',0,0,0)
    cursor:add(coords); cursor:setColor(1,1,1,1); cursor:setPos(0,0,0)
    cursor.alwaysOnTop=true
    state.paint.cursor=cursor
end

local function updatePaintCursorHover()
    if state.workspace~='paint' or not state.paint.cursorPendingX then return end
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
    if not state.meshD then return end
    local bones=getBones()
    local bone=bones[state.paint.boneIndex]
    if not bone then return end
    state.paint.boneId=bone.boneId
    state.boneIndex=state.paint.boneIndex
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return end
    local weights={}
    local distributionTotal,distributionMin,distributionMax=0,math.huge,0
    local distributionCounts={0,0,0,0}
    for _,vertex in pairs(cache.vertices) do
        if state.paint.visualizationMode==2 then
            local _,influences=vertexWeightForBone(vertex.globalIndex,'')
            local dominant,count=0,0
            for _,pair in ipairs(influences) do
                local name,weight=pair[1],tonumber(pair[2]) or 0
                if name and weight>0 then dominant=math.max(dominant,weight); count=count+1 end
            end
            weights[vertex.globalIndex]=math.max(0,math.min(1,(dominant-0.25)/0.75))
            distributionTotal=distributionTotal+dominant
            distributionMin=math.min(distributionMin,dominant)
            distributionMax=math.max(distributionMax,dominant)
            if count>=1 and count<=4 then distributionCounts[count]=distributionCounts[count]+1 end
        else
            weights[vertex.globalIndex]=vertexWeightForBone(vertex.globalIndex,bone.name)
        end
    end
    if state.paint.visualizationMode==2 then
        local total=#cache.vertices
        state.paint.distributionStats={minimum=total>0 and distributionMin or 0,
            maximum=distributionMax,average=total>0 and distributionTotal/total or 0,
            counts=distributionCounts,total=total}
    end
    local vertices,uvs,indices={},{},{}
    local useIndexed=#cache.vertices<=65535
    if useIndexed then
        for index=1,#cache.vertices do
            local entry=cache.vertices[index]
            appendPoint(vertices,entry.point.x,entry.point.y,entry.point.z)
            uvs[#uvs+1]=math.max(0,math.min(1,weights[entry.globalIndex] or 0))
            uvs[#uvs+1]=0.5
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
                uvs[#uvs+1]=0.5
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

local function analyzeSelection()
    if not state.meshD then return end
    state.abruptDiagnostics=nil
    destroyObject(state.abruptLines); state.abruptLines=nil
    destroyObject(state.boundaryLines); state.boundaryLines=nil
    local allVertices = collectVertices()
    local selected, bones = {}, getBones()
    if state.selectionMode == 1 then
        local b = state.aabb
        for _, v in ipairs(allVertices) do
            local alpha,region=transitionAlpha(v.point,b)
            if alpha then v.blendAlpha,v.region=alpha,region; selected[#selected+1]=v end
        end
    elseif state.selectionMode == 2 then
        for _, v in ipairs(allVertices) do
            if v.subset == state.subsetIndex then v.blendAlpha,v.region=1,'core'; selected[#selected+1] = v end
        end
    else
        local target = bones[state.boneIndex]
        if target then
            local targetSegment={a=target,b=findBone(bones,target.parentName) or target}
            local segments = nil
            if state.proximityNearestOnly then
                segments={}
                for _, bone in ipairs(bones) do
                    segments[bone.name] = {a=bone, b=findBone(bones, bone.parentName) or bone}
                end
            end
            local radiusSquared = state.proximityRadius * state.proximityRadius
            for _, v in ipairs(allVertices) do
                local targetDistance=pointSegmentDistanceSquared(v.point,targetSegment.a,targetSegment.b)
                local selectedByOwnership=true
                if segments then
                    local nearestName,nearestDistance
                    for name,segment in pairs(segments) do
                        local distance=pointSegmentDistanceSquared(v.point,segment.a,segment.b)
                        if not nearestDistance or distance<nearestDistance then
                            nearestName,nearestDistance=name,distance
                        end
                    end
                    selectedByOwnership=nearestName==target.name
                end
                if selectedByOwnership and targetDistance<=radiusSquared then
                    v.blendAlpha,v.region=1,'core'
                    selected[#selected+1] = v
                end
            end
        end
    end

    local missing, invalidSum, unknown, disallowed = 0, 0, 0, 0
    local heatmapInfluenced,heatmapMinPositive,heatmapMax=0,math.huge,0
    local known = {}
    for _, bone in ipairs(bones) do known[bone.name] = true end
    local analysisTarget=bones[state.analysisBoneIndex]
    for _, vertex in ipairs(selected) do
        local targetWeight,influences=vertexWeightForBone(vertex.globalIndex,
            analysisTarget and analysisTarget.name or '')
        vertex.targetWeight=targetWeight
        if targetWeight>1e-8 then
            heatmapInfluenced=heatmapInfluenced+1
            heatmapMinPositive=math.min(heatmapMinPositive,targetWeight)
            heatmapMax=math.max(heatmapMax,targetWeight)
        end
        local n1,w1=influences[1][1],influences[1][2]
        local n2,w2=influences[2][1],influences[2][2]
        local n3,w3=influences[3][1],influences[3][2]
        local n4,w4=influences[4][1],influences[4][2]
        local ok=#influences>0
        if not ok or not n1 then
            missing = missing + 1
        else
            local sum = 0
            for _, pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
                if pair[1] then
                    sum = sum + (pair[2] or 0)
                    if not known[pair[1]] then unknown = unknown + 1 end
                    if state.restrictBones and not state.allowedBones[pair[1]] then disallowed=disallowed+1 end
                end
            end
            if math.abs(sum - 1) > 0.001 then invalidSum = invalidSum + 1 end
        end
    end
    local core,shell={},{}
    for _,vertex in ipairs(selected) do
        if vertex.region=='shell' then shell[#shell+1]=vertex else core[#core+1]=vertex end
    end
    state.analysis = {vertices=selected, allVertices=allVertices, core=core, shell=shell, missing=missing,
        invalidSum=invalidSum, unknown=unknown, disallowed=disallowed, totalMesh=#allVertices,
        heatmapBoneName=analysisTarget and analysisTarget.name or nil,
        heatmapInfluenced=heatmapInfluenced,
        heatmapMinPositive=heatmapInfluenced>0 and heatmapMinPositive or 0,
        heatmapMax=heatmapMax}
    state.analysisDirty = false
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    rebuildAnalysisMarkers(core,shell,extent)
    setStatus(string.format(tLang.L('swl_analysis_complete_fmt'), #selected), false)
end

local function rebuildPreview(sourcePath)
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
    sourcePath=sourcePath or state.fileName
    if not sourcePath then return end
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    local separation=extent*0.65
    local function loadRuntimePreview(method,x)
        local preview=mesh:new('3d')
        if not preview:setSkeletalSkinningMethod(method) then preview:destroy(); return nil end
        if not preview:load(sourcePath) then preview:destroy(); return nil end
        preview:setPos(x,0,0)
        preview.visible=state.meshVisible
        return preview
    end
    if playback.poseStress then
        state.preview=loadRuntimePreview('lbs',-separation)
        state.comparisonPreview=loadRuntimePreview('dqs',separation)
        playback.comparisonReady=state.preview~=nil and state.comparisonPreview~=nil
    else
        local method=playback.method==1 and 'auto' or playback.method==3 and 'dqs' or 'lbs'
        state.preview=loadRuntimePreview(method,0)
    end
    if state.preview then
        local total=state.preview:getTotalSkeletalAnimations()
        for index=1,total do
            playback.clips[index]=state.preview:getSkeletalAnimationName(index) or ('Clip '..index)
        end
    end
    applyWorkspaceVisibility()
end

local function rebuildRuntimePreviewFromMemory()
    if not state.meshD then return false end
    local temporaryPath=os.tmpname()..'.msh'
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
    end
end

local function syncPoseStressPreview()
    local playback=state.skeletalPreview
    if state.workspace~='runtime' or not playback.poseStress or not playback.playing or
            not playback.comparisonReady or
            not state.preview or not state.comparisonPreview then return end
    local time=state.preview:getSkeletalAnimationTime()
    if time then state.comparisonPreview:seekSkeletalAnimation(time) end
end

local function framePoseStressLayout()
    if not state.meshBounds then return end
    if not state.skeletalPreview.poseStress then frameCamera(state.meshBounds); return end
    local bounds={}
    for key,value in pairs(state.meshBounds) do bounds[key]=value end
    local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
    local separation=extent*0.65
    bounds.minX=bounds.minX-separation
    bounds.maxX=bounds.maxX+separation
    frameCamera(bounds)
end

local function showSkeletalPreviewControls()
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
    local poseStress=tImGui.Checkbox(tLang.L('swl_pose_stress_compare'),playback.poseStress)
    if poseStress~=playback.poseStress then
        playback.poseStress=poseStress
        if state.runtimePreviewFromMemory then rebuildRuntimePreviewFromMemory()
        else rebuildPreview() end
        framePoseStressLayout()
    end
    if playback.poseStress then
        tImGui.TextWrapped(tLang.L('swl_pose_stress_layout'))
    end
    local methods={tLang.L('swl_skinning_auto'),tLang.L('swl_skinning_lbs'),tLang.L('swl_skinning_dqs')}
    tImGui.BeginDisabled(playback.poseStress)
    tImGui.PushItemWidth(190)
    local methodChanged,method=tImGui.Combo(tLang.L('swl_skinning_method'),playback.method,methods,-1)
    tImGui.PopItemWidth()
    tImGui.EndDisabled()
    if methodChanged then
        playback.method=method
        if state.runtimePreviewFromMemory then rebuildRuntimePreviewFromMemory()
        else rebuildPreview() end
    end
    if not state.preview then
        tImGui.TextDisabled(tLang.L('swl_runtime_preview_unavailable'))
        return
    end
    local lbsReport=state.preview:getSkeletalSkinningReport()
    tImGui.Text(string.format(tLang.L('swl_lbs_report_fmt'),
        (lbsReport.requestedMethod or 'unknown'):upper(),
        (lbsReport.resolvedMethod or 'unknown'):upper(),lbsReport.status or 'unknown'))
    tImGui.TextDisabled(string.format(tLang.L('swl_skinning_reason_fmt'),
        lbsReport.resolutionReason or 'unknown'))
    tImGui.TextWrapped(string.format(tLang.L('swl_lbs_capacity_fmt'),
        lbsReport.requiredBoneCount or 0,lbsReport.effectiveBoneCapacity or 0))
    tImGui.TextDisabled(tLang.L('swl_lbs_capacity_note'))
    if playback.poseStress and state.comparisonPreview then
        local dqsReport=state.comparisonPreview:getSkeletalSkinningReport()
        tImGui.Text(string.format(tLang.L('swl_pose_stress_reports'),lbsReport.status or 'unknown',
            dqsReport.status or 'unknown'))
        if playback.playing and not playback.comparisonReady then
            tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_pose_stress_clip_rejected'))
        end
    elseif playback.poseStress then
        tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_pose_stress_unavailable'))
    end
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
        end
    end
    tImGui.EndDisabled()
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
    local time=state.preview:getSkeletalAnimationTime() or 0
    tImGui.PushItemWidth(240)
    local seekChanged,seekTime=tImGui.SliderFloat(tLang.L('swl_preview_time'),time,
        0,math.max(playback.duration,0.0001),'%.3f s')
    tImGui.PopItemWidth()
    if seekChanged and playback.playing then
        state.preview:seekSkeletalAnimation(seekTime)
        if state.comparisonPreview then state.comparisonPreview:seekSkeletalAnimation(seekTime) end
    end
    tImGui.TextDisabled(tLang.L('swl_bind_gizmo_note'))
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
    clearRollback()
    clearSelectionVisuals()
    clearPaintVisuals()
    destroySkeletonVisuals()
    state.fileName, state.meshD = path, meshD
    state.info = meshDebug:getInfo(path)
    refreshBindReport()
    state.modified = false
    state.normalizeReport=nil
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
    state.animationKeyClipboard=nil
    state.animationBonePoseClipboard=nil
    state.animationSkeletonPoseClipboard=nil
    state.boneEditorPosition={x=0,y=0,z=0}
    state.boneEditorLength=1
    state.boneEditorSelectedIndex=nil
    state.boneEditorSelection=nil
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
    state.proximityBoneHighlight=false
    state.analysis = nil
    state.analysisDirty = true
    state.subsetIndex, state.boneIndex, state.analysisBoneIndex, state.targetBoneIndex = 1, 1, 1, 1
    state.allowedBones={}
    state.allowedBonesHighlight=false
    state.hoveredAllowedBone=nil
    state.topologyAdjacency=nil
    state.paint.geometry=nil
    state.paint.boneIndex=1
    state.paint.boneId=nil
    state.paint.heatmapDirty=true
    for _,bone in ipairs(getBones()) do state.allowedBones[bone.name]=true end
    state.meshBounds = bounds
    state.aabb = bounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    state.aabbDragSensitivity=math.max(extent*0.0025,0.0001)
    state.proximityRadius=math.max(extent*0.1,0.001)
    state.paint.radius=math.max(extent*0.05,0.001)
    state.proximityNearestOnly=false
    rebuildPreview()
    rebuildSkeletonVisuals()
    buildPaintGeometryCache()
    if state.workspace=='paint' then rebuildPaintHeatmap() end
    rebuildSelectionBox()
    applyWorkspaceVisibility()
    frameCamera(bounds)
    setStatus(string.format(tLang.L('swl_loaded_fmt'), shortName(path)), false)
    return true
end

local function stageRollbackSnapshot(descriptionKey)
    local path = os.tmpname() .. '.msh'
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

local function writeInfluences(globalIndex,influences)
    local a,b,c,d=influences[1],influences[2],influences[3],influences[4]
    return state.meshD:setSkeletalVertexWeight(globalIndex,
        a and a.name or nil,a and a.weight or 0,
        b and b.name or nil,b and b.weight or 0,
        c and c.name or nil,c and c.weight or 0,
        d and d.name or nil,d and d.weight or 0)
end

local function readInfluenceMap(globalIndex,respectRestriction)
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

local function queryPaintVertices(point,radius)
    local cache=state.paint.geometry
    if not cache or not cache.vertexBvh then return {} end
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
                if distanceSquared<=radiusSquared then
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

local function stampPaintStroke(stroke,point)
    for _,candidate in ipairs(queryPaintVertices(point,state.paint.radius)) do
        local alpha=math.max(0,math.min(1,state.paint.strength*
            paintFalloff(candidate.distance,state.paint.radius)))
        if alpha>0 then
            local index=candidate.vertex.globalIndex
            local previous=stroke.alphas[index] or 0
            stroke.alphas[index]=1-(1-previous)*(1-alpha)
        end
    end
end

local function extendPaintStroke(hit)
    local stroke=state.paint.stroke
    if not stroke or not hit then return end
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
                stampPaintStroke(stroke,{x=previous.x+dx*t,y=previous.y+dy*t,z=previous.z+dz*t})
                sampleDistance=sampleDistance+spacing
            end
            stroke.distanceSinceSample=(carried+distance)%spacing
        end
    else
        stampPaintStroke(stroke,point)
        stroke.distanceSinceSample=0
    end
    stroke.lastPoint={x=point.x,y=point.y,z=point.z}
end

local function beginPaintStroke(hit)
    local bone=getBones()[state.paint.boneIndex]
    if not bone or not hit then return false end
    local snapshot=stageRollbackSnapshot('swl_history_paint_add')
    if not snapshot then setStatus(tLang.L('swl_snapshot_failed'),true); return false end
    state.paint.stroke={snapshot=snapshot,boneName=bone.name,boneId=bone.boneId,
        operationMode=state.paint.operationMode,smoothIterations=state.paint.smoothIterations,
        alphas={},lastPoint=nil,distanceSinceSample=0}
    extendPaintStroke(hit)
    rebuildPaintCursor(hit)
    return true
end

local function cancelPaintStroke()
    local stroke=state.paint.stroke
    if not stroke then return false end
    discardRollbackSnapshot(stroke.snapshot)
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
    if #indices==0 then
        discardRollbackSnapshot(stroke.snapshot)
        return false
    end
    local edits={}
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
        local noChangeKey=stroke.operationMode==3 and 'swl_paint_smooth_no_change' or
            stroke.operationMode==2 and 'swl_paint_subtract_no_change' or
            'swl_paint_add_no_change'
        setStatus(tLang.L(noChangeKey),false)
        return false
    end
    local ok,committed=safeCall(function()
        return state.meshD:setSkeletalVertexWeightsBatch(edits)
    end)
    if not ok or not committed then
        discardRollbackSnapshot(stroke.snapshot)
        if ok then setStatus(tLang.L('swl_paint_stroke_failed'),true) end
        return false
    end
    local historyKey=stroke.operationMode==3 and 'swl_history_paint_smooth' or
        stroke.operationMode==2 and 'swl_history_paint_subtract' or 'swl_history_paint_add'
    commitRollbackSnapshot(stroke.snapshot,historyKey)
    state.modified=true
    invalidateAnalysis()
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    applyWorkspaceVisibility()
    local statusKey=stroke.operationMode==3 and 'swl_paint_smooth_applied_fmt' or
        stroke.operationMode==2 and 'swl_paint_subtract_applied_fmt' or
        'swl_paint_stroke_applied_fmt'
    setStatus(string.format(tLang.L(statusKey),#edits,stroke.boneName),false)
    return true
end

local function cleanPaintWeakInfluences()
    local cache=state.paint.geometry or buildPaintGeometryCache()
    if not cache then return false end
    local indices={}
    for index in pairs(cache.vertices) do indices[#indices+1]=index end
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
    invalidateAnalysis()
    state.paint.heatmapDirty=true
    rebuildPaintHeatmap()
    applyWorkspaceVisibility()
    setStatus(string.format(tLang.L('swl_paint_clean_applied_fmt'),#edits,
        state.paint.cleanThreshold),false)
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

local function influenceDistance(a,b)
    local names={}
    for name in pairs(a) do names[name]=true end
    for name in pairs(b) do names[name]=true end
    local total=0
    for name in pairs(names) do total=total+math.abs((a[name] or 0)-(b[name] or 0)) end
    return math.min(1,total*0.5)
end

local function diagnoseAbruptTransitions()
    if not state.analysis or state.analysisDirty then return end
    local okMode,mode=safeCall(function() return state.meshD:getModeDraw() end)
    if not okMode or mode~='TRIANGLES' then
        setStatus(tLang.L('swl_diagnostic_requires_triangles'),true); return
    end
    local adjacency=buildTopologyAdjacency()
    local selected,points={},{}
    for _,vertex in ipairs(state.analysis.allVertices or {}) do points[vertex.globalIndex]=vertex end
    for _,vertex in ipairs(state.analysis.vertices) do
        selected[vertex.globalIndex]=true; points[vertex.globalIndex]=vertex
    end
    local weights={}
    local function weightsFor(index)
        if not weights[index] then weights[index]=readInfluenceMap(index,false) end
        return weights[index]
    end
    local affected,abruptEdges,maxDistance={},0,0
    local boundaryAffected,boundaryInside,boundaryExternal,boundaryEdges,boundaryMaxDistance={},{},{},{},0
    for index in pairs(selected) do
        for neighbor in pairs(adjacency[index] or {}) do
            if index<neighbor and selected[neighbor] then
                local distance=influenceDistance(weightsFor(index),weightsFor(neighbor))
                maxDistance=math.max(maxDistance,distance)
                if distance>=state.abruptThreshold then
                    abruptEdges=abruptEdges+1
                    affected[index]=true; affected[neighbor]=true
                end
            elseif not selected[neighbor] then
                local distance=influenceDistance(weightsFor(index),weightsFor(neighbor))
                boundaryMaxDistance=math.max(boundaryMaxDistance,distance)
                if distance>=state.abruptThreshold and points[index] and points[neighbor] then
                    boundaryEdges[#boundaryEdges+1]={points[index],points[neighbor]}
                    boundaryAffected[index]=true; boundaryAffected[neighbor]=true
                    boundaryInside[index]=true
                    boundaryExternal[neighbor]=true
                end
            end
        end
    end
    local vertices={}
    for index in pairs(affected) do vertices[#vertices+1]=points[index] end
    local boundaryVertices,boundaryInsideVertices,boundaryExternalVertices={},{},{}
    for index in pairs(boundaryAffected) do boundaryVertices[#boundaryVertices+1]=points[index] end
    for index in pairs(boundaryInside) do boundaryInsideVertices[#boundaryInsideVertices+1]=points[index] end
    for index in pairs(boundaryExternal) do boundaryExternalVertices[#boundaryExternalVertices+1]=points[index] end
    state.abruptDiagnostics={edges=abruptEdges,vertices=#vertices,maxDistance=maxDistance,
        affectedVertices=vertices,boundaryEdges=#boundaryEdges,boundaryVertices=#boundaryVertices,
        boundaryInsideVertices=boundaryInsideVertices,boundaryExternalVertices=boundaryExternalVertices,
        boundaryMaxDistance=boundaryMaxDistance}
    destroyObject(state.abruptLines)
    destroyObject(state.boundaryLines)
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    state.abruptLines=buildVertexMarkers(vertices,1,0,1,extent)
    if state.abruptLines then state.abruptLines.visible=state.abruptMarkersVisible end
    state.boundaryLines=buildEdgeLines(boundaryEdges,1,0.5,0)
    if state.boundaryLines then state.boundaryLines.visible=state.boundaryMarkersVisible end
    setStatus(string.format(tLang.L('swl_abrupt_complete_fmt'),abruptEdges,#vertices,
        #boundaryEdges,#boundaryVertices),false)
    return state.abruptDiagnostics
end

local function smoothVertices(vertices)
    local okMode,mode=safeCall(function() return state.meshD:getModeDraw() end)
    if not okMode or mode~='TRIANGLES' then
        setStatus(tLang.L('swl_smoothing_requires_triangles'),true)
        return nil
    end
    if #vertices==0 then return nil end
    local adjacency=buildTopologyAdjacency()
    if not snapshotForRollback('swl_history_smooth_weights') then
        setStatus(tLang.L('swl_snapshot_failed'),true)
        return nil
    end
    local editable={}
    for _,vertex in ipairs(vertices) do editable[vertex.globalIndex]=true end
    local needed={}
    for index in pairs(editable) do
        needed[index]=true
        for neighbor in pairs(adjacency[index] or {}) do needed[neighbor]=true end
    end
    local current={}
    for index in pairs(needed) do current[index]=readInfluenceMap(index,true) end
    local strength=math.max(0,math.min(1,state.smoothStrength))
    for _=1,state.smoothIterations do
        local nextWeights={}
        for index in pairs(editable) do
            local neighbors=adjacency[index] or {}
            local average,count={},0
            for neighbor in pairs(neighbors) do
                count=count+1
                for name,weight in pairs(current[neighbor] or {}) do
                    average[name]=(average[name] or 0)+weight
                end
            end
            local mixed={}
            for name,weight in pairs(current[index] or {}) do mixed[name]=weight*(1-strength) end
            if count>0 then
                for name,weight in pairs(average) do
                    mixed[name]=(mixed[name] or 0)+weight/count*strength
                end
            else
                mixed=current[index] or {}
            end
            nextWeights[index]=mixed
        end
        for index,weights in pairs(nextWeights) do current[index]=weights end
    end
    local applied,skipped=0,0
    for index in pairs(editable) do
        local influences=normalizedInfluences(current[index] or {})
        if #influences>0 then
            local ok,result=safeCall(function() return writeInfluences(index,influences) end)
            if ok and result then applied=applied+1 else skipped=skipped+1 end
        else
            -- Smoothing has no destination bone. If an allowed-bone restriction removes every
            -- effective influence, preserve the vertex instead of silently assigning a rigid target.
            skipped=skipped+1
        end
    end
    state.modified=state.modified or applied>0
    return applied,skipped
end

local function applyLocalSmoothing()
    if not state.analysis or state.analysisDirty then return end
    local vertices=#state.analysis.shell>0 and state.analysis.shell or state.analysis.vertices
    local applied,skipped=smoothVertices(vertices)
    if applied==nil then return end
    invalidateAnalysis()
    setStatus(string.format(tLang.L('swl_smoothed_fmt'),applied,state.smoothIterations,skipped),applied==0)
end

local function readRawWeightRecord(globalIndex)
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    if not ok then return nil end
    return {n1=n1,w1=w1,n2=n2,w2=w2,n3=n3,w3=w3,n4=n4,w4=w4}
end

local function sameRawWeightRecord(a,b)
    if not a or not b then return false end
    local function sameValue(x,y)
        return x==y or (type(x)=='number' and type(y)=='number' and x~=x and y~=y)
    end
    for _,field in ipairs({'n1','w1','n2','w2','n3','w3','n4','w4'}) do
        if not sameValue(a[field],b[field]) then return false end
    end
    return true
end

local function applyAbruptTransitionSmoothing()
    local before=state.abruptDiagnostics
    if not before or not before.affectedVertices or #before.affectedVertices==0 then return end
    local externalBefore,auditFailures={},0
    for _,vertex in ipairs(before.boundaryExternalVertices or {}) do
        local record=readRawWeightRecord(vertex.globalIndex)
        if record then externalBefore[vertex.globalIndex]=record else auditFailures=auditFailures+1 end
    end
    local applied,skipped=smoothVertices(before.affectedVertices)
    if applied==nil then return end

    local externalVerified,externalModified=0,0
    for index,record in pairs(externalBefore) do
        local afterRecord=readRawWeightRecord(index)
        if afterRecord then
            externalVerified=externalVerified+1
            if not sameRawWeightRecord(record,afterRecord) then externalModified=externalModified+1 end
        else
            auditFailures=auditFailures+1
        end
    end

    -- Rebuild both layers from the edited weights so the magenta markers and summary immediately
    -- describe the result, not the pre-operation diagnosis.
    analyzeSelection()
    local after=diagnoseAbruptTransitions()
    if not after then return end
    setStatus(string.format(tLang.L('swl_abrupt_smoothed_fmt'),applied,state.smoothIterations,
        before.edges,after.edges,before.vertices,after.vertices,skipped,
        externalVerified,externalModified,auditFailures),
        applied==0 or externalModified>0 or auditFailures>0)
end

local function applyNormalizeAndLimit()
    if not state.analysis or state.analysisDirty or #state.analysis.vertices==0 then return end
    local jobs,unchanged,skipped,readFailed={},0,0,0
    for _,vertex in ipairs(state.analysis.vertices) do
        local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
            return state.meshD:getSkeletalVertexWeight(vertex.globalIndex)
        end)
        if not ok then
            readFailed=readFailed+1
        else
            local weightMap,seen,sum,effective,needsCleanup={}, {},0,0,false
            for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
                local name,weight=pair[1],tonumber(pair[2]) or 0
                if name then
                    if weight<=0 or weight~=weight or weight==math.huge or weight==-math.huge then
                        needsCleanup=true
                    else
                        if seen[name] then needsCleanup=true end
                        seen[name]=true
                        effective=effective+1
                        sum=sum+weight
                        weightMap[name]=(weightMap[name] or 0)+weight
                    end
                end
            end
            local influences=normalizedInfluences(weightMap)
            if #influences==0 then
                skipped=skipped+1
            elseif needsCleanup or effective>4 or math.abs(sum-1)>0.001 then
                jobs[#jobs+1]={index=vertex.globalIndex,influences=influences}
            else
                unchanged=unchanged+1
            end
        end
    end
    if #jobs>0 and not snapshotForRollback('swl_history_normalize_weights') then
        setStatus(tLang.L('swl_snapshot_failed'),true)
        return
    end
    local applied,writeFailed=0,0
    for _,job in ipairs(jobs) do
        local ok,result=safeCall(function() return writeInfluences(job.index,job.influences) end)
        if ok and result then applied=applied+1 else writeFailed=writeFailed+1 end
    end
    local failed=readFailed+writeFailed
    state.modified=state.modified or applied>0
    state.normalizeReport={total=#state.analysis.vertices,corrected=applied,unchanged=unchanged,
        skipped=skipped,failed=failed}
    if applied>0 then invalidateAnalysis() end
    setStatus(string.format(tLang.L('swl_normalized_fmt'),applied,unchanged,skipped,failed),failed>0)
end

local function applyRigidBind()
    if not state.analysis or state.analysisDirty or #state.analysis.vertices == 0 then return end
    local bones = getBones()
    local target = bones[state.targetBoneIndex]
    if not target then return end
    if not snapshotForRollback('swl_history_apply_weights') then
        setStatus(tLang.L('swl_snapshot_failed'), true)
        return
    end
    local applied = 0
    local hasTransition = #state.analysis.shell > 0
    for _, vertex in ipairs(state.analysis.vertices) do
        local influences=blendedInfluences(vertex.globalIndex,target.name,vertex.blendAlpha or 1)
        local ok, result = safeCall(function()
            return writeInfluences(vertex.globalIndex,influences)
        end)
        if ok and result then applied = applied + 1 end
    end
    state.modified = applied > 0
    invalidateAnalysis()
    local statusKey = hasTransition and 'swl_applied_transition_fmt' or 'swl_applied_fmt'
    setStatus(string.format(tLang.L(statusKey), applied, target.name), applied == 0)
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
    state.workspace=entry.workspace or state.workspace
    state.boneIndex=entry.boneIndex or state.boneIndex
    state.animationClipSelected=entry.clipIndex or state.animationClipSelected
    state.authoringTime=entry.authoringTime or 0
    state.normalizeReport=nil
    state.bindRenameBoneId=nil
    state.bindReparentBoneId=nil
    state.bindEditBoneId=nil
    refreshBindReport()
    local bones=getBones()
    state.boneIndex=math.max(1,math.min(state.boneIndex,#bones))
    if state.workspace=='paint' then state.paint.boneIndex=state.boneIndex end
    state.allowedBones={}
    for _,bone in ipairs(bones) do state.allowedBones[bone.name]=true end
    invalidateAnalysis()
    rebuildPreview(entry.path)
    buildPaintGeometryCache()
    if state.workspace=='paint' then rebuildPaintHeatmap() end
    rebuildSkeletonVisuals()
    rebuildSelectionBox()
    rebuildProximityCapsule()
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
        tLang.renderLanguageSubmenu()
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

local function showItemTooltip(text,allowWhenDisabled)
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

local function showSelectionInputs()
    local labels = {tLang.L('swl_selection_aabb'), tLang.L('swl_selection_subset'), tLang.L('swl_selection_bone')}
    tImGui.PushItemWidth(190)
    local changed, mode = tImGui.Combo(tLang.L('swl_selection_method'), state.selectionMode, labels, -1)
    showItemTooltip(tLang.L('swl_selection_method_tooltip'))
    tImGui.PopItemWidth()
    if changed then
        state.selectionMode = mode
        if mode ~= 3 and state.proximityBoneHighlight then
            state.proximityBoneHighlight = false
            rebuildProximityBoneHighlight()
        end
        invalidateAnalysis()
        rebuildSelectionBox()
        rebuildProximityCapsule()
    end
    if state.selectionMode == 1 and state.aabb then
        local b = state.aabb
        local aabbChanged = false
        local fields = {
            {'Min X', 'minX'}, {'Min Y', 'minY'}, {'Min Z', 'minZ'},
            {'Max X', 'maxX'}, {'Max Y', 'maxY'}, {'Max Z', 'maxZ'},
        }
        local reference = state.meshBounds
        local extent = reference and math.max(reference.maxX-reference.minX,
            reference.maxY-reference.minY, reference.maxZ-reference.minZ) or 1
        local automaticDragSpeed=math.max(extent*0.0025,0.0001)
        tImGui.PushItemWidth(150)
        local sensitivityChanged,sensitivity=tImGui.InputFloat(tLang.L('swl_aabb_drag_sensitivity'),
            state.aabbDragSensitivity,automaticDragSpeed*0.1,automaticDragSpeed,'%.6f',0)
        showItemTooltip(tLang.L('swl_aabb_drag_sensitivity_tooltip'))
        tImGui.PopItemWidth()
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_reset_auto')..'##swlAabbDragSensitivity') then
            state.aabbDragSensitivity=automaticDragSpeed
        elseif sensitivityChanged then
            state.aabbDragSensitivity=math.max(sensitivity,0.000001)
        end
        local dragSpeed=math.max(state.aabbDragSensitivity,0.000001)
        tImGui.PushItemWidth(150)
        for _, field in ipairs(fields) do
            local edited, value = tImGui.DragFloat(field[1], b[field[2]], dragSpeed, -1000000, 1000000, '%.4f')
            if edited then b[field[2]] = value; aabbChanged = true end
        end
        local sizeFields={
            {tLang.L('swl_size_x'),'minX','maxX'},
            {tLang.L('swl_size_y'),'minY','maxY'},
            {tLang.L('swl_size_z'),'minZ','maxZ'},
        }
        for _,field in ipairs(sizeFields) do
            local currentSize=math.max(0,b[field[3]]-b[field[2]])
            local edited,value=tImGui.DragFloat(field[1],currentSize,dragSpeed,0,2000000,'%.4f')
            showItemTooltip(tLang.L('swl_aabb_size_tooltip'))
            if edited then
                local center=(b[field[2]]+b[field[3]])*0.5
                local halfSize=math.max(0,value)*0.5
                b[field[2]],b[field[3]]=center-halfSize,center+halfSize
                aabbChanged=true
            end
        end
        tImGui.PopItemWidth()
        tImGui.Text(tLang.L('swl_transition_faces'))
        showItemTooltip(tLang.L('swl_transition_faces_tooltip'))
        local faceControls={
            {'-X','minX'},{'+X','maxX'},{'-Y','minY'},
            {'+Y','maxY'},{'-Z','minZ'},{'+Z','maxZ'},
        }
        for _,control in ipairs(faceControls) do
            local face=state.shellFaces[control[2]]
            local enabled=tImGui.Checkbox(control[1]..'##swlShellEnabled'..control[2],face.enabled)
            if enabled~=face.enabled then face.enabled=enabled; aabbChanged=true end
            tImGui.SameLine()
            tImGui.BeginDisabled(not face.enabled)
            tImGui.PushItemWidth(130)
            local changed,width=tImGui.DragFloat(tLang.L('swl_shell_width')..'##'..control[2],
                face.width,dragSpeed,0,math.max(extent*2,dragSpeed),'%.4f')
            tImGui.PopItemWidth()
            tImGui.EndDisabled()
            if changed then face.width=math.max(0,width); aabbChanged=true end
        end
        local falloffLabels={tLang.L('swl_falloff_linear'),tLang.L('swl_falloff_smooth')}
        tImGui.PushItemWidth(150)
        local falloffChanged,falloff=tImGui.Combo(tLang.L('swl_falloff'),state.falloffMode,falloffLabels,-1)
        tImGui.PopItemWidth()
        if falloffChanged then state.falloffMode=falloff; aabbChanged=true end
        if aabbChanged then
            if b.minX > b.maxX then b.minX,b.maxX=b.maxX,b.minX end
            if b.minY > b.maxY then b.minY,b.maxY=b.maxY,b.minY end
            if b.minZ > b.maxZ then b.minZ,b.maxZ=b.maxZ,b.minZ end
            invalidateAnalysis()
            rebuildSelectionBox()
        end
    elseif state.selectionMode == 2 then
        local ok, total = safeCall(function() return state.meshD:getTotalSubset(1) end)
        total = ok and total or 1
        tImGui.PushItemWidth(190)
        local edited, value = tImGui.SliderInt(tLang.L('swl_subset'), state.subsetIndex, 1, math.max(1,total))
        tImGui.PopItemWidth()
        if edited then state.subsetIndex=value; invalidateAnalysis() end
    else
        local bones, names = getBones(), {}
        for _, bone in ipairs(bones) do names[#names+1] = bone.name end
        if #names > 0 then
            tImGui.PushItemWidth(190)
            local edited, value = tImGui.Combo(tLang.L('swl_source_bone'), math.min(state.boneIndex,#names), names, -1)
            showItemTooltip(tLang.L('swl_source_bone_tooltip'))
            tImGui.PopItemWidth()
            tImGui.SameLine()
            local highlight=tImGui.Checkbox(tLang.L('swl_highlight')..'##swlProximityBoneHighlight',
                state.proximityBoneHighlight)
            showItemTooltip(tLang.L('swl_source_bone_highlight_tooltip'))
            if highlight~=state.proximityBoneHighlight then
                state.proximityBoneHighlight=highlight
                rebuildProximityBoneHighlight()
            end
            if edited then
                state.boneIndex=value
                invalidateAnalysis()
                rebuildProximityBoneHighlight()
                rebuildProximityCapsule()
            end
            local bounds=state.meshBounds
            local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                bounds.maxZ-bounds.minZ) or 1
            local dragSpeed=math.max(extent*0.0025,0.0001)
            tImGui.PushItemWidth(190)
            local radiusChanged,radius=tImGui.DragFloat(tLang.L('swl_proximity_radius'),
                state.proximityRadius,dragSpeed,0,math.max(extent*2,dragSpeed),'%.4f')
            showItemTooltip(tLang.L('swl_proximity_radius_tooltip'))
            tImGui.PopItemWidth()
            if radiusChanged then
                state.proximityRadius=math.max(0,radius)
                invalidateAnalysis()
                rebuildProximityCapsule()
            end
            local nearestOnly=tImGui.Checkbox(tLang.L('swl_proximity_nearest_only'),
                state.proximityNearestOnly)
            showItemTooltip(tLang.L('swl_proximity_nearest_only_tooltip'))
            if nearestOnly~=state.proximityNearestOnly then
                state.proximityNearestOnly=nearestOnly
                invalidateAnalysis()
            end
        else
            tImGui.TextDisabled(tLang.L('swl_no_bones'))
        end
    end
    local heatmap=tImGui.Checkbox(tLang.L('swl_heatmap'),state.heatmapEnabled)
    showItemTooltip(tLang.L('swl_heatmap_tooltip'))
    if heatmap~=state.heatmapEnabled then
        state.heatmapEnabled=heatmap
        if not heatmap and state.analysisBoneHighlight then
            state.analysisBoneHighlight=false
            rebuildAnalysisBoneHighlight()
        end
        if state.analysis then
            local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
                state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
            rebuildAnalysisMarkers(state.analysis.core,state.analysis.shell,extent)
        end
    end
    if state.heatmapEnabled then
        local bones, names = getBones(), {}
        for _, bone in ipairs(bones) do names[#names+1] = bone.name end
        if #names > 0 then
            state.analysisBoneIndex=math.min(state.analysisBoneIndex,#names)
            tImGui.PushItemWidth(190)
            local edited,value=tImGui.Combo(tLang.L('swl_analysis_bone'),state.analysisBoneIndex,names,-1)
            showItemTooltip(tLang.L('swl_analysis_bone_tooltip'))
            tImGui.PopItemWidth()
            tImGui.SameLine()
            local highlight=tImGui.Checkbox(tLang.L('swl_highlight')..'##swlAnalysisBoneHighlight',
                state.analysisBoneHighlight)
            if highlight~=state.analysisBoneHighlight then
                state.analysisBoneHighlight=highlight
                rebuildAnalysisBoneHighlight()
            end
            if edited then
                state.analysisBoneIndex=value
                invalidateAnalysis()
                rebuildAnalysisBoneHighlight()
            end
        end
        tImGui.TextDisabled(tLang.L('swl_heatmap_legend'))
    end
end

local function showStatusMessage()
    if not state.status then return end
    tImGui.Separator()
    if state.statusError then
        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=0.3,b=0.2,a=1})
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

local function refreshAllowedBoneDiagnostics()
    if not state.analysis then updateAllowedBoneColors(); return end
    local disallowed=0
    if state.restrictBones then
        for _,vertex in ipairs(state.analysis.vertices) do
            local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
                return state.meshD:getSkeletalVertexWeight(vertex.globalIndex)
            end)
            if ok then
                for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
                    if pair[1] and (tonumber(pair[2]) or 0)>0 and not state.allowedBones[pair[1]] then
                        disallowed=disallowed+1
                    end
                end
            end
        end
    end
    state.analysis.disallowed=disallowed
    updateAllowedBoneColors()
end

local function showBoneRestrictions(bones,showTargetOnly)
    local restrict=tImGui.Checkbox(tLang.L('swl_restrict_bones'),state.restrictBones)
    showItemTooltip(tLang.L('swl_restrict_bones_tooltip'))
    if restrict~=state.restrictBones then
        state.restrictBones=restrict
        refreshAllowedBoneDiagnostics()
    end
    if not state.restrictBones or #bones==0 then
        state.hoveredAllowedBone=nil
        updateAllowedBoneColors()
        return
    end
    local highlight=tImGui.Checkbox(tLang.L('swl_highlight_allowed_bones'),state.allowedBonesHighlight)
    showItemTooltip(tLang.L('swl_highlight_allowed_bones_tooltip'))
    if highlight~=state.allowedBonesHighlight then
        state.allowedBonesHighlight=highlight
        if not highlight then state.hoveredAllowedBone=nil end
        updateAllowedBoneColors()
    end
    if tImGui.Button(tLang.L('swl_allow_all')) then
        for _,bone in ipairs(bones) do state.allowedBones[bone.name]=true end
        refreshAllowedBoneDiagnostics()
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_clear_all')) then
        state.allowedBones={}
        refreshAllowedBoneDiagnostics()
    end
    if showTargetOnly then
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_target_only')) then
            state.allowedBones={}
            state.allowedBones[bones[state.targetBoneIndex].name]=true
            refreshAllowedBoneDiagnostics()
        end
    end
    local hoveredBone=nil
    tImGui.BeginChild('##swlAllowedBones',{x=300,y=115},true)
    for _,bone in ipairs(bones) do
        local wasAllowed=state.allowedBones[bone.name]==true
        local allowed=tImGui.Checkbox(bone.name..'##swlAllowed',wasAllowed)
        if allowed~=wasAllowed then
            state.allowedBones[bone.name]=allowed or nil
            refreshAllowedBoneDiagnostics()
        end
        if tImGui.IsItemHovered(0) then hoveredBone=bone.name end
    end
    tImGui.EndChild()
    if hoveredBone~=state.hoveredAllowedBone then
        state.hoveredAllowedBone=hoveredBone
        updateAllowedBoneColors()
    end
end

local function showSmoothingControls()
    tImGui.PushItemWidth(190)
    local strengthChanged,strength=tImGui.SliderFloat(tLang.L('swl_smooth_strength'),
        state.smoothStrength,0,1,'%.2f')
    showItemTooltip(tLang.L('swl_smooth_strength_tooltip'))
    if strengthChanged then state.smoothStrength=strength end
    local iterationsChanged,iterations=tImGui.SliderInt(tLang.L('swl_smooth_iterations'),
        state.smoothIterations,1,10)
    showItemTooltip(tLang.L('swl_smooth_iterations_tooltip'))
    if iterationsChanged then state.smoothIterations=iterations end
    tImGui.PopItemWidth()
end

local function showDiagnosticControls(canOperate,allowRepair)
    tImGui.PushItemWidth(190)
    local thresholdChanged,threshold=tImGui.SliderFloat(tLang.L('swl_abrupt_threshold'),
        state.abruptThreshold,0.05,1,'%.2f')
    showItemTooltip(tLang.L('swl_abrupt_threshold_tooltip'))
    tImGui.PopItemWidth()
    if thresholdChanged then
        state.abruptThreshold=threshold
        state.abruptDiagnostics=nil
        destroyObject(state.abruptLines); state.abruptLines=nil
        destroyObject(state.boundaryLines); state.boundaryLines=nil
    end
    tImGui.BeginDisabled(not canOperate)
    local diagnosePressed=tImGui.Button(tLang.L('swl_diagnose_transitions'))
    showItemTooltip(tLang.L('swl_diagnose_transitions_tooltip'))
    if diagnosePressed then diagnoseAbruptTransitions() end
    tImGui.EndDisabled()
    if state.abruptDiagnostics then
        local diagnostic=state.abruptDiagnostics
        tImGui.Text(string.format(tLang.L('swl_abrupt_result_fmt'),diagnostic.edges,
            diagnostic.vertices,diagnostic.maxDistance))
        tImGui.Text(string.format(tLang.L('swl_boundary_result_fmt'),diagnostic.boundaryEdges,
            diagnostic.boundaryVertices,diagnostic.boundaryMaxDistance))
        tImGui.TextDisabled(tLang.L('swl_abrupt_marker_hint'))
        if allowRepair then
            tImGui.BeginDisabled(diagnostic.vertices==0)
            local pressed=tImGui.Button(tLang.L('swl_smooth_abrupt_transitions'))
            showItemTooltip(tLang.L('swl_smooth_abrupt_transitions_tooltip'))
            if pressed then applyAbruptTransitionSmoothing() end
            tImGui.EndDisabled()
        end
    end
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
                invalidateAnalysis()
                rebuildPreview()
                rebuildSkeletonVisuals()
                setWorkspace('weights')
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

    local visited={}
    local function showNode(index)
        if visited[index] then return end
        visited[index]=true
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
    -- Defensive display for malformed snapshots; canonical validation should make this empty.
    for index=1,#bones do if not visited[index] then showNode(index) end end
    state.bindTreeOpenAll=false
end

local function showBindPoseDiagnostics()
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

local function showWeightLabSkeletonControls()
    local skeletonVisible=tImGui.Checkbox(tLang.L('swl_show_skeleton'),state.skeletonVisible)
    if skeletonVisible~=state.skeletonVisible then
        state.skeletonVisible=skeletonVisible
        applyWorkspaceVisibility()
    end
    tImGui.SameLine()
    tImGui.BeginDisabled(not state.skeletonVisible)
    local skeletonAlwaysOnTop=tImGui.Checkbox(tLang.L('swl_skeleton_always_on_top'),
        state.skeletonAlwaysOnTop)
    tImGui.EndDisabled()
    if skeletonAlwaysOnTop~=state.skeletonAlwaysOnTop then
        state.skeletonAlwaysOnTop=skeletonAlwaysOnTop
        for _,object in pairs(state.skeletonGizmo.spheres) do
            object.alwaysOnTop=skeletonAlwaysOnTop
        end
        for _,object in pairs(state.skeletonGizmo.bones) do
            object.alwaysOnTop=skeletonAlwaysOnTop
        end
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
    tImGui.TextWrapped(tLang.L(state.paint.operationMode==3 and 'swl_paint_smooth_help' or
        state.paint.operationMode==2 and 'swl_paint_subtract_help' or 'swl_paint_add_help'))
    local showSkeleton=tImGui.Checkbox(tLang.L('swl_show_skeleton'),state.paint.showSkeleton)
    if showSkeleton~=state.paint.showSkeleton then
        state.paint.showSkeleton=showSkeleton
        applyWorkspaceVisibility()
    end
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
    tImGui.Text(tLang.L('swl_paint_operation'))
    state.paint.operationMode=tImGui.RadioButton(tLang.L('swl_paint_operation_add'),
        state.paint.operationMode,1)
    state.paint.operationMode=tImGui.RadioButton(tLang.L('swl_paint_operation_subtract'),
        state.paint.operationMode,2)
    state.paint.operationMode=tImGui.RadioButton(tLang.L('swl_paint_operation_smooth'),
        state.paint.operationMode,3)
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    tImGui.PushItemWidth(240)
    local radiusChanged,radius=tImGui.SliderFloat(tLang.L('swl_paint_brush_radius'),
        state.paint.radius,math.max(extent*0.002,0.0001),math.max(extent*0.5,0.001),'%.4g')
    tImGui.PopItemWidth()
    if radiusChanged then
        state.paint.radius=radius
        rebuildPaintCursor(state.paint.cursorHit)
    end
    tImGui.PushItemWidth(240)
    local strengthChanged,strength=tImGui.SliderFloat(tLang.L('swl_paint_brush_strength'),
        state.paint.strength,0.01,1,'%.2f')
    tImGui.PopItemWidth()
    if strengthChanged then state.paint.strength=strength end
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
    if falloffChanged then state.paint.falloffMode=falloffMode end
    tImGui.TextDisabled(tLang.L('swl_heatmap_legend'))
    local geometry=state.paint.geometry
    if geometry then
        tImGui.Text(string.format(tLang.L('swl_paint_geometry_fmt'),
            #geometry.vertices,#geometry.triangles))
        if #state.paint.heatmapLines>0 then
            tImGui.TextDisabled(tLang.L(state.paint.heatmapIndexed and
                'swl_paint_indexed_heatmap' or 'swl_paint_nonindexed_heatmap'))
        end
    end
    if state.paint.cursorHit then
        local p=state.paint.cursorHit.point
        tImGui.Text(string.format(tLang.L('swl_paint_hit_fmt'),p.x,p.y,p.z,
            state.paint.cursorHit.triangle.subset))
    else
        tImGui.TextDisabled(tLang.L('swl_paint_no_hit'))
    end
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_paint_weight_tools'))
    tImGui.TextWrapped(tLang.L('swl_paint_clean_help'))
    tImGui.PushItemWidth(240)
    local thresholdChanged,cleanThreshold=tImGui.SliderFloat(
        tLang.L('swl_paint_clean_threshold'),state.paint.cleanThreshold,0.0001,0.25,'%.4f')
    tImGui.PopItemWidth()
    if thresholdChanged then state.paint.cleanThreshold=cleanThreshold end
    if tImGui.Button(tLang.L('swl_paint_clean_apply')) then cleanPaintWeakInfluences() end
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_paint_repair_diagnostics'))
    local showDistribution=tImGui.Checkbox(tLang.L('swl_paint_show_distribution'),
        state.paint.visualizationMode==2)
    local visualizationMode=showDistribution and 2 or 1
    if visualizationMode~=state.paint.visualizationMode then
        state.paint.visualizationMode=visualizationMode
        state.paint.heatmapDirty=true
        rebuildPaintHeatmap()
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
            local temporaryPath=os.tmpname()..'.msh'
            local okSaved,saved=safeCall(function()
                return state.meshD:save(temporaryPath,false,false)
            end)
            if okSaved and saved then
                state.skeletalPreview.method=authoringMethod
                state.skeletalPreview.poseStress=false
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
        tImGui.SameLine()
        state.animationTransformTool=tImGui.RadioButton(
            tLang.L('swl_animation_tool_rotate')..'##swlAnimationRotate',
            state.animationTransformTool,2)
        tImGui.SameLine()
        tImGui.BeginDisabled(resolvedAuthoringMethod~='lbs')
        state.animationTransformTool=tImGui.RadioButton(
            tLang.L('swl_animation_tool_scale')..'##swlAnimationScale',
            state.animationTransformTool,3)
        tImGui.EndDisabled()
        if state.animationTransformTool==3 and resolvedAuthoringMethod~='lbs' then
            state.animationTransformTool=1
        end
        if previousTransformTool~=state.animationTransformTool then
            state.translationGizmo.drag=nil; state.rotationGizmo.drag=nil
            state.scaleGizmo.drag=nil
            rebuildTranslationGizmo(); rebuildRotationGizmo(); rebuildScaleGizmo()
        end
        if state.animationTransformTool==3 then
            tImGui.TextWrapped(tLang.L('swl_animation_scale_uniform_only'))
        elseif resolvedAuthoringMethod~='lbs' then
            tImGui.TextDisabled(tLang.L('swl_animation_scale_requires_lbs'))
        end
        state.animationAutoKey=tImGui.Checkbox(
            tLang.L('swl_animation_auto_key')..'##swlAnimationAutoKey',state.animationAutoKey)
        tImGui.TextDisabled(tLang.L('swl_animation_auto_key_help'))
        tImGui.TextDisabled(tLang.L('swl_animation_viewport_select_help'))
        if state.authoringOverride then
            tImGui.TextColored({r=1,g=0.75,b=0.15,a=1},
                tLang.L('swl_animation_temporary_pose'))
            local rotationOverride=state.authoringOverride.channelMask==2
            local scaleOverride=state.authoringOverride.channelMask==4
            if tImGui.Button(tLang.L(scaleOverride and
                    'swl_animation_commit_scale_key' or rotationOverride and
                    'swl_animation_commit_rotation_key' or
                    'swl_animation_commit_translation_key')..'##swlCommitAuthoringKey') then
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

local function nextSimpleBoneName()
    local used={}
    for _,bone in ipairs(getBones()) do used[bone.name]=true end
    local index=1
    while used['Bone_'..index] do index=index+1 end
    return 'Bone_'..index
end

local function showBoneEditor()
    local previousRemovePreview=state.boneEditorRemovePreviewIndex
    state.boneEditorRemovePreviewIndex=nil
    tImGui.TextWrapped(tLang.L('swl_bone_editor_help'))
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
        tImGui.TextDisabled(string.format(tLang.L(inheritedExtensionLength and
            'swl_bone_editor_inherited_length' or 'swl_bone_editor_configured_length'),
            extensionLength or 0))
    end
    tImGui.TextDisabled(tLang.L('swl_bone_editor_root_note'))
    if state.boneEditorSelection then
        local selectionKey=state.boneEditorSelection.kind=='segment' and
            'swl_bone_editor_selected_segment' or state.boneEditorSelection.kind=='joint' and
            'swl_bone_editor_selected_joint' or state.boneEditorSelection.kind=='tail' and
            'swl_bone_editor_selected_tail' or 'swl_bone_editor_selected_head'
        tImGui.TextWrapped(string.format(tLang.L(selectionKey),state.boneEditorSelection.boneName))
        local selectedBone=getBones()[state.boneEditorSelection.boneIndex]
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
                local headPoint,tailPoint=getBoneEditorEndpoints(selectedBone,1)
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
            showSharedVisualization()
            tImGui.Separator()
            tImGui.Text(tLang.L('swl_workspaces'))
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
            if openWorkspaceNode('weights',tLang.L('swl_weight_lab_workspace'),
                    '##swlWeightLabWorkspace') then
            showSectionTitle('swl_visualization')
            showWeightLabSkeletonControls()
            local markersAlwaysOnTop=tImGui.Checkbox(tLang.L('swl_markers_always_on_top'),
                state.markersAlwaysOnTop)
            if markersAlwaysOnTop~=state.markersAlwaysOnTop then
                state.markersAlwaysOnTop=markersAlwaysOnTop
                if state.selectionLines then state.selectionLines.alwaysOnTop=markersAlwaysOnTop end
                if state.transitionLines then state.transitionLines.alwaysOnTop=markersAlwaysOnTop end
                for _,marker in ipairs(state.heatmapLines) do marker.alwaysOnTop=markersAlwaysOnTop end
                if state.abruptLines then state.abruptLines.alwaysOnTop=markersAlwaysOnTop end
                if state.boundaryLines then state.boundaryLines.alwaysOnTop=markersAlwaysOnTop end
            end
            local hasAnalysisMarkers=state.selectionLines~=nil or state.transitionLines~=nil or
                #state.heatmapLines>0
            tImGui.BeginDisabled(not hasAnalysisMarkers)
            local analysisMarkersVisible=tImGui.Checkbox(tLang.L('swl_show_analysis_markers'),
                state.analysisMarkersVisible)
            if analysisMarkersVisible~=state.analysisMarkersVisible then
                state.analysisMarkersVisible=analysisMarkersVisible
                if state.selectionLines then state.selectionLines.visible=analysisMarkersVisible end
                if state.transitionLines then state.transitionLines.visible=analysisMarkersVisible end
                for _,marker in ipairs(state.heatmapLines) do marker.visible=analysisMarkersVisible end
            end
            tImGui.EndDisabled()
            tImGui.BeginDisabled(state.abruptLines==nil)
            local abruptMarkersVisible=tImGui.Checkbox(tLang.L('swl_show_abrupt_markers'),
                state.abruptMarkersVisible)
            if abruptMarkersVisible~=state.abruptMarkersVisible then
                state.abruptMarkersVisible=abruptMarkersVisible
                if state.abruptLines then state.abruptLines.visible=abruptMarkersVisible end
            end
            tImGui.EndDisabled()
            tImGui.BeginDisabled(state.boundaryLines==nil)
            local boundaryMarkersVisible=tImGui.Checkbox(tLang.L('swl_show_boundary_markers'),
                state.boundaryMarkersVisible)
            if boundaryMarkersVisible~=state.boundaryMarkersVisible then
                state.boundaryMarkersVisible=boundaryMarkersVisible
                if state.boundaryLines then state.boundaryLines.visible=boundaryMarkersVisible end
            end
            tImGui.EndDisabled()
            tImGui.Separator()
            showSectionTitle('swl_selection_analysis')
            showSelectionInputs()
            if tImGui.Button(tLang.L('swl_analyze')) then analyzeSelection() end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel')) then invalidateAnalysis() end
            if state.analysis then
                local a = state.analysis
                tImGui.Text(string.format(tLang.L('swl_selected_fmt'), #a.vertices, a.totalMesh))
                tImGui.Text(string.format(tLang.L('swl_core_shell_fmt'),#a.core,#a.shell))
                tImGui.Text(string.format(tLang.L('swl_diagnostics_fmt'), a.missing, a.invalidSum, a.unknown))
                tImGui.Text(string.format(tLang.L('swl_disallowed_fmt'),a.disallowed))
                if state.heatmapEnabled and a.heatmapBoneName and #a.vertices>0 then
                    if a.heatmapInfluenced>0 then
                        tImGui.Text(string.format(tLang.L('swl_heatmap_analysis_stats_fmt'),
                            a.heatmapBoneName,a.heatmapInfluenced,#a.vertices,
                            a.heatmapMinPositive,a.heatmapMax))
                    else
                        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=0.75,b=0.15,a=1})
                        tImGui.TextWrapped(string.format(tLang.L('swl_heatmap_no_influence_fmt'),
                            a.heatmapBoneName))
                        tImGui.PopStyleColor()
                    end
                end
            elseif state.analysisDirty then
                tImGui.TextDisabled(tLang.L('swl_analysis_required'))
            end
            tImGui.Separator()
            showSectionTitle('swl_operation')
            local operations={tLang.L('swl_operation_inspect'),tLang.L('swl_operation_rigid'),
                tLang.L('swl_operation_normalize'),tLang.L('swl_operation_smooth'),
                tLang.L('swl_operation_repair')}
            tImGui.PushItemWidth(240)
            local operationChanged,operation=tImGui.Combo(tLang.L('swl_operation_mode'),
                state.operationMode,operations,-1)
            tImGui.PopItemWidth()
            showItemTooltip(tLang.L('swl_operation_mode_tooltip'))
            if operationChanged then
                state.operationMode=operation
                state.allowedBonesHighlight=false
                state.hoveredAllowedBone=nil
                updateAllowedBoneColors()
                if operation~=2 and state.targetBoneHighlight then
                    state.targetBoneHighlight=false
                    rebuildTargetBoneHighlight()
                end
            end
            local canOperate=state.analysis and not state.analysisDirty and
                #state.analysis.vertices>0

            if state.operationMode==3 then
            tImGui.Text(tLang.L('swl_weight_cleanup'))
            local canNormalize=state.analysis and not state.analysisDirty and #state.analysis.vertices>0
            tImGui.BeginDisabled(not canNormalize)
            local normalizePressed=tImGui.Button(tLang.L('swl_normalize_limit'))
            showItemTooltip(tLang.L('swl_normalize_limit_tooltip'))
            if normalizePressed then applyNormalizeAndLimit() end
            tImGui.EndDisabled()
            if state.normalizeReport then
                local report=state.normalizeReport
                tImGui.TextColored({r=0.25,g=0.80,b=1,a=1},tLang.L('swl_last_report'))
                tImGui.Text(string.format(tLang.L('swl_normalize_report_fmt'),report.total,
                    report.corrected,report.unchanged,report.skipped,report.failed))
            end
            elseif state.operationMode==2 then
            tImGui.Text(tLang.L('swl_rigid_bind'))
            local names = {}
            for _, bone in ipairs(bones) do names[#names+1] = bone.name end
            if #names > 0 then
                state.targetBoneIndex = math.min(state.targetBoneIndex, #names)
                tImGui.PushItemWidth(190)
                local edited, value = tImGui.Combo(tLang.L('swl_target_bone'), state.targetBoneIndex, names, -1)
                tImGui.PopItemWidth()
                tImGui.SameLine()
                local highlight=tImGui.Checkbox(tLang.L('swl_highlight')..'##swlTargetBoneHighlight',
                    state.targetBoneHighlight)
                if highlight~=state.targetBoneHighlight then
                    state.targetBoneHighlight=highlight
                    rebuildTargetBoneHighlight()
                end
                if edited then
                    state.targetBoneIndex=value
                    rebuildTargetBoneHighlight()
                end
            end
            showBoneRestrictions(bones,true)
            local canApply=state.analysis and not state.analysisDirty and
                #state.analysis.vertices>0 and #bones>0
            tImGui.BeginDisabled(not canApply)
            local applyRigidPressed=tImGui.Button(state.selectionMode==1 and hasTransitionShell() and
                    tLang.L('swl_apply_transition') or tLang.L('swl_apply_rigid'))
            showItemTooltip(tLang.L('swl_apply_rigid_tooltip'))
            if applyRigidPressed then
                applyRigidBind()
            end
            tImGui.EndDisabled()
            elseif state.operationMode==4 then
            tImGui.Text(tLang.L('swl_local_smoothing'))
            showBoneRestrictions(bones,false)
            showSmoothingControls()
            local canSmooth=state.analysis and not state.analysisDirty and #state.analysis.vertices>0
            tImGui.BeginDisabled(not canSmooth)
            if tImGui.Button(tLang.L('swl_apply_smoothing')) then applyLocalSmoothing() end
            tImGui.EndDisabled()
            tImGui.TextDisabled(tLang.L('swl_smoothing_scope'))
            elseif state.operationMode==1 then
            tImGui.Text(tLang.L('swl_transition_diagnostics'))
            showDiagnosticControls(canOperate,false)
            elseif state.operationMode==5 then
            tImGui.Text(tLang.L('swl_transition_repair'))
            showBoneRestrictions(bones,false)
            showSmoothingControls()
            showDiagnosticControls(canOperate,true)
            end
            showRollbackControls('swlWeightRevert')
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

function onInitScene()
    camera3d = mbm.getCamera('3d')
    camera3d:setFar(9999999)
    noMoveFlag = tImGui.Flags('ImGuiWindowFlags_NoCollapse')
    tUtil.sMessageOverlay = tLang.L('swl_welcome')
    tUtil.bRightSide = true
    tUtil.tTimerOverlay:start()
    mbm.setColor(0.08, 0.09, 0.12)
    applyCamera()
end

function onLoop(delta)
    updateCameraKeyboard(delta)
    updatePaintCursorHover()
    updateAuthoringPlayback(delta)
    showMenu()
    showPanel()
    showCameraPanel()
    showSkeletalTimelineWindow()
    syncPoseStressPreview()
    tUtil.showOverlayMessage()
end

function onTouchDown(key, x, y)
    if key==1 and state.workspace=='bone_editor' and state.boneEditorDrag then
        cancelBoneEditorDrag()
        return
    end
    if key==1 and state.workspace=='paint' and not tImGui.GetWantCaptureMouse() then
        local hit=pickPaintSurface(x,y)
        if hit then beginPaintStroke(hit) end
        return
    end
    if key == 0 and not tImGui.GetWantCaptureMouse() then
        if state.workspace=='paint' then
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
        if isWeightLabWorkspace() and state.meshD and state.selectionMode == 1 and state.aabb and
                rayHitsAABB(x,y,state.aabb) then
            local px,py,pz = cameraPosition()
            local nx,ny,nz = state.cam.fx-px,state.cam.fy-py,state.cam.fz-pz
            local length = math.sqrt(nx*nx+ny*ny+nz*nz)
            if length > 1e-6 then nx,ny,nz=nx/length,ny/length,nz/length end
            local center = {x=(state.aabb.minX+state.aabb.maxX)*0.5,
                y=(state.aabb.minY+state.aabb.maxY)*0.5,z=(state.aabb.minZ+state.aabb.maxZ)*0.5}
            local plane = {point=center,normal={x=nx,y=ny,z=nz}}
            local wx,wy,wz = rayPlaneHit(x,y,plane.point,plane.normal)
            if wx then
                state.aabbDragging = true
                state.aabbDragPlane = plane
                state.aabbDragOffset = {x=center.x-wx,y=center.y-wy,z=center.z-wz}
                return
            end
        end
        mouseDown, mouseX, mouseY = true, x, y
    end
end

function onTouchMove(key, x, y)
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
    elseif isWeightLabWorkspace() and state.aabbDragging and state.aabbDragPlane then
        local wx,wy,wz = rayPlaneHit(x,y,state.aabbDragPlane.point,state.aabbDragPlane.normal)
        if wx then
            local b,o = state.aabb,state.aabbDragOffset
            local cx,cy,cz = (b.minX+b.maxX)*0.5,(b.minY+b.maxY)*0.5,(b.minZ+b.maxZ)*0.5
            local nx,ny,nz = wx+o.x,wy+o.y,wz+o.z
            local dx,dy,dz = nx-cx,ny-cy,nz-cz
            b.minX,b.maxX=b.minX+dx,b.maxX+dx
            b.minY,b.maxY=b.minY+dy,b.maxY+dy
            b.minZ,b.maxZ=b.minZ+dz,b.maxZ+dz
            state.aabbDragPlane.point={x=nx,y=ny,z=nz}
            invalidateAnalysis()
            rebuildSelectionBox()
        end
    elseif mouseDown and not tImGui.GetWantCaptureMouse() then
        state.cam.azimuth = state.cam.azimuth + (x-mouseX) * 0.008
        state.cam.elevation = math.max(-1.45, math.min(1.45, state.cam.elevation + (y-mouseY) * 0.008))
        mouseX, mouseY = x, y
        applyCamera()
    end
end

function onTouchUp(key, x, y)
    if key==1 and state.workspace=='paint' then
        if state.paint.stroke then
            local hit=pickPaintSurface(x,y)
            if hit then extendPaintStroke(hit) end
            commitPaintStroke()
        end
        return
    end
    if key == 0 then
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
        state.aabbDragging = false
        state.aabbDragPlane = nil
        state.aabbDragOffset = nil
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
