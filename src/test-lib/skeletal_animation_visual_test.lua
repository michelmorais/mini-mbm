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

-- Pure regression checks for editor/skeletal_animation_visual.lua.
local source=debug.getinfo(1,'S').source
local testDirectory=source:sub(1,1)=='@' and source:sub(2):match('^(.*[/\\])') or ''
package.path=testDirectory..'../../editor/?.lua;'..package.path

local visual=require 'skeletal_animation_visual'

assert(math.abs(visual.getJointDisplayRadius(0,1)-0.006)<1e-9)
assert(math.abs(visual.getJointDisplayRadius(0.02,1)-0.02)<1e-9)
assert(math.abs(visual.getJointDisplayRadius(4.240535736,1)-0.03)<1e-9)
assert(math.abs(visual.getJointDisplayRadius(0.0001,100)-0.6)<1e-9)

local head={x=0,y=0,z=0}
local ordinary=visual.getTailDisplayPoint(head,{x=0.2,y=0,z=0},1)
assert(math.abs(ordinary.x-0.2)<1e-9)
local clamped=visual.getTailDisplayPoint(head,{x=20,y=0,z=0},1)
assert(math.abs(clamped.x-0.35)<1e-9)

print('skeletal_animation_visual_test: ok')
