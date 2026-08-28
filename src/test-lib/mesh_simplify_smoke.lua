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

local started = nil

local function totals(asset)
    local vertices, triangles = 0, 0
    for subset = 1, asset:getTotalSubset(1) do
        vertices = vertices + asset:getTotalVertex(1, subset)
        triangles = triangles + math.floor(asset:getTotalIndex(1, subset) / 3)
    end
    return vertices, triangles
end

function onInitScene()
    started = mbm.getTimeRun()
    local source = meshDebug:new()
    assert(source:load('Crate.msh'))
    local beforeVertices, beforeTriangles = totals(source)
    local report, simplifyError = source:simplify(0.5)
    assert(report, simplifyError)
    assert(report.sourceTriangleCount == beforeTriangles)
    assert(report.resultTriangleCount < report.sourceTriangleCount)
    assert(source:check())
    assert(source:save('/tmp/mbm_simplified_crate.msh', false, false, true))

    local restored = meshDebug:new()
    assert(restored:load('/tmp/mbm_simplified_crate.msh'))
    assert(restored:check())
    local restoredVertices, restoredTriangles = totals(restored)
    assert(restoredVertices == report.resultVertexCount)
    assert(restoredTriangles == report.resultTriangleCount)

    local skeletal = meshDebug:new()
    assert(skeletal:load('Lorekeeper-walk.msh'))
    local skeletalVertices, skeletalTriangles = totals(skeletal)
    local rejected, rejection = skeletal:simplify(0.5)
    assert(rejected == nil and rejection:find('skeletal'))
    local afterVertices, afterTriangles = totals(skeletal)
    assert(afterVertices == skeletalVertices and afterTriangles == skeletalTriangles)

    print('info', 'green', string.format(
        'SIMPLIFY SMOKETEST OK source=%d/%d result=%d/%d error=%.6g',
        beforeVertices, beforeTriangles, restoredVertices, restoredTriangles,
        report.maximumGeometricError))
end

function onLoop(delta)
    if started and mbm.getTimeRun() - started >= 1 then mbm.quit() end
end
