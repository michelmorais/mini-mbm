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

   Real-Time Skinning Editor — Phase 1 Skin Weight Lab
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
    selectionBox = nil,
    status = nil,
    statusError = false,
    cam = {azimuth = 0.35, elevation = 0.25, distance = 5, fx = 0, fy = 0, fz = 0},
}

local camera3d
local mouseDown = false
local mouseX, mouseY = 0, 0
local noMoveFlag = 0

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
    destroyObject(state.selectionBox)
    state.selectionLines = nil
    state.selectionBox = nil
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
    state.selectionLines = nil
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

local function rebuildSelectionBox()
    destroyObject(state.selectionBox)
    state.selectionBox = nil
    if not state.meshD or state.selectionMode ~= 1 or not state.aabb then return end
    local b = state.aabb
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
    box:setColor(0, 1, 1, 1)
    box.z = -10
    state.selectionBox = box
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

local function analyzeSelection()
    if not state.meshD then return end
    local allVertices = collectVertices()
    local selected, bones = {}, getBones()
    if state.selectionMode == 1 then
        local b = state.aabb
        for _, v in ipairs(allVertices) do
            local p = v.point
            if p.x >= b.minX and p.x <= b.maxX and p.y >= b.minY and p.y <= b.maxY and
               p.z >= b.minZ and p.z <= b.maxZ then selected[#selected+1] = v end
        end
    elseif state.selectionMode == 2 then
        for _, v in ipairs(allVertices) do
            if v.subset == state.subsetIndex then selected[#selected+1] = v end
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
    state.analysis = {vertices=selected, missing=missing, invalidSum=invalidSum, unknown=unknown, totalMesh=#allVertices}
    state.analysisDirty = false
    destroyObject(state.selectionLines)
    state.selectionLines = nil
    if #selected > 0 then
        local extent = state.aabb and math.max(state.aabb.maxX-state.aabb.minX,
            state.aabb.maxY-state.aabb.minY, state.aabb.maxZ-state.aabb.minZ) or 1
        local size = math.max(extent * 0.006, 0.001)
        local coords, step = {}, math.max(1, math.ceil(#selected / 500))
        for i = 1, #selected, step do
            local p = selected[i].point
            appendPoint(coords, p.x-size, p.y, p.z)
            appendPoint(coords, p.x+size, p.y, p.z)
            appendPoint(coords, p.x, p.y-size, p.z)
            appendPoint(coords, p.x, p.y+size, p.z)
        end
        local marks = line:new('3d', 0, 0, 0)
        marks:add(coords)
        marks:setColor(1, 0.15, 0.1, 1)
        marks.z = -20
        state.selectionLines = marks
    end
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
    for _, vertex in ipairs(state.analysis.vertices) do
        local ok, result = safeCall(function()
            return state.meshD:setVertexWeight(vertex.globalIndex, target.name, 1, nil, 0, nil, 0, nil, 0)
        end)
        if ok and result then applied = applied + 1 end
    end
    state.modified = applied > 0
    invalidateAnalysis()
    setStatus(string.format(tLang.L('swl_applied_fmt'), applied, target.name), applied == 0)
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
    local changed, mode = tImGui.Combo(tLang.L('swl_selection_method'), state.selectionMode, labels, -1)
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
        for _, field in ipairs(fields) do
            local edited, value = tImGui.DragFloat(field[1], b[field[2]], 0.01, -1000000, 1000000, '%.4f')
            if edited then b[field[2]] = value; aabbChanged = true end
        end
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
    local screenW, screenH = mbm.getSizeScreen()
    tImGui.SetNextWindowPos({x=0,y=22}, tImGui.Flags('ImGuiCond_Always'))
    tImGui.SetNextWindowSize({x=390,y=screenH-22}, tImGui.Flags('ImGuiCond_Always'))
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
                if edited then state.targetBoneIndex=value end
            end
            local canApply = state.analysis and not state.analysisDirty and #state.analysis.vertices > 0 and #bones > 0
            tImGui.BeginDisabled(not canApply)
            if tImGui.Button(tLang.L('swl_apply_rigid')) then applyRigidBind() end
            tImGui.EndDisabled()
            tImGui.SameLine()
            tImGui.BeginDisabled(state.rollbackPath == nil)
            if tImGui.Button(tLang.L('swl_revert')) then revertLast() end
            tImGui.EndDisabled()
            tImGui.Separator()
            tImGui.TextDisabled(tLang.L('swl_phase1_notice'))
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

function onInitScene()
    camera3d = mbm.getCamera('3d')
    camera3d:setFar(9999999)
    noMoveFlag = tImGui.Flags('ImGuiWindowFlags_NoMove', 'ImGuiWindowFlags_NoResize', 'ImGuiWindowFlags_NoCollapse')
    tUtil.sMessageOverlay = tLang.L('swl_welcome')
    tUtil.bRightSide = true
    tUtil.tTimerOverlay:start()
    mbm.setColor(0.08, 0.09, 0.12)
    applyCamera()
end

function onLoop(delta)
    showMenu()
    showPanel()
    tUtil.showOverlayMessage()
end

function onTouchDown(key, x, y)
    if key == 0 and not tImGui.GetWantCaptureMouse() then
        mouseDown, mouseX, mouseY = true, x, y
    end
end

function onTouchMove(key, x, y)
    if mouseDown and not tImGui.GetWantCaptureMouse() then
        state.cam.azimuth = state.cam.azimuth + (x-mouseX) * 0.008
        state.cam.elevation = math.max(-1.45, math.min(1.45, state.cam.elevation + (y-mouseY) * 0.008))
        mouseX, mouseY = x, y
        applyCamera()
    end
end

function onTouchUp(key, x, y)
    if key == 0 then mouseDown = false end
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
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then state.controlDown = false end
end
