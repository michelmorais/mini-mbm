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
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Facial project morph-target edits. Store only changed targets and nonzero deltas relative to
   the source MSH; rebuild dense arrays only while saving or opening a project.
]]--

local Targets=require 'facial_rig_targets'
local M={}

local function finite(value)
    return type(value)=='number' and value==value and value~=math.huge and value~=-math.huge
end

local function sparse(values,vertexCount)
    local result={}
    for index=1,vertexCount do
        local v=values and values[index]
        if v and (v.x~=0 or v.y~=0 or v.z~=0) then
            result[index]={x=v.x,y=v.y,z=v.z}
        end
    end
    return result
end

local function sameVectors(a,b,vertexCount)
    for index=1,vertexCount do
        local va=a and a[index] or nil
        local vb=b and b[index] or nil
        if (va and va.x or 0)~=(vb and vb.x or 0) or
                (va and va.y or 0)~=(vb and vb.y or 0) or
                (va and va.z or 0)~=(vb and vb.z or 0) then return false end
    end
    return true
end

local function changed(a,b,vertexCount)
    return not b or a.name~=b.name or a.convention~=b.convention or
        a.defaultWeight~=b.defaultWeight or
        not sameVectors(a.positionDeltas,b.positionDeltas,vertexCount) or
        not sameVectors(a.normalDeltas,b.normalDeltas,vertexCount)
end

function M.capture(base,current,vertexCount,topologySignature,createdIds,deletedBaseIds)
    if type(base)~='table' or type(current)~='table' or
            not finite(vertexCount) or vertexCount<1 or vertexCount%1~=0 then
        return nil,'invalid morph target catalog'
    end
    local patch={topologySignature=topologySignature,vertexCount=vertexCount,baseIds={},removedIds={},targets={}}
    local oldById,currentById={},{}
    for _,target in ipairs(base) do
        patch.baseIds[#patch.baseIds+1]=target.id
        oldById[target.id]=target
    end
    for _,target in ipairs(current) do currentById[target.id]=true end
    for _,target in ipairs(base) do
        if deletedBaseIds and deletedBaseIds[target.id] or not currentById[target.id] then
            patch.removedIds[#patch.removedIds+1]=target.id
        end
    end
    for _,target in ipairs(current) do
        local original=oldById[target.id]
        if createdIds and createdIds[target.id] then original=nil end
        if changed(target,original,vertexCount) then
            patch.targets[#patch.targets+1]={id=target.id,name=target.name,
                convention=target.convention,defaultWeight=target.defaultWeight,
                positions=sparse(target.positionDeltas,vertexCount),
                normals=sparse(target.normalDeltas,vertexCount)}
        end
    end
    if #patch.removedIds==0 and #patch.targets==0 then return nil end
    return patch
end

local function dense(values,vertexCount)
    if type(values)~='table' then error('invalid morph target deltas') end
    local result=Targets.zeroDeltas(vertexCount)
    for rawIndex,vector in pairs(values) do
        local index=tonumber(rawIndex)
        if not index or index%1~=0 or index<1 or index>vertexCount or type(vector)~='table' or
                not finite(vector.x) or not finite(vector.y) or not finite(vector.z) then
            error('invalid morph target delta index or vector')
        end
        result[index]={x=vector.x,y=vector.y,z=vector.z}
    end
    return result
end

function M.apply(meshDebug,patch,topologySignature)
    if type(patch)~='table' then return true,{} end
    if patch.topologySignature~=topologySignature then return false,'morph target topology mismatch' end
    local base,vertexCount,hasNormals=Targets.catalogFromDebug(meshDebug)
    if #base==0 and vertexCount==0 then vertexCount=patch.vertexCount end
    if vertexCount~=patch.vertexCount or type(patch.baseIds)~='table' or
            #base~=#patch.baseIds then return false,'morph target source MSH mismatch' end
    for index,target in ipairs(base) do
        if target.id~=patch.baseIds[index] then return false,'morph target source MSH mismatch' end
    end
    if type(patch.removedIds)~='table' or type(patch.targets)~='table' then
        return false,'invalid morph target project edits'
    end
    local removed={}
    for _,id in ipairs(patch.removedIds) do removed[id]=true end
    local baseById,usedNames={},{}
    for _,target in ipairs(base) do
        baseById[target.id]=true
        usedNames[target.name]=true
    end
    local edits={}
    for _,record in ipairs(patch.targets) do
        if type(record)~='table' or type(record.id)~='string' or
                type(record.name)~='string' or record.name=='' or
                type(record.convention)~='string' or not finite(record.defaultWeight) then
            return false,'invalid morph target project record'
        end
        usedNames[record.name]=true
        edits[#edits+1]={record=record,positions=dense(record.positions,vertexCount),
            normals=hasNormals and dense(record.normals,vertexCount) or nil,
            isNew=not baseById[record.id] or removed[record.id]}
    end
    local tempCounter=0
    local function temporaryName()
        local name
        repeat
            tempCounter=tempCounter+1
            name='__facial_project_restore_'..tempCounter
        until not usedNames[name]
        usedNames[name]=true
        return name
    end
    local idMap,addedRecords={},{}
    -- Add before removing old targets: embedded clips cannot lose their last
    -- target while keys still exist. Temporary names avoid name collisions.
    for _,edit in ipairs(edits) do
        if edit.isNew then
            local index=meshDebug:addFacialTarget(temporaryName(),edit.record.convention,
                edit.record.defaultWeight,edit.positions,edit.normals)
            if not index then return false,'could not restore new morph target' end
            addedRecords[#addedRecords+1]={oldId=edit.record.id,index=index}
        end
    end
    if #addedRecords>0 then
        local restored=meshDebug:getFacialExportData()
        for _,entry in ipairs(addedRecords) do
            local target=restored and restored.targets and restored.targets[entry.index]
            if not target or not target.id then return false,'could not identify restored morph target' end
            idMap[entry.oldId]=target.id
        end
    end
    for index=#base,1,-1 do
        if removed[base[index].id] and not meshDebug:removeFacialTarget(index) then
            return false,'could not remove project morph target'
        end
    end
    local remaining=Targets.catalogFromDebug(meshDebug)
    local currentById={}
    for index,target in ipairs(remaining) do currentById[target.id]=index end
    -- Temporary names also allow valid rename chains and swaps.
    for _,edit in ipairs(edits) do
        local targetIndex=currentById[idMap[edit.record.id] or edit.record.id]
        if not edit.isNew and targetIndex and remaining[targetIndex].name~=edit.record.name then
            local target=remaining[targetIndex]
            if not meshDebug:updateFacialTarget(targetIndex,temporaryName(),target.convention,
                    target.defaultWeight,target.positionDeltas,
                    #target.normalDeltas>0 and target.normalDeltas or nil) then
                return false,'could not prepare morph target rename'
            end
        end
    end
    for _,edit in ipairs(edits) do
        local record=edit.record
        local targetIndex=currentById[idMap[record.id] or record.id]
        if not targetIndex or not meshDebug:updateFacialTarget(targetIndex,record.name,
                record.convention,record.defaultWeight,edit.positions,edit.normals) then
            return false,'could not restore morph target edit'
        end
    end
    return true,idMap
end

return M
