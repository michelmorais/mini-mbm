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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-------------------------------------------------------------------------------------------------------------------------|

   Facial rig skeleton binding.

   Materializes the editor's generated facial bones in an existing canonical MeshDebug skeleton.
   New bones are first appended as roots, then reparented while preserving their global bind. If
   any step fails, newly appended leaf bones are removed in reverse order.
]]--

local M={}

local function call(meshDebug,method,...)
    if not meshDebug or type(meshDebug[method])~='function' then
        return false,nil,'mesh skeleton API is unavailable: ' .. tostring(method)
    end
    local arguments={...}
    local result=table.pack(pcall(function() return meshDebug[method](meshDebug,table.unpack(arguments)) end))
    if not result[1] then return false,nil,tostring(result[2]) end
    return true,table.unpack(result,2,result.n)
end

local function report(meshDebug)
    local ok,value,errorMessage=call(meshDebug,'getSkeletonBindReport',false)
    if not ok or type(value)~='table' then
        return nil,errorMessage or 'mesh has no canonical skeleton'
    end
    if value.canonical~=true then return nil,'mesh skeleton is not canonical' end
    return value,nil
end

local function boneMap(skeletonReport)
    local byName={}
    for _,bone in ipairs(skeletonReport and skeletonReport.bones or {}) do
        if type(bone)=='table' and type(bone.name)=='string' and bone.name~='' then
            byName[bone.name]=bone.sourceIndex
        end
    end
    return byName
end

local function boneLength(bone)
    local head,tail=bone and bone.head,bone and bone.tail
    if type(head)~='table' or type(tail)~='table' then return 0.01 end
    local dx=(tail.x or 0)-(head.x or 0)
    local dy=(tail.y or 0)-(head.y or 0)
    local dz=(tail.z or 0)-(head.z or 0)
    return math.max(math.sqrt(dx*dx+dy*dy+dz*dz),0.01)
end

function M.inspect(meshDebug,rig)
    local result={version=1,valid=true,existingCount=0,missingCount=0,entries={},errors={},warnings={}}
    if not rig or rig.valid~=true then
        result.valid=false
        result.errors[#result.errors+1]='canonical facial rig is not valid'
        return result
    end
    local skeleton,errorMessage=report(meshDebug)
    if not skeleton then
        result.valid=false
        result.errors[#result.errors+1]=errorMessage
        return result
    end
    local existing=boneMap(skeleton)
    local rigBones=rig and rig.bones or {}
    if #rigBones==0 then
        result.valid=false
        result.errors[#result.errors+1]='canonical facial rig has no generated bones'
        return result
    end
    local seen={}
    for _,bone in ipairs(rigBones) do
        local name=tostring(bone.id or bone.name or '')
        local item={name=name,parent=bone.parent,status='missing',sourceIndex=nil}
        if name=='' then
            item.status='blocked'
            item.reason='generated facial bone has no name'
            result.valid=false
        elseif seen[name] then
            item.status='blocked'
            item.reason='generated facial bone name is duplicated'
            result.valid=false
        elseif existing[name] then
            seen[name]=true
            item.status='existing'
            item.sourceIndex=existing[name]
            result.existingCount=result.existingCount+1
        else
            seen[name]=true
            result.missingCount=result.missingCount+1
        end
        result.entries[#result.entries+1]=item
    end
    return result
end

local function removeAdded(meshDebug,added)
    for index=#added,1,-1 do
        call(meshDebug,'removeSkeletalBone',added[index])
    end
end

function M.apply(meshDebug,rig)
    local inspection=M.inspect(meshDebug,rig)
    if inspection.valid~=true then return false,0,inspection.errors[1] or 'facial skeleton binding is blocked' end
    if inspection.missingCount==0 then return true,0,nil end

    local skeleton,errorMessage=report(meshDebug)
    if not skeleton then return false,0,errorMessage end
    local indices=boneMap(skeleton)
    local added={}
    for _,bone in ipairs(rig.bones or {}) do
        local name=tostring(bone.id or bone.name or '')
        if not indices[name] then
            local head=bone.head or {x=0,y=0,z=0}
            local length=boneLength(bone)
            local ok,newIndex,errorValue=call(meshDebug,'addSkeletalBone',0,name,
                tonumber(head.x) or 0,tonumber(head.y) or 0,tonumber(head.z) or 0,
                math.max(length*0.15,0.001),length,true,false)
            if not ok or not newIndex then
                removeAdded(meshDebug,added)
                return false,0,errorValue or 'could not add generated facial bone'
            end
            indices[name]=newIndex
            added[#added+1]=newIndex
        end
    end

    for _,bone in ipairs(rig.bones or {}) do
        local childName=tostring(bone.id or bone.name or '')
        local parentName=bone.parent
        if parentName and indices[childName] and indices[parentName] and
                indices[childName]~=indices[parentName] then
            local ok,_,errorValue=call(meshDebug,'reparentSkeletalBone',indices[childName],
                indices[parentName],true)
            if not ok then
                removeAdded(meshDebug,added)
                return false,0,errorValue or 'could not establish generated facial hierarchy'
            end
        end
    end
    return true,#added,nil
end

return M
