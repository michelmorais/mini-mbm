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
local P=require 'articulated_sprite_pose'
local M=require 'articulated_sprite_model'
local function near(a,b) assert(math.abs(a-b)<0.001,tostring(a)..' ~= '..tostring(b)) end
local clip={tracks={{part=1,keys={{time=0,angle=0,sx=1},{time=1,angle=100,sx=2}}}}}
local E={project={clips={clip}},frame=1,clip=1,selected=1,time=0.35,mode='animate'}
P.sync(E); near(E.pose.angle,35); near(E.pose.sx,1.35)
E.pose.angle=70; P.stage(E)
E.dirty=false; E.selected=2; P.sync(E)
near(E.pose.angle,0); near(E.pose.sx,1); assert(not E.dirty)
near(E.drafts[1].angle,70); assert(not E.drafts[2])
E.pose.angle=20; P.stage(E)
E.selected=1; P.sync(E); near(E.pose.angle,70)
M.key(E.project,1,1,E.time,E.pose); P.recorded(E)
assert(E.transient and not E.drafts[1] and E.drafts[2])
E.geometryRevision=1; P.sync(E); near(E.pose.angle,70)
E.selected=2; P.sync(E); near(E.pose.angle,20)
E.time=0.5; P.sync(E); near(E.pose.angle,0); assert(not E.transient)
E.selected=1; P.sync(E); near(E.pose.angle,70+30*(0.5-0.35)/0.65)
E.keyOrigin=0.35; E.selected=2; P.sync(E); assert(not E.keyOrigin)
local pose=E.pose; P.sync(E); assert(E.pose==pose,'idle sync allocated a new pose')
local key={time=0,angle=0,easing=1}
clip.tracks={{part=1,keys={key,{time=1,angle=100}}}}
near(P.sample(clip,1,0.5).angle,25)
key.easing=2; near(P.sample(clip,1,0.5).angle,75)
key.easing=3; near(P.sample(clip,1,0.5).angle,50)
key.easing=4; near(P.sample(clip,1,0.5).angle,50)
key.easing=5; key.bezier={0.25,0.25,0.75,0.75}; near(P.sample(clip,1,0.35).angle,35)
clip.tracks[1].keys={{time=0,angle=0},{time=1,angle=720}}
near(P.sample(clip,1,0.5).angle,360)
clip.tracks[1].keys={{time=0,q={0,0,0,1}},{time=1,q={0,0,1,0}}}
near(P.sample(clip,1,0.5).angle,90)
print('TIMELINE POSE OWNERSHIP / SAMPLING / IDLE OK')
