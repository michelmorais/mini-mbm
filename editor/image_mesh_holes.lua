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
local function L(key) return tLang.L('ime_holes_'..key) end
function M.points(r,hole)
 local points={}
 for _,p in ipairs(hole) do points[#points+1]={x=r.x+p.x*(r.w-1),y=r.y+p.y*(r.h-1)} end
 return points
end
function M.finish(E,action)
 local points=Model.copy(E.polygon)
 if #points<3 then E.status=L('points');return false end
 local ok=action(function(project)
  local r=assert(Model.region(project,E.selected));r.holes=r.holes or {}
  local hole={};for _,p in ipairs(points) do hole[#hole+1]={x=(p.x-r.x)/math.max(1,r.w-1),y=(p.y-r.y)/math.max(1,r.h-1)} end
  r.holes[#r.holes+1]=hole
 end)
 if ok then E.holeIndex=#Model.region(E.project,E.selected).holes;E.polygon={};E.tool='holes';E.canvasDirty=true end
 return ok
end
function M.add(E,action,kind)
 local ok=action(function(project)
  local r=assert(Model.region(project,E.selected));r.holes=r.holes or {}
  assert(#r.holes<16,'ime_holes_limit')
  local hole={}
  if kind==2 then
   for i=0,15 do local a=i*math.pi/8;hole[#hole+1]={x=.5+.1*math.min(r.w-1,r.h-1)/math.max(1,r.w-1)*math.cos(a),y=.5+.1*math.min(r.w-1,r.h-1)/math.max(1,r.h-1)*math.sin(a)} end
  else hole={{x=.4,y=.4},{x=.6,y=.4},{x=.6,y=.6},{x=.4,y=.6}} end
  local outer=Model.outline(r,r.overrides.ellipseSegments or project.defaults.ellipseSegments or 48)
  for _,p in ipairs(outer) do p.x=(p.x-r.x)/math.max(1,r.w-1);p.y=(p.y-r.y)/math.max(1,r.h-1) end
  -- Search outwards from the center only on Add, preserving the primitive size.
  for radius=0,9 do
   for y=-radius,radius do for x=-radius,radius do
    if math.max(math.abs(x),math.abs(y))==radius then
     local candidate={}
     for i,p in ipairs(hole) do candidate[i]={x=p.x+x*.05,y=p.y+y*.05} end
     if Geometry.canPlace(outer,r.holes,candidate) then
      if kind==2 then candidate.primitive='ellipse';candidate.preserveShape=true end
      r.holes[#r.holes+1]=candidate
      return
     end
    end
   end end
  end
  error('ime_holes_no_space')
 end)
 if ok then E.holeIndex=#Model.region(E.project,E.selected).holes;E.canvasDirty=true end
 return ok
end
function M.panel(E,action,apply)
 if E.editDefaults or not E.draft or not tImGui.CollapsingHeader(L('title')) then return end
 local r=Model.region(E.project,E.selected)
 local holes=r.holes or {}
 tImGui.Text(string.format(L('count'),#holes))
 if E.editMode then
  local c,v=tImGui.Combo(L('primitive'),E.holePrimitive or 1,{L('rectangle'),L('circle')})
  if c then E.holePrimitive=v end
  if tImGui.Button(L('add')) and apply() then
   if M.add(E,action,E.holePrimitive or 1) then E.tool='holes';E.canvasDirty=true;if E.paint then E.paint.enabled=false end end
  end
  tImGui.SameLine()
  if tImGui.Button(L('draw')) and apply() then E.tool='hole_draw';E.polygon={};E.canvasDirty=true;if E.paint then E.paint.enabled=false end end
  if E.tool=='hole_draw' then
   if tImGui.Button(L('finish')) then M.finish(E,action) end
   tImGui.SameLine();if tImGui.Button(tLang.L('ime_cancel')) then E.tool='holes';E.polygon={};E.canvasDirty=true end
  end
 end
 -- Actions replace the project; use the new list when selecting the new hole.
 holes=Model.region(E.project,E.selected).holes or {}
 if #holes>0 then
  E.holeIndex=math.min(E.holeIndex or 1,#holes)
  local names={};for i=1,#holes do names[i]=string.format(L('name'),i) end
  local changed,index=tImGui.Combo(L('selected'),E.holeIndex,names)
  if changed then E.holeIndex=index;E.canvasDirty=true end
  local selected=holes[E.holeIndex]
  if selected.primitive=='ellipse' then
   local preserve=tImGui.Checkbox(L('preserve'),selected.preserveShape==true)
   if preserve~=(selected.preserveShape==true) and apply() then
    M.setPreserve(E,action,preserve)
   end
   tImGui.TextWrapped(L('resize_help'))
  end
  if E.editMode then
   local edit=tImGui.Checkbox(L('edit'),E.tool=='holes')
   if edit~=(E.tool=='holes') and (not edit or apply()) then E.tool=edit and 'holes' or 'select';E.polygon={};E.canvasDirty=true;if E.paint then E.paint.enabled=false end end
  end
  if tImGui.Button(L('remove')) and apply() then
   if action(function(p) table.remove(Model.region(p,E.selected).holes,E.holeIndex) end) then E.holeIndex=1 end
  end
 end
 tImGui.TextWrapped(L('help'))
end
function M.setPreserve(E,action,preserve)
 return action(function(project)
  local hole=Model.region(project,E.selected).holes[E.holeIndex]
  if preserve then
   local b=Geometry.bounds(hole)
   for i,p in ipairs(hole) do local a=(i-1)*2*math.pi/#hole;p.x=b.x+b.rx*math.cos(a);p.y=b.y+b.ry*math.sin(a) end
  end
  hole.preserveShape=preserve
 end)
end
function M.handles(r,hole)
 if not hole.preserveShape or hole.primitive~='ellipse' then return M.points(r,hole) end
 local b=Geometry.bounds(hole)
 local x,y=r.x+b.x*(r.w-1),r.y+b.y*(r.h-1)
 local rx,ry=b.rx*(r.w-1),b.ry*(r.h-1)
 return {{x=x+rx,y=y,axis='width'},{x=x,y=y+ry,axis='height'},{x=x+rx,y=y+ry,axis='circle'}}
end
function M.draw(E,draw,handle)
 for _,r in ipairs(E.project.regions) do for i,hole in ipairs(r.holes or {}) do
  local points=M.points(r,hole);draw(points,true,'hole')
  if r.id==E.selected and i==(E.holeIndex or 1) and E.tool=='holes' then
   for _,p in ipairs(M.handles(r,hole)) do handle(p.x,p.y,'hole') end
  end
 end end
end
function M.input(E,H,event,mx,my,origin,radius)
 local r=Model.region(E.project,E.selected);if not r then return false end
 local x,y=(mx-origin.x)/origin.scale,(my-origin.y)/origin.scaleY
 if E.tool=='hole_draw' then
  E.cursor={x=x,y=y}
  if event=='down' and x>=r.x and y>=r.y and x<r.x+r.w and y<r.y+r.h then
   if #E.polygon<128 then E.polygon[#E.polygon+1]={x=x,y=y} end
   E.canvasDirty=true;return true
  end
  if #E.polygon>0 then E.canvasDirty=true end
  return false
 end
 if event=='down' then
  local index,point,axis
  -- Only the selected hole shows handles; other holes are selected by their interior.
  local i=E.holeIndex or 1
  local hole=(r.holes or {})[i]
  if hole then
   local nearest=math.huge
   for j,p in ipairs(M.handles(r,hole)) do
    local dx,dy=(p.x-x)*origin.scale,(p.y-y)*origin.scaleY
    local distance=dx*dx+dy*dy
    if math.abs(dx)<=radius+6 and math.abs(dy)<=radius+6 and distance<nearest then
     index=i;point=j;axis=p.axis;nearest=distance
    end
   end
  end
  if not index then for i,hole in ipairs(r.holes or {}) do if Model.contains(M.points(r,hole),x,y) then index=i;break end end end
  if not index then return false end
  E.holeIndex=index;E.canvasDirty=true
  E.drag={mode='hole',index=index,point=point,axis=axis,id=r.id,x=x,y=y,before=Model.copy(E.project),hole=Model.copy(r.holes[index])}
  return true
 end
 local d=E.drag;if not d or d.mode~='hole' then return false end
 if event=='move' then
  local dx,dy=(x-d.x)/math.max(1,r.w-1),(y-d.y)/math.max(1,r.h-1)
  if d.lastX==dx and d.lastY==dy then return true end
  d.lastX=dx;d.lastY=dy
  r.holes[d.index]=Model.copy(d.hole)
  if d.axis then
   local b=Geometry.bounds(d.hole)
   local w,h=math.max(1,r.w-1),math.max(1,r.h-1)
   if d.axis=='width' then b.rx=math.max(.5/w,b.rx+dx)
   elseif d.axis=='height' then b.ry=math.max(.5/h,b.ry+dy)
   else
    local size=math.max(.5,b.rx*w+dx*w,b.ry*h+dy*h)
    b.rx=size/w;b.ry=size/h
   end
   for i,p in ipairs(r.holes[d.index]) do local a=(i-1)*2*math.pi/#d.hole;p.x=b.x+b.rx*math.cos(a);p.y=b.y+b.ry*math.sin(a) end
  else
   for i,p in ipairs(r.holes[d.index]) do if not d.point or i==d.point then p.x=p.x+dx;p.y=p.y+dy end end
  end
  d.changed=true;E.canvasDirty=true
 elseif event=='up' then
  E.drag=nil
  if d.changed then H.commitDrag(d.before) end
  E.canvasDirty=true
 end
 return true
end
return M
