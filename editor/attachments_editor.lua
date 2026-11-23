--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,           |
| and to permit persons to whom the Software is furnished to do so, subject to the following conditions:                |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Attachments Editor

   Independent authoring tool for root-transform, skeletal-bone, and articulated-part attachments.
   Its layout follows the interaction animation editor, but its state and
   saved definition are independent from interaction_animation_editor.lua.
]]--

tImGui = require 'ImGui'
tUtil = require 'editor_utils'
tRuntimeExport = require 'attachments_runtime_export'

local function vector(x, y, z)
    return {x = x or 0, y = y or 0, z = z or 0}
end

local function pose(x, y, z)
    return {x = x or 0, y = y or 0, z = z or 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1}
end

local function clonePose(value)
    return {
        x = value.x or 0, y = value.y or 0, z = value.z or 0,
        rx = value.rx or 0, ry = value.ry or 0, rz = value.rz or 0,
        sx = value.sx or 1, sy = value.sy or 1, sz = value.sz or 1,
    }
end

local function newSpringState()
    return {
        initialized = false,
        position = vector(0, 0, 0),
        rotation = vector(0, 0, 0),
        positionVelocity = vector(0, 0, 0),
        rotationVelocity = vector(0, 0, 0),
    }
end

local function newTargetState()
    return {pose = pose(100, 0, 0), manualOverride = false, kind = 'placeholder',
        path = '', previewSize = 50, meshLoaded = false, selectable = true,
        springState = newSpringState()}
end

local function newDefinition()
    return {
        version = 4,
        source = 'source',
        target = 'target',
        targetSource = 'source',
        sourceType = 'root',
        sourceBone = '',
        sourceSocket = '',
        sourcePart = '',
        aimPoint = vector(0, 0, 0),
        aimAtSource = false,
        influenceEnabled = true,
        initialState = '',
        states = {},
        aimLimits = {
            enabled = false,
            reference = 'dynamic',
            referenceRotationMatrix = nil,
            yawMin = -math.pi,
            yawMax = math.pi,
            pitchMin = -math.pi * 0.5,
            pitchMax = math.pi * 0.5,
        },
        spring = {
            enabled = false,
            position = true,
            rotation = true,
            stiffness = 120,
            damping = 24,
        },
        blend = {
            enabled = false,
            position = true,
            rotation = true,
            scale = true,
            weight = 1,
        },
        inherit = {position = true, rotation = true, scale = false},
        offset = {
            position = vector(100, 0, 0),
            rotation = vector(0, 0, 0),
            rotationMatrix = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
            scale = vector(1, 1, 1),
        },
        targetPivot = vector(0, 0, 0),
        targets = {},
        editor = {
            sourceKind = 'placeholder',
            targetKind = 'placeholder',
            sourceMesh = '',
            targetMesh = '',
            sourcePose = pose(0, 0, 0),
            targetPose = pose(100, 0, 0),
            targetManualOverride = false,
            language = nil,
            sourceSelectable = true,
            targetSelectable = true,
            sourceAnimation = '',
            sourceAnimationPaused = false,
        },
    }
end

local S = {
    definition = newDefinition(),
    projectPath = '',
    status = '',
    modified = false,
    dirtyPreview = true,
    selectedRole = 'target',
    transformMode = 'move',
    source = {pose = pose(0, 0, 0), object = nil, kind = 'placeholder', path = '',
        previewSize = 70, meshLoaded = false, selectable = true, debugMesh = nil,
        skeletonReport = nil, meshBounds = nil, boneVisuals = {}, selectedBone = '', skeletonGeneration = 0,
        socketCatalog = {},
        articulatedPartCatalog = {},
        articulatedPartPickGeometry = nil,
        animationClips = {}, selectedAnimation = 1, activeAnimation = '', animationPaused = false,
        selectionAnimation = nil},
    targets = {newTargetState()},
    selectedTargetIndex = 1,
    target = nil,
    relationLine = nil,
    relationLines = {},
    targetPivotMarker = nil,
    targetPivotMarkerRadius = nil,
    targetPivotMarkerSignature = nil,
    partSelectionMarker = nil,
    aimPointMarker = nil,
    aimLimitsVisual = {objects = {}, signature = nil},
    springState = {
        initialized = false,
        position = vector(0, 0, 0),
        rotation = vector(0, 0, 0),
        positionVelocity = vector(0, 0, 0),
        rotationVelocity = vector(0, 0, 0),
    },
    originLines = {x = nil, y = nil, z = nil},
    xzPlane = nil,
    xzPlaneSize = nil,
    xzPlaneX = nil,
    xzPlaneZ = nil,
    showOrigins = true,
    showXZPlane = true,
    showAimLimits = false,
    showStates = false,
    showGraph = false,
    graphLayout = {root = nil, targets = {}},
    chainStatus = '',
    boneSelectionMode = false,
    boneSelectionCamera = nil,
    boneSelectionTargetVisible = nil,
    partSelectionMode = false,
    partSelectionCamera = nil,
    partSelectionTargetVisible = nil,
    camera = nil,
    cameraOrbit = {azimuth = 0.3, elevation = 0.3, distance = 700, fx = 50, fy = 30, fz = 0},
    cameraDrag = nil,
    gizmo = {axes = {}, rings = {}, origin = nil, length = nil, radius = nil,
        axesSignature = nil, drag = nil},
    windows = {objects = '', attachment = '', camera = '', timeline = '', states = '', graph = ''},
    stateDraft = '',
}

S.target = S.targets[1]

local L
local refreshWindowTitles

local function setStatus(message, warning)
    S.status = tostring(message or '')
    if warning then tUtil.showMessageWarn(S.status, 6) else tUtil.showMessage(S.status, 6) end
end

local function showTooltip(message)
    if not message or message == '' or not tImGui.IsItemHovered(0) then return end
    if not tImGui.BeginTooltip() then return end
    tImGui.PushTextWrapPos(420)
    tImGui.Text(message)
    tImGui.PopTextWrapPos()
    tImGui.EndTooltip()
end

local function isSourceSelectionMode()
    return S.boneSelectionMode or S.partSelectionMode
end

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then
        setStatus(result[2], true)
        print('[attachments_editor] ' .. S.status)
    end
    return table.unpack(result, 1, result.n)
end

local function identityRotation()
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}
end

local function cloneRotation(matrix)
    return {
        {matrix[1][1], matrix[1][2], matrix[1][3]},
        {matrix[2][1], matrix[2][2], matrix[2][3]},
        {matrix[3][1], matrix[3][2], matrix[3][3]},
    }
end

local function readRotation(value, fallback)
    if type(value) ~= 'table' then return cloneRotation(fallback) end
    local result = {}
    for row = 1, 3 do
        if type(value[row]) ~= 'table' then return cloneRotation(fallback) end
        result[row] = {}
        for column = 1, 3 do
            local number = tonumber(value[row][column])
            if not number then return cloneRotation(fallback) end
            result[row][column] = number
        end
    end
    return result
end

local function multiplyRotation(a, b)
    local result = {}
    for row = 1, 3 do
        result[row] = {}
        for column = 1, 3 do
            result[row][column] = a[row][1] * b[1][column] +
                a[row][2] * b[2][column] + a[row][3] * b[3][column]
        end
    end
    return result
end

local function transposeRotation(matrix)
    return {
        {matrix[1][1], matrix[2][1], matrix[3][1]},
        {matrix[1][2], matrix[2][2], matrix[3][2]},
        {matrix[1][3], matrix[2][3], matrix[3][3]},
    }
end

local function axisRotation(axis, angle)
    local x, y, z = axis.x, axis.y, axis.z
    local cosine, sine = math.cos(angle), math.sin(angle)
    local complement = 1 - cosine
    return {
        {complement * x * x + cosine, complement * x * y + sine * z, complement * x * z - sine * y},
        {complement * y * x - sine * z, complement * y * y + cosine, complement * y * z + sine * x},
        {complement * z * x + sine * y, complement * z * y - sine * x, complement * z * z + cosine},
    }
end

local function eulerRotation(rx, ry, rz)
    local xRotation = axisRotation({x = 1, y = 0, z = 0}, rx)
    local yRotation = axisRotation({x = 0, y = 1, z = 0}, ry)
    local zRotation = axisRotation({x = 0, y = 0, z = 1}, rz)
    return multiplyRotation(zRotation, multiplyRotation(yRotation, xRotation))
end

local function matrixToEuler(matrix)
    local y = math.asin(math.max(-1, math.min(1, matrix[3][1])))
    local x, z
    if math.abs(matrix[3][1]) > 0.999999 then
        x = 0
        z = math.atan(matrix[1][2], matrix[2][2])
    else
        x = math.atan(-matrix[3][2], matrix[3][3])
        z = math.atan(-matrix[2][1], matrix[1][1])
    end
    return x, y, z
end

local rotateVector

local function lookAtRotation(from, to, fallback)
    local fx, fy, fz = to.x - from.x, to.y - from.y, to.z - from.z
    local length = math.sqrt(fx * fx + fy * fy + fz * fz)
    if length <= 1e-6 then return fallback end
    fx, fy, fz = fx / length, fy / length, fz / length
    local ux, uy, uz = 0, 1, 0
    if math.abs(fy) > 0.999 then ux, uy, uz = 1, 0, 0 end
    local rx, ry, rz = uy * fz - uz * fy, uz * fx - ux * fz, ux * fy - uy * fx
    length = math.sqrt(rx * rx + ry * ry + rz * rz)
    if length <= 1e-6 then return fallback end
    rx, ry, rz = rx / length, ry / length, rz / length
    ux, uy, uz = fy * rz - fz * ry, fz * rx - fx * rz, fx * ry - fy * rx
    return {{rx, ry, rz}, {ux, uy, uz}, {fx, fy, fz}}
end

local function limitedLookAtRotation(from, to, baseRotation, fallback, limits)
    if not limits or not limits.enabled then
        return lookAtRotation(from, to, fallback)
    end
    local fx, fy, fz = to.x - from.x, to.y - from.y, to.z - from.z
    local length = math.sqrt(fx * fx + fy * fy + fz * fz)
    if length <= 1e-6 then return fallback end
    fx, fy, fz = fx / length, fy / length, fz / length
    local localForward = rotateVector(transposeRotation(baseRotation), vector(fx, fy, fz))
    length = math.sqrt(localForward.x * localForward.x + localForward.y * localForward.y +
        localForward.z * localForward.z)
    if length <= 1e-6 then return fallback end
    localForward.x, localForward.y, localForward.z = localForward.x / length,
        localForward.y / length, localForward.z / length
    local yaw = math.atan(localForward.x, localForward.z)
    local pitch = math.atan(localForward.y,
        math.sqrt(localForward.x * localForward.x + localForward.z * localForward.z))
    local yawMin, yawMax = math.min(limits.yawMin, limits.yawMax), math.max(limits.yawMin, limits.yawMax)
    local pitchMin, pitchMax = math.min(limits.pitchMin, limits.pitchMax),
        math.max(limits.pitchMin, limits.pitchMax)
    yaw = math.max(yawMin, math.min(yawMax, yaw))
    pitch = math.max(pitchMin, math.min(pitchMax, pitch))
    local cosinePitch = math.cos(pitch)
    localForward = vector(cosinePitch * math.sin(yaw), math.sin(pitch), cosinePitch * math.cos(yaw))
    local ux, uy, uz = 0, 1, 0
    if math.abs(localForward.y) > 0.999 then ux, uy, uz = 1, 0, 0 end
    local rx, ry, rz = uy * localForward.z - uz * localForward.y,
        uz * localForward.x - ux * localForward.z, ux * localForward.y - uy * localForward.x
    length = math.sqrt(rx * rx + ry * ry + rz * rz)
    if length <= 1e-6 then return fallback end
    rx, ry, rz = rx / length, ry / length, rz / length
    ux, uy, uz = localForward.y * rz - localForward.z * ry,
        localForward.z * rx - localForward.x * rz, localForward.x * ry - localForward.y * rx
    return multiplyRotation({{rx, ry, rz}, {ux, uy, uz},
        {localForward.x, localForward.y, localForward.z}}, baseRotation)
end

local function unwrapAngle(value, reference)
    if reference == nil then return value end
    local turns = math.floor((reference - value) / (math.pi * 2) + 0.5)
    return value + turns * math.pi * 2
end

rotateVector = function(matrix, value)
    return vector(
        matrix[1][1] * value.x + matrix[2][1] * value.y + matrix[3][1] * value.z,
        matrix[1][2] * value.x + matrix[2][2] * value.y + matrix[3][2] * value.z,
        matrix[1][3] * value.x + matrix[2][3] * value.y + matrix[3][3] * value.z)
end

local function roleEntry(role)
    return role == 'source' and S.source or S.target
end

local function ensureRelationLine(index)
    local relation = S.relationLines[index]
    if relation then return relation end
    relation = line:new('3d', 0, 0, 0)
    relation:add({0, 0, 0, 100, 0, 0})
    relation:setColor(0.85, 0.85, 0.85, 1)
    relation.alwaysOnTop = true
    relation.alwaysOnTopPriority = 2
    relation.alwaysRender = true
    S.relationLines[index] = relation
    if index == 1 then S.relationLine = relation end
    return relation
end

local cloneBlend
local markModified
local getPreviewSourcePose

local function copyVector(value, fallback)
    value = value or fallback or vector(0, 0, 0)
    return vector(value.x or 0, value.y or 0, value.z or 0)
end

local function copyTargetRecord(record)
    local inherit = record.inherit or {}
    local result = {
        name = record.name or 'target',
        source = record.source or 'source',
        sourceBone = record.sourceBone or '',
        sourceSocket = record.sourceSocket or '',
        sourcePart = record.sourcePart or '',
        inherit = {
            position = inherit.position ~= false,
            rotation = inherit.rotation ~= false,
            scale = inherit.scale == true,
        },
        aimAtSource = record.aimAtSource == true,
        aimPoint = copyVector(record.aimPoint),
        aimLimits = {
            enabled = record.aimLimits and record.aimLimits.enabled == true or false,
            reference = record.aimLimits and record.aimLimits.reference == 'captured' and 'captured' or 'dynamic',
            referenceRotationMatrix = record.aimLimits and record.aimLimits.referenceRotationMatrix and
                cloneRotation(record.aimLimits.referenceRotationMatrix) or nil,
            yawMin = record.aimLimits and record.aimLimits.yawMin or -math.pi,
            yawMax = record.aimLimits and record.aimLimits.yawMax or math.pi,
            pitchMin = record.aimLimits and record.aimLimits.pitchMin or -math.pi * 0.5,
            pitchMax = record.aimLimits and record.aimLimits.pitchMax or math.pi * 0.5,
        },
        spring = {
            enabled = record.spring and record.spring.enabled == true or false,
            position = not record.spring or record.spring.position ~= false,
            rotation = not record.spring or record.spring.rotation ~= false,
            stiffness = record.spring and record.spring.stiffness or 120,
            damping = record.spring and record.spring.damping or 24,
        },
        blend = cloneBlend(record.blend),
        offset = {
            position = copyVector(record.offset and record.offset.position),
            rotation = copyVector(record.offset and record.offset.rotation),
            rotationMatrix = record.offset and record.offset.rotationMatrix and
                cloneRotation(record.offset.rotationMatrix) or identityRotation(),
            scale = copyVector(record.offset and record.offset.scale, vector(1, 1, 1)),
        },
        targetPivot = copyVector(record.targetPivot),
        editor = {},
    }
    result.editor.targetKind = record.editor and record.editor.targetKind or 'placeholder'
    result.editor.targetMesh = record.editor and record.editor.targetMesh or ''
    result.editor.targetPose = record.editor and clonePose(record.editor.targetPose or pose(100, 0, 0)) or
        pose(100, 0, 0)
    result.editor.targetManualOverride = record.editor and record.editor.targetManualOverride == true or false
    result.editor.targetSelectable = not record.editor or record.editor.targetSelectable ~= false
    return result
end

local function snapshotActiveTarget()
    local record = {
        name = S.definition.target,
        source = S.definition.targetSource or S.definition.source,
        sourceBone = S.definition.sourceBone,
        sourceSocket = S.definition.sourceSocket,
        sourcePart = S.definition.sourcePart,
        inherit = S.definition.inherit,
        aimAtSource = S.definition.aimAtSource,
        aimPoint = S.definition.aimPoint,
        aimLimits = S.definition.aimLimits,
        spring = S.definition.spring,
        blend = S.definition.blend,
        offset = S.definition.offset,
        targetPivot = S.definition.targetPivot,
        editor = {
            targetKind = S.target.kind,
            targetMesh = S.target.path,
            targetPose = clonePose(S.target.pose),
            targetManualOverride = S.target.manualOverride == true,
            targetSelectable = S.target.selectable ~= false,
        },
    }
    return copyTargetRecord(record)
end

local function ensureTargetDefinitions()
    if type(S.definition.targets) ~= 'table' then S.definition.targets = {} end
    if #S.definition.targets == 0 then
        S.definition.targets[1] = snapshotActiveTarget()
    end
    while #S.targets < #S.definition.targets do S.targets[#S.targets + 1] = newTargetState() end
    while #S.targets > #S.definition.targets do
        local removed = table.remove(S.targets)
        if removed.object then removed.object:destroy() end
    end
    if S.selectedTargetIndex > #S.definition.targets then S.selectedTargetIndex = 1 end
end

local function syncActiveTargetDefinition()
    ensureTargetDefinitions()
    local record = S.definition.targets[S.selectedTargetIndex]
    if not record then return end
    local snapshot = snapshotActiveTarget()
    S.definition.targets[S.selectedTargetIndex] = snapshot
end

local function activateTargetDefinition(index)
    ensureTargetDefinitions()
    local record = S.definition.targets[index]
    if not record then return false end
    S.selectedTargetIndex = index
    S.target = S.targets[index]
    S.springState = S.target.springState or newSpringState()
    S.target.springState = S.springState
    S.definition.target = record.name
    S.definition.targetSource = record.source or S.definition.source
    S.definition.sourceBone = record.sourceBone or ''
    S.definition.sourceSocket = record.sourceSocket or ''
    S.definition.sourcePart = record.sourcePart or ''
    S.definition.inherit = record.inherit
    S.definition.aimAtSource = record.aimAtSource
    S.definition.aimPoint = record.aimPoint
    S.definition.aimLimits = record.aimLimits
    S.definition.spring = record.spring
    S.definition.blend = record.blend
    S.definition.offset = record.offset
    S.definition.targetPivot = record.targetPivot
    S.target.pose = clonePose(record.editor.targetPose)
    S.target.manualOverride = record.editor.targetManualOverride == true
    S.target.selectable = record.editor.targetSelectable ~= false
    return true
end

local function selectTarget(index)
    if index == S.selectedTargetIndex then return true end
    syncActiveTargetDefinition()
    if not activateTargetDefinition(index) then return false end
    S.selectedRole = 'target'
    S.transformMode = 'move'
    S.gizmo.drag = nil
    S.targetPivotMarkerRadius, S.targetPivotMarkerSignature = nil, nil
    S.dirtyPreview = true
    return true
end

local function targetIndexByName(name)
    for index, record in ipairs(S.definition.targets or {}) do
        if record.name == name then return index end
    end
    return nil
end

local function validateAttachmentGraph()
    local graph = {
        valid = true,
        errorKey = nil,
        nodes = {},
        edges = {},
        targetIndexes = {},
    }
    local sourceName = tostring(S.definition.source or '')
    if sourceName == '' then
        graph.valid, graph.errorKey = false, 'target_source_invalid'
        return graph
    end
    graph.nodes[#graph.nodes + 1] = {name = sourceName, root = true}
    local names = {[sourceName] = true}
    for index, record in ipairs(S.definition.targets or {}) do
        local name = tostring(record.name or '')
        if name == '' or names[name] then
            graph.valid, graph.errorKey = false, 'target_name_invalid'
        else
            names[name] = true
            graph.targetIndexes[name] = index
            graph.nodes[#graph.nodes + 1] = {name = name, index = index}
        end
    end
    for index, record in ipairs(S.definition.targets or {}) do
        local targetName = tostring(record.name or '')
        local parentName = tostring(record.source or sourceName)
        graph.edges[#graph.edges + 1] = {source = parentName, target = targetName, index = index}
        if parentName == '' or not names[parentName] or parentName == targetName then
            graph.valid, graph.errorKey = false, 'target_source_invalid'
        end
    end
    local visiting, visited = {}, {}
    local function visit(name)
        if name == sourceName then return true end
        if visiting[name] then
            graph.valid, graph.errorKey = false, 'target_cycle'
            return false
        end
        if visited[name] then return true end
        local index = graph.targetIndexes[name]
        if not index then
            graph.valid, graph.errorKey = false, 'target_source_invalid'
            return false
        end
        visiting[name] = true
        local parentName = tostring(S.definition.targets[index].source or sourceName)
        local result = visit(parentName)
        visiting[name] = nil
        if result then visited[name] = true end
        return result
    end
    for name in pairs(graph.targetIndexes) do visit(name) end
    return graph
end

local function getActiveTargetSourcePose()
    local sourceName = S.definition.targetSource or S.definition.source
    if sourceName == S.definition.source then
        return getPreviewSourcePose() or clonePose(S.source.pose)
    end
    local sourceIndex = targetIndexByName(sourceName)
    if not sourceIndex or not S.targets[sourceIndex] then return nil end
    return clonePose(S.targets[sourceIndex].pose)
end

local function addTarget()
    syncActiveTargetDefinition()
    local index = #S.definition.targets + 1
    local name = 'target_' .. index
    local used = {}
    for _, record in ipairs(S.definition.targets) do used[record.name] = true end
    while used[name] do
        index = index + 1
        name = 'target_' .. index
    end
    local record = snapshotActiveTarget()
    record.name = name
    record.editor.targetPose = clonePose(newTargetState().pose)
    record.editor.targetManualOverride = false
    record.editor.targetKind = 'placeholder'
    record.editor.targetMesh = ''
    S.definition.targets[#S.definition.targets + 1] = record
    S.targets[#S.targets + 1] = newTargetState()
    S.selectedTargetIndex = #S.definition.targets
    activateTargetDefinition(S.selectedTargetIndex)
    S.selectedRole = 'target'
    markModified()
    setStatus(L('target_added'))
end

local function removeTarget()
    if #S.definition.targets <= 1 then
        setStatus(L('target_remove_last'), true)
        return
    end
    syncActiveTargetDefinition()
    local index = S.selectedTargetIndex
    local removedState = table.remove(S.targets, index)
    if removedState and removedState.object then removedState.object:destroy() end
    table.remove(S.definition.targets, index)
    table.remove(S.graphLayout.targets, index)
    if index > #S.definition.targets then index = #S.definition.targets end
    S.selectedTargetIndex = index
    activateTargetDefinition(index)
    S.targetPivotMarkerRadius, S.targetPivotMarkerSignature = nil, nil
    markModified()
    setStatus(L('target_removed'))
end

local function setSourceBindingName(name)
    local oldName = S.definition.source
    S.definition.source = name
    for _, record in ipairs(S.definition.targets) do
        if record.source == oldName then record.source = name end
    end
    if S.definition.targetSource == oldName then S.definition.targetSource = name end
    markModified()
end

local function setTargetBindingName(name)
    local oldName = S.definition.target
    S.definition.target = name
    local record = S.definition.targets[S.selectedTargetIndex]
    if record then
        record.name = name
        for index, other in ipairs(S.definition.targets) do
            if index ~= S.selectedTargetIndex and other.source == oldName then other.source = name end
        end
    end
    markModified()
end

local function destroySourceSkeletonVisuals()
    for _, visual in pairs(S.source.boneVisuals) do
        if visual.object then visual.object:destroy() end
        if visual.joint then visual.joint:destroy() end
    end
    S.source.boneVisuals = {}
end

local function updateSourceSkeletonVisibility()
    local visible = S.definition.sourceType == 'bone' and S.boneSelectionMode
    for _, visual in pairs(S.source.boneVisuals) do
        if visual.object then visual.object.visible = visible end
        if visual.joint then visual.joint.visible = visible end
    end
end

local prepareSourceSkeleton
local updateSourceSkeletonVisuals
local setBoneSelectionMode
local hideGizmo
local targetPivotWorldPosition
local sourceAimPointWorldPosition
local updateAimPointMarker
local updateTargetPivotMarker
local updatePartSelectionMarker
local showTargetPivotFields
local showAimPointFields
local captureOffset
local captureAimReference

local function roleName(role)
    return role == 'source' and S.definition.source or S.definition.target
end

markModified = function()
    S.modified = true
    S.dirtyPreview = true
end

local function resetSpringState()
    S.springState.initialized = false
    S.springState.positionVelocity = vector(0, 0, 0)
    S.springState.rotationVelocity = vector(0, 0, 0)
end

local function springScalar(current, velocity, target, stiffness, damping, delta)
    local acceleration = stiffness * (target - current) - damping * velocity
    velocity = velocity + acceleration * delta
    return current + velocity * delta, velocity
end

local function springAngle(current, velocity, target, stiffness, damping, delta)
    local difference = math.atan(math.sin(target - current), math.cos(target - current))
    local next, nextVelocity = springScalar(current, velocity, current + difference,
        stiffness, damping, delta)
    return next, nextVelocity
end

local function blendScalar(current, target, weight)
    return current + (target - current) * weight
end

local function blendAngle(current, target, weight)
    local difference = math.atan(math.sin(target - current), math.cos(target - current))
    return current + difference * weight
end

cloneBlend = function(value)
    value = value or {}
    return {
        enabled = value.enabled == true,
        position = value.position ~= false,
        rotation = value.rotation ~= false,
        scale = value.scale ~= false,
        weight = math.min(1, math.max(0, tonumber(value.weight) or 1)),
    }
end

local function activeStateProfile()
    local name = S.definition.initialState
    if type(name) ~= 'string' or name == '' or type(S.definition.states) ~= 'table' then return nil end
    return S.definition.states[name]
end

local function previewBlend()
    local profile = activeStateProfile()
    return profile and profile.blend or S.definition.blend
end

local function previewInfluenceEnabled()
    local profile = activeStateProfile()
    return profile and profile.influenceEnabled ~= false or S.definition.influenceEnabled ~= false
end

local function ensureStateBlend(profile)
    if not profile.blend then profile.blend = cloneBlend(S.definition.blend) end
    return profile.blend
end

local function stateNames()
    local names = {}
    for name in pairs(S.definition.states or {}) do names[#names + 1] = name end
    table.sort(names)
    return names
end

local function selectAttachmentState(name)
    if name ~= '' and not S.definition.states[name] then return false end
    S.definition.initialState = name
    S.stateDraft = name
    S.target.manualOverride = false
    resetSpringState()
    markModified()
    return true
end

local function cameraPosition()
    local c = S.cameraOrbit
    return c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth),
        c.fy + c.distance * math.sin(c.elevation),
        c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
end

local function applyCamera()
    if not S.camera then return end
    local x, y, z = cameraPosition()
    S.camera:setPos(x, y, z)
    S.camera:setFocus(S.cameraOrbit.fx, S.cameraOrbit.fy, S.cameraOrbit.fz)
end

local function resetCamera()
    S.cameraOrbit = {azimuth = 0.3, elevation = 0.3, distance = 700, fx = 50, fy = 30, fz = 0}
    applyCamera()
    setStatus(L('camera_reset'))
end

local function cubeVertices()
    local h = 0.5
    local corners = {
        {-h, -h, h}, {-h, h, h}, {h, h, h}, {h, -h, h},
        {-h, -h, -h}, {-h, h, -h}, {h, h, -h}, {h, -h, -h},
    }
    local faces = {
        {1, 2, 3}, {1, 3, 4}, {8, 7, 6}, {8, 6, 5},
        {5, 6, 2}, {5, 2, 1}, {4, 3, 7}, {4, 7, 8},
        {2, 6, 7}, {2, 7, 3}, {5, 1, 4}, {5, 4, 8},
    }
    local vertices = {}
    for _, face in ipairs(faces) do
        for _, index in ipairs(face) do
            local corner = corners[index]
            vertices[#vertices + 1] = corner[1]
            vertices[#vertices + 1] = corner[2]
            vertices[#vertices + 1] = corner[3]
        end
    end
    return vertices
end

local function createMarker(role, color)
    local object = shape:new('3d', 0, 0, 0)
    if not object:create(cubeVertices(), nil, 'attachments_' .. role .. '_marker') then
        object:destroy()
        return nil
    end
    object:setColor(table.unpack(color))
    object.alwaysOnTop = true
    object.alwaysOnTopPriority = role == 'source' and 3 or 4
    object.alwaysRender = true
    return object
end

local function directoryOf(path)
    return type(path) == 'string' and path:match('^(.*)[/\\]') or ''
end

local function destroyRoleObject(role)
    local entry = roleEntry(role)
    if role == 'source' then destroySourceSkeletonVisuals() end
    if entry.object then entry.object:destroy(); entry.object = nil end
    if entry.debugMesh then entry.debugMesh:destroy(); entry.debugMesh = nil end
    entry.meshLoaded = false
    entry.skeletonReport = nil
    entry.meshBounds = nil
    entry.articulatedPartPickGeometry = nil
    entry.selectedBone = ''
    entry.socketCatalog = {}
    entry.articulatedPartCatalog = {}
    if role == 'source' then
        entry.animationClips = {}
        entry.selectedAnimation = 1
        entry.activeAnimation = ''
        entry.animationPaused = false
        entry.selectionAnimation = nil
    end
end

local function refreshSourceAnimationCatalog()
    local source = S.source
    source.animationClips = {}
    source.selectedAnimation = 1
    source.activeAnimation = ''
    source.animationPaused = false
    if not source.object or not source.meshLoaded then return end
    local articulated = S.definition.sourceType == 'articulated_part'
    local okTotal, total = dpCall(function()
        return articulated and source.object:getTotalArticulatedAnimations() or
            source.object:getTotalSkeletalAnimations()
    end)
    if not okTotal then return end
    for index = 1, tonumber(total) or 0 do
        local okName, name = dpCall(function()
            return articulated and source.object:getArticulatedAnimationName(index) or
                source.object:getSkeletalAnimationName(index)
        end)
        if okName and type(name) == 'string' and name ~= '' then
            source.animationClips[#source.animationClips + 1] = name
        end
    end
end

local function refreshSourceArticulatedPartCatalog()
    local source = S.source
    source.articulatedPartCatalog = {}
    if not source.object or not source.meshLoaded then return end
    local okTotal, total = dpCall(function() return source.object:getTotalArticulatedParts() end)
    if not okTotal then return end
    local seen = {}
    for index = 1, tonumber(total) or 0 do
        local okInfo, info = dpCall(function() return source.object:getArticulatedPartInfo(index) end)
        if okInfo and type(info) == 'table' and type(info.name) == 'string' and info.name ~= '' and
                not seen[info.name] then
            seen[info.name] = true
            source.articulatedPartCatalog[#source.articulatedPartCatalog + 1] = info.name
        end
    end
end

local function sourceUsesArticulatedAnimation()
    return S.definition.sourceType == 'articulated_part'
end

local function refreshSourceSocketCatalog()
    local source = S.source
    source.socketCatalog = {}
    if not source.object or not source.meshLoaded then return end
    local ok, report = dpCall(function() return source.object:getSkeletalSocketReport() end)
    if not ok or type(report) ~= 'table' then return end
    for _, item in ipairs(report) do
        if type(item) == 'table' and type(item.name) == 'string' and item.name ~= '' then
            source.socketCatalog[#source.socketCatalog + 1] = item.name
        end
    end
end

local function restoreSourceAnimation()
    local source = S.source
    local saved = source.selectionAnimation
    source.selectionAnimation = nil
    if not saved or saved.name == '' or not source.object then return end
    local ok, played = dpCall(function()
        return sourceUsesArticulatedAnimation() and source.object:playArticulatedAnimation(saved.name) or
            source.object:playSkeletalAnimation(saved.name)
    end)
    if not ok or played == false then return end
    source.activeAnimation = saved.name
    source.animationPaused = saved.paused == true
    if source.animationPaused then
        dpCall(function()
            return sourceUsesArticulatedAnimation() and source.object:pauseArticulatedAnimation(saved.name) or
                source.object:pauseSkeletalAnimation()
        end)
    end
    S.dirtyPreview = true
end

local function playSourceAnimation()
    local source = S.source
    if isSourceSelectionMode() then
        setStatus(L('animation_unavailable_in_selection_mode'), true)
        return
    end
    local name = source.animationClips[source.selectedAnimation]
    if not source.object or not name then return end
    if source.activeAnimation == name and source.animationPaused then
        resumeSourceAnimation()
        return
    end
    local ok, played = dpCall(function()
        return sourceUsesArticulatedAnimation() and source.object:playArticulatedAnimation(name) or
            source.object:playSkeletalAnimation(name)
    end)
    if not ok or played == false then
        setStatus(L('animation_play_failed'), true)
        return
    end
    source.activeAnimation = name
    source.animationPaused = false
    S.dirtyPreview = true
end

local function pauseSourceAnimation()
    if not S.source.object or S.source.activeAnimation == '' then return end
    local name = S.source.activeAnimation
    local ok, paused = dpCall(function()
        return sourceUsesArticulatedAnimation() and S.source.object:pauseArticulatedAnimation(name) or
            S.source.object:pauseSkeletalAnimation()
    end)
    if ok and paused ~= false then
        S.source.animationPaused = true
        S.dirtyPreview = true
    end
end

local function resumeSourceAnimation()
    if not S.source.object or S.source.activeAnimation == '' then return end
    local name = S.source.activeAnimation
    local ok, resumed = dpCall(function()
        return sourceUsesArticulatedAnimation() and S.source.object:resumeArticulatedAnimation(name) or
            S.source.object:resumeSkeletalAnimation()
    end)
    if ok and resumed ~= false then
        S.source.animationPaused = false
        S.dirtyPreview = true
    end
end

local function stopSourceAnimation()
    if not S.source.object then return end
    if sourceUsesArticulatedAnimation() then
        dpCall(function() return S.source.object:disableArticulatedAnimation(S.source.activeAnimation) end)
    else
        dpCall(function() return S.source.object:stopSkeletalAnimation() end)
    end
    S.source.activeAnimation = ''
    S.source.animationPaused = false
    S.dirtyPreview = true
end

local function sourceAnimationDuration()
    local source = S.source
    if not source.object or #source.animationClips == 0 then return 0 end
    local ok, duration = dpCall(function()
        if sourceUsesArticulatedAnimation() then
            local name = source.animationClips[source.selectedAnimation]
            return name and source.object:getArticulatedAnimationDuration(name) or 0
        end
        return source.object:getSkeletalAnimationDuration(source.selectedAnimation)
    end)
    return ok and math.max(0, tonumber(duration) or 0) or 0
end

local function seekSourceAnimation(time)
    local source = S.source
    local name = source.animationClips[source.selectedAnimation]
    if not source.object or not name then return end
    if source.activeAnimation ~= name then
        playSourceAnimation()
        if source.activeAnimation ~= name then return end
    end
    local name = source.activeAnimation
    dpCall(function()
        return sourceUsesArticulatedAnimation() and source.object:pauseArticulatedAnimation(name) or
            source.object:pauseSkeletalAnimation()
    end)
    local ok, seeked = dpCall(function()
        return sourceUsesArticulatedAnimation() and source.object:seekArticulatedAnimation(
            name, math.max(0, time or 0)) or
            source.object:seekSkeletalAnimation(math.max(0, time or 0))
    end)
    if ok and seeked ~= false then
        source.animationPaused = true
        S.dirtyPreview = true
    end
end

local function ensureRolePlaceholder(role)
    local entry = roleEntry(role)
    if entry.object then return end
    local color = role == 'source' and {0.15, 0.75, 1.0, 0.42} or {1.0, 0.65, 0.15, 0.42}
    entry.object = createMarker(role, color)
    entry.meshLoaded = false
end

local function setRoleKind(role, kind)
    local entry = roleEntry(role)
    if kind ~= 'mesh' and kind ~= 'placeholder' then return end
    entry.kind = kind
    if kind == 'placeholder' then
        entry.path = ''
        destroyRoleObject(role)
        ensureRolePlaceholder(role)
        markModified()
        return
    end
    if entry.meshLoaded then destroyRoleObject(role) end
    entry.path = ''
    ensureRolePlaceholder(role)
    markModified()
end

local function loadRoleMesh(role, path)
    local entry = roleEntry(role)
    if not path or path == '' then return false end
    local ok, object = dpCall(function()
        local folder = directoryOf(path)
        if folder ~= '' then mbm.addPath(folder) end
        local loaded = mesh:new('3d')
        if not loaded:load(path) then
            loaded:destroy()
            error('Could not load attachment mesh: ' .. tostring(path))
        end
        return loaded
    end)
    if not ok or not object then return false end
    destroyRoleObject(role)
    entry.kind = 'mesh'
    entry.path = path
    entry.object = object
    entry.meshLoaded = true
    object.alwaysRender = true
    object.visible = true
    if role == 'source' then
        dpCall(function() return object:stopSkeletalAnimation() end)
        local okTotal, total = dpCall(function() return object:getTotalArticulatedAnimations() end)
        for index = 1, (okTotal and tonumber(total) or 0) do
            local okName, name = dpCall(function() return object:getArticulatedAnimationName(index) end)
            if okName and name and name ~= '' then
                dpCall(function() return object:disableArticulatedAnimation(name) end)
            end
        end
        refreshSourceAnimationCatalog()
        refreshSourceSocketCatalog()
        refreshSourceArticulatedPartCatalog()
        if S.definition.sourceType == 'bone' then prepareSourceSkeleton() end
    end
    markModified()
    setStatus(string.format(L('mesh_loaded'), roleName(role), path))
    return true
end

local function matrixPoint(matrix)
    return vector(matrix[13] or 0, matrix[14] or 0, matrix[15] or 0)
end

local function matrixPose(matrix)
    if type(matrix) ~= 'table' then return nil end
    local x1, x2, x3 = matrix[1] or 1, matrix[2] or 0, matrix[3] or 0
    local y1, y2, y3 = matrix[5] or 0, matrix[6] or 1, matrix[7] or 0
    local z1, z2, z3 = matrix[9] or 0, matrix[10] or 0, matrix[11] or 1
    local sx = math.sqrt(x1 * x1 + x2 * x2 + x3 * x3)
    local sy = math.sqrt(y1 * y1 + y2 * y2 + y3 * y3)
    local sz = math.sqrt(z1 * z1 + z2 * z2 + z3 * z3)
    if sx <= 1e-8 or sy <= 1e-8 or sz <= 1e-8 then return nil end
    local rotation = {
        {x1 / sx, x2 / sx, x3 / sx},
        {y1 / sy, y2 / sy, y3 / sy},
        {z1 / sz, z2 / sz, z3 / sz},
    }
    local rx, ry, rz = matrixToEuler(rotation)
    local position = matrixPoint(matrix)
    return {
        x = position.x, y = position.y, z = position.z,
        rx = rx, ry = ry, rz = rz, sx = sx, sy = sy, sz = sz,
        rotation = rotation,
    }
end

local function transformSourcePoint(point)
    local source = S.source.pose
    local rootRotation = eulerRotation(source.rx, source.ry, source.rz)
    local rotated = rotateVector(rootRotation, vector(point.x * source.sx,
        point.y * source.sy, point.z * source.sz))
    return vector(source.x + rotated.x, source.y + rotated.y, source.z + rotated.z)
end

local function getSourceBone(name)
    local report = S.source.skeletonReport
    if not report or type(report.bones) ~= 'table' then return nil end
    for _, bone in ipairs(report.bones) do
        if bone.name == name then return bone end
    end
    return nil
end

local function getSourceBoneEndpoints(bone)
    local report = S.source.skeletonReport
    if not bone or not report or type(report.bones) ~= 'table' then return nil end
    local head
    if (bone.parentIndex or 0) > 0 then
        local parent = report.bones[bone.parentIndex]
        head = parent and matrixPoint(parent.globalBindMatrix)
    else
        head = matrixPoint(bone.globalBindMatrix)
    end
    if not head then return nil end
    local matrix = bone.globalBindMatrix or {}
    local offset = bone.tailOffset
    if type(offset) ~= 'table' then
        offset = {x = 0, y = math.max(bone.length or 0, 0.001), z = 0}
    end
    local tail = vector(
        (matrix[13] or head.x) + (matrix[1] or 1) * (offset.x or 0) +
            (matrix[5] or 0) * (offset.y or 0) + (matrix[9] or 0) * (offset.z or 0),
        (matrix[14] or head.y) + (matrix[2] or 0) * (offset.x or 0) +
            (matrix[6] or 1) * (offset.y or 0) + (matrix[10] or 0) * (offset.z or 0),
        (matrix[15] or head.z) + (matrix[3] or 0) * (offset.x or 0) +
            (matrix[7] or 0) * (offset.y or 0) + (matrix[11] or 1) * (offset.z or 0))
    if (bone.parentIndex or 0) > 0 then tail = matrixPoint(bone.globalBindMatrix) end
    return head, tail
end

local function unitSphereVertices()
    local vertices = {}
    local function add(x, y, z)
        vertices[#vertices + 1] = x
        vertices[#vertices + 1] = y
        vertices[#vertices + 1] = z
    end
    local latitudeSegments, longitudeSegments = 8, 12
    for latitude = 0, latitudeSegments - 1 do
        local theta1 = latitude / latitudeSegments * math.pi
        local theta2 = (latitude + 1) / latitudeSegments * math.pi
        for longitude = 0, longitudeSegments - 1 do
            local phi1 = longitude / longitudeSegments * math.pi * 2
            local phi2 = (longitude + 1) / longitudeSegments * math.pi * 2
            local function point(theta, phi)
                local sine = math.sin(theta)
                return sine * math.cos(phi), math.cos(theta), sine * math.sin(phi)
            end
            local x1, y1, z1 = point(theta1, phi1)
            local x2, y2, z2 = point(theta1, phi2)
            local x3, y3, z3 = point(theta2, phi1)
            local x4, y4, z4 = point(theta2, phi2)
            add(x1, y1, z1); add(x2, y2, z2); add(x3, y3, z3)
            add(x2, y2, z2); add(x4, y4, z4); add(x3, y3, z3)
        end
    end
    return vertices
end

local function createPivotMarker()
    local object = shape:new('3d', 0, 0, 0)
    if not object:create(unitSphereVertices(), nil, 'attachments_target_pivot_marker') then
        object:destroy()
        return nil
    end
    object:setColor(1.0, 0.9, 0.15, 1.0)
    object.alwaysOnTop = true
    object.alwaysOnTopPriority = 25
    object.alwaysRender = true
    return object
end

local function createPartSelectionMarker()
    local object = shape:new('3d', 0, 0, 0)
    if not object:create(unitSphereVertices(), nil, 'attachments_articulated_part_selection_marker') then
        object:destroy()
        return nil
    end
    object:setColor(1.0, 0.35, 0.1, 1.0)
    object.alwaysOnTop = true
    object.alwaysOnTopPriority = 26
    object.alwaysRender = true
    object.visible = false
    return object
end

local function createAimPointMarker()
    local object = shape:new('3d', 0, 0, 0)
    if not object:create(cubeVertices(), nil, 'attachments_source_aim_point_marker') then
        object:destroy()
        return nil
    end
    object:setColor(1.0, 0.25, 0.85, 1.0)
    object.alwaysOnTop = true
    object.alwaysOnTopPriority = 24
    object.alwaysRender = true
    return object
end

local function getTargetPivotMarkerRadius()
    local entry = S.target
    local signature = table.concat({
        entry.path or '', entry.meshLoaded and 'mesh' or 'placeholder',
        tostring(entry.previewSize or 0), tostring(entry.pose.sx or 1),
        tostring(entry.pose.sy or 1), tostring(entry.pose.sz or 1),
    }, ':')
    if S.targetPivotMarkerRadius and S.targetPivotMarkerSignature == signature then
        return S.targetPivotMarkerRadius
    end

    local extent
    if entry.object then
        local ok, width, height, depth = dpCall(function()
            return entry.object:getAABB(true)
        end)
        if ok then
            extent = math.max(tonumber(width) or 0, tonumber(height) or 0, tonumber(depth) or 0)
        end
    end
    if not extent or extent <= 1e-6 then
        extent = entry.meshLoaded and 1 or math.max(1, entry.previewSize or 1)
    end

    S.targetPivotMarkerRadius = math.max(extent * 0.012, 0.25)
    S.targetPivotMarkerSignature = signature
    return S.targetPivotMarkerRadius
end

local function getAimPointHandleSize()
    local targetExtent = getTargetPivotMarkerRadius() / 0.012
    return math.max(targetExtent * 0.10, 0.01)
end

local function ensureSourceDebugMesh()
    local source = S.source
    if source.debugMesh then return true end
    if not source.path or source.path == '' then return false end
    local ok, debugMesh = dpCall(function()
        local result = meshDebug:new()
        if not result:load(source.path) then
            result:destroy()
            error('Could not load mesh data for attachment part picker')
        end
        return result
    end)
    if not ok or not debugMesh then return false end
    source.debugMesh = debugMesh
    return true
end

local function getSourceMeshBounds()
    local source = S.source
    if source.meshBounds then return source.meshBounds end
    if not ensureSourceDebugMesh() then return nil end
    local minX, minY, minZ = math.huge, math.huge, math.huge
    local maxX, maxY, maxZ = -math.huge, -math.huge, -math.huge
    local total = 0
    local ok, subsets = dpCall(function() return source.debugMesh:getTotalSubset(1) end)
    if not ok then return nil end
    for subset = 1, subsets do
        local okVertices, count = dpCall(function()
            return source.debugMesh:getTotalVertex(1, subset)
        end)
        if okVertices then
            for vertex = 1, count do
                local okPoint, point = dpCall(function()
                    return source.debugMesh:getVertex(1, subset, vertex)
                end)
                if okPoint and point then
                    minX, minY, minZ = math.min(minX, point.x), math.min(minY, point.y), math.min(minZ, point.z)
                    maxX, maxY, maxZ = math.max(maxX, point.x), math.max(maxY, point.y), math.max(maxZ, point.z)
                    total = total + 1
                end
            end
        end
    end
    if total == 0 then return nil end
    source.meshBounds = {minX = minX, minY = minY, minZ = minZ,
        maxX = maxX, maxY = maxY, maxZ = maxZ, total = total}
    return source.meshBounds
end

local function transformMatrixPoint(matrix, point)
    if type(matrix) ~= 'table' then return vector(point.x, point.y, point.z) end
    return vector(
        point.x * (matrix[1] or 1) + point.y * (matrix[5] or 0) + point.z * (matrix[9] or 0) + (matrix[13] or 0),
        point.x * (matrix[2] or 0) + point.y * (matrix[6] or 1) + point.z * (matrix[10] or 0) + (matrix[14] or 0),
        point.x * (matrix[3] or 0) + point.y * (matrix[7] or 0) + point.z * (matrix[11] or 1) + (matrix[15] or 0))
end

local function sourcePoseSignature()
    local poseValue = S.source.pose
    return table.concat({poseValue.x, poseValue.y, poseValue.z, poseValue.rx, poseValue.ry,
        poseValue.rz, poseValue.sx, poseValue.sy, poseValue.sz}, ':')
end

local function buildArticulatedPartPickGeometry()
    local source = S.source
    if not ensureSourceDebugMesh() then return false end
    local signature = sourcePoseSignature()
    if source.articulatedPartPickGeometry and source.articulatedPartPickGeometry.signature == signature then
        return true
    end
    local okTotal, totalParts = dpCall(function() return source.object:getTotalArticulatedParts() end)
    if not okTotal then return false end
    local geometry = {signature = signature, parts = {}}
    for index = 1, tonumber(totalParts) or 0 do
        local okInfo, info = dpCall(function() return source.object:getArticulatedPartInfo(index) end)
        if okInfo and type(info) == 'table' and type(info.name) == 'string' and info.name ~= '' then
            local frame = tonumber(info.frame) or 1
            local subset = tonumber(info.subset) or 1
            local okTransform, part = dpCall(function()
                return source.object:getArticulatedPartTransform(info.name, 'model')
            end)
            local partMatrix = okTransform and type(part) == 'table' and part.matrix or nil
            local okVertices, vertexCount = dpCall(function()
                return source.debugMesh:getTotalVertex(frame, subset)
            end)
            if okVertices and tonumber(vertexCount) and vertexCount > 0 then
                local vertices = {}
                for vertex = 1, vertexCount do
                    local okPoint, point = dpCall(function()
                        return source.debugMesh:getVertex(frame, subset, vertex)
                    end)
                    if okPoint and point then
                        vertices[vertex] = transformSourcePoint(transformMatrixPoint(partMatrix, point))
                    end
                end
                local okIndices, indices = dpCall(function()
                    return source.debugMesh:getIndex(frame, subset)
                end)
                local triangles = {}
                local function addTriangle(a, b, c)
                    if not vertices[a] or not vertices[b] or not vertices[c] then return end
                    local first, second, third = vertices[a], vertices[b], vertices[c]
                    triangles[#triangles + 1] = {a = first, b = second, c = third}
                end
                if okIndices and type(indices) == 'table' and #indices >= 3 then
                    for current = 1, #indices - 2, 3 do
                        addTriangle(indices[current], indices[current + 1], indices[current + 2])
                    end
                else
                    for current = 1, vertexCount - 2, 3 do
                        addTriangle(current, current + 1, current + 2)
                    end
                end
                if #triangles > 0 then
                    local bounds = {minX = math.huge, minY = math.huge, minZ = math.huge,
                        maxX = -math.huge, maxY = -math.huge, maxZ = -math.huge}
                    for _, triangle in ipairs(triangles) do
                        for _, point in ipairs({triangle.a, triangle.b, triangle.c}) do
                            bounds.minX = math.min(bounds.minX, point.x)
                            bounds.minY = math.min(bounds.minY, point.y)
                            bounds.minZ = math.min(bounds.minZ, point.z)
                            bounds.maxX = math.max(bounds.maxX, point.x)
                            bounds.maxY = math.max(bounds.maxY, point.y)
                            bounds.maxZ = math.max(bounds.maxZ, point.z)
                        end
                    end
                    bounds.centerX = (bounds.minX + bounds.maxX) * 0.5
                    bounds.centerY = (bounds.minY + bounds.maxY) * 0.5
                    bounds.centerZ = (bounds.minZ + bounds.maxZ) * 0.5
                    geometry.parts[#geometry.parts + 1] = {
                        name = info.name, triangles = triangles, bounds = bounds,
                    }
                end
            end
        end
    end
    source.articulatedPartPickGeometry = geometry
    return #geometry.parts > 0
end

local function getBoneSelectionMinCameraDistance()
    if not isSourceSelectionMode() then return 80 end
    local bounds = getSourceMeshBounds()
    if not bounds then return 0.01 end
    local extent = math.max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY,
        bounds.maxZ - bounds.minZ)
    return math.max(0.01, extent * 0.01)
end

local function frameBoneSelectionCamera()
    local bounds = getSourceMeshBounds()
    if not bounds then return false end
    local points = {}
    for _, x in ipairs({bounds.minX, bounds.maxX}) do
        for _, y in ipairs({bounds.minY, bounds.maxY}) do
            for _, z in ipairs({bounds.minZ, bounds.maxZ}) do
                points[#points + 1] = transformSourcePoint({x = x, y = y, z = z})
            end
        end
    end
    local minX, minY, minZ = math.huge, math.huge, math.huge
    local maxX, maxY, maxZ = -math.huge, -math.huge, -math.huge
    for _, point in ipairs(points) do
        minX, minY, minZ = math.min(minX, point.x), math.min(minY, point.y), math.min(minZ, point.z)
        maxX, maxY, maxZ = math.max(maxX, point.x), math.max(maxY, point.y), math.max(maxZ, point.z)
    end
    S.cameraOrbit.fx = (minX + maxX) * 0.5
    S.cameraOrbit.fy = (minY + maxY) * 0.5
    S.cameraOrbit.fz = (minZ + maxZ) * 0.5
    local dx, dy, dz = maxX - minX, maxY - minY, maxZ - minZ
    S.cameraOrbit.distance = math.max(getBoneSelectionMinCameraDistance(),
        math.sqrt(dx * dx + dy * dy + dz * dz) * 1.4)
    applyCamera()
    return true
end

local function setSourceMeshToTPose()
    local source = S.source
    local report = source.skeletonReport
    if not source.object or not report or type(report.bones) ~= 'table' or #report.bones == 0 then return false end
    local okMethod, method = dpCall(function()
        return source.object:getResolvedSkeletalSkinningMethod()
    end)
    if not okMethod or (method ~= 'lbs' and method ~= 'dqs') then method = 'lbs' end
    local rows, boneIds = {}, {}
    local function appendRow(a, b, c, d)
        rows[#rows + 1] = a
        rows[#rows + 1] = b
        rows[#rows + 1] = c
        rows[#rows + 1] = d
    end
    for _, bone in ipairs(report.bones) do
        boneIds[#boneIds + 1] = bone.boneId
        if method == 'dqs' then
            appendRow(0, 0, 0, 1)
            appendRow(0, 0, 0, 0)
        else
            appendRow(1, 0, 0, 0)
            appendRow(0, 1, 0, 0)
            appendRow(0, 0, 1, 0)
        end
    end
    local okApply, applied, reason = dpCall(function()
        return source.object:setSkeletalAuthoringPalette(method, rows, 0, boneIds)
    end)
    if not okApply or not applied then
        setStatus('Could not set skeletal mesh to bind/T-pose: ' .. tostring(reason or 'unknown'), true)
        return false
    end
    return true
end

local function buildSourceSkeletonVisuals()
    destroySourceSkeletonVisuals()
    local report = S.source.skeletonReport
    if not report or report.canonical ~= true or type(report.bones) ~= 'table' then return false end
    S.source.skeletonGeneration = S.source.skeletonGeneration + 1
    local bounds = getSourceMeshBounds()
    local extent = bounds and math.max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY,
        bounds.maxZ - bounds.minZ) or 1
    local jointRadius = math.max(extent * 0.018, 0.001)
    local sphereVertices = unitSphereVertices()
    for _, bone in ipairs(report.bones) do
        local head, tail = getSourceBoneEndpoints(bone)
        if head and tail then
            local object = line:new('3d', 0, 0, 0)
            object:add({head.x, head.y, head.z, tail.x, tail.y, tail.z})
            object.alwaysOnTop = true
            object.alwaysOnTopPriority = 6
            object.alwaysRender = true
            local joint = shape:new('3d', head.x, head.y, head.z)
            joint:create(sphereVertices, nil, 'attachments_bone_joint_' ..
                tostring(S.source.skeletonGeneration) .. '_' .. tostring(bone.name))
            joint:setScale(jointRadius, jointRadius, jointRadius)
            joint.alwaysOnTop = true
            joint.alwaysOnTopPriority = 7
            joint.alwaysRender = true
            S.source.boneVisuals[bone.name] = {
                object = object, joint = joint, head = head, tail = tail, radius = jointRadius,
            }
        end
    end
    updateSourceSkeletonVisuals()
    return next(S.source.boneVisuals) ~= nil
end

updateSourceSkeletonVisuals = function()
    local source = S.source
    local visible = S.definition.sourceType == 'bone' and S.boneSelectionMode
    for name, visual in pairs(source.boneVisuals) do
        local object = visual.object
        object:setPos(source.pose.x, source.pose.y, source.pose.z)
        object:setAngle(source.pose.rx, source.pose.ry, source.pose.rz)
        object:setScale(source.pose.sx, source.pose.sy, source.pose.sz)
        if visual.joint then
            local jointPosition = transformSourcePoint(visual.head)
            visual.joint:setPos(jointPosition.x, jointPosition.y, jointPosition.z)
            visual.joint:setAngle(source.pose.rx, source.pose.ry, source.pose.rz)
            visual.joint:setScale(visual.radius * source.pose.sx, visual.radius * source.pose.sy,
                visual.radius * source.pose.sz)
        end
        if name == source.selectedBone then
            object:setColor(1, 0.85, 0.05, 1)
            if visual.joint then visual.joint:setColor(1, 0.85, 0.05, 1) end
        else
            object:setColor(1, 0.1, 1, 0.9)
            if visual.joint then visual.joint:setColor(1, 0, 1, 0.85) end
        end
        object.visible = visible
        if visual.joint then visual.joint.visible = visible end
    end
end

prepareSourceSkeleton = function()
    local source = S.source
    if not source.object or not source.meshLoaded or source.path == '' then return false end
    if not source.debugMesh then
        local ok, debugMesh = dpCall(function()
            local result = meshDebug:new()
            if not result:load(source.path) then
                result:destroy()
                error('Could not load skeletal data for attachment bone picker')
            end
            return result
        end)
        if not ok or not debugMesh then return false end
        source.debugMesh = debugMesh
    end
    local okReport, report = dpCall(function() return source.debugMesh:getSkeletonBindReport(false) end)
    if not okReport or type(report) ~= 'table' or report.canonical ~= true or
            type(report.bones) ~= 'table' or #report.bones == 0 then
        setStatus('The source mesh has no canonical skeletal bind pose.', true)
        destroySourceSkeletonVisuals()
        source.skeletonReport = nil
        return false
    end
    source.skeletonReport = report
    source.selectedBone = S.definition.sourceBone or ''
    setSourceMeshToTPose()
    buildSourceSkeletonVisuals()
    updateSourceSkeletonVisibility()
    if S.boneSelectionMode then frameBoneSelectionCamera() end
    return true
end

setBoneSelectionMode = function(enabled)
    enabled = enabled == true
    if enabled == S.boneSelectionMode then return true end
    if enabled then
        if S.definition.sourceType ~= 'bone' then return false end
        if not S.source.meshLoaded then
            setStatus(L('bone_select_requires_mesh'), true)
            return false
        end
        S.source.selectionAnimation = S.source.activeAnimation ~= '' and {
            name = S.source.activeAnimation, paused = S.source.animationPaused == true,
        } or nil
        if not prepareSourceSkeleton() then return false end
        S.boneSelectionCamera = {
            azimuth = S.cameraOrbit.azimuth, elevation = S.cameraOrbit.elevation,
            distance = S.cameraOrbit.distance, fx = S.cameraOrbit.fx,
            fy = S.cameraOrbit.fy, fz = S.cameraOrbit.fz,
        }
        S.boneSelectionTargetVisible = {}
        S.boneSelectionMode = true
        for index, targetState in ipairs(S.targets) do
            S.boneSelectionTargetVisible[index] = targetState.object and targetState.object.visible ~= false
            if targetState.object then targetState.object.visible = false end
            local relation = S.relationLines[index]
            if relation then relation.visible = false end
        end
        if S.targetPivotMarker then S.targetPivotMarker.visible = false end
        if S.xzPlane then S.xzPlane.visible = false end
        updateSourceSkeletonVisibility()
        frameBoneSelectionCamera()
        hideGizmo()
        return true
    end
    S.boneSelectionMode = false
    for index, targetState in ipairs(S.targets) do
        if targetState.object then
            targetState.object.visible = not S.boneSelectionTargetVisible or
                S.boneSelectionTargetVisible[index] ~= false
        end
        local relation = S.relationLines[index]
        if relation then relation.visible = true end
    end
    if S.targetPivotMarker then S.targetPivotMarker.visible = true end
    if S.boneSelectionCamera then
        S.cameraOrbit = S.boneSelectionCamera
        S.boneSelectionCamera = nil
        applyCamera()
    end
    S.boneSelectionTargetVisible = nil
    updateSourceSkeletonVisibility()
    restoreSourceAnimation()
    S.dirtyPreview = true
    return true
end

local setPartSelectionMode

setPartSelectionMode = function(enabled)
    enabled = enabled == true
    if enabled == S.partSelectionMode then return true end
    if enabled then
        if S.definition.sourceType ~= 'articulated_part' then return false end
        if not S.source.meshLoaded then
            setStatus(L('part_select_requires_mesh'), true)
            return false
        end
        S.source.selectionAnimation = S.source.activeAnimation ~= '' and {
            name = S.source.activeAnimation, paused = S.source.animationPaused == true,
        } or nil
        stopSourceAnimation()
        if not buildArticulatedPartPickGeometry() then
            S.source.selectionAnimation = nil
            setStatus(L('no_articulated_parts'), true)
            return false
        end
        S.partSelectionCamera = {
            azimuth = S.cameraOrbit.azimuth, elevation = S.cameraOrbit.elevation,
            distance = S.cameraOrbit.distance, fx = S.cameraOrbit.fx,
            fy = S.cameraOrbit.fy, fz = S.cameraOrbit.fz,
        }
        S.partSelectionTargetVisible = {}
        S.partSelectionMode = true
        for index, targetState in ipairs(S.targets) do
            S.partSelectionTargetVisible[index] = targetState.object and targetState.object.visible ~= false
            if targetState.object then targetState.object.visible = false end
            local relation = S.relationLines[index]
            if relation then relation.visible = false end
        end
        if S.targetPivotMarker then S.targetPivotMarker.visible = false end
        if S.aimPointMarker then S.aimPointMarker.visible = false end
        if S.xzPlane then S.xzPlane.visible = false end
        if S.source.object then S.source.object.visible = true end
        frameBoneSelectionCamera()
        hideGizmo()
        updatePartSelectionMarker()
        return true
    end
    S.partSelectionMode = false
    for index, targetState in ipairs(S.targets) do
        if targetState.object then
            targetState.object.visible = not S.partSelectionTargetVisible or
                S.partSelectionTargetVisible[index] ~= false
        end
        local relation = S.relationLines[index]
        if relation then relation.visible = true end
    end
    if S.targetPivotMarker then S.targetPivotMarker.visible = true end
    if S.partSelectionCamera then
        S.cameraOrbit = S.partSelectionCamera
        S.partSelectionCamera = nil
        applyCamera()
    end
    S.partSelectionTargetVisible = nil
    restoreSourceAnimation()
    updatePartSelectionMarker()
    S.dirtyPreview = true
    return true
end

local function getSourceBonePose(name)
    local bone = getSourceBone(name)
    local bindPose = bone and matrixPose(bone.globalBindMatrix) or nil
    if not bindPose then return nil end
    local source = S.source.pose
    local rootRotation = eulerRotation(source.rx, source.ry, source.rz)
    local localPosition = vector(bindPose.x * source.sx, bindPose.y * source.sy, bindPose.z * source.sz)
    local worldPosition = rotateVector(rootRotation, localPosition)
    local worldRotation = multiplyRotation(rootRotation, bindPose.rotation)
    local rx, ry, rz = matrixToEuler(worldRotation)
    return {
        x = source.x + worldPosition.x, y = source.y + worldPosition.y, z = source.z + worldPosition.z,
        rx = rx, ry = ry, rz = rz,
        sx = source.sx * bindPose.sx, sy = source.sy * bindPose.sy, sz = source.sz * bindPose.sz,
        rotation = worldRotation,
    }
end

local function openRoleMesh(role)
    local entry = roleEntry(role)
    local path = mbm.openFile(entry.path or '', 'msh')
    if path then loadRoleMesh(role, path) end
end

local function setOriginsVisible(visible)
    S.showOrigins = visible == true
    for _, object in pairs(S.originLines) do
        if object then object.visible = S.showOrigins end
    end
end

local function createOriginLines()
    local definitions = {
        x = {{-100000, 0, 0, 100000, 0, 0}, {1, 0, 0, 1}},
        y = {{0, -100000, 0, 0, 100000, 0}, {0, 1, 0, 1}},
        z = {{0, 0, -100000, 0, 0, 100000}, {0, 0, 1, 1}},
    }
    for name, definition in pairs(definitions) do
        local object = line:new('3d', 0, 0, 0)
        object:add(definition[1])
        object:setColor(table.unpack(definition[2]))
        object.alwaysOnTop = true
        object.alwaysOnTopPriority = 1
        object.alwaysRender = true
        S.originLines[name] = object
    end
    setOriginsVisible(S.showOrigins)
end

local function createXZPlane()
    local points = {
        -0.5, 0, -0.5, 0.5, 0, -0.5, 0.5, 0, 0.5,
        -0.5, 0, -0.5, 0.5, 0, 0.5, -0.5, 0, 0.5,
    }
    local object = shape:new('3d', 0, 0, 0)
    if not object:create(points, nil, 'attachments_xz_plane') then
        object:destroy()
        return
    end
    object:setColor(0.24, 0.30, 0.38, 0.8)
    object.alwaysRender = true
    object.visible = S.showXZPlane
    S.xzPlane = object
end

local function updateXZPlane()
    if not S.xzPlane then return end
    local size = math.max(400, S.cameraOrbit.distance * 2.4)
    if not S.xzPlaneSize or math.abs(S.xzPlaneSize - size) > 0.001 then
        S.xzPlane:setScale(size, 1, size)
        S.xzPlaneSize = size
    end
    if S.xzPlaneX ~= S.cameraOrbit.fx or S.xzPlaneZ ~= S.cameraOrbit.fz then
        S.xzPlane:setPos(S.cameraOrbit.fx, 0, S.cameraOrbit.fz)
        S.xzPlaneX, S.xzPlaneZ = S.cameraOrbit.fx, S.cameraOrbit.fz
    end
    S.xzPlane.visible = S.showXZPlane and not isSourceSelectionMode()
end

local function applyMarker(role, value)
    local entry = roleEntry(role)
    ensureRolePlaceholder(role)
    if not entry.object then return end
    entry.object:setPos(value.x, value.y, value.z)
    entry.object:setAngle(value.rx, value.ry, value.rz)
    local size = entry.meshLoaded and 1 or entry.previewSize
    entry.object:setScale(size * value.sx, size * value.sy, size * value.sz)
end

getPreviewSourcePose = function()
    if S.definition.sourceType == 'articulated_part' then
        if not S.source.object or not S.source.meshLoaded or S.definition.sourcePart == '' then return nil end
        local ok, part = dpCall(function()
            return S.source.object:getArticulatedPartTransform(S.definition.sourcePart, 'world')
        end)
        if ok and type(part) == 'table' and type(part.position) == 'table' and
                type(part.angle) == 'table' and type(part.scale) == 'table' then
            local partMatrixPose = matrixPose(part.matrix)
            local anchor = type(part.pivotPosition) == 'table' and part.pivotPosition or part.position
            return {
                x = anchor.x or 0, y = anchor.y or 0, z = anchor.z or 0,
                rx = part.angle.x or 0, ry = part.angle.y or 0, rz = part.angle.z or 0,
                sx = part.scale.x or 1, sy = part.scale.y or 1, sz = part.scale.z or 1,
                rotation = partMatrixPose and partMatrixPose.rotation or nil,
            }
        end
        return nil
    end
    if S.definition.sourceType == 'socket' then
        if not S.source.object or not S.source.meshLoaded or S.definition.sourceSocket == '' then return nil end
        local ok, socket = dpCall(function()
            return S.source.object:getSkeletalSocketTransform(S.definition.sourceSocket, 'world')
        end)
        if ok and type(socket) == 'table' and type(socket.position) == 'table' and
                type(socket.angle) == 'table' and type(socket.scale) == 'table' then
            local socketMatrixPose = matrixPose(socket.matrix)
            return {
                x = socket.position.x or 0, y = socket.position.y or 0, z = socket.position.z or 0,
                rx = socket.angle.x or 0, ry = socket.angle.y or 0, rz = socket.angle.z or 0,
                sx = socket.scale.x or 1, sy = socket.scale.y or 1, sz = socket.scale.z or 1,
                rotation = socketMatrixPose and socketMatrixPose.rotation or nil,
            }
        end
        return nil
    end
    if S.definition.sourceType ~= 'bone' then return clonePose(S.source.pose) end
    if not S.source.object or not S.source.meshLoaded or S.definition.sourceBone == '' then return nil end
    if not S.boneSelectionMode and S.source.activeAnimation ~= '' then
        local ok, bone = dpCall(function()
            return S.source.object:getSkeletalBoneTransform(S.definition.sourceBone, 'world')
        end)
        if ok and type(bone) == 'table' and type(bone.position) == 'table' and
                type(bone.angle) == 'table' and type(bone.scale) == 'table' then
            local boneMatrixPose = matrixPose(bone.matrix)
            return {
                x = bone.position.x or 0, y = bone.position.y or 0, z = bone.position.z or 0,
                rx = bone.angle.x or 0, ry = bone.angle.y or 0, rz = bone.angle.z or 0,
                sx = bone.scale.x or 1, sy = bone.scale.y or 1, sz = bone.scale.z or 1,
                rotation = boneMatrixPose and boneMatrixPose.rotation or nil,
            }
        end
    end
    return getSourceBonePose(S.definition.sourceBone)
end

local function dynamicAimBaseRotation(source)
    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
    local offsetRotation = S.definition.offset.rotationMatrix or
        eulerRotation(S.definition.offset.rotation.x, S.definition.offset.rotation.y,
            S.definition.offset.rotation.z)
    return multiplyRotation(offsetRotation, sourceRotation)
end

local function aimBaseRotation(source)
    local limits = S.definition.aimLimits
    if limits and limits.reference == 'captured' then
        if not limits.referenceRotationMatrix then captureAimReference(source) end
        if limits.referenceRotationMatrix then return limits.referenceRotationMatrix end
    end
    return dynamicAimBaseRotation(source)
end

captureAimReference = function(sourceOverride)
    local source = sourceOverride or getActiveTargetSourcePose() or S.source.pose
    if not source then return false end
    S.definition.aimLimits.referenceRotationMatrix = cloneRotation(dynamicAimBaseRotation(source))
    return true
end

local function dot(ax, ay, az, bx, by, bz)
    return ax * bx + ay * by + az * bz
end

local function normalizedDirection(x, y, z)
    local length = math.sqrt(x * x + y * y + z * z)
    if length <= 1e-8 then return nil end
    return {x = x / length, y = y / length, z = z / length}
end

local function rayAxisParameter(screenX, screenY, origin, axis)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, screenX, screenY)
    if not ok then return nil end
    local rx, ry, rz = ox - origin.x, oy - origin.y, oz - origin.z
    local a = dot(dx, dy, dz, dx, dy, dz)
    local b = dot(dx, dy, dz, axis.x, axis.y, axis.z)
    local c = dot(axis.x, axis.y, axis.z, axis.x, axis.y, axis.z)
    local d = dot(dx, dy, dz, rx, ry, rz)
    local e = dot(axis.x, axis.y, axis.z, rx, ry, rz)
    local denominator = a * c - b * b
    if math.abs(denominator) < 1e-6 then return nil end
    return (a * e - b * d) / denominator
end

local function raySegmentDistance(screenX, screenY, first, second)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, screenX, screenY)
    if not ok then return math.huge end
    local ax, ay, az = second.x - first.x, second.y - first.y, second.z - first.z
    local rx, ry, rz = ox - first.x, oy - first.y, oz - first.z
    local aa = dot(dx, dy, dz, dx, dy, dz)
    local ab = dot(dx, dy, dz, ax, ay, az)
    local bb = dot(ax, ay, az, ax, ay, az)
    local ar = dot(dx, dy, dz, rx, ry, rz)
    local br = dot(ax, ay, az, rx, ry, rz)
    local denominator = aa * bb - ab * ab
    local rayT, segmentT
    if math.abs(denominator) < 1e-6 then
        rayT = 0
        segmentT = bb > 1e-6 and math.max(0, math.min(1, br / bb)) or 0
    else
        rayT = math.max(0, (ab * br - bb * ar) / denominator)
        segmentT = math.max(0, math.min(1, (aa * br - ab * ar) / denominator))
    end
    local px, py, pz = ox + dx * rayT, oy + dy * rayT, oz + dz * rayT
    local qx, qy, qz = first.x + ax * segmentT, first.y + ay * segmentT, first.z + az * segmentT
    local ex, ey, ez = px - qx, py - qy, pz - qz
    return math.sqrt(ex * ex + ey * ey + ez * ez)
end

local function rayPlaneHit(screenX, screenY, point, normal)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, screenX, screenY)
    if not ok then return nil end
    local denominator = normal.x * dx + normal.y * dy + normal.z * dz
    if math.abs(denominator) < 1e-6 then return nil end
    local distance = ((point.x - ox) * normal.x + (point.y - oy) * normal.y +
        (point.z - oz) * normal.z) / denominator
    if distance < 0 then return nil end
    return ox + dx * distance, oy + dy * distance, oz + dz * distance
end

local function pickRole(screenX, screenY)
    local entries = {{role = 'source', index = nil, entry = S.source}}
    for index, targetState in ipairs(S.targets) do
        entries[#entries + 1] = {role = 'target', index = index, entry = targetState}
    end
    for _, item in ipairs(entries) do
        local role, entry = item.role, item.entry
        if entry.selectable ~= false and entry.object then
            local ok, hit = dpCall(function()
                return entry.object:collide(screenX, screenY, true)
            end)
            if ok and hit then return role, item.index end
        end
    end
    return nil
end

local function hideAimLimitsVisual()
    for _, object in pairs(S.aimLimitsVisual.objects) do object.visible = false end
    for _, handle in pairs(S.aimLimitsVisual.handles or {}) do handle.object.visible = false end
    S.aimLimitsVisual.signature = nil
end

local function updateAimLimitsVisual()
    local visual = S.aimLimitsVisual
    local limits = S.definition.aimLimits
    if isSourceSelectionMode() or not S.showAimLimits or not S.target.object or
            not S.definition.aimAtSource or not limits or not limits.enabled then
        hideAimLimitsVisual()
        return
    end
    local source = getActiveTargetSourcePose() or S.source.pose
    local baseRotation = aimBaseRotation(source)
    local origin = targetPivotWorldPosition(S.target.pose)
    local radius = math.max(S.target.previewSize * 1.8, 45)
    local signatureParts = {origin.x, origin.y, origin.z, radius,
        limits.yawMin, limits.yawMax, limits.pitchMin, limits.pitchMax}
    for row = 1, 3 do
        for column = 1, 3 do signatureParts[#signatureParts + 1] = baseRotation[row][column] end
    end
    local signature = table.concat(signatureParts, ':')
    if visual.signature == signature then
        for _, object in pairs(visual.objects) do object.visible = true end
        for _, handle in pairs(visual.handles or {}) do handle.object.visible = true end
        return
    end
    local function worldPoint(x, y, z)
        local point = rotateVector(baseRotation, vector(x, y, z))
        return {x = origin.x + point.x, y = origin.y + point.y, z = origin.z + point.z}
    end
    local function arcPoints(first, last, angleFunction)
        local points = {}
        local steps = 64
        for index = 0, steps do
            local angle = first + (last - first) * index / steps
            local x, y, z = angleFunction(angle)
            local point = worldPoint(x * radius, y * radius, z * radius)
            points[#points + 1] = point.x
            points[#points + 1] = point.y
            points[#points + 1] = point.z
        end
        return points
    end
    local yawPoints = arcPoints(limits.yawMin, limits.yawMax, function(angle)
        return math.sin(angle), 0, math.cos(angle)
    end)
    local pitchPoints = arcPoints(limits.pitchMin, limits.pitchMax, function(angle)
        return 0, math.sin(angle), math.cos(angle)
    end)
    local function corner(yaw, pitch)
        local cosinePitch = math.cos(pitch)
        return worldPoint(radius * cosinePitch * math.sin(yaw), radius * math.sin(pitch),
            radius * cosinePitch * math.cos(yaw))
    end
    local function rayPoints(yaw, pitch)
        local point = corner(yaw, pitch)
        return {origin.x, origin.y, origin.z, point.x, point.y, point.z}
    end
    local paths = {
        {name = 'yaw', points = yawPoints, color = {0.15, 1, 0.2, 1}},
        {name = 'pitch', points = pitchPoints, color = {1, 0.15, 0.15, 1}},
        {name = 'corner_a', points = rayPoints(limits.yawMin, limits.pitchMin), color = {1, 0.85, 0.15, 1}},
        {name = 'corner_b', points = rayPoints(limits.yawMin, limits.pitchMax), color = {1, 0.85, 0.15, 1}},
        {name = 'corner_c', points = rayPoints(limits.yawMax, limits.pitchMin), color = {1, 0.85, 0.15, 1}},
        {name = 'corner_d', points = rayPoints(limits.yawMax, limits.pitchMax), color = {1, 0.85, 0.15, 1}},
    }
    for _, path in ipairs(paths) do
        local object = visual.objects[path.name]
        if not object then
            object = line:new('3d', 0, 0, 0)
            object:add(path.points)
            object.alwaysOnTop = true
            object.alwaysOnTopPriority = 12
            object.alwaysRender = true
            visual.objects[path.name] = object
        else
            object:set(path.points, 1)
        end
        object:setColor(table.unpack(path.color))
        object.visible = true
    end
    local handlePoints = {
        yaw_min = corner(limits.yawMin, 0),
        yaw_max = corner(limits.yawMax, 0),
        pitch_min = corner(0, limits.pitchMin),
        pitch_max = corner(0, limits.pitchMax),
    }
    local handleSize = getAimPointHandleSize()
    visual.handles = visual.handles or {}
    for name, point in pairs(handlePoints) do
        local handle = visual.handles[name]
        if not handle then
            local object = shape:new('3d', point.x, point.y, point.z)
            if object:create(cubeVertices(), nil, 'attachments_aim_limit_handle_' .. name) then
                object.alwaysOnTop = true
                object.alwaysOnTopPriority = 14
                object.alwaysRender = true
                handle = {object = object}
                visual.handles[name] = handle
            else
                object:destroy()
            end
        end
        if handle then
            handle.object:setPos(point.x, point.y, point.z)
            handle.object:setScale(handleSize, handleSize, handleSize)
            handle.object:setColor(name:find('yaw') and 0.15 or 1,
                name:find('yaw') and 1 or 0.15, 0.2, 1)
            handle.object.visible = true
        end
    end
    visual.signature = signature
end

local function pickAimLimitsHandle(screenX, screenY)
    local visual = S.aimLimitsVisual
    if isSourceSelectionMode() or not S.showAimLimits or S.selectedRole ~= 'target' or
            not S.definition.aimAtSource or
            not S.definition.aimLimits or not S.definition.aimLimits.enabled then
        return nil
    end
    for name, handle in pairs(visual.handles or {}) do
        local ok, hit = dpCall(function()
            return handle.object:collide(screenX, screenY, true)
        end)
        if ok and hit then return name end
    end
    return nil
end

hideGizmo = function()
    for _, object in pairs(S.gizmo.axes) do object.visible = false end
    for _, object in pairs(S.gizmo.rings) do object.visible = false end
    hideAimLimitsVisual()
    S.gizmo.drag = nil
    S.gizmo.origin, S.gizmo.length, S.gizmo.radius, S.gizmo.axesSignature = nil, nil, nil, nil
end

local function getGizmoAxes()
    local axes = {
        x = {x = 1, y = 0, z = 0, color = {1, 0.15, 0.15, 1}},
        y = {x = 0, y = 1, z = 0, color = {0.15, 1, 0.2, 1}},
        z = {x = 0, y = 0, z = 1, color = {0.2, 0.45, 1, 1}},
    }
    return axes
end

local function updateGizmo()
    if isSourceSelectionMode() then
        if S.targetPivotMarker then S.targetPivotMarker.visible = false end
        hideGizmo()
        return
    end
    updateTargetPivotMarker()
    if not S.selectedRole and S.transformMode ~= 'aim_point' then hideGizmo(); return end
    if S.transformMode == 'pivot' and S.selectedRole ~= 'target' then hideGizmo(); return end
    if S.transformMode == 'aim_point' and (not S.definition.aimAtSource or not S.source.object) then
        hideGizmo()
        return
    end
    local entry = S.transformMode == 'aim_point' and S.source or roleEntry(S.selectedRole)
    if entry.selectable == false and S.transformMode ~= 'aim_point' then hideGizmo(); return end
    if not entry.object then hideGizmo(); return end
    local origin = {x = entry.pose.x, y = entry.pose.y, z = entry.pose.z}
    if S.transformMode == 'pivot' then
        origin = targetPivotWorldPosition(entry.pose)
    elseif S.transformMode == 'aim_point' then
        local point = sourceAimPointWorldPosition(getActiveTargetSourcePose() or S.source.pose)
        origin = {x = point.x, y = point.y, z = point.z}
    end
    local length = math.max(entry.previewSize * 1.6, 45)
    local radius = math.max(entry.previewSize * 0.9, 30)
    local originChanged = not S.gizmo.origin or S.gizmo.origin.x ~= origin.x or
        S.gizmo.origin.y ~= origin.y or S.gizmo.origin.z ~= origin.z
    local lengthChanged = S.gizmo.length ~= length
    local radiusChanged = S.gizmo.radius ~= radius
    local axes = getGizmoAxes()
    local axisSignatureParts = {}
    for _, name in ipairs({'x', 'y', 'z'}) do
        axisSignatureParts[#axisSignatureParts + 1] = axes[name].x
        axisSignatureParts[#axisSignatureParts + 1] = axes[name].y
        axisSignatureParts[#axisSignatureParts + 1] = axes[name].z
    end
    local axesSignature = table.concat(axisSignatureParts, ':')
    local axesChanged = S.gizmo.axesSignature ~= axesSignature
    for name, axis in pairs(axes) do
        local object = S.gizmo.axes[name]
        local points = {0, 0, 0, axis.x * length, axis.y * length, axis.z * length}
        if not object then
            object = line:new('3d', 0, 0, 0)
            object:add(points)
            S.gizmo.axes[name] = object
        elseif lengthChanged or axesChanged then
            object:set(points, 1)
        end
        if originChanged then object:setPos(origin.x, origin.y, origin.z) end
        object:setColor(table.unpack(axis.color))
        object.alwaysOnTop, object.alwaysOnTopPriority, object.alwaysRender = true, 10, true
        object.visible = S.transformMode == 'move' or S.transformMode == 'pivot' or
            S.transformMode == 'aim_point'
    end
    local rings = {
        x = {u = axes.y, v = axes.z}, y = {u = axes.z, v = axes.x}, z = {u = axes.x, v = axes.y},
    }
    for name, ring in pairs(rings) do
        local object = S.gizmo.rings[name]
        local points = {}
        for index = 0, 64 do
            local angle = index / 64 * math.pi * 2
            points[#points + 1] = (ring.u.x * math.cos(angle) + ring.v.x * math.sin(angle)) * radius
            points[#points + 1] = (ring.u.y * math.cos(angle) + ring.v.y * math.sin(angle)) * radius
            points[#points + 1] = (ring.u.z * math.cos(angle) + ring.v.z * math.sin(angle)) * radius
        end
        if not object then
            object = line:new('3d', 0, 0, 0)
            object:add(points)
            S.gizmo.rings[name] = object
        elseif radiusChanged or axesChanged then
            object:set(points, 1)
        end
        if originChanged then object:setPos(origin.x, origin.y, origin.z) end
        object:setColor(table.unpack(axes[name].color))
        object.alwaysOnTop, object.alwaysOnTopPriority, object.alwaysRender = true, 20, true
        object.visible = S.transformMode == 'rotate'
    end
    S.gizmo.origin, S.gizmo.length, S.gizmo.radius, S.gizmo.axesSignature = origin, length, radius,
        axesSignature
    updateAimLimitsVisual()
end

local function pickGizmoAxis(screenX, screenY)
    if (not S.selectedRole and S.transformMode ~= 'aim_point') or
            (S.transformMode ~= 'move' and S.transformMode ~= 'pivot' and
             S.transformMode ~= 'aim_point') or
            not S.gizmo.origin or
            (S.transformMode ~= 'aim_point' and roleEntry(S.selectedRole).selectable == false) then
        return nil
    end
    local axes = {x = {x = 1, y = 0, z = 0}, y = {x = 0, y = 1, z = 0}, z = {x = 0, y = 0, z = 1}}
    local bestName, bestDistance
    local threshold = math.max(S.gizmo.length * 0.12, 12)
    for name, axis in pairs(axes) do
        local endpoint = {x = S.gizmo.origin.x + axis.x * S.gizmo.length,
            y = S.gizmo.origin.y + axis.y * S.gizmo.length,
            z = S.gizmo.origin.z + axis.z * S.gizmo.length}
        local distance = raySegmentDistance(screenX, screenY, S.gizmo.origin, endpoint)
        if distance <= threshold and (not bestDistance or distance < bestDistance) then
            bestName, bestDistance = name, distance
        end
    end
    return bestName
end

local function pickRotationRing(screenX, screenY)
    if not S.selectedRole or S.transformMode ~= 'rotate' or
            not S.gizmo.origin or
            roleEntry(S.selectedRole).selectable == false then
        return nil
    end
    local axes = getGizmoAxes()
    local best, bestError
    local tolerance = math.max(S.gizmo.radius * 0.12, 12)
    for name, axis in pairs(axes) do
        local wx, wy, wz = rayPlaneHit(screenX, screenY, S.gizmo.origin, axis)
        if wx then
            local direction = normalizedDirection(wx - S.gizmo.origin.x, wy - S.gizmo.origin.y,
                wz - S.gizmo.origin.z)
            local radial = direction and math.sqrt((wx - S.gizmo.origin.x)^2 +
                (wy - S.gizmo.origin.y)^2 + (wz - S.gizmo.origin.z)^2) or 0
            local error = math.abs(radial - S.gizmo.radius)
            if direction and error <= tolerance and (not bestError or error < bestError) then
                best = {name = name, axis = axis, direction = direction}
                bestError = error
            end
        end
    end
    return best
end

local function pickSourceBone(screenX, screenY)
    if S.definition.sourceType ~= 'bone' or not S.boneSelectionMode then return nil end
    local bestName, bestDistance
    local threshold = math.max(S.source.previewSize * 0.04, 2)
    for name, visual in pairs(S.source.boneVisuals) do
        local first = transformSourcePoint(visual.head)
        local second = transformSourcePoint(visual.tail)
        local distance = raySegmentDistance(screenX, screenY, first, second)
        if distance <= threshold and (not bestDistance or distance < bestDistance) then
            bestName, bestDistance = name, distance
        end
    end
    return bestName
end

local function rayTriangleHit(ox, oy, oz, dx, dy, dz, triangle)
    local a, b, c = triangle.a, triangle.b, triangle.c
    local e1x, e1y, e1z = b.x - a.x, b.y - a.y, b.z - a.z
    local e2x, e2y, e2z = c.x - a.x, c.y - a.y, c.z - a.z
    local px, py, pz = dy * e2z - dz * e2y, dz * e2x - dx * e2z, dx * e2y - dy * e2x
    local determinant = e1x * px + e1y * py + e1z * pz
    if math.abs(determinant) < 1e-9 then return nil end
    local inverse = 1 / determinant
    local tx, ty, tz = ox - a.x, oy - a.y, oz - a.z
    local u = (tx * px + ty * py + tz * pz) * inverse
    if u < 0 or u > 1 then return nil end
    local qx, qy, qz = ty * e1z - tz * e1y, tz * e1x - tx * e1z, tx * e1y - ty * e1x
    local v = (dx * qx + dy * qy + dz * qz) * inverse
    if v < 0 or u + v > 1 then return nil end
    local distance = (e2x * qx + e2y * qy + e2z * qz) * inverse
    return distance > 1e-7 and distance or nil
end

local function rayHitsBounds(ox, oy, oz, dx, dy, dz, bounds)
    local near, far = -math.huge, math.huge
    local function slab(origin, direction, minimum, maximum)
        if math.abs(direction) < 1e-9 then
            return origin >= minimum and origin <= maximum
        end
        local first, second = (minimum - origin) / direction, (maximum - origin) / direction
        if first > second then first, second = second, first end
        near, far = math.max(near, first), math.min(far, second)
        return near <= far
    end
    return slab(ox, dx, bounds.minX, bounds.maxX) and slab(oy, dy, bounds.minY, bounds.maxY) and
        slab(oz, dz, bounds.minZ, bounds.maxZ) and far >= 0
end

local function pickArticulatedPart(screenX, screenY)
    if S.definition.sourceType ~= 'articulated_part' or not S.partSelectionMode then return nil end
    if not buildArticulatedPartPickGeometry() then return nil end
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, screenX, screenY)
    if not ok then return nil end
    local bestName, bestDistance
    for _, part in ipairs(S.source.articulatedPartPickGeometry.parts) do
        if rayHitsBounds(ox, oy, oz, dx, dy, dz, part.bounds) then
            for _, triangle in ipairs(part.triangles) do
                local distance = rayTriangleHit(ox, oy, oz, dx, dy, dz, triangle)
                if distance and (not bestDistance or distance < bestDistance) then
                    bestName, bestDistance = part.name, distance
                end
            end
        end
    end
    return bestName
end

local function beginViewportInteraction(key, screenX, screenY)
    if tImGui.GetWantCaptureMouse() then return end
    if key == 0 then
        local pickedPart = pickArticulatedPart(screenX, screenY)
        if pickedPart then
            local preservedTargetPose = clonePose(S.target.pose)
            S.selectedRole = 'source'
            S.definition.sourcePart = pickedPart
            captureOffset(preservedTargetPose)
            S.target.manualOverride = false
            markModified()
            updatePartSelectionMarker()
            return
        end
        if S.partSelectionMode then
            S.cameraDrag = {kind = 'orbit', startX = screenX, startY = screenY}
            return
        end
        local aimHandle = pickAimLimitsHandle(screenX, screenY)
        if aimHandle then
            local source = getActiveTargetSourcePose() or S.source.pose
            local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
            local offsetRotation = S.definition.offset.rotationMatrix or
                eulerRotation(S.definition.offset.rotation.x, S.definition.offset.rotation.y,
                    S.definition.offset.rotation.z)
            local baseRotation = multiplyRotation(offsetRotation, sourceRotation)
            local origin = targetPivotWorldPosition(S.target.pose)
            local field = aimHandle == 'yaw_min' and 'yawMin' or aimHandle == 'yaw_max' and 'yawMax' or
                aimHandle == 'pitch_min' and 'pitchMin' or 'pitchMax'
            S.gizmo.drag = {
                mode = 'aim_limit', handle = aimHandle, field = field, origin = origin,
                baseRotation = baseRotation,
                planeNormal = aimHandle:find('yaw') and rotateVector(baseRotation, vector(0, 1, 0)) or
                    rotateVector(baseRotation, vector(1, 0, 0)),
            }
            S.cameraDrag = nil
            return
        end
        local ring = pickRotationRing(screenX, screenY)
        if ring then
            local entry = roleEntry(S.selectedRole)
            local drag = {
                mode = S.transformMode, axis = ring.axis, origin = {x = S.gizmo.origin.x, y = S.gizmo.origin.y,
                    z = S.gizmo.origin.z}, startDirection = ring.direction, base = clonePose(entry.pose),
                lastAngle = 0, accumulatedAngle = 0,
                lastEuler = {rx = entry.pose.rx, ry = entry.pose.ry, rz = entry.pose.rz},
            }
            S.gizmo.drag = drag
            S.cameraDrag = nil
            return
        end
        local axisName = pickGizmoAxis(screenX, screenY)
        if axisName then
            local axis = ({x = {x = 1, y = 0, z = 0}, y = {x = 0, y = 1, z = 0},
                z = {x = 0, y = 0, z = 1}})[axisName]
            local parameter = rayAxisParameter(screenX, screenY, S.gizmo.origin, axis)
            if parameter then
                if S.transformMode == 'aim_point' then
                    local source = getActiveTargetSourcePose() or S.source.pose
                    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
                    S.gizmo.drag = {
                        mode = 'aim_point', axis = axis,
                        origin = {x = S.gizmo.origin.x, y = S.gizmo.origin.y, z = S.gizmo.origin.z},
                        startParameter = parameter,
                        baseAimPoint = vector(S.definition.aimPoint.x, S.definition.aimPoint.y,
                            S.definition.aimPoint.z),
                        sourceRotation = cloneRotation(sourceRotation),
                        sourceScale = vector(source.sx, source.sy, source.sz),
                    }
                    S.cameraDrag = nil
                    return
                end
                local drag = {mode = S.transformMode, axis = axis,
                    origin = {x = S.gizmo.origin.x, y = S.gizmo.origin.y, z = S.gizmo.origin.z},
                    startParameter = parameter, base = clonePose(roleEntry(S.selectedRole).pose)}
                if S.transformMode == 'pivot' then
                    drag.basePivot = vector(S.definition.targetPivot.x, S.definition.targetPivot.y,
                        S.definition.targetPivot.z)
                end
                S.gizmo.drag = drag
                S.cameraDrag = nil
                return
            end
        end
        local pickedBone = pickSourceBone(screenX, screenY)
        if pickedBone then
            local preservedTargetPose = clonePose(S.target.pose)
            S.selectedRole = 'source'
            S.source.selectedBone = pickedBone
            S.definition.sourceType = 'bone'
            S.definition.sourceBone = pickedBone
            captureOffset(preservedTargetPose)
            S.target.manualOverride = false
            markModified()
            return
        end
        local picked, pickedTargetIndex = pickRole(screenX, screenY)
        if picked then
            if picked == 'target' and pickedTargetIndex then selectTarget(pickedTargetIndex) end
            S.selectedRole = picked
            updateGizmo()
            return
        end
        S.cameraDrag = {kind = 'orbit', startX = screenX, startY = screenY}
    elseif key == 1 then
        S.cameraDrag = {kind = 'pan', startX = screenX, startY = screenY}
    end
end

local function updateViewportInteraction(screenX, screenY)
    local drag = S.gizmo.drag
    if drag then
        if drag.mode == 'aim_limit' then
            local wx, wy, wz = rayPlaneHit(screenX, screenY, drag.origin, drag.planeNormal)
            if wx then
                local localPoint = rotateVector(transposeRotation(drag.baseRotation), vector(
                    wx - drag.origin.x, wy - drag.origin.y, wz - drag.origin.z))
                local angle
                if drag.handle:find('yaw') then
                    angle = math.atan(localPoint.x, localPoint.z)
                    angle = unwrapAngle(angle, S.definition.aimLimits[drag.field])
                else
                    angle = math.atan(localPoint.y, localPoint.z)
                    angle = math.max(-math.pi * 0.5, math.min(math.pi * 0.5, angle))
                end
                local limits = S.definition.aimLimits
                if drag.handle == 'yaw_min' then
                    limits.yawMin = math.min(angle, limits.yawMax)
                elseif drag.handle == 'yaw_max' then
                    limits.yawMax = math.max(angle, limits.yawMin)
                elseif drag.handle == 'pitch_min' then
                    limits.pitchMin = math.min(angle, limits.pitchMax)
                else
                    limits.pitchMax = math.max(angle, limits.pitchMin)
                end
                S.target.manualOverride = false
                markModified()
            end
            return
        end
        if drag.mode == 'aim_point' then
            local parameter = rayAxisParameter(screenX, screenY, drag.origin, drag.axis)
            if parameter then
                local amount = parameter - drag.startParameter
                local worldDelta = vector(drag.axis.x * amount, drag.axis.y * amount,
                    drag.axis.z * amount)
                local localDelta = rotateVector(transposeRotation(drag.sourceRotation), worldDelta)
                if math.abs(drag.sourceScale.x) > 1e-6 then localDelta.x = localDelta.x / drag.sourceScale.x end
                if math.abs(drag.sourceScale.y) > 1e-6 then localDelta.y = localDelta.y / drag.sourceScale.y end
                if math.abs(drag.sourceScale.z) > 1e-6 then localDelta.z = localDelta.z / drag.sourceScale.z end
                S.definition.aimPoint.x = drag.baseAimPoint.x + localDelta.x
                S.definition.aimPoint.y = drag.baseAimPoint.y + localDelta.y
                S.definition.aimPoint.z = drag.baseAimPoint.z + localDelta.z
                S.target.manualOverride = false
                markModified()
            end
            return
        end
        local entry = roleEntry(S.selectedRole)
        if drag.mode == 'move' then
            local parameter = rayAxisParameter(screenX, screenY, drag.origin, drag.axis)
            if parameter then
                local amount = parameter - drag.startParameter
                entry.pose.x = drag.base.x + drag.axis.x * amount
                entry.pose.y = drag.base.y + drag.axis.y * amount
                entry.pose.z = drag.base.z + drag.axis.z * amount
                if S.selectedRole == 'target' then S.target.manualOverride = true
                else S.target.manualOverride = false end
                markModified()
            end
        elseif drag.mode == 'pivot' then
            local parameter = rayAxisParameter(screenX, screenY, drag.origin, drag.axis)
            if parameter then
                local amount = parameter - drag.startParameter
                local targetRotation = eulerRotation(drag.base.rx, drag.base.ry, drag.base.rz)
                local localDelta = rotateVector(transposeRotation(targetRotation), vector(
                    drag.axis.x * amount, drag.axis.y * amount, drag.axis.z * amount))
                if math.abs(drag.base.sx) > 1e-6 then localDelta.x = localDelta.x / drag.base.sx end
                if math.abs(drag.base.sy) > 1e-6 then localDelta.y = localDelta.y / drag.base.sy end
                if math.abs(drag.base.sz) > 1e-6 then localDelta.z = localDelta.z / drag.base.sz end
                S.definition.targetPivot.x = drag.basePivot.x + localDelta.x
                S.definition.targetPivot.y = drag.basePivot.y + localDelta.y
                S.definition.targetPivot.z = drag.basePivot.z + localDelta.z
                S.target.manualOverride = false
                markModified()
            end
        else
            local wx, wy, wz = rayPlaneHit(screenX, screenY, drag.origin, drag.axis)
            local current = wx and normalizedDirection(wx - drag.origin.x, wy - drag.origin.y,
                wz - drag.origin.z) or nil
            if current then
                local start = drag.startDirection
                local cross = {x = start.y * current.z - start.z * current.y,
                    y = start.z * current.x - start.x * current.z,
                    z = start.x * current.y - start.y * current.x}
                local sinAngle = dot(drag.axis.x, drag.axis.y, drag.axis.z, cross.x, cross.y, cross.z)
                local cosAngle = math.max(-1, math.min(1, dot(start.x, start.y, start.z,
                    current.x, current.y, current.z)))
                local angle = math.atan(sinAngle, cosAngle)
                local delta = angle - drag.lastAngle
                if delta > math.pi then delta = delta - math.pi * 2
                elseif delta < -math.pi then delta = delta + math.pi * 2 end
                drag.accumulatedAngle = drag.accumulatedAngle + delta
                drag.lastAngle = angle
                local baseRotation = eulerRotation(drag.base.rx, drag.base.ry, drag.base.rz)
                local rotated = multiplyRotation(baseRotation, axisRotation(drag.axis, drag.accumulatedAngle))
                local rx, ry, rz = matrixToEuler(rotated)
                entry.pose.rx = unwrapAngle(rx, drag.lastEuler.rx)
                entry.pose.ry = unwrapAngle(ry, drag.lastEuler.ry)
                entry.pose.rz = unwrapAngle(rz, drag.lastEuler.rz)
                drag.lastEuler = {rx = entry.pose.rx, ry = entry.pose.ry, rz = entry.pose.rz}
                if S.selectedRole == 'target' then S.target.manualOverride = true
                else S.target.manualOverride = false end
                markModified()
            end
        end
        return
    end
    local cameraDrag = S.cameraDrag
    if not cameraDrag or tImGui.GetWantCaptureMouse() then return end
    local dx, dy = screenX - cameraDrag.startX, screenY - cameraDrag.startY
    if cameraDrag.kind == 'orbit' then
        S.cameraOrbit.azimuth = S.cameraOrbit.azimuth + dx * 0.008
        S.cameraOrbit.elevation = math.max(-1.45, math.min(1.45, S.cameraOrbit.elevation + dy * 0.008))
    else
        local right = S.camera:getNormal('R')
        local scale = S.cameraOrbit.distance * 0.0015
        S.cameraOrbit.fx = S.cameraOrbit.fx - right.x * dx * scale
        S.cameraOrbit.fy = S.cameraOrbit.fy + dy * scale
        S.cameraOrbit.fz = S.cameraOrbit.fz - right.z * dx * scale
    end
    cameraDrag.startX, cameraDrag.startY = screenX, screenY
    applyCamera()
end

local function endViewportInteraction(key)
    if key == 0 then S.gizmo.drag = nil end
    if key == 0 or key == 1 then S.cameraDrag = nil end
end

local function calculateTargetPose(delta, sourceOverride)
    local source, target = sourceOverride or getActiveTargetSourcePose() or S.source.pose, S.target.pose
    local result = clonePose(target)
    if not previewInfluenceEnabled() then return result end
    local inherit = S.definition.inherit
    local offset = S.definition.offset
    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
    local targetRotation = eulerRotation(target.rx, target.ry, target.rz)
    if inherit.rotation then
        local offsetRotation = S.definition.offset.rotationMatrix or
            eulerRotation(offset.rotation.x, offset.rotation.y, offset.rotation.z)
        targetRotation = multiplyRotation(offsetRotation,
            sourceRotation)
    end
    if inherit.scale then
        result.sx = source.sx * offset.scale.x
        result.sy = source.sy * offset.scale.y
        result.sz = source.sz * offset.scale.z
    end
    if inherit.position then
        local localPosition = vector(offset.position.x, offset.position.y, offset.position.z)
        if inherit.scale then
            localPosition.x = localPosition.x * source.sx
            localPosition.y = localPosition.y * source.sy
            localPosition.z = localPosition.z * source.sz
        end
        local worldPosition = rotateVector(sourceRotation, localPosition)
        result.x = source.x + worldPosition.x
        result.y = source.y + worldPosition.y
        result.z = source.z + worldPosition.z
    end
    if S.definition.aimAtSource then
        local aimPoint = sourceAimPointWorldPosition(source)
        local limits = S.definition.aimLimits
        local referenceRotation = limits and limits.enabled and aimBaseRotation(source) or targetRotation
        targetRotation = limits and limits.enabled
            and limitedLookAtRotation(result, aimPoint, referenceRotation,
                targetRotation, limits)
            or lookAtRotation(result, aimPoint, targetRotation)
    end
    if inherit.rotation or S.definition.aimAtSource then
        result.rx, result.ry, result.rz = matrixToEuler(targetRotation)
    end
    if inherit.position then
        local pivot = vector(S.definition.targetPivot.x * result.sx,
            S.definition.targetPivot.y * result.sy, S.definition.targetPivot.z * result.sz)
        local worldPivot = rotateVector(targetRotation, pivot)
        result.x = result.x - worldPivot.x
        result.y = result.y - worldPivot.y
        result.z = result.z - worldPivot.z
    end
    local blend = previewBlend()
    if blend and blend.enabled then
        local weight = math.min(math.max(tonumber(blend.weight) or 1, 0), 1)
        if blend.position and inherit.position then
            result.x = blendScalar(target.x, result.x, weight)
            result.y = blendScalar(target.y, result.y, weight)
            result.z = blendScalar(target.z, result.z, weight)
        end
        if blend.rotation and (inherit.rotation or S.definition.aimAtSource) then
            result.rx = blendAngle(target.rx, result.rx, weight)
            result.ry = blendAngle(target.ry, result.ry, weight)
            result.rz = blendAngle(target.rz, result.rz, weight)
        end
        if blend.scale and inherit.scale then
            result.sx = blendScalar(target.sx, result.sx, weight)
            result.sy = blendScalar(target.sy, result.sy, weight)
            result.sz = blendScalar(target.sz, result.sz, weight)
        end
    end
    local spring = S.definition.spring
    if spring and spring.enabled then
        local step = math.min(math.max(tonumber(delta) or 0, 0), 0.1)
        if not S.springState.initialized then
            S.springState.position = vector(target.x, target.y, target.z)
            S.springState.rotation = vector(target.rx, target.ry, target.rz)
            S.springState.initialized = true
        end
        if spring.position and inherit.position then
            S.springState.position.x, S.springState.positionVelocity.x = springScalar(
                S.springState.position.x, S.springState.positionVelocity.x, result.x,
                spring.stiffness, spring.damping, step)
            S.springState.position.y, S.springState.positionVelocity.y = springScalar(
                S.springState.position.y, S.springState.positionVelocity.y, result.y,
                spring.stiffness, spring.damping, step)
            S.springState.position.z, S.springState.positionVelocity.z = springScalar(
                S.springState.position.z, S.springState.positionVelocity.z, result.z,
                spring.stiffness, spring.damping, step)
            result.x, result.y, result.z = S.springState.position.x, S.springState.position.y,
                S.springState.position.z
        end
        if spring.rotation and (inherit.rotation or S.definition.aimAtSource) then
            S.springState.rotation.x, S.springState.rotationVelocity.x = springAngle(
                S.springState.rotation.x, S.springState.rotationVelocity.x, result.rx,
                spring.stiffness, spring.damping, step)
            S.springState.rotation.y, S.springState.rotationVelocity.y = springAngle(
                S.springState.rotation.y, S.springState.rotationVelocity.y, result.ry,
                spring.stiffness, spring.damping, step)
            S.springState.rotation.z, S.springState.rotationVelocity.z = springAngle(
                S.springState.rotation.z, S.springState.rotationVelocity.z, result.rz,
                spring.stiffness, spring.damping, step)
            result.rx, result.ry, result.rz = S.springState.rotation.x, S.springState.rotation.y,
                S.springState.rotation.z
        end
    else
        resetSpringState()
    end
    return result
end

targetPivotWorldPosition = function(target)
    local targetRotation = eulerRotation(target.rx, target.ry, target.rz)
    local localPivot = vector(S.definition.targetPivot.x * target.sx,
        S.definition.targetPivot.y * target.sy, S.definition.targetPivot.z * target.sz)
    local worldPivot = rotateVector(targetRotation, localPivot)
    return vector(target.x + worldPivot.x, target.y + worldPivot.y, target.z + worldPivot.z)
end

sourceAimPointWorldPosition = function(source)
    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
    local localPoint = vector(S.definition.aimPoint.x * source.sx,
        S.definition.aimPoint.y * source.sy, S.definition.aimPoint.z * source.sz)
    local worldPoint = rotateVector(sourceRotation, localPoint)
    return vector(source.x + worldPoint.x, source.y + worldPoint.y, source.z + worldPoint.z)
end

updateAimPointMarker = function()
    if not S.aimPointMarker then return end
    local source = getActiveTargetSourcePose() or S.source.pose
    local point = sourceAimPointWorldPosition(source)
    local handleSize = getAimPointHandleSize()
    S.aimPointMarker:setPos(point.x, point.y, point.z)
    S.aimPointMarker:setScale(handleSize, handleSize, handleSize)
    S.aimPointMarker.visible = not isSourceSelectionMode() and S.definition.aimAtSource
end

updateTargetPivotMarker = function()
    if not S.targetPivotMarker then return end
    local targetPivot = targetPivotWorldPosition(S.target.pose)
    S.targetPivotMarker:setPos(targetPivot.x, targetPivot.y, targetPivot.z)
    local radius = getTargetPivotMarkerRadius()
    S.targetPivotMarker:setScale(radius, radius, radius)
    S.targetPivotMarker.visible = not isSourceSelectionMode() and
        (S.selectedRole == 'target' or S.transformMode == 'pivot' or
         math.abs(S.definition.targetPivot.x) > 1e-6 or
         math.abs(S.definition.targetPivot.y) > 1e-6 or
         math.abs(S.definition.targetPivot.z) > 1e-6)
end

updatePartSelectionMarker = function()
    local marker = S.partSelectionMarker
    if not marker then return end
    if not S.partSelectionMode or S.definition.sourceType ~= 'articulated_part' or
            not buildArticulatedPartPickGeometry() then
        marker.visible = false
        return
    end
    local selectedPart = S.definition.sourcePart
    for _, part in ipairs(S.source.articulatedPartPickGeometry.parts) do
        if part.name == selectedPart then
            local bounds = part.bounds
            local extent = math.max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY,
                bounds.maxZ - bounds.minZ)
            local radius = math.max(extent * 0.025, 0.05)
            marker:setPos(bounds.centerX, bounds.centerY, bounds.centerZ)
            marker:setScale(radius, radius, radius)
            marker.visible = true
            return
        end
    end
    marker.visible = false
end

local function evaluateTargetPreview(index, delta, visiting, evaluated, errors)
    if evaluated[index] then return evaluated[index].pose, evaluated[index].sourcePose end
    if visiting[index] then errors.cycle = true; return nil, nil, true end
    visiting[index] = true
    local record = S.definition.targets[index]
    activateTargetDefinition(index)
    local sourceName = record.source or S.definition.source
    local sourcePose
    if sourceName == S.definition.source then
        sourcePose = getPreviewSourcePose() or S.source.pose
    else
        local sourceIndex = targetIndexByName(sourceName)
        if sourceIndex then
            sourcePose = evaluateTargetPreview(sourceIndex, delta, visiting, evaluated, errors)
        else
            errors.invalid = true
        end
    end
    activateTargetDefinition(index)
    if not sourcePose then
        errors.invalid = true
        sourcePose = getPreviewSourcePose() or S.source.pose
    end
    if not S.target.manualOverride then S.target.pose = calculateTargetPose(delta, sourcePose) end
    applyMarker('target', S.target.pose)
    local relation = ensureRelationLine(index)
    local targetPivot = targetPivotWorldPosition(S.target.pose)
    relation:set({sourcePose.x, sourcePose.y, sourcePose.z,
        targetPivot.x, targetPivot.y, targetPivot.z}, 1)
    relation.visible = not isSourceSelectionMode()
    syncActiveTargetDefinition()
    visiting[index] = nil
    evaluated[index] = {pose = clonePose(S.target.pose), sourcePose = clonePose(sourcePose)}
    return evaluated[index].pose, evaluated[index].sourcePose
end

local function rebuildPreview(delta)
    ensureTargetDefinitions()
    local selectedTargetIndex = S.selectedTargetIndex
    syncActiveTargetDefinition()
    applyMarker('source', S.source.pose)
    updateSourceSkeletonVisuals()
    local visiting, evaluated, errors = {}, {}, {}
    for index = 1, #S.definition.targets do
        evaluateTargetPreview(index, delta, visiting, evaluated, errors)
    end
    local chainMessage = errors.cycle and L('target_cycle') or errors.invalid and L('target_source_invalid') or ''
    if chainMessage ~= S.chainStatus then
        S.chainStatus = chainMessage
        if chainMessage ~= '' then setStatus(chainMessage, true) end
    end
    for index = #S.definition.targets + 1, #S.relationLines do
        S.relationLines[index].visible = false
    end
    activateTargetDefinition(selectedTargetIndex)
    applyMarker('target', S.target.pose)
    updateTargetPivotMarker()
    updateAimPointMarker()
    updatePartSelectionMarker()
    S.dirtyPreview = false
end

captureOffset = function(targetPoseOverride)
    local source = getActiveTargetSourcePose() or S.source.pose
    local target = targetPoseOverride or S.target.pose
    local inherit = S.definition.inherit
    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
    local inverseSource = transposeRotation(sourceRotation)
    local targetAnchor = targetPivotWorldPosition(target)
    local delta = vector(targetAnchor.x - source.x, targetAnchor.y - source.y,
        targetAnchor.z - source.z)
    local localPosition = rotateVector(inverseSource, delta)
    if inherit.scale then
        if math.abs(source.sx) < 1e-6 or math.abs(source.sy) < 1e-6 or math.abs(source.sz) < 1e-6 then
            setStatus(L('zero_scale_capture'), true)
            return
        end
        localPosition.x = localPosition.x / source.sx
        localPosition.y = localPosition.y / source.sy
        localPosition.z = localPosition.z / source.sz
    end
    local targetRotation = eulerRotation(target.rx, target.ry, target.rz)
    local relativeRotation = multiplyRotation(targetRotation, inverseSource)
    local rx, ry, rz = matrixToEuler(relativeRotation)
    S.definition.offset.position = localPosition
    S.definition.offset.rotation = vector(rx, ry, rz)
    S.definition.offset.rotationMatrix = cloneRotation(relativeRotation)
    captureAimReference(source)
    if inherit.scale then
        S.definition.offset.scale = vector(target.sx / source.sx, target.sy / source.sy, target.sz / source.sz)
    end
    resetSpringState()
    S.target.manualOverride = false
    markModified()
    setStatus(L('captured'))
end

local function resetTargetPreview()
    S.target.manualOverride = false
    S.dirtyPreview = true
    rebuildPreview()
    updateGizmo()
    setStatus(L('preview_reset'))
end

local function syncEditorDefinition()
    syncActiveTargetDefinition()
    S.definition.editor = {
        sourceKind = S.source.kind,
        targetKind = S.target.kind,
        sourceMesh = S.source.path,
        targetMesh = S.target.path,
        sourcePose = clonePose(S.source.pose),
        targetPose = clonePose(S.target.pose),
        targetManualOverride = S.target.manualOverride == true,
        language = tLang.getLanguage(),
        sourceSelectable = S.source.selectable ~= false,
        targetSelectable = S.target.selectable ~= false,
        sourceAnimation = S.source.activeAnimation ~= '' and S.source.activeAnimation or
            (S.source.animationClips[S.source.selectedAnimation] or ''),
        sourceAnimationPaused = S.source.animationPaused == true,
    }
end

local function saveDefinition(path)
    if not path or path == '' then path = mbm.saveFile(S.projectPath or '', 'att.lua') end
    if not path then return end
    if S.definition.sourceType == 'socket' and S.definition.sourceSocket == '' then
        setStatus(L('no_sockets'), true)
        return
    end
    syncEditorDefinition()
    local output = {'attachmentDefinition = {}'}
    tUtil.save('attachmentDefinition', S.definition, output)
    local file, errorMessage = io.open(path, 'w')
    if not file then setStatus(errorMessage or L('save_failed'), true); return end
    for _, line in ipairs(output) do file:write(line .. '\n') end
    file:close()
    S.projectPath, S.modified = path, false
    setStatus(string.format(L('saved'), path))
end

local function saveDefinitionAs()
    local path = mbm.saveFile(S.projectPath or '', 'att.lua')
    if path then saveDefinition(path) end
end

local function runtimeExportDefaultPath()
    if not S.projectPath or S.projectPath == '' then return '' end
    local base = S.projectPath:gsub('%.att%.lua$', '')
    if base == S.projectPath then base = S.projectPath:gsub('%.lua$', '') end
    return base .. '.runtime.lua'
end

local function exportRuntimeDefinition()
    syncEditorDefinition()
    local graph = validateAttachmentGraph()
    if not graph.valid then
        setStatus(L(graph.errorKey), true)
        return
    end
    local path = mbm.saveFile(runtimeExportDefaultPath(), 'runtime.lua')
    if not path then return end
    local ok, errorMessage = tRuntimeExport.write(path, S.definition)
    if not ok then
        setStatus(errorMessage or L('runtime_export_failed'), true)
        return
    end
    setStatus(string.format(L('runtime_exported'), path))
end

local function previewDefaultPath()
    if not S.projectPath or S.projectPath == '' then return 'attachment-preview.lua' end
    local base = S.projectPath:gsub('%.att%.lua$', '')
    if base == S.projectPath then base = S.projectPath:gsub('%.lua$', '') end
    return base .. '-preview.lua'
end

local function previewDriverSource(projectPath)
    local projectLiteral = string.format('%q', projectPath)
    return [[
local project = dofile(]] .. projectLiteral .. [[)
local definition = project.definition
local attachments = require 'attachments'
local source
local targets = {}
local targetsByName = {}
local previewRelations = nil

local function cubeVertices()
    return {
        -0.5, -0.5, 0.5,  -0.5, 0.5, 0.5,  0.5, 0.5, 0.5,
        -0.5, -0.5, 0.5,   0.5, 0.5, 0.5,  0.5, -0.5, 0.5,
        0.5, -0.5, -0.5,   0.5, 0.5, -0.5, -0.5, 0.5, -0.5,
        0.5, -0.5, -0.5,  -0.5, 0.5, -0.5, -0.5, -0.5, -0.5,
        -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, 0.5, 0.5,
        -0.5, -0.5, -0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5,
        0.5, -0.5, 0.5,   0.5, 0.5, 0.5,  0.5, 0.5, -0.5,
        0.5, -0.5, 0.5,   0.5, 0.5, -0.5, 0.5, -0.5, -0.5,
        -0.5, 0.5, 0.5,  -0.5, 0.5, -0.5, 0.5, 0.5, -0.5,
        -0.5, 0.5, 0.5,   0.5, 0.5, -0.5, 0.5, 0.5, 0.5,
        -0.5, -0.5, -0.5,  0.5, -0.5, -0.5, 0.5, -0.5, 0.5,
        -0.5, -0.5, -0.5,  0.5, -0.5, 0.5, -0.5, -0.5, 0.5,
    }
end

local function loadRole(role, editor)
    local roleKey = role == 'source' and 'source' or 'target'
    local pose = editor[roleKey .. 'Pose'] or {}
    local object
    if editor[roleKey .. 'Kind'] == 'mesh' and editor[roleKey .. 'Mesh'] and editor[roleKey .. 'Mesh'] ~= '' then
        local path = editor[roleKey .. 'Mesh']
        local folder = path:match('^(.*)[/\\][^/\\]+$')
        if folder then mbm.addPath(folder) end
        object = mesh:new('3d')
        assert(object:load(path), 'Could not load attachment preview mesh: ' .. tostring(path))
    else
        object = shape:new('3d', 0, 0, 0)
        assert(object:create(cubeVertices(), nil, 'attachment_preview_' .. role))
        if role == 'source' then object:setColor(0.15, 0.75, 1.0, 0.42)
        else object:setColor(1.0, 0.65, 0.15, 0.42) end
    end
    object.alwaysRender = true
    object:setPos(pose.x or 0, pose.y or 0, pose.z or 0)
    object:setAngle(pose.rx or 0, pose.ry or 0, pose.rz or 0)
    object:setScale(pose.sx or 1, pose.sy or 1, pose.sz or 1)
    return object
end

function onInitScene()
    mbm.setAmbientLight('3d', {r = 0.25, g = 0.25, b = 0.25, a = 1})
    mbm.setDirectionalLight('3d', {x = -0.8, y = -0.7, z = -0.5},
        {r = 1, g = 0.98, b = 0.9, a = 1})
    source = loadRole('source', definition.editor)
    for index, targetDefinition in ipairs(definition.targets or {}) do
        targets[index] = loadRole('target_' .. index, targetDefinition.editor or {})
        targetsByName[targetDefinition.name] = targets[index]
    end
    local camera = mbm.getCamera('3d')
    local orbit = project.camera or {azimuth = 0.3, elevation = 0.3, distance = 700,
        fx = 50, fy = 30, fz = 0}
    local cameraX = orbit.fx + orbit.distance * math.cos(orbit.elevation) * math.sin(orbit.azimuth)
    local cameraY = orbit.fy + orbit.distance * math.sin(orbit.elevation)
    local cameraZ = orbit.fz + orbit.distance * math.cos(orbit.elevation) * math.cos(orbit.azimuth)
    camera:setPos(cameraX, cameraY, cameraZ)
    camera:setFocus(orbit.fx, orbit.fy, orbit.fz)
    local clip = definition.editor.sourceAnimation
    if clip and clip ~= '' then
        if definition.sourceType == 'articulated_part' then
            assert(source:playArticulatedAnimation(clip))
        else
            assert(source:playSkeletalAnimation(clip))
        end
    end
    if definition.editor.sourceAnimationPaused and clip and clip ~= '' then
        if definition.sourceType == 'articulated_part' then
            assert(source:pauseArticulatedAnimation(clip))
        else
            assert(source:pauseSkeletalAnimation())
        end
    end
end

function onLoop()
    if not previewRelations then
        if definition.sourceType == 'bone' or definition.sourceType == 'socket' or
                definition.sourceType == 'articulated_part' then
            for _, targetDefinition in ipairs(definition.targets or {}) do
                local sourceName = targetDefinition.source or definition.source
                if sourceName == definition.source then
                    local valid = definition.sourceType == 'bone' and
                        source:getSkeletalBoneTransform(targetDefinition.sourceBone or definition.sourceBone, 'world') or
                        definition.sourceType == 'socket' and
                        source:getSkeletalSocketTransform(targetDefinition.sourceSocket or definition.sourceSocket, 'world') or
                        source:getArticulatedPartTransform(targetDefinition.sourcePart or definition.sourcePart, 'world')
                    if not valid then return end
                end
            end
        end
        previewRelations = {}
        for index, targetDefinition in ipairs(definition.targets or {}) do
            local sourceName = targetDefinition.source or definition.source
            local relationSource = sourceName == definition.source and source or targetsByName[sourceName]
            assert(relationSource, 'Could not resolve attachment source: ' ..
                tostring(sourceName))
            local usesOriginalSource = sourceName == definition.source
            local specification = {
                source = relationSource,
                target = targets[index],
                sourceType = usesOriginalSource and definition.sourceType or 'root',
                inherit = targetDefinition.inherit,
                aimAtSource = targetDefinition.aimAtSource == true,
                aimPoint = targetDefinition.aimPoint,
                influenceEnabled = definition.influenceEnabled ~= false,
                initialState = definition.initialState,
                states = definition.states,
                aimLimits = targetDefinition.aimLimits,
                spring = targetDefinition.spring,
                blend = targetDefinition.blend,
                offset = targetDefinition.offset,
                targetPivot = targetDefinition.targetPivot,
            }
            if usesOriginalSource and targetDefinition.sourceBone and targetDefinition.sourceBone ~= '' then
                specification.sourceBone = targetDefinition.sourceBone
            end
            if usesOriginalSource and targetDefinition.sourceSocket and targetDefinition.sourceSocket ~= '' then
                specification.sourceSocket = targetDefinition.sourceSocket
            end
            if usesOriginalSource and targetDefinition.sourcePart and targetDefinition.sourcePart ~= '' then
                specification.sourcePart = targetDefinition.sourcePart
            end
            local relation, errorMessage = attachments.attach(specification)
            assert(relation, errorMessage)
            previewRelations[index] = relation
        end
    end
end

function onKeyDown(key)
    if key == mbm.getKeyCode('ESC') then mbm.quit() end
end
]]
end

local function writePreviewFiles(projectPath, driverPath)
    syncEditorDefinition()
    local output = {'local attachmentPreview = {}'}
    tUtil.save('attachmentPreview', {definition = S.definition,
        camera = tUtil.deepCopyTable(S.cameraOrbit)}, output)
    output[#output + 1] = 'return attachmentPreview'
    local project, projectError = io.open(projectPath, 'w')
    if not project then setStatus(tostring(projectError), true); return false end
    for _, line in ipairs(output) do project:write(line .. '\n') end
    project:close()
    local driver, driverError = io.open(driverPath, 'w')
    if not driver then setStatus(tostring(driverError), true); return false end
    driver:write(previewDriverSource(projectPath))
    driver:close()
    return true
end

local function validatePreview()
    local graph = validateAttachmentGraph()
    if not graph.valid then
        setStatus(L(graph.errorKey), true)
        return false
    end
    if S.source.kind ~= 'mesh' or not S.source.meshLoaded then
        setStatus(L('run_preview_requires_source'), true)
        return false
    end
    if (S.definition.sourceType == 'bone' or S.definition.sourceType == 'socket' or
            S.definition.sourceType == 'articulated_part') and
            (S.source.activeAnimation == '' and S.source.animationClips[S.source.selectedAnimation] == nil) then
        setStatus(L('run_preview_requires_animation'), true)
        return false
    end
    return true
end

local function startPreview(driverPath)
    local width, height = mbm.getSizeScreen()
    width, height = math.max(320, math.floor(width or 1280)), math.max(240, math.floor(height or 720))
    tUtil.newInstance(width, height, width, height, driverPath)
end

local function runPreview()
    if not validatePreview() then return end
    local projectPath = tUtil.getTemporaryFilePath('_attachments_preview_project.lua')
    local driverPath = tUtil.getTemporaryFilePath('_attachments_preview_main.lua')
    if not writePreviewFiles(projectPath, driverPath) then return end
    startPreview(driverPath)
    setStatus(L('run_preview_started'))
end

local function savePreviewAs()
    if not validatePreview() then return end
    local driverPath = mbm.saveFile(previewDefaultPath(), 'lua')
    if not driverPath then return end
    local projectPath = driverPath:gsub('%.lua$', '') .. '.att.lua'
    if not writePreviewFiles(projectPath, driverPath) then return end
    setStatus(string.format(L('preview_saved'), driverPath, projectPath))
end

local function normalizeDefinition(value)
    if type(value) ~= 'table' then return nil end
    local result = newDefinition()
    result.version = math.max(1, math.floor(tonumber(value.version) or 1))
    result.source = tostring(value.source or result.source)
    result.target = tostring(value.target or result.target)
    result.sourceType = value.sourceType == 'bone' and 'bone' or
        (value.sourceType == 'socket' and 'socket' or
            (value.sourceType == 'articulated_part' and 'articulated_part' or 'root'))
    result.sourceBone = type(value.sourceBone) == 'string' and value.sourceBone or ''
    result.sourceSocket = type(value.sourceSocket) == 'string' and value.sourceSocket or ''
    result.sourcePart = type(value.sourcePart) == 'string' and value.sourcePart or ''
    result.aimAtSource = value.aimAtSource == true
    result.influenceEnabled = value.influenceEnabled ~= false
    result.initialState = type(value.initialState) == 'string' and value.initialState or ''
    if type(value.aimPoint) == 'table' then
        for _, axis in ipairs({'x', 'y', 'z'}) do
            result.aimPoint[axis] = tonumber(value.aimPoint[axis]) or 0
        end
    end
    if type(value.aimLimits) == 'table' then
        result.aimLimits.enabled = result.aimAtSource and value.aimLimits.enabled == true
        result.aimLimits.reference = value.aimLimits.reference == 'captured' and 'captured' or 'dynamic'
        if type(value.aimLimits.referenceRotationMatrix) == 'table' then
            result.aimLimits.referenceRotationMatrix = readRotation(
                value.aimLimits.referenceRotationMatrix, identityRotation())
        end
        for _, field in ipairs({'yawMin', 'yawMax', 'pitchMin', 'pitchMax'}) do
            local number = tonumber(value.aimLimits[field])
            if number and number == number and number ~= math.huge and number ~= -math.huge then
                result.aimLimits[field] = number
            end
        end
        if result.aimLimits.yawMin > result.aimLimits.yawMax then
            result.aimLimits.yawMin, result.aimLimits.yawMax = result.aimLimits.yawMax, result.aimLimits.yawMin
        end
        if result.aimLimits.pitchMin > result.aimLimits.pitchMax then
            result.aimLimits.pitchMin, result.aimLimits.pitchMax = result.aimLimits.pitchMax, result.aimLimits.pitchMin
        end
    end
    if type(value.spring) == 'table' then
        result.spring.enabled = value.spring.enabled == true
        result.spring.position = value.spring.position ~= false
        result.spring.rotation = value.spring.rotation ~= false
        result.spring.stiffness = math.max(0, tonumber(value.spring.stiffness) or result.spring.stiffness)
        result.spring.damping = math.max(0, tonumber(value.spring.damping) or result.spring.damping)
    end
    if type(value.blend) == 'table' then
        result.blend.enabled = value.blend.enabled == true
        result.blend.position = value.blend.position ~= false
        result.blend.rotation = value.blend.rotation ~= false
        result.blend.scale = value.blend.scale ~= false
        result.blend.weight = math.min(1, math.max(0, tonumber(value.blend.weight) or result.blend.weight))
    end
    if type(value.states) == 'table' then
        for name, state in pairs(value.states) do
            if type(name) == 'string' and type(state) == 'table' and name ~= '' then
                local profile = {influenceEnabled = state.influenceEnabled ~= false}
                if type(state.blend) == 'table' then
                    profile.blend = {
                        enabled = state.blend.enabled == true,
                        position = state.blend.position ~= false,
                        rotation = state.blend.rotation ~= false,
                        scale = state.blend.scale ~= false,
                        weight = math.min(1, math.max(0, tonumber(state.blend.weight) or 1)),
                    }
                end
                result.states[name] = profile
            end
        end
    end
    if result.initialState ~= '' and not result.states[result.initialState] then
        result.initialState = ''
    end
    if type(value.inherit) == 'table' then
        result.inherit.position = value.inherit.position ~= false
        result.inherit.rotation = value.inherit.rotation ~= false
        result.inherit.scale = value.inherit.scale == true
    end
    if type(value.targetPivot) == 'table' then
        for _, axis in ipairs({'x', 'y', 'z'}) do
            result.targetPivot[axis] = tonumber(value.targetPivot[axis]) or 0
        end
    end
    for _, component in ipairs({'position', 'rotation', 'scale'}) do
        if type(value.offset) == 'table' and type(value.offset[component]) == 'table' then
            for _, axis in ipairs({'x', 'y', 'z'}) do
                local fallback = result.offset[component][axis]
                result.offset[component][axis] = tonumber(value.offset[component][axis]) or fallback
            end
        end
    end
    local fallbackRotation = eulerRotation(result.offset.rotation.x, result.offset.rotation.y,
        result.offset.rotation.z)
    if type(value.offset) == 'table' then
        result.offset.rotationMatrix = readRotation(value.offset.rotationMatrix, fallbackRotation)
    else
        result.offset.rotationMatrix = fallbackRotation
    end
    if type(value.editor) == 'table' then
        local editor = value.editor
        result.editor.sourceKind = editor.sourceKind == 'mesh' and 'mesh' or 'placeholder'
        result.editor.targetKind = editor.targetKind == 'mesh' and 'mesh' or 'placeholder'
        result.editor.sourceMesh = type(editor.sourceMesh) == 'string' and editor.sourceMesh or ''
        result.editor.targetMesh = type(editor.targetMesh) == 'string' and editor.targetMesh or ''
        if type(editor.sourcePose) == 'table' then result.editor.sourcePose = clonePose(editor.sourcePose) end
        if type(editor.targetPose) == 'table' then result.editor.targetPose = clonePose(editor.targetPose) end
        result.editor.targetManualOverride = editor.targetManualOverride == true
        if editor.language == 'pt_br' or editor.language == 'en' then
            result.editor.language = editor.language
        end
        result.editor.sourceSelectable = editor.sourceSelectable ~= false
        result.editor.targetSelectable = editor.targetSelectable ~= false
        result.editor.sourceAnimation = type(editor.sourceAnimation) == 'string' and editor.sourceAnimation or ''
        result.editor.sourceAnimationPaused = editor.sourceAnimationPaused == true
    end
    local function targetRecordFromValue(raw, fallbackName)
        local record = {
            name = tostring(raw.name or fallbackName or result.target),
            source = type(raw.source) == 'string' and raw.source ~= '' and raw.source or result.source,
            sourceBone = type(raw.sourceBone) == 'string' and raw.sourceBone or result.sourceBone,
            sourceSocket = type(raw.sourceSocket) == 'string' and raw.sourceSocket or result.sourceSocket,
            sourcePart = type(raw.sourcePart) == 'string' and raw.sourcePart or result.sourcePart,
            inherit = {
                position = result.inherit.position,
                rotation = result.inherit.rotation,
                scale = result.inherit.scale,
            },
            aimAtSource = result.aimAtSource,
            aimPoint = copyVector(result.aimPoint),
            aimLimits = copyTargetRecord({aimLimits = result.aimLimits}).aimLimits,
            spring = copyTargetRecord({spring = result.spring}).spring,
            blend = cloneBlend(result.blend),
            offset = copyTargetRecord({offset = result.offset}).offset,
            targetPivot = copyVector(result.targetPivot),
            editor = {
                targetKind = result.editor.targetKind,
                targetMesh = result.editor.targetMesh,
                targetPose = clonePose(result.editor.targetPose),
                targetManualOverride = result.editor.targetManualOverride,
                targetSelectable = result.editor.targetSelectable,
            },
        }
        if type(raw.inherit) == 'table' then
            record.inherit.position = raw.inherit.position ~= false
            record.inherit.rotation = raw.inherit.rotation ~= false
            record.inherit.scale = raw.inherit.scale == true
        end
        if raw.aimAtSource ~= nil then record.aimAtSource = raw.aimAtSource == true end
        if type(raw.aimPoint) == 'table' then record.aimPoint = copyVector(raw.aimPoint) end
        if type(raw.aimLimits) == 'table' then
            local limits = raw.aimLimits
            record.aimLimits.enabled = record.aimAtSource and limits.enabled == true
            record.aimLimits.reference = limits.reference == 'captured' and 'captured' or 'dynamic'
            if type(limits.referenceRotationMatrix) == 'table' then
                record.aimLimits.referenceRotationMatrix = readRotation(
                    limits.referenceRotationMatrix, identityRotation())
            end
            for _, field in ipairs({'yawMin', 'yawMax', 'pitchMin', 'pitchMax'}) do
                if tonumber(limits[field]) then record.aimLimits[field] = tonumber(limits[field]) end
            end
        end
        if type(raw.spring) == 'table' then
            record.spring.enabled = raw.spring.enabled == true
            record.spring.position = raw.spring.position ~= false
            record.spring.rotation = raw.spring.rotation ~= false
            record.spring.stiffness = math.max(0, tonumber(raw.spring.stiffness) or record.spring.stiffness)
            record.spring.damping = math.max(0, tonumber(raw.spring.damping) or record.spring.damping)
        end
        if type(raw.blend) == 'table' then record.blend = cloneBlend(raw.blend) end
        if type(raw.offset) == 'table' then
            for _, component in ipairs({'position', 'rotation', 'scale'}) do
                if type(raw.offset[component]) == 'table' then
                    for _, axis in ipairs({'x', 'y', 'z'}) do
                        if tonumber(raw.offset[component][axis]) then
                            record.offset[component][axis] = tonumber(raw.offset[component][axis])
                        end
                    end
                end
            end
            local fallbackRotation = eulerRotation(record.offset.rotation.x, record.offset.rotation.y,
                record.offset.rotation.z)
            record.offset.rotationMatrix = readRotation(raw.offset.rotationMatrix, fallbackRotation)
        end
        if type(raw.targetPivot) == 'table' then record.targetPivot = copyVector(raw.targetPivot) end
        if type(raw.editor) == 'table' then
            local editor = raw.editor
            record.editor.targetKind = editor.targetKind == 'mesh' and 'mesh' or 'placeholder'
            record.editor.targetMesh = type(editor.targetMesh) == 'string' and editor.targetMesh or ''
            if type(editor.targetPose) == 'table' then record.editor.targetPose = clonePose(editor.targetPose) end
            record.editor.targetManualOverride = editor.targetManualOverride == true
            record.editor.targetSelectable = editor.targetSelectable ~= false
        end
        return record
    end
    local baseTarget = targetRecordFromValue({}, result.target)
    result.targets = {}
    if type(value.targets) == 'table' and #value.targets > 0 then
        for index, raw in ipairs(value.targets) do
            if type(raw) == 'table' then
                result.targets[#result.targets + 1] = targetRecordFromValue(raw, 'target_' .. index)
            end
        end
    end
    if #result.targets == 0 then result.targets[1] = baseTarget end
    local firstTarget = result.targets[1]
    result.target = firstTarget.name
    result.targetSource = firstTarget.source or result.source
    result.sourceBone = firstTarget.sourceBone or result.sourceBone
    result.sourceSocket = firstTarget.sourceSocket or result.sourceSocket
    result.sourcePart = firstTarget.sourcePart or result.sourcePart
    result.inherit = firstTarget.inherit
    result.aimAtSource = firstTarget.aimAtSource
    result.aimPoint = firstTarget.aimPoint
    result.aimLimits = firstTarget.aimLimits
    result.spring = firstTarget.spring
    result.blend = firstTarget.blend
    result.offset = firstTarget.offset
    result.targetPivot = firstTarget.targetPivot
    result.editor.targetKind = firstTarget.editor.targetKind
    result.editor.targetMesh = firstTarget.editor.targetMesh
    result.editor.targetPose = firstTarget.editor.targetPose
    result.editor.targetManualOverride = firstTarget.editor.targetManualOverride
    result.editor.targetSelectable = firstTarget.editor.targetSelectable
    if result.sourceType == 'bone' and result.sourceBone == '' then return nil end
    return result
end

local function restoreRoleAsset(role, kind, path)
    local entry = roleEntry(role)
    destroyRoleObject(role)
    entry.kind = kind == 'mesh' and 'mesh' or 'placeholder'
    entry.path = path or ''
    if entry.kind == 'mesh' and entry.path ~= '' and loadRoleMesh(role, entry.path) then return end
    entry.kind = 'placeholder'
    entry.path = ''
    ensureRolePlaceholder(role)
end

local function loadDefinition(path)
    path = path or mbm.openFile(S.projectPath or '', 'att.lua')
    if not path then return end
    local fn, errorMessage = loadfile(path)
    if not fn then setStatus(errorMessage, true); return end
    attachmentDefinition = nil
    local ok = dpCall(fn)
    if not ok then return end
    local definition = normalizeDefinition(attachmentDefinition)
    if not definition then setStatus(L('invalid_definition'), true); return end
    local repairedSource = false
    if S.boneSelectionMode then setBoneSelectionMode(false) end
    if S.partSelectionMode then setPartSelectionMode(false) end
    for _, targetState in ipairs(S.targets) do
        if targetState.object then targetState.object:destroy() end
    end
    S.targets = {}
    S.selectedTargetIndex = 1
    S.definition = definition
    S.graphLayout = {root = nil, targets = {}}
    S.stateDraft = definition.initialState
    S.source.pose = clonePose(definition.editor.sourcePose)
    for index = 1, #definition.targets do S.targets[index] = newTargetState() end
    activateTargetDefinition(1)
    restoreRoleAsset('source', definition.editor.sourceKind, definition.editor.sourceMesh)
    for index, targetDefinition in ipairs(definition.targets) do
        activateTargetDefinition(index)
        restoreRoleAsset('target', targetDefinition.editor.targetKind, targetDefinition.editor.targetMesh)
        S.target.pose = clonePose(targetDefinition.editor.targetPose)
        S.target.manualOverride = targetDefinition.editor.targetManualOverride == true
    end
    activateTargetDefinition(1)
    if definition.sourceType == 'socket' and definition.sourceSocket == '' then
        local firstSocket = S.source.socketCatalog and S.source.socketCatalog[1] or nil
        if not firstSocket then
            setStatus(L('invalid_definition'), true)
            return
        end
        definition.sourceSocket = firstSocket
        repairedSource = true
    end
    if definition.sourceType == 'articulated_part' and definition.sourcePart == '' then
        local firstPart = S.source.articulatedPartCatalog and S.source.articulatedPartCatalog[1] or nil
        if not firstPart then
            setStatus(L('invalid_definition'), true)
            return
        end
        definition.sourcePart = firstPart
        repairedSource = true
    end
    if definition.sourceType == 'articulated_part' then
        for _, targetDefinition in ipairs(definition.targets) do
            if targetDefinition.sourcePart == '' then
                targetDefinition.sourcePart = definition.sourcePart
                repairedSource = true
            end
        end
    end
    if definition.sourceType == 'bone' then
        for _, targetDefinition in ipairs(definition.targets) do
            if targetDefinition.sourceBone == '' then
                targetDefinition.sourceBone = definition.sourceBone
                repairedSource = true
            end
        end
    elseif definition.sourceType == 'socket' then
        for _, targetDefinition in ipairs(definition.targets) do
            if targetDefinition.sourceSocket == '' then
                targetDefinition.sourceSocket = definition.sourceSocket
                repairedSource = true
            end
        end
    end
    if definition.editor.sourceAnimation ~= '' then
        for index, name in ipairs(S.source.animationClips) do
            if name == definition.editor.sourceAnimation then
                S.source.selectedAnimation = index
                break
            end
        end
    end
    if definition.version < 2 and definition.sourceType == 'bone' and S.source.skeletonReport then
        captureOffset()
        S.definition.version = 2
    end
    S.definition.version = math.max(4, S.definition.version)
    S.source.selectable = definition.editor.sourceSelectable ~= false
    if definition.editor.language == 'pt_br' or definition.editor.language == 'en' then
        tLang.setLanguage(definition.editor.language)
        refreshWindowTitles()
    end
    S.projectPath, S.modified = path, repairedSource
    S.target.manualOverride = definition.editor.targetManualOverride == true
    S.dirtyPreview = true
    setStatus(string.format(L('loaded'), path))
end

local function showPoseFields(role)
    local entry = roleEntry(role)
    local value = entry.pose
    local fields = {
        {'x', L('pos_x'), 0}, {'y', L('pos_y'), 0}, {'z', L('pos_z'), 0},
        {'rx', L('rot_x'), 0}, {'ry', L('rot_y'), 0}, {'rz', L('rot_z'), 0},
        {'sx', L('scale_x'), 1}, {'sy', L('scale_y'), 1}, {'sz', L('scale_z'), 1},
    }
    for _, field in ipairs(fields) do
        tImGui.SetNextItemWidth(105)
        local changed, nextValue = tImGui.InputFloat(field[2] .. '##att_' .. role .. '_' .. field[1], value[field[1]])
        if changed then
            value[field[1]] = nextValue
            if role == 'target' then S.target.manualOverride = true else S.target.manualOverride = false end
            markModified()
        end
    end
    if role == 'target' then showTargetPivotFields() end
end

local function showRoleAsset(role)
    local entry = roleEntry(role)
    local kindIndex = entry.kind == 'mesh' and 1 or 2
    tImGui.SetNextItemWidth(210)
    local kindChanged, nextKind = tImGui.Combo(L('actor_type') .. '##att_' .. role .. '_type', kindIndex,
        {L('mesh'), L('placeholder')})
    if kindChanged then setRoleKind(role, nextKind == 1 and 'mesh' or 'placeholder') end
    if entry.kind == 'mesh' then
        local meshName = entry.path ~= '' and tUtil.getShortName(entry.path) or L('not_loaded')
        tImGui.Text(string.format(L('mesh_file'), meshName))
        if tImGui.Button(L('browse') .. '##att_' .. role .. '_mesh') then openRoleMesh(role) end
    else
        tImGui.SetNextItemWidth(150)
        local sizeChanged, nextSize = tImGui.InputFloat(L('preview_size') .. '##att_' .. role,
            entry.previewSize, 1)
        if sizeChanged then entry.previewSize = math.max(3, nextSize); markModified() end
        tImGui.TextWrapped(L('placeholder_note'))
    end
end

local function showViewSelectable(role)
    local entry = roleEntry(role)
    local previous = entry.selectable ~= false
    local nextValue = tImGui.Checkbox(L('select_in_view') .. '##att_' .. role .. '_selectable', previous)
    if nextValue ~= previous then
        entry.selectable = nextValue
        markModified()
        if not nextValue and S.selectedRole == role then S.gizmo.drag = nil end
        updateGizmo()
    end
end

local function showSourceAnimationControls()
    local source = S.source
    if not source.meshLoaded then return end
    if #source.animationClips == 0 then
        tImGui.TextWrapped(L('no_source_animations'))
        return
    end
    source.selectedAnimation = math.max(1, math.min(source.selectedAnimation, #source.animationClips))
    tImGui.SetNextItemWidth(210)
    local changed, selected = tImGui.Combo(L('animation_clip'), source.selectedAnimation, source.animationClips)
    if changed then source.selectedAnimation = selected end
    showTooltip(L('animation_clip_tooltip'))
    if tImGui.Button(L('play_animation') .. '##att_source_animation_play') then
        playSourceAnimation()
    end
    showTooltip(L('play_animation_tooltip'))
    tImGui.SameLine()
    if tImGui.Button(L('pause_animation') .. '##att_source_animation_pause') then
        pauseSourceAnimation()
    end
    showTooltip(L('pause_animation_tooltip'))
    tImGui.SameLine()
    if tImGui.Button(L('resume_animation') .. '##att_source_animation_resume') then
        resumeSourceAnimation()
    end
    showTooltip(L('resume_animation_tooltip'))
    tImGui.SameLine()
    if tImGui.Button(L('stop_animation') .. '##att_source_animation_stop') then
        stopSourceAnimation()
    end
    showTooltip(L('stop_animation_tooltip'))
    local duration = sourceAnimationDuration()
    local currentTime = 0
    if source.activeAnimation ~= '' then
        local ok, time
        if sourceUsesArticulatedAnimation() then
            ok, time = dpCall(function()
                return source.object:getArticulatedAnimationTime(source.activeAnimation)
            end)
        else
            ok, time = dpCall(function() return source.object:getSkeletalAnimationTime() end)
        end
        if ok then currentTime = math.max(0, tonumber(time) or 0) end
    end
    if duration > 0 then
        local timelineChanged, timelineValue = tImGui.SliderFloat(L('animation_time'), currentTime, 0, duration)
        if timelineChanged then seekSourceAnimation(timelineValue) end
        showTooltip(L('animation_time_tooltip'))
        if tImGui.Button(L('step_back') .. '##att_source_animation_step_back') then
            seekSourceAnimation(math.max(0, currentTime - 1 / 30))
        end
        showTooltip(L('step_back_tooltip'))
        tImGui.SameLine()
        if tImGui.Button(L('step_forward') .. '##att_source_animation_step_forward') then
            seekSourceAnimation(math.min(duration, currentTime + 1 / 30))
        end
        showTooltip(L('step_forward_tooltip'))
    end
end

local function showObjects()
    tUtil.setInitialWindowPositionLeft(S.windows.objects, 0, 20, 380)
    local opened = tImGui.Begin(S.windows.objects, true, 0)
    if opened then
        tImGui.Text(L('objects'))
        tImGui.TextWrapped(L('objects_help'))
        tImGui.Separator()
        tImGui.Text(L('targets'))
        for index, targetDefinition in ipairs(S.definition.targets) do
            local label = string.format('%s %d: %s##att_target_item_%d', L('target'), index,
                targetDefinition.name, index)
            if tImGui.Selectable(label, S.selectedTargetIndex == index) then
                selectTarget(index)
            end
        end
        if tImGui.Button(L('add_target') .. '##att_add_target') then addTarget() end
        if #S.definition.targets > 1 then
            tImGui.SameLine()
            if tImGui.Button(L('remove_target') .. '##att_remove_target') then removeTarget() end
        end
        tImGui.Separator()
        if tImGui.Selectable(L('source') .. ': ' .. roleName('source'), S.selectedRole == 'source') then
            S.selectedRole = 'source'
        end
        tImGui.SetNextItemWidth(210)
        local sourceChanged, sourceName = tImGui.InputText(L('source_name'), S.definition.source)
        if sourceChanged then setSourceBindingName(sourceName) end
        showViewSelectable('source')
        showRoleAsset('source')
        if tImGui.Selectable(L('target') .. ': ' .. roleName('target'), S.selectedRole == 'target') then
            S.selectedRole = 'target'
        end
        tImGui.SetNextItemWidth(210)
        local targetChanged, targetName = tImGui.InputText(L('target_name'), S.definition.target)
        if targetChanged then setTargetBindingName(targetName) end
        local sourceOptions = {S.definition.source}
        for index, targetDefinition in ipairs(S.definition.targets) do
            if index ~= S.selectedTargetIndex and targetDefinition.name ~= '' then
                sourceOptions[#sourceOptions + 1] = targetDefinition.name
            end
        end
        local selectedSource = 1
        for index, sourceNameValue in ipairs(sourceOptions) do
            if sourceNameValue == S.definition.targetSource then selectedSource = index; break end
        end
        tImGui.SetNextItemWidth(210)
        local sourceTargetChanged, nextSourceTarget = tImGui.Combo(L('target_source'), selectedSource, sourceOptions)
        if sourceTargetChanged and sourceOptions[nextSourceTarget] then
            S.definition.targetSource = sourceOptions[nextSourceTarget]
            markModified()
        end
        showViewSelectable('target')
        showRoleAsset('target')
        tImGui.TextWrapped(L('runtime_id_help'))
        tImGui.Separator()
        if S.selectedRole then
            tImGui.Text(string.format(L('selected_object'), roleName(S.selectedRole)))
            showPoseFields(S.selectedRole)
            if S.selectedRole == 'target' then
                tImGui.TextWrapped(L('target_pose_help'))
                tImGui.Separator()
                if tImGui.Button(L('capture') .. '##att_objects_capture') then captureOffset() end
                tImGui.SameLine()
                if tImGui.Button(L('reset_preview') .. '##att_objects_reset') then resetTargetPreview() end
            else
                tImGui.TextWrapped(L('source_pose_help'))
            end
        else
            tImGui.Text(string.format(L('selected_object'), L('none_selected')))
            tImGui.TextWrapped(L('select_actor_hint'))
        end
        tImGui.Separator()
        tImGui.Text(L('transform_mode'))
        local currentModeIndex = S.transformMode == 'rotate' and 2 or
            (S.transformMode == 'pivot' and S.selectedRole == 'target' and 3 or
             (S.transformMode == 'aim_point' and S.definition.aimAtSource and 4 or 1))
        local modeIndex = tImGui.RadioButton(L('move'), currentModeIndex, 1)
        tImGui.SameLine()
        modeIndex = tImGui.RadioButton(L('rotate'), modeIndex, 2)
        if S.selectedRole == 'target' then
            tImGui.SameLine()
            modeIndex = tImGui.RadioButton(L('move_pivot'), modeIndex, 3)
        end
        if S.definition.aimAtSource then
            tImGui.SameLine()
            modeIndex = tImGui.RadioButton(L('move_aim_point'), modeIndex, 4)
        end
        local nextMode
        if modeIndex == 2 then nextMode = 'rotate'
        elseif modeIndex == 3 and S.selectedRole == 'target' then nextMode = 'pivot'
        elseif modeIndex == 4 and S.definition.aimAtSource then nextMode = 'aim_point'
        else nextMode = 'move' end
        if nextMode ~= S.transformMode then
            S.transformMode = nextMode
            S.gizmo.drag = nil
            S.dirtyPreview = true
        end
        local help = L('move_gizmo_help')
        if S.transformMode == 'rotate' then help = L('rotate_gizmo_help')
        elseif S.transformMode == 'pivot' then help = L('move_pivot_gizmo_help')
        elseif S.transformMode == 'aim_point' then help = L('move_aim_point_gizmo_help') end
        tImGui.TextWrapped(help)
    end
    tImGui.End()
end

local function showOffsetFields()
    local components = {
        {'position', L('offset_position')}, {'rotation', L('offset_rotation')}, {'scale', L('offset_scale')},
    }
    for _, component in ipairs(components) do
        tImGui.Text(component[2])
        for _, axis in ipairs({'x', 'y', 'z'}) do
            tImGui.SetNextItemWidth(105)
            local key = component[1]
            local changed, nextValue = tImGui.InputFloat(string.upper(axis) .. '##att_offset_' .. key .. '_' .. axis,
                S.definition.offset[key][axis])
            if changed then
                S.definition.offset[key][axis] = nextValue
                if key == 'rotation' then
                    S.definition.offset.rotationMatrix = eulerRotation(
                        S.definition.offset.rotation.x, S.definition.offset.rotation.y,
                        S.definition.offset.rotation.z)
                end
                S.target.manualOverride = false
                markModified()
            end
            if axis ~= 'z' then tImGui.SameLine() end
        end
    end
end

showTargetPivotFields = function()
    tImGui.Text(L('target_pivot'))
    for _, axis in ipairs({'x', 'y', 'z'}) do
        tImGui.SetNextItemWidth(105)
        local changed, nextValue = tImGui.InputFloat(L('pivot_' .. axis) .. '##att_target_pivot_' .. axis,
            S.definition.targetPivot[axis])
        if changed then
            S.definition.targetPivot[axis] = nextValue
            S.target.manualOverride = false
            markModified()
        end
    end
    tImGui.TextWrapped(L('target_pivot_help'))
end

showAimPointFields = function()
    tImGui.Text(L('aim_point'))
    for _, axis in ipairs({'x', 'y', 'z'}) do
        tImGui.SetNextItemWidth(105)
        local changed, nextValue = tImGui.InputFloat(L('aim_point_' .. axis) ..
            '##att_aim_point_' .. axis, S.definition.aimPoint[axis])
        if changed then
            S.definition.aimPoint[axis] = nextValue
            S.target.manualOverride = false
            markModified()
        end
    end
    showTooltip(L('aim_point_help'))
end

local function showAttachment()
    tUtil.setInitialWindowPositionRight(S.windows.attachment, 0, 20, 400, 430, 560)
    local opened = tImGui.Begin(S.windows.attachment, true, 0)
    if opened then
        tImGui.Text(L('attachment'))
        tImGui.Text(string.format('%s -> %s', roleName('source'), roleName('target')))
        tImGui.SetNextItemWidth(210)
        local sourceTypeIndex = S.definition.sourceType == 'bone' and 2 or
            (S.definition.sourceType == 'socket' and 3 or
                (S.definition.sourceType == 'articulated_part' and 4 or 1))
        local sourceTypeChanged, nextSourceType = tImGui.Combo(L('source_type'), sourceTypeIndex,
            {L('root_transform'), L('bone_transform'), L('socket_transform'),
             L('articulated_part_transform')})
        if sourceTypeChanged then
            syncActiveTargetDefinition()
            if S.partSelectionMode then setPartSelectionMode(false) end
            stopSourceAnimation()
            S.definition.sourceType = nextSourceType == 2 and 'bone' or
                (nextSourceType == 3 and 'socket' or
                    (nextSourceType == 4 and 'articulated_part' or 'root'))
            if S.definition.sourceType == 'root' then
                S.definition.sourceBone = ''
                S.definition.sourceSocket = ''
                S.definition.sourcePart = ''
                setBoneSelectionMode(false)
            elseif S.definition.sourceType == 'bone' then
                S.definition.sourceSocket = ''
                S.definition.sourcePart = ''
                setBoneSelectionMode(true)
            elseif S.definition.sourceType == 'socket' then
                S.definition.sourceBone = ''
                S.definition.sourcePart = ''
                S.definition.sourceSocket = S.source.socketCatalog[1] or ''
                for _, targetDefinition in ipairs(S.definition.targets) do
                    if targetDefinition.sourceSocket == '' then
                        targetDefinition.sourceSocket = S.definition.sourceSocket
                    end
                end
                setBoneSelectionMode(false)
            else
                S.definition.sourceBone = ''
                S.definition.sourceSocket = ''
                S.definition.sourcePart = S.source.articulatedPartCatalog[1] or ''
                for _, targetDefinition in ipairs(S.definition.targets) do
                    if targetDefinition.sourcePart == '' then
                        targetDefinition.sourcePart = S.definition.sourcePart
                    end
                end
                setBoneSelectionMode(false)
            end
            refreshSourceAnimationCatalog()
            -- Changing the source frame must not apply the previous offset to the
            -- target immediately. Keep the current target pose until the user
            -- explicitly captures an offset for the new source.
            S.target.manualOverride = true
            markModified()
        end
        if S.definition.sourceType == 'bone' then
            local selectedBone = S.definition.sourceBone ~= '' and S.definition.sourceBone or L('none_selected')
            tImGui.Text(string.format('%s: %s', L('source_bone'), selectedBone))
            local selectBone = tImGui.Checkbox(L('select_bone'), S.boneSelectionMode)
            if selectBone ~= S.boneSelectionMode then
                setBoneSelectionMode(selectBone)
            end
            tImGui.TextWrapped(L('bone_select_help'))
            tImGui.TextWrapped(L('source_bone_help'))
        elseif S.definition.sourceType == 'socket' then
            local socketNames = S.source.socketCatalog or {}
            if #socketNames > 0 then
                if S.definition.sourceSocket == '' then
                    S.definition.sourceSocket = socketNames[1]
                    markModified()
                end
                local selectedSocket = 1
                for index, name in ipairs(socketNames) do
                    if name == S.definition.sourceSocket then selectedSocket = index; break end
                end
                tImGui.SetNextItemWidth(210)
                local socketChanged, nextSocket = tImGui.Combo(L('source_socket'), selectedSocket, socketNames)
                if socketChanged and socketNames[nextSocket] then
                    S.definition.sourceSocket = socketNames[nextSocket]
                    S.target.manualOverride = true
                    markModified()
                end
            else
                tImGui.Text(L('no_sockets'))
            end
            tImGui.TextWrapped(L('source_socket_help'))
        elseif S.definition.sourceType == 'articulated_part' then
            local partNames = S.source.articulatedPartCatalog or {}
            if #partNames > 0 then
                if S.definition.sourcePart == '' then
                    S.definition.sourcePart = partNames[1]
                    markModified()
                end
                local selectedPart = 1
                for index, name in ipairs(partNames) do
                    if name == S.definition.sourcePart then selectedPart = index; break end
                end
                tImGui.SetNextItemWidth(210)
                local partChanged, nextPart = tImGui.Combo(L('source_part'), selectedPart, partNames)
                if partChanged and partNames[nextPart] then
                    S.definition.sourcePart = partNames[nextPart]
                    S.target.manualOverride = true
                    markModified()
                end
            else
                tImGui.Text(L('no_articulated_parts'))
            end
            local selectPart = tImGui.Checkbox(L('select_part'), S.partSelectionMode)
            if selectPart ~= S.partSelectionMode then
                setPartSelectionMode(selectPart)
            end
            tImGui.TextWrapped(L('part_select_help'))
            tImGui.TextWrapped(L('source_part_help'))
        end
        local components = {{'position', L('inherit_position')}, {'rotation', L('inherit_rotation')},
            {'scale', L('inherit_scale')}}
        for _, component in ipairs(components) do
            local key, label = component[1], component[2]
            local previousValue = S.definition.inherit[key] == true
            local nextValue = tImGui.Checkbox(label, previousValue)
            if nextValue ~= previousValue then
                if key == 'rotation' and S.definition.aimAtSource and not nextValue then
                    setStatus(L('aim_requires_rotation'), true)
                else
                    S.definition.inherit[key] = nextValue
                    S.target.manualOverride = false
                    markModified()
                end
            end
        end
        local previousAim = S.definition.aimAtSource == true
        local nextAim = tImGui.Checkbox(L('aim_at_source'), previousAim)
        if nextAim ~= previousAim then
            S.definition.aimAtSource = nextAim == true
            if nextAim then S.definition.inherit.rotation = true end
            S.target.manualOverride = false
            markModified()
        end
        tImGui.TextWrapped(L('aim_at_source_help'))
        if S.definition.aimAtSource then
            showAimPointFields()
            tImGui.Separator()
            tImGui.Text(L('aim_limits'))
            local limits = S.definition.aimLimits
            local previousLimitsEnabled = limits.enabled == true
            local nextLimitsEnabled = tImGui.Checkbox(L('aim_limits_enabled'), previousLimitsEnabled)
            if nextLimitsEnabled ~= previousLimitsEnabled then
                limits.enabled = nextLimitsEnabled == true
                S.target.manualOverride = false
                markModified()
            end
            showTooltip(L('aim_limits_help'))
            if limits.enabled then
                local nextShowLimits = tImGui.Checkbox(L('show_aim_limits'), S.showAimLimits)
                if nextShowLimits ~= S.showAimLimits then S.showAimLimits = nextShowLimits end
                local referenceMode = limits.reference == 'captured' and 2 or 1
                local nextReference = tImGui.RadioButton(L('aim_limits_dynamic_frame'), referenceMode, 1)
                nextReference = tImGui.RadioButton(L('aim_limits_captured_frame'), nextReference, 2)
                if nextReference ~= referenceMode then
                    limits.reference = nextReference == 2 and 'captured' or 'dynamic'
                    if limits.reference == 'captured' then captureAimReference() end
                    S.target.manualOverride = false
                    markModified()
                end
                tImGui.SetNextItemWidth(210)
                local changed, value = tImGui.InputFloat(L('aim_yaw_min'), limits.yawMin)
                if changed then
                    limits.yawMin = math.min(value, limits.yawMax)
                    S.target.manualOverride = false
                    markModified()
                end
                tImGui.SetNextItemWidth(210)
                changed, value = tImGui.InputFloat(L('aim_yaw_max'), limits.yawMax)
                if changed then
                    limits.yawMax = math.max(value, limits.yawMin)
                    S.target.manualOverride = false
                    markModified()
                end
                tImGui.SetNextItemWidth(210)
                changed, value = tImGui.InputFloat(L('aim_pitch_min'), limits.pitchMin)
                if changed then
                    limits.pitchMin = math.min(value, limits.pitchMax)
                    S.target.manualOverride = false
                    markModified()
                end
                tImGui.SetNextItemWidth(210)
                changed, value = tImGui.InputFloat(L('aim_pitch_max'), limits.pitchMax)
                if changed then
                    limits.pitchMax = math.max(value, limits.pitchMin)
                    S.target.manualOverride = false
                    markModified()
                end
            end
        end
        tImGui.Separator()
        tImGui.Text(L('spring'))
        local spring = S.definition.spring
        local previousSpringEnabled = spring.enabled == true
        local nextSpringEnabled = tImGui.Checkbox(L('spring_enabled'), previousSpringEnabled)
        if nextSpringEnabled ~= previousSpringEnabled then
            spring.enabled = nextSpringEnabled == true
            resetSpringState()
            S.target.manualOverride = false
            markModified()
        end
        if spring.enabled then
            local previousSpringPosition = spring.position == true
            local nextSpringPosition = tImGui.Checkbox(L('spring_position'), previousSpringPosition)
            if nextSpringPosition ~= previousSpringPosition then
                spring.position = nextSpringPosition == true
                resetSpringState()
                markModified()
            end
            local previousSpringRotation = spring.rotation == true
            local nextSpringRotation = tImGui.Checkbox(L('spring_rotation'), previousSpringRotation)
            if nextSpringRotation ~= previousSpringRotation then
                spring.rotation = nextSpringRotation == true
                resetSpringState()
                markModified()
            end
            tImGui.SetNextItemWidth(210)
            local changed, value = tImGui.InputFloat(L('spring_stiffness'), spring.stiffness)
            if changed then
                spring.stiffness = math.max(0, value)
                resetSpringState()
                markModified()
            end
            tImGui.SetNextItemWidth(210)
            changed, value = tImGui.InputFloat(L('spring_damping'), spring.damping)
            if changed then
                spring.damping = math.max(0, value)
                resetSpringState()
                markModified()
            end
            tImGui.TextWrapped(L('spring_help'))
        end
        tImGui.Separator()
        tImGui.Text(L('blend'))
        local profile = activeStateProfile()
        local blend = profile and ensureStateBlend(profile) or S.definition.blend
        local previousBlendEnabled = blend.enabled == true
        local nextBlendEnabled = tImGui.Checkbox(L('blend_enabled'), previousBlendEnabled)
        if nextBlendEnabled ~= previousBlendEnabled then
            blend.enabled = nextBlendEnabled == true
            S.target.manualOverride = false
            markModified()
        end
        if blend.enabled then
            tImGui.SetNextItemWidth(210)
            local changed, value = tImGui.InputFloat(L('blend_weight'), blend.weight, 0.05, 0.05, '%.2f')
            if changed then
                blend.weight = math.min(1, math.max(0, value))
                S.target.manualOverride = false
                markModified()
            end
            local blendComponents = {
                {'position', L('blend_position')}, {'rotation', L('blend_rotation')},
                {'scale', L('blend_scale')},
            }
            for _, component in ipairs(blendComponents) do
                local key, label = component[1], component[2]
                local previousValue = blend[key] == true
                local nextValue = tImGui.Checkbox(label, previousValue)
                if nextValue ~= previousValue then
                    blend[key] = nextValue == true
                    S.target.manualOverride = false
                    markModified()
                end
            end
            tImGui.TextWrapped(L('blend_help'))
        end
        tImGui.Separator()
        showOffsetFields()
        tImGui.TextWrapped(L('offset_help'))
    end
    tImGui.End()
end

local function showGraphPanel()
    if not S.showGraph then return end
    local screenWidth, screenHeight = mbm.getRealSizeScreen()
    local leftPanelWidth, rightPanelWidth = 390, 400
    local top = tImGui.GetMainMenuBarHeight() + 20
    local bottom = math.floor(screenHeight * 0.20)
    local graphWidth = math.max(240, screenWidth - leftPanelWidth - rightPanelWidth)
    local graphHeight = math.max(180, screenHeight - top - bottom)
    tImGui.SetNextWindowPos({x = leftPanelWidth, y = top}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x = graphWidth, y = graphHeight}, tImGui.Flags('ImGuiCond_Once'))
    local opened, closed = tImGui.Begin(S.windows.graph, true, 0)
    if closed then S.showGraph = false end
    if opened then
        local graph = validateAttachmentGraph()
        tImGui.Text(L('graph'))
        if graph.valid then
            tImGui.TextColored({r = 0.35, g = 1, b = 0.45, a = 1}, L('graph_valid'))
        else
            tImGui.TextColored({r = 1, g = 0.35, b = 0.3, a = 1}, L('graph_invalid'))
            tImGui.TextWrapped(L(graph.errorKey))
        end
        tImGui.TextWrapped(L('graph_help'))

        local available = tImGui.GetContentRegionAvail()
        local canvasWidth = math.max(180, available.x)
        local rowHeight, nodeHeight = 42, 28
        local targetCount = #S.definition.targets
        local canvasHeight = math.max(100, available.y)
        local origin = tImGui.GetCursorScreenPos()
        local rootWidth = math.min(135, math.max(80, canvasWidth * 0.36))
        local targetWidth = math.min(175, math.max(100, canvasWidth * 0.42))
        local rootDefaultY = 12 + math.max(0, targetCount - 1) * rowHeight * 0.5
        local function clampLayoutPosition(layoutPosition, width)
            layoutPosition.x = math.max(0, math.min(math.max(0, canvasWidth - width), layoutPosition.x))
            layoutPosition.y = math.max(0, math.min(math.max(0, canvasHeight - nodeHeight), layoutPosition.y))
        end
        if not S.graphLayout.root then S.graphLayout.root = {x = 8, y = rootDefaultY} end
        clampLayoutPosition(S.graphLayout.root, rootWidth)
        local positions = {
            root = {x = origin.x + S.graphLayout.root.x, y = origin.y + S.graphLayout.root.y,
                width = rootWidth, layout = S.graphLayout.root},
            targets = {},
        }
        local function targetDepth(index, visiting)
            visiting = visiting or {}
            if visiting[index] then return 1 end
            visiting[index] = true
            local record = S.definition.targets[index]
            local sourceName = record and (record.source or S.definition.source) or S.definition.source
            local sourceIndex = graph.targetIndexes[sourceName]
            local depth = sourceIndex and (targetDepth(sourceIndex, visiting) + 1) or 1
            visiting[index] = nil
            return depth
        end
        for index, record in ipairs(S.definition.targets) do
            if not S.graphLayout.targets[index] then
                local depth = targetDepth(index)
                local defaultX = 8 + rootWidth + 48 + (depth - 1) * (targetWidth + 48)
                S.graphLayout.targets[index] = {x = defaultX,
                    y = 12 + (index - 1) * rowHeight}
            end
            clampLayoutPosition(S.graphLayout.targets[index], targetWidth)
            positions.targets[index] = {
                x = origin.x + S.graphLayout.targets[index].x,
                y = origin.y + S.graphLayout.targets[index].y,
                width = targetWidth, index = index, layout = S.graphLayout.targets[index],
            }
        end

        local selectedChain = {}
        local selected = S.definition.targets[S.selectedTargetIndex]
        local currentName = selected and selected.name or nil
        while currentName and currentName ~= S.definition.source and not selectedChain[currentName] do
            selectedChain[currentName] = true
            local index = targetIndexByName(currentName)
            currentName = index and (S.definition.targets[index].source or S.definition.source) or nil
        end
        selectedChain[S.definition.source] = selected ~= nil

        for _, edge in ipairs(graph.edges) do
            local sourceIndex = edge.source == S.definition.source and nil or targetIndexByName(edge.source)
            local targetIndex = targetIndexByName(edge.target)
            local sourcePosition = sourceIndex and positions.targets[sourceIndex] or positions.root
            local targetPosition = targetIndex and positions.targets[targetIndex]
            if sourcePosition and targetPosition then
                local highlighted = selectedChain[edge.source] and selectedChain[edge.target]
                local color = highlighted and {r = 1, g = 0.8, b = 0.2, a = 1} or
                    {r = 0.55, g = 0.6, b = 0.68, a = 1}
                tImGui.AddLine(
                    {x = sourcePosition.x + sourcePosition.width, y = sourcePosition.y + nodeHeight * 0.5},
                    {x = targetPosition.x, y = targetPosition.y + nodeHeight * 0.5}, color,
                    highlighted and 3 or 1)
            end
        end

        local rootPosition = positions.root
        tImGui.AddRectFilled({x = rootPosition.x, y = rootPosition.y},
            {x = rootPosition.x + rootPosition.width, y = rootPosition.y + nodeHeight},
            {r = 0.12, g = 0.42, b = 0.62, a = 1}, 4)
        tImGui.AddText({x = rootPosition.x + 8, y = rootPosition.y + 6},
            {r = 1, g = 1, b = 1, a = 1}, S.definition.source)
        for index, record in ipairs(S.definition.targets) do
            local name = tostring(record.name or '')
            local position = positions.targets[index]
            if position then
                local selectedNode = index == S.selectedTargetIndex
                local nodeColor = selectedNode and {r = 0.2, g = 0.48, b = 0.78, a = 1} or
                    {r = 0.32, g = 0.34, b = 0.4, a = 1}
                if not graph.valid then nodeColor = {r = 0.46, g = 0.2, b = 0.2, a = 1} end
                tImGui.AddRectFilled({x = position.x, y = position.y},
                    {x = position.x + position.width, y = position.y + nodeHeight}, nodeColor, 4)
                tImGui.AddText({x = position.x + 8, y = position.y + 6},
                    {r = 1, g = 1, b = 1, a = 1}, name ~= '' and name or ('target_' .. index))
            end
        end
        local function makeNodeDraggable(id, position, onClick)
            tImGui.SetCursorScreenPos({x = position.x, y = position.y})
            if tImGui.InvisibleButton(id, {x = position.width, y = nodeHeight}) and onClick then
                onClick()
            end
            if tImGui.IsItemActive() then
                local delta = tImGui.GetMouseDragDelta(0)
                if delta.x ~= 0 or delta.y ~= 0 then
                    position.layout.x = position.layout.x + delta.x
                    position.layout.y = position.layout.y + delta.y
                    clampLayoutPosition(position.layout, position.width)
                    tImGui.ResetMouseDragDelta(0)
                end
            end
        end
        makeNodeDraggable('##att_graph_source', positions.root, function()
            S.selectedRole = 'source'
            S.gizmo.drag = nil
        end)
        for index, record in ipairs(S.definition.targets) do
            local position = positions.targets[index]
            if position then
                makeNodeDraggable('##att_graph_target_' .. index, position, function()
                    selectTarget(index)
                end)
            end
        end
        tImGui.SetCursorScreenPos({x = origin.x, y = origin.y + canvasHeight})
        tImGui.Dummy({x = canvasWidth, y = 1})
    end
    tImGui.End()
end

local function showStatePanel()
    if not S.showStates then return end
    tUtil.setInitialWindowPositionLeft(S.windows.states, 380, 20, 380, 380, 380)
    local opened, closed = tImGui.Begin(S.windows.states, true, 0)
    if closed then S.showStates = false end
    if opened then
        tImGui.Text(L('states'))
        tImGui.TextWrapped(L('states_help'))
        local names = stateNames()
        for _, name in ipairs(names) do
            if tImGui.Selectable(name, S.definition.initialState == name) then
                selectAttachmentState(name)
            end
        end
        if #names == 0 then tImGui.Text(L('no_states')) end
        if tImGui.Button(L('add_state')) then
            local index = 1
            local name = 'state_' .. index
            while S.definition.states[name] do
                index = index + 1
                name = 'state_' .. index
            end
            S.definition.states[name] = {
                influenceEnabled = S.definition.influenceEnabled ~= false,
                blend = cloneBlend(S.definition.blend),
            }
            selectAttachmentState(name)
        end
        if S.definition.initialState ~= '' and S.definition.states[S.definition.initialState] then
            local profile = S.definition.states[S.definition.initialState]
            S.stateDraft = S.stateDraft ~= '' and S.stateDraft or S.definition.initialState
            tImGui.SetNextItemWidth(210)
            local nameChanged, nextName = tImGui.InputText(L('state_name') .. '##att_state_name', S.stateDraft)
            if nameChanged then S.stateDraft = nextName end
            if tImGui.Button(L('rename_state')) then
                local oldName = S.definition.initialState
                local newName = S.stateDraft:gsub('^%s*(.-)%s*$', '%1')
                if newName == '' or (newName ~= oldName and S.definition.states[newName]) then
                    setStatus(L('state_invalid_name'), true)
                else
                    S.definition.states[newName] = S.definition.states[oldName]
                    if newName ~= oldName then S.definition.states[oldName] = nil end
                    S.definition.initialState = newName
                    S.stateDraft = newName
                    markModified()
                end
            end
            tImGui.SameLine()
            if tImGui.Button(L('remove_state')) then
                S.definition.states[S.definition.initialState] = nil
                local nextStates = stateNames()
                selectAttachmentState(nextStates[1] or '')
            end
            local previousInfluence = profile.influenceEnabled ~= false
            local nextInfluence = tImGui.Checkbox(L('state_influence'), previousInfluence)
            if nextInfluence ~= previousInfluence then
                profile.influenceEnabled = nextInfluence == true
                S.target.manualOverride = false
                resetSpringState()
                markModified()
            end
            local blend = ensureStateBlend(profile)
            local previousBlendEnabled = blend.enabled == true
            local nextBlendEnabled = tImGui.Checkbox(L('blend_enabled'), previousBlendEnabled)
            if nextBlendEnabled ~= previousBlendEnabled then
                blend.enabled = nextBlendEnabled == true
                S.target.manualOverride = false
                markModified()
            end
            tImGui.SetNextItemWidth(210)
            local changed, value = tImGui.InputFloat(L('blend_weight'), blend.weight, 0.05, 0.05, '%.2f')
            if changed then
                blend.weight = math.min(1, math.max(0, value))
                S.target.manualOverride = false
                markModified()
            end
            local components = {
                {'position', L('blend_position')}, {'rotation', L('blend_rotation')},
                {'scale', L('blend_scale')},
            }
            for _, component in ipairs(components) do
                local key, label = component[1], component[2]
                local previousValue = blend[key] == true
                local nextValue = tImGui.Checkbox(label, previousValue)
                if nextValue ~= previousValue then
                    blend[key] = nextValue == true
                    S.target.manualOverride = false
                    markModified()
                end
            end
        end
    end
    tImGui.End()
end

local function showCameraPanel()
    tUtil.setInitialWindowPositionRight(S.windows.camera, 0, 600, 350, 380, 380)
    local opened = tImGui.Begin(S.windows.camera, true, 0)
    if opened then
        tImGui.Text(L('camera'))
        if tUtil.drawOrbitGizmo(S.cameraOrbit, {size = 110}) then applyCamera() end
        if tImGui.Button(L('reset_camera')) then resetCamera() end
        local changed, value = tImGui.InputFloat(L('camera_distance'), S.cameraOrbit.distance)
        if changed then S.cameraOrbit.distance = math.max(80, value); applyCamera() end
        changed, value = tImGui.InputFloat(L('focus_x'), S.cameraOrbit.fx)
        if changed then S.cameraOrbit.fx = value; applyCamera() end
        changed, value = tImGui.InputFloat(L('focus_y'), S.cameraOrbit.fy)
        if changed then S.cameraOrbit.fy = value; applyCamera() end
        changed, value = tImGui.InputFloat(L('focus_z'), S.cameraOrbit.fz)
        if changed then S.cameraOrbit.fz = value; applyCamera() end
        local showOrigins = tImGui.Checkbox(L('show_origins'), S.showOrigins)
        if showOrigins ~= S.showOrigins then setOriginsVisible(showOrigins) end
        local showPlane = tImGui.Checkbox(L('show_xz_plane'), S.showXZPlane)
        if showPlane ~= S.showXZPlane then S.showXZPlane = showPlane end
    end
    tImGui.End()
end

local function showTimelinePanel()
    tUtil.setInitialWindowPositionDown(S.windows.timeline, 390, 0.20, 390)
    local opened = tImGui.Begin(S.windows.timeline, true, 0)
    if opened then
        tImGui.Text(L('timeline'))
        showSourceAnimationControls()
        tImGui.Text(string.format(L('relation_status'), roleName('source'), roleName('target')))
    end
    tImGui.End()
end

local function showMainMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(L('file')) then
        if tImGui.MenuItem(L('load')) then loadDefinition() end
        if tImGui.MenuItem(L('save')) then saveDefinition(S.projectPath) end
        if tImGui.MenuItem(L('save_as')) then saveDefinitionAs() end
        if tImGui.MenuItem(L('export_runtime')) then exportRuntimeDefinition() end
        local showStatesPressed, showStatesValue =
            tImGui.MenuItem(L('show_states_in_attachment'), nil, S.showStates)
        if showStatesPressed then S.showStates = showStatesValue end
        local showGraphPressed, showGraphValue =
            tImGui.MenuItem(L('show_graph'), nil, S.showGraph)
        if showGraphPressed then S.showGraph = showGraphValue end
        tImGui.Separator()
        if tImGui.MenuItem(L('quit')) then mbm.quit() end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(L('run')) then
        if tImGui.MenuItem(L('run_preview'), 'F5') then runPreview() end
        if tImGui.MenuItem(L('save_preview_as')) then savePreviewAs() end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(L('options')) then
        if tImGui.MenuItem(L('english')) then
            tLang.setLanguage('en')
            refreshWindowTitles()
            S.modified = true
        end
        if tImGui.MenuItem(L('portuguese')) then
            tLang.setLanguage('pt_br')
            refreshWindowTitles()
            S.modified = true
        end
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

refreshWindowTitles = function()
    S.windows.objects = L('objects')
    S.windows.attachment = L('attachment')
    S.windows.camera = L('camera')
    S.windows.timeline = L('timeline')
    S.windows.states = L('states')
    S.windows.graph = L('graph')
end

function onInitScene()
    L = function(key) return tLang.L('att_' .. key) end
    refreshWindowTitles()
    ensureTargetDefinitions()
    activateTargetDefinition(1)
    mbm.setAmbientLight('3d', {r = 0.25, g = 0.25, b = 0.25, a = 1})
    mbm.setDirectionalLight('3d', {x = -0.8, y = -0.7, z = -0.5}, {r = 1, g = 0.98, b = 0.9, a = 1})
    S.camera = mbm.getCamera('3d')
    S.camera:setFar(9999999)
    applyCamera()
    S.source.object = createMarker('source', {0.15, 0.75, 1.0, 0.42})
    S.target.object = createMarker('target', {1.0, 0.65, 0.15, 0.42})
    S.targetPivotMarker = createPivotMarker()
    S.partSelectionMarker = createPartSelectionMarker()
    S.aimPointMarker = createAimPointMarker()
    if S.targetPivotMarker then
        S.targetPivotMarker.visible = false
    end
    if S.partSelectionMarker then
        S.partSelectionMarker.visible = false
    end
    if S.aimPointMarker then
        S.aimPointMarker.visible = false
    end
    ensureRelationLine(1)
    createOriginLines()
    createXZPlane()
    tUtil.bRightSide = false
    tUtil.showMessage(L('welcome'), 8)
    rebuildPreview()
end

function onLoop(delta)
    if S.definition.spring and S.definition.spring.enabled then S.dirtyPreview = true end
    if (S.definition.sourceType == 'bone' or S.definition.sourceType == 'socket' or
            S.definition.sourceType == 'articulated_part') and
            S.source.activeAnimation ~= '' and
            not S.source.animationPaused and not isSourceSelectionMode() then
        S.dirtyPreview = true
    end
    if S.dirtyPreview then rebuildPreview(delta) end
    showMainMenu()
    showObjects()
    showGraphPanel()
    showAttachment()
    showStatePanel()
    showCameraPanel()
    showTimelinePanel()
    updateGizmo()
    updateXZPlane()
    tUtil.showOverlayMessage()
end

function onKeyDown(key)
    if key == mbm.getKeyCode('F5') then
        runPreview()
        return
    end
    if key == mbm.getKeyCode('ESC') then
        if S.boneSelectionMode then setBoneSelectionMode(false) end
        if S.partSelectionMode then setPartSelectionMode(false) end
        S.selectedRole = nil
        hideGizmo()
        return
    end
    if key == mbm.getKeyCode('control') then S.control = true end
    if S.control and key == mbm.getKeyCode('S') then saveDefinition(S.projectPath) end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then S.control = false end
end

function onTouchZoom(zoom)
    if not tImGui.GetWantCaptureMouse() then
        local minimum = getBoneSelectionMinCameraDistance()
        S.cameraOrbit.distance = math.max(minimum, S.cameraOrbit.distance * (1 - zoom * 0.15))
        applyCamera()
    end
end

function onTouchDown(key, x, y)
    beginViewportInteraction(key, x, y)
end

function onTouchMove(key, x, y)
    updateViewportInteraction(x, y)
end

function onTouchUp(key, x, y)
    endViewportInteraction(key)
end

function onFinalizeScene()
    for _, object in pairs(S.gizmo.axes) do object:destroy() end
    for _, object in pairs(S.gizmo.rings) do object:destroy() end
    for _, object in pairs(S.aimLimitsVisual.objects) do object:destroy() end
    for _, handle in pairs(S.aimLimitsVisual.handles or {}) do handle.object:destroy() end
    S.gizmo.axes, S.gizmo.rings = {}, {}
    S.aimLimitsVisual.objects = {}
    S.aimLimitsVisual.handles = {}
    destroySourceSkeletonVisuals()
    if S.source.object then S.source.object:destroy(); S.source.object = nil end
    if S.source.debugMesh then S.source.debugMesh:destroy(); S.source.debugMesh = nil end
    for _, targetState in ipairs(S.targets) do
        if targetState.object then targetState.object:destroy(); targetState.object = nil end
    end
    if S.targetPivotMarker then S.targetPivotMarker:destroy(); S.targetPivotMarker = nil end
    if S.partSelectionMarker then S.partSelectionMarker:destroy(); S.partSelectionMarker = nil end
    if S.aimPointMarker then S.aimPointMarker:destroy(); S.aimPointMarker = nil end
    for index, relation in pairs(S.relationLines) do
        relation:destroy()
        S.relationLines[index] = nil
    end
    S.relationLine = nil
    if S.xzPlane then S.xzPlane:destroy(); S.xzPlane = nil end
    for key, object in pairs(S.originLines) do
        if object then object:destroy(); S.originLines[key] = nil end
    end
end
