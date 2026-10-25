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
tFacialClips = require "facial_rig_clips"
tFacialPlayback = require "facial_mesh_playback"
tBlender = require "blender_cli_wrapper"

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
    exportWindow={open=false,target=1},
    facialComparison={mesh=nil,enabled=false,ready=false,layoutActive=false},
    expressions={catalogDirty=true,catalog={},selectedTarget=1,selectedPose=1,nameDraft='',renameDraft='',editMode=1,lastEditMode=1},
    clipEditor={selectedClip=1,keySelected=1,time=0,playing=false,nameDraft='',renameDraft='',dirty=true,sourceInitialized=false,timelineVisible=true},
    preview={selectedPose=1,facialPlayback=nil,skeletalDirty=true,skeletalAnimations={},selectedSkeletal=1,
        authoredClipSelected=1,authoredClipTime=0,authoredClipPlaying=false,authoredClipInitialized=false},
    blenderBridgePath=nil, blenderSourcePath=nil,
    undoStack={}, redoStack={}, historyLimit=50
}

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

local function destroyPreview()
    destroyRigPreview()
    if state.facialComparison and state.facialComparison.mesh then
        -- This comparison instance is temporary. Clear the editor-only
        -- no-frustum-culling override before destroying it.
        state.facialComparison.mesh.alwaysRender=false
        dpCall(function() state.facialComparison.mesh:destroy() end)
    end
    state.facialComparison={mesh=nil,enabled=false,ready=false,layoutActive=false}
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

local function saveAssignedMesh()
    if not state.meshDebug or not state.weightAssignment.modified or
            not state.weightAssignment.facialWeightsApplied then return false end
    local suggested=replaceExtension(state.project.meshPath or 'facial-rig','-facial-weighted.msh')
    local path=mbm.saveFile(suggested,'msh')
    if not path then return false end
    local callOk,saved=dpCall(function() return state.meshDebug:save(path,false,false,false) end)
    if not callOk or saved~=true then
        setStatus(tLang.L('fre_weight_assignment_save_failed'),true)
        return false
    end
    -- Keep the authoring project attached to the derived MSH in memory. The facial
    -- metadata lives in the project Lua, not in the binary MSH; saving the project
    -- afterward persists this new asset path together with its landmarks and masks.
    state.project.meshPath=path
    state.modified=true
    state.weightAssignment.modified=false
    state.weightAssignment.facialWeightsApplied=false
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

local function syncFacialComparisonWeights()
    local comparison=state.facialComparison and state.facialComparison.mesh
    if not comparison or not state.mesh then return end
    for index,entry in ipairs(state.expressions.catalog or {}) do
        local ok,value=dpCall(function() return state.mesh:getFacialWeight(entry.name) end)
        -- The Lua mesh binding accepts a target name or a 1-based target index
        -- through setFacialWeight(), but it does not expose the C++ convenience
        -- method setFacialWeightByName(). Keep the comparison path on the
        -- public Lua API so project-clip playback cannot abort the editor loop.
        if ok then dpCall(function() return comparison:setFacialWeight(index,tonumber(value) or 0) end) end
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

local function rebuildFacialComparison(enabled)
    local comparison=state.facialComparison
    if comparison.mesh then
        comparison.mesh.alwaysRender=false
        dpCall(function() comparison.mesh:destroy() end)
    end
    comparison.mesh=nil
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
    local primaryPathOk,primaryPath=dpCall(function() return state.mesh:getResolvedSkeletalExecutionPath() end)
    if not primaryPathOk or primaryPath~='gpu' then
        return false,'fre_preview_gpu_cpu_reason_gpu_unavailable'
    end
    local candidate=mesh:new('3d')
    local cpuOk,cpuReady=dpCall(function() return candidate:setSkeletalExecutionPath('cpu') end)
    local loadOk,loaded=false,false
    if cpuOk and cpuReady~=false then loadOk,loaded=dpCall(function() return candidate:load(state.project.meshPath) end) end
    if not cpuOk or cpuReady==false or not loadOk or loaded==false then
        candidate.alwaysRender=false
        dpCall(function() candidate:destroy() end)
        return false,'fre_preview_gpu_cpu_reason_cpu_load'
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
    local okLoaded=dpCall(function() return candidate:load(path) end)
    if not okLoaded then
        candidate:destroy()
        setStatus(string.format(tLang.L('fre_load_failed_fmt'),shortName(path)),true)
        return false
    end
    -- A mesh may persist a skeletal autoplay flag. The facial editor starts from a
    -- deterministic bind/T-pose; playback is opt-in from Preview.
    dpCall(function() return candidate:stopSkeletalAnimation() end)
    local inspector=meshDebug:new()
    local okInspected=dpCall(function() return inspector:load(path) end)
    if not okInspected then
        candidate:destroy()
        setStatus(tLang.L('fre_mesh_inspection_failed'),true)
        return false
    end
    local bounds,count,topology=inspectMesh(inspector)
    destroyPreview()
    if not topology then
        candidate:destroy()
        setStatus(tLang.L('fre_mesh_inspection_failed'),true)
        return false
    end
    local requestedPreserve=preserveProject==true
    local projectCompatible=not requestedPreserve or state.project.meshTopologySignature=='' or
        (state.project.meshTopologySignature==topology.signature and
            (state.project.meshVertexCount==0 or state.project.meshVertexCount==topology.vertexCount))
    if requestedPreserve and not projectCompatible then preserveProject=false end
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
    if project.language then tLang.setLanguage(project.language) end
    state.project=project
    state.projectPath=path
    state.blenderBridgePath=nil
    state.blenderSourcePath=nil
    state.selectedRole=project.selectedRole or 1
    state.selectedLandmark=project.selectedLandmark or 1
    state.landmarkEditMode=project.landmarkEditMode or 1
    state.modified=false
    if project.meshPath~='' then
        if not loadMesh(project.meshPath,true) then state.project=project end
    else
        destroyPreview()
        state.expressions={catalogDirty=true,catalog={},selectedTarget=project.expressions.selectedTarget or 1,
            selectedPose=project.expressions.selectedPose or 1,nameDraft='',renameDraft='',editMode=1,lastEditMode=1}
        state.preview={selectedPose=project.expressions.selectedPose or 1,facialPlayback=nil,
            skeletalDirty=true,skeletalAnimations={},selectedSkeletal=1,authoredClipSelected=1,
            authoredClipTime=0,authoredClipPlaying=false,authoredClipInitialized=false}
        state.clipEditor={selectedClip=project.selectedClip or 1,keySelected=1,time=0,playing=false,nameDraft='',renameDraft='',dirty=true,sourceInitialized=false,timelineVisible=true}
    end
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
    state.project.selectedClip=state.clipEditor.selectedClip or 1
    state.project.language=tLang.getLanguage()
    local saved,errorMessage=tFacialProject.save(state.project,path,tUtil.save)
    if not saved then
        setStatus(string.format(tLang.L('fre_project_save_failed_fmt'),tostring(errorMessage)),true)
        return false
    end
    state.projectPath=path
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
        setStatus(tLang.L('fre_blender_addon_missing'),true)
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
        setStatus(tLang.L('fre_blender_command_failed'),true)
        return false
    end
    tBlender.launchCmdAsync(command)
    setStatus(string.format(tLang.L('fre_blender_opened_fmt'),shortName(sourcePath)),false)
    return true
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
            local canExport=state.weightAssignment.modified==true and
                state.weightAssignment.facialWeightsApplied==true and state.meshDebug~=nil
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
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
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

local function showMasks()
    tImGui.TextWrapped(tLang.L('fre_masks_help'))
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    tImGui.TextWrapped(tLang.L('fre_masks_object_mode_help'))
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
            if tImGui.Selectable(mask.name,index==state.selectedMask) then state.selectedMask=index end
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
        tImGui.PushItemWidth(320)
        local changed,name=tImGui.InputText(tLang.L('fre_mask_name')..'##freMaskName',selectedMask.name or '')
        if changed and name~='' then
            pushUndoSnapshot()
            selectedMask.name=name
            markModified()
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
        tImGui.TextWrapped(tLang.L('fre_mask_edit_help'))
        local addPressed=tImGui.Button(tLang.L('fre_mask_add_vertices'))
        showHelpTooltip(tLang.L('fre_mask_add_vertices_tooltip'))
        tImGui.SameLine()
        local removePressed=tImGui.Button(tLang.L('fre_mask_remove_vertices'))
        showHelpTooltip(tLang.L('fre_mask_remove_vertices_tooltip'))
        if addPressed then state.maskEditMode='add' end
        if removePressed then state.maskEditMode='remove' end
        tImGui.Text(tLang.L('fre_mask_mode') .. ': ' .. state.maskEditMode)
        tImGui.TextWrapped(tLang.L('fre_mask_edit_action_help'))
        if tImGui.Button(tLang.L('fre_mask_clear')) then
            pushUndoSnapshot()
            selectedMask.vertices={}
            markModified()
        end
        showHelpTooltip(tLang.L('fre_mask_clear_tooltip'))
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
    if #catalog==0 then
        tImGui.TextDisabled(tLang.L('fre_expressions_no_targets'))
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
    local compare=tImGui.Checkbox(tLang.L('fre_preview_gpu_cpu_compare'),
        state.facialComparison.enabled==true)
    if compare~=state.facialComparison.enabled then
        local rebuilt,reason=rebuildFacialComparison(compare)
        if rebuilt then
            state.facialComparison.enabled=compare
            setStatus(compare and tLang.L('fre_preview_gpu_cpu_enabled') or
                tLang.L('fre_preview_gpu_cpu_disabled'),false)
        else
            state.facialComparison.enabled=false
            local reasonText=reason and tLang.L(reason) or tLang.L('fre_preview_gpu_cpu_reason_unknown')
            setStatus(tLang.L('fre_preview_gpu_cpu_failed') .. ': ' .. reasonText,true)
        end
    end
    showHelpTooltip(tLang.L('fre_preview_gpu_cpu_tooltip'))
    if state.facialComparison.enabled and state.facialComparison.ready then
        tImGui.TextWrapped(tLang.L('fre_preview_gpu_cpu_layout'))
        local gpuOk,gpuReport=dpCall(function() return state.mesh:getFacialAnimationReport() end)
        local cpuOk,cpuReport=dpCall(function()
            return state.facialComparison.mesh:getFacialAnimationReport()
        end)
        if gpuOk and type(gpuReport)=='table' then
            tImGui.Text(tLang.L('fre_preview_gpu_report_fmt'))
            tImGui.Text(string.format(tLang.L('fre_preview_skin_fmt'),gpuReport.executionPath or 'unknown'))
            tImGui.Text(string.format(tLang.L('fre_preview_morph_fmt'),gpuReport.morphExecutionPath or 'unknown'))
        end
        if cpuOk and type(cpuReport)=='table' then
            tImGui.Text(tLang.L('fre_preview_cpu_report_fmt'))
            tImGui.Text(string.format(tLang.L('fre_preview_skin_fmt'),cpuReport.executionPath or 'unknown'))
            tImGui.Text(string.format(tLang.L('fre_preview_morph_fmt'),cpuReport.morphExecutionPath or 'unknown'))
        end
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
            if state.facialComparison.mesh then
                dpCall(function() return state.facialComparison.mesh:playSkeletalAnimation(skeletalName) end)
            end
            if ok and played~=false then setStatus(string.format(tLang.L('fre_preview_skeletal_played_fmt'),skeletalName),false)
            else setStatus(tLang.L('fre_preview_skeletal_failed'),true) end
        end
        showHelpTooltip(tLang.L('fre_preview_skeletal_play_tooltip'))
        if tImGui.Button(tLang.L('fre_preview_skeletal_pause') .. '##frePreviewSkeletalPause',{x=90,y=0}) then
            dpCall(function() return state.mesh:pauseSkeletalAnimation() end)
            if state.facialComparison.mesh then
                dpCall(function() return state.facialComparison.mesh:pauseSkeletalAnimation() end)
            end
        end
        showHelpTooltip(tLang.L('fre_preview_skeletal_pause_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_preview_skeletal_resume') .. '##frePreviewSkeletalResume',{x=100,y=0}) then
            dpCall(function() return state.mesh:resumeSkeletalAnimation() end)
            if state.facialComparison.mesh then
                dpCall(function() return state.facialComparison.mesh:resumeSkeletalAnimation() end)
            end
        end
        showHelpTooltip(tLang.L('fre_preview_skeletal_resume_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('fre_preview_skeletal_stop') .. '##frePreviewSkeletalStop',{x=90,y=0}) then
            dpCall(function() return state.mesh:stopSkeletalAnimation() end)
            if state.facialComparison.mesh then
                dpCall(function() return state.facialComparison.mesh:stopSkeletalAnimation() end)
            end
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
    tFacialPlayback.draw(state.preview,nil,state.mesh,true,dpCall)
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
            tImGui.TextDisabled(tLang.L('fre_rig_action_skeleton_ready'))
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
        local canSave=state.weightAssignment.modified==true and
            state.weightAssignment.facialWeightsApplied==true
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
        {'rig','fre_rig'}, {'expressions','fre_expressions'}, {'clips','fre_clips'}, {'preview','fre_preview'}
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
    updateCameraKeyboard(delta)
    updateFacialClipPlayback(delta)
    updatePreviewAuthoredClipPlayback(delta)
    updateFacialComparisonVisibility()
    showMenu()
    showExportWindow()
    showMainPanel()
    showLandmarkReferenceModal()
    showCameraPanel()
    showFacialClipTimelineWindow()
    if state.landmarkMarkersDirty then refreshLandmarkMarkers() end
    if state.faceGuideDirty then refreshFaceGuide() end
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

local function toggleMaskVertex(index)
    local mask=state.project.masks[state.selectedMask]
    if not mask or not index then return false end
    mask.topologySignature=state.topology.signature
    for position,vertex in ipairs(mask.vertices or {}) do
        if vertex==index then
            if state.maskEditMode=='remove' then
                pushUndoSnapshot()
                table.remove(mask.vertices,position)
                markModified()
            end
            return true
        end
    end
    if state.maskEditMode=='add' then
        pushUndoSnapshot()
        mask.vertices[#mask.vertices+1]=index
        markModified()
    end
    return true
end

local function handleViewportAuthoringClick(x,y)
    if not state.topology or not state.mesh then return false end
    local origin,direction=viewportPickRay(x,y)
    if not origin then return false end
    if state.workspace=='face_position' then
        local faceOrigin=state.project.faceOrigin
        if not faceOrigin or not faceOrigin.placed or state.controlDown then
            local vertexHit=tFacialLandmarks.nearestVertex(state.topology,origin,direction,
                viewportPickRadius(0.025))
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
            local vertexHit=tFacialLandmarks.nearestVertex(state.topology,origin,direction,
                viewportPickRadius(0.025))
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
    elseif state.workspace=='masks' then
        local vertexHit=tFacialLandmarks.nearestVertex(state.topology,origin,direction,
            viewportPickRadius(0.025))
        if vertexHit and state.project.masks[state.selectedMask] then
            return toggleMaskVertex(vertexHit.index)
        end
    end
    return false
end

function onTouchDown(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
    if key==0 and (state.workspace=='face_position' or state.workspace=='landmarks') then
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
    if key==0 and handleViewportAuthoringClick(x,y) then return end
    if key==0 then
        state.cameraDrag={x=x,y=y}
    elseif key==1 then
        state.cameraPan={x=x,y=y}
    end
end

function onTouchMove(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
    if state.faceGizmoDrag then
        local drag=state.faceGizmoDrag
        local parameter=rayAxisParameter(x,y,drag.basePoint,drag.axisVector)
        if parameter then
            applyFaceGuideAxisDelta(drag.axis,parameter-drag.startParameter,drag.baseOrigin,drag.baseRelative)
        end
        return
    end
    if state.cameraPan then
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
    state.camera.azimuth=state.camera.azimuth+(x-state.cameraDrag.x)*0.008
    state.camera.elevation=math.max(-1.45,math.min(1.45,
        state.camera.elevation+(y-state.cameraDrag.y)*0.008))
    state.cameraDrag={x=x,y=y}
    applyCamera()
end

function onTouchUp(key,x,y)
    if key==0 then state.cameraDrag=nil; state.faceGizmoDrag=nil
    elseif key==1 then state.cameraPan=nil end
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
