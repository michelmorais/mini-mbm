--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to     |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Persistence model for generic surface-region projects.
]]--

local tRegions=require 'morph_surface_regions'
local M={}

function M.new()
    return {version=1,meshPath='',topologySignature='',regions={},selectedRegion=1}
end

function M.normalize(project)
    if type(project)~='table' then return nil,'invalid_project' end
    project.version=1
    project.meshPath=type(project.meshPath)=='string' and project.meshPath or ''
    project.topologySignature=type(project.topologySignature)=='string' and
        project.topologySignature or ''
    local regions,ids={},{}
    for _,region in ipairs(type(project.regions)=='table' and project.regions or {}) do
        if type(region)=='table' then
            region.id=type(region.id)=='string' and region.id~='' and region.id or
                ('region_'..(#regions+1))
            if ids[region.id] then
                local base=region.id
                local suffix=2
                repeat region.id=base..'_'..suffix; suffix=suffix+1 until not ids[region.id]
            end
            ids[region.id]=true
            tRegions.normalize(region)
            regions[#regions+1]=region
        end
    end
    project.regions=regions
    project.selectedRegion=math.max(1,math.min(#regions>0 and #regions or 1,
        math.floor(tonumber(project.selectedRegion) or 1)))
    return project
end

function M.save(project,path,serializer)
    if type(serializer)~='function' then return false,'serializer_missing' end
    local normalized,errorCode=M.normalize(project)
    if not normalized then return false,errorCode end
    local lines={'-- mini-mbm generic morph surface project','local morphProject = {}'}
    serializer('morphProject',normalized,lines)
    lines[#lines+1]='return morphProject'
    local file,errorMessage=io.open(path,'w')
    if not file then return false,errorMessage end
    local ok,writeError=file:write(table.concat(lines,'\n'),'\n')
    local closeOk,closeError=file:close()
    if not ok or not closeOk then return false,writeError or closeError end
    return true
end

function M.load(path)
    local chunk,errorMessage=loadfile(path)
    if not chunk then return nil,errorMessage end
    local ok,project=pcall(chunk)
    if not ok then return nil,project end
    return M.normalize(project)
end

return M
