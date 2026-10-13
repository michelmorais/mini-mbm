-- From engine root: mini-mbm --scene src/test-lib/fluid_rise_smoke.lua --disable_select_monitor
require 'box2dLiquidFun'
local world, immediate, gradual, paused, mortal, doomed
local elapsed, count, checkpoint, resumed, complete, initialized, pausedCount = 0
local shapeDef={type='circle',ray=5}
local function create(rise,lifetime)
 return world:createFluid(shapeDef,{riseTime=rise,radius=.1,texture='#FFFF00FF',lifetime=lifetime or 0,fixtureMaskBits=65533})
end
function onInitScene()
 mbm.onErrorStop(true)
 world=box2dLiquidFun:new();world:setGravity(0,0)
 immediate=create(0);count=immediate:getParticleCount();assert(count>10)
 gradual=create(1);paused=create(1);paused:setPaused(true)
 mortal=create(2,.02);doomed=create(2)
 assert(gradual:getParticleCount()==0 and paused:getParticleCount()==0)
 world:destroyFluid(doomed);doomed=nil
 for _,value in ipairs({-1,math.huge,0/0,'invalid'}) do
  assert(not pcall(function() create(value) end),'Invalid riseTime accepted')
 end
 initialized=true
end
function onLoop(delta)
 assert(initialized,'Initialization failed')
 elapsed=elapsed+delta
 if elapsed>.45 and not checkpoint then
  local n=gradual:getParticleCount()
  assert(n>0 and n<count,'Emission not gradual')
  assert(paused:getParticleCount()==0,'Paused system emitted')
  print('RISE_PARTIAL',n,count)
  paused:setPaused(false);checkpoint=true
 end
 if elapsed>1.2 and not complete then
  assert(gradual:getParticleCount()==count,'Emission total differs from immediate')
  assert(immediate:getParticleCount()==count,'Legacy count changed')
  -- add remains immediate, even for a fluid whose initial emission was gradual.
  local n=gradual:add(shapeDef,{x=30,y=0})
  assert(n==count and gradual:getParticleCount()==2*count,'add changed behavior')
  pausedCount=paused:getParticleCount()
  world:pause();resumed=elapsed;complete=true
 end
 if resumed and elapsed-resumed>.3 then
  assert(paused:getParticleCount()==pausedCount,'World pause did not suspend emission')
  world:start();resumed=nil
 end
 if elapsed>3 then
  assert(paused:getParticleCount()==count,'Paused emission did not resume to completion')
  assert(mortal:getParticleCount()==0,'Lifetime did not expire')
  -- Re-emitting after an empty group was destroyed must not dereference a stale group.
  assert(mortal:add(shapeDef,{x=0,y=0})==count)
  world:destroyFluid(gradual)
  print('FLUID_RISE_SMOKE_OK',count)
  mbm.quit()
 end
end
