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
local A=require 'mesh_to_sprite_animation'
local Export=require 'mesh_to_sprite_export'
local Pixels=require 'mesh_to_sprite_pixels'
local M={}
function M.applyLight(p)
    local l=p.light
    mbm.clearPointLights('3d')
    mbm.setLightEnabled('3d',l.enabled)
    mbm.setAmbientLight('3d',l.ambient)
    mbm.setDirectionalLight('3d',tUtil.dirFromOrbit(l.orbit),l.color)
    mbm.setPointLight('3d',l.position,l.radius,l.point)
end
function M.restoreLight(l)
    mbm.clearPointLights('3d')
    mbm.setLightEnabled('3d',l.enabled)
    mbm.setAmbientLight('3d',l.ambientColor)
    mbm.setDirectionalLight('3d',l.directionalDirection,l.directionalColor)
    mbm.setPointLight('3d',l.pointPosition,l.pointRadius,l.pointColor)
    for _,v in ipairs(l.pointLights or {}) do mbm.addPointLight('3d',v.position,v.radius,v.color) end
end
function M.camera(p,rt)
    local c=p.camera; local ce=math.cos(c.elevation)
    local x=c.fx+c.distance*ce*math.sin(c.azimuth)
    local y=c.fy+c.distance*math.sin(c.elevation)
    local z=c.fz+c.distance*ce*math.cos(c.azimuth)
    local cam=rt:getCamera('3d')
    local sa,ca=math.sin(c.azimuth),math.cos(c.azimuth)
    local se=math.sin(c.elevation); local sr,cr=math.sin(c.roll),math.cos(c.roll)
    -- Up and right use the same left-handed orbit basis as drawOrbitGizmo.
    local upX=-se*sa*cr+ca*sr
    local upY=ce*cr
    local upZ=-se*ca*cr-sa*sr
    cam:setPos(x,y,z); cam:setFocus(c.fx,c.fy,c.fz); cam:setUp(upX,upY,upZ)
    cam:setNear(c.near); cam:setFar(c.far)
    return x,y,z
end
function M.prepare(e)
    local p=P.validate(e.project); local a=assert(e.animation,'Load a mesh first')
    if a.skinning~=p.skinning then
        local obj=A.newObject(a.path,p.skinning)
        if e.target then e.target:remove(a.object); e.target:add(obj) end
        a.object:destroy(); a.object=obj; a.skinning=p.skinning
    end
    if not e.target or e.width~=p.width or e.height~=p.height then
        if e.target then e.target:remove(a.object); e.target:destroy() end
        e.target=render2texture:new('2ds')
        local ok,name,info=e.target:create(p.width,p.height,true)
        assert(ok,'Cannot create render target')
        e.target:enableFrame(false); e.target.alwaysRender=true
        e.target:add(a.object); e.texture=info; e.width=p.width; e.height=p.height
    end
    e.target:setColor(p.background.r,p.background.g,p.background.b,p.background.a)
    M.camera(p,e.target); M.applyLight(p)
    local obj=a.object
    obj:setPos(p.position.x,p.position.y,p.position.z)
    obj:setAngle(math.rad(p.rotation.x),math.rad(p.rotation.y),math.rad(p.rotation.z))
    obj:setScale(p.scale.x,p.scale.y,p.scale.z)
    A.configure(a,p)
end
function M.preview(e,time)
    M.prepare(e); A.pose(e.animation,time)
    e.target.visible=true; e.pendingPreview=true
end
function M.clearImages(e)
    if e.spriteTarget then e.spriteTarget:destroy(); e.spriteTarget=nil end
    if e.spritePreview then e.spritePreview:destroy(); e.spritePreview=nil end
    e.spriteTexture=nil; e.spritePlaying=false; e.exported=nil
    for _,file in ipairs(e.images or {}) do os.remove(file) end
    e.images={}; e.captured=nil
    if e.thumbnail then e.thumbnail:release() end
    e.thumbnail=nil; e.thumbnailIndex=nil; e.selected=1
end
function M.begin(e)
    assert(not e.job,'Capture already running')
    M.prepare(e)
    M.clearImages(e)
    local prefix=os.tmpname(); os.remove(prefix)
    e.job={config=P.copy(e.project),index=1,prefix=prefix,phase='pose'}
    e.playing=false
end
function M.cancel(e)
    if e.job then os.remove(e.job.prefix..'_black.png'); os.remove(e.job.prefix..'_white.png') end
    e.job=nil; e.pendingPreview=nil
    if e.target then e.target.visible=false end
    M.clearImages(e)
end
-- Scene logic precedes render in CORE_MANAGER::onLoop. A frozen pose scheduled
-- here renders before the next scene callback; save() performs synchronous readback.
function M.tickSprite(e,delta)
    if not e.spriteTarget then return end
    e.spriteTarget.visible=false
    if e.spriteDirty or e.spritePlaying then
        local config=e.exportConfig;local duration=config.count*config.frameTime
        if e.spritePlaying then
            e.spriteTime=e.spriteTime+delta
            if e.spriteTime>=duration then
                if config.cycle then e.spriteTime=e.spriteTime%duration
                else e.spriteTime=duration; e.spritePlaying=false end
            end
        end
        local index=math.min(config.count,math.floor(e.spriteTime/config.frameTime)+1)
        e.spritePreview:setIndexFrame(index)
        e.spriteTarget.visible=true; e.spriteDirty=false
    end
end
function M.tick(e)
    if e.pendingPreview then e.target.visible=false; e.pendingPreview=nil end
    local j=e.job; if not j then return false end
    if j.phase=='pose' then
        A.pose(e.animation,P.sample(j.config,j.index))
        if j.config.background.a<1 then e.target:setColor(0,0,0,1) end
        e.target.visible=true; j.phase='read'
        return false
    end
    e.target.visible=false
    local file=j.prefix..string.format('_%05d.png',j.index)
    if j.config.background.a<1 then
        local blackPath=j.prefix..'_black.png'; local whitePath=j.prefix..'_white.png'
        if j.phase=='read' then
            assert(e.target:save(blackPath),'Cannot read black capture')
            j.black=assert(mbm.readImagePixels(blackPath));os.remove(blackPath)
            e.target:setColor(1,1,1,1); e.target.visible=true;j.phase='white'
            return false
        end
        assert(e.target:save(whitePath),'Cannot read white capture')
        local white=assert(mbm.readImagePixels(whitePath));os.remove(whitePath)
        local rgba=Pixels.compose(j.black,white,j.config.width,j.config.height,j.config.background)
        e.images[#e.images+1]=file
        assert(mbm.writeImagePixels(file,rgba,j.config.width,j.config.height))
        j.black=nil
    else
        e.images[#e.images+1]=file
        assert(e.target:save(file),'Cannot save captured image')
    end
    if j.index==j.config.count then
        e.captured=j.config; e.job=nil; e.refresh=true; return true
    end
    j.index=j.index+1; j.phase='pose'
    return false
end
local function copyFile(source,dest)
    local input=assert(io.open(source,'rb')); local data=input:read('a'); input:close()
    local out=assert(io.open(dest,'wb')); local ok,err=out:write(data); local closed,ce=out:close()
    assert(ok and closed,err or ce)
end
function M.export(e,path)
    P.validate(e.project)
    assert(e.captured and not e.stale,'Capture is out of date')
    assert(path:lower():match('%.spt$'),'Output must end in .spt')
    local tmp=os.tmpname(); os.remove(tmp)
    local token=P.basename(tmp):gsub('[^%w]','')
    local base=path:sub(1,-5)..'_'..token
    local files={}; e.exportFiles=files
    for i,src in ipairs(e.images) do
        local dest=base..string.format('_%05d.png',i)
        files[#files+1]=dest; copyFile(src,dest)
    end
    mbm.addPath(P.dirname(path))
    local config=P.copy(e.captured)
    config.name=e.project.name; config.frameTime=e.project.frameTime; config.cycle=e.project.cycle
    for _,key in ipairs({'frameWidth','frameHeight','followImage','keepAspect','pivotX','pivotY'}) do config[key]=e.project[key] end
    local stage=base..'.spt'; files[#files+1]=stage
    Export.write(config,{table.unpack(files,1,#files-1)},stage)
    -- Validate the exact staged file before publishing the manifest.
    local spritePreview=sprite:new('2dw'); e.stagedSprite=spritePreview
    assert(spritePreview:load(stage),'Exported sprite cannot be loaded')
    spritePreview.visible=false
    P.publish(stage,path)
    if e.spriteTarget then e.spriteTarget:destroy(); e.spriteTarget=nil end
    if e.spritePreview then e.spritePreview:destroy() end
    e.spritePreview=spritePreview; e.stagedSprite=nil; e.exportFiles=nil
    e.exported=path; e.exportConfig=config
    local rt=render2texture:new('2ds'); e.spriteTarget=rt
    local fw,fh=P.frameSize(config)
    local size=P.fitSize(fw,fh,config.width,config.height)
    local pw,ph=math.max(1,math.floor(size.x+0.5)),math.max(1,math.floor(size.y+0.5))
    local ok,_,info=rt:create(pw,ph,true)
    e.spriteWidth=pw; e.spriteHeight=ph
    spritePreview:setScale(pw/fw,ph/fh)
    assert(ok,'Cannot create sprite preview')
    rt:enableFrame(false); rt.alwaysRender=true; rt:setColor(0,0,0,0); rt:add(spritePreview)
    local cam=rt:getCamera('2d')
    cam:setPos(pw*(0.5-config.pivotX),ph*(config.pivotY-0.5),0)
    spritePreview.visible=true; e.spriteTexture=info; e.spriteTime=0; e.spriteDirty=true; e.spritePlaying=false
end
function M.exportFailed(e)
    if e.stagedSprite then e.stagedSprite:destroy(); e.stagedSprite=nil end
    for _,file in ipairs(e.exportFiles or {}) do os.remove(file) end
    e.exportFiles=nil
end
function M.release(e)
    M.cancel(e); M.exportFailed(e)
    if e.spriteTarget then e.spriteTarget:destroy(); e.spriteTarget=nil end
    e.spriteTexture=nil; e.exported=nil
    if e.target then e.target:destroy(); e.target=nil end
    if e.spritePreview then e.spritePreview:destroy(); e.spritePreview=nil end
    A.release(e.animation); e.animation=nil; e.texture=nil
end
return M
