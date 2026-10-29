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
        mode=1,newNameDraft='',newConventionDraft='ARKit',newDefaultWeight=0,
        nameDraft='',conventionDraft='ARKit',defaultWeight=0,modified=false,previewDirty=false,
        createdIds={},deletedBaseIds={},
        editMode=1,selectionMode=1,dx=0,dy=0,dz=0,rx=0,ry=0,rz=0,editUndo={},editRedo={},
        previewWeight=1,previewTarget=nil,selectedVertex=nil,brushMode=1,brushRadius=0.025,
        brushStrength=0.5,brushDx=0,brushDy=-0.01,brushDz=0}
end

function M.catalogFromDebug(meshDebug)
    local result={}
    if not meshDebug then return result,0,false end
    local exportData=meshDebug:getFacialExportData()
    if type(exportData)~='table' then return result,0,false end
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
    return result,tonumber(exportData.vertexCount) or 0,hasNormals
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

function M.selectionSet(authoring,masks,vertexCount,topologySignature)
    if (authoring.selectionMode or 1)==1 then return nil end
    if authoring.selectionMode==2 then
        return authoring.selectedVertex and {[authoring.selectedVertex]=true} or {}
    end
    local mask=masks and masks[(authoring.selectionMode or 1)-2]
    local set={}
    for _,index in ipairs(M.maskIndices(mask,vertexCount,topologySignature)) do set[index]=true end
    return set
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

-- Stroke changes stay in the Lua candidate until mouse release, then use one
-- MeshDebug transaction. This avoids a full target validation on every sample.
function M.brushStamp(target,baseVertices,point,normal,radius,strength,offset,smooth,allowed,indices)
    if not target or not baseVertices or not point or not normal or radius<=0 or strength<=0 then return 0 end
    local positions=target.positionDeltas or {}
    local affected={}
    local radiusSquared=radius*radius
    local nx,ny,nz=normal.x,normal.y,normal.z
    for _,index in ipairs(indices or M.allIndices(#baseVertices)) do
        local base=baseVertices[index]
        if base and (not allowed or allowed[index]) then
            local x,y,z=base.x-point.x,base.y-point.y,base.z-point.z
            local normalDistance=x*nx+y*ny+z*nz
            local tangentSquared=x*x+y*y+z*z-normalDistance*normalDistance
            if tangentSquared<=radiusSquared and math.abs(normalDistance)<=radius*0.45 then
                local distance=math.sqrt(math.max(0,tangentSquared))/radius
                local falloff=(1-distance*distance)^2*strength
                if falloff>1e-6 then affected[#affected+1]={index=index,weight=falloff} end
            end
        end
    end
    if #affected==0 then return 0 end
    local average={x=0,y=0,z=0}
    if smooth then
        local sum=0
        for _,entry in ipairs(affected) do
            local delta=positions[entry.index] or {x=0,y=0,z=0}
            average.x=average.x+delta.x*entry.weight
            average.y=average.y+delta.y*entry.weight
            average.z=average.z+delta.z*entry.weight
            sum=sum+entry.weight
        end
        if sum>0 then average.x,average.y,average.z=average.x/sum,average.y/sum,average.z/sum end
    end
    local changed=0
    for _,entry in ipairs(affected) do
        local old=positions[entry.index] or {x=0,y=0,z=0}
        local weight=entry.weight
        local nextPosition
        if smooth then
            nextPosition={x=old.x+(average.x-old.x)*weight,y=old.y+(average.y-old.y)*weight,
                z=old.z+(average.z-old.z)*weight}
        else
            nextPosition={x=old.x+(offset.x or 0)*weight,y=old.y+(offset.y or 0)*weight,
                z=old.z+(offset.z or 0)*weight}
        end
        if math.abs(nextPosition.x-old.x)>1e-8 or math.abs(nextPosition.y-old.y)>1e-8 or
                math.abs(nextPosition.z-old.z)>1e-8 then changed=changed+1 end
        positions[entry.index]=nextPosition
    end
    target.positionDeltas=positions
    return changed
end

return M
