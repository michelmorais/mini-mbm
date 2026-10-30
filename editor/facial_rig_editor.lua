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

   Facial Rig Editor

   Facial-only authoring shell following the Skeletal Animation Editor conventions.
]]--

tImGui = require "ImGui"
tUtil = require "editor_utils"
tFacialProject = require "facial_rig_project"
tFacialLandmarks = require "facial_rig_landmarks"
tFacialBridge = require "facial_rig_blender_bridge"
tFacialBindingPlan = require "facial_rig_binding_plan"
tFacialSkeletonBinding = require "facial_rig_skeleton_binding"
tFacialWeightAssignment = require "facial_rig_weight_assignment"
tFacialExpressions = require "facial_rig_expressions"
tFacialTargets = require "facial_rig_targets"
tFacialTargetProject = require "facial_rig_target_project"
tFacialClips = require "facial_rig_clips"
tFacialPlayback = require "facial_mesh_playback"
tBlender = require "blender_cli_wrapper"
tMeshImporter = require "blender_mesh_importer"
tFacialMeshExport = require "facial_rig_blender_mesh_export"

-- Protected-call wrapper used by all editor operations that can cross the Lua/C++ boundary.
local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then
        print('[facial_rig_editor] ' .. tostring(result[2]))
    end
    return table.unpack(result, 1, result.n)
end

local state = {
    project=tFacialProject.new(), projectPath=nil, mesh=nil, meshDebug=nil,
    meshBounds=nil, meshVertexCount=0, topology=nil, landmarkMarkers={}, landmarkMarkersDirty=true,
    workspace='setup', modified=false,
    selectedRole=1, selectedLandmark=1, landmarkEditMode=1, landmarkSurfaceOffset=0,
    landmarkSurfaceAutoOffset=true, landmarkSurfaceAxis=3,
    landmarkReferenceLarge=false, landmarkReferenceOpenRequested=false,
    status='', statusError=false,
    selectedMask=1, maskEditMode='add',
    maskBrush=nil,
    maskBrushRadius=0.045,
    maskBrushReference={key=nil,extent=nil},
    maskBrushSpatial={key=nil,cellSize=nil,cells=nil},
    maskBrushGeometry={signature=nil,bvh=nil},
    maskBrushHover={cursor=nil,lastX=nil,lastY=nil,pendingX=nil,pendingY=nil,
        lastTime=0,lastHit=nil,forceSurfaceHit=true},
    maskVisualization={dirty=true,needsBuild=true,building=false,buildCursor=0,buildTotal=0,
        buildSelected=nil,buildSurfaceCoords=nil,buildSurfaceUvs=nil,buildSurfaceIndices=nil,
        visible=true,surface=nil,faces=nil,markers=nil,hoverMarker=nil,
        hoverFace=nil,hoverTriangleKey=nil,hoverIndex=nil,vertexCount=0,faceCount=0,markerCount=0,
        surfaceUvDirty=false,surfaceUvCallbackArmed=false,surfaceUvChanges={},
        meshHidden=false,meshWasVisible=true,keepSurfaceWhileDirty=false},
    sourceObjectDraft='',
    camera={fx=0,fy=0,fz=0,distance=10,azimuth=0,elevation=0.15},
    cameraDrag=nil, cameraPan=nil, controlDown=false, shiftDown=false,
    faceGuideDirty=true, faceGuide={wireframe=nil,markers={},axes={},worldPoints={},origin=nil,length=nil},
    faceGizmoDrag=nil, windowMove=false, leftPanelRight=560,
    rigPreview={dirty=true,objects={},descriptor=nil},
    rigReview={dirty=true,data=nil},
    bindingPlan={dirty=true,data=nil},
    facialSkeleton={dirty=true,data=nil},
    weightAssignment={dirty=true,data=nil,modified=false,facialWeightsApplied=false},
    targetAuthoring=tFacialTargets.new(),
    targetOverlay={visible=true,dirty=true,building=false,surface=nil,meshHidden=false},
    targetBrushHover={pendingX=nil,pendingY=nil,lastHit=nil,lastX=nil,lastY=nil},
    exportWindow={open=false,target=1,includeProjectFacialClips=true},
    facialComparison={mesh=nil,enabled=false,ready=false,layoutActive=false},
    expressions={catalogDirty=true,catalog={},selectedTarget=1,selectedPose=1,nameDraft='',renameDraft='',editMode=1,lastEditMode=1},
    clipEditor={selectedClip=1,keySelected=1,time=0,playing=false,nameDraft='',renameDraft='',dirty=true,sourceInitialized=false,timelineVisible=true},
    preview={selectedPose=1,facialPlayback=nil,skeletalDirty=true,skeletalAnimations={},selectedSkeletal=1,
        authoredClipSelected=1,authoredClipTime=0,authoredClipPlaying=false,authoredClipInitialized=false},
    blenderBridgePath=nil, blenderSourcePath=nil, blenderGeneratedBlendPath=nil,
    automatedTargetJob={status='idle',jsonPath=nil,sourceBlendPath=nil,blendPath=nil,mshPath=nil,
        exportLogPath=nil,logPath=nil,importLogPath=nil,generationFinishedAt=nil,job=nil,
        startedAt=0,phase='',detail='',progress=0},
    undoStack={}, redoStack={}, historyLimit=50
}

local refreshMaskVisualization
local startMaskVisualizationBuild
local stepMaskVisualizationBuild
local updateMaskBrushHover
local drawMaskBrushScreenCursor
local drawTargetBrushScreenCursor
local updateTargetBrushHover
local setTargetOverlayVisible
local startTargetOverlayBuild
local stepTargetOverlayBuild
local destroyTargetOverlay

local function markMaskVisualizationDirty(keepSurface)
    local visualization=state.maskVisualization
    if visualization.building then
        visualization.building=false
        visualization.buildSelected=nil
        visualization.buildSelectedSet=nil
        visualization.buildSurfaceCoords=nil
        visualization.buildSurfaceUvs=nil
        visualization.buildSurfaceIndices=nil
    end
    visualization.dirty=true
    visualization.needsBuild=true
    visualization.keepSurfaceWhileDirty=keepSurface==true and visualization.surface~=nil
    if not visualization.keepSurfaceWhileDirty and visualization.meshHidden and state.mesh then
        state.mesh.visible=visualization.meshWasVisible
        visualization.meshHidden=false
    end
    if not visualization.keepSurfaceWhileDirty and visualization.surface then visualization.surface.visible=false end
    if visualization.faces then visualization.faces.visible=false end
    if visualization.markers then visualization.markers.visible=false end
    if visualization.hoverFace then visualization.hoverFace.visible=false end
    if visualization.hoverMarker then visualization.hoverMarker.visible=false end
    if state.maskBrushHover and state.maskBrushHover.cursor then
        state.maskBrushHover.cursor.visible=false
    end
    if state.maskBrushHover then
        state.maskBrushHover.lastHit=nil
        state.maskBrushHover.pendingX=nil
        state.maskBrushHover.pendingY=nil
        state.maskBrushHover.forceSurfaceHit=true
    end
    state.maskBrushReference={key=nil,extent=nil}
    state.maskBrushSpatial={key=nil,cellSize=nil,cells=nil}
    if not visualization.keepSurfaceWhileDirty then
        visualization.surfaceUvDirty=false
        visualization.surfaceUvCallbackArmed=false
        visualization.surfaceUvChanges={}
    end
end

local function setMaskVisualizationViewportVisible(visible)
    local visualization=state.maskVisualization
    local surfaceCanRemainVisible=visualization.keepSurfaceWhileDirty and visualization.surface~=nil
    local show=visible and visualization.visible and (not visualization.needsBuild or surfaceCanRemainVisible) and
        not visualization.building and state.workspace=='masks'
    if show and visualization.surface and state.mesh then
        if not visualization.meshHidden then
            visualization.meshWasVisible=state.mesh.visible~=false
            visualization.meshHidden=true
        end
        state.mesh.visible=false
    elseif visualization.meshHidden and state.mesh then
        state.mesh.visible=visualization.meshWasVisible
        visualization.meshHidden=false
    end
    if visualization.surface then visualization.surface.visible=show end
    if visualization.faces then visualization.faces.visible=show end
    if visualization.markers then visualization.markers.visible=show end
    if state.maskBrushHover and state.maskBrushHover.cursor then
        state.maskBrushHover.cursor.visible=visible and (not visualization.needsBuild or surfaceCanRemainVisible) and
            not visualization.building and state.workspace=='masks' and
            state.maskBrushHover.lastHit~=nil
    end
    if not show then
        if visualization.hoverFace then visualization.hoverFace.visible=false end
        if visualization.hoverMarker then visualization.hoverMarker.visible=false end
    end
end

local windows = {
    main='facial_rig_main', camera='facial_rig_camera', report='facial_rig_report',
    clipTimeline='facial_clip_timeline'
}

local faceGuidePaths={
    {'left_eye_corner','left_eye_center','brow_left'},
    {'right_eye_corner','right_eye_center','brow_right'},
    {'nose_bridge','nose_tip','upper_lip'},
    {'mouth_left','upper_lip','mouth_right','lower_lip','mouth_left'},
    {'jaw_hinge_left','mouth_left'},
    {'jaw_hinge_right','mouth_right'},
    {'jaw_hinge_left','chin','jaw_hinge_right'}
}

local landmarkReferencePoints={
    face_left={0.271,0.527}, face_right={0.729,0.527}, face_top={0.500,0.170}, face_bottom={0.500,0.920},
    left_eye_center={0.393,0.527}, right_eye_center={0.607,0.527},
    left_eye_corner={0.354,0.527}, right_eye_corner={0.646,0.527},
    nose_bridge={0.500,0.473}, nose_tip={0.500,0.589},
    mouth_left={0.404,0.705}, mouth_right={0.596,0.705},
    upper_lip={0.500,0.679}, lower_lip={0.500,0.732}, chin={0.500,0.804},
    jaw_hinge_left={0.329,0.679}, jaw_hinge_right={0.671,0.679},
    brow_left={0.396,0.375}, brow_right={0.604,0.375}
}

local function shortName(path)
    return tUtil.getShortName(path or '', false) or path or ''
end

local function pathExists(path)
    if not path or path=='' then return false end
    local file=io.open(path,'rb')
    if not file then return false end
    file:close()
    return true
end

local function getEditorDir()
    local source=debug.getinfo(1,'S').source or ''
    if source:sub(1,1)=='@' then source=source:sub(2) end
    return source:match('^(.*)[/\\]') or '.'
end

local setStatus
local showHelpTooltip

local function replaceExtension(path,extension)
    if not path or path=='' then return '' end
    return (path:gsub('%.[^%./\\]+$','')) .. extension
end

local function sourceObjectCatalogPath(meshPath)
    local stem=(meshPath or ''):match('^(.*)%.[^%.]+$') or meshPath or ''
    return stem .. '_mbm_objects.lua'
end

local function loadSourceObjectCatalog(meshPath)
    local catalogPath=sourceObjectCatalogPath(meshPath)
    local file=io.open(catalogPath,'rb')
    if not file then return 0 end
    file:close()
    local chunk,errorMessage=loadfile(catalogPath)
    if not chunk then
        print('[facial_rig_editor] source object catalog load failed: ' .. tostring(errorMessage))
        return 0
    end
    local ok,catalog=pcall(chunk)
    if not ok or type(catalog)~='table' or type(catalog.objects)~='table' then return 0 end
    local count=0
    for _,objectName in ipairs(catalog.objects) do
        if tFacialProject.addSourceObject(state.project,objectName) then count=count + 1 end
    end
    if type(catalog.vertexMap)=='table' then
        tFacialProject.setSourceVertexMap(state.project,catalog.vertexMap,catalog.topologySignature)
    end
    return count
end

local function sourceObjectOptions(roleName)
    local labels={tLang.L('fre_unassigned')}
    local values={''}
    local known={['']=true}
    local function append(objectName, suggested)
        if type(objectName) ~= 'string' or objectName == '' or known[objectName] then return end
        known[objectName]=true
        values[#values + 1]=objectName
        labels[#labels + 1]=suggested and (objectName .. tLang.L('fre_source_object_suggestion_suffix')) or objectName
    end
    for _, objectName in ipairs(tFacialProject.sourceObjects(state.project)) do append(objectName,false) end
    for _, role in ipairs(state.project.roles or {}) do append(role.object,false) end
    for _, objectName in ipairs(tFacialProject.sourceObjectSuggestions(roleName)) do append(objectName,true) end
    return labels, values
end

local function localizedLandmarkName(name)
    local key='fre_landmark_name_' .. tostring(name or '')
    local translated=tLang.L(key)
    return translated==key and tostring(name or '') or translated
end

local function localizedLandmarkSide(side)
    local key='fre_side_' .. tostring(side or '')
    local translated=tLang.L(key)
    return translated==key and tostring(side or '') or translated
end

local function localizedProjectMessage(message)
    local text=tostring(message or '')
    local lower=text:lower()
    local function diagnosticName(pattern)
        local value=lower:match(pattern)
        return value and value:gsub('[%s%.]+$','') or nil
    end
    local name=diagnosticName('^required landmark not placed:?%s*(.+)$')
    if name then return string.format(tLang.L('fre_error_required_landmark_fmt'),localizedLandmarkName(name)) end
    name=diagnosticName('^landmark vertex is outside the current topology:?%s*(.+)$')
    if name then return string.format(tLang.L('fre_error_landmark_vertex_fmt'),localizedLandmarkName(name)) end
    name=diagnosticName('^mask topology is stale:?%s*(.+)$')
    if name then return string.format(tLang.L('fre_error_mask_stale_fmt'),name) end
    name=diagnosticName('^mask has an invalid or duplicate vertex:?%s*(.+)$')
    if name then return string.format(tLang.L('fre_error_mask_invalid_vertex_fmt'),name) end
    local exact={
        ['mesh topology changed since the project was saved']='fre_error_mesh_topology',
        ['mesh vertex count changed since the project was saved']='fre_error_mesh_vertex_count',
        ['mesh is not loaded']='fre_error_mesh_not_loaded',
        ['project is missing']='fre_error_project_missing',
        ['face origin is not placed']='fre_error_face_origin',
        ['face origin vertex is outside the current topology']='fre_error_face_origin_vertex',
        ['mask has no name']='fre_error_mask_no_name',
        ['mask has invalid vertex list']='fre_error_mask_invalid_list',
    }
    return exact[lower] and tLang.L(exact[lower]) or text
end

local function localizedRigMessage(message)
    local text=tostring(message or '')
    local lower=text:lower()
    local name=lower:match('^rig landmark is missing:?%s*(.+)$')
    if name then return string.format(tLang.L('fre_error_rig_landmark_fmt'),localizedLandmarkName(name)) end
    name=lower:match('^mask has no target role:?%s*(.+)$')
    if name then return string.format(tLang.L('fre_error_rig_mask_role_fmt'),name) end
    name=lower:match('^mask targets a disabled role:?%s*(.+)$')
    if name then return string.format(tLang.L('fre_warning_rig_disabled_role_fmt'),name) end
    local exact={
        ['rig origin is not placed']='fre_error_rig_origin',
        ['main_face has no object or vertex mask']='fre_warning_rig_main_face',
        ['eye corner landmarks are optional; eye bones use their centers']='fre_warning_rig_eye_corners',
        ['jaw hinge landmarks are optional; jaw uses the mouth center']='fre_warning_rig_jaw_hinges',
        ['nose_bridge is optional; nose starts at nose_tip']='fre_warning_rig_nose_bridge',
        ['nose_tip is optional; the nose bone is omitted']='fre_warning_rig_nose_tip',
        ['chin is optional; the jaw bone is omitted']='fre_warning_rig_chin'
    }
    return exact[lower] and tLang.L(exact[lower]) or text
end

local function localizedBridgeMessage(message)
    local text=tostring(message or '')
    local name=text:lower():match('^role has no source object or mask:?%s*(.+)$')
    return name and string.format(tLang.L('fre_warning_bridge_unassigned_fmt'),name) or text
end

local function localizedBindingReviewStatus(status)
    local key='fre_binding_review_status_' .. tostring(status or '')
    local translated=tLang.L(key)
    return translated==key and tostring(status or '') or translated
end

local function localizedBindingReviewMode(mode)
    local key='fre_binding_review_mode_' .. tostring(mode or '')
    local translated=tLang.L(key)
    return translated==key and tostring(mode or '') or translated
end

local function localizedBindingPlanMethod(method)
    local key='fre_binding_plan_method_' .. tostring(method or '')
    local translated=tLang.L(key)
    return translated==key and tostring(method or '') or translated
end

local function localizedBindingPlanReason(reason)
    local exact={
        ['canonical rig description is not valid']='fre_binding_plan_reason_rig',
        ['role has no object or vertex mask']='fre_binding_plan_reason_unassigned',
        ['object vertices are not available in the merged MSH topology']='fre_binding_plan_reason_object_unverified',
        ['source vertex map topology is stale or missing']='fre_weight_reason_stale_source_map',
        ['the selected policy has no generated bone']='fre_binding_plan_reason_no_bone',
        ['the role mask has no vertices']='fre_binding_plan_reason_empty_mask',
        ['vertex masks overlap across enabled roles']='fre_binding_plan_reason_overlap',
        ['no enabled facial roles are available']='fre_binding_plan_reason_no_roles'
    }
    local translated=exact[reason] and tLang.L(exact[reason]) or reason
    return translated
end

local function localizedWeightAssignmentReason(reason)
    local exact={
        ['binding plan entry is blocked']='fre_weight_reason_plan_blocked',
        ['this role policy is not enabled in this milestone']='fre_weight_reason_policy_blocked',
        ['only rigid role masks are enabled in this milestone']='fre_weight_reason_rigid_only',
        ['rigid role must resolve to exactly one generated bone']='fre_weight_reason_one_bone',
        ['generated bone is not present in the mesh skeleton']='fre_weight_reason_bone_missing',
        ['role has no explicit vertex mask']='fre_weight_reason_mask_missing',
        ['mesh skeleton is not canonical']='fre_weight_reason_noncanonical',
        ['mesh has no canonical vertex weights']='fre_weight_reason_no_weights',
        ['facial root is not attached to the body head']='fre_weight_reason_root_parent',
        ['mesh has no readable canonical skeleton']='fre_weight_reason_no_skeleton',
        ['mesh skeleton report API is unavailable']='fre_weight_reason_no_report',
        ['all enabled roles must be ready before assignment']='fre_weight_reason_all_ready',
        ['assignment contains a blocked role']='fre_weight_reason_plan_blocked',
        ['assignment contains overlapping vertices']='fre_weight_reason_overlap',
        ['assignment has no vertices']='fre_weight_reason_no_vertices',
        ['role contains an invalid mapped vertex']='fre_weight_reason_invalid_mapped_vertex',
        ['source vertex map topology is stale or missing']='fre_weight_reason_stale_source_map',
        ['mask topology is stale:']='fre_weight_reason_stale_mask',
        ['mask contains an invalid vertex:']='fre_weight_reason_invalid_mask',
        ['some enabled roles remain blocked; only ready rigid roles can be assigned']='fre_weight_reason_partial',
        ['some enabled roles remain blocked; only ready roles can be assigned']='fre_weight_reason_partial',
        ['mesh geometry API is unavailable: getTotalSubset']='fre_weight_reason_geometry_api',
        ['mesh geometry API is unavailable: getTotalVertex']='fre_weight_reason_geometry_api',
        ['mesh geometry API is unavailable: getVertex']='fre_weight_reason_geometry_api',
        ['mesh vertex positions are unavailable']='fre_weight_reason_geometry_api',
        ['mesh vertex order does not match the loaded topology']='fre_weight_reason_vertex_order',
        ['generated facial bones are not present in the mesh skeleton']='fre_weight_reason_bone_missing',
        ['facial role contains a vertex without a position']='fre_weight_reason_vertex_position'
    }
    if exact[reason] then return tLang.L(exact[reason]) end
    local prefixKeys={
        ['mask topology is stale:']='fre_weight_reason_stale_mask',
        ['mask contains an invalid vertex:']='fre_weight_reason_invalid_mask'
    }
    for prefix,key in pairs(prefixKeys) do
        if tostring(reason or ''):sub(1,#prefix)==prefix then return tLang.L(key) end
    end
    return tostring(reason or '')
end

local function localizedFacialSkeletonReason(reason)
    local exact={
        ['canonical facial rig is not valid']='fre_facial_skeleton_reason_rig',
        ['mesh has no canonical skeleton']='fre_facial_skeleton_reason_no_skeleton',
        ['mesh skeleton is not canonical']='fre_facial_skeleton_reason_noncanonical',
        ['canonical facial rig has no generated bones']='fre_facial_skeleton_reason_no_bones',
        ['generated facial bone has no name']='fre_facial_skeleton_reason_no_name',
        ['generated facial bone name is duplicated']='fre_facial_skeleton_reason_duplicate'
    }
    return exact[reason] and tLang.L(exact[reason]) or tostring(reason or '')
end

local function showProjectMessages(report)
    for _,message in ipairs(report.errors or {}) do
        tImGui.TextWrapped(tLang.L('fre_error_prefix') .. localizedProjectMessage(message))
    end
    for _,message in ipairs(report.warnings or {}) do
        tImGui.TextWrapped(tLang.L('fre_warning_prefix') .. localizedProjectMessage(message))
    end
end

local function localizedLandmarkDescription(name)
    local key='fre_landmark_reference_' .. tostring(name or '')
    local translated=tLang.L(key)
    return translated==key and localizedLandmarkName(name) or translated
end

local function localizedLandmarkGuideName(name)
    local key='fre_landmark_guide_' .. tostring(name or '')
    local translated=tLang.L(key)
    return translated==key and localizedLandmarkName(name) or translated
end

local function drawLandmarkReferenceGuide(origin,width,height,selectedName)
    local function point(name)
        local normalized=landmarkReferencePoints[name] or {0.5,0.5}
        return {x=origin.x+normalized[1]*width,y=origin.y+normalized[2]*height}
    end
    local lineColor={r=0.14,g=0.78,b=0.96,a=1}
    local pointColor={r=0.14,g=0.78,b=0.96,a=1}
    local outlineColor={r=0.39,g=0.85,b=1,a=1}
    local featureColor={r=0.56,g=0.65,b=0.82,a=1}
    local hairColor={r=0.08,g=0.07,b=0.07,a=1}
    local hairAccent={r=0.32,g=0.24,b=0.20,a=1}
    local textColor={r=0.92,g=0.96,b=1,a=1}
    local labelSize=math.max(15,math.min(24,width/55))
    local center={x=origin.x+width*0.5,y=origin.y+height*0.53}
    local radiusX=width*0.257
    local radiusY=height*0.393
    local k=0.5522848
    local function curve(p1,p2,p3,p4,color,thickness)
        tImGui.AddBezierCubic(p1,p2,p3,p4,color,thickness,20)
    end
    curve({x=center.x,y=center.y-radiusY},{x=center.x+k*radiusX,y=center.y-radiusY},
        {x=center.x+radiusX,y=center.y-k*radiusY},{x=center.x+radiusX,y=center.y},outlineColor,2.5)
    curve({x=center.x+radiusX,y=center.y},{x=center.x+radiusX,y=center.y+k*radiusY},
        {x=center.x+k*radiusX,y=center.y+radiusY},{x=center.x,y=center.y+radiusY},outlineColor,2.5)
    curve({x=center.x,y=center.y+radiusY},{x=center.x-k*radiusX,y=center.y+radiusY},
        {x=center.x-radiusX,y=center.y+k*radiusY},{x=center.x-radiusX,y=center.y},outlineColor,2.5)
    curve({x=center.x-radiusX,y=center.y},{x=center.x-radiusX,y=center.y-k*radiusY},
        {x=center.x-k*radiusX,y=center.y-radiusY},{x=center.x,y=center.y-radiusY},outlineColor,2.5)
    tImGui.AddLine({x=center.x,y=origin.y+height*0.17},{x=center.x,y=origin.y+height*0.94},
        {r=0.84,g=0.72,b=0.35,a=0.9},1.5)
    local leftBrow,rightBrow=point('brow_left'),point('brow_right')
    curve({x=leftBrow.x-width*0.075,y=leftBrow.y+height*0.01},
        {x=leftBrow.x-width*0.045,y=leftBrow.y-height*0.04},
        {x=leftBrow.x-width*0.015,y=leftBrow.y-height*0.02},leftBrow,featureColor,4)
    curve(leftBrow,{x=leftBrow.x+width*0.015,y=leftBrow.y-height*0.02},
        {x=leftBrow.x+width*0.045,y=leftBrow.y-height*0.04},
        {x=leftBrow.x+width*0.075,y=leftBrow.y+height*0.01},featureColor,4)
    curve({x=rightBrow.x-width*0.075,y=rightBrow.y+height*0.01},
        {x=rightBrow.x-width*0.045,y=rightBrow.y-height*0.04},
        {x=rightBrow.x-width*0.015,y=rightBrow.y-height*0.02},rightBrow,featureColor,4)
    curve(rightBrow,{x=rightBrow.x+width*0.015,y=rightBrow.y-height*0.02},
        {x=rightBrow.x+width*0.045,y=rightBrow.y-height*0.04},
        {x=rightBrow.x+width*0.075,y=rightBrow.y+height*0.01},featureColor,4)
    local leftEye,rightEye=point('left_eye_center'),point('right_eye_center')
    curve({x=leftEye.x-width*0.06,y=leftEye.y},{x=leftEye.x-width*0.025,y=leftEye.y-height*0.04},
        {x=leftEye.x+width*0.025,y=leftEye.y-height*0.04},{x=leftEye.x+width*0.06,y=leftEye.y},featureColor,3)
    curve({x=rightEye.x-width*0.06,y=rightEye.y},{x=rightEye.x-width*0.025,y=rightEye.y-height*0.04},
        {x=rightEye.x+width*0.025,y=rightEye.y-height*0.04},{x=rightEye.x+width*0.06,y=rightEye.y},featureColor,3)
    curve({x=leftEye.x-width*0.06,y=leftEye.y},{x=leftEye.x-width*0.025,y=leftEye.y+height*0.04},
        {x=leftEye.x+width*0.025,y=leftEye.y+height*0.04},{x=leftEye.x+width*0.06,y=leftEye.y},featureColor,2)
    curve({x=rightEye.x-width*0.06,y=rightEye.y},{x=rightEye.x-width*0.025,y=rightEye.y+height*0.04},
        {x=rightEye.x+width*0.025,y=rightEye.y+height*0.04},{x=rightEye.x+width*0.06,y=rightEye.y},featureColor,2)
    tImGui.AddCircle(leftEye,width*0.022,featureColor,16,2)
    tImGui.AddCircle(rightEye,width*0.022,featureColor,16,2)
    tImGui.AddCircleFilled(leftEye,width*0.010,{r=0.08,g=0.08,b=0.10,a=1},12)
    tImGui.AddCircleFilled(rightEye,width*0.010,{r=0.08,g=0.08,b=0.10,a=1},12)
    tImGui.AddLine({x=center.x-width*0.025,y=center.y-height*0.02},
        {x=center.x-width*0.025,y=center.y+height*0.10},featureColor,3)
    tImGui.AddLine({x=center.x-width*0.025,y=center.y+height*0.10},
        {x=center.x+width*0.025,y=center.y+height*0.10},featureColor,3)
    curve({x=origin.x+width*0.40,y=origin.y+height*0.69},{x=origin.x+width*0.45,y=origin.y+height*0.65},
        {x=origin.x+width*0.55,y=origin.y+height*0.65},{x=origin.x+width*0.60,y=origin.y+height*0.69},{r=0.96,g=0.55,b=0.65,a=1},4)
    curve({x=origin.x+width*0.40,y=origin.y+height*0.73},{x=origin.x+width*0.45,y=origin.y+height*0.77},
        {x=origin.x+width*0.55,y=origin.y+height*0.77},{x=origin.x+width*0.60,y=origin.y+height*0.73},{r=0.96,g=0.55,b=0.65,a=1},4)
    curve({x=origin.x+width*0.36,y=origin.y+height*0.30},{x=origin.x+width*0.42,y=origin.y+height*0.10},
        {x=origin.x+width*0.58,y=origin.y+height*0.10},{x=origin.x+width*0.64,y=origin.y+height*0.30},hairColor,9)
    curve({x=origin.x+width*0.36,y=origin.y+height*0.30},{x=origin.x+width*0.42,y=origin.y+height*0.10},
        {x=origin.x+width*0.58,y=origin.y+height*0.10},{x=origin.x+width*0.64,y=origin.y+height*0.30},hairAccent,2)
    local labels={
        {'face_left',0.02,0.53,0.20,0.53},{'face_right',0.80,0.53,0.98,0.53},
        {'face_top',0.44,0.04,0.50,0.08},{'face_bottom',0.40,0.96,0.50,0.97},
        {'brow_left',0.02,0.27,0.25,0.34},{'brow_right',0.76,0.27,0.75,0.34},
        {'left_eye_corner',0.02,0.43,0.28,0.46},{'right_eye_corner',0.73,0.43,0.72,0.46},
        {'left_eye_center',0.02,0.61,0.29,0.61},{'right_eye_center',0.72,0.61,0.71,0.61},
        {'jaw_hinge_left',0.02,0.68,0.25,0.68},{'jaw_hinge_right',0.76,0.68,0.75,0.68},
        {'mouth_left',0.02,0.80,0.27,0.80},{'mouth_right',0.78,0.80,0.73,0.80},
        {'nose_bridge',0.54,0.45,0.53,0.45},{'nose_tip',0.54,0.57,0.53,0.57},
        {'upper_lip',0.54,0.67,0.53,0.67},{'lower_lip',0.54,0.74,0.53,0.74},
        {'chin',0.54,0.81,0.53,0.81}
    }
    for _,label in ipairs(labels) do
        local name,x,y,pointX,pointY=table.unpack(label)
        tImGui.AddLine(point(name),{x=origin.x+pointX*width,y=origin.y+pointY*height},lineColor,2)
        tImGui.AddText({x=origin.x+x*width,y=origin.y+y*height},textColor,
            localizedLandmarkGuideName(name),nil,labelSize)
    end
    local hairAnchor={x=origin.x+width*0.70,y=origin.y+height*0.14}
    tImGui.AddLine({x=origin.x+width*0.65,y=origin.y+height*0.22},hairAnchor,lineColor,1.2)
    tImGui.AddText(hairAnchor,textColor,tLang.L('fre_landmark_reference_hair'),nil,labelSize)
    for name in pairs(landmarkReferencePoints) do
        local landmarkPoint=point(name)
        local selected=name==selectedName
        tImGui.AddCircleFilled(landmarkPoint,selected and 5 or 3,
            selected and {r=1,g=0.75,b=0.15,a=1} or pointColor,12)
        if selected then tImGui.AddCircle(landmarkPoint,9,{r=1,g=0.85,b=0.3,a=1},16,1.5) end
    end
end

local function showLandmarkReferenceModal()
    if not state.landmarkReferenceLarge then return end
    if state.landmarkReferenceOpenRequested then
        -- OpenPopup must run at the same (root) ID stack as BeginPopupModal.
        -- The checkbox lives below the landmarks TreeNode, so defer the call
        -- until this root-level modal function.
        tImGui.OpenPopup('fre_landmark_reference_modal')
        state.landmarkReferenceOpenRequested=false
    end
    local screenWidth,screenHeight=mbm.getRealSizeScreen()
    local maxWidth=math.max(520,screenWidth-48)
    local maxHeight=math.max(360,screenHeight-56)
    local initialWidth=math.min(1120,maxWidth)
    local initialHeight=math.min(860,maxHeight)
    tImGui.SetNextWindowSizeConstraints({x=520,y=360},{x=maxWidth,y=maxHeight})
    tImGui.SetNextWindowSize({x=initialWidth,y=initialHeight},tImGui.Flags('ImGuiCond_Appearing'))
    local opened,closed=tImGui.BeginPopupModal(
        tLang.L('fre_landmark_reference_large_title') .. '###fre_landmark_reference_modal',true,0)
    if closed then state.landmarkReferenceLarge=false end
    if not opened then return end

    tImGui.TextWrapped(tLang.L('fre_landmark_reference_large_help'))
    tImGui.Separator()
    local available=tImGui.GetContentRegionAvail()
    local canvasWidth=math.min(math.max(420,available.x),1000)
    local canvasHeight=math.min(math.max(330,available.y-48),canvasWidth*0.80)
    if available.x>canvasWidth then tImGui.SetCursorPosX(tImGui.GetCursorPosX()+(available.x-canvasWidth)*0.5) end
    local origin=tImGui.GetCursorScreenPos()
    tImGui.Dummy({x=canvasWidth,y=canvasHeight})
    local selected=state.project.landmarks[state.selectedLandmark]
    drawLandmarkReferenceGuide(origin,canvasWidth,canvasHeight,selected and selected.name or nil)
    tImGui.Separator()
    if tImGui.Button(tLang.L('fre_landmark_reference_large_close')) then
        state.landmarkReferenceLarge=false
        tImGui.CloseCurrentPopup()
    end
    tImGui.EndPopup()
end

local function showLandmarkReference(selected)
    if not selected then return end
    tImGui.Text(tLang.L('fre_landmark_reference_title'))
    tImGui.TextWrapped(localizedLandmarkDescription(selected.name))
    tImGui.TextWrapped(tLang.L('fre_landmark_reference_help'))
    local largeVisible=tImGui.Checkbox(
        tLang.L('fre_landmark_reference_large'),state.landmarkReferenceLarge)
    if largeVisible~=state.landmarkReferenceLarge then
        state.landmarkReferenceLarge=largeVisible
        if largeVisible then
            state.landmarkReferenceOpenRequested=true
        else
            state.landmarkReferenceOpenRequested=false
        end
    end
    showHelpTooltip(tLang.L('fre_landmark_reference_large_tooltip'))

    local origin=tImGui.GetCursorScreenPos()
    local width,height=250,180
    tImGui.Dummy({x=width,y=height})
    local center={x=origin.x+width*0.5,y=origin.y+height*0.5}
    local faceRadius=72
    tImGui.AddCircle(center,faceRadius,{r=0.45,g=0.48,b=0.55,a=0.9},36,1.5)
    tImGui.AddLine({x=center.x-37,y=center.y-25},{x=center.x-12,y=center.y-25},{r=0.55,g=0.65,b=0.9,a=0.8},1.5)
    tImGui.AddLine({x=center.x+12,y=center.y-25},{x=center.x+37,y=center.y-25},{r=0.55,g=0.65,b=0.9,a=0.8},1.5)
    tImGui.AddLine({x=center.x,y=center.y-18},{x=center.x,y=center.y+20},{r=0.55,g=0.65,b=0.9,a=0.8},1.5)
    tImGui.AddLine({x=center.x-28,y=center.y+38},{x=center.x+28,y=center.y+38},{r=0.55,g=0.65,b=0.9,a=0.8},1.5)

    for name,point in pairs(landmarkReferencePoints) do
        local position={x=origin.x+point[1]*width,y=origin.y+point[2]*height}
        local selectedPoint=name==selected.name
        local color=selectedPoint and {r=1,g=0.75,b=0.15,a=1} or {r=0.2,g=0.75,b=1,a=0.9}
        tImGui.AddCircleFilled(position,selectedPoint and 5 or 3,color,12)
        if selectedPoint then tImGui.AddCircle(position,8,{r=1,g=0.85,b=0.3,a=1},16,1.5) end
    end
end

setStatus=function(message, isError)
    state.status=tostring(message or '')
    state.statusError=isError == true
    tUtil.showMessage(state.status, state.statusError and 5 or 3)
end

local function markModified()
    state.modified=true
    state.rigPreview.dirty=true
    state.rigReview.dirty=true
    state.bindingPlan.dirty=true
    state.weightAssignment.dirty=true
end

local function clearHistory()
    state.undoStack={}
    state.redoStack={}
end

local function historySnapshot()
    return {project=tUtil.deepCopyTable(state.project),modified=state.modified,
        selectedRole=state.selectedRole,selectedLandmark=state.selectedLandmark,
        selectedMask=state.selectedMask,landmarkEditMode=state.landmarkEditMode,
        selectedExpressionTarget=state.expressions.selectedTarget,
        selectedExpressionPose=state.expressions.selectedPose,
        selectedClip=state.clipEditor.selectedClip,keySelected=state.clipEditor.keySelected,
        clipTime=state.clipEditor.time,clipPlaying=state.clipEditor.playing}
end

local function pushUndoSnapshot()
    state.undoStack[#state.undoStack+1]=historySnapshot()
    while #state.undoStack>(state.historyLimit or 50) do table.remove(state.undoStack,1) end
    state.redoStack={}
end

local function restoreHistorySnapshot(snapshot)
    if not snapshot or not snapshot.project then return false end
    state.project=tUtil.deepCopyTable(snapshot.project)
    state.modified=snapshot.modified == true
    state.selectedRole=snapshot.selectedRole or state.project.selectedRole or 1
    state.selectedLandmark=snapshot.selectedLandmark or state.project.selectedLandmark or 1
    state.selectedMask=snapshot.selectedMask or state.project.selectedMask or 1
    state.landmarkEditMode=snapshot.landmarkEditMode or state.project.landmarkEditMode or 1
    state.expressions.selectedTarget=snapshot.selectedExpressionTarget or 1
    state.expressions.selectedPose=snapshot.selectedExpressionPose or 1
    state.expressions.renameDraft=''
    state.clipEditor.selectedClip=snapshot.selectedClip or 1
    state.clipEditor.keySelected=snapshot.keySelected or 1
    state.clipEditor.time=snapshot.clipTime or 0
    state.clipEditor.playing=snapshot.clipPlaying==true
    state.preview.authoredClipInitialized=false
    state.preview.authoredClipPlaying=false
    state.project.selectedRole=state.selectedRole
    state.project.selectedLandmark=state.selectedLandmark
    state.project.selectedMask=state.selectedMask
    state.project.landmarkEditMode=state.landmarkEditMode
    state.landmarkMarkersDirty=true
    state.faceGuideDirty=true
    markMaskVisualizationDirty()
    state.rigPreview.dirty=true
    state.rigReview.dirty=true
    state.bindingPlan.dirty=true
    state.weightAssignment.dirty=true
    return true
end

local function undoProject()
    if #state.undoStack==0 then return false end
    local current=historySnapshot()
    local snapshot=table.remove(state.undoStack)
    state.redoStack[#state.redoStack+1]=current
    return restoreHistorySnapshot(snapshot)
end

local function redoProject()
    if #state.redoStack==0 then return false end
    local current=historySnapshot()
    local snapshot=table.remove(state.redoStack)
    state.undoStack[#state.undoStack+1]=current
    return restoreHistorySnapshot(snapshot)
end

local function sourceObjectCombo(role, id, labels, values)
    local selected=1
    for index, value in ipairs(values) do
        if value == (role.object or '') then selected=index;break end
    end
    tImGui.SetNextItemWidth(-1)
    local changed,index=tImGui.Combo('##freRoleObject' .. tostring(id),selected,labels)
    if not changed then return false end
    local objectName=values[index] or ''
    if objectName == (role.object or '') then return false end
    pushUndoSnapshot()
    role.object=objectName
    tFacialProject.addSourceObject(state.project,objectName)
    markModified()
    return true
end

local function destroyRigPreview()
    for _,object in ipairs(state.rigPreview.objects or {}) do
        dpCall(function() object:destroy() end)
    end
    state.rigPreview={dirty=true,objects={},descriptor=nil}
end

local function destroyMaskVisualization()
    local visualization=state.maskVisualization
    if visualization.meshHidden and state.mesh then
        state.mesh.visible=visualization.meshWasVisible
    end
    if visualization.surface then dpCall(function() visualization.surface:destroy() end) end
    if visualization.faces then dpCall(function() visualization.faces:destroy() end) end
    if visualization.markers then dpCall(function() visualization.markers:destroy() end) end
    if visualization.hoverMarker then dpCall(function() visualization.hoverMarker:destroy() end) end
    if visualization.hoverFace then dpCall(function() visualization.hoverFace:destroy() end) end
    if state.maskBrushHover and state.maskBrushHover.cursor then
        dpCall(function() state.maskBrushHover.cursor:destroy() end)
    end
    state.maskVisualization={dirty=true,needsBuild=true,building=false,buildCursor=0,buildTotal=0,
        buildSelected=nil,buildSurfaceCoords=nil,buildSurfaceUvs=nil,buildSurfaceIndices=nil,
        visible=true,surface=nil,faces=nil,markers=nil,hoverMarker=nil,
        hoverFace=nil,hoverTriangleKey=nil,hoverIndex=nil,vertexCount=0,faceCount=0,markerCount=0,
        meshHidden=false,meshWasVisible=true,keepSurfaceWhileDirty=false}
    state.maskBrushHover={cursor=nil,lastX=nil,lastY=nil,pendingX=nil,pendingY=nil,
        lastTime=0,lastHit=nil,forceSurfaceHit=true}
    state.maskBrushReference={key=nil,extent=nil}
    state.maskBrushGeometry={signature=nil,bvh=nil}
end

local function destroyPreview()
    destroyRigPreview()
    destroyMaskVisualization()
    destroyTargetOverlay()
    state.targetBrushHover={pendingX=nil,pendingY=nil,lastHit=nil,lastX=nil,lastY=nil}
    if state.facialComparison and state.facialComparison.mesh then
        -- This comparison instance is temporary. Clear the editor-only
        -- no-frustum-culling override before destroying it.
        state.facialComparison.mesh.alwaysRender=false
        dpCall(function() state.facialComparison.mesh:destroy() end)
    end
    state.facialComparison={mesh=nil,enabled=false,ready=false,layoutActive=false}
    if state.targetAuthoring and state.targetAuthoring.previewPath then
        if state.mesh then dpCall(function() state.mesh:destroy() end); state.mesh=nil end
        dpCall(function() meshDebug:fakeRelease(state.targetAuthoring.previewPath) end)
        os.remove(state.targetAuthoring.previewPath)
        state.targetAuthoring.previewPath=nil
    end
    for _,marker in ipairs(state.landmarkMarkers or {}) do
        dpCall(function() marker:destroy() end)
    end
    if state.faceGuide.wireframe then dpCall(function() state.faceGuide.wireframe:destroy() end) end
    for _,marker in pairs(state.faceGuide.markers or {}) do dpCall(function() marker:destroy() end) end
    for _,axis in pairs(state.faceGuide.axes or {}) do dpCall(function() axis:destroy() end) end
    state.faceGuide={wireframe=nil,markers={},axes={},worldPoints={},origin=nil,length=nil}
    state.faceGuideDirty=true
    state.landmarkMarkers={}
    if state.mesh then
        dpCall(function() state.mesh:destroy() end)
        state.mesh=nil
    end
    state.meshDebug=nil
    state.meshBounds=nil
    state.meshVertexCount=0
    state.topology=nil
    state.landmarkMarkersDirty=true
end

local function applyCamera()
    local c=state.camera
    local x=c.fx+c.distance*math.cos(c.elevation)*math.sin(c.azimuth)
    local y=c.fy+c.distance*math.sin(c.elevation)
    local z=c.fz+c.distance*math.cos(c.elevation)*math.cos(c.azimuth)
    camera3d:setPos(x,y,z)
    camera3d:setFocus(c.fx,c.fy,c.fz)
end

local function frameCamera(bounds)
    if not bounds then return end
    state.camera.fx=(bounds.minX+bounds.maxX)*0.5
    state.camera.fy=(bounds.minY+bounds.maxY)*0.5
    state.camera.fz=(bounds.minZ+bounds.maxZ)*0.5
    local dx=bounds.maxX-bounds.minX
    local dy=bounds.maxY-bounds.minY
    local dz=bounds.maxZ-bounds.minZ
    state.camera.distance=math.max(0.1,math.sqrt(dx*dx+dy*dy+dz*dz)*1.4)
    applyCamera()
end

local function faceGuideRelative(name, scale)
    -- Human-face reference preset calibrated from the mapped Megan project.
    local points={
        face_left={-0.3618,0.1413,-0.2554}, face_right={0.3595,0.1413,-0.2554},
        face_top={0,0.6291,-0.2279}, face_bottom={0.1052,-0.4323,-0.1315},
        left_eye_center={-0.1905,0.1697,-0.1346}, right_eye_center={0.1882,0.1697,-0.1346},
        left_eye_corner={-0.2819,0.1455,-0.1914}, right_eye_corner={0.2796,0.1455,-0.1914},
        nose_bridge={0,0.005,0.0789}, nose_tip={-0.0012,0.0021,0.0882},
        mouth_left={-0.1418,-0.1959,-0.0603}, mouth_right={0.1395,-0.1959,-0.0603},
        upper_lip={0,-0.1635,0.0187}, lower_lip={0,-0.2314,0.0034}, chin={0,-0.4175,-0.0558},
        jaw_hinge_left={-0.1193,-0.3522,-0.0601}, jaw_hinge_right={0.1193,-0.3522,-0.0601},
        brow_left={-0.1871,0.2732,-0.1142}, brow_right={0.1848,0.2732,-0.1142}
    }
    local point=points[name] or {0,0,0}
    return {x=point[1]*scale,y=point[2]*scale,z=point[3]*scale}
end

local function faceGuideScale(bounds)
    if not bounds then return 1 end
    local width=math.abs(bounds.maxX-bounds.minX)
    local height=math.abs(bounds.maxY-bounds.minY)
    local depth=math.abs(bounds.maxZ-bounds.minZ)
    local widthSafe=math.max(width,0.0001)
    local shallow=depth/widthSafe<0.35 or height/widthSafe<0.8
    if shallow then
        return math.max(width*0.10,height*0.06,depth*0.25,0.001)
    end
    return math.max(width*0.38,height*0.24,depth*0.38,0.001)
end

local function addPoint(a,b)
    return {x=a.x+b.x,y=a.y+b.y,z=a.z+b.z}
end

local function faceFrameBounds()
    local minX,minY,minZ,maxX,maxY,maxZ
    local function include(point)
        if not point then return end
        minX=minX and math.min(minX,point.x) or point.x
        minY=minY and math.min(minY,point.y) or point.y
        minZ=minZ and math.min(minZ,point.z) or point.z
        maxX=maxX and math.max(maxX,point.x) or point.x
        maxY=maxY and math.max(maxY,point.y) or point.y
        maxZ=maxZ and math.max(maxZ,point.z) or point.z
    end
    local placedCount=0
    for _,landmark in ipairs(state.project.landmarks or {}) do
        if landmark.placed and landmark.position then
            include(landmark.position)
            placedCount=placedCount+1
        end
    end
    local origin=state.project.faceOrigin
    if placedCount==0 and origin and origin.placed and origin.position then
        local scale=math.max(tonumber(origin.scale) or faceGuideScale(state.meshBounds),0.001)
        include(origin.position)
        for _,landmark in ipairs(state.project.landmarks or {}) do
            include(addPoint(origin.position,faceGuideRelative(landmark.name,scale)))
        end
    end
    if not minX then return state.meshBounds end
    local centerX=(minX+maxX)*0.5
    local centerY=(minY+maxY)*0.5
    local centerZ=(minZ+maxZ)*0.5
    local extent=math.max(maxX-minX,maxY-minY,maxZ-minZ)
    local minimumExtent=origin and origin.placed and tonumber(origin.scale) or 0
    extent=math.max(extent,minimumExtent*1.2,0.1)
    local half=extent*0.5
    return {minX=centerX-half,maxX=centerX+half,
        minY=centerY-half,maxY=centerY+half,
        minZ=centerZ-half,maxZ=centerZ+half}
end

local function frameFaceCamera()
    local bounds=faceFrameBounds()
    if not bounds then return end
    local origin=state.project.faceOrigin
    if origin and origin.placed and origin.position then
        local center=origin.position
        local halfX=math.max(math.abs(bounds.minX-center.x),math.abs(bounds.maxX-center.x))
        local halfY=math.max(math.abs(bounds.minY-center.y),math.abs(bounds.maxY-center.y))
        local halfZ=math.max(math.abs(bounds.minZ-center.z),math.abs(bounds.maxZ-center.z))
        bounds={minX=center.x-math.max(halfX,0.05),maxX=center.x+math.max(halfX,0.05),
            minY=center.y-math.max(halfY,0.05),maxY=center.y+math.max(halfY,0.05),
            minZ=center.z-math.max(halfZ,0.05),maxZ=center.z+math.max(halfZ,0.05)}
    end
    -- A face frame is an authoring view: use the front axis so the facial
    -- landmarks and wireframe are centered in the viewport instead of keeping
    -- an arbitrary orbit left by a previous inspection.
    state.camera.azimuth=0
    state.camera.elevation=0
    frameCamera(bounds)
end

local function frameLandmarkCamera()
    local landmark=state.project.landmarks[state.selectedLandmark]
    if not landmark or not landmark.placed or not landmark.position then return end
    local origin=state.project.faceOrigin
    local scale=origin and origin.placed and origin.scale or faceGuideScale(state.meshBounds)
    local radius=math.max(tonumber(scale) or 1,0.05)*0.18
    local point=landmark.position
    frameCamera({minX=point.x-radius,maxX=point.x+radius,
        minY=point.y-radius,maxY=point.y+radius,
        minZ=point.z-radius,maxZ=point.z+radius})
end

local function hideFaceGuide()
    local guide=state.faceGuide
    if guide.wireframe then guide.wireframe.visible=false end
    for _,object in pairs(guide.markers) do object.visible=false end
    for _,object in pairs(guide.axes) do object.visible=false end
    guide.worldPoints={}
    guide.origin=nil
    guide.length=nil
end

local function selectedFaceGuideName()
    if state.workspace=='face_position' then return 'face_origin' end
    local landmark=state.project.landmarks[state.selectedLandmark]
    return landmark and landmark.name or nil
end

local function appendWirePoint(points,point)
    points[#points+1]=point.x; points[#points+1]=point.y; points[#points+1]=point.z
end

local function buildFaceOutline(relative,scale)
    local left,right,top,bottom=relative.face_left,relative.face_right,relative.face_top,relative.face_bottom
    if not left or not right or not top or not bottom then return nil end
    local centerX=(left.x+right.x)*0.5
    local centerY=(top.y+bottom.y)*0.5
    local centerZ=(left.z+right.z+top.z+bottom.z)*0.25
    local radiusX=math.max(math.abs(right.x-left.x)*0.5,scale*0.45)
    local radiusY=math.max(math.abs(top.y-bottom.y)*0.5,scale*0.45)
    local outline={
        {x=centerX,y=top.y,z=top.z},
        {x=centerX+radiusX*0.5,y=centerY+radiusY*0.866,z=centerZ},
        {x=right.x,y=centerY+radiusY*0.35,z=right.z},
        {x=right.x,y=centerY-radiusY*0.35,z=right.z},
        {x=centerX+radiusX*0.5,y=centerY-radiusY*0.866,z=centerZ},
        {x=centerX,y=bottom.y,z=bottom.z},
        {x=centerX-radiusX*0.5,y=centerY-radiusY*0.866,z=centerZ},
        {x=left.x,y=centerY-radiusY*0.35,z=left.z},
        {x=left.x,y=centerY+radiusY*0.35,z=left.z},
        {x=centerX-radiusX*0.5,y=centerY+radiusY*0.866,z=centerZ}
    }
    local points={}
    for _,point in ipairs(outline) do
        appendWirePoint(points,point)
    end
    appendWirePoint(points,outline[1])
    return points
end

local function buildFaceGuidePaths(relative,scale)
    local paths={}
    local outline=buildFaceOutline(relative,scale)
    if outline then paths[#paths+1]=outline end
    for _,path in ipairs(faceGuidePaths) do
        local points={}
        for _,name in ipairs(path) do
            local point=relative[name]
            if point then appendWirePoint(points,point) end
        end
        if #points>=6 then paths[#paths+1]=points end
    end
    return paths
end

local function refreshFaceGuide()
    local guide=state.faceGuide
    local origin=state.project and state.project.faceOrigin
    if (state.workspace~='face_position' and state.workspace~='landmarks') or
            not state.mesh or not origin or not origin.placed then
        hideFaceGuide()
        state.faceGuideDirty=false
        return
    end
    local scale=math.max(tonumber(origin.scale) or faceGuideScale(state.meshBounds),0.001)
    local originPoint={x=origin.position.x,y=origin.position.y,z=origin.position.z}
    local relative={face_origin={x=0,y=0,z=0}}
    for _,landmark in ipairs(state.project.landmarks or {}) do
        relative[landmark.name]=landmark.relativePosition and
            {x=landmark.relativePosition.x,y=landmark.relativePosition.y,z=landmark.relativePosition.z} or
            faceGuideRelative(landmark.name,scale)
    end
    guide.worldPoints={}
    for name,point in pairs(relative) do guide.worldPoints[name]=addPoint(originPoint,point) end

    local wirePaths=buildFaceGuidePaths(relative,scale)
    if not guide.wireframe then
        guide.wireframe=line:new('3d',0,0,0)
        for _,path in ipairs(wirePaths) do guide.wireframe:add(path) end
        guide.wireframe.alwaysOnTop=true
        guide.wireframe.alwaysOnTopPriority=2
    else
        for index,path in ipairs(wirePaths) do guide.wireframe:set(path,index) end
    end
    guide.wireframe:setPos(originPoint.x,originPoint.y,originPoint.z)
    guide.wireframe:setColor(0.25,0.8,1,0.9)
    guide.wireframe.visible=true

    local pointSize=math.max(scale*0.045,0.003)
    local selectedName=selectedFaceGuideName()
    for name,point in pairs(relative) do
        local marker=guide.markers[name]
        local paths={
            {-pointSize,0,0,pointSize,0,0},
            {0,-pointSize,0,0,pointSize,0},
            {0,0,-pointSize,0,0,pointSize}
        }
        if not marker then
            marker=line:new('3d',0,0,0)
            for _,path in ipairs(paths) do marker:add(path) end
            marker.alwaysOnTop=true
            marker.alwaysOnTopPriority=3
            guide.markers[name]=marker
        else
            for index,path in ipairs(paths) do marker:set(path,index) end
        end
        local world=guide.worldPoints[name]
        marker:setPos(world.x,world.y,world.z)
        if name==selectedName then marker:setColor(1,0.78,0.12,1)
        elseif name=='face_origin' then marker:setColor(1,0.35,0.85,1)
        else marker:setColor(0.2,0.85,1,0.95) end
        marker.visible=true
    end
    for name,axis in pairs({x={x=1,y=0,z=0},y={x=0,y=1,z=0},z={x=0,y=0,z=1}}) do
        local object=guide.axes[name]
        local length=math.max(scale*0.65,0.01)
        local points={0,0,0,axis.x*length,axis.y*length,axis.z*length}
        if not object then
            object=line:new('3d',0,0,0)
            object:add(points)
            object.alwaysOnTop=true
            object.alwaysOnTopPriority=4
            guide.axes[name]=object
        else
            object:set(points,1)
        end
        local gizmoOrigin=guide.worldPoints[selectedName or 'face_origin'] or originPoint
        object:setPos(gizmoOrigin.x,gizmoOrigin.y,gizmoOrigin.z)
        local color=name=='x' and {1,0.15,0.15,1} or name=='y' and {0.15,1,0.2,1} or {0.2,0.45,1,1}
        object:setColor(table.unpack(color))
        object.visible=true
        guide.length=length
    end
    guide.origin=guide.worldPoints[selectedName or 'face_origin'] or originPoint
    state.faceGuideDirty=false
end

local function hideRigPreview()
    for _,object in ipairs(state.rigPreview.objects or {}) do object.visible=false end
end

local function rigBoneColor(kind)
    if kind=='root' then return {1,0.35,0.85,1} end
    if kind=='eye' then return {0.25,0.9,1,1} end
    if kind=='jaw' then return {1,0.55,0.2,1} end
    if kind=='mouth' then return {1,0.3,0.45,1} end
    if kind=='brow' then return {0.75,0.45,1,1} end
    return {0.3,1,0.45,1}
end

local function refreshRigPreview()
    local preview=state.rigPreview
    if state.workspace~='rig' or not state.mesh then
        hideRigPreview()
        preview.descriptor=nil
        preview.dirty=false
        return
    end
    local descriptor=tFacialProject.rigDescription(state.project,state.topology)
    preview.descriptor=descriptor
    for index,bone in ipairs(descriptor.bones or {}) do
        local object=preview.objects[index]
        local dx,dy,dz=bone.tail.x-bone.head.x,bone.tail.y-bone.head.y,bone.tail.z-bone.head.z
        if not object then
            object=line:new('3d',0,0,0)
            object.alwaysOnTop=true
            object.alwaysOnTopPriority=5
            preview.objects[index]=object
        end
        object:set({0,0,0,dx,dy,dz},1)
        object:setPos(bone.head.x,bone.head.y,bone.head.z)
        local color=rigBoneColor(bone.kind)
        object:setColor(table.unpack(color))
        object.visible=descriptor.valid
    end
    for index=#(descriptor.bones or {})+1,#preview.objects do
        preview.objects[index].visible=false
    end
    preview.dirty=false
end

local function refreshBindingPlan(force)
    if (not force and state.workspace~='rig') or not state.mesh then return end
    local rig=tFacialProject.rigDescription(state.project,state.topology)
    local review=tFacialProject.bindingReview(state.project,state.topology)
    state.bindingPlan.data=tFacialBindingPlan.build(state.project,state.topology,rig,review)
    state.bindingPlan.dirty=false
    state.weightAssignment.dirty=true
end

local function refreshRigReview()
    if not state.mesh or not state.topology then
        state.rigReview.data=nil
        state.rigReview.dirty=false
        return
    end
    local report=tFacialProject.validate(state.project,true,state.topology)
    local rig=tFacialProject.rigDescription(state.project,state.topology)
    local binding=tFacialProject.bindingDescription(state.project,state.topology)
    state.rigReview.data={report=report,rig=rig,binding=binding,
        bridgeReport=tFacialBridge.bindingReport(state.project,rig,binding),
        bindingReview=tFacialProject.bindingReview(state.project,state.topology)}
    state.rigReview.dirty=false
end

local function refreshFacialSkeletonBinding(force)
    if (not force and state.workspace~='rig') or not state.meshDebug then return end
    local rig=tFacialProject.rigDescription(state.project,state.topology)
    state.facialSkeleton.data=tFacialSkeletonBinding.inspect(state.meshDebug,rig)
    state.facialSkeleton.dirty=false
end

local function applyFacialSkeletonBinding()
    if state.facialSkeleton.dirty then refreshFacialSkeletonBinding(true) end
    local binding=state.facialSkeleton.data
    if not binding or binding.valid~=true then
        setStatus(tLang.L('fre_facial_skeleton_blocked') .. ': ' ..
            localizedFacialSkeletonReason(binding and binding.errors and binding.errors[1] or ''),true)
        return false
    end
    local rig=tFacialProject.rigDescription(state.project,state.topology)
    local ok,count,errorMessage=tFacialSkeletonBinding.apply(state.meshDebug,rig)
    if not ok then
        setStatus(tLang.L('fre_facial_skeleton_failed') .. ': ' .. tostring(errorMessage or ''),true)
        return false
    end
    local repairedRoot=binding.rootParentNeedsRepair==true
    state.facialSkeleton.dirty=true
    state.bindingPlan.dirty=true
    state.weightAssignment.dirty=true
    state.weightAssignment.modified=true
    state.weightAssignment.facialWeightsApplied=false
    if repairedRoot then
        setStatus(tLang.L('fre_facial_skeleton_root_repaired'),false)
    else
        setStatus(string.format(tLang.L('fre_facial_skeleton_applied_fmt'),count),false)
    end
    return true
end

local function refreshWeightAssignment(force)
    if (not force and state.workspace~='rig') or not state.meshDebug then return end
    if state.bindingPlan.dirty then refreshBindingPlan(force) end
    if state.facialSkeleton.dirty then refreshFacialSkeletonBinding(force) end
    if state.facialSkeleton.data and state.facialSkeleton.data.rootParentNeedsRepair then
        state.weightAssignment.data={version=1,valid=false,entries={},readyCount=0,blockedCount=0,
            totalVertexCount=0,errors={'facial root is not attached to the body head'},warnings={}}
        state.weightAssignment.dirty=false
        return
    end
    local rig=tFacialProject.rigDescription(state.project,state.topology)
    state.weightAssignment.data=tFacialWeightAssignment.inspect(state.meshDebug,state.project,state.topology,
        state.bindingPlan.data,rig)
    state.weightAssignment.dirty=false
end

local function applyRigidFacialWeights()
    if state.weightAssignment.dirty then refreshWeightAssignment(true) end
    local assignment=state.weightAssignment.data
    if not assignment or assignment.valid~=true then
        local reason=assignment and assignment.errors and assignment.errors[1] or
            'explicit weight assignment is not ready'
        setStatus(tLang.L('fre_weight_assignment_blocked') .. ': ' .. tostring(reason),true)
        return false
    end
    local ok,count,errorMessage=tFacialWeightAssignment.apply(state.meshDebug,assignment)
    if not ok then
        setStatus(tLang.L('fre_weight_assignment_failed') .. ': ' .. tostring(errorMessage or ''),true)
        return false
    end
    state.weightAssignment.modified=true
    state.weightAssignment.facialWeightsApplied=true
    state.weightAssignment.dirty=true
    setStatus(string.format(tLang.L('fre_weight_assignment_applied_fmt'),count),false)
    return true
end

local function materializeProjectFacialClips()
    if not state.meshDebug or state.exportWindow.includeProjectFacialClips==false then return true end
    local clips=state.project.clips or {}
    if #clips==0 then return true end
    if state.expressions.catalogDirty then refreshExpressionCatalog() end
    local targetNames={}
    for _,entry in ipairs(state.expressions.catalog or {}) do targetNames[#targetNames+1]=entry.name end
    if #targetNames==0 then
        setStatus(tLang.L('fre_export_clips_blocked_no_targets'),true)
        return false
    end
    local function findEmbeddedClipIndex(clipName)
        local ok,total=dpCall(function() return state.meshDebug:getTotalFacialClips() end)
        if not ok then return nil end
        for index=1,tonumber(total) or 0 do
            local nameOk,name=dpCall(function() return state.meshDebug:getFacialClipName(index) end)
            if nameOk and name==clipName then return index end
        end
        return nil
    end
    for _,clip in ipairs(clips) do
        local validation=tFacialClips.validate(clip)
        if validation.ok~=true then
            setStatus(string.format(tLang.L('fre_export_clips_blocked_invalid_fmt'),clip.name or ''),true)
            return false
        end
        -- Query the current list for every replacement. Removing an earlier clip
        -- shifts subsequent 1-based indices, so a cached name->index map is unsafe.
        local oldIndex=findEmbeddedClipIndex(clip.name)
        if oldIndex then
            local removed,removedOk=dpCall(function() return state.meshDebug:removeFacialClip(oldIndex) end)
            if not removed or removedOk==false then
                setStatus(string.format(tLang.L('fre_export_clips_failed_fmt'),clip.name or ''),true)
                return false
            end
        end
        local keys={}
        for _,key in ipairs(clip.keys or {}) do
            local weights={}
            for _,targetName in ipairs(targetNames) do
                weights[#weights+1]=tonumber(key.weights and key.weights[targetName] or 0) or 0
            end
            keys[#keys+1]={time=key.time,weights=weights}
        end
        local added,clipIndex=dpCall(function()
            return state.meshDebug:addFacialClip(clip.name,clip.duration,clip.loop==true,
                clip.interpolation==0 and 0 or 1,keys)
        end)
        if not added or not clipIndex then
            setStatus(string.format(tLang.L('fre_export_clips_failed_fmt'),clip.name or ''),true)
            return false
        end
    end
    return true
end

local function canExportRuntimeMsh()
    if not state.meshDebug then return false end
    if state.targetAuthoring and state.targetAuthoring.modified==true then
        return true
    end
    if state.weightAssignment.modified==true and state.weightAssignment.facialWeightsApplied==true then
        return true
    end
    -- Once a weighted/target-bearing MSH is already loaded, a newly authored project
    -- clip can be exported without reapplying unchanged weights.
    if state.exportWindow.includeProjectFacialClips==false or #(state.project.clips or {})==0 then
        return false
    end
    local ok,targetCount=dpCall(function() return state.meshDebug:getTotalFacialTargets() end)
    return ok and (tonumber(targetCount) or 0)>0
end

local function saveAssignedMesh()
    if not canExportRuntimeMsh() then return false end
    if not materializeProjectFacialClips() then return false end
    local suggested=replaceExtension(state.project.meshPath or 'facial-rig','-facial-weighted.msh')
    local path=mbm.saveFile(suggested,'msh')
    if not path then return false end
    local callOk,saved=dpCall(function() return state.meshDebug:save(path,false,false,false) end)
    if not callOk or saved~=true then
        setStatus(tLang.L('fre_weight_assignment_save_failed'),true)
        return false
    end
    -- Keep the authoring project attached to the derived MSH in memory. Facial
    -- targets, clips, skeleton and weights are now serialized in the binary MSH;
    -- saving the project afterward persists this new asset path as well.
    state.project.meshPath=path
    state.modified=true
    state.weightAssignment.modified=false
    state.weightAssignment.facialWeightsApplied=false
    if state.targetAuthoring then
        state.targetAuthoring.modified=false
        state.targetAuthoring.previewDirty=true
        state.targetAuthoring.createdIds={}
        state.targetAuthoring.deletedBaseIds={}
    end
    state.project.morphTargetEdits=nil
    setStatus(string.format(tLang.L('fre_weight_assignment_saved_fmt'),shortName(path)),false)
    return true
end

local function inspectMesh(meshDebugObject)
    local ok,topology=dpCall(function() return tFacialLandmarks.inspect(meshDebugObject) end)
    if not ok or not topology or not topology.bounds then return nil, 0, nil end
    topology.bounds.total=topology.vertexCount
    return topology.bounds,topology.vertexCount,topology
end

local function faceRegionVertices()
    if not state.meshDebug or not state.topology then return nil,'mesh is not loaded' end
    local points={}
    for _,landmark in ipairs(state.project.landmarks or {}) do
        if landmark.placed and landmark.position then points[#points+1]=landmark.position end
    end
    if #points<3 then return nil,'facial landmarks are not placed' end
    local minX,maxX,minY,maxY,minZ,maxZ=math.huge,-math.huge,math.huge,-math.huge,math.huge,-math.huge
    for _,point in ipairs(points) do
        minX=math.min(minX,point.x); maxX=math.max(maxX,point.x)
        minY=math.min(minY,point.y); maxY=math.max(maxY,point.y)
        minZ=math.min(minZ,point.z); maxZ=math.max(maxZ,point.z)
    end
    local origin=state.project.faceOrigin
    local scale=math.max(tonumber(origin and origin.scale) or faceGuideScale(state.meshBounds),0.001)
    local xMargin,yMargin,zMargin=scale*0.18,scale*0.20,scale*0.75
    minX,maxX=minX-xMargin,maxX+xMargin
    minY,maxY=minY-yMargin,maxY+yMargin
    minZ,maxZ=minZ-zMargin,maxZ+zMargin
    -- Use the topology snapshot that was produced by facial_rig_landmarks.inspect().
    -- Its order is the canonical global order used by project masks (subset-local
    -- vertex calls concatenated in subset order).  Re-reading ranges through the
    -- MeshDebug binding here used to make it possible for a malformed/partial Lua
    -- table to produce an out-of-range or repeated project index.
    local vertices,seen={},{}
    for index,point in ipairs(state.topology.vertices or {}) do
        if point.x>=minX and point.x<=maxX and point.y>=minY and point.y<=maxY and
                point.z>=minZ and point.z<=maxZ and not seen[index] then
            seen[index]=true
            vertices[#vertices+1]=index
        end
    end
    table.sort(vertices)
    if #vertices==0 then return nil,'face landmark bounds selected no mesh vertices' end
    return vertices,nil
end

local function generateMainFaceMask()
    local roleIndex
    for index,role in ipairs(state.project.roles or {}) do
        if role.name=='main_face' then roleIndex=index break end
    end
    if not roleIndex then return false end
    local vertices,errorMessage=faceRegionVertices()
    if not vertices then
        setStatus(tLang.L('fre_face_mask_generation_failed') .. ': ' .. tostring(errorMessage or ''),true)
        return false
    end
    local selectedMask
    for index,mask in ipairs(state.project.masks or {}) do
        if (mask.roleIndex or 1)==roleIndex and mask.enabled~=false then
            selectedMask=mask; state.selectedMask=index; break
        end
    end
    pushUndoSnapshot()
    if not selectedMask then
        tFacialProject.addMask(state.project,'main_face_region',roleIndex)
        state.selectedMask=#state.project.masks
        selectedMask=state.project.masks[state.selectedMask]
    end
    selectedMask.vertices=vertices
    selectedMask.topologySignature=state.topology.signature
    selectedMask.enabled=true
    markMaskVisualizationDirty()
    markModified()
    setStatus(string.format(tLang.L('fre_face_mask_generated_fmt'),#vertices),false)
    return true
end

local function refreshLandmarkMarkers()
    if not state.topology or not state.mesh then return end
    local bounds=state.meshBounds or {minX=0,maxX=1,minY=0,maxY=1,minZ=0,maxZ=1}
    local extent=math.sqrt((bounds.maxX-bounds.minX)^2+(bounds.maxY-bounds.minY)^2+
        (bounds.maxZ-bounds.minZ)^2)
    local size=math.max(extent*0.012,0.001)
    local landmarks=state.project.landmarks or {}
    for index,landmark in ipairs(landmarks) do
        local marker=state.landmarkMarkers[index]
        local markerPaths={
            {-size,0,0,size,0,0},
            {0,-size,0,0,size,0},
            {0,0,-size,0,0,size}
        }
        if not marker then
            marker=line:new('3d',0,0,0)
            for _,path in ipairs(markerPaths) do marker:add(path) end
            marker.alwaysOnTop=true
            marker.alwaysOnTopPriority=1
            state.landmarkMarkers[index]=marker
        else
            for pathIndex,path in ipairs(markerPaths) do marker:set(path,pathIndex) end
        end
        local originPlaced=state.project.faceOrigin and state.project.faceOrigin.placed
        local visible=state.workspace=='landmarks' and state.mesh.visible and landmark.placed and landmark.position and not originPlaced
        marker.visible=visible and true or false
        if landmark.position then marker:setPos(landmark.position.x,landmark.position.y,landmark.position.z) end
        if index==state.selectedLandmark then marker:setColor(1,0.8,0.1,1)
        else marker:setColor(0.2,0.8,1,0.9) end
    end
    state.landmarkMarkersDirty=false
end

local function markLandmarkMarkersDirty()
    state.landmarkMarkersDirty=true
    state.maskBrushReference={key=nil,extent=nil}
end

local function markFaceGuideDirty()
    state.faceGuideDirty=true
end

local function refreshExpressionCatalog()
    if not state.mesh then
        state.expressions.catalog={}
        state.expressions.catalogDirty=false
        return
    end
    local ok,catalog=dpCall(function() return tFacialExpressions.catalogFromMesh(state.mesh) end)
    if not ok or type(catalog)~='table' then catalog={} end
    state.expressions.catalog=catalog
    state.project.expressions=tFacialExpressions.syncCatalog(state.project.expressions,catalog)
    state.expressions.selectedTarget=math.max(1,math.min(state.expressions.selectedTarget,#catalog>0 and #catalog or 1))
    state.expressions.selectedPose=math.max(1,math.min(state.expressions.selectedPose,#(state.project.expressions.poses or {})>0 and #(state.project.expressions.poses or {}) or 1))
    state.expressions.catalogDirty=false
end

local function refreshFacialTargetCatalog()
    local authoring=state.targetAuthoring
    if not state.meshDebug then
        authoring.targets={}
        authoring.vertexCount=0
        authoring.catalogDirty=false
        return
    end
    local ok,catalog,vertexCount=dpCall(function()
        return tFacialTargets.catalogFromDebug(state.meshDebug)
    end)
    if not ok or type(catalog)~='table' then catalog={} end
    authoring.targets=catalog
    authoring.vertexCount=math.max(tonumber(vertexCount) or 0,tonumber(state.meshVertexCount) or 0)
    authoring.selectedTarget=math.max(1,math.min(authoring.selectedTarget or 1,#catalog>0 and #catalog or 1))
    authoring.draftTargetIndex=nil
    authoring.catalogDirty=false
    state.targetOverlay.dirty=true
end

local function markFacialTargetAuthoringModified()
    state.targetAuthoring.modified=true
    state.targetAuthoring.previewDirty=true
    state.targetAuthoring.catalogDirty=true
    state.targetOverlay.dirty=true
    refreshFacialTargetCatalog()
    state.expressions.catalog=state.targetAuthoring.targets
    state.project.expressions=tFacialExpressions.syncCatalog(state.project.expressions,state.expressions.catalog)
    state.expressions.selectedTarget=math.max(1,math.min(state.expressions.selectedTarget,#state.expressions.catalog>0 and #state.expressions.catalog or 1))
    state.expressions.catalogDirty=false
    state.modified=true
end

local function targetAuthoringError(message)
    setStatus(tLang.L('fre_target_authoring_failed') .. ': ' .. tostring(message or ''),true)
end

local function updateTargetPreviewWeights()
    local authoring=state.targetAuthoring
    if not state.mesh or not authoring or authoring.previewDirty then return end
    for index=1,#authoring.targets do
        local weight=index==authoring.selectedTarget and (authoring.previewWeight or 1) or 0
        dpCall(function() return state.mesh:setFacialWeight(index,weight) end)
    end
    authoring.previewTarget=authoring.selectedTarget
    setTargetOverlayVisible(true)
end

local function refreshTargetPreview()
    local authoring=state.targetAuthoring
    if not authoring or not authoring.previewDirty or not state.meshDebug or not state.mesh then return end
    local path=tUtil.getTemporaryFilePath('_facial_target_preview.msh')
    local savedOk,saved=dpCall(function() return state.meshDebug:save(path,false,false,true) end)
    if not savedOk or saved~=true then
        os.remove(path)
        targetAuthoringError(tLang.L('fre_target_preview_failed'))
        authoring.previewDirty=false
        return
    end
    local candidate=mesh:new('3d')
    local loadedOk,loaded=dpCall(function()
        return candidate:setSkeletalSkinningMethod('auto') and meshDebug:loadMeshPreview(candidate,path)
    end)
    if not loadedOk or loaded~=true then
        candidate:destroy()
        dpCall(function() meshDebug:fakeRelease(path) end)
        os.remove(path)
        targetAuthoringError(tLang.L('fre_target_preview_failed'))
        authoring.previewDirty=false
        return
    end
    dpCall(function() candidate:stopSkeletalAnimation() end)
    candidate.alwaysRender=true
    candidate.visible=state.targetOverlay.meshHidden and state.targetOverlay.meshWasVisible or state.mesh.visible
    local old=state.mesh
    if state.facialComparison and state.facialComparison.mesh then
        state.facialComparison.mesh.alwaysRender=false
        dpCall(function() state.facialComparison.mesh:destroy() end)
        state.facialComparison={mesh=nil,enabled=false,ready=false,layoutActive=false}
    end
    candidate:setPos(0,0,0)
    destroyTargetOverlay()
    old.alwaysRender=false
    dpCall(function() old:destroy() end)
    if authoring.previewPath then
        dpCall(function() meshDebug:fakeRelease(authoring.previewPath) end)
        os.remove(authoring.previewPath)
    end
    state.mesh=candidate
    authoring.previewPath=path
    authoring.previewDirty=false
    authoring.previewTarget=nil
    state.preview.skeletalDirty=true
    state.preview.activeSkeletal=nil
    state.preview.authoredClipInitialized=false
    updateTargetPreviewWeights()
end

local function showTargets()
    local authoring=state.targetAuthoring
    if authoring.catalogDirty then refreshFacialTargetCatalog() end
    if not state.meshDebug then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    tImGui.TextWrapped(tLang.L('fre_targets_help'))
    if #authoring.targets==0 then authoring.mode=1 end
    local workMode=authoring.mode or 1
    workMode=tImGui.RadioButton(tLang.L('fre_target_mode_add'),workMode,1)
    showHelpTooltip(tLang.L('fre_target_mode_add_tooltip'))
    tImGui.SameLine()
    tImGui.BeginDisabled(#authoring.targets==0)
    workMode=tImGui.RadioButton(tLang.L('fre_target_mode_edit'),workMode,2)
    showHelpTooltip(tLang.L('fre_target_mode_edit_tooltip'))
    tImGui.SameLine()
    workMode=tImGui.RadioButton(tLang.L('fre_target_mode_paint'),workMode,3)
    showHelpTooltip(tLang.L('fre_target_mode_paint_tooltip'))
    tImGui.EndDisabled()
    if workMode~=authoring.mode then state.targetBrushHover.lastHit=nil end
    authoring.mode=workMode
    tImGui.Separator()
    local purposeKey=workMode==1 and 'fre_target_add_purpose' or
        workMode==2 and 'fre_target_edit_purpose' or 'fre_target_paint_purpose'
    tImGui.TextWrapped(tLang.L(purposeKey))
    tImGui.Separator()
    if workMode==1 then
        tImGui.Text(string.format(tLang.L('fre_targets_vertex_count_fmt'),authoring.vertexCount or 0))
        tImGui.Text(string.format(tLang.L('fre_targets_count_fmt'),#authoring.targets))
        tImGui.SetNextItemWidth(300)
        local nameChanged,name=tImGui.InputText(tLang.L('fre_target_name')..'##freNewTargetName',
            authoring.newNameDraft or '')
        showHelpTooltip(tLang.L('fre_target_name_tooltip'))
        if nameChanged then authoring.newNameDraft=name end
        tImGui.SetNextItemWidth(300)
        local conventionChanged,_,convention=tImGui.Combo(
            tLang.L('fre_target_convention')..'##freNewTargetConvention',
            tFacialTargets.conventionIndex(authoring.newConventionDraft),tFacialTargets.conventions)
        showHelpTooltip(tLang.L('fre_target_convention_tooltip'))
        if conventionChanged then authoring.newConventionDraft=convention end
        tImGui.SetNextItemWidth(300)
        local weightChanged,weight=tImGui.SliderFloat(
            tLang.L('fre_target_default_weight')..'##freNewTargetDefaultWeight',
            authoring.newDefaultWeight or 0,0,1,'%.3f')
        showHelpTooltip(tLang.L('fre_target_default_weight_tooltip'))
        if weightChanged then authoring.newDefaultWeight=weight end
        if tImGui.Button(tLang.L('fre_target_create')) then
            local targetName=(authoring.newNameDraft or ''):match('^%s*(.-)%s*$')
            if targetName=='' then
                targetAuthoringError(tLang.L('fre_target_name_required'))
            else
                local positions=tFacialTargets.zeroDeltas(authoring.vertexCount)
                local callOk,newIndex=dpCall(function()
                    return state.meshDebug:addFacialTarget(targetName,authoring.newConventionDraft or '',
                        authoring.newDefaultWeight or 0,positions)
                end)
                if callOk and newIndex then
                    markFacialTargetAuthoringModified()
                    local created=tFacialTargets.find(authoring.targets,newIndex)
                    if created then authoring.createdIds[created.id]=true end
                    tFacialTargets.clearEditHistory(authoring)
                    authoring.selectedTarget=newIndex
                    authoring.newNameDraft=''
                    authoring.mode=3
                    state.targetOverlay.dirty=true
                    setStatus(tLang.L('fre_target_created'),false)
                else
                    targetAuthoringError('could not create target')
                end
            end
        end
        showHelpTooltip(tLang.L('fre_target_create_tooltip'))
        if authoring.modified and state.modified then
            tImGui.TextWrapped(tLang.L('fre_targets_unsaved_help'))
        end
        return
    end
    tImGui.Text(string.format(tLang.L('fre_targets_vertex_count_fmt'),authoring.vertexCount or 0))
    if #authoring.targets==0 then
        tImGui.TextDisabled(tLang.L('fre_targets_no_targets'))
    else
        tImGui.Text(string.format(tLang.L('fre_targets_count_fmt'),#authoring.targets))
        if tImGui.BeginTable('freFacialTargets',2,tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
            tImGui.TableSetupColumn(tLang.L('fre_target_name_column'))
            tImGui.TableSetupColumn(tLang.L('fre_target_convention_column'))
            tImGui.TableHeadersRow()
            for index,target in ipairs(authoring.targets) do
                tImGui.TableNextRow(); tImGui.TableSetColumnIndex(0)
                if tImGui.Selectable(target.name,index==authoring.selectedTarget) then
                    authoring.selectedTarget=index
                    authoring.draftTargetIndex=nil
                    state.targetOverlay.dirty=true
                    state.modified=true
                    updateTargetPreviewWeights()
                end
                tImGui.TableSetColumnIndex(1)
                tImGui.Text(target.convention~='' and target.convention or '-')
            end
            tImGui.EndTable()
        end
    end

    local selected=tFacialTargets.find(authoring.targets,authoring.selectedTarget)
    if selected then
        tImGui.SetNextItemWidth(300)
        local changed,value=tImGui.SliderFloat(tLang.L('fre_target_preview_weight')..'##freTargetPreviewWeight',
            authoring.previewWeight or 1,0,1,'%.2f')
        showHelpTooltip(tLang.L('fre_target_preview_weight_tooltip'))
        if changed then
            authoring.previewWeight=value
            state.targetOverlay.dirty=true
            state.modified=true
            updateTargetPreviewWeights()
        end
        if authoring.previewTarget~=authoring.selectedTarget then updateTargetPreviewWeights() end
    end
    local overlayVisible=tImGui.Checkbox(tLang.L('fre_target_overlay'),state.targetOverlay.visible)
    showHelpTooltip(tLang.L('fre_target_overlay_tooltip'))
    if overlayVisible~=state.targetOverlay.visible then
        state.targetOverlay.visible=overlayVisible
        state.targetOverlay.dirty=true
        setTargetOverlayVisible(true)
    end
    if workMode==2 then
        if selected and authoring.draftTargetIndex~=authoring.selectedTarget then
            authoring.nameDraft=selected.name
            authoring.conventionDraft=selected.convention or ''
            authoring.defaultWeight=selected.defaultWeight or 0
            authoring.draftTargetIndex=authoring.selectedTarget
        end
        tImGui.Separator()
        tImGui.Text(tLang.L('fre_target_metadata_title'))
        tImGui.SetNextItemWidth(300)
        local nameChanged,name=tImGui.InputText(tLang.L('fre_target_name')..'##freTargetName',authoring.nameDraft or '')
        showHelpTooltip(tLang.L('fre_target_name_tooltip'))
        if nameChanged then authoring.nameDraft=name end
        tImGui.SetNextItemWidth(300)
        local conventionChanged,_,convention=tImGui.Combo(
            tLang.L('fre_target_convention')..'##freTargetConvention',
            tFacialTargets.conventionIndex(authoring.conventionDraft),tFacialTargets.conventions)
        showHelpTooltip(tLang.L('fre_target_convention_tooltip'))
        if conventionChanged then authoring.conventionDraft=convention end
        if tFacialTargets.conventionIndex(authoring.conventionDraft)==0 then
            tImGui.TextWrapped(string.format(tLang.L('fre_target_convention_existing_fmt'),
                authoring.conventionDraft~='' and authoring.conventionDraft or '-'))
        end
        tImGui.SetNextItemWidth(300)
        local weightChanged,weight=tImGui.SliderFloat(tLang.L('fre_target_default_weight')..'##freTargetDefaultWeight',authoring.defaultWeight or 0,0,1,'%.3f')
        showHelpTooltip(tLang.L('fre_target_default_weight_tooltip'))
        if weightChanged then authoring.defaultWeight=weight end
        tImGui.BeginDisabled(selected==nil)
        if tImGui.Button(tLang.L('fre_target_update')) then
            local targetName=(authoring.nameDraft or ''):match('^%s*(.-)%s*$')
            local data=tFacialTargets.copyTargetData(selected)
            local before=tFacialTargets.copyTargetData(selected)
            if targetName=='' then
                targetAuthoringError(tLang.L('fre_target_name_required'))
            elseif data then
                data.name=targetName
                data.convention=authoring.conventionDraft or ''
                data.defaultWeight=authoring.defaultWeight or 0
                local callOk,updated=dpCall(function()
                    return state.meshDebug:updateFacialTarget(authoring.selectedTarget,targetName,
                        authoring.conventionDraft or '',authoring.defaultWeight or 0,
                        data.positionDeltas,#data.normalDeltas>0 and data.normalDeltas or nil)
                end)
                if callOk and updated~=false then
                    tFacialTargets.recordEdit(authoring,authoring.selectedTarget,before,data)
                    markFacialTargetAuthoringModified()
                    setStatus(tLang.L('fre_target_updated'),false)
                else
                    targetAuthoringError('could not update target')
                end
            end
        end
        showHelpTooltip(tLang.L('fre_target_update_tooltip'))
        tImGui.EndDisabled()
    end

    tImGui.Separator()
    if workMode==2 then
        tImGui.Text(tLang.L('fre_target_edit_title'))
        tImGui.TextWrapped(tLang.L('fre_target_edit_help'))
        tImGui.Text(tLang.L('fre_target_edit_mode'))
        local mode=authoring.editMode or 1
        mode=tImGui.RadioButton(tLang.L('fre_target_edit_translate')..'##freTargetEditTranslate',mode,1)
        showHelpTooltip(tLang.L('fre_target_edit_translate_tooltip'))
        tImGui.SameLine()
        mode=tImGui.RadioButton(tLang.L('fre_target_edit_rotate')..'##freTargetEditRotate',mode,2)
        showHelpTooltip(tLang.L('fre_target_edit_rotate_tooltip'))
        tImGui.SameLine()
        mode=tImGui.RadioButton(tLang.L('fre_target_edit_reset')..'##freTargetEditReset',mode,3)
        showHelpTooltip(tLang.L('fre_target_edit_reset_tooltip'))
        authoring.editMode=mode
    end
    local selectionLabels={tLang.L('fre_target_edit_all'),tLang.L('fre_target_edit_picked')}
    for index,mask in ipairs(state.project.masks or {}) do
        selectionLabels[#selectionLabels+1]=string.format(tLang.L('fre_target_edit_mask_fmt'),mask.name or ('#'..tostring(index)))
    end
    authoring.selectionMode=math.max(1,math.min(authoring.selectionMode or 1,#selectionLabels))
    tImGui.SetNextItemWidth(300)
    local selectionChanged,selection=tImGui.Combo(tLang.L('fre_target_edit_selection')..'##freTargetEditSelection',
        authoring.selectionMode or 1,selectionLabels,-1)
    showHelpTooltip(tLang.L('fre_target_edit_selection_tooltip'))
    if selectionChanged then authoring.selectionMode=selection end
    tImGui.TextWrapped(tLang.L(workMode==2 and 'fre_target_edit_pick_help' or 'fre_target_paint_help'))
    if authoring.selectedVertex then
        tImGui.Text(string.format(tLang.L('fre_target_picked_fmt'),authoring.selectedVertex))
    end
    tImGui.Separator()
    if workMode==3 then
        tImGui.Text(tLang.L('fre_target_brush_title'))
        local brushMode=authoring.brushMode or 1
        brushMode=tImGui.RadioButton(tLang.L('fre_target_brush_move'),brushMode,1)
        showHelpTooltip(tLang.L('fre_target_brush_move_tooltip'))
        tImGui.SameLine()
        brushMode=tImGui.RadioButton(tLang.L('fre_target_brush_smooth'),brushMode,2)
        showHelpTooltip(tLang.L('fre_target_brush_smooth_tooltip'))
        authoring.brushMode=brushMode
        tImGui.SetNextItemWidth(150)
        local radiusChanged,radius=tImGui.SliderFloat(tLang.L('fre_target_brush_radius'),authoring.brushRadius,0.001,0.15,'%.3f')
        showHelpTooltip(tLang.L('fre_target_brush_radius_tooltip'))
        if radiusChanged then authoring.brushRadius=radius end
        tImGui.SetNextItemWidth(150)
        local strengthChanged,strength=tImGui.SliderFloat(tLang.L('fre_target_brush_strength'),authoring.brushStrength,0.01,1,'%.2f')
        showHelpTooltip(tLang.L('fre_target_brush_strength_tooltip'))
        if strengthChanged then authoring.brushStrength=strength end
        if brushMode==1 then
            for _,axis in ipairs({{'X','brushDx'},{'Y','brushDy'},{'Z','brushDz'}}) do
                tImGui.SetNextItemWidth(115)
                local changed,value=tImGui.InputFloat(axis[1]..'##freTargetBrush'..axis[1],authoring[axis[2]] or 0,0.001,0.01,'%.4f')
                showHelpTooltip(string.format(tLang.L('fre_target_brush_axis_tooltip_fmt'),axis[1]))
                if changed then authoring[axis[2]]=value end
            end
        end
    end
    if workMode==2 then
        if authoring.editMode==1 then
            tImGui.SetNextItemWidth(115)
            local changed,value=tImGui.InputFloat(tLang.L('fre_target_edit_dx')..'##freTargetEditDx',authoring.dx or 0,0.001,0.01,'%.5f')
            showHelpTooltip(string.format(tLang.L('fre_target_batch_axis_tooltip_fmt'),'X'))
            if changed then authoring.dx=value end
            tImGui.SetNextItemWidth(115)
            changed,value=tImGui.InputFloat(tLang.L('fre_target_edit_dy')..'##freTargetEditDy',authoring.dy or 0,0.001,0.01,'%.5f')
            showHelpTooltip(string.format(tLang.L('fre_target_batch_axis_tooltip_fmt'),'Y'))
            if changed then authoring.dy=value end
            tImGui.SetNextItemWidth(115)
            changed,value=tImGui.InputFloat(tLang.L('fre_target_edit_dz')..'##freTargetEditDz',authoring.dz or 0,0.001,0.01,'%.5f')
            showHelpTooltip(string.format(tLang.L('fre_target_batch_axis_tooltip_fmt'),'Z'))
            if changed then authoring.dz=value end
        elseif authoring.editMode==2 then
            tImGui.SetNextItemWidth(115)
            local changed,value=tImGui.InputFloat(tLang.L('fre_target_edit_rx')..'##freTargetEditRx',authoring.rx or 0,1,10,'%.2f')
            showHelpTooltip(string.format(tLang.L('fre_target_rotation_axis_tooltip_fmt'),'X'))
            if changed then authoring.rx=value end
            tImGui.SetNextItemWidth(115)
            changed,value=tImGui.InputFloat(tLang.L('fre_target_edit_ry')..'##freTargetEditRy',authoring.ry or 0,1,10,'%.2f')
            showHelpTooltip(string.format(tLang.L('fre_target_rotation_axis_tooltip_fmt'),'Y'))
            if changed then authoring.ry=value end
            tImGui.SetNextItemWidth(115)
            changed,value=tImGui.InputFloat(tLang.L('fre_target_edit_rz')..'##freTargetEditRz',authoring.rz or 0,1,10,'%.2f')
            showHelpTooltip(string.format(tLang.L('fre_target_rotation_axis_tooltip_fmt'),'Z'))
            if changed then authoring.rz=value end
            local origin=state.project.faceOrigin
            if not origin or not origin.placed or not origin.position then
                tImGui.TextWrapped(tLang.L('fre_target_edit_no_face_origin'))
            else
                tImGui.TextWrapped(tLang.L('fre_target_edit_pivot'))
            end
        end
        local canApplyEdit=selected~=nil and state.topology and state.topology.vertices and #state.topology.vertices>0
        tImGui.BeginDisabled(not canApplyEdit)
        if tImGui.Button(tLang.L('fre_target_edit_apply')) then
            local indices
            if (authoring.selectionMode or 1)==1 then
                indices=tFacialTargets.allIndices(authoring.vertexCount)
            elseif authoring.selectionMode==2 then
                indices=authoring.selectedVertex and {authoring.selectedVertex} or {}
            else
                local mask=state.project.masks and state.project.masks[(authoring.selectionMode or 1)-2]
                indices=tFacialTargets.maskIndices(mask,authoring.vertexCount,state.topology.signature)
            end
            if #indices==0 then
                targetAuthoringError(tLang.L('fre_target_edit_no_vertices'))
            else
                local before=tFacialTargets.copyTargetData(selected)
                local edited=tFacialTargets.copyTargetData(selected)
                local bounds=state.meshBounds or {minX=0,maxX=0,minY=0,maxY=0,minZ=0,maxZ=0}
                local origin=state.project.faceOrigin
                local pivot=origin and origin.placed and origin.position or {
                    x=(bounds.minX+bounds.maxX)*0.5,y=(bounds.minY+bounds.maxY)*0.5,z=(bounds.minZ+bounds.maxZ)*0.5}
                local edit={mode=(authoring.editMode or 1)==1 and 'translate' or
                    (authoring.editMode or 1)==2 and 'rotate' or 'reset',
                    dx=authoring.dx or 0,dy=authoring.dy or 0,dz=authoring.dz or 0,
                    rx=math.rad(authoring.rx or 0),ry=math.rad(authoring.ry or 0),rz=math.rad(authoring.rz or 0),pivot=pivot}
                local changedCount=tFacialTargets.applyEdit(edited,state.topology.vertices,indices,edit)
                if changedCount==0 and edit.mode~='reset' then
                    setStatus(tLang.L('fre_target_edit_no_vertices'),true)
                else
                    local callOk,updated=dpCall(function()
                        return state.meshDebug:updateFacialTarget(authoring.selectedTarget,edited.name,
                            edited.convention,edited.defaultWeight,edited.positionDeltas,
                            #edited.normalDeltas>0 and edited.normalDeltas or nil)
                    end)
                    if callOk and updated~=false then
                        tFacialTargets.recordEdit(authoring,authoring.selectedTarget,before,edited)
                        markFacialTargetAuthoringModified()
                        setStatus(string.format(tLang.L('fre_target_edit_applied_fmt'),changedCount),false)
                    else
                        targetAuthoringError('could not apply delta edit')
                    end
                end
            end
        end
        showHelpTooltip(tLang.L('fre_target_edit_apply_tooltip'))
        tImGui.EndDisabled()
    end
    tImGui.Separator()
    tImGui.Text(tLang.L('fre_target_history'))
    tImGui.BeginDisabled(not authoring.editUndo or #authoring.editUndo==0)
    if tImGui.Button(tLang.L('fre_target_edit_undo')) then
        local record=table.remove(authoring.editUndo)
        local target=tFacialTargets.find(authoring.targets,record and record.index)
        local callOk,restored=false,false
        if target and record then
            callOk,restored=dpCall(function()
                return state.meshDebug:updateFacialTarget(record.index,record.before.name,record.before.convention,
                    record.before.defaultWeight,record.before.positionDeltas,
                    #record.before.normalDeltas>0 and record.before.normalDeltas or nil)
            end)
        end
        if callOk and restored~=false then
            authoring.editRedo=authoring.editRedo or {}
            authoring.editRedo[#authoring.editRedo+1]=record
            authoring.selectedTarget=record.index
            markFacialTargetAuthoringModified()
            setStatus(tLang.L('fre_undo_done'),false)
        elseif record then
            authoring.editUndo[#authoring.editUndo+1]=record
            targetAuthoringError('could not undo delta edit')
        end
    end
    showHelpTooltip(tLang.L('fre_target_edit_undo_tooltip'))
    tImGui.EndDisabled()
    tImGui.SameLine()
    tImGui.BeginDisabled(not authoring.editRedo or #authoring.editRedo==0)
    if tImGui.Button(tLang.L('fre_target_edit_redo')) then
        local record=table.remove(authoring.editRedo)
        local target=tFacialTargets.find(authoring.targets,record and record.index)
        local callOk,restored=false,false
        if target and record then
            callOk,restored=dpCall(function()
                return state.meshDebug:updateFacialTarget(record.index,record.after.name,record.after.convention,
                    record.after.defaultWeight,record.after.positionDeltas,
                    #record.after.normalDeltas>0 and record.after.normalDeltas or nil)
            end)
        end
        if callOk and restored~=false then
            authoring.editUndo=authoring.editUndo or {}
            authoring.editUndo[#authoring.editUndo+1]=record
            authoring.selectedTarget=record.index
            markFacialTargetAuthoringModified()
            setStatus(tLang.L('fre_redo_done'),false)
        elseif record then
            authoring.editRedo[#authoring.editRedo+1]=record
            targetAuthoringError('could not redo delta edit')
        end
    end
    showHelpTooltip(tLang.L('fre_target_edit_redo_tooltip'))
    tImGui.EndDisabled()

    if workMode==2 then
        tImGui.Separator()
        local canEdit=selected~=nil
        tImGui.BeginDisabled(not canEdit)
        if tImGui.Button(tLang.L('fre_target_duplicate')) then
            local data=tFacialTargets.copyTargetData(selected)
            local targetName=(authoring.nameDraft or ''):match('^%s*(.-)%s*$') .. '_copy'
            if data then
                local callOk,newIndex=dpCall(function()
                    return state.meshDebug:addFacialTarget(targetName,data.convention,data.defaultWeight,
                        data.positionDeltas,#data.normalDeltas>0 and data.normalDeltas or nil)
                end)
                if callOk and newIndex then
                    markFacialTargetAuthoringModified()
                    local created=tFacialTargets.find(authoring.targets,newIndex)
                    if created then authoring.createdIds[created.id]=true end
                    tFacialTargets.clearEditHistory(authoring)
                    authoring.selectedTarget=newIndex
                    setStatus(tLang.L('fre_target_duplicated'),false)
                else
                    targetAuthoringError('could not duplicate target')
                end
            end
        end
        showHelpTooltip(tLang.L('fre_target_duplicate_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_target_delete')) then
            local deletedId=selected and selected.id
            local callOk,removed=dpCall(function() return state.meshDebug:removeFacialTarget(authoring.selectedTarget) end)
            if callOk and removed~=false then
                if authoring.createdIds[deletedId] then
                    authoring.createdIds[deletedId]=nil
                else
                    authoring.deletedBaseIds[deletedId]=true
                end
                markFacialTargetAuthoringModified()
                tFacialTargets.clearEditHistory(authoring)
                authoring.selectedTarget=math.max(1,authoring.selectedTarget-1)
                setStatus(tLang.L('fre_target_deleted'),false)
            else
                targetAuthoringError('could not delete target')
            end
        end
        showHelpTooltip(tLang.L('fre_target_delete_tooltip'))
        tImGui.EndDisabled()
    end
    if authoring.modified and state.modified then
        tImGui.TextWrapped(tLang.L('fre_targets_unsaved_help'))
    end
end

local function syncFacialComparisonWeights()
    local comparison=state.facialComparison and state.facialComparison.mesh
    if not comparison or not state.mesh then return end
    local cached=state.facialComparison.weights or {}
    state.facialComparison.weights=cached
    for index,entry in ipairs(state.expressions.catalog or {}) do
        local ok,value=dpCall(function() return state.mesh:getFacialWeight(entry.name) end)
        -- The Lua mesh binding accepts a target name or a 1-based target index
        -- through setFacialWeight(), but it does not expose the C++ convenience
        -- method setFacialWeightByName(). Keep the comparison path on the
        -- public Lua API so project-clip playback cannot abort the editor loop.
        value=tonumber(value) or 0
        if ok and cached[index]~=value then
            local applied,result=dpCall(function() return comparison:setFacialWeight(index,value) end)
            if applied and result~=false then cached[index]=value end
        end
    end
end

local function applyFacialWeights(weights)
    if not state.mesh then return false end
    local success=true
    for _,entry in ipairs(state.expressions.catalog or {}) do
        local ok,applied=dpCall(function() return state.mesh:setFacialWeight(entry.name,weights[entry.name] or 0) end)
        if not ok or applied==false then success=false end
    end
    syncFacialComparisonWeights()
    return success
end

local function applyExpressionPose(pose)
    if not pose then return false end
    return applyFacialWeights(tFacialExpressions.applyPose(state.project.expressions,pose))
end

local function rebuildFacialComparison(enabled, mode)
    local comparison=state.facialComparison
    if comparison.mesh then
        comparison.mesh.alwaysRender=false
        dpCall(function() comparison.mesh:destroy() end)
    end
    comparison.mesh=nil
    comparison.mode=mode or comparison.mode or 'gpu_cpu'
    comparison.syncedClip=nil
    comparison.weights={}
    if state.mesh then state.mesh:setPos(0,0,0) end
    comparison.enabled=enabled==true
    comparison.ready=false
    comparison.layoutActive=false
    if not comparison.enabled then
        if state.mesh then
            state.mesh.alwaysRender=true
            dpCall(function() state.mesh:setPos(0,0,0) end)
        end
        if state.meshBounds then frameCamera(state.meshBounds) end
        return true
    end
    if not state.mesh or not state.project.meshPath or state.project.meshPath=='' then
        return false,'fre_preview_gpu_cpu_reason_no_mesh'
    end
    if comparison.mode=='lbs_dqs' and not state.facialDqsSupported then
        return false,'fre_preview_dqs_unavailable'
    end
    local primaryPathOk,primaryPath=dpCall(function() return state.mesh:getResolvedSkeletalExecutionPath() end)
    if not primaryPathOk or primaryPath~='gpu' then
        return false,'fre_preview_gpu_cpu_reason_gpu_unavailable'
    end
    local primaryReportOk,primaryReport=dpCall(function() return state.mesh:getSkeletalSkinningReport() end)
    if not primaryReportOk or type(primaryReport)~='table' then
        return false,'fre_preview_gpu_cpu_reason_gpu_unavailable'
    end
    local primaryMethod=primaryReport.resolvedMethod=='dqs' and 'dqs' or 'lbs'
    local candidateMethod=comparison.mode=='lbs_dqs' and (primaryMethod=='dqs' and 'lbs' or 'dqs') or primaryMethod
    local candidate=mesh:new('3d')
    local cpuOk,cpuReady=dpCall(function()
        return candidate:setSkeletalSkinningMethod(candidateMethod) and
            candidate:setSkeletalExecutionPath(comparison.mode=='lbs_dqs' and 'gpu' or 'cpu')
    end)
    local loadOk,loaded=false,false
    local comparisonPath=state.targetAuthoring and state.targetAuthoring.previewPath or state.project.meshPath
    if cpuOk and cpuReady~=false then loadOk,loaded=dpCall(function() return candidate:load(comparisonPath) end) end
    if not cpuOk or cpuReady==false or not loadOk or loaded==false then
        candidate.alwaysRender=false
        dpCall(function() candidate:destroy() end)
        return false,comparison.mode=='lbs_dqs' and 'fre_preview_dqs_unavailable' or
            'fre_preview_gpu_cpu_reason_cpu_load'
    end
    if comparison.mode=='lbs_dqs' then
        local ok,report=dpCall(function() return candidate:getSkeletalSkinningReport() end)
        if not ok or report.resolvedMethod==primaryMethod or report.executionStatus~='gpu-ready' then
            candidate:destroy()
            return false,'fre_preview_dqs_unavailable'
        end
    end
    candidate.alwaysRender=true
    dpCall(function() return candidate:stopSkeletalAnimation() end)
    comparison.mesh=candidate
    comparison.ready=true
    local bounds=state.meshBounds or {minX=0,maxX=1,minY=0,maxY=1,minZ=0,maxZ=1}
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local separation=math.max(extent*0.65,0.1)
    dpCall(function() state.mesh:setPos(-separation,0,0) end)
    dpCall(function() candidate:setPos(separation,0,0) end)
    comparison.layoutActive=true
    candidate.visible=state.workspace=='preview' and state.mesh.visible
    syncFacialComparisonWeights()
    local expanded={minX=bounds.minX-separation,maxX=bounds.maxX+separation,
        minY=bounds.minY,maxY=bounds.maxY,minZ=bounds.minZ,maxZ=bounds.maxZ}
    frameCamera(expanded)
    return true
end

local function updateFacialComparisonVisibility()
    local comparison=state.facialComparison
    if comparison and comparison.mesh then
        local active=comparison.enabled and state.workspace=='preview'
        if active and not comparison.layoutActive then
            local bounds=state.meshBounds or {minX=0,maxX=1,minY=0,maxY=1,minZ=0,maxZ=1}
            local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
            local separation=math.max(extent*0.65,0.1)
            dpCall(function() state.mesh:setPos(-separation,0,0) end)
            dpCall(function() comparison.mesh:setPos(separation,0,0) end)
            comparison.layoutActive=true
            frameCamera({minX=bounds.minX-separation,maxX=bounds.maxX+separation,
                minY=bounds.minY,maxY=bounds.maxY,minZ=bounds.minZ,maxZ=bounds.maxZ})
        elseif not active and comparison.layoutActive then
            dpCall(function() state.mesh:setPos(0,0,0) end)
            dpCall(function() comparison.mesh:setPos(0,0,0) end)
            comparison.layoutActive=false
        end
        comparison.mesh.visible=active and state.mesh and state.mesh.visible
        if active and state.mesh then
            local ok,time=dpCall(function() return state.mesh:getSkeletalAnimationTime() end)
            local name=state.preview.activeSkeletal
            if ok and time and name then
                if comparison.syncedClip~=name then
                    dpCall(function() return comparison.mesh:playSkeletalAnimation(name) end)
                    dpCall(function() return comparison.mesh:pauseSkeletalAnimation() end)
                    comparison.syncedClip=name
                end
                local _,otherTime=dpCall(function() return comparison.mesh:getSkeletalAnimationTime() end)
                if not otherTime or math.abs(time-otherTime)>0.0001 then
                    dpCall(function() return comparison.mesh:seekSkeletalAnimation(time) end)
                end
            elseif comparison.syncedClip then
                dpCall(function() return comparison.mesh:stopSkeletalAnimation() end)
                comparison.syncedClip=nil
            end
            local reportOk,report=dpCall(function() return state.mesh:getFacialAnimationReport() end)
            if reportOk and report.playing and not report.paused then syncFacialComparisonWeights() end
        end
    end
end

local function refreshPreviewSkeletalCatalog()
    state.preview.skeletalAnimations={}
    state.preview.selectedSkeletal=math.max(1,state.preview.selectedSkeletal or 1)
    if not state.mesh then
        state.preview.skeletalDirty=false
        return
    end
    local ok,total=dpCall(function() return state.mesh:getTotalSkeletalAnimations() end)
    if ok then
        for index=1,tonumber(total) or 0 do
            local nameOk,name=dpCall(function() return state.mesh:getSkeletalAnimationName(index) end)
            if nameOk and type(name)=='string' and name~='' then
                state.preview.skeletalAnimations[#state.preview.skeletalAnimations+1]=name
            end
        end
    end
    state.preview.selectedSkeletal=math.max(1,math.min(state.preview.selectedSkeletal,
        math.max(1,#state.preview.skeletalAnimations)))
    state.preview.skeletalDirty=false
end

local function loadMesh(path, preserveProject)
    if not path or path=='' then return false end
    local directory=path:match('^(.*)[/\\][^/\\]+$')
    if directory then mbm.addPath(directory) end
    local candidate=mesh:new('3d')
    local okLoaded,loaded=dpCall(function()
        return candidate:setSkeletalSkinningMethod('auto') and candidate:load(path)
    end)
    if not okLoaded or loaded==false then
        candidate:destroy()
        setStatus(string.format(tLang.L('fre_load_failed_fmt'),shortName(path)),true)
        return false
    end
    -- A mesh may persist a skeletal autoplay flag. The facial editor starts from a
    -- deterministic bind/T-pose; playback is opt-in from Preview.
    dpCall(function() return candidate:stopSkeletalAnimation() end)
    local inspector=meshDebug:new()
    local okInspected,inspected=dpCall(function() return inspector:load(path) end)
    if not okInspected or inspected~=true then
        candidate:destroy()
        setStatus(tLang.L('fre_mesh_inspection_failed'),true)
        return false
    end
    local bounds,count,topology=inspectMesh(inspector)
    if not topology then
        candidate:destroy()
        setStatus(tLang.L('fre_mesh_inspection_failed'),true)
        return false
    end
    local requestedPreserve=preserveProject==true
    local projectCompatible=not requestedPreserve or state.project.meshTopologySignature=='' or
        (state.project.meshTopologySignature==topology.signature and
            (state.project.meshVertexCount==0 or state.project.meshVertexCount==topology.vertexCount))
    if requestedPreserve and not projectCompatible and state.project.morphTargetEdits then
        candidate:destroy()
        setStatus(tLang.L('fre_mesh_project_topology_mismatch'),true)
        return false
    end
    if requestedPreserve and not projectCompatible then preserveProject=false end
    local restoredIds={}
    if preserveProject and state.project.morphTargetEdits then
        local restored,ok,idMap=dpCall(function()
            return tFacialTargetProject.apply(inspector,state.project.morphTargetEdits,topology.signature)
        end)
        if not restored or not ok then
            candidate:destroy()
            setStatus(string.format(tLang.L('fre_target_project_restore_failed'),tostring(idMap or ok)),true)
            return false
        end
        restoredIds=idMap or {}
    end
    destroyPreview()
    state.mesh=candidate
    state.meshDebug=inspector
    state.meshBounds=bounds
    state.meshVertexCount=count
    state.topology=topology
    state.mesh.visible=true
    -- Keep the authoring mesh available while the user zooms into the face.
    -- `alwaysRender` is the engine's no-frustum-culling flag; `alwaysOnTop`
    -- would change depth ordering and is therefore not appropriate here.
    state.mesh.alwaysRender=true
    if not preserveProject then state.project=tFacialProject.new() end
    loadSourceObjectCatalog(path)
    state.project.meshPath=path
    if not preserveProject or state.project.meshTopologySignature=='' then
        tFacialProject.setTopology(state.project,topology)
    end
    state.selectedRole=state.project.selectedRole or 1
    state.selectedLandmark=state.project.selectedLandmark or 1
    state.selectedMask=state.project.selectedMask or 1
    state.landmarkEditMode=state.project.landmarkEditMode or 1
    clearHistory()
    state.modified=preserveProject and state.modified or true
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
    state.rigPreview.dirty=true
    state.rigReview={dirty=true,data=nil}
    state.bindingPlan.dirty=true
    state.facialSkeleton={dirty=true,data=nil}
    state.weightAssignment={dirty=true,data=nil,modified=false,facialWeightsApplied=false}
    state.targetAuthoring=tFacialTargets.new()
    if preserveProject and state.project.morphTargetEdits then
        state.targetAuthoring.modified=true
        state.targetAuthoring.previewDirty=true
        state.targetAuthoring.projectRestorePending=true
        local patch=state.project.morphTargetEdits
        local baseIds={}
        for _,id in ipairs(patch.baseIds or {}) do baseIds[id]=true end
        for _,id in ipairs(patch.removedIds or {}) do state.targetAuthoring.deletedBaseIds[id]=true end
        for _,record in ipairs(patch.targets or {}) do
            if not baseIds[record.id] or state.targetAuthoring.deletedBaseIds[record.id] then
                state.targetAuthoring.createdIds[restoredIds[record.id] or record.id]=true
            end
        end
    end
    local savedPreview=state.project.morphTargetPreview
    if savedPreview and savedPreview.targetId and savedPreview.targetId~='' then
        refreshFacialTargetCatalog()
        for index,target in ipairs(state.targetAuthoring.targets) do
            if target.id==(restoredIds[savedPreview.targetId] or savedPreview.targetId) then
                state.targetAuthoring.selectedTarget=index
                state.targetAuthoring.previewWeight=savedPreview.weight
                updateTargetPreviewWeights()
                break
            end
        end
    end
    state.expressions={catalogDirty=true,catalog={},selectedTarget=state.project.expressions.selectedTarget or 1,
        selectedPose=state.project.expressions.selectedPose or 1,nameDraft='',renameDraft='',editMode=1,lastEditMode=1}
    state.preview={selectedPose=state.project.expressions.selectedPose or 1,facialPlayback=nil,
        skeletalDirty=true,skeletalAnimations={},selectedSkeletal=1,authoredClipSelected=1,
        authoredClipTime=0,authoredClipPlaying=false,authoredClipInitialized=false}
    state.clipEditor={selectedClip=state.project.selectedClip or 1,keySelected=1,time=0,playing=false,nameDraft='',renameDraft='',dirty=true,sourceInitialized=false,timelineVisible=true}
    frameCamera(bounds)
    if requestedPreserve and not projectCompatible then
        setStatus(tLang.L('fre_mesh_project_topology_mismatch'),true)
    else
        setStatus(string.format(tLang.L('fre_loaded_fmt'),shortName(path),count),false)
    end
    return true
end

local function newProject()
    destroyPreview()
    state.project=tFacialProject.new()
    state.projectPath=nil
    state.blenderBridgePath=nil
    state.blenderSourcePath=nil
    state.blenderGeneratedBlendPath=nil
    state.automatedTargetJob={status='idle',jsonPath=nil,sourceBlendPath=nil,blendPath=nil,mshPath=nil,
        exportLogPath=nil,logPath=nil,importLogPath=nil,generationFinishedAt=nil,job=nil,
        startedAt=0,phase='',detail='',progress=0}
    state.modified=false
    state.selectedRole=1
    state.selectedLandmark=1
    state.selectedMask=1
    state.landmarkEditMode=state.project.landmarkEditMode or 1
    clearHistory()
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
    state.rigPreview.dirty=true
    state.rigReview={dirty=true,data=nil}
    state.bindingPlan.dirty=true
    state.facialSkeleton={dirty=true,data=nil}
    state.weightAssignment={dirty=true,data=nil,modified=false,facialWeightsApplied=false}
    state.targetAuthoring=tFacialTargets.new()
    state.expressions={catalogDirty=true,catalog={},selectedTarget=1,selectedPose=1,nameDraft='',renameDraft='',editMode=1,lastEditMode=1}
    state.preview={selectedPose=1,facialPlayback=nil,skeletalDirty=true,skeletalAnimations={},selectedSkeletal=1,
        authoredClipSelected=1,authoredClipTime=0,authoredClipPlaying=false,authoredClipInitialized=false}
    state.clipEditor={selectedClip=1,keySelected=1,time=0,playing=false,nameDraft='',renameDraft='',dirty=true,sourceInitialized=false,timelineVisible=true}
    setStatus(tLang.L('fre_new_project'),false)
end

local function openMesh()
    local path=mbm.openFile(state.project.meshPath or '', 'msh')
    if path then loadMesh(path,state.project.meshPath~='') end
end

local function openProject()
    local path=mbm.openFile(state.projectPath or '', 'lua')
    if not path then return end
    local project,errorMessage=tFacialProject.load(path)
    if not project then
        setStatus(string.format(tLang.L('fre_project_load_failed_fmt'),tostring(errorMessage)),true)
        return
    end
    local previousProject,previousPath,previousModified=state.project,state.projectPath,state.modified
    local previousLanguage=tLang.getLanguage()
    if project.language then tLang.setLanguage(project.language) end
    state.project=project
    state.projectPath=path
    state.modified=false
    if project.meshPath~='' then
        if not loadMesh(project.meshPath,true) then
            state.project=previousProject
            state.projectPath=previousPath
            state.modified=previousModified
            tLang.setLanguage(previousLanguage)
            return
        end
    else
        destroyPreview()
        state.targetAuthoring=tFacialTargets.new()
        state.expressions={catalogDirty=true,catalog={},selectedTarget=project.expressions.selectedTarget or 1,
            selectedPose=project.expressions.selectedPose or 1,nameDraft='',renameDraft='',editMode=1,lastEditMode=1}
        state.preview={selectedPose=project.expressions.selectedPose or 1,facialPlayback=nil,
            skeletalDirty=true,skeletalAnimations={},selectedSkeletal=1,authoredClipSelected=1,
            authoredClipTime=0,authoredClipPlaying=false,authoredClipInitialized=false}
        state.clipEditor={selectedClip=project.selectedClip or 1,keySelected=1,time=0,playing=false,nameDraft='',renameDraft='',dirty=true,sourceInitialized=false,timelineVisible=true}
    end
    state.blenderBridgePath=nil
    state.blenderSourcePath=nil
    state.blenderGeneratedBlendPath=nil
    state.automatedTargetJob={status='idle',jsonPath=nil,sourceBlendPath=nil,blendPath=nil,mshPath=nil,
        exportLogPath=nil,logPath=nil,importLogPath=nil,generationFinishedAt=nil,job=nil,
        startedAt=0,phase='',detail='',progress=0}
    state.selectedRole=project.selectedRole or 1
    state.selectedLandmark=project.selectedLandmark or 1
    state.landmarkEditMode=project.landmarkEditMode or 1
    clearHistory()
    setStatus(string.format(tLang.L('fre_project_loaded_fmt'),shortName(path)),false)
end

local function saveProject(path)
    path=path or state.projectPath
    if not path or path=='' then return false end
    state.project.selectedRole=state.selectedRole
    state.project.selectedLandmark=state.selectedLandmark
    state.project.selectedMask=state.selectedMask
    state.project.landmarkEditMode=state.landmarkEditMode
    state.project.expressions=state.project.expressions or tFacialExpressions.new()
    state.project.expressions.selectedTarget=state.expressions.selectedTarget or 1
    state.project.expressions.selectedPose=state.expressions.selectedPose or 1
    if state.targetAuthoring.catalogDirty and state.meshDebug then refreshFacialTargetCatalog() end
    local selected=tFacialTargets.find(state.targetAuthoring.targets,state.targetAuthoring.selectedTarget)
    state.project.morphTargetPreview={targetId=selected and selected.id or '',
        weight=state.targetAuthoring.previewWeight or 1}
    if state.targetAuthoring.modified and state.meshDebug then
        local source=meshDebug:new()
        local loaded,sourceLoaded=dpCall(function() return source:load(state.project.meshPath) end)
        if not loaded or sourceLoaded==false then
            setStatus(tLang.L('fre_target_project_source_failed'),true)
            return false
        end
        local base=tFacialTargets.catalogFromDebug(source)
        local captured,patch,errorMessage=dpCall(function()
            return tFacialTargetProject.capture(base,state.targetAuthoring.targets,
                state.targetAuthoring.vertexCount,state.topology.signature,
                state.targetAuthoring.createdIds,state.targetAuthoring.deletedBaseIds)
        end)
        if not captured or errorMessage then
            setStatus(string.format(tLang.L('fre_project_save_failed_fmt'),tostring(errorMessage or patch)),true)
            return false
        end
        state.project.morphTargetEdits=patch
    end
    state.project.selectedClip=state.clipEditor.selectedClip or 1
    state.project.language=tLang.getLanguage()
    local saved,errorMessage=tFacialProject.save(state.project,path,tUtil.save)
    if not saved then
        setStatus(string.format(tLang.L('fre_project_save_failed_fmt'),tostring(errorMessage)),true)
        return false
    end
    state.projectPath=path
    -- The project contains the target edits; the MSH export remains explicit.
    state.modified=false
    setStatus(string.format(tLang.L('fre_project_saved_fmt'),shortName(path)),false)
    return true
end

local function saveProjectAs()
    local path=mbm.saveFile(state.projectPath or 'facial-rig.lua','lua')
    if path then saveProject(path) end
end

local function exportBlenderBridge()
    local report=tFacialProject.validate(state.project,state.mesh~=nil,state.topology)
    local rig=tFacialProject.rigDescription(state.project,state.topology)
    local binding=tFacialProject.bindingDescription(state.project,state.topology)
    local payload,errorValue=tFacialBridge.buildPayload(state.project,state.topology,rig,binding)
    if not payload then
        local message=type(errorValue)=='table' and errorValue.errors and errorValue.errors[1] or errorValue
        setStatus(tLang.L('fre_bridge_export_blocked') .. ': ' .. tostring(message or ''),true)
        return false
    end
    if not report.ok then
        setStatus(tLang.L('fre_bridge_export_blocked') .. ': ' .. localizedProjectMessage(report.errors[1] or ''),true)
        return false
    end
    local suggested=(state.projectPath or state.project.meshPath or 'facial-rig')
    suggested=suggested:gsub('%.[^%./\\]+$','') .. '-blender-bridge.lua'
    local path=mbm.saveFile(suggested,'lua')
    if not path then return false end
    local saved,errorMessage=tFacialBridge.save(path,payload,tUtil.save)
    if not saved then
        setStatus(string.format(tLang.L('fre_bridge_export_failed_fmt'),tostring(errorMessage)),true)
        return false
    end
    state.blenderBridgePath=path
    setStatus(string.format(tLang.L('fre_bridge_exported_fmt'),shortName(path)),false)
    return true
end

local function chooseBlenderSource()
    if pathExists(state.blenderSourcePath) then return state.blenderSourcePath end
    local suggested=replaceExtension(state.project and state.project.meshPath or '', '.blend')
    if pathExists(suggested) then
        state.blenderSourcePath=suggested
        return suggested
    end
    local path=mbm.openFile(suggested or '', '*.blend')
    if path and path~='' then
        state.blenderSourcePath=path
        return path
    end
    return nil
end

local function launchBlenderFacialRig()
    if not pathExists(state.blenderBridgePath) then
        setStatus(tLang.L('fre_blender_bridge_export_required'),true)
        return false
    end
    local sourcePath=chooseBlenderSource()
    if not sourcePath then
        setStatus(tLang.L('fre_blender_source_missing'),true)
        return false
    end
    local addonPath=getEditorDir() .. '/facial_rig_blender_addon.py'
    if not pathExists(addonPath) then
        failAutomatedFacialTargetGeneration(tLang.L('fre_blender_addon_missing'))
        return false
    end
    tBlender.resetDetection()
    local blender=tBlender.detectBlender()
    if not blender or not blender.found then
        setStatus(tLang.L('fre_blender_not_found'),true)
        return false
    end
    local command=tBlender.buildFacialRigPreviewCmd(sourcePath,addonPath,state.blenderBridgePath)
    if not command then
        failAutomatedFacialTargetGeneration(tLang.L('fre_blender_command_failed'))
        return false
    end
    tBlender.launchCmdAsync(command)
    setStatus(string.format(tLang.L('fre_blender_opened_fmt'),shortName(sourcePath)),false)
    return true
end

local function launchBlenderFacialTargetGeneration()
    if not pathExists(state.blenderBridgePath) then
        setStatus(tLang.L('fre_blender_bridge_export_required'),true)
        return false
    end
    local sourcePath=chooseBlenderSource()
    if not sourcePath then
        setStatus(tLang.L('fre_blender_source_missing'),true)
        return false
    end
    local addonPath=getEditorDir() .. '/facial_rig_blender_addon.py'
    if not pathExists(addonPath) then
        setStatus(tLang.L('fre_blender_addon_missing'),true)
        return false
    end
    local suggested=replaceExtension(sourcePath,'-facial-targets.blend')
    local outputPath=mbm.saveFile(suggested,'blend')
    if not outputPath or outputPath=='' then return false end
    tBlender.resetDetection()
    local blender=tBlender.detectBlender()
    if not blender or not blender.found then
        setStatus(tLang.L('fre_blender_not_found'),true)
        return false
    end
    local command=tBlender.buildFacialTargetGenerationCmd(sourcePath,addonPath,state.blenderBridgePath,outputPath)
    if not command then
        setStatus(tLang.L('fre_blender_command_failed'),true)
        return false
    end
    tBlender.launchCmdAsync(command)
    state.blenderGeneratedBlendPath=outputPath
    setStatus(string.format(tLang.L('fre_blender_targets_generation_started_fmt'),shortName(outputPath)),false)
    return true
end

local function failAutomatedFacialTargetGeneration(message)
    local task=state.automatedTargetJob
    task.status='failed'
    task.job=nil
    task.detail=tostring(message or '')
    task.progress=0
    setStatus(task.detail,true)
end

local function startAutomatedFacialTargetImport()
    local task=state.automatedTargetJob
    local importLogPath=task.importLogPath or replaceExtension(task.mshPath,'-automated-import.log')
    task.importLogPath=importLogPath
    local importerJob,errorMessage=tMeshImporter.start({
        wrapper=tBlender,
        input=task.blendPath,
        output=task.mshPath,
        -- Keep the import log separate from the generation log. On Windows the
        -- first Blender process can still own its redirected log briefly after
        -- saving the generated blend.
        log=importLogPath,
        timeout=600,
        expectedFrames=1,
        options={
            includeBones=true,
            includeFacial=true,
            facialNormalDeltas=true,
            facialConvention='ARKit',
            -- The generated Blender scene contains the source armature action and
            -- its frame range.  Bake that range back into the MSH so the
            -- automated target round-trip does not collapse the skeletal clip
            -- to the bind pose (the exporter defaults to frame 1 otherwise).
            bakeAnimation=true,
            useSceneFrameRange=true,
            sampleStep=1,
            normalizeTextures=true,
            includeTextureDiffuse=true,
            includeTextureNormal=true,
            includeTextureSpecular=true,
            includeTextureEmissive=true,
            includeTextureMask=false,
            largeMeshMode='fail',
            unweightedMeshMode='fail',
            importPostProcess=true,
            importInvertU=false,
            importInvertV=true,
            importAngleX=-90,
            importAngleY=0,
            importAngleZ=0,
            debugSteps=true,
        },
    })
    if not importerJob then
        failAutomatedFacialTargetGeneration(errorMessage or tLang.L('fre_blender_auto_import_failed'))
        return false
    end
    task.job=importerJob
    task.status='importing'
    task.phase='importing'
    task.detail=tLang.L('fre_blender_auto_importing')
    task.progress=0.35
    return true
end

local function startAutomatedFacialTargetGenerationFromBlend()
    local task=state.automatedTargetJob
    local sourcePath=task.sourceBlendPath
    local generatedBlend=task.blendPath
    local addonPath=getEditorDir() .. '/facial_rig_blender_addon.py'
    if not pathExists(addonPath) then
        failAutomatedFacialTargetGeneration(tLang.L('fre_blender_addon_missing'))
        return false
    end
    local command=tBlender.buildFacialTargetGenerationHeadlessCmd(sourcePath,addonPath,
        state.blenderBridgePath,generatedBlend)
    if not command then
        failAutomatedFacialTargetGeneration(tLang.L('fre_blender_command_failed'))
        return false
    end
    tBlender.launchCmdAsync(command,task.logPath)
    task.status='generating'
    task.phase='generating'
    task.detail=tLang.L('fre_blender_auto_generating')
    task.progress=0.15
    task.startedAt=os.time()
    task.job=nil
    setStatus(string.format(tLang.L('fre_blender_auto_started_fmt'),shortName(task.mshPath)),false)
    return true
end

local function ensureAutomatedFacialBridge(path)
    if pathExists(state.blenderBridgePath) then return true end
    local report=tFacialProject.validate(state.project,state.mesh~=nil,state.topology)
    if not report.ok then
        local reason=localizedProjectMessage(report.errors and report.errors[1] or '')
        setStatus(tLang.L('fre_blender_auto_bridge_failed') .. ': ' .. tostring(reason or ''),true)
        return false
    end
    local rig=tFacialProject.rigDescription(state.project,state.topology)
    local binding=tFacialProject.bindingDescription(state.project,state.topology)
    local payload,errorValue=tFacialBridge.buildPayload(state.project,state.topology,rig,binding)
    if not payload then
        local reason=type(errorValue)=='table' and errorValue.errors and errorValue.errors[1] or errorValue
        setStatus(tLang.L('fre_blender_auto_bridge_failed') .. ': ' .. tostring(reason or ''),true)
        return false
    end
    local saved,errorMessage=tFacialBridge.save(path,payload,tUtil.save)
    if not saved then
        setStatus(tLang.L('fre_blender_auto_bridge_failed') .. ': ' .. tostring(errorMessage or ''),true)
        return false
    end
    state.blenderBridgePath=path
    return true
end

local function startAutomatedFacialTargetGeneration()
    local task=state.automatedTargetJob
    if task.status=='exporting' or task.status=='generating' or task.status=='importing' then return false end
    if not state.meshDebug or not state.project.meshPath or state.project.meshPath=='' then
        setStatus(tLang.L('fre_blender_auto_mesh_missing'),true)
        return false
    end
    local suggested=replaceExtension(state.project.meshPath,'-facial-targets-auto.msh')
    local outputMsh=mbm.saveFile(suggested,'msh')
    if not outputMsh or outputMsh=='' then return false end
    outputMsh=replaceExtension(outputMsh,'.msh')
    if pathExists(outputMsh) then
        setStatus(tLang.L('fre_blender_auto_output_exists'),true)
        return false
    end
    local sourceBlend=replaceExtension(outputMsh,'-source.blend')
    local generatedBlend=replaceExtension(outputMsh,'-generated.blend')
    local bridgePath=pathExists(state.blenderBridgePath) and state.blenderBridgePath or
        replaceExtension(outputMsh,'-bridge.lua')
    local jsonPath=replaceExtension(outputMsh,'-source.json')
    local exportLog=replaceExtension(outputMsh,'-source-blender.log')
    local targetLog=replaceExtension(outputMsh,'-automated-blender.log')
    local importLog=replaceExtension(outputMsh,'-automated-import.log')
    if pathExists(sourceBlend) then sourceBlend=replaceExtension(outputMsh,'-source-' .. tostring(os.time()) .. '.blend') end
    if pathExists(generatedBlend) then generatedBlend=replaceExtension(outputMsh,'-generated-' .. tostring(os.time()) .. '.blend') end
    os.remove(jsonPath)
    os.remove(exportLog)
    os.remove(targetLog)
    os.remove(importLog)
    if not ensureAutomatedFacialBridge(bridgePath) then return false end
    tBlender.resetDetection()
    local blender=tBlender.detectBlender()
    if not blender or not blender.found then
        setStatus(tLang.L('fre_blender_not_found'),true)
        return false
    end
    local callOk,okJson,errorJson=dpCall(function()
        return tFacialMeshExport.write(state.meshDebug,jsonPath)
    end)
    if not callOk or not okJson then
        setStatus(tLang.L('fre_blender_auto_mesh_export_failed') .. ': ' .. tostring(errorJson or ''),true)
        return false
    end
    local exporterPath=getEditorDir() .. '/blender_mesh_skeleton_export.py'
    local command=tBlender.buildMeshSkeletonExportCmd(jsonPath,sourceBlend,exporterPath,{
        exportAngleX=90,exportAngleY=0,exportAngleZ=0,exportInvertU=false,exportInvertV=true,
        debugSteps=true,
    })
    if not command then
        setStatus(tLang.L('fre_blender_command_failed'),true)
        return false
    end
    tBlender.launchCmdAsync(command,exportLog)
    task.status='exporting'
    task.phase='exporting'
    task.detail=tLang.L('fre_blender_auto_exporting_mesh')
    task.progress=0.05
    task.startedAt=os.time()
    task.jsonPath=jsonPath
    task.sourceBlendPath=sourceBlend
    task.blendPath=generatedBlend
    task.mshPath=outputMsh
    task.exportLogPath=exportLog
    task.logPath=targetLog
    task.importLogPath=importLog
    task.generationFinishedAt=nil
    task.job=nil
    state.blenderSourcePath=sourceBlend
    state.blenderGeneratedBlendPath=generatedBlend
    setStatus(string.format(tLang.L('fre_blender_auto_started_fmt'),shortName(outputMsh)),false)
    return true
end

local function updateAutomatedFacialTargetGeneration()
    local task=state.automatedTargetJob
    if task.status=='exporting' then
        if tBlender.fileExists(task.sourceBlendPath) then
            task.progress=0.30
            startAutomatedFacialTargetGenerationFromBlend()
            return
        end
        if task.exportLogPath and tBlender.fileExists(task.exportLogPath) then
            local file=io.open(task.exportLogPath,'rb')
            local content=file and file:read('*a') or ''
            if file then file:close() end
            local exportError=content:match('%[mesh_skeleton_export%]%s+ERROR:%s*([^\r\n]+)') or
                content:match('Exporter failed:%s*([^\r\n]+)')
            if exportError and exportError~='' then
                failAutomatedFacialTargetGeneration(exportError)
                return
            end
        end
        if os.time()-(task.startedAt or os.time()) >= 600 then
            failAutomatedFacialTargetGeneration(tLang.L('fre_blender_auto_generation_timeout'))
        end
        return
    end
    if task.status=='generating' then
        local generationFinished=false
        if task.logPath and tBlender.fileExists(task.logPath) then
            local file=io.open(task.logPath,'rb')
            local content=file and file:read('*a') or ''
            if file then file:close() end
            local blenderError=content:match('%[mini_mbm_facial%]%s+ERROR:%s*([^\r\n]+)')
            if blenderError and blenderError~='' then
                failAutomatedFacialTargetGeneration(blenderError)
                return
            end
            -- The generated .blend appears before Blender releases the
            -- redirected log and exits. Wait for its final marker before
            -- starting the importer, especially on Windows.
            generationFinished=content:find('Blender quit',1,true)~=nil
        end
        if tBlender.fileExists(task.blendPath) and generationFinished then
            task.generationFinishedAt=task.generationFinishedAt or os.time()
            if os.time()-task.generationFinishedAt>=2 then
                startAutomatedFacialTargetImport()
                return
            end
        elseif not generationFinished then
            task.generationFinishedAt=nil
        end
        if os.time()-(task.startedAt or os.time()) >= 600 then
            failAutomatedFacialTargetGeneration(tLang.L('fre_blender_auto_generation_timeout'))
        end
        return
    end
    if task.status~='importing' or not task.job then return end
    local status=task.job:update()
    task.progress=0.35 + (tonumber(task.job.progress) or 0) * 0.65
    task.detail=task.job.detail or task.detail
    if status=='completed' then
        local loaded=loadMesh(task.mshPath,true)
        if not loaded then
            failAutomatedFacialTargetGeneration(tLang.L('fre_blender_auto_reload_failed'))
            return
        end
        task.status='completed'
        task.phase='completed'
        task.progress=1
        task.detail=tLang.L('fre_blender_auto_completed')
        task.job=nil
        setStatus(string.format(tLang.L('fre_blender_auto_completed_fmt'),shortName(task.mshPath)),false)
    elseif status=='failed' or status=='canceled' then
        failAutomatedFacialTargetGeneration(task.job.message or tLang.L('fre_blender_auto_import_failed'))
    end
end

local function showStatus()
    if state.status=='' then return end
    if state.statusError then
        tImGui.TextColored({r=1,g=0.35,b=0.25,a=1},state.status)
    else
        tImGui.TextWrapped(state.status)
    end
end

local function textColoredWrapped(color,message)
    tImGui.PushStyleColor('ImGuiCol_Text',color)
    tImGui.TextWrapped(message)
    tImGui.PopStyleColor(1)
end

showHelpTooltip=function(message)
    if not message or message=='' or not tImGui.IsItemHovered(0) then return end
    tImGui.SetNextWindowSizeConstraints({x=280,y=0},{x=440,y=360})
    tImGui.BeginTooltip()
    tImGui.TextWrapped(message)
    tImGui.EndTooltip()
end

local function showMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(tLang.L('fre_new'), 'Ctrl+N') then newProject() end
        if tImGui.MenuItem(tLang.L('fre_open_project'), 'Ctrl+O') then openProject() end
        if tImGui.MenuItem(tLang.L('fre_open_mesh'), nil, false, true) then openMesh() end
        if tImGui.MenuItem(tLang.L('fre_save'), 'Ctrl+S', false, state.modified and state.projectPath~=nil) then
            saveProject()
        end
        if tImGui.MenuItem(tLang.L('fre_save_as'), nil, false, true) then saveProjectAs() end
        if tImGui.MenuItem(tLang.L('fre_export_asset')) then state.exportWindow.open=true end
        if tImGui.MenuItem(tLang.L('fre_undo'), 'Ctrl+Z', false, #state.undoStack>0) then
            if undoProject() then setStatus(tLang.L('fre_undo_done'),false) end
        end
        if tImGui.MenuItem(tLang.L('fre_redo'), 'Ctrl+Y', false, #state.redoStack>0) then
            if redoProject() then setStatus(tLang.L('fre_redo_done'),false) end
        end
        tImGui.Separator()
        if tImGui.MenuItem(tLang.L('menu_quit'), 'Ctrl+Q') then mbm.quit() end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        local pressed,checked=tImGui.MenuItem(tLang.L('move_windows'),nil,state.windowMove)
        if pressed then state.windowMove=checked end
        if tImGui.BeginMenu(tLang.L('menu_language')) then
            if tImGui.MenuItem(tLang.L('lang_english')) then tLang.setLanguage('en') end
            if tImGui.MenuItem(tLang.L('lang_portuguese_br')) then tLang.setLanguage('pt_br') end
            tImGui.EndMenu()
        end
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

local function showExportWindow()
    if not state.exportWindow.open then return end
    local screenWidth,screenHeight=mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=math.max(0,(screenWidth-560)*0.5),y=math.max(0,(screenHeight-360)*0.5)},
        tImGui.Flags('ImGuiCond_FirstUseEver'))
    tImGui.SetNextWindowSize({x=560,y=360},tImGui.Flags('ImGuiCond_FirstUseEver'))
    tImGui.SetNextWindowSizeConstraints({x=480,y=300},{x=900,y=720})
    local opened,closed=tImGui.Begin(tLang.L('fre_export_window_title')..'##freExportWindow',true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if closed then state.exportWindow.open=false end
    if opened then
        tImGui.TextWrapped(tLang.L('fre_export_window_help'))
        tImGui.Separator()
        local targets={tLang.L('fre_export_target_project'),tLang.L('fre_export_target_msh'),
            tLang.L('fre_export_target_bridge')}
        tImGui.SetNextItemWidth(300)
        local targetChanged,target=tImGui.Combo(tLang.L('fre_export_target'),state.exportWindow.target,targets)
        if targetChanged then state.exportWindow.target=target end
        tImGui.Separator()
        if state.exportWindow.target==1 then
            tImGui.TextWrapped(tLang.L('fre_export_project_help'))
            if tImGui.Button(tLang.L('fre_export_project_button'),{x=220,y=0}) then saveProjectAs() end
            showHelpTooltip(tLang.L('fre_export_project_tooltip'))
        elseif state.exportWindow.target==2 then
            tImGui.TextWrapped(tLang.L('fre_export_msh_help'))
            local includeClipsChanged,includeClips=tImGui.Checkbox(
                tLang.L('fre_export_msh_include_project_clips'),
                state.exportWindow.includeProjectFacialClips~=false)
            if includeClipsChanged then state.exportWindow.includeProjectFacialClips=includeClips end
            showHelpTooltip(tLang.L('fre_export_msh_include_project_clips_tooltip'))
            if not state.meshDebug then
                tImGui.TextDisabled(tLang.L('fre_no_mesh'))
            else
                if state.facialSkeleton.dirty then refreshFacialSkeletonBinding(true) end
                local skeleton=state.facialSkeleton.data
                tImGui.Text(tLang.L('fre_export_msh_skeleton_title'))
                if skeleton then
                    tImGui.Text(string.format(tLang.L('fre_facial_skeleton_summary_fmt'),
                        skeleton.existingCount or 0,skeleton.missingCount or 0))
                    local needsSkeleton=skeleton.valid==true and
                        ((skeleton.missingCount or 0)>0 or skeleton.rootParentNeedsRepair==true)
                    tImGui.BeginDisabled(not needsSkeleton)
                    local skeletonLabel=skeleton.rootParentNeedsRepair and
                        tLang.L('fre_facial_skeleton_repair_root') or tLang.L('fre_facial_skeleton_apply')
                    if tImGui.Button(skeletonLabel..'##freExportApplySkeleton',{x=260,y=0}) then
                        applyFacialSkeletonBinding()
                    end
                    tImGui.EndDisabled()
                    showHelpTooltip(tLang.L('fre_facial_skeleton_apply_tooltip'))
                    if skeleton.valid~=true then
                        tImGui.TextDisabled(tLang.L('fre_facial_skeleton_blocked'))
                    elseif not needsSkeleton then
                        tImGui.TextDisabled(tLang.L('fre_export_msh_skeleton_ready'))
                    end
                end
                if state.weightAssignment.dirty then refreshWeightAssignment(true) end
                local assignment=state.weightAssignment.data
                tImGui.Text(tLang.L('fre_export_msh_weights_title'))
                if assignment then
                    tImGui.Text(string.format(tLang.L('fre_weight_assignment_summary_fmt'),
                        assignment.readyCount or 0,assignment.blockedCount or 0,assignment.totalVertexCount or 0))
                    tImGui.BeginDisabled(assignment.valid~=true)
                    if tImGui.Button(tLang.L('fre_weight_assignment_apply')..'##freExportApplyWeights',{x=390,y=0}) then
                        applyRigidFacialWeights()
                    end
                    tImGui.EndDisabled()
                    showHelpTooltip(tLang.L('fre_weight_assignment_apply_tooltip'))
                    if assignment.valid~=true then
                        tImGui.TextDisabled(tLang.L('fre_export_msh_weights_blocked'))
                    end
                end
            end
            local canExport=canExportRuntimeMsh()
            tImGui.BeginDisabled(not canExport)
            if tImGui.Button(tLang.L('fre_export_msh_button')..'##freExportSaveMsh',{x=220,y=0}) then saveAssignedMesh() end
            tImGui.EndDisabled()
            showHelpTooltip(tLang.L('fre_export_msh_tooltip'))
            if not canExport then tImGui.TextDisabled(tLang.L('fre_export_msh_blocked')) end
        else
            tImGui.TextWrapped(tLang.L('fre_export_bridge_help'))
            if tImGui.Button(tLang.L('fre_export_bridge_button'),{x=220,y=0}) then exportBlenderBridge() end
            showHelpTooltip(tLang.L('fre_export_bridge_tooltip'))
        end
    end
    tImGui.End()
end

local function selectWorkspace(workspace)
    state.workspace=state.workspace==workspace and nil or workspace
    state.targetBrushHover.pendingX=nil
    state.targetBrushHover.pendingY=nil
    state.targetBrushHover.lastHit=nil
    if state.workspace~='targets' and state.targetOverlay.building then
        destroyTargetOverlay()
    else
        setTargetOverlayVisible(true)
    end
    -- The mask overlay owns the primary mesh visibility while it is active.
    -- Restore that ownership immediately when leaving the mask worktree so a
    -- dirty surface kept during brush edits cannot hide the mesh elsewhere.
    if state.workspace=='masks' then
        setMaskVisualizationViewportVisible(true)
    else
        setMaskVisualizationViewportVisible(false)
    end
    if state.workspace~='masks' and state.maskVisualization.building then
        state.maskVisualization.building=false
        state.maskVisualization.buildSelected=nil
        state.maskVisualization.buildSelectedSet=nil
        state.maskVisualization.buildSurfaceCoords=nil
        state.maskVisualization.buildSurfaceUvs=nil
        state.maskVisualization.buildSurfaceIndices=nil
        state.maskVisualization.needsBuild=true
    end
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
    state.maskVisualization.dirty=true
end

local function showSetup()
    tImGui.TextWrapped(tLang.L('fre_setup_help'))
    tImGui.TextWrapped(tLang.L('fre_project_mesh_persistence_help'))
    if tImGui.Button(tLang.L('fre_open_mesh')) then openMesh() end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('fre_open_project')) then openProject() end
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    tImGui.Separator()
    tImGui.Text(string.format(tLang.L('fre_mesh_summary_fmt'),shortName(state.project.meshPath),state.meshVertexCount))
    local mappedObjects,mappedVertices=0,0
    for _,indices in pairs(state.project.sourceVertexMap or {}) do
        mappedObjects=mappedObjects+1
        mappedVertices=mappedVertices+#(indices or {})
    end
    tImGui.Text(string.format(tLang.L('fre_source_map_summary_fmt'),mappedObjects,mappedVertices))
    tImGui.TextWrapped(tLang.L('fre_setup_roles_help'))
    tImGui.Text(string.format(tLang.L('fre_role_summary_fmt'),#state.project.roles,#state.project.masks))
end

local function deleteSelectedMask()
    local masks=state.project.masks or {}
    local index=state.selectedMask
    if not masks[index] then return false end
    pushUndoSnapshot()
    table.remove(masks,index)
    if #masks==0 then
        state.selectedMask=1
    else
        state.selectedMask=math.min(index,#masks)
    end
    state.project.selectedMask=state.selectedMask
    state.maskDeletePending=nil
    markMaskVisualizationDirty()
    markModified()
    setStatus(tLang.L('fre_mask_deleted'),false)
    return true
end

local function showMasks()
    tImGui.TextWrapped(tLang.L('fre_masks_help'))
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    if tImGui.BeginTable('freMasksGuide',2,tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(tLang.L('fre_masks_guide_situation'))
        tImGui.TableSetupColumn(tLang.L('fre_masks_guide_action'))
        tImGui.TableHeadersRow()
        local guideRows={
            {'fre_masks_guide_single_object','fre_masks_guide_single_object_action'},
            {'fre_masks_guide_separate_objects','fre_masks_guide_separate_objects_action'},
            {'fre_masks_guide_same_object_regions','fre_masks_guide_same_object_regions_action'}
        }
        for _,row in ipairs(guideRows) do
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0)
            tImGui.TextWrapped(tLang.L(row[1]))
            tImGui.TableSetColumnIndex(1)
            tImGui.TextWrapped(tLang.L(row[2]))
        end
        tImGui.EndTable()
    end
    tImGui.TextWrapped(tLang.L('fre_masks_subset_help'))
    local roles=state.project.roles
    local selected=roles[state.selectedRole] or roles[1]
    if tImGui.BeginTable('freRoles',3,tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(tLang.L('fre_role_column'))
        tImGui.TableSetupColumn(tLang.L('fre_object_column'))
        tImGui.TableSetupColumn(tLang.L('fre_enabled_column'))
        tImGui.TableHeadersRow()
        for index,role in ipairs(roles) do
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0)
            local chosen=tImGui.Selectable(role.name,index==state.selectedRole)
            if chosen then state.selectedRole=index end
            tImGui.TableSetColumnIndex(1)
            local objectLabels,objectValues=sourceObjectOptions(role.name)
            sourceObjectCombo(role,index,objectLabels,objectValues)
            showHelpTooltip(tLang.L('fre_source_object_tooltip'))
            tImGui.TableSetColumnIndex(2)
            tImGui.Text(role.enabled and tLang.L('fre_yes') or tLang.L('fre_no'))
        end
        tImGui.EndTable()
    end
    if selected then
        tImGui.Separator()
        tImGui.Text(string.format(tLang.L('fre_selected_role_fmt'),selected.name))
        tImGui.TextWrapped(tLang.L('fre_source_object_help'))
        local selectedRoleMaskCount=0
        for _,mask in ipairs(state.project.masks or {}) do
            if (mask.roleIndex or 1)==state.selectedRole and mask.enabled~=false and #(mask.vertices or {})>0 then
                selectedRoleMaskCount=selectedRoleMaskCount + 1
            end
        end
        if selected.object~='' and selectedRoleMaskCount==0 then
            tImGui.TextWrapped(string.format(tLang.L('fre_role_object_only_help'),selected.object))
        end
        tImGui.PushItemWidth(320)
        local changed,object=tImGui.InputText(tLang.L('fre_object_name')..'##freObjectName',state.sourceObjectDraft or '')
        if changed then state.sourceObjectDraft=object end
        tImGui.PopItemWidth()
        if tImGui.Button(tLang.L('fre_register_object')) then
            local objectName=(state.sourceObjectDraft or ''):match('^%s*(.-)%s*$')
            if objectName ~= '' then
                pushUndoSnapshot()
                tFacialProject.addSourceObject(state.project,objectName)
                selected.object=objectName
                state.sourceObjectDraft=''
                markModified()
            end
        end
        showHelpTooltip(tLang.L('fre_register_object_tooltip'))
        local enabled=tImGui.Checkbox(tLang.L('fre_role_enabled'),selected.enabled)
        showHelpTooltip(tLang.L('fre_role_enabled_tooltip'))
        if enabled~=selected.enabled then
            pushUndoSnapshot()
            selected.enabled=enabled
            markModified()
        end
        if tImGui.Button(tLang.L('fre_add_mask')) then
            pushUndoSnapshot()
            tFacialProject.addMask(state.project,selected.name,state.selectedRole)
            state.selectedMask=#state.project.masks
            markModified()
        end
        showHelpTooltip(tLang.L('fre_add_mask_tooltip'))
        if selected.name=='main_face' then
            if tImGui.Button(tLang.L('fre_generate_face_mask')) then generateMainFaceMask() end
            showHelpTooltip(tLang.L('fre_generate_face_mask_tooltip'))
        end
    end
    tImGui.Separator()
    tImGui.Text(tLang.L('fre_masks'))
    if tImGui.BeginTable('freMasks',3,tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
        tImGui.TableSetupColumn(tLang.L('fre_mask_column'))
        tImGui.TableSetupColumn(tLang.L('fre_role_column'))
        tImGui.TableSetupColumn(tLang.L('fre_mask_vertices_column'))
        tImGui.TableHeadersRow()
        for index,mask in ipairs(state.project.masks) do
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0)
            local maskLabel=(mask.name or tLang.L('fre_unassigned')) .. '##freMaskRow' .. index
            if tImGui.Selectable(maskLabel,index==state.selectedMask) then
                state.selectedMask=index
                state.maskDeletePending=nil
                markMaskVisualizationDirty()
            end
            tImGui.TableSetColumnIndex(1)
            local role=state.project.roles[mask.roleIndex or 1]
            tImGui.Text(role and role.name or tLang.L('fre_unassigned'))
            tImGui.TableSetColumnIndex(2)
            tImGui.Text(tostring(#(mask.vertices or {})))
        end
        tImGui.EndTable()
    end
    local selectedMask=state.project.masks[state.selectedMask]
    if selectedMask then
        local visualizationReady=not state.maskVisualization.building and
            (not state.maskVisualization.needsBuild or state.maskVisualization.surface~=nil)
        if state.maskVisualization.building then
            local total=math.max(state.maskVisualization.buildTotal,1)
            local progress=math.max(0,math.min(1,(state.maskVisualization.buildCursor-1)/total))
            tImGui.Text(tLang.L('fre_mask_visualization_building'))
            tImGui.ProgressBar(progress,{x=-1,y=0},string.format('%.0f%%',progress*100))
        else
            local buildLabel=state.maskVisualization.needsBuild and
                tLang.L('fre_mask_visualization_generate') or
                tLang.L('fre_mask_visualization_refresh')
            if tImGui.Button(buildLabel .. '##freMaskVisualizationBuild') then
                startMaskVisualizationBuild()
                visualizationReady=false
            end
            showHelpTooltip(tLang.L('fre_mask_visualization_generate_tooltip'))
            if state.maskVisualization.needsBuild then
                tImGui.TextWrapped(tLang.L('fre_mask_visualization_pending'))
            end
        end
        tImGui.Separator()
        tImGui.BeginDisabled(not visualizationReady)
        tImGui.PushItemWidth(320)
        local changed,name=tImGui.InputText(tLang.L('fre_mask_name')..'##freMaskName',selectedMask.name or '')
        if changed and name~='' then
            local duplicate=false
            for index,mask in ipairs(state.project.masks or {}) do
                if index~=state.selectedMask and mask.name==name then duplicate=true break end
            end
            if duplicate then
                setStatus(tLang.L('fre_mask_name_duplicate'),true)
            else
                pushUndoSnapshot()
                selectedMask.name=name
                markModified()
            end
        end
        tImGui.PopItemWidth()
        local roleNames=tFacialProject.roleNames()
        tImGui.SetNextItemWidth(240)
        local roleChanged,roleIndex=tImGui.Combo(tLang.L('fre_mask_role'),selectedMask.roleIndex or 1,roleNames)
        showHelpTooltip(tLang.L('fre_mask_role_tooltip'))
        if roleChanged then
            pushUndoSnapshot()
            selectedMask.roleIndex=roleIndex
            markModified()
        end
        local maskEnabled=tImGui.Checkbox(tLang.L('fre_mask_enabled') .. '##freMaskEnabled' .. state.selectedMask,
            selectedMask.enabled~=false)
        showHelpTooltip(tLang.L('fre_mask_enabled_tooltip'))
        if maskEnabled ~= (selectedMask.enabled~=false) then
            pushUndoSnapshot()
            selectedMask.enabled=maskEnabled
            markMaskVisualizationDirty()
            markModified()
        end
        tImGui.TextWrapped(tLang.L('fre_mask_edit_help'))
        tImGui.Text(tLang.L('fre_mask_edit_mode'))
        local editMode=state.maskEditMode=='remove' and 2 or 1
        editMode=tImGui.RadioButton(tLang.L('fre_mask_add_vertices'),editMode,1)
        tImGui.SameLine()
        editMode=tImGui.RadioButton(tLang.L('fre_mask_remove_vertices'),editMode,2)
        state.maskEditMode=editMode==2 and 'remove' or 'add'
        showHelpTooltip(state.maskEditMode=='remove' and
            tLang.L('fre_mask_remove_vertices_tooltip') or
            tLang.L('fre_mask_add_vertices_tooltip'))
        local brushRadiusChanged,brushRadius=tImGui.SliderFloat(
            tLang.L('fre_mask_brush_radius'),state.maskBrushRadius,0.005,0.20,'%.3f')
        if brushRadiusChanged then state.maskBrushRadius=brushRadius end
        showHelpTooltip(tLang.L('fre_mask_brush_radius_tooltip'))
        local modeLabel=state.maskEditMode=='remove' and tLang.L('fre_mask_mode_remove') or
            tLang.L('fre_mask_mode_add')
        tImGui.Text(tLang.L('fre_mask_mode') .. ': ' .. modeLabel)
        local modeHelp=state.maskEditMode=='remove' and tLang.L('fre_mask_remove_mode_help') or
            tLang.L('fre_mask_add_mode_help')
        tImGui.TextWrapped(modeHelp)
        tImGui.TextWrapped(tLang.L('fre_mask_edit_action_help'))
        tImGui.TextWrapped(tLang.L('fre_mask_click_help'))
        if visualizationReady then
            local showMask=tImGui.Checkbox(tLang.L('fre_mask_visualization'),state.maskVisualization.visible)
            showHelpTooltip(tLang.L('fre_mask_visualization_tooltip'))
            if showMask~=state.maskVisualization.visible then
                state.maskVisualization.visible=showMask
                state.maskVisualization.dirty=true
            end
        else
            tImGui.TextDisabled(tLang.L('fre_mask_visualization_unavailable'))
        end
        tImGui.Text(string.format(tLang.L('fre_mask_visualization_stats_fmt'),
            state.maskVisualization.vertexCount,state.maskVisualization.faceCount))
        if tImGui.Button(tLang.L('fre_mask_clear')) then
            pushUndoSnapshot()
            selectedMask.vertices={}
            markMaskVisualizationDirty()
            markModified()
        end
        showHelpTooltip(tLang.L('fre_mask_clear_tooltip'))
        tImGui.SameLine()
        if state.maskDeletePending==state.selectedMask then
            if tImGui.Button(tLang.L('fre_mask_delete_confirm') .. '##freMaskDeleteConfirm') then
                deleteSelectedMask()
            end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('fre_mask_delete_cancel') .. '##freMaskDeleteCancel') then
                state.maskDeletePending=nil
            end
        else
            if tImGui.Button(tLang.L('fre_mask_delete') .. '##freMaskDelete') then
                state.maskDeletePending=state.selectedMask
            end
            showHelpTooltip(tLang.L('fre_mask_delete_tooltip'))
        end
        if state.maskDeletePending==state.selectedMask then
            tImGui.TextWrapped(tLang.L('fre_mask_delete_confirm_help'))
        end
        tImGui.EndDisabled()
    end
end

local function setFaceOriginFromVertex(vertexHit)
    if not vertexHit or not state.meshBounds then return false end
    pushUndoSnapshot()
    local origin=state.project.faceOrigin or {position={x=0,y=0,z=0}}
    local scale=origin.placed and origin.scale or faceGuideScale(state.meshBounds)
    if not tFacialProject.setFaceOrigin(state.project,vertexHit.point,scale,vertexHit.index) then return false end
    for _,landmark in ipairs(state.project.landmarks or {}) do
        if landmark.placed and landmark.position and not landmark.relativePosition then
            landmark.relativePosition={x=landmark.position.x-vertexHit.point.x,
                y=landmark.position.y-vertexHit.point.y,z=landmark.position.z-vertexHit.point.z}
        end
    end
    state.project.selectedLandmark=state.selectedLandmark
    markModified()
    markFaceGuideDirty()
    markLandmarkMarkersDirty()
    setStatus(tLang.L('fre_face_origin_set'),false)
    return true
end

local function scaleFaceReference(newScale)
    local origin=state.project.faceOrigin
    if not origin or not origin.placed or not origin.position then return false end
    local oldScale=math.max(tonumber(origin.scale) or 1e-6,1e-6)
    local scale=math.max(tonumber(newScale) or oldScale,1e-6)
    local factor=scale/oldScale
    if math.abs(factor-1)<=1e-7 then return false end
    origin.scale=scale
    for _,landmark in ipairs(state.project.landmarks or {}) do
        if landmark.placed and landmark.position then
            local relative=landmark.relativePosition or {
                x=landmark.position.x-origin.position.x,
                y=landmark.position.y-origin.position.y,
                z=landmark.position.z-origin.position.z}
            relative={x=relative.x*factor,y=relative.y*factor,z=relative.z*factor}
            landmark.relativePosition=relative
            landmark.position={x=origin.position.x+relative.x,
                y=origin.position.y+relative.y,z=origin.position.z+relative.z}
            landmark.vertexIndex=nil
        end
    end
    return true
end

local function showFacePosition()
    tImGui.TextWrapped(tLang.L('fre_face_position_help'))
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    local origin=state.project.faceOrigin
    if not origin or not origin.placed then
        tImGui.TextDisabled(tLang.L('fre_face_origin_not_placed'))
        return
    end
    tImGui.Text(string.format(tLang.L('fre_face_origin_placed_fmt'),origin.position.x,origin.position.y,origin.position.z))
    tImGui.TextWrapped(tLang.L('fre_face_origin_drag_help'))
    tImGui.PushItemWidth(240)
    local scaleChanged,newScale=tImGui.DragFloat(tLang.L('fre_face_scale'),origin.scale,
        math.max(origin.scale*0.01,0.001),0.001,0,'%.4f',0)
    tImGui.PopItemWidth()
    tImGui.TextWrapped(tLang.L('fre_face_scale_help'))
    if scaleChanged then
        pushUndoSnapshot()
        if scaleFaceReference(newScale) then
            markModified()
            markFaceGuideDirty()
            markLandmarkMarkersDirty()
        else
            table.remove(state.undoStack)
        end
    end
    if tImGui.Button(tLang.L('fre_face_position_clear')) then
        pushUndoSnapshot()
        origin.placed=false
        origin.vertexIndex=nil
        origin.position={x=0,y=0,z=0}
        markModified()
        markFaceGuideDirty()
        markLandmarkMarkersDirty()
        setStatus(tLang.L('fre_face_position_cleared'),false)
    end
end

local function syncLandmarkRelativePosition(landmark)
    local origin=state.project.faceOrigin
    if not origin or not origin.placed or not landmark or not landmark.position then return end
    landmark.relativePosition={x=landmark.position.x-origin.position.x,
        y=landmark.position.y-origin.position.y,z=landmark.position.z-origin.position.z}
end

local function findLandmark(name)
    for _,landmark in ipairs(state.project.landmarks or {}) do
        if landmark.name==name then return landmark end
    end
    return nil
end

local function mirrorLandmarkName(name,side)
    if type(name)~='string' or (side~='left' and side~='right') then return nil end
    local sourcePrefix=side .. '_'
    local oppositePrefix=(side=='left' and 'right_' or 'left_')
    if name:sub(1,#sourcePrefix)==sourcePrefix then
        return oppositePrefix .. name:sub(#sourcePrefix+1)
    end
    local sourceSuffix='_' .. side
    if name:sub(-#sourceSuffix)==sourceSuffix then
        return name:sub(1,-#sourceSuffix-1) .. '_' .. (side=='left' and 'right' or 'left')
    end
    return nil
end

local function landmarkMirrorCenterX()
    local nose=findLandmark('nose_tip') or findLandmark('nose_bridge')
    if nose and nose.placed and nose.position then return nose.position.x end
    local origin=state.project.faceOrigin
    if origin and origin.placed and origin.position then return origin.position.x end
    return 0
end

local function mirrorLandmark(landmark)
    if state.landmarkEditMode~=2 or not landmark or
            (landmark.side~='left' and landmark.side~='right') then return end
    local oppositeName=mirrorLandmarkName(landmark.name,landmark.side)
    local opposite=oppositeName and findLandmark(oppositeName)
    if not opposite or not landmark.position then return end
    local centerX=landmarkMirrorCenterX()
    opposite.position={x=2*centerX-landmark.position.x,y=landmark.position.y,z=landmark.position.z}
    opposite.placed=landmark.placed
    opposite.vertexIndex=nil
    syncLandmarkRelativePosition(opposite)
end

local function synchronizeLandmarkSides()
    local changed=false
    for _,landmark in ipairs(state.project.landmarks or {}) do
        if landmark.placed and landmark.position and
                (landmark.side=='left' or landmark.side=='right') then
            local oppositeName=mirrorLandmarkName(landmark.name,landmark.side)
            local opposite=oppositeName and findLandmark(oppositeName)
            if opposite then
                local centerX=landmarkMirrorCenterX()
                local mirroredX=2*centerX-landmark.position.x
                local previous=opposite.position
                local origin=state.project.faceOrigin
                local relative=origin and origin.placed and {
                    x=mirroredX-origin.position.x,
                    y=landmark.position.y-origin.position.y,
                    z=landmark.position.z-origin.position.z} or nil
                local previousRelative=opposite.relativePosition
                if not previous or math.abs(previous.x-mirroredX)>1e-6 or
                        math.abs(previous.y-landmark.position.y)>1e-6 or
                        math.abs(previous.z-landmark.position.z)>1e-6 or
                        relative and (not previousRelative or
                            math.abs(previousRelative.x-relative.x)>1e-6 or
                            math.abs(previousRelative.y-relative.y)>1e-6 or
                            math.abs(previousRelative.z-relative.z)>1e-6) or
                        not opposite.placed or opposite.vertexIndex~=nil then
                    changed=true
                end
                opposite.position={x=mirroredX,y=landmark.position.y,z=landmark.position.z}
                opposite.placed=true
                opposite.vertexIndex=nil
                syncLandmarkRelativePosition(opposite)
            end
        end
    end
    return changed
end

local function setLandmarkPosition(landmark,position,vertexIndex)
    if not landmark or not position then return end
    pushUndoSnapshot()
    landmark.position={x=position.x,y=position.y,z=position.z}
    landmark.vertexIndex=vertexIndex
    landmark.placed=true
    syncLandmarkRelativePosition(landmark)
    mirrorLandmark(landmark)
end

local function mainFaceSurfaceVertices()
    local mainFaceRole
    for index,role in ipairs(state.project.roles or {}) do
        if role.name=='main_face' then mainFaceRole=index break end
    end
    if not mainFaceRole then return nil end
    local allowed={}
    local count=0
    for _,mask in ipairs(state.project.masks or {}) do
        if mask.enabled~=false and mask.roleIndex==mainFaceRole then
            for _,vertex in ipairs(mask.vertices or {}) do
                local index=tonumber(vertex)
                if index and not allowed[index] then allowed[index]=true; count=count+1 end
            end
        end
    end
    return count>0 and allowed or nil
end

local function landmarkSurfaceReferenceOffset(name, scale)
    -- The values are deliberately conservative authoring hints, not rig
    -- invariants. Surface landmarks stay on the detected skin; eye centers and
    -- jaw hinges receive a small inward offset because they represent pivots.
    local coefficient={
        left_eye_center=-0.035, right_eye_center=-0.035,
        jaw_hinge_left=-0.10, jaw_hinge_right=-0.10
    }
    return (coefficient[name] or 0)*math.max(tonumber(scale) or 1,0.001)
end

local function landmarkSurfaceRay(axis, reference, bounds, extent, reverse)
    local sign=reverse and 1 or -1
    if axis==1 then
        return {x=reverse and bounds.minX-extent*2 or bounds.maxX+extent*2,
            y=reference.y,z=reference.z},{x=sign,y=0,z=0}
    elseif axis==2 then
        return {x=reference.x,y=reverse and bounds.minY-extent*2 or bounds.maxY+extent*2,
            z=reference.z},{x=0,y=sign,z=0}
    end
    return {x=reference.x,y=reference.y,
        z=reverse and bounds.minZ-extent*2 or bounds.maxZ+extent*2},{x=0,y=0,z=sign}
end

local function snapSelectedLandmarkDepthToSurface()
    local landmark=state.project.landmarks[state.selectedLandmark]
    if not landmark or not state.topology or not state.meshBounds then return false end
    local reference=(landmark.placed and landmark.position) or state.faceGuide.worldPoints[landmark.name]
    if not reference and state.project.faceOrigin and state.project.faceOrigin.placed then
        local origin=state.project.faceOrigin
        reference=addPoint(origin.position,faceGuideRelative(landmark.name,
            math.max(tonumber(origin.scale) or faceGuideScale(state.meshBounds),0.001)))
    end
    if not reference then
        setStatus(tLang.L('fre_landmark_surface_snap_failed'),true)
        return false
    end
    local bounds=state.meshBounds
    local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ,0.001)
    local axis=math.max(1,math.min(3,tonumber(state.landmarkSurfaceAxis) or 3))
    local origin,direction=landmarkSurfaceRay(axis,reference,bounds,extent,false)
    local allowed=mainFaceSurfaceVertices()
    local hit=tFacialLandmarks.intersectRaySurface(state.topology,origin,direction,allowed)
    if not hit and allowed then
        -- A manually edited mask can omit one triangle at the requested X/Y.
        -- Retry against the complete topology rather than failing a valid
        -- authoring operation; the masked pass remains the preferred result.
        hit=tFacialLandmarks.intersectRaySurface(state.topology,origin,direction)
    end
    if not hit then
        -- A mirrored asset may have its surface toward the opposite side of
        -- the selected mesh axis. This remains axis-aligned and independent of
        -- camera rotation.
        origin,direction=landmarkSurfaceRay(axis,reference,bounds,extent,true)
        hit=tFacialLandmarks.intersectRaySurface(state.topology,origin,direction,allowed)
        if not hit and allowed then
            hit=tFacialLandmarks.intersectRaySurface(state.topology,origin,direction)
        end
    end
    if not hit then
        setStatus(tLang.L('fre_landmark_surface_snap_failed'),true)
        return false
    end
    pushUndoSnapshot()
    local scale=state.project.faceOrigin and state.project.faceOrigin.scale or
        faceGuideScale(state.meshBounds)
    local offset=state.landmarkSurfaceAutoOffset and
        landmarkSurfaceReferenceOffset(landmark.name,scale) or
        (tonumber(state.landmarkSurfaceOffset) or 0)
    -- Preserve the two coordinates that define the user's selected line and
    -- solve only the requested depth axis. Applying the full triangle normal
    -- here would move X/Y for an oblique jaw surface even when Z is selected.
    local point={x=reference.x,y=reference.y,z=reference.z}
    if axis==1 then
        point.x=hit.point.x+offset
    elseif axis==2 then
        point.y=hit.point.y+offset
    else
        point.z=hit.point.z+offset
    end
    setLandmarkPosition(landmark,point,nil)
    state.project.selectedLandmark=state.selectedLandmark
    markModified()
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
    setStatus(tLang.L('fre_landmark_surface_snapped'),false)
    return true
end

local function showLandmarks()
    tImGui.TextWrapped(tLang.L('fre_landmarks_help'))
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    local landmarks=state.project.landmarks
    tImGui.PushItemWidth(220)
    tImGui.Text(tLang.L('fre_landmark_edit_mode'))
    local previousEditMode=state.landmarkEditMode
    state.landmarkEditMode=tImGui.RadioButton(tLang.L('fre_landmark_mode_async'),state.landmarkEditMode,1)
    tImGui.SameLine()
    state.landmarkEditMode=tImGui.RadioButton(tLang.L('fre_landmark_mode_sync'),state.landmarkEditMode,2)
    if previousEditMode~=2 and state.landmarkEditMode==2 then
        pushUndoSnapshot()
        if synchronizeLandmarkSides() then
            markModified()
            markLandmarkMarkersDirty()
            markFaceGuideDirty()
        else
            table.remove(state.undoStack)
        end
    end
    if state.landmarkEditMode==2 then
        tImGui.TextWrapped(tLang.L('fre_landmark_sync_help'))
        local nose=findLandmark('nose_tip') or findLandmark('nose_bridge')
        if not nose or not nose.placed then
            tImGui.TextDisabled(tLang.L('fre_landmark_sync_center_fallback'))
        end
    end
    local names={}
    for _,landmark in ipairs(landmarks) do names[#names+1]=localizedLandmarkName(landmark.name) end
    local option=math.max(1,math.min(#names,state.selectedLandmark))
    tImGui.SetNextItemWidth(300)
    local changed,index=tImGui.Combo(tLang.L('fre_landmark_select'),option,names)
    if changed then
        state.selectedLandmark=index
        state.project.selectedLandmark=state.selectedLandmark
        markLandmarkMarkersDirty()
        markFaceGuideDirty()
    end
    local selected=landmarks[state.selectedLandmark]
    if selected then
        tImGui.Text(string.format(tLang.L('fre_landmark_side_fmt'),localizedLandmarkName(selected.name),localizedLandmarkSide(selected.side)))
        showLandmarkReference(selected)
        local xChanged,x=tImGui.InputFloat('X##freLandmarkX',selected.position.x,0,0,'%.4f',0)
        local yChanged,y=tImGui.InputFloat('Y##freLandmarkY',selected.position.y,0,0,'%.4f',0)
        local zChanged,z=tImGui.InputFloat('Z##freLandmarkZ',selected.position.z,0,0,'%.4f',0)
        if xChanged then setLandmarkPosition(selected,{x=x,y=selected.position.y,z=selected.position.z}); markModified(); markLandmarkMarkersDirty(); markFaceGuideDirty() end
        if yChanged then setLandmarkPosition(selected,{x=selected.position.x,y=y,z=selected.position.z}); markModified(); markLandmarkMarkersDirty(); markFaceGuideDirty() end
        if zChanged then setLandmarkPosition(selected,{x=selected.position.x,y=selected.position.y,z=z}); markModified(); markLandmarkMarkersDirty(); markFaceGuideDirty() end
        tImGui.Separator()
        tImGui.Text(tLang.L('fre_landmark_surface_title'))
        tImGui.TextWrapped(tLang.L('fre_landmark_surface_help'))
        local scale=state.project.faceOrigin and state.project.faceOrigin.scale or faceGuideScale(state.meshBounds)
        local axisOptions={tLang.L('fre_landmark_surface_axis_x'),tLang.L('fre_landmark_surface_axis_y'),
            tLang.L('fre_landmark_surface_axis_z')}
        tImGui.SetNextItemWidth(220)
        local axisChanged,axisIndex=tImGui.Combo(tLang.L('fre_landmark_surface_axis'),
            state.landmarkSurfaceAxis,axisOptions)
        if axisChanged then state.landmarkSurfaceAxis=axisIndex end
        showHelpTooltip(tLang.L('fre_landmark_surface_axis_tooltip'))
        local referenceOffset=landmarkSurfaceReferenceOffset(selected.name,scale)
        local autoChanged=tImGui.Checkbox(tLang.L('fre_landmark_surface_auto'),
            state.landmarkSurfaceAutoOffset)
        if autoChanged~=state.landmarkSurfaceAutoOffset then
            state.landmarkSurfaceAutoOffset=autoChanged
            if autoChanged then state.landmarkSurfaceOffset=referenceOffset end
        end
        if state.landmarkSurfaceAutoOffset then state.landmarkSurfaceOffset=referenceOffset end
        showHelpTooltip(tLang.L('fre_landmark_surface_auto_tooltip'))
        tImGui.Text(string.format(tLang.L('fre_landmark_surface_reference_fmt'),referenceOffset))
        tImGui.BeginDisabled(state.landmarkSurfaceAutoOffset)
        local offsetChanged,newOffset=tImGui.DragFloat(tLang.L('fre_landmark_surface_offset'),
            state.landmarkSurfaceOffset,math.max(scale*0.01,0.0001),-math.max(scale,0.001),
            math.max(scale,0.001),'%.4f',0)
        if offsetChanged then state.landmarkSurfaceOffset=newOffset end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_landmark_surface_offset_tooltip'))
        if tImGui.Button(tLang.L('fre_landmark_surface_snap')) then
            snapSelectedLandmarkDepthToSurface()
        end
        showHelpTooltip(tLang.L('fre_landmark_surface_snap_tooltip'))
        local placed=tImGui.Checkbox(tLang.L('fre_landmark_placed'),selected.placed)
        if placed~=selected.placed then
            pushUndoSnapshot()
            selected.placed=placed
            syncLandmarkRelativePosition(selected)
            mirrorLandmark(selected)
            markModified(); markLandmarkMarkersDirty(); markFaceGuideDirty()
        end
    end
    tImGui.PopItemWidth()
    local report=tFacialProject.validate(state.project,true,state.topology)
    tImGui.Text(string.format(tLang.L('fre_landmark_count_fmt'),report.placedLandmarkCount,report.landmarkCount))
    showProjectMessages(report)
end

local function showExpressions()
    if state.expressions.catalogDirty then refreshExpressionCatalog() end
    local expressions=state.project.expressions or tFacialExpressions.new()
    state.project.expressions=expressions
    local catalog=state.expressions.catalog or {}
    tImGui.Text(tLang.L('fre_expression_edit_mode'))
    local previousEditMode=state.expressions.lastEditMode or state.expressions.editMode
    state.expressions.editMode=tImGui.RadioButton(tLang.L('fre_expression_mode_new'),state.expressions.editMode,1)
    tImGui.SameLine()
    state.expressions.editMode=tImGui.RadioButton(tLang.L('fre_expression_mode_edit'),state.expressions.editMode,2)
    tImGui.SameLine()
    state.expressions.editMode=tImGui.RadioButton(tLang.L('fre_expression_mode_delete'),state.expressions.editMode,3)
    local modeChanged=previousEditMode~=state.expressions.editMode
    state.expressions.lastEditMode=state.expressions.editMode
    tImGui.TextWrapped(tLang.L('fre_expressions_help'))
    if tImGui.TreeNode(tLang.L('fre_expression_blender_title')) then
        tImGui.TextWrapped(tLang.L('fre_expression_blender_help'))
        tImGui.TextWrapped(tLang.L('fre_expression_blender_targets_help'))
        tImGui.TextWrapped(tLang.L('fre_expression_blender_clips_help'))
        local automatedTask=state.automatedTargetJob
        local automatedRunning=automatedTask.status=='generating' or automatedTask.status=='importing'
        local canGenerateTargets=pathExists(state.blenderBridgePath) and #catalog==0 and not automatedRunning
        tImGui.BeginDisabled(not canGenerateTargets)
        if tImGui.Button(tLang.L('fre_expression_generate_targets')..'##freExpressionGenerateTargets',{x=360,y=0}) then
            launchBlenderFacialTargetGeneration()
        end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_expression_generate_targets_tooltip'))
        local canGenerateAutomated=state.meshDebug~=nil and #catalog==0 and not automatedRunning
        tImGui.BeginDisabled(not canGenerateAutomated)
        if tImGui.Button(tLang.L('fre_expression_generate_targets_automated')..'##freExpressionGenerateTargetsAutomated',{x=360,y=0}) then
            startAutomatedFacialTargetGeneration()
        end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_expression_generate_targets_automated_tooltip'))
        if #catalog==0 and not canGenerateTargets and not canGenerateAutomated then
            tImGui.TextDisabled(tLang.L('fre_expression_generate_targets_bridge_help'))
        end
        if automatedRunning then
            tImGui.TextWrapped(automatedTask.detail or '')
            tImGui.ProgressBar(automatedTask.progress or 0,{x=360,y=0})
        elseif automatedTask.status=='completed' then
            tImGui.TextColored({r=0.35,g=1,b=0.45,a=1},automatedTask.detail or '')
        elseif automatedTask.status=='failed' and automatedTask.detail~='' then
            tImGui.TextWrapped(automatedTask.detail)
        end
        if pathExists(state.blenderGeneratedBlendPath) then
            tImGui.TextWrapped(string.format(tLang.L('fre_expression_generated_blend_help'),
                shortName(state.blenderGeneratedBlendPath)))
        end
        tImGui.TreePop()
    end
    if #catalog==0 then
        tImGui.TextWrapped(tLang.L('fre_expressions_no_targets'))
        return
    end
    local targetNames={}
    for _,entry in ipairs(catalog) do targetNames[#targetNames+1]=entry.name end
    state.expressions.selectedTarget=math.max(1,math.min(state.expressions.selectedTarget,#targetNames))
    tImGui.Text(string.format(tLang.L('fre_expressions_target_count_fmt'),#catalog))
    local poseNames={}
    for _,pose in ipairs(expressions.poses or {}) do poseNames[#poseNames+1]=pose.name end
    local selectedPose=nil
    if state.expressions.editMode~=1 then
        if #poseNames==0 then
            tImGui.TextDisabled(tLang.L('fre_expressions_no_poses'))
            return
        end
        state.expressions.selectedPose=math.max(1,math.min(state.expressions.selectedPose,#poseNames))
        tImGui.SetNextItemWidth(280)
        local changed,index=tImGui.Combo(tLang.L('fre_expression_select'),state.expressions.selectedPose,poseNames)
        if changed then
            state.expressions.selectedPose=index
            expressions.selectedPose=index
            selectedPose=expressions.poses[index]
            if state.expressions.editMode~=1 then applyExpressionPose(selectedPose) end
        end
        selectedPose=expressions.poses[state.expressions.selectedPose]
    end
    if modeChanged and state.expressions.editMode~=1 and selectedPose then
        applyExpressionPose(selectedPose)
    end
    if state.expressions.editMode==1 then
        tImGui.SetNextItemWidth(250)
        local draftChanged,draft=tImGui.InputText(tLang.L('fre_expression_name') .. '##freExpressionName',state.expressions.nameDraft or '')
        if draftChanged then state.expressions.nameDraft=draft end
    end
    if state.expressions.editMode~=3 then
        if tImGui.BeginTable('freExpressionTargets',2,tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
            tImGui.TableSetupColumn(tLang.L('fre_expression_target_column'))
            tImGui.TableSetupColumn(tLang.L('fre_expression_weight_column'))
            tImGui.TableHeadersRow()
            for index,entry in ipairs(catalog) do
                tImGui.TableNextRow(); tImGui.TableSetColumnIndex(0)
                if tImGui.Selectable(entry.name,index==state.expressions.selectedTarget) then state.expressions.selectedTarget=index end
                tImGui.TableSetColumnIndex(1)
                local ok,current=dpCall(function() return state.mesh:getFacialWeight(entry.name) end)
                current=ok and tonumber(current) or 0
                tImGui.SetNextItemWidth(220)
                local changed,value=tImGui.SliderFloat('##freExpressionWeight' .. tostring(index),current,0,1,'%.3f')
                if changed then
                    local callOk,applied=dpCall(function() return state.mesh:setFacialWeight(entry.name,value) end)
                    if not callOk or applied==false then setStatus(tLang.L('fre_expression_apply_failed'),true) end
                end
            end
            tImGui.EndTable()
        end
    end
    if state.expressions.editMode==1 then
        if tImGui.Button(tLang.L('fre_expression_create'),{x=230,y=0}) then
            local duplicate=false
            for _,pose in ipairs(expressions.poses or {}) do if pose.name==state.expressions.nameDraft then duplicate=true end end
            if duplicate then
                setStatus(tLang.L('fre_expression_add_failed'),true)
            else
                pushUndoSnapshot()
                local pose,errorMessage=tFacialExpressions.addPose(expressions,state.expressions.nameDraft)
                if pose then
                    local weights={}
                    for _,entry in ipairs(catalog) do
                        local ok,value=dpCall(function() return state.mesh:getFacialWeight(entry.name) end)
                        if ok then weights[entry.name]=tonumber(value) or 0 end
                    end
                    tFacialExpressions.capturePose(expressions,pose,weights)
                    state.expressions.selectedPose=#expressions.poses
                    expressions.selectedPose=state.expressions.selectedPose
                    state.expressions.nameDraft=''
                    markModified(); setStatus(tLang.L('fre_expression_created'),false)
                else
                    table.remove(state.undoStack)
                    setStatus(tLang.L('fre_expression_add_failed') .. ': ' .. tostring(errorMessage or ''),true)
                end
            end
        end
        showHelpTooltip(tLang.L('fre_expression_create_tooltip'))
    elseif state.expressions.editMode==2 and selectedPose then
        if tImGui.Button(tLang.L('fre_expression_update'),{x=230,y=0}) then
            local weights={}
            for _,entry in ipairs(catalog) do
                local ok,value=dpCall(function() return state.mesh:getFacialWeight(entry.name) end)
                if ok then weights[entry.name]=tonumber(value) or 0 end
            end
            pushUndoSnapshot(); tFacialExpressions.capturePose(expressions,selectedPose,weights); markModified()
            setStatus(tLang.L('fre_expression_updated'),false)
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_expression_mirror'),{x=180,y=0}) then
            pushUndoSnapshot(); tFacialExpressions.mirrorPose(expressions,selectedPose); applyExpressionPose(selectedPose); markModified()
            setStatus(tLang.L('fre_expression_mirrored'),false)
        end
    elseif state.expressions.editMode==3 and selectedPose then
        tImGui.Text(string.format(tLang.L('fre_expression_delete_selected_fmt'),selectedPose.name))
        if tImGui.Button(tLang.L('fre_expression_delete'),{x=230,y=0}) then
            pushUndoSnapshot(); tFacialExpressions.removePose(expressions,state.expressions.selectedPose)
            state.expressions.selectedPose=math.max(1,math.min(state.expressions.selectedPose,#expressions.poses>0 and #expressions.poses or 1))
            markModified(); setStatus(tLang.L('fre_expression_deleted'),false)
        end
    end
    local report=tFacialExpressions.validate(expressions)
    if report.ok then
        textColoredWrapped({r=0.35,g=1,b=0.45,a=1},string.format(tLang.L('fre_expression_valid_fmt'),report.poseCount))
    else
        textColoredWrapped({r=1,g=0.55,b=0.2,a=1},tLang.L('fre_expression_invalid'))
        for _,message in ipairs(report.errors) do tImGui.BulletText(message) end
    end
end

local function showClips()
    tImGui.TextWrapped(tLang.L('fre_clips_help'))
    if state.expressions.catalogDirty then refreshExpressionCatalog() end
    if state.clipEditor.dirty then
        state.project.clips=tFacialClips.normalize({clips=state.project.clips}).clips
        state.clipEditor.dirty=false
    end
    local clips=state.project.clips
    local names={}
    for _,clip in ipairs(clips) do names[#names+1]=clip.name end
    if #names>0 then
        state.clipEditor.selectedClip=math.max(1,math.min(state.clipEditor.selectedClip,#names))
        state.project.selectedClip=state.clipEditor.selectedClip
        tImGui.SetNextItemWidth(280)
        local changed,index=tImGui.Combo(tLang.L('fre_clip_select'),state.clipEditor.selectedClip,names)
        if changed then
            state.clipEditor.selectedClip=index
            state.project.selectedClip=index
            state.clipEditor.keySelected=1
            state.clipEditor.time=0
            state.clipEditor.renameDraft=clips[index].name
            applyFacialWeights(tFacialClips.sample(clips[index],0,tFacialExpressions.targetNames(state.project.expressions)))
        end
    else
        tImGui.TextDisabled(tLang.L('fre_clips_no_clips'))
    end
    tImGui.SetNextItemWidth(250)
    local draftChanged,draft=tImGui.InputText(tLang.L('fre_clip_name') .. '##freClipName',state.clipEditor.nameDraft or '')
    if draftChanged then state.clipEditor.nameDraft=draft end
    if tImGui.Button(tLang.L('fre_clip_add'),{x=190,y=0}) then
        pushUndoSnapshot()
        local clipState={clips=clips,selectedClip=state.clipEditor.selectedClip}
        local clip,errorMessage=tFacialClips.addClip(clipState,state.clipEditor.nameDraft)
        if clip then
            state.project.clips=clipState.clips
            clips=state.project.clips
            state.clipEditor.selectedClip=#clips
            state.project.selectedClip=state.clipEditor.selectedClip
            state.clipEditor.nameDraft=''
            state.clipEditor.renameDraft=clip.name
            state.clipEditor.keySelected=1
            state.clipEditor.time=0
            markModified(); state.clipEditor.dirty=true
        else
            table.remove(state.undoStack)
            setStatus(tLang.L('fre_clip_add_failed') .. ': ' .. tostring(errorMessage or ''),true)
        end
    end
    local clip=clips[state.clipEditor.selectedClip]
    if not clip then return end
    if state.clipEditor.renameDraft=='' then state.clipEditor.renameDraft=clip.name end
    tImGui.Separator()
    tImGui.SetNextItemWidth(250)
    local renameChanged,rename=tImGui.InputText(tLang.L('fre_clip_rename') .. '##freClipRename',state.clipEditor.renameDraft or clip.name or '')
    if renameChanged then state.clipEditor.renameDraft=rename end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('fre_clip_apply_name'),{x=125,y=0}) then
        local candidate=(state.clipEditor.renameDraft or clip.name or ''):match('^%s*(.-)%s*$')
        if candidate=='' then
            setStatus(tLang.L('fre_clip_name_invalid'),true)
        else
            pushUndoSnapshot(); clip.name=candidate; state.clipEditor.renameDraft=candidate; markModified(); state.clipEditor.dirty=true
        end
    end
    tImGui.SetNextItemWidth(160)
    local durationChanged,duration=tImGui.DragFloat(tLang.L('fre_clip_duration'),clip.duration,0.01,0.001,0,'%.3f')
    if durationChanged then clip.duration=math.max(duration,0.001); state.clipEditor.time=math.min(state.clipEditor.time,clip.duration); markModified(); state.clipEditor.dirty=true end
    tImGui.SetNextItemWidth(160)
    local fpsChanged,fps=tImGui.DragFloat(tLang.L('fre_clip_fps'),clip.fps,0.1,1,240,'%.1f')
    if fpsChanged then clip.fps=math.max(fps,1); markModified(); state.clipEditor.dirty=true end
    local loop=tImGui.Checkbox(tLang.L('fre_clip_loop'),clip.loop==true)
    if loop~=clip.loop then clip.loop=loop; markModified(); state.clipEditor.dirty=true end
    local interpolationLabels={tLang.L('fre_clip_interpolation_step'),tLang.L('fre_clip_interpolation_linear')}
    tImGui.SetNextItemWidth(180)
    local interpolationChanged,interpolation=tImGui.Combo(tLang.L('fre_clip_interpolation'),clip.interpolation==0 and 1 or 2,interpolationLabels)
    if interpolationChanged then clip.interpolation=interpolation==1 and 0 or 1; markModified(); state.clipEditor.dirty=true end
    local poseNames={}
    for _,pose in ipairs(state.project.expressions.poses or {}) do poseNames[#poseNames+1]=pose.name end
    if not state.clipEditor.sourceInitialized then
        local initialSource=state.project.expressions.poses[state.expressions.selectedPose]
        if initialSource then applyExpressionPose(initialSource) end
        state.clipEditor.sourceInitialized=true
    end
    if #poseNames>0 then
        tImGui.SetNextItemWidth(280)
        local poseChanged,poseIndex=tImGui.Combo(tLang.L('fre_clip_expression_source'),state.expressions.selectedPose,poseNames)
        showHelpTooltip(tLang.L('fre_clip_expression_source_tooltip'))
        if poseChanged then
            state.expressions.selectedPose=poseIndex
            local selectedSource=state.project.expressions.poses[poseIndex]
            if selectedSource then applyExpressionPose(selectedSource) end
        end
    end
    if not state.clipEditor.timelineVisible then
        if tImGui.Button(tLang.L('fre_clip_show_timeline') .. '##freClipShowTimeline',{x=190,y=0}) then
            state.clipEditor.timelineVisible=true
        end
    else
        tImGui.TextDisabled(tLang.L('fre_clip_timeline_help'))
    end
end

local function showFacialClipTimelineWindow()
    if state.workspace~='clips' or not state.clipEditor.timelineVisible then return end
    local clips=state.project.clips or {}
    local clip=clips[state.clipEditor.selectedClip]
    if not clip then return end
    local screenWidth,screenHeight=mbm.getRealSizeScreen()
    local windowHeight=280
    local left=math.max(0,math.min(state.leftPanelRight or 440,screenWidth-420))
    tImGui.SetNextWindowPos({x=left,y=math.max(0,screenHeight-windowHeight)},tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x=math.max(420,screenWidth-left),y=windowHeight},tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSizeConstraints({x=420,y=210},{x=math.max(420,screenWidth),y=math.max(210,screenHeight)})
    local opened,closed=tImGui.Begin(tLang.L('fre_clip_timeline_window')..'##'..windows.clipTimeline,true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if closed then state.clipEditor.timelineVisible=false end
    if opened then
        tImGui.Text(string.format('%s: %s',tLang.L('fre_clip_select'),clip.name or ''))
        tImGui.SameLine()
        tImGui.Text(string.format(tLang.L('fre_clip_timeline_selection_fmt'),state.clipEditor.keySelected or 1))
        tImGui.TextWrapped(tLang.L('fre_clip_timeline_help'))
        tImGui.SetNextItemWidth(420)
        local timeChanged,time=tImGui.SliderFloat(tLang.L('fre_clip_time')..'##freClipTimelineTime',state.clipEditor.time,0,clip.duration,'%.3f s')
        showHelpTooltip(tLang.L('fre_clip_time_tooltip'))
        if timeChanged then
            state.clipEditor.time=time
            state.clipEditor.playing=false
            applyFacialWeights(tFacialClips.sample(clip,time,tFacialExpressions.targetNames(state.project.expressions)))
        end
        if tImGui.Button(tLang.L('fre_clip_insert_expression_key')..'##freClipTimelineInsert',{x=230,y=0}) then
            local selectedPose=state.project.expressions.poses[state.expressions.selectedPose]
            pushUndoSnapshot()
            local weights=selectedPose and tFacialExpressions.applyPose(state.project.expressions,selectedPose) or {}
            tFacialClips.insertKey(clip,state.clipEditor.time,weights)
            state.clipEditor.keySelected=1; markModified(); state.clipEditor.dirty=true
        end
        showHelpTooltip(tLang.L('fre_clip_insert_expression_key_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_clip_capture_key')..'##freClipTimelineCapture',{x=190,y=0}) then
            local weights={}
            for _,entry in ipairs(state.expressions.catalog or {}) do
                local ok,value=dpCall(function() return state.mesh:getFacialWeight(entry.name) end)
                if ok then weights[entry.name]=tonumber(value) or 0 end
            end
            pushUndoSnapshot(); tFacialClips.insertKey(clip,state.clipEditor.time,weights); markModified(); state.clipEditor.dirty=true
        end
        showHelpTooltip(tLang.L('fre_clip_capture_key_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_clip_play')..'##freClipTimelinePlay',{x=110,y=0}) then state.clipEditor.playing=not state.clipEditor.playing end
        showHelpTooltip(tLang.L('fre_clip_play_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_clip_stop')..'##freClipTimelineStop',{x=90,y=0}) then
            state.clipEditor.playing=false
            state.clipEditor.time=0
            state.clipEditor.keySelected=1
            applyFacialWeights(tFacialClips.sample(clip,0,
                tFacialExpressions.targetNames(state.project.expressions)))
        end
        showHelpTooltip(tLang.L('fre_clip_stop_tooltip'))
        local keys=clip.keys or {}
        if tImGui.BeginTable('freFacialClipTimelineKeys',3,tImGui.Flags('ImGuiTableFlags_Borders','ImGuiTableFlags_RowBg')) then
            tImGui.TableSetupColumn(tLang.L('fre_clip_key_column'))
            tImGui.TableSetupColumn(tLang.L('fre_clip_time_column'))
            tImGui.TableSetupColumn(tLang.L('fre_clip_weight_count_column'))
            tImGui.TableHeadersRow()
            for index,key in ipairs(keys) do
                tImGui.TableNextRow(); tImGui.TableSetColumnIndex(0)
                if tImGui.Selectable(tLang.L('fre_clip_key')..' '..tostring(index)..'##freClipTimelineKey'..tostring(index),index==state.clipEditor.keySelected) then
                    state.clipEditor.keySelected=index
                    state.clipEditor.time=key.time
                    state.clipEditor.playing=false
                    applyFacialWeights(tFacialClips.sample(clip,key.time,tFacialExpressions.targetNames(state.project.expressions)))
                end
                tImGui.TableSetColumnIndex(1); tImGui.Text(string.format('%.3f',key.time))
                local count=0
                for _,value in pairs(key.weights or {}) do if math.abs(tonumber(value) or 0)>0.0001 then count=count+1 end end
                tImGui.TableSetColumnIndex(2); tImGui.Text(tostring(count))
            end
            tImGui.EndTable()
        end
        tImGui.BeginDisabled(#keys==0)
        if tImGui.Button(tLang.L('fre_clip_remove_key')..'##freClipTimelineRemove',{x=150,y=0}) then
            pushUndoSnapshot()
            tFacialClips.removeKey(clip,state.clipEditor.keySelected)
            state.clipEditor.keySelected=math.max(1,math.min(state.clipEditor.keySelected,#keys-1))
            markModified(); state.clipEditor.dirty=true
        end
        showHelpTooltip(tLang.L('fre_clip_remove_key_tooltip'))
        tImGui.EndDisabled()
        local report=tFacialClips.validate(clip)
        if report.ok then
            textColoredWrapped({r=0.35,g=1,b=0.45,a=1},string.format(tLang.L('fre_clip_valid_fmt'),report.keyCount))
        else
            textColoredWrapped({r=1,g=0.55,b=0.2,a=1},tLang.L('fre_clip_invalid'))
            for _,message in ipairs(report.errors) do tImGui.BulletText(message) end
        end
    end
    tImGui.End()
end

local function updateFacialClipPlayback(delta)
    if state.workspace~='clips' or not state.clipEditor.playing or not state.mesh then return end
    local clip=state.project.clips and state.project.clips[state.clipEditor.selectedClip]
    if not clip then state.clipEditor.playing=false; return end
    state.clipEditor.time=state.clipEditor.time+math.max(tonumber(delta) or 0,0)
    if state.clipEditor.time>clip.duration then
        if clip.loop then state.clipEditor.time=state.clipEditor.time%clip.duration
        else state.clipEditor.time=clip.duration; state.clipEditor.playing=false end
    end
    applyFacialWeights(tFacialClips.sample(clip,state.clipEditor.time,tFacialExpressions.targetNames(state.project.expressions)))
end

local function updatePreviewAuthoredClipPlayback(delta)
    if state.workspace~='preview' or not state.preview.authoredClipPlaying or not state.mesh then return end
    local clips=state.project.clips or {}
    local clip=clips[state.preview.authoredClipSelected]
    if not clip then state.preview.authoredClipPlaying=false; return end
    state.preview.authoredClipTime=state.preview.authoredClipTime+math.max(tonumber(delta) or 0,0)
    if state.preview.authoredClipTime>clip.duration then
        if clip.loop then state.preview.authoredClipTime=state.preview.authoredClipTime%clip.duration
        else state.preview.authoredClipTime=clip.duration; state.preview.authoredClipPlaying=false end
    end
    applyFacialWeights(tFacialClips.sample(clip,state.preview.authoredClipTime,
        tFacialExpressions.targetNames(state.project.expressions)))
end

local function showPreview()
    tImGui.TextWrapped(tLang.L('fre_preview_help'))
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    if state.expressions.catalogDirty then refreshExpressionCatalog() end
    local expressions=state.project.expressions or tFacialExpressions.new()
    state.project.expressions=expressions
    if state.preview.skeletalDirty then refreshPreviewSkeletalCatalog() end
    local comparison=state.facialComparison
    local selected=comparison.enabled and (comparison.mode=='lbs_dqs' and 3 or 2) or 1
    tImGui.Text(tLang.L('fre_preview_compare_mode'))
    local options={
        {label=tLang.L('fre_preview_compare_none'),value=1},
        {label=tLang.L('fre_preview_gpu_cpu_compare'),value=2},
        {label=tLang.L('fre_preview_lbs_dqs_compare'),value=3},
    }
    for _,option in ipairs(options) do
        local nextSelected=tImGui.RadioButton(option.label,selected,option.value)
        if nextSelected~=selected then
            local mode=option.value==3 and 'lbs_dqs' or 'gpu_cpu'
            local rebuilt,reason=rebuildFacialComparison(option.value~=1,mode)
            if rebuilt then
                selected=nextSelected
            else
                comparison.enabled=false
                if state.meshBounds then frameCamera(state.meshBounds) end
                setStatus(tLang.L('fre_preview_compare_failed') .. ': ' ..
                    tLang.L(reason or 'fre_preview_gpu_cpu_reason_unknown'),true)
            end
        end
    end
    showHelpTooltip(tLang.L('fre_preview_compare_tooltip'))
    if comparison.enabled and comparison.ready then
        tImGui.TextWrapped(tLang.L(comparison.mode=='lbs_dqs' and
            'fre_preview_lbs_dqs_layout' or 'fre_preview_gpu_cpu_layout'))
        tImGui.Text(tLang.L('fre_preview_left'))
        tFacialPlayback.drawReport(state.mesh,dpCall)
        tImGui.Text(tLang.L('fre_preview_right'))
        tFacialPlayback.drawReport(comparison.mesh,dpCall)
    end
    tImGui.Text(tLang.L('fre_preview_skeletal_title'))
    local skeletalAnimations=state.preview.skeletalAnimations or {}
    if #skeletalAnimations==0 then
        tImGui.TextDisabled(tLang.L('fre_preview_no_skeletal'))
    else
        tImGui.SetNextItemWidth(280)
        local skeletalChanged,skeletalIndex=tImGui.Combo(
            tLang.L('fre_preview_skeletal_clip'),state.preview.selectedSkeletal,skeletalAnimations)
        if skeletalChanged then state.preview.selectedSkeletal=skeletalIndex end
        showHelpTooltip(tLang.L('fre_preview_skeletal_clip_tooltip'))
        local skeletalName=skeletalAnimations[state.preview.selectedSkeletal]
        if tImGui.Button(tLang.L('fre_preview_skeletal_play') .. '##frePreviewSkeletalPlay',{x=230,y=0}) then
            local ok,played=dpCall(function() return state.mesh:playSkeletalAnimation(skeletalName) end)
            if ok and played~=false then
                state.preview.activeSkeletal=skeletalName
                setStatus(string.format(tLang.L('fre_preview_skeletal_played_fmt'),skeletalName),false)
            else setStatus(tLang.L('fre_preview_skeletal_failed'),true) end
        end
        showHelpTooltip(tLang.L('fre_preview_skeletal_play_tooltip'))
        if tImGui.Button(tLang.L('fre_preview_skeletal_pause') .. '##frePreviewSkeletalPause',{x=90,y=0}) then
            dpCall(function() return state.mesh:pauseSkeletalAnimation() end)
        end
        showHelpTooltip(tLang.L('fre_preview_skeletal_pause_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_preview_skeletal_resume') .. '##frePreviewSkeletalResume',{x=100,y=0}) then
            dpCall(function() return state.mesh:resumeSkeletalAnimation() end)
        end
        showHelpTooltip(tLang.L('fre_preview_skeletal_resume_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_preview_skeletal_stop') .. '##frePreviewSkeletalStop',{x=90,y=0}) then
            dpCall(function() return state.mesh:stopSkeletalAnimation() end)
        end
        showHelpTooltip(tLang.L('fre_preview_skeletal_stop_tooltip'))
        local durationOk,duration=dpCall(function()
            return state.mesh:getSkeletalAnimationDuration(state.preview.selectedSkeletal)
        end)
        local timeOk,time=dpCall(function() return state.mesh:getSkeletalAnimationTime() end)
        if durationOk and timeOk and duration and time then
            tImGui.Text(string.format(tLang.L('fre_preview_skeletal_time_fmt'),time,duration))
        end
    end
    tImGui.Separator()
    local poses=expressions.poses or {}
    if #poses==0 then
        tImGui.TextDisabled(tLang.L('fre_preview_no_expressions'))
    else
        local names={}
        for _,pose in ipairs(poses) do names[#names+1]=pose.name end
        state.preview.selectedPose=math.max(1,math.min(state.preview.selectedPose,#names))
        tImGui.SetNextItemWidth(280)
        local changed,index=tImGui.Combo(tLang.L('fre_preview_expression'),state.preview.selectedPose,names)
        if changed then state.preview.selectedPose=index end
        showHelpTooltip(tLang.L('fre_preview_expression_tooltip'))
        local selectedPose=poses[state.preview.selectedPose]
        if selectedPose then
            if tImGui.Button(tLang.L('fre_preview_apply_expression') .. '##frePreviewApplyExpression',{x=230,y=0}) then
                if applyExpressionPose(selectedPose) then setStatus(tLang.L('fre_preview_applied'),false)
                else setStatus(tLang.L('fre_expression_apply_failed'),true) end
            end
            showHelpTooltip(tLang.L('fre_preview_apply_expression_tooltip'))
            if tImGui.Button(tLang.L('fre_preview_reset_expression') .. '##frePreviewResetExpression',{x=230,y=0}) then
                local ok,reset=dpCall(function() return state.mesh:resetFacialWeights() end)
                if ok and reset~=false then syncFacialComparisonWeights() end
                if ok and reset~=false then setStatus(tLang.L('fre_preview_reset'),false)
                else setStatus(tLang.L('fre_expression_apply_failed'),true) end
            end
            showHelpTooltip(tLang.L('fre_preview_reset_expression_tooltip'))
            tImGui.Text(string.format(tLang.L('fre_preview_selected_fmt'),selectedPose.name))
            local active=0
            for _,value in pairs(selectedPose.weights or {}) do if (tonumber(value) or 0)>0.0001 then active=active+1 end end
            tImGui.Text(string.format(tLang.L('fre_preview_active_targets_fmt'),active))
        end
    end
    tImGui.Separator()
    tImGui.Text(tLang.L('fre_preview_authored_clips_title'))
    tImGui.TextWrapped(tLang.L('fre_preview_authored_clips_help'))
    local authoredClips=state.project.clips or {}
    if #authoredClips==0 then
        tImGui.TextDisabled(tLang.L('fre_preview_authored_clips_none'))
    else
        local authoredNames={}
        for _,clip in ipairs(authoredClips) do authoredNames[#authoredNames+1]=clip.name end
        state.preview.authoredClipSelected=math.max(1,math.min(state.preview.authoredClipSelected or 1,#authoredNames))
        tImGui.SetNextItemWidth(280)
        local authoredChanged,authoredIndex=tImGui.Combo(tLang.L('fre_preview_authored_clip'),
            state.preview.authoredClipSelected,authoredNames)
        if authoredChanged then
            state.preview.authoredClipSelected=authoredIndex
            state.preview.authoredClipTime=0
            state.preview.authoredClipPlaying=false
            applyFacialWeights(tFacialClips.sample(authoredClips[authoredIndex],0,
                tFacialExpressions.targetNames(state.project.expressions)))
        end
        showHelpTooltip(tLang.L('fre_preview_authored_clip_tooltip'))
        local authoredClip=authoredClips[state.preview.authoredClipSelected]
        if authoredClip then
            if not state.preview.authoredClipInitialized then
                state.preview.authoredClipTime=0
                state.preview.authoredClipPlaying=false
                applyFacialWeights(tFacialClips.sample(authoredClip,0,
                    tFacialExpressions.targetNames(state.project.expressions)))
                state.preview.authoredClipInitialized=true
            end
            tImGui.SetNextItemWidth(360)
            local authoredTimeChanged,authoredTime=tImGui.SliderFloat(
                tLang.L('fre_clip_time')..'##frePreviewAuthoredClipTime',state.preview.authoredClipTime,
                0,authoredClip.duration,'%.3f s')
            if authoredTimeChanged then
                state.preview.authoredClipTime=authoredTime
                state.preview.authoredClipPlaying=false
                applyFacialWeights(tFacialClips.sample(authoredClip,authoredTime,
                    tFacialExpressions.targetNames(state.project.expressions)))
            end
            if tImGui.Button(tLang.L('fre_clip_play')..'##frePreviewAuthoredClipPlay',{x=110,y=0}) then
                state.preview.authoredClipPlaying=not state.preview.authoredClipPlaying
            end
            showHelpTooltip(tLang.L('fre_preview_authored_clip_play_tooltip'))
            tImGui.SameLine()
            if tImGui.Button(tLang.L('fre_preview_authored_clip_stop')..'##frePreviewAuthoredClipStop',{x=90,y=0}) then
                state.preview.authoredClipPlaying=false
                state.preview.authoredClipTime=0
                applyFacialWeights(tFacialClips.sample(authoredClip,0,
                    tFacialExpressions.targetNames(state.project.expressions)))
            end
            showHelpTooltip(tLang.L('fre_preview_authored_clip_stop_tooltip'))
            local authoredReport=tFacialClips.validate(authoredClip)
            if authoredReport.ok then
                textColoredWrapped({r=0.35,g=1,b=0.45,a=1},string.format(
                    tLang.L('fre_clip_valid_fmt'),authoredReport.keyCount))
            else
                textColoredWrapped({r=1,g=0.55,b=0.2,a=1},tLang.L('fre_clip_invalid'))
            end
        end
    end
    tImGui.Separator()
    tImGui.Text(tLang.L('fre_preview_runtime_title'))
    tFacialPlayback.draw(state.preview,nil,state.mesh,true,dpCall,syncFacialComparisonWeights)
end

local function showRig()
    tImGui.TextWrapped(tLang.L('fre_rig_help'))
    if state.bindingPlan.dirty then refreshBindingPlan() end
    if state.rigReview.dirty then refreshRigReview() end
    if state.facialSkeleton.dirty then refreshFacialSkeletonBinding() end
    if state.weightAssignment.dirty then refreshWeightAssignment() end
    local reviewData=state.rigReview.data or {report={ok=false,errors={},warnings={}},
        rig={valid=false,bones={},roleBindings={}},binding={roles={},masks={},landmarks={},vertexCount=0},
        bridgeReport={entries={},warnings={}},bindingReview={roles={},totalMaskedVertices=0,overlapCount=0}}
    local report=reviewData.report
    local rig=reviewData.rig
    local binding=reviewData.binding
    local bridgeReport=reviewData.bridgeReport
    local bindingReview=reviewData.bindingReview

    -- Keep the two materialization steps visible at the top of Rig. Reports
    -- below are read-only diagnostics; these buttons are the actual rigging
    -- operations on the loaded MeshDebug asset.
    tImGui.Text(tLang.L('fre_rig_actions_title'))
    tImGui.TextWrapped(tLang.L('fre_rig_actions_help'))
    tImGui.Separator()
    local facialSkeleton=state.facialSkeleton.data
    tImGui.Text(tLang.L('fre_facial_skeleton_title'))
    if facialSkeleton then
        tImGui.Text(string.format(tLang.L('fre_facial_skeleton_summary_fmt'),
            facialSkeleton.existingCount or 0,facialSkeleton.missingCount or 0))
        local rootRepair=facialSkeleton.rootParentNeedsRepair==true
        local canApply=facialSkeleton.valid==true and
            ((facialSkeleton.missingCount or 0)>0 or rootRepair)
        tImGui.BeginDisabled(not canApply)
        local applyLabel=rootRepair and tLang.L('fre_facial_skeleton_repair_root') or
            tLang.L('fre_facial_skeleton_apply')
        if tImGui.Button(applyLabel..'##freRigActionSkeleton',{x=300,y=0}) then
            applyFacialSkeletonBinding()
        end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_facial_skeleton_apply_tooltip'))
        if facialSkeleton.valid~=true then
            tImGui.TextDisabled(tLang.L('fre_facial_skeleton_blocked'))
        elseif not canApply then
            tImGui.TextWrapped(tLang.L('fre_rig_action_skeleton_ready'))
        end
    else
        tImGui.TextDisabled(tLang.L('fre_facial_skeleton_blocked'))
    end
    tImGui.Separator()
    local assignment=state.weightAssignment.data
    tImGui.Text(tLang.L('fre_weight_assignment_title'))
    tImGui.TextWrapped(tLang.L('fre_rig_action_weights_help'))
    if assignment then
        tImGui.Text(string.format(tLang.L('fre_weight_assignment_summary_fmt'),
            assignment.readyCount or 0,assignment.blockedCount or 0,assignment.totalVertexCount or 0))
        tImGui.BeginDisabled(assignment.valid~=true)
        if tImGui.Button(tLang.L('fre_weight_assignment_apply')..'##freRigActionWeights',{x=390,y=0}) then
            applyRigidFacialWeights()
        end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_weight_assignment_apply_tooltip'))
        local canSave=canExportRuntimeMsh()
        local includeClipsChanged,includeClips=tImGui.Checkbox(
            tLang.L('fre_export_msh_include_project_clips')..'##freRigIncludeProjectClips',
            state.exportWindow.includeProjectFacialClips~=false)
        if includeClipsChanged then state.exportWindow.includeProjectFacialClips=includeClips end
        showHelpTooltip(tLang.L('fre_export_msh_include_project_clips_tooltip'))
        tImGui.BeginDisabled(not canSave)
        if tImGui.Button(tLang.L('fre_weight_assignment_save')..'##freRigActionSave',{x=300,y=0}) then
            saveAssignedMesh()
        end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_weight_assignment_save_tooltip'))
        if assignment.valid~=true then
            tImGui.TextDisabled(tLang.L('fre_weight_assignment_blocked'))
        elseif not canSave then
            tImGui.TextDisabled(tLang.L('fre_rig_action_save_help'))
        end
    else
        tImGui.TextDisabled(tLang.L('fre_weight_assignment_blocked'))
    end
    tImGui.Separator()

    if tImGui.TreeNode(tLang.L('fre_rig_reports_title')) then
    if report.ok and rig.valid then
        textColoredWrapped({r=0.35,g=1,b=0.45,a=1},tLang.L('fre_validation_ok'))
    else
        textColoredWrapped({r=1,g=0.55,b=0.2,a=1},tLang.L('fre_validation_pending'))
    end
    showProjectMessages(report)
    tImGui.Separator()
    tImGui.Text(string.format(tLang.L('fre_binding_roles_fmt'),#binding.roles))
    tImGui.Text(string.format(tLang.L('fre_binding_masks_fmt'),#binding.masks))
    tImGui.Text(string.format(tLang.L('fre_binding_landmarks_fmt'),#binding.landmarks))
    tImGui.Text(string.format(tLang.L('fre_binding_vertices_fmt'),binding.vertexCount))
    tImGui.TextWrapped(tLang.L('fre_binding_preview_help'))
    if tImGui.TreeNode(tLang.L('fre_binding_review_title')) then
        tImGui.TextWrapped(tLang.L('fre_binding_review_help'))
        tImGui.Text(string.format(tLang.L('fre_binding_review_summary_fmt'),
            bindingReview.totalMaskedVertices,bindingReview.overlapCount))
        for _,entry in ipairs(bindingReview.roles or {}) do
            local object=entry.object~='' and entry.object or tLang.L('fre_unassigned')
            tImGui.TextWrapped(string.format(tLang.L('fre_binding_review_role_fmt'),entry.name,
                object,localizedBindingReviewMode(entry.mode),entry.vertexCount or entry.maskVertexCount,
                localizedBindingReviewStatus(entry.status)))
            for _,issue in ipairs(entry.issues or {}) do
                tImGui.TextWrapped(tLang.L('fre_warning_prefix') .. issue)
            end
        end
        if bindingReview.overlapCount>0 then
            tImGui.TextWrapped(tLang.L('fre_binding_review_overlap_help'))
        end
        tImGui.TreePop()
    end
    tImGui.Separator()
    for _,message in ipairs(rig.errors or {}) do
        tImGui.TextWrapped(tLang.L('fre_error_prefix') .. localizedRigMessage(message))
    end
    for _,message in ipairs(rig.warnings or {}) do
        tImGui.TextWrapped(tLang.L('fre_warning_prefix') .. localizedRigMessage(message))
    end
    tImGui.Text(string.format(tLang.L('fre_rig_bones_fmt'),#(rig.bones or {})))
    tImGui.Text(string.format(tLang.L('fre_rig_role_bindings_fmt'),#(rig.roleBindings or {})))
    tImGui.Text(string.format(tLang.L('fre_bridge_entries_fmt'),#(bridgeReport.entries or {})))
    for _,message in ipairs(bridgeReport.warnings or {}) do
        tImGui.TextWrapped(tLang.L('fre_warning_prefix') .. localizedBridgeMessage(message))
    end
    if state.facialSkeleton.dirty then refreshFacialSkeletonBinding() end
    if tImGui.TreeNode(tLang.L('fre_facial_skeleton_title')) then
        tImGui.TextWrapped(tLang.L('fre_facial_skeleton_help'))
        local facialSkeleton=state.facialSkeleton.data
        if facialSkeleton then
            tImGui.Text(string.format(tLang.L('fre_facial_skeleton_summary_fmt'),
                facialSkeleton.existingCount,facialSkeleton.missingCount))
            if facialSkeleton.rootParentName then
                tImGui.TextWrapped(string.format(tLang.L('fre_facial_skeleton_parent_fmt'),
                    facialSkeleton.rootParentName))
            end
            for _,entry in ipairs(facialSkeleton.entries or {}) do
                local status=entry.status=='existing' and tLang.L('fre_facial_skeleton_existing') or
                    entry.status=='missing' and tLang.L('fre_facial_skeleton_missing') or
                    tLang.L('fre_facial_skeleton_blocked_status')
                tImGui.TextWrapped(string.format(tLang.L('fre_facial_skeleton_entry_fmt'),
                    entry.name,status))
                if entry.reason then
                    tImGui.TextWrapped(tLang.L('fre_error_prefix') .. localizedFacialSkeletonReason(entry.reason))
                end
            end
            for _,reason in ipairs(facialSkeleton.errors or {}) do
                tImGui.TextWrapped(tLang.L('fre_error_prefix') .. localizedFacialSkeletonReason(reason))
            end
        end
        tImGui.TreePop()
    end
    local plan=state.bindingPlan.data
    if tImGui.TreeNode(tLang.L('fre_binding_plan_title')) then
        tImGui.TextWrapped(tLang.L('fre_binding_plan_help'))
        if plan then
            tImGui.Text(string.format(tLang.L('fre_binding_plan_summary_fmt'),plan.readyCount,plan.blockedCount))
            for _,entry in ipairs(plan.entries or {}) do
                tImGui.TextWrapped(string.format(tLang.L('fre_binding_plan_role_fmt'),entry.name,
                    localizedBindingPlanMethod(entry.method),entry.vertexCount,
                    entry.status=='ready' and tLang.L('fre_binding_plan_ready') or tLang.L('fre_binding_plan_blocked')))
                if #entry.bones>0 then
                    tImGui.TextWrapped(string.format(tLang.L('fre_binding_plan_bones_fmt'),table.concat(entry.bones,', ')))
                end
                for _,reason in ipairs(entry.reasons or {}) do
                    tImGui.TextWrapped(tLang.L('fre_warning_prefix') .. localizedBindingPlanReason(reason))
                end
            end
            for _,reason in ipairs(plan.errors or {}) do
                tImGui.TextWrapped(tLang.L('fre_error_prefix') .. localizedBindingPlanReason(reason))
            end
        end
        tImGui.TreePop()
    end
    if state.weightAssignment.dirty then refreshWeightAssignment() end
    if tImGui.TreeNode(tLang.L('fre_weight_assignment_title')) then
        tImGui.TextWrapped(tLang.L('fre_weight_assignment_help'))
        local assignment=state.weightAssignment.data
        if assignment then
            tImGui.Text(string.format(tLang.L('fre_weight_assignment_summary_fmt'),
                assignment.readyCount,assignment.blockedCount,assignment.totalVertexCount))
            for _,entry in ipairs(assignment.entries or {}) do
                local boneLabel=entry.bone or (entry.method=='facial_skeleton' and
                    tLang.L('fre_weight_assignment_facial_bones') or tLang.L('fre_unassigned'))
                tImGui.TextWrapped(string.format(tLang.L('fre_weight_assignment_role_fmt'),entry.name,
                    boneLabel,#(entry.vertices or {}),
                    entry.status=='ready' and tLang.L('fre_weight_assignment_ready') or
                        tLang.L('fre_weight_assignment_blocked_status')))
                for _,reason in ipairs(entry.reasons or {}) do
                    tImGui.TextWrapped(tLang.L('fre_warning_prefix') .. localizedWeightAssignmentReason(reason))
                end
            end
            for _,reason in ipairs(assignment.errors or {}) do
                tImGui.TextWrapped(tLang.L('fre_error_prefix') .. localizedWeightAssignmentReason(reason))
            end
            for _,reason in ipairs(assignment.warnings or {}) do
                tImGui.TextWrapped(tLang.L('fre_warning_prefix') .. localizedWeightAssignmentReason(reason))
            end
        end
        tImGui.TreePop()
    end
    tImGui.TreePop()
    end
    tImGui.Separator()
    if tImGui.TreeNode(tLang.L('fre_rig_blender_title')) then
    tImGui.Text(tLang.L('fre_blender_workflow_title'))
    tImGui.TextWrapped(tLang.L('fre_blender_workflow_runtime_help'))
    tImGui.TextWrapped(tLang.L('fre_blender_workflow_purpose_help'))
    tImGui.TextWrapped(tLang.L('fre_blender_workflow_limits_help'))
    local targetCount=0
    if state.mesh then
        local ok,count=dpCall(function() return state.mesh:getTotalFacialTargets() end)
        if ok then targetCount=math.max(0,tonumber(count) or 0) end
    end
    if targetCount>0 then
        textColoredWrapped({r=0.35,g=1,b=0.45,a=1},
            string.format(tLang.L('fre_blender_targets_ready_fmt'),targetCount))
    else
        textColoredWrapped({r=1,g=0.65,b=0.25,a=1},tLang.L('fre_blender_targets_missing_help'))
    end
    if rig.valid then
        textColoredWrapped({r=0.35,g=1,b=0.45,a=1},tLang.L('fre_rig_preview_ready'))
        tImGui.TextWrapped(tLang.L('fre_rig_preview_help'))
        tImGui.TextWrapped(tLang.L('fre_bridge_help'))
        tImGui.BeginDisabled(not report.ok or not bridgeReport.valid)
        if tImGui.Button(tLang.L('fre_export_blender_bridge')) then exportBlenderBridge() end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_export_blender_bridge_tooltip'))
        local canOpenBlender=state.blenderBridgePath~=nil and pathExists(state.blenderBridgePath)
        tImGui.BeginDisabled(not canOpenBlender)
        if tImGui.Button(tLang.L('fre_open_blender_facial_workflow')) then launchBlenderFacialRig() end
        tImGui.EndDisabled()
        showHelpTooltip(tLang.L('fre_open_blender_facial_workflow_tooltip'))
        if not canOpenBlender then
            tImGui.TextDisabled(tLang.L('fre_blender_export_first_help'))
        end
    else
        tImGui.TextWrapped(tLang.L('fre_rig_preview_pending'))
    end
    if tImGui.TreeNode(tLang.L('fre_bridge_report')) then
        for _,entry in ipairs(bridgeReport.entries or {}) do
            local object=entry.object~='' and entry.object or tLang.L('fre_unassigned')
            tImGui.BulletText(string.format('%s -> %s (%s, %d)',entry.name,object,entry.status,entry.maskCount))
        end
        tImGui.TreePop()
    end
    tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('fre_rig_structure_reports_title')) then
    if tImGui.TreeNode(tLang.L('fre_rig_bones')) then
        for _,bone in ipairs(rig.bones or {}) do
            tImGui.BulletText(bone.id .. '  (' .. (bone.parent or tLang.L('fre_rig_root')) .. ')')
        end
        tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('fre_rig_bindings')) then
        for _,bindingRow in ipairs(rig.roleBindings or {}) do
            local maskCount=#(bindingRow.maskIndices or {})
            tImGui.BulletText(string.format('%s -> %s (%d)',bindingRow.name,bindingRow.binding,maskCount))
        end
        tImGui.TreePop()
    end
    tImGui.TreePop()
    end
end

local function showPlaceholder(key)
    tImGui.TextWrapped(tLang.L(key))
    tImGui.TextDisabled(tLang.L('fre_future_workspace'))
end

local function showWorkspaceTree()
    tImGui.Text(tLang.L('fre_workspaces'))
    local workspaces={
        {'setup','fre_setup'}, {'face_position','fre_face_position'}, {'landmarks','fre_landmarks'}, {'masks','fre_masks'},
        {'rig','fre_rig'}, {'targets','fre_targets'}, {'expressions','fre_expressions'}, {'clips','fre_clips'}, {'preview','fre_preview'}
    }
    for _,entry in ipairs(workspaces) do
        local label=tLang.L(entry[2])
        local flags=state.workspace==entry[1] and tImGui.Flags('ImGuiTreeNodeFlags_Selected') or 0
        tImGui.SetNextItemOpen(state.workspace==entry[1],tImGui.Flags('ImGuiCond_Always'))
        local opened=tImGui.TreeNodeEx(label .. '##freWorkspace_' .. entry[1],flags)
        if tImGui.IsItemClicked() then selectWorkspace(entry[1]) end
        if opened then
            if state.workspace==entry[1] then
                if entry[1]=='setup' then showSetup()
                elseif entry[1]=='face_position' then showFacePosition()
                elseif entry[1]=='masks' then showMasks()
                elseif entry[1]=='landmarks' then showLandmarks()
                elseif entry[1]=='rig' then showRig()
                elseif entry[1]=='targets' then showTargets()
                elseif entry[1]=='expressions' then showExpressions()
                elseif entry[1]=='clips' then showClips()
                elseif entry[1]=='preview' then showPreview() end
            end
            tImGui.TreePop()
        end
    end
end

local function showMainPanel()
    local flags=state.windowMove and 0 or noMoveFlag
    tUtil.setInitialWindowPositionLeft(windows.main,0,24,560)
    tImGui.SetNextWindowSize({x=560,y=720},firstUseWindowFlag)
    tImGui.SetNextWindowSizeConstraints({x=560,y=400},{x=1200,y=1400})
    local opened=tImGui.Begin(tLang.L('fre_title')..'##'..windows.main,true,flags)
    local position=tImGui.GetWindowPos()
    local size=tImGui.GetWindowSize()
    state.leftPanelRight=(position.x or 0)+(size.x or 440)
    if opened then
        if state.projectPath then
            tImGui.Text(shortName(state.projectPath) .. (state.modified and ' *' or ''))
        else
            tImGui.TextDisabled(tLang.L('fre_unsaved_project'))
        end
        showStatus()
        if state.controlDown and (state.workspace=='face_position' or state.workspace=='landmarks') then
            tImGui.TextColored({r=1,g=0.8,b=0.25,a=1},tLang.L('fre_ctrl_pressed'))
            local hintKey=state.workspace=='face_position' and 'fre_ctrl_face_hint' or
                'fre_ctrl_landmarks_hint'
            tImGui.TextWrapped(tLang.L(hintKey))
        end
        tImGui.Separator()
        showWorkspaceTree()
    end
    tImGui.End()
end

local function showCameraPanel()
    tUtil.setInitialWindowPositionRight(windows.camera,0,25,330)
    tImGui.SetNextWindowSize({x=330,y=300},tImGui.Flags('ImGuiCond_Always'))
    tImGui.SetNextWindowSizeConstraints({x=330,y=300},{x=330,y=300})
    local opened=tImGui.Begin(tLang.L('fre_camera')..'##'..windows.camera,true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if opened then
        if tImGui.Button(tLang.L('fre_frame_mesh')) then frameCamera(state.meshBounds) end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_frame_face')) then frameFaceCamera() end
        tImGui.Separator()
        if state.workspace=='landmarks' then
            local selectedLandmark=state.project.landmarks[state.selectedLandmark]
            local canFrameLandmark=selectedLandmark and selectedLandmark.placed and selectedLandmark.position
            tImGui.BeginDisabled(not canFrameLandmark)
            if tImGui.Button(tLang.L('fre_frame_landmark')) then frameLandmarkCamera() end
            tImGui.EndDisabled()
            tImGui.SameLine()
        end
        local visible=tImGui.Checkbox(tLang.L('fre_show_mesh'),state.mesh and state.mesh.visible or false)
        if state.mesh and visible~=state.mesh.visible then state.mesh.visible=visible; markLandmarkMarkersDirty() end
        tImGui.Separator()
        if tUtil.drawOrbitGizmo(state.camera,{size=110}) then applyCamera() end
        tImGui.Separator()
        tImGui.Text(string.format(tLang.L('fre_camera_distance_fmt'),state.camera.distance))
        tImGui.TextWrapped(tLang.L('fre_camera_help'))
    end
    tImGui.End()
end

local function updateCameraKeyboard(delta)
    if tImGui.GetWantCaptureKeyboard() then return end
    if not state.cameraMove then return end
    local speed=state.camera.distance*0.8*math.max(delta or 0,0)
    state.camera.fz=state.camera.fz+(state.cameraMove.forward or 0)*speed
    state.camera.fx=state.camera.fx+(state.cameraMove.right or 0)*speed
    state.camera.fy=state.camera.fy+(state.cameraMove.vertical or 0)*speed
    applyCamera()
end

function onInitScene()
    state.facialDqsSupported=mbm.get("USE_OPENGL_ES") or mbm.get("USE_DIRECTX9") or
        mbm.get("USE_DIRECTX11") or mbm.get("USE_METAL")
    camera3d=mbm.getCamera('3d')
    camera3d:setFar(9999999)
    noMoveFlag=tImGui.Flags('ImGuiWindowFlags_NoMove')
    firstUseWindowFlag=tImGui.Flags('ImGuiCond_FirstUseEver')
    tUtil.sMessageOverlay=tLang.L('fre_welcome')
    state.cameraMove={forward=0,right=0,vertical=0}
    mbm.setColor(0.08,0.09,0.12)
    applyCamera()
end

function onLoop(delta)
    updateAutomatedFacialTargetGeneration()
    updateCameraKeyboard(delta)
    if state.targetAuthoring.previewDirty and
            (state.workspace=='targets' or state.targetAuthoring.projectRestorePending) then
        refreshTargetPreview()
        state.targetAuthoring.projectRestorePending=false
    end
    if state.workspace=='targets' and state.targetOverlay.dirty and not state.targetOverlay.building then
        startTargetOverlayBuild()
    end
    updateMaskBrushHover()
    updateTargetBrushHover()
    updateFacialClipPlayback(delta)
    updatePreviewAuthoredClipPlayback(delta)
    updateFacialComparisonVisibility()
    showMenu()
    showExportWindow()
    showMainPanel()
    showLandmarkReferenceModal()
    showCameraPanel()
    showFacialClipTimelineWindow()
    drawMaskBrushScreenCursor()
    drawTargetBrushScreenCursor()
    if state.landmarkMarkersDirty then refreshLandmarkMarkers() end
    if state.faceGuideDirty then refreshFaceGuide() end
    if state.maskVisualization.building then
        stepMaskVisualizationBuild()
    elseif state.maskVisualization.dirty then
        refreshMaskVisualization()
    end
    if state.targetOverlay.building then stepTargetOverlayBuild() end
    if state.rigPreview.dirty then refreshRigPreview() end
    tUtil.showOverlayMessage()
end

function onEndScene()
    destroyPreview()
end

local function viewportPickRay(x,y)
    local ok,ox,oy,oz,dx,dy,dz=dpCall(function() return mbm.getPickRay(x,y) end)
    if not ok or not ox or not dx then return nil end
    return {x=ox,y=oy,z=oz},{x=dx,y=dy,z=dz}
end

local function viewportPickRadius(fraction)
    if not state.meshBounds then return 0.001 end
    local bounds=state.meshBounds
    local extent=math.sqrt((bounds.maxX-bounds.minX)^2+(bounds.maxY-bounds.minY)^2+
        (bounds.maxZ-bounds.minZ)^2)
    return math.max(extent*(fraction or 0.02),0.001)
end

local function rayIntersectsBounds(origin,direction,bounds)
    if not origin or not direction or not bounds then return true end
    local near,far=0,math.huge
    for _,axis in ipairs({'x','y','z'}) do
        local value=origin[axis]
        local delta=direction[axis]
        local minimum=bounds['min'..axis:upper()]
        local maximum=bounds['max'..axis:upper()]
        if math.abs(delta)<1e-12 then
            if value<minimum or value>maximum then return false end
        else
            local a,b=(minimum-value)/delta,(maximum-value)/delta
            if a>b then a,b=b,a end
            near,far=math.max(near,a),math.min(far,b)
            if near>far then return false end
        end
    end
    return far>=0
end

local function rayPointDistance(ox,oy,oz,dx,dy,dz,point,maxDistance)
    local vx,vy,vz=point.x-ox,point.y-oy,point.z-oz
    local depth=vx*dx+vy*dy+vz*dz
    if depth<0 then return nil end
    local px,py,pz=ox+dx*depth,oy+dy*depth,oz+dz*depth
    local ex,ey,ez=point.x-px,point.y-py,point.z-pz
    local distance=math.sqrt(ex*ex+ey*ey+ez*ez)
    return distance<=maxDistance and distance or nil
end

local function nearestFaceGuidePoint(x,y)
    local guide=state.faceGuide
    if state.workspace~='face_position' and state.workspace~='landmarks' or
            not guide.worldPoints or not guide.origin then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,x,y)
    if not ok then return nil end
    local length=math.sqrt(dx*dx+dy*dy+dz*dz)
    if length<=1e-8 then return nil end
    dx,dy,dz=dx/length,dy/length,dz/length
    local scale=state.project.faceOrigin and state.project.faceOrigin.scale or 1
    local radius=math.max(scale*0.035,0.003)
    local best,bestDistance=nil,math.huge
    for name,point in pairs(guide.worldPoints) do
        local distance=rayPointDistance(ox,oy,oz,dx,dy,dz,point,radius)
        if distance and distance<bestDistance then
            best={name=name,point=point}; bestDistance=distance
        end
    end
    return best
end

local function raySegmentDistance(ox,oy,oz,dx,dy,dz,a,b,radius)
    local vx,vy,vz=b.x-a.x,b.y-a.y,b.z-a.z
    local lengthSquared=vx*vx+vy*vy+vz*vz
    if lengthSquared<1e-12 then return rayPointDistance(ox,oy,oz,dx,dy,dz,a,radius) end
    local wx,wy,wz=ox-a.x,oy-a.y,oz-a.z
    local uv=dx*vx+dy*vy+dz*vz
    local uw=dx*wx+dy*wy+dz*wz
    local vw=vx*wx+vy*wy+vz*wz
    local denominator=lengthSquared-uv*uv
    local segmentT=math.abs(denominator)<1e-12 and -vw/lengthSquared or
        (vw-uv*uw)/denominator
    segmentT=math.max(0,math.min(1,segmentT))
    local px,py,pz=a.x+vx*segmentT,a.y+vy*segmentT,a.z+vz*segmentT
    local rayT=(px-ox)*dx+(py-oy)*dy+(pz-oz)*dz
    if rayT<0 then return nil end
    local qx,qy,qz=ox+dx*rayT,oy+dy*rayT,oz+dz*rayT
    local ex,ey,ez=px-qx,py-qy,pz-qz
    return ex*ex+ey*ey+ez*ez<=radius*radius and rayT or nil
end

local function hitTestFaceGuideAxis(x,y)
    local guide=state.faceGuide
    if state.workspace~='face_position' and state.workspace~='landmarks' or
            not guide.origin or not guide.length then return nil end
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,x,y)
    if not ok then return nil end
    local radius=math.max(((state.project.faceOrigin and state.project.faceOrigin.scale) or 1)*0.08,0.003)
    local best,bestDistance=nil,math.huge
    for name,axis in pairs({x={x=1,y=0,z=0},y={x=0,y=1,z=0},z={x=0,y=0,z=1}}) do
        local endpoint={x=guide.origin.x+axis.x*guide.length,
            y=guide.origin.y+axis.y*guide.length,z=guide.origin.z+axis.z*guide.length}
        local distance=raySegmentDistance(ox,oy,oz,dx,dy,dz,guide.origin,endpoint,radius)
        if distance and distance<bestDistance then best,bestDistance=name,distance end
    end
    return best
end

local function rayAxisParameter(x,y,origin,axis)
    local ok,ox,oy,oz,dx,dy,dz=pcall(mbm.getPickRay,x,y)
    if not ok then return nil end
    local wx,wy,wz=ox-origin.x,oy-origin.y,oz-origin.z
    local parallel=dx*axis.x+dy*axis.y+dz*axis.z
    local rayProjection=dx*wx+dy*wy+dz*wz
    local axisProjection=axis.x*wx+axis.y*wy+axis.z*wz
    local denominator=1-parallel*parallel
    if math.abs(denominator)<0.05 then return nil end
    return (axisProjection-parallel*rayProjection)/denominator
end

local function applyFaceGuideAxisDelta(axisName,delta,baseOrigin,baseRelative)
    local axis=axisName=='x' and {x=1,y=0,z=0} or axisName=='y' and {x=0,y=1,z=0} or {x=0,y=0,z=1}
    local origin=state.project.faceOrigin
    if not origin then return end
    local selectedName=selectedFaceGuideName()
    if selectedName=='face_origin' then
        origin.position={x=baseOrigin.x+axis.x*delta,y=baseOrigin.y+axis.y*delta,z=baseOrigin.z+axis.z*delta}
        origin.vertexIndex=nil
        for _,landmark in ipairs(state.project.landmarks or {}) do
            if landmark.placed and landmark.relativePosition then
                landmark.position=addPoint(origin.position,landmark.relativePosition)
                landmark.vertexIndex=nil
            end
        end
    else
        local landmark
        for _,candidate in ipairs(state.project.landmarks or {}) do
            if candidate.name==selectedName then landmark=candidate break end
        end
        if not landmark then return end
        local relative={x=baseRelative.x+axis.x*delta,y=baseRelative.y+axis.y*delta,
            z=baseRelative.z+axis.z*delta}
        landmark.relativePosition=relative
        landmark.position=addPoint(origin.position,relative)
        landmark.placed=true
        landmark.vertexIndex=nil
        mirrorLandmark(landmark)
    end
    markModified()
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
end

local maskVisualizationGeneration=0
local maskOverlayShaderName='facial_mask_overlay.ps'

local function ensureMaskOverlayShader()
    if mbm.existShader(maskOverlayShaderName) then return true end
    local code
    if mbm.get('USE_METAL') then
        code=[=[
        fragment float4 frag_main(VOut in [[stage_in]])
        {
            float selected=clamp(in.uv.x,0.0f,1.0f);
            float3 color=mix(float3(0.12f,0.15f,0.20f),float3(0.05f,0.85f,1.00f),selected);
            return float4(color,mix(0.35f,0.92f,selected));
        }
        ]=]
    elseif mbm.get('USE_DIRECTX11') then
        code=[[
        struct PSInput
        {
            float4 position : SV_POSITION;
            float2 texCoord : TEXCOORD0;
        };

        float4 main(PSInput input) : SV_TARGET
        {
            float selected=saturate(input.texCoord.x);
            float3 color=lerp(float3(0.12,0.15,0.20),float3(0.05,0.85,1.00),selected);
            return float4(color,lerp(0.35,0.92,selected));
        }
        ]]
    elseif mbm.get('USE_DIRECTX9') then
        code=[[
        float4 main(float2 texCoord : TEXCOORD0) : COLOR0
        {
            float selected=saturate(texCoord.x);
            float3 color=lerp(float3(0.12,0.15,0.20),float3(0.05,0.85,1.00),selected);
            return float4(color,lerp(0.35,0.92,selected));
        }
        ]]
    else
        code=[[
        precision mediump float;
        varying vec2 vTexCoord;

        void main()
        {
            float selected=clamp(vTexCoord.x,0.0,1.0);
            vec3 color=mix(vec3(0.12,0.15,0.20),vec3(0.05,0.85,1.00),selected);
            gl_FragColor=vec4(color,mix(0.35,0.92,selected));
        }
        ]]
    end
    local ok,result=pcall(function()
        return mbm.addShader({name=maskOverlayShaderName,code=code,var={},min={},max={}})
    end)
    return ok and result~=false
end

-- Reuse the mask shader as a displacement heat map. The overlay replaces the
-- preview mesh while visible, so the matching surfaces cannot fight for depth.
setTargetOverlayVisible=function(visible)
    local overlay=state.targetOverlay
    local show=visible and overlay.visible and overlay.surface~=nil and not overlay.building and
        not overlay.dirty and state.workspace=='targets'
    if show and state.mesh then
        if not overlay.meshHidden then
            overlay.meshWasVisible=state.mesh.visible~=false
            overlay.meshHidden=true
        end
        state.mesh.visible=false
    elseif overlay.meshHidden and state.mesh then
        state.mesh.visible=overlay.meshWasVisible
        overlay.meshHidden=false
    end
    if overlay.surface then overlay.surface.visible=show end
end

destroyTargetOverlay=function()
    local overlay=state.targetOverlay
    setTargetOverlayVisible(false)
    if overlay.surface then dpCall(function() overlay.surface:destroy() end) end
    state.targetOverlay={visible=overlay.visible,dirty=true,building=false,surface=nil,meshHidden=false}
end

startTargetOverlayBuild=function()
    local overlay=state.targetOverlay
    if not overlay.dirty or not state.topology or not state.mesh or state.targetAuthoring.previewDirty then return end
    setTargetOverlayVisible(false)
    if overlay.surface then dpCall(function() overlay.surface:destroy() end); overlay.surface=nil end
    local target=tFacialTargets.find(state.targetAuthoring.targets,state.targetAuthoring.selectedTarget)
    if not overlay.visible or not target then overlay.dirty=false; return end
    local vertices=state.topology.vertices or {}
    local deltas=target.positionDeltas or {}
    local weight=state.targetAuthoring.previewWeight or 1
    local maximum=0
    for _,delta in ipairs(deltas) do
        maximum=math.max(maximum,math.sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z))
    end
    local coords,uvs={},{}
    for index,point in ipairs(vertices) do
        local delta=deltas[index] or {x=0,y=0,z=0}
        local magnitude=math.sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z)
        coords[#coords+1]=point.x+delta.x*weight
        coords[#coords+1]=point.y+delta.y*weight
        coords[#coords+1]=point.z+delta.z*weight
        uvs[#uvs+1]=maximum>1e-10 and math.min(1,magnitude/maximum) or 0
        uvs[#uvs+1]=1
    end
    overlay.coords=coords
    overlay.uvs=uvs
    overlay.indices={}
    overlay.cursor=1
    overlay.building=true
    overlay.dirty=false
end

stepTargetOverlayBuild=function()
    local overlay=state.targetOverlay
    if not overlay.building then return end
    local triangles=state.topology and state.topology.triangles or {}
    local finish=math.min(#triangles,overlay.cursor+499)
    for index=overlay.cursor,finish do
        local triangle=triangles[index]
        if triangle and state.topology.vertices[triangle[1]] and
                state.topology.vertices[triangle[2]] and state.topology.vertices[triangle[3]] then
            local indices=overlay.indices
            indices[#indices+1]=triangle[1]
            indices[#indices+1]=triangle[2]
            indices[#indices+1]=triangle[3]
        end
    end
    overlay.cursor=finish+1
    if overlay.cursor<=#triangles then return end
    overlay.building=false
    if #overlay.indices>0 then
        local surface=shape:new('3d',0,0,0.0001)
        local created=surface:createDynamicIndexed(overlay.coords,overlay.indices,overlay.uvs,
            'facial_target_overlay_'..tostring(mbm.getTimeRun()))
        if created then
            local okShader,shader=dpCall(function() return surface:getShader() end)
            local loaded=false
            if ensureMaskOverlayShader() and okShader and shader then
                local okLoad,result=dpCall(function() return shader:load(maskOverlayShaderName,nil) end)
                loaded=okLoad and result~=false
            end
            if not loaded then surface:setColor(0.1,0.7,1,0.8) end
            surface.visible=false
            overlay.surface=surface
        else
            dpCall(function() surface:destroy() end)
        end
    end
    overlay.coords=nil; overlay.uvs=nil; overlay.indices=nil; overlay.cursor=nil
    setTargetOverlayVisible(true)
end

-- Dynamic indexed shapes let a brush stroke recolor only the vertices it
-- touched.  The callback is armed for one render and then unregisters itself;
-- this keeps the shader responsive without copying the complete UV buffer on
-- every frame while the editor is idle.
local function updateMaskSurfaceOnRender(_,_,uv)
    local visualization=state.maskVisualization
    if not visualization.surfaceUvDirty then
        visualization.surfaceUvCallbackArmed=false
        return
    end
    for index,value in pairs(visualization.surfaceUvChanges or {}) do
        local entry=uv[tonumber(index)]
        if entry then entry.u=value end
    end
    uv.name='uv'
    visualization.surfaceUvDirty=false
    visualization.surfaceUvChanges={}
    return uv
end

local function queueMaskSurfaceUv(index,selected)
    local visualization=state.maskVisualization
    if not visualization.surface then return end
    visualization.surfaceUvChanges[index]=selected and 1 or 0
    visualization.surfaceUvDirty=true
    if not visualization.surfaceUvCallbackArmed then
        visualization.surfaceUvCallbackArmed=true
        local ok=pcall(function()
            visualization.surface:onRender(updateMaskSurfaceOnRender)
        end)
        if not ok then visualization.surfaceUvCallbackArmed=false end
    end
end

local function finishMaskVisualizationBuild()
    local visualization=state.maskVisualization
    if visualization.surface then dpCall(function() visualization.surface:destroy() end) end
    if visualization.faces then dpCall(function() visualization.faces:destroy() end) end
    if visualization.markers then dpCall(function() visualization.markers:destroy() end) end
    visualization.surface=nil
    visualization.faces=nil
    visualization.markers=nil
    maskVisualizationGeneration=maskVisualizationGeneration+1
    local surfaceCoords=visualization.buildSurfaceCoords or {}
    local surfaceUvs=visualization.buildSurfaceUvs or {}
    local surfaceIndices=visualization.buildSurfaceIndices or {}
    if #surfaceCoords>0 and #surfaceIndices>0 then
        -- The vertices are absolute mesh coordinates. Keep the object off exact z=0
        -- so DEVICE does not replace its position with the automatic 3D z-order.
        local surface=shape:new('3d',0,0,0.0001)
        local created=surface:createDynamicIndexed(surfaceCoords,surfaceIndices,surfaceUvs,
            'facial_mask_surface_'..tostring(maskVisualizationGeneration))
        if created then
            local okShader,shader=pcall(function() return surface:getShader() end)
            local loaded=false
            if ensureMaskOverlayShader() and okShader and shader then
                local okLoad,result=pcall(function() return shader:load(maskOverlayShaderName,nil) end)
                loaded=okLoad and result~=false
            end
            if not loaded then surface:setColor(0.16,0.72,0.92,0.75) end
            surface.visible=false
            visualization.surface=surface
            visualization.surfaceUvDirty=false
            visualization.surfaceUvCallbackArmed=false
            visualization.surfaceUvChanges={}
        else
            dpCall(function() surface:destroy() end)
        end
    end
    visualization.markerCount=0
    visualization.buildSelected=nil
    visualization.buildSurfaceCoords=nil
    visualization.buildSurfaceUvs=nil
    visualization.buildSurfaceIndices=nil
    visualization.building=false
    visualization.needsBuild=false
    visualization.dirty=false
    visualization.keepSurfaceWhileDirty=false
    setMaskVisualizationViewportVisible(true)
end

startMaskVisualizationBuild=function()
    local visualization=state.maskVisualization
    if visualization.building or not state.mesh or not state.topology then return false end
    local mask=state.project.masks and state.project.masks[state.selectedMask]
    if not mask or mask.enabled==false then return false end
    visualization.keepSurfaceWhileDirty=false
    if visualization.meshHidden then
        state.mesh.visible=visualization.meshWasVisible
        visualization.meshHidden=false
    end
    if visualization.surface then dpCall(function() visualization.surface:destroy() end) end
    if visualization.faces then dpCall(function() visualization.faces:destroy() end) end
    if visualization.markers then dpCall(function() visualization.markers:destroy() end) end
    visualization.surface=nil
    visualization.faces=nil
    visualization.markers=nil
    visualization.buildSelected={}
    local selected=visualization.buildSelected
    for _,rawIndex in ipairs(mask.vertices or {}) do
        local index=tonumber(rawIndex)
        if index and state.topology.vertices[index] then selected[#selected+1]=index end
    end
    table.sort(selected)
    local selectedSet={}
    for _,index in ipairs(selected) do selectedSet[index]=true end
    visualization.buildSelectedSet=selectedSet
    visualization.buildSurfaceCoords={}
    visualization.buildSurfaceUvs={}
    visualization.buildSurfaceIndices={}
    for index,point in ipairs(state.topology.vertices or {}) do
        local coords=visualization.buildSurfaceCoords
        local uvs=visualization.buildSurfaceUvs
        coords[#coords+1]=point.x; coords[#coords+1]=point.y; coords[#coords+1]=point.z
        uvs[#uvs+1]=selectedSet[index] and 1 or 0
        uvs[#uvs+1]=1
    end
    visualization.buildCursor=1
    visualization.buildTotal=#(state.topology.triangles or {})
    visualization.vertexCount=#selected
    visualization.faceCount=0
    visualization.markerCount=0
    visualization.building=true
    visualization.needsBuild=true
    visualization.dirty=false
    if visualization.hoverMarker then visualization.hoverMarker.visible=false end
    if visualization.hoverFace then visualization.hoverFace.visible=false end
    return true
end

stepMaskVisualizationBuild=function()
    local visualization=state.maskVisualization
    if not visualization.building then return end
    local triangles=state.topology and state.topology.triangles or {}
    local selected=visualization.buildSelectedSet or {}
    local cursor=visualization.buildCursor
    local finish=math.min(#triangles,cursor+249)
    local indices=visualization.buildSurfaceIndices
    for triangleIndex=cursor,finish do
        local triangle=triangles[triangleIndex]
        local ia,ib,ic=triangle[1],triangle[2],triangle[3]
        if state.topology.vertices[ia] and state.topology.vertices[ib] and state.topology.vertices[ic] then
            indices[#indices+1]=ia
            indices[#indices+1]=ib
            indices[#indices+1]=ic
            if selected[ia] and selected[ib] and selected[ic] then
                visualization.faceCount=visualization.faceCount+1
            end
        end
    end
    visualization.buildCursor=finish+1
    if visualization.buildCursor>visualization.buildTotal then finishMaskVisualizationBuild() end
end

refreshMaskVisualization=function()
    local visualization=state.maskVisualization
    if visualization.building then return end
    if not visualization.dirty then return end
    if visualization.keepSurfaceWhileDirty and visualization.surface and visualization.visible and
            state.workspace=='masks' then
        visualization.dirty=false
        return
    end
    setMaskVisualizationViewportVisible(true)
    visualization.dirty=false
end

local function maskBrushWorldRadius()
    local mask=state.project.masks and state.project.masks[state.selectedMask]
    local vertices=mask and mask.vertices or {}
    local landmarkKeyParts={}
    for _,landmark in ipairs(state.project.landmarks or {}) do
        local point=landmark.placed and landmark.position
        if point then
            landmarkKeyParts[#landmarkKeyParts+1]=tostring(landmark.name or '')..':'..
                string.format('%.5f,%.5f,%.5f',point.x,point.y,point.z)
        end
    end
    local key=tostring(state.selectedMask)..':'..tostring(#vertices)..':'..
        tostring(mask and mask.topologySignature or '')..':'..table.concat(landmarkKeyParts,';')
    local cache=state.maskBrushReference
    if cache.key~=key or not cache.extent then
        local minX,minY,minZ=math.huge,math.huge,math.huge
        local maxX,maxY,maxZ=-math.huge,-math.huge,-math.huge
        local valid=0
        local landmarkCount=0
        for _,landmark in ipairs(state.project.landmarks or {}) do
            local point=landmark.placed and landmark.position
            if point then
                landmarkCount=landmarkCount+1
                minX,minY,minZ=math.min(minX,point.x),math.min(minY,point.y),math.min(minZ,point.z)
                maxX,maxY,maxZ=math.max(maxX,point.x),math.max(maxY,point.y),math.max(maxZ,point.z)
            end
        end
        if landmarkCount>=3 then
            cache.extent=math.max(maxX-minX,maxY-minY,maxZ-minZ)
        end
        minX,minY,minZ=math.huge,math.huge,math.huge
        maxX,maxY,maxZ=-math.huge,-math.huge,-math.huge
        for _,rawIndex in ipairs(vertices) do
            local point=state.topology and state.topology.vertices[tonumber(rawIndex)]
            if point then
                valid=valid+1
                minX,minY,minZ=math.min(minX,point.x),math.min(minY,point.y),math.min(minZ,point.z)
                maxX,maxY,maxZ=math.max(maxX,point.x),math.max(maxY,point.y),math.max(maxZ,point.z)
            end
        end
        if (not cache.extent or cache.extent<=1e-8) and valid>0 then
            cache.extent=math.max(maxX-minX,maxY-minY,maxZ-minZ)
        end
        if not cache.extent or cache.extent<=1e-8 then
            local bounds=state.meshBounds
            cache.extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                bounds.maxZ-bounds.minZ) or 1
        end
        cache.key=key
    end
    return math.max(cache.extent*math.max(tonumber(state.maskBrushRadius) or 0.045,0.001),0.0001)
end

local function buildMaskBrushGeometry()
    local topology=state.topology
    if not topology or not topology.vertices or not topology.triangles then return nil end
    local signature=topology.signature or tostring(#topology.vertices)..':'..tostring(#topology.triangles)
    local cache=state.maskBrushGeometry
    if cache and cache.signature==signature and cache.bvh then return cache end
    local triangles={}
    for _,indices in ipairs(topology.triangles) do
        local a,b,c=topology.vertices[indices[1]],topology.vertices[indices[2]],topology.vertices[indices[3]]
        if a and b and c then
            triangles[#triangles+1]={a=a,b=b,c=c,
                minX=math.min(a.x,b.x,c.x),maxX=math.max(a.x,b.x,c.x),
                minY=math.min(a.y,b.y,c.y),maxY=math.max(a.y,b.y,c.y),
                minZ=math.min(a.z,b.z,c.z),maxZ=math.max(a.z,b.z,c.z),
                cx=(a.x+b.x+c.x)/3,cy=(a.y+b.y+c.y)/3,cz=(a.z+b.z+c.z)/3}
        end
    end
    local function buildBvh(entries)
        if #entries==0 then return nil end
        local node={minX=math.huge,minY=math.huge,minZ=math.huge,
            maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge}
        for _,triangle in ipairs(entries) do
            node.minX=math.min(node.minX,triangle.minX); node.maxX=math.max(node.maxX,triangle.maxX)
            node.minY=math.min(node.minY,triangle.minY); node.maxY=math.max(node.maxY,triangle.maxY)
            node.minZ=math.min(node.minZ,triangle.minZ); node.maxZ=math.max(node.maxZ,triangle.maxZ)
        end
        if #entries<=12 then node.triangles=entries; return node end
        local dx,dy,dz=node.maxX-node.minX,node.maxY-node.minY,node.maxZ-node.minZ
        local axis=dx>=dy and dx>=dz and 'cx' or (dy>=dz and 'cy' or 'cz')
        table.sort(entries,function(left,right) return left[axis]<right[axis] end)
        local middle=math.floor(#entries/2)
        local left,right={},{}
        for index,triangle in ipairs(entries) do
            if index<=middle then left[#left+1]=triangle else right[#right+1]=triangle end
        end
        node.left=buildBvh(left); node.right=buildBvh(right)
        return node
    end
    cache={signature=signature,triangles=triangles,bvh=buildBvh(triangles)}
    state.maskBrushGeometry=cache
    return cache
end

local function maskBrushRayTriangle(ox,oy,oz,dx,dy,dz,triangle)
    local a,b,c=triangle.a,triangle.b,triangle.c
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
        normal={x=nx,y=ny,z=nz}}
end

local function maskBrushSurfaceHit(x,y)
    local origin,direction=viewportPickRay(x,y)
    if not origin or not rayIntersectsBounds(origin,direction,state.meshBounds) then return nil end
    local cache=buildMaskBrushGeometry()
    if not cache or not cache.bvh then return nil end
    local function hitsBounds(node,maxDistance)
        local near,far=0,maxDistance or math.huge
        for _,axis in ipairs({{'x',origin.x,direction.x},{'y',origin.y,direction.y},
                {'z',origin.z,direction.z}}) do
            local name,value,delta=axis[1],axis[2],axis[3]
            local minimum=node['min'..name:upper()]
            local maximum=node['max'..name:upper()]
            if math.abs(delta)<1e-12 then
                if value<minimum or value>maximum then return false end
            else
                local a,b=(minimum-value)/delta,(maximum-value)/delta
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
                local hit=maskBrushRayTriangle(origin.x,origin.y,origin.z,
                    direction.x,direction.y,direction.z,triangle)
                if hit and (not nearest or hit.distance<nearest.distance) then nearest=hit end
            end
        else
            visit(node.left); visit(node.right)
        end
    end
    visit(cache.bvh)
    return nearest
end

updateTargetBrushHover=function()
    local hover=state.targetBrushHover
    local x,y=hover.pendingX,hover.pendingY
    if x==nil or y==nil then return end
    hover.pendingX=nil; hover.pendingY=nil
    if state.workspace~='targets' or state.targetAuthoring.mode~=3 or not state.topology then
        hover.lastHit=nil
        return
    end
    hover.lastHit=maskBrushSurfaceHit(x,y)
    hover.lastX=x; hover.lastY=y
end

local function maskBrushSurfaceHitForBrush(x,y,brush)
    local previous=brush and brush.lastHit
    local sampleCount=brush and brush.sampleCount or 0
    -- Reuse the last tangent plane for nearby samples.  An exact triangle
    -- intersection is still performed periodically and whenever the cursor
    -- leaves that local patch, so curvature and occlusion remain correct.
    if previous and sampleCount%8~=0 then
        local origin,direction=viewportPickRay(x,y)
        if origin and direction then
            local normal=previous.normal
            local denominator=direction.x*normal.x+direction.y*normal.y+direction.z*normal.z
            if math.abs(denominator)>1e-7 then
                local px,py,pz=previous.point.x-origin.x,previous.point.y-origin.y,
                    previous.point.z-origin.z
                local distance=(px*normal.x+py*normal.y+pz*normal.z)/denominator
                if distance>=0 then
                    local candidate={x=origin.x+direction.x*distance,
                        y=origin.y+direction.y*distance,z=origin.z+direction.z*distance}
                    local dx,dy,dz=candidate.x-previous.point.x,candidate.y-previous.point.y,
                        candidate.z-previous.point.z
                    local radius=maskBrushWorldRadius()
                    local bounds=state.meshBounds
                    local inside=not bounds or
                        (candidate.x>=bounds.minX and candidate.x<=bounds.maxX and
                         candidate.y>=bounds.minY and candidate.y<=bounds.maxY and
                         candidate.z>=bounds.minZ and candidate.z<=bounds.maxZ)
                    if inside and dx*dx+dy*dy+dz*dz<=radius*radius*36 then
                        return {point=candidate,normal=normal}
                    end
                end
            end
        end
    end
    return maskBrushSurfaceHit(x,y)
end

updateMaskBrushHover=function(x,y)
    local hover=state.maskBrushHover
    if hover and (x==nil or y==nil) then
        x,y=hover.pendingX,hover.pendingY
    end
    if not hover or x==nil or y==nil then return end
    if state.workspace~='masks' or state.maskVisualization.building or
            (state.maskVisualization.needsBuild and not state.maskVisualization.surface) then
        if hover and hover.cursor then hover.cursor.visible=false end
        return
    end
    if hover.cursor then
        dpCall(function() hover.cursor:destroy() end)
        hover.cursor=nil
    end
    local now=mbm.getTimeRun()
    if not hover.forceSurfaceHit and now-(hover.lastTime or 0)<1/30 and hover.lastX and
            math.abs(x-hover.lastX)<2 and math.abs(y-(hover.lastY or y))<2 then return end
    hover.pendingX=nil
    hover.pendingY=nil
    hover.lastTime=now
    hover.lastX,hover.lastY=x,y
    local previous=hover.lastHit
    local radius=maskBrushWorldRadius()
    -- Match Skeletal Animation Editor paint behavior: the cursor is always
    -- anchored to the exact triangle under the pointer. The BVH-backed query
    -- keeps this exact path cheap enough for normal mouse motion.
    local hit=maskBrushSurfaceHit(x,y)
    hover.forceSurfaceHit=false
    if not hit then
        hover.lastHit=nil
        return
    end
    if previous and math.abs(previous.point.x-hit.point.x)<radius*0.02 and
            math.abs(previous.point.y-hit.point.y)<radius*0.02 and
            math.abs(previous.point.z-hit.point.z)<radius*0.02 then
        return
    end
    hover.lastHit=hit
end

drawMaskBrushScreenCursor=function()
    local hover=state.maskBrushHover
    if not hover or state.workspace~='masks' or not hover.lastHit or
            not hover.lastX or not hover.lastY or tImGui.GetWantCaptureMouse() then return end
    local mouse=tImGui.GetMousePos()
    local mouseX=mouse and tonumber(mouse.x) or hover.lastX
    local mouseY=mouse and tonumber(mouse.y) or hover.lastY
    if not mouseX or not mouseY or mouseX<-100000 or mouseY<-100000 then return end
    local radius=math.max(6,math.min(96,(tonumber(state.maskBrushRadius) or 0.045)/0.045*20))
    local color=state.maskEditMode=='remove' and {r=1,g=0.25,b=0.12,a=0.95} or
        {r=0.20,g=1,b=0.35,a=0.95}
    tImGui.SetImDrawListToForeground(true)
    tImGui.AddCircle({x=mouseX,y=mouseY},radius,color,32,1.5)
    tImGui.SetImDrawListToForeground(false)
end

drawTargetBrushScreenCursor=function()
    if state.workspace~='targets' or state.targetAuthoring.mode~=3 or
            not state.mesh or not state.targetBrushHover.lastHit or
            tImGui.GetWantCaptureMouse() then return end
    local mouse=tImGui.GetMousePos()
    if not mouse or not tonumber(mouse.x) or not tonumber(mouse.y) then return end
    local authoring=state.targetAuthoring
    local radius=math.max(6,math.min(96,(authoring.brushRadius or 0.025)/0.025*20))
    local color=authoring.brushMode==2 and {r=0.25,g=0.75,b=1,a=0.9} or
        {r=1,g=0.75,b=0.2,a=0.9}
    tImGui.SetImDrawListToForeground(true)
    tImGui.AddCircle({x=mouse.x,y=mouse.y},radius,color,32,1.5)
    tImGui.SetImDrawListToForeground(false)
end

local function buildMaskBrushSpatialIndex(radius)
    local topology=state.topology
    if not topology or not topology.vertices then return nil end
    local cellSize=math.max(tonumber(radius) or 0,1e-5)
    local key=tostring(topology.signature or '')..':'..string.format('%.6f',cellSize)
    local cache=state.maskBrushSpatial
    if cache and cache.key==key and cache.cells then return cache end
    local cells={}
    local function cellCoordinate(value) return math.floor(value/cellSize) end
    for index,vertex in ipairs(topology.vertices) do
        local cx,cy,cz=cellCoordinate(vertex.x),cellCoordinate(vertex.y),cellCoordinate(vertex.z)
        local cellKey=cx..':'..cy..':'..cz
        local cell=cells[cellKey]
        if not cell then cell={}; cells[cellKey]=cell end
        cell[#cell+1]=index
    end
    cache={key=key,cellSize=cellSize,cells=cells}
    state.maskBrushSpatial=cache
    return cache
end

local function collectMaskBrushVertices(point,normal,radius)
    local result={}
    if not point or not normal then return result end
    local normalLength=math.sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z)
    if normalLength<=1e-8 then return result end
    local nx,ny,nz=normal.x/normalLength,normal.y/normalLength,normal.z/normalLength
    local radiusSquared=radius*radius
    local normalLimit=radius*0.45
    local spatial=buildMaskBrushSpatialIndex(radius)
    local candidateIndices={}
    local seen={}
    if spatial then
        local cellSize=spatial.cellSize
        local minX,maxX=math.floor((point.x-radius)/cellSize),math.floor((point.x+radius)/cellSize)
        local minY,maxY=math.floor((point.y-radius)/cellSize),math.floor((point.y+radius)/cellSize)
        local minZ,maxZ=math.floor((point.z-radius)/cellSize),math.floor((point.z+radius)/cellSize)
        for cx=minX,maxX do
            for cy=minY,maxY do
                for cz=minZ,maxZ do
                    local cell=spatial.cells[cx..':'..cy..':'..cz]
                    if cell then
                        for _,index in ipairs(cell) do
                            if not seen[index] then
                                seen[index]=true
                                candidateIndices[#candidateIndices+1]=index
                            end
                        end
                    end
                end
            end
        end
    else
        for index in ipairs(state.topology.vertices or {}) do candidateIndices[#candidateIndices+1]=index end
    end
    for _,index in ipairs(candidateIndices) do
        local vertex=state.topology.vertices[index]
        if vertex then
            local dx,dy,dz=vertex.x-point.x,vertex.y-point.y,vertex.z-point.z
            local distanceSquared=dx*dx+dy*dy+dz*dz
            if distanceSquared<=radiusSquared then
                local normalDistance=dx*nx+dy*ny+dz*nz
                local tangentSquared=distanceSquared-normalDistance*normalDistance
                if math.abs(normalDistance)<=normalLimit and tangentSquared<=radiusSquared then
                    result[#result+1]=index
                end
            end
        end
    end
    return result
end

local function stampMaskBrush(x,y,brush)
    brush.sampleCount=(brush.sampleCount or 0)+1
    local hit=maskBrushSurfaceHitForBrush(x,y,brush)
    if not hit then return false end
    local candidates=collectMaskBrushVertices(hit.point,hit.normal,maskBrushWorldRadius())
    if #candidates==0 then return false end
    if not brush.snapshotPushed then
        pushUndoSnapshot()
        brush.snapshotPushed=true
    end
    local changed=false
    if state.maskEditMode=='remove' then
        for _,index in ipairs(candidates) do
            if brush.present[index] then
                brush.present[index]=nil
                queueMaskSurfaceUv(index,false)
                changed=true
            end
        end
    else
        for _,index in ipairs(candidates) do
            if not brush.present[index] then
                brush.present[index]=true
                queueMaskSurfaceUv(index,true)
                changed=true
            end
        end
    end
    if changed then brush.changed=true end
    brush.lastHit=hit
    brush.lastX,brush.lastY=x,y
    return changed
end

local function beginMaskBrush(x,y)
    local mask=state.project.masks and state.project.masks[state.selectedMask]
    if not mask or state.maskVisualization.building or
            (state.maskVisualization.needsBuild and not state.maskVisualization.surface) then return false end
    local present={}
    for _,rawIndex in ipairs(mask.vertices or {}) do
        local index=tonumber(rawIndex)
        if index then present[index]=true end
    end
    local brush={startX=x,startY=y,lastStampX=x,lastStampY=y,present=present,
        snapshotPushed=false,changed=false,lastHit=nil,sampleCount=0}
    state.maskBrush=brush
    stampMaskBrush(x,y,brush)
    return true
end

local function updateMaskBrush(x,y)
    local brush=state.maskBrush
    if not brush then return end
    local dx,dy=x-brush.lastStampX,y-brush.lastStampY
    local distance=math.sqrt(dx*dx+dy*dy)
    if distance<4 then return end
    local steps=math.max(1,math.ceil(distance/10))
    for step=1,steps do
        local fraction=step/steps
        stampMaskBrush(brush.lastStampX+dx*fraction,brush.lastStampY+dy*fraction,brush)
    end
    brush.lastStampX,brush.lastStampY=x,y
end

local function finishMaskBrush()
    local brush=state.maskBrush
    state.maskBrush=nil
    if not brush then return end
    if not brush.changed then
        if brush.snapshotPushed and #state.undoStack>0 then table.remove(state.undoStack) end
        return
    end
    local mask=state.project.masks and state.project.masks[state.selectedMask]
    if not mask then return end
    mask.vertices={}
    for index in pairs(brush.present) do mask.vertices[#mask.vertices+1]=index end
    table.sort(mask.vertices)
    mask.topologySignature=state.topology.signature
    markMaskVisualizationDirty(true)
    markModified()
end

local function targetBrushRadius()
    local bounds=state.meshBounds
    if not bounds then return 0 end
    local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
    return math.max(extent*(state.targetAuthoring.brushRadius or 0.025),0.0001)
end

local function stampTargetBrush(x,y,stroke)
    local hit=maskBrushSurfaceHit(x,y)
    if not hit then return end
    state.targetBrushHover.lastHit=hit
    state.targetBrushHover.lastX=x
    state.targetBrushHover.lastY=y
    local radius=targetBrushRadius()
    local nearby=collectMaskBrushVertices(hit.point,hit.normal,radius)
    local authoring=state.targetAuthoring
    local changed=tFacialTargets.brushStamp(stroke.edited,state.topology.vertices,hit.point,hit.normal,
        radius,authoring.brushStrength or 0.5,{x=authoring.brushDx or 0,y=authoring.brushDy or 0,
            z=authoring.brushDz or 0},authoring.brushMode==2,stroke.allowed,nearby)
    stroke.changed=stroke.changed+changed
    stroke.lastX,stroke.lastY=x,y
end

local function beginTargetBrush(x,y)
    local authoring=state.targetAuthoring
    local selected=tFacialTargets.find(authoring.targets,authoring.selectedTarget)
    if not selected or not state.topology then return false end
    local stroke={index=authoring.selectedTarget,before=tFacialTargets.copyTargetData(selected),
        edited=tFacialTargets.copyTargetData(selected),changed=0,lastX=x,lastY=y,
        allowed=tFacialTargets.selectionSet(authoring,state.project.masks,authoring.vertexCount,state.topology.signature)}
    authoring.brushStroke=stroke
    stampTargetBrush(x,y,stroke)
    return true
end

local function finishTargetBrush()
    local authoring=state.targetAuthoring
    local stroke=authoring.brushStroke
    authoring.brushStroke=nil
    if not stroke or stroke.changed==0 then return end
    local ok,updated=dpCall(function()
        return state.meshDebug:updateFacialTarget(stroke.index,stroke.edited.name,stroke.edited.convention,
            stroke.edited.defaultWeight,stroke.edited.positionDeltas,
            #stroke.edited.normalDeltas>0 and stroke.edited.normalDeltas or nil)
    end)
    if ok and updated~=false then
        tFacialTargets.recordEdit(authoring,stroke.index,stroke.before,stroke.edited)
        markFacialTargetAuthoringModified()
        setStatus(string.format(tLang.L('fre_target_edit_applied_fmt'),stroke.changed),false)
    else
        targetAuthoringError('could not apply brush stroke')
    end
end

local function handleViewportAuthoringClick(x,y)
    if not state.topology or not state.mesh then return false end
    local origin,direction=viewportPickRay(x,y)
    if not origin then return false end
    if state.workspace=='face_position' then
        local faceOrigin=state.project.faceOrigin
        if not faceOrigin or not faceOrigin.placed or state.controlDown then
            local vertexHit=tFacialLandmarks.nearestSurfaceVertex(state.topology,origin,direction)
            if vertexHit then return setFaceOriginFromVertex(vertexHit) end
        end
    elseif state.workspace=='landmarks' then
        local guideHit=nearestFaceGuidePoint(x,y)
        if guideHit then
            if guideHit.name=='face_origin' then
                state.workspace='face_position'
            else
                for index,landmark in ipairs(state.project.landmarks or {}) do
                    if landmark.name==guideHit.name then
                        state.selectedLandmark=index
                        state.project.selectedLandmark=index
                        break
                    end
                end
            end
            markLandmarkMarkersDirty()
            markFaceGuideDirty()
            return true
        end
        local landmarkHit=tFacialLandmarks.nearestLandmark(state.project.landmarks,origin,direction,
            viewportPickRadius(0.025))
        if landmarkHit then
            state.selectedLandmark=landmarkHit.index
            state.project.selectedLandmark=landmarkHit.index
            markLandmarkMarkersDirty()
            return true
        end
        if state.controlDown then
            local vertexHit=tFacialLandmarks.nearestSurfaceVertex(state.topology,origin,direction)
            if vertexHit then
                local selected=state.project.landmarks[state.selectedLandmark]
                if selected then
                    setLandmarkPosition(selected,vertexHit.point,vertexHit.index)
                    state.project.selectedLandmark=state.selectedLandmark
                    markModified()
                    markLandmarkMarkersDirty()
                    markFaceGuideDirty()
                    return true
                end
            end
        end
    end
    return false
end

function onTouchDown(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
    if state.workspace=='targets' and key==1 then
        if state.targetAuthoring.mode==2 and state.topology then
            local origin,direction=viewportPickRay(x,y)
            local hit=origin and tFacialLandmarks.nearestSurfaceVertex(state.topology,origin,direction)
            if hit then
                state.targetAuthoring.selectedVertex=hit.index
                state.targetAuthoring.selectionMode=2
                setStatus(string.format(tLang.L('fre_target_picked_fmt'),hit.index),false)
            end
        elseif state.targetAuthoring.mode==3 then
            beginTargetBrush(x,y)
        end
        return
    end
    if key==0 and state.controlDown and (state.workspace=='face_position' or state.workspace=='landmarks') then
        local axis=hitTestFaceGuideAxis(x,y)
        if axis and state.faceGuide.origin then
            local axisVector=axis=='x' and {x=1,y=0,z=0} or axis=='y' and {x=0,y=1,z=0} or {x=0,y=0,z=1}
            local parameter=rayAxisParameter(x,y,state.faceGuide.origin,axisVector)
            if parameter then
                pushUndoSnapshot()
                local selectedName=selectedFaceGuideName()
                local baseRelative={x=0,y=0,z=0}
                if selectedName and selectedName~='face_origin' then
                    for _,candidate in ipairs(state.project.landmarks or {}) do
                        if candidate.name==selectedName then
                            if candidate.relativePosition then baseRelative={x=candidate.relativePosition.x,
                                y=candidate.relativePosition.y,z=candidate.relativePosition.z}
                            else
                                baseRelative=faceGuideRelative(selectedName,
                                    (state.project.faceOrigin and state.project.faceOrigin.scale) or 1)
                            end
                            break
                        end
                    end
                end
                local baseOrigin={x=state.project.faceOrigin.position.x,y=state.project.faceOrigin.position.y,
                    z=state.project.faceOrigin.position.z}
                local basePoint=selectedName=='face_origin' and baseOrigin or addPoint(baseOrigin,baseRelative)
                state.faceGizmoDrag={axis=axis,axisVector=axisVector,startParameter=parameter,
                    baseOrigin=baseOrigin,baseRelative=baseRelative,basePoint=basePoint}
                return
            end
        end
    end
    if key==0 and state.workspace=='masks' then
        state.maskBrushHover.lastHit=nil
        state.maskBrushHover.pendingX=nil
        state.maskBrushHover.pendingY=nil
        state.maskBrushHover.forceSurfaceHit=true
        state.cameraDrag={x=x,y=y,startX=x,startY=y,moved=false}
        setMaskVisualizationViewportVisible(false)
        return
    end
    if key==1 and state.workspace=='masks' then
        beginMaskBrush(x,y)
        return
    end
    if key==0 and state.controlDown and handleViewportAuthoringClick(x,y) then return end
    if key==0 then
        state.cameraDrag={x=x,y=y}
        if state.workspace=='masks' then setMaskVisualizationViewportVisible(false) end
        if state.workspace=='targets' then setTargetOverlayVisible(false) end
    elseif key==2 then
        state.cameraPan={x=x,y=y}
        if state.workspace=='masks' then setMaskVisualizationViewportVisible(false) end
        if state.workspace=='targets' then setTargetOverlayVisible(false) end
    end
end

function onTouchMove(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
    local targetStroke=state.targetAuthoring and state.targetAuthoring.brushStroke
    if targetStroke then
        local dx,dy=x-targetStroke.lastX,y-targetStroke.lastY
        if dx*dx+dy*dy>=16 then stampTargetBrush(x,y,targetStroke) end
        return
    end
    if state.faceGizmoDrag then
        local drag=state.faceGizmoDrag
        local parameter=rayAxisParameter(x,y,drag.basePoint,drag.axisVector)
        if parameter then
            applyFaceGuideAxisDelta(drag.axis,parameter-drag.startParameter,drag.baseOrigin,drag.baseRelative)
        end
        return
    end
    if state.maskBrush then
        updateMaskBrush(x,y)
        state.maskBrushHover.pendingX=x
        state.maskBrushHover.pendingY=y
        return
    end
    if state.workspace=='targets' and state.targetAuthoring.mode==3 and
            not state.cameraDrag and not state.cameraPan then
        state.targetBrushHover.pendingX=x
        state.targetBrushHover.pendingY=y
    end
    if state.workspace=='masks' and not state.cameraDrag and not state.cameraPan then
        state.maskBrushHover.pendingX=x
        state.maskBrushHover.pendingY=y
    end
    if state.cameraPan then
        if state.workspace=='masks' then setMaskVisualizationViewportVisible(false) end
        if state.workspace=='targets' then setTargetOverlayVisible(false) end
        local c=state.camera
        local px=c.fx+c.distance*math.cos(c.elevation)*math.sin(c.azimuth)
        local pz=c.fz+c.distance*math.cos(c.elevation)*math.cos(c.azimuth)
        local vx,vz=c.fx-px,c.fz-pz
        local viewLength=math.sqrt(vx*vx+vz*vz)
        if viewLength>0 then vx,vz=vx/viewLength,vz/viewLength end
        -- Match MeshDebug: horizontal movement follows camera-right and vertical movement
        -- follows the screen/world Y axis while keeping the orbit orientation unchanged.
        local rx,rz=vz,-vx
        local rightLength=math.sqrt(rx*rx+rz*rz)
        if rightLength>0 then rx,rz=rx/rightLength,rz/rightLength end
        local scale=c.distance*0.001
        local dx=(state.cameraPan.x-x)*scale
        c.fx=c.fx+rx*dx
        c.fy=c.fy+(y-state.cameraPan.y)*scale
        c.fz=c.fz+rz*dx
        state.cameraPan={x=x,y=y}
        applyCamera()
        return
    end
    if not state.cameraDrag then return end
    if state.workspace=='masks' then setMaskVisualizationViewportVisible(false) end
    if state.workspace=='targets' then setTargetOverlayVisible(false) end
    if state.workspace=='masks' and
            (math.abs(x-(state.cameraDrag.startX or state.cameraDrag.x))>2 or
                math.abs(y-(state.cameraDrag.startY or state.cameraDrag.y))>2) then
        state.cameraDrag.moved=true
    end
    state.camera.azimuth=state.camera.azimuth+(x-state.cameraDrag.x)*0.008
    state.camera.elevation=math.max(-1.45,math.min(1.45,
        state.camera.elevation+(y-state.cameraDrag.y)*0.008))
    state.cameraDrag.x=x
    state.cameraDrag.y=y
    applyCamera()
end

function onTouchUp(key,x,y)
    if key==1 and state.targetAuthoring and state.targetAuthoring.brushStroke then
        if not tImGui.GetWantCaptureMouse() then
            stampTargetBrush(x,y,state.targetAuthoring.brushStroke)
        end
        finishTargetBrush()
        return
    end
    if key==0 then
        state.cameraDrag=nil; state.faceGizmoDrag=nil
        if state.workspace=='targets' then
            setTargetOverlayVisible(true)
            state.targetBrushHover.pendingX=x
            state.targetBrushHover.pendingY=y
        end
        if state.workspace=='masks' then
            setMaskVisualizationViewportVisible(true)
            state.maskBrushHover.lastHit=nil
            state.maskBrushHover.forceSurfaceHit=true
            state.maskBrushHover.pendingX=x
            state.maskBrushHover.pendingY=y
        end
    elseif key==1 then
        if state.workspace=='masks' and state.maskBrush then
            updateMaskBrush(x,y)
            finishMaskBrush()
            setMaskVisualizationViewportVisible(true)
            state.maskBrushHover.pendingX=x
            state.maskBrushHover.pendingY=y
            return
        end
        if state.workspace=='masks' then setMaskVisualizationViewportVisible(true) end
    elseif key==2 then
        state.cameraPan=nil
        if state.workspace=='targets' then
            setTargetOverlayVisible(true)
            state.targetBrushHover.pendingX=x
            state.targetBrushHover.pendingY=y
        end
        if state.workspace=='masks' then
            setMaskVisualizationViewportVisible(true)
            state.maskBrushHover.forceSurfaceHit=true
            state.maskBrushHover.pendingX=x
            state.maskBrushHover.pendingY=y
        end
    end
end

function onTouchZoom(zoom)
    if tImGui.GetWantCaptureMouse() then return end
    state.camera.distance=math.max(0.01,state.camera.distance*(1-zoom*0.15))
    applyCamera()
end

function onKeyDown(key)
    if key==mbm.getKeyCode('control') then state.controlDown=true
    elseif key==mbm.getKeyCode('shift') then state.shiftDown=true
    elseif state.controlDown and key==mbm.getKeyCode('Z') and not tImGui.IsAnyItemActive() then
        if state.shiftDown then
            if redoProject() then setStatus(tLang.L('fre_redo_done'),false) end
        elseif undoProject() then
            setStatus(tLang.L('fre_undo_done'),false)
        end
    elseif state.controlDown and key==mbm.getKeyCode('Y') and not tImGui.IsAnyItemActive() then
        if redoProject() then setStatus(tLang.L('fre_redo_done'),false) end
    elseif state.controlDown and key==mbm.getKeyCode('N') then newProject()
    elseif state.controlDown and key==mbm.getKeyCode('O') then openProject()
    elseif state.controlDown and key==mbm.getKeyCode('S') then
        if state.projectPath then saveProject() else saveProjectAs() end
    elseif state.controlDown and key==mbm.getKeyCode('Q') then mbm.quit()
    elseif not state.controlDown and key==mbm.getKeyCode('W') then state.cameraMove.forward=1
    elseif not state.controlDown and key==mbm.getKeyCode('S') then state.cameraMove.forward=-1
    elseif not state.controlDown and key==mbm.getKeyCode('A') then state.cameraMove.right=-1
    elseif not state.controlDown and key==mbm.getKeyCode('D') then state.cameraMove.right=1
    elseif key==mbm.getKeyCode('pageup') then state.cameraMove.vertical=1
    elseif key==mbm.getKeyCode('pagedown') then state.cameraMove.vertical=-1
    end
end

function onKeyUp(key)
    if key==mbm.getKeyCode('control') then state.controlDown=false
    elseif key==mbm.getKeyCode('shift') then state.shiftDown=false
    elseif key==mbm.getKeyCode('W') or key==mbm.getKeyCode('S') then state.cameraMove.forward=0
    elseif key==mbm.getKeyCode('A') or key==mbm.getKeyCode('D') then state.cameraMove.right=0
    elseif key==mbm.getKeyCode('pageup') or key==mbm.getKeyCode('pagedown') then state.cameraMove.vertical=0
    end
end
