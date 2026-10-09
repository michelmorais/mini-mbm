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

]]--

-- Shared by plain Lua tests and engine smoke scenes; no shell or engine required.
local M={}
function M.new()
    local native=os.tmpname()
    local normalized=native:gsub('\\','/')
    local directory=os.getenv('MBM_TEST_TMPDIR')
    if not directory or directory=='' then
        if package.config:sub(1,1)=='\\' then
            directory=os.getenv('TEMP') or os.getenv('TMP') or '.'
        else
            directory=os.getenv('TMPDIR') or normalized:match('^(.*)/') or '.'
        end
    end
    if directory=='' then directory='.' end
    directory=directory:gsub('\\','/'):gsub('/+$','')
    local path=directory..'/'..assert(normalized:match('[^/]+$'))
    -- Reserve/check the destination before dropping Lua's native placeholder.
    local file,err=io.open(path,'wb')
    if path~=normalized then os.remove(native) end
    assert(file,'Cannot create temporary test file: '..path..': '..tostring(err))
    assert(file:close())
    return path
end
return M
