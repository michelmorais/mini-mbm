--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
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
|------------------------------------------------------------------------------------------------------------------------|
]]--

local source = debug.getinfo(1, 'S').source
local testDirectory = source:sub(1, 1) == '@' and source:sub(2):match('^(.*[/\\])') or ''
package.path = testDirectory .. '../../editor/?.lua;' .. package.path

local importer = require 'blender_mesh_importer'
local input = os.tmpname() .. '.glb'
local output = os.tmpname() .. '.msh'
local log = output .. '.log'
local cancel = output .. '.cancel'
local file = assert(io.open(input, 'wb'))
file:write('glb')
file:close()

local launched = nil
local wrapper = {
    detectBlender = function() return {found = true, path = '/test/blender', version = '1.0'} end,
    buildBakeCmd = function(inputPath, outputPath, exporterPath, options)
        assert(inputPath == input)
        assert(outputPath == output)
        assert(exporterPath:match('/editor/blender_mesh_export%.py$'))
        assert(options.directMshOutput == true)
        assert(options.largeMeshMode == 'vb_only')
        assert(options.uniformScale == 100)
        assert(options.normalizeTextures == true)
        assert(options.includeTextureDiffuse == true)
        assert(options.includeTextureNormal == true)
        assert(options.includeTextureSpecular == true)
        assert(options.includeTextureEmissive == true)
        assert(options.includeTextureMask == false)
        return 'mock-command'
    end,
    setDebugEnabled = function() end,
    launchCmdAsync = function(command, logPath)
        launched = {command = command, log = logPath}
    end,
}

local completed = false
local job = assert(importer.start({
    wrapper = wrapper, input = input, output = output, log = log, cancelFile = cancel,
    options = {largeMeshMode = 'vb_only', uniformScale = 100, normalizeTextures = true,
        includeTextureDiffuse = true, includeTextureNormal = true, includeTextureSpecular = true,
        includeTextureEmissive = true, includeTextureMask = false},
    onComplete = function(path) completed = path == output end,
}))
assert(job.status == 'running')
assert(launched.command == 'mock-command' and launched.log == log)

file = assert(io.open(log, 'wb'))
file:write('[blender_export] export frame: 1\n[blender_export] applying uniform scale: 100\n')
file:close()
assert(job:update() == 'running')
assert(job.progress == 0.6 and job.phase == 'scaling')

file = assert(io.open(output, 'wb'))
file:write('msh')
file:close()
file = assert(io.open(log, 'wb'))
file:write('[blender_export] writing output\n')
file:close()
assert(job:update() == 'completed')
assert(job.progress == 1 and completed)

os.remove(input)
os.remove(output)
os.remove(log)
os.remove(cancel)
