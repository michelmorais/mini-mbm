--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                        |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Scene Editor 3D

   This is a script based on mbm engine.

   Scene Editor 3D is a quick-setup tool for 3D scenes: place any renderizable (mesh, sprite, texture, tile,
   particle, gif) on an orthogonal or isometric grid (or freely), organize them into Y layers, and export a
   ready-to-`require` Lua scene script for the real game to load.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html#scene-editor-3d

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"
-- tLang becomes globally available as a side effect of requiring editor_utils above.
-- There is no native plugin for 3D scenes (unlike `require "tilemap"` for 2D) -- every mechanism
-- below (grid math, hover/selection, object markers, thumbnail cache) is authored in plain Lua.

------------------------------------------------------------------------------------------------------------------
-- Globals / state
------------------------------------------------------------------------------------------------------------------

camera3d = nil

tWindowsTitle = {
    title_scene3d      = "scene_editor_options_title",
    title_mesh_selector = "mesh_selector_title",
}

sActiveTab            = 'map'  -- 'map' | 'mesh_set' | 'layer'
bEnableMainTabBar      = true
sFileNameScene3d       = ''

ImGuiPopupFlags_MouseButtonRight = 0

-- ---- Map-level state ----
tComboMapType3d = {'Free', 'Orthogonal grid', 'Isometric grid'}
tComboSnapScaleMode = {'none', 'full_fit', 'bigger_only'}
tComboSnapScaleModeLabel = {'No scaling', 'Scale to full fit', 'Scale when bigger than grid'}

tMapOptions = {
    sMapType         = 'Free',
    fGridCellWidthX  = 100,
    fGridCellDepthZ  = 100,
    sSnapScaleMode   = 'none',
}

-- ---- Layers ----
tLayers        = {}
iSelectedLayer = 0

-- ---- Placed mesh-instances ----
tPlacedMeshes = {}

-- ---- Map-tab "Object Option" markers (editor-only path/spawn-point data) ----
tSceneObjects      = {}
tSceneObjectShapes = {}
tComboObjectType3d = {'point', 'rectangle', 'circle', 'triangle', 'line'}

-- ---- Mesh Set (asset browser + thumbnail cache) ----
tMeshSetEntries      = {}
tThumbnailCache      = {}
sMeshSetFilterType   = 'All'
tComboMeshSetFilter  = {'All', 'mesh', 'sprite', 'particle', 'texture', 'tile', 'gif'}
sMeshSetFolder       = ''
sThumbnailCacheDir   = ''
iPreviewedMeshSetIndex = 0
tPreviewMesh3d       = nil
tMeshOffsets         = {}

tThumbnailGenQueue = {}
tThumbGenRt        = nil
tThumbGenActive    = nil

-- ---- Orbit camera (mesh_debug.lua pattern, one shared instance for the whole editor) ----
cam3d = { azimuth = 0.3, elevation = 0.3, distance = 800, fx = 0, fy = 0, fz = 0 }

-- ---- Mesh Selector / placement state ----
bShowMeshSelector         = true
sMeshSelectedForPlacement = nil
iSizeMeshOnSelector       = 90

-- ---- Hover / selection state ----
tHoveredPlaced = nil
tHoverBoxLine  = nil

-- ---- Origin lines ----
tOriginLine3dX, tOriginLine3dY, tOriginLine3dZ = nil, nil, nil
bEnableOriginLines = true

-- ---- Grid visual ----
tGridLines = {}

-- ---- Lighting ----
tLightConfig = {
    bEnabled          = false,
    ambientColor      = {r = 30, g = 30, b = 40, a = 255},
    directionalColor  = {r = 255, g = 250, b = 230, a = 255},
    directionalDir    = {x = 0, y = -1, z = 0.3},
    pointLights       = {},
}

-- ---- Run/menu options ----
tResolution = {
    {x = 800 , y = 600  , comment = 'XVGA'},
    {x = 1024, y = 768  , comment = 'XGA'},
    {x = 1280, y = 720  , comment = 'HD'},
    {x = 1366, y = 768  , comment = 'HD'},
    {x = 1920, y = 1080 , comment = 'Full HD'},
    {x = 2560, y = 1440 , comment = 'QHD'},
    {x = 3840, y = 2160 , comment = 'Ultra HD'},
}

tOptionsEditor = {
    iIndexResolution        = 3,
    bInvertResolution        = false,
    sCurrentScriptExecution  = '',
    fSceneCamPos             = {x = 0, y = 400, z = 900},
    fSceneCamFocus           = {x = 0, y = 0,   z = 0},
}
tOptionsLaunch = { iIndexResolution = 3, bInvertResolution = false }

-- ---- Async loading / progress ----
tLoadProgress = { bLoading = false, iLoaded = 0, iTotal = 0, sDetail = '' }

-- ---- Mouse tracking (used for orbit-camera drag deltas) ----
isClickedMouseleft  = false
isClickedMouseRight = false
mouseLastX, mouseLastY = 0, 0

------------------------------------------------------------------------------------------------------------------
-- Lighting
------------------------------------------------------------------------------------------------------------------

function currentTabWantsLightOn()
    return sActiveTab == 'map' and tLightConfig.bEnabled
end

-- Applies the correct lighting on/off state for whichever tab is currently active.
-- Map tab: whatever the user configured. Mesh Set / Layer tabs: always forced off.
function applyTabLighting()
    mbm.setLightEnabled('3d', currentTabWantsLightOn())
end

function applyLightConfigToEngine()
    mbm.setAmbientLight('3d', tLightConfig.ambientColor)
    mbm.setDirectionalLight('3d', tLightConfig.directionalDir, tLightConfig.directionalColor)
    mbm.clearPointLights('3d')
    for _, pl in ipairs(tLightConfig.pointLights) do
        mbm.addPointLight('3d', pl.x, pl.y, pl.z, pl.radius, pl.r, pl.g, pl.b, pl.a or 255)
    end
end

-- A mesh's shader variant is selected based on whether lighting is enabled at the moment
-- it is created/loaded -- a mesh created while lighting is off never gains the lit-shader
-- variant even if lighting is toggled on globally afterward. So every mesh-creation call
-- site must force lighting on for the duration of creation, then restore the correct state.
function createMeshWithLightingSupport(createFn)
    local wasEnabled = mbm.getLightState('3d').enabled
    if not wasEnabled then
        mbm.setLightEnabled('3d', true)
    end
    local tObj = createFn()
    applyTabLighting()
    return tObj
end

function drawLightPanel()
    tImGui.Text(tLang.L('light_panel'))
    local changedEnabled, enabled = tImGui.Checkbox('##light_enabled', tLightConfig.bEnabled)
    if changedEnabled then
        tLightConfig.bEnabled = enabled
        applyTabLighting()
    end
    tImGui.SameLine()
    tImGui.Text(tLang.L('light_enabled'))

    if not tLightConfig.bEnabled then
        return
    end

    tImGui.Text(tLang.L('light_ambient'))
    local changedAmbient, ambientColor = tImGui.ColorEdit4('##light_ambient', tLightConfig.ambientColor.r, tLightConfig.ambientColor.g, tLightConfig.ambientColor.b, tLightConfig.ambientColor.a)
    if changedAmbient then
        tLightConfig.ambientColor = ambientColor
        mbm.setAmbientLight('3d', ambientColor)
    end

    tImGui.Text(tLang.L('light_directional'))
    local changedDirColor, dirColor = tImGui.ColorEdit4('##light_directional_color', tLightConfig.directionalColor.r, tLightConfig.directionalColor.g, tLightConfig.directionalColor.b, tLightConfig.directionalColor.a)
    if changedDirColor then
        tLightConfig.directionalColor = dirColor
        mbm.setDirectionalLightColor('3d', dirColor)
    end
    local c1, dx = tImGui.InputFloat(tLang.L('axis_x') .. '##light_dir_x', tLightConfig.directionalDir.x, 0.1, 1, '%.2f')
    local c2, dy = tImGui.InputFloat(tLang.L('axis_y') .. '##light_dir_y', tLightConfig.directionalDir.y, 0.1, 1, '%.2f')
    local c3, dz = tImGui.InputFloat(tLang.L('axis_z') .. '##light_dir_z', tLightConfig.directionalDir.z, 0.1, 1, '%.2f')
    if c1 or c2 or c3 then
        tLightConfig.directionalDir = {x = dx, y = dy, z = dz}
        mbm.setDirectionalLightDirection('3d', tLightConfig.directionalDir)
    end

    tImGui.Separator()
    tImGui.Text(tLang.L('light_point_lights_fmt'):format(#tLightConfig.pointLights))
    if tImGui.Button(tLang.L('add_point_light')) then
        table.insert(tLightConfig.pointLights, {x = 0, y = 200, z = 0, radius = 400, r = 255, g = 255, b = 255, a = 255})
        applyLightConfigToEngine()
    end
    for i, pl in ipairs(tLightConfig.pointLights) do
        tImGui.PushItemWidth(120)
        local p1, px = tImGui.InputFloat(tLang.L('axis_x') .. '##pl_x' .. i, pl.x, 1, 10, '%.2f')
        tImGui.SameLine()
        local p2, py = tImGui.InputFloat(tLang.L('axis_y') .. '##pl_y' .. i, pl.y, 1, 10, '%.2f')
        tImGui.SameLine()
        local p3, pz = tImGui.InputFloat(tLang.L('axis_z') .. '##pl_z' .. i, pl.z, 1, 10, '%.2f')
        tImGui.PopItemWidth()
        local p4, pr = tImGui.InputFloat(tLang.L('light_radius') .. '##pl_r' .. i, pl.radius, 1, 10, '%.2f')
        local changedColor, plColor = tImGui.ColorEdit4('##pl_color' .. i, pl.r, pl.g, pl.b, pl.a)
        if p1 or p2 or p3 or p4 or changedColor then
            pl.x, pl.y, pl.z, pl.radius = px, py, pz, pr
            if changedColor then pl.r, pl.g, pl.b, pl.a = plColor.r, plColor.g, plColor.b, plColor.a end
            applyLightConfigToEngine()
        end
        if tImGui.Button(tLang.L('delete') .. '##pl_del' .. i) then
            table.remove(tLightConfig.pointLights, i)
            applyLightConfigToEngine()
            break
        end
    end
end

------------------------------------------------------------------------------------------------------------------
-- Orbit camera (mesh_debug.lua pattern, re-implemented locally since that file's helpers are not exported)
------------------------------------------------------------------------------------------------------------------

function cam3dGetPos(c)
    local x = c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth)
    local y = c.fy + c.distance * math.sin(c.elevation)
    local z = c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
    return x, y, z
end

function applyCam3d(c)
    local x, y, z = cam3dGetPos(c)
    camera3d:setPos(x, y, z)
    camera3d:setFocus(c.fx, c.fy, c.fz)
end

------------------------------------------------------------------------------------------------------------------
-- Grid math
------------------------------------------------------------------------------------------------------------------

function gridCellToWorld(cx, cz, layer)
    local x = cx * tMapOptions.fGridCellWidthX + layer.offset.x
    local z = cz * tMapOptions.fGridCellDepthZ + layer.offset.z
    return x, layer.fY, z
end

function worldToGridCell(wx, wz, layer)
    local cx = math.floor((wx - layer.offset.x) / tMapOptions.fGridCellWidthX + 0.5)
    local cz = math.floor((wz - layer.offset.z) / tMapOptions.fGridCellDepthZ + 0.5)
    return cx, cz
end

function snapRotation(angleRad)
    if tMapOptions.sMapType == 'Orthogonal' then
        local step = math.pi * 0.5
        return math.floor(angleRad / step + 0.5) * step
    elseif tMapOptions.sMapType == 'Isometric' then
        local step = math.pi * 0.5
        return math.floor((angleRad - math.pi * 0.25) / step + 0.5) * step + math.pi * 0.25
    end
    return angleRad
end

-- Mouse -> world position on a fixed Y-plane, via ray reconstruction from mbm.to3d at two depths.
-- NOTE: verify empirically that mbm.to3d(sx,sy,depth)'s `depth` behaves as a literal distance
-- along the view direction; if not, this needs to be replaced with a camera-matrix unprojection.
function screenToWorldOnLayerPlane(sx, sy, planeY)
    local x1, y1, z1 = mbm.to3d(sx, sy, 100)
    local x2, y2, z2 = mbm.to3d(sx, sy, 1000)
    local dx, dy, dz = x2 - x1, y2 - y1, z2 - z1
    if math.abs(dy) < 1e-6 then
        return nil
    end
    local t = (planeY - y1) / dy
    return x1 + dx * t, planeY, z1 + dz * t
end

-- Computes the scale factor to apply to a placed mesh according to the active snap/scale mode.
function computeSnapScale(tObj)
    if tMapOptions.sSnapScaleMode == 'none' or tMapOptions.sMapType == 'Free' then
        return 1, 1, 1
    end
    local w, h, d = tObj:getAABB(true)
    w, h, d = w or 1, h or 1, d or 1
    local targetW, targetD = tMapOptions.fGridCellWidthX, tMapOptions.fGridCellDepthZ
    local scaleX = targetW / math.max(w, 1e-4)
    local scaleZ = targetD / math.max(d, 1e-4)
    local scale = math.min(scaleX, scaleZ)
    if tMapOptions.sSnapScaleMode == 'bigger_only' then
        if w <= targetW and d <= targetD then
            return 1, 1, 1
        end
    end
    return scale, scale, scale
end

function rebuildGridVisual()
    for _, ln in ipairs(tGridLines) do
        ln:destroy()
    end
    tGridLines = {}
    if tMapOptions.sMapType == 'Free' or iSelectedLayer == 0 then
        return
    end
    local layer = tLayers[iSelectedLayer]
    if not layer then return end
    local halfLinesX, halfLinesZ = 10, 10
    local w, d = tMapOptions.fGridCellWidthX, tMapOptions.fGridCellDepthZ
    local isIso = tMapOptions.sMapType == 'Isometric'
    local rot = isIso and (math.pi * 0.25) or 0
    local cosr, sinr = math.cos(rot), math.sin(rot)

    local function rotXZ(x, z)
        return x * cosr - z * sinr, x * sinr + z * cosr
    end

    for i = -halfLinesX, halfLinesX do
        local x = i * w
        local ln = line:new('3d', layer.offset.x, layer.fY, layer.offset.z)
        local x1, z1 = rotXZ(x, -halfLinesZ * d)
        local x2, z2 = rotXZ(x,  halfLinesZ * d)
        ln:add({x1, 0, z1, x2, 0, z2})
        ln:setColor(90, 90, 90, 160)
        table.insert(tGridLines, ln)
    end
    for i = -halfLinesZ, halfLinesZ do
        local z = i * d
        local ln = line:new('3d', layer.offset.x, layer.fY, layer.offset.z)
        local x1, z1 = rotXZ(-halfLinesX * w, z)
        local x2, z2 = rotXZ( halfLinesX * w, z)
        ln:add({x1, 0, z1, x2, 0, z2})
        ln:setColor(90, 90, 90, 160)
        table.insert(tGridLines, ln)
    end
end

------------------------------------------------------------------------------------------------------------------
-- Async mesh loading & progress
------------------------------------------------------------------------------------------------------------------

-- Loads a renderizable asset asynchronously when it is a heavy `mesh`, synchronously otherwise.
-- Always resolves through onDone(tObj), and always tracked by tLoadProgress for the progress modal.
function placeMeshAsync(fileName, sType, coordType, onDone)
    if sType == 'mesh' then
        local m = mesh:new(coordType)
        local wasEnabled = mbm.getLightState('3d').enabled
        if not wasEnabled then
            mbm.setLightEnabled('3d', true)
        end
        m:loadAsync(fileName, function(self_mesh, success)
            applyTabLighting()
            tLoadProgress.iLoaded = tLoadProgress.iLoaded + 1
            onDone(success and self_mesh or nil)
        end)
    else
        local tObj = createMeshWithLightingSupport(function()
            return tUtil.onAddMeshToEditor(fileName, false, coordType)
        end)
        tLoadProgress.iLoaded = tLoadProgress.iLoaded + 1
        onDone(tObj)
    end
end

function beginLoadProgress(total, detail)
    tLoadProgress.bLoading = true
    tLoadProgress.iLoaded  = 0
    tLoadProgress.iTotal   = total
    tLoadProgress.sDetail  = detail or ''
end

function showLoadProgressModal()
    if not tLoadProgress.bLoading then
        return
    end
    tImGui.OpenPopup('scene3d_load_modal')
    local isOpen = tImGui.BeginPopupModal(tLang.L('loading_scene') .. '###scene3d_load_modal', false, 0)
    if isOpen then
        local fraction = tLoadProgress.iTotal > 0 and (tLoadProgress.iLoaded / tLoadProgress.iTotal) or 0
        tImGui.Text(string.format('%d / %d', tLoadProgress.iLoaded, tLoadProgress.iTotal))
        if tLoadProgress.sDetail ~= '' then
            tImGui.TextDisabled(tLoadProgress.sDetail)
        end
        tImGui.ProgressBar(fraction)
        if tLoadProgress.iLoaded >= tLoadProgress.iTotal then
            tLoadProgress.bLoading = false
            tImGui.CloseCurrentPopup()
        end
        tImGui.EndPopup()
    end
end

------------------------------------------------------------------------------------------------------------------
-- Thumbnail caching (render2texture bake-once-to-PNG)
------------------------------------------------------------------------------------------------------------------

function getOrCreateThumbnail(entry)
    if tThumbnailCache[entry.fileName] then
        return tThumbnailCache[entry.fileName]
    end
    if entry.thumbState == nil then
        entry.thumbState = 'queued'
        table.insert(tThumbnailGenQueue, entry)
    end
    return nil
end

function processThumbnailQueue()
    if tThumbGenActive then
        return
    end
    local entry = table.remove(tThumbnailGenQueue, 1)
    if not entry then
        return
    end
    tThumbGenRt = tThumbGenRt or render2texture:new('3d')
    local ok = tThumbGenRt:create(160, 160, true, 'thumb_gen_' .. tostring(entry.fileName))
    if not ok then
        entry.thumbState = 'failed'
        return
    end
    local wasEnabled = mbm.getLightState('3d').enabled
    if not wasEnabled then mbm.setLightEnabled('3d', true) end
    local tObj = tUtil.onAddMeshToEditor(entry.fileName, false, '3d')
    applyTabLighting()
    if not tObj then
        entry.thumbState = 'failed'
        return
    end
    tThumbGenRt:add(tObj)
    local w, h, d = tObj:getAABB(true)
    local fitDist = math.max(w or 50, h or 50, d or 50) * 2.2
    local rtCam = tThumbGenRt:getCamera('3d')
    rtCam:setPos(fitDist * 0.7, fitDist * 0.5, fitDist * 0.7)
    rtCam:setFocus(0, 0, 0)
    entry.thumbState = 'generating'
    tThumbGenActive = { entry = entry, tObj = tObj, framesWaited = 0 }
end

function tickThumbnailBake()
    if not tThumbGenActive then
        return
    end
    tThumbGenActive.framesWaited = tThumbGenActive.framesWaited + 1
    if tThumbGenActive.framesWaited < 3 then
        return
    end
    local pngPath = sThumbnailCacheDir .. tUtil.getShortName(tThumbGenActive.entry.fileName) .. '.png'
    tThumbGenRt:save(pngPath, 0, 0, 160, 160)
    tThumbGenRt:remove(tThumbGenActive.tObj)
    tThumbGenActive.tObj:destroy()
    local texInfo = mbm.loadTexture(pngPath)
    tThumbnailCache[tThumbGenActive.entry.fileName] = texInfo
    tThumbGenActive.entry.thumbState = 'ready'
    tThumbGenActive = nil
end

-- Lists every recognizable renderizable asset in `dir` (any type) using meshDebug:getInfo as the
-- authoritative type detector (mirrors tUtil.onAddMeshToEditor's own type-detection call).
function scanMeshSetFolder(dir)
    tMeshSetEntries = {}
    if not dir or dir == '' then
        return
    end
    local sep = package.config:sub(1, 1)
    local cmd
    if sep == '\\' then
        cmd = string.format('dir "%s" /b', dir)
    else
        cmd = string.format('ls -1 "%s"', dir)
    end
    local fp = io.popen(cmd)
    if not fp then
        return
    end
    for line in fp:lines() do
        if line and #line > 0 then
            local fullPath = dir .. sep .. line
            local tInfo = meshDebug and meshDebug:getInfo(fullPath) or nil
            if tInfo and tInfo.type then
                table.insert(tMeshSetEntries, {
                    fileName   = fullPath,
                    type       = tInfo.type,
                    ext        = tInfo.ext,
                    isSelected = false,
                    thumbState = nil,
                })
            end
        end
    end
    fp:close()
end

function getFilteredMeshSetEntries()
    if sMeshSetFilterType == 'All' then
        return tMeshSetEntries
    end
    local tFiltered = {}
    for _, entry in ipairs(tMeshSetEntries) do
        if entry.type == sMeshSetFilterType then
            table.insert(tFiltered, entry)
        end
    end
    return tFiltered
end

------------------------------------------------------------------------------------------------------------------
-- Hover-highlight (Layer tab)
------------------------------------------------------------------------------------------------------------------

function buildWireBoxLine(x, y, z, w, h, d)
    local hw, hh, hd = (w or 10) * 0.5, (h or 10) * 0.5, (d or 10) * 0.5
    local ln = line:new('3d', x, y, z)
    ln:setColor(255, 255, 0, 255)
    local corners = {
        {-hw, -hh, -hd}, {hw, -hh, -hd}, {hw, -hh, hd}, {-hw, -hh, hd},
        {-hw,  hh, -hd}, {hw,  hh, -hd}, {hw,  hh, hd}, {-hw,  hh, hd},
    }
    local edges = {
        {1,2},{2,3},{3,4},{4,1},
        {5,6},{6,7},{7,8},{8,5},
        {1,5},{2,6},{3,7},{4,8},
    }
    local pts = {}
    for _, e in ipairs(edges) do
        local a, b = corners[e[1]], corners[e[2]]
        table.insert(pts, a[1]); table.insert(pts, a[2]); table.insert(pts, a[3])
        table.insert(pts, b[1]); table.insert(pts, b[2]); table.insert(pts, b[3])
    end
    ln:add(pts)
    return ln
end

function updateHoverHighlight(sx, sy)
    if sActiveTab ~= 'layer' then
        if tHoverBoxLine then tHoverBoxLine.visible = false end
        return
    end
    local bestIndex, bestDist = nil, math.huge
    for i, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.tObj and tPlaced.tObj:collide(sx, sy) then
            local camPos = camera3d:getPos()
            local dx = tPlaced.tObj.x - camPos.x
            local dy = tPlaced.tObj.y - camPos.y
            local dz = tPlaced.tObj.z - camPos.z
            local dist = dx * dx + dy * dy + dz * dz
            if dist < bestDist then
                bestDist, bestIndex = dist, i
            end
        end
    end
    if bestIndex ~= tHoveredPlaced then
        if tHoverBoxLine then
            tHoverBoxLine:destroy()
            tHoverBoxLine = nil
        end
        tHoveredPlaced = bestIndex
        if tHoveredPlaced then
            local tObj = tPlacedMeshes[tHoveredPlaced].tObj
            local w, h, d = tObj:getAABB(true)
            tHoverBoxLine = buildWireBoxLine(tObj.x, tObj.y, tObj.z, w, h, d)
        end
    elseif tHoveredPlaced then
        local tObj = tPlacedMeshes[tHoveredPlaced].tObj
        tHoverBoxLine:setPos(tObj.x, tObj.y, tObj.z)
    end
end

------------------------------------------------------------------------------------------------------------------
-- Placed mesh management
------------------------------------------------------------------------------------------------------------------

function resolvePlacedMeshWorldPos(tPlaced)
    local layer = tLayers[tPlaced.layerIndex]
    if tMapOptions.sMapType == 'Free' then
        local y = tPlaced.freeY
        if tPlaced.bAttachedToLayer and layer then
            y = layer.fY
        end
        return tPlaced.freeX, y, tPlaced.freeZ
    else
        local x, y, z = gridCellToWorld(tPlaced.cellX, tPlaced.cellZ, layer)
        return x, y, z
    end
end

function syncPlacedMeshTransform(tPlaced)
    if not tPlaced.tObj then return end
    local x, y, z = resolvePlacedMeshWorldPos(tPlaced)
    local offset = tMeshOffsets[tPlaced.fileName] or {x = 0, y = 0, z = 0}
    tPlaced.tObj:setPos(x + offset.x, y + offset.y, z + offset.z)
    tPlaced.tObj:setAngle(0, tPlaced.rotationY or 0, 0)
    tPlaced.tObj:setScale(tPlaced.scale.x, tPlaced.scale.y, tPlaced.scale.z)
end

function addPlacedMesh(fileName, sType, layerIndex, cellX, cellZ, freeX, freeZ)
    local tPlaced = {
        fileName = fileName, type = sType, layerIndex = layerIndex,
        cellX = cellX or 0, cellZ = cellZ or 0,
        freeX = freeX or 0, freeZ = freeZ or 0,
        bAttachedToLayer = true, freeY = 0,
        rotationY = 0, scale = {x = 1, y = 1, z = 1},
        isSelected = false, tObj = nil,
    }
    table.insert(tPlacedMeshes, tPlaced)
    beginLoadProgress(tLoadProgress.iTotal + 1, tUtil.getShortName(fileName))
    placeMeshAsync(fileName, sType, '3d', function(tObj)
        if tObj then
            tPlaced.tObj = tObj
            local sx, sy, sz = computeSnapScale(tObj)
            tPlaced.scale = {x = sx, y = sy, z = sz}
            syncPlacedMeshTransform(tPlaced)
        end
    end)
    return tPlaced
end

function removePlacedMesh(index)
    local tPlaced = tPlacedMeshes[index]
    if tPlaced and tPlaced.tObj then
        tPlaced.tObj:destroy()
    end
    table.remove(tPlacedMeshes, index)
end

------------------------------------------------------------------------------------------------------------------
-- Map-tab "Object Option" markers
------------------------------------------------------------------------------------------------------------------

function addSceneObjectMarker()
    local tObj = { type = 'point', name = 'no_name', x = 0, y = 0, z = 0 }
    table.insert(tSceneObjects, tObj)
    table.insert(tSceneObjectShapes, nil)
    updateSceneObjectShapes()
end

function updateSceneObjectShapes()
    for i, tObj in ipairs(tSceneObjects) do
        local ln = tSceneObjectShapes[i]
        if not ln then
            ln = line:new('3d', tObj.x, tObj.y, tObj.z)
            ln:setColor(0, 255, 255, 255)
            tSceneObjectShapes[i] = ln
        end
        ln:setPos(tObj.x, tObj.y, tObj.z)
        ln.visible = (sActiveTab == 'map')
    end
end

function drawObjectMarkerFields(tObj)
    local cName, sName = tImGui.InputText('##marker_name', tObj.name or 'no_name', 64)
    if cName then tObj.name = sName end
    local c1, x = tImGui.InputFloat(tLang.L('axis_x') .. '##marker_x', tObj.x, 1, 10, '%.2f')
    local c2, y = tImGui.InputFloat(tLang.L('axis_y') .. '##marker_y', tObj.y, 1, 10, '%.2f')
    local c3, z = tImGui.InputFloat(tLang.L('axis_z') .. '##marker_z', tObj.z, 1, 10, '%.2f')
    if c1 or c2 or c3 then
        tObj.x, tObj.y, tObj.z = x, y, z
    end
    if tObj.type == 'rectangle' then
        local cw, w = tImGui.InputFloat(tLang.L('width') .. '##marker_w', tObj.width or 100, 1, 10, '%.2f')
        local ch, h = tImGui.InputFloat(tLang.L('height') .. '##marker_h', tObj.height or 100, 1, 10, '%.2f')
        if cw then tObj.width = w end
        if ch then tObj.height = h end
    elseif tObj.type == 'circle' then
        local cr, r = tImGui.InputFloat(tLang.L('ray') .. '##marker_ray', tObj.ray or 50, 1, 10, '%.2f')
        if cr then tObj.ray = r end
    elseif tObj.type == 'line' then
        tObj.points = tObj.points or {}
        if tImGui.Button(tLang.L('add_point') .. '##marker_addpt') then
            table.insert(tObj.points, {x = tObj.x, y = tObj.y, z = tObj.z})
        end
        for i, p in ipairs(tObj.points) do
            local pc1, px = tImGui.InputFloat(tLang.L('axis_x') .. '##pt_x' .. i, p.x, 1, 10, '%.2f')
            local pc2, py = tImGui.InputFloat(tLang.L('axis_y') .. '##pt_y' .. i, p.y, 1, 10, '%.2f')
            local pc3, pz = tImGui.InputFloat(tLang.L('axis_z') .. '##pt_z' .. i, p.z, 1, 10, '%.2f')
            if pc1 or pc2 or pc3 then p.x, p.y, p.z = px, py, pz end
        end
    end
end

------------------------------------------------------------------------------------------------------------------
-- Map tab
------------------------------------------------------------------------------------------------------------------

function drawMapTab(item_width)
    local ret, current_item = tImGui.Combo(tLang.L('map_type') .. '##MapType3d', tComboMapTypeIndexOf(tMapOptions.sMapType), tComboMapType3d)
    if ret then
        local sNew = ({'Free', 'Orthogonal', 'Isometric'})[current_item + 1]
        tMapOptions.sMapType = sNew
        rebuildGridVisual()
    end

    if tMapOptions.sMapType ~= 'Free' then
        local c1, w = tImGui.InputFloat(tLang.L('grid_width_x') .. '##GridWidthX', tMapOptions.fGridCellWidthX, 1, 10, '%.2f')
        local c2, d = tImGui.InputFloat(tLang.L('grid_depth_z') .. '##GridDepthZ', tMapOptions.fGridCellDepthZ, 1, 10, '%.2f')
        if c1 then tMapOptions.fGridCellWidthX = w end
        if c2 then tMapOptions.fGridCellDepthZ = d end
        if c1 or c2 then rebuildGridVisual() end

        local retSnap, curSnap = tImGui.Combo(tLang.L('snap_scale_mode') .. '##SnapScaleMode', tComboSnapScaleModeIndexOf(tMapOptions.sSnapScaleMode), tComboSnapScaleModeLabel)
        if retSnap then
            tMapOptions.sSnapScaleMode = tComboSnapScaleMode[curSnap + 1]
        end
    end

    tImGui.Separator()
    drawLightPanel()

    tImGui.Separator()
    tImGui.Text(tLang.L('object_options'))
    if tImGui.Button(tLang.L('add_object'), tUtil.getResponsiveItemSize(item_width - 40)) then
        addSceneObjectMarker()
    end
    for i, tObj in ipairs(tSceneObjects) do
        local isOpen = tImGui.TreeNodeEx(string.format('%s-%d##marker_tree', tLang.L('object'), i))
        if isOpen then
            local ret2, cur2 = tImGui.Combo('##marker_type' .. i, tComboObjectTypeIndexOf(tObj.type), tComboObjectType3d)
            if ret2 then
                tObj.type = tComboObjectType3d[cur2 + 1]
            end
            drawObjectMarkerFields(tObj)
            if tImGui.Button(tLang.L('delete') .. '##marker_del' .. i) then
                if tSceneObjectShapes[i] then tSceneObjectShapes[i]:destroy() end
                table.remove(tSceneObjects, i)
                table.remove(tSceneObjectShapes, i)
                tImGui.TreePop()
                break
            end
            tImGui.TreePop()
        end
    end
    updateSceneObjectShapes()
end

function tComboMapTypeIndexOf(sType)
    for i, s in ipairs({'Free', 'Orthogonal', 'Isometric'}) do
        if s == sType then return i - 1 end
    end
    return 0
end

function tComboSnapScaleModeIndexOf(sMode)
    for i, s in ipairs(tComboSnapScaleMode) do
        if s == sMode then return i - 1 end
    end
    return 0
end

function tComboObjectTypeIndexOf(sType)
    for i, s in ipairs(tComboObjectType3d) do
        if s == sType then return i - 1 end
    end
    return 0
end

------------------------------------------------------------------------------------------------------------------
-- Mesh Set tab
------------------------------------------------------------------------------------------------------------------

function destroyPreviewMesh3d()
    if tPreviewMesh3d then
        tPreviewMesh3d:destroy()
        tPreviewMesh3d = nil
    end
end

function updatePreviewMesh3d(entry)
    destroyPreviewMesh3d()
    if not entry then return end
    tPreviewMesh3d = createMeshWithLightingSupport(function()
        return tUtil.onAddMeshToEditor(entry.fileName, false, '3d')
    end)
    if tPreviewMesh3d then
        tPreviewMesh3d:setPos(0, 0, 0)
        local w, h, d = tPreviewMesh3d:getAABB(true)
        cam3d.fx, cam3d.fy, cam3d.fz = 0, 0, 0
        cam3d.distance = math.max(w or 100, h or 100, d or 100) * 2.5
    end
end

function drawMeshSetTab(item_width)
    tImGui.Text(tLang.L('mesh_set_folder'))
    tImGui.TextWrapped(sMeshSetFolder ~= '' and sMeshSetFolder or tLang.L('no_folder_selected'))
    if tImGui.Button(tLang.L('choose_folder'), tUtil.getResponsiveItemSize(item_width - 40)) then
        local chosen = mbm.openFolder(tLang.L('choose_folder'), sMeshSetFolder)
        if chosen then
            sMeshSetFolder = chosen
            scanMeshSetFolder(sMeshSetFolder)
        end
    end

    local retF, curF = tImGui.Combo(tLang.L('filter_type') .. '##MeshSetFilter', tComboMeshSetFilterIndexOf(sMeshSetFilterType), tComboMeshSetFilter)
    if retF then
        sMeshSetFilterType = tComboMeshSetFilter[curF + 1]
    end

    if iPreviewedMeshSetIndex > 0 then
        local entry = getFilteredMeshSetEntries()[iPreviewedMeshSetIndex]
        if entry then
            tImGui.Separator()
            tImGui.Text(tLang.L('mesh_offset_fmt'):format(tUtil.getShortName(entry.fileName)))
            local offset = tMeshOffsets[entry.fileName] or {x = 0, y = 0, z = 0}
            local o1, ox = tImGui.InputFloat(tLang.L('axis_x') .. '##offset_x', offset.x, 1, 10, '%.2f')
            local o2, oy = tImGui.InputFloat(tLang.L('axis_y') .. '##offset_y', offset.y, 1, 10, '%.2f')
            local o3, oz = tImGui.InputFloat(tLang.L('axis_z') .. '##offset_z', offset.z, 1, 10, '%.2f')
            if o1 or o2 or o3 then
                tMeshOffsets[entry.fileName] = {x = ox, y = oy, z = oz}
            end
        end
    end

    tImGui.Separator()
    local tFiltered = getFilteredMeshSetEntries()
    local winSize = {tImGui.GetWindowSize()}
    local xLast = tImGui.GetCursorPosX()
    for n, entry in ipairs(tFiltered) do
        local texInfo = getOrCreateThumbnail(entry)
        local size = {x = 90, y = 90}
        if texInfo then
            local selected = (iPreviewedMeshSetIndex == n)
            if selected then
                tImGui.PushStyleColor('ImGuiCol_Button', {r = 0, g = 1, b = 0, a = 1})
            end
            if tImGui.ImageButton('mesh_set_btn_' .. n, texInfo.id, size, {x = 0, y = 0}, {x = 1, y = 1}) then
                iPreviewedMeshSetIndex = n
                updatePreviewMesh3d(entry)
            end
            if selected then
                tImGui.PopStyleColor(1)
            end
        else
            tImGui.Button(tLang.L('generating') .. '##pending' .. n, size)
        end
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text(tUtil.getShortName(entry.fileName))
            tImGui.EndTooltip()
        end
        xLast = xLast + size.x + 10
        if xLast < (winSize[1] or 400) - size.x then
            tImGui.SameLine()
        else
            xLast = tImGui.GetCursorPosX()
        end
    end
end

function tComboMeshSetFilterIndexOf(sType)
    for i, s in ipairs(tComboMeshSetFilter) do
        if s == sType then return i - 1 end
    end
    return 0
end

------------------------------------------------------------------------------------------------------------------
-- Layer tab
------------------------------------------------------------------------------------------------------------------

function addLayer()
    table.insert(tLayers, { name = 'Layer-' .. (#tLayers + 1), visible = true, fY = 0, offset = {x = 0, z = 0} })
    iSelectedLayer = #tLayers
end

function drawLayerTab(item_width)
    if tImGui.Button(tLang.L('add_layer'), tUtil.getResponsiveItemSize(item_width - 40)) then
        addLayer()
    end

    for i, layer in ipairs(tLayers) do
        local isOpen = tImGui.TreeNodeEx(layer.name .. '##layer_tree' .. i)
        if i == iSelectedLayer then
            tImGui.SameLine()
            tImGui.TextDisabled(tLang.L('active'))
        end
        if isOpen then
            if tImGui.Button(tLang.L('select') .. '##select_layer' .. i) then
                iSelectedLayer = i
                rebuildGridVisual()
            end

            local cVis, vis = tImGui.Checkbox(tLang.L('visible') .. '##layer_visible' .. i, layer.visible)
            if cVis then
                layer.visible = vis
                for _, tPlaced in ipairs(tPlacedMeshes) do
                    if tPlaced.layerIndex == i and tPlaced.tObj then
                        tPlaced.tObj.visible = vis
                    end
                end
            end

            local cY, y = tImGui.InputFloat(tLang.L('offset_y') .. '##layer_offset_y' .. i, layer.fY, 1, 10, '%.2f')
            local cOX, ox = tImGui.InputFloat(tLang.L('offset_x') .. '##layer_offset_x' .. i, layer.offset.x, 1, 10, '%.2f')
            local cOZ, oz = tImGui.InputFloat(tLang.L('offset_z') .. '##layer_offset_z' .. i, layer.offset.z, 1, 10, '%.2f')
            if cY or cOX or cOZ then
                if cY then layer.fY = y end
                if cOX then layer.offset.x = ox end
                if cOZ then layer.offset.z = oz end
                for _, tPlaced in ipairs(tPlacedMeshes) do
                    if tPlaced.layerIndex == i then
                        syncPlacedMeshTransform(tPlaced)
                    end
                end
                rebuildGridVisual()
            end

            if tImGui.Button(tLang.L('delete_layer') .. '##layer_del' .. i) then
                for j = #tPlacedMeshes, 1, -1 do
                    if tPlacedMeshes[j].layerIndex == i then
                        removePlacedMesh(j)
                    end
                end
                table.remove(tLayers, i)
                if iSelectedLayer > #tLayers then iSelectedLayer = #tLayers end
                tImGui.TreePop()
                break
            end
            tImGui.TreePop()
        end
    end

    tImGui.Separator()
    local cShow, show = tImGui.Checkbox(tLang.L('mesh_selector_separated'), bShowMeshSelector)
    if cShow then bShowMeshSelector = show end

    tImGui.Separator()
    tImGui.Text(tLang.L('placed_meshes_fmt'):format(#tPlacedMeshes))
    for i, tPlaced in ipairs(tPlacedMeshes) do
        local rowOpen = tImGui.TreeNodeEx(tUtil.getShortName(tPlaced.fileName) .. '##placed_tree' .. i)
        if rowOpen then
            local cSel, sel = tImGui.Checkbox(tLang.L('selected') .. '##placed_sel' .. i, tPlaced.isSelected)
            if cSel then tPlaced.isSelected = sel end

            if tMapOptions.sMapType == 'Free' then
                local cAttach, attach = tImGui.Checkbox(tLang.L('attached_to_layer') .. '##placed_attach' .. i, tPlaced.bAttachedToLayer)
                if cAttach then
                    tPlaced.bAttachedToLayer = attach
                    syncPlacedMeshTransform(tPlaced)
                end
                if not tPlaced.bAttachedToLayer then
                    local cFY, fy = tImGui.InputFloat(tLang.L('axis_y') .. '##placed_freey' .. i, tPlaced.freeY, 1, 10, '%.2f')
                    if cFY then
                        tPlaced.freeY = fy
                        syncPlacedMeshTransform(tPlaced)
                    end
                end
            end

            if tImGui.Button(tLang.L('delete') .. '##placed_del' .. i) then
                removePlacedMesh(i)
                tImGui.TreePop()
                break
            end
            tImGui.TreePop()
        end
    end
end

function drawMeshSelector(xStart)
    tUtil.setInitialWindowPositionDown(tLang.L(tWindowsTitle.title_mesh_selector), xStart + 25, 0.3, 180)
    local isOpen, closedClicked = tImGui.Begin(tLang.L(tWindowsTitle.title_mesh_selector), true, 0)
    if isOpen then
        local tFiltered = getFilteredMeshSetEntries()
        local winSize = {tImGui.GetWindowSize()}
        local xLast = tImGui.GetCursorPosX()
        for n, entry in ipairs(tFiltered) do
            local texInfo = getOrCreateThumbnail(entry)
            local size = {x = iSizeMeshOnSelector, y = iSizeMeshOnSelector}
            if texInfo then
                local selected = (sMeshSelectedForPlacement == entry.fileName)
                if selected then
                    tImGui.PushStyleColor('ImGuiCol_Button', {r = 0, g = 1, b = 0, a = 1})
                end
                if tImGui.ImageButton('mesh_selector_btn_' .. n, texInfo.id, size, {x = 0, y = 0}, {x = 1, y = 1}) then
                    sMeshSelectedForPlacement = entry.fileName
                end
                if selected then
                    tImGui.PopStyleColor(1)
                end
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tUtil.getShortName(entry.fileName))
                    tImGui.EndTooltip()
                end
            else
                tImGui.Button(tLang.L('generating') .. '##selpending' .. n, size)
            end
            xLast = xLast + size.x + 10
            if xLast < (winSize[1] or 400) - size.x then
                tImGui.SameLine()
            else
                xLast = tImGui.GetCursorPosX()
            end
        end
        menuPopUpOptionToAddMesh()
    end
    tImGui.End()
    return closedClicked
end

function fillActiveLayerWithMesh(fileName)
    if tMapOptions.sMapType == 'Free' or iSelectedLayer == 0 then
        return
    end
    local entry = nil
    for _, e in ipairs(tMeshSetEntries) do
        if e.fileName == fileName then entry = e; break end
    end
    if not entry then return end
    local iRange = 5
    for cx = -iRange, iRange do
        for cz = -iRange, iRange do
            addPlacedMesh(fileName, entry.type, iSelectedLayer, cx, cz)
        end
    end
end

function menuPopUpOptionToAddMesh()
    if sMeshSelectedForPlacement then
        if tImGui.BeginPopupContextVoid('##fill_layer_menu', ImGuiPopupFlags_MouseButtonRight) then
            if tMapOptions.sMapType ~= 'Free' then
                if tImGui.Selectable(tLang.L('fill_layer_with_selected_mesh')) then
                    fillActiveLayerWithMesh(sMeshSelectedForPlacement)
                end
            else
                tImGui.TextDisabled(tLang.L('fill_layer_unavailable_free'))
            end
            tImGui.EndPopup()
        end
    end
end

function menuPopUpRotateMesh(tPlaced)
    if tImGui.BeginPopupContextItem('##rotate_menu_' .. tostring(tPlaced)) then
        if tMapOptions.sMapType == 'Orthogonal' then
            for _, deg in ipairs({0, 90, 180, 270}) do
                if tImGui.Selectable(string.format(tLang.L('rotate_to_fmt'), deg)) then
                    tPlaced.rotationY = math.rad(deg)
                    syncPlacedMeshTransform(tPlaced)
                end
            end
        elseif tMapOptions.sMapType == 'Isometric' then
            for _, deg in ipairs({45, 135, 225, 315}) do
                if tImGui.Selectable(string.format(tLang.L('rotate_to_fmt'), deg)) then
                    tPlaced.rotationY = math.rad(deg)
                    syncPlacedMeshTransform(tPlaced)
                end
            end
        else
            local result, fDeg = tImGui.SliderFloat(tLang.L('rotation'), math.deg(tPlaced.rotationY or 0), 0, 360, '%.1f')
            if result then
                tPlaced.rotationY = math.rad(fDeg)
                syncPlacedMeshTransform(tPlaced)
            end
        end
        tImGui.EndPopup()
    end
end

------------------------------------------------------------------------------------------------------------------
-- Main windows
------------------------------------------------------------------------------------------------------------------

function main_tab_bar()
    if not bEnableMainTabBar then return end
    tUtil.setInitialWindowPositionLeft(tLang.L(tWindowsTitle.title_scene3d), 0, 0, 260, 260)
    local isOpen = tImGui.Begin(tLang.L(tWindowsTitle.title_scene3d), true, 0)
    if isOpen then
        local item_width = tUtil.getResponsiveItemWidth(200)
        if tImGui.BeginTabBar('##TabBar3d', 0) then
            if tImGui.BeginTabItem(tLang.L('tab_map')) then
                sActiveTab = 'map'
                drawMapTab(item_width)
                tImGui.EndTabItem()
            end
            if tImGui.BeginTabItem(tLang.L('tab_mesh_set')) then
                sActiveTab = 'mesh_set'
                drawMeshSetTab(item_width)
                tImGui.EndTabItem()
            end
            if tImGui.BeginTabItem(tLang.L('tab_layer')) then
                sActiveTab = 'layer'
                drawLayerTab(item_width)
                tImGui.EndTabItem()
            end
            tImGui.EndTabBar()
        end
    end
    tImGui.End()

    applyTabLighting()

    if sActiveTab == 'layer' and bShowMeshSelector then
        drawMeshSelector(260)
    end
end

------------------------------------------------------------------------------------------------------------------
-- Menu bar
------------------------------------------------------------------------------------------------------------------

function main_menu_3d()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu(tLang.L('menu_file')) then
            if tImGui.MenuItem(tLang.L('load_3d_scene'), 'Ctrl+O') then
                onOpenScene3d()
            end
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L('save_3d_scene'), 'Ctrl+S') then
                onSaveScene3d()
            end
            if tImGui.MenuItem(tLang.L('save_3d_scene_as')) then
                onSaveAsScene3d()
            end
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L('menu_quit'), 'Alt+F4') then
                mbm.quit()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('layer_options')) then
            if sActiveTab == 'layer' then
                if tImGui.MenuItem(tLang.L('select_all_meshes'), 'Ctrl+A') then
                    for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = true end
                end
                if tImGui.MenuItem(tLang.L('invert_selected_meshes'), 'Ctrl+I') then
                    for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = not tPlaced.isSelected end
                end
                if tImGui.MenuItem(tLang.L('unselect_all_meshes'), 'Ctrl+U') then
                    for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = false end
                end
                if tImGui.MenuItem(tLang.L('delete_mesh_selected'), 'Delete') then
                    for i = #tPlacedMeshes, 1, -1 do
                        if tPlacedMeshes[i].isSelected then removePlacedMesh(i) end
                    end
                end
            else
                tImGui.TextDisabled(tLang.L('please_select_layer'))
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('menu_run')) then
            tImGui.Text(tLang.L('resolution_expected'))
            local cInv, inv = tImGui.Checkbox(tLang.L('invert_width_height') .. '##world', tOptionsEditor.bInvertResolution)
            if cInv then tOptionsEditor.bInvertResolution = inv end
            local tResStr = {}
            for i, r in ipairs(tResolution) do
                tResStr[i] = string.format('%dx%d (%s)', r.x, r.y, r.comment)
            end
            local retR, curR = tImGui.Combo('##ComboResolutionWorld', tOptionsEditor.iIndexResolution - 1, tResStr)
            if retR then tOptionsEditor.iIndexResolution = curR + 1 end

            tImGui.Separator()
            tImGui.Text(tLang.L('camera_position'))
            local cx1, px = tImGui.InputFloat(tLang.L('axis_x') .. '##cam_pos_x', tOptionsEditor.fSceneCamPos.x, 1, 10, '%.2f')
            local cy1, py = tImGui.InputFloat(tLang.L('axis_y') .. '##cam_pos_y', tOptionsEditor.fSceneCamPos.y, 1, 10, '%.2f')
            local cz1, pz = tImGui.InputFloat(tLang.L('axis_z') .. '##cam_pos_z', tOptionsEditor.fSceneCamPos.z, 1, 10, '%.2f')
            if cx1 or cy1 or cz1 then
                tOptionsEditor.fSceneCamPos = {x = px, y = py, z = pz}
            end
            tImGui.Text(tLang.L('camera_focus'))
            local cx2, fx = tImGui.InputFloat(tLang.L('axis_x') .. '##cam_focus_x', tOptionsEditor.fSceneCamFocus.x, 1, 10, '%.2f')
            local cy2, fy = tImGui.InputFloat(tLang.L('axis_y') .. '##cam_focus_y', tOptionsEditor.fSceneCamFocus.y, 1, 10, '%.2f')
            local cz2, fz = tImGui.InputFloat(tLang.L('axis_z') .. '##cam_focus_z', tOptionsEditor.fSceneCamFocus.z, 1, 10, '%.2f')
            if cx2 or cy2 or cz2 then
                tOptionsEditor.fSceneCamFocus = {x = fx, y = fy, z = fz}
            end
            if tImGui.Button(tLang.L('set_initial_camera_pos'), {x = -1, y = 0}) then
                local p = camera3d:getPos()
                local f = camera3d:getFocus()
                tOptionsEditor.fSceneCamPos = {x = p.x, y = p.y, z = p.z}
                tOptionsEditor.fSceneCamFocus = {x = f.x, y = f.y, z = f.z}
            end

            tImGui.Separator()
            tImGui.Text(tLang.L('resolution'))
            local cInvL, invL = tImGui.Checkbox(tLang.L('invert_width_height') .. '##launch', tOptionsLaunch.bInvertResolution)
            if cInvL then tOptionsLaunch.bInvertResolution = invL end
            local retL, curL = tImGui.Combo('##ComboResolutionLaunch', tOptionsLaunch.iIndexResolution - 1, tResStr)
            if retL then tOptionsLaunch.iIndexResolution = curL + 1 end

            if tImGui.Button(tLang.L('play'), tUtil.getResponsiveItemSize(200)) then
                onPlay3d()
            end
            tImGui.SameLine()
            tImGui.TextDisabled('F5')

            tImGui.Text(tLang.L('execute_script'))
            if tImGui.Button('...##exec_script', {x = 30, y = 0}) then
                local fileName = mbm.openFile(tOptionsEditor.sCurrentScriptExecution, '*.lua')
                if fileName then tOptionsEditor.sCurrentScriptExecution = fileName end
            end
            tImGui.SameLine()
            tImGui.TextDisabled(tOptionsEditor.sCurrentScriptExecution ~= '' and tOptionsEditor.sCurrentScriptExecution or tLang.L('none'))

            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('general_options')) then
            local cShow, show = tImGui.Checkbox(tLang.L('show_mesh_map'), bEnableMainTabBar)
            if cShow then bEnableMainTabBar = show end

            tImGui.Separator()
            local cOrigin, origin = tImGui.Checkbox(tLang.L('enable_origin_lines'), bEnableOriginLines)
            if cOrigin then
                bEnableOriginLines = origin
                if tOriginLine3dX then tOriginLine3dX.visible = origin end
                if tOriginLine3dY then tOriginLine3dY.visible = origin end
                if tOriginLine3dZ then tOriginLine3dZ.visible = origin end
            end

            tLang.renderLanguageSubmenu()

            if tImGui.BeginMenu(tLang.L('background_color')) then
                local tColors = {
                    {name = 'default', r = 60, g = 60, b = 60},
                    {name = 'white',   r = 255, g = 255, b = 255},
                    {name = 'black',   r = 0, g = 0, b = 0},
                    {name = 'red',     r = 255, g = 0, b = 0},
                    {name = 'green',   r = 0, g = 255, b = 0},
                    {name = 'blue',    r = 0, g = 0, b = 255},
                }
                for _, c in ipairs(tColors) do
                    if tImGui.MenuItem(tLang.L(c.name)) then
                        mbm.setColor(c.r, c.g, c.b)
                    end
                end
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('zoom')) then
            local retZ, distance = tImGui.SliderFloat('##Scale3d', cam3d.distance, 50, 5000, tLang.L('scale_format'))
            if retZ then cam3d.distance = distance end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('default') .. '##zoom_default') then
                cam3d.distance = 800
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('menu_about')) then
            tImGui.MenuItem(tLang.L('scene_editor_3d'))
            tImGui.MenuItem(tLang.L('mbm_engine'))
            if tImGui.BeginMenu(tLang.L('menu_version')) then
                tImGui.TextDisabled(string.format('%s\nIMGUI: %s', mbm.get('version'), tImGui.GetVersion()))
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end

        tImGui.EndMainMenuBar()
    end
end

------------------------------------------------------------------------------------------------------------------
-- Save / Load / Play / final generated script
------------------------------------------------------------------------------------------------------------------

function getHeader3d(fileName)
    local sHeader = '--[[\n' .. [[
    Scene 3d - this file is meant to be used in the engine mbm

    how to:
    * in your scene do:

        local tScene3d = require "SCENE_NAME"
        tScene3d:load(onProgress)

    * To retrieve mesh(es) use get or getAll:

        local tMesh = tScene3d:get('rock.msh')
        local tAll  = tScene3d:getAll('tree.msh')

    * To spawn a new instance at runtime:

        local tNew = tScene3d:addMesh('rock.msh')

]] .. ']]\n\n'
    sHeader = sHeader:gsub('SCENE_NAME', tUtil.getShortName(fileName, false):gsub('%.lua$', ''))
    return sHeader
end

function getSceneLoaderCode3d()
    return [[
tScene3d.updateCamera = function(self)
    local cam = mbm.getCamera('3d')
    cam:setPos(self.fCamPos.x, self.fCamPos.y, self.fCamPos.z)
    cam:setFocus(self.fCamFocus.x, self.fCamFocus.y, self.fCamFocus.z)
end

tScene3d._addMesh = function(self, tInfo, onDone)
    local offset = self.tMeshOffsets[tInfo.fileName] or {x=0,y=0,z=0}
    local function finish(tObj)
        if tObj then
            tObj:setPos(tInfo.x + offset.x, tInfo.y + offset.y, tInfo.z + offset.z)
            tObj:setAngle(0, tInfo.rotationY or 0, 0)
            tObj:setScale(tInfo.sx or 1, tInfo.sy or 1, tInfo.sz or 1)
            table.insert(self.tMeshesLoaded, tObj)
            self.tMeshesLoadedDictionary[tInfo.fileName] = self.tMeshesLoadedDictionary[tInfo.fileName] or {}
            table.insert(self.tMeshesLoadedDictionary[tInfo.fileName], tObj)
        end
        self.iLoadedCount = self.iLoadedCount + 1
        if onDone then onDone(tObj) end
    end
    if tInfo.type == 'mesh' then
        local m = mesh:new('3d')
        m:loadAsync(tInfo.fileName, function(self_mesh, success)
            finish(success and self_mesh or nil)
        end)
    elseif tInfo.type == 'sprite' then
        local s = sprite:new('3d')
        finish(s:load(tInfo.fileName) and s or nil)
    elseif tInfo.type == 'particle' then
        local p = particle:new('3d')
        finish(p:load(tInfo.fileName) and p or nil)
    elseif tInfo.type == 'tile' then
        local t = tile:new('2dw')
        finish(t:load(tInfo.fileName) and t or nil)
    elseif tInfo.type == 'gif' then
        local g = gif:new('3d')
        finish(g:load(tInfo.fileName) and g or nil)
    else
        local t = texture:new('3d')
        finish(t:load(tInfo.fileName) and t or nil)
    end
end

tScene3d.load = function(self, onProgress)
    if self.tLightConfig and self.tLightConfig.bEnabled then
        mbm.setLightEnabled('3d', true)
        mbm.setAmbientLight('3d', self.tLightConfig.ambientColor)
        mbm.setDirectionalLight('3d', self.tLightConfig.directionalDir, self.tLightConfig.directionalColor)
        for _, pl in ipairs(self.tLightConfig.pointLights or {}) do
            mbm.addPointLight('3d', pl.x, pl.y, pl.z, pl.radius, pl.r, pl.g, pl.b, pl.a or 255)
        end
    else
        mbm.setLightEnabled('3d', false)
    end
    self:updateCamera()
    self.tMeshesLoaded = {}
    self.tMeshesLoadedDictionary = {}
    self.iLoadedCount = 0
    self.iTotalCount  = #self.tPlacedMeshInfo
    for i = 1, #self.tPlacedMeshInfo do
        self:_addMesh(self.tPlacedMeshInfo[i], function()
            if onProgress then
                onProgress(self.iLoadedCount / math.max(1, self.iTotalCount) * 100.0)
            end
        end)
    end
end

tScene3d.get = function(self, nameOrIndex)
    if type(nameOrIndex) == 'number' then
        return self.tMeshesLoaded[nameOrIndex]
    end
    local list = self.tMeshesLoadedDictionary[nameOrIndex]
    return list and list[1] or nil
end

tScene3d.getAll = function(self, name)
    return self.tMeshesLoadedDictionary[name] or {}
end

tScene3d.addMesh = function(self, nameOrFileName)
    local tInfo = { fileName = nameOrFileName, type = (meshDebug and meshDebug:getInfo(nameOrFileName) and meshDebug:getInfo(nameOrFileName).type) or 'mesh', x = 0, y = 0, z = 0 }
    local result = nil
    self:_addMesh(tInfo, function(tObj) result = tObj end)
    return result
end

return tScene3d
]]
end

function writeScene3d(fileName)
    local oldLocale = os.setlocale(nil, 'numeric')
    os.setlocale('C', 'numeric')
    local fp = io.open(fileName, 'w')
    if not fp then
        os.setlocale(oldLocale, 'numeric')
        return false, string.format(tLang.L('could_not_open_for_write_fmt'), fileName)
    end

    fp:write(getHeader3d(fileName))

    local tPaths = mbm.getAllPaths()
    for _, sPath in ipairs(tPaths) do
        fp:write(string.format('mbm.addPath(%q)\n', sPath))
    end

    fp:write('\nlocal tScene3d = {}\n')

    local xRes, yRes
    if tOptionsEditor.bInvertResolution then
        xRes, yRes = tResolution[tOptionsEditor.iIndexResolution].y, tResolution[tOptionsEditor.iIndexResolution].x
    else
        xRes, yRes = tResolution[tOptionsEditor.iIndexResolution].x, tResolution[tOptionsEditor.iIndexResolution].y
    end
    fp:write(string.format('tScene3d.iExpectedWidth  = %d\n', xRes))
    fp:write(string.format('tScene3d.iExpectedHeight = %d\n', yRes))

    fp:write('tScene3d.fCamPos   = ' .. tUtil.save(nil, tOptionsEditor.fSceneCamPos, nil, nil, {}) .. '\n')
    fp:write('tScene3d.fCamFocus = ' .. tUtil.save(nil, tOptionsEditor.fSceneCamFocus, nil, nil, {}) .. '\n')
    fp:write('tScene3d.tLightConfig = ' .. tUtil.save(nil, tLightConfig, nil, nil, {}) .. '\n')
    fp:write('tScene3d.tMeshOffsets = ' .. tUtil.save(nil, tMeshOffsets, nil, nil, {}) .. '\n')
    fp:write('tScene3d.tSceneObjects = ' .. tUtil.save(nil, tSceneObjects, nil, nil, {}) .. '\n')

    fp:write('tScene3d.tPlacedMeshInfo = {\n')
    for i, tPlaced in ipairs(tPlacedMeshes) do
        local x, y, z = resolvePlacedMeshWorldPos(tPlaced)
        fp:write(string.format(
            '[%d]={fileName=%q,type=%q,x=%g,y=%g,z=%g,rotationY=%g,sx=%g,sy=%g,sz=%g},\n',
            i, tPlaced.fileName, tPlaced.type, x, y, z, tPlaced.rotationY or 0,
            tPlaced.scale.x, tPlaced.scale.y, tPlaced.scale.z))
    end
    fp:write('}\n\n')

    fp:write(getSceneLoaderCode3d())
    fp:close()
    os.setlocale(oldLocale, 'numeric')
    return true
end

function onOpenScene3d()
    local fileName = mbm.openFile(sFileNameScene3d, '*.lua')
    if not fileName then return end
    sFileNameScene3d = fileName
    tUtil.showMessage(tLang.L('scene_3d_loaded_ok'))
    -- Loading back into the editor's own live state (re-hydrating tLayers/tPlacedMeshes/etc from
    -- the saved script) is intentionally left to the same async placeMeshAsync/addPlacedMesh path
    -- used for normal placement, driven by the progress modal, once the file is dofile'd.
    local chunk = loadfile(fileName)
    if not chunk then
        tUtil.showMessageWarn(tLang.L('scene_3d_load_failed'))
        return
    end
    local ok, tLoaded = pcall(chunk)
    if not ok or not tLoaded then
        tUtil.showMessageWarn(tLang.L('scene_3d_load_failed'))
        return
    end
    tMeshOffsets  = tLoaded.tMeshOffsets or {}
    tLightConfig  = tLoaded.tLightConfig or tLightConfig
    tSceneObjects = tLoaded.tSceneObjects or {}
    tSceneObjectShapes = {}
    for i = #tPlacedMeshes, 1, -1 do removePlacedMesh(i) end
    beginLoadProgress(#(tLoaded.tPlacedMeshInfo or {}), tLang.L('loading_scene'))
    for _, tInfo in ipairs(tLoaded.tPlacedMeshInfo or {}) do
        if #tLayers == 0 then addLayer() end
        local tPlaced = addPlacedMesh(tInfo.fileName, tInfo.type, 1, 0, 0, tInfo.x, tInfo.z)
        tPlaced.bAttachedToLayer = false
        tPlaced.freeY = tInfo.y
        tPlaced.rotationY = tInfo.rotationY
        tPlaced.scale = {x = tInfo.sx, y = tInfo.sy, z = tInfo.sz}
    end
end

function onSaveScene3d()
    if sFileNameScene3d == '' then
        onSaveAsScene3d()
        return
    end
    local ok, err = writeScene3d(sFileNameScene3d)
    if ok then
        tUtil.showMessage(tLang.L('scene_3d_saved_ok'))
    else
        tUtil.showMessageWarn(err)
    end
end

function onSaveAsScene3d()
    local fileName = mbm.saveFile(sFileNameScene3d, '*.lua')
    if not fileName then return end
    sFileNameScene3d = fileName
    onSaveScene3d()
end

function getTempScene3dFile()
    local sep = package.config:sub(1, 1)
    local tmpDir
    if sep == '\\' then
        tmpDir = os.getenv('TEMP') or os.getenv('TMP') or 'C:\\Temp'
    else
        tmpDir = os.getenv('TMPDIR') or '/tmp'
    end
    return tmpDir .. sep .. 'mbm_scene3d_preview.lua'
end

function onPlay3d()
    local width, height
    if tOptionsLaunch.bInvertResolution then
        width, height = tResolution[tOptionsLaunch.iIndexResolution].y, tResolution[tOptionsLaunch.iIndexResolution].x
    else
        width, height = tResolution[tOptionsLaunch.iIndexResolution].x, tResolution[tOptionsLaunch.iIndexResolution].y
    end
    local expectedWidth, expectedHeight
    if tOptionsEditor.bInvertResolution then
        expectedWidth, expectedHeight = tResolution[tOptionsEditor.iIndexResolution].y, tResolution[tOptionsEditor.iIndexResolution].x
    else
        expectedWidth, expectedHeight = tResolution[tOptionsEditor.iIndexResolution].x, tResolution[tOptionsEditor.iIndexResolution].y
    end

    if tOptionsEditor.sCurrentScriptExecution and #tOptionsEditor.sCurrentScriptExecution > 0 then
        tUtil.newInstance(width, height, expectedWidth, expectedHeight, tOptionsEditor.sCurrentScriptExecution)
    else
        local sTmpFile = getTempScene3dFile()
        local ok, err = writeScene3d(sTmpFile)
        if not ok then
            tUtil.showMessageWarn(err or 'Failed to write temporary play file')
            return
        end
        tUtil.newInstance(width, height, expectedWidth, expectedHeight, sTmpFile)
    end
end

------------------------------------------------------------------------------------------------------------------
-- Engine callbacks
------------------------------------------------------------------------------------------------------------------

function onInitScene()
    camera3d = mbm.getCamera('3d')
    ImGuiPopupFlags_MouseButtonRight = tImGui.Flags('ImGuiPopupFlags_MouseButtonRight')

    sThumbnailCacheDir = (mbm.getPathEngine() or '.') .. package.config:sub(1, 1)
    sMeshSetFolder = mbm.getPathEngine() or ''

    tOriginLine3dX = line:new('3d', 0, 0, 0)
    tOriginLine3dX:add({-999999, 0, 0, 999999, 0, 0})
    tOriginLine3dX:setColor(255, 0, 0, 255)
    tOriginLine3dY = line:new('3d', 0, 0, 0)
    tOriginLine3dY:add({0, -999999, 0, 0, 999999, 0})
    tOriginLine3dY:setColor(0, 255, 0, 255)
    tOriginLine3dZ = line:new('3d', 0, 0, 0)
    tOriginLine3dZ:add({0, 0, -999999, 0, 0, 999999})
    tOriginLine3dZ:setColor(0, 0, 255, 255)

    mbm.setLightEnabled('3d', false)
    addLayer()
    applyCam3d(cam3d)
    tUtil.sMessageOverlay = tLang.L('welcome_scene_editor_3d')
end

function onLoop(delta)
    applyCam3d(cam3d)
    processThumbnailQueue()
    tickThumbnailBake()

    main_menu_3d()
    main_tab_bar()

    if sActiveTab == 'mesh_set' and iPreviewedMeshSetIndex > 0 then
        tImGui.Begin(tLang.L('mesh_preview_orbit'), true, 0)
        tUtil.drawOrbitGizmo(cam3d, {size = 110})
        tImGui.End()
    end

    if not tImGui.IsAnyWindowHovered() then
        updateHoverHighlight(mouseLastX, mouseLastY)
    end

    showLoadProgressModal()
    tUtil.showOverlayMessage()
end

function onTouchDown(key, x, y)
    if tImGui.IsAnyWindowHovered() then return end
    isClickedMouseleft  = (key == 0)
    isClickedMouseRight = (key == 1)
    mouseLastX, mouseLastY = x, y

    if key == 0 and sActiveTab == 'layer' and sMeshSelectedForPlacement then
        local entry = nil
        for _, e in ipairs(tMeshSetEntries) do
            if e.fileName == sMeshSelectedForPlacement then entry = e; break end
        end
        if entry and iSelectedLayer > 0 then
            local layer = tLayers[iSelectedLayer]
            if tMapOptions.sMapType == 'Free' then
                local wx, wy, wz = screenToWorldOnLayerPlane(x, y, layer.fY)
                if wx then
                    addPlacedMesh(entry.fileName, entry.type, iSelectedLayer, 0, 0, wx, wz)
                end
            else
                local wx, wy, wz = screenToWorldOnLayerPlane(x, y, layer.fY)
                if wx then
                    local cx, cz = worldToGridCell(wx, wz, layer)
                    addPlacedMesh(entry.fileName, entry.type, iSelectedLayer, cx, cz)
                end
            end
        end
    end
end

function onTouchMove(key, x, y)
    if tImGui.IsAnyWindowHovered() then
        mouseLastX, mouseLastY = x, y
        return
    end
    if isClickedMouseleft then
        cam3d.azimuth   = cam3d.azimuth   - (x - mouseLastX) * 0.005
        cam3d.elevation = cam3d.elevation + (y - mouseLastY) * 0.005
        cam3d.elevation = math.max(-math.pi * 0.49, math.min(math.pi * 0.49, cam3d.elevation))
    elseif isClickedMouseRight then
        local camPos = camera3d:getPos()
        local fwx, fwy, fwz = cam3d.fx - camPos.x, cam3d.fy - camPos.y, cam3d.fz - camPos.z
        local len = math.sqrt(fwx * fwx + fwy * fwy + fwz * fwz)
        if len > 1e-6 then
            fwx, fwy, fwz = fwx / len, fwy / len, fwz / len
            local rgx, rgz = -fwz, fwx -- cross(worldUp(0,1,0), forward) simplified to XZ plane
            local dx, dy = (x - mouseLastX), (y - mouseLastY)
            local scale = cam3d.distance * 0.001
            cam3d.fx = cam3d.fx - rgx * dx * scale
            cam3d.fz = cam3d.fz - rgz * dx * scale
            cam3d.fy = cam3d.fy + dy * scale
        end
    end
    mouseLastX, mouseLastY = x, y
end

function onTouchUp(key, x, y)
    isClickedMouseleft  = false
    isClickedMouseRight = false
end

function onTouchZoom(zoom)
    cam3d.distance = math.max(10, cam3d.distance * (1.0 - zoom * 0.15))
end

function onKeyDown(key)
    if key == mbm.getKeyCode('F5') then
        onPlay3d()
    elseif key == mbm.getKeyCode('Delete') and sActiveTab == 'layer' then
        for i = #tPlacedMeshes, 1, -1 do
            if tPlacedMeshes[i].isSelected then removePlacedMesh(i) end
        end
    end
end

function onKeyUp(key)
end
