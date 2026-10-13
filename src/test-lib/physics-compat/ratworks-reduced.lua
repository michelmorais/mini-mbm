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

--]]
-- Asset-free Ratworks emission/ownership cycle; default duration is ten minutes.
local moduleDir = assert(os.getenv('MBM_PHYSICS_MODULE_DIR'))
package.cpath = moduleDir .. '/?.so;' .. package.cpath
require 'box2dLiquidFun'
local duration = tonumber(os.getenv('MBM_PHYSICS_SECONDS')) or 600
local world, floor, blocker, fluid, emitter, textureName
local started, cycleStart, phase, pausedCount, cycles, peak = nil, nil, 0, 0, 0, 0
local circle = {type='circle', ray=5}
local function body(x, y, w, h, category)
    local s = shape:new('3d', x, y, 0)
    assert(s:create('rectangle', w, h))
    assert(world:addStaticBody(s))
    world:setFilter(s, {categoryBits=category, maskBits=65535})
    return s
end
local function beginCycle()
    cycleStart, phase = 0, 0
    emitter = body(0, 35, 12, 12, 2)
    fluid = world:createFluid(circle, {is3d=true, radius=.03, radiusScale=2.1,
        lifetime=5, riseTime=1.5, position={x=0,y=40,z=0}, texture=textureName,
        flags='water', fixtureMaskBits=65533})
    assert(fluid:getParticleCount() == 0)
end
local function checked(fn)
    local ok, err = pcall(fn)
    if not ok then print('RATWORKS_REDUCED_FAILED', tostring(err)); mbm.quit() end
end
function onInitScene()
    mbm.onErrorStop(true)
    checked(function()
        world = box2dLiquidFun:new()
        assert(world:supportsParticles())
        world:setGravity(0,-10)
        local camera = mbm.getCamera('3d')
        camera:setPos(0,30,180); camera:setFocus(0,30,0)
        mbm.setLightEnabled('3d',false)
        textureName = '#40FF40FF'
        floor = body(0,-5,200,10,1)
        blocker = body(0,20,180,5,2)
        started = mbm.getTimeRun()
        beginCycle()
    end)
end
function onLoop(delta)
    checked(function()
        cycleStart = cycleStart + delta
        local age = cycleStart
        local count = fluid:getParticleCount()
        peak = math.max(peak,count)
        if phase == 0 and age > .3 then
            assert(count > 0, 'No gradual emission')
            world:destroyBody(emitter); emitter:destroy(); emitter=nil
            fluid:setPaused(true); pausedCount=count; phase=1
        elseif phase == 1 and age > .6 then
            assert(count == pausedCount, 'Paused fluid emitted/expired')
            fluid:setPaused(false); world:pause(); phase=2
        elseif phase == 2 and age > .9 then
            assert(count == pausedCount, 'Paused world emitted/expired')
            world:start(); phase=3
        elseif phase == 3 and age > 3 then
            assert(count > pausedCount, 'Emission did not resume after rat removal')
            local bounds=fluid:computeAABB()
            print('RATWORKS_REDUCED_BOUNDS',bounds.lowerBound.y,bounds.upperBound.y,count)
            assert(bounds.lowerBound.y > -2 and bounds.upperBound.y < 18,
                'Fluid did not bypass rat category and settle on floor')
            phase=4
        elseif phase == 4 and age > 7.5 then
            assert(count == 0, 'Lifetime did not expire')
            assert(fluid:add(circle,{x=0,y=40}) > 0, 'Re-emission failed')
            phase=5
        elseif phase == 5 and age > 8 then
            assert(count > 0, 'Re-emission lost')
            world:destroyFluid(fluid)
            assert(not pcall(function() fluid:getParticleCount() end), 'Stale fluid accepted')
            cycles=cycles+1
            print('RATWORKS_REDUCED_CYCLE',cycles,peak)
            if mbm.getTimeRun()-started >= duration then
                print('RATWORKS_REDUCED_OK',cycles,peak)
                mbm.quit()
            else beginCycle() end
        end
    end)
end
