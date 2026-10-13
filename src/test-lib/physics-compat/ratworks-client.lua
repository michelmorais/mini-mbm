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
-- Runs the real external game without modifying its sources or level configuration.
local root = assert(os.getenv('MBM_RATWORKS_ROOT'), 'Set MBM_RATWORKS_ROOT')
local duration = tonumber(os.getenv('MBM_PHYSICS_SECONDS')) or 60
local moduleDir = assert(os.getenv('MBM_PHYSICS_MODULE_DIR'))
local suffix = package.config:sub(1, 1) == '\\' and '.dll' or '.so'
package.cpath = moduleDir .. '/?' .. suffix
assert(loadfile(root .. '/main.lua'))(root)
local init, update = onInitScene, onLoop
local game = require 'game'
local Rat = require 'rat'
local release = Rat.releaseFluid
local fluids, frames, peak, expired = {}, 0, 0, 0
local started, cpuStarted
local updateTimes = {}
function Rat:releaseFluid()
    local existed = self.fluid ~= nil
    release(self)
    if not existed then
        assert(self.fluid and self.fluid:getParticleCount() == 0, 'Expected gradual emission')
        fluids[#fluids + 1] = {fluid=self.fluid, rat=self, peak=0, survived=false}
    end
end
local function checked(fn, ...)
    local ok, err = pcall(fn, ...)
    if not ok then print('RATWORKS_FAILED', tostring(err)); mbm.quit() end
end
function onInitScene()
    mbm.onErrorStop(true)
    started, cpuStarted = mbm.getTimeRun(), os.clock()
    checked(init)
end
function onLoop(delta)
    checked(function()
        local before = os.clock()
        update(delta)
        updateTimes[#updateTimes + 1] = (os.clock() - before) * 1000
        frames = frames + 1
        local total = 0
        expired = 0
        for _, item in ipairs(fluids) do
            local count = item.fluid:getParticleCount()
            item.peak = math.max(item.peak, count)
            total = total + count
            if not item.rat.model and count > 0 then item.survived = true end
            if item.peak > 0 and count == 0 then expired = expired + 1 end
        end
        peak = math.max(peak, total)
        if mbm.getTimeRun() - started < duration then return end
        assert(game.ready, 'Game not ready')
        if os.getenv('MBM_PHYSICS_BACKEND') ~= 'legacy' then
            assert(game.physics.world:supportsParticles(), 'Particle backend not ready')
        end
        local expected = 0
        for _, wave in ipairs(game.swarm.config.waves) do expected = expected + wave.count end
        assert(game.swarm.spawned == expected and game.swarm.consumed == expected,
            'Not all configured rats completed the machine cycle')
        assert(#fluids == expected and expired == expected, 'Emission/lifetime incomplete')
        for _, item in ipairs(fluids) do assert(item.survived, 'Fluid did not outlive rat') end
        table.sort(updateTimes)
        local function percentile(p) return updateTimes[math.max(1, math.ceil(#updateTimes*p))] end
        print(string.format('RATWORKS_RESULT frames=%d spawned=%d consumed=%d fluids=%d peak=%d expired=%d cpu_s=%.6f update_p50_ms=%.6f update_p95_ms=%.6f update_p99_ms=%.6f',
            frames, game.swarm.spawned, game.swarm.consumed, #fluids, peak, expired,
            os.clock()-cpuStarted, percentile(.5), percentile(.95), percentile(.99)))
        print('RATWORKS_OK')
        mbm.quit()
    end)
end
