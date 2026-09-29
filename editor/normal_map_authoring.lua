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

-- CPU preparation is invoked only by an explicit action or a geometry build.
local M={}
function M.prepare(asset,frame,subset,policy,corners)
    local report,err=asset:prepareNormalMap(frame,subset,policy or 'preserve',corners)
    assert(report,err)
    return report
end
-- Zero is a UI selection sentinel; the engine API remains one-based.
-- Each successful subset is committed; the panel retains one Undo for the whole action.
function M.prepareSelection(asset,frame,subset,policy)
    local result={batches=0,vertices=0,unusableTriangles=0,prepared=0,preserved=0,failures={}}
    local total=asset:getTotalFrame()
    if total==0 or frame>total then
        result.failures[1]={frame=frame,subset=subset,error='No matching frame'}
        return result
    end
    local first,last=frame==0 and 1 or frame,frame==0 and total or frame
    for f=first,last do
        local count=asset:getTotalSubset(f)
        local start,stop=subset==0 and 1 or subset,subset==0 and count or subset
        for part=start,stop do
            local report,err=asset:prepareNormalMap(f,part,policy)
            if report then
                result.prepared=result.prepared+1
                if report.reused then result.preserved=result.preserved+1 end
                for _,field in ipairs({'batches','vertices','unusableTriangles'}) do
                    result[field]=result[field]+report[field]
                end
            else
                result.failures[#result.failures+1]={frame=f,subset=part,error=err}
            end
        end
    end
    result.reused=result.prepared>0 and result.preserved==result.prepared
    return result
end
function M.precompute(asset)
    local report={batches=0,vertices=0,unusableTriangles=0}
    for frame=1,asset:getTotalFrame() do
        for subset=1,asset:getTotalSubset(frame) do
            local result=M.prepare(asset,frame,subset,'preserve')
            for _,field in ipairs({'batches','vertices','unusableTriangles'}) do report[field]=report[field]+result[field] end
        end
    end
    return report
end
-- Intermediate import contract: the caller must choose whether source tangents
-- are authoritative. Never silently discard supplied bases under the default policy.
function M.importFrame(asset,frameIndex,frame,options)
    options=options or {}
    local policy=options.normalMapPolicy
    for _,subset in ipairs(frame.subsets) do
        if subset.cornerTangents and policy~='import' and policy~='generate' then
            return false,'Source corner tangents require an explicit import or generate policy.'
        end
    end
    if not policy and not options.normalMapPrecompute then return true end
    policy=policy or 'preserve'
    if policy=='import' and options.importPostProcess then
        return false,'Importing corner tangents with UV/geometry postprocessing is unsupported; choose generate.'
    end
    for subsetIndex,subset in ipairs(frame.subsets) do
        if policy=='import' and type(subset.cornerTangents)~='table' then
            return false,'Import policy requires cornerTangents for every selected subset.'
        end
        local report,err=asset:prepareNormalMap(frameIndex,subsetIndex,policy,
            policy=='import' and subset.cornerTangents or nil)
        if not report then return false,err end
    end
    return true
end
-- Selection scans run only after selection/edit changes or explicit actions.
function M.readSettings(asset,frame,subset)
    local result={targets={},convention='+Y',strength=1,mixed=false}
    local total=asset:getTotalFrame()
    if total==0 or frame>total then return result end
    local first,last=frame==0 and 1 or frame,frame==0 and total or frame
    for f=first,last do
        local count=asset:getTotalSubset(f)
        if subset>count then result.invalid=true;return result end
        local start,stop=subset==0 and 1 or subset,subset==0 and count or subset
        for part=start,stop do
            local convention,strength=asset:getNormalMapSettings(f,part)
            if not convention then result.invalid=true;return result end
            if #result.targets==0 then
                result.convention=convention;result.strength=strength
            elseif convention~=result.convention or strength~=result.strength then
                result.mixed=true
            end
            result.targets[#result.targets+1]={frame=f,subset=part}
        end
    end
    return result
end
function M.applySettings(asset,frame,subset,convention,strength)
    if (convention~='+Y' and convention~='-Y') or type(strength)~='number' or
        strength~=strength or strength<0 or strength>3.4028234663852886e38 then return false end
    local selection=M.readSettings(asset,frame,subset)
    if selection.invalid or #selection.targets==0 then return false end
    local changed=false
    for _,target in ipairs(selection.targets) do
        local oldConvention,oldStrength=asset:getNormalMapSettings(target.frame,target.subset)
        if oldConvention~=convention or oldStrength~=strength then
            assert(asset:setNormalMapSettings(target.frame,target.subset,convention,strength))
            changed=true
        end
    end
    return changed
end
function M.panel(entry,id,onEdit,applyUndo,restoreUndo)
    local asset=entry.meshDebug
    local state=entry.normalMapAuthoring
    if not state or state.asset~=asset then
        state={asset=asset,frame=0,subset=0,policy=1}
        entry.normalMapAuthoring=state
    end
    tImGui.SetNextItemWidth(110)
    local changed,value=tImGui.InputInt(tLang.L('nm_frame')..'##nmf'..id,state.frame)
    if changed then state.frame=math.max(0,math.min(asset:getTotalFrame(),value));state.subset=0;state.report=nil;state.settings=nil end
    tImGui.SetNextItemWidth(110)
    changed,value=tImGui.InputInt(tLang.L('nm_subset')..'##nms'..id,state.subset)
    if changed then state.subset=math.max(0,value);state.report=nil;state.settings=nil end
    tImGui.TextDisabled(tLang.L('nm_all_hint'))
    if not state.settings then state.settings=M.readSettings(asset,state.frame,state.subset) end
    local settings=state.settings
    tImGui.Separator()
    tImGui.Text(tLang.L('nm_material_settings'))
    if settings.mixed then tImGui.TextWrapped(tLang.L('nm_settings_mixed')) end
    tImGui.SetNextItemWidth(110)
    changed,value=tImGui.Combo(tLang.L('nm_convention')..'##nmc'..id,
        settings.convention=='+Y' and 1 or 2,{'+Y','-Y'},-1)
    if changed then settings.convention=value==1 and '+Y' or '-Y' end
    tImGui.SetNextItemWidth(140)
    changed,value=tImGui.InputFloat(tLang.L('nm_strength')..'##nmi'..id,settings.strength,0.1,1,'%.3f',0)
    if changed then settings.strength=value end
    tImGui.TextWrapped(tLang.L('nm_settings_help'))
    local valid=settings.strength==settings.strength and settings.strength>=0 and
        settings.strength<=3.4028234663852886e38
    if not valid then tImGui.TextWrapped(tLang.L('nm_settings_invalid')) end
    if settings.invalid or #settings.targets==0 then tImGui.TextWrapped(tLang.L('nm_settings_empty')) end
    tImGui.BeginDisabled(not valid or settings.invalid==true or #settings.targets==0 or
        (entry.tSimplifyState or {}).running==true)
    local apply=tImGui.Button(tLang.L('nm_settings_apply')..'##nmapply'..id)
    tImGui.EndDisabled()
    if apply then
        if applyUndo(function()
            return M.applySettings(asset,state.frame,state.subset,settings.convention,settings.strength)
        end) then onEdit() end
        state.settings=nil
    end
    tImGui.Separator()
    tImGui.SetNextItemWidth(260)
    changed,value=tImGui.Combo(tLang.L('nm_policy')..'##nmp'..id,state.policy,
        {tLang.L('nm_preserve'),tLang.L('nm_generate')},-1)
    if changed then state.policy=value end
    tImGui.TextWrapped(tLang.L('nm_prepare_help'))
    tImGui.BeginDisabled((entry.tSimplifyState or {}).running==true)
    local prepare=tImGui.Button(tLang.L('nm_prepare')..'##nma'..id)
    tImGui.EndDisabled()
    if prepare then
        if applyUndo(function()
            state.report=M.prepareSelection(asset,state.frame,state.subset,state.policy==1 and 'preserve' or 'generate')
            return state.report.prepared>0
        end) then onEdit() end
    end
    if state.report then
        local r=state.report
        tImGui.TextWrapped(string.format(tLang.L('nm_report'),r.batches,r.vertices,r.unusableTriangles))
        tImGui.TextWrapped(string.format(tLang.L('nm_selection_report'),r.prepared,r.preserved,#r.failures))
        for _,failure in ipairs(r.failures) do
            tImGui.TextWrapped(string.format(tLang.L('nm_failure'),failure.frame,failure.subset,tostring(failure.error)))
        end
        if r.reused then tImGui.Text(tLang.L('nm_reused')) end
    end
    tImGui.BeginDisabled(not entry.tTransformUndo)
    local undo=tImGui.Button(tLang.L('nm_undo')..'##nmu'..id)
    tImGui.EndDisabled()
    if undo then return restoreUndo() end
    return false
end
return M
