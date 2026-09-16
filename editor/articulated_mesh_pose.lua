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
local Base=require 'articulated_sprite_pose'
local M={clear=Base.clear,stage=Base.stage,recorded=Base.recorded}
local function clamp(x) return math.max(-1,math.min(1,x)) end
function M.quaternion(e)
    local x,y,z=-e[1]*math.pi/360,e[2]*math.pi/360,e[3]*math.pi/360
    local sx,cx,sy,cy,sz,cz=math.sin(x),math.cos(x),math.sin(y),math.cos(y),math.sin(z),math.cos(z)
    return {cy*sx*cz+sy*cx*sz,sy*cx*cz-cy*sx*sz,cy*cx*sz-sy*sx*cz,cy*cx*cz+sy*sx*sz}
end
local function normalized(q)
    local length=math.sqrt(q[1]^2+q[2]^2+q[3]^2+q[4]^2)
    if length<0.000001 then return {0,0,0,1} end
    return {q[1]/length,q[2]/length,q[3]/length,q[4]/length}
end
function M.euler(q)
    local x,y,z,w=table.unpack(normalized(q))
    return {-math.asin(clamp(2*(w*x-y*z)))*180/math.pi,
        math.atan(2*(x*z+w*y),1-2*(x*x+y*y))*180/math.pi,
        math.atan(2*(x*y+w*z),1-2*(x*x+z*z))*180/math.pi}
end
function M.multiply(a,b)
    return {a[4]*b[1]+a[1]*b[4]+a[2]*b[3]-a[3]*b[2],
        a[4]*b[2]-a[1]*b[3]+a[2]*b[4]+a[3]*b[1],
        a[4]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[4],
        a[4]*b[4]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3]}
end
function M.rotate(q,x,y,z)
    local tx,ty,tz=2*(q[2]*z-q[3]*y),2*(q[3]*x-q[1]*z),2*(q[1]*y-q[2]*x)
    return x+q[4]*tx+q[2]*tz-q[3]*ty,y+q[4]*ty+q[3]*tx-q[1]*tz,z+q[4]*tz+q[1]*ty-q[2]*tx
end
function M.sample(clip,id,time)
    local pose=Base.sample(clip,id,time)
    pose.angle=nil; pose.euler={0,0,0}; pose.q={0,0,0,1}
    local track
    for _,t in ipairs(clip and clip.tracks or {}) do if t.part==id then track=t; break end end
    if not track or #track.keys==0 or (track.mask or 7)&2==0 then return pose end
    local keys=track.keys; local a,b=keys[1],keys[1]
    if time>=keys[#keys].time then a,b=keys[#keys],keys[#keys]
    elseif time>keys[1].time then
        local lo,hi=2,#keys
        while lo<hi do local mid=math.floor((lo+hi)/2); if keys[mid].time<time then lo=mid+1 else hi=mid end end
        a,b=keys[lo-1],keys[lo]
    end
    local t=a==b and 0 or Base.ease((time-a.time)/(b.time-a.time),a)
    if a.euler and b.euler then
        for i=1,3 do pose.euler[i]=a.euler[i]+(b.euler[i]-a.euler[i])*t end
        pose.q=M.quaternion(pose.euler)
    else
        local qa,qb=a.q or M.quaternion(a.euler or {0,0,0}),b.q or M.quaternion(b.euler or {0,0,0})
        local dot=0; for i=1,4 do dot=dot+qa[i]*qb[i] end
        local length=0
        for i=1,4 do pose.q[i]=qa[i]+((dot<0 and -qb[i] or qb[i])-qa[i])*t; length=length+pose.q[i]^2 end
        length=math.sqrt(length); for i=1,4 do pose.q[i]=pose.q[i]/length end
        pose.euler=M.euler(pose.q)
    end
    return pose
end
function M.sync(E)
    local clip=E.project.clips[E.clip]; local c=E.poseContext
    local context=not c or c.project~=E.project or c.clip~=clip or c.frame~=E.frame or c.mode~=E.mode
    local time=not c or c.time~=E.time
    if context or time then M.clear(E) end
    if context or c.part~=E.selected then E.keyOrigin=nil; E.worldDrag=nil end
    if context or time or c.part~=E.selected or c.revision~=E.geometryRevision then
        E.pose=E.drafts and E.drafts[E.selected] and Model.copy(E.drafts[E.selected]) or M.sample(clip,E.selected,E.time)
        E.poseContext={project=E.project,clip=clip,frame=E.frame,mode=E.mode,time=E.time,part=E.selected,revision=E.geometryRevision}
    end
end
-- Build affine transforms once per pose change; no geometry scan is required.
function M.transforms(E)
    if not E.posePartMap or E.poseMapRevision~=E.geometryRevision or E.poseMapFrame~=E.frame then
        E.posePartMap={}
        for _,p in ipairs(E.project.frames[E.frame].parts) do E.posePartMap[p.id]=p end
        E.poseMapRevision=E.geometryRevision; E.poseMapFrame=E.frame
    end
    local map={}; local clip=E.mode=='animate' and E.project.clips[E.clip] or nil
    local function transform(id,x,y,z)
        local item=map[id]
        if not item then
            local p=E.posePartMap[id]; if not p then return x,y,z end
            local pose=E.drafts and E.drafts[id] or M.sample(clip,id,E.time)
            local q=normalized(p.q); local r=M.multiply(M.multiply(q,pose.q),{-q[1],-q[2],-q[3],q[4]})
            item={part=p,pose=pose,q=r}; map[id]=item
        end
        local p,k=item.part,item.pose
        x,y,z=M.rotate(item.q,(x-p.pivot[1])*k.sx,(y-p.pivot[2])*k.sy,(z-p.pivot[3])*k.sz)
        return transform(p.parent,x+p.pivot[1]+k.x,y+p.pivot[2]+k.y,z+p.pivot[3]+k.z)
    end
    return transform
end
return M
