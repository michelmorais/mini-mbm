--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------]]


local helper={};assert(loadfile('src/test-lib/mesh_coplanar_smoke.lua'))(helper)
local function create(kind,tri,slope,reverse,near)
 local _,v,idx=helper.grid(kind,slope)
 if near then for _,p in ipairs(v) do p.z=p.z+1e-8*p.x*p.x end end
 local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
 assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx));d:addSubSet(1)
 local ov={};for _,p in ipairs(tri) do ov[#ov+1]={x=p[1],y=p[2],z=slope*p[1],u=p[1]/8,v=p[2]/8,nx=0,ny=0,nz=2} end
 assert(d:addVertex(1,2,ov));assert(d:addIndex(1,2,reverse and {3,2,1} or {1,2,3}))
 return d
end
local function subsetSignature(d)
 local v=d:getVertex(1,2,1,d:getTotalVertex(1,2));local idx=d:getIndex(1,2)
 local triangles={}
 for i=1,#idx,3 do
  local corners={}
  for j=0,2 do local values={};local p=v[idx[i+j]]
   for _,k in ipairs{'x','y','z','u','v','nx','ny','nz'} do values[#values+1]=tostring(p[k]) end
   corners[#corners+1]=table.concat(values,',')
  end
  local rotations={}
  for j=1,3 do rotations[j]=corners[j]..'/'..corners[j%3+1]..'/'..corners[(j+1)%3+1] end
  table.sort(rotations);triangles[#triangles+1]=rotations[1]
 end
 table.sort(triangles);return table.concat(triangles,';')
end
function onInitScene()
 local ok,e=xpcall(function()
  for _,kind in ipairs{'hole','outer_concave'} do
   local tri=kind=='hole' and {{3.5,3.5},{4.5,3.5},{4,4.5}} or {{6,6},{7,6},{6,7}}
   for _,slope in ipairs{0,.5} do for _,reverse in ipairs{false,true} do
    for _,reduce in ipairs{false,true} do for _,selected in ipairs{false,true} do
     local d=create(kind,tri,slope,reverse);local before=subsetSignature(d)
     local r,err=d:simplify(nil,selected and 1 or nil,1,true,0,'coplanar',nil,nil,reduce);assert(r,err)
     assert(not r.unchanged and r.planarSurroundings==0,kind)
     assert(#d:getIndex(1,1)/3==(reduce and (kind=='hole' and 8 or 10) or 40))
     assert(before==subsetSignature(d),'obstacle changed')
     assert(assert(d:simplify(nil,selected and 1 or nil,1,true,0,'coplanar',nil,nil,reduce)).unchanged)
    end end
   end end
  end
  local rejected={
   {{2.5,3.5},{4.5,3.5},{4,4.5}}, -- crosses inner boundary
   {{3,3.5},{4.5,3.5},{4,4.5}}, -- touches inner boundary
   {{3,3},{4,3},{3.5,4}}, -- shares inner edge
   {{1,1},{2,1},{1,2}}, -- contained in solid domain
   {{-10,-10},{30,-10},{-10,30}}, -- surrounds the region
   {{3.5,3.5},{4,4},{4.5,4.5}}, -- degenerate obstacle
  }
  for _,tri in ipairs(rejected) do
   local d=create('hole',tri,0,false);local before=helper.signature(d)
   local r=assert(d:simplify(nil,1,1,true,0,'coplanar',nil,nil,true))
   assert(r.unchanged and r.planarSurroundings>0 and helper.signature(d)==before,'unsafe obstacle accepted')
  end
  local d=create('hole',{{3.5,3.5},{4.5,3.5},{4,4.5}},0,false,true);local before=helper.signature(d)
  local r=assert(d:simplify(nil,1,1,true,0,'coplanar'))
  assert(r.unchanged and helper.signature(d)==before,'near-plane guard')
  print('COPLANAR OBSTACLE SEPARATION OK')
 end,debug.traceback)
 if not ok then print('COPLANAR OBSTACLE SEPARATION FAIL '..tostring(e)) end
 mbm.quit()
end
function onLoop() end
