--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

package.path = 'editor/?.lua;' .. package.path
local Transform = require 'mesh_debug_transform'
local function fixture(skeletal)
    local calls = {}
    local mesh = { getSkeletonBindReport = function(_, dependencies)
        assert(dependencies == false)
        if skeletal then return {canonical=true, boneCount=2} end
        return nil
    end }
    for _, name in ipairs({'rotateFrame','translateFrame','scaleFrame','scaleSkeletalAsset','centralize','centralizeItself'}) do
        mesh[name] = function(_, ...) calls[#calls+1] = {name, ...} end
    end
    return mesh, calls
end
local function values(overrides)
    local xf = {frame=0,subset=0,rx=0,ry=0,rz=0,sx=1,sy=1,sz=1,dx=0,dy=0,dz=0}
    for k,v in pairs(overrides or {}) do xf[k]=v end
    return xf
end
local function blocked(operation, overrides, expected)
    local mesh,calls=fixture(true)
    local ok,err=pcall(Transform.apply,mesh,operation,values(overrides))
    assert(not ok and err==expected,tostring(err))
    assert(#calls==0,'rejection must precede every mutation')
end
for _, operation in ipairs({'rotate','translate','centralize','centralizeItself'}) do
    blocked(operation,{},'mesh_debug_skeletal_transform_blocked')
end
blocked('centralize',{subset=2},'mesh_debug_skeletal_transform_blocked')
blocked('combined',{sx=2,sy=2,sz=2,dx=4},'mesh_debug_skeletal_transform_blocked')
blocked('combined',{sx=2,sy=2,sz=2,rx=90},'mesh_debug_skeletal_transform_blocked')
for _, scale in ipairs({{2,1,2},{-1,-1,-1},{0,0,0},{math.huge,math.huge,math.huge},{0/0,1,1}}) do
    blocked('scale',{sx=scale[1],sy=scale[2],sz=scale[3]},'bones_uniform_positive_scale_required')
end
for _, operation in ipairs({'scale','combined'}) do
    local mesh,calls=fixture(true)
    Transform.apply(mesh,operation,values({sx=2,sy=2,sz=2}))
    assert(#calls==1 and calls[1][1]=='scaleSkeletalAsset' and calls[1][2]==2)
end
for _, skeletal in ipairs({false,true}) do
    local mesh,calls=fixture(skeletal)
    Transform.apply(mesh,'combined',values({frame=1,subset=2,rx=90,sx=2,sy=3,sz=4,dx=5}))
    assert(#calls==3 and calls[1][1]=='rotateFrame' and calls[2][1]=='scaleFrame' and calls[3][1]=='translateFrame')
end
local mesh,calls=fixture(false)
Transform.apply(mesh,'combined',values({rx=90,sx=2,sy=3,sz=4,dx=5}))
assert(#calls==3 and calls[2][1]=='scaleFrame')
print('Mesh Debug transform policy OK')
