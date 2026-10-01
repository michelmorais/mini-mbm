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

local Model=require 'image_mesh_model'
local Height=require 'height_map_source'
local Generator=require 'normal_map_generator'
local Panel=require 'normal_map_panel'
local M={}
local function same(a,b)
    if type(a)~=type(b) then return false end
    if type(a)~='table' then return a==b end
    for k,v in pairs(a) do if not same(v,b[k]) then return false end end
    for k in pairs(b) do if a[k]==nil then return false end end
    return true
end
function M.sameGeometry(a,b)
    if not a or not same(a.image,b.image) or #a.regions~=#b.regions then return false end
    for i,r in ipairs(a.regions) do
        local nextRegion=b.regions[i]
        local oldOptions,newOptions=Model.options(a,r),Model.options(b,nextRegion)
        if r.id~=nextRegion.id or oldOptions.reliefMode~=newOptions.reliefMode
            or not same(Model.geometryOptions(oldOptions),Model.geometryOptions(newOptions)) then return false end
    end
    return true
end
-- Preview-only resources; never written back to the project or exported asset.
function M.clearComparison(E)
    local c=E.normalComparison
    if not c then return end
    if c.preview then meshDebug:loadMeshPreview(c.preview,nil);c.preview:destroy() end
    if c.additive then meshDebug:loadMeshPreview(c.additive,nil);c.additive:destroy() end
    if E.preview==c.target then E.preview:setPos(c.x,c.y,c.z) end
    E.orbit.distance=c.distance
    E.normalComparison=nil
    if c.camera then c.camera() end
end
function M.canCompare(E)
    return E.values.reliefMode=='normal' and E.previewNormalMode and E.preview~=nil and not E.editMode
        and not E.dirty and not E.previewStale and not E.meshTask and not E.normalDirty
        and not E.wireframe and not E.compareSideBySide and not (E.assembly and E.assembly.enabled)
end
function M.compare(E,enabled,camera,prepared)
    if not enabled then M.clearComparison(E);return end
    if E.normalComparison or (not prepared and not M.canCompare(E)) then return end
    local additivePath,sidePath,options
    if E.values.normalMapResidual then additivePath,sidePath,options=M.comparisonTextures(E) end
    local asset=meshDebug:new();assert(asset:load(E.previewPath))
    local lo,hi=math.huge,-math.huge
    for _,v in ipairs(require('image_mesh_asset').vertices(asset)) do
        lo=math.min(lo,v.x);hi=math.max(hi,v.x)
    end
    local target=E.preview
    local c={target=target,x=target.x,y=target.y,z=target.z,distance=E.orbit.distance,camera=camera}
    E.normalComparison=c -- Allows cleanup even if loading fails.
    c.preview=mesh:new('3d');c.preview.visible=false
    assert(meshDebug:loadMeshPreview(c.preview,E.previewPath))
    for _,subset in ipairs(require('image_mesh_normal_material').subsets(asset)) do
        assert(c.preview:setNormalMapSettings('+Y',0,subset))
    end
    if additivePath then
        c.additive=mesh:new('3d');c.additive.visible=false
        assert(meshDebug:loadMeshPreview(c.additive,E.previewPath))
        for _,subset in ipairs(require('image_mesh_normal_material').subsets(asset)) do
            assert(c.additive:setMaterialTexture('normal',subset==1 and additivePath or sidePath,true,subset))
            assert(c.additive:setNormalMapSettings(options.normalMapConvention,1,subset))
        end
        c.additive:setPos(c.x,c.y,c.z)
        c.additive.alwaysRender=true;c.additive.visible=true
    end
    local offset=math.max(.001,(hi-lo)*(c.additive and 1.15 or .575))
    target:setPos(c.x+offset,c.y,c.z)
    c.preview:setPos(c.x-offset,c.y,c.z)
    c.preview.alwaysRender=true;c.preview.visible=true
    E.orbit.distance=math.max(c.distance,(hi-lo)*(c.additive and 3.3 or 2.15)*2.7)
    if camera then camera() end
end
-- Texture generation can yield; the UI schedules it through the existing job runner.
function M.requestComparison(E,enabled,camera,safe)
    if not enabled then M.clearComparison(E);return end
    if E.normalComparison or not M.canCompare(E) then return end
    require('image_mesh_simplify').run(E,function()
        local ok=safe(M.compare,E,true,camera,true)
        if not ok then M.clearComparison(E) end
    end)
end
function M.syncComparison(E)
    local c=E.normalComparison
    if c and (c.target~=E.preview or not M.canCompare(E)) then M.clearComparison(E) end
end
function M.panel(E,camera,safe)
    local enabled=tImGui.Checkbox(tLang.L('ime_normal_enabled'),E.values.reliefMode=='normal')
    E.values.reliefMode=enabled and 'normal' or 'geometry'
    if not enabled then M.clearComparison(E);return end
    tImGui.TextWrapped(tLang.L('ime_normal_workflow'))
    tImGui.TextWrapped(tLang.L('ime_normal_help'))
    E.values.normalMapResidual=tImGui.Checkbox(tLang.L('ime_normal_residual'),E.values.normalMapResidual)
    if E.values.normalMapResidual then
        E.values.normalMapBasis=tImGui.Checkbox(tLang.L('ime_normal_basis'),E.values.normalMapBasis)
        tImGui.TextWrapped(tLang.L(E.values.normalMapBasis and 'ime_normal_basis_help' or 'ime_normal_residual_help'))
        if E.values.normalMapBasis then
            local available=Model.canSeparateDetail(E.values)
            tImGui.BeginDisabled(not available)
            if E.values.normalMapAutomatic then
                tImGui.TextWrapped(tLang.L('ime_target_separation_owned'))
            else
                local changed,radius=tImGui.SliderInt(tLang.L('ime_normal_separation'),E.values.normalMapGeometryBlur,0,32)
                if changed then E.values.normalMapGeometryBlur=radius end
            end
            tImGui.EndDisabled()
            tImGui.TextWrapped(tLang.L(available and 'ime_normal_separation_help' or 'ime_normal_separation_unavailable'))
        end
    end
    local o={strength=E.values.normalMapStrength,blur=E.values.normalMapBlur,
        convention=E.values.normalMapConvention,edge=E.values.normalMapEdge}
    Panel.draw(tImGui,function(k) return tLang.L('nmg_'..k) end,o,true)
    E.values.normalMapStrength=o.strength;E.values.normalMapBlur=o.blur
    E.values.normalMapConvention=o.convention;E.values.normalMapEdge=o.edge
    if E.normalCompiled==nil then E.normalCompiled=mbm.isNormalMapping3DCompiled() end
    if not E.normalCompiled then tImGui.TextWrapped(tLang.L('nm_3d_build_disabled')) end
    tImGui.Separator()
    tImGui.BeginDisabled(not M.canCompare(E))
    local compare=tImGui.Checkbox(tLang.L('ime_normal_compare'),E.normalComparison~=nil)
    if compare~=(E.normalComparison~=nil) then
        local ok=safe(M.requestComparison,E,compare,camera,safe)
        if not ok then M.clearComparison(E) end
    end
    tImGui.EndDisabled()
    if E.normalComparison then
        local prefix=E.normalComparison.additive and 'ime_normal_compare_three_' or 'ime_normal_compare_'
        tImGui.TextWrapped(tLang.L(prefix..(math.cos(E.orbit.azimuth)<0 and 'order' or 'reverse')))
    else tImGui.TextWrapped(tLang.L('ime_normal_compare_help')) end
end
local function cancelled(E)
    if not E.generationCancelling then return end
    E.generationCancelled=true;E.batch=nil;error('ime_generation_cancelled',0)
end
local function processedHeight(E,project,o)
    -- The core has one map worker. Retire the 2D height preview before borrowing it.
    if E.heightJob then
        E.heightJob.job:cancel()
        while E.heightJob.job:getStatus().state=='running' do coroutine.yield() end
        E.heightJob.job:close();os.remove(E.heightJob.path);E.heightJob=nil
    end
    local path=tUtil.getTemporaryFilePath('.png')
    E.normalHeightPath=path
    local job,err=mbm.startImageMeshMap(project.image.path,o,path,false)
    assert(job,err);E.normalHeightJob=job;E.imageJob=job
    coroutine.yield()
    while true do
        local status=job:getStatus();E.generationProgress=status.progress;E.generationStage=status.stage
        if status.state~='running' then
            E.imageJob=nil;E.normalHeightJob=nil
            local ok,message
            if status.state=='completed' then ok,message=job:takeResult() end
            job:close()
            if not ok then
                os.remove(path);E.normalHeightPath=nil
                cancelled(E);error(message or status.error or 'ime_generation_cancelled',0)
            end
            break
        end
        coroutine.yield()
    end
    cancelled(E)
    local bytes,w,h=mbm.readImagePixels(path);os.remove(path);E.normalHeightPath=nil
    assert(bytes,w)
    assert(w==o.cropWidth and h==o.cropHeight,'Unexpected height map dimensions')
    local alpha,aw,ah=mbm.readImagePixels(project.image.path,'alpha');assert(alpha,aw)
    assert(aw==project.image.width and ah==project.image.height,'Source dimensions changed')
    local rows={}
    for y=1,h do
        local row={}
        for x=1,w do
            local p=((y-1)*w+x-1)*4+1
            local a=math.min(bytes:byte(p+3),alpha:byte((o.y+y-1)*aw+o.x+x))
            row[x]=bytes:sub(p,p+2)..string.char(a)
        end
        rows[y]=table.concat(row)
        if y%8==0 then coroutine.yield();cancelled(E) end
    end
    return Height.image(table.concat(rows),w,h)
end
local function atlas(result,project,o,E)
    local width,height=project.image.width,project.image.height
    local neutral=string.char(128,128,255,255)
    local blank=string.rep(neutral,width)
    local prefix,suffix=string.rep(neutral,o.x),string.rep(neutral,width-o.x-result.width)
    local rows={}
    for y=1,height do
        if y>o.y and y<=o.y+result.height then
            local offset=(y-o.y-1)*result.width*4
            rows[y]=prefix..result.bytes:sub(offset+1,offset+result.width*4)..suffix
        else rows[y]=blank end
        if y%32==0 then coroutine.yield();cancelled(E) end
    end
    return table.concat(rows)
end
-- Repeated sides derive detail from their own diffuse image, not front height edits.
local function repeatedImage(E,project,o)
    local external=o.sideTexture and o.sideTexture~=''
    local bytes,w,h=mbm.readImagePixels(external and o.sideTexture or project.image.path)
    assert(bytes,w)
    if external then return Height.image(bytes,w,h),true end
    local rows={}
    for y=1,o.cropHeight do
        local start=((o.y+y-1)*w+o.x)*4+1
        rows[y]=bytes:sub(start,start+o.cropWidth*4-1)
        if y%32==0 then coroutine.yield();cancelled(E) end
    end
    return Height.image(table.concat(rows),o.cropWidth,o.cropHeight),false
end
local function buildTexture(E,project,region,o,side,geometry)
    o=o or Model.options(project,region)
    E.normalResources=E.normalResources or {}
    local residual=o.normalMapResidual and not side
    if residual then assert(geometry,'Missing geometry for residual normal map') end
    local key=project.image.path..'\0'..region.id..(side and ':'..side or (residual and ':residual:'..geometry.role or ':front'))
    local record=E.normalResources[key]
    if record and same(record.options,o) and (not residual or record.geometry==geometry) then return record.path end
    E.generationCancelled=nil;E.generationCancelling=nil
    E.normalProcessing=true
    local image,external
    if side=='repeat' then image,external=repeatedImage(E,project,o)
    else image=processedHeight(E,project,o) end
    local settings={strength=o.normalMapStrength,blur=o.normalMapBlur,
        convention=o.normalMapConvention,edge=side=='repeat' and 'repeat' or o.normalMapEdge}
    local job
    if residual then
        local scale=o.relief
        if o.heightSource=='curved' then scale=select(2,Model.curved.range(o))/(o.curvedSymmetric and 2 or 1) end
        local domain={imageWidth=project.image.width,imageHeight=project.image.height,
            x=o.x,y=o.y,width=o.width,height=o.height,scale=scale}
        job=Generator.job(function(tick)
            if o.normalMapBasis then
                return require('normal_map_baker').generate(image,geometry.vertices,geometry.indices,geometry.corners,domain,settings,tick)
            end
            return require('normal_map_residual').generate(image,geometry.vertices,geometry.indices,domain,settings,tick)
        end)
    else job=Generator.start(image,settings) end
    E.normalJob=job;E.generationStage='normal'
    repeat
        cancelled(E);job:step(.006);E.generationProgress=job.progress
        coroutine.yield()
    until job.state~='running'
    E.normalJob=nil
    cancelled(E);assert(job.state=='completed',job.error)
    local bytes=external and job.result.bytes or atlas(job.result,project,o,E)
    local width=external and job.result.width or project.image.width
    local height=external and job.result.height or project.image.height
    record=record or {path=tUtil.getTemporaryFilePath('.png')}
    E.normalResources[key]=record
    assert(mbm.writeImagePixels(record.path,bytes,width,height))
    mbm.addPath(record.path:match('^(.*)[/\\]') or '.')
    if record.info then assert(record.info:reload(record.path))
    else record.info=assert(mbm.loadTexture(record.path)) end
    record.options=Model.copy(o);record.geometry=residual and geometry or nil
    E.normalProcessing=nil;E.generationProgress=nil;E.generationStage=nil
    E.normalBuilds=(E.normalBuilds or 0)+1
    return record.path
end
function M.cancelWork(E)
    if E.normalHeightJob then E.normalHeightJob:cancel();E.normalHeightJob:close();E.normalHeightJob=nil;E.imageJob=nil end
    if E.normalHeightPath then os.remove(E.normalHeightPath);E.normalHeightPath=nil end
    if E.normalJob then E.normalJob:cancel();E.normalJob=nil end
    E.normalProcessing=nil;E.generationStage=nil;E.generationProgress=nil
end
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[image_mesh_normal_map] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end
function M.texture(E,...)
    local ok,result=dpCall(buildTexture,E,...)
    if not ok then M.cancelWork(E);error(result,0) end
    return result
end
local function paths(E,project,region,o,geometry)
    local path=M.texture(E,project,region,o,nil,geometry)
    local sidePath=path
    if o.sideMode=='repeat' then sidePath=M.texture(E,project,region,o,'repeat')
    elseif o.sideMode=='band' and o.normalMapResidual then sidePath=M.texture(E,project,region,o,'band') end
    return path,sidePath
end
function M.comparisonTextures(E)
    local region=assert(Model.region(E.project,E.viewRegion))
    local o=Model.options(E.project,region)
    o.normalMapResidual=false
    local path,sidePath=paths(E,E.project,region,o)
    return path,sidePath,o
end
function M.apply(E,asset,project,region,o,role)
    if o.reliefMode~='normal' then return asset end
    local subsets
    asset,subsets=require('image_mesh_normal_material').split(asset,o)
    -- Retain a snapshot for appearance-only rebakes, including additive -> residual.
    asset.imageMeshNormalGeometry={vertices=asset:getVertex(1,1,1,asset:getTotalVertex(1,1)),
        indices=asset:getIndex(1,1),corners=assert(asset:getNormalMapCorners(1,1)),role=role or 'final'}
    local path,sidePath=paths(E,project,region,o,asset.imageMeshNormalGeometry)
    for _,subset in ipairs(subsets) do
        assert(asset:setMaterialTexture(1,subset,'normal',subset==1 and path or sidePath))
        assert(asset:setNormalMapSettings(1,subset,o.normalMapConvention,1))
        assert(asset:prepareNormalMap(1,subset,subset==1 and 'preserve' or 'generate'))
    end
    return asset
end
-- Prepare the original only when its geometry comparison is requested.
function M.prepareComparison(E)
    local source=E.comparison
    if not source then return end
    local region=assert(Model.region(E.project,E.viewRegion))
    local o=Model.options(E.project,region)
    if o.reliefMode~='normal' or same(source.normalOptions,o) then return end
    local preview=source.preview
    if not preview.imageMeshNormalGeometry then
        local asset=meshDebug:new();assert(asset:load(source.previewPath),tLang.L('ime_preview_failed'))
        asset=M.apply(E,asset,E.project,region,o,'original')
        local path=tUtil.getTemporaryFilePath('.msh')
        local ok,err=dpCall(function()
            assert(require('image_mesh_texture_aliases').savePreview(asset,path,E.path or E.project.image.path),tLang.L('ime_export_failed'))
            assert(meshDebug:loadMeshPreview(preview,path),tLang.L('ime_preview_failed'))
        end)
        if not ok then os.remove(path);error(err,0) end
        os.remove(source.previewPath);source.previewPath=path
        preview.imageMeshNormalGeometry=asset.imageMeshNormalGeometry
        preview.imageMeshNormalSubsets=require('image_mesh_normal_material').subsets(asset)
    else
        local path,sidePath=paths(E,E.project,region,o,preview.imageMeshNormalGeometry)
        for _,subset in ipairs(preview.imageMeshNormalSubsets) do
            assert(preview:setMaterialTexture('normal',subset==1 and path or sidePath,true,subset))
            assert(preview:setNormalMapSettings(o.normalMapConvention,1,subset))
        end
    end
    source.normalOptions=Model.copy(o)
end
function M.refresh(E)
    E.normalDirty=nil
    local objects={}
    if E.preview then objects[#objects+1]={id=E.viewRegion,preview=E.preview} end
    if E.comparison and E.compareSideBySide then M.prepareComparison(E) end
    for _,item in ipairs(E.assembly and E.assembly.items or {}) do objects[#objects+1]=item end
    local regions={}
    for _,item in ipairs(objects) do regions[item.id]=true end
    local cached=E.generatedMesh
    if cached then regions[cached.id]=true end
    for id in pairs(regions) do
        local r=Model.region(E.project,id)
        if r then
            local o=Model.options(E.project,r)
            if o.reliefMode=='normal' then
                for _,item in ipairs(objects) do if item.id==id then
                    local path,sidePath=paths(E,E.project,r,o,item.preview.imageMeshNormalGeometry)
                    for _,subset in ipairs(item.preview.imageMeshNormalSubsets) do
                        assert(item.preview:setMaterialTexture('normal',subset==1 and path or sidePath,true,subset))
                        assert(item.preview:setNormalMapSettings(o.normalMapConvention,1,subset))
                    end
                end end
                if cached and cached.id==id and cached.asset then
                    local path,sidePath=paths(E,E.project,r,o,cached.asset.imageMeshNormalGeometry)
                    for _,subset in ipairs(require('image_mesh_normal_material').subsets(cached.asset)) do
                        assert(cached.asset:setMaterialTexture(1,subset,'normal',subset==1 and path or sidePath))
                        assert(cached.asset:setNormalMapSettings(1,subset,o.normalMapConvention,1))
                    end
                end
            end
        end
    end
    if E.assembly then E.assembly.revision=E.revision end
end
function M.shutdown(E)
    M.cancelWork(E)
    for _,record in pairs(E.normalResources or {}) do
        if record.info then record.info:release() end
        os.remove(record.path)
    end
    E.normalResources=nil;E.normalProcessing=nil;E.normalDirty=nil
end
return M
