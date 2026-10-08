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

package.path='editor/?.lua;'..package.path
dofile('editor/mesh_to_sprite_editor.lua')
local init,loop,finish=onInitScene,onLoop,onEndScene
local P=require 'mesh_to_sprite_project'
local C=require 'mesh_to_sprite_capture'
local A=require 'mesh_to_sprite_animation'
local started,stage,idle,reads=0,0,0,0
local unlit
local temp=os.tmpname(); os.remove(temp)
local output=temp..'.spt'; local project=temp..'.mesh2sprite'; local fixture=temp..'.msh'
local originalPose=A.pose
A.pose=function(...) reads=reads+1; return originalPose(...) end
local e=MeshToSpriteEditor
local image=tImGui.Image
local seenImages={}
local suggestedOutput
local button,saveFile=tImGui.Button,mbm.saveFile
tImGui.Button=function(label,...)
    if stage==33 and label==tLang.L('m2s_export') then return true end
    return button(label,...)
end
mbm.saveFile=function(path,...)
    if stage==33 then suggestedOutput=path;return nil end
    return saveFile(path,...)
end
local visitedFrames={}
local chooseBite=false
local combo=tImGui.Combo
tImGui.Combo=function(label,...)
    if chooseBite and label=='##clip' then chooseBite=false;return true,2 end
    return combo(label,...)
end
tImGui.Image=function(info,size,uv0,uv1,...)
    if info==e.texture or info==e.spriteTexture then
        local topOrigin=mbm.get('USE_DIRECTX9') or mbm.get('USE_DIRECTX11') or mbm.get('USE_METAL')
        assert(uv0 and uv1 and uv0.y==(topOrigin and 0 or 1) and uv1.y==(topOrigin and 1 or 0),
            'Render-target preview has inverted orientation')
        seenImages[info==e.texture and 'capture' or 'sprite']=true
    elseif info==e.thumbnail then
        assert(not uv0 and not uv1,'PNG thumbnail must retain standard UVs')
        assert(info:isValid(),'Captured frame texture was released: '..tostring(e.selected))
        visitedFrames[e.selected or 1]=true
        seenImages.png=true
    end
    if uv0 then return image(info,size,uv0,uv1,...) end
    return image(info,size)
end
local function checkImages()
    assert(#e.images==e.project.count)
    local previous,distinct=nil,false
    for _,file in ipairs(e.images) do
        local bytes,w,h=mbm.readImagePixels(file,'alpha'); assert(bytes and w==128 and h==128)
        local visible,clear=false,false
        for i=1,#bytes do if bytes:byte(i)>0 then visible=true else clear=true end end
        if not (visible and clear) then
            print('BAD IMAGE',stage,file,w,h,e.project.camera.distance,e.project.camera.fx,e.project.camera.fy,e.project.camera.fz)
            local rgba=assert(mbm.readImagePixels(file));assert(mbm.writeImagePixels('/tmp/m2s-bad.png',rgba,w,h))
        end
        assert(visible and clear,'Missing visible pixels or transparency at stage '..stage)
        if previous and bytes~=previous then distinct=true end; previous=bytes
    end
    assert(distinct,'Animation frames are identical')
end
local function setup(path)
    e.loadSource(path)
    e.project.width=128;e.project.height=128;e.project.count=4
    e.project.cycle=false;e.refresh=true
end
local function staticFixture(translucent)
    local d=meshDebug:new();d:setStride(3);d:enableNormal(false)
    for i=1,3 do
        local f=d:addFrame(3);local s=d:addSubSet(f);local x=(i-2)*20
        assert(d:addVertex(f,s,{{x=x-10,y=-10,z=0,u=0,v=1},{x=x+10,y=-10,z=0,u=1,v=1},{x=x,y=10,z=0,u=0.5,v=0}}))
        assert(d:addIndex(f,s,{1,2,3})); assert(d:setTexture(f,s,translucent and '#FFFFFF80' or '#FFFFFFFF'))
    end
    d:setType('mesh');d:setModeDraw('TRIANGLES');d:setModeCullFace('BACK');d:setModeFrontFace('CCW')
    assert(d:addAnim('static_walk',1,3,0.1,mbm.GROWING_LOOP));assert(d:save(translucent and fixture..'_alpha.msh' or fixture,false,false))
end
function onInitScene()
    local ok,err=pcall(function()
        init();started=mbm.getTimeRun();mbm.addPath('src/test-lib')
        setup('src/test-lib/ChompBot.msh')
        assert(e.animation.articulated[2].name=='bite')
        chooseBite=true
        assert(not pcall(function() e.animation.object:setIndexFrame(0) end))
        staticFixture();staticFixture(true)
    end)
    if not ok then print('M2S SMOKE FAIL '..tostring(err));mbm.quit() end
end
local function advance()
    if stage==0 and e.texture then
        assert(e.project.clip=='bite' and e.project.name=='bite','Clip name did not initialize output name')
        e.capture();stage=1
    elseif stage==1 and e.captured then
        checkImages();e.selected=2;stage=30
    elseif stage==30 then e.selected=4;stage=31
    elseif stage==31 then e.selected=2;stage=32
    elseif stage==32 then e.selected=1;stage=33
    elseif stage==33 then
        assert(visitedFrames[1] and visitedFrames[2] and visitedFrames[4])
        assert(suggestedOutput=='ChompBot.spt','Wrong export filename suggestion')
        e.project.name='custom_bite';e.export(output);e.save(project);stage=2
    elseif stage==2 then
        assert(e.spritePreview:getTotalFrame()==4)
        assert(e.spriteTarget:save(temp..'_sprite.png'))
        local bytes=assert(mbm.readImagePixels(temp..'_sprite.png','alpha'))
        local visible=false;for i=1,#bytes do if bytes:byte(i)>0 then visible=true;break end end
        assert(visible,'Exported sprite renders blank')
        local original=assert(mbm.readImagePixels(e.images[1],'alpha'))
        assert(bytes==original,'Exported sprite changed alpha or orientation')
        e.open(project);assert(e.project.clip=='bite' and e.project.stop==1.5)
        assert(e.project.name=='custom_bite','Reopen overwrote custom animation name')
        assert(#e.images==0 and not e.captured,'Project restored image cache')
        e.capture();stage=3
    elseif stage==3 and e.captured then
        checkImages();setup('src/test-lib/Lorekeeper-walk.msh');stage=4
    elseif stage==4 then e.capture();stage=5
    elseif stage==5 and e.captured then
        checkImages();unlit=assert(mbm.readImagePixels(e.images[1]));e.project.light.enabled=true;e.refresh=true;stage=6
    elseif stage==6 then e.capture();stage=7
    elseif stage==7 and e.captured then
        checkImages();assert(unlit~=mbm.readImagePixels(e.images[1]),'Lighting did not change pixels');setup(fixture);e.project.stop=0.3;e.project.camera.azimuth=math.pi;e.project.camera.elevation=0;e.project.camera.distance=100
        e.project.camera.fx=0;e.project.camera.fy=0;e.project.camera.fz=0;stage=8
    elseif stage==8 then e.capture();stage=9
    elseif stage==9 and e.captured then
        checkImages();assert(e.project.name=='static_walk');e.capture();C.cancel(e);assert(not e.job and #e.images==0)
        setup(fixture..'_alpha.msh');e.project.stop=0.3;stage=10
    elseif stage==10 then e.capture();stage=11
    elseif stage==11 and e.captured then
        checkImages()
        local alpha=assert(mbm.readImagePixels(e.images[1],'alpha'));local found=false
        for i=1,#alpha do local a=alpha:byte(i);if a>0 then assert(math.abs(a-128)<=2,'Translucent alpha is not straight');found=true end end
        assert(found);idle=reads;stage=20
    elseif stage>=20 then
        assert(reads==idle,'Idle editor keeps seeking')
        stage=stage+1
        if stage==26 then
            assert(seenImages.capture and seenImages.sprite and seenImages.png,'Missing preview coverage')
            assert(e.versions:find(mbm.get('version'),1,true) and e.versions:find(tImGui.GetVersion(),1,true),'Missing About versions')
            print('M2S SMOKE OK: articulated / skeletal / static / PNG / SPT / project / light / cancel / idle');mbm.quit() end
    end
end
function onLoop(delta)
    local ok,err=pcall(function() loop(delta);advance();assert(mbm.getTimeRun()-started<25,'timeout') end)
    if not ok then print('M2S SMOKE FAIL '..tostring(err));mbm.quit() end
end
function onEndScene()
    finish();os.remove(project);os.remove(fixture);os.remove(fixture..'_alpha.msh')
    -- Keep exported SPT/PNGs and a sprite render in /tmp for visual inspection.
    print('M2S ARTIFACT '..output)
end
