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
dofile('editor/mesh_debug.lua')
local init=onInitScene
local Wire=require 'mesh_debug_capture_wireframe'
local boxes,entry,start={},{}
local function tests()
    init()
    local camera=mbm.getCamera('3d')
    camera:setPos(0,0,-550); camera:setFocus(0,0,0)
    for i,kind in ipairs({'cube','sphere','cylinder'}) do
        local box={volumeType=kind,x=(i-2)*160,y=0,z=0,width=120,height=120,depth=120}
        splitCaptureBuildBox(box)
        assert(box.tShape and box.tLine)
        boxes[#boxes+1]=box
        local shapeBefore = box.tShape
        local edgeBefore = box.tAxisEdgeLines.x
        for _,kindHover in ipairs({'center','size'}) do
            for _,axis in ipairs({'x','y','z'}) do
                splitCaptureSetHover(box,kindHover,axis)
                for _,other in ipairs({'x','y','z'}) do
                    assert(box.tAxisEdgeLines[other].visible == (kindHover=='center' and other==axis))
                    assert(box.tAxisFaceShapes[other].visible == (kindHover=='size' and other==axis))
                    assert(box.tAxisEdgeLines[other].alwaysOnTop and box.tAxisFaceShapes[other].alwaysOnTop)
                end
            end
        end
        splitCaptureSetHover(box,nil,nil)
        box.x=box.x+10
        splitCaptureMoveBox(box)
        assert(box.tAxisEdgeLines.x.x==box.x and box.tAxisFaceShapes.z.x==box.x)
        assert(box.aabbMax.x==box.x+60,'guides must not expand capture bounds')
        assert(box.tShape==shapeBefore and box.tAxisEdgeLines.x==edgeBefore,'hover/move must reuse geometry')
        box.x=box.x-10
        splitCaptureMoveBox(box)
        assert(splitCapturePointInside({x=box.x,y=0,z=0},box))
    end
    local d=meshDebug:new(); d:setType('mesh'); d:setModeDraw('TRIANGLES'); d:addFrame(3); d:addSubSet(1)
    assert(d:addVertex(1,1,{{x=-100,y=-100,z=0},{x=100,y=-100,z=0},{x=0,y=100,z=0}}))
    assert(d:addIndex(1,1,{1,2,3})); d:setTexture(1,1,'#FFFFFFFF')
    for _,kind in ipairs({'sphere','cylinder'}) do
        local box={volumeType=kind,aabbMin={x=-20,y=-20,z=-20},aabbMax={x=20,y=20,z=20}}
        local analysis=assert(splitCaptureAnalyze({},d,box))
        assert(analysis.resolved[2].faces==0 and analysis.resolved[3].faces==0)
        assert(analysis.resolved[4].faces==1)
        Wire.update(entry,analysis.resolved[4]); assert(entry.captureWire.wireObject.alwaysOnTop)
        local cached=entry.captureWire
        Wire.update(entry,analysis.resolved[4]); assert(entry.captureWire==cached,'idle rebuild')
        Wire.update(entry,analysis.resolved[2]); assert(entry.captureWire~=cached,'algorithm change')
        Wire.update(entry,analysis.resolved[4])
    end
    print('CAPTURE_RENDER_OK')
end
function onInitScene()
    start=mbm.getTimeRun()
    local ok,err=pcall(tests)
    if not ok then print('CAPTURE_RENDER_FAIL '..tostring(err)); mbm.quit() end
end
function onLoop()
    local elapsed=mbm.getTimeRun()-start
    local axis=({'x','y','z'})[math.floor(elapsed)%3+1]
    for _,box in ipairs(boxes) do
        splitCaptureSetHover(box,elapsed<3 and 'center' or 'size',axis)
    end
    if mbm.getTimeRun()-start>5 then
        destroySplitCaptureIslandMarkers(entry)
        assert(not entry.captureWire)
        for _,box in ipairs(boxes) do splitCaptureDestroy(box) end
        print('CAPTURE_CLEANUP_OK'); mbm.quit()
    end
end
