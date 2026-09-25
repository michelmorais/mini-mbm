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


-- Public, optional offline API. Does not load CGAL into the engine or mutate meshes.
local M={}
local active={}
local configured
local preference=os.getenv('MBM_CGAL_AUDIT_CONFIG') or
    ((os.getenv('APPDATA') or os.getenv('HOME') or '.')..'/.mini-mbm-cgal-audit-path')
local function protect(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[mesh_audit] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end
function M.getPath()
    if configured==nil then
        configured=os.getenv('MBM_CGAL_AUDIT_EXECUTABLE') or ''
        if configured=='' then
            local f=io.open(preference,'r')
            if f then configured=f:read('*l') or '';f:close() end
        end
    end
    return configured
end
function M.setPath(path,persist)
    assert(type(path)=='string' and not path:find('[%z\r\n]'),'Invalid audit executable path')
    if persist then
        local f,err=io.open(preference,'w');if not f then return nil,err end
        local ok,why=f:write(path,'\n');local closed,closeError=f:close()
        if not ok or not closed then return nil,why or closeError end
    end
    configured=path
    return true
end
-- The v1 wire format is a flat JSON object containing scalar values only.
-- Decode data, never execute it as Lua. Null fields are absent in the Lua table;
-- the corresponding *_status field describes why a measurement is unavailable.
function M.decodeReport(json)
    assert(type(json)=='string' and #json<=1024*1024,'Invalid audit JSON size')
    local pos,result,seen=1,{},{}
    local function whitespace() local _,last=json:find('^%s*',pos);pos=(last or pos-1)+1 end
    local function stringValue()
        assert(json:sub(pos,pos)=='"','Expected JSON string');pos=pos+1
        local out={}
        while pos<=#json do
            local c=json:sub(pos,pos);pos=pos+1
            if c=='"' then return table.concat(out) end
            assert(c:byte()>=32,'Invalid JSON control character')
            if c=='\\' then
                local escape=json:sub(pos,pos);pos=pos+1
                local escapes={['"']='"',['\\']='\\',['/']='/',b='\b',f='\f',n='\n',r='\r',t='\t'}
                if escape=='u' then
                    local hex=json:sub(pos,pos+3);assert(hex:match('^%x%x%x%x$'),'Invalid Unicode escape')
                    local code=tonumber(hex,16);assert(code<0xD800 or code>0xDFFF,'Unsupported surrogate escape')
                    c=utf8.char(code);pos=pos+4
                else c=assert(escapes[escape],'Invalid JSON escape') end
            end
            out[#out+1]=c
        end
        error('Unterminated JSON string')
    end
    whitespace();assert(json:sub(pos,pos)=='{','Expected JSON object');pos=pos+1;whitespace()
    if json:sub(pos,pos)~='}' then
        while true do
            local key=stringValue();assert(not seen[key],'Duplicate JSON key');seen[key]=true
            whitespace();assert(json:sub(pos,pos)==':','Expected colon');pos=pos+1;whitespace()
            local value
            if json:sub(pos,pos)=='"' then value=stringValue()
            else
                local token=json:match('^([^,%s}]+)',pos);assert(token,'Expected JSON value');pos=pos+#token
                if token=='true' then value=true elseif token=='false' then value=false
                elseif token~='null' then
                    local mantissa=token:match('^(.-)[eE]([+-]?%d+)$')
                    mantissa=mantissa or token
                    local integer=mantissa:match('^(-?%d+)%.(%d+)$')
                    integer=integer or mantissa:match('^(-?%d+)$')
                    assert(integer and not integer:match('^-?0%d'),'Invalid JSON scalar')
                    value=tonumber(token);assert(value and value==value and math.abs(value)<math.huge,'Invalid JSON number')
                end
            end
            result[key]=value;whitespace()
            local separator=json:sub(pos,pos)
            if separator=='}' then break end
            assert(separator==',','Expected JSON separator');pos=pos+1;whitespace()
        end
    end
    assert(json:sub(pos,pos)=='}','Expected object end');pos=pos+1;whitespace();assert(pos>#json,'Trailing JSON data')
    assert(result.schema_version==1 and result.operation=='mesh_audit','Unsupported audit protocol')
    assert(result.status=='completed' or result.status=='failed','Invalid audit status')
    if result.status=='failed' then assert(type(result.error)=='string','Missing audit error') end
    if result.status=='completed' then
        assert(type(result.vertices)=='number' and type(result.triangles)=='number' and
            type(result.valid_polygon_mesh)=='boolean' and type(result.self_intersections_status)=='string','Incomplete audit report')
        local check=result.self_intersections_status
        assert(check=='completed' or check=='not_requested' or check=='skipped_invalid_topology' or
            check=='skipped_non_triangles' or check=='skipped_degenerate_triangles','Invalid check status')
        if check=='completed' then assert(type(result.has_self_intersections)=='boolean','Missing intersection result')
        else assert(result.has_self_intersections==nil,'Unexpected intersection result') end
        for _,key in ipairs{'vertices','triangles'} do
            assert(result[key]>=0 and result[key]%1==0,'Invalid audit count')
        end
    end
    return result
end
local function cleanup(job)
    active[job]=nil
    if job.process then
        if job.process:isRunning() then job.process:cancel() end
        job.process:destroy();job.process=nil
    end
    for _,path in ipairs(job.files) do os.remove(path) end
end
local function launch(source,options,files)
    local base=os.tmpname();os.remove(base)
    local reportPath=base..'.audit.json';files[#files+1]=reportPath
    local job={files=files,reportPath=reportPath,status={state='running'},started=mbm.getTimeRun(),timeout=options.timeout or 300}
    local ok,err=protect(function()
        local executable=options.executable or M.getPath()
        assert(type(executable)=='string' and executable~='','Configure mbm-cgal-audit executable')
        assert(type(job.timeout)=='number' and job.timeout>0 and job.timeout<math.huge,'Invalid audit timeout')
        local args={source,'--report',reportPath}
        if options.selfIntersections==false then args[#args+1]='--skip-self-intersections' end
        job.process=assert(mbm.executeProcessAsync({executable=executable,arguments=args,hidden=true}))
    end)
    if not ok then cleanup(job);return nil,err end
    function job:getStatus()
        if self.status.state~='running' then return self.status end
        if self.process:isRunning() then
            if mbm.getTimeRun()-self.started>self.timeout then
                self.status={state='failed',error='Mesh audit timed out'};cleanup(self)
            end
            return self.status
        end
        local code=self.process:getExitCode()
        local success,report=protect(function()
            local f=io.open(self.reportPath,'rb');assert(f,'Audit report missing (exit '..tostring(code)..')')
            local json=f:read(1024*1024+1);f:close()
            local decoded=M.decodeReport(json)
            assert(code==0 and decoded.status=='completed',decoded.error or ('Audit exit '..tostring(code)))
            self.json=json
            return decoded
        end)
        self.status=success and {state='completed',report=report} or {state='failed',error=tostring(report)}
        cleanup(self);return self.status
    end
    function job:cancel()
        if self.status.state=='running' then self.status={state='cancelled'};cleanup(self) end
        return self.status
    end
    function job:destroy() self:cancel();cleanup(self) end
    active[job]=true
    return job
end
function M.startFile(source,options)
    options=options or {}
    if type(source)~='string' or source=='' then return nil,'Invalid audit source path' end
    return launch(source,options,{})
end
function M.startMesh(asset,options)
    options=options or {}
    local base=os.tmpname();os.remove(base)
    local path=base..'.audit.obj'
    local f
    local ok,err=protect(function()
        local frame=options.frame or 1
        assert(type(frame)=='number' and frame%1==0 and frame>=1 and frame<=asset:getTotalFrame(),'Invalid audit frame')
        assert(asset:getModeDraw()=='TRIANGLES','Audit snapshot requires TRIANGLES')
        local subset=options.subset
        assert(not subset or (type(subset)=='number' and subset%1==0 and subset>=1 and subset<=asset:getTotalSubset(frame)),'Invalid audit subset')
        f=assert(io.open(path,'w'))
        local offset=0
        for s=subset or 1,subset or asset:getTotalSubset(frame) do
            local count=asset:getTotalVertex(frame,s)
            local vertices=count>0 and asset:getVertex(frame,s,1,count) or {}
            for _,v in ipairs(vertices) do
                for _,k in ipairs{'x','y','z'} do assert(type(v[k])=='number' and v[k]==v[k] and math.abs(v[k])<math.huge,'Non-finite position') end
                assert(f:write(string.format('v %.17g %.17g %.17g\n',v.x,v.y,v.z)))
            end
            local indices=asset:getIndex(frame,s)
            if not indices or #indices==0 then indices={};for i=1,count do indices[i]=i end end
            assert(#indices%3==0,'Incomplete triangle indices')
            for i=1,#indices,3 do
                for j=i,i+2 do assert(indices[j]>=1 and indices[j]<=count and indices[j]%1==0,'Invalid vertex index') end
                assert(f:write(string.format('f %d %d %d\n',offset+indices[i],offset+indices[i+1],offset+indices[i+2])))
            end
            offset=offset+count
        end
        assert(f:close());f=nil
    end)
    if not ok then if f then f:close() end;os.remove(path);return nil,err end
    return launch(path,options,{path})
end
function M.shutdown()
    local jobs={};for job in pairs(active) do jobs[#jobs+1]=job end
    for _,job in ipairs(jobs) do job:destroy() end
end
return M
