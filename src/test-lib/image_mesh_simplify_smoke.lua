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

package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local IO=require 'image_mesh_io'
local api={}; assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local started,task,failed
local function awaitTask()
    while api.state.meshTask do coroutine.yield() end
end
local function checkSaved(path,count)
    local asset=meshDebug:new(); assert(asset:load(path)); assert(#asset:getIndex(1,1)/3==count)
end
local function test()
    local e=api.state
    local pixels={}; for y=0,63 do for x=0,63 do
        local v=80+math.floor(x*2+y/2)
        pixels[#pixels+1]=v; pixels[#pixels+1]=v; pixels[#pixels+1]=v
    end end
    local path='/tmp/ime-simplify-source.png'
    assert(mbm.createTexture(pixels,64,64,3,'ime-simplify-source',path)); assert(api.openImage(path))
    assert(api.action(function(p) Model.add(p,'rectangle',0,0,64,64) end))
    api.select(1); api.setEditMode(false); api.rebuild(); awaitTask()
    local source=e.report.triangles
    local orbit=Model.copy(e.orbit)
    e.values.simplify=true; e.values.simplifyRatio=0.5; e.values.simplifyDetails=true; e.values.simplifyBoundary=0.1
    api.applyProperties(); api.rebuild()
    assert(e.meshTask,'simplification did not yield to the UI'); awaitTask()
    assert(e.preview and e.report.simplification,e.status)
    local count=e.report.triangles
    assert(count<source and e.report.sourceTriangles==source)
    assert(e.report.simplification.degenerateTriangleCount==0 and e.report.simplification.nonManifoldEdgeCount==0)
    for k,v in pairs(orbit) do assert(e.orbit[k]==v,'simplification moved camera') end
    checkSaved(e.previewPath,count)
    api.exportOne('/tmp/ime-simplify-selected.msh'); awaitTask(); checkSaved('/tmp/ime-simplify-selected.msh',count)
    os.remove('/tmp/ime-simplify-batch/'..IO.exportName(e.project.regions[1]))
    api.beginBatch('/tmp/ime-simplify-batch')
    while e.batch do coroutine.yield() end
    checkSaved('/tmp/ime-simplify-batch/'..IO.exportName(e.project.regions[1]),count)
    api.saveProject('/tmp/ime-simplify.imesh'); api.openProject('/tmp/ime-simplify.imesh')
    api.select(1); assert(e.values.simplify and e.values.simplifyRatio==0.5 and e.values.simplifyBoundary==0.1)
    api.rebuild(); awaitTask(); assert(e.report.triangles==count)
    api.setEditMode(true); api.updateStatistics(); awaitTask(); assert(e.report.triangles==count)
    e.values.simplify=false; api.applyProperties(); api.updateStatistics(); awaitTask(); assert(e.report.triangles==source)
    api.undo(false); api.updateStatistics(); awaitTask(); assert(e.report.triangles==count)
    -- Impossible target must fail visibly and never write the unsimplified fallback.
    e.values.simplifyRatio=0.001; e.values.simplifyBoundary=0; api.applyProperties()
    api.setEditMode(false); api.rebuild(); awaitTask()
    assert(not e.preview and e.generationFailure,'missing simplification failure')
    os.remove('/tmp/ime-simplify-failed.msh')
    local ok=pcall(api.exportOne,'/tmp/ime-simplify-failed.msh'); awaitTask()
    assert(not IO.exists('/tmp/ime-simplify-failed.msh'),'failure exported unsimplified mesh')
    api.undo(false); api.rebuild(); awaitTask(); assert(e.preview and e.report.triangles==count)
    local builds=e.builds
    for _=1,10 do coroutine.yield() end
    assert(e.builds==builds and not e.meshTask,'idle simplification repeated')
    print(string.format('IMAGE MESH SIMPLIFY PREVIEW / EXPORT / BATCH / SAVE / UNDO / FAILURE / IDLE OK: %d -> %d',source,count))
end
function onInitScene()
    init(); started=mbm.getTimeRun(); task=coroutine.create(test)
    local header=tImGui.CollapsingHeader
    tImGui.CollapsingHeader=function(label,...)
        local open=header(label,...)
        return label==tLang.L('simplify_geometry') or open
    end
end
function onLoop(delta)
    loop(delta)
    if coroutine.status(task)~='dead' and not failed then
        local ok,err=coroutine.resume(task)
        if not ok then failed=true; print('IMAGE MESH SIMPLIFY FAIL: '..tostring(err)); mbm.quit() end
    elseif not api.state.meshTask then mbm.quit() end
    if mbm.getTimeRun()-started>30 then print('IMAGE MESH SIMPLIFY FAIL: timeout'); mbm.quit() end
end
function onEndScene() finish() end
