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
   events. Exported Lua files are self-contained runtime reaction modules.
]]--

tImGui = require 'ImGui'
tUtil = require 'editor_utils'
local Animation = require 'mesh_to_sprite_animation'
local reactionRuntimeSource = require 'interaction_animation_runtime_source'

local actorSlots = {'actor1', 'actor2'}

local function newActorState(baseX)
    return {
        active = false,
        path = '',
        base = {x = baseX or 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1},
        bundle = nil,
        object = nil,
        clipKey = '',
        runtimeKey = nil,
        runtimeClip = nil,
        runtimeOverride = false,
        runtimeStartTime = 0,
        offset = {x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0},
    }
end

local S = {
    actors = {
        actor1 = newActorState(180),
        actor2 = newActorState(0),
    },
    actorOrder = {},
    anchorActor = nil,
    clockActor = nil,
    clockKey = '',
    time = 0,
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
    for index, slot in ipairs(actorSlots) do
        if slot == id then return index end
    end
    return nil
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

local function addActor()
    for index, id in ipairs(actorSlots) do
        if not S.actors[id].active then
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
    end
    return nil
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

local function actorPosition(entry)
    if entry.object then
        local ok, x, y, z = pcall(function() return entry.object:getAABBCenter() end)
        if ok and x and y and z then return {x = x, y = y, z = z} end
    end
    return {
        x = entry.base.x + entry.offset.x,
        y = entry.base.y + entry.offset.y,
        z = entry.base.z + entry.offset.z,
    }
end

local function actorMeshExtent(entry)
    if not entry.object then return 40 / 0.45 end
    local ok, width, height, depth = pcall(function() return entry.object:getSize(true) end)
    if not ok then return 40 / 0.45 end
    local extent = math.max(width or 0, height or 0, depth or 0)
    if extent <= 0 then
        local aabbOk, aabbWidth, aabbHeight, aabbDepth = pcall(function() return entry.object:getAABB(true) end)
        if aabbOk then extent = math.max(aabbWidth or 0, aabbHeight or 0, aabbDepth or 0) end
    end
    return math.max(extent, 40 / 0.45)
end

local function actorPickRadius(entry)
    return actorMeshExtent(entry) * 0.45
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
        if entry.object then
            local distance = raySphereDistance(sx, sy, actorPosition(entry), actorPickRadius(entry))
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
                base = cloneTable(entry.base),
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
                    base = cloneTable(entry.base),
                    startX = x,
                    startY = y,
                    moved = false,
                }
                return
            end
        end
        local picked = pickActor(x, y)
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
                entry.base.x = drag.base.x + drag.axis.x * amount
                entry.base.y = drag.base.y + drag.axis.y * amount
                entry.base.z = drag.base.z + drag.axis.z * amount
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
                    local baseRotation = eulerRotation(math.rad(drag.base.rx), math.rad(drag.base.ry),
                        math.rad(drag.base.rz))
                    local rotated = multiplyRotation(baseRotation, axisRotation(drag.axis, angle))
                    entry.base.rx, entry.base.ry, entry.base.rz = matrixToEulerDegrees(rotated)
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
    object:setColor(0.24, 0.30, 0.38, 0.18)
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
    local base, offset = entry.base, entry.offset
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
    entry.object:setScale(base.sx, base.sy, base.sz)
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
        if clip.kind == 'skeletal' then
            assert(entry.object:playSkeletalAnimation(clip.name))
        else
            assert(entry.object:playArticulatedAnimation(clip.name, clip.priority or 0, 0, 1))
        end
        entry.runtimeKey = clip.key
        entry.runtimeClip = clip
    end
    local targetTime = math.max(0, math.min(time or 0, clip.duration or 0))
    if clip.kind == 'skeletal' then
        assert(entry.object:seekSkeletalAnimation(targetTime))
        if not playing then entry.object:pauseSkeletalAnimation() end
    else
        assert(entry.object:seekArticulatedAnimation(clip.name, targetTime))
        if not playing then entry.object:pauseArticulatedAnimation(clip.name) end
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

local function clamp(value, minimum, maximum)
    return math.max(minimum, math.min(maximum, value))
end

local function transitionRange(event, maximumTime)
    local finish = tonumber(event.transitionEnd)
    if finish == nil then finish = tonumber(event.time) or 0 end
    local start = tonumber(event.transitionStart)
    if start == nil then
        start = finish - math.max(tonumber(event.duration) or 0.2, 0.0001)
    end
    start = clamp(start, 0, maximumTime)
    finish = clamp(finish, start, maximumTime)
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

local function transformProgress(event, time)
    if event.transition ~= 'smooth' then return 1 end
    local transitionStart, transitionEnd = transitionRange(event, clockDuration())
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
        applyTransform(entry)
    elseif event.action == 'play' then
        local clip = findClip(entry, event.clipKey)
        if clip.kind ~= 'none' then
            entry.runtimeOverride = true
            entry.runtimeStartTime = event.time or 0
            seekClip(entry, clip, math.max(0, S.time - entry.runtimeStartTime), S.playing)
        end
    end
end

local function resetOffsets()
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        entry.offset = {x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0}
        entry.offsetRotation = identityRotation()
        applyTransform(entry)
    end
end

local function seekAll(time, keepPlaying)
    S.time = math.max(0, math.min(time or 0, clockDuration()))
    resetOffsets()
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        stopEntry(entry)
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

local function setBaseReference()
    if #S.actorOrder == 0 then
        setStatus(L('base_reference_missing'), true)
        return
    end
    S.baseReference = buildBaseReference()
    S.modified = true
    setStatus(L('base_reference_set'))
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
    entry.object.alwaysRender = true
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
    if S.time >= clockDuration() - 0.0001 then
        seekAll(0, true)
    else
        seekAll(S.time, true)
    end
    S.playing = true
end

local function pausePlayback()
    S.playing = false
    seekAll(S.time, false)
end

local function serializeProject()
    local events = cloneTable(S.events)
    local maximumTime = clockDuration()
    for _, event in ipairs(events) do normalizeTransition(event, maximumTime) end
    local project = {
        version = 6,
        language = tLang.getLanguage(),
        baseReference = cloneTable(S.baseReference),
        actors = {},
        actorOrder = cloneTable(S.actorOrder),
        anchorActor = S.anchorActor,
        clockActor = S.clockActor,
        clockKey = S.clockKey,
        duration = clockDuration(),
        time = S.time,
        events = events,
    }
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        project.actors[id] = {
            path = entry.path,
            base = cloneTable(entry.base),
            clipKey = entry.clipKey,
        }
    end
    return project
end

local function clearActorSlots()
    for index, id in ipairs(actorSlots) do
        local entry = S.actors[id]
        if entry.object then entry.object:destroy() end
        S.actors[id] = newActorState(index == 1 and 180 or 0)
    end
    S.actorOrder = {}
    S.anchorActor = nil
    S.clockActor = nil
    S.clockKey = ''
    S.selectedActor = nil
    S.baseReference = nil
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
    for index, id in ipairs(actorSlots) do
        local saved = savedActors[id]
        if saved then
            local entry = S.actors[id]
            entry.active = true
            entry.base = cloneTable(saved.base or entry.base)
            entry.clipKey = saved.clipKey or ''
            if saved.path and saved.path ~= '' then loadActor(id, saved.path) end
        end
    end
    rebuildActorOrder()
    S.baseReference = cloneTable(project.baseReference) or buildBaseReference()
    local anchorActor = project.anchorActor
    local clockActor = project.clockActor
    S.anchorActor = actor(anchorActor) and anchorActor or S.actorOrder[1]
    S.clockActor = actor(clockActor) and clockActor or S.actorOrder[1]
    S.events = cloneTable(project.events or {})
    for _, event in ipairs(S.events) do
        if not actor(event.target) then event.target = S.actorOrder[1] end
    end
    S.gizmo.axesWorld = nil
    S.clockKey = project.clockKey or ''
    if S.clockKey == '' and actor(S.clockActor) then S.clockKey = actor(S.clockActor).clipKey or '' end
    local maximumTime = clockDuration()
    for _, event in ipairs(S.events) do normalizeTransition(event, maximumTime) end
    S.time = tonumber(project.time) or 0
    seekAll(S.time, false)
    S.modified = false
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
    path = path or S.projectPath
    if not path or path == '' then path = mbm.saveFile('', 'iae.lua') end
    if not path then return end
    if saveLua(path, 'interactionProject', serializeProject(), false) then
        S.projectPath = path
        S.modified = false
        setStatus(string.format(L('saved'), path))
    end
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

local function saveReactionModule(path)
    local output = {'local definition = {}'}
    tUtil.save('definition', serializeProject(), output)
    for line in reactionRuntimeSource:gmatch('[^\n]+') do table.insert(output, line) end
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
    path = path or mbm.saveFile(exportSuggestion(), 'lua')
    if not path then return end
    if saveReactionModule(path) then
        setStatus(string.format(L('exported'), path))
    end
end

local function eventLabel(event, index)
    local targetEntry = actor(event.target)
    local target = targetEntry and actorName(event.target, targetEntry) or string.format(L('actor_default'), 0)
    local action = event.action == 'play' and L('play_animation') or L('transform')
    return string.format('%d: %.3fs %s %s', index, event.time or 0, target, action)
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
    local reference = S.baseReference and S.baseReference.actors[id]
    if not reference then
        setStatus(L('base_reference_missing'), true)
        return
    end
    if S.playing then S.playing = false end
    local origin = reference.base
    local current = entry.base
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
    local rx, ry, rz = rotationToAnchorEuler(eventRotation)
    local isNewEvent = not event
    if isNewEvent then
        event = {
            time = S.time,
            target = id,
            action = 'transform',
            transition = 'instant',
            clipKey = '',
        }
        table.insert(S.events, event)
        S.selectedEvent = #S.events
    end
    if event.transition == 'smooth' then
        local start, finish = transitionRange(event, clockDuration())
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
    if event then
        local selectedProgress = transformProgress(event, S.time)
        event.x = (event.x or 0) * selectedProgress + capturedX
        event.y = (event.y or 0) * selectedProgress + capturedY
        event.z = (event.z or 0) * selectedProgress + capturedZ
    else
        event.x = capturedX
        event.y = capturedY
        event.z = capturedZ
    end
    event.rx = rx
    event.ry = ry
    event.rz = rz
    event.clipKey = ''
    entry.base = cloneTable(origin)
    S.modified = true
    seekAll(S.time, false)
    setStatus(string.format(L('captured'), actorName(id, entry), S.time))
end

local function addEvent()
    table.insert(S.events, {
        time = S.time,
        target = S.selectedActor or S.clockActor,
        action = 'transform',
        x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0,
        transition = 'instant',
        clipKey = '',
    })
    S.selectedEvent = #S.events
    S.modified = true
end

local function removeSelectedEvent()
    if S.selectedEvent < 1 or S.selectedEvent > #S.events then return end
    table.remove(S.events, S.selectedEvent)
    S.selectedEvent = math.min(S.selectedEvent, #S.events)
    S.modified = true
    seekAll(S.time, false)
end

local function updatePlayback(delta)
    if not S.playing then return end
    local clip = clockClip()
    local clockEntry = actor(S.clockActor)
    if not clockEntry or clip.kind == 'none' or not clockEntry.object then
        S.playing = false
        return
    end
    local value
    if clip.kind == 'skeletal' then
        value = clockEntry.object:getSkeletalAnimationTime()
    elseif clip.kind == 'articulated' then
        value = clockEntry.object:getArticulatedAnimationTime(clip.name)
    end
    local previousTime = S.time
    S.time = value or (S.time + math.max(delta or 0, 0))
    if S.time >= clockDuration() - 0.0001 then
        S.time = clockDuration()
        S.playing = false
    end
    for _, event in ipairs(S.events) do
        local eventTime = event.time or 0
        if event.action == 'play' and eventTime > previousTime and eventTime <= S.time then
            applyEvent(event)
        end
    end
    resetOffsets()
    for _, event in ipairs(S.events) do
        if event.action == 'transform' then
            applyEvent(event, transformProgress(event, S.time))
        end
    end
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        local selected = entry.runtimeOverride and entry.runtimeClip or findClip(entry, entry.clipKey)
        local localTime = entry.runtimeOverride and (S.time - entry.runtimeStartTime) or S.time
        if selected.kind ~= 'none' and id ~= S.clockActor then seekClip(entry, selected, localTime, true) end
        applyTransform(entry)
    end
    if not S.playing then seekAll(S.time, false) end
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
                S.modified = true
                seekAll(S.time, false)
            end
            local base = entry.base
            tImGui.SetNextItemWidth(105)
            local c1, n1 = tImGui.InputFloat('X##base' .. id, base.x); if c1 then base.x = n1; applyTransform(entry); S.modified = true end
            tImGui.SetNextItemWidth(105)
            local c2, n2 = tImGui.InputFloat('Y##base' .. id, base.y); if c2 then base.y = n2; applyTransform(entry); S.modified = true end
            tImGui.SetNextItemWidth(105)
            local c3, n3 = tImGui.InputFloat('Z##base' .. id, base.z); if c3 then base.z = n3; applyTransform(entry); S.modified = true end
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
            if not addActor() then setStatus(L('actor_limit'), true) end
        end
        if #S.actorOrder == 0 then tImGui.TextWrapped(L('add_actor_hint')) end
        tImGui.Separator()
        for _, id in ipairs(S.actorOrder) do actorPanel(id) end
        if #S.actorOrder > 0 then
            tImGui.Separator()
            if tImGui.Button(L('set_base_reference')) then setBaseReference() end
            tImGui.TextWrapped(S.baseReference and L('base_reference_set') or L('base_reference_missing'))
            local choices, clockIndex = {}, 1
            for index, id in ipairs(S.actorOrder) do
                choices[index] = actorName(id, actor(id))
                if id == S.clockActor then clockIndex = index end
            end
            tImGui.SetNextItemWidth(210)
            local clockChanged, nextClock = tImGui.Combo(L('clock_actor'), clockIndex, choices)
            if clockChanged then
                setClockActor(S.actorOrder[nextClock])
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
            local base = selected.base
            tImGui.SetNextItemWidth(105)
            local changed, value = tImGui.InputFloat('POS X##iae_selected_pos_x', base.x)
            if changed then base.x = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('POS Y##iae_selected_pos_y', base.y)
            if changed then base.y = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('POS Z##iae_selected_pos_z', base.z)
            if changed then base.z = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('ROT X##iae_selected_rot_x', base.rx)
            if changed then base.rx = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('ROT Y##iae_selected_rot_y', base.ry)
            if changed then base.ry = value; applyTransform(selected); S.modified = true end
            tImGui.SetNextItemWidth(105)
            changed, value = tImGui.InputFloat('ROT Z##iae_selected_rot_z', base.rz)
            if changed then base.rz = value; applyTransform(selected); S.modified = true end
            tImGui.TextWrapped(S.transformMode == 'move' and L('move_gizmo_help') or L('rotate_gizmo_help'))
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
    tUtil.setInitialWindowPositionRight(S.windows.events, 0, 390, 330, 360, 360)
    local opened = tImGui.Begin(S.windows.events, true, 0)
    if opened then
        tImGui.Text(L('events'))
        if tImGui.Button(L('add_event')) then addEvent() end
        tImGui.SameLine()
        if tImGui.Button(L('remove_event')) then removeSelectedEvent() end
        for index, event in ipairs(S.events) do
            if tImGui.Selectable(eventLabel(event, index), index == S.selectedEvent) then
                S.selectedEvent = index
                S.playing = false
                local eventTime = event.time or 0
                if event.transition == 'smooth' then
                    local _, transitionEnd = transitionRange(event, clockDuration())
                    eventTime = transitionEnd
                end
                seekAll(eventTime, false)
            end
        end
        local event = S.events[S.selectedEvent]
        if event then
            tImGui.Separator()
            tImGui.SetNextItemWidth(105)
            local c, value = tImGui.InputFloat(L('event_time'), event.time or 0)
            if c then
                if event.transition == 'smooth' then
                    local start = select(1, transitionRange(event, clockDuration()))
                    local finish = clamp(value, start, clockDuration())
                    event.transitionStart = start
                    event.transitionEnd = finish
                    event.time = finish
                else
                    event.time = clamp(value, 0, clockDuration())
                end
                S.modified = true
                seekAll(S.time, false)
            end
            local targetIds, targetNames, targetIndex = activeActorIds(), {}, 1
            for index, id in ipairs(targetIds) do
                targetNames[index] = actorName(id, actor(id))
                if event.target == id then targetIndex = index end
            end
            if #targetIds > 0 then
                tImGui.SetNextItemWidth(210)
            local targetChanged, nextTarget = tImGui.Combo(L('event_target'), targetIndex, targetNames)
                if targetChanged then event.target = targetIds[nextTarget]; S.modified = true end
            end
            tImGui.SetNextItemWidth(210)
            local actionChanged, actionIndex = tImGui.Combo(L('event_action'), event.action == 'play' and 2 or 1, {L('transform'), L('play')})
            if actionChanged then event.action = actionIndex == 2 and 'play' or 'transform'; S.modified = true; seekAll(S.time, false) end
            if event.action == 'play' then
                local target = actor(event.target)
                local clips, names, current = clipList(target), {}, 1
                for index, clip in ipairs(clips) do names[index] = clip.label; if clip.key == event.clipKey then current = index end end
                tImGui.SetNextItemWidth(230)
                local clipChanged, clipIndex = tImGui.Combo(L('event_animation'), current, names)
                if clipChanged then event.clipKey = clips[clipIndex].key; S.modified = true; seekAll(S.time, false) end
            else
                tImGui.SetNextItemWidth(210)
                local transitionChanged, transitionIndex = tImGui.Combo(
                    L('transition'), event.transition == 'smooth' and 2 or 1,
                    {L('instant'), L('smooth')})
                if transitionChanged then
                    event.transition = transitionIndex == 2 and 'smooth' or 'instant'
                    if event.transition == 'smooth' then
                        local start = clamp(event.time or S.time, 0, clockDuration())
                        local finish = math.min(clockDuration(), start + 0.2)
                        event.transitionStart = start
                        event.transitionEnd = finish
                        event.time = finish
                        event.duration = nil
                    else
                        event.time = select(2, transitionRange(event, clockDuration()))
                        event.transitionStart = nil
                        event.transitionEnd = nil
                        event.duration = nil
                    end
                    S.modified = true
                    seekAll(S.time, false)
                end
                if event.transition == 'smooth' then
                    local start, finish = transitionRange(event, clockDuration())
                    tImGui.SetNextItemWidth(210)
                    local rangeChanged, range = tImGui.SliderFloat2(
                        L('transition_interval'), {start, finish}, 0, clockDuration())
                    if rangeChanged then
                        start = clamp(range[1], 0, clockDuration())
                        finish = clamp(range[2], start, clockDuration())
                        event.transitionStart = start
                        event.transitionEnd = finish
                        event.time = finish
                        event.duration = nil
                        S.modified = true
                        seekAll(S.time, false)
                    end
                end
                local fields = {{'x', L('offset_x')}, {'y', L('offset_y')}, {'z', L('offset_z')}, {'rx', L('offset_rx')}, {'ry', L('offset_ry')}, {'rz', L('offset_rz')}}
                for _, field in ipairs(fields) do
                    tImGui.SetNextItemWidth(105)
                    local changed, value = tImGui.InputFloat(field[2], event[field[1]] or 0)
                    if changed then event[field[1]] = value; S.modified = true; seekAll(S.time, false) end
                end
            end
        end
        tImGui.Separator()
        if tImGui.Button(L('capture_transform')) then captureTransformEvent() end
    end
    tImGui.End()
end

local function showTimeline()
    tUtil.setInitialWindowPositionDown(S.windows.timeline, 390, 0.12, 390)
    local opened = tImGui.Begin(S.windows.timeline, true, 0)
    if opened then
        local maxTime = clockDuration()
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
        if tImGui.MenuItem(L('export')) then exportSpec() end
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
    updateXZPlane()
    tUtil.showOverlayMessage()
end

function onKeyDown(key)
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
