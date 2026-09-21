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
local HoleGeometry=require 'image_mesh_holes_geometry'
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
function M.texturePaths(project,convert)
    local function options(o)
        for _,key in ipairs{'sideTexture','backTexture'} do
            if o[key] and o[key]~='' then o[key]=convert(o[key]) end
        end
    end
    options(project.defaults)
    for _,r in ipairs(project.regions) do options(r.overrides) end
    for _,preset in ipairs(project.presets or {}) do options(preset.settings) end
end
function M.save(project,path,serialize)
    Model.validate(project)
    local saved=Model.copy(project); saved.image.path=M.relative(project.image.path,path)
    M.texturePaths(saved,function(texture) return M.relative(texture,path) end)
    local lines={}; serialize('project',saved,lines)
    local text=table.concat(lines,'\n')..'\nreturn project\n'
    assert(#text<=4*1024*1024,'ime_project_too_large')
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
    if loaded.presets then
        project.presets={}
        for _,preset in ipairs(loaded.presets) do project.presets[#project.presets+1]={
            name=preset.name,settings=Model.settings(preset.settings)} end
    end
    project.nextId=loaded.nextId; project.defaults=Model.copy(loaded.defaults)
    for _,r in ipairs(loaded.regions) do
        local region={id=r.id,name=r.name,shape=r.shape,x=r.x,y=r.y,w=r.w,h=r.h,overrides=Model.copy(r.overrides)}
        if r.holes then region.holes=Model.copy(r.holes) end
        for _,hole in ipairs(region.holes or {}) do
            if hole.primitive==nil and HoleGeometry.isEllipse(hole) then
                hole.primitive='ellipse';hole.preserveShape=true
            end
        end
        if r.backCrop then region.backCrop={x=r.backCrop.x,y=r.backCrop.y,w=r.backCrop.w,h=r.backCrop.h} end
        if r.heightEdits then
            region.heightEdits={}
            for _,d in ipairs(r.heightEdits) do region.heightEdits[#region.heightEdits+1]={
                x=d.x,y=d.y,radius=d.radius,strength=d.strength,height=d.height,mode=d.mode} end
        end
        if r.shape=='polygon' then region.contour={}; for _,point in ipairs(r.contour) do
            region.contour[#region.contour+1]={x=point.x,y=point.y}
        end end
        project.regions[#project.regions+1]=region
    end
    M.texturePaths(project,function(texture) return M.resolve(texture,path) end)
    return project
end
function M.exportName(region)
    local name=region.name:gsub('[^%w_-]','_'):sub(1,80)
    return string.format('%03d_%s.msh',region.id,name)
end
return M
