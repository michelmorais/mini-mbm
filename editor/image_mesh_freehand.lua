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

local Model=require 'image_mesh_model'
local Geometry=require 'image_mesh_holes_geometry'
local M={}
local function L(key) return tLang.L('ime_freehand_'..key) end
local function distance(p,a,b)
 local dx,dy=b.x-a.x,b.y-a.y
 local t=dx*dx+dy*dy
 if t>0 then t=math.max(0,math.min(1,((p.x-a.x)*dx+(p.y-a.y)*dy)/t)) end
 return (p.x-a.x-t*dx)^2+(p.y-a.y-t*dy)^2
end
-- Iterative Ramer-Douglas-Peucker on a closed stroke. Never silently discard
-- excess vertices: the user can increase tolerance or draw a shorter contour.
function M.simplify(points,tolerance)
 if #points<3 then return Model.copy(points) end
 local source=Model.copy(points);source[#source+1]=source[1]
 local keep={[1]=true,[#source]=true};local stack={{1,#source}}
 while #stack>0 do
  local span=table.remove(stack);local first,last=span[1],span[2]
  local far,index=tolerance*tolerance,nil
  for i=first+1,last-1 do local d=distance(source[i],source[first],source[last])
   if d>far then far=d;index=i end
  end
  if index then keep[index]=true;stack[#stack+1]={first,index};stack[#stack+1]={index,last} end
 end
 local out={};for i=1,#source-1 do if keep[i] then out[#out+1]=source[i] end end
 return out
end
function M.reduce(E)
 if not E.stroke then return end
 E.polygon=M.simplify(E.stroke.points,E.strokeTolerance or 1.5)
 E.stroke.error=nil
 if E.stroke.overflow then E.stroke.error='overflow'
 elseif #E.polygon>128 then E.stroke.error='limit'
 elseif #E.polygon<3 or not Geometry.simple(E.polygon) then E.stroke.error='invalid' end
 E.status='';E.cursor=nil;E.canvasDirty=true
end
function M.ready(E)
 if not E.stroke or E.drag or E.stroke.error then
  E.status=L(E.stroke and E.stroke.error or 'draw_help');return false
 end
 return true
end
function M.panel(E,finish,cancel)
 local changed,value=tImGui.SliderFloat(L('tolerance'),E.strokeTolerance or 1.5,.1,20,'%.2f')
 if changed then E.strokeTolerance=math.max(.1,math.min(20,value));if not E.drag then M.reduce(E) end end
 if E.stroke then
  tImGui.Text(string.format(L('count'),#E.stroke.points,#E.polygon))
  if E.stroke.error then tImGui.TextWrapped(L(E.stroke.error)) end
 end
 if tImGui.Button(L('finish')) and M.ready(E) then finish() end
 tImGui.SameLine();if tImGui.Button(tLang.L('ime_cancel')) then cancel() end
 tImGui.TextWrapped(L('draw_help'))
end
function M.input(E,event,mx,my,origin)
 local r=E.tool=='hole_freehand' and Model.region(E.project,E.selected) or nil
 if E.tool=='hole_freehand' and not r then return false end
 local left,top=r and r.x or 0,r and r.y or 0
 local right=left+(r and r.w or E.project.image.width)-1
 local bottom=top+(r and r.h or E.project.image.height)-1
 local x,y=(mx-origin.x)/origin.scale,(my-origin.y)/origin.scaleY
 if event=='down' then
  if x<left or y<top or x>right or y>bottom then return false end
  E.stroke={points={{x=x,y=y}}};E.polygon={{x=x,y=y}};E.cursor=nil;E.status=''
  E.drag={mode='freehand'};E.canvasDirty=true;return true
 end
 if not E.drag or E.drag.mode~='freehand' then return false end
 x=math.max(left,math.min(right,x));y=math.max(top,math.min(bottom,y))
 local points=E.stroke.points;local last=points[#points]
 local delta=((x-last.x)*origin.scale)^2+((y-last.y)*origin.scaleY)^2
 if delta>=4 or (event=='up' and delta>0) then
  if #points<4096 then
   points[#points+1]={x=x,y=y};E.polygon[#E.polygon+1]={x=x,y=y};E.canvasDirty=true
  else E.stroke.overflow=true end
 end
 if event=='up' then E.drag=nil;M.reduce(E) end
 return true
end
return M
