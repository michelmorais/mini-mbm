-- Engine root: mini-mbm --scene src/test-lib/fluid_convex_smoke.lua --disable_select_monitor
require 'box2dLiquidFun'
local world, objects, expected, gradual, initialized
local elapsed=0
local square={{x=-10,y=-10},{x=10,y=-10},{x=10,y=10},{x=-10,y=10}}
local function make(points,rise,kind)
 local f=world:createFluid({type=kind or 'convexPolygon',points=points},
  {radius=.3,texture='#FFFF00FF',riseTime=rise or 0})
 objects[#objects+1]=f
 return f
end
local function init()
 mbm.onErrorStop(true)
 world=box2dLiquidFun:new();world:setGravity(0,0);objects={}
 local rect=world:createFluid({type='rectangle',width=20,height=20},{radius=.3,texture='#FFFF00FF'})
 expected=rect:getParticleCount()
 local poly=make(square)
 assert(poly:getParticleCount()==expected,
  string.format('Square contour differs from rectangle: %d ~= %d',poly:getParticleCount(),expected))
 local hits=0
 poly:queryAABB(-.01,-.01,.01,.01,function() hits=hits+1 end)
 assert(hits==1,'Duplicate particles on shared diagonal')
 local reversed={square[4],square[3],square[2],square[1]}
 assert(make(reversed):getParticleCount()==expected,'Clockwise contour mismatch')
 gradual=make(square,1);assert(gradual:getParticleCount()==0)
 local triangles={square[1],square[2],square[3],square[1],square[3],square[4]}
 assert(make(triangles,0,'triangleList'):getParticleCount()==expected,'Triangle-list union mismatch')
 assert(make(triangles,0,'polygon'):getParticleCount()==expected,'Legacy polygon alias mismatch')
 assert(poly:add({type='convexPolygon',points=square},{x=50,y=0})==expected,'fluid:add mismatch')
 local hex={{x=-10,y=0},{x=-5,y=-8},{x=5,y=-8},{x=10,y=0},{x=5,y=8},{x=-5,y=8}}
 assert(make(hex):getParticleCount()>0,'Hexagon empty')
 local outline=line:new('2dw',0,-40)
 outline:setPhysics({type='convexPolygon',points=hex},true)
 objects[#objects+1]=outline
 assert(world:addStaticBody(outline),'Shared shape parser did not create body')
 local invalid={
  {},{{x=0,y=0},{x=1,y=0}},
  {{x=0,y=0},{x=1,y=0},{x=2,y=0}},
  {{x=0,y=0},{x=10,y=0},{x=2,y=2},{x=0,y=10}},
  {square[1],square[3],square[2],square[4]},
  {square[1],square[2],square[3],square[1]},
  {{x=0,y=0},{x=10,y=0},{x=0,y=math.huge}},
  {{x=0,y=0},{x=10,y=0},{x=0}},
 }
 for i,points in ipairs(invalid) do
  assert(not pcall(function() make(points) end),'Invalid contour accepted: '..i)
 end
 initialized=true
end
local function check(dt)
 assert(initialized,'Initialization failed')
 elapsed=elapsed+dt
 if elapsed>1.2 then
  assert(gradual:getParticleCount()==expected,'riseTime changed filled particle count')
  print('FLUID_CONVEX_SMOKE_OK',expected)
  mbm.quit()
 end
end

local function checked(fn,...)
 local ok,err=pcall(fn,...)
 if not ok then
  print('FLUID_CONVEX_SMOKE_FAILED',tostring(err))
  mbm.quit()
 end
end
function onInitScene() checked(init) end
function onLoop(dt) checked(check,dt) end
