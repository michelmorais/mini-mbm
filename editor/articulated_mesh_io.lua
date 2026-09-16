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

local Model=require 'articulated_sprite_model'
local M={}
function M.read(d)
    local project={frames={},clips={},options={autoKey=false}}
    for f=1,d:getTotalFrame() do project.frames[f]={parts={}} end
    for i=1,d:getTotalArticulatedParts() do
        local id,f,s,name,x,y,z,qx,qy,qz,qw,parent=d:getArticulatedPart(i)
        local p={id=id,frame=f,subset=s,name=name,pivot={x,y,z},q={qx,qy,qz,qw},parent=parent}
        table.insert(project.frames[f].parts,p)
    end
    for ci=1,d:getTotalArticulatedAnimations() do
        local name,duration,speed,priority,loop,blend=d:getArticulatedAnimation(ci)
        local c={name=name,duration=duration,speed=speed,priority=priority,loop=loop,blend=blend,tracks={}}
        project.clips[ci]=c
        for ti=1,d:getTotalArticulatedTracks(ci) do
            local id,mask,count=d:getArticulatedTrack(ci,ti)
            local t={part=id,mask=mask,keys={}}; c.tracks[ti]=t
            for ki=1,count do
                local time,x,y,z,qx,qy,qz,qw,sx,sy,sz,easing,bx,by,cx,cy,ex,ey,ez,has=d:getArticulatedKey(ci,ti,ki)
                t.keys[ki]={time=time,x=x,y=y,z=z,q={qx,qy,qz,qw},sx=sx,sy=sy,sz=sz,
                    easing=easing,bezier={bx,by,cx,cy},euler=has and {ex,ey,ez} or nil}
            end
        end
    end
    return project
end
function M.apply(d,project)
    for i=d:getTotalArticulatedAnimations(),1,-1 do d:removeArticulatedAnimation(i) end
    d:removeArticulatedParts()
    local indices={}
    for _,frame in ipairs(project.frames) do for _,p in ipairs(frame.parts) do
        indices[p]=d:addArticulatedPart(p.id,p.frame,p.subset,p.name,table.unpack({p.pivot[1],p.pivot[2],p.pivot[3],p.q[1],p.q[2],p.q[3],p.q[4],0}))
    end end
    for _,frame in ipairs(project.frames) do for _,p in ipairs(frame.parts) do
        if p.parent~=0 then d:updateArticulatedPart(indices[p],p.name,p.pivot[1],p.pivot[2],p.pivot[3],p.q[1],p.q[2],p.q[3],p.q[4],p.parent) end
    end end
    for _,c in ipairs(project.clips) do
        local ci=d:addArticulatedAnimation(c.name,c.duration,c.speed,c.priority,c.loop,c.blend)
        for _,t in ipairs(c.tracks) do
            local ti=d:addArticulatedTrack(ci,t.part,t.mask)
            for ki,k in ipairs(t.keys) do
                local q=k.q or {0,0,0,1}
                d:addArticulatedKey(ci,ti,k.time,k.x or 0,k.y or 0,k.z or 0,q[1],q[2],q[3],q[4],k.sx or 1,k.sy or 1,k.sz or 1)
                if k.euler then d:setArticulatedKeyEuler(ci,ti,k.time,table.unpack(k.euler)) end
                d:setArticulatedKeyEasing(ci,ti,ki,k.easing or 0)
                if k.bezier then d:setArticulatedKeyBezier(ci,ti,ki,table.unpack(k.bezier)) end
            end
        end
    end
    return d
end
function M.build(base,project)
    local d=meshDebug:new(); assert(d:load(base),'load_mesh_failed')
    return M.apply(d,project)
end
function M.bounds(d)
    local result={}; local all={minX=math.huge,minY=math.huge,minZ=math.huge,maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge}
    for f=1,d:getTotalFrame() do
        result[f]={}
        for s=1,d:getTotalSubset(f) do
            local b={minX=math.huge,minY=math.huge,minZ=math.huge,maxX=-math.huge,maxY=-math.huge,maxZ=-math.huge}
            local count=d:getTotalVertex(f,s)
            local vertices=count>0 and d:getVertex(f,s,1,count) or {}
            if count==1 then vertices={vertices} end
            for _,v in ipairs(vertices) do
                for _,axis in ipairs({'X','Y','Z'}) do local value=v[axis:lower()] or 0
                    b['min'..axis]=math.min(b['min'..axis],value); b['max'..axis]=math.max(b['max'..axis],value)
                    all['min'..axis]=math.min(all['min'..axis],value); all['max'..axis]=math.max(all['max'..axis],value)
                end
            end
            if count==0 then b={minX=0,minY=0,minZ=0,maxX=0,maxY=0,maxZ=0} end
            result[f][s]=b
        end
    end
    assert(all.minX~=math.huge,'empty_mesh')
    return result,all
end
function M.record(project,clip,id,time,pose,mask)
    local c=assert(project.clips[clip],'missing_clip'); assert(time>=0 and time<=c.duration,'tl_out_of_range')
    local track; for _,t in ipairs(c.tracks) do if t.part==id then track=t; break end end
    if not track then track={part=id,mask=mask or 7,keys={}}; c.tracks[#c.tracks+1]=track end
    local key=Model.copy(pose); key.time=time
    for i,k in ipairs(track.keys) do if math.abs(k.time-time)<0.00001 then track.keys[i]=key; return end end
    track.keys[#track.keys+1]=key; table.sort(track.keys,function(a,b) return a.time<b.time end)
end
return M
