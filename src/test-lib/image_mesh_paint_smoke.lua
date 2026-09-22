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
local function run()
    init()
    local pixels={};for i=1,65*65*3 do pixels[i]=128 end
    local path='/tmp/ime_paint_source.png'
    assert(mbm.createTexture(pixels,65,65,3,'ime_paint_source',path))
    local options={columns=16,rows=16,lockBorder=false,relief=10}
    local function height(edits)
        options.heightEdits=edits
        local asset,report=mbm.generateImageMesh(path,options);assert(asset,report)
        return report.maxHeight,asset,report
    end
    local function dab(mode,strength,target,radius)
        return {x=.5,y=.5,mode=mode,radius=radius or .25,strength=strength or 1,height=target or .5}
    end
    local original=height()
    assert(height({dab('raise',.25)})>original+2.4)
    local lower,asset=height({dab('lower',.25)})
    local center
    for _,v in ipairs(asset:getVertex(1,1,1,asset:getTotalVertex(1,1))) do
        if math.abs(v.x)<.001 and math.abs(v.y)<.001 and v.nz<-.5 then center=v.z end
    end
    assert(center and math.abs(center-(-10-(128/255-.25)*10))<.001,'lower center')
    assert(math.abs(height({dab('flatten',1,.9)})-9)<.001,'flatten target')
    local raised=height({dab('raise',.5,nil,.03)})
    assert(height({dab('raise',.5,nil,.03),dab('smooth',1,nil,.2)})<raised-.1,'smooth peak')
    options.twoLevels=true;options.grooveThreshold=.8
    assert(height({dab('flatten',1,.7)})>6.9,'paint must follow two-level mapping')
    options.lockBorder=true;options.borderWidth=.1
    local border=dab('raise');border.x=0;border.y=0
    local _,edgeAsset=height({border})
    for _,v in ipairs(edgeAsset:getVertex(1,1,1,edgeAsset:getTotalVertex(1,1))) do
        if math.abs(v.x)>49.99 and v.z<0 then assert(math.abs(v.z+10)<.001,'locked border changed') end
    end
    options.heightEdits={dab('raise')};options.heightEdits[1].radius=0/0
    local bad=mbm.generateImageMesh(path,options);assert(not bad,'NaN brush accepted')
    options.heightEdits={dab('raise')};options.heightEdits[1].radius=0
    assert(not mbm.generateImageMeshMap(path,options,'/tmp/ime_bad_map.png',false))
    -- A local dab must not change two-level remapping elsewhere, even between source pixels.
    local gradient={}
    for y=0,64 do for x=0,64 do for c=1,3 do gradient[#gradient+1]=x*3 end end end
    local gradientPath='/tmp/ime_paint_gradient.png'
    assert(mbm.createTexture(gradient,65,65,3,'ime_paint_gradient',gradientPath))
    local opts={columns=19,rows=17,lockBorder=false,twoLevels=true,grooveTransition=.7}
    local clean,cleanReport=mbm.generateImageMesh(gradientPath,opts);assert(clean,cleanReport)
    opts.heightEdits={dab('raise',.3,nil,.03)}
    local painted,paintedReport=mbm.generateImageMesh(gradientPath,opts);assert(painted,paintedReport)
    assert(cleanReport.vertices==paintedReport.vertices)
    for i,v in ipairs(clean:getVertex(1,1,1,cleanReport.vertices)) do
        if math.abs(v.x)>10 or math.abs(v.y)>10 then
            assert(painted:getVertex(1,1,i).z==v.z,'painting altered automatic height outside brush')
        end
    end
    assert(api.openImage(path))
    api.action(function(p) p.defaults.columns=16;p.defaults.rows=16;p.defaults.lockBorder=false;Model.add(p,'ellipse',0,0,65,65) end)
    api.select(1)
    local E=api.state
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
    api.saveProject('/tmp/ime_paint.imesh')
    local loaded=IO.load('/tmp/ime_paint.imesh');assert(#loaded.regions[1].heightEdits==#saved)
    assert(api.openProject('/tmp/ime_paint.imesh'));api.select(1);E.heightView=2
    api.updateHeightPreview();assert(E.heightObject and not E.heightError,'painted map')
    api.setEditMode(false);api.rebuild();assert(E.preview,'painted geometry preview')
    api.exportOne('/tmp/ime_paint.msh')
    local exported=meshDebug:new();assert(exported:load('/tmp/ime_paint.msh'))
    local expected,report=mbm.generateImageMesh(path,Model.options(E.project,E.project.regions[1]));assert(expected,report)
    assert(exported:getTotalVertex(1,1)==report.vertices)
    for i,v in ipairs(expected:getVertex(1,1,1,report.vertices)) do
        local actual=exported:getVertex(1,1,i)
        assert(math.abs(actual.z+v.z)<.0001,'export omitted painting')
    end
    api.setEditMode(true);api.paint.state(E).enabled=true;E.heightView=2
    api.updateHeightPreview();Canvas.sync(E)
    baseline={height=E.heightBuilds,mesh=E.builds,stats=E.statisticsBuilds}
    print('PAINT BRUSHES / BORDERS / VALIDATION / HISTORY / PERSISTENCE / EXPORT OK')
    started=mbm.getTimeRun()
end
function onInitScene()
    local ok,err=pcall(run)
    if not ok then print('PAINT FAIL '..tostring(err));mbm.quit() end
end
function onLoop(delta)
    if not started then return end
    local collapsing=tImGui.CollapsingHeader
    tImGui.CollapsingHeader=function(label,...)
        if label==tLang.L('ime_paint_title') then tImGui.SetNextItemOpen(true,0) end
        return collapsing(label,...)
    end
    loop(delta)
    tImGui.CollapsingHeader=collapsing
    if mbm.getTimeRun()-started>3 then
        local E=api.state
        assert(E.heightBuilds==baseline.height and E.builds==baseline.mesh and E.statisticsBuilds==baseline.stats,'idle painting rebuild')
        print('PAINT EDITOR UI / IDLE OK');mbm.quit()
    end
end
