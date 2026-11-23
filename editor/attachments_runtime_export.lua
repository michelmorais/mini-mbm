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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Attachments runtime export helper.
]]--

local M = {}

local function finite(value)
    return type(value) == 'number' and value == value and value ~= math.huge and value ~= -math.huge
end

local function number(value, fallback)
    return finite(value) and value or fallback
end

local function quote(value)
    return string.format('%q', tostring(value or ''))
end

local function validateVector(value, fallback)
    if type(value) ~= 'table' then return false end
    for _, axis in ipairs({'x', 'y', 'z'}) do
        if not finite(value[axis]) then value[axis] = fallback end
    end
    return true
end

local function validateRotationMatrix(value)
    if type(value) ~= 'table' then return false end
    for row = 1, 3 do
        if type(value[row]) ~= 'table' then return false end
        for column = 1, 3 do
            if not finite(value[row][column]) then return false end
        end
    end
    return true
end

local function validateTarget(target, sourceType)
    if type(target) ~= 'table' then return false, 'targets must contain tables' end
    if type(target.name) ~= 'string' or target.name == '' then
        return false, 'target binding name must not be empty'
    end
    if type(target.inherit) ~= 'table' then return false, 'target inherit must be a table' end
    local offset = target.offset
    if type(offset) ~= 'table' then return false, 'target offset must be a table' end
    for _, component in ipairs({'position', 'rotation', 'scale'}) do
        if not validateVector(offset[component], component == 'scale' and 1 or 0) then
            return false, 'target offset.' .. component .. ' must be a vector'
        end
    end
    if offset.rotationMatrix ~= nil and not validateRotationMatrix(offset.rotationMatrix) then
        return false, 'target offset.rotationMatrix must be a 3x3 matrix'
    end
    if target.targetPivot ~= nil and not validateVector(target.targetPivot, 0) then
        return false, 'target targetPivot must be a vector'
    end
    if target.aimPoint ~= nil and not validateVector(target.aimPoint, 0) then
        return false, 'target aimPoint must be a vector'
    end
    if target.aimLimits ~= nil then
        if type(target.aimLimits) ~= 'table' then return false, 'target aimLimits must be a table' end
        local limits = target.aimLimits
        if limits.enabled == true and target.aimAtSource ~= true then
            return false, 'target aimLimits requires aimAtSource=true'
        end
        local reference = limits.reference or 'dynamic'
        if reference ~= 'dynamic' and reference ~= 'captured' then
            return false, "target aimLimits.reference must be 'dynamic' or 'captured'"
        end
        if reference == 'captured' and not validateRotationMatrix(limits.referenceRotationMatrix) then
            return false, 'target captured aim limits require a reference rotation matrix'
        end
        if number(limits.yawMin, -math.pi) > number(limits.yawMax, math.pi) or
                number(limits.pitchMin, -math.pi * 0.5) > number(limits.pitchMax, math.pi * 0.5) then
            return false, 'target aim limits minimum values must not exceed maximum values'
        end
    end
    if target.spring ~= nil then
        if type(target.spring) ~= 'table' then return false, 'target spring must be a table' end
        if number(target.spring.stiffness, 120) < 0 or number(target.spring.damping, 24) < 0 then
            return false, 'target spring stiffness and damping must be non-negative'
        end
    end
    if target.blend ~= nil then
        if type(target.blend) ~= 'table' then return false, 'target blend must be a table' end
        local weight = number(target.blend.weight, 1)
        if weight < 0 or weight > 1 then return false, 'target blend weight must be between 0 and 1' end
    end
    return true
end

function M.validate(definition)
    if type(definition) ~= 'table' then return false, 'attachment definition must be a table' end
    if type(definition.source) ~= 'string' or definition.source == '' then
        return false, 'source binding name must not be empty'
    end
    if definition.sourceType ~= 'root' and definition.sourceType ~= 'bone' and
            definition.sourceType ~= 'socket' then
        return false, "sourceType must be 'root', 'bone' or 'socket'"
    end
    if definition.sourceType == 'bone' and
            (type(definition.sourceBone) ~= 'string' or definition.sourceBone == '') then
        return false, "sourceBone must not be empty when sourceType is 'bone'"
    end
    if definition.sourceType == 'socket' and
            (type(definition.sourceSocket) ~= 'string' or definition.sourceSocket == '') then
        return false, "sourceSocket must not be empty when sourceType is 'socket'"
    end
    if type(definition.targets) ~= 'table' or #definition.targets == 0 then
        return false, 'at least one target is required'
    end
    local names = {}
    for index, target in ipairs(definition.targets) do
        local valid, errorMessage = validateTarget(target, definition.sourceType)
        if not valid then return false, string.format('target %d: %s', index, errorMessage) end
        if target.name == definition.source then return false, 'source and target binding names must be different' end
        if names[target.name] then return false, 'target binding names must be unique' end
        names[target.name] = true
    end
    local visiting, visited = {}, {}
    local function visitTarget(name)
        if name == definition.source then return true end
        if visited[name] then return true end
        if visiting[name] then return false, 'attachment target chain contains a cycle' end
        local index
        for targetIndex, target in ipairs(definition.targets) do
            if target.name == name then index = targetIndex; break end
        end
        if not index then return false, 'attachment target source does not exist: ' .. tostring(name) end
        visiting[name] = true
        local sourceName = definition.targets[index].source or definition.source
        local valid, errorMessage = visitTarget(sourceName)
        visiting[name] = nil
        if not valid then return false, errorMessage end
        visited[name] = true
        return true
    end
    for _, target in ipairs(definition.targets) do
        local valid, errorMessage = visitTarget(target.name)
        if not valid then return false, errorMessage end
    end
    if definition.states ~= nil then
        if type(definition.states) ~= 'table' then return false, 'states must be a table' end
        for name, state in pairs(definition.states) do
            if type(name) ~= 'string' or name == '' or type(state) ~= 'table' then
                return false, 'states must map names to tables'
            end
            if state.blend ~= nil then
                if type(state.blend) ~= 'table' then return false, 'state blend must be a table' end
                local weight = number(state.blend.weight, 1)
                if weight < 0 or weight > 1 then
                    return false, 'state blend weight must be between 0 and 1'
                end
            end
        end
        if definition.initialState ~= nil and definition.initialState ~= '' and
                definition.states[definition.initialState] == nil then
            return false, 'initialState does not name a configured state'
        end
    elseif definition.initialState ~= nil and definition.initialState ~= '' then
        return false, 'initialState requires states'
    end
    return true
end

local function appendDefinition(output, definition)
    local function addVector(name, value, indent)
        output[#output + 1] = string.format('%s%s = {x = %.17g, y = %.17g, z = %.17g},',
            indent, name, number(value.x, 0), number(value.y, 0),
            number(value.z, name == 'scale' and 1 or 0))
    end
    local function addMatrix(name, value, indent)
        if not validateRotationMatrix(value) then return end
        output[#output + 1] = indent .. name .. ' = {'
        for row = 1, 3 do
            output[#output + 1] = string.format('%s    {%.17g, %.17g, %.17g},', indent,
                value[row][1], value[row][2], value[row][3])
        end
        output[#output + 1] = indent .. '},'
    end
    local function addBlend(blend, indent)
        if type(blend) ~= 'table' then return end
        output[#output + 1] = indent .. 'blend = {'
        output[#output + 1] = indent .. '    enabled = ' .. tostring(blend.enabled == true) .. ','
        output[#output + 1] = indent .. '    position = ' .. tostring(blend.position ~= false) .. ','
        output[#output + 1] = indent .. '    rotation = ' .. tostring(blend.rotation ~= false) .. ','
        output[#output + 1] = indent .. '    scale = ' .. tostring(blend.scale ~= false) .. ','
        output[#output + 1] = string.format('%s    weight = %.17g,', indent, number(blend.weight, 1))
        output[#output + 1] = indent .. '},'
    end
    local function addTarget(target, index)
        local prefix = '        '
        local inherit = target.inherit
        local offset = target.offset
        output[#output + 1] = '        [' .. index .. '] = {'
        output[#output + 1] = prefix .. 'name = ' .. quote(target.name) .. ','
        output[#output + 1] = prefix .. 'source = ' .. quote(target.source or definition.source) .. ','
        output[#output + 1] = prefix .. 'inherit = {'
        output[#output + 1] = prefix .. '    position = ' .. tostring(inherit.position == true) .. ','
        output[#output + 1] = prefix .. '    rotation = ' .. tostring(inherit.rotation == true) .. ','
        output[#output + 1] = prefix .. '    scale = ' .. tostring(inherit.scale == true) .. ','
        output[#output + 1] = prefix .. '},'
        output[#output + 1] = prefix .. 'aimAtSource = ' .. tostring(target.aimAtSource == true) .. ','
        local aimPoint = target.aimPoint or {x = 0, y = 0, z = 0}
        output[#output + 1] = string.format('%saimPoint = {x = %.17g, y = %.17g, z = %.17g},',
            prefix, number(aimPoint.x, 0), number(aimPoint.y, 0), number(aimPoint.z, 0))
        local aimLimits = target.aimLimits
        if type(aimLimits) == 'table' then
            output[#output + 1] = prefix .. 'aimLimits = {'
            output[#output + 1] = prefix .. '    enabled = ' .. tostring(aimLimits.enabled == true) .. ','
            output[#output + 1] = prefix .. '    reference = ' .. quote(aimLimits.reference or 'dynamic') .. ','
            addMatrix('referenceRotationMatrix', aimLimits.referenceRotationMatrix, prefix .. '    ')
            output[#output + 1] = string.format('%s    yawMin = %.17g,', prefix, number(aimLimits.yawMin, -math.pi))
            output[#output + 1] = string.format('%s    yawMax = %.17g,', prefix, number(aimLimits.yawMax, math.pi))
            output[#output + 1] = string.format('%s    pitchMin = %.17g,', prefix, number(aimLimits.pitchMin, -math.pi * 0.5))
            output[#output + 1] = string.format('%s    pitchMax = %.17g,', prefix, number(aimLimits.pitchMax, math.pi * 0.5))
            output[#output + 1] = prefix .. '},'
        end
        local spring = target.spring
        if type(spring) == 'table' then
            output[#output + 1] = prefix .. 'spring = {'
            output[#output + 1] = prefix .. '    enabled = ' .. tostring(spring.enabled == true) .. ','
            output[#output + 1] = prefix .. '    position = ' .. tostring(spring.position ~= false) .. ','
            output[#output + 1] = prefix .. '    rotation = ' .. tostring(spring.rotation ~= false) .. ','
            output[#output + 1] = string.format('%s    stiffness = %.17g,', prefix, number(spring.stiffness, 120))
            output[#output + 1] = string.format('%s    damping = %.17g,', prefix, number(spring.damping, 24))
            output[#output + 1] = prefix .. '},'
        end
        addBlend(target.blend, prefix)
        output[#output + 1] = prefix .. 'offset = {'
        addVector('position', offset.position, prefix .. '    ')
        addVector('rotation', offset.rotation, prefix .. '    ')
        addVector('scale', offset.scale, prefix .. '    ')
        addMatrix('rotationMatrix', offset.rotationMatrix, prefix .. '    ')
        output[#output + 1] = prefix .. '},'
        local targetPivot = target.targetPivot or {x = 0, y = 0, z = 0}
        output[#output + 1] = string.format('%stargetPivot = {x = %.17g, y = %.17g, z = %.17g},',
            prefix, number(targetPivot.x, 0), number(targetPivot.y, 0), number(targetPivot.z, 0))
        output[#output + 1] = '        },'
    end
    output[#output + 1] = 'local definition = {'
    output[#output + 1] = '    source = ' .. quote(definition.source) .. ','
    output[#output + 1] = '    sourceType = ' .. quote(definition.sourceType) .. ','
    if definition.sourceType == 'bone' then
        output[#output + 1] = '    sourceBone = ' .. quote(definition.sourceBone) .. ','
    elseif definition.sourceType == 'socket' then
        output[#output + 1] = '    sourceSocket = ' .. quote(definition.sourceSocket) .. ','
    end
    output[#output + 1] = '    influenceEnabled = ' .. tostring(definition.influenceEnabled ~= false) .. ','
    if type(definition.initialState) == 'string' and definition.initialState ~= '' then
        output[#output + 1] = '    initialState = ' .. quote(definition.initialState) .. ','
    end
    local states = definition.states
    if type(states) == 'table' and next(states) ~= nil then
        output[#output + 1] = '    states = {'
        for name, state in pairs(states) do
            output[#output + 1] = '        [' .. quote(name) .. '] = {'
            output[#output + 1] = '            influenceEnabled = ' .. tostring(state.influenceEnabled ~= false) .. ','
            addBlend(state.blend, '            ')
            output[#output + 1] = '        },'
        end
        output[#output + 1] = '    },'
    end
    output[#output + 1] = '    targets = {'
    for index, target in ipairs(definition.targets) do addTarget(target, index) end
    output[#output + 1] = '    },'
    output[#output + 1] = '}'
end

function M.build(definition)
    local valid, errorMessage = M.validate(definition)
    if not valid then return nil, errorMessage end
    local output = {
        '-- Generated by Attachments Editor. Runtime actor bindings are supplied by the game.',
        'local attachments = require "attachments"',
        '',
    }
    appendDefinition(output, definition)
    output[#output + 1] = ''
    output[#output + 1] = 'local function attach(bindings)'
    output[#output + 1] = '    if type(bindings) ~= "table" then'
    output[#output + 1] = '        return nil, "bindings must be a table"'
    output[#output + 1] = '    end'
    output[#output + 1] = '    local source = bindings[definition.source]'
    output[#output + 1] = '    if not source then return nil, "missing source binding: " .. definition.source end'
    output[#output + 1] = '    local relations = {}'
    output[#output + 1] = '    for index, targetDefinition in ipairs(definition.targets) do'
    output[#output + 1] = '        local target = bindings[targetDefinition.name]'
    output[#output + 1] = '        local sourceName = targetDefinition.source or definition.source'
    output[#output + 1] = '        local relationSource = bindings[sourceName]'
    output[#output + 1] = '        if not target then return nil, "missing target binding: " .. targetDefinition.name end'
    output[#output + 1] = '        if not relationSource then return nil, "missing source binding: " .. sourceName end'
    output[#output + 1] = '        local specification = {'
    output[#output + 1] = '            source = relationSource,'
    output[#output + 1] = '            target = target,'
    output[#output + 1] = '            sourceType = definition.sourceType,'
    output[#output + 1] = '            inherit = targetDefinition.inherit,'
    output[#output + 1] = '            aimAtSource = targetDefinition.aimAtSource,'
    output[#output + 1] = '            aimPoint = targetDefinition.aimPoint,'
    output[#output + 1] = '            influenceEnabled = definition.influenceEnabled,'
    output[#output + 1] = '            initialState = definition.initialState,'
    output[#output + 1] = '            states = definition.states,'
    output[#output + 1] = '            aimLimits = targetDefinition.aimLimits,'
    output[#output + 1] = '            spring = targetDefinition.spring,'
    output[#output + 1] = '            blend = targetDefinition.blend,'
    output[#output + 1] = '            offset = targetDefinition.offset,'
    output[#output + 1] = '            targetPivot = targetDefinition.targetPivot,'
    output[#output + 1] = '        }'
    output[#output + 1] = '        if definition.sourceBone then specification.sourceBone = definition.sourceBone end'
    output[#output + 1] = '        if definition.sourceSocket then specification.sourceSocket = definition.sourceSocket end'
    output[#output + 1] = '        local relation, errorMessage = attachments.attach(specification)'
    output[#output + 1] = '        if not relation then'
    output[#output + 1] = '            for _, created in ipairs(relations) do created:detach() end'
    output[#output + 1] = '            return nil, errorMessage'
    output[#output + 1] = '        end'
    output[#output + 1] = '        relations[index] = relation'
    output[#output + 1] = '    end'
    output[#output + 1] = '    return relations'
    output[#output + 1] = 'end'
    output[#output + 1] = ''
    output[#output + 1] = 'return {definition = definition, attach = attach}'
    return table.concat(output, '\n') .. '\n'
end

function M.write(path, definition)
    local content, errorMessage = M.build(definition)
    if not content then return false, errorMessage end
    local file, openError = io.open(path, 'w')
    if not file then return false, openError or 'could not open runtime export path' end
    file:write(content)
    file:close()
    return true
end

return M
