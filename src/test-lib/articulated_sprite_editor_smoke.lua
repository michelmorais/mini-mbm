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
local init=onInitScene
local loop=onLoop
local start
local tempRoot
function onInitScene()
    init()
    tempRoot=os.tmpname(); os.remove(tempRoot); assert(mbm.createDirectories(tempRoot))
    start=mbm.getTimeRun()
    local pixels={}
    for y=0,31 do for x=0,31 do
        for _,v in ipairs({255,180,40,(x>10 and x<20 and y>10 and y<20) and 0 or 255}) do pixels[#pixels+1]=v end
    end end
    mbm.createTexture(pixels,32,32,4,'ase_fixture',tempRoot..'/fixture.png')
    editor.addImage(tempRoot..'/fixture.png')
    assert(mbm.createDirectories(tempRoot..'/other'))
    mbm.createTexture(pixels,32,32,4,'ase_fixture_other',tempRoot..'/other/fixture.png')
    editor.addImage(tempRoot..'/other/fixture.png')
    editor.state.image=1
    editor.state.kind='alpha'
    editor.generate(false)
    local E=editor.state
    local Model=require 'articulated_sprite_model'
    E.project.clips={{name='walk',duration=1,speed=1,loop=true,blend=1,tracks={}}}
    Model.key(E.project,1,E.selected,0,{angle=0})
    Model.key(E.project,1,E.selected,1,{angle=45})
    local IO=require 'articulated_sprite_io'
    local G=require 'articulated_sprite_geometry'
    -- Run with the engine's 32-bit Lua integers. A 216 x 226 whole-image
    -- rectangle used to overflow the edge intersection products (crossing_contours).
    for _,size in ipairs({{216,226},{1024,1024}}) do
        local rectangle=G.form('rectangle',{x=0,y=0,w=size[1],h=size[2]},2,{})
        assert(G.validate(rectangle),'integer rectangle rejected')
        local _,indices=G.triangulate(rectangle,2)
        assert(#indices==6,'whole image must produce two triangles')
    end
    local valid,reason=G.validate({{{x=0,y=0},{x=216,y=226},{x=20,y=200},{x=216,y=0}}})
    assert(not valid and reason=='crossing_contours','real crossing was accepted')
    print('ARTICULATED SPRITE INTEGER RECTANGLE OK')
    E.project.frames[2]={parts={}}
    local rings=G.form('ring',{x=0,y=0,w=32,h=32},20,{inner=0.3,dx=2})
    local vertices,indices=G.triangulate(rings,20)
    local second=Model.addPart(E.project,2,1,rings,vertices,indices,{kind='ring'})
    second.x=60; second.pivot.x=60
    local child=Model.duplicate(E.project,1,E.selected,true)
    Model.reparent(E.project,1,child.id,E.selected)
    child.x=40; child.pivot.x=40
    IO.export(E.project,tempRoot..'/export.spt')
    local imported=IO.import(tempRoot..'/export.spt')
    assert(#imported.clips==1 and #imported.clips[1].tracks[1].keys==2)
    assert(imported.frames[1].parts[1].id==E.selected)
    assert(#imported.frames==2 and #imported.frames[1].parts==2)
    assert(imported.frames[1].parts[2].parent==E.selected)
    assert(imported.frames[1].parts[2].pivot.x==40)
    assert(imported.frames[2].parts[1].pivot.x==60)
    IO.save(E.project,tempRoot..'/project.asprite')
    local restored=IO.load(tempRoot..'/project.asprite')
    assert(restored.images[1].path:find('.assets/'))
    assert(restored.images[1].path~=restored.images[2].path)
    assert(restored.images[1].path:match('/fixture%.png$'))
    assert(restored.images[2].path:match('/fixture_2%.png$'))
    local a=assert(io.open(restored.images[1].path,'rb')); a:close()
    local b=assert(io.open(restored.images[2].path,'rb')); b:close()
    local missing,reason=mbm.readPngAlpha(tempRoot..'/missing.png')
    assert(missing==nil and type(reason)=='string')
    assert(imported.frameAnimations and #imported.frameAnimations>=1)
    local independent=sprite:new('2dw',100,0)
    assert(independent:load(tempRoot..'/export.spt'))
    assert(independent:playArticulatedAnimation('walk'))
    assert(independent:seekArticulatedAnimation('walk',0.5))
    independent:pauseArticulatedAnimation('walk')
    assert(math.abs(independent:getArticulatedAnimationTime('walk')-0.5)<0.001)
    E.mode='animate'; E.project.options.onion=true; E.time=0.5
    editor.rebuild()
    for i=1,5 do editor.rebuild() end
    local preview=sprite:new('2dw')
    assert(preview:loadEditorPreview(tempRoot..'/export.spt'))
    assert(preview:loadEditorPreview(nil))
    preview:destroy()
    print('ARTICULATED SPRITE ROUNDTRIP OK')
end
local zoomCase=1
local expectedZoom
local zoomCases={
    {delta=1,hovered=false},
    {delta=1,hovered=false},
    {delta=-1,hovered=false},
    {delta=-1,hovered=false},
    {delta=0,hovered=false},
    {delta=100,hovered=false},
    {delta=-1000,hovered=false},
    {delta=1,hovered=true},
}
local checkboxTested=false
local function testCheckboxTransitions()
    local E=editor.state
    local original=tImGui.Checkbox
    local ok,err=pcall(function()
        for _,entry in ipairs({
            {'show_source',E.project.options,'showSource'},
            {'copy_images',E.project.options,'copyImages'},
            {'move_windows',E,'moveWindows'},
            {'auto_key',E.project.options,'autoKey'},
            {'onion',E.project.options,'onion'},
            {'preserve_holes',E.project.options,'preserveHoles'},
            {'split_regions',E.project.options,'splitRegions'},
        }) do
            local key,object,name=table.unpack(entry)
            local initial=not not object[name]
            for _,target in ipairs({false,true,true,false,false,initial}) do
                local previous=not not object[name]
                -- Simulate a click result using the binding's single-return contract.
                tImGui.Checkbox=function(_,value)
                    assert(type(value)=='boolean')
                    return target
                end
                local changed=editor.check(key,object,name)
                assert(object[name]==target,'checkbox did not retain its value: '..key)
                assert(changed==(previous~=target),'incorrect change detection: '..key)
            end
        end
    end)
    tImGui.Checkbox=original
    assert(ok,err)
end
function onLoop(delta)
    if not checkboxTested then
        testCheckboxTransitions()
        checkboxTested=true
    end
    if expectedZoom then
        assert(math.abs(editor.state.camera.sx-expectedZoom)<0.0001,'zoom reset between engine frames')
    end
    local case=zoomCases[zoomCase]
    if case then
        -- Keep the real camera binding and scroll callback; control only whether
        -- ImGui captures the mouse so the regression is independent of its position.
        local camera=editor.state.camera
        local before=camera.sx
        local hovered=tImGui.IsAnyWindowHovered
        local active,capture=tImGui.IsAnyItemActive,tImGui.GetWantCaptureMouse
        tImGui.IsAnyWindowHovered=function() return case.hovered end
        tImGui.IsAnyItemActive=function() return false end
        tImGui.GetWantCaptureMouse=function() return false end
        onTouchZoom(case.delta)
        local ok,err=pcall(editor.worldInput)
        tImGui.IsAnyWindowHovered=hovered
        tImGui.IsAnyItemActive,tImGui.GetWantCaptureMouse=active,capture
        assert(ok,err)
        local expected=case.hovered and before or math.max(0.1,math.min(10,before*1.1^case.delta))
        assert(math.abs(camera.sx-expected)<0.0001,'unexpected camera zoom X')
        assert(math.abs(camera.sy-expected)<0.0001,'unexpected camera zoom Y')
        assert(editor.state.zoomRequest==nil,'scroll request was not consumed')
        expectedZoom=expected
        zoomCase=zoomCase+1
        if zoomCase>#zoomCases then
            local width,height=mbm.getRealSizeScreen()
            camera:scaleToScreen(width,height,'xy')
            expectedZoom=1
            print('ARTICULATED SPRITE ZOOM OK')
        end
    end
    local preview=editor.state.preview
    local language=mbm.getTimeRun()-start<2 and 'pt_br' or 'en'
    if tLang.current~=language then
        tLang.setLanguage(language); editor.refreshTitles()
    end
    assert(editor.state.titles.canvas==tLang.L('ase_canvas_title')..'###ase_canvas')
    loop(delta)
    -- Real ImGui controls must leave idle values and preview resources unchanged.
    local E=editor.state
    assert(E.project.options.showSource==true and E.project.options.copyImages==true)
    assert(E.project.options.onion==true and E.project.options.autoKey==false)
    assert(E.project.options.preserveHoles==true and E.project.options.splitRegions==true)
    assert(E.project.clips[1].loop==true and E.project.clips[1].blend==1)
    assert(E.preview==preview,'idle checkboxes triggered a preview rebuild')
    if mbm.getTimeRun()-start>4 then
        print('ARTICULATED SPRITE CHECKBOXES OK')
        print('ARTICULATED SPRITE UI OK')
        mbm.quit()
    end
end

local finalize=onEndScene
function onEndScene()
    finalize()
    if tempRoot then
        for _,path in ipairs({'fixture.png','export.spt','project.asprite','project.asprite.assets/fixture.png','project.asprite.assets/fixture_2.png','other/fixture.png'}) do
            os.remove(tempRoot..'/'..path)
        end
        os.remove(tempRoot..'/project.asprite.assets'); os.remove(tempRoot..'/other'); os.remove(tempRoot)
    end
end
