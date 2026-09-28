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
package.path='editor/?.lua;'..package.path
local Auto=require 'image_mesh_auto'
local Detect=require 'image_mesh_detect'
local Freehand=require 'image_mesh_freehand'
local Model=require 'image_mesh_model'
local Geometry=require 'image_mesh_holes_geometry'
local IO=require 'image_mesh_io'
tLang=require 'lang.language'
local E={project=Model.new('/tmp/source.png',1024,1024),polygon={}}
local a=Auto.state(E)
assert(a.maxVertices==128 and a.samplingStep==0)
local points={}
for i=1,582 do
 local angle=i*2*math.pi/582
 points[i]={x=512+400*math.cos(angle),y=512+400*math.sin(angle)}
end
E.stroke={points=points,maxVertices=128};E.strokeTolerance=.001
Freehand.reduce(E);assert(#E.polygon==582 and E.stroke.error=='limit')
E.stroke.maxVertices=500;Freehand.reduce(E)
assert(not Freehand.ready(E) and E.status:find('500',1,true))
-- Exercise InputInt changes through the real panel and ensure idle draws do no geometry work.
local pending={ime_auto_max_vertices=600}
tImGui={InputInt=function(key,value)
 local nextValue=pending[key];pending[key]=nil
 return nextValue~=nil,nextValue or value
end,Combo=function(_,v) return false,v end,SliderInt=function(_,v) return false,v end,
SliderFloat=function(_,v) return false,v end,Checkbox=function(_,v) return v end,
SetNextItemWidth=function() end,IsItemHovered=function() return false end,
Button=function() return false end,Text=function() end,TextWrapped=function() end,SameLine=function() end}
local language=tLang.L;tLang.L=function(key) return key end
Auto.panel(E,function() error('unexpected finish') end,function() error('unexpected hole') end)
assert(a.maxVertices==600 and not E.stroke.error and Freehand.ready(E))
local preview=E.polygon
Auto.panel(E,function() end,function() end);assert(E.polygon==preview,'idle panel rebuilt contour')
local region=Model.fromPoints(E.project,E.polygon)
region.holes={{}}
for i,p in ipairs(region.contour) do region.holes[1][i]={x=.5+(p.x-.5)*.5,y=.5+(p.y-.5)*.5} end
Model.validate(E.project)
local function encode(v)
 if type(v)=='table' then
  local fields={};for k,item in pairs(v) do fields[#fields+1]='['..encode(k)..']='..encode(item) end
  return '{'..table.concat(fields,',')..'}'
 end
 if type(v)=='string' then return string.format('%q',v) end
 return tostring(v)
end
local path=os.tmpname()
IO.save(E.project,path,function(name,v,out) out[#out+1]=name..'='..encode(v) end)
local restored=IO.load(path);os.remove(path)
assert(#restored.regions[1].contour==582 and #restored.regions[1].holes[1]==582)
pending.ime_auto_sampling_step=1
E.autoTask=coroutine.create(function() end)
Auto.panel(E,function() end,function() end)
assert(a.samplingStep==1 and not E.stroke and not E.autoTask and #E.polygon==0)
pending.ime_auto_max_vertices=-5;pending.ime_auto_sampling_step=-1
Auto.panel(E,function() end,function() end);assert(a.maxVertices==3 and a.samplingStep==0)
pending.ime_auto_max_vertices=99999
Auto.panel(E,function() end,function() end);assert(a.maxVertices==Geometry.maxContourPoints)
tLang.L=language
for _,lang in ipairs{'en','pt_br'} do
 tLang.setLanguage(lang)
 assert(not string.format(tLang.L('ime_auto_result'),582,600,4):find('%%d',1,true))
 assert(string.format(tLang.L('ime_auto_vertex_limit'),600):find('600',1,true))
end
local bytes=string.rep(string.char(255,255,255,255),1024*1024)
local options={mode=1,alpha=127}
local crop={x=0,y=0,w=1024,h=1024}
local _,step,count=Detect.trace(bytes,1024,1024,crop,10,10,options,function() end)
assert(step==2 and count==262144)
options.samplingStep=1
_,step,count=Detect.trace(bytes,1024,1024,crop,10,10,options,function() end)
assert(step==1 and count==1048576)
options.samplingStep=4
_,step,count=Detect.trace(bytes,1024,1024,crop,10,10,options,function() end)
assert(step==4 and count==65536)
options.samplingStep=1
local ok,err=pcall(Detect.trace,'',4096,4096,{x=0,y=0,w=4096,h=4096},0,0,options,function() end)
assert(not ok and err:find('ime_auto_sampling_large',1,true))
print('AUTO SETTINGS / 582 POINTS / HOLES / SAVE / LOAD / IDLE / SAMPLING OK')
