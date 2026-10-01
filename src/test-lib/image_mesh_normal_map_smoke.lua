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
        p.defaults.sideMode='band';p.defaults.sideInset=4
        Model.add(p,'rectangle',8,4,24,24)
    end))
    api.select(1);api.setEditMode(false);api.rebuild();wait()
    assert(E.preview and E.report and not E.generationFailure,E.status)
    local object=E.preview
    local asset=meshDebug:new();assert(asset:load(E.previewPath))
    assert(asset:getTotalSubset(1)==3,'Front/back/walls must be separate')
    assert(asset:getMaterialTexture(1,1,'normal'))
    assert(not asset:getMaterialTexture(1,2,'normal'),'Back unexpectedly received normal map')
    assert(asset:getMaterialTexture(1,3,'normal')==asset:getMaterialTexture(1,1,'normal'),'Band side missing normal map')
    assert(asset:prepareNormalMap(1,3,'preserve').reused,'Band side missing tangents')
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
    assert(E.normalComparison and E.normalComparison.preview.visible and not E.normalComparison.additive)
    local _,strength=E.normalComparison.preview:getNormalMapSettings(1);assert(strength==0)
    _,strength=E.normalComparison.preview:getNormalMapSettings(3);assert(strength==0,'Side normal remains enabled in comparison')
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
    assert(E.preview:getNormalMapSettings(1)=='-Y' and E.preview:getNormalMapSettings(3)=='-Y')
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
    assert(exported:getNormalMapSettings(1,1)=='-Y' and exported:getNormalMapSettings(1,3)=='-Y')
    assert(IO.exists(root..'/'..exported:getMaterialTexture(1,3,'normal')))
    E.portableCrop=true
    api.exportOne(root..'/portable.msh',true);wait()
    assert(IO.exists(root..'/portable_normal_01.png'))
    local packed=meshDebug:new();assert(packed:load(root..'/portable.msh'))
    local normal,nw,nh=mbm.readImagePixels(root..'/portable_normal_01.png')
    local diffuse,dw,dh=mbm.readImagePixels(root..'/portable_texture_01.png')
    assert(normal and diffuse and nw==dw and nh==dh,'Portable UV domains differ')
    assert(packed:prepareNormalMap(1,1,'preserve').reused,'Portable export lost prepared tangents')
    assert(packed:prepareNormalMap(1,3,'preserve').reused,'Portable side lost prepared tangents')
    local _,sw,sh=mbm.readImagePixels(root..'/'..packed:getMaterialTexture(1,3,'normal'))
    local _,dw,dh=mbm.readImagePixels(root..'/'..packed:getTexture(1,3))
    assert(sw==dw and sh==dh,'Portable side UV domains differ')
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
    -- A separate back material and an open back must keep the side subset correct.
    for _,back in ipairs({'solid','open'}) do
        preservedRegion.overrides.backSolid=back=='solid'
        preservedRegion.overrides.backOpen=back=='open'
        local owner={}
        local mesh=assert(Build.generate(owner,preserved,preservedRegion))
        local side=mesh:getTotalSubset(1)
        assert(mesh:getMaterialTexture(1,side,'normal'),'Separate side normal missing')
        assert(mesh:prepareNormalMap(1,side,'preserve').reused)
        if back=='solid' then
            assert(mesh:getTexture(1,2):sub(1,1)=='#' and not mesh:getMaterialTexture(1,2,'normal'))
        end
        Normal.shutdown(owner)
    end
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
    assert(originalObject:getNormalMapSettings(3)=='-Y' and resultObject:getNormalMapSettings(3)=='-Y')
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
    assert(E.comparison.preview:getNormalMapSettings(1)=='-Y' and E.comparison.preview:getNormalMapSettings(3)=='-Y')
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
    -- Repeated sides use either the source crop or a separate diffuse image.
    local externalBytes={}
    for y=1,12 do for x=1,8 do
        local v=(x*29+y*11)%256
        externalBytes[#externalBytes+1]=string.char(v,v,v,255)
    end end
    externalBytes=table.concat(externalBytes)
    local externalPath=root..'/side.png'
    assert(mbm.writeImagePixels(externalPath,externalBytes,8,12))
    for case,sidePath in ipairs({'',externalPath}) do
        assert(api.action(function(p)
            p.defaults.heightSource='image';p.defaults.simplify=false
            p.defaults.sideMode='repeat';p.defaults.sideTexture=sidePath
            p.defaults.sideRepeatU=2.5;p.defaults.sideRepeatV=1.5
            p.defaults.normalMapBlur=1;p.defaults.normalMapStrength=2
            p.defaults.normalMapConvention='+Y'
        end));wait()
        local side=E.preview.imageMeshNormalSubsets[#E.preview.imageMeshNormalSubsets]
        assert(side>1,'Repeated side missing normal material')
        local current=meshDebug:new();assert(current:load(E.previewPath))
        assert(current:getMaterialTexture(1,1,'normal')~=current:getMaterialTexture(1,side,'normal'))
        assert(current:prepareNormalMap(1,side,'preserve').reused)
        local record
        for key,value in pairs(E.normalResources) do if key:sub(-7)==':repeat' then record=value end end
        assert(record)
        local bytes,w,h=mbm.readImagePixels(record.path)
        local input,iw,ih=externalBytes,8,12
        if case==1 then
            local sourceBytes=mbm.readImagePixels(source);local rows={}
            for y=1,24 do local first=((4+y-1)*48+8)*4+1;rows[y]=sourceBytes:sub(first,first+24*4-1) end
            input=table.concat(rows);iw=24;ih=24
            local actual={}
            for y=1,24 do local first=((4+y-1)*w+8)*4+1;actual[y]=bytes:sub(first,first+24*4-1) end
            bytes=table.concat(actual)
            assert(w==48 and h==32,'Repeated crop lost atlas UV domain')
        else assert(w==8 and h==12,'Independent side used front dimensions') end
        local expected=require('normal_map_generator').generate(require('height_map_source').image(input,iw,ih),
            {strength=2,blur=1,convention='+Y',edge='repeat'})
        assert(bytes==expected.bytes,'Repeated normal uses wrong source or border mode')
        local previous,counter=E.preview,calls
        assert(api.action(function(p) p.defaults.normalMapConvention='-Y';p.defaults.normalMapStrength=3 end));wait()
        assert(E.preview==previous and calls==counter,'Repeated normal edit rebuilt geometry')
        assert(E.preview:getNormalMapSettings(side)=='-Y')
        Normal.compare(E,true,api.camera)
        local _,strength=E.normalComparison.preview:getNormalMapSettings(side);assert(strength==0)
        Normal.compare(E,false,api.camera)
        assert(api.action(function(p) p.defaults.simplify=true;p.defaults.simplifyRatio=.95 end));wait()
        assert(E.comparison,'Repeated texture lost simplification comparison')
        local ref=E.comparison.preview
        local refSide=ref.imageMeshNormalSubsets[#ref.imageMeshNormalSubsets]
        assert(ref:getNormalMapSettings(refSide)=='-Y')
        local savedCalls=calls
        assert(api.action(function(p) p.defaults.normalMapConvention='+Y' end));wait()
        assert(ref:getNormalMapSettings(refSide)=='+Y' and E.preview:getNormalMapSettings(side)=='+Y')
        assert(calls==savedCalls,'Comparison material update rebuilt repeated geometry')
        api.exportOne(root..'/repeat-plain'..case..'.msh',false);wait()
        local plain=meshDebug:new();assert(plain:load(root..'/repeat-plain'..case..'.msh'))
        assert(IO.exists(root..'/'..plain:getMaterialTexture(1,side,'normal')))
        api.exportOne(root..'/repeat'..case..'.msh',true);wait()
        local packed=meshDebug:new();assert(packed:load(root..'/repeat'..case..'.msh'))
        assert(packed:prepareNormalMap(1,side,'preserve').reused)
        local _,nw,nh=mbm.readImagePixels(root..'/'..packed:getMaterialTexture(1,side,'normal'))
        local _,dw,dh=mbm.readImagePixels(root..'/'..packed:getTexture(1,side))
        assert(nw==dw and nh==dh,'Repeated portable UV domains differ')
    end
    -- Use a visibly curved fixture so simplification changes more than coplanar triangles.
    assert(api.action(function(p)
        p.defaults.width=24;p.defaults.relief=12;p.defaults.simplifyRatio=.7
        p.defaults.simplifyDetails=false
    end));wait()
    -- Residual maps depend on the particular mesh, including the original comparison.
    local beforeResidual=meshDebug:new();assert(beforeResidual:load(E.previewPath))
    local previous,counter=E.preview,calls
    assert(api.action(function(p)
        p.defaults.normalMapResidual=true;p.defaults.normalMapStrength=1;p.defaults.normalMapBlur=0
    end));wait()
    assert(not E.generationFailure,E.status)
    assert(E.preview==previous and calls==counter,'Residual toggle rebuilt geometry')
    assert(E.comparison and E.preview.imageMeshNormalGeometry and E.comparison.preview.imageMeshNormalGeometry)
    local key=E.project.image.path..'\0'..E.viewRegion..':residual:'
    local finalRecord,originalRecord=E.normalResources[key..'final'],E.normalResources[key..'original']
    assert(finalRecord and originalRecord and finalRecord.path~=originalRecord.path,'Comparisons share residual texture')
    assert(finalRecord.geometry~=originalRecord.geometry,'Comparisons share bake geometry')
    assert(#finalRecord.geometry.indices<#originalRecord.geometry.indices,'Residual fixture did not simplify the front')
    assert(pixels(finalRecord.path)~=pixels(originalRecord.path),'Simplification was not subtracted from residual')
    local approximate=pixels(finalRecord.path)
    assert(api.action(function(p) p.defaults.normalMapBasis=true end));wait()
    assert(E.preview==previous and calls==counter,'Basis compensation rebuilt geometry')
    assert(pixels(finalRecord.path)~=approximate,'Basis compensation did not change the residual')
    local geometry=E.preview.imageMeshNormalGeometry
    assert(#geometry.corners==#geometry.indices)
    local basisAsset=meshDebug:new();assert(basisAsset:load(E.previewPath))
    local snapshot=assert(basisAsset:getNormalMapCorners(1,1))
    local oldX=snapshot[1].x;snapshot[1].x=100
    assert(basisAsset:getNormalMapCorners(1,1)[1].x==oldX,'Corner snapshot aliases engine state')
    local absent,message=basisAsset:getNormalMapCorners(1,999)
    assert(not absent and type(message)=='string')
    assert(not pcall(function() basisAsset:getNormalMapCorners(0,1) end))
    local modified=basisAsset:getVertex(1,1,1,basisAsset:getTotalVertex(1,1))
    for _,v in ipairs(modified) do v.nx=.6;v.ny=0;v.nz=.8 end
    basisAsset:setVertex(1,1,1,modified)
    for _,t in ipairs(assert(basisAsset:getNormalMapCorners(1,1))) do
        assert(t.sign==0 or math.abs(.6*t.x+.8*t.z)<1e-5,'Snapshot retained a stale basis')
    end
    local imported={}
    for i=1,#snapshot do imported[i]={x=-.8,y=0,z=.6,sign=-1} end
    assert(basisAsset:prepareNormalMap(1,1,'import',imported))
    local preserved=assert(basisAsset:getNormalMapCorners(1,1))
    assert(preserved[1].sign==-1 and math.abs(preserved[1].z-.6)<1e-6,'Snapshot replaced imported tangents')
    local savedPixels=pixels(finalRecord.path)
    assert(api.action(function(p) p.defaults.normalMapConvention='-Y' end));wait()
    assert(calls==counter and savedPixels~=pixels(finalRecord.path),'Residual settings did not rebake')
    api.undo();wait();assert(pixels(finalRecord.path)==savedPixels,'Residual undo did not restore pixels')
    local beforeComparison=E.revision
    local comparisonDistance=E.orbit.distance
    local centerX,centerY,centerZ=E.preview.x,E.preview.y,E.preview.z
    Normal.requestComparison(E,true,api.camera,pcall);wait()
    local triple=assert(E.normalComparison)
    assert(triple.additive and triple.additive.visible and triple.preview.visible,'Missing third comparison mesh')
    assert(triple.preview.x<centerX and triple.additive.x==centerX and E.preview.x>centerX)
    assert(triple.additive.y==centerY and triple.additive.z==centerZ)
    local _,off=triple.preview:getNormalMapSettings(1)
    local convention,on=triple.additive:getNormalMapSettings(1)
    assert(off==0 and on==1 and convention==E.values.normalMapConvention)
    assert(E.revision==beforeComparison and calls==counter,'Comparison changed project or generated geometry')
    assert(pixels(finalRecord.path)==savedPixels,'Comparison changed residual texture')
    local maps,started=E.normalBuilds,mbm.getTimeRun()
    while mbm.getTimeRun()-started<.3 do coroutine.yield() end
    assert(E.normalComparison==triple and maps==E.normalBuilds,'Comparison continuously rebakes')
    Normal.compare(E,false,api.camera)
    assert(not E.normalComparison and E.orbit.distance==comparisonDistance and E.preview.x==centerX)
    Normal.requestComparison(E,true,api.camera,pcall);wait()
    assert(E.normalComparison and E.normalComparison.additive and maps==E.normalBuilds,'Comparison did not reuse textures')
    Normal.compare(E,false,api.camera)
    api.saveProject(root..'/residual.imesh')
    assert(IO.load(root..'/residual.imesh').defaults.normalMapResidual)
    assert(IO.load(root..'/residual.imesh').defaults.normalMapBasis)
    api.exportOne(root..'/residual-plain.msh',false);wait()
    api.exportOne(root..'/residual-portable.msh',true);wait()
    for _,name in ipairs({'residual-plain','residual-portable'}) do
        local exported=meshDebug:new();assert(exported:load(root..'/'..name..'.msh'))
        assert(exported:prepareNormalMap(1,1,'preserve').reused)
        assert(IO.exists(root..'/'..exported:getMaterialTexture(1,1,'normal')))
    end
    local afterResidual=meshDebug:new();assert(afterResidual:load(root..'/residual-plain.msh'))
    assert(surface(beforeResidual)==surface(afterResidual),'Residual changed geometry')
    -- Band sides must use the original additive map, never the front residual.
    assert(api.action(function(p) p.defaults.sideMode='band' end));wait()
    assert(not E.generationFailure,E.status)
    local band=meshDebug:new();assert(band:load(E.previewPath))
    local subsets=require('image_mesh_normal_material').subsets(band)
    assert(#subsets>=2)
    assert(band:getMaterialTexture(1,1,'normal')~=band:getMaterialTexture(1,subsets[#subsets],'normal'))
    -- Assembly rebakes use the captured geometry without generating new meshes.
    api.setAssembly(true);wait()
    counter=calls
    assert(api.action(function(p) p.defaults.normalMapBlur=1 end));wait()
    assert(calls==counter and not E.generationFailure,'Residual assembly update rebuilt geometry')
    api.setAssembly(false);wait()
    local residualBatch=root..'/residual-batch';assert(mbm.createDirectories(residualBatch))
    api.beginBatch(residualBatch,true)
    repeat coroutine.yield() until not E.batch and not E.meshTask
    local baked=meshDebug:new();assert(baked:load(residualBatch..'/'..IO.exportName(E.project.regions[1])))
    assert(IO.exists(residualBatch..'/'..baked:getMaterialTexture(1,1,'normal')))
    -- Curved height rasters store total normalized thickness, including symmetric backs.
    assert(api.action(function(p)
        p.defaults.heightSource='curved';p.defaults.simplify=false;p.defaults.normalMapBlur=0
    end));wait()
    assert(E.preview and not E.generationFailure,E.status)
    local curved=meshDebug:new();assert(curved:load(E.previewPath))
    assert(curved:getMaterialTexture(1,1,'normal') and curved:prepareNormalMap(1,1,'preserve').reused)
    -- Frequency separation changes geometry, but keeps full detail as the bake target.
    assert(api.action(function(p)
        p.defaults.heightSource='image';p.defaults.followImage=false
        p.defaults.normalMapGeometryBlur=0;p.defaults.normalMapBasis=true;p.defaults.normalMapResidual=true
    end));wait()
    local full=meshDebug:new();assert(full:load(E.previewPath))
    local fullSurface=surface(full)
    counter=calls
    assert(api.action(function(p) p.defaults.normalMapGeometryBlur=3 end));wait()
    assert(calls>counter and E.preview,'Separation did not rebuild geometry')
    local separated=meshDebug:new();assert(separated:load(E.previewPath))
    local separatedSurface=surface(separated)
    assert(separatedSurface~=fullSurface,'Separation did not change relief')
    api.undo();wait()
    local restored=meshDebug:new();assert(restored:load(E.previewPath))
    assert(surface(restored)==fullSurface,'Separation undo lost original geometry')
    assert(api.action(function(p) p.defaults.normalMapGeometryBlur=3 end));wait()
    counter=calls;local separatedObject=E.preview
    assert(api.action(function(p) p.defaults.normalMapStrength=.8 end));wait()
    assert(E.preview==separatedObject and calls==counter,'Normal strength rebuilt separated geometry')
    Normal.requestComparison(E,true,api.camera,pcall);wait()
    assert(E.normalComparison and E.normalComparison.additive,'Separated geometry lost three-way comparison')
    Normal.compare(E,false,api.camera)
    api.saveProject(root..'/separated.imesh')
    assert(IO.load(root..'/separated.imesh').defaults.normalMapGeometryBlur==3)
    api.exportOne(root..'/separated.msh',false);wait()
    local separatedExport=meshDebug:new();assert(separatedExport:load(root..'/separated.msh'))
    assert(surface(separatedExport)==separatedSurface,'Export used unfiltered geometry')
    assert(IO.exists(root..'/'..separatedExport:getMaterialTexture(1,1,'normal')))
    api.exportOne(root..'/separated-portable.msh',true);wait()
    local portableSeparated=meshDebug:new();assert(portableSeparated:load(root..'/separated-portable.msh'))
    assert(portableSeparated:prepareNormalMap(1,1,'preserve').reused)
    assert(api.action(function(p) p.defaults.normalMapBasis=false end));wait()
    restored=meshDebug:new();assert(restored:load(E.previewPath))
    assert(surface(restored)==fullSurface,'Disabling compensation did not restore full geometry')
    assert(api.action(function(p) p.defaults.normalMapBasis=true end));wait()
    -- Automatic selection measures final geometry and leaves normal-only edits cheap.
    assert(api.action(function(p)
        p.defaults.normalMapAutomatic=true;p.defaults.normalMapTargetTriangles=100000
    end));wait()
    local chosen=assert(E.report.detailSeparation)
    assert(chosen.radius==0 and chosen.reached and chosen.attempts==1)
    counter=calls;local automaticObject=E.preview
    assert(api.action(function(p) p.defaults.normalMapStrength=.9 end));wait()
    assert(calls==counter and E.preview==automaticObject,'Texture edit repeated automatic search')
    assert(api.action(function(p) p.defaults.normalMapTargetTriangles=2 end));wait()
    chosen=assert(E.report.detailSeparation)
    assert(not chosen.reached and chosen.attempts==7 and chosen.triangles==E.report.triangles)
    api.saveProject(root..'/automatic.imesh')
    local automaticProject=IO.load(root..'/automatic.imesh')
    assert(automaticProject.defaults.normalMapAutomatic and automaticProject.defaults.normalMapTargetTriangles==2)
    local automaticAsset=meshDebug:new();assert(automaticAsset:load(E.previewPath))
    api.exportOne(root..'/automatic.msh',false);wait()
    local automaticExport=meshDebug:new();assert(automaticExport:load(root..'/automatic.msh'))
    assert(surface(automaticAsset)==surface(automaticExport),'Automatic export changed geometry')
    assert(automaticExport:getMaterialTexture(1,1,'normal'))
    Normal.requestComparison(E,true,api.camera,pcall);wait()
    assert(E.normalComparison and E.normalComparison.additive)
    Normal.compare(E,false,api.camera)
    -- Cancelling a probe must prevent publishing a partial search result.
    local owner={}
    local cancelTask=coroutine.create(function() return Build.generate(owner,automaticProject,automaticProject.regions[1]) end)
    assert(coroutine.resume(cancelTask));assert(owner.imageJob)
    api.generation.cancel(owner)
    local ok,asset,message
    repeat
        coroutine.yield()
        ok,asset,message=coroutine.resume(cancelTask);assert(ok,asset)
    until coroutine.status(cancelTask)=='dead'
    assert(not asset and owner.generationCancelled and message=='ime_generation_cancelled')
    assert(api.action(function(p) p.defaults.normalMapAutomatic=false end));wait()
    assert(not E.report.detailSeparation and E.project.defaults.normalMapGeometryBlur==3)
    local manualAgain=meshDebug:new();assert(manualAgain:load(E.previewPath))
    assert(surface(manualAgain)==separatedSurface,'Manual radius was lost')
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
    if mbm.getTimeRun()-started>120 then print('IMAGE MESH NORMAL FAIL timeout');mbm.quit() end
end
function onEndScene()
    mbm.startImageMesh=native
    finish()
end
