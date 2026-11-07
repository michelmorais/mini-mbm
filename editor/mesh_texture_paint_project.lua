--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
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
|------------------------------------------------------------------------------------------------------------------------|

   Versioned, non-executable project persistence for the mesh texture paint editor.
]]--

local M = {}

M.FORMAT = 'mesh-texture-paint-project'
M.VERSION = 1
M.MAX_FILE_BYTES = 65536
M.MAX_PATH_BYTES = 4096
M.MAX_ROLE_BYTES = 64
M.MAX_INDEX = 1000000
M.MAX_UV_CHANNEL = 7
M.MAX_DILATION_RADIUS = 64

local REQUIRED_FIELDS = {
    'version', 'source_mesh', 'source_texture', 'paint_layer',
    'frame', 'subset', 'texture_role', 'uv_channel',
    'projection_mode', 'dilation_radius',
}

local KNOWN_FIELDS = {
    version = true,
    source_mesh = true,
    source_texture = true,
    paint_layer = true,
    working_texture = true,
    frame = true,
    subset = true,
    texture_role = true,
    uv_channel = true,
    projection_mode = true,
    dilation_radius = true,
    limit_to_subset = true,
}

local PROJECTION_MODES = {
    uv_brush = true,
    project_stencil = true,
    project_stencil_through = true,
    surface_3d = true,
}

local function copy(value)
    if type(value) ~= 'table' then return value end
    local result = {}
    for key, item in pairs(value) do result[key] = copy(item) end
    return result
end

local function finite(value)
    return type(value) == 'number' and value == value and
        value ~= math.huge and value ~= -math.huge
end

local function validInteger(value, minimum, maximum)
    return finite(value) and value == math.floor(value) and
        value >= minimum and value <= maximum
end

local function hasControlByte(value)
    for index = 1, #value do
        if value:byte(index) < 32 or value:byte(index) == 127 then return true end
    end
    return false
end

local function validPath(path, field, optional)
    if optional and path == nil then return end
    assert(type(path) == 'string', field .. ' must be a string')
    assert(#path > 0 and #path <= M.MAX_PATH_BYTES, field .. ' must be a non-empty path')
    assert(not hasControlByte(path), field .. ' contains a control character')
    assert(M.isAbsolutePath(path), field .. ' must be an absolute path')
end

local function validToken(value, field)
    assert(type(value) == 'string', field .. ' must be a string')
    assert(#value > 0 and #value <= M.MAX_ROLE_BYTES, field .. ' has an invalid length')
    assert(value:match('^[%w_.%-]+$'), field .. ' contains invalid characters')
end

function M.isAbsolutePath(path)
    if type(path) ~= 'string' then return false end
    return path:sub(1, 1) == '/' or path:match('^%a:[/\\]') ~= nil or
        path:match('^\\\\') ~= nil
end

function M.defaults()
    return {
        version = M.VERSION,
        source_mesh = '',
        source_texture = '',
        paint_layer = '',
        working_texture = nil,
        frame = 1,
        subset = 1,
        texture_role = 'base_color',
        uv_channel = 0,
        projection_mode = 'uv_brush',
        dilation_radius = 0,
        limit_to_subset = false,
    }
end

function M.new(sourceMesh, sourceTexture, paintLayer)
    local project = M.defaults()
    project.source_mesh = sourceMesh or project.source_mesh
    project.source_texture = sourceTexture or project.source_texture
    project.paint_layer = paintLayer or project.paint_layer
    return project
end

function M.copy(project)
    return copy(project)
end

function M.validate(project)
    assert(type(project) == 'table', 'project must be a table')
    for key in pairs(project) do
        assert(type(key) == 'string' and KNOWN_FIELDS[key] == true,
            'unknown project field: ' .. tostring(key))
    end
    for _, field in ipairs(REQUIRED_FIELDS) do
        assert(project[field] ~= nil, 'missing project field: ' .. field)
    end

    assert(project.version == M.VERSION, 'unsupported project version')
    validPath(project.source_mesh, 'source_mesh')
    validPath(project.source_texture, 'source_texture')
    validPath(project.paint_layer, 'paint_layer')
    validPath(project.working_texture, 'working_texture', true)

    assert(validInteger(project.frame, 1, M.MAX_INDEX), 'frame must be a positive integer')
    assert(validInteger(project.subset, 1, M.MAX_INDEX), 'subset must be a positive integer')
    validToken(project.texture_role, 'texture_role')
    assert(validInteger(project.uv_channel, 0, M.MAX_UV_CHANNEL),
        'uv_channel must be an integer from 0 to ' .. M.MAX_UV_CHANNEL)
    assert(type(project.projection_mode) == 'string' and
        PROJECTION_MODES[project.projection_mode] == true,
        'invalid projection_mode')
    assert(finite(project.dilation_radius) and project.dilation_radius >= 0 and
        project.dilation_radius <= M.MAX_DILATION_RADIUS,
        'dilation_radius must be a finite number from 0 to ' .. M.MAX_DILATION_RADIUS)
    assert(type(project.limit_to_subset) == 'boolean', 'limit_to_subset must be boolean')
    return project
end

local function hexByte(value)
    return string.format('%02X', value:byte())
end

function M.encodePath(path)
    assert(type(path) == 'string', 'path must be a string')
    return (path:gsub('.', hexByte))
end

function M.decodePath(encoded)
    assert(type(encoded) == 'string', 'encoded path must be a string')
    assert(#encoded <= M.MAX_PATH_BYTES * 2 and #encoded % 2 == 0,
        'encoded path has an invalid length')
    assert(encoded:match('^[%x]*$'), 'encoded path contains invalid characters')
    local path = encoded:gsub('(%x%x)', function(pair)
        return string.char(tonumber(pair, 16))
    end)
    assert(#path <= M.MAX_PATH_BYTES and not hasControlByte(path),
        'decoded path contains invalid characters')
    return path
end

local function formatNumber(value)
    return string.format('%.17g', value)
end

local function readFile(path)
    local file, errorMessage = io.open(path, 'rb')
    assert(file, errorMessage)
    local data = file:read(M.MAX_FILE_BYTES + 1)
    local closed, closeError = file:close()
    assert(closed, closeError)
    assert(data and #data <= M.MAX_FILE_BYTES, 'project file is too large')
    return data
end

local function writeFile(path, data)
    local temporaryPath = path .. '.tmp'
    local file, errorMessage = io.open(temporaryPath, 'wb')
    assert(file, errorMessage)
    local written, writeError = file:write(data)
    local closed, closeError = file:close()
    if not written or not closed then
        os.remove(temporaryPath)
        error(writeError or closeError)
    end
    local renamed, renameError = os.rename(temporaryPath, path)
    if not renamed then
        os.remove(temporaryPath)
        error(renameError)
    end
end

local function readLuaProject(path)
    local file, errorMessage = io.open(path, 'rb')
    assert(file, errorMessage)
    local data = file:read(M.MAX_FILE_BYTES + 1)
    assert(file:close())
    assert(data and #data <= M.MAX_FILE_BYTES, 'project file is too large')
    local chunk, loadError = load(data, '@' .. path, 't', {})
    assert(chunk, loadError)
    local ok, value = pcall(chunk)
    assert(ok, value)
    return value
end

local function luaString(value)
    return string.format('%q', value)
end

function M.save(project, path)
    M.validate(project)
    assert(type(path) == 'string' and path ~= '', 'project output path is required')

    local lines = {
        M.FORMAT,
        'version=' .. tostring(project.version),
        'source_mesh=' .. M.encodePath(project.source_mesh),
        'source_texture=' .. M.encodePath(project.source_texture),
        'paint_layer=' .. M.encodePath(project.paint_layer),
        'frame=' .. tostring(project.frame),
        'subset=' .. tostring(project.subset),
        'texture_role=' .. project.texture_role,
        'uv_channel=' .. tostring(project.uv_channel),
        'projection_mode=' .. project.projection_mode,
        'dilation_radius=' .. formatNumber(project.dilation_radius),
        'limit_to_subset=' .. tostring(project.limit_to_subset),
    }
    if project.working_texture ~= nil then
        lines[#lines + 1] = 'working_texture=' .. M.encodePath(project.working_texture)
    end
    writeFile(path, table.concat(lines, '\n') .. '\n')
    return true
end

function M.saveLua(project, path)
    M.validate(project)
    assert(type(path) == 'string' and path ~= '', 'project output path is required')
    local lines = {
        '-- Mesh Texture Paint project. Edit with the Mesh Texture Paint Editor.',
        'return {',
        '    version = ' .. tostring(project.version) .. ',',
        '    source_mesh = ' .. luaString(project.source_mesh) .. ',',
        '    source_texture = ' .. luaString(project.source_texture) .. ',',
        '    paint_layer = ' .. luaString(project.paint_layer) .. ',',
        '    working_texture = ' .. (project.working_texture and luaString(project.working_texture) or 'nil') .. ',',
        '    frame = ' .. tostring(project.frame) .. ',',
        '    subset = ' .. tostring(project.subset) .. ',',
        '    texture_role = ' .. luaString(project.texture_role) .. ',',
        '    uv_channel = ' .. tostring(project.uv_channel) .. ',',
        '    projection_mode = ' .. luaString(project.projection_mode) .. ',',
        '    dilation_radius = ' .. formatNumber(project.dilation_radius) .. ',',
        '    limit_to_subset = ' .. tostring(project.limit_to_subset) .. ',',
        '}',
    }
    writeFile(path, table.concat(lines, '\n') .. '\n')
    return true
end

local function splitLines(data)
    local lines = {}
    for line in (data .. '\n'):gmatch('(.-)\n') do
        if line:sub(-1) == '\r' then line = line:sub(1, -2) end
        lines[#lines + 1] = line
    end
    if lines[#lines] == '' then lines[#lines] = nil end
    return lines
end

local function parseInteger(value, field)
    assert(value:match('^%d+$'), 'invalid ' .. field)
    local number = tonumber(value)
    assert(validInteger(number, 0, M.MAX_INDEX), 'invalid ' .. field)
    return number
end

local function parseNumber(value, field)
    local mantissa, exponent = value:match('^(.+)[eE]([+-]?%d+)$')
    mantissa = mantissa or value
    local validMantissa = mantissa:match('^[-+]?%d+%.?%d*$') or
        mantissa:match('^[-+]?%.%d+$')
    assert(validMantissa and (exponent == nil or exponent:match('^[+-]?%d+$')),
        'invalid ' .. field)
    local number = tonumber(value)
    assert(finite(number), 'invalid ' .. field)
    return number
end

function M.load(path)
    local lines = splitLines(readFile(path))
    assert(lines[1] == M.FORMAT, 'invalid mesh texture paint project format')
    local values = {}
    for index = 2, #lines do
        local key, value = lines[index]:match('^([a-z_]+)=(.*)$')
        assert(key and KNOWN_FIELDS[key] == true, 'unknown or malformed project field')
        assert(values[key] == nil, 'duplicate project field: ' .. key)
        values[key] = value
    end
    for _, field in ipairs(REQUIRED_FIELDS) do
        assert(values[field] ~= nil, 'missing project field: ' .. field)
    end

    local project = M.defaults()
    project.version = parseInteger(values.version, 'version')
    project.source_mesh = M.decodePath(values.source_mesh)
    project.source_texture = M.decodePath(values.source_texture)
    project.paint_layer = M.decodePath(values.paint_layer)
    if values.working_texture ~= nil and values.working_texture ~= '' then
        project.working_texture = M.decodePath(values.working_texture)
    end
    project.frame = parseInteger(values.frame, 'frame')
    project.subset = parseInteger(values.subset, 'subset')
    project.texture_role = values.texture_role
    project.uv_channel = parseInteger(values.uv_channel, 'uv_channel')
    project.projection_mode = values.projection_mode
    project.dilation_radius = parseNumber(values.dilation_radius, 'dilation_radius')
    if values.limit_to_subset ~= nil then
        assert(values.limit_to_subset == 'true' or values.limit_to_subset == 'false',
            'invalid limit_to_subset')
        project.limit_to_subset = values.limit_to_subset == 'true'
    end
    return M.validate(project)
end

function M.loadLua(path)
    local value = readLuaProject(path)
    assert(type(value) == 'table', 'Lua project must return a table')
    local project = M.defaults()
    for key, item in pairs(value) do project[key] = item end
    return M.validate(project)
end

return M
