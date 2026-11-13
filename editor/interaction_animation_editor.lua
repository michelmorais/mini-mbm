--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
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
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Interaction Animation Editor

   Editor for synchronizing mesh instances and authoring relative interaction
   events. Exported Lua files contain interaction definitions consumed by the
   C++ interaction animation runtime.
]]--

tImGui = require 'ImGui'
tUtil = require 'editor_utils'
local Animation = require 'mesh_to_sprite_animation'

local actorSlots = {}

local function newActorState(baseX)
    return {
        active = false,
        path = '',
        base = {x = baseX or 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1, visible = true},
        selectable = true,
        previewBase = nil,
        bundle = nil,
        object = nil,
        clipKey = '',
        runtimeKey = nil,
        runtimeClip = nil,
        runtimeOverride = false,
        runtimeStartTime = 0,
        rotationIntent = nil,
        offset = {x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0},
        offsetScale = {x = 1, y = 1, z = 1},
    }
end

local S = {
    actors = {},
    actorOrder = {},
    anchorActor = nil,
    clockActor = nil,
    clockKey = '',
    time = 0,
    timelineDuration = 0.01,
    timelineDurationExplicit = false,
    continueClipsAfterTimeline = false,
    playing = false,
    events = {},
    selectedEvent = 0,
    projectPath = '',
    status = '',
    modified = false,
    lastAppliedTime = -1,
    camera = nil,
    cameraOrbit = {azimuth = 0.3, elevation = 0.3, distance = 800, fx = 80, fy = 50, fz = 0},
    selectedActor = nil,
    baseReference = nil,
    transformMode = 'move',
    gizmo = {axes = {}, rings = {}, origin = nil, length = nil, radius = nil, axesWorld = nil, drag = nil},
    eventHighlight = {actor = nil, remaining = 0, shape = nil, line = nil},
    originLines = {x = nil, y = nil, z = nil},
    showOrigins = true,
    xzPlane = nil,
    showXZPlane = true,
    xzPlaneSize = nil,
    xzPlaneX = nil,
    xzPlaneZ = nil,
    mouse = {left = false, right = false, x = 0, y = 0},
    cameraDrag = nil,
    windows = {
        actors = '',
        events = '',
        timeline = '',
        camera = '',
    },
}

local L

L = function(key)
    return tLang.L('iae_' .. key)
end

local function refreshWindowTitles()
    S.windows.actors = L('actors')
    S.windows.events = L('events')
    S.windows.timeline = L('timeline')
    S.windows.camera = L('camera')
end

local function setStatus(message, warning)
    S.status = tostring(message or '')
    if warning then
        tUtil.showMessageWarn(S.status, 6)
    else
        tUtil.showMessage(S.status, 6)
    end
end

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then
        setStatus(result[2], true)
        print('[interaction_animation_editor] ' .. S.status)
    end
    return table.unpack(result, 1, result.n)
end

local function cloneTable(value)
    if type(value) ~= 'table' then return value end
    local copy = {}
    for key, item in pairs(value) do copy[key] = cloneTable(item) end
    return copy
end

-- At time zero the editable pose is the project's base pose. At any later
-- time, manual edits are only a preview until the user captures them into an
-- event. This prevents an uncaptured edit from silently changing the origin.
local function displayBase(entry)
    return entry.previewBase or entry.base
end

local function editableBase(entry)
    if math.abs(S.time) <= 1e-6 then return entry.base end
    if not entry.previewBase then entry.previewBase = cloneTable(entry.base) end
    return entry.previewBase
end

local function directoryOf(path)
    return tostring(path or ''):gsub('\\', '/'):match('^(.*)/[^/]*$') or '.'
end

local function exportSuggestion()
    if not S.projectPath or S.projectPath == '' then return 'interaction.lua' end
    local filename = tUtil.getShortName(S.projectPath) or 'interaction'
    filename = filename:gsub('%.iae%.lua$', ''):gsub('%.lua$', '')
    return directoryOf(S.projectPath) .. '/' .. filename .. '-interaction.lua'
end

local function actor(id)
    local entry = S.actors[id]
    return entry and entry.active and entry or nil
end

local function actorSlotIndex(id)
    local index = tostring(id or ''):match('^actor(%d+)$')
    return index and tonumber(index) or nil
end

local function actorName(id, entry)
    if not entry then return '' end
    return string.format(L('actor_default'), actorSlotIndex(id) or 1)
end

local function rebuildActorOrder()
    S.actorOrder = {}
    for _, id in ipairs(actorSlots) do
        if S.actors[id].active then table.insert(S.actorOrder, id) end
    end
end

local function activeActorIds()
    return S.actorOrder
end

local function setAnchorActor(id)
    if not actor(id) then return end
    S.anchorActor = id
    S.gizmo.axesWorld = nil
end

local function setClockActor(id)
    if not actor(id) then return end
    S.clockActor = id
    S.clockKey = actor(id).clipKey or ''
end

local function buildBaseReference()
    local reference = {actors = {}}
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        reference.actors[id] = {
            base = cloneTable(entry.base),
            clipKey = entry.clipKey,
        }
    end
    return reference
end

local function refreshBaseReferenceAtZero()
    if math.abs(S.time) > 1e-6 or #S.actorOrder == 0 then return end
    S.baseReference = buildBaseReference()
end

local function addActor()
    local index = #actorSlots + 1
    local id = 'actor' .. index
    while S.actors[id] do
        index = index + 1
        id = 'actor' .. index
    end
    table.insert(actorSlots, id)
    S.actors[id] = newActorState(index == 1 and 180 or 0)
    S.actors[id].active = true
    rebuildActorOrder()
    S.baseReference = nil
    if not S.anchorActor then setAnchorActor(id) end
    if not S.clockActor then setClockActor(id) end
    S.selectedActor = id
    S.modified = true
    return id
end

local applyTransform

local function cameraPosition()
    local c = S.cameraOrbit
    return c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth),
        c.fy + c.distance * math.sin(c.elevation),
        c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
end

local function applyCamera()
    local x, y, z = cameraPosition()
    S.camera:setPos(x, y, z)
    S.camera:setFocus(S.cameraOrbit.fx, S.cameraOrbit.fy, S.cameraOrbit.fz)
end

local function resetCamera()
    S.cameraOrbit.azimuth = 0.3
    S.cameraOrbit.elevation = 0.3
    S.cameraOrbit.distance = 800
    S.cameraOrbit.fx = 80
    S.cameraOrbit.fy = 50
    S.cameraOrbit.fz = 0
    applyCamera()
end

local function dot(ax, ay, az, bx, by, bz)
    return ax * bx + ay * by + az * bz
end

local normalizedDirection

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

local function identityRotation()
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}
end

local function isIdentityRotation(matrix)
    return math.abs(matrix[1][1] - 1) < 0.000001 and
        math.abs(matrix[2][2] - 1) < 0.000001 and
        math.abs(matrix[3][3] - 1) < 0.000001 and
        math.abs(matrix[1][2]) < 0.000001 and math.abs(matrix[1][3]) < 0.000001 and
        math.abs(matrix[2][1]) < 0.000001 and math.abs(matrix[2][3]) < 0.000001 and
        math.abs(matrix[3][1]) < 0.000001 and math.abs(matrix[3][2]) < 0.000001
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
    return multiplyRotation(zRotation, multiplyRotation(yRotation, multiplyRotation(xRotation, identityRotation())))
end

local function rotationDirection(matrix, row)
    return normalizedDirection(matrix[row][1], matrix[row][2], matrix[row][3])
end

local function interactionAxes()
    local entry = actor(S.anchorActor)
    if not entry then
        return {
            x = {x = 1, y = 0, z = 0},
            y = {x = 0, y = 1, z = 0},
            z = {x = 0, y = 0, z = 1},
        }
    end
    local matrix = eulerRotation(math.rad(entry.base.rx), math.rad(entry.base.ry), math.rad(entry.base.rz))
    return {
        x = rotationDirection(matrix, 1),
        y = rotationDirection(matrix, 2),
        z = rotationDirection(matrix, 3),
    }
end

local function anchorEulerRotation(rx, ry, rz)
    local axes = interactionAxes()
    local xRotation = axisRotation(axes.x, math.rad(rx or 0))
    local yRotation = axisRotation(axes.y, math.rad(ry or 0))
    local zRotation = axisRotation(axes.z, math.rad(rz or 0))
    return multiplyRotation(zRotation, multiplyRotation(yRotation, multiplyRotation(xRotation, identityRotation())))
end

local function matrixToEulerDegrees(matrix)
    local y = math.asin(math.max(-1, math.min(1, matrix[3][1])))
    local x, z
    if math.abs(matrix[3][1]) > 0.999999 then
        x = 0
        z = math.atan(matrix[1][2], matrix[2][2])
    else
        x = math.atan(-matrix[3][2], matrix[3][3])
        z = math.atan(-matrix[2][1], matrix[1][1])
    end
    return math.deg(x), math.deg(y), math.deg(z)
end

local function unwrapAngleDegrees(angle, reference)
    if reference == nil then return angle end
    local turns = math.floor((reference - angle) / 360 + 0.5)
    return angle + turns * 360
end

local function unwrapEulerDegrees(rx, ry, rz, reference)
    return unwrapAngleDegrees(rx, reference.rx),
        unwrapAngleDegrees(ry, reference.ry),
        unwrapAngleDegrees(rz, reference.rz)
end

-- Euler decomposition is only a shortest representation of a rotation. Near
-- the Y branch change, the visible base pose can legitimately report a Z
-- value on the other side of the origin even though the user dragged the Z
-- ring in the positive direction. Keep the matrix's signed shortest result
-- in that case; only unwrap when the pose explicitly indicates a turn beyond
-- 180 degrees. This preserves deliberate long rotations without converting
-- a normal +150 degree drag into a negative rotation.
local function capturedRotationAngle(raw, reference)
    if reference == nil or math.abs(reference) <= 180 then return raw end
    return unwrapAngleDegrees(raw, reference)
end

local function rotationToAnchorEuler(matrix)
    local axes = interactionAxes()
    local basis = {
        {axes.x.x, axes.y.x, axes.z.x},
        {axes.x.y, axes.y.y, axes.z.y},
        {axes.x.z, axes.y.z, axes.z.z},
    }
    local localRotation = multiplyRotation(transposeRotation(basis), multiplyRotation(matrix, basis))
    return matrixToEulerDegrees(localRotation)
end

local function rayAxisParameter(sx, sy, origin, axis)
    local ox, oy, oz, dx, dy, dz = mbm.getPickRay(sx, sy)
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

local function raySphereDistance(sx, sy, center, radius)
    local ox, oy, oz, dx, dy, dz = mbm.getPickRay(sx, sy)
    local lx, ly, lz = ox - center.x, oy - center.y, oz - center.z
    local b = dot(lx, ly, lz, dx, dy, dz)
    local c = dot(lx, ly, lz, lx, ly, lz) - radius * radius
    local discriminant = b * b - c
    if discriminant < 0 then return nil end
    local distance = -b - math.sqrt(discriminant)
    return distance >= 0 and distance or -b + math.sqrt(discriminant)
end

local function rayAABBDistance(sx, sy, center, width, height, depth)
    local ox, oy, oz, dx, dy, dz = mbm.getPickRay(sx, sy)
    local minimum = {center.x - width * 0.5, center.y - height * 0.5, center.z - depth * 0.5}
    local maximum = {center.x + width * 0.5, center.y + height * 0.5, center.z + depth * 0.5}
    local origin = {ox, oy, oz}
    local direction = {dx, dy, dz}
    local nearDistance, farDistance = 0, math.huge
    for axis = 1, 3 do
        if math.abs(direction[axis]) < 1e-8 then
            if origin[axis] < minimum[axis] or origin[axis] > maximum[axis] then return nil end
        else
            local first = (minimum[axis] - origin[axis]) / direction[axis]
            local second = (maximum[axis] - origin[axis]) / direction[axis]
            if first > second then first, second = second, first end
            nearDistance = math.max(nearDistance, first)
            farDistance = math.min(farDistance, second)
            if nearDistance > farDistance then return nil end
        end
    end
    return farDistance >= 0 and nearDistance or nil
end

local function raySegmentDistance(sx, sy, a, b, radius)
    local ox, oy, oz, dx, dy, dz = mbm.getPickRay(sx, sy)
    local ax, ay, az = b.x - a.x, b.y - a.y, b.z - a.z
    local rx, ry, rz = ox - a.x, oy - a.y, oz - a.z
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
        rayT = (ab * br - bb * ar) / denominator
        segmentT = (aa * br - ab * ar) / denominator
        segmentT = math.max(0, math.min(1, segmentT))
        rayT = math.max(0, rayT)
    end
    local px, py, pz = ox + dx * rayT, oy + dy * rayT, oz + dz * rayT
    local qx, qy, qz = a.x + ax * segmentT, a.y + ay * segmentT, a.z + az * segmentT
    local ex, ey, ez = px - qx, py - qy, pz - qz
    return math.sqrt(ex * ex + ey * ey + ez * ez)
end

local function actorPosition(entry, refreshBounds)
    if entry.object then
        local ok, x, y, z = pcall(function() return entry.object:getAABBCenter(refreshBounds == true) end)
        if ok and x and y and z then return {x = x, y = y, z = z} end
    end
    return {
        x = displayBase(entry).x + entry.offset.x,
        y = displayBase(entry).y + entry.offset.y,
        z = displayBase(entry).z + entry.offset.z,
    }
end

local function actorMeshExtent(entry, refreshBounds)
    if not entry.object then return 40 / 0.45 end
    -- getAABB(true) is the current world-space size and includes the actor's
    -- scale and rotation. getSize(true) is only a geometry-size fallback.
    local ok, width, height, depth = pcall(function()
        return entry.object:getAABB(refreshBounds == true)
    end)
    local extent = ok and math.max(width or 0, height or 0, depth or 0) or 0
    if extent <= 0 then
        ok, width, height, depth = pcall(function() return entry.object:getSize(true) end)
        if ok then extent = math.max(width or 0, height or 0, depth or 0) end
    end
    return math.max(extent, 40 / 0.45)
end

local function actorPickRadius(entry, refreshBounds)
    return actorMeshExtent(entry, refreshBounds) * 0.45
end

local function actorPickDistance(sx, sy, entry)
    local center = actorPosition(entry, true)
    local ok, width, height, depth = pcall(function() return entry.object:getAABB(true) end)
    if ok and width and height and depth and width > 0 and height > 0 and depth > 0 then
        return rayAABBDistance(sx, sy, center, width, height, depth)
    end
    return raySphereDistance(sx, sy, center, actorPickRadius(entry, true))
end

local function eventHighlightVertices()
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

local function hideEventHighlight()
    local highlight = S.eventHighlight
    highlight.actor = nil
    highlight.remaining = 0
    if highlight.shape then highlight.shape.visible = false end
    if highlight.line then highlight.line.visible = false end
end

local function ensureEventHighlight()
    local highlight = S.eventHighlight
    if highlight.shape and highlight.line then return true end
    local object = shape:new('3d', 0, 0, 0)
    if not object:create(eventHighlightVertices(), nil, 'interaction_animation_event_highlight') then
        object:destroy()
        return false
    end
    object:setColor(1, 0.65, 0.05, 0.12)
    object.alwaysOnTop = true
    object.alwaysOnTopPriority = 5
    object.alwaysRender = true
    object.visible = false
    local outline = line:new('3d', 0, 0, 0)
    outline:drawBounding(object, false)
    outline:setColor(1, 0.75, 0.1)
    outline.alwaysOnTop = true
    outline.alwaysOnTopPriority = 6
    outline.alwaysRender = true
    outline.visible = false
    highlight.shape = object
    highlight.line = outline
    return true
end

local function showEventHighlight(event)
    local id = event and event.action ~= 'notify' and event.target or nil
    local entry = id and actor(id) or nil
    if not entry or not entry.object or not ensureEventHighlight() then
        hideEventHighlight()
        return
    end
    S.eventHighlight.actor = id
    S.eventHighlight.remaining = 3
end

local function updateEventHighlight(delta)
    local highlight = S.eventHighlight
    if not highlight.actor then return end
    highlight.remaining = highlight.remaining - math.max(delta or 0, 0)
    local entry = actor(highlight.actor)
    if highlight.remaining <= 0 or not entry or not entry.object then
        hideEventHighlight()
        return
    end
    local ok, width, height, depth = pcall(function() return entry.object:getAABB(true) end)
    if not ok or not width or not height or not depth then
        ok, width, height, depth = pcall(function() return entry.object:getSize(true) end)
    end
    if not ok then
        hideEventHighlight()
        return
    end
    width, height, depth = math.max(width or 0.01, 0.01), math.max(height or 0.01, 0.01),
        math.max(depth or 0.01, 0.01)
    local center = actorPosition(entry)
    local scale = 1.10
    highlight.shape:setPos(center.x, center.y, center.z)
    highlight.shape:setScale(width * scale, height * scale, depth * scale)
    highlight.shape.visible = true
    highlight.line:setPos(center.x, center.y, center.z)
    highlight.line:setScale(width * scale, height * scale, depth * scale)
    highlight.line.visible = true
end

local function hideGizmo()
    for _, object in pairs(S.gizmo.axes) do object.visible = false end
    for _, object in pairs(S.gizmo.rings) do object.visible = false end
    S.gizmo.origin = nil
    S.gizmo.length = nil
    S.gizmo.radius = nil
    S.gizmo.axesWorld = nil
end

normalizedDirection = function(x, y, z)
    local length = math.sqrt(x * x + y * y + z * z)
    if length <= 1e-8 then return nil end
    return {x = x / length, y = y / length, z = z / length}
end

local function rayPlaneHit(sx, sy, point, normal)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, sx, sy)
    if not ok then return nil end
    local denominator = normal.x * dx + normal.y * dy + normal.z * dz
    if math.abs(denominator) < 1e-6 then return nil end
    local distance = ((point.x - ox) * normal.x + (point.y - oy) * normal.y +
        (point.z - oz) * normal.z) / denominator
    if distance < 0 then return nil end
    return ox + dx * distance, oy + dy * distance, oz + dz * distance
end

local function updateGizmo()
    local entry = S.selectedActor and actor(S.selectedActor) or nil
    if not entry or not entry.object then
        hideGizmo()
        return
    end
    local origin = actorPosition(entry)
    local length = math.max(actorMeshExtent(entry) * 0.8, 32)
    local previousOrigin, previousLength = S.gizmo.origin, S.gizmo.length
    local previousAxes = S.gizmo.axesWorld
    local originChanged = not previousOrigin or previousOrigin.x ~= origin.x or
        previousOrigin.y ~= origin.y or previousOrigin.z ~= origin.z
    local lengthChanged = not previousLength or math.abs(previousLength - length) > 1e-6
    local axesWorld = interactionAxes()
    local axesChanged = not previousAxes
    if previousAxes then
        for _, name in ipairs({'x', 'y', 'z'}) do
            local oldAxis, newAxis = previousAxes[name], axesWorld[name]
            if math.abs(oldAxis.x - newAxis.x) > 1e-6 or math.abs(oldAxis.y - newAxis.y) > 1e-6 or
                    math.abs(oldAxis.z - newAxis.z) > 1e-6 then
                axesChanged = true
                break
            end
        end
    end
    local definitions = {
        x = {axis = axesWorld.x, color = {1, 0.15, 0.15, 1}},
        y = {axis = axesWorld.y, color = {0.15, 1, 0.2, 1}},
        z = {axis = axesWorld.z, color = {0.2, 0.45, 1, 1}},
    }
    for name, definition in pairs(definitions) do
        local object = S.gizmo.axes[name]
        local axis = definition.axis
        local points = {0, 0, 0, axis.x * length, axis.y * length, axis.z * length}
        if object then
            if lengthChanged or axesChanged then object:set(points, 1) end
        else
            object = line:new('3d', 0, 0, 0)
            object:add(points)
            S.gizmo.axes[name] = object
        end
        object.alwaysOnTop = true
        object.alwaysOnTopPriority = 10
        object.alwaysRender = true
        if originChanged then object:setPos(origin.x, origin.y, origin.z) end
        object:setColor(table.unpack(definition.color))
        object.visible = S.transformMode == 'move'
    end
    local radius = math.max(actorMeshExtent(entry) * 0.55, 20)
    local previousRadius = S.gizmo.radius
    local radiusChanged = not previousRadius or math.abs(previousRadius - radius) > 1e-6 or axesChanged
    local ringDefinitions = {
        x = {u = axesWorld.y, v = axesWorld.z, color = {1, 0.15, 0.15, 1}},
        y = {u = axesWorld.z, v = axesWorld.x, color = {0.15, 1, 0.2, 1}},
        z = {u = axesWorld.x, v = axesWorld.y, color = {0.2, 0.45, 1, 1}},
    }
    for name, definition in pairs(ringDefinitions) do
        local object = S.gizmo.rings[name]
        local created = false
        if not object then
            object = line:new('3d', 0, 0, 0)
            S.gizmo.rings[name] = object
            radiusChanged = true
            created = true
        end
        if created or radiusChanged then
            local points = {}
            for item = 0, 64 do
                local angle = item / 64 * math.pi * 2
                points[#points + 1] = (definition.u.x * math.cos(angle) + definition.v.x * math.sin(angle)) * radius
                points[#points + 1] = (definition.u.y * math.cos(angle) + definition.v.y * math.sin(angle)) * radius
                points[#points + 1] = (definition.u.z * math.cos(angle) + definition.v.z * math.sin(angle)) * radius
            end
            if created then object:add(points) else object:set(points, 1) end
        end
        object.alwaysOnTop = true
        object.alwaysOnTopPriority = 20
        object.alwaysRender = true
        if originChanged then object:setPos(origin.x, origin.y, origin.z) end
        object:setColor(table.unpack(definition.color))
        object.visible = S.transformMode == 'rotate'
    end
    S.gizmo.origin = origin
    S.gizmo.length = length
    S.gizmo.radius = radius
    S.gizmo.axesWorld = axesWorld
end

local function pickActor(sx, sy)
    local bestId, bestDistance
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        if entry.object and entry.selectable ~= false then
            local distance = actorPickDistance(sx, sy, entry)
            if distance and (not bestDistance or distance < bestDistance) then
                bestId, bestDistance = id, distance
            end
        end
    end
    return bestId
end

local function pickGizmoAxis(sx, sy)
    if S.transformMode ~= 'move' or not S.gizmo.origin or not S.gizmo.length or not S.gizmo.axesWorld then return nil end
    local bestName, bestDistance
    local threshold = math.max(S.gizmo.length * 0.12, 12)
    for name, axis in pairs(S.gizmo.axesWorld) do
        local endpoint = {
            x = S.gizmo.origin.x + axis.x * S.gizmo.length,
            y = S.gizmo.origin.y + axis.y * S.gizmo.length,
            z = S.gizmo.origin.z + axis.z * S.gizmo.length,
        }
        local distance = raySegmentDistance(sx, sy, S.gizmo.origin, endpoint, threshold)
        if distance and distance <= threshold and (not bestDistance or distance < bestDistance) then
            bestName, bestDistance = name, distance
        end
    end
    return bestName
end

local function pickRotationRing(sx, sy)
    if S.transformMode ~= 'rotate' or not S.gizmo.origin or not S.gizmo.radius or not S.gizmo.axesWorld then
        return nil
    end
    local tolerance = math.max(S.gizmo.radius * 0.12, 12)
    local best, bestError
    for name, axis in pairs(S.gizmo.axesWorld) do
        local wx, wy, wz = rayPlaneHit(sx, sy, S.gizmo.origin, axis)
        if wx then
            local vx, vy, vz = wx - S.gizmo.origin.x, wy - S.gizmo.origin.y, wz - S.gizmo.origin.z
            local radial = math.sqrt(vx * vx + vy * vy + vz * vz)
            local error = math.abs(radial - S.gizmo.radius)
            if error <= tolerance and (not bestError or error < bestError) then
                local direction = normalizedDirection(vx, vy, vz)
                if direction then
                    best = {name = name, axis = axis, direction = direction}
                    bestError = error
                end
            end
        end
    end
    return best
end

local function setOriginsVisible(visible)
    S.showOrigins = visible == true
    for _, object in pairs(S.originLines) do
        if object then object.visible = S.showOrigins end
    end
end

local function beginViewportInteraction(key, x, y)
    if tImGui.GetWantCaptureMouse() then return end
    if key == 0 then
        local picked = pickActor(x, y)
        if picked and picked ~= S.selectedActor then
            S.selectedActor = picked
            S.gizmo.drag = nil
            updateGizmo()
            return
        end
        local entry = S.selectedActor and actor(S.selectedActor) or nil
        local ring = S.selectedActor and pickRotationRing(x, y) or nil
        if ring and entry and S.gizmo.origin then
            if S.playing then S.playing = false end
            S.gizmo.drag = {
                mode = 'rotate',
                actor = S.selectedActor,
                axisName = ring.name,
                axis = cloneTable(ring.axis),
                startDirection = cloneTable(ring.direction),
                origin = cloneTable(S.gizmo.origin),
                base = cloneTable(displayBase(entry)),
                lastAngle = 0,
                accumulatedAngle = 0,
                lastEuler = {
                    rx = displayBase(entry).rx,
                    ry = displayBase(entry).ry,
                    rz = displayBase(entry).rz,
                },
                startX = x,
                startY = y,
                moved = false,
            }
            return
        end
        local axisName = S.selectedActor and pickGizmoAxis(x, y) or nil
        if axisName and entry and S.gizmo.origin then
            local axis = S.gizmo.axesWorld[axisName]
            local parameter = rayAxisParameter(x, y, S.gizmo.origin, axis)
            if parameter then
                if S.playing then
                    S.playing = false
                end
                S.gizmo.drag = {
                    mode = 'move',
                    actor = S.selectedActor,
                    axisName = axisName,
                    axis = cloneTable(axis),
                    startParameter = parameter,
                    origin = cloneTable(S.gizmo.origin),
                    base = cloneTable(displayBase(entry)),
                    startX = x,
                    startY = y,
                    moved = false,
                }
                return
            end
        end
        if picked then
            S.selectedActor = picked
            updateGizmo()
            return
        end
        S.selectedActor = nil
        hideGizmo()
        S.cameraDrag = {kind = 'orbit', startX = x, startY = y}
    elseif key == 1 then
        S.cameraDrag = {kind = 'pan', startX = x, startY = y}
    end
end

local function updateViewportInteraction(x, y)
    local drag = S.gizmo.drag
    if drag then
        local entry = actor(drag.actor)
        if not entry then return end
        if drag.mode == 'move' then
            local parameter = rayAxisParameter(x, y, drag.origin, drag.axis)
            if parameter then
                local amount = parameter - drag.startParameter
                local base = editableBase(entry)
                base.x = drag.base.x + drag.axis.x * amount
                base.y = drag.base.y + drag.axis.y * amount
                base.z = drag.base.z + drag.axis.z * amount
                entry.rotationIntent = nil
                applyTransform(entry)
                drag.moved = true
                S.modified = true
            end
        else
            local wx, wy, wz = rayPlaneHit(x, y, drag.origin, drag.axis)
            if wx then
                local current = normalizedDirection(wx - drag.origin.x, wy - drag.origin.y, wz - drag.origin.z)
                if current then
                    local start = drag.startDirection
                    local crossX = start.y * current.z - start.z * current.y
                    local crossY = start.z * current.x - start.x * current.z
                    local crossZ = start.x * current.y - start.y * current.x
                    local sinAngle = drag.axis.x * crossX + drag.axis.y * crossY + drag.axis.z * crossZ
                    local cosAngle = math.max(-1, math.min(1,
                        start.x * current.x + start.y * current.y + start.z * current.z))
                    local angle = math.atan(sinAngle, cosAngle)
                    local deltaAngle = angle - drag.lastAngle
                    if deltaAngle > math.pi then
                        deltaAngle = deltaAngle - 2 * math.pi
                    elseif deltaAngle < -math.pi then
                        deltaAngle = deltaAngle + 2 * math.pi
                    end
                    drag.accumulatedAngle = drag.accumulatedAngle + deltaAngle
                    drag.lastAngle = angle
                    local baseRotation = eulerRotation(math.rad(drag.base.rx), math.rad(drag.base.ry),
                        math.rad(drag.base.rz))
                    local rotated = multiplyRotation(baseRotation,
                        axisRotation(drag.axis, drag.accumulatedAngle))
                    local base = editableBase(entry)
                    local rx, ry, rz = matrixToEulerDegrees(rotated)
                    base.rx, base.ry, base.rz = unwrapEulerDegrees(rx, ry, rz, drag.lastEuler)
                    drag.lastEuler = {rx = base.rx, ry = base.ry, rz = base.rz}
                    entry.rotationIntent = {
                        axisName = drag.axisName,
                        angle = math.deg(drag.accumulatedAngle),
                    }
                    applyTransform(entry)
                    drag.moved = true
                    S.modified = true
                end
            end
        end
        updateGizmo()
        return
    end
    local cameraDrag = S.cameraDrag
    if not cameraDrag or tImGui.GetWantCaptureMouse() then return end
    local dx, dy = x - cameraDrag.startX, y - cameraDrag.startY
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
    cameraDrag.startX, cameraDrag.startY = x, y
    applyCamera()
end

local function endViewportInteraction(key)
    if key == 0 then S.gizmo.drag = nil end
    if key == 0 or key == 1 then S.cameraDrag = nil end
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
        -0.5, 0, -0.5,  0.5, 0, -0.5,  0.5, 0,  0.5,
        -0.5, 0, -0.5,  0.5, 0,  0.5, -0.5, 0,  0.5,
         0.5, 0, -0.5, -0.5, 0, -0.5,  0.5, 0,  0.5,
         0.5, 0,  0.5, -0.5, 0, -0.5, -0.5, 0,  0.5,
    }
    local object = shape:new('3d', 0, 0, 0)
    if not object:create(points, nil, 'interaction_animation_xz_plane') then
        object:destroy()
        return
    end
    object:setColor(0.24, 0.30, 0.38, 0.8)
    object.alwaysRender = true
    object.visible = S.showXZPlane
    S.xzPlane = object
end

local function updateXZPlane()
    local object = S.xzPlane
    if not object then return end
    local camera = S.cameraOrbit
    local size = math.max(400, camera.distance * 2.4)
    if not S.xzPlaneSize or math.abs(S.xzPlaneSize - size) > 0.001 then
        object:setScale(size, 1, size)
        S.xzPlaneSize = size
    end
    if S.xzPlaneX ~= camera.fx or S.xzPlaneZ ~= camera.fz then
        object:setPos(camera.fx, 0, camera.fz)
        S.xzPlaneX, S.xzPlaneZ = camera.fx, camera.fz
    end
    object.visible = S.showXZPlane
end

local function clipList(entry)
    local list = {{key = '', label = L('no_animation'), kind = 'none', name = '', duration = 0}}
    if not entry or not entry.bundle then return list end
    for _, clip in ipairs(entry.bundle.static or {}) do
        table.insert(list, {
            key = 'frame:' .. clip.name,
            label = string.format('%s - %s (%.3fs)', L('static'), clip.name, clip.duration or 0),
            kind = 'frame', name = clip.name, duration = clip.duration or 0,
            loop = clip.mode == 2 or clip.mode == 4 or clip.mode == 6,
            first = clip.first, last = clip.last, interval = clip.interval, mode = clip.mode,
        })
    end
    for _, clip in ipairs(entry.bundle.skeletal or {}) do
        table.insert(list, {
            key = 'skeletal:' .. clip.name,
            label = string.format('%s - %s (%.3fs)', L('skeletal'), clip.name, clip.duration or 0),
            kind = 'skeletal', name = clip.name, duration = clip.duration or 0,
            loop = clip.loop == true,
        })
    end
    for _, clip in ipairs(entry.bundle.articulated or {}) do
        table.insert(list, {
            key = 'articulated:' .. clip.name,
            label = string.format('%s - %s (%.3fs)', L('articulated'), clip.name, clip.duration or 0),
            kind = 'articulated', name = clip.name, duration = clip.duration or 0,
            loop = clip.loop == true, priority = clip.priority or 0,
        })
    end
    return list
end

local function findClip(entry, key)
    for _, clip in ipairs(clipList(entry)) do
        if clip.key == (key or '') then return clip end
    end
    return clipList(entry)[1]
end

applyTransform = function(entry)
    if not entry.object then return end
    local base, offset = displayBase(entry), entry.offset
    entry.object:setPos(base.x + offset.x, base.y + offset.y, base.z + offset.z)
    local offsetRotation = entry.offsetRotation or identityRotation()
    if isIdentityRotation(offsetRotation) then
        entry.object:setAngle(math.rad(base.rx), math.rad(base.ry), math.rad(base.rz))
    else
        local baseRotation = eulerRotation(math.rad(base.rx), math.rad(base.ry), math.rad(base.rz))
        local rotation = multiplyRotation(baseRotation, offsetRotation)
        local rx, ry, rz = matrixToEulerDegrees(rotation)
        entry.object:setAngle(math.rad(rx), math.rad(ry), math.rad(rz))
    end
    local scale = entry.offsetScale or {x = 1, y = 1, z = 1}
    entry.object:setScale(base.sx * scale.x, base.sy * scale.y, base.sz * scale.z)
    refreshBaseReferenceAtZero()
end

local function stopEntry(entry)
    if not entry.object then return end
    if entry.bundle then
        Animation.stop(entry.bundle)
    end
    entry.runtimeKey = nil
    entry.runtimeClip = nil
    entry.runtimeOverride = false
    entry.runtimeStartTime = 0
end

local function seekClip(entry, clip, time, playing)
    if not entry.object or not clip or clip.kind == 'none' then
        stopEntry(entry)
        return
    end
    if entry.runtimeKey ~= clip.key then
        stopEntry(entry)
        if clip.kind == 'frame' then
            entry.object:setAnim(clip.name)
        elseif clip.kind == 'skeletal' then
            assert(entry.object:playSkeletalAnimation(clip.name))
        else
            assert(entry.object:playArticulatedAnimation(clip.name, clip.priority or 0, 0, 1))
        end
        entry.runtimeKey = clip.key
        entry.runtimeClip = clip
    end
    local targetTime = math.max(0, time or 0)
    if clip.kind == 'frame' and clip.loop and (clip.duration or 0) > 0 then
        -- Looping static modes must wrap their sampled time. Clamping them to
        -- the duration would keep the preview on one frame after the first
        -- cycle, even though the asset metadata says the clip loops.
        targetTime = targetTime % clip.duration
    else
        targetTime = math.min(targetTime, clip.duration or 0)
    end
    if clip.kind == 'frame' then
        local frame = Animation.staticFrame(clip, targetTime)
        entry.object:setIndexFrame(frame)
    elseif clip.kind == 'skeletal' then
        assert(entry.object:seekSkeletalAnimation(targetTime))
        if not playing then entry.object:pauseSkeletalAnimation() end
    else
        assert(entry.object:seekArticulatedAnimation(clip.name, targetTime))
        if playing then
            -- Selecting an event, changing a field, and loading a project all
            -- rebuild the pose through seekAll(..., false). That intentionally
            -- pauses the clip. When playback resumes, make the state explicit
            -- instead of relying only on playArticulatedAnimation() from the
            -- key-change branch above.
            assert(entry.object:resumeArticulatedAnimation(clip.name))
        else
            entry.object:pauseArticulatedAnimation(clip.name)
        end
    end
end

local function clockClip()
    local entry = actor(S.clockActor)
    if not entry then return clipList(nil)[1] end
    return findClip(entry, S.clockKey)
end

local function clockDuration()
    local clip = clockClip()
    return math.max(0.01, clip.duration or 0)
end

local function selectedClip(id)
    local entry = actor(id)
    if not entry then return clipList(nil)[1] end
    local key = entry.clipKey
    if id == S.clockActor and S.clockKey ~= '' then key = S.clockKey end
    return findClip(entry, key)
end

local function refreshDefaultTimelineDuration()
    if S.timelineDurationExplicit then return end
    local maximum = 0
    for _, id in ipairs(S.actorOrder) do
        local clip = selectedClip(id)
        maximum = math.max(maximum, clip.duration or 0)
    end
    S.timelineDuration = math.max(0.01, maximum)
end

local function projectDuration()
    return math.max(0.01, S.timelineDuration or 0)
end

local function maximumEventTime()
    local maximum = 0
    for _, event in ipairs(S.events) do
        maximum = math.max(maximum, tonumber(event.time) or 0)
    end
    return maximum
end

local function transitionBounds(event, maximumTime)
    -- Transform transitions belong to the project timeline. A clip may have
    -- ended already, or may continue beyond it, without limiting this event.
    return 0, maximumTime or projectDuration()
end

local function clamp(value, minimum, maximum)
    return math.max(minimum, math.min(maximum, value))
end

local function transitionRange(event, maximumTime)
    local lower, upper = transitionBounds(event, maximumTime)
    local finish = tonumber(event.transitionEnd)
    if finish == nil then finish = tonumber(event.time) or 0 end
    local start = tonumber(event.transitionStart)
    if start == nil then
        start = finish - math.max(tonumber(event.duration) or 0.2, 0.0001)
    end
    start = clamp(start, lower, upper)
    finish = clamp(finish, start, upper)
    return start, finish
end

local function normalizeTransition(event, maximumTime)
    if event.transition ~= 'smooth' then return end
    local start, finish = transitionRange(event, maximumTime)
    event.transitionStart = start
    event.transitionEnd = finish
    event.time = finish
    event.duration = nil
end

local function normalizeEventDefaults(event)
    if type(event) ~= 'table' or event.action ~= 'transform' then return end
    if event.x == nil then event.x = 0 end
    if event.y == nil then event.y = 0 end
    if event.z == nil then event.z = 0 end
    if event.rx == nil then event.rx = 0 end
    if event.ry == nil then event.ry = 0 end
    if event.rz == nil then event.rz = 0 end
    if event.sx == nil then event.sx = 1 end
    if event.sy == nil then event.sy = 1 end
    if event.sz == nil then event.sz = 1 end
    if event.transition == nil then event.transition = 'instant' end
end

local function transformProgress(event, time)
    if event.transition ~= 'smooth' then return 1 end
    local transitionStart, transitionEnd = transitionRange(event, projectDuration())
    if time < transitionStart then return 0 end
    if transitionEnd <= transitionStart then return time >= transitionEnd and 1 or 0 end
    local progress = clamp((time - transitionStart) / (transitionEnd - transitionStart), 0, 1)
    return progress * progress * (3 - 2 * progress)
end

local function applyEvent(event, progress)
    local entry = actor(event.target)
    if not entry then return end
    if event.action == 'transform' then
        progress = progress == nil and 1 or progress
        if progress <= 0 then return end
        local axes = interactionAxes()
        local x = (event.x or 0) * progress
        local y = (event.y or 0) * progress
        local z = (event.z or 0) * progress
        entry.offset.x = entry.offset.x + axes.x.x * x + axes.y.x * y + axes.z.x * z
        entry.offset.y = entry.offset.y + axes.x.y * x + axes.y.y * y + axes.z.y * z
        entry.offset.z = entry.offset.z + axes.x.z * x + axes.y.z * y + axes.z.z * z
        local deltaRotation = anchorEulerRotation(
            (event.rx or 0) * progress,
            (event.ry or 0) * progress,
            (event.rz or 0) * progress)
        entry.offsetRotation = multiplyRotation(entry.offsetRotation or identityRotation(), deltaRotation)
        local scaleX = 1 + ((event.sx or 1) - 1) * progress
        local scaleY = 1 + ((event.sy or 1) - 1) * progress
        local scaleZ = 1 + ((event.sz or 1) - 1) * progress
        local scale = entry.offsetScale or {x = 1, y = 1, z = 1}
        scale.x, scale.y, scale.z = scale.x * scaleX, scale.y * scaleY, scale.z * scaleZ
        entry.offsetScale = scale
        applyTransform(entry)
    elseif event.action == 'play' then
        local clip = findClip(entry, event.clipKey)
        if clip.kind ~= 'none' then
            entry.runtimeStartTime = event.time or 0
            seekClip(entry, clip, math.max(0, S.time - entry.runtimeStartTime), S.playing)
            -- seekClip may switch from the actor's base clip and call
            -- stopEntry(), which clears runtimeOverride. Restore the event
            -- override after the switch so the next frame keeps this clip
            -- instead of returning to the base animation.
            entry.runtimeOverride = true
            entry.runtimeStartTime = event.time or 0
        end
    elseif event.action == 'visibility' then
        entry.object.visible = event.visible ~= false
    end
end

local function resetOffsets()
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        entry.offset = {x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0}
        entry.offsetRotation = identityRotation()
        entry.offsetScale = {x = 1, y = 1, z = 1}
        applyTransform(entry)
    end
end

local function seekAll(time, keepPlaying)
    S.time = math.max(0, math.min(time or 0, projectDuration()))
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        if entry then
            entry.previewBase = nil
            entry.rotationIntent = nil
        end
    end
    resetOffsets()
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        stopEntry(entry)
        if entry.object then entry.object.visible = displayBase(entry).visible ~= false end
        local clip = findClip(entry, entry.clipKey)
        seekClip(entry, clip, S.time, keepPlaying == true)
        applyTransform(entry)
    end
    for _, event in ipairs(S.events) do
        if event.action == 'transform' then
            applyEvent(event, transformProgress(event, S.time))
        elseif (event.time or 0) <= S.time then
            applyEvent(event)
        end
    end
    S.lastAppliedTime = S.time
end

local function sortEvents()
    local selected = S.events[S.selectedEvent]
    local indexed = {}
    for index, event in ipairs(S.events) do
        indexed[index] = {event = event, index = index}
    end
    table.sort(indexed, function(left, right)
        local leftTime = left.event.time or 0
        local rightTime = right.event.time or 0
        if leftTime == rightTime then return left.index < right.index end
        return leftTime < rightTime
    end)
    local selectedIndex = 0
    for index, item in ipairs(indexed) do
        S.events[index] = item.event
        if item.event == selected then selectedIndex = index end
    end
    S.selectedEvent = selectedIndex > 0 and selectedIndex or math.min(S.selectedEvent, #S.events)
end

local function setTransitionEndpoint(event, endpoint)
    local minimum, maximum = transitionBounds(event, projectDuration())
    local start, finish = transitionRange(event, projectDuration())
    local current = clamp(S.time, minimum, maximum)
    if endpoint == 'start' then
        start = current
        if finish < start then finish = start end
    else
        finish = current
        if finish < start then start = finish end
    end
    event.transitionStart = start
    event.transitionEnd = finish
    event.time = finish
    event.duration = nil
    sortEvents()
    S.modified = true
    seekAll(S.time, false)
end

local function setEventTimeToTimeline(event)
    event.time = clamp(S.time, 0, projectDuration())
    sortEvents()
    S.modified = true
    seekAll(S.time, false)
end

local function setTimelineDuration(value)
    local requested = tonumber(value) or S.timelineDuration
    local minimum = math.max(0.01, maximumEventTime())
    S.timelineDuration = math.max(minimum, requested)
    S.timelineDurationExplicit = true
end

local function loadActor(id, path)
    if not path or path == '' then return false end
    local entry = actor(id)
    if not entry then return false end
    local ok, bundle = dpCall(function()
        mbm.addPath(directoryOf(path))
        return Animation.load(path, 'auto')
    end)
    if not ok or not bundle then return false end
    if entry.object then entry.object:destroy() end
    entry.bundle = bundle
    entry.object = bundle.object
    entry.path = path
    entry.runtimeKey = nil
    entry.runtimeClip = nil
    entry.runtimeOverride = false
    entry.runtimeStartTime = 0
    entry.clipKey = entry.clipKey or ''
    entry.base.visible = entry.base.visible ~= false
    entry.object.alwaysRender = true
    entry.object.visible = entry.base.visible
    refreshDefaultTimelineDuration()
    applyTransform(entry)
    S.baseReference = nil
    S.modified = true
    setStatus(string.format(L('loaded'), actorName(id, entry), path))
    return true
end

local function openActorFile(id)
    local entry = actor(id)
    local path = mbm.openFile(entry.path or '', 'msh')
    if path then loadActor(id, path); seekAll(S.time, false) end
end

local function resetPlayback()
    S.playing = false
    seekAll(0, false)
end

local function playPlayback()
    if clockClip().kind == 'none' then
        setStatus(L('clock_missing'), true)
        return
    end
    S.playing = true
    if S.time >= projectDuration() - 0.0001 then
        seekAll(0, true)
    else
        seekAll(S.time, true)
    end
end

local function pausePlayback()
    S.playing = false
    seekAll(S.time, false)
end

local function serializeProject()
    local events = cloneTable(S.events)
    local maximumTime = projectDuration()
    for _, event in ipairs(events) do normalizeTransition(event, maximumTime) end
    local project = {
        version = 8,
        language = tLang.getLanguage(),
        baseReference = cloneTable(S.baseReference),
        actors = {},
        actorOrder = cloneTable(S.actorOrder),
        anchorActor = S.anchorActor,
        clockActor = S.clockActor,
        clockKey = S.clockKey,
        duration = projectDuration(),
        continueClipsAfterTimeline = S.continueClipsAfterTimeline,
        time = S.time,
        events = events,
    }
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        project.actors[id] = {
            path = entry.path,
            base = cloneTable(entry.base),
            clipKey = entry.clipKey,
            selectable = entry.selectable ~= false,
        }
    end
    return project
end

local function clearActorSlots()
    for _, id in ipairs(actorSlots) do
        local entry = S.actors[id]
        if entry.object then entry.object:destroy() end
    end
    actorSlots = {}
    S.actors = {}
    S.actorOrder = {}
    S.anchorActor = nil
    S.clockActor = nil
    S.clockKey = ''
    S.selectedActor = nil
    S.baseReference = nil
    S.timelineDuration = 0.01
    S.timelineDurationExplicit = false
    S.continueClipsAfterTimeline = false
end

local function applyProject(project)
    if type(project) ~= 'table' then error(L('invalid_project')) end
    if project.language == 'en' or project.language == 'pt_br' then
        tLang.setLanguage(project.language)
        refreshWindowTitles()
    end
    S.playing = false
    clearActorSlots()
    local savedActors = project.actors or {}
    local savedOrder = project.actorOrder or {}
    local loaded = {}
    local function loadSavedActor(id, saved)
        if type(id) ~= 'string' or type(saved) ~= 'table' or loaded[id] then return end
        local index = tonumber(id:match('^actor(%d+)$')) or (#actorSlots + 1)
        table.insert(actorSlots, id)
        local entry = newActorState(index == 1 and 180 or 0)
        entry.active = true
        entry.base = cloneTable(saved.base or entry.base)
        entry.base.visible = entry.base.visible ~= false
        entry.selectable = saved.selectable ~= false
        entry.clipKey = saved.clipKey or ''
        S.actors[id] = entry
        if saved.path and saved.path ~= '' then loadActor(id, saved.path) end
        loaded[id] = true
    end
    for _, id in ipairs(savedOrder) do
        loadSavedActor(id, savedActors[id])
    end
    local remaining = {}
    for id, saved in pairs(savedActors) do
        if not loaded[id] then table.insert(remaining, {id = id, saved = saved}) end
    end
    table.sort(remaining, function(left, right)
        local leftNumber = tonumber(tostring(left.id):match('^actor(%d+)$')) or math.huge
        local rightNumber = tonumber(tostring(right.id):match('^actor(%d+)$')) or math.huge
        return leftNumber < rightNumber
    end)
    for _, item in ipairs(remaining) do
        loadSavedActor(item.id, item.saved)
    end
    rebuildActorOrder()
    S.timelineDurationExplicit = tonumber(project.version or 0) >= 7 and tonumber(project.duration) ~= nil
    if S.timelineDurationExplicit then
        S.timelineDuration = math.max(0.01, tonumber(project.duration) or 0)
    else
        refreshDefaultTimelineDuration()
    end
    S.baseReference = cloneTable(project.baseReference) or buildBaseReference()
    local anchorActor = project.anchorActor
    local clockActor = project.clockActor
    S.anchorActor = actor(anchorActor) and anchorActor or S.actorOrder[1]
    S.clockActor = actor(clockActor) and clockActor or S.actorOrder[1]
    S.events = cloneTable(project.events or {})
    for _, event in ipairs(S.events) do
        normalizeEventDefaults(event)
        if event.action ~= 'notify' and not actor(event.target) then event.target = S.actorOrder[1] end
        if type(event.id) ~= 'string' then event.id = '' end
    end
    S.gizmo.axesWorld = nil
    S.clockKey = project.clockKey or ''
    if S.clockKey == '' and actor(S.clockActor) then S.clockKey = actor(S.clockActor).clipKey or '' end
    if not S.timelineDurationExplicit then refreshDefaultTimelineDuration() end
    S.timelineDuration = math.max(projectDuration(), maximumEventTime())
    S.continueClipsAfterTimeline = project.continueClipsAfterTimeline == true
    local maximumTime = projectDuration()
    for _, event in ipairs(S.events) do normalizeTransition(event, maximumTime) end
    sortEvents()
    S.time = tonumber(project.time) or 0
    seekAll(S.time, false)
    S.modified = false
end

local function eventIdExists(identifier, except)
    for index, event in ipairs(S.events) do
        if index ~= except and event.id == identifier then return true end
    end
    return false
end

local function nextEventId()
    local index = #S.events + 1
    local identifier = 'event_' .. index
    while eventIdExists(identifier) do
        index = index + 1
        identifier = 'event_' .. index
    end
    return identifier
end

local function validateEventIds()
    local seen = {}
    for _, event in ipairs(S.events) do
        local identifier = event.id or ''
        if identifier ~= '' then
            if seen[identifier] then
                setStatus(string.format(L('event_id_duplicate'), identifier), true)
                return false
            end
            seen[identifier] = true
        end
    end
    return true
end

local function validateProject(forExport)
    local function fail(key, ...)
        setStatus(string.format(L(key), ...), true)
        return false
    end

    local function finite(value)
        return type(value) == 'number' and value == value and math.abs(value) < math.huge
    end

    local function checkNumber(value, key, label)
        if not finite(value) then return fail(key, label) end
        return true
    end

    for _, event in ipairs(S.events) do normalizeEventDefaults(event) end
    if not validateEventIds() then return false end
    local duration = S.timelineDuration
    if not finite(duration) or duration <= 0 then
        return fail('validation_duration')
    end

    local seenActors = {}
    local actorCount = 0
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        if type(id) ~= 'string' or seenActors[id] or not entry then
            return fail('validation_actor_order', tostring(id))
        end
        seenActors[id] = true
        actorCount = actorCount + 1
        if forExport and (not entry.path or entry.path == '') then
            return fail('validation_actor_mesh', actorName(id, entry))
        end
        for _, field in ipairs({'x', 'y', 'z', 'rx', 'ry', 'rz', 'sx', 'sy', 'sz'}) do
            if not checkNumber(entry.base[field], 'validation_actor_transform',
                actorName(id, entry) .. '.' .. field) then return false end
        end
        if forExport and entry.clipKey ~= '' and findClip(entry, entry.clipKey).kind == 'none' then
            return fail('validation_actor_clip', actorName(id, entry), entry.clipKey)
        end
    end
    for id, entry in pairs(S.actors) do
        if entry.active and not seenActors[id] then
            return fail('validation_actor_order', tostring(id))
        end
    end
    if forExport and actorCount == 0 then return fail('validation_no_actors') end

    if actorCount > 0 then
        if not actor(S.anchorActor) then return fail('validation_anchor') end
        if not actor(S.clockActor) then return fail('validation_clock_actor') end
        if forExport and clockClip().kind == 'none' then
            return fail('validation_clock_animation')
        end
    end

    for index, event in ipairs(S.events) do
        local label = event.id and event.id ~= '' and event.id or tostring(index)
        if type(event.action) ~= 'string' or
            (event.action ~= 'transform' and event.action ~= 'play' and
                event.action ~= 'notify' and event.action ~= 'visibility') then
            return fail('validation_event_action', label)
        end
        local time = tonumber(event.time)
        if not checkNumber(time, 'validation_event_time', label) or time < 0 or time > duration then
            return fail('validation_event_time', label)
        end
        if event.action ~= 'notify' and not actor(event.target) then
            return fail('validation_event_target', label)
        end
        if event.action == 'play' then
            if type(event.clipKey) ~= 'string' or event.clipKey == '' then
                return fail('validation_event_clip', label)
            end
            if forExport and findClip(actor(event.target), event.clipKey).kind == 'none' then
                return fail('validation_event_clip', label)
            end
        elseif event.action == 'visibility' then
            if event.visible ~= nil and type(event.visible) ~= 'boolean' then
                return fail('validation_event_visible', label)
            end
        elseif event.action == 'transform' then
            for _, field in ipairs({'x', 'y', 'z', 'rx', 'ry', 'rz', 'sx', 'sy', 'sz'}) do
                if not checkNumber(event[field], 'validation_event_number', label .. '.' .. field) then
                    return false
                end
            end
            if event.transition ~= 'instant' and event.transition ~= 'smooth' then
                return fail('validation_event_transition', label)
            end
            if event.transition == 'smooth' then
                local start = tonumber(event.transitionStart)
                local finish = tonumber(event.transitionEnd or event.time)
                if not finite(start) or not finite(finish) or start < 0 or finish < start or finish > duration then
                    return fail('validation_event_range', label)
                end
                local lower, upper = transitionBounds(event, duration)
                if start < lower - 1e-6 or finish > upper + 1e-6 then
                    return fail('validation_event_range', label)
                end
            end
        end
    end
    return true
end

local function saveLua(path, name, value, returnValue)
    local output = {}
    if returnValue then table.insert(output, 'local ' .. name .. ' = {}') end
    tUtil.save(name, value, output)
    if returnValue then table.insert(output, 'return ' .. name) end
    local file = io.open(path, 'w')
    if not file then
        setStatus(L('save_failed'), true)
        return false
    end
    for _, line in ipairs(output) do file:write(line .. '\n') end
    file:close()
    return true
end

local function saveProject(path)
    if not validateProject(false) then return end
    path = path or S.projectPath
    if not path or path == '' then path = mbm.saveFile('', 'iae.lua') end
    if not path then return end
    if saveLua(path, 'interactionProject', serializeProject(), false) then
        S.projectPath = path
        S.modified = false
        setStatus(string.format(L('saved'), path))
    end
end

local function saveProjectAs()
    if not validateProject(false) then return end
    local path = mbm.saveFile(S.projectPath or '', 'iae.lua')
    if path then saveProject(path) end
end

local function loadProject(path)
    path = path or mbm.openFile(S.projectPath or '', 'iae.lua')
    if not path then return end
    local fn, err = loadfile(path)
    if not fn then
        setStatus(err, true)
        return
    end
    interactionProject = nil
    local ok = dpCall(fn)
    if not ok or type(interactionProject) ~= 'table' then
        setStatus(L('invalid_project'), true)
        return
    end
    local applied = dpCall(applyProject, interactionProject)
    if applied then
        S.projectPath = path
        setStatus(string.format(L('loaded_project'), path))
    end
end

local function serializeRuntimeDefinition()
    local definition = serializeProject()
    definition.language = nil
    definition.baseReference = nil
    definition.time = nil
    for _, actorDefinition in pairs(definition.actors) do
        actorDefinition.path = nil
        actorDefinition.base = nil
    end
    return definition
end

local function saveReactionModule(path)
    local output = {'local definition = {}'}
    tUtil.save('definition', serializeRuntimeDefinition(), output)
    table.insert(output, 'return definition')
    local file = io.open(path, 'w')
    if not file then
        setStatus(L('save_failed'), true)
        return false
    end
    for _, line in ipairs(output) do file:write(line .. '\n') end
    file:close()
    return true
end

local function exportSpec(path)
    if not validateProject(true) then return end
    path = path or mbm.saveFile(exportSuggestion(), 'lua')
    if not path then return end
    if saveReactionModule(path) then
        setStatus(string.format(L('exported'), path))
    end
end

local function previewDriverSource(projectPath)
    local cameraX, cameraY, cameraZ = cameraPosition()
    local focusX, focusY, focusZ = S.cameraOrbit.fx, S.cameraOrbit.fy, S.cameraOrbit.fz
    return string.format([[
local project = dofile(%q)
local reactionRuntime = require 'interaction_animation_runtime'
local actors = {}

local function loadActor(id, definition)
    local base = definition.base or {}
    mbm.addPath(tostring(definition.path or ''):gsub('\\', '/'):match('^(.*)/[^/]*$') or '.')
    local object = mesh:new('3d')
    assert(object:load(definition.path), 'Could not load interaction mesh: ' .. tostring(definition.path))
    object.alwaysRender = true
    object:setPos(base.x or 0, base.y or 0, base.z or 0)
    object:setAngle(math.rad(base.rx or 0), math.rad(base.ry or 0), math.rad(base.rz or 0))
    object:setScale(base.sx or 1, base.sy or 1, base.sz or 1)
    object.visible = base.visible ~= false
    actors[id] = object
end

function onInitScene()
    mbm.setAmbientLight('3d', {r = 0.25, g = 0.25, b = 0.25, a = 1})
    mbm.setDirectionalLight('3d', {x = -0.8, y = -0.7, z = -0.5},
        {r = 1, g = 0.98, b = 0.9, a = 1})
    local camera = mbm.getCamera('3d')
    camera:setPos(%.17g, %.17g, %.17g)
    camera:setFocus(%.17g, %.17g, %.17g)
    for _, id in ipairs(project.actorOrder or {}) do
        loadActor(id, project.actors[id])
    end
    reactionRuntime.start {
        definition = project,
        actors = actors,
        callbacks = {
            onStart = function() print('Interaction preview started') end,
            onEvent = function(_, event) print('Interaction event: ' .. tostring(event.id)) end,
            onFinish = function() print('Interaction preview finished') end,
        },
    }
end

function onKeyDown(key)
    if key == mbm.getKeyCode('ESC') then mbm.quit() end
end
]], projectPath, cameraX, cameraY, cameraZ, focusX, focusY, focusZ)
end

local function writePreviewFiles(projectPath, driverPath)
    if not saveLua(projectPath, 'interactionProject', serializeProject(), true) then return false end
    local driver = previewDriverSource(projectPath)
    local file, err = io.open(driverPath, 'w')
    if not file then
        setStatus(tostring(err), true)
        return false
    end
    file:write(driver)
    file:close()
    return true
end

local function startPreview(driverPath)
    local width, height = mbm.getSizeScreen()
    width, height = math.max(320, math.floor(width or 1280)), math.max(240, math.floor(height or 720))
    tUtil.newInstance(width, height, width, height, driverPath)
end

local function runPreview()
    if not validateProject(true) then return end
    if #S.actorOrder == 0 then
        setStatus(L('run_preview_no_actors'), true)
        return
    end

    local projectPath = tUtil.getTemporaryFilePath('_iae_preview_project.lua')
    local driverPath = tUtil.getTemporaryFilePath('_iae_preview_main.lua')
    if not writePreviewFiles(projectPath, driverPath) then return end
    startPreview(driverPath)
    setStatus(L('run_preview_started'))
end

local function savePreviewAs()
    if not validateProject(true) then return end
    if #S.actorOrder == 0 then
        setStatus(L('run_preview_no_actors'), true)
        return
    end
    local defaultPath = S.projectPath ~= '' and
        S.projectPath:gsub('%.iae%.lua$', '-preview.lua') or 'interaction-preview.lua'
    local driverPath = mbm.saveFile(defaultPath, 'lua')
    if not driverPath then return end
    local projectPath = driverPath:gsub('%.lua$', '') .. '.iae.lua'
    if not writePreviewFiles(projectPath, driverPath) then return end
    setStatus(string.format(L('preview_saved'), driverPath, projectPath))
end

local function eventLabel(event, index)
    if event.action == 'notify' then
        local identifier = event.id and event.id ~= '' and (' [' .. event.id .. ']') or ''
        return string.format('%d: %.3fs%s %s', index, event.time or 0, identifier, L('notify_event'))
    end
    local targetEntry = actor(event.target)
    local target = targetEntry and actorName(event.target, targetEntry) or string.format(L('actor_default'), 0)
    local action = event.action == 'play' and L('play_animation') or
        event.action == 'visibility' and L('visibility') or L('transform')
    local identifier = event.id and event.id ~= '' and (' [' .. event.id .. ']') or ''
    local timeLabel
    if event.action == 'transform' and event.transition == 'smooth' then
        local start, finish = transitionRange(event, projectDuration())
        timeLabel = string.format('%.3fs -> %.3fs', start, finish)
    else
        timeLabel = string.format('%.3fs', event.time or 0)
    end
    return string.format('%d: %s%s %s %s', index, timeLabel, identifier, target, action)
end

local function captureTransformEvent()
    local selectedEvent = S.events[S.selectedEvent]
    local event = selectedEvent and selectedEvent.action == 'transform' and selectedEvent or nil
    local id = event and event.target or S.selectedActor or (selectedEvent and selectedEvent.target)
    local entry = id and actor(id) or nil
    if not entry then
        setStatus(L('capture_actor_missing'), true)
        return
    end
    if S.playing then S.playing = false end
    if not S.baseReference then
        S.baseReference = buildBaseReference()
    end
    local reference = S.baseReference.actors[id]
    if not reference then
        setStatus(L('base_reference_missing'), true)
        return
    end
    local origin = reference.base
    local current = displayBase(entry)
    local axes = interactionAxes()
    local dx, dy, dz = current.x - origin.x, current.y - origin.y, current.z - origin.z
    local eventRotation
    local originRotation = eulerRotation(math.rad(origin.rx), math.rad(origin.ry), math.rad(origin.rz))
    local currentRotation = eulerRotation(math.rad(current.rx), math.rad(current.ry), math.rad(current.rz))
    local baseDeltaRotation = multiplyRotation(transposeRotation(originRotation), currentRotation)
    if event then
        local prefixRotation = identityRotation()
        for index, previous in ipairs(S.events) do
            if index == S.selectedEvent then break end
            if previous.action == 'transform' and previous.target == id then
                local progress = transformProgress(previous, S.time)
                if progress > 0 then
                    prefixRotation = multiplyRotation(prefixRotation, anchorEulerRotation(
                        (previous.rx or 0) * progress,
                        (previous.ry or 0) * progress,
                        (previous.rz or 0) * progress))
                end
            end
        end
        local selectedProgress = transformProgress(event, S.time)
        local selectedRotation = anchorEulerRotation(
            (event.rx or 0) * selectedProgress,
            (event.ry or 0) * selectedProgress,
            (event.rz or 0) * selectedProgress)
        eventRotation = multiplyRotation(
            multiplyRotation(transposeRotation(prefixRotation),
                multiplyRotation(baseDeltaRotation, prefixRotation)), selectedRotation)
    else
        local totalRotation = entry.offsetRotation or identityRotation()
        eventRotation = multiplyRotation(transposeRotation(totalRotation),
            multiplyRotation(baseDeltaRotation, totalRotation))
    end
    local rawRx, rawRy, rawRz = rotationToAnchorEuler(eventRotation)
    local rotationReference = {
        rx = current.rx - origin.rx,
        ry = current.ry - origin.ry,
        rz = current.rz - origin.rz,
    }
    local rotationIntent = entry.rotationIntent
    if rotationIntent then
        local field = rotationIntent.axisName == 'x' and 'rx' or
            rotationIntent.axisName == 'y' and 'ry' or 'rz'
        local intendedAngle = rotationIntent.angle
        if event then
            intendedAngle = intendedAngle + (event[field] or 0) * transformProgress(event, S.time)
        end
        local otherA, otherB
        if field == 'rx' then
            otherA, otherB = rawRy, rawRz
        elseif field == 'ry' then
            otherA, otherB = rawRx, rawRz
        else
            otherA, otherB = rawRx, rawRy
        end
        if math.abs(otherA) < 0.001 and math.abs(otherB) < 0.001 then
            rotationReference[field] = intendedAngle
        end
    end
    local rx = capturedRotationAngle(rawRx, rotationReference.rx)
    local ry = capturedRotationAngle(rawRy, rotationReference.ry)
    local rz = capturedRotationAngle(rawRz, rotationReference.rz)
    local isNewEvent = not event
    if isNewEvent then
        event = {
            id = nextEventId(),
            time = S.time,
            target = id,
            action = 'transform',
            transition = 'instant',
            clipKey = '',
        }
        table.insert(S.events, event)
        S.selectedEvent = #S.events
        sortEvents()
    end
    if event.transition == 'smooth' then
        local start, finish = transitionRange(event, projectDuration())
        event.transitionStart = start
        event.transitionEnd = finish
        event.time = finish
        event.duration = nil
    else
        event.time = S.time
    end
    event.target = id
    event.action = 'transform'
    local capturedX = dot(dx, dy, dz, axes.x.x, axes.x.y, axes.x.z)
    local capturedY = dot(dx, dy, dz, axes.y.x, axes.y.y, axes.y.z)
    local capturedZ = dot(dx, dy, dz, axes.z.x, axes.z.y, axes.z.z)
    local function scaleRatio(currentValue, originValue)
        if math.abs(originValue) < 1e-6 then return 1 end
        return currentValue / originValue
    end
    local capturedScaleX = scaleRatio(current.sx, origin.sx)
    local capturedScaleY = scaleRatio(current.sy, origin.sy)
    local capturedScaleZ = scaleRatio(current.sz, origin.sz)
    if not isNewEvent then
        local selectedProgress = transformProgress(event, S.time)
        event.x = (event.x or 0) * selectedProgress + capturedX
        event.y = (event.y or 0) * selectedProgress + capturedY
        event.z = (event.z or 0) * selectedProgress + capturedZ
        if selectedProgress > 1e-6 then
            local currentScaleX = 1 + ((event.sx or 1) - 1) * selectedProgress
            local currentScaleY = 1 + ((event.sy or 1) - 1) * selectedProgress
            local currentScaleZ = 1 + ((event.sz or 1) - 1) * selectedProgress
            event.sx = 1 + (capturedScaleX * currentScaleX - 1) / selectedProgress
            event.sy = 1 + (capturedScaleY * currentScaleY - 1) / selectedProgress
            event.sz = 1 + (capturedScaleZ * currentScaleZ - 1) / selectedProgress
        else
            event.sx, event.sy, event.sz = capturedScaleX, capturedScaleY, capturedScaleZ
        end
    else
        event.x = capturedX
        event.y = capturedY
        event.z = capturedZ
        event.sx, event.sy, event.sz = capturedScaleX, capturedScaleY, capturedScaleZ
    end
    event.rx = rx
    event.ry = ry
    event.rz = rz
    event.clipKey = ''
    entry.previewBase = nil
    S.modified = true
    seekAll(S.time, false)
    setStatus(string.format(L('captured'), actorName(id, entry), S.time))
end

local function addEventAtTime(time, target)
    local event = {
        id = nextEventId(),
        time = clamp(time or S.time, 0, projectDuration()),
        target = actor(target) and target or S.selectedActor or S.clockActor,
        action = 'transform',
        x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1,
        transition = 'instant',
        clipKey = '',
    }
    table.insert(S.events, event)
    S.selectedEvent = #S.events
    sortEvents()
    S.modified = true
    seekAll(event.time, false)
    showEventHighlight(event)
end

local function addEvent()
    addEventAtTime(S.time, S.selectedActor or S.clockActor)
end

local function addEventContextMenu(event, index, pending)
    if not tImGui.BeginPopupContextItem('iae_event_context_' .. tostring(index)) then return end
    local target = event.action ~= 'notify' and event.target or S.selectedActor or S.clockActor
    if event.action == 'transform' and event.transition == 'smooth' then
        local start, finish = transitionRange(event, projectDuration())
        if tImGui.MenuItem(string.format(L('create_event_transition_start'), start)) then
            pending.time = start
            pending.target = target
        end
        if tImGui.MenuItem(string.format(L('create_event_transition_end'), finish)) then
            pending.time = finish
            pending.target = target
        end
    elseif tImGui.MenuItem(L('create_event_same_time')) then
        pending.time = event.time or 0
        pending.target = target
    end
    tImGui.EndPopup()
end

local function removeSelectedEvent()
    if S.selectedEvent < 1 or S.selectedEvent > #S.events then return end
    table.remove(S.events, S.selectedEvent)
    S.selectedEvent = math.min(S.selectedEvent, #S.events)
    S.modified = true
    seekAll(S.time, false)
    showEventHighlight(S.events[S.selectedEvent])
end

local function updatePlayback(delta)
    if not S.playing then return end
    local clip = clockClip()
    local clockEntry = actor(S.clockActor)
    if not clockEntry or clip.kind == 'none' or not clockEntry.object then
        S.playing = false
        return
    end
    local previousTime = S.time
    S.time = math.min(projectDuration(), S.time + math.max(delta or 0, 0))
    local reachedTimelineEnd = false
    if S.time >= projectDuration() - 0.0001 then
        S.time = projectDuration()
        reachedTimelineEnd = true
    end
    for _, event in ipairs(S.events) do
        local eventTime = event.time or 0
        if (event.action == 'play' or event.action == 'visibility') and
            eventTime > previousTime and eventTime <= S.time then
            applyEvent(event)
        end
    end
    resetOffsets()
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        local selected = entry.runtimeOverride and entry.runtimeClip or findClip(entry, entry.clipKey)
        local localTime = entry.runtimeOverride and (S.time - entry.runtimeStartTime) or S.time
        if selected.kind ~= 'none' then
            if selected.kind == 'frame' and reachedTimelineEnd then
                -- Frame seeking pauses the animation. Apply the final frame first,
                -- then restore the asset playback mode when the project allows the
                -- clip to continue past the master timeline.
                seekClip(entry, selected, localTime, false)
                if S.continueClipsAfterTimeline and (selected.mode or 0) ~= 0 then
                    entry.object:setTypeAnim(selected.mode)
                end
            elseif not reachedTimelineEnd or not S.continueClipsAfterTimeline then
                seekClip(entry, selected, localTime, true)
            end
        end
    end
    -- Apply interaction transforms after clip sampling. Some renderable
    -- animation operations update the selected frame/player state, so the
    -- final object transform must be the last write in the playback pass.
    for _, event in ipairs(S.events) do
        if event.action == 'transform' then
            applyEvent(event, transformProgress(event, S.time))
        end
    end
    for _, id in ipairs(S.actorOrder) do
        applyTransform(actor(id))
    end
    if reachedTimelineEnd then S.playing = false end
    if not S.playing and not S.continueClipsAfterTimeline then seekAll(S.time, false) end
end

local function actorPanel(id)
    local entry = actor(id)
    if not entry then return end
    if tImGui.TreeNode(actorName(id, entry) .. '##' .. id) then
        local wasAnchor = S.anchorActor == id
        local anchorChecked = tImGui.Checkbox(L('anchor') .. '##' .. id, wasAnchor)
        if anchorChecked and not wasAnchor then
            setAnchorActor(id)
            S.modified = true
        end
        local selectable = tImGui.Checkbox(L('selectable') .. '##' .. id, entry.selectable ~= false)
        if selectable ~= (entry.selectable ~= false) then
            entry.selectable = selectable
            if not selectable and S.selectedActor == id then
                S.selectedActor = nil
                hideGizmo()
            end
            S.modified = true
        end
        local meshName = entry.path ~= '' and tUtil.getShortName(entry.path) or L('not_loaded')
        tImGui.Text(string.format(L('mesh_file'), meshName))
        if tImGui.Button(L('browse') .. '##' .. id) then openActorFile(id) end
        if entry.bundle then
            local clips = clipList(entry)
            local names, current = {}, 1
            for index, clip in ipairs(clips) do
                names[index] = clip.label
                if clip.key == entry.clipKey then current = index end
            end
            tImGui.SetNextItemWidth(230)
            local clipChanged, clipIndex = tImGui.Combo(L('animation') .. '##' .. id, current, names)
            if clipChanged then
                entry.clipKey = clips[clipIndex].key
                if id == S.clockActor then S.clockKey = entry.clipKey end
                refreshDefaultTimelineDuration()
                S.modified = true
                seekAll(S.time, false)
            end
            local base = displayBase(entry)
            tImGui.SetNextItemWidth(105)
            local c1, n1 = tImGui.InputFloat('X##base' .. id, base.x); if c1 then entry.rotationIntent = nil; local edited = editableBase(entry); edited.x = n1; applyTransform(entry); S.modified = true end
            tImGui.SetNextItemWidth(105)
            local c2, n2 = tImGui.InputFloat('Y##base' .. id, base.y); if c2 then entry.rotationIntent = nil; local edited = editableBase(entry); edited.y = n2; applyTransform(entry); S.modified = true end
            tImGui.SetNextItemWidth(105)
            local c3, n3 = tImGui.InputFloat('Z##base' .. id, base.z); if c3 then entry.rotationIntent = nil; local edited = editableBase(entry); edited.z = n3; applyTransform(entry); S.modified = true end
            local clip = findClip(entry, entry.clipKey)
            tImGui.Text(string.format(L('duration'), clip.duration or 0))
        else
            tImGui.Text(L('not_loaded'))
        end
        tImGui.TreePop()
    end
end

local function showActors()
    tUtil.setInitialWindowPositionLeft(S.windows.actors, 0, 20, 380)
    local opened = tImGui.Begin(S.windows.actors, true, 0)
    if opened then
        tImGui.Text(L('actors'))
        if tImGui.Button(L('add_actor')) then
            addActor()
        end
        if #S.actorOrder == 0 then tImGui.TextWrapped(L('add_actor_hint')) end
        tImGui.Separator()
        for _, id in ipairs(S.actorOrder) do actorPanel(id) end
        if #S.actorOrder > 0 then
            tImGui.Separator()
            tImGui.TextWrapped(L('base_reference_auto'))
            local choices, clockIndex = {}, 1
            for index, id in ipairs(S.actorOrder) do
                choices[index] = actorName(id, actor(id))
                if id == S.clockActor then clockIndex = index end
            end
            tImGui.SetNextItemWidth(210)
            local clockChanged, nextClock = tImGui.Combo(L('clock_actor'), clockIndex, choices)
            if clockChanged then
                setClockActor(S.actorOrder[nextClock])
                refreshDefaultTimelineDuration()
                S.modified = true
                seekAll(S.time, false)
            end
            local clockNames, clockCurrent = {}, 1
            for index, clip in ipairs(clipList(actor(S.clockActor))) do
                clockNames[index] = clip.label
                if clip.key == S.clockKey then clockCurrent = index end
            end
            tImGui.SetNextItemWidth(230)
            local clockChanged2, clockIndex2 = tImGui.Combo(L('clock_animation'), clockCurrent, clockNames)
            if clockChanged2 then
                S.clockKey = clipList(actor(S.clockActor))[clockIndex2].key
                actor(S.clockActor).clipKey = S.clockKey
                refreshDefaultTimelineDuration()
                S.time = 0
                S.modified = true
                seekAll(0, false)
            end
            tImGui.Text(string.format(L('clock_duration'), clockDuration()))
        end
        tImGui.TextWrapped(L('anchor_space_note'))
        local modeIndex = tImGui.RadioButton(L('move'), S.transformMode == 'move' and 1 or 2, 1)
        tImGui.SameLine()
        modeIndex = tImGui.RadioButton(L('rotate'), modeIndex, 2)
        if S.transformMode ~= (modeIndex == 2 and 'rotate' or 'move') then
            S.transformMode = modeIndex == 2 and 'rotate' or 'move'
            S.gizmo.drag = nil
        end
        if S.selectedActor and actor(S.selectedActor) then
            local selected = actor(S.selectedActor)
            tImGui.Separator()
            tImGui.Text(string.format('%s: %s', L('selected_actor'), actorName(S.selectedActor, selected)))
            local base = displayBase(selected)
            tImGui.SetNextItemWidth(105)
            local changed, value = tImGui.InputFloat('POS X##iae_selected_pos_x', base.x)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.x = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('POS Y##iae_selected_pos_y', base.y)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.y = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('POS Z##iae_selected_pos_z', base.z)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.z = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('ROT X##iae_selected_rot_x', base.rx)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.rx = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('ROT Y##iae_selected_rot_y', base.ry)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.ry = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('ROT Z##iae_selected_rot_z', base.rz)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.rz = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat(L('scale_x') .. '##iae_selected_scale_x', base.sx, 0.05)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.sx = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat(L('scale_y') .. '##iae_selected_scale_y', base.sy, 0.05)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.sy = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat(L('scale_z') .. '##iae_selected_scale_z', base.sz, 0.05)
            if changed then selected.rotationIntent = nil; local edited = editableBase(selected); edited.sz = value; applyTransform(selected); S.modified = true end
            tImGui.TextWrapped(S.transformMode == 'move' and L('move_gizmo_help') or L('rotate_gizmo_help'))
            if math.abs(S.time) > 1e-6 then tImGui.TextWrapped(L('preview_transform_note')) end
        else
            tImGui.Text(L('select_actor_hint'))
        end
    end
    tImGui.End()
end

local function showCameraPanel()
    tUtil.setInitialWindowPositionRight(S.windows.camera, 0, 20, 330, 360, 360)
    local opened = tImGui.Begin(S.windows.camera, true, 0)
    if opened then
        tImGui.Text(L('camera'))
        if tUtil.drawOrbitGizmo(S.cameraOrbit, {size = 110}) then applyCamera() end
        if tImGui.Button(L('reset_camera')) then resetCamera() end
        local c = S.cameraOrbit
        local changed, value = tImGui.InputFloat(L('camera_distance'), c.distance)
        if changed then c.distance = math.max(80, value); applyCamera() end
        changed, value = tImGui.InputFloat('Focus X', c.fx)
        if changed then c.fx = value; applyCamera() end
        changed, value = tImGui.InputFloat('Focus Y', c.fy)
        if changed then c.fy = value; applyCamera() end
        changed, value = tImGui.InputFloat('Focus Z', c.fz)
        if changed then c.fz = value; applyCamera() end
        local showOrigins = tImGui.Checkbox(L('show_origins'), S.showOrigins)
        if showOrigins ~= S.showOrigins then setOriginsVisible(showOrigins) end
        local showXZPlane = tImGui.Checkbox(L('show_xz_plane'), S.showXZPlane)
        if showXZPlane ~= S.showXZPlane then S.showXZPlane = showXZPlane end
    end
    tImGui.End()
end

local function showEvents()
    tUtil.setInitialWindowPositionRight(S.windows.events, 0, 390, 330, 360, 560)
    local opened = tImGui.Begin(S.windows.events, true, 0)
    if opened then
        tImGui.Text(L('events'))
        if tImGui.Button(L('add_event')) then addEvent() end
        tImGui.SameLine()
        if tImGui.Button(L('remove_event')) then removeSelectedEvent() end
        local pendingEvent = {}
        for index, event in ipairs(S.events) do
            if tImGui.Selectable(eventLabel(event, index), index == S.selectedEvent) then
                S.selectedEvent = index
                S.playing = false
                local eventTime = event.time or 0
                if event.transition == 'smooth' then
                    local _, transitionEnd = transitionRange(event, projectDuration())
                    eventTime = transitionEnd
                end
                seekAll(eventTime, false)
                showEventHighlight(event)
            end
            addEventContextMenu(event, index, pendingEvent)
        end
        if pendingEvent.time ~= nil then
            addEventAtTime(pendingEvent.time, pendingEvent.target)
        end
        local event = S.events[S.selectedEvent]
        if event then
            tImGui.Separator()
            tImGui.SetNextItemWidth(180)
            local idChanged, identifier = tImGui.InputText(L('event_id'), event.id or '')
            if idChanged then
                event.id = identifier:gsub('[%z\1-\31\127]', '')
                S.modified = true
                validateEventIds()
            end
            if event.transition == 'smooth' then
                local _, finish = transitionRange(event, projectDuration())
                tImGui.Text(string.format(L('transition_end_value'), finish))
                tImGui.SameLine()
                tImGui.HelpMarker(L('transition_end_help'))
            else
                tImGui.SetNextItemWidth(105)
                local c, value = tImGui.InputFloat(L('event_time'), event.time or 0, 0.05)
                tImGui.SameLine()
                tImGui.HelpMarker(L('event_time_help'))
                if c then event.time = clamp(value, 0, projectDuration()) end
                if c then
                    sortEvents()
                    S.modified = true
                    seekAll(S.time, false)
                end
                tImGui.NewLine()
                if tImGui.Button(L('set_event_time_current'), {x = 210, y = 0}) then
                    setEventTimeToTimeline(event)
                end
            end
            if event.action ~= 'notify' then
                local targetIds, targetNames, targetIndex = activeActorIds(), {}, 1
                for index, id in ipairs(targetIds) do
                    targetNames[index] = actorName(id, actor(id))
                    if event.target == id then targetIndex = index end
                end
                if #targetIds > 0 then
                    tImGui.SetNextItemWidth(210)
                    local targetChanged, nextTarget = tImGui.Combo(L('event_target'), targetIndex, targetNames)
                    if targetChanged then
                        event.target = targetIds[nextTarget]
                        S.modified = true
                        showEventHighlight(event)
                    end
                end
            end
            tImGui.SetNextItemWidth(210)
            local actionIndex = event.action == 'play' and 2 or
                event.action == 'notify' and 3 or event.action == 'visibility' and 4 or 1
            local actionChanged, nextActionIndex = tImGui.Combo(
                L('event_action'), actionIndex,
                {L('transform'), L('play'), L('notify_event'), L('visibility')})
            if actionChanged then
                event.action = nextActionIndex == 2 and 'play' or
                    nextActionIndex == 3 and 'notify' or
                    nextActionIndex == 4 and 'visibility' or 'transform'
                if event.action == 'notify' then
                    event.target = nil
                    event.clipKey = ''
                    event.transition = 'instant'
                    event.transitionStart = nil
                    event.transitionEnd = nil
                    event.duration = nil
                elseif event.action == 'visibility' then
                    event.target = actor(event.target) and event.target or S.selectedActor or S.clockActor
                    event.visible = event.visible ~= false
                    event.clipKey = ''
                    event.transition = 'instant'
                    event.transitionStart = nil
                    event.transitionEnd = nil
                    event.duration = nil
                elseif not actor(event.target) then
                    event.target = S.selectedActor or S.clockActor
                end
                S.modified = true
                seekAll(S.time, false)
                showEventHighlight(event)
            end
            if event.action == 'play' then
                local target = actor(event.target)
                local clips, names, current = clipList(target), {}, 1
                for index, clip in ipairs(clips) do names[index] = clip.label; if clip.key == event.clipKey then current = index end end
                tImGui.SetNextItemWidth(230)
                local clipChanged, clipIndex = tImGui.Combo(L('event_animation'), current, names)
                if clipChanged then event.clipKey = clips[clipIndex].key; S.modified = true; seekAll(S.time, false) end
            elseif event.action == 'visibility' then
                local visible = tImGui.Checkbox(L('visible'), event.visible ~= false)
                if visible ~= (event.visible ~= false) then
                    event.visible = visible
                    S.modified = true
                    seekAll(S.time, false)
                end
            elseif event.action == 'transform' then
                tImGui.SetNextItemWidth(210)
                local transitionChanged, transitionIndex = tImGui.Combo(
                    L('transition'), event.transition == 'smooth' and 2 or 1,
                    {L('instant'), L('smooth')})
                if transitionChanged then
                    event.transition = transitionIndex == 2 and 'smooth' or 'instant'
                    if event.transition == 'smooth' then
                        local minimum, maximum = transitionBounds(event, projectDuration())
                        local start = clamp(event.time or S.time, minimum, maximum)
                        local finish = math.min(maximum, start + 0.2)
                        event.transitionStart = start
                        event.transitionEnd = finish
                        event.time = finish
                        event.duration = nil
                    else
                        event.time = select(2, transitionRange(event, projectDuration()))
                        event.transitionStart = nil
                        event.transitionEnd = nil
                        event.duration = nil
                    end
                    sortEvents()
                    S.modified = true
                    seekAll(S.time, false)
                end
                if event.transition == 'smooth' then
                    local start, finish = transitionRange(event, projectDuration())
                    local minimum, maximum = transitionBounds(event, projectDuration())
                    tImGui.SetNextItemWidth(210)
                    local rangeChanged, range = tImGui.SliderFloat2(
                        L('transition_interval'), {start, finish}, minimum, maximum)
                    tImGui.SameLine()
                    tImGui.HelpMarker(L('transition_interval_help'))
                    if rangeChanged then
                        start = clamp(range[1], minimum, maximum)
                        finish = clamp(range[2], start, maximum)
                        event.transitionStart = start
                        event.transitionEnd = finish
                        event.time = finish
                        event.duration = nil
                        sortEvents()
                        S.modified = true
                        seekAll(S.time, false)
                    end
                    if tImGui.Button(L('set_transition_start'), {x = 105, y = 0}) then
                        setTransitionEndpoint(event, 'start')
                    end
                    tImGui.SameLine()
                    if tImGui.Button(L('set_transition_end'), {x = 105, y = 0}) then
                        setTransitionEndpoint(event, 'end')
                    end
                end
                local fields = {{'x', L('offset_x'), 0}, {'y', L('offset_y'), 0}, {'z', L('offset_z'), 0},
                    {'rx', L('offset_rx'), 0}, {'ry', L('offset_ry'), 0}, {'rz', L('offset_rz'), 0},
                    {'sx', L('scale_x'), 1, 0.05}, {'sy', L('scale_y'), 1, 0.05},
                    {'sz', L('scale_z'), 1, 0.05}}
                for _, field in ipairs(fields) do
                    tImGui.SetNextItemWidth(105)
                    local changed, value
                    if field[4] then
                        changed, value = tImGui.InputFloat(field[2], event[field[1]] or field[3], field[4])
                    else
                        changed, value = tImGui.InputFloat(field[2], event[field[1]] or field[3])
                    end
                    if changed then event[field[1]] = value; S.modified = true; seekAll(S.time, false) end
                end
            end
        end
        if not event or event.action == 'transform' then
            tImGui.Separator()
            if tImGui.Button(L('capture_transform')) then captureTransformEvent() end
        end
    end
    tImGui.End()
end

local function showTimeline()
    tUtil.setInitialWindowPositionDown(S.windows.timeline, 390, 0.16, 390)
    local opened = tImGui.Begin(S.windows.timeline, true, 0)
    if opened then
        local maxTime = projectDuration()
        tImGui.SetNextItemWidth(120)
        local durationChanged, durationValue = tImGui.InputFloat(
            L('timeline_duration'), S.timelineDuration, 0.05)
        if durationChanged then
            setTimelineDuration(durationValue)
            S.modified = true
            maxTime = projectDuration()
            seekAll(S.time, false)
        end
        tImGui.SameLine()
        local continueClips = tImGui.Checkbox(
            L('continue_clips_after_timeline'), S.continueClipsAfterTimeline)
        if continueClips ~= S.continueClipsAfterTimeline then
            S.continueClipsAfterTimeline = continueClips
            S.modified = true
        end
        local changed, value = tImGui.SliderFloat(L('time'), S.time, 0, maxTime)
        if changed then pausePlayback(); seekAll(value, false) end
        tImGui.Text(string.format(L('timeline_status'), S.time, maxTime))
        if tImGui.Button(L('play')) then playPlayback() end
        tImGui.SameLine()
        if tImGui.Button(L('pause')) then pausePlayback() end
        tImGui.SameLine()
        if tImGui.Button(L('restart')) then resetPlayback() end
        tImGui.SameLine()
        if tImGui.Button(L('step_back')) then pausePlayback(); seekAll(math.max(0, S.time - 1 / 30), false) end
        tImGui.SameLine()
        if tImGui.Button(L('step')) then pausePlayback(); seekAll(math.min(maxTime, S.time + 1 / 30), false) end
    end
    tImGui.End()
end

local function showMainMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(L('file')) then
        if tImGui.MenuItem(L('load_project')) then loadProject() end
        if tImGui.MenuItem(L('save_project')) then saveProject() end
        if tImGui.MenuItem(L('save_project_as')) then saveProjectAs() end
        if tImGui.MenuItem(L('export')) then exportSpec() end
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

function onInitScene()
    refreshWindowTitles()
    S.camera = mbm.getCamera('3d')
    S.camera:setFar(9999999)
    applyCamera()
    createXZPlane()
    createOriginLines()
    tUtil.bRightSide = false
    tUtil.showMessage(L('welcome'), 13.5)
end

function onLoop(delta)
    updatePlayback(delta)
    showMainMenu()
    showActors()
    showCameraPanel()
    showEvents()
    showTimeline()
    updateGizmo()
    updateEventHighlight(delta)
    updateXZPlane()
    tUtil.showOverlayMessage()
end

function onKeyDown(key)
    if key == mbm.getKeyCode('F5') then
        runPreview()
        return
    end
    if key == mbm.getKeyCode('ESC') then
        S.selectedActor = nil
        S.gizmo.drag = nil
        S.cameraDrag = nil
        hideGizmo()
        return
    end
    if key == mbm.getKeyCode('control') then S.control = true; return end
    if not S.control then return end
    if key == mbm.getKeyCode('S') then saveProject()
    elseif key == mbm.getKeyCode('O') then loadProject()
    elseif key == mbm.getKeyCode('Q') then mbm.quit() end
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
    S.mouse.x, S.mouse.y = x, y
    if key == 0 then S.mouse.left = true end
    if key == 1 then S.mouse.right = true end
    beginViewportInteraction(key, x, y)
end

function onTouchMove(key, x, y)
    S.mouse.x, S.mouse.y = x, y
    updateViewportInteraction(x, y)
end

function onTouchUp(key, x, y)
    S.mouse.x, S.mouse.y = x, y
    if key == 0 then S.mouse.left = false end
    if key == 1 then S.mouse.right = false end
    endViewportInteraction(key)
end

function onFinalizeScene()
    hideEventHighlight()
    if S.eventHighlight.shape then S.eventHighlight.shape:destroy(); S.eventHighlight.shape = nil end
    if S.eventHighlight.line then S.eventHighlight.line:destroy(); S.eventHighlight.line = nil end
    if S.xzPlane then S.xzPlane:destroy(); S.xzPlane = nil end
    for _, object in pairs(S.originLines) do
        if object then object:destroy() end
    end
    for _, object in pairs(S.gizmo.axes) do
        if object then object:destroy() end
    end
    for _, object in pairs(S.gizmo.rings) do
        if object then object:destroy() end
    end
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        if entry.object then entry.object:destroy(); entry.object = nil end
    end
end
