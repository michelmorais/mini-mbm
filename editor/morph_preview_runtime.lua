--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,           |
| and to permit persons to whom the Software is furnished to do so, subject to the following conditions:                |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Runtime comparison and skeletal playback controls for the generic Morph Preview worktree.
]]--

local tImGui=require 'ImGui'
local tLang=require 'lang.language'
local tUtil=require 'editor_utils'
local M={}

local function L(key)
    return tLang.L('mre_'..key)
end

local function finite(value,default)
    value=tonumber(value)
    if not value or value~=value or value==math.huge or value==-math.huge then return default or 0 end
    return value
end

local function showHelp(message)
    if not message or message=='' or not tImGui.IsItemHovered(0) then return end
    tImGui.SetNextWindowSizeConstraints({x=280,y=0},{x=440,y=360})
    tImGui.BeginTooltip()
    tImGui.TextWrapped(message)
    tImGui.EndTooltip()
end

function M.new()
    return {comparisonMode=1,comparisonMesh=nil,comparisonReady=false,comparisonLayout=false,
        previewMesh=nil,previewReady=false,previewMethod=nil,previewExecution=nil,
        previewSourceKey=nil,previewTempPath=nil,previewVisible=true,morphWeightKey=nil,
        comparisonSyncedClip=nil,skeletalAnimations={},selectedSkeletal=1,skeletalDirty=true,
        skeletalPlaying=false,skeletalPaused=false,activeSkeletal=nil,lastError=nil,lastReportedError=nil}
end

local function releaseComparison(runtime,call)
    if runtime.comparisonMesh then
        runtime.comparisonMesh.alwaysRender=false
        call(function() runtime.comparisonMesh:destroy() end)
    end
    runtime.comparisonMesh=nil
    runtime.comparisonReady=false
    runtime.comparisonLayout=false
    runtime.comparisonSyncedClip=nil
end

local function releasePreviewMesh(runtime,call,removeFile)
    if runtime.previewMesh then
        runtime.previewMesh.alwaysRender=false
        call(function() runtime.previewMesh:destroy() end)
    end
    runtime.previewMesh=nil
    runtime.previewReady=false
    runtime.previewMethod=nil
    runtime.previewExecution=nil
    runtime.morphWeightKey=nil
    if removeFile and runtime.previewTempPath then
        os.remove(runtime.previewTempPath)
        runtime.previewTempPath=nil
    end
end

function M.destroy(runtime,primary,call)
    if not runtime then return end
    releaseComparison(runtime,call)
    releasePreviewMesh(runtime,call,true)
    if primary then call(function() primary:setPos(0,0,0) end) end
    runtime.comparisonMode=1
    runtime.previewSourceKey=nil
    runtime.skeletalPlaying=false
    runtime.skeletalPaused=false
    runtime.activeSkeletal=nil
    runtime.lastError=nil
    runtime.lastReportedError=nil
end

function M.invalidate(runtime,call)
    if not runtime then return end
    releaseComparison(runtime,call)
    releasePreviewMesh(runtime,call,true)
    runtime.comparisonMode=1
    runtime.previewSourceKey=nil
    runtime.skeletalAnimations={}
    runtime.selectedSkeletal=1
    runtime.skeletalDirty=true
    runtime.skeletalPlaying=false
    runtime.skeletalPaused=false
    runtime.activeSkeletal=nil
    runtime.lastError=nil
    runtime.lastReportedError=nil
end

function M.refreshCatalog(runtime,primary,call)
    runtime.skeletalAnimations={}
    runtime.selectedSkeletal=math.max(1,math.floor(runtime.selectedSkeletal or 1))
    local source=runtime.previewMesh or primary
    if source and source.getTotalSkeletalAnimations then
        local ok,total=call(function() return source:getTotalSkeletalAnimations() end)
        if ok then
            for index=1,tonumber(total) or 0 do
                local nameOk,name=call(function() return source:getSkeletalAnimationName(index) end)
                if nameOk and type(name)=='string' and name~='' then
                    runtime.skeletalAnimations[#runtime.skeletalAnimations+1]=name
                end
            end
        end
    end
    runtime.selectedSkeletal=math.max(1,math.min(runtime.selectedSkeletal,
        math.max(1,#runtime.skeletalAnimations)))
    runtime.skeletalDirty=false
end

local function boundsWithComparison(bounds,separation)
    if not bounds then return nil end
    return {minX=bounds.minX-separation,maxX=bounds.maxX+separation,
        minY=bounds.minY,maxY=bounds.maxY,minZ=bounds.minZ,maxZ=bounds.maxZ}
end

local function targetSourceKey(sourcePath,targets)
    local parts={sourcePath or ''}
    for _,target in ipairs(targets or {}) do
        parts[#parts+1]=tostring(target.id or '')
        parts[#parts+1]=tostring(target.name or '')
        for _,delta in ipairs(target.deltas or {}) do
            parts[#parts+1]=string.format('%d:%.8f:%.8f:%.8f',tonumber(delta.index) or 0,
                tonumber(delta.x) or 0,tonumber(delta.y) or 0,tonumber(delta.z) or 0)
        end
    end
    return table.concat(parts,'|')
end

local function densePositions(target,vertexCount)
    local positions={}
    for index=1,vertexCount do positions[index]={x=0,y=0,z=0} end
    for _,delta in ipairs(target and target.deltas or {}) do
        local index=math.floor(tonumber(delta.index) or 0)
        if positions[index] then
            positions[index]={x=tonumber(delta.x) or 0,y=tonumber(delta.y) or 0,
                z=tonumber(delta.z) or 0}
        end
    end
    return positions
end

local function buildRuntimeSource(runtime,meshDebug,topology,targets,call)
    if not meshDebug or not topology or type(meshDebug.save)~='function' then
        return false,nil,'preview_runtime_source_unavailable'
    end
    local sourcePath=tUtil.getTemporaryFilePath('_morph_runtime_source.msh')
    local savedOk,saved=call(function()
        return meshDebug:save(sourcePath,false,false,true)
    end)
    if not savedOk or saved~=true then
        os.remove(sourcePath)
        return false,nil,'preview_runtime_source_unavailable'
    end
    local copy=meshDebug:new()
    local loadedOk,loaded=call(function() return copy:load(sourcePath) end)
    if not loadedOk or loaded~=true then
        call(function() return meshDebug:fakeRelease(sourcePath) end)
        os.remove(sourcePath)
        return false,nil,'preview_runtime_source_unavailable'
    end
    call(function() return copy:removeNormalMap() end)
    local dataOk,exportData=call(function() return copy:getFacialExportData() end)
    if not dataOk or type(exportData)~='table' then
        call(function() return meshDebug:fakeRelease(sourcePath) end)
        os.remove(sourcePath)
        return false,nil,'preview_runtime_source_unavailable'
    end
    local existing={}
    for index,entry in ipairs(exportData.targets or {}) do
        if type(entry)=='table' and type(entry.name)=='string' and entry.name~='' then
            existing[entry.name]={index=index,normalDeltas=entry.normalDeltas}
        end
    end
    local vertexCount=#(topology.vertices or {})
    for _,target in ipairs(targets or {}) do
        local name=type(target.name)=='string' and target.name or ''
        if name~='' then
            local entry=existing[name]
            local positions=densePositions(target,vertexCount)
            local callOk,result
            if entry then
                callOk,result=call(function()
                    return copy:updateFacialTarget(entry.index,name,'Manual',0,positions,entry.normalDeltas)
                end)
            else
                callOk,result=call(function()
                    return copy:addFacialTarget(name,'Manual',0,positions,nil)
                end)
            end
            if not callOk or (result~=true and type(result)~='number') then
                call(function() return meshDebug:fakeRelease(sourcePath) end)
                os.remove(sourcePath)
                return false,nil,'preview_runtime_source_unavailable'
            end
            if not entry and type(result)=='number' then
                existing[name]={index=result,normalDeltas=nil}
            end
        end
    end
    local runtimePath=tUtil.getTemporaryFilePath('_morph_runtime_targets.msh')
    local outputOk,outputSaved=call(function()
        return copy:save(runtimePath,false,false,false)
    end)
    copy=nil
    call(function() return meshDebug:fakeRelease(sourcePath) end)
    os.remove(sourcePath)
    if not outputOk or outputSaved~=true then
        os.remove(runtimePath)
        return false,nil,'preview_runtime_source_unavailable'
    end
    if runtime.previewTempPath then os.remove(runtime.previewTempPath) end
    runtime.previewTempPath=runtimePath
    return true,runtimePath,nil
end

local function buildRuntimeMesh(path,method,execution,call)
    local candidate=mesh:new('3d')
    local setupOk,setup=call(function()
        return candidate:setSkeletalSkinningMethod(method or 'auto') and
            candidate:setSkeletalExecutionPath(execution or 'auto')
    end)
    local loadOk,loaded=false,false
    if setupOk and setup~=false then
        loadOk,loaded=call(function() return candidate:load(path) end)
    end
    if not setupOk or setup==false or not loadOk or loaded==false then
        candidate.alwaysRender=false
        call(function() return candidate:destroy() end)
        return nil
    end
    call(function() return candidate:stopSkeletalAnimation() end)
    candidate.alwaysRender=true
    candidate.visible=false
    return candidate
end

local function resolvedSkinning(primary,call)
    local ok,report=call(function() return primary:getSkeletalSkinningReport() end)
    if not ok or type(report)~='table' then return 'auto','auto',nil end
    local method=report.resolvedMethod=='dqs' and 'dqs' or report.resolvedMethod=='lbs' and 'lbs' or 'auto'
    local execution=report.resolvedExecutionPath=='cpu' and 'cpu' or report.resolvedExecutionPath=='gpu' and 'gpu' or 'auto'
    return method,execution,report
end

local function ensureSource(runtime,primary,meshDebug,topology,targets,sourcePath,call)
    local key=targetSourceKey(sourcePath,targets)
    if runtime.previewTempPath and runtime.previewSourceKey==key then return true,runtime.previewTempPath,nil end
    releaseComparison(runtime,call)
    releasePreviewMesh(runtime,call,true)
    local ok,path,reason=buildRuntimeSource(runtime,meshDebug,topology,targets,call)
    if not ok then return false,nil,reason end
    runtime.previewSourceKey=key
    return true,path,nil
end

local function ensurePreviewMesh(runtime,path,method,execution,call)
    if runtime.previewMesh and runtime.previewMethod==method and runtime.previewExecution==execution then
        return true,runtime.previewMesh,nil
    end
    releasePreviewMesh(runtime,call,false)
    local candidate=buildRuntimeMesh(path,method,execution,call)
    if not candidate then return false,nil,'preview_runtime_mesh_unavailable' end
    runtime.previewMesh=candidate
    runtime.previewReady=true
    runtime.previewMethod=method
    runtime.previewExecution=execution
    runtime.skeletalDirty=true
    return true,candidate,nil
end

function M.rebuild(runtime,primary,meshDebug,topology,targets,sourcePath,bounds,mode,dqsSupported,call)
    if not runtime or not primary then return false,nil,'preview_runtime_no_mesh' end
    releaseComparison(runtime,call)
    runtime.comparisonMode=1
    runtime.lastError=nil
    runtime.lastReportedError=nil
    if type(sourcePath)~='string' or sourcePath=='' then
        runtime.lastError='preview_runtime_no_mesh'
        return false,nil,runtime.lastError
    end
    if mode==3 and not dqsSupported then
        runtime.lastError='preview_runtime_dqs_unavailable'
        return false,nil,runtime.lastError
    end
    local primaryMethod,primaryExecution,report=resolvedSkinning(primary,call)
    if mode~=1 and (not report or primaryMethod=='auto') then
        runtime.lastError='preview_runtime_no_skeleton'
        return false,nil,runtime.lastError
    end
    if mode~=1 and primaryExecution~='gpu' then
        runtime.lastError='preview_runtime_gpu_unavailable'
        return false,nil,runtime.lastError
    end
    local sourceOk,runtimeSource,sourceReason=ensureSource(runtime,primary,meshDebug,topology,targets,sourcePath,call)
    if not sourceOk then
        runtime.lastError=sourceReason or 'preview_runtime_source_unavailable'
        return false,nil,runtime.lastError
    end
    local previewMethod=primaryMethod
    local previewExecution=mode==1 and primaryExecution or 'gpu'
    local previewOk,previewMesh,previewReason=ensurePreviewMesh(runtime,runtimeSource,previewMethod,
        previewExecution,call)
    if not previewOk then
        runtime.lastError=previewReason or 'preview_runtime_mesh_unavailable'
        return false,nil,runtime.lastError
    end
    runtime.morphWeightKey=nil
    if mode==1 then
        runtime.comparisonMode=1
        call(function() primary:setPos(0,0,0) end)
        call(function() previewMesh:setPos(0,0,0) end)
        return true,nil,nil
    end
    local candidateMethod=mode==3 and (primaryMethod=='dqs' and 'lbs' or 'dqs') or primaryMethod
    local candidate=buildRuntimeMesh(runtimeSource,candidateMethod,mode==2 and 'cpu' or 'gpu',call)
    if not candidate then
        runtime.lastError=mode==3 and 'preview_runtime_dqs_unavailable' or 'preview_runtime_cpu_unavailable'
        return false,nil,runtime.lastError
    end
    if mode==2 then
        local candidatePathOk,candidatePath=call(function() return candidate:getResolvedSkeletalExecutionPath() end)
        if not candidatePathOk or candidatePath~='cpu' then
            candidate.alwaysRender=false
            call(function() candidate:destroy() end)
            runtime.lastError='preview_runtime_cpu_unavailable'
            return false,nil,runtime.lastError
        end
    else
        local candidateReportOk,candidateReport=call(function() return candidate:getSkeletalSkinningReport() end)
        if not candidateReportOk or type(candidateReport)~='table' or
                candidateReport.resolvedMethod==primaryMethod or
                (candidateReport.resolvedExecutionPath or candidateReport.executionPath)~='gpu' then
            candidate.alwaysRender=false
            call(function() candidate:destroy() end)
            runtime.lastError='preview_runtime_dqs_unavailable'
            return false,nil,runtime.lastError
        end
    end
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local separation=math.max(extent*0.65,0.1)
    call(function() primary:setPos(0,0,0) end)
    call(function() previewMesh:setPos(-separation,0,0) end)
    call(function() candidate:setPos(separation,0,0) end)
    candidate.visible=true
    runtime.comparisonMesh=candidate
    runtime.comparisonReady=true
    runtime.comparisonLayout=true
    runtime.comparisonMode=mode
    if runtime.skeletalPlaying and runtime.activeSkeletal then
        call(function() return candidate:playSkeletalAnimation(runtime.activeSkeletal) end)
        if runtime.skeletalPaused then
            call(function() return candidate:pauseSkeletalAnimation() end)
        end
        local timeOk,time=call(function() return previewMesh:getSkeletalAnimationTime() end)
        if timeOk and time~=nil then
            call(function() return candidate:seekSkeletalAnimation(time) end)
        end
    end
    return true,boundsWithComparison(bounds,separation),nil
end

local function activeName(runtime)
    return runtime.skeletalAnimations[runtime.selectedSkeletal]
end

local function activeMesh(runtime,primary)
    return runtime.previewMesh or primary
end

function M.play(runtime,primary,call)
    local name=activeName(runtime)
    local target=activeMesh(runtime,primary)
    if not name or not target then return false end
    local ok,played=call(function() return target:playSkeletalAnimation(name) end)
    if not ok or played==false then return false end
    if runtime.comparisonMesh then
        call(function() return runtime.comparisonMesh:playSkeletalAnimation(name) end)
        call(function() return runtime.comparisonMesh:seekSkeletalAnimation(0) end)
    end
    runtime.skeletalPlaying=true
    runtime.skeletalPaused=false
    runtime.activeSkeletal=name
    return true
end

function M.pause(runtime,primary,call)
    local target=activeMesh(runtime,primary)
    if not target then return false end
    local ok,paused=call(function() return target:pauseSkeletalAnimation() end)
    if not ok or paused==false then return false end
    if runtime.comparisonMesh then call(function() return runtime.comparisonMesh:pauseSkeletalAnimation() end) end
    runtime.skeletalPaused=true
    return true
end

function M.resume(runtime,primary,call)
    local target=activeMesh(runtime,primary)
    if not target then return false end
    local ok,resumed=call(function() return target:resumeSkeletalAnimation() end)
    if not ok or resumed==false then return false end
    if runtime.comparisonMesh then call(function() return runtime.comparisonMesh:resumeSkeletalAnimation() end) end
    runtime.skeletalPaused=false
    runtime.skeletalPlaying=true
    return true
end

function M.stop(runtime,primary,call)
    local target=activeMesh(runtime,primary)
    if not target then return false end
    local ok,stopped=call(function() return target:stopSkeletalAnimation() end)
    if not ok or stopped==false then return false end
    if runtime.comparisonMesh then call(function() return runtime.comparisonMesh:stopSkeletalAnimation() end) end
    runtime.skeletalPlaying=false
    runtime.skeletalPaused=false
    runtime.activeSkeletal=nil
    return true
end

function M.seek(runtime,primary,time,call)
    local target=activeMesh(runtime,primary)
    if not target then return false end
    local ok,seeked=call(function() return target:seekSkeletalAnimation(time) end)
    if not ok or seeked==false then return false end
    if runtime.comparisonMesh then call(function() return runtime.comparisonMesh:seekSkeletalAnimation(time) end) end
    return true
end

function M.ensure(runtime,primary,meshDebug,topology,targets,sourcePath,call)
    if not runtime or not primary then return false,'preview_runtime_no_mesh' end
    local sourceOk,runtimeSource,sourceReason=ensureSource(runtime,primary,meshDebug,topology,targets,sourcePath,call)
    if not sourceOk then
        runtime.lastError=sourceReason or 'preview_runtime_source_unavailable'
        return false,runtime.lastError
    end
    local method,execution=resolvedSkinning(primary,call)
    local previewOk,_,previewReason=ensurePreviewMesh(runtime,runtimeSource,method,execution,call)
    if not previewOk then
        runtime.lastError=previewReason or 'preview_runtime_mesh_unavailable'
        return false,runtime.lastError
    end
    runtime.lastError=nil
    runtime.lastReportedError=nil
    return true,nil
end

function M.setVisible(runtime,visible)
    if not runtime then return end
    runtime.previewVisible=visible==true
    if runtime.previewMesh then runtime.previewMesh.visible=runtime.previewVisible end
    if runtime.comparisonMesh then runtime.comparisonMesh.visible=runtime.previewVisible end
end

function M.applyMorphWeights(runtime,primary,expression,targets,weight,call)
    if not runtime then return false end
    local source=activeMesh(runtime,primary)
    if not source then return false end
    weight=math.max(0,math.min(1,tonumber(weight) or 1))
    local parts={tostring(expression and expression.id or 'neutral'),string.format('%.6f',weight)}
    for _,entry in ipairs(expression and expression.weights or {}) do
        parts[#parts+1]=table.concat({entry.targetId or '',entry.targetName or '',
            string.format('%.6f',tonumber(entry.weight) or 0)},':')
    end
    local key=table.concat(parts,'|')
    if runtime.morphWeightKey==key then return true end
    local targetById={}
    for _,target in ipairs(targets or {}) do targetById[target.id]=target end
    local function apply(meshObject)
        local resetOk,reset=call(function() return meshObject:resetFacialWeights() end)
        if not resetOk or reset==false then return false end
        for _,entry in ipairs(expression and expression.weights or {}) do
            local target=targetById[entry.targetId]
            local name=entry.targetName~='' and entry.targetName or target and target.name
            if name and name~='' then
                local ok,result=call(function()
                    return meshObject:setFacialWeight(name,(tonumber(entry.weight) or 0)*weight)
                end)
                if not ok or result==false then return false end
            end
        end
        return true
    end
    local applied=apply(source)
    if runtime.comparisonMesh then applied=apply(runtime.comparisonMesh) and applied end
    if applied then runtime.morphWeightKey=key end
    return applied
end

function M.update(runtime,primary,active,nativeActive,bounds,call)
    if not runtime or not primary then return end
    local source=runtime.previewMesh or primary
    local comparison=runtime.comparisonMesh
    if not active or not nativeActive then
        if comparison then comparison.visible=false end
        if runtime.previewMesh then runtime.previewMesh.visible=false end
        primary.visible=true
        if runtime.comparisonLayout then
            call(function() primary:setPos(0,0,0) end)
            call(function() source:setPos(0,0,0) end)
            call(function() comparison:setPos(0,0,0) end)
            runtime.comparisonLayout=false
        end
        return
    end
    if runtime.comparisonMode==1 then
        if comparison then comparison.visible=false end
        primary.visible=not runtime.previewReady
        if runtime.previewReady then
            source.visible=runtime.previewVisible~=false
            call(function() source:setPos(0,0,0) end)
        end
        return
    end
    if not runtime.comparisonReady or not comparison then
        if comparison then comparison.visible=false end
        primary.visible=not runtime.previewReady
        if runtime.previewReady then
            source.visible=runtime.previewVisible~=false
            call(function() source:setPos(0,0,0) end)
        end
        return
    end
    primary.visible=false
    source.visible=runtime.previewVisible~=false
    comparison.visible=true
    if not runtime.comparisonLayout then
        local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
            bounds.maxZ-bounds.minZ) or 1
        local separation=math.max(extent*0.65,0.1)
        call(function() source:setPos(-separation,0,0) end)
        call(function() comparison:setPos(separation,0,0) end)
        runtime.comparisonLayout=true
    end
    if runtime.skeletalPlaying and runtime.activeSkeletal then
        local ok,time=call(function() return source:getSkeletalAnimationTime() end)
        if ok and time~=nil then
            local otherOk,otherTime=call(function() return comparison:getSkeletalAnimationTime() end)
            if not otherOk or otherTime==nil or math.abs(time-otherTime)>0.0001 then
                call(function() return comparison:seekSkeletalAnimation(time) end)
            end
        end
    end
end

function M.draw(runtime,primary,active,dqsSupported,call,rebuild,setStatus,preparePlayback,finishPlayback,
        markModified)
    if not active or not runtime or not primary then return end
    if runtime.skeletalDirty then M.refreshCatalog(runtime,primary,call) end
    if runtime.comparisonMode~=1 and not runtime.comparisonReady and not runtime.lastError then
        local ok,_,reason=rebuild(runtime.comparisonMode)
        if not ok then setStatus(L(reason or 'preview_runtime_compare_failed'),true) end
    end
    tImGui.Text(L('preview_runtime_compare_title'))
    local labels={L('preview_runtime_compare_none'),L('preview_runtime_compare_gpu_cpu'),
        L('preview_runtime_compare_lbs_dqs')}
    local selected=runtime.comparisonMode
    for index,label in ipairs(labels) do
        local nextSelected=tImGui.RadioButton(label..'##mrePreviewRuntimeMode'..index,selected,index)
        if nextSelected~=selected then
            local ok,_,reason=rebuild(nextSelected)
            if ok then
                selected=nextSelected
                if markModified then markModified() end
            else
                setStatus(L(reason or 'preview_runtime_compare_failed'),true)
            end
        end
    end
    showHelp(L('preview_runtime_compare_help'))
    if runtime.comparisonMode~=1 and runtime.comparisonReady then
        local display=activeMesh(runtime,primary)
        local primaryOk,primaryReport=call(function() return display:getSkeletalSkinningReport() end)
        local secondaryOk,secondaryReport=call(function() return runtime.comparisonMesh:getSkeletalSkinningReport() end)
        if primaryOk and secondaryOk then
            tImGui.Text(string.format(L('preview_runtime_compare_reports'),
                primaryReport.resolvedExecutionPath or primaryReport.executionPath or 'unknown',
                primaryReport.resolvedMethod or 'unknown',
                secondaryReport.resolvedExecutionPath or secondaryReport.executionPath or 'unknown',
                secondaryReport.resolvedMethod or 'unknown'))
        end
    end
    tImGui.Separator()
    tImGui.Text(L('preview_runtime_skeletal_title'))
    local names=runtime.skeletalAnimations
    if #names==0 then
        tImGui.TextDisabled(L('preview_runtime_no_skeletal'))
        return
    end
    runtime.selectedSkeletal=math.max(1,math.min(runtime.selectedSkeletal,#names))
    tImGui.SetNextItemWidth(280)
    local changed,index=tImGui.Combo(L('preview_runtime_skeletal_clip'),runtime.selectedSkeletal,names)
    if changed then
        runtime.selectedSkeletal=index
        if runtime.skeletalPlaying then M.stop(runtime,primary,call) end
        if markModified then markModified() end
    end
    showHelp(L('preview_runtime_skeletal_clip_help'))
    if tImGui.Button(L('preview_runtime_skeletal_play')..'##mrePreviewRuntimePlay',{x=110,y=0}) then
        if preparePlayback then preparePlayback() end
        if not M.play(runtime,primary,call) then setStatus(L('preview_runtime_skeletal_failed'),true) end
    end
    tImGui.SameLine()
    if tImGui.Button(L('preview_runtime_skeletal_pause')..'##mrePreviewRuntimePause',{x=90,y=0}) then
        M.pause(runtime,primary,call)
    end
    tImGui.NewLine()
    if tImGui.Button(L('preview_runtime_skeletal_resume')..'##mrePreviewRuntimeResume',{x=100,y=0}) then
        M.resume(runtime,primary,call)
    end
    tImGui.SameLine()
    if tImGui.Button(L('preview_runtime_skeletal_stop')..'##mrePreviewRuntimeStop',{x=90,y=0}) then
        M.stop(runtime,primary,call)
        if finishPlayback then finishPlayback() end
    end
    local durationOk,duration=call(function()
        return activeMesh(runtime,primary):getSkeletalAnimationDuration(runtime.selectedSkeletal)
    end)
    local timeOk,time=call(function() return activeMesh(runtime,primary):getSkeletalAnimationTime() end)
    if durationOk and timeOk and duration and time then
        tImGui.SetNextItemWidth(360)
        local timeChanged,newTime=tImGui.SliderFloat(L('preview_runtime_skeletal_time')..'##mrePreviewRuntimeTime',
            time,0,math.max(finite(duration,0.001),0.001),'%.3f s')
        if timeChanged and (runtime.skeletalPlaying or runtime.skeletalPaused) then
            M.seek(runtime,primary,newTime,call)
        end
    end
end

return M
