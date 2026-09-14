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
    E.project.clips={{name='walk',duration=1,speed=1,loop=true,tracks={}}}
    Model.key(E.project,1,E.selected,0,{angle=0})
    Model.key(E.project,1,E.selected,1,{angle=45})
    local IO=require 'articulated_sprite_io'
    local G=require 'articulated_sprite_geometry'
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
function onLoop(delta)
    loop(delta)
    if mbm.getTimeRun()-start>4 then print('ARTICULATED SPRITE UI OK'); mbm.quit() end
end

local finalize=onEndScene
function onEndScene()
    finalize()
    if tempRoot then
        for _,path in ipairs({'fixture.png','export.spt','project.asprite','project.asprite.assets/image_1.png','project.asprite.assets/image_2.png','other/fixture.png'}) do
            os.remove(tempRoot..'/'..path)
        end
        os.remove(tempRoot..'/project.asprite.assets'); os.remove(tempRoot..'/other'); os.remove(tempRoot)
    end
end
