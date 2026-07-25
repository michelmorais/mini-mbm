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

-- Initial width of the "Scene Editor Options" panel (main_tab_bar) -- also the left edge the
-- Mesh Selector window (drawMeshSelector) is anchored against, so both stay in sync. 312 is the
-- old 260 widened ~20% per a direct user report that several labels/fields were clipped at 260.
iMainPanelWidth = 312

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
}

-- Grid visibility is per-tab, same pattern as tCamByTab -- Main Scene has no layer/mesh placement
-- of its own to snap to, so it starts hidden there; Map edition/Mesh property keep the grid on
-- like before.
tShowGridByTab = { map = false, mesh_set = true, layer = true }

-- ---- Layers ----
tLayers        = {}
iSelectedLayer = 0

-- ---- Placed mesh-instances ----
tPlacedMeshes = {}

-- ---- Map-tab "Object Option" markers (editor-only path/spawn-point data) ----
tSceneObjects            = {}
tSceneObjectShapes       = {}
tComboObjectType3d       = {'point', 'rectangle', 'circle', 'triangle', 'line'}
bShowSceneObjectMarkers  = true -- manual show/hide toggle, ANDed with the "Main Scene tab only" rule

-- ---- Mesh Set (asset browser + thumbnail cache) ----
tMeshSetEntries      = {}
tThumbnailCache      = {}
sMeshSetFilterType   = 'All'
tComboMeshSetFilter  = {'All', 'mesh', 'sprite', 'particle', 'tile', 'gif'}
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

-- WASD/arrow-key viewport movement -- one shared accumulator (not per-tab like tCamByTab) since
-- only one tab's camera is ever being driven by the keyboard at a time (whichever is active).
-- Set/cleared in onKeyDown/onKeyUp, consumed once per frame in onLoop before applyCam3d(cam3d).
tCam3dMove = { forward = 0, right = 0 }

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
-- Per-tab like tShowGridByTab/tCamByTab -- Main Scene starts hidden, Map edition/Mesh property
-- keep the old always-on default.
tShowOriginByTab = { map = false, mesh_set = true, layer = true }

-- ---- Grid visual ----
tGridLines = {}

-- ---- Lighting ----
-- Colors are 0.0-1.0 floats (ImGui ColorEdit4 range and what mbm.setAmbientLight/etc. actually
-- expect/clamp to internally) -- NOT 0-255, despite some docs/lua-api.md examples suggesting otherwise.
tLightConfig = {
    -- On by default -- currentTabWantsLightOn() already ANDs this with sActiveTab == 'map', so
    -- this only takes effect on the Main Scene tab (sActiveTab's own initial value); Mesh
    -- property/Map edition stay unlit by default regardless, same as before.
    bEnabled          = true,
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
    -- Editor-only, intentionally never persisted by Save/Export: picks which mesh-loading branch
    -- (loadAsync vs sync load) gets baked into the next Export/Play-generated script.
    bAsyncMeshLoad           = true,
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
        mbm.addPointLight('3d', pl.x, pl.y, pl.z, pl.radius, pl.r, pl.g, pl.b, pl.a or 1)
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
    tImGui.TextDisabled(string.format('x=%.3f', tLightConfig.directionalDir.x))
    tImGui.TextDisabled(string.format('y=%.3f', tLightConfig.directionalDir.y))
    tImGui.TextDisabled(string.format('z=%.3f', tLightConfig.directionalDir.z))

    tImGui.Separator()
    tImGui.Text(tLang.L('light_point_lights_fmt'):format(#tLightConfig.pointLights))
    if tImGui.Button(tLang.L('add_point_light')) then
        table.insert(tLightConfig.pointLights, {x = 0, y = 200, z = 0, radius = 400, r = 1, g = 1, b = 1, a = 1})
        applyLightConfigToEngine()
    end
    local minX, maxX, minY, maxY, minZ, maxZ = getPointLightDragRange()
    for i, pl in ipairs(tLightConfig.pointLights) do
        tImGui.PushItemWidth(120)
        local p1, px = tImGui.DragFloat(tLang.L('axis_x') .. '##pl_x' .. i, pl.x, (maxX - minX) * 0.01, minX, maxX, '%.2f')
        local p2, py = tImGui.DragFloat(tLang.L('axis_y') .. '##pl_y' .. i, pl.y, (maxY - minY) * 0.01, minY, maxY, '%.2f')
        local p3, pz = tImGui.DragFloat(tLang.L('axis_z') .. '##pl_z' .. i, pl.z, (maxZ - minZ) * 0.01, minZ, maxZ, '%.2f')
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

-- WASD/arrow-key ground-plane movement for the active tab's orbit camera, consumed once per frame
-- from onLoop (before applyCam3d(cam3d), so this frame's applyCam3d already reflects the move).
-- Reuses the exact forward/right XZ-plane derivation already used by right-drag-pan (onTouchMove,
-- `fwx,fwy,fwz` normalized then `rgx,rgz = -fwz, fwx`) so keyboard and mouse panning feel
-- consistent. Speed scales with cam3d.distance like every other camera interaction in this file
-- (pan drag, zoom) -- a fixed absolute speed would feel wrong at very different zoom levels.
-- Gated on GetWantCaptureKeyboard() here (at the point of consumption), not at key-down time,
-- since ImGui focus can change between the key press and the next frame.
function updateCam3dKeyboardMovement(delta)
    if tCam3dMove.forward == 0 and tCam3dMove.right == 0 then return end
    if tImGui.GetWantCaptureKeyboard() then return end
    -- Was re-deriving "right" by hand as cross(forward, worldUp) (-fwz, fwx) -- that guess had the
    -- wrong handedness (D strafed toward the camera's actual LEFT, not right, A vice versa).
    -- camera:getNormal('R')/('F') (src/lua-wrap/camera-lua.cpp) read the engine's own true
    -- normalRight/normalForward directly, removing the guesswork entirely.
    local fw = camera3d:getNormal('F')
    local rg = camera3d:getNormal('R')
    -- Re-normalized on the XZ plane only (ground-plane movement at constant speed regardless of
    -- how much the camera is pitched up/down), same as the original derivation.
    local fwLen = math.sqrt(fw.x * fw.x + fw.z * fw.z)
    local rgLen = math.sqrt(rg.x * rg.x + rg.z * rg.z)
    if fwLen < 1e-6 or rgLen < 1e-6 then return end
    local fwx, fwz = fw.x / fwLen, fw.z / fwLen
    local rgx, rgz = rg.x / rgLen, rg.z / rgLen
    local speed = cam3d.distance * 0.8 * delta
    local dx = (fwx * tCam3dMove.forward + rgx * tCam3dMove.right) * speed
    local dz = (fwz * tCam3dMove.forward + rgz * tCam3dMove.right) * speed
    cam3d.fx = cam3d.fx + dx
    cam3d.fz = cam3d.fz + dz
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

-- "Half X/Z offset for each odd line" staggers alternate rows/columns by half a cell -- the
-- classic brick-wall pattern -- applied here in local (pre-rotation) grid space so Isometric's
-- 45-degree rotation carries the stagger along with it, same as the base cell lattice itself.
-- X staggers by row (odd cz, so an entire row of constant Z shifts sideways on X); Z staggers by
-- column (odd cx, so an entire column of constant X shifts on Z) -- two independent perpendicular
-- directions, matching Tiled's stagger-X vs stagger-Y convention, rather than one shared axis.
function gridCellToWorld(cx, cz, layer)
    local lx = cx * tMapOptions.fGridCellWidthX
    local lz = cz * tMapOptions.fGridCellDepthZ
    if layer.bHalfOffsetX and (cz % 2 ~= 0) then
        lx = lx + tMapOptions.fGridCellWidthX * 0.5
    end
    if layer.bHalfOffsetZ and (cx % 2 ~= 0) then
        lz = lz + tMapOptions.fGridCellDepthZ * 0.5
    end
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
    local w, d = tMapOptions.fGridCellWidthX, tMapOptions.fGridCellDepthZ
    -- gridCellToWorld's half-offset stagger is mutually dependent -- X's offset keys off cz's
    -- parity, Z's keys off cx's parity -- so it can't be undone with a single division like the
    -- unstaggered case above. This was the real bug behind the hover-highlight/click-placement
    -- landing in the wrong cell whenever "Half X/Z offset for each odd line" was on: the forward
    -- placement math (gridCellToWorld) staggered alternate rows/columns, but this inverse never
    -- accounted for that shift at all. Fixed with a short fixed-point pass: start from the
    -- unstaggered guess, then alternately re-derive each axis using the other's latest parity --
    -- converges within a couple of iterations since the correction is a fixed +/- half cell, never
    -- a moving target.
    local cx = math.floor(localX / w + 0.5)
    local cz = math.floor(localZ / d + 0.5)
    for _ = 1, 3 do
        local adjX = (layer.bHalfOffsetX and (cz % 2 ~= 0)) and (w * 0.5) or 0
        local adjZ = (layer.bHalfOffsetZ and (cx % 2 ~= 0)) and (d * 0.5) or 0
        cx = math.floor((localX - adjX) / w + 0.5)
        cz = math.floor((localZ - adjZ) / d + 0.5)
    end
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
    -- Only the active tab's own "Show Grid" toggle and having an active layer hide it.
    if not tShowGridByTab[sActiveTab] or iSelectedLayer == 0 then
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

-- `tObj:getAABB()` above is physics-driven, not geometry-driven (RENDERIZABLE::updateAABB(),
-- src/core_mbm/renderizable.cpp, derives boundingAABB from INFO_PHYSICS::getBounds()) -- a
-- hand-authored physics box (Physic Editor) left much smaller than the mesh's real vertex extents
-- reports a bogus small size here. That starves fitDist below into framing the camera far tighter
-- than the actual geometry, which then partially or fully clips past the render2texture camera's
-- frustum and bakes a blank/white thumbnail (see the fitDist near/far comment further down for the
-- companion failure mode). Meshes saved with no hand-authored physics don't hit this: the save-time
-- auto-bound (MESH_MBM_DEBUG::fillAtLeastOneBound(), src/core_mbm/mesh-manager.cpp) already derives
-- its cube from the true vertex min/max, so physics and geometry agree there.
-- There is no physics-independent size query on the Lua RENDERIZABLE (getSize/getAABB/
-- getWidthHeight all read INFO_PHYSICS), so the only way to get the real vertex extents from Lua is
-- to rescan the raw mesh buffer via a throwaway meshDebug instance -- same approach mesh_debug.lua's
-- computeMeshVertexBoundsFrame1 uses, reimplemented locally since that file's helpers aren't
-- exported (see the orbit-camera comment above). Frame 1 only, matching the thumbnail bake itself.
function computeMeshTrueVertexExtentFrame1(fileName)
    local meshD = meshDebug:new()
    local ok = meshD:load(fileName)
    if not ok then
        meshD:fakeRelease(fileName)
        return nil
    end
    local minX, maxX = math.huge, -math.huge
    local minY, maxY = math.huge, -math.huge
    local minZ, maxZ = math.huge, -math.huge
    local total = 0
    local okScan = pcall(function()
        local nSubsets = meshD:getTotalSubset(1)
        for s = 1, nSubsets do
            local nV = meshD:getTotalVertex(1, s)
            for v = 1, nV do
                local vd = meshD:getVertex(1, s, v)
                if vd then
                    local vz = vd.z or 0
                    minX = math.min(minX, vd.x); maxX = math.max(maxX, vd.x)
                    minY = math.min(minY, vd.y); maxY = math.max(maxY, vd.y)
                    minZ = math.min(minZ, vz);   maxZ = math.max(maxZ, vz)
                    total = total + 1
                end
            end
        end
    end)
    meshD:fakeRelease(fileName)
    if not okScan or total == 0 then
        return nil
    end
    return maxX - minX, maxY - minY, maxZ - minZ
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
    -- A fixed nickname, not one derived from entry.fileName: tThumbGenRt is a single scratch
    -- render target reused for every bake (RENDER_2_TEXTURE::load only ever allocates its real
    -- FBO/texture once per object -- every later :create() call here is a no-op that returns the
    -- same one), so the nickname only ever matters for the very first bake of the session anyway.
    -- A path-derived nickname was actively dangerous: TEXTURE_MANAGER::createTextureRenderTarget
    -- caches by util::getBaseName(nickName), which returns only the text after the LAST path
    -- separator -- since entry.fileName is a full path, that silently discarded the "thumb_gen_"
    -- prefix and left just the mesh's bare filename (e.g. "hex_grass.msh") as the real cache key,
    -- so two same-named meshes from different folders could collide and hand this render target
    -- someone else's cached texture, leaving its own idTextureDynamic never actually set (the
    -- "texture is not created!" failure later out of saveAsPNG).
    local ok = tThumbGenRt:create(160, 160, true, 'scene3d_editor_thumb_gen_scratch')
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
    -- Cached here (computed once per bake, angle already normalized to (0,0,0) above) so the Mesh
    -- property tab can show a read-only Width/Height/Depth without loading a second live object.
    entry.physWidth, entry.physHeight, entry.physDepth = w, h, d
    local fitDist = math.max(w or 50, h or 50, d or 50) * 2.2
    -- Flag (for the Mesh Set / Mesh Selector tooltip) rather than correct: a physics box less than
    -- half the mesh's true largest extent means whatever framed fitDist above is unreliable, and the
    -- right fix lives in the Physics tab (physic_editor.lua), not here -- silently widening fitDist
    -- to the true geometry would hide the authoring bug instead of surfacing it.
    entry.physicsBoundsSuspect = false
    if entry.type == 'mesh' then
        local tw, th, td = computeMeshTrueVertexExtentFrame1(entry.fileName)
        if tw then
            local trueMax = math.max(tw, th, td)
            local physMax = math.max(w or 0, h or 0, d or 0)
            if trueMax > 1e-4 and physMax < trueMax * 0.5 then
                entry.physicsBoundsSuspect = true
            end
        end
    end
    -- Frame the mesh's true visual center, not its pivot (obj:getAABBCenter(), MBM_VERSION
    -- 6.9.0) -- for a mesh anchored at its base (e.g. a building), focusing on the pivot (its
    -- floor) would frame the thumbnail on its feet instead of centering the whole asset.
    local cx, cy, cz = tObj:getAABBCenter(true)
    local rtCam = tThumbGenRt:getCamera('3d')
    -- render2texture's own 3D camera (CAMERA_TARGET, render-2-texture.cpp) defaults to
    -- zNear=0.1/zFar=1000, independent of the main scene camera's cam:setFar/setNear. A mesh
    -- whose bounding box put the fitDist camera distance beyond that fixed far plane got
    -- silently culled, baking a flat white thumbnail (render2texture's clear color) -- this was
    -- previously unfixable from Lua since rt:getCamera() never exposed setNear/setFar at all
    -- (MBM_VERSION 6.24.0 added the binding). Scale the clip range to this mesh's own framing
    -- distance so any mesh, however large or small, stays inside the frustum.
    rtCam:setNear(math.max(0.01, fitDist * 0.001))
    rtCam:setFar(fitDist * 4)
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
    -- Strip the asset's own extension first -- tUtil.getShortName keeps it (e.g. "hex_grass.msh"),
    -- and any Mesh Set entry that's itself a plain texture file (scanMeshSetFolder registers every
    -- recognized file in the folder, not just meshes) already ends in ".png"; appending another
    -- ".png" on top of that produced "grass_top.png.png".
    local baseName = tUtil.getShortName(tThumbGenActive.entry.fileName):gsub('%.[^.]*$', '')
    local pngPath = sThumbnailCacheDir .. baseName .. '.png'
    tThumbGenRt:save(pngPath, 0, 0, 160, 160)
    tThumbGenRt:remove(tThumbGenActive.tObj)
    tThumbGenActive.tObj:destroy()
    local texInfo = mbm.loadTexture(pngPath)
    tThumbnailCache[tThumbGenActive.entry.fileName] = texInfo
    -- Marking 'ready' when texInfo is actually nil would permanently mask a real bake/load
    -- failure as success (getOrCreateThumbnail only re-queues when thumbState == nil) -- the
    -- entry would then silently show no thumbnail forever instead of surfacing the failure.
    tThumbGenActive.entry.thumbState = texInfo and 'ready' or 'failed'
    tThumbGenActive = nil
end

-- Lists every supported renderizable asset in `dir` (mesh/sprite/particle/tile/font/gif, see
-- isSupportedMeshSetType3d) using meshDebug:getInfo as the authoritative type detector (mirrors
-- tUtil.onAddMeshToEditor's own type-detection call).
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
            if isSupportedMeshSetType3d(tInfo) then
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

-- A highlight box scaled to the object's exact AABB is invisible on any object that's itself a
-- cube (its faces sit flush with the AABB's own faces, so the translucent overlay has no visible
-- margin to shade). Inflating by a fixed 10% keeps the box hugging the object while guaranteeing a
-- visible rim on every shape, cube or not.
HIGHLIGHT_BOX_SCALE_FACTOR = 1.10

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
    -- Guarantees this box wins over the highlighted mesh regardless of distance/z ties or
    -- actual geometry occlusion -- see RENDERIZABLE::isAlwaysOnTop's own doc comment
    -- (renderizable.h) for why HIGHLIGHT_BOX_SCALE_FACTOR alone isn't a complete fix.
    shp.alwaysOnTop = true
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
        -- Restricted to the active layer -- hovering (and therefore click/Ctrl+click selecting)
        -- a mesh that belongs to a different layer would let Rotate/Delete act on it too, which
        -- is exactly the cross-layer mistake this is meant to prevent.
        if tPlaced.layerIndex == iSelectedLayer and tPlaced.tObj and tPlaced.tObj:collide(sx, sy) then
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
-- 0.15 while merely hovered, blinking between alpha 0.05 and 0.15 while selected (a plain per-frame
-- sine wave -- no shader needed for this). Kept subtle to match physic_editor.lua's own translucent
-- shape-overlay convention (alpha ~0.1-0.15) -- alwaysOnTop (see makeHighlightBoxShape) already
-- guarantees this box is fully visible over the mesh, so it no longer needs a strong alpha to read
-- as "on top"; a heavier alpha just looked like a solid tinted duplicate of the mesh underneath.
-- Selected takes priority over hover when both apply.
function updateSelectionHighlights()
    if sActiveTab ~= 'layer' then
        for _, tPlaced in ipairs(tPlacedMeshes) do
            if tPlaced.tHighlightShape then tPlaced.tHighlightShape.visible = false end
        end
        return
    end
    local blinkAlpha = 0.15 + 0.05 * math.sin(mbm.getTimeRun() * 4)
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
            tPlaced.tHighlightShape:setPos(cx, cy + (0.1 * h), cz)
            tPlaced.tHighlightShape:setScale(w * HIGHLIGHT_BOX_SCALE_FACTOR, h * HIGHLIGHT_BOX_SCALE_FACTOR, d * HIGHLIGHT_BOX_SCALE_FACTOR)
            local belongsToActiveLayer = tPlaced.layerIndex == iSelectedLayer
            local r, g = belongsToActiveLayer and 0 or 1, belongsToActiveLayer and 1 or 0
            tPlaced.tHighlightShape:setColor(r, g, 0, tPlaced.isSelected and blinkAlpha or 0.2)
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
        -- Degenerate (near-zero-area) drag -- a Shift+click with no real drag. Unlike a plain
        -- click (handleSelectClickAt, which replaces the whole selection), Shift is the user's
        -- explicit "build up a selection" signal -- ADD whatever's under the cursor to the
        -- existing selection instead of replacing it, so repeated Shift+clicks can select several
        -- meshes one at a time (e.g. to Copy them together). Clicking empty space with Shift held
        -- intentionally leaves the current selection untouched rather than clearing it.
        if tHoveredPlaced then
            tPlacedMeshes[tHoveredPlaced].isSelected = true
        end
        return
    end
    for _, tPlaced in ipairs(tPlacedMeshes) do
        -- Restricted to the active layer, same reasoning as updateHoverHighlight above -- a
        -- rectangle drawn on screen can enclose meshes from other layers too, but only the
        -- active layer's should end up selected (and therefore actionable by Rotate/Delete).
        tPlaced.isSelected = tPlaced.layerIndex == iSelectedLayer and tPlaced.tObj ~= nil and isPointInScreenRectAtObjectDepth(
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
    local step = math.pi * 0.25
    for _, tPlaced in ipairs(tPlacedMeshes) do
        -- Restricted to the active layer even though selection is already layer-scoped upstream
        -- (updateHoverHighlight/finalizeRectSelection) -- the per-mesh "Selected" checkbox in the
        -- Placed Meshes list (drawLayerTab) can still mark a mesh from another layer selected, and
        -- this must not act on it.
        if tPlaced.isSelected and tPlaced.layerIndex == iSelectedLayer then
            tPlaced.rotationY = (tPlaced.rotationY or 0) + sign * step
            syncPlacedMeshTransform(tPlaced)
        end
    end
    -- Only ever called from showMeshTools' Rotate Right/Left buttons, which are themselves only
    -- shown while at least one mesh matching this same condition is selected -- always a real,
    -- already-completed rotation by the time this returns.
    pushUndoSnapshot()
end

-- Bottom-right floating tool window, mirrors tilemap_editor.lua's showTileTools -- appears only
-- while at least one placed mesh is selected, offering the same Rotate Right / Rotate Left /
-- Delete actions (no Flip -- that's a 2D sprite-image operation with no 3D equivalent) against
-- every currently-selected mesh at once.
-- Shared by showMeshTools and main_menu_3d's Edit menu (Copy/Paste enabled state) so the two
-- never disagree about what "selected" means here.
function countSelectedMeshesOnActiveLayer()
    local iSelectedCount = 0
    for _, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected and tPlaced.layerIndex == iSelectedLayer then iSelectedCount = iSelectedCount + 1 end
    end
    return iSelectedCount
end

function showMeshTools()
    if sActiveTab ~= 'layer' then return end
    local iSelectedCount = countSelectedMeshesOnActiveLayer()
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
            if tPlacedMeshes[i].isSelected and tPlacedMeshes[i].layerIndex == iSelectedLayer then removePlacedMesh(i) end
        end
        pushUndoSnapshot()
    end
    if tImGui.Button(tLang.L('copy_btn'), tUtil.getResponsiveItemSize(item_width)) then
        copySelectedMeshes()
    end
    -- Paste needs exactly one selected mesh to act as its unambiguous anchor -- see
    -- pasteCopiedMeshes' own comment.
    if tCopyBuffer and #tCopyBuffer > 0 and iSelectedCount == 1 then
        if tImGui.Button(tLang.L('paste_btn'), tUtil.getResponsiveItemSize(item_width)) then
            pasteCopiedMeshes()
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

-- Copy/Paste (Layer tab's Total Selected tool window, showMeshTools). nil until the first Copy --
-- an array of {fileName, type, rotationY, sx, sy, sz, dx, dy, dz}, one entry per mesh selected at
-- copy time, where dx/dy/dz is that mesh's WORLD-SPACE offset from the FIRST selected mesh (the
-- copy-time "anchor"). Paste re-anchors that same relative layout onto whichever single mesh is
-- selected at paste time -- the first copied mesh's own dx/dy/dz is (0,0,0), so its pasted copy
-- lands exactly on the new anchor (replacing it in grid modes, same overwrite rule ordinary
-- placement already uses), which is what lets Paste "stamp" a whole copied group starting at an
-- existing mesh rather than only adding the OTHER meshes around it.
tCopyBuffer = nil

function copySelectedMeshes()
    local tSelected = {}
    for _, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected and tPlaced.layerIndex == iSelectedLayer then
            table.insert(tSelected, tPlaced)
        end
    end
    if #tSelected == 0 then return end
    local ax, ay, az = resolvePlacedMeshWorldPos(tSelected[1])
    tCopyBuffer = {}
    for _, tPlaced in ipairs(tSelected) do
        local wx, wy, wz = resolvePlacedMeshWorldPos(tPlaced)
        table.insert(tCopyBuffer, {
            fileName = tPlaced.fileName, type = tPlaced.type,
            rotationY = tPlaced.rotationY or 0,
            sx = tPlaced.scale.x, sy = tPlaced.scale.y, sz = tPlaced.scale.z,
            dx = wx - ax, dy = wy - ay, dz = wz - az,
        })
    end
    -- Pure read of the current selection -- no scene mutation, so no undo snapshot here.
end

-- Requires exactly one selected mesh (the paste anchor) -- see showMeshTools, which only shows
-- the Paste button under that same condition. Reuses the exact same grid-snap/bounds-check/
-- overwrite-in-place logic tryPlaceMeshAt already uses for a normal click-placement, so a pasted
-- mesh always lands the same way a manually placed one would.
function pasteCopiedMeshes()
    if not tCopyBuffer or #tCopyBuffer == 0 or iSelectedLayer == 0 then return end
    local anchor = nil
    for _, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected and tPlaced.layerIndex == iSelectedLayer then
            anchor = tPlaced
            break
        end
    end
    if not anchor then return end
    local ax, ay, az = resolvePlacedMeshWorldPos(anchor)
    local layer = tLayers[iSelectedLayer]
    local tNewlyPasted = {}
    for _, tCopy in ipairs(tCopyBuffer) do
        local wx, wy, wz = ax + tCopy.dx, ay + tCopy.dy, az + tCopy.dz
        local tNew = nil
        if tMapOptions.sMapType == 'Free' then
            if isWorldPosWithinGridBounds(wx, wz, layer) then
                tNew = addPlacedMesh(tCopy.fileName, tCopy.type, iSelectedLayer, 0, 0, wx, wz, true)
                -- Explicit Y (not layer-attached) so the copied group's own vertical offsets carry
                -- over exactly, even for a mesh that was originally layer-attached at copy time.
                tNew.bAttachedToLayer = false
                tNew.freeY = wy
            end
        else
            local cx, cz = worldToGridCell(wx, wz, layer)
            if isCellWithinGridBounds(cx, cz) then
                local existingIndex = findPlacedMeshAtCell(iSelectedLayer, cx, cz)
                if existingIndex then removePlacedMesh(existingIndex) end
                tNew = addPlacedMesh(tCopy.fileName, tCopy.type, iSelectedLayer, cx, cz, nil, nil, true)
            end
        end
        if tNew then
            tNew.rotationY = tCopy.rotationY
            tNew.scale = {x = tCopy.sx, y = tCopy.sy, z = tCopy.sz}
            syncPlacedMeshTransform(tNew)
            table.insert(tNewlyPasted, tNew)
        end
    end
    if #tNewlyPasted > 0 then
        -- Select the pasted result instead of the anchor -- lets the user immediately see/adjust
        -- what was just pasted, and matches conventional paste UX.
        for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = false end
        for _, tPlaced in ipairs(tNewlyPasted) do tPlaced.isSelected = true end
        pushUndoSnapshot()
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
        tLoadProgress.iLoaded = tLoadProgress.iLoaded + 1
    else
        -- Only (re)start the progress tracker for a standalone placement (e.g. one grid click).
        -- A bulk caller like onOpenScene3d already calls beginLoadProgress once with the correct
        -- total before its loop; stomping it here on every single placement grew iTotal by 1 per
        -- mesh (ending up double the real count, e.g. 800 instead of 400) and kept resetting
        -- iLoaded back to 0, so the modal could never reach 100% -- a real, confirmed hang.
        if not tLoadProgress.bLoading then
            beginLoadProgress(1, tUtil.getShortName(fileName))
        end
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

-- Scene Editor 3D only supports mesh/sprite/particle/tile/font placement, plus gif (an animated
-- texture, kept as its own case) -- meshDebug:getInfo reports every other recognized image format
-- (png/jpg/bmp/tga/psd/etc) as tInfo.type == 'texture' too, with no way to tell them apart from
-- gif except tInfo.ext, so a plain (non-gif) texture is the one recognized type this editor must
-- still reject. onAddMeshDirect's own file-picker filter already excludes those extensions, but
-- scanMeshSetFolder/registerMeshSetEntry list whatever's actually in a folder on disk regardless
-- of that filter, so the same rule has to be enforced here too, not just at the picker.
function isSupportedMeshSetType3d(tInfo)
    return tInfo ~= nil and tInfo.type ~= nil and not (tInfo.type == 'texture' and tInfo.ext ~= 'GIF')
end

-- Registers a single file into the Mesh Set list (if not already present) so it also
-- becomes selectable later from the Mesh Set tab / Mesh Selector window.
function registerMeshSetEntry(fileName)
    for _, e in ipairs(tMeshSetEntries) do
        if e.fileName == fileName then return e end
    end
    local tInfo = meshDebug and meshDebug:getInfo(fileName) or nil
    if not isSupportedMeshSetType3d(tInfo) then return nil end
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
        'tile', 'spt', 'ptl', 'msh', 'fnt', 'gif')
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

-- Registers every recognized asset file directly inside `dir` (flat, non-recursive, same
-- directory-listing approach as scanMeshSetFolder) into the Mesh Set inventory via
-- registerMeshSetEntry. Unlike scanMeshSetFolder (which REPLACES the "Mesh Set Folder" browser's
-- whole list with whatever the folder currently holds), this is purely additive -- dedup is
-- registerMeshSetEntry's own job -- so it can be called repeatedly across several folders without
-- losing anything already registered. Non-asset files are skipped silently (a folder routinely
-- contains files that aren't meshes); returns the last entry successfully registered, or nil.
function registerMeshesFromFolder(dir)
    if not dir or dir == '' then return nil end
    local sep = package.config:sub(1, 1)
    local cmd = sep == '\\' and string.format('dir "%s" /b', dir) or string.format('ls -1 "%s"', dir)
    local fp = io.popen(cmd)
    if not fp then return nil end

    local lastEntry = nil
    for line in fp:lines() do
        if line and #line > 0 then
            local entry = registerMeshSetEntry(dir .. sep .. line)
            if entry then lastEntry = entry end
        end
    end
    fp:close()
    return lastEntry
end

-- Folder counterpart to onAddMeshDirect: lets the user pick a folder directly (instead of
-- individual files) and registers every recognized asset in it into the Mesh Set inventory.
function onAddMeshFromFolder()
    local chosen = mbm.openFolder(tLang.L('choose_folder'), sLastMeshAddDir ~= '' and sLastMeshAddDir or sMeshSetFolder)
    if not chosen then return end
    sLastMeshAddDir = chosen

    local lastEntry = registerMeshesFromFolder(chosen)
    if lastEntry then
        sMeshSelectedForPlacement = lastEntry.fileName
    else
        tUtil.showMessageWarn(tLang.L('failed_to_add_mesh'))
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

-- All marker shapes share this color -- semi-transparent magenta, matching the Tile Map Editor's
-- own "this is an editor marker, not real geometry" convention (editor/tilemap_editor.lua:2018,
-- the line-type object marker there uses the same {1.0, 0.0, 1.0, 0.7}).
tSceneMarkerColor = {r = 1, g = 0, b = 1, a = 0.7}

-- 8 corners / 12 triangles of a unit box (half-extent 0.5), same CUBE_COMPLEX corner convention as
-- physic_editor.lua's own box-gizmo builder (include/core_mbm/shapes.h: front face a,b,c,d @
-- +halfDepth, back face e,f,g,h @ -halfDepth). Built at unit size and scaled per-instance via
-- :setScale() rather than baking each marker's absolute size into its own vertex data.
local function unitCubeVerts()
    local h = 0.5
    local a, b, c, d = {x = -h, y = -h, z =  h}, {x = -h, y =  h, z =  h}, {x =  h, y =  h, z =  h}, {x =  h, y = -h, z =  h}
    local e, f, g, hh = {x = -h, y = -h, z = -h}, {x = -h, y =  h, z = -h}, {x =  h, y =  h, z = -h}, {x =  h, y = -h, z = -h}
    local faces = {
        {a, b, c}, {a, c, d}, {hh, g, f}, {hh, f, e}, {e, f, b}, {e, b, a}, {d, c, g}, {d, g, hh}, {b, f, g}, {b, g, c}, {e, a, d}, {e, d, hh},
    }
    local verts = {}
    for _, tri in ipairs(faces) do
        for _, p in ipairs(tri) do
            table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z)
        end
    end
    return verts
end

-- Unit-radius UV-sphere, low tessellation (this is an editor marker, not a shipped asset).
-- Scaled per-instance via :setScale(r,r,r) from tObj.ray. No named 'sphere' primitive exists in
-- SHAPE_MESH's Lua binding (src/lua-wrap/render-table/shape-lua.cpp:134-203 only defines
-- circle/rectangle/triangle, all flat) -- hence a raw-vertex build, same idiom as unitCubeVerts.
local function unitSphereVerts(latSegments, lonSegments)
    latSegments = latSegments or 8
    lonSegments = lonSegments or 12
    local function toXYZ(theta, phi)
        local s = math.sin(theta)
        return s * math.cos(phi), math.cos(theta), s * math.sin(phi)
    end
    local verts = {}
    local function push(x, y, z) table.insert(verts, x); table.insert(verts, y); table.insert(verts, z) end
    for i = 0, latSegments - 1 do
        local theta1 = (i / latSegments) * math.pi
        local theta2 = ((i + 1) / latSegments) * math.pi
        for j = 0, lonSegments - 1 do
            local phi1 = (j / lonSegments) * math.pi * 2
            local phi2 = ((j + 1) / lonSegments) * math.pi * 2
            local x1, y1, z1 = toXYZ(theta1, phi1)
            local x2, y2, z2 = toXYZ(theta1, phi2)
            local x3, y3, z3 = toXYZ(theta2, phi1)
            local x4, y4, z4 = toXYZ(theta2, phi2)
            push(x1, y1, z1); push(x3, y3, z3); push(x4, y4, z4)
            push(x1, y1, z1); push(x4, y4, z4); push(x2, y2, z2)
        end
    end
    return verts
end

-- Unit (1x1) double-sided flat quad, centered on the origin in the local XY plane (z=0) -- the
-- named 'rectangle' shape primitive is single-sided (backface-culled), which is exactly why a
-- rectangle marker was only visible from one side; building it here from raw vertices lets a
-- second copy of the same two triangles be added with reversed winding, so one or the other is
-- always front-facing regardless of which side the camera is on.
local function unitQuadVerts()
    local h = 0.5
    local a, b, c, d = {x = -h, y = -h, z = 0}, {x =  h, y = -h, z = 0}, {x =  h, y =  h, z = 0}, {x = -h, y =  h, z = 0}
    local front = {{a, b, c}, {a, c, d}}
    local back  = {{a, c, b}, {a, d, c}}
    local verts = {}
    for _, tri in ipairs(front) do
        for _, p in ipairs(tri) do table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z) end
    end
    for _, tri in ipairs(back) do
        for _, p in ipairs(tri) do table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z) end
    end
    return verts
end

-- Double-sided triangle (same front+reversed-back-winding idea as unitQuadVerts) built directly
-- from 3 absolute world points, since a triangle marker's 3 corners are independently user-editable
-- (not a fixed size around a center) and need not be axis-aligned/coplanar with any particular
-- plane.
local function triangleVertsFromPoints(p1, p2, p3)
    local front = {p1, p2, p3}
    local back  = {p1, p3, p2}
    local verts = {}
    for _, p in ipairs(front) do table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z) end
    for _, p in ipairs(back) do table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z) end
    return verts
end

-- Flat xyz array for a `line` marker's current point path, built at the world origin (see the
-- comment this replaced) -- falls back to a short 2-point stub centered on the marker's own x/y/z
-- so a brand-new line marker with no points yet is still visible.
local function lineFlatPoints(tObj)
    local pts = tObj.points or {}
    local flat = {}
    if #pts > 0 then
        for _, p in ipairs(pts) do
            table.insert(flat, p.x); table.insert(flat, p.y); table.insert(flat, p.z)
        end
    else
        table.insert(flat, tObj.x); table.insert(flat, tObj.y); table.insert(flat, tObj.z)
        table.insert(flat, tObj.x + 1); table.insert(flat, tObj.y); table.insert(flat, tObj.z)
    end
    return flat
end

-- Builds/rebuilds tSceneObjectShapes[i] = {handle=, sType=, sig=} so it always matches tObj.type
-- and (for line/triangle, whose geometry IS their editable points, not a fixed unit shape scaled
-- from a single center) their current point values. `line` updates live via LINE_MESH:set()
-- (no destroy/recreate needed -- it replaces the point list on the existing object, confirmed via
-- src/lua-wrap/render-table/line-mesh-lua.cpp's onSetLineMeshLua, which places no size constraint
-- on the new array). `triangle` has no such in-place update in SHAPE_MESH's Lua binding, so it
-- destroys/recreates whenever a signature of its 3 points changes.
function updateSceneObjectShapes()
    for i, tObj in ipairs(tSceneObjects) do
        local entry = tSceneObjectShapes[i]
        if not entry or entry.sType ~= tObj.type then
            if entry and entry.handle then entry.handle:destroy() end
            local handle
            if tObj.type == 'point' then
                handle = shape:new('3d', tObj.x, tObj.y, tObj.z)
                handle:create(unitCubeVerts(), nil, 'editor_marker_cube_unit')
            elseif tObj.type == 'circle' then
                handle = shape:new('3d', tObj.x, tObj.y, tObj.z)
                handle:create(unitSphereVerts(), nil, 'editor_marker_sphere_unit')
            elseif tObj.type == 'rectangle' then
                handle = shape:new('3d', tObj.x, tObj.y, tObj.z)
                handle:create(unitQuadVerts(), nil, 'editor_marker_quad_unit')
            elseif tObj.type == 'triangle' then
                -- Real geometry (from tObj.points) is filled in by the per-frame check below,
                -- which always runs immediately after this (entry.sig starts nil, never equal to
                -- a real signature) -- this placeholder just needs to be a valid triangle. Unique
                -- nickname per marker index for the same reason explained below on the real
                -- rebuild path -- SHAPE_MESH::load (src/render/shape-mesh.cpp:859) resolves its
                -- nickname through MESH_MANAGER::load, a name-keyed SHARED cache: a second
                -- :create() call anywhere using an already-seen name gets back that FIRST call's
                -- cached geometry, silently ignoring whatever new vertex data was just passed in.
                handle = shape:new('3d', 0, 0, 0)
                handle:create({0, 0, 0, 0.01, 0, 0, 0, 0.01, 0}, nil, 'editor_marker_triangle_init_' .. i)
            else -- 'line'
                -- Built at the world origin, not tObj.x/y/z -- tObj.points already store absolute
                -- world coordinates (captured from the marker's own x/y/z fields when "add point"
                -- was pressed, then independently editable), the same convention the always-on
                -- origin/axis lines use (tOriginLine3dX etc. are created at (0,0,0) and given
                -- absolute-extent coordinates via :add()). Giving this handle its own non-zero
                -- setPos as well would double-offset every point.
                handle = line:new('3d', 0, 0, 0)
                handle:add(lineFlatPoints(tObj))
            end
            handle:setColor(tSceneMarkerColor.r, tSceneMarkerColor.g, tSceneMarkerColor.b, tSceneMarkerColor.a)
            entry = { handle = handle, sType = tObj.type, sig = nil, rebuildCount = 0 }
            tSceneObjectShapes[i] = entry
        end
        if tObj.type == 'point' then
            entry.handle:setPos(tObj.x, tObj.y, tObj.z)
            entry.handle:setScale(40, 40, 40)
        elseif tObj.type == 'circle' then
            entry.handle:setPos(tObj.x, tObj.y, tObj.z)
            local r = tObj.ray or 50
            entry.handle:setScale(r, r, r)
        elseif tObj.type == 'rectangle' then
            entry.handle:setPos(tObj.x, tObj.y, tObj.z)
            entry.handle:setScale(tObj.width or 100, tObj.height or 100, 1)
        elseif tObj.type == 'triangle' then
            local p1 = (tObj.points and tObj.points[1]) or {x = tObj.x, y = tObj.y, z = tObj.z}
            local p2 = (tObj.points and tObj.points[2]) or {x = tObj.x, y = tObj.y, z = tObj.z}
            local p3 = (tObj.points and tObj.points[3]) or {x = tObj.x, y = tObj.y, z = tObj.z}
            local sig = string.format('%.2f,%.2f,%.2f|%.2f,%.2f,%.2f|%.2f,%.2f,%.2f',
                p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, p3.x, p3.y, p3.z)
            if entry.sig ~= sig then
                -- A genuinely unique nickname per rebuild (marker index + a running counter) is
                -- required here, not a fixed literal string -- see the comment on the placeholder
                -- creation above: MESH_MANAGER::load caches by nickname across ALL shape objects,
                -- so reusing one fixed name meant every edit after the very first triangle ever
                -- created just got that first triangle's stale geometry back, silently ignoring
                -- the new points (this was the actual bug: dragging any point/the whole triangle
                -- visibly did nothing, and every triangle marker rendered identically).
                entry.rebuildCount = (entry.rebuildCount or 0) + 1
                entry.handle:destroy()
                entry.handle = shape:new('3d', 0, 0, 0)
                entry.handle:create(triangleVertsFromPoints(p1, p2, p3), nil,
                    'editor_marker_triangle_' .. i .. '_' .. entry.rebuildCount)
                entry.handle:setColor(tSceneMarkerColor.r, tSceneMarkerColor.g, tSceneMarkerColor.b, tSceneMarkerColor.a)
                entry.sig = sig
            end
        else -- 'line'
            entry.handle:set(lineFlatPoints(tObj), 1)
        end
        entry.handle.visible = (sActiveTab == 'map') and bShowSceneObjectMarkers
    end
end

-- markerIndex must be part of every widget ID here -- ImGui identifies widgets by ID string, and
-- every field below (bar the caller's own type-combo/delete-button) used a bare, non-unique ID, so
-- multiple markers rendered in the same frame all shared ONE underlying widget/edit-buffer per
-- field: typing a name into one marker's field could be silently clobbered the moment a second
-- marker's row (using the exact same '##marker_name' ID) rendered afterward.
function drawObjectMarkerFields(tObj, markerIndex)
    -- tImGui.InputText(label, text, flags?) has no "max length" parameter at all -- its C++
    -- binding always auto-sizes the edit buffer from the current text's own length + 256 bytes of
    -- headroom. The "64" here was passed expecting some other framework's (label, text, maxLength)
    -- shape; this binding instead validates any bare number as a raw ImGuiInputTextFlags bitmask,
    -- and 64 == ImGuiInputTextFlags_EnterReturnsTrue (1<<6) -- a real, valid flag -- so it silently
    -- changed this field's contract to "only report a change once Enter is pressed" instead of
    -- "report a change on every keystroke", which is why a typed name never appeared to stick
    -- (confirmed: the user never pressed Enter after typing, just moved on).
    local cName, sName = tImGui.InputText('##marker_name' .. markerIndex, tObj.name or 'no_name')
    if cName then tObj.name = sName end

    -- Same DragFloat + scene-range convention already used for point lights (getPointLightDragRange)
    -- -- a typed InputFloat has no sense of the scene's actual scale, so nudging a marker into place
    -- meant guessing numbers; dragging within the grid's real extent is the same "grab and move it"
    -- interaction the point-light panel already has.
    local minX, maxX, minY, maxY, minZ, maxZ = getPointLightDragRange()
    local rangeSpan = math.max(maxX - minX, maxZ - minZ)

    tImGui.PushItemWidth(120)
    local c1, x = tImGui.DragFloat(tLang.L('axis_x') .. '##marker_x' .. markerIndex, tObj.x, 1, minX, maxX, '%.2f')
    local c2, y = tImGui.DragFloat(tLang.L('axis_y') .. '##marker_y' .. markerIndex, tObj.y, 1, minY, maxY, '%.2f')
    local c3, z = tImGui.DragFloat(tLang.L('axis_z') .. '##marker_z' .. markerIndex, tObj.z, 1, minZ, maxZ, '%.2f')
    tImGui.PopItemWidth()
    if c1 or c2 or c3 then
        -- For a triangle, x/y/z is a "move the whole shape" convenience control, not its own
        -- corner -- translate all 3 stored corners by the same delta so dragging it relocates the
        -- triangle without reshaping it.
        if tObj.type == 'triangle' and tObj.points then
            local dx, dy, dz = x - tObj.x, y - tObj.y, z - tObj.z
            for _, p in ipairs(tObj.points) do
                p.x, p.y, p.z = p.x + dx, p.y + dy, p.z + dz
            end
        end
        tObj.x, tObj.y, tObj.z = x, y, z
    end

    if tObj.type == 'rectangle' then
        local cw, w = tImGui.DragFloat(tLang.L('width') .. '##marker_w' .. markerIndex, tObj.width or 100, 1, 1, rangeSpan, '%.2f')
        local ch, h = tImGui.DragFloat(tLang.L('height') .. '##marker_h' .. markerIndex, tObj.height or 100, 1, 1, rangeSpan, '%.2f')
        if cw then tObj.width = w end
        if ch then tObj.height = h end
    elseif tObj.type == 'circle' then
        local cr, r = tImGui.DragFloat(tLang.L('ray') .. '##marker_ray' .. markerIndex, tObj.ray or 50, 1, 1, rangeSpan, '%.2f')
        if cr then tObj.ray = r end
    elseif tObj.type == 'triangle' then
        -- 3 independently draggable corners, initialized around the marker's own position the
        -- first time this marker becomes a triangle (or if points was never a valid 3-tuple, e.g.
        -- an older save/a fresh marker) -- not degenerate all-zero, so the shape is visible and
        -- editable immediately.
        if not tObj.points or #tObj.points ~= 3 then
            tObj.points = {
                {x = tObj.x - 50, y = tObj.y, z = tObj.z - 50},
                {x = tObj.x + 50, y = tObj.y, z = tObj.z - 50},
                {x = tObj.x,      y = tObj.y, z = tObj.z + 50},
            }
        end
        for i, p in ipairs(tObj.points) do
            tImGui.Text(tLang.L('object') .. ' ' .. i)
            local pc1, px = tImGui.DragFloat(tLang.L('axis_x') .. '##tri_x' .. markerIndex .. '_' .. i, p.x, 1, minX, maxX, '%.2f')
            local pc2, py = tImGui.DragFloat(tLang.L('axis_y') .. '##tri_y' .. markerIndex .. '_' .. i, p.y, 1, minY, maxY, '%.2f')
            local pc3, pz = tImGui.DragFloat(tLang.L('axis_z') .. '##tri_z' .. markerIndex .. '_' .. i, p.z, 1, minZ, maxZ, '%.2f')
            if pc1 or pc2 or pc3 then p.x, p.y, p.z = px, py, pz end
        end
    elseif tObj.type == 'line' then
        tObj.points = tObj.points or {}
        if tImGui.Button(tLang.L('add_point') .. '##marker_addpt' .. markerIndex) then
            local n = #tObj.points
            local newPoint
            if n >= 2 then
                -- Continue the last segment: same direction AND same length as (points[n] -
                -- points[n-1]) -- plain linear extrapolation (new = last + (last - secondLast)),
                -- the "calculated, not typed" convention this mirrors from Tile Map Editor's own
                -- line tool (editor/tilemap_editor.lua's add-point button), except that one always
                -- steps by exactly one tile width/height regardless of the previous segment's real
                -- length -- here the previous segment's actual length is preserved, as asked.
                local last, prev = tObj.points[n], tObj.points[n - 1]
                newPoint = {
                    x = last.x + (last.x - prev.x),
                    y = last.y + (last.y - prev.y),
                    z = last.z + (last.z - prev.z),
                }
            elseif n == 1 then
                -- No prior segment to continue -- step off in a default direction/length.
                local last = tObj.points[1]
                newPoint = {x = last.x + 100, y = last.y, z = last.z}
            else
                newPoint = {x = tObj.x, y = tObj.y, z = tObj.z}
            end
            table.insert(tObj.points, newPoint)
        end
        for i, p in ipairs(tObj.points) do
            local pc1, px = tImGui.DragFloat(tLang.L('axis_x') .. '##pt_x' .. markerIndex .. '_' .. i, p.x, 1, minX, maxX, '%.2f')
            local pc2, py = tImGui.DragFloat(tLang.L('axis_y') .. '##pt_y' .. markerIndex .. '_' .. i, p.y, 1, minY, maxY, '%.2f')
            local pc3, pz = tImGui.DragFloat(tLang.L('axis_z') .. '##pt_z' .. markerIndex .. '_' .. i, p.z, 1, minZ, maxZ, '%.2f')
            if pc1 or pc2 or pc3 then p.x, p.y, p.z = px, py, pz end
        end
    end
end

------------------------------------------------------------------------------------------------------------------
-- Map tab
------------------------------------------------------------------------------------------------------------------

-- resolvePlacedMeshWorldPos only ever reads ONE of {cellX,cellZ} or {freeX,freeZ,freeY}, whichever
-- the CURRENT tMapOptions.sMapType selects -- the other pair is stale/never populated for a given
-- mesh (e.g. a grid-placed mesh's freeX/freeZ are left at their addPlacedMesh default of 0). So
-- switching sMapType with no resync collapses every mesh whose *previous* mode didn't maintain the
-- pair the *new* mode reads (confirmed: grid-placed meshes snapping to the origin the moment Free
-- mode is selected, reported directly by a user testing this). Preserve visual position across
-- either switch direction: capture each mesh's currently-resolved world position BEFORE the mode
-- changes (captureAllPlacedMeshWorldPositions, called while the OLD sMapType/rotation is still
-- active), then re-derive both the free and grid-cell fields from that snapshot AFTER the new mode
-- is in effect (applyPlacedMeshWorldPositions, so worldToGridCell's rotation matches the new mode).
function captureAllPlacedMeshWorldPositions()
    local tSnapshots = {}
    for i, tPlaced in ipairs(tPlacedMeshes) do
        local x, y, z = resolvePlacedMeshWorldPos(tPlaced)
        tSnapshots[i] = {x = x, y = y, z = z}
    end
    return tSnapshots
end

function applyPlacedMeshWorldPositions(tSnapshots)
    for i, tPlaced in ipairs(tPlacedMeshes) do
        local snap = tSnapshots[i]
        if snap then
            local layer = tLayers[tPlaced.layerIndex]
            tPlaced.freeX, tPlaced.freeY, tPlaced.freeZ = snap.x, snap.y, snap.z
            if layer then
                tPlaced.cellX, tPlaced.cellZ = worldToGridCell(snap.x, snap.z, layer)
            end
        end
    end
end

function drawMapTab(item_width)
    local ret, current_item = tImGui.Combo(tLang.L('map_type') .. '##MapType3d', tComboMapTypeIndexOf(tMapOptions.sMapType), tComboMapType3dCodes)
    -- Read immediately after each widget below (ImGui's "last item" state) -- a Combo only ever
    -- fires `ret`/deactivates once per selection anyway, but every grid field here is gated the
    -- same uniform way for consistency, since the InputFloat/InputInt fields right below genuinely
    -- need it (see their own comment).
    local bMapTypeChanged = tImGui.IsItemDeactivatedAfterEdit()
    if ret then
        local tSnapshots = captureAllPlacedMeshWorldPositions() -- BEFORE the switch, while the old mode/rotation is still active
        tMapOptions.sMapType = tComboMapType3dCodes[current_item]
        applyPlacedMeshWorldPositions(tSnapshots) -- AFTER, so worldToGridCell uses the new mode's rotation
        rebuildGridVisual()
        resyncAllPlacedMeshes()
    end
    if bMapTypeChanged then pushUndoSnapshot() end

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
    -- InputFloat/InputInt below can return true repeatedly while a user holds down their +/-
    -- spinner button (auto-repeat), not just once on commit -- pushing a snapshot on every `true`
    -- would flood history with one entry per held-button frame. IsItemDeactivatedAfterEdit fires
    -- once instead, when the field actually loses focus/the button is released, after having
    -- really changed.
    local bWidthChanged = tImGui.IsItemDeactivatedAfterEdit()
    local c2, d = tImGui.InputFloat(tLang.L('grid_depth_z') .. '##GridDepthZ', tMapOptions.fGridCellDepthZ, 1, 10, '%.2f')
    local bDepthChanged = tImGui.IsItemDeactivatedAfterEdit()
    if c1 then tMapOptions.fGridCellWidthX = math.max(MIN_GRID_CELL_SIZE, w) end
    if c2 then tMapOptions.fGridCellDepthZ = math.max(MIN_GRID_CELL_SIZE, d) end
    if c1 or c2 then
        rebuildGridVisual()
        -- Placed meshes are stored by cell index (cellX/cellZ), not world position -- their
        -- world position (and, under a snap-scale mode, their scale) is derived from the cell
        -- size, so it must be recomputed here too, not just the grid's visual lines.
        resyncAllPlacedMeshes()
    end
    if bWidthChanged or bDepthChanged then pushUndoSnapshot() end

    -- How many cells wide/deep the visible grid (and "fill layer") span -- previously fixed
    -- at 10 half-lines (21x21) regardless of any setting.
    local c3, cx = tImGui.InputInt(tLang.L('grid_count_x') .. '##GridCountX', tMapOptions.iGridCountX, 1, 10)
    local bCountXChanged = tImGui.IsItemDeactivatedAfterEdit()
    local c4, cz = tImGui.InputInt(tLang.L('grid_count_z') .. '##GridCountZ', tMapOptions.iGridCountZ, 1, 10)
    local bCountZChanged = tImGui.IsItemDeactivatedAfterEdit()
    if c3 then tMapOptions.iGridCountX = math.max(MIN_GRID_COUNT, cx) end
    if c4 then tMapOptions.iGridCountZ = math.max(MIN_GRID_COUNT, cz) end
    if c3 or c4 then
        rebuildGridVisual()
        -- Shrinking the grid can leave previously-placed meshes past the new edge -- the grid
        -- is the bounds of the scene, so anything now outside it is removed, not left floating.
        -- This cascade lands in the SAME snapshot pushed below (once the field is deactivated),
        -- not a separate one -- it's a side effect of this one grid-count edit, not its own action.
        removePlacedMeshesOutsideGrid()
    end
    if bCountXChanged or bCountZChanged then pushUndoSnapshot() end

    if tMapOptions.sMapType ~= 'Free' then
        -- Snapping/scaling to the grid is meaningless in Free mode (no snapping applies there at
        -- all), so this combo stays exclusive to the two grid-snapped modes.
        local retSnap, curSnap = tImGui.Combo(tLang.L('snap_scale_mode') .. '##SnapScaleMode', tComboSnapScaleModeIndexOf(tMapOptions.sSnapScaleMode), tComboSnapScaleModeLabel)
        local bSnapChanged = tImGui.IsItemDeactivatedAfterEdit()
        if retSnap then
            tMapOptions.sSnapScaleMode = tComboSnapScaleMode[curSnap]
            resyncAllPlacedMeshes()
        end
        if bSnapChanged then pushUndoSnapshot() end
    end

    tImGui.Separator()
    drawLightPanel()

    tImGui.Separator()
    tImGui.Text(tLang.L('object_options'))
    local showObj = tImGui.Checkbox(tLang.L('show_scene_objects'), bShowSceneObjectMarkers)
    if showObj ~= bShowSceneObjectMarkers then bShowSceneObjectMarkers = showObj end
    if tImGui.Button(tLang.L('add_object'), tUtil.getResponsiveItemSize(item_width - 40)) then
        addSceneObjectMarker()
        pushUndoSnapshot()
    end
    for i, tObj in ipairs(tSceneObjects) do
        local isOpen = tImGui.TreeNodeEx(string.format('%s-%d##marker_tree', tLang.L('object'), i))
        if isOpen then
            local ret2, cur2 = tImGui.Combo('##marker_type' .. i, tComboObjectTypeIndexOf(tObj.type), tComboObjectType3d)
            if ret2 then
                tObj.type = tComboObjectType3d[cur2]
            end
            drawObjectMarkerFields(tObj, i)
            if tImGui.Button(tLang.L('delete') .. '##marker_del' .. i) then
                if tSceneObjectShapes[i] and tSceneObjectShapes[i].handle then tSceneObjectShapes[i].handle:destroy() end
                table.remove(tSceneObjects, i)
                table.remove(tSceneObjectShapes, i)
                pushUndoSnapshot()
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

            tImGui.Separator()
            tImGui.Text(tLang.L('mesh_dimensions_fmt'):format(tUtil.getShortName(entry.fileName)))
            -- Width/Height/Depth are read-only here -- they come straight off the mesh's physics
            -- box (INFO_PHYSICS::getBounds(), same value the Physics tab authors), not something
            -- this tab computes or lets you edit. Baked once per entry alongside its thumbnail
            -- (processThumbnailQueue); getOrCreateThumbnail keeps that bake queued even if the
            -- Mesh Selector window happens to be hidden right now.
            getOrCreateThumbnail(entry)
            if entry.physWidth then
                tImGui.Text(string.format('%s: %.2f', tLang.L('width'), entry.physWidth))
                tImGui.Text(string.format('%s: %.2f', tLang.L('height'), entry.physHeight))
                tImGui.Text(string.format('%s: %.2f', tLang.L('depth'), entry.physDepth))
                if entry.physicsBoundsSuspect then
                    tImGui.PushStyleColor('ImGuiCol_Text', {r = 1, g = 0.7, b = 0, a = 1})
                    tImGui.TextWrapped(tLang.L('thumb_physics_bounds_warning'))
                    tImGui.PopStyleColor(1)
                end
            else
                tImGui.TextDisabled(tLang.L('dimensions_computing'))
            end
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
    table.insert(tLayers, {
        name = 'Layer-' .. (#tLayers + 1), visible = true, fY = 0, offset = {x = 0, z = 0},
        bHalfOffsetX = false, bHalfOffsetZ = false,
    })
    iSelectedLayer = #tLayers
end

function drawLayerTab(item_width)
    if tImGui.Button(tLang.L('add_layer'), tUtil.getResponsiveItemSize(item_width - 40)) then
        addLayer()
    end

    for i, layer in ipairs(tLayers) do
        -- Visibility toggle lives on the header row (not buried inside the expanded node) so a
        -- layer can be hidden/shown without expanding it first, matching a typical layer-panel
        -- eye-toggle convention.
        local visHdr = tImGui.Checkbox('##layer_visible_hdr' .. i, layer.visible)
        if visHdr ~= layer.visible then
            layer.visible = visHdr
            for _, tPlaced in ipairs(tPlacedMeshes) do
                if tPlaced.layerIndex == i then
                    applyPlacedMeshVisibility(tPlaced)
                end
            end
        end
        tImGui.SameLine()
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

            -- Brick-wall stagger only makes sense against the integer cell lattice (Orthogonal/
            -- Isometric) -- Free mode has no cellX/cellZ "line" to key off of.
            if tMapOptions.sMapType ~= 'Free' then
                local hx = tImGui.Checkbox(tLang.L('half_offset_x_odd_line') .. '##layer_half_x' .. i, layer.bHalfOffsetX)
                local hz = tImGui.Checkbox(tLang.L('half_offset_z_odd_line') .. '##layer_half_z' .. i, layer.bHalfOffsetZ)
                if hx ~= layer.bHalfOffsetX or hz ~= layer.bHalfOffsetZ then
                    layer.bHalfOffsetX = hx
                    layer.bHalfOffsetZ = hz
                    for _, tPlaced in ipairs(tPlacedMeshes) do
                        if tPlaced.layerIndex == i then
                            syncPlacedMeshTransform(tPlaced)
                        end
                    end
                end
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
    -- Placed Meshes: the full editing row (name/Selected checkbox/Free-mode attach+Y/Delete).
    -- Collapsed by default -- kept behind a tree node so a scene with many placed meshes doesn't
    -- force every one of their thumbnails to be generated/queued on every frame this tab is drawn.
    if tImGui.TreeNodeEx(tLang.L('placed_meshes_fmt'):format(#tPlacedMeshes) .. '##placed_meshes_tree') then
        local thumbSize = {x = iSizeMeshOnSelector, y = iSizeMeshOnSelector}
        for i, tPlaced in ipairs(tPlacedMeshes) do
            if drawPlacedMeshRow(i, tPlaced, thumbSize, true) then
                break
            end
        end
        tImGui.TreePop()
    end

    tImGui.Separator()
    -- Selected Meshes: a live filtered view of whichever placed meshes are currently selected
    -- (3D-view click/Ctrl+click/Shift+drag, or the Selected checkbox above) -- lets the user dial
    -- in rotation for just the active selection without scrolling the full Placed Meshes list.
    -- Also collapsed by default and only walks tPlacedMeshes/generates thumbnails while open.
    local tSelectedIndices = {}
    for i, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected then table.insert(tSelectedIndices, i) end
    end
    if tImGui.TreeNodeEx(tLang.L('selected_meshes_fmt') .. '##selected_meshes_tree') then
        local thumbSize = {x = iSizeMeshOnSelector, y = iSizeMeshOnSelector}
        for _, i in ipairs(tSelectedIndices) do
            drawPlacedMeshRow(i, tPlacedMeshes[i], thumbSize, false)
        end
        tImGui.TreePop()
    end
end

-- Draws one placed-mesh thumbnail row (thumbnail + name + Y rotation DragFloat), shared by the
-- Placed Meshes and Selected Meshes tree nodes above. `bFullControls` adds the Selected checkbox,
-- Free-mode attach/Y fields, and Delete button -- Selected Meshes only ever shows an already-
-- selected mesh, so those controls would be redundant/out of place there.
-- Returns true if this row deleted itself (index i is no longer valid -- caller must stop
-- iterating tPlacedMeshes by that same index).
function drawPlacedMeshRow(i, tPlaced, thumbSize, bFullControls)
    -- Same cache/queue this mesh's entry already uses in the Mesh Selector (getOrCreateThumbnail
    -- keys by fileName) -- registerMeshSetEntry is idempotent, returning the existing
    -- tMeshSetEntries entry for an already-registered fileName, so this shares one bake/texture
    -- across every placed instance of the same mesh instead of generating a thumbnail per instance.
    local entry = registerMeshSetEntry(tPlaced.fileName)
    local texInfo = entry and getOrCreateThumbnail(entry) or nil
    if texInfo then
        tImGui.Image(texInfo, thumbSize, {x = 0, y = 0}, {x = 1, y = 1})
    else
        tImGui.Button(tLang.L('generating') .. '##placedpending' .. i, thumbSize)
    end

    tImGui.SameLine()
    tImGui.BeginGroup()
    if tPlaced.isSelected then
        tImGui.PushStyleColor('ImGuiCol_Text', {r = 0, g = 1, b = 0, a = 1})
    end
    tImGui.Text(tUtil.getShortName(tPlaced.fileName))
    if tPlaced.isSelected then
        tImGui.PopStyleColor(1)
    end

    if bFullControls then
        local sel = tImGui.Checkbox(tLang.L('selected') .. '##placed_sel' .. i, tPlaced.isSelected)
        if sel ~= tPlaced.isSelected then tPlaced.isSelected = sel end
    end

    -- Stored/applied in radians (matching rotationY everywhere else in this editor, see
    -- rotateSelectedMeshes) but shown in degrees, same convention as the mesh Offset rotation
    -- fields above. Unbounded (min=-360, max=360)
    local cRotY, rotYDeg = tImGui.DragFloat(tLang.L('placed_rotation_y') .. '##placed_roty' .. i,
        math.deg(tPlaced.rotationY or 0), 1, -360, 360, '%.2f')
    -- Must be read immediately after the widget (ImGui's "last item" state) -- fires exactly once,
    -- on the frame the drag ends having actually changed the value, so an active drag collapses
    -- into one undo entry instead of one per dragged frame.
    local bRotYDragFinished = tImGui.IsItemDeactivatedAfterEdit()
    
    if tImGui.Button("+" .. '##add_1_deg_' .. i, {x = 40, y = 30}) then
        rotYDeg = rotYDeg + 1
        tPlaced.rotationY = math.rad(rotYDeg)
        syncPlacedMeshTransform(tPlaced)
        pushUndoSnapshot()
    end
    tImGui.SameLine()
    if tImGui.Button("-" .. '##sub_1_deg_' .. i, {x = 40, y = 30}) then
        rotYDeg = rotYDeg - 1
        tPlaced.rotationY = math.rad(rotYDeg)
        syncPlacedMeshTransform(tPlaced)
        pushUndoSnapshot()
    end
    if cRotY then
        tPlaced.rotationY = math.rad(rotYDeg)
        syncPlacedMeshTransform(tPlaced)
    end
    if bRotYDragFinished then
        pushUndoSnapshot()
    end

    local bDeleted = false
    if bFullControls then
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
            pushUndoSnapshot()
            bDeleted = true
        end
    end
    tImGui.EndGroup()
    tImGui.Separator()
    return bDeleted
end

-- Placed meshes currently selected in the 3D view (Shift+drag rectangle, click, Ctrl+click)
-- are tracked per-instance via tPlaced.isSelected; this collapses that into a fileName -> true
-- lookup so drawMeshSelector can highlight every Mesh Selector entry that has at least one
-- selected instance in the scene, independent of sMeshSelectedForPlacement (the separate
-- "next mesh to place" selection).
function getSceneSelectedFileNames()
    local tSelected = {}
    for _, tPlaced in ipairs(tPlacedMeshes) do
        if tPlaced.isSelected then tSelected[tPlaced.fileName] = true end
    end
    return tSelected
end

function drawMeshSelector(xStart)
    tUtil.setInitialWindowPositionDown(tLang.L(tWindowsTitle.title_mesh_selector), xStart + 25, 0.3, 180)
    local isOpen, closedClicked = tImGui.Begin(tLang.L(tWindowsTitle.title_mesh_selector), true, 0)
    if isOpen then
        local tFiltered = getFilteredMeshSetEntries()
        local tSceneSelected = getSceneSelectedFileNames()
        local winWidth = tImGui.GetWindowWidth()
        local xLast = tImGui.GetCursorPosX()
        for n, entry in ipairs(tFiltered) do
            local texInfo = getOrCreateThumbnail(entry)
            local size = {x = iSizeMeshOnSelector, y = iSizeMeshOnSelector}
            if texInfo then
                -- On the Mesh property tab this window doubles as that tab's own inline picker
                -- (iPreviewedMeshSetIndex / updatePreviewMesh3d) rather than the Layer tab's
                -- click-to-place target, so both the highlight and the click handler track
                -- whichever selection is meaningful for the active tab.
                local activeForPlacement = (sMeshSelectedForPlacement == entry.fileName)
                local activeForPreview = (sActiveTab == 'mesh_set' and iPreviewedMeshSetIndex == n)
                local selectedInScene = tSceneSelected[entry.fileName] == true
                local highlight = nil
                if selectedInScene and (activeForPlacement or activeForPreview) then
                    highlight = {r = 0, g = 1, b = 1, a = 1}
                elseif activeForPlacement or activeForPreview then
                    highlight = {r = 0, g = 1, b = 0, a = 1}
                elseif selectedInScene then
                    highlight = {r = 1, g = 0.55, b = 0, a = 1}
                end
                if highlight then
                    tImGui.PushStyleColor('ImGuiCol_Button', highlight)
                end
                if tImGui.ImageButton('mesh_selector_btn_' .. n, texInfo, size, {x = 0, y = 0}, {x = 1, y = 1}) then
                    sMeshSelectedForPlacement = entry.fileName
                    if sActiveTab == 'mesh_set' then
                        iPreviewedMeshSetIndex = n
                        updatePreviewMesh3d(entry)
                    end
                end
                if highlight then
                    tImGui.PopStyleColor(1)
                end
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tUtil.getShortName(entry.fileName))
                    if entry.physicsBoundsSuspect then
                        tImGui.PushStyleColor('ImGuiCol_Text', {r = 1, g = 0.7, b = 0, a = 1})
                        tImGui.TextWrapped(tLang.L('thumb_physics_bounds_warning'))
                        tImGui.PopStyleColor(1)
                    end
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
    -- "Fill layer" is a Map edition (Layer tab) action against the active grid -- keep it out of
    -- reach while the Mesh Selector is showing on the Mesh property tab, where there's no grid in
    -- view and a stray right-click shouldn't silently bulk-edit the layer the user isn't looking at.
    if sActiveTab == 'layer' and sMeshSelectedForPlacement then
        if tImGui.BeginPopupContextVoid('##fill_layer_menu', ImGuiPopupFlags_MouseButtonRight) then
            if tMapOptions.sMapType ~= 'Free' then
                if tImGui.Selectable(tLang.L('fill_layer_with_selected_mesh')) then
                    fillActiveLayerWithMesh(sMeshSelectedForPlacement)
                    -- One snapshot for the whole fill, not per-cell -- pushed here (the caller),
                    -- not inside fillActiveLayerWithMesh itself, since that function is also called
                    -- nowhere else that would want per-cell history.
                    pushUndoSnapshot()
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
    -- Grid/origin-lines visibility is per-tab (tShowGridByTab/tShowOriginByTab) -- re-apply the
    -- newly active tab's own flags, since these are real scene objects/line pools that keep
    -- whatever visibility the previously active tab last left them at otherwise.
    rebuildGridVisual()
    local showOrigin = tShowOriginByTab[sTab]
    if tOriginLine3dX then tOriginLine3dX.visible = showOrigin end
    if tOriginLine3dY then tOriginLine3dY.visible = showOrigin end
    if tOriginLine3dZ then tOriginLine3dZ.visible = showOrigin end
end

function main_tab_bar()
    if not bEnableMainTabBar then return end
    -- max_width used to equal the initial width (260, 260), which left the window unable to be
    -- dragged any wider at all -- a direct user report. Now gives real resize headroom.
    tUtil.setInitialWindowPositionLeft(tLang.L(tWindowsTitle.title_scene3d), 0, 0, iMainPanelWidth, iMainPanelWidth + 300)
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

    if (sActiveTab == 'layer' or sActiveTab == 'mesh_set') and bShowMeshSelector then
        drawMeshSelector(iMainPanelWidth)
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
            if tImGui.MenuItem(tLang.L('export_scene')) then
                onExportGameScene3d()
            end
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L('menu_quit'), 'Alt+F4') then
                mbm.quit()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('menu_edit')) then
            if tImGui.MenuItem(tLang.L('undo'), 'Ctrl+Z', false, canUndoScene3d()) then
                onUndoScene3d()
            end
            if tImGui.MenuItem(tLang.L('redo'), 'Ctrl+Y', false, canRedoScene3d()) then
                onRedoScene3d()
            end
            tImGui.Separator()
            local iEditMenuSelectedCount = countSelectedMeshesOnActiveLayer()
            if tImGui.MenuItem(tLang.L('copy_btn'), 'Ctrl+C', false, iEditMenuSelectedCount > 0) then
                copySelectedMeshes()
            end
            if tImGui.MenuItem(tLang.L('paste_btn'), 'Ctrl+V', false,
                    tCopyBuffer ~= nil and #tCopyBuffer > 0 and iEditMenuSelectedCount == 1) then
                pasteCopiedMeshes()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('menu_mesh')) then
            if tImGui.MenuItem(tLang.L('add_mesh_from_folder')) then
                onAddMeshFromFolder()
            end
            if tImGui.MenuItem(tLang.L('add_mesh'), 'Ctrl+M') then
                onAddMeshDirect()
            end
            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('layer_options')) then
            if sActiveTab == 'layer' then
                -- Select All / Invert are scoped to the active layer only (same reasoning as
                -- updateHoverHighlight/finalizeRectSelection) -- replaces the whole selection, so
                -- any stray selection left over in another layer is cleared as a side effect,
                -- exactly like a plain click/drag-select already does.
                if tImGui.MenuItem(tLang.L('select_all_meshes'), 'Ctrl+A') then
                    for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = (tPlaced.layerIndex == iSelectedLayer) end
                end
                if tImGui.MenuItem(tLang.L('invert_selected_meshes'), 'Ctrl+I') then
                    for _, tPlaced in ipairs(tPlacedMeshes) do
                        tPlaced.isSelected = (tPlaced.layerIndex == iSelectedLayer) and not tPlaced.isSelected
                    end
                end
                if tImGui.MenuItem(tLang.L('unselect_all_meshes'), 'Ctrl+U') then
                    for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = false end
                end
                if tImGui.MenuItem(tLang.L('delete_mesh_selected'), 'Delete') then
                    for i = #tPlacedMeshes, 1, -1 do
                        if tPlacedMeshes[i].isSelected and tPlacedMeshes[i].layerIndex == iSelectedLayer then removePlacedMesh(i) end
                    end
                    pushUndoSnapshot()
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

            tImGui.Separator()
            local retAsync = tImGui.Checkbox(tLang.L('async_mesh_load_export'), tOptionsEditor.bAsyncMeshLoad)
            if retAsync ~= tOptionsEditor.bAsyncMeshLoad then tOptionsEditor.bAsyncMeshLoad = retAsync end
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L('async_mesh_load_export_help'))

            if tImGui.Button(tLang.L('play'), tUtil.getResponsiveItemSize(200)) then
                onPlay3d()
            end
            tImGui.SameLine()
            tImGui.TextDisabled('F5')

            tImGui.Text(tLang.L('execute_script'))
            tImGui.SameLine()
            tImGui.HelpMarker(tLang.L('help_execute_script_test'))
            if tImGui.Button('...##exec_script', {x = 30, y = 0}) then
                local fileName = mbm.openFile(tOptionsEditor.sCurrentScriptExecution, '*.lua')
                if fileName then tOptionsEditor.sCurrentScriptExecution = fileName end
            end

            -- Without a driver script, Play (onPlay3d) launches the bare exported/module file
            -- directly -- it defines no onInitScene/onLoop of its own (it ends in `return
            -- tScene3d`, meant to be `require`d), so a fresh engine instance loading it as --scene
            -- does nothing at all (blank window). createBasicScriptForScene3d generates a small
            -- driver that requires the scene module and actually calls tScene:load(), mirroring
            -- scene_editor2d.lua's own createBasicScriptForScene -- this button was missing here
            -- entirely, which is why Run/Play did not work.
            if tOptionsEditor.sCurrentScriptExecution:len() == 0 then
                tImGui.SameLine()
                if tImGui.Button(tLang.L('create_it_for_me'), tUtil.getResponsiveItemSize(160)) then
                    if sFileNameScene3d ~= '' then
                        local sceneModulePath = sFileNameScene3d:gsub('%.scene3d%-edit%.lua$', '-scene3d.lua')
                        local ok, err = writeScene3d(sceneModulePath, tOptionsEditor.bAsyncMeshLoad, true)
                        if ok then
                            createBasicScriptForScene3d(sceneModulePath)
                        else
                            tUtil.showMessageWarn(err or 'Failed to write the exported scene file')
                        end
                    else
                        tUtil.showMessageWarn(tLang.L('no_scene_loaded_for_script'))
                    end
                end
            end

            if tOptionsEditor.sCurrentScriptExecution and tOptionsEditor.sCurrentScriptExecution:len() > 0 then
                tImGui.TextDisabled(tUtil.getShortName(tOptionsEditor.sCurrentScriptExecution))
                tImGui.SameLine()
                tImGui.HelpMarker(tOptionsEditor.sCurrentScriptExecution)
                tImGui.SameLine()
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r = 1, g = 0, b = 0.3, a = 1})
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r = 0, g = 0, b = 0.3, a = 0})
                if tImGui.Button('X', {x = 40, y = 0}) then
                    tOptionsEditor.sCurrentScriptExecution = ''
                end
                tImGui.PopStyleColor(2)
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('clear_script'))
                    tImGui.EndTooltip()
                end
            else
                tImGui.TextDisabled(tLang.L('none'))
            end

            tImGui.EndMenu()
        end

        if tImGui.BeginMenu(tLang.L('general_options')) then
            local show = tImGui.Checkbox(tLang.L('show_mesh_map'), bEnableMainTabBar)
            if show ~= bEnableMainTabBar then bEnableMainTabBar = show end

            tImGui.Separator()
            -- Grid/origin-lines visibility is per-tab (tShowGridByTab/tShowOriginByTab) -- this
            -- menu is drawn every frame regardless of which tab is active, so it always reads/
            -- writes whichever tab's own flag is current (sActiveTab), same as the orbit camera.
            local showGrid = tImGui.Checkbox(tLang.L('show_grid') .. '##ShowGrid', tShowGridByTab[sActiveTab])
            if showGrid ~= tShowGridByTab[sActiveTab] then
                tShowGridByTab[sActiveTab] = showGrid
                rebuildGridVisual()
            end
            local origin = tImGui.Checkbox(tLang.L('enable_origin_lines'), tShowOriginByTab[sActiveTab])
            if origin ~= tShowOriginByTab[sActiveTab] then
                tShowOriginByTab[sActiveTab] = origin
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

function getHeader3d(fileName, bIsExport)
    local sHeader
    if bIsExport then
        sHeader = '--[[\n' .. [[
    Scene 3d - this file is meant to be used in the engine mbm

    how to:
    * in your scene do:

        local tScene3d = require "SCENE_NAME"
        tScene3d:load(onProgress)

    * To retrieve mesh(es) use get, getObject or getAll:

        local tMesh = tScene3d:get(1)               -- by load-order index
        local tMesh = tScene3d:getObject('rock.msh') -- by name
        local tAll  = tScene3d:getAll('tree.msh')

    * To spawn a new instance at runtime:

        local tNew = tScene3d:addMesh('rock.msh')

    * To read back the scene's lighting:

        local tAmbient     = tScene3d:getAmbientLight()
        local tDirectional = tScene3d:getDirectionalLight()  -- {color=, dir=}
        local tPointLights = tScene3d:getPointLight()        -- no arg: all
        local tPointLight1 = tScene3d:getPointLight(1)       -- one, by index

    * To read back Map-tab marker objects (spawn points/paths/triggers):

        local tSpawn = tScene3d:getSceneObject('spawn_player')
        local tAll   = tScene3d:getAllSceneObjects()

]] .. ']]\n\n'
    else
        sHeader = '--[[\n' .. [[
    Scene 3d - EDITOR SAVE FILE (Scene Editor 3D)

    This file is Scene Editor 3D's own save format, meant to be reopened by the editor itself
    (File > Load, or onOpenScene3d) -- it is NOT meant to be `require`'d directly by a game.
    To get a clean, `require`-able scene for a real game to load, use File > Export Game Scene...
    instead, which writes a separate "<name>-scene3d.lua" file with a small, game-facing API
    (tScene3d:load/get/getObject/getAmbientLight/etc -- see that file's own header once exported).

]] .. ']]\n\n'
    end
    sHeader = sHeader:gsub('SCENE_NAME', tUtil.getShortName(fileName, false):gsub('%.lua$', ''))
    return sHeader
end

-- bAsyncMesh picks which of two literal code branches gets baked into the generated
-- tScene3d._addMesh for the 'mesh' render type (the only type with a real loadAsync in this
-- engine) -- the emitted file only ever contains ONE of the two, not a runtime branch on a
-- baked boolean.
function getSceneLoaderCode3d(bAsyncMesh)
    local sMeshLoadBranch, sAsyncQueueHelper
    if bAsyncMesh then
        -- Routed through _loadMeshAsyncQueued instead of calling mesh:loadAsync directly -- firing
        -- many concurrent loadAsync calls for the SAME file (e.g. a level with many placed
        -- instances of one prop) is a confirmed engine-level crash (SIGSEGV inside the async
        -- callback's Lua registry lookup, mesh-lua.cpp's onLoadAsyncMeshLua via
        -- MESH_MANAGER::pumpAsyncLoads -- see the async-mesh-load-concurrency-crash memory/commit
        -- history), reproduced via this exact Play-with-async-enabled path. The underlying pipeline
        -- bug itself is out of scope here (shared engine infrastructure, needs a dedicated
        -- investigation) -- this caps same-file concurrency to 1-in-flight at the Lua level, which
        -- is what the confirmed reproduction actually needed. Different files still load fully in
        -- parallel; only repeated instances of one file are serialized.
        sMeshLoadBranch = [[
        self:_loadMeshAsyncQueued(tInfo.fileName, finish)]]
        sAsyncQueueHelper = [[
tScene3d._loadMeshAsyncQueued = function(self, fileName, onLoaded)
    self.tAsyncQueueByFile = self.tAsyncQueueByFile or {}
    local queue = self.tAsyncQueueByFile[fileName]
    if not queue then
        queue = { bLoading = false, pending = {} }
        self.tAsyncQueueByFile[fileName] = queue
    end
    local function processNext()
        local req = table.remove(queue.pending, 1)
        if not req then
            queue.bLoading = false
            return
        end
        queue.bLoading = true
        local m = mesh:new('3d')
        m:loadAsync(fileName, function(self_mesh, success)
            req(success and self_mesh or nil)
            processNext()
        end)
    end
    table.insert(queue.pending, onLoaded)
    if not queue.bLoading then
        processNext()
    end
end
]]
    else
        sMeshLoadBranch = [[
        local m = mesh:new('3d')
        finish(m:load(tInfo.fileName) and m or nil)]]
        sAsyncQueueHelper = ''
    end

    local sCode = [[
-- Maps a file extension to this engine's render-type string, with no dependency on any
-- editor-only global (mesh_debug.lua's `meshDebug` tool does not exist in a shipped game).
local tExtToType3d = {
    msh = 'mesh', spt = 'sprite', ptl = 'particle', tile = 'tile', gif = 'gif',
}
local function inferType3dFromFileName(fileName)
    local ext = fileName:match('%.([%a%d]+)$')
    return ext and tExtToType3d[ext:lower()] or 'texture'
end

tScene3d.updateCamera = function(self)
    local cam = mbm.getCamera('3d')
    cam:setPos(self.fCamPos.x, self.fCamPos.y, self.fCamPos.z)
    cam:setFocus(self.fCamFocus.x, self.fCamFocus.y, self.fCamFocus.z)
end

@@ASYNC_QUEUE_HELPER@@
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
@@MESH_LOAD_BRANCH@@
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
        mbm.clearPointLights('3d')
        for _, pl in ipairs(self.tLightConfig.pointLights or {}) do
            mbm.addPointLight('3d', pl.x, pl.y, pl.z, pl.radius, pl.r, pl.g, pl.b, pl.a or 1)
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

tScene3d.getObject = function(self, name)
    local list = self.tMeshesLoadedDictionary[name]
    return list and list[1] or nil
end

tScene3d.getAll = function(self, name)
    return self.tMeshesLoadedDictionary[name] or {}
end

tScene3d.getAmbientLight = function(self)
    return self.tLightConfig and self.tLightConfig.ambientColor
end

tScene3d.getDirectionalLight = function(self)
    if not self.tLightConfig then return nil end
    return {color = self.tLightConfig.directionalColor, dir = self.tLightConfig.directionalDir}
end

tScene3d.getPointLight = function(self, index)
    local pointLights = self.tLightConfig and self.tLightConfig.pointLights or {}
    if index then
        return pointLights[index]
    end
    return pointLights
end

tScene3d.getSceneObject = function(self, name)
    for _, tObj in ipairs(self.tSceneObjects or {}) do
        if tObj.name == name then
            return tObj
        end
    end
    return nil
end

tScene3d.getAllSceneObjects = function(self)
    return self.tSceneObjects or {}
end

tScene3d.addMesh = function(self, nameOrFileName, sTypeOverride)
    local tInfo = { fileName = nameOrFileName, type = sTypeOverride or inferType3dFromFileName(nameOrFileName), x = 0, y = 0, z = 0 }
    local result = nil
    self:_addMesh(tInfo, function(tObj) result = tObj end)
    return result
end

return tScene3d]]
    sCode = sCode:gsub('@@MESH_LOAD_BRANCH@@', (sMeshLoadBranch:gsub('%%', '%%%%')))
    sCode = sCode:gsub('@@ASYNC_QUEUE_HELPER@@', (sAsyncQueueHelper:gsub('%%', '%%%%')))
    return sCode
end

------------------------------------------------------------------------------------------------------------------
-- Undo / redo (Map edition: place/delete/rotate placed meshes; Main Scene: grid/map options,
-- Object Options add/delete) -- one shared history across both tabs, mirroring tilemap_editor.lua's
-- own model (a linear stack + cursor; a new action after an undo truncates the redo branch) but
-- implemented as plain in-memory Lua snapshots instead of tilemap's native disk-backed binary
-- snapshots -- this editor's undoable state (placed-mesh records, grid options, object markers) is
-- tiny plain data, so there's no need for temp files. Restoring is always a full teardown/rebuild of
-- tPlacedMeshes (never touching a captured live RENDERIZABLE handle -- none is ever captured in the
-- first place), the same "destroy everything, recreate fresh from data" approach applyLoadedScene3d
-- already uses for file load, just leaner: it skips the camera/mesh-set-folder-rescan/thumbnail-
-- requeue/loading-progress-overlay steps a real file load needs, none of which ever change here, so
-- undo/redo stays instant instead of feeling like a mini file-load on every step.
------------------------------------------------------------------------------------------------------------------

tUndoHistory     = {} -- array of snapshot tables; tUndoHistory[iUndoIndex] is always the CURRENT state
iUndoIndex       = 0
MAX_UNDO_HISTORY = 100

-- Only ever called on the known-plain-data subset captured by captureScene3dSnapshot below (grid
-- options, object markers) -- never on tPlacedMeshes itself, which holds live tObj/tHighlightShape
-- userdata that must never be captured or copied (see captureScene3dSnapshot's own tPlacedMeshInfo
-- construction, which builds a fresh plain-data record per entry instead of copying tPlaced itself).
local function deepCopyPlainTable(value)
    if type(value) ~= 'table' then return value end
    local out = {}
    for k, v in pairs(value) do
        out[k] = deepCopyPlainTable(v)
    end
    return out
end

-- Captures everything undo/redo needs as one plain-data table -- no RENDERIZABLE/userdata anywhere
-- in it. tLayers is deliberately NOT captured: nothing this history tracks (place/delete/rotate a
-- mesh, edit grid options, add/delete an Object Options marker) ever mutates tLayers, so it stays
-- constant across every undo/redo step and doesn't need its own snapshot/restore.
function captureScene3dSnapshot()
    local tPlacedInfo = {}
    for i, tPlaced in ipairs(tPlacedMeshes) do
        tPlacedInfo[i] = {
            fileName = tPlaced.fileName, type = tPlaced.type, layerIndex = tPlaced.layerIndex,
            cellX = tPlaced.cellX, cellZ = tPlaced.cellZ,
            freeX = tPlaced.freeX, freeZ = tPlaced.freeZ, freeY = tPlaced.freeY,
            bAttachedToLayer = tPlaced.bAttachedToLayer,
            rotationY = tPlaced.rotationY or 0,
            sx = tPlaced.scale.x, sy = tPlaced.scale.y, sz = tPlaced.scale.z,
        }
    end
    return {
        tPlacedMeshInfo = tPlacedInfo,
        tMapOptions     = deepCopyPlainTable(tMapOptions),
        tSceneObjects   = deepCopyPlainTable(tSceneObjects),
    }
end

-- Restores a snapshot captured above. Always a full teardown/rebuild -- see the section banner
-- comment for why this is deliberately leaner than applyLoadedScene3d rather than reusing it.
function restoreScene3dSnapshot(snapshot)
    for i = #tPlacedMeshes, 1, -1 do removePlacedMesh(i) end
    tSceneObjects       = deepCopyPlainTable(snapshot.tSceneObjects)
    tSceneObjectShapes  = {}
    tMapOptions         = deepCopyPlainTable(snapshot.tMapOptions)
    rebuildGridVisual()
    for _, tInfo in ipairs(snapshot.tPlacedMeshInfo) do
        local tPlaced = addPlacedMesh(tInfo.fileName, tInfo.type, tInfo.layerIndex,
            tInfo.cellX, tInfo.cellZ, tInfo.freeX, tInfo.freeZ, true)
        tPlaced.bAttachedToLayer = tInfo.bAttachedToLayer
        tPlaced.freeY            = tInfo.freeY
        tPlaced.rotationY        = tInfo.rotationY
        tPlaced.scale            = {x = tInfo.sx, y = tInfo.sy, z = tInfo.sz}
        syncPlacedMeshTransform(tPlaced)
    end
end

-- Called once from onInitScene (fresh/blank editor state) and once at the end of
-- applyLoadedScene3d (a newly loaded file) -- undo must never cross either boundary: the history
-- would otherwise reference placed-mesh layerIndex positions from before the load/reset, which may
-- no longer mean the same thing (or exist at all) in tLayers afterward. Mirrors tilemap_editor.lua's
-- own clearHistory()+addHistoric() pair on file open.
function resetUndoHistory()
    tUndoHistory = {}
    iUndoIndex   = 0
    pushUndoSnapshot()
end

-- Call AFTER a discrete, already-completed edit (place one mesh, delete N selected meshes, a
-- finished rotation drag, a committed grid-option change, add/delete an Object Options marker).
-- Never called mid-drag/mid-edit -- callers that touch a per-frame-changing widget (a rotation
-- DragFloat, a grid InputFloat/InputInt's held +/- spinner) gate this behind
-- tImGui.IsItemDeactivatedAfterEdit() so a whole drag/hold collapses into exactly one entry.
function pushUndoSnapshot()
    local snap = captureScene3dSnapshot()
    if iUndoIndex == #tUndoHistory then
        table.insert(tUndoHistory, snap)
        iUndoIndex = #tUndoHistory
    else
        -- Cursor isn't at the tip (a previous undo happened, then this new edit arrived) --
        -- overwrite forward from here and drop everything past it, the same "new edit destroys
        -- the redo branch" rule tilemap_editor.lua's own addHistoric() enforces.
        iUndoIndex = iUndoIndex + 1
        tUndoHistory[iUndoIndex] = snap
        for i = #tUndoHistory, iUndoIndex + 1, -1 do
            table.remove(tUndoHistory, i)
        end
    end
    if #tUndoHistory > MAX_UNDO_HISTORY then
        table.remove(tUndoHistory, 1)
        iUndoIndex = iUndoIndex - 1
    end
end

function canUndoScene3d()
    return iUndoIndex > 1
end

function canRedoScene3d()
    return iUndoIndex < #tUndoHistory
end

function onUndoScene3d()
    if not canUndoScene3d() then
        tUtil.showMessageWarn(tLang.L('no_more_undo'))
        return
    end
    iUndoIndex = iUndoIndex - 1
    restoreScene3dSnapshot(tUndoHistory[iUndoIndex])
end

function onRedoScene3d()
    if not canRedoScene3d() then
        tUtil.showMessageWarn(tLang.L('no_more_redo'))
        return
    end
    iUndoIndex = iUndoIndex + 1
    restoreScene3dSnapshot(tUndoHistory[iUndoIndex])
end

function writeScene3d(fileName, bAsyncMesh, bIsExport)
    local oldLocale = os.setlocale(nil, 'numeric')
    os.setlocale('C', 'numeric')
    local fp = io.open(fileName, 'w')
    if not fp then
        os.setlocale(oldLocale, 'numeric')
        return false, string.format(tLang.L('could_not_open_for_write_fmt'), fileName)
    end

    fp:write(getHeader3d(fileName, bIsExport))

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

    -- tUtil.save(name, value, tOut, onSaveUserData) mutates tOut (an array of source-code lines);
    -- it does not return a string -- match the same convention already used successfully by
    -- scene_editor2d.lua's own writeScene(), instead of concatenating a non-existent return value.
    local function writeSavedTable(varName, value)
        local tOut = {}
        tUtil.save(varName, value, tOut, nil)
        for _, sLine in ipairs(tOut) do
            fp:write(sLine .. '\n')
        end
    end

    writeSavedTable('tScene3d.fCamPos', tOptionsEditor.fSceneCamPos)
    writeSavedTable('tScene3d.fCamFocus', tOptionsEditor.fSceneCamFocus)
    writeSavedTable('tScene3d.tLightConfig', tLightConfig)
    writeSavedTable('tScene3d.tMeshOffsets', tMeshOffsets)
    writeSavedTable('tScene3d.tSceneObjects', tSceneObjects)

    -- Full editor-authoring state (map/grid settings, layers, per-mesh cell/attachment info) is
    -- Save-only -- Export must keep producing a clean, game-facing file with no editor concepts in
    -- it, per the earlier, separate "no editor information" requirement for Export Game Scene.
    if not bIsExport then
        writeSavedTable('tScene3d.tMapOptions', tMapOptions)
        writeSavedTable('tScene3d.tLayers', tLayers)
        -- Mesh Set tab's asset-browser root (Mesh Set Folder) -- editor-authoring-only, same as
        -- tMapOptions/tLayers/tCamByTab above, so it has no place in an Export'd game-facing file.
        writeSavedTable('tScene3d.sMeshSetFolder', sMeshSetFolder)
        writeSavedTable('tScene3d.tOptionsEditor', {
            iIndexResolution        = tOptionsEditor.iIndexResolution,
            bInvertResolution       = tOptionsEditor.bInvertResolution,
            sCurrentScriptExecution = tOptionsEditor.sCurrentScriptExecution,
            fSceneCamPos            = tOptionsEditor.fSceneCamPos,
            fSceneCamFocus          = tOptionsEditor.fSceneCamFocus,
        })
        -- The editor's own per-tab orbit-navigation viewport camera (azimuth/elevation/distance/
        -- focus) -- distinct from tOptionsEditor.fSceneCamPos/fSceneCamFocus above, which is the
        -- exported game's initial camera. Without this, "Main Scene"/"Map edition" always reopened
        -- framed at whatever the default orbit happened to be, not where the scene was last viewed.
        writeSavedTable('tScene3d.tCamByTab', tCamByTab)
    end

    fp:write('tScene3d.tPlacedMeshInfo = {\n')
    for i, tPlaced in ipairs(tPlacedMeshes) do
        local x, y, z = resolvePlacedMeshWorldPos(tPlaced)
        if bIsExport then
            fp:write(string.format(
                '[%d]={fileName=%q,type=%q,x=%g,y=%g,z=%g,rotationY=%g,sx=%g,sy=%g,sz=%g},\n',
                i, tPlaced.fileName, tPlaced.type, x, y, z, tPlaced.rotationY or 0,
                tPlaced.scale.x, tPlaced.scale.y, tPlaced.scale.z))
        else
            fp:write(string.format(
                '[%d]={fileName=%q,type=%q,x=%g,y=%g,z=%g,rotationY=%g,sx=%g,sy=%g,sz=%g,'
                .. 'layerIndex=%d,cellX=%d,cellZ=%d,freeX=%g,freeZ=%g,freeY=%g,bAttachedToLayer=%s},\n',
                i, tPlaced.fileName, tPlaced.type, x, y, z, tPlaced.rotationY or 0,
                tPlaced.scale.x, tPlaced.scale.y, tPlaced.scale.z,
                tPlaced.layerIndex or 1, tPlaced.cellX or 0, tPlaced.cellZ or 0,
                tPlaced.freeX or 0, tPlaced.freeZ or 0, tPlaced.freeY or 0,
                tostring(tPlaced.bAttachedToLayer == true)))
        end
    end
    fp:write('}\n\n')

    fp:write(getSceneLoaderCode3d(bAsyncMesh))
    fp:close()
    os.setlocale(oldLocale, 'numeric')
    return true
end

-- Applies a loaded/deserialized scene3d-edit.lua module table to the editor's live global state.
-- Extracted out of onOpenScene3d so it can be exercised directly in a headless test (no
-- mbm.openFile dialog involved) -- also the single place that must tolerate old-format save files
-- (saved before tMapOptions/tLayers/per-mesh cell data existed), so every new field read here falls
-- back to today's pre-existing behavior when absent.
function applyLoadedScene3d(tLoaded)
    tMeshOffsets  = tLoaded.tMeshOffsets or {}
    tLightConfig  = tLoaded.tLightConfig or tLightConfig
    computeOrbitFromDirectionalDir() -- re-derive the trackball's angles from whatever loaded
    -- applyLightConfigToEngine() only ever gets called from UI actions (the enabled checkbox,
    -- color pickers, add/remove point light) -- restoring tLightConfig's DATA here doesn't, on its
    -- own, push it to the engine (mbm.setLightEnabled/setAmbientLight/etc), so a reload always
    -- looked like lights were off until some unrelated light edit incidentally called it. Apply it
    -- explicitly now that the real data is in place.
    applyLightConfigToEngine()
    tSceneObjects = tLoaded.tSceneObjects or {}
    tSceneObjectShapes = {}
    for i = #tPlacedMeshes, 1, -1 do removePlacedMesh(i) end

    -- Map/grid settings: old-format files have no tMapOptions key at all, so this simply leaves
    -- whatever the current session already has (matches pre-existing behavior for such files).
    tMapOptions = tLoaded.tMapOptions or tMapOptions

    -- Layers must be rebuilt BEFORE placed meshes, in the saved order, since each placed mesh's
    -- layerIndex is a positional index into this array -- restoring one without the other would
    -- silently attach meshes to the wrong layer's Y-height/offset.
    tLayers = {}
    if tLoaded.tLayers and #tLoaded.tLayers > 0 then
        for _, l in ipairs(tLoaded.tLayers) do
            table.insert(tLayers, {
                name = l.name, visible = l.visible, fY = l.fY,
                offset = {x = l.offset.x, z = l.offset.z},
                bHalfOffsetX = l.bHalfOffsetX or false, bHalfOffsetZ = l.bHalfOffsetZ or false,
            })
        end
    else
        addLayer() -- old-format fallback: exactly today's pre-existing single-default-layer behavior
    end
    iSelectedLayer = #tLayers > 0 and 1 or 0
    rebuildGridVisual() -- grid must reflect the just-restored tMapOptions/tLayers before placing meshes

    -- Mesh Set tab's asset-browser root -- old-format files have no such key, so this simply
    -- leaves whatever the current session already has. Re-scan it now, BEFORE the placed-mesh
    -- loop below (which registers additional entries straight from tPlacedMeshInfo), so the
    -- Mesh Set / Mesh Selector panels start from the same folder listing the scene was saved
    -- with, without losing that loop's own "still show placed meshes even if the folder isn't
    -- reachable" guarantee (registerMeshSetEntry dedups, so entries from both sources merge).
    sMeshSetFolder = tLoaded.sMeshSetFolder or sMeshSetFolder
    scanMeshSetFolder(sMeshSetFolder)

    -- Also treat every folder a placed mesh actually came from as "known" and register everything
    -- in it, not just the placed file itself -- a scene may place only one or two assets out of a
    -- whole pack (e.g. one hex tile repeated hundreds of times to fill a grid) while the rest of
    -- that pack was still being actively browsed/used to build the scene, and should still be
    -- there to keep building with after a reload, exactly as if it had just been browsed again.
    -- Editor-only convenience: writeScene3d still only ever writes the placed instances themselves
    -- (tPlacedMeshInfo below), never a whole-folder listing, so Export/the shipped game scene is
    -- unaffected -- it keeps including only the meshes actually used.
    local tScannedDirs = {}
    for _, tInfo in ipairs(tLoaded.tPlacedMeshInfo or {}) do
        local dir = dirOf(tInfo.fileName)
        if dir ~= '' and not tScannedDirs[dir] then
            tScannedDirs[dir] = true
            registerMeshesFromFolder(dir)
        end
    end

    -- Synchronous on purpose: firing loadAsync for every placed mesh back-to-back (hundreds of
    -- concurrent in-flight requests, common for a large fill-the-grid scene) reproducibly crashes
    -- the process (SIGSEGV inside the Lua registry lookup in mesh-lua.cpp's async callback, hit
    -- while investigating a user report of a stuck/broken reload) -- a real bug in the async-load
    -- pipeline under heavy concurrency that's out of scope to redesign here. A one-time scene
    -- reload has no need for non-blocking loads in the first place, so bSync=true sidesteps it.
    beginLoadProgress(#(tLoaded.tPlacedMeshInfo or {}), tLang.L('loading_scene'))
    for _, tInfo in ipairs(tLoaded.tPlacedMeshInfo or {}) do
        local bAttached = tInfo.bAttachedToLayer
        if bAttached == nil then bAttached = false end -- old-format fallback: today's free-position behavior
        local tPlaced = addPlacedMesh(tInfo.fileName, tInfo.type, tInfo.layerIndex or 1,
            tInfo.cellX or 0, tInfo.cellZ or 0, tInfo.freeX or tInfo.x, tInfo.freeZ or tInfo.z, true)
        tPlaced.bAttachedToLayer = bAttached
        tPlaced.freeY = tInfo.freeY or tInfo.y
        tPlaced.rotationY = tInfo.rotationY
        tPlaced.scale = {x = tInfo.sx, y = tInfo.sy, z = tInfo.sz}
        -- Belt-and-suspenders on top of the per-folder registerMeshesFromFolder scan above (which
        -- covers the common case): register this exact placed file directly too, so it still shows
        -- up in the Mesh Set / Mesh Selector panels even if its folder listing failed/was partial
        -- (e.g. a permissions issue) or its path doesn't resolve to a real directory at all.
        registerMeshSetEntry(tInfo.fileName)
        -- addPlacedMesh's synchronous path already positioned tObj using its own hardcoded
        -- defaults (bAttachedToLayer=true, freeY=0, rotationY=0, scale={1,1,1}) before returning --
        -- the patches above only updated the tPlaced data record, not the live object's transform.
        -- Re-sync now that the real saved values are in place.
        syncPlacedMeshTransform(tPlaced)
    end

    -- Game-facing initial camera -- written by writeScene3d but never restored until now.
    tOptionsEditor.fSceneCamPos   = tLoaded.fCamPos or tOptionsEditor.fSceneCamPos
    tOptionsEditor.fSceneCamFocus = tLoaded.fCamFocus or tOptionsEditor.fSceneCamFocus
    if tLoaded.tOptionsEditor then
        tOptionsEditor.iIndexResolution        = tLoaded.tOptionsEditor.iIndexResolution or tOptionsEditor.iIndexResolution
        tOptionsEditor.bInvertResolution        = tLoaded.tOptionsEditor.bInvertResolution
        tOptionsEditor.sCurrentScriptExecution   = tLoaded.tOptionsEditor.sCurrentScriptExecution or tOptionsEditor.sCurrentScriptExecution
    end
    -- The editor's own per-tab orbit-navigation viewport camera (distinct from fSceneCamPos/
    -- fSceneCamFocus above, which is the game's initial camera) -- old-format files have no such
    -- key, so this simply leaves the current session's camera framing untouched for those. `cam3d`
    -- itself needs no re-aliasing here: main_tab_bar() re-derives `cam3d = tCamByTab[sTab]` every
    -- frame regardless.
    tCamByTab = tLoaded.tCamByTab or tCamByTab

    -- Thumbnails otherwise only get (re)queued the first time drawMeshSelector/the Properties tab
    -- actually draws an entry (getOrCreateThumbnail) -- and the tab this scene lands on right after
    -- load is 'map', which shows neither. The on-disk PNG cache also lives under the OS temp dir
    -- (onInitScene) and does not survive a reboot, so every entry here starts this session with
    -- thumbState == nil regardless of whether it was baked before. Queue them all now so baking is
    -- already under way by the time the user switches to a tab that displays them, instead of
    -- looking like the thumbnails are simply gone.
    for _, entry in ipairs(tMeshSetEntries) do
        getOrCreateThumbnail(entry)
    end

    -- Undo must never cross a file-load boundary -- see resetUndoHistory's own comment.
    resetUndoHistory()
end

function onOpenScene3d()
    local fileName = mbm.openFile(sFileNameScene3d, '*.scene3d-edit.lua')
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
    applyLoadedScene3d(tLoaded)
end

function onSaveScene3d()
    if sFileNameScene3d == '' then
        onSaveAsScene3d()
        return
    end
    local ok, err = writeScene3d(sFileNameScene3d, tOptionsEditor.bAsyncMeshLoad)
    if ok then
        tUtil.showMessage(tLang.L('scene_3d_saved_ok'))
    else
        tUtil.showMessageWarn(err)
    end
end

function onSaveAsScene3d()
    local fileName = mbm.saveFile(sFileNameScene3d, '*.scene3d-edit.lua')
    if not fileName then return end
    -- ensure the file always carries the .scene3d-edit.lua extension
    if not fileName:match('%.scene3d%-edit%.lua$') then
        fileName = fileName:gsub('%.lua$', '') .. '.scene3d-edit.lua'
    end
    sFileNameScene3d = fileName
    onSaveScene3d()
end

function onExportGameScene3d()
    -- derive the export default: my-scene.scene3d-edit.lua -> my-scene-scene3d.lua
    local exportDefault = sFileNameScene3d:gsub('%.scene3d%-edit%.lua$', '-scene3d.lua')
    if exportDefault == sFileNameScene3d then
        -- fallback for files not following the convention (or not yet saved at all)
        local base = (sFileNameScene3d ~= '' and sFileNameScene3d) or 'scene'
        exportDefault = base:gsub('%.lua$', '') .. '-scene3d.lua'
    end
    local fileName = mbm.saveFile(exportDefault, '*-scene3d.lua')
    if not fileName then return end

    local ok, err = writeScene3d(fileName, tOptionsEditor.bAsyncMeshLoad, true)
    if not ok then
        tUtil.showMessageWarn(err)
        return
    end
    tUtil.showMessage(string.format(tLang.L('scene_exported_ok_fmt'), fileName))
end

-- Modeled directly on scene_editor2d.lua's createBasicScriptForScene -- generates a small
-- `require`-and-drive script the "Run" menu's Play button can actually launch, since the
-- exported/module scene file itself defines no onInitScene/onLoop (see the menu_run comment at
-- the "create it for me" button for why that matters). `sFullSceneName` must be a real, already
-- written scene module path (the caller is responsible for exporting first) -- this only derives
-- the require name and writes the driver alongside it.
function createBasicScriptForScene3d(sFullSceneName)
    local tDefaultScene = [[
tScene = require "YOUR_SCENE3D"

local tMove  = {forward = 0, right = 0}
local fSpeed = 400 -- units/second; adjust to taste

function onInitScene()
    tScene:load(function(percent)
        print(string.format('Loading your scene %.1f', percent))
    end)
end

function onKeyDown(key)
    if key == mbm.getKeyCode('W') or key == mbm.getKeyCode('up') then tMove.forward = 1
    elseif key == mbm.getKeyCode('S') or key == mbm.getKeyCode('down') then tMove.forward = -1
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('left') then tMove.right = -1
    elseif key == mbm.getKeyCode('D') or key == mbm.getKeyCode('right') then tMove.right = 1
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('W') or key == mbm.getKeyCode('up')
    or key == mbm.getKeyCode('S') or key == mbm.getKeyCode('down') then tMove.forward = 0
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('left')
    or key == mbm.getKeyCode('D') or key == mbm.getKeyCode('right') then tMove.right = 0
    end
end

function onLoop(delta)
    if tMove.forward ~= 0 or tMove.right ~= 0 then
        local cam = mbm.getCamera('3d')
        local pos, focus = cam:getPos(), cam:getFocus()
        local fw, rg = cam:getNormal('F'), cam:getNormal('R')
        local fwLen = math.sqrt(fw.x * fw.x + fw.z * fw.z)
        local rgLen = math.sqrt(rg.x * rg.x + rg.z * rg.z)
        if fwLen > 1e-6 and rgLen > 1e-6 then
            local fwx, fwz = fw.x / fwLen, fw.z / fwLen
            local rgx, rgz = rg.x / rgLen, rg.z / rgLen
            local dx = (fwx * tMove.forward + rgx * tMove.right) * fSpeed * delta
            local dz = (fwz * tMove.forward + rgz * tMove.right) * fSpeed * delta
            cam:setPos(pos.x + dx, pos.y, pos.z + dz)
            cam:setFocus(focus.x + dx, focus.y, focus.z + dz)
        end
    end
    -- your logic here
end
]]

    local sProjectName = sFullSceneName:gsub("\\", '/')
    local tProjectName = sProjectName:split('/')
    sProjectName        = tProjectName[#tProjectName]:gsub('%..*$', '')
    tDefaultScene       = tDefaultScene:gsub('YOUR_SCENE3D', string.format('%s', sProjectName))
    local sTemp         = tProjectName[1] or ''
    local sSeparator    = '/'
    if mbm.is('windows') then
        sSeparator = '\\'
    end
    for i = 2, (#tProjectName - 1) do
        sTemp = sTemp .. sSeparator .. tProjectName[i]
    end

    local sFileToSave = string.format('%s%s%s-logic.lua', sTemp, sSeparator, sProjectName)
    sFileToSave = mbm.saveFile(sFileToSave, 'lua')
    if sFileToSave then
        local fp = io.open(sFileToSave, 'w')
        if fp then
            fp:write(tDefaultScene)
            fp:close()
            tOptionsEditor.sCurrentScriptExecution = sFileToSave
            tUtil.showMessage(tLang.L('file_created_ok'))
        else
            tUtil.showMessageWarn(string.format(tLang.L('could_not_open_for_write_fmt'), sFileToSave))
        end
    end
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
        local ok, err = writeScene3d(sTmpFile, tOptionsEditor.bAsyncMeshLoad, true)
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
    -- Origin lines default to visible=true (line:new) -- apply the active tab's own default
    -- (tShowOriginByTab) right away, since sActiveTab is 'map' here and Main Scene now starts hidden.
    local showOriginInit = tShowOriginByTab[sActiveTab]
    tOriginLine3dX.visible = showOriginInit
    tOriginLine3dY.visible = showOriginInit
    tOriginLine3dZ.visible = showOriginInit

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
    resetUndoHistory() -- seed the baseline undo entry now that the blank editor state is set up
end

function onLoop(delta)
    processThumbnailQueue()
    tickThumbnailBake()

    main_menu_3d()
    main_tab_bar()
    -- after main_tab_bar() so `cam3d` already points at the tab that was active *this* frame
    -- (switching tabs would otherwise apply the previous tab's camera for one stale frame)
    updateCam3dKeyboardMovement(delta)
    applyCam3d(cam3d)

    -- Unconditional, every frame, regardless of which tab is active -- drawMapTab also calls this
    -- once at the end of its own marker-list UI, but that only runs while the Map tab itself is
    -- being drawn, so its `.visible = (sActiveTab == 'map') and bShowSceneObjectMarkers` toggle
    -- never got a chance to turn markers back OFF once the user switched to a different tab (they
    -- stayed visible everywhere, not just the Main Scene/Map tab they're meant to be a reference
    -- for). Calling it here as well keeps visibility correct no matter which tab is current.
    updateSceneObjectShapes()

    -- Shown in all three tabs (Map, Mesh View, Map edition) -- `cam3d` already aliases whichever
    -- tab's own camera is current (setActiveTab), so this always drives the right one.
    -- setInitialWindowPositionRight's `x` is added on top of its own right-edge alignment (it
    -- already computes screenWidth - width internally) -- passing x = -width here (as before)
    -- double-shifted it an extra `width` in from the edge, which is why "almost its own width"
    -- was missing on the right. x = 0 sits it flush against the edge.
    -- AlwaysAutoResize (instead of the old fixed NoResize square) since the position/focus/distance
    -- fields below make the content height vary; NoResize would either clip them or leave dead space.
    tUtil.setInitialWindowPositionRight(tLang.L('mesh_preview_orbit'), 0, 0, 200, 220, 460)
    tImGui.Begin(tLang.L('mesh_preview_orbit'), true,
        tImGui.Flags({'ImGuiWindowFlags_NoTitleBar', 'ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoCollapse'}))
    tUtil.drawOrbitGizmo(cam3d, {size = 110})
    tImGui.Separator()

    -- Position/focus/distance readout+edit -- same math and widget pattern as mesh_debug.lua's
    -- camera panel (cam3dGetPos/applyCam3d are the same shared spherical-coordinate formulas,
    -- re-implemented locally per the comment above since mesh_debug.lua doesn't export them).
    local orbitPx, orbitPy, orbitPz = cam3dGetPos(cam3d)
    tUtil.pushResponsiveItemWidth(72)

    tImGui.Text(tLang.L('cam_position'))
    local rpx, npx = tImGui.InputFloat('X##orbitCamPx', orbitPx, 0, 0, '%.1f', 0)
    local rpy, npy = tImGui.InputFloat('Y##orbitCamPy', orbitPy, 0, 0, '%.1f', 0)
    local rpz, npz = tImGui.InputFloat('Z##orbitCamPz', orbitPz, 0, 0, '%.1f', 0)
    if rpx or rpy or rpz then
        npx = rpx and npx or orbitPx
        npy = rpy and npy or orbitPy
        npz = rpz and npz or orbitPz
        local dx, dy, dz = npx - cam3d.fx, npy - cam3d.fy, npz - cam3d.fz
        local dist = math.sqrt(dx * dx + dy * dy + dz * dz)
        if dist > 0 then
            cam3d.distance  = dist
            cam3d.elevation = math.asin(math.max(-1, math.min(1, dy / dist)))
            cam3d.azimuth   = math.atan(dx, dz)
        end
        applyCam3d(cam3d)
    end

    tImGui.Text(tLang.L('cam_focus'))
    local rfx, nfx = tImGui.DragFloat('X##orbitCamFx', cam3d.fx, 1.0, 0, 0, '%.1f', 0)
    local rfy, nfy = tImGui.DragFloat('Y##orbitCamFy', cam3d.fy, 1.0, 0, 0, '%.1f', 0)
    local rfz, nfz = tImGui.DragFloat('Z##orbitCamFz', cam3d.fz, 1.0, 0, 0, '%.1f', 0)
    if rfx then cam3d.fx = nfx end
    if rfy then cam3d.fy = nfy end
    if rfz then cam3d.fz = nfz end
    if rfx or rfy or rfz then applyCam3d(cam3d) end

    local rd, ndist = tImGui.InputFloat(tLang.L('cam_distance') .. '##orbitCamDist',
                                        cam3d.distance, 10, 100, '%.0f', 0)
    if rd and ndist and ndist > 0 then cam3d.distance = ndist; applyCam3d(cam3d) end

    tImGui.PopItemWidth()
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
    -- Every early-return above happens before either addPlacedMesh call, so reaching here always
    -- means a real placement just happened -- exactly one snapshot per successful click.
    pushUndoSnapshot()
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
    elseif keyControlPressed and key == mbm.getKeyCode('S') then
        onSaveScene3d()
    elseif keyControlPressed and key == mbm.getKeyCode('Z') then
        onUndoScene3d()
    elseif keyControlPressed and key == mbm.getKeyCode('Y') then
        onRedoScene3d()
    elseif keyControlPressed and key == mbm.getKeyCode('C') and sActiveTab == 'layer' then
        copySelectedMeshes()
    elseif keyControlPressed and key == mbm.getKeyCode('V') and sActiveTab == 'layer' then
        pasteCopiedMeshes()
    -- Select All / Invert / Unselect All mirror the Layer Options menu items of the same name
    -- (main_menu_3d) -- scoped to the active layer only, same reasoning as those menu actions.
    elseif keyControlPressed and key == mbm.getKeyCode('A') and sActiveTab == 'layer' then
        for _, tPlaced in ipairs(tPlacedMeshes) do tPlaced.isSelected = (tPlaced.layerIndex == iSelectedLayer) end
    elseif keyControlPressed and key == mbm.getKeyCode('I') and sActiveTab == 'layer' then
        for _, tPlaced in ipairs(tPlacedMeshes) do
            tPlaced.isSelected = (tPlaced.layerIndex == iSelectedLayer) and not tPlaced.isSelected
        end
    elseif keyControlPressed and key == mbm.getKeyCode('U') and sActiveTab == 'layer' then
        unselectAllPlacedMeshes()
    elseif key == mbm.getKeyCode('Delete') and sActiveTab == 'layer' then
        for i = #tPlacedMeshes, 1, -1 do
            if tPlacedMeshes[i].isSelected and tPlacedMeshes[i].layerIndex == iSelectedLayer then removePlacedMesh(i) end
        end
        pushUndoSnapshot()
    elseif key == mbm.getKeyCode('esc') and sActiveTab == 'layer' then
        unselectAllPlacedMeshes()
    elseif key == mbm.getKeyCode('W') or key == mbm.getKeyCode('up') then
        tCam3dMove.forward = 1
    elseif key == mbm.getKeyCode('S') or key == mbm.getKeyCode('down') then
        tCam3dMove.forward = -1
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('left') then
        tCam3dMove.right = -1
    elseif key == mbm.getKeyCode('D') or key == mbm.getKeyCode('right') then
        tCam3dMove.right = 1
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
    elseif key == mbm.getKeyCode('W') or key == mbm.getKeyCode('up')
        or key == mbm.getKeyCode('S') or key == mbm.getKeyCode('down') then
        tCam3dMove.forward = 0
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('left')
        or key == mbm.getKeyCode('D') or key == mbm.getKeyCode('right') then
        tCam3dMove.right = 0
    end
end
