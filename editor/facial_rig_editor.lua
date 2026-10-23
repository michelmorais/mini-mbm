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
    selectedRole=1, selectedLandmark=1, status='', statusError=false,
    selectedMask=1, maskEditMode='add',
    camera={fx=0,fy=0,fz=0,distance=10,azimuth=0,elevation=0.15},
    cameraDrag=nil, cameraPan=nil, controlDown=false, shiftDown=false,
    windowMove=false, leftPanelRight=440
}

local windows = {
    main='facial_rig_main', camera='facial_rig_camera', report='facial_rig_report'
}

local function shortName(path)
    return tUtil.getShortName(path or '', false) or path or ''
end

local function setStatus(message, isError)
    state.status=tostring(message or '')
    state.statusError=isError == true
    tUtil.showMessage(state.status, state.statusError and 5 or 3)
end

local function markModified()
    state.modified=true
end

local function destroyPreview()
    for _,marker in ipairs(state.landmarkMarkers or {}) do
        dpCall(function() marker:destroy() end)
    end
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
        if not marker then
            marker=line:new('3d',0,0,0)
            marker:add({{x=-size,y=0,z=0},{x=size,y=0,z=0},
                {x=0,y=-size,z=0},{x=0,y=size,z=0},
                {x=0,y=0,z=-size},{x=0,y=0,z=size}})
            marker.alwaysOnTop=true
            marker.alwaysOnTopPriority=1
            state.landmarkMarkers[index]=marker
        end
        local visible=state.workspace=='landmarks' and state.mesh.visible and landmark.placed and landmark.position
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
    state.modified=preserveProject and state.modified or true
    markLandmarkMarkersDirty()
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
    markLandmarkMarkersDirty()
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
    state.modified=false
    if project.meshPath~='' then
        if not loadMesh(project.meshPath,true) then state.project=project end
    else
        destroyPreview()
    end
    setStatus(string.format(tLang.L('fre_project_loaded_fmt'),shortName(path)),false)
end

local function saveProject(path)
    path=path or state.projectPath
    if not path or path=='' then return false end
    state.project.selectedRole=state.selectedRole
    state.project.selectedLandmark=state.selectedLandmark
    state.project.selectedMask=state.selectedMask
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
    if state.workspace~=workspace then
        state.workspace=workspace
        markLandmarkMarkersDirty()
    end
end

local function showWorkspaceButtons()
    tImGui.Text(tLang.L('fre_workspaces'))
    local workspaces={
        {'setup','fre_setup'}, {'masks','fre_masks'}, {'landmarks','fre_landmarks'},
        {'rig','fre_rig'}, {'expressions','fre_expressions'}, {'preview','fre_preview'}
    }
    for _,entry in ipairs(workspaces) do
        local label=tLang.L(entry[2])
        if state.workspace==entry[1] then label='[x] ' .. label end
        if tImGui.SmallButton(label .. '##freWorkspace_' .. entry[1]) then selectWorkspace(entry[1]) end
        tImGui.SameLine()
    end
    tImGui.NewLine()
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
        local changed,object=tImGui.InputText(tLang.L('fre_object_name')..'##freObjectName',selected.object or '',256)
        if changed then selected.object=object; markModified() end
        local enabled=tImGui.Checkbox(tLang.L('fre_role_enabled'),selected.enabled)
        if enabled~=selected.enabled then selected.enabled=enabled; markModified() end
        if tImGui.Button(tLang.L('fre_add_mask')) then
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
        local changed,name=tImGui.InputText(tLang.L('fre_mask_name')..'##freMaskName',selectedMask.name or '',128)
        if changed and name~='' then selectedMask.name=name; markModified() end
        local roleNames=tFacialProject.roleNames()
        local roleChanged,roleIndex=tImGui.Combo(tLang.L('fre_mask_role'),selectedMask.roleIndex or 1,roleNames)
        if roleChanged then selectedMask.roleIndex=roleIndex; markModified() end
        tImGui.TextWrapped(tLang.L('fre_mask_edit_help'))
        local addPressed=tImGui.Button(tLang.L('fre_mask_add_vertices'))
        tImGui.SameLine()
        local removePressed=tImGui.Button(tLang.L('fre_mask_remove_vertices'))
        if addPressed then state.maskEditMode='add' end
        if removePressed then state.maskEditMode='remove' end
        tImGui.Text(tLang.L('fre_mask_mode') .. ': ' .. state.maskEditMode)
        if tImGui.Button(tLang.L('fre_mask_clear')) then selectedMask.vertices={}; markModified() end
    end
end

local function showLandmarks()
    tImGui.TextWrapped(tLang.L('fre_landmarks_help'))
    if not state.mesh then
        tImGui.TextDisabled(tLang.L('fre_no_mesh'))
        return
    end
    local landmarks=state.project.landmarks
    tImGui.PushItemWidth(220)
    local names={}
    for _,landmark in ipairs(landmarks) do names[#names+1]=landmark.name end
    local changed,index=tImGui.Combo(tLang.L('fre_landmark_select'),state.selectedLandmark,names)
    if changed then state.selectedLandmark=index; markLandmarkMarkersDirty() end
    local selected=landmarks[state.selectedLandmark]
    if selected then
        tImGui.Text(string.format(tLang.L('fre_landmark_side_fmt'),selected.name,selected.side))
        local xChanged,x=tImGui.InputFloat('X##freLandmarkX',selected.position.x,0,0,'%.4f',0)
        local yChanged,y=tImGui.InputFloat('Y##freLandmarkY',selected.position.y,0,0,'%.4f',0)
        local zChanged,z=tImGui.InputFloat('Z##freLandmarkZ',selected.position.z,0,0,'%.4f',0)
        if xChanged then selected.position.x=x; selected.vertexIndex=nil; selected.placed=true; markModified(); markLandmarkMarkersDirty() end
        if yChanged then selected.position.y=y; selected.vertexIndex=nil; selected.placed=true; markModified(); markLandmarkMarkersDirty() end
        if zChanged then selected.position.z=z; selected.vertexIndex=nil; selected.placed=true; markModified(); markLandmarkMarkersDirty() end
        local placed=tImGui.Checkbox(tLang.L('fre_landmark_placed'),selected.placed)
        if placed~=selected.placed then selected.placed=placed; markModified(); markLandmarkMarkersDirty() end
    end
    tImGui.PopItemWidth()
    local report=tFacialProject.validate(state.project,true,state.topology)
    tImGui.Text(string.format(tLang.L('fre_landmark_count_fmt'),report.placedLandmarkCount,report.landmarkCount))
    for _,message in ipairs(report.errors) do tImGui.TextWrapped('Error: ' .. message) end
end

local function showRig()
    tImGui.TextWrapped(tLang.L('fre_rig_help'))
    local report=tFacialProject.validate(state.project,state.mesh~=nil,state.topology)
    if report.ok then
        tImGui.TextColored({r=0.35,g=1,b=0.45,a=1},tLang.L('fre_validation_ok'))
    else
        tImGui.TextColored({r=1,g=0.55,b=0.2,a=1},tLang.L('fre_validation_pending'))
    end
    for _,message in ipairs(report.errors) do tImGui.TextWrapped('Error: ' .. message) end
    for _,message in ipairs(report.warnings) do tImGui.TextWrapped('Warning: ' .. message) end
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

local function showMainPanel()
    local flags=state.windowMove and 0 or noMoveFlag
    tUtil.setInitialWindowPositionLeft(windows.main,0,24,440)
    tImGui.SetNextWindowSizeConstraints({x=350,y=360},{x=800,y=900})
    local opened=tImGui.Begin(tLang.L('fre_title')..'##'..windows.main,true,flags)
    local position=tImGui.GetWindowPos()
    local size=tImGui.GetWindowSize()
    state.leftPanelRight=(position.x or 0)+(size.x or 440)
    if opened then
        showWorkspaceButtons()
        if state.projectPath then
            tImGui.Text(shortName(state.projectPath) .. (state.modified and ' *' or ''))
        else
            tImGui.TextDisabled(tLang.L('fre_unsaved_project'))
        end
        showStatus()
        tImGui.Separator()
        if state.workspace=='setup' then showSetup()
        elseif state.workspace=='masks' then showMasks()
        elseif state.workspace=='landmarks' then showLandmarks()
        elseif state.workspace=='rig' then showRig()
        elseif state.workspace=='expressions' then showPlaceholder('fre_expressions_help')
        elseif state.workspace=='preview' then showPlaceholder('fre_preview_help')
        end
    end
    tImGui.End()
end

local function showCameraPanel()
    local width=mbm.getRealSizeScreen()
    tUtil.setInitialWindowPositionRight(windows.camera,0,25,310)
    local opened=tImGui.Begin(tLang.L('fre_camera')..'##'..windows.camera,true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if opened then
        if tImGui.Button(tLang.L('fre_frame_mesh')) then frameCamera(state.meshBounds) end
        tImGui.SameLine()
        local visible=tImGui.Checkbox(tLang.L('fre_show_mesh'),state.mesh and state.mesh.visible or false)
        if state.mesh and visible~=state.mesh.visible then state.mesh.visible=visible; markLandmarkMarkersDirty() end
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

local function toggleMaskVertex(index)
    local mask=state.project.masks[state.selectedMask]
    if not mask or not index then return false end
    mask.topologySignature=state.topology.signature
    for position,vertex in ipairs(mask.vertices or {}) do
        if vertex==index then
            if state.maskEditMode=='remove' then table.remove(mask.vertices,position); markModified() end
            return true
        end
    end
    if state.maskEditMode=='add' then mask.vertices[#mask.vertices+1]=index; markModified() end
    return true
end

local function handleViewportAuthoringClick(x,y)
    if not state.topology or not state.mesh then return false end
    local origin,direction=viewportPickRay(x,y)
    if not origin then return false end
    if state.workspace=='landmarks' then
        local landmarkHit=tFacialLandmarks.nearestLandmark(state.project.landmarks,origin,direction,
            viewportPickRadius(0.04))
        if landmarkHit then
            state.selectedLandmark=landmarkHit.index
            state.project.selectedLandmark=landmarkHit.index
            markLandmarkMarkersDirty()
            return true
        end
        local vertexHit=tFacialLandmarks.nearestVertex(state.topology,origin,direction,
            viewportPickRadius(0.025))
        if vertexHit then
            local selected=state.project.landmarks[state.selectedLandmark]
            if selected then
                selected.position={x=vertexHit.point.x,y=vertexHit.point.y,z=vertexHit.point.z}
                selected.placed=true
                selected.vertexIndex=vertexHit.index
                state.project.selectedLandmark=state.selectedLandmark
                markModified()
                markLandmarkMarkersDirty()
                return true
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
    if key==0 and handleViewportAuthoringClick(x,y) then return end
    if key==0 then
        state.cameraDrag={x=x,y=y}
    elseif key==1 then
        state.cameraPan={x=x,y=y}
    end
end

function onTouchMove(key,x,y)
    if tImGui.GetWantCaptureMouse() then return end
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
    if key==0 then state.cameraDrag=nil
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
