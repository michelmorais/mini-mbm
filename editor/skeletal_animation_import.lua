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

   Offline skeletal-animation import and strict same-topology retarget helpers.
]]--

local function commonPrefix(names)
    if #names==0 then return '' end
    local prefix=names[1]
    for index=2,#names do
        local name=names[index]
        local length=math.min(#prefix,#name)
        local match=0
        while match<length and prefix:byte(match+1)==name:byte(match+1) do match=match+1 end
        prefix=prefix:sub(1,match)
        if prefix=='' then return '' end
    end
    local boundary=0
    for index=1,#prefix do
        local character=prefix:sub(index,index)
        if character==':' or character=='|' or character=='/' or character=='\\' or
                character=='_' or character=='.' or character=='-' then boundary=index end
    end
    return prefix:sub(1,boundary)
end

local function analyze(targetReport,sourceReport)
    if not targetReport or not sourceReport or targetReport.valid~=true or sourceReport.valid~=true or
            type(targetReport.bones)~='table' or type(sourceReport.bones)~='table' then
        return nil,'invalid_skeleton'
    end
    local targetNames,sourceNames={},{}
    for index,bone in ipairs(targetReport.bones) do targetNames[index]=bone.name end
    for index,bone in ipairs(sourceReport.bones) do sourceNames[index]=bone.name end
    local targetPrefix,sourcePrefix=commonPrefix(targetNames),commonPrefix(sourceNames)
    local targetByNormalized={}
    for index,bone in ipairs(targetReport.bones) do
        local normalized=bone.name:sub(#targetPrefix+1):lower()
        if normalized=='' or targetByNormalized[normalized] then return nil,'ambiguous_target_names' end
        targetByNormalized[normalized]={index=index,bone=bone,normalized=normalized}
    end
    local mapping={}
    for sourceIndex,sourceBone in ipairs(sourceReport.bones) do
        local normalized=sourceBone.name:sub(#sourcePrefix+1):lower()
        local target=targetByNormalized[normalized]
        if not target then return nil,'missing_bone',sourceBone.name end
        mapping[sourceIndex]={sourceIndex=sourceIndex,targetIndex=target.index,
            source=sourceBone,target=target.bone,normalized=normalized}
    end
    if #mapping~=#targetReport.bones then return nil,'bone_count_mismatch' end
    for sourceIndex,item in ipairs(mapping) do
        local sourceParent=item.source.parentIndex or 0
        local targetParent=item.target.parentIndex or 0
        local mappedParent=sourceParent>0 and mapping[sourceParent] and mapping[sourceParent].targetIndex or 0
        if targetParent~=mappedParent then return nil,'hierarchy_mismatch',item.source.name end
    end
    local targetMin,targetMax,sourceMin,sourceMax=nil,nil,nil,nil
    for _,item in ipairs(mapping) do
        local targetY=item.target.y or (item.target.globalMatrix and item.target.globalMatrix[14])
        local sourceY=item.source.y or (item.source.globalMatrix and item.source.globalMatrix[14])
        if targetY and sourceY then
            targetMin=targetMin and math.min(targetMin,targetY) or targetY
            targetMax=targetMax and math.max(targetMax,targetY) or targetY
            sourceMin=sourceMin and math.min(sourceMin,sourceY) or sourceY
            sourceMax=sourceMax and math.max(sourceMax,sourceY) or sourceY
        end
    end
    local targetHeight=targetMin and targetMax-targetMin or 0
    local sourceHeight=sourceMin and sourceMax-sourceMin or 0
    local heightRatio=sourceHeight>1e-8 and targetHeight/sourceHeight or 1
    for _,item in ipairs(mapping) do item.heightRatio=heightRatio end
    return {mapping=mapping,targetPrefix=targetPrefix,sourcePrefix=sourcePrefix,
        boneCount=#mapping,heightRatio=heightRatio},nil
end

local function multiplyQuaternion(a,b)
    return {x=a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        y=a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        z=a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        w=a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}
end

local function normalizedQuaternion(q)
    local length=math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w)
    if length<=1e-12 then return nil end
    return {x=q.x/length,y=q.y/length,z=q.z/length,w=q.w/length}
end

local function retargetKey(key,item)
    local sourceBind=item.source.localTranslation
    local sourceRotation=item.source.localRotation
    local sourceScale=item.source.localScale
    local targetBind=item.target.localTranslation
    local targetRotation=item.target.localRotation
    local targetScale=item.target.localScale
    if not sourceBind or not sourceRotation or not sourceScale or not targetBind or
            not targetRotation or not targetScale then return nil end
    local sourceLength=math.max(math.abs(item.source.length or 0),1e-8)
    local targetLength=math.max(math.abs(item.target.length or 0),1e-8)
    local ratio=targetLength/sourceLength
    if (item.source.parentIndex or 0)==0 then
        ratio=item.heightRatio or 1
    end
    local sourceKeyRotation=normalizedQuaternion(key.rotation)
    local sourceBindRotation=normalizedQuaternion(sourceRotation)
    local targetBindRotation=normalizedQuaternion(targetRotation)
    if not sourceKeyRotation or not sourceBindRotation or not targetBindRotation then return nil end
    local delta=normalizedQuaternion(multiplyQuaternion(
        {x=-sourceBindRotation.x,y=-sourceBindRotation.y,z=-sourceBindRotation.z,
            w=sourceBindRotation.w},sourceKeyRotation))
    local rotation=delta and normalizedQuaternion(multiplyQuaternion(targetBindRotation,delta)) or nil
    if not rotation or math.abs(sourceScale.x)<1e-12 or math.abs(sourceScale.y)<1e-12 or
            math.abs(sourceScale.z)<1e-12 then return nil end
    return {targetBind.x+(key.translation.x-sourceBind.x)*ratio,
        targetBind.y+(key.translation.y-sourceBind.y)*ratio,
        targetBind.z+(key.translation.z-sourceBind.z)*ratio,
        rotation.x,rotation.y,rotation.z,rotation.w,
        targetScale.x*(key.scale.x/sourceScale.x),
        targetScale.y*(key.scale.y/sourceScale.y),
        targetScale.z*(key.scale.z/sourceScale.z)}
end

local function buildPayload(analysis,clip)
    if not analysis or not clip or type(clip.tracks)~='table' then return nil,'invalid_clip' end
    local payload={}
    for _,track in ipairs(clip.tracks) do
        local item=analysis.mapping[track.boneIndex or 0]
        if not item then return nil,'unmapped_track',track.boneName end
        for _,key in ipairs(track.keys or {}) do
            local values=retargetKey(key,item)
            if not values then return nil,'invalid_key',track.boneName end
            local bezier=key.bezier or {}
            payload[#payload+1]={item.target.boneId,track.channelMask,key.time,
                values[1],values[2],values[3],values[4],values[5],values[6],values[7],
                values[8],values[9],values[10],key.easing or 0,bezier[1] or 0,
                bezier[2] or 0,bezier[3] or 1,bezier[4] or 1}
        end
    end
    return payload,nil
end

local function defaultClipName(path,clipName)
    local base=(path or ''):match('([^/\\]+)$') or ''
    base=base:gsub('%.[^%.]+$','')
    local candidate=base:match('([^_]+)$') or base
    if candidate=='' then candidate=clipName or 'Imported Clip' end
    return candidate:sub(1,1):upper()..candidate:sub(2)
end

return {analyze=analyze,buildPayload=buildPayload,defaultClipName=defaultClipName,
    commonPrefix=commonPrefix}
