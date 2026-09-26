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

local Wire=require 'image_mesh_wireframe'
local M={}
local function L(key) return tLang.L('md_simplify_'..key) end

function M.hide(entry)
    local view=entry and entry.simplifyComparisonView
    if not view then return end
    if view.main and view.active then view.main.visible=view.mainVisible end
    view.main=nil;view.active=false
    for _,side in ipairs{view.original,view.result} do
        side.preview.visible=false
        if side.wireObject then side.wireObject.visible=false end
    end
end
function M.release(entry,discard)
    if not entry then return end
    M.hide(entry)
    local view=entry.simplifyComparisonView
    if view then
        for _,side in ipairs{view.original,view.result} do
            Wire.release(side)
            meshDebug:loadMeshPreview(side.preview,nil);side.preview:destroy()
            os.remove(side.previewPath)
        end
    end
    entry.simplifyComparisonView=nil;entry.simplifyCatalog=nil
    if discard and entry.simplifyComparison then
        meshDebug:fakeRelease(entry.simplifyComparison.path)
        os.remove(entry.simplifyComparison.path);entry.simplifyComparison=nil
    end
end
function M.record(entry,asset,frame,report)
    M.release(entry,true)
    entry.simplifyComparisonError=nil
    local record={path=tUtil.getTemporaryFilePath('_simplify_result.msh'),frame=frame,operation=report.backend,
        sourceVertices=report.sourceVertexCount,vertices=report.resultVertexCount,
        sourceTriangles=report.sourceTriangleCount,triangles=report.resultTriangleCount}
    if not asset:save(record.path,false,false,true) then
        os.remove(record.path);entry.simplifyComparisonError=L('snapshot_failed');error(entry.simplifyComparisonError)
    end
    entry.simplifyComparison=record
end
function M.catalog(entry,asset)
    local cached=entry.simplifyCatalog
    if cached and cached.asset==asset then return cached.frames,cached.subsets end
    local frames=asset:getTotalFrame();local subsets={}
    for f=1,frames do for s=1,asset:getTotalSubset(f) do
        local tex=asset:getTexture(f,s)
        subsets[#subsets+1]={f=f,s=s,texName=tex and tex~='' and (' ['..tUtil.getShortName(tex)..']') or '',
            vertexCount=asset:getTotalVertex(f,s),indexCount=asset:getTotalIndex(f,s)}
    end end
    entry.simplifyCatalog={asset=asset,frames=frames,subsets=subsets}
    return frames,subsets
end
local function snapshot(path,frame,built)
    local source=meshDebug:new();assert(source:load(path),L('snapshot_failed'))
    local asset=meshDebug:new();asset:setType('mesh');asset:setModeFrontFace(source:getModeFrontFace())
    asset:setModeCullFace(source:getModeCullFace());asset:addFrame(3)
    local lo={math.huge,math.huge,math.huge};local hi={-math.huge,-math.huge,-math.huge}
    for s=1,source:getTotalSubset(frame) do
        asset:addSubSet(1)
        local vertices=source:getVertex(frame,s,1,source:getTotalVertex(frame,s))
        local indices=source:getIndex(frame,s)
        if #indices==0 then for i=1,#vertices do indices[i]=i end end
        assert(asset:addVertex(1,s,vertices));assert(asset:addIndex(1,s,indices))
        local texture=source:getTexture(frame,s);if texture and texture~='' then asset:setTexture(1,s,texture) end
        for _,v in ipairs(vertices) do
            local p={v.x,v.y,v.z}
            for k=1,3 do lo[k]=math.min(lo[k],p[k]);hi[k]=math.max(hi[k],p[k]) end
        end
    end
    asset:setMaterial(source:getMaterial());asset:addAnim('Static',1,1,1,0)
    local side={previewPath=tUtil.getTemporaryFilePath('_simplify_view.msh'),lo=lo,hi=hi}
    built[#built+1]=side
    assert(asset:save(side.previewPath,false,false,true),L('snapshot_failed'))
    side.preview=mesh:new('3d');side.preview.visible=false
    assert(meshDebug:loadMeshPreview(side.preview,side.previewPath),L('snapshot_failed'))
    side.preview.visible=false;side.preview.alwaysRender=true;side.preview:setPos(0,0,0)
    return side
end
function M.ensure(entry,safe)
    if entry.simplifyComparisonView then return true end
    local record=entry.simplifyComparison
    if not record or not entry.tSimplifyBackup then return false end
    local built={}
    local ok=safe(function()
        snapshot(entry.tSimplifyBackup.path,record.frame,built)
        snapshot(record.path,record.frame,built)
    end)
    if not ok then
        entry.simplifyComparisonError=L('snapshot_failed')
        for _,side in ipairs(built) do
            if side.preview then meshDebug:loadMeshPreview(side.preview,nil);side.preview:destroy() end
            os.remove(side.previewPath)
        end
        return false
    end
    entry.simplifyComparisonError=nil
    local a,b=built[1],built[2]
    local widthA,widthB=a.hi[1]-a.lo[1],b.hi[1]-b.lo[1]
    local width=widthA+widthB+math.max(.001,math.max(widthA,widthB)*.15)
    a.x=-width/2-a.lo[1];b.x=width/2-b.hi[1]
    a.preview:setPos(a.x,0,0);b.preview:setPos(b.x,0,0)
    entry.simplifyComparisonView={original=a,result=b,enabled=false,wire=false,
        showOriginal=true,showResult=true,width=width,builds=1}
    return true
end
function M.wire(entry,enabled,safe)
    local view=entry.simplifyComparisonView;if not view or view.wire==enabled then return end
    if enabled then
        local ok=safe(function()
            for _,side in ipairs{view.original,view.result} do
                Wire.ensure(side);side.wireObject:setPos(side.x,0,0)
            end
        end)
        if not ok then Wire.release(view.original);Wire.release(view.result);return end
    end
    view.wire=enabled;view.dirty=true
end
function M.sync(entry,main,active)
    local view=entry and entry.simplifyComparisonView
    if not view then return end
    active=active and main~=nil and view.enabled
    if not active then if view.active then M.hide(entry) end;return end
    if view.active and view.main==main and not view.dirty then
        -- Other editor panels restore the primary preview each frame.
        if main.visible then main.visible=false end
        return
    end
    if view.main~=main then M.hide(entry) end
    if not view.active then view.main=main;view.mainVisible=main.visible end
    view.active=true;view.dirty=false;main.visible=false
    for i,side in ipairs{view.original,view.result} do
        local show=i==1 and view.showOriginal or (i==2 and view.showResult)
        side.preview.visible=show and not view.wire
        if side.wireObject then side.wireObject.visible=show and view.wire end
    end
end
function M.fit(entry,applyCamera)
    local view=entry.simplifyComparisonView;if not view then return end
    local a,b=view.original,view.result;local c=entry.cam3d
    c.fx=0;c.fy=(math.min(a.lo[2],b.lo[2])+math.max(a.hi[2],b.hi[2]))/2
    c.fz=(math.min(a.lo[3],b.lo[3])+math.max(a.hi[3],b.hi[3]))/2
    c.distance=math.max(1,view.width,math.max(a.hi[2],b.hi[2])-math.min(a.lo[2],b.lo[2]),
        math.max(a.hi[3],b.hi[3])-math.min(a.lo[3],b.lo[3]))*2.7
    applyCamera(c)
end
function M.panel(entry,safe,applyCamera,available)
    local record=entry.simplifyComparison
    if not record then tImGui.TextWrapped(entry.simplifyComparisonError or L('comparison_pending'));return end
    if entry.simplifyComparisonError then tImGui.TextWrapped(entry.simplifyComparisonError) end
    tImGui.Separator()
    local title=record.operation=='repair' and 'comparison_repair' or
        record.operation=='remesh' and 'comparison_remesh' or 'comparison'
    tImGui.Text(L(title))
    tImGui.TextWrapped(string.format(L('comparison_frame'),record.frame))
    tImGui.Text(string.format(L('comparison_counts'),record.sourceVertices,record.vertices,record.sourceTriangles,record.triangles,
        record.sourceTriangles>0 and 100*(1-record.triangles/record.sourceTriangles) or 0))
    tImGui.BeginDisabled(not available)
    local view=entry.simplifyComparisonView
    local enabled=tImGui.Checkbox(L('side_by_side'),view and view.enabled or false)
    if enabled and not view then if M.ensure(entry,safe) then view=entry.simplifyComparisonView else enabled=false end end
    if view and view.enabled~=enabled then
        view.enabled=enabled;view.dirty=true
        if enabled then M.fit(entry,applyCamera) end
    end
    if view and view.enabled then
        local original=tImGui.Checkbox(L('original'),view.showOriginal)
        local result=tImGui.Checkbox(L('result'),view.showResult)
        if original~=view.showOriginal or result~=view.showResult then
            view.showOriginal=original;view.showResult=result;view.dirty=true
        end
        M.wire(entry,tImGui.Checkbox(L('wireframe'),view.wire),safe)
        if tImGui.Button(L('fit')) then M.fit(entry,applyCamera) end
    end
    tImGui.EndDisabled()
    if not available then tImGui.TextWrapped(L('comparison_unavailable')) end
    tImGui.TextWrapped(L('display_only'))
end
function M.draw(entry,asset,index,drawSettings,safe,applyCamera,available,node,openTree)
    node=node or 'simplification'
    local label=node=='remesh' and tLang.L('cgal_remesh_method') or L('tree')
    local open
    if openTree then open=openTree(entry,node,label,0,node..'-'..index)
    else
        local wantOpen=entry.sOpenNode==node
        tImGui.SetNextItemOpen(wantOpen,tImGui.Flags('ImGuiCond_Always'))
        open=tImGui.TreeNodeEx(label,0,node..'-'..index)
        if tImGui.IsItemClicked() then entry.sOpenNode=wantOpen and nil or node end
    end
    if not open then return end
    if (entry.info or {}).type~='mesh' then tImGui.TextWrapped(L('mesh_only'))
    else
        local frames,subsets=M.catalog(entry,asset)
        if frames>0 then drawSettings(entry,asset,index,frames,subsets,node) end
        local state=entry.tSimplifyState or {}
        M.panel(entry,safe,applyCamera,available and not state.running)
    end
    tImGui.TreePop()
end
return M
