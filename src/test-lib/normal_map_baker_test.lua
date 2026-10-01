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
local Baker=require 'normal_map_baker'
local Height=require 'height_map_source'
local function unit(x,y,z) local l=math.sqrt(x*x+y*y+z*z);return x/l,y/l,z/l end
local n={nx=.6,ny=0,nz=.8}
local t={x=-.8,y=0,z=.6,sign=1}
local x,y,z=Baker.project(n,n,n,t,t,t,.2,.3,0,0,1,1,1)
assert(math.abs(x-.6)<1e-7 and math.abs(y)<1e-7 and math.abs(z-.8)<1e-7)
-- Decode with the renderer basis: correct a smooth mesh normal to a flat target.
assert(math.abs(-.8*x+.6*z)<1e-7 and math.abs(.6*x+.8*z-1)<1e-7)
x,y,z=Baker.project(n,n,n,t,t,t,.2,.3,.6,0,.8,1,1)
assert(math.abs(x)<1e-7 and math.abs(y)<1e-7 and math.abs(z-1)<1e-7)
x,y,z=Baker.project(n,n,n,t,t,t,.2,.3,0,0,1,0,1)
assert(math.abs(x)<1e-7 and math.abs(y)<1e-7 and z==1)
-- Interpolated normals/tangents must be re-orthogonalized, as in the shader.
local a,b,c={nx=0,ny=0,nz=1},{nx=.6,ny=0,nz=.8},{nx=-.6,ny=0,nz=.8}
local ta,tb,tc={x=-1,y=0,z=0,sign=1},t,{x=-.8,y=0,z=-.6,sign=1}
local dx,dy,dz=unit(.3,.2,1)
x,y,z=Baker.project(a,b,c,ta,tb,tc,.4,.1,dx,dy,dz,1,1)
local nx,ny,nz=unit(.18,0,.9)
local tx,ty,tz=unit(-nz,0,nx)
local rx,ry,rz=tx*x+nx*z,-y,tz*x+nz*z
assert(math.abs(rx-dx)<1e-7 and math.abs(ry-dy)<1e-7 and math.abs(rz-dz)<1e-7)
local _,flipped=Baker.project(a,b,c,ta,tb,tc,.4,.1,dx,dy,dz,1,-1)
assert(math.abs(flipped+y)<1e-7)
ta.sign=-1;tb.sign=-1;tc.sign=-1
local _,mirrored=Baker.project(a,b,c,ta,tb,tc,.4,.1,dx,dy,dz,1,1)
assert(math.abs(mirrored+y)<1e-7)
ta.sign=0;tb.sign=0;tc.sign=0
x,y,z=Baker.project(a,b,c,ta,tb,tc,.4,.1,dx,dy,dz,1,1)
assert(x==0 and y==0 and z==1)
-- Rasterized UV crop, including boundary texels, with a physically matching ramp.
local pixels={}
for j=1,9 do for i=1,9 do local v=math.floor((i-1)*255/8+.5);pixels[#pixels+1]=string.char(v,v,v,255) end end
local image=Height.image(table.concat(pixels),9,9)
local vertices={}
local sx,sy,sz=unit(1,0,1)
for _,uv in ipairs{{2.5/13,1.5/11},{10.5/13,1.5/11},{2.5/13,9.5/11},{10.5/13,9.5/11}} do
 vertices[#vertices+1]={u=uv[1],v=uv[2],z=0,nx=sx,ny=sy,nz=sz}
end
local tangent={x=-sz,y=0,z=sx,sign=1}
local corners={tangent,tangent,tangent,tangent,tangent,tangent}
local domain={imageWidth=13,imageHeight=11,x=2,y=1,width=8,height=8,scale=8}
local result=Baker.generate(image,vertices,{1,2,3,2,4,3},corners,domain,{strength=1})
for i=1,#result.bytes,4 do
 local r,g,b,alpha=result.bytes:byte(i,i+3)
 assert(math.abs(r-128)<=1 and math.abs(g-128)<=1 and b==255 and alpha==255,'Matching ramp is not neutral')
end
local job=require('normal_map_generator').job(function(tick)
 return Baker.generate(image,vertices,{1,2,3,2,4,3},corners,domain,{strength=1},tick)
end)
job:step(0);assert(job.state=='running');job:cancel();assert(job.state=='cancelled')
print('NORMAL MAP BAKER TESTS PASS')
