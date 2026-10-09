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

local P=require 'mesh_to_sprite_project'
local M={}
local function dpCall(fn,...)
    local r=table.pack(pcall(fn,...))
    if not r[1] then print('[mesh_to_sprite_animation] '..tostring(r[2])) end
    return table.unpack(r,1,r.n)
end
function M.newObject(path,method)
    local enabled=mbm.getLightState('3d').enabled
    -- Default-lit shaders can be switched off at runtime; unlit ones cannot be lit.
    mbm.setLightEnabled('3d',true)
    local obj=mesh:new('3d')
    local ok,err=dpCall(function()
        assert(obj:setSkeletalSkinningMethod(method or 'auto'))
        assert(obj:load(path),'Cannot load mesh')
    end)
    mbm.setLightEnabled('3d',enabled)
    if not ok then obj:destroy(); error(err) end
    return obj
end
function M.load(path,method)
    mbm.addPath(P.dirname(path))
    local d=meshDebug:new(); assert(d:load(path),'Cannot read mesh')
    local obj=M.newObject(path,method)
    local a={object=obj,debug=d,path=path,skinning=method or 'auto',static={},skeletal={},articulated={}}
    for i=1,obj:getTotalAnim() do
        local name,first,last,interval,mode=d:getAnim(i)
        assert(name and last>=first,'Invalid static animation metadata')
        if interval<=0 then assert(mode==0 or first==last,'Invalid frame interval'); interval=1 end
        local count=last-first+1
        local duration=count*interval
        if mode==5 or mode==6 then duration=math.max(1,2*(count-1))*interval end
        a.static[i]={name=name,first=first,last=last,interval=interval,mode=mode,duration=duration}
    end
    local report=d:getSkeletalAnimationReport()
    for i=1,obj:getTotalSkeletalAnimations() do
        a.skeletal[i]={name=obj:getSkeletalAnimationName(i),duration=obj:getSkeletalAnimationDuration(i),loop=report[i] and report[i].loop}
    end
    for i=1,d:getTotalArticulatedAnimations() do
        local name,duration,speed,priority,loop=d:getArticulatedAnimation(i)
        a.articulated[i]={name=name,duration=duration,speed=speed,priority=priority,loop=loop}
    end
    M.stop(a)
    return a
end
function M.stop(a)
    a.object:stopSkeletalAnimation()
    for _,c in ipairs(a.articulated) do a.object:disableArticulatedAnimation(c.name) end
    a.object:setTypeAnim(mbm.PAUSED)
end
function M.configure(a,p)
    M.stop(a)
    local base=assert(a.static[p.static],'Static animation missing')
    assert(p.staticName=='' or base.name==p.staticName,'Static animation changed; select again')
    assert(p.baseFrame>=base.first and p.baseFrame<=base.last,'Base frame outside animation')
    a.object:setAnim(p.static)
    a.object:setTypeAnim(mbm.PAUSED)
    a.clip=nil
    if p.kind~='none' then
        for _,c in ipairs(a[p.kind]) do if c.name==p.clip then a.clip=c; break end end
        assert(a.clip,'Selected clip missing')
        if p.kind=='skeletal' then
            assert(a.object:playSkeletalAnimation(p.clip))
            assert(a.object:pauseSkeletalAnimation())
            a.object:disableAutomaticSkeletalRootMotion()
        else
            assert(a.object:playArticulatedAnimation(p.clip,a.clip.priority,0,1))
            assert(a.object:pauseArticulatedAnimation(p.clip))
        end
    end
    a.config=P.copy(p)
end
function M.pose(a,t)
    local p=a.config; local obj=a.object
    if p.animateTransform then
        local progress=P.transformProgress(p,t)
        obj:setPos(P.transformComponents(p,'position',progress))
        local x,y,z=P.transformComponents(p,'rotation',progress)
        obj:setAngle(math.rad(x),math.rad(y),math.rad(z))
        obj:setScale(P.transformComponents(p,'scale',progress))
    end
    local base=a.static[p.static]
    obj:setIndexFrame(p.staticMode==0 and p.baseFrame or P.staticFrame({first=base.first,last=base.last,interval=p.staticInterval,mode=p.staticMode},t))
    if p.kind=='skeletal' then
        local c=a.clip; local time=t
        if c.loop and c.duration>0 and not (not p.cycle and time==c.duration) then time=time%c.duration end
        assert(obj:seekSkeletalAnimation(time))
    elseif p.kind=='articulated' then
        local c=a.clip; local time=t*c.speed
        if c.loop and c.duration>0 and not (not p.cycle and time==c.duration) then time=time%c.duration else time=math.min(time,c.duration) end
        assert(obj:seekArticulatedAnimation(p.clip,time))
    end
end
function M.release(a)
    if a then a.object:destroy(); a.debug=nil end
end
return M
