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
local Detect=require 'image_mesh_detect'
local Freehand=require 'image_mesh_freehand'
local M={}
local function L(key) return tLang.L('ime_auto_'..key) end
function M.cancel(E)
 E.autoTask=nil;E.autoSource=nil;E.stroke=nil;E.polygon={};E.canvasDirty=true
end
function M.state(E)
 if not E.auto then E.auto={mode=1,alpha=127,tolerance=24,color={r=0,g=0,b=0,a=1},target=1,crop=true} end
 return E.auto
end
function M.resume(E)
 if not E.autoTask then return end
 if E.autoSource~=E.project or E.autoSelected~=E.selected then M.cancel(E);return end
 local ok,err=coroutine.resume(E.autoTask)
 if not ok or coroutine.status(E.autoTask)=='dead' then E.autoTask=nil;E.autoSource=nil end
 if not ok then error(err,0) end
end
function M.input(E,event,mx,my,origin)
 if event~='down' then return false end
 local x,y=math.floor((mx-origin.x)/origin.scale),math.floor((my-origin.y)/origin.scaleY)
 local image=E.project.image
 if x<0 or y<0 or x>=image.width or y>=image.height then return false end
 M.cancel(E)
 local options=Model.copy(M.state(E));local r=Model.region(E.project,E.selected)
 local crop={x=0,y=0,w=image.width,h=image.height}
 if options.target==2 or (options.crop and r) then
  if not r then E.status=L('select_module');return true end
  crop={x=r.x,y=r.y,w=r.w,h=r.h}
 end
 local picking=E.tool=='auto_background'
 E.autoSource=E.project;E.autoSelected=E.selected;E.status=''
 E.autoTask=coroutine.create(function()
  local bytes,w,h=mbm.readImagePixels(image.path)
  assert(bytes,w);assert(w==image.width and h==image.height,'ime_auto_changed_image')
  if picking then
   local red,green,blue=bytes:byte((y*w+x)*4+1,(y*w+x)*4+3)
   E.auto.color={r=red/255,g=green/255,b=blue/255,a=1};E.tool='auto_contour';return
  end
  local points,step,count=Detect.trace(bytes,w,h,crop,x,y,options,function() coroutine.yield() end)
  E.stroke={points=points};E.auto.step=step;E.auto.count=count
  Freehand.reduce(E)
 end)
 return true
end
function M.panel(E,finishModule,finishHole)
 local a=M.state(E)
 local function reset() M.cancel(E) end
 tImGui.SetNextItemWidth(220)
 local c,v=tImGui.Combo(L('target'),a.target,{L('module'),L('hole')})
 if c then a.target=v;reset() end
 c,v=tImGui.Combo(L('mode'),a.mode,{L('alpha'),L('background')})
 if c then a.mode=v;reset() end
 c,v=tImGui.SliderInt(L('alpha_limit'),a.alpha,0,254)
 if c then a.alpha=math.max(0,math.min(254,v));reset() end
 if a.mode==2 then
  c,v=tImGui.ColorEdit4(L('color'),a.color)
  if c then a.color=v;reset() end
  c,v=tImGui.SliderInt(L('tolerance'),a.tolerance,0,255)
  if c then a.tolerance=math.max(0,math.min(255,v));reset() end
  if tImGui.Button(L('pick')) then reset();E.tool='auto_background' end
 end
 if a.target==1 then
  local checked=tImGui.Checkbox(L('crop'),a.crop)
  if checked~=a.crop then a.crop=checked;reset() end
 end
 if E.autoTask then
  tImGui.Text(L('working'))
  if tImGui.Button(tLang.L('ime_cancel')) then reset() end
 else
  c,v=tImGui.SliderFloat(tLang.L('ime_freehand_tolerance'),E.strokeTolerance or 1.5,.1,20,'%.2f')
  if c then E.strokeTolerance=math.max(.1,math.min(20,v));Freehand.reduce(E) end
  if E.stroke then
   tImGui.Text(string.format(L('result'),#E.polygon,a.step or 1))
   if E.stroke.error then tImGui.TextWrapped(tLang.L('ime_freehand_'..E.stroke.error)) end
   if tImGui.Button(tLang.L('ime_freehand_finish')) and Freehand.ready(E) then
    if a.target==2 then finishHole() else finishModule() end
   end
   tImGui.SameLine();if tImGui.Button(tLang.L('ime_cancel')) then reset() end
  end
 end
 tImGui.TextWrapped(L(E.tool=='auto_background' and 'pick_help' or 'help'))
end
return M
