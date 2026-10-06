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
]]

-- Run through mini-mbm --scene with --disable_select_monitor --nosplash.
-- Require SHAPE DOUBLE SIDED PASS in the log; runtime errors may still exit zero.
local cases = {
    {"rectangle", 2, 4}, {"quad", 2, 4}, {"square", 2, 4}, {"rect", 2, 4},
    {"rectangle", 5, 8}, -- Odd counts round up to six front triangles.
    {"circle", 18, 19}, {"triangle", 1, 3}, {"triangle", 4, 6},
    {"points", 1, 3}, {"points-reversed", 1, 3},
}
local rt, object, started
local caseIndex, dynamic, back, ticks = 1, false, false, 0
local callbacks, frontPixels = 0, 0
local attached = false
local temporaryName = os.tmpname()
os.remove(temporaryName)
local path = temporaryName .. ".png"

local function fail(message)
    print("SHAPE DOUBLE SIDED FAIL: " .. tostring(message))
    os.remove(path)
    mbm.quit()
end

local function createCase()
    rt:clear()
    object = shape:new("3d")
    local c = cases[caseIndex]
    local nickname = "double-sided-" .. caseIndex .. "-" .. tostring(dynamic)
    if c[1] == "points" then
        assert(object:create("triangle", {-1,-1, 0,1, 1,-1}, dynamic, nickname))
    elseif c[1] == "points-reversed" then
        assert(object:create("triangle", {1,-1, 0,1, -1,-1}, dynamic, nickname))
    else
        assert(object:create(c[1], 2, 2, c[2], dynamic, nickname))
    end
    object:setColor(1, 1, 1, 1)
    object.alwaysRender = true
    callbacks = 0
    if dynamic then
        object:onRender(function(_, vertices, uv, indices)
            local ok, err = pcall(function()
                local triangles = c[2]
                if c[1] == "rectangle" and triangles % 2 == 1 then triangles = triangles + 1 end
                assert(#vertices == c[3] and #uv == c[3], "vertex/UV count changed")
                local frontSize = triangles * 3
                assert(#indices == frontSize * 2, "missing back indices")
                for i = 1, frontSize, 3 do
                    assert(indices[i] == indices[frontSize+i])
                    assert(indices[i+1] == indices[frontSize+i+2])
                    assert(indices[i+2] == indices[frontSize+i+1])
                end
                -- Deform once and retain the same geometry for front/back readback.
                if callbacks == 0 then
                    for _, v in ipairs(vertices) do v.x = v.x * 0.8 end
                end
                callbacks = callbacks + 1
            end)
            if not ok then fail(err) end
            return vertices, uv
        end)
    end
    -- First exercise the dynamic callback in the main pass; off-screen rendering
    -- intentionally suppresses callbacks to avoid animating objects twice.
    attached = not dynamic
    if attached then rt:add(object) end
end

function onInitScene()
    started = mbm.getTimeRun()
    local ok, err = pcall(function()
        mbm.setLightEnabled("3d", false)
        mbm.getCamera("3d"):setPos(0, 0, -4)
        mbm.getCamera("3d"):setFocus(0, 0, 0)
        rt = render2texture:new("2ds")
        assert(rt:create(96, 96, true))
        rt:setColor(0, 0, 0, 1)
        rt:getCamera("3d"):setPos(0, 0, -4)
        rt:getCamera("3d"):setFocus(0, 0, 0)
        createCase()
    end)
    if not ok then fail(err) end
end

function onLoop(delta)
    if mbm.getTimeRun() - started > 20 then fail("timeout"); return end
    if not attached then
        if callbacks > 0 then rt:add(object); attached = true; ticks = 0 end
        return
    end
    ticks = ticks + delta
    if ticks < 0.05 then return end
    ticks = 0
    local ok, err = pcall(function()
        assert(rt:save(path))
        local pixels = assert(mbm.readImagePixels(path))
        local visible = 0
        for i = 1, #pixels, 4 do
            if pixels:byte(i) > 200 then visible = visible + 1 end
        end
        assert(visible > 100, "invisible " .. cases[caseIndex][1] .. " back=" .. tostring(back))
        if dynamic then assert(callbacks > 0, "dynamic callback did not run") end
        if not back then
            frontPixels = visible
            back = true
            object:setAngle(0, math.pi, 0)
            return
        end
        assert(math.abs(visible - frontPixels) < 10, "front/back coverage differs")
        back = false
        if not dynamic then
            dynamic = true
        else
            dynamic = false
            caseIndex = caseIndex + 1
        end
        if caseIndex > #cases then
            os.remove(path)
            print("SHAPE DOUBLE SIDED PASS (40 front/back readbacks, static/dynamic, aliases, tessellation, vertex edits)")
            mbm.quit()
            return
        end
        createCase()
    end)
    if not ok then fail(err) end
end
