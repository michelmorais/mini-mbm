--[[
----------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                         |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                          |
|                                                                                                                           |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated              |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation           |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and          |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                        |
|                                                                                                                           |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of          |
| the Software.                                                                                                             |
|                                                                                                                           |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE      |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR     |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR          |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.          |
|---------------------------------------------------------------------------------------------------------------------------|

]]--

-- Stage all outputs beside their destinations, then publish with rollback on
-- reported I/O failures. This is not a crash-atomic multi-file filesystem commit.
local M = {}

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then print('[mesh_texture_paint_io] ' .. tostring(result[2])) end
    return table.unpack(result, 1, result.n)
end

local function exists(path)
    local file, err, code = io.open(path, 'rb')
    if not file then
        if code == 2 then return false end -- ENOENT; other errors must not imply an absent destination
        error(err, 0)
    end
    local _, readError = file:read(1) -- reject directories even when fopen accepts them
    local closed, closeError = file:close()
    assert(not readError and closed, readError or closeError)
    return true
end

local function uniquePaths(path)
    while true do
        local native = os.tmpname()
        -- POSIX reserves a file; Windows may return only a root-relative name.
        if package.config:sub(1, 1) ~= '\\' then os.remove(native) end
        local token = assert(native:match('[^/\\]+$'))
        local stem = path .. '.meshpaint-' .. token
        local temporary = stem .. '.tmp' .. (path:match('(%.[^./\\]+)$') or '')
        local backup = stem .. '.bak'
        if not exists(temporary) and not exists(backup) then return temporary, backup end
    end
end

local function removeFile(path, errors)
    local removed, err = os.remove(path)
    if not removed then errors[#errors + 1] = path .. ': ' .. tostring(err) end
    return removed
end

-- Each writer must return true only after closing/flushing its complete file.
-- A failed rollback retains the backup and returns its path for recovery.
function M.publish(outputs, call)
    call = call or dpCall
    local files = {}
    local ok, err = call(function()
        assert(type(outputs) == 'table' and #outputs > 0, 'outputs are required')
        local seen = {}
        for _, output in ipairs(outputs) do
            assert(type(output.path) == 'string' and output.path ~= '', 'output path is required')
            local key = package.config:sub(1, 1) == '\\' and output.path:lower() or output.path
            assert(not seen[key], 'duplicate output path: ' .. output.path)
            seen[key] = true
            local temporary, backup = uniquePaths(output.path)
            local file = {path=output.path, temporary=temporary, backup=backup}
            files[#files + 1] = file
            local written, writeError = output.write(temporary)
            assert(written == true, writeError or ('could not write ' .. output.path))
            assert(exists(temporary), 'writer did not create ' .. temporary)
        end
        for _, file in ipairs(files) do
            if exists(file.path) then
                assert(os.rename(file.path, file.backup))
                file.backedUp = true
            end
            assert(os.rename(file.temporary, file.path))
            file.published = true
        end
    end)
    if not ok then
        local recoveryErrors = {}
        for i = #files, 1, -1 do
            local file = files[i]
            local cleared = not file.published or removeFile(file.path, recoveryErrors)
            if file.backedUp then
                local restored, restoreError
                if cleared then restored, restoreError = os.rename(file.backup, file.path) end
                if not restored then
                    recoveryErrors[#recoveryErrors + 1] = 'Backup: ' .. file.backup ..
                        ' (' .. tostring(restoreError or 'destination could not be removed') .. ')'
                end
            end
            os.remove(file.temporary)
        end
        local message = tostring(err)
        if #recoveryErrors > 0 then message = message .. ' / Restore: ' .. table.concat(recoveryErrors, '; ') end
        return false, message
    end
    local cleanupErrors = {}
    for _, file in ipairs(files) do
        if file.backedUp then removeFile(file.backup, cleanupErrors) end
    end
    return true, #cleanupErrors > 0 and ('Cleanup: ' .. table.concat(cleanupErrors, '; ')) or nil
end

return M
