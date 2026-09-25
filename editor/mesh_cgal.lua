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


-- Optional external backend. No CGAL library is loaded into the engine.
local M={}
local active={}
local Obj=require 'mesh_cgal_obj'
local preference=os.getenv('MBM_CGAL_CONFIG') or ((os.getenv('APPDATA') or os.getenv('HOME') or '.')..'/.mini-mbm-cgal-path')
local remeshPreference=os.getenv('MBM_CGAL_REMESH_CONFIG') or ((os.getenv('APPDATA') or os.getenv('HOME') or '.')..'/.mini-mbm-cgal-remesh-path')
local loaded,path,draft=false,'',''
local remeshLoaded,remeshPath,remeshDraft=false,'',''
local function safe(fn,...)
    local r=table.pack(pcall(fn,...))
    if not r[1] then print('[mesh_cgal] '..tostring(r[2])) end
    return table.unpack(r,1,r.n)
end
function M.getPath()
    if not loaded then
        loaded=true
        local f=io.open(preference,'r')
        if f then path=f:read('*l') or '';f:close() end
        draft=path
    end
    return path
end
function M.setPath(value,persist)
    assert(type(value)=='string' and not value:find('[%z\r\n]'),'Invalid executable path')
    if persist then
        local f,err=io.open(preference,'w');if not f then return nil,err end
        local ok,why=f:write(value,'\n');local closed,closeError=f:close()
        if not ok or not closed then return nil,why or closeError end
    end
    loaded=true;path=value;draft=value
    return true
end
function M.getRemeshPath()
    if not remeshLoaded then
        remeshLoaded=true
        local f=io.open(remeshPreference,'r')
        if f then remeshPath=f:read('*l') or '';f:close() end
        remeshDraft=remeshPath
    end
    return remeshPath
end
function M.setRemeshPath(value,persist)
    assert(type(value)=='string' and not value:find('[%z\r\n]'),'Invalid remesh executable path')
    if persist then
        local f,err=io.open(remeshPreference,'w');if not f then return nil,err end
        local ok,why=f:write(value,'\n');local closed,closeError=f:close()
        if not ok or not closed then return nil,why or closeError end
    end
    remeshLoaded=true;remeshPath=value;remeshDraft=value
    return true
end
function M.panel()
    M.getPath()
    tImGui.SetNextItemWidth(360)
    local changed,value=tImGui.InputText(tLang.L('cgal_executable'),draft,4096)
    if changed then draft=value end
    if tImGui.Button(tLang.L('cgal_browse')) then
        local picked=mbm.openFile(draft,package.config:sub(1,1)=='\\' and '*.exe' or '*')
        if picked and picked~='' then draft=picked end
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('cgal_save')) then
        local ok,err=M.setPath(draft,true)
        if not ok then tUtil.showMessageWarn(tostring(err)) else tUtil.showMessage(tLang.L('cgal_saved')) end
    end
    tImGui.TextWrapped(tLang.L('cgal_help'))
    tImGui.Separator()
    M.getRemeshPath()
    tImGui.SetNextItemWidth(360)
    changed,value=tImGui.InputText(tLang.L('cgal_remesh_executable'),remeshDraft,4096)
    if changed then remeshDraft=value end
    if tImGui.Button(tLang.L('cgal_remesh_browse')) then
        local picked=mbm.openFile(remeshDraft,package.config:sub(1,1)=='\\' and '*.exe' or '*')
        if picked and picked~='' then remeshDraft=picked end
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('cgal_remesh_save')) then
        local ok,err=M.setRemeshPath(remeshDraft,true)
        if not ok then tUtil.showMessageWarn(tostring(err)) else tUtil.showMessage(tLang.L('cgal_saved')) end
    end
    tImGui.TextWrapped(tLang.L('cgal_remesh_help'))
    tImGui.Separator()
    require('mesh_audit_ui').settings(tImGui,tLang.L)
end
function M.menu()
    if tImGui.BeginMenu(tLang.L('cgal_settings')) then M.panel();tImGui.EndMenu() end
end
local function cleanup(job)
    active[job]=nil
    if job.process then job.process:destroy();job.process=nil end
    for _,file in ipairs(job.files) do os.remove(file) end
end
local function totals(asset)
    local v,t=0,0
    for s=1,asset:getTotalSubset(1) do
        local n=asset:getTotalVertex(1,s);v=v+n
        local i=asset:getTotalIndex(1,s);t=t+(i>0 and i or n)/3
    end
    return v,t
end
local function import(job,data)
    data=data or Obj.read(job.output)
    local bySubset={}
    for _,g in ipairs(data.groups) do
        local s=tonumber(g.name:match('^subset_(%d+)$'))
        assert(s and job.sources[s] and not bySubset[s],'Unexpected output material')
        bySubset[s]=g
    end
    local result=meshDebug:new();result:setType('mesh');result:setModeDraw('TRIANGLES');result:addFrame(3)
    local total=select(1,totals(job.asset))
    local targets={}
    for s,source in pairs(job.sources) do
        local g=assert(bySubset[s],'Output lost a subset')
        targets[#targets+1]=s
        -- Recompute face normals explicitly; OBJ preserves UVs, not authored normals.
        if source.normals and not job.deferNormals then
            local vertices,indices,keys={},{},{}
            for i=1,#g.indices,3 do
                local a,b,c=g.vertices[g.indices[i]],g.vertices[g.indices[i+1]],g.vertices[g.indices[i+2]]
                local ux,uy,uz=b.x-a.x,b.y-a.y,b.z-a.z
                local vx,vy,vz=c.x-a.x,c.y-a.y,c.z-a.z
                local nx,ny,nz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
                local length=math.sqrt(nx*nx+ny*ny+nz*nz);assert(length>0,'Degenerate output triangle')
                nx,ny,nz=nx/length,ny/length,nz/length
                for _,v in ipairs({a,b,c}) do
                    local key=string.format('%.17g %.17g %.17g %.17g %.17g %.7g %.7g %.7g',v.x,v.y,v.z,v.u,v.v,nx,ny,nz)
                    local index=keys[key]
                    if not index then index=#vertices+1;keys[key]=index;vertices[index]={x=v.x,y=v.y,z=v.z,u=v.u,v=v.v,nx=nx,ny=ny,nz=nz} end
                    indices[#indices+1]=index
                end
            end
            g.vertices,g.indices=vertices,indices
        end
        total=total-job.asset:getTotalVertex(1,s)+#g.vertices
    end
    assert(total<=job.maxVertices,'CGAL result exceeds vertex limit: '..job.maxVertices)
    table.sort(targets)
    for i,s in ipairs(targets) do
        local g,source=bySubset[s],job.sources[s]
        for _,v in ipairs(g.vertices) do
            for _,key in ipairs({'x','y','z','u','v'}) do assert(v[key]==v[key] and math.abs(v[key])<math.huge,'Invalid output vertex') end
        end
        result:addSubSet(1);assert(result:addVertex(1,i,g.vertices));assert(result:addIndex(1,i,g.indices))
        if source.texture then result:setTexture(1,i,source.texture) end
        for role,texture in pairs(source.roles) do if texture and texture~='' then result:setMaterialTexture(1,i,role,texture) end end
    end
    if job.physics and #job.physics>0 then result:setPhysics(job.physics) end
    if not job.normals then result:removeNormals() end
    assert(result:check(),'Invalid CGAL geometry')
    -- Caller owns a disposable working mesh; the visible asset is committed only on success.
    for i=#targets,1,-1 do job.asset:removeSubset(1,targets[i]) end
    for i,s in ipairs(targets) do
        job.asset:copySubsetFrom(1,result,1,i)
        local current=job.asset:getTotalSubset(1)
        while current>s do assert(job.asset:moveSubsetUp(1,current));current=current-1 end
    end
    assert(job.asset:check(),'Invalid reconstructed mesh')
    local vertices,triangles=totals(job.asset)
    local report={backend=job.kind,qemRan=false,sourceVertexCount=job.sourceVertices,sourceTriangleCount=job.sourceTriangles,
        resultVertexCount=vertices,resultTriangleCount=triangles,unchanged=job.kind=='cgal' and triangles>=job.sourceTriangles,
        maximumGeometricError=job.report.sampled_bidirectional_error,maximumRelativeError=job.report.sampled_error_fraction}
    if job.kind=='remesh' then report.remesh=job.report else report.cgal=job.report end
    return report
end
function M.start(asset,subset,frame,angle,distance,maxVertices,hasNormals,deferNormals,remesh)
    local job={asset=asset,files={},sources={},state='running',started=mbm.getTimeRun(),maxVertices=math.min(maxVertices or 65535,65535),
        kind=remesh and 'remesh' or 'cgal'}
    job.deferNormals=deferNormals==true
    local ok,err=safe(function()
        local executable=job.kind=='remesh' and M.getRemeshPath() or M.getPath()
        assert(executable~='',tLang.L(job.kind=='remesh' and 'cgal_remesh_missing' or 'cgal_missing'))
        assert(asset:getTotalFrame()==1 and (not frame or frame==0 or frame==1) and asset:getModeDraw()=='TRIANGLES'
            and not asset:hasSkeletalVertexWeights() and ((asset:getSkeletonBindReport(false) or {}).boneCount or 0)==0 and asset:getTotalArticulatedParts()==0,tLang.L('cgal_static_only'))
        job.sourceVertices,job.sourceTriangles=totals(asset)
        job.physics=asset:getPhysics()
        for s=1,asset:getTotalSubset(1) do
            if not subset or s==subset then
                local source={count=asset:getTotalVertex(1,s),normals=hasNormals~=false,texture=asset:getTexture(1,s),roles={}}
                for _,role in ipairs({'normal','specular','emissive','mask'}) do source.roles[role]=asset:getMaterialTexture(1,s,role) end
                job.sources[s]=source;job.normals=source.normals
            end
        end
        assert(next(job.sources),'Empty capture')
        local input=tUtil.getTemporaryFilePath('.obj')
        job.output=input..'.result.obj';job.reportPath=input..'.report'
        job.files={input,input:sub(1,-5)..'.mtl',job.output,job.reportPath}
        Obj.export(asset,input,subset)
        local arguments
        if remesh then
            arguments={input,job.output,tostring(remesh.edgeLengthFraction),tostring(remesh.iterations),
                tostring(remesh.featureAngle),job.reportPath}
        else
            arguments={input,job.output,tostring(angle or 10),tostring(distance or .05),'0.000001',job.reportPath}
        end
        job.process=assert(mbm.executeProcessAsync({executable=executable,arguments=arguments,hidden=true}))
    end)
    if not ok then cleanup(job);return nil,err end
    -- Face-normal splits are rendering attributes, not open boundaries for QEM.
    -- Keep CGAL's shared position/UV topology until both geometric passes finish.
    function job:finalizeNormals()
        if not self.deferNormals then return true end
        local ok,value=safe(function()
            local data={groups={}}
            for subset in pairs(self.sources) do
                local vertices=self.asset:getVertex(1,subset,1,self.asset:getTotalVertex(1,subset))
                local indices=self.asset:getIndex(1,subset)
                if not indices or #indices==0 then
                    indices={};for i=1,#vertices do indices[i]=i end
                end
                data.groups[#data.groups+1]={name='subset_'..subset,vertices=vertices,indices=indices}
            end
            self.deferNormals=false
            return import(self,data)
        end)
        if not ok then return nil,value end
        return true,value.resultVertexCount
    end
    function job:cancelSimplify()
        self.cancelled=true
        if self.process then self.process:cancel() end
    end
    function job:getSimplifyStatus()
        if self.state~='running' then return self.status end
        if self.process:isRunning() then
            if mbm.getTimeRun()-self.started>300 then self:cancelSimplify();self.timeout=true end
            return {state='running',progress=0}
        end
        local code=self.process:getExitCode()
        local success,value=safe(function()
            if self.cancelled then return end
            local f=io.open(self.reportPath,'r')
            local line=''
            if f then line=f:read('*a');f:close() end
            if code~=0 then
                local detail=line:match('CGAL_FAIL ([^\r\n]+)') or ''
                error(string.format(tLang.L('cgal_exit'),tostring(code))..' '..detail,0)
            end
            local prefix=self.kind=='remesh' and '^CGAL_REMESH_RESULT ' or '^CGAL_RESULT '
            assert(line:match(prefix),tLang.L('cgal_protocol'))
            self.report={}
            for key,number in line:gmatch('([%w_]+)=([^%s]+)') do self.report[key]=tonumber(number) end
            assert(self.report.result_triangles and self.report.sampled_error_fraction and
                ((self.kind=='remesh' and self.report.charts) or (self.kind=='cgal' and self.report.uv_enabled==1)),
                tLang.L('cgal_protocol'))
            return import(self)
        end)
        cleanup(self)
        self.state=self.cancelled and 'cancelled' or (success and 'completed' or 'failed')
        if self.timeout then self.state='failed';value=tLang.L('cgal_timeout') end
        self.status={state=self.state,progress=1,report=success and value or nil,error=self.state=='failed' and value or nil}
        return self.status
    end
    active[job]=true
    return job
end
function M.startRemesh(asset,subset,frame,edgeLengthFraction,iterations,featureAngle,maxVertices,hasNormals)
    local fraction,passes,angle=tonumber(edgeLengthFraction),tonumber(iterations),tonumber(featureAngle)
    if not fraction or fraction~=fraction or fraction<=0 or fraction>.25 then return nil,'Invalid target edge-length fraction' end
    if not passes or passes%1~=0 or passes<1 or passes>10 then return nil,'Invalid remesh iteration count' end
    if not angle or angle~=angle or angle<0 or angle>180 then return nil,'Invalid feature angle' end
    return M.start(asset,subset,frame,nil,nil,maxVertices,hasNormals,false,
        {edgeLengthFraction=fraction,iterations=passes,featureAngle=angle})
end
function M.shutdown()
    for job in pairs(active) do job:cancelSimplify();cleanup(job) end
end
return M
