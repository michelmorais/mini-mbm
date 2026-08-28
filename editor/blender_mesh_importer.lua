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

    Shared asynchronous Blender GLB/FBX to mini-mbm MSH importer.

    The caller owns UI and persistence. This module owns one Blender process job and
    advances it only when update() is called from the current engine's frame loop.
]]--

local M = {}

local function normalizePath(path)
    return tostring(path or ''):gsub('\\', '/'):gsub('/+$', '')
end

local function moduleDirectory()
    local source = debug.getinfo(1, 'S').source or ''
    if source:sub(1, 1) == '@' then source = source:sub(2) end
    source = normalizePath(source)
    return source:match('^(.*)/[^/]+$') or '.'
end

local function fileSize(path)
    local file = io.open(path, 'rb')
    if not file then return 0 end
    local size = file:seek('end') or 0
    file:close()
    return size
end

local function readText(path)
    local file = io.open(path, 'rb')
    if not file then return '' end
    local content = file:read('*a') or ''
    file:close()
    return content
end

local function writeText(path, content)
    local file = io.open(path, 'wb')
    if not file then return false end
    file:write(content or '')
    file:close()
    return true
end

local function progressFromLog(content)
    local progress = {sourceFrame = nil, framesExported = nil, decimating = false, scaling = false, writing = false, done = false}
    for line in tostring(content or ''):gmatch('[^\r\n]+') do
        local frame = line:match('%[blender_export%]%s+export frame:%s+(%d+)')
        if frame then progress.sourceFrame = tonumber(frame) end
        local exported = line:match('%[blender_export%]%s+frames exported:%s+(%d+)')
        if exported then progress.framesExported = tonumber(exported) end
        if line:find('[blender_export] decimate:', 1, true) then progress.decimating = true end
        if line:find('[blender_export] applying uniform scale:', 1, true) then progress.scaling = true end
        if line:find('[blender_export] writing output', 1, true) then progress.writing = true end
        if line:find('[blender_export] done', 1, true) then progress.done = true end
    end
    return progress
end

local function errorFromLog(content)
    local lines = {}
    for line in tostring(content or ''):gmatch('[^\r\n]+') do
        if line:match('%S') then lines[#lines + 1] = line end
    end
    for index = #lines, 1, -1 do
        local line = lines[index]
        if line:match('^[%a_][%w_]*Error:%s+.+') or line:match('^[%a_][%w_]*Exception:%s+.+') then
            return line:gsub('^%[blender_export%]%s*', '')
        end
        if line:find('Exporter failed:', 1, true) or line:find('RuntimeError:', 1, true) then
            return line:gsub('^%[blender_export%]%s*', '')
        end
    end
    return nil
end

local Job = {}
Job.__index = Job

function Job:_finish(status, message)
    if self.status ~= 'running' then return end
    self.status = status
    self.message = tostring(message or '')
    if status == 'failed' and self.errorCode == '' then self.errorCode = 'conversion_blender_failed' end
    if status == 'completed' and self.onComplete then
        self.onComplete(self.output)
    elseif status ~= 'completed' and self.onError then
        self.onError(self.message)
    end
end

function Job:update()
    if self.status ~= 'running' then return self.status end
    local content = readText(self.log)
    local logError = errorFromLog(content)
    if logError then
        self:_finish('failed', logError)
        return self.status
    end
    local progress = progressFromLog(content)
    if progress.framesExported then
        self.progress = 0.9
        self.phase = 'exported'
        self.framesExported = progress.framesExported
    elseif progress.writing then
        self.progress = 0.9
        self.phase = 'writing'
        self.detail = 'Writing MSH'
    elseif progress.scaling then
        self.progress = math.max(self.progress or 0, 0.6)
        self.phase = 'scaling'
        self.detail = 'Applying uniform scale'
    elseif progress.sourceFrame then
        self.progress = math.max(0, math.min(0.9, progress.sourceFrame / math.max(1, self.expectedFrames)))
        self.phase = 'frame'
        self.sourceFrame = progress.sourceFrame
        self.detail = string.format('Frame %d / %d', progress.sourceFrame, self.expectedFrames)
    elseif progress.decimating then
        self.progress = math.max(self.progress or 0, 0.4)
        self.phase = 'decimating'
        self.detail = 'Reducing polygons'
    end
    -- Direct MSH export atomically renames its fully flushed temporary file to
    -- self.output. Seeing the final path is therefore a safe completion signal,
    -- even if Blender never flushes the optional trailing "done" log line.
    if fileSize(self.output) > 0 then
        self.progress = 1
        self.phase = 'completed'
        self.detail = 'Completed'
        self:_finish('completed', '')
        return self.status
    end
    if content ~= self.lastLog then
        self.lastLog = content
        self.lastActivity = os.time()
    elseif os.time() - self.lastActivity >= self.timeout then
        self.errorCode = 'conversion_timeout'
        self:_finish('failed', 'Blender conversion timed out.')
    end
    return self.status
end

function Job:cancel()
    if self.status ~= 'running' then return false end
    writeText(self.cancelFile, 'cancel\n')
    self:_finish('canceled', 'Conversion canceled.')
    return true
end

function M.start(config)
    assert(type(config) == 'table', 'blender_mesh_importer.start expects a configuration table')
    local wrapper = assert(config.wrapper, 'blender_mesh_importer requires wrapper')
    local source = normalizePath(assert(config.input, 'blender_mesh_importer requires input'))
    local output = normalizePath(assert(config.output, 'blender_mesh_importer requires output'))
    local options = config.options or {}
    local detected = wrapper.detectBlender()
    if not detected or not detected.found then return nil, 'Blender was not found.' end
    if fileSize(source) <= 0 then return nil, 'Input GLB/FBX was not found or is empty.' end
    if fileSize(output) > 0 then return nil, 'Output MSH already exists.' end

    local stem = output:gsub('%.msh$', '')
    local log = normalizePath(config.log or (stem .. '_blender.log'))
    local cancelFile = normalizePath(config.cancelFile or (stem .. '_cancel'))
    os.remove(log)
    os.remove(cancelFile)
    local exporter = moduleDirectory() .. '/blender_mesh_export.py'
    local importOptions = {
        directMshOutput = true,
        includeBones = options.includeBones ~= false,
        uniformScale = options.uniformScale,
        normalizeTextures = options.normalizeTextures == true,
        includeTextureDiffuse = options.includeTextureDiffuse,
        includeTextureNormal = options.includeTextureNormal,
        includeTextureSpecular = options.includeTextureSpecular,
        includeTextureEmissive = options.includeTextureEmissive,
        includeTextureMask = options.includeTextureMask,
        largeMeshMode = options.largeMeshMode or 'fail',
        decimateRatio = options.decimateRatio,
        debugSteps = options.debugSteps == true,
        cancelFile = cancelFile,
        importPostProcess = options.importPostProcess == true,
        importInvertU = options.importInvertU == true,
        importInvertV = options.importInvertV == true,
        importAngleX = options.importAngleX or 0,
        importAngleY = options.importAngleY or 0,
        importAngleZ = options.importAngleZ or 0,
        bakeAnimation = options.bakeAnimation == true,
        useSceneFrameRange = options.useSceneFrameRange == true,
        frameStart = options.frameStart,
        frameEnd = options.frameEnd,
        sampleStep = options.sampleStep,
        animationName = options.animationName,
        animationClips = options.animationClips,
    }
    local command = wrapper.buildBakeCmd(source, output, exporter, importOptions)
    if not command then return nil, 'Could not build the Blender conversion command.' end
    wrapper.setDebugEnabled(options.debugSteps == true)
    wrapper.launchCmdAsync(command, log)
    local job = setmetatable({
        status = 'running', input = source, output = output, log = log,
        cancelFile = cancelFile, progress = 0, phase = 'starting', detail = 'Starting Blender', errorCode = '',
        expectedFrames = math.max(1, tonumber(config.expectedFrames) or 1),
        timeout = math.max(5, tonumber(config.timeout) or 120),
        lastActivity = os.time(), lastLog = '',
        onComplete = config.onComplete, onError = config.onError,
    }, Job)
    return job
end

return M
