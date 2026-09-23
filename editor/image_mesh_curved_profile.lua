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

local C=require 'image_mesh_curved_model'
local Help=require 'image_mesh_help'
local M={}
local profiles={'linear','smooth','bezier'}
local curveColor={r=.2,g=.85,b=1,a=1}
local guideColor={r=.55,g=.55,b=.55,a=.7}
local controlColor={r=1,g=.75,b=.2,a=1}
local function L(key) return tLang.L('ime_curved_'..key) end
local function control(n,index,value)
 if index==1 then n.bezier1=math.max(0,math.min(n.bezier2 or 1,value))
 else n.bezier2=math.min(1,math.max(n.bezier1 or 0,value)) end
end
function M.panel(E,n)
 local index=1
 for i,p in ipairs(profiles) do if n.profile==p then index=i end end
 tImGui.TextWrapped(L('profile'));tImGui.SetNextItemWidth(-1)
 local changed,value=tImGui.Combo('##ime_curved_profile',index,{L('profile_linear'),L('profile_smooth'),L('profile_bezier')})
 if changed then n.profile=profiles[value];E.profileDrag=nil end
 if tImGui.IsItemHovered() then Help.tooltip(L('profile_help')) end
 if n.profile=='bezier' then
  for i=1,2 do
   tImGui.TextWrapped(L('bezier'..i));tImGui.SetNextItemWidth(-1)
   local c,v=tImGui.SliderFloat('##ime_bezier'..i,n['bezier'..i] or (i-1),0,1,'%.3f')
   if c then control(n,i,v) end
  end
 end
 local width=math.max(40,tImGui.GetContentRegionAvail().x)
 local height=100
 tImGui.InvisibleButton('##ime_profile_preview',{x=width,y=height})
 local origin=tImGui.GetItemRectMin()
 local x0,y0=origin.x+8,origin.y+height-8
 local w,h=width-16,height-16
 local function point(t,v) return {x=x0+t*w,y=y0-v*h} end
 if n.profile=='bezier' then
  local mouse=tImGui.GetMousePos()
  if tImGui.IsItemClicked(0) then
   E.profileDrag=nil
   for i=1,2 do local p=point(i/3,n['bezier'..i] or (i-1))
    if (p.x-mouse.x)^2+(p.y-mouse.y)^2<=100 then E.profileDrag=i end
   end
  end
  if tImGui.IsItemActive() and E.profileDrag then control(n,E.profileDrag,(y0-mouse.y)/h)
  else E.profileDrag=nil end
  local a,b=point(1/3,n.bezier1 or 0),point(2/3,n.bezier2 or 1)
  tImGui.AddLine(point(0,0),a,guideColor,1);tImGui.AddLine(a,b,guideColor,1);tImGui.AddLine(b,point(1,1),guideColor,1)
  tImGui.AddCircleFilled(a,5,controlColor,12);tImGui.AddCircleFilled(b,5,controlColor,12)
 end
 local profile,b1,b2=n.profile or 'linear',n.bezier1 or 0,n.bezier2 or 1
 if E.profileKind~=profile or E.profileB1~=b1 or E.profileB2~=b2 then
  E.profileKind=profile;E.profileB1=b1;E.profileB2=b2;E.profileSamples={}
  for i=0,48 do E.profileSamples[i+1]=C.profileValue(n,i/48) end
  E.profileBuilds=(E.profileBuilds or 0)+1
 end
 tImGui.AddLine(point(0,0),point(1,0),guideColor,1)
 tImGui.AddLine(point(0,0),point(0,1),guideColor,1)
 for i=1,48 do tImGui.AddLine(point((i-1)/48,E.profileSamples[i]),point(i/48,E.profileSamples[i+1]),curveColor,2) end
 tImGui.TextWrapped(L('profile_axes'))
 if n.profile=='bezier' then tImGui.TextWrapped(L('bezier_help')) end
end
return M
