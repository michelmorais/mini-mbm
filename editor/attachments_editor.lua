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

   Independent Phase 1 authoring tool for root-transform attachments.
   Its layout follows the interaction animation editor, but its state and
   saved definition are independent from interaction_animation_editor.lua.
]]--

tImGui = require 'ImGui'
tUtil = require 'editor_utils'

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
        version = 1,
        source = 'source',
        target = 'target',
        sourceType = 'root',
        inherit = {position = true, rotation = true, scale = false},
        offset = {
            position = vector(100, 0, 0),
            rotation = vector(0, 0, 0),
            scale = vector(1, 1, 1),
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
    source = {pose = pose(0, 0, 0), object = nil},
    target = {pose = pose(100, 0, 0), object = nil, manualOverride = false},
    relationLine = nil,
    originLines = {x = nil, y = nil, z = nil},
    xzPlane = nil,
    xzPlaneSize = nil,
    xzPlaneX = nil,
    xzPlaneZ = nil,
    showOrigins = true,
    showXZPlane = true,
    camera = nil,
    cameraOrbit = {azimuth = 0.3, elevation = 0.3, distance = 700, fx = 50, fy = 30, fz = 0},
    cameraDrag = nil,
    windows = {objects = '', attachment = '', camera = '', preview = ''},
}

local L
local refreshWindowTitles

local function setStatus(message, warning)
    S.status = tostring(message or '')
    if warning then tUtil.showMessageWarn(S.status, 6) else tUtil.showMessage(S.status, 6) end
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

local function rotateVector(matrix, value)
    return vector(
        matrix[1][1] * value.x + matrix[1][2] * value.y + matrix[1][3] * value.z,
        matrix[2][1] * value.x + matrix[2][2] * value.y + matrix[2][3] * value.z,
        matrix[3][1] * value.x + matrix[3][2] * value.y + matrix[3][3] * value.z)
end

local function roleEntry(role)
    return role == 'source' and S.source or S.target
end

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
    S.xzPlane.visible = S.showXZPlane
end

local function applyMarker(role, value)
    local entry = roleEntry(role)
    if not entry.object then return end
    entry.object:setPos(value.x, value.y, value.z)
    entry.object:setAngle(value.rx, value.ry, value.rz)
    local size = role == 'source' and 70 or 50
    entry.object:setScale(size * value.sx, size * value.sy, size * value.sz)
end

local function calculateTargetPose()
    local source, target = S.source.pose, S.target.pose
    local result = clonePose(target)
    local inherit = S.definition.inherit
    local offset = S.definition.offset
    local sourceRotation = eulerRotation(source.rx, source.ry, source.rz)
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
    if inherit.rotation then
        local targetRotation = multiplyRotation(sourceRotation,
            eulerRotation(offset.rotation.x, offset.rotation.y, offset.rotation.z))
        result.rx, result.ry, result.rz = matrixToEuler(targetRotation)
    end
    if inherit.scale then
        result.sx = source.sx * offset.scale.x
        result.sy = source.sy * offset.scale.y
        result.sz = source.sz * offset.scale.z
    end
    return result
end

local function rebuildPreview()
    if not S.target.manualOverride then S.target.pose = calculateTargetPose() end
    applyMarker('source', S.source.pose)
    applyMarker('target', S.target.pose)
    if S.relationLine then
        S.relationLine:set({S.source.pose.x, S.source.pose.y, S.source.pose.z,
            S.target.pose.x, S.target.pose.y, S.target.pose.z}, 1)
    end
    S.dirtyPreview = false
end

local function captureOffset()
    local source, target = S.source.pose, S.target.pose
    local inherit = S.definition.inherit
    local sourceRotation = eulerRotation(source.rx, source.ry, source.rz)
    local inverseSource = transposeRotation(sourceRotation)
    local delta = vector(target.x - source.x, target.y - source.y, target.z - source.z)
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
    setStatus(L('preview_reset'))
end

local function saveDefinition(path)
    if not path or path == '' then path = mbm.saveFile(S.projectPath or '', 'att.lua') end
    if not path then return end
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

local function normalizeDefinition(value)
    if type(value) ~= 'table' then return nil end
    local result = newDefinition()
    result.source = tostring(value.source or result.source)
    result.target = tostring(value.target or result.target)
    result.sourceType = tostring(value.sourceType or 'root')
    if type(value.inherit) == 'table' then
        result.inherit.position = value.inherit.position ~= false
        result.inherit.rotation = value.inherit.rotation ~= false
        result.inherit.scale = value.inherit.scale == true
    end
    for _, component in ipairs({'position', 'rotation', 'scale'}) do
        if type(value.offset) == 'table' and type(value.offset[component]) == 'table' then
            for _, axis in ipairs({'x', 'y', 'z'}) do
                local fallback = result.offset[component][axis]
                result.offset[component][axis] = tonumber(value.offset[component][axis]) or fallback
            end
        end
    end
    if result.sourceType ~= 'root' then return nil end
    return result
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
    S.definition = definition
    S.projectPath, S.modified = path, false
    S.target.manualOverride = false
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
        local sourceChanged, sourceName = tImGui.InputText(L('source_name'), S.definition.source)
        if sourceChanged then S.definition.source = sourceName; markModified() end
        if tImGui.Selectable(L('target') .. ': ' .. roleName('target'), S.selectedRole == 'target') then
            S.selectedRole = 'target'
        end
        local targetChanged, targetName = tImGui.InputText(L('target_name'), S.definition.target)
        if targetChanged then S.definition.target = targetName; markModified() end
        tImGui.Separator()
        tImGui.Text(string.format(L('selected_object'), roleName(S.selectedRole)))
        showPoseFields(S.selectedRole)
        if S.selectedRole == 'target' then tImGui.TextWrapped(L('target_pose_help'))
        else tImGui.TextWrapped(L('source_pose_help')) end
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
                S.target.manualOverride = false
                markModified()
            end
            if axis ~= 'z' then tImGui.SameLine() end
        end
    end
end

local function showAttachment()
    tUtil.setInitialWindowPositionRight(S.windows.attachment, 0, 20, 350, 360, 560)
    local opened = tImGui.Begin(S.windows.attachment, true, 0)
    if opened then
        tImGui.Text(L('attachment'))
        tImGui.Text(string.format('%s -> %s', roleName('source'), roleName('target')))
        tImGui.Text(L('root_transform'))
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
        tImGui.Separator()
        if tImGui.Button(L('capture')) then captureOffset() end
        tImGui.SameLine()
        if tImGui.Button(L('reset_preview')) then resetTargetPreview() end
        tImGui.TextWrapped(L('offset_help'))
        tImGui.Text(S.status .. (S.modified and ' *' or ''))
    end
    tImGui.End()
end

local function showCameraPanel()
    tUtil.setInitialWindowPositionRight(S.windows.camera, 0, 390, 350, 360, 430)
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

local function showPreviewPanel()
    tUtil.setInitialWindowPositionDown(S.windows.preview, 390, 0.16, 350)
    local opened = tImGui.Begin(S.windows.preview, true, 0)
    if opened then
        tImGui.Text(L('preview'))
        tImGui.TextWrapped(L('preview_help'))
        if tImGui.Button(L('capture')) then captureOffset() end
        tImGui.SameLine()
        if tImGui.Button(L('reset_preview')) then resetTargetPreview() end
        tImGui.SameLine()
        if tImGui.Button(L('save')) then saveDefinition(S.projectPath) end
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
        tImGui.Separator()
        if tImGui.MenuItem(L('quit')) then mbm.quit() end
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
    S.windows.preview = L('preview')
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
    if S.dirtyPreview then rebuildPreview() end
    showMainMenu()
    showObjects()
    showAttachment()
    showCameraPanel()
    showPreviewPanel()
    updateXZPlane()
    tUtil.showOverlayMessage()
end

function onKeyDown(key)
    if key == mbm.getKeyCode('ESC') then mbm.quit() end
    if key == mbm.getKeyCode('control') then S.control = true end
    if S.control and key == mbm.getKeyCode('S') then saveDefinition(S.projectPath) end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then S.control = false end
end

function onTouchZoom(zoom)
    if not tImGui.GetWantCaptureMouse() then
        S.cameraOrbit.distance = math.max(80, S.cameraOrbit.distance - zoom * 40)
        applyCamera()
    end
end

function onTouchDown(key, x, y)
    if tImGui.GetWantCaptureMouse() then return end
    if key == 0 then S.cameraDrag = {kind = 'orbit', startX = x, startY = y} end
    if key == 1 then S.cameraDrag = {kind = 'pan', startX = x, startY = y} end
end

function onTouchMove(key, x, y)
    local drag = S.cameraDrag
    if not drag or tImGui.GetWantCaptureMouse() then return end
    local dx, dy = x - drag.startX, y - drag.startY
    if drag.kind == 'orbit' then
        S.cameraOrbit.azimuth = S.cameraOrbit.azimuth + dx * 0.008
        S.cameraOrbit.elevation = math.max(-1.45, math.min(1.45, S.cameraOrbit.elevation + dy * 0.008))
    else
        local right = S.camera:getNormal('R')
        local scale = S.cameraOrbit.distance * 0.0015
        S.cameraOrbit.fx = S.cameraOrbit.fx - right.x * dx * scale
        S.cameraOrbit.fy = S.cameraOrbit.fy + dy * scale
        S.cameraOrbit.fz = S.cameraOrbit.fz - right.z * dx * scale
    end
    drag.startX, drag.startY = x, y
    applyCamera()
end

function onTouchUp(key, x, y)
    if key == 0 or key == 1 then S.cameraDrag = nil end
end

function onFinalizeScene()
    if S.source.object then S.source.object:destroy(); S.source.object = nil end
    if S.target.object then S.target.object:destroy(); S.target.object = nil end
    if S.relationLine then S.relationLine:destroy(); S.relationLine = nil end
    if S.xzPlane then S.xzPlane:destroy(); S.xzPlane = nil end
    for key, object in pairs(S.originLines) do
        if object then object:destroy(); S.originLines[key] = nil end
    end
end
