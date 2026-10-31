--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  |
| OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                                                          |
|------------------------------------------------------------------------------------------------------------------------|
]]--

package.path='editor/?.lua;'..package.path
local Mirror=require 'facial_rig_mirror'
local References=require 'facial_rig_references'
local function vec(x,y,z) return {x=x,y=y,z=z} end

assert(Mirror.pairedName('eyeBlinkLeft')=='eyeBlinkRight')
assert(Mirror.pairedName('browUp_R')=='browUp_L')
assert(Mirror.pairedName('jawOpen')=='jawOpen_mirror')
local vertices={vec(-1,0,0),vec(1,0,0),vec(0,0,0)}
local target={positionDeltas={vec(0.25,0.2,0),vec(0,0,0),vec(0,0,0)},
    normalDeltas={vec(0.1,0,0),vec(0,0,0),vec(0,0,0)}}
local result=assert(Mirror.build(target,vertices,0,0.001))
assert(result.matched==3 and result.totalActive==1 and
    result.unmatchedActive==0 and result.ambiguous==0)
assert(result.positions[2].x==-0.25 and result.positions[2].y==0.2)
assert(result.normals[2].x==-0.1 and result.positions[1].x==0)
local shifted=assert(Mirror.build(target,vertices,2,0.001))
assert(shifted.unmatchedActive==1)
local duplicateVertices={vec(-1,0,0),vec(-1,0,0),vec(1,0,0)}
local conflicting={positionDeltas={vec(1,0,0),vec(0,0,0),vec(0,0,0)},normalDeltas={}}
local ambiguous=assert(Mirror.build(conflicting,duplicateVertices,0,0.001))
assert(ambiguous.ambiguous>0)

local project={expressions={poses={{name='smile',weights={eyeBlinkLeft=0.8}}}},
    clips={{name='blink',keys={{weights={eyeBlinkLeft=0.2}},{weights={eyeBlinkLeft=0}}}}}}
local usage=References.inspect(project,'eyeBlinkLeft')
assert(#usage.poses==1 and #usage.clips==1 and usage.keyCount==2)
References.rename(project,'eyeBlinkLeft','eyeBlinkRight')
assert(project.expressions.poses[1].weights.eyeBlinkRight==0.8)
assert(project.clips[1].keys[2].weights.eyeBlinkRight==0)
assert(References.inspect(project,'eyeBlinkLeft').keyCount==0)
References.remove(project,'eyeBlinkRight')
assert(next(project.expressions.poses[1].weights)==nil)
assert(next(project.clips[1].keys[1].weights)==nil)
print('FACIAL RIG MIRROR OK')
