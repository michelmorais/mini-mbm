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
local Presets=require 'image_mesh_presets'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local task,started,baseline,deadline,activated=false,nil,nil,nil,false
local function run()
    init()
    local pixels={};for i=1,65*65*3 do pixels[i]=128 end
    local path='/tmp/ime_bevel_editor.png';assert(mbm.createTexture(pixels,65,65,3,'ime_bevel_editor',path))
    assert(api.openImage(path));assert(api.action(function(p) Model.primitive(p,'rectangle',60,60) end))
    api.select(1,false)
    local E=api.state
    E.values.relief=0;E.values.bevelWidth=2
    -- The real panel's checkbox is activated once by the widget shim below.
    repeat coroutine.yield() until activated
    assert(E.values.bevel);assert(api.applyProperties())
    assert(Model.options(E.project,E.project.regions[1]).bevel)
    api.undo(false);assert(not Model.options(E.project,E.project.regions[1]).bevel)
    api.undo(true);assert(Model.options(E.project,E.project.regions[1]).bevel)
    assert(api.action(function(p) Presets.store(p,'Bevel',E.values) end))
    assert(api.saveProject('/tmp/ime_bevel_editor.imesh'))
    assert(api.openProject('/tmp/ime_bevel_editor.imesh'));api.select(1,false)
    assert(E.values.bevel and E.values.bevelWidth==2 and E.project.presets[1].settings.bevel)
    E.values.bevel=false;assert(api.applyProperties());assert(E.values.bevelWidth==2)
    E.values.bevel=true;assert(api.applyProperties())
    api.exportOne('/tmp/ime_bevel_editor.msh')
    repeat coroutine.yield() until not E.meshTask
    local loaded=meshDebug:new();assert(loaded:load('/tmp/ime_bevel_editor.msh'));assert(loaded:check())
    repeat coroutine.yield() until not E.meshTask and not E.heightJob
    baseline={builds=E.builds,height=E.heightBuilds,stats=E.statisticsBuilds}
    started=mbm.getTimeRun();print('BEVEL EDITOR HISTORY / PERSISTENCE / EXPORT OK')
end
function onInitScene() deadline=mbm.getTimeRun()+30;task=coroutine.create(run) end
function onLoop(delta)
    if mbm.getTimeRun()>deadline then print('BEVEL EDITOR FAIL timeout');mbm.quit();return end
    if coroutine.status(task)~='dead' then
        local ok,err=coroutine.resume(task)
        if not ok then print('BEVEL EDITOR FAIL '..tostring(err));mbm.quit();return end
    end
    local header,checkbox=tImGui.CollapsingHeader,tImGui.Checkbox
    tImGui.CollapsingHeader=function(label,...)
        if label==tLang.L('ime_bevel_title') then tImGui.SetNextItemOpen(true,0) end
        return header(label,...)
    end
    tImGui.Checkbox=function(label,value,...)
        if label==tLang.L('ime_bevel_enabled') and not activated then activated=true;return true end
        return checkbox(label,value,...)
    end
    loop(delta)
    tImGui.CollapsingHeader=header;tImGui.Checkbox=checkbox
    if started and mbm.getTimeRun()-started>2 then
        local E=api.state
        assert(E.builds==baseline.builds and E.heightBuilds==baseline.height and E.statisticsBuilds==baseline.stats,'idle bevel rebuild')
        print('BEVEL EDITOR UI / IDLE OK');mbm.quit()
    end
end
