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

    blender_cli_wrapper.lua

    Minimal Blender CLI wrapper used by mesh_debug.lua to:
      - detect Blender executable on each OS
      - build background export commands
      - run commands asynchronously
      - poll output-file existence

]]--

local M = {}

M.blender = nil
M.customPath = nil
M.debugEnabled = false

local function debugPrint(fmt, ...)
    if not M.debugEnabled then return end
    local ok, msg = pcall(string.format, fmt, ...)
    if ok then
        print('[blender_cli] ' .. msg)
    end
end

function M.getOS()
    local s = (mbm and mbm.get('os') or ''):lower()
    if s == '' then
        return package.config:sub(1, 1) == '\\' and 'windows' or 'linux'
    end
    return s
end

local function shellQuote(s)
    return '"' .. tostring(s):gsub('"', '\\"') .. '"'
end

local function wrapCmd(cmd)
    if M.getOS() == 'windows' then
        return 'cmd /c "' .. cmd .. '"'
    end
    return cmd
end

local function trim(s)
    return (tostring(s or ''):match('^%s*(.-)%s*$'))
end

local function pathExists(path)
    local f = io.open(path, 'rb')
    if not f then return false end
    f:close()
    return true
end

local function normalizeBlenderExePath(exe)
    exe = trim(exe)
    if exe == '' then return '' end

    if M.getOS() == 'windows' then
        if exe:lower():match('%.exe$') then
            return exe
        end

        local clean = exe:gsub('[/\\]+$', '')
        local withExe = clean .. '\\blender.exe'
        if pathExists(withExe) then
            return withExe
        end
        return exe
    end

    local clean = exe:gsub('[/\\]+$', '')
    if clean:sub(-8) == '/blender' then
        return clean
    end

    local withExe = clean .. '/blender'
    if pathExists(withExe) then
        return withExe
    end
    return exe
end

local function tryDetectExe(exe)
    exe = normalizeBlenderExePath(exe)
    if exe == '' then return nil end

    debugPrint('detect: probing executable [%s]', tostring(exe))
    local f = io.popen(wrapCmd(shellQuote(exe) .. ' --version') .. ' 2>&1')
    if not f then return nil end
    local output = f:read('*a')
    f:close()
    if output and output:find('Blender') then
        local version = output:match('Blender%s+([%d%.]+)') or ''
        debugPrint('detect: found Blender version [%s] at [%s]', version, tostring(exe))
        return { found = true, path = exe, version = version }
    end
    debugPrint('detect: not a Blender executable [%s]', tostring(exe))
    return nil
end

local WINDOWS_BLENDER_CANDIDATES = {
    'C:\\Program Files\\Blender Foundation\\Blender 5.2\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 5.1\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 5.0\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 4.4\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 4.3\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 4.2\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 4.1\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender\\blender.exe',
}

local function whereExe(exe)
    local f = io.popen('where "' .. exe .. '" 2>nul')
    if not f then return nil end
    local line = f:read('*l')
    f:close()
    if line and line ~= '' and not line:lower():find('could not find') then
        return trim(line)
    end
    return nil
end

local function findWindowsBlenderInstallCandidates()
    local out = {}
    local seen = {}

    local function addCandidate(path)
        path = trim(path)
        if path ~= '' and not seen[path:lower()] then
            seen[path:lower()] = true
            table.insert(out, path)
        end
    end

    local bases = {
        os.getenv('ProgramW6432'),
        os.getenv('ProgramFiles'),
        os.getenv('ProgramFiles(x86)'),
    }

    for _, base in ipairs(bases) do
        base = trim(base)
        if base ~= '' then
            local root = base .. '\\Blender Foundation'
            local f = io.popen('cmd /c dir /b /ad "' .. root .. '\\Blender*" 2>nul')
            if f then
                for line in f:lines() do
                    line = trim(line)
                    if line ~= '' then
                        addCandidate(root .. '\\' .. line .. '\\blender.exe')
                    end
                end
                f:close()
            end
            addCandidate(root .. '\\Blender\\blender.exe')
        end
    end

    for _, p in ipairs(WINDOWS_BLENDER_CANDIDATES) do
        addCandidate(p)
    end

    return out
end

function M.detectBlender()
    if M.blender ~= nil then return M.blender end

    if M.customPath and M.customPath ~= '' then
        local r = tryDetectExe(M.customPath)
        if r then M.blender = r; return M.blender end
    end

    local r = tryDetectExe('blender')
    if r then M.blender = r; return M.blender end

    if M.getOS() == 'windows' then
        local whereBlender = whereExe('blender.exe')
        if whereBlender then
            r = tryDetectExe(whereBlender)
            if r then M.blender = r; return M.blender end
        end

        for _, candidate in ipairs(findWindowsBlenderInstallCandidates()) do
            local probe = io.open(candidate, 'rb')
            if probe then
                probe:close()
                r = tryDetectExe(candidate)
                if r then M.blender = r; return M.blender end
            end
        end
    end

    M.blender = { found = false, path = '', version = '' }
    return M.blender
end

function M.resetDetection()
    M.blender = nil
end

function M.setCustomPath(path)
    M.customPath = path
    M.blender = nil
end

function M.setDebugEnabled(enabled)
    M.debugEnabled = enabled and true or false
end

function M.buildBakeCmd(sourcePath, outputLuaPath, exporterScriptPath, options)
    local b = M.blender or M.detectBlender()
    if not b.found then return nil end

    options = options or {}

    local sourceExt = ((sourcePath:match('%.([^%.]+)$')) or ''):lower()

    local args = {
        shellQuote(b.path),
        '-b',
    }

    if sourceExt == 'blend' then
        table.insert(args, shellQuote(sourcePath))
    else
        table.insert(args, '--factory-startup')
    end

    table.insert(args, '--python')
    table.insert(args, shellQuote(exporterScriptPath))
    table.insert(args, '--')
    table.insert(args, '--input')
    table.insert(args, shellQuote(sourcePath))
    table.insert(args, '--output')
    table.insert(args, shellQuote(outputLuaPath))

    if options.streamOutput then
        table.insert(args, '--stream-output')
    end

    if options.directMshOutput then
        table.insert(args, '--direct-msh-output')
    end

    if options.bakeAnimation then
        table.insert(args, '--bake-animation')
    end

    if options.useSceneFrameRange then
        table.insert(args, '--use-scene-frame-range')
    end

    if options.frameStart then
        table.insert(args, '--frame-start')
        table.insert(args, tostring(options.frameStart))
    end

    if options.frameEnd then
        table.insert(args, '--frame-end')
        table.insert(args, tostring(options.frameEnd))
    end

    if options.sampleStep then
        table.insert(args, '--sample-step')
        table.insert(args, tostring(options.sampleStep))
    end

    if options.animationName and options.animationName ~= '' then
        table.insert(args, '--animation-name')
        table.insert(args, shellQuote(options.animationName))
    end

    if type(options.animationClips) == 'table' then
        for i = 1, #options.animationClips do
            local clip = options.animationClips[i]
            if type(clip) == 'table' then
                table.insert(args, '--animation-clip')
                table.insert(args, shellQuote(clip.name or ('Bake ' .. i)))
                table.insert(args, tostring(math.max(1, math.floor(tonumber(clip.frameStart or 1) or 1))))
                table.insert(args, tostring(math.max(1, math.floor(tonumber(clip.frameEnd or clip.frameStart or 1) or 1))))
                table.insert(args, tostring(math.max(1, math.floor(tonumber(clip.sampleStep or 1) or 1))))
            end
        end
    end

    if options.largeMeshMode and options.largeMeshMode ~= '' then
        table.insert(args, '--large-mesh-mode')
        table.insert(args, tostring(options.largeMeshMode))
    end

    if options.importPostProcess then
        table.insert(args, '--post-process')
        if options.importInvertU then
            table.insert(args, '--invert-u')
        end
        if options.importInvertV then
            table.insert(args, '--invert-v')
        end
        table.insert(args, '--angle-x')
        table.insert(args, tostring(options.importAngleX or 0))
        table.insert(args, '--angle-y')
        table.insert(args, tostring(options.importAngleY or 0))
        table.insert(args, '--angle-z')
        table.insert(args, tostring(options.importAngleZ or 0))
    end

    if options.cancelFile and options.cancelFile ~= '' then
        table.insert(args, '--cancel-file')
        table.insert(args, shellQuote(options.cancelFile))
    end

    if options.debugSteps then
        table.insert(args, '--debug-steps')
    end

    local cmd = table.concat(args, ' ')
    debugPrint('build cmd: %s', cmd)
    return cmd
end

function M.buildScanCmd(sourcePath, outputLuaPath, exporterScriptPath, options)
    local b = M.blender or M.detectBlender()
    if not b.found then return nil end

    options = options or {}
    local sourceExt = ((sourcePath:match('%.([^%.]+)$')) or ''):lower()

    local args = {
        shellQuote(b.path),
        '-b',
    }

    if sourceExt == 'blend' then
        table.insert(args, shellQuote(sourcePath))
    else
        table.insert(args, '--factory-startup')
    end

    table.insert(args, '--python')
    table.insert(args, shellQuote(exporterScriptPath))
    table.insert(args, '--')
    table.insert(args, '--input')
    table.insert(args, shellQuote(sourcePath))
    table.insert(args, '--output')
    table.insert(args, shellQuote(outputLuaPath))
    table.insert(args, '--scan-only')

    if options.debugSteps then
        table.insert(args, '--debug-steps')
    end

    local cmd = table.concat(args, ' ')
    debugPrint('build scan cmd: %s', cmd)
    return cmd
end

function M.launchCmdAsync(cmd, logPath)
    local osName = M.getOS()
    debugPrint('launch async (%s): %s', osName, cmd)
    if logPath and logPath ~= '' then
        debugPrint('launch log path: %s', logPath)
    end
    if M.getOS() == 'windows' then
        if logPath and logPath ~= '' then
            os.execute('cmd /c start /b "" ' .. cmd .. ' > ' .. shellQuote(logPath) .. ' 2>&1')
        else
            os.execute('cmd /c start /b "" ' .. cmd)
        end
    else
        if logPath and logPath ~= '' then
            os.execute(cmd .. ' >' .. shellQuote(logPath) .. ' 2>&1 &')
        else
            os.execute(cmd .. ' >/dev/null 2>&1 &')
        end
    end
end

function M.fileExists(path)
    local f = io.open(path, 'rb')
    if not f then return false end
    f:close()
    return true
end

return M
