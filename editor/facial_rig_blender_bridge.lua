--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Facial rig Blender bridge.

   This module exports a deterministic Lua interchange document. Blender-side conversion is a
   later step; this file is deliberately independent from viewport objects and mesh mutation.
]]--

local M={}

function M.bindingReport(project, rig, binding)
    local report={valid=rig and rig.valid==true and #(binding and binding.roles or {})>0,
        entries={},errors={},warnings={}}
    if not rig or rig.valid~=true then
        report.errors[#report.errors+1]='canonical rig description is not valid'
    end
    for _,row in ipairs(rig and rig.roleBindings or {}) do
        local maskCount=#(row.maskIndices or {})
        local status=row.object~='' and 'object' or maskCount>0 and 'mask' or 'unassigned'
        report.entries[#report.entries+1]={index=row.index,name=row.name,object=row.object or '',
            maskCount=maskCount,binding=row.binding,status=status}
        if status=='unassigned' then
            report.warnings[#report.warnings+1]='role has no source object or mask: ' .. tostring(row.name)
        end
    end
    if #report.entries==0 then
        report.errors[#report.errors+1]='no enabled facial roles are available'
    end
    return report
end

function M.buildPayload(project, topology, rig, binding)
    if type(project)~='table' or type(rig)~='table' or type(binding)~='table' then
        return nil,'invalid facial rig data'
    end
    local report=M.bindingReport(project,rig,binding)
    if not report.valid then return nil,report end
    local payload={schema='mini-mbm-facial-rig-bridge-1',coordinateSystem='mini-mbm-3d-y-up',
        meshPath=project.meshPath or '',meshTopologySignature=topology and topology.signature or
            project.meshTopologySignature or '',vertexCount=topology and topology.vertexCount or
            project.meshVertexCount or 0,rig=rig,binding=binding,report=report}
    return payload
end

function M.save(path, payload, serializer)
    if type(path)~='string' or path=='' then return false,'output path is missing' end
    if type(payload)~='table' or type(serializer)~='function' then return false,'invalid bridge payload' end
    local temporary=path .. '.tmp'
    local lines={'-- mini-mbm facial rig Blender bridge', 'local facialRigBridge = {}'}
    serializer('facialRigBridge',payload,lines)
    lines[#lines+1]='return facialRigBridge'
    local file,errorMessage=io.open(temporary,'w')
    if not file then return false,errorMessage end
    local ok,writeError=file:write(table.concat(lines,'\n'),'\n')
    file:close()
    if not ok then os.remove(temporary); return false,writeError end
    local backup=path .. '.bak'
    os.remove(backup)
    local existing=io.open(path,'rb')
    if existing then existing:close() end
    if existing and not os.rename(path,backup) then
        os.remove(temporary)
        return false,'could not stage the previous bridge document'
    end
    local renamed,renameError=os.rename(temporary,path)
    if not renamed then
        if existing then os.rename(backup,path) end
        os.remove(temporary)
        return false,renameError
    end
    os.remove(backup)
    return true
end

function M.load(path)
    local chunk,errorMessage=loadfile(path)
    if not chunk then return nil,errorMessage end
    local ok,payload=pcall(chunk)
    if not ok then return nil,payload end
    if type(payload)~='table' or payload.schema~='mini-mbm-facial-rig-bridge-1' then
        return nil,'invalid facial rig bridge document'
    end
    return payload
end

return M
