--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,   |
| OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                |
|------------------------------------------------------------------------------------------------------------------------|

   Facial expression authoring model.

   This module deliberately stores only named target weights and expression poses. It does not
   duplicate mesh deltas; those remain in the MSH facial sections and are sampled by the runtime.
]]--

local M = {}

local function finite(value, fallback)
    value=tonumber(value)
    if not value or value ~= value or value == math.huge or value == -math.huge then
        return fallback or 0
    end
    return value
end

local function clamp(value, low, high)
    value=finite(value,0)
    return math.max(low,math.min(high,value))
end

local function copyWeights(weights)
    local result={}
    for name,value in pairs(type(weights)=='table' and weights or {}) do
        if type(name)=='string' then result[name]=clamp(value,0,1) end
    end
    return result
end

local function defaultState()
    return {targets={},poses={},selectedTarget=1,selectedPose=1}
end

function M.new()
    return defaultState()
end

function M.normalize(expressions)
    if type(expressions)~='table' then expressions=defaultState() end
    expressions.targets=type(expressions.targets)=='table' and expressions.targets or {}
    expressions.poses=type(expressions.poses)=='table' and expressions.poses or {}
    expressions.selectedTarget=math.max(1,math.floor(finite(expressions.selectedTarget,1)))
    expressions.selectedPose=math.max(1,math.floor(finite(expressions.selectedPose,1)))
    local targets,seen={},{ }
    for _,entry in ipairs(expressions.targets) do
        if type(entry)=='table' and type(entry.name)=='string' and entry.name~='' and not seen[entry.name] then
            seen[entry.name]=true
            targets[#targets+1]={name=entry.name,convention=type(entry.convention)=='string' and entry.convention or '',
                defaultWeight=clamp(entry.defaultWeight,0,1),enabled=entry.enabled~=false}
        end
    end
    expressions.targets=targets
    local poses={}
    for index,pose in ipairs(expressions.poses) do
        if type(pose)=='table' then
            local name=type(pose.name)=='string' and pose.name:match('^%s*(.-)%s*$') or ''
            if name=='' then name='expression_' .. index end
            poses[#poses+1]={name=name,weights=copyWeights(pose.weights),notes=type(pose.notes)=='string' and pose.notes or ''}
        end
    end
    expressions.poses=poses
    expressions.selectedTarget=math.min(expressions.selectedTarget,math.max(1,#targets))
    expressions.selectedPose=math.min(expressions.selectedPose,math.max(1,#poses))
    return expressions
end

function M.catalogFromMesh(mesh)
    local result={}
    if not mesh then return result end
    local count=mesh:getTotalFacialTargets()
    if not count then return result end
    for index=1,tonumber(count) or 0 do
        local name=mesh:getFacialTargetName(index)
        if type(name)=='string' and name~='' then
            local info=mesh:getFacialTargetInfo(index) or {}
            result[#result+1]={name=name,convention=info.convention or '',
                defaultWeight=clamp(info.defaultWeight,0,1),enabled=true}
        end
    end
    return result
end

function M.syncCatalog(expressions,catalog)
    expressions=M.normalize(expressions)
    local previous={}
    for _,entry in ipairs(expressions.targets) do previous[entry.name]=entry end
    local targets={}
    for _,entry in ipairs(catalog or {}) do
        if type(entry)=='table' and type(entry.name)=='string' and entry.name~='' then
            local old=previous[entry.name]
            targets[#targets+1]={name=entry.name,convention=entry.convention or '',
                defaultWeight=clamp(entry.defaultWeight,0,1),enabled=old and old.enabled~=false or true}
        end
    end
    expressions.targets=targets
    local valid={}
    for _,entry in ipairs(targets) do valid[entry.name]=true end
    for _,pose in ipairs(expressions.poses) do
        local weights={}
        for name,value in pairs(pose.weights or {}) do if valid[name] then weights[name]=clamp(value,0,1) end end
        pose.weights=weights
    end
    return expressions
end

function M.targetNames(expressions)
    local names={}
    for _,entry in ipairs(M.normalize(expressions).targets) do names[#names+1]=entry.name end
    return names
end

function M.addPose(expressions,name)
    expressions=M.normalize(expressions)
    name=type(name)=='string' and name:match('^%s*(.-)%s*$') or ''
    if name=='' then return nil,'expression name is empty' end
    for _,pose in ipairs(expressions.poses) do if pose.name==name then return nil,'duplicate expression name' end end
    local pose={name=name,weights={},notes=''}
    expressions.poses[#expressions.poses+1]=pose
    expressions.selectedPose=#expressions.poses
    return pose
end

function M.removePose(expressions,index)
    if type(expressions)~='table' or type(expressions.poses)~='table' or
            index<1 or index>#expressions.poses then return false end
    table.remove(expressions.poses,index)
    expressions.selectedPose=math.max(1,math.min(expressions.selectedPose or 1,math.max(1,#expressions.poses)))
    return true
end

function M.capturePose(expressions,pose,weights)
    if not pose then return false end
    pose.weights=copyWeights(weights)
    return true
end

function M.applyPose(expressions,pose)
    if not pose then return {} end
    local result={}
    for _,entry in ipairs(expressions.targets or {}) do result[entry.name]=0 end
    for name,value in pairs(pose.weights or {}) do result[name]=clamp(value,0,1) end
    return result
end

local function pairedName(name)
    local base,side=name:match('^(.*)([Ll]eft)$')
    if base then return base .. (side=='Left' and 'Right' or 'left') end
    base,side=name:match('^(.*)([Rr]ight)$')
    if base then return base .. (side=='Right' and 'Left' or 'left') end
    local prefix,separator,side=name:match('^(.*)([_%.%-])([LR])$')
    if prefix then return prefix .. separator .. (side=='L' and 'R' or 'L') end
    base,side=name:match('^(.*)_([lr])$')
    if base then return base .. '_' .. (side=='l' and 'r' or 'l') end
    return nil
end

function M.mirrorPose(expressions,pose)
    if not pose then return false,0 end
    local known={}
    for _,entry in ipairs(expressions.targets or {}) do known[entry.name]=true end
    local mirrored={}
    for name,value in pairs(pose.weights or {}) do
        local opposite=pairedName(name)
        if opposite and known[opposite] then mirrored[opposite]=value end
    end
    for name,value in pairs(mirrored) do pose.weights[name]=clamp(value,0,1) end
    return true,0
end

function M.validate(expressions)
    expressions=M.normalize(expressions)
    local report={ok=true,errors={},warnings={},targetCount=#expressions.targets,poseCount=#expressions.poses}
    local targets={}
    for _,entry in ipairs(expressions.targets) do targets[entry.name]=true end
    local poses={}
    for _,pose in ipairs(expressions.poses) do
        if poses[pose.name] then report.ok=false; report.errors[#report.errors+1]='duplicate expression: ' .. pose.name end
        poses[pose.name]=true
        for name,value in pairs(pose.weights or {}) do
            if not targets[name] then report.ok=false; report.errors[#report.errors+1]='expression references unknown target: ' .. name end
            if value<0 or value>1 then report.ok=false; report.errors[#report.errors+1]='expression weight is outside 0..1: ' .. name end
        end
    end
    if #expressions.targets==0 then report.warnings[#report.warnings+1]='mesh has no facial targets' end
    return report
end

return M
