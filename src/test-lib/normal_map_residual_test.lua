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
local Residual=require 'normal_map_residual'
local Height=require 'height_map_source'
local Generator=require 'normal_map_generator'
local domain={imageWidth=13,imageHeight=11,x=2,y=1,width=8,height=6,scale=3}
local vertices={
 {u=2.5/13,v=1.5/11,z=10},{u=10.5/13,v=1.5/11,z=13},
 {u=2.5/13,v=7.5/11,z=10},{u=10.5/13,v=7.5/11,z=13}}
local indices={1,2,3,2,4,3}
local function source(fn)
 local pixels={}
 for y=1,7 do for x=1,9 do
  local value,alpha=fn(x,y);value=math.floor(value+.5)
  pixels[#pixels+1]=string.char(value,value,value,alpha or 255)
 end end
 return Height.image(table.concat(pixels),9,7)
end
local function pixel(result,x,y) return result.bytes:byte(((y-1)*9+x-1)*4+1,((y-1)*9+x-1)*4+4) end
local ramp=source(function(x) return (x-1)*255/8 end)
local settings={strength=1,blur=1,edge='clamp',convention='+Y'}
local result=Residual.generate(ramp,vertices,indices,domain,settings)
for y=1,7 do for x=1,9 do
 local r,g,b=pixel(result,x,y)
 assert(math.abs(r-128)<=1 and math.abs(g-128)<=1 and b==255,'Matching ramp must be neutral, even after blur')
end end
-- Coarse flat geometry leaves physical source slope in the residual.
for _,v in ipairs(vertices) do v.z=10 end
settings.blur=0
result=Residual.generate(ramp,vertices,indices,domain,settings)
local r,g,b=pixel(result,5,4)
local expected=math.floor(127.5-127.5*(3/8)/math.sqrt(1+(3/8)^2)+.5)
assert(math.abs(r-expected)<=1 and g==128 and b<255,'Residual lost physical scale or sign')
-- Negative residuals must survive (subtract a ramp from a flat source).
vertices[2].z=13;vertices[4].z=13
local flat=source(function() return 0 end)
local reverse=Residual.generate(flat,vertices,indices,domain,settings)
assert(pixel(reverse,5,4)>128,'Negative heights were clamped')
settings.strength=0
local zero=Residual.generate(ramp,vertices,indices,domain,settings)
assert(pixel(zero,5,4)==128)
settings.strength=1
-- Vertical gradient flips only the green convention.
for _,v in ipairs(vertices) do v.z=0 end
local vertical=source(function(x,y) return (y-1)*255/6 end)
local positive=Residual.generate(vertical,vertices,indices,domain,settings)
settings.convention='-Y'
local negative=Residual.generate(vertical,vertices,indices,domain,settings)
local pr,pg,pb=pixel(positive,5,4);local nr,ng,nb=pixel(negative,5,4)
assert(pr==nr and pb==nb and pg>128 and ng<128 and math.abs(pg+ng-255)<=1)
-- Uncovered UVs and transparent holes stay neutral, without sampling outside.
local cut=source(function(x,y) return 127,(x==4 and y==4) and 0 or 255 end)
local triangle=Residual.generate(cut,vertices,{1,2,3},domain,settings)
local _,_,_,a=pixel(triangle,9,7);assert(a==0)
local hr,hg,hb,ha=pixel(triangle,4,4);assert(hr==128 and hg==128 and hb==255 and ha==0)
local job=Generator.job(function(tick) return Residual.generate(ramp,vertices,indices,domain,settings,tick) end)
job:step(0);assert(job.state=='running');job:cancel();assert(job.state=='cancelled')
print('NORMAL MAP RESIDUAL TESTS PASS')
