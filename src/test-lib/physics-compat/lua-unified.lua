--[[
/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
|-----------------------------------------------------------------------------------------------------------------------*/

#if PHYSICS_PARTICLES && !PHYSICS_UNIFIED
--]]
-- MBM_PHYSICS_PROFILE=off|on, MBM_PHYSICS_ORDER=legacy|modern.
local moduleDir = os.getenv("MBM_PHYSICS_MODULE_DIR")
local moduleSuffix = package.config:sub(1,1) == "\\" and ".dll" or ".so"
if moduleDir then package.cpath = moduleDir .. "/?" .. moduleSuffix .. ";" .. package.cpath end
local enabled = os.getenv("MBM_PHYSICS_PROFILE") ~= "off"
local legacyFirst = os.getenv("MBM_PHYSICS_ORDER") == "legacy"
local world, legacy, bodyA, bodyB, joint, fluid, doomed, shader, retained
local elapsed, stage = 0, 0
local function near(a,b) assert(math.abs(a-b)<0.001, tostring(a).." ~= "..tostring(b)) end
local function fails(fn, message)
    local ok, err = pcall(fn)
    assert(not ok and tostring(err):find(message,1,true), tostring(err))
end
local function init()
    if enabled and legacyFirst then require "box2dLiquidFun" end
    require "box2d"
    local factory = box2d
    if enabled then
        require "box2dLiquidFun"
        assert(factory == box2d)
        assert(box2dLiquidFun:getVersion() == box2d:getVersion())
    else
        fails(function() require "box2dLiquidFun" end, "USE_BOX2D_PARTICLES")
    end
    for _,name in ipairs(enabled and {"box2d","box2dLiquidFun"} or {"box2d"}) do
        local path=assert(package.searchpath(name,package.cpath))
        assert(package.loadlib(path,"luaopen_lib"..name))()
        assert(factory==box2d)
    end
    assert(box2d:supportsParticles() == enabled)
    world = box2d:new(0,0)
    assert(world:supportsParticles() == enabled)
    bodyA,bodyB=shape:new("2dw",0,0),shape:new("2dw",50,0)
    assert(bodyA:create("rectangle",10,10) and bodyB:create("rectangle",10,10))
    assert(world:addStaticBody(bodyA) and world:addDynamicBody(bodyB,1,.3,0))
    local index=world:joint(bodyA,bodyB,{name="distance",frequencyHz=3,dampingRatio=.4})
    -- A handle may originate in a short-lived coroutine, including engine callbacks.
    joint=coroutine.wrap(function() return world:getJoint(bodyA,index) end)()
    collectgarbage("collect")
    near(joint:getFrequencyHz(),3);near(joint:getDampingRatio(),.4)
    local stiffness=joint:getStiffness()
    joint:setFrequencyHz(6);near(joint:getStiffness(),stiffness*4)
    joint:setDampingRatio(.2);near(joint:getDampingRatio(),.2)
    joint:setFrequencyHz(0);near(joint:getDampingRatio(),.2)
    joint:setFrequencyHz(3);near(joint:getFrequencyHz(),3)
    if not enabled then assert(world.createFluid==nil);return end
    legacy=box2dLiquidFun:new(0,0)
    local a,b=shape:new("2dw",0,100),shape:new("2dw",50,100)
    assert(a:create("rectangle",10,10) and b:create("rectangle",10,10))
    assert(legacy:addStaticBody(a) and legacy:addDynamicBody(b))
    retained={a,b}
    local rope=legacy:getJoint(a,legacy:joint(a,b,{name="rope",maxLength=7}))
    near(rope:getLength(),7);rope:setLength(4);near(rope:getLength(),4)
    for _,entry in ipairs({{"distance",0,0},{"wheel",2,.7},{"weld",0,0}}) do
        local x,y=shape:new("2dw",100,100),shape:new("2dw",130,100)
        assert(x:create("rectangle",10,10) and y:create("rectangle",10,10))
        assert(legacy:addStaticBody(x) and legacy:addDynamicBody(y))
        retained[#retained+1]=x;retained[#retained+1]=y
        local spring=legacy:getJoint(x,legacy:joint(x,y,{name=entry[1]}))
        near(spring:getFrequencyHz(),entry[2]);near(spring:getDampingRatio(),entry[3])
        spring:setFrequencyHz(4);spring:setDampingRatio(.3)
        near(spring:getFrequencyHz(),4);near(spring:getDampingRatio(),.3)
    end
    local def={type="circle",ray=5}
    fluid=world:createFluid(def,{radius=.1,riseTime=.2,lifetime=.5,texture="#FF00FFFF"})
    assert(fluid:getParticleCount()==0)
    doomed=coroutine.wrap(function()
        return legacy:createFluid(def,{radius=.1,segmented=true,texture="#FFFFFFFF"})
    end)()
    collectgarbage("collect")
    shader=doomed:getShader()
    local hits=0
    doomed:queryShapeAABB({type="circle",ray=20}, function(self,index)
        assert(self==doomed and index>=1);hits=hits+1;return true
    end)
    assert(hits==doomed:getParticleCount())
    for _=1,50 do
        local n=0
        doomed:queryAABB(-20,-20,20,20,function() n=n+1;return true end)
        assert(n==hits)
    end
    fails(function()world:destroyFluid(doomed)end,"different world")
    legacy:destroyFluid(doomed)
    fails(function()doomed:getParticleCount()end,"pending destruction")
    fails(function()shader:getPS()end,"pending destruction")
    -- A collected world invalidates retained fluid tables as well.
    local temporary=box2d:new(0,0)
    local orphan=temporary:createFluid(def,{radius=.1})
    temporary=nil;collectgarbage("collect")
    fails(function()orphan:getParticleCount()end,"destroyed")
end
local failed=false
local function checked(fn,...)
    local ok,err=pcall(fn,...)
    if not ok then failed=true;print("PHYSICS_LUA_FAILED",tostring(err));mbm.quit() end
end
function onInitScene() checked(init) end
function onLoop(delta)
    if failed then return end
    checked(function()
        elapsed=elapsed+delta
        if elapsed>.3 and stage==0 then
            if enabled then
                assert(fluid:getParticleCount()>0)
                fails(function()doomed:getParticleCount()end,"destroyed")
                fails(function()shader:getPS()end,"destroyed")
            end
            world:destroyBody(bodyB)
            stage=1
        end
        if elapsed>.9 and stage==1 then
            fails(function()joint:getLength()end,"destroyed")
            if enabled then
                assert(fluid:getParticleCount()==0)
                assert(fluid:add({type="circle",ray=5},{x=0,y=0})>0)
                assert(fluid:getParticleCount()>0)
                world:destroyFluid(fluid)
            end
            stage=2
        end
        if elapsed>1.1 then
            if enabled then fails(function()fluid:getParticleCount()end,"destroyed") end
            print("PHYSICS_LUA_OK",enabled and "ON" or "OFF",legacyFirst and "legacy" or "modern")
            mbm.quit()
        end
    end)
end
