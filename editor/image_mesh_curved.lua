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
local Graph=require 'image_mesh_curved_hierarchy'
local Facets=require 'image_mesh_facets'
local M={hierarchy=Graph}
local function L(key) return tLang.L('ime_curved_'..key) end
function M.panel(E,apply,action)
 Facets.panel(E)
 if E.draft and E.draft.curvedNodes and not E.editDefaults then return Graph.panel(E,apply,action) end
 local convert=false
 if E.draft and not E.editDefaults then
  tImGui.BeginDisabled(E.values.curvedFaceted)
  convert=tImGui.Button(L('enable_hierarchy'))
  tImGui.EndDisabled()
 end
 if convert and apply() then
  if action(function(p) local r=Model.region(p,E.selected);Model.curved.convert(r,Model.options(p,r));E.curvedNode=1 end) then
   Graph.open(E,function() return true end)
  end
  return
 end
 local v=E.values
 for _,key in ipairs({'curvedEdge','curvedTarget','curvedX','curvedY','curvedRadius','heightTolerance'}) do
  tImGui.TextWrapped(L(key))
  tImGui.SetNextItemWidth(-1)
  tImGui.BeginDisabled(key=='heightTolerance' and v.curvedFaceted)
  local c,n=tImGui.InputFloat('##ime_curved_'..key,v[key],.01,1,'%.3f')
  tImGui.EndDisabled()
  if c then v[key]=Model.clampOption(key,n,v[key]) end
  if key=='curvedRadius' then tImGui.TextWrapped(L('radius_help')) end
 end
 if tImGui.Button(L('invert')) then v.curvedEdge,v.curvedTarget=v.curvedTarget,v.curvedEdge end
 v.curvedSymmetric=tImGui.Checkbox(L('symmetric'),v.curvedSymmetric)
 tImGui.TextWrapped(L('help'))
 if E.editMode and not E.editDefaults then
  if E.heightView==3 then E.heightView=2 end
  local c,view=tImGui.Combo(tLang.L('ime_height_view'),E.heightView,
   {tLang.L('ime_original_image'),tLang.L('ime_height_map')})
  if c then E.heightView=view end
  if tImGui.Button(L('edit')) and apply() then
   E.tool='curved';E.canvasDirty=true
   if E.paint then E.paint.enabled=false end
  end
 end
end
function M.draw(E,draw,handle)
 if E.editDefaults then return end
 local r=Model.region(E.project,E.selected)
 if not r then return end
 local o=Model.options(E.project,r)
 if o.heightSource~='curved' then return end
 if r.curvedNodes then return Graph.draw(E,draw,handle,r) end
 local x,y=r.x+o.curvedX*(r.w-1),r.y+o.curvedY*(r.h-1)
 local rx,ry=o.curvedRadius/o.width*(r.w-1),o.curvedRadius/o.height*(r.h-1)
 local points={}
 if rx>0 and ry>0 then
  for i=0,63 do local a=i*math.pi/32;points[#points+1]={x=x+rx*math.cos(a),y=y+ry*math.sin(a)} end
  draw(points,true,'area')
 end
 handle(x,y,'area')
 if E.tool=='curved' and rx>0 then handle(x+rx,y,'area') end
end
function M.input(E,H,event,mx,my,origin,radius)
 local r=Model.region(E.project,E.selected)
 if not r or r.locked or E.editDefaults then return false end
 local o=Model.options(E.project,r)
 if o.heightSource~='curved' then return false end
 if r.curvedNodes then return Graph.input(E,H,event,mx,my,origin,radius,r) end
 local x,y=(mx-origin.x)/origin.scale,(my-origin.y)/origin.scaleY
 if event=='down' then
  local cx,cy=r.x+o.curvedX*(r.w-1),r.y+o.curvedY*(r.h-1)
  local rx=o.curvedRadius/o.width*(r.w-1)
  local function near(px,py) return math.abs(x-px)*origin.scale<=radius+6 and math.abs(y-py)*origin.scaleY<=radius+6 end
  local resize=rx>0 and near(cx+rx,cy)
  if not resize and not near(cx,cy) then return false end
  E.drag={mode='curved',before=Model.copy(E.project),resize=resize,options=o,x=x,y=y}
  return true
 end
 local d=E.drag
 if not d or d.mode~='curved' then return false end
 if event=='move' then
  local dx,dy=(x-d.x)/math.max(1,r.w-1),(y-d.y)/math.max(1,r.h-1)
  if dx==d.lastX and dy==d.lastY then return true end
  d.lastX,d.lastY=dx,dy
  if d.resize then r.overrides.curvedRadius=Model.clampOption('curvedRadius',d.options.curvedRadius+dx*o.width,0)
  else
   r.overrides.curvedX=Model.clampOption('curvedX',d.options.curvedX+dx,.5)
   r.overrides.curvedY=Model.clampOption('curvedY',d.options.curvedY+dy,.5)
  end
  d.changed=true;E.canvasDirty=true
 elseif event=='up' then E.drag=nil;if d.changed then H.commitDrag(d.before) end;E.canvasDirty=true end
 return true
end
return M
