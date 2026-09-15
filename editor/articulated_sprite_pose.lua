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

-- Timeline editing state is separate from serialized keys and keyed by part.
local Model=require 'articulated_sprite_model'
local M={}
local function identity() return {x=0,y=0,z=0,angle=0,sx=1,sy=1,sz=1,easing=0} end
local function ease(t,k)
    local e=k.easing or 0
    if e==1 then return t*t end
    if e==2 then return 1-(1-t)*(1-t) end
    if e==3 then return t<0.5 and 2*t*t or 1-2*(1-t)*(1-t) end
    if e==4 then return t*t*(3-2*t) end
    if e~=5 then return t end
    local b=k.bezier or {0.25,0.25,0.75,0.75}
    local function cubic(u,p,q) return 3*(1-u)^2*u*p+3*(1-u)*u*u*q+u^3 end
    local lo,hi=0,1
    for _=1,24 do
        local mid=(lo+hi)/2
        if cubic(mid,b[1],b[3])<t then lo=mid else hi=mid end
    end
    return cubic((lo+hi)/2,b[2],b[4])
end
local function angle(k)
    return k.euler and k.euler[3] or k.angle or (k.q and 2*math.atan(k.q[3],k.q[4])*180/math.pi) or 0
end
function M.sample(clip,id,time)
    local pose=identity()
    if not clip or not id or id==0 then return pose end
    local track
    for _,t in ipairs(clip.tracks) do if t.part==id then track=t; break end end
    if not track or #track.keys==0 then return pose end
    local keys=track.keys
    local a,b=keys[1],keys[1]
    if time>=keys[#keys].time then a,b=keys[#keys],keys[#keys]
    elseif time>keys[1].time then
        local lo,hi=2,#keys
        while lo<hi do
            local mid=math.floor((lo+hi)/2)
            if keys[mid].time<time then lo=mid+1 else hi=mid end
        end
        a,b=keys[lo-1],keys[lo]
    end
    local t=a==b and 0 or ease((time-a.time)/(b.time-a.time),a)
    local function lerp(x,y) return x+(y-x)*t end
    local mask=track.mask or 7
    if mask & 1~=0 then
        for _,f in ipairs({'x','y','z'}) do pose[f]=lerp(a[f] or 0,b[f] or 0) end
    end
    if mask & 4~=0 then
        for _,f in ipairs({'sx','sy','sz'}) do pose[f]=lerp(a[f] or 1,b[f] or 1) end
    end
    if mask & 2~=0 then
        if (a.euler or not a.q) and (b.euler or not b.q) then
            pose.angle=lerp(angle(a),angle(b))
        else
            -- The runtime uses shortest-path normalized quaternion lerp.
            local function quat(k)
                local r=angle(k)*math.pi/360
                return k.q or {0,0,math.sin(r),math.cos(r)}
            end
            local qa,qb=quat(a),quat(b)
            local dot=0
            for i=1,4 do dot=dot+qa[i]*qb[i] end
            local sign=dot<0 and -1 or 1
            pose.angle=2*math.atan(lerp(qa[3],sign*qb[3]),lerp(qa[4],sign*qb[4]))*180/math.pi
        end
    end
    local source=math.abs(time-b.time)<0.00001 and b or a
    pose.easing=source.easing or 0; pose.bezier=Model.copy(source.bezier)
    return pose
end
function M.clear(E)
    if E.transient then E.dirty=true end
    E.drafts={}; E.transient=false
end
function M.sync(E)
    local clip=E.project.clips[E.clip]
    local c=E.poseContext
    local contextChanged=not c or c.project~=E.project or c.clip~=clip or c.frame~=E.frame or c.mode~=E.mode
    local timeChanged=not c or c.time~=E.time
    if contextChanged or timeChanged then M.clear(E) end
    if contextChanged or (c and c.part~=E.selected) then E.keyOrigin=nil; E.worldDrag=nil end
    if contextChanged or timeChanged or c.part~=E.selected or c.revision~=E.geometryRevision then
        local draft=E.drafts and E.drafts[E.selected]
        E.pose=draft and Model.copy(draft) or M.sample(clip,E.selected,E.time)
        E.poseContext={project=E.project,clip=clip,frame=E.frame,part=E.selected,
            time=E.time,mode=E.mode,revision=E.geometryRevision}
    end
end
function M.stage(E)
    if E.playing then E.playing=false; E.poseDirty=true end
    E.drafts=E.drafts or {}
    E.drafts[E.selected]=Model.copy(E.pose)
    E.transient=true; E.dirty=true
end
function M.recorded(E)
    if E.drafts then E.drafts[E.selected]=nil end
    E.transient=E.drafts and next(E.drafts)~=nil or false
end
return M
