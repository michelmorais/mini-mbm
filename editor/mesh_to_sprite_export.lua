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

local P=require 'mesh_to_sprite_project'
local M={}
function M.write(p,images,path)
    local d=meshDebug:new(); d:setStride(2); d:enableNormal(false)
    local l=-p.width*p.pivotX; local r=l+p.width
    local bottom=-p.height*(1-p.pivotY); local top=bottom+p.height
    for i,file in ipairs(images) do
        local f=d:addFrame(2); local s=d:addSubSet(f)
        assert(d:addVertex(f,s,{{x=l,y=bottom,u=0,v=1},{x=r,y=bottom,u=1,v=1},{x=r,y=top,u=1,v=0},{x=l,y=top,u=0,v=0}}))
        assert(d:addIndex(f,s,{1,2,3,1,3,4}))
        assert(d:setTexture(f,s,P.basename(file)))
    end
    d:setType('sprite'); d:setModeDraw('TRIANGLES'); d:setModeCullFace('BACK'); d:setModeFrontFace('CCW')
    assert(d:addAnim(p.name,1,#images,p.frameTime,p.cycle and mbm.GROWING_LOOP or mbm.GROWING))
    assert(d:save(path,false,false),'Cannot save sprite')
end
return M
