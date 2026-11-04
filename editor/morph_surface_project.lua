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
local tTargets=require 'morph_surface_targets'
local M={}

local PROJECT_VERSION=4

local function validNumber(value,default)
    local number=tonumber(value)
    if not number or number~=number or number==math.huge or number==-math.huge then
        return default
    end
    return number
end

local function clamp(value,minValue,maxValue)
    return math.max(minValue,math.min(maxValue,value))
end

local function normalizeLanguage(language)
    return language=='pt_br' and 'pt_br' or 'en'
end

local function normalizePreferences(preferences)
    preferences=type(preferences)=='table' and preferences or {}
    local camera=type(preferences.camera)=='table' and preferences.camera or nil
    local normalized={
        handleScale=clamp(validNumber(preferences.handleScale,1),0.05,5),
        surfaceAxis=math.floor(clamp(validNumber(preferences.surfaceAxis,3),1,3)),
        shapeMode=math.floor(clamp(validNumber(preferences.shapeMode,1),1,3)),
        worktree=math.floor(clamp(validNumber(preferences.worktree,1),1,2)),
        targetMode=math.floor(clamp(validNumber(preferences.targetMode,1),1,4)),
        newShapeType=math.floor(clamp(validNumber(preferences.newShapeType,1),1,3)),
        newShapeFree=preferences.newShapeFree==true,
        nameDraft=type(preferences.nameDraft)=='string' and preferences.nameDraft~='' and
            preferences.nameDraft or 'region',
        targetNameDraft=type(preferences.targetNameDraft)=='string' and
            preferences.targetNameDraft~='' and preferences.targetNameDraft or 'target',
        targetDx=validNumber(preferences.targetDx,0),
        targetDy=validNumber(preferences.targetDy,0),
        targetDz=validNumber(preferences.targetDz,0),
        targetEditMode=math.floor(clamp(validNumber(preferences.targetEditMode,1),1,3)),
        targetRx=validNumber(preferences.targetRx,0),
        targetRy=validNumber(preferences.targetRy,0),
        targetRz=validNumber(preferences.targetRz,0),
        targetFeatherRings=math.floor(clamp(validNumber(preferences.targetFeatherRings,0),0,8)),
        targetBrushMode=math.floor(clamp(validNumber(preferences.targetBrushMode,1),1,2)),
        targetBrushRadius=clamp(validNumber(preferences.targetBrushRadius,0.025),0.001,0.15),
        targetBrushStrength=clamp(validNumber(preferences.targetBrushStrength,0.5),0.01,1),
        targetBrushDx=validNumber(preferences.targetBrushDx,0),
        targetBrushDy=validNumber(preferences.targetBrushDy,-0.01),
        targetBrushDz=validNumber(preferences.targetBrushDz,0),
        targetPreviewWeight=clamp(validNumber(preferences.targetPreviewWeight,1),0,1),
        targetPreviewVisible=preferences.targetPreviewVisible~=false,
        meshVisible=preferences.meshVisible~=false,
    }
    if camera then
        normalized.camera={
            fx=validNumber(camera.fx,0),
            fy=validNumber(camera.fy,0),
            fz=validNumber(camera.fz,0),
            distance=math.max(validNumber(camera.distance,10),0.1),
            azimuth=validNumber(camera.azimuth,0.45),
            elevation=clamp(validNumber(camera.elevation,0.22),-1.45,1.45),
        }
    end
    return normalized
end

function M.new(language)
    return {
        version=PROJECT_VERSION,
        language=normalizeLanguage(language),
        preferences=normalizePreferences(),
        meshPath='',topologySignature='',regions={},selectedRegion=1,targets={},selectedTarget=1,
    }
end

function M.normalize(project)
    if type(project)~='table' then return nil,'invalid_project' end
    if tonumber(project.version)~=PROJECT_VERSION then
        return nil,'unsupported_morph_project_version: '..tostring(project.version)
    end
    project.version=PROJECT_VERSION
    project.language=normalizeLanguage(project.language)
    project.preferences=normalizePreferences(project.preferences)
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
    local targets,targetIds={},{}
    for _,target in ipairs(type(project.targets)=='table' and project.targets or {}) do
        if type(target)=='table' then
            target.id=type(target.id)=='string' and target.id~='' and target.id or
                ('target_'..(#targets+1))
            if targetIds[target.id] then
                local base=target.id
                local suffix=2
                repeat target.id=base..'_'..suffix; suffix=suffix+1 until not targetIds[target.id]
            end
            targetIds[target.id]=true
            tTargets.normalize(target)
            targets[#targets+1]=target
        end
    end
    project.targets=targets
    project.selectedTarget=math.max(1,math.min(#targets>0 and #targets or 1,
        math.floor(tonumber(project.selectedTarget) or 1)))
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
