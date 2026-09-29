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
local Authoring=require 'normal_map_authoring'
local api={}; assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local frames,started=0,0
local task
local asset,entry
local apiCalls,baseline=0,0
local builds,statistics
local failed=false
local dir=assert(os.getenv('MBM_NORMAL_MAP_FIXTURE_DIR'))..'/'
local function runInit()

    asset=meshDebug:new(); assert(asset:load(dir..'no-map.msh'))
    local corners={{x=0,y=1,z=0,sign=1},{x=0,y=1,z=0,sign=1},{x=0,y=1,z=0,sign=1}}
    local report=assert(asset:prepareNormalMap(1,1,'import',corners))
    assert(report.vertices==3 and not report.reused)
    assert(asset:prepareNormalMap(1,1,'preserve').reused)
    local bad,err=asset:prepareNormalMap(1,1,'import',{{x=1,y=0,z=0,sign=0}})
    assert(not bad and type(err)=='string')
    assert(asset:prepareNormalMap(1,1,'preserve').reused,'failed import mutated basis')
    assert(not pcall(asset.prepareNormalMap,asset,1,1,'unknown'))
    assert(not pcall(asset.prepareNormalMap,asset,1,1,'generate',corners))
    assert(asset:getMaterialTexture(1,1,'normal')==nil,'preparation assigned normal texture')
    local missingNormals=meshDebug:new();assert(missingNormals:load(dir..'no-map.msh'))
    missingNormals:enableNormal(false)
    local calls={}
    local multiple={getTotalFrame=function() return 2 end,getTotalSubset=function(_,f) return f end}
    function multiple:prepareNormalMap(f,s,policy)
        calls[#calls+1]=f..':'..s
        if f==2 and s==2 then return missingNormals:prepareNormalMap(1,1,policy) end
        return asset:prepareNormalMap(1,1,policy)
    end
    local bulk=Authoring.prepareSelection(multiple,0,0,'preserve')
    assert(table.concat(calls,',')=='1:1,2:1,2:2','all uses each frame subset count')
    assert(bulk.prepared==2 and bulk.preserved==2 and #bulk.failures==1)
    assert(bulk.failures[1].frame==2 and bulk.failures[1].subset==2)
    calls={};bulk=Authoring.prepareSelection(multiple,0,1,'preserve')
    assert(table.concat(calls,',')=='1:1,2:1' and bulk.prepared==2 and bulk.reused)
    calls={};bulk=Authoring.prepareSelection(multiple,2,0,'preserve')
    assert(table.concat(calls,',')=='2:1,2:2' and bulk.prepared==1 and #bulk.failures==1)
    calls={};bulk=Authoring.prepareSelection(multiple,2,2,'generate')
    assert(table.concat(calls,',')=='2:2' and bulk.prepared==0 and #bulk.failures==1)
    local sourceFrame={subsets={{cornerTangents=corners}}}
    assert(not Authoring.importFrame(asset,1,sourceFrame,{}),'implicit tangent discard')
    assert(not Authoring.importFrame(asset,1,sourceFrame,{normalMapPolicy='import',importPostProcess=true}))
    assert(Authoring.importFrame(asset,1,sourceFrame,{normalMapPolicy='import'}))
    assert(asset:prepareNormalMap(1,1,'preserve').reused)

    local pixels={};for i=1,32*32 do pixels[#pixels+1]=180;pixels[#pixels+1]=180;pixels[#pixels+1]=180 end
    local source=dir..'normal-author-image.png'
    assert(mbm.createTexture(pixels,32,32,3,'normal_author_source',source))
    assert(api.openImage(source))
    assert(api.action(function(p)
        p.defaults.columns=2;p.defaults.rows=2
        Model.add(p,'rectangle',0,0,32,32)
    end))
    api.select(1)
    api.state.values.normalMapPrecompute=true
    assert(api.applyProperties())
    assert(Model.options(api.state.project,api.state.project.regions[1]).normalMapPrecompute)
    api.undo(false)
    assert(not Model.options(api.state.project,api.state.project.regions[1]).normalMapPrecompute,'undo option')
    api.undo(true)
    assert(Model.options(api.state.project,api.state.project.regions[1]).normalMapPrecompute,'redo option')
    assert(api.saveProject(dir..'normal-author.imesh'))
    assert(api.openProject(dir..'normal-author.imesh'))
    api.select(1);api.setEditMode(false);api.rebuild()
    while api.state.meshTask do coroutine.yield() end
    assert(api.state.report and api.state.report.normalMap,api.state.status)
    api.exportOne(dir..'normal-author-image.msh')
    while api.state.meshTask do coroutine.yield() end
    local reloaded=meshDebug:new();assert(reloaded:load(dir..'normal-author-image.msh'))
    for s=1,reloaded:getTotalSubset(1) do
        assert(reloaded:prepareNormalMap(1,s,'preserve').reused,'export did not retain explicit basis')
        assert(reloaded:getMaterialTexture(1,s,'normal')==nil)
    end
    -- A proxy counts only expensive preparation requests from the panel.
    local proxy={}
    function proxy:getTotalFrame() return asset:getTotalFrame() end
    function proxy:getTotalSubset(f) return asset:getTotalSubset(f) end
    function proxy:prepareNormalMap(...) apiCalls=apiCalls+1;return asset:prepareNormalMap(...) end
    entry={meshDebug=proxy}
    print('NORMAL MAP AUTHORING API / PROJECT / UNDO / EXPORT PASS')
end
function onInitScene()
    init(); started=mbm.getTimeRun();task=coroutine.create(runInit)
end
function onLoop(delta)
    local ok,err=pcall(loop,delta)
    if not ok then failed=true;print('NORMAL MAP AUTHORING FAIL: '..tostring(err));mbm.quit();return end
    if coroutine.status(task)~='dead' then
        ok,err=coroutine.resume(task)
        if not ok then print('NORMAL MAP AUTHORING FAIL: '..tostring(err));mbm.quit();return end
        if mbm.getTimeRun()-started>10 then print('NORMAL MAP AUTHORING FAIL: task timeout');mbm.quit() end
        return
    end
    frames=frames+1
    tImGui.Begin('Normal map preparation smoke',true,0)
    local button=tImGui.Button
    -- Draw real controls; simulate the prepare action once without relying on mouse automation.
    tImGui.Button=function(label,...)
        local pressed=button(label,...)
        return pressed or (frames==1 and label:find('##nma',1,true)~=nil)
    end
    local panelOk,panelError=pcall(Authoring.panel,entry,1,function() end,
        function(action) return action() end,function() return false end)
    tImGui.Button=button
    tImGui.End()
    if not panelOk then failed=true;print('NORMAL MAP AUTHORING FAIL: '..tostring(panelError));mbm.quit();return end
    if frames==1 then baseline=apiCalls;builds=api.state.builds;statistics=api.state.statisticsBuilds;assert(baseline==1) end
    if frames>=20 then
        local result,why=pcall(function()
            assert(apiCalls==baseline,'idle panel repeated tangent preparation')
            assert(entry.normalMapAuthoring.report.reused,'panel did not preserve imported basis')
            assert(not api.state.dirty,'idle editor stayed dirty')
            assert(api.state.builds==builds and api.state.statisticsBuilds==statistics,'idle editor rebuilt geometry')
            assert(api.state.report.normalMap,'idle editor lost preparation report')
        end)
        if not result then failed=true;print('NORMAL MAP AUTHORING FAIL: '..tostring(why)) end
        print(failed and 'NORMAL MAP AUTHORING FAIL' or 'NORMAL MAP AUTHORING PASS')
        mbm.quit()
    elseif mbm.getTimeRun()-started>10 then print('NORMAL MAP AUTHORING FAIL: timeout');mbm.quit() end
end
function onEndScene() if finish then finish() end end
