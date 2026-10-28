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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        |
|------------------------------------------------------------------------------------------------------------------------|

   Facial clip authoring model.

   Clips store named target-weight keys. They are intentionally independent from Blender and from
   the MSH writer; a later exporter can sample this same representation into Shape Key Actions or
   native facial clip sections.
]]--

local M={}

local function finite(value,fallback)
    value=tonumber(value)
    if not value or value~=value or value==math.huge or value==-math.huge then return fallback or 0 end
    return value
end

local function clamp(value)
    return math.max(0,math.min(1,finite(value,0)))
end

local function copyWeights(weights)
    local result={}
    for name,value in pairs(type(weights)=='table' and weights or {}) do
        if type(name)=='string' and name~='' then result[name]=clamp(value) end
    end
    return result
end

local function copyKey(key)
    return {time=math.max(0,finite(key and key.time,0)),weights=copyWeights(key and key.weights)}
end

function M.new()
    return {clips={},selectedClip=1}
end

function M.normalize(state)
    if type(state)~='table' then state=M.new() end
    state.clips=type(state.clips)=='table' and state.clips or {}
    state.selectedClip=math.max(1,math.floor(finite(state.selectedClip,1)))
    local clips={}
    for index,raw in ipairs(state.clips) do
        if type(raw)=='table' then
            local clip={name=type(raw.name)=='string' and raw.name or ('facial_clip_' .. index),
                duration=math.max(finite(raw.duration,1),0.001),fps=math.max(finite(raw.fps,30),1),
                loop=raw.loop==true,interpolation=raw.interpolation==0 and 0 or 1,keys={}}
            for _,key in ipairs(type(raw.keys)=='table' and raw.keys or {}) do clip.keys[#clip.keys+1]=copyKey(key) end
            table.sort(clip.keys,function(a,b) return a.time<b.time end)
            clips[#clips+1]=clip
        end
    end
    state.clips=clips
    state.selectedClip=math.min(state.selectedClip,math.max(1,#clips))
    return state
end

function M.addClip(state,name)
    state=M.normalize(state)
    name=type(name)=='string' and name:match('^%s*(.-)%s*$') or ''
    if name=='' then return nil,'clip name is empty' end
    for _,clip in ipairs(state.clips) do if clip.name==name then return nil,'duplicate facial clip name' end end
    local clip={name=name,duration=1,fps=30,loop=true,interpolation=1,keys={}}
    state.clips[#state.clips+1]=clip
    state.selectedClip=#state.clips
    return clip
end

function M.insertKey(clip,time,weights)
    if type(clip)~='table' then return false end
    time=math.max(0,math.min(tonumber(clip.duration) or 1,finite(time,0)))
    for _,key in ipairs(clip.keys or {}) do
        if math.abs(key.time-time)<0.000001 then key.weights=copyWeights(weights); return true end
    end
    clip.keys=clip.keys or {}
    clip.keys[#clip.keys+1]={time=time,weights=copyWeights(weights)}
    table.sort(clip.keys,function(a,b) return a.time<b.time end)
    return true
end

function M.removeKey(clip,index)
    if type(clip)~='table' or type(clip.keys)~='table' or index<1 or index>#clip.keys then return false end
    table.remove(clip.keys,index)
    return true
end

function M.sample(clip,time,targetNames)
    local result={}
    for _,name in ipairs(targetNames or {}) do result[name]=0 end
    if type(clip)~='table' or #(clip.keys or {})==0 then return result end
    local duration=math.max(finite(clip.duration,0.001),0.001)
    time=math.max(0,math.min(duration,finite(time,0)))
    local keys=clip.keys
    if time<=keys[1].time then
        for name,value in pairs(keys[1].weights or {}) do result[name]=clamp(value) end
        return result
    end
    local last=keys[#keys]
    if time>=last.time then
        for name,value in pairs(last.weights or {}) do result[name]=clamp(value) end
        return result
    end
    local left,right=keys[1],keys[#keys]
    for index=2,#keys do
        if keys[index].time>=time then left,right=keys[index-1],keys[index];break end
    end
    local span=math.max(right.time-left.time,0.000001)
    local factor=clip.interpolation==0 and 0 or (time-left.time)/span
    local names={}
    for name in pairs(left.weights or {}) do names[name]=true end
    for name in pairs(right.weights or {}) do names[name]=true end
    for name in pairs(names) do
        local a=clamp(left.weights and left.weights[name] or 0)
        local b=clamp(right.weights and right.weights[name] or 0)
        result[name]=a+(b-a)*factor
    end
    return result
end

function M.validate(clip)
    local report={ok=true,errors={},warnings={},keyCount=0}
    if type(clip)~='table' then report.ok=false;report.errors[#report.errors+1]='clip is missing';return report end
    if type(clip.name)~='string' or clip.name=='' then report.ok=false;report.errors[#report.errors+1]='clip has no name' end
    if finite(clip.duration,0)<=0 then report.ok=false;report.errors[#report.errors+1]='clip duration must be positive' end
    local keys=type(clip.keys)=='table' and clip.keys or {}
    report.keyCount=#keys
    if #keys<2 then report.ok=false;report.errors[#report.errors+1]='clip requires at least two keys' end
    local previous=-math.huge
    for _,key in ipairs(keys) do
        if finite(key.time,-1)<0 or key.time<previous then report.ok=false;report.errors[#report.errors+1]='clip key times must be ordered' end
        previous=key.time
    end
    if #keys>0 and math.abs(keys[1].time)>0.000001 then report.ok=false;report.errors[#report.errors+1]='clip must have a key at time zero' end
    if #keys>0 and math.abs(keys[#keys].time-finite(clip.duration,0))>0.000001 then report.ok=false;report.errors[#report.errors+1]='clip must have an end key' end
    return report
end

return M
