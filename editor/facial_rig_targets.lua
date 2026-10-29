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
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Facial morph-target authoring model.

   This module owns editor-side catalog and candidate helpers only. Dense target storage remains in
   MeshDebug so the binary facial asset stays validated and PIMPL-owned by the engine.
]]--

local M={}

local function finite(value,fallback)
    value=tonumber(value)
    if not value or value~=value or value==math.huge or value==-math.huge then return fallback or 0 end
    return value
end

local function copyVector(value)
    return {x=finite(value and value.x,0),y=finite(value and value.y,0),z=finite(value and value.z,0)}
end

local function copyVectors(values)
    local result={}
    for index,value in ipairs(values or {}) do result[index]=copyVector(value) end
    return result
end

function M.new()
    return {targets={},selectedTarget=1,vertexCount=0,catalogDirty=true,
        nameDraft='',conventionDraft='ARKit',defaultWeight=0,modified=false,previewDirty=false,
        editMode=1,selectionMode=1,dx=0,dy=0,dz=0,rx=0,ry=0,rz=0,editUndo={},editRedo={}}
end

function M.catalogFromDebug(meshDebug)
    local result={}
    if not meshDebug then return result,0 end
    local exportData=meshDebug:getFacialExportData()
    if type(exportData)~='table' then return result,0 end
    local hasNormals=exportData.hasNormals==true
    for index,target in ipairs(exportData.targets or {}) do
        if type(target)=='table' and type(target.name)=='string' and target.name~='' then
            result[#result+1]={index=index,name=target.name,convention=target.convention or '',
                defaultWeight=finite(target.defaultWeight,0),id=target.id or '',
                hasNormalDeltas=hasNormals and type(target.normalDeltas)=='table' and #target.normalDeltas>0,
                positionDeltas=copyVectors(target.positionDeltas),
                normalDeltas=hasNormals and copyVectors(target.normalDeltas) or {}}
        end
    end
    return result,tonumber(exportData.vertexCount) or 0
end

function M.zeroDeltas(vertexCount)
    local result={}
    for index=1,math.max(0,math.floor(finite(vertexCount,0))) do
        result[index]={x=0,y=0,z=0}
    end
    return result
end

function M.targetNames(targets)
    local result={}
    for _,target in ipairs(targets or {}) do result[#result+1]=target.name end
    return result
end

function M.find(targets,index)
    return type(targets)=='table' and targets[index] or nil
end

function M.copyTargetData(target)
    if not target then return nil end
    return {name=target.name,convention=target.convention or '',defaultWeight=finite(target.defaultWeight,0),
        positionDeltas=copyVectors(target.positionDeltas),normalDeltas=copyVectors(target.normalDeltas)}
end

function M.clearEditHistory(authoring)
    authoring.editUndo={}
    authoring.editRedo={}
end

function M.recordEdit(authoring,index,before,after)
    authoring.editUndo=authoring.editUndo or {}
    authoring.editRedo={}
    authoring.editUndo[#authoring.editUndo+1]={index=index,before=before,after=after}
    while #authoring.editUndo>30 do table.remove(authoring.editUndo,1) end
end

local function rotateVector(value,rx,ry,rz)
    local x,y,z=value.x,value.y,value.z
    local cx,sx=math.cos(rx),math.sin(rx)
    y,z=y*cx-z*sx,y*sx+z*cx
    local cy,sy=math.cos(ry),math.sin(ry)
    x,z=x*cy+z*sy,-x*sy+z*cy
    local cz,sz=math.cos(rz),math.sin(rz)
    x,y=x*cz-y*sz,x*sz+y*cz
    return {x=x,y=y,z=z}
end

function M.allIndices(vertexCount)
    local result={}
    for index=1,math.max(0,math.floor(finite(vertexCount,0))) do result[index]=index end
    return result
end

function M.maskIndices(mask,vertexCount,topologySignature)
    if type(mask)~='table' or mask.enabled==false or
            mask.topologySignature~=nil and mask.topologySignature~=topologySignature then return {} end
    local result,seen={},{}
    for _,rawIndex in ipairs(mask.vertices or {}) do
        local index=tonumber(rawIndex)
        if index and index>=1 and index<=vertexCount and not seen[index] then
            seen[index]=true
            result[#result+1]=math.floor(index)
        end
    end
    table.sort(result)
    return result
end

function M.applyEdit(target,baseVertices,indices,edit)
    if type(target)~='table' or type(baseVertices)~='table' or type(indices)~='table' then return 0 end
    local positions=target.positionDeltas or {}
    local normals=target.normalDeltas or {}
    local mode=edit and edit.mode or 'translate'
    local pivot=edit and edit.pivot or {x=0,y=0,z=0}
    local changed=0
    local seen={}
    for _,rawIndex in ipairs(indices) do
        local index=tonumber(rawIndex)
        local base=baseVertices[index]
        if index and base and not seen[index] then
            index=math.floor(index); seen[index]=true
            local old=positions[index] or {x=0,y=0,z=0}
            local nextPosition
            if mode=='reset' then
                nextPosition={x=0,y=0,z=0}
            elseif mode=='rotate' then
                local current={x=base.x+old.x,y=base.y+old.y,z=base.z+old.z}
                local relative={x=current.x-pivot.x,y=current.y-pivot.y,z=current.z-pivot.z}
                local rotated=rotateVector(relative,edit.rx or 0,edit.ry or 0,edit.rz or 0)
                nextPosition={x=pivot.x+rotated.x-base.x,y=pivot.y+rotated.y-base.y,z=pivot.z+rotated.z-base.z}
            else
                nextPosition={x=old.x+(edit.dx or 0),y=old.y+(edit.dy or 0),z=old.z+(edit.dz or 0)}
            end
            if math.abs(nextPosition.x-old.x)>1e-7 or math.abs(nextPosition.y-old.y)>1e-7 or
                    math.abs(nextPosition.z-old.z)>1e-7 then changed=changed+1 end
            positions[index]=nextPosition
            if #normals>0 then
                if mode=='reset' then normals[index]={x=0,y=0,z=0}
                elseif mode=='rotate' then normals[index]=rotateVector(normals[index] or {x=0,y=0,z=0},edit.rx or 0,edit.ry or 0,edit.rz or 0) end
            end
        end
    end
    target.positionDeltas=positions
    target.normalDeltas=normals
    return changed
end

return M
