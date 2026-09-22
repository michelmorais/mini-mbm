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

-- Transactional normal processing. Originals remain owned by their entries until confirmation.
local M={}
local function copy(t)
    local out={};for k,v in pairs(t or {}) do out[k]=v end;return out
end
local function cleanup(p)
    for _,path in ipairs(p.paths) do os.remove(path) end
end
function M.entry(original)
    local p=M.pending
    if p and not p.original then
        for _,item in ipairs(p.items) do
            if item.source==original then return item.candidate end
        end
    end
    return original
end
function M.begin(targets,state,frame,subset,vertex,after,protect)
    if M.pending then return end
    local p={items={},paths={},selected=iSelectedMeshIndex,after=after,method=state.normalMethod or 1,
        count=0,labels={},protect=protect,original=false}
    -- Refuse concurrent simplification: the authored source must stay stable throughout review.
    local ok,err=protect(function()
        for _,target in ipairs(targets) do
            local source=target.entry
            if source.tSimplifyState and source.tSimplifyState.running then
                error(tLang.L('normal_review_busy'))
            end
            local d=source.meshDebug
            local missing=not (source.info and source.info.hasNormal)
            if d:getModeDraw()=='TRIANGLES' and (not missing or after) then
                local path=tUtil.getTemporaryFilePath('.msh')
                p.paths[#p.paths+1]=path
                assert(d:save(path,false,false),tLang.L('normal_review_copy_failed'))
                local clone=meshDebug:new()
                assert(clone:load(path),tLang.L('normal_review_copy_failed'))
                if missing then clone:addNormals() end
                local count=0
                for f=frame or 1,frame or clone:getTotalFrame() do
                    for s=subset or 1,subset or clone:getTotalSubset(f) do
                        local geo=tMeshNormals.geometry(clone,f,s,state,computeGeoNormalsForSubset)
                        for v=vertex or 1,vertex or clone:getTotalVertex(f,s) do
                            local vd=clone:getVertex(f,s,v)
                            local mode=(missing and p.method==1) and 2 or p.method
                            local nx,ny,nz=tMeshNormals.select(vd,geo[v],mode)
                            if nx then
                                vd.nx,vd.ny,vd.nz=nx,ny,nz
                                clone:setVertex(f,s,v,vd)
                                count=count+1
                            end
                        end
                    end
                end
                if count>0 or missing or after then
                    local candidate=copy(source)
                    candidate.info=copy(source.info);candidate.info.hasNormal=true
                    candidate.meshDebug=clone;candidate.modified=true
                    candidate.previewPath=tUtil.getTemporaryFilePath('.msh')
                    p.paths[#p.paths+1]=candidate.previewPath
                    p.items[#p.items+1]={source=source,candidate=candidate,index=target.index,
                        count=count,changed=count>0 or missing,authored=d}
                    p.labels[#p.labels+1]=string.format('%s (%d)',tUtil.getShortName(source.fileName),count)
                    p.count=p.count+count
                end
            end
        end
    end)
    if not ok then
        cleanup(p);tUtil.showMessageWarn(tLang.L('normal_review_failed')..': '..tostring(err))
        return {success=0,failed=1,skipped=0}
    end
    if #p.items==0 then
        cleanup(p);tUtil.showMessage(tLang.L('normal_no_changes'),4)
        return {success=0,failed=0,skipped=#targets}
    end
    M.pending=p
    p.choice=1
    for i,item in ipairs(p.items) do if item.index==p.selected then p.choice=i end end
    iSelectedMeshIndex=p.items[p.choice].index
    iLastPreviewedIndex=0
    -- Normal lines must not overlay the candidate using stale authored vectors.
    for _,item in ipairs(p.items) do destroyNormalVisualization(item.source) end
    return {success=#p.items,failed=0,skipped=#targets-#p.items}
end
function M.finish(confirm)
    local p=M.pending
    if not p then return false end
    if confirm then
        -- Validate every source before committing any part of a batch.
        for _,item in ipairs(p.items) do
            if tLoadedMeshes[item.index]~=item.source or item.source.meshDebug~=item.authored then
                tUtil.showMessageWarn(tLang.L('normal_review_stale'));return false
            end
        end
        for _,item in ipairs(p.items) do
            if item.changed then
                item.source.meshDebug=item.candidate.meshDebug
                item.source.info.hasNormal=true
                item.source.modified=true
                item.source.bNormalsVizDirty=true
                destroyNormalVisualization(item.source)
            end
        end
    end
    M.pending=nil
    iSelectedMeshIndex=p.selected
    iLastPreviewedIndex=0
    cleanup(p)
    tUtil.showMessage(tLang.L(confirm and 'normal_review_confirmed' or 'normal_review_cancelled'),4)
    if confirm and p.after then p.after(p.items) end
    return true
end
function M.dispose()
    if M.pending then cleanup(M.pending);M.pending=nil end
end
function M.draw()
    local p=M.pending
    if not p then return end
    local gui=tImGui
    gui.SetNextWindowSize({x=400,y=260},gui.Flags('ImGuiCond_Appearing'))
    local opened,closed=gui.Begin(tLang.L('normal_review_title')..'##normalReview',true,0)
    if opened then
        gui.Text(tLang.L(tMeshNormals.label(p.method)))
        gui.TextWrapped(tLang.L('normal_review_help'))
        gui.Text(string.format(tLang.L('normal_review_count'),#p.items,p.count))
        gui.PushItemWidth(320)
        local changed,choice=gui.Combo('##normalReviewTarget',p.choice,p.labels)
        gui.PopItemWidth()
        if changed then
            p.choice=choice;iSelectedMeshIndex=p.items[choice].index;iLastPreviewedIndex=0
        end
        local original=gui.Checkbox(tLang.L('normal_review_original'),p.original)
        if original~=p.original then p.original=original;iLastPreviewedIndex=0 end
        gui.TextDisabled(tLang.L(p.original and 'normal_review_showing_original' or 'normal_review_showing_result'))
        if p.after then gui.TextWrapped(tLang.L('normal_review_save_warning')) end
        if gui.Button(tLang.L(p.after and 'normal_review_confirm_save' or 'normal_review_confirm')) then M.finish(true) end
        gui.SameLine()
        if gui.Button(tLang.L('cancel')) then M.finish(false) end
    end
    gui.End()
    if closed then M.finish(false) end
end
return M
