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
local editor={}
assert(loadfile('editor/articulated_sprite_editor.lua'))(editor)
local init,loop=onInitScene,onLoop
local Model=require 'articulated_sprite_model'
local G=require 'articulated_sprite_geometry'
local cases={{1,0,0},{0.5,0,0},{2,35,-22},{1.3,-80,65}}
local caseIndex,stage=1,0
local imagePath
function onInitScene()
    init()
    imagePath=os.tmpname(); os.remove(imagePath); imagePath=imagePath..'.png'
    local pixels={}
    for i=1,16*16 do
        for _,v in ipairs({255,180,40,255}) do pixels[#pixels+1]=v end
    end
    mbm.createTexture(pixels,16,16,4,'ase_pick_fixture',imagePath)
    editor.addImage(imagePath)
    local E=editor.state
    local rings=G.form('rectangle',{x=0,y=0,w=4,h=6},2,{})
    local vertices,indices=G.triangulate(rings,2)
    local p=Model.addPart(E.project,1,1,rings,vertices,indices,{})
    p.x,p.y=85,-47
    p.pivot.x,p.pivot.y=p.x,p.y
    E.project.options.showSource=false
    editor.rebuild()
end
local function input(x,y,click,down,hovered,mouseButton,active,captured)
    mouseButton=mouseButton or 0
    local saved={}
    local overrides={
        GetMousePos=function() return {x=x,y=y} end,
        IsAnyWindowHovered=function() return hovered or false end,
        IsAnyItemActive=function() return active or false end,
        GetWantCaptureMouse=function() return captured or false end,
        IsMouseClicked=function(button) return button==mouseButton and click end,
        IsMouseDown=function(button) return button==mouseButton and down end,
        IsMouseReleased=function() return false end,
    }
    for name,fn in pairs(overrides) do saved[name]=tImGui[name]; tImGui[name]=fn end
    local ok,err=pcall(editor.worldInput)
    for name,fn in pairs(saved) do tImGui[name]=fn end
    assert(ok,err)
end
function onLoop(delta)
    local E=editor.state
    local case=cases[caseIndex]
    if stage==0 then
        local width,height=mbm.getRealSizeScreen()
        E.camera:scaleToScreen(width/case[1],height/case[1],'xy')
        E.camera:setPos(case[2],case[3])
        stage=1
        return -- Allow the engine's camera projection cache to update.
    end
    local p=E.project.frames[1].parts[1]
    local x,y=p.x,p.y
    -- Engine projection returns scaled input coordinates; ImGui uses pixels.
    local screenX,screenY=mbm.to2ds(x,y)
    screenX,screenY=screenX*E.camera.sx,screenY*E.camera.sy
    E.selected=0; E.worldDrag=nil
    input(screenX+6*E.camera.sx,screenY,false,false)
    input(screenX+6*E.camera.sx,screenY,true,false)
    assert(E.selected==0,'click outside small part selected it')
    input(screenX,screenY,true,true)
    assert(E.selected==p.id and E.worldDrag,'click at rendered center missed small part')
    assert(math.abs(p.x-x)<0.001 and math.abs(p.y-y)<0.001,'part jumped on mouse down')
    input(screenX+12,screenY+9,false,true)
    assert(math.abs(p.x-x-12/E.camera.sx)<0.001,'incorrect horizontal drag')
    assert(math.abs(p.y-y+9/E.camera.sy)<0.001,'incorrect vertical drag')
    E.worldDrag=nil
    assert(E.pan==nil,'dragging a part also started camera movement')
    local px,py=p.x,p.y
    for _,button in ipairs({0,2}) do
        local cx,cy=E.camera.x,E.camera.y
        input(20,20,true,true,true,button)
        assert(E.pan==nil,'GUI click started camera movement')
        input(20,20,true,true,false,button)
        assert(E.pan and E.pan.button==button,'empty click did not start camera movement')
        input(44,38,false,true,false,button)
        assert(math.abs(E.camera.x-cx+24/E.camera.sx)<0.001,'incorrect camera drag X')
        assert(math.abs(E.camera.y-cy-18/E.camera.sy)<0.001,'incorrect camera drag Y')
        assert(p.x==px and p.y==py,'camera movement changed the selected part')
        input(44,38,false,false,false,button)
        assert(E.pan==nil,'camera movement did not stop on release')
        for _,capture in ipairs({{true,false,false},{false,true,false},{false,false,true}}) do
            local beforeX,beforeY=E.camera.x,E.camera.y
            input(20,20,true,true,false,button)
            input(44,38,false,true,capture[1],button,capture[2],capture[3])
            assert(E.pan==nil,'GUI did not cancel an existing camera gesture')
            assert(E.camera.x==beforeX and E.camera.y==beforeY,'camera moved while GUI captured mouse')
            input(50,40,false,true,false,button)
            assert(E.pan==nil,'camera gesture resumed after leaving GUI')
            input(20,20,true,true,capture[1],button,capture[2],capture[3])
            assert(E.pan==nil,'active GUI control started camera movement')
        end
    end
    loop(delta)
    caseIndex=caseIndex+1; stage=0
    if caseIndex>#cases then
        print('ARTICULATED SPRITE SMALL PART PICK/DRAG AND CAMERA PAN OK')
        mbm.quit()
    end
end
local finalize=onEndScene
function onEndScene()
    finalize()
    if imagePath then os.remove(imagePath) end
end
