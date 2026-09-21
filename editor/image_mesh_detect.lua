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

-- Connected foreground only; internal cavities are not silently turned into holes.
local M={}
function M.trace(bytes,width,height,crop,sx,sy,options,tick)
 local step=math.max(1,math.ceil(math.sqrt(crop.w*crop.h/262144)))
 while math.ceil(crop.w/step)*math.ceil(crop.h/step)>262144 do step=step+1 end
 local w,h=math.ceil(crop.w/step),math.ceil(crop.h/step)
 local function pixel(x,y)
  local px=math.min(crop.x+crop.w-1,crop.x+x*step+math.floor(step/2))
  local py=math.min(crop.y+crop.h-1,crop.y+y*step+math.floor(step/2))
  return bytes:byte((py*width+px)*4+1,(py*width+px)*4+4)
 end
 local function foreground(x,y)
  local r,g,b,a=pixel(x,y)
  if a<=options.alpha then return false end
  if options.mode==1 then return true end
  local c=options.color
  return math.max(math.abs(r-c.r*255),math.abs(g-c.g*255),math.abs(b-c.b*255))>options.tolerance
 end
 local x,y=math.floor((sx-crop.x)/step),math.floor((sy-crop.y)/step)
 assert(x>=0 and y>=0 and x<w and y<h and foreground(x,y),'ime_auto_background_seed')
 local first=y*w+x;local queue={first};local seen={[first]=true};local head=1
 local function visit(x,y)
  if x<0 or y<0 or x>=w or y>=h then return end
  local id=y*w+x
  if seen[id]~=nil then return end
  seen[id]=foreground(x,y)
  if seen[id] then queue[#queue+1]=id end
 end
 while head<=#queue do
  local id=queue[head];head=head+1;local x,y=id%w,math.floor(id/w)
  visit(x-1,y);visit(x+1,y);visit(x,y-1);visit(x,y+1)
  if head%2048==0 then tick() end
 end
 local edges={};local count=0;local stride=w+1
 local function add(x,y,xx,yy)
  local a,b=y*stride+x,yy*stride+xx
  assert(not edges[a],'ime_auto_touch')
  edges[a]=b;count=count+1;assert(count<=65536,'ime_auto_complex')
 end
 for i,id in ipairs(queue) do
  local x,y=id%w,math.floor(id/w)
  if y==0 or not seen[id-w] then add(x,y,x+1,y) end
  if x==w-1 or not seen[id+1] then add(x+1,y,x+1,y+1) end
  if y==h-1 or not seen[id+w] then add(x+1,y+1,x,y+1) end
  if x==0 or not seen[id-1] then add(x,y+1,x,y) end
  if i%2048==0 then tick() end
 end
 local best,bestArea=nil,0
 local starts={};for id in pairs(edges) do starts[#starts+1]=id end
 local visitedEdges=0
 for _,start in ipairs(starts) do if edges[start] then
  local id=start;local ring={};local area=0
  repeat
   local nextId=assert(edges[id],'ime_auto_touch');edges[id]=nil
   local x,y=id%stride,math.floor(id/stride);local xx,yy=nextId%stride,math.floor(nextId/stride)
   ring[#ring+1]={x=x,y=y};area=area+x*yy-y*xx;id=nextId
   visitedEdges=visitedEdges+1;if visitedEdges%2048==0 then tick() end
  until id==start
  if area>bestArea then best,bestArea=ring,area end
 end end
 assert(best and #best>=3,'ime_auto_empty')
 local points={}
 for i,p in ipairs(best) do
  local a,b=best[(i+#best-2)%#best+1],best[i%#best+1]
  if (p.x-a.x)*(b.y-p.y)~=(p.y-a.y)*(b.x-p.x) then
   points[#points+1]={x=math.min(crop.x+crop.w-1,math.max(crop.x,crop.x+p.x*step-.5)),
                    y=math.min(crop.y+crop.h-1,math.max(crop.y,crop.y+p.y*step-.5))}
  end
  if i%2048==0 then tick() end
 end
 assert(#points<=4096,'ime_auto_complex')
 return points,step,#queue
end
return M
