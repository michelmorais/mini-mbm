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


-- Shared UI; all geometry/file work is initiated by a button, never by idle drawing.
local Audit=require 'mesh_audit'
local M={}
local pending={}
function M.settings(I,L,notify)
    require('mesh_cgal').panel()
end
function M.update()
    for state in pairs(pending) do
        local status=state.job:getStatus()
        if status.state~='running' then
            state.status=status;state.json=state.job.json;state.job=nil;pending[state]=nil
        end
    end
end
function M.start(state,source,options)
    if state.job then return false end
    state.source=source
    local asset=source
    if type(source)=='function' then
        local ok,value=pcall(source)
        if not ok then state.status={state='failed',error=tostring(value)};return false end
        asset=value
    end
    local job,err
    if type(asset)=='string' and asset:lower():match('%.msh$') then
        local ok,value=pcall(function()
            local snapshot=meshDebug:new();assert(snapshot:load(asset),'Cannot load audit MSH')
            return snapshot
        end)
        if not ok then state.status={state='failed',error=tostring(value)};return false end
        asset=value
    end
    if type(asset)=='string' then job,err=Audit.startFile(asset,options)
    else job,err=Audit.startMesh(asset,options) end
    state.status=job and {state='running'} or {state='failed',error=tostring(err)}
    state.job=job;state.json=nil
    if job then pending[state]=true end
    return job~=nil
end
function M.draw(state,source,id,I,L,disabled,treeOwner,openTree)
    if state.source~=source then
        if state.job then state.job:destroy();state.job=nil;pending[state]=nil end
        state.source=source;state.status=nil;state.json=nil;state.exportError=nil
    end
    local open
    if openTree then open=openTree(treeOwner,'audit',L('audit_title'),0,'audit-'..id)
    else open=I.TreeNode(L('audit_title')..'##audit-'..id) end
    if not open then return end
    I.TextWrapped(L('audit_snapshot'))
    I.BeginDisabled(state.job~=nil or disabled==true or source==nil)
    if I.Button(L('audit_run')..'##audit-run-'..id) then M.start(state,source,{selfIntersections=state.intersections~=false,printJson=state.printJson==true}) end
    I.SameLine()
    state.intersections=I.Checkbox(L('audit_intersections')..'##audit-self-'..id,state.intersections~=false)
    if I.IsItemHovered() and I.BeginTooltip() then
        I.PushTextWrapPos(420)
        I.Text(L('audit_intersections_tooltip'))
        I.PopTextWrapPos()
        I.EndTooltip()
    end
    state.printJson=I.Checkbox(L('audit_print_json')..'##audit-json-'..id,state.printJson==true)
    I.EndDisabled()
    if state.job then
        I.Text(L('audit_running'))
        if I.Button(L('audit_cancel')..'##audit-cancel-'..id) then state.job:cancel();M.update() end
    end
    local status=state.status
    if status and status.state=='failed' then I.TextWrapped(status.error)
    elseif status and status.state=='cancelled' then I.Text(L('audit_cancelled'))
    elseif status and status.report then
        local r=status.report
        local function value(v)
            if v==nil then return L('audit_unavailable') end
            if type(v)=='boolean' then return L(v and 'audit_yes' or 'audit_no') end
            if type(v)=='number' then return string.format('%.7g',v) end
            return tostring(v)
        end
        for _,key in ipairs{'vertices','triangles','surface_area','diagonal','components','boundary_edges',
            'valid_polygon_mesh','closed','degenerate_triangles','duplicate_faces','duplicate_positions',
            'isolated_vertices','non_manifold_edges','orientation_conflicts','triangle_quality_min','minimum_angle_degrees',
            'edge_length_min','edge_length_mean','edge_length_max'} do
            I.Text(L('audit_'..key)..': '..value(r[key]))
        end
        I.TextWrapped(L('audit_intersections')..': '..(r.self_intersections_status=='completed' and value(r.has_self_intersections) or L('audit_'..r.self_intersections_status)))
        I.TextWrapped(L('audit_attributes_notice'))
        if state.json and I.Button(L('audit_export')..'##audit-export-'..id) then
            local path=mbm.saveFile('mesh-audit.json','json')
            if path then
                local f,err=io.open(path,'wb')
                if f then local ok,why=f:write(state.json);local closed,closeError=f:close();err=not ok and why or not closed and closeError or nil end
                state.exportError=err and tostring(err) or nil
            end
        end
        if state.exportError then I.TextWrapped(state.exportError) end
    end
    I.TreePop()
end
function M.shutdown()
    for state in pairs(pending) do state.job:destroy();state.job=nil;state.status={state='cancelled'} end
    pending={}
end
return M
