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
]]--

package.path = 'src/test-lib/?.lua;editor/?.lua;' .. package.path
local Project = require 'mesh_texture_paint_project'

local function expectFailure(callback)
    local ok = pcall(callback)
    assert(not ok, 'expected operation to fail')
end

local function write(path, content)
    local file = assert(io.open(path, 'wb'))
    assert(file:write(content))
    assert(file:close())
end

local function read(path)
    local file = assert(io.open(path, 'rb'))
    local content = assert(file:read('*a'))
    assert(file:close())
    return content
end

local function tempPath(suffix)
    local path = os.tmpname()
    os.remove(path)
    return path .. suffix
end

local projectPath = tempPath('.meshpaint')
local project = Project.new(
    '/tmp/characters/face = source.msh',
    '/tmp/characters/textures/face base.png',
    '/tmp/characters/paint layers/face layer.rgba')
project.working_texture = '/tmp/characters/output/face painted.png'
project.frame = 12
project.subset = 3
project.texture_role = 'base_color'
project.uv_channel = 1
project.projection_mode = 'project_stencil'
project.dilation_radius = 2.5
assert(Project.validate(project) == project)
assert(Project.save(project, projectPath))

local encoded = read(projectPath)
assert(encoded:find('mesh%-texture%-paint%-project\n', 1, false))
assert(not encoded:find(project.source_mesh, 1, true), 'path must not be stored as raw text')
assert(not encoded:find('return', 1, true), 'project format must not be Lua source')

local loaded = assert(Project.load(projectPath))
for _, field in ipairs({
    'version', 'source_mesh', 'source_texture', 'paint_layer', 'working_texture',
    'frame', 'subset', 'texture_role', 'uv_channel', 'projection_mode',
    'dilation_radius',
}) do
    assert(loaded[field] == project[field], field .. ' did not round-trip')
end
assert(Project.isAbsolutePath('/tmp/a'))
assert(Project.isAbsolutePath('C:\\assets\\a.msh'))
assert(Project.isAbsolutePath('\\\\server\\share\\a.msh'))
assert(not Project.isAbsolutePath('relative/a.msh'))

local noWorkingPath = tempPath('.meshpaint')
local noWorking = Project.copy(project)
noWorking.working_texture = nil
assert(Project.save(noWorking, noWorkingPath))
local noWorkingLoaded = assert(Project.load(noWorkingPath))
assert(noWorkingLoaded.working_texture == nil)

local luaProjectPath = tempPath('.lua')
assert(Project.saveLua(project, luaProjectPath))
local luaSource = read(luaProjectPath)
assert(luaSource:find('^%-%- Mesh Texture Paint project'))
assert(luaSource:find('return {', 1, true))
assert(luaSource:find('version = 1', 1, true))
local luaLoaded = assert(Project.loadLua(luaProjectPath))
assert(luaLoaded.source_mesh == project.source_mesh)
assert(luaLoaded.paint_layer == project.paint_layer)
assert(luaLoaded.projection_mode == project.projection_mode)
os.remove(luaProjectPath)

project.projection_mode = 'project_stencil_through'
assert(Project.validate(project) == project)

local invalid = Project.copy(project)
invalid.source_mesh = 'relative.msh'
expectFailure(function() Project.validate(invalid) end)
invalid = Project.copy(project); invalid.frame = 1 / 0
expectFailure(function() Project.validate(invalid) end)
invalid = Project.copy(project); invalid.dilation_radius = 0 / 0
expectFailure(function() Project.validate(invalid) end)
invalid = Project.copy(project); invalid.uv_channel = 8
expectFailure(function() Project.validate(invalid) end)
invalid = Project.copy(project); invalid.projection_mode = 'unknown'
expectFailure(function() Project.validate(invalid) end)
invalid = Project.copy(project); invalid.unknown = true
expectFailure(function() Project.validate(invalid) end)
invalid = Project.copy(project); invalid.texture_role = 'base color'
expectFailure(function() Project.validate(invalid) end)

local base = table.concat({
    'mesh-texture-paint-project',
    'version=1',
    'source_mesh=' .. Project.encodePath('/tmp/a.msh'),
    'source_texture=' .. Project.encodePath('/tmp/a.png'),
    'paint_layer=' .. Project.encodePath('/tmp/a.layer'),
    'frame=1',
    'subset=1',
    'texture_role=base_color',
    'uv_channel=0',
    'projection_mode=uv_brush',
    'dilation_radius=0',
    '',
}, '\n')

local unknownPath = tempPath('.meshpaint')
write(unknownPath, base .. 'unknown_key=value\n')
expectFailure(function() Project.load(unknownPath) end)
os.remove(unknownPath)

local badVersionPath = tempPath('.meshpaint')
write(badVersionPath, base:gsub('version=1', 'version=2'))
expectFailure(function() Project.load(badVersionPath) end)
os.remove(badVersionPath)

local malformedPath = tempPath('.meshpaint')
write(malformedPath, base:gsub('source_mesh=' .. Project.encodePath('/tmp/a.msh'), 'source_mesh=GG'))
expectFailure(function() Project.load(malformedPath) end)
os.remove(malformedPath)

local duplicatePath = tempPath('.meshpaint')
write(duplicatePath, base .. 'frame=2\n')
expectFailure(function() Project.load(duplicatePath) end)
os.remove(duplicatePath)

local executionPath = tempPath('.meshpaint')
local markerPath = executionPath .. '.executed'
write(executionPath, 'return os.execute(' .. string.format('%q', 'touch ' .. markerPath) .. ')\n')
expectFailure(function() Project.load(executionPath) end)
local marker = io.open(markerPath, 'rb')
assert(marker == nil, 'opening a project must not execute Lua')
if marker then marker:close() end
os.remove(executionPath)
os.remove(markerPath)

local luaExecutionPath = tempPath('.lua')
local luaMarkerPath = luaExecutionPath .. '.executed'
write(luaExecutionPath, 'return os.execute(' .. string.format('%q', 'touch ' .. luaMarkerPath) .. ')\n')
expectFailure(function() Project.loadLua(luaExecutionPath) end)
local luaMarker = io.open(luaMarkerPath, 'rb')
assert(luaMarker == nil, 'loading a project must not execute globals')
if luaMarker then luaMarker:close() end
os.remove(luaExecutionPath)
os.remove(luaMarkerPath)

os.remove(projectPath)
os.remove(noWorkingPath)
print('MESH TEXTURE PAINT PROJECT TEST OK')
