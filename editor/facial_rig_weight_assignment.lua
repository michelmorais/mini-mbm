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
|-------------------------------------------------------------------------------------------------------------------------|

   Explicit facial weight assignment.

   This module creates detached edit batches for rigid roles and for the first deterministic
   facial-skeleton preview. Facial weights use verified role vertices and distance to generated
   bone segments; no Blender object boundary is inferred and no edit is applied during inspection.
]]--

local M={}

local function call(meshDebug,method,...)
    if not meshDebug or type(meshDebug[method])~='function' then
        return false,nil,'mesh geometry API is unavailable: ' .. tostring(method)
    end
    local arguments={...}
    local result=table.pack(pcall(function() return meshDebug[method](meshDebug,table.unpack(arguments)) end))
    if not result[1] then return false,nil,tostring(result[2]) end
    return true,table.unpack(result,2,result.n)
end

local function appendUnique(list,seen,value)
    if not seen[value] then
        seen[value]=true
        list[#list+1]=value
    end
end

local function reportBoneSet(meshDebug)
    local result,report={},nil
    if not meshDebug or type(meshDebug.getSkeletonBindReport)~='function' then
        return result,nil,'mesh skeleton report API is unavailable'
    end
    local ok,value=pcall(function() return meshDebug:getSkeletonBindReport(false) end)
    if not ok or type(value)~='table' then
        return result,nil,'mesh has no readable canonical skeleton'
    end
    report=value
    for _,bone in ipairs(report.bones or {}) do
        if type(bone)=='table' and type(bone.name)=='string' and bone.name~='' then result[bone.name]=true end
    end
    return result,report,nil
end

local function vertexPositions(meshDebug,topology)
    local positions={}
    local okSubset,subsetCount,errorMessage=call(meshDebug,'getTotalSubset',1)
    if not okSubset or not subsetCount then return nil,errorMessage or 'mesh subsets are unavailable' end
    local offset=0
    for subset=1,subsetCount do
        local okCount,count,countError=call(meshDebug,'getTotalVertex',1,subset)
        if not okCount or not count then return nil,countError or 'mesh vertex count is unavailable' end
        if count>0 then
            local okVertices,vertices,verticesError=call(meshDebug,'getVertex',1,subset,1,count)
            if not okVertices or type(vertices)~='table' then
                return nil,verticesError or 'mesh vertex positions are unavailable'
            end
            for localIndex,point in ipairs(vertices) do
                positions[offset+localIndex]={x=tonumber(point.x) or 0,y=tonumber(point.y) or 0,
                    z=tonumber(point.z) or 0}
            end
        end
        offset=offset+count
    end
    if topology and topology.vertexCount and offset~=topology.vertexCount then
        return nil,'mesh vertex order does not match the loaded topology'
    end
    return positions,nil
end

local function distanceToSegmentSquared(point,head,tail)
    local ax,ay,az=head.x,head.y,head.z
    local bx,by,bz=tail.x,tail.y,tail.z
    local dx,dy,dz=bx-ax,by-ay,bz-az
    local lengthSquared=dx*dx+dy*dy+dz*dz
    local t=0
    if lengthSquared>1e-12 then
        t=((point.x-ax)*dx+(point.y-ay)*dy+(point.z-az)*dz)/lengthSquared
        t=math.max(0,math.min(1,t))
    end
    local qx,qy,qz=ax+dx*t,ay+dy*t,az+dz*t
    local ex,ey,ez=point.x-qx,point.y-qy,point.z-qz
    return ex*ex+ey*ey+ez*ez
end

local function facialEdits(meshDebug,topology,vertices,rig,bones)
    local positions,errorMessage=vertexPositions(meshDebug,topology)
    if not positions then return nil,errorMessage end
    local candidates={}
    for _,bone in ipairs(rig and rig.bones or {}) do
        local name=tostring(bone.id or bone.name or '')
        if bones[name] and bone.head and bone.tail then
            candidates[#candidates+1]={name=name,head=bone.head,tail=bone.tail}
        end
    end
    if #candidates==0 then return nil,'generated facial bones are not present in the mesh skeleton' end
    local edits={}
    for _,vertex in ipairs(vertices) do
        local point=positions[vertex]
        if not point then return nil,'facial role contains a vertex without a position' end
        local scored={}
        for _,candidate in ipairs(candidates) do
            scored[#scored+1]={name=candidate.name,
                distance=distanceToSegmentSquared(point,candidate.head,candidate.tail)}
        end
        table.sort(scored,function(a,b) return a.distance<b.distance end)
        local influenceCount=math.min(4,#scored)
        local total=0
        for index=1,influenceCount do total=total+1/(scored[index].distance+1e-8) end
        local row={[1]=vertex}
        for slot=1,4 do
            local candidate=scored[slot]
            if slot<=influenceCount then
                row[slot*2]=candidate.name
                row[slot*2+1]=(1/(candidate.distance+1e-8))/total
            else
                row[slot*2]=nil
                row[slot*2+1]=0
            end
        end
        edits[#edits+1]=row
    end
    return edits,nil
end

local function roleVertices(project,entry,topology)
    local vertices,seen={},{}
    for _,rawIndex in ipairs(entry.vertexIndices or {}) do
        local index=tonumber(rawIndex)
        if not index or index<1 or index%1~=0 or topology and index>topology.vertexCount then
            return nil,'role contains an invalid mapped vertex'
        end
        appendUnique(vertices,seen,math.floor(index))
    end
    for _,mask in ipairs(project and project.masks or {}) do
        if mask.purpose~='target' and mask.enabled~=false and (mask.roleIndex or 1)==entry.index then
            if topology and mask.topologySignature~='' and mask.topologySignature~=topology.signature then
                return nil,'mask topology is stale: ' .. tostring(mask.name or '')
            end
            for _,rawIndex in ipairs(mask.vertices or {}) do
                local index=tonumber(rawIndex)
                if not index or index<1 or index%1~=0 or topology and index>topology.vertexCount then
                    return nil,'mask contains an invalid vertex: ' .. tostring(mask.name or '')
                end
                appendUnique(vertices,seen,math.floor(index))
            end
        end
    end
    table.sort(vertices)
    return vertices,nil
end

function M.inspect(meshDebug,project,topology,plan,rig)
    local result={version=1,valid=true,entries={},readyCount=0,blockedCount=0,totalVertexCount=0,
        errors={},warnings={}}
    if not plan or type(plan.entries)~='table' then
        result.valid=false
        result.errors[#result.errors+1]='binding plan is unavailable'
    end
    local bones,report,boneError=reportBoneSet(meshDebug)
    if boneError then
        result.valid=false
        result.errors[#result.errors+1]=boneError
    elseif report.canonical~=true then
        result.valid=false
        result.errors[#result.errors+1]='mesh skeleton is not canonical'
    end
    local hasWeights=false
    if meshDebug and type(meshDebug.hasSkeletalVertexWeights)=='function' then
        local ok,value=pcall(function() return meshDebug:hasSkeletalVertexWeights() end)
        hasWeights=ok and value==true
    end
    if not hasWeights then
        result.valid=false
        result.errors[#result.errors+1]='mesh has no canonical vertex weights'
    end
    for _,entry in ipairs(plan and plan.entries or {}) do
        local item={index=entry.index,name=entry.name,method=entry.method,bone=nil,vertices={},status='blocked',reasons={}}
        if entry.status~='ready' then
            item.reasons[#item.reasons+1]='binding plan entry is blocked'
        elseif entry.method~='rigid' and entry.method~='facial_skeleton' then
            item.reasons[#item.reasons+1]='this role policy is not enabled in this milestone'
        elseif entry.method=='rigid' and #(entry.bones or {})~=1 then
            item.reasons[#item.reasons+1]='rigid role must resolve to exactly one generated bone'
        else
            if entry.method=='rigid' then
                item.bone=entry.bones[1]
                if not bones[item.bone] then item.reasons[#item.reasons+1]='generated bone is not present in the mesh skeleton' end
            end
            local vertices,errorMessage=roleVertices(project,entry,topology)
            if not vertices then item.reasons[#item.reasons+1]=errorMessage
            elseif #vertices==0 then item.reasons[#item.reasons+1]='role has no explicit vertex mask'
            else
                item.vertices=vertices
                if entry.method=='facial_skeleton' then
                    local edits,facialError=facialEdits(meshDebug,topology,vertices,rig,bones)
                    if not edits then item.reasons[#item.reasons+1]=facialError else item.edits=edits end
                end
            end
        end
        if #item.reasons==0 then
            item.status='ready'
            result.readyCount=result.readyCount+1
            result.totalVertexCount=result.totalVertexCount+#item.vertices
        else
            result.blockedCount=result.blockedCount+1
        end
        result.entries[#result.entries+1]=item
    end
    if result.readyCount==0 then result.valid=false end
    if result.readyCount>0 and result.blockedCount>0 then
        result.warnings[#result.warnings+1]='some enabled roles remain blocked; only ready roles can be assigned'
    end
    return result
end

function M.buildEdits(assignment)
    if not assignment or assignment.valid~=true then return nil,'assignment is not valid' end
    local edits,seen={},{}
    for _,entry in ipairs(assignment.entries or {}) do
        if entry.status=='ready' then
            if entry.edits then
                for _,edit in ipairs(entry.edits) do
                    local vertex=edit[1]
                    if seen[vertex] then return nil,'assignment contains overlapping vertices' end
                    seen[vertex]=true
                    edits[#edits+1]=edit
                end
            else
                for _,vertex in ipairs(entry.vertices or {}) do
                    if seen[vertex] then return nil,'assignment contains overlapping vertices' end
                    seen[vertex]=true
                    edits[#edits+1]={[1]=vertex,[2]=entry.bone,[3]=1.0,[4]=nil,[5]=0.0,[6]=nil,[7]=0.0,[8]=nil,[9]=0.0}
                end
            end
        end
    end
    if #edits==0 then return nil,'assignment has no vertices' end
    return edits
end

function M.apply(meshDebug,assignment)
    local edits,errorMessage=M.buildEdits(assignment)
    if not edits then return false,errorMessage end
    if not meshDebug or type(meshDebug.setSkeletalVertexWeightsBatch)~='function' then
        return false,'mesh weight batch API is unavailable'
    end
    local ok,result=pcall(function() return meshDebug:setSkeletalVertexWeightsBatch(edits) end)
    if not ok or result~=true then return false,ok and 'MeshDebug rejected the weight batch' or tostring(result) end
    return true,#edits
end

return M
