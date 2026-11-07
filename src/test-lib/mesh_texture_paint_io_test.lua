--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                           |
|                                                                                                                       |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated            |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation         |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and         |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                      |
|                                                                                                                       |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                         |
|                                                                                                                       |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE    |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR   |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR         |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        |
|-----------------------------------------------------------------------------------------------------------------------|
]]--

package.path = 'src/test-lib/?.lua;editor/?.lua;' .. package.path
local PaintIO = require 'mesh_texture_paint_io'
local temp = require('test_temp_path').new()
os.remove(temp)
local paths = {temp .. '.png', temp .. '.lua'}
local rename = os.rename
local tracked, backups

local function read(path)
    local file = io.open(path, 'rb')
    if not file then return nil end
    local bytes = assert(file:read('*a'))
    assert(file:close())
    return bytes
end
local function write(path, bytes)
    local file = assert(io.open(path, 'wb'))
    assert(file:write(bytes)); assert(file:close())
    return true
end
local function reset(existing)
    tracked, backups = {}, {}
    for i, path in ipairs(paths) do
        os.remove(path)
        if existing then write(path, 'old-' .. i) end
    end
end
local function outputs(failWriter, kind)
    local result = {}
    for i, path in ipairs(paths) do
        result[i] = {path=path, write=function(temporary)
            tracked[temporary] = true
            write(temporary, 'new-' .. i)
            if i == failWriter then
                if kind == 'throw' then error('injected encoder failure') end
                if kind == 'false' then return false, 'injected write failure' end
                return nil, 'injected write failure'
            end
            return true
        end}
    end
    return result
end
local function checkOld(existing)
    for i, path in ipairs(paths) do assert(read(path) == (existing and ('old-' .. i) or nil)) end
    for path in pairs(tracked) do assert(not read(path), 'Temporary or backup leaked: ' .. path) end
end
local function interceptRename(failAt, failRestore)
    local count = 0
    os.rename = function(from, to)
        count = count + 1
        if to:sub(-4) == '.bak' then tracked[to] = true; backups[from] = to end
        if count == failAt or (failRestore and from == backups[paths[1]]) then
            return nil, 'injected rename failure'
        end
        -- Exercise Windows replacement semantics even when running on POSIX.
        assert(not read(to), 'Attempted to rename over an existing file')
        return rename(from, to)
    end
end

local ok, err = xpcall(function()
    for _, kind in ipairs({'false', 'nil', 'throw'}) do
        for index = 1, 2 do
            reset(true)
            assert(not PaintIO.publish(outputs(index, kind), pcall))
            checkOld(true)
        end
    end
    for failure = 1, 4 do
        reset(true); interceptRename(failure)
        assert(not PaintIO.publish(outputs(), pcall))
        checkOld(true)
    end
    reset(false); interceptRename(2)
    assert(not PaintIO.publish(outputs(), pcall))
    checkOld(false)

    reset(true); interceptRename()
    assert(PaintIO.publish(outputs(), pcall))
    for i, path in ipairs(paths) do assert(read(path) == 'new-' .. i) end
    for path in pairs(tracked) do assert(not read(path), 'Temporary or backup leaked: ' .. path) end

    reset(true); interceptRename(4, true)
    local saved, errorMessage = PaintIO.publish(outputs(), pcall)
    assert(not saved and errorMessage:find(backups[paths[1]], 1, true), 'Missing recovery path')
    assert(read(backups[paths[1]]) == 'old-1', 'Recovery backup was lost')
    assert(read(paths[2]) == 'old-2')
    for path in pairs(tracked) do os.remove(path) end

    os.rename = rename
    reset(true)
    local duplicate = outputs()
    duplicate[2].path = duplicate[1].path
    assert(not PaintIO.publish(duplicate, pcall))
    checkOld(true)
end, debug.traceback)
os.rename = rename
for path in pairs(tracked or {}) do os.remove(path) end
for _, path in ipairs(paths) do os.remove(path) end
assert(ok, err)
print('MESH TEXTURE PAINT IO TEST OK')
