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
local Normal=require 'image_mesh_normal_map'
local E={}
local task,started
local function test()
    local root='/tmp/imesh-automatic-smoke'
    assert(mbm.createDirectories(root));mbm.addPath(root)
    local pixels={}
    for y=0,8 do for x=0,8 do
        local v=64+x*16+(y%2)*48
        pixels[#pixels+1]=string.char(v,v,v,255)
    end end
    local source=root..'/source.png'
    assert(mbm.writeImagePixels(source,table.concat(pixels),9,9))
    local project=Model.new(source,9,9)
    local defaults=project.defaults
    for k,v in pairs({width=8,height=8,depth=2,relief=8,columns=4,rows=4,lockBorder=false,
        backOpen=true,followImage=true,heightTolerance=.05,reliefMode='normal',
        normalMapResidual=true,normalMapBasis=true,normalMapAutomatic=true,normalMapTargetTriangles=50}) do
        defaults[k]=v
    end
    local region=Model.add(project,'rectangle',0,0,9,9)
    if type(region)=='number' then region=Model.region(project,region) end
    Model.validate(project)
    local asset,report=Build.generate(E,project,region)
    assert(asset,report)
    local choice=assert(report.detailSeparation)
    assert(choice.reached and choice.triangles<=50,'Automatic search did not reduce adaptive geometry')
    local options=Model.geometryOptions(Model.options(project,region))
    options.geometryTargetTriangles=nil;options.geometryBlurRadius=0
    local raw,rawReport=mbm.generateImageMesh(source,options);assert(raw,rawReport)
    assert(rawReport.triangles>choice.triangles)
    assert(asset:getMaterialTexture(1,1,'normal'),'Winner was not baked')
    print('AUTOMATIC TRIANGLES',rawReport.triangles,choice.triangles,'RADIUS',choice.radius)
    -- The budget is measured after the optional simplification stage.
    defaults.simplify=true;defaults.simplifyMode='qem';defaults.simplifyRatio=.9;defaults.simplifyDetails=false
    defaults.normalMapTargetTriangles=40
    asset,report=Build.generate(E,project,region);assert(asset,report)
    assert(report.detailSeparation.triangles==report.triangles)
    assert(report.detailSeparation.reached and report.triangles<=40)
    -- Fixed grid: automatic mode must activate QEM even when manual reduction is off.
    defaults.followImage=false;defaults.columns=24;defaults.rows=24
    defaults.simplify=false;defaults.simplifyMode='none';defaults.simplifyRatio=.95
    defaults.normalMapTargetTriangles=500
    asset,report=Build.generate(E,project,region);assert(asset,report)
    assert(report.simplification and report.detailSeparation.reached and report.triangles<=500)
    assert(not defaults.simplify and defaults.simplifyMode=='none' and defaults.simplifyRatio==.95)
    print('FIXED GRID TARGET',report.sourceTriangles,report.triangles)
    defaults.reliefMode='geometry'
    asset,report=Build.generate(E,project,region);assert(asset,report)
    assert(report.detailSeparation.reached and report.triangles<=500)
    assert(report.detailSeparation.radius==0 and report.detailSeparation.attempts==1)
    assert(not asset:getMaterialTexture(1,1,'normal'),'Geometry target unexpectedly generated normal material')
    defaults.reliefMode='normal';defaults.normalMapResidual=false
    asset,report=Build.generate(E,project,region);assert(asset,report)
    assert(report.detailSeparation.radius==0 and report.detailSeparation.attempts==1)
    assert(asset:getMaterialTexture(1,1,'normal'),'Additive normal material was lost')
    print('GEOMETRY TARGET WITHOUT RESIDUAL PASS')
    print('IMAGE MESH AUTOMATIC PASS')
end
function onInitScene()
    tImGui=require 'ImGui'
    tUtil=require 'editor_utils'
    E.values={heightSource='image',normalMapAutomatic=true,normalMapTargetTriangles=500,reliefMode='geometry'}
    started=mbm.getTimeRun()
    task=coroutine.create(test)
end
function onLoop()
    local open=tImGui.Begin('Triangle target smoke',false,0)
    if open then require('image_mesh_triangle_target').panel(E) end
    tImGui.End()
    local ok,err=coroutine.resume(task)
    if not ok then print('IMAGE MESH AUTOMATIC FAIL '..tostring(err));mbm.quit()
    elseif coroutine.status(task)=='dead' then mbm.quit()
    elseif mbm.getTimeRun()-started>90 then print('IMAGE MESH AUTOMATIC FAIL timeout');mbm.quit() end
end
function onEndScene() Normal.shutdown(E) end
