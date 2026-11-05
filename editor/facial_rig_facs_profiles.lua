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

   Facial Action Unit profile authoring and target-weight evaluation.
]]--

local M={}
local RESPONSES={'linear','smoothstep','ease_in','ease_out'}
local SIDES={bilateral=true,left=true,right=true}

local function finite(value,fallback)
    value=tonumber(value)
    if not value or value~=value or value==math.huge or value==-math.huge then return fallback or 0 end
    return value
end

local function clamp(value,low,high)
    return math.max(low,math.min(high,finite(value,low)))
end

local function trimmed(value)
    return type(value)=='string' and value:match('^%s*(.-)%s*$') or ''
end

local function validResponse(response)
    for _,candidate in ipairs(RESPONSES) do
        if response==candidate then return true end
    end
    return false
end

local function actionUnitExists(actionUnits,actionUnit)
    if type(actionUnits)~='table' then return true end
    for _,candidate in ipairs(actionUnits) do
        if candidate==actionUnit then return true end
    end
    return false
end

local function copyOutput(raw)
    local output={target=trimmed(raw.target),weight=clamp(raw.weight,0,1),
        response=validResponse(raw.response) and raw.response or 'linear',
        secondary=raw.secondary==true}
    if raw.clamp~=nil then output.clamp=clamp(raw.clamp,0,1) end
    return output
end

function M.new()
    return {profiles={},selectedProfile=1,composition='add_saturate'}
end

function M.normalize(data)
    if type(data)~='table' then data=M.new() end
    data.composition=data.composition=='max' and 'max' or 'add_saturate'
    local profiles,ids={},{}
    for index,raw in ipairs(type(data.profiles)=='table' and data.profiles or {}) do
        if type(raw)=='table' then
            local profile={
                id=trimmed(raw.id),
                name=trimmed(raw.name),
                actionUnit=trimmed(raw.actionUnit):upper(),
                side=SIDES[raw.side] and raw.side or 'bilateral',
                enabled=raw.enabled~=false,
                intensityMin=clamp(raw.intensityMin,0,1),
                intensityMax=clamp(raw.intensityMax,0,1),
                previewIntensity=clamp(raw.previewIntensity,0,1),
                outputs={}
            }
            if profile.id=='' or ids[profile.id] then profile.id='facs_profile_'..index end
            local baseId=profile.id
            local suffix=2
            while ids[profile.id] do profile.id=baseId..'_'..suffix; suffix=suffix+1 end
            ids[profile.id]=true
            if profile.name=='' then profile.name=profile.actionUnit~='' and profile.actionUnit or profile.id end
            if profile.intensityMax<=profile.intensityMin then
                profile.intensityMin=0
                profile.intensityMax=1
            end
            profile.previewIntensity=clamp(profile.previewIntensity,profile.intensityMin,profile.intensityMax)
            local seenTargets={}
            for _,candidate in ipairs(type(raw.outputs)=='table' and raw.outputs or {}) do
                if type(candidate)=='table' then
                    local output=copyOutput(candidate)
                    if output.target~='' and not seenTargets[output.target] then
                        seenTargets[output.target]=true
                        profile.outputs[#profile.outputs+1]=output
                    end
                end
            end
            profiles[#profiles+1]=profile
        end
    end
    data.profiles=profiles
    data.selectedProfile=math.max(1,math.min(#profiles>0 and #profiles or 1,
        math.floor(finite(data.selectedProfile,1))))
    return data
end

function M.addProfile(data,name,actionUnit,side,actionUnits)
    data=M.normalize(data)
    name=trimmed(name)
    actionUnit=trimmed(actionUnit):upper()
    side=SIDES[side] and side or 'bilateral'
    if name=='' then return nil,'empty_name' end
    if actionUnit=='' then return nil,'empty_action_unit' end
    if not actionUnitExists(actionUnits,actionUnit) then return nil,'unsupported_action_unit' end
    for _,profile in ipairs(data.profiles) do
        if profile.actionUnit==actionUnit and profile.side==side then return nil,'duplicate_action_side' end
        if profile.name:lower()==name:lower() then return nil,'duplicate_name' end
    end
    local ids={}
    for _,profile in ipairs(data.profiles) do ids[profile.id]=true end
    local baseId='facs_profile_'..tostring(#data.profiles+1)
    local id=baseId
    local suffix=2
    while ids[id] do id=baseId..'_'..suffix; suffix=suffix+1 end
    local profile={id=id,name=name,actionUnit=actionUnit,side=side,enabled=true,
        intensityMin=0,intensityMax=1,previewIntensity=0,outputs={}}
    data.profiles[#data.profiles+1]=profile
    data.selectedProfile=#data.profiles
    return profile
end

function M.updateProfile(data,index,changes,actionUnits)
    data=M.normalize(data)
    local profile=data.profiles[index]
    if not profile then return false,'missing_profile' end
    changes=type(changes)=='table' and changes or {}
    local name=trimmed(changes.name~=nil and changes.name or profile.name)
    local actionUnit=trimmed(changes.actionUnit~=nil and changes.actionUnit or profile.actionUnit):upper()
    local side=SIDES[changes.side] and changes.side or profile.side
    local intensityMin=clamp(changes.intensityMin~=nil and changes.intensityMin or profile.intensityMin,0,1)
    local intensityMax=clamp(changes.intensityMax~=nil and changes.intensityMax or profile.intensityMax,0,1)
    if name=='' then return false,'empty_name' end
    if actionUnit=='' then return false,'empty_action_unit' end
    if not actionUnitExists(actionUnits,actionUnit) then return false,'unsupported_action_unit' end
    if intensityMax<=intensityMin then return false,'invalid_range' end
    for candidateIndex,candidate in ipairs(data.profiles) do
        if candidateIndex~=index then
            if candidate.name:lower()==name:lower() then return false,'duplicate_name' end
            if candidate.actionUnit==actionUnit and candidate.side==side then
                return false,'duplicate_action_side'
            end
        end
    end
    profile.name=name
    profile.actionUnit=actionUnit
    profile.side=side
    profile.intensityMin=intensityMin
    profile.intensityMax=intensityMax
    profile.previewIntensity=clamp(changes.previewIntensity~=nil and changes.previewIntensity or
        profile.previewIntensity,intensityMin,intensityMax)
    if changes.enabled~=nil then profile.enabled=changes.enabled==true end
    return true,profile
end

function M.removeProfile(data,index)
    data=M.normalize(data)
    if index<1 or index>#data.profiles then return false end
    table.remove(data.profiles,index)
    data.selectedProfile=math.max(1,math.min(data.selectedProfile,#data.profiles>0 and #data.profiles or 1))
    return true
end

function M.addOutput(data,profileIndex,targetName)
    data=M.normalize(data)
    local profile=data.profiles[profileIndex]
    targetName=trimmed(targetName)
    if not profile or targetName=='' then return false,'invalid_output' end
    for _,output in ipairs(profile.outputs) do
        if output.target==targetName then return false,'duplicate_target' end
    end
    profile.outputs[#profile.outputs+1]={target=targetName,weight=1,response='linear',secondary=false}
    return true
end

function M.updateOutput(data,profileIndex,outputIndex,changes)
    data=M.normalize(data)
    local profile=data.profiles[profileIndex]
    local output=profile and profile.outputs[outputIndex]
    if not output then return false,'missing_output' end
    changes=type(changes)=='table' and changes or {}
    if changes.weight~=nil then output.weight=clamp(changes.weight,0,1) end
    if changes.response~=nil then
        if not validResponse(changes.response) then return false,'invalid_response' end
        output.response=changes.response
    end
    if changes.secondary~=nil then output.secondary=changes.secondary==true end
    if changes.clampEnabled==false then
        output.clamp=nil
    elseif changes.clampEnabled==true then
        output.clamp=clamp(changes.clamp~=nil and changes.clamp or output.clamp or 1,0,1)
    end
    return true,output
end

function M.removeOutput(data,profileIndex,outputIndex)
    data=M.normalize(data)
    local profile=data.profiles[profileIndex]
    if not profile or outputIndex<1 or outputIndex>#profile.outputs then return false end
    table.remove(profile.outputs,outputIndex)
    return true
end

function M.validate(data,targetNames,actionUnits)
    data=type(data)=='table' and data or {}
    local profiles=type(data.profiles)=='table' and data.profiles or {}
    local known={}
    for _,entry in ipairs(targetNames or {}) do
        local name=type(entry)=='table' and entry.name or entry
        if type(name)=='string' and name~='' then known[name]=true end
    end
    local report={ok=true,errors={},warnings={},profileCount=#profiles,outputCount=0}
    local profileNames={}
    local profileActions={}
    for _,profile in ipairs(profiles) do
        if type(profile)=='table' then
            local profileName=type(profile.name)=='string' and profile.name or ''
            local actionUnit=type(profile.actionUnit)=='string' and profile.actionUnit or ''
            local side=SIDES[profile.side] and profile.side or 'bilateral'
            local nameKey=profileName:lower()
            if profileNames[nameKey] then
                report.ok=false
                report.errors[#report.errors+1]={code='duplicate_name',profile=profileName}
            end
            profileNames[nameKey]=true
            local actionKey=actionUnit..'|'..side
            if profileActions[actionKey] then
                report.ok=false
                report.errors[#report.errors+1]={code='duplicate_action_side',profile=profileName}
            end
            profileActions[actionKey]=true
            if actionUnit=='' then
                report.ok=false
                report.errors[#report.errors+1]={code='missing_action_unit',profile=profileName}
            elseif not actionUnitExists(actionUnits,actionUnit) then
                report.ok=false
                report.errors[#report.errors+1]={code='unsupported_action_unit',profile=profileName,
                    actionUnit=actionUnit}
            end
            local outputs=type(profile.outputs)=='table' and profile.outputs or {}
            if #outputs==0 then
                report.warnings[#report.warnings+1]={code='no_outputs',profile=profileName}
            end
            for _,output in ipairs(outputs) do
                if type(output)=='table' then
                    report.outputCount=report.outputCount+1
                    local target=type(output.target)=='string' and output.target or ''
                    if not known[target] then
                        report.ok=false
                        report.errors[#report.errors+1]={code='missing_target',profile=profileName,target=target}
                    end
                end
            end
        end
    end
    return report
end

local function applyResponse(value,response)
    value=clamp(value,0,1)
    if response=='smoothstep' then return value*value*(3-2*value) end
    if response=='ease_in' then return value*value end
    if response=='ease_out' then return 1-(1-value)*(1-value) end
    return value
end

function M.evaluate(data,targetNames,actionUnits)
    data=M.normalize(data)
    local report=M.validate(data,targetNames,actionUnits)
    local weights,known={},{}
    for _,entry in ipairs(targetNames or {}) do
        local name=type(entry)=='table' and entry.name or entry
        if type(name)=='string' and name~='' then known[name]=true; weights[name]=0 end
    end
    if not report.ok then return weights,report end
    local contributions={}
    for _,profile in ipairs(data.profiles) do
        if profile.enabled and profile.previewIntensity>profile.intensityMin then
            local amount=(profile.previewIntensity-profile.intensityMin)/
                (profile.intensityMax-profile.intensityMin)
            amount=clamp(amount,0,1)
            for _,output in ipairs(profile.outputs) do
                if known[output.target] then
                    local contribution=applyResponse(amount,output.response)*output.weight
                    if output.clamp~=nil then contribution=math.min(contribution,output.clamp) end
                    local current=contributions[output.target] or 0
                    if data.composition=='max' then
                        contributions[output.target]=math.max(current,contribution)
                    else
                        contributions[output.target]=current+contribution
                    end
                end
            end
        end
    end
    for target,value in pairs(contributions) do weights[target]=clamp(value,0,1) end
    return weights,report
end

return M
