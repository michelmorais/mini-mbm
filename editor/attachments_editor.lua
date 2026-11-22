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

   Independent authoring tool for root-transform and skeletal-bone attachments.
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

local function newDefinition()
    return {
        version = 3,
        source = 'source',
        target = 'target',
        sourceType = 'root',
        sourceBone = '',
        inherit = {position = true, rotation = true, scale = false},
        offset = {
            position = vector(100, 0, 0),
            rotation = vector(0, 0, 0),
            rotationMatrix = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
            scale = vector(1, 1, 1),
        },
        targetPivot = vector(0, 0, 0),
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
        animationClips = {}, selectedAnimation = 1, activeAnimation = '', animationPaused = false,
        selectionAnimation = nil},
    target = {pose = pose(100, 0, 0), object = nil, manualOverride = false, kind = 'placeholder',
        path = '', previewSize = 50, meshLoaded = false, selectable = true},
    relationLine = nil,
    targetPivotMarker = nil,
    targetPivotMarkerRadius = nil,
    targetPivotMarkerSignature = nil,
    frameSourceAxes = {},
    frameSourceAxesSignature = nil,
    originLines = {x = nil, y = nil, z = nil},
    xzPlane = nil,
    xzPlaneSize = nil,
    xzPlaneX = nil,
    xzPlaneZ = nil,
    showOrigins = true,
    showXZPlane = true,
    boneSelectionMode = false,
    boneSelectionCamera = nil,
    boneSelectionTargetVisible = nil,
    camera = nil,
    cameraOrbit = {azimuth = 0.3, elevation = 0.3, distance = 700, fx = 50, fy = 30, fz = 0},
    cameraDrag = nil,
    gizmo = {axes = {}, rings = {}, origin = nil, length = nil, radius = nil,
        axesSignature = nil, drag = nil},
    windows = {objects = '', attachment = '', camera = '', timeline = ''},
}

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

local function unwrapAngle(value, reference)
    if reference == nil then return value end
    local turns = math.floor((reference - value) / (math.pi * 2) + 0.5)
    return value + turns * math.pi * 2
end

local function rotateVector(matrix, value)
    return vector(
        matrix[1][1] * value.x + matrix[2][1] * value.y + matrix[3][1] * value.z,
        matrix[1][2] * value.x + matrix[2][2] * value.y + matrix[3][2] * value.z,
        matrix[1][3] * value.x + matrix[2][3] * value.y + matrix[3][3] * value.z)
end

local function roleEntry(role)
    return role == 'source' and S.source or S.target
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
local updateTargetPivotMarker
local showTargetPivotFields
local getPreviewSourcePose
local getAttachmentFrameAxes
local captureOffset

local function roleName(role)
    return role == 'source' and S.definition.source or S.definition.target
end

local function markModified()
    S.modified = true
    S.dirtyPreview = true
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
    entry.selectedBone = ''
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
    local okTotal, total = dpCall(function() return source.object:getTotalSkeletalAnimations() end)
    if not okTotal then return end
    for index = 1, tonumber(total) or 0 do
        local okName, name = dpCall(function() return source.object:getSkeletalAnimationName(index) end)
        if okName and type(name) == 'string' and name ~= '' then
            source.animationClips[#source.animationClips + 1] = name
        end
    end
end

local function restoreSourceAnimation()
    local source = S.source
    local saved = source.selectionAnimation
    source.selectionAnimation = nil
    if not saved or saved.name == '' or not source.object then return end
    local ok, played = dpCall(function() return source.object:playSkeletalAnimation(saved.name) end)
    if not ok or played == false then return end
    source.activeAnimation = saved.name
    source.animationPaused = saved.paused == true
    if source.animationPaused then dpCall(function() return source.object:pauseSkeletalAnimation() end) end
    S.dirtyPreview = true
end

local function playSourceAnimation()
    local source = S.source
    if S.boneSelectionMode then
        setStatus(L('animation_unavailable_in_bone_mode'), true)
        return
    end
    local name = source.animationClips[source.selectedAnimation]
    if not source.object or not name then return end
    if source.activeAnimation == name and source.animationPaused then
        resumeSourceAnimation()
        return
    end
    local ok, played = dpCall(function() return source.object:playSkeletalAnimation(name) end)
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
    local ok, paused = dpCall(function() return S.source.object:pauseSkeletalAnimation() end)
    if ok and paused ~= false then
        S.source.animationPaused = true
        S.dirtyPreview = true
    end
end

local function resumeSourceAnimation()
    if not S.source.object or S.source.activeAnimation == '' then return end
    local ok, resumed = dpCall(function() return S.source.object:resumeSkeletalAnimation() end)
    if ok and resumed ~= false then
        S.source.animationPaused = false
        S.dirtyPreview = true
    end
end

local function stopSourceAnimation()
    if not S.source.object then return end
    dpCall(function() return S.source.object:stopSkeletalAnimation() end)
    S.source.activeAnimation = ''
    S.source.animationPaused = false
    S.dirtyPreview = true
end

local function sourceAnimationDuration()
    local source = S.source
    if not source.object or #source.animationClips == 0 then return 0 end
    local ok, duration = dpCall(function()
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
    dpCall(function() return source.object:pauseSkeletalAnimation() end)
    local ok, seeked = dpCall(function() return source.object:seekSkeletalAnimation(math.max(0, time or 0)) end)
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
        refreshSourceAnimationCatalog()
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

local function getSourceMeshBounds()
    local source = S.source
    if source.meshBounds then return source.meshBounds end
    if not source.debugMesh then return nil end
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

local function getBoneSelectionMinCameraDistance()
    if not S.boneSelectionMode then return 80 end
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
        S.boneSelectionTargetVisible = S.target.object and S.target.object.visible ~= false
        S.boneSelectionMode = true
        if S.target.object then S.target.object.visible = false end
        if S.relationLine then S.relationLine.visible = false end
        if S.targetPivotMarker then S.targetPivotMarker.visible = false end
        if S.xzPlane then S.xzPlane.visible = false end
        updateSourceSkeletonVisibility()
        frameBoneSelectionCamera()
        hideGizmo()
        return true
    end
    S.boneSelectionMode = false
    if S.target.object then S.target.object.visible = S.boneSelectionTargetVisible ~= false end
    if S.relationLine then S.relationLine.visible = true end
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
    S.xzPlane.visible = S.showXZPlane and not S.boneSelectionMode
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

getAttachmentFrameAxes = function()
    local source = getPreviewSourcePose() or S.source.pose
    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
    local offsetRotation = S.definition.offset.rotationMatrix or
        eulerRotation(S.definition.offset.rotation.x, S.definition.offset.rotation.y,
            S.definition.offset.rotation.z)
    local frameRotation = multiplyRotation(sourceRotation, offsetRotation)
    return {
        x = rotateVector(frameRotation, vector(1, 0, 0)),
        y = rotateVector(frameRotation, vector(0, 1, 0)),
        z = rotateVector(frameRotation, vector(0, 0, 1)),
    }, sourceRotation, frameRotation
end

local function updateAttachmentSourceAxes()
    local visible = not S.boneSelectionMode and S.selectedRole == 'target' and
        S.transformMode == 'frame'
    if not visible then
        for _, object in pairs(S.frameSourceAxes) do object.visible = false end
        S.frameSourceAxesSignature = nil
        return
    end
    local source = getPreviewSourcePose() or S.source.pose
    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
    local length = math.max(S.target.previewSize * 1.1, 35)
    local axes = {
        x = {vector(1, 0, 0), {1, 0.15, 0.15, 0.55}},
        y = {vector(0, 1, 0), {0.15, 1, 0.2, 0.55}},
        z = {vector(0, 0, 1), {0.2, 0.45, 1, 0.55}},
    }
    local signatureParts = {source.x, source.y, source.z, length}
    local axisVectors = {}
    for _, name in ipairs({'x', 'y', 'z'}) do
        local axis = rotateVector(sourceRotation, axes[name][1])
        axisVectors[name] = axis
        signatureParts[#signatureParts + 1] = axis.x
        signatureParts[#signatureParts + 1] = axis.y
        signatureParts[#signatureParts + 1] = axis.z
    end
    local signature = table.concat(signatureParts, ':')
    for _, name in ipairs({'x', 'y', 'z'}) do
        local axis = axisVectors[name]
        local object = S.frameSourceAxes[name]
        if not object then
            object = line:new('3d', 0, 0, 0)
            S.frameSourceAxes[name] = object
            object.alwaysOnTop = true
            object.alwaysOnTopPriority = 8
            object.alwaysRender = true
        end
        object.visible = true
        object:setColor(table.unpack(axes[name][2]))
        if S.frameSourceAxesSignature ~= signature then
            object:set({0, 0, 0, axis.x * length, axis.y * length, axis.z * length}, 1)
            object:setPos(source.x, source.y, source.z)
        end
    end
    S.frameSourceAxesSignature = signature
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
    for _, role in ipairs({'source', 'target'}) do
        local entry = roleEntry(role)
        if entry.selectable ~= false and entry.object then
            local ok, hit = dpCall(function()
                return entry.object:collide(screenX, screenY, true)
            end)
            if ok and hit then
                return role
            end
        end
    end
    return nil
end

hideGizmo = function()
    for _, object in pairs(S.gizmo.axes) do object.visible = false end
    for _, object in pairs(S.gizmo.rings) do object.visible = false end
    S.gizmo.drag = nil
    S.gizmo.origin, S.gizmo.length, S.gizmo.radius, S.gizmo.axesSignature = nil, nil, nil, nil
end

local function getGizmoAxes()
    local axes = {
        x = {x = 1, y = 0, z = 0, color = {1, 0.15, 0.15, 1}},
        y = {x = 0, y = 1, z = 0, color = {0.15, 1, 0.2, 1}},
        z = {x = 0, y = 0, z = 1, color = {0.2, 0.45, 1, 1}},
    }
    if S.transformMode == 'frame' then
        local frameAxes = getAttachmentFrameAxes()
        for _, name in ipairs({'x', 'y', 'z'}) do
            axes[name].x, axes[name].y, axes[name].z = frameAxes[name].x,
                frameAxes[name].y, frameAxes[name].z
        end
    end
    return axes
end

local function updateGizmo()
    updateAttachmentSourceAxes()
    if S.boneSelectionMode then
        if S.targetPivotMarker then S.targetPivotMarker.visible = false end
        hideGizmo()
        return
    end
    updateTargetPivotMarker()
    if not S.selectedRole then hideGizmo(); return end
    if (S.transformMode == 'pivot' or S.transformMode == 'frame') and
            S.selectedRole ~= 'target' then hideGizmo(); return end
    local entry = roleEntry(S.selectedRole)
    if entry.selectable == false then hideGizmo(); return end
    if not entry.object then hideGizmo(); return end
    local origin = {x = entry.pose.x, y = entry.pose.y, z = entry.pose.z}
    if S.transformMode == 'pivot' or S.transformMode == 'frame' then
        origin = targetPivotWorldPosition(entry.pose)
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
            S.transformMode == 'frame'
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
        object.visible = S.transformMode == 'rotate' or S.transformMode == 'frame'
    end
    S.gizmo.origin, S.gizmo.length, S.gizmo.radius, S.gizmo.axesSignature = origin, length, radius,
        axesSignature
end

local function pickGizmoAxis(screenX, screenY)
    if not S.selectedRole or (S.transformMode ~= 'move' and S.transformMode ~= 'pivot') or
            not S.gizmo.origin or
            roleEntry(S.selectedRole).selectable == false then
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
    if not S.selectedRole or (S.transformMode ~= 'rotate' and S.transformMode ~= 'frame') or
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

local function beginViewportInteraction(key, screenX, screenY)
    if tImGui.GetWantCaptureMouse() then return end
    if key == 0 then
        local ring = pickRotationRing(screenX, screenY)
        if ring then
            local entry = roleEntry(S.selectedRole)
            local drag = {
                mode = S.transformMode, axis = ring.axis, origin = {x = S.gizmo.origin.x, y = S.gizmo.origin.y,
                    z = S.gizmo.origin.z}, startDirection = ring.direction, base = clonePose(entry.pose),
                lastAngle = 0, accumulatedAngle = 0,
                lastEuler = {rx = entry.pose.rx, ry = entry.pose.ry, rz = entry.pose.rz},
            }
            if S.transformMode == 'frame' then
                local _, sourceRotation, frameRotation = getAttachmentFrameAxes()
                drag.baseSourceRotation = cloneRotation(sourceRotation)
                drag.baseFrameWorldRotation = cloneRotation(frameRotation)
            end
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
        local picked = pickRole(screenX, screenY)
        if picked then
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
        elseif drag.mode == 'frame' then
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
                local worldRotation = multiplyRotation(axisRotation(drag.axis, drag.accumulatedAngle),
                    drag.baseFrameWorldRotation)
                local localRotation = multiplyRotation(transposeRotation(drag.baseSourceRotation),
                    worldRotation)
                S.definition.offset.rotationMatrix = cloneRotation(localRotation)
                local rx, ry, rz = matrixToEuler(localRotation)
                S.definition.offset.rotation = vector(rx, ry, rz)
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

local function calculateTargetPose()
    local source, target = getPreviewSourcePose() or S.source.pose, S.target.pose
    local result = clonePose(target)
    local inherit = S.definition.inherit
    local offset = S.definition.offset
    local sourceRotation = source.rotation or eulerRotation(source.rx, source.ry, source.rz)
    local targetRotation = eulerRotation(target.rx, target.ry, target.rz)
    if inherit.rotation then
        local offsetRotation = S.definition.offset.rotationMatrix or
            eulerRotation(offset.rotation.x, offset.rotation.y, offset.rotation.z)
        targetRotation = multiplyRotation(sourceRotation,
            offsetRotation)
        result.rx, result.ry, result.rz = matrixToEuler(targetRotation)
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
        local pivot = vector(S.definition.targetPivot.x * result.sx,
            S.definition.targetPivot.y * result.sy, S.definition.targetPivot.z * result.sz)
        local worldPivot = rotateVector(targetRotation, pivot)
        result.x = result.x - worldPivot.x
        result.y = result.y - worldPivot.y
        result.z = result.z - worldPivot.z
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

updateTargetPivotMarker = function()
    if not S.targetPivotMarker then return end
    local targetPivot = targetPivotWorldPosition(S.target.pose)
    S.targetPivotMarker:setPos(targetPivot.x, targetPivot.y, targetPivot.z)
    local radius = getTargetPivotMarkerRadius()
    S.targetPivotMarker:setScale(radius, radius, radius)
    S.targetPivotMarker.visible = not S.boneSelectionMode and
        (S.selectedRole == 'target' or S.transformMode == 'pivot' or
         math.abs(S.definition.targetPivot.x) > 1e-6 or
         math.abs(S.definition.targetPivot.y) > 1e-6 or
         math.abs(S.definition.targetPivot.z) > 1e-6)
end

local function rebuildPreview()
    if not S.target.manualOverride then S.target.pose = calculateTargetPose() end
    applyMarker('source', S.source.pose)
    updateSourceSkeletonVisuals()
    applyMarker('target', S.target.pose)
    if S.relationLine then
        local source = getPreviewSourcePose() or S.source.pose
        local targetPivot = targetPivotWorldPosition(S.target.pose)
        S.relationLine:set({source.x, source.y, source.z,
            targetPivot.x, targetPivot.y, targetPivot.z}, 1)
    end
    updateTargetPivotMarker()
    S.dirtyPreview = false
end

captureOffset = function(targetPoseOverride)
    local source = getPreviewSourcePose() or S.source.pose
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
    local relativeRotation = multiplyRotation(inverseSource, targetRotation)
    local rx, ry, rz = matrixToEuler(relativeRotation)
    S.definition.offset.position = localPosition
    S.definition.offset.rotation = vector(rx, ry, rz)
    S.definition.offset.rotationMatrix = cloneRotation(relativeRotation)
    if inherit.scale then
        S.definition.offset.scale = vector(target.sx / source.sx, target.sy / source.sy, target.sz / source.sz)
    end
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
local target
local previewRelation = nil

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
    local pose = editor[role .. 'Pose'] or {}
    local object
    if editor[role .. 'Kind'] == 'mesh' and editor[role .. 'Mesh'] and editor[role .. 'Mesh'] ~= '' then
        local path = editor[role .. 'Mesh']
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
    target = loadRole('target', definition.editor)
    local camera = mbm.getCamera('3d')
    local orbit = project.camera or {azimuth = 0.3, elevation = 0.3, distance = 700,
        fx = 50, fy = 30, fz = 0}
    local cameraX = orbit.fx + orbit.distance * math.cos(orbit.elevation) * math.sin(orbit.azimuth)
    local cameraY = orbit.fy + orbit.distance * math.sin(orbit.elevation)
    local cameraZ = orbit.fz + orbit.distance * math.cos(orbit.elevation) * math.cos(orbit.azimuth)
    camera:setPos(cameraX, cameraY, cameraZ)
    camera:setFocus(orbit.fx, orbit.fy, orbit.fz)
    local clip = definition.editor.sourceAnimation
    if clip and clip ~= '' then assert(source:playSkeletalAnimation(clip)) end
    if definition.editor.sourceAnimationPaused and clip and clip ~= '' then
        assert(source:pauseSkeletalAnimation())
    end
end

function onLoop()
    if not previewRelation then
        if definition.sourceType == 'bone' and
                not source:getSkeletalBoneTransform(definition.sourceBone, 'world') then
            return
        end
        local specification = {
            source = source,
            target = target,
            sourceType = definition.sourceType,
            inherit = definition.inherit,
            offset = definition.offset,
            targetPivot = definition.targetPivot,
        }
        if definition.sourceBone and definition.sourceBone ~= '' then
            specification.sourceBone = definition.sourceBone
        end
        local relation, errorMessage = attachments.attach(specification)
        assert(relation, errorMessage)
        previewRelation = relation
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
    if S.source.kind ~= 'mesh' or not S.source.meshLoaded then
        setStatus(L('run_preview_requires_source'), true)
        return false
    end
    if S.definition.sourceType == 'bone' and
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
    result.sourceType = value.sourceType == 'bone' and 'bone' or 'root'
    result.sourceBone = type(value.sourceBone) == 'string' and value.sourceBone or ''
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
    if S.boneSelectionMode then setBoneSelectionMode(false) end
    S.definition = definition
    S.source.pose = clonePose(definition.editor.sourcePose)
    S.target.pose = clonePose(definition.editor.targetPose)
    restoreRoleAsset('source', definition.editor.sourceKind, definition.editor.sourceMesh)
    restoreRoleAsset('target', definition.editor.targetKind, definition.editor.targetMesh)
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
    S.definition.version = math.max(3, S.definition.version)
    S.source.selectable = definition.editor.sourceSelectable ~= false
    S.target.selectable = definition.editor.targetSelectable ~= false
    if definition.editor.language == 'pt_br' or definition.editor.language == 'en' then
        tLang.setLanguage(definition.editor.language)
        refreshWindowTitles()
    end
    S.projectPath, S.modified = path, false
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
        local ok, time = dpCall(function() return source.object:getSkeletalAnimationTime() end)
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
        if tImGui.Selectable(L('source') .. ': ' .. roleName('source'), S.selectedRole == 'source') then
            S.selectedRole = 'source'
        end
        tImGui.SetNextItemWidth(210)
        local sourceChanged, sourceName = tImGui.InputText(L('source_name'), S.definition.source)
        if sourceChanged then S.definition.source = sourceName; markModified() end
        showViewSelectable('source')
        showRoleAsset('source')
        if tImGui.Selectable(L('target') .. ': ' .. roleName('target'), S.selectedRole == 'target') then
            S.selectedRole = 'target'
        end
        tImGui.SetNextItemWidth(210)
        local targetChanged, targetName = tImGui.InputText(L('target_name'), S.definition.target)
        if targetChanged then S.definition.target = targetName; markModified() end
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
            (S.transformMode == 'frame' and S.selectedRole == 'target' and 4 or 1))
        local modeIndex = tImGui.RadioButton(L('move'), currentModeIndex, 1)
        tImGui.SameLine()
        modeIndex = tImGui.RadioButton(L('rotate'), modeIndex, 2)
        if S.selectedRole == 'target' then
            tImGui.SameLine()
            modeIndex = tImGui.RadioButton(L('move_pivot'), modeIndex, 3)
            tImGui.SameLine()
            modeIndex = tImGui.RadioButton(L('orient_frame'), modeIndex, 4)
        end
        local nextMode
        if modeIndex == 2 then nextMode = 'rotate'
        elseif modeIndex == 3 and S.selectedRole == 'target' then nextMode = 'pivot'
        elseif modeIndex == 4 and S.selectedRole == 'target' then nextMode = 'frame'
        else nextMode = 'move' end
        if nextMode ~= S.transformMode then
            S.transformMode = nextMode
            S.gizmo.drag = nil
            S.dirtyPreview = true
        end
        local help = L('move_gizmo_help')
        if S.transformMode == 'rotate' then help = L('rotate_gizmo_help')
        elseif S.transformMode == 'pivot' then help = L('move_pivot_gizmo_help') end
        if S.transformMode == 'frame' then help = L('orient_frame_gizmo_help') end
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

local function showAttachment()
    tUtil.setInitialWindowPositionRight(S.windows.attachment, 0, 20, 350, 380, 560)
    local opened = tImGui.Begin(S.windows.attachment, true, 0)
    if opened then
        tImGui.Text(L('attachment'))
        tImGui.Text(string.format('%s -> %s', roleName('source'), roleName('target')))
        tImGui.SetNextItemWidth(210)
        local sourceTypeIndex = S.definition.sourceType == 'bone' and 2 or 1
        local sourceTypeChanged, nextSourceType = tImGui.Combo(L('source_type'), sourceTypeIndex,
            {L('root_transform'), L('bone_transform')})
        if sourceTypeChanged then
            S.definition.sourceType = nextSourceType == 2 and 'bone' or 'root'
            if S.definition.sourceType == 'root' then
                S.definition.sourceBone = ''
                setBoneSelectionMode(false)
            else
                setBoneSelectionMode(true)
            end
            S.target.manualOverride = false
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
        end
        local components = {{'position', L('inherit_position')}, {'rotation', L('inherit_rotation')},
            {'scale', L('inherit_scale')}}
        for _, component in ipairs(components) do
            local key, label = component[1], component[2]
            local previousValue = S.definition.inherit[key] == true
            local nextValue = tImGui.Checkbox(label, previousValue)
            if nextValue ~= previousValue then
                S.definition.inherit[key] = nextValue
                S.target.manualOverride = false
                markModified()
            end
        end
        tImGui.Separator()
        showOffsetFields()
        tImGui.TextWrapped(L('offset_help'))
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
end

function onInitScene()
    L = function(key) return tLang.L('att_' .. key) end
    refreshWindowTitles()
    mbm.setAmbientLight('3d', {r = 0.25, g = 0.25, b = 0.25, a = 1})
    mbm.setDirectionalLight('3d', {x = -0.8, y = -0.7, z = -0.5}, {r = 1, g = 0.98, b = 0.9, a = 1})
    S.camera = mbm.getCamera('3d')
    S.camera:setFar(9999999)
    applyCamera()
    S.source.object = createMarker('source', {0.15, 0.75, 1.0, 0.42})
    S.target.object = createMarker('target', {1.0, 0.65, 0.15, 0.42})
    S.targetPivotMarker = createPivotMarker()
    if S.targetPivotMarker then
        S.targetPivotMarker.visible = false
    end
    S.relationLine = line:new('3d', 0, 0, 0)
    S.relationLine:add({0, 0, 0, 100, 0, 0})
    S.relationLine:setColor(0.85, 0.85, 0.85, 1)
    S.relationLine.alwaysOnTop = true
    S.relationLine.alwaysOnTopPriority = 2
    S.relationLine.alwaysRender = true
    createOriginLines()
    createXZPlane()
    tUtil.bRightSide = false
    tUtil.showMessage(L('welcome'), 8)
    rebuildPreview()
end

function onLoop(delta)
    if S.definition.sourceType == 'bone' and S.source.activeAnimation ~= '' and
            not S.source.animationPaused and not S.boneSelectionMode then
        S.dirtyPreview = true
    end
    if S.dirtyPreview then rebuildPreview() end
    showMainMenu()
    showObjects()
    showAttachment()
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
    S.gizmo.axes, S.gizmo.rings = {}, {}
    for _, object in pairs(S.frameSourceAxes) do object:destroy() end
    S.frameSourceAxes, S.frameSourceAxesSignature = {}, nil
    destroySourceSkeletonVisuals()
    if S.source.object then S.source.object:destroy(); S.source.object = nil end
    if S.source.debugMesh then S.source.debugMesh:destroy(); S.source.debugMesh = nil end
    if S.target.object then S.target.object:destroy(); S.target.object = nil end
    if S.targetPivotMarker then S.targetPivotMarker:destroy(); S.targetPivotMarker = nil end
    if S.relationLine then S.relationLine:destroy(); S.relationLine = nil end
    if S.xzPlane then S.xzPlane:destroy(); S.xzPlane = nil end
    for key, object in pairs(S.originLines) do
        if object then object:destroy(); S.originLines[key] = nil end
    end
end
