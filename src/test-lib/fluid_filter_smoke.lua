-- From engine root: mini-mbm --scene src/test-lib/fluid_filter_smoke.lua --disable_select_monitor
require 'box2dLiquidFun'
local world, bodies, fluids, dynamic
local elapsed, added, initialized = 0, false, false
local function body(x,y,w,h,category,kind)
    local s=shape:new('2dw',x,y)
    assert(s:create('rectangle',w,h))
    if kind=='dynamic' then assert(world:addDynamicBody(s,1,.3,0))
    else assert(world:addStaticBody(s)) end
    world:setFilter(s,{categoryBits=category,maskBits=65535,groupIndex=category==2 and -1 or 0})
    bodies[#bodies+1]=s
    return s
end
function onInitScene()
    mbm.onErrorStop(true)
    world=box2dLiquidFun:new()
    world:setGravity(0,-10)
    bodies,fluids={},{}
    body(0,-5,1000,10,1)
    for i,mask in ipairs({65535,65533,0}) do
        local x=(i-2)*150
        body(x,30,100,10,2)
        local opts={radius=.3,position={x=x,y=70},texture='#FFFF00FF',flags='water'}
        -- First lane verifies the unmodified default, including normal fixture contacts.
        if i~=1 then opts.fixtureMaskBits=mask end
        fluids[i]=world:createFluid({type='circle',ray=5},opts)
        assert(fluids[i]:getParticleCount()>0)
    end
    dynamic=body(280,70,12,12,2,'dynamic')
    for _,mask in ipairs({-1,65536,1.5,'invalid'}) do
        local ok=pcall(function()
            world:createFluid({type='circle',ray=5},{fixtureMaskBits=mask})
        end)
        assert(not ok,'Invalid mask accepted: '..tostring(mask))
    end
    initialized=true
end
function onLoop(delta)
    assert(initialized, 'Initialization failed')
    elapsed=elapsed+delta
    if elapsed>3 and not added then
        local a,b,c=fluids[1]:computeAABB(),fluids[2]:computeAABB(),fluids[3]:computeAABB()
        print('FILTER_BOUNDS',a.lowerBound.y,b.lowerBound.y,c.upperBound.y,dynamic.y)
        assert(a.lowerBound.y>25,'Default fluid did not collide with category 2')
        assert(b.upperBound.y<25 and b.lowerBound.y>-10,'Filtered fluid did not reach/stay on floor')
        assert(c.upperBound.y < -50,'Zero mask did not bypass the floor')
        assert(dynamic.y>5 and dynamic.y<8,'Body-body ground collision regressed')
        assert(fluids[2]:add({type='circle',ray=5},{x=0,y=70})>0)
        added=true
    end
    if elapsed>6 then
        local b=fluids[2]:computeAABB()
        assert(b.upperBound.y<25 and b.lowerBound.y>-10,'Added particles lost the filter')
        world:destroyFluid(fluids[1])
        world:destroyFluid(fluids[2])
        world:destroyFluid(fluids[3])
        print('FLUID_FILTER_SMOKE_OK')
        mbm.quit()
    end
end
