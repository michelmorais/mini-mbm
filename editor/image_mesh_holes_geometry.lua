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
local epsilon=1e-10
function M.bounds(ring)
 local left,top,right,bottom=math.huge,math.huge,-math.huge,-math.huge
 for _,p in ipairs(ring) do left=math.min(left,p.x);top=math.min(top,p.y);right=math.max(right,p.x);bottom=math.max(bottom,p.y) end
 return {x=(left+right)/2,y=(top+bottom)/2,rx=(right-left)/2,ry=(bottom-top)/2}
end
-- Recognize the axis-aligned 16-point primitive from projects predating metadata.
function M.isEllipse(ring)
 if #ring~=16 then return false end
 local b=M.bounds(ring)
 if b.rx<=0 or b.ry<=0 then return false end
 for i,p in ipairs(ring) do
  local a=(i-1)*math.pi/8
  if math.abs((p.x-b.x)/b.rx-math.cos(a))>1e-6 or math.abs((p.y-b.y)/b.ry-math.sin(a))>1e-6 then return false end
 end
 return true
end
local function cross(a,b,c) return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x) end
local function on(a,b,p)
 return math.abs(cross(a,b,p))<=epsilon and p.x>=math.min(a.x,b.x)-epsilon and p.x<=math.max(a.x,b.x)+epsilon and p.y>=math.min(a.y,b.y)-epsilon and p.y<=math.max(a.y,b.y)+epsilon
end
local function hit(a,b,c,d)
 return (cross(a,b,c)*cross(a,b,d)<0 and cross(c,d,a)*cross(c,d,b)<0) or on(a,b,c) or on(a,b,d) or on(c,d,a) or on(c,d,b)
end
function M.simple(ring)
 local area=0
 for i,a in ipairs(ring) do
  local b,c=ring[i%#ring+1],ring[(i+1)%#ring+1]
  if (a.x-b.x)^2+(a.y-b.y)^2<1e-12 then return false end
  if math.abs(cross(a,b,c))<=epsilon and not on(a,c,b) then return false end
  for j=i+1,#ring do
   if j~=i%#ring+1 and j%#ring+1~=i and hit(a,b,ring[j],ring[j%#ring+1]) then return false end
  end
  area=area+a.x*b.y-a.y*b.x
 end
 return #ring>=3 and math.abs(area)>1e-8
end
function M.inside(r,p)
 local inside=false
 for i,a in ipairs(r) do local b=r[i%#r+1]
  if on(a,b,p) then return false end
  if (a.y>p.y)~=(b.y>p.y) and p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x then inside=not inside end
 end
 return inside
end
-- Placement of a simple primitive; existing rings have already been validated.
function M.canPlace(outer,holes,ring)
 for i,a in ipairs(ring) do
  local b=ring[i%#ring+1]
  if not M.inside(outer,a) then return false end
  for j,p in ipairs(outer) do if hit(a,b,p,outer[j%#outer+1]) then return false end end
  for _,other in ipairs(holes) do
   if M.inside(other,a) or M.inside(ring,other[1]) then return false end
   for j,p in ipairs(other) do if hit(a,b,p,other[j%#other+1]) then return false end end
  end
 end
 return true
end
function M.validate(outer,holes)
 if holes==nil then return true end
 assert(type(holes)=='table' and #holes<=16,'ime_holes_limit')
 for h,ring in ipairs(holes) do
  assert(type(ring)=='table' and #ring>=3 and #ring<=128,'ime_holes_points')
  assert(ring.primitive==nil or ring.primitive=='ellipse','ime_holes_points')
  assert(ring.preserveShape==nil or type(ring.preserveShape)=='boolean','ime_holes_points')
  for _,p in ipairs(ring) do assert(type(p)=='table' and type(p.x)=='number' and type(p.y)=='number' and p.x>=0 and p.x<=1 and p.y>=0 and p.y<=1,'ime_holes_points') end
  local area=0
  for i,a in ipairs(ring) do
   local b,c=ring[i%#ring+1],ring[(i+1)%#ring+1]
   assert((a.x-b.x)^2+(a.y-b.y)^2>=1e-12,'ime_holes_points')
   assert(math.abs(cross(a,b,c))>epsilon or on(a,c,b),'ime_holes_cross')
   assert(M.inside(outer,a),'ime_holes_inside')
   for j=i+1,#ring do
    if j~=i%#ring+1 and j%#ring+1~=i then assert(not hit(a,b,ring[j],ring[j%#ring+1]),'ime_holes_cross') end
   end
   for j,p in ipairs(outer) do assert(not hit(a,b,p,outer[j%#outer+1]),'ime_holes_inside') end
   for k=1,h-1 do
    local other=holes[k]
    assert(not M.inside(other,a) and not M.inside(ring,other[1]),'ime_holes_overlap')
    for j,p in ipairs(other) do assert(not hit(a,b,p,other[j%#other+1]),'ime_holes_overlap') end
   end
   area=area+a.x*b.y-a.y*b.x
  end
  assert(math.abs(area)>1e-8,'ime_holes_points')
 end
 return true
end
return M
