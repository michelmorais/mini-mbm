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

tImGui=require 'ImGui'
local batch,frame
function onInitScene() frame=0 end
function onLoop(delta)
    frame=frame+1
    tImGui.SetNextWindowPos({x=10,y=10})
    tImGui.SetNextWindowSize({x=620,y=460})
    local opened=tImGui.Begin('Large cached geometry',false,0)
    if opened then
        if not batch then
            batch=tImGui.CreateGeometryBatch(function()
                -- 4096 anti-aliased circles produce over 196,000 vertices,
                -- spanning several 16-bit draw-command ranges.
                for y=0,63 do
                    for x=0,63 do
                        local color=y<32 and {r=0,g=1,b=0,a=1} or {r=1,g=0,b=1,a=1}
                        tImGui.AddCircleFilled({x=30+x*8,y=45+y*6},2,color,24)
                    end
                end
            end)
        end
        tImGui.AddGeometryBatch(batch)
        -- Ordinary primitives after the batch must use the new vertex base.
        tImGui.AddRectFilled({x=550,y=60},{x=590,y=100},{r=1,g=1,b=0,a=1})
    end
    tImGui.End()
    tImGui.SetNextWindowPos({x=640,y=10})
    tImGui.SetNextWindowSize({x=240,y=160})
    opened=tImGui.Begin('Next draw list',false,0)
    if opened then
        -- A new list starts at offset zero; the backend must reset its base.
        tImGui.Text('Large batch rendered')
        tImGui.AddRectFilled({x=660,y=65},{x=710,y=115},{r=0,g=1,b=1,a=1})
    end
    tImGui.End()
    if frame>=180 then
        print('IMGUI LARGE GEOMETRY BATCH AND VERTEX OFFSET OK')
        batch=nil; collectgarbage('collect')
        mbm.quit()
    end
end
