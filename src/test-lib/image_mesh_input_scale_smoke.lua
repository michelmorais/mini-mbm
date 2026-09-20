--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]

package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local api={}; assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local frame,started,failed=0,0,false
local function runInit()
    init(); started=mbm.getTimeRun()
    local project=os.getenv('MBM_IMAGE_MESH_PROJECT')
    if project then assert(api.openProject(project)) else
        local pixels={}; for i=1,128*96 do pixels[#pixels+1]=170; pixels[#pixels+1]=180; pixels[#pixels+1]=190 end
        local path='/tmp/ime_input_scale.png'
        assert(mbm.createTexture(pixels,128,96,3,'ime_input_scale',path)); api.openImage(path)
        api.action(function(p) Model.add(p,'rectangle',66,3,32,24) end)
    end
    api.select(api.state.project.regions[1].id); tUtil.sMessageOverlay=nil
end
function onInitScene()
    local ok,err=pcall(runInit)
    if not ok then failed=true; print('IMAGE MESH INPUT SCALE FAIL: '..tostring(err)); mbm.quit() end
end
local function verifyCorner(rightColumn)
    local e=api.state; local c=e.camera2d; local Canvas=require 'image_mesh_canvas'
    local r=e.project.regions[1]; local before=Model.copy(r)
    e.tool='select'; api.fit(e); Canvas.sync(e)
    if rightColumn then
        local t=e.canvasTransform
        local hx=t.x+(r.x+r.w-1)*t.scale
        local hy=t.y+(r.y+r.h-1)*t.scaleY
        c:setPos(c.x+(hx-(e.screenW-140))/c.sx,c.y+((e.screenH-100)-hy)/c.sy)
        Canvas.sync(e)
    end
    local t=e.canvasTransform
    local hx=t.x+(r.x+r.w-1)*t.scale
    local hy=t.y+(r.y+r.h-1)*t.scaleY
    -- Compare against the engine's world-to-screen conversion, not just our hit math.
    local sx,sy=mbm.to2ds((r.x+r.w-1-e.project.image.width/2)*e.zoom,
        (e.project.image.height/2-r.y-r.h+1)*e.zoom)
    assert(math.abs(sx*c.sx-hx)<0.05 and math.abs(sy*c.sy-hy)<0.05,'draw/input coordinate mismatch')
    if rightColumn then assert(hx>e.screenW-e.rightbar and hy>400) end
    local captured=tImGui.GetWantCaptureMouse
    tImGui.GetWantCaptureMouse=function() return false end
    -- Match CORE_MANAGER and SCENE_SCRIPT: physical pixels / scale, then integer coordinates.
    local x,y=math.floor(hx/c.sx),math.floor(hy/c.sy)
    onTouchDown(0,x,y)
    assert(e.drag and e.drag.mode=='resize','visible handle rejected')
    onTouchMove(0,x+12,y+9); onTouchUp(0,x+12,y+9)
    assert(e.project.regions[1].w>before.w and e.project.regions[1].h>before.h,'resize did not apply')
    api.undo(false); assert(e.project.regions[1].w==before.w and e.project.regions[1].h==before.h)
    tImGui.GetWantCaptureMouse=function() return true end
    onTouchDown(0,x,y); assert(not e.drag,'UI capture ignored')
    tImGui.GetWantCaptureMouse=captured
end
local function verifyLeftDrag()
    local e=api.state; local c=e.camera2d; local Canvas=require 'image_mesh_canvas'
    e.tool='select'; api.fit(e); Canvas.sync(e)
    local captured=tImGui.GetWantCaptureMouse; tImGui.GetWantCaptureMouse=function() return false end
    local t=e.canvasTransform
    local x=(t.x+e.project.image.width*0.1*t.scale)/c.sx
    local y=(t.y+e.project.image.height*0.8*t.scaleY)/c.sy
    local cx,cy=c.x,c.y; local revision=e.revision; local builds=e.canvasBuilds
    onTouchDown(0,x,y); assert(e.panDrag and not e.drag,'empty space did not start camera drag')
    onTouchMove(0,x+20,y+12); onTouchUp(0,x+20,y+12)
    assert(math.abs(c.x-cx+20)<0.001 and math.abs(c.y-cy-12)<0.001)
    Canvas.sync(e)
    assert(e.revision==revision and e.canvasBuilds==builds,'camera drag edited/rebuilt shapes')
    api.fit(e); Canvas.sync(e)
    t=e.canvasTransform
    -- Empty scene outside the image also pans, and the choice stays fixed until release.
    x=(e.screenW/2)/c.sx; y=50/c.sy
    onTouchDown(0,x,y); assert(e.panDrag,'scene background did not start camera drag')
    onTouchMove(0,x+10,y+10); onTouchUp(0,x+10,y+10)
    api.fit(e); Canvas.sync(e)
    local r=e.project.regions[1]
    x=(t.x+(r.x+r.w*0.4)*t.scale)/c.sx
    y=(t.y+(r.y+r.h*0.4)*t.scaleY)/c.sy
    cx,cy=c.x,c.y
    onTouchDown(0,x,y); assert(e.drag and e.drag.mode=='move' and not e.panDrag,'shape did not get drag')
    onTouchMove(0,x-12,y+10); onTouchUp(0,x-12,y+10)
    assert(c.x==cx and c.y==cy,'shape drag moved camera'); api.undo(false)
    e.control=true
    onTouchDown(0,x,y); assert(not e.panDrag and not e.drag,'Ctrl selection started pan')
    onTouchUp(0,x,y); e.control=false; api.select(e.project.regions[1].id)
    tImGui.GetWantCaptureMouse=captured
end
local function runFrame(dt)
    loop(dt)
    local e=api.state
    if frame==10 then e.camera2d:scaleToScreen(e.screenW/2,e.screenH/2,'xy') end
    if frame==12 then verifyLeftDrag(); verifyCorner(false); verifyCorner(true) end
    if frame==20 then e.camera2d:scaleToScreen(e.screenW/2,e.screenH/1.5,'xy') end
    if frame==22 then verifyLeftDrag(); verifyCorner(false); verifyCorner(true) end
    if frame==30 then e.camera2d:scaleToScreen(e.screenW,e.screenH,'xy') end
    if frame==32 then
        verifyLeftDrag(); verifyCorner(false); verifyCorner(true)
        print('IMAGE MESH INPUT SCALE / RIGHT COLUMN / PROJECT / LEFT DRAG OK'); mbm.quit()
    end
end
function onLoop(dt)
    if failed then return end
    frame=frame+1
    local ok,err=pcall(runFrame,dt)
    if not ok then failed=true; print('IMAGE MESH INPUT SCALE FAIL: '..tostring(err)); mbm.quit() end
    if mbm.getTimeRun()-started>8 then print('IMAGE MESH INPUT SCALE TIMEOUT'); mbm.quit() end
end
function onEndScene() finish() end
