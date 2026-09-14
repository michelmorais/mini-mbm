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
assert(loadfile('editor/articulated_sprite_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local G=require 'articulated_sprite_geometry'
local Model=require 'articulated_sprite_model'
local triangulations,copies=0,0
local triangulate,copy=G.triangulate,Model.copy
G.triangulate=function(...) triangulations=triangulations+1; return triangulate(...) end
Model.copy=function(...) copies=copies+1; return copy(...) end
local closeRequested=false
local begin=tImGui.Begin
tImGui.Begin=function(title,...)
    local opened,closed=begin(title,...)
    if closeRequested and title==api.state.titles.canvas then closed=true end
    return opened,closed
end
local imagePath,frame
function onInitScene()
    init()
    imagePath=os.tmpname(); os.remove(imagePath); imagePath=imagePath..'.png'
    local pixels={}
    for i=1,16*16*4 do pixels[i]=255 end
    mbm.createTexture(pixels,16,16,4,'ase_contour_regression',imagePath)
    api.addImage(imagePath)
    api.generate(false)
    api.rebuild()
    frame=0
end
function onLoop(delta)
    frame=frame+1
    local E=api.state
    local part=E.project.frames[1].parts[1]
    local v=part.rings[1][1]
    E.editContour=true
    if frame==1 then
        triangulations,copies=0,0
        local preview=E.preview
        for i=1,100 do
            api.startContourDrag(part,1,1,v.x,v.y)
            api.moveContourPoint(v.x+0.25,v.y+0.25,1)
            api.finishContourDrag(false)
        end
        assert(triangulations==0 and copies==0,'point selection performed geometry work or copied project')
        assert(not E.dirty and E.preview==preview,'point selection invalidated preview')
        local x,y=v.x,v.y
        api.startContourDrag(part,1,1,x,y)
        for i=1,100 do api.moveContourPoint(x+i*0.03,y,1) end
        assert(triangulations==0 and copies==0,'held drag repeatedly rebuilt geometry')
        api.finishContourDrag(false)
        assert(triangulations==1 and E.dirty,'real edit did not triangulate exactly once')
        loop(delta)
    elseif frame==2 then
        local x,y=v.x,v.y
        local preview=E.preview
        local count=triangulations
        api.startContourDrag(part,1,1,x,y)
        api.moveContourPoint(x+4,y,1)
        closeRequested=true
        loop(delta)
        assert(not E.project.options.showSource and not E.editContour,'closing canvas left edit checkbox enabled')
        assert(not E.dragPoint and not E.contourDrag,'closing canvas left an active drag')
        assert(v.x==x and v.y==y,'closing canvas did not cancel uncommitted movement')
        assert(triangulations==count and E.preview==preview,'closing canvas rebuilt geometry')
    else
        E.project.options.showSource=false
        loop(delta)
        assert(not E.editContour,'hiding source in Options left edit mode enabled')
        print('ARTICULATED SPRITE CONTOUR: 100 SELECTIONS WITHOUT COPY/REBUILD; ONE TRIANGULATION PER EDIT; CLOSE OK')
        mbm.quit()
    end
end
local finalize=onEndScene
function onEndScene()
    finalize()
    if imagePath then os.remove(imagePath) end
end
