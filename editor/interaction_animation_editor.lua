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

   First prototype for synchronizing two mesh instances and authoring relative
   interaction events. The exported Lua file is a data specification; the runtime
   player will be added in a later milestone.
]]--

tImGui = require 'ImGui'
tUtil = require 'editor_utils'
local Animation = require 'mesh_to_sprite_animation'

local S = {
    actors = {
        rat = {
            label = 'rat',
            path = '/home/michel/Ratworks/assets/meshes/Cyclopsv2.msh',
            base = {x = 180, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1},
            bundle = nil,
            object = nil,
            clipKey = '',
            runtimeKey = nil,
            runtimeClip = nil,
            runtimeOverride = false,
            runtimeStartTime = 0,
            offset = {x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0},
        },
        machine = {
            label = 'machine',
            path = '/home/michel/Ratworks/assets/meshes/ChompBot.msh',
            base = {x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1},
            bundle = nil,
            object = nil,
            clipKey = '',
            runtimeKey = nil,
            runtimeClip = nil,
            runtimeOverride = false,
            runtimeStartTime = 0,
            offset = {x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0},
        },
    },
    actorOrder = {'rat', 'machine'},
    clockActor = 'machine',
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
    transformMode = 'move',
    gizmo = {axes = {}, origin = nil, length = nil, drag = nil},
    originLines = {x = nil, y = nil, z = nil},
    showOrigins = true,
    mouse = {left = false, right = false, x = 0, y = 0},
    cameraDrag = nil,
    windows = {
        main = 'iae_main',
        actors = 'iae_actors',
        events = 'iae_events',
        timeline = 'iae_timeline',
        camera = 'iae_camera',
    },
}

local function L(key)
    return tLang.L('iae_' .. key)
end

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then
        S.status = tostring(result[2])
        print('[interaction_animation_editor] ' .. S.status)
        tUtil.showMessageWarn(S.status, 6)
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

local function actor(id)
    return S.actors[id]
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

local function axisVector(name)
    if name == 'x' then return 1, 0, 0 end
    if name == 'y' then return 0, 1, 0 end
    return 0, 0, 1
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
    return {
        x = entry.base.x + entry.offset.x,
        y = entry.base.y + entry.offset.y,
        z = entry.base.z + entry.offset.z,
    }
end

local function actorPickRadius(entry)
    if not entry.object then return 40 end
    local ok, width, height, depth = pcall(function() return entry.object:getSize(true) end)
    if not ok then return 40 end
    local extent = math.max(width or 0, height or 0, depth or 0)
    return math.max(extent * 0.45, 20)
end

local function hideGizmo()
    for _, object in pairs(S.gizmo.axes) do object.visible = false end
    S.gizmo.origin = nil
    S.gizmo.length = nil
end

local function updateGizmo()
    local entry = S.selectedActor and actor(S.selectedActor) or nil
    if not entry or not entry.object then
        hideGizmo()
        return
    end
    local origin = actorPosition(entry)
    local length = math.max(actorPickRadius(entry) * 1.4, S.cameraOrbit.distance * 0.06)
    local previousOrigin, previousLength = S.gizmo.origin, S.gizmo.length
    local originChanged = not previousOrigin or previousOrigin.x ~= origin.x or
        previousOrigin.y ~= origin.y or previousOrigin.z ~= origin.z
    local lengthChanged = not previousLength or math.abs(previousLength - length) > 1e-6
    local definitions = {
        x = {x = 1, y = 0, z = 0, color = {1, 0.15, 0.15, 1}},
        y = {x = 0, y = 1, z = 0, color = {0.15, 1, 0.2, 1}},
        z = {x = 0, y = 0, z = 1, color = {0.2, 0.45, 1, 1}},
    }
    for name, definition in pairs(definitions) do
        local object = S.gizmo.axes[name]
        local points = {0, 0, 0, definition.x * length, definition.y * length, definition.z * length}
        if object then
            if lengthChanged then object:set(points, 1) end
        else
            object = line:new('3d', 0, 0, 0)
            object:add(points)
            object.alwaysOnTop = true
            S.gizmo.axes[name] = object
        end
        if originChanged then object:setPos(origin.x, origin.y, origin.z) end
        object:setColor(table.unpack(definition.color))
        object.visible = true
    end
    S.gizmo.origin = origin
    S.gizmo.length = length
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
    if not S.gizmo.origin or not S.gizmo.length then return nil end
    local bestName, bestDistance
    local threshold = math.max(S.gizmo.length * 0.12, 12)
    for name, axis in pairs({x = {x = 1, y = 0, z = 0}, y = {x = 0, y = 1, z = 0}, z = {x = 0, y = 0, z = 1}}) do
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

local function setOriginsVisible(visible)
    S.showOrigins = visible == true
    for _, object in pairs(S.originLines) do
        if object then object.visible = S.showOrigins end
    end
end

local function beginViewportInteraction(key, x, y)
    if tImGui.GetWantCaptureMouse() then return end
    if key == 0 then
        local axisName = S.selectedActor and pickGizmoAxis(x, y) or nil
        local entry = S.selectedActor and actor(S.selectedActor) or nil
        if axisName and entry and S.gizmo.origin then
            local ax, ay, az = axisVector(axisName)
            local parameter = rayAxisParameter(x, y, S.gizmo.origin, {x = ax, y = ay, z = az})
            if parameter then
                if S.playing then
                    S.playing = false
                end
                S.gizmo.drag = {
                    actor = S.selectedActor,
                    axisName = axisName,
                    axis = {x = ax, y = ay, z = az},
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
        if S.transformMode == 'move' then
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
            local amount = (x - drag.startX) * 0.5
            entry.base.rx = drag.base.rx + (drag.axisName == 'x' and amount or 0)
            entry.base.ry = drag.base.ry + (drag.axisName == 'y' and amount or 0)
            entry.base.rz = drag.base.rz + (drag.axisName == 'z' and amount or 0)
            applyTransform(entry)
            drag.moved = true
            S.modified = true
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
        S.cameraOrbit.fy = S.cameraOrbit.fy - dy * scale
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
        S.originLines[name] = object
    end
    setOriginsVisible(S.showOrigins)
end

local function clipList(entry)
    local list = {{key = '', label = L('no_animation'), kind = 'none', name = '', duration = 0}}
    if not entry.bundle then return list end
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
    entry.object:setAngle(
        math.rad(base.rx + offset.rx),
        math.rad(base.ry + offset.ry),
        math.rad(base.rz + offset.rz))
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
    return findClip(actor(S.clockActor), S.clockKey)
end

local function clockDuration()
    local clip = clockClip()
    return math.max(0.01, clip.duration or 0)
end

local function applyEvent(event)
    local entry = actor(event.target)
    if not entry then return end
    if event.action == 'transform' then
        entry.offset.x = entry.offset.x + (event.x or 0)
        entry.offset.y = entry.offset.y + (event.y or 0)
        entry.offset.z = entry.offset.z + (event.z or 0)
        entry.offset.rx = entry.offset.rx + (event.rx or 0)
        entry.offset.ry = entry.offset.ry + (event.ry or 0)
        entry.offset.rz = entry.offset.rz + (event.rz or 0)
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
        if (event.time or 0) <= S.time then applyEvent(event) end
    end
    S.lastAppliedTime = S.time
end

local function loadActor(id, path)
    if not path or path == '' then return false end
    local entry = actor(id)
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
    S.modified = true
    S.status = string.format(L('loaded'), id, path)
    return true
end

local function loadDefaults()
    local loadedRat = io.open(actor('rat').path, 'rb')
    if loadedRat then loadedRat:close(); loadActor('rat', actor('rat').path) end
    local loadedMachine = io.open(actor('machine').path, 'rb')
    if loadedMachine then loadedMachine:close(); loadActor('machine', actor('machine').path) end
    local machine = actor('machine')
    for _, clip in ipairs(clipList(machine)) do
        if clip.name == 'bite' then machine.clipKey = clip.key; S.clockKey = clip.key; break end
    end
    local rat = actor('rat')
    for _, clip in ipairs(clipList(rat)) do
        if clip.name == 'Eaten' then rat.clipKey = clip.key; break end
    end
    seekAll(0, false)
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
        S.status = L('clock_missing')
        return
    end
    seekAll(S.time, true)
    S.playing = true
end

local function pausePlayback()
    S.playing = false
    seekAll(S.time, false)
end

local function serializeProject()
    local project = {
        version = 1,
        actors = {},
        actorOrder = cloneTable(S.actorOrder),
        clockActor = S.clockActor,
        clockKey = S.clockKey,
        time = S.time,
        events = cloneTable(S.events),
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

local function applyProject(project)
    if type(project) ~= 'table' then error(L('invalid_project')) end
    S.playing = false
    S.events = cloneTable(project.events or {})
    S.clockActor = project.clockActor == 'rat' and 'rat' or 'machine'
    S.clockKey = project.clockKey or ''
    S.time = tonumber(project.time) or 0
    for _, id in ipairs(S.actorOrder) do
        local saved = project.actors and project.actors[id]
        if saved then
            actor(id).base = cloneTable(saved.base or actor(id).base)
            actor(id).clipKey = saved.clipKey or ''
            if saved.path and saved.path ~= '' then loadActor(id, saved.path) end
        end
    end
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
        S.status = L('save_failed')
        return false
    end
    for _, line in ipairs(output) do file:write(line .. '\n') end
    file:close()
    return true
end

local function saveProject(path)
    path = path or mbm.saveFile(S.projectPath or '', 'iae.lua')
    if not path then return end
    if saveLua(path, 'interactionProject', serializeProject(), false) then
        S.projectPath = path
        S.modified = false
        S.status = string.format(L('saved'), path)
    end
end

local function loadProject(path)
    path = path or mbm.openFile(S.projectPath or '', 'iae.lua')
    if not path then return end
    local fn, err = loadfile(path)
    if not fn then
        S.status = tostring(err)
        return
    end
    interactionProject = nil
    local ok = dpCall(fn)
    if not ok or type(interactionProject) ~= 'table' then
        S.status = L('invalid_project')
        return
    end
    local applied = dpCall(applyProject, interactionProject)
    if applied then
        S.projectPath = path
        S.status = string.format(L('loaded_project'), path)
    end
end

local function exportSpec(path)
    path = path or mbm.saveFile(S.projectPath or '', 'lua')
    if not path then return end
    if saveLua(path, 'interaction', serializeProject(), true) then
        S.status = string.format(L('exported'), path)
    end
end

local function eventLabel(event, index)
    local target = event.target or 'actor'
    local action = event.action == 'play' and L('play_animation') or L('transform')
    return string.format('%d: %.3fs %s %s', index, event.time or 0, target, action)
end

local function addEvent()
    table.insert(S.events, {
        time = S.time,
        target = S.clockActor,
        action = 'transform',
        x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0,
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
    local value
    if clip.kind == 'skeletal' then
        value = actor(S.clockActor).object:getSkeletalAnimationTime()
    elseif clip.kind == 'articulated' then
        value = actor(S.clockActor).object:getArticulatedAnimationTime(clip.name)
    end
    local previousTime = S.time
    S.time = value or (S.time + math.max(delta or 0, 0))
    if S.time >= clockDuration() - 0.0001 then
        S.time = clockDuration()
        S.playing = false
    end
    for _, event in ipairs(S.events) do
        local eventTime = event.time or 0
        if eventTime > previousTime and eventTime <= S.time then applyEvent(event) end
    end
    resetOffsets()
    for _, event in ipairs(S.events) do
        if (event.time or 0) <= S.time and event.action == 'transform' then applyEvent(event) end
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
    if tImGui.TreeNode(entry.label .. '##' .. id) then
        local changed, path = tImGui.InputText(L('path') .. '##' .. id, entry.path or '', 512)
        if changed then entry.path = path; S.modified = true end
        if tImGui.Button(L('load') .. '##' .. id) then loadActor(id, entry.path); seekAll(S.time, false) end
        tImGui.SameLine()
        if tImGui.Button(L('browse') .. '##' .. id) then openActorFile(id) end
        if entry.bundle then
            local clips = clipList(entry)
            local names, current = {}, 1
            for index, clip in ipairs(clips) do
                names[index] = clip.label
                if clip.key == entry.clipKey then current = index end
            end
            local clipChanged, clipIndex = tImGui.Combo(L('animation') .. '##' .. id, current, names)
            if clipChanged then
                entry.clipKey = clips[clipIndex].key
                if id == S.clockActor then S.clockKey = entry.clipKey end
                S.modified = true
                seekAll(S.time, false)
            end
            local base = entry.base
            local c1, n1 = tImGui.InputFloat('X##base' .. id, base.x); if c1 then base.x = n1; applyTransform(entry); S.modified = true end
            local c2, n2 = tImGui.InputFloat('Y##base' .. id, base.y); if c2 then base.y = n2; applyTransform(entry); S.modified = true end
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
        actorPanel('rat')
        actorPanel('machine')
        tImGui.Separator()
        local choices = {L('rat'), L('machine')}
        local clockIndex = S.clockActor == 'rat' and 1 or 2
        local clockChanged, nextClock = tImGui.Combo(L('clock_actor'), clockIndex, choices)
        if clockChanged then
            S.clockActor = nextClock == 1 and 'rat' or 'machine'
            S.clockKey = actor(S.clockActor).clipKey
            S.modified = true
            seekAll(S.time, false)
        end
        local clockNames, clockCurrent = {}, 1
        for index, clip in ipairs(clipList(actor(S.clockActor))) do
            clockNames[index] = clip.label
            if clip.key == S.clockKey then clockCurrent = index end
        end
        local clockChanged2, clockIndex2 = tImGui.Combo(L('clock_animation'), clockCurrent, clockNames)
        if clockChanged2 then
            S.clockKey = clipList(actor(S.clockActor))[clockIndex2].key
            actor(S.clockActor).clipKey = S.clockKey
            S.time = 0
            S.modified = true
            seekAll(0, false)
        end
        tImGui.Text(string.format(L('clock_duration'), clockDuration()))
        if S.status ~= '' then tImGui.TextWrapped(S.status) end
    end
    tImGui.End()
end

local function showCameraPanel()
    tUtil.setInitialWindowPositionRight(S.windows.camera, 0, 20, 300, 330, 360)
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
        local modeChanged, modeIndex = tImGui.Combo(L('transform_mode'), S.transformMode == 'rotate' and 2 or 1,
            {L('move'), L('rotate')})
        if modeChanged then
            S.transformMode = modeIndex == 2 and 'rotate' or 'move'
            S.gizmo.drag = nil
        end
        if S.selectedActor then
            tImGui.Text(string.format('%s: %s', L('selected_actor'), actor(S.selectedActor).label))
        else
            tImGui.Text(L('select_actor_hint'))
        end
        tImGui.TextWrapped(S.transformMode == 'move' and L('move_gizmo_help') or L('rotate_gizmo_help'))
    end
    tImGui.End()
end

local function showEvents()
    tUtil.setInitialWindowPositionRight(S.windows.events, 0, 390, 390)
    local opened = tImGui.Begin(S.windows.events, true, 0)
    if opened then
        tImGui.Text(L('events'))
        if tImGui.Button(L('add_event')) then addEvent() end
        tImGui.SameLine()
        if tImGui.Button(L('remove_event')) then removeSelectedEvent() end
        for index, event in ipairs(S.events) do
            if tImGui.Selectable(eventLabel(event, index), index == S.selectedEvent) then
                S.selectedEvent = index
            end
        end
        local event = S.events[S.selectedEvent]
        if event then
            tImGui.Separator()
            local c, value = tImGui.InputFloat(L('event_time'), event.time or 0)
            if c then event.time = math.max(0, value); S.modified = true; seekAll(S.time, false) end
            local targetChanged, targetIndex = tImGui.Combo(L('event_target'), event.target == 'rat' and 1 or 2, {L('rat'), L('machine')})
            if targetChanged then event.target = targetIndex == 1 and 'rat' or 'machine'; S.modified = true end
            local actionChanged, actionIndex = tImGui.Combo(L('event_action'), event.action == 'play' and 2 or 1, {L('transform'), L('play')})
            if actionChanged then event.action = actionIndex == 2 and 'play' or 'transform'; S.modified = true; seekAll(S.time, false) end
            if event.action == 'play' then
                local target = actor(event.target)
                local clips, names, current = clipList(target), {}, 1
                for index, clip in ipairs(clips) do names[index] = clip.label; if clip.key == event.clipKey then current = index end end
                local clipChanged, clipIndex = tImGui.Combo(L('event_animation'), current, names)
                if clipChanged then event.clipKey = clips[clipIndex].key; S.modified = true; seekAll(S.time, false) end
            else
                local fields = {{'x', L('offset_x')}, {'y', L('offset_y')}, {'z', L('offset_z')}, {'rx', L('offset_rx')}, {'ry', L('offset_ry')}, {'rz', L('offset_rz')}}
                for _, field in ipairs(fields) do
                    local changed, value = tImGui.InputFloat(field[2], event[field[1]] or 0)
                    if changed then event[field[1]] = value; S.modified = true; seekAll(S.time, false) end
                end
            end
        end
    end
    tImGui.End()
end

local function showTimeline()
    tUtil.setInitialWindowPositionDown(S.windows.timeline, 390, 0.84, 390)
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
        if tImGui.MenuItem(L('english')) then tLang.setLanguage('en') end
        if tImGui.MenuItem(L('portuguese')) then tLang.setLanguage('pt_br') end
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

function onInitScene()
    S.camera = mbm.getCamera('3d')
    S.camera:setFar(9999999)
    applyCamera()
    createOriginLines()
    tUtil.sMessageOverlay = L('welcome')
    tUtil.bRightSide = true
    tUtil.tTimerOverlay:start()
    loadDefaults()
end

function onLoop(delta)
    updatePlayback(delta)
    showMainMenu()
    showActors()
    showCameraPanel()
    showEvents()
    showTimeline()
    updateGizmo()
    tUtil.showOverlayMessage()
end

function onKeyDown(key)
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
    for _, object in pairs(S.originLines) do
        if object then object:destroy() end
    end
    for _, object in pairs(S.gizmo.axes) do
        if object then object:destroy() end
    end
    for _, id in ipairs(S.actorOrder) do
        local entry = actor(id)
        if entry.object then entry.object:destroy(); entry.object = nil end
    end
end
