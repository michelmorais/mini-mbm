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

   Built-in Armature Templates for the canonical Skeletal Animation Editor worktree.
]]--

local ARMATURE_NO_FINGERS_23 = {
    label = 'No Fingers (23)',
    schema = 'mini-mbm-armature-1',
    bones = {
        { name="root", parentIndex=0, tx=0, ty=0, tz=0, qx=0.707106829, qy=-8.42936885e-08, qz=8.42937027e-08, qw=0.707106829, sx=1, sy=1, sz=1, radius=0.0608495139, length=0.405663431, tailX=0, tailY=-1.77321162e-08, tailZ=-0.405663431, hasExplicitTail=true, connectedToParent=false },
        { name="hips", parentIndex=1, tx=0, ty=-1.77321162e-08, tz=-0.405663431, qx=-3.55271368e-15, qy=-5.21080312e-15, qz=-1.19209275e-07, qw=1, sx=1, sy=1, sz=1, radius=0.0301230755, length=0.200820506, tailX=-1.21666134e-28, tailY=-8.77814443e-09, tailZ=-0.200820506, hasExplicitTail=true, connectedToParent=true },
        { name="spine", parentIndex=2, tx=1.21666134e-28, ty=-8.39160208e-09, tz=-0.191977501, qx=3.55271368e-15, qy=3.10587968e-22, qz=7.10542736e-15, qw=1, sx=1, sy=1, sz=1, radius=0.0562481955, length=0.37498796, tailX=-1.35259986e-29, tailY=-1.63912333e-08, tailZ=-0.37498796, hasExplicitTail=true, connectedToParent=false },
        { name="upperleg.l", parentIndex=2, tx=-0.170945063, ty=8.95821728e-09, tz=-0.11358726, qx=0.999849081, qy=7.69868009e-08, qz=-6.69691502e-08, qw=0.0173739512, sx=1, sy=1.00000036, sz=1, radius=0.0340616219, length=0.227077484, tailX=-3.57965857e-09, tailY=-1.27447457e-08, tailZ=-0.227077439, hasExplicitTail=true, connectedToParent=false },
        { name="upperleg.r", parentIndex=2, tx=0.170945063, ty=8.95821728e-09, tz=-0.11358726, qx=0.999849081, qy=-7.69868009e-08, qz=6.69691502e-08, qw=0.0173739512, sx=1, sy=1.00000036, sz=1, radius=0.0340616219, length=0.227077484, tailX=3.57965857e-09, tailY=-1.27447457e-08, tailZ=-0.227077439, hasExplicitTail=true, connectedToParent=false },
        { name="chest", parentIndex=3, tx=1.19857686e-28, ty=-1.63912333e-08, tz=-0.37498796, qx=-3.55271368e-15, qy=2.5243549e-29, qz=0, qw=1, sx=1, sy=1, sz=1, radius=0.0385273844, length=0.256849229, tailX=-2.79487487e-28, tailY=-1.12272502e-08, tailZ=-0.256849229, hasExplicitTail=true, connectedToParent=false },
        { name="lowerleg.l", parentIndex=4, tx=-3.57965835e-09, ty=-1.36760683e-08, tz=-0.227077439, qx=0.106227122, qy=6.40321218e-08, qz=6.29175815e-07, qw=0.99434191, sx=1, sy=0.999999821, sz=0.99999994, radius=0.0224155579, length=0.149437055, tailX=5.25273336e-09, tailY=-3.52095242e-09, tailZ=-0.149437025, hasExplicitTail=true, connectedToParent=true },
        { name="lowerleg.r", parentIndex=5, tx=3.57965835e-09, ty=-1.36760683e-08, tz=-0.227077439, qx=0.106227122, qy=-6.40321147e-08, qz=-6.29175872e-07, qw=0.99434191, sx=1, sy=0.999999821, sz=0.99999994, radius=0.0224155579, length=0.149437055, tailX=-5.25273336e-09, tailY=-3.52093821e-09, tailZ=-0.149437025, hasExplicitTail=true, connectedToParent=true },
        { name="head", parentIndex=6, tx=4.50733209e-29, ty=-1.17494849e-08, tz=-0.268796623, qx=3.55271368e-15, qy=-5.04870979e-29, qz=0, qw=1, sx=1, sy=1, sz=1, radius=0.0385273956, length=0.256849289, tailX=7.39222107e-29, tailY=-1.12272289e-08, tailZ=-0.256849289, hasExplicitTail=true, connectedToParent=false },
        { name="upperarm.l", parentIndex=6, tx=-0.212007359, ty=7.90152939e-08, tz=-0.134132326, qx=-0.514124334, qy=0.485464901, qz=-0.485464871, qw=0.514124393, sx=0.99999994, sy=1, sz=1, radius=0.0362846069, length=0.241897374, tailX=1.07509024e-09, tailY=-1.60742619e-08, tailZ=-0.241897389, hasExplicitTail=true, connectedToParent=false },
        { name="upperarm.r", parentIndex=6, tx=0.212007359, ty=7.90152939e-08, tz=-0.134132326, qx=-0.514124334, qy=-0.485464901, qz=0.485464871, qw=0.514124393, sx=0.99999994, sy=1, sz=1, radius=0.0362846069, length=0.241897374, tailX=-1.07509024e-09, tailY=-1.60742619e-08, tailZ=-0.241897389, hasExplicitTail=true, connectedToParent=false },
        { name="foot.l", parentIndex=7, tx=5.25273336e-09, ty=-3.52095242e-09, tz=-0.149437025, qx=-0.455239922, qy=2.2968058e-07, qz=-2.79684855e-07, qw=0.890368819, sx=1, sy=1, sz=1, radius=0.0248475112, length=0.16565007, tailX=-2.73025469e-09, tailY=-2.66562026e-08, tailZ=-0.165650025, hasExplicitTail=true, connectedToParent=true },
        { name="foot.r", parentIndex=8, tx=-5.25273336e-09, ty=-3.52093821e-09, tz=-0.149437025, qx=-0.455239922, qy=-2.29680595e-07, qz=2.79684855e-07, qw=0.890368819, sx=1, sy=1, sz=1, radius=0.0248475112, length=0.16565007, tailX=2.73025469e-09, tailY=-2.66561955e-08, tailZ=-0.165650025, hasExplicitTail=true, connectedToParent=true },
        { name="lowerarm.l", parentIndex=10, tx=2.00641281e-09, ty=-1.60742619e-08, tz=-0.241897389, qx=-2.33294473e-09, qy=0.0552964881, qz=-1.46223314e-07, qw=0.998470068, sx=1.00000012, sy=1, sz=1, radius=0.0390065871, length=0.260043919, tailX=-1.74537007e-09, tailY=-2.31895214e-08, tailZ=-0.260043919, hasExplicitTail=true, connectedToParent=true },
        { name="lowerarm.r", parentIndex=11, tx=-2.00641281e-09, ty=-1.60742619e-08, tz=-0.241897389, qx=-2.33294473e-09, qy=-0.0552964881, qz=1.46223314e-07, qw=0.998470068, sx=1.00000012, sy=1, sz=1, radius=0.0390065871, length=0.260043919, tailX=1.74537007e-09, tailY=-2.31895214e-08, tailZ=-0.260043919, hasExplicitTail=true, connectedToParent=true },
        { name="toes.l", parentIndex=12, tx=-2.73025691e-09, ty=-2.10682671e-08, tz=-0.165650025, qx=1.69581753e-07, qy=0.391084433, qz=0.920354784, qw=3.98629425e-07, sx=1, sy=0.999999881, sz=0.99999994, radius=0.0248475131, length=0.165650085, tailX=1.46890233e-09, tailY=1.23358479e-09, tailZ=-0.165650025, hasExplicitTail=true, connectedToParent=true },
        { name="toes.r", parentIndex=13, tx=2.73025691e-09, ty=-2.106826e-08, tz=-0.165650025, qx=1.69581753e-07, qy=-0.391084433, qz=-0.920354784, qw=3.98629425e-07, sx=1, sy=0.999999881, sz=0.99999994, radius=0.0248475131, length=0.165650085, tailX=-1.46890233e-09, tailY=1.2335919e-09, tailZ=-0.165650025, hasExplicitTail=true, connectedToParent=true },
        { name="wrist.l", parentIndex=14, tx=-1.10506519e-08, ty=-2.31895214e-08, tz=-0.260043859, qx=3.07179038e-09, qy=-0.026658088, qz=9.47995815e-08, qw=0.999644637, sx=0.99999994, sy=1, sz=1, radius=0.0110738752, length=0.0738258362, tailX=5.4710636e-10, tailY=-3.44011717e-08, tailZ=-0.0738257542, hasExplicitTail=true, connectedToParent=false },
        { name="wrist.r", parentIndex=15, tx=1.10506519e-08, ty=-2.31895214e-08, tz=-0.260043859, qx=3.07179038e-09, qy=0.026658088, qz=-9.47995815e-08, qw=0.999644637, sx=0.99999994, sy=1, sz=1, radius=0.0110738752, length=0.0738258362, tailX=-5.4710636e-10, tailY=-3.44011717e-08, tailZ=-0.0738257542, hasExplicitTail=true, connectedToParent=false },
        { name="hand.l", parentIndex=18, tx=4.34956071e-10, ty=-3.44011752e-08, tz=-0.0738258138, qx=-4.3186933e-16, qy=-1.86264759e-09, qz=-1.8626527e-09, qw=1, sx=1, sy=1, sz=1, radius=0.0168015454, length=0.1120103, tailX=5.47110801e-10, tailY=-3.92972943e-08, tailZ=-0.112010278, hasExplicitTail=true, connectedToParent=false },
        { name="hand.r", parentIndex=19, tx=-4.34956071e-10, ty=-3.44011752e-08, tz=-0.0738258138, qx=-4.3186933e-16, qy=1.86264759e-09, qz=1.8626527e-09, qw=1, sx=1, sy=1, sz=1, radius=0.0168015454, length=0.1120103, tailX=-5.47110801e-10, tailY=-3.92972943e-08, tailZ=-0.112010278, hasExplicitTail=true, connectedToParent=false },
        { name="handslot.l", parentIndex=20, tx=-8.56819504e-10, ty=-0.057500042, tz=-0.0961251035, qx=-6.23656575e-08, qy=0.707106829, qz=-6.97569931e-08, qw=0.707106829, sx=1, sy=1, sz=1, radius=0.0168015398, length=0.11201027, tailX=2.08574651e-08, tailY=-1.01551301e-08, tailZ=-0.11201027, hasExplicitTail=true, connectedToParent=false },
        { name="handslot.r", parentIndex=21, tx=8.56819504e-10, ty=-0.057500042, tz=-0.0961251035, qx=-6.23656575e-08, qy=-0.707106829, qz=6.97569931e-08, qw=0.707106829, sx=1, sy=1, sz=1, radius=0.0168015398, length=0.11201027, tailX=-2.08574651e-08, tailY=-1.01551301e-08, tailZ=-0.11201027, hasExplicitTail=true, connectedToParent=false },
    },
}
local ARMATURE_TEMPLATES = {
    ARMATURE_NO_FINGERS_23,
}

local CANONICAL_NUMBER_FIELDS={'tx','ty','tz','qx','qy','qz','qw','sx','sy','sz',
    'radius','length','tailX','tailY','tailZ'}

local function isFinite(value)
    return type(value)=='number' and value==value and value~=math.huge and value~=-math.huge
end

local function multiplyQuaternion(a,b)
    return {x=a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        y=a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        z=a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        w=a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}
end

local function normalizeQuaternion(q)
    local length=math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w)
    if length<=1e-8 then return nil end
    return {x=q.x/length,y=q.y/length,z=q.z/length,w=q.w/length}
end

local function rotateRowVector(x,y,z,q)
    local xx,yy,zz=q.x*q.x,q.y*q.y,q.z*q.z
    local xy,xz,yz=q.x*q.y,q.x*q.z,q.y*q.z
    local xw,yw,zw=q.x*q.w,q.y*q.w,q.z*q.w
    return x*(1-2*(yy+zz))+y*(2*(xy-zw))+z*(2*(xz+yw)),
        x*(2*(xy+zw))+y*(1-2*(xx+zz))+z*(2*(yz-xw)),
        x*(2*(xz-yw))+y*(2*(yz+xw))+z*(1-2*(xx+yy))
end

local function calculateCanonicalBones(template)
    local calculated,names={},{ }
    for index,source in ipairs(template.bones) do
        if type(source.name)~='string' or source.name=='' or
                type(source.parentIndex)~='number' or source.parentIndex<0 or
                source.parentIndex%1~=0 or source.parentIndex>=index or names[source.name] then
            return nil
        end
        for _,field in ipairs(CANONICAL_NUMBER_FIELDS) do
            if not isFinite(source[field]) then return nil end
        end
        if source.radius<0 or source.length<0 or source.sx==0 or source.sy==0 or source.sz==0 or
                type(source.hasExplicitTail)~='boolean' or
                type(source.connectedToParent)~='boolean' then return nil end
        names[source.name]=true
        local rotation=normalizeQuaternion({x=source.qx or 0,y=source.qy or 0,
            z=source.qz or 0,w=source.qw or 0})
        if not rotation then return nil end
        local parent=source.parentIndex>0 and calculated[source.parentIndex] or nil
        if source.parentIndex>0 and not parent then return nil end
        local translation={x=source.tx or 0,y=source.ty or 0,z=source.tz or 0}
        local scale={x=source.sx or 1,y=source.sy or 1,z=source.sz or 1}
        local position={x=translation.x,y=translation.y,z=translation.z}
        local globalRotation=rotation
        local globalScale=scale
        if parent then
            local x,y,z=rotateRowVector(translation.x*parent.globalScale.x,
                translation.y*parent.globalScale.y,translation.z*parent.globalScale.z,
                parent.globalRotation)
            position={x=parent.position.x+x,y=parent.position.y+y,z=parent.position.z+z}
            globalRotation=normalizeQuaternion(multiplyQuaternion(rotation,parent.globalRotation))
            globalScale={x=scale.x*parent.globalScale.x,y=scale.y*parent.globalScale.y,
                z=scale.z*parent.globalScale.z}
        end
        calculated[index]={source=source,position=position,rotation=rotation,
            globalRotation=globalRotation,scale=scale,globalScale=globalScale}
    end
    return calculated
end

local function calculateSkeletonBounds(template)
    if not template or template.schema~='mini-mbm-armature-1' or
            type(template.bones)~='table' or #template.bones==0 then return nil end
    local calculated=calculateCanonicalBones(template)
    if not calculated then return nil end
    local bounds
    local function includePoint(x,y,z)
        if not bounds then
            bounds={minX=x,minY=y,minZ=z,maxX=x,maxY=y,maxZ=z}
            return
        end
        bounds.minX=math.min(bounds.minX,x); bounds.maxX=math.max(bounds.maxX,x)
        bounds.minY=math.min(bounds.minY,y); bounds.maxY=math.max(bounds.maxY,y)
        bounds.minZ=math.min(bounds.minZ,z); bounds.maxZ=math.max(bounds.maxZ,z)
    end
    for _,bone in ipairs(calculated) do
        local source=bone.source
        includePoint(bone.position.x,bone.position.y,bone.position.z)
        if source.hasExplicitTail then
            local tailX,tailY,tailZ=rotateRowVector(source.tailX*bone.globalScale.x,
                source.tailY*bone.globalScale.y,source.tailZ*bone.globalScale.z,
                bone.globalRotation)
            includePoint(bone.position.x+tailX,bone.position.y+tailY,bone.position.z+tailZ)
        end
    end
    bounds.height=bounds.maxY-bounds.minY
    return bounds,calculated
end

local function fitCanonical(template,target)
    local reference,calculated=calculateSkeletonBounds(template)
    if not target or not reference or type(target.minX)~='number' or
            type(target.minY)~='number' or type(target.minZ)~='number' or
            type(target.maxX)~='number' or type(target.maxY)~='number' or
            type(target.maxZ)~='number' then return nil,'invalid_template' end
    local targetHeight=target.maxY-target.minY
    if reference.height<=1e-6 or targetHeight<=1e-6 then return nil,'invalid_bounds' end
    local scale=targetHeight/reference.height
    local referenceAnchor={x=(reference.minX+reference.maxX)*0.5,y=reference.minY,
        z=(reference.minZ+reference.maxZ)*0.5}
    local targetAnchor={x=(target.minX+target.maxX)*0.5,y=target.minY,
        z=(target.minZ+target.maxZ)*0.5}
    local fitted={}
    for index,bone in ipairs(calculated) do
        local source=bone.source
        local translation={x=(source.tx or 0)*scale,y=(source.ty or 0)*scale,
            z=(source.tz or 0)*scale}
        if source.parentIndex==0 then
            translation={x=targetAnchor.x+(bone.position.x-referenceAnchor.x)*scale,
                y=targetAnchor.y+(bone.position.y-referenceAnchor.y)*scale,
                z=targetAnchor.z+(bone.position.z-referenceAnchor.z)*scale}
        end
        fitted[index]={source=source,parentIndex=source.parentIndex,index=index,
            translation=translation,rotation=bone.rotation,scale=bone.scale,
            radius=math.max(0,(source.radius or 0)*scale),
            length=math.max(0,(source.length or 0)*scale),
            tail={x=(source.tailX or 0)*scale,y=(source.tailY or 0)*scale,
                z=(source.tailZ or 0)*scale},hasExplicitTail=source.hasExplicitTail==true,
            connectedToParent=source.connectedToParent==true}
    end
    fitted.sourceBounds=reference; fitted.height=targetHeight; fitted.scale=scale
    return fitted,nil
end

local function fit(template,target)
    if not template or template.schema~='mini-mbm-armature-1' then
        return nil,'invalid_template'
    end
    return fitCanonical(template,target)
end

local function apply(meshD,template,target)
    local fitted,fitError=fit(template,target)
    if not fitted then return false,fitError end
    local report=meshD:getSkeletonBindReport(false)
    if report and report.canonical and (report.boneCount or 0)>0 then
        meshD:removeAllSkeletalData()
    end
    local root=fitted[1]
    if root.parentIndex~=0 then return false,'invalid_template' end
    local source=root.source
    meshD:initializeSkeletalSkeleton(source.name,root.translation.x,root.translation.y,
        root.translation.z,root.radius,root.length,root.hasExplicitTail~=false)
    meshD:setSkeletalBoneBind(1,root.translation.x,root.translation.y,root.translation.z,
        root.rotation.x,root.rotation.y,root.rotation.z,root.rotation.w,
        root.scale and root.scale.x or source.scaleX or 1,
        root.scale and root.scale.y or source.scaleY or 1,
        root.scale and root.scale.z or source.scaleZ or 1,root.radius,root.length)
    if root.tail then meshD:setSkeletalBoneTail(1,root.tail.x,root.tail.y,root.tail.z,
        root.hasExplicitTail,false) end
    for index=2,#fitted do
        local bone=fitted[index]
        local item=bone.source
        local created=meshD:addSkeletalBone(bone.parentIndex,item.name,
            bone.translation.x,bone.translation.y,bone.translation.z,bone.radius,bone.length,
            bone.hasExplicitTail~=false,bone.connectedToParent==true)
        if created~=index then return false,'invalid_template' end
        meshD:setSkeletalBoneBind(index,bone.translation.x,bone.translation.y,bone.translation.z,
            bone.rotation.x,bone.rotation.y,bone.rotation.z,bone.rotation.w,
            bone.scale and bone.scale.x or item.scaleX or 1,
            bone.scale and bone.scale.y or item.scaleY or 1,
            bone.scale and bone.scale.z or item.scaleZ or 1,bone.radius,bone.length)
        if bone.tail then meshD:setSkeletalBoneTail(index,bone.tail.x,bone.tail.y,bone.tail.z,
            bone.hasExplicitTail,false) end
    end
    return true,#fitted,fitted.height
end

local function fromReport(report,label)
    if not report or report.canonical~=true or report.valid~=true or
            type(report.bones)~='table' or #report.bones==0 then return nil,'invalid_skeleton' end
    local template={schema='mini-mbm-armature-1',label=label or 'Extracted Armature',bones={}}
    for index,bone in ipairs(report.bones) do
        local t,r,s,tail=bone.localTranslation,bone.localRotation,bone.localScale,bone.tailOffset
        if not t or not r or not s or not tail then return nil,'invalid_skeleton' end
        template.bones[index]={name=bone.name,parentIndex=bone.parentIndex,tx=t.x,ty=t.y,tz=t.z,
            qx=r.x,qy=r.y,qz=r.z,qw=r.w,sx=s.x,sy=s.y,sz=s.z,radius=bone.radius,
            length=bone.length,tailX=tail.x,tailY=tail.y,tailZ=tail.z,
            hasExplicitTail=bone.hasExplicitTail==true,
            connectedToParent=bone.connectedToParent==true}
    end
    if not calculateSkeletonBounds(template) then return nil,'invalid_skeleton' end
    return template
end


local function serialize(template)
    if not template or template.schema~='mini-mbm-armature-1' or
            not calculateSkeletonBounds(template) then return nil,'invalid_template' end
    local lines={"return {","    schema = 'mini-mbm-armature-1',",
        string.format('    label = %q,',template.label or 'Extracted Armature'),'    bones = {'}
    for _,bone in ipairs(template.bones) do
        lines[#lines+1]=string.format(
            '        { name=%q, parentIndex=%d, tx=%.9g, ty=%.9g, tz=%.9g, qx=%.9g, qy=%.9g, qz=%.9g, qw=%.9g, sx=%.9g, sy=%.9g, sz=%.9g, radius=%.9g, length=%.9g, tailX=%.9g, tailY=%.9g, tailZ=%.9g, hasExplicitTail=%s, connectedToParent=%s },',
            bone.name,bone.parentIndex,bone.tx,bone.ty,bone.tz,bone.qx,bone.qy,bone.qz,bone.qw,
            bone.sx,bone.sy,bone.sz,bone.radius,bone.length,bone.tailX,bone.tailY,bone.tailZ,
            tostring(bone.hasExplicitTail),tostring(bone.connectedToParent))
    end
    lines[#lines+1]='    },'; lines[#lines+1]='}'; return table.concat(lines,'\n')..'\n'
end

local function saveFile(path,template)
    local content,err=serialize(template)
    if not content then return false,err end
    local file,openError=io.open(path,'w')
    if not file then return false,openError end
    local ok,writeError=file:write(content); file:close()
    if not ok then return false,writeError end
    return true
end

local function loadFile(path)
    local chunk,loadError=loadfile(path,'t',{})
    if not chunk then return nil,loadError end
    local ok,template=pcall(chunk)
    if not ok then return nil,template end
    if type(template)~='table' or template.schema~='mini-mbm-armature-1' or
            not calculateSkeletonBounds(template) then return nil,'invalid_template' end
    return template
end

local ARMATURE_LABELS={}
for index,template in ipairs(ARMATURE_TEMPLATES) do
    ARMATURE_LABELS[index]=template.label
end

return {items=ARMATURE_TEMPLATES,labels=ARMATURE_LABELS,fit=fit,apply=apply,
    fromReport=fromReport,serialize=serialize,saveFile=saveFile,loadFile=loadFile}
