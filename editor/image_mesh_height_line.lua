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

local M={}
local function radius(r,a) return a.lineWidth*math.max(1,math.min(r.w,r.h)-1)*.5 end
function M.contains(r,a,x,y)
 local nearest=math.huge
 for i=1,#a-1 do
  local p,q=a[i],a[i+1]
  local ax,ay=r.x+p.x*(r.w-1),r.y+p.y*(r.h-1)
  local dx,dy=(q.x-p.x)*(r.w-1),(q.y-p.y)*(r.h-1)
  local length=dx*dx+dy*dy
  local t=length>0 and math.max(0,math.min(1,((x-ax)*dx+(y-ay)*dy)/length)) or 0
  nearest=math.min(nearest,(x-ax-t*dx)^2+(y-ay-t*dy)^2)
 end
 return nearest<=radius(r,a)^2
end
-- Only called when canvas geometry is dirty. Each segment has round caps;
-- overlaps display the union's construction lines without polygon triangulation.
function M.draw(r,a,draw,color)
 local rad=radius(r,a)
 for i=1,#a-1 do
  local p,q=a[i],a[i+1]
  local ax,ay=r.x+p.x*(r.w-1),r.y+p.y*(r.h-1)
  local bx,by=r.x+q.x*(r.w-1),r.y+q.y*(r.h-1)
  local angle=math.atan(by-ay,bx-ax);local points={}
  local function arc(x,y,start)
   for j=0,8 do
    local theta=start+j*math.pi/8
    points[#points+1]={x=math.max(r.x,math.min(r.x+r.w-1,x+rad*math.cos(theta))),
     y=math.max(r.y,math.min(r.y+r.h-1,y+rad*math.sin(theta)))}
   end
  end
  arc(ax,ay,angle+math.pi/2);arc(bx,by,angle-math.pi/2)
  draw(points,true,color)
 end
end
return M
