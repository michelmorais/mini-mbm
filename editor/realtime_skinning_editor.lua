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

   Real-Time Skinning Editor — Phase 2 Skin Weight Lab
]]--

tImGui = require "ImGui"
tUtil = require "editor_utils"

local state = {
    fileName = nil,
    meshD = nil,
    preview = nil,
    info = nil,
    modified = false,
    selectionMode = 1, -- 1 AABB, 2 subset, 3 bone proximity
    subsetIndex = 1,
    boneIndex = 1,
    targetBoneIndex = 1,
    aabb = nil,
    analysis = nil,
    analysisDirty = true,
    rollbackPath = nil,
    rollbackModified = nil,
    selectionLines = nil,
    transitionLines = nil,
    selectionBox = nil,
    transitionBox = nil,
    shellWidth = 0,
    falloffMode = 2, -- 1 linear, 2 smooth
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
    state.selectionLines = nil
    state.transitionLines = nil
    state.selectionBox = nil
    state.transitionBox = nil
end

local function clearRollback()
    if state.rollbackPath then pcall(os.remove, state.rollbackPath) end
    state.rollbackPath = nil
    state.rollbackModified = nil
end

local function invalidateAnalysis()
    state.analysis = nil
    state.analysisDirty = true
    destroyObject(state.selectionLines)
    destroyObject(state.transitionLines)
    state.selectionLines = nil
    state.transitionLines = nil
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
    box.z = -10
    return box
end

local function rebuildSelectionBox()
    destroyObject(state.selectionBox)
    destroyObject(state.transitionBox)
    state.selectionBox, state.transitionBox = nil, nil
    if not state.meshD or state.selectionMode ~= 1 or not state.aabb then return end
    state.selectionBox = createSelectionBox(state.aabb, 0, 1, 1)
    if state.shellWidth > 0 then
        local b, width = state.aabb, state.shellWidth
        state.transitionBox = createSelectionBox({
            minX=b.minX-width,minY=b.minY-width,minZ=b.minZ-width,
            maxX=b.maxX+width,maxY=b.maxY+width,maxZ=b.maxZ+width,
        }, 1, 0.65, 0)
    end
end

local function getBones()
    local bones = {}
    if not state.meshD then return bones end
    local okT, total = safeCall(function() return state.meshD:getTotalBone() end)
    if not okT then return bones end
    for i = 1, total do
        local ok, name, x, y, z, radius, parentName = safeCall(function() return state.meshD:getBone(i) end)
        if ok and name then
            bones[#bones+1] = {index=i, name=name, x=x, y=y, z=z, radius=radius or 0, parentName=parentName}
        end
    end
    return bones
end

local function findBone(bones, name)
    for _, bone in ipairs(bones) do if bone.name == name then return bone end end
    return nil
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

local function transitionAlpha(p, b, width)
    if pointInsideAABB(p, b) then return 1, 'core' end
    if width <= 0 then return nil end
    local dx = math.max(b.minX-p.x, 0, p.x-b.maxX)
    local dy = math.max(b.minY-p.y, 0, p.y-b.maxY)
    local dz = math.max(b.minZ-p.z, 0, p.z-b.maxZ)
    local t = math.max(dx,dy,dz) / width
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
    marks:add(coords); marks:setColor(r,g,b,1); marks.z=-20
    return marks
end

local function analyzeSelection()
    if not state.meshD then return end
    local allVertices = collectVertices()
    local selected, bones = {}, getBones()
    if state.selectionMode == 1 then
        local b = state.aabb
        for _, v in ipairs(allVertices) do
            local alpha,region=transitionAlpha(v.point,b,state.shellWidth)
            if alpha then v.blendAlpha,v.region=alpha,region; selected[#selected+1]=v end
        end
    elseif state.selectionMode == 2 then
        for _, v in ipairs(allVertices) do
            if v.subset == state.subsetIndex then v.blendAlpha,v.region=1,'core'; selected[#selected+1] = v end
        end
    else
        local target = bones[state.boneIndex]
        if target then
            local segments = {}
            for _, bone in ipairs(bones) do
                segments[bone.name] = {a=bone, b=findBone(bones, bone.parentName) or bone}
            end
            local radiusSquared = target.radius * target.radius
            for _, v in ipairs(allVertices) do
                local nearestName, nearestDistance
                for name, segment in pairs(segments) do
                    local distance = pointSegmentDistanceSquared(v.point, segment.a, segment.b)
                    if not nearestDistance or distance < nearestDistance then
                        nearestName, nearestDistance = name, distance
                    end
                end
                if nearestName == target.name and nearestDistance <= radiusSquared then
                    v.blendAlpha,v.region=1,'core'
                    selected[#selected+1] = v
                end
            end
        end
    end

    local missing, invalidSum, unknown = 0, 0, 0
    local known = {}
    for _, bone in ipairs(bones) do known[bone.name] = true end
    for _, vertex in ipairs(selected) do
        local ok, n1,w1,n2,w2,n3,w3,n4,w4 = safeCall(function()
            return state.meshD:getVertexWeight(vertex.globalIndex)
        end)
        if not ok or not n1 then
            missing = missing + 1
        else
            local sum = 0
            for _, pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
                if pair[1] then
                    sum = sum + (pair[2] or 0)
                    if not known[pair[1]] then unknown = unknown + 1 end
                end
            end
            if math.abs(sum - 1) > 0.001 then invalidSum = invalidSum + 1 end
        end
    end
    local core,shell={},{}
    for _,vertex in ipairs(selected) do
        if vertex.region=='shell' then shell[#shell+1]=vertex else core[#core+1]=vertex end
    end
    state.analysis = {vertices=selected, core=core, shell=shell, missing=missing,
        invalidSum=invalidSum, unknown=unknown, totalMesh=#allVertices}
    state.analysisDirty = false
    destroyObject(state.selectionLines)
    destroyObject(state.transitionLines)
    state.selectionLines = nil
    state.transitionLines = nil
    local extent=state.meshBounds and math.max(state.meshBounds.maxX-state.meshBounds.minX,
        state.meshBounds.maxY-state.meshBounds.minY,state.meshBounds.maxZ-state.meshBounds.minZ) or 1
    state.selectionLines=buildVertexMarkers(core,1,0.15,0.1,extent)
    state.transitionLines=buildVertexMarkers(shell,1,0.75,0.1,extent)
    setStatus(string.format(tLang.L('swl_analysis_complete_fmt'), #selected), false)
end

local function rebuildPreview()
    destroyObject(state.preview)
    state.preview = nil
    if not state.fileName then return end
    local preview = mesh:new('3d')
    if preview:load(state.fileName) then
        state.preview = preview
    else
        preview:destroy()
    end
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
    state.fileName, state.meshD = path, meshD
    state.info = meshDebug:getInfo(path)
    state.modified = false
    state.analysis = nil
    state.analysisDirty = true
    state.subsetIndex, state.boneIndex, state.targetBoneIndex = 1, 1, 1
    local bounds = computeAABB(meshD)
    state.meshBounds = bounds
    state.aabb = bounds
    rebuildPreview()
    rebuildSelectionBox()
    frameCamera(bounds)
    setStatus(string.format(tLang.L('swl_loaded_fmt'), shortName(path)), false)
    return true
end

local function snapshotForRollback()
    clearRollback()
    local path = os.tmpname() .. '.msh'
    if not state.meshD:save(path, false, false) then return false end
    state.rollbackPath = path
    state.rollbackModified = state.modified
    return true
end

local function blendedInfluences(globalIndex,targetName,alpha)
    if alpha >= 0.999999 then return {{name=targetName,weight=1}} end
    local ok,n1,w1,n2,w2,n3,w3,n4,w4=safeCall(function()
        return state.meshD:getVertexWeight(globalIndex)
    end)
    local byName={}
    if ok then
        for _,pair in ipairs({{n1,w1},{n2,w2},{n3,w3},{n4,w4}}) do
            local name,weight=pair[1],tonumber(pair[2]) or 0
            if name and weight>0 and weight==weight and weight<math.huge then
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
    return state.meshD:setVertexWeight(globalIndex,
        a and a.name or nil,a and a.weight or 0,
        b and b.name or nil,b and b.weight or 0,
        c and c.name or nil,c and c.weight or 0,
        d and d.name or nil,d and d.weight or 0)
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
    invalidateAnalysis()
    rebuildSelectionBox()
    setStatus(tLang.L('swl_reverted'), false)
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

local function showSelectionInputs()
    local labels = {tLang.L('swl_selection_aabb'), tLang.L('swl_selection_subset'), tLang.L('swl_selection_bone')}
    tImGui.PushItemWidth(190)
    local changed, mode = tImGui.Combo(tLang.L('swl_selection_method'), state.selectionMode, labels, -1)
    tImGui.PopItemWidth()
    if changed then
        state.selectionMode = mode
        invalidateAnalysis()
        rebuildSelectionBox()
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
        local dragSpeed = math.max(extent * 0.0025, 0.0001)
        tImGui.PushItemWidth(150)
        for _, field in ipairs(fields) do
            local edited, value = tImGui.DragFloat(field[1], b[field[2]], dragSpeed, -1000000, 1000000, '%.4f')
            if edited then b[field[2]] = value; aabbChanged = true end
        end
        local shellChanged,shell=tImGui.DragFloat(tLang.L('swl_shell_width'),state.shellWidth,
            dragSpeed,0,math.max(extent*2,dragSpeed),'%.4f')
        tImGui.PopItemWidth()
        if shellChanged then state.shellWidth=math.max(0,shell); aabbChanged=true end
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
        local edited, value = tImGui.SliderInt(tLang.L('swl_subset'), state.subsetIndex, 1, math.max(1,total))
        if edited then state.subsetIndex=value; invalidateAnalysis() end
    else
        local bones, names = getBones(), {}
        for _, bone in ipairs(bones) do names[#names+1] = bone.name end
        if #names > 0 then
            local edited, value = tImGui.Combo(tLang.L('swl_source_bone'), math.min(state.boneIndex,#names), names, -1)
            if edited then state.boneIndex=value; invalidateAnalysis() end
        else
            tImGui.TextDisabled(tLang.L('swl_no_bones'))
        end
    end
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
            local okW, hasWeights = safeCall(function() return state.meshD:hasVertexWeights() end)
            tImGui.Text(string.format(tLang.L('swl_summary_fmt'), state.aabb and state.aabb.total or 0,
                #bones, okW and hasWeights and tLang.L('swl_yes') or tLang.L('swl_no')))
            tImGui.Separator()
            showSelectionInputs()
            if tImGui.Button(tLang.L('swl_analyze')) then analyzeSelection() end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel')) then invalidateAnalysis() end
            if state.analysis then
                local a = state.analysis
                tImGui.Text(string.format(tLang.L('swl_selected_fmt'), #a.vertices, a.totalMesh))
                tImGui.Text(string.format(tLang.L('swl_core_shell_fmt'),#a.core,#a.shell))
                tImGui.Text(string.format(tLang.L('swl_diagnostics_fmt'), a.missing, a.invalidSum, a.unknown))
            elseif state.analysisDirty then
                tImGui.TextDisabled(tLang.L('swl_analysis_required'))
            end
            tImGui.Separator()
            tImGui.Text(tLang.L('swl_rigid_bind'))
            local names = {}
            for _, bone in ipairs(bones) do names[#names+1] = bone.name end
            if #names > 0 then
                state.targetBoneIndex = math.min(state.targetBoneIndex, #names)
                local edited, value = tImGui.Combo(tLang.L('swl_target_bone'), state.targetBoneIndex, names, -1)
                if edited then state.targetBoneIndex=value; invalidateAnalysis() end
            end
            local canApply = state.analysis and not state.analysisDirty and #state.analysis.vertices > 0 and #bones > 0
            tImGui.BeginDisabled(not canApply)
            if tImGui.Button(state.selectionMode==1 and state.shellWidth>0 and tLang.L('swl_apply_transition') or
                    tLang.L('swl_apply_rigid')) then applyRigidBind() end
            tImGui.EndDisabled()
            tImGui.SameLine()
            tImGui.BeginDisabled(state.rollbackPath == nil)
            if tImGui.Button(tLang.L('swl_revert')) then revertLast() end
            tImGui.EndDisabled()
            tImGui.Separator()
            tImGui.TextDisabled(tLang.L('swl_phase2_notice'))
        end
        if state.status then
            tImGui.Separator()
            if state.statusError then
                tImGui.TextColored({r=1,g=0.3,b=0.2,a=1}, state.status)
            else
                tImGui.TextWrapped(state.status)
            end
        end
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
    tUtil.showOverlayMessage()
end

function onTouchDown(key, x, y)
    if key == 0 and not tImGui.GetWantCaptureMouse() then
        if state.meshD and state.selectionMode == 1 and state.aabb and rayHitsAABB(x,y,state.aabb) then
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
    if state.aabbDragging and state.aabbDragPlane then
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
