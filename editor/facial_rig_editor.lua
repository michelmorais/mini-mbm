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
    selectedRole=1, selectedLandmark=1, landmarkEditMode=1, status='', statusError=false,
    selectedMask=1, maskEditMode='add',
    camera={fx=0,fy=0,fz=0,distance=10,azimuth=0,elevation=0.15},
    cameraDrag=nil, cameraPan=nil, controlDown=false, shiftDown=false,
    faceGuideDirty=true, faceGuide={wireframe=nil,markers={},axes={},worldPoints={},origin=nil,length=nil},
    faceGizmoDrag=nil, windowMove=false, leftPanelRight=560,
    undoStack={}, redoStack={}, historyLimit=50
}

local windows = {
    main='facial_rig_main', camera='facial_rig_camera', report='facial_rig_report'
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
    face_left={0.18,0.50}, face_right={0.82,0.50}, face_top={0.50,0.10}, face_bottom={0.50,0.90},
    left_eye_center={0.39,0.36}, right_eye_center={0.61,0.36},
    left_eye_corner={0.31,0.36}, right_eye_corner={0.69,0.36},
    nose_bridge={0.50,0.40}, nose_tip={0.50,0.54},
    mouth_left={0.38,0.68}, mouth_right={0.62,0.68},
    upper_lip={0.50,0.64}, lower_lip={0.50,0.72}, chin={0.50,0.82},
    jaw_hinge_left={0.25,0.66}, jaw_hinge_right={0.75,0.66},
    brow_left={0.37,0.27}, brow_right={0.63,0.27}
}

local function shortName(path)
    return tUtil.getShortName(path or '', false) or path or ''
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

local function showLandmarkReference(selected)
    if not selected then return end
    tImGui.Text(tLang.L('fre_landmark_reference_title'))
    tImGui.TextWrapped(localizedLandmarkDescription(selected.name))
    tImGui.TextWrapped(tLang.L('fre_landmark_reference_help'))

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

local function setStatus(message, isError)
    state.status=tostring(message or '')
    state.statusError=isError == true
    tUtil.showMessage(state.status, state.statusError and 5 or 3)
end

local function markModified()
    state.modified=true
end

local function clearHistory()
    state.undoStack={}
    state.redoStack={}
end

local function historySnapshot()
    return {project=tUtil.deepCopyTable(state.project),modified=state.modified,
        selectedRole=state.selectedRole,selectedLandmark=state.selectedLandmark,
        selectedMask=state.selectedMask,landmarkEditMode=state.landmarkEditMode}
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
    state.project.selectedRole=state.selectedRole
    state.project.selectedLandmark=state.selectedLandmark
    state.project.selectedMask=state.selectedMask
    state.project.landmarkEditMode=state.landmarkEditMode
    state.landmarkMarkersDirty=true
    state.faceGuideDirty=true
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

local function destroyPreview()
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
    local points={
        face_left={-1,0,0}, face_right={1,0,0}, face_top={0,1,0}, face_bottom={0,-1,0},
        left_eye_center={-0.38,0.35,0.03}, right_eye_center={0.38,0.35,0.03},
        left_eye_corner={-0.58,0.35,0.02}, right_eye_corner={0.58,0.35,0.02},
        nose_bridge={0,0.18,0.08}, nose_tip={0,-0.02,0.18},
        mouth_left={-0.42,-0.42,0.04}, mouth_right={0.42,-0.42,0.04},
        upper_lip={0,-0.36,0.08}, lower_lip={0,-0.54,0.07}, chin={0,-0.78,0.01},
        jaw_hinge_left={-0.82,-0.36,0}, jaw_hinge_right={0.82,-0.36,0},
        brow_left={-0.36,0.58,0.02}, brow_right={0.36,0.58,0.02}
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

local function inspectMesh(meshDebugObject)
    local ok,topology=dpCall(function() return tFacialLandmarks.inspect(meshDebugObject) end)
    if not ok or not topology or not topology.bounds then return nil, 0, nil end
    topology.bounds.total=topology.vertexCount
    return topology.bounds,topology.vertexCount,topology
end

local function refreshLandmarkMarkers()
    if not state.topology or not state.mesh then return end
    local bounds=state.meshBounds
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
    state.mesh=candidate
    state.meshDebug=inspector
    state.meshBounds=bounds
    state.meshVertexCount=count
    state.topology=topology
    state.mesh.visible=true
    if not preserveProject then state.project=tFacialProject.new() end
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
    frameCamera(bounds)
    setStatus(string.format(tLang.L('fre_loaded_fmt'),shortName(path),count),false)
    return true
end

local function newProject()
    destroyPreview()
    state.project=tFacialProject.new()
    state.projectPath=nil
    state.modified=false
    state.selectedRole=1
    state.selectedLandmark=1
    state.selectedMask=1
    state.landmarkEditMode=state.project.landmarkEditMode or 1
    clearHistory()
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
    setStatus(tLang.L('fre_new_project'),false)
end

local function openMesh()
    local path=mbm.openFile(state.project.meshPath or '', 'msh')
    if path then loadMesh(path,false) end
end

local function openProject()
    local path=mbm.openFile(state.projectPath or '', 'lua')
    if not path then return end
    local project,errorMessage=tFacialProject.load(path)
    if not project then
        setStatus(string.format(tLang.L('fre_project_load_failed_fmt'),tostring(errorMessage)),true)
        return
    end
    state.project=project
    state.projectPath=path
    state.selectedRole=project.selectedRole or 1
    state.selectedLandmark=project.selectedLandmark or 1
    state.landmarkEditMode=project.landmarkEditMode or 1
    state.modified=false
    if project.meshPath~='' then
        if not loadMesh(project.meshPath,true) then state.project=project end
    else
        destroyPreview()
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

local function showStatus()
    if state.status=='' then return end
    if state.statusError then
        tImGui.TextColored({r=1,g=0.35,b=0.25,a=1},state.status)
    else
        tImGui.TextWrapped(state.status)
    end
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

local function selectWorkspace(workspace)
    state.workspace=state.workspace==workspace and nil or workspace
    markLandmarkMarkersDirty()
    markFaceGuideDirty()
end

local function showSetup()
    tImGui.TextWrapped(tLang.L('fre_setup_help'))
    if tImGui.Button(tLang.L('fre_open_mesh')) then openMesh() end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('fre_open_project')) then openProject() end
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    tImGui.Separator()
    tImGui.Text(string.format(tLang.L('fre_mesh_summary_fmt'),shortName(state.project.meshPath),state.meshVertexCount))
    tImGui.TextWrapped(tLang.L('fre_setup_roles_help'))
    tImGui.Text(string.format(tLang.L('fre_role_summary_fmt'),#state.project.roles,#state.project.masks))
end

local function showMasks()
    tImGui.TextWrapped(tLang.L('fre_masks_help'))
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
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
            tImGui.Text(role.object~='' and role.object or tLang.L('fre_unassigned'))
            tImGui.TableSetColumnIndex(2)
            tImGui.Text(role.enabled and tLang.L('fre_yes') or tLang.L('fre_no'))
        end
        tImGui.EndTable()
    end
    if selected then
        tImGui.Separator()
        tImGui.Text(string.format(tLang.L('fre_selected_role_fmt'),selected.name))
        tImGui.PushItemWidth(320)
        local changed,object=tImGui.InputText(tLang.L('fre_object_name')..'##freObjectName',selected.object or '')
        if changed then
            pushUndoSnapshot()
            selected.object=object
            markModified()
        end
        tImGui.PopItemWidth()
        local enabled=tImGui.Checkbox(tLang.L('fre_role_enabled'),selected.enabled)
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
        local roleChanged,roleIndex=tImGui.Combo(tLang.L('fre_mask_role'),selectedMask.roleIndex or 1,roleNames)
        if roleChanged then
            pushUndoSnapshot()
            selectedMask.roleIndex=roleIndex
            markModified()
        end
        tImGui.TextWrapped(tLang.L('fre_mask_edit_help'))
        local addPressed=tImGui.Button(tLang.L('fre_mask_add_vertices'))
        tImGui.SameLine()
        local removePressed=tImGui.Button(tLang.L('fre_mask_remove_vertices'))
        if addPressed then state.maskEditMode='add' end
        if removePressed then state.maskEditMode='remove' end
        tImGui.Text(tLang.L('fre_mask_mode') .. ': ' .. state.maskEditMode)
        if tImGui.Button(tLang.L('fre_mask_clear')) then
            pushUndoSnapshot()
            selectedMask.vertices={}
            markModified()
        end
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

local function showRig()
    tImGui.TextWrapped(tLang.L('fre_rig_help'))
    local report=tFacialProject.validate(state.project,state.mesh~=nil,state.topology)
    if report.ok then
        tImGui.TextColored({r=0.35,g=1,b=0.45,a=1},tLang.L('fre_validation_ok'))
    else
        tImGui.TextColored({r=1,g=0.55,b=0.2,a=1},tLang.L('fre_validation_pending'))
    end
    showProjectMessages(report)
    local binding=tFacialProject.bindingDescription(state.project,state.topology)
    tImGui.Separator()
    tImGui.Text(string.format(tLang.L('fre_binding_summary_fmt'),#binding.roles,#binding.masks,
        #binding.landmarks,binding.vertexCount))
    tImGui.TextWrapped(tLang.L('fre_binding_preview_help'))
    tImGui.Separator()
    tImGui.TextWrapped(tLang.L('fre_rig_not_implemented'))
end

local function showPlaceholder(key)
    tImGui.TextWrapped(tLang.L(key))
    tImGui.TextDisabled(tLang.L('fre_future_workspace'))
end

local function showWorkspaceTree()
    tImGui.Text(tLang.L('fre_workspaces'))
    local workspaces={
        {'setup','fre_setup'}, {'face_position','fre_face_position'}, {'landmarks','fre_landmarks'}, {'masks','fre_masks'},
        {'rig','fre_rig'}, {'expressions','fre_expressions'}, {'preview','fre_preview'}
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
                elseif entry[1]=='expressions' then showPlaceholder('fre_expressions_help')
                elseif entry[1]=='preview' then showPlaceholder('fre_preview_help') end
            end
            tImGui.TreePop()
        end
    end
end

local function showMainPanel()
    local flags=state.windowMove and 0 or noMoveFlag
    tUtil.setInitialWindowPositionLeft(windows.main,0,24,560)
    tImGui.SetNextWindowSize({x=560,y=720},tImGui.Flags('ImGuiCond_Always'))
    tImGui.SetNextWindowSizeConstraints({x=560,y=720},{x=560,y=720})
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
    tUtil.sMessageOverlay=tLang.L('fre_welcome')
    state.cameraMove={forward=0,right=0,vertical=0}
    mbm.setColor(0.08,0.09,0.12)
    applyCamera()
end

function onLoop(delta)
    updateCameraKeyboard(delta)
    showMenu()
    showMainPanel()
    showCameraPanel()
    if state.landmarkMarkersDirty then refreshLandmarkMarkers() end
    if state.faceGuideDirty then refreshFaceGuide() end
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
