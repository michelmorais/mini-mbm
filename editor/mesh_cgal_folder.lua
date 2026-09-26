--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]


-- Shared folder discovery. Filesystem probes run only on load, selection or refresh.
local M={names={'mbm-cgal-planar','mbm-cgal-remesh','mbm-cgal-repair','mbm-cgal-audit'}}
local preference=os.getenv('MBM_CGAL_FOLDER_CONFIG') or
    ((os.getenv('APPDATA') or os.getenv('HOME') or '.')..'/.mini-mbm-cgal-folder')
local loaded,directory=false,nil
local paths={}
function M.refresh()
    paths={}
    if not directory or directory=='' then return end
    local separator=package.config:sub(1,1)
    local suffix=separator=='\\' and '.exe' or ''
    local prefix=directory
    if not prefix:match('[/\\]$') then prefix=prefix..separator end
    for _,name in ipairs(M.names) do
        local candidate=prefix..name..suffix
        local file=io.open(candidate,'rb')
        if file then
            -- A directory may be openable on Unix but cannot be read as a file.
            local _,err=file:read(1);file:close()
            if not err then paths[name]=candidate end
        end
    end
end
function M.getDirectory()
    if not loaded then
        loaded=true
        local file=io.open(preference,'r')
        if file then directory=file:read('*l');file:close() end
        M.refresh()
    end
    return directory
end
function M.getPath(name)
    if M.getDirectory()==nil then return nil end
    return paths[name] or ''
end
function M.setDirectory(value,persist)
    assert(type(value)=='string' and not value:find('[%z\r\n]'),'Invalid CGAL folder')
    if persist then
        local file,err=io.open(preference,'w');if not file then return nil,err end
        local ok,why=file:write(value,'\n');local closed,closeError=file:close()
        if not ok or not closed then return nil,why or closeError end
    end
    loaded=true;directory=value;M.refresh()
    return true
end
return M
