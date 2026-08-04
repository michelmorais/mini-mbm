--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
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
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Mesh Debug Editor

   Batch mesh operations: load meshes from file or folder, apply operations (remove normals,
   add normals, centralize, etc.) per mesh or to all. Useful for batch processing sprite folders.

   More info at: https://mbm-documentation.readthedocs.io/en/latest/editors.html

]]--

tImGui        =     require "ImGui"
tUtil         =     require "editor_utils"
tBlender      =     require "blender_cli_wrapper"

-- pcall wrapper that prints the error on failure, then returns all values normally
local function dpCall(fn, ...)
    local res = table.pack(pcall(fn, ...))
    if not res[1] then
        print('[mesh_debug] ' .. tostring(res[2]))
    end
    return table.unpack(res, 1, res.n)
end

-- Per-axis rotation of a row vector, derived directly from this engine's own matrix code (not
-- guessed): MatrixRotationX/Y/Z (src/core_mbm/primitives.cpp:457-516) and MatrixMultiply's
-- out=pm1*pm2 convention (same file:254-270) together mean, for a row vector v transformed as
-- v'=v*M, that RotationX(a) maps (x,y,z) -> (x, y*cos(a)-z*sin(a), y*sin(a)+z*cos(a)), and
-- similarly for Y/Z below (matching each matrix's exact entries, not a textbook default).
local function rotateX(x, y, z, a)
    local c, s = math.cos(a), math.sin(a)
    return x, y * c - z * s, y * s + z * c
end
local function rotateY(x, y, z, a)
    local c, s = math.cos(a), math.sin(a)
    return x * c + z * s, y, -x * s + z * c
end
local function rotateZ(x, y, z, a)
    local c, s = math.cos(a), math.sin(a)
    return x * c - y * s, x * s + y * c, z
end

-- Decodes a bone's stored orientation (rotX/Y/Z, Euler XYZ degrees) into its local Y (bone axis,
-- head->tail direction) and Z (roll axis) basis vectors, both in the same space rotX/Y/Z are
-- stored in (world/armature space, same convention as x,y,z). Inverse of boneFrameToEuler below --
-- together these let the bake helpers compose a rotation into a bone's stored orientation instead
-- of only rotating its position, matching how editor/blender_mesh_skeleton_export.py reconstructs
-- a bone's tail/roll from the identical rotX/Y/Z + length fields (kept in lockstep by hand, no
-- shared implementation between Lua and Python).
function eulerToBoneFrame(rotXdeg, rotYdeg, rotZdeg)
    local radX = rotXdeg * math.pi / 180
    local radY = rotYdeg * math.pi / 180
    local radZ = rotZdeg * math.pi / 180
    local yx, yy, yz = 0, 1, 0
    local zx, zy, zz = 0, 0, 1
    if rotXdeg ~= 0 then
        yx, yy, yz = rotateX(yx, yy, yz, radX)
        zx, zy, zz = rotateX(zx, zy, zz, radX)
    end
    if rotYdeg ~= 0 then
        yx, yy, yz = rotateY(yx, yy, yz, radY)
        zx, zy, zz = rotateY(zx, zy, zz, radY)
    end
    if rotZdeg ~= 0 then
        yx, yy, yz = rotateZ(yx, yy, yz, radZ)
        zx, zy, zz = rotateZ(zx, zy, zz, radZ)
    end
    return yx, yy, yz, zx, zy, zz
end

-- Encodes a bone's local Y (bone axis) and Z (roll axis) basis vectors back into Euler XYZ
-- degrees, inverse of eulerToBoneFrame above. Closed-form extraction from M = Rx*Ry*Rz (this
-- engine's own row-vector rotation convention -- matrix rows are the images of the X/Y/Z basis
-- vectors, X derived here as cross(Y,Z) since only Y/Z are ever stored/needed).
function boneFrameToEuler(yx, yy, yz, zx, zy, zz)
    local xx = yy * zz - yz * zy
    local xy = yz * zx - yx * zz
    local xz = yx * zy - yy * zx
    local clamped = math.max(-1, math.min(1, -xz))
    local rotY = math.asin(clamped)
    local rotX, rotZ
    if math.abs(xz) > 0.999999 then
        -- gimbal lock: X and Z rotation become indistinguishable, collapse to rotX=0
        rotX = 0
        rotZ = math.atan(-yx, yy)
    else
        rotX = math.atan(yz, zz)
        rotZ = math.atan(xy, xx)
    end
    return rotX * 180 / math.pi, rotY * 180 / math.pi, rotZ * 180 / math.pi
end

local function cross3(ax, ay, az, bx, by, bz)
    return ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx
end
local function dot3(ax, ay, az, bx, by, bz)
    return ax * bx + ay * by + az * bz
end
local function normalize3(x, y, z)
    local len = math.sqrt(x * x + y * y + z * z)
    if len < 1e-9 then return 0, 0, 0 end
    return x / len, y / len, z / len
end

-- Canonical "roll = 0" reference axis for a given bone aim (local Y) direction: world Y
-- ("up"), unless aim itself is nearly parallel to world Y (a near-vertical bone, the single most
-- common case in this engine -- spines/limbs), in which case world X instead. Deliberately a
-- wide-margin threshold (0.9), not a three-way "smallest |component| wins" comparison: comparing
-- two near-zero magnitudes against EACH OTHER (rather than against a fixed, well-separated
-- threshold) flips its choice on ordinary floating-point noise -- confirmed directly: an aim
-- reconstructed from stored rotX/Y/Z came back as (~1e-7, 1, ~1e-7) instead of exactly (0,1,0),
-- and the old three-way comparison picked a DIFFERENT (perpendicular) reference axis for it than
-- for the exact input, silently rotating every decoded roll by ~90 degrees. This threshold form
-- only switches near true axis-alignment (|ay| within 1e-7 of 1.0 stays well above 0.9), so it's
-- immune to that class of noise.
local function canonicalRollAxis(ax, ay, az)
    local refX, refY, refZ = 0, 1, 0
    if math.abs(ay) > 0.9 then
        refX, refY, refZ = 1, 0, 0
    end
    local d = dot3(refX, refY, refZ, ax, ay, az)
    return normalize3(refX - d * ax, refY - d * ay, refZ - d * az)
end

-- Reads a bone's CURRENT roll (degrees) relative to its own canonicalRollAxis, purely from its
-- already-stored rotX/Y/Z -- stateless, recomputed fresh every frame exactly like every other
-- field in the bones table (X/Y/Z/Radius/Length), since SKELETON_BONE_V11 has no separate roll
-- field of its own to persist this in.
function currentRollDeg(rotX, rotY, rotZ)
    local yx, yy, yz, zx, zy, zz = eulerToBoneFrame(rotX, rotY, rotZ)
    local rx, ry, rz = canonicalRollAxis(yx, yy, yz)
    local cosT = dot3(rx, ry, rz, zx, zy, zz)
    local cx, cy, cz = cross3(rx, ry, rz, zx, zy, zz)
    local sinT = dot3(cx, cy, cz, yx, yy, yz)
    return math.atan(sinT, cosT) * 180 / math.pi
end

-- Inverse of currentRollDeg: given a bone aim direction (unit vector, local Y axis) and a target
-- roll angle in degrees, returns rotX/Y/Z. Rodrigues' rotation formula around the aim axis,
-- simplified since canonicalRollAxis is already guaranteed orthogonal to aim (k.v = 0 term drops).
function eulerFromAimAndRoll(ax, ay, az, rollDeg)
    local rx, ry, rz = canonicalRollAxis(ax, ay, az)
    local kx, ky, kz = cross3(ax, ay, az, rx, ry, rz)
    local c, s = math.cos(rollDeg * math.pi / 180), math.sin(rollDeg * math.pi / 180)
    return boneFrameToEuler(ax, ay, az, rx * c + kx * s, ry * c + ky * s, rz * c + kz * s)
end

-- Bakes a rotation into every bone's own x,y,z (degrees, applied X then Y then Z), matching
-- meshDebug:rotateFrame's exact per-axis formulas and order (src/core_mbm/mesh-manager.cpp:3128)
-- -- rotateX/Y/Z above are the same helpers verified against MatrixRotationX/Y/Z. Used to keep the
-- skeleton in sync whenever mesh_debug.lua bakes a rotation into vertex data via rotateFrame
-- (Bones-node Rotate, Transform-node Rotate, Apply-All Transform, and the Blender-import
-- post-process rotation), since bones are stored independently of vertex data. Also composes the
-- same bake rotation into each bone's own stored orientation (rotX/Y/Z), not just its position --
-- otherwise a rotated skeleton would keep pointing/rolled the old way. scaleX/Y/Z and length pass
-- through unchanged: length is a scalar (rotation-invariant), scale isn't touched by a rotation.
local function applyRotationToBonesDeg(meshD, angleXDeg, angleYDeg, angleZDeg)
    local radX = angleXDeg * math.pi / 180
    local radY = angleYDeg * math.pi / 180
    local radZ = angleZDeg * math.pi / 180
    local okTotal, nBones = dpCall(function() return meshD:getTotalBone() end)
    nBones = (okTotal and nBones) or 0
    for i = 1, nBones do
        local okG, name, x, y, z, radius, parentName, rotX, rotY, rotZ, scaleX, scaleY, scaleZ, length =
            dpCall(function() return meshD:getBone(i) end)
        if okG and name then
            local nx, ny, nz = x, y, z
            if angleXDeg ~= 0 then nx, ny, nz = rotateX(nx, ny, nz, radX) end
            if angleYDeg ~= 0 then nx, ny, nz = rotateY(nx, ny, nz, radY) end
            if angleZDeg ~= 0 then nx, ny, nz = rotateZ(nx, ny, nz, radZ) end
            local nRotX, nRotY, nRotZ = rotX, rotY, rotZ
            if angleXDeg ~= 0 or angleYDeg ~= 0 or angleZDeg ~= 0 then
                local yx, yy, yz, zx, zy, zz = eulerToBoneFrame(rotX, rotY, rotZ)
                if angleXDeg ~= 0 then
                    yx, yy, yz = rotateX(yx, yy, yz, radX)
                    zx, zy, zz = rotateX(zx, zy, zz, radX)
                end
                if angleYDeg ~= 0 then
                    yx, yy, yz = rotateY(yx, yy, yz, radY)
                    zx, zy, zz = rotateY(zx, zy, zz, radY)
                end
                if angleZDeg ~= 0 then
                    yx, yy, yz = rotateZ(yx, yy, yz, radZ)
                    zx, zy, zz = rotateZ(zx, zy, zz, radZ)
                end
                nRotX, nRotY, nRotZ = boneFrameToEuler(yx, yy, yz, zx, zy, zz)
            end
            dpCall(function()
                return meshD:updateBone(i, name, parentName, nx, ny, nz, radius,
                    nRotX, nRotY, nRotZ, scaleX, scaleY, scaleZ, length)
            end)
        end
    end
end

-- Mutual-exclusion tree node: only one top-level node per mesh can be open at a time.
-- Uses SetNextItemOpen to enforce state; IsItemClicked detects toggle intent.
local function openNode(tEntry, nodeKey, label, flags, id)
    local wantOpen = (tEntry.sOpenNode == nodeKey)
    tImGui.SetNextItemOpen(wantOpen, tImGui.Flags('ImGuiCond_Always'))
    local isOpen = tImGui.TreeNodeEx(label, flags or 0, id)
    if tImGui.IsItemClicked() then
        tEntry.sOpenNode = wantOpen and nil or nodeKey
    end
    return isOpen
end

-- Mesh entry: { fileName, meshDebug, info, loaded }
-- info from meshDebug:getInfo(fileName) - type, hasNormal, hasTexture, totalFrames, etc.

local TEXTURE_ANIMATION_EFFECT_VERSION_MESH = 10

local function getMeshFileVersion(info)
    return tonumber(info and info.version) or 0
end

local function isLegacyTextureAnimationEffectStorage(info)
    local version = getMeshFileVersion(info)
    return version > 0 and version < TEXTURE_ANIMATION_EFFECT_VERSION_MESH
end

local function getTextureAnimationEffectStorageLabel(info)
    local version = getMeshFileVersion(info)
    if version <= 0 then
        return nil
    end
    if version >= TEXTURE_ANIMATION_EFFECT_VERSION_MESH then
        return tLang.L('fx_texture_storage_native_v10')
    end
    return tLang.L('fx_texture_storage_legacy_stage2')
end

local function refreshEntryInfoFromFile(tEntry, fileName)
    local targetFile = fileName or (tEntry and tEntry.fileName) or ''
    if targetFile == '' then
        return nil
    end
    local ok, newInfo = dpCall(function() return meshDebug:getInfo(targetFile) end)
    if ok and newInfo then
        if tEntry and targetFile == tEntry.fileName then
            tEntry.info = newInfo
        end
        return newInfo
    end
    return nil
end

function onInitScene()
    camera2d              = mbm.getCamera("2d")
    camera3d              = mbm.getCamera("3d")
    camera3d:setFar(9999999)
    bCameraMode3D         = false
    tLoadedMeshes         = {}
    sLastMeshPath         = mbm.get('user_home') or mbm.get('HOME') or '~'
    sLastFolderPath       = sLastMeshPath
    bShowMeshTree         = true
    -- Live current width of the "Loaded Meshes" tree window, captured each frame by
    -- showMeshTreeWindow (via tImGui.GetWindowWidth, only valid while that window is the current
    -- ImGui context) -- read by showBonesWindow so the bottom Bones window's own X origin always
    -- starts right where the tree panel ends, even after the user resizes it, matching a live query
    -- instead of scene_editor3d.lua's separate fixed-constant convention (iMainPanelWidth), since
    -- this tree panel is user-resizable and a fixed constant would drift out of sync.
    iLoadedMeshesWindowWidth = 350
    tWindowsTitle         = {
        title_mesh_tree   = "title_mesh_tree",
        title_apply_all   = "title_apply_all"
    }
    tUtil.sMessageOverlay = 'Welcome to Mesh Debug Editor! Load meshes from File or Folder.'
    tUtil.bRightSide      = true   -- overlay on right so it is not covered by mesh tree on left
    tUtil.tTimerOverlay:start()   -- start timer so welcome message auto-hides after 13.5s
    iSelectedMeshIndex   = 0      -- single-expand: only one mesh node open at a time
    tPreviewMesh         = nil    -- mesh/sprite/tile shown on screen when selected
    tPreviewFont         = nil    -- font object when preview is a font (tPreviewMesh.tFont)
    iLastPreviewedIndex  = 0      -- track which mesh we last previewed
    tGhostMesh           = nil    -- separate translucent mesh instance shown while Bones node is open
    isClickedMouseleft   = false
    isClickedMouseRight  = false
    -- Continuous 3D camera movement, set by onKeyDown/onKeyUp and consumed once per frame using
    -- delta so movement follows the engine's real-time speed instead of OS key-repeat timing.
    tCam3dMove           = {forward = 0, right = 0, vertical = 0}
    -- Origin lines: 2D (X red, Y green) and 3D (X red, Y green, Z blue)
    originLine2dX = line:new('2dw', 0, 0, 50)
    originLine2dY = line:new('2dw', 0, 0, 50)
    originLine2dX:add({-9999999, 0, 9999999, 0})
    originLine2dY:add({0, -9999999, 0, 9999999})
    originLine2dX:setColor(255, 0, 0)
    originLine2dY:setColor(0, 255, 0)
    originLine2dX.visible = false
    originLine2dY.visible = false
    originLine3dX = line:new('3d', 0, 0, 0)
    originLine3dY = line:new('3d', 0, 0, 0)
    originLine3dZ = line:new('3d', 0, 0, 0)
    originLine3dX:add({-9999999, 0, 0,  9999999, 0, 0})
    originLine3dY:add({0, -9999999, 0,  0, 9999999, 0})
    originLine3dZ:add({0, 0, -9999999,  0, 0, 9999999})
    originLine3dX:setColor(255, 0, 0)
    originLine3dY:setColor(0, 255, 0)
    originLine3dZ:setColor(0, 0, 255)
    originLine3dX.visible = false
    originLine3dY.visible = false
    originLine3dZ.visible = false
    bShowOrigin2d = false
    bShowOrigin3d = false
    tListTexturesWin = {
        open          = false,
        folder        = '',
        usedSet       = {},
        usedList      = {},
        folderFiles   = {},
        selectedCount = 0,
        needRebuild   = true,
        showConfirm   = false,
    }
    tListMeshesWin = {
        open          = false,
        folder        = '',
        loadedSet     = {},
        loadedList    = {},
        folderFiles   = {},
        selectedCount = 0,
        needRebuild   = true,
    }
    tApplyAllWin = {
        open = false,
        selectedType = nil,
        transform = {
            frame = 0,
            subset = 0,
            rx = 0,
            ry = 0,
            rz = 0,
            sx = 1,
            sy = 1,
            sz = 1,
            targetWidth = 1,
            targetHeight = 1,
            targetDepth = 1,
            dx = 0,
            dy = 0,
            dz = 0,
        },
        texture = {
            frame = 0,
            subset = 0,
            stage = 0,
            filename = '',
        },
        uv = {
            frame = 0,
        },
        shader = {
            sourceFile = '',
        },
        physics = {
            primitiveType = 1,
            triCountRect  = 2,
            triCountCircle = 5,
        },
        -- Mirrors cloneMaterialTable(nil)'s defaults; cloneMaterialTable itself is defined
        -- later in the file (below onInitScene) so it can't be called from here.
        material = {
            Diffuse  = {r = 1, g = 1, b = 1, a = 1},
            Ambient  = {r = 1, g = 1, b = 1, a = 1},
            Specular = {r = 0, g = 0, b = 0, a = 1},
            Emissive = {r = 0, g = 0, b = 0, a = 1},
            Power    = 0,
        },
        lastResultText = '',
    }

    tBlenderImportState = {
        bOpen = false,
        bOpenPopup = false,
        tSourceFiles = {},
        iSelectedCount = 0,
        bIntermediateOnly = false,
        bPrintDebugSteps = false,
        sStatus = '',
        bStatusOk = true,
        bImporting = false,
        co = nil,
        iAnimSettingsIndex = 0,
        bOpenAnimSettingsPopup = false,
        iProgress = 0,
        iTotal = 0,
        sProgressDetail = '',
        iTimeoutSecs = 120,
        bAbortRequested = false,
        sCancelFile = '',
        customBlenderPath = '',
        bKeepInSourceFolder = false,
        bImportPostProcess = true,
        bImportInvertU = false,
        bImportInvertV = true,
        nImportAngleX = -90,
        nImportAngleY = 0,
        nImportAngleZ = 0,
        iLargeMeshMode = 1,
        bImportIncludeBones = true,
        tRunResults = {},
    }
    tMixamoGuideState = {
        bOpen = false,
        bOpenPopup = false,
    }
    tMeshExportBuildState = {
        bOpen = false,
        bOpenPopup = false,
        bBuilding = false,
        bAbortRequested = false,
        sStatus = '',
        bStatusOk = true,
        co = nil,
        sCancelFile = '',
        iTimeoutSecs = 120,
        bDebugSteps = false,
        tRunResults = {}, -- {name=, ok=, msg=} per entry, populated for both single and "all" runs
    }
    -- Shown before meshExportBuildCoroutine actually runs (triggered by "Export Current Mesh"/
    -- "Export All Meshes"), mirroring the Blender-import dialog's own "Post-processing" block.
    -- Rotation defaults (90,0,0) are the exact negation of the import dialog's own default
    -- (nImportAngleX = -90 above) -- rotation isn't self-cancelling, so undoing it needs the
    -- opposite angle. Invert U/V defaults (false, true) are instead the *same* flags as import's
    -- own defaults (bImportInvertU/V above) -- inverting is self-cancelling, so applying the same
    -- flip again on export is what undoes import's own flip.
    tMeshExportOptionsState = {
        bOpen = false,
        bOpenPopup = false,
        nAngleX = 90,
        nAngleY = 0,
        nAngleZ = 0,
        bInvertU = false,
        bInvertV = true,
        tEntries = nil, -- resolved by the menu handler before opening this dialog
    }
    tEditorLightUi = {}
end

local function makeColorRGBA(color, defaultColor)
    local base = defaultColor or {r = 1, g = 1, b = 1, a = 1}
    return {
        r = (color and color.r) or base.r or 1,
        g = (color and color.g) or base.g or 1,
        b = (color and color.b) or base.b or 1,
        a = (color and color.a) or base.a or 1,
    }
end

local function cloneMaterialTable(mat)
    return {
        Diffuse = makeColorRGBA(mat and mat.Diffuse, {r = 1, g = 1, b = 1, a = 1}),
        Ambient = makeColorRGBA(mat and mat.Ambient, {r = 1, g = 1, b = 1, a = 1}),
        Specular = makeColorRGBA(mat and mat.Specular, {r = 0, g = 0, b = 0, a = 1}),
        Emissive = makeColorRGBA(mat and mat.Emissive, {r = 0, g = 0, b = 0, a = 1}),
        Power = (mat and mat.Power) or 0,
    }
end

local function cloneLightState(state)
    local directionalDirection = {
        x = (state and state.directionalDirection and state.directionalDirection.x) or 0,
        y = (state and state.directionalDirection and state.directionalDirection.y) or -0.70710677,
        z = (state and state.directionalDirection and state.directionalDirection.z) or -0.70710677,
    }
    return {
        enabled = state and state.enabled or false,
        ambientColor = makeColorRGBA(state and state.ambientColor, {r = 0.2, g = 0.2, b = 0.2, a = 1}),
        directionalColor = makeColorRGBA(state and state.directionalColor, {r = 1, g = 1, b = 1, a = 1}),
        directionalDirection = directionalDirection,
        -- Orbit-trackball state for the directional-direction gizmo (same widget the 3D camera and
        -- scene_editor3d.lua's own light panel use) -- seeded once from whatever direction is
        -- currently set so it starts in sync instead of snapping somewhere arbitrary.
        orbit = tUtil.orbitFromDir(directionalDirection),
        pointColor = makeColorRGBA(state and state.pointColor, {r = 1, g = 1, b = 1, a = 1}),
        pointPosition = {
            x = (state and state.pointPosition and state.pointPosition.x) or 0,
            y = (state and state.pointPosition and state.pointPosition.y) or 0,
            z = (state and state.pointPosition and state.pointPosition.z) or 128,
        },
        pointRadius = (state and state.pointRadius) or 512,
    }
end

local function isSameFloat(a, b)
    return math.abs((a or 0) - (b or 0)) <= 0.0001
end

local function isSameColor(a, b)
    return isSameFloat(a and a.r, b and b.r) and
           isSameFloat(a and a.g, b and b.g) and
           isSameFloat(a and a.b, b and b.b) and
           isSameFloat(a and a.a, b and b.a)
end

local function isSameMaterial(a, b)
    return isSameColor(a and a.Diffuse, b and b.Diffuse) and
           isSameColor(a and a.Ambient, b and b.Ambient) and
           isSameColor(a and a.Specular, b and b.Specular) and
           isSameColor(a and a.Emissive, b and b.Emissive) and
           isSameFloat(a and a.Power, b and b.Power)
end

local function getEditorLightState(target)
    local ok, state = dpCall(function() return mbm.getLightState(target) end)
    if ok and type(state) == 'table' then
        state.ambientColor = makeColorRGBA(state.ambientColor, {r = 0.2, g = 0.2, b = 0.2, a = 1})
        state.directionalColor = makeColorRGBA(state.directionalColor, {r = 1, g = 1, b = 1, a = 1})
        state.directionalDirection = state.directionalDirection or {x = 0, y = -0.70710677, z = -0.70710677}
        state.pointColor = makeColorRGBA(state.pointColor, {r = 1, g = 1, b = 1, a = 1})
        state.pointPosition = state.pointPosition or {x = 0, y = 0, z = 128}
        state.pointRadius = state.pointRadius or 512
        return state
    end
    return {
        enabled = false,
        ambientColor = {r = 0.2, g = 0.2, b = 0.2, a = 1},
        directionalColor = {r = 1, g = 1, b = 1, a = 1},
        directionalDirection = {x = 0, y = -0.70710677, z = -0.70710677},
        pointColor = {r = 1, g = 1, b = 1, a = 1},
        pointPosition = {x = 0, y = 0, z = 128},
        pointRadius = 512,
    }
end

local function getEditorLightUi(target, forceRefresh)
    tEditorLightUi = tEditorLightUi or {}
    if forceRefresh or tEditorLightUi[target] == nil then
        tEditorLightUi[target] = cloneLightState(getEditorLightState(target))
    end
    return tEditorLightUi[target]
end

local function getPreviewStage2Texture(tEntry)
    if tEntry and tEntry.meshDebug then
        local meshD = tEntry.meshDebug
        local okF, totalFrames = dpCall(function() return meshD:getTotalFrame() end)
        if okF and totalFrames then
            for frame = 1, totalFrames do
                local okS, totalSubsets = dpCall(function() return meshD:getTotalSubset(frame) end)
                if okS and totalSubsets then
                    for subset = 1, totalSubsets do
                        local okT, tex2 = dpCall(function() return meshD:getMaterialTexture(frame, subset, 'normal') end)
                        if okT and type(tex2) == 'string' and tex2 ~= '' then
                            return tex2
                        end
                    end
                end
            end
        end
    end
    if not tPreviewMesh then
        return nil
    end
    -- plain pcall (no dpCall/print): a mesh with no active shader animation has no FX,
    -- which is an expected, frequent case here, not a bug worth logging every frame.
    local okSh, tShader = pcall(function() return tPreviewMesh:getShader() end)
    if not okSh or not tShader then
        return nil
    end
    local okTex, tex2 = pcall(function() return tShader:getTextureStage2() end)
    if not okTex or type(tex2) ~= 'string' or tex2 == '' then
        return nil
    end
    return tex2
end

local function getEditorLightDebugInfo(target, lightState)
    if iSelectedMeshIndex <= 0 or iSelectedMeshIndex > #tLoadedMeshes then
        return nil
    end
    local tEntry = tLoadedMeshes[iSelectedMeshIndex]
    if not tEntry then
        return nil
    end
    local info = tEntry.info or {}
    local hasUv = info.hasTexture == true
    local hasNormals = info.hasNormal == true
    local texStage2 = getPreviewStage2Texture(tEntry)
    local effectiveMode = tLang.L('light_debug_disabled')

    if lightState.enabled then
        if target == '2dw' then
            if hasUv == false then
                effectiveMode = tLang.L('light_debug_off_no_uv')
            elseif texStage2 then
                effectiveMode = tLang.L('light_debug_2dw_normal_map')
            else
                effectiveMode = tLang.L('light_debug_2dw_flat')
            end
        elseif hasNormals then
            effectiveMode = tLang.L('light_debug_3d_directional')
        else
            effectiveMode = tLang.L('light_debug_off_no_normals')
        end
    end

    return {
        assetType = info.type or 'unknown',
        hasUv = hasUv,
        hasNormals = hasNormals,
        texStage2 = texStage2,
        effectiveMode = effectiveMode,
    }
end

local function showEditorLightDebug(target, lightState)
    local dbg = getEditorLightDebugInfo(target, lightState)
    tImGui.Separator()
    tImGui.TextDisabled(tLang.L('light_debug'))
    if not dbg then
        tImGui.TextDisabled(tLang.L('light_debug_no_selection'))
        return
    end
    tImGui.TextDisabled(tLang.L('type_label') .. ' ' .. tostring(dbg.assetType))
    tImGui.TextDisabled(tLang.L('target_label') .. ' ' .. tostring(target))
    tImGui.TextDisabled((dbg.hasUv and tLang.L('light_debug_has_uv')) or tLang.L('light_debug_no_uv'))
    tImGui.TextDisabled((dbg.hasNormals and tLang.L('light_debug_has_mesh_normals')) or tLang.L('light_debug_no_mesh_normals'))
    tImGui.TextDisabled(tLang.L('texture_stage_2') .. ': ' ..
        (dbg.texStage2 and tUtil.getShortName(dbg.texStage2) or tLang.L('none')))
    tImGui.TextWrapped(tLang.L('light_debug_effective') .. ' ' .. dbg.effectiveMode)
    if target == '2dw' then
        tImGui.TextWrapped(tLang.L('light_debug_2dw_note'))
    end
end

local function ensureEditorLightingEnabled(target)
    local state = getEditorLightState(target)
    if state.enabled then
        return
    end
    dpCall(function() mbm.setLightEnabled(target, true) end)
    local uiState = getEditorLightUi(target, true)
    uiState.enabled = true
end

local function setMeshDebugCameraMode3d(enabled)
    local newMode = enabled and true or false
    local changed = bCameraMode3D ~= newMode
    bCameraMode3D = newMode
    if changed then
        iLastPreviewedIndex = 0
    end
    originLine2dX.visible = (not bCameraMode3D) and bShowOrigin2d
    originLine2dY.visible = (not bCameraMode3D) and bShowOrigin2d
    originLine3dX.visible = bCameraMode3D and bShowOrigin3d
    originLine3dY.visible = bCameraMode3D and bShowOrigin3d
    originLine3dZ.visible = bCameraMode3D and bShowOrigin3d
    if changed and bCameraMode3D then
        ensureEditorLightingEnabled('3d')
    end
end

function setMeshDebugOriginLinesVisible(visible)
    visible = visible and true or false
    if bCameraMode3D then
        bShowOrigin3d = visible
        originLine3dX.visible = visible
        originLine3dY.visible = visible
        originLine3dZ.visible = visible
    else
        bShowOrigin2d = visible
        originLine2dX.visible = visible
        originLine2dY.visible = visible
    end
end

local function showEditorLightPanel(target, idSuffix)
    local lightState = getEditorLightUi(target)
    local lightColorFlags = tImGui.Flags('ImGuiColorEditFlags_NoInputs')
    tImGui.Text(tLang.L('light_panel'))
    tImGui.SameLine()
    local enabled = tImGui.Checkbox('##lightEnabled' .. idSuffix, lightState.enabled)
    if enabled ~= lightState.enabled then
        dpCall(function() mbm.setLightEnabled(target, enabled) end)
        lightState.enabled = enabled
    end
    tImGui.SameLine()
    tImGui.TextDisabled(tLang.L('light_enabled'))

    tImGui.Text(tLang.L('ambient'))
    tImGui.SameLine()
    local changedAmbient, ambientColor = tImGui.ColorEdit4('##lightAmbient' .. idSuffix, lightState.ambientColor, lightColorFlags)
    if changedAmbient and ambientColor then
        lightState.ambientColor = makeColorRGBA(ambientColor, lightState.ambientColor)
        dpCall(function() mbm.setAmbientLight(target, ambientColor) end)
    end

    if target == '2dw' then
        tImGui.Text(tLang.L('light_color'))
        tImGui.SameLine()
        local changedPointColor, pointColor = tImGui.ColorEdit4('##lightPointColor' .. idSuffix, lightState.pointColor, lightColorFlags)
        if changedPointColor and pointColor then
            lightState.pointColor = makeColorRGBA(pointColor, lightState.pointColor)
            dpCall(function() mbm.setPointLightColor(target, pointColor) end)
        end

        local point = lightState.pointPosition or {x = 0, y = 0, z = 128}
        tImGui.Text(tLang.L('position'))
        tUtil.pushResponsiveItemWidth(120)
        local p1, px = tImGui.InputFloat('X##lightPosX' .. idSuffix, point.x or 0, 1, 10, '%.2f', 0)
        local p2, py = tImGui.InputFloat('Y##lightPosY' .. idSuffix, point.y or 0, 1, 10, '%.2f', 0)
        local p3, pz = tImGui.InputFloat('Z##lightPosZ' .. idSuffix, point.z or 128, 1, 10, '%.2f', 0)
        tImGui.PopItemWidth()
        if p1 or p2 or p3 then
            lightState.pointPosition = {
                x = p1 and px or point.x or 0,
                y = p2 and py or point.y or 0,
                z = p3 and pz or point.z or 128,
            }
            dpCall(function()
                mbm.setPointLightPosition(target,
                    lightState.pointPosition.x,
                    lightState.pointPosition.y,
                    lightState.pointPosition.z)
            end)
        end

        tImGui.Text(tLang.L('radius'))
        tUtil.pushResponsiveItemWidth(120)
        local changedRadius, pointRadius = tImGui.InputFloat('##lightRadius' .. idSuffix, lightState.pointRadius or 512, 1, 10, '%.2f', 0)
        tImGui.PopItemWidth()
        if changedRadius and pointRadius then
            lightState.pointRadius = pointRadius
            dpCall(function() mbm.setPointLightRadius(target, pointRadius) end)
        end
    else
        tImGui.Text(tLang.L('directional_color'))
        tImGui.SameLine()
        local changedDirectionalColor, directionalColor = tImGui.ColorEdit4('##lightDirectionalColor' .. idSuffix, lightState.directionalColor, lightColorFlags)
        if changedDirectionalColor and directionalColor then
            lightState.directionalColor = makeColorRGBA(directionalColor, lightState.directionalColor)
            dpCall(function() mbm.setDirectionalLightColor(target, directionalColor) end)
        end

        -- Same orbit trackball widget already used for the camera here and for the light in
        -- scene_editor3d.lua (tUtil.drawOrbitGizmo only reads/writes .azimuth/.elevation, so it
        -- works unmodified on lightState.orbit) -- drag to re-aim the sun instead of typing a raw
        -- direction vector by hand.
        tImGui.Text(tLang.L('direction_label'))
        lightState.orbit = lightState.orbit or tUtil.orbitFromDir(lightState.directionalDirection)
        if tUtil.drawOrbitGizmo(lightState.orbit, {size = 110}) then
            lightState.directionalDirection = tUtil.dirFromOrbit(lightState.orbit)
            dpCall(function()
                mbm.setDirectionalLightDirection(target,
                    lightState.directionalDirection.x,
                    lightState.directionalDirection.y,
                    lightState.directionalDirection.z)
            end)
        end
        tImGui.TextDisabled(string.format('x=%.3f', lightState.directionalDirection.x))
        tImGui.TextDisabled(string.format('y=%.3f', lightState.directionalDirection.y))
        tImGui.TextDisabled(string.format('z=%.3f', lightState.directionalDirection.z))
    end

    if tImGui.Button(tLang.L('reset_light') .. '##lightReset' .. idSuffix) then
        dpCall(function() mbm.resetLight(target) end)
        getEditorLightUi(target, true)
    end

    showEditorLightDebug(target, lightState)
end

local function showMaterialEditor(tEntry, index)
    local meshD = tEntry.meshDebug
    local flags = 0

    local function onEdit()
        tEntry.modified = true
        if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
    end

    local ok, mat = dpCall(function() return meshD:getMaterial() end)
    if not ok then
        return
    end

    tEntry.tMaterialUI = tEntry.tMaterialUI or {
        original = cloneMaterialTable(mat),
        current = cloneMaterialTable(mat),
    }
    local materialUi = tEntry.tMaterialUI
    if isSameMaterial(materialUi.original, mat) == false and isSameMaterial(materialUi.current, materialUi.original) then
        materialUi.original = cloneMaterialTable(mat)
        materialUi.current = cloneMaterialTable(mat)
    end

    local function editMaterialColor(key, label)
        local clicked, color = tImGui.ColorEdit4(label .. '##mat-' .. key .. '-' .. index, materialUi.current[key], flags)
        if clicked and color then
            materialUi.current[key] = makeColorRGBA(color, materialUi.current[key])
        end
    end

    editMaterialColor('Diffuse', tLang.L("diffuse"))
    editMaterialColor('Ambient', tLang.L("ambient"))
    editMaterialColor('Specular', tLang.L("specular"))
    editMaterialColor('Emissive', tLang.L("emissive"))

    local rp, np = tImGui.InputFloat(tLang.L("power") .. '##mat-power-' .. index, materialUi.current.Power, 0.1, 1, '%.2f', flags)
    if rp then
        materialUi.current.Power = np
    end

    if isSameMaterial(materialUi.current, materialUi.original) == false then
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=1,b=0,a=1})
        tImGui.Text(tLang.L("unsaved_changes"))
        tImGui.PopStyleColor(1)
        if tImGui.Button(tLang.L("apply_btn") .. '##mat-apply-' .. index) then
            local applyMat = cloneMaterialTable(materialUi.current)
            local okSet = dpCall(function() meshD:setMaterial(applyMat) end)
            if okSet then
                onEdit()
                materialUi.original = cloneMaterialTable(applyMat)
                materialUi.current = cloneMaterialTable(applyMat)
            end
        end
    end
end

local function getTempDir()
    if package.config:sub(1, 1) == '\\' then
        return os.getenv('TEMP') or os.getenv('TMP') or os.getenv('TMPDIR') or 'C:\\Temp'
    end
    return os.getenv('TMPDIR') or os.getenv('TEMP') or os.getenv('TMP') or '/tmp'
end

local function getEditorDir()
    local src = debug.getinfo(1, 'S').source or ''
    if src:sub(1, 1) == '@' then
        local path = src:sub(2)
        local dir = path:match('^(.*)[/\\]')
        return dir or '.'
    end
    return '.'
end

local function getFileDir(path)
    return (path and path:match('^(.*)[/\\]')) or '.'
end

local function getFileStem(path)
    local name = (path and path:match('[/\\]([^/\\]+)$')) or path or 'mesh'
    return (name and name:match('(.+)%.[^%.]+$')) or name or 'mesh'
end

local function shellQuoteLocal(path)
    if package.config:sub(1, 1) == '\\' then
        return '"' .. tostring(path):gsub('"', '\\"') .. '"'
    end
    return "'" .. tostring(path):gsub("'", "'\\''") .. "'"
end

local function removePathRecursive(path)
    if not path or path == '' then return end
    if package.config:sub(1, 1) == '\\' then
        os.execute('rmdir /s /q ' .. shellQuoteLocal(path) .. ' 2>nul')
    else
        os.execute('rm -rf ' .. shellQuoteLocal(path))
    end
end

local function joinPath(base, rel)
    if not base or base == '' then return rel end
    if not rel or rel == '' then return base end
    local sep = package.config:sub(1, 1)
    if base:sub(-1) == '/' or base:sub(-1) == '\\' then
        return base .. rel
    end
    return base .. sep .. rel
end

local function addPathIfValid(path, tSeen)
    if type(path) ~= 'string' or path == '' then return 0 end
    path = path:gsub("\\", "/")
    if tSeen and tSeen[path] then return 0 end
    mbm.addPath(path)
    if tSeen then tSeen[path] = true end
    return 1
end

local function addBlenderSourceFallbackSearchPath(sourcePath)
    local added = 0
    local tSeen = {}
    added = added + addPathIfValid(getFileDir(sourcePath), tSeen)
    return added
end

local function parseBlenderVersion(version)
    local major, minor, patch = tostring(version or ''):match('^(%d+)%.(%d+)%.?(%d*)')
    return tonumber(major) or 0, tonumber(minor) or 0, tonumber(patch) or 0
end

local function isBlenderVersionOlderThan(blender, majorMin, minorMin)
    if not blender or not blender.found then return false end
    local major, minor = parseBlenderVersion(blender.version)
    if major == 0 then return false end
    if major < majorMin then return true end
    if major > majorMin then return false end
    return minor < minorMin
end

local function getDefaultSampleStep(frameStart, frameEnd)
    local total = math.max(1, math.abs((frameEnd or 1) - (frameStart or 1)) + 1)
    if total > 800 then return 10 end
    if total > 300 then return 5 end
    return 1
end

local function ensureBlenderAnimSettings(row)
    row.anim = row.anim or {
        scanStatus = 'not_scanned',
        scanData = nil,
        scanError = '',
        scanOutput = '',
        scanLog = '',
        scanStartTime = 0,
        bEnableAnimation = false,
        bManualRange = false,
        iSelectedSource = 0,
        iStaticFrame = 1,
        iFrameStart = 1,
        iFrameEnd = 1,
        iSampleStep = 1,
        sAnimationName = 'Bake',
        tClips = {},
    }
    row.anim.tClips = type(row.anim.tClips) == 'table' and row.anim.tClips or {}
    return row.anim
end

local function getSourceFrameCount(src)
    if not src then return 1 end
    return math.max(1, math.abs((src.frameEnd or 1) - (src.frameStart or 1)) + 1)
end

local function getBakedFrameCount(frameStart, frameEnd, sampleStep)
    local total = math.max(1, math.abs((frameEnd or 1) - (frameStart or 1)) + 1)
    return math.floor((total - 1) / math.max(1, sampleStep or 1)) + 1
end

local function formatLargeInt(value)
    value = math.floor(tonumber(value or 0) or 0)
    local s = tostring(value)
    local out = ''
    while #s > 3 do
        out = ',' .. s:sub(-3) .. out
        s = s:sub(1, -4)
    end
    return s .. out
end

local function formatBytes(bytes)
    bytes = tonumber(bytes or 0) or 0
    local units = {'B', 'KB', 'MB', 'GB'}
    local idx = 1
    while bytes >= 1024 and idx < #units do
        bytes = bytes / 1024
        idx = idx + 1
    end
    if idx == 1 then
        return string.format('%d %s', math.floor(bytes), units[idx])
    end
    return string.format('%.1f %s', bytes, units[idx])
end

local function getTextureSearchPathCountFromScan(row)
    local anim = row and row.anim
    local stats = anim and anim.scanData and anim.scanData.meshStats
    local paths = type(stats) == 'table' and stats.textureSearchPaths or nil
    return type(paths) == 'table' and #paths or nil
end

local function estimateRawMeshBytes(frames, verticesPerFrame, indicesPerFrame, subsetsPerFrame, texturePathBytes)
    frames = math.max(1, math.floor(tonumber(frames or 1) or 1))
    verticesPerFrame = math.max(0, math.floor(tonumber(verticesPerFrame or 0) or 0))
    indicesPerFrame = math.max(0, math.floor(tonumber(indicesPerFrame or 0) or 0))
    subsetsPerFrame = math.max(0, math.floor(tonumber(subsetsPerFrame or 0) or 0))
    texturePathBytes = math.max(0, math.floor(tonumber(texturePathBytes or 0) or 0))
    local headerBytes = 56 + texturePathBytes + 12 + 40 + 140
    local frameBytes = 20 + (subsetsPerFrame * 84) + (indicesPerFrame * 2) + (verticesPerFrame * 32)
    return headerBytes + (frameBytes * frames)
end

local getEnabledBlenderSourceRows
local getBlenderImportOptionsForRow

local function getRowImportEstimate(row)
    local options = getBlenderImportOptionsForRow(row)
    local targetFrames = 1
    if options.bakeAnimation and type(options.animationClips) == 'table' and #options.animationClips > 0 then
        targetFrames = 0
        for i = 1, #options.animationClips do
            local clip = options.animationClips[i]
            targetFrames = targetFrames + getBakedFrameCount(clip.frameStart, clip.frameEnd, clip.sampleStep)
        end
    elseif options.bakeAnimation then
        targetFrames = getBakedFrameCount(options.frameStart, options.frameEnd, options.sampleStep)
    end
    local anim = row and row.anim
    local stats = anim and anim.scanData and anim.scanData.meshStats or nil
    local out = {
        targetFrames = targetFrames,
        hasStats = type(stats) == 'table' and stats.available == true,
        verticesPerFrame = nil,
        indicesPerFrame = nil,
        subsetsPerFrame = nil,
        totalVertices = nil,
        totalIndices = nil,
        estimatedRawBytes = nil,
        texturePaths = getTextureSearchPathCountFromScan(row),
        animationText = '',
    }
    if options.bakeAnimation and type(options.animationClips) == 'table' and #options.animationClips > 1 then
        out.animationText = string.format('%d clips', #options.animationClips)
    elseif options.bakeAnimation then
        out.animationText = string.format('%s %d..%d step %d', options.animationName or 'Bake', options.frameStart or 1, options.frameEnd or 1, options.sampleStep or 1)
    else
        out.animationText = string.format('static frame %d', options.frameStart or 1)
    end
    if out.hasStats then
        out.verticesPerFrame = math.max(0, math.floor(tonumber(stats.vertices or 0) or 0))
        out.indicesPerFrame = math.max(0, math.floor(tonumber(stats.indices or 0) or 0))
        out.subsetsPerFrame = math.max(0, math.floor(tonumber(stats.subsets or 0) or 0))
        out.totalVertices = out.verticesPerFrame * targetFrames
        out.totalIndices = out.indicesPerFrame * targetFrames
        local pathBytes = 0
        if type(stats.textureSearchPaths) == 'table' then
            for i = 1, #stats.textureSearchPaths do
                pathBytes = pathBytes + 5 + tostring(stats.textureSearchPaths[i]):len()
            end
        end
        out.estimatedRawBytes = estimateRawMeshBytes(targetFrames, out.verticesPerFrame, out.indicesPerFrame, out.subsetsPerFrame, pathBytes)
    end
    return out
end

local function getBlenderImportEstimateSummary()
    local rows = getEnabledBlenderSourceRows()
    local fileCount = #rows
    local targetFrames = 0
    local vertices = 0
    local indices = 0
    local rawBytes = 0
    local statsMissing = 0
    local maxFrames = 0
    local vertexLimitExceeded = false
    for i = 1, #rows do
        local est = getRowImportEstimate(rows[i])
        targetFrames = targetFrames + est.targetFrames
        maxFrames = math.max(maxFrames, est.targetFrames)
        if est.hasStats then
            if (est.verticesPerFrame or 0) > 65535 then
                vertexLimitExceeded = true
            end
            vertices = vertices + (est.totalVertices or 0)
            indices = indices + (est.totalIndices or 0)
            rawBytes = rawBytes + (est.estimatedRawBytes or 0)
        else
            statsMissing = statsMissing + 1
        end
    end

    local warning = nil
    if vertexLimitExceeded and (tBlenderImportState.iLargeMeshMode or 1) == 2 then
        warning = tLang.L('blender_import_estimate_warning_vb_only')
    elseif vertexLimitExceeded then
        warning = tLang.L('blender_import_estimate_warning_vertex_limit')
    elseif rawBytes >= 1024 * 1024 * 1024 or maxFrames > 800 then
        warning = tLang.L('blender_import_estimate_warning_large')
    elseif rawBytes >= 256 * 1024 * 1024 or maxFrames > 300 then
        warning = tLang.L('blender_import_estimate_warning_medium')
    end

    return {
        fileCount = fileCount,
        targetFrames = targetFrames,
        vertices = vertices,
        indices = indices,
        rawBytes = rawBytes,
        statsMissing = statsMissing,
        vertexLimitExceeded = vertexLimitExceeded,
        warning = warning,
    }
end

local function getBlenderLargeMeshModeArg()
    if (tBlenderImportState.iLargeMeshMode or 1) == 2 then
        return 'vb_only'
    end
    return 'fail'
end

local function makeBlenderClipFromSource(src, index, previous)
    if not src then return end
    local frameStart = math.max(1, math.floor(tonumber(src.frameStart or 1) or 1))
    local frameEnd = math.max(1, math.floor(tonumber(src.frameEnd or frameStart) or frameStart))
    local clip = previous or {}
    clip.enabled = clip.enabled == true
    clip.sourceIndex = index or clip.sourceIndex or 0
    clip.sourceName = tostring(src.name or ('Source ' .. tostring(index or 0)))
    clip.kind = tostring(src.kind or '')
    clip.reason = tostring(src.reason or src.confidence or '')
    clip.name = tostring(clip.name or src.name or ('Bake ' .. tostring(index or 0)))
    clip.frameStart = math.max(1, math.floor(tonumber(clip.frameStart or frameStart) or frameStart))
    clip.frameEnd = math.max(1, math.floor(tonumber(clip.frameEnd or frameEnd) or frameEnd))
    clip.sampleStep = math.max(1, math.floor(tonumber(clip.sampleStep or getDefaultSampleStep(frameStart, frameEnd)) or 1))
    return clip
end

local function syncBlenderClipsWithSources(anim)
    local sources = anim and anim.scanData and anim.scanData.sources or {}
    if type(sources) ~= 'table' then return end
    anim.tClips = type(anim.tClips) == 'table' and anim.tClips or {}
    local old = anim.tClips
    local clips = {}
    for i = 1, #sources do
        clips[i] = makeBlenderClipFromSource(sources[i], i, old[i])
    end
    anim.tClips = clips
end

local function getSelectedBlenderClips(anim)
    local out = {}
    if not anim or anim.bEnableAnimation ~= true then return out end
    if anim.bManualRange then
        out[1] = {
            name = anim.sAnimationName or 'Bake',
            frameStart = math.max(1, anim.iFrameStart or 1),
            frameEnd = math.max(1, anim.iFrameEnd or anim.iFrameStart or 1),
            sampleStep = math.max(1, anim.iSampleStep or 1),
        }
        return out
    end
    local clips = type(anim.tClips) == 'table' and anim.tClips or {}
    for i = 1, #clips do
        local clip = clips[i]
        if type(clip) == 'table' and clip.enabled == true then
            out[#out + 1] = {
                name = clip.name or ('Bake ' .. i),
                frameStart = math.max(1, clip.frameStart or 1),
                frameEnd = math.max(1, clip.frameEnd or clip.frameStart or 1),
                sampleStep = math.max(1, clip.sampleStep or 1),
                sourceIndex = clip.sourceIndex or i,
            }
        end
    end
    return out
end

local function applyBlenderSourceToSettings(anim, src, index)
    if not src then return end
    syncBlenderClipsWithSources(anim)
    local clips = anim.tClips or {}
    for i = 1, #clips do
        clips[i].enabled = false
    end
    local clip = clips[index]
    if clip then
        clip.enabled = true
        anim.bManualRange = false
        anim.bEnableAnimation = true
        anim.iSelectedSource = index or 0
        anim.iFrameStart = clip.frameStart
        anim.iFrameEnd = clip.frameEnd
        anim.iSampleStep = clip.sampleStep
        anim.sAnimationName = clip.name or tostring(src.name or 'Bake')
    end
end

local function getBestBlenderScanSource(scanData)
    local sources = scanData and scanData.sources
    if type(sources) ~= 'table' then return nil, 0 end
    for i = 1, #sources do
        local src = sources[i]
        if src.confidence == 'high' and getSourceFrameCount(src) > 1 then
            return src, i
        end
    end
    return nil, 0
end

local function canBakeBlenderAnimation(anim)
    if not anim or type(anim.scanData) ~= 'table' then
        return true
    end
    local issues = anim.scanData.meshCacheIssues or {}
    if #issues == 0 then
        return true
    end
    local clips = getSelectedBlenderClips(anim)
    if #clips > 0 then
        local sources = anim.scanData.sources or {}
        for i = 1, #clips do
            local selected = sources[clips[i].sourceIndex or 0]
            if not (selected and selected.hasGeometryAnimation and selected.confidence ~= 'low') then
                return false
            end
        end
        return true
    end
    local sources = anim.scanData.sources or {}
    local selected = sources[anim.iSelectedSource or 0]
    return selected and selected.hasGeometryAnimation and selected.confidence ~= 'low'
end

local function applyBlenderScanDefaults(row)
    local anim = ensureBlenderAnimSettings(row)
    local scene = anim.scanData and anim.scanData.scene or nil
    if type(scene) == 'table' then
        anim.iStaticFrame = math.max(1, math.floor(scene.currentFrame or scene.frameStart or 1))
        anim.iFrameStart = math.max(1, math.floor(scene.frameStart or 1))
        anim.iFrameEnd = math.max(1, math.floor(scene.frameEnd or anim.iFrameStart))
        anim.iSampleStep = getDefaultSampleStep(anim.iFrameStart, anim.iFrameEnd)
    end

    syncBlenderClipsWithSources(anim)
    local src, index = getBestBlenderScanSource(anim.scanData)
    if src then
        applyBlenderSourceToSettings(anim, src, index)
    elseif not canBakeBlenderAnimation(anim) then
        anim.bEnableAnimation = false
        anim.iSelectedSource = 0
    end
end

local function getBlenderAnimSummary(row)
    local anim = row and row.anim
    if not anim or anim.scanStatus == 'not_scanned' then
        return string.format(tLang.L('blender_anim_summary_not_scanned_fmt'), 1)
    end
    if anim.scanStatus == 'scanning' then
        return tLang.L('blender_anim_summary_scanning')
    end
    if anim.scanStatus == 'failed' then
        return tLang.L('blender_anim_summary_failed')
    end
    if anim.bEnableAnimation then
        local clips = getSelectedBlenderClips(anim)
        if #clips > 1 then
            local baked = 0
            for i = 1, #clips do
                baked = baked + getBakedFrameCount(clips[i].frameStart, clips[i].frameEnd, clips[i].sampleStep)
            end
            return string.format(tLang.L('blender_anim_summary_multi_fmt'), #clips, baked)
        elseif #clips == 1 then
            local clip = clips[1]
            local baked = getBakedFrameCount(clip.frameStart, clip.frameEnd, clip.sampleStep)
            return string.format(tLang.L('blender_anim_summary_animation_fmt'), clip.name or 'Bake', baked, clip.sampleStep)
        end
    end
    return string.format(tLang.L('blender_anim_summary_static_fmt'), anim.iStaticFrame or 1)
end

local function appendBlenderSourceFiles(tFiles)
    local st = tBlenderImportState
    local seen = {}
    for i = 1, #st.tSourceFiles do
        local row = st.tSourceFiles[i]
        if row and row.path then
            seen[row.path] = true
        end
    end
    for i = 1, #tFiles do
        local p = tFiles[i]
        if p and p ~= '' and not seen[p] then
            table.insert(st.tSourceFiles, { path = p, enabled = true, anim = nil })
            seen[p] = true
        end
    end
    local count = 0
    for i = 1, #st.tSourceFiles do
        if st.tSourceFiles[i].enabled then count = count + 1 end
    end
    st.iSelectedCount = count
end

local function setBlenderSelectionAll(enabled)
    local st = tBlenderImportState
    local count = 0
    for i = 1, #st.tSourceFiles do
        st.tSourceFiles[i].enabled = enabled
        if enabled then count = count + 1 end
    end
    st.iSelectedCount = count
end

local function refreshBlenderSelectedCount()
    local st = tBlenderImportState
    local count = 0
    for i = 1, #st.tSourceFiles do
        if st.tSourceFiles[i].enabled then count = count + 1 end
    end
    st.iSelectedCount = count
end

local function removeBlenderSourceFileAt(index)
    local st = tBlenderImportState
    table.remove(st.tSourceFiles, index)
    refreshBlenderSelectedCount()
end

getEnabledBlenderSourceRows = function()
    local st = tBlenderImportState
    local out = {}
    for i = 1, #st.tSourceFiles do
        local row = st.tSourceFiles[i]
        if row.enabled and row.path and row.path ~= '' then
            table.insert(out, row)
        end
    end
    return out
end

local function clearBlenderRunResults()
    tBlenderImportState.tRunResults = {}
end

local function pushBlenderRunResult(source, status, message)
    local st = tBlenderImportState
    table.insert(st.tRunResults, {
        source = source,
        status = status,
        message = message or '',
    })
end

local function blenderDebugPrint(st, fmt, ...)
    if not st or not st.bPrintDebugSteps then return end
    local ok, msg = pcall(string.format, fmt, ...)
    if ok then
        print('[blender_import] ' .. msg)
    end
end

local function readTextFile(path)
    local fp = io.open(path, 'rb')
    if not fp then return nil end
    local content = fp:read('*a')
    fp:close()
    return content
end

local function getFileSize(path)
    local fp = io.open(path, 'rb')
    if not fp then return 0 end
    local sz = fp:seek('end') or 0
    fp:close()
    return sz
end

local function getLogLastNonEmptyLine(content)
    if type(content) ~= 'string' or content == '' then
        return nil
    end
    local last = nil
    for line in content:gmatch('[^\r\n]+') do
        if line and line:match('%S') then
            last = line
        end
    end
    return last
end

local function shouldPrintBlenderLatestLogLine(line)
    if type(line) ~= 'string' or line == '' then
        return false
    end

    -- Hide traceback stack-frame chatter while keeping meaningful status/error lines.
    if line:match('^%s*File ".+", line %d+') then
        return false
    end
    if line:match('^%s*%a[%w_]*%s*=%s*.+') then
        return false
    end
    if line:match('^%s*return .+') then
        return false
    end
    if line:find('^%s*%^%s*$', 1) then
        return false
    end

    return true
end

local function writeTextFile(path, content)
    local fp = io.open(path, 'wb')
    if not fp then return false end
    fp:write(content or '')
    fp:close()
    return true
end

local function extractBlenderExportProgress(content)
    local out = {
        sourceFrame = nil,
        framesExported = nil,
        writingOutput = false,
        done = false,
        lastProgressLine = nil,
    }
    if type(content) ~= 'string' or content == '' then
        return out
    end

    for line in content:gmatch('[^\r\n]+') do
        local frame = line:match('%[blender_export%]%s+export frame:%s+(%d+)')
        if frame then
            out.sourceFrame = tonumber(frame)
            out.lastProgressLine = line
        end
        local exported = line:match('%[blender_export%]%s+frames exported:%s+(%d+)')
        if exported then
            out.framesExported = tonumber(exported)
            out.lastProgressLine = line
        end
        if line:find('[blender_export] writing output', 1, true) then
            out.writingOutput = true
            out.lastProgressLine = line
        end
        if line:find('[blender_export] done', 1, true) then
            out.done = true
            out.lastProgressLine = line
        end
    end
    return out
end

local function extractBlenderLogError(logPath)
    if not logPath or logPath == '' then
        return nil
    end
    local content = readTextFile(logPath)
    if type(content) ~= 'string' or content == '' then
        return nil
    end

    local lines = {}
    for line in content:gmatch('[^\r\n]+') do
        if line and line:match('%S') then
            table.insert(lines, line)
        end
    end

    local function normalize(msg)
        if not msg then return nil end
        msg = msg:gsub('^%[blender_export%]%s*', '')
        msg = msg:gsub('^%s+', ''):gsub('%s+$', '')
        return msg ~= '' and msg or nil
    end

    -- Prefer specific Python exception lines over generic traceback headers.
    for i = #lines, 1, -1 do
        local line = lines[i]
        if line:match('^[%a_][%w_]*Error:%s+.+') or line:match('^[%a_][%w_]*Exception:%s+.+') then
            return normalize(line)
        end
    end

    -- Fallback: exporter/runtime wrapper messages with useful details.
    for i = #lines, 1, -1 do
        local line = lines[i]
        if line:find('Exporter failed:', 1, true) then
            return normalize(line)
        end
        if line:find('RuntimeError:', 1, true) and not line:find('Traceback %(most recent call last%)', 1) then
            return normalize(line)
        end
        if (line:find('ERROR:', 1, true) or line:find('Error:', 1, true)) and not line:find('Traceback %(most recent call last%)', 1) then
            return normalize(line)
        end
    end

    return nil
end

local function enrichBlenderErrorMessage(msg)
    if type(msg) ~= 'string' then
        return msg
    end
    local lower = msg:lower()
    if lower:find('ascii fbx files are not supported', 1, true) then
        return msg .. ' | Hint: re-export this FBX as Binary FBX (not ASCII) or convert with a DCC tool before importing.'
    end
    if lower:find("module 'numpy' has no attribute 'bool'", 1, true)
       or lower:find('module \"numpy\" has no attribute \"bool\"', 1, true) then
        return msg .. ' | Hint: Blender 3.4 glTF addon is using deprecated np.bool with newer NumPy. Use Blender >= 3.6/4.x or install a compatible NumPy for this Blender build.'
    end
    return msg
end

local function validateBlenderScanData(tData)
    if type(tData) ~= 'table' then
        return false, 'Scan file did not return a table.'
    end
    if type(tData.scene) ~= 'table' then
        return false, 'Scan file has no scene metadata.'
    end
    tData.sources = type(tData.sources) == 'table' and tData.sources or {}

    local scene = tData.scene
    scene.frameStart = math.max(1, math.floor(tonumber(scene.frameStart or 1) or 1))
    scene.frameEnd = math.max(1, math.floor(tonumber(scene.frameEnd or scene.frameStart) or scene.frameStart))
    scene.currentFrame = math.max(1, math.floor(tonumber(scene.currentFrame or scene.frameStart) or scene.frameStart))
    scene.fps = tonumber(scene.fps or 24) or 24

    for i = 1, #tData.sources do
        local src = tData.sources[i]
        src.name = tostring(src.name or ('Source ' .. i))
        src.kind = tostring(src.kind or 'unknown')
        src.frameStart = math.max(1, math.floor(tonumber(src.frameStart or scene.frameStart) or scene.frameStart))
        src.frameEnd = math.max(1, math.floor(tonumber(src.frameEnd or src.frameStart) or src.frameStart))
        src.fps = tonumber(src.fps or scene.fps) or scene.fps
        src.confidence = tostring(src.confidence or 'low')
        src.reason = tostring(src.reason or '')
    end

    if type(tData.meshStats) == 'table' then
        local stats = tData.meshStats
        stats.available = stats.available == true
        stats.frame = math.max(1, math.floor(tonumber(stats.frame or scene.currentFrame) or scene.currentFrame))
        stats.subsets = math.max(0, math.floor(tonumber(stats.subsets or 0) or 0))
        stats.vertices = math.max(0, math.floor(tonumber(stats.vertices or 0) or 0))
        stats.indices = math.max(0, math.floor(tonumber(stats.indices or 0) or 0))
        stats.textureSearchPaths = type(stats.textureSearchPaths) == 'table' and stats.textureSearchPaths or {}
        stats.error = tostring(stats.error or '')
    else
        tData.meshStats = { available = false, textureSearchPaths = {} }
    end

    return true
end

local function startBlenderAnimationScan(row)
    local st = tBlenderImportState
    local anim = ensureBlenderAnimSettings(row)
    local exporterPath = getEditorDir() .. '/blender_mesh_export.py'
    local baseDir = st.bKeepInSourceFolder and getFileDir(row.path) or getTempDir()
    local stem = getFileStem(row.path)
    local outLua = baseDir .. '/' .. stem .. '_mbm_scan.lua'
    local dbgLog = baseDir .. '/' .. stem .. '_mbm_scan_debug.log'

    os.remove(outLua)
    os.remove(dbgLog)

    local cmd = tBlender.buildScanCmd(row.path, outLua, exporterPath, {
        debugSteps = st.bPrintDebugSteps,
    })
    if not cmd then
        anim.scanStatus = 'failed'
        anim.scanError = tLang.L('blender_anim_scan_cmd_failed')
        return
    end

    blenderDebugPrint(st, 'scan start: %s', row.path)
    blenderDebugPrint(st, 'scan outputs: lua=%s log=%s', outLua, dbgLog)
    tBlender.launchCmdAsync(cmd, dbgLog)

    anim.scanStatus = 'scanning'
    anim.scanError = ''
    anim.scanOutput = outLua
    anim.scanLog = dbgLog
    anim.scanStartTime = os.time()
end

local function pollBlenderAnimationScan(row)
    local st = tBlenderImportState
    local anim = ensureBlenderAnimSettings(row)
    if anim.scanStatus ~= 'scanning' then return end

    if tBlender.fileExists(anim.scanOutput) then
        local chunk, loadErr = loadfile(anim.scanOutput)
        if not chunk then
            anim.scanStatus = 'failed'
            anim.scanError = loadErr or tLang.L('blender_anim_scan_invalid')
            return
        end

        local okRun, tData = pcall(chunk)
        if not okRun then
            anim.scanStatus = 'failed'
            anim.scanError = tostring(tData)
            return
        end

        local okVal, errVal = validateBlenderScanData(tData)
        if not okVal then
            anim.scanStatus = 'failed'
            anim.scanError = errVal or tLang.L('blender_anim_scan_invalid')
            return
        end

        anim.scanData = tData
        anim.scanStatus = 'ready'
        applyBlenderScanDefaults(row)
        blenderDebugPrint(st, 'scan ready: %s sources=%d', row.path, #(tData.sources or {}))
        return
    end

    if tBlender.fileExists(anim.scanLog) then
        local logErr = extractBlenderLogError(anim.scanLog)
        if logErr then
            anim.scanStatus = 'failed'
            anim.scanError = enrichBlenderErrorMessage(logErr)
            return
        end
    end

    local elapsed = os.time() - (anim.scanStartTime or os.time())
    if elapsed >= math.max(10, st.iTimeoutSecs or 120) then
        anim.scanStatus = 'failed'
        anim.scanError = tLang.L('blender_anim_scan_timed_out')
    end
end

local function validateIntermediateData(tData)
    if type(tData) ~= 'table' then
        return false, 'Intermediate file did not return a table.'
    end
    local frames = tData.frames
    if type(frames) ~= 'table' or #frames == 0 then
        return false, 'No frames found in intermediate.'
    end

    for fi = 1, #frames do
        local frame = frames[fi]
        if type(frame) ~= 'table' then
            return false, string.format('Frame %d is invalid.', fi)
        end
        local subsets = frame.subsets
        if type(subsets) ~= 'table' or #subsets == 0 then
            return false, string.format('Frame %d has no subsets.', fi)
        end
        local frameVertices = 0
        for si = 1, #subsets do
            local subset = subsets[si]
            local verts = subset.vertices
            local indices = subset.indices
            if type(verts) ~= 'table' or #verts == 0 then
                return false, string.format('Frame %d subset %d has no vertices.', fi, si)
            end
            frameVertices = frameVertices + #verts
            if #verts > 65535 then
                return false, string.format('Frame %d subset %d exceeds 65535 vertices.', fi, si)
            end
            if type(indices) ~= 'table' or #indices == 0 then
                return false, string.format('Frame %d subset %d has no indices.', fi, si)
            end
            if (#indices % 3) ~= 0 then
                return false, string.format('Frame %d subset %d indices must be divisible by 3.', fi, si)
            end
            for ii = 1, #indices do
                local idx = indices[ii]
                if type(idx) ~= 'number' then
                    return false, string.format('Frame %d subset %d index %d is not numeric.', fi, si, ii)
                end
                idx = math.floor(idx)
                if idx < 1 then
                    return false, string.format('Frame %d subset %d index %d is < 1.', fi, si, ii)
                end
                if idx > #verts then
                    return false, string.format('Frame %d subset %d index %d is out of bounds.', fi, si, ii)
                end
                indices[ii] = idx
            end
        end
        if frameVertices > 65535 then
            return false, string.format('Frame %d has %d vertices total. MSH v8 uses a 16-bit index buffer and supports at most 65535 vertices per frame.', fi, frameVertices)
        end
    end

    if type(tData.animations) == 'table' then
        for ai = 1, #tData.animations do
            local anim = tData.animations[ai]
            local ini = math.floor(anim.initialFrame or 1)
            local fin = math.floor(anim.finalFrame or #frames)
            if ini < 1 or ini > #frames or fin < 1 or fin > #frames or ini > fin then
                return false, string.format('Animation %d has invalid frame range.', ai)
            end
            local tbf = tonumber(anim.timeBetweenFrame or 0)
            if not tbf or tbf <= 0 then
                return false, string.format('Animation %d has invalid frame time.', ai)
            end
            local typ = math.floor(anim.typeAnimation or 1)
            if typ < 0 or typ > 6 then
                return false, string.format('Animation %d type is out of range [0..6].', ai)
            end
            anim.initialFrame = ini
            anim.finalFrame = fin
            anim.timeBetweenFrame = tbf
            anim.typeAnimation = typ
        end
    end

    return true
end

local function validateStreamManifest(tData)
    if type(tData) ~= 'table' then
        return false, 'Stream manifest did not return a table.'
    end
    if type(tData.frames) ~= 'table' or #tData.frames == 0 then
        return false, 'Stream manifest has no frames.'
    end
    for fi = 1, #tData.frames do
        local frame = tData.frames[fi]
        if type(frame) ~= 'table' then
            return false, string.format('Manifest frame %d is invalid.', fi)
        end
        if type(frame.path) ~= 'string' or frame.path == '' then
            return false, string.format('Manifest frame %d has no path.', fi)
        end
        frame.sourceFrame = math.max(1, math.floor(tonumber(frame.sourceFrame or fi) or fi))
    end

    if type(tData.animations) == 'table' then
        for ai = 1, #tData.animations do
            local anim = tData.animations[ai]
            local ini = math.floor(anim.initialFrame or 1)
            local fin = math.floor(anim.finalFrame or #tData.frames)
            if ini < 1 or ini > #tData.frames or fin < 1 or fin > #tData.frames or ini > fin then
                return false, string.format('Animation %d has invalid frame range.', ai)
            end
            local tbf = tonumber(anim.timeBetweenFrame or 0)
            if not tbf or tbf <= 0 then
                return false, string.format('Animation %d has invalid frame time.', ai)
            end
            local typ = math.floor(anim.typeAnimation or 1)
            if typ < 0 or typ > 6 then
                return false, string.format('Animation %d type is out of range [0..6].', ai)
            end
            anim.initialFrame = ini
            anim.finalFrame = fin
            anim.timeBetweenFrame = tbf
            anim.typeAnimation = typ
        end
    end
    return true
end

local function validateStreamFrameData(tData, frameIndex)
    if type(tData) ~= 'table' then
        return false, string.format('Frame file %d did not return a table.', frameIndex)
    end
    return validateIntermediateData({
        frames = { tData },
        animations = {},
    })
end

local function applyImportVertexOptions(vertices, options)
    if type(vertices) ~= 'table' or type(options) ~= 'table' then return end
    if options.importPostProcess ~= true then return end
    local invertU = options.importInvertU == true
    local invertV = options.importInvertV == true
    if not invertU and not invertV then return end
    for vi = 1, #vertices do
        local v = vertices[vi]
        if type(v) == 'table' then
            if invertU and type(v.u) == 'number' then v.u = 1 - v.u end
            if invertV and type(v.v) == 'number' then v.v = 1 - v.v end
        end
    end
end

local function applyGeneratedMeshOptions(meshD, options)
    if type(options) ~= 'table' then return end
    if options.importPostProcess ~= true then return end
    -- Bakes the import UI's Rot X/Y/Z (degrees -- a plain, user-typeable "-90" for the usual
    -- Blender Z-up -> engine Y-up correction) directly into vertices + bones, via the same
    -- rotateFrame/applyRotationToBonesDeg pair every other rotate-bake in this file uses --
    -- instead of the old meshDebug:setAngle/getAngle ("Default angle") mechanism, which has been
    -- removed entirely (confusing, effectively unused, and the source of a real degrees-vs-radians
    -- bug: setAngle expected radians but this UI's value was always degrees).
    local ax = tonumber(options.importAngleX or 0) or 0
    local ay = tonumber(options.importAngleY or 0) or 0
    local az = tonumber(options.importAngleZ or 0) or 0
    if ax ~= 0 or ay ~= 0 or az ~= 0 then
        dpCall(function() return meshD:rotateFrame(0, ax, ay, az, 0) end)
        applyRotationToBonesDeg(meshD, ax, ay, az)
        blenderDebugPrint(tBlenderImportState, 'applied import rotation (deg): %.4f %.4f %.4f', ax, ay, az)
    end
end

local function addIntermediateFrameToMesh(meshD, frame, frameNumber, options)
    local frameIdx = meshD:addFrame(3)
    for si = 1, #frame.subsets do
        local subset = frame.subsets[si]
        local subsetIdx = meshD:addSubSet(frameIdx)
        if subset.texture and subset.texture ~= '' then
            meshD:setTexture(frameIdx, subsetIdx, subset.texture)
        end
        for _, extra in ipairs(subset.extraTextures or {}) do
            if extra.texture and extra.texture ~= '' then
                meshD:setMaterialTexture(frameIdx, subsetIdx, extra.role, extra.texture)
            end
        end
        applyImportVertexOptions(subset.vertices, options)
        blenderDebugPrint(tBlenderImportState, 'subset [%d/%d] vertices=%d indices=%d', si, #frame.subsets, #(subset.vertices or {}), #(subset.indices or {}))
        if not meshD:addVertex(frameIdx, subsetIdx, subset.vertices) then
            return false, string.format('Failed to add vertices for frame %d subset %d (vertices=%d).', frameNumber, si, #(subset.vertices or {}))
        end
        if not meshD:addIndex(frameIdx, subsetIdx, subset.indices) then
            local sample = {}
            for ii = 1, math.min(6, #(subset.indices or {})) do
                sample[#sample + 1] = tostring(subset.indices[ii])
            end
            return false, string.format('Failed to add indices for frame %d subset %d (indices=%d sample=%s).', frameNumber, si, #(subset.indices or {}), table.concat(sample, ','))
        end
    end
    return true
end

local function addAnimationsToMesh(meshD, anims, totalFrames)
    if type(anims) ~= 'table' or #anims == 0 then
        if totalFrames > 1 then
            anims = {
                {
                    name = 'Bake',
                    initialFrame = 1,
                    finalFrame = totalFrames,
                    timeBetweenFrame = 1.0 / 30.0,
                    typeAnimation = 1,
                }
            }
        else
            anims = {}
        end
    end

    for ai = 1, #anims do
        local anim = anims[ai]
        local ret = meshD:addAnim(
            anim.name or ('Bake ' .. ai),
            anim.initialFrame,
            anim.finalFrame,
            anim.timeBetweenFrame,
            anim.typeAnimation
        )
        if not ret then
            return false, string.format('Failed to add animation %d.', ai)
        end
    end
    return true
end

local function saveGeneratedMesh(meshD, outMshPath)
    local okCheck, errCheck = meshD:check()
    if not okCheck then
        return false, errCheck or 'Mesh check failed.'
    end

    if not meshD:save(outMshPath, false, false) then
        return false, 'Failed to save generated MSH.'
    end
    return true
end

local function buildMeshFromIntermediate(tData, outMshPath, options)
    local meshD = meshDebug:new()
    meshD:setType('mesh')

    local totalFrames = #tData.frames
    local totalSubsets = 0
    local totalVertices = 0
    local totalIndices = 0
    for fi = 1, totalFrames do
        local frame = tData.frames[fi]
        local subsets = frame and frame.subsets or {}
        totalSubsets = totalSubsets + #subsets
        for si = 1, #subsets do
            local subset = subsets[si]
            totalVertices = totalVertices + #(subset.vertices or {})
            totalIndices = totalIndices + #(subset.indices or {})
        end
    end
    blenderDebugPrint(tBlenderImportState, 'build mesh data: frames=%d subsets=%d vertices=%d indices=%d', totalFrames, totalSubsets, totalVertices, totalIndices)

    for fi = 1, #tData.frames do
        local frame = tData.frames[fi]
        local okAdd, errAdd = addIntermediateFrameToMesh(meshD, frame, fi, options)
        if not okAdd then
            return false, errAdd
        end
    end

    local okAnim, errAnim = addAnimationsToMesh(meshD, tData.animations, #tData.frames)
    if not okAnim then return false, errAnim end
    applyGeneratedMeshOptions(meshD, options)
    return saveGeneratedMesh(meshD, outMshPath)
end

local function buildMeshFromStreamManifest(manifestPath, outMshPath, options)
    local chunk, loadErr = loadfile(manifestPath)
    if not chunk then
        return false, loadErr or 'Failed to load stream manifest.'
    end
    local okRun, manifest = pcall(chunk)
    if not okRun then
        return false, tostring(manifest)
    end
    local okVal, errVal = validateStreamManifest(manifest)
    if not okVal then
        return false, errVal
    end

    local baseDir = getFileDir(manifestPath)
    local meshD = meshDebug:new()
    meshD:setType('mesh')

    local totalFrames = #manifest.frames
    local totalSubsets = 0
    local totalVertices = 0
    local totalIndices = 0

    blenderDebugPrint(tBlenderImportState, 'build stream mesh data: frames=%d', totalFrames)
    for fi = 1, totalFrames do
        local entry = manifest.frames[fi]
        local framePath = joinPath(baseDir, entry.path)
        local frameChunk, frameLoadErr = loadfile(framePath)
        if not frameChunk then
            return false, frameLoadErr or string.format('Failed to load stream frame %d.', fi)
        end
        local okFrameRun, frameData = pcall(frameChunk)
        if not okFrameRun then
            return false, tostring(frameData)
        end
        local okFrameVal, errFrameVal = validateStreamFrameData(frameData, fi)
        if not okFrameVal then
            return false, errFrameVal
        end

        local subsets = frameData.subsets or {}
        totalSubsets = totalSubsets + #subsets
        for si = 1, #subsets do
            totalVertices = totalVertices + #(subsets[si].vertices or {})
            totalIndices = totalIndices + #(subsets[si].indices or {})
        end

        tBlenderImportState.sProgressDetail = string.format(tLang.L('blender_import_progress_build_frame_fmt'), fi, totalFrames)
        local okAdd, errAdd = addIntermediateFrameToMesh(meshD, frameData, fi, options)
        if not okAdd then
            return false, errAdd
        end
    end
    blenderDebugPrint(tBlenderImportState, 'build stream totals: frames=%d subsets=%d vertices=%d indices=%d', totalFrames, totalSubsets, totalVertices, totalIndices)

    local okAnim, errAnim = addAnimationsToMesh(meshD, manifest.animations, totalFrames)
    if not okAnim then return false, errAnim end
    applyGeneratedMeshOptions(meshD, options)
    return saveGeneratedMesh(meshD, outMshPath)
end

local function onOpenBlenderImportDialog()
    local st = tBlenderImportState
    st.bOpen = true
    st.bOpenPopup = true
    st.sStatus = ''
    st.bStatusOk = true
end

local function onOpenMixamoGuideDialog()
    local st = tMixamoGuideState
    st.bOpen = true
    st.bOpenPopup = true
end

getBlenderImportOptionsForRow = function(row)
    local anim = ensureBlenderAnimSettings(row)
    if anim.bEnableAnimation and canBakeBlenderAnimation(anim) then
        local clips = getSelectedBlenderClips(anim)
        if #clips > 0 then
            return {
                bakeAnimation = true,
                useSceneFrameRange = false,
                frameStart = math.max(1, clips[1].frameStart or 1),
                frameEnd = math.max(1, clips[#clips].frameEnd or clips[#clips].frameStart or 1),
                sampleStep = math.max(1, clips[1].sampleStep or 1),
                animationName = clips[1].name or 'Bake',
                animationClips = clips,
            }
        end
        return {
            bakeAnimation = true,
            useSceneFrameRange = false,
            frameStart = math.max(1, anim.iFrameStart or 1),
            frameEnd = math.max(1, anim.iFrameEnd or anim.iFrameStart or 1),
            sampleStep = math.max(1, anim.iSampleStep or 1),
            animationName = anim.sAnimationName or 'Bake',
        }
    end

    local staticFrame = math.max(1, anim.iStaticFrame or 1)
    return {
        bakeAnimation = false,
        useSceneFrameRange = false,
        frameStart = staticFrame,
        frameEnd = staticFrame,
        sampleStep = 1,
    }
end

local function buildBlenderImportSuccessSummary(row, outMsh, importOptions)
    local info = meshDebug:getInfo(outMsh) or {}
    local frames = tonumber(info.totalFrames or 0) or 0
    local sizeText = formatBytes(getFileSize(outMsh))
    local texturePaths = getTextureSearchPathCountFromScan(row)
    local textureText = texturePaths and string.format(tLang.L('blender_import_summary_texture_paths_count_fmt'), texturePaths)
        or tLang.L('blender_import_summary_texture_paths_embedded')
    local animationText
    if importOptions and importOptions.bakeAnimation and type(importOptions.animationClips) == 'table' and #importOptions.animationClips > 1 then
        animationText = string.format(tLang.L('blender_import_summary_multi_animation_fmt'), #importOptions.animationClips)
    elseif importOptions and importOptions.bakeAnimation then
        animationText = string.format(
            tLang.L('blender_import_summary_animation_fmt'),
            importOptions.animationName or 'Bake',
            importOptions.frameStart or 1,
            importOptions.frameEnd or 1,
            importOptions.sampleStep or 1)
    else
        animationText = string.format(tLang.L('blender_import_summary_static_fmt'), (importOptions and importOptions.frameStart) or 1)
    end
    return string.format(
        tLang.L('blender_import_summary_fmt'),
        outMsh,
        frames,
        animationText,
        sizeText,
        textureText)
end

local function blenderImportCoroutine()
    local st = tBlenderImportState
    local exporterPath = getEditorDir() .. '/blender_mesh_export.py'
    local selected = getEnabledBlenderSourceRows()
    local total = #selected
    st.iTotal = total
    st.iProgress = 0
    local exported = 0
    local imported = 0
    local timedOut = 0
    local failed = 0
    local lastErr = nil
    local modeIntermediateOnly = st.bIntermediateOnly

    blenderDebugPrint(st, 'import start: selected=%d intermediateOnly=%s idleTimeout=%ds', total, tostring(modeIntermediateOnly), st.iTimeoutSecs)
    tBlender.setDebugEnabled(st.bPrintDebugSteps)

    for i = 1, total do
        if st.bAbortRequested then break end

        local row = selected[i]
        local src = row.path
        local baseDir = st.bKeepInSourceFolder and getFileDir(src) or getTempDir()
        local outDir = baseDir .. '/' .. getFileStem(src) .. '_mbm_import'
        local outManifest = joinPath(outDir, 'manifest.lua')
        local oldOutLua = baseDir .. '/' .. getFileStem(src) .. '_mbm_import.lua'
        local outMsh = baseDir .. '/' .. getFileStem(src) .. '.msh'
        local waitOutput = modeIntermediateOnly and outManifest or outMsh
        local dbgLog = baseDir .. '/' .. getFileStem(src) .. '_mbm_blender_debug.log'
        local cancelFile = baseDir .. '/' .. getFileStem(src) .. '_mbm_cancel'
        os.remove(oldOutLua)
        removePathRecursive(outDir)
        os.remove(outMsh)
        os.remove(dbgLog)
        os.remove(cancelFile)

        blenderDebugPrint(st, 'file %d/%d: %s', i, total, src)
        blenderDebugPrint(st, 'outputs: stream=%s manifest=%s msh=%s log=%s', outDir, outManifest, outMsh, dbgLog)
        st.sProgressDetail = string.format(tLang.L('blender_import_progress_waiting_fmt'), tUtil.getShortName(src))

        local importOptions = getBlenderImportOptionsForRow(row)
        importOptions.debugSteps = st.bPrintDebugSteps
        importOptions.cancelFile = cancelFile
        importOptions.streamOutput = modeIntermediateOnly
        importOptions.directMshOutput = not modeIntermediateOnly
        importOptions.importPostProcess = st.bImportPostProcess
        importOptions.importInvertU = st.bImportInvertU
        importOptions.importInvertV = st.bImportInvertV
        importOptions.importAngleX = st.nImportAngleX
        importOptions.importAngleY = st.nImportAngleY
        importOptions.importAngleZ = st.nImportAngleZ
        importOptions.largeMeshMode = getBlenderLargeMeshModeArg()
        importOptions.includeBones = st.bImportIncludeBones
        local cmd = tBlender.buildBakeCmd(src, modeIntermediateOnly and outDir or outMsh, exporterPath, importOptions)
        if cmd then
            tBlender.launchCmdAsync(cmd, dbgLog)
            st.sCancelFile = cancelFile
            local startTime = os.time()
            local lastActivityTime = startTime
            local lastWaitLog = -1
            local lastLogLinePrinted = nil
            local lastProgressLine = nil
            local expectedFrames = importOptions.bakeAnimation and getBakedFrameCount(importOptions.frameStart, importOptions.frameEnd, importOptions.sampleStep) or 1
            local finished = false
            while not finished do
                if st.bAbortRequested then
                    writeTextFile(cancelFile, 'cancel\n')
                    failed = failed + 1
                    lastErr = tLang.L('blender_import_status_canceled')
                    blenderDebugPrint(st, 'abort requested: %s', src)
                    pushBlenderRunResult(src, 'failed', lastErr)
                    finished = true
                end

                if not finished and tBlender.fileExists(waitOutput) then
                    local elapsed = os.time() - startTime
                    local outSize = getFileSize(waitOutput)
                    if st.bPrintDebugSteps then
                        blenderDebugPrint(st, 'output exists: %s (%d bytes)', waitOutput, outSize)
                    end

                    local chunk, loadErr = nil, nil
                    local okRun, tData = false, nil
                    local okVal, errVal = (not modeIntermediateOnly), nil

                    if outSize > 0 and modeIntermediateOnly then
                        chunk, loadErr = loadfile(outManifest)
                        if chunk then
                            okRun, tData = pcall(chunk)
                            if okRun then
                                okVal, errVal = validateStreamManifest(tData)
                            end
                        end
                    end

                    if outSize > 0 and (not modeIntermediateOnly or (chunk and okRun and okVal)) then
                        exported = exported + 1
                        blenderDebugPrint(st, 'output ready: %s', waitOutput)
                        st.sProgressDetail = tLang.L('blender_import_progress_building')
                        if modeIntermediateOnly then
                            blenderDebugPrint(st, 'intermediate-only mode: skip build/load')
                            pushBlenderRunResult(src, 'exported', tLang.L('blender_import_status_exported'))
                        else
                            local addedPaths = addBlenderSourceFallbackSearchPath(src)
                            if addedPaths > 0 then
                                blenderDebugPrint(st, 'added fallback mesh search paths: %d', addedPaths)
                            end
                            if addMeshToTable(outMsh) then
                                imported = imported + 1
                                bShowMeshTree = true
                                blenderDebugPrint(st, 'imported mesh: %s', outMsh)
                                pushBlenderRunResult(src, 'imported', buildBlenderImportSuccessSummary(row, outMsh, importOptions))
                            else
                                failed = failed + 1
                                lastErr = 'Generated MSH could not be loaded into editor.'
                                blenderDebugPrint(st, 'addMeshToTable failed: %s', outMsh)
                                pushBlenderRunResult(src, 'failed', lastErr)
                            end
                        end
                        finished = true
                    else
                        -- On Windows the file can appear before write completion; keep polling until valid or timeout.
                        local parseReason = nil
                        if outSize <= 0 then
                            parseReason = 'stream manifest is still empty'
                        elseif not chunk then
                            parseReason = loadErr or 'failed to compile stream manifest'
                        elseif not okRun then
                            parseReason = tostring(tData)
                        elseif not okVal then
                            parseReason = errVal
                        else
                            parseReason = 'stream manifest is not ready yet'
                        end

                        if st.bPrintDebugSteps then
                            blenderDebugPrint(st, 'waiting valid stream manifest (%ds): %s', elapsed, tostring(parseReason))
                        end

                        local idleSecs = os.time() - lastActivityTime
                        if idleSecs >= st.iTimeoutSecs then
                            failed = failed + 1
                            lastErr = parseReason
                            pushBlenderRunResult(src, 'failed', tostring(parseReason))
                            finished = true
                        else
                            coroutine.yield()
                        end
                    end
                elseif not finished then
                    local elapsed = os.time() - startTime
                    local content = nil
                    if tBlender.fileExists(dbgLog) then
                        content = readTextFile(dbgLog)
                        local progress = extractBlenderExportProgress(content)
                        if progress.lastProgressLine and progress.lastProgressLine ~= lastProgressLine then
                            lastProgressLine = progress.lastProgressLine
                            lastActivityTime = os.time()
                        end
                        if progress.writingOutput then
                            st.sProgressDetail = tLang.L('blender_import_progress_writing')
                        elseif progress.framesExported then
                            st.sProgressDetail = string.format(tLang.L('blender_import_progress_write_fmt'), progress.framesExported)
                        elseif progress.sourceFrame then
                            local targetFrame = 1
                            if importOptions.bakeAnimation then
                                targetFrame = math.floor((progress.sourceFrame - importOptions.frameStart) / math.max(1, importOptions.sampleStep)) + 1
                                targetFrame = math.max(1, math.min(expectedFrames, targetFrame))
                            end
                            st.sProgressDetail = string.format(tLang.L('blender_import_progress_frame_fmt'), progress.sourceFrame, targetFrame, expectedFrames)
                        end

                        local logErr = extractBlenderLogError(dbgLog)
                        if logErr then
                            logErr = enrichBlenderErrorMessage(logErr)
                            failed = failed + 1
                            lastErr = logErr
                            blenderDebugPrint(st, 'detected exporter error from log: %s', logErr)
                            pushBlenderRunResult(src, 'failed', logErr .. ' (' .. dbgLog .. ')')
                            finished = true
                        end
                    end
                    if finished then
                        -- If we already found an explicit error in Blender output, skip timeout wait.
                    elseif st.bPrintDebugSteps then
                        content = content or readTextFile(dbgLog)
                        local lastLine = getLogLastNonEmptyLine(content)
                        if lastLine
                           and lastLine ~= ''
                           and lastLine ~= lastLogLinePrinted
                           and shouldPrintBlenderLatestLogLine(lastLine) then
                            blenderDebugPrint(st, 'latest log: %s', lastLine)
                            lastLogLinePrinted = lastLine
                        end
                    end
                    if st.bPrintDebugSteps then
                        local nowTick = math.floor(elapsed / 5)
                        if nowTick ~= lastWaitLog then
                            blenderDebugPrint(st, 'waiting output (%ds idle %ds): %s', elapsed, os.time() - lastActivityTime, waitOutput)
                            lastWaitLog = nowTick
                        end
                    end
                    local idleSecs = os.time() - lastActivityTime
                    if not finished and idleSecs >= st.iTimeoutSecs then
                        timedOut = timedOut + 1
                        local msg = tLang.L('blender_import_status_timed_out')
                        if st.bPrintDebugSteps then
                            msg = msg .. ' (' .. dbgLog .. ')'
                        end
                        blenderDebugPrint(st, 'timed out after %ds idle: %s', idleSecs, src)
                        pushBlenderRunResult(src, 'timed_out', msg)
                        finished = true
                    else
                        coroutine.yield()
                    end
                end
            end
        else
            timedOut = timedOut + 1
            blenderDebugPrint(st, 'failed to build command for: %s', src)
            pushBlenderRunResult(src, 'timed_out', tLang.L('blender_import_status_timed_out'))
        end
        st.iProgress = i
        st.sCancelFile = ''
    end

    local okCount = modeIntermediateOnly and exported or imported
    if okCount > 0 then
        st.sStatus = string.format(tLang.L('blender_import_done_fmt'), okCount, timedOut)
        if failed > 0 and lastErr then
            st.sStatus = st.sStatus .. '\n' .. string.format(tLang.L('blender_import_failed_with_reason_fmt'), failed, lastErr)
        end
        st.bStatusOk = true
    else
        st.sStatus = tLang.L('blender_import_failed')
        if lastErr then
            st.sStatus = st.sStatus .. '\n' .. lastErr
        end
        st.bStatusOk = false
    end
    blenderDebugPrint(st, 'import done: okCount=%d exported=%d imported=%d timedOut=%d failed=%d', okCount, exported, imported, timedOut, failed)
    st.bImporting = false
end

local function startBlenderImport()
    local st = tBlenderImportState
    st.iProgress = 0
    st.iTotal = 0
    st.sProgressDetail = ''
    st.bAbortRequested = false
    st.sCancelFile = ''
    st.sStatus = ''
    st.bStatusOk = true
    st.bImporting = true
    clearBlenderRunResults()
    tBlender.setDebugEnabled(st.bPrintDebugSteps)
    st.co = coroutine.create(blenderImportCoroutine)
end

local function showBlenderAnimationSettingsPopup(st)
    if st.bOpenAnimSettingsPopup then
        tImGui.OpenPopup('blender_animation_settings_modal')
        st.bOpenAnimSettingsPopup = false
    end

    local row = st.tSourceFiles[st.iAnimSettingsIndex or 0]
    if not row then return end
    local anim = ensureBlenderAnimSettings(row)
    pollBlenderAnimationScan(row)

    local flags = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
    local isOpen, _ = tImGui.BeginPopupModal(tLang.L('blender_animation_settings_title') .. '###blender_animation_settings_modal', false, flags)
    if not isOpen then return end

    tImGui.Text(tUtil.getShortName(row.path))
    if tImGui.IsItemHovered(0) then
        tImGui.BeginTooltip()
        tImGui.Text(row.path)
        tImGui.EndTooltip()
    end

    tImGui.Separator()
    if anim.scanStatus == 'scanning' then
        tImGui.Text(tLang.L('blender_anim_scanning'))
    elseif anim.scanStatus == 'failed' then
        tImGui.PushStyleColor('ImGuiCol_Text', {r=1, g=0.3, b=0.3, a=1})
        tImGui.TextWrapped(anim.scanError ~= '' and anim.scanError or tLang.L('blender_anim_scan_failed'))
        tImGui.PopStyleColor()
        if tImGui.Button(tLang.L('blender_anim_rescan')) then
            startBlenderAnimationScan(row)
        end
        local changedStatic, newStatic = tImGui.InputInt(tLang.L('blender_anim_static_frame'), anim.iStaticFrame or 1, 1, 10)
        if changedStatic and newStatic then
            anim.iStaticFrame = math.max(1, newStatic)
        end
    elseif anim.scanStatus == 'ready' then
        local scan = anim.scanData or {}
        local scene = scan.scene or {}
        tImGui.Text(string.format(tLang.L('blender_anim_scene_info_fmt'),
            scene.frameStart or 1,
            scene.frameEnd or 1,
            tonumber(scene.fps or 0) or 0))
        local stats = scan.meshStats or {}
        if stats.available then
            local rowEstimate = getRowImportEstimate(row)
            tImGui.TextDisabled(string.format(
                tLang.L('blender_anim_mesh_stats_fmt'),
                stats.frame or scene.currentFrame or 1,
                stats.subsets or 0,
                formatLargeInt(stats.vertices or 0),
                formatLargeInt(stats.indices or 0),
                formatLargeInt(rowEstimate.totalVertices or 0),
                formatLargeInt(rowEstimate.totalIndices or 0),
                formatBytes(rowEstimate.estimatedRawBytes or 0)))
        elseif stats.error and stats.error ~= '' then
            tImGui.TextDisabled(string.format(tLang.L('blender_anim_mesh_stats_unavailable_fmt'), stats.error))
        end

        local issues = scan.meshCacheIssues or {}
        if #issues > 0 then
            tImGui.PushStyleColor('ImGuiCol_Text', {r=1, g=0.65, b=0.2, a=1})
            tImGui.TextWrapped(tLang.L('blender_anim_mesh_cache_issue'))
            for i = 1, #issues do
                local issue = issues[i]
                tImGui.TextWrapped('- ' .. tostring(issue.message or issue.cacheFile or 'Mesh cache issue'))
            end
            tImGui.PopStyleColor()
        end

        local sources = scan.sources or {}
        if #sources == 0 then
            tImGui.TextDisabled(tLang.L('blender_anim_no_sources'))
        else
            syncBlenderClipsWithSources(anim)
            local sourceFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg', 'ImGuiTableFlags_ScrollY')
            if tImGui.BeginTable('blenderAnimSourcesTable', 5, sourceFlags, {x=720, y=150}) then
                tImGui.TableSetupScrollFreeze(0, 1)
                tImGui.TableSetupColumn(tLang.L('blender_anim_col_use'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 55)
                tImGui.TableSetupColumn(tLang.L('blender_anim_col_name'))
                tImGui.TableSetupColumn(tLang.L('blender_anim_col_kind'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 100)
                tImGui.TableSetupColumn(tLang.L('blender_anim_col_frames'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 120)
                tImGui.TableSetupColumn(tLang.L('blender_anim_col_reason'))
                tImGui.TableHeadersRow()
                for i = 1, #sources do
                    local src = sources[i]
                    local clip = anim.tClips and anim.tClips[i] or makeBlenderClipFromSource(src, i)
                    tImGui.TableNextRow()
                    tImGui.TableNextColumn()
                    local newEnabled = tImGui.Checkbox('##blendAnimSrc-' .. i, clip.enabled == true)
                    if newEnabled ~= clip.enabled then
                        clip.enabled = newEnabled
                        anim.bManualRange = false
                        anim.bEnableAnimation = true
                        if newEnabled then
                            anim.iSelectedSource = i
                        end
                    end
                    tImGui.TableNextColumn()
                    tImGui.Text(src.name or ('Source ' .. i))
                    tImGui.TableNextColumn()
                    tImGui.Text(src.kind or '')
                    tImGui.TableNextColumn()
                    tImGui.Text(string.format('%d..%d', src.frameStart or 1, src.frameEnd or 1))
                    tImGui.TableNextColumn()
                    tImGui.TextWrapped(src.reason or src.confidence or '')
                end
                tImGui.EndTable()
            end
            tImGui.TextDisabled(tLang.L('blender_anim_multi_source_note'))
        end

        tImGui.Separator()
        local canBake = canBakeBlenderAnimation(anim)
        if not canBake then
            anim.bEnableAnimation = false
        end
        if not canBake then tImGui.BeginDisabled(true) end
        anim.bEnableAnimation = tImGui.Checkbox(tLang.L('blender_import_bake_animation'), anim.bEnableAnimation)
        if not canBake then tImGui.EndDisabled() end
        if not canBake then
            tImGui.TextDisabled(tLang.L('blender_anim_bake_disabled_cache_issue'))
        end
        if not anim.bEnableAnimation then
            local changedStatic, newStatic = tImGui.InputInt(tLang.L('blender_anim_static_frame'), anim.iStaticFrame or 1, 1, 10)
            if changedStatic and newStatic then
                anim.iStaticFrame = math.max(1, newStatic)
            end
        else
            anim.bManualRange = tImGui.Checkbox(tLang.L('blender_anim_manual_range'), anim.bManualRange)
            if anim.bManualRange then
                anim.iSelectedSource = 0
                if type(anim.tClips) == 'table' then
                    for i = 1, #anim.tClips do
                        anim.tClips[i].enabled = false
                    end
                end
                if anim.sAnimationName == '' or anim.sAnimationName == 'Bake' then
                    anim.sAnimationName = tLang.L('blender_anim_manual_default_name')
                end
            end

            local selectedClips = getSelectedBlenderClips(anim)
            if not anim.bManualRange and #selectedClips > 0 then
                tImGui.Text(tLang.L('blender_anim_selected_clips_title'))
                local clipFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg')
                if tImGui.BeginTable('blenderSelectedClipsTable', 5, clipFlags, {x=1080, y=math.min(180, 34 + (#selectedClips * 30))}) then
                    tImGui.TableSetupColumn(tLang.L('blender_anim_col_name'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 430)
                    tImGui.TableSetupColumn(tLang.L('blender_import_frame_start'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 135)
                    tImGui.TableSetupColumn(tLang.L('blender_import_frame_end'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 135)
                    tImGui.TableSetupColumn(tLang.L('blender_import_sample_step'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 150)
                    tImGui.TableSetupColumn(tLang.L('blender_anim_col_frames'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 90)
                    tImGui.TableHeadersRow()
                    for i = 1, #(anim.tClips or {}) do
                        local clip = anim.tClips[i]
                        if clip.enabled == true then
                            tImGui.TableNextRow()
                            tImGui.TableNextColumn()
                            tImGui.PushItemWidth(-1)
                            local nameChanged, newName = tImGui.InputText('##clipName-' .. i, clip.name or ('Bake ' .. i), 64, 0)
                            tImGui.PopItemWidth()
                            if nameChanged and newName then clip.name = newName end
                            tImGui.TableNextColumn()
                            tImGui.PushItemWidth(-1)
                            local fsChanged, newFs = tImGui.InputInt('##clipStart-' .. i, clip.frameStart or 1, 1, 10)
                            tImGui.PopItemWidth()
                            if fsChanged and newFs then clip.frameStart = math.max(1, newFs) end
                            tImGui.TableNextColumn()
                            tImGui.PushItemWidth(-1)
                            local feChanged, newFe = tImGui.InputInt('##clipEnd-' .. i, clip.frameEnd or clip.frameStart or 1, 1, 10)
                            tImGui.PopItemWidth()
                            if feChanged and newFe then clip.frameEnd = math.max(1, newFe) end
                            tImGui.TableNextColumn()
                            tImGui.PushItemWidth(-1)
                            local stepChanged, newStep = tImGui.InputInt('##clipStep-' .. i, clip.sampleStep or 1, 1, 10)
                            tImGui.PopItemWidth()
                            if stepChanged and newStep then clip.sampleStep = math.max(1, newStep) end
                            if tImGui.IsItemHovered(0) then
                                tImGui.BeginTooltip()
                                tImGui.Text(tLang.L('blender_import_sample_step_tooltip'))
                                tImGui.EndTooltip()
                            end
                            tImGui.TableNextColumn()
                            tImGui.Text(tostring(getBakedFrameCount(clip.frameStart, clip.frameEnd, clip.sampleStep)))
                        end
                    end
                    tImGui.EndTable()
                end
            else
                local nameChanged, newName = tImGui.InputText(tLang.L('blender_anim_engine_name'), anim.sAnimationName or 'Bake', 64, 0)
                if nameChanged and newName then
                    anim.sAnimationName = newName
                end

                local fsChanged, newFs = tImGui.InputInt(tLang.L('blender_import_frame_start'), anim.iFrameStart or 1, 1, 10)
                if fsChanged and newFs then
                    anim.iFrameStart = math.max(1, newFs)
                end
                local feChanged, newFe = tImGui.InputInt(tLang.L('blender_import_frame_end'), anim.iFrameEnd or anim.iFrameStart or 1, 1, 10)
                if feChanged and newFe then
                    anim.iFrameEnd = math.max(1, newFe)
                end
                local stepChanged, newStep = tImGui.InputInt(tLang.L('blender_import_sample_step'), anim.iSampleStep or 1, 1, 10)
                if stepChanged and newStep then
                    anim.iSampleStep = math.max(1, newStep)
                end
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('blender_import_sample_step_tooltip'))
                    tImGui.EndTooltip()
                end
            end

            local baked = 0
            local clipsForEstimate = getSelectedBlenderClips(anim)
            if #clipsForEstimate > 0 then
                for i = 1, #clipsForEstimate do
                    baked = baked + getBakedFrameCount(clipsForEstimate[i].frameStart, clipsForEstimate[i].frameEnd, clipsForEstimate[i].sampleStep)
                end
            else
                baked = getBakedFrameCount(anim.iFrameStart, anim.iFrameEnd, anim.iSampleStep)
            end
            local warnKey = nil
            if baked > 800 then
                warnKey = 'blender_anim_warning_red_fmt'
            elseif baked > 300 then
                warnKey = 'blender_anim_warning_yellow_fmt'
            end
            tImGui.Text(string.format(tLang.L('blender_anim_estimate_fmt'), baked))
            if warnKey then
                tImGui.PushStyleColor('ImGuiCol_Text', {r=1, g=0.65, b=0.2, a=1})
                tImGui.TextWrapped(string.format(tLang.L(warnKey), baked))
                tImGui.PopStyleColor()
            end
        end

        if tImGui.Button(tLang.L('blender_anim_rescan')) then
            startBlenderAnimationScan(row)
        end
    else
        if tImGui.Button(tLang.L('blender_anim_scan')) then
            startBlenderAnimationScan(row)
        end
    end

    tImGui.Separator()
    if tImGui.Button(tLang.L('blender_import_btn_close')) then
        tImGui.CloseCurrentPopup()
    end
    tImGui.EndPopup()
end

local function openUrlInBrowser(url)
    if mbm.is('windows') then
        os.execute('start "" "' .. url .. '"')
    elseif mbm.is('linux') then
        os.execute('sensible-browser "' .. url .. '"')
    elseif mbm.is('macos') then
        os.execute('open "' .. url .. '"')
    end
end

function showMixamoGuideDialog()
    local st = tMixamoGuideState
    if not st.bOpen then return end

    if st.bOpenPopup then
        tImGui.OpenPopup('mixamo_guide_modal')
        st.bOpenPopup = false
    end

    local iW, iH = mbm.getRealSizeScreen()
    local maxW = math.max(420, iW - 40)
    local maxH = math.max(260, iH - 60)
    local initialW = math.min(620, maxW)
    local initialH = math.min(520, maxH)
    tImGui.SetNextWindowSizeConstraints({x=420, y=260}, {x=maxW, y=maxH})
    tImGui.SetNextWindowSize({x=initialW, y=initialH}, tImGui.Flags('ImGuiCond_Appearing'))
    local isOpen, _ = tImGui.BeginPopupModal(tLang.L('mixamo_guide_title') .. '###mixamo_guide_modal', false, 0)
    if not isOpen then return end

    tImGui.TextWrapped(tLang.L('mixamo_guide_intro'))
    tImGui.Separator()
    tImGui.TextWrapped(tLang.L('mixamo_guide_step1'))
    tImGui.TextWrapped(tLang.L('mixamo_guide_step2'))
    tImGui.TextWrapped(tLang.L('mixamo_guide_step3'))
    tImGui.TextWrapped(tLang.L('mixamo_guide_step4'))
    tImGui.TextWrapped(tLang.L('mixamo_guide_step5'))
    tImGui.TextWrapped(tLang.L('mixamo_guide_step6'))
    tImGui.Separator()
    tImGui.PushStyleColor('ImGuiCol_Text', {r=0.8, g=0.8, b=0.3, a=1})
    tImGui.TextWrapped(tLang.L('mixamo_guide_note'))
    tImGui.PopStyleColor()
    tImGui.Separator()

    if tImGui.Button(tLang.L('mixamo_guide_btn_open')) then
        openUrlInBrowser('https://www.mixamo.com')
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('mixamo_guide_btn_close')) then
        st.bOpen = false
        tImGui.CloseCurrentPopup()
    end

    tImGui.EndPopup()
end

function showBlenderImportDialog()
    local st = tBlenderImportState
    if not st.bOpen then return end

    if st.bOpenPopup then
        tImGui.OpenPopup('blender_import_modal')
        st.bOpenPopup = false
    end

    local iW, iH = mbm.getRealSizeScreen()
    local maxW = math.max(420, iW - 40)
    local maxH = math.max(260, iH - 60)
    local initialW = math.min(720, maxW)
    local initialH = math.min(600, maxH)
    tImGui.SetNextWindowSizeConstraints({x=420, y=260}, {x=maxW, y=maxH})
    tImGui.SetNextWindowSize({x=initialW, y=initialH}, tImGui.Flags('ImGuiCond_Appearing'))
    local flags = 0
    local isOpen, _ = tImGui.BeginPopupModal(tLang.L('blender_import_modal_title') .. '###blender_import_modal', false, flags)
    if not isOpen then return end

    local blender = tBlender.blender or tBlender.detectBlender()
    local osName = (mbm.get('os') or ''):lower()

    if st.bImporting then
        if st.co and coroutine.status(st.co) == 'suspended' then
            local ok, err = coroutine.resume(st.co)
            if not ok then
                st.bImporting = false
                st.co = nil
                st.sStatus = tostring(err)
                st.bStatusOk = false
            end
        end

        local fraction = st.iTotal > 0 and (st.iProgress / st.iTotal) or 0
        tImGui.Text(string.format(tLang.L('blender_import_progress_fmt'), st.iProgress, st.iTotal))
        if st.sProgressDetail and st.sProgressDetail ~= '' then
            tImGui.TextDisabled(st.sProgressDetail)
        end
        tImGui.ProgressBar(fraction)
        tImGui.SameLine()
        if tImGui.Button(tLang.L('blender_import_btn_abort')) then
            st.bAbortRequested = true
            if st.sCancelFile and st.sCancelFile ~= '' then
                writeTextFile(st.sCancelFile, 'cancel\n')
            end
        end

        if not st.bImporting then
            if st.bStatusOk then
                tUtil.showMessage(st.sStatus)
                st.bOpen = false
                tImGui.CloseCurrentPopup()
            else
                tImGui.Separator()
                tImGui.PushStyleColor('ImGuiCol_Text', {r=1, g=0.3, b=0.3, a=1})
                tImGui.TextWrapped(st.sStatus)
                tImGui.PopStyleColor()
                if tImGui.Button(tLang.L('blender_import_btn_close')) then
                    st.bOpen = false
                    tImGui.CloseCurrentPopup()
                end
            end
        end

        tImGui.EndPopup()
        return
    end

    tImGui.Text(string.format(tLang.L('blender_import_selected_files_fmt'), st.iSelectedCount, #st.tSourceFiles))
    if tImGui.Button(tLang.L('blender_import_pick_files')) then
        local files = mbm.openMultiFile(sLastMeshPath, 'blend', 'fbx', 'glb', 'gltf', 'obj')
        if files then
            if type(files) == 'string' then
                appendBlenderSourceFiles({ files })
                sLastMeshPath = files
            elseif type(files) == 'table' then
                appendBlenderSourceFiles(files)
                if #files > 0 then sLastMeshPath = files[1] end
            end
        end
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('blender_import_clear_files')) then
        st.tSourceFiles = {}
        st.iSelectedCount = 0
    end

    if #st.tSourceFiles > 0 then
        if tImGui.Button(tLang.L('blender_import_select_all')) then
            setBlenderSelectionAll(true)
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('blender_import_select_none')) then
            setBlenderSelectionAll(false)
        end

        local tableFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg', 'ImGuiTableFlags_ScrollY')
        if tImGui.BeginTable('blenderImportFilesTable', 4, tableFlags, {x=-1, y=180}) then
            tImGui.TableSetupScrollFreeze(0, 1)
            tImGui.TableSetupColumn(tLang.L('blender_import_col_enable'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 80)
            tImGui.TableSetupColumn(tLang.L('blender_import_col_file'))
            tImGui.TableSetupColumn(tLang.L('blender_import_col_animation'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 260)
            tImGui.TableSetupColumn(tLang.L('blender_import_col_remove'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 80)
            tImGui.TableHeadersRow()
            local removeAt = 0
            for i = 1, #st.tSourceFiles do
                local row = st.tSourceFiles[i]
                tImGui.TableNextRow()
                tImGui.TableNextColumn()
                local newEnabled = tImGui.Checkbox('##blenderFileEnabled-' .. i, row.enabled)
                if newEnabled ~= row.enabled then
                    row.enabled = newEnabled
                    refreshBlenderSelectedCount()
                end
                tImGui.TableNextColumn()
                local base = tUtil.getShortName(row.path)
                tImGui.Text(base)
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(row.path)
                    tImGui.EndTooltip()
                end
                tImGui.TableNextColumn()
                if tImGui.Button(tLang.L('blender_import_animation_settings') .. '##blenderAnim-' .. i) then
                    ensureBlenderAnimSettings(row)
                    st.iAnimSettingsIndex = i
                    st.bOpenAnimSettingsPopup = true
                    if row.anim.scanStatus == 'not_scanned' then
                        startBlenderAnimationScan(row)
                    end
                end
                tImGui.TextDisabled(getBlenderAnimSummary(row))
                tImGui.TableNextColumn()
                if tImGui.Button(tLang.L('blender_import_btn_remove') .. '##blenderRemove-' .. i) then
                    removeAt = i
                end
            end
            if removeAt > 0 then
                removeBlenderSourceFileAt(removeAt)
            end
            tImGui.EndTable()
        end
    else
        tImGui.TextDisabled(tLang.L('blender_import_no_files'))
    end

    st.bIntermediateOnly = tImGui.Checkbox(tLang.L('blender_import_intermediate_only'), st.bIntermediateOnly)
    st.bPrintDebugSteps = tImGui.Checkbox(tLang.L('blender_import_print_debug_steps'), st.bPrintDebugSteps)
    st.bKeepInSourceFolder = tImGui.Checkbox(tLang.L('blender_import_keep_in_source_folder'), st.bKeepInSourceFolder)
    local largeMeshOpts = {
        tLang.L('blender_import_large_mesh_fail'),
        tLang.L('blender_import_large_mesh_vb_only'),
    }
    tImGui.PushItemWidth(300)
    local largeChanged, newLargeIdx = tImGui.Combo(tLang.L('blender_import_large_mesh_mode'), st.iLargeMeshMode or 1, largeMeshOpts, -1)
    if largeChanged and newLargeIdx then
        st.iLargeMeshMode = newLargeIdx
    end
    tImGui.PopItemWidth()
    if (st.iLargeMeshMode or 1) == 2 then
        tImGui.TextDisabled(tLang.L('blender_import_large_mesh_vb_only_note'))
    end
    st.bImportIncludeBones = tImGui.Checkbox(tLang.L('blender_import_include_bones'), st.bImportIncludeBones)
    tImGui.SameLine()
    tImGui.HelpMarker(tLang.L('blender_import_include_bones_help'))
    tImGui.Separator()
    st.bImportPostProcess = tImGui.Checkbox(tLang.L('blender_import_postprocess'), st.bImportPostProcess)
    tImGui.BeginDisabled(not st.bImportPostProcess)
    st.bImportInvertU = tImGui.Checkbox(tLang.L('blender_import_invert_u'), st.bImportInvertU)
    tImGui.SameLine()
    st.bImportInvertV = tImGui.Checkbox(tLang.L('blender_import_invert_v'), st.bImportInvertV)
    tImGui.PushItemWidth(120)
    local rxChanged, newRx = tImGui.InputFloat(tLang.L('blender_import_rotation_x'), st.nImportAngleX, 1, 15, '%.1f', 0)
    if rxChanged and newRx then st.nImportAngleX = newRx end
    tImGui.SameLine()
    local ryChanged, newRy = tImGui.InputFloat(tLang.L('blender_import_rotation_y'), st.nImportAngleY, 1, 15, '%.1f', 0)
    if ryChanged and newRy then st.nImportAngleY = newRy end
    tImGui.SameLine()
    local rzChanged, newRz = tImGui.InputFloat(tLang.L('blender_import_rotation_z'), st.nImportAngleZ, 1, 15, '%.1f', 0)
    if rzChanged and newRz then st.nImportAngleZ = newRz end
    tImGui.PopItemWidth()
    tImGui.EndDisabled()
    tImGui.PushItemWidth(120)
    local toChanged, newTo = tImGui.InputInt(tLang.L('blender_import_timeout_secs'), st.iTimeoutSecs, 10, 60)
    if toChanged and newTo and newTo >= 10 then
        st.iTimeoutSecs = newTo
    end
    tImGui.PopItemWidth()

    if blender and not blender.found then
        local key = 'blender_import_missing_' .. osName
        tImGui.PushStyleColor('ImGuiCol_Text', {r=1, g=0.6, b=0, a=1})
        tImGui.TextWrapped(tLang.L(key))
        tImGui.PopStyleColor()
        if tImGui.Button(tLang.L('blender_import_browse_blender')) then
            local exeFilter = (osName == 'windows') and '*.exe' or '*'
            local picked = mbm.openFile(st.customBlenderPath or '', exeFilter)
            if picked and picked ~= '' then
                st.customBlenderPath = picked
                tBlender.setCustomPath(picked)
                blender = tBlender.detectBlender()
            end
        end
    elseif isBlenderVersionOlderThan(blender, 5, 1) then
        tImGui.PushStyleColor('ImGuiCol_Text', {r=1, g=0.6, b=0, a=1})
        tImGui.TextWrapped(string.format(tLang.L('blender_import_version_warning_fmt'), blender.version or '?'))
        tImGui.PopStyleColor()
    end

    if st.sStatus ~= '' then
        local col = st.bStatusOk and {r=0.2, g=0.8, b=0.2, a=1} or {r=1, g=0.3, b=0.3, a=1}
        tImGui.PushStyleColor('ImGuiCol_Text', col)
        tImGui.TextWrapped(st.sStatus)
        tImGui.PopStyleColor()
    end

    if st.iSelectedCount > 0 then
        local estimate = getBlenderImportEstimateSummary()
        tImGui.Separator()
        tImGui.Text(tLang.L('blender_import_estimate_title'))
        if estimate.statsMissing > 0 then
            tImGui.TextWrapped(string.format(
                tLang.L('blender_import_estimate_partial_fmt'),
                estimate.fileCount,
                estimate.targetFrames,
                estimate.statsMissing))
        else
            tImGui.TextWrapped(string.format(
                tLang.L('blender_import_estimate_full_fmt'),
                estimate.fileCount,
                estimate.targetFrames,
                formatLargeInt(estimate.vertices),
                formatLargeInt(estimate.indices),
                formatBytes(estimate.rawBytes)))
        end
        if estimate.warning then
            tImGui.PushStyleColor('ImGuiCol_Text', {r=1, g=0.65, b=0.2, a=1})
            tImGui.TextWrapped(estimate.warning)
            tImGui.PopStyleColor()
        end
        tImGui.TextDisabled(tLang.L('blender_import_multi_anim_note'))
    end

    if #st.tRunResults > 0 then
        tImGui.Separator()
        tImGui.Text(tLang.L('blender_import_results_title'))
        local resultFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg', 'ImGuiTableFlags_ScrollY')
        if tImGui.BeginTable('blenderImportResultsTable', 3, resultFlags, {x=-1, y=150}) then
            tImGui.TableSetupScrollFreeze(0, 1)
            tImGui.TableSetupColumn(tLang.L('blender_import_col_file'))
            tImGui.TableSetupColumn(tLang.L('blender_import_col_status'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 120)
            tImGui.TableSetupColumn(tLang.L('blender_import_col_message'))
            tImGui.TableHeadersRow()

            for i = 1, #st.tRunResults do
                local rr = st.tRunResults[i]
                tImGui.TableNextRow()
                tImGui.TableNextColumn()
                tImGui.Text(tUtil.getShortName(rr.source or ''))
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(rr.source or '')
                    tImGui.EndTooltip()
                end
                tImGui.TableNextColumn()
                local statusLabel = rr.status or ''
                local statusColor = {r=1, g=0.3, b=0.3, a=1}
                if statusLabel == 'imported' then
                    statusLabel = tLang.L('blender_import_status_imported')
                    statusColor = {r=0.3, g=0.9, b=0.3, a=1}
                elseif statusLabel == 'exported' then
                    statusLabel = tLang.L('blender_import_status_exported')
                    statusColor = {r=0.3, g=0.9, b=0.3, a=1}
                elseif statusLabel == 'timed_out' then
                    statusLabel = tLang.L('blender_import_status_timed_out')
                    statusColor = {r=1, g=0.8, b=0.2, a=1}
                else
                    statusLabel = tLang.L('blender_import_status_failed')
                end
                tImGui.PushStyleColor('ImGuiCol_Text', statusColor)
                tImGui.Text(statusLabel)
                tImGui.PopStyleColor()
                tImGui.TableNextColumn()
                tImGui.TextWrapped(rr.message or '')
            end
            tImGui.EndTable()
        end
    end

    tImGui.Separator()
    local canImport = blender and blender.found and st.iSelectedCount > 0
    tImGui.BeginDisabled(not canImport)
        if tImGui.Button(tLang.L('blender_import_btn_import')) then
            startBlenderImport()
        end
    tImGui.EndDisabled()
    tImGui.SameLine()
    if tImGui.Button(tLang.L('blender_import_btn_close')) then
        st.bOpen = false
        tImGui.CloseCurrentPopup()
    end

    showBlenderAnimationSettingsPopup(st)
    tImGui.EndPopup()
end

-- Converts a parsed OBJ data table (from tiny_obj_loader.tiny_parse) into a .msh file,
-- then loads it into tLoadedMeshes via addMeshToTable. Returns true on success.
-- Each entry in tParsed is a shape subset: { tVertex, tIndex, tMaterial }.
-- tVertex[i] = { x, y, z, u, v, nx, ny, nz }   (1-based, named fields)
-- tIndex[i]  = integer (1-based vertex index into tVertex)
-- Note: tiny_parse only populates tParsed when the OBJ file has at least one .mtl material.
local function convertObjToMesh(objPath, tParsed)
    if not tParsed or #tParsed == 0 then
        tUtil.showMessage(string.format("OBJ has no usable shapes (needs a .mtl material file): %s", objPath))
        return false
    end

    local meshD = meshDebug:new()
    meshD:setType("mesh")
    local frameIdx = meshD:addFrame(3)  -- stride 3 = x,y,z

    for s = 1, #tParsed do
        local subset = tParsed[s]
        local subIdx = meshD:addSubSet(frameIdx)

        -- Add vertices: array of {x,y,z,u,v,nx,ny,nz}
        if subset.tVertex and #subset.tVertex > 0 then
            if not meshD:addVertex(frameIdx, subIdx, subset.tVertex) then
                tUtil.showMessage(string.format("Failed to add vertices for subset %d in %s", s, objPath))
                return false
            end
        end

        -- Add indices (already 1-based from tiny_parse)
        if subset.tIndex and #subset.tIndex > 0 then
            if not meshD:addIndex(frameIdx, subIdx, subset.tIndex) then
                tUtil.showMessage(string.format("Failed to add indices for subset %d in %s", s, objPath))
                return false
            end
        end

        -- Apply diffuse texture from material if available
        if subset.tMaterial then
            local texName = subset.tMaterial.map_Kd
            if texName and texName ~= '' then
                meshD:setTexture(frameIdx, subIdx, texName)
            end
        end
    end

    -- Recalculate smooth normals from geometry
    meshD:addNormals()

    -- OBJ/MTL has no equivalent of the engine's specular highlight, and the default MATERIAL
    -- (Specular white, Power=1) makes freshly imported models show an unwanted shiny highlight.
    -- Zero it out so imported meshes render flat until the user deliberately adds specular.
    local okMat, mat = dpCall(function() return meshD:getMaterial() end)
    if okMat and mat then
        mat.Specular = {r = 0, g = 0, b = 0, a = 0}
        mat.Power = 0
        dpCall(function() meshD:setMaterial(mat) end)
    end

    -- Output .msh next to the original .obj
    local mshPath = objPath:gsub('%.obj$', '.msh')
    if mshPath == objPath then
        mshPath = objPath .. '.msh'
    end

    if not meshD:save(mshPath, false, false) then
        tUtil.showMessage(string.format("Failed to save converted mesh: %s", mshPath))
        return false
    end

    return addMeshToTable(mshPath)
end

function onLoadObj()
    local fileName = mbm.openMultiFile(sLastMeshPath, "obj")
    if fileName then
        local ok, tiny_obj_loader = pcall(require, "tiny_obj_loader")
        if ok then
            local tFiles = {}
            if type(fileName) == 'string' then
                tFiles = { fileName }
            elseif type(fileName) == 'table' then
                tFiles = fileName
            end
            local nConverted = 0
            for i = 1, #tFiles do
                local objPath = tFiles[i]
                local dir = objPath:match('^(.*)[/\\]')
                if dir then mbm.addPath(dir) end
                local parsed_ok, tParsed = tiny_obj_loader.tiny_parse(objPath)
                if parsed_ok then
                    if convertObjToMesh(objPath, tParsed) then
                        nConverted = nConverted + 1
                        -- Switch to 3D camera for OBJ/mesh viewing
                        if not bCameraMode3D then
                            setMeshDebugCameraMode3d(true)
                        end
                    end
                else
                    tUtil.showMessage(string.format("Failed to parse OBJ: %s", objPath))
                end
            end
            if #tFiles > 0 then
                sLastMeshPath = tFiles[1]
                bShowMeshTree = true
                tUtil.showMessage(string.format("Converted %d/%d OBJ file(s) to .msh", nConverted, #tFiles))
            end
        else
            print('Failed to load tiny_obj_loader module')
            tUtil.showMessage("Failed to load tiny_obj_loader module")
        end
    end
end

function onLoadMeshFromFile()
    local fileName = mbm.openMultiFile(sLastMeshPath, "spt", "msh", "fnt", "tile", "ptl")
    if fileName then
        local tFiles = {}
        if type(fileName) == 'string' then
            tFiles = { fileName }
        elseif type(fileName) == 'table' then
            tFiles = fileName
        end
        for i = 1, #tFiles do
            addMeshToTable(tFiles[i])
        end
        if #tFiles > 0 then
            sLastMeshPath = tFiles[1]
            bShowMeshTree = true
            tUtil.showMessage(string.format(tLang.L("loaded_meshes_fmt"), #tFiles))
        end
    end
end

function onLoadMeshFromFolder()
    local dirname = mbm.openFolder(sLastFolderPath)
    if dirname then
        dirname = dirname:gsub("\\", "/")
        sLastFolderPath = dirname
        local tFiles = tUtil.getMeshFilesFromFolder(dirname)
        local iAdded = 0
        for i = 1, #tFiles do
            if addMeshToTable(tFiles[i]) then
                iAdded = iAdded + 1
            end
        end
        bShowMeshTree = true
        tUtil.showMessage(string.format(tLang.L("loaded_meshes_folder_fmt"), iAdded, #tFiles))
    end
end

function addMeshToTable(fileName)
    if not fileName or fileName:len() == 0 then return false end
    local meshD = meshDebug:new()
    if not meshD:load(fileName) then
        print('Failed to load mesh:', fileName)
        return false
    end
    local info = meshDebug:getInfo(fileName)
    if not info then
        info = { type = 'unknown', hasNormal = false, hasTexture = false, totalFrames = 0 }
    end
    local tFrameSel = {}
    local tCheckedRm = {}
    local nFrames = 0
    local okF, nF = dpCall(function() return meshD:getTotalFrame() end)
    if okF and nF then nFrames = nF end
    for f = 1, nFrames do
        tFrameSel[f] = true
        tCheckedRm[f * 100] = true
        local okS, nS = dpCall(function() return meshD:getTotalSubset(f) end)
        if okS and nS then
            for s = 1, nS do tCheckedRm[f * 100 + s] = true end
        end
    end
    table.insert(tLoadedMeshes, {
        fileName = fileName,
        meshDebug = meshD,
        info = info,
        loaded = true,
        modified = false,
        tFrameSelection  = tFrameSel,
        tAnimFrameExpanded  = {},
        bAutoRefreshPreview  = true,
        bFrameSelectionDirty = false,
        framePreviewPath     = nil,
        bPreviewIsFiltered   = false,
        cam3d                = { azimuth=0.3, elevation=0.3, distance=500, fx=0, fy=0, fz=0 },
        tPendingOps          = {},
        tCheckedRemove       = tCheckedRm,
        bShowFramePick       = false,
        tImportMeshD         = nil,
        iLeftSelectedRow     = nil,
        tRightChecked        = {},
        tPickLeftExpanded    = {},
        tPickRightExpanded   = {},
        sOpenNode            = nil,
        tNormalLineGood      = nil,
        tNormalLineBad       = nil,
        bNormalsVizDirty     = true,
        sNormalVizCoordType  = nil,
        tPhysicsLine         = nil,
        bPhysicsVizDirty     = true,
        sPhysicsVizCoordType = nil,
        tPhysicsCache        = nil,
        tPhysicsCountsCache  = nil,
        tPhysicsBoundsCache  = nil,
        tPhysicsExtentCache  = nil
    })
    -- Auto-switch camera to 3D when a mesh (.msh) file is loaded
    if info.type == 'mesh' and not bCameraMode3D then
        setMeshDebugCameraMode3d(true)
    end
    return true
end

local function buildCubeFaceVertices(p0, p1, p2, p3)
    return {
        {x = p0.x, y = p0.y, z = p0.z, u = 0, v = 0},
        {x = p1.x, y = p1.y, z = p1.z, u = 1, v = 0},
        {x = p2.x, y = p2.y, z = p2.z, u = 1, v = 1},
        {x = p3.x, y = p3.y, z = p3.z, u = 0, v = 1},
    }
end

-- Builds a 100x100x100 cube with 6 independently-colored, flat-shaded faces (24 unique vertices,
-- no sharing across faces, so addNormals() gives each face its own crisp normal instead of
-- averaging with its neighbors) and saves it as a real, reloadable .msh next to the engine's other
-- reference test assets -- a known-good mesh for testing lighting/normals against, and a working
-- example of the from-scratch mesh-building API. Positive axes get primary colors, negative axes
-- their complements, so the cube doubles as an orientation reference.
-- Face winding was hand-verified against calculateNormals()'s actual cross(p1-p0, p2-p0) formula
-- (it does NOT apply any CW/CCW correction) so each face's auto-computed normal genuinely points
-- outward. Despite that, the engine's actual screen-space rasterization treats this same winding
-- as CW-front, not CCW-front (confirmed by visual test: setModeFrontFace('CCW') showed the inside
-- of the cube instead of its outer faces) -- so setModeFrontFace('CW') is required here even
-- though the winding is genuinely "outward" by the raw cross-product math. Normals and culling are
-- independent mechanisms in this engine, so this doesn't affect the already-verified normals above.
function onAddColoredCube()
    local h = 50
    local A = {x = -h, y = -h, z = -h}
    local B = {x =  h, y = -h, z = -h}
    local C = {x =  h, y =  h, z = -h}
    local D = {x = -h, y =  h, z = -h}
    local E = {x = -h, y = -h, z =  h}
    local F = {x =  h, y = -h, z =  h}
    local G = {x =  h, y =  h, z =  h}
    local H = {x = -h, y =  h, z =  h}

    local tFaces = {
        {verts = {B, C, G, F}, color = '#FF0000FF'}, -- +X red
        {verts = {A, E, H, D}, color = '#00FFFFFF'}, -- -X cyan
        {verts = {D, H, G, C}, color = '#00FF00FF'}, -- +Y green
        {verts = {A, B, F, E}, color = '#FF00FFFF'}, -- -Y magenta
        {verts = {E, F, G, H}, color = '#0000FFFF'}, -- +Z blue
        {verts = {A, D, C, B}, color = '#FFFF00FF'}, -- -Z yellow
    }

    local meshD = meshDebug:new()
    meshD:setType('mesh')
    meshD:setModeFrontFace('CW')
    local frameIdx = meshD:addFrame(3)

    for _, face in ipairs(tFaces) do
        local subIdx = meshD:addSubSet(frameIdx)
        local verts = buildCubeFaceVertices(face.verts[1], face.verts[2], face.verts[3], face.verts[4])
        if not meshD:addVertex(frameIdx, subIdx, verts) then
            tUtil.showMessage(tLang.L('colored_cube_add_vertex_failed'))
            return
        end
        if not meshD:addIndex(frameIdx, subIdx, {1, 2, 3, 1, 3, 4}) then
            tUtil.showMessage(tLang.L('colored_cube_add_index_failed'))
            return
        end
        meshD:setTexture(frameIdx, subIdx, face.color)
    end

    meshD:addNormals()

    -- MESH::render() (src/render/mesh.cpp) draws nothing at all when a mesh has zero animations,
    -- even a static single-frame one -- add a trivial frame-1-to-1 animation so the cube renders
    -- immediately, without requiring a manual "Add Animation" click.
    if not meshD:addAnim('Static', 1, 1, 1.0, 0) then
        tUtil.showMessage(tLang.L('colored_cube_add_anim_failed'))
        return
    end

    -- This is a generated editor reference, not a project asset. Keep it in the platform's temp
    -- directory instead of depending on the launcher's working directory or modifying the tree.
    local mshPath = joinPath(getTempDir(), 'mini-mbm-colored_cube.msh')
    local okCheck, errCheck = meshD:check()
    if not okCheck then
        tUtil.showMessage(string.format(tLang.L('colored_cube_check_failed_fmt'), tostring(errCheck)))
        return
    end
    if not meshD:save(mshPath, false, false) then
        tUtil.showMessage(string.format(tLang.L('colored_cube_save_failed_fmt'), mshPath))
        return
    end

    if addMeshToTable(mshPath) then
        bShowMeshTree = true
        tUtil.showMessage(string.format(tLang.L('colored_cube_added_fmt'), mshPath))
    end
end

function removeMeshFromTable(index)
    local wasSelected = (iSelectedMeshIndex == index)
    local removed = table.remove(tLoadedMeshes, index)
    if removed then
        if removed.tSplitCapture then splitCaptureDestroy(removed.tSplitCapture) end
        splitCaptureDiscardBackup(removed)
        destroySplitCaptureIslandMarkers(removed)
        destroyTransformSubsetHoverMarker(removed)
        destroyNormalVisualization(removed)
        destroyPhysicsVisualization(removed)
    end
    if wasSelected then
        iLastPreviewedIndex = 0
        destroyPreviewMesh()
        destroyGhostMesh()
        -- Select the neighboring entry that took this slot (the old "next"); if the removed
        -- entry was last in the list, fall back to the new last entry (the old "previous").
        if #tLoadedMeshes == 0 then
            iSelectedMeshIndex = 0
        elseif index <= #tLoadedMeshes then
            iSelectedMeshIndex = index
        else
            iSelectedMeshIndex = #tLoadedMeshes
        end
    elseif iSelectedMeshIndex > index then
        iSelectedMeshIndex = iSelectedMeshIndex - 1
        iLastPreviewedIndex = 0
    end
end

function destroyPreviewMesh()
    if tPreviewMesh then
        tPreviewMesh.tFont = nil
        tPreviewMesh:destroy()
        tPreviewMesh = nil
    end
    tPreviewFont = nil
end

-- Spherical coordinate helpers for 3D camera orbit
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

-- WASD translates the orbit focus on the camera's horizontal forward/right plane; Page Up/Down
-- translates it on world Y. Since cam3dGetPos derives camera position from the focus, moving the
-- focus moves camera and target together without changing azimuth, elevation or orbit distance.
function updateCam3dKeyboardMovement(delta)
    local move = tCam3dMove
    if not move or (move.forward == 0 and move.right == 0 and move.vertical == 0) then return end
    if tImGui.GetWantCaptureKeyboard() or not bCameraMode3D then return end
    if iSelectedMeshIndex <= 0 or iSelectedMeshIndex > #tLoadedMeshes then return end

    local tEntry = tLoadedMeshes[iSelectedMeshIndex]
    -- isMesh3DType is declared later as a local helper, so keep this early camera helper
    -- self-contained instead of accidentally resolving a nonexistent global of the same name.
    if not (tEntry.info and tEntry.info.type == 'mesh') then return end
    local c = tEntry.cam3d
    local speed = c.distance * 0.8 * math.max(delta or 0, 0)
    local dx, dz = 0, 0

    if move.forward ~= 0 or move.right ~= 0 then
        -- Use the engine camera's own basis, as Scene Editor 3D does, then flatten and normalize
        -- it so pitched views do not move more slowly across the XZ plane.
        local fw = camera3d:getNormal('F')
        local rg = camera3d:getNormal('R')
        local fwLen = math.sqrt(fw.x * fw.x + fw.z * fw.z)
        local rgLen = math.sqrt(rg.x * rg.x + rg.z * rg.z)
        if fwLen > 1e-6 and rgLen > 1e-6 then
            dx = (fw.x / fwLen * move.forward + rg.x / rgLen * move.right) * speed
            dz = (fw.z / fwLen * move.forward + rg.z / rgLen * move.right) * speed
        end
    end

    c.fx = c.fx + dx
    c.fy = c.fy + move.vertical * speed
    c.fz = c.fz + dz
    applyCam3d(c)
end

-- Each loaded mesh entry keeps its own cam3d (orbit position around the object), so switching
-- the selected entry re-applies that entry's own (possibly still-default) camera instead of
-- keeping the view the user just set up. Only 'mesh' (.msh) entries render in 3D world space;
-- everything else (sprite/tile/particle/font/texture) renders flat in 2D world space regardless
-- of the camera-mode toggle, so a mesh<->mesh switch is the only case where carrying over the
-- 3D orbit camera is meaningful. 2D<->2D needs no action since camera2d is a single shared
-- object that already keeps its position across selection changes.
local function isMesh3DType(tEntry)
    return tEntry ~= nil and tEntry.info ~= nil and tEntry.info.type == 'mesh'
end

-- Changes the selected mesh index, preserving the 3D orbit camera view when moving between two
-- same-type (3D<->3D) entries instead of snapping to the new entry's own cam3d.
function selectMeshIndex(newIndex)
    if #tLoadedMeshes == 0 then
        iSelectedMeshIndex = 0
        return
    end
    if newIndex < 1 then newIndex = 1 end
    if newIndex > #tLoadedMeshes then newIndex = #tLoadedMeshes end
    if newIndex == iSelectedMeshIndex then return end

    local tOld = tLoadedMeshes[iSelectedMeshIndex]
    local tNew = tLoadedMeshes[newIndex]
    if bCameraMode3D and isMesh3DType(tOld) and isMesh3DType(tNew) then
        local c = tOld.cam3d
        tNew.cam3d.azimuth   = c.azimuth
        tNew.cam3d.elevation = c.elevation
        tNew.cam3d.distance  = c.distance
        tNew.cam3d.fx        = c.fx
        tNew.cam3d.fy        = c.fy
        tNew.cam3d.fz        = c.fz
    end
    iSelectedMeshIndex = newIndex
end

-- Returns map[origAnimIdx] = filteredAnimIdx (integer) if the animation survives the frame
-- filter, or false if all its frames are deselected. Also returns nRemoved count.
function computeAnimFilterMap(tEntry, meshD, nAnim)
    local tSel = tEntry.tFrameSelection or {}
    local result = {}
    local filteredCount = 0
    local nRemoved = 0
    for i = 1, nAnim do
        local ok, _, initF, finF = dpCall(function() return meshD:getAnim(i) end)
        if ok and initF and finF then
            local alive = false
            for f = initF, finF do
                if tSel[f] ~= false then alive = true; break end
            end
            if alive then
                filteredCount = filteredCount + 1
                result[i] = filteredCount
            else
                result[i] = false
                nRemoved = nRemoved + 1
            end
        else
            filteredCount = filteredCount + 1  -- unknown state: assume alive
            result[i] = filteredCount
        end
    end
    return result, nRemoved
end

-- Load selected mesh for preview. When mesh has unsaved changes, saves to temp and loads from there
-- so animation/transform edits are visible at runtime without saving to file.
function updatePreviewMesh()
    if iSelectedMeshIndex == iLastPreviewedIndex then return end
    destroyPreviewMesh()
    iLastPreviewedIndex = iSelectedMeshIndex
    if iSelectedMeshIndex <= 0 or iSelectedMeshIndex > #tLoadedMeshes then return end

    local tEntry = tLoadedMeshes[iSelectedMeshIndex]
    local meshD = tEntry.meshDebug
    local fileName = tEntry.fileName
    local info = tEntry.info or {}
    local meshType = info.type or 'unknown'

    local loadPath = fileName
    if tEntry.modified then
        local ext = fileName:match('%.([^%.]+)$') or 'msh'
        tEntry.previewPath = tEntry.previewPath or (os.tmpname() .. '.' .. ext)
        if meshD:save(tEntry.previewPath, false, false) then
            meshDebug:fakeRelease(fileName)
            meshDebug:fakeRelease(tEntry.previewPath)
            loadPath = tEntry.previewPath
        end
    else
        meshDebug:fakeRelease(fileName)
    end

    local dir = fileName:match('^(.*)[/\\]')
    if dir then mbm.addPath(dir) end

    local coordType = bCameraMode3D and '3d' or '2dw'
    local ok = false
    if meshType == 'sprite' then
        tPreviewMesh = sprite:new(coordType)
        ok = tPreviewMesh:load(loadPath)
    elseif meshType == 'mesh' then
        tPreviewMesh = mesh:new(coordType)
        ok = tPreviewMesh:load(loadPath)
        -- Force the preview's position back to the origin immediately after load, overriding
        -- DEVICE::addRenderizable's z-order auto-assign (src/core_mbm/device-common.cpp:1173,
        -- `if (position.z == 0.0f) position.z = getNextZOrderControl3d()`). Every freshly
        -- constructed 3D mesh starts at z=0 and gets stamped with a small, globally-monotonic,
        -- never-resetting z-order nudge the instant it's constructed. Since this function
        -- recreates tPreviewMesh from scratch on every single edit (bone drag, frame removal,
        -- etc. -- anything that sets iLastPreviewedIndex = 0), each recreation would otherwise
        -- land at a different tiny nonzero Z depending on how many other 3D objects happened to be
        -- created elsewhere in the session -- confirmed empirically and previously compounded by
        -- MESH_MBM::load's now-removed additive "Default position" application (30 destroy+reload
        -- cycles used to add 30 * 0.01 to Z, never converging back even when the edited field was
        -- reverted). With that field gone, only this residual z-order nudge remains, so resetting
        -- straight to the origin is the correct fix rather than reading any stored value.
        if ok then
            tPreviewMesh:setPos(0, 0, 0)
        end
    elseif meshType == 'tile' then
        tPreviewMesh = tile:new(coordType)
        ok = tPreviewMesh:load(loadPath)
    elseif meshType == 'particle' then
        tPreviewMesh = particle:new(coordType)
        ok = tPreviewMesh:load(loadPath)
        if ok then tPreviewMesh:add(100); tPreviewMesh.revive = true end
    elseif meshType == 'font' then
        tPreviewFont = font:new(loadPath)
        if tPreviewFont then
            tPreviewMesh = tPreviewFont:add(coordType, 'Mesh Debug')
            tPreviewMesh.tFont = tPreviewFont
            ok = (tPreviewMesh ~= nil)
        end
    elseif meshType == 'texture' then
        tPreviewMesh = texture:new(coordType)
        ok = tPreviewMesh:load(loadPath)
    end

    if ok and tPreviewMesh then
        tPreviewMesh.visible = true
        dpCall(function() tPreviewMesh:setAnim(tEntry.iSelectedAnim or 1) end)
        if bCameraMode3D then applyCam3d(tEntry.cam3d) end
        tEntry.bPreviewIsFiltered = false
    else
        destroyPreviewMesh()
    end
end

-- Inverts UV coordinates (u = 1-u and/or v = 1-v) across the mesh.
-- targetFrame: 0 = all frames, 1..N = specific frame.
-- Returns total number of vertices modified.
function invertMeshUV(meshD, targetFrame, invertU, invertV)
    local total = 0
    local ok, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not ok or not nFrames then return 0 end
    local fStart = targetFrame == 0 and 1 or targetFrame
    local fEnd   = targetFrame == 0 and nFrames or targetFrame
    for f = fStart, fEnd do
        local ok2, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if ok2 and nSubsets then
            for s = 1, nSubsets do
                local ok3, nV = dpCall(function() return meshD:getTotalVertex(f, s) end)
                if ok3 and nV then
                    for v = 1, nV do
                        local ok4, vd = dpCall(function() return meshD:getVertex(f, s, v) end)
                        if ok4 and vd then
                            if invertU then vd.u = 1 - vd.u end
                            if invertV then vd.v = 1 - vd.v end
                            dpCall(function() meshD:setVertex(f, s, v, vd) end)
                            total = total + 1
                        end
                    end
                end
            end
        end
    end
    return total
end

-- Detects the V coordinate range and applies the correct legacy fix:
--   V in [-1, 0]  → v = -v    (old format: V was stored as -V_correct)
--   V in [1, 2]   → v = v - 1 (already had 1-V inversion applied once, shift back)
-- targetFrame: 0 = all frames, 1..N = specific frame.
-- Returns fixType ('negate'|'shift'|'none'), total vertices touched; or false on error.
function normalizeLegacyV(meshD, targetFrame)
    local ok, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not ok or not nFrames then return false end
    local fStart = targetFrame == 0 and 1 or targetFrame
    local fEnd   = targetFrame == 0 and nFrames or targetFrame

    -- First pass: scan V range
    local minV, maxV = math.huge, -math.huge
    for f = fStart, fEnd do
        local ok2, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if ok2 and nSubsets then
            for s = 1, nSubsets do
                local ok3, nV = dpCall(function() return meshD:getTotalVertex(f, s) end)
                if ok3 and nV then
                    for v = 1, nV do
                        local ok4, vd = dpCall(function() return meshD:getVertex(f, s, v) end)
                        if ok4 and vd then
                            if vd.v < minV then minV = vd.v end
                            if vd.v > maxV then maxV = vd.v end
                        end
                    end
                end
            end
        end
    end
    if minV == math.huge then return false end

    -- Determine which fix applies based on the detected range
    local fixType = 'none'
    if maxV < 0.1 and minV < -0.1 then
        fixType = 'negate'   -- V in [-1, 0]: was stored as -V_correct; fix = -v
    elseif minV > 0.9 then
        fixType = 'shift'    -- V in [1, 2]: shift down by 1; fix = v - 1
    end
    if fixType == 'none' then return 'none', 0 end

    -- Second pass: apply fix
    local total = 0
    for f = fStart, fEnd do
        local ok2, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if ok2 and nSubsets then
            for s = 1, nSubsets do
                local ok3, nV = dpCall(function() return meshD:getTotalVertex(f, s) end)
                if ok3 and nV then
                    for v = 1, nV do
                        local ok4, vd = dpCall(function() return meshD:getVertex(f, s, v) end)
                        if ok4 and vd then
                            if fixType == 'negate' then
                                vd.v = -vd.v
                            else
                                vd.v = vd.v - 1
                            end
                            dpCall(function() meshD:setVertex(f, s, v, vd) end)
                            total = total + 1
                        end
                    end
                end
            end
        end
    end
    return fixType, total
end

-- Returns total vertex count across all frames/subsets, or 0 on error
function getMeshTotalVertices(meshD)
    local total = 0
    local ok, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not ok or not nFrames then return 0 end
    for f = 1, nFrames do
        local ok2, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if ok2 and nSubsets then
            for s = 1, nSubsets do
                local ok3, nV = dpCall(function() return meshD:getTotalVertex(f, s) end)
                if ok3 and nV then
                    total = total + nV
                end
            end
        end
    end
    return total
end

-- Computes the mesh's own bounding box from frame 1's vertex data only (not mesh:getSize()).
-- Returns center/width/height/depth, or false if frame 1 has no vertices.
function computeMeshVertexBoundsFrame1(meshD)
    local ok, nSubsets = dpCall(function() return meshD:getTotalSubset(1) end)
    if not ok or not nSubsets then return false end
    local minX,maxX,minY,maxY,minZ,maxZ = math.huge,-math.huge,math.huge,-math.huge,math.huge,-math.huge
    local total = 0
    for s = 1, nSubsets do
        local ok2, nV = dpCall(function() return meshD:getTotalVertex(1, s) end)
        if ok2 and nV then
            for v = 1, nV do
                local ok3, vd = dpCall(function() return meshD:getVertex(1, s, v) end)
                if ok3 and vd then
                    local vz = vd.z or 0
                    minX = math.min(minX, vd.x); maxX = math.max(maxX, vd.x)
                    minY = math.min(minY, vd.y); maxY = math.max(maxY, vd.y)
                    minZ = math.min(minZ, vz);   maxZ = math.max(maxZ, vz)
                    total = total + 1
                end
            end
        end
    end
    if total == 0 then return false end
    return {
        cx = (minX + maxX) * 0.5, cy = (minY + maxY) * 0.5, cz = (minZ + maxZ) * 0.5,
        width = maxX - minX, height = maxY - minY, depth = maxZ - minZ,
    }
end

-- Physics-only bounding extent, unioned across every shape kind in a meshD:getPhysics() list --
-- the Lua-side equivalent of INFO_PHYSICS::getBoundsMinMax() (src/core_mbm/physics.cpp), which
-- meshDebug has no direct binding for (getAABB/getSize/getWidthHeight only exist on a loaded
-- RENDERIZABLE, and those are themselves physics-driven -- see scene_editor3d.lua's
-- computeMeshTrueVertexExtentFrame1 comment for the underlying bug this mirrors). Used to compare
-- against computeMeshVertexBoundsFrame1() so the Physics node can warn when a hand-authored shape
-- is left much smaller than the mesh's real geometry. Returns width,height,depth or nil if the
-- shape list is empty / has no usable fields.
function computePhysicsExtent(tPhysics)
    if not tPhysics or #tPhysics == 0 then return nil end
    local minX,maxX,minY,maxY,minZ,maxZ = math.huge,-math.huge,math.huge,-math.huge,math.huge,-math.huge
    local function acc(x, y, z)
        if not x or not y then return end
        z = z or 0
        minX = math.min(minX, x); maxX = math.max(maxX, x)
        minY = math.min(minY, y); maxY = math.max(maxY, y)
        minZ = math.min(minZ, z); maxZ = math.max(maxZ, z)
    end
    for _, shape in ipairs(tPhysics) do
        if shape.type == 'cube' and shape.center and shape.half then
            acc(shape.center.x - shape.half.x, shape.center.y - shape.half.y, shape.center.z - shape.half.z)
            acc(shape.center.x + shape.half.x, shape.center.y + shape.half.y, shape.center.z + shape.half.z)
        elseif shape.type == 'sphere' and shape.center and shape.ray then
            acc(shape.center.x - shape.ray, shape.center.y - shape.ray, shape.center.z - shape.ray)
            acc(shape.center.x + shape.ray, shape.center.y + shape.ray, shape.center.z + shape.ray)
        elseif shape.type == 'triangle' then
            for _, key in ipairs({'a', 'b', 'c'}) do
                local p = shape[key]
                if p then acc(p.x, p.y, p.z) end
            end
        elseif shape.type == 'complex' then
            for _, key in ipairs({'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}) do
                local p = shape[key]
                if p then acc(p.x, p.y, p.z) end
            end
        end
    end
    if minX > maxX then return nil end
    return maxX - minX, maxY - minY, maxZ - minZ
end

-- Normalizes a 3D vector. Returns nx,ny,nz,len or nil if the vector is (near) zero.
local function normalizeVec3(x, y, z)
    local len = math.sqrt(x * x + y * y + z * z)
    if len < 1e-8 then return nil end
    return x / len, y / len, z / len, len
end

-- Computes, per local vertex index (1..nV, matching getIndex/getVertex's own local-index
-- convention), the unit geometric normal averaged from every adjacent triangle in subset
-- (f,s). Only meaningful when meshD:getModeDraw() == 'TRIANGLES' (the caller is expected to
-- check that first). Vertices not referenced by any triangle are absent from the result.
function computeGeoNormalsForSubset(meshD, f, s)
    local okIdx, indices = dpCall(function() return meshD:getIndex(f, s) end)
    if not okIdx or not indices then return {} end
    local okV, nV = dpCall(function() return meshD:getTotalVertex(f, s) end)
    if not okV or not nV or nV <= 0 then return {} end
    local okVerts, verts = dpCall(function() return meshD:getVertex(f, s, 1, nV) end)
    if not okVerts or not verts then return {} end

    -- Deliberately NOT front-face-aware: MESH_MBM_DEBUG::calculateNormals() (mesh-manager.cpp,
    -- the actual engine algorithm behind addNormals()/"Recompute from geometry") always uses the
    -- raw, uncorrected cross(p1-p0, p2-p0) with no front-face sign flip -- so this check must use
    -- exactly the same formula to answer its one real question ("does the stored normal agree
    -- with what this engine's own tool would (re)compute for this winding"). A front-face-based
    -- sign flip was tried before (assuming CW-declared meshes need it negated) but that isn't a
    -- reliable rule: culling (glFrontFace, screen-space triangle winding) and lighting (the stored
    -- per-vertex normal) are independent mechanisms with no fixed relationship to each other --
    -- whether raw cross(e1,e2) happens to point outward or inward depends on how each individual
    -- mesh's vertices were actually wound, not on its declared front-face string. Confirmed both
    -- ways: a hand-built cube (front_face=CW, raw-cross genuinely outward) was misreported as
    -- universally "Flipped" under the old sign flip despite rendering correctly, while base.msh
    -- (a Blender export, also front_face=CW) was misreported as universally "OK" under the same
    -- flip despite its top/bottom faces actually lighting from the wrong side (verified: an
    -- overhead light lit its bottom, not its top -- a real, pre-existing defect the old heuristic
    -- was masking).
    local sign = 1

    local accum = {}  -- local vertex index -> {x=,y=,z=} summed face normals
    local nTri = math.floor(#indices / 3)
    for t = 1, nTri do
        local i0, i1, i2 = indices[t * 3 - 2], indices[t * 3 - 1], indices[t * 3]
        local p0, p1, p2 = verts[i0], verts[i1], verts[i2]
        if p0 and p1 and p2 then
            local e1x, e1y, e1z = p1.x - p0.x, p1.y - p0.y, p1.z - p0.z
            local e2x, e2y, e2z = p2.x - p0.x, p2.y - p0.y, p2.z - p0.z
            local cx = sign * (e1y * e2z - e1z * e2y)
            local cy = sign * (e1z * e2x - e1x * e2z)
            local cz = sign * (e1x * e2y - e1y * e2x)
            local nx, ny, nz = normalizeVec3(cx, cy, cz)
            if nx then
                for _, vi in ipairs({i0, i1, i2}) do
                    local a = accum[vi]
                    if a then
                        a.x, a.y, a.z = a.x + nx, a.y + ny, a.z + nz
                    else
                        accum[vi] = {x = nx, y = ny, z = nz}
                    end
                end
            end
        end
    end

    local geo = {}
    for vi, sum in pairs(accum) do
        local nx, ny, nz = normalizeVec3(sum.x, sum.y, sum.z)
        if nx then geo[vi] = {x = nx, y = ny, z = nz} end
    end
    return geo
end

-- Destroys tEntry's normal-visualization line objects (if any) and clears the fields.
function destroyNormalVisualization(tEntry)
    if tEntry.tNormalLineGood then tEntry.tNormalLineGood:destroy() end
    if tEntry.tNormalLineBad then tEntry.tNormalLineBad:destroy() end
    tEntry.tNormalLineGood = nil
    tEntry.tNormalLineBad  = nil
    tEntry.tNormalGeoCache = nil
    tEntry.tNormalVertexCache = nil
    tEntry.iNormalPreviewTotal = nil
    tEntry.iNormalPreviewSegments = nil
end

NORMAL_PREVIEW_MAX_SEGMENTS = 500
NORMAL_TABLE_PAGE_SIZE = 100

-- Rebuilds the two line objects that visualize frame-1's per-vertex normals: green where the
-- stored normal agrees with the geometric face normal (from triangle winding), red where it
-- opposes it. Works for any type that has normals (sprite/tile/mesh/etc all share the same
-- vertex/index buffer shape); only color-coded when the draw mode is 'TRIANGLES' (otherwise
-- every normal is drawn green/neutral, since face winding isn't well-defined). Lines are drawn
-- in whichever coord space the preview is currently using (2dw or 3d), matching bCameraMode3D.
function rebuildNormalVisualization(tEntry, meshD)
    destroyNormalVisualization(tEntry)
    tEntry.bNormalsVizDirty = false
    local info = tEntry.info or {}
    if not info.hasNormal then return end

    local okNS, nSubsets = dpCall(function() return meshD:getTotalSubset(1) end)
    if not okNS or not nSubsets or nSubsets <= 0 then return end

    local bounds = computeMeshVertexBoundsFrame1(meshD)
    local diag = 50.0
    if bounds then
        diag = math.sqrt(bounds.width * bounds.width + bounds.height * bounds.height + bounds.depth * bounds.depth)
    end
    local lineLen = math.max(2.0, math.min(diag * 0.05, 500.0))

    local okMode, modeDraw = dpCall(function() return meshD:getModeDraw() end)
    local triOk = okMode and modeDraw == 'TRIANGLES'

    local coordType = bCameraMode3D and '3d' or '2dw'
    tEntry.sNormalVizCoordType = coordType
    local is3D = coordType == '3d'
    local lnGood = line:new(coordType, 0, 0, 0)
    local lnBad  = line:new(coordType, 0, 0, 0)
    lnGood:setColor(0, 1, 0)
    lnBad:setColor(1, 0, 0)
    local anySegment = false
    local totalVertices = 0
    for s = 1, nSubsets do
        local okV, nV = dpCall(function() return meshD:getTotalVertex(1, s) end)
        if okV and nV then totalVertices = totalVertices + nV end
    end
    local sampleStride = math.max(1, math.ceil(totalVertices / NORMAL_PREVIEW_MAX_SEGMENTS))
    local globalVertexIndex, previewSegments = 0, 0
    tEntry.tNormalGeoCache = {}
    tEntry.tNormalVertexCache = {}

    for s = 1, nSubsets do
        local okV, nV = dpCall(function() return meshD:getTotalVertex(1, s) end)
        if okV and nV and nV > 0 then
            local okVerts, verts = dpCall(function() return meshD:getVertex(1, s, 1, nV) end)
            if okVerts and verts then
                local geo = triOk and computeGeoNormalsForSubset(meshD, 1, s) or nil
                tEntry.tNormalVertexCache[s] = verts
                tEntry.tNormalGeoCache[s] = geo or {}
                for ii = 1, nV do
                    globalVertexIndex = globalVertexIndex + 1
                    local vd = verts[ii]
                    local nx, ny, nz = normalizeVec3(vd.nx, vd.ny, vd.nz)
                    if nx and (globalVertexIndex - 1) % sampleStride == 0 then
                        local isBad = false
                        if geo and geo[ii] then
                            local g = geo[ii]
                            local dot = nx * g.x + ny * g.y + nz * g.z
                            isBad = dot <= 0
                        end
                        local target = isBad and lnBad or lnGood
                        if is3D then
                            target:add({vd.x, vd.y, vd.z, vd.x + nx * lineLen, vd.y + ny * lineLen, vd.z + nz * lineLen})
                        else
                            target:add({vd.x, vd.y, vd.x + nx * lineLen, vd.y + ny * lineLen})
                        end
                        anySegment = true
                        previewSegments = previewSegments + 1
                    end
                end
            end
        end
    end
    tEntry.iNormalPreviewTotal = totalVertices
    tEntry.iNormalPreviewSegments = previewSegments

    if anySegment then
        tEntry.tNormalLineGood = lnGood
        tEntry.tNormalLineBad  = lnBad
    else
        lnGood:destroy()
        lnBad:destroy()
    end
end

-- Returns total triangle count (indexCount/3) across all frames/subsets, or 0 on error
function getMeshTotalTriangles(meshD)
    local total = 0
    local ok, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not ok or not nFrames then return 0 end
    for f = 1, nFrames do
        local ok2, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if ok2 and nSubsets then
            for s = 1, nSubsets do
                local ok3, nIdx = dpCall(function() return meshD:getTotalIndex(f, s) end)
                if ok3 and nIdx then
                    total = total + math.floor(nIdx / 3)
                end
            end
        end
    end
    return total
end

-- Returns unique texture names used by the mesh
function getMeshTextures(meshD)
    local seen = {}
    local list = {}
    local ok, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not ok or not nFrames then return list end
    for f = 1, nFrames do
        local ok2, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if ok2 and nSubsets then
            for s = 1, nSubsets do
                local ok3, tex = dpCall(function() return meshD:getTexture(f, s) end)
                if ok3 and tex and tex ~= '' and not seen[tex] then
                    seen[tex] = true
                    table.insert(list, tex)
                end
            end
        end
    end
    return list
end

-- Format bytes for display (e.g. 1234 -> "1.2 KB")
function formatBytes(bytes)
    if bytes >= 1024 * 1024 then
        return string.format('%.1f MB', bytes / (1024 * 1024))
    elseif bytes >= 1024 then
        return string.format('%.1f KB', bytes / 1024)
    else
        return string.format('%d B', bytes)
    end
end

-- Options for editable draw/cull/front-face combos
local tModeDrawOpts   = {'TRIANGLES','TRIANGLE_STRIP','TRIANGLE_FAN','LINES','LINE_LOOP','LINE_STRIP','POINTS'}
local tModeCullOpts   = {'FRONT','BACK','FRONT_AND_BACK'}
local tModeFrontOpts  = {'CW','CCW'}
-- Animation type: 0 PAUSED, 1 GROWING, 2 GROWING_LOOP, 3 DECREASING, 4 DECREASING_LOOP, 5 RECURSIVE, 6 RECURSIVE_LOOP
local tAnimTypeOpts   = {'PAUSED','GROWING','GROWING_LOOP','DECREASING','DECREASING_LOOP','RECURSIVE','RECURSIVE_LOOP'}
local function getArticulatedEasingOptions()
    return {
        tLang.L('articulated_easing_linear'),
        tLang.L('articulated_easing_in'),
        tLang.L('articulated_easing_out'),
        tLang.L('articulated_easing_in_out'),
        tLang.L('articulated_easing_smoothstep'),
        tLang.L('articulated_easing_bezier')
    }
end

function drawArticulatedBezierPreview(x1, y1, x2, y2)
    local size = {x = 180, y = 110}
    local origin = tImGui.GetCursorScreenPos()
    local minY = math.min(-0.1, y1, y2)
    local maxY = math.max(1.1, y1, y2)
    local spanY = math.max(0.001, maxY - minY)
    local function point(x, y)
        return {
            x = origin.x + math.max(0, math.min(1, x)) * size.x,
            y = origin.y + (maxY - y) / spanY * size.y
        }
    end
    local p0 = point(0, 0)
    local p1 = point(x1, y1)
    local p2 = point(x2, y2)
    local p3 = point(1, 1)
    tImGui.AddRect(origin, {x = origin.x + size.x, y = origin.y + size.y},
        {r = 0.55, g = 0.55, b = 0.55, a = 0.7}, 0, 0, 1)
    tImGui.AddLine(point(0, 0), point(1, 0), {r = 0.35, g = 0.35, b = 0.35, a = 0.8}, 1)
    tImGui.AddLine(point(0, 1), point(1, 1), {r = 0.35, g = 0.35, b = 0.35, a = 0.8}, 1)
    tImGui.AddLine(p0, p1, {r = 0.65, g = 0.65, b = 0.65, a = 0.8}, 1)
    tImGui.AddLine(p2, p3, {r = 0.65, g = 0.65, b = 0.65, a = 0.8}, 1)
    tImGui.AddBezierCubic(p0, p1, p2, p3, {r = 0.2, g = 0.75, b = 1, a = 1}, 2, 32)
    tImGui.AddCircleFilled(p1, 4, {r = 1, g = 0.65, b = 0.1, a = 1}, 12)
    tImGui.AddCircleFilled(p2, 4, {r = 1, g = 0.35, b = 0.2, a = 1}, 12)
    tImGui.Dummy(size)
end

-- Animation type: 0 PAUSED, 1 GROWING, 2 GROWING_LOOP, 3 DECREASING, 4 DECREASING_LOOP, 5 RECURSIVE, 6 RECURSIVE_LOOP
local tAnimTypeOpts   = {'PAUSED','GROWING','GROWING_LOOP','DECREASING','DECREASING_LOOP','RECURSIVE','RECURSIVE_LOOP'}

-- Returns a table: frameIndex (1-based) -> animIndex (1-based) owning that frame, or 0 for orphan.
function buildFrameOwnerMap(meshD, nAnim)
    local ownerMap = {}
    local okT, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okT or not nFrames then return ownerMap end
    for f = 1, nFrames do ownerMap[f] = 0 end
    for i = 1, nAnim do
        local ok, _, initF, finF = dpCall(function() return meshD:getAnim(i) end)
        if ok and initF and finF then
            for f = initF, finF do
                if ownerMap[f] ~= nil then ownerMap[f] = i end
            end
        end
    end
    return ownerMap
end

-- Builds a filtered meshDebug with only the selected frames + remapped animations.
-- Source is always tEntry.fileName (saved file). Returns tempD or nil.
function buildFilteredMesh(tEntry)
    local meshD = tEntry.meshDebug
    local tSel  = tEntry.tFrameSelection or {}
    local okT, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okT or not nFrames or nFrames == 0 then return nil end

    -- Build old->new frame index map (only selected frames, ascending order)
    local oldToNew = {}
    local newIdx   = 0
    for f = 1, nFrames do
        if tSel[f] ~= false then
            newIdx = newIdx + 1
            oldToNew[f] = newIdx
        end
    end
    if newIdx == 0 then return nil end

    -- Load a fresh copy from the saved file
    local tempD = meshDebug:new()
    if not tempD:load(tEntry.fileName) then return nil end

    -- Remap / remove animations (process in reverse so indices stay stable)
    local nAnim = (tEntry.info and tEntry.info.animation) or 0
    for i = nAnim, 1, -1 do
        local ok, name, initF, finF, time, typ = dpCall(function() return tempD:getAnim(i) end)
        if ok and name and initF and finF then
            local newInit, newFin
            for f = initF, finF do
                if oldToNew[f] then
                    if not newInit then newInit = oldToNew[f] end
                    newFin = oldToNew[f]
                end
            end
            if not newInit then
                tempD:removeAnim(i)
            else
                tempD:updateAnim(i, name, newInit, newFin, time or 0.1, typ or 0)
            end
        end
    end

    -- For kept frames with subset-level selection: remove UN-checked subsets
    -- (whole-frame selection keeps all subsets; subset selection keeps only chosen ones)
    local tCR = tEntry.tCheckedRemove or {}
    local framesToRemove = {}
    for f = nFrames, 1, -1 do
        if tSel[f] ~= false then
            local okS, nSubs = dpCall(function() return tempD:getTotalSubset(f) end)
            if okS and nSubs then
                -- Count checked vs unchecked subsets
                local anyUnchecked = false
                local anyChecked   = false
                for s = 1, nSubs do
                    if tCR[f * 100 + s] or false then
                        anyChecked = true
                    else
                        anyUnchecked = true
                    end
                end
                if anyUnchecked then
                    if not anyChecked then
                        -- All subsets unchecked: remove whole frame to avoid empty-frame error
                        framesToRemove[f] = true
                    else
                        -- Some subsets unchecked: remove only the unchecked ones
                        for s = nSubs, 1, -1 do
                            if not (tCR[f * 100 + s] or false) then
                                tempD:removeSubset(f, s)
                            end
                        end
                    end
                end
            end
        end
    end

    -- Guard: if nothing survives after both frame + subset filters, bail out early
    local survivingFrames = 0
    for f = 1, nFrames do
        if tSel[f] ~= false and not framesToRemove[f] then
            survivingFrames = survivingFrames + 1
        end
    end
    if survivingFrames == 0 then return nil end

    -- Remove unselected frames (tFrameSelection) and all-subsets-unchecked frames
    for f = nFrames, 1, -1 do
        if tSel[f] == false or framesToRemove[f] then
            tempD:removeFrame(f)
        end
    end
    return tempD
end

-- Rebuilds tPreviewMesh from a frame-filtered temp file. Blocked when tEntry.modified=true.
function refreshFrameFilterPreview(tEntry, index)
    if tEntry.modified then return end  -- blocked; UI shows tooltip

    local meshD    = tEntry.meshDebug
    local info     = tEntry.info or {}
    local meshType = info.type or 'unknown'

    local tempD = buildFilteredMesh(tEntry)
    if not tempD then
        -- Zero frames/subsets selected: disable preview and warn
        destroyPreviewMesh()
        iLastPreviewedIndex = index
        tUtil.showMessageWarn(tLang.L('no_frames_to_save'))
        return
    end

    -- Save filtered mesh to a dedicated temp path
    local ext = tEntry.fileName:match('%.([^%.]+)$') or 'msh'
    tEntry.framePreviewPath = tEntry.framePreviewPath or (os.tmpname() .. '.' .. ext)
    if not tempD:save(tEntry.framePreviewPath, false, false) then return end
    meshDebug:fakeRelease(tEntry.framePreviewPath)

    local dir = tEntry.fileName:match('^(.*)[/\\]')
    if dir then mbm.addPath(dir) end

    destroyPreviewMesh()
    local coordType = bCameraMode3D and '3d' or '2dw'
    local ok = false
    if meshType == 'sprite' then
        tPreviewMesh = sprite:new(coordType); ok = tPreviewMesh:load(tEntry.framePreviewPath)
    elseif meshType == 'mesh' then
        tPreviewMesh = mesh:new(coordType);   ok = tPreviewMesh:load(tEntry.framePreviewPath)
    elseif meshType == 'tile' then
        tPreviewMesh = tile:new(coordType);       ok = tPreviewMesh:load(tEntry.framePreviewPath)
    elseif meshType == 'particle' then
        tPreviewMesh = particle:new(coordType)
        ok = tPreviewMesh:load(tEntry.framePreviewPath)
        if ok then tPreviewMesh:add(100); tPreviewMesh.revive = true end
    elseif meshType == 'font' then
        tPreviewFont = font:new(tEntry.framePreviewPath)
        if tPreviewFont then
            tPreviewMesh = tPreviewFont:add(coordType, 'Mesh Debug')
            tPreviewMesh.tFont = tPreviewFont
            ok = (tPreviewMesh ~= nil)
        end
    elseif meshType == 'texture' then
        tPreviewMesh = texture:new(coordType); ok = tPreviewMesh:load(tEntry.framePreviewPath)
    end

    if ok and tPreviewMesh then
        -- A freshly-created 3D mesh can receive an automatic device Z-order nudge when
        -- registered. Frame/subset filter refreshes must not move the user's preview or the
        -- apparent reference used by the split-capture cube.
        if meshType == 'mesh' then tPreviewMesh:setPos(0, 0, 0) end
        tPreviewMesh.visible = true
        dpCall(function()
            local nA = (tEntry.info and tEntry.info.animation) or 0
            local selAnim = tEntry.iSelectedAnim or 1
            if nA > 0 then
                local animMap = computeAnimFilterMap(tEntry, meshD, nA)
                local mapped = animMap[selAnim]
                if mapped then
                    tPreviewMesh:setAnim(mapped)
                else
                    -- Selected anim filtered out; play first surviving animation
                    for i = 1, nA do
                        if animMap[i] then tPreviewMesh:setAnim(animMap[i]); break end
                    end
                end
            else
                tPreviewMesh:setAnim(selAnim)
            end
        end)
        if bCameraMode3D then applyCam3d(tEntry.cam3d) end
        tEntry.bPreviewIsFiltered = true
    else
        destroyPreviewMesh()
    end
    iLastPreviewedIndex = index  -- prevent updatePreviewMesh from overriding this
end

-- Renders the compact+expand frame-selection table inside the Animations tree node.
function showAnimFrameSelectionTable(tEntry, meshD, index)
    local tSel  = tEntry.tFrameSelection or {}
    local nAnim = (tEntry.info and tEntry.info.animation) or 0
    local okT, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okT or not nFrames or nFrames == 0 then return end

    local ownerMap = buildFrameOwnerMap(meshD, nAnim)

    if tImGui.TreeNodeEx(tLang.L("frame_animation_edit") .. '##fsel-' .. index, 0) then
        local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg')
        if tImGui.BeginTable('frameSel-' .. index, 3, tblFlags) then
            tImGui.TableSetupColumn('##cbcol', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 22)
            tImGui.TableSetupColumn('Animation')
            tImGui.TableSetupColumn('Selected', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 90)
            tImGui.TableHeadersRow()

            -- Per-animation rows
            for i = 1, nAnim do
                local ok, name, initF, finF = dpCall(function() return meshD:getAnim(i) end)
                if ok and name and initF and finF then
                    -- Count selected frames in this animation range
                    local total, selected = 0, 0
                    for f = initF, finF do
                        total = total + 1
                        if tSel[f] ~= false then selected = selected + 1 end
                    end
                    local animChecked = (selected == total)

                    tImGui.TableNextRow()
                    tImGui.TableNextColumn()
                    local newAnimChecked = tImGui.Checkbox('##animCb-' .. index .. '-' .. i, animChecked)
                    if newAnimChecked ~= animChecked then
                        for f = initF, finF do tSel[f] = newAnimChecked end
                        tEntry.bFrameSelectionDirty = true
                    end

                    tImGui.TableNextColumn()
                    local expandLabel = (tEntry.tAnimFrameExpanded[i] and '\xe2\x96\xbc ' or '\xe2\x96\xba ') .. (name ~= '' and name or ('Anim ' .. i)) .. '  (' .. initF .. '\xe2\x80\x93' .. finF .. ')'
                    if tImGui.Selectable(expandLabel .. '##animRow-' .. index .. '-' .. i, false, 0) then
                        tEntry.tAnimFrameExpanded[i] = not tEntry.tAnimFrameExpanded[i]
                    end

                    tImGui.TableNextColumn()
                    tImGui.Text(string.format(tLang.L("frames_selected_fmt"), selected, total))

                    -- Expanded per-frame checkboxes
                    if tEntry.tAnimFrameExpanded[i] then
                        tImGui.TableNextRow()
                        tImGui.TableNextColumn()
                        tImGui.TableNextColumn()
                        local perLine, count = 8, 0
                        for f = initF, finF do
                            local fChecked = (tSel[f] ~= false)
                            local newFChecked = tImGui.Checkbox(tostring(f) .. '##fc-' .. index .. '-' .. f, fChecked)
                            if newFChecked ~= fChecked then
                                tSel[f] = newFChecked
                                tEntry.bFrameSelectionDirty = true
                            end
                            count = count + 1
                            if count < (finF - initF + 1) and count % perLine ~= 0 then
                                tImGui.SameLine()
                            end
                        end
                        tImGui.TableNextColumn()
                    end
                end
            end

            -- Orphan (unassigned) frames row
            local orphans = {}
            for f = 1, nFrames do
                if ownerMap[f] == 0 then table.insert(orphans, f) end
            end
            if #orphans > 0 then
                local selOrphan = 0
                for _, f in ipairs(orphans) do
                    if tSel[f] ~= false then selOrphan = selOrphan + 1 end
                end
                local orphanChecked = (selOrphan == #orphans)

                tImGui.TableNextRow()
                tImGui.TableNextColumn()
                local newOrphanChecked = tImGui.Checkbox('##orphCb-' .. index, orphanChecked)
                if newOrphanChecked ~= orphanChecked then
                    for _, f in ipairs(orphans) do tSel[f] = newOrphanChecked end
                    tEntry.bFrameSelectionDirty = true
                end

                tImGui.TableNextColumn()
                local orphKey = 'orphRow-' .. index
                local orphLabel = (tEntry.tAnimFrameExpanded[orphKey] and '\xe2\x96\xbc ' or '\xe2\x96\xba ') .. tLang.L("unassigned_frames") .. '  (' .. #orphans .. ')'
                if tImGui.Selectable(orphLabel .. '##' .. orphKey, false, 0) then
                    tEntry.tAnimFrameExpanded[orphKey] = not tEntry.tAnimFrameExpanded[orphKey]
                end
                tImGui.TableNextColumn()
                tImGui.Text(string.format(tLang.L("frames_selected_fmt"), selOrphan, #orphans))

                if tEntry.tAnimFrameExpanded[orphKey] then
                    tImGui.TableNextRow()
                    tImGui.TableNextColumn()
                    tImGui.TableNextColumn()
                    local perLine, count = 8, 0
                    for _, f in ipairs(orphans) do
                        local fChecked = (tSel[f] ~= false)
                        local newFChecked = tImGui.Checkbox(tostring(f) .. '##ofc-' .. index .. '-' .. f, fChecked)
                        if newFChecked ~= fChecked then
                            tSel[f] = newFChecked
                            tEntry.bFrameSelectionDirty = true
                        end
                        count = count + 1
                        if count < #orphans and count % perLine ~= 0 then
                            tImGui.SameLine()
                        end
                    end
                    tImGui.TableNextColumn()
                end
            end

            tImGui.EndTable()
        end
        tImGui.Separator()
        local hasFilt = false
        for _, v in pairs(tSel) do
            if v == false then hasFilt = true; break end
        end
        if tImGui.Button(tLang.L("save_frame_sel_as") .. '##saveAs-' .. index) then
            doSaveAs(tEntry, index)
        end
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text(tLang.L(hasFilt and 'save_as_tooltip_filt' or 'save_as_tooltip'))
            tImGui.EndTooltip()
        end
        tImGui.TextDisabled(tLang.L("save_as_hint"))
        tImGui.TreePop()
    end
end

-- Shader effect variable helpers (same as shader_editor)
local function shaderInputFloatMinMax(psVs, tVar, index, sAlias, sTreeName)
    local flags, ret = 0, false
    if tImGui.TreeNodeEx(sTreeName or tVar.name, flags) then
        local step = (tVar.max[index] - tVar.min[index]) * 0.05
        if step <= 0 then step = 0.01 end
        local stepFast, fmt = step * 5, "%.7f"
        local r, fv = tImGui.InputFloat('##' .. psVs .. '-' .. tVar.name .. '-' .. tostring(index), tVar.value[index], step, stepFast, fmt, flags)
        if r and fv >= tVar.min[index] and fv <= tVar.max[index] then tVar.value[index] = fv ret = true end
        tImGui.TextDisabled((sAlias or 'Value') .. ' min')
        r, fv = tImGui.InputFloat('##' .. psVs .. '-min-' .. tVar.name, tVar.min[index], 0.5, 1, fmt, flags)
        if r and fv <= tVar.max[index] then tVar.min[index] = fv ret = true end
        tImGui.TextDisabled((sAlias or 'Value') .. ' max')
        r, fv = tImGui.InputFloat('##' .. psVs .. '-max-' .. tVar.name, tVar.max[index], 0.5, 1, fmt, flags)
        if r and fv >= tVar.min[index] then tVar.max[index] = fv ret = true end
        tImGui.TreePop()
    end
    return ret
end

local function shaderColorRGBMinMax(psVs, tVar)
    local flags, ret = tImGui.Flags('ImGuiColorEditFlags_HDR','ImGuiColorEditFlags_NoLabel'), false
    if tImGui.TreeNodeEx(tVar.name, 0) then
        local c = {r=tVar.value[1], g=tVar.value[2], b=tVar.value[3]}
        local clicked, rgb = tImGui.ColorEdit3('##cur-' .. psVs .. tVar.name, c, flags)
        if clicked then tVar.value[1],tVar.value[2],tVar.value[3] = rgb.r,rgb.g,rgb.b ret = true end
        c = {r=tVar.min[1], g=tVar.min[2], b=tVar.min[3]}
        clicked, rgb = tImGui.ColorEdit3('##min-' .. psVs .. tVar.name, c, flags)
        if clicked then tVar.min[1],tVar.min[2],tVar.min[3] = rgb.r,rgb.g,rgb.b ret = true end
        c = {r=tVar.max[1], g=tVar.max[2], b=tVar.max[3]}
        clicked, rgb = tImGui.ColorEdit3('##max-' .. psVs .. tVar.name, c, flags)
        if clicked then tVar.max[1],tVar.max[2],tVar.max[3] = rgb.r,rgb.g,rgb.b ret = true end
        tImGui.TreePop()
    end
    return ret
end

local function shaderColorRGBAMinMax(psVs, tVar)
    local flags, ret = tImGui.Flags('ImGuiColorEditFlags_HDR','ImGuiColorEditFlags_NoLabel'), false
    if tImGui.TreeNodeEx(tVar.name, 0) then
        local c = {r=tVar.value[1], g=tVar.value[2], b=tVar.value[3], a=tVar.value[4]}
        local clicked, rgb = tImGui.ColorEdit4('##cur-' .. psVs .. tVar.name, c, flags)
        if clicked then tVar.value[1],tVar.value[2],tVar.value[3],tVar.value[4] = rgb.r,rgb.g,rgb.b,rgb.a ret = true end
        c = {r=tVar.min[1], g=tVar.min[2], b=tVar.min[3], a=tVar.min[4]}
        clicked, rgb = tImGui.ColorEdit4('##min-' .. psVs .. tVar.name, c, flags)
        if clicked then tVar.min[1],tVar.min[2],tVar.min[3],tVar.min[4] = rgb.r,rgb.g,rgb.b,rgb.a ret = true end
        c = {r=tVar.max[1], g=tVar.max[2], b=tVar.max[3], a=tVar.max[4]}
        clicked, rgb = tImGui.ColorEdit4('##max-' .. psVs .. tVar.name, c, flags)
        if clicked then tVar.max[1],tVar.max[2],tVar.max[3],tVar.max[4] = rgb.r,rgb.g,rgb.b,rgb.a ret = true end
        tImGui.TreePop()
    end
    return ret
end

local function indexOf(t, val)
    for i, v in ipairs(t) do if v == val then return i end end
    return 1
end

-- Aggregates SECTION_VERTEX_SKIN_WEIGHTS summary stats (docs/mesh-v11-format.md Sec. 6f) by
-- looping every frame-1 vertex, mirroring getMeshTotalVertices/getMeshTotalTriangles's own
-- per-frame/per-subset aggregation idiom -- except this loop is per-VERTEX (thousands, not tens),
-- so the caller MUST cache the result (tEntry.weightStats) rather than calling this every frame;
-- see showMeshInfoTable's own cache-invalidation comment for where it gets cleared.
local function computeWeightStats(meshD)
    local okHas, has = dpCall(function() return meshD:hasVertexWeights() end)
    if not (okHas and has) then return { has = false } end
    local nVert = getMeshTotalVertices(meshD)
    local weighted, totalInfluences, maxInfluences = 0, 0, 0
    for i = 1, nVert do
        local okGW, n1, w1, n2, w2, n3, w3, n4, w4 = dpCall(function() return meshD:getVertexWeight(i) end)
        if okGW and n1 then
            weighted = weighted + 1
            local infl = 1
            if n2 then infl = infl + 1 end
            if n3 then infl = infl + 1 end
            if n4 then infl = infl + 1 end
            totalInfluences = totalInfluences + infl
            if infl > maxInfluences then maxInfluences = infl end
        end
    end
    local okBones, nBones = dpCall(function() return meshD:getTotalVertexWeightBones() end)
    return {
        has = true,
        totalVertices = nVert,
        weightedVertices = weighted,
        bones = (okBones and nBones) or 0,
        avgInfluences = weighted > 0 and (totalInfluences / weighted) or 0,
        maxInfluences = maxInfluences,
    }
end

function showMeshInfoTable(tEntry, index)
    local meshD = tEntry.meshDebug
    local info = tEntry.info or {}

    local function onEdit()
        tEntry.modified = true
        tEntry.weightStats = nil
        if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
    end

    -- Read-only info table
    local tRows = {}
    local function addRow(prop, val)
        if val ~= nil and val ~= '' then table.insert(tRows, { prop, tostring(val) }) end
    end
    addRow('File version', info.version)
    do local ok, v = dpCall(function() return meshD:getVersion() end); if ok and v and v > 0 then addRow('Loaded version', v) end end
    addRow(tLang.L('fx_texture_storage'), getTextureAnimationEffectStorageLabel(info))
    addRow('Type', info.type)
    do local ok, v = dpCall(function() return meshD:getTotalFrame() end); addRow('Total frames', info.totalFrames or (ok and v)) end
    if info.type == 'particle' then addRow('Stages', info.stages) end
    if info.type == 'texture' and info.ext then addRow('Extension', info.ext) end
    addRow('Has normals', info.hasNormal ~= nil and (info.hasNormal and 'yes' or 'no') or nil)
    addRow('Has texture', info.hasTexture ~= nil and (info.hasTexture and 'yes' or 'no') or nil)
    local nVert = getMeshTotalVertices(meshD)
    if nVert > 0 then addRow('Total vertices', nVert) end
    local nTri = getMeshTotalTriangles(meshD)
    if nTri > 0 then addRow('Total triangles', nTri) end
    do local ok, ib = dpCall(function() return meshD:isIndexBuffer() end); if ok then addRow('Index buffer', ib and 'yes' or 'no') end end
    local texList = getMeshTextures(meshD)
    if #texList > 0 then addRow('Textures', table.concat(texList, ', ')) end

    -- SECTION_VERTEX_SKIN_WEIGHTS (docs/mesh-v11-format.md Sec. 6f) summary. Cached on tEntry
    -- (computeWeightStats loops every vertex -- thousands, unlike this table's other per-frame/
    -- per-subset stats above), invalidated only by onEdit() below (the same "Remove" action that
    -- can change it) and by import/reload (addMeshToTable never carries a stale tEntry forward).
    if tEntry.weightStats == nil then
        tEntry.weightStats = computeWeightStats(meshD)
    end
    local ws = tEntry.weightStats
    addRow('Skin weights', ws.has and 'yes' or 'no')
    if ws.has then
        addRow('Weighted vertices', string.format('%d / %d', ws.weightedVertices, ws.totalVertices))
        addRow('Bones referenced', ws.bones)
        addRow('Avg influences/vertex', string.format('%.2f', ws.avgInfluences))
        addRow('Max influences/vertex', ws.maxInfluences)
    end

    if #tRows > 0 then
        if isLegacyTextureAnimationEffectStorage(info) then
            tImGui.TextWrapped(string.format(tLang.L('mesh_migration_warning_fmt'), getMeshFileVersion(info)))
        end
        local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg')
        if tImGui.BeginTable('meshInfoRO-' .. index, 2, tblFlags) then
            tImGui.TableSetupColumn('Property')
            tImGui.TableSetupColumn('Value')
            tImGui.TableHeadersRow()
            for i = 1, #tRows do
                tImGui.TableNextRow()
                tImGui.TableNextColumn()
                tImGui.Text(tRows[i][1])
                tImGui.TableNextColumn()
                tImGui.TextWrapped(tRows[i][2])
            end
            tImGui.EndTable()
        end
    end

    -- Editable: Remove Vertex Skin Weights (single mesh) -- mirrors the Normals node's own
    -- removeNormals pattern (byte-savings toast computed before removal, then clear + invalidate
    -- caches). The main use case (per direct user request): once the FBX exported with real
    -- weights has done its job for an external animation tool, and this engine only ever supports
    -- static-frame mesh (no runtime skinning consumer exists to use this data going forward), the
    -- section can be dropped to reclaim file size.
    if ws.has then
        tImGui.Spacing()
        if tImGui.Button(tLang.L('remove_vertex_weights') .. '##removeVertexWeights-' .. index) then
            local shortName = tUtil.getShortName(tEntry.fileName)
            local bytesSaved = ws.weightedVertices * 20 -- 4x u8 paletteIndex + 4x f32 weight per vertex
            local okRemove = dpCall(function() meshD:removeVertexWeights() end)
            if okRemove then
                onEdit()
                tUtil.showMessage(string.format('Removed vertex skin weights: %s\n%d vertices (~%s saved)',
                    shortName, ws.weightedVertices, formatBytes(bytesSaved)), 5)
            end
        end
    end

    -- Editable: Remap texture paths
    if #texList > 0 then
        tImGui.Spacing()
        tImGui.Text(tLang.L("set_texture_path"))
        for _, oldTex in ipairs(texList) do
            tImGui.TextDisabled(tUtil.getShortName(oldTex))
            tImGui.SameLine()
            if tImGui.Button('...' .. '##remapTex-' .. index .. '-' .. oldTex) then
                local newTex = mbm.openFile(sLastMeshPath, table.unpack(tUtil.supported_images or {'png','jpg','bmp','tga'}))
                if newTex then
                    if type(newTex) == 'table' then newTex = newTex[1] end
                    local okF2, nF2 = dpCall(function() return meshD:getTotalFrame() end)
                    if okF2 and nF2 then
                        for ff = 1, nF2 do
                            local okS2, nS2 = dpCall(function() return meshD:getTotalSubset(ff) end)
                            if okS2 and nS2 then
                                for ss = 1, nS2 do
                                    local okG, curTex = dpCall(function() return meshD:getTexture(ff, ss) end)
                                    if okG and curTex == oldTex then
                                        dpCall(function() meshD:setTexture(ff, ss, newTex) end)
                                    end
                                end
                            end
                        end
                    end
                    onEdit()
                    tUtil.showMessage(string.format('%s -> %s', tUtil.getShortName(oldTex), tUtil.getShortName(newTex)))
                end
            end
        end
    end

    -- Editable: Mode draw
    tImGui.Spacing()
    tImGui.Text(tLang.L("draw_mode"))
    tImGui.SameLine()
    tImGui.HelpMarker(tLang.L("help_draw_mode_error"))
    do
        local ok, curMode = dpCall(function() return meshD:getModeDraw() end)
        if ok and curMode then
            local idx = indexOf(tModeDrawOpts, curMode)
            local ret, newIdx = tImGui.Combo('##modeDraw-' .. index, idx, tModeDrawOpts, -1)
            if ret and newIdx and newIdx ~= idx then
                local okSet = dpCall(function() meshD:setModeDraw(tModeDrawOpts[newIdx]) end)
                if okSet then onEdit() end
            end
        end
    end

    -- Editable: Face culling
    tImGui.Text(tLang.L("face_culling"))
    do
        local ok, curMode = dpCall(function() return meshD:getModeCullFace() end)
        if ok and curMode then
            local idx = indexOf(tModeCullOpts, curMode)
            local ret, newIdx = tImGui.Combo('##modeCull-' .. index, idx, tModeCullOpts, -1)
            if ret and newIdx and newIdx ~= idx then
                local okSet = dpCall(function() meshD:setModeCullFace(tModeCullOpts[newIdx]) end)
                if okSet then onEdit() end
            end
        end
    end

    -- Editable: Front face
    tImGui.Text(tLang.L("front_face_label"))
    do
        local ok, curMode = dpCall(function() return meshD:getModeFrontFace() end)
        if ok and curMode then
            local idx = indexOf(tModeFrontOpts, curMode)
            local ret, newIdx = tImGui.Combo('##modeFront-' .. index, idx, tModeFrontOpts, -1)
            if ret and newIdx and newIdx ~= idx then
                local okSet = dpCall(function() meshD:setModeFrontFace(tModeFrontOpts[newIdx]) end)
                if okSet then onEdit() end
            end
        end
    end

end

-- ---------------------------------------------------------------------------
-- Execute all pending frame/subset ops for tEntry in-memory
-- ---------------------------------------------------------------------------
function executeFrameOps(tEntry, meshD, index)
    local ops = tEntry.tPendingOps
    if not ops or #ops == 0 then return end
    -- Sort removals descending to avoid index shifting
    local removals = {}
    local copies   = {}
    for _, op in ipairs(ops) do
        if op.kind == 'removeFrame' or op.kind == 'removeSubset' then
            table.insert(removals, op)
        else
            table.insert(copies, op)
        end
    end
    -- Execute copy/insert ops first (low→high target)
    for _, op in ipairs(copies) do
        if op.kind == 'copyFrame' then
            dpCall(function()
                meshD:copyFrameFrom(op.srcMesh, op.srcFrame)
            end)
            -- copyFrameFrom always appends; rotate into the correct position when an
            -- explicit anchor is set (insertBefore=true → position anchor; false → anchor+1).
            local anchor = op.anchor or 0
            if anchor > 0 then
                local okF, nTotal = dpCall(function() return meshD:getTotalFrame() end)
                nTotal = (okF and nTotal) or 0
                local insertAt = op.insertBefore and anchor or (anchor + 1)
                -- Rotation needed only when fNEW (at nTotal) isn't already at the right spot
                if insertAt > 0 and insertAt < nTotal then
                    local N = nTotal - 1  -- number of frames before this append
                    -- Create stable staging copies of the frames that must shift right
                    for p = insertAt, N do
                        dpCall(function() meshD:copyFrameFrom(meshD, p) end)
                    end
                    -- Remove the original frames by always picking from position insertAt;
                    -- each removal shifts subsequent frames left, bubbling fNEW into place.
                    for _ = insertAt, N do
                        dpCall(function() meshD:removeFrame(insertAt) end)
                    end
                end
            end
        elseif op.kind == 'copySubset' then
            dpCall(function()
                meshD:copySubsetFrom(op.targetFrame, op.srcMesh, op.srcFrame, op.srcSubset)
            end)
            -- copySubsetFrom always appends; rotate into the correct position when an
            -- explicit anchor is set (insertBefore=true → position anchor; false → anchor+1).
            local anchor = op.anchorSubset or 0
            if anchor > 0 then
                local okS, nTotal = dpCall(function() return meshD:getTotalSubset(op.targetFrame) end)
                nTotal = (okS and nTotal) or 0
                -- Desired 1-based position for the newly appended subset
                local insertAt = op.insertBefore and anchor or (anchor + 1)
                -- Rotation only needed when the new subset (at nTotal) isn't already there
                if insertAt > 0 and insertAt < nTotal then
                    -- Duplicate targetFrame as a temp staging frame so we can safely
                    -- copy from it (self-copy within the same frame is a use-after-free in C++).
                    local okF, nFramesNow = dpCall(function() return meshD:getTotalFrame() end)
                    nFramesNow = (okF and nFramesNow) or 0
                    dpCall(function() meshD:copyFrameFrom(meshD, op.targetFrame) end)
                    local tempFrame = nFramesNow + 1
                    -- For each subset that must shift right (positions insertAt..nTotal-1),
                    -- copy it from the stable tempFrame to the end of targetFrame and then
                    -- remove the original at position insertAt.
                    -- After (nTotal-insertAt) passes the new subset has bubbled to insertAt.
                    for i = 0, (nTotal - 1 - insertAt) do
                        local srcIdx = insertAt + i
                        dpCall(function()
                            meshD:copySubsetFrom(op.targetFrame, meshD, tempFrame, srcIdx)
                        end)
                        dpCall(function()
                            meshD:removeSubset(op.targetFrame, insertAt)
                        end)
                    end
                    -- Remove the temporary staging frame (always the last frame)
                    dpCall(function() meshD:removeFrame(tempFrame) end)
                end
            end
        end
    end
    -- Sort removals: subsets first (desc within same frame), then frames (desc)
    table.sort(removals, function(a, b)
        if a.kind == 'removeSubset' and b.kind == 'removeSubset' then
            if a.frame == b.frame then return a.subset > b.subset end
            return a.frame > b.frame
        elseif a.kind == 'removeFrame' and b.kind == 'removeFrame' then
            return a.frame > b.frame
        elseif a.kind == 'removeSubset' then return true
        else return false
        end
    end)
    for _, op in ipairs(removals) do
        if op.kind == 'removeFrame' then
            dpCall(function() meshD:removeFrame(op.frame) end)
        elseif op.kind == 'removeSubset' then
            dpCall(function() meshD:removeSubset(op.frame, op.subset) end)
        end
    end
    tEntry.tPendingOps        = {}
    tEntry.iLeftSelectedRow   = nil
    tEntry.tRightChecked      = {}
    tEntry.tCheckedRemove     = {}
    -- Refresh info
    local ok, newInfo = dpCall(function() return meshDebug:getInfo(tEntry.fileName) end)
    if ok and newInfo then tEntry.info = newInfo end
    -- Clean orphaned animation frames
    local nAnim = (tEntry.info and tEntry.info.animation) or 0
    local okTF, nF = dpCall(function() return meshD:getTotalFrame() end)
    if okTF and nF and nF > 0 and nAnim > 0 then
        for i = nAnim, 1, -1 do
            local ok2, _, initF, finF = dpCall(function() return meshD:getAnim(i) end)
            if ok2 and initF and finF then
                if initF > nF and finF > nF then
                    dpCall(function() meshD:removeAnim(i) end)
                else
                    local clampI = math.min(initF, nF)
                    local clampF = math.min(finF, nF)
                    if clampI ~= initF or clampF ~= finF then
                        dpCall(function()
                            local name, _, _, t, typ = meshD:getAnim(i)
                            meshD:updateAnim(i, name, clampI, clampF, t, typ)
                        end)
                    end
                end
            end
        end
        local ok3, newInfo2 = dpCall(function() return meshDebug:getInfo(tEntry.fileName) end)
        if ok3 and newInfo2 then tEntry.info = newInfo2 end
    end
    tEntry.modified = true
end

-- ---------------------------------------------------------------------------
-- Frame Pick popup (import frames/subsets from another .msh file)
-- ---------------------------------------------------------------------------
function showFramePickWindow(tEntry, meshD, index)
    if not tEntry.bShowFramePick then return end
    local title = tLang.L('frame_pick_title') .. '##fp-' .. index
    local ww, wh = mbm.getSizeScreen()
    local popW   = math.min(900, ww - 40)
    local popH   = math.min(540, wh - 80)
    tImGui.SetNextWindowSize({x=popW, y=popH}, tImGui.Flags('ImGuiCond_Appearing'))
    tImGui.SetNextWindowPos({x=ww * 0.5, y=wh * 0.5}, tImGui.Flags('ImGuiCond_Appearing'), {x=0.5, y=0.5})
    local fp_visible, fp_closed = tImGui.Begin(title, true, 0)
    if fp_visible then
        -- Load button
        if tImGui.Button(tLang.L('import_frames_from_file') .. '##pickLoad-' .. index) then
            local file = mbm.openFile(sLastMeshPath, 'msh')
            if file and file ~= '' then
                local newD = meshDebug:new()
                if newD:load(file) then
                    tEntry.tImportMeshD    = newD
                    tEntry.tPickLeftExpanded  = {}
                    tEntry.tPickRightExpanded = {}
                    tEntry.tRightChecked   = {}
                    tEntry.iLeftSelectedRow = nil
                end
            end
        end
        local importD = tEntry.tImportMeshD
        if importD then
            -- Two-column layout
            local halfW = (popW - 20) * 0.5
            local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_ScrollY', 'ImGuiTableFlags_RowBg')

            -- Left table: current mesh (single-select anchor)
            local okLF, nLeftFrames = dpCall(function() return meshD:getTotalFrame() end)
            if not okLF then nLeftFrames = 0 end
            tImGui.Text(tLang.L('frame_pick_left'))
            if tImGui.BeginTable('fpLeft-' .. index, 2, tblFlags, {x=halfW, y=popH - 120}) then
                tImGui.TableSetupColumn('##fpleft-sel', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 40)
                tImGui.TableSetupColumn(tLang.L('frame_selection'))
                tImGui.TableHeadersRow()
                for f = 1, (nLeftFrames or 0) do
                    local isPendingDel = false
                    for _, op in ipairs(tEntry.tPendingOps) do
                        if op.kind == 'removeFrame' and op.frame == f then isPendingDel = true break end
                    end
                    tImGui.TableNextRow()
                    tImGui.TableSetColumnIndex(0)
                    if isPendingDel then
                        tImGui.TextDisabled('x')
                    else
                        local cur = tEntry.iLeftSelectedRow or 0
                        local newVal = tImGui.RadioButton('##fplr-' .. index .. '-' .. f, cur, f * 100)
                        if newVal ~= cur then tEntry.iLeftSelectedRow = newVal end
                    end
                    tImGui.TableSetColumnIndex(1)
                    local expanded = tEntry.tPickLeftExpanded[f]
                    if isPendingDel then
                        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), 0.9, 0.3, 0.3, 1)
                        tImGui.Text('[DEL] Frame ' .. f)
                        tImGui.PopStyleColor()
                    else
                        if tImGui.TreeNodeEx('Frame ' .. f .. '##fplt-' .. index .. '-' .. f, expanded and tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen') or 0) then
                            tEntry.tPickLeftExpanded[f] = true
                            local okS, nS = dpCall(function() return meshD:getTotalSubset(f) end)
                            for s = 1, (okS and nS or 0) do
                                tImGui.TableNextRow()
                                tImGui.TableSetColumnIndex(0)
                                local cur = tEntry.iLeftSelectedRow or 0
                                local newVal = tImGui.RadioButton('##fpls-' .. index .. '-' .. f .. '-' .. s, cur, f * 100 + s)
                                if newVal ~= cur then tEntry.iLeftSelectedRow = newVal end
                                tImGui.TableSetColumnIndex(1)
                                tImGui.Text('    Subset ' .. s)
                            end
                            tImGui.TreePop()
                        else
                            tEntry.tPickLeftExpanded[f] = false
                        end
                    end
                end
                tImGui.EndTable()
            end

            tImGui.SameLine()

            -- Right table: import mesh (multi-select checkboxes)
            local okRF, nRightFrames = dpCall(function() return importD:getTotalFrame() end)
            if not okRF then nRightFrames = 0 end
            tImGui.BeginGroup()
            tImGui.Text(tLang.L('frame_pick_right'))
            if tImGui.BeginTable('fpRight-' .. index, 2, tblFlags, {x=halfW, y=popH - 120}) then
                tImGui.TableSetupColumn('##fpright-sel', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 40)
                tImGui.TableSetupColumn(tLang.L('frame_selection'))
                tImGui.TableHeadersRow()
                for f = 1, (nRightFrames or 0) do
                    tImGui.TableNextRow()
                    tImGui.TableSetColumnIndex(0)
                    local key = 'f' .. f
                    local checked = tEntry.tRightChecked[key] and true or false
                    tEntry.tRightChecked[key] = tImGui.Checkbox('##fprc-' .. index .. '-' .. f, checked)
                    tImGui.TableSetColumnIndex(1)
                    local expanded = tEntry.tPickRightExpanded[f]
                    if tImGui.TreeNodeEx('Frame ' .. f .. '##fprt-' .. index .. '-' .. f, expanded and tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen') or 0) then
                        tEntry.tPickRightExpanded[f] = true
                        local okS, nS = dpCall(function() return importD:getTotalSubset(f) end)
                        for s = 1, (okS and nS or 0) do
                            tImGui.TableNextRow()
                            tImGui.TableSetColumnIndex(0)
                            local skey = 'f' .. f .. 's' .. s
                            local schecked = tEntry.tRightChecked[skey] and true or false
                            tEntry.tRightChecked[skey] = tImGui.Checkbox('##fprsc-' .. index .. '-' .. f .. '-' .. s, schecked)
                            tImGui.TableSetColumnIndex(1)
                            tImGui.Text('    Subset ' .. s)
                        end
                        tImGui.TreePop()
                    else
                        tEntry.tPickRightExpanded[f] = false
                    end
                end
                tImGui.EndTable()
            end
            tImGui.EndGroup()

            tImGui.Separator()

            -- Validate selection
            local anchorRow  = tEntry.iLeftSelectedRow
            local anchorIsFrame = anchorRow and (anchorRow % 100 == 0)
            local rightHasFrames, rightHasSubsets = false, false
            local selectedRightFrames = {}
            local selectedRightSubsets = {}
            for k, v in pairs(tEntry.tRightChecked) do
                if v then
                    if k:match('^f%d+$') then
                        rightHasFrames = true
                        local fn = tonumber(k:match('%d+'))
                        table.insert(selectedRightFrames, fn)
                    elseif k:match('^f%d+s%d+$') then
                        rightHasSubsets = true
                        local fn, sn = k:match('^f(%d+)s(%d+)$')
                        table.insert(selectedRightSubsets, {f=tonumber(fn), s=tonumber(sn)})
                    end
                end
            end
            local mixedRight = rightHasFrames and rightHasSubsets

            -- Add buttons
            local canAddFrame  = anchorIsFrame and rightHasFrames and not mixedRight and anchorRow
            local canAddSubset = (not anchorIsFrame and anchorRow) and rightHasSubsets and not mixedRight

            if not anchorRow then
                tImGui.TextDisabled(tLang.L('no_anchor_selected'))
            elseif mixedRight then
                tImGui.TextDisabled(tLang.L('select_frames_or_subsets'))
            else
                local function queueFrames(before)
                    local tgt = math.floor(anchorRow / 100)
                    table.sort(selectedRightFrames)
                    if not before then table.sort(selectedRightFrames, function(a,b) return a > b end) end
                    for _, f in ipairs(selectedRightFrames) do
                        table.insert(tEntry.tPendingOps, {
                            kind='copyFrame', srcMesh=importD, srcFrame=f, insertBefore=before, anchor=tgt
                        })
                    end
                    tEntry.bShowFramePick = false
                end
                local function queueSubsets(before)
                    local tgtFrame  = math.floor(anchorRow / 100)
                    local tgtSubset = anchorRow % 100
                    table.sort(selectedRightSubsets, function(a, b)
                        if a.f == b.f then
                            if before then return a.s < b.s else return a.s > b.s end
                        end
                        if before then return a.f < b.f else return a.f > b.f end
                    end)
                    for _, ss in ipairs(selectedRightSubsets) do
                        table.insert(tEntry.tPendingOps, {
                            kind='copySubset', srcMesh=importD, srcFrame=ss.f, srcSubset=ss.s,
                            targetFrame=tgtFrame, insertBefore=before, anchorSubset=tgtSubset
                        })
                    end
                    tEntry.bShowFramePick = false
                end
                if canAddFrame then
                    if tImGui.Button(tLang.L('add_frame_after') .. '##fpaf-' .. index) then queueFrames(false) end
                    tImGui.SameLine()
                    if tImGui.Button(tLang.L('add_frame_before') .. '##fpbf-' .. index) then queueFrames(true) end
                elseif canAddSubset then
                    if tImGui.Button(tLang.L('add_subset_after') .. '##fpas-' .. index) then queueSubsets(false) end
                    tImGui.SameLine()
                    if tImGui.Button(tLang.L('add_subset_before') .. '##fpbs-' .. index) then queueSubsets(true) end
                end
            end
        end
    end
    if fp_closed then tEntry.bShowFramePick = false end
    tImGui.End()
end

-- ---------------------------------------------------------------------------
-- Bones 3D gizmo: world<->bone-space conversion and sphere/cylinder gizmo geometry.
-- ---------------------------------------------------------------------------

-- Raw-vertex sphere/cylinder builders -- no named sphere/cylinder primitive exists in SHAPE_MESH's
-- Lua binding.
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

-- Cylinder oriented directly toward an arbitrary world-space direction (dx,dy,dz), via vertex
-- math instead of building it along a fixed local +Y axis and rotating the object with
-- setAngle(0,0,theta) afterward -- the bone-link cylinder used to only rotate around Z (a flat,
-- XY-plane-only angle, correct only when dz==0), which visually detached the cylinder from its
-- two joints as soon as a bone moved along Z relative to its parent (direct user report:
-- dragging a bone in Z/Y mode "moved the joint but the bone got lost/detached" -- Z/Y dragging is
-- exactly what makes dz nonzero). Rather than guess this engine's Euler rotation order/composition
-- to fix setAngle, this builds an orthonormal (right, axis, forward) basis from the direction
-- itself and bakes the cylinder's geometry directly into that basis -- correct for any direction,
-- no setAngle call needed at all (matches physic_editor.lua's own boxCorners/handle-marker
-- precedent of computing world-relative vertices directly rather than relying on object rotation).
local function orientedCylinderVerts(dx, dy, dz, radiusTop, radiusBottom, radialSegments)
    radialSegments = radialSegments or 10
    local height = math.sqrt(dx * dx + dy * dy + dz * dz)
    if height < 0.0001 then return {} end
    local ax, ay, az = dx / height, dy / height, dz / height -- unit "length" axis (replaces local +Y)

    -- Any vector not (nearly) parallel to axis, to seed a perpendicular basis.
    local sx, sy, sz = 0, 1, 0
    if math.abs(ay) > 0.999 then sx, sy, sz = 1, 0, 0 end

    -- right = normalize(cross(axis, seed)) -- replaces local +X
    local rx, ry, rz = ay * sz - az * sy, az * sx - ax * sz, ax * sy - ay * sx
    local rlen = math.sqrt(rx * rx + ry * ry + rz * rz)
    rx, ry, rz = rx / rlen, ry / rlen, rz / rlen

    -- forward = cross(right, axis) -- replaces local +Z, completes the orthonormal basis
    local fx, fy, fz = ry * az - rz * ay, rz * ax - rx * az, rx * ay - ry * ax

    local verts = {}
    -- u,w are the local radial offsets around the ring, v is the distance along the cylinder's
    -- own length -- projected through the (right, axis, forward) basis instead of raw X/Y/Z.
    local function push(u, v, w)
        table.insert(verts, u * rx + v * ax + w * fx)
        table.insert(verts, u * ry + v * ay + w * fy)
        table.insert(verts, u * rz + v * az + w * fz)
    end
    for i = 0, radialSegments - 1 do
        local a1 = (i / radialSegments) * math.pi * 2
        local a2 = ((i + 1) / radialSegments) * math.pi * 2
        local x1b, z1b = math.cos(a1) * radiusBottom, math.sin(a1) * radiusBottom
        local x2b, z2b = math.cos(a2) * radiusBottom, math.sin(a2) * radiusBottom
        local x1t, z1t = math.cos(a1) * radiusTop,    math.sin(a1) * radiusTop
        local x2t, z2t = math.cos(a2) * radiusTop,    math.sin(a2) * radiusTop
        push(x1b, 0, z1b); push(x2t, height, z2t); push(x2b, 0, z2b)
        push(x1b, 0, z1b); push(x1t, height, z1t); push(x2t, height, z2t)
    end
    return verts
end

-- Bones are stored in the same raw coordinate space as vertex data -- there is no persistent
-- object-level position/angle applied on top anymore (that mechanism, "Default position"/"Default
-- angle", was removed: it was confusing and effectively unused, and every rotate/scale/translate
-- of a mesh now bakes directly into vertices + bones together instead, see
-- applyRotationToBonesDeg/applyScaleToBones/applyTranslateToBones below). So bone-local coordinates
-- already ARE world coordinates; these two are trivial passthroughs, kept only so call sites don't
-- need to care whether a conversion is needed.
local function boneToWorld(meshD, bx, by, bz)
    return bx, by, bz
end

local function worldToBone(meshD, wx, wy, wz)
    return wx, wy, wz
end

-- ---------------------------------------------------------------------------
-- Bones-node Rotate/Scale/Translate: keeps the skeleton in sync with the Transform-style vertex
-- bakes triggered from the Bones node itself (meshD:rotateFrame/scaleFrame/translateFrame). Bones
-- are stored independently of vertex data (see bones_transform_warning) -- these helpers replay
-- the identical operation against every joint's own x,y,z so the two never drift apart, instead of
-- silently leaving old bone positions/orientations behind a freshly re-scaled/re-rotated mesh.
-- All three always target every frame/subset (matching meshD:rotateFrame(0,...)'s "0 = all" default
-- convention) since there is exactly one skeleton per mesh, not one per frame.
-- (applyRotationToBonesDeg itself now lives near the top of the file, alongside rotateX/Y/Z, since
-- the Blender-import post-process rotation needs it long before this point in the file.)

-- Radius scales by the mean of |sx|,|sy|,|sz| -- bones are spheres, so a single non-uniform scale
-- factor has no exact meaning; the mean is a reasonable stand-in and matches the common case of a
-- uniform scale exactly (sx==sy==sz). `length` (feeds tail reconstruction on export) scales by the
-- same mean factor for the same reason -- exact anisotropic tracking isn't possible for an
-- arbitrarily-oriented bone. Stored scaleX/Y/Z compose the real per-axis factors (round-trip
-- metadata only -- Blender edit bones have no scale concept to reconstruct against). rotX/Y/Z pass
-- through unchanged: a scale doesn't change a bone's orientation.
local function applyScaleToBones(meshD, sx, sy, sz)
    local radiusScale = (math.abs(sx) + math.abs(sy) + math.abs(sz)) / 3
    local okTotal, nBones = dpCall(function() return meshD:getTotalBone() end)
    nBones = (okTotal and nBones) or 0
    for i = 1, nBones do
        local okG, name, x, y, z, radius, parentName, rotX, rotY, rotZ, scaleX, scaleY, scaleZ, length =
            dpCall(function() return meshD:getBone(i) end)
        if okG and name then
            dpCall(function()
                return meshD:updateBone(i, name, parentName, x * sx, y * sy, z * sz, (radius or 0.05) * radiusScale,
                    rotX, rotY, rotZ, scaleX * sx, scaleY * sy, scaleZ * sz, length * radiusScale)
            end)
        end
    end
end

local function applyTranslateToBones(meshD, dx, dy, dz)
    local okTotal, nBones = dpCall(function() return meshD:getTotalBone() end)
    nBones = (okTotal and nBones) or 0
    for i = 1, nBones do
        local okG, name, x, y, z, radius, parentName, rotX, rotY, rotZ, scaleX, scaleY, scaleZ, length =
            dpCall(function() return meshD:getBone(i) end)
        if okG and name then
            dpCall(function()
                return meshD:updateBone(i, name, parentName, x + dx, y + dy, z + dz, radius,
                    rotX, rotY, rotZ, scaleX, scaleY, scaleZ, length)
            end)
        end
    end
end

local BONE_GIZMO_COLOR = {1, 0, 1, 0.85}
local BONE_HIGHLIGHT_COLOR = {1, 1, 0, 0.95}
-- The Roll field (showBonesWindow) has no visual reference of its own -- this is that reference,
-- a thin rod along the bone's CURRENT decoded roll (local Z) axis, shown only for a highlighted
-- bone (direct user request) so checking Highlight is the one action that answers "which way does
-- Roll actually point" instead of the number alone.
local BONE_ROLL_AXIS_COLOR = {0, 1, 1, 1}

-- DEVICE::addRenderizable (device-common.cpp) silently overwrites a 3D object's own position.z with
-- an ever-incrementing internal "z order control" counter whenever that object is created with z
-- exactly 0.0 -- a convenience for objects that don't care about their own depth, not something a
-- deliberately-positioned 3D gizmo wants. Humanoid rigs routinely have several bones sitting exactly
-- on the character's own sagittal (z=0) plane (root, hips, spine, chest, head...), so without this
-- guard those spheres/links silently drift further along +Z on every single gizmo rebuild --
-- confirmed via a real headless test: repeated rebuilds of the same static skeleton data moved
-- root->hips by +0.01 world units per call, with nothing in this file's own logic touching position
-- at all. This is exactly the "tail keeps moving toward +Z" reported once Highlight started
-- triggering a full rebuild on every checkbox click. Nudging any exactly-zero Z off of 0.0 by a
-- visually negligible epsilon keeps the auto-order path from ever firing.
local function dodgeAutoZOrder(z)
    return (z == 0) and 0.0001 or z
end

-- Standard analytic ray-sphere intersection (dir must be normalized, as mbm.getPickRay's is).
-- Returns true + hit distance along the ray, or false. Ported from physic_editor.lua (which
-- itself ported this from THIS file's own removed bone-gizmo drag code, git show 78958a2) -- now
-- used again here for bone-sphere hit-testing, now that the underlying mbm.getPickRay/
-- camera.scaleScreen2d bug that caused the original removal is fixed (MBM_VERSION 6.31.9).
local function raySphereHit(ox, oy, oz, dx, dy, dz, cx, cy, cz, radius)
    local lx, ly, lz = cx - ox, cy - oy, cz - oz
    local tca = lx * dx + ly * dy + lz * dz
    if tca < 0 then return false end
    local d2 = lx * lx + ly * ly + lz * lz - tca * tca
    local r2 = radius * radius
    if d2 > r2 then return false end
    local thc = math.sqrt(r2 - d2)
    return true, tca - thc
end

-- Intersects the pick ray through (sx,sy) with a fixed plane. Ported from physic_editor.lua
-- (same provenance as raySphereHit above). Used here with a fixed WORLD-axis plane (constant Z
-- for X/Y-locked bone dragging, constant X for Z/Y-locked dragging) rather than a camera-facing
-- billboard plane, so the drag stays exactly axis-locked regardless of camera angle.
local function rayPlaneHit(sx, sy, planePt, planeNormal)
    local okRay, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, sx, sy)
    if not okRay then return nil end
    local denom = planeNormal.x * dx + planeNormal.y * dy + planeNormal.z * dz
    if math.abs(denom) < 1e-6 then return nil end
    local t = ((planePt.x - ox) * planeNormal.x + (planePt.y - oy) * planeNormal.y + (planePt.z - oz) * planeNormal.z) / denom
    if t < 0 then return nil end
    return ox + dx * t, oy + dy * t, oz + dz * t
end

-- Nearest-hit-wins ray-sphere test against every bone's own gizmo sphere (tEntry.tBoneGizmo.spheres),
-- for the 3D drag/drop bone editor (Bones node's "Drag Drop Joint/Bone X/Y" / "Z/Y" checkboxes).
-- Each sphere's world radius is read directly off its own scale (unitSphereVerts() is a radius-1
-- sphere, so h:getScale().x IS the true world hit-test radius, matching how rebuildBoneGizmo set it) --
-- no separate parallel radius table needed.
local function hitTestBoneSpheres3d(tEntry, sx, sy)
    if not tEntry.tBoneGizmo or not tEntry.tBoneGizmo.spheres then return nil end
    local okRay, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, sx, sy)
    if not okRay then return nil end
    local bestName, bestDist = nil, math.huge
    for name, h in pairs(tEntry.tBoneGizmo.spheres) do
        local p, s = h:getPos(), h:getScale()
        local hit, dist = raySphereHit(ox, oy, oz, dx, dy, dz, p.x, p.y, p.z, s.x)
        if hit and dist < bestDist then
            bestName, bestDist = name, dist
        end
    end
    if not bestName then return nil end
    local p = tEntry.tBoneGizmo.spheres[bestName]:getPos()
    return bestName, p.x, p.y, p.z
end

-- Snaps tEntry's own 3D orbit camera to a canonical straight-on view matching the just-checked
-- drag plane -- azimuth=0/elevation=0 puts the camera on +Z looking down -Z (screen X/Y = world
-- X/Y); azimuth=pi/2 puts it on +X looking down -X (screen X/Y = world Z/Y). A ONE-TIME snap on
-- check (per direct user request), not a hard lock -- the user can still orbit away afterward by
-- dragging empty space; the drag math itself stays correct (fixed world-axis plane) regardless of
-- camera angle.
local function snapCam3dToDragPlane(tEntry, mode)
    local c = tEntry.cam3d
    c.azimuth   = (mode == 'xy') and 0 or (math.pi * 0.5)
    c.elevation = 0
    if bCameraMode3D then applyCam3d(c) end
end

local function destroyBoneGizmo(tEntry)
    if tEntry.tBoneGizmo then
        for _, h in pairs(tEntry.tBoneGizmo.spheres) do h:destroy() end
        for _, link in ipairs(tEntry.tBoneGizmo.bones) do link.handle:destroy() end
        for _, r in ipairs(tEntry.tBoneGizmo.rollAxes or {}) do r:destroy() end
    end
    tEntry.tBoneGizmo = { spheres = {}, bones = {}, rollAxes = {} }
end

-- Full gizmo rebuild: called whenever the bone list itself changes (add/remove/reparent/rename),
-- the Bones node opens/closes/selection changes, or a 3D drag/drop edit commits a new bone
-- position (onTouchMove, via onBonesEdit -- same call path the DragFloat X/Y/Z fields already use
-- on every frame while being click-dragged in ImGui, so a full rebuild on every 3D-viewport drag
-- frame too is consistent with existing, already-shipped behavior for this exact mutation).
function rebuildBoneGizmo(tEntry, meshD, index)
    destroyBoneGizmo(tEntry)
    if tEntry.sOpenNode ~= 'bones' or index ~= iSelectedMeshIndex then return end

    local okTotal, nBones = dpCall(function() return meshD:getTotalBone() end)
    nBones = (okTotal and nBones) or 0
    if nBones == 0 then return end

    local tBones = {}
    for i = 1, nBones do
        local okG, name, x, y, z, radius, parentName, rotX, rotY, rotZ = dpCall(function() return meshD:getBone(i) end)
        if okG and name then
            local wx, wy, wz = boneToWorld(meshD, x, y, z)
            tBones[name] = { wx = wx, wy = wy, wz = wz, radius = radius or 1, parentName = parentName,
                rotX = rotX or 0, rotY = rotY or 0, rotZ = rotZ or 0 }
        end
    end

    local tHighlight = tEntry.tBoneHighlight or {}

    for name, b in pairs(tBones) do
        local h = shape:new('3d', b.wx, b.wy, dodgeAutoZOrder(b.wz))
        -- Unique nickname per instance, NOT a fixed shared one, despite every sphere using
        -- byte-identical unitSphereVerts() data. shape:create()'s nickName is a MESH_MANAGER cache
        -- key for the underlying MESH_MBM resource itself (SHAPE_MESH::load, shape-mesh.cpp:859) --
        -- every SHAPE_MESH instance loaded with the SAME nickname shares that ONE MESH_MBM object,
        -- and setColor() (onSetTextureAnimationLua, animation-lua.cpp:273) sets the DIFFUSE TEXTURE
        -- on renderizable->getMesh(), i.e. on that SAME shared object -- not a per-instance
        -- material/uniform. Confirmed by a real headless reproduction: two shape spheres created
        -- with a shared nickname and different setColor() calls both read back the SAME (the
        -- LAST-set) texture via getMaterialTexture(); with unique nicknames each kept its own. This
        -- was invisible before Highlight existed (every sphere always set the identical magenta), and
        -- was exactly the "checking one bone highlights every joint" bug reported by the user --
        -- whichever bone's setColor() happened to run last in this pairs() iteration silently
        -- determined the color of the whole shared mesh, hence all 23 spheres.
        tEntry.iBoneGizmoGen = (tEntry.iBoneGizmoGen or 0) + 1
        h:create(unitSphereVerts(), nil, 'mesh_debug_bone_sphere_' .. index .. '_' .. tEntry.iBoneGizmoGen)
        h:setScale(b.radius, b.radius, b.radius)
        local c = tHighlight[name] and BONE_HIGHLIGHT_COLOR or BONE_GIZMO_COLOR
        h:setColor(c[1], c[2], c[3], c[4])
        tEntry.tBoneGizmo.spheres[name] = h

        -- Roll-axis reference rod (direct user request: the Roll field had no visual meaning) --
        -- only for a highlighted bone, a thin cylinder from this bone's own position along its
        -- CURRENT decoded roll (local Z) axis, the same eulerToBoneFrame this whole feature already
        -- decodes rotX/Y/Z with. rotX/Y/Z are stored in the same space x,y,z are (see
        -- eulerToBoneFrame's own comment), and boneToWorld is presently an identity passthrough
        -- (bone-local coords already equal world coords), so the direction needs no extra transform
        -- beyond what positions above already went through.
        if tHighlight[name] then
            local _, _, _, zx, zy, zz = eulerToBoneFrame(b.rotX, b.rotY, b.rotZ)
            local rodLen = b.radius * 5
            tEntry.iBoneGizmoGen = (tEntry.iBoneGizmoGen or 0) + 1
            local rod = shape:new('3d', b.wx, b.wy, dodgeAutoZOrder(b.wz))
            rod:create(orientedCylinderVerts(zx * rodLen, zy * rodLen, zz * rodLen, b.radius * 0.15, b.radius * 0.15, 8),
                nil, 'mesh_debug_bone_roll_' .. index .. '_' .. tEntry.iBoneGizmoGen)
            rod:setColor(BONE_ROLL_AXIS_COLOR[1], BONE_ROLL_AXIS_COLOR[2], BONE_ROLL_AXIS_COLOR[3], BONE_ROLL_AXIS_COLOR[4])
            table.insert(tEntry.tBoneGizmo.rollAxes, rod)
        end
    end

    for name, b in pairs(tBones) do
        local parent = b.parentName and tBones[b.parentName]
        if parent then
            local dx, dy, dz = b.wx - parent.wx, b.wy - parent.wy, b.wz - parent.wz
            local height = math.sqrt(dx * dx + dy * dy + dz * dz)
            if height > 0.001 then
                local h = shape:new('3d', parent.wx, parent.wy, dodgeAutoZOrder(parent.wz))
                -- Unique nickname each rebuild for the same reason as the spheres above (color
                -- isolation, not just avoiding stale geometry reuse as originally noted here).
                tEntry.iBoneGizmoGen = (tEntry.iBoneGizmoGen or 0) + 1
                local nick = 'mesh_debug_bone_link_' .. index .. '_' .. tEntry.iBoneGizmoGen
                h:create(orientedCylinderVerts(dx, dy, dz, b.radius * 0.5, parent.radius * 0.5, 8), nil, nick)
                -- No setAngle needed -- orientedCylinderVerts already bakes the parent->child
                -- direction directly into the geometry, correct for any dx/dy/dz combination.
                -- Colored by the CHILD bone's own highlight state (name here) -- this link visually
                -- represents "the bone ending at this joint," not its parent.
                local c = tHighlight[name] and BONE_HIGHLIGHT_COLOR or BONE_GIZMO_COLOR
                h:setColor(c[1], c[2], c[3], c[4])
                table.insert(tEntry.tBoneGizmo.bones, { handle = h, childName = name })
            end
        end
    end
end

-- Outlined preview of the mesh being rigged, shown alongside the bone gizmo (per direct user
-- request -- with tPreviewMesh hidden while Bones is open, the armature had nothing to align
-- against). Deliberately a SEPARATE mesh instance from tPreviewMesh, never the same object: applying
-- a shader is a real (if reversible) mutation of the object's FX state, and the user explicitly
-- asked for a dedicated outline object rather than reusing/toggling the live preview's own shader.
-- Uses the engine's built-in opaque outline.ps/outline.vs pair (shipped on every backend). The
-- pair discards all fragments except surfaces nearly tangent to the camera, NOT obj:setColor():
-- setColor(r,g,b,a) with numeric args replaces the mesh's real diffuse texture with a synthetic
-- solid-color one (see showBonesNode's own comment on tPreviewMesh above) -- destructive on any
-- real textured mesh, which is exactly what the ghost mesh is.
function destroyGhostMesh()
    if tGhostMesh then
        tGhostMesh.tFont = nil
        tGhostMesh:destroy()
        tGhostMesh = nil
    end
end

local function applyGhostOutlineSettings(ghost, tEntry)
    local okSh, fx = pcall(function() return ghost:getShader() end)
    if not okSh or not fx then return end
    local color = tEntry.tGhostOutlineColor or {r = 1.0, g = 0.9, b = 0.1}
    fx:setPS('color', color.r, color.g, color.b)
    fx:setPS('thickness', tEntry.fGhostOutlineThickness or 0.12)
end

-- Only meaningful for 'mesh' (.msh) entries -- the type this Bones/armature feature targets and the
-- only one that renders in 3D world space (isMesh3DType). Mirrors updatePreviewMesh's own mesh:new +
-- load + setPos(0,0,0) pattern (same z-order-auto-nudge counteraction, see updatePreviewMesh's
-- comment) so the ghost lines up with the gizmo exactly like the hidden live preview would have.
function rebuildGhostMesh(tEntry, index)
    destroyGhostMesh()
    if index ~= iSelectedMeshIndex or not isMesh3DType(tEntry) then return end

    local loadPath = tEntry.previewPath or tEntry.fileName
    local dir = tEntry.fileName:match('^(.*)[/\\]')
    if dir then mbm.addPath(dir) end

    local coordType = bCameraMode3D and '3d' or '2dw'
    local newGhost = mesh:new(coordType)
    if not newGhost:load(loadPath) then
        newGhost:destroy()
        return
    end
    newGhost:setPos(0, 0, 0)

    local okSh, fx = pcall(function() return newGhost:getShader() end)
    if okSh and fx then
        if fx:load('outline.ps', 'outline.vs') then
            applyGhostOutlineSettings(newGhost, tEntry)
        else
            print('mesh_debug: ghost mesh shader failed to load')
        end
    end
    tGhostMesh = newGhost
end

-- ---------------------------------------------------------------------------
-- Raw vertex AABB helper. Armature/template callers use the default frame 1/all-subsets scope;
-- transform centralization can optionally select a different frame/subset anchor.
-- ---------------------------------------------------------------------------

-- Reads raw vertex data -- not meshD:getPhysics(), whose configured bounds can be absent/stale.
-- targetFrame defaults to frame 1; targetSubset defaults to every subset.
local function computeMeshAABB(meshD, targetFrame, targetSubset)
    targetFrame = targetFrame or 1
    targetSubset = targetSubset or 0
    local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(targetFrame) end)
    nSubsets = (okS and nSubsets) or 0
    local minX, minY, minZ, maxX, maxY, maxZ = nil, nil, nil, nil, nil, nil
    local firstSubset = targetSubset > 0 and targetSubset or 1
    local lastSubset = targetSubset > 0 and targetSubset or nSubsets
    if firstSubset > nSubsets then return nil end
    for s = firstSubset, lastSubset do
        local okV, nVerts = dpCall(function() return meshD:getTotalVertex(targetFrame, s) end)
        nVerts = (okV and nVerts) or 0
        for v = 1, nVerts do
            local okG, vert = dpCall(function() return meshD:getVertex(targetFrame, s, v) end)
            if okG and vert then
                local x, y, z = vert.x, vert.y, vert.z
                minX = (not minX or x < minX) and x or minX
                maxX = (not maxX or x > maxX) and x or maxX
                minY = (not minY or y < minY) and y or minY
                maxY = (not maxY or y > maxY) and y or maxY
                minZ = (not minZ or z < minZ) and z or minZ
                maxZ = (not maxZ or z > maxZ) and z or maxZ
            end
        end
    end
    if not minX then return nil end
    return { minX = minX, minY = minY, minZ = minZ, maxX = maxX, maxY = maxY, maxZ = maxZ }
end

function computeExactAxisScale(currentSize, targetSize, axis)
    currentSize = tonumber(currentSize) or 0
    targetSize = tonumber(targetSize) or 0
    if currentSize <= 1e-7 or targetSize <= 0 then return nil end
    local factor = targetSize / currentSize
    if axis == 'X' then return factor, 1, 1 end
    if axis == 'Y' then return 1, factor, 1 end
    if axis == 'Z' then return 1, 1, factor end
    return nil
end

function getTransformBounds(tEntry, meshD, frame, subset)
    local referenceFrame = math.max(1, frame or 0)
    subset = subset or 0
    local key = referenceFrame .. ':' .. subset
    if tEntry.tTransformBoundsCache and tEntry.tTransformBoundsCache.key == key then
        return tEntry.tTransformBoundsCache.bounds
    end
    local aabb = computeMeshAABB(meshD, referenceFrame, subset)
    local bounds = nil
    if aabb then
        bounds = {
            width = aabb.maxX - aabb.minX,
            height = aabb.maxY - aabb.minY,
            depth = aabb.maxZ - aabb.minZ,
        }
    end
    tEntry.tTransformBoundsCache = {key = key, bounds = bounds}
    return bounds
end

function destroyTransformSubsetHoverMarker(tEntry, owner)
    if owner and tEntry.sTransformSubsetHoverOwner ~= owner then return end
    if tEntry.tTransformSubsetHoverMarker then
        tEntry.tTransformSubsetHoverMarker:destroy()
        tEntry.tTransformSubsetHoverMarker = nil
    end
    tEntry.sTransformSubsetHoverCoordType = nil
    tEntry.sTransformSubsetHoverOwner = nil
end

-- Shows a hovered table-row subset independently of the preview filter. The marker is derived
-- from raw vertices, so it still exists when that subset is unchecked, and alwaysOnTop keeps it
-- readable through the mesh regardless of the current orbit angle.
function updateTransformSubsetHoverMarker(tEntry, meshD, frame, subset, xf, index, owner)
    owner = owner or 'transform'
    local bounds = computeMeshAABB(meshD, frame, subset)
    if not bounds then
        destroyTransformSubsetHoverMarker(tEntry, owner)
        return
    end
    local cx = (bounds.minX + bounds.maxX) * 0.5
    local cy = (bounds.minY + bounds.maxY) * 0.5
    local cz = (bounds.minZ + bounds.maxZ) * 0.5
    if xf.rx ~= 0 then cx, cy, cz = rotateX(cx, cy, cz, math.rad(xf.rx)) end
    if xf.ry ~= 0 then cx, cy, cz = rotateY(cx, cy, cz, math.rad(xf.ry)) end
    if xf.rz ~= 0 then cx, cy, cz = rotateZ(cx, cy, cz, math.rad(xf.rz)) end
    cx, cy, cz = cx * xf.sx + xf.dx, cy * xf.sy + xf.dy, cz * xf.sz + xf.dz

    local extent = math.max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY,
        bounds.maxZ - bounds.minZ)
    local markerSize = math.max(extent * 0.06, 0.05)
    local coordType = bCameraMode3D and '3d' or '2dw'
    if tEntry.tTransformSubsetHoverMarker and
            (tEntry.sTransformSubsetHoverCoordType ~= coordType or tEntry.sTransformSubsetHoverOwner ~= owner) then
        destroyTransformSubsetHoverMarker(tEntry)
    end
    if not tEntry.tTransformSubsetHoverMarker then
        tEntry.iTransformSubsetHoverGeneration = (tEntry.iTransformSubsetHoverGeneration or 0) + 1
        local marker = shape:new(coordType, cx, cy, dodgeAutoZOrder(cz))
        marker:create(unitSphereVerts(8, 12), nil, 'mesh_debug_subset_hover_' .. index .. '_' ..
            tEntry.iTransformSubsetHoverGeneration)
        marker:setColor(0.1, 1.0, 1.0, 0.95)
        marker.alwaysOnTop = true
        tEntry.tTransformSubsetHoverMarker = marker
        tEntry.sTransformSubsetHoverCoordType = coordType
        tEntry.sTransformSubsetHoverOwner = owner
    else
        tEntry.tTransformSubsetHoverMarker:setPos(cx, cy, dodgeAutoZOrder(cz))
    end
    tEntry.tTransformSubsetHoverMarker:setScale(markerSize, markerSize, markerSize)
    tEntry.tTransformSubsetHoverMarker.visible = true
end

-- Reproduces MESH_MBM_DEBUG::centralizeFrame's exact offset formula given the selected anchor
-- AABB (computeMeshAABB's shape), so bones can be translated by the identical delta
-- meshD:centralize() bakes into vertices (every vertex gets `pos -= offset`).
-- Replicates centralizeFrame's near-zero-crossing tolerance heuristic verbatim (if an axis's min/max
-- are already nearly symmetric about zero, treat that axis as already centered rather than
-- re-deriving from dist*0.5) rather than approximating with a plain AABB-center formula -- including
-- its 0/0 = NaN edge case when both min and max are exactly 0 on an axis (NaN < 0.001 is false in
-- both C++ and Lua's IEEE-754 doubles, so that branch naturally falls through the same way without
-- needing a special case here).
local function computeCentralizeOffset(aabb)
    local minX, minY, minZ = aabb.minX, aabb.minY, aabb.minZ
    local maxX, maxY, maxZ = aabb.maxX, aabb.maxY, aabb.maxZ
    local distX, distY, distZ = maxX - minX, maxY - minY, maxZ - minZ

    local xDif, yDif, zDif = math.abs(maxX), math.abs(maxY), math.abs(maxZ)
    local xDiff, yDiff, zDiff = math.abs(minX), math.abs(minY), math.abs(minZ)
    local xMin, xMax = math.min(xDiff, xDif), math.max(xDiff, xDif)
    local yMin, yMax = math.min(yDiff, yDif), math.max(yDiff, yDif)
    local zMin, zMax = math.min(zDiff, zDif), math.max(zDiff, zDif)

    local xDiv, yDiv, zDiv = xMin / xMax, yMin / yMax, zMin / zMax
    if xDiv < 0.001 then distX = xMin; minX = 0 end
    if yDiv < 0.001 then distY = yMin; minY = 0 end
    if zDiv < 0.001 then distZ = zMin; minZ = 0 end

    local midX, midY, midZ = distX * 0.5, distY * 0.5, distZ * 0.5
    return minX + midX, minY + midY, minZ + midZ
end

-- Real armature templates, each a full bone hierarchy (name/parent/x,y,z/radius/rotX,Y,Z/
-- scaleX,Y,Z/length -- the complete SKELETON_BONE_V11 field set, see header-mesh.h) extracted
-- verbatim from a real, working rig -- not a hand-approximated fraction-based placement like the
-- 18-joint HUMANOID_TEMPLATE this replaces. referenceAABB is that same source mesh's own frame-1
-- AABB (computeMeshAABB's shape), captured alongside the bones so applyArmatureTemplate can fit
-- this real rig onto an arbitrary target mesh by UNIFORM scale + reposition only -- never
-- anisotropic (per-axis) stretching, which would corrupt rotX/Y/Z (a real 3D direction, not
-- meaningful under non-uniform scaling) the way it never could for the old fraction template
-- (which carried no orientation data to begin with).
--
-- ARMATURE_STANDARD_SKELETON_65 was extracted from tests/mike-rig-from-mixamo.msh: a real
-- character auto-rigged by Mixamo's own web tool (chin/wrists/elbows/knees/groin markers +
-- "Standard Skeleton (65)" preset), downloaded in T-pose, and round-tripped through this engine's
-- own Blender import. Only 33 bones came through (Mixamo's own web UI still calls the preset
-- "Standard Skeleton (65)" regardless of how many of those 65 the specific download actually
-- populates -- this rig has no per-finger detail past the index finger) -- the label below matches
-- Mixamo's own preset name, not a literal bone count.
local ARMATURE_STANDARD_SKELETON_65 = {
    label = 'Standard Skeleton (65)',
    referenceAABB = { minX=-0.953590, minY=-0.000157, minZ=-0.163369, maxX=0.952638, maxY=1.913079, maxZ=0.239427 },
    bones = {
        { name = 'mixamorig:Hips', parent = nil, x=0.001954, y=1.018964, z=-0.027472, radius=0.017810, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.118735 },
        { name = 'mixamorig:Spine', parent = 'mixamorig:Hips', x=0.001954, y=1.126944, z=-0.036337, radius=0.018960, rotX=-4.693449, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.126400 },
        { name = 'mixamorig:LeftUpLeg', parent = 'mixamorig:Hips', x=0.110291, y=0.958871, z=-0.030830, radius=0.062366, rotX=-3.047297, rotY=0.360019, rotZ=-173.265015, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.415772 },
        { name = 'mixamorig:RightUpLeg', parent = 'mixamorig:Hips', x=-0.106384, y=0.958871, z=-0.030655, radius=0.062368, rotX=-3.077373, rotY=-0.363550, rotZ=173.265381, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.415784 },
        { name = 'mixamorig:Spine1', parent = 'mixamorig:Spine', x=0.001954, y=1.252921, z=-0.046680, radius=0.021669, rotX=-4.693448, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.144458 },
        { name = 'mixamorig:LeftLeg', parent = 'mixamorig:LeftUpLeg', x=0.159121, y=0.546568, z=-0.052932, radius=0.060870, rotX=-2.687390, rotY=0.407074, rotZ=-172.385559, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.405798 },
        { name = 'mixamorig:RightLeg', parent = 'mixamorig:RightUpLeg', x=-0.155213, y=0.546568, z=-0.052976, radius=0.060875, rotX=-2.792445, rotY=-0.410901, rotZ=172.386490, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.405834 },
        { name = 'mixamorig:Spine2', parent = 'mixamorig:Spine1', x=0.001954, y=1.396894, z=-0.058500, radius=0.024393, rotX=-4.693454, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.162619 },
        { name = 'mixamorig:LeftFoot', parent = 'mixamorig:LeftLeg', x=0.212967, y=0.144808, z=-0.071958, radius=0.032511, rotX=51.206329, rotY=-9.083311, rotZ=-171.936569, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.216739 },
        { name = 'mixamorig:RightFoot', parent = 'mixamorig:RightLeg', x=-0.209059, y=0.144808, z=-0.072746, radius=0.032943, rotX=51.806446, rotY=8.882802, rotZ=171.929749, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.219620 },
        { name = 'mixamorig:Neck', parent = 'mixamorig:Spine2', x=0.001954, y=1.558864, z=-0.071798, radius=0.008050, rotX=0.000000, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.053668 },
        { name = 'mixamorig:LeftShoulder', parent = 'mixamorig:Spine2', x=0.075299, y=1.541482, z=-0.071794, radius=0.022655, rotX=0.029381, rotY=85.177055, rotZ=-103.269150, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151035 },
        { name = 'mixamorig:RightShoulder', parent = 'mixamorig:Spine2', x=-0.071392, y=1.541482, z=-0.071801, radius=0.022655, rotX=-0.029374, rotY=-85.175842, rotZ=103.327698, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151035 },
        { name = 'mixamorig:LeftToeBase', parent = 'mixamorig:LeftFoot', x=0.258419, y=0.014100, z=0.094852, radius=0.013121, rotX=91.257622, rotY=-13.754485, rotZ=-177.986542, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.087472 },
        { name = 'mixamorig:RightToeBase', parent = 'mixamorig:RightFoot', x=-0.254512, y=0.014099, z=0.097788, radius=0.013472, rotX=91.201202, rotY=13.391500, rotZ=177.886215, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.089811 },
        { name = 'mixamorig:Head', parent = 'mixamorig:Neck', x=0.001954, y=1.609482, z=-0.053962, radius=0.046270, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.308468 },
        { name = 'mixamorig:LeftArm', parent = 'mixamorig:LeftShoulder', x=0.222285, y=1.506740, z=-0.071788, radius=0.040572, rotX=9.747616, rotY=84.915649, rotZ=-84.800674, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.270482 },
        { name = 'mixamorig:RightArm', parent = 'mixamorig:RightShoulder', x=-0.218377, y=1.506740, z=-0.071807, radius=0.040571, rotX=7.950571, rotY=-84.964287, rotZ=86.590340, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.270472 },
        { name = 'mixamorig:LeftToe_End', parent = 'mixamorig:LeftToeBase', x=0.279131, y=0.016750, z=0.179795, radius=0.013121, rotX=91.257614, rotY=-13.754465, rotZ=-177.986481, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.087472 },
        { name = 'mixamorig:RightToe_End', parent = 'mixamorig:RightToeBase', x=-0.275224, y=0.016748, z=0.185138, radius=0.013472, rotX=91.201180, rotY=13.391499, rotZ=177.886154, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.089811 },
        { name = 'mixamorig:HeadTop_End', parent = 'mixamorig:Head', x=0.001954, y=1.900418, z=0.048550, radius=0.046270, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.308468 },
        { name = 'mixamorig:LeftForeArm', parent = 'mixamorig:LeftArm', x=0.491899, y=1.485470, z=-0.067729, radius=0.033055, rotX=1.181309, rotY=84.945183, rotZ=-86.996162, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.220369 },
        { name = 'mixamorig:RightForeArm', parent = 'mixamorig:RightArm', x=-0.487991, y=1.485471, z=-0.068524, radius=0.033057, rotX=-5.899307, rotY=-85.012611, rotZ=94.050064, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.220377 },
        { name = 'mixamorig:LeftHand', parent = 'mixamorig:LeftForeArm', x=0.712155, y=1.492497, z=-0.067329, radius=0.020135, rotX=53.151276, rotY=82.267815, rotZ=-38.823112, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.134235 },
        { name = 'mixamorig:RightHand', parent = 'mixamorig:RightForeArm', x=-0.708247, y=1.492497, z=-0.070493, radius=0.023043, rotX=62.126709, rotY=-80.576805, rotZ=29.662706, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.153618 },
        { name = 'mixamorig:LeftHandIndex1', parent = 'mixamorig:LeftHand', x=0.845549, y=1.488484, z=-0.052877, radius=0.004506, rotX=-75.147537, rotY=75.644257, rotZ=-163.785980, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030040 },
        { name = 'mixamorig:RightHandIndex1', parent = 'mixamorig:RightHand', x=-0.860198, y=1.488607, z=-0.048260, radius=0.003849, rotX=7.909595, rotY=-85.458687, rotZ=77.243103, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.025658 },
        { name = 'mixamorig:LeftHandIndex2', parent = 'mixamorig:LeftHandIndex1', x=0.874710, y=1.488945, z=-0.060076, radius=0.006490, rotX=-75.147240, rotY=75.644264, rotZ=-163.786072, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.043267 },
        { name = 'mixamorig:RightHandIndex2', parent = 'mixamorig:RightHandIndex1', x=-0.885762, y=1.490785, z=-0.047980, radius=0.005510, rotX=7.908145, rotY=-85.458572, rotZ=77.243698, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.036731 },
        { name = 'mixamorig:LeftHandIndex3', parent = 'mixamorig:LeftHandIndex2', x=0.916940, y=1.486856, z=-0.050896, radius=0.004576, rotX=-75.147484, rotY=75.644241, rotZ=-163.786026, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030509 },
        { name = 'mixamorig:RightHandIndex3', parent = 'mixamorig:RightHandIndex2', x=-0.920641, y=1.483321, z=-0.039209, radius=0.003606, rotX=7.908519, rotY=-85.458641, rotZ=77.243500, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.024040 },
        { name = 'mixamorig:LeftHandIndex4', parent = 'mixamorig:LeftHandIndex3', x=0.945577, y=1.482247, z=-0.041434, radius=0.004576, rotX=-75.147255, rotY=75.644241, rotZ=-163.786026, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030510 },
        { name = 'mixamorig:RightHandIndex4', parent = 'mixamorig:RightHandIndex3', x=-0.944469, y=1.481889, z=-0.036367, radius=0.003606, rotX=7.908629, rotY=-85.458641, rotZ=77.243385, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.024040 },
    },
}
local ARMATURE_3_CHAIN_FINGERS = {
    label = '3 Chain Fingers (49)',
    referenceAABB = { minX=-0.740790, minY=-0.000114, minZ=-0.151165, maxX=0.748511, maxY=1.822469, maxZ=0.208525 },
    bones = {
        { name = "mixamorig:Hips", parent = nil, x=0.000000, y=0.926227, z=-0.028060, radius=0.016094, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.107293 },
        { name = "mixamorig:Spine", parent = "mixamorig:Hips", x=0.000000, y=1.039920, z=-0.026879, radius=0.019897, rotX=0.595406, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.132649 },
        { name = "mixamorig:LeftUpLeg", parent = "mixamorig:Hips", x=0.082740, y=0.863078, z=-0.029367, radius=0.056145, rotX=-2.653563, rotY=-0.070910, rotZ=178.470764, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.374301 },
        { name = "mixamorig:RightUpLeg", parent = "mixamorig:Hips", x=-0.082740, y=0.863078, z=-0.028519, radius=0.056171, rotX=-3.163926, rotY=0.084448, rotZ=-178.472153, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.374471 },
        { name = "mixamorig:Spine1", parent = "mixamorig:Spine", x=0.000000, y=1.172562, z=-0.025500, radius=0.022740, rotX=0.595406, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151599 },
        { name = "mixamorig:LeftLeg", parent = "mixamorig:LeftUpLeg", x=0.072740, y=0.489312, z=-0.046696, radius=0.058006, rotX=-0.079035, rotY=0.081460, rotZ=-178.241730, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.386709 },
        { name = "mixamorig:RightLeg", parent = "mixamorig:RightUpLeg", x=-0.072740, y=0.489312, z=-0.049187, radius=0.058006, rotX=0.019028, rotY=-0.097177, rotZ=178.241592, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.386709 },
        { name = "mixamorig:Spine2", parent = "mixamorig:Spine1", x=0.000000, y=1.324153, z=-0.023925, radius=0.024670, rotX=0.595406, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.164464 },
        { name = "mixamorig:LeftFoot", parent = "mixamorig:LeftLeg", x=0.084606, y=0.102785, z=-0.047229, radius=0.023382, rotX=50.269875, rotY=-15.468593, rotZ=-178.219757, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.155881 },
        { name = "mixamorig:RightFoot", parent = "mixamorig:RightLeg", x=-0.084606, y=0.102785, z=-0.049059, radius=0.022571, rotX=48.543678, rotY=16.496124, rotZ=178.247269, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.150471 },
        { name = "mixamorig:Neck", parent = "mixamorig:Spine2", x=0.000000, y=1.494692, z=-0.022152, radius=0.011283, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.075221 },
        { name = "mixamorig:LeftShoulder", parent = "mixamorig:Spine2", x=0.065546, y=1.471656, z=-0.022637, radius=0.021686, rotX=-143.193878, rotY=89.357948, rotZ=110.597511, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.144574 },
        { name = "mixamorig:RightShoulder", parent = "mixamorig:Spine2", x=-0.065546, y=1.471656, z=-0.021668, radius=0.021686, rotX=153.063095, rotY=-89.151520, rotZ=-46.858749, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.144574 },
        { name = "mixamorig:LeftToeBase", parent = "mixamorig:LeftFoot", x=0.119660, y=0.004191, z=0.068311, radius=0.009166, rotX=90.589684, rotY=-16.677780, rotZ=-179.745148, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061109 },
        { name = "mixamorig:RightToeBase", parent = "mixamorig:RightFoot", x=-0.119660, y=0.004192, z=0.059072, radius=0.008818, rotX=90.473701, rotY=17.362391, rotZ=179.277725, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058784 },
        { name = "mixamorig:Head", parent = "mixamorig:Neck", x=0.000000, y=1.569418, z=-0.013541, radius=0.037451, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.249673 },
        { name = "mixamorig:LeftArm", parent = "mixamorig:LeftShoulder", x=0.204372, y=1.431306, z=-0.023607, radius=0.026980, rotX=150.208664, rotY=89.433632, rotZ=48.879364, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.179865 },
        { name = "mixamorig:RightArm", parent = "mixamorig:RightShoulder", x=-0.204372, y=1.431306, z=-0.020698, radius=0.026981, rotX=140.853973, rotY=-89.084854, rotZ=-39.527004, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.179872 },
        { name = "mixamorig:LeftToe_End", parent = "mixamorig:LeftToeBase", x=0.137194, y=0.004898, z=0.126847, radius=0.009166, rotX=90.589691, rotY=-16.677790, rotZ=-179.745163, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061109 },
        { name = "mixamorig:RightToe_End", parent = "mixamorig:RightToeBase", x=-0.137194, y=0.004899, z=0.115175, radius=0.008818, rotX=90.473709, rotY=17.362394, rotZ=179.277710, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058784 },
        { name = "mixamorig:HeadTop_End", parent = "mixamorig:Head", x=0.000000, y=1.817449, z=0.015044, radius=0.037451, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.249673 },
        { name = "mixamorig:LeftForeArm", parent = "mixamorig:LeftArm", x=0.380729, y=1.395968, z=-0.022724, radius=0.036149, rotX=-107.850189, rotY=84.812241, rotZ=163.251862, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.240990 },
        { name = "mixamorig:RightForeArm", parent = "mixamorig:RightArm", x=-0.380729, y=1.395968, z=-0.018885, radius=0.036310, rotX=-107.620689, rotY=-82.328033, rotZ=-163.400940, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.242063 },
        { name = "mixamorig:LeftHand", parent = "mixamorig:LeftForeArm", x=0.620775, y=1.400874, z=-0.043465, radius=0.010102, rotX=23.350143, rotY=-64.426994, rotZ=-78.428665, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.067345 },
        { name = "mixamorig:RightHand", parent = "mixamorig:RightForeArm", x=-0.620775, y=1.400874, z=-0.049684, radius=0.009249, rotX=16.918432, rotY=0.721628, rotZ=88.736382, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061660 },
        { name = "mixamorig:LeftHandIndex1", parent = "mixamorig:LeftHand", x=0.676519, y=1.436865, z=-0.031943, radius=0.003127, rotX=-155.008316, rotY=46.454838, rotZ=112.320648, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.020848 },
        { name = "mixamorig:RightHandIndex1", parent = "mixamorig:RightHand", x=-0.679747, y=1.402401, z=-0.031742, radius=0.002619, rotX=-166.525299, rotY=14.309860, rotZ=-95.931923, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.017460 },
        { name = "mixamorig:LeftHandIndex2", parent = "mixamorig:LeftHandIndex1", x=0.696423, y=1.438135, z=-0.038011, radius=0.003453, rotX=-155.008545, rotY=46.454731, rotZ=112.320518, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.023023 },
        { name = "mixamorig:RightHandIndex2", parent = "mixamorig:RightHandIndex1", x=-0.696531, y=1.405156, z=-0.035684, radius=0.003295, rotX=-166.525467, rotY=14.310186, rotZ=-95.932716, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.021967 },
        { name = "mixamorig:LeftHandIndex3", parent = "mixamorig:LeftHandIndex2", x=0.717848, y=1.434085, z=-0.045402, radius=0.001695, rotX=-155.008423, rotY=46.454708, rotZ=112.320976, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.011303 },
        { name = "mixamorig:RightHandIndex3", parent = "mixamorig:RightHandIndex2", x=-0.718471, y=1.404729, z=-0.034694, radius=0.001817, rotX=-166.525497, rotY=14.310227, rotZ=-95.932892, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.012116 },
        { name = "mixamorig:LeftHandIndex4", parent = "mixamorig:LeftHandIndex3", x=0.728734, y=1.434607, z=-0.048399, radius=0.001695, rotX=-155.008545, rotY=46.454552, rotZ=112.321465, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.011303 },
        { name = "mixamorig:RightHandIndex4", parent = "mixamorig:RightHandIndex3", x=-0.730570, y=1.404833, z=-0.035324, radius=0.001817, rotX=-166.525482, rotY=14.310113, rotZ=-95.932343, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.012116 },
    },
}

local ARMATURE_2_CHAIN_FINGERS = {
    label = '2 Chain Fingers (41)',
    referenceAABB = { minX=-0.741990, minY=-0.000168, minZ=-0.151150, maxX=0.748067, maxY=1.821747, maxZ=0.208540 },
    bones = {
        { name = "mixamorig:Hips", parent = nil, x=0.000000, y=0.927429, z=-0.028792, radius=0.016085, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.107235 },
        { name = "mixamorig:Spine", parent = "mixamorig:Hips", x=0.000000, y=1.041066, z=-0.027515, radius=0.019888, rotX=0.643814, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.132585 },
        { name = "mixamorig:LeftUpLeg", parent = "mixamorig:Hips", x=0.082532, y=0.864099, z=-0.029326, radius=0.059681, rotX=-3.342882, rotY=-0.073730, rotZ=178.736511, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.397875 },
        { name = "mixamorig:RightUpLeg", parent = "mixamorig:Hips", x=-0.082532, y=0.864099, z=-0.028602, radius=0.059699, rotX=-3.620366, rotY=0.079793, rotZ=-178.737259, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.397992 },
        { name = "mixamorig:Spine1", parent = "mixamorig:Spine", x=0.000000, y=1.173643, z=-0.026025, radius=0.022729, rotX=0.643816, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151526 },
        { name = "mixamorig:LeftLeg", parent = "mixamorig:LeftUpLeg", x=0.073743, y=0.466998, z=-0.052527, radius=0.054645, rotX=0.851641, rotY=0.097370, rotZ=-178.333740, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.364302 },
        { name = "mixamorig:RightLeg", parent = "mixamorig:RightUpLeg", x=-0.073743, y=0.466998, z=-0.053734, radius=0.054644, rotX=0.735102, rotY=-0.105518, rotZ=178.333832, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.364292 },
        { name = "mixamorig:Spine2", parent = "mixamorig:Spine1", x=0.000000, y=1.325159, z=-0.024323, radius=0.024789, rotX=0.643816, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165261 },
        { name = "mixamorig:LeftFoot", parent = "mixamorig:LeftLeg", x=0.084326, y=0.102890, z=-0.047112, radius=0.023237, rotX=50.019455, rotY=-16.240631, rotZ=-178.583298, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.154912 },
        { name = "mixamorig:RightFoot", parent = "mixamorig:RightLeg", x=-0.084326, y=0.102890, z=-0.049060, radius=0.022601, rotX=48.646500, rotY=17.048336, rotZ=178.560593, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.150670 },
        { name = "mixamorig:Neck", parent = "mixamorig:Spine2", x=0.000000, y=1.495614, z=-0.022407, radius=0.010582, rotX=-0.000003, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.070544 },
        { name = "mixamorig:LeftShoulder", parent = "mixamorig:Spine2", x=0.066726, y=1.473488, z=-0.022656, radius=0.021072, rotX=-161.794159, rotY=89.351067, rotZ=91.020599, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.140482 },
        { name = "mixamorig:RightShoulder", parent = "mixamorig:Spine2", x=-0.066726, y=1.473488, z=-0.022159, radius=0.021072, rotX=164.733261, rotY=-89.229683, rotZ=-57.550373, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.140482 },
        { name = "mixamorig:LeftToeBase", parent = "mixamorig:LeftFoot", x=0.119975, y=0.004206, z=0.066855, radius=0.009153, rotX=90.030121, rotY=-17.883352, rotZ=179.345535, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061020 },
        { name = "mixamorig:RightToeBase", parent = "mixamorig:RightFoot", x=-0.119975, y=0.004206, z=0.059070, radius=0.008873, rotX=89.909355, rotY=18.468466, rotZ=-179.729736, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.059150 },
        { name = "mixamorig:Head", parent = "mixamorig:Neck", x=0.000000, y=1.565681, z=-0.014211, radius=0.038072, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.253814 },
        { name = "mixamorig:LeftArm", parent = "mixamorig:LeftShoulder", x=0.200936, y=1.431984, z=-0.023153, radius=0.030288, rotX=-133.360474, rotY=88.972885, rotZ=125.933090, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.201919 },
        { name = "mixamorig:RightArm", parent = "mixamorig:RightShoulder", x=-0.200936, y=1.431984, z=-0.021661, radius=0.030286, rotX=161.655945, rotY=-89.240746, rotZ=-60.955521, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.201904 },
        { name = "mixamorig:LeftToe_End", parent = "mixamorig:LeftToeBase", x=0.138712, y=0.004024, z=0.124926, radius=0.009153, rotX=90.030121, rotY=-17.883347, rotZ=179.345535, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061020 },
        { name = "mixamorig:RightToe_End", parent = "mixamorig:RightToeBase", x=-0.138712, y=0.004024, z=0.115174, radius=0.008872, rotX=89.909355, rotY=18.468479, rotZ=-179.729752, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.059150 },
        { name = "mixamorig:HeadTop_End", parent = "mixamorig:Head", x=0.000000, y=1.817776, z=0.015277, radius=0.038072, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.253814 },
        { name = "mixamorig:LeftForeArm", parent = "mixamorig:LeftArm", x=0.399326, y=1.394491, z=-0.025785, radius=0.026545, rotX=-110.919655, rotY=86.316376, rotZ=158.264435, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.176967 },
        { name = "mixamorig:RightForeArm", parent = "mixamorig:RightArm", x=-0.399326, y=1.394491, z=-0.020819, radius=0.026701, rotX=-105.398102, rotY=-82.650970, rotZ=-163.704132, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.178006 },
        { name = "mixamorig:LeftHand", parent = "mixamorig:LeftForeArm", x=0.575957, y=1.392098, z=-0.036406, radius=0.003586, rotX=-91.620926, rotY=-84.495583, rotZ=50.624012, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.023906 },
        { name = "mixamorig:RightHand", parent = "mixamorig:RightForeArm", x=-0.575957, y=1.392097, z=-0.042771, radius=0.004617, rotX=-85.864952, rotY=-78.748955, rotZ=-169.145660, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030777 },
        { name = "mixamorig:LeftHandIndex1", parent = "mixamorig:LeftHand", x=0.591570, y=1.410055, z=-0.038698, radius=0.006338, rotX=-40.247967, rotY=84.203163, rotZ=-112.600563, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.042254 },
        { name = "mixamorig:RightHandIndex1", parent = "mixamorig:RightHand", x=-0.605108, y=1.384248, z=-0.048761, radius=0.005585, rotX=15.452384, rotY=57.404198, rotZ=86.994911, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.037235 },
        { name = "mixamorig:LeftHandIndex2", parent = "mixamorig:LeftHandIndex1", x=0.631781, y=1.422736, z=-0.041455, radius=0.009662, rotX=-40.247978, rotY=84.203140, rotZ=-112.600677, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.064414 },
        { name = "mixamorig:RightHandIndex2", parent = "mixamorig:RightHandIndex1", x=-0.640509, y=1.394476, z=-0.043416, radius=0.008474, rotX=15.452312, rotY=57.404232, rotZ=86.994835, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.056496 },
        { name = "mixamorig:LeftHandIndex3", parent = "mixamorig:LeftHandIndex2", x=0.694163, y=1.438223, z=-0.037243, radius=0.005476, rotX=-40.247971, rotY=84.203194, rotZ=-112.600685, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.036505 },
        { name = "mixamorig:RightHandIndex3", parent = "mixamorig:RightHandIndex2", x=-0.695453, y=1.405216, z=-0.035825, radius=0.005269, rotX=15.452374, rotY=57.404217, rotZ=86.994804, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.035124 },
        { name = "mixamorig:LeftHandIndex4", parent = "mixamorig:LeftHandIndex3", x=0.728734, y=1.434608, z=-0.048399, radius=0.005476, rotX=-40.247986, rotY=84.203194, rotZ=-112.600700, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.036505 },
        { name = "mixamorig:RightHandIndex4", parent = "mixamorig:RightHandIndex3", x=-0.730571, y=1.404832, z=-0.035324, radius=0.005269, rotX=15.452349, rotY=57.404179, rotZ=86.994995, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.035124 },
    },
}
local ARMATURE_NO_FINGERS_25 = {
    label = 'No Fingers (25)',
    referenceAABB = { minX=-0.733316, minY=-0.000161, minZ=-0.151244, maxX=0.736469, maxY=1.822023, maxZ=0.208446 },
    bones = {
        { name = "mixamorig:Hips", parent = nil, x=0.000000, y=0.936117, z=-0.029040, radius=0.015899, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.105995 },
        { name = "mixamorig:Spine", parent = "mixamorig:Hips", x=0.000000, y=1.047633, z=-0.027799, radius=0.019516, rotX=0.637164, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.130110 },
        { name = "mixamorig:LeftUpLeg", parent = "mixamorig:Hips", x=0.082588, y=0.874185, z=-0.029391, radius=0.060107, rotX=-3.232387, rotY=-0.083342, rotZ=178.523285, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.400712 },
        { name = "mixamorig:RightUpLeg", parent = "mixamorig:Hips", x=-0.082588, y=0.874185, z=-0.028313, radius=0.060107, rotX=-3.232189, rotY=0.083352, rotZ=-178.523285, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.400712 },
        { name = "mixamorig:Spine1", parent = "mixamorig:Spine", x=0.000000, y=1.177734, z=-0.026352, radius=0.022304, rotX=0.637164, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.148697 },
        { name = "mixamorig:LeftLeg", parent = "mixamorig:LeftUpLeg", x=0.072245, y=0.474243, z=-0.051986, radius=0.055767, rotX=0.733447, rotY=0.107789, rotZ=-178.092239, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.371778 },
        { name = "mixamorig:RightLeg", parent = "mixamorig:RightUpLeg", x=-0.072245, y=0.474243, z=-0.050906, radius=0.055763, rotX=0.325850, rotY=-0.107725, rotZ=178.093002, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.371753 },
        { name = "mixamorig:Spine2", parent = "mixamorig:Spine1", x=0.000000, y=1.326421, z=-0.024699, radius=0.024506, rotX=0.637165, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.163376 },
        { name = "mixamorig:LeftFoot", parent = "mixamorig:LeftLeg", x=0.084611, y=0.102702, z=-0.047227, radius=0.023352, rotX=50.271675, rotY=-15.199527, rotZ=-178.292892, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.155678 },
        { name = "mixamorig:RightFoot", parent = "mixamorig:RightLeg", x=-0.084611, y=0.102702, z=-0.048792, radius=0.022597, rotX=48.633934, rotY=16.022581, rotZ=178.187195, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.150649 },
        { name = "mixamorig:Neck", parent = "mixamorig:Spine2", x=0.000000, y=1.493695, z=-0.022839, radius=0.008616, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.057438 },
        { name = "mixamorig:LeftShoulder", parent = "mixamorig:Spine2", x=0.068881, y=1.472398, z=-0.023094, radius=0.021457, rotX=-161.087799, rotY=89.368996, rotZ=90.200729, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.143049 },
        { name = "mixamorig:RightShoulder", parent = "mixamorig:Spine2", x=-0.068881, y=1.472398, z=-0.022583, radius=0.021457, rotX=164.397141, rotY=-89.239380, rotZ=-55.688076, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.143049 },
        { name = "mixamorig:LeftToeBase", parent = "mixamorig:LeftFoot", x=0.118952, y=0.004180, z=0.068314, radius=0.009197, rotX=90.956543, rotY=-17.296185, rotZ=179.044830, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061317 },
        { name = "mixamorig:RightToeBase", parent = "mixamorig:RightFoot", x=-0.118952, y=0.004180, z=0.059879, radius=0.008735, rotX=90.978867, rotY=18.245518, rotZ=-179.134781, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058232 },
        { name = "mixamorig:Head", parent = "mixamorig:Neck", x=0.000000, y=1.550730, z=-0.016052, radius=0.040315, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.268765 },
        { name = "mixamorig:LeftArm", parent = "mixamorig:LeftShoulder", x=0.204369, y=1.426510, z=-0.023605, radius=0.032483, rotX=-145.014679, rotY=89.161446, rotZ=115.907913, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.216552 },
        { name = "mixamorig:RightArm", parent = "mixamorig:RightShoulder", x=-0.204369, y=1.426510, z=-0.022073, radius=0.032482, rotX=173.098831, rotY=-89.266853, rotZ=-74.024925, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.216545 },
        { name = "mixamorig:LeftToe_End", parent = "mixamorig:LeftToeBase", x=0.137194, y=0.004900, z=0.126850, radius=0.009197, rotX=90.956543, rotY=-17.296190, rotZ=179.044830, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061317 },
        { name = "mixamorig:RightToe_End", parent = "mixamorig:RightToeBase", x=-0.137194, y=0.004899, z=0.115175, radius=0.008735, rotX=90.978851, rotY=18.245510, rotZ=-179.134796, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058232 },
        { name = "mixamorig:HeadTop_End", parent = "mixamorig:Head", x=0.000000, y=1.817613, z=0.015704, radius=0.040315, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.268765 },
        { name = "mixamorig:LeftForeArm", parent = "mixamorig:LeftArm", x=0.418203, y=1.392357, z=-0.025422, radius=0.028807, rotX=-108.689583, rotY=84.928093, rotZ=163.154465, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.192045 },
        { name = "mixamorig:RightForeArm", parent = "mixamorig:RightArm", x=-0.418203, y=1.392357, z=-0.021740, radius=0.029016, rotX=-105.569199, rotY=-81.288918, rotZ=-166.170456, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.193440 },
        { name = "mixamorig:LeftHand", parent = "mixamorig:LeftForeArm", x=0.609467, y=1.398743, z=-0.041504, radius=0.028807, rotX=-108.689728, rotY=84.928131, rotZ=163.154434, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.192045 },
        { name = "mixamorig:RightHand", parent = "mixamorig:RightForeArm", x=-0.609467, y=1.398744, z=-0.049962, radius=0.029016, rotX=-105.569382, rotY=-81.288940, rotZ=-166.170380, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.193440 },
    },
}

local ARMATURE_NO_FINGERS_23 = {
    label = 'No Fingers (23)',
    referenceAABB = { minX=-0.971253, minY=-0.000000, minZ=-0.829249, maxX=0.971253, maxY=2.136544, maxZ=0.566499 },
    bones = {
        { name = "root", parent = nil, x=0.000000, y=0.000000, z=0.000000, radius=0.060850, rotX=-0.000007, rotY=0.000014, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.405663 },
        { name = "hips", parent = "root", x=0.000000, y=0.405663, z=-0.000000, radius=0.030123, rotX=-0.000007, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.200821 },
        { name = "spine", parent = "hips", x=0.000000, y=0.597641, z=-0.000000, radius=0.056248, rotX=-0.000007, rotY=0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.374988 },
        { name = "upperleg.l", parent = "hips", x=0.170945, y=0.519251, z=-0.000000, radius=0.034062, rotX=178.008987, rotY=0.000009, rotZ=-0.000008, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.227077 },
        { name = "upperleg.r", parent = "hips", x=-0.170945, y=0.519251, z=-0.000000, radius=0.034062, rotX=178.008987, rotY=-0.000009, rotZ=0.000008, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.227077 },
        { name = "chest", parent = "spine", x=0.000000, y=0.972629, z=-0.000000, radius=0.038527, rotX=-0.000007, rotY=0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.256849 },
        { name = "lowerleg.l", parent = "upperleg.l", x=0.170945, y=0.292310, z=0.007889, radius=0.022416, rotX=-169.795273, rotY=-0.000064, rotZ=-0.000006, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.149437 },
        { name = "lowerleg.r", parent = "upperleg.r", x=-0.170945, y=0.292310, z=0.007889, radius=0.022416, rotX=-169.795273, rotY=0.000064, rotZ=0.000006, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.149437 },
        { name = "upperarm.l", parent = "chest", x=0.212007, y=1.106761, z=-0.000000, radius=0.036285, rotX=-90.000000, rotY=-86.715446, rotZ=-0.000001, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.241897 },
        { name = "upperarm.r", parent = "chest", x=-0.212007, y=1.106761, z=-0.000000, radius=0.036285, rotX=-90.000000, rotY=86.715446, rotZ=0.000001, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.241897 },
        { name = "head", parent = "chest", x=0.000000, y=1.241426, z=-0.000000, radius=0.038527, rotX=-0.000007, rotY=0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.256849 },
        { name = "foot.l", parent = "lowerleg.l", x=0.170945, y=0.145237, z=-0.018586, radius=0.024848, rotX=136.043976, rotY=-0.000025, rotZ=0.000010, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "foot.r", parent = "lowerleg.r", x=-0.170945, y=0.145237, z=-0.018586, radius=0.024848, rotX=136.043976, rotY=0.000025, rotZ=-0.000010, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "lowerarm.l", parent = "upperarm.l", x=0.453507, y=1.106761, z=-0.013860, radius=0.039007, rotX=90.000313, rotY=-86.944817, rotZ=179.999695, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.260044 },
        { name = "lowerarm.r", parent = "upperarm.r", x=-0.453507, y=1.106761, z=-0.013860, radius=0.039007, rotX=90.000313, rotY=86.944817, rotZ=-179.999695, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.260044 },
        { name = "toes.l", parent = "foot.l", x=0.170945, y=0.025990, z=0.096393, radius=0.024848, rotX=90.000000, rotY=-0.000010, rotZ=179.999969, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "toes.r", parent = "foot.r", x=-0.170945, y=0.025990, z=0.096393, radius=0.024848, rotX=90.000000, rotY=0.000010, rotZ=-179.999969, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "wrist.l", parent = "lowerarm.l", x=0.713182, y=1.106761, z=-0.000000, radius=0.011074, rotX=0.000000, rotY=-90.000000, rotZ=-90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.073826 },
        { name = "wrist.r", parent = "lowerarm.r", x=-0.713182, y=1.106761, z=-0.000000, radius=0.011074, rotX=0.000000, rotY=90.000000, rotZ=90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.073826 },
        { name = "hand.l", parent = "wrist.l", x=0.787008, y=1.106761, z=-0.000000, radius=0.016802, rotX=0.000000, rotY=-90.000000, rotZ=-90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
        { name = "hand.r", parent = "wrist.r", x=-0.787008, y=1.106761, z=-0.000000, radius=0.016802, rotX=0.000000, rotY=90.000000, rotZ=90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
        { name = "handslot.l", parent = "hand.l", x=0.883133, y=1.049261, z=-0.000000, radius=0.016802, rotX=90.000000, rotY=-0.000000, rotZ=179.999985, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
        { name = "handslot.r", parent = "hand.r", x=-0.883133, y=1.049261, z=-0.000000, radius=0.016802, rotX=90.000000, rotY=0.000000, rotZ=-179.999985, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
    },
}
-- Extend with more named rigs here as they're captured (see this session's extraction method:
-- load a real rigged .msh in mesh_debug, dump meshD:getBone(i) for every bone plus computeMeshAABB
-- as Lua literals). showBonesNode's combo lists these by `label`, in order.
local ARMATURE_TEMPLATES = {
    ARMATURE_NO_FINGERS_23,
    ARMATURE_NO_FINGERS_25,
    ARMATURE_2_CHAIN_FINGERS , 
    ARMATURE_3_CHAIN_FINGERS,
    ARMATURE_STANDARD_SKELETON_65,
}

-- Replaces the mesh's entire current skeleton (if any) with `template`'s real bone hierarchy,
-- uniformly scaled (never stretched per-axis -- see ARMATURE_TEMPLATES' own comment on why) from
-- template.referenceAABB's own height to the target mesh's own current height, and repositioned so
-- the scaled skeleton's own bottom-center lands on the target's own AABB bottom-center (its floor,
-- centered on width/depth) -- the same "stands on its own floor, centered" placement convention the
-- old fraction-based template used. Clearing existing bones first is cheap and cascade-free by
-- construction: since parent always precedes child in the stored vector (the same invariant
-- updateBone's resort maintains), the *last* index always has zero children, so removing from the
-- end never hits removeBone's cascade-refusal path. Returns true/false, err.
local function applyArmatureTemplate(meshD, template)
    local aabb = computeMeshAABB(meshD)
    if not aabb then return false, tLang.L('bones_humanoid_no_geometry') end

    local okTotal, nExisting = dpCall(function() return meshD:getTotalBone() end)
    nExisting = (okTotal and nExisting) or 0
    for i = nExisting, 1, -1 do
        dpCall(function() return meshD:removeBone(i, false) end)
    end

    -- Any existing SECTION_VERTEX_SKIN_WEIGHTS data (docs/mesh-v11-format.md Sec. 6f) references
    -- the OLD skeleton's own bone names by its own separate palette -- deliberately independent of
    -- SECTION_FRAME_SKINNED so ordinary bone edits (rename/reorder/add/remove a bone or two) don't
    -- silently corrupt it. Replacing the WHOLE skeleton with a different rig's bone names is not an
    -- ordinary edit: none of the old weight palette's names exist in the new skeleton at all, so
    -- every one of those weights is now orphaned. Confirmed via real user testing to cause a
    -- concrete, silent export failure if left alone: build_mesh's export sees hasVertexWeights()
    -- still true and trusts the (now-meaningless) old data completely, skipping
    -- bind_mesh_to_armature's envelope-binding fallback entirely -- the exported FBX ends up with
    -- vertex groups named after bones that don't exist in its own armature, so every vertex gets
    -- zero real deform weight (mesh invisible in Mixamo) while the armature itself, being
    -- structurally valid on its own, still animates fine. Clearing here forces export to correctly
    -- fall back to fresh envelope binding for the newly-applied skeleton instead.
    dpCall(function() meshD:removeVertexWeights() end)

    local ref = template.referenceAABB
    local refHeight = ref.maxY - ref.minY
    local targetHeight = aabb.maxY - aabb.minY
    local scale = (refHeight > 0.0001) and (targetHeight / refHeight) or 1.0

    local refAnchorX, refAnchorY, refAnchorZ = (ref.minX + ref.maxX) / 2, ref.minY, (ref.minZ + ref.maxZ) / 2
    local targetAnchorX, targetAnchorY, targetAnchorZ = (aabb.minX + aabb.maxX) / 2, aabb.minY, (aabb.minZ + aabb.maxZ) / 2

    for _, j in ipairs(template.bones) do
        local x = targetAnchorX + (j.x - refAnchorX) * scale
        local y = targetAnchorY + (j.y - refAnchorY) * scale
        local z = targetAnchorZ + (j.z - refAnchorZ) * scale
        local okA, err = dpCall(function()
            return meshD:addBone(j.name, j.parent, x, y, z, j.radius * scale,
                j.rotX, j.rotY, j.rotZ, j.scaleX, j.scaleY, j.scaleZ, j.length * scale)
        end)
        if not okA then return false, err end
    end
    return true, nil
end

-- Dumps the CURRENT mesh's own skeleton as a standalone Lua chunk in exactly
-- ARMATURE_STANDARD_SKELETON_65's own shape (`return { label=, referenceAABB=, bones={...} }`) --
-- loadArmatureFromFile below reads it back with loadfile(), and this is also the same by-hand
-- format used to embed a new template directly into this file's own ARMATURE_TEMPLATES (see that
-- table's own comment) if the user wants to promote an experiment to a permanent entry later.
-- Lets the user capture a skeleton they've hand-fitted to one mesh (per applyArmatureTemplate's own
-- "adapt the bones to the mesh" caveat -- a uniform scale-fit alone doesn't know a target mesh's
-- own limb proportions) and reapply it to other meshes via Import Armature below, without needing
-- a source-code change for every experiment.
local function exportArmatureToFile(meshD, path)
    local aabb = computeMeshAABB(meshD)
    if not aabb then return false, tLang.L('bones_humanoid_no_geometry') end
    local okTotal, nBones = dpCall(function() return meshD:getTotalBone() end)
    nBones = (okTotal and nBones) or 0
    if nBones == 0 then return false, tLang.L('bones_export_armature_no_bones') end

    -- string.format('%.6f', ...) is locale-sensitive (LC_NUMERIC) the same way writeMeshDebugJson's
    -- own JSON numbers are -- a ',' decimal separator here wouldn't just be ugly, it would make the
    -- exported chunk invalid Lua syntax on load. Same C-locale-for-the-duration pattern.
    local prevNumericLocale = nil
    if os and os.setlocale then
        prevNumericLocale = os.setlocale(nil, 'numeric')
        os.setlocale('C', 'numeric')
    end

    local f = io.open(path, 'w')
    if not f then
        if os and os.setlocale and prevNumericLocale then os.setlocale(prevNumericLocale, 'numeric') end
        return false, 'Failed to create file: ' .. path
    end

    f:write('-- Armature exported from mesh_debug.lua -- return-value chunk, load with loadfile()\n')
    f:write('-- (see loadArmatureFromFile) or paste into ARMATURE_TEMPLATES to make it permanent.\n')
    f:write('return {\n')
    f:write(string.format('    label = %q,\n', tUtil.getShortName(path):gsub('%.[^%.]+$', '')))
    f:write(string.format('    referenceAABB = { minX=%.6f, minY=%.6f, minZ=%.6f, maxX=%.6f, maxY=%.6f, maxZ=%.6f },\n',
        aabb.minX, aabb.minY, aabb.minZ, aabb.maxX, aabb.maxY, aabb.maxZ))
    f:write('    bones = {\n')
    for i = 1, nBones do
        local okG, name, x, y, z, radius, parentName, rotX, rotY, rotZ, scaleX, scaleY, scaleZ, length =
            dpCall(function() return meshD:getBone(i) end)
        if okG and name then
            f:write(string.format(
                '        { name = %q, parent = %s, x=%.6f, y=%.6f, z=%.6f, radius=%.6f, rotX=%.6f, rotY=%.6f, rotZ=%.6f, scaleX=%.6f, scaleY=%.6f, scaleZ=%.6f, length=%.6f },\n',
                name, parentName and string.format('%q', parentName) or 'nil', x, y, z, radius,
                rotX or 0, rotY or 0, rotZ or 0, scaleX or 1, scaleY or 1, scaleZ or 1, length or 0))
        end
    end
    f:write('    },\n}\n')
    f:close()

    if os and os.setlocale and prevNumericLocale then os.setlocale(prevNumericLocale, 'numeric') end
    return true, nil
end

-- Reads back a file exportArmatureToFile wrote (or a hand-authored one following the same shape)
-- as a real armature template, suitable for applyArmatureTemplate exactly like a built-in
-- ARMATURE_TEMPLATES entry -- executes it as trusted Lua (the same dofile/loadfile-as-data-format
-- convention this whole codebase already uses, e.g. editor/lang/language.lua), since this file is
-- always either something the user exported from this same tool or something they hand-authored
-- themselves, never untrusted external input.
local function loadArmatureFromFile(path)
    local chunk, errLoad = loadfile(path)
    if not chunk then return nil, errLoad end
    local okRun, result = pcall(chunk)
    if not okRun then return nil, tostring(result) end
    if type(result) ~= 'table' or type(result.bones) ~= 'table' or type(result.referenceAABB) ~= 'table' then
        return nil, tLang.L('bones_import_armature_invalid')
    end
    return result, nil
end

-- ---------------------------------------------------------------------------
-- FBX export (current/all): dump geometry+bones to JSON, hand off to headless Blender
-- (editor/blender_mesh_skeleton_export.py) via editor/blender_cli_wrapper.lua using a
-- write-JSON/launch/poll pattern (see startBlenderBuild/blenderBuildCoroutine/
-- showBlenderBuildDialog below).
-- ---------------------------------------------------------------------------

local function jsonStr(s)
    s = tostring(s or '')
    s = s:gsub('\\', '\\\\')
    s = s:gsub('"', '\\"')
    s = s:gsub('\n', '\\n')
    s = s:gsub('\r', '\\r')
    s = s:gsub('\t', '\\t')
    return '"' .. s .. '"'
end

-- Dumps frame 1's raw geometry (all subsets, pooled into one combined vertex list with globally-
-- offset indices, each subset keeping its own resolved texture path) plus the mesh's current bone
-- list (may be empty -- a mesh-only FBX is still a valid export) to jsonPath.
local function writeMeshDebugJson(meshD, jsonPath)
    local okTF, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    nFrames = (okTF and nFrames) or 0
    if nFrames < 1 then
        return false, tLang.L('bones_export_no_geometry')
    end

    -- JSON numbers must use '.' as decimal separator, but string.format('%f', ...) follows
    -- the C runtime's LC_NUMERIC locale -- on locales such as pt_BR/de_DE this produces ','
    -- instead, corrupting the JSON (see editor_utils.lua's tUtil.save locale note). Force the
    -- "C" locale for the duration of the write, same pattern as texture_packer.lua/
    -- particle_editor.lua/scene_editor3d.lua/scene_editor2d.lua.
    local prevNumericLocale = nil
    if os and os.setlocale then
        prevNumericLocale = os.setlocale(nil, 'numeric')
        os.setlocale('C', 'numeric')
    end
    local function restoreLocale()
        if os and os.setlocale and prevNumericLocale then
            os.setlocale(prevNumericLocale, 'numeric')
        end
    end

    local f = io.open(jsonPath, 'w')
    if not f then
        restoreLocale()
        return false, 'Failed to create file: ' .. jsonPath
    end

    f:write('{\n  "joints": [\n')
    local okTB, nBones = dpCall(function() return meshD:getTotalBone() end)
    nBones = (okTB and nBones) or 0
    for i = 1, nBones do
        local okG, name, x, y, z, radius, parentName, rotX, rotY, rotZ, scaleX, scaleY, scaleZ, length =
            dpCall(function() return meshD:getBone(i) end)
        if okG and name then
            f:write(string.format(
                '    { "name": %s, "parent": %s, "x": %.6f, "y": %.6f, "z": %.6f, "radius": %.6f, ' ..
                '"rotX": %.6f, "rotY": %.6f, "rotZ": %.6f, "scaleX": %.6f, "scaleY": %.6f, "scaleZ": %.6f, "length": %.6f }',
                jsonStr(name), parentName and jsonStr(parentName) or 'null', x, y, z, radius,
                rotX or 0, rotY or 0, rotZ or 0, scaleX or 1, scaleY or 1, scaleZ or 1, length or 0))
            f:write(i < nBones and ',\n' or '\n')
        end
    end
    f:write('  ],\n  "mesh": {\n    "vertices": [\n')

    -- Real per-vertex bone weights (SECTION_VERTEX_SKIN_WEIGHTS, docs/mesh-v11-format.md Sec. 6f),
    -- when present, ride along on each vertex dict so blender_mesh_skeleton_export.py's build_mesh
    -- can use the mesh's own originally-authored weights instead of inventing new ones via
    -- ARMATURE_ENVELOPE. meshD:getVertexWeight() takes a GLOBAL (frame-wide, across every subset)
    -- 1-based vertex index -- exactly (totalVerts + v) below, the same running-total convention the
    -- index-list offset a few lines down already uses for the same reason.
    local okHasW, hasWeights = dpCall(function() return meshD:hasVertexWeights() end)
    hasWeights = okHasW and hasWeights
    local function weightJsonFragment(globalVertexIndex)
        local okGW, n1, w1, n2, w2, n3, w3, n4, w4 = dpCall(function() return meshD:getVertexWeight(globalVertexIndex) end)
        if not okGW or not n1 then return '' end
        local names, weights = {}, {}
        local function addSlot(n, w)
            if n then
                table.insert(names, jsonStr(n))
                table.insert(weights, string.format('%.6f', w or 0))
            end
        end
        addSlot(n1, w1); addSlot(n2, w2); addSlot(n3, w3); addSlot(n4, w4)
        if #names == 0 then return '' end
        return ', "boneNames": [' .. table.concat(names, ', ') .. '], "weights": [' .. table.concat(weights, ', ') .. ']'
    end

    local okTS, nSubsets = dpCall(function() return meshD:getTotalSubset(1) end)
    nSubsets = (okTS and nSubsets) or 0
    local totalVerts = 0
    local tSubsetIndexLists = {}
    local firstVert = true
    for s = 1, nSubsets do
        local okV, nVerts = dpCall(function() return meshD:getTotalVertex(1, s) end)
        nVerts = (okV and nVerts) or 0
        for v = 1, nVerts do
            local okG, vert = dpCall(function() return meshD:getVertex(1, s, v) end)
            if okG and vert then
                if not firstVert then f:write(',\n') end
                firstVert = false
                local weightFrag = hasWeights and weightJsonFragment(totalVerts + v) or ''
                f:write(string.format('      { "x": %.6f, "y": %.6f, "z": %.6f, "u": %.6f, "v": %.6f%s }',
                    vert.x, vert.y, vert.z, vert.u or 0, vert.v or 0, weightFrag))
            end
        end
        -- getIndex returns subset-local 1-based indices -- offset by the running vertex total
        -- (accumulated from prior subsets only, see below) to make them global across the single
        -- combined vertex list this JSON writes.
        local okI, idxList = dpCall(function() return meshD:getIndex(1, s) end)
        local offsetIdx = {}
        if okI and idxList then
            for _, li in ipairs(idxList) do
                table.insert(offsetIdx, li + totalVerts)
            end
        end
        tSubsetIndexLists[s] = offsetIdx
        totalVerts = totalVerts + nVerts
    end
    f:write('\n    ],\n    "subsets": [\n')
    local function resolveTextureForExport(texturePath)
        if not texturePath or texturePath == '' then return nil end
        local candidate = mbm.getFullPath(texturePath) or texturePath
        local textureFile = io.open(candidate, 'rb')
        if not textureFile then return nil end
        textureFile:close()
        return candidate
    end
    for s = 1, nSubsets do
        local idx = tSubsetIndexLists[s] or {}
        -- Resolve+verify each subset's own texture so the exported FBX carries a real material
        -- instead of none at all (confirmed: a totally material-less mesh renders as fully
        -- invisible in Mixamo's viewer, not a plain/gray fallback). mbm.getFullPath echoes a
        -- texture back unresolved if no search path matches (bare "texture.png"-style refs seen
        -- before, see docs/future_investigation.md) -- verify with io.open before trusting it,
        -- same defensive check as copyMeshTexturesToFolder.
        local okTex, texPath = dpCall(function() return meshD:getTexture(1, s) end)
        local resolvedTex = okTex and resolveTextureForExport(texPath) or nil
        local materialTextures = {}
        for _, role in ipairs({'normal', 'specular', 'emissive', 'mask'}) do
            local okRole, rolePath = dpCall(function() return meshD:getMaterialTexture(1, s, role) end)
            local resolvedRole = okRole and resolveTextureForExport(rolePath) or nil
            if resolvedRole then
                table.insert(materialTextures, string.format('%s: %s', jsonStr(role), jsonStr(resolvedRole)))
            end
        end
        f:write(string.format('      { "indices": [%s], "texture": %s, "materialTextures": {%s} }',
            table.concat(idx, ', '), resolvedTex and jsonStr(resolvedTex) or 'null',
            table.concat(materialTextures, ', ')))
        f:write(s < nSubsets and ',\n' or '\n')
    end
    f:write('    ]\n  }\n}\n')
    f:close()
    restoreLocale()

    if totalVerts == 0 then
        os.remove(jsonPath)
        return false, tLang.L('bones_export_no_geometry')
    end
    return true, nil
end

local function getOSTempDirForExport()
    return os.getenv('TMPDIR') or os.getenv('TEMP') or os.getenv('TMP') or '/tmp'
end

-- Self-contained unique ".fbx" path builder for "Export All Meshes": the file's own
-- makeUniqueBatchOutputPath/joinFolderFile/getBatchPathKey (used by onSaveAllToFolder) are
-- declared later in this same chunk (~line 6939+), after this point, so aren't visible here yet --
-- this is a small, deliberately independent equivalent rather than reordering unrelated code.
local function joinPathForFbxExport(folder, fileName)
    local sep = folder:match('[/\\]$') and '' or (folder:find('\\') and '\\' or '/')
    return folder .. sep .. fileName
end

local function makeUniqueFbxOutputPath(folder, sourceFile, usedNames, index)
    local base = tUtil.getShortName(sourceFile or '') or ''
    base = base:gsub('%.[^%.]+$', '')
    if base == '' then base = string.format('mesh_%d', index or 1) end
    local candidate = base
    local suffix = 2
    while usedNames[candidate] do
        candidate = base .. '_' .. suffix
        suffix = suffix + 1
    end
    usedNames[candidate] = true
    return joinPathForFbxExport(folder, candidate .. '.fbx')
end

-- Processes `entries` ({name=, meshD=, outputFbx=}, one or many) sequentially -- one Blender
-- process at a time, waiting for each to finish before starting the next, to avoid overlapping
-- temp files/processes sharing the same fixed dbgLog/cancelFile paths. Populates
-- tMeshExportBuildState.tRunResults with a per-entry {name, ok, msg} outcome, mirroring the
-- results-list idiom already used elsewhere in this file for bulk operations (e.g. Blender import).
local function meshExportBuildCoroutine(entries)
    local st = tMeshExportBuildState
    local exporterPath = getEditorDir() .. '/blender_mesh_skeleton_export.py'
    local tmpDir = getOSTempDirForExport()
    local dbgLog = tmpDir .. '/mesh_debug_skeleton_export.log'
    local cancelFile = tmpDir .. '/mesh_debug_skeleton_export_cancel'
    local jsonPath = tmpDir .. '/mesh_debug_skeleton_export_input.json'
    st.sCancelFile = cancelFile

    for _, entry in ipairs(entries) do
        if st.bAbortRequested then
            st.tRunResults[#st.tRunResults + 1] = { name = entry.name, ok = false, msg = tLang.L('cancel') }
            goto continueEntry
        end

        os.remove(jsonPath)
        -- mbm.getFullPath (used below, inside writeMeshDebugJson, to resolve each subset's
        -- texture) only finds a bare/relative filename if the mesh's own directory was already
        -- registered as a search path -- same gap fixed for "Save All to Folder" in 6.26.4, not
        -- guaranteed here since this entry may never have been individually previewed.
        local dirForTex = entry.fileName and entry.fileName:match('^(.*)[/\\]')
        if dirForTex then mbm.addPath(dirForTex) end
        local okJson, errJson = writeMeshDebugJson(entry.meshD, jsonPath)
        if not okJson then
            st.tRunResults[#st.tRunResults + 1] = { name = entry.name, ok = false, msg = errJson }
            goto continueEntry
        end

        -- Rotation/invert values come from tMeshExportOptionsState, edited by the user in
        -- showMeshExportOptionsDialog right before this coroutine starts -- defaulted there to the
        -- exact inverse of the Blender-import dialog's own defaults (Rot X -90 -> 90, and the same
        -- Invert U/V flags, since inverting is self-cancelling but rotation isn't). There is no
        -- per-mesh record of what rotation/invert was actually used at import time (it's baked
        -- into vertex data, not tracked as metadata), so the defaults assume the standard import
        -- settings were used -- the whole point of exposing this dialog is letting the user
        -- override that assumption when it doesn't hold.
        local xo = tMeshExportOptionsState
        local cmd = tBlender.buildMeshSkeletonExportCmd(jsonPath, entry.outputFbx, exporterPath,
            { cancelFile = cancelFile, debugSteps = st.bDebugSteps,
              exportAngleX = xo.nAngleX, exportAngleY = xo.nAngleY, exportAngleZ = xo.nAngleZ,
              exportInvertU = xo.bInvertU, exportInvertV = xo.bInvertV })
        if not cmd then
            st.tRunResults[#st.tRunResults + 1] = { name = entry.name, ok = false, msg = tLang.L('blender_not_found') }
            goto continueEntry
        end

        os.remove(entry.outputFbx)
        os.remove(dbgLog)
        os.remove(cancelFile)
        tBlender.launchCmdAsync(cmd, dbgLog)

        do
            local startTime = os.time()
            local lastActivityTime = startTime
            local lastLogSize = -1
            local finished, ok, msg = false, false, ''
            while not finished do
                if st.bAbortRequested then
                    local cf = io.open(cancelFile, 'w')
                    if cf then cf:write('cancel\n'); cf:close() end
                    ok, msg, finished = false, tLang.L('cancel'), true
                elseif tBlender.fileExists(entry.outputFbx) then
                    ok, msg, finished = true, entry.outputFbx, true
                else
                    local logSize = 0
                    local lf = io.open(dbgLog, 'rb')
                    if lf then logSize = lf:seek('end'); lf:close() end
                    if logSize ~= lastLogSize then
                        lastLogSize = logSize
                        lastActivityTime = os.time()
                    end
                    if os.time() - lastActivityTime >= st.iTimeoutSecs then
                        local errLog = io.open(dbgLog, 'r')
                        local tail = ''
                        if errLog then tail = errLog:read('*a') or ''; errLog:close() end
                        ok, msg, finished = false, tail:sub(-400), true
                    else
                        coroutine.yield()
                    end
                end
            end
            st.tRunResults[#st.tRunResults + 1] = { name = entry.name, ok = ok, msg = msg }
        end

        ::continueEntry::
    end

    local okCount, total = 0, #entries
    for _, r in ipairs(st.tRunResults) do
        if r.ok then okCount = okCount + 1 end
    end
    st.sStatus = string.format(tLang.L('bones_export_summary_fmt'), okCount, total)
    st.bStatusOk = okCount == total
    st.bBuilding = false
end

-- Shared entry point for both "Export Current Mesh" and "Export All Meshes". `entries` is a list
-- of {name=, meshD=, outputFbx=}; the caller resolves output paths before calling this (single
-- mbm.saveFile prompt for one mesh, a folder pick + per-entry filename for "all").
local function startMeshExportBuild(entries)
    local st = tMeshExportBuildState
    if st.bBuilding or #entries == 0 then return end

    st.bOpen = true
    st.bOpenPopup = true
    st.bBuilding = true
    st.bAbortRequested = false
    st.sStatus = ''
    st.bStatusOk = true
    st.tRunResults = {}
    st.co = coroutine.create(function() meshExportBuildCoroutine(entries) end)
end

-- Shown before startMeshExportBuild actually runs -- opened by the "Export Current Mesh"/"Export
-- All Meshes" menu handlers once they've already resolved `entries` (output path(s) picked via the
-- existing mbm.saveFile/mbm.openFolder flow, unchanged). Mirrors showBlenderImportDialog's own
-- "Post-processing" block, in reverse: same Invert U/V + Rot X/Y/Z widgets, reusing those exact
-- language keys since the wording ("Invert U", "Rot X"...) is generic, not import-specific.
function showMeshExportOptionsDialog()
    local st = tMeshExportOptionsState
    if not st.bOpen then return end

    if st.bOpenPopup then
        tImGui.OpenPopup('mesh_export_options_modal')
        st.bOpenPopup = false
    end

    local pFlags = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
    local isOpen = tImGui.BeginPopupModal(tLang.L('mesh_export_options_title') .. '###mesh_export_options_modal', false, pFlags)
    if not isOpen then return end

    tImGui.TextWrapped(tLang.L('mesh_export_options_help'))
    tImGui.Separator()

    st.bInvertU = tImGui.Checkbox(tLang.L('blender_import_invert_u') .. '##exportInvU', st.bInvertU)
    tImGui.SameLine()
    st.bInvertV = tImGui.Checkbox(tLang.L('blender_import_invert_v') .. '##exportInvV', st.bInvertV)

    tImGui.PushItemWidth(120)
    local rxChanged, newRx = tImGui.InputFloat(tLang.L('blender_import_rotation_x') .. '##exportRx', st.nAngleX, 1, 15, '%.1f', 0)
    if rxChanged and newRx then st.nAngleX = newRx end
    tImGui.SameLine()
    local ryChanged, newRy = tImGui.InputFloat(tLang.L('blender_import_rotation_y') .. '##exportRy', st.nAngleY, 1, 15, '%.1f', 0)
    if ryChanged and newRy then st.nAngleY = newRy end
    tImGui.SameLine()
    local rzChanged, newRz = tImGui.InputFloat(tLang.L('blender_import_rotation_z') .. '##exportRz', st.nAngleZ, 1, 15, '%.1f', 0)
    if rzChanged and newRz then st.nAngleZ = newRz end
    tImGui.PopItemWidth()

    tImGui.Separator()
    if tImGui.Button(tLang.L('start_export_button') .. '##meshExportOptionsStart') then
        local entries = st.tEntries
        st.bOpen = false
        st.tEntries = nil
        tImGui.CloseCurrentPopup()
        if entries then startMeshExportBuild(entries) end
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('cancel') .. '##meshExportOptionsCancel') then
        st.bOpen = false
        st.tEntries = nil
        tImGui.CloseCurrentPopup()
    end

    tImGui.EndPopup()
end

function showMeshExportBuildDialog()
    local st = tMeshExportBuildState
    if not st.bOpen then return end

    if st.bOpenPopup then
        tImGui.OpenPopup('mesh_debug_skeleton_export_modal')
        st.bOpenPopup = false
    end

    local pFlags = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
    local isOpen = tImGui.BeginPopupModal(tLang.L('bones_export_dialog_title') .. '###mesh_debug_skeleton_export_modal', false, pFlags)
    if not isOpen then return end

    if st.bBuilding then
        if st.co and coroutine.status(st.co) == 'suspended' then
            local ok, err = coroutine.resume(st.co)
            if not ok then
                st.bBuilding = false
                st.sStatus = tostring(err)
                st.bStatusOk = false
            end
        end
        tImGui.Text(tLang.L('bones_export_building'))
        if tImGui.Button(tLang.L('cancel') .. '##meshExportCancel') then
            st.bAbortRequested = true
        end
    else
        for _, r in ipairs(st.tRunResults) do
            local prefix = r.ok and '[OK] ' or '[FAIL] '
            tImGui.TextWrapped(prefix .. tostring(r.name) .. ' -- ' .. tostring(r.msg))
        end
        tImGui.Separator()
        tImGui.TextWrapped(st.sStatus)
        if tImGui.Button(tLang.L('blender_import_btn_close') .. '##meshExportClose') then
            st.bOpen = false
            tImGui.CloseCurrentPopup()
        end
    end

    tImGui.EndPopup()
end

-- ---------------------------------------------------------------------------
-- Bones tree node: view/add/edit/remove the mesh's optional skeleton
-- (SECTION_FRAME_SKINNED / meshDebug:addBone|getBone|updateBone|removeBone).
-- Editor/diagnostic data only -- never consulted by rendering (docs/mesh-v11-format.md Sec 6e).
-- ---------------------------------------------------------------------------
-- Shared by showBonesNode (Up axis/Humanoid/bake/add-bone) and showBonesWindow (the table) -- both
-- need the current skeleton's bone list read fresh every frame (tens of joints at most, same idiom
-- as showFrameNode's per-frame subset read).
local function getBoneList(meshD)
    local okTotal, nBones = dpCall(function() return meshD:getTotalBone() end)
    nBones = (okTotal and nBones) or 0
    local tBones = {}
    for i = 1, nBones do
        local okG, name, x, y, z, radius, parentName, rotX, rotY, rotZ, scaleX, scaleY, scaleZ, length =
            dpCall(function() return meshD:getBone(i) end)
        if okG and name then
            table.insert(tBones, { idx = i, name = name, x = x, y = y, z = z, radius = radius, parentName = parentName,
                rotX = rotX, rotY = rotY, rotZ = rotZ, scaleX = scaleX, scaleY = scaleY, scaleZ = scaleZ, length = length })
        end
    end
    return tBones, nBones
end

-- Shared by every bone-mutating action in showBonesNode/showBonesWindow -- keeps the "no
-- iLastPreviewedIndex reset" fix for the preview-mesh show/hide flicker (see rebuildBoneGizmo's
-- caller history) in exactly one place instead of two independently-maintained copies.
local function onBonesEdit(tEntry, meshD, index)
    tEntry.modified = true
    -- A bone edit can indirectly invalidate SECTION_VERTEX_SKIN_WEIGHTS data too (most notably
    -- applyArmatureTemplate replacing the whole skeleton, which also clears weights itself -- see
    -- its own comment) -- Mesh Info's cached stats (showMeshInfoTable's tEntry.weightStats) must
    -- not keep showing stale numbers after that.
    tEntry.weightStats = nil
    -- A moved/resized bone (or Recompute/Roll changing its orientation) can change which vertices
    -- are actually nearest to it -- a cached Rigid Bind preview (showBonesWindow's
    -- tEntry.tRigidBindUI.matched) must not stay showing a stale vertex set after that.
    if tEntry.tRigidBindUI then tEntry.tRigidBindUI.matched = nil end
    rebuildBoneGizmo(tEntry, meshD, index)
end

-- Smooth-drag speed scaled to this specific skeleton's own data, instead of one fixed value that's
-- either way too coarse (a small stylized character, every value under ~1.5) or way too fine (a
-- large-scale import), confirmed by direct user testing of the previous fixed 0.5 speed on
-- Lorekeeper (values roughly -0.9..1.24). Speed = full observed range / 200, so a full-width mouse
-- drag roughly spans that whole range; floored at `fallback` so a single-bone or perfectly flat
-- skeleton (range 0) doesn't end up with a zero-speed, un-draggable field.
local function computeFieldDragSpeed(tBones, fields, fallback)
    local minV, maxV = nil, nil
    for _, b in ipairs(tBones) do
        for _, f in ipairs(fields) do
            local v = b[f]
            if v then
                minV = (minV == nil or v < minV) and v or minV
                maxV = (maxV == nil or v > maxV) and v or maxV
            end
        end
    end
    if minV == nil or maxV <= minV then return fallback end
    return math.max((maxV - minV) / 200, fallback)
end

local function findBoneByName(tBones, name)
    for _, bb in ipairs(tBones) do
        if bb.name == name then return bb end
    end
    return nil
end

-- Returns the conventional opposite-side bone name, or nil for a center/unrecognized bone. The
-- built-in humanoid armatures use Mixamo's Left/Right tokens or Blender-style .l/.r suffixes. Keep
-- this name based: position alone cannot distinguish a deliberately off-center spine/head from a
-- limb, and must never make a center bone mirror onto itself.
function getOppositeSideBoneName(name)
    if type(name) ~= 'string' then return nil end
    local replacements = {
        {'Left', 'Right'}, {'Right', 'Left'},
        {'left', 'right'}, {'right', 'left'},
    }
    for _, pair in ipairs(replacements) do
        local startPos, endPos = string.find(name, pair[1], 1, true)
        if startPos then
            return string.sub(name, 1, startPos - 1) .. pair[2] .. string.sub(name, endPos + 1)
        end
    end
    local suffixPairs = { ['.l'] = '.r', ['.r'] = '.l', ['.L'] = '.R', ['.R'] = '.L' }
    local suffix = string.sub(name, -2)
    local opposite = suffixPairs[suffix]
    if opposite then return string.sub(name, 1, -3) .. opposite end
    return nil
end

function updateBonePosition(meshD, bone, x, y, z)
    return dpCall(function()
        return meshD:updateBone(bone.idx, bone.name, bone.parentName, x, y, z, bone.radius,
            bone.rotX, bone.rotY, bone.rotZ, bone.scaleX, bone.scaleY, bone.scaleZ, bone.length)
    end)
end

-- children_by_parent (blender_mesh_skeleton_export.py:200-203), ported to Lua so Recompute can
-- mirror the exporter's own topology rules exactly.
local function computeChildrenByParent(tBones)
    local childrenMap = {}
    for _, bb in ipairs(tBones) do
        if bb.parentName then
            childrenMap[bb.parentName] = childrenMap[bb.parentName] or {}
            table.insert(childrenMap[bb.parentName], bb)
        end
    end
    return childrenMap
end

-- Ports compute_tail (blender_mesh_skeleton_export.py:228-245) -- the exact fallback the FBX
-- exporter already uses, silently, whenever a bone's length is <= EPS -- into Lua, so "Recompute"
-- bakes that same direction into real, inspectable/editable fields instead of leaving it an
-- invisible export-time guess. Same rules: a single child -> aim at it; a multi-child ROOT (no
-- parent) -> aim at its first child; a leaf or a multi-child NON-root -> continue the direction
-- the bone arrived from (parent -> self); returns a unit aim vector (bone-local Y axis) + length.
local function computeAimAndLength(childrenMap, b, parentB)
    local children = childrenMap[b.name]
    local ax, ay, az, length
    if children and (#children == 1 or not parentB) then
        local target = children[1]
        local dx, dy, dz = target.x - b.x, target.y - b.y, target.z - b.z
        local d = math.sqrt(dx * dx + dy * dy + dz * dz)
        if d >= 1e-6 then
            ax, ay, az, length = dx / d, dy / d, dz / d, d
        end
    end
    if not ax and parentB then
        local dx, dy, dz = b.x - parentB.x, b.y - parentB.y, b.z - parentB.z
        local dlen = math.sqrt(dx * dx + dy * dy + dz * dz)
        if dlen >= 1e-6 then
            ax, ay, az, length = dx / dlen, dy / dlen, dz / dlen, math.max(0.01, dlen)
        end
    end
    if not ax then
        -- Rootless leaf or a degenerate coincident parent -- both rare. Default to straight up
        -- (+Y), this engine's own vertical convention (e.g. CAMERA position/focus height), rather
        -- than Python's own post-rotation +Z default which only makes sense in its own transformed
        -- export space.
        ax, ay, az, length = 0, 1, 0, 0.01
    end
    return ax, ay, az, length
end

-- Squared point-to-segment distance in 3D (segment (ax,ay,az)-(bx,by,bz), point (px,py,pz)) --
-- the same distance concept Blender's ARMATURE_ENVELOPE uses to decide which bone influences a
-- vertex (blender_mesh_skeleton_export.py's set_envelope_radius), computed explicitly here so the
-- editor can preview/select a rigid-bind target before writing real per-vertex weights, instead of
-- leaving Blender to guess it at export time.
local function pointSegmentDistSq(px, py, pz, ax, ay, az, bx, by, bz)
    local dx, dy, dz = bx - ax, by - ay, bz - az
    local segLenSq = dx * dx + dy * dy + dz * dz
    if segLenSq < 1e-12 then
        local ex, ey, ez = px - ax, py - ay, pz - az
        return ex * ex + ey * ey + ez * ez
    end
    local t = ((px - ax) * dx + (py - ay) * dy + (pz - az) * dz) / segLenSq
    t = math.max(0, math.min(1, t))
    local cx, cy, cz = ax + dx * t, ay + dy * t, az + dz * t
    local ex, ey, ez = px - cx, py - cy, pz - cz
    return ex * ex + ey * ey + ez * ez
end

-- Finds every frame-1 vertex whose NEAREST bone segment (every bone's own position <-> its
-- parent's position; a root bone's "segment" is just its own point) is targetBone's own segment,
-- within targetBone.radius -- a rigid-bind candidate set for "Apply Rigid Bind" below. Global
-- vertex indices follow the exact same running-subset-offset convention writeMeshDebugJson (and
-- meshD:setVertexWeight/getVertexWeight) already use: cumulative vertex count from PRIOR subsets
-- (frame 1 only) + this subset's own 1-based index.
local function findVerticesNearBoneSegment(meshD, tBones, targetBone)
    local segments = {}
    for _, bb in ipairs(tBones) do
        local parentB = bb.parentName and findBoneByName(tBones, bb.parentName)
        segments[bb.name] = {
            ax = bb.x, ay = bb.y, az = bb.z,
            bx = parentB and parentB.x or bb.x,
            by = parentB and parentB.y or bb.y,
            bz = parentB and parentB.z or bb.z,
        }
    end
    local radiusSq = (targetBone.radius or 0) * (targetBone.radius or 0)
    local matched = {}
    local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(1) end)
    nSubsets = (okS and nSubsets) or 0
    local totalVerts = 0
    for s = 1, nSubsets do
        local okV, nVerts = dpCall(function() return meshD:getTotalVertex(1, s) end)
        nVerts = (okV and nVerts) or 0
        for v = 1, nVerts do
            local okG, vert = dpCall(function() return meshD:getVertex(1, s, v) end)
            if okG and vert then
                local bestName, bestDistSq = nil, nil
                for name, seg in pairs(segments) do
                    local d2 = pointSegmentDistSq(vert.x, vert.y, vert.z, seg.ax, seg.ay, seg.az, seg.bx, seg.by, seg.bz)
                    if not bestDistSq or d2 < bestDistSq then
                        bestDistSq, bestName = d2, name
                    end
                end
                if bestName == targetBone.name and bestDistSq <= radiusSq then
                    table.insert(matched, { globalIndex = totalVerts + v })
                end
            end
        end
        totalVerts = totalVerts + nVerts
    end
    return matched
end

-- Fast path for when the rigid-bind target is already its own material subset (e.g. the sword was
-- a separate object/material in the source file): every vertex in that subset, no distance math.
local function findVerticesInSubset(meshD, subsetIndex)
    local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(1) end)
    nSubsets = (okS and nSubsets) or 0
    local totalVerts = 0
    local matched = {}
    for s = 1, nSubsets do
        local okV, nVerts = dpCall(function() return meshD:getTotalVertex(1, s) end)
        nVerts = (okV and nVerts) or 0
        if s == subsetIndex then
            for v = 1, nVerts do
                table.insert(matched, { globalIndex = totalVerts + v })
            end
        end
        totalVerts = totalVerts + nVerts
    end
    return matched
end

-- ---------------------------------------------------------------------------
-- Per-frame safety sweep, called unconditionally from onLoop (unlike showBonesNode below). A
-- loaded mesh's own top-level tree entry only stays expanded while it's the selected mesh
-- (showMeshTreeWindow's SetNextItemOpen(isSelected, ...)) -- the instant a DIFFERENT mesh becomes
-- selected, that entry's top-level TreeNodeEx collapses and everything nested inside it, including
-- showBonesNode, simply stops being called at all. showBonesNode's own open/close-transition logic
-- (destroying the gizmo, restoring preview visibility) therefore never gets a chance to run for a
-- mesh the user just switched away from, even though its tEntry.sOpenNode/bBonesWasOpen are still
-- 'bones'/true -- confirmed via direct user testing (both meshes' bone gizmos visible at once after
-- switching selection while the first mesh's Bones node was left open). This sweep independently
-- catches that: any entry whose gizmo is still marked open but is no longer the selected mesh gets
-- cleaned up here instead, regardless of whether showBonesNode itself ran this frame.
function sweepStaleBoneGizmos()
    for i, tEntry in ipairs(tLoadedMeshes) do
        -- The articulated pivot marker follows the selected mesh only. The selected mesh tree
        -- collapses immediately when another object is chosen, so its articulated node may not
        -- run again to perform the normal close cleanup.
        if tEntry.tArticulatedPivotGizmo and i ~= iSelectedMeshIndex then
            destroyArticulatedPivotGizmo(tEntry)
        end
        if tEntry.bBonesWasOpen and i ~= iSelectedMeshIndex then
            destroyBoneGizmo(tEntry)
            tEntry.bBonesWasOpen = false
        end
        -- Same leak, same fix, for the ghost mesh: it's a single global (only ever exists for the
        -- currently selected mesh), so this only ever actually destroys something for the one entry
        -- that owned it, but every entry needs the flag cleared so a later reselect properly rebuilds
        -- rather than being skipped by the transition check in showBonesNode.
        if tEntry.bGhostWasShown and i ~= iSelectedMeshIndex then
            destroyGhostMesh()
            tEntry.bGhostWasShown = false
        end
    end
end

-- ---------------------------------------------------------------------------
-- Bones tree node: view/add/edit/remove the mesh's optional skeleton
-- (SECTION_FRAME_SKINNED / meshDebug:addBone|getBone|updateBone|removeBone).
-- Editor/diagnostic data only -- never consulted by rendering (docs/mesh-v11-format.md Sec 6e).
-- Holds everything except the per-bone table itself (name/parent/position/radius/length/highlight),
-- which lives in the dedicated showBonesWindow instead -- that table alone is wide enough to want
-- the whole window's width, but Up axis/Apply Humanoid Armature/bake Rotate-Scale-Translate/Add
-- Bone are one-line-or-so controls that read naturally as a tree node, per direct user request.
-- ---------------------------------------------------------------------------
function showBonesNode(tEntry, meshD, index)
    local wantOpen = (tEntry.sOpenNode == 'bones')
    tImGui.SetNextItemOpen(wantOpen, tImGui.Flags('ImGuiCond_Always'))
    local isOpen = tImGui.TreeNodeEx(tLang.L('bones_node') .. '##bones-' .. index, 0)
    if tImGui.IsItemClicked() then
        tEntry.sOpenNode = wantOpen and nil or 'bones'
    end

    -- sOpenNode (and therefore isOpen, this tree node's own visual open/collapsed state) is tracked
    -- PER ENTRY and persists across a selection change -- selecting a different mesh does not
    -- implicitly collapse a previous mesh's still-expanded Bones node. But the 3D gizmo and the
    -- hidden-preview-mesh state must only ever exist for the currently SELECTED mesh (matching
    -- rebuildBoneGizmo's own `index ~= iSelectedMeshIndex` guard and tPreviewMesh always reflecting
    -- iSelectedMeshIndex) -- so gizmo/preview lifecycle below is driven by isOpen AND index ==
    -- iSelectedMeshIndex together, not isOpen alone. Without this, switching to a different mesh
    -- while an earlier mesh's Bones node was left open (never explicitly re-clicked closed) leaked
    -- that earlier mesh's gizmo forever: this function still runs (and still sees isOpen==true) for
    -- every loaded entry every frame, not just the selected one, confirmed via direct user testing
    -- (both meshes' bone gizmos visible simultaneously after switching selection).
    local gizmoShouldBeOpen = isOpen and (index == iSelectedMeshIndex)

    -- Hide the live preview mesh entirely while this node is open (per the user's own request),
    -- restored the moment it closes. Uses ONLY obj.visible/setEnableRender -- NEVER obj:setColor
    -- with numeric args here, confirmed via direct user testing (and by reading
    -- onSetTextureAnimationLua, src/lua-wrap/render-table/animation-lua.cpp:240) to be the actual
    -- root cause of a real, longstanding bug: obj:setColor(r,g,b,a) is not a multiplicative tint on
    -- top of the existing texture -- it converts the RGBA into a hex string and calls the *same*
    -- code path as obj:setTexture(), replacing the mesh's real diffuse texture with a synthetic
    -- solid-color one. setColor(1,1,1,1) does not mean "clear the tint," it means "swap in a plain
    -- solid white texture" -- the original texture reference is gone for good, not just multiplied
    -- by white. This bug predates this whole feature (the original 35%-alpha dim, which also called
    -- setColor with numbers, had it too) and independently affects anything else in this file that
    -- calls obj:setColor(number,...) on a real (non-placeholder) textured preview mesh.
    if index == iSelectedMeshIndex and tPreviewMesh then
        if gizmoShouldBeOpen then
            tPreviewMesh.visible = false
        elseif tEntry.bBonesWasOpen then
            tPreviewMesh.visible = true
        end
    end
    -- Gizmo geometry is rebuilt only on open/close transitions and after mutations (via
    -- onBonesEdit), never unconditionally every frame -- rebuilding involves shape:create() calls
    -- with freshly-generated nicknames for the cylinder links (see rebuildBoneGizmo's own comment),
    -- so doing that every single frame the node stays open would thrash the mesh-geometry cache for
    -- no reason.
    if gizmoShouldBeOpen and not tEntry.bBonesWasOpen then
        rebuildBoneGizmo(tEntry, meshD, index)
    elseif not gizmoShouldBeOpen and tEntry.bBonesWasOpen then
        destroyBoneGizmo(tEntry)
    end
    tEntry.bBonesWasOpen = gizmoShouldBeOpen

    -- "Show mesh" checkbox: an opt-in outlined mesh alongside the bone gizmo (only
    -- for 'mesh'/.msh entries -- isMesh3DType -- since sprites/tiles/etc render flat in 2D and this
    -- Bones/armature workflow targets 3D skeletal meshes). Drawn BEFORE the ghostShouldBeOpen check
    -- below (rather than down among the rest of this node's isOpen content) so a checkbox toggle
    -- this same frame is reflected immediately -- computing ghostShouldBeOpen from tEntry.bShowGhostMesh
    -- before the checkbox had a chance to update it would lag the create/destroy transition by one
    -- frame relative to what the user just clicked.
    if isOpen and isMesh3DType(tEntry) then
        tEntry.bShowGhostMesh = tImGui.Checkbox(tLang.L('bones_show_mesh_checkbox') .. '##showGhost-' .. index, tEntry.bShowGhostMesh or false)
        if tEntry.bShowGhostMesh then
            tEntry.tGhostOutlineColor = tEntry.tGhostOutlineColor or {r = 1.0, g = 0.9, b = 0.1}
            tEntry.fGhostOutlineThickness = tEntry.fGhostOutlineThickness or 0.12
            local colorChanged, color = tImGui.ColorEdit3(tLang.L('bones_mesh_outline_color') .. '##ghostOutlineColor-' .. index, tEntry.tGhostOutlineColor, 0)
            if colorChanged then
                tEntry.tGhostOutlineColor = color
                if tGhostMesh then applyGhostOutlineSettings(tGhostMesh, tEntry) end
            end
            tUtil.pushResponsiveItemWidth(160)
            local thicknessChanged, thickness = tImGui.SliderFloat(tLang.L('bones_mesh_outline_thickness') .. '##ghostOutlineThickness-' .. index, tEntry.fGhostOutlineThickness, 0.01, 0.5, '%.2f')
            tImGui.PopItemWidth()
            if thicknessChanged then
                tEntry.fGhostOutlineThickness = thickness
                if tGhostMesh then applyGhostOutlineSettings(tGhostMesh, tEntry) end
            end
        end
    end

    -- Axis-locked 3D drag/drop for bone/joint positions (direct user request): click-drag a bone's
    -- gizmo sphere in the 3D viewport to move it, constrained to X/Y (Z fixed) or Z/Y (X fixed) --
    -- never a free 3-axis drag, which is what made the original (removed) bone-drag feature easy to
    -- mis-place a joint with (pushing it too far forward/back along the hidden depth axis). The two
    -- checkboxes are mutually exclusive (enforced below, not a RadioButton per direct user request)
    -- and only meaningful with the 3D camera active -- same gating orbit itself already requires.
    -- Enabling either one clears any existing bone-table Highlight selection -- the Highlight
    -- checkbox column is repurposed while a drag plane is active as a live "which joint is under
    -- the cursor right now" hover indicator (see onTouchMove), so any stale selection from before
    -- would otherwise sit there misleadingly until the mouse next moves over the viewport.
    if isOpen and isMesh3DType(tEntry) and bCameraMode3D then
        tEntry.bSyncLeftRightBoneDrag = tImGui.Checkbox(tLang.L('bones_sync_left_right_drag_checkbox') .. '##boneSyncLeftRightDrag-' .. index,
            tEntry.bSyncLeftRightBoneDrag == true)
        local curPlane = tEntry.sBoneDragPlane
        local newXY = tImGui.Checkbox(tLang.L('bones_drag_xy_checkbox') .. '##boneDragXY-' .. index, curPlane == 'xy')
        if newXY and curPlane ~= 'xy' then
            tEntry.sBoneDragPlane = 'xy'
            curPlane = 'xy'
            snapCam3dToDragPlane(tEntry, 'xy')
            tEntry.tBoneHighlight = {}
            tEntry.sHoveredBoneName = nil
            rebuildBoneGizmo(tEntry, meshD, index)
        elseif not newXY and curPlane == 'xy' then
            tEntry.sBoneDragPlane = nil
            curPlane = nil
        end
        local newZY = tImGui.Checkbox(tLang.L('bones_drag_zy_checkbox') .. '##boneDragZY-' .. index, curPlane == 'zy')
        if newZY and curPlane ~= 'zy' then
            tEntry.sBoneDragPlane = 'zy'
            snapCam3dToDragPlane(tEntry, 'zy')
            tEntry.tBoneHighlight = {}
            tEntry.sHoveredBoneName = nil
            rebuildBoneGizmo(tEntry, meshD, index)
        elseif not newZY and curPlane == 'zy' then
            tEntry.sBoneDragPlane = nil
        end
    end

    -- Same open/selected gating as the gizmo itself (gizmoShouldBeOpen), ANDed with the checkbox
    -- just read above, so the ghost disappears the instant the node closes or a different mesh is
    -- selected -- exactly per the user's own request.
    local ghostShouldBeOpen = gizmoShouldBeOpen and tEntry.bShowGhostMesh and isMesh3DType(tEntry)
    if ghostShouldBeOpen and not tEntry.bGhostWasShown then
        rebuildGhostMesh(tEntry, index)
    elseif not ghostShouldBeOpen and tEntry.bGhostWasShown then
        destroyGhostMesh()
    end
    tEntry.bGhostWasShown = ghostShouldBeOpen

    if isOpen then
        tImGui.TextDisabled(tLang.L('bones_moved_to_window_label'))
        tImGui.HelpMarker(tLang.L('bones_transform_warning'))

        local tBones, nBones = getBoneList(meshD)

        tImGui.Separator()
        tEntry.iArmatureTemplateIndex = tEntry.iArmatureTemplateIndex or 1
        tImGui.TextDisabled(tLang.L('bones_armature_overwrite_label'))
        local armatureLabels = {}
        for i, t in ipairs(ARMATURE_TEMPLATES) do armatureLabels[i] = t.label end
        tUtil.pushResponsiveItemWidth(220)
        local chgArm, newArmIdx = tImGui.Combo('##armatureTemplate-' .. index, tEntry.iArmatureTemplateIndex, armatureLabels, -1)
        tImGui.PopItemWidth()
        if chgArm and newArmIdx then
            tEntry.iArmatureTemplateIndex = newArmIdx
        end
        -- Shared by the combo-selected "Apply Armature" button and "Import Armature" below -- both
        -- destructively replace the whole skeleton, so both go through the same existing-bones
        -- confirmation gate instead of each keeping their own copy of it.
        local function requestApplyArmature(tmpl)
            if nBones > 0 then
                tEntry.tPendingArmatureApply = tmpl
                tEntry.bBonesHumanoidConfirmPending = true
            else
                local okH, errH = dpCall(function() return applyArmatureTemplate(meshD, tmpl) end)
                if okH then onBonesEdit(tEntry, meshD, index) else tUtil.showMessageWarn(errH or tLang.L('an_error_occurred')) end
            end
        end

        if tEntry.bBonesHumanoidConfirmPending then
            tImGui.PushStyleColor('ImGuiCol_Text', {r = 1, g = 0.6, b = 0.2, a = 1})
            tImGui.TextWrapped(string.format(tLang.L('bones_apply_armature_confirm_fmt'), nBones))
            tImGui.PopStyleColor()
            if tImGui.Button(tLang.L('bones_apply_armature_button') .. '##boneArmatureConfirm-' .. index) then
                local tmpl = tEntry.tPendingArmatureApply
                tEntry.bBonesHumanoidConfirmPending = false
                tEntry.tPendingArmatureApply = nil
                if tmpl then
                    local okH, errH = dpCall(function() return applyArmatureTemplate(meshD, tmpl) end)
                    if okH then onBonesEdit(tEntry, meshD, index) else tUtil.showMessageWarn(errH or tLang.L('an_error_occurred')) end
                end
            end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel') .. '##boneArmatureCancel-' .. index) then
                tEntry.bBonesHumanoidConfirmPending = false
                tEntry.tPendingArmatureApply = nil
            end
        else
            if tImGui.Button(tLang.L('bones_apply_armature_button') .. '##boneArmature-' .. index) then
                requestApplyArmature(ARMATURE_TEMPLATES[tEntry.iArmatureTemplateIndex])
            end
        end

        -- Export/Import let the user capture a skeleton they've hand-fitted to one mesh (per
        -- applyArmatureTemplate's own "uniform scale only" caveat -- it doesn't know a target
        -- mesh's own limb proportions, only the reference's) and reapply it to other meshes, without
        -- needing a source-code change (ARMATURE_TEMPLATES) for every experiment.
        tImGui.Spacing()
        if tImGui.Button(tLang.L('bones_export_armature_button') .. '##boneArmatureExport-' .. index) then
            local defaultName = tUtil.getShortName(tEntry.fileName):gsub('%.[^%.]+$', '') .. '_armature.lua'
            local savePath = mbm.saveFile(defaultName, 'lua')
            if savePath then
                local okExp, errExp = exportArmatureToFile(meshD, savePath)
                if okExp then
                    tUtil.showMessage(string.format('%s: %s', tLang.L('bones_export_armature_button'), tUtil.getShortName(savePath)))
                else
                    tUtil.showMessageWarn(errExp or tLang.L('an_error_occurred'))
                end
            end
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('bones_import_armature_button') .. '##boneArmatureImport-' .. index) then
            local loadPath = mbm.openFile(sLastMeshPath, 'lua')
            if loadPath then
                if type(loadPath) == 'table' then loadPath = loadPath[1] end
                local tmpl, errImp = loadArmatureFromFile(loadPath)
                if tmpl then
                    requestApplyArmature(tmpl)
                else
                    tUtil.showMessageWarn(errImp or tLang.L('an_error_occurred'))
                end
            end
        end

        tImGui.Separator()
        tImGui.HelpMarker(tLang.L('bones_bake_xform_help'))
        tEntry.tBoneXformUI = tEntry.tBoneXformUI or { rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1, dx = 0, dy = 0, dz = 0 }
        local bxf = tEntry.tBoneXformUI

        tImGui.Text(tLang.L('rotate_xyz'))
        local chg_brx, brx = tImGui.DragFloat('X##bonesXfRx-' .. index, bxf.rx, 1.0, 0, 0, '%.1f')
        local chg_bry, bry = tImGui.DragFloat('Y##bonesXfRy-' .. index, bxf.ry, 1.0, 0, 0, '%.1f')
        local chg_brz, brz = tImGui.DragFloat('Z##bonesXfRz-' .. index, bxf.rz, 1.0, 0, 0, '%.1f')
        if chg_brx then bxf.rx = brx end
        if chg_bry then bxf.ry = bry end
        if chg_brz then bxf.rz = brz end
        if tImGui.Button(tLang.L('apply_rotation') .. '##bonesXfRotBtn-' .. index) then
            applyRotationToBonesDeg(meshD, bxf.rx, bxf.ry, bxf.rz)
            onBonesEdit(tEntry, meshD, index)
            bxf.rx, bxf.ry, bxf.rz = 0, 0, 0
        end

        tImGui.Spacing()
        tImGui.Text(tLang.L('scale_xyz'))
        local chg_bsx, bsx = tImGui.DragFloat('X##bonesXfSx-' .. index, bxf.sx, 0.01, 0, 0, '%.3f')
        local chg_bsy, bsy = tImGui.DragFloat('Y##bonesXfSy-' .. index, bxf.sy, 0.01, 0, 0, '%.3f')
        local chg_bsz, bsz = tImGui.DragFloat('Z##bonesXfSz-' .. index, bxf.sz, 0.01, 0, 0, '%.3f')
        if chg_bsx then bxf.sx = bsx end
        if chg_bsy then bxf.sy = bsy end
        if chg_bsz then bxf.sz = bsz end
        if tImGui.Button(tLang.L('apply_scale') .. '##bonesXfScaleBtn-' .. index) then
            applyScaleToBones(meshD, bxf.sx, bxf.sy, bxf.sz)
            onBonesEdit(tEntry, meshD, index)
            bxf.sx, bxf.sy, bxf.sz = 1, 1, 1
        end

        tImGui.Spacing()
        tImGui.Text(tLang.L('translate_xyz'))
        local chg_bdx, bdx = tImGui.DragFloat('X##bonesXfDx-' .. index, bxf.dx, 1.0, 0, 0, '%.1f')
        local chg_bdy, bdy = tImGui.DragFloat('Y##bonesXfDy-' .. index, bxf.dy, 1.0, 0, 0, '%.1f')
        local chg_bdz, bdz = tImGui.DragFloat('Z##bonesXfDz-' .. index, bxf.dz, 1.0, 0, 0, '%.1f')
        if chg_bdx then bxf.dx = bdx end
        if chg_bdy then bxf.dy = bdy end
        if chg_bdz then bxf.dz = bdz end
        if tImGui.Button(tLang.L('apply_translate') .. '##bonesXfTransBtn-' .. index) then
            applyTranslateToBones(meshD, bxf.dx, bxf.dy, bxf.dz)
            onBonesEdit(tEntry, meshD, index)
            bxf.dx, bxf.dy, bxf.dz = 0, 0, 0
        end

        tImGui.Separator()
        -- Batch version of the per-row Recompute button (showBonesWindow). By default only touches
        -- bones that actually need it (length <= EPS, the same threshold the length-warning marker
        -- uses), so a bone with real Blender-imported orientation data is never clobbered by the
        -- position-topology heuristic just because it happened to be in the same skeleton. The
        -- "even if Length is already set" checkbox opts into recomputing EVERY bone instead --
        -- direct user request, for the "applied a borrowed/Mixamo armature template, then manually
        -- dragged joints to fit this specific mesh" workflow: applyArmatureTemplate only does a
        -- uniform scale + reposition (it can't know this mesh's own limb proportions), and manually
        -- repositioning a joint afterward (the X/Y/Z drag fields) never touches Length/rotation --
        -- so every bone can end up with a real but now geometrically STALE length. Safe to force
        -- now that Recompute preserves each bone's existing roll instead of resetting it.
        tEntry.bRecomputeAllForce = tImGui.Checkbox(tLang.L('bones_recompute_all_force_checkbox') .. '##boneRecomputeAllForce-' .. index, tEntry.bRecomputeAllForce or false)
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text(tLang.L('bones_recompute_all_force_tooltip'))
            tImGui.EndTooltip()
        end
        if tImGui.Button(tLang.L('bones_recompute_all_button') .. '##boneRecomputeAll-' .. index) then
            local childrenByParent = computeChildrenByParent(tBones)
            local anyOk = false
            for _, b in ipairs(tBones) do
                if tEntry.bRecomputeAllForce or b.length <= 1e-6 then
                    local parentB = b.parentName and findBoneByName(tBones, b.parentName)
                    local ax, ay, az, newLength = computeAimAndLength(childrenByParent, b, parentB)
                    -- Preserve whatever roll the bone currently decodes to (relative to its OLD
                    -- aim) rather than resetting to the canonical 0 reference -- for a length<=0
                    -- bone this is usually a no-op (rotX/Y/Z was never real to begin with), but it's
                    -- never wrong, and keeps this in lockstep with the per-row Recompute fix below.
                    local curRoll = currentRollDeg(b.rotX, b.rotY, b.rotZ)
                    local nRotX, nRotY, nRotZ = eulerFromAimAndRoll(ax, ay, az, curRoll)
                    local okU = dpCall(function()
                        return meshD:updateBone(b.idx, b.name, b.parentName, b.x, b.y, b.z, b.radius,
                            nRotX, nRotY, nRotZ, b.scaleX, b.scaleY, b.scaleZ, newLength)
                    end)
                    anyOk = anyOk or okU
                end
            end
            if anyOk then onBonesEdit(tEntry, meshD, index) end
        end
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text(tLang.L('bones_recompute_all_tooltip'))
            tImGui.EndTooltip()
        end

        tImGui.Separator()
        tEntry.sBonesNewName = tEntry.sBonesNewName or ('Bone ' .. (nBones + 1))
        tUtil.pushResponsiveItemWidth(150)
        local _, newBoneName = tImGui.InputText('##boneNewName-' .. index, tEntry.sBonesNewName, 64, 0)
        tImGui.PopItemWidth()
        tEntry.sBonesNewName = newBoneName
        tImGui.SameLine()
        if tImGui.Button(tLang.L('bones_add_button') .. '##boneAdd-' .. index) then
            local nameToAdd = (tEntry.sBonesNewName ~= '' and tEntry.sBonesNewName) or ('Bone ' .. (nBones + 1))
            -- New bone starts at the viewport's current orbit focus point (converted from world space
            -- back to the mesh's own model space), so it appears where the user is already looking
            -- instead of always at the mesh origin.
            local fx, fy, fz = tEntry.cam3d and tEntry.cam3d.fx or 0, tEntry.cam3d and tEntry.cam3d.fy or 0, tEntry.cam3d and tEntry.cam3d.fz or 0
            local bx, by, bz = worldToBone(meshD, fx, fy, fz)
            local defaultParent = (#tBones > 0) and tBones[#tBones].name or nil
            local okA, err = dpCall(function() return meshD:addBone(nameToAdd, defaultParent, bx, by, bz, 2.0) end)
            if okA then
                onBonesEdit(tEntry, meshD, index)
                tEntry.sBonesNewName = 'Bone ' .. (nBones + 2)
            else
                tUtil.showMessageWarn(err or tLang.L('an_error_occurred'))
            end
        end

        -- Export to FBX moved to File menu (next to Import via Blender) -- it isn't specific to
        -- bones (a bone-less mesh still exports fine), so it doesn't belong buried in this node.

        tImGui.TreePop()
    end
end

-- ---------------------------------------------------------------------------
-- Articulated Animation node: one selected animated subset drives its Part/pivot editor,
-- the current Clip's Track/Keys, timeline preview and pivot gizmos.
-- ---------------------------------------------------------------------------
ARTICULATED_PIVOT_COLOR = {1, 0.55, 0.05, 0.95}

function destroyArticulatedPivotGizmo(tEntry)
    if tEntry.tArticulatedPivotGizmo then
        tEntry.tArticulatedPivotGizmo:destroy()
        tEntry.tArticulatedPivotGizmo = nil
    end
end

function updateArticulatedPivotGizmo(tEntry, meshD, index, totalParts)
    if index ~= iSelectedMeshIndex or not tEntry.bShowArticulatedPivot or totalParts == 0 then
        destroyArticulatedPivotGizmo(tEntry)
        return
    end
    tEntry.iArticulatedPart = math.max(1, math.min(tEntry.iArticulatedPart or 1, totalParts))
    local ok, partId, frame, subset, name, px, py, pz = dpCall(function()
        return meshD:getArticulatedPart(tEntry.iArticulatedPart)
    end)
    if not ok or not partId then
        destroyArticulatedPivotGizmo(tEntry)
        return
    end
    tEntry.fArticulatedPivotSizePercent = math.max(1, math.min(99,
        tEntry.fArticulatedPivotSizePercent or 5))
    if not tEntry.fArticulatedPivotMarkerSize or
        tEntry.fArticulatedPivotMarkerPartId ~= partId or
        tEntry.fArticulatedPivotMarkerPercent ~= tEntry.fArticulatedPivotSizePercent then
        local okFrames, totalFrames = dpCall(function() return meshD:getTotalFrame() end)
        totalFrames = (okFrames and totalFrames) or 1
        local targetFrame = (frame or 0) + 1
        if targetFrame > totalFrames then
            targetFrame = math.max(1, math.min(frame or 1, totalFrames))
        end
        local okSubsets, totalSubsets = dpCall(function() return meshD:getTotalSubset(targetFrame) end)
        totalSubsets = (okSubsets and totalSubsets) or 1
        local targetSubset = (subset or 0) + 1
        if targetSubset > totalSubsets then
            targetSubset = math.max(1, math.min(subset or 1, totalSubsets))
        end
        local bounds = computeMeshAABB(meshD, targetFrame, targetSubset)
        if not bounds then bounds = computeMeshAABB(meshD) end
        local maxExtent = bounds and math.max(bounds.maxX - bounds.minX,
            bounds.maxY - bounds.minY, bounds.maxZ - bounds.minZ) or 1
        tEntry.fArticulatedPivotMarkerSize = math.max(maxExtent *
            (tEntry.fArticulatedPivotSizePercent / 100), 0.01)
        tEntry.fArticulatedPivotMarkerPartId = partId
        tEntry.fArticulatedPivotMarkerPercent = tEntry.fArticulatedPivotSizePercent
    end
    if not tEntry.tArticulatedPivotGizmo then
        tEntry.iArticulatedPivotGizmoGeneration = (tEntry.iArticulatedPivotGizmoGeneration or 0) + 1
        local h = shape:new('3d', px, py, dodgeAutoZOrder(pz))
        h:create(unitSphereVerts(8, 12), nil,
            'mesh_debug_articulated_pivot_' .. index .. '_' .. tEntry.iArticulatedPivotGizmoGeneration)
        h:setColor(ARTICULATED_PIVOT_COLOR[1], ARTICULATED_PIVOT_COLOR[2],
            ARTICULATED_PIVOT_COLOR[3], ARTICULATED_PIVOT_COLOR[4])
        h.visible = true
        h.alwaysOnTop = true
        tEntry.tArticulatedPivotGizmo = h
    else
        tEntry.tArticulatedPivotGizmo:setPos(px, py, dodgeAutoZOrder(pz))
    end
    tEntry.tArticulatedPivotGizmo:setScale(tEntry.fArticulatedPivotMarkerSize,
        tEntry.fArticulatedPivotMarkerSize, tEntry.fArticulatedPivotMarkerSize)
end

function showArticulatedAnimationNode(tEntry, meshD, index)
    local isOpen = openNode(tEntry, 'articulated', tLang.L('articulated_animation'), 0, 'articulated-' .. index)
    if not isOpen then
        destroyArticulatedPivotGizmo(tEntry)
        destroyTransformSubsetHoverMarker(tEntry, 'articulated')
        return
    end
    local function markArticulatedEdit()
        tEntry.modified = true
        if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
    end
    local function articulatedTooltip(key)
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            -- Tooltip windows have no reliable wrap width in this ImGui binding;
            -- use explicit language newlines instead of wrapping one character at a time.
            tImGui.Text(tLang.L(key))
            tImGui.EndTooltip()
        end
    end

    tImGui.TextWrapped(tLang.L('articulated_help'))

    if tImGui.Button(tLang.L('articulated_initialize_parts') .. '##artInit-' .. index) then
        local ok, added = dpCall(function() return meshD:initializeArticulatedParts() end)
        if ok and added and added > 0 then markArticulatedEdit() end
    end
    articulatedTooltip('articulated_initialize_parts_tooltip')

    local okParts, totalParts = dpCall(function() return meshD:getTotalArticulatedParts() end)
    totalParts = (okParts and totalParts) or 0
    local articulatedPartIds = {}
    local articulatedParts = {}
    local articulatedPartOptions = {}
    for partIndex = 1, totalParts do
        local okPart, partId, frame, subset, name, px, py, pz, qx, qy, qz, qw, parent = dpCall(function()
            return meshD:getArticulatedPart(partIndex)
        end)
        if okPart and partId then
            articulatedPartIds[partIndex] = partId
            articulatedParts[partIndex] = {
                partId = partId, frame = frame, subset = subset, name = name,
                px = px, py = py, pz = pz, qx = qx, qy = qy, qz = qz, qw = qw,
                parent = parent
            }
            articulatedPartOptions[partIndex] = string.format('F%d S%d - %s',
                frame or 0, subset or 0, (name and name ~= '') and name or ('Part ' .. partIndex))
        else
            articulatedPartOptions[partIndex] = 'Part ' .. partIndex
        end
    end
    if totalParts > 0 then
        if not tEntry.bArticulatedRemovePartsConfirm then
            if tImGui.Button(tLang.L('articulated_remove_parts') .. '##artRemoveParts-' .. index) then
                tEntry.bArticulatedRemovePartsConfirm = true
            end
        else
            tImGui.TextWrapped(tLang.L('articulated_remove_parts_confirm'))
            if tImGui.Button('Confirm##artRemovePartsConfirm-' .. index) then
                local okRemove = dpCall(function() return meshD:removeArticulatedParts() end)
                if okRemove then
                    tEntry.bArticulatedRemovePartsConfirm = false
                    tEntry.iArticulatedPart = 1
                    markArticulatedEdit()
                    totalParts = 0
                    articulatedPartIds = {}
                    articulatedParts = {}
                    articulatedPartOptions = {}
                end
            end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel') .. '##artRemovePartsCancel-' .. index) then
                tEntry.bArticulatedRemovePartsConfirm = false
            end
        end
    end
    tEntry.bShowArticulatedPivot = tImGui.Checkbox('Show Pivot Gizmo##artPivotShow-' .. index,
        tEntry.bShowArticulatedPivot ~= false)
    articulatedTooltip('articulated_show_pivot_tooltip')
    tImGui.SameLine()
    tImGui.PushItemWidth(125)
    local pivotSizeChanged, pivotSize = tImGui.SliderFloat(
        tLang.L('articulated_pivot_size') .. '##artPivotSize-' .. index,
        tEntry.fArticulatedPivotSizePercent or 5, 1, 99, '%.0f%%')
    tImGui.PopItemWidth()
    if pivotSizeChanged then
        tEntry.fArticulatedPivotSizePercent = math.max(1, math.min(99, pivotSize or 10))
    end
    articulatedTooltip('articulated_pivot_size_tooltip')
    if totalParts > 0 then
        tEntry.iArticulatedPart = math.max(1,
            math.min(tEntry.iArticulatedPart or 1, totalParts))
        local okFrames, totalFrames = dpCall(function() return meshD:getTotalFrame() end)
        totalFrames = (okFrames and totalFrames) or 1
        local currentPartInfo = articulatedParts[tEntry.iArticulatedPart]
        tEntry.iArticulatedFrame = math.max(1, math.min(tEntry.iArticulatedFrame or
            (currentPartInfo and currentPartInfo.frame) or 1, totalFrames))

        tImGui.PushItemWidth(100)
        local frameChanged, selectedFrame = tImGui.InputInt(
            tLang.L('frame_selection') .. '##artFrame-' .. index,
            tEntry.iArticulatedFrame, 1, 1, 0)
        tImGui.PopItemWidth()
        if frameChanged then
            tEntry.iArticulatedFrame = math.max(1, math.min(selectedFrame or 1, totalFrames))
        end

        local framePartOptions, framePartIndices = {}, {}
        local frameComboIndex = 1
        for partIndex = 1, totalParts do
            local partInfo = articulatedParts[partIndex]
            if partInfo and partInfo.frame == tEntry.iArticulatedFrame then
                framePartOptions[#framePartOptions + 1] = articulatedPartOptions[partIndex]
                framePartIndices[#framePartIndices + 1] = partIndex
                if partIndex == tEntry.iArticulatedPart then frameComboIndex = #framePartIndices end
            end
        end
        if #framePartIndices > 0 then
            local selectedIsInFrame = articulatedParts[tEntry.iArticulatedPart] and
                articulatedParts[tEntry.iArticulatedPart].frame == tEntry.iArticulatedFrame
            if not selectedIsInFrame then
                tEntry.iArticulatedPart = framePartIndices[1]
                frameComboIndex = 1
            end
            tImGui.PushItemWidth(260)
            local partChanged, selectedFramePart = tImGui.Combo(
                tLang.L('articulated_animated_subset') .. '##artPivotPart-' .. index,
                frameComboIndex, framePartOptions, -1)
            tImGui.PopItemWidth()
            if partChanged and selectedFramePart then
                tEntry.iArticulatedPart = framePartIndices[selectedFramePart] or tEntry.iArticulatedPart
            end
            articulatedTooltip('articulated_part_tooltip')

            local hoveredSubset = nil
            local okSubsets, totalSubsets = dpCall(function()
                return meshD:getTotalSubset(tEntry.iArticulatedFrame)
            end)
            totalSubsets = (okSubsets and totalSubsets) or 0
            local tableFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg')
            if tImGui.BeginTable('artSubsetTable-' .. index, 2, tableFlags) then
                tImGui.TableSetupColumn(tLang.L('subset'))
                tImGui.TableSetupColumn(tLang.L('select'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 90)
                tImGui.TableHeadersRow()
                for subset = 1, totalSubsets do
                    local partForSubset = nil
                    for _, candidatePartIndex in ipairs(framePartIndices) do
                        if articulatedParts[candidatePartIndex].subset == subset then
                            partForSubset = candidatePartIndex
                            break
                        end
                    end
                    tImGui.TableNextRow()
                    tImGui.TableSetColumnIndex(0)
                    tImGui.Text(string.format('%s %d', tLang.L('subset'), subset))
                    if tImGui.IsItemHovered(0) then hoveredSubset = subset end
                    tImGui.TableSetColumnIndex(1)
                    tImGui.BeginDisabled(not partForSubset)
                    local selectPressed = tImGui.Button(tLang.L('select') ..
                        '##artSubsetSelect-' .. index .. '-' .. tEntry.iArticulatedFrame .. '-' .. subset)
                    if tImGui.IsItemHovered(0) then hoveredSubset = subset end
                    tImGui.EndDisabled()
                    if selectPressed and partForSubset then tEntry.iArticulatedPart = partForSubset end
                end
                tImGui.EndTable()
            end
            if hoveredSubset then
                tEntry.tArticulatedSubsetMarkerTransform = tEntry.tArticulatedSubsetMarkerTransform or
                    {rx=0, ry=0, rz=0, sx=1, sy=1, sz=1, dx=0, dy=0, dz=0}
                updateTransformSubsetHoverMarker(tEntry, meshD, tEntry.iArticulatedFrame,
                    hoveredSubset, tEntry.tArticulatedSubsetMarkerTransform, index, 'articulated')
            else
                destroyTransformSubsetHoverMarker(tEntry, 'articulated')
            end
        else
            tImGui.TextDisabled(tLang.L('articulated_no_parts'))
            destroyTransformSubsetHoverMarker(tEntry, 'articulated')
        end
        if tImGui.Button('Open Pivot Gizmo##artPivotWindow-' .. index) then
            tEntry.bArticulatedPivotWindow = true
        end
        articulatedTooltip('articulated_open_pivot_tooltip')
    end
    updateArticulatedPivotGizmo(tEntry, meshD, index, totalParts)
    tImGui.Separator()
    if totalParts == 0 then
        tImGui.TextDisabled(tLang.L('articulated_no_parts'))
    else
        local partIndex = tEntry.iArticulatedPart
        local selectedPartInfo = articulatedParts[partIndex]
        if selectedPartInfo then
            local partId = selectedPartInfo.partId
            local frame, subset, name = selectedPartInfo.frame, selectedPartInfo.subset, selectedPartInfo.name
            local px, py, pz = selectedPartInfo.px, selectedPartInfo.py, selectedPartInfo.pz
            local qx, qy, qz, qw = selectedPartInfo.qx, selectedPartInfo.qy,
                selectedPartInfo.qz, selectedPartInfo.qw
            local parent = selectedPartInfo.parent
            tImGui.PushID('artPart-' .. index .. '-' .. partIndex)
            tImGui.Text(tLang.L('articulated_selected_subset'))
            tImGui.Text(string.format('Frame %d   Subset %d   Part ID %s',
                frame or 0, subset or 0, tostring(partId)))
            tImGui.PushItemWidth(220)
            local changedName, newName = tImGui.InputText(tLang.L('name'), name or '', 96, 0)
            tImGui.PopItemWidth()
            tImGui.PushItemWidth(220)
            local posChanged, pos = tImGui.DragFloat3(tLang.L('articulated_pivot_position'),
                {px or 0, py or 0, pz or 0}, 0.01, -math.huge, math.huge, '%.3f', 0)
            tImGui.PopItemWidth()
            local pivotOrbit = articulatedOrbitFromQuaternion(qx or 0, qy or 0, qz or 0, qw or 1)
            tImGui.PushItemWidth(220)
            local rotChanged, rot = tImGui.DragFloat3(
                tLang.L('articulated_pivot_rotation') .. '##artPivotEuler-' .. index .. '-' .. partIndex,
                {-(pivotOrbit.elevation or 0) * 180 / math.pi,
                    (pivotOrbit.azimuth or 0) * 180 / math.pi,
                    (pivotOrbit.roll or 0) * 180 / math.pi},
                0.5, -360, 360, '%.2f', 0)
            tImGui.PopItemWidth()
            if rotChanged and rot then
                pivotOrbit.elevation = -(rot[1] or 0) * math.pi / 180
                pivotOrbit.azimuth = (rot[2] or 0) * math.pi / 180
                pivotOrbit.roll = (rot[3] or 0) * math.pi / 180
                qx, qy, qz, qw = articulatedQuaternionFromOrbit(pivotOrbit)
            end
            local parentOptions = {tLang.L('none')}
            local parentPartIndices = {0}
            local parentComboIndex = 1
            for candidateIndex = 1, totalParts do
                if candidateIndex ~= partIndex then
                    parentOptions[#parentOptions + 1] = articulatedPartOptions[candidateIndex]
                    parentPartIndices[#parentPartIndices + 1] = candidateIndex
                    if parent and parent ~= 0 and articulatedPartIds[candidateIndex] == parent then
                        parentComboIndex = #parentOptions
                    end
                end
            end
            tImGui.PushItemWidth(260)
            local parentChanged, newParentComboIndex = tImGui.Combo(
                tLang.L('articulated_parent_part') .. '##artParent-' .. index .. '-' .. partIndex,
                parentComboIndex, parentOptions, -1)
            tImGui.PopItemWidth()
            local parentIndex = parentPartIndices[
                (parentChanged and newParentComboIndex) or parentComboIndex] or 0
            articulatedTooltip('articulated_parent_part_tooltip')
            local parentPartId = articulatedPartIds[parentIndex] or 0
            if changedName or posChanged or rotChanged or parentChanged then
                local p = pos or {px or 0, py or 0, pz or 0}
                dpCall(function()
                    return meshD:updateArticulatedPart(partIndex, newName or name or '',
                        p[1], p[2], p[3], qx or 0, qy or 0, qz or 0, qw or 1, parentPartId)
                end)
                markArticulatedEdit()
            end
            tImGui.PopID()
            tImGui.Separator()
        end
    end

    local okClips, totalClips = dpCall(function() return meshD:getTotalArticulatedAnimations() end)
    totalClips = (okClips and totalClips) or 0
    tImGui.Text(string.format('%s: %d', tLang.L('articulated_clips_label'), totalClips))
    if tImGui.Button(tLang.L('articulated_add_clip') .. '##artClipAdd-' .. index) then
        local clipName = 'Articulated ' .. (totalClips + 1)
        local ok = dpCall(function()
            return meshD:addArticulatedAnimation(clipName, 1.0, 1.0, 0, true, 0)
        end)
        if ok then markArticulatedEdit() end
    end
    articulatedTooltip('articulated_add_clip_tooltip')
    if totalClips > 0 then
        if not tEntry.bArticulatedRemoveClipConfirm then
            if tImGui.Button(tLang.L('articulated_remove_clip') .. '##artRemoveClip-' .. index) then
                tEntry.bArticulatedRemoveClipConfirm = true
            end
        else
            tImGui.TextWrapped(tLang.L('articulated_remove_clip_confirm'))
            if tImGui.Button('Confirm##artRemoveClipConfirm-' .. index) then
                local removeIndex = math.max(1, math.min(tEntry.iArticulatedClip or 1, totalClips))
                local okRemove = dpCall(function()
                    return meshD:removeArticulatedAnimation(removeIndex)
                end)
                if okRemove then
                    tEntry.bArticulatedRemoveClipConfirm = false
                    totalClips = totalClips - 1
                    tEntry.iArticulatedClip = math.max(1, math.min(removeIndex, math.max(1, totalClips)))
                    markArticulatedEdit()
                end
            end
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel') .. '##artRemoveClipCancel-' .. index) then
                tEntry.bArticulatedRemoveClipConfirm = false
            end
        end
    end
    if totalClips == 0 then
        tImGui.TextDisabled(tLang.L('articulated_no_clips'))
    else
        tEntry.iArticulatedClip = math.max(1, math.min(tEntry.iArticulatedClip or 1, totalClips))
        local partSubsetById = {}
        for partIndex = 1, totalParts do
            local partInfo = articulatedParts[partIndex]
            if partInfo and partInfo.partId then
                partSubsetById[partInfo.partId] = partInfo.subset
            end
        end
        local clipTableFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg')
        if tImGui.BeginTable('artClipTable-' .. index, 3, clipTableFlags) then
            tImGui.TableSetupColumn(tLang.L('select'),
                tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 55)
            tImGui.TableSetupColumn(tLang.L('articulated_clip'))
            tImGui.TableSetupColumn(tLang.L('articulated_affected_subsets'))
            tImGui.TableHeadersRow()
            for clipIndex = 1, totalClips do
                local ok, clipName = dpCall(function()
                    return meshD:getArticulatedAnimationName(clipIndex)
                end)
                local affectedSubsets = {}
                local affectedSubsetSet = {}
                local okTracks, totalClipTracks = dpCall(function()
                    return meshD:getTotalArticulatedTracks(clipIndex)
                end)
                totalClipTracks = (okTracks and totalClipTracks) or 0
                for trackIndex = 1, totalClipTracks do
                    local okTrack, trackPartId = dpCall(function()
                        return meshD:getArticulatedTrack(clipIndex, trackIndex)
                    end)
                    local subset = okTrack and partSubsetById[trackPartId] or nil
                    if subset and not affectedSubsetSet[subset] then
                        affectedSubsetSet[subset] = true
                        affectedSubsets[#affectedSubsets + 1] = subset
                    end
                end
                table.sort(affectedSubsets)

                tImGui.TableNextRow()
                tImGui.TableSetColumnIndex(0)
                tEntry.iArticulatedClip = tImGui.RadioButton(
                    '##artClipSelect-' .. index .. '-' .. clipIndex,
                    tEntry.iArticulatedClip, clipIndex)
                articulatedTooltip('articulated_clip_selector_tooltip')
                tImGui.TableSetColumnIndex(1)
                tImGui.Text((ok and clipName) or ('Clip ' .. clipIndex))
                tImGui.TableSetColumnIndex(2)
                tImGui.Text(#affectedSubsets > 0 and table.concat(affectedSubsets, ',') or '-')
            end
            tImGui.EndTable()
        end

        local activeClip = tEntry.iArticulatedClip
        local okInfo, infoName, infoDuration, infoSpeed, infoPriority, infoLoop, infoBlendMode = dpCall(function()
            return meshD:getArticulatedAnimation(activeClip)
        end)
        if okInfo and infoName then
            tEntry.tArticulatedClipNameEdits = tEntry.tArticulatedClipNameEdits or {}
            local nameEdit = tEntry.tArticulatedClipNameEdits[activeClip]
            if not nameEdit or nameEdit.engineName ~= infoName then
                nameEdit = {value = infoName, engineName = infoName}
                tEntry.tArticulatedClipNameEdits[activeClip] = nameEdit
            end
            tImGui.PushItemWidth(220)
            local nameChanged, newName = tImGui.InputText(
                'Clip Name##artClipName-' .. index .. '-' .. activeClip, nameEdit.value, 96, 0)
            local nameDeactivatedAfterEdit = tImGui.IsItemDeactivatedAfterEdit()
            tImGui.PopItemWidth()
            if newName ~= nil then
                nameEdit.value = newName
            end
            articulatedTooltip('articulated_clip_name_tooltip')
            tImGui.PushItemWidth(115)
            local durationChanged, newDuration = tImGui.InputFloat('Duration', infoDuration or 0, 0.01, 0.1, '%.3f', 0)
            tImGui.PopItemWidth()
            articulatedTooltip('articulated_duration_tooltip')
            tImGui.PushItemWidth(115)
            local speedChanged, newSpeed = tImGui.InputFloat('Speed', infoSpeed or 1, 0.01, 0.1, '%.3f', 0)
            tImGui.PopItemWidth()
            articulatedTooltip('articulated_speed_tooltip')
            tImGui.PushItemWidth(80)
            local priorityChanged, newPriority = tImGui.InputInt('Priority', infoPriority or 0, 1, 10, 0)
            tImGui.PopItemWidth()
            articulatedTooltip('articulated_priority_tooltip')
            local newLoop = tImGui.Checkbox('Loop', infoLoop == true)
            articulatedTooltip('articulated_loop_tooltip')
            local loopChanged = newLoop ~= (infoLoop == true)
            local blendModeOptions = {
                tLang.L('articulated_blend_absolute'),
                tLang.L('articulated_blend_additive')
            }
            tImGui.PushItemWidth(115)
            local blendModeChanged, blendModeIndex = tImGui.Combo(
                tLang.L('articulated_blend_mode') .. '##artBlendMode-' .. index,
                math.max(1, math.min((infoBlendMode or 0) + 1, #blendModeOptions)),
                blendModeOptions, -1)
            tImGui.PopItemWidth()
            articulatedTooltip('articulated_blend_mode_tooltip')
            local newBlendMode = blendModeChanged and blendModeIndex and (blendModeIndex - 1)
                or (infoBlendMode or 0)
            if nameChanged or nameDeactivatedAfterEdit or durationChanged or speedChanged or
                priorityChanged or loopChanged or blendModeChanged then
                local okUpdate = dpCall(function()
                    return meshD:updateArticulatedAnimation(activeClip, nameEdit.value or infoName,
                        math.max(0, newDuration or infoDuration or 0), newSpeed or infoSpeed or 1,
                        newPriority or infoPriority or 0, newLoop == true, newBlendMode)
                end)
                if okUpdate then
                    nameEdit.engineName = nameEdit.value
                    markArticulatedEdit()
                end
            end
            local previewReady = index == iSelectedMeshIndex and tPreviewMesh and not tEntry.modified
            local timelineMin, timelineMax
            if previewReady then
                tEntry.fArticulatedBlendDuration = math.max(0, tEntry.fArticulatedBlendDuration or 0)
                tImGui.PushItemWidth(115)
                local blendChanged, blendDuration = tImGui.DragFloat(
                    tLang.L('articulated_blend_time') .. '##artBlend-' .. index,
                    tEntry.fArticulatedBlendDuration, 0.01, 0, 60, '%.3f', 0)
                tImGui.PopItemWidth()
                if blendChanged and blendDuration ~= nil then
                    tEntry.fArticulatedBlendDuration = math.max(0, blendDuration)
                end
                articulatedTooltip('articulated_blend_time_tooltip')
                tEntry.fArticulatedPreviewWeight = math.max(0,
                    math.min(1, tEntry.fArticulatedPreviewWeight or 1))
                if newBlendMode == 1 then
                    tImGui.PushItemWidth(115)
                    local weightChanged, previewWeight = tImGui.DragFloat(
                        tLang.L('articulated_preview_weight') .. '##artWeight-' .. index,
                        tEntry.fArticulatedPreviewWeight, 0.01, 0, 1, '%.3f', 0)
                    tImGui.PopItemWidth()
                    if weightChanged and previewWeight ~= nil then
                        tEntry.fArticulatedPreviewWeight = math.max(0, math.min(1, previewWeight))
                    end
                    articulatedTooltip('articulated_preview_weight_tooltip')
                end
                local okCurrentTime, currentTime = dpCall(function()
                    return tPreviewMesh:getArticulatedAnimationTime(infoName)
                end)
                if okCurrentTime and currentTime ~= nil then
                    tEntry.fArticulatedPreviewTime = currentTime
                end
                tEntry.fArticulatedPreviewTime = math.max(0, math.min(tEntry.fArticulatedPreviewTime or 0,
                    math.max(0, infoDuration or 0)))
                tImGui.PushItemWidth(260)
                local seekChanged, seekTime = tImGui.SliderFloat('Timeline##artTimeline-' .. index,
                    tEntry.fArticulatedPreviewTime, 0, math.max(0.001, infoDuration or 0), '%.3f')
                tImGui.PopItemWidth()
                timelineMin = tImGui.GetItemRectMin()
                timelineMax = tImGui.GetItemRectMax()
                if seekChanged then
                    tEntry.fArticulatedPreviewTime = seekTime or tEntry.fArticulatedPreviewTime
                    dpCall(function()
                        return tPreviewMesh:seekArticulatedAnimation(infoName, tEntry.fArticulatedPreviewTime)
                    end)
                end
                articulatedTooltip('articulated_timeline_tooltip')
                if tImGui.Button('Play##artPlay-' .. index) then
                    dpCall(function()
                        return tPreviewMesh:playArticulatedAnimation(
                            infoName, infoPriority or 0, tEntry.fArticulatedBlendDuration or 0,
                            newBlendMode == 1 and (tEntry.fArticulatedPreviewWeight or 1) or 1)
                    end)
                end
                articulatedTooltip('articulated_playback_tooltip')
                tImGui.SameLine()
                if tImGui.Button('Pause##artPause-' .. index) then
                    dpCall(function() return tPreviewMesh:pauseArticulatedAnimation(infoName) end)
                end
                articulatedTooltip('articulated_playback_tooltip')
                tImGui.SameLine()
                if tImGui.Button('Resume##artResume-' .. index) then
                    dpCall(function() return tPreviewMesh:resumeArticulatedAnimation(infoName) end)
                end
                articulatedTooltip('articulated_playback_tooltip')
                tImGui.SameLine()
                if tImGui.Button('Disable##artDisable-' .. index) then
                    dpCall(function() return tPreviewMesh:disableArticulatedAnimation(infoName) end)
                end
                articulatedTooltip('articulated_playback_tooltip')
            elseif index == iSelectedMeshIndex then
                tImGui.TextDisabled(tLang.L('articulated_save_to_preview'))
            end
        end

        tImGui.Separator()
        tImGui.NewLine()
        tImGui.Text(tLang.L('articulated_tracks'))
        local selectedPartInfo = articulatedParts[tEntry.iArticulatedPart or 1]
        local selectedPartId = selectedPartInfo and selectedPartInfo.partId or nil
        local okTracks, totalTracks = dpCall(function()
            return meshD:getTotalArticulatedTracks(activeClip)
        end)
        totalTracks = (okTracks and totalTracks) or 0
        local selectedTrackIndex = nil
        for candidateTrackIndex = 1, totalTracks do
            local okTrack, trackPartId = dpCall(function()
                return meshD:getArticulatedTrack(activeClip, candidateTrackIndex)
            end)
            if okTrack and trackPartId == selectedPartId then
                selectedTrackIndex = candidateTrackIndex
                break
            end
        end
        if selectedPartInfo then
            tImGui.Text(string.format('F%d S%d - %s - Part ID %s',
                selectedPartInfo.frame or 0, selectedPartInfo.subset or 0,
                selectedPartInfo.name or '', tostring(selectedPartId)))
        end
        if selectedPartId and not selectedTrackIndex then
            tImGui.Text(tLang.L('articulated_new_track_channels'))
            tEntry.bArticulatedPosition = tEntry.bArticulatedPosition ~= false
            tEntry.bArticulatedRotation = tEntry.bArticulatedRotation ~= false
            tEntry.bArticulatedScale = tEntry.bArticulatedScale ~= false
            local channelPosition = tImGui.Checkbox(
                'Position##artChannel-' .. index, tEntry.bArticulatedPosition)
            tEntry.bArticulatedPosition = channelPosition
            tImGui.SameLine()
            local channelRotation = tImGui.Checkbox(
                'Rotation##artChannel-' .. index, tEntry.bArticulatedRotation)
            tEntry.bArticulatedRotation = channelRotation
            tImGui.SameLine()
            local channelScale = tImGui.Checkbox(
                'Scale##artChannel-' .. index, tEntry.bArticulatedScale)
            tEntry.bArticulatedScale = channelScale
            articulatedTooltip('articulated_channel_tooltip')
            local selectedMask = (channelPosition and 1 or 0) +
                (channelRotation and 2 or 0) + (channelScale and 4 or 0)
            if tImGui.Button(tLang.L('articulated_add_track') .. '##addSelected-' .. index) and
                selectedMask ~= 0 then
                local okTrack = dpCall(function()
                    return meshD:addArticulatedTrack(activeClip, selectedPartId, selectedMask)
                end)
                if okTrack then markArticulatedEdit() end
            end
            articulatedTooltip('articulated_add_track_tooltip')
        end

        tImGui.Separator()

        if selectedTrackIndex then
            local trackIndex = selectedTrackIndex
            local okTrack, trackPartId, channelMask, keyCount = dpCall(function()
                return meshD:getArticulatedTrack(activeClip, trackIndex)
            end)
            if okTrack and trackPartId then
                tImGui.PushID('artTrack-' .. index .. '-' .. trackIndex)
                tImGui.Text(string.format('Track %d  Keys: %d', trackIndex, keyCount or 0))
                local trackPosition = tImGui.Checkbox(
                    'Position##artTrackPosition', ((channelMask or 0) & 1) ~= 0)
                tImGui.SameLine()
                local trackRotation = tImGui.Checkbox(
                    'Rotation##artTrackRotation', ((channelMask or 0) & 2) ~= 0)
                tImGui.SameLine()
                local trackScale = tImGui.Checkbox(
                    'Scale##artTrackScale', ((channelMask or 0) & 4) ~= 0)
                articulatedTooltip('articulated_channel_tooltip')
                local updatedChannelMask = (trackPosition and 1 or 0) +
                    (trackRotation and 2 or 0) + (trackScale and 4 or 0)
                if updatedChannelMask ~= (channelMask or 0) then
                    if updatedChannelMask == 0 then
                        tUtil.showMessageWarn(tLang.L('articulated_channel_required'))
                    else
                        local okChannels = dpCall(function()
                            return meshD:setArticulatedTrackChannels(
                                activeClip, trackIndex, updatedChannelMask)
                        end)
                        if okChannels then markArticulatedEdit() end
                    end
                end
                tImGui.PushItemWidth(100)
                local newKeyTimeChanged, newKeyTime = tImGui.InputFloat(
                    tLang.L('articulated_new_key_time') .. '##newKeyTime',
                    tEntry.fArticulatedNewKeyTime or 0, 0.01, 0.1, '%.3f', 0)
                tImGui.PopItemWidth()
                if newKeyTimeChanged then
                    tEntry.fArticulatedNewKeyTime = math.max(0, newKeyTime or 0)
                end
                articulatedTooltip('articulated_new_key_time_tooltip')
                tImGui.SameLine()
                if tImGui.Button(tLang.L('articulated_add_key') .. '##addKey') then
                    local okKey = dpCall(function()
                        return meshD:addArticulatedKey(activeClip, trackIndex, tEntry.fArticulatedNewKeyTime or 0,
                            0, 0, 0, 0, 0, 0, 1, 1, 1, 1)
                    end)
                    if okKey then
                        dpCall(function()
                            return meshD:setArticulatedKeyEuler(activeClip, trackIndex,
                                tEntry.fArticulatedNewKeyTime or 0, 0, 0, 0)
                        end)
                        markArticulatedEdit()
                    end
                end
                articulatedTooltip('articulated_add_key_tooltip')
                for keyIndex = 1, (keyCount or 0) do
                    local okKey, keyTime, px, py, pz, qx, qy, qz, qw, sx, sy, sz,
                        keyEasing, bezierX1, bezierY1, bezierX2, bezierY2,
                        authoredEulerX, authoredEulerY, authoredEulerZ,
                        hasAuthoredEuler = dpCall(function()
                        return meshD:getArticulatedKey(activeClip, trackIndex, keyIndex)
                    end)
                    if okKey and keyTime then
                        tImGui.Separator()
                        tImGui.Text(string.format(tLang.L('articulated_key_time_fmt'), trackIndex, keyIndex))
                        if timelineMin and timelineMax and (infoDuration or 0) > 0 then
                            local markerX = timelineMin.x + (timelineMax.x - timelineMin.x) *
                                math.max(0, math.min(1, keyTime / infoDuration))
                            tImGui.AddLine({x = markerX, y = timelineMin.y},
                                {x = markerX, y = timelineMax.y}, {r = 1, g = 0.75, b = 0.1, a = 1}, 2)
                        end
                        tImGui.PushID('artKey-' .. index .. '-' .. trackIndex .. '-' .. keyIndex)
                        tImGui.PushItemWidth(100)
                        local timeChanged, newTime = tImGui.InputFloat('Time',keyTime, 0.01, 0.1, '%.3f', 0)
                        tImGui.PopItemWidth()
                        articulatedTooltip('articulated_key_tooltip')
                        tImGui.SameLine()
                        if tImGui.Button('Remove##removeKey') then
                            local okRemove = dpCall(function()
                                return meshD:removeArticulatedKey(activeClip, trackIndex, keyIndex)
                            end)
                            if okRemove then
                                markArticulatedEdit()
                                tImGui.PopID()
                                break
                            end
                        end
                        local keyId = index .. '-' .. trackIndex .. '-' .. keyIndex
                        tEntry.tArticulatedKeyEuler = tEntry.tArticulatedKeyEuler or {}
                        local keyEuler = tEntry.tArticulatedKeyEuler[keyId]
                        local easingOptions = getArticulatedEasingOptions()
                        local easingIndex = math.max(1, math.min((keyEasing or 0) + 1, #easingOptions))
                        tImGui.PushItemWidth(150)
                        local easingChanged, newEasingIndex = tImGui.Combo(
                            tLang.L('articulated_easing') .. '##artKeyEasing-' .. keyId,
                            easingIndex, easingOptions, -1)
                        tImGui.PopItemWidth()
                        articulatedTooltip('articulated_easing_tooltip')
                        local newEasing = (easingChanged and newEasingIndex and newEasingIndex > 0)
                            and (newEasingIndex - 1) or (keyEasing or 0)
                        local bezierChanged = false
                        local newBezierX1, newBezierY1 = bezierX1 or 0.25, bezierY1 or 0.25
                        local newBezierX2, newBezierY2 = bezierX2 or 0.75, bezierY2 or 0.75
                        if newEasing == 5 then
                            tImGui.PushItemWidth(150)
                            local p1Changed, p1 = tImGui.DragFloat2(
                                tLang.L('articulated_bezier_p1') .. '##artBezierP1-' .. keyId,
                                {newBezierX1, newBezierY1}, 0.01, 0, 0, '%.3f', 0)
                            tImGui.PopItemWidth()
                            articulatedTooltip('articulated_bezier_points_tooltip')
                            tImGui.PushItemWidth(150)
                            local p2Changed, p2 = tImGui.DragFloat2(
                                tLang.L('articulated_bezier_p2') .. '##artBezierP2-' .. keyId,
                                {newBezierX2, newBezierY2}, 0.01, 0, 0, '%.3f', 0)
                            tImGui.PopItemWidth()
                            articulatedTooltip('articulated_bezier_points_tooltip')
                            if p1Changed and p1 then
                                newBezierX1 = math.max(0, math.min(1, p1[1] or newBezierX1))
                                newBezierY1 = p1[2] or newBezierY1
                            end
                            if p2Changed and p2 then
                                newBezierX2 = math.max(0, math.min(1, p2[1] or newBezierX2))
                                newBezierY2 = p2[2] or newBezierY2
                            end
                            bezierChanged = p1Changed or p2Changed
                            drawArticulatedBezierPreview(
                                newBezierX1, newBezierY1, newBezierX2, newBezierY2)
                        end
                        local qSignature = string.format('%.7f:%.7f:%.7f:%.7f',
                            qx or 0, qy or 0, qz or 0, qw or 1)
                        local authoredEulerSignature = hasAuthoredEuler and string.format(
                            '%.7f:%.7f:%.7f', authoredEulerX or 0,
                            authoredEulerY or 0, authoredEulerZ or 0) or nil
                        if not keyEuler or keyEuler.qSignature ~= qSignature or
                            keyEuler.authoredEulerSignature ~= authoredEulerSignature then
                            if hasAuthoredEuler then
                                keyEuler = {
                                    x = authoredEulerX or 0,
                                    y = authoredEulerY or 0,
                                    z = authoredEulerZ or 0,
                                    qSignature = qSignature,
                                    authoredEulerSignature = authoredEulerSignature
                                }
                            else
                                local keyOrbit = articulatedOrbitFromQuaternion(qx, qy, qz, qw)
                                keyEuler = {
                                    x = -(keyOrbit.elevation or 0) * 180 / math.pi,
                                    y = (keyOrbit.azimuth or 0) * 180 / math.pi,
                                    z = (keyOrbit.roll or 0) * 180 / math.pi,
                                    qSignature = qSignature
                                }
                            end
                            tEntry.tArticulatedKeyEuler[keyId] = keyEuler
                        end
                        tImGui.PushItemWidth(220)
                        local posChanged, pos = tImGui.DragFloat3('Position##artKeyPos-' .. keyId,
                            {px or 0, py or 0, pz or 0},
                            0.01, -math.huge, math.huge, '%.3f', 0)
                        tImGui.PopItemWidth()
                        tImGui.PushItemWidth(220)
                        local rotChanged, rot = tImGui.DragFloat3(
                            tLang.L('articulated_key_rotation') .. '##artKeyRot-' .. keyId,
                            {keyEuler.x, keyEuler.y, keyEuler.z},
                            0.5, -360.0, 360.0, '%.2f', 0)
                        tImGui.PopItemWidth()
                        articulatedTooltip('articulated_key_rotation_tooltip')
                        if rotChanged and rot then
                            for axis = 1, 3 do
                                rot[axis] = math.max(-360.0, math.min(360.0, rot[axis] or 0))
                            end
                            keyEuler.x = rot[1] or keyEuler.x
                            keyEuler.y = rot[2] or keyEuler.y
                            keyEuler.z = rot[3] or keyEuler.z
                            qx, qy, qz, qw = articulatedQuaternionFromOrbit({
                                elevation = -keyEuler.x * math.pi / 180,
                                azimuth = keyEuler.y * math.pi / 180,
                                roll = keyEuler.z * math.pi / 180
                            })
                            keyEuler.qSignature = string.format('%.7f:%.7f:%.7f:%.7f', qx, qy, qz, qw)
                            keyEuler.authoredEulerSignature = string.format('%.7f:%.7f:%.7f',
                                keyEuler.x, keyEuler.y, keyEuler.z)
                        end
                        tImGui.PushItemWidth(220)
                        local scaleChanged, scale = tImGui.DragFloat3('Scale##artKeyScale-' .. keyId,
                            {sx or 1, sy or 1, sz or 1},
                            0.01, -math.huge, math.huge, '%.3f', 0)
                        tImGui.PopItemWidth()
                        if easingChanged and newEasingIndex and newEasingIndex > 0 then
                            local okEasing = dpCall(function()
                                return meshD:setArticulatedKeyEasing(activeClip, trackIndex, keyIndex, newEasing)
                            end)
                            if okEasing then markArticulatedEdit() end
                        end
                        if bezierChanged then
                            local okBezier = dpCall(function()
                                return meshD:setArticulatedKeyBezier(activeClip, trackIndex, keyIndex,
                                    newBezierX1, newBezierY1, newBezierX2, newBezierY2)
                            end)
                            if okBezier then markArticulatedEdit() end
                        end
                        if posChanged or rotChanged or scaleChanged then
                            local p = pos or {px or 0, py or 0, pz or 0}
                            local r = {qx or 0, qy or 0, qz or 0}
                            local s = scale or {sx or 1, sy or 1, sz or 1}
                            local okUpdate = dpCall(function()
                                return meshD:updateArticulatedKey(activeClip, trackIndex, keyIndex,
                                    timeChanged and math.max(0, newTime or keyTime) or keyTime,
                                    p[1], p[2], p[3], r[1], r[2], r[3], qw or 1,
                                    s[1], s[2], s[3])
                            end)
                            if okUpdate then
                                if rotChanged then
                                    dpCall(function()
                                        return meshD:setArticulatedKeyEuler(activeClip, trackIndex,
                                            timeChanged and math.max(0, newTime or keyTime) or keyTime,
                                            keyEuler.x, keyEuler.y, keyEuler.z)
                                    end)
                                end
                                markArticulatedEdit()
                            end
                        elseif timeChanged then
                            local okTime = dpCall(function()
                                return meshD:updateArticulatedKey(activeClip, trackIndex, keyIndex,
                                    math.max(0, newTime or keyTime), px or 0, py or 0, pz or 0,
                                    qx or 0, qy or 0, qz or 0, qw or 1, sx or 1, sy or 1, sz or 1)
                            end)
                            if okTime then markArticulatedEdit() end
                        end
                        tImGui.PopID()
                    end
                end
                tImGui.PopID()
            end
        end
        if not selectedTrackIndex then
            tImGui.TextDisabled(tLang.L('articulated_add_key_hint'))
        end
    end
    tImGui.TreePop()
end

function articulatedQuaternionFromOrbit(orbit)
    local halfYaw = (orbit.azimuth or 0) * 0.5
    local halfPitch = -(orbit.elevation or 0) * 0.5
    local halfRoll = (orbit.roll or 0) * 0.5
    local sy, cy = math.sin(halfYaw), math.cos(halfYaw)
    local sx, cx = math.sin(halfPitch), math.cos(halfPitch)
    local sz, cz = math.sin(halfRoll), math.cos(halfRoll)
    -- q = yaw(Y) * pitch(X) * roll(Z), using x/y/z/w quaternion storage.
    local qx, qy, qz, qw = 0, sy, 0, cy
    local px, py, pz, pw = sx, 0, 0, cx
    local ax = qw * px + qx * pw + qy * pz - qz * py
    local ay = qw * py - qx * pz + qy * pw + qz * px
    local az = qw * pz + qx * py - qy * px + qz * pw
    local aw = qw * pw - qx * px - qy * py - qz * pz
    return ax * cz + ay * sz, ay * cz - ax * sz, az * cz + aw * sz, aw * cz - az * sz
end

function articulatedOrbitFromQuaternion(qx, qy, qz, qw)
    qx, qy, qz, qw = qx or 0, qy or 0, qz or 0, qw or 1
    local m02 = 2 * (qx * qz + qy * qw)
    local m12 = 2 * (qy * qz - qx * qw)
    local m22 = 1 - 2 * (qx * qx + qy * qy)
    local m10 = 2 * (qx * qy + qz * qw)
    local m11 = 1 - 2 * (qx * qx + qz * qz)
    local azimuth = math.atan(m02, m22)
    local elevation = -math.asin(math.max(-1, math.min(1, m12)))
    local roll = math.atan(m10, m11)
    return {
        azimuth = azimuth,
        elevation = math.max(-math.pi * 0.49, math.min(math.pi * 0.49, elevation)),
        roll = roll,
    }
end

function showArticulatedPivotWindow()
    local index = iSelectedMeshIndex
    local tEntry = tLoadedMeshes[index]
    if not tEntry or not tEntry.bArticulatedPivotWindow or tEntry.sOpenNode ~= 'articulated' then
        return
    end
    local meshD = tEntry.meshDebug
    local okTotal, totalParts = dpCall(function() return meshD:getTotalArticulatedParts() end)
    totalParts = (okTotal and totalParts) or 0
    if totalParts == 0 then
        tEntry.bArticulatedPivotWindow = false
        return
    end
    tEntry.iArticulatedPart = math.max(1, math.min(tEntry.iArticulatedPart or 1, totalParts))
    local ok, partId, frame, subset, name, px, py, pz, qx, qy, qz, qw, parent = dpCall(function()
        return meshD:getArticulatedPart(tEntry.iArticulatedPart)
    end)
    if not ok or not partId then return end

    local iW, iH = mbm.getRealSizeScreen()
    local winW, winH = 360, 470
    local winX = math.min(iW - winW - 10, (iLoadedMeshesWindowWidth or 520) + 10)
    tImGui.SetNextWindowPos({x = math.max(0, winX), y = math.max(30, iH - winH - 20)},
        tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x = winW, y = winH}, tImGui.Flags('ImGuiCond_Once'))
    local opened, closed = tImGui.Begin('Articulated Pivot Gizmo##artPivotWindow', true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if closed then tEntry.bArticulatedPivotWindow = false end
    if not opened then
        tImGui.End()
        return
    end

    tImGui.Text(string.format('Part %s  F%d S%d', tostring(partId), frame or 0, subset or 0))
    tImGui.Text(name or '')
    local quaternionSignature = string.format('%.7f:%.7f:%.7f:%.7f', qx or 0, qy or 0, qz or 0, qw or 1)
    if tEntry.tArticulatedPivotOrbitPartId ~= partId or
        tEntry.sArticulatedPivotQuaternionSignature ~= quaternionSignature then
        tEntry.tArticulatedPivotOrbit = articulatedOrbitFromQuaternion(qx, qy, qz, qw)
        tEntry.tArticulatedPivotOrbitPartId = partId
        tEntry.sArticulatedPivotQuaternionSignature = quaternionSignature
    end
    tEntry.tArticulatedPivotOrbit = tEntry.tArticulatedPivotOrbit or {azimuth = 0.3, elevation = 0.3}
    local orbitChanged = tUtil.drawOrbitGizmo(tEntry.tArticulatedPivotOrbit, {size = 130})
    if orbitChanged then
        qx, qy, qz, qw = articulatedQuaternionFromOrbit(tEntry.tArticulatedPivotOrbit)
        local okUpdate = dpCall(function()
            return meshD:updateArticulatedPart(tEntry.iArticulatedPart, name or '',
                px or 0, py or 0, pz or 0, qx, qy, qz, qw, parent or 0)
        end)
        if okUpdate then
            tEntry.modified = true
            tEntry.sArticulatedPivotQuaternionSignature = string.format('%.7f:%.7f:%.7f:%.7f',
                qx, qy, qz, qw)
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
        end
    end
    tImGui.PushItemWidth(180)
    local eulerChanged, euler = tImGui.DragFloat3('Euler (deg)##artPivotEuler-' .. index,
        {-(tEntry.tArticulatedPivotOrbit.elevation or 0) * 180 / math.pi,
            (tEntry.tArticulatedPivotOrbit.azimuth or 0) * 180 / math.pi,
            (tEntry.tArticulatedPivotOrbit.roll or 0) * 180 / math.pi},
        0.5, -360, 360, '%.2f', 0)
    tImGui.PopItemWidth()
    if eulerChanged and euler then
        tEntry.tArticulatedPivotOrbit.elevation = -(euler[1] or 0) * math.pi / 180
        tEntry.tArticulatedPivotOrbit.azimuth = (euler[2] or 0) * math.pi / 180
        tEntry.tArticulatedPivotOrbit.roll = (euler[3] or 0) * math.pi / 180
        qx, qy, qz, qw = articulatedQuaternionFromOrbit(tEntry.tArticulatedPivotOrbit)
        local okUpdate = dpCall(function()
            return meshD:updateArticulatedPart(tEntry.iArticulatedPart, name or '',
                px or 0, py or 0, pz or 0, qx, qy, qz, qw, parent or 0)
        end)
        if okUpdate then
            tEntry.modified = true
            tEntry.sArticulatedPivotQuaternionSignature = string.format('%.7f:%.7f:%.7f:%.7f',
                qx, qy, qz, qw)
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
        end
    end
    tImGui.Separator()
    tImGui.PushItemWidth(220)
    local posChanged, pos = tImGui.DragFloat3('Pivot Position##artPivotWindowPos-' .. index,
        {px or 0, py or 0, pz or 0}, 0.01, -math.huge, math.huge, '%.3f', 0)
    tImGui.PopItemWidth()
    tImGui.PushItemWidth(220)
    local rotChanged, rot = tImGui.DragFloat3('Pivot Quaternion##artPivotWindowRot-' .. index,
        {qx or 0, qy or 0, qz or 0}, 0.01, -1, 1, '%.3f', 0)
    tImGui.PopItemWidth()
    tImGui.PushItemWidth(105)
    local wChanged, newQw = tImGui.InputFloat('Pivot QW##artPivotWindowQw-' .. index,
        qw or 1, 0.01, 0.1, '%.3f', 0)
    tImGui.PopItemWidth()
    if posChanged or rotChanged or wChanged then
        local p = pos or {px or 0, py or 0, pz or 0}
        local r = rot or {qx or 0, qy or 0, qz or 0}
        local okUpdate = dpCall(function()
            return meshD:updateArticulatedPart(tEntry.iArticulatedPart, name or '',
                p[1], p[2], p[3], r[1], r[2], r[3], newQw or qw or 1, parent or 0)
        end)
        if okUpdate then
            tEntry.modified = true
            tEntry.tArticulatedPivotOrbit = articulatedOrbitFromQuaternion(r[1], r[2], r[3], newQw or qw or 1)
            tEntry.tArticulatedPivotOrbitPartId = partId
            tEntry.sArticulatedPivotQuaternionSignature = string.format('%.7f:%.7f:%.7f:%.7f',
                r[1], r[2], r[3], newQw or qw or 1)
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
        end
    end
    tImGui.TextDisabled(tLang.L('articulated_pivot_gizmo_hint'))
    tImGui.End()
end

-- Just the per-bone table (name/parent/x/y/z/radius/length/highlight/remove) in its own
-- bottom-docked window -- too wide to read comfortably inside the narrow "Loaded Meshes" tree panel
-- once it grew past a handful of columns. Everything else the Bones node used to draw inline (Up
-- axis, Apply Humanoid Armature, bake Rotate/Scale/Translate, Add Bone) stayed in showBonesNode
-- itself, per direct user request -- only the table moved out. Called once per frame from onLoop
-- (unlike showBonesNode, which runs once per loaded-mesh entry as part of the tree) -- only ever
-- shows the CURRENTLY SELECTED mesh's bones, matching the scope the mesh-hide/gizmo logic in
-- showBonesNode already uses (tPreviewMesh only ever reflects iSelectedMeshIndex, so showing any
-- other entry's bones here would have nothing to hide/gizmo against). Closing this window (its own
-- titlebar X) clears tEntry.sOpenNode -- showBonesNode's own per-frame open/close-transition logic
-- (mesh restore, gizmo destroy) then runs on the very next frame exactly as if the tree item itself
-- had been clicked closed, so there's only one real "closed" state to keep in sync, not two.
function showBonesWindow()
    local index = iSelectedMeshIndex
    local tEntry = tLoadedMeshes[index]
    if not tEntry or tEntry.sOpenNode ~= 'bones' then return end
    local meshD = tEntry.meshDebug

    -- Bottom-anchored, wide on first appearance (ImGuiCond_Once -- movable/resizable by the user
    -- afterward), following showCameraWindow's "real utility window" pattern rather than
    -- showMeshTools's minimal undecorated HUD style, since this holds a data-heavy table rather
    -- than a few buttons. X origin starts right where the "Loaded Meshes" tree window ends (its
    -- own live current width, per direct user request), not a fixed left margin -- avoids
    -- overlapping that panel regardless of how wide the user has resized it.
    local iW, iH = mbm.getRealSizeScreen()
    local winX = (iLoadedMeshesWindowWidth or 350) + 10
    local winW, winH = math.max(iW - winX - 20, 200), 300
    tImGui.SetNextWindowPos({x = winX, y = iH - winH - 20}, tImGui.Flags('ImGuiCond_Once'))
    tImGui.SetNextWindowSize({x = winW, y = winH}, tImGui.Flags('ImGuiCond_Once'))
    local wFlags = tImGui.Flags('ImGuiWindowFlags_NoCollapse')
    local isWinOpen, closedClicked = tImGui.Begin(tLang.L('bones_node') .. ' - ' .. tUtil.getShortName(tEntry.fileName) .. '##bonesWin', true, wFlags)
    if closedClicked then
        tEntry.sOpenNode = nil
    end
    if not isWinOpen then
        tImGui.End()
        return
    end

    local tBones = getBoneList(meshD)
    local tParentNames = { tLang.L('bones_root_label') }
    for _, b in ipairs(tBones) do table.insert(tParentNames, b.name) end

    tEntry.tBoneHighlight = tEntry.tBoneHighlight or {}

    local posDragSpeed = computeFieldDragSpeed(tBones, {'x', 'y', 'z'}, 0.001)
    local sizeDragSpeed = computeFieldDragSpeed(tBones, {'radius', 'length'}, 0.0005)

    local function findBoneInList(tBonesList, name)
        for _, bb in ipairs(tBonesList) do
            if bb.name == name then return bb end
        end
        return nil
    end

    if #tBones > 0 then
        -- ScrollX + fixed-width columns (not the default stretch sizing) so a wide row (name +
        -- parent combo + 3 drag floats + remove button) scrolls horizontally within the panel
        -- instead of forcing the whole Mesh Tree window wider or clipping the rightmost columns.
        -- Columns widened from their original inline-tree-panel sizing (per direct user testing
        -- feedback that several were clipping their own header/value text) now that this table has
        -- the whole bottom window's width to work with instead of the narrow "Loaded Meshes" tree
        -- panel -- horizontal scroll should rarely be needed in practice.
        local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg',
            'ImGuiTableFlags_ScrollY', 'ImGuiTableFlags_ScrollX', 'ImGuiTableFlags_SizingFixedFit')
        local listH = math.min(#tBones * 30 + 34, 320)
        if tImGui.BeginTable('bonesTbl-' .. index, 12, tblFlags, {x = 0, y = listH}) then
            tImGui.TableSetupScrollFreeze(1, 1)
            tImGui.TableSetupColumn(tLang.L('bones_name_label'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 160)
            tImGui.TableSetupColumn(tLang.L('bones_parent_label'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 160)
            tImGui.TableSetupColumn('X', tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 110)
            tImGui.TableSetupColumn('Y', tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 110)
            tImGui.TableSetupColumn('Z', tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 110)
            tImGui.TableSetupColumn(tLang.L('bones_radius_label'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 130)
            tImGui.TableSetupColumn(tLang.L('bones_length_label'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 130)
            tImGui.TableSetupColumn(tLang.L('bones_recompute_button'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 120)
            tImGui.TableSetupColumn(tLang.L('bones_roll_label'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 110)
            tImGui.TableSetupColumn(tLang.L('bones_highlight_label'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 50)
            tImGui.TableSetupColumn('Remove?', tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 130)
            tImGui.TableSetupColumn('Add Child?', tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 150)
            tImGui.TableHeadersRow()

            local childrenByParent = computeChildrenByParent(tBones)
            local iCountColumns = 0
            for _, b in ipairs(tBones) do
                tImGui.TableNextRow()

                -- Every updateBone call below forwards b.rotX/Y/Z, b.scaleX/Y/Z, b.length unchanged
                -- alongside whichever field this row's widget actually edited -- otherwise editing a
                -- bone's name/parent/position/radius/length here would silently reset its stored
                -- orientation (set by Blender import, not hand-authored here).
                tImGui.TableNextColumn()
                tUtil.pushResponsiveItemWidth(140)
                iCountColumns = iCountColumns + 1
                local chgName, newName = tImGui.InputText(tostring(iCountColumns) .. '##boneName-' .. index .. '-' .. b.idx, b.name, 64, 0)
                tImGui.PopItemWidth()
                if chgName and newName ~= '' and newName ~= b.name then
                    local okU = dpCall(function()
                        return meshD:updateBone(b.idx, newName, b.parentName, b.x, b.y, b.z, b.radius,
                            b.rotX, b.rotY, b.rotZ, b.scaleX, b.scaleY, b.scaleZ, b.length)
                    end)
                    if okU then onBonesEdit(tEntry, meshD, index) end
                end

                tImGui.TableNextColumn()
                local curParentPos = 1
                for pi, pname in ipairs(tParentNames) do
                    if pname == (b.parentName or tLang.L('bones_root_label')) then curParentPos = pi end
                end
                tUtil.pushResponsiveItemWidth(140)
                local chgParent, newParentPos = tImGui.Combo('##boneParent-' .. index .. '-' .. b.idx, curParentPos, tParentNames, -1)
                tImGui.PopItemWidth()
                if chgParent and newParentPos and newParentPos ~= curParentPos then
                    local newParentName = (newParentPos == 1) and nil or tParentNames[newParentPos]
                    local okU, err = dpCall(function()
                        return meshD:updateBone(b.idx, b.name, newParentName, b.x, b.y, b.z, b.radius,
                            b.rotX, b.rotY, b.rotZ, b.scaleX, b.scaleY, b.scaleZ, b.length)
                    end)
                    if okU then onBonesEdit(tEntry, meshD, index) else tUtil.showMessageWarn(err or tLang.L('an_error_occurred')) end
                end

                local function dragAxis(axisLabel, val)
                    tUtil.pushResponsiveItemWidth(100)
                    local chg, nv = tImGui.DragFloat(axisLabel .. '##bone' .. axisLabel .. '-' .. index .. '-' .. b.idx, val, posDragSpeed, 0, 0, '%.3f')
                    tImGui.PopItemWidth()
                    return chg, nv
                end
                tImGui.TableNextColumn()
                local chgX, nx = dragAxis('X', b.x)
                tImGui.TableNextColumn()
                local chgY, ny = dragAxis('Y', b.y)
                tImGui.TableNextColumn()
                local chgZ, nz = dragAxis('Z', b.z)
                if chgX or chgY or chgZ then
                    local okU = dpCall(function()
                        return meshD:updateBone(b.idx, b.name, b.parentName, nx or b.x, ny or b.y, nz or b.z, b.radius,
                            b.rotX, b.rotY, b.rotZ, b.scaleX, b.scaleY, b.scaleZ, b.length)
                    end)
                    if okU then onBonesEdit(tEntry, meshD, index) end
                end

                tImGui.TableNextColumn()
                tUtil.pushResponsiveItemWidth(120)
                local chgRadius, nRadius = tImGui.DragFloat('Radius##boneRadius-' .. index .. '-' .. b.idx, b.radius, sizeDragSpeed, 0, 0, '%.3f')
                tImGui.PopItemWidth()
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('bones_radius_tooltip'))
                    tImGui.EndTooltip()
                end
                if chgRadius then
                    local okU = dpCall(function()
                        return meshD:updateBone(b.idx, b.name, b.parentName, b.x, b.y, b.z, nRadius,
                            b.rotX, b.rotY, b.rotZ, b.scaleX, b.scaleY, b.scaleZ, b.length)
                    end)
                    if okU then onBonesEdit(tEntry, meshD, index) end
                end

                tImGui.TableNextColumn()
                -- length <= EPS (blender_mesh_skeleton_export.py's has_orientation() sentinel) means
                -- THIS bone's rotX/Y/Z is silently ignored on FBX export -- ARMATURE_STANDARD_SKELETON_65
                -- and every real Blender import always carries a real nonzero length, so this only ever
                -- fires for a hand-authored bone that never got an orientation. Warn, never block --
                -- length==0 stays a legitimate "use the position-topology fallback" sentinel for
                -- legacy/no-provenance bones, per docs/bones-armatures-and-fbx.md.
                --
                -- Drawn BEFORE the DragFloat (not after, via SameLine): pushResponsiveItemWidth's
                -- min_width is a floor, not a cap (max(min_width, available-reserve)), so the
                -- DragFloat below normally fills the entire column -- a marker tacked on AFTER it
                -- via SameLine was getting clipped off past the column's own boundary, invisible.
                -- Drawing it first means GetContentRegionAvail() (which pushResponsiveItemWidth
                -- reads) already reflects the space this marker consumed by the time the DragFloat
                -- sizes itself, so the two never fight over the same pixels.
                local lengthIsZero = b.length <= 1e-6
                if lengthIsZero then
                    tImGui.PushStyleColor('ImGuiCol_Text', {r = 1, g = 0.65, b = 0.2, a = 1})
                    tImGui.Text('!')
                    tImGui.PopStyleColor(1)
                    if tImGui.IsItemHovered(0) then
                        tImGui.BeginTooltip()
                        tImGui.Text(tLang.L('bones_length_zero_warning'))
                        tImGui.EndTooltip()
                    end
                    tImGui.SameLine()
                end
                tUtil.pushResponsiveItemWidth(140)
                -- Same warning color as the "!" marker, applied to the field itself (not just the
                -- marker) so the zero-length row reads as flagged even without hovering for the
                -- tooltip -- direct user request.
                if lengthIsZero then
                    tImGui.PushStyleColor('ImGuiCol_FrameBg', {r = 1, g = 0.5, b = 0.15, a = 0.35})
                    tImGui.PushStyleColor('ImGuiCol_FrameBgHovered', {r = 1, g = 0.5, b = 0.15, a = 0.5})
                    tImGui.PushStyleColor('ImGuiCol_FrameBgActive', {r = 1, g = 0.5, b = 0.15, a = 0.6})
                end
                local chgLength, nLength = tImGui.DragFloat('Length##boneLength-' .. index .. '-' .. b.idx, b.length, sizeDragSpeed, 0, 0, '%.3f')
                if lengthIsZero then
                    tImGui.PopStyleColor(3)
                end
                tImGui.PopItemWidth()
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('bones_length_tooltip'))
                    tImGui.EndTooltip()
                end
                if chgLength then
                    local okU = dpCall(function()
                        return meshD:updateBone(b.idx, b.name, b.parentName, b.x, b.y, b.z, b.radius,
                            b.rotX, b.rotY, b.rotZ, b.scaleX, b.scaleY, b.scaleZ, nLength)
                    end)
                    if okU then onBonesEdit(tEntry, meshD, index) end
                end

                tImGui.TableNextColumn()
                -- Bakes compute_tail's own fallback (see computeAimAndLength above) into real
                -- rotX/Y/Z + length, replacing an invisible export-time guess with inspectable,
                -- further-editable data. Roll is PRESERVED (decoded from the bone's current
                -- rotX/Y/Z relative to its OLD aim, then reapplied to the NEW aim) rather than
                -- reset to 0 -- confirmed via direct user testing that resetting roll here was
                -- real data loss: clicking Recompute on a bone that already had a real,
                -- Blender-authored roll (e.g. the root bone) silently discarded it, even though
                -- Recompute's whole purpose is fixing MISSING orientation, not overwriting a good one.
                if tImGui.Button(tLang.L('bones_recompute_button') .. '##boneRecompute-' .. index .. '-' .. b.idx) then
                    local parentB = b.parentName and findBoneByName(tBones, b.parentName)
                    local ax, ay, az, newLength = computeAimAndLength(childrenByParent, b, parentB)
                    local curRoll = currentRollDeg(b.rotX, b.rotY, b.rotZ)
                    local nRotX, nRotY, nRotZ = eulerFromAimAndRoll(ax, ay, az, curRoll)
                    local okU = dpCall(function()
                        return meshD:updateBone(b.idx, b.name, b.parentName, b.x, b.y, b.z, b.radius,
                            nRotX, nRotY, nRotZ, b.scaleX, b.scaleY, b.scaleZ, newLength)
                    end)
                    if okU then onBonesEdit(tEntry, meshD, index) end
                end
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('bones_recompute_tooltip'))
                    tImGui.EndTooltip()
                end

                tImGui.TableNextColumn()
                -- Stateless: decoded fresh from b.rotX/Y/Z every frame (same convention as every
                -- other field in this row) rather than a persisted field -- SKELETON_BONE_V11 has
                -- no roll field of its own, only rotX/Y/Z, so "roll" only exists as this
                -- canonicalRollAxis-relative decomposition.
                tUtil.pushResponsiveItemWidth(100)
                local curRoll = currentRollDeg(b.rotX, b.rotY, b.rotZ)
                local chgRoll, nRoll = tImGui.DragFloat('Roll##boneRoll-' .. index .. '-' .. b.idx, curRoll, 1.0, 0, 0, '%.1f')
                tImGui.PopItemWidth()
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('bones_roll_tooltip'))
                    tImGui.EndTooltip()
                end
                if chgRoll then
                    local yx, yy, yz = eulerToBoneFrame(b.rotX, b.rotY, b.rotZ)
                    local nRotX, nRotY, nRotZ = eulerFromAimAndRoll(yx, yy, yz, nRoll)
                    local okU = dpCall(function()
                        return meshD:updateBone(b.idx, b.name, b.parentName, b.x, b.y, b.z, b.radius,
                            nRotX, nRotY, nRotZ, b.scaleX, b.scaleY, b.scaleZ, b.length)
                    end)
                    if okU then onBonesEdit(tEntry, meshD, index) end
                end

                tImGui.TableNextColumn()
                -- Pure view preference (which bone renders yellow in the 3D gizmo view), not a mesh
                -- edit -- no onBonesEdit()/tEntry.modified, just a gizmo recolor. Multiple bones can
                -- be highlighted at once (independent per-row checkboxes, not a radio selection).
                local newHighlight = tImGui.Checkbox('##boneHighlight-' .. index .. '-' .. b.idx, tEntry.tBoneHighlight[b.name] or false)
                if newHighlight ~= (tEntry.tBoneHighlight[b.name] or false) then
                    tEntry.tBoneHighlight[b.name] = newHighlight or nil
                    rebuildBoneGizmo(tEntry, meshD, index)
                end

                tImGui.TableNextColumn()
                if tImGui.Button(tLang.L('bones_remove_button') .. '##boneRm-' .. index .. '-' .. b.idx) then
                    local okR = dpCall(function() return meshD:removeBone(b.idx, false) end)
                    if okR then
                        onBonesEdit(tEntry, meshD, index)
                        tEntry.tBonePendingRemove = nil
                    else
                        tEntry.tBonePendingRemove = { idx = b.idx, name = b.name }
                    end
                end

                -- "+ Add Child Bone" (direct user request): appends a new bone as this row's own
                -- child, continuing the same direction and distance this bone itself has from ITS
                -- OWN parent (grandparent -> parent -> this -> new, evenly spaced and colinear) --
                -- a quick way to extend a limb chain one joint at a time without hand-computing a
                -- position. Its own column (not a new row), right after Remove?, per direct user
                -- request.
                tImGui.TableNextColumn()
                if tImGui.Button(tLang.L('bones_add_child_button') .. '##boneAddChild-' .. index .. '-' .. b.idx) then
                    local parentB = b.parentName and findBoneInList(tBones, b.parentName)
                    local nx, ny, nz
                    if parentB then
                        local dx, dy, dz = b.x - parentB.x, b.y - parentB.y, b.z - parentB.z
                        local dlen = math.sqrt(dx*dx + dy*dy + dz*dz)
                        if dlen > 0.0001 then
                            nx, ny, nz = b.x + dx, b.y + dy, b.z + dz
                        end
                    end
                    if not nx then
                        -- No parent (root bone) or a degenerate zero-length step to extrapolate
                        -- from -- fall back to a fixed, visible default offset.
                        nx, ny, nz = b.x, b.y + 10, b.z
                    end
                    local newName = 'Bone ' .. (#tBones + 1)
                    local okA, errA = dpCall(function()
                        return meshD:addBone(newName, b.name, nx, ny, nz, b.radius)
                    end)
                    if okA then
                        onBonesEdit(tEntry, meshD, index)
                    else
                        tUtil.showMessageWarn(errA or tLang.L('an_error_occurred'))
                    end
                end
            end
            tImGui.EndTable()
        end
    else
        tImGui.TextDisabled(tLang.L('bones_none_label'))
    end

    if tEntry.tBonePendingRemove then
        local pend = tEntry.tBonePendingRemove
        tImGui.TextColored({r = 1, g = 0.6, b = 0.2, a = 1}, string.format(tLang.L('bones_confirm_cascade_fmt'), pend.name))
        if tImGui.Button(tLang.L('bones_confirm_cascade_button') .. '##boneRmCascade-' .. index) then
            local okR = dpCall(function() return meshD:removeBone(pend.idx, true) end)
            if okR then onBonesEdit(tEntry, meshD, index) end
            tEntry.tBonePendingRemove = nil
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('cancel') .. '##boneRmCancel-' .. index) then
            tEntry.tBonePendingRemove = nil
        end
    end

    -- ---------------------------------------------------------------------------
    -- Rigid Bind: writes real per-vertex weight 1.0 to one bone (SECTION_VERTEX_SKIN_WEIGHTS,
    -- meshD:setVertexWeight) for a prop that shouldn't deform (e.g. a sword welded to a hand),
    -- instead of leaving Blender's distance-based ARMATURE_ENVELOPE guess to decide which nearby
    -- bone(s) influence it -- exactly the scenario that produced a mismatched/wrong-looking
    -- attachment in Mixamo that prompted this whole feature. blender_mesh_skeleton_export.py's
    -- override pass keeps every OTHER vertex's normal envelope-derived weighting untouched.
    -- ---------------------------------------------------------------------------
    if #tBones > 0 then
        tImGui.Separator()
        tImGui.Text(tLang.L('bones_rigid_bind_section'))
        tImGui.HelpMarker(tLang.L('bones_rigid_bind_help'))

        tEntry.tRigidBindUI = tEntry.tRigidBindUI or { boneName = tBones[1].name, mode = 'proximity', subsetIndex = 1, matched = nil }
        local rb = tEntry.tRigidBindUI
        if not findBoneByName(tBones, rb.boneName) then
            rb.boneName = tBones[1].name
            rb.matched = nil
        end

        local tBoneNames = {}
        for _, bb in ipairs(tBones) do table.insert(tBoneNames, bb.name) end
        -- Fixed width, NOT pushResponsiveItemWidth -- that helper's min_width is a FLOOR, not a
        -- cap (it returns max(min_width, available-reserve)), so on this wide, bottom-docked
        -- window it stretched to nearly the full window width instead of staying a modest combo,
        -- crowding the checkbox right after it off the edge of the window.
        tImGui.PushItemWidth(140)
        local chgRbBone, newRbBonePos = tImGui.Combo('##rigidBindBone-' .. index, indexOf(tBoneNames, rb.boneName), tBoneNames, -1)
        tImGui.PopItemWidth()
        if chgRbBone then rb.boneName = tBoneNames[newRbBonePos]; rb.matched = nil end

        tImGui.SameLine()
        -- tImGui.Checkbox returns ONLY the resulting boolean (unlike Combo/DragFloat's
        -- (changed, value) pair) -- every other checkbox in this file compares against the old
        -- value to detect a change (e.g. the Highlight checkbox below); this one previously
        -- assumed a (changed, value) return here, so wantSubsetMode was always nil and rb.mode
        -- could never actually become 'subset'.
        local wantSubsetMode = tImGui.Checkbox(tLang.L('bones_rigid_bind_use_subset') .. '##rigidBindMode-' .. index, rb.mode == 'subset')
        local newRbMode = wantSubsetMode and 'subset' or 'proximity'
        if newRbMode ~= rb.mode then rb.mode = newRbMode; rb.matched = nil end

        local okRbS, nRbSubsets = dpCall(function() return meshD:getTotalSubset(1) end)
        nRbSubsets = (okRbS and nRbSubsets) or 0
        if rb.mode == 'subset' and nRbSubsets > 0 then
            rb.subsetIndex = math.min(math.max(rb.subsetIndex or 1, 1), nRbSubsets)
            local tSubsetLabels = {}
            for s = 1, nRbSubsets do
                local okRbV, nRbV = dpCall(function() return meshD:getTotalVertex(1, s) end)
                table.insert(tSubsetLabels, string.format('%s %d (%d v)', tLang.L('bones_rigid_bind_subset_label'), s, (okRbV and nRbV) or 0))
            end
            tImGui.PushItemWidth(160)
            local chgRbSubset, newRbSubset = tImGui.Combo('##rigidBindSubset-' .. index, rb.subsetIndex, tSubsetLabels, -1)
            tImGui.PopItemWidth()
            if chgRbSubset then rb.subsetIndex = newRbSubset; rb.matched = nil end
        end

        -- Recomputes on an explicit click only -- per-vertex distance math over the whole mesh
        -- (findVerticesNearBoneSegment) is the same "thousands, not tens, of iterations" cost
        -- computeWeightStats's own comment warns about, so this follows that same
        -- cache-and-invalidate-on-edit pattern (tEntry.tRigidBindUI.matched, cleared by
        -- onBonesEdit) rather than recomputing every frame the window happens to be open.
        if tImGui.Button(tLang.L('bones_rigid_bind_preview_button') .. '##rigidBindPreview-' .. index) then
            local targetBone = findBoneByName(tBones, rb.boneName)
            if targetBone then
                if rb.mode == 'subset' then
                    rb.matched = findVerticesInSubset(meshD, rb.subsetIndex)
                else
                    rb.matched = findVerticesNearBoneSegment(meshD, tBones, targetBone)
                end
            end
        end
        tImGui.SameLine()
        if rb.matched then
            tImGui.Text(string.format(tLang.L('bones_rigid_bind_match_count_fmt'), #rb.matched))
        else
            tImGui.TextDisabled(tLang.L('bones_rigid_bind_no_preview_label'))
        end

        if rb.matched and #rb.matched > 0 then
            if tImGui.Button(tLang.L('bones_rigid_bind_apply_button') .. '##rigidBindApply-' .. index) then
                local nApplied, allOk = 0, true
                for _, m in ipairs(rb.matched) do
                    local okSet = dpCall(function()
                        return meshD:setVertexWeight(m.globalIndex, rb.boneName, 1.0, nil, 0, nil, 0, nil, 0)
                    end)
                    if okSet then nApplied = nApplied + 1 else allOk = false end
                end
                if allOk then
                    tUtil.showMessage(string.format(tLang.L('bones_rigid_bind_applied_fmt'), nApplied, rb.boneName))
                else
                    tUtil.showMessageWarn(string.format(tLang.L('bones_rigid_bind_applied_partial_fmt'), nApplied, #rb.matched))
                end
                rb.matched = nil
                onBonesEdit(tEntry, meshD, index)
            end
        end
    end

    tImGui.End()
end

-- ---------------------------------------------------------------------------
-- Split capture: an axis-aligned 3D volume used to extract triangle groups.
-- The volume is deliberately only evaluated when Start Capture is turned off;
-- dragging/resizing it never mutates mesh data.
-- ---------------------------------------------------------------------------
function splitCaptureSetObjectPosition(obj, t)
    if not obj then return end
    obj.x, obj.y, obj.z = t.x, t.y, t.z
end

function splitCaptureMoveBox(t)
    splitCaptureSetObjectPosition(t.tShape, t)
    splitCaptureSetObjectPosition(t.tLine, t)
    for _, overlay in pairs(t.tAxisEdgeLines or {}) do splitCaptureSetObjectPosition(overlay, t) end
    for _, overlay in pairs(t.tAxisFaceShapes or {}) do splitCaptureSetObjectPosition(overlay, t) end
    local hw, hh, hd = math.max(t.width, 0.01) * 0.5,
                       math.max(t.height, 0.01) * 0.5,
                       math.max(t.depth, 0.01) * 0.5
    t.aabbMin = {x=t.x-hw, y=t.y-hh, z=t.z-hd}
    t.aabbMax = {x=t.x+hw, y=t.y+hh, z=t.z+hd}
end

function splitCaptureBuildBox(t)
    if t.tShape then t.tShape:destroy(); t.tShape = nil end
    if t.tLine then t.tLine:destroy(); t.tLine = nil end
    for _, overlay in pairs(t.tAxisEdgeLines or {}) do overlay:destroy() end
    for _, overlay in pairs(t.tAxisFaceShapes or {}) do overlay:destroy() end
    t.tAxisEdgeLines, t.tAxisFaceShapes = {}, {}
    local hw, hh, hd = math.max(t.width, 0.01) * 0.5,
                       math.max(t.height, 0.01) * 0.5,
                       math.max(t.depth, 0.01) * 0.5
    local corners = {
        {x=-hw,y=-hh,z= hd}, {x=-hw,y= hh,z= hd}, {x= hw,y= hh,z= hd}, {x= hw,y=-hh,z= hd},
        {x=-hw,y=-hh,z=-hd}, {x=-hw,y= hh,z=-hd}, {x= hw,y= hh,z=-hd}, {x= hw,y=-hh,z=-hd},
    }
    local faces = {
        {1,2,3},{1,3,4},{5,7,6},{5,8,7},
        {5,6,2},{5,2,1},{4,3,7},{4,7,8},
        {2,6,7},{2,7,3},{5,1,4},{5,4,8},
    }
    local verts = {}
    for _, tri in ipairs(faces) do
        for _, idx in ipairs(tri) do
            local p = corners[idx]
            table.insert(verts, p.x); table.insert(verts, p.y); table.insert(verts, p.z)
        end
    end
    local name = 'mesh_debug_split_capture_' .. tostring(os.clock())
    t.tShape = shape:new('3d', t.x, t.y, t.z)
    t.tShape:create(verts, nil, name)
    t.tShape:setColor(1, 0.65, 0.05, 0.12)
    t.tShape.alwaysOnTop = true
    -- drawBounding() generates local coordinates, so the outline must share
    -- the capture volume's world-space center.
    t.tLine = line:new('3d', t.x, t.y, t.z)
    t.tLine:drawBounding(t.tShape, false)
    t.tLine:setColor(1, 0.75, 0.1)

    -- Hover overlays are built together with the capture box, then only their
    -- visibility changes per frame. This avoids allocating render objects while
    -- the mouse moves across the six controls.
    local axisColors = {
        x = {1.0, 0.1, 0.8}, -- magenta
        y = {0.1, 1.0, 1.0}, -- cyan
        z = {0.8, 1.0, 0.1}, -- lime
    }
    local axisEdges = {
        x = {{1,4},{2,3},{5,8},{6,7}},
        y = {{1,2},{4,3},{5,6},{8,7}},
        z = {{1,5},{2,6},{3,7},{4,8}},
    }
    local axisFaces = {
        x = {{5,6,2},{5,2,1},{4,3,7},{4,7,8}},
        y = {{2,6,7},{2,7,3},{5,1,4},{5,4,8}},
        z = {{1,2,3},{1,3,4},{5,7,6},{5,8,7}},
    }
    for _, axis in ipairs({'x', 'y', 'z'}) do
        local edgeLine = line:new('3d', t.x, t.y, t.z)
        for _, edge in ipairs(axisEdges[axis]) do
            local edgeVertices = {}
            for _, cornerIndex in ipairs(edge) do
                local p = corners[cornerIndex]
                table.insert(edgeVertices, p.x)
                table.insert(edgeVertices, p.y)
                table.insert(edgeVertices, p.z)
            end
            -- Each add() is its own LINE_STRIP. Keeping one strip per edge
            -- prevents diagonal connector segments between parallel edges.
            edgeLine:add(edgeVertices)
        end
        edgeLine:setColor(axisColors[axis][1], axisColors[axis][2], axisColors[axis][3], 1)
        edgeLine.alwaysOnTop = true
        edgeLine.visible = false
        t.tAxisEdgeLines[axis] = edgeLine

        local faceVertices = {}
        for _, tri in ipairs(axisFaces[axis]) do
            for _, cornerIndex in ipairs(tri) do
                local p = corners[cornerIndex]
                table.insert(faceVertices, p.x)
                table.insert(faceVertices, p.y)
                table.insert(faceVertices, p.z)
            end
        end
        local faceShape = shape:new('3d', t.x, t.y, t.z)
        faceShape:create(faceVertices, nil, name .. '_hover_face_' .. axis)
        faceShape:setColor(axisColors[axis][1], axisColors[axis][2], axisColors[axis][3], 0.32)
        faceShape.alwaysOnTop = true
        faceShape.visible = false
        t.tAxisFaceShapes[axis] = faceShape
    end
    -- Assign all coordinates explicitly after creation. Center-only edits use
    -- this same path without destroying or recreating any render object.
    splitCaptureMoveBox(t)
end

function splitCaptureSetHover(t, kind, axis)
    for name, overlay in pairs(t.tAxisEdgeLines or {}) do
        overlay.visible = kind == 'center' and name == axis
    end
    for name, overlay in pairs(t.tAxisFaceShapes or {}) do
        overlay.visible = kind == 'size' and name == axis
    end
end

function splitCaptureRayHitsAABB(ox, oy, oz, dx, dy, dz, minX, minY, minZ, maxX, maxY, maxZ)
    local tmin, tmax = -math.huge, math.huge
    local function slab(o, d, mn, mx)
        if math.abs(d) < 1e-9 then return o >= mn and o <= mx end
        local a, b = (mn-o)/d, (mx-o)/d
        if a > b then a, b = b, a end
        tmin, tmax = math.max(tmin, a), math.min(tmax, b)
        return tmin <= tmax
    end
    return slab(ox,dx,minX,maxX) and slab(oy,dy,minY,maxY) and slab(oz,dz,minZ,maxZ) and tmax >= 0
end

function splitCaptureDestroy(t)
    if not t then return end
    if t.tShape then t.tShape:destroy(); t.tShape = nil end
    if t.tLine then t.tLine:destroy(); t.tLine = nil end
    for _, overlay in pairs(t.tAxisEdgeLines or {}) do overlay:destroy() end
    for _, overlay in pairs(t.tAxisFaceShapes or {}) do overlay:destroy() end
    t.tAxisEdgeLines, t.tAxisFaceShapes = nil, nil
end

function splitCaptureCopyTable(source)
    local copy = {}
    for key, value in pairs(source or {}) do
        if type(value) == 'table' then
            copy[key] = splitCaptureCopyTable(value)
        else
            copy[key] = value
        end
    end
    return copy
end

function splitCaptureDiscardBackup(tEntry)
    local backup = tEntry and tEntry.tSplitCaptureBackup
    if not backup then return end
    meshDebug:fakeRelease(backup.path)
    os.remove(backup.path)
    tEntry.tSplitCaptureBackup = nil
end

function splitCaptureCreateBackup(tEntry, meshD)
    local backupPath = os.tmpname() .. '.msh'
    if not meshD:save(backupPath, false, false) then
        meshDebug:fakeRelease(backupPath)
        os.remove(backupPath)
        return nil
    end
    return {
        path = backupPath,
        modified = tEntry.modified == true,
        info = splitCaptureCopyTable(tEntry.info),
        checkedRemove = splitCaptureCopyTable(tEntry.tCheckedRemove),
        capturedSignatures = splitCaptureCopyTable(tEntry.tSplitCapturedSignatures),
        captures = splitCaptureCopyTable(tEntry.tSplitCaptures),
        lastFaces = tEntry.tSplitCapture and tEntry.tSplitCapture.lastFaces or nil,
        lastFrames = tEntry.tSplitCapture and tEntry.tSplitCapture.lastFrames or nil,
    }
end

function splitCaptureRevert(tEntry, index)
    local backup = tEntry.tSplitCaptureBackup
    if not backup then return end
    local restored = meshDebug:new()
    if not restored:load(backup.path) then
        tUtil.showMessageWarn(tLang.L('revert_last_capture_failed'))
        return
    end

    tEntry.meshDebug = restored
    tEntry.modified = backup.modified
    tEntry.info = backup.info
    tEntry.tCheckedRemove = backup.checkedRemove
    tEntry.tSplitCapturedSignatures = backup.capturedSignatures
    tEntry.tSplitCaptures = backup.captures
    if tEntry.tSplitCapture then
        tEntry.tSplitCapture.lastFaces = backup.lastFaces
        tEntry.tSplitCapture.lastFrames = backup.lastFrames
    end
    splitCaptureDiscardBackup(tEntry)
    destroyNormalVisualization(tEntry)
    destroyPhysicsVisualization(tEntry)
    tEntry.bNormalsVizDirty = true
    tEntry.bPhysicsVizDirty = true
    tEntry.tTransformBoundsCache = nil
    if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
    tUtil.showMessage(tLang.L('revert_last_capture_success'), 5)
end

function splitCaptureSignature(vertices, indices, texture)
    local h = 2166136261
    local function add(v)
        h = (h * 16777619 + (tonumber(v) or 0) * 1000) % 2147483647
    end
    add(texture and #texture or 0)
    for _, v in ipairs(vertices or {}) do
        add(v.x); add(v.y); add(v.z); add(v.nx); add(v.ny); add(v.nz); add(v.u); add(v.v)
    end
    for _, i in ipairs(indices or {}) do add(i) end
    return tostring(math.floor(h)) .. ':' .. tostring(#vertices) .. ':' .. tostring(#indices)
end

function splitCaptureGroup(vertices, triangles)
    local outVertices, outIndices, remap = {}, {}, {}
    for _, tri in ipairs(triangles) do
        for _, oldIndex in ipairs(tri) do
            local newIndex = remap[oldIndex]
            if not newIndex then
                newIndex = #outVertices + 1
                remap[oldIndex] = newIndex
                outVertices[newIndex] = vertices[oldIndex]
            end
            table.insert(outIndices, newIndex)
        end
    end
    return outVertices, outIndices
end

function splitCaptureSetSubsetTextures(meshD, frame, subset, texture, materialTextures)
    meshD:setTexture(frame, subset, texture or '')
    for role, value in pairs(materialTextures or {}) do
        meshD:setMaterialTexture(frame, subset, role, value or '')
    end
end

function splitCaptureGetSubsetSignature(meshD, frame, subset)
    local okV, nVertices = dpCall(function() return meshD:getTotalVertex(frame, subset) end)
    if not okV or not nVertices then return nil end
    local vertices = {}
    for v = 1, nVertices do
        local ok, value = dpCall(function() return meshD:getVertex(frame, subset, v) end)
        if not ok or not value then return nil end
        vertices[v] = value
    end
    local okI, nIndices = dpCall(function() return meshD:getTotalIndex(frame, subset) end)
    local indices = {}
    if okI and nIndices and nIndices > 0 then
        local ok, value = dpCall(function() return meshD:getIndex(frame, subset) end)
        indices = (ok and value) or {}
    else
        for i = 1, nVertices do indices[i] = i end
    end
    local okT, texture = dpCall(function() return meshD:getTexture(frame, subset) end)
    return splitCaptureSignature(vertices, indices, okT and texture or '')
end

function splitCapturePointInside(p, box)
    return p.x >= box.aabbMin.x and p.x <= box.aabbMax.x and
           p.y >= box.aabbMin.y and p.y <= box.aabbMax.y and
           p.z >= box.aabbMin.z and p.z <= box.aabbMax.z
end

-- Complete triangle/AABB overlap test (box axes, triangle normal, and the nine edge-cross-axis
-- separating axes). It selects whole source triangles; it does not clip or invent vertices.
function splitCaptureTriangleIntersectsBox(a, b, c, box)
    local cx = (box.aabbMin.x + box.aabbMax.x) * 0.5
    local cy = (box.aabbMin.y + box.aabbMax.y) * 0.5
    local cz = (box.aabbMin.z + box.aabbMax.z) * 0.5
    local hx = (box.aabbMax.x - box.aabbMin.x) * 0.5
    local hy = (box.aabbMax.y - box.aabbMin.y) * 0.5
    local hz = (box.aabbMax.z - box.aabbMin.z) * 0.5
    local p = {
        {x=a.x-cx, y=a.y-cy, z=a.z-cz},
        {x=b.x-cx, y=b.y-cy, z=b.z-cz},
        {x=c.x-cx, y=c.y-cy, z=c.z-cz},
    }
    local function overlapsAxis(x, y, z)
        if x*x + y*y + z*z < 1e-18 then return true end
        local p1 = p[1].x*x + p[1].y*y + p[1].z*z
        local p2 = p[2].x*x + p[2].y*y + p[2].z*z
        local p3 = p[3].x*x + p[3].y*y + p[3].z*z
        local r = hx*math.abs(x) + hy*math.abs(y) + hz*math.abs(z)
        return math.min(p1, p2, p3) <= r and math.max(p1, p2, p3) >= -r
    end
    if not overlapsAxis(1,0,0) or not overlapsAxis(0,1,0) or not overlapsAxis(0,0,1) then return false end
    local edges = {
        {x=p[2].x-p[1].x, y=p[2].y-p[1].y, z=p[2].z-p[1].z},
        {x=p[3].x-p[2].x, y=p[3].y-p[2].y, z=p[3].z-p[2].z},
        {x=p[1].x-p[3].x, y=p[1].y-p[3].y, z=p[1].z-p[3].z},
    }
    for _, e in ipairs(edges) do
        if not overlapsAxis(0, e.z, -e.y) or
           not overlapsAxis(-e.z, 0, e.x) or
           not overlapsAxis(e.y, -e.x, 0) then return false end
    end
    local e1, e2 = edges[1], {x=p[3].x-p[1].x, y=p[3].y-p[1].y, z=p[3].z-p[1].z}
    return overlapsAxis(e1.y*e2.z-e1.z*e2.y,
                        e1.z*e2.x-e1.x*e2.z,
                        e1.x*e2.y-e1.y*e2.x)
end

function splitCapturePositionKey(v)
    return string.format('%.9g:%.9g:%.9g', v.x or 0, v.y or 0, v.z or 0)
end

function splitCaptureBuildIslands(triangles, vertices)
    local tokenFaces = {}
    for ti, tri in ipairs(triangles) do
        for _, vi in ipairs(tri) do
            local tokens = {'i:' .. tostring(vi), 'p:' .. splitCapturePositionKey(vertices[vi])}
            for _, token in ipairs(tokens) do
                tokenFaces[token] = tokenFaces[token] or {}
                table.insert(tokenFaces[token], ti)
            end
        end
    end
    local visited, islands = {}, {}
    for start = 1, #triangles do
        if not visited[start] then
            local island, queue, head = {}, {start}, 1
            visited[start] = true
            while head <= #queue do
                local ti = queue[head]; head = head + 1
                table.insert(island, triangles[ti])
                for _, vi in ipairs(triangles[ti]) do
                    local tokens = {'i:' .. tostring(vi), 'p:' .. splitCapturePositionKey(vertices[vi])}
                    for _, token in ipairs(tokens) do
                        for _, neighbor in ipairs(tokenFaces[token] or {}) do
                            if not visited[neighbor] then visited[neighbor] = true; table.insert(queue, neighbor) end
                        end
                    end
                end
            end
            table.insert(islands, island)
        end
    end
    table.sort(islands, function(left, right) return #left > #right end)
    return islands
end

function splitCaptureAnalyze(tEntry, meshD, box)
    local okMode, mode = dpCall(function() return meshD:getModeDraw() end)
    if not okMode or mode ~= 'TRIANGLES' then return nil, tLang.L('capture_requires_triangles') end
    local algorithms = {
        {id='center', label=tLang.L('capture_algorithm_center'), groups={}},
        {id='entire', label=tLang.L('capture_algorithm_entire'), groups={}},
        {id='vertex', label=tLang.L('capture_algorithm_vertex'), groups={}},
        {id='intersect', label=tLang.L('capture_algorithm_intersect'), groups={}},
    }
    local captured = tEntry.tSplitCapturedSignatures or {}
    local okF, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okF or not nFrames then return nil, tLang.L('capture_analysis_failed') end
    for f = 1, nFrames do
        local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        for s = 1, (okS and nSubsets or 0) do
            if (tEntry.tCheckedRemove or {})[f * 100 + s] ~= false then
                local okV, nVertices = dpCall(function() return meshD:getTotalVertex(f, s) end)
                local vertices = {}
                for v = 1, (okV and nVertices or 0) do
                    local ok, value = dpCall(function() return meshD:getVertex(f, s, v) end)
                    if not ok or not value then vertices = nil; break end
                    vertices[v] = value
                end
                if vertices then
                    local okI, nIndices = dpCall(function() return meshD:getTotalIndex(f, s) end)
                    local indices = {}
                    if okI and nIndices and nIndices > 0 then
                        local ok, value = dpCall(function() return meshD:getIndex(f, s) end)
                        indices = (ok and value) or {}
                    else
                        for v = 1, #vertices do indices[v] = v end
                    end
                    local okT, texture = dpCall(function() return meshD:getTexture(f, s) end)
                    texture = okT and texture or ''
                    local signature = splitCaptureSignature(vertices, indices, texture)
                    if not captured[tostring(f) .. ':' .. signature] then
                        local materialTextures = {}
                        for _, role in ipairs({'normal', 'specular', 'emissive', 'mask'}) do
                            local okM, value = dpCall(function() return meshD:getMaterialTexture(f, s, role) end)
                            if okM and value and value ~= '' then materialTextures[role] = value end
                        end
                        local selected = {{}, {}, {}, {}}
                        for i = 1, #indices - 2, 3 do
                            local tri = {indices[i], indices[i+1], indices[i+2]}
                            local a, b, c = vertices[tri[1]], vertices[tri[2]], vertices[tri[3]]
                            if a and b and c then
                                local ai, bi, ci = splitCapturePointInside(a, box), splitCapturePointInside(b, box), splitCapturePointInside(c, box)
                                local center = {x=(a.x+b.x+c.x)/3, y=(a.y+b.y+c.y)/3, z=(a.z+b.z+c.z)/3}
                                if splitCapturePointInside(center, box) then table.insert(selected[1], tri) end
                                if ai and bi and ci then table.insert(selected[2], tri) end
                                if ai or bi or ci then table.insert(selected[3], tri) end
                                if splitCaptureTriangleIntersectsBox(a, b, c, box) then table.insert(selected[4], tri) end
                            end
                        end
                        for ai, algorithm in ipairs(algorithms) do
                            if #selected[ai] > 0 then
                                table.insert(algorithm.groups, {
                                    frame=f, subset=s, vertices=vertices, indices=indices,
                                    texture=texture, materialTextures=materialTextures,
                                    signature=signature, triangles=selected[ai],
                                    islands=splitCaptureBuildIslands(selected[ai], vertices),
                                })
                            end
                        end
                    end
                end
            end
        end
    end
    local analysis = {
        algorithms=algorithms, selected=1, filterIslands=false,
        threshold=10, appliedThreshold=10,
    }
    splitCaptureRefreshResolved(analysis)
    return analysis
end

function splitCaptureResolveAlgorithm(algorithm, filterIslands, threshold)
    local resolved = {groups={}, faces=0, vertices=0, islands=0, removed=0, frames=0, islandMarkers={}}
    local affectedFrames = {}
    for _, group in ipairs(algorithm.groups) do
        local kept, islands = {}, group.islands or {}
        local largest = islands[1] and #islands[1] or 0
        for islandIndex, island in ipairs(islands) do
            local keep = not filterIslands or islandIndex == 1 or #island >= largest * threshold * 0.01
            if filterIslands then
                local minX, minY, minZ, maxX, maxY, maxZ
                for _, tri in ipairs(island) do
                    for _, vi in ipairs(tri) do
                        local vertex = group.vertices[vi]
                        if vertex then
                            minX = not minX and vertex.x or math.min(minX, vertex.x)
                            minY = not minY and vertex.y or math.min(minY, vertex.y)
                            minZ = not minZ and vertex.z or math.min(minZ, vertex.z)
                            maxX = not maxX and vertex.x or math.max(maxX, vertex.x)
                            maxY = not maxY and vertex.y or math.max(maxY, vertex.y)
                            maxZ = not maxZ and vertex.z or math.max(maxZ, vertex.z)
                        end
                    end
                end
                if minX then
                    local extent = math.max(maxX-minX, maxY-minY, maxZ-minZ)
                    table.insert(resolved.islandMarkers, {
                        x=(minX+maxX)*0.5, y=(minY+maxY)*0.5, z=(minZ+maxZ)*0.5,
                        size=math.max(extent*0.06, 0.05), removed=not keep,
                    })
                end
            end
            if keep then
                for _, tri in ipairs(island) do table.insert(kept, tri) end
            else
                resolved.removed = resolved.removed + 1
            end
        end
        if #kept > 0 then
            local used = {}
            for _, tri in ipairs(kept) do for _, vi in ipairs(tri) do used[vi] = true end end
            local vertexCount = 0
            for _ in pairs(used) do vertexCount = vertexCount + 1 end
            table.insert(resolved.groups, {
                frame=group.frame, subset=group.subset, vertices=group.vertices,
                indices=group.indices, texture=group.texture,
                materialTextures=group.materialTextures, signature=group.signature,
                triangles=kept,
            })
            resolved.faces = resolved.faces + #kept
            resolved.vertices = resolved.vertices + vertexCount
            affectedFrames[group.frame] = true
        end
        resolved.islands = resolved.islands + #islands
    end
    for _ in pairs(affectedFrames) do resolved.frames = resolved.frames + 1 end
    return resolved
end

function destroySplitCaptureIslandMarkers(tEntry)
    for _, marker in ipairs(tEntry.tSplitCaptureIslandMarkers or {}) do marker:destroy() end
    tEntry.tSplitCaptureIslandMarkers = nil
    tEntry.sSplitCaptureIslandMarkerKey = nil
end

function updateSplitCaptureIslandMarkers(tEntry, index, analysis, resolved)
    if not analysis.filterIslands or not analysis.showIslandCenters then
        destroySplitCaptureIslandMarkers(tEntry)
        return
    end
    local markerKey = tostring(analysis.resolvedCacheKey) .. ':' .. tostring(analysis.selected)
    if tEntry.sSplitCaptureIslandMarkerKey == markerKey and tEntry.tSplitCaptureIslandMarkers then return end
    destroySplitCaptureIslandMarkers(tEntry)
    tEntry.tSplitCaptureIslandMarkers = {}
    tEntry.iSplitCaptureIslandMarkerGeneration = (tEntry.iSplitCaptureIslandMarkerGeneration or 0) + 1
    for markerIndex, info in ipairs(resolved.islandMarkers or {}) do
        local marker = shape:new('3d', info.x, info.y, dodgeAutoZOrder(info.z))
        marker:create(unitSphereVerts(8, 12), nil, 'mesh_debug_capture_island_' .. index .. '_' ..
            tEntry.iSplitCaptureIslandMarkerGeneration .. '_' .. markerIndex)
        if info.removed then marker:setColor(1.0, 0.45, 0.05, 0.95)
        else marker:setColor(0.1, 1.0, 1.0, 0.95) end
        marker:setScale(info.size, info.size, info.size)
        marker.alwaysOnTop = true
        marker.visible = true
        table.insert(tEntry.tSplitCaptureIslandMarkers, marker)
    end
    tEntry.sSplitCaptureIslandMarkerKey = markerKey
end

-- Resolving an algorithm walks every selected triangle again to rebuild filtered groups and
-- unique-vertex totals. Cache all four rows and redo that work only when a filter input changes,
-- never once per rendered ImGui frame.
function splitCaptureRefreshResolved(analysis)
    local appliedThreshold = analysis.appliedThreshold or analysis.threshold or 10
    local cacheKey = tostring(analysis.filterIslands == true) .. ':' .. tostring(appliedThreshold)
    if analysis.resolvedCacheKey == cacheKey and analysis.resolved then return analysis.resolved end
    analysis.resolved = {}
    for algorithmIndex, algorithm in ipairs(analysis.algorithms) do
        analysis.resolved[algorithmIndex] = splitCaptureResolveAlgorithm(
            algorithm, analysis.filterIslands, appliedThreshold)
    end
    analysis.resolvedCacheKey = cacheKey
    return analysis.resolved
end

function splitCaptureApply(tEntry, meshD, resolved)
    tEntry.tSplitCapturedSignatures = tEntry.tSplitCapturedSignatures or {}
    local sourceHadNormals = tEntry.info and tEntry.info.hasNormal == true
    local sourceNormalStateKnown = tEntry.info and tEntry.info.hasNormal ~= nil
    table.sort(resolved.groups, function(left, right)
        return left.frame > right.frame or (left.frame == right.frame and left.subset > right.subset)
    end)
    for _, group in ipairs(resolved.groups) do
        if splitCaptureGetSubsetSignature(meshD, group.frame, group.subset) ~= group.signature then
            return nil, tLang.L('capture_mesh_changed')
        end
    end
    for _, group in ipairs(resolved.groups) do
        local chosen, chosenSet = group.triangles, {}
        for _, tri in ipairs(chosen) do chosenSet[table.concat(tri, ':')] = true end
        local outside = {}
        for i = 1, #group.indices - 2, 3 do
            local tri = {group.indices[i], group.indices[i+1], group.indices[i+2]}
            if not chosenSet[table.concat(tri, ':')] then table.insert(outside, tri) end
        end
        local outsideV, outsideI = splitCaptureGroup(group.vertices, outside)
        local insideV, insideI = splitCaptureGroup(group.vertices, chosen)
        meshD:removeSubset(group.frame, group.subset)
        if #outsideI > 0 then
            local newS = meshD:addSubSet(group.frame)
            meshD:addVertex(group.frame, newS, outsideV); meshD:addIndex(group.frame, newS, outsideI)
            splitCaptureSetSubsetTextures(meshD, group.frame, newS, group.texture, group.materialTextures)
        end
        local newS = meshD:addSubSet(group.frame)
        meshD:addVertex(group.frame, newS, insideV); meshD:addIndex(group.frame, newS, insideI)
        splitCaptureSetSubsetTextures(meshD, group.frame, newS, group.texture, group.materialTextures)
        tEntry.tSplitCapturedSignatures[tostring(group.frame) .. ':' .. splitCaptureSignature(insideV, insideI, group.texture)] = true
    end
    if resolved.faces > 0 and sourceNormalStateKnown and not sourceHadNormals then
        meshD:removeNormals(); tEntry.info.hasNormal = false
    end
    return resolved.faces, resolved.frames
end

function splitCaptureCommitAnalysis(tEntry, meshD, index, sp, resolved)
    if resolved.faces == 0 then tUtil.showMessageWarn(tLang.L('capture_no_faces')); return end
    local pendingBackup = splitCaptureCreateBackup(tEntry, meshD)
    if not pendingBackup then tUtil.showMessageWarn(tLang.L('capture_backup_failed')); return end
    local faces, framesOrError = splitCaptureApply(tEntry, meshD, resolved)
    if not faces then
        meshDebug:fakeRelease(pendingBackup.path); os.remove(pendingBackup.path)
        tUtil.showMessageWarn(framesOrError or tLang.L('capture_analysis_failed'))
        return
    end
    splitCaptureDiscardBackup(tEntry)
    tEntry.tSplitCaptureBackup = pendingBackup
    tEntry.modified = true
    tEntry.bNormalsVizDirty = true
    tEntry.bPhysicsVizDirty = true
    tEntry.tTransformBoundsCache = nil
    iLastPreviewedIndex = 0
    sp.lastFaces, sp.lastFrames = faces, framesOrError
    table.insert(tEntry.tSplitCaptures, {
        faces=faces, frames=framesOrError, x=sp.x, y=sp.y, z=sp.z,
        width=sp.width, height=sp.height, depth=sp.depth,
    })
    destroySplitCaptureIslandMarkers(tEntry)
    sp.analysis = nil
    tUtil.showMessage(string.format(tLang.L('capture_applied_fmt'), faces, framesOrError), 5)
end

function showSplitCaptureAnalysis(tEntry, meshD, index, sp)
    local analysis = sp.analysis
    if not analysis then return end
    tImGui.Text(tLang.L('capture_results'))
    local filtered = tImGui.Checkbox(tLang.L('capture_filter_islands') .. '##captureFilter-' .. index, analysis.filterIslands)
    analysis.filterIslands = filtered
    if tImGui.IsItemHovered(0) then
        tImGui.BeginTooltip()
        tImGui.PushTextWrapPos(400)
        tImGui.Text(tLang.L('capture_filter_islands_help'))
        tImGui.PopTextWrapPos()
        tImGui.EndTooltip()
    end
    if analysis.filterIslands then
        tImGui.PushItemWidth(150)
        local changed, threshold = tImGui.DragFloat(
            tLang.L('capture_island_threshold') .. '##captureThreshold-' .. index,
            analysis.threshold, 1, 1, 100, '%.0f%%')
        tImGui.PopItemWidth()
        if changed then analysis.threshold = math.max(1, math.min(100, threshold)) end
        -- DragFloat reports changes continuously while dragging (and while its +/- controls
        -- auto-repeat). Display that live value, but rebuild the expensive result sets only once
        -- the drag or keyboard edit is committed/deactivated.
        if tImGui.IsItemDeactivatedAfterEdit() then
            analysis.appliedThreshold = analysis.threshold
        end
        analysis.showIslandCenters = tImGui.Checkbox(
            tLang.L('capture_show_island_centers') .. '##captureIslandCenters-' .. index,
            analysis.showIslandCenters == true)
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.PushTextWrapPos(400)
            tImGui.Text(tLang.L('capture_show_island_centers_help'))
            tImGui.PopTextWrapPos()
            tImGui.EndTooltip()
        end
    end
    local resolvedResults = splitCaptureRefreshResolved(analysis)
    local flags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg', 'ImGuiTableFlags_ScrollX')
    if tImGui.BeginTable('captureResults-' .. index, 6, flags, {x=0, y=150}) then
        tImGui.TableSetupColumn(tLang.L('select'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 45)
        tImGui.TableSetupColumn(tLang.L('capture_algorithm'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 180)
        tImGui.TableSetupColumn(tLang.L('capture_faces'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 55)
        tImGui.TableSetupColumn(tLang.L('capture_vertices'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 65)
        tImGui.TableSetupColumn(tLang.L('capture_islands'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 55)
        tImGui.TableSetupColumn(tLang.L('capture_removed'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 100)
        tImGui.TableHeadersRow()
        for algorithmIndex, algorithm in ipairs(analysis.algorithms) do
            local result = resolvedResults[algorithmIndex]
            tImGui.TableNextRow()
            tImGui.TableNextColumn()
            analysis.selected = tImGui.RadioButton(
                '##captureAlgorithm-' .. index .. '-' .. algorithmIndex,
                analysis.selected, algorithmIndex)
            tImGui.TableNextColumn(); tImGui.Text(algorithm.label)
            tImGui.TableNextColumn(); tImGui.Text(tostring(result.faces))
            tImGui.TableNextColumn(); tImGui.Text(tostring(result.vertices))
            tImGui.TableNextColumn(); tImGui.Text(tostring(result.islands))
            tImGui.TableNextColumn(); tImGui.Text(tostring(result.removed))
        end
        tImGui.EndTable()
    end
    local selected = resolvedResults[analysis.selected]
    updateSplitCaptureIslandMarkers(tEntry, index, analysis, selected)
    if tImGui.Button(tLang.L('capture_apply') .. '##captureApply-' .. index) then
        splitCaptureCommitAnalysis(tEntry, meshD, index, sp, selected)
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('cancel') .. '##captureCancel-' .. index) then
        destroySplitCaptureIslandMarkers(tEntry)
        sp.analysis = nil
    end
end

function saveCapturedSplitAs(tEntry)
    local captured = tEntry.tSplitCapturedSignatures or {}
    if next(captured) == nil then
        tUtil.showMessageWarn('No captured groups to save.')
        return
    end
    local newFile = mbm.saveFile(sLastMeshPath, 'msh')
    if not newFile or newFile == '' then return end

    local sourceD = tEntry.meshDebug
    local tempPath
    local sourceForCopy = sourceD
    if tEntry.modified then
        tempPath = os.tmpname() .. '.msh'
        if not sourceD:save(tempPath, false, false) then
            tUtil.showMessageWarn('Could not prepare captured groups for saving.')
            return
        end
        sourceForCopy = meshDebug:new()
        if not sourceForCopy:load(tempPath) then
            meshDebug:fakeRelease(tempPath)
            os.remove(tempPath)
            tUtil.showMessageWarn('Could not reload captured groups for saving.')
            return
        end
        meshDebug:fakeRelease(tempPath)
        os.remove(tempPath)
    else
        sourceForCopy = meshDebug:new()
        if not sourceForCopy:load(tEntry.fileName) then
            tUtil.showMessageWarn('Could not load mesh for captured-group save.')
            return
        end
    end

    local okF, nFrames = dpCall(function() return sourceForCopy:getTotalFrame() end)
    local keepFrame, oldToNew = {}, {}
    local newFrame = 0
    for f = 1, (okF and nFrames or 0) do
        local okS, nSubsets = dpCall(function() return sourceForCopy:getTotalSubset(f) end)
        local hasCaptured = false
        for s = 1, (okS and nSubsets or 0) do
            local signature = splitCaptureGetSubsetSignature(sourceForCopy, f, s)
            if signature and captured[tostring(f) .. ':' .. signature] then hasCaptured = true; break end
        end
        if hasCaptured then
            newFrame = newFrame + 1
            keepFrame[f], oldToNew[f] = true, newFrame
        end
    end
    if newFrame == 0 then
        sourceForCopy = nil -- meshDebug userdata is released by Lua GC; it has no :destroy() method
        tUtil.showMessageWarn('No captured groups remain to save.')
        return
    end

    local nAnim = (tEntry.info and tEntry.info.animation) or 0
    for i = nAnim, 1, -1 do
        local ok, name, initial, final, time, typ = dpCall(function() return sourceForCopy:getAnim(i) end)
        if ok and name and initial and final then
            local newInitial, newFinal
            for f = initial, final do
                if oldToNew[f] then
                    newInitial = newInitial or oldToNew[f]
                    newFinal = oldToNew[f]
                end
            end
            if not newInitial then sourceForCopy:removeAnim(i)
            else sourceForCopy:updateAnim(i, name, newInitial, newFinal, time or 0.1, typ or 0) end
        end
    end

    for f = (okF and nFrames or 0), 1, -1 do
        if keepFrame[f] then
            local okS, nSubsets = dpCall(function() return sourceForCopy:getTotalSubset(f) end)
            for s = (okS and nSubsets or 0), 1, -1 do
                local signature = splitCaptureGetSubsetSignature(sourceForCopy, f, s)
                if not (signature and captured[tostring(f) .. ':' .. signature]) then
                    sourceForCopy:removeSubset(f, s)
                end
            end
        else
            sourceForCopy:removeFrame(f)
        end
    end

    local okSave = sourceForCopy:save(newFile, false, false)
    sourceForCopy = nil -- meshDebug userdata is released by Lua GC; it has no :destroy() method
    if okSave then
        sLastMeshPath = newFile
        tUtil.showMessage('Captured groups saved: ' .. tUtil.getShortName(newFile), 5)
    else
        tUtil.showMessageWarn('Could not save captured groups.')
    end
end

function showSplitCapture(tEntry, meshD, index)
    local sp = tEntry.tSplitCapture
    if not sp then return end
    tImGui.Separator()
    tImGui.Text('Split')
    local oldActive = sp.active == true
    local newActive = tImGui.Checkbox('Start Capture##splitCapture-' .. index, oldActive)
    sp.active = newActive
    if newActive and not oldActive then
        destroySplitCaptureIslandMarkers(tEntry)
        sp.analysis = nil
        -- Refresh this for every capture because the live mesh may have changed since the cube
        -- was first initialized. A tiny cube inside a very large mesh must still traverse the
        -- mesh at a useful speed instead of deriving its DragFloat step from the cube alone.
        local aabb = computeMeshAABB(meshD)
        sp.dragReferenceSize = aabb and math.max(
            aabb.maxX - aabb.minX,
            aabb.maxY - aabb.minY,
            aabb.maxZ - aabb.minZ
        ) or nil
        if not sp.initialized then
            local cx = aabb and (aabb.minX + aabb.maxX) * 0.5 or 0
            local cy = aabb and (aabb.minY + aabb.maxY) * 0.5 or 0
            local cz = aabb and (aabb.minZ + aabb.maxZ) * 0.5 or 0
            sp.x, sp.y, sp.z = cx, cy, cz
            sp.width = math.max(aabb and (aabb.maxX - aabb.minX) * 0.25 or 100, 1)
            sp.height = math.max(aabb and (aabb.maxY - aabb.minY) * 0.25 or 100, 1)
            sp.depth = math.max(aabb and (aabb.maxZ - aabb.minZ) * 0.25 or 100, 1)
            sp.initialized = true
        end
        splitCaptureBuildBox(sp)
        tEntry.sSplitDragging = nil
        sp.dragPlanePoint, sp.dragPlaneNormal, sp.dragOffset = nil, nil, nil
    elseif not newActive and oldActive then
        local analysis, analysisError = splitCaptureAnalyze(tEntry, meshD, sp)
        splitCaptureDestroy(sp)
        tEntry.sSplitDragging = nil
        sp.dragPlanePoint, sp.dragPlaneNormal, sp.dragOffset = nil, nil, nil
        sp.analysis = analysis
        if not analysis then tUtil.showMessageWarn(analysisError or tLang.L('capture_analysis_failed')) end
    end
    if sp.active then
        local hoverKind, hoverAxis = nil, nil
        -- Use the larger of the whole mesh bounds and capture cube. The mesh reference keeps a
        -- small cube responsive across a large model; the cube reference remains useful if the
        -- user deliberately grows it beyond the original mesh bounds.
        local dragSpeed = math.max(
            math.max(sp.dragReferenceSize or 0, sp.width, sp.height, sp.depth) * 0.0025,
            0.01
        )
        tImGui.Text('Center')
        tImGui.PushItemWidth(120)
        local changedX, x = tImGui.DragFloat('X##splitCenterX-' .. index, sp.x, dragSpeed, 0, 0, '%.2f')
        if tImGui.IsItemHovered(0) then hoverKind, hoverAxis = 'center', 'x' end
        tImGui.SameLine()
        local changedY, y = tImGui.DragFloat('Y##splitCenterY-' .. index, sp.y, dragSpeed, 0, 0, '%.2f')
        if tImGui.IsItemHovered(0) then hoverKind, hoverAxis = 'center', 'y' end
        tImGui.SameLine()
        local changedZ, z = tImGui.DragFloat('Z##splitCenterZ-' .. index, sp.z, dragSpeed, 0, 0, '%.2f')
        if tImGui.IsItemHovered(0) then hoverKind, hoverAxis = 'center', 'z' end
        tImGui.PopItemWidth()

        tImGui.Text('Size')
        tImGui.PushItemWidth(120)
        local changedW, width = tImGui.DragFloat('X##splitSizeX-' .. index, sp.width, dragSpeed, 0.01, 0, '%.2f')
        if tImGui.IsItemHovered(0) then hoverKind, hoverAxis = 'size', 'x' end
        tImGui.SameLine()
        local changedH, height = tImGui.DragFloat('Y##splitSizeY-' .. index, sp.height, dragSpeed, 0.01, 0, '%.2f')
        if tImGui.IsItemHovered(0) then hoverKind, hoverAxis = 'size', 'y' end
        tImGui.SameLine()
        local changedD, depth = tImGui.DragFloat('Z##splitSizeZ-' .. index, sp.depth, dragSpeed, 0.01, 0, '%.2f')
        if tImGui.IsItemHovered(0) then hoverKind, hoverAxis = 'size', 'z' end
        tImGui.PopItemWidth()

        local centerChanged = changedX or changedY or changedZ
        local sizeChanged = changedW or changedH or changedD
        if centerChanged or sizeChanged then
            tEntry.sSplitDragging = nil
            sp.dragPlanePoint, sp.dragPlaneNormal, sp.dragOffset = nil, nil, nil
            sp.x, sp.y, sp.z = changedX and x or sp.x, changedY and y or sp.y, changedZ and z or sp.z
            sp.width = changedW and math.max(width, 0.01) or sp.width
            sp.height = changedH and math.max(height, 0.01) or sp.height
            sp.depth = changedD and math.max(depth, 0.01) or sp.depth
            if sizeChanged then
                splitCaptureBuildBox(sp)
            else
                splitCaptureMoveBox(sp)
            end
        end
        splitCaptureSetHover(sp, hoverKind, hoverAxis)
        if sp.lastFaces then tImGui.Text(string.format('Last capture: %d face(s)', sp.lastFaces)) end
    end
    if not sp.active then showSplitCaptureAnalysis(tEntry, meshD, index, sp) end
    if tEntry.tSplitCaptures and #tEntry.tSplitCaptures > 0 then
        tImGui.Text('Captured groups: ' .. tostring(#tEntry.tSplitCaptures))
        for i, cap in ipairs(tEntry.tSplitCaptures) do
            tImGui.Text(string.format('%d: %d face(s), %d frame(s)', i, cap.faces, cap.frames))
        end
        if tImGui.Button(tLang.L('save_captured_as') .. '##saveCaptured-' .. index) then
            saveCapturedSplitAs(tEntry)
        end
    end
end

-- ---------------------------------------------------------------------------
-- Frame tree node: view/queue removals, open Frame Pick
-- ---------------------------------------------------------------------------
function showFrameNode(tEntry, meshD, index)
    local nodeFlags = 0
    local wantOpen = (tEntry.sOpenNode == 'frameNode')
    tImGui.SetNextItemOpen(wantOpen, tImGui.Flags('ImGuiCond_Always'))
    local isOpen = tImGui.TreeNodeEx(tLang.L('frame_node') .. '##frameNode-' .. index, nodeFlags)
    if tImGui.IsItemClicked() then
        tEntry.sOpenNode = wantOpen and nil or 'frameNode'
    end
    if not isOpen then
        destroySplitCaptureIslandMarkers(tEntry)
        return
    end

    local okTF, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okTF then nFrames = 0 end
    nFrames = nFrames or 0

    -- Build full subset list (all frames, all subsets)
    local allSubsets = {}
    for f = 1, nFrames do
        local okS, nSubs = dpCall(function() return meshD:getTotalSubset(f) end)
        for s = 1, (okS and nSubs or 0) do
            local okT, tex = dpCall(function() return meshD:getTexture(f, s) end)
            local okV, vertexCount = dpCall(function() return meshD:getTotalVertex(f, s) end)
            local texName = (okT and tex and tex ~= '') and (' [' .. tUtil.getShortName(tex) .. ']') or ''
            table.insert(allSubsets, {
                f = f,
                s = s,
                texName = texName,
                vertexCount = okV and vertexCount or nil,
            })
        end
    end
    -- Newly-created subsets (including split-capture outputs) have no entry yet in the
    -- visibility table. Missing means visible by default; only an explicit false disables it.
    for f = 1, nFrames do
        if tEntry.tCheckedRemove[f * 100] == nil then tEntry.tCheckedRemove[f * 100] = true end
        local okS, nSubs = dpCall(function() return meshD:getTotalSubset(f) end)
        for s = 1, (okS and nSubs or 0) do
            local key = f * 100 + s
            if tEntry.tCheckedRemove[key] == nil then tEntry.tCheckedRemove[key] = true end
        end
    end

    -- Helper: is a frame queued for removal?
    local pendingFrames   = {}
    local pendingSubsets  = {}   -- key = f*1000+s
    for _, op in ipairs(tEntry.tPendingOps) do
        if op.kind == 'removeFrame' then
            pendingFrames[op.frame] = true
        elseif op.kind == 'removeSubset' then
            pendingSubsets[op.frame * 1000 + op.subset] = true
        end
    end

    -- Per-frame aligned table: each frame occupies its own row group.
    -- Left column = frame checkbox; right column = that frame's subsets (one per row).
    -- A separator row divides consecutive frame groups.
    local listH      = math.min((nFrames + #allSubsets) * 22 + 8, 300)
    local tblFlags   = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_ScrollY')
    local frameSelChanged = false
    local requestedSubsetMove = nil

    -- Precompute: which frames have at least one subset staged for removal
    local tImplicit = {}
    for _, sub2 in ipairs(allSubsets) do
        if tEntry.tCheckedRemove[sub2.f * 100 + sub2.s] then
            tImplicit[sub2.f] = true
        end
    end

    if tImGui.BeginTable('fnOuter-' .. index, 4, tblFlags, {x=0, y=listH}) then
        tImGui.TableSetupScrollFreeze(0, 1)
        tImGui.TableSetupColumn(tLang.L('frame_node'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 0.35)
        tImGui.TableSetupColumn(tLang.L('subsets'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 0.65)
        tImGui.TableSetupColumn(tLang.L('subset_vertices'),
            tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 64)
        tImGui.TableSetupColumn(tLang.L('move_subset_up'),
            tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 48)
        tImGui.TableHeadersRow()

        for f = 1, nFrames do
            local key = f * 100
            -- Collect subsets that belong to this frame, in order
            local fSubsets = {}
            for _, sub in ipairs(allSubsets) do
                if sub.f == f then table.insert(fSubsets, sub) end
            end

            -- Helper: render one subset cell (called for column 1 of any row in this group)
            local function renderSubsetCell(sub)
                local subKey = sub.f * 100 + sub.s
                local label  = 'F' .. sub.f .. ' S' .. sub.s .. sub.texName
                if pendingFrames[f] or pendingSubsets[f * 1000 + sub.s] then
                    tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), 0.9, 0.3, 0.3, 1)
                    tImGui.Text('[DEL] ' .. label)
                    tImGui.PopStyleColor()
                else
                    local checked    = tEntry.tCheckedRemove[subKey] or false
                    local newChecked = tImGui.Checkbox(label .. '##fnrcb-' .. index .. '-' .. sub.f .. '-' .. sub.s, checked)
                    tEntry.tCheckedRemove[subKey] = newChecked
                    if newChecked ~= checked then
                        frameSelChanged = true
                        if not newChecked then
                            -- auto-clear frame key when every subset of this frame is unchecked
                            local allUnchecked = true
                            for _, sub3 in ipairs(allSubsets) do
                                if sub3.f == f and (tEntry.tCheckedRemove[sub3.f * 100 + sub3.s] or false) then
                                    allUnchecked = false; break
                                end
                            end
                            if allUnchecked then tEntry.tCheckedRemove[f * 100] = false end
                        end
                    end
                end
            end

            local function renderMoveSubsetUp(sub)
                if sub.s <= 1 or pendingFrames[f] then
                    tImGui.TextDisabled('-')
                    return
                end
                if tImGui.ArrowButton('##fnMoveUp-' .. index .. '-' .. sub.f .. '-' .. sub.s,
                                      tImGui.Flags('ImGuiDir_Up')) then
                    requestedSubsetMove = {frame = sub.f, subset = sub.s}
                end
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('move_subset_up_tooltip'))
                    tImGui.EndTooltip()
                end
            end

            -- ── First row of this frame group ────────────────────────────
            tImGui.TableNextRow()
            tImGui.TableSetColumnIndex(0)
            if pendingFrames[f] then
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), 0.9, 0.3, 0.3, 1)
                tImGui.Text('[DEL] Frame ' .. f)
                tImGui.PopStyleColor()
            else
                local explicitChecked = tEntry.tCheckedRemove[key] or false
                local displayChecked  = explicitChecked or (tImplicit[f] or false)
                local newChecked = tImGui.Checkbox('Frame ' .. f .. '##fnlcb-' .. index .. '-' .. f, displayChecked)
                if newChecked ~= displayChecked then
                    frameSelChanged = true
                    tEntry.tCheckedRemove[key] = newChecked
                    for _, sub2 in ipairs(allSubsets) do
                        if sub2.f == f then
                            tEntry.tCheckedRemove[sub2.f * 100 + sub2.s] = newChecked
                        end
                    end
                else
                    tEntry.tCheckedRemove[key] = explicitChecked
                end
            end
            tImGui.TableSetColumnIndex(1)
            if #fSubsets >= 1 then renderSubsetCell(fSubsets[1]) end
            tImGui.TableSetColumnIndex(2)
            if #fSubsets >= 1 then
                tImGui.Text(fSubsets[1].vertexCount and tostring(fSubsets[1].vertexCount) or '-')
            end
            tImGui.TableSetColumnIndex(3)
            if #fSubsets >= 1 then renderMoveSubsetUp(fSubsets[1]) end

            -- ── Additional rows: blank left, one more subset on right ────
            for i = 2, #fSubsets do
                tImGui.TableNextRow()
                tImGui.TableSetColumnIndex(1)
                renderSubsetCell(fSubsets[i])
                tImGui.TableSetColumnIndex(2)
                tImGui.Text(fSubsets[i].vertexCount and tostring(fSubsets[i].vertexCount) or '-')
                tImGui.TableSetColumnIndex(3)
                renderMoveSubsetUp(fSubsets[i])
            end

            -- ── Thin separator row between frame groups ──────────────────
            if f < nFrames then
                tImGui.TableNextRow()
                tImGui.TableSetColumnIndex(0)
                tImGui.Separator()
                tImGui.TableSetColumnIndex(1)
                tImGui.Separator()
                tImGui.TableSetColumnIndex(2)
                tImGui.Separator()
                tImGui.TableSetColumnIndex(3)
                tImGui.Separator()
            end
        end

        tImGui.EndTable()
    end

    if requestedSubsetMove then
        local moveFrame = requestedSubsetMove.frame
        local moveSubset = requestedSubsetMove.subset
        local okMove, moved = dpCall(function()
            return meshD:moveSubsetUp(moveFrame, moveSubset)
        end)
        if okMove and moved then
            local previousSubset = moveSubset - 1
            local currentKey = moveFrame * 100 + moveSubset
            local previousKey = moveFrame * 100 + previousSubset
            tEntry.tCheckedRemove[currentKey], tEntry.tCheckedRemove[previousKey] =
                tEntry.tCheckedRemove[previousKey], tEntry.tCheckedRemove[currentKey]

            -- Pending removals identify subset indices too, so keep them attached to the same
            -- geometry while the two adjacent entries exchange positions.
            for _, op in ipairs(tEntry.tPendingOps) do
                if op.kind == 'removeSubset' and op.frame == moveFrame then
                    if op.subset == moveSubset then
                        op.subset = previousSubset
                    elseif op.subset == previousSubset then
                        op.subset = moveSubset
                    end
                end
            end
            tEntry.modified = true
            tEntry.bFrameSelectionDirty = true
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
        end
    end

    -- Auto-refresh preview when any checkbox changed
    if frameSelChanged then
        -- Rebuild tFrameSelection as positive selection:
        -- checked frame/subset = shown; nothing checked = show all
        local anyChecked = false
        for _, v in pairs(tEntry.tCheckedRemove) do
            if v then anyChecked = true; break end
        end
        if anyChecked then
            -- Recompute implicit (with updated tCheckedRemove)
            local newImplicit = {}
            for _, sub2 in ipairs(allSubsets) do
                if tEntry.tCheckedRemove[sub2.f * 100 + sub2.s] then
                    newImplicit[sub2.f] = true
                end
            end
            for f = 1, nFrames do
                tEntry.tFrameSelection[f] =
                    (tEntry.tCheckedRemove[f * 100] or false) or (newImplicit[f] or false)
            end
        else
            for f = 1, nFrames do
                tEntry.tFrameSelection[f] = true
            end
        end
        tEntry.bFrameSelectionDirty = true
        if index == iSelectedMeshIndex and tEntry.bAutoRefreshPreview and not tEntry.modified then
            refreshFrameFilterPreview(tEntry, index)
            tEntry.bFrameSelectionDirty = false
        end
    end

    -- Preview refresh controls (same pattern as Animations section)
    if index == iSelectedMeshIndex then
        if tEntry.modified then
            tImGui.BeginDisabled(true)
            tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=1,b=0,a=1})
            tImGui.Button(tLang.L("save_first_to_preview") .. '##rfpBtnFN-' .. index)
            tImGui.PopStyleColor(1)
            tImGui.EndDisabled()
        elseif not tEntry.bAutoRefreshPreview then
            if tImGui.Button('Refresh Preview##rfpBtnFN-' .. index) then
                refreshFrameFilterPreview(tEntry, index)
                tEntry.bFrameSelectionDirty = false
            end
            tImGui.SameLine()
        end
        local autoVal = tImGui.Checkbox(tLang.L('auto_refresh') .. '##autoRefreshFN-' .. index, tEntry.bAutoRefreshPreview and not tEntry.modified)
        if not tEntry.modified then
            tEntry.bAutoRefreshPreview = autoVal
        end
    end

    tImGui.Separator()

    -- Pending ops list
    if #tEntry.tPendingOps > 0 then
        tImGui.Text(tLang.L('pending_ops') .. ':')
        for i, op in ipairs(tEntry.tPendingOps) do
            local desc
            if     op.kind == 'removeFrame'  then desc = '[DEL] Frame ' .. op.frame
            elseif op.kind == 'removeSubset' then desc = '[DEL] Frame ' .. op.frame .. ' Subset ' .. op.subset
            elseif op.kind == 'copyFrame'    then desc = '+ Frame (import)'
            elseif op.kind == 'copySubset'   then desc = '+ Subset (import)'
            else                                  desc = op.kind
            end
            tImGui.BulletText(i .. '. ' .. desc)
        end
    end

    -- Execute and Cancel immediately after the pending-operation list. Execute mutates frame and
    -- subset counts, so stop drawing this node for the current ImGui pass; the next pass rebuilds
    -- nFrames/allSubsets from the updated mesh instead of using the stale lists computed above.
    if #tEntry.tPendingOps > 0 then
        if tImGui.Button(tLang.L('execute_ops') .. '##fnex-' .. index) then
            executeFrameOps(tEntry, meshD, index)
            tImGui.TreePop()
            return
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('cancel') .. '##fnclr-' .. index) then
            tEntry.tPendingOps = {}
        end
        tImGui.Separator()
        tImGui.NewLine()
    end

    -- ── Buttons ────────────────────────────────────────────────────────
    -- Check if any checkbox is ticked
    local anyChecked = false
    for _, v in pairs(tEntry.tCheckedRemove) do
        if v then anyChecked = true; break end
    end

    -- Remove selected: queue all checked items, then clear tCheckedRemove
    if anyChecked then
        if tImGui.Button(tLang.L('remove_selected') .. '##fnrm-' .. index) then
            for key, checked in pairs(tEntry.tCheckedRemove) do
                if checked then
                    local f = math.floor(key / 100)
                    local s = key % 100
                    if s > 0 then
                        -- subset removal
                        local already = false
                        for _, op in ipairs(tEntry.tPendingOps) do
                            if op.kind == 'removeSubset' and op.frame == f and op.subset == s then
                                already = true; break
                            end
                        end
                        if not already then
                            table.insert(tEntry.tPendingOps, {kind='removeSubset', frame=f, subset=s})
                        end
                    else
                        -- whole frame removal
                        local already = false
                        for _, op in ipairs(tEntry.tPendingOps) do
                            if op.kind == 'removeFrame' and op.frame == f then
                                already = true; break
                            end
                        end
                        if not already then
                            table.insert(tEntry.tPendingOps, {kind='removeFrame', frame=f})
                        end
                    end
                end
            end
            tEntry.tCheckedRemove = {}
            for f2 = 1, nFrames do
                tEntry.tCheckedRemove[f2 * 100] = true
                tEntry.tFrameSelection[f2] = true
            end
            for _, sub3 in ipairs(allSubsets) do
                tEntry.tCheckedRemove[sub3.f * 100 + sub3.s] = true
            end
        end
    end

    -- Remove unselected (always visible)
    if tImGui.Button(tLang.L('remove_unselected') .. '##fnrmu-' .. index) then
        for f = 1, nFrames do
            if not pendingFrames[f] then
                local fChecked = tEntry.tCheckedRemove[f * 100] or false
                local fSubsets, anySubChecked = {}, false
                for _, sub2 in ipairs(allSubsets) do
                    if sub2.f == f then
                        table.insert(fSubsets, sub2.s)
                        if tEntry.tCheckedRemove[f * 100 + sub2.s] then anySubChecked = true end
                    end
                end
                if fChecked or anySubChecked then
                    -- Frame or at least one subset is selected: remove only the unselected subsets
                    for _, s in ipairs(fSubsets) do
                        if not (tEntry.tCheckedRemove[f * 100 + s] or false)
                           and not pendingSubsets[f * 1000 + s] then
                            table.insert(tEntry.tPendingOps, {kind='removeSubset', frame=f, subset=s})
                        end
                    end
                else
                    -- Nothing selected for this frame: remove the whole frame
                    local already = false
                    for _, op in ipairs(tEntry.tPendingOps) do
                        if op.kind == 'removeFrame' and op.frame == f then already = true; break end
                    end
                    if not already then
                        table.insert(tEntry.tPendingOps, {kind='removeFrame', frame=f})
                    end
                end
            end
        end
        tEntry.tCheckedRemove = {}
        for f2 = 1, nFrames do
            tEntry.tCheckedRemove[f2 * 100] = true
            tEntry.tFrameSelection[f2] = true
        end
        for _, sub3 in ipairs(allSubsets) do
            tEntry.tCheckedRemove[sub3.f * 100 + sub3.s] = true
        end
    end
    -- Import from file (always visible)
    if tImGui.Button(tLang.L('import_frames_from_file') .. '##fnif-' .. index) then
        tEntry.bShowFramePick = true
        tEntry.tRightChecked  = {}
    end
    if tImGui.Button(tLang.L('export_frame_subset_files') .. '##fnExportSubset-' .. index) then
        exportSelectedFrameSubsets(tEntry)
    end

    tEntry.tSplitCapture = tEntry.tSplitCapture or {active=false, initialized=false}
    tEntry.tSplitCaptures = tEntry.tSplitCaptures or {}
    showSplitCapture(tEntry, meshD, index)

    -- Save As (visible after Execute, when mesh is modified in-memory)
    if tEntry.modified then
        if tImGui.Button(tLang.L('save_frame_sel_as') .. '##fnsa-' .. index) then
            local info    = tEntry.info or {}
            local extMap  = { mesh = 'msh', sprite = 'spt', font = 'fnt', tile = 'tile', particle = 'ptl' }
            local ext     = extMap[info.type] or 'msh'
            local newFile = mbm.saveFile(sLastMeshPath, ext)
            if newFile and newFile ~= '' then
                local wasLegacy = isLegacyTextureAnimationEffectStorage(tEntry.info)
                if meshD:save(newFile, false, false) then
                    local newInfo = refreshEntryInfoFromFile(nil, newFile)
                    if wasLegacy and not isLegacyTextureAnimationEffectStorage(newInfo) then
                        tUtil.showMessage(string.format(tLang.L('mesh_migrated_save_fmt'), tUtil.getShortName(newFile)))
                    else
                        tUtil.showMessage(string.format(tLang.L('save_as_success_fmt'), tUtil.getShortName(newFile)))
                    end
                    sLastMeshPath = newFile
                else
                    tUtil.showMessageWarn(string.format(tLang.L('save_failed_fmt'), tUtil.getShortName(tEntry.fileName)))
                end
            end
        end
    end
    -- Saving clears `modified`, but it does not end the editor session or discard the temporary
    -- pre-capture snapshot. Keep rollback available until it is used, superseded by another
    -- successful capture, or the mesh is removed from the editor.
    if tEntry.tSplitCaptureBackup and
            tImGui.Button(tLang.L('revert_last_capture') .. '##revertCapture-' .. index) then
        splitCaptureRevert(tEntry, index)
    end

    tImGui.TreePop()
end

-- Returns nil when all animation frame indices are within bounds,
-- or an error string listing every violation.
function collectAnimFrameErrors(tEntry)
    local meshD  = tEntry.meshDebug
    local info   = tEntry.info or {}
    local okTF, nTotalF = dpCall(function() return meshD:getTotalFrame() end)
    local totalF = (okTF and nTotalF) or (info.totalFrames or 0)
    if totalF == 0 then return nil end
    local errors = {}
    local nAnimC = info.animation or 0
    for ai = 1, nAnimC do
        local okA, aName, initF, finF = dpCall(function() return meshD:getAnim(ai) end)
        if okA and aName then
            if (initF or 0) > totalF then
                errors[#errors+1] = string.format('Anim %d (%s): initFrame %d > totalFrames %d', ai, aName, initF, totalF)
            end
            if (finF or 0) > totalF then
                errors[#errors+1] = string.format('Anim %d (%s): finFrame %d > totalFrames %d', ai, aName, finF, totalF)
            end
        end
    end
    return #errors > 0 and table.concat(errors, '\n') or nil
end

-- Renders one row's NX/NY/NZ DragFloats + status + Flip/Recompute buttons for local vertex
-- `v` of subset (1, s). `geo` is the (possibly nil) result of computeGeoNormalsForSubset for
-- this subset, reused for every row so it is computed once per subset per frame, not per row.
function showNormalVertexRow(tEntry, meshD, index, s, v, geo, triOk, vertices)
    local vd = vertices and vertices[v]
    if not vd then
        local okVd, loaded = dpCall(function() return meshD:getVertex(1, s, v) end)
        if not okVd or not loaded then return end
        vd = loaded
        if vertices then vertices[v] = vd end
    end
    local rid = index .. '-' .. s .. '-' .. v

    tImGui.TableNextRow()
    tImGui.TableNextColumn()
    tImGui.Text(tostring(v))

    local function commitNormal(nx, ny, nz)
        vd.nx, vd.ny, vd.nz = nx, ny, nz
        dpCall(function() meshD:setVertex(1, s, v, vd) end)
        tEntry.modified = true
        tEntry.bNormalsVizDirty = true
        if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
    end

    tImGui.PushItemWidth(52)
    tImGui.TableNextColumn()
    local chgX, nx = tImGui.DragFloat('##nx-' .. rid, vd.nx, 0.01, 0, 0, '%.2f')
    tImGui.TableNextColumn()
    local chgY, ny = tImGui.DragFloat('##ny-' .. rid, vd.ny, 0.01, 0, 0, '%.2f')
    tImGui.TableNextColumn()
    local chgZ, nz = tImGui.DragFloat('##nz-' .. rid, vd.nz, 0.01, 0, 0, '%.2f')
    tImGui.PopItemWidth()
    if chgX or chgY or chgZ then
        commitNormal(chgX and nx or vd.nx, chgY and ny or vd.ny, chgZ and nz or vd.nz)
    end

    -- Status + Flip/Recompute share one narrow column so the table stays compact.
    tImGui.TableNextColumn()
    local snx, sny, snz = normalizeVec3(vd.nx, vd.ny, vd.nz)
    if not snx then
        tImGui.TextDisabled(tLang.L('normal_status_zero'))
    elseif geo and geo[v] then
        local g = geo[v]
        local dot = snx * g.x + sny * g.y + snz * g.z
        if dot > 0 then
            tImGui.PushStyleColor('ImGuiCol_Text', {r = 0.3, g = 1, b = 0.3, a = 1})
            tImGui.Text(tLang.L('normal_status_ok'))
        else
            tImGui.PushStyleColor('ImGuiCol_Text', {r = 1, g = 0.3, b = 0.3, a = 1})
            tImGui.Text(tLang.L('normal_status_flipped'))
        end
        tImGui.PopStyleColor(1)
    else
        tImGui.TextDisabled('-')
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('normal_flip_short') .. '##nflip-' .. rid) then
        commitNormal(-vd.nx, -vd.ny, -vd.nz)
    end
    if tImGui.IsItemHovered(0) then
        tImGui.BeginTooltip()
        tImGui.Text(tLang.L('normal_flip'))
        tImGui.EndTooltip()
    end
    if triOk and geo and geo[v] then
        tImGui.SameLine()
        if tImGui.Button(tLang.L('normal_recompute_short') .. '##nrecalc-' .. rid) then
            local g = geo[v]
            commitNormal(g.x, g.y, g.z)
        end
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text(tLang.L('normal_recompute'))
            tImGui.EndTooltip()
        end
    end
end

-- Renders the per-subset header (bulk Flip/Recompute buttons + a scrollable vertex table)
-- for frame 1, subset s. Only called when meshType == 'mesh' and info.hasNormal.
function showNormalSubsetEditor(tEntry, meshD, index, s, triOk)
    local vertices = (tEntry.tNormalVertexCache or {})[s]
    local nV = vertices and #vertices or nil
    if not nV then
        local okNV, loadedCount = dpCall(function() return meshD:getTotalVertex(1, s) end)
        if not okNV then return end
        nV = loadedCount
    end
    if not nV or nV <= 0 then return end

    local label = string.format('%s %d (%d)', tLang.L('normal_subset_label'), s, nV)
    if not tImGui.TreeNodeEx(label .. '##nsub-' .. index .. '-' .. s, 0) then return end

    local geo = (tEntry.tNormalGeoCache or {})[s]
    if not geo then
        geo = triOk and computeGeoNormalsForSubset(meshD, 1, s) or {}
        tEntry.tNormalGeoCache = tEntry.tNormalGeoCache or {}
        tEntry.tNormalGeoCache[s] = geo
    end
    if not vertices then
        local okVerts, loaded = dpCall(function() return meshD:getVertex(1, s, 1, nV) end)
        vertices = (okVerts and loaded) or {}
        tEntry.tNormalVertexCache = tEntry.tNormalVertexCache or {}
        tEntry.tNormalVertexCache[s] = vertices
    end

    local function bulkUpdate(fn)
        for v = 1, nV do
            local vd = vertices[v]
            if vd then
                local nx, ny, nz = fn(v, vd, geo[v])
                if nx then
                    vd.nx, vd.ny, vd.nz = nx, ny, nz
                    dpCall(function() meshD:setVertex(1, s, v, vd) end)
                end
            end
        end
        tEntry.modified = true
        tEntry.bNormalsVizDirty = true
        if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
    end

    if tImGui.Button(tLang.L('normal_flip_all') .. '##nflipall-' .. index .. '-' .. s) then
        bulkUpdate(function(v, vd) return -vd.nx, -vd.ny, -vd.nz end)
    end
    if triOk then
        tImGui.SameLine()
        if tImGui.Button(tLang.L('normal_recompute_all') .. '##nrecalcall-' .. index .. '-' .. s) then
            bulkUpdate(function(v, vd, g) if g then return g.x, g.y, g.z end return nil end)
        end
    end

    -- ImGui still processes every submitted widget on every frame, even though the vertex data is
    -- cached. Keep each expanded subset bounded so large meshes do not stall the editor.
    tEntry.tNormalTablePages = tEntry.tNormalTablePages or {}
    local totalPages = math.max(1, math.ceil(nV / NORMAL_TABLE_PAGE_SIZE))
    local page = math.max(1, math.min(tEntry.tNormalTablePages[s] or 1, totalPages))

    if totalPages > 1 then
        tImGui.BeginDisabled(page <= 1)
        if tImGui.Button(tLang.L('normal_page_previous') .. '##nprev-' .. index .. '-' .. s) then
            page = page - 1
        end
        tImGui.EndDisabled()
        tImGui.SameLine()
        tImGui.BeginDisabled(page >= totalPages)
        if tImGui.Button(tLang.L('normal_page_next') .. '##nnext-' .. index .. '-' .. s) then
            page = page + 1
        end
        tImGui.EndDisabled()
    end

    page = math.max(1, math.min(page, totalPages))
    tEntry.tNormalTablePages[s] = page
    local firstVertex = (page - 1) * NORMAL_TABLE_PAGE_SIZE + 1
    local lastVertex = math.min(nV, firstVertex + NORMAL_TABLE_PAGE_SIZE - 1)
    if totalPages > 1 then
        tImGui.SameLine()
        tImGui.TextDisabled(string.format(tLang.L('normal_page_fmt'),
            page, totalPages, firstVertex, lastVertex, nV))
    end

    -- Kept intentionally narrow (Idx + NX/NY/NZ + one combined Status/Actions column) so the
    -- whole table fits inside the Mesh Tree window's default ~350px width; ScrollX is a safety
    -- net if the window is narrower still or a translation makes the status text wider.
    local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_ScrollY',
        'ImGuiTableFlags_ScrollX', 'ImGuiTableFlags_RowBg')
    local listH = math.min((lastVertex - firstVertex + 1) * 24 + 30, 260)
    if tImGui.BeginTable('nsubTbl-' .. index .. '-' .. s, 5, tblFlags, {x = 0, y = listH}) then
        tImGui.TableSetupScrollFreeze(0, 1)
        tImGui.TableSetupColumn(tLang.L('normal_col_idx'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 30)
        tImGui.TableSetupColumn('NX', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 56)
        tImGui.TableSetupColumn('NY', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 56)
        tImGui.TableSetupColumn('NZ', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 56)
        tImGui.TableSetupColumn(tLang.L('normal_col_status'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 110)
        tImGui.TableHeadersRow()
        for v = firstVertex, lastVertex do
            showNormalVertexRow(tEntry, meshD, index, s, v, geo, triOk, vertices)
        end
        tImGui.EndTable()
    end

    tImGui.TreePop()
end

-- Draws the per-vertex normal viewer/editor for frame 1 of any entry that has normals:
-- auto-builds the line visualization while the Normals node is open, and lists every subset's
-- vertices with inline nx/ny/nz editing plus Flip/Recompute actions.
function showNormalsEditor(tEntry, meshD, index)
    if not (tEntry.info or {}).hasNormal then return end

    local wantCoordType = bCameraMode3D and '3d' or '2dw'
    if not tEntry.tNormalLineGood or tEntry.bNormalsVizDirty or tEntry.sNormalVizCoordType ~= wantCoordType then
        rebuildNormalVisualization(tEntry, meshD)
    end

    local okMode, modeDraw = dpCall(function() return meshD:getModeDraw() end)
    local triOk = okMode and modeDraw == 'TRIANGLES'
    if not triOk then
        tImGui.TextDisabled(tLang.L('normal_non_triangle_note'))
    end
    if (tEntry.iNormalPreviewTotal or 0) > (tEntry.iNormalPreviewSegments or 0) then
        tImGui.TextDisabled(string.format(tLang.L('normal_preview_sampled_fmt'),
            tEntry.iNormalPreviewSegments or 0, tEntry.iNormalPreviewTotal or 0))
    end

    local okNS, nSubsets = dpCall(function() return meshD:getTotalSubset(1) end)
    if okNS and nSubsets then
        for s = 1, nSubsets do
            showNormalSubsetEditor(tEntry, meshD, index, s, triOk)
        end
    end
end

function showMeshOptions(tEntry, index)
    local meshD = tEntry.meshDebug
    local info = tEntry.info or {}
    local shortName = tUtil.getShortName(tEntry.fileName)
    local flags = 0

    local hasFilt = false
    for _, v in pairs(tEntry.tFrameSelection or {}) do
        if v == false then hasFilt = true; break end
    end

    local function onEdit()
        tEntry.modified = true
        if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
    end

    if tEntry.modified then
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=1,b=0,a=1})
        tImGui.Text(tLang.L("unsaved_changes"))
        tImGui.PopStyleColor(1)
    end

    if openNode(tEntry, 'meshinfo', tLang.L("mesh_info"), 0, 'meshinfo-' .. index) then
        showMeshInfoTable(tEntry, index)
        tImGui.TreePop()
    end

    if openNode(tEntry, 'material', tLang.L("material"), 0, 'material-' .. index) then
        showMaterialEditor(tEntry, index)
        tImGui.TreePop()
    end

    if openNode(tEntry, 'normals', tLang.L("normals_label"), 0, 'normals-' .. index) then
        if info and info.hasNormal then
            tImGui.TextDisabled('Has normals')
        else
            tImGui.TextDisabled('No normals')
        end
        if tImGui.Button(tLang.L("remove_normals") .. '##' .. index) then
            local nVertices = 0
            if info and info.hasNormal then
                nVertices = getMeshTotalVertices(meshD)
            end
            meshD:removeNormals()
            if tEntry.info then tEntry.info.hasNormal = false end
            tEntry.modified = true
            tEntry.bNormalsVizDirty = true
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
            if nVertices > 0 then
                local bytesSaved = nVertices * 12  -- 3 floats per normal
                tUtil.showMessage(string.format('Removed normals: %s\n%d vertices (~%s saved)', shortName, nVertices, formatBytes(bytesSaved)), 5)
            else
                tUtil.showMessage('Removed normals: ' .. shortName, 4)
            end
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L("add_normals") .. '##' .. index) then
            local nVertices = getMeshTotalVertices(meshD)
            meshD:addNormals()
            if tEntry.info then tEntry.info.hasNormal = true end
            tEntry.modified = true
            tEntry.bNormalsVizDirty = true
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
            if nVertices > 0 then
                tUtil.showMessage(string.format('Added normals: %s\n%d vertices', shortName, nVertices), 4)
            else
                tUtil.showMessage('Added normals: ' .. shortName, 4)
            end
        end
        showNormalsEditor(tEntry, meshD, index)
        tImGui.TreePop()
    elseif tEntry.tNormalLineGood or tEntry.tNormalLineBad then
        destroyNormalVisualization(tEntry)
    end

    -- Visualization only (list the shape types present) plus a "Reset physics to" shortcut --
    -- actually editing/authoring individual shapes stays in physic_editor.lua's own "Edit Physics"
    -- window, which is the only place with the primitive-by-primitive editing UI.
    if openNode(tEntry, 'physics', tLang.L("physics_label"), 0, 'physics-' .. index) then
        -- getPhysics() may materialize tens of thousands of triangle tables, while the true mesh
        -- bounds walk every vertex. Neither value changes merely because this node stays open.
        if tEntry.bPhysicsVizDirty then
            tEntry.tPhysicsCache = nil
            tEntry.tPhysicsCountsCache = nil
            tEntry.tPhysicsBoundsCache = nil
            tEntry.tPhysicsExtentCache = nil
        end
        if not tEntry.tPhysicsCache then
            tEntry.tPhysicsCache = meshD:getPhysics() or {}
            local counts = {}
            for _, shape in ipairs(tEntry.tPhysicsCache) do
                counts[shape.type] = (counts[shape.type] or 0) + 1
            end
            tEntry.tPhysicsCountsCache = counts
            if #tEntry.tPhysicsCache > 0 then
                tEntry.tPhysicsBoundsCache = computeMeshVertexBoundsFrame1(meshD)
                tEntry.tPhysicsExtentCache = {computePhysicsExtent(tEntry.tPhysicsCache)}
            end
        end
        local tPhysics = tEntry.tPhysicsCache
        -- Same rebuild-on-mismatch pattern as showNormalsEditor/rebuildNormalVisualization: the
        -- 2D/3D toggle (bCameraMode3D) changes the coord space the wireframe line must be created
        -- in, so a stale line built under the previous mode has to be thrown away and redrawn.
        local wantCoordType = bCameraMode3D and '3d' or '2dw'
        if not tEntry.tPhysicsLine or tEntry.bPhysicsVizDirty or tEntry.sPhysicsVizCoordType ~= wantCoordType then
            rebuildPhysicsVisualization(tEntry, meshD, tPhysics)
        end
        tImGui.HelpMarker(string.format(tLang.L('physics_edit_elsewhere_fmt'), tLang.L('physic_editor')))
        tImGui.SameLine()
        if #tPhysics == 0 then
            tImGui.TextDisabled(tLang.L('no_physics_available'))
        else
            local counts = tEntry.tPhysicsCountsCache or {}
            local tShapeLabels = {
                {'cube',     tLang.L('physics_shape_cube')},
                {'sphere',   tLang.L('physics_shape_sphere')},
                {'triangle', tLang.L('triangle')},
                {'complex',  tLang.L('physics_shape_complex')},
            }
            for _, pair in ipairs(tShapeLabels) do
                if counts[pair[1]] then
                    tImGui.Text(pair[2] .. ': ' .. counts[pair[1]])
                end
            end
        end

        -- Same check as Scene Editor 3D's Mesh Set thumbnail bake (scene_editor3d.lua,
        -- computeMeshTrueVertexExtentFrame1): a hand-authored physics box much smaller than the
        -- mesh's real frame-1 vertex extents means anything reading physics-derived size (physics
        -- itself, but also getAABB/getSize/getWidthHeight on the loaded RENDERIZABLE) is unreliable.
        local bounds = tEntry.tPhysicsBoundsCache
        if bounds then
            local extent = tEntry.tPhysicsExtentCache or {}
            local pw2, ph2, pd2 = extent[1], extent[2], extent[3]
            if pw2 then
                local trueMax = math.max(bounds.width, bounds.height, bounds.depth)
                local physMax = math.max(pw2, ph2, pd2)
                if trueMax > 1e-4 and physMax < trueMax * 0.5 then
                    tImGui.PushStyleColor('ImGuiCol_Text', {r = 1, g = 0.65, b = 0.2, a = 1})
                    tImGui.TextWrapped(tLang.L('physics_bounds_too_small_warning'))
                    tImGui.PopStyleColor(1)
                end
            end
        end

        if (tEntry.iPhysicsPreviewTotal or 0) > (tEntry.iPhysicsPreviewShapes or 0) then
            tImGui.TextDisabled(string.format(tLang.L('physics_preview_sampled_fmt'),
                tEntry.iPhysicsPreviewShapes or 0, tEntry.iPhysicsPreviewTotal or 0))
        end

        tImGui.Separator()
        tEntry.tPhysicsResetUI = tEntry.tPhysicsResetUI or {primitiveType = 1, triCountRect = 2, triCountCircle = 5}
        local pw = tEntry.tPhysicsResetUI
        local idxSel = tImGui.RadioButton(tLang.L('rectangle') .. '##physReset' .. index, pw.primitiveType, 1)
        idxSel       = tImGui.RadioButton(tLang.L('rectangle_triangle') .. '##physReset' .. index, idxSel, 2)
        if idxSel == 2 then
            tImGui.SameLine()
            tUtil.pushResponsiveItemWidth(70)
            local result, iValue = tImGui.InputInt('##physResetTriRect-' .. index, pw.triCountRect, 2, 2, 0)
            if result and iValue > 1 and iValue < 1000 and iValue % 2 == 0 then
                pw.triCountRect = iValue
            end
            tImGui.PopItemWidth()
        end
        idxSel = tImGui.RadioButton(tLang.L('circle') .. '##physReset' .. index, idxSel, 3)
        idxSel = tImGui.RadioButton(tLang.L('circle_triangle') .. '##physReset' .. index, idxSel, 4)
        if idxSel == 4 then
            tImGui.SameLine()
            tUtil.pushResponsiveItemWidth(70)
            local result, iValue = tImGui.InputInt('##physResetTriCircle-' .. index, pw.triCountCircle, 1, 1, 0)
            if result and iValue > 3 and iValue < 1000 then
                pw.triCountCircle = iValue
            end
            tImGui.PopItemWidth()
        end
        idxSel = tImGui.RadioButton(tLang.L('triangle') .. '##physReset' .. index, idxSel, 5)
        idxSel = tImGui.RadioButton(tLang.L('complex_cube') .. '##physReset' .. index, idxSel, 6)
        pw.primitiveType = idxSel

        if tImGui.Button(tLang.L('reset_physics_to') .. '##physResetBtn-' .. index) then
            local ok, err = resetSingleMeshPhysics(tEntry, pw.primitiveType, pw.triCountRect, pw.triCountCircle)
            if ok then
                if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
                tEntry.bPhysicsVizDirty = true
                tUtil.showMessage(string.format('%s: %s', tLang.L('reset_physics_to'), shortName), 4)
            else
                tUtil.showMessageWarn(err or tLang.L('an_error_occurred'))
            end
        end
        tImGui.SameLine()
        tImGui.HelpMarker(tLang.L('reset_physics_to_tooltip'))

        tImGui.TreePop()
    elseif tEntry.tPhysicsLine then
        destroyPhysicsVisualization(tEntry)
    end

    -- Auto-cancel transform preview when the Transform node is closed
    if tEntry.sOpenNode ~= 'transform' then
        destroyTransformSubsetHoverMarker(tEntry)
        if tEntry.tXformPreviewMesh then
            tEntry.tXformPreviewMesh:destroy()
            tEntry.tXformPreviewMesh = nil
        end
        tEntry.bXformSubsetFilterActive = false
        tEntry.fnBuildXformPreview = nil
        tEntry.tXformSubsetDrag = nil
        tEntry.bXformOrbiting = nil
        if tPreviewMesh and index == iSelectedMeshIndex then tPreviewMesh.visible = true end
    end

    if openNode(tEntry, 'transform', tLang.L("transform"), 0, 'transform-' .. index) then
        -- Shared targeting state for every operation in this node. A selected subset is the
        -- centralization anchor; all subsets in each targeted frame move by the same offset.
        tEntry.tXformUI = tEntry.tXformUI or { frame=0, subset=0, rx=0, ry=0, rz=0, sx=1, sy=1, sz=1, dx=0, dy=0, dz=0, hideOriginal=false, autoPreview=false, previewTint=true, enableSubsetDrag=false, subsetVisibility={} }
        local xf = tEntry.tXformUI
        xf.subset = xf.subset or 0
        xf.targetWidth = xf.targetWidth or 0
        xf.targetHeight = xf.targetHeight or 0
        xf.targetDepth = xf.targetDepth or 0
        xf.dx = xf.dx or 0; xf.dy = xf.dy or 0; xf.dz = xf.dz or 0
        if xf.hideOriginal == nil then xf.hideOriginal = false end
        if xf.autoPreview == nil then xf.autoPreview = false end
        if xf.previewTint == nil then xf.previewTint = true end
        if xf.enableSubsetDrag == nil then xf.enableSubsetDrag = false end
        xf.subsetVisibility = xf.subsetVisibility or {}

        if tImGui.Button(tLang.L("centralize") .. '##' .. index) then
            -- Bones are mesh-wide rather than per-frame. Match the targeted frame when one is
            -- selected; for "all frames", retain the established frame-1 reference behavior.
            local boneReferenceFrame = xf.frame > 0 and xf.frame or 1
            local aabb = computeMeshAABB(meshD, boneReferenceFrame, xf.subset)
            meshD:centralize(xf.frame, xf.subset)
            if aabb then
                local offX, offY, offZ = computeCentralizeOffset(aabb)
                applyTranslateToBones(meshD, -offX, -offY, -offZ)
                rebuildBoneGizmo(tEntry, meshD, index)
            end
            tEntry.modified = true
            tEntry.tTransformBoundsCache = nil
            tEntry.bPhysicsVizDirty = true
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
            tUtil.showMessage(string.format('Centralized: %s', shortName))
        end

        -- Rotate/Scale/Translate with per-frame/per-subset targeting
        local totalFrames = info.totalFrames or 0
        local totalSubsets = 0
        do
            local okS, nS = dpCall(function() return meshD:getTotalSubset(math.max(1, xf.frame)) end)
            if okS and nS then totalSubsets = nS end
        end

        local function cancelXformPreview()
            if tEntry.tXformPreviewMesh then
                tEntry.tXformPreviewMesh:destroy()
                tEntry.tXformPreviewMesh = nil
            end
            tEntry.bXformSubsetFilterActive = false
            if tPreviewMesh and index == iSelectedMeshIndex then tPreviewMesh.visible = true end
        end

        local function getSubsetVisibility(frame, subset)
            local frameVisibility = xf.subsetVisibility[frame]
            if not frameVisibility then
                frameVisibility = {}
                xf.subsetVisibility[frame] = frameVisibility
            end
            if frameVisibility[subset] == nil then frameVisibility[subset] = true end
            return frameVisibility, frameVisibility[subset]
        end

        local function hasHiddenSubset(frame, subsetCount)
            for subset = 1, subsetCount do
                local _, visible = getSubsetVisibility(frame, subset)
                if not visible then return true end
            end
            return false
        end

        tImGui.Separator()
        tImGui.Text(tLang.L("target_frame_label"))
        local _, nf = tImGui.InputInt('##xfFrame-' .. index, xf.frame, 1, 1, 0)
        if nf ~= nil then
            nf = math.max(0, math.min(nf, totalFrames))
            if nf ~= xf.frame then cancelXformPreview() end
            if nf ~= xf.frame then tEntry.tTransformBoundsCache = nil end
            xf.frame = nf
        end
        tImGui.Text(tLang.L("target_subset_label"))
        local _, ns = tImGui.InputInt('##xfSubset-' .. index, xf.subset, 1, 1, 0)
        if ns ~= nil then
            ns = math.max(0, math.min(ns, totalSubsets))
            if ns ~= xf.subset then cancelXformPreview() end
            if ns ~= xf.subset then tEntry.tTransformBoundsCache = nil end
            xf.subset = ns
        end
        if tImGui.Button(tLang.L("centralize_itself") .. '##' .. index) then
            meshD:centralizeItself(xf.frame, xf.subset)
            cancelXformPreview()
            tEntry.modified = true
            tEntry.tTransformBoundsCache = nil
            tEntry.bPhysicsVizDirty = true
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
            tUtil.showMessage(string.format('%s: %s', tLang.L("centralize_itself"), shortName))
        end

        -- Rotation
        tImGui.Spacing()
        tImGui.Text(tLang.L("rotate_xyz"))
        local chg_rx, rx = tImGui.DragFloat('X##xfRx-' .. index, xf.rx, 1.0, 0, 0, '%.1f')
        local chg_ry, ry = tImGui.DragFloat('Y##xfRy-' .. index, xf.ry, 1.0, 0, 0, '%.1f')
        local chg_rz, rz = tImGui.DragFloat('Z##xfRz-' .. index, xf.rz, 1.0, 0, 0, '%.1f')
        if chg_rx then xf.rx = rx end
        if chg_ry then xf.ry = ry end
        if chg_rz then xf.rz = rz end
        if tImGui.Button(tLang.L("apply_rotation") .. '##' .. index) then
            local ok = dpCall(function() meshD:rotateFrame(xf.frame, xf.rx, xf.ry, xf.rz, xf.subset) end)
            if ok then
                -- Bones aren't per-frame/per-subset (one skeleton per mesh), so they always follow
                -- the bake regardless of which frame/subset the vertex bake targeted.
                applyRotationToBonesDeg(meshD, xf.rx, xf.ry, xf.rz)
                rebuildBoneGizmo(tEntry, meshD, index)
                cancelXformPreview()
                onEdit()
                tEntry.tTransformBoundsCache = nil
                tEntry.bPhysicsVizDirty = true
                local target = xf.frame == 0 and 'all frames' or ('frame ' .. xf.frame)
                tUtil.showMessage(string.format(tLang.L("rotation_applied_fmt"), target))
                xf.rx = 0; xf.ry = 0; xf.rz = 0
            end
        end

        -- Scale
        tImGui.Spacing()
        tImGui.Text(tLang.L("scale_xyz"))
        local chg_sx, sx = tImGui.DragFloat('X##xfSx-' .. index, xf.sx, 0.01, 0, 0, '%.3f')
        local chg_sy, sy = tImGui.DragFloat('Y##xfSy-' .. index, xf.sy, 0.01, 0, 0, '%.3f')
        local chg_sz, sz = tImGui.DragFloat('Z##xfSz-' .. index, xf.sz, 0.01, 0, 0, '%.3f')
        if chg_sx then xf.sx = sx end
        if chg_sy then xf.sy = sy end
        if chg_sz then xf.sz = sz end
        if tImGui.Button(tLang.L("apply_scale") .. '##' .. index) then
            local ok = dpCall(function() meshD:scaleFrame(xf.frame, xf.sx, xf.sy, xf.sz, xf.subset) end)
            if ok then
                applyScaleToBones(meshD, xf.sx, xf.sy, xf.sz)
                rebuildBoneGizmo(tEntry, meshD, index)
                cancelXformPreview()
                onEdit()
                tEntry.tTransformBoundsCache = nil
                tEntry.bPhysicsVizDirty = true
                local target = xf.frame == 0 and 'all frames' or ('frame ' .. xf.frame)
                tUtil.showMessage(string.format(tLang.L("scale_applied_fmt"), target))
                xf.sx = 1; xf.sy = 1; xf.sz = 1
            end
        end

        -- Exact-size scaling. X controls width, Y controls height, and Z controls depth. Bounds
        -- are cached for the current target and invalidated only by target/geometry changes.
        local exactBounds = getTransformBounds(tEntry, meshD, xf.frame, xf.subset)
        if exactBounds then
            if xf.targetWidth <= 0 then xf.targetWidth = exactBounds.width end
            if xf.targetHeight <= 0 then xf.targetHeight = exactBounds.height end
            if xf.targetDepth <= 0 then xf.targetDepth = exactBounds.depth end
            tImGui.TextWrapped(string.format(tLang.L('current_bounds_fmt'),
                exactBounds.width, exactBounds.height, exactBounds.depth))

            local function exactSizeRow(axis, field, labelKey, currentSize)
                tImGui.TableNextRow()
                tImGui.TableNextColumn()
                tImGui.Text(tLang.L(labelKey))
                tImGui.TableNextColumn()
                tImGui.SetNextItemWidth(-1)
                local changed, value = tImGui.InputFloat('##xfExact' .. axis .. '-' .. index,
                    xf[field], 1, 10, '%.3f', 0)
                if changed then xf[field] = value end
                tImGui.TableNextColumn()
                tImGui.BeginDisabled(xf[field] <= 0 or currentSize <= 1e-7)
                if tImGui.Button(tLang.L('apply_btn') .. '##xfExactApply' .. axis .. '-' .. index) then
                    local sxExact, syExact, szExact = computeExactAxisScale(currentSize, xf[field], axis)
                    local ok = dpCall(function() meshD:scaleFrame(xf.frame, sxExact, syExact, szExact, xf.subset) end)
                    if ok then
                        applyScaleToBones(meshD, sxExact, syExact, szExact)
                        rebuildBoneGizmo(tEntry, meshD, index)
                        cancelXformPreview()
                        onEdit()
                        tEntry.tTransformBoundsCache = nil
                        tEntry.bPhysicsVizDirty = true
                        tUtil.showMessage(string.format(tLang.L('exact_size_applied_fmt'), axis, xf[field]))
                    end
                end
                tImGui.EndDisabled()
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(string.format(tLang.L('scale_axis_to'), axis))
                    tImGui.EndTooltip()
                end
            end

            local exactFlags = tImGui.Flags('ImGuiTableFlags_SizingStretchProp')
            if tImGui.BeginTable('xfExactSize-' .. index, 3, exactFlags) then
                tImGui.TableSetupColumn('##xfExactLabel', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 82)
                tImGui.TableSetupColumn('##xfExactValue', tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'))
                tImGui.TableSetupColumn('##xfExactButton', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 58)
                exactSizeRow('X', 'targetWidth', 'target_width', exactBounds.width)
                exactSizeRow('Y', 'targetHeight', 'target_height', exactBounds.height)
                exactSizeRow('Z', 'targetDepth', 'target_depth', exactBounds.depth)
                tImGui.EndTable()
            end
        else
            tImGui.TextDisabled(tLang.L('bounds_unavailable'))
        end

        -- Translate
        tImGui.Spacing()
        tImGui.Text(tLang.L("translate_xyz"))
        local chg_dx, dx = tImGui.DragFloat('X##xfDx-' .. index, xf.dx, 1.0, 0, 0, '%.1f')
        local chg_dy, dy = tImGui.DragFloat('Y##xfDy-' .. index, xf.dy, 1.0, 0, 0, '%.1f')
        local chg_dz, dz = tImGui.DragFloat('Z##xfDz-' .. index, xf.dz, 1.0, 0, 0, '%.1f')
        if chg_dx then xf.dx = dx end
        if chg_dy then xf.dy = dy end
        if chg_dz then xf.dz = dz end
        if tImGui.Button(tLang.L("apply_translate") .. '##' .. index) then
            local ok = dpCall(function() meshD:translateFrame(xf.frame, xf.dx, xf.dy, xf.dz, xf.subset) end)
            if ok then
                applyTranslateToBones(meshD, xf.dx, xf.dy, xf.dz)
                rebuildBoneGizmo(tEntry, meshD, index)
                cancelXformPreview()
                onEdit()
                tEntry.tTransformBoundsCache = nil
                tEntry.bPhysicsVizDirty = true
                local target = xf.frame == 0 and 'all frames' or ('frame ' .. xf.frame)
                tUtil.showMessage(string.format(tLang.L("translate_applied_fmt"), target))
                xf.dx = 0; xf.dy = 0; xf.dz = 0
            end
        end

        -- Combined Preview → Apply workflow
        tImGui.Separator()
        local newHide = tImGui.Checkbox(tLang.L("hide_original_mesh") .. '##xfHide-' .. index, xf.hideOriginal)
        if newHide ~= xf.hideOriginal then
            xf.hideOriginal = newHide
            if tEntry.tXformPreviewMesh and tPreviewMesh and index == iSelectedMeshIndex then
                tPreviewMesh.visible = not (newHide or tEntry.bXformSubsetFilterActive)
            end
        end
        xf.autoPreview = tImGui.Checkbox(tLang.L("auto_preview_transform") .. '##xfAuto-' .. index, xf.autoPreview)
        local oldPreviewTint = xf.previewTint
        xf.previewTint = tImGui.Checkbox(tLang.L('preview_yellow_tint') .. '##xfTint-' .. index, xf.previewTint)
        local previewTintChanged = oldPreviewTint ~= xf.previewTint

        local visibilityBlocksDrag = xf.frame > 0 and hasHiddenSubset(xf.frame, totalSubsets)
        local canDragSubset = xf.frame > 0 and xf.subset > 0 and not visibilityBlocksDrag
        local oldEnableSubsetDrag = xf.enableSubsetDrag
        tImGui.BeginDisabled(not canDragSubset)
        xf.enableSubsetDrag = tImGui.Checkbox(tLang.L('drag_target_subset') .. '##xfDrag-' .. index, xf.enableSubsetDrag)
        tImGui.EndDisabled()
        local iCountNewLine = 0
        if visibilityBlocksDrag then
            xf.enableSubsetDrag = false
            tImGui.TextDisabled(tLang.L('drag_target_subset_visibility_help'))
            iCountNewLine = 0
        elseif not canDragSubset then
            xf.enableSubsetDrag = false
            tImGui.TextDisabled(tLang.L('drag_target_subset_select_help'))
        elseif xf.enableSubsetDrag then
            tImGui.TextWrapped(tLang.L('drag_target_subset_help'))
        else
            iCountNewLine = 2
        end
        local enableSubsetDragChanged = oldEnableSubsetDrag ~= xf.enableSubsetDrag
        -- anoying the check box been repositioned, so add some new line to make the layout looks better
        for i = 1, iCountNewLine do tImGui.NewLine() end

        -- Shared helper: build/rebuild the preview clone
        local function buildXformPreview()
            cancelXformPreview()
            local ext = tEntry.fileName:match('%.([^%.]+)$') or 'msh'
            tEntry.xfPreviewPath = tEntry.xfPreviewPath or (os.tmpname() .. '_xf.' .. ext)
            if meshD:save(tEntry.xfPreviewPath, false, false) then
                local cloneMeshD = meshDebug:new()
                if cloneMeshD:load(tEntry.xfPreviewPath) then
                    if xf.rx ~= 0 or xf.ry ~= 0 or xf.rz ~= 0 then
                        dpCall(function() cloneMeshD:rotateFrame(xf.frame, xf.rx, xf.ry, xf.rz, xf.subset) end)
                    end
                    if xf.sx ~= 1 or xf.sy ~= 1 or xf.sz ~= 1 then
                        dpCall(function() cloneMeshD:scaleFrame(xf.frame, xf.sx, xf.sy, xf.sz, xf.subset) end)
                    end
                    if xf.dx ~= 0 or xf.dy ~= 0 or xf.dz ~= 0 then
                        dpCall(function() cloneMeshD:translateFrame(xf.frame, xf.dx, xf.dy, xf.dz, xf.subset) end)
                    end
                    -- Drag mode keeps the temporary render clone down to the one target subset.
                    -- onTouchMove can then translate that render object directly instead of
                    -- rewriting, saving, reloading, and uploading every vertex on every event.
                    local hiddenSubset = xf.frame > 0 and hasHiddenSubset(xf.frame, totalSubsets)
                    local isolateDragSubset = xf.enableSubsetDrag and xf.frame > 0 and xf.subset > 0 and not hiddenSubset
                    local visibleSubsetCount = totalSubsets
                    if isolateDragSubset then
                        for subset = totalSubsets, 1, -1 do
                            if subset ~= xf.subset then
                                visibleSubsetCount = visibleSubsetCount - 1
                                dpCall(function() cloneMeshD:removeSubset(xf.frame, subset) end)
                            end
                        end
                    elseif hiddenSubset then
                        for subset = totalSubsets, 1, -1 do
                            local _, visible = getSubsetVisibility(xf.frame, subset)
                            if not visible then
                                visibleSubsetCount = visibleSubsetCount - 1
                                -- Keep an all-hidden clone structurally valid; the render object is
                                -- hidden below instead of trying to save a frame with zero subsets.
                                if visibleSubsetCount > 0 then
                                    dpCall(function() cloneMeshD:removeSubset(xf.frame, subset) end)
                                end
                            end
                        end
                    end
                    if cloneMeshD:save(tEntry.xfPreviewPath, false, false) then
                        meshDebug:fakeRelease(tEntry.xfPreviewPath)
                        local dir = tEntry.fileName:match('^(.*)[/\\]')
                        if dir then mbm.addPath(dir) end
                        local coordType = bCameraMode3D and '3d' or '2dw'
                        local meshType = info.type or 'unknown'
                        local cloneRender, rok = nil, false
                        if meshType == 'sprite' then
                            cloneRender = sprite:new(coordType); rok = cloneRender:load(tEntry.xfPreviewPath)
                        elseif meshType == 'mesh' then
                            cloneRender = mesh:new(coordType); rok = cloneRender:load(tEntry.xfPreviewPath)
                        elseif meshType == 'tile' then
                            cloneRender = tile:new(coordType); rok = cloneRender:load(tEntry.xfPreviewPath)
                        elseif meshType == 'particle' then
                            cloneRender = particle:new(coordType)
                            rok = cloneRender:load(tEntry.xfPreviewPath)
                            if rok then cloneRender:add(100); cloneRender.revive = true end
                        elseif meshType == 'texture' then
                            cloneRender = texture:new(coordType); rok = cloneRender:load(tEntry.xfPreviewPath)
                        end
                        if rok and cloneRender then
                            -- Match updatePreviewMesh's origin reset. A newly-created renderizable
                            -- at z=0 receives DEVICE's automatic render-order Z nudge; leaving it
                            -- in place makes the transform clone look like a displaced duplicate
                            -- of the original, especially when the yellow preview tint is off.
                            cloneRender:setPos(0, 0, 0)
                            -- obj:setColor takes 0.0-1.0 per channel, not 0-255 (docs/lua-api.md:539)
                            -- -- values above 1 clamp, so the previous (255,220,50,200) call silently
                            -- rendered opaque white instead of the intended yellow/gold tint.
                            if xf.previewTint then
                                cloneRender:setColor(1.0, 220/255, 50/255, 200/255)
                            end
                            dpCall(function() cloneRender:setAnim(tEntry.iSelectedAnim or 1) end)
                            cloneRender.visible = visibleSubsetCount > 0
                            tEntry.tXformPreviewMesh = cloneRender
                            tEntry.bXformSubsetFilterActive = hiddenSubset
                            if tPreviewMesh and index == iSelectedMeshIndex then
                                tPreviewMesh.visible = not (xf.hideOriginal or hiddenSubset)
                            end
                        else
                            if cloneRender then cloneRender:destroy() end
                            tUtil.showMessageWarn('Failed to create transform preview')
                        end
                    end
                end
            end
        end
        tEntry.fnBuildXformPreview = buildXformPreview
        if (previewTintChanged or enableSubsetDragChanged) and tEntry.tXformPreviewMesh then buildXformPreview() end

        -- Auto-preview: rebuild only when values change (fingerprint)
        if xf.autoPreview then
            local fp = string.format('%d|%d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f',
                xf.frame, xf.subset, xf.rx, xf.ry, xf.rz, xf.sx, xf.sy, xf.sz, xf.dx, xf.dy, xf.dz)
            if fp ~= tEntry.xfLastPreviewFP then
                tEntry.xfLastPreviewFP = fp
                buildXformPreview()
            end
        else
            tEntry.xfLastPreviewFP = nil
        end

        if not tEntry.tXformPreviewMesh then
            if not xf.autoPreview then
                if tImGui.Button(tLang.L("preview_transform") .. '##' .. index) then
                    buildXformPreview()
                end
            end
        else
            tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0.5,g=0.1,b=0.1,a=1})
            if tImGui.Button(tLang.L("cancel_preview") .. '##' .. index) then
                cancelXformPreview()
                tEntry.xfLastPreviewFP = nil
                xf.autoPreview = false
            end
            tImGui.PopStyleColor(1)
            tImGui.SameLine()
            tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0.1,g=0.5,b=0.1,a=1})
            if tImGui.Button(tLang.L("apply_transform") .. '##' .. index) then
                local anyChange = false
                if xf.rx ~= 0 or xf.ry ~= 0 or xf.rz ~= 0 then
                    if dpCall(function() meshD:rotateFrame(xf.frame, xf.rx, xf.ry, xf.rz, xf.subset) end) then anyChange = true end
                end
                if xf.sx ~= 1 or xf.sy ~= 1 or xf.sz ~= 1 then
                    if dpCall(function() meshD:scaleFrame(xf.frame, xf.sx, xf.sy, xf.sz, xf.subset) end) then anyChange = true end
                end
                if xf.dx ~= 0 or xf.dy ~= 0 or xf.dz ~= 0 then
                    if dpCall(function() meshD:translateFrame(xf.frame, xf.dx, xf.dy, xf.dz, xf.subset) end) then anyChange = true end
                end
                cancelXformPreview()
                tEntry.xfLastPreviewFP = nil
                if anyChange then
                    local target = xf.frame == 0 and 'all frames' or ('frame ' .. xf.frame)
                    tUtil.showMessage(string.format(tLang.L("transform_applied_fmt"), target))
                    xf.rx = 0; xf.ry = 0; xf.rz = 0
                    xf.sx = 1; xf.sy = 1; xf.sz = 1
                    xf.dx = 0; xf.dy = 0; xf.dz = 0
                    onEdit()
                    tEntry.tTransformBoundsCache = nil
                    tEntry.bPhysicsVizDirty = true
                end
            end
            tImGui.PopStyleColor(1)
        end

        -- Subset visibility is a preview-only filter. It is available for one concrete frame;
        -- frame 0 targets all frames and may have a different subset layout in each frame.
        if xf.frame > 0 then
            tImGui.Spacing()
            tImGui.Text(tLang.L('preview_subset_visibility'))
            local hoveredSubset = nil
            local tableFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg')
            if tImGui.BeginTable('xfSubsetVisibility-' .. index, 2, tableFlags) then
                tImGui.TableSetupColumn(tLang.L('subset'))
                tImGui.TableSetupColumn(tLang.L('select'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 90)
                tImGui.TableHeadersRow()
                for subset = 1, totalSubsets do
                    local frameVisibility, visible = getSubsetVisibility(xf.frame, subset)
                    tImGui.TableNextRow()
                    tImGui.TableSetColumnIndex(0)
                    local label = string.format('%s %d##xfSubsetVisible-%d-%d-%d',
                        tLang.L('subset'), subset, index, xf.frame, subset)
                    local newVisible = tImGui.Checkbox(label, visible)
                    if tImGui.IsItemHovered(0) then hoveredSubset = subset end
                    if newVisible ~= visible then
                        frameVisibility[subset] = newVisible
                        if tEntry.tXformPreviewMesh then
                            buildXformPreview()
                        elseif xf.autoPreview then
                            tEntry.xfLastPreviewFP = nil
                        end
                    end
                    tImGui.TableSetColumnIndex(1)
                    local selectPressed = tImGui.Button(tLang.L('select') .. '##xfSubsetSelect-' .. index .. '-' .. xf.frame .. '-' .. subset)
                    if tImGui.IsItemHovered(0) then hoveredSubset = subset end
                    if selectPressed then
                        if xf.subset ~= subset then
                            cancelXformPreview()
                            xf.subset = subset
                            tEntry.xfLastPreviewFP = nil
                        end
                    end
                end
                tImGui.EndTable()
            end
            if hoveredSubset then
                updateTransformSubsetHoverMarker(tEntry, meshD, xf.frame, hoveredSubset, xf, index, 'transform')
            else
                destroyTransformSubsetHoverMarker(tEntry, 'transform')
            end
        else
            destroyTransformSubsetHoverMarker(tEntry, 'transform')
        end

        tImGui.TreePop()
    end

    -- ── Texture node ──────────────────────────────────────────────────────────
    if openNode(tEntry, 'texture', tLang.L("texture_node"), 0, 'texture-' .. index) then
        tEntry.tTexUI = tEntry.tTexUI or { frame=0, subset=0, stage=0, role='primary', filename='' }
        local tx      = tEntry.tTexUI
        tx.filename   = tx.filename or ''
        tx.role       = tx.role or 'primary'

        local totalFrames  = info.totalFrames or 0
        local totalSubsets = 0
        do
            local okS, nS = dpCall(function() return meshD:getTotalSubset(math.max(1, tx.frame)) end)
            if okS and nS then totalSubsets = nS end
        end

        -- Stage selector first: frame/subset/role only make sense for stage 0 (FX is one shared
        -- texture per animation, not addressable by frame/subset - see the Animations node note).
        tImGui.Separator()
        tImGui.Text(tLang.L("tex_stage_label"))
        local stageOpts = {'0 - Primary', '1 - FX (per anim step)'}
        local stageRet, newStageIdx = tImGui.Combo('##txStage-' .. index, tx.stage + 1, stageOpts, -1)
        if stageRet and newStageIdx then tx.stage = newStageIdx - 1 end

        tImGui.Spacing()
        if tx.stage == 1 then
            tImGui.TextWrapped(tLang.L("tex_stage1_note"))
            local nAnimTx = info.animation or 0
            if nAnimTx > 0 then
                local tAnimNamesTx = {}
                for a = 1, nAnimTx do
                    local okN, aName = dpCall(function() return meshD:getAnim(a) end)
                    tAnimNamesTx[a] = (okN and aName and aName ~= '') and aName or ('Anim ' .. a)
                end
                tx.fxAnimIndex = math.max(1, math.min(tx.fxAnimIndex or 1, nAnimTx))
                local changedFxAnim, newFxAnimIdx = tImGui.Combo(tLang.L("animation") .. '##txFxAnim-' .. index, tx.fxAnimIndex, tAnimNamesTx, -1)
                if changedFxAnim and newFxAnimIdx then tx.fxAnimIndex = newFxAnimIdx end

                local okFxTx, curFxTexTx = dpCall(function() return meshD:getFxTexture(tx.fxAnimIndex) end)
                tImGui.Text(tLang.L("tex_current_label"))
                tImGui.SameLine()
                if okFxTx and curFxTexTx and curFxTexTx ~= '' then
                    tImGui.TextDisabled(tUtil.getShortName(curFxTexTx))
                else
                    tImGui.TextDisabled('(none)')
                end

                tx.fxFilename = tx.fxFilename or ''
                local modFxTx, newFxTx = tImGui.InputText('##txFxFile-' .. index, tx.fxFilename, 512, 0)
                if modFxTx and newFxTx ~= nil then tx.fxFilename = newFxTx end
                tImGui.SameLine()
                if tImGui.Button(tLang.L("tex_browse") .. '##txFxBrowse-' .. index) then
                    local picked = mbm.openFile(sLastMeshPath,
                        table.unpack(tUtil.supported_images or {'png', 'jpg', 'bmp', 'tga'}))
                    if picked then
                        if type(picked) == 'table' then picked = picked[1] end
                        tx.fxFilename = picked
                    end
                end
                if tImGui.Button(tLang.L("tex_set") .. '##txFxSet-' .. index) then
                    if tx.fxFilename == '' then
                        tUtil.showMessageWarn('No filename specified')
                    else
                        local okSetTx = dpCall(function() return meshD:setFxTexture(tx.fxAnimIndex, tx.fxFilename) end)
                        if okSetTx then
                            onEdit()
                            tUtil.showMessage(string.format(tLang.L('tex_set_ok_fmt'), 'anim ' .. tx.fxAnimIndex, tUtil.getShortName(tx.fxFilename)))
                        end
                    end
                end
                tImGui.SameLine()
                if tImGui.Button(tLang.L("tex_clear") .. '##txFxClear-' .. index) then
                    local okClrTx = dpCall(function() return meshD:setFxTexture(tx.fxAnimIndex, '') end)
                    if okClrTx then
                        tx.fxFilename = ''
                        onEdit()
                        tUtil.showMessage(string.format(tLang.L('tex_clear_ok_fmt'), 'anim ' .. tx.fxAnimIndex))
                    end
                end
            else
                tImGui.TextDisabled('No animations')
            end
        else
            -- Frame / subset selectors (0 = all, consistent with Transform node)
            tImGui.Text(tLang.L("target_frame_label"))
            local _, nf = tImGui.InputInt('##txFrame-' .. index, tx.frame, 1, 1, 0)
            if nf ~= nil then tx.frame = math.max(0, math.min(nf, totalFrames)) end

            tImGui.Text(tLang.L("target_subset_label"))
            local _, ns = tImGui.InputInt('##txSubset-' .. index, tx.subset, 1, 1, 0)
            if ns ~= nil then tx.subset = math.max(0, math.min(ns, totalSubsets)) end
            tImGui.Spacing()
            local roleOpts = {
                tLang.L("tex_role_primary"),
                tLang.L("tex_role_normal"),
                tLang.L("tex_role_specular"),
                tLang.L("tex_role_emissive"),
                tLang.L("tex_role_mask"),
            }
            local roleValues = {'primary', 'normal', 'specular', 'emissive', 'mask'}
            local roleIndex = 1
            for i = 1, #roleValues do
                if roleValues[i] == tx.role then
                    roleIndex = i
                    break
                end
            end
            local specificTextureTarget = tx.frame > 0 and tx.subset > 0
            local roleTableFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_RowBg')
            if tImGui.BeginTable('txRoleTable-' .. index, 3, roleTableFlags) then
                tImGui.TableSetupColumn(tLang.L('select'),
                    tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 55)
                tImGui.TableSetupColumn(tLang.L('tex_role_label'),
                    tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 105)
                tImGui.TableSetupColumn(tLang.L('tex_texture_name'),
                    tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'))
                tImGui.TableHeadersRow()
                for roleRow = 1, #roleValues do
                        local roleValue = roleValues[roleRow]
                        local okTexture, roleTexture
                        if specificTextureTarget and roleValue == 'primary' then
                            okTexture, roleTexture = dpCall(function()
                                return meshD:getTexture(tx.frame, tx.subset)
                            end)
                        elseif specificTextureTarget then
                            okTexture, roleTexture = dpCall(function()
                                return meshD:getMaterialTexture(tx.frame, tx.subset, roleValue)
                            end)
                        end

                        tImGui.TableNextRow()
                        tImGui.TableSetColumnIndex(0)
                        roleIndex = tImGui.RadioButton(
                            '##txRoleSelect-' .. index .. '-' .. roleRow, roleIndex, roleRow)
                        tImGui.TableSetColumnIndex(1)
                        tImGui.Text(roleOpts[roleRow])
                        tImGui.TableSetColumnIndex(2)
                        if not specificTextureTarget then
                            tImGui.Text('')
                        elseif okTexture and roleTexture and roleTexture ~= '' then
                            tImGui.TextDisabled(tUtil.getShortName(roleTexture))
                            if tImGui.IsItemHovered(0) then
                                tImGui.BeginTooltip()
                                tImGui.Text(roleTexture)
                                tImGui.EndTooltip()
                            end
                        else
                            tImGui.TextDisabled('(none)')
                        end
                end
                tImGui.EndTable()
                tx.role = roleValues[roleIndex] or 'primary'
            end

            -- Filename input + browse button
            tImGui.Text(tLang.L("tex_filename_label"))
            local modF, newFile = tImGui.InputText('##txFile-' .. index, tx.filename, 512, 0)
            if modF and newFile ~= nil then tx.filename = newFile end
            tImGui.SameLine()
            if tImGui.Button(tLang.L("tex_browse") .. '##txBrowse-' .. index) then
                local picked = mbm.openFile(sLastMeshPath,
                    table.unpack(tUtil.supported_images or {'png', 'jpg', 'bmp', 'tga'}))
                if picked then
                    if type(picked) == 'table' then picked = picked[1] end
                    tx.filename = picked
                end
            end

            -- Set / Clear buttons
            tImGui.Spacing()
            if tImGui.Button(tLang.L("tex_set") .. '##txSet-' .. index) then
                local fn = tx.filename or ''
                if fn == '' then
                    tUtil.showMessageWarn('No filename specified')
                else
                    local count = 0
                    local f1, f2 = tx.frame == 0 and 1 or tx.frame, tx.frame == 0 and totalFrames or tx.frame
                    for f = f1, f2 do
                        local nS2 = totalSubsets
                        if tx.subset == 0 then
                            local okS2, ns2 = dpCall(function() return meshD:getTotalSubset(f) end)
                            if okS2 and ns2 then nS2 = ns2 end
                        end
                        local s1 = tx.subset == 0 and 1 or tx.subset
                        local s2 = tx.subset == 0 and nS2 or tx.subset
                        for s = s1, s2 do
                            local ok
                            if tx.role == 'primary' then
                                ok = dpCall(function() meshD:setTexture(f, s, fn) end)
                            else
                                ok = dpCall(function() meshD:setMaterialTexture(f, s, tx.role, fn) end)
                            end
                            if ok then count = count + 1 end
                        end
                    end
                    if count > 0 then
                        onEdit()
                        local target = (tx.frame == 0 and 'all frames' or ('frame ' .. tx.frame))
                                    .. ' / ' .. (tx.subset == 0 and 'all subsets' or ('subset ' .. tx.subset))
                        tUtil.showMessage(string.format(tLang.L("tex_set_ok_fmt"), target, tUtil.getShortName(fn)))
                    end
                end
            end
            tImGui.SameLine()
            if tImGui.Button(tLang.L("tex_clear") .. '##txClear-' .. index) then
                local count = 0
                local f1, f2 = tx.frame == 0 and 1 or tx.frame, tx.frame == 0 and totalFrames or tx.frame
                for f = f1, f2 do
                    local nS2 = totalSubsets
                    if tx.subset == 0 then
                        local okS2, ns2 = dpCall(function() return meshD:getTotalSubset(f) end)
                        if okS2 and ns2 then nS2 = ns2 end
                    end
                    local s1 = tx.subset == 0 and 1 or tx.subset
                    local s2 = tx.subset == 0 and nS2 or tx.subset
                    for s = s1, s2 do
                        local ok
                        if tx.role == 'primary' then
                            ok = dpCall(function() meshD:setTexture(f, s, '') end)
                        else
                            ok = dpCall(function() meshD:setMaterialTexture(f, s, tx.role, '') end)
                        end
                        if ok then count = count + 1 end
                    end
                end
                if count > 0 then
                    onEdit()
                    local target = (tx.frame == 0 and 'all frames' or ('frame ' .. tx.frame))
                               .. ' / ' .. (tx.subset == 0 and 'all subsets' or ('subset ' .. tx.subset))
                    tUtil.showMessage(string.format(tLang.L("tex_clear_ok_fmt"), target))
                end
            end
        end

        tImGui.TreePop()
    end

    if openNode(tEntry, 'uv', tLang.L("uv_label"), 0, 'uv-' .. index) then
        if info and info.hasTexture then
            tImGui.TextDisabled('Has UV')
        else
            tImGui.TextDisabled('No UV')
        end

        tEntry.tUvUI = tEntry.tUvUI or { frame = 0 }
        local uv = tEntry.tUvUI
        local totalFrames = info.totalFrames or 0

        tImGui.Separator()
        tImGui.Text(tLang.L("target_frame_label"))
        local _, nf = tImGui.InputInt('##uvFrame-' .. index, uv.frame, 1, 1, 0)
        if nf ~= nil then
            nf = math.max(0, math.min(nf, totalFrames))
            uv.frame = nf
        end

        tImGui.Spacing()
        if tImGui.Button(tLang.L("invert_u") .. '##' .. index) then
            if not (info and info.hasTexture) then
                tUtil.showMessage(string.format(tLang.L("uv_no_data_warning"), shortName))
            else
                local n = invertMeshUV(meshD, uv.frame, true, false)
                if n > 0 then
                    onEdit()
                    local target = uv.frame == 0 and 'all frames' or ('frame ' .. uv.frame)
                    tUtil.showMessage(string.format(tLang.L("uv_invert_applied_fmt"), 'U ' .. target, shortName))
                end
            end
        end
        if tImGui.Button(tLang.L("invert_v") .. '##' .. index) then
            if not (info and info.hasTexture) then
                tUtil.showMessage(string.format(tLang.L("uv_no_data_warning"), shortName))
            else
                local n = invertMeshUV(meshD, uv.frame, false, true)
                if n > 0 then
                    onEdit()
                    local target = uv.frame == 0 and 'all frames' or ('frame ' .. uv.frame)
                    tUtil.showMessage(string.format(tLang.L("uv_invert_applied_fmt"), 'V ' .. target, shortName))
                end
            end
        end
        if tImGui.Button(tLang.L("invert_uv") .. '##' .. index) then
            if not (info and info.hasTexture) then
                tUtil.showMessage(string.format(tLang.L("uv_no_data_warning"), shortName))
            else
                local n = invertMeshUV(meshD, uv.frame, true, true)
                if n > 0 then
                    onEdit()
                    local target = uv.frame == 0 and 'all frames' or ('frame ' .. uv.frame)
                    tUtil.showMessage(string.format(tLang.L("uv_invert_applied_fmt"), 'UV ' .. target, shortName))
                end
            end
        end
        if tImGui.Button(tLang.L("fix_legacy_v") .. '##' .. index) then
            if not (info and info.hasTexture) then
                tUtil.showMessage(string.format(tLang.L("uv_no_data_warning"), shortName))
            else
                local fixType, n = normalizeLegacyV(meshD, uv.frame)
                if fixType == false then
                    tUtil.showMessageWarn('Failed to scan UV data: ' .. shortName)
                elseif fixType == 'none' then
                    tUtil.showMessage(string.format(tLang.L("fix_legacy_v_none_fmt"), shortName))
                else
                    onEdit()
                    local target = uv.frame == 0 and 'all frames' or ('frame ' .. uv.frame)
                    tUtil.showMessage(string.format(tLang.L("fix_legacy_v_ok_fmt"), fixType, n, target, shortName))
                end
            end
        end
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.Text(tLang.L("fix_legacy_v_tooltip"))
            tImGui.EndTooltip()
        end

        tImGui.TreePop()
    end

    local nAnim = info.animation or 0
    if openNode(tEntry, 'anims', tLang.L("frame_animation_node") .. (nAnim and nAnim > 0 and (' (' .. nAnim .. ')') or ''), 0, 'anims-' .. index) then
        -- Frame-filter preview refresh controls (only for the currently selected mesh)
        if index == iSelectedMeshIndex then
            if tPreviewMesh then
                local okFr, iFrame = dpCall(function() return tPreviewMesh:getIndexFrame() end)
                if okFr and iFrame then
                    local okTotFr, iTotFr = dpCall(function() return tPreviewMesh:getTotalFrame() end)
                    tImGui.Text(string.format(tLang.L('current_frame_fmt'), iFrame, (okTotFr and iTotFr) or 0))
                end
            end
            if tEntry.modified then
                tImGui.BeginDisabled(true)
                tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Text'), {r=1,g=1,b=0,a=1})
                tImGui.Button(tLang.L("save_first_to_preview") .. '##rfpBtn-' .. index)
                tImGui.PopStyleColor(1)
                tImGui.EndDisabled()
            elseif not tEntry.bAutoRefreshPreview then
                if tImGui.Button('Refresh Preview##rfpBtn-' .. index) then
                    refreshFrameFilterPreview(tEntry, index)
                    tEntry.bFrameSelectionDirty = false
                end
                tImGui.SameLine()
            end
            local autoVal = tImGui.Checkbox(tLang.L('auto_refresh') .. '##autoRefresh-' .. index, tEntry.bAutoRefreshPreview and not tEntry.modified)
            if not tEntry.modified then
                tEntry.bAutoRefreshPreview = autoVal
            end
            tImGui.Separator()
        end

        -- Frame selection compact+expand table
        showAnimFrameSelectionTable(tEntry, meshD, index)

        if index == iSelectedMeshIndex and tPreviewMesh then
            -- Build animation name list; mark filter-removed animations with [!]
            if nAnim > 0 then
                local animMap, nRemoved = computeAnimFilterMap(tEntry, meshD, nAnim)
                local tAnimNames = {}
                for i = 1, nAnim do
                    local ok, aName = dpCall(function() return meshD:getAnim(i) end)
                    local name = (ok and aName and aName ~= '') and aName or ('Anim ' .. i)
                    tAnimNames[i] = animMap[i] == false and ('[!] ' .. name) or name
                end
                tEntry.iSelectedAnim = tEntry.iSelectedAnim or 1
                tImGui.PushItemWidth(180)
                local changed, newIdx = tImGui.Combo(tLang.L("animation") .. '##animSel-' .. index, tEntry.iSelectedAnim, tAnimNames, -1)
                tImGui.PopItemWidth()
                if changed and newIdx and newIdx >= 1 and newIdx <= nAnim then
                    tEntry.iSelectedAnim = newIdx
                    local filteredIdx = animMap[newIdx]
                    if filteredIdx then
                        -- Use remapped index when preview is filtered, original otherwise
                        local targetIdx = tEntry.bPreviewIsFiltered and filteredIdx or newIdx
                        dpCall(function() tPreviewMesh:setAnim(targetIdx) end)
                    end
                    -- else: [!] animation removed by filter, skip setAnim silently
                end
                if nRemoved > 0 then
                    tImGui.TextDisabled(string.format(tLang.L('anim_removed_by_filter_fmt'), nRemoved))
                end
            end
            if tImGui.Button(tLang.L("restart_animation") .. '##' .. index) then
                dpCall(function() tPreviewMesh:restartAnim() end)
            end
        end
        if nAnim and nAnim > 0 then
            local bAnimRemoved = false
            for i = 1, nAnim do
                if bAnimRemoved then break end
                local ok, name, initF, finF, time, typ = dpCall(function()
                    return meshD:getAnim(i)
                end)
                if ok and name then
                    if tImGui.TreeNodeEx(name or ('Anim ' .. i), 0, 'anim-' .. index .. '-' .. i) then
                        tImGui.Text(tLang.L("name"))
                        tImGui.PushItemWidth(220)
                        local mod, newName = tImGui.InputText('##animName-' .. index .. '-' .. i, name or '', flags)
                        tImGui.PopItemWidth()
                        tImGui.Text(tLang.L("initial_frame"))
                        tImGui.PushItemWidth(70)
                        local ri, ni = tImGui.InputInt('##animInit-' .. index .. '-' .. i, initF or 1, 1, 1, flags)
                        tImGui.PopItemWidth()
                        tImGui.Text(tLang.L("final_frame"))
                        tImGui.PushItemWidth(70)
                        local rf, nf = tImGui.InputInt('##animFin-' .. index .. '-' .. i, finF or 1, 1, 1, flags)
                        tImGui.PopItemWidth()
                        tImGui.Text(tLang.L("time_between_frames_anim"))
                        tImGui.PushItemWidth(105)
                        local rt, nt = tImGui.InputFloat('##animTime-' .. index .. '-' .. i, time or 0.1, 0.01, 0.1, '%.3f', flags)
                        tImGui.PopItemWidth()
                        tImGui.Text(tLang.L("type_label"))
                        local typIdx = math.max(1, math.min((typ or 0) + 1, #tAnimTypeOpts))
                        tImGui.PushItemWidth(180)
                        local rty, newTypIdx = tImGui.Combo('##animType-' .. index .. '-' .. i, typIdx, tAnimTypeOpts, -1)
                        tImGui.PopItemWidth()
                        local nty = (rty and newTypIdx and newTypIdx > 0) and (newTypIdx - 1) or (typ or 0)
                        if (mod or ri or rf or rt or rty) then
                            local totalFrames = info.totalFrames or 0
                            local okTotal, nF = dpCall(function() return meshD:getTotalFrame() end)
                            if okTotal and nF then totalFrames = nF end
                            local initVal = math.max(1, math.min(ni or initF or 1, totalFrames > 0 and totalFrames or 1))
                            local finVal  = math.max(1, math.min(nf or finF or 1, totalFrames > 0 and totalFrames or 1))
                            local timeVal = (nt or time or 0.1) > 0 and (nt or time or 0.1) or 0.1
                            local typeVal = math.max(0, math.min(nty, 6))
                            if totalFrames > 0 then
                                local okUp = dpCall(function()
                                    meshD:updateAnim(i, newName or name, initVal, finVal, timeVal, typeVal)
                                end)
                                if okUp then
                                    tEntry.iSelectedAnim = i
                                    onEdit()
                                end
                            end
                        end
                        tImGui.Separator()
                        tImGui.Text(tLang.L('fx_texture_label'))
                        local okFx, curFxTex = dpCall(function() return meshD:getFxTexture(i) end)
                        tImGui.SameLine()
                        if okFx and curFxTex and curFxTex ~= '' then
                            tImGui.TextDisabled(tUtil.getShortName(curFxTex))
                            if tImGui.IsItemHovered(0) then
                                tImGui.BeginTooltip()
                                tImGui.Text(curFxTex)
                                tImGui.EndTooltip()
                            end
                        else
                            tImGui.TextDisabled('(none)')
                        end
                        tEntry.tFxFilenames = tEntry.tFxFilenames or {}
                        local fxFn = tEntry.tFxFilenames[i] or ''
                        tImGui.PushItemWidth(220)
                        local modFxF, newFxF = tImGui.InputText('##animFxFile-' .. index .. '-' .. i, fxFn, 512, 0)
                        tImGui.PopItemWidth()
                        if modFxF and newFxF ~= nil then tEntry.tFxFilenames[i] = newFxF end
                        tImGui.SameLine()
                        if tImGui.Button(tLang.L('tex_browse') .. '##animFxBrowse-' .. index .. '-' .. i) then
                            local picked = mbm.openFile(sLastMeshPath,
                                table.unpack(tUtil.supported_images or {'png', 'jpg', 'bmp', 'tga'}))
                            if picked then
                                if type(picked) == 'table' then picked = picked[1] end
                                tEntry.tFxFilenames[i] = picked
                            end
                        end
                        if tImGui.Button(tLang.L('tex_set') .. '##animFxSet-' .. index .. '-' .. i) then
                            local fn = tEntry.tFxFilenames[i] or ''
                            if fn == '' then
                                tUtil.showMessageWarn('No filename specified')
                            else
                                local okSet = dpCall(function() return meshD:setFxTexture(i, fn) end)
                                if okSet then
                                    onEdit()
                                    tUtil.showMessage(string.format(tLang.L('tex_set_ok_fmt'), 'anim ' .. i, tUtil.getShortName(fn)))
                                end
                            end
                        end
                        tImGui.SameLine()
                        if tImGui.Button(tLang.L('tex_clear') .. '##animFxClear-' .. index .. '-' .. i) then
                            local okClr = dpCall(function() return meshD:setFxTexture(i, '') end)
                            if okClr then
                                tEntry.tFxFilenames[i] = ''
                                onEdit()
                                tUtil.showMessage(string.format(tLang.L('tex_clear_ok_fmt'), 'anim ' .. i))
                            end
                        end
                        tImGui.Separator()
                        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'), {r=0.6,g=0.1,b=0.1,a=1})
                        if tImGui.Button(tLang.L('remove_animation_btn') .. '##animRm-' .. index .. '-' .. i) then
                            dpCall(function() meshD:removeAnim(i) end)
                            tEntry.info = tEntry.info or {}
                            tEntry.info.animation = math.max(0, (tEntry.info.animation or 1) - 1)
                            if tEntry.iSelectedAnim and tEntry.iSelectedAnim >= i and tEntry.iSelectedAnim > 1 then
                                tEntry.iSelectedAnim = tEntry.iSelectedAnim - 1
                            end
                            onEdit()
                            bAnimRemoved = true
                        end
                        tImGui.PopStyleColor(1)
                        tImGui.TreePop()
                    end
                end
            end
        else
            tImGui.TextDisabled('No animations')
        end
        local okTFAdd, nFAdd = dpCall(function() return meshD:getTotalFrame() end)
        local nTotalFramesAdd = (okTFAdd and nFAdd and nFAdd > 0) and nFAdd or (info.totalFrames or 0)
        if tImGui.Button(tLang.L('add_animation_btn') .. '##animAdd-' .. index) then
            if nTotalFramesAdd > 0 then
                local newAnimName = 'Animation ' .. ((tEntry.info and tEntry.info.animation or 0) + 1)
                local okA = dpCall(function()
                    return meshD:addAnim(newAnimName, 1, nTotalFramesAdd, 0.1, 1)
                end)
                if okA then
                    tEntry.info = tEntry.info or {}
                    tEntry.info.animation = (tEntry.info.animation or 0) + 1
                    tEntry.iSelectedAnim = tEntry.info.animation
                    onEdit()
                end
            end
        end
        tImGui.TreePop()
    end

    -- Frame node: view/queue frame+subset edits (outside Animations)
    showFrameNode(tEntry, meshD, index)

    -- Bones node: view/add/edit/remove the mesh's optional skeleton (diagnostic-only)
    showBonesNode(tEntry, meshD, index)

    -- Articulated Animation node: persistent parts/pivots and named clips
    showArticulatedAnimationNode(tEntry, meshD, index)

    if openNode(tEntry, 'shader', tLang.L("shader_label"), 0, 'shader-' .. index) then
        if index == iSelectedMeshIndex and tPreviewMesh then
            -- plain pcall (no dpCall/print): a mesh with no active shader animation has no FX,
            -- an expected state (shown via the "Preview required" message below), not a bug to log.
            local okSh, tShader = pcall(function() return tPreviewMesh:getShader() end)
            if okSh and tShader then
                tUtil.pushResponsiveItemWidth(180)
                local sAnim, iCurAnim = tPreviewMesh:getAnim()
                local nTotalAnim = (tPreviewMesh.getTotalAnim and tPreviewMesh:getTotalAnim()) or 1
                if nTotalAnim > 1 then
                    tImGui.Text(tLang.L("animation_shader_applies"))
                    local r, v = tImGui.InputInt('##shaderAnimIdx-' .. index, iCurAnim or 1, 1, 1, 0)
                    if r and v and v >= 1 and v <= nTotalAnim then
                        tPreviewMesh:setAnim(v)
                    end
                    tImGui.TextDisabled(sAnim or '')
                end
                local tBlend = {'DISABLE','ZERO','ONE','SRC COLOR','INV SRC COLOR','SRC ALPHA','INV SRC ALPHA','DEST ALPHA','INV DEST ALPHA','DEST COLOR','INV DEST COLOR'}
                local tBlendOp = {'ADD','SUBTRACT','REVERSE_SUBTRACT','MIN','MAX'}
                local function applyShaderToMesh() meshD:copyAnimationsFromMesh(tPreviewMesh) onEdit() end
                if tImGui.TreeNodeEx(tLang.L("blend_label"), 0) then
                    local sBlend, iBlend = tPreviewMesh:getBlend()
                    local r, ci = tImGui.Combo(tLang.L("blend_function") .. '##' .. index, (iBlend or 0) + 1, tBlend)
                    if r and ci then tPreviewMesh:setBlend(ci - 1) applyShaderToMesh() end
                    local sOp = tShader:getBlendOp()
                    local iOpIdx = 1
                    for k = 1, #tBlendOp do if tBlendOp[k] == sOp then iOpIdx = k break end end
                    r, ci = tImGui.Combo(tLang.L("blend_operation") .. '##' .. index, iOpIdx, tBlendOp)
                    if r and ci then tShader:setBlendOp(tBlendOp[ci]) applyShaderToMesh() end
                    tImGui.TreePop()
                end
                local psName, vsName = tShader:getNames()
                local tVarPs, tVarVs = tShader:getVars()
                if tImGui.TreeNodeEx(tLang.L("pixel_shader"), 0) then
                    local tList = mbm.getShaderList(false, 'ps')
                    table.insert(tList, '\0')
                    table.sort(tList)
                    tList[1] = 'No Shader'
                    local iIdx = 1
                    if psName then for k = 1, #tList do if tList[k] == psName then iIdx = k break end end end
                    local r, ci = tImGui.Combo('##ComboPS-' .. index, iIdx, tList)
                    if r and ci then
                        local _, iTypeVs = tShader:getVStype()
                        local fTimeVs = tShader:getVStime()
                        local newPs = (tList[ci] == 'No Shader') and nil or tList[ci]
                        if tShader:load(newPs, vsName, mbm.GROWING, 1.0, iTypeVs or 0, fTimeVs or 1.0) then
                            applyShaderToMesh()
                        else
                            tUtil.showMessageWarn(string.format(tLang.L("failed_to_load_shader_mesh_fmt"), tostring(tList[ci])))
                        end
                    end
                    if psName then
                        tImGui.Text(tLang.L("type_label"))
                        local _, iTypePs = tShader:getPStype()
                        r, ci = tImGui.Combo('##TypePS-' .. index, (iTypePs or 0) + 1, tAnimTypeOpts)
                        if r and ci then tShader:setPStype(ci - 1) dpCall(function() tPreviewMesh:restartAnim() end) applyShaderToMesh() end
                        tImGui.Text(tLang.L("time_short"))
                        local fTime = tShader:getPStime()
                        local rt, ft = tImGui.InputFloat('##TimePS-' .. index, fTime or 1, 0.1, 1, '%.3f', 0)
                        if rt and ft and ft >= 0 then tShader:setPStime(ft) applyShaderToMesh() end
                        for j = 1, #(tVarPs or {}) do
                            local tv = tVarPs[j]
                            local changed = false
                            if tv.type == 'number' then changed = shaderInputFloatMinMax('ps', tv, 1, 'Value') end
                            if tv.type == 'vec2' then for k, ax in ipairs({'X','Y'}) do if shaderInputFloatMinMax('ps', tv, k, ax, tv.name .. '-' .. ax) then changed = true end end end
                            if tv.type == 'vec3' then for k, ax in ipairs({'X','Y','Z'}) do if shaderInputFloatMinMax('ps', tv, k, ax, tv.name .. '-' .. ax) then changed = true end end end
                            if tv.type == 'rgb' then changed = shaderColorRGBMinMax('ps', tv) end
                            if tv.type == 'rgba' then changed = shaderColorRGBAMinMax('ps', tv) end
                            if changed then
                                tShader:setPSmin(tv.name, tv.min[1], tv.min[2], tv.min[3], tv.min[4])
                                tShader:setPSmax(tv.name, tv.max[1], tv.max[2], tv.max[3], tv.max[4])
                                tShader:setPS(tv.name, tv.value[1], tv.value[2], tv.value[3], tv.value[4])
                                applyShaderToMesh()
                            end
                        end
                    end
                    tImGui.TreePop()
                end
                if tImGui.TreeNodeEx(tLang.L("vertex_shader"), 0) then
                    local tList = mbm.getShaderList(false, 'vs')
                    table.insert(tList, '\0')
                    table.sort(tList)
                    tList[1] = 'No Shader'
                    local iIdx = 1
                    if vsName then for k = 1, #tList do if tList[k] == vsName then iIdx = k break end end end
                    local r, ci = tImGui.Combo('##ComboVS-' .. index, iIdx, tList)
                    if r and ci then
                        local _, iTypePs = tShader:getPStype()
                        local fTimePs = tShader:getPStime()
                        local newVs = (tList[ci] == 'No Shader') and nil or tList[ci]
                        if tShader:load(psName, newVs, iTypePs or 0, fTimePs or 1.0, mbm.GROWING, 1.0, 1) then
                            applyShaderToMesh()
                        else
                            tUtil.showMessageWarn(string.format(tLang.L("failed_to_load_shader_mesh_fmt"), tostring(tList[ci])))
                        end
                    end
                    if vsName then
                        tImGui.Text(tLang.L("type_label"))
                        local _, iTypeVs = tShader:getVStype()
                        r, ci = tImGui.Combo('##TypeVS-' .. index, (iTypeVs or 0) + 1, tAnimTypeOpts)
                        if r and ci then tShader:setVStype(ci - 1) dpCall(function() tPreviewMesh:restartAnim() end) applyShaderToMesh() end
                        tImGui.Text(tLang.L("time_short"))
                        local fTime = tShader:getVStime()
                        local rt, ft = tImGui.InputFloat('##TimeVS-' .. index, fTime or 1, 0.1, 1, '%.3f', 0)
                        if rt and ft and ft >= 0 then tShader:setVStime(ft) applyShaderToMesh() end
                        for j = 1, #(tVarVs or {}) do
                            local tv = tVarVs[j]
                            local changed = false
                            if tv.type == 'number' then changed = shaderInputFloatMinMax('vs', tv, 1, 'Value') end
                            if tv.type == 'vec2' then for k, ax in ipairs({'X','Y'}) do if shaderInputFloatMinMax('vs', tv, k, ax, tv.name .. '-' .. ax) then changed = true end end end
                            if tv.type == 'vec3' then for k, ax in ipairs({'X','Y','Z'}) do if shaderInputFloatMinMax('vs', tv, k, ax, tv.name .. '-' .. ax) then changed = true end end end
                            if tv.type == 'rgb' then changed = shaderColorRGBMinMax('vs', tv) end
                            if tv.type == 'rgba' then changed = shaderColorRGBAMinMax('vs', tv) end
                            if changed then
                                tShader:setVSmin(tv.name, tv.min[1], tv.min[2], tv.min[3], tv.min[4])
                                tShader:setVSmax(tv.name, tv.max[1], tv.max[2], tv.max[3], tv.max[4])
                                tShader:setVS(tv.name, tv.value[1], tv.value[2], tv.value[3], tv.value[4])
                                applyShaderToMesh()
                            end
                        end
                    end
                    tImGui.TreePop()
                end
                if tImGui.TreeNodeEx(tLang.L("texture_stage_2"), 0) then
                    local tex2 = tShader:getTextureStage2()
                    tImGui.TextDisabled(tex2 and tUtil.getShortName(tex2) or 'No Texture')
                    if tImGui.Button(tLang.L("set_texture") .. '##' .. index) then
                        local f = mbm.openFile(sLastMeshPath, table.unpack(tUtil.supported_images or {'png','jpg'}))
                        if f then
                            if type(f) == 'table' then f = f[1] end
                            tPreviewMesh:setTexture(f, true, 2)
                            applyShaderToMesh()
                        end
                    end
                    tImGui.TreePop()
                end
                tImGui.PopItemWidth()
            else
                tImGui.TextDisabled('Preview required. Select mesh to see shader options.')
            end
        else
            tImGui.TextDisabled('Copy shader from another mesh file.')
            if tImGui.Button(tLang.L("copy_from_file") .. '##' .. index) then
                local refFile = mbm.openMultiFile(sLastMeshPath, 'spt', 'msh', 'fnt', 'tile', 'ptl')
                if refFile then
                    if type(refFile) == 'table' then refFile = refFile[1] end
                    local refDir = refFile:match('^(.*)[/\\]')
                    if refDir then mbm.addPath(refDir) end
                    local refInfo = meshDebug:getInfo(refFile)
                    if refInfo and refInfo.type then
                        local refMesh = nil
                        if refInfo.type == 'sprite' then refMesh = sprite:new('2dw')
                        elseif refInfo.type == 'mesh' then refMesh = mesh:new('2dw')
                        elseif refInfo.type == 'tile' then refMesh = tile:new('2dw')
                        elseif refInfo.type == 'particle' then refMesh = particle:new('2dw')
                        elseif refInfo.type == 'font' then local f = font:new(refFile) if f then refMesh = f:add('2dw', 'Copy') end end
                        if refMesh and refMesh:load(refFile) then
                            local ok = meshD:copyAnimationsFromMesh(refMesh)
                            refMesh:destroy()
                            if ok then onEdit() tUtil.showMessage(string.format('Copied shader from %s', tUtil.getShortName(refFile)))
                            else tUtil.showMessageWarn(tLang.L("copy_failed_no_shader")) end
                        else
                            if refMesh then refMesh:destroy() end
                            tUtil.showMessageWarn(tLang.L("failed_to_load_reference_mesh"))
                        end
                    else tUtil.showMessageWarn(tLang.L("could_not_read_mesh_info")) end
                end
            end
        end
        tImGui.TreePop()
    end

    if tImGui.Button(tLang.L("check") .. '##' .. index) then
        local ok, err = meshD:check()
        local animErr = collectAnimFrameErrors(tEntry)
        if ok and not animErr then
            tUtil.showMessage(string.format('Check OK: %s', shortName))
        elseif ok and animErr then
            tUtil.showMessageWarn('Animation frame bounds exceeded:\n' .. animErr)
        elseif not ok and animErr then
            tUtil.showMessageWarn(string.format(tLang.L("check_failed_fmt"), shortName, (err or '') .. '\nAnimation frame bounds exceeded:\n' .. animErr))
        else
            tUtil.showMessageWarn(string.format(tLang.L("check_failed_fmt"), shortName, err or ''))
        end
    end

    if tImGui.Button(tLang.L("save_all_overwrite") .. '##' .. index) then
        local animErr = collectAnimFrameErrors(tEntry)
        if animErr then
            tUtil.showMessageWarn('Cannot save — animation frame bounds exceeded:\n' .. animErr)
        else
            local wasLegacy = isLegacyTextureAnimationEffectStorage(tEntry.info)
            local ok = meshD:save(tEntry.fileName, false, false)
            if ok then
                tEntry.modified = false
                local newInfo = refreshEntryInfoFromFile(tEntry)
                iLastPreviewedIndex = 0
                if wasLegacy and not isLegacyTextureAnimationEffectStorage(newInfo) then
                    tUtil.showMessage(string.format(tLang.L('mesh_migrated_save_fmt'), shortName))
                else
                    tUtil.showMessage(string.format('Saved: %s', shortName))
                end
            else
                tUtil.showMessageWarn(string.format(tLang.L("save_failed_fmt"), shortName))
            end
        end
    end
    if tImGui.IsItemHovered(0) then
        tImGui.BeginTooltip()
        tImGui.Text(tLang.L(hasFilt and 'save_overwrite_tooltip_filt' or 'save_overwrite_tooltip'))
        tImGui.EndTooltip()
    end
    --if tImGui.Button(tLang.L("save_all_calc_normals") .. '##' .. index) then
    --    local animErr = collectAnimFrameErrors(tEntry)
    --    if animErr then
    --        tUtil.showMessageWarn('Cannot save — animation frame bounds exceeded:\n' .. animErr)
    --    else
    --        local ok = meshD:save(tEntry.fileName, true, false)
    --        if ok then
    --            tEntry.modified = false
    --            if tEntry.info then tEntry.info.hasNormal = true end
    --            iLastPreviewedIndex = 0
    --            tUtil.showMessage(string.format('Saved: %s', shortName))
    --        else
    --            tUtil.showMessageWarn(string.format(tLang.L("save_failed_fmt"), shortName))
    --        end
    --    end
    --end
    --if tImGui.IsItemHovered(0) then
    --    tImGui.BeginTooltip()
    --    tImGui.Text(tLang.L(hasFilt and 'save_normals_tooltip_filt' or 'save_normals_tooltip'))
    --    tImGui.EndTooltip()
    --end
    --tImGui.TextDisabled('Overwrite: as-is. Calculated: compute normals from geometry then save.')
end

-- Defined later with the batch-save helpers; forward-declared so Save As can reuse the exact same
-- primary/material-role/animation-FX texture collection and cross-platform binary copy behavior.
local copyMeshTexturesToFolder

function doSaveAs(tEntry, index)
    local info     = tEntry.info or {}
    local meshD    = tEntry.meshDebug
    local shortName = tUtil.getShortName(tEntry.fileName)
    local extMap   = { mesh = 'msh', sprite = 'spt', font = 'fnt', tile = 'tile', particle = 'ptl' }
    local suggestedExt = extMap[info.type] or 'msh'

    local suggestedFile = tostring(tEntry.fileName or ''):gsub('%.[^%.\\/]+$', '') .. '.' .. suggestedExt
    local newFile = mbm.saveFile(suggestedFile, suggestedExt)
    if not newFile or newFile == '' then return end

    local animErr = collectAnimFrameErrors(tEntry)
    if animErr then
        tUtil.showMessageWarn('Cannot save — animation frame bounds exceeded:\n' .. animErr)
        return
    end

    -- Check whether any frame has been deselected
    local tSel = tEntry.tFrameSelection or {}
    local okT, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    nFrames = (okT and nFrames) or 0
    local hasDeselected = false
    for f = 1, nFrames do
        if tSel[f] == false then hasDeselected = true; break end
        local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if okS and nSubsets then
            for s = 1, nSubsets do
                if (tEntry.tCheckedRemove or {})[f * 100 + s] == false then
                    hasDeselected = true
                    break
                end
            end
        end
        if hasDeselected then break end
    end

    local ok = false
    local wasLegacy = isLegacyTextureAnimationEffectStorage(tEntry.info)
    if not hasDeselected then
        -- All frames selected: simple save
        ok = meshD:save(newFile, false, false)
    else
        local tempD = buildFilteredMeshForSave(tEntry)
        if not tempD then
            tUtil.showMessageWarn('Nothing to save (all frames deselected)')
            return
        end
        ok = tempD:save(newFile, false, false)
    end

    if ok then
        local newInfo = refreshEntryInfoFromFile(nil, newFile)
        local outputFolder = newFile:match('^(.*)[/\\]') or '.'
        local texturesCopied, texturesFailed = copyMeshTexturesToFolder(tEntry, outputFolder, {})
        local message
        if wasLegacy and not isLegacyTextureAnimationEffectStorage(newInfo) then
            message = string.format(tLang.L('mesh_migrated_save_fmt'), tUtil.getShortName(newFile))
        else
            message = string.format(tLang.L("save_as_success_fmt"), tUtil.getShortName(newFile))
        end
        if texturesCopied > 0 then
            message = message .. '\n' .. string.format(tLang.L('save_all_to_folder_textures_fmt'), texturesCopied)
        end
        if texturesFailed > 0 then
            message = message .. '\n' .. string.format(tLang.L('save_all_to_folder_textures_failed_fmt'), texturesFailed)
            tUtil.showMessageWarn(message)
        else
            tUtil.showMessage(message)
        end
        sLastMeshPath = newFile
    else
        tUtil.showMessageWarn(string.format(tLang.L("save_failed_fmt"), shortName))
    end
end

local function normalizeFolderPath(path)
    path = (path or ''):gsub('\\', '/')
    return path:gsub('/+$', '')
end

local function joinFolderFile(folder, fileName)
    folder = normalizeFolderPath(folder)
    if folder == '' then return fileName end
    return folder .. '/' .. fileName
end

local function getBatchPathKey(path)
    if mbm and mbm.is and mbm.is('windows') then
        return tostring(path or ''):lower()
    end
    return tostring(path or '')
end

local function makeUniqueBatchOutputPath(folder, sourceFile, usedPaths, index)
    local baseName = tUtil.getBaseFileName(sourceFile or '') or ''
    if baseName == '' then
        baseName = string.format('mesh_%d.msh', index or 1)
    end

    local stem, ext = baseName:match('^(.*)(%.[^%.]+)$')
    if not stem then
        stem = baseName
        ext = ''
    end

    local candidate = joinFolderFile(folder, baseName)
    local key = getBatchPathKey(candidate)
    local suffix = 2
    while usedPaths[key] do
        candidate = joinFolderFile(folder, string.format('%s_%d%s', stem, suffix, ext))
        key = getBatchPathKey(candidate)
        suffix = suffix + 1
    end
    usedPaths[key] = true
    return candidate
end

local function hasFrameOrSubsetFilter(tEntry)
    local meshD = tEntry.meshDebug
    local tSel = tEntry.tFrameSelection or {}
    local tChecked = tEntry.tCheckedRemove or {}
    local okT, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okT or not nFrames then return false end

    for f = 1, nFrames do
        if tSel[f] == false then return true end
        local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if okS and nSubsets then
            for s = 1, nSubsets do
                if tChecked[f * 100 + s] == false then
                    return true
                end
            end
        end
    end
    return false
end

local function buildCheckedRemoveDefaults(tEntry)
    local meshD = tEntry.meshDebug
    local oldChecked = tEntry.tCheckedRemove or {}
    local checked = {}
    local okT, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okT or not nFrames then return checked end

    for f = 1, nFrames do
        checked[f * 100] = oldChecked[f * 100] == false and false or true
        local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if okS and nSubsets then
            for s = 1, nSubsets do
                checked[f * 100 + s] = oldChecked[f * 100 + s] == false and false or true
            end
        end
    end
    return checked
end

function buildFilteredMeshForSave(tEntry)
    local oldCheckedRemove = tEntry.tCheckedRemove
    tEntry.tCheckedRemove = buildCheckedRemoveDefaults(tEntry)

    local function restoreCheckedRemove()
        tEntry.tCheckedRemove = oldCheckedRemove
    end

    if not tEntry.modified then
        local tempD = buildFilteredMesh(tEntry)
        restoreCheckedRemove()
        return tempD
    end

    local ext = tEntry.fileName:match('%.([^%.]+)$') or 'msh'
    local tempPath = os.tmpname() .. '.' .. ext
    if not tEntry.meshDebug:save(tempPath, false, false) then
        restoreCheckedRemove()
        return nil
    end

    local oldFileName = tEntry.fileName
    tEntry.fileName = tempPath
    local tempD = buildFilteredMesh(tEntry)
    tEntry.fileName = oldFileName
    restoreCheckedRemove()
    meshDebug:fakeRelease(tempPath)
    os.remove(tempPath)
    return tempD
end

local function saveMeshEntryToPath(tEntry, outFile)
    local animErr = collectAnimFrameErrors(tEntry)
    if animErr then
        return false, tLang.L('apply_all_anim_bounds_failed') .. ': ' .. animErr
    end

    if hasFrameOrSubsetFilter(tEntry) then
        local tempD = buildFilteredMeshForSave(tEntry)
        if not tempD then
            return false, tLang.L('no_frames_to_save')
        end
        return tempD:save(outFile, false, false)
    end

    return tEntry.meshDebug:save(outFile, false, false)
end

-- Every texture path actually referenced by a mesh entry: primary + material-role
-- (normal/specular/emissive/mask) textures per frame/subset, plus per-animation FX textures.
-- Used by "Save All to Folder" to also copy each mesh's textures alongside the exported .msh --
-- getMeshTextures() (used by the Info node's texture-remap UI) only looks at primary textures,
-- which would silently skip normal/specular/emissive/mask/FX textures here.
local function getMeshAllUsedTextures(tEntry)
    local meshD = tEntry.meshDebug
    local seen = {}
    local list = {}
    local function add(tex)
        if tex and tex ~= '' and not seen[tex] then
            seen[tex] = true
            table.insert(list, tex)
        end
    end

    local okF, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if okF and nFrames then
        for f = 1, nFrames do
            local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
            if okS and nSubsets then
                for s = 1, nSubsets do
                    local okP, primaryTex = dpCall(function() return meshD:getTexture(f, s) end)
                    if okP then add(primaryTex) end
                    for _, role in ipairs({'normal', 'specular', 'emissive', 'mask'}) do
                        local okR, roleTex = dpCall(function() return meshD:getMaterialTexture(f, s, role) end)
                        if okR then add(roleTex) end
                    end
                end
            end
        end
    end

    local nAnim = (tEntry.info and tEntry.info.animation) or 0
    for i = 1, nAnim do
        local okFx, fxTex = dpCall(function() return meshD:getFxTexture(i) end)
        if okFx then add(fxTex) end
    end

    return list
end

-- Plain binary read/write copy (not tUtil.copyFile, which shells out to `cp` and is a no-op on
-- Windows) so texture copying actually works on every platform this tool runs on.
local function copyFileBinary(src, dst)
    if getBatchPathKey(src) == getBatchPathKey(dst) then return true end
    local fin = io.open(src, 'rb')
    if not fin then return false end
    local data = fin:read('*a')
    fin:close()
    if not data then return false end
    local fout = io.open(dst, 'wb')
    if not fout then return false end
    fout:write(data)
    fout:close()
    return true
end

-- Copies every texture tEntry references into `folder`, next to its saved .msh, skipping any
-- basename already copied by an earlier entry in this batch. The engine resolves texture
-- references by searching known asset paths for a matching basename at load time (getFullPath,
-- file-util.cpp) rather than requiring the mesh's stored path to still exist, so a same-named
-- copy next to the .msh is enough for it to be found -- no need to rewrite the path stored
-- inside the mesh itself.
copyMeshTexturesToFolder = function(tEntry, folder, copiedBasenames)
    -- mbm.getFullPath only finds a bare/relative texture filename if the mesh's own directory
    -- was already registered as a search path -- which normally only happens lazily, the first
    -- time this specific entry is previewed (updatePreviewMesh's mbm.addPath(dir) call). A mesh
    -- loaded via "Load Mesh(s)"/"Load from folder" and never individually opened in the tree
    -- never gets that registration, so its textures would fail to resolve here. Register it
    -- explicitly so every entry resolves regardless of whether it was ever previewed.
    local dir = tEntry.fileName and tEntry.fileName:match('^(.*)[/\\]')
    if dir then mbm.addPath(dir) end

    local copied, failed = 0, 0
    for _, texPath in ipairs(getMeshAllUsedTextures(tEntry)) do
        local baseName = tUtil.getBaseFileName(texPath)
        local key = getBatchPathKey(baseName)
        if baseName ~= '' and not copiedBasenames[key] then
            copiedBasenames[key] = true
            -- Textures are commonly stored on the mesh as a bare filename or a path relative to
            -- an engine search path (mbm.addPath), not a path the plain Lua io library can open
            -- directly from the process's own working directory. Resolve it the same way the
            -- engine itself would before reading the bytes. mbm.getFullPath echoes the input
            -- back unresolved if no search path matches, so this is a no-op when texPath is
            -- already a real filesystem path.
            local resolvedPath = mbm.getFullPath(texPath) or texPath
            if copyFileBinary(resolvedPath, joinFolderFile(folder, baseName)) then
                copied = copied + 1
            else
                failed = failed + 1
            end
        end
    end
    return copied, failed
end

-- Exports every checked (frame, subset) occurrence as an independent one-frame/one-subset mesh.
function exportSelectedFrameSubsets(tEntry)
    local meshD = tEntry.meshDebug
    local selectedOccurrences = {}
    local okF, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    nFrames = (okF and nFrames) or 0
    for f = 1, nFrames do
        local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        for s = 1, (okS and nSubsets or 0) do
            if (tEntry.tCheckedRemove or {})[f * 100 + s] then
                table.insert(selectedOccurrences, {frame = f, subset = s})
            end
        end
    end

    if #selectedOccurrences == 0 then
        tUtil.showMessageWarn(tLang.L('export_subset_none_selected'))
        return
    end

    local suggestedBase = tostring(tEntry.fileName or ''):gsub('%.[^%.\\/]+$', '') .. '.msh'
    local pickedBase = mbm.saveFile(suggestedBase, 'msh')
    if not pickedBase or pickedBase == '' then return end
    pickedBase = pickedBase:gsub('%.[^%.\\/]+$', '') .. '.msh'
    local stem = pickedBase:gsub('%.msh$', '')
    local ext = 'msh'

    local animErr = collectAnimFrameErrors(tEntry)
    if animErr then
        tUtil.showMessageWarn(tLang.L('apply_all_anim_bounds_failed') .. ': ' .. animErr)
        return
    end

    local success, failed = 0, 0
    local outputFolder = pickedBase:match('^(.*)[/\\]') or '.'
    local copiedTextures = {}
    local texturesCopied, texturesFailed = 0, 0

    -- Snapshot in-memory edits once. Each output loads its own copy and destructively removes
    -- every occurrence except one exact source (frame, subset) pair.
    local sourcePath = tEntry.fileName
    local snapshotPath = nil
    if tEntry.modified then
        snapshotPath = os.tmpname() .. '.msh'
        if not meshD:save(snapshotPath, false, false) then
            tUtil.showMessageWarn(string.format(tLang.L('save_failed_fmt'), tUtil.getShortName(tEntry.fileName)))
            return
        end
        sourcePath = snapshotPath
    end

    for _, occurrence in ipairs(selectedOccurrences) do
        local sourceFrame = occurrence.frame
        local sourceSubset = occurrence.subset
        local isolatedMesh = meshDebug:new()
        local loaded = isolatedMesh:load(sourcePath)
        local hasExactlyOneOccurrence = loaded
        if hasExactlyOneOccurrence then
            local nAnim = (tEntry.info and tEntry.info.animation) or 0
            for i = nAnim, 1, -1 do
                local okAnim, name, initialFrame, finalFrame, time, animType =
                    dpCall(function() return isolatedMesh:getAnim(i) end)
                if okAnim and name and initialFrame and finalFrame then
                    if sourceFrame >= initialFrame and sourceFrame <= finalFrame then
                        isolatedMesh:updateAnim(i, name, 1, 1, time or 0.1, animType or 0)
                    else
                        isolatedMesh:removeAnim(i)
                    end
                end
            end

            local okS, sourceSubsetCount = dpCall(function()
                return isolatedMesh:getTotalSubset(sourceFrame)
            end)
            for s = (okS and sourceSubsetCount or 0), 1, -1 do
                if s ~= sourceSubset then isolatedMesh:removeSubset(sourceFrame, s) end
            end
            for f = nFrames, 1, -1 do
                if f ~= sourceFrame then isolatedMesh:removeFrame(f) end
            end

            local okFrameCount, frameCount = dpCall(function() return isolatedMesh:getTotalFrame() end)
            local okSubsetCount, subsetCount = dpCall(function() return isolatedMesh:getTotalSubset(1) end)
            hasExactlyOneOccurrence = okFrameCount and frameCount == 1
                and okSubsetCount and subsetCount == 1
        end

        local outputPath = string.format('%s-f%d-s%d.%s', stem, sourceFrame, sourceSubset, ext)
        if hasExactlyOneOccurrence and isolatedMesh:save(outputPath, false, false) then
            -- An earlier export with the same suffixed name may still be held by Mesh Debug's
            -- file cache. Invalidate it so reopening the output immediately shows this newly
            -- isolated one-subset file rather than the previous cached multi-subset version.
            meshDebug:fakeRelease(outputPath)
            success = success + 1
        else
            failed = failed + 1
        end
        isolatedMesh = nil
    end

    if snapshotPath then
        meshDebug:fakeRelease(snapshotPath)
        os.remove(snapshotPath)
    end

    if success > 0 then
        texturesCopied, texturesFailed = copyMeshTexturesToFolder(
            tEntry, outputFolder, copiedTextures)
    end
    sLastMeshPath = pickedBase

    local message = string.format(
        tLang.L('export_subset_result_fmt'), success, #selectedOccurrences, outputFolder)
    if texturesCopied > 0 then
        message = message .. '\n' .. string.format(tLang.L('save_all_to_folder_textures_fmt'), texturesCopied)
    end
    if failed > 0 then
        message = message .. '\n' .. string.format(tLang.L('export_subset_failed_fmt'), failed)
    end
    if texturesFailed > 0 then
        message = message .. '\n' .. string.format(tLang.L('save_all_to_folder_textures_failed_fmt'), texturesFailed)
    end
    if failed > 0 or texturesFailed > 0 then
        tUtil.showMessageWarn(message)
    else
        tUtil.showMessage(message, 8)
    end
end

function onSaveAllToFolder()
    if #tLoadedMeshes == 0 then
        tUtil.showMessageWarn(tLang.L('save_all_to_folder_no_meshes'))
        return
    end

    local folder = mbm.openFolder(tLang.L('save_all_to_folder'), sLastFolderPath)
    if not folder or folder == '' then return end
    folder = normalizeFolderPath(folder)
    sLastFolderPath = folder

    local usedPaths = {}
    local copiedTextures = {}
    local success = 0
    local failed = 0
    local migrated = 0
    local texturesCopied = 0
    local texturesFailed = 0
    local details = {}

    for i, tEntry in ipairs(tLoadedMeshes) do
        local outFile = makeUniqueBatchOutputPath(folder, tEntry.fileName, usedPaths, i)
        local wasLegacy = isLegacyTextureAnimationEffectStorage(tEntry.info)
        local ok, err = saveMeshEntryToPath(tEntry, outFile)
        if ok then
            success = success + 1
            if wasLegacy then
                migrated = migrated + 1
            end
            local copied, failedTex = copyMeshTexturesToFolder(tEntry, folder, copiedTextures)
            texturesCopied = texturesCopied + copied
            texturesFailed = texturesFailed + failedTex
        else
            failed = failed + 1
            if #details < 6 then
                table.insert(details, string.format('%s: %s',
                    tUtil.getShortName(tEntry.fileName),
                    err or tLang.L('an_error_occurred')))
            end
        end
    end

    local msg = string.format(tLang.L('save_all_to_folder_result_fmt'), success, #tLoadedMeshes, folder)
    if migrated > 0 then
        msg = msg .. '\n' .. string.format(tLang.L('save_all_to_folder_migrated_fmt'), migrated)
    end
    if texturesCopied > 0 then
        msg = msg .. '\n' .. string.format(tLang.L('save_all_to_folder_textures_fmt'), texturesCopied)
    end
    if texturesFailed > 0 then
        msg = msg .. '\n' .. string.format(tLang.L('save_all_to_folder_textures_failed_fmt'), texturesFailed)
    end
    if failed > 0 then
        msg = msg .. '\n' .. string.format(tLang.L('save_all_to_folder_failed_fmt'), failed)
        if #details > 0 then msg = msg .. '\n' .. table.concat(details, '\n') end
    end
    if failed > 0 or texturesFailed > 0 then
        tUtil.showMessageWarn(msg)
    else
        tUtil.showMessage(msg, 8)
    end
end

local function getApplyAllTypeData()
    local tOrder = {}
    local tCounts = {}
    for _, tEntry in ipairs(tLoadedMeshes) do
        local sType = (tEntry.info and tEntry.info.type) or 'unknown'
        if not tCounts[sType] then
            tCounts[sType] = 0
            table.insert(tOrder, sType)
        end
        tCounts[sType] = tCounts[sType] + 1
    end
    return tOrder, tCounts
end

local function ensureApplyAllTypeSelected()
    local tOrder, tCounts = getApplyAllTypeData()
    if #tOrder == 0 then
        if tApplyAllWin then tApplyAllWin.selectedType = nil end
        return tOrder, tCounts
    end
    if not tApplyAllWin.selectedType or not tCounts[tApplyAllWin.selectedType] then
        tApplyAllWin.selectedType = tOrder[1]
    end
    return tOrder, tCounts
end

local function getApplyAllTargets(sType)
    local tTargets = {}
    if not sType or sType == '' then return tTargets end
    for i, tEntry in ipairs(tLoadedMeshes) do
        local entryType = (tEntry.info and tEntry.info.type) or 'unknown'
        if entryType == sType then
            table.insert(tTargets, { entry = tEntry, index = i })
        end
    end
    return tTargets
end

local function formatApplyAllDetails(tDetails)
    if not tDetails or #tDetails == 0 then return '' end
    local tShown = {}
    local limit = math.min(#tDetails, 6)
    for i = 1, limit do
        tShown[i] = tDetails[i]
    end
    if #tDetails > limit then
        table.insert(tShown, string.format(tLang.L('apply_all_more_fmt'), #tDetails - limit))
    end
    return table.concat(tShown, '\n')
end

local function setApplyAllResult(summary)
    local headline = string.format(
        tLang.L('apply_all_summary_fmt'),
        summary.operation,
        summary.typeName or '?',
        summary.total or 0,
        summary.success or 0,
        summary.skipped or 0,
        summary.failed or 0)
    local details = formatApplyAllDetails(summary.details)
    if details ~= '' then
        headline = headline .. '\n' .. details
    end
    tApplyAllWin.lastResultText = headline
    if summary.failed > 0 and summary.success == 0 then
        tUtil.showMessageWarn(headline)
    else
        tUtil.showMessage(headline, 8)
    end
end

local function runApplyAllOperation(sType, sOperationLabel, fnApply)
    local tTargets = getApplyAllTargets(sType)
    local summary = {
        operation = sOperationLabel,
        typeName = sType,
        total = #tTargets,
        success = 0,
        skipped = 0,
        failed = 0,
        details = {},
    }
    for _, target in ipairs(tTargets) do
        local status, detail = fnApply(target.entry, target.index)
        if status == 'success' then
            summary.success = summary.success + 1
            iLastPreviewedIndex = 0
        elseif status == 'skipped' then
            summary.skipped = summary.skipped + 1
            if detail and detail ~= '' then
                table.insert(summary.details, string.format('[skip] %s: %s', tUtil.getShortName(target.entry.fileName), detail))
            end
        else
            summary.failed = summary.failed + 1
            if detail and detail ~= '' then
                table.insert(summary.details, string.format('[fail] %s: %s', tUtil.getShortName(target.entry.fileName), detail))
            end
        end
    end
    setApplyAllResult(summary)
    return summary
end

local function openApplyAllWindow()
    tApplyAllWin.open = true
    ensureApplyAllTypeSelected()
end

local function applyAllRemoveNormals(sType)
    local totalVertices = 0
    local totalBytesSaved = 0
    local summary = runApplyAllOperation(sType, tLang.L('remove_normals'), function(tEntry)
        local meshD = tEntry.meshDebug
        if tEntry.info and tEntry.info.hasNormal then
            local nVertices = getMeshTotalVertices(meshD)
            totalVertices = totalVertices + nVertices
            totalBytesSaved = totalBytesSaved + (nVertices * 12)
        end
        meshD:removeNormals()
        if tEntry.info then tEntry.info.hasNormal = false end
        tEntry.modified = true
        return 'success'
    end)
    if totalVertices > 0 then
        tApplyAllWin.lastResultText = tApplyAllWin.lastResultText
            .. string.format('\n%d vertices, ~%s saved', totalVertices, formatBytes(totalBytesSaved))
        tUtil.showMessage(tApplyAllWin.lastResultText, 8)
    end
    return summary
end

-- Same pattern as applyAllRemoveNormals above, for SECTION_VERTEX_SKIN_WEIGHTS
-- (docs/mesh-v11-format.md Sec. 6f) -- see showMeshInfoTable's own single-mesh "Remove Vertex
-- Skin Weights" button for the underlying use case (reclaim file size once external animation
-- tooling has already consumed a batch of exported FBX files).
local function applyAllRemoveVertexWeights(sType)
    local totalVertices = 0
    local totalBytesSaved = 0
    local summary = runApplyAllOperation(sType, tLang.L('remove_vertex_weights'), function(tEntry)
        local meshD = tEntry.meshDebug
        local okHas, has = dpCall(function() return meshD:hasVertexWeights() end)
        if not (okHas and has) then
            return 'skipped'
        end
        local nVertices = getMeshTotalVertices(meshD)
        totalVertices = totalVertices + nVertices
        totalBytesSaved = totalBytesSaved + (nVertices * 20) -- 4x u8 paletteIndex + 4x f32 weight per vertex
        dpCall(function() meshD:removeVertexWeights() end)
        tEntry.weightStats = nil
        tEntry.modified = true
        return 'success'
    end)
    if totalVertices > 0 then
        tApplyAllWin.lastResultText = tApplyAllWin.lastResultText
            .. string.format('\n%d vertices, ~%s saved', totalVertices, formatBytes(totalBytesSaved))
        tUtil.showMessage(tApplyAllWin.lastResultText, 8)
    end
    return summary
end

local function applyAllAddNormals(sType)
    local totalVertices = 0
    local summary = runApplyAllOperation(sType, tLang.L('add_normals'), function(tEntry)
        totalVertices = totalVertices + getMeshTotalVertices(tEntry.meshDebug)
        tEntry.meshDebug:addNormals()
        if tEntry.info then tEntry.info.hasNormal = true end
        tEntry.modified = true
        return 'success'
    end)
    if totalVertices > 0 then
        tApplyAllWin.lastResultText = tApplyAllWin.lastResultText
            .. string.format('\n%d vertices', totalVertices)
        tUtil.showMessage(tApplyAllWin.lastResultText, 8)
    end
    return summary
end

-- Applies fnNormal(vd, geoNormal) -> nx, ny, nz (or nil to leave the vertex untouched) to every
-- vertex across every frame and subset of meshD. Unlike showNormalSubsetEditor's bulkUpdate
-- (frame 1, one subset, triggered from the Mesh Tree), this walks the whole mesh so the "Apply
-- to All" bulk operation covers every animation frame, matching how removeNormals()/addNormals()
-- already behave. geoNormal is only computed (via computeGeoNormalsForSubset) when needGeo is
-- true and the subset's draw mode is TRIANGLES; it's nil otherwise.
local function bulkUpdateAllNormals(meshD, needGeo, fnNormal)
    local okF, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okF or not nFrames then return 0 end
    local okMode, modeDraw = dpCall(function() return meshD:getModeDraw() end)
    local triOk = okMode and modeDraw == 'TRIANGLES'
    local count = 0
    for f = 1, nFrames do
        local okS, nSubsets = dpCall(function() return meshD:getTotalSubset(f) end)
        if okS and nSubsets then
            for s = 1, nSubsets do
                local geo = (needGeo and triOk) and computeGeoNormalsForSubset(meshD, f, s) or nil
                local okV, nV = dpCall(function() return meshD:getTotalVertex(f, s) end)
                if okV and nV and nV > 0 then
                    for v = 1, nV do
                        local okVd, vd = dpCall(function() return meshD:getVertex(f, s, v) end)
                        if okVd and vd then
                            local nx, ny, nz = fnNormal(vd, geo and geo[v])
                            if nx then
                                vd.nx, vd.ny, vd.nz = nx, ny, nz
                                dpCall(function() meshD:setVertex(f, s, v, vd) end)
                                count = count + 1
                            end
                        end
                    end
                end
            end
        end
    end
    return count
end

local function applyAllFlipNormalsBulk(sType)
    local totalVertices = 0
    local summary = runApplyAllOperation(sType, tLang.L('normal_flip_all'), function(tEntry)
        if not (tEntry.info and tEntry.info.hasNormal) then
            return 'skipped', tLang.L('apply_all_no_matching_targets')
        end
        local count = bulkUpdateAllNormals(tEntry.meshDebug, false, function(vd)
            return -vd.nx, -vd.ny, -vd.nz
        end)
        if count == 0 then
            return 'skipped', tLang.L('apply_all_no_matching_targets')
        end
        totalVertices = totalVertices + count
        tEntry.modified = true
        tEntry.bNormalsVizDirty = true
        return 'success'
    end)
    if totalVertices > 0 then
        tApplyAllWin.lastResultText = tApplyAllWin.lastResultText
            .. string.format('\n%d vertices', totalVertices)
        tUtil.showMessage(tApplyAllWin.lastResultText, 8)
    end
    return summary
end

local function applyAllRecomputeNormalsBulk(sType)
    local totalVertices = 0
    local summary = runApplyAllOperation(sType, tLang.L('normal_recompute_all'), function(tEntry)
        if not (tEntry.info and tEntry.info.hasNormal) then
            return 'skipped', tLang.L('apply_all_no_matching_targets')
        end
        local count = bulkUpdateAllNormals(tEntry.meshDebug, true, function(vd, g)
            if g then return g.x, g.y, g.z end
            return nil
        end)
        if count == 0 then
            return 'skipped', tLang.L('apply_all_no_matching_targets')
        end
        totalVertices = totalVertices + count
        tEntry.modified = true
        tEntry.bNormalsVizDirty = true
        return 'success'
    end)
    if totalVertices > 0 then
        tApplyAllWin.lastResultText = tApplyAllWin.lastResultText
            .. string.format('\n%d vertices', totalVertices)
        tUtil.showMessage(tApplyAllWin.lastResultText, 8)
    end
    return summary
end

local function applyAllCentralize(sType)
    local xf = tApplyAllWin.transform
    return runApplyAllOperation(sType, tLang.L('centralize'), function(tEntry, index)
        local meshD = tEntry.meshDebug
        -- Bones are mesh-wide rather than per-frame. Match the targeted frame when one is
        -- selected; for "all frames", retain the established frame-1 reference behavior.
        local boneReferenceFrame = xf.frame > 0 and xf.frame or 1
        local aabb = computeMeshAABB(meshD, boneReferenceFrame, xf.subset)
        meshD:centralize(xf.frame, xf.subset)
        if aabb then
            local offX, offY, offZ = computeCentralizeOffset(aabb)
            applyTranslateToBones(meshD, -offX, -offY, -offZ)
            rebuildBoneGizmo(tEntry, meshD, index)
        end
        tEntry.modified = true
        tEntry.tTransformBoundsCache = nil
        tEntry.bPhysicsVizDirty = true
        return 'success'
    end)
end

local function applyAllCentralizeItself(sType)
    local xf = tApplyAllWin.transform
    return runApplyAllOperation(sType, tLang.L('centralize_itself'), function(tEntry)
        tEntry.meshDebug:centralizeItself(xf.frame, xf.subset)
        tEntry.modified = true
        tEntry.tTransformBoundsCache = nil
        tEntry.bPhysicsVizDirty = true
        return 'success'
    end)
end

local function applyAllTransform(sType, sMode)
    local xf = tApplyAllWin.transform
    local operationLabel = tLang.L('apply_transform')
    if sMode == 'rotate' then
        operationLabel = tLang.L('apply_rotation')
    elseif sMode == 'scale' then
        operationLabel = tLang.L('apply_scale')
    elseif sMode == 'translate' then
        operationLabel = tLang.L('apply_translate')
    end
    return runApplyAllOperation(sType, operationLabel, function(tEntry, index)
        local meshD = tEntry.meshDebug
        local ok = false
        if sMode == 'rotate' then
            ok = dpCall(function() meshD:rotateFrame(xf.frame, xf.rx, xf.ry, xf.rz, xf.subset) end)
            if ok then applyRotationToBonesDeg(meshD, xf.rx, xf.ry, xf.rz) end
        elseif sMode == 'scale' then
            ok = dpCall(function() meshD:scaleFrame(xf.frame, xf.sx, xf.sy, xf.sz, xf.subset) end)
            if ok then applyScaleToBones(meshD, xf.sx, xf.sy, xf.sz) end
        elseif sMode == 'translate' then
            ok = dpCall(function() meshD:translateFrame(xf.frame, xf.dx, xf.dy, xf.dz, xf.subset) end)
            if ok then applyTranslateToBones(meshD, xf.dx, xf.dy, xf.dz) end
        end
        if ok then
            -- Bones aren't per-frame/per-subset (one skeleton per mesh), so they always follow the
            -- bake regardless of which frame/subset the vertex bake targeted.
            rebuildBoneGizmo(tEntry, meshD, index)
            tEntry.modified = true
            tEntry.tTransformBoundsCache = nil
            tEntry.bPhysicsVizDirty = true
            return 'success'
        end
        return 'failed', tLang.L('an_error_occurred')
    end)
end

function applyAllScaleToExactSize(sType, axis)
    local xf = tApplyAllWin.transform
    local field = axis == 'X' and 'targetWidth' or (axis == 'Y' and 'targetHeight' or 'targetDepth')
    local targetSize = tonumber(xf[field]) or 0
    if targetSize <= 0 then
        tUtil.showMessageWarn(tLang.L('exact_size_positive_required'))
        return
    end
    return runApplyAllOperation(sType, string.format(tLang.L('scale_axis_to'), axis), function(tEntry, index)
        local meshD = tEntry.meshDebug
        local referenceFrame = math.max(1, xf.frame or 0)
        local aabb = computeMeshAABB(meshD, referenceFrame, xf.subset or 0)
        if not aabb then return 'skipped', tLang.L('bounds_unavailable') end
        local currentSize = axis == 'X' and (aabb.maxX - aabb.minX)
            or (axis == 'Y' and (aabb.maxY - aabb.minY) or (aabb.maxZ - aabb.minZ))
        if currentSize <= 1e-7 then return 'skipped', tLang.L('exact_size_zero_axis') end
        local sx, sy, sz = computeExactAxisScale(currentSize, targetSize, axis)
        local ok = dpCall(function() return meshD:scaleFrame(xf.frame, sx, sy, sz, xf.subset) end)
        if not ok then return 'failed', tLang.L('an_error_occurred') end
        applyScaleToBones(meshD, sx, sy, sz)
        rebuildBoneGizmo(tEntry, meshD, index)
        tEntry.modified = true
        tEntry.tTransformBoundsCache = nil
        tEntry.bPhysicsVizDirty = true
        return 'success'
    end)
end

local BOX_LETTERS_PHYS = {'a','b','c','d','e','f','g','h'}

-- 8 corners of an axis-aligned box, same CUBE_COMPLEX convention as physic_editor.lua
-- (front face a,b,c,d @ +halfDepth, back face e,f,g,h @ -halfDepth), see include/core_mbm/shapes.h
local function boxCornersPhys(hw,hh,hd)
    return {
        {x=-hw,y=-hh,z= hd}, {x=-hw,y= hh,z= hd}, {x= hw,y= hh,z= hd}, {x= hw,y=-hh,z= hd},
        {x=-hw,y=-hh,z=-hd}, {x=-hw,y= hh,z=-hd}, {x= hw,y= hh,z=-hd}, {x= hw,y=-hh,z=-hd},
    }
end

------------------------------------------------------------------------------------------------------------------
-- Physics node visualization (wireframe of the mesh's own physics shapes, read-only) --------------
------------------------------------------------------------------------------------------------------------------

-- 12 edges of an axis-aligned box, indexing boxCornersPhys()'s corner order (front face
-- 1,2,3,4 @ +halfDepth, back face 5,6,7,8 @ -halfDepth).
local PHYSICS_BOX_EDGES = {
    {1,2},{2,3},{3,4},{4,1},
    {5,6},{6,7},{7,8},{8,5},
    {1,5},{2,6},{3,7},{4,8},
}

-- Appends every edge of an axis-aligned box (8 corners, already absolute-positioned) to `ln`, one
-- 3D segment per `line:add()` call (same one-segment-per-call convention as
-- rebuildNormalVisualization above).
local function addBoxWireframe3d(ln, corners)
    for _, e in ipairs(PHYSICS_BOX_EDGES) do
        local p1, p2 = corners[e[1]], corners[e[2]]
        ln:add({p1.x, p1.y, p1.z, p2.x, p2.y, p2.z})
    end
end

-- Appends one closed 2D polygon loop (array of {x,y}) to `ln`, one segment per edge.
local function addLoop2d(ln, pts)
    local n = #pts
    for i = 1, n do
        local a = pts[i]
        local b = pts[(i % n) + 1]
        ln:add({a.x, a.y, b.x, b.y})
    end
end

-- Appends an nSeg-gon circle approximation to `ln` -- 2D only, there is no true sphere wireframe
-- primitive in this codebase's Lua (same limitation physic_editor.lua's 3D sphere visualization
-- works around by drawing a box instead, see the 3D 'sphere' branch below).
local function addCircle2d(ln, cx, cy, r, nSeg)
    local pts = {}
    for i = 0, nSeg - 1 do
        local a = (i / nSeg) * math.pi * 2
        table.insert(pts, {x = cx + math.cos(a) * r, y = cy + math.sin(a) * r})
    end
    addLoop2d(ln, pts)
end

-- Appends a 3-great-circle wireframe sphere (XY/XZ/YZ planes) to `ln` -- 3D only. There is no true
-- sphere mesh/line primitive in this codebase's Lua (physic_editor.lua's own 3D sphere
-- visualization works around the same gap by drawing a box instead, per CLAUDE.md's "physic_editor
-- .lua visualizes its own SPHERE physics shapes as boxes too" note), but a box reads as
-- indistinguishable from an actual cube shape at a glance -- three orthogonal circles read
-- unambiguously as "sphere" instead.
local function addSphereWireframe3d(ln, cx, cy, cz, r, nSeg)
    for i = 0, nSeg - 1 do
        local a1 = (i / nSeg) * math.pi * 2
        local a2 = ((i + 1) / nSeg) * math.pi * 2
        local c1, s1 = math.cos(a1) * r, math.sin(a1) * r
        local c2, s2 = math.cos(a2) * r, math.sin(a2) * r
        ln:add({cx + c1, cy + s1, cz,      cx + c2, cy + s2, cz})      -- XY plane
        ln:add({cx + c1, cy,      cz + s1, cx + c2, cy,      cz + s2}) -- XZ plane
        ln:add({cx,      cy + c1, cz + s1, cx,      cy + c2, cz + s2}) -- YZ plane
    end
end

-- Appends one meshD:getPhysics() shape to `ln`, in whichever coord space `ln` was created with.
-- 3D: cube draws as an axis-aligned wireframe box, sphere as a 3-great-circle wireframe (see
-- addSphereWireframe3d), triangle/complex draw their own exact edges since both already have real
-- vertex data. 2D: cube/complex flatten to their xy bounding rectangle (z dropped), sphere draws a
-- real circle, triangle draws its exact 3 edges.
local function addPhysicsShapeWireframe(ln, shape, is3D)
    if shape.type == 'cube' and shape.center and shape.half then
        local c, h = shape.center, shape.half
        if is3D then
            local corners = boxCornersPhys(h.x, h.y, h.z)
            for _, p in ipairs(corners) do p.x = p.x + c.x; p.y = p.y + c.y; p.z = p.z + c.z end
            addBoxWireframe3d(ln, corners)
        else
            addLoop2d(ln, {
                {x = c.x - h.x, y = c.y - h.y}, {x = c.x - h.x, y = c.y + h.y},
                {x = c.x + h.x, y = c.y + h.y}, {x = c.x + h.x, y = c.y - h.y},
            })
        end
    elseif shape.type == 'sphere' and shape.center and shape.ray then
        local c, r = shape.center, shape.ray
        if is3D then
            addSphereWireframe3d(ln, c.x, c.y, c.z, r, 24)
        else
            addCircle2d(ln, c.x, c.y, r, 32)
        end
    elseif shape.type == 'triangle' and shape.a and shape.b and shape.c then
        local a, b, c = shape.a, shape.b, shape.c
        if is3D then
            ln:add({a.x, a.y, a.z or 0, b.x, b.y, b.z or 0})
            ln:add({b.x, b.y, b.z or 0, c.x, c.y, c.z or 0})
            ln:add({c.x, c.y, c.z or 0, a.x, a.y, a.z or 0})
        else
            addLoop2d(ln, {a, b, c})
        end
    elseif shape.type == 'complex' then
        local corners = {}
        for _, key in ipairs(BOX_LETTERS_PHYS) do
            if shape[key] then table.insert(corners, shape[key]) end
        end
        if #corners == 8 then
            if is3D then
                addBoxWireframe3d(ln, corners)
            else
                local minX,maxX,minY,maxY = math.huge,-math.huge,math.huge,-math.huge
                for _, p in ipairs(corners) do
                    minX = math.min(minX, p.x); maxX = math.max(maxX, p.x)
                    minY = math.min(minY, p.y); maxY = math.max(maxY, p.y)
                end
                addLoop2d(ln, {
                    {x=minX,y=minY}, {x=minX,y=maxY}, {x=maxX,y=maxY}, {x=maxX,y=minY},
                })
            end
        end
    end
end

-- Destroys tEntry's physics-visualization line (if any) and clears the field. Same shape as
-- destroyNormalVisualization above.
function destroyPhysicsVisualization(tEntry)
    if tEntry.tPhysicsLine then tEntry.tPhysicsLine:destroy() end
    tEntry.tPhysicsLine = nil
end

-- Rebuilds the wireframe line that visualizes every physics shape on frame 1. Drawn in whichever
-- coord space the preview is currently using (2dw or 3d), matching bCameraMode3D -- same
-- rebuild-on-mismatch pattern as rebuildNormalVisualization above.
PHYSICS_PREVIEW_MAX_SHAPES = 200

function rebuildPhysicsVisualization(tEntry, meshD, cachedPhysics)
    destroyPhysicsVisualization(tEntry)
    tEntry.bPhysicsVizDirty = false
    tEntry.iPhysicsPreviewTotal = 0
    tEntry.iPhysicsPreviewShapes = 0
    local tPhysics = cachedPhysics
    if not tPhysics then
        local okP, loaded = dpCall(function() return meshD:getPhysics() end)
        if not okP then return end
        tPhysics = loaded
    end
    if not tPhysics or #tPhysics == 0 then return end

    local coordType = bCameraMode3D and '3d' or '2dw'
    tEntry.sPhysicsVizCoordType = coordType
    local is3D = coordType == '3d'
    local ln = line:new(coordType, 0, 0, 0)
    ln:setColor(0, 1, 0)
    local stride = math.max(1, math.ceil(#tPhysics / PHYSICS_PREVIEW_MAX_SHAPES))
    local previewShapes = 0
    for i, shape in ipairs(tPhysics) do
        if (i - 1) % stride == 0 then
            addPhysicsShapeWireframe(ln, shape, is3D)
            previewShapes = previewShapes + 1
        end
    end
    tEntry.iPhysicsPreviewTotal = #tPhysics
    tEntry.iPhysicsPreviewShapes = previewShapes
    tEntry.tPhysicsLine = ln
end

-- Builds physic table(s) for the given primitive kind, centered/sized from a
-- computeMeshVertexBoundsFrame1() result. Mirrors the construction math in
-- physic_editor.lua's "Add physic" button, driven by the mesh's own vertex bounds
-- instead of tMesh:getSize()/an assumed (0,0,0) center.
-- kind: 1=rectangle 2=rectangle_triangle 3=circle 4=circle_triangle 5=triangle 6=complex_cube
local function buildPhysicsFromBounds(kind, bounds, triCountRect, triCountCircle)
    local cx,cy,cz = bounds.cx, bounds.cy, bounds.cz
    local width,height,depth = bounds.width, bounds.height, bounds.depth
    local tList = {}

    if kind == 1 then --rectangle -> cube
        table.insert(tList, {type='cube', x=cx,y=cy,z=cz, width=width, height=height})
    elseif kind == 2 then --rectangle/triangle
        local half_width  = width / 2
        local half_height = height / 2
        local step_div = width / (triCountRect / 2.0)
        local step = -half_width
        for i=1, triCountRect / 2 do
            local tri1 = {x=0,y=0,z=0,type='triangle'}
            tri1.a = {x = cx + step,            y = cy - half_height}
            tri1.b = {x = cx + step,            y = cy + half_height}
            tri1.c = {x = cx + step + step_div, y = cy - half_height}
            table.insert(tList, tri1)
            step = step + step_div
            local tri2 = {x=0,y=0,z=0,type='triangle'}
            tri2.a = {x = tri1.c.x, y = tri1.c.y}
            tri2.b = {x = tri1.b.x, y = tri1.b.y}
            tri2.c = {x = cx + step, y = cy + half_height}
            table.insert(tList, tri2)
        end
    elseif kind == 3 then --circle -> sphere
        table.insert(tList, {type='sphere', x=cx,y=cy,z=cz, ray = width * 0.5})
    elseif kind == 4 then --circle/triangle
        local degree     = math.rad(360) / triCountCircle
        local ray_width  = width * 0.5
        local ray_height = height * 0.5
        local pVertex = {[1]=math.sin(0) * ray_width,[2]=math.cos(0) * ray_height}
        for i=1, triCountCircle do
            table.insert(pVertex, math.sin(degree * i) * ray_width)
            table.insert(pVertex, math.cos(degree * i) * ray_height)
        end
        local index = 3
        for i=1, triCountCircle do
            local tri = {x=0,y=0,z=0,type='triangle'}
            tri.a = {x = cx, y = cy}
            tri.b = {x = cx + pVertex[index-2], y = cy + pVertex[index-1]}
            tri.c = {x = cx + pVertex[index+0], y = cy + pVertex[index+1]}
            index = index + 2
            table.insert(tList, tri)
        end
    elseif kind == 5 then --triangle
        table.insert(tList, {
            type='triangle', x=0,y=0,z=0,
            a = {x = cx + width * -0.25, y = cy + height * -0.25},
            b = {x = cx,                 y = cy + height * 0.25},
            c = {x = cx + width * 0.25,  y = cy + height * -0.25},
        })
    elseif kind == 6 then --complex (single 8-point box)
        local useDepth = (depth and depth > 0) and depth or width
        local corners  = boxCornersPhys(width * 0.5, height * 0.5, useDepth * 0.5)
        local tComplex = {type='complex', x=0,y=0,z=0}
        for i,l in ipairs(BOX_LETTERS_PHYS) do
            tComplex[l] = {x = cx + corners[i].x, y = cy + corners[i].y, z = cz + corners[i].z}
        end
        table.insert(tList, tComplex)
    end

    return tList
end

-- Replaces each mesh's physics with one primitive computed from that mesh's own
-- frame-1 vertex bounds (not mesh:getSize()). setPhysics() fully releases the prior
-- INFO_PHYSICS before rebuilding (plugins/plugin-helper/plugin-helper.cpp), so this is
-- a true reset.
local function applyAllResetPhysics(sType)
    local opt = tApplyAllWin.physics
    return runApplyAllOperation(sType, tLang.L('reset_physics_to'), function(tEntry)
        local meshD  = tEntry.meshDebug
        local bounds = computeMeshVertexBoundsFrame1(meshD)
        if not bounds then
            return 'skipped', tLang.L('apply_all_no_matching_targets')
        end
        local tPhysics = buildPhysicsFromBounds(opt.primitiveType, bounds, opt.triCountRect, opt.triCountCircle)
        if #tPhysics == 0 then
            return 'skipped', tLang.L('apply_all_no_matching_targets')
        end
        local ok = dpCall(function() meshD:setPhysics(tPhysics) end)
        if not ok then
            return 'failed', tLang.L('an_error_occurred')
        end
        tEntry.modified = true
        return 'success'
    end)
end

-- Single-mesh counterpart to applyAllResetPhysics above, for the per-mesh Physics node's own
-- "Reset physics to" button (File > Apply to All's version targets every matching loaded mesh;
-- this one only ever touches tEntry). Same buildPhysicsFromBounds construction math, just without
-- runApplyAllOperation's multi-target iteration. Returns true, or false plus an error message.
function resetSingleMeshPhysics(tEntry, primitiveType, triCountRect, triCountCircle)
    local meshD  = tEntry.meshDebug
    local bounds = computeMeshVertexBoundsFrame1(meshD)
    if not bounds then
        return false, tLang.L('apply_all_no_matching_targets')
    end
    local tPhysics = buildPhysicsFromBounds(primitiveType, bounds, triCountRect, triCountCircle)
    if #tPhysics == 0 then
        return false, tLang.L('apply_all_no_matching_targets')
    end
    local ok = dpCall(function() meshD:setPhysics(tPhysics) end)
    if not ok then
        return false, tLang.L('an_error_occurred')
    end
    tEntry.modified = true
    return true
end

-- Overwrites each target mesh's whole material (setMaterial() applies to the entire mesh, not
-- a specific frame/subset -- there is no per-frame/subset material in the engine) with the
-- Diffuse/Ambient/Specular/Emissive/Power currently staged in tApplyAllWin.material.
local function applyAllMaterial(sType)
    local mat = cloneMaterialTable(tApplyAllWin.material)
    return runApplyAllOperation(sType, tLang.L('material'), function(tEntry)
        local meshD = tEntry.meshDebug
        local okSet = dpCall(function() meshD:setMaterial(mat) end)
        if not okSet then
            return 'failed', tLang.L('an_error_occurred')
        end
        tEntry.modified = true
        if tEntry.tMaterialUI then
            tEntry.tMaterialUI.original = cloneMaterialTable(mat)
            tEntry.tMaterialUI.current = cloneMaterialTable(mat)
        end
        return 'success'
    end)
end

local function applyAllTexture(sType, bClear)
    local tx = tApplyAllWin.texture
    local sOperation = bClear and tLang.L('tex_clear') or tLang.L('tex_set')
    return runApplyAllOperation(sType, sOperation, function(tEntry)
        local meshD = tEntry.meshDebug
        if tx.stage == 1 then
            local animIndex = math.max(1, tx.animIndex or 1)
            local ok = dpCall(function()
                return meshD:setFxTexture(animIndex, bClear and '' or tx.filename)
            end)
            if ok then
                tEntry.modified = true
                return 'success'
            end
            return 'skipped', tLang.L('apply_all_no_matching_targets')
        end
        local info = tEntry.info or {}
        local totalFrames = info.totalFrames or 0
        local count = 0
        local f1 = tx.frame == 0 and 1 or tx.frame
        local f2 = tx.frame == 0 and totalFrames or tx.frame
        for f = f1, f2 do
            local okS2, nS2 = dpCall(function() return meshD:getTotalSubset(f) end)
            nS2 = (okS2 and nS2) or 0
            local s1 = tx.subset == 0 and 1 or tx.subset
            local s2 = tx.subset == 0 and nS2 or tx.subset
            for s = s1, s2 do
                local ok = dpCall(function()
                    meshD:setTexture(f, s, bClear and '' or tx.filename)
                end)
                if ok then count = count + 1 end
            end
        end
        if count > 0 then
            tEntry.modified = true
            return 'success'
        end
        return 'skipped', tLang.L('apply_all_no_matching_targets')
    end)
end

local function applyAllUv(sType, sMode)
    return runApplyAllOperation(sType, sMode, function(tEntry)
        local shortName = tUtil.getShortName(tEntry.fileName)
        local info = tEntry.info or {}
        if not info.hasTexture then
            return 'skipped', string.format(tLang.L('uv_no_data_warning'), shortName)
        end
        if sMode == tLang.L('fix_legacy_v') then
            local fixType, n = normalizeLegacyV(tEntry.meshDebug, tApplyAllWin.uv.frame)
            if fixType == false then
                return 'failed', tLang.L('apply_all_uv_scan_failed')
            elseif fixType == 'none' then
                return 'skipped', tLang.L('fix_legacy_v_none_fmt'):format(shortName)
            end
            tEntry.modified = true
            return 'success', string.format('%s (%d)', fixType, n or 0)
        end
        local invertU = (sMode == tLang.L('invert_u') or sMode == tLang.L('invert_uv'))
        local invertV = (sMode == tLang.L('invert_v') or sMode == tLang.L('invert_uv'))
        local n = invertMeshUV(tEntry.meshDebug, tApplyAllWin.uv.frame, invertU, invertV)
        if n > 0 then
            tEntry.modified = true
            return 'success'
        end
        return 'skipped', tLang.L('apply_all_no_matching_targets')
    end)
end

local function createShaderReferenceRenderizable(fileName)
    local refInfo = meshDebug:getInfo(fileName)
    if not refInfo or not refInfo.type then
        return nil, tLang.L('could_not_read_mesh_info'), nil
    end
    local refDir = fileName:match('^(.*)[/\\]')
    if refDir then mbm.addPath(refDir) end
    local refMesh = nil
    if refInfo.type == 'sprite' then
        refMesh = sprite:new('2dw')
    elseif refInfo.type == 'mesh' then
        refMesh = mesh:new('2dw')
    elseif refInfo.type == 'tile' then
        refMesh = tile:new('2dw')
    elseif refInfo.type == 'particle' then
        refMesh = particle:new('2dw')
    elseif refInfo.type == 'font' then
        local fontRef = font:new(fileName)
        if fontRef then refMesh = fontRef:add('2dw', 'ApplyAllShader') end
    elseif refInfo.type == 'texture' then
        refMesh = texture:new('2dw')
    end
    if not refMesh or not refMesh:load(fileName) then
        if refMesh then refMesh:destroy() end
        return nil, tLang.L('failed_to_load_reference_mesh'), refInfo
    end
    if refInfo.type == 'particle' then
        refMesh:add(100)
        refMesh.revive = true
    end
    return refMesh, nil, refInfo
end

local function applyAllShaderFromFile(sType)
    local sourceFile = tApplyAllWin.shader.sourceFile or ''
    if sourceFile == '' then
        tUtil.showMessageWarn(tLang.L('apply_all_pick_shader_source'))
        return nil
    end
    local refMesh, err, refInfo = createShaderReferenceRenderizable(sourceFile)
    if not refMesh then
        tUtil.showMessageWarn(err or tLang.L('failed_to_load_reference_mesh'))
        return nil
    end
    if refInfo and refInfo.type and refInfo.type ~= sType then
        if refMesh.destroy then refMesh:destroy() end
        tUtil.showMessageWarn(string.format(tLang.L('apply_all_shader_type_mismatch_fmt'), refInfo.type, sType))
        return nil
    end
    local summary = runApplyAllOperation(sType, tLang.L('shader_label'), function(tEntry)
        local ok = tEntry.meshDebug:copyAnimationsFromMesh(refMesh)
        if ok then
            tEntry.modified = true
            return 'success'
        end
        return 'skipped', tLang.L('copy_failed_no_shader')
    end)
    if refMesh.destroy then refMesh:destroy() end
    return summary
end

local function applyAllCheck(sType)
    return runApplyAllOperation(sType, tLang.L('check'), function(tEntry)
        local ok, err = tEntry.meshDebug:check()
        local animErr = collectAnimFrameErrors(tEntry)
        if ok and not animErr then
            return 'success'
        end
        if ok and animErr then
            return 'failed', tLang.L('apply_all_anim_bounds_failed') .. ': ' .. animErr
        end
        if animErr then
            return 'failed', (err or '') .. ' | ' .. tLang.L('apply_all_anim_bounds_failed') .. ': ' .. animErr
        end
        return 'failed', err or tLang.L('an_error_occurred')
    end)
end

local function applyAllSave(sType, bRecalcNormals)
    local operationLabel = bRecalcNormals and tLang.L('save_all_calc_normals') or tLang.L('save_all_overwrite')
    return runApplyAllOperation(sType, operationLabel, function(tEntry)
        local animErr = collectAnimFrameErrors(tEntry)
        if animErr then
            return 'failed', tLang.L('apply_all_anim_bounds_failed') .. ': ' .. animErr
        end
        local wasLegacy = isLegacyTextureAnimationEffectStorage(tEntry.info)
        local ok = tEntry.meshDebug:save(tEntry.fileName, bRecalcNormals, false)
        if ok then
            tEntry.modified = false
            if bRecalcNormals and tEntry.info then tEntry.info.hasNormal = true end
            local newInfo = refreshEntryInfoFromFile(tEntry)
            if wasLegacy and not isLegacyTextureAnimationEffectStorage(newInfo) then
                return 'success', tLang.L('mesh_migrated_save_fmt'):format(tUtil.getShortName(tEntry.fileName))
            end
            return 'success'
        end
        return 'failed', string.format(tLang.L('save_failed_fmt'), tUtil.getShortName(tEntry.fileName))
    end)
end

function showApplyToAllMenu()
    if tImGui.BeginMenu(tLang.L("apply_to_all")) then
        local enabled = (#tLoadedMeshes > 0)
        if tImGui.MenuItem(tLang.L('apply_all_open_window'), nil, false, enabled) then
            openApplyAllWindow()
        end
        tImGui.EndMenu()
    end
end

function showApplyAllWindow()
    local win = tApplyAllWin
    if not win.open then return end

    local tTypes, tCounts = ensureApplyAllTypeSelected()
    local iW, iH = mbm.getSizeScreen()
    tImGui.SetNextWindowSize({x=560, y=620}, tImGui.Flags('ImGuiCond_Appearing'))
    tImGui.SetNextWindowPos({x=iW * 0.5, y=iH * 0.5}, tImGui.Flags('ImGuiCond_Appearing'), {x=0.5, y=0.5})

    local isOpen, closed = tImGui.Begin(tLang.L(tWindowsTitle.title_apply_all) .. '##applyAllWin', true, 0)
    if isOpen then
        tImGui.TextDisabled(string.format(tLang.L('apply_all_loaded_meshes_fmt'), #tLoadedMeshes))
        if #tTypes == 0 then
            tImGui.Separator()
            tImGui.TextWrapped(tLang.L('ltw_no_meshes'))
        else
            local labels = {}
            local currentIndex = 1
            for i, sType in ipairs(tTypes) do
                labels[i] = string.format('%s (%d)', sType, tCounts[sType] or 0)
                if sType == win.selectedType then currentIndex = i end
            end
            tImGui.Text(tLang.L('apply_all_target_type'))
            local changedType, newTypeIndex = tImGui.Combo('##applyAllType', currentIndex, labels, -1)
            if changedType and newTypeIndex then
                win.selectedType = tTypes[newTypeIndex]
            end
            local tTargets = getApplyAllTargets(win.selectedType)
            tImGui.TextDisabled(string.format(tLang.L('apply_all_target_count_fmt'), #tTargets, win.selectedType))
            tImGui.Separator()

            if tImGui.TreeNodeEx(tLang.L('normals_label') .. '##applyAllNormals', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                if tImGui.Button(tLang.L('remove_normals') .. '##applyAllRemoveNormals') then
                    applyAllRemoveNormals(win.selectedType)
                end
                tImGui.SameLine()
                if tImGui.Button(tLang.L('add_normals') .. '##applyAllAddNormals') then
                    applyAllAddNormals(win.selectedType)
                end
                tImGui.Separator()
                if tImGui.Button(tLang.L('normal_flip_all') .. '##applyAllFlipNormals') then
                    applyAllFlipNormalsBulk(win.selectedType)
                end
                tImGui.SameLine()
                if tImGui.Button(tLang.L('normal_recompute_all') .. '##applyAllRecomputeNormals') then
                    applyAllRecomputeNormalsBulk(win.selectedType)
                end
                tImGui.TextDisabled(tLang.L('apply_all_normals_scope_note'))
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('skin_weights_label') .. '##applyAllSkinWeights', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                if tImGui.Button(tLang.L('remove_vertex_weights') .. '##applyAllRemoveVertexWeights') then
                    applyAllRemoveVertexWeights(win.selectedType)
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('material') .. '##applyAllMaterial', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                tImGui.TextWrapped(tLang.L('apply_all_material_help'))
                local matFlags = 0
                local mat = win.material

                local function editApplyAllColor(key, label)
                    local clicked, color = tImGui.ColorEdit4(label .. '##applyAllMat-' .. key, mat[key], matFlags)
                    if clicked and color then
                        mat[key] = makeColorRGBA(color, mat[key])
                    end
                end

                editApplyAllColor('Diffuse', tLang.L('diffuse'))
                editApplyAllColor('Ambient', tLang.L('ambient'))
                editApplyAllColor('Specular', tLang.L('specular'))
                editApplyAllColor('Emissive', tLang.L('emissive'))

                local rp, np = tImGui.InputFloat(tLang.L('power') .. '##applyAllMatPower', mat.Power, 0.1, 1, '%.2f', matFlags)
                if rp then mat.Power = np end

                if tImGui.Button(tLang.L('apply_btn') .. '##applyAllMaterialApply') then
                    applyAllMaterial(win.selectedType)
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('transform') .. '##applyAllTransform', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                local xf = win.transform
                if tImGui.Button(tLang.L('centralize') .. '##applyAllCentralize') then
                    applyAllCentralize(win.selectedType)
                end
                tImGui.Separator()
                tImGui.Text(tLang.L('target_frame_label'))
                local _, nf = tImGui.InputInt('##applyAllXfFrame', xf.frame, 1, 1, 0)
                if nf ~= nil then xf.frame = math.max(0, nf) end
                tImGui.Text(tLang.L('target_subset_label'))
                local _, ns = tImGui.InputInt('##applyAllXfSubset', xf.subset, 1, 1, 0)
                if ns ~= nil then xf.subset = math.max(0, ns) end
                if tImGui.Button(tLang.L('centralize_itself') .. '##applyAllCentralizeItself') then
                    applyAllCentralizeItself(win.selectedType)
                end
                tImGui.Spacing()
                tImGui.Text(tLang.L('rotate_xyz'))
                local crx, rx = tImGui.DragFloat('X##applyAllRx', xf.rx, 1.0, 0, 0, '%.1f')
                local cry, ry = tImGui.DragFloat('Y##applyAllRy', xf.ry, 1.0, 0, 0, '%.1f')
                local crz, rz = tImGui.DragFloat('Z##applyAllRz', xf.rz, 1.0, 0, 0, '%.1f')
                if crx then xf.rx = rx end
                if cry then xf.ry = ry end
                if crz then xf.rz = rz end
                if tImGui.Button(tLang.L('apply_rotation') .. '##applyAllRotate') then
                    applyAllTransform(win.selectedType, 'rotate')
                end
                tImGui.Spacing()
                tImGui.Text(tLang.L('scale_xyz'))
                local csx, sx = tImGui.DragFloat('X##applyAllSx', xf.sx, 0.01, 0, 0, '%.3f')
                local csy, sy = tImGui.DragFloat('Y##applyAllSy', xf.sy, 0.01, 0, 0, '%.3f')
                local csz, sz = tImGui.DragFloat('Z##applyAllSz', xf.sz, 0.01, 0, 0, '%.3f')
                if csx then xf.sx = sx end
                if csy then xf.sy = sy end
                if csz then xf.sz = sz end
                if tImGui.Button(tLang.L('apply_scale') .. '##applyAllScale') then
                    applyAllTransform(win.selectedType, 'scale')
                end
                tImGui.TextWrapped(tLang.L('bulk_exact_size_help'))
                local function bulkExactSizeRow(axis, field, labelKey)
                    tImGui.TableNextRow()
                    tImGui.TableNextColumn()
                    tImGui.Text(tLang.L(labelKey))
                    tImGui.TableNextColumn()
                    tImGui.SetNextItemWidth(-1)
                    local changed, value = tImGui.InputFloat('##applyAllExact' .. axis,
                        xf[field], 1, 10, '%.3f', 0)
                    if changed then xf[field] = value end
                    tImGui.TableNextColumn()
                    if tImGui.Button(tLang.L('apply_btn') .. '##applyAllExactBtn' .. axis) then
                        applyAllScaleToExactSize(win.selectedType, axis)
                    end
                    if tImGui.IsItemHovered(0) then
                        tImGui.BeginTooltip()
                        tImGui.Text(string.format(tLang.L('scale_axis_to'), axis))
                        tImGui.EndTooltip()
                    end
                end
                local exactFlags = tImGui.Flags('ImGuiTableFlags_SizingStretchProp')
                if tImGui.BeginTable('applyAllExactSize', 3, exactFlags) then
                    tImGui.TableSetupColumn('##applyAllExactLabel', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 82)
                    tImGui.TableSetupColumn('##applyAllExactValue', tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'))
                    tImGui.TableSetupColumn('##applyAllExactButton', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 58)
                    bulkExactSizeRow('X', 'targetWidth', 'target_width')
                    bulkExactSizeRow('Y', 'targetHeight', 'target_height')
                    bulkExactSizeRow('Z', 'targetDepth', 'target_depth')
                    tImGui.EndTable()
                end
                tImGui.Spacing()
                tImGui.Text(tLang.L('translate_xyz'))
                local cdx, dx = tImGui.DragFloat('X##applyAllDx', xf.dx, 1.0, 0, 0, '%.1f')
                local cdy, dy = tImGui.DragFloat('Y##applyAllDy', xf.dy, 1.0, 0, 0, '%.1f')
                local cdz, dz = tImGui.DragFloat('Z##applyAllDz', xf.dz, 1.0, 0, 0, '%.1f')
                if cdx then xf.dx = dx end
                if cdy then xf.dy = dy end
                if cdz then xf.dz = dz end
                if tImGui.Button(tLang.L('apply_translate') .. '##applyAllTranslate') then
                    applyAllTransform(win.selectedType, 'translate')
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('physics_label') .. '##applyAllPhysics', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                local pw  = win.physics
                local idx = tImGui.RadioButton(tLang.L('rectangle'), pw.primitiveType, 1)
                idx       = tImGui.RadioButton(tLang.L('rectangle_triangle'), idx, 2)
                if idx == 2 then
                    tImGui.SameLine()
                    tUtil.pushResponsiveItemWidth(70)
                    local result, iValue = tImGui.InputInt('##applyAllPrimRect', pw.triCountRect, 2, 2, 0)
                    if result and iValue > 1 and iValue < 1000 and iValue % 2 == 0 then
                        pw.triCountRect = iValue
                    end
                    tImGui.PopItemWidth()
                end
                idx       = tImGui.RadioButton(tLang.L('circle'), idx, 3)
                idx       = tImGui.RadioButton(tLang.L('circle_triangle'), idx, 4)
                if idx == 4 then
                    tImGui.SameLine()
                    tUtil.pushResponsiveItemWidth(70)
                    local result, iValue = tImGui.InputInt('##applyAllPrimCircle', pw.triCountCircle, 1, 1, 0)
                    if result and iValue > 3 and iValue < 1000 then
                        pw.triCountCircle = iValue
                    end
                    tImGui.PopItemWidth()
                end
                idx       = tImGui.RadioButton(tLang.L('triangle'), idx, 5)
                idx       = tImGui.RadioButton(tLang.L('complex_cube'), idx, 6)
                pw.primitiveType = idx

                local color     = {r=1,g=1,b=0.4,a=1}
                local thickness = 5.0
                local winPos    = tImGui.GetCursorScreenPos()
                if idx == 1 then
                    local p_min = {x = winPos.x + 75,  y = winPos.y + 15}
                    local p_max = {x = winPos.x + 125, y = winPos.y + 65}
                    tImGui.AddRect(p_min, p_max, color, 2.0, tImGui.Flags('ImDrawFlags_RoundCornersAll'), thickness)
                elseif idx == 2 then
                    local p_min = {x = winPos.x + 75,  y = winPos.y + 15}
                    local p_max = {x = winPos.x + 125, y = winPos.y + 65}
                    tImGui.AddRect(p_min, p_max, color, 2.0, tImGui.Flags('ImDrawFlags_RoundCornersAll'), thickness)
                    tImGui.AddLine(p_min,p_max,color,thickness)
                elseif idx == 3 then
                    local center = {x=winPos.x + 100,y=winPos.y + 25 + 7.5}
                    tImGui.AddCircle(center, 25, color, 18, thickness)
                elseif idx == 4 then
                    local center = {x=winPos.x + 100,y=winPos.y + 25 + 7.5}
                    tImGui.AddNgon(center, 25, color, pw.triCountCircle, thickness)
                elseif idx == 5 then
                    local p1 = {x=0 +  winPos.x + 75,y=50 + winPos.y + 15}
                    local p2 = {x=25 + winPos.x + 75,y=0  + winPos.y + 15}
                    local p3 = {x=50 + winPos.x + 75,y=50 + winPos.y + 15}
                    tImGui.AddTriangle(p1, p2, p3, color, thickness + 5)
                elseif idx == 6 then
                    local off   = 18
                    local p_min = {x = winPos.x + 75,       y = winPos.y + 15 + off}
                    local p_max = {x = winPos.x + 125,      y = winPos.y + 65 + off}
                    local q_min = {x = winPos.x + 75 + off, y = winPos.y + 15}
                    local q_max = {x = winPos.x + 125 + off,y = winPos.y + 65}
                    tImGui.AddRect(p_min, p_max, color, 0, 0, thickness)
                    tImGui.AddRect(q_min, q_max, color, 0, 0, thickness)
                    tImGui.AddLine({x=p_min.x,y=p_min.y}, {x=q_min.x,y=q_min.y}, color, thickness)
                    tImGui.AddLine({x=p_max.x,y=p_min.y}, {x=q_max.x,y=q_min.y}, color, thickness)
                    tImGui.AddLine({x=p_min.x,y=p_max.y}, {x=q_min.x,y=q_max.y}, color, thickness)
                    tImGui.AddLine({x=p_max.x,y=p_max.y}, {x=q_max.x,y=q_max.y}, color, thickness)
                end
                winPos.y = winPos.y + 100
                tImGui.SetCursorScreenPos(winPos)

                tImGui.Separator()
                if tImGui.Button(tLang.L('reset_physics_to') .. '##applyAllResetPhysics') then
                    applyAllResetPhysics(win.selectedType)
                end
                tImGui.SameLine()
                tImGui.HelpMarker(tLang.L('reset_physics_to_tooltip'))
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('texture_node') .. '##applyAllTexture', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                local tx = win.texture
                -- Stage selector first: frame/subset only make sense for stage 0 (FX is one
                -- shared texture per animation, not addressable by frame/subset).
                tImGui.Text(tLang.L('tex_stage_label'))
                local stageOpts = {'0 - Primary', '1 - FX (per anim step)'}
                local rStage, newStageIdx = tImGui.Combo('##applyAllTxStage', tx.stage + 1, stageOpts, -1)
                if rStage and newStageIdx then tx.stage = newStageIdx - 1 end
                if tx.stage == 1 then
                    tImGui.TextWrapped(tLang.L('tex_stage1_note'))
                    tImGui.Text(tLang.L('target_animation_label'))
                    local _, nAnimIdx = tImGui.InputInt('##applyAllTxAnim', tx.animIndex or 1, 1, 1, 0)
                    if nAnimIdx ~= nil then tx.animIndex = math.max(1, nAnimIdx) end
                    local changedFxFile, newFxFile = tImGui.InputText('##applyAllTxFxFile', tx.filename or '', 512, 0)
                    if changedFxFile and newFxFile ~= nil then tx.filename = newFxFile end
                    tImGui.SameLine()
                    if tImGui.Button(tLang.L('tex_browse') .. '##applyAllTxFxBrowse') then
                        local picked = mbm.openFile(sLastMeshPath,
                            table.unpack(tUtil.supported_images or {'png', 'jpg', 'bmp', 'tga'}))
                        if picked then
                            if type(picked) == 'table' then picked = picked[1] end
                            tx.filename = picked
                        end
                    end
                    if tImGui.Button(tLang.L('tex_set') .. '##applyAllTxFxSet') then
                        if not tx.filename or tx.filename == '' then
                            tUtil.showMessageWarn('No filename specified')
                        else
                            applyAllTexture(win.selectedType, false)
                        end
                    end
                    tImGui.SameLine()
                    if tImGui.Button(tLang.L('tex_clear') .. '##applyAllTxFxClear') then
                        applyAllTexture(win.selectedType, true)
                    end
                else
                    tImGui.Text(tLang.L('target_frame_label'))
                    local _, nf = tImGui.InputInt('##applyAllTxFrame', tx.frame, 1, 1, 0)
                    if nf ~= nil then tx.frame = math.max(0, nf) end
                    tImGui.Text(tLang.L('target_subset_label'))
                    local _, ns = tImGui.InputInt('##applyAllTxSubset', tx.subset, 1, 1, 0)
                    if ns ~= nil then tx.subset = math.max(0, ns) end
                    local changedFile, newFile = tImGui.InputText('##applyAllTxFile', tx.filename or '', 512, 0)
                    if changedFile and newFile ~= nil then tx.filename = newFile end
                    tImGui.SameLine()
                    if tImGui.Button(tLang.L('tex_browse') .. '##applyAllTxBrowse') then
                        local picked = mbm.openFile(sLastMeshPath,
                            table.unpack(tUtil.supported_images or {'png', 'jpg', 'bmp', 'tga'}))
                        if picked then
                            if type(picked) == 'table' then picked = picked[1] end
                            tx.filename = picked
                        end
                    end
                    if tImGui.Button(tLang.L('tex_set') .. '##applyAllTxSet') then
                        if not tx.filename or tx.filename == '' then
                            tUtil.showMessageWarn('No filename specified')
                        else
                            applyAllTexture(win.selectedType, false)
                        end
                    end
                    tImGui.SameLine()
                    if tImGui.Button(tLang.L('tex_clear') .. '##applyAllTxClear') then
                        applyAllTexture(win.selectedType, true)
                    end
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('uv_label') .. '##applyAllUv', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                local uv = win.uv
                tImGui.Text(tLang.L('target_frame_label'))
                local _, nf = tImGui.InputInt('##applyAllUvFrame', uv.frame, 1, 1, 0)
                if nf ~= nil then uv.frame = math.max(0, nf) end
                if tImGui.Button(tLang.L('invert_u') .. '##applyAllInvU') then
                    applyAllUv(win.selectedType, tLang.L('invert_u'))
                end
                if tImGui.Button(tLang.L('invert_v') .. '##applyAllInvV') then
                    applyAllUv(win.selectedType, tLang.L('invert_v'))
                end
                if tImGui.Button(tLang.L('invert_uv') .. '##applyAllInvUV') then
                    applyAllUv(win.selectedType, tLang.L('invert_uv'))
                end
                if tImGui.Button(tLang.L('fix_legacy_v') .. '##applyAllFixLegacyV') then
                    applyAllUv(win.selectedType, tLang.L('fix_legacy_v'))
                end
                if tImGui.IsItemHovered(0) then
                    tImGui.BeginTooltip()
                    tImGui.Text(tLang.L('fix_legacy_v_tooltip'))
                    tImGui.EndTooltip()
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('shader_label') .. '##applyAllShader', tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                tImGui.TextWrapped(tLang.L('apply_all_shader_help'))
                local changedSource, newSource = tImGui.InputText('##applyAllShaderFile', win.shader.sourceFile or '', 512, 0)
                if changedSource and newSource ~= nil then win.shader.sourceFile = newSource end
                tImGui.SameLine()
                if tImGui.Button(tLang.L('copy_from_file') .. '##applyAllShaderBrowse') then
                    local picked = mbm.openMultiFile(sLastMeshPath, 'spt', 'msh', 'fnt', 'tile', 'ptl')
                    if picked then
                        if type(picked) == 'table' then picked = picked[1] end
                        win.shader.sourceFile = picked
                    end
                end
                if tImGui.Button(tLang.L('apply_all_shader_copy') .. '##applyAllShaderCopy') then
                    applyAllShaderFromFile(win.selectedType)
                end
                tImGui.TreePop()
            end

            if tImGui.TreeNodeEx(tLang.L('apply_all_validation_save'), tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')) then
                if tImGui.Button(tLang.L('check') .. '##applyAllCheck') then
                    applyAllCheck(win.selectedType)
                end
                tImGui.SameLine()
                if tImGui.Button(tLang.L('save_all_overwrite') .. '##applyAllSave') then
                    applyAllSave(win.selectedType, false)
                end
                tImGui.SameLine()
                if tImGui.Button(tLang.L('save_all_calc_normals') .. '##applyAllSaveNormals') then
                    applyAllSave(win.selectedType, true)
                end
                tImGui.TreePop()
            end

            tImGui.Separator()
            tImGui.Text(tLang.L('apply_all_last_result'))
            local text = (win.lastResultText and win.lastResultText ~= '') and win.lastResultText or tLang.L('apply_all_no_result')
            local flagsRO = tImGui.Flags('ImGuiInputTextFlags_ReadOnly')
            tImGui.InputTextMultiline('##applyAllResult', text, {x=-1, y=130}, flagsRO)
        end
    end
    tImGui.End()
    if closed then
        win.open = false
    end
end

function main_menu_mesh_debug()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu(tLang.L("menu_file")) then
            if tImGui.MenuItem(tLang.L("load_meshes")) then
                onLoadMeshFromFile()
            end
            if tImGui.MenuItem(tLang.L("load_from_folder")) then
                onLoadMeshFromFolder()
            end
            if tImGui.MenuItem(tLang.L("add_colored_cube")) then
                onAddColoredCube()
            end
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L('save_as'), nil, false,
                    iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes) then
                doSaveAs(tLoadedMeshes[iSelectedMeshIndex], iSelectedMeshIndex)
            end
            if tImGui.MenuItem(tLang.L("save_all_to_folder"), nil, false, #tLoadedMeshes > 0) then
                onSaveAllToFolder()
            end
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L('mixamo_guide_menu')) then
                onOpenMixamoGuideDialog()
            end
            if tImGui.MenuItem(tLang.L('import_via_blender')) then
                onOpenBlenderImportDialog()
            end
            if tImGui.MenuItem(tLang.L('bones_export_current_button'), nil, false,
                    iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes) then
                local tEntry = tLoadedMeshes[iSelectedMeshIndex]
                -- Default filename must end in .fbx, not carry the source mesh's own .msh extension
                -- forward -- same extension-swap makeUniqueFbxOutputPath already does for "Export
                -- All Meshes", applied here only on first use (sLastMeshExportFbxPath, once set,
                -- already ends in .fbx from a prior export and is reused as-is).
                local defaultFbxName = sLastMeshExportFbxPath or
                    (tUtil.getShortName(tEntry.fileName):gsub('%.[^%.]+$', '') .. '.fbx')
                local outputFbx = mbm.saveFile(defaultFbxName, 'fbx')
                if outputFbx then
                    sLastMeshExportFbxPath = outputFbx
                    tMeshExportOptionsState.tEntries = { { name = tUtil.getShortName(tEntry.fileName), meshD = tEntry.meshDebug, outputFbx = outputFbx, fileName = tEntry.fileName } }
                    tMeshExportOptionsState.bOpen = true
                    tMeshExportOptionsState.bOpenPopup = true
                end
            end
            if tImGui.MenuItem(tLang.L('bones_export_all_button'), nil, false, #tLoadedMeshes > 0) then
                local folder = mbm.openFolder(tLang.L('bones_export_all_button'), sLastFolderPath)
                if folder and folder ~= '' then
                    sLastFolderPath = folder
                    local usedNames = {}
                    local entries = {}
                    for i, e in ipairs(tLoadedMeshes) do
                        local outputFbx = makeUniqueFbxOutputPath(folder, e.fileName, usedNames, i)
                        table.insert(entries, { name = tUtil.getShortName(e.fileName), meshD = e.meshDebug, outputFbx = outputFbx, fileName = e.fileName })
                    end
                    tMeshExportOptionsState.tEntries = entries
                    tMeshExportOptionsState.bOpen = true
                    tMeshExportOptionsState.bOpenPopup = true
                end
            end
            tImGui.Separator()
            if tImGui.MenuItem('Legacy: Load OBJ(s)') then
                onLoadObj()
            end
            tImGui.Separator()
            showApplyToAllMenu()
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L("clear_all")) then
                tLoadedMeshes = {}
                iSelectedMeshIndex = 0
                iLastPreviewedIndex = 0
                destroyPreviewMesh()
                tUtil.showMessage(tLang.L("cleared_all_meshes"))
            end
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L("menu_quit")) then
                mbm.quit()
            end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(tLang.L("menu_view")) then
            local pressed, checked = tImGui.MenuItem(tLang.L("show_mesh_tree"), nil, bShowMeshTree)
            if pressed then
                bShowMeshTree = not bShowMeshTree
            end
            local showOrigin = bCameraMode3D and bShowOrigin3d or bShowOrigin2d
            local pressedOrigin = tImGui.MenuItem(tLang.L('enable_origin_lines'), nil, showOrigin)
            if pressedOrigin then
                setMeshDebugOriginLinesVisible(not showOrigin)
            end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(tLang.L("menu_options")) then
            tLang.renderLanguageSubmenu()
            tImGui.Separator()
            local pressedLT, _ = tImGui.MenuItem(tLang.L('list_textures'))
            if pressedLT then
                tListTexturesWin.open = true
                if tListTexturesWin.folder == '' and sLastFolderPath ~= '' then
                    tListTexturesWin.folder = sLastFolderPath
                end
                tListTexturesWin.needRebuild = true
            end
            local pressedLM, _ = tImGui.MenuItem(tLang.L('list_meshes'))
            if pressedLM then
                tListMeshesWin.open = true
                if tListMeshesWin.folder == '' and sLastFolderPath ~= '' then
                    tListMeshesWin.folder = sLastFolderPath
                end
                tListMeshesWin.needRebuild = true
            end
            if tImGui.BeginMenu(tLang.L("background_color")) then
                local sz        = tImGui.GetTextLineHeight()
                
                local rounding  =  0
                local flags     =  0

                local colors    = { {'default',    tUtil.tColorBackground},
                                    {'white',      {r=1,g=1,b=1,a=1}},
                                    {'black',      {r=0,g=0,b=0,a=1}},
                                    {'red',        {r=1,g=0,b=0,a=1}},
                                    {'green',      {r=0,g=1,b=0,a=1}},
                                    {'blue',       {r=0,g=0,b=1,a=1}},
                                    {'cyan',       {r=0,g=1,b=1,a=1}},
                                    {'yellow',     {r=1,g=1,b=0,a=1}},
                                    {'magenta',    {r=1,g=0,b=1,a=1}}
                                  }
                
                for i=1, #colors do
                    local winPos  = tImGui.GetCursorScreenPos()
                    local p_max   = {x=winPos.x + sz,y=winPos.y + sz}
                    local name    = tLang.L(colors[i][1])
                    local color   = colors[i][2]
                    tImGui.AddRectFilled(winPos, p_max, color, rounding, flags)
                    tImGui.Dummy({x =sz, y = sz})
                    tImGui.SameLine()
                    local pressed,checked = tImGui.MenuItem(name)
                    if pressed then
                        mbm.setColor(color.r,color.g,color.b)
                        tColorBackgroundGlobal = color
                    end
                end
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu(tLang.L("menu_about")) then
            local pressed = tImGui.MenuItem(tLang.L("mesh_debug_editor"), nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/editors.html#mesh-debug"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/editors.html#mesh-debug"')
                elseif mbm.is('macos') then
                    os.execute('open "https://mbm-documentation.readthedocs.io/en/latest/editors.html#mesh-debug"')
                end
            end
            pressed = tImGui.MenuItem(tLang.L("mbm_engine"), nil, false)
            if pressed then
                if mbm.is('windows') then
                    os.execute('start "" "https://mbm-documentation.readthedocs.io/en/latest/"')
                elseif mbm.is('linux') then
                    os.execute('sensible-browser "https://mbm-documentation.readthedocs.io/en/latest/"')
                elseif mbm.is('macos') then
                    os.execute('open "https://mbm-documentation.readthedocs.io/en/latest/"')
                end
            end
            if tImGui.BeginMenu(tLang.L("menu_version")) then
                tImGui.TextDisabled(string.format('%s\nIMGUI: %s', mbm.get('version'), tImGui.GetVersion()))
                tImGui.EndMenu()
            end
            tImGui.EndMenu()
        end
        tImGui.EndMainMenuBar()
    end
end

function showMeshTreeWindow()
    if not bShowMeshTree then return end

    -- The tree contains long articulated labels (frame/subset identity, pivot channels and
    -- keyframe controls). Keep a usable minimum while still allowing the user to resize it.
    local width = 520
    local iW, iH = mbm.getSizeScreen()
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_mesh_tree, 0, 0, width, width + 100, iH * 0.8)
    local minTreeWidth = math.min(width, math.max(350, iW - 40))
    local maxTreeWidth = math.max(minTreeWidth, iW - 10)
    tImGui.SetNextWindowSizeConstraints({x = minTreeWidth, y = 260},
        {x = maxTreeWidth, y = math.max(260, iH - 20)})
    local is_opened, closed_clicked = tImGui.Begin(tLang.L(tWindowsTitle.title_mesh_tree), true, 0)

    if is_opened then
        iLoadedMeshesWindowWidth = tImGui.GetWindowWidth()
        if tImGui.BeginMenuBar() then
            if tImGui.MenuItem('Load Mesh(s)') then
                onLoadMeshFromFile()
            end
            if tImGui.MenuItem(tLang.L("load_from_folder")) then
                onLoadMeshFromFolder()
            end
            if tImGui.MenuItem(tLang.L("save_all_to_folder"), nil, false, #tLoadedMeshes > 0) then
                onSaveAllToFolder()
            end
            showApplyToAllMenu()
            tImGui.EndMenuBar()
        end

        tImGui.TextDisabled(string.format('%d mesh(es) loaded', #tLoadedMeshes))

        if #tLoadedMeshes == 0 then
            tImGui.TextWrapped(tLang.L("use_file_menu_or_load"))
        else
            local tToRemove = {}
            for i = 1, #tLoadedMeshes do
                local tEntry = tLoadedMeshes[i]
                local shortName = tUtil.getShortName(tEntry.fileName)
                local typeStr = (tEntry.info and tEntry.info.type) or '?'
                local hasFilt = false
                for _, v in pairs(tEntry.tFrameSelection or {}) do
                    if v == false then hasFilt = true; break end
                end
                local label = string.format('%s [%s]%s%s', shortName, typeStr,
                    tEntry.modified and ' *' or '',
                    hasFilt and ' ~' or '')

                local isSelected = (iSelectedMeshIndex == i)
                tImGui.SetNextItemOpen(isSelected, tImGui.Flags('ImGuiCond_Always'))
                local flags = isSelected and tImGui.Flags('ImGuiTreeNodeFlags_Selected') or tImGui.Flags('ImGuiTreeNodeFlags_None')

                if tImGui.TreeNodeEx(label, flags, 'mesh-' .. i) then
                    iSelectedMeshIndex = i
                    showMeshOptions(tEntry, i)
                    if tImGui.Button(tLang.L("remove_from_list") .. '##' .. i) then
                        table.insert(tToRemove, i)
                    end
                    tImGui.TreePop()
                else
                    if i == iSelectedMeshIndex then
                        iSelectedMeshIndex = 0
                    end
                    -- showMeshOptions (and its own Normals-node auto-cancel) only runs for the
                    -- currently expanded/selected entry, so a collapsed entry's normal-viz lines
                    -- would otherwise never get destroyed and leak on screen after switching to
                    -- another mesh. Enforce it here unconditionally instead.
                    if tEntry.tNormalLineGood or tEntry.tNormalLineBad then
                        destroyNormalVisualization(tEntry)
                    end
                    -- Same leak class, same fix, for the Physics node's wireframe line.
                    if tEntry.tPhysicsLine then
                        destroyPhysicsVisualization(tEntry)
                    end
                    -- Same pre-existing leak class for the Transform tab's preview clone (its
                    -- own auto-cancel likewise only ran inside showMeshOptions, i.e. only while
                    -- this entry was the selected one).
                    if tEntry.tXformPreviewMesh then
                        tEntry.tXformPreviewMesh:destroy()
                        tEntry.tXformPreviewMesh = nil
                    end
                    destroyTransformSubsetHoverMarker(tEntry)
                    destroySplitCaptureIslandMarkers(tEntry)
                end
            end
            for j = #tToRemove, 1, -1 do
                removeMeshFromTable(tToRemove[j])
            end
        end
    end
    if closed_clicked then
        bShowMeshTree = false
    end
    tImGui.End()
end

function showCameraWindow()
    -- getRealSizeScreen (physical framebuffer pixels), not getSizeScreen (logical/scaled
    -- points): SetNextWindowPos operates in the same space ImGui itself renders in, which on a
    -- HiDPI/Retina display (macOS) is larger than the logical size. Using the logical size here
    -- pushed this right-anchored window off the right edge of the screen on macOS.
    local iW = mbm.getRealSizeScreen()
    local winW = 300
    tImGui.SetNextWindowPos({x = iW - winW - 5, y = 25}, tImGui.Flags('ImGuiCond_Once'))
    local wFlags = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoCollapse')
    -- Mode toggle: 2D / 3D radio buttons
    local camMode = bCameraMode3D and 1 or 0
    local camModeStr = bCameraMode3D and ' 3d' or ' 2d'
    local opened = tImGui.Begin(tLang.L('camera_panel') .. camModeStr .. '##camWin', false, wFlags)
    if opened then
        camMode = tImGui.RadioButton(tLang.L('camera_2d') .. '##camMode', camMode, 0)
        tImGui.SameLine()
        camMode = tImGui.RadioButton(tLang.L('camera_3d') .. '##camMode', camMode, 1)
        setMeshDebugCameraMode3d(camMode == 1)
        -- Origin lines checkbox (per-camera)
        local showOrig
        if bCameraMode3D then showOrig = bShowOrigin3d else showOrig = bShowOrigin2d end
        local newOrig = tImGui.Checkbox(tLang.L('enable_origin_lines') .. '##origLines', showOrig)
        if newOrig ~= showOrig then
            setMeshDebugOriginLinesVisible(newOrig)
        end
        tImGui.Separator()

        if bCameraMode3D then
            if iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
                local c = tLoadedMeshes[iSelectedMeshIndex].cam3d
                if tUtil.drawOrbitGizmo(c) then
                    applyCam3d(c)
                end
                tImGui.Separator()
                local px, py, pz = cam3dGetPos(c)
                tUtil.pushResponsiveItemWidth(72)

                -- Position (editable; back-computes spherical coords on change)
                tImGui.Text(tLang.L('cam_position'))
                local r1, nx = tImGui.InputFloat('X##cpx', px, 0, 0, '%.1f', 0)
                local r2, ny = tImGui.InputFloat('Y##cpy', py, 0, 0, '%.1f', 0)
                local r3, nz = tImGui.InputFloat('Z##cpz', pz, 0, 0, '%.1f', 0)
                if r1 or r2 or r3 then
                    nx = r1 and nx or px
                    ny = r2 and ny or py
                    nz = r3 and nz or pz
                    local dx = nx - c.fx; local dy = ny - c.fy; local dz = nz - c.fz
                    local dist = math.sqrt(dx*dx + dy*dy + dz*dz)
                    if dist > 0 then
                        c.distance  = dist
                        c.elevation = math.asin(math.max(-1, math.min(1, dy / dist)))
                        c.azimuth   = math.atan(dx, dz)
                    end
                    applyCam3d(c)
                end

                -- Focus (drag-to-edit)
                tImGui.Text(tLang.L('cam_focus'))
                local f1, nfx = tImGui.DragFloat('X##cfx', c.fx, 1.0, 0, 0, '%.1f', 0)
                local f2, nfy = tImGui.DragFloat('Y##cfy', c.fy, 1.0, 0, 0, '%.1f', 0)
                local f3, nfz = tImGui.DragFloat('Z##cfz', c.fz, 1.0, 0, 0, '%.1f', 0)
                if f1 then c.fx = nfx end
                if f2 then c.fy = nfy end
                if f3 then c.fz = nfz end
                if f1 or f2 or f3 then applyCam3d(c) end

                -- Distance shortcut
                local rd, ndist = tImGui.InputFloat(tLang.L('cam_distance') .. '##cdist',
                                                    c.distance, 10, 100, '%.0f', 0)
                if rd and ndist and ndist > 0 then c.distance = ndist; applyCam3d(c) end

                tImGui.PopItemWidth()
                tImGui.Separator()
                if tImGui.Button(tLang.L('reset_camera') .. '##cam3dReset') then
                    c.azimuth = 0.3; c.elevation = 0.3; c.distance = 500
                    c.fx = 0; c.fy = 0; c.fz = 0
                    applyCam3d(c)
                end
                tImGui.TextDisabled(tLang.L('cam_hint_3d'))
                tImGui.TextDisabled('Scroll:zoom')
                tImGui.TextDisabled(tLang.L('cam_hint_keyboard'))
            else
                tImGui.TextDisabled(tLang.L('cam_no_mesh'))
            end
        else
            tUtil.pushResponsiveItemWidth(72)
            local rx, nx = tImGui.DragFloat('X##c2dx', camera2d.x, 5.0, 0, 0, '%.1f', 0)
            local ry, ny = tImGui.DragFloat('Y##c2dy', camera2d.y, 5.0, 0, 0, '%.1f', 0)
            if rx or ry then
                camera2d:setPos(rx and nx or camera2d.x, ry and ny or camera2d.y)
            end
            tImGui.PopItemWidth()
            tImGui.Separator()
            if tImGui.Button(tLang.L('reset_camera') .. '##cam2dReset') then
                camera2d:setPos(0, 0)
            end
            tImGui.Separator()
            tImGui.TextDisabled(tLang.L('cam_hint_2d'))
        end
    end
    tImGui.End()
end

function showLightWindow()
    -- See showCameraWindow's comment: must use the real/physical screen size, not the
    -- logical one, or this right-anchored window ends up off-screen on HiDPI displays.
    local iW = mbm.getRealSizeScreen()
    local winW = 470
    local winY = bCameraMode3D and 500 or 220
    tImGui.SetNextWindowPos({x = iW - winW - 5, y = winY}, tImGui.Flags('ImGuiCond_Once'))
    local camModeStr = bCameraMode3D and ' 3d' or ' 2d'
    local wFlags = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize', 'ImGuiWindowFlags_NoCollapse')
    local opened = tImGui.Begin(tLang.L('light_panel') .. camModeStr .. '##lightWin', false, wFlags)
    if opened then
        if bCameraMode3D then
            showEditorLightPanel('3d', '3d')
        else
            showEditorLightPanel('2dw', '2dw')
        end
    end
    tImGui.End()
end

-- ─────────────────────────────────────────────────────────────────────────────
-- List Textures window
-- ─────────────────────────────────────────────────────────────────────────────
local function buildListTexturesData()
    local win = tListTexturesWin
    win.usedSet  = {}
    win.usedList = {}
    for _, tE in ipairs(tLoadedMeshes) do
        if tE.meshDebug then
            local texList = getMeshTextures(tE.meshDebug)
            for _, tex in ipairs(texList) do
                local bn = tUtil.getBaseFileName(tex)
                if bn and bn ~= '' and not win.usedSet[bn] then
                    win.usedSet[bn] = true
                    table.insert(win.usedList, bn)
                end
            end
        end
    end
    table.sort(win.usedList)

    win.folderFiles   = {}
    win.selectedCount = 0
    if win.folder ~= '' then
        local imgExts = { png=true, jpg=true, jpeg=true, bmp=true, tga=true,
                          gif=true, psd=true, tif=true, tiff=true }
        local tFiles  = mbm.listFiles(win.folder, false)
        if tFiles then
            local sep = tFiles.separator or '/'
            for i = 1, #tFiles do
                local tDir = tFiles[i]
                for j = 1, #tDir do
                    local name = tDir[j]
                    local ext  = (name:match('%.([^%.]+)$') or ''):lower()
                    if imgExts[ext] then
                        local fullPath = tDir.path .. sep .. name
                        table.insert(win.folderFiles, {
                            name     = name,
                            path     = fullPath,
                            used     = win.usedSet[name] == true,
                            selected = false,
                        })
                    end
                end
            end
        end
        table.sort(win.folderFiles, function(a, b)
            return a.name:lower() < b.name:lower()
        end)
    end
    win.needRebuild = false
end

function showListTexturesWindow()
    local win = tListTexturesWin
    if not win.open then return end
    if win.needRebuild then buildListTexturesData() end

    local iW, iH = mbm.getSizeScreen()
    tImGui.SetNextWindowSize({x=620, y=540}, tImGui.Flags('ImGuiCond_Appearing'))
    tImGui.SetNextWindowPos({x=iW*0.5, y=iH*0.5},
        tImGui.Flags('ImGuiCond_Appearing'), {x=0.5, y=0.5})

    local is_open, closed = tImGui.Begin(
        tLang.L('list_textures') .. '##ltwWin', true, 0)

    if is_open then
        -- ── Top panel: used textures ─────────────────────────────────────────
        tImGui.Text(tLang.L('ltw_used_section')
            .. string.format(' (%d)', #win.usedList))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('ltw_refresh') .. '##ltwRefresh') then
            win.needRebuild = true
            buildListTexturesData()
        end
        local usedText = #win.usedList > 0
            and table.concat(win.usedList, '\n')
            or  tLang.L('ltw_no_meshes')
        local flagsRO = tImGui.Flags('ImGuiInputTextFlags_ReadOnly')
        tImGui.InputTextMultiline('##ltwUsed', usedText, {x=-1, y=110}, flagsRO)

        tImGui.Separator()

        -- ── Folder selector ───────────────────────────────────────────────────
        tImGui.Text(tLang.L('ltw_folder_section'))
        tImGui.Spacing()
        tImGui.Text(tLang.L('ltw_folder_label'))
        tImGui.SameLine()
        local dispFolder = win.folder ~= '' and win.folder
                           or tLang.L('ltw_no_folder')
        tImGui.TextDisabled(dispFolder)
        if tImGui.IsItemHovered(0) and win.folder ~= '' then
            tImGui.BeginTooltip()
            tImGui.Text(win.folder)
            tImGui.EndTooltip()
        end
        if tImGui.Button(tLang.L('ltw_browse_folder') .. '##ltwBrowse') then
            local picked = mbm.openFolder(tLang.L('list_textures'),
                win.folder ~= '' and win.folder or sLastFolderPath)
            if picked then
                win.folder = picked:gsub('\\', '/')
                sLastFolderPath = win.folder
                win.needRebuild = true
                buildListTexturesData()
            end
        end
        if win.folder ~= '' then
            local total  = #win.folderFiles
            local unused = 0
            for _, f in ipairs(win.folderFiles) do
                if not f.used then unused = unused + 1 end
            end
            tImGui.SameLine()
            tImGui.TextDisabled(string.format(tLang.L('ltw_unused_count_fmt'), total, unused))
        end

        tImGui.Spacing()

        -- ── File table ────────────────────────────────────────────────────────
        local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders',
                                      'ImGuiTableFlags_RowBg',
                                      'ImGuiTableFlags_ScrollY')
        if tImGui.BeginTable('ltwFiles', 3, tblFlags, {x=-1, y=-38}) then
            tImGui.TableSetupScrollFreeze(0, 1)
            tImGui.TableSetupColumn('##cb',
                tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 22)
            tImGui.TableSetupColumn('File')
            tImGui.TableSetupColumn('Status',
                tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 80)
            tImGui.TableHeadersRow()

            if #win.folderFiles == 0 then
                tImGui.TableNextRow()
                tImGui.TableNextColumn()
                tImGui.TableNextColumn()
                tImGui.TextDisabled(win.folder ~= ''
                    and tLang.L('ltw_no_files')
                    or  tLang.L('ltw_no_folder'))
            else
                for idx, f in ipairs(win.folderFiles) do
                    tImGui.TableNextRow()
                    tImGui.TableNextColumn()
                    if f.used then
                        tImGui.BeginDisabled(true)
                        tImGui.Checkbox('##ltwCb-' .. idx, false)
                        tImGui.EndDisabled()
                    else
                        local newSel = tImGui.Checkbox('##ltwCb-' .. idx, f.selected)
                        if newSel ~= f.selected then
                            f.selected = newSel
                            win.selectedCount = win.selectedCount
                                + (newSel and 1 or -1)
                        end
                    end

                    tImGui.TableNextColumn()
                    tImGui.Text(f.name)
                    if tImGui.IsItemHovered(0) then
                        tImGui.BeginTooltip()
                        tImGui.Text(f.path)
                        tImGui.EndTooltip()
                    end

                    tImGui.TableNextColumn()
                    if f.used then
                        tImGui.PushStyleColor(
                            tImGui.Flags('ImGuiCol_Text'),
                            {r=0.3, g=0.9, b=0.3, a=1})
                        tImGui.Text(tLang.L('ltw_status_used'))
                        tImGui.PopStyleColor(1)
                    else
                        tImGui.PushStyleColor(
                            tImGui.Flags('ImGuiCol_Text'),
                            {r=0.9, g=0.6, b=0.1, a=1})
                        tImGui.Text(tLang.L('ltw_status_unused'))
                        tImGui.PopStyleColor(1)
                    end
                end
            end
            tImGui.EndTable()
        end

        -- ── Bottom action bar ─────────────────────────────────────────────────
        if tImGui.Button(tLang.L('ltw_select_all') .. '##ltwSelAll') then
            win.selectedCount = 0
            for _, f in ipairs(win.folderFiles) do
                if not f.used then
                    f.selected        = true
                    win.selectedCount = win.selectedCount + 1
                end
            end
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('ltw_select_none') .. '##ltwSelNone') then
            for _, f in ipairs(win.folderFiles) do
                f.selected = false
            end
            win.selectedCount = 0
        end
        tImGui.SameLine()
        local hasSelection = win.selectedCount > 0
        if not hasSelection then tImGui.BeginDisabled(true) end
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'),
            {r=0.55, g=0.1, b=0.1, a=1})
        if tImGui.Button(string.format(
                tLang.L('ltw_delete_selected'), win.selectedCount)
                .. '##ltwDel') then
            tImGui.OpenPopup('ltwConfirmDel##ltw')
        end
        tImGui.PopStyleColor(1)
        if not hasSelection then tImGui.EndDisabled() end

        -- ── Confirmation popup ────────────────────────────────────────────────
        local pFlags = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
        local popOpen, _ = tImGui.BeginPopupModal(
            'ltwConfirmDel##ltw', false, pFlags)
        if popOpen then
            tImGui.Text(string.format(
                tLang.L('ltw_confirm_msg'), win.selectedCount))
            tImGui.Separator()
            if tImGui.Button(tLang.L('ok') .. '##ltwOk', {x=120, y=0}) then
                local deleted = 0
                local i = 1
                while i <= #win.folderFiles do
                    local f = win.folderFiles[i]
                    if f.selected then
                        local ok, err = os.remove(f.path)
                        if ok then
                            deleted = deleted + 1
                            table.remove(win.folderFiles, i)
                        else
                            tUtil.showMessageWarn(string.format(
                                'Could not delete: %s',
                                err or f.name))
                            i = i + 1
                        end
                    else
                        i = i + 1
                    end
                end
                win.selectedCount = 0
                if deleted > 0 then
                    tUtil.showMessage(string.format(
                        tLang.L('ltw_delete_ok_fmt'), deleted))
                end
                tImGui.CloseCurrentPopup()
            end
            tImGui.SetItemDefaultFocus()
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel') .. '##ltwCancel',
                    {x=120, y=0}) then
                tImGui.CloseCurrentPopup()
            end
            tImGui.EndPopup()
        end
    end

    tImGui.End()
    if closed then
        win.open = false
    end
end

-- ─────────────────────────────────────────────────────────────────────────────
-- List Meshes window
-- ─────────────────────────────────────────────────────────────────────────────
local function buildListMeshesData()
    local win = tListMeshesWin
    win.loadedSet  = {}
    win.loadedList = {}
    for _, tE in ipairs(tLoadedMeshes) do
        local bn = tUtil.getBaseFileName(tE.fileName)
        if bn and bn ~= '' and not win.loadedSet[bn] then
            local ok, err = tE.meshDebug:check()
            win.loadedSet[bn] = { valid = ok, err = err }
            table.insert(win.loadedList, bn)
        end
    end
    table.sort(win.loadedList)

    win.folderFiles   = {}
    win.selectedCount = 0
    if win.folder ~= '' then
        local meshExts = { spt=true, msh=true, fnt=true, tile=true, ptl=true }
        local tFiles   = mbm.listFiles(win.folder, false)
        if tFiles then
            local sep = tFiles.separator or '/'
            for i = 1, #tFiles do
                local tDir = tFiles[i]
                for j = 1, #tDir do
                    local name = tDir[j]
                    local ext  = (name:match('%.([^%.]+)$') or ''):lower()
                    if meshExts[ext] then
                        local fullPath = tDir.path .. sep .. name
                        local entry = win.loadedSet[name]
                        table.insert(win.folderFiles, {
                            name     = name,
                            path     = fullPath,
                            inEditor = entry ~= nil,
                            valid    = entry == nil or entry.valid,
                            checkErr = entry ~= nil and entry.err or nil,
                            selected = false,
                        })
                    end
                end
            end
        end
        table.sort(win.folderFiles, function(a, b)
            return a.name:lower() < b.name:lower()
        end)
    end
    win.needRebuild = false
end

function showListMeshesWindow()
    local win = tListMeshesWin
    if not win.open then return end
    if win.needRebuild then buildListMeshesData() end

    local iW, iH = mbm.getSizeScreen()
    tImGui.SetNextWindowSize({x=620, y=540}, tImGui.Flags('ImGuiCond_Appearing'))
    tImGui.SetNextWindowPos({x=iW*0.5, y=iH*0.5},
        tImGui.Flags('ImGuiCond_Appearing'), {x=0.5, y=0.5})

    local is_open, closed = tImGui.Begin(
        tLang.L('list_meshes') .. '##lmwWin', true, 0)

    if is_open then
        -- ── Top panel: meshes loaded in editor ───────────────────────────────
        tImGui.Text(tLang.L('lmw_loaded_section')
            .. string.format(' (%d)', #win.loadedList))
        tImGui.SameLine()
        if tImGui.Button(tLang.L('lmw_refresh') .. '##lmwRefresh') then
            win.needRebuild = true
            buildListMeshesData()
        end
        local loadedText = #win.loadedList > 0
            and table.concat(win.loadedList, '\n')
            or  tLang.L('lmw_no_meshes')
        local flagsRO = tImGui.Flags('ImGuiInputTextFlags_ReadOnly')
        tImGui.InputTextMultiline('##lmwLoaded', loadedText, {x=-1, y=110}, flagsRO)

        tImGui.Separator()

        -- ── Folder selector ───────────────────────────────────────────────────
        tImGui.Text(tLang.L('lmw_folder_section'))
        tImGui.Spacing()
        tImGui.Text(tLang.L('lmw_folder_label'))
        tImGui.SameLine()
        local dispFolder = win.folder ~= '' and win.folder
                           or tLang.L('lmw_no_folder')
        tImGui.TextDisabled(dispFolder)
        if tImGui.IsItemHovered(0) and win.folder ~= '' then
            tImGui.BeginTooltip()
            tImGui.Text(win.folder)
            tImGui.EndTooltip()
        end
        if tImGui.Button(tLang.L('lmw_browse_folder') .. '##lmwBrowse') then
            local picked = mbm.openFolder(tLang.L('list_meshes'),
                win.folder ~= '' and win.folder or sLastFolderPath)
            if picked then
                win.folder = picked:gsub('\\', '/')
                sLastFolderPath = win.folder
                win.needRebuild = true
                buildListMeshesData()
            end
        end
        if win.folder ~= '' then
            local total   = #win.folderFiles
            local notInEd = 0
            local invalid = 0
            for _, f in ipairs(win.folderFiles) do
                if not f.inEditor then notInEd = notInEd + 1 end
                if f.inEditor and not f.valid then invalid = invalid + 1 end
            end
            tImGui.SameLine()
            tImGui.TextDisabled(string.format(
                tLang.L('lmw_unloaded_count_fmt'), total, notInEd, invalid))
        end

        tImGui.Spacing()

        -- ── File table ────────────────────────────────────────────────────────
        local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders',
                                      'ImGuiTableFlags_RowBg',
                                      'ImGuiTableFlags_ScrollY')
        if tImGui.BeginTable('lmwFiles', 3, tblFlags, {x=-1, y=-38}) then
            tImGui.TableSetupScrollFreeze(0, 1)
            tImGui.TableSetupColumn('##cb',
                tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 22)
            tImGui.TableSetupColumn('File')
            tImGui.TableSetupColumn('Status',
                tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 96)
            tImGui.TableHeadersRow()

            if #win.folderFiles == 0 then
                tImGui.TableNextRow()
                tImGui.TableNextColumn()
                tImGui.TableNextColumn()
                tImGui.TextDisabled(win.folder ~= ''
                    and tLang.L('lmw_no_files')
                    or  tLang.L('lmw_no_folder'))
            else
                for idx, f in ipairs(win.folderFiles) do
                    tImGui.TableNextRow()
                    tImGui.TableNextColumn()
                    local canDelete = not f.inEditor or not f.valid
                    if canDelete then
                        local newSel = tImGui.Checkbox('##lmwCb-' .. idx, f.selected)
                        if newSel ~= f.selected then
                            f.selected        = newSel
                            win.selectedCount = win.selectedCount
                                + (newSel and 1 or -1)
                        end
                    else
                        tImGui.BeginDisabled(true)
                        tImGui.Checkbox('##lmwCb-' .. idx, false)
                        tImGui.EndDisabled()
                    end

                    tImGui.TableNextColumn()
                    tImGui.Text(f.name)
                    if tImGui.IsItemHovered(0) then
                        tImGui.BeginTooltip()
                        tImGui.Text(f.path)
                        tImGui.EndTooltip()
                    end

                    tImGui.TableNextColumn()
                    if f.inEditor and f.valid then
                        tImGui.PushStyleColor(
                            tImGui.Flags('ImGuiCol_Text'),
                            {r=0.3, g=0.9, b=0.3, a=1})
                        tImGui.Text(tLang.L('lmw_status_in_editor'))
                        tImGui.PopStyleColor(1)
                    elseif f.inEditor and not f.valid then
                        tImGui.PushStyleColor(
                            tImGui.Flags('ImGuiCol_Text'),
                            {r=0.9, g=0.2, b=0.2, a=1})
                        tImGui.Text(tLang.L('lmw_status_invalid'))
                        tImGui.PopStyleColor(1)
                        if tImGui.IsItemHovered(0) and f.checkErr then
                            tImGui.BeginTooltip()
                            tImGui.Text(f.checkErr)
                            tImGui.EndTooltip()
                        end
                    else
                        tImGui.PushStyleColor(
                            tImGui.Flags('ImGuiCol_Text'),
                            {r=0.9, g=0.6, b=0.1, a=1})
                        tImGui.Text(tLang.L('lmw_status_not_in_editor'))
                        tImGui.PopStyleColor(1)
                    end
                end
            end
            tImGui.EndTable()
        end

        -- ── Bottom action bar ─────────────────────────────────────────────────
        if tImGui.Button(tLang.L('lmw_select_all') .. '##lmwSelAll') then
            win.selectedCount = 0
            for _, f in ipairs(win.folderFiles) do
                if not f.inEditor or not f.valid then
                    f.selected        = true
                    win.selectedCount = win.selectedCount + 1
                end
            end
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('lmw_select_none') .. '##lmwSelNone') then
            for _, f in ipairs(win.folderFiles) do
                f.selected = false
            end
            win.selectedCount = 0
        end
        tImGui.SameLine()
        local hasSelection = win.selectedCount > 0
        if not hasSelection then tImGui.BeginDisabled(true) end
        tImGui.PushStyleColor(tImGui.Flags('ImGuiCol_Button'),
            {r=0.55, g=0.1, b=0.1, a=1})
        if tImGui.Button(string.format(
                tLang.L('lmw_delete_selected'), win.selectedCount)
                .. '##lmwDel') then
            tImGui.OpenPopup('lmwConfirmDel##lmw')
        end
        tImGui.PopStyleColor(1)
        if not hasSelection then tImGui.EndDisabled() end

        -- ── Confirmation popup ────────────────────────────────────────────────
        local pFlags = tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize')
        local popOpen, _ = tImGui.BeginPopupModal(
            'lmwConfirmDel##lmw', false, pFlags)
        if popOpen then
            tImGui.Text(string.format(
                tLang.L('lmw_confirm_msg'), win.selectedCount))
            tImGui.Separator()
            if tImGui.Button(tLang.L('ok') .. '##lmwOk', {x=120, y=0}) then
                local deleted = 0
                local removedNames = {}
                local i = 1
                while i <= #win.folderFiles do
                    local f = win.folderFiles[i]
                    if f.selected then
                        local ok, err = os.remove(f.path)
                        if ok then
                            deleted = deleted + 1
                            if f.inEditor then
                                removedNames[f.name] = true
                            end
                            table.remove(win.folderFiles, i)
                        else
                            tUtil.showMessageWarn(string.format(
                                'Could not delete: %s',
                                err or f.name))
                            i = i + 1
                        end
                    else
                        i = i + 1
                    end
                end
                -- Remove deleted meshes from the editor (descending to keep indices valid)
                if next(removedNames) then
                    local toRemove = {}
                    for idx, tE in ipairs(tLoadedMeshes) do
                        local bn = tUtil.getBaseFileName(tE.fileName)
                        if removedNames[bn] then
                            table.insert(toRemove, idx)
                        end
                    end
                    for j = #toRemove, 1, -1 do
                        removeMeshFromTable(toRemove[j])
                    end
                end
                win.selectedCount = 0
                win.needRebuild   = true
                if deleted > 0 then
                    tUtil.showMessage(string.format(
                        tLang.L('lmw_delete_ok_fmt'), deleted))
                end
                tImGui.CloseCurrentPopup()
            end
            tImGui.SetItemDefaultFocus()
            tImGui.SameLine()
            if tImGui.Button(tLang.L('cancel') .. '##lmwCancel',
                    {x=120, y=0}) then
                tImGui.CloseCurrentPopup()
            end
            tImGui.EndPopup()
        end
    end

    tImGui.End()
    if closed then
        win.open = false
    end
end

function onLoop(delta)
    main_menu_mesh_debug()
    showMixamoGuideDialog()
    showBlenderImportDialog()
    showMeshExportOptionsDialog()
    showMeshExportBuildDialog()
    showCameraWindow()
    showLightWindow()
    showMeshTreeWindow()
    showArticulatedPivotWindow()
    sweepStaleBoneGizmos()
    showBonesWindow()
    showApplyAllWindow()
    showListTexturesWindow()
    showListMeshesWindow()
    updatePreviewMesh()
    updateCam3dKeyboardMovement(delta)
    -- Frame Pick popup (per loaded mesh)
    for i = 1, #tLoadedMeshes do
        local tE = tLoadedMeshes[i]
        if tE.bShowFramePick then
            showFramePickWindow(tE, tE.meshDebug, i)
        end
    end
    -- Auto-refresh frame-filter preview when any frame checkbox was toggled
    if iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
        local tE = tLoadedMeshes[iSelectedMeshIndex]
        if tE.bFrameSelectionDirty and tE.bAutoRefreshPreview and not tE.modified then
            refreshFrameFilterPreview(tE, iSelectedMeshIndex)
            tE.bFrameSelectionDirty = false
        end
    end
    tUtil.showOverlayMessage()
    if mbm.getGlobal('TEST_GHOST_MESH') then runGhostMeshTest() end
end

-- TEMP TEST HOOK -- ghost-mesh feature verification, remove before shipping.
function runGhostMeshTest()
    gTestStep = gTestStep or 0
    local function log(s) print('TESTLOG', 'yellow', s) end
    if gTestStep == 0 then
        mbm.addPath('tests')
        addMeshToTable('tests/mike-rig-from-mixamo.msh')
        addMeshToTable('tests/mike-rig-from-mixamo.msh')
        iSelectedMeshIndex = 1
        log('loaded=' .. tostring(#tLoadedMeshes))
        gTestStep = 1
    elseif gTestStep == 1 then
        gTestStep = 2 -- let updatePreviewMesh create tPreviewMesh
    elseif gTestStep == 2 then
        local tEntry = tLoadedMeshes[1]
        tEntry.sOpenNode = 'bones'
        tEntry.bShowGhostMesh = true
        gTestStep = 3
    elseif gTestStep == 3 then
        gTestStep = 4 -- let showBonesNode's transition create the ghost this frame
    elseif gTestStep == 4 then
        log('after-enable tGhostMesh=' .. tostring(tGhostMesh ~= nil))
        if tGhostMesh then
            local okSh, fx = pcall(function() return tGhostMesh:getShader() end)
            log('getShader ok=' .. tostring(okSh) .. ' fx=' .. tostring(fx ~= nil))
            if okSh and fx then
                local okA, a = pcall(function() return fx:getPS('alpha') end)
                log('alpha ok=' .. tostring(okA) .. ' value=' .. tostring(a) .. ' expected=' .. tostring(1.0 - 0.35))
            end
        end
        -- Close the tree node -- ghost must be destroyed
        tLoadedMeshes[1].sOpenNode = nil
        gTestStep = 5
    elseif gTestStep == 5 then
        gTestStep = 6
    elseif gTestStep == 6 then
        log('after-close tGhostMesh=' .. tostring(tGhostMesh ~= nil) .. ' (expect false)')
        -- Reopen, re-show, then switch selected mesh -- sweepStaleBoneGizmos must destroy it
        tLoadedMeshes[1].sOpenNode = 'bones'
        tLoadedMeshes[1].bShowGhostMesh = true
        gTestStep = 7
    elseif gTestStep == 7 then
        gTestStep = 8
    elseif gTestStep == 8 then
        log('reopened tGhostMesh=' .. tostring(tGhostMesh ~= nil) .. ' (expect true)')
        iSelectedMeshIndex = 2 -- switch selection while mesh1's Bones node stays open
        gTestStep = 9
    elseif gTestStep == 9 then
        gTestStep = 10 -- let sweepStaleBoneGizmos (called at top of onLoop) run with new selection
    elseif gTestStep == 10 then
        log('after-switch tGhostMesh=' .. tostring(tGhostMesh ~= nil) .. ' (expect false) bGhostWasShown=' .. tostring(tLoadedMeshes[1].bGhostWasShown))
        -- Now test removeMeshFromTable cleanup: reselect mesh1, show ghost, then remove it
        iSelectedMeshIndex = 1
        tLoadedMeshes[1].sOpenNode = 'bones'
        tLoadedMeshes[1].bShowGhostMesh = true
        gTestStep = 11
    elseif gTestStep == 11 then
        gTestStep = 12
    elseif gTestStep == 12 then
        log('before-remove tGhostMesh=' .. tostring(tGhostMesh ~= nil) .. ' (expect true)')
        removeMeshFromTable(1)
        log('after-remove tGhostMesh=' .. tostring(tGhostMesh ~= nil) .. ' (expect false)')
        print('info', 'green', 'GHOSTMESH TEST DONE')
        mbm.quit()
    end
end

function onTouchDown(key, x, y)
    if not tImGui.IsAnyWindowHovered() then
        if (key == 0 or key == 1 or key == 2) and iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
            local tEntry = tLoadedMeshes[iSelectedMeshIndex]
            local xf = tEntry.tXformUI
            if tEntry.sOpenNode == 'transform' and tEntry.tXformPreviewMesh and
                    xf and xf.enableSubsetDrag and not tEntry.bXformSubsetFilterActive and
                    xf.frame > 0 and xf.subset > 0 then
                if key == 0 and bCameraMode3D then
                    tEntry.bXformOrbiting = true
                    camera2d.mx, camera2d.my = x, y
                    return
                end
                if key == 0 then
                    isClickedMouseleft = true
                    camera2d.mx, camera2d.my = x, y
                    return
                end
                if key == 2 then
                    tEntry.tXformSubsetDrag = {
                        mode='rotate', startScreenX=x, startScreenY=y,
                        initialRx=xf.rx, initialRy=xf.ry, initialRz=xf.rz,
                        deltaRx=0, deltaRy=0, deltaRz=0,
                    }
                    camera2d.mx, camera2d.my = x, y
                    return
                end
                -- In 2D there is no orbit camera, so preserve the editor's normal left-button
                -- viewport behavior. Right-button dragging remains the subset translation action.
                local planePoint
                local aabb = computeMeshAABB(tEntry.meshDebug, xf.frame, xf.subset)
                if aabb then
                    planePoint = {
                        x = (aabb.minX + aabb.maxX) * 0.5 + xf.dx,
                        y = (aabb.minY + aabb.maxY) * 0.5 + xf.dy,
                        z = (aabb.minZ + aabb.maxZ) * 0.5 + xf.dz,
                    }
                else
                    planePoint = {x=xf.dx, y=xf.dy, z=xf.dz}
                end
                local planeNormal = {x=0, y=0, z=1}
                if bCameraMode3D then
                    local px, py, pz = cam3dGetPos(tEntry.cam3d)
                    local nx, ny, nz = tEntry.cam3d.fx - px, tEntry.cam3d.fy - py, tEntry.cam3d.fz - pz
                    local length = math.sqrt(nx*nx + ny*ny + nz*nz)
                    if length > 0 then planeNormal = {x=nx/length, y=ny/length, z=nz/length} end
                end
                local wx, wy, wz = rayPlaneHit(x, y, planePoint, planeNormal)
                if wx then
                    tEntry.tXformSubsetDrag = {
                        mode='translate',
                        planePoint=planePoint, planeNormal=planeNormal,
                        startX=wx, startY=wy, startZ=wz,
                        initialDx=xf.dx, initialDy=xf.dy, initialDz=xf.dz,
                        deltaX=0, deltaY=0, deltaZ=0,
                    }
                    camera2d.mx, camera2d.my = x, y
                    return
                end
            end
        end
        if key == 0 and bCameraMode3D and iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
            local tEntry = tLoadedMeshes[iSelectedMeshIndex]
            local sp = tEntry.tSplitCapture
            if sp and sp.active and sp.aabbMin then
                local okRay, ox, oy, oz, dx, dy, dz = pcall(mbm.getPickRay, x, y)
                if okRay and splitCaptureRayHitsAABB(ox, oy, oz, dx, dy, dz,
                        sp.aabbMin.x, sp.aabbMin.y, sp.aabbMin.z,
                        sp.aabbMax.x, sp.aabbMax.y, sp.aabbMax.z) then
                    local px, py, pz = cam3dGetPos(tEntry.cam3d)
                    local nx, ny, nz = tEntry.cam3d.fx - px, tEntry.cam3d.fy - py, tEntry.cam3d.fz - pz
                    local length = math.sqrt(nx*nx + ny*ny + nz*nz)
                    if length > 0 then nx, ny, nz = nx/length, ny/length, nz/length end
                    sp.dragPlaneNormal = {x=nx,y=ny,z=nz}
                    sp.dragPlanePoint = {x=sp.x,y=sp.y,z=sp.z}
                    sp.dragOffset = {x=sp.x,y=sp.y,z=sp.z}
                    local wx, wy, wz = rayPlaneHit(x, y, sp.dragPlanePoint, sp.dragPlaneNormal)
                    if wx then sp.dragOffset = {x=sp.x-wx,y=sp.y-wy,z=sp.z-wz} end
                    tEntry.sSplitDragging = true
                    camera2d.mx, camera2d.my = x, y
                    return
                end
            end
        end
        -- Axis-locked bone drag/drop takes priority over ordinary orbit, but only when a drag
        -- plane is actually checked -- otherwise every ordinary orbit-click would pay for a
        -- hit-test against bone spheres that aren't even a relevant target. Same
        -- hit-test-first-else-orbit structure physic_editor.lua's own 3D handle drag uses.
        if key == 0 and bCameraMode3D and iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
            local tEntry = tLoadedMeshes[iSelectedMeshIndex]
            if tEntry.sBoneDragPlane then
                local name, hx, hy, hz = hitTestBoneSpheres3d(tEntry, x, y)
                if name then
                    tEntry.sDraggingBoneName = name
                    tEntry.tDragPlaneNormal  = (tEntry.sBoneDragPlane == 'xy') and {x=0,y=0,z=1} or {x=1,y=0,z=0}
                    tEntry.tDragPlanePoint   = {x=hx, y=hy, z=hz}
                    camera2d.mx = x
                    camera2d.my = y
                    return -- don't also start an orbit for this click
                end
            end
        end
        isClickedMouseleft  = (key == 0)
        isClickedMouseRight = (key == 1)
        camera2d.mx = x
        camera2d.my = y
    end
end

function onTouchMove(key, x, y)
    if tImGui.IsAnyWindowHovered() then return end
    if iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
        local tDragEntry = tLoadedMeshes[iSelectedMeshIndex]
        if tDragEntry.bXformOrbiting and bCameraMode3D then
            local c = tDragEntry.cam3d
            c.azimuth   = c.azimuth   - (x - camera2d.mx) * 0.005
            c.elevation = c.elevation + (y - camera2d.my) * 0.005
            c.elevation = math.max(-math.pi * 0.49, math.min(math.pi * 0.49, c.elevation))
            camera2d.mx, camera2d.my = x, y
            if tPreviewMesh then applyCam3d(c) end
            return
        end
        local xfDrag = tDragEntry.tXformSubsetDrag
        if xfDrag then
            if xfDrag.mode == 'rotate' then
                if bCameraMode3D then
                    xfDrag.deltaRx = (y - xfDrag.startScreenY) * 0.5
                    xfDrag.deltaRy = (x - xfDrag.startScreenX) * 0.5
                    xfDrag.deltaRz = 0
                else
                    xfDrag.deltaRx, xfDrag.deltaRy = 0, 0
                    xfDrag.deltaRz = (x - xfDrag.startScreenX) * 0.5
                end
                if tDragEntry.tXformPreviewMesh then
                    tDragEntry.tXformPreviewMesh:setAngle(math.rad(xfDrag.deltaRx), math.rad(xfDrag.deltaRy), math.rad(xfDrag.deltaRz))
                end
            else
                local wx, wy, wz = rayPlaneHit(x, y, xfDrag.planePoint, xfDrag.planeNormal)
                if wx then
                    xfDrag.deltaX = wx - xfDrag.startX
                    xfDrag.deltaY = wy - xfDrag.startY
                    xfDrag.deltaZ = wz - xfDrag.startZ
                    -- The drag preview contains only the selected subset, so moving its render
                    -- object is equivalent to moving that subset and does not touch vertex data.
                    if tDragEntry.tXformPreviewMesh then
                        tDragEntry.tXformPreviewMesh:setPos(xfDrag.deltaX, xfDrag.deltaY, xfDrag.deltaZ)
                    end
                end
            end
            camera2d.mx, camera2d.my = x, y
            return
        end
        local sp = tDragEntry.tSplitCapture
        if tDragEntry.sSplitDragging and sp and sp.active then
            local wx, wy, wz = rayPlaneHit(x, y, sp.dragPlanePoint, sp.dragPlaneNormal)
            if wx then
                sp.x, sp.y, sp.z = wx + sp.dragOffset.x, wy + sp.dragOffset.y, wz + sp.dragOffset.z
                splitCaptureMoveBox(sp)
            end
            camera2d.mx, camera2d.my = x, y
            return
        end
        if tDragEntry.sDraggingBoneName then
            local wx, wy, wz = rayPlaneHit(x, y, tDragEntry.tDragPlanePoint, tDragEntry.tDragPlaneNormal)
            if wx then
                local meshD = tDragEntry.meshDebug
                local tBones = getBoneList(meshD)
                for _, b in ipairs(tBones) do
                    if b.name == tDragEntry.sDraggingBoneName then
                        local okU = updateBonePosition(meshD, b, wx, wy, wz)
                        if okU and tDragEntry.bSyncLeftRightBoneDrag then
                            local oppositeName = getOppositeSideBoneName(b.name)
                            local oppositeBone = oppositeName and findBoneByName(tBones, oppositeName) or nil
                            if oppositeBone then
                                if tDragEntry.sBoneDragPlane == 'xy' then
                                    okU = updateBonePosition(meshD, oppositeBone, -wx, wy, oppositeBone.z)
                                else
                                    okU = updateBonePosition(meshD, oppositeBone, oppositeBone.x, wy, wz)
                                end
                            end
                        end
                        if okU then onBonesEdit(tDragEntry, meshD, iSelectedMeshIndex) end
                        break
                    end
                end
                tDragEntry.tDragPlanePoint = {x=wx, y=wy, z=wz}
            end
            camera2d.mx = x
            camera2d.my = y
            return
        elseif tDragEntry.sBoneDragPlane and bCameraMode3D and not isClickedMouseleft and not isClickedMouseRight then
            -- Hover preview (direct user request): while a drag plane is checked and the mouse
            -- isn't held for orbit/pan, continuously hit-test bone spheres and highlight (yellow,
            -- reusing the existing Highlight checkbox/color mechanism) whichever one is currently
            -- under the cursor -- so the user can see which joint a click would grab before
            -- committing. Only rebuilds the gizmo when the hovered bone actually changes, not on
            -- every single mouse-move frame while hovering the same one.
            local name = hitTestBoneSpheres3d(tDragEntry, x, y)
            if name ~= tDragEntry.sHoveredBoneName then
                tDragEntry.tBoneHighlight = {}
                if name then tDragEntry.tBoneHighlight[name] = true end
                tDragEntry.sHoveredBoneName = name
                rebuildBoneGizmo(tDragEntry, tDragEntry.meshDebug, iSelectedMeshIndex)
            end
        end
    end
    if bCameraMode3D and iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
        local tEntry = tLoadedMeshes[iSelectedMeshIndex]
        local c = tEntry.cam3d
        if isClickedMouseleft then
            -- Orbit: rotate around focus point
            c.azimuth   = c.azimuth   - (x - camera2d.mx) * 0.005
            c.elevation = c.elevation + (y - camera2d.my) * 0.005
            c.elevation = math.max(-math.pi * 0.49, math.min(math.pi * 0.49, c.elevation))
        elseif isClickedMouseRight then
            -- Pan: translate focus in the camera right+up plane
            local px, py, pz = cam3dGetPos(c)
            local vx, vy, vz = c.fx - px, c.fy - py, c.fz - pz
            local vlen = math.sqrt(vx*vx + vy*vy + vz*vz)
            if vlen > 0 then vx, vy, vz = vx/vlen, vy/vlen, vz/vlen end
            -- right = normalize(cross(view, worldUp=(0,1,0))) = (vz, 0, -vx)
            local rx, rz = vz, -vx
            local rlen = math.sqrt(rx*rx + rz*rz)
            if rlen > 0 then rx, rz = rx/rlen, rz/rlen end
            local scale = c.distance * 0.001
            c.fx = c.fx + rx * (camera2d.mx - x) * scale
            c.fy = c.fy + (y - camera2d.my) * scale
            c.fz = c.fz + rz * (camera2d.mx - x) * scale
        end
        camera2d.mx = x
        camera2d.my = y
        if tPreviewMesh then applyCam3d(c) end
    elseif isClickedMouseleft then
        local px = (camera2d.mx - x) * camera2d.sx
        local py = (camera2d.my - y) * camera2d.sy
        camera2d.mx = x
        camera2d.my = y
        camera2d:setPos(camera2d.x + px, camera2d.y - py)
    end
end

function onTouchUp(key, x, y)
    isClickedMouseleft  = false
    isClickedMouseRight = false
    camera2d.mx = x
    camera2d.my = y
    if iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
        local tEntry = tLoadedMeshes[iSelectedMeshIndex]
        tEntry.bXformOrbiting = nil
        local xfDrag = tEntry.tXformSubsetDrag
        if xfDrag then
            local xf = tEntry.tXformUI
            if xfDrag.mode == 'rotate' then
                xf.rx = xfDrag.initialRx + xfDrag.deltaRx
                xf.ry = xfDrag.initialRy + xfDrag.deltaRy
                xf.rz = xfDrag.initialRz + xfDrag.deltaRz
            else
                xf.dx = xfDrag.initialDx + xfDrag.deltaX
                xf.dy = xfDrag.initialDy + xfDrag.deltaY
                xf.dz = xfDrag.initialDz + xfDrag.deltaZ
            end
            tEntry.tXformSubsetDrag = nil
            if xf.autoPreview then
                tEntry.xfLastPreviewFP = string.format('%d|%d|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f|%.3f',
                    xf.frame, xf.subset, xf.rx, xf.ry, xf.rz, xf.sx, xf.sy, xf.sz, xf.dx, xf.dy, xf.dz)
            end
            -- Bake the final translation/rotation into the temporary clone once, on drop. No mesh
            -- reconstruction occurs while the mouse is moving.
            if tEntry.fnBuildXformPreview then tEntry.fnBuildXformPreview() end
        end
        tEntry.tXformSubsetDrag = nil
        tEntry.sSplitDragging = nil
        if tEntry.tSplitCapture then
            tEntry.tSplitCapture.dragPlanePoint = nil
            tEntry.tSplitCapture.dragPlaneNormal = nil
            tEntry.tSplitCapture.dragOffset = nil
        end
        -- Nothing to "commit" here -- onTouchMove's live updateBone/onBonesEdit already wrote
        -- every change as it happened. Just clear the drag state.
        tEntry.sDraggingBoneName = nil
        tEntry.tDragPlanePoint   = nil
        tEntry.tDragPlaneNormal  = nil
    end
end

function onTouchZoom(zoom)
    if tImGui.IsAnyWindowHovered() then return end
    if bCameraMode3D and iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
        local c = tLoadedMeshes[iSelectedMeshIndex].cam3d
        c.distance = c.distance * (1.0 - zoom * 0.15)
        c.distance = math.max(1, c.distance)
        if tPreviewMesh then applyCam3d(c) end
    elseif tPreviewMesh then
        local s = zoom * 0.2
        tPreviewMesh.sx = (tPreviewMesh.sx or 1) + s
        if (tPreviewMesh.sx or 1) < 0.2 then tPreviewMesh.sx = 0.2 end
        tPreviewMesh.sy = tPreviewMesh.sx
        tPreviewMesh.sz = tPreviewMesh.sx
    end
end

function onKeyDown(key)
    if mbm.getKeyName(key) == 'DOWN' then
        selectMeshIndex(iSelectedMeshIndex + 1)
    elseif mbm.getKeyName(key) == 'UP' then
        selectMeshIndex(iSelectedMeshIndex - 1)
    elseif key == mbm.getKeyCode('W') then
        tCam3dMove.forward = 1
    elseif key == mbm.getKeyCode('S') then
        tCam3dMove.forward = -1
    elseif key == mbm.getKeyCode('A') then
        tCam3dMove.right = -1
    elseif key == mbm.getKeyCode('D') then
        tCam3dMove.right = 1
    elseif key == mbm.getKeyCode('pageup') then
        tCam3dMove.vertical = 1
    elseif key == mbm.getKeyCode('pagedown') then
        tCam3dMove.vertical = -1
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('W') or key == mbm.getKeyCode('S') then
        tCam3dMove.forward = 0
    elseif key == mbm.getKeyCode('A') or key == mbm.getKeyCode('D') then
        tCam3dMove.right = 0
    elseif key == mbm.getKeyCode('pageup') or key == mbm.getKeyCode('pagedown') then
        tCam3dMove.vertical = 0
    end
end
