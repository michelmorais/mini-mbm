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
local Help=require 'image_mesh_help'
local Geometry=require 'image_mesh_holes_geometry'
local Freehand=require 'image_mesh_freehand'
local Holes=require 'image_mesh_holes'
local M={}
local function L(key) return tLang.L('ime_areas_'..key) end
local function clamp(v,a,b) return math.max(a,math.min(b,v)) end
function M.active(E) return E.tool=='height_areas' or E.tool=='area_draw' or E.tool=='area_freehand' end
function M.different(a,b)
 a,b=a or {},b or {}
 if #a~=#b then return true end
 for i,area in ipairs(a) do
  local other=b[i]
  for _,key in ipairs({'name','shape','enabled','height','transition'}) do if area[key]~=other[key] then return true end end
  if #area~=#other then return true end
  for j,p in ipairs(area) do if p.x~=other[j].x or p.y~=other[j].y then return true end end
 end
 return false
end
local function activate(E)
 E.tool='height_areas';E.polygon={};E.stroke=nil;E.canvasDirty=true
 if E.paint then E.paint.enabled=false end
end
function M.add(E,action,shape,points)
 local ok=action(function(project)
  local r=assert(Model.region(project,E.selected));r.heightAreas=r.heightAreas or {}
  assert(#r.heightAreas<32,'ime_areas_limit')
  local area=Model.copy(points or {})
  if not points then
   if shape=='ellipse' then
    for i=0,31 do local a=i*math.pi/16;area[#area+1]={x=.5+.2*math.cos(a),y=.5+.2*math.sin(a)} end
   else area={{x=.3,y=.3},{x=.7,y=.3},{x=.7,y=.7},{x=.3,y=.7}} end
  end
  area.name=L('name')..' '..(#r.heightAreas+1);area.shape=shape
  area.height=.75;area.transition=.02;area.enabled=true
  r.heightAreas[#r.heightAreas+1]=area
  if Model.options(project,r).heightSource=='image' then r.overrides.heightSource='mixed';r.overrides.followImage=true end
 end)
 if ok then E.areaIndex=#Model.region(E.project,E.selected).heightAreas;activate(E) end
 return ok
end
function M.finish(E,action)
 local r=Model.region(E.project,E.selected);if not r or #E.polygon<3 then return false end
 local points={}
 for _,p in ipairs(E.polygon) do points[#points+1]={x=(p.x-r.x)/math.max(1,r.w-1),y=(p.y-r.y)/math.max(1,r.h-1)} end
 return M.add(E,action,'polygon',points)
end
function M.change(E,action,kind)
 local index=E.areaIndex or 1
 local nextIndex=index
 local ok=action(function(project)
  local areas=assert(Model.region(project,E.selected).heightAreas)
  if kind=='remove' then table.remove(areas,index);nextIndex=math.max(1,index-1)
  elseif kind=='duplicate' then
   assert(#areas<32,'ime_areas_limit');local a=Model.copy(areas[index]);a.name=a.name:sub(1,120)..' (copy)';table.insert(areas,index+1,a);nextIndex=index+1
  else
   nextIndex=clamp(index+(kind=='up' and -1 or 1),1,#areas)
   areas[index],areas[nextIndex]=areas[nextIndex],areas[index]
  end
 end)
 if ok then E.areaIndex=nextIndex;E.canvasDirty=true end
 return ok
end
function M.resize(area,width,height)
 local b=Geometry.bounds(area);local x,y=b.x-b.rx,b.y-b.ry
 width=clamp(width,.0001,1-x);height=clamp(height,.0001,1-y)
 for _,p in ipairs(area) do p.x=x+(p.x-x)*width/(2*b.rx);p.y=y+(p.y-y)*height/(2*b.ry) end
end
function M.modePanel(E)
 local sources={'image','manual','mixed'};local index=1
 for i,v in ipairs(sources) do if E.values.heightSource==v then index=i end end
 local c,v=tImGui.Combo(L('source'),index,{L('image'),L('manual'),L('mixed')})
 if c then E.values.heightSource=sources[v];if v~=1 then E.values.followImage=true end;if v==2 and E.heightView==3 then E.heightView=2 end end
 Help.show('mode_'..E.values.heightSource)
 if E.values.heightSource=='manual' then
  c,v=tImGui.SliderFloat(L('base'),E.values.baseHeight,0,1,'%.3f')
  Help.show('base')
  if c then E.values.baseHeight=Model.clampOption('baseHeight',v,E.values.baseHeight) end
 end
end
function M.panel(E,action,apply)
 if E.editDefaults or not E.draft then return end
 if E.values.heightSource=='image' then
  if #(E.draft.heightAreas or {})>0 then tImGui.TextWrapped(L('inactive_mode')) end
  return
 end
 tImGui.Separator();tImGui.Text(L('title'))
 local c,v
 local areas=E.draft.heightAreas or {}
 if #areas>0 then
  E.areaIndex=clamp(E.areaIndex or 1,1,#areas)
  local names={};for i,a in ipairs(areas) do names[i]=i..': '..a.name end
  c,v=tImGui.Combo(L('selected'),E.areaIndex,names)
  Help.show('order')
  if c then E.areaIndex=v;E.canvasDirty=true end
  local a=areas[E.areaIndex]
  c,v=tImGui.InputText(L('name'),a.name);if c then a.name=v:gsub('%c',''):sub(1,128) end
  a.enabled=tImGui.Checkbox(L('enabled'),a.enabled)
  Help.show('enabled')
  if not a.enabled then tImGui.TextWrapped(L('disabled')) end
  c,v=tImGui.SliderFloat(L('height'),a.height,0,1,'%.3f');if c then a.height=Model.clampNumber(v,0,1,a.height) end
  Help.show('height')
  tImGui.TextWrapped(string.format(L('world_height'),a.height,E.values.relief,a.height*E.values.relief))
  if E.values.heightSource=='manual' then
   tImGui.TextWrapped(string.format(L('relative_height'),(a.height-E.values.baseHeight)*E.values.relief))
  end
  tImGui.TextWrapped(L('height_help'))
  if tImGui.Button(tLang.L('ime_apply')..'##height_area') then apply();return end
  local span=math.max(1,math.min(E.draft.w,E.draft.h)-1)
  c,v=tImGui.InputFloat(L('transition'),a.transition*span,.5,5,'%.2f');if c then a.transition=Model.clampNumber(v,0,span,a.transition*span)/span end
  Help.show('transition')
  local b=Geometry.bounds(a);local w,h=math.max(1,E.draft.w-1),math.max(1,E.draft.h-1)
  c,v=tImGui.InputFloat(L('width'),2*b.rx*w,1,10,'%.2f');if c then M.resize(a,Model.clampNumber(v,1,w,2*b.rx*w)/w,2*b.ry) end
  Help.show('size')
  b=Geometry.bounds(a)
  c,v=tImGui.InputFloat(L('height_px'),2*b.ry*h,1,10,'%.2f');if c then M.resize(a,2*b.rx,Model.clampNumber(v,1,h,2*b.ry*h)/h) end
  Help.show('size')
  for _,kind in ipairs({'up','down','duplicate','remove'}) do
   if kind=='down' or kind=='remove' then tImGui.SameLine() end
   if tImGui.Button(L(kind)) and apply() then M.change(E,action,kind) end
   if kind=='up' or kind=='down' then Help.show('order') elseif kind=='duplicate' then Help.show('duplicate') end
  end
  if E.editMode then
   local edit=tImGui.Checkbox(L('edit'),E.tool=='height_areas')
   if edit~=(E.tool=='height_areas') and apply() then activate(E);if not edit then E.tool='select' end end
  end
 end
 tImGui.Separator()
 if E.editMode then
  c,v=tImGui.Combo(L('primitive'),E.areaPrimitive or 1,{L('rectangle'),L('ellipse')});if c then E.areaPrimitive=v end
  if tImGui.Button(L('add')) and apply() then M.add(E,action,E.areaPrimitive==2 and 'ellipse' or 'rectangle') end
  if tImGui.Button(L('draw')) and apply() then activate(E);E.tool='area_draw' end
  tImGui.SameLine()
  if tImGui.Button(L('freehand')) and apply() then activate(E);E.tool='area_freehand' end
  if E.tool=='area_freehand' then Freehand.panel(E,function() M.finish(E,action) end,function() activate(E) end) end
  if E.tool=='area_draw' then
   if tImGui.Button(L('finish')) then M.finish(E,action) end
   tImGui.SameLine();if tImGui.Button(tLang.L('ime_cancel')) then activate(E) end
  end
 end
 tImGui.TextWrapped(L('help'))
end
function M.handles(r,a)
 if a.shape=='polygon' then return Holes.points(r,a) end
 local b=Geometry.bounds(a)
 return {{x=r.x+(b.x+b.rx)*(r.w-1),y=r.y+(b.y+b.ry)*(r.h-1),resize=true}}
end
function M.draw(E,draw,handle)
 if not M.active(E) then return end
 local r=Model.region(E.project,E.selected);if not r then return end
 for i,a in ipairs(r.heightAreas or {}) do
  local color=not a.enabled and 'area_disabled' or (i==E.areaIndex and 'side' or 'area')
  draw(Holes.points(r,a),true,color)
  if i==E.areaIndex and E.tool=='height_areas' then for _,p in ipairs(M.handles(r,a)) do handle(p.x,p.y,color) end end
 end
end
function M.input(E,H,event,mx,my,origin,radius)
 local r=Model.region(E.project,E.selected);if not r then return false end
 local x,y=(mx-origin.x)/origin.scale,(my-origin.y)/origin.scaleY
 if E.tool=='area_draw' then
  E.cursor={x=x,y=y}
  if event=='down' and x>=r.x and y>=r.y and x<=r.x+r.w-1 and y<=r.y+r.h-1 then
   if #E.polygon<128 then E.polygon[#E.polygon+1]={x=x,y=y} end
   E.canvasDirty=true;return true
  end
  if #E.polygon>0 then E.canvasDirty=true end
  return false
 end
 local areas=r.heightAreas or {}
 if event=='down' then
  local index,point,resize
  local selected=areas[E.areaIndex or 1]
  if selected then for j,p in ipairs(M.handles(r,selected)) do
   if math.abs(p.x-x)*origin.scale<=radius+6 and math.abs(p.y-y)*origin.scaleY<=radius+6 then index=E.areaIndex or 1;point=j;resize=p.resize;break end
  end end
  if not index then for i=#areas,1,-1 do if Model.contains(Holes.points(r,areas[i]),x,y) then index=i;break end end end
  if not index then return false end
  E.areaIndex=index;E.canvasDirty=true
  E.drag={mode='height_area',index=index,point=point,resize=resize,x=x,y=y,before=Model.copy(E.project),area=Model.copy(areas[index])}
  return true
 end
 local d=E.drag;if not d or d.mode~='height_area' then return false end
 if event=='move' then
  local dx,dy=(x-d.x)/math.max(1,r.w-1),(y-d.y)/math.max(1,r.h-1)
  if dx==d.lastX and dy==d.lastY then return true end
  d.lastX=dx;d.lastY=dy
  local a=Model.copy(d.area);local b=Geometry.bounds(a)
  if d.resize then M.resize(a,2*b.rx+dx,2*b.ry+dy)
  elseif d.point then local p=a[d.point];p.x=clamp(p.x+dx,0,1);p.y=clamp(p.y+dy,0,1)
  else
   dx=clamp(dx,-b.x+b.rx,1-b.x-b.rx);dy=clamp(dy,-b.y+b.ry,1-b.y-b.ry)
   for _,p in ipairs(a) do p.x=p.x+dx;p.y=p.y+dy end
  end
  areas[d.index]=a;d.changed=true;E.canvasDirty=true
 elseif event=='up' then E.drag=nil;if d.changed then H.commitDrag(d.before) end;E.canvasDirty=true end
 return true
end
return M
