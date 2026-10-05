--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
|------------------------------------------------------------------------------------------------------------------------|

]]--

package.path='editor/?.lua;'..package.path
local Model=require 'image_mesh_model'
local Build=require 'image_mesh_build'
local Asset=require 'image_mesh_asset'
local IO=require 'image_mesh_io'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop=onInitScene,onLoop
local task,started
local function await() while api.state.meshTask do coroutine.yield() end end
local function geometry(asset)
    local vertices,indices=Asset.geometry(asset)
    local result={}
    for _,v in ipairs(vertices) do
        for _,k in ipairs{'x','y','z','u','v'} do result[#result+1]=string.format('%a',v[k]) end
    end
    for _,i in ipairs(indices) do result[#result+1]=tostring(i) end
    return table.concat(result,',')
end
local function test()
    local E=api.state
    local pixels={}
    for y=0,8 do for x=0,8 do
        local value=((x+y)%2==0) and 30 or 220
        pixels[#pixels+1]=value;pixels[#pixels+1]=value;pixels[#pixels+1]=value
    end end
    local source='/tmp/ime-confirm-target-source.png'
    assert(mbm.createTexture(pixels,9,9,3,'ime_confirm_target',source));assert(api.openImage(source))
    assert(api.action(function(p)
        for k,v in pairs({width=8,height=8,depth=2,relief=8,columns=4,rows=4,lockBorder=false,
            backOpen=true,followImage=true,heightTolerance=.05,reliefMode='normal',
            normalMapResidual=true,normalMapBasis=true,normalMapAutomatic=true,normalMapTargetTriangles=50}) do
            p.defaults[k]=v
        end
        Model.add(p,'rectangle',0,0,9,9);Model.add(p,'rectangle',0,0,9,9)
    end))
    api.select(1);E.selection[2]=true
    api.updateStatistics();await()
    local report=assert(E.report,E.status)
    local parameters=assert(report.targetParameters)
    assert(report.detailSeparation.radius>0,'fixture must exercise smoothing conversion')
    local original=geometry(assert(E.generatedMesh).asset)
    local blur=report.detailSeparation.radius
    local depth=E.values.depth;E.values.depth=depth+1
    assert(not api.confirmAutomaticTarget(),'stale draft was accepted');E.values.depth=depth
    assert(api.confirmAutomaticTarget())
    assert(not E.values.normalMapAutomatic and E.values.normalMapGeometryBlur==blur)
    assert(E.values.simplifyRatio==parameters.simplifyRatio)
    assert(Model.options(E.project,E.project.regions[2]).normalMapAutomatic,'confirmation changed another module')
    assert(not api.confirmAutomaticTarget(),'already-confirmed result accepted again')
    api.undo(false);assert(E.values.normalMapAutomatic)
    api.undo(true);assert(not E.values.normalMapAutomatic)
    local target=require 'image_mesh_target'
    local originalApply=target.apply
    target.apply=function() error('confirmed parameters repeated the search') end
    api.updateStatistics();await()
    assert(E.generatedMesh and geometry(E.generatedMesh.asset)==original,E.status)
    assert(api.saveProject('/tmp/ime-confirm-target.imesh'))
    assert(api.openProject('/tmp/ime-confirm-target.imesh'));api.select(1)
    assert(not E.values.normalMapAutomatic and E.values.simplifyRatio==parameters.simplifyRatio)
    api.updateStatistics();await()
    assert(E.generatedMesh and geometry(E.generatedMesh.asset)==original,E.status)
    local builds=E.statisticsBuilds
    for _=1,5 do coroutine.yield() end
    assert(E.statisticsBuilds==builds,'idle recomputed geometry')
    target.apply=originalApply
    print('CONFIRM AUTOMATIC TARGET PASS: identical geometry, smoothing, exact ratio, stale draft, selected only, undo, persistence, no search, idle')
end
function onInitScene() init();started=mbm.getTimeRun();task=coroutine.create(test) end
function onLoop(delta)
    loop(delta)
    if coroutine.status(task)=='dead' then mbm.quit();return end
    local ok,err=coroutine.resume(task)
    if not ok then print('CONFIRM AUTOMATIC TARGET FAIL: '..tostring(err));mbm.quit() end
    if mbm.getTimeRun()-started>90 then print('CONFIRM AUTOMATIC TARGET FAIL: timeout');mbm.quit() end
end
