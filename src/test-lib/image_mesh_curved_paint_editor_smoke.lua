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
local Canvas=require 'image_mesh_canvas'
local IO=require 'image_mesh_io'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local started,baseline
local task,activated
local function run()
    init()
    local pixels={};for i=1,65*65*3 do pixels[i]=128 end
    local path='/tmp/ime_curved_paint_editor.png'
    assert(mbm.createTexture(pixels,65,65,3,'ime_curved_paint_editor',path))
    assert(api.openImage(path))
    api.action(function(p) p.defaults.heightSource='curved';p.defaults.curvedPainting=false;p.defaults.curvedRadius=15;p.defaults.curvedSimplify=true;p.defaults.columns=16;p.defaults.rows=16;p.defaults.lockBorder=false;Model.add(p,'ellipse',0,0,65,65) end)
    api.select(1)
    local E=api.state
    while not activated do coroutine.yield() end
    assert(E.values.curvedPainting and E.values.heightSource=='curved')
    E.heightView=2;api.paint.state(E).enabled=true
    Canvas.sync(E)
    local function position(x,y) local t=Canvas.transform(E);return t.x+x*t.scale,t.y+y*t.scaleY end
    local x,y=position(25,32)
    local hx=#E.history.past
    api.paintInput('down',x,y)
    x,y=position(40,32);api.paintInput('move',x,y)
    assert(not Model.region(E.project,1).heightEdits,'stroke mutated project before release')
    api.paintInput('up',x,y)
    local region=Model.region(E.project,1)
    assert(#region.heightEdits>1 and #E.history.past==hx+1,'one undo entry per stroke')
    local saved=Model.copy(region.heightEdits)
    api.undo(false);assert(not Model.region(E.project,1).heightEdits)
    api.undo(true);assert(#Model.region(E.project,1).heightEdits==#saved)
    x,y=position(32,32);api.paintInput('down',x,y);api.paint.cancel(E)
    assert(#Model.region(E.project,1).heightEdits==#saved,'cancel changed painting')
    x,y=position(1,1);assert(not api.paintInput('down',x,y),'paint outside ellipse')
    assert(api.paint.state(E).enabled,'committing a stroke disabled curved painting')
    api.saveProject('/tmp/ime_curved_paint_editor.imesh')
    local loaded=IO.load('/tmp/ime_curved_paint_editor.imesh');assert(#loaded.regions[1].heightEdits==#saved);assert(Model.options(loaded,loaded.regions[1]).curvedPainting)
    assert(api.openProject('/tmp/ime_curved_paint_editor.imesh'));api.select(1);E.heightView=2
    repeat api.updateHeightPreview();coroutine.yield() until E.heightObject or E.heightError;assert(E.heightObject and not E.heightError,'painted map')
    api.setEditMode(false);api.rebuild();while E.meshTask do coroutine.yield() end;assert(E.preview,'painted geometry preview')
    local reference=meshDebug:new();assert(E.comparison and reference:load(E.comparison.previewPath),'missing painted comparison')
    local fullOptions=Model.options(E.project,E.project.regions[1]);fullOptions.curvedSimplify=false
    local full,fullReport=mbm.generateImageMesh(path,fullOptions);assert(full,fullReport)
    assert(reference:getTotalVertex(1,1)==fullReport.vertices)
    for i,v in ipairs(full:getVertex(1,1,1,fullReport.vertices)) do
        assert(math.abs(reference:getVertex(1,1,i).z+v.z)<.0001,'comparison original omitted painting')
    end
    api.exportOne('/tmp/ime_curved_paint_editor.msh')
    while E.meshTask do coroutine.yield() end
    local exported=meshDebug:new();assert(exported:load('/tmp/ime_curved_paint_editor.msh'))
    local expected,report=mbm.generateImageMesh(path,Model.options(E.project,E.project.regions[1]));assert(expected,report)
    assert(exported:getTotalVertex(1,1)==report.vertices)
    for i,v in ipairs(expected:getVertex(1,1,1,report.vertices)) do
        local actual=exported:getVertex(1,1,i)
        assert(math.abs(actual.z+v.z)<.0001,'export omitted painting')
    end
    api.setEditMode(true);api.paint.state(E).enabled=true;E.heightView=2
    repeat api.updateHeightPreview();coroutine.yield() until not E.heightJob and not E.heightRequested;Canvas.sync(E)
    baseline={height=E.heightBuilds,mesh=E.builds,stats=E.statisticsBuilds}
    print('CURVED PAINT EDITOR HISTORY / PERSISTENCE / EXPORT OK')
    started=mbm.getTimeRun()
end
function onInitScene()
    task=coroutine.create(run)
end
function onLoop(delta)
    if coroutine.status(task)~='dead' then
        local ok,err=coroutine.resume(task)
        if not ok then print('CURVED PAINT EDITOR FAIL '..tostring(err));mbm.quit();return end
    end
    local collapsing=tImGui.CollapsingHeader
    tImGui.CollapsingHeader=function(label,...)
        if label==tLang.L('ime_paint_title') then tImGui.SetNextItemOpen(true,0) end
        return collapsing(label,...)
    end
    local checkbox=tImGui.Checkbox
    tImGui.Checkbox=function(label,value,...)
        if label==tLang.L('ime_paint_curved_enabled') and not activated then activated=true;return true end
        return checkbox(label,value,...)
    end
    loop(delta)
    tImGui.Checkbox=checkbox
    tImGui.CollapsingHeader=collapsing
    if started and mbm.getTimeRun()-started>3 then
        local E=api.state
        assert(E.heightBuilds==baseline.height and E.builds==baseline.mesh and E.statisticsBuilds==baseline.stats,'idle painting rebuild')
        print('CURVED PAINT EDITOR UI / IDLE OK');mbm.quit()
    end
end
