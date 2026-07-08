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
    isClickedMouseleft   = false
    isClickedMouseRight  = false
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
        tRunResults = {},
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
        tImGui.TextDisabled(string.format('%s: x=%.3f y=%.3f z=%.3f', tLang.L('direction_label'),
            lightState.directionalDirection.x, lightState.directionalDirection.y, lightState.directionalDirection.z))
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
    return os.getenv("TMPDIR") or os.getenv("TEMP") or os.getenv("TMP") or "/tmp"
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
    local ax = tonumber(options.importAngleX or 0) or 0
    local ay = tonumber(options.importAngleY or 0) or 0
    local az = tonumber(options.importAngleZ or 0) or 0
    if ax ~= 0 or ay ~= 0 or az ~= 0 then
        meshD:setAngle(ax, ay, az)
        blenderDebugPrint(tBlenderImportState, 'applied import rotation: %.3f %.3f %.3f', ax, ay, az)
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
    local initialH = math.min(560, maxH)
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
        sNormalVizCoordType  = nil
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
        {verts = {B, C, G, F}, color = '#FFFF0000'}, -- +X red
        {verts = {A, E, H, D}, color = '#FF00FFFF'}, -- -X cyan
        {verts = {D, H, G, C}, color = '#FF00FF00'}, -- +Y green
        {verts = {A, B, F, E}, color = '#FFFF00FF'}, -- -Y magenta
        {verts = {E, F, G, H}, color = '#FF0000FF'}, -- +Z blue
        {verts = {A, D, C, B}, color = '#FFFFFF00'}, -- -Z yellow
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

    local mshPath = 'src/test-lib/colored_cube.msh'
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
    local removed = table.remove(tLoadedMeshes, index)
    if removed then destroyNormalVisualization(removed) end
    if iSelectedMeshIndex == index then
        iSelectedMeshIndex = 0
        iLastPreviewedIndex = 0
        destroyPreviewMesh()
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
end

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

    for s = 1, nSubsets do
        local okV, nV = dpCall(function() return meshD:getTotalVertex(1, s) end)
        if okV and nV and nV > 0 then
            local okVerts, verts = dpCall(function() return meshD:getVertex(1, s, 1, nV) end)
            if okVerts and verts then
                local geo = triOk and computeGeoNormalsForSubset(meshD, 1, s) or nil
                for ii = 1, nV do
                    local vd = verts[ii]
                    local nx, ny, nz = normalizeVec3(vd.nx, vd.ny, vd.nz)
                    if nx then
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
                    end
                end
            end
        end
    end

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

function showMeshInfoTable(tEntry, index)
    local meshD = tEntry.meshDebug
    local info = tEntry.info or {}
    local step, stepFast, fmt = 0.01, 0.1, '%.3f'
    local flags = 0

    local function onEdit()
        tEntry.modified = true
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

    -- Editable: Default angle
    tImGui.Text(tLang.L("default_angle_xyz"))
    do
        local ok, ang = dpCall(function() return meshD:getAngle() end)
        if ok and ang then
            local v = {ang.x or 0, ang.y or 0, ang.z or 0}
            local r1, n1 = tImGui.InputFloat('##angX-' .. index, v[1], step, stepFast, fmt, flags)
            local r2, n2 = tImGui.InputFloat('##angY-' .. index, v[2], step, stepFast, fmt, flags)
            local r3, n3 = tImGui.InputFloat('##angZ-' .. index, v[3], step, stepFast, fmt, flags)
            if (r1 or r2 or r3) then
                local okSet = dpCall(function() meshD:setAngle(n1 or v[1], n2 or v[2], n3 or v[3]) end)
                if okSet then onEdit() end
            end
        end
    end

    -- Editable: Default position
    tImGui.Text(tLang.L("default_position_xyz"))
    do
        local ok, pos = dpCall(function() return meshD:getPosition() end)
        if ok and pos then
            local v = {pos.x or 0, pos.y or 0, pos.z or 0}
            local r1, n1 = tImGui.InputFloat('##posX-' .. index, v[1], step, stepFast, fmt, flags)
            local r2, n2 = tImGui.InputFloat('##posY-' .. index, v[2], step, stepFast, fmt, flags)
            local r3, n3 = tImGui.InputFloat('##posZ-' .. index, v[3], step, stepFast, fmt, flags)
            if (r1 or r2 or r3) then
                local okSet = dpCall(function() meshD:setPosition(n1 or v[1], n2 or v[2], n3 or v[3]) end)
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
    if not isOpen then return end

    local okTF, nFrames = dpCall(function() return meshD:getTotalFrame() end)
    if not okTF then nFrames = 0 end
    nFrames = nFrames or 0

    -- Build full subset list (all frames, all subsets)
    local allSubsets = {}
    for f = 1, nFrames do
        local okS, nSubs = dpCall(function() return meshD:getTotalSubset(f) end)
        for s = 1, (okS and nSubs or 0) do
            local okT, tex = dpCall(function() return meshD:getTexture(f, s) end)
            local texName = (okT and tex and tex ~= '') and (' [' .. tUtil.getShortName(tex) .. ']') or ''
            table.insert(allSubsets, {f=f, s=s, texName=texName})
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

    -- Precompute: which frames have at least one subset staged for removal
    local tImplicit = {}
    for _, sub2 in ipairs(allSubsets) do
        if tEntry.tCheckedRemove[sub2.f * 100 + sub2.s] then
            tImplicit[sub2.f] = true
        end
    end

    if tImGui.BeginTable('fnOuter-' .. index, 2, tblFlags, {x=0, y=listH}) then
        tImGui.TableSetupScrollFreeze(0, 1)
        tImGui.TableSetupColumn(tLang.L('frame_node'), tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 0.4)
        tImGui.TableSetupColumn(tLang.L('subsets'),   tImGui.Flags('ImGuiTableColumnFlags_WidthStretch'), 0.6)
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

            -- ── Additional rows: blank left, one more subset on right ────
            for i = 2, #fSubsets do
                tImGui.TableNextRow()
                tImGui.TableSetColumnIndex(1)
                renderSubsetCell(fSubsets[i])
            end

            -- ── Thin separator row between frame groups ──────────────────
            if f < nFrames then
                tImGui.TableNextRow()
                tImGui.TableSetColumnIndex(0)
                tImGui.Separator()
                tImGui.TableSetColumnIndex(1)
                tImGui.Separator()
            end
        end

        tImGui.EndTable()
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
        tImGui.Separator()
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

    -- Execute and Clear (when pending ops exist)
    if #tEntry.tPendingOps > 0 then
        tImGui.SameLine()
        if tImGui.Button(tLang.L('execute_ops') .. '##fnex-' .. index) then
            executeFrameOps(tEntry, meshD, index)
        end
        tImGui.SameLine()
        if tImGui.Button('X##fnclr-' .. index) then
            tEntry.tPendingOps = {}
        end
    end
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
function showNormalVertexRow(tEntry, meshD, index, s, v, geo, triOk)
    local okVd, vd = dpCall(function() return meshD:getVertex(1, s, v) end)
    if not okVd or not vd then return end
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
    local okNV, nV = dpCall(function() return meshD:getTotalVertex(1, s) end)
    if not okNV or not nV or nV <= 0 then return end

    local label = string.format('%s %d (%d)', tLang.L('normal_subset_label'), s, nV)
    if not tImGui.TreeNodeEx(label .. '##nsub-' .. index .. '-' .. s, 0) then return end

    local geo = triOk and computeGeoNormalsForSubset(meshD, 1, s) or {}

    local function bulkUpdate(fn)
        for v = 1, nV do
            local okVd, vd = dpCall(function() return meshD:getVertex(1, s, v) end)
            if okVd and vd then
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

    -- Kept intentionally narrow (Idx + NX/NY/NZ + one combined Status/Actions column) so the
    -- whole table fits inside the Mesh Tree window's default ~350px width; ScrollX is a safety
    -- net if the window is narrower still or a translation makes the status text wider.
    local tblFlags = tImGui.Flags('ImGuiTableFlags_Borders', 'ImGuiTableFlags_ScrollY',
        'ImGuiTableFlags_ScrollX', 'ImGuiTableFlags_RowBg')
    local listH = math.min(nV * 24 + 30, 260)
    if tImGui.BeginTable('nsubTbl-' .. index .. '-' .. s, 5, tblFlags, {x = 0, y = listH}) then
        tImGui.TableSetupScrollFreeze(0, 1)
        tImGui.TableSetupColumn(tLang.L('normal_col_idx'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 30)
        tImGui.TableSetupColumn('NX', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 56)
        tImGui.TableSetupColumn('NY', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 56)
        tImGui.TableSetupColumn('NZ', tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 56)
        tImGui.TableSetupColumn(tLang.L('normal_col_status'), tImGui.Flags('ImGuiTableColumnFlags_WidthFixed'), 110)
        tImGui.TableHeadersRow()
        for v = 1, nV do
            showNormalVertexRow(tEntry, meshD, index, s, v, geo, triOk)
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

    -- Auto-cancel transform preview when the Transform node is closed
    if tEntry.tXformPreviewMesh and tEntry.sOpenNode ~= 'transform' then
        tEntry.tXformPreviewMesh:destroy()
        tEntry.tXformPreviewMesh = nil
        local xfHide = tEntry.tXformUI and tEntry.tXformUI.hideOriginal
        if xfHide and tPreviewMesh and index == iSelectedMeshIndex then tPreviewMesh.visible = true end
    end

    if openNode(tEntry, 'transform', tLang.L("transform"), 0, 'transform-' .. index) then
        if tImGui.Button(tLang.L("centralize") .. '##' .. index) then
            meshD:centralize()
            tEntry.modified = true
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
            tUtil.showMessage(string.format('Centralized: %s', shortName))
        end

        -- Rotate/Scale/Translate with per-frame/per-subset targeting
        tEntry.tXformUI = tEntry.tXformUI or { frame=0, subset=0, rx=0, ry=0, rz=0, sx=1, sy=1, sz=1, dx=0, dy=0, dz=0, hideOriginal=false, autoPreview=false }
        local xf = tEntry.tXformUI
        xf.subset = xf.subset or 0
        xf.dx = xf.dx or 0; xf.dy = xf.dy or 0; xf.dz = xf.dz or 0
        if xf.hideOriginal == nil then xf.hideOriginal = false end
        if xf.autoPreview == nil then xf.autoPreview = false end
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
            if xf.hideOriginal and tPreviewMesh and index == iSelectedMeshIndex then tPreviewMesh.visible = true end
        end

        tImGui.Separator()
        tImGui.Text(tLang.L("target_frame_label"))
        local _, nf = tImGui.InputInt('##xfFrame-' .. index, xf.frame, 1, 1, 0)
        if nf ~= nil then
            nf = math.max(0, math.min(nf, totalFrames))
            if nf ~= xf.frame then cancelXformPreview() end
            xf.frame = nf
        end
        tImGui.Text(tLang.L("target_subset_label"))
        local _, ns = tImGui.InputInt('##xfSubset-' .. index, xf.subset, 1, 1, 0)
        if ns ~= nil then
            ns = math.max(0, math.min(ns, totalSubsets))
            if ns ~= xf.subset then cancelXformPreview() end
            xf.subset = ns
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
                cancelXformPreview()
                onEdit()
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
                cancelXformPreview()
                onEdit()
                local target = xf.frame == 0 and 'all frames' or ('frame ' .. xf.frame)
                tUtil.showMessage(string.format(tLang.L("scale_applied_fmt"), target))
                xf.sx = 1; xf.sy = 1; xf.sz = 1
            end
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
                cancelXformPreview()
                onEdit()
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
                tPreviewMesh.visible = not newHide
            end
        end
        xf.autoPreview = tImGui.Checkbox(tLang.L("auto_preview_transform") .. '##xfAuto-' .. index, xf.autoPreview)

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
                            cloneRender:setColor(255, 220, 50, 200)
                            dpCall(function() cloneRender:setAnim(tEntry.iSelectedAnim or 1) end)
                            tEntry.tXformPreviewMesh = cloneRender
                            if xf.hideOriginal and tPreviewMesh and index == iSelectedMeshIndex then
                                tPreviewMesh.visible = false
                            end
                        else
                            if cloneRender then cloneRender:destroy() end
                            tUtil.showMessageWarn('Failed to create transform preview')
                        end
                    end
                end
            end
        end

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
                end
            end
            tImGui.PopStyleColor(1)
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
            tImGui.Text(tLang.L("tex_role_label"))
            local roleRet, newRoleIndex = tImGui.Combo('##txRole-' .. index, roleIndex, roleOpts, -1)
            if roleRet and newRoleIndex then
                tx.role = roleValues[newRoleIndex] or 'primary'
            end

            -- Show current texture when a specific frame+subset is selected
            if tx.frame > 0 and tx.subset > 0 then
                local okG, curTex
                if tx.role == 'primary' then
                    okG, curTex = dpCall(function() return meshD:getTexture(tx.frame, tx.subset) end)
                else
                    okG, curTex = dpCall(function() return meshD:getMaterialTexture(tx.frame, tx.subset, tx.role) end)
                end
                if okG then
                    tImGui.Text(tLang.L("tex_current_label"))
                    tImGui.SameLine()
                    if curTex and curTex ~= '' then
                        tImGui.TextDisabled(tUtil.getShortName(curTex))
                        if tImGui.IsItemHovered(0) then
                            tImGui.BeginTooltip()
                            tImGui.Text(curTex)
                            tImGui.EndTooltip()
                        end
                    else
                        tImGui.TextDisabled('(none)')
                    end
                end
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
    if openNode(tEntry, 'anims', tLang.L("animations") .. (nAnim and nAnim > 0 and (' (' .. nAnim .. ')') or ''), 0, 'anims-' .. index) then
        -- Frame-filter preview refresh controls (only for the currently selected mesh)
        if index == iSelectedMeshIndex then
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
                local changed, newIdx = tImGui.Combo(tLang.L("animation") .. '##animSel-' .. index, tEntry.iSelectedAnim, tAnimNames, -1)
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
                        local mod, newName = tImGui.InputText('##animName-' .. index .. '-' .. i, name or '', flags)
                        tImGui.Text(tLang.L("initial_frame"))
                        local ri, ni = tImGui.InputInt('##animInit-' .. index .. '-' .. i, initF or 1, 1, 1, flags)
                        tImGui.Text(tLang.L("final_frame"))
                        local rf, nf = tImGui.InputInt('##animFin-' .. index .. '-' .. i, finF or 1, 1, 1, flags)
                        tImGui.Text(tLang.L("time_between_frames_anim"))
                        local rt, nt = tImGui.InputFloat('##animTime-' .. index .. '-' .. i, time or 0.1, 0.01, 0.1, '%.3f', flags)
                        tImGui.Text(tLang.L("type_label"))
                        local typIdx = math.max(1, math.min((typ or 0) + 1, #tAnimTypeOpts))
                        local rty, newTypIdx = tImGui.Combo('##animType-' .. index .. '-' .. i, typIdx, tAnimTypeOpts, -1)
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
                        local modFxF, newFxF = tImGui.InputText('##animFxFile-' .. index .. '-' .. i, fxFn, 512, 0)
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

function doSaveAs(tEntry, index)
    local info     = tEntry.info or {}
    local meshD    = tEntry.meshDebug
    local shortName = tUtil.getShortName(tEntry.fileName)
    local extMap   = { mesh = 'msh', sprite = 'spt', font = 'fnt', tile = 'tile', particle = 'ptl' }
    local suggestedExt = extMap[info.type] or 'msh'

    local newFile = mbm.saveFile(sLastMeshPath, suggestedExt)
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
    end

    local ok = false
    local wasLegacy = isLegacyTextureAnimationEffectStorage(tEntry.info)
    if not hasDeselected then
        -- All frames selected: simple save
        ok = meshD:save(newFile, false, false)
    else
        local tempD = buildFilteredMesh(tEntry)
        if not tempD then
            tUtil.showMessageWarn('Nothing to save (all frames deselected)')
            return
        end
        ok = tempD:save(newFile, false, false)
    end

    if ok then
        local newInfo = refreshEntryInfoFromFile(nil, newFile)
        if wasLegacy and not isLegacyTextureAnimationEffectStorage(newInfo) then
            tUtil.showMessage(string.format(tLang.L('mesh_migrated_save_fmt'), tUtil.getShortName(newFile)))
        else
            tUtil.showMessage(string.format(tLang.L("save_as_success_fmt"), tUtil.getShortName(newFile)))
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

local function buildFilteredMeshForSave(tEntry)
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
    local success = 0
    local failed = 0
    local migrated = 0
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
    if failed > 0 then
        msg = msg .. '\n' .. string.format(tLang.L('save_all_to_folder_failed_fmt'), failed)
        if #details > 0 then msg = msg .. '\n' .. table.concat(details, '\n') end
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

local function applyAllCentralize(sType)
    return runApplyAllOperation(sType, tLang.L('centralize'), function(tEntry)
        tEntry.meshDebug:centralize()
        tEntry.modified = true
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
    return runApplyAllOperation(sType, operationLabel, function(tEntry)
        local ok = false
        if sMode == 'rotate' then
            ok = dpCall(function() tEntry.meshDebug:rotateFrame(xf.frame, xf.rx, xf.ry, xf.rz, xf.subset) end)
        elseif sMode == 'scale' then
            ok = dpCall(function() tEntry.meshDebug:scaleFrame(xf.frame, xf.sx, xf.sy, xf.sz, xf.subset) end)
        elseif sMode == 'translate' then
            ok = dpCall(function() tEntry.meshDebug:translateFrame(xf.frame, xf.dx, xf.dy, xf.dz, xf.subset) end)
        end
        if ok then
            tEntry.modified = true
            return 'success'
        end
        return 'failed', tLang.L('an_error_occurred')
    end)
end

-- Bakes each mesh's own current default angle into its geometry (same-sign rotateFrame,
-- so the rendered appearance is unchanged) then zeroes the default angle. No-op if already 0,0,0.
local function applyAllResetDefaultAngle(sType)
    return runApplyAllOperation(sType, tLang.L('reset_default_angle'), function(tEntry)
        local meshD = tEntry.meshDebug
        local okGet, ang = dpCall(function() return meshD:getAngle() end)
        if not okGet or not ang then
            return 'failed', tLang.L('an_error_occurred')
        end
        if (ang.x or 0) == 0 and (ang.y or 0) == 0 and (ang.z or 0) == 0 then
            return 'skipped', tLang.L('default_already_zero')
        end
        local okRotate = dpCall(function() meshD:rotateFrame(0, ang.x, ang.y, ang.z, 0) end)
        if not okRotate then
            return 'failed', tLang.L('an_error_occurred')
        end
        local okSet = dpCall(function() meshD:setAngle(0, 0, 0) end)
        if not okSet then
            return 'failed', tLang.L('an_error_occurred')
        end
        tEntry.modified = true
        return 'success'
    end)
end

-- Bakes each mesh's own current default position into its geometry (same-sign translateFrame,
-- so the rendered appearance is unchanged) then zeroes the default position. No-op if already 0,0,0.
local function applyAllResetDefaultPosition(sType)
    return runApplyAllOperation(sType, tLang.L('reset_default_position'), function(tEntry)
        local meshD = tEntry.meshDebug
        local okGet, pos = dpCall(function() return meshD:getPosition() end)
        if not okGet or not pos then
            return 'failed', tLang.L('an_error_occurred')
        end
        if (pos.x or 0) == 0 and (pos.y or 0) == 0 and (pos.z or 0) == 0 then
            return 'skipped', tLang.L('default_already_zero')
        end
        local okTranslate = dpCall(function() meshD:translateFrame(0, pos.x, pos.y, pos.z, 0) end)
        if not okTranslate then
            return 'failed', tLang.L('an_error_occurred')
        end
        local okSet = dpCall(function() meshD:setPosition(0, 0, 0) end)
        if not okSet then
            return 'failed', tLang.L('an_error_occurred')
        end
        tEntry.modified = true
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
-- a true reset, matching applyAllResetDefaultAngle/applyAllResetDefaultPosition above.
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
                tImGui.Spacing()
                tImGui.Separator()
                if tImGui.Button(tLang.L('reset_default_angle') .. '##applyAllResetDefaultAngle') then
                    applyAllResetDefaultAngle(win.selectedType)
                end
                tImGui.SameLine()
                tImGui.HelpMarker(tLang.L('reset_default_angle_tooltip'))
                if tImGui.Button(tLang.L('reset_default_position') .. '##applyAllResetDefaultPosition') then
                    applyAllResetDefaultPosition(win.selectedType)
                end
                tImGui.SameLine()
                tImGui.HelpMarker(tLang.L('reset_default_position_tooltip'))
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
            if tImGui.MenuItem(tLang.L("save_all_to_folder"), nil, false, #tLoadedMeshes > 0) then
                onSaveAllToFolder()
            end
            tImGui.Separator()
            if tImGui.MenuItem(tLang.L('import_via_blender')) then
                onOpenBlenderImportDialog()
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

    local width = 350
    local iW, iH = mbm.getSizeScreen()
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_mesh_tree, 0, 0, width, width + 100, iH * 0.8)
    local is_opened, closed_clicked = tImGui.Begin(tLang.L(tWindowsTitle.title_mesh_tree), true, 0)

    if is_opened then
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
                    -- Same pre-existing leak class for the Transform tab's preview clone (its
                    -- own auto-cancel likewise only ran inside showMeshOptions, i.e. only while
                    -- this entry was the selected one).
                    if tEntry.tXformPreviewMesh then
                        tEntry.tXformPreviewMesh:destroy()
                        tEntry.tXformPreviewMesh = nil
                    end
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
            if bCameraMode3D then
                bShowOrigin3d = newOrig
                originLine3dX.visible = newOrig
                originLine3dY.visible = newOrig
                originLine3dZ.visible = newOrig
            else
                bShowOrigin2d = newOrig
                originLine2dX.visible = newOrig
                originLine2dY.visible = newOrig
            end
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
    showBlenderImportDialog()
    showCameraWindow()
    showLightWindow()
    showMeshTreeWindow()
    showApplyAllWindow()
    showListTexturesWindow()
    showListMeshesWindow()
    updatePreviewMesh()
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
end

function onTouchDown(key, x, y)
    if not tImGui.IsAnyWindowHovered() then
        isClickedMouseleft  = (key == 0)
        isClickedMouseRight = (key == 1)
        camera2d.mx = x
        camera2d.my = y
    end
end

function onTouchMove(key, x, y)
    if tImGui.IsAnyWindowHovered() then return end
    if bCameraMode3D and iSelectedMeshIndex > 0 and iSelectedMeshIndex <= #tLoadedMeshes then
        local c = tLoadedMeshes[iSelectedMeshIndex].cam3d
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
        iSelectedMeshIndex = iSelectedMeshIndex + 1
        if iSelectedMeshIndex > #tLoadedMeshes then 
            iSelectedMeshIndex = #tLoadedMeshes
        end
    elseif mbm.getKeyName(key) == 'UP' then
        iSelectedMeshIndex = iSelectedMeshIndex - 1
        if iSelectedMeshIndex < 1 then
            iSelectedMeshIndex = 1
        end
    end
end
function onKeyUp() end
