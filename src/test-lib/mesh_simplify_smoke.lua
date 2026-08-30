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

local function totals(asset, frame)
    frame = frame or 1
    local vertices, triangles = 0, 0
    for subset = 1, asset:getTotalSubset(frame) do
        vertices = vertices + asset:getTotalVertex(frame, subset)
        triangles = triangles + math.floor(asset:getTotalIndex(frame, subset) / 3)
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
    assert(report.skinWeightAware == false)
    assert(report.poseSampledError == false)
    assert(report.sampledPoseCount == 0 and report.sampledClipCount == 0)
    assert(source:check())
    assert(source:save('/tmp/mbm_simplified_crate.msh', false, false, true))

    local restored = meshDebug:new()
    assert(restored:load('/tmp/mbm_simplified_crate.msh'))
    assert(restored:check())
    local restoredVertices, restoredTriangles = totals(restored)
    assert(restoredVertices == report.resultVertexCount)
    assert(restoredTriangles == report.resultTriangleCount)

    local multiFrame = meshDebug:new()
    assert(multiFrame:load('Crate.msh'))
    assert(multiFrame:copyFrameFrom(multiFrame, 1) == 2)
    local frame1Vertices, frame1Triangles = totals(multiFrame, 1)
    local frame2Vertices, frame2Triangles = totals(multiFrame, 2)
    local frameReport, frameError = multiFrame:simplify(0.5, nil, 2)
    assert(frameReport, frameError)
    local frame1AfterVertices, frame1AfterTriangles = totals(multiFrame, 1)
    local frame2AfterVertices, frame2AfterTriangles = totals(multiFrame, 2)
    assert(frame1AfterVertices == frame1Vertices and frame1AfterTriangles == frame1Triangles)
    assert(frameReport.sourceVertexCount == frame2Vertices)
    assert(frameReport.sourceTriangleCount == frame2Triangles)
    assert(frame2AfterVertices == frameReport.resultVertexCount)
    assert(frame2AfterTriangles == frameReport.resultTriangleCount)
    assert(multiFrame:check())
    assert(multiFrame:save('/tmp/mbm_simplified_selected_frame.msh', false, false, true))
    local multiFrameRestored = meshDebug:new()
    assert(multiFrameRestored:load('/tmp/mbm_simplified_selected_frame.msh'))
    assert(multiFrameRestored:getTotalFrame() == 2)
    assert(totals(multiFrameRestored, 1) == frame1Vertices)
    assert(totals(multiFrameRestored, 2) == frame2AfterVertices)

    local skeletal = meshDebug:new()
    assert(skeletal:load('Lorekeeper-walk.msh'))
    local skeletalVertices, skeletalTriangles = totals(skeletal)
    local skeletalReport, skeletalError = skeletal:simplify(0.8)
    assert(skeletalReport, skeletalError)
    assert(skeletalReport.skinWeightAware == true)
    assert(skeletalReport.poseSampledError == true)
    assert(skeletalReport.sampledPoseCount > 0)
    assert(skeletalReport.sampledClipCount > 0)
    assert(skeletalReport.maximumPoseError >= 0)
    assert(skeletalReport.resultTriangleCount < skeletalTriangles)
    assert(skeletal:check())
    assert(skeletal:save('/tmp/mbm_simplified_lorekeeper.msh', false, false, true))

    local skeletalRestored = meshDebug:new()
    assert(skeletalRestored:load('/tmp/mbm_simplified_lorekeeper.msh'))
    assert(skeletalRestored:check())
    local skeletalRestoredVertices, skeletalRestoredTriangles = totals(skeletalRestored)
    assert(skeletalRestoredVertices == skeletalReport.resultVertexCount)
    assert(skeletalRestoredTriangles == skeletalReport.resultTriangleCount)

    print('info', 'green', string.format(
        'SIMPLIFY SMOKETEST OK static=%d/%d->%d/%d frame2=%d/%d->%d/%d skeletal=%d/%d->%d/%d error=%.6g',
        beforeVertices, beforeTriangles, restoredVertices, restoredTriangles,
        frame2Vertices, frame2Triangles, frame2AfterVertices, frame2AfterTriangles,
        skeletalVertices, skeletalTriangles, skeletalRestoredVertices, skeletalRestoredTriangles,
        report.maximumGeometricError))
end

function onLoop(delta)
    if started and mbm.getTimeRun() - started >= 1 then mbm.quit() end
end
