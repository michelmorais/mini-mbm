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
|------------------------------------------------------------------------------------------------------------------------|
]]--

local M={}

function M.getJointDisplayRadius(authoredRadius,meshExtent)
    local extent=math.max(tonumber(meshExtent) or 1,0.000001)
    local minimum=math.max(extent*0.006,0.001)
    local maximum=math.max(extent*0.03,minimum)
    return math.min(math.max(tonumber(authoredRadius) or 0,minimum),maximum)
end

function M.getTailDisplayPoint(head,tailPoint,meshExtent)
    local dx=(tailPoint.x or 0)-(head.x or 0)
    local dy=(tailPoint.y or 0)-(head.y or 0)
    local dz=(tailPoint.z or 0)-(head.z or 0)
    local length=math.sqrt(dx*dx+dy*dy+dz*dz)
    local maximum=math.max((tonumber(meshExtent) or 1)*0.35,0.001)
    if length<=maximum or length<=0.000001 then return tailPoint end
    local scale=maximum/length
    return {x=head.x+dx*scale,y=head.y+dy*scale,z=head.z+dz*scale}
end

return M
