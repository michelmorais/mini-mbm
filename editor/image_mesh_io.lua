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

local Model=require 'image_mesh_model'
local M={}
function M.directory(path) return path:gsub('\\','/'):match('^(.*)/[^/]*$') or '.' end
local function absolute(path) return path:match('^[/\\]') or path:match('^%a:') end
function M.resolve(path,projectPath)
    if absolute(path) then return path end
    return M.directory(projectPath)..'/'..path
end
function M.relative(path,projectPath)
    local base=M.directory(projectPath):gsub('\\','/')..'/'
    path=path:gsub('\\','/')
    if path:sub(1,#base)==base then return path:sub(#base+1) end
    return path
end
function M.exists(path)
    local f=io.open(path,'rb'); if not f then return false end; f:close(); return true
end
function M.save(project,path,serialize)
    Model.validate(project)
    local saved=Model.copy(project); saved.image.path=M.relative(project.image.path,path)
    local lines={}; serialize('project',saved,lines)
    local text=table.concat(lines,'\n')..'\nreturn project\n'
    local f,err=io.open(path,'wb'); assert(f,err)
    local ok,writeError=f:write(text); local closed,closeError=f:close()
    assert(ok,writeError); assert(closed,closeError)
    return true
end
function M.load(path)
    local f,err=io.open(path,'rb'); assert(f,err)
    local data=f:read(4*1024*1024+1); f:close()
    assert(data and #data<=4*1024*1024,'ime_project_too_large')
    local fn,loadError=load(data,'@'..path,'t',{}); assert(fn,loadError)
    local loaded=fn(); Model.validate(loaded)
    -- Retain only the versioned data schema, never arbitrary extra tables from a file.
    local project=Model.new(M.resolve(loaded.image.path,path),loaded.image.width,loaded.image.height)
    project.nextId=loaded.nextId; project.defaults=Model.copy(loaded.defaults)
    for _,r in ipairs(loaded.regions) do
        local region={id=r.id,name=r.name,shape=r.shape,x=r.x,y=r.y,w=r.w,h=r.h,overrides=Model.copy(r.overrides)}
        if r.shape=='polygon' then region.contour={}; for _,point in ipairs(r.contour) do
            region.contour[#region.contour+1]={x=point.x,y=point.y}
        end end
        project.regions[#project.regions+1]=region
    end
    return project
end
function M.exportName(region)
    local name=region.name:gsub('[^%w_-]','_'):sub(1,80)
    return string.format('%03d_%s.msh',region.id,name)
end
return M
