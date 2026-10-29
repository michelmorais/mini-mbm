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

local function vector(x, y, z)
    return {x = x, y = y, z = z}
end

local function run()
    local author = meshDebug:new()
    author:setType('mesh')
    author:setModeDraw('TRIANGLES')
    author:addFrame(3)
    author:addSubSet(1)
    assert(author:addVertex(1, 1, {
        {x = 0, y = 0, z = 0, nx = 0, ny = 0, nz = 1, u = 0, v = 0},
        {x = 1, y = 0, z = 0, nx = 0, ny = 0, nz = 1, u = 1, v = 0},
        {x = 0, y = 1, z = 0, nx = 0, ny = 0, nz = 1, u = 0, v = 1},
    }))
    assert(author:addIndex(1, 1, {1, 2, 3}))
    assert(author:setTexture(1, 1, '#FFFFFFFF'))

    local base = {vector(0.1, 0, 0), vector(0.2, 0, 0), vector(0.3, 0, 0)}
    local jawOpen = author:addFacialTarget('jawOpen', 'ARKit', 0, base)
    assert(jawOpen == 1)
    assert(author:getTotalFacialTargets() == 1)
    local export = author:getFacialExportData()
    assert(export.vertexCount == 3 and #export.targets == 1)
    assert(math.abs(export.targets[1].positionDeltas[3].x - 0.3) < 1e-6)
    assert(export.targets[1].normalDeltas[1].x == 0)

    local updated = {vector(0.4, 0, 0), vector(0.5, 0, 0), vector(0.6, 0, 0)}
    assert(author:updateFacialTarget(1, 'jawOpen', 'ARKit', 0.25, updated))
    export = author:getFacialExportData()
    assert(math.abs(export.targets[1].defaultWeight - 0.25) < 1e-6)
    assert(math.abs(export.targets[1].positionDeltas[1].x - 0.4) < 1e-6)

    local blink = author:addFacialTarget('eyeBlinkLeft', 'ARKit', 0, {
        vector(0, 0.1, 0), vector(0, 0.2, 0), vector(0, 0.3, 0),
    })
    assert(blink == 2)
    assert(author:addFacialClip('test', 1, false, 0, {
        {time = 0, weights = {0, 0}},
        {time = 1, weights = {1, 0.5}},
    }) == 1)
    assert(author:removeFacialTarget(1))
    assert(author:getTotalFacialTargets() == 1)
    export = author:getFacialExportData()
    assert(#export.clips == 1 and #export.clips[1].keys[1].weights == 1)
    assert(not pcall(function() author:removeFacialTarget(1) end),
        'last target removal with clips must be rejected')
    assert(author:removeFacialClip(1))
    assert(author:removeFacialTarget(1))
    assert(author:getTotalFacialTargets() == 0)

    local finalTarget = author:addFacialTarget('jawOpen', 'ARKit', 0, base)
    assert(finalTarget == 1)
    local path = '/tmp/facial-target-authoring-smoke.msh'
    assert(author:save(path, false, false, true))
    local loaded = meshDebug:new()
    assert(loaded:load(path))
    assert(loaded:getTotalFacialTargets() == 1)
    local loadedExport = loaded:getFacialExportData()
    assert(loadedExport.targets[1].name == 'jawOpen')
    assert(math.abs(loadedExport.targets[1].positionDeltas[2].x - 0.2) < 1e-6)
    print('FACIAL TARGET AUTHORING API / CLIP REMAP / MSH ROUNDTRIP OK')
end

function onInitScene()
    local ok, errorMessage = pcall(run)
    if not ok then
        print('FACIAL TARGET AUTHORING SMOKE FAIL '..tostring(errorMessage))
    end
    mbm.quit()
end
