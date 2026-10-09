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

local M = {}
function M.copy(v)
    if type(v) ~= 'table' then return v end
    local out = {}; for k,x in pairs(v) do out[k]=M.copy(x) end; return out
end
-- Static sprite animation names reserve one byte of their 32-byte field for NUL.
function M.suggestAnimationName(name)
    if name=='' then return 'animation' end
    local last=math.min(#name,31)
    if #name>last then
        -- Exclude a character that crosses the byte limit, preserving UTF-8.
        while last>0 do
            local byte=name:byte(last+1)
            if byte<128 or byte>=192 then break end
            last=last-1
        end
    end
    return name:sub(1,last)
end
function M.defaults()
    return {version=3, source='', signature='', static=1, staticName='', staticMode=0, staticInterval=1, baseFrame=1, skinning='auto', kind='none', clip='',
        start=0, stop=1, count=16, cycle=true, frameTime=1/16, width=256, height=256,
        frameWidth=256, frameHeight=256, followImage=true, keepAspect=true, showPivot=false,
        pivotX=0.5, pivotY=0.5, output='', name='animation',
        position={x=0,y=0,z=0}, rotation={x=0,y=0,z=0}, scale={x=1,y=1,z=1},
        animateTransform=false,
        finalTransform={position={x=0,y=0,z=0},rotation={x=0,y=0,z=0},scale={x=1,y=1,z=1}},
        camera={azimuth=0.3,elevation=0.2,roll=0,distance=500,fx=0,fy=0,fz=0,near=0.1,far=100000},
        background={r=0,g=0,b=0,a=0},
        light={enabled=false, ambient={r=0.3,g=0.3,b=0.3,a=1},
            color={r=1,g=1,b=1,a=1}, orbit={azimuth=0.6,elevation=-0.8},
            point={r=0,g=0,b=0,a=1},position={x=0,y=100,z=-100},radius=500}}
end
local function check(v, ref, path)
    assert(type(v)==type(ref), path..': invalid type')
    if type(v)=='number' then assert(v==v and math.abs(v)<1e12,path..': invalid number') end
    if type(v)=='string' then assert(#v<=4096 and not v:find('%z'),path..': invalid text') end
    if type(ref)=='table' then
        for k,x in pairs(ref) do check(v[k],x,path..'.'..k) end
        for k in pairs(v) do assert(ref[k]~=nil,path..': unknown field '..tostring(k)) end
    end
end
function M.validate(p)
    check(p,M.defaults(),'project')
    assert(p.version==3,'Unsupported project version')
    assert(p.kind=='none' or p.kind=='skeletal' or p.kind=='articulated','Invalid animation kind')
    assert(p.staticMode>=0 and p.staticMode<=6 and p.staticMode%1==0 and p.staticInterval>0,'Invalid static playback')
    assert(p.baseFrame>=1 and p.baseFrame%1==0,'Invalid base frame')
    assert(p.skinning=='auto' or p.skinning=='lbs' or p.skinning=='dqs','Invalid skinning method')
    assert(p.static>=1 and p.static%1==0,'Invalid static animation')
    assert(p.count>=1 and p.count<=4096 and p.count%1==0,'Frame count must be 1..4096')
    assert(p.start>=0 and p.stop>=p.start and p.stop<=36000,'Invalid capture interval')
    assert(p.frameTime>0 and p.frameTime<=3600,'Invalid output frame time')
    for _,k in ipairs({'width','height'}) do assert(p[k]>=8 and p[k]<=4096 and p[k]%1==0,'Resolution must be 8..4096') end
    assert(4.0*p.width*p.height*p.count<=1024^3,'Capture exceeds 1 GiB uncompressed budget')
    assert(p.camera.near>0 and p.camera.far>p.camera.near and p.camera.distance>0,'Invalid camera')
    assert(math.abs(p.camera.elevation)<math.pi/2,'Camera elevation at pole')
    for _,c in ipairs({p.background,p.light.ambient,p.light.color,p.light.point}) do
        for _,k in ipairs({'r','g','b','a'}) do assert(c[k]>=0 and c[k]<=1,'Invalid color') end
    end
    assert(p.frameWidth>0 and p.frameWidth<=1000000 and p.frameHeight>0 and p.frameHeight<=1000000,'Invalid frame size')
    assert(p.light.radius>0,'Invalid light radius')
    assert(p.pivotX>=0 and p.pivotX<=1 and p.pivotY>=0 and p.pivotY<=1,'Invalid pivot')
    assert(#p.name>0 and #p.name<32,'Animation name must contain 1..31 bytes')
    return p
end
-- Use the final sampled instant as the endpoint so loop sampling also reaches
-- the requested transform. Preview holds it until the playback interval restarts.
function M.transformProgress(p,time)
    if not p.animateTransform or p.count<=1 then return 0 end
    local span=M.sample(p,p.count)-p.start
    if span<=0 then return 0 end
    return math.max(0,math.min(1,(time-p.start)/span))
end
function M.transformComponents(p,key,progress)
    local a,b=p[key],p.finalTransform[key]
    return a.x+(b.x-a.x)*progress,a.y+(b.y-a.y)*progress,a.z+(b.z-a.z)*progress
end
function M.frameSize(p)
    if p.followImage then return p.width,p.height end
    return p.frameWidth,p.frameHeight
end
function M.setFrameDimension(p,field,value)
    local limit=1000000
    if p.keepAspect then
        local ratio=p.width/p.height
        limit=math.min(limit,field=='frameWidth' and limit*ratio or limit/ratio)
    end
    p[field]=math.max(0.01,math.min(value,limit))
    value=p[field]
    if p.keepAspect then
        if field=='frameWidth' then p.frameHeight=value*p.height/p.width
        else p.frameWidth=value*p.width/p.height end
    end
end
function M.fitSize(width,height,availableWidth,availableHeight)
    local scale=math.min(math.max(1,availableWidth)/width,math.max(1,availableHeight)/height)
    return {x=width*scale,y=height*scale}
end
function M.sample(p,i)
    assert(i>=1 and i<=p.count)
    local divisor=p.cycle and p.count or math.max(1,p.count-1)
    return p.start+(i-1)*(p.stop-p.start)/divisor
end
-- The existing seven frame playback modes, sampled at frame boundaries.
function M.staticFrame(a,t)
    local n=a.last-a.first+1
    local step=math.floor(t/a.interval+1e-7)
    local mode=a.mode
    if mode==0 or n==1 then return a.first end
    if mode==1 then return a.first+math.min(step,n-1) end
    if mode==2 then return a.first+step%n end
    if mode==3 then return a.last-math.min(step,n-1) end
    if mode==4 then return a.last-step%n end
    local period=2*(n-1)
    if mode==5 and step>=period then return a.first end
    step=step%period
    return a.first+(step<n and step or period-step)
end
function M.dirname(path) return path:match('^(.*[/\\])') or './' end
function M.basename(path) return path:match('[^/\\]+$') or path end
function M.absolute(path,base)
    if path:match('^[/\\]') or path:match('^%a:') then return path end
    return (base or './')..path
end
function M.relative(path,base)
    if path:sub(1,#base)==base then return path:sub(#base+1) end
    return path
end
function M.signature(path)
    local f=assert(io.open(path,'rb')); local a,b=1,0; local size=0
    while true do
        local block=f:read(65536); if not block then break end
        size=size+#block
        for i=1,#block do a=(a+block:byte(i))%65521; b=(b+a)%65521 end
    end
    f:close(); return string.format('%d:%d:%d',size,a,b)
end
local function serialize(v)
    if type(v)=='string' then return string.format('%q',v) end
    if type(v)~='table' then return tostring(v) end
    local keys={}; for k in pairs(v) do keys[#keys+1]=k end; table.sort(keys)
    local out={'{'}; for _,k in ipairs(keys) do out[#out+1]='['..string.format('%q',k)..']='..serialize(v[k])..',' end
    out[#out+1]='}'; return table.concat(out,'\n')
end
-- POSIX rename replaces atomically. Windows needs an explicit backup when the
-- destination exists; restore it if publishing the staged file fails.
function M.publish(stage,path)
    local ok,err=os.rename(stage,path)
    if ok then return end
    local existing=io.open(path,'rb'); if not existing then error(err) end; existing:close()
    local token=os.tmpname(); os.remove(token)
    local backup=path..'.'..M.basename(token)..'.bak'
    assert(os.rename(path,backup))
    local replaced,re=os.rename(stage,path)
    if not replaced then
        local restored,restoreError=os.rename(backup,path)
        assert(restored,'Restore failed: '..tostring(restoreError)..'; backup: '..backup)
        error(re)
    end
    os.remove(backup)
end
function M.save(p,path)
    M.validate(p)
    local data=M.copy(p); local base=M.dirname(path)
    data.source=M.relative(data.source,base); data.output=M.relative(data.output,base)
    local tmp=path..'.tmp'; local f=assert(io.open(tmp,'wb'))
    local ok,err=f:write('return ',serialize(data),'\n'); local closed,ce=f:close()
    if not ok or not closed then os.remove(tmp); error(err or ce) end
    M.publish(tmp,path)
end
function M.load(path)
    local f=assert(io.open(path,'rb')); local data=f:read(65537); f:close()
    assert(#data<=65536,'Project too large')
    local fn=assert(load(data,'@'..path,'t',{}))
    -- Bound execution even for malformed data masquerading as a Lua project.
    local co=coroutine.create(fn)
    debug.sethook(co,function() error('Project instruction limit') end,'',100000)
    local ok,p=coroutine.resume(co); debug.sethook(co)
    assert(ok,p)
    if type(p)=='table' and p.version==1 then
        p.version=2
        p.frameWidth=p.width; p.frameHeight=p.height
        p.followImage=true; p.keepAspect=true; p.showPivot=false
    end
    if type(p)=='table' and p.version==2 then
        p.version=3; p.animateTransform=false
        p.finalTransform={position=M.copy(p.position),rotation=M.copy(p.rotation),scale=M.copy(p.scale)}
    end
    M.validate(p)
    local base=M.dirname(path)
    if p.source~='' then p.source=M.absolute(p.source,base) end
    if p.output~='' then p.output=M.absolute(p.output,base) end
    return p
end
return M
