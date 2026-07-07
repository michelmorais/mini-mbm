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
-- NOTE: tImGui.Combo takes/returns a 1-based index (Lua convention), NOT a 0-based C index --
-- the binding does the -1/+1 conversion internally. tComboXxxIndexOf() helpers below return
-- 1-based indices and combo results are used directly (no +-1 offset) to match this.
tComboMapType3d      = {'Free', 'Orthogonal grid', 'Isometric grid'}
tComboMapType3dCodes = {'Free', 'Orthogonal', 'Isometric'}
tComboSnapScaleMode = {'none', 'full_fit', 'bigger_only'}
tComboSnapScaleModeLabel = {'No scaling', 'Scale to full fit', 'Scale when bigger than grid'}

-- A cell size of 0 (or negative) is a division-by-zero hazard in worldToGridCell, and a grid
-- count of 0 (or negative) is meaningless -- both fields previously had no floor at all.
MIN_GRID_CELL_SIZE = 1
MIN_GRID_COUNT     = 1

tMapOptions = {
    sMapType         = 'Orthogonal',
    fGridCellWidthX  = 100,
    fGridCellDepthZ  = 100,
    iGridCountX      = 20,
    iGridCountZ      = 20,
    sSnapScaleMode   = 'none',
    bShowGrid        = true,
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

-- ---- Orbit camera (mesh_debug.lua pattern) ----
-- Default camera3d:setFar() (camera.cpp) is only 1000 -- far too close for a scene editor where
-- placed objects and the orbit distance routinely exceed it, silently clipping meshes out of view
-- well before they leave the screen. This is a scene-authoring tool, not a shipped game render path,
-- so there is no real perf reason to keep the default; push it out generously (applied in onInitScene).
iCameraFarPlane3d = 80000
-- Each tab keeps its own independent camera state so previewing a mesh in the Mesh Set tab (which
-- recenters/refits the camera on that mesh) can't disturb the framing you set up in the Layer tab,
-- and vice versa. `cam3d` always points at whichever of these is current for `sActiveTab` -- kept
-- as a plain alias (not a copy) so every existing `cam3d.xxx` read/write below keeps working as-is.
tCamByTab = {
    map      = { azimuth = 0.3, elevation = 0.3, distance = 800, fx = 0, fy = 0, fz = 0 },
    mesh_set = { azimuth = 0.3, elevation = 0.3, distance = 800, fx = 0, fy = 0, fz = 0 },
    layer    = { azimuth = 0.3, elevation = 0.3, distance = 800, fx = 0, fy = 0, fz = 0 },
}
cam3d = tCamByTab.map

-- ---- Mesh Selector / placement state ----
bShowMeshSelector         = true
sMeshSelectedForPlacement = nil
iSizeMeshOnSelector       = 90

-- ---- Hover / selection state ----
tHoveredPlaced = nil

-- ---- Multi-select rectangle drag (Layer tab, Shift+drag) ----
keyShiftPressed    = false
keyControlPressed  = false
bRectSelecting     = false
tRectSelection     = { xStart = 0, yStart = 0 }
lnRectSelection    = nil
tToolsMeshSize     = { x = 0, y = 0 }

-- ---- Origin lines ----
tOriginLine3dX, tOriginLine3dY, tOriginLine3dZ = nil, nil, nil
bEnableOriginLines = true

-- ---- Grid visual ----
tGridLines = {}

-- ---- Lighting ----
-- Colors are 0.0-1.0 floats (ImGui ColorEdit4 range and what mbm.setAmbientLight/etc. actually
-- expect/clamp to internally) -- NOT 0-255, despite some docs/lua-api.md examples suggesting otherwise.
tLightConfig = {
    bEnabled          = false,
    ambientColor      = {r = 0.12, g = 0.12, b = 0.16, a = 1},
    directionalColor  = {r = 1, g = 0.98, b = 0.9, a = 1},
    directionalDir    = {x = 0, y = -1, z = 0.3},
    pointLights       = {},
}

-- Editor-only, NOT serialized into the saved/exported scene (tLightConfig.directionalDir is the
-- one persisted, engine-facing value -- this is just a friendlier way to edit it, derived from it
-- on load and re-derived into it on every change). Same azimuth/elevation shape as cam3d, so the
-- existing orbit trackball widget (tUtil.drawOrbitGizmo) works on it unmodified.
tLightOrbit = { azimuth = 0, elevation = 0 }

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

-- tLightOrbit uses the exact same azimuth/elevation convention as cam3d/drawOrbitGizmo. The
-- conversion math itself lives in editor_utils.lua (tUtil.dirFromOrbit/orbitFromDir) since
-- mesh_debug.lua's light panel now uses the same orbit-gizmo-for-light-direction pattern -- these
-- two wrappers just bridge that shared math to this file's own tLightOrbit/tLightConfig globals.
function computeDirectionalDirFromOrbit()
    tLightConfig.directionalDir = tUtil.dirFromOrbit(tLightOrbit)
end

function computeOrbitFromDirectionalDir()
    local orbit = tUtil.orbitFromDir(tLightConfig.directionalDir)
    tLightOrbit.azimuth = orbit.azimuth
    tLightOrbit.elevation = orbit.elevation
end

------------------------------------------------------------------------------------------------------------------
-- Light gizmos (Map tab only): a visual arrow showing where the directional light comes from,
-- and a small tinted marker at each point light's position -- otherwise there is no way to see
-- where a light actually is/points relative to the placed scene, only numbers in a panel.
------------------------------------------------------------------------------------------------------------------

tLightGizmo = { lnShaft = nil, lnWing1 = nil, lnWing2 = nil }
tPointLightMarkers = {}

-- An arbitrary vector perpendicular to (dx,dy,dz), used to build the arrow's two head "wings".
-- Picks world-X as the reference instead of world-Y whenever the direction is close to vertical,
-- so the cross product never degenerates near-parallel.
function perpendicularToDirection(dx, dy, dz)
    local refx, refy, refz = 0, 1, 0
    if math.abs(dy) > 0.99 then
        refx, refy, refz = 1, 0, 0
    end
    local px = dy * refz - dz * refy
    local py = dz * refx - dx * refz
    local pz = dx * refy - dy * refx
    local len = math.sqrt(px * px + py * py + pz * pz)
    if len < 1e-6 then len = 1 end
    return px / len, py / len, pz / len
end

-- Where the arrow's head (the "light hits here") end sits -- above the currently active layer's
-- center, or a reasonable fixed point if no layer is selected yet.
function getLightGizmoAnchor()
    local layer = tLayers[iSelectedLayer]
    if layer then
        return layer.offset.x, layer.fY + 300, layer.offset.z
    end
    return 0, 300, 0
end

function ensureLightGizmoLines()
    if tLightGizmo.lnShaft then return end
    tLightGizmo.lnShaft = line:new('3d', 0, 0, 0)
    tLightGizmo.lnShaft:add({0, 0, 0, 0, 0, 0})
    tLightGizmo.lnShaft:setColor(1, 0.9, 0.3, 1)
    tLightGizmo.lnWing1 = line:new('3d', 0, 0, 0)
    tLightGizmo.lnWing1:add({0, 0, 0, 0, 0, 0})
    tLightGizmo.lnWing1:setColor(1, 0.9, 0.3, 1)
    tLightGizmo.lnWing2 = line:new('3d', 0, 0, 0)
    tLightGizmo.lnWing2:add({0, 0, 0, 0, 0, 0})
    tLightGizmo.lnWing2:setColor(1, 0.9, 0.3, 1)
end

-- Called every frame (Map tab only) -- draws an arrow from where the light comes from (the tail)
-- to a fixed reference point near the active layer (the head), pointing in the direction light
-- actually travels (tLightConfig.directionalDir), so it visually reads as "the sun is over there,
-- shining this way" instead of just three numbers in a panel.
function updateDirectionalLightGizmo()
    local showGizmo = sActiveTab == 'map' and tLightConfig.bEnabled
    if not showGizmo then
        if tLightGizmo.lnShaft then
            tLightGizmo.lnShaft.visible = false
            tLightGizmo.lnWing1.visible = false
            tLightGizmo.lnWing2.visible = false
        end
        return
    end
    ensureLightGizmoLines()

    local dir = tLightConfig.directionalDir
    local len = math.sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z)
    if len < 1e-6 then len = 1 end
    local ndx, ndy, ndz = dir.x / len, dir.y / len, dir.z / len

    local ax, ay, az = getLightGizmoAnchor()
    local shaftLen = 150
    local tx, ty, tz = ax - ndx * shaftLen, ay - ndy * shaftLen, az - ndz * shaftLen

    tLightGizmo.lnShaft:setPos(0, 0, 0)
    tLightGizmo.lnShaft:set({tx, ty, tz, ax, ay, az}, 1)
    tLightGizmo.lnShaft.visible = true

    local px, py, pz = perpendicularToDirection(ndx, ndy, ndz)
    local headLen, headWidth = 24, 12
    local hx, hy, hz = ax - ndx * headLen, ay - ndy * headLen, az - ndz * headLen
    tLightGizmo.lnWing1:setPos(0, 0, 0)
    tLightGizmo.lnWing1:set({ax, ay, az, hx + px * headWidth, hy + py * headWidth, hz + pz * headWidth}, 1)
    tLightGizmo.lnWing1.visible = true
    tLightGizmo.lnWing2:setPos(0, 0, 0)
    tLightGizmo.lnWing2:set({ax, ay, az, hx - px * headWidth, hy - py * headWidth, hz - pz * headWidth}, 1)
    tLightGizmo.lnWing2.visible = true
end

-- Called every frame (Map tab only) -- one small tinted box marker per point light (same
-- technique as the placed-mesh selection highlight: physic_editor.lua visualizes its own SPHERE
-- physics shapes as boxes too -- there's no true sphere-mesh primitive in this codebase's Lua
-- tooling), so each point light's actual position is visible among the placed scene, not just
-- x/y/z numbers in a panel.
function updatePointLightMarkers()
    local showMarkers = sActiveTab == 'map' and tLightConfig.bEnabled
    if not showMarkers then
        for _, marker in ipairs(tPointLightMarkers) do marker.visible = false end
        return
    end
    while #tPointLightMarkers < #tLightConfig.pointLights do
        table.insert(tPointLightMarkers, makeHighlightBoxShape())
    end
    while #tPointLightMarkers > #tLightConfig.pointLights do
        tPointLightMarkers[#tPointLightMarkers]:destroy()
        tPointLightMarkers[#tPointLightMarkers] = nil
    end
    local markerSize = 20
    for i, pl in ipairs(tLightConfig.pointLights) do
        local marker = tPointLightMarkers[i]
        marker:setPos(pl.x, pl.y, pl.z)
        marker:setScale(markerSize, markerSize, markerSize)
        marker:setColor(pl.r, pl.g, pl.b, 0.5)
        marker.visible = true
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

-- Point lights are usually meant to sit somewhere around the scene being built, so drag range
-- should scale with it rather than being some arbitrary fixed constant -- reuses the exact same
-- extent the grid itself is drawn/bordered to (getGridWorldExtent), offset by the active layer's
-- own position, so a bigger grid naturally gets a bigger draggable range. There's no equivalent
-- "grid extent" on Y (the grid is a flat plane) -- reuses the X/Z span, biased upward, since a
-- light more often sits above the scene than below it.
function getPointLightDragRange()
    local xLow, xHigh, zLow, zHigh = getGridWorldExtent()
    local layer = tLayers[iSelectedLayer]
    local offX, offZ, offY = 0, 0, 0
    if layer then
        offX, offZ, offY = layer.offset.x, layer.offset.z, layer.fY
    end
    local minX, maxX = offX + xLow, offX + xHigh
    local minZ, maxZ = offZ + zLow, offZ + zHigh
    local span = math.max(maxX - minX, maxZ - minZ, 1)
    local minY, maxY = offY - span * 0.25, offY + span * 1.5
    return minX, maxX, minY, maxY, minZ, maxZ
end

function drawLightPanel()
    tImGui.Text(tLang.L('light_panel'))
    local enabled = tImGui.Checkbox('##light_enabled', tLightConfig.bEnabled)
    if enabled ~= tLightConfig.bEnabled then
        tLightConfig.bEnabled = enabled
        applyTabLighting()
    end
    tImGui.SameLine()
    tImGui.Text(tLang.L('light_enabled'))

    if not tLightConfig.bEnabled then
        return
    end

    tImGui.Text(tLang.L('light_ambient'))
    local changedAmbient, ambientColor = tImGui.ColorEdit4('##light_ambient', tLightConfig.ambientColor)
    if changedAmbient then
        tLightConfig.ambientColor = ambientColor
        mbm.setAmbientLight('3d', ambientColor)
    end

    tImGui.Text(tLang.L('light_directional'))
    local changedDirColor, dirColor = tImGui.ColorEdit4('##light_directional_color', tLightConfig.directionalColor)
    if changedDirColor then
        tLightConfig.directionalColor = dirColor
        mbm.setDirectionalLightColor('3d', dirColor)
    end
    -- Same orbit trackball widget already used for the camera (tUtil.drawOrbitGizmo only reads/
    -- writes .azimuth/.elevation, nothing else, so it works unmodified on tLightOrbit) -- drag to
    -- re-aim the sun instead of typing a raw direction vector by hand.
    tImGui.Text(tLang.L('light_direction'))
    if tUtil.drawOrbitGizmo(tLightOrbit, {size = 110}) then
        computeDirectionalDirFromOrbit()
        mbm.setDirectionalLightDirection('3d', tLightConfig.directionalDir)
    end
    tImGui.TextDisabled(string.format('%s: x=%.3f y=%.3f z=%.3f', tLang.L('light_direction'),
        tLightConfig.directionalDir.x, tLightConfig.directionalDir.y, tLightConfig.directionalDir.z))

    tImGui.Separator()
    tImGui.Text(tLang.L('light_point_lights_fmt'):format(#tLightConfig.pointLights))
    if tImGui.Button(tLang.L('add_point_light')) then
        table.insert(tLightConfig.pointLights, {x = 0, y = 200, z = 0, radius = 400, r = 1, g = 1, b = 1, a = 1})
        applyLightConfigToEngine()
    end
    local minX, maxX, minY, maxY, minZ, maxZ = getPointLightDragRange()
    for i, pl in ipairs(tLightConfig.pointLights) do
        tImGui.PushItemWidth(120)
        local p1, px = tImGui.DragFloat(tLang.L('axis_x') .. '##pl_x' .. i, pl.x, 1, minX, maxX, '%.2f')
        local p2, py = tImGui.DragFloat(tLang.L('axis_y') .. '##pl_y' .. i, pl.y, 1, minY, maxY, '%.2f')
        local p3, pz = tImGui.DragFloat(tLang.L('axis_z') .. '##pl_z' .. i, pl.z, 1, minZ, maxZ, '%.2f')
        tImGui.PopItemWidth()
        local p4, pr = tImGui.DragFloat(tLang.L('light_radius') .. '##pl_r' .. i, pl.radius, 1, 1, math.max(maxX - minX, maxZ - minZ), '%.2f')
        local changedColor, plColor = tImGui.ColorEdit4('##pl_color' .. i, {r = pl.r, g = pl.g, b = pl.b, a = pl.a})
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

-- Isometric grid lines are drawn rotated 45 degrees (rebuildGridVisual) -- cell math must apply
-- the same rotation, or placement keeps snapping to the (invisible) orthogonal axes underneath
-- the isometric visual, which is what it used to do here (a real, confirmed bug: the grid *looked*
-- isometric but always snapped orthogonally).
function getMapRotationRad()
    return tMapOptions.sMapType == 'Isometric' and (math.pi * 0.25) or 0
end

function gridCellToWorld(cx, cz, layer)
    local lx = cx * tMapOptions.fGridCellWidthX
    local lz = cz * tMapOptions.fGridCellDepthZ
    local rot = getMapRotationRad()
    local cosr, sinr = math.cos(rot), math.sin(rot)
    local x = lx * cosr - lz * sinr + layer.offset.x
    local z = lx * sinr + lz * cosr + layer.offset.z
    return x, layer.fY, z
end

function worldToGridCell(wx, wz, layer)
    local lx = wx - layer.offset.x
    local lz = wz - layer.offset.z
    -- inverse rotation: rotate world-space coords back into the grid's own (unrotated) local space
    local rot = -getMapRotationRad()
    local cosr, sinr = math.cos(rot), math.sin(rot)
    local localX = lx * cosr - lz * sinr
    local localZ = lx * sinr + lz * cosr
    local cx = math.floor(localX / tMapOptions.fGridCellWidthX + 0.5)
    local cz = math.floor(localZ / tMapOptions.fGridCellDepthZ + 0.5)
    return cx, cz
end

-- Single source of truth for how an axis's "count" setting maps to a span of integer cell
-- indices, shared by rebuildGridVisual (visual lines), fillActiveLayerWithMesh, and the
-- placement bounds check below. Cell indices land on the same absolute lattice regardless of
-- parity (cMin = -floor(nCells/2)), so this must NOT be reimplemented as nCells*0.5 anywhere --
-- that alternative is only an integer for even nCells, and for odd nCells it silently shifts
-- the visual grid by half a cell relative to where cellX/cellZ actually snap (a real, confirmed
-- bug: changing "Width count"/"Depth count" from an even to an odd number re-centered the grid
-- lines under already-placed meshes instead of leaving the lattice -- and the meshes on it -- alone).
function gridCellRange(nCells)
    local cMin = -math.floor(nCells * 0.5)
    return cMin, cMin + nCells - 1
end

-- The grid's local-space (pre-rotation, pre-layer-offset) extent, in the SAME lattice
-- gridCellRange/gridCellToWorld/worldToGridCell all share -- used both for the Orthogonal/
-- Isometric integer-cell bounds check and the Free-mode continuous world-position bounds check
-- below, so the two never disagree about where the grid's edge actually is.
function getGridWorldExtent()
    local nCellsX = math.max(1, tMapOptions.iGridCountX)
    local nCellsZ = math.max(1, tMapOptions.iGridCountZ)
    local w, d = tMapOptions.fGridCellWidthX, tMapOptions.fGridCellDepthZ
    local cxMin = gridCellRange(nCellsX)
    local czMin = gridCellRange(nCellsZ)
    return cxMin * w, (cxMin + nCellsX) * w, czMin * d, (czMin + nCellsZ) * d
end

-- Grid modes only place/keep meshes within the currently configured extent -- the grid is meant
-- to represent the bounds of the scene being built, so anything outside it is not a valid
-- placement.
function isCellWithinGridBounds(cx, cz)
    local cxMin, cxMax = gridCellRange(math.max(1, tMapOptions.iGridCountX))
    local czMin, czMax = gridCellRange(math.max(1, tMapOptions.iGridCountZ))
    return cx >= cxMin and cx <= cxMax and cz >= czMin and cz <= czMax
end

-- Free mode has no snapping, but the grid still represents the bounds of the scene -- a world
-- position is valid as long as it falls within the same extent the grid lines are drawn at
-- (allowing for the layer's own offset and the map's rotation, exactly like worldToGridCell's
-- inverse-rotation step).
function isWorldPosWithinGridBounds(wx, wz, layer)
    local lx = wx - layer.offset.x
    local lz = wz - layer.offset.z
    local rot = -getMapRotationRad()
    local cosr, sinr = math.cos(rot), math.sin(rot)
    local localX = lx * cosr - lz * sinr
    local localZ = lx * sinr + lz * cosr
    local xLow, xHigh, zLow, zHigh = getGridWorldExtent()
    return localX >= xLow and localX <= xHigh and localZ >= zLow and localZ <= zHigh
end

-- Called whenever the grid extent shrinks (count reduced) -- meshes that fall outside the new
-- bounds no longer belong to the scene the grid represents, so they're removed rather than left
-- floating past the visible edge. Applies to Free-mode meshes too now that Free mode is bordered
-- by the same grid (only the snapping is skipped for Free, not the bounds).
function removePlacedMeshesOutsideGrid()
    for i = #tPlacedMeshes, 1, -1 do
        local tPlaced = tPlacedMeshes[i]
        local layer = tLayers[tPlaced.layerIndex]
        local within
        if not layer then
            within = true -- no layer to test against -- leave orphaned entries alone here
        elseif tMapOptions.sMapType == 'Free' then
            within = isWorldPosWithinGridBounds(tPlaced.freeX, tPlaced.freeZ, layer)
        else
            within = isCellWithinGridBounds(tPlaced.cellX, tPlaced.cellZ)
        end
        if not within then
            removePlacedMesh(i)
        end
    end
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
    -- getAABB(true) measures the CURRENT world-space AABB, which factors in the object's CURRENT
    -- scale (confirmed: RENDERIZABLE::updateAABB() builds its transform from getScale() too, not
    -- just position/angle). Reset to identity scale first, or this measures an *already-scaled*
    -- object and computes a new scale relative to that -- which compounds every time this runs
    -- again on the same object (e.g. resyncAllPlacedMeshes firing on every click of a cell-size
    -- spinner's +/- buttons), drifting further off with each call instead of converging.
    tObj:setScale(1, 1, 1)
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

-- Grid lines are a persistent pool, reused in place -- NOT destroyed and recreated every time
-- position/size changes. obj:destroy() (renderizable.cpp) only unregisters an object from
-- rendering; the actual C++/GPU resource release waits for Lua's GC to run its __gc metamethods,
-- "potentially many frames later" (verbatim from that function's own comment). Rebuilding all
-- ~42 lines from scratch on every width/depth/offset tweak was real, unbounded churn for what is
-- genuinely just a position update -- ln:set(vertices, segmentIndex) repositions an *existing*
-- line's geometry directly, so the pool only ever grows/shrinks by the exact delta when the
-- number of lines actually needed changes (grid count), never for a plain reposition.
function ensureGridLinePoolSize(n)
    while #tGridLines < n do
        local ln = line:new('3d', 0, 0, 0)
        ln:add({0, 0, 0, 0, 0, 0}) -- placeholder segment 1, so ln:set(..., 1) always has something to update
        table.insert(tGridLines, ln)
    end
    while #tGridLines > n do
        tGridLines[#tGridLines]:destroy()
        tGridLines[#tGridLines] = nil
    end
end

function rebuildGridVisual()
    -- Free mode still shows the grid (it just skips snapping when placing) -- the grid also
    -- borders Free-mode placement now, so hiding it there would leave that border invisible.
    -- Only the explicit "Show Grid" checkbox and having an active layer hide it.
    if not tMapOptions.bShowGrid or iSelectedLayer == 0 then
        ensureGridLinePoolSize(0)
        return
    end
    local layer = tLayers[iSelectedLayer]
    if not layer then
        ensureGridLinePoolSize(0)
        return
    end
    -- N cells need exactly N+1 boundary lines. The previous "halfLines = floor(count/2), draw
    -- -halfLines..halfLines" scheme could only ever produce an ODD number of lines, so it was
    -- structurally incapable of representing an odd cell count -- and floor(1*0.5)=0 clamped up
    -- to the same 1 as floor(2*0.5)=1, so count=1 and count=2 rendered identically. This scheme
    -- draws exactly nCellsX+1 / nCellsZ+1 lines, so every count >= 1 is distinct and matches its
    -- label (count=1 -> exactly one cell/segment shown).
    local nCellsX = math.max(1, tMapOptions.iGridCountX)
    local nCellsZ = math.max(1, tMapOptions.iGridCountZ)
    local w, d = tMapOptions.fGridCellWidthX, tMapOptions.fGridCellDepthZ
    local rot = getMapRotationRad()
    local cosr, sinr = math.cos(rot), math.sin(rot)

    local function rotXZ(x, z)
        return x * cosr - z * sinr, x * sinr + z * cosr
    end

    -- Line positions must land on the exact same integer*cellSize lattice that cellX/cellZ snap
    -- to (gridCellRange) -- NOT a float "-nCellsX*0.5*w..+nCellsX*0.5*w" center, which is only an
    -- integer multiple of w when nCellsX is even. For odd counts that float center lands half a
    -- cell off the lattice, so the whole visual grid would shift under already-placed meshes
    -- (which stay pinned to their absolute cellX*w world position, unaffected by count) instead of
    -- the grid consistently framing them. Confirmed bug: toggling "Width count" 20 -> 19 re-centered
    -- the lines while placed meshes correctly stayed put, making it look like the meshes had moved.
    local cxMin = gridCellRange(nCellsX)
    local czMin = gridCellRange(nCellsZ)
    local xLow, xHigh = cxMin * w, (cxMin + nCellsX) * w
    local zLow, zHigh = czMin * d, (czMin + nCellsZ) * d

    ensureGridLinePoolSize((nCellsX + 1) + (nCellsZ + 1))

    local idx = 1
    for i = 0, nCellsX do
        local x = (cxMin + i) * w
        local x1, z1 = rotXZ(x, zLow)
        local x2, z2 = rotXZ(x, zHigh)
        local ln = tGridLines[idx]
        ln:setPos(layer.offset.x, layer.fY, layer.offset.z)
        ln:set({x1, 0, z1, x2, 0, z2}, 1)
        ln:setColor(0.35, 0.35, 0.35, 0.63)
        idx = idx + 1
    end
    for i = 0, nCellsZ do
        local z = (czMin + i) * d
        local x1, z1 = rotXZ(xLow, z)
        local x2, z2 = rotXZ(xHigh, z)
        local ln = tGridLines[idx]
        ln:setPos(layer.offset.x, layer.fY, layer.offset.z)
        ln:set({x1, 0, z1, x2, 0, z2}, 1)
        ln:setColor(0.35, 0.35, 0.35, 0.63)
        idx = idx + 1
    end
end

------------------------------------------------------------------------------------------------------------------
-- Async mesh loading & progress
------------------------------------------------------------------------------------------------------------------

-- Tracks every fileName that has ever been loaded once (via a thumbnail bake, a Mesh Set preview,
-- or an earlier placement). MESH_MANAGER caches mesh data by fileName internally, so a *second*
-- `mesh:load()` of the same file is cheap (just spins up a new instance from the cached data,
-- no disk I/O) -- only the very first load of a given asset is potentially heavy and worth the
-- async/progress-modal treatment.
tMeshAlreadyLoaded = {}

function markMeshLoaded(fileName)
    tMeshAlreadyLoaded[fileName] = true
end

-- Synchronous load for interactive placement (Layer tab click, fill-layer): the asset is expected
-- to already be registered in the Mesh Set inventory, and in the common case has already been
-- loaded once for its thumbnail/preview (see markMeshLoaded call sites), so a plain load() is cheap
-- -- no async, no progress modal. Used directly by anything that places one object in response to a
-- single user action, where a modal popping up mid-click would be surprising.
function placeMeshSync(fileName, sType, coordType)
    local tObj = createMeshWithLightingSupport(function()
        if sType == 'mesh' then
            local m = mesh:new(coordType)
            return m:load(fileName) and m or nil
        else
            return tUtil.onAddMeshToEditor(fileName, false, coordType)
        end
    end)
    if tObj then markMeshLoaded(fileName) end
    return tObj
end

-- Loads a renderizable asset for placement. `mesh` assets use the (potentially heavy) async path
-- only the first time a given fileName is loaded; every subsequent placement of an already-loaded
-- mesh loads synchronously, since the engine's mesh cache makes that effectively instant. Other
-- (lighter) asset types always load synchronously. Always resolves through onDone(tObj), and always
-- tracked by tLoadProgress for the progress modal. Used for bulk/first-time loads (e.g. Load 3D
-- Scene) where a progress modal is actually wanted -- NOT for single interactive placements, see
-- placeMeshSync above.
function placeMeshAsync(fileName, sType, coordType, onDone)
    if sType == 'mesh' and not tMeshAlreadyLoaded[fileName] then
        local m = mesh:new(coordType)
        local wasEnabled = mbm.getLightState('3d').enabled
        if not wasEnabled then
            mbm.setLightEnabled('3d', true)
        end
        m:loadAsync(fileName, function(self_mesh, success)
            applyTabLighting()
            if success then markMeshLoaded(fileName) end
            tLoadProgress.iLoaded = tLoadProgress.iLoaded + 1
            onDone(success and self_mesh or nil)
        end)
    else
        local tObj = placeMeshSync(fileName, sType, coordType)
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
    if not tThumbGenRt then
        -- render2texture is itself a RENDERIZABLE placed in the world (inherits setPos/visible/etc,
        -- §6 of docs/lua-api.md) -- it is NOT an invisible offscreen-only object by default, it also
        -- draws its own quad wherever it's positioned. Park it far beyond any camera's far plane so
        -- that quad never actually appears, and force `alwaysRender = true` so the engine still runs
        -- its internal capture pass every frame regardless: `isRenderEnabled() == false` (a hidden/
        -- invisible object) makes the engine skip frustum-culling AND treat it as never-on-frustum,
        -- and RENDER_2_TEXTURE's own capture pass (core-manager-opengl_es.cpp's renderToTargets())
        -- is gated on that same on-frustum flag -- so naively hiding it via `.visible = false` would
        -- silently stop thumbnails from ever baking. Being physically beyond the far plane means the
        -- GPU's own clip test discards its quad, independent of that app-level flag.
        tThumbGenRt = render2texture:new('3d', 0, -1000000, 0)
        tThumbGenRt.alwaysRender = true
    end
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
    markMeshLoaded(entry.fileName)
    -- Same normalization as the preview/placement paths -- a baked default angle (e.g. Crate.msh's
    -- ~-32/4/0) would otherwise bake a tilted thumbnail that doesn't match how the asset actually
    -- looks once placed.
    tObj:setAngle(0, 0, 0)
    tThumbGenRt:add(tObj)
    local w, h, d = tObj:getAABB(true)
    local fitDist = math.max(w or 50, h or 50, d or 50) * 2.2
    -- Frame the mesh's true visual center, not its pivot (obj:getAABBCenter(), MBM_VERSION
    -- 6.9.0) -- for a mesh anchored at its base (e.g. a building), focusing on the pivot (its
    -- floor) would frame the thumbnail on its feet instead of centering the whole asset.
    local cx, cy, cz = tObj:getAABBCenter(true)
    local rtCam = tThumbGenRt:getCamera('3d')
    rtCam:setPos(cx + fitDist * 0.7, cy + fitDist * 0.5, cz + fitDist * 0.7)
    rtCam:setFocus(cx, cy, cz)
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

-- 8 corners / 12 triangles (2 per face) of a UNIT axis-aligned box (half-extent 0.5 on every
-- axis) -- same corner/winding convention as physic_editor.lua's makeBoxShape3d. Built at unit
-- size once and resized per-object via setScale(w,h,d) every frame (cheap: a transform update,
-- not a geometry rebuild) rather than rebuilding the mesh whenever an object's size changes.
function highlightBoxCorners()
    local h = 0.5
    return {
        {x=-h,y=-h,z= h}, {x=-h,y= h,z= h}, {x= h,y= h,z= h}, {x= h,y=-h,z= h},
        {x=-h,y=-h,z=-h}, {x=-h,y= h,z=-h}, {x= h,y= h,z=-h}, {x= h,y=-h,z=-h},
    }
end

function highlightBoxTriangleFaces(corners)
    local a,b,c,d,e,f,g,h = corners[1],corners[2],corners[3],corners[4],corners[5],corners[6],corners[7],corners[8]
    return {
        {a,b,c},{a,c,d}, -- front
        {h,g,f},{h,f,e}, -- back
        {e,f,b},{e,b,a}, -- left
        {d,c,g},{d,g,h}, -- right
        {b,f,g},{b,g,c}, -- top
        {e,a,d},{e,d,h}, -- bottom
    }
end

iHighlightShapeNickName = 1

-- A solid, translucent box (not a wireframe) -- mirrors the tinted-overlay visual language
-- tilemap_editor.lua and physic_editor.lua already use for hover/selection feedback, instead of
-- the plain yellow wireframe this used to draw.
function makeHighlightBoxShape()
    local corners = highlightBoxCorners()
    local faces = highlightBoxTriangleFaces(corners)
    local verts = {}
    for _, tri in ipairs(faces) do
        for _, p in ipairs(tri) do
            table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z)
        end
    end
    iHighlightShapeNickName = iHighlightShapeNickName + 1
    local shp = shape:new('3d', 0, 0, 0)
    shp:create(verts, nil, string.format('scene3d_highlight_%d', iHighlightShapeNickName))
    return shp
end

function destroyHighlightShape(tPlaced)
    if tPlaced.tHighlightShape then
        tPlaced.tHighlightShape:destroy()
        tPlaced.tHighlightShape = nil
    end
end

-- Literal Euclidean distance from the camera to a world point -- mbm.to3d(sx,sy,depth)'s `depth`
-- is exactly this (confirmed empirically: depth=0 returns the camera's own position, independent
-- of sx,sy), so this is the value that must feed any unprojection meant to land near a specific
-- known world point (see isPointInScreenRectAtObjectDepth below).
function distanceFromCamera(px, py, pz)
    local camPos = camera3d:getPos()
    local dx, dy, dz = px - camPos.x, py - camPos.y, pz - camPos.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

-- Only recomputes WHICH mesh the cursor is over (a real screen-space hit test, obj:collide) --
-- the actual highlight visuals (color/blink/visibility) are updated separately, every frame
-- regardless of whether the hover target changed, by updateSelectionHighlights() below (blinking
-- needs a per-frame update even while hovering nothing new).
function updateHoverHighlight(sx, sy)
    if sActiveTab ~= 'layer' then
        tHoveredPlaced = nil
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
    tHoveredPlaced = bestIndex
end

-- Called every frame (Layer tab only) -- shows a translucent box over every placed mesh that's
-- either hovered or selected: green while it belongs to the currently active layer, red
-- otherwise (mirrors the active-layer concept the grid/placement already enforces), fixed alpha
-- 0.4 while merely hovered, blinking between alpha 0.2 and 0.5 while selected (a plain per-frame
-- sine wave -- no shader needed for this). Selected takes priority over hover when both apply.
function updateSelectionHighlights()
    if sActiveTab ~= 'layer' then
        for _, tPlaced in ipairs(tPlacedMeshes) do
            if tPlaced.tHighlightShape then tPlaced.tHighlightShape.visible = false end
        end
        return
    end
    local blinkAlpha = 0.35 + 0.15 * math.sin(mbm.getTimeRun() * 4)
    for i, tPlaced in ipairs(tPlacedMeshes) do
        local isHovered = (i == tHoveredPlaced)
        -- Skip a mesh whose own layer is currently hidden (or is otherwise not rendering) -- a
        -- tinted box floating over nothing would be confusing, not helpful.
        if (tPlaced.isSelected or isHovered) and tPlaced.tObj and tPlaced.tObj.visible then
            if not tPlaced.tHighlightShape then
                tPlaced.tHighlightShape = makeHighlightBoxShape()
            end
            local w, h, d = tPlaced.tObj:getAABB(true)
            local cx, cy, cz = tPlaced.tObj:getAABBCenter()
            tPlaced.tHighlightShape:setPos(cx, cy, cz)
            tPlaced.tHighlightShape:setScale(w, h, d)
            local belongsToActiveLayer = tPlaced.layerIndex == iSelectedLayer
            local r, g = belongsToActiveLayer and 0 or 1, belongsToActiveLayer and 1 or 0
            tPlaced.tHighlightShape:setColor(r, g, 0, tPlaced.isSelected and blinkAlpha or 0.4)
            tPlaced.tHighlightShape.visible = true
        elseif tPlaced.tHighlightShape then
            tPlaced.tHighlightShape.visible = false
        end
    end
end

------------------------------------------------------------------------------------------------------------------
-- Multi-select (Layer tab): click-to-select, Shift+drag rectangle-select, rotate/delete tools
------------------------------------------------------------------------------------------------------------------

-- A screen-space rectangle does not map to an axis-aligned rectangle in world space once the
-- camera can orbit/tilt (unlike tilemap_editor.lua's fixed top-down 2D camera, where screen and
-- world rects coincide) -- it maps to a general quadrilateral. Unprojecting onto a fixed world
-- Y-plane (screenToWorldOnLayerPlane's own trick) is unreliable here: as a screen ray approaches
-- parallel-to-the-plane (looking toward the horizon, which the top of the viewport can easily be
-- for a modestly-elevated orbit camera), the ray/plane intersection point swings wildly, which
-- empirically produced badly-skewed quads for perfectly ordinary selection rectangles (confirmed
-- by hand-checking the actual unprojected corners against a real orbit camera pose).
--
-- Unprojecting each rectangle corner at the object's true camera-distance (distanceFromCamera)
-- keeps every corner on the same "shell" the object sits on, which stays well-defined for any
-- camera pose (no near-parallel-to-plane singularity like the Y-plane approach above suffers
-- from). Testing X/Y containment on that shell (rather than X/Z) mirrors obj:collide's own
-- simplification of disregarding the matching depth axis, which is fine since it's fixed equal by
-- construction. Note this is only reliable for a rectangle with reasonable screen-space extent --
-- a single-point version of this same trick was tried for hover/click hit-testing and rejected: at
-- the tight margins of one object's own footprint, small unprojection error (a few percent of the
-- camera distance) is enough to miss, so hover-pick keeps using the native, pre-existing
-- obj:collide(x,y) instead (see updateHoverHighlight).
function isPointInScreenRectAtObjectDepth(px, py, pz, xStart, yStart, xEnd, yEnd)
    local depth = distanceFromCamera(px, py, pz)
    local screenCorners = {
        {xStart, yStart}, {xEnd, yStart}, {xEnd, yEnd}, {xStart, yEnd},
    }
    local poly = {}
    for _, sc in ipairs(screenCorners) do
        local wx, wy, wz = mbm.to3d(sc[1], sc[2], depth)
        table.insert(poly, {wx, wy})
    end
    local function cross(o, a, b)
        return (a[1] - o[1]) * (b[2] - o[2]) - (a[2] - o[2]) * (b[1] - o[1])
    end
    local sign = nil
    for i = 1, 4 do
        local a, b = poly[i], poly[(i % 4) + 1]
        local c = cross(a, b, {px, py})
        if math.abs(c) > 1e-6 then
            local isPositive = c > 0
            if sign == nil then
                sign = isPositive
            elseif sign ~= isPositive then
                return false
            end
        end
    end
    return true
end

function updateRectSelectionLine(xStart, yStart, xEnd, yEnd)
    local pts = {
        xStart, yStart, xEnd, yStart,
        xEnd, yEnd, xStart, yEnd,
        xStart, yStart,
    }
    lnRectSelection:set(pts, 1)
end

-- Called once on mouse-up (matches tilemap_editor.lua: the drag only draws a preview, selection
-- itself is finalized a single time on release, not recomputed every dragged frame).
function finalizeRectSelection(xStart, yStart, xEnd, yEnd)
    if sActiveTab ~= 'layer' then return end
    if math.abs(xEnd - xStart) < 2 and math.abs(yEnd - yStart) < 2 then
        -- Degenerate (near-zero-area) drag -- a Shift+click with no real drag. A zero-area
        -- rectangle has no well-defined "inside" for the quad test above (every point would
        -- trivially satisfy it), so fall back to whatever's directly under the cursor, same as a
        -- plain click.
        for i, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = (i == tHoveredPlaced) end
        return
    end
    for _, tPlaced in ipairs(tPlacedMeshes) do
        tPlaced.isSelected = tPlaced.tObj ~= nil and isPointInScreenRectAtObjectDepth(
            tPlaced.tObj.x, tPlaced.tObj.y, tPlaced.tObj.z, xStart, yStart, xEnd, yEnd)
    end
end

-- A plain click replaces the selection outright with whatever's hovered (or clears it, over empty
-- space). Ctrl+click instead toggles just the hovered mesh's own selection state, leaving every
-- other selected mesh alone -- the "add one more to the selection" gesture, complementing
-- Shift+drag's rectangle-select (which also replaces, over an area rather than a single mesh).
function handleSelectClickAt()
    if sActiveTab ~= 'layer' then return end
    if keyControlPressed then
        if tHoveredPlaced then
            local tPlaced = tPlacedMeshes[tHoveredPlaced]
            tPlaced.isSelected = not tPlaced.isSelected
        end
        return
    end
    for i, tPlaced in ipairs(tPlacedMeshes) do
        tPlaced.isSelected = (i == tHoveredPlaced)
    end
end

function unselectAllPlacedMeshes()
    for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = false end
end

-- Both Orthogonal's snap step and Isometric's (just phase-shifted by 45 degrees, see
-- snapRotation) are 90 degrees apart, so a single +-90 degree step is exactly one grid rotation
-- and never drifts off the active mode's lattice; used for Free mode too as the "quick" tool-
-- window rotate step (a precise arbitrary angle is out of scope for this button).
function rotateSelectedMeshes(sign)
    local step = math.pi * 0.5
    for _, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected then
            tPlaced.rotationY = (tPlaced.rotationY or 0) + sign * step
            syncPlacedMeshTransform(tPlaced)
        end
    end
end

-- Bottom-right floating tool window, mirrors tilemap_editor.lua's showTileTools -- appears only
-- while at least one placed mesh is selected, offering the same Rotate Right / Rotate Left /
-- Delete actions (no Flip -- that's a 2D sprite-image operation with no 3D equivalent) against
-- every currently-selected mesh at once.
function showMeshTools()
    if sActiveTab ~= 'layer' then return end
    local iSelectedCount = 0
    for _, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected then iSelectedCount = iSelectedCount + 1 end
    end
    if iSelectedCount == 0 then return end

    local item_width = 150
    local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoSavedSettings', 'ImGuiWindowFlags_NoFocusOnAppearing', 'ImGuiWindowFlags_NoNav'}
    tImGui.SetNextWindowBgAlpha(0.75)
    local iW, iH = mbm.getRealSizeScreen()
    tImGui.SetNextWindowPos({x = iW - tToolsMeshSize.x, y = iH - tToolsMeshSize.y}, 0, {x = 0, y = 0})
    tImGui.Begin('##ToolsMeshLayer', false, tImGui.Flags(flags))
    if iSelectedCount > 1 then
        tImGui.Text(string.format('Total Selected (%d)', iSelectedCount))
    else
        tImGui.Text(tLang.L('total_selected_1'))
    end
    if tImGui.Button(tLang.L('rotate_right'), tUtil.getResponsiveItemSize(item_width)) then
        rotateSelectedMeshes(1)
    end
    if tImGui.Button(tLang.L('rotate_left'), tUtil.getResponsiveItemSize(item_width)) then
        rotateSelectedMeshes(-1)
    end
    if tImGui.Button(tLang.L('delete_btn'), tUtil.getResponsiveItemSize(item_width)) then
        for i = #tPlacedMeshes, 1, -1 do
            if tPlacedMeshes[i].isSelected then removePlacedMesh(i) end
        end
    end
    tToolsMeshSize = tImGui.GetWindowSize()
    tImGui.End()
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

-- Per-asset offset (Mesh property tab, keyed by fileName) has three independent parts, each
-- ADDED/MULTIPLIED on top of whatever the placement/grid/rotate-tool logic already computed for
-- this specific instance -- correcting a whole asset's authored pivot/rotation/scale in one place
-- fixes every placed copy of it at once, same reasoning as the original position-only offset.
function getMeshOffset(fileName)
    local offset = tMeshOffsets[fileName]
    return {
        x = (offset and offset.x) or 0, y = (offset and offset.y) or 0, z = (offset and offset.z) or 0,
        rx = (offset and offset.rx) or 0, ry = (offset and offset.ry) or 0, rz = (offset and offset.rz) or 0,
        sx = (offset and offset.sx) or 1, sy = (offset and offset.sy) or 1, sz = (offset and offset.sz) or 1,
    }
end

function syncPlacedMeshTransform(tPlaced)
    if not tPlaced.tObj then return end
    local x, y, z = resolvePlacedMeshWorldPos(tPlaced)
    local offset = getMeshOffset(tPlaced.fileName)
    tPlaced.tObj:setPos(x + offset.x, y + offset.y, z + offset.z)
    tPlaced.tObj:setAngle(offset.rx, (tPlaced.rotationY or 0) + offset.ry, offset.rz)
    tPlaced.tObj:setScale(tPlaced.scale.x * offset.sx, tPlaced.scale.y * offset.sy, tPlaced.scale.z * offset.sz)
end

-- Placed meshes are the scene being built (Map / Map edition) -- Mesh View is only ever supposed
-- to show the single currently-previewed asset, never the actual placed content. Without this,
-- every placed instance kept rendering there too (a real bug: visible in Mesh View despite that
-- tab's own "Placed Meshes" count having nothing to do with it).
function applyPlacedMeshVisibility(tPlaced)
    if not tPlaced.tObj then return end
    local layer = tLayers[tPlaced.layerIndex]
    local layerVisible = (not layer) or layer.visible
    tPlaced.tObj.visible = layerVisible and (sActiveTab ~= 'mesh_set')
end

function updateAllPlacedMeshVisibility()
    for _, tPlaced in ipairs(tPlacedMeshes) do
        applyPlacedMeshVisibility(tPlaced)
    end
end

-- Re-derives world position (and, under an active snap-scale mode, scale) for every placed mesh
-- from its stored cell index -- needed whenever the grid definition itself changes (cell size, map
-- type, snap-scale mode), since placed meshes are stored by cell index, not a frozen world position.
function resyncAllPlacedMeshes()
    for _, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.tObj then
            if tMapOptions.sSnapScaleMode ~= 'none' then
                local sx, sy, sz = computeSnapScale(tPlaced.tObj)
                tPlaced.scale = {x = sx, y = sy, z = sz}
            end
            syncPlacedMeshTransform(tPlaced)
        end
    end
end

-- `bSync`: true for direct interactive placement (Layer tab click, fill-layer) -- always
-- synchronous, no progress modal. false/nil (the default) goes through the async-capable path,
-- with a progress modal, used for bulk/first-time loads like "Load 3D Scene".
function addPlacedMesh(fileName, sType, layerIndex, cellX, cellZ, freeX, freeZ, bSync)
    local tPlaced = {
        fileName = fileName, type = sType, layerIndex = layerIndex,
        cellX = cellX or 0, cellZ = cellZ or 0,
        freeX = freeX or 0, freeZ = freeZ or 0,
        bAttachedToLayer = true, freeY = 0,
        rotationY = 0, scale = {x = 1, y = 1, z = 1},
        isSelected = false, tObj = nil,
    }
    table.insert(tPlacedMeshes, tPlaced)
    local function onLoaded(tObj)
        if tObj then
            tPlaced.tObj = tObj
            -- Some mesh files embed a non-zero default angle (confirmed: e.g. Crate.msh loads at
            -- roughly -32/4/0 degrees) -- getAABB(true) measures the CURRENT world-space AABB,
            -- which for a tilted object is inflated/skewed relative to its true footprint. Reset
            -- to the orientation this placement will actually use (0, rotationY, 0) *before*
            -- measuring, or "fit to cell" scale is computed against the wrong footprint.
            tObj:setAngle(0, tPlaced.rotationY or 0, 0)
            local sx, sy, sz = computeSnapScale(tObj)
            tPlaced.scale = {x = sx, y = sy, z = sz}
            syncPlacedMeshTransform(tPlaced)
            applyPlacedMeshVisibility(tPlaced)
        end
    end
    if bSync then
        onLoaded(placeMeshSync(fileName, sType, '3d'))
    else
        beginLoadProgress(tLoadProgress.iTotal + 1, tUtil.getShortName(fileName))
        placeMeshAsync(fileName, sType, '3d', onLoaded)
    end
    return tPlaced
end

function removePlacedMesh(index)
    local tPlaced = tPlacedMeshes[index]
    if tPlaced and tPlaced.tObj then
        tPlaced.tObj:destroy()
    end
    if tPlaced then
        destroyHighlightShape(tPlaced)
    end
    table.remove(tPlacedMeshes, index)
end

-- Registers a single file into the Mesh Set list (if not already present) so it also
-- becomes selectable later from the Mesh Set tab / Mesh Selector window.
function registerMeshSetEntry(fileName)
    for _, e in ipairs(tMeshSetEntries) do
        if e.fileName == fileName then return e end
    end
    local tInfo = meshDebug and meshDebug:getInfo(fileName) or nil
    if not tInfo or not tInfo.type then return nil end
    local entry = { fileName = fileName, type = tInfo.type, ext = tInfo.ext, isSelected = false, thumbState = nil }
    table.insert(tMeshSetEntries, entry)
    return entry
end

-- Only the directory (not the full file+extension) of the last pick is kept as the next dialog's
-- default -- passing a full "name.ext" back into a multi-extension open dialog as its default is
-- what native file pickers on some desktops mis-render (e.g. re-appending the filter's extension).
-- Reopening into the same folder is all the convenience this actually needs.
sLastMeshAddDir = ''

local function dirOf(sPath)
    return sPath:match('^(.*)[\\/][^\\/]*$') or ''
end

-- Direct "Add Mesh" entry point (mirrors scene_editor2d.lua's onAddMesh): opens a native
-- multi-file picker and registers every chosen asset into the Mesh Set inventory only --
-- it does NOT place an instance into the scene. Placement happens explicitly afterward, from
-- the Layer tab's Mesh Selector (pick the asset there, then click in the scene to place it).
function onAddMeshDirect()
    local fileName = mbm.openMultiFile(sLastMeshAddDir,
        'tile', 'spt', 'ptl', 'png', 'msh', 'fnt', 'jpeg', 'jpg', 'bmp', 'gif', 'psd', 'pic', 'pnm', 'hdr', 'tga', 'tif')
    if not fileName then return end

    local lastEntry = nil
    local function addOne(sFile)
        local entry = registerMeshSetEntry(sFile)
        if not entry then
            tUtil.showMessageWarn(tLang.L('failed_to_add_mesh'))
            return
        end
        sLastMeshAddDir = dirOf(sFile)
        lastEntry = entry
    end

    if type(fileName) == 'string' then
        addOne(fileName)
    elseif type(fileName) == 'table' then
        for _, f in ipairs(fileName) do
            addOne(f)
        end
    end

    -- Select the last-added asset for placement so the natural next step (switch to the Layer
    -- tab, click in the scene) works immediately without an extra trip to the Mesh Selector.
    if lastEntry then
        sMeshSelectedForPlacement = lastEntry.fileName
    end
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
            ln:setColor(0, 1, 1, 1)
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
    local ret, current_item = tImGui.Combo(tLang.L('map_type') .. '##MapType3d', tComboMapTypeIndexOf(tMapOptions.sMapType), tComboMapType3dCodes)
    if ret then
        tMapOptions.sMapType = tComboMapType3dCodes[current_item]
        rebuildGridVisual()
        resyncAllPlacedMeshes()
    end

    local showGrid = tImGui.Checkbox(tLang.L('show_grid') .. '##ShowGrid', tMapOptions.bShowGrid)
    if showGrid ~= tMapOptions.bShowGrid then
        tMapOptions.bShowGrid = showGrid
        rebuildGridVisual()
    end

    -- Cell size of 0 (or negative) is not just meaningless, it's a division-by-zero hazard in
    -- worldToGridCell -- and there was previously no floor at all on these two fields.
    --
    -- rebuildGridVisual() now repositions its existing line pool in place (ln:set(...)) instead
    -- of destroying and recreating every line on every call, so updating live on every changed
    -- frame here -- even while holding a +/- spinner's auto-repeat -- is cheap and safe: no
    -- destroy/create churn for a plain reposition, and grid-count changes only grow/shrink the
    -- pool by the actual delta, not a full rebuild. See rebuildGridVisual's own comment.
    --
    -- These fields are shown/editable in ALL three map types now, not just Orthogonal/Isometric --
    -- Free mode still has no snapping, but it's now bordered by this same grid (see
    -- isWorldPosWithinGridBounds), so its extent must stay adjustable there too.
    local c1, w = tImGui.InputFloat(tLang.L('grid_width_x') .. '##GridWidthX', tMapOptions.fGridCellWidthX, 1, 10, '%.2f')
    local c2, d = tImGui.InputFloat(tLang.L('grid_depth_z') .. '##GridDepthZ', tMapOptions.fGridCellDepthZ, 1, 10, '%.2f')
    if c1 then tMapOptions.fGridCellWidthX = math.max(MIN_GRID_CELL_SIZE, w) end
    if c2 then tMapOptions.fGridCellDepthZ = math.max(MIN_GRID_CELL_SIZE, d) end
    if c1 or c2 then
        rebuildGridVisual()
        -- Placed meshes are stored by cell index (cellX/cellZ), not world position -- their
        -- world position (and, under a snap-scale mode, their scale) is derived from the cell
        -- size, so it must be recomputed here too, not just the grid's visual lines.
        resyncAllPlacedMeshes()
    end

    -- How many cells wide/deep the visible grid (and "fill layer") span -- previously fixed
    -- at 10 half-lines (21x21) regardless of any setting.
    local c3, cx = tImGui.InputInt(tLang.L('grid_count_x') .. '##GridCountX', tMapOptions.iGridCountX, 1, 10)
    local c4, cz = tImGui.InputInt(tLang.L('grid_count_z') .. '##GridCountZ', tMapOptions.iGridCountZ, 1, 10)
    if c3 then tMapOptions.iGridCountX = math.max(MIN_GRID_COUNT, cx) end
    if c4 then tMapOptions.iGridCountZ = math.max(MIN_GRID_COUNT, cz) end
    if c3 or c4 then
        rebuildGridVisual()
        -- Shrinking the grid can leave previously-placed meshes past the new edge -- the grid
        -- is the bounds of the scene, so anything now outside it is removed, not left floating.
        removePlacedMeshesOutsideGrid()
    end

    if tMapOptions.sMapType ~= 'Free' then
        -- Snapping/scaling to the grid is meaningless in Free mode (no snapping applies there at
        -- all), so this combo stays exclusive to the two grid-snapped modes.
        local retSnap, curSnap = tImGui.Combo(tLang.L('snap_scale_mode') .. '##SnapScaleMode', tComboSnapScaleModeIndexOf(tMapOptions.sSnapScaleMode), tComboSnapScaleModeLabel)
        if retSnap then
            tMapOptions.sSnapScaleMode = tComboSnapScaleMode[curSnap]
            resyncAllPlacedMeshes()
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
                tObj.type = tComboObjectType3d[cur2]
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
    for i, s in ipairs(tComboMapType3dCodes) do
        if s == sType then return i end
    end
    return 1
end

function tComboSnapScaleModeIndexOf(sMode)
    for i, s in ipairs(tComboSnapScaleMode) do
        if s == sMode then return i end
    end
    return 1
end

function tComboObjectTypeIndexOf(sType)
    for i, s in ipairs(tComboObjectType3d) do
        if s == sType then return i end
    end
    return 1
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
        markMeshLoaded(entry.fileName)
        -- Reflect this asset's per-asset offset (Mesh property tab) in the preview too -- it used
        -- to always sit at the literal world origin regardless of the configured offset, which is
        -- why editing the offset looked like it did nothing here (it silently only ever affected
        -- newly-placed instances going forward, never anything already visible).
        local offset = getMeshOffset(entry.fileName)
        tPreviewMesh3d:setPos(offset.x, offset.y, offset.z)
        -- Some mesh files embed a non-zero default angle (confirmed: e.g. Crate.msh loads at
        -- roughly -32/4/0 degrees) -- normalize the BASE angle to (0,0,0) here, same reason as the
        -- placement path (addPlacedMesh), so the preview matches how the asset will actually look
        -- once placed (always at angle (0, rotationY, 0)) before layering the rotation offset on
        -- top of that, instead of showing it at whatever tilt the source file happens to bake in.
        tPreviewMesh3d:setAngle(offset.rx, offset.ry, offset.rz)
        tPreviewMesh3d:setScale(offset.sx, offset.sy, offset.sz)
        -- Deliberately does NOT touch cam3d (focus/distance) here -- selecting a different mesh in
        -- this tab must not re-frame/move the camera out from under a view the user already set up.
        -- Every previewed mesh is placed at the same (0,0,0), so the existing view stays valid.
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
        sMeshSetFilterType = tComboMeshSetFilter[curF]
    end

    if iPreviewedMeshSetIndex > 0 then
        local entry = getFilteredMeshSetEntries()[iPreviewedMeshSetIndex]
        if entry then
            tImGui.Separator()
            tImGui.Text(tLang.L('mesh_offset_fmt'):format(tUtil.getShortName(entry.fileName)))
            local offset = getMeshOffset(entry.fileName)

            tImGui.Text(tLang.L('offset_position'))
            local o1, ox = tImGui.InputFloat(tLang.L('axis_x') .. '##offset_x', offset.x, 1, 10, '%.2f')
            local o2, oy = tImGui.InputFloat(tLang.L('axis_y') .. '##offset_y', offset.y, 1, 10, '%.2f')
            local o3, oz = tImGui.InputFloat(tLang.L('axis_z') .. '##offset_z', offset.z, 1, 10, '%.2f')

            -- Stored/applied in radians (matching rotationY and every other angle in this editor)
            -- but edited in degrees here -- degrees are what a human reasonably types by hand.
            tImGui.Text(tLang.L('offset_rotation'))
            local o4, orxDeg = tImGui.InputFloat(tLang.L('axis_x') .. '##offset_rx', math.deg(offset.rx), 1, 10, '%.2f')
            local o5, oryDeg = tImGui.InputFloat(tLang.L('axis_y') .. '##offset_ry', math.deg(offset.ry), 1, 10, '%.2f')
            local o6, orzDeg = tImGui.InputFloat(tLang.L('axis_z') .. '##offset_rz', math.deg(offset.rz), 1, 10, '%.2f')

            -- A multiplier on top of whatever scale placement/snap-scale already computed, not a
            -- replacement for it -- 1.0 (the default) means "no change".
            tImGui.Text(tLang.L('offset_scale'))
            local o7, osx = tImGui.InputFloat(tLang.L('axis_x') .. '##offset_sx', offset.sx, 0.1, 1, '%.2f')
            local o8, osy = tImGui.InputFloat(tLang.L('axis_y') .. '##offset_sy', offset.sy, 0.1, 1, '%.2f')
            local o9, osz = tImGui.InputFloat(tLang.L('axis_z') .. '##offset_sz', offset.sz, 0.1, 1, '%.2f')

            if o1 or o2 or o3 or o4 or o5 or o6 or o7 or o8 or o9 then
                tMeshOffsets[entry.fileName] = {
                    x = ox, y = oy, z = oz,
                    rx = math.rad(orxDeg), ry = math.rad(oryDeg), rz = math.rad(orzDeg),
                    sx = math.max(osx, 0.001), sy = math.max(osy, 0.001), sz = math.max(osz, 0.001),
                }
                -- Reflect the change immediately, not just for future placements: the live
                -- preview (positioned/rotated/scaled by this offset -- see updatePreviewMesh3d)
                -- and every already-placed instance of this same asset, in every tab.
                local newOffset = tMeshOffsets[entry.fileName]
                if tPreviewMesh3d then
                    tPreviewMesh3d:setPos(newOffset.x, newOffset.y, newOffset.z)
                    tPreviewMesh3d:setAngle(newOffset.rx, newOffset.ry, newOffset.rz)
                    tPreviewMesh3d:setScale(newOffset.sx, newOffset.sy, newOffset.sz)
                end
                for _, tPlaced in ipairs(tPlacedMeshes) do
                    if tPlaced.fileName == entry.fileName then
                        syncPlacedMeshTransform(tPlaced)
                    end
                end
            end
        end
    end

    tImGui.Separator()
    local tFiltered = getFilteredMeshSetEntries()
    local winWidth = tImGui.GetWindowWidth()
    local xLast = tImGui.GetCursorPosX()
    for n, entry in ipairs(tFiltered) do
        local texInfo = getOrCreateThumbnail(entry)
        local size = {x = 90, y = 90}
        if texInfo then
            local selected = (iPreviewedMeshSetIndex == n)
            if selected then
                tImGui.PushStyleColor('ImGuiCol_Button', {r = 0, g = 1, b = 0, a = 1})
            end
            if tImGui.ImageButton('mesh_set_btn_' .. n, texInfo, size, {x = 0, y = 0}, {x = 1, y = 1}) then
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
        if xLast < winWidth - size.x then
            tImGui.SameLine()
        else
            xLast = tImGui.GetCursorPosX()
        end
    end
end

function tComboMeshSetFilterIndexOf(sType)
    for i, s in ipairs(tComboMeshSetFilter) do
        if s == sType then return i end
    end
    return 1
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
            tImGui.PushStyleColor('ImGuiCol_Text', {r = 0, g = 1, b = 0, a = 1})
            tImGui.Text(tLang.L('active'))
            tImGui.PopStyleColor(1)
        end
        if isOpen then
            if tImGui.Button(tLang.L('select') .. '##select_layer' .. i) then
                iSelectedLayer = i
                rebuildGridVisual()
            end

            local vis = tImGui.Checkbox(tLang.L('visible') .. '##layer_visible' .. i, layer.visible)
            if vis ~= layer.visible then
                layer.visible = vis
                for _, tPlaced in ipairs(tPlacedMeshes) do
                    if tPlaced.layerIndex == i then
                        applyPlacedMeshVisibility(tPlaced)
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
    local show = tImGui.Checkbox(tLang.L('mesh_selector_separated'), bShowMeshSelector)
    if show ~= bShowMeshSelector then bShowMeshSelector = show end

    tImGui.Separator()
    tImGui.Text(tLang.L('placed_meshes_fmt'):format(#tPlacedMeshes))
    for i, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected then
            tImGui.PushStyleColor('ImGuiCol_Text', {r = 0, g = 1, b = 0, a = 1})
        end
        local rowOpen = tImGui.TreeNodeEx(tUtil.getShortName(tPlaced.fileName) .. '##placed_tree' .. i)
        if tPlaced.isSelected then
            tImGui.PopStyleColor(1)
        end
        if rowOpen then
            local sel = tImGui.Checkbox(tLang.L('selected') .. '##placed_sel' .. i, tPlaced.isSelected)
            if sel ~= tPlaced.isSelected then tPlaced.isSelected = sel end

            if tMapOptions.sMapType == 'Free' then
                local attach = tImGui.Checkbox(tLang.L('attached_to_layer') .. '##placed_attach' .. i, tPlaced.bAttachedToLayer)
                if attach ~= tPlaced.bAttachedToLayer then
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
        local winWidth = tImGui.GetWindowWidth()
        local xLast = tImGui.GetCursorPosX()
        for n, entry in ipairs(tFiltered) do
            local texInfo = getOrCreateThumbnail(entry)
            local size = {x = iSizeMeshOnSelector, y = iSizeMeshOnSelector}
            if texInfo then
                local selected = (sMeshSelectedForPlacement == entry.fileName)
                if selected then
                    tImGui.PushStyleColor('ImGuiCol_Button', {r = 0, g = 1, b = 0, a = 1})
                end
                if tImGui.ImageButton('mesh_selector_btn_' .. n, texInfo, size, {x = 0, y = 0}, {x = 1, y = 1}) then
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
            if xLast < winWidth - size.x then
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
    -- Fill exactly nCellsX * nCellsZ cells (tMapOptions.iGridCountX/Z), matching
    -- rebuildGridVisual's extent -- see gridCellRange's own comment for why this must be the
    -- one shared definition of the cell-index span rather than a locally reimplemented copy.
    local cxMin, cxMax = gridCellRange(math.max(1, tMapOptions.iGridCountX))
    local czMin, czMax = gridCellRange(math.max(1, tMapOptions.iGridCountZ))
    for cx = cxMin, cxMax do
        for cz = czMin, czMax do
            local existingIndex = findPlacedMeshAtCell(iSelectedLayer, cx, cz)
            if existingIndex then removePlacedMesh(existingIndex) end
            addPlacedMesh(fileName, entry.type, iSelectedLayer, cx, cz, nil, nil, true)
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

------------------------------------------------------------------------------------------------------------------
-- Main windows
------------------------------------------------------------------------------------------------------------------

function setActiveTab(sTab)
    sActiveTab = sTab
    cam3d = tCamByTab[sTab]
    -- The Mesh View preview object is a real renderizable placed at the world origin -- without
    -- this it kept rendering in Map/Map edition too (a real bug: it showed up in the scene even
    -- though nothing had actually been placed, e.g. "Placed Meshes (0)" was still correct).
    if tPreviewMesh3d then
        tPreviewMesh3d.visible = (sTab == 'mesh_set')
    end
    updateAllPlacedMeshVisibility()
end

function main_tab_bar()
    if not bEnableMainTabBar then return end
    tUtil.setInitialWindowPositionLeft(tLang.L(tWindowsTitle.title_scene3d), 0, 0, 260, 260)
    local isOpen = tImGui.Begin(tLang.L(tWindowsTitle.title_scene3d), true, 0)
    if isOpen then
        local item_width = tUtil.getResponsiveItemWidth(200)
        if tImGui.BeginTabBar('##TabBar3d', 0) then
            if tImGui.BeginTabItem(tLang.L('tab_map')) then
                setActiveTab('map')
                drawMapTab(item_width)
                tImGui.EndTabItem()
            end
            if tImGui.BeginTabItem(tLang.L('tab_mesh_set')) then
                setActiveTab('mesh_set')
                drawMeshSetTab(item_width)
                tImGui.EndTabItem()
            end
            if tImGui.BeginTabItem(tLang.L('tab_layer')) then
                setActiveTab('layer')
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

        if tImGui.BeginMenu(tLang.L('menu_mesh')) then
            if tImGui.MenuItem(tLang.L('add_mesh'), 'Ctrl+M') then
                onAddMeshDirect()
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
            local inv = tImGui.Checkbox(tLang.L('invert_width_height') .. '##world', tOptionsEditor.bInvertResolution)
            if inv ~= tOptionsEditor.bInvertResolution then tOptionsEditor.bInvertResolution = inv end
            local tResStr = {}
            for i, r in ipairs(tResolution) do
                tResStr[i] = string.format('%dx%d (%s)', r.x, r.y, r.comment)
            end
            local retR, curR = tImGui.Combo('##ComboResolutionWorld', tOptionsEditor.iIndexResolution, tResStr)
            if retR then tOptionsEditor.iIndexResolution = curR end

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
            local invL = tImGui.Checkbox(tLang.L('invert_width_height') .. '##launch', tOptionsLaunch.bInvertResolution)
            if invL ~= tOptionsLaunch.bInvertResolution then tOptionsLaunch.bInvertResolution = invL end
            local retL, curL = tImGui.Combo('##ComboResolutionLaunch', tOptionsLaunch.iIndexResolution, tResStr)
            if retL then tOptionsLaunch.iIndexResolution = curL end

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
            local show = tImGui.Checkbox(tLang.L('show_mesh_map'), bEnableMainTabBar)
            if show ~= bEnableMainTabBar then bEnableMainTabBar = show end

            tImGui.Separator()
            local origin = tImGui.Checkbox(tLang.L('enable_origin_lines'), bEnableOriginLines)
            if origin ~= bEnableOriginLines then
                bEnableOriginLines = origin
                if tOriginLine3dX then tOriginLine3dX.visible = origin end
                if tOriginLine3dY then tOriginLine3dY.visible = origin end
                if tOriginLine3dZ then tOriginLine3dZ.visible = origin end
            end

            tLang.renderLanguageSubmenu()

            if tImGui.BeginMenu(tLang.L('background_color')) then
                local sz       = tImGui.GetTextLineHeight()
                local rounding = 0
                local flags    = 0

                local tColors = {
                    {'default', tUtil.tColorBackground},
                    {'white',   {r = 1, g = 1, b = 1, a = 1}},
                    {'black',   {r = 0, g = 0, b = 0, a = 1}},
                    {'red',     {r = 1, g = 0, b = 0, a = 1}},
                    {'green',   {r = 0, g = 1, b = 0, a = 1}},
                    {'blue',    {r = 0, g = 0, b = 1, a = 1}},
                    {'cyan',    {r = 0, g = 1, b = 1, a = 1}},
                    {'yellow',  {r = 1, g = 1, b = 0, a = 1}},
                    {'magenta', {r = 1, g = 0, b = 1, a = 1}},
                }

                for i = 1, #tColors do
                    local winPos = tImGui.GetCursorScreenPos()
                    local p_max  = {x = winPos.x + sz, y = winPos.y + sz}
                    local name   = tLang.L(tColors[i][1])
                    local color  = tColors[i][2]
                    tImGui.AddRectFilled(winPos, p_max, color, rounding, flags)
                    tImGui.Dummy({x = sz, y = sz})
                    tImGui.SameLine()
                    if tImGui.MenuItem(name) then
                        mbm.setColor(color.r, color.g, color.b)
                    end
                end
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('zoom')) then
            -- This is the orbit camera's distance from its focus point, not a scale factor --
            -- labeled/formatted accordingly (was previously showing "Scale %.1f").
            local retZ, distance = tImGui.SliderFloat('##Scale3d', cam3d.distance, 50, iCameraFarPlane3d - 5000, tLang.L('camera_distance_format'))
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
    local offset = self.tMeshOffsets[tInfo.fileName] or {}
    local ox, oy, oz = offset.x or 0, offset.y or 0, offset.z or 0
    local orx, ory, orz = offset.rx or 0, offset.ry or 0, offset.rz or 0
    local osx, osy, osz = offset.sx or 1, offset.sy or 1, offset.sz or 1
    local function finish(tObj)
        if tObj then
            tObj:setPos(tInfo.x + ox, tInfo.y + oy, tInfo.z + oz)
            tObj:setAngle(orx, (tInfo.rotationY or 0) + ory, orz)
            tObj:setScale((tInfo.sx or 1) * osx, (tInfo.sy or 1) * osy, (tInfo.sz or 1) * osz)
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
    computeOrbitFromDirectionalDir() -- re-derive the trackball's angles from whatever loaded
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

function getOSTempDir()
    local sep = package.config:sub(1, 1)
    local tmpDir
    if sep == '\\' then
        tmpDir = os.getenv('TEMP') or os.getenv('TMP') or 'C:\\Temp'
    else
        tmpDir = os.getenv('TMPDIR') or '/tmp'
    end
    return tmpDir, sep
end

function getTempScene3dFile()
    local tmpDir, sep = getOSTempDir()
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
    camera3d:setFar(iCameraFarPlane3d)
    ImGuiPopupFlags_MouseButtonRight = tImGui.Flags('ImGuiPopupFlags_MouseButtonRight')

    -- Thumbnails are cache/scratch data, not project assets -- generate them under the OS temp
    -- dir (same place getTempScene3dFile() already uses), never inside the engine/project folder.
    local tmpDir, sep = getOSTempDir()
    sThumbnailCacheDir = tmpDir .. sep .. 'mbm_scene3d_thumbnails' .. sep
    if sep == '\\' then
        os.execute('mkdir "' .. sThumbnailCacheDir .. '" 2>nul')
    else
        os.execute('mkdir -p "' .. sThumbnailCacheDir .. '"')
    end
    sMeshSetFolder = mbm.getPathEngine() or ''

    tOriginLine3dX = line:new('3d', 0, 0, 0)
    tOriginLine3dX:add({-999999, 0, 0, 999999, 0, 0})
    tOriginLine3dX:setColor(1, 0, 0, 1)
    tOriginLine3dY = line:new('3d', 0, 0, 0)
    tOriginLine3dY:add({0, -999999, 0, 0, 999999, 0})
    tOriginLine3dY:setColor(0, 1, 0, 1)
    tOriginLine3dZ = line:new('3d', 0, 0, 0)
    tOriginLine3dZ:add({0, 0, -999999, 0, 0, 999999})
    tOriginLine3dZ:setColor(0, 0, 1, 1)

    lnRectSelection = line:new('2ds')
    lnRectSelection:add({0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
    lnRectSelection:setColor(0, 1, 1, 1)
    lnRectSelection.visible = false

    mbm.setLightEnabled('3d', false)
    computeOrbitFromDirectionalDir() -- seed tLightOrbit from the default directionalDir
    addLayer()
    rebuildGridVisual() -- Orthogonal is now the default map type, so the grid must exist from the
                        -- start, not only after the user first touches a Map-tab field.
    applyCam3d(cam3d)
    tUtil.sMessageOverlay = tLang.L('welcome_scene_editor_3d')
end

function onLoop(delta)
    processThumbnailQueue()
    tickThumbnailBake()

    main_menu_3d()
    main_tab_bar()
    -- after main_tab_bar() so `cam3d` already points at the tab that was active *this* frame
    -- (switching tabs would otherwise apply the previous tab's camera for one stale frame)
    applyCam3d(cam3d)

    -- Shown in all three tabs (Map, Mesh View, Map edition) -- `cam3d` already aliases whichever
    -- tab's own camera is current (setActiveTab), so this always drives the right one.
    -- setInitialWindowPositionRight's `x` is added on top of its own right-edge alignment (it
    -- already computes screenWidth - width internally) -- passing x = -width here (as before)
    -- double-shifted it an extra `width` in from the edge, which is why "almost its own width"
    -- was missing on the right. x = 0 sits it flush against the edge.
    tUtil.setInitialWindowPositionRight(tLang.L('mesh_preview_orbit'), 0, 0, 160, 160, 160)
    tImGui.Begin(tLang.L('mesh_preview_orbit'), true,
        tImGui.Flags({'ImGuiWindowFlags_NoTitleBar', 'ImGuiWindowFlags_NoResize'}))
    tUtil.drawOrbitGizmo(cam3d, {size = 110})
    tImGui.End()

    -- Deciding "should this input reach the scene, or was it meant for the UI" is exactly what
    -- io.WantCaptureMouse is for -- unlike tImGui.IsAnyWindowHovered() (which this used to call),
    -- it also covers active drags and popups/combos that extend outside their parent window's
    -- rect, and stays correct even when queried from onTouchDown/onTouchMove/onTouchZoom, which
    -- fire *before* onLoop() draws this frame's windows (see docs/lua-api.md's ImGui Common
    -- Pitfalls). IsAnyWindowHovered() intermittently missed clicks on this editor's own windows,
    -- letting them fall through and place/rotate/zoom the scene underneath the UI.
    if not tImGui.GetWantCaptureMouse() then
        updateHoverHighlight(mouseLastX, mouseLastY)
    end
    -- Runs every frame regardless of mouse-capture state -- blinking on already-selected meshes
    -- must keep animating even while the cursor is over the UI, not just while hovering the scene.
    updateSelectionHighlights()
    updateDirectionalLightGizmo()
    updatePointLightMarkers()

    showMeshTools()
    showLoadProgressModal()
    tUtil.showOverlayMessage()
end

-- A left-button press+release only places a mesh if the camera was never actually rotated in
-- between -- otherwise "rotate around" (drag) would drop a new mesh at the press position every
-- single time you start orbiting, which is what onTouchDown used to do. iDragThresholdPx allows a
-- few pixels of incidental jitter (real mice/trackpads rarely produce a perfectly static click)
-- without that counting as an intentional drag.
iDragThresholdPx    = 3
fMouseDownX, fMouseDownY = 0, 0
bCameraDraggedThisPress  = false

-- Finds an existing placed mesh occupying the same grid cell on the given layer -- Orthogonal/
-- Isometric placement replaces whatever is already there (matching tilemap_editor.lua's brick
-- placement: painting over an occupied tile overwrites it, it never stacks duplicates). Free mode
-- has no cell concept, so duplicates/overlaps there are the user's own call, same as before.
function findPlacedMeshAtCell(layerIndex, cx, cz)
    for i, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.layerIndex == layerIndex and tPlaced.cellX == cx and tPlaced.cellZ == cz then
            return i
        end
    end
    return nil
end

function tryPlaceMeshAt(x, y)
    if sActiveTab ~= 'layer' or not sMeshSelectedForPlacement then return end
    local entry = nil
    for _, e in ipairs(tMeshSetEntries) do
        if e.fileName == sMeshSelectedForPlacement then entry = e; break end
    end
    if not entry or iSelectedLayer == 0 then return end
    local layer = tLayers[iSelectedLayer]
    local wx, wy, wz = screenToWorldOnLayerPlane(x, y, layer.fY)
    if not wx then return end
    if tMapOptions.sMapType == 'Free' then
        -- The grid still borders Free mode (no snapping, but bounded) -- reject a click past the
        -- grid's edge the same way grid modes do.
        if not isWorldPosWithinGridBounds(wx, wz, layer) then return end
        addPlacedMesh(entry.fileName, entry.type, iSelectedLayer, 0, 0, wx, wz, true)
    else
        local cx, cz = worldToGridCell(wx, wz, layer)
        -- The grid represents the bounds of the scene being built -- clicking past its edge is
        -- not a valid placement, so it's silently rejected rather than placing a mesh the grid
        -- itself doesn't visually contain.
        if not isCellWithinGridBounds(cx, cz) then return end
        local existingIndex = findPlacedMeshAtCell(iSelectedLayer, cx, cz)
        if existingIndex then removePlacedMesh(existingIndex) end
        addPlacedMesh(entry.fileName, entry.type, iSelectedLayer, cx, cz, nil, nil, true)
    end
end

function onTouchDown(key, x, y)
    if tImGui.GetWantCaptureMouse() then return end
    isClickedMouseleft  = (key == 0)
    isClickedMouseRight = (key == 1)
    mouseLastX, mouseLastY = x, y

    if key == 0 then
        fMouseDownX, fMouseDownY = x, y
        bCameraDraggedThisPress  = false
        -- Shift+drag starts a selection rectangle instead of orbiting -- Shift is the explicit
        -- signal that this press means "select", so the camera must not move underneath it.
        if keyShiftPressed and sActiveTab == 'layer' then
            bRectSelecting = true
            tRectSelection.xStart, tRectSelection.yStart = x, y
            updateRectSelectionLine(x, y, x, y)
            lnRectSelection.visible = true
        end
    end
end

function onTouchMove(key, x, y)
    if tImGui.GetWantCaptureMouse() then
        mouseLastX, mouseLastY = x, y
        return
    end
    if bRectSelecting then
        updateRectSelectionLine(tRectSelection.xStart, tRectSelection.yStart, x, y)
        mouseLastX, mouseLastY = x, y
        return
    end
    if isClickedMouseleft then
        if not bCameraDraggedThisPress then
            local ddx, ddy = x - fMouseDownX, y - fMouseDownY
            if (ddx * ddx + ddy * ddy) > (iDragThresholdPx * iDragThresholdPx) then
                bCameraDraggedThisPress = true
            end
        end
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
    if key == 0 and not tImGui.GetWantCaptureMouse() then
        if bRectSelecting then
            finalizeRectSelection(tRectSelection.xStart, tRectSelection.yStart, x, y)
            lnRectSelection.visible = false
        elseif isClickedMouseleft and not bCameraDraggedThisPress then
            if sMeshSelectedForPlacement then
                tryPlaceMeshAt(x, y)
            else
                handleSelectClickAt()
            end
        end
    end
    bRectSelecting      = false
    isClickedMouseleft  = false
    isClickedMouseRight = false
end

-- Zooms toward whatever is under the cursor (mouseLastX/Y, tracked from the last move/down event --
-- onTouchZoom's own signature has no x,y) instead of always toward the orbit focus point. This is
-- the expected behavior in other 3D editors (Blender/Maya/etc: scroll dollies toward the point
-- under the cursor). Implemented by moving the orbit focus (fx,fy,fz) along the cursor's world-space
-- ray by the same amount the camera-to-focus distance shrinks/grows, which keeps azimuth/elevation
-- (the viewing angle) unchanged while making the camera itself dolly along that ray -- see the
-- derivation in the memory/commit for this change if the math needs revisiting.
function onTouchZoom(zoom)
    if tImGui.GetWantCaptureMouse() then return end
    local oldDistance = cam3d.distance
    local newDistance = math.max(10, oldDistance * (1.0 - zoom * 0.15))
    local deltaDistance = oldDistance - newDistance

    local caz, saz = math.cos(cam3d.azimuth), math.sin(cam3d.azimuth)
    local cel, sel = math.cos(cam3d.elevation), math.sin(cam3d.elevation)
    local dirX, dirY, dirZ = cel * saz, sel, cel * caz -- focus -> camera unit vector (matches cam3dGetPos)

    local x1, y1, z1 = mbm.to3d(mouseLastX, mouseLastY, 100)
    local x2, y2, z2 = mbm.to3d(mouseLastX, mouseLastY, 1000)
    local rx, ry, rz = x2 - x1, y2 - y1, z2 - z1
    local rlen = math.sqrt(rx * rx + ry * ry + rz * rz)
    if rlen > 1e-6 then
        rx, ry, rz = rx / rlen, ry / rlen, rz / rlen
        cam3d.fx = cam3d.fx + deltaDistance * (dirX + rx)
        cam3d.fy = cam3d.fy + deltaDistance * (dirY + ry)
        cam3d.fz = cam3d.fz + deltaDistance * (dirZ + rz)
    end
    cam3d.distance = newDistance
end

function onKeyDown(key)
    if key == mbm.getKeyCode('shift') then
        keyShiftPressed = true
    elseif key == mbm.getKeyCode('control') or key == mbm.getKeyCode('windows') then
        keyControlPressed = true
    elseif key == mbm.getKeyCode('F5') then
        onPlay3d()
    elseif key == mbm.getKeyCode('Delete') and sActiveTab == 'layer' then
        for i = #tPlacedMeshes, 1, -1 do
            if tPlacedMeshes[i].isSelected then removePlacedMesh(i) end
        end
    elseif key == mbm.getKeyCode('esc') and sActiveTab == 'layer' then
        unselectAllPlacedMeshes()
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('shift') then
        keyShiftPressed = false
        -- Releasing Shift mid-drag should not leave a stuck selection rectangle or camera-orbit
        -- state behind -- onTouchUp is the normal finalize path, but a key-up can arrive first
        -- (e.g. releasing Shift before the mouse button).
        if bRectSelecting then
            bRectSelecting = false
            if lnRectSelection then lnRectSelection.visible = false end
        end
    elseif key == mbm.getKeyCode('control') or key == mbm.getKeyCode('windows') then
        keyControlPressed = false
    end
end
