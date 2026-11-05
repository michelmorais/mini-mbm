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
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        |
|------------------------------------------------------------------------------------------------------------------------|

   Generic Morph clip authoring model.

   A clip is a time-ordered set of keys. Each key stores stable Morph Target IDs and normalized
   weights. Expressions can be copied into keys by the editor, but the runtime representation stays
   generic and does not depend on FACS metadata.
]]--

local M={}

local function finite(value,default)
    value=tonumber(value)
    if not value or value~=value or value==math.huge or value==-math.huge then return default or 0 end
    return value
end

local function clamp(value)
    return math.max(0,math.min(1,finite(value,0)))
end

local function trim(value)
    return (type(value)=='string' and value or ''):match('^%s*(.-)%s*$')
end

local function copyWeights(weights)
    local result={}
    if type(weights)~='table' then return result end
    if #weights>0 then
        for _,entry in ipairs(weights) do
            if type(entry)=='table' then
                local id=trim(entry.targetId)
                local value=clamp(entry.weight)
                if id~='' and value>1e-8 then
                    result[#result+1]={targetId=id,targetName=trim(entry.targetName),weight=value}
                end
            end
        end
    else
        for id,value in pairs(weights) do
            id=trim(id)
            value=clamp(value)
            if id~='' and value>1e-8 then
                result[#result+1]={targetId=id,targetName='',weight=value}
            end
        end
    end
    table.sort(result,function(left,right) return left.targetId<right.targetId end)
    return result
end

local function copyKey(key)
    return {time=math.max(0,finite(key and key.time,0)),weights=copyWeights(key and key.weights)}
end

function M.new(name,id)
    return {id=trim(id),name=trim(name),duration=1,fps=30,loop=true,interpolation=1,
        keys={{time=0,weights={}},{time=1,weights={}}}}
end

function M.normalize(clip,index)
    if type(clip)~='table' then return nil,'invalid_clip' end
    clip.id=trim(clip.id)
    clip.name=trim(clip.name)
    if clip.name=='' then clip.name='clip_'..tostring(index or 1) end
    clip.duration=math.max(finite(clip.duration,1),0.001)
    clip.fps=math.max(finite(clip.fps,30),1)
    clip.loop=clip.loop~=false
    clip.interpolation=clip.interpolation==0 and 0 or 1
    local keys={}
    for _,raw in ipairs(type(clip.keys)=='table' and clip.keys or {}) do
        if type(raw)=='table' then
            local key=copyKey(raw)
            key.time=math.min(key.time,clip.duration)
            keys[#keys+1]=key
        end
    end
    table.sort(keys,function(left,right) return left.time<right.time end)
    local unique={}
    for _,key in ipairs(keys) do
        local previous=unique[#unique]
        if previous and math.abs(previous.time-key.time)<0.000001 then
            previous.weights=key.weights
        else
            unique[#unique+1]=key
        end
    end
    clip.keys=unique
    return clip
end

function M.copyWeights(weights)
    return copyWeights(weights)
end

function M.setDuration(clip,duration)
    if type(clip)~='table' then return false end
    local oldDuration=math.max(finite(clip.duration,1),0.001)
    duration=math.max(finite(duration,oldDuration),0.001)
    if math.abs(oldDuration-duration)<=1e-8 then return false end
    local scale=duration/oldDuration
    clip.duration=duration
    for _,key in ipairs(clip.keys or {}) do key.time=math.max(0,math.min(duration,key.time*scale)) end
    table.sort(clip.keys or {},function(left,right) return left.time<right.time end)
    return true
end

function M.insertKey(clip,time,weights)
    if type(clip)~='table' then return false end
    time=math.max(0,math.min(finite(clip.duration,1),finite(time,0)))
    for _,key in ipairs(clip.keys or {}) do
        if math.abs(key.time-time)<0.000001 then key.weights=copyWeights(weights); return true end
    end
    clip.keys=clip.keys or {}
    clip.keys[#clip.keys+1]={time=time,weights=copyWeights(weights)}
    table.sort(clip.keys,function(left,right) return left.time<right.time end)
    return true
end

function M.removeKey(clip,index)
    if type(clip)~='table' or type(clip.keys)~='table' or index<1 or index>#clip.keys then return false end
    table.remove(clip.keys,index)
    return true
end

local function weightsAsMap(weights)
    local result={}
    for _,entry in ipairs(weights or {}) do result[entry.targetId]=clamp(entry.weight) end
    return result
end

function M.sample(clip,time)
    if type(clip)~='table' or #(clip.keys or {})==0 then return {} end
    local duration=math.max(finite(clip.duration,0.001),0.001)
    time=math.max(0,math.min(duration,finite(time,0)))
    local keys=clip.keys
    if time<=keys[1].time then return copyWeights(keys[1].weights) end
    local last=keys[#keys]
    if time>=last.time then return copyWeights(last.weights) end
    local left,right=keys[1],last
    for index=2,#keys do
        if keys[index].time>=time then left,right=keys[index-1],keys[index];break end
    end
    local span=math.max(right.time-left.time,0.000001)
    local factor=clip.interpolation==0 and 0 or (time-left.time)/span
    local names={}
    for _,entry in ipairs(left.weights or {}) do names[entry.targetId]=entry.targetName end
    for _,entry in ipairs(right.weights or {}) do names[entry.targetId]=entry.targetName end
    local leftMap=weightsAsMap(left.weights)
    local rightMap=weightsAsMap(right.weights)
    local result={}
    for id,name in pairs(names) do
        local leftValue=leftMap[id] or 0
        local rightValue=rightMap[id] or 0
        local value=leftValue+(rightValue-leftValue)*factor
        if clip.interpolation==0 then value=leftMap[id] or 0 end
        if value>1e-8 then result[#result+1]={targetId=id,targetName=name or '',weight=clamp(value)} end
    end
    table.sort(result,function(leftEntry,rightEntry) return leftEntry.targetId<rightEntry.targetId end)
    return result
end

function M.inspect(clip,targets,statusFn)
    local targetById={}
    for _,target in ipairs(type(targets)=='table' and targets or {}) do
        if type(target)=='table' and trim(target.id)~='' then targetById[target.id]=target end
    end
    local missing,stale,active=0,0,0
    for _,key in ipairs(clip and clip.keys or {}) do
        for _,entry in ipairs(key.weights or {}) do
            if entry.weight and entry.weight>1e-8 then
                active=active+1
                local target=targetById[entry.targetId]
                if not target then
                    missing=missing+1
                elseif statusFn and statusFn(target)~='ready' then
                    stale=stale+1
                end
            end
        end
    end
    return {missing=missing,stale=stale,active=active}
end

function M.validate(clip)
    local report={ok=true,errors={},keyCount=0}
    if type(clip)~='table' then report.ok=false;report.errors[#report.errors+1]='clip is missing';return report end
    if trim(clip.name)=='' then report.ok=false;report.errors[#report.errors+1]='clip has no name' end
    if finite(clip.duration,0)<=0 then report.ok=false;report.errors[#report.errors+1]='clip duration must be positive' end
    local keys=type(clip.keys)=='table' and clip.keys or {}
    report.keyCount=#keys
    if #keys<2 then report.ok=false;report.errors[#report.errors+1]='clip requires at least two keys' end
    local previous=-math.huge
    for _,key in ipairs(keys) do
        if finite(key.time,-1)<0 or key.time<previous then
            report.ok=false;report.errors[#report.errors+1]='clip key times must be ordered'
        end
        previous=key.time
    end
    if #keys>0 and math.abs(keys[1].time)>0.000001 then
        report.ok=false;report.errors[#report.errors+1]='clip must have a key at time zero'
    end
    if #keys>0 and math.abs(keys[#keys].time-finite(clip.duration,0))>0.000001 then
        report.ok=false;report.errors[#report.errors+1]='clip must have an end key'
    end
    return report
end

return M
