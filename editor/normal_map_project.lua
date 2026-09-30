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

local Height=require 'height_map_source'
local M={}
local function directory(path) return path:gsub('\\','/'):match('^(.*)/[^/]*$') or '.' end
local function absolute(path) return path:match('^[/\\]') or path:match('^%a:') end
function M.save(path,source,options)
    local o=Height.settings(options)
    assert(type(source)=='string' and absolute(source),'Source must be an absolute path')
    local base=directory(path)..'/'
    source=source:gsub('\\','/')
    if source:sub(1,#base)==base then source=source:sub(#base+1) end
    local lines={'normal-map-project 1','source='..source:gsub('.',function(c) return string.format('%02x',c:byte()) end)}
    local keys={};for key in pairs(Height.defaults) do keys[#keys+1]=key end;table.sort(keys)
    for _,key in ipairs(keys) do lines[#lines+1]=key..'='..tostring(o[key]) end
    local f,err=io.open(path,'wb');assert(f,err)
    local ok,message=f:write(table.concat(lines,'\n')..'\n');local closed,closeError=f:close()
    assert(ok,message);assert(closed,closeError)
end
function M.load(path)
    local f,err=io.open(path,'rb');assert(f,err)
    local data=f:read(16385);f:close();assert(#data<=16384,'Project too large')
    local lines={};for line in data:gmatch('[^\n]+') do lines[#lines+1]=line:gsub('\r$','') end
    assert(lines[1]=='normal-map-project 1','Unsupported project version')
    local hex=assert(lines[2] and lines[2]:match('^source=([%x]+)$'),'Missing source')
    assert(#hex%2==0,'Invalid source')
    local source=hex:gsub('%x%x',function(pair) return string.char(tonumber(pair,16)) end)
    assert(not source:find('%z'),'Invalid source')
    local options={};local seen={}
    for i=3,#lines do
        local key,value=lines[i]:match('^(%w+)=(.*)$')
        assert(key and Height.defaults[key]~=nil and not seen[key],'Invalid project setting')
        seen[key]=true
        local kind=type(Height.defaults[key])
        if kind=='number' then options[key]=assert(tonumber(value),'Invalid number')
        elseif kind=='boolean' then assert(value=='true' or value=='false');options[key]=value=='true'
        else options[key]=value end
    end
    if not absolute(source) then source=directory(path)..'/'..source end
    return source,Height.settings(options)
end
return M
