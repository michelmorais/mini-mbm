--[[
--------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                       |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                        |
|                                                                                                                         |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated            |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation         |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and        |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                      |
|                                                                                                                         |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of        |
| the Software.                                                                                                           |
|                                                                                                                         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE    |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR   |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        |
|                                                                                                                         |
|-------------------------------------------------------------------------------------------------------------------------|

]]

return [[
local reaction = {}

local function multiply(a, b)
    local result = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
    for row = 1, 3 do
        for column = 1, 3 do
            result[row][column] = a[row][1] * b[1][column] +
                a[row][2] * b[2][column] + a[row][3] * b[3][column]
        end
    end
    return result
end

local function identity()
    return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}
end
local function isIdentity(matrix)
    return math.abs(matrix[1][1] - 1) < 0.000001 and
        math.abs(matrix[2][2] - 1) < 0.000001 and
        math.abs(matrix[3][3] - 1) < 0.000001 and
        math.abs(matrix[1][2]) < 0.000001 and math.abs(matrix[1][3]) < 0.000001 and
        math.abs(matrix[2][1]) < 0.000001 and math.abs(matrix[2][3]) < 0.000001 and
        math.abs(matrix[3][1]) < 0.000001 and math.abs(matrix[3][2]) < 0.000001
end

local function transpose(matrix)
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
        {complement * x * x + cosine, complement * x * y + sine * z,
            complement * x * z - sine * y},
        {complement * y * x - sine * z, complement * y * y + cosine,
            complement * y * z + sine * x},
        {complement * z * x + sine * y, complement * z * y - sine * x,
            complement * z * z + cosine},
    }
end

local function eulerRotation(rx, ry, rz)
    local xRotation = axisRotation({x = 1, y = 0, z = 0}, rx)
    local yRotation = axisRotation({x = 0, y = 1, z = 0}, ry)
    local zRotation = axisRotation({x = 0, y = 0, z = 1}, rz)
    return multiply(zRotation, multiply(yRotation, multiply(xRotation, identity())))
end

local function rotationToEuler(matrix)
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

local function clipParts(key)
    local kind, name = string.match(key or '', '^([^:]+):(.+)$')
    return kind, name
end

local function playClip(object, key)
    local kind, name = clipParts(key)
    if not object or not name then return false end
    if kind == 'skeletal' and type(object.playSkeletalAnimation) == 'function' then
        return object:playSkeletalAnimation(name) ~= false
    end
    if kind == 'articulated' and type(object.playArticulatedAnimation) == 'function' then
        return object:playArticulatedAnimation(name, 0, 0, 1) ~= false
    end
    return false
end

local function stopClip(object, key)
    local kind, name = clipParts(key)
    if not object then return end
    if kind == 'skeletal' and type(object.stopSkeletalAnimation) == 'function' then
        object:stopSkeletalAnimation()
    elseif kind == 'articulated' and name and type(object.disableArticulatedAnimation) == 'function' then
        object:disableArticulatedAnimation(name)
    end
end

local function pauseClip(object, key)
    local kind, name = clipParts(key)
    if not object then return end
    if kind == 'skeletal' and type(object.pauseSkeletalAnimation) == 'function' then
        object:pauseSkeletalAnimation()
    elseif kind == 'articulated' and name and type(object.pauseArticulatedAnimation) == 'function' then
        object:pauseArticulatedAnimation(name)
    end
end

local function readTransform(object)
    local position = object:getPos()
    local angle = object:getAngle()
    local scale = object:getScale()
    return {
        position = {x = position.x, y = position.y, z = position.z},
        angle = {x = angle.x, y = angle.y, z = angle.z},
        scale = {x = scale.x, y = scale.y, z = scale.z},
    }
end

local function setTransform(object, transform)
    object:setPos(transform.position.x, transform.position.y, transform.position.z)
    object:setAngle(transform.angle.x, transform.angle.y, transform.angle.z)
    object:setScale(transform.scale.x, transform.scale.y, transform.scale.z)
end

local function interactionAxes(anchorTransform)
    local matrix = eulerRotation(anchorTransform.angle.x, anchorTransform.angle.y,
        anchorTransform.angle.z)
    return {
        x = {x = matrix[1][1], y = matrix[1][2], z = matrix[1][3]},
        y = {x = matrix[2][1], y = matrix[2][2], z = matrix[2][3]},
        z = {x = matrix[3][1], y = matrix[3][2], z = matrix[3][3]},
    }
end

local function anchorRotation(axes, rx, ry, rz)
    local xRotation = axisRotation(axes.x, math.rad(rx or 0))
    local yRotation = axisRotation(axes.y, math.rad(ry or 0))
    local zRotation = axisRotation(axes.z, math.rad(rz or 0))
    return multiply(zRotation, multiply(yRotation, multiply(xRotation, identity())))
end

local function transformProgress(event, time)
    if event.transition ~= 'smooth' then return 1 end
    local transitionEnd = tonumber(event.transitionEnd) or tonumber(event.time) or 0
    local transitionStart = tonumber(event.transitionStart)
    if transitionStart == nil then
        transitionStart = transitionEnd - math.max(tonumber(event.duration) or 0.2, 0.0001)
    end
    transitionStart = math.max(0, transitionStart)
    transitionEnd = math.max(transitionStart, transitionEnd)
    if time < transitionStart then return 0 end
    if transitionEnd <= transitionStart then return time >= transitionEnd and 1 or 0 end
    local progress = math.max(0, math.min(1, (time - transitionStart) / (transitionEnd - transitionStart)))
    return progress * progress * (3 - 2 * progress)
end

local function eventMaximumTime(definition)
    local maximum = 0
    for _, event in ipairs(definition.events or {}) do
        maximum = math.max(maximum, tonumber(event.time) or 0)
    end
    return maximum
end

local function bindActors(definition, bindings)
    local actors = {}
    for id, actorDefinition in pairs(definition.actors or {}) do
        local name = actorDefinition.name or id
        local fixedName = id:gsub('^(actor)(%d+)$', '%1_%2')
        local object = bindings[name] or bindings[fixedName] or bindings[id]
        if not object then error('Missing interaction actor binding: ' .. tostring(name)) end
        actors[id] = {id = id, definition = actorDefinition, object = object, base = readTransform(object)}
    end
    return actors
end

local function applyTransforms(instance)
    local offsets = {}
    for id in pairs(instance.actors) do
        offsets[id] = {
            position = {x = 0, y = 0, z = 0},
            rotation = identity(),
            scale = {x = 1, y = 1, z = 1},
        }
    end
    for _, event in ipairs(instance.definition.events or {}) do
        if event.action == 'transform' and offsets[event.target] then
            local progress = transformProgress(event, instance.time)
            if progress > 0 then
                local offset = offsets[event.target]
                local x = (event.x or 0) * progress
                local y = (event.y or 0) * progress
                local z = (event.z or 0) * progress
                offset.position.x = offset.position.x + instance.axes.x.x * x +
                    instance.axes.y.x * y + instance.axes.z.x * z
                offset.position.y = offset.position.y + instance.axes.x.y * x +
                    instance.axes.y.y * y + instance.axes.z.y * z
                offset.position.z = offset.position.z + instance.axes.x.z * x +
                    instance.axes.y.z * y + instance.axes.z.z * z
                offset.rotation = multiply(offset.rotation, anchorRotation(instance.axes,
                    (event.rx or 0) * progress, (event.ry or 0) * progress,
                    (event.rz or 0) * progress))
                offset.scale.x = offset.scale.x * (1 + ((event.sx or 1) - 1) * progress)
                offset.scale.y = offset.scale.y * (1 + ((event.sy or 1) - 1) * progress)
                offset.scale.z = offset.scale.z * (1 + ((event.sz or 1) - 1) * progress)
            end
        end
    end
    for id, actor in pairs(instance.actors) do
        local base = actor.base
        local offset = offsets[id]
        local rx, ry, rz
        if isIdentity(offset.rotation) then
            rx, ry, rz = base.angle.x, base.angle.y, base.angle.z
        else
            local baseRotation = eulerRotation(base.angle.x, base.angle.y, base.angle.z)
            rx, ry, rz = rotationToEuler(multiply(baseRotation, offset.rotation))
        end
        actor.object:setPos(base.position.x + offset.position.x,
            base.position.y + offset.position.y, base.position.z + offset.position.z)
        actor.object:setAngle(rx, ry, rz)
        actor.object:setScale(base.scale.x * offset.scale.x,
            base.scale.y * offset.scale.y, base.scale.z * offset.scale.z)
    end
end

local function applyPlayEvents(instance, previousTime, currentTime, includeInitial)
    for index, event in ipairs(instance.definition.events or {}) do
        if event.action == 'play' and not instance.fired[index] then
            local eventTime = event.time or 0
            local reached = includeInitial and eventTime <= currentTime or
                eventTime > previousTime and eventTime <= currentTime
            if reached then
                local actor = instance.actors[event.target]
                if actor then
                    playClip(actor.object, event.clipKey)
                    instance.activeClips[event.target] = event.clipKey
                end
                instance.fired[index] = true
            end
        end
    end
end

local function resetInstance(instance)
    for id, actor in pairs(instance.actors) do
        stopClip(actor.object, instance.activeClips[id] or actor.definition.clipKey)
        setTransform(actor.object, actor.base)
    end
    instance.time = 0
    instance.fired = {}
    instance.activeClips = {}
    instance.finished = false
    instance.running = true
    for id, actor in pairs(instance.actors) do
        local key = actor.definition.clipKey
        if id == instance.definition.clockActor then key = instance.definition.clockKey end
        if key and key ~= '' then
            playClip(actor.object, key)
            instance.activeClips[id] = key
        end
    end
    applyPlayEvents(instance, -0.0001, 0, true)
    applyTransforms(instance)
end

function reaction.start(bindings)
    bindings = bindings or {}
    local instance = {
        definition = definition,
        actors = bindActors(definition, bindings),
        time = 0,
        duration = tonumber(definition.duration) or eventMaximumTime(definition),
        continueClipsAfterTimeline = definition.continueClipsAfterTimeline == true,
        fired = {},
        activeClips = {},
        finished = false,
        running = true,
    }
    if instance.duration <= 0 then instance.duration = math.huge end
    local anchor = instance.actors[definition.anchorActor]
    if not anchor then error('Missing interaction anchor actor') end
    instance.axes = interactionAxes(anchor.base)
    function instance:update(delta)
        if not self.running or self.finished then return false end
        local previousTime = self.time
        local amount = math.max(0, tonumber(delta) or 0)
        self.time = math.min(self.duration, previousTime + amount)
        applyPlayEvents(self, previousTime, self.time, false)
        applyTransforms(self)
        if self.time >= self.duration then
            if not self.continueClipsAfterTimeline then
                for id, actor in pairs(self.actors) do
                    pauseClip(actor.object, self.activeClips[id] or actor.definition.clipKey)
                end
            end
            self.finished = true
            self.running = false
        end
        return not self.finished
    end
    function instance:restart()
        resetInstance(self)
        return self
    end
    function instance:stop()
        for id, actor in pairs(self.actors) do
            stopClip(actor.object, self.activeClips[id] or actor.definition.clipKey)
            setTransform(actor.object, actor.base)
        end
        self.activeClips = {}
        self.running = false
        self.finished = true
        return self
    end
    function instance:isFinished()
        return self.finished
    end
    resetInstance(instance)
    return instance
end

function reaction._definition()
    return definition
end

return reaction
]]
