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
local IO=require 'image_mesh_io'
local Build=require 'image_mesh_build'
local Normal=require 'image_mesh_normal_map'
local api={};assert(loadfile('editor/image_mesh_editor.lua'))(api)
local init,loop,finish=onInitScene,onLoop,onEndScene
local E=api.state
local root='/tmp/imesh-normal-smoke'
local task,started
local native=mbm.startImageMesh
local calls=0
mbm.startImageMesh=function(...) calls=calls+1;return native(...) end
local function wait() repeat coroutine.yield() until not E.meshTask and not E.normalDirty end
local function pixels(path) return assert(mbm.readImagePixels(path)) end
local function surface(asset)
    local vertices,indices=require('image_mesh_asset').geometry(asset)
    local triangles={}
    for i=1,#indices,3 do
        local corners={}
        for j=i,i+2 do
            local v=vertices[indices[j]]
            corners[#corners+1]=string.format('%.6f/%.6f/%.6f/%.6f/%.6f',v.x,v.y,v.z,v.u,v.v)
        end
        table.sort(corners);triangles[#triangles+1]=table.concat(corners,';')
    end
    table.sort(triangles);return table.concat(triangles,'\n')
end
local function test()
    assert(mbm.createDirectories(root));mbm.addPath(root)
    local rows={}
    for y=1,32 do
        local row={}
        for x=1,48 do local v=math.floor(120+80*math.sin(x*.3)*math.cos(y*.3));row[x]=string.char(v,v,v,255) end
        rows[y]=table.concat(row)
    end
    local source=root..'/source.png';assert(mbm.writeImagePixels(source,table.concat(rows),48,32))
    api.openImage(source)
    assert(api.action(function(p)
        p.defaults.reliefMode='normal';p.defaults.columns=3;p.defaults.rows=3
        p.defaults.normalMapStrength=4;p.defaults.lockBorder=false
        Model.add(p,'rectangle',8,4,24,24)
    end))
    api.select(1);api.setEditMode(false);api.rebuild();wait()
    assert(E.preview and E.report and not E.generationFailure,E.status)
    local object=E.preview
    local asset=meshDebug:new();assert(asset:load(E.previewPath))
    assert(asset:getTotalSubset(1)==3,'Front/back/walls must be separate')
    assert(asset:getMaterialTexture(1,1,'normal'))
    assert(not asset:getMaterialTexture(1,2,'normal') and not asset:getMaterialTexture(1,3,'normal'))
    local vertices=asset:getVertex(1,1,1,asset:getTotalVertex(1,1))
    local minZ,maxZ=math.huge,-math.huge
    for _,v in ipairs(vertices) do minZ=math.min(minZ,v.z);maxZ=math.max(maxZ,v.z) end
    assert(maxZ-minZ>.01,'Normal map flattened authored relief')
    local mapPath
    for _,record in pairs(E.normalResources) do mapPath=record.path end
    local before,w,h=mbm.readImagePixels(mapPath);assert(w==48 and h==32,'UV atlas size')
    assert(before:sub(1,4)==string.char(128,128,255,255),'Atlas padding')
    local geometryCalls,builds=calls,E.builds
    local revision,distance=E.revision,E.orbit.distance
    local px,py,pz=E.preview.x,E.preview.y,E.preview.z
    Normal.compare(E,true,api.camera)
    assert(E.normalComparison and E.normalComparison.preview.visible)
    local _,strength=E.normalComparison.preview:getNormalMapSettings(1);assert(strength==0)
    _,strength=E.preview:getNormalMapSettings(1);assert(strength==1,'Comparison changed target material')
    assert(E.revision==revision and calls==geometryCalls and E.builds==builds,'Comparison mutated project or geometry')
    for i=1,4 do coroutine.yield() end
    Normal.compare(E,false,api.camera)
    assert(not E.normalComparison and E.orbit.distance==distance)
    assert(E.preview.x==px and E.preview.y==py and E.preview.z==pz,'Comparison did not restore position')
    Normal.compare(E,true,api.camera)

    assert(api.action(function(p) p.defaults.normalMapStrength=8;p.defaults.normalMapBlur=1 end));wait()
    assert(E.preview==object and calls==geometryCalls and E.builds==builds,'Texture edit rebuilt geometry')
    assert(not E.normalComparison,'Editing did not release comparison')
    assert(pixels(mapPath)~=before,'Texture edit was not applied')
    api.undo(false);wait()
    assert(calls==geometryCalls and E.preview==object,'Undo rebuilt geometry')
    assert(pixels(mapPath)==before,'Undo did not restore normal map')
    assert(api.action(function(p) p.defaults.normalMapConvention='-Y';p.defaults.heightSource='manual';p.defaults.baseHeight=.5 end));wait()
    assert(calls>geometryCalls,'Height-source edit did not rebuild geometry')
    assert(E.preview:getNormalMapSettings(1)=='-Y')
    local neutral=pixels(mapPath)
    local offset=((10-1)*48+16-1)*4+1
    assert(neutral:sub(offset,offset+2)==string.char(128,128,255),'Manual constant normal')
    assert(api.saveProject(root..'/project.imesh'))
    local project=IO.load(root..'/project.imesh')
    assert(project.defaults.reliefMode=='normal' and project.defaults.normalMapConvention=='-Y')
    api.exportOne(root..'/plain.msh',false);wait()
    assert(IO.exists(root..'/plain_normal_01.png'),'Plain export did not package normal PNG')
    local exported=meshDebug:new();assert(exported:load(root..'/plain.msh'))
    assert(exported:getMaterialTexture(1,1,'normal')=='plain_normal_01.png')
    assert(exported:getNormalMapSettings(1,1)=='-Y')
    E.portableCrop=true
    api.exportOne(root..'/portable.msh',true);wait()
    assert(IO.exists(root..'/portable_normal_01.png'))
    local packed=meshDebug:new();assert(packed:load(root..'/portable.msh'))
    local normal,nw,nh=mbm.readImagePixels(root..'/portable_normal_01.png')
    local diffuse,dw,dh=mbm.readImagePixels(root..'/portable_texture_01.png')
    assert(normal and diffuse and nw==dw and nh==dh,'Portable UV domains differ')
    assert(packed:prepareNormalMap(1,1,'preserve').reused,'Portable export lost prepared tangents')
    local batchDir=root..'/batch';assert(mbm.createDirectories(batchDir))
    api.beginBatch(batchDir,true)
    repeat coroutine.yield() until not E.batch and not E.meshTask
    local batchPath=batchDir..'/'..IO.exportName(E.project.regions[1])
    local batchAsset=meshDebug:new();assert(batchAsset:load(batchPath))
    assert(IO.exists(batchDir..'/'..batchAsset:getMaterialTexture(1,1,'normal')),'Batch normal texture missing')
    -- Native splitting must preserve existing geometry/UVs and handle an open back.
    local o=Model.geometryOptions(Model.options(E.project,E.project.regions[1]));o.backOpen=true;o.separateFront=true
    local open,err=mbm.generateImageMesh(source,o);assert(open,err)
    assert(open:getTotalSubset(1)==2,'Open back created an empty subset')
    -- Mixed source, painted areas and a hole use the shared height raster.
    local region=Model.copy(E.project.regions[1]);region.overrides.heightSource='mixed'
    region.heightAreas={{shape='rectangle',mode='raise',height=.8,transition=.05,{x=.2,y=.2},{x=.8,y=.2},{x=.8,y=.8},{x=.2,y=.8}}}
    region.holes={{{x=.4,y=.4},{x=.6,y=.4},{x=.6,y=.6},{x=.4,y=.6}}}
    local owner={}
    local generated,report=Build.generate(owner,E.project,region);assert(generated,report)
    assert(generated:getMaterialTexture(1,1,'normal'))
    Normal.shutdown(owner)
    -- Material splitting must not influence adaptive geometry or QEM constraints.
    local preserved=Model.copy(E.project)
    preserved.defaults.heightSource='mixed';preserved.defaults.followImage=true
    preserved.defaults.simplify=true;preserved.defaults.simplifyRatio=.15
    preserved.defaults.columns=12;preserved.defaults.rows=12
    preserved.defaults.sideMode='band'
    local preservedRegion=preserved.regions[1]
    preservedRegion.heightAreas=Model.copy(region.heightAreas)
    preservedRegion.overrides.reliefMode='geometry'
    local without=assert(Build.generate({},preserved,preservedRegion))
    local owner={}
    preservedRegion.overrides.reliefMode='normal'
    local with=assert(Build.generate(owner,preserved,preservedRegion))
    assert(surface(without)==surface(with),'Normal material changed geometry or UVs after QEM')
    assert(with:getMaterialTexture(1,1,'normal'))
    Normal.shutdown(owner)
    -- Simplification compares geometry under the same normal-map material.
    assert(api.action(function(p)
        p.defaults.simplify=true;p.defaults.simplifyRatio=.8
        p.defaults.columns=12;p.defaults.rows=12
        p.defaults.heightSource='image';p.defaults.normalMapConvention='+Y'
    end));wait()
    assert(E.comparison,'Missing simplification original')
    api.setComparison(true)
    local reference=meshDebug:new();assert(reference:load(E.comparison.previewPath))
    local result=meshDebug:new();assert(result:load(E.previewPath))
    assert(reference:getMaterialTexture(1,1,'normal')==result:getMaterialTexture(1,1,'normal'))
    assert(reference:getMaterialTexture(1,1,'normal'),'Original has no normal texture')
    assert(reference:prepareNormalMap(1,1,'preserve').reused,'Original has no prepared tangents')
    local originalObject=E.comparison.preview
    local resultObject=E.preview
    local comparisonCalls=calls
    assert(api.action(function(p) p.defaults.normalMapConvention='-Y';p.defaults.normalMapStrength=3 end));wait()
    assert(E.comparison.preview==originalObject and E.preview==resultObject and calls==comparisonCalls)
    assert(originalObject:getNormalMapSettings(1)=='-Y' and resultObject:getNormalMapSettings(1)=='-Y')
    api.setComparison(false)
    -- Cached statistics must decorate the stored original when entering 3D.
    api.setEditMode(true)
    assert(api.action(function(p) p.defaults.columns=14 end))
    api.updateStatistics();wait()
    assert(E.generatedMesh and E.generatedMesh.originalPath)
    comparisonCalls=calls
    api.setEditMode(false);wait()
    assert(calls==comparisonCalls,'Statistics geometry was not reused')
    reference=meshDebug:new();assert(reference:load(E.comparison.previewPath))
    assert(reference:getMaterialTexture(1,1,'normal'))
    assert(E.comparison.preview:getNormalMapSettings(1)=='-Y')
    -- Assembly previews also update their material without replacing geometry.
    api.setAssembly(true);wait()
    local assemblyObject=E.assembly.items[1].preview
    geometryCalls=calls
    assert(api.action(function(p) p.defaults.normalMapStrength=2 end));wait()
    assert(E.assembly.items[1].preview==assemblyObject and calls==geometryCalls,'Assembly rebuilt geometry')
    -- Cancelling processing must release the map worker and allow the next build.
    local cancelledOwner={}
    local cancelTask=coroutine.create(function() return pcall(Normal.texture,cancelledOwner,E.project,E.project.regions[1]) end)
    assert(coroutine.resume(cancelTask));assert(cancelledOwner.normalProcessing)
    api.generation.cancel(cancelledOwner)
    local ok,result,message
    repeat
        coroutine.yield()
        ok,result,message=coroutine.resume(cancelTask);assert(ok,result)
    until coroutine.status(cancelTask)=='dead'
    assert(result==false and tostring(message):find('ime_generation_cancelled'))
    assert(not cancelledOwner.normalProcessing and not cancelledOwner.normalHeightJob)
    Normal.shutdown(cancelledOwner)
    api.setAssembly(false);wait()
    assert(api.action(function(p) p.defaults.reliefMode='geometry' end));wait()
    local switched=meshDebug:new();assert(switched:load(E.previewPath))
    assert(not switched:getMaterialTexture(1,1,'normal'),'Geometry mode retained normal texture')
    assert(api.action(function(p) p.defaults.reliefMode='normal';p.defaults.heightSource='curved' end));wait()
    switched=meshDebug:new();assert(switched:load(E.previewPath))
    assert(switched:getMaterialTexture(1,1,'normal'),'Curved height did not generate normal texture')
    local idleCalls,idleBuilds,idleMaps=calls,E.builds,E.normalBuilds
    local time=mbm.getTimeRun()
    while mbm.getTimeRun()-time<2 do coroutine.yield() end
    assert(calls==idleCalls and E.builds==idleBuilds and E.normalBuilds==idleMaps,'Idle processing')
    print('IMAGE MESH NORMAL PASS')
end
function onInitScene()
    init();started=mbm.getTimeRun();task=coroutine.create(test)
end
function onLoop(delta)
    loop(delta)
    if coroutine.status(task)=='dead' then mbm.quit();return end
    local ok,err=coroutine.resume(task)
    if not ok then print('IMAGE MESH NORMAL FAIL '..tostring(err));mbm.quit() end
    if mbm.getTimeRun()-started>45 then print('IMAGE MESH NORMAL FAIL timeout');mbm.quit() end
end
function onEndScene()
    mbm.startImageMesh=native
    finish()
end
