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
local obstacleApi={};assert(loadfile('src/test-lib/mesh_coplanar_obstacles_smoke.lua'))(obstacleApi)
local function create(tri,axis,scale,reverse,near)
 local _,v,idx=helper.grid('hole');local offset={16,-16,8}
 local function transform(p)
  local q={p.x,p.y,p.z};local n={p.nx,p.ny,p.nz}
  p.x=(q[axis%3+1]+offset[1])*scale;p.y=(q[(axis+1)%3+1]+offset[2])*scale;p.z=(q[(axis+2)%3+1]+offset[3])*scale
  p.nx=n[axis%3+1];p.ny=n[(axis+1)%3+1];p.nz=n[(axis+2)%3+1]
 end
 for _,p in ipairs(v) do if near then p.z=1e-4*p.x*p.x end;transform(p) end
 local d=meshDebug:new();d:setType('mesh');d:setModeFrontFace('CCW');d:addFrame(3);d:addSubSet(1)
 assert(d:addVertex(1,1,v));assert(d:addIndex(1,1,idx));d:addSubSet(1)
 local ov={};for _,p in ipairs(tri) do
  local q={x=p[1],y=p[2],z=p[3],u=p[1]/8,v=p[2]/8,nx=0,ny=0,nz=2};transform(q);ov[#ov+1]=q
 end
 assert(d:addVertex(1,2,ov));assert(d:addIndex(1,2,reverse and {3,2,1} or {1,2,3}))
 return d
end
function onInitScene()
 local ok,e=xpcall(function()
  -- Projection reaches x=7 (solid domain); the z=0 cut stays at x=4.25 in the hole.
  local disjoint={{4,3.5,-1},{4,4.5,-1},{7,4,11}}
  -- A perpendicular plate has a degenerate projection but a positive 3D gap.
  local perpendicular={{4,3.5,-1},{4,4.5,-1},{4,4,1}}
  for _,tri in ipairs{disjoint,perpendicular} do
   for axis=0,2 do for _,scale in ipairs{1/1024,1,1024} do for _,reverse in ipairs{false,true} do
    for _,reduce in ipairs{false,true} do for _,selected in ipairs{false,true} do
     local d=create(tri,axis,scale,reverse);local before=obstacleApi.subsetSignature(d)
     local r,e=d:simplify(nil,selected and 1 or nil,1,true,0,'coplanar',nil,nil,reduce);assert(r,e)
     assert(not r.unchanged and #d:getIndex(1,1)/3==(reduce and 8 or 40),'3D gap rejected')
     assert(before==obstacleApi.subsetSignature(d),'obstacle changed')
     assert(assert(d:simplify(nil,selected and 1 or nil,1,true,0,'coplanar',nil,nil,reduce)).unchanged)
    end end
   end end end
  end
  local rejected={
   {{4,3.5,-1},{4,4.5,-1},{7,4,1}}, -- plane cut x=5.5 crosses solid material
   {{4,3.5,-1},{4,4.5,-1},{7,4,2}}, -- plane cut exactly x=5 touches inner boundary
   {{5,3.5,-1},{5,4.5,-1},{5,4,1}}, -- perpendicular boundary contact
   {{4,3.5,-1},{4,4.5,1},{4,4.5,1}}, -- degenerate 3D obstacle
  }
  for _,tri in ipairs(rejected) do for axis=0,2 do
   local d=create(tri,axis,1,false);local before=helper.signature(d)
   local r=assert(d:simplify(nil,1,1,true,0,'coplanar',nil,nil,true))
   assert(r.unchanged and r.planarSurroundings>0 and helper.signature(d)==before,'unsafe crossing accepted')
  end end
  local d=create(disjoint,0,1,false,true);local before=helper.signature(d)
  local r=assert(d:simplify(nil,1,1,true,0,'coplanar',.001,5))
  assert(r.unchanged and helper.signature(d)==before,'approximate region changed')
  print('COPLANAR 3D SEPARATION OK')
 end,debug.traceback)
 if not ok then print('COPLANAR 3D SEPARATION FAIL '..tostring(e)) end
 mbm.quit()
end
function onLoop() end
