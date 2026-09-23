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
local Profile=require 'image_mesh_curved_profile'
local C=Model.curved
local M={}
local function L(key) return tLang.L('ime_curved_'..key) end
local kinds={'point','line','ellipse','rectangle','polygon'}
local function input(label,value,id)
 tImGui.TextWrapped(label);tImGui.SetNextItemWidth(-1)
 return tImGui.InputFloat('##ime_graph_'..id,value,.01,1,'%.3f')
end
local function outer(E,r)
 local points=Model.outline(r,r.overrides.ellipseSegments or E.project.defaults.ellipseSegments or 48)
 for _,p in ipairs(points) do p.x=(p.x-r.x)/math.max(1,r.w-1);p.y=(p.y-r.y)/math.max(1,r.h-1) end
 return points
end
local function reject(E,reason,field,value)
 E.graphEditError=L(reason)
 if field then
  E.graphEditError=string.format(L('rejected_value'),L(field),tostring(value),E.graphEditError)
 end
 tUtil.showMessageWarn(E.graphEditError,8)
 -- Validation feedback must not steal focus from the numeric input.
 tUtil.bFocusMsgOnce=false
 return false
end
function M.edit(E,change,field,value)
 local candidate=Model.copy(E.draft.curvedNodes)
 if change(candidate)==false then return reject(E,'edit_bounds',field,value) end
 local valid,reason=C.geometry(candidate,outer(E,E.draft))
 if not valid then return reject(E,reason,field,value) end
 E.draft.curvedNodes=candidate;E.graphEditError=nil;E.graphLabelsSource=nil
 return true
end
local function geometryPanel(E,index)
 local n=E.draft.curvedNodes[index]
 if E.graphBoundsSource~=n then E.graphBoundsSource=n;E.graphBounds=C.bounds(n) end
 local b=E.graphBounds
 tImGui.TextWrapped(L('whole_shape'))
 for _,key in ipairs({'x','y','rx','ry'}) do
  if (key=='x' or key=='y') or #n>=3 then
   local changed,value=input(L('shape_'..key),b[key],'shape_'..key)
   if changed then M.edit(E,function(nodes) return C.transform(nodes,index,key,value) end,'shape_'..key,value) end
  end
 end
 if #n>2 and n.shape~='polygon' then
  tImGui.TextWrapped(L('primitive_help'))
  if tImGui.Button(L('convert_polygon')) then
   M.edit(E,function(nodes) nodes[index].shape='polygon' end)
  end
  return
 end
 if #n==1 then return end
 if not tImGui.CollapsingHeader(L('vertices_advanced')) then return end
 tImGui.TextWrapped(L('vertex_help'))
 E.curvedPoint=math.max(1,math.min(E.curvedPoint or 1,#n))
 local changed,point=tImGui.InputInt(L('vertex'),E.curvedPoint)
 if changed then E.curvedPoint=math.max(1,math.min(point,#n));E.canvasDirty=true end
 for _,axis in ipairs({'x','y'}) do
  local p=E.draft.curvedNodes[index][E.curvedPoint]
  local changed,value=input(L('vertex_'..axis),p[axis],'vertex_'..axis)
  if changed then M.edit(E,function(nodes) nodes[index][E.curvedPoint][axis]=value end,'vertex_'..axis,value) end
 end
 if #n>=3 then
  if #n<128 and tImGui.Button(L('add_vertex')) then
   if M.edit(E,function(nodes)
    local points=nodes[index];local a,b=points[E.curvedPoint],points[E.curvedPoint%#points+1]
    table.insert(points,E.curvedPoint+1,{x=(a.x+b.x)/2,y=(a.y+b.y)/2})
   end) then E.curvedPoint=E.curvedPoint+1 end
  end
  if #n>3 and tImGui.Button(L('remove_vertex')) then
   if M.edit(E,function(nodes) table.remove(nodes[index],E.curvedPoint) end) then E.curvedPoint=1 end
  end
 end
end
function M.open(E,apply)
 if E.drag or (E.draft and E.draft.locked) or not apply() then return false end
 E.curvedPanelOpen=true;E.tool='curved';E.canvasDirty=true
 if E.paint then E.paint.enabled=false end
 if (E.curvedNode or 0)==0 and E.draft and #E.draft.curvedNodes>0 then E.curvedNode=1 end
 return true
end
function M.close(E,apply)
 if E.drag or not apply() then return false end
 E.curvedPanelOpen=false;E.graphEditError=nil
 if E.tool=='curved' then E.tool='select' end
 E.canvasDirty=true
 return true
end
function M.panel(E,apply,action)
 local nodes=E.draft.curvedNodes
 if E.tool~='curved' then E.curvedPanelOpen=false end
 if E.curvedPanelOpen and tImGui.Button(L('back_to_relief')) then
  if M.close(E,apply) then return end
 end
 for _,key in ipairs({'curvedEdge','heightTolerance'}) do
  local c,n=input(L(key),E.values[key],key)
  if c then E.values[key]=Model.clampOption(key,n,E.values[key]) end
 end
 E.values.curvedSymmetric=tImGui.Checkbox(L('symmetric'),E.values.curvedSymmetric)
 if E.editMode then
  if E.heightView==3 then E.heightView=2 end
  local changed,view=tImGui.Combo(tLang.L('ime_height_view'),E.heightView,
   {tLang.L('ime_original_image'),tLang.L('ime_height_map')})
  if changed then E.heightView=view end
 end
 if not E.curvedPanelOpen then
  tImGui.TextWrapped(L('hierarchy_active'))
  if tImGui.Button(L('enable_hierarchy')) then M.open(E,apply) end
  return
 end
 if E.graphLabelsSource~=nodes then
  E.graphLabelsSource=nodes;E.graphLabels={L('root')};E.graphTargets={};E.graphKinds={}
  for i,k in ipairs(kinds) do E.graphKinds[i]=L(k) end
  for i,n in ipairs(nodes) do
   E.graphLabels[i+1]=i..': '..L(n.role)..' - '..L(n.shape)..' ('..n.parent..')'
   if n.role=='target' then E.graphTargets[n.parent]=true end
  end
 end
 tImGui.TextWrapped(L('selection'));tImGui.SetNextItemWidth(-1)
 local c,index=tImGui.Combo('##ime_graph_node',(E.curvedNode or 0)+1,E.graphLabels)
 if c and apply() then E.curvedNode=index-1;E.curvedPoint=1;E.graphEditError=nil;E.canvasDirty=true end
 nodes=E.draft.curvedNodes
 local selected=E.curvedNode or 0;local n=nodes[selected]
 if n then
  if n.role=='target' then
   local changed,value=input(L('curvedTarget'),n.thickness,'thickness')
   if changed then n.thickness=Model.clampOption('curvedTarget',value,n.thickness) end
   Profile.panel(E,n)
  else tImGui.TextWrapped(L('inherited')) end
  geometryPanel(E,selected)
  if tImGui.Button(L('remove_node')) and apply() then
   action(function(project)
    local r=Model.region(project,E.selected);r.curvedNodes=C.remove(r.curvedNodes,selected);E.curvedNode=0
   end)
   return
  end
 end
 if not n or #n>=3 then
  E.curvedKind=E.curvedKind or 1
  local labels=E.graphKinds
  tImGui.TextWrapped(L('shape'));tImGui.SetNextItemWidth(-1)
  local changed,kind=tImGui.Combo('##ime_graph_kind',E.curvedKind,labels)
  if changed then E.curvedKind=kind end
  local hasTarget=E.graphTargets[selected]
  local function add(role)
   if apply() then action(function(project)
    E.curvedNode=C.add(Model.region(project,E.selected),selected,role,kinds[E.curvedKind],E.values.curvedTarget)
   end) end
  end
  if not hasTarget then
   local clicked=tImGui.Button(L('add_target'))
   if tImGui.IsItemHovered() then Help.tooltip(L('add_target_help')) end
   if clicked then add('target');return end
  end
  if E.curvedKind>=3 then
   local clicked=tImGui.Button(L('add_region'))
   if tImGui.IsItemHovered() then Help.tooltip(L('add_region_help')) end
   if clicked then add('region');return end
  end
 else tImGui.TextWrapped(L('terminal')) end
 tImGui.TextWrapped(L('hierarchy_help'))
 tImGui.TextWrapped(L(E.editMode and 'editing_targets_canvas' or 'editing_targets_properties'))
 if tImGui.Button(L('back_to_relief')..'##curved_footer') then M.close(E,apply) end
end
function M.draw(E,draw,handle,r)
 if not E.curvedPanelOpen and E.tool~='curved' then return end
 local nodes=r.curvedNodes
 for i,n in ipairs(nodes) do
  local points={};for j,p in ipairs(n) do points[j]={x=r.x+p.x*(r.w-1),y=r.y+p.y*(r.h-1)} end
  if #points>1 then draw(points,#points>2,'area') end
  if #points==1 then handle(points[1].x,points[1].y,'area') end
  if i==E.curvedNode and E.tool=='curved' then
   local b=C.bounds(points);handle(b.x,b.y,'area')
   if #points>2 then handle(b.x+b.rx,b.y+b.ry,'area') end
   if #points<=2 or n.shape=='polygon' then for _,p in ipairs(points) do handle(p.x,p.y,'area') end end
  end
 end
end
function M.input(E,H,event,mx,my,origin,radius,r)
 local nodes=r.curvedNodes
 local x=((mx-origin.x)/origin.scale-r.x)/math.max(1,r.w-1)
 local y=((my-origin.y)/origin.scaleY-r.y)/math.max(1,r.h-1)
 local function near(px,py)
  return math.abs(x-px)*(r.w-1)*origin.scale<=radius+6 and math.abs(y-py)*(r.h-1)*origin.scaleY<=radius+6
 end
 if event=='down' then
  local index=E.curvedNode or 0;local n=nodes[index];local mode,point
  if n then
   local b=C.bounds(n)
   if #n>2 and near(b.x+b.rx,b.y+b.ry) then mode='resize'
   elseif near(b.x,b.y) then mode='move'
   elseif #n<=2 or n.shape=='polygon' then
    for j,p in ipairs(n) do if near(p.x,p.y) then mode='vertex';point=j;break end end
   end
  end
  if not mode then
   for i=#nodes,1,-1 do local b=C.bounds(nodes[i]);if near(b.x,b.y) then index=i;n=nodes[i];mode='move';break end end
  end
  if not mode then return false end
  E.curvedNode=index;E.curvedPoint=point or 1
  E.drag={mode='curved_graph',operation=mode,point=point,index=index,x=x,y=y,bounds=C.bounds(n),before=Model.copy(E.project),nodes=Model.copy(nodes)}
  E.canvasDirty=true;return true
 end
 local d=E.drag;if not d or d.mode~='curved_graph' then return false end
 if event=='move' then
  if x==d.lastX and y==d.lastY then return true end;d.lastX=x;d.lastY=y
  local previous=Model.copy(nodes)
  local dx,dy=x-d.x,y-d.y;local b=d.bounds
  local sx=math.max(.01,(b.rx+dx)/math.max(.000001,b.rx))
  local sy=math.max(.01,(b.ry+dy)/math.max(.000001,b.ry))
  for i,n in ipairs(nodes) do if C.descendant(nodes,i,d.index) then
   for j,p in ipairs(n) do local old=d.nodes[i][j]
    if d.operation=='move' then p.x=old.x+dx;p.y=old.y+dy
    elseif d.operation=='resize' then p.x=b.x+(old.x-b.x)*sx;p.y=b.y+(old.y-b.y)*sy
    elseif i==d.index and j==d.point then p.x=x;p.y=y;if #n>2 then n.shape='polygon' end end
   end
  end end
  local valid,reason=C.geometry(nodes,outer(E,r))
  if not valid then r.curvedNodes=previous;E.graphEditError=L(reason);return true end
  E.graphEditError=nil;d.changed=true;E.canvasDirty=true
 elseif event=='up' then E.drag=nil;if d.changed then H.commitDrag(d.before) end;E.canvasDirty=true end
 return true
end
return M
