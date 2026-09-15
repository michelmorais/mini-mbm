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

-- Pure, transactional timeline operations. All keys retain their full payload.
local Model=require 'articulated_sprite_model'
local M={epsilon=0.00001}
local function finite(n) return type(n)=='number' and n==n and math.abs(n)<math.huge end
function M.selection(clip,selected)
    local items,first,last={},math.huge,-math.huge
    for _,track in ipairs(clip.tracks) do for _,key in ipairs(track.keys) do
        if selected[key] then
            items[#items+1]={part=track.part,mask=track.mask or 7,key=Model.copy(key)}
            first=math.min(first,key.time); last=math.max(last,key.time)
        end
    end end
    return {items=items,first=first,last=last}
end
local function validate(clip)
    assert(finite(clip.duration) and clip.duration>0,'tl_invalid_time')
    for _,track in ipairs(clip.tracks) do
        table.sort(track.keys,function(a,b) return a.time<b.time end)
        for i,key in ipairs(track.keys) do
            assert(finite(key.time) and key.time>=0 and key.time<=clip.duration+M.epsilon,'tl_out_of_range')
            assert(i==1 or key.time-track.keys[i-1].time>M.epsilon,'tl_collision')
        end
    end
end
local function candidate(clip) return Model.copy(clip) end
local function publish(clip,out)
    validate(out); clip.tracks=out.tracks; clip.duration=out.duration
end
local function shift(clip,time,duration)
    for _,track in ipairs(clip.tracks) do for _,key in ipairs(track.keys) do
        if key.time+M.epsilon>=time then key.time=key.time+duration end
    end end
    clip.duration=clip.duration+duration
end
local function paste(out,clipboard,time,parts)
    assert(#clipboard.items>0,'tl_no_selection')
    assert(time>=0 and time+clipboard.last-clipboard.first<=out.duration+M.epsilon,'tl_out_of_range')
    local tracks={}; for _,t in ipairs(out.tracks) do tracks[t.part]=t end
    for _,item in ipairs(clipboard.items) do
        assert(parts[item.part],'tl_missing_part')
        local track=tracks[item.part]
        if not track then
            track={part=item.part,mask=item.mask,keys={}}; tracks[item.part]=track; out.tracks[#out.tracks+1]=track
        end
        assert((track.mask or 7)==item.mask,'tl_channel_mismatch')
        local key=Model.copy(item.key); key.time=time+key.time-clipboard.first
        track.keys[#track.keys+1]=key
    end
end
function M.paste(clip,clipboard,time,parts,insert)
    assert(clipboard and #clipboard.items>0,'tl_no_selection')
    assert(finite(time) and time>=0 and time<=clip.duration,'tl_out_of_range')
    local out=candidate(clip)
    if insert then
        local span=clipboard.last-clipboard.first
        assert(span>M.epsilon,'tl_zero_span')
        -- Match the 3D editor's ripple insertion, with room for a boundary key.
        shift(out,time,span+math.max(M.epsilon*4,out.duration*0.000001))
    end
    paste(out,clipboard,time,parts); publish(clip,out)
end
function M.move(clip,selected,delta)
    assert(finite(delta),'tl_invalid_time')
    local out=candidate(clip); local count=0
    for ti,track in ipairs(clip.tracks) do for ki,key in ipairs(track.keys) do
        if selected[key] then out.tracks[ti].keys[ki].time=key.time+delta; count=count+1 end
    end end
    assert(count>0,'tl_no_selection'); publish(clip,out)
end
function M.delete(clip,selected)
    local out=candidate(clip); local count=0
    for ti,track in ipairs(clip.tracks) do
        for ki=#track.keys,1,-1 do
            if selected[track.keys[ki]] then table.remove(out.tracks[ti].keys,ki); count=count+1 end
        end
    end
    assert(count>0,'tl_no_selection'); publish(clip,out)
end
function M.insertTime(clip,time,duration)
    assert(finite(time) and time>=0 and time<=clip.duration,'tl_out_of_range')
    assert(finite(duration) and duration>M.epsilon,'tl_invalid_time')
    local out=candidate(clip); shift(out,time,duration); publish(clip,out)
end
function M.removalImpact(clip,time,duration)
    local stop=math.min(clip.duration,time+duration); local count=0
    local function lower(keys,bound)
        local lo,hi=1,#keys+1
        while lo<hi do
            local mid=math.floor((lo+hi)/2)
            if keys[mid].time<bound then lo=mid+1 else hi=mid end
        end
        return lo
    end
    for _,track in ipairs(clip.tracks) do
        count=count+math.max(0,lower(track.keys,stop-M.epsilon)-lower(track.keys,time-M.epsilon))
    end
    return count,stop
end
function M.removeTime(clip,time,duration)
    assert(finite(time) and time>=0 and time<clip.duration,'tl_out_of_range')
    assert(finite(duration) and duration>M.epsilon,'tl_invalid_time')
    local _,stop=M.removalImpact(clip,time,duration)
    local span=stop-time; assert(clip.duration-span>M.epsilon,'tl_invalid_time')
    local out=candidate(clip)
    for _,track in ipairs(out.tracks) do
        for i=#track.keys,1,-1 do
            local key=track.keys[i]
            if key.time+M.epsilon>=time and key.time<stop-M.epsilon then table.remove(track.keys,i)
            elseif key.time>=stop-M.epsilon then key.time=math.max(0,key.time-span) end
        end
    end
    out.duration=clip.duration-span; publish(clip,out)
end
return M
