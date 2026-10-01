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
function M.compare(E,enabled,camera)
    if not enabled then M.clearComparison(E);return end
    if E.normalComparison or not M.canCompare(E) then return end
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
    local offset=math.max(.001,(hi-lo)*.575)
    target:setPos(c.x+offset,c.y,c.z)
    c.preview:setPos(c.x-offset,c.y,c.z)
    c.preview.alwaysRender=true;c.preview.visible=true
    E.orbit.distance=math.max(c.distance,(hi-lo)*2.15*2.7)
    if camera then camera() end
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
    tImGui.BeginDisabled(not M.canCompare(E))
    local compare=tImGui.Checkbox(tLang.L('ime_normal_compare'),E.normalComparison~=nil)
    if compare~=(E.normalComparison~=nil) then
        local ok=safe(M.compare,E,compare,camera)
        if not ok then M.clearComparison(E) end
    end
    tImGui.EndDisabled()
    if E.normalComparison then
        tImGui.TextWrapped(tLang.L(math.cos(E.orbit.azimuth)<0 and 'ime_normal_compare_order' or 'ime_normal_compare_reverse'))
    else tImGui.TextWrapped(tLang.L('ime_normal_compare_help')) end

    local o={strength=E.values.normalMapStrength,blur=E.values.normalMapBlur,
        convention=E.values.normalMapConvention,edge=E.values.normalMapEdge}
    Panel.draw(tImGui,function(k) return tLang.L('nmg_'..k) end,o,true)
    E.values.normalMapStrength=o.strength;E.values.normalMapBlur=o.blur
    E.values.normalMapConvention=o.convention;E.values.normalMapEdge=o.edge
    if E.normalCompiled==nil then E.normalCompiled=mbm.isNormalMapping3DCompiled() end
    if not E.normalCompiled then tImGui.TextWrapped(tLang.L('nm_3d_build_disabled')) end
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
local function buildTexture(E,project,region,o)
    o=o or Model.options(project,region)
    E.normalResources=E.normalResources or {}
    local key=project.image.path..'\0'..region.id
    local record=E.normalResources[key]
    if record and same(record.options,o) then return record.path end
    E.generationCancelled=nil;E.generationCancelling=nil
    E.normalProcessing=true
    local image=processedHeight(E,project,o)
    local job=Generator.start(image,{strength=o.normalMapStrength,blur=o.normalMapBlur,
        convention=o.normalMapConvention,edge=o.normalMapEdge})
    E.normalJob=job;E.generationStage='normal'
    repeat
        cancelled(E);job:step(.006);E.generationProgress=job.progress
        coroutine.yield()
    until job.state~='running'
    E.normalJob=nil
    cancelled(E);assert(job.state=='completed',job.error)
    local bytes=atlas(job.result,project,o,E)
    record=record or {path=tUtil.getTemporaryFilePath('.png')}
    E.normalResources[key]=record
    assert(mbm.writeImagePixels(record.path,bytes,project.image.width,project.image.height))
    mbm.addPath(record.path:match('^(.*)[/\\]') or '.')
    if record.info then assert(record.info:reload(record.path))
    else record.info=assert(mbm.loadTexture(record.path)) end
    record.options=Model.copy(o)
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
function M.apply(E,asset,project,region,o)
    if o.reliefMode~='normal' then return asset end
    local subsets
    asset,subsets=require('image_mesh_normal_material').split(asset,o)
    local path=M.texture(E,project,region,o)
    for _,subset in ipairs(subsets) do
        assert(asset:setMaterialTexture(1,subset,'normal',path))
        assert(asset:setNormalMapSettings(1,subset,o.normalMapConvention,1))
        assert(asset:prepareNormalMap(1,subset,'generate'))
    end
    return asset
end
function M.refresh(E)
    E.normalDirty=nil
    local objects={}
    if E.preview then objects[#objects+1]={id=E.viewRegion,preview=E.preview} end
    if E.comparison then objects[#objects+1]={id=E.viewRegion,preview=E.comparison.preview} end
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
                local path=M.texture(E,E.project,r,o)
                for _,item in ipairs(objects) do if item.id==id then
                    for _,subset in ipairs(item.preview.imageMeshNormalSubsets) do
                        assert(item.preview:setMaterialTexture('normal',path,true,subset))
                        assert(item.preview:setNormalMapSettings(o.normalMapConvention,1,subset))
                    end
                end end
                if cached and cached.id==id and cached.asset then
                    for _,subset in ipairs(require('image_mesh_normal_material').subsets(cached.asset)) do
                        assert(cached.asset:setMaterialTexture(1,subset,'normal',path))
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
