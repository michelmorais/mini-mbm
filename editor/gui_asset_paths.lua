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
| the Software.                                                                                                        |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Portable GUI project assets, including legacy basename-only saves.
]]--
local P = require 'editor_asset_paths'
local M = {}
local function isAbsolute(path) return path:match('^/') or path:match('^%a:[/\\]') end
function M.saved(path, project, engine)
    if not path or path=='' or path:sub(1,1)=='#' then return path or '' end
    path=P.normalize(path)
    if not isAbsolute(path) and engine.getFullPath then path=engine.getFullPath(path) or path end
    return P.saved(path,project)
end
function M.options(options,project,engine)
    local result={}
    for k,v in pairs(options) do result[k]=v end
    for _,key in ipairs({'sCurrentScriptExecution','sExtraScript'}) do
        if result[key] then result[key]=M.saved(result[key],project,engine) end
    end
    return result
end
function M.write(fp,project,objects,engine)
    local assets={}
    for _,object in ipairs(objects) do
        local path=object.fileName
        if path:sub(1,1)~='#' then
            if not isAbsolute(path) and engine.getFullPath then path=engine.getFullPath(path) or path end
            assets[#assets+1]={fileName=path}
        end
    end
    local paths=P.searchPaths(project,engine.getAllPaths(),assets)
    fp:write('tScene.tAssetPaths = {\n')
    for _,path in ipairs(paths) do fp:write(string.format('    %q,\n',path)) end
    fp:write('}\n')
    fp:write([=[
local _guiSource = debug.getinfo(1, 'S').source:sub(2):gsub('\\', '/')
local _guiBase = _guiSource:match('^(.*)/[^/]*$') or '.'
local function _guiAssetPath(path)
    if not path or path=='' or path:sub(1,1)=='#' then return path end
    path=path:gsub('\\', '/')
    if path:sub(1,1)=='/' or path:match('^%a:/') then return path end
    return _guiBase .. '/' .. path
end
for _,path in ipairs(tScene.tAssetPaths) do mbm.addPath(_guiAssetPath(path)) end
mbm.addPath(_guiBase)
]=])
end
function M.load(project,engine)
    local paths={}
    local function resolve(path)
        if not path or path=='' or path:sub(1,1)=='#' then return path end
        path=P.normalize(path)
        local direct=P.asset(path,project)
        if P.exists(direct) then return direct end
        if not path:find('/',1,true) then
            local found
            for _,dir in ipairs(paths) do
                local candidate=P.normalize(dir..'/'..path)
                if P.exists(candidate) then
                    assert(not found or found==candidate,'Ambiguous GUI asset: '..path)
                    found=candidate
                end
            end
            if found then return found end
        end
        return direct
    end
    local proxy=setmetatable({addPath=function(path)
        local dir=P.asset(path,project)
        paths[#paths+1]=dir
        engine.addPath(dir)
    end}, {__index=engine})
    proxy.include=function(path,...) return engine.include(resolve(path),...) end
    local env=setmetatable({mbm=proxy},{__index=_G})
    local chunk,err=loadfile(project,'t',env)
    assert(chunk,err)
    local scene=chunk()
    assert(type(scene)=='table' and type(scene.tAllMeshInfo)=='table','Invalid GUI project')
    for _,info in ipairs(scene.tAllMeshInfo) do info.fileName=resolve(info.fileName) end
    if scene.getOptionsEditor then
        local options=scene:getOptionsEditor()
        for _,key in ipairs({'sCurrentScriptExecution','sExtraScript'}) do
            if options[key] then options[key]=resolve(options[key]) end
        end
        scene.getOptionsEditor=function() return options end
    end
    engine.addPath(P.directory(project))
    return scene
end
return M
