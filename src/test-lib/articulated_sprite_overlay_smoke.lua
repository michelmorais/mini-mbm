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
local G=require 'articulated_sprite_geometry'
local Model=require 'articulated_sprite_model'
local elapsed,count,cpu,samples,batches=0,0,0,0,0
local create=tImGui.CreateGeometryBatch
tImGui.CreateGeometryBatch=function(...) batches=batches+1; return create(...) end
function onInitScene()
    init()
    local E=api.state
    E.project.images={{path='#FFFFFFFF',width=1024,height=1024}}
    local rings=G.form('circle',{x=0,y=0,w=900,h=900},642,{})
    local vertices,indices=G.triangulate(rings,640)
    local p=Model.addPart(E.project,1,1,rings,vertices,indices,{budget=640})
    E.selected=p.id; E.editContour=true; E.showParts=false; E.showTimeline=false
    E.dirty=true
    print('BENCH TRIANGLES',#indices/3,'POINTS',#rings[1])
end
function onLoop(delta)
    local start=os.clock()
    local previous=api.state.overlayCache
    if count==60 then api.state.point=2 end
    if count==120 then
        local part=api.state.project.frames[1].parts[1]
        local v=part.rings[1][2]
        api.startContourDrag(part,1,2,v.x,v.y)
        api.moveContourPoint(v.x+5,v.y,1)
    elseif count==121 then
        api.finishContourDrag(true)
    end
    loop(delta)
    if count==120 or count==121 then
        assert(api.state.overlayCache~=previous,'geometry change did not refresh overlay')
    elseif count>3 then
        assert(api.state.overlayCache==previous,'idle/point selection rebuilt overlay')
    end
    count=count+1; elapsed=elapsed+delta
    if count==1 then
        local ok=pcall(tImGui.CreateGeometryBatch,function() error('expected batch failure') end)
        assert(not ok,'batch swallowed callback error')
        ok=pcall(tImGui.CreateGeometryBatch,function()
            tImGui.CreateGeometryBatch(function() end)
        end)
        assert(not ok,'nested batch creation was accepted')
    end
    if count>30 then cpu=cpu+os.clock()-start; samples=samples+1 end
    if elapsed>4 then
        print('ARTICULATED SPRITE OVERLAY CACHE OK')
        print('BENCH LOOP MS',cpu/math.max(1,samples)*1000,'FRAMES',samples,'BATCHES',batches)
        mbm.quit()
    end
end
