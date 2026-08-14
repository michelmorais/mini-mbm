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

   Skeletal Animation Editor — shared worktree shell
]]--

tImGui = require "ImGui"
tUtil = require "editor_utils"

local state = {
    fileName = nil,
    meshD = nil,
    preview = nil,
    comparisonPreview = nil,
    skeletalPreview = {clips={}, selected=1, method=1, duration=0, playing=false, paused=false,
        poseStress=false, comparisonReady=false},
    animationClipSelected = 1,
    workspace = 'weights',
    meshVisible = true,
    skeletonVisible = true,
    skeletonAlwaysOnTop = true,
    skeletonGizmo = {spheres={}, bones={}},
    skeletonGizmoGeneration = 0,
    analysisBoneHighlight = false,
    analysisBoneHighlightSphere = nil,
    proximityBoneHighlight = false,
    proximityBoneHighlightSphere = nil,
    targetBoneHighlight = false,
    targetBoneHighlightSphere = nil,
    markersAlwaysOnTop = true,
    analysisMarkersVisible = true,
    abruptMarkersVisible = true,
    boundaryMarkersVisible = true,
    info = nil,
    bindReport = nil,
    bindTreeOpenAll = false,
    bindRenameBoneId = nil,
    bindRenameName = '',
    bindReparentBoneId = nil,
    bindParentChoice = 1,
    bindPreserveGlobal = true,
    bindEditBoneId = nil,
    bindEdit = {},
    bindAddBoneId = nil,
    bindAddName = '',
    bindAddParentChoice = 1,
    bindAddTranslation = {x=0,y=1,z=0},
    bindChainBoneId = nil,
    bindChainPrefix = '',
    bindChainCount = 3,
    bindChainStep = {x=0,y=1,z=0},
    bindMirrorBoneId = nil,
    bindMirrorPrefix = 'mirror_',
    bindMirrorAxis = 1,
    bindMirrorConfirmed = false,
    bindWeightsBoneId = nil,
    bindWeightsConfirmed = false,
    bindRemoveBoneId = nil,
    bindRemoveConfirmed = false,
    bindRemoveReplacement = 1,
    bindRemoveDiscardTracks = false,
    bindRemoveReparentChildren = false,
    bindInitialName = 'root',
    bindInitialTranslation = {x=0,y=0,z=0},
    bindInitialRadius = 0.1,
    bindInitialLength = 1,
    modified = false,
    normalizeReport = nil,
    operationMode = 1, -- 1 inspect, 2 rigid, 3 normalize, 4 smooth, 5 repair abrupt
    selectionMode = 1, -- 1 AABB, 2 subset, 3 bone proximity
    subsetIndex = 1,
    boneIndex = 1,
    proximityRadius = 1,
    proximityNearestOnly = false,
    proximityCapsule = nil,
    analysisBoneIndex = 1,
    targetBoneIndex = 1,
    aabb = nil,
    aabbDragSensitivity = 0.001,
    analysis = nil,
    analysisDirty = true,
    rollbackPath = nil,
    rollbackModified = nil,
    selectionLines = nil,
    transitionLines = nil,
    heatmapLines = {},
    selectionBox = nil,
    transitionBox = nil,
    shellFaces = {
        minX={enabled=true,width=0}, maxX={enabled=true,width=0},
        minY={enabled=true,width=0}, maxY={enabled=true,width=0},
        minZ={enabled=true,width=0}, maxZ={enabled=true,width=0},
    },
    falloffMode = 2, -- 1 linear, 2 smooth
    heatmapEnabled = true,
    restrictBones = false,
    allowedBones = {},
    allowedBonesHighlight = false,
    hoveredAllowedBone = nil,
    smoothStrength = 0.5,
    smoothIterations = 1,
    abruptThreshold = 0.35,
    abruptDiagnostics = nil,
    abruptLines = nil,
    boundaryLines = nil,
    topologyAdjacency = nil,
    meshBounds = nil,
    aabbDragging = false,
    aabbDragPlane = nil,
    aabbDragOffset = nil,
    status = nil,
    statusError = false,
    cam = {azimuth = 0.35, elevation = 0.25, distance = 5, fx = 0, fy = 0, fz = 0},
}

local camera3d
local mouseDown = false
local mouseX, mouseY = 0, 0
local noMoveFlag = 0
local cameraMove = {forward=0, right=0, vertical=0}

local function isWeightLabWorkspace()
    return state.workspace == 'weights'
end

local function shouldShowSkeleton()
    return state.workspace=='bind' or (isWeightLabWorkspace() and state.skeletonVisible)
end

local function safeCall(fn)
    local result = table.pack(pcall(fn))
    if not result[1] then
        state.status = tostring(result[2])
        state.statusError = true
        return false
    end
    return true, table.unpack(result, 2, result.n)
end

local function setStatus(message, isError)
    state.status = message
    state.statusError = isError == true
end

local function shortName(path)
    return path and (path:match('([^/\\]+)$') or path) or ''
end

local function fileDir(path)
    return path and path:match('^(.*)[/\\]') or nil
end

local function destroyObject(object)
    if object then pcall(function() object:destroy() end) end
end

local function appendPoint(coords, x, y, z)
    coords[#coords + 1] = x
    coords[#coords + 1] = y
    coords[#coords + 1] = z
end

local function clearSelectionVisuals()
    destroyObject(state.selectionLines)
    destroyObject(state.transitionLines)
    destroyObject(state.selectionBox)
    destroyObject(state.transitionBox)
    destroyObject(state.proximityCapsule)
    destroyObject(state.abruptLines)
    destroyObject(state.boundaryLines)
    for _, object in ipairs(state.heatmapLines) do destroyObject(object) end
    state.selectionLines = nil
    state.transitionLines = nil
    state.selectionBox = nil
    state.transitionBox = nil
    state.proximityCapsule = nil
    state.abruptLines = nil
    state.boundaryLines = nil
    state.heatmapLines = {}
end

local function clearRollback()
    if state.rollbackPath then pcall(os.remove, state.rollbackPath) end
    state.rollbackPath = nil
    state.rollbackModified = nil
end

local function invalidateAnalysis()
    state.analysis = nil
    state.abruptDiagnostics = nil
    state.analysisDirty = true
    destroyObject(state.selectionLines)
    destroyObject(state.transitionLines)
    destroyObject(state.abruptLines)
    destroyObject(state.boundaryLines)
    for _, object in ipairs(state.heatmapLines) do destroyObject(object) end
    state.selectionLines = nil
    state.transitionLines = nil
    state.abruptLines = nil
    state.boundaryLines = nil
    state.heatmapLines = {}
end

local function computeAABB(meshD)
    local minX, minY, minZ = math.huge, math.huge, math.huge
    local maxX, maxY, maxZ = -math.huge, -math.huge, -math.huge
    local total = 0
    local okS, subsets = safeCall(function() return meshD:getTotalSubset(1) end)
    if not okS then return nil end
    for subset = 1, subsets do
        local okV, vertices = safeCall(function() return meshD:getTotalVertex(1, subset) end)
        if okV then
            for vertex = 1, vertices do
                local okP, p = safeCall(function() return meshD:getVertex(1, subset, vertex) end)
                if okP and p then
                    minX, minY, minZ = math.min(minX, p.x), math.min(minY, p.y), math.min(minZ, p.z)
                    maxX, maxY, maxZ = math.max(maxX, p.x), math.max(maxY, p.y), math.max(maxZ, p.z)
                    total = total + 1
                end
            end
        end
    end
    if total == 0 then return nil end
    return {minX=minX, minY=minY, minZ=minZ, maxX=maxX, maxY=maxY, maxZ=maxZ, total=total}
end

local function applyCamera()
    local c = state.cam
    local x = c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth)
    local y = c.fy + c.distance * math.sin(c.elevation)
    local z = c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
    camera3d:setPos(x, y, z)
    camera3d:setFocus(c.fx, c.fy, c.fz)
end

local function frameCamera(bounds)
    if not bounds then return end
    state.cam.fx = (bounds.minX + bounds.maxX) * 0.5
    state.cam.fy = (bounds.minY + bounds.maxY) * 0.5
    state.cam.fz = (bounds.minZ + bounds.maxZ) * 0.5
    local dx, dy, dz = bounds.maxX - bounds.minX, bounds.maxY - bounds.minY, bounds.maxZ - bounds.minZ
    state.cam.distance = math.max(0.1, math.sqrt(dx * dx + dy * dy + dz * dz) * 1.4)
    applyCamera()
end

local function cameraPosition()
    local c = state.cam
    return c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth),
           c.fy + c.distance * math.sin(c.elevation),
           c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
end

local function updateCameraKeyboard(delta)
    if cameraMove.forward == 0 and cameraMove.right == 0 and cameraMove.vertical == 0 then return end
    if tImGui.GetWantCaptureKeyboard() then return end
    local speed = state.cam.distance * 0.8 * math.max(delta or 0, 0)
    local dx, dz = 0, 0
    if cameraMove.forward ~= 0 or cameraMove.right ~= 0 then
        local fw, rg = camera3d:getNormal('F'), camera3d:getNormal('R')
        local fwLen = math.sqrt(fw.x*fw.x + fw.z*fw.z)
        local rgLen = math.sqrt(rg.x*rg.x + rg.z*rg.z)
        if fwLen > 1e-6 and rgLen > 1e-6 then
            dx = (fw.x/fwLen*cameraMove.forward + rg.x/rgLen*cameraMove.right) * speed
            dz = (fw.z/fwLen*cameraMove.forward + rg.z/rgLen*cameraMove.right) * speed
        end
    end
    state.cam.fx = state.cam.fx + dx
    state.cam.fy = state.cam.fy + cameraMove.vertical * speed
    state.cam.fz = state.cam.fz + dz
    applyCamera()
end

local function rayPlaneHit(sx, sy, point, normal)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, sx, sy)
    if not ok then return nil end
    local denom = normal.x*dx + normal.y*dy + normal.z*dz
    if math.abs(denom) < 1e-6 then return nil end
    local distance = ((point.x-ox)*normal.x + (point.y-oy)*normal.y + (point.z-oz)*normal.z) / denom
    if distance < 0 then return nil end
    return ox+dx*distance, oy+dy*distance, oz+dz*distance
end

local function rayHitsAABB(sx, sy, b)
    local ok, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, sx, sy)
    if not ok then return false end
    local near, far = -math.huge, math.huge
    local function slab(origin, direction, minimum, maximum)
        if math.abs(direction) < 1e-9 then return origin >= minimum and origin <= maximum end
        local a, c = (minimum-origin)/direction, (maximum-origin)/direction
        if a > c then a,c = c,a end
        near, far = math.max(near,a), math.min(far,c)
        return near <= far
    end
    return slab(ox,dx,b.minX,b.maxX) and slab(oy,dy,b.minY,b.maxY) and
           slab(oz,dz,b.minZ,b.maxZ) and far >= 0
end

local function createSelectionBox(b, r, g, blue)
    local p = {
        {b.minX,b.minY,b.minZ}, {b.maxX,b.minY,b.minZ}, {b.maxX,b.maxY,b.minZ}, {b.minX,b.maxY,b.minZ},
        {b.minX,b.minY,b.maxZ}, {b.maxX,b.minY,b.maxZ}, {b.maxX,b.maxY,b.maxZ}, {b.minX,b.maxY,b.maxZ},
    }
    local edges = {1,2,2,3,3,4,4,1,5,6,6,7,7,8,8,5,1,5,2,6,3,7,4,8}
    local coords = {}
    for _, i in ipairs(edges) do
        appendPoint(coords, p[i][1], p[i][2], p[i][3])
    end
    local box = line:new('3d', 0, 0, 0)
    box:add(coords)
    box:setColor(r, g, blue, 1)
    box:setPos(0,0,0)
    return box
end

local function rebuildSelectionBox()
    destroyObject(state.selectionBox)
    destroyObject(state.transitionBox)
    state.selectionBox, state.transitionBox = nil, nil
    if not state.meshD or state.selectionMode ~= 1 or not state.aabb then return end
    state.selectionBox = createSelectionBox(state.aabb, 0, 1, 1)
    local b,faces=state.aabb,state.shellFaces
    local outer={
        minX=b.minX-(faces.minX.enabled and faces.minX.width or 0),
        maxX=b.maxX+(faces.maxX.enabled and faces.maxX.width or 0),
        minY=b.minY-(faces.minY.enabled and faces.minY.width or 0),
        maxY=b.maxY+(faces.maxY.enabled and faces.maxY.width or 0),
        minZ=b.minZ-(faces.minZ.enabled and faces.minZ.width or 0),
        maxZ=b.maxZ+(faces.maxZ.enabled and faces.maxZ.width or 0),
    }
    if outer.minX<b.minX or outer.maxX>b.maxX or outer.minY<b.minY or
            outer.maxY>b.maxY or outer.minZ<b.minZ or outer.maxZ>b.maxZ then
        state.transitionBox = createSelectionBox({
            minX=outer.minX,minY=outer.minY,minZ=outer.minZ,
            maxX=outer.maxX,maxY=outer.maxY,maxZ=outer.maxZ,
        }, 1, 0.65, 0)
    end
end

local function getBones()
    local bones = {}
    local report=state.bindReport
    if not report or report.canonical~=true or type(report.bones)~='table' then return bones end
    for index,bone in ipairs(report.bones) do
        local global=bone.globalBindMatrix or {}
        local parentIndex=bone.parentIndex or 0
        local parent=parentIndex>0 and report.bones[parentIndex] or nil
        bones[#bones+1]={
            index=index,
            sourceIndex=bone.sourceIndex,
            boneId=bone.boneId,
            name=bone.name,
            parentName=parent and parent.name or nil,
            x=global[13] or 0,
            y=global[14] or 0,
            z=global[15] or 0,
            radius=bone.radius or 0,
            length=bone.length or 0,
        }
    end
    return bones
end

local function refreshBindReport()
    state.bindReport = nil
    if not state.meshD then return end
    local ok, report = safeCall(function() return state.meshD:getSkeletonBindReport() end)
    if ok then state.bindReport = report end
end

local function findBone(bones, name)
    for _, bone in ipairs(bones) do if bone.name == name then return bone end end
    return nil
end

local function rebuildProximityCapsule()
    destroyObject(state.proximityCapsule)
    state.proximityCapsule=nil
    if not state.meshD or state.selectionMode~=3 then return end
    local bones=getBones()
    local bone=bones[state.boneIndex]
    if not bone then return end
    local parent=findBone(bones,bone.parentName) or bone
    local ax,ay,az=bone.x,bone.y,bone.z
    local bx,by,bz=parent.x,parent.y,parent.z
    local dx,dy,dz=bx-ax,by-ay,bz-az
    local length=math.sqrt(dx*dx+dy*dy+dz*dz)
    local ux,uy,uz
    if length>1e-8 then ux,uy,uz=dx/length,dy/length,dz/length else ux,uy,uz=0,1,0 end
    local rx,ry,rz=math.abs(uy)<0.9 and 0 or 1,math.abs(uy)<0.9 and 1 or 0,0
    local vx,vy,vz=uy*rz-uz*ry,uz*rx-ux*rz,ux*ry-uy*rx
    local vLength=math.sqrt(vx*vx+vy*vy+vz*vz)
    vx,vy,vz=vx/vLength,vy/vLength,vz/vLength
    local wx,wy,wz=uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx
    local radius=math.max(tonumber(state.proximityRadius) or 0,0)
    local coords,segments={},24
    local function point(cx,cy,cz,angle)
        local c,s=math.cos(angle)*radius,math.sin(angle)*radius
        return cx+vx*c+wx*s,cy+vy*c+wy*s,cz+vz*c+wz*s
    end
    local function appendCircle(cx,cy,cz,p1x,p1y,p1z,p2x,p2y,p2z)
        for index=0,segments-1 do
            local a1,a2=index*math.pi*2/segments,(index+1)*math.pi*2/segments
            local c1,s1=math.cos(a1)*radius,math.sin(a1)*radius
            local c2,s2=math.cos(a2)*radius,math.sin(a2)*radius
            appendPoint(coords,cx+p1x*c1+p2x*s1,cy+p1y*c1+p2y*s1,cz+p1z*c1+p2z*s1)
            appendPoint(coords,cx+p1x*c2+p2x*s2,cy+p1y*c2+p2y*s2,cz+p1z*c2+p2z*s2)
        end
    end
    for _,center in ipairs({{ax,ay,az},{bx,by,bz}}) do
        appendCircle(center[1],center[2],center[3],vx,vy,vz,wx,wy,wz)
        appendCircle(center[1],center[2],center[3],ux,uy,uz,vx,vy,vz)
        appendCircle(center[1],center[2],center[3],ux,uy,uz,wx,wy,wz)
    end
    for index=0,7 do
        local angle=index*math.pi*2/8
        local x1,y1,z1=point(ax,ay,az,angle)
        local x2,y2,z2=point(bx,by,bz,angle)
        appendPoint(coords,x1,y1,z1); appendPoint(coords,x2,y2,z2)
    end
    local capsule=line:new('3d',0,0,0)
    capsule:add(coords)
    capsule:setColor(1,0.55,0,1)
    capsule:setPos(0,0,0)
    capsule.alwaysOnTop=true
    state.proximityCapsule=capsule
end

local function unitSphereVerts(latSegments,lonSegments)
    latSegments,lonSegments=latSegments or 8,lonSegments or 12
    local vertices={}
    local function push(x,y,z)
        vertices[#vertices+1]=x
        vertices[#vertices+1]=y
        vertices[#vertices+1]=z
    end
    local function xyz(theta,phi)
        local sine=math.sin(theta)
        return sine*math.cos(phi),math.cos(theta),sine*math.sin(phi)
    end
    for latitude=0,latSegments-1 do
        local theta1,theta2=latitude/latSegments*math.pi,(latitude+1)/latSegments*math.pi
        for longitude=0,lonSegments-1 do
            local phi1,phi2=longitude/lonSegments*math.pi*2,(longitude+1)/lonSegments*math.pi*2
            local x1,y1,z1=xyz(theta1,phi1)
            local x2,y2,z2=xyz(theta1,phi2)
            local x3,y3,z3=xyz(theta2,phi1)
            local x4,y4,z4=xyz(theta2,phi2)
            push(x1,y1,z1); push(x3,y3,z3); push(x4,y4,z4)
            push(x1,y1,z1); push(x4,y4,z4); push(x2,y2,z2)
        end
    end
    return vertices
end

local function orientedCylinderVerts(dx,dy,dz,radiusTop,radiusBottom,segments)
    segments=segments or 8
    local height=math.sqrt(dx*dx+dy*dy+dz*dz)
    if height<0.0001 then return {} end
    local ax,ay,az=dx/height,dy/height,dz/height
    local sx,sy,sz=0,1,0
    if math.abs(ay)>0.999 then sx,sy,sz=1,0,0 end
    local rx,ry,rz=ay*sz-az*sy,az*sx-ax*sz,ax*sy-ay*sx
    local rightLength=math.sqrt(rx*rx+ry*ry+rz*rz)
    rx,ry,rz=rx/rightLength,ry/rightLength,rz/rightLength
    local fx,fy,fz=ry*az-rz*ay,rz*ax-rx*az,rx*ay-ry*ax
    local vertices={}
    local function push(u,v,w)
        vertices[#vertices+1]=u*rx+v*ax+w*fx
        vertices[#vertices+1]=u*ry+v*ay+w*fy
        vertices[#vertices+1]=u*rz+v*az+w*fz
    end
    for index=0,segments-1 do
        local a1,a2=index/segments*math.pi*2,(index+1)/segments*math.pi*2
        local x1b,z1b=math.cos(a1)*radiusBottom,math.sin(a1)*radiusBottom
        local x2b,z2b=math.cos(a2)*radiusBottom,math.sin(a2)*radiusBottom
        local x1t,z1t=math.cos(a1)*radiusTop,math.sin(a1)*radiusTop
        local x2t,z2t=math.cos(a2)*radiusTop,math.sin(a2)*radiusTop
        push(x1b,0,z1b); push(x2t,height,z2t); push(x2b,0,z2b)
        push(x1b,0,z1b); push(x1t,height,z1t); push(x2t,height,z2t)
    end
    return vertices
end

local function visualZ(z)
    return z==0 and 0.0001 or z
end

local function destroySkeletonVisuals()
    for _,object in pairs(state.skeletonGizmo.spheres) do destroyObject(object) end
    for _,object in pairs(state.skeletonGizmo.bones) do destroyObject(object) end
    state.skeletonGizmo={spheres={},bones={}}
    destroyObject(state.analysisBoneHighlightSphere)
    state.analysisBoneHighlightSphere=nil
    destroyObject(state.proximityBoneHighlightSphere)
    state.proximityBoneHighlightSphere=nil
    destroyObject(state.targetBoneHighlightSphere)
    state.targetBoneHighlightSphere=nil
end

local function nextSkeletonNickname(prefix)
    state.skeletonGizmoGeneration=state.skeletonGizmoGeneration+1
    return prefix..state.skeletonGizmoGeneration
end

local function createBoneShape(x,y,z,vertices,nickname,r,g,b,a)
    local object=shape:new('3d',x,y,visualZ(z))
    object:create(vertices,nil,nextSkeletonNickname(nickname))
    object:setColor(r,g,b,a)
    object.visible=shouldShowSkeleton()
    object.alwaysOnTop=state.skeletonAlwaysOnTop
    return object
end

local function updateSkeletonVisibility()
    local weightWorkspace=isWeightLabWorkspace()
    local analyzedBone=weightWorkspace and state.analysisBoneHighlight and
        getBones()[state.analysisBoneIndex] or nil
    local targetBone=weightWorkspace and state.targetBoneHighlight and
        getBones()[state.targetBoneIndex] or nil
    local proximityBone=weightWorkspace and state.selectionMode==3 and state.proximityBoneHighlight and
        getBones()[state.boneIndex] or nil
    for name,object in pairs(state.skeletonGizmo.spheres) do
        -- Highlight spheres replace their regular joints instead of occupying the same surface
        -- in the same always-on-top depth pass.
        local highlighted=(analyzedBone and name==analyzedBone.name) or
            (targetBone and name==targetBone.name) or
            (proximityBone and name==proximityBone.name)
        object.visible=shouldShowSkeleton() and not highlighted
    end
    for _,object in pairs(state.skeletonGizmo.bones) do object.visible=shouldShowSkeleton() end
end

local function applyWorkspaceVisibility()
    local weightWorkspace=isWeightLabWorkspace()
    local runtimeWorkspace=state.workspace=='runtime'
    local analysisVisible=weightWorkspace and state.analysisMarkersVisible

    if state.preview then
        state.preview.visible=state.meshVisible
        pcall(function()
            local x=0
            if runtimeWorkspace and state.skeletalPreview.poseStress then
                local bounds=state.meshBounds
                local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                    bounds.maxZ-bounds.minZ) or 1
                x=-extent*0.65
            end
            state.preview:setPos(x,0,0)
        end)
    end
    if state.comparisonPreview then
        state.comparisonPreview.visible=state.meshVisible and runtimeWorkspace and
            state.skeletalPreview.poseStress
    end
    if state.selectionBox then state.selectionBox.visible=weightWorkspace end
    if state.transitionBox then state.transitionBox.visible=weightWorkspace end
    if state.proximityCapsule then state.proximityCapsule.visible=weightWorkspace end
    if state.selectionLines then state.selectionLines.visible=analysisVisible end
    if state.transitionLines then state.transitionLines.visible=analysisVisible end
    for _,marker in ipairs(state.heatmapLines) do marker.visible=analysisVisible end
    if state.abruptLines then
        state.abruptLines.visible=weightWorkspace and state.abruptMarkersVisible
    end
    if state.boundaryLines then
        state.boundaryLines.visible=weightWorkspace and state.boundaryMarkersVisible
    end
    if state.analysisBoneHighlightSphere then
        state.analysisBoneHighlightSphere.visible=shouldShowSkeleton() and weightWorkspace and
            state.analysisBoneHighlight
    end
    if state.proximityBoneHighlightSphere then
        state.proximityBoneHighlightSphere.visible=shouldShowSkeleton() and weightWorkspace and
            state.proximityBoneHighlight
    end
    if state.targetBoneHighlightSphere then
        state.targetBoneHighlightSphere.visible=shouldShowSkeleton() and weightWorkspace and
            state.targetBoneHighlight
    end
    local selectedBindBone=state.workspace=='bind' and getBones()[state.boneIndex] or nil
    for name,object in pairs(state.skeletonGizmo.spheres) do
        if weightWorkspace and name==state.hoveredAllowedBone then
            object:setColor(1,0.45,0.05,1)
        elseif weightWorkspace and state.allowedBonesHighlight and state.allowedBones[name] then
            object:setColor(0.1,0.85,1,0.95)
        elseif selectedBindBone and name==selectedBindBone.name then
            object:setColor(0.1,0.85,1,1)
        else
            object:setColor(1,0,1,0.85)
        end
    end
    for boneId,object in pairs(state.skeletonGizmo.bones) do
        if selectedBindBone and boneId==selectedBindBone.boneId then
            object:setColor(0.1,0.85,1,1)
        else
            object:setColor(1,0,1,0.75)
        end
    end
    updateSkeletonVisibility()
end

local function setWorkspace(workspace)
    if state.workspace==workspace then return end
    state.workspace=workspace
    state.aabbDragging=false
    state.aabbDragPlane=nil
    state.aabbDragOffset=nil
    applyWorkspaceVisibility()
    if state.meshBounds then
        local bounds={}
        for key,value in pairs(state.meshBounds) do bounds[key]=value end
        if workspace=='runtime' and state.skeletalPreview.poseStress then
            local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                bounds.maxZ-bounds.minZ)
            local separation=extent*0.65
            bounds.minX=bounds.minX-separation
            bounds.maxX=bounds.maxX+separation
        end
        frameCamera(bounds)
    end
end

local function rebuildProximityBoneHighlight()
    destroyObject(state.proximityBoneHighlightSphere)
    state.proximityBoneHighlightSphere=nil
    if state.selectionMode~=3 or not state.proximityBoneHighlight then
        updateSkeletonVisibility()
        return
    end
    local bone=getBones()[state.boneIndex]
    if not bone then
        updateSkeletonVisibility()
        return
    end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(bone.radius or 0,extent*0.018,0.001)
    local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
        'swl_proximity_bone_highlight_',1,0.45,0.05,0.95)
    sphere:setScale(radius*1.9,radius*1.9,radius*1.9)
    sphere.visible=shouldShowSkeleton() and isWeightLabWorkspace()
    sphere.alwaysOnTop=true
    state.proximityBoneHighlightSphere=sphere
    updateSkeletonVisibility()
end

local function updateAllowedBoneColors()
    local selectedBindBone=state.workspace=='bind' and getBones()[state.boneIndex] or nil
    for name,object in pairs(state.skeletonGizmo.spheres) do
        if selectedBindBone and name==selectedBindBone.name then
            object:setColor(0.1,0.85,1,1)
        elseif not isWeightLabWorkspace() then
            object:setColor(1,0,1,0.85)
        elseif name==state.hoveredAllowedBone then
            object:setColor(1,0.45,0.05,1)
        elseif state.allowedBonesHighlight and state.allowedBones[name] then
            object:setColor(0.1,0.85,1,0.95)
        else
            object:setColor(1,0,1,0.85)
        end
    end
end

local function rebuildAnalysisBoneHighlight()
    destroyObject(state.analysisBoneHighlightSphere)
    state.analysisBoneHighlightSphere=nil
    if not state.analysisBoneHighlight then
        updateSkeletonVisibility()
        return
    end
    local bone=getBones()[state.analysisBoneIndex]
    if not bone then
        updateSkeletonVisibility()
        return
    end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(bone.radius or 0,extent*0.018,0.001)
    local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
        'swl_analysis_bone_highlight_',1,1,0,0.95)
    sphere:setScale(radius*1.6,radius*1.6,radius*1.6)
    sphere.visible=shouldShowSkeleton() and isWeightLabWorkspace()
    sphere.alwaysOnTop=true
    state.analysisBoneHighlightSphere=sphere
    updateSkeletonVisibility()
end

local function rebuildTargetBoneHighlight()
    destroyObject(state.targetBoneHighlightSphere)
    state.targetBoneHighlightSphere=nil
    if not state.targetBoneHighlight then
        updateSkeletonVisibility()
        return
    end
    local bone=getBones()[state.targetBoneIndex]
    if not bone then
        updateSkeletonVisibility()
        return
    end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    local radius=math.max(bone.radius or 0,extent*0.018,0.001)
    local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
        'swl_target_bone_highlight_',0.1,1,0.2,0.9)
    sphere:setScale(radius*1.75,radius*1.75,radius*1.75)
    sphere.visible=shouldShowSkeleton() and isWeightLabWorkspace()
    sphere.alwaysOnTop=true
    state.targetBoneHighlightSphere=sphere
    updateSkeletonVisibility()
end

local function rebuildSkeletonVisuals()
    destroySkeletonVisuals()
    local bones=getBones()
    local byName={}
    for _,bone in ipairs(bones) do byName[bone.name]=bone end
    local bounds=state.meshBounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    for _,bone in ipairs(bones) do
        local radius=math.max(bone.radius or 0,extent*0.006,0.001)
        local sphere=createBoneShape(bone.x,bone.y,bone.z,unitSphereVerts(),
            'swl_bone_joint_',1,0,1,0.85)
        sphere:setScale(radius,radius,radius)
        state.skeletonGizmo.spheres[bone.name]=sphere
        local parent=bone.parentName and byName[bone.parentName]
        if parent then
            local dx,dy,dz=bone.x-parent.x,bone.y-parent.y,bone.z-parent.z
            local parentRadius=math.max(parent.radius or 0,extent*0.006,0.001)
            if dx*dx+dy*dy+dz*dz>0.000001 then
                local link=createBoneShape(parent.x,parent.y,parent.z,
                    orientedCylinderVerts(dx,dy,dz,radius*0.5,parentRadius*0.5,8),
                    'swl_bone_link_',1,0,1,0.75)
                -- A visual bone segment belongs to its child transform: parent joint -> child
                -- joint. Keying by the child's stable ID lets tree selection highlight the exact
                -- incoming segment even after rename or future hierarchy reordering.
                state.skeletonGizmo.bones[bone.boneId]=link
            end
        end
    end
    rebuildAnalysisBoneHighlight()
    rebuildProximityBoneHighlight()
    rebuildTargetBoneHighlight()
end

local function pointSegmentDistanceSquared(p, a, b)
    local dx, dy, dz = b.x-a.x, b.y-a.y, b.z-a.z
    local lengthSquared = dx*dx + dy*dy + dz*dz
    if lengthSquared < 1e-12 then
        local x, y, z = p.x-a.x, p.y-a.y, p.z-a.z
        return x*x + y*y + z*z
    end
    local t = ((p.x-a.x)*dx + (p.y-a.y)*dy + (p.z-a.z)*dz) / lengthSquared
    t = math.max(0, math.min(1, t))
    local x, y, z = p.x-(a.x+dx*t), p.y-(a.y+dy*t), p.z-(a.z+dz*t)
    return x*x + y*y + z*z
end

local function collectVertices()
    local vertices = {}
    local okS, subsets = safeCall(function() return state.meshD:getTotalSubset(1) end)
    if not okS then return vertices end
    local offset = 0
    for subset = 1, subsets do
        local okV, total = safeCall(function() return state.meshD:getTotalVertex(1, subset) end)
        total = okV and total or 0
        for vertex = 1, total do
            local okP, p = safeCall(function() return state.meshD:getVertex(1, subset, vertex) end)
            if okP and p then
                vertices[#vertices+1] = {globalIndex=offset+vertex, subset=subset, point=p}
            end
        end
        offset = offset + total
    end
    return vertices
end

local function pointInsideAABB(p, b)
    return p.x >= b.minX and p.x <= b.maxX and p.y >= b.minY and p.y <= b.maxY and
           p.z >= b.minZ and p.z <= b.maxZ
end

local function hasTransitionShell()
    for _,face in pairs(state.shellFaces) do
        if face.enabled and face.width>0 then return true end
    end
    return false
end

local function transitionAlpha(p, b)
    if pointInsideAABB(p, b) then return 1, 'core' end
    local faces=state.shellFaces
    local distances={
        p.x<b.minX and {'minX',b.minX-p.x} or (p.x>b.maxX and {'maxX',p.x-b.maxX} or nil),
        p.y<b.minY and {'minY',b.minY-p.y} or (p.y>b.maxY and {'maxY',p.y-b.maxY} or nil),
        p.z<b.minZ and {'minZ',b.minZ-p.z} or (p.z>b.maxZ and {'maxZ',p.z-b.maxZ} or nil),
    }
    local t=0
    for _,distance in pairs(distances) do
        local face=faces[distance[1]]
        if not face.enabled or face.width<=0 then return nil end
        t=math.max(t,distance[2]/face.width)
    end
    if t >= 1 then return nil end
    if state.falloffMode == 2 then t = t*t*(3-2*t) end
    return math.max(0,math.min(1,1-t)), 'shell'
end

local function buildVertexMarkers(vertices, r, g, b, extent)
    if #vertices == 0 then return nil end
    local size = math.max(extent*0.006,0.001)
    local coords,step={},math.max(1,math.ceil(#vertices/500))
    for i=1,#vertices,step do
        local p=vertices[i].point
        appendPoint(coords,p.x-size,p.y,p.z); appendPoint(coords,p.x+size,p.y,p.z)
        appendPoint(coords,p.x,p.y-size,p.z); appendPoint(coords,p.x,p.y+size,p.z)
    end
    local marks=line:new('3d',0,0,0)
    marks:add(coords); marks:setColor(r,g,b,1); marks:setPos(0,0,0)
    marks.alwaysOnTop=state.markersAlwaysOnTop
    return marks
end

local function buildEdgeLines(edges,r,g,b)
    if #edges==0 then return nil end
    local coords={}
    for _,edge in ipairs(edges) do
        appendPoint(coords,edge[1].point.x,edge[1].point.y,edge[1].point.z)
        appendPoint(coords,edge[2].point.x,edge[2].point.y,edge[2].point.z)
    end
    local lines=line:new('3d',0,0,0)
    lines:add(coords)
    lines:setColor(r,g,b,1)
    lines:setPos(0,0,0)
    lines.alwaysOnTop=state.markersAlwaysOnTop
    return lines
end

local heatmapColors = {
    {0.1,0.25,1}, {0,0.85,1}, {0.1,1,0.25}, {1,0.9,0}, {1,0.45,0}, {1,0.1,0},
}

local function vertexWeightForBone(globalIndex,boneName)
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    if not ok then return 0,{} end
    local influences={{n1,w1},{n2,w2},{n3,w3},{n4,w4}}
    local weight=0
    for _,pair in ipairs(influences) do
        if pair[1]==boneName then weight=weight+(tonumber(pair[2]) or 0) end
    end
    return math.max(0,math.min(1,weight)),influences
end

local function rebuildAnalysisMarkers(core,shell,extent)
    destroyObject(state.selectionLines); destroyObject(state.transitionLines)
    for _,object in ipairs(state.heatmapLines) do destroyObject(object) end
    state.selectionLines,state.transitionLines=nil,nil
    state.heatmapLines={}
    if not state.heatmapEnabled then
        state.selectionLines=buildVertexMarkers(core,1,0,1,extent)
        state.transitionLines=buildVertexMarkers(shell,0.9,0.9,0.9,extent)
        if state.selectionLines then state.selectionLines.visible=state.analysisMarkersVisible end
        if state.transitionLines then state.transitionLines.visible=state.analysisMarkersVisible end
        return
    end
    local buckets={{},{},{},{},{},{}}
    for _,vertex in ipairs(state.analysis.vertices) do
        local index=math.min(6,math.floor((vertex.targetWeight or 0)*6)+1)
        buckets[index][#buckets[index]+1]=vertex
    end
    for index,vertices in ipairs(buckets) do
        local color=heatmapColors[index]
        local marker=buildVertexMarkers(vertices,color[1],color[2],color[3],extent)
        if marker then
            marker.visible=state.analysisMarkersVisible
            state.heatmapLines[#state.heatmapLines+1]=marker
        end
    end
end

local function analyzeSelection()
    if not state.meshD then return end
    state.abruptDiagnostics=nil
    destroyObject(state.abruptLines); state.abruptLines=nil
    destroyObject(state.boundaryLines); state.boundaryLines=nil
    local allVertices = collectVertices()
    local selected, bones = {}, getBones()
    if state.selectionMode == 1 then
        local b = state.aabb
        for _, v in ipairs(allVertices) do
            local alpha,region=transitionAlpha(v.point,b)
            if alpha then v.blendAlpha,v.region=alpha,region; selected[#selected+1]=v end
        end
    elseif state.selectionMode == 2 then
        for _, v in ipairs(allVertices) do
            if v.subset == state.subsetIndex then v.blendAlpha,v.region=1,'core'; selected[#selected+1] = v end
        end
    else
        local target = bones[state.boneIndex]
        if target then
            local targetSegment={a=target,b=findBone(bones,target.parentName) or target}
            local segments = nil
            if state.proximityNearestOnly then
                segments={}
                for _, bone in ipairs(bones) do
                    segments[bone.name] = {a=bone, b=findBone(bones, bone.parentName) or bone}
                end
            end
            local radiusSquared = state.proximityRadius * state.proximityRadius
            for _, v in ipairs(allVertices) do
                local targetDistance=pointSegmentDistanceSquared(v.point,targetSegment.a,targetSegment.b)
                local selectedByOwnership=true
                if segments then
                    local nearestName,nearestDistance
                    for name,segment in pairs(segments) do
                        local distance=pointSegmentDistanceSquared(v.point,segment.a,segment.b)
                        if not nearestDistance or distance<nearestDistance then
                            nearestName,nearestDistance=name,distance
                        end
                    end
                    selectedByOwnership=nearestName==target.name
                end
                if selectedByOwnership and targetDistance<=radiusSquared then
                    v.blendAlpha,v.region=1,'core'
                    selected[#selected+1] = v
                end
            end
        end
    end

    local missing, invalidSum, unknown, disallowed = 0, 0, 0, 0
    local heatmapInfluenced,heatmapMinPositive,heatmapMax=0,math.huge,0
    local known = {}
    for _, bone in ipairs(bones) do known[bone.name] = true end
    local analysisTarget=bones[state.analysisBoneIndex]
    for _, vertex in ipairs(selected) do
        local targetWeight,influences=vertexWeightForBone(vertex.globalIndex,
            analysisTarget and analysisTarget.name or '')
        vertex.targetWeight=targetWeight
        if targetWeight>1e-8 then
            heatmapInfluenced=heatmapInfluenced+1
            heatmapMinPositive=math.min(heatmapMinPositive,targetWeight)
            heatmapMax=math.max(heatmapMax,targetWeight)
        end
        local n1,w1=influences[1][1],influences[1][2]
        local n2,w2=influences[2][1],influences[2][2]
        local n3,w3=influences[3][1],influences[3][2]
        local n4,w4=influences[4][1],influences[4][2]
        local ok=#influences>0
        if not ok or not n1 then
            missing = missing + 1
        else
            local sum = 0
            for _, pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
                if pair[1] then
                    sum = sum + (pair[2] or 0)
                    if not known[pair[1]] then unknown = unknown + 1 end
                    if state.restrictBones and not state.allowedBones[pair[1]] then disallowed=disallowed+1 end
                end
            end
            if math.abs(sum - 1) > 0.001 then invalidSum = invalidSum + 1 end
        end
    end
    local core,shell={},{}
    for _,vertex in ipairs(selected) do
        if vertex.region=='shell' then shell[#shell+1]=vertex else core[#core+1]=vertex end
    end
    state.analysis = {vertices=selected, allVertices=allVertices, core=core, shell=shell, missing=missing,
        invalidSum=invalidSum, unknown=unknown, disallowed=disallowed, totalMesh=#allVertices,
        heatmapBoneName=analysisTarget and analysisTarget.name or nil,
        heatmapInfluenced=heatmapInfluenced,
        heatmapMinPositive=heatmapInfluenced>0 and heatmapMinPositive or 0,
        heatmapMax=heatmapMax}
    state.analysisDirty = false
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    rebuildAnalysisMarkers(core,shell,extent)
    setStatus(string.format(tLang.L('swl_analysis_complete_fmt'), #selected), false)
end

local function rebuildPreview()
    destroyObject(state.preview)
    destroyObject(state.comparisonPreview)
    state.preview = nil
    state.comparisonPreview = nil
    local playback=state.skeletalPreview
    playback.clips={}
    playback.selected=1
    playback.duration=0
    playback.playing=false
    playback.paused=false
    playback.comparisonReady=false
    if not state.fileName then return end
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    local separation=extent*0.65
    local function loadRuntimePreview(method,x)
        local preview=mesh:new('3d')
        if not preview:setSkeletalSkinningMethod(method) then preview:destroy(); return nil end
        if not preview:load(state.fileName) then preview:destroy(); return nil end
        preview:setPos(x,0,0)
        preview.visible=state.meshVisible
        return preview
    end
    if playback.poseStress then
        state.preview=loadRuntimePreview('lbs',-separation)
        state.comparisonPreview=loadRuntimePreview('dqs',separation)
        playback.comparisonReady=state.preview~=nil and state.comparisonPreview~=nil
    else
        local method=playback.method==1 and 'auto' or playback.method==3 and 'dqs' or 'lbs'
        state.preview=loadRuntimePreview(method,0)
    end
    if state.preview then
        local total=state.preview:getTotalSkeletalAnimations()
        for index=1,total do
            playback.clips[index]=state.preview:getSkeletalAnimationName(index) or ('Clip '..index)
        end
    end
    applyWorkspaceVisibility()
end

local function playSelectedSkeletalClip()
    local playback=state.skeletalPreview
    local name=playback.clips[playback.selected]
    if not state.preview or not name then return end
    if state.preview:playSkeletalAnimation(name) then
        playback.duration=state.preview:getSkeletalAnimationDuration(playback.selected) or 0
        playback.playing=true
        playback.paused=false
        if state.comparisonPreview then
            playback.comparisonReady=state.comparisonPreview:playSkeletalAnimation(name)
            if playback.comparisonReady then state.comparisonPreview:seekSkeletalAnimation(0) end
        end
    end
end

local function syncPoseStressPreview()
    local playback=state.skeletalPreview
    if state.workspace~='runtime' or not playback.poseStress or not playback.playing or
            not playback.comparisonReady or
            not state.preview or not state.comparisonPreview then return end
    local time=state.preview:getSkeletalAnimationTime()
    if time then state.comparisonPreview:seekSkeletalAnimation(time) end
end

local function framePoseStressLayout()
    if not state.meshBounds then return end
    if not state.skeletalPreview.poseStress then frameCamera(state.meshBounds); return end
    local bounds={}
    for key,value in pairs(state.meshBounds) do bounds[key]=value end
    local extent=math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,bounds.maxZ-bounds.minZ)
    local separation=extent*0.65
    bounds.minX=bounds.minX-separation
    bounds.maxX=bounds.maxX+separation
    frameCamera(bounds)
end

local function showSkeletalPreviewControls()
    local playback=state.skeletalPreview
    local poseStress=tImGui.Checkbox(tLang.L('swl_pose_stress_compare'),playback.poseStress)
    if poseStress~=playback.poseStress then
        playback.poseStress=poseStress
        rebuildPreview()
        framePoseStressLayout()
    end
    if playback.poseStress then
        tImGui.TextWrapped(tLang.L('swl_pose_stress_layout'))
    end
    local methods={tLang.L('swl_skinning_auto'),tLang.L('swl_skinning_lbs'),tLang.L('swl_skinning_dqs')}
    tImGui.BeginDisabled(playback.poseStress)
    tImGui.PushItemWidth(190)
    local methodChanged,method=tImGui.Combo(tLang.L('swl_skinning_method'),playback.method,methods,-1)
    tImGui.PopItemWidth()
    tImGui.EndDisabled()
    if methodChanged then
        playback.method=method
        rebuildPreview()
    end
    if not state.preview then
        tImGui.TextDisabled(tLang.L('swl_runtime_preview_unavailable'))
        return
    end
    local lbsReport=state.preview:getSkeletalSkinningReport()
    tImGui.Text(string.format(tLang.L('swl_lbs_report_fmt'),
        (lbsReport.requestedMethod or 'unknown'):upper(),
        (lbsReport.resolvedMethod or 'unknown'):upper(),lbsReport.status or 'unknown'))
    tImGui.TextDisabled(string.format(tLang.L('swl_skinning_reason_fmt'),
        lbsReport.resolutionReason or 'unknown'))
    tImGui.TextWrapped(string.format(tLang.L('swl_lbs_capacity_fmt'),
        lbsReport.requiredBoneCount or 0,lbsReport.effectiveBoneCapacity or 0))
    tImGui.TextDisabled(tLang.L('swl_lbs_capacity_note'))
    if playback.poseStress and state.comparisonPreview then
        local dqsReport=state.comparisonPreview:getSkeletalSkinningReport()
        tImGui.Text(string.format(tLang.L('swl_pose_stress_reports'),lbsReport.status or 'unknown',
            dqsReport.status or 'unknown'))
        if playback.playing and not playback.comparisonReady then
            tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_pose_stress_clip_rejected'))
        end
    elseif playback.poseStress then
        tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_pose_stress_unavailable'))
    end
    if #playback.clips==0 then
        tImGui.TextDisabled(tLang.L('swl_no_skeletal_clips'))
        return
    end
    tImGui.PushItemWidth(190)
    local changed,selected=tImGui.Combo(tLang.L('swl_skeletal_clip'),playback.selected,
        playback.clips,-1)
    tImGui.PopItemWidth()
    if changed then
        playback.selected=selected
        playSelectedSkeletalClip()
    end
    if tImGui.Button(tLang.L('swl_play_restart')) then playSelectedSkeletalClip() end
    tImGui.SameLine()
    tImGui.BeginDisabled(not playback.playing)
    if tImGui.Button(tLang.L('swl_restore_bind_pose')) then
        if state.preview:stopSkeletalAnimation() then
            if state.comparisonPreview then state.comparisonPreview:stopSkeletalAnimation() end
            playback.playing=false
            playback.paused=false
        end
    end
    tImGui.EndDisabled()
    tImGui.SameLine()
    tImGui.BeginDisabled(not playback.playing)
    if tImGui.Button(playback.paused and tLang.L('swl_resume') or tLang.L('swl_pause')) then
        if playback.paused then
            if state.preview:resumeSkeletalAnimation() then playback.paused=false end
            if state.comparisonPreview then state.comparisonPreview:resumeSkeletalAnimation() end
        elseif state.preview:pauseSkeletalAnimation() then
            playback.paused=true
            if state.comparisonPreview then state.comparisonPreview:pauseSkeletalAnimation() end
        end
    end
    tImGui.EndDisabled()
    local time=state.preview:getSkeletalAnimationTime() or 0
    tImGui.PushItemWidth(240)
    local seekChanged,seekTime=tImGui.SliderFloat(tLang.L('swl_preview_time'),time,
        0,math.max(playback.duration,0.0001),'%.3f s')
    tImGui.PopItemWidth()
    if seekChanged and playback.playing then
        state.preview:seekSkeletalAnimation(seekTime)
        if state.comparisonPreview then state.comparisonPreview:seekSkeletalAnimation(seekTime) end
    end
    tImGui.TextDisabled(tLang.L('swl_bind_gizmo_note'))
end

local function loadMesh(path)
    if not path or path == '' then return false end
    local dir = fileDir(path)
    if dir then mbm.addPath(dir) end
    local meshD = meshDebug:new()
    if not meshD:load(path) then
        setStatus(string.format(tLang.L('swl_load_failed_fmt'), path), true)
        return false
    end
    clearRollback()
    clearSelectionVisuals()
    destroySkeletonVisuals()
    state.fileName, state.meshD = path, meshD
    state.info = meshDebug:getInfo(path)
    refreshBindReport()
    state.modified = false
    state.normalizeReport=nil
    state.bindRenameBoneId=nil
    state.bindReparentBoneId=nil
    state.bindEditBoneId=nil
    state.bindAddBoneId=nil
    state.bindChainBoneId=nil
    state.bindMirrorBoneId=nil
    state.bindWeightsBoneId=nil
    state.bindRemoveBoneId=nil
    state.animationClipSelected=1
    local bounds = computeAABB(meshD)
    local initialExtent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    state.bindInitialName='root'
    state.bindInitialTranslation={x=bounds and (bounds.minX+bounds.maxX)*0.5 or 0,
        y=bounds and bounds.minY or 0,z=bounds and (bounds.minZ+bounds.maxZ)*0.5 or 0}
    state.bindInitialRadius=math.max(initialExtent*0.02,0.001)
    state.bindInitialLength=math.max(initialExtent*0.1,0.001)
    state.proximityBoneHighlight=false
    state.analysis = nil
    state.analysisDirty = true
    state.subsetIndex, state.boneIndex, state.analysisBoneIndex, state.targetBoneIndex = 1, 1, 1, 1
    state.allowedBones={}
    state.allowedBonesHighlight=false
    state.hoveredAllowedBone=nil
    state.topologyAdjacency=nil
    for _,bone in ipairs(getBones()) do state.allowedBones[bone.name]=true end
    state.meshBounds = bounds
    state.aabb = bounds
    local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
        bounds.maxZ-bounds.minZ) or 1
    state.aabbDragSensitivity=math.max(extent*0.0025,0.0001)
    state.proximityRadius=math.max(extent*0.1,0.001)
    state.proximityNearestOnly=false
    rebuildPreview()
    rebuildSkeletonVisuals()
    rebuildSelectionBox()
    applyWorkspaceVisibility()
    frameCamera(bounds)
    setStatus(string.format(tLang.L('swl_loaded_fmt'), shortName(path)), false)
    return true
end

local function stageRollbackSnapshot()
    local path = os.tmpname() .. '.msh'
    if not state.meshD:save(path, false, false) then return false end
    return {path=path,modified=state.modified}
end

local function commitRollbackSnapshot(snapshot)
    clearRollback()
    state.rollbackPath = snapshot.path
    state.rollbackModified = snapshot.modified
end

local function discardRollbackSnapshot(snapshot)
    if snapshot and snapshot.path then pcall(os.remove,snapshot.path) end
end

local function snapshotForRollback()
    local snapshot=stageRollbackSnapshot()
    if not snapshot then return false end
    commitRollbackSnapshot(snapshot)
    return true
end

local function blendedInfluences(globalIndex,targetName,alpha)
    if alpha >= 0.999999 then return {{name=targetName,weight=1}} end
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    local byName={}
    if ok then
        for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
            local name,weight=pair[1],tonumber(pair[2]) or 0
            if name and weight>0 and weight==weight and weight<math.huge and
                    (not state.restrictBones or state.allowedBones[name] or name==targetName) then
                byName[name]=(byName[name] or 0)+weight*(1-alpha)
            end
        end
    end
    byName[targetName]=(byName[targetName] or 0)+alpha
    local result={}
    for name,weight in pairs(byName) do
        if weight>0 then result[#result+1]={name=name,weight=weight} end
    end
    table.sort(result,function(a,b)
        if a.weight==b.weight then return a.name<b.name end
        return a.weight>b.weight
    end)
    while #result>4 do table.remove(result) end
    local sum=0
    for _,influence in ipairs(result) do sum=sum+influence.weight end
    if sum<=0 then return {{name=targetName,weight=1}} end
    for _,influence in ipairs(result) do influence.weight=influence.weight/sum end
    return result
end

local function writeInfluences(globalIndex,influences)
    local a,b,c,d=influences[1],influences[2],influences[3],influences[4]
    return state.meshD:setSkeletalVertexWeight(globalIndex,
        a and a.name or nil,a and a.weight or 0,
        b and b.name or nil,b and b.weight or 0,
        c and c.name or nil,c and c.weight or 0,
        d and d.name or nil,d and d.weight or 0)
end

local function readInfluenceMap(globalIndex,respectRestriction)
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    local result={}
    if not ok then return result end
    for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
        local name,weight=pair[1],tonumber(pair[2]) or 0
        if name and weight>0 and weight==weight and weight<math.huge and
                (respectRestriction==false or not state.restrictBones or state.allowedBones[name]) then
            result[name]=(result[name] or 0)+weight
        end
    end
    return result
end

local function normalizedInfluences(weightMap)
    local result={}
    for name,weight in pairs(weightMap) do
        if weight>0 and weight==weight and weight<math.huge then
            result[#result+1]={name=name,weight=weight}
        end
    end
    table.sort(result,function(a,b)
        if a.weight==b.weight then return a.name<b.name end
        return a.weight>b.weight
    end)
    while #result>4 do table.remove(result) end
    local sum=0
    for _,influence in ipairs(result) do sum=sum+influence.weight end
    if sum<=0 then return {} end
    for _,influence in ipairs(result) do influence.weight=influence.weight/sum end
    return result
end

local function buildTopologyAdjacency()
    if state.topologyAdjacency then return state.topologyAdjacency end
    local adjacency={}
    local okS,subsets=safeCall(function() return state.meshD:getTotalSubset(1) end)
    if not okS then return adjacency end
    local offset=0
    local function connect(a,b)
        if a==b then return end
        adjacency[a]=adjacency[a] or {}; adjacency[b]=adjacency[b] or {}
        adjacency[a][b]=true; adjacency[b][a]=true
    end
    for subset=1,subsets do
        local okV,total=safeCall(function() return state.meshD:getTotalVertex(1,subset) end)
        total=okV and total or 0
        local okI,indices=safeCall(function() return state.meshD:getIndex(1,subset) end)
        if okI and indices then
            for i=1,#indices-2,3 do
                local a,b,c=offset+indices[i],offset+indices[i+1],offset+indices[i+2]
                connect(a,b); connect(b,c); connect(c,a)
            end
        end
        offset=offset+total
    end
    state.topologyAdjacency=adjacency
    return adjacency
end

local function influenceDistance(a,b)
    local names={}
    for name in pairs(a) do names[name]=true end
    for name in pairs(b) do names[name]=true end
    local total=0
    for name in pairs(names) do total=total+math.abs((a[name] or 0)-(b[name] or 0)) end
    return math.min(1,total*0.5)
end

local function diagnoseAbruptTransitions()
    if not state.analysis or state.analysisDirty then return end
    local okMode,mode=safeCall(function() return state.meshD:getModeDraw() end)
    if not okMode or mode~='TRIANGLES' then
        setStatus(tLang.L('swl_diagnostic_requires_triangles'),true); return
    end
    local adjacency=buildTopologyAdjacency()
    local selected,points={},{}
    for _,vertex in ipairs(state.analysis.allVertices or {}) do points[vertex.globalIndex]=vertex end
    for _,vertex in ipairs(state.analysis.vertices) do
        selected[vertex.globalIndex]=true; points[vertex.globalIndex]=vertex
    end
    local weights={}
    local function weightsFor(index)
        if not weights[index] then weights[index]=readInfluenceMap(index,false) end
        return weights[index]
    end
    local affected,abruptEdges,maxDistance={},0,0
    local boundaryAffected,boundaryInside,boundaryExternal,boundaryEdges,boundaryMaxDistance={},{},{},{},0
    for index in pairs(selected) do
        for neighbor in pairs(adjacency[index] or {}) do
            if index<neighbor and selected[neighbor] then
                local distance=influenceDistance(weightsFor(index),weightsFor(neighbor))
                maxDistance=math.max(maxDistance,distance)
                if distance>=state.abruptThreshold then
                    abruptEdges=abruptEdges+1
                    affected[index]=true; affected[neighbor]=true
                end
            elseif not selected[neighbor] then
                local distance=influenceDistance(weightsFor(index),weightsFor(neighbor))
                boundaryMaxDistance=math.max(boundaryMaxDistance,distance)
                if distance>=state.abruptThreshold and points[index] and points[neighbor] then
                    boundaryEdges[#boundaryEdges+1]={points[index],points[neighbor]}
                    boundaryAffected[index]=true; boundaryAffected[neighbor]=true
                    boundaryInside[index]=true
                    boundaryExternal[neighbor]=true
                end
            end
        end
    end
    local vertices={}
    for index in pairs(affected) do vertices[#vertices+1]=points[index] end
    local boundaryVertices,boundaryInsideVertices,boundaryExternalVertices={},{},{}
    for index in pairs(boundaryAffected) do boundaryVertices[#boundaryVertices+1]=points[index] end
    for index in pairs(boundaryInside) do boundaryInsideVertices[#boundaryInsideVertices+1]=points[index] end
    for index in pairs(boundaryExternal) do boundaryExternalVertices[#boundaryExternalVertices+1]=points[index] end
    state.abruptDiagnostics={edges=abruptEdges,vertices=#vertices,maxDistance=maxDistance,
        affectedVertices=vertices,boundaryEdges=#boundaryEdges,boundaryVertices=#boundaryVertices,
        boundaryInsideVertices=boundaryInsideVertices,boundaryExternalVertices=boundaryExternalVertices,
        boundaryMaxDistance=boundaryMaxDistance}
    destroyObject(state.abruptLines)
    destroyObject(state.boundaryLines)
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    state.abruptLines=buildVertexMarkers(vertices,1,0,1,extent)
    if state.abruptLines then state.abruptLines.visible=state.abruptMarkersVisible end
    state.boundaryLines=buildEdgeLines(boundaryEdges,1,0.5,0)
    if state.boundaryLines then state.boundaryLines.visible=state.boundaryMarkersVisible end
    setStatus(string.format(tLang.L('swl_abrupt_complete_fmt'),abruptEdges,#vertices,
        #boundaryEdges,#boundaryVertices),false)
    return state.abruptDiagnostics
end

local function smoothVertices(vertices)
    local okMode,mode=safeCall(function() return state.meshD:getModeDraw() end)
    if not okMode or mode~='TRIANGLES' then
        setStatus(tLang.L('swl_smoothing_requires_triangles'),true)
        return nil
    end
    if #vertices==0 then return nil end
    local adjacency=buildTopologyAdjacency()
    if not snapshotForRollback() then
        setStatus(tLang.L('swl_snapshot_failed'),true)
        return nil
    end
    local editable={}
    for _,vertex in ipairs(vertices) do editable[vertex.globalIndex]=true end
    local needed={}
    for index in pairs(editable) do
        needed[index]=true
        for neighbor in pairs(adjacency[index] or {}) do needed[neighbor]=true end
    end
    local current={}
    for index in pairs(needed) do current[index]=readInfluenceMap(index,true) end
    local strength=math.max(0,math.min(1,state.smoothStrength))
    for _=1,state.smoothIterations do
        local nextWeights={}
        for index in pairs(editable) do
            local neighbors=adjacency[index] or {}
            local average,count={},0
            for neighbor in pairs(neighbors) do
                count=count+1
                for name,weight in pairs(current[neighbor] or {}) do
                    average[name]=(average[name] or 0)+weight
                end
            end
            local mixed={}
            for name,weight in pairs(current[index] or {}) do mixed[name]=weight*(1-strength) end
            if count>0 then
                for name,weight in pairs(average) do
                    mixed[name]=(mixed[name] or 0)+weight/count*strength
                end
            else
                mixed=current[index] or {}
            end
            nextWeights[index]=mixed
        end
        for index,weights in pairs(nextWeights) do current[index]=weights end
    end
    local applied,skipped=0,0
    for index in pairs(editable) do
        local influences=normalizedInfluences(current[index] or {})
        if #influences>0 then
            local ok,result=safeCall(function() return writeInfluences(index,influences) end)
            if ok and result then applied=applied+1 else skipped=skipped+1 end
        else
            -- Smoothing has no destination bone. If an allowed-bone restriction removes every
            -- effective influence, preserve the vertex instead of silently assigning a rigid target.
            skipped=skipped+1
        end
    end
    state.modified=state.modified or applied>0
    return applied,skipped
end

local function applyLocalSmoothing()
    if not state.analysis or state.analysisDirty then return end
    local vertices=#state.analysis.shell>0 and state.analysis.shell or state.analysis.vertices
    local applied,skipped=smoothVertices(vertices)
    if applied==nil then return end
    invalidateAnalysis()
    setStatus(string.format(tLang.L('swl_smoothed_fmt'),applied,state.smoothIterations,skipped),applied==0)
end

local function readRawWeightRecord(globalIndex)
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getSkeletalVertexWeight(globalIndex)
    end)
    if not ok then return nil end
    return {n1=n1,w1=w1,n2=n2,w2=w2,n3=n3,w3=w3,n4=n4,w4=w4}
end

local function sameRawWeightRecord(a,b)
    if not a or not b then return false end
    local function sameValue(x,y)
        return x==y or (type(x)=='number' and type(y)=='number' and x~=x and y~=y)
    end
    for _,field in ipairs({'n1','w1','n2','w2','n3','w3','n4','w4'}) do
        if not sameValue(a[field],b[field]) then return false end
    end
    return true
end

local function applyAbruptTransitionSmoothing()
    local before=state.abruptDiagnostics
    if not before or not before.affectedVertices or #before.affectedVertices==0 then return end
    local externalBefore,auditFailures={},0
    for _,vertex in ipairs(before.boundaryExternalVertices or {}) do
        local record=readRawWeightRecord(vertex.globalIndex)
        if record then externalBefore[vertex.globalIndex]=record else auditFailures=auditFailures+1 end
    end
    local applied,skipped=smoothVertices(before.affectedVertices)
    if applied==nil then return end

    local externalVerified,externalModified=0,0
    for index,record in pairs(externalBefore) do
        local afterRecord=readRawWeightRecord(index)
        if afterRecord then
            externalVerified=externalVerified+1
            if not sameRawWeightRecord(record,afterRecord) then externalModified=externalModified+1 end
        else
            auditFailures=auditFailures+1
        end
    end

    -- Rebuild both layers from the edited weights so the magenta markers and summary immediately
    -- describe the result, not the pre-operation diagnosis.
    analyzeSelection()
    local after=diagnoseAbruptTransitions()
    if not after then return end
    setStatus(string.format(tLang.L('swl_abrupt_smoothed_fmt'),applied,state.smoothIterations,
        before.edges,after.edges,before.vertices,after.vertices,skipped,
        externalVerified,externalModified,auditFailures),
        applied==0 or externalModified>0 or auditFailures>0)
end

local function applyNormalizeAndLimit()
    if not state.analysis or state.analysisDirty or #state.analysis.vertices==0 then return end
    local jobs,unchanged,skipped,readFailed={},0,0,0
    for _,vertex in ipairs(state.analysis.vertices) do
        local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
            return state.meshD:getSkeletalVertexWeight(vertex.globalIndex)
        end)
        if not ok then
            readFailed=readFailed+1
        else
            local weightMap,seen,sum,effective,needsCleanup={}, {},0,0,false
            for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
                local name,weight=pair[1],tonumber(pair[2]) or 0
                if name then
                    if weight<=0 or weight~=weight or weight==math.huge or weight==-math.huge then
                        needsCleanup=true
                    else
                        if seen[name] then needsCleanup=true end
                        seen[name]=true
                        effective=effective+1
                        sum=sum+weight
                        weightMap[name]=(weightMap[name] or 0)+weight
                    end
                end
            end
            local influences=normalizedInfluences(weightMap)
            if #influences==0 then
                skipped=skipped+1
            elseif needsCleanup or effective>4 or math.abs(sum-1)>0.001 then
                jobs[#jobs+1]={index=vertex.globalIndex,influences=influences}
            else
                unchanged=unchanged+1
            end
        end
    end
    if #jobs>0 and not snapshotForRollback() then
        setStatus(tLang.L('swl_snapshot_failed'),true)
        return
    end
    local applied,writeFailed=0,0
    for _,job in ipairs(jobs) do
        local ok,result=safeCall(function() return writeInfluences(job.index,job.influences) end)
        if ok and result then applied=applied+1 else writeFailed=writeFailed+1 end
    end
    local failed=readFailed+writeFailed
    state.modified=state.modified or applied>0
    state.normalizeReport={total=#state.analysis.vertices,corrected=applied,unchanged=unchanged,
        skipped=skipped,failed=failed}
    if applied>0 then invalidateAnalysis() end
    setStatus(string.format(tLang.L('swl_normalized_fmt'),applied,unchanged,skipped,failed),failed>0)
end

local function applyRigidBind()
    if not state.analysis or state.analysisDirty or #state.analysis.vertices == 0 then return end
    local bones = getBones()
    local target = bones[state.targetBoneIndex]
    if not target then return end
    if not snapshotForRollback() then
        setStatus(tLang.L('swl_snapshot_failed'), true)
        return
    end
    local applied = 0
    local hasTransition = #state.analysis.shell > 0
    for _, vertex in ipairs(state.analysis.vertices) do
        local influences=blendedInfluences(vertex.globalIndex,target.name,vertex.blendAlpha or 1)
        local ok, result = safeCall(function()
            return writeInfluences(vertex.globalIndex,influences)
        end)
        if ok and result then applied = applied + 1 end
    end
    state.modified = applied > 0
    invalidateAnalysis()
    local statusKey = hasTransition and 'swl_applied_transition_fmt' or 'swl_applied_fmt'
    setStatus(string.format(tLang.L(statusKey), applied, target.name), applied == 0)
end

local function revertLast()
    if not state.rollbackPath then return end
    local restored = meshDebug:new()
    if not restored:load(state.rollbackPath) then
        setStatus(tLang.L('swl_revert_failed'), true)
        return
    end
    state.meshD = restored
    local restoredModified = state.rollbackModified == true
    clearRollback()
    state.modified = restoredModified
    state.normalizeReport=nil
    state.bindRenameBoneId=nil
    state.bindReparentBoneId=nil
    state.bindEditBoneId=nil
    refreshBindReport()
    local bones=getBones()
    state.boneIndex=math.max(1,math.min(state.boneIndex,#bones))
    state.allowedBones={}
    for _,bone in ipairs(bones) do state.allowedBones[bone.name]=true end
    invalidateAnalysis()
    rebuildPreview()
    rebuildSkeletonVisuals()
    rebuildSelectionBox()
    rebuildProximityCapsule()
    applyWorkspaceVisibility()
    setStatus(tLang.L('swl_reverted'), false)
end

local function showRollbackControls(id)
    tImGui.Separator()
    tImGui.Text(tLang.L('swl_history'))
    tImGui.BeginDisabled(state.rollbackPath == nil)
    if tImGui.Button(tLang.L('swl_revert')..'##'..id) then revertLast() end
    tImGui.EndDisabled()
end

local function saveTo(path)
    if not state.meshD or not path or path == '' then return false end
    local okCheck, valid, message = safeCall(function() return state.meshD:check() end)
    if not okCheck or not valid then
        setStatus(message or tLang.L('swl_check_failed'), true)
        return false
    end
    if state.meshD:save(path, false, false) then
        state.fileName = path
        state.modified = false
        rebuildPreview()
        setStatus(string.format(tLang.L('swl_saved_fmt'), shortName(path)), false)
        return true
    end
    setStatus(string.format(tLang.L('swl_save_failed_fmt'), path), true)
    return false
end

local function showMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(tLang.L('swl_open_mesh'), 'Ctrl+O') then
            local path = mbm.openFile(state.fileName or '', 'msh')
            if path then loadMesh(path) end
        end
        if tImGui.MenuItem(tLang.L('swl_save'), 'Ctrl+S', false, state.meshD ~= nil) then
            saveTo(state.fileName)
        end
        if tImGui.MenuItem(tLang.L('swl_save_as'), nil, false, state.meshD ~= nil) then
            local path = mbm.saveFile(state.fileName or 'skin-weight-lab.msh', 'msh')
            if path then saveTo(path) end
        end
        tImGui.Separator()
        if tImGui.MenuItem(tLang.L('menu_quit'), 'Ctrl+Q') then mbm.quit() end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        tLang.renderLanguageSubmenu()
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

local function showItemTooltip(text)
    if tImGui.IsItemHovered(0) then
        tImGui.BeginTooltip()
        -- Tooltip windows have no reliable wrap width in this ImGui binding.
        -- Localized tooltip strings use explicit line breaks instead.
        tImGui.Text(text)
        tImGui.EndTooltip()
    end
end

local function showSelectionInputs()
    local labels = {tLang.L('swl_selection_aabb'), tLang.L('swl_selection_subset'), tLang.L('swl_selection_bone')}
    tImGui.PushItemWidth(190)
    local changed, mode = tImGui.Combo(tLang.L('swl_selection_method'), state.selectionMode, labels, -1)
    showItemTooltip(tLang.L('swl_selection_method_tooltip'))
    tImGui.PopItemWidth()
    if changed then
        state.selectionMode = mode
        if mode ~= 3 and state.proximityBoneHighlight then
            state.proximityBoneHighlight = false
            rebuildProximityBoneHighlight()
        end
        invalidateAnalysis()
        rebuildSelectionBox()
        rebuildProximityCapsule()
    end
    if state.selectionMode == 1 and state.aabb then
        local b = state.aabb
        local aabbChanged = false
        local fields = {
            {'Min X', 'minX'}, {'Min Y', 'minY'}, {'Min Z', 'minZ'},
            {'Max X', 'maxX'}, {'Max Y', 'maxY'}, {'Max Z', 'maxZ'},
        }
        local reference = state.meshBounds
        local extent = reference and math.max(reference.maxX-reference.minX,
            reference.maxY-reference.minY, reference.maxZ-reference.minZ) or 1
        local automaticDragSpeed=math.max(extent*0.0025,0.0001)
        tImGui.PushItemWidth(150)
        local sensitivityChanged,sensitivity=tImGui.InputFloat(tLang.L('swl_aabb_drag_sensitivity'),
            state.aabbDragSensitivity,automaticDragSpeed*0.1,automaticDragSpeed,'%.6f',0)
        showItemTooltip(tLang.L('swl_aabb_drag_sensitivity_tooltip'))
        tImGui.PopItemWidth()
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_reset_auto')..'##swlAabbDragSensitivity') then
            state.aabbDragSensitivity=automaticDragSpeed
        elseif sensitivityChanged then
            state.aabbDragSensitivity=math.max(sensitivity,0.000001)
        end
        local dragSpeed=math.max(state.aabbDragSensitivity,0.000001)
        tImGui.PushItemWidth(150)
        for _, field in ipairs(fields) do
            local edited, value = tImGui.DragFloat(field[1], b[field[2]], dragSpeed, -1000000, 1000000, '%.4f')
            if edited then b[field[2]] = value; aabbChanged = true end
        end
        local sizeFields={
            {tLang.L('swl_size_x'),'minX','maxX'},
            {tLang.L('swl_size_y'),'minY','maxY'},
            {tLang.L('swl_size_z'),'minZ','maxZ'},
        }
        for _,field in ipairs(sizeFields) do
            local currentSize=math.max(0,b[field[3]]-b[field[2]])
            local edited,value=tImGui.DragFloat(field[1],currentSize,dragSpeed,0,2000000,'%.4f')
            showItemTooltip(tLang.L('swl_aabb_size_tooltip'))
            if edited then
                local center=(b[field[2]]+b[field[3]])*0.5
                local halfSize=math.max(0,value)*0.5
                b[field[2]],b[field[3]]=center-halfSize,center+halfSize
                aabbChanged=true
            end
        end
        tImGui.PopItemWidth()
        tImGui.Text(tLang.L('swl_transition_faces'))
        showItemTooltip(tLang.L('swl_transition_faces_tooltip'))
        local faceControls={
            {'-X','minX'},{'+X','maxX'},{'-Y','minY'},
            {'+Y','maxY'},{'-Z','minZ'},{'+Z','maxZ'},
        }
        for _,control in ipairs(faceControls) do
            local face=state.shellFaces[control[2]]
            local enabled=tImGui.Checkbox(control[1]..'##swlShellEnabled'..control[2],face.enabled)
            if enabled~=face.enabled then face.enabled=enabled; aabbChanged=true end
            tImGui.SameLine()
            tImGui.BeginDisabled(not face.enabled)
            tImGui.PushItemWidth(130)
            local changed,width=tImGui.DragFloat(tLang.L('swl_shell_width')..'##'..control[2],
                face.width,dragSpeed,0,math.max(extent*2,dragSpeed),'%.4f')
            tImGui.PopItemWidth()
            tImGui.EndDisabled()
            if changed then face.width=math.max(0,width); aabbChanged=true end
        end
        local falloffLabels={tLang.L('swl_falloff_linear'),tLang.L('swl_falloff_smooth')}
        tImGui.PushItemWidth(150)
        local falloffChanged,falloff=tImGui.Combo(tLang.L('swl_falloff'),state.falloffMode,falloffLabels,-1)
        tImGui.PopItemWidth()
        if falloffChanged then state.falloffMode=falloff; aabbChanged=true end
        if aabbChanged then
            if b.minX > b.maxX then b.minX,b.maxX=b.maxX,b.minX end
            if b.minY > b.maxY then b.minY,b.maxY=b.maxY,b.minY end
            if b.minZ > b.maxZ then b.minZ,b.maxZ=b.maxZ,b.minZ end
            invalidateAnalysis()
            rebuildSelectionBox()
        end
    elseif state.selectionMode == 2 then
        local ok, total = safeCall(function() return state.meshD:getTotalSubset(1) end)
        total = ok and total or 1
        tImGui.PushItemWidth(190)
        local edited, value = tImGui.SliderInt(tLang.L('swl_subset'), state.subsetIndex, 1, math.max(1,total))
        tImGui.PopItemWidth()
        if edited then state.subsetIndex=value; invalidateAnalysis() end
    else
        local bones, names = getBones(), {}
        for _, bone in ipairs(bones) do names[#names+1] = bone.name end
        if #names > 0 then
            tImGui.PushItemWidth(190)
            local edited, value = tImGui.Combo(tLang.L('swl_source_bone'), math.min(state.boneIndex,#names), names, -1)
            showItemTooltip(tLang.L('swl_source_bone_tooltip'))
            tImGui.PopItemWidth()
            tImGui.SameLine()
            local highlight=tImGui.Checkbox(tLang.L('swl_highlight')..'##swlProximityBoneHighlight',
                state.proximityBoneHighlight)
            showItemTooltip(tLang.L('swl_source_bone_highlight_tooltip'))
            if highlight~=state.proximityBoneHighlight then
                state.proximityBoneHighlight=highlight
                rebuildProximityBoneHighlight()
            end
            if edited then
                state.boneIndex=value
                invalidateAnalysis()
                rebuildProximityBoneHighlight()
                rebuildProximityCapsule()
            end
            local bounds=state.meshBounds
            local extent=bounds and math.max(bounds.maxX-bounds.minX,bounds.maxY-bounds.minY,
                bounds.maxZ-bounds.minZ) or 1
            local dragSpeed=math.max(extent*0.0025,0.0001)
            tImGui.PushItemWidth(190)
            local radiusChanged,radius=tImGui.DragFloat(tLang.L('swl_proximity_radius'),
                state.proximityRadius,dragSpeed,0,math.max(extent*2,dragSpeed),'%.4f')
            showItemTooltip(tLang.L('swl_proximity_radius_tooltip'))
            tImGui.PopItemWidth()
            if radiusChanged then
                state.proximityRadius=math.max(0,radius)
                invalidateAnalysis()
                rebuildProximityCapsule()
            end
            local nearestOnly=tImGui.Checkbox(tLang.L('swl_proximity_nearest_only'),
                state.proximityNearestOnly)
            showItemTooltip(tLang.L('swl_proximity_nearest_only_tooltip'))
            if nearestOnly~=state.proximityNearestOnly then
                state.proximityNearestOnly=nearestOnly
                invalidateAnalysis()
            end
        else
            tImGui.TextDisabled(tLang.L('swl_no_bones'))
        end
    end
    local heatmap=tImGui.Checkbox(tLang.L('swl_heatmap'),state.heatmapEnabled)
    showItemTooltip(tLang.L('swl_heatmap_tooltip'))
    if heatmap~=state.heatmapEnabled then
        state.heatmapEnabled=heatmap
        if not heatmap and state.analysisBoneHighlight then
            state.analysisBoneHighlight=false
            rebuildAnalysisBoneHighlight()
        end
        if state.analysis then
            local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
                state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
            rebuildAnalysisMarkers(state.analysis.core,state.analysis.shell,extent)
        end
    end
    if state.heatmapEnabled then
        local bones, names = getBones(), {}
        for _, bone in ipairs(bones) do names[#names+1] = bone.name end
        if #names > 0 then
            state.analysisBoneIndex=math.min(state.analysisBoneIndex,#names)
            tImGui.PushItemWidth(190)
            local edited,value=tImGui.Combo(tLang.L('swl_analysis_bone'),state.analysisBoneIndex,names,-1)
            showItemTooltip(tLang.L('swl_analysis_bone_tooltip'))
            tImGui.PopItemWidth()
            tImGui.SameLine()
            local highlight=tImGui.Checkbox(tLang.L('swl_highlight')..'##swlAnalysisBoneHighlight',
                state.analysisBoneHighlight)
            if highlight~=state.analysisBoneHighlight then
                state.analysisBoneHighlight=highlight
                rebuildAnalysisBoneHighlight()
            end
            if edited then
                state.analysisBoneIndex=value
                invalidateAnalysis()
                rebuildAnalysisBoneHighlight()
            end
        end
        tImGui.TextDisabled(tLang.L('swl_heatmap_legend'))
    end
end

local function showStatusMessage()
    if not state.status then return end
    tImGui.Separator()
    if state.statusError then
        tImGui.TextColored({r=1,g=0.3,b=0.2,a=1},state.status)
    else
        tImGui.TextWrapped(state.status)
    end
end

local sectionTitleColor={r=0.25,g=0.80,b=1.0,a=1}

local function showSectionTitle(key)
    tImGui.TextColored(sectionTitleColor,tLang.L(key))
end

local function refreshAllowedBoneDiagnostics()
    if not state.analysis then updateAllowedBoneColors(); return end
    local disallowed=0
    if state.restrictBones then
        for _,vertex in ipairs(state.analysis.vertices) do
            local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
                return state.meshD:getSkeletalVertexWeight(vertex.globalIndex)
            end)
            if ok then
                for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
                    if pair[1] and (tonumber(pair[2]) or 0)>0 and not state.allowedBones[pair[1]] then
                        disallowed=disallowed+1
                    end
                end
            end
        end
    end
    state.analysis.disallowed=disallowed
    updateAllowedBoneColors()
end

local function showBoneRestrictions(bones,showTargetOnly)
    local restrict=tImGui.Checkbox(tLang.L('swl_restrict_bones'),state.restrictBones)
    showItemTooltip(tLang.L('swl_restrict_bones_tooltip'))
    if restrict~=state.restrictBones then
        state.restrictBones=restrict
        refreshAllowedBoneDiagnostics()
    end
    if not state.restrictBones or #bones==0 then
        state.hoveredAllowedBone=nil
        updateAllowedBoneColors()
        return
    end
    local highlight=tImGui.Checkbox(tLang.L('swl_highlight_allowed_bones'),state.allowedBonesHighlight)
    showItemTooltip(tLang.L('swl_highlight_allowed_bones_tooltip'))
    if highlight~=state.allowedBonesHighlight then
        state.allowedBonesHighlight=highlight
        if not highlight then state.hoveredAllowedBone=nil end
        updateAllowedBoneColors()
    end
    if tImGui.Button(tLang.L('swl_allow_all')) then
        for _,bone in ipairs(bones) do state.allowedBones[bone.name]=true end
        refreshAllowedBoneDiagnostics()
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('swl_clear_all')) then
        state.allowedBones={}
        refreshAllowedBoneDiagnostics()
    end
    if showTargetOnly then
        tImGui.SameLine()
        if tImGui.Button(tLang.L('swl_target_only')) then
            state.allowedBones={}
            state.allowedBones[bones[state.targetBoneIndex].name]=true
            refreshAllowedBoneDiagnostics()
        end
    end
    local hoveredBone=nil
    tImGui.BeginChild('##swlAllowedBones',{x=300,y=115},true)
    for _,bone in ipairs(bones) do
        local wasAllowed=state.allowedBones[bone.name]==true
        local allowed=tImGui.Checkbox(bone.name..'##swlAllowed',wasAllowed)
        if allowed~=wasAllowed then
            state.allowedBones[bone.name]=allowed or nil
            refreshAllowedBoneDiagnostics()
        end
        if tImGui.IsItemHovered(0) then hoveredBone=bone.name end
    end
    tImGui.EndChild()
    if hoveredBone~=state.hoveredAllowedBone then
        state.hoveredAllowedBone=hoveredBone
        updateAllowedBoneColors()
    end
end

local function showSmoothingControls()
    tImGui.PushItemWidth(190)
    local strengthChanged,strength=tImGui.SliderFloat(tLang.L('swl_smooth_strength'),
        state.smoothStrength,0,1,'%.2f')
    showItemTooltip(tLang.L('swl_smooth_strength_tooltip'))
    if strengthChanged then state.smoothStrength=strength end
    local iterationsChanged,iterations=tImGui.SliderInt(tLang.L('swl_smooth_iterations'),
        state.smoothIterations,1,10)
    showItemTooltip(tLang.L('swl_smooth_iterations_tooltip'))
    if iterationsChanged then state.smoothIterations=iterations end
    tImGui.PopItemWidth()
end

local function showDiagnosticControls(canOperate,allowRepair)
    tImGui.PushItemWidth(190)
    local thresholdChanged,threshold=tImGui.SliderFloat(tLang.L('swl_abrupt_threshold'),
        state.abruptThreshold,0.05,1,'%.2f')
    showItemTooltip(tLang.L('swl_abrupt_threshold_tooltip'))
    tImGui.PopItemWidth()
    if thresholdChanged then
        state.abruptThreshold=threshold
        state.abruptDiagnostics=nil
        destroyObject(state.abruptLines); state.abruptLines=nil
        destroyObject(state.boundaryLines); state.boundaryLines=nil
    end
    tImGui.BeginDisabled(not canOperate)
    local diagnosePressed=tImGui.Button(tLang.L('swl_diagnose_transitions'))
    showItemTooltip(tLang.L('swl_diagnose_transitions_tooltip'))
    if diagnosePressed then diagnoseAbruptTransitions() end
    tImGui.EndDisabled()
    if state.abruptDiagnostics then
        local diagnostic=state.abruptDiagnostics
        tImGui.Text(string.format(tLang.L('swl_abrupt_result_fmt'),diagnostic.edges,
            diagnostic.vertices,diagnostic.maxDistance))
        tImGui.Text(string.format(tLang.L('swl_boundary_result_fmt'),diagnostic.boundaryEdges,
            diagnostic.boundaryVertices,diagnostic.boundaryMaxDistance))
        tImGui.TextDisabled(tLang.L('swl_abrupt_marker_hint'))
        if allowRepair then
            tImGui.BeginDisabled(diagnostic.vertices==0)
            local pressed=tImGui.Button(tLang.L('swl_smooth_abrupt_transitions'))
            showItemTooltip(tLang.L('swl_smooth_abrupt_transitions_tooltip'))
            if pressed then applyAbruptTransitionSmoothing() end
            tImGui.EndDisabled()
        end
    end
end

local function formatBindMatrix(matrix)
    if not matrix or #matrix ~= 16 then return tLang.L('swl_bind_matrix_unavailable') end
    return string.format('%.5g %.5g %.5g %.5g\n%.5g %.5g %.5g %.5g\n%.5g %.5g %.5g %.5g\n%.5g %.5g %.5g %.5g',
        table.unpack(matrix))
end

local function showSelectedBindBone(report)
    local bone=report.bones and report.bones[state.boneIndex] or nil
    if not bone then return end
    if state.bindRenameBoneId~=bone.boneId then
        state.bindRenameBoneId=bone.boneId
        state.bindRenameName=bone.name or ''
    end
    if state.bindReparentBoneId~=bone.boneId then
        state.bindReparentBoneId=bone.boneId
        state.bindParentChoice=(bone.parentIndex or 0)+1
        state.bindPreserveGlobal=true
    end
    if state.bindEditBoneId~=bone.boneId then
        local translation,rotation,scale=bone.localTranslation or {},bone.localRotation or {},bone.localScale or {}
        state.bindEditBoneId=bone.boneId
        state.bindEdit={tx=translation.x or 0,ty=translation.y or 0,tz=translation.z or 0,
            qx=rotation.x or 0,qy=rotation.y or 0,qz=rotation.z or 0,qw=rotation.w or 1,
            sx=scale.x or 1,sy=scale.y or 1,sz=scale.z or 1,
            radius=bone.radius or 0,length=bone.length or 0}
    end
    if state.bindAddBoneId~=bone.boneId then
        state.bindAddBoneId=bone.boneId
        state.bindAddName=(bone.name or 'bone')..'_child'
        state.bindAddParentChoice=state.boneIndex+1
        state.bindAddTranslation={x=0,y=(bone.length or 0)>0 and bone.length or 1,z=0}
    end
    if state.bindChainBoneId~=bone.boneId then
        state.bindChainBoneId=bone.boneId
        state.bindChainPrefix=(bone.name or 'bone')..'_chain_'
        state.bindChainCount=3
        state.bindChainStep={x=0,y=(bone.length or 0)>0 and bone.length or 1,z=0}
    end
    if state.bindMirrorBoneId~=bone.boneId then
        state.bindMirrorBoneId=bone.boneId
        state.bindMirrorPrefix='mirror_'
        state.bindMirrorAxis=1
        state.bindMirrorConfirmed=false
    end
    if state.bindWeightsBoneId~=bone.boneId then
        state.bindWeightsBoneId=bone.boneId
        state.bindWeightsConfirmed=false
    end
    if state.bindRemoveBoneId~=bone.boneId then
        state.bindRemoveBoneId=bone.boneId
        state.bindRemoveConfirmed=false
        state.bindRemoveDiscardTracks=false
        state.bindRemoveReparentChildren=false
        state.bindRemoveReplacement=(bone.parentIndex or 0)>0 and bone.parentIndex or
            (state.boneIndex==1 and 2 or 1)
    end
    tImGui.Separator()
    tImGui.Text(string.format('%s: %s',tLang.L('swl_source_bone'),bone.name or '?'))
    tImGui.Text(string.format('ID: %s  Parent: %s (%d)',bone.boneId or '?',
        bone.parentBoneId or '?',bone.parentIndex or 0))
    local translation=bone.localTranslation or {}
    local rotation=bone.localRotation or {}
    local scale=bone.localScale or {}
    tImGui.Text(string.format('T %.6g %.6g %.6g',translation.x or 0,
        translation.y or 0,translation.z or 0))
    tImGui.Text(string.format('Q %.6g %.6g %.6g %.6g',rotation.x or 0,
        rotation.y or 0,rotation.z or 0,rotation.w or 1))
    tImGui.Text(string.format('S %.6g %.6g %.6g',scale.x or 1,scale.y or 1,scale.z or 1))
    tImGui.Text(string.format('Radius %.6g  Length %.6g',bone.radius or 0,bone.length or 0))
    tImGui.PushItemWidth(190)
    local changed,newName=tImGui.InputText(tLang.L('swl_bone_name')..'##swlBindRename',
        state.bindRenameName,tImGui.Flags('ImGuiInputTextFlags_None'))
    tImGui.PopItemWidth()
    if changed then state.bindRenameName=newName end
    local trimmed=(state.bindRenameName or ''):match('^%s*(.-)%s*$')
    tImGui.BeginDisabled(trimmed=='' or trimmed==bone.name)
    if tImGui.Button(tLang.L('swl_apply_rename')..'##swlBindRenameApply') then
        local selectedBoneId=bone.boneId
        local snapshot=stageRollbackSnapshot()
        local ok=false
        if snapshot then
            ok=select(1,safeCall(function()
                return state.meshD:renameSkeletalBone(state.boneIndex,trimmed)
            end))
        else
            setStatus(tLang.L('swl_snapshot_failed'),true)
        end
        if ok then
            commitRollbackSnapshot(snapshot)
            state.modified=true
            refreshBindReport()
            for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                if item.boneId==selectedBoneId then state.boneIndex=index break end
            end
            state.bindRenameBoneId=nil
            state.bindAddBoneId=nil
            state.bindRemoveConfirmed=false
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
            setStatus(tLang.L('swl_bone_renamed'),false)
        else
            discardRollbackSnapshot(snapshot)
        end
    end
    tImGui.EndDisabled()
    local parentNames={tLang.L('swl_root_no_parent')}
    for _,candidate in ipairs(report.bones or {}) do
        parentNames[#parentNames+1]=candidate.name or '?'
    end
    tImGui.PushItemWidth(190)
    local parentChanged,parentChoice=tImGui.Combo(tLang.L('swl_parent_bone'),
        state.bindParentChoice,parentNames,-1)
    tImGui.PopItemWidth()
    if parentChanged then state.bindParentChoice=parentChoice end
    local preserveGlobal=tImGui.Checkbox(tLang.L('swl_preserve_global_bind'),state.bindPreserveGlobal)
    if preserveGlobal~=state.bindPreserveGlobal then state.bindPreserveGlobal=preserveGlobal end
    local requestedParent=state.bindParentChoice-1 -- 0=root; otherwise one-based bone index
    local currentParent=bone.parentIndex or 0
    local selectsSelf=requestedParent==state.boneIndex
    tImGui.BeginDisabled(requestedParent==currentParent or selectsSelf)
    if tImGui.Button(tLang.L('swl_apply_reparent')..'##swlBindReparentApply') then
        local selectedBoneId=bone.boneId
        local snapshot=stageRollbackSnapshot()
        local ok=false
        if snapshot then
            ok=select(1,safeCall(function()
                return state.meshD:reparentSkeletalBone(state.boneIndex,requestedParent,
                    state.bindPreserveGlobal)
            end))
        else
            setStatus(tLang.L('swl_snapshot_failed'),true)
        end
        if ok then
            commitRollbackSnapshot(snapshot)
            state.modified=true
            refreshBindReport()
            for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                if item.boneId==selectedBoneId then state.boneIndex=index break end
            end
            state.bindRenameBoneId=nil
            state.bindReparentBoneId=nil
            state.bindEditBoneId=nil
            state.bindRemoveConfirmed=false
            rebuildSkeletonVisuals()
            applyWorkspaceVisibility()
            setStatus(tLang.L('swl_bone_reparented'),false)
        else
            discardRollbackSnapshot(snapshot)
        end
    end
    tImGui.EndDisabled()
    if selectsSelf then tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},
        tLang.L('swl_parent_self_invalid')) end
    if tImGui.TreeNode(tLang.L('swl_edit_local_bind')..'##swlEditLocalBind') then
        local fields={{'T X','tx'},{'T Y','ty'},{'T Z','tz'},
            {'Q X','qx'},{'Q Y','qy'},{'Q Z','qz'},{'Q W','qw'},
            {'S X','sx'},{'S Y','sy'},{'S Z','sz'},
            {tLang.L('swl_bone_radius'),'radius'},{tLang.L('swl_bone_length'),'length'}}
        tImGui.PushItemWidth(190)
        for _,field in ipairs(fields) do
            local edited,value=tImGui.InputFloat(field[1]..'##swlBindEdit'..field[2],
                state.bindEdit[field[2]],0,0,'%.6g',0)
            if edited then state.bindEdit[field[2]]=value end
        end
        tImGui.PopItemWidth()
        tImGui.TextWrapped(tLang.L('swl_local_bind_moves_subtree'))
        if tImGui.Button(tLang.L('swl_apply_bind_edit')..'##swlBindEditApply') then
            local selectedBoneId=bone.boneId
            local value=state.bindEdit
            local snapshot=stageRollbackSnapshot()
            local ok=false
            if snapshot then
                ok=select(1,safeCall(function()
                    return state.meshD:setSkeletalBoneBind(state.boneIndex,
                        value.tx,value.ty,value.tz,value.qx,value.qy,value.qz,value.qw,
                        value.sx,value.sy,value.sz,value.radius,value.length)
                end))
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot)
                state.modified=true
                refreshBindReport()
                for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                    if item.boneId==selectedBoneId then state.boneIndex=index break end
                end
                state.bindEditBoneId=nil
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_bone_bind_updated'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_add_bone')..'##swlAddBone') then
        tImGui.PushItemWidth(190)
        local nameChanged,addName=tImGui.InputText(tLang.L('swl_bone_name')..'##swlAddBoneName',
            state.bindAddName,tImGui.Flags('ImGuiInputTextFlags_None'))
        if nameChanged then state.bindAddName=addName end
        local addParentChanged,addParent=tImGui.Combo(tLang.L('swl_parent_bone')..'##swlAddBoneParent',
            state.bindAddParentChoice,parentNames,-1)
        if addParentChanged then state.bindAddParentChoice=addParent end
        for _,field in ipairs({{'T X','x'},{'T Y','y'},{'T Z','z'}}) do
            local edited,value=tImGui.InputFloat(field[1]..'##swlAddBone'..field[2],
                state.bindAddTranslation[field[2]],0,0,'%.6g',0)
            if edited then state.bindAddTranslation[field[2]]=value end
        end
        tImGui.PopItemWidth()
        tImGui.TextWrapped(tLang.L('swl_add_bone_defaults'))
        local addTrimmed=(state.bindAddName or ''):match('^%s*(.-)%s*$')
        tImGui.BeginDisabled(addTrimmed=='')
        if tImGui.Button(tLang.L('swl_apply_add_bone')..'##swlAddBoneApply') then
            local snapshot=stageRollbackSnapshot()
            local ok,newIndex=false,nil
            if snapshot then
                ok,newIndex=safeCall(function()
                    local value=state.bindAddTranslation
                    return state.meshD:addSkeletalBone(state.bindAddParentChoice-1,addTrimmed,
                        value.x,value.y,value.z,bone.radius or 0,bone.length or 0)
                end)
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot)
                state.modified=true
                refreshBindReport()
                state.boneIndex=newIndex
                state.bindRenameBoneId=nil
                state.bindReparentBoneId=nil
                state.bindEditBoneId=nil
                state.bindAddBoneId=nil
                state.bindRemoveConfirmed=false
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_bone_added'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        if tImGui.TreeNode(tLang.L('swl_add_chain')..'##swlAddChain') then
            tImGui.PushItemWidth(190)
            local prefixChanged,prefix=tImGui.InputText(tLang.L('swl_chain_prefix')..'##swlChainPrefix',
                state.bindChainPrefix,tImGui.Flags('ImGuiInputTextFlags_None'))
            if prefixChanged then state.bindChainPrefix=prefix end
            local countChanged,count=tImGui.InputInt(tLang.L('swl_chain_count')..'##swlChainCount',
                state.bindChainCount,1,1,tImGui.Flags('ImGuiInputTextFlags_None'))
            if countChanged then state.bindChainCount=math.max(1,math.min(256,count)) end
            for _,field in ipairs({{'T X','x'},{'T Y','y'},{'T Z','z'}}) do
                local edited,value=tImGui.InputFloat(field[1]..'##swlChain'..field[2],
                    state.bindChainStep[field[2]],0,0,'%.6g',0)
                if edited then state.bindChainStep[field[2]]=value end
            end
            tImGui.PopItemWidth()
            tImGui.TextWrapped(tLang.L('swl_add_chain_help'))
            local chainPrefix=(state.bindChainPrefix or ''):match('^%s*(.-)%s*$')
            tImGui.BeginDisabled(chainPrefix=='')
            if tImGui.Button(tLang.L('swl_apply_add_chain')..'##swlAddChainApply') then
                local snapshot=stageRollbackSnapshot()
                local ok,lastIndex=false,nil
                if snapshot then
                    ok,lastIndex=safeCall(function()
                        local step=state.bindChainStep
                        return state.meshD:addSkeletalBoneChain(state.bindAddParentChoice-1,
                            chainPrefix,state.bindChainCount,step.x,step.y,step.z,
                            bone.radius or 0,bone.length or 0)
                    end)
                else
                    setStatus(tLang.L('swl_snapshot_failed'),true)
                end
                if ok then
                    commitRollbackSnapshot(snapshot)
                    state.modified=true
                    refreshBindReport()
                    state.boneIndex=lastIndex
                    state.bindRenameBoneId=nil
                    state.bindReparentBoneId=nil
                    state.bindEditBoneId=nil
                    state.bindAddBoneId=nil
                    state.bindChainBoneId=nil
                    state.bindRemoveConfirmed=false
                    rebuildPreview()
                    rebuildSkeletonVisuals()
                    applyWorkspaceVisibility()
                    setStatus(tLang.L('swl_chain_added'),false)
                else
                    discardRollbackSnapshot(snapshot)
                end
            end
            tImGui.EndDisabled()
            tImGui.TreePop()
        end
        tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_mirror_subtree')..'##swlMirrorSubtree') then
        local subtreeCount=0
        for index=1,#(report.bones or {}) do
            local cursor=index
            while cursor and cursor>0 do
                if cursor==state.boneIndex then subtreeCount=subtreeCount+1 break end
                cursor=report.bones[cursor] and report.bones[cursor].parentIndex or 0
            end
        end
        tImGui.Text(string.format(tLang.L('swl_mirror_subtree_count_fmt'),subtreeCount))
        tImGui.PushItemWidth(190)
        local prefixChanged,prefix=tImGui.InputText(tLang.L('swl_mirror_name_prefix')..'##swlMirrorPrefix',
            state.bindMirrorPrefix,tImGui.Flags('ImGuiInputTextFlags_None'))
        if prefixChanged then state.bindMirrorPrefix=prefix; state.bindMirrorConfirmed=false end
        local axes={'X','Y','Z'}
        local axisChanged,axis=tImGui.Combo(tLang.L('swl_mirror_global_axis'),state.bindMirrorAxis,axes,-1)
        if axisChanged then state.bindMirrorAxis=axis; state.bindMirrorConfirmed=false end
        tImGui.PopItemWidth()
        tImGui.TextWrapped(tLang.L('swl_mirror_subtree_help'))
        local clipBlocked=(report.animationClipCount or 0)>0
        if clipBlocked then
            tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_mirror_clips_blocked'))
        end
        local mirrorPrefix=(state.bindMirrorPrefix or ''):match('^%s*(.-)%s*$')
        tImGui.BeginDisabled(clipBlocked or mirrorPrefix=='')
        local confirmed=tImGui.Checkbox(tLang.L('swl_confirm_mirror_subtree'),state.bindMirrorConfirmed)
        if confirmed~=state.bindMirrorConfirmed then state.bindMirrorConfirmed=confirmed end
        tImGui.BeginDisabled(not state.bindMirrorConfirmed)
        if tImGui.Button(tLang.L('swl_apply_mirror_subtree')..'##swlMirrorApply') then
            local snapshot=stageRollbackSnapshot()
            local ok,newRoot=false,nil
            if snapshot then
                ok,newRoot=safeCall(function()
                    return state.meshD:mirrorSkeletalBoneSubtree(state.boneIndex,
                        state.bindMirrorAxis,mirrorPrefix)
                end)
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot)
                state.modified=true
                refreshBindReport()
                state.boneIndex=newRoot
                state.bindRenameBoneId=nil
                state.bindReparentBoneId=nil
                state.bindEditBoneId=nil
                state.bindAddBoneId=nil
                state.bindChainBoneId=nil
                state.bindMirrorBoneId=nil
                state.bindRemoveConfirmed=false
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_subtree_mirrored'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    local hasWeightsOk,hasWeights=safeCall(function()
        return state.meshD:hasSkeletalVertexWeights()
    end)
    if hasWeightsOk and not hasWeights and
            tImGui.TreeNode(tLang.L('swl_initialize_weights')..'##swlInitializeWeights') then
        local vertexCount=state.meshBounds and state.meshBounds.total or 0
        tImGui.TextWrapped(string.format(tLang.L('swl_initialize_weights_help_fmt'),
            vertexCount,bone.name or '?'))
        local confirmed=tImGui.Checkbox(tLang.L('swl_confirm_initialize_weights'),
            state.bindWeightsConfirmed)
        if confirmed~=state.bindWeightsConfirmed then state.bindWeightsConfirmed=confirmed end
        tImGui.BeginDisabled(not state.bindWeightsConfirmed or vertexCount==0)
        if tImGui.Button(tLang.L('swl_initialize_and_open_weights')..'##swlInitializeWeightsApply') then
            local snapshot=stageRollbackSnapshot()
            local ok,affected=false,nil
            if snapshot then
                ok,affected=safeCall(function()
                    return state.meshD:initializeSkeletalVertexWeights(state.boneIndex)
                end)
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot)
                state.modified=true
                refreshBindReport()
                state.bindWeightsBoneId=nil
                state.allowedBones={}
                for _,item in ipairs(getBones()) do state.allowedBones[item.name]=true end
                invalidateAnalysis()
                rebuildPreview()
                rebuildSkeletonVisuals()
                setWorkspace('weights')
                applyWorkspaceVisibility()
                setStatus(string.format(tLang.L('swl_weights_initialized_fmt'),affected,bone.name),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_remove_bone')..'##swlRemoveBone') then
        tImGui.Text(string.format(tLang.L('swl_remove_bone_impact_fmt'),bone.childCount or 0,
            bone.weightedVertexCount or 0,bone.animationTrackCount or 0))
        if bone.weightPaletteReferenced then
            tImGui.TextColored({r=1,g=0.45,b=0.2,a=1},tLang.L('swl_remove_weight_palette_blocked'))
        end
        local hasReferences=bone.weightPaletteReferenced==true or (bone.animationTrackCount or 0)>0
        local hasChildren=(bone.childCount or 0)>0
        local blocked=#(report.bones or {})<=1
        tImGui.TextWrapped(blocked and tLang.L('swl_remove_bone_blocked') or
            ((hasReferences or hasChildren) and tLang.L('swl_remove_bone_remap_policy') or
                tLang.L('swl_remove_bone_strict_policy')))
        local replacementNames,replacementIndices={},{}
        for index,candidate in ipairs(report.bones or {}) do
            if index~=state.boneIndex then
                replacementNames[#replacementNames+1]=candidate.name or '?'
                replacementIndices[#replacementIndices+1]=index
            end
        end
        if (hasReferences or hasChildren) and #replacementNames>0 then
            local replacementChoice=1
            for choice,index in ipairs(replacementIndices) do
                if index==state.bindRemoveReplacement then replacementChoice=choice break end
            end
            tImGui.PushItemWidth(190)
            local replacementChanged,newChoice=tImGui.Combo(tLang.L('swl_remove_remap_target'),
                replacementChoice,replacementNames,-1)
            tImGui.PopItemWidth()
            if replacementChanged then
                state.bindRemoveReplacement=replacementIndices[newChoice]
                state.bindRemoveConfirmed=false
            end
        end
        if hasChildren then
            local reparent=tImGui.Checkbox(tLang.L('swl_reparent_children_preserve_global'),
                state.bindRemoveReparentChildren)
            if reparent~=state.bindRemoveReparentChildren then
                state.bindRemoveReparentChildren=reparent
                state.bindRemoveConfirmed=false
            end
            if (report.animationClipCount or 0)>0 then
                tImGui.TextWrapped(tLang.L('swl_remove_children_tracks_baked'))
            end
        end
        if (bone.animationTrackCount or 0)>0 then
            local discard=tImGui.Checkbox(tLang.L('swl_discard_removed_bone_tracks'),
                state.bindRemoveDiscardTracks)
            if discard~=state.bindRemoveDiscardTracks then
                state.bindRemoveDiscardTracks=discard
                state.bindRemoveConfirmed=false
            end
        end
        local actionBlocked=blocked or ((bone.animationTrackCount or 0)>0 and
            not state.bindRemoveDiscardTracks) or (hasChildren and
            not state.bindRemoveReparentChildren)
        tImGui.BeginDisabled(actionBlocked)
        local confirmed=tImGui.Checkbox(tLang.L('swl_confirm_remove_bone'),state.bindRemoveConfirmed)
        if confirmed~=state.bindRemoveConfirmed then state.bindRemoveConfirmed=confirmed end
        tImGui.BeginDisabled(not state.bindRemoveConfirmed)
        if tImGui.Button(tLang.L('swl_apply_remove_bone')..'##swlRemoveBoneApply') then
            local previousParentId=bone.parentBoneId
            local snapshot=stageRollbackSnapshot()
            local ok=false
            if snapshot then
                ok=select(1,safeCall(function()
                    if hasReferences or hasChildren then
                        return state.meshD:removeSkeletalBoneRemapped(state.boneIndex,
                            state.bindRemoveReplacement,state.bindRemoveDiscardTracks,
                            state.bindRemoveReparentChildren)
                    end
                    return state.meshD:removeSkeletalBone(state.boneIndex)
                end))
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot)
                state.modified=true
                refreshBindReport()
                state.boneIndex=1
                for index,item in ipairs((state.bindReport and state.bindReport.bones) or {}) do
                    if item.boneId==previousParentId then state.boneIndex=index break end
                end
                state.bindRenameBoneId=nil
                state.bindReparentBoneId=nil
                state.bindEditBoneId=nil
                state.bindAddBoneId=nil
                state.bindRemoveBoneId=nil
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_bone_removed'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        tImGui.EndDisabled()
        tImGui.TreePop()
    end
    if bone.hasNegativeScale then tImGui.TextColored({r=1,g=0.65,b=0.1,a=1},
        tLang.L('swl_bind_negative_scale')) end
    if bone.hasShear then tImGui.TextColored({r=1,g=0.3,b=0.25,a=1},
        tLang.L('swl_bind_shear')) end
    if tImGui.TreeNode(tLang.L('swl_bind_local_matrix')..'##selectedLocal') then
        tImGui.Text(formatBindMatrix(bone.localBindMatrix)); tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_bind_global_matrix')..'##selectedGlobal') then
        tImGui.Text(formatBindMatrix(bone.globalBindMatrix)); tImGui.TreePop()
    end
    if tImGui.TreeNode(tLang.L('swl_bind_inverse_matrix')..'##selectedInverse') then
        tImGui.Text(formatBindMatrix(bone.inverseGlobalBindMatrix)); tImGui.TreePop()
    end
end

local function showBindBoneHierarchy(report)
    local bones=report.bones or {}
    local children={}
    local roots={}
    local diagnosticsByBone={}
    for _,diagnostic in ipairs(report.diagnostics or {}) do
        local index=tonumber(diagnostic.sourceIndex) or 0
        diagnosticsByBone[index]=diagnosticsByBone[index] or {}
        diagnosticsByBone[index][#diagnosticsByBone[index]+1]=diagnostic
    end
    for index,bone in ipairs(bones) do
        local parentIndex=tonumber(bone.parentIndex) or 0
        if parentIndex>0 and parentIndex<=#bones and parentIndex~=index then
            children[parentIndex]=children[parentIndex] or {}
            children[parentIndex][#children[parentIndex]+1]=index
        else
            roots[#roots+1]=index
        end
    end

    local visited={}
    local function showNode(index)
        if visited[index] then return end
        visited[index]=true
        local bone=bones[index]
        local findings=diagnosticsByBone[index]
        local prefix=findings and '! ' or ''
        local label=string.format('%s%s##swlHierarchyBone%d',prefix,bone.name or '?',index)
        local flags=state.boneIndex==index and tImGui.Flags('ImGuiTreeNodeFlags_Selected') or
            tImGui.Flags('ImGuiTreeNodeFlags_None')
        if state.bindTreeOpenAll then
            tImGui.SetNextItemOpen(true,tImGui.Flags('ImGuiCond_Always'))
        end
        if findings then tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'),
            {r=1,g=0.45,b=0.2,a=1}) end
        local open=tImGui.TreeNodeEx(label,flags)
        local clicked=tImGui.IsItemClicked()
        if findings then tImGui.PopStyleColor(1) end
        if clicked then
            state.boneIndex=index
            applyWorkspaceVisibility()
        end
        if open then
            for _,childIndex in ipairs(children[index] or {}) do showNode(childIndex) end
            tImGui.TreePop()
        end
    end
    for _,rootIndex in ipairs(roots) do showNode(rootIndex) end
    -- Defensive display for malformed snapshots; canonical validation should make this empty.
    for index=1,#bones do if not visited[index] then showNode(index) end end
    state.bindTreeOpenAll=false
end

local function showBindPoseDiagnostics()
    local report=state.bindReport
    if not report then
        tImGui.TextDisabled(tLang.L('swl_bind_report_unavailable'))
        tImGui.TextWrapped(tLang.L('swl_initialize_skeleton_help'))
        tImGui.PushItemWidth(190)
        local nameChanged,name=tImGui.InputText(tLang.L('swl_bone_name')..'##swlInitialRootName',
            state.bindInitialName,tImGui.Flags('ImGuiInputTextFlags_None'))
        if nameChanged then state.bindInitialName=name end
        for _,field in ipairs({{'T X','x'},{'T Y','y'},{'T Z','z'}}) do
            local changed,value=tImGui.InputFloat(field[1]..'##swlInitialRoot'..field[2],
                state.bindInitialTranslation[field[2]],0,0,'%.6g',0)
            if changed then state.bindInitialTranslation[field[2]]=value end
        end
        local radiusChanged,radius=tImGui.InputFloat(tLang.L('swl_bone_radius')..'##swlInitialRadius',
            state.bindInitialRadius,0,0,'%.6g',0)
        if radiusChanged then state.bindInitialRadius=radius end
        local lengthChanged,length=tImGui.InputFloat(tLang.L('swl_bone_length')..'##swlInitialLength',
            state.bindInitialLength,0,0,'%.6g',0)
        if lengthChanged then state.bindInitialLength=length end
        tImGui.PopItemWidth()
        local trimmed=(state.bindInitialName or ''):match('^%s*(.-)%s*$')
        tImGui.BeginDisabled(trimmed=='' or state.bindInitialRadius<0 or state.bindInitialLength<0)
        if tImGui.Button(tLang.L('swl_initialize_skeleton')..'##swlInitializeSkeleton') then
            local snapshot=stageRollbackSnapshot()
            local ok=false
            if snapshot then
                local value=state.bindInitialTranslation
                ok=select(1,safeCall(function()
                    return state.meshD:initializeSkeletalSkeleton(trimmed,value.x,value.y,value.z,
                        state.bindInitialRadius,state.bindInitialLength)
                end))
            else
                setStatus(tLang.L('swl_snapshot_failed'),true)
            end
            if ok then
                commitRollbackSnapshot(snapshot)
                state.modified=true
                refreshBindReport()
                state.boneIndex=1
                rebuildPreview()
                rebuildSkeletonVisuals()
                applyWorkspaceVisibility()
                setStatus(tLang.L('swl_skeleton_initialized'),false)
            else
                discardRollbackSnapshot(snapshot)
            end
        end
        tImGui.EndDisabled()
        showRollbackControls('swlInitializeRevert')
        return
    end
    local color=report.valid and {r=0.25,g=0.9,b=0.35,a=1} or {r=1,g=0.3,b=0.25,a=1}
    tImGui.TextColored(color,report.valid and tLang.L('swl_bind_valid') or tLang.L('swl_bind_invalid'))
    tImGui.Text(string.format(tLang.L('swl_bind_error_summary_fmt'),
        report.maximumReconstructionError or 0,report.maximumBindIdentityError or 0,
        report.diagnosticCount or 0))
    if tImGui.Button(tLang.L('swl_refresh')..'##swlBindRefresh') then refreshBindReport() end
    if report.diagnostics and #report.diagnostics>0 and
            tImGui.TreeNode(tLang.L('swl_bind_diagnostics')..'##swlBindDiagnostics') then
        for _,diagnostic in ipairs(report.diagnostics) do
            local label=string.format('%s: %s [%d] error=%.7g',
                diagnostic.fatal and tLang.L('swl_fatal') or tLang.L('swl_warning'),
                diagnostic.code or '?',diagnostic.sourceIndex or 0,diagnostic.observedError or 0)
            tImGui.TextWrapped(label..((diagnostic.boneName and diagnostic.boneName~='') and
                (' - '..diagnostic.boneName) or ''))
        end
        tImGui.TreePop()
    end
    if report.bones and tImGui.TreeNode(string.format('%s (%d)##swlBindBones',
            tLang.L('swl_bind_compiled_bones'),#report.bones)) then
        if tImGui.Button(tLang.L('swl_expand_all')..'##swlBindExpandAll') then
            state.bindTreeOpenAll=true
        end
        tImGui.TextDisabled(tLang.L('swl_hierarchy_scroll_hint'))
        tImGui.BeginChild('##swlBindHierarchyScroll',{x=0,y=300},true)
        showBindBoneHierarchy(report)
        tImGui.EndChild()
        showSelectedBindBone(report)
        tImGui.TreePop()
    end
    showRollbackControls('swlBindRevert')
end

local function openWorkspaceNode(key,label,id)
    local selected=state.workspace==key
    tImGui.SetNextItemOpen(selected,tImGui.Flags('ImGuiCond_Always'))
    local open=tImGui.TreeNodeEx(label,0,id)
    if tImGui.IsItemClicked() and not selected then setWorkspace(key) end
    return open
end

local function showSharedVisualization()
    showSectionTitle('swl_shared_visualization')
    local meshVisible=tImGui.Checkbox(tLang.L('swl_show_mesh'),state.meshVisible)
    if meshVisible~=state.meshVisible then
        state.meshVisible=meshVisible
        applyWorkspaceVisibility()
    end
end

local function showWeightLabSkeletonControls()
    local skeletonVisible=tImGui.Checkbox(tLang.L('swl_show_skeleton'),state.skeletonVisible)
    if skeletonVisible~=state.skeletonVisible then
        state.skeletonVisible=skeletonVisible
        applyWorkspaceVisibility()
    end
    tImGui.SameLine()
    tImGui.BeginDisabled(not state.skeletonVisible)
    local skeletonAlwaysOnTop=tImGui.Checkbox(tLang.L('swl_skeleton_always_on_top'),
        state.skeletonAlwaysOnTop)
    tImGui.EndDisabled()
    if skeletonAlwaysOnTop~=state.skeletonAlwaysOnTop then
        state.skeletonAlwaysOnTop=skeletonAlwaysOnTop
        for _,object in pairs(state.skeletonGizmo.spheres) do
            object.alwaysOnTop=skeletonAlwaysOnTop
        end
        for _,object in pairs(state.skeletonGizmo.bones) do
            object.alwaysOnTop=skeletonAlwaysOnTop
        end
    end
end

local skeletalEasingNames={'Linear','Ease In','Ease Out','Ease In Out','Smoothstep','Cubic Bezier'}

local function skeletalChannelLabel(mask)
    local channels={}
    if (mask & 1)~=0 then channels[#channels+1]='T' end
    if (mask & 2)~=0 then channels[#channels+1]='R' end
    if (mask & 4)~=0 then channels[#channels+1]='S' end
    return table.concat(channels,'+')
end

local function showSkeletalAnimationInspection()
    tImGui.TextWrapped(tLang.L('swl_animation_inspection_help'))
    local ok,clips=safeCall(function() return state.meshD:getSkeletalAnimationReport() end)
    if not ok or type(clips)~='table' or #clips==0 then
        tImGui.TextDisabled(tLang.L('swl_animation_no_clips'))
        return
    end
    local names={}
    for index,clip in ipairs(clips) do names[index]=clip.name or ('Clip '..index) end
    state.animationClipSelected=math.max(1,math.min(state.animationClipSelected,#clips))
    local changed,selected=tImGui.Combo(tLang.L('swl_skeletal_clip'),state.animationClipSelected,names)
    if changed then state.animationClipSelected=selected end
    local clip=clips[state.animationClipSelected]
    tImGui.TextWrapped(string.format(tLang.L('swl_animation_clip_summary_fmt'),
        clip.duration or 0,#(clip.tracks or {}),clip.loop and tLang.L('swl_yes') or tLang.L('swl_no'),
        clip.clipId or '?'))
    tImGui.BeginChild('##swlCanonicalTracks',{x=0,y=360},true)
    for trackIndex,track in ipairs(clip.tracks or {}) do
        local label=string.format(tLang.L('swl_animation_track_fmt'),track.boneName or '?',
            #(track.keys or {}),skeletalChannelLabel(track.channelMask or 0))
        if tImGui.TreeNode(label..'##swlTrack'..trackIndex) then
            if tImGui.IsItemClicked() and track.boneIndex then state.boneIndex=track.boneIndex end
            tImGui.Text(string.format('%s: %s',tLang.L('swl_stable_id'),track.boneId or '?'))
            tImGui.Text(string.format(tLang.L('swl_animation_channels_fmt'),
                skeletalChannelLabel(track.channelMask or 0)))
            for keyIndex,key in ipairs(track.keys or {}) do
                local easing=skeletalEasingNames[(key.easing or 0)+1] or '?'
                if tImGui.TreeNode(string.format(tLang.L('swl_animation_key_fmt'),
                        keyIndex,key.time or 0,easing)..'##swlKey'..trackIndex..'-'..keyIndex) then
                    local p,q,s=key.translation or {},key.rotation or {},key.scale or {}
                    tImGui.Text(string.format('%s: (%.6g, %.6g, %.6g)',
                        tLang.L('swl_animation_translation'),p.x or 0,p.y or 0,p.z or 0))
                    tImGui.Text(string.format('%s: (%.6g, %.6g, %.6g, %.6g)',
                        tLang.L('swl_animation_rotation'),q.x or 0,q.y or 0,q.z or 0,q.w or 1))
                    tImGui.Text(string.format('%s: (%.6g, %.6g, %.6g)',
                        tLang.L('swl_animation_scale'),s.x or 1,s.y or 1,s.z or 1))
                    tImGui.Text(string.format('%s: %s',tLang.L('swl_animation_easing'),easing))
                    if (key.easing or 0)==5 and key.bezier then
                        tImGui.Text(string.format('Bezier: (%.4g, %.4g) (%.4g, %.4g)',
                            key.bezier.x1 or 0,key.bezier.y1 or 0,key.bezier.x2 or 1,key.bezier.y2 or 1))
                    end
                    tImGui.TreePop()
                end
            end
            tImGui.TreePop()
        end
    end
    tImGui.EndChild()
end

local function showPanel()
    local _, screenH = mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=0,y=22}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x=440,y=math.max(500,screenH-27)}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSizeConstraints({x=330,y=320}, {x=900,y=math.max(320,screenH-27)})
    local opened = tImGui.Begin(tLang.L('swl_title'), false, noMoveFlag)
    if opened then
        tImGui.Text(tLang.L('swl_workspace'))
        tImGui.Separator()
        if not state.meshD then
            tImGui.TextWrapped(tLang.L('swl_welcome'))
            if tImGui.Button(tLang.L('swl_open_mesh')) then
                local path = mbm.openFile('', 'msh')
                if path then loadMesh(path) end
            end
        else
            tImGui.Text(shortName(state.fileName) .. (state.modified and ' *' or ''))
            local bones = getBones()
            local okW, hasWeights = safeCall(function() return state.meshD:hasSkeletalVertexWeights() end)
            tImGui.Text(string.format(tLang.L('swl_summary_fmt'), state.aabb and state.aabb.total or 0,
                #bones, okW and hasWeights and tLang.L('swl_yes') or tLang.L('swl_no')))
            showStatusMessage()
            showSharedVisualization()
            tImGui.Separator()
            tImGui.Text(tLang.L('swl_workspaces'))
            if openWorkspaceNode('bind',tLang.L('swl_bind_pose_contract'),'##swlBindPoseContract') then
                showBindPoseDiagnostics()
                tImGui.TreePop()
            end
            if openWorkspaceNode('runtime',tLang.L('swl_runtime_preview'),'##swlRuntimePreview') then
                showSkeletalPreviewControls()
                tImGui.TreePop()
            end
            if openWorkspaceNode('animation',tLang.L('swl_animation_workspace'),
                    '##swlAnimationWorkspace') then
                showSkeletalAnimationInspection()
                tImGui.TreePop()
            end
            if openWorkspaceNode('paint',tLang.L('swl_paint_weights_workspace'),
                    '##swlPaintWeightsWorkspace') then
                tImGui.TextWrapped(tLang.L('swl_paint_weights_workspace_reserved'))
                tImGui.TreePop()
            end
            if openWorkspaceNode('weights',tLang.L('swl_weight_lab_workspace'),
                    '##swlWeightLabWorkspace') then
            showSectionTitle('swl_visualization')
            showWeightLabSkeletonControls()
            local markersAlwaysOnTop=tImGui.Checkbox(tLang.L('swl_markers_always_on_top'),
                state.markersAlwaysOnTop)
            if markersAlwaysOnTop~=state.markersAlwaysOnTop then
                state.markersAlwaysOnTop=markersAlwaysOnTop
                if state.selectionLines then state.selectionLines.alwaysOnTop=markersAlwaysOnTop end
                if state.transitionLines then state.transitionLines.alwaysOnTop=markersAlwaysOnTop end
                for _,marker in ipairs(state.heatmapLines) do marker.alwaysOnTop=markersAlwaysOnTop end
                if state.abruptLines then state.abruptLines.alwaysOnTop=markersAlwaysOnTop end
                if state.boundaryLines then state.boundaryLines.alwaysOnTop=markersAlwaysOnTop end
            end
            local hasAnalysisMarkers=state.selectionLines~=nil or state.transitionLines~=nil or
                #state.heatmapLines>0
            tImGui.BeginDisabled(not hasAnalysisMarkers)
            local analysisMarkersVisible=tImGui.Checkbox(tLang.L('swl_show_analysis_markers'),
                state.analysisMarkersVisible)
            if analysisMarkersVisible~=state.analysisMarkersVisible then
                state.analysisMarkersVisible=analysisMarkersVisible
                if state.selectionLines then state.selectionLines.visible=analysisMarkersVisible end
                if state.transitionLines then state.transitionLines.visible=analysisMarkersVisible end
                for _,marker in ipairs(state.heatmapLines) do marker.visible=analysisMarkersVisible end
            end
            tImGui.EndDisabled()
            tImGui.BeginDisabled(state.abruptLines==nil)
            local abruptMarkersVisible=tImGui.Checkbox(tLang.L('swl_show_abrupt_markers'),
                state.abruptMarkersVisible)
            if abruptMarkersVisible~=state.abruptMarkersVisible then
                state.abruptMarkersVisible=abruptMarkersVisible
                if state.abruptLines then state.abruptLines.visible=abruptMarkersVisible end
            end
            tImGui.EndDisabled()
            tImGui.BeginDisabled(state.boundaryLines==nil)
            local boundaryMarkersVisible=tImGui.Checkbox(tLang.L('swl_show_boundary_markers'),
                state.boundaryMarkersVisible)
            if boundaryMarkersVisible~=state.boundaryMarkersVisible then
                state.boundaryMarkersVisible=boundaryMarkersVisible
                if state.boundaryLines then state.boundaryLines.visible=boundaryMarkersVisible end
            end
            tImGui.EndDisabled()
            tImGui.Separator()
            showSectionTitle('swl_selection_analysis')
            showSelectionInputs()
            if tImGui.Button(tLang.L('swl_analyze')) then analyzeSelection() end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel')) then invalidateAnalysis() end
            if state.analysis then
                local a = state.analysis
                tImGui.Text(string.format(tLang.L('swl_selected_fmt'), #a.vertices, a.totalMesh))
                tImGui.Text(string.format(tLang.L('swl_core_shell_fmt'),#a.core,#a.shell))
                tImGui.Text(string.format(tLang.L('swl_diagnostics_fmt'), a.missing, a.invalidSum, a.unknown))
                tImGui.Text(string.format(tLang.L('swl_disallowed_fmt'),a.disallowed))
                if state.heatmapEnabled and a.heatmapBoneName and #a.vertices>0 then
                    if a.heatmapInfluenced>0 then
                        tImGui.Text(string.format(tLang.L('swl_heatmap_analysis_stats_fmt'),
                            a.heatmapBoneName,a.heatmapInfluenced,#a.vertices,
                            a.heatmapMinPositive,a.heatmapMax))
                    else
                        tImGui.PushStyleColor('ImGuiCol_Text',{r=1,g=0.75,b=0.15,a=1})
                        tImGui.TextWrapped(string.format(tLang.L('swl_heatmap_no_influence_fmt'),
                            a.heatmapBoneName))
                        tImGui.PopStyleColor()
                    end
                end
            elseif state.analysisDirty then
                tImGui.TextDisabled(tLang.L('swl_analysis_required'))
            end
            tImGui.Separator()
            showSectionTitle('swl_operation')
            local operations={tLang.L('swl_operation_inspect'),tLang.L('swl_operation_rigid'),
                tLang.L('swl_operation_normalize'),tLang.L('swl_operation_smooth'),
                tLang.L('swl_operation_repair')}
            tImGui.PushItemWidth(240)
            local operationChanged,operation=tImGui.Combo(tLang.L('swl_operation_mode'),
                state.operationMode,operations,-1)
            tImGui.PopItemWidth()
            showItemTooltip(tLang.L('swl_operation_mode_tooltip'))
            if operationChanged then
                state.operationMode=operation
                state.allowedBonesHighlight=false
                state.hoveredAllowedBone=nil
                updateAllowedBoneColors()
                if operation~=2 and state.targetBoneHighlight then
                    state.targetBoneHighlight=false
                    rebuildTargetBoneHighlight()
                end
            end
            local canOperate=state.analysis and not state.analysisDirty and
                #state.analysis.vertices>0

            if state.operationMode==3 then
            tImGui.Text(tLang.L('swl_weight_cleanup'))
            local canNormalize=state.analysis and not state.analysisDirty and #state.analysis.vertices>0
            tImGui.BeginDisabled(not canNormalize)
            local normalizePressed=tImGui.Button(tLang.L('swl_normalize_limit'))
            showItemTooltip(tLang.L('swl_normalize_limit_tooltip'))
            if normalizePressed then applyNormalizeAndLimit() end
            tImGui.EndDisabled()
            if state.normalizeReport then
                local report=state.normalizeReport
                tImGui.TextColored({r=0.25,g=0.80,b=1,a=1},tLang.L('swl_last_report'))
                tImGui.Text(string.format(tLang.L('swl_normalize_report_fmt'),report.total,
                    report.corrected,report.unchanged,report.skipped,report.failed))
            end
            elseif state.operationMode==2 then
            tImGui.Text(tLang.L('swl_rigid_bind'))
            local names = {}
            for _, bone in ipairs(bones) do names[#names+1] = bone.name end
            if #names > 0 then
                state.targetBoneIndex = math.min(state.targetBoneIndex, #names)
                tImGui.PushItemWidth(190)
                local edited, value = tImGui.Combo(tLang.L('swl_target_bone'), state.targetBoneIndex, names, -1)
                tImGui.PopItemWidth()
                tImGui.SameLine()
                local highlight=tImGui.Checkbox(tLang.L('swl_highlight')..'##swlTargetBoneHighlight',
                    state.targetBoneHighlight)
                if highlight~=state.targetBoneHighlight then
                    state.targetBoneHighlight=highlight
                    rebuildTargetBoneHighlight()
                end
                if edited then
                    state.targetBoneIndex=value
                    rebuildTargetBoneHighlight()
                end
            end
            showBoneRestrictions(bones,true)
            local canApply=state.analysis and not state.analysisDirty and
                #state.analysis.vertices>0 and #bones>0
            tImGui.BeginDisabled(not canApply)
            local applyRigidPressed=tImGui.Button(state.selectionMode==1 and hasTransitionShell() and
                    tLang.L('swl_apply_transition') or tLang.L('swl_apply_rigid'))
            showItemTooltip(tLang.L('swl_apply_rigid_tooltip'))
            if applyRigidPressed then
                applyRigidBind()
            end
            tImGui.EndDisabled()
            elseif state.operationMode==4 then
            tImGui.Text(tLang.L('swl_local_smoothing'))
            showBoneRestrictions(bones,false)
            showSmoothingControls()
            local canSmooth=state.analysis and not state.analysisDirty and #state.analysis.vertices>0
            tImGui.BeginDisabled(not canSmooth)
            if tImGui.Button(tLang.L('swl_apply_smoothing')) then applyLocalSmoothing() end
            tImGui.EndDisabled()
            tImGui.TextDisabled(tLang.L('swl_smoothing_scope'))
            elseif state.operationMode==1 then
            tImGui.Text(tLang.L('swl_transition_diagnostics'))
            showDiagnosticControls(canOperate,false)
            elseif state.operationMode==5 then
            tImGui.Text(tLang.L('swl_transition_repair'))
            showBoneRestrictions(bones,false)
            showSmoothingControls()
            showDiagnosticControls(canOperate,true)
            end
            showRollbackControls('swlWeightRevert')
            tImGui.TreePop()
            end
        end
        if not state.meshD then showStatusMessage() end
    end
    tImGui.End()
end

local function showCameraPanel()
    local screenW = mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x=math.max(0,screenW-315),y=25}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x=310,y=410}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSizeConstraints({x=270,y=260}, {x=600,y=800})
    local opened = tImGui.Begin(tLang.L('camera_panel') .. ' 3D##swlCamera', false,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if opened then
        if tUtil.drawOrbitGizmo(state.cam, {size=120}) then applyCamera() end
        tImGui.Separator()
        local px,py,pz = cameraPosition()
        tImGui.PushItemWidth(105)
        tImGui.Text(tLang.L('cam_position'))
        local xChanged,nx = tImGui.InputFloat('X##swlCamPX',px,0,0,'%.3f',0)
        tImGui.SameLine()
        local yChanged,ny = tImGui.InputFloat('Y##swlCamPY',py,0,0,'%.3f',0)
        local zChanged,nz = tImGui.InputFloat('Z##swlCamPZ',pz,0,0,'%.3f',0)
        if xChanged or yChanged or zChanged then
            nx,ny,nz = xChanged and nx or px, yChanged and ny or py, zChanged and nz or pz
            local dx,dy,dz = nx-state.cam.fx,ny-state.cam.fy,nz-state.cam.fz
            local distance = math.sqrt(dx*dx+dy*dy+dz*dz)
            if distance > 1e-6 then
                state.cam.distance = distance
                state.cam.elevation = math.asin(math.max(-1,math.min(1,dy/distance)))
                state.cam.azimuth = math.atan(dx,dz)
                applyCamera()
            end
        end
        tImGui.Text(tLang.L('cam_focus'))
        local step = math.max(state.cam.distance*0.0025,0.0001)
        local fxChanged,nfx = tImGui.DragFloat('X##swlCamFX',state.cam.fx,step,0,0,'%.3f')
        tImGui.SameLine()
        local fyChanged,nfy = tImGui.DragFloat('Y##swlCamFY',state.cam.fy,step,0,0,'%.3f')
        local fzChanged,nfz = tImGui.DragFloat('Z##swlCamFZ',state.cam.fz,step,0,0,'%.3f')
        tImGui.PopItemWidth()
        if fxChanged then state.cam.fx=nfx end
        if fyChanged then state.cam.fy=nfy end
        if fzChanged then state.cam.fz=nfz end
        if fxChanged or fyChanged or fzChanged then applyCamera() end
        if tImGui.Button(tLang.L('reset_camera')) then frameCamera(state.meshBounds) end
        tImGui.TextDisabled(tLang.L('cam_hint_3d'))
        tImGui.TextDisabled(tLang.L('cam_hint_keyboard'))
        tImGui.TextDisabled('Scroll: zoom  |  Drag AABB: move')
    end
    tImGui.End()
end

function onInitScene()
    camera3d = mbm.getCamera('3d')
    camera3d:setFar(9999999)
    noMoveFlag = tImGui.Flags('ImGuiWindowFlags_NoCollapse')
    tUtil.sMessageOverlay = tLang.L('swl_welcome')
    tUtil.bRightSide = true
    tUtil.tTimerOverlay:start()
    mbm.setColor(0.08, 0.09, 0.12)
    applyCamera()
end

function onLoop(delta)
    updateCameraKeyboard(delta)
    showMenu()
    showPanel()
    showCameraPanel()
    syncPoseStressPreview()
    tUtil.showOverlayMessage()
end

function onTouchDown(key, x, y)
    if key == 0 and not tImGui.GetWantCaptureMouse() then
        if isWeightLabWorkspace() and state.meshD and state.selectionMode == 1 and state.aabb and
                rayHitsAABB(x,y,state.aabb) then
            local px,py,pz = cameraPosition()
            local nx,ny,nz = state.cam.fx-px,state.cam.fy-py,state.cam.fz-pz
            local length = math.sqrt(nx*nx+ny*ny+nz*nz)
            if length > 1e-6 then nx,ny,nz=nx/length,ny/length,nz/length end
            local center = {x=(state.aabb.minX+state.aabb.maxX)*0.5,
                y=(state.aabb.minY+state.aabb.maxY)*0.5,z=(state.aabb.minZ+state.aabb.maxZ)*0.5}
            local plane = {point=center,normal={x=nx,y=ny,z=nz}}
            local wx,wy,wz = rayPlaneHit(x,y,plane.point,plane.normal)
            if wx then
                state.aabbDragging = true
                state.aabbDragPlane = plane
                state.aabbDragOffset = {x=center.x-wx,y=center.y-wy,z=center.z-wz}
                return
            end
        end
        mouseDown, mouseX, mouseY = true, x, y
    end
end

function onTouchMove(key, x, y)
    if isWeightLabWorkspace() and state.aabbDragging and state.aabbDragPlane then
        local wx,wy,wz = rayPlaneHit(x,y,state.aabbDragPlane.point,state.aabbDragPlane.normal)
        if wx then
            local b,o = state.aabb,state.aabbDragOffset
            local cx,cy,cz = (b.minX+b.maxX)*0.5,(b.minY+b.maxY)*0.5,(b.minZ+b.maxZ)*0.5
            local nx,ny,nz = wx+o.x,wy+o.y,wz+o.z
            local dx,dy,dz = nx-cx,ny-cy,nz-cz
            b.minX,b.maxX=b.minX+dx,b.maxX+dx
            b.minY,b.maxY=b.minY+dy,b.maxY+dy
            b.minZ,b.maxZ=b.minZ+dz,b.maxZ+dz
            state.aabbDragPlane.point={x=nx,y=ny,z=nz}
            invalidateAnalysis()
            rebuildSelectionBox()
        end
    elseif mouseDown and not tImGui.GetWantCaptureMouse() then
        state.cam.azimuth = state.cam.azimuth + (x-mouseX) * 0.008
        state.cam.elevation = math.max(-1.45, math.min(1.45, state.cam.elevation + (y-mouseY) * 0.008))
        mouseX, mouseY = x, y
        applyCamera()
    end
end

function onTouchUp(key, x, y)
    if key == 0 then
        mouseDown = false
        state.aabbDragging = false
        state.aabbDragPlane = nil
        state.aabbDragOffset = nil
    end
end

function onTouchZoom(zoom)
    if tImGui.GetWantCaptureMouse() then return end
    state.cam.distance = math.max(0.01, state.cam.distance * (1 - zoom * 0.15))
    applyCamera()
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then
        state.controlDown = true
    elseif state.controlDown and key == mbm.getKeyCode('O') then
        local path = mbm.openFile(state.fileName or '', 'msh')
        if path then loadMesh(path) end
    elseif state.controlDown and key == mbm.getKeyCode('S') then
        saveTo(state.fileName)
    elseif state.controlDown and key == mbm.getKeyCode('Q') then
        mbm.quit()
    elseif not state.controlDown and key == mbm.getKeyCode('W') then cameraMove.forward=1
    elseif not state.controlDown and key == mbm.getKeyCode('S') then cameraMove.forward=-1
    elseif not state.controlDown and key == mbm.getKeyCode('A') then cameraMove.right=-1
    elseif not state.controlDown and key == mbm.getKeyCode('D') then cameraMove.right=1
    elseif key == mbm.getKeyCode('pageup') then cameraMove.vertical=1
    elseif key == mbm.getKeyCode('pagedown') then cameraMove.vertical=-1
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then state.controlDown = false
    elseif key == mbm.getKeyCode('W') or key == mbm.getKeyCode('S') then cameraMove.forward=0
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('D') then cameraMove.right=0
    elseif key == mbm.getKeyCode('pageup') or key == mbm.getKeyCode('pagedown') then cameraMove.vertical=0
    end
end
