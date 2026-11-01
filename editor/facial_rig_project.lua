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

   Facial Rig Editor project model.
]]--

local M = {}
local tTargetGizmo = require 'facial_rig_target_gizmo'

local ROLE_NAMES = {
    'main_face', 'left_eye', 'right_eye', 'upper_teeth', 'lower_teeth',
    'tongue', 'brows', 'nose', 'mouth', 'hair', 'accessory'
}

local SOURCE_OBJECT_SUGGESTIONS = {
    main_face={'Head', 'Face', 'FaceMesh'},
    left_eye={'Eye.L', 'LeftEye', 'eye_left'},
    right_eye={'Eye.R', 'RightEye', 'eye_right'},
    upper_teeth={'UpperTeeth', 'Teeth.Upper', 'Teeth'},
    lower_teeth={'LowerTeeth', 'Teeth.Lower'},
    tongue={'Tongue'},
    brows={'Brows', 'Brow.L', 'Brow.R'},
    nose={'Nose'},
    mouth={'Mouth', 'Lips'},
    hair={'Hair'},
    accessory={'Accessory'}
}

local LANDMARK_NAMES = {
    {'face_left', 'left', true}, {'face_right', 'right', true},
    {'face_top', 'midline', true}, {'face_bottom', 'midline', true},
    {'left_eye_center', 'left', true}, {'right_eye_center', 'right', true},
    {'left_eye_corner', 'left', false}, {'right_eye_corner', 'right', false},
    {'nose_bridge', 'midline', false}, {'nose_tip', 'midline', false},
    {'mouth_left', 'left', true}, {'mouth_right', 'right', true},
    {'upper_lip', 'midline', false}, {'lower_lip', 'midline', false},
    {'chin', 'midline', false}, {'jaw_hinge_left', 'left', false},
    {'jaw_hinge_right', 'right', false}, {'brow_left', 'left', false},
    {'brow_right', 'right', false}
}

local function copyPoint(point)
    return {x=tonumber(point and point.x) or 0,
        y=tonumber(point and point.y) or 0,
        z=tonumber(point and point.z) or 0}
end

local function copyColor(color)
    return {r=tonumber(color and color.r) or 1,g=tonumber(color and color.g) or 1,
        b=tonumber(color and color.b) or 1,a=tonumber(color and color.a) or 1}
end

local function defaultRoles()
    local result = {}
    for index, name in ipairs(ROLE_NAMES) do
        result[index] = {name=name, object='', enabled=name == 'main_face'}
    end
    return result
end

local function defaultLandmarks()
    local result = {}
    for index, entry in ipairs(LANDMARK_NAMES) do
        result[index] = {name=entry[1], side=entry[2], required=entry[3],
            position={x=0, y=0, z=0}, placed=false}
    end
    return result
end

local function defaultFaceOrigin()
    return {placed=false, position={x=0, y=0, z=0}, vertexIndex=nil, scale=1}
end

function M.new()
    return {
        schemaVersion=13,
        meshPath='',
        meshVertexCount=0,
        meshTopologySignature='',
        roles=defaultRoles(),
        sourceObjects={},
        sourceVertexMap={},
        sourceVertexMapSignature='',
        masks={},
        landmarks=defaultLandmarks(),
        facialMasks={},
        surfaceControls={},
        faceOrigin=defaultFaceOrigin(),
        selectedRole=1,
        selectedLandmark=1,
        selectedMask=1,
        selectedFacialMask=1,
        selectedSurfaceControl=1,
        landmarkEditMode=1,
        morphTargetPreview={targetId='',weight=1},
        morphTargetPivots={},
        morphTargetEdits=nil,
        expressions={targets={},poses={},selectedTarget=1,selectedPose=1},
        clips={},selectedClip=1,
        language=nil
    }
end

function M.roleNames()
    local result = {}
    for index, name in ipairs(ROLE_NAMES) do result[index] = name end
    return result
end

function M.sourceObjectSuggestions(roleName)
    local result={}
    for _, objectName in ipairs(SOURCE_OBJECT_SUGGESTIONS[roleName] or {}) do
        result[#result + 1]=objectName
    end
    return result
end

function M.normalize(project)
    if type(project) ~= 'table' then return nil, 'project must be a table' end
    project.schemaVersion=math.max(13,tonumber(project.schemaVersion) or 1)
    project.meshPath=type(project.meshPath) == 'string' and project.meshPath or ''
    project.meshVertexCount=math.max(0, math.floor(tonumber(project.meshVertexCount) or 0))
    project.meshTopologySignature=type(project.meshTopologySignature) == 'string' and
        project.meshTopologySignature or ''
    project.roles=type(project.roles) == 'table' and project.roles or defaultRoles()
    project.sourceObjects=type(project.sourceObjects) == 'table' and project.sourceObjects or {}
    project.sourceVertexMap=type(project.sourceVertexMap) == 'table' and project.sourceVertexMap or {}
    project.sourceVertexMapSignature=type(project.sourceVertexMapSignature)=='string' and
        project.sourceVertexMapSignature or ''
    project.masks=type(project.masks) == 'table' and project.masks or {}
    project.landmarks=type(project.landmarks) == 'table' and project.landmarks or defaultLandmarks()
    project.facialMasks=type(project.facialMasks)=='table' and project.facialMasks or {}
    project.surfaceControls=type(project.surfaceControls)=='table' and project.surfaceControls or {}
    if type(project.faceOrigin) ~= 'table' then project.faceOrigin=defaultFaceOrigin() end
    project.faceOrigin.placed=project.faceOrigin.placed == true
    project.faceOrigin.position=copyPoint(project.faceOrigin.position)
    local originVertex=tonumber(project.faceOrigin.vertexIndex)
    project.faceOrigin.vertexIndex=originVertex and math.floor(originVertex) or nil
    project.faceOrigin.scale=math.max(tonumber(project.faceOrigin.scale) or 1,1e-6)
    project.selectedRole=math.max(1, math.floor(tonumber(project.selectedRole) or 1))
    project.selectedLandmark=math.max(1, math.floor(tonumber(project.selectedLandmark) or 1))
    project.selectedMask=math.max(1, math.floor(tonumber(project.selectedMask) or 1))
    project.selectedFacialMask=math.max(1,math.floor(tonumber(project.selectedFacialMask) or 1))
    project.selectedSurfaceControl=math.max(1,math.floor(tonumber(project.selectedSurfaceControl) or 1))
    project.landmarkEditMode=math.max(1,math.min(2,
        math.floor(tonumber(project.landmarkEditMode) or 1)))
    local targetPreview=type(project.morphTargetPreview)=='table' and project.morphTargetPreview or {}
    local previewWeight=tonumber(targetPreview.weight)
    if not previewWeight or previewWeight~=previewWeight or
            previewWeight==math.huge or previewWeight==-math.huge then previewWeight=1 end
    project.morphTargetPreview={targetId=type(targetPreview.targetId)=='string' and
        targetPreview.targetId or '',weight=math.max(0,math.min(1,previewWeight))}
    project.morphTargetPivots=tTargetGizmo.normalizePivots(project.morphTargetPivots)
    if type(project.morphTargetEdits)~='table' then project.morphTargetEdits=nil end
    if type(project.expressions)~='table' then
        project.expressions={targets={},poses={},selectedTarget=1,selectedPose=1}
    else
        project.expressions.targets=type(project.expressions.targets)=='table' and project.expressions.targets or {}
        project.expressions.poses=type(project.expressions.poses)=='table' and project.expressions.poses or {}
        project.expressions.selectedTarget=math.max(1,math.floor(tonumber(project.expressions.selectedTarget) or 1))
        project.expressions.selectedPose=math.max(1,math.floor(tonumber(project.expressions.selectedPose) or 1))
    end
    project.clips=type(project.clips)=='table' and project.clips or {}
    project.selectedClip=math.max(1,math.floor(tonumber(project.selectedClip) or 1))
    project.selectedClip=math.min(project.selectedClip,math.max(1,#project.clips))
    if project.language~='en' and project.language~='pt_br' then project.language=nil end
    for index, role in ipairs(project.roles) do
        if type(role) ~= 'table' then
            project.roles[index]={name=ROLE_NAMES[index] or ('role_' .. index), object='', enabled=true}
        else
            role.name=type(role.name) == 'string' and role.name ~= '' and role.name or
                (ROLE_NAMES[index] or ('role_' .. index))
            role.object=type(role.object) == 'string' and role.object or ''
            role.enabled=role.enabled ~= false
        end
    end
    local sourceObjects={}
    local sourceObjectSet={}
    for _, objectName in ipairs(project.sourceObjects) do
        if type(objectName) == 'string' then
            objectName=objectName:match('^%s*(.-)%s*$')
            if objectName ~= '' and not sourceObjectSet[objectName] then
                sourceObjectSet[objectName]=true
                sourceObjects[#sourceObjects + 1]=objectName
            end
        end
    end
    for _, role in ipairs(project.roles) do
        local objectName=role.object
        if objectName ~= '' and not sourceObjectSet[objectName] then
            sourceObjectSet[objectName]=true
            sourceObjects[#sourceObjects + 1]=objectName
        end
    end
    project.sourceObjects=sourceObjects
    local sourceVertexMap={}
    for objectName,indices in pairs(project.sourceVertexMap) do
        if type(objectName)=='string' and type(indices)=='table' then
            local copied,seen={},{}
            for _,rawIndex in ipairs(indices) do
                local index=tonumber(rawIndex)
                if index and index>=1 and index%1==0 and not seen[index] then
                    seen[index]=true
                    copied[#copied+1]=math.floor(index)
                end
            end
            table.sort(copied)
            if #copied>0 then sourceVertexMap[objectName]=copied end
        end
    end
    project.sourceVertexMap=sourceVertexMap
    for objectName in pairs(sourceVertexMap) do
        if not sourceObjectSet[objectName] then
            sourceObjectSet[objectName]=true
            project.sourceObjects[#project.sourceObjects+1]=objectName
        end
    end
    for index, mask in ipairs(project.masks) do
        if type(mask) ~= 'table' then
            project.masks[index]={name='region_' .. index,roleIndex=1,representation='vertex',
                vertices={},topologySignature='',enabled=true}
        else
            mask.name=type(mask.name)=='string' and mask.name~='' and mask.name or ('region_' .. index)
            mask.roleIndex=math.max(1,math.floor(tonumber(mask.roleIndex) or 1))
            mask.purpose=mask.purpose=='target' and 'target' or 'rig'
            mask.representation=mask.representation=='polygon' and 'polygon' or 'vertex'
            -- Normalize the serialized list before any read-only review runs. Older
            -- editor versions could leave boolean set-members in this array while
            -- calculating a binding review; discard those malformed values and keep
            -- one canonical integer entry for each vertex.
            local normalizedVertices,seenVertices={},{}
            for _,rawIndex in ipairs(type(mask.vertices)=='table' and mask.vertices or {}) do
                local vertexIndex=tonumber(rawIndex)
                if vertexIndex and vertexIndex>=1 and vertexIndex%1==0 and not seenVertices[vertexIndex] then
                    seenVertices[vertexIndex]=true
                    normalizedVertices[#normalizedVertices+1]=math.floor(vertexIndex)
                end
            end
            table.sort(normalizedVertices)
            mask.vertices=normalizedVertices
            mask.topologySignature=type(mask.topologySignature)=='string' and mask.topologySignature or ''
            mask.enabled=mask.enabled ~= false
            if type(mask.polygon)=='table' or mask.representation=='polygon' then
                if type(mask.polygon)~='table' then mask.polygon={} end
                if type(mask.polygon.points)~='table' then mask.polygon.points={} end
                mask.polygon.closed=mask.polygon.closed==true
                mask.polygon.topologySignature=type(mask.polygon.topologySignature)=='string' and
                    mask.polygon.topologySignature or mask.topologySignature
                if type(mask.polygon.derivedVertexWeights)~='table' then
                    mask.polygon.derivedVertexWeights={}
                end
                local normalizedPoints,seenPoints={},{}
                for _,rawPoint in ipairs(mask.polygon.points) do
                    local pointIndex=type(rawPoint)=='table' and tonumber(rawPoint.index) or tonumber(rawPoint)
                    pointIndex=pointIndex and math.floor(pointIndex) or nil
                    if pointIndex and pointIndex>=1 and not seenPoints[pointIndex] then
                        seenPoints[pointIndex]=true
                        normalizedPoints[#normalizedPoints+1]={index=pointIndex}
                    end
                end
                mask.polygon.points=normalizedPoints
                local normalizedWeights,seenWeights={},{}
                for _,rawWeight in ipairs(mask.polygon.derivedVertexWeights) do
                    local pointIndex=type(rawWeight)=='table' and tonumber(rawWeight.index) or tonumber(rawWeight)
                    local weight=type(rawWeight)=='table' and tonumber(rawWeight.weight) or 1
                    pointIndex=pointIndex and math.floor(pointIndex) or nil
                    if pointIndex and pointIndex>=1 and not seenWeights[pointIndex] then
                        seenWeights[pointIndex]=true
                        normalizedWeights[#normalizedWeights+1]={index=pointIndex,weight=weight or 1}
                    end
                end
                mask.polygon.derivedVertexWeights=normalizedWeights
            end
        end
    end
    for index, landmark in ipairs(project.landmarks) do
        if type(landmark) ~= 'table' then
            project.landmarks[index]=defaultLandmarks()[index]
        else
            landmark.name=type(landmark.name) == 'string' and landmark.name or ('landmark_' .. index)
            landmark.side=type(landmark.side) == 'string' and landmark.side or 'midline'
            landmark.required=landmark.required == true
            landmark.position=copyPoint(landmark.position)
            if landmark.relativePosition ~= nil then
                landmark.relativePosition=copyPoint(landmark.relativePosition)
            elseif project.faceOrigin.placed and landmark.placed then
                landmark.relativePosition={x=landmark.position.x-project.faceOrigin.position.x,
                    y=landmark.position.y-project.faceOrigin.position.y,
                    z=landmark.position.z-project.faceOrigin.position.z}
            end
            landmark.placed=landmark.placed == true
            local vertexIndex=tonumber(landmark.vertexIndex)
            landmark.vertexIndex=vertexIndex and math.floor(vertexIndex) or nil
        end
    end
    local surfaceControlIds={}
    for index,control in ipairs(project.surfaceControls) do
        if type(control)~='table' then
            project.surfaceControls[index]={id='surface_control_'..index,name='control_'..index,
                kind='polygon',points={},closed=true,topologySignature='',depthOffset=0,
                relationships={},enabled=true}
            local generated=project.surfaceControls[index]
            local suffix=2
            while surfaceControlIds[generated.id] do
                generated.id='surface_control_'..index..'_'..suffix
                suffix=suffix+1
            end
            surfaceControlIds[generated.id]=true
        else
            control.id=type(control.id)=='string' and control.id~='' and control.id or
                ('surface_control_'..index)
            if surfaceControlIds[control.id] then
                local suffix=2
                local baseId='surface_control_'..index
                control.id=baseId
                while surfaceControlIds[control.id] do
                    control.id=baseId..'_'..suffix
                    suffix=suffix+1
                end
            end
            surfaceControlIds[control.id]=true
            control.name=type(control.name)=='string' and control.name~='' and control.name or
                ('control_'..index)
            control.kind=control.kind=='polyline' and 'polyline' or control.kind=='point' and 'point' or 'polygon'
            control.closed=control.kind=='polygon' and control.closed~=false or false
            control.topologySignature=type(control.topologySignature)=='string' and control.topologySignature or ''
            control.depthOffset=tonumber(control.depthOffset) or 0
            control.enabled=control.enabled~=false
            control.groupId=type(control.groupId)=='string' and control.groupId or ''
            control.role=control.role=='action_line' and 'action_line' or
                control.role=='controller' and 'controller' or 'area'
            control.color=copyColor(control.color)
            control.relationships=type(control.relationships)=='table' and control.relationships or {}
            if control.kind~='point' then control.relationships={} end
            local normalizedRelationships,seenRelationships={},{}
            for _,relationship in ipairs(control.relationships) do
                local controlId=type(relationship)=='table' and relationship.controlId or relationship
                local weight=type(relationship)=='table' and tonumber(relationship.weight) or 1
                if type(controlId)=='string' and controlId~='' and not seenRelationships[controlId] then
                    seenRelationships[controlId]=true
                    normalizedRelationships[#normalizedRelationships+1]={controlId=controlId,
                        weight=math.max(0,math.min(1,weight or 1))}
                end
            end
            control.relationships=normalizedRelationships
            local normalizedPoints,seenPoints={},{}
            for _,rawPoint in ipairs(type(control.points)=='table' and control.points or {}) do
                local pointIndex=type(rawPoint)=='table' and tonumber(rawPoint.index) or tonumber(rawPoint)
                pointIndex=pointIndex and math.floor(pointIndex) or nil
                if pointIndex and pointIndex>=1 and not seenPoints[pointIndex] then
                    seenPoints[pointIndex]=true
                    normalizedPoints[#normalizedPoints+1]={index=pointIndex}
                end
            end
            control.points=normalizedPoints
            if control.kind=='point' and #control.points>1 then
                control.points={control.points[1]}
            end
        end
    end
    local facialMaskIds={}
    for index,group in ipairs(project.facialMasks) do
        if type(group)~='table' then
            project.facialMasks[index]={id='facial_mask_'..index,name='facial_mask_'..index,
                nameKey='',templateId='',actionUnits={},color={r=1,g=1,b=1,a=1},
                controlIds={},enabled=true}
            group=project.facialMasks[index]
        end
        group.id=type(group.id)=='string' and group.id~='' and group.id or ('facial_mask_'..index)
        if facialMaskIds[group.id] then group.id='facial_mask_'..index end
        facialMaskIds[group.id]=true
        group.name=type(group.name)=='string' and group.name~='' and group.name or group.id
        group.nameKey=type(group.nameKey)=='string' and group.nameKey or ''
        group.templateId=type(group.templateId)=='string' and group.templateId or ''
        group.color=copyColor(group.color)
        group.enabled=group.enabled~=false
        local actionUnits={}
        for _,actionUnit in ipairs(type(group.actionUnits)=='table' and group.actionUnits or {}) do
            if type(actionUnit)=='string' and actionUnit~='' then actionUnits[#actionUnits+1]=actionUnit end
        end
        group.actionUnits=actionUnits
        local controlIds,seenControlIds={},{}
        for _,controlId in ipairs(type(group.controlIds)=='table' and group.controlIds or {}) do
            if type(controlId)=='string' and controlId~='' and not seenControlIds[controlId] then
                seenControlIds[controlId]=true
                controlIds[#controlIds+1]=controlId
            end
        end
        group.controlIds=controlIds
    end
    project.selectedFacialMask=math.min(project.selectedFacialMask,math.max(1,#project.facialMasks))
    project.selectedSurfaceControl=math.min(project.selectedSurfaceControl,math.max(1,#project.surfaceControls))
    return project
end

function M.setFaceOrigin(project, point, scale, vertexIndex)
    if type(project) ~= 'table' or type(point) ~= 'table' then return false end
    project.faceOrigin=project.faceOrigin or defaultFaceOrigin()
    project.faceOrigin.position=copyPoint(point)
    project.faceOrigin.scale=math.max(tonumber(scale) or project.faceOrigin.scale or 1,1e-6)
    project.faceOrigin.vertexIndex=tonumber(vertexIndex) and math.floor(vertexIndex) or nil
    project.faceOrigin.placed=true
    return true
end

function M.addMask(project, name, roleIndex)
    local baseName=type(name)=='string' and name:match('^%s*(.-)%s*$') or ''
    if baseName=='' then baseName='region_' .. (#project.masks + 1) end
    local uniqueName=baseName
    local suffix=2
    local nameTaken=true
    while nameTaken do
        nameTaken=false
        for _,existing in ipairs(project.masks or {}) do
            if existing.name==uniqueName then
                nameTaken=true
                uniqueName=baseName .. '_' .. suffix
                suffix=suffix+1
                break
            end
        end
    end
    local mask = {name=uniqueName, roleIndex=roleIndex or 1,purpose='rig',representation='vertex',
        vertices={}, topologySignature=project.meshTopologySignature or '', enabled=true}
    project.masks[#project.masks + 1] = mask
    return mask
end

function M.addTargetMask(project,name)
    local mask=M.addMask(project,name,1)
    mask.purpose='target'
    return mask
end

function M.addSurfaceControl(project,name,kind)
    project.surfaceControls=project.surfaceControls or {}
    local baseName=type(name)=='string' and name:match('^%s*(.-)%s*$') or ''
    if baseName=='' then baseName='surface_control_' .. (#project.surfaceControls+1) end
    local uniqueName=baseName
    local suffix=2
    while true do
        local taken=false
        for _,existing in ipairs(project.surfaceControls) do
            if existing.name==uniqueName then taken=true break end
        end
        if not taken then break end
        uniqueName=baseName .. '_' .. suffix
        suffix=suffix+1
    end
    local controlKind=kind=='polyline' and 'polyline' or kind=='point' and 'point' or 'polygon'
    local controlIdIndex=#project.surfaceControls+1
    local controlId='surface_control_'..tostring(controlIdIndex)
    local idTaken=true
    while idTaken do
        idTaken=false
        for _,existing in ipairs(project.surfaceControls) do
            if existing.id==controlId then
                idTaken=true
                controlIdIndex=controlIdIndex+1
                controlId='surface_control_'..tostring(controlIdIndex)
                break
            end
        end
    end
    local control={id=controlId,name=uniqueName,
        kind=controlKind,points={},closed=false,topologySignature='',depthOffset=0,
        relationships={},enabled=true}
    project.surfaceControls[#project.surfaceControls+1]=control
    return control
end

function M.surfaceControlNames(project)
    local result={}
    for _,control in ipairs(project and project.surfaceControls or {}) do
        result[#result+1]=control.name or ''
    end
    return result
end

function M.facialMaskNames(project)
    local result={}
    for _,group in ipairs(project and project.facialMasks or {}) do
        result[#result+1]=group.name or group.id or ''
    end
    return result
end

function M.isRigMask(mask)
    return mask and mask.purpose~='target'
end

function M.isPolygonMask(mask)
    return type(mask)=='table' and mask.representation=='polygon'
end

function M.maskRepresentation(mask)
    return M.isPolygonMask(mask) and 'polygon' or 'vertex'
end

function M.sourceObjects(project)
    local result={}
    for _, objectName in ipairs(project and project.sourceObjects or {}) do
        result[#result + 1]=objectName
    end
    return result
end

function M.setSourceVertexMap(project, vertexMap, topologySignature)
    if type(project)~='table' or type(vertexMap)~='table' then return false end
    project.sourceVertexMap={}
    project.sourceVertexMapSignature=type(topologySignature)=='string' and topologySignature or ''
    for objectName,indices in pairs(vertexMap) do
        if type(objectName)=='string' and type(indices)=='table' then
            local copied,seen={},{}
            for _,rawIndex in ipairs(indices) do
                local index=tonumber(rawIndex)
                if index and index>=1 and index%1==0 and not seen[index] then
                    seen[index]=true
                    copied[#copied+1]=math.floor(index)
                end
            end
            table.sort(copied)
            if #copied>0 then
                project.sourceVertexMap[objectName]=copied
                M.addSourceObject(project,objectName)
            end
        end
    end
    return true
end

function M.sourceVertexIndices(project, objectName)
    local result={}
    for _,index in ipairs(project and project.sourceVertexMap and project.sourceVertexMap[objectName] or {}) do
        result[#result+1]=index
    end
    return result
end

function M.addSourceObject(project, objectName)
    if type(project) ~= 'table' or type(objectName) ~= 'string' then return false end
    objectName=objectName:match('^%s*(.-)%s*$')
    if objectName == '' then return false end
    project.sourceObjects=project.sourceObjects or {}
    for index, current in ipairs(project.sourceObjects) do
        if current == objectName then return index end
    end
    project.sourceObjects[#project.sourceObjects + 1]=objectName
    return #project.sourceObjects
end

function M.setTopology(project, topology)
    if type(project) ~= 'table' or type(topology) ~= 'table' then return false end
    project.meshVertexCount=math.max(0, math.floor(tonumber(topology.vertexCount) or 0))
    project.meshTopologySignature=type(topology.signature) == 'string' and topology.signature or ''
    return project.meshTopologySignature ~= '' and project.meshVertexCount > 0
end

function M.validate(project, meshLoaded, topology)
    local faceOriginPlaced=type(project)=='table' and type(project.faceOrigin)=='table' and
        project.faceOrigin.placed == true
    local report = {ok=true, errors={}, warnings={}, roleCount=0, maskCount=0,
        landmarkCount=0, placedLandmarkCount=0,
        faceOriginPlaced=faceOriginPlaced}
    if type(project) ~= 'table' then
        report.ok=false
        report.errors[#report.errors + 1]='project is missing'
        return report
    end
    if not meshLoaded or project.meshPath == '' then
        report.ok=false
        report.errors[#report.errors + 1]='mesh is not loaded'
    end
    if not report.faceOriginPlaced then
        report.ok=false
        report.errors[#report.errors + 1]='face origin is not placed'
    elseif topology and project.faceOrigin.vertexIndex and
            (project.faceOrigin.vertexIndex<1 or project.faceOrigin.vertexIndex>topology.vertexCount) then
        report.ok=false
        report.errors[#report.errors + 1]='face origin vertex is outside the current topology'
    end
    if topology then
        if project.meshTopologySignature == '' then
            report.warnings[#report.warnings + 1]='project has no saved topology signature'
        elseif project.meshTopologySignature ~= topology.signature then
            report.ok=false
            report.errors[#report.errors + 1]='mesh topology changed since the project was saved'
        end
        if project.meshVertexCount > 0 and project.meshVertexCount ~= topology.vertexCount then
            report.ok=false
            report.errors[#report.errors + 1]='mesh vertex count changed since the project was saved'
        end
    end
    local names={}
    for _, role in ipairs(project.roles or {}) do
        if role.enabled then
            report.roleCount=report.roleCount+1
            if role.name == '' then
                report.ok=false
                report.errors[#report.errors + 1]='enabled role has no name'
            elseif names[role.name] then
                report.ok=false
                report.errors[#report.errors + 1]='duplicate role: ' .. role.name
            else
                names[role.name]=true
            end
            if role.name == 'main_face' and role.object == '' then
                report.warnings[#report.warnings + 1]='main_face has no object assignment'
            end
        end
    end
    for index, mask in ipairs(project.masks or {}) do
        local role=project.roles and project.roles[mask.roleIndex or 0]
        if M.isRigMask(mask) and not role then
            description.valid=false
            description.errors[#description.errors + 1]='mask has no target role: ' .. tostring(mask.name or index)
        elseif M.isRigMask(mask) and role.enabled == false and mask.enabled ~= false then
            description.warnings[#description.warnings + 1]='mask targets a disabled role: ' .. tostring(mask.name or index)
        end
    end
    report.maskCount=#(project.masks or {})
    for _, mask in ipairs(project.masks or {}) do
        if type(mask.name) ~= 'string' or mask.name == '' then
            report.ok=false
            report.errors[#report.errors + 1]='mask has no name'
        end
        if type(mask.vertices) ~= 'table' then
            report.ok=false
            report.errors[#report.errors + 1]='mask has invalid vertex list'
        end
        local vertices=type(mask.vertices)=='table' and mask.vertices or {}
        if topology and (mask.topologySignature or '') ~= '' and mask.topologySignature ~= topology.signature then
            report.ok=false
            report.errors[#report.errors + 1]='mask topology is stale: ' .. tostring(mask.name)
        end
        local seen={}
        for _, vertex in ipairs(vertices) do
            local index=tonumber(vertex)
            if not index or index<1 or index> (topology and topology.vertexCount or project.meshVertexCount) or
                    index%1~=0 or seen[index] then
                report.ok=false
                report.errors[#report.errors + 1]='mask has an invalid or duplicate vertex: ' .. tostring(mask.name)
                break
            end
            seen[index]=true
        end
        if M.isPolygonMask(mask) then
            local polygon=mask.polygon
            local polygonSignature=type(polygon)=='table' and polygon.topologySignature or ''
            if type(polygon)~='table' then
                report.ok=false
                report.errors[#report.errors + 1]='polygon mask has no polygon data: ' .. tostring(mask.name)
            elseif topology and polygonSignature~='' and polygonSignature~=topology.signature then
                report.ok=false
                report.errors[#report.errors + 1]='polygon topology is stale: ' .. tostring(mask.name)
            elseif polygon.closed and #(polygon.points or {})<3 then
                report.ok=false
                report.errors[#report.errors + 1]='polygon mask has fewer than three points: ' .. tostring(mask.name)
            end
            for _,rawWeight in ipairs(type(polygon)=='table' and polygon.derivedVertexWeights or {}) do
                local index=type(rawWeight)=='table' and tonumber(rawWeight.index) or tonumber(rawWeight)
                if not index or index<1 or index%1~=0 or topology and index>topology.vertexCount then
                    report.ok=false
                    report.errors[#report.errors + 1]='polygon mask has an invalid derived vertex: ' .. tostring(mask.name)
                    break
                end
            end
        end
    end
    for _, landmark in ipairs(project.landmarks or {}) do
        report.landmarkCount=report.landmarkCount+1
        if landmark.placed then
            report.placedLandmarkCount=report.placedLandmarkCount+1
            if topology and landmark.vertexIndex and
                    (landmark.vertexIndex<1 or landmark.vertexIndex>topology.vertexCount) then
                report.ok=false
                report.errors[#report.errors + 1]='landmark vertex is outside the current topology: ' .. tostring(landmark.name)
            end
        elseif landmark.required then
            report.ok=false
            report.errors[#report.errors + 1]='required landmark not placed: ' .. tostring(landmark.name)
        end
    end
    return report
end

function M.bindingDescription(project, topology)
    local origin=project.faceOrigin or defaultFaceOrigin()
    local description={meshTopologySignature=project.meshTopologySignature or '',
        vertexCount=topology and topology.vertexCount or project.meshVertexCount or 0,
        faceOrigin={placed=origin.placed == true,position=copyPoint(origin.position),
            vertexIndex=origin.vertexIndex,scale=origin.scale},
        roles={},masks={},landmarks={}}
    for index,role in ipairs(project.roles or {}) do
        if role.enabled then
            description.roles[#description.roles + 1]={index=index,name=role.name,object=role.object or ''}
        end
    end
    for index,mask in ipairs(project.masks or {}) do
        if M.isRigMask(mask) then
            local vertices=type(mask.vertices)=='table' and mask.vertices or {}
            if M.isPolygonMask(mask) and type(mask.polygon)=='table' then
                vertices=type(mask.polygon.derivedVertexWeights)=='table' and
                    mask.polygon.derivedVertexWeights or {}
            end
            description.masks[#description.masks + 1]={index=index,name=mask.name,
                representation=M.maskRepresentation(mask),
                roleIndex=mask.roleIndex or 1,vertexCount=#vertices,
                topologySignature=mask.topologySignature or ''}
        end
    end
    for index,landmark in ipairs(project.landmarks or {}) do
        if landmark.placed then
            description.landmarks[#description.landmarks + 1]={index=index,name=landmark.name,
                side=landmark.side,vertexIndex=landmark.vertexIndex,
                position={x=landmark.position.x,y=landmark.position.y,z=landmark.position.z},
                relativePosition=landmark.relativePosition and copyPoint(landmark.relativePosition) or nil}
        end
    end
    return description
end

-- Produces a mesh-aware, read-only binding review. Object-only roles are verified when the
-- Blender sidecar provides their flattened MSH vertex indices; old sidecars without that map
-- remain unverified. Explicit vertex masks are checked against the loaded topology as well.
function M.bindingReview(project, topology)
    local review={valid=true,roles={},overlapCount=0,totalMaskedVertices=0,
        topologySignature=topology and topology.signature or '',vertexCount=topology and topology.vertexCount or 0}
    local owners,roleVertexSets={},{}
    for roleIndex,role in ipairs(project and project.roles or {}) do
        if role.enabled then
            local item={index=roleIndex,name=role.name or '',object=role.object or '',
                maskCount=0,maskVertexCount=0,objectVertexCount=0,vertexCount=0,vertexIndices={},
                objectVertexVerified=false,mode='unassigned',status='unassigned',issues={}}
            local vertices,roleMaskVertices={},{}
            local sourceMap=project.sourceVertexMap and project.sourceVertexMap[item.object] or {}
            local sourceMapUsable=item.object~='' and #sourceMap>0 and topology and
                project.sourceVertexMapSignature~='' and project.sourceVertexMapSignature==topology.signature
            local sourceMapIssue=item.object~='' and #sourceMap>0 and not sourceMapUsable
            local sourceVertices=sourceMapUsable and sourceMap or {}
            for _,rawIndex in ipairs(sourceVertices) do
                local index=tonumber(rawIndex)
                if not index or index<1 or index%1~=0 or topology and index>topology.vertexCount then
                    item.issues[#item.issues+1]='invalid source vertex map: ' .. tostring(item.object)
                else
                    vertices[index]=true
                end
            end
            for maskIndex,mask in ipairs(project.masks or {}) do
                if M.isRigMask(mask) and mask.enabled ~= false and (mask.roleIndex or 1)==roleIndex then
                    item.maskCount=item.maskCount+1
                    local rawMaskVertices=type(mask.vertices)=='table' and mask.vertices or {}
                    if M.isPolygonMask(mask) and type(mask.polygon)=='table' then
                        rawMaskVertices=mask.polygon.derivedVertexWeights or {}
                    end
                    if topology and (mask.topologySignature or '')~='' and mask.topologySignature~=topology.signature then
                        item.issues[#item.issues+1]='stale mask: ' .. tostring(mask.name or maskIndex)
                    end
                    local seen={}
                    if M.isPolygonMask(mask) and type(mask.polygon)=='table' and topology and
                            (mask.polygon.topologySignature or '')~='' and
                            mask.polygon.topologySignature~=topology.signature then
                        item.issues[#item.issues+1]='stale polygon: ' .. tostring(mask.name or maskIndex)
                    end
                    for _,rawIndex in ipairs(rawMaskVertices) do
                        local index=M.isPolygonMask(mask) and type(rawIndex)=='table' and
                            tonumber(rawIndex.index) or tonumber(rawIndex)
                        if not index or index<1 or index%1~=0 or
                                topology and index>topology.vertexCount or seen[index] then
                            item.issues[#item.issues+1]='invalid mask vertex: ' .. tostring(mask.name or maskIndex)
                        else
                            seen[index]=true
                            -- Keep the serialized mask array untouched. This set is
                            -- only for review accounting and overlap detection.
                            roleMaskVertices[index]=true
                            vertices[index]=true
                        end
                    end
                end
            end
            for index in pairs(roleMaskVertices) do item.maskVertexCount=item.maskVertexCount+1 end
            if sourceMapIssue and item.maskVertexCount==0 then
                item.issues[#item.issues+1]='source vertex map topology is stale or missing'
            end
            for index in pairs(vertices) do
                item.vertexIndices[#item.vertexIndices+1]=index
                if not roleMaskVertices[index] then item.objectVertexCount=item.objectVertexCount+1 end
                if not owners[index] then owners[index]={roleIndex=roleIndex,name=item.name} end
            end
            table.sort(item.vertexIndices)
            item.vertexCount=#item.vertexIndices
            roleVertexSets[roleIndex]=vertices
            if item.object~='' and item.objectVertexCount>0 then item.objectVertexVerified=true end
            if item.object~='' and item.maskVertexCount>0 then
                item.mode='object_and_mask'
            elseif item.object~='' then
                item.mode='object'
            elseif item.maskVertexCount>0 then
                item.mode='mask'
            end
            if #item.issues>0 then
                item.status='invalid'
                review.valid=false
            elseif item.mode=='unassigned' then
                item.status='unassigned'
                if item.name=='main_face' then
                    item.issues[#item.issues+1]='main face has no object or vertex mask'
                    review.valid=false
                end
            elseif item.mode=='object' and item.objectVertexVerified then
                item.status='object_verified'
            elseif item.mode=='object' then
                item.status='object_unverified'
            else
                item.status='mask_ready'
            end
            review.roles[#review.roles+1]=item
        end
    end
    local seenOverlap={}
    for _ in pairs(owners) do review.totalMaskedVertices=review.totalMaskedVertices+1 end
    for roleIndex,vertices in pairs(roleVertexSets) do
        for index in pairs(vertices) do
            local owner=owners[index]
            if owner and owner.roleIndex~=roleIndex and not seenOverlap[index] then
                seenOverlap[index]=true
                review.overlapCount=review.overlapCount+1
                review.valid=false
            end
        end
    end
    return review
end

local function midpoint(a, b)
    if not a and not b then return nil end
    if not a then return copyPoint(b) end
    if not b then return copyPoint(a) end
    return {x=(a.x+b.x)*0.5, y=(a.y+b.y)*0.5, z=(a.z+b.z)*0.5}
end

local function landmarkPoint(project, name)
    for _, landmark in ipairs(project.landmarks or {}) do
        if landmark.name == name and landmark.placed and landmark.position then
            return copyPoint(landmark.position)
        end
    end
    return nil
end

local function appendBone(description, id, parent, head, tail, requiredLandmarks, kind)
    if not head or not tail then return false end
    description.bones[#description.bones + 1] = {
        id=id, name=id, parent=parent, head=head, tail=tail,
        kind=kind or 'facial', landmarks=requiredLandmarks or {}
    }
    return true
end

-- Build the deterministic, review-only rig contract used by the editor preview.
-- Coordinates stay in mesh-local space so a later Blender bridge can consume the
-- same data without depending on viewport state or generated engine objects.
function M.rigDescription(project, topology)
    local origin=project and project.faceOrigin or defaultFaceOrigin()
    local description={version=1, valid=true, errors={}, warnings={}, bones={},
        roleBindings={}, meshTopologySignature=project and project.meshTopologySignature or '',
        vertexCount=topology and topology.vertexCount or project and project.meshVertexCount or 0,
        origin={placed=origin.placed == true, position=copyPoint(origin.position),
            scale=origin.scale, vertexIndex=origin.vertexIndex}}
    local function requirePoint(name, label)
        local point=landmarkPoint(project, name)
        if not point then
            description.valid=false
            description.errors[#description.errors + 1]='rig landmark is missing: ' .. (label or name)
        end
        return point
    end
    if not description.origin.placed then
        description.valid=false
        description.errors[#description.errors + 1]='rig origin is not placed'
    end
    local rootHead=description.origin.position
    local faceTop=landmarkPoint(project,'face_top')
    appendBone(description,'face_root',nil,rootHead,faceTop or
        {x=rootHead.x,y=rootHead.y+math.max(origin.scale or 1,0.1),z=rootHead.z},nil,'root')

    local leftEye=requirePoint('left_eye_center','left_eye_center')
    local rightEye=requirePoint('right_eye_center','right_eye_center')
    local mouthLeft=requirePoint('mouth_left','mouth_left')
    local mouthRight=requirePoint('mouth_right','mouth_right')
    local chin=landmarkPoint(project,'chin')
    local noseTip=landmarkPoint(project,'nose_tip')
    local jawLeft=landmarkPoint(project,'jaw_hinge_left')
    local jawRight=landmarkPoint(project,'jaw_hinge_right')
    local browLeft=landmarkPoint(project,'brow_left')
    local browRight=landmarkPoint(project,'brow_right')
    local leftCorner=landmarkPoint(project,'left_eye_corner')
    local rightCorner=landmarkPoint(project,'right_eye_corner')
    local noseBridge=landmarkPoint(project,'nose_bridge')
    local upperLip=landmarkPoint(project,'upper_lip')
    local lowerLip=landmarkPoint(project,'lower_lip')

    appendBone(description,'eye.L','face_root',leftEye,leftCorner or leftEye,{'left_eye_center'},'eye')
    appendBone(description,'eye.R','face_root',rightEye,rightCorner or rightEye,{'right_eye_center'},'eye')
    appendBone(description,'nose','face_root',noseBridge or noseTip,noseTip,{'nose_tip'},'nose')
    local jawHead=midpoint(jawLeft,jawRight) or midpoint(mouthLeft,mouthRight)
    local hasJaw=appendBone(description,'jaw','face_root',jawHead,chin,{'chin'},'jaw')
    appendBone(description,'mouth',hasJaw and 'jaw' or 'face_root',midpoint(mouthLeft,mouthRight),lowerLip or upperLip or chin,
        {'mouth_left','mouth_right'},'mouth')
    appendBone(description,'brow.L','face_root',browLeft,leftEye,{'brow_left','left_eye_center'},'brow')
    appendBone(description,'brow.R','face_root',browRight,rightEye,{'brow_right','right_eye_center'},'brow')

    for _, role in ipairs(project.roles or {}) do
        if role.enabled then
            local masks={}
            for index, mask in ipairs(project.masks or {}) do
                if M.isRigMask(mask) and mask.enabled and mask.roleIndex == _ then masks[#masks + 1]=index end
            end
            description.roleBindings[#description.roleBindings + 1]={index=_, name=role.name,
                object=role.object or '', maskIndices=masks,
                binding=role.name == 'main_face' and 'facial_skeleton' or 'rigid_role'}
            if role.name == 'main_face' and role.object == '' and #masks == 0 then
                description.warnings[#description.warnings + 1]='main_face has no object or vertex mask'
            end
        end
    end
    if not leftCorner or not rightCorner then
        description.warnings[#description.warnings + 1]='eye corner landmarks are optional; eye bones use their centers'
    end
    if not jawLeft or not jawRight then
        description.warnings[#description.warnings + 1]='jaw hinge landmarks are optional; jaw uses the mouth center'
    end
    if not noseBridge and noseTip then
        description.warnings[#description.warnings + 1]='nose_bridge is optional; nose starts at nose_tip'
    end
    if not noseTip then
        description.warnings[#description.warnings + 1]='nose_tip is optional; the nose bone is omitted'
    end
    if not chin then
        description.warnings[#description.warnings + 1]='chin is optional; the jaw bone is omitted'
    end
    description.landmarkCount=0
    for _, landmark in ipairs(project.landmarks or {}) do
        if landmark.placed then description.landmarkCount=description.landmarkCount+1 end
    end
    return description
end

function M.save(project, path, serializer)
    if type(serializer) ~= 'function' then return false, 'serializer is missing' end
    local lines={'-- mini-mbm facial rig project', 'local facialProject = {}'}
    serializer('facialProject', project, lines)
    lines[#lines + 1]='return facialProject'
    local file, errorMessage = io.open(path, 'w')
    if not file then return false, errorMessage end
    file:write(table.concat(lines, '\n'), '\n')
    file:close()
    return true
end

function M.load(path)
    local chunk, errorMessage = loadfile(path)
    if not chunk then return nil, errorMessage end
    local ok, project = pcall(chunk)
    if not ok then return nil, project end
    return M.normalize(project)
end

return M
