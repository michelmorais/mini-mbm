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
local P=require 'articulated_mesh_pose'
local function near(a,b) assert(math.abs(a-b)<0.00001,tostring(a)..' ~= '..tostring(b)) end
local q=P.quaternion({90,0,0}); near(q[1],-math.sqrt(0.5)); near(q[4],math.sqrt(0.5))
for _,angles in ipairs({{25,40,35},{-45,30,-60},{0,90,0}}) do
    local back=P.euler(P.quaternion(angles)); for i=1,3 do near(back[i],angles[i]) end
end
local clip={tracks={{part=1,mask=7,keys={
    {time=0,x=0,y=0,z=0,euler={0,0,0},sx=1,sy=1,sz=1,easing=0},
    {time=2,x=4,y=6,z=8,euler={90,180,720},sx=2,sy=3,sz=4,easing=0}}}}}
local pose=P.sample(clip,1,1); near(pose.z,4); near(pose.sz,2.5); near(pose.euler[3],360); near(pose.euler[1],45)
clip.tracks[1].mask=1
pose=P.sample(clip,1,1); near(pose.x,2); near(pose.q[4],1); near(pose.sx,1)
local E={project={clips={clip},frames={{parts={{id=1,parent=0,pivot={0,0,0},q={0,0,0,1}},
    {id=2,parent=1,pivot={1,0,0},q={0,0,0,1}}}}}},geometryRevision=1,frame=1,clip=1,time=1,mode='animate',selected=1}
local transform=P.transforms(E); local x,y,z=transform(2,1,0,0); near(x,3); near(y,3); near(z,4)
P.sync(E); local current=E.pose; P.sync(E); assert(current==E.pose,'idle resampled')
E.pose.z=42; P.stage(E); E.selected=2; P.sync(E); near(E.pose.z,0); E.selected=1; P.sync(E); near(E.pose.z,42)
print('ARTICULATED MESH QUATERNION / XYZ / FULL TURNS / MASK / HIERARCHY / DRAFTS / IDLE OK')
