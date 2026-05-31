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

local function tryDetectExe(exe)
    local f = io.popen(wrapCmd(shellQuote(exe) .. ' --version') .. ' 2>&1')
    if not f then return nil end
    local output = f:read('*a')
    f:close()
    if output and output:find('Blender') then
        local version = output:match('Blender%s+([%d%.]+)') or ''
        return { found = true, path = exe, version = version }
    end
    return nil
end

local WINDOWS_BLENDER_CANDIDATES = {
    'C:\\Program Files\\Blender Foundation\\Blender 4.3\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 4.2\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender 4.1\\blender.exe',
    'C:\\Program Files\\Blender Foundation\\Blender\\blender.exe',
}

function M.detectBlender()
    if M.blender ~= nil then return M.blender end

    if M.customPath and M.customPath ~= '' then
        local r = tryDetectExe(M.customPath)
        if r then M.blender = r; return M.blender end
    end

    local r = tryDetectExe('blender')
    if r then M.blender = r; return M.blender end

    if M.getOS() == 'windows' then
        for _, candidate in ipairs(WINDOWS_BLENDER_CANDIDATES) do
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

    if options.bakeAnimation then
        table.insert(args, '--bake-animation')
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

    return table.concat(args, ' ')
end

function M.launchCmdAsync(cmd)
    if M.getOS() == 'windows' then
        os.execute('cmd /c start /b "" ' .. cmd)
    else
        os.execute(cmd .. ' >/dev/null 2>&1 &')
    end
end

function M.fileExists(path)
    local f = io.open(path, 'rb')
    if not f then return false end
    f:close()
    return true
end

return M
