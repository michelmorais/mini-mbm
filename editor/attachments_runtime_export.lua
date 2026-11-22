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

function M.validate(definition)
    if type(definition) ~= 'table' then return false, 'attachment definition must be a table' end
    if type(definition.source) ~= 'string' or definition.source == '' then
        return false, 'source binding name must not be empty'
    end
    if type(definition.target) ~= 'string' or definition.target == '' then
        return false, 'target binding name must not be empty'
    end
    if definition.source == definition.target then
        return false, 'source and target binding names must be different'
    end
    if definition.sourceType ~= 'root' and definition.sourceType ~= 'bone' then
        return false, "sourceType must be 'root' or 'bone'"
    end
    if definition.sourceType == 'bone' and
            (type(definition.sourceBone) ~= 'string' or definition.sourceBone == '') then
        return false, "sourceBone must not be empty when sourceType is 'bone'"
    end
    local inherit = definition.inherit
    if type(inherit) ~= 'table' then return false, 'inherit must be a table' end
    local offset = definition.offset
    if type(offset) ~= 'table' then return false, 'offset must be a table' end
    for _, component in ipairs({'position', 'rotation', 'scale'}) do
        if not validateVector(offset[component], component == 'scale' and 1 or 0) then
            return false, 'offset.' .. component .. ' must be a vector'
        end
    end
    if definition.targetPivot ~= nil and not validateVector(definition.targetPivot, 0) then
        return false, 'targetPivot must be a vector'
    end
    return true
end

local function appendDefinition(output, definition)
    local inherit = definition.inherit
    local offset = definition.offset
    local function addVector(name, value)
        output[#output + 1] = string.format('        %s = {x = %.17g, y = %.17g, z = %.17g},',
            name, number(value.x, 0), number(value.y, 0), number(value.z, name == 'scale' and 1 or 0))
    end
    output[#output + 1] = 'local definition = {'
    output[#output + 1] = '    source = ' .. quote(definition.source) .. ','
    output[#output + 1] = '    target = ' .. quote(definition.target) .. ','
    output[#output + 1] = '    sourceType = ' .. quote(definition.sourceType) .. ','
    if definition.sourceType == 'bone' then
        output[#output + 1] = '    sourceBone = ' .. quote(definition.sourceBone) .. ','
    end
    output[#output + 1] = '    inherit = {'
    output[#output + 1] = '        position = ' .. tostring(inherit.position == true) .. ','
    output[#output + 1] = '        rotation = ' .. tostring(inherit.rotation == true) .. ','
    output[#output + 1] = '        scale = ' .. tostring(inherit.scale == true) .. ','
    output[#output + 1] = '    },'
    output[#output + 1] = '    offset = {'
    addVector('position', offset.position)
    addVector('rotation', offset.rotation)
    addVector('scale', offset.scale)
    output[#output + 1] = '    },'
    local targetPivot = definition.targetPivot or {x = 0, y = 0, z = 0}
    output[#output + 1] = string.format('    targetPivot = {x = %.17g, y = %.17g, z = %.17g},',
        number(targetPivot.x, 0), number(targetPivot.y, 0), number(targetPivot.z, 0))
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
    output[#output + 1] = '    local target = bindings[definition.target]'
    output[#output + 1] = '    if not source then return nil, "missing source binding: " .. definition.source end'
    output[#output + 1] = '    if not target then return nil, "missing target binding: " .. definition.target end'
    output[#output + 1] = '    local specification = {'
    output[#output + 1] = '        source = source,'
    output[#output + 1] = '        target = target,'
    output[#output + 1] = '        sourceType = definition.sourceType,'
    output[#output + 1] = '        inherit = definition.inherit,'
    output[#output + 1] = '        offset = definition.offset,'
    output[#output + 1] = '        targetPivot = definition.targetPivot,'
    output[#output + 1] = '    }'
    output[#output + 1] = '    if definition.sourceBone then specification.sourceBone = definition.sourceBone end'
    output[#output + 1] = '    return attachments.attach(specification)'
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
