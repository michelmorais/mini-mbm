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

local M = {normalMethod=1, normalSurfaceAngle=25, includeArticulated=false, includeSkeletal=false}
local math3d = require 'scene_mesh_tools'
local transforms = require 'mesh_debug_transform'
local normals = require 'mesh_debug_normals'
local serial=0
local dimensions={{'width','targetWidth'},{'height','targetHeight'},{'depth','targetDepth'}}
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[scene_mesh_panel] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end
local function unique(path,suffix)
    local result
    repeat
        serial=serial+1
        result=path..'.scene3d-'..suffix..'-'..serial..(suffix=='backup' and '.bak' or '.msh')
        local f=io.open(result,'rb')
        if not f then return result end
        f:close()
    until false
end
local function load(path)
    local d=meshDebug:new()
    assert(d:load(path),'Cannot load '..path)
    return d
end
local function copy(t) local out={};for k,v in pairs(t) do out[k]=v end;return out end
local function instances(path,offset)
    local out={}
    for _,p in ipairs(tPlacedMeshes) do if p.fileName==path then
        out[#out+1]={placed=p,r={(p.rotationX or 0)+offset.rx,(p.rotationY or 0)+offset.ry,(p.rotationZ or 0)+offset.rz},
            s={p.scale.x*offset.sx,p.scale.y*offset.sy,p.scale.z*offset.sz}}
    end end
    return out
end
local function linear(r,s) return math3d.mul(math3d.rotation(table.unpack(r)),math3d.scale(table.unpack(s))) end

-- Inspect authored data only when a scene-wide action is requested, never per frame.
-- Rigs without a currently playing clip are protected too; assets with both kinds
-- require both opt-ins. Opting in does not bypass the transform backend's limits.
function M.includesAsset(d)
    if not M.includeArticulated and
            (d:getTotalArticulatedParts()>0 or d:getTotalArticulatedAnimations()>0) then return false end
    if not M.includeSkeletal then
        local report=d:getSkeletonBindReport(false)
        if report and ((report.boneCount or 0)>0 or (report.animationClipCount or 0)>0) then return false end
        if d:getTotalSkeletalWeightBones()>0 then return false end
    end
    return true
end

-- File writes are staged before changing any scene data. Retain a unique binary backup;
-- undo snapshots cannot safely refer to geometry from before an in-place asset edit.
function M.apply(entry,operation,scene,requestedScale)
    local path=entry.fileName
    if scene and not (M.includeArticulated and M.includeSkeletal) and not M.includesAsset(load(path)) then
        return nil -- excluded before staging, backups, physics changes or scene mutations
    end
    local offset=copy(getMeshOffset(path))
    local items=instances(path,offset)
    local newOffset=copy(offset)
    local scale,base
    if operation=='scale' then
        scale=requestedScale or {offset.sx,offset.sy,offset.sz}
        if scene then scale=math3d.dominant(items,'s').s end
        for _,v in ipairs(scale) do
            if not (v==v and v>0 and v<math.huge) then error('scene3d_singular_transform',0) end
        end
        base=math3d.scale(table.unpack(scale))
        if not requestedScale then
            newOffset.sx,newOffset.sy,newOffset.sz=1,1,1
            for _,item in ipairs(items) do
                item.newScale={x=item.s[1]/scale[1],y=item.s[2]/scale[2],z=item.s[3]/scale[3]}
            end
        end
    elseif operation=='rotate' then
        local selected=scene and math3d.dominant(items,'r') or {r={offset.rx,offset.ry,offset.rz},s={offset.sx,offset.sy,offset.sz}}
        local rotation=math3d.rotation(table.unpack(selected.r))
        local scaling=math3d.scale(table.unpack(selected.s))
        base=math3d.mul(math3d.mul(math3d.inverse(scaling),rotation),scaling)
        newOffset.rx,newOffset.ry,newOffset.rz=0,0,0
        for _,item in ipairs(items) do
            local r
            if scene then
                r={math3d.angles(math3d.mul(math3d.rotation(table.unpack(item.r)),math3d.transpose(rotation)))}
            else
                r={item.placed.rotationX or 0,item.placed.rotationY or 0,item.placed.rotationZ or 0}
            end
            item.newRotation=r
            item.matrix=math3d.mul(math3d.inverse(linear(r,item.s)),linear(item.r,item.s))
        end
    end
    local stages,variants={},{}
    local function stage(matrix,target)
        local d=load(path)
        if operation=='normals' then math3d.normals(d,M,not meshDebug:getInfo(path).hasNormal)
        else math3d.bake(d,matrix,scale) end
        local temporary=unique(path,'pending')
        local record={temporary=temporary,target=target}
        stages[#stages+1]=record
        assert(d:save(temporary,false,false),'Cannot save '..temporary)
        -- Parse the serialized result before touching the source or its live renderables.
        local check=load(temporary)
        math3d.bounds(check)
        meshDebug:fakeRelease(temporary)
    end
    local ok,err=dpCall(function()
        stage(base,path)
        if operation=='rotate' then
            variants[math3d.key(base)]=path
            for _,item in ipairs(items) do
                local key=math3d.key(item.matrix)
                local target=variants[key]
                if not target then
                    target=unique(path,'rotation')
                    stage(item.matrix,target)
                    variants[key]=target
                end
                item.newPath=target
            end
        end
    end)
    if not ok then
        for _,s in ipairs(stages) do meshDebug:fakeRelease(s.temporary);os.remove(s.temporary) end
        error(err,0)
    end
    local backup=unique(path,'backup')
    local moved,moveError=os.rename(path,backup)
    if not moved then
        for _,s in ipairs(stages) do os.remove(s.temporary) end
        error(moveError,0)
    end
    local committed={}
    ok,err=dpCall(function()
        for _,s in ipairs(stages) do assert(os.rename(s.temporary,s.target));committed[#committed+1]=s.target end
    end)
    if not ok then
        for _,target in ipairs(committed) do assert(os.remove(target)) end
        assert(os.rename(backup,path))
        for _,s in ipairs(stages) do os.remove(s.temporary) end
        error(err,0)
    end
    -- Stop every owner before evicting the old shared mesh. The thumbnail renderer is
    -- another owner, even when the mesh selector is hidden.
    destroyPreviewMesh3d()
    if tThumbGenActive and tThumbGenActive.entry.fileName==path then
        tThumbGenRt:remove(tThumbGenActive.tObj);tThumbGenActive.tObj:destroy();tThumbGenActive=nil
    end
    for _,item in ipairs(items) do if item.placed.tObj then item.placed.tObj:destroy();item.placed.tObj=nil end end
    meshDebug:fakeRelease(path)
    tMeshAlreadyLoaded[path]=nil
    tThumbnailCache[path]=nil
    entry.thumbState=nil;entry.geometryBounds=nil;entry.geometryError=nil
    entry.physWidth=nil;entry.physHeight=nil;entry.physDepth=nil
    for i=#tThumbnailGenQueue,1,-1 do if tThumbnailGenQueue[i].fileName==path then table.remove(tThumbnailGenQueue,i) end end
    tMeshOffsets[path]=newOffset
    for _,item in ipairs(items) do
        local p=item.placed
        if item.newPath then p.fileName=item.newPath;tMeshOffsets[p.fileName]=copy(newOffset) end
        if item.newScale then p.scale=item.newScale end
        if item.newRotation then p.rotationX,p.rotationY,p.rotationZ=table.unpack(item.newRotation) end
        registerMeshSetEntry(p.fileName)
        p.tObj=placeMeshSync(p.fileName,p.type,'3d')
        syncPlacedMeshTransform(p);applyPlacedMeshAnimations(p);applyPlacedMeshVisibility(p)
    end
    updatePreviewMesh3d(entry)
    tCopyBuffer=nil -- copied transforms refer to the previous binary geometry
    resetUndoHistory()
    print('[scene_mesh_panel] Backup: '..backup)
    return #stages-1
end
function M.run(entry,operation,scene,scale)
    local targets={}
    if scene then
        local seen={}
        for _,p in ipairs(tPlacedMeshes) do if p.type=='mesh' and not seen[p.fileName] then
            seen[p.fileName]=true;targets[#targets+1]=registerMeshSetEntry(p.fileName)
        end end
    elseif entry and entry.type=='mesh' then targets[1]=entry end
    local done,failed,variants,skipped=0,0,0,0
    local errors={}
    for _,target in ipairs(targets) do
        local ok,result=dpCall(M.apply,target,operation,scene,scale)
        if ok and result==nil then skipped=skipped+1
        elseif ok then done=done+1;variants=variants+result
        else failed=failed+1;errors[#errors+1]=target.fileName..': '..tLang.L(tostring(result)) end
    end
    if scene then
        local selected=getFilteredMeshSetEntries()[iPreviewedMeshSetIndex]
        updatePreviewMesh3d(selected)
    end
    local message=string.format(tLang.L('scene3d_bake_result'),done,failed,variants)
    if scene then message=message..'\n'..string.format(tLang.L('scene3d_bake_skipped'),skipped) end
    if #errors>0 then message=message..'\n'..table.concat(errors,'\n');tUtil.showMessageWarn(message,15)
    else tUtil.showMessage(message,8) end
end
function M.button(entry,operation)
    if entry.type~='mesh' then return end
    return tImGui.Button(tLang.L('scene3d_bake_'..operation))
end
function M.dimensions(entry)
    if entry.type~='mesh' then
        getOrCreateThumbnail(entry)
        if entry.physWidth then
            tImGui.Text(string.format('%s: %.2f',tLang.L('width'),entry.physWidth))
            tImGui.Text(string.format('%s: %.2f',tLang.L('height'),entry.physHeight))
            tImGui.Text(string.format('%s: %.2f',tLang.L('depth'),entry.physDepth))
        else
            tImGui.TextWrapped(tLang.L(entry.thumbState=='failed' and 'scene3d_dimensions_failed' or 'dimensions_computing'))
        end
        return
    end
    if not entry.geometryBounds and not entry.geometryError then
        local ok,b=dpCall(function() return math3d.bounds(load(entry.fileName)) end)
        if ok then
            entry.geometryBounds=b
            entry.resize={targetWidth=b.width,targetHeight=b.height,targetDepth=b.depth,keepRatio=1}
        else entry.geometryError=tostring(b) end
    end
    if entry.geometryError then tImGui.TextWrapped(tLang.L('scene3d_dimensions_failed')..' '..entry.geometryError);return end
    local b,xf=entry.geometryBounds,entry.resize
    transforms.drawRatio(tImGui,tLang,xf,'scene3dRatio',b)
    for _,axis in ipairs(dimensions) do
        local changed,value=tImGui.InputFloat(tLang.L(axis[1])..'##scene3dSize',xf[axis[2]],.1,1,'%.3f')
        if changed then xf[axis[2]]=math.max(.001,value);transforms.syncRatio(xf,b) end
    end
    if tImGui.Button(tLang.L('scene3d_apply_dimensions')) then
        local scale={}
        for i,axis in ipairs(dimensions) do
            local key,target=axis[1],xf[axis[2]]
            if b[key]<=1e-7 and math.abs(target-b[key])>1e-7 then
                tUtil.showMessageWarn(tLang.L('scene3d_flat_dimension'));return
            end
            scale[i]=b[key]>1e-7 and target/b[key] or 1
        end
        M.run(entry,'scale',false,scale)
    end
end
function M.footer(entry)
    tImGui.Separator()
    normals.draw(tImGui,tLang,M,'scene3dNormals')
    if entry and entry.type=='mesh' and tImGui.Button(tLang.L('scene3d_apply_normals')) then M.run(entry,'normals',false) end
    tImGui.Separator()
    tImGui.TextWrapped(tLang.L('scene3d_scene_bake_help'))
    M.includeArticulated=tImGui.Checkbox(tLang.L('scene3d_include_articulated'),M.includeArticulated)
    M.includeSkeletal=tImGui.Checkbox(tLang.L('scene3d_include_skeletal'),M.includeSkeletal)
    tImGui.TextWrapped(tLang.L('scene3d_animation_filter_help'))
    if tImGui.Button(tLang.L('scene3d_standard_scale')) then M.run(nil,'scale',true) end
    if tImGui.Button(tLang.L('scene3d_standard_rotation')) then M.run(nil,'rotate',true) end
    if tImGui.Button(tLang.L('scene3d_normalize_all')) then M.run(nil,'normals',true) end
end
return M
