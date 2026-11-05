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
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Open facial capture adapter UI and deterministic file replay integration.
]]--

local Mapping=require 'facial_capture_mapping'
local Stream=require 'facial_capture_stream'
local M={}

function M.new()
    return {path='',replay=nil,mapping=nil,playing=false,loop=false,
        resetOnDisconnect=true,lastResult=nil,lastPacket=nil,lastError=nil,status='stopped'}
end

function M.stop(capture,mesh,dpCall,resetWeights)
    if not capture then return end
    capture.playing=false
    capture.status='stopped'
    if not capture.replay then return end
    capture.replay.resetOnDisconnect=resetWeights==true or capture.resetOnDisconnect==true
    if mesh and capture.mapping then
        dpCall(function() capture.replay:stop(mesh,capture.mapping) end)
    else
        capture.replay:stop()
    end
end

function M.reset(capture,mesh,dpCall)
    M.stop(capture,mesh,dpCall,true)
    return M.new()
end

function M.load(capture,mesh,openFile,dpCall,setStatus,L)
    if not mesh then
        setStatus(L('fre_capture_mesh_required'),true)
        return false
    end
    local path=openFile(capture.path or '','*.jsonl','*.json')
    if not path then return false end
    local mappingOk,mapping=dpCall(function() return Mapping.build(mesh) end)
    if not mappingOk or not mapping then
        setStatus(L('fre_capture_mapping_failed'),true)
        return false
    end
    local replayOk,replay=dpCall(function()
        return Stream.newFileReplay(path,{loop=capture.loop==true,
            resetOnDisconnect=capture.resetOnDisconnect~=false})
    end)
    if not replayOk or not replay then
        setStatus(L('fre_capture_file_failed'),true)
        return false
    end
    M.stop(capture,mesh,dpCall,true)
    capture.path=path
    capture.mapping=mapping
    capture.replay=replay
    capture.playing=false
    capture.status='stopped'
    capture.lastResult=nil
    capture.lastPacket=nil
    capture.lastError=nil
    setStatus(string.format(L('fre_capture_loaded_fmt'),path,#replay.packets),false)
    return true
end

function M.start(capture,mesh,dpCall,setStatus,L)
    if not capture.replay or not capture.mapping or not mesh then
        setStatus(L('fre_capture_open_first'),true)
        return false
    end
    if capture.replay.ended or not capture.replay.connected then
        Mapping.reset(mesh,capture.mapping)
        capture.replay:reset()
    end
    capture.replay.loop=capture.loop==true
    capture.replay.resetOnDisconnect=capture.resetOnDisconnect~=false
    capture.playing=true
    capture.status='playing'
    capture.lastError=nil
    setStatus(L('fre_capture_started'),false)
    return true
end

function M.update(capture,workspace,delta,mesh,dpCall,syncWeights,setStatus,L)
    if workspace~='capture' or not capture.playing or not capture.replay or
            not capture.mapping or not mesh then return end
    local ok,result=dpCall(function()
        return capture.replay:step(math.max(0,tonumber(delta) or 0),mesh,capture.mapping)
    end)
    if not ok or not result then
        capture.playing=false
        capture.status='stopped'
        capture.lastError=L('fre_capture_step_failed')
        setStatus(capture.lastError,true)
        return
    end
    capture.lastResult=result
    if result.applied>0 then
        local last=result.packets[#result.packets]
        capture.lastPacket=last and last.report or nil
        syncWeights()
    end
    if result.ended then
        capture.playing=false
        capture.status='ended'
        setStatus(L('fre_capture_finished'),false)
    end
end

function M.show(capture,project,mesh,imgui,L,openFile,dpCall,setStatus)
    imgui.TextWrapped(L('fre_capture_help'))
    if project.authoringWorkflow~='facs' then
        imgui.TextDisabled(L('fre_capture_facs_only'))
        return
    end
    if not mesh then
        imgui.TextDisabled(L('fre_capture_mesh_required'))
        return
    end
    if imgui.Button(L('fre_capture_open_file')..'##freCaptureOpen',{x=220,y=0}) then
        M.load(capture,mesh,openFile,dpCall,setStatus,L)
    end
    if imgui.IsItemHovered(0) then
        imgui.SetNextWindowSizeConstraints({x=280,y=0},{x=520,y=360})
        imgui.BeginTooltip(); imgui.TextWrapped(L('fre_capture_open_file_tooltip')); imgui.EndTooltip()
    end
    if capture.path~='' then imgui.TextWrapped(string.format(L('fre_capture_file_fmt'),capture.path)) end
    local loop=imgui.Checkbox(L('fre_capture_loop')..'##freCaptureLoop',capture.loop==true)
    if loop~=capture.loop then
        capture.loop=loop
        if capture.replay then capture.replay.loop=loop end
    end
    local reset=imgui.Checkbox(L('fre_capture_reset_neutral')..'##freCaptureReset',
        capture.resetOnDisconnect~=false)
    if reset~=capture.resetOnDisconnect then
        capture.resetOnDisconnect=reset
        if capture.replay then capture.replay.resetOnDisconnect=reset end
    end
    imgui.Separator()
    imgui.BeginDisabled(not capture.replay)
    if imgui.Button(L(capture.playing and 'fre_capture_stop' or 'fre_capture_start')..
            '##freCapturePlayback',{x=180,y=0}) then
        if capture.playing then M.stop(capture,mesh,dpCall,true)
        else M.start(capture,mesh,dpCall,setStatus,L) end
    end
    imgui.EndDisabled()
    if imgui.IsItemHovered(0) then
        imgui.SetNextWindowSizeConstraints({x=280,y=0},{x=520,y=360})
        imgui.BeginTooltip(); imgui.TextWrapped(L(capture.playing and 'fre_capture_stop_tooltip' or
            'fre_capture_start_tooltip')); imgui.EndTooltip()
    end
    imgui.SameLine()
    imgui.BeginDisabled(not capture.replay)
    if imgui.Button(L('fre_capture_reset')..'##freCaptureResetReplay',{x=180,y=0}) then
        M.stop(capture,mesh,dpCall,true)
        if capture.replay then capture.replay:reset() end
        capture.lastResult=nil
        capture.lastPacket=nil
        setStatus(L('fre_capture_reset_done'),false)
    end
    imgui.EndDisabled()
    if not capture.replay then
        imgui.TextDisabled(L('fre_capture_open_first'))
        return
    end
    local replayStatus=capture.replay:status()
    imgui.Text(string.format(L('fre_capture_status_fmt'),
        L(capture.status=='playing' and 'fre_capture_connected' or
            (capture.status=='ended' and 'fre_capture_ended' or 'fre_capture_stopped')),
        replayStatus.packetIndex-1,replayStatus.packetCount,replayStatus.time,replayStatus.duration))
    if capture.mapping then
        imgui.Text(string.format(L('fre_capture_mapping_fmt'),#capture.mapping.targets,
            #capture.mapping.duplicateTargets))
    end
    local packet=capture.lastPacket
    if packet then
        imgui.Text(string.format(L('fre_capture_packet_report_fmt'),packet.mapped,packet.changed,
            packet.unchanged,packet.clamped,packet.rejected))
        if #packet.unknownSource>0 then
            imgui.TextWrapped(string.format(L('fre_capture_unknown_fmt'),
                table.concat(packet.unknownSource,', ')))
        end
        if #packet.missingAliasTarget>0 then
            imgui.TextWrapped(string.format(L('fre_capture_alias_missing_fmt'),
                table.concat(packet.missingAliasTarget,', ')))
        end
    end
    if capture.lastError then imgui.TextColored({r=1,g=0.35,b=0.25,a=1},capture.lastError) end
end

return M
