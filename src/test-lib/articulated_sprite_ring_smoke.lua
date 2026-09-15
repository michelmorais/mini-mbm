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
local api={}
assert(loadfile('editor/sprite_maker_articulated.lua'))(api)
local init,loop=onInitScene,onLoop
local Model=require 'articulated_sprite_model'
local IO=require 'articulated_sprite_io'
local G=require 'articulated_sprite_geometry'
local frame=0
function onInitScene()
    init()
    local E=api.state
    E.project.images={{path='#FFFFFFFF',width=512,height=512}}
    E.kind='ring'; E.rect={x=0,y=0,w=250,h=250}; E.budget=12
    E.project.options.showSource=true; E.editContour=true; E.showParts=false; E.showTimeline=false
    api.generate(false); api.rebuild()
end
function onLoop(delta)
    frame=frame+1
    local E=api.state
    if frame<=3 then
        local part=Model.part(E.project,E.frame,E.selected)
        local point=part.rings[1][1]
        api.startContourDrag(part,1,1,point.x,point.y)
        api.moveContourPoint(point.x-5,point.y,1)
        if frame~=2 then api.finishContourDrag(false) end
        -- Frame 2 also covers regeneration while an old contour drag is pending.
        E.budget=({64,128,512})[frame]
        local previous=E.preview
        local id=E.selected
        api.generate(true)
        assert(E.preview~=previous and E.selected==id,'ring regeneration did not publish new preview')
        local p=Model.part(E.project,E.frame,id)
        assert(#p.rings==2 and #p.indices>=E.budget*3,'ring topology lost')
        for i=1,#p.indices,3 do
            local a,b,c=p.vertices[p.indices[i]],p.vertices[p.indices[i+1]],p.vertices[p.indices[i+2]]
            assert(not G.contains(p.rings[2],(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3),'hole was filled')
        end
        assert(not E.dirty,'successful regeneration queued redundant rebuild')
        assert(E.editContour,'regeneration disabled contour editing')
        assert(not E.contourDrag and not E.dragPoint,'regeneration retained stale drag')
        assert(p.manual==false,'regenerated shape retained manual geometry flag')
    elseif frame==4 then
        local previous=E.preview
        local before=Model.encode(E.project)
        local export=IO.export
        IO.export=function() error('EXPECTED regeneration failure') end
        E.budget=96
        api.generate(true)
        IO.export=export
        assert(E.preview==previous,'failed regeneration destroyed old preview')
        assert(Model.encode(E.project)==before,'failed regeneration modified the project')
        assert(E.status:find('EXPECTED regeneration failure',1,true),'missing failure message')
        local file=assert(io.open(E.previewPath,'rb')); file:close()
    end
    loop(delta)
    if frame==5 then print('ARTICULATED SPRITE RING REGENERATION/ROLLBACK OK'); mbm.quit() end
end
