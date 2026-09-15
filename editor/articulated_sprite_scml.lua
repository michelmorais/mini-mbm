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

-- SCML is evaluated once on import. Runtime playback uses ordinary SPT TRS tracks.
local Model=require 'articulated_sprite_model'
local Parser=require 'spriter_scml_wrapper'
local M={}
local function number(value,default) return tonumber(value) or default end
local function clamp(t) return math.max(0,math.min(1,t)) end
local function lerp(a,b,t) return a+(b-a)*t end
local function polynomial(points,t)
    for n=#points-1,1,-1 do
        for i=1,n do points[i]=lerp(points[i],points[i+1],t) end
    end
    return points[1]
end
function M.curve(attrs,t)
    t=clamp(t); attrs=attrs or {}
    local kind=attrs.curve_type or 'linear'
    if kind=='linear' then return t end
    if kind=='instant' then return 0 end
    local c1,c2,c3,c4=number(attrs.c1,0),number(attrs.c2,0),number(attrs.c3,0),number(attrs.c4,0)
    if kind=='bezier' then
        local lo,hi=0,1
        for _=1,30 do
            local mid=(lo+hi)/2
            if polynomial({0,c1,c3,1},mid)<t then lo=mid else hi=mid end
        end
        return polynomial({0,c2,c4,1},(lo+hi)/2)
    end
    local points=({quadratic={0,c1,1},cubic={0,c1,c2,1},
        quartic={0,c1,c2,c3,1},quintic={0,c1,c2,c3,c4,1}})[kind]
    assert(points,'Unsupported SCML curve: '..kind)
    return polynomial(points,t)
end
local function fileFor(data,attrs)
    local folder=assert(data.folders[number(attrs.folder,0)],'Missing SCML folder')
    return assert(folder.files[number(attrs.file,0)],'Missing SCML image reference')
end
local function spatial(key,data)
    local a=key.attrs
    local f=key.type=='object' and fileFor(data,a) or nil
    return {x=number(a.x,0),y=number(a.y,0),angle=number(a.angle,0),
        sx=number(a.scale_x,1),sy=number(a.scale_y,1),alpha=number(a.a,1),
        px=number(a.pivot_x,f and f.pivot_x or 0),py=number(a.pivot_y,f and f.pivot_y or 1),file=f}
end
local function combine(parent,child)
    if not parent then return child end
    local a=parent.angle*math.pi/180
    local x,y=child.x*parent.sx,child.y*parent.sy
    child.x=parent.x+x*math.cos(a)-y*math.sin(a)
    child.y=parent.y+x*math.sin(a)+y*math.cos(a)
    child.angle=parent.angle+child.angle*(parent.sx*parent.sy<0 and -1 or 1)
    child.sx=child.sx*parent.sx; child.sy=child.sy*parent.sy
    child.alpha=child.alpha*parent.alpha
    return child
end
-- Reference keys (not timeline array positions) determine interpolation endpoints.
function M.evaluate(data,anim,time)
    local main,index=assert(anim.mainline[1],'Empty SCML mainline'),1
    for i,k in ipairs(anim.mainline) do if k.time<=time then main,index=k,i else break end end
    local nextMain=anim.mainline[index+1]
    local stop=nextMain and nextMain.time or anim.length
    local tweenTime=time
    if stop>main.time then tweenTime=lerp(main.time,stop,M.curve(main.curve,(time-main.time)/(stop-main.time))) end
    local function localPose(ref)
        local timeline=assert(anim.timelines[number(ref.timeline,0)],'Missing SCML timeline')
        assert(timeline.objectType=='sprite' or timeline.objectType=='bone','Unsupported SCML object: '..timeline.objectType)
        local key,ki
        for i,k in ipairs(timeline.keys) do if k.id==number(ref.key,0) then key,ki=k,i; break end end
        assert(key,'Missing SCML key')
        local a=spatial(key,data)
        local nextKey=timeline.keys[ki+1] or (anim.looping and timeline.keys[1])
        if not nextKey or #timeline.keys==1 then return a,timeline end
        local endTime=nextKey.time
        if endTime<key.time then endTime=endTime+anim.length end
        local t=endTime>key.time and M.curve(key.curve,(tweenTime-key.time)/(endTime-key.time)) or 0
        local b=spatial(nextKey,data)
        for _,field in ipairs({'x','y','sx','sy','alpha','px','py'}) do a[field]=lerp(a[field],b[field],t) end
        local angle=b.angle
        if key.spin==0 then angle=a.angle
        elseif key.spin>0 and angle<a.angle then angle=angle+360
        elseif key.spin<0 and angle>a.angle then angle=angle-360 end
        a.angle=lerp(a.angle,angle,t)
        return a,timeline
    end
    local refs,cache,visiting={},{},{}
    for _,ref in ipairs(main.boneRefs) do refs[number(ref.id,0)]=ref end
    local function bone(id)
        if not id or id<0 then return nil end
        if cache[id] then return cache[id] end
        assert(not visiting[id],'SCML bone cycle'); visiting[id]=true
        local ref=assert(refs[id],'Missing SCML bone parent')
        cache[id]=combine(bone(number(ref.parent,-1)),localPose(ref))
        visiting[id]=nil
        return cache[id]
    end
    local out={}
    for i,ref in ipairs(main.objectRefs) do
        local pose,timeline=localPose(ref)
        pose=combine(bone(number(ref.parent,-1)),pose)
        pose.timeline=timeline.id; pose.name=timeline.name; pose.order=number(ref.z_index,i-1)
        out[#out+1]=pose
    end
    return out
end
local function variant(pose)
    return pose.timeline..':'..pose.file.path..':'..pose.order
end
local function timesFor(anim,rate)
    local times,seen={},{}
    local quantum=math.max(0.05,anim.length*0.000001)
    local function add(t)
        t=math.max(0,math.min(anim.length,math.floor(t/quantum+0.5)*quantum))
        if not seen[t] then seen[t]=true; times[#times+1]=t end
    end
    for i=0,math.ceil(anim.length*rate/1000) do add(i*1000/rate) end
    -- Paired keys stay above the engine 10 us key replacement tolerance.
    for _,key in ipairs(anim.mainline) do add(key.time); if key.time>0 then add(key.time-quantum) end end
    for _,timeline in pairs(anim.timelines) do
        table.sort(timeline.keys,function(a,b) return a.time<b.time end)
        for _,key in ipairs(timeline.keys) do add(key.time); if key.time>0 then add(key.time-quantum) end end
    end
    add(anim.length-quantum); add(anim.length)
    table.sort(times)
    return times
end
function M.import(path,rate)
    rate=rate or 60
    assert(rate>=1 and rate<=240,'Invalid SCML sample rate')
    local data,err=Parser.parse(path); assert(data,err)
    local project=Model.new(); project.frames={}; project.frameAnimations={}
    local images,names={},{}
    local function image(file)
        if images[file.path] then return images[file.path] end
        local f=assert(io.open(file.path,'rb'),'Missing SCML image: '..file.path); f:close()
        assert(file.width>0 and file.height>0,'Missing SCML image dimensions: '..file.path)
        local index=#project.images+1; images[file.path]=index
        project.images[index]={path=file.path,width=file.width,height=file.height}
        return index
    end
    for _,entity in ipairs(data.entities) do for _,anim in ipairs(entity.animations) do
        assert(anim.length>0,'Empty SCML animation: '..anim.name)
        local name=(#data.entities>1 and entity.name..'_' or '')..anim.name
        name=name:sub(1,32)
        local base,suffix=name,1
        while names[name] do suffix=suffix+1; name=base:sub(1,27)..'_'..suffix end
        names[name]=true
        local fi=#project.frames+1; project.frames[fi]={parts={}}
        local clip={name=name,duration=anim.length/1000,speed=1,priority=0,loop=anim.looping,blend=0,tracks={},frame=fi}
        project.clips[#project.clips+1]=clip
        -- TYPE_ANIMATION PAUSED = 0; only this single base frame is selected.
        project.frameAnimations[#project.frameAnimations+1]={name,fi,fi,1,0}
        local times=timesFor(anim,rate)
        local samples,variants,list={},{},{}
        for _,time in ipairs(times) do
            local poses=M.evaluate(data,anim,anim.looping and time==anim.length and 0 or time)
            local sample={time=time/1000,poses={}}; samples[#samples+1]=sample
            for _,pose in ipairs(poses) do
                assert(math.abs(pose.alpha-1)<0.00001 or pose.alpha==0,
                    'SCML opacity animation is not supported by SPT TRS tracks: '..pose.name)
                local id=variant(pose); sample.poses[id]=pose
                if not variants[id] then
                    local file=pose.file
                    local w,h=file.width,file.height
                    local vertices={{x=0,y=0},{x=w,y=0},{x=w,y=h},{x=0,y=h}}
                    local p=Model.addPart(project,fi,image(file),{vertices},vertices,{1,2,3,1,3,4},
                        {kind='rectangle',rect={x=0,y=0,w=w,h=h},budget=2,form={}})
                    p.name=pose.name..' / '..file.name
                    p.origin={x=file.pivot_x*w,y=(1-file.pivot_y)*h}
                    p.x=pose.x; p.y=pose.y; p.pivot={x=p.x,y=p.y,z=0}
                    local v={id=id,part=p,file=file,order=pose.order,timeline=pose.timeline}
                    variants[id]=v; list[#list+1]=v
                end
            end
        end
        table.sort(list,function(a,b)
            if a.order~=b.order then return a.order<b.order end
            if a.timeline~=b.timeline then return a.timeline<b.timeline end
            return a.id<b.id
        end)
        project.frames[fi].parts={}
        for _,v in ipairs(list) do
            local p=v.part
            project.frames[fi].parts[#project.frames[fi].parts+1]=p
            local track={part=p.id,mask=7,keys={}}; clip.tracks[#clip.tracks+1]=track
            local previousAngle
            for _,sample in ipairs(samples) do
                local pose=sample.poses[v.id]
                local key={time=sample.time,x=0,y=0,z=0,angle=previousAngle or 0,sx=0,sy=0}
                if pose then
                    local angle=pose.angle
                    if previousAngle then angle=previousAngle+(angle-previousAngle+180)%360-180 end
                    previousAngle=angle
                    local a=angle*math.pi/180
                    local dx=(v.file.pivot_x-pose.px)*v.file.width*pose.sx
                    local dy=(v.file.pivot_y-pose.py)*v.file.height*pose.sy
                    key.x=pose.x+dx*math.cos(a)-dy*math.sin(a)-p.x
                    key.y=pose.y+dx*math.sin(a)+dy*math.cos(a)-p.y
                    key.angle=angle
                    key.sx=pose.alpha==0 and 0 or pose.sx; key.sy=pose.alpha==0 and 0 or pose.sy
                end
                track.keys[#track.keys+1]=key
            end
        end
        assert(#list>0,'SCML animation has no sprites: '..name)
    end end
    assert(#project.clips>0,'SCML has no animations')
    project.scml={path=path,sampleRate=rate}
    return Model.validate(project)
end
return M
