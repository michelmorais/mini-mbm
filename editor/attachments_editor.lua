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
        editor = {
            sourceKind = 'placeholder',
            targetKind = 'placeholder',
            sourceMesh = '',
            targetMesh = '',
            sourcePose = pose(0, 0, 0),
            targetPose = pose(100, 0, 0),
            targetManualOverride = false,
            sourceSelectable = true,
            targetSelectable = true,
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
        previewSize = 70, meshLoaded = false, selectable = true},
    target = {pose = pose(100, 0, 0), object = nil, manualOverride = false, kind = 'placeholder',
        path = '', previewSize = 50, meshLoaded = false, selectable = true},
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
    gizmo = {axes = {}, rings = {}, origin = nil, length = nil, radius = nil, drag = nil},
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

local function directoryOf(path)
    return type(path) == 'string' and path:match('^(.*)[/\\]') or ''
end

local function destroyRoleObject(role)
    local entry = roleEntry(role)
    if entry.object then entry.object:destroy(); entry.object = nil end
    entry.meshLoaded = false
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
    markModified()
    setStatus(string.format(L('mesh_loaded'), roleName(role), path))
    return true
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
    S.xzPlane.visible = S.showXZPlane
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

local function raySphereDistance(screenX, screenY, center, radius)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, screenX, screenY)
    if not ok then return nil end
    local lx, ly, lz = ox - center.x, oy - center.y, oz - center.z
    local b = dot(lx, ly, lz, dx, dy, dz)
    local c = dot(lx, ly, lz, lx, ly, lz) - radius * radius
    local discriminant = b * b - c
    if discriminant < 0 then return nil end
    local near = -b - math.sqrt(discriminant)
    return near >= 0 and near or -b + math.sqrt(discriminant)
end

local function pickRole(screenX, screenY)
    local bestRole, bestDistance
    for _, role in ipairs({'source', 'target'}) do
        local entry = roleEntry(role)
        if entry.selectable ~= false then
            local scale = math.max(math.abs(entry.pose.sx), math.abs(entry.pose.sy), math.abs(entry.pose.sz))
            local radius = math.max(entry.previewSize * scale, 25)
            local distance = raySphereDistance(screenX, screenY, entry.pose, radius)
            if distance and (not bestDistance or distance < bestDistance) then
                bestRole, bestDistance = role, distance
            end
        end
    end
    return bestRole
end

local function hideGizmo()
    for _, object in pairs(S.gizmo.axes) do object.visible = false end
    for _, object in pairs(S.gizmo.rings) do object.visible = false end
    S.gizmo.origin, S.gizmo.length, S.gizmo.radius = nil, nil, nil
end

local function updateGizmo()
    local entry = roleEntry(S.selectedRole)
    if not entry.object then hideGizmo(); return end
    local origin = {x = entry.pose.x, y = entry.pose.y, z = entry.pose.z}
    local length = math.max(entry.previewSize * 1.6, 45)
    local radius = math.max(entry.previewSize * 0.9, 30)
    local originChanged = not S.gizmo.origin or S.gizmo.origin.x ~= origin.x or
        S.gizmo.origin.y ~= origin.y or S.gizmo.origin.z ~= origin.z
    local lengthChanged = S.gizmo.length ~= length
    local radiusChanged = S.gizmo.radius ~= radius
    local axes = {
        x = {x = 1, y = 0, z = 0, color = {1, 0.15, 0.15, 1}},
        y = {x = 0, y = 1, z = 0, color = {0.15, 1, 0.2, 1}},
        z = {x = 0, y = 0, z = 1, color = {0.2, 0.45, 1, 1}},
    }
    for name, axis in pairs(axes) do
        local object = S.gizmo.axes[name]
        local points = {0, 0, 0, axis.x * length, axis.y * length, axis.z * length}
        if not object then
            object = line:new('3d', 0, 0, 0)
            object:add(points)
            S.gizmo.axes[name] = object
        elseif lengthChanged then
            object:set(points, 1)
        end
        if originChanged then object:setPos(origin.x, origin.y, origin.z) end
        object:setColor(table.unpack(axis.color))
        object.alwaysOnTop, object.alwaysOnTopPriority, object.alwaysRender = true, 10, true
        object.visible = S.transformMode == 'move'
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
        elseif radiusChanged then
            object:set(points, 1)
        end
        if originChanged then object:setPos(origin.x, origin.y, origin.z) end
        object:setColor(table.unpack(axes[name].color))
        object.alwaysOnTop, object.alwaysOnTopPriority, object.alwaysRender = true, 20, true
        object.visible = S.transformMode == 'rotate'
    end
    S.gizmo.origin, S.gizmo.length, S.gizmo.radius = origin, length, radius
end

local function pickGizmoAxis(screenX, screenY)
    if S.transformMode ~= 'move' or not S.gizmo.origin then return nil end
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
    if S.transformMode ~= 'rotate' or not S.gizmo.origin then return nil end
    local axes = {x = {x = 1, y = 0, z = 0}, y = {x = 0, y = 1, z = 0}, z = {x = 0, y = 0, z = 1}}
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

local function beginViewportInteraction(key, screenX, screenY)
    if tImGui.GetWantCaptureMouse() then return end
    if key == 0 then
        local ring = pickRotationRing(screenX, screenY)
        if ring then
            local entry = roleEntry(S.selectedRole)
            S.gizmo.drag = {
                mode = 'rotate', axis = ring.axis, origin = {x = S.gizmo.origin.x, y = S.gizmo.origin.y,
                    z = S.gizmo.origin.z}, startDirection = ring.direction, base = clonePose(entry.pose),
                lastAngle = 0, accumulatedAngle = 0,
                lastEuler = {rx = entry.pose.rx, ry = entry.pose.ry, rz = entry.pose.rz},
            }
            S.cameraDrag = nil
            return
        end
        local axisName = pickGizmoAxis(screenX, screenY)
        if axisName then
            local axis = ({x = {x = 1, y = 0, z = 0}, y = {x = 0, y = 1, z = 0},
                z = {x = 0, y = 0, z = 1}})[axisName]
            local parameter = rayAxisParameter(screenX, screenY, S.gizmo.origin, axis)
            if parameter then
                S.gizmo.drag = {mode = 'move', axis = axis,
                    origin = {x = S.gizmo.origin.x, y = S.gizmo.origin.y, z = S.gizmo.origin.z},
                    startParameter = parameter, base = clonePose(roleEntry(S.selectedRole).pose)}
                S.cameraDrag = nil
                return
            end
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
        if math.abs(offset.rotation.x) < 1e-8 and math.abs(offset.rotation.y) < 1e-8 and
                math.abs(offset.rotation.z) < 1e-8 then
            result.rx, result.ry, result.rz = source.rx, source.ry, source.rz
        else
            local targetRotation = multiplyRotation(sourceRotation,
                eulerRotation(offset.rotation.x, offset.rotation.y, offset.rotation.z))
            result.rx, result.ry, result.rz = matrixToEuler(targetRotation)
        end
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
        sourceSelectable = S.source.selectable ~= false,
        targetSelectable = S.target.selectable ~= false,
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
    if type(value.editor) == 'table' then
        local editor = value.editor
        result.editor.sourceKind = editor.sourceKind == 'mesh' and 'mesh' or 'placeholder'
        result.editor.targetKind = editor.targetKind == 'mesh' and 'mesh' or 'placeholder'
        result.editor.sourceMesh = type(editor.sourceMesh) == 'string' and editor.sourceMesh or ''
        result.editor.targetMesh = type(editor.targetMesh) == 'string' and editor.targetMesh or ''
        if type(editor.sourcePose) == 'table' then result.editor.sourcePose = clonePose(editor.sourcePose) end
        if type(editor.targetPose) == 'table' then result.editor.targetPose = clonePose(editor.targetPose) end
        result.editor.targetManualOverride = editor.targetManualOverride == true
        result.editor.sourceSelectable = editor.sourceSelectable ~= false
        result.editor.targetSelectable = editor.targetSelectable ~= false
    end
    if result.sourceType ~= 'root' then return nil end
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
    S.definition = definition
    S.source.pose = clonePose(definition.editor.sourcePose)
    S.target.pose = clonePose(definition.editor.targetPose)
    restoreRoleAsset('source', definition.editor.sourceKind, definition.editor.sourceMesh)
    restoreRoleAsset('target', definition.editor.targetKind, definition.editor.targetMesh)
    S.source.selectable = definition.editor.sourceSelectable ~= false
    S.target.selectable = definition.editor.targetSelectable ~= false
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
        tImGui.Text(string.format(L('selected_object'), roleName(S.selectedRole)))
        showPoseFields(S.selectedRole)
        if S.selectedRole == 'target' then tImGui.TextWrapped(L('target_pose_help'))
        else tImGui.TextWrapped(L('source_pose_help')) end
        tImGui.Separator()
        tImGui.Text(L('transform_mode'))
        local modeIndex = tImGui.RadioButton(L('move'), S.transformMode == 'move' and 1 or 2, 1)
        tImGui.SameLine()
        modeIndex = tImGui.RadioButton(L('rotate'), modeIndex, 2)
        local nextMode = modeIndex == 2 and 'rotate' or 'move'
        if nextMode ~= S.transformMode then
            S.transformMode = nextMode
            S.gizmo.drag = nil
        end
        tImGui.TextWrapped(S.transformMode == 'move' and L('move_gizmo_help') or L('rotate_gizmo_help'))
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
    updateGizmo()
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
    if S.source.object then S.source.object:destroy(); S.source.object = nil end
    if S.target.object then S.target.object:destroy(); S.target.object = nil end
    if S.relationLine then S.relationLine:destroy(); S.relationLine = nil end
    if S.xzPlane then S.xzPlane:destroy(); S.xzPlane = nil end
    for key, object in pairs(S.originLines) do
        if object then object:destroy(); S.originLines[key] = nil end
    end
end
