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
                            bCameraMode3D = true
                            originLine2dX.visible = false
                            originLine2dY.visible = false
                            originLine3dX.visible = bShowOrigin3d
                            originLine3dY.visible = bShowOrigin3d
                            originLine3dZ.visible = bShowOrigin3d
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
        sOpenNode            = nil
    })
    -- Auto-switch camera to 3D when a mesh (.msh) file is loaded
    if info.type == 'mesh' and not bCameraMode3D then
        bCameraMode3D = true
        originLine2dX.visible = false
        originLine2dY.visible = false
        originLine3dX.visible = bShowOrigin3d
        originLine3dY.visible = bShowOrigin3d
        originLine3dZ.visible = bShowOrigin3d
    end
    return true
end

function removeMeshFromTable(index)
    table.remove(tLoadedMeshes, index)
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

    -- Editable: Material (Diffuse + Power)
    if tImGui.TreeNodeEx(tLang.L("material"), 0, 'material-' .. index) then
        local ok, mat = dpCall(function() return meshD:getMaterial() end)
        if ok and mat and mat.Diffuse then
            local d = {r=mat.Diffuse.r or 1, g=mat.Diffuse.g or 1, b=mat.Diffuse.b or 1}
            local clicked, newD = tImGui.ColorEdit3(tLang.L("diffuse") .. '##mat-' .. index, d, flags)
            if clicked and newD then
                local newMat = { Diffuse = {r=newD.r,g=newD.g,b=newD.b,a=1}, Ambient = mat.Ambient, Specular = mat.Specular, Emissive = mat.Emissive, Power = mat.Power or 1 }
                local okSet = dpCall(function() meshD:setMaterial(newMat) end)
                if okSet then onEdit() end
            end
            local pw = mat.Power or 1
            local rp, np = tImGui.InputFloat(tLang.L("power") .. '##mat-' .. index, pw, 0.1, 1, '%.2f', flags)
            if rp then
                local newMat = { Diffuse = mat.Diffuse, Ambient = mat.Ambient, Specular = mat.Specular, Emissive = mat.Emissive, Power = np }
                local okSet = dpCall(function() meshD:setMaterial(newMat) end)
                if okSet then onEdit() end
            end
        end
        tImGui.TreePop()
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
                if meshD:save(newFile, false, false) then
                    tUtil.showMessage(string.format(tLang.L('save_as_success_fmt'), tUtil.getShortName(newFile)))
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
            if index == iSelectedMeshIndex then iLastPreviewedIndex = 0 end
            if nVertices > 0 then
                tUtil.showMessage(string.format('Added normals: %s\n%d vertices', shortName, nVertices), 4)
            else
                tUtil.showMessage('Added normals: ' .. shortName, 4)
            end
        end
        tImGui.TreePop()
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
        tEntry.tTexUI = tEntry.tTexUI or { frame=0, subset=0, stage=0, filename='' }
        local tx      = tEntry.tTexUI
        tx.filename   = tx.filename or ''

        local totalFrames  = info.totalFrames or 0
        local totalSubsets = 0
        do
            local okS, nS = dpCall(function() return meshD:getTotalSubset(math.max(1, tx.frame)) end)
            if okS and nS then totalSubsets = nS end
        end

        -- Frame / subset selectors (0 = all, consistent with Transform node)
        tImGui.Separator()
        tImGui.Text(tLang.L("target_frame_label"))
        local _, nf = tImGui.InputInt('##txFrame-' .. index, tx.frame, 1, 1, 0)
        if nf ~= nil then tx.frame = math.max(0, math.min(nf, totalFrames)) end

        tImGui.Text(tLang.L("target_subset_label"))
        local _, ns = tImGui.InputInt('##txSubset-' .. index, tx.subset, 1, 1, 0)
        if ns ~= nil then tx.subset = math.max(0, math.min(ns, totalSubsets)) end

        -- Stage selector
        tImGui.Spacing()
        tImGui.Text(tLang.L("tex_stage_label"))
        local stageOpts = {'0 - Primary', '1 - FX (per anim step)'}
        local stageRet, newStageIdx = tImGui.Combo('##txStage-' .. index, tx.stage + 1, stageOpts, -1)
        if stageRet and newStageIdx then tx.stage = newStageIdx - 1 end

        tImGui.Spacing()
        if tx.stage == 1 then
            tImGui.TextWrapped(tLang.L("tex_stage1_note"))
        else
            -- Show current texture when a specific frame+subset is selected
            if tx.frame > 0 and tx.subset > 0 then
                local okG, curTex = dpCall(function() return meshD:getTexture(tx.frame, tx.subset) end)
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
                            local ok = dpCall(function() meshD:setTexture(f, s, fn) end)
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
                        local ok = dpCall(function() meshD:setTexture(f, s, '') end)
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
            local okSh, tShader = dpCall(function() return tPreviewMesh:getShader() end)
            if okSh and tShader then
                tImGui.PushItemWidth(180)
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
            local ok = meshD:save(tEntry.fileName, false, false)
            if ok then
                tEntry.modified = false
                iLastPreviewedIndex = 0
                tUtil.showMessage(string.format('Saved: %s', shortName))
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
        tUtil.showMessage(string.format(tLang.L("save_as_success_fmt"), tUtil.getShortName(newFile)))
        sLastMeshPath = newFile
    else
        tUtil.showMessageWarn(string.format(tLang.L("save_failed_fmt"), shortName))
    end
end

function applyToAll(operation)
    local iSuccess = 0
    local iFailed = 0
    local iTotalVertices = 0
    local iTotalBytesSaved = 0
    for i = 1, #tLoadedMeshes do
        local tEntry = tLoadedMeshes[i]
        local meshD = tEntry.meshDebug
        local ok = false
        if operation == 'removeNormals' then
            if tEntry.info and tEntry.info.hasNormal then
                local nV = getMeshTotalVertices(meshD)
                iTotalVertices = iTotalVertices + nV
                iTotalBytesSaved = iTotalBytesSaved + (nV * 12)
            end
            meshD:removeNormals()
            if tEntry.info then tEntry.info.hasNormal = false end
            tEntry.modified = true
            ok = true
        elseif operation == 'addNormals' then
            iTotalVertices = iTotalVertices + getMeshTotalVertices(meshD)
            meshD:addNormals()
            if tEntry.info then tEntry.info.hasNormal = true end
            tEntry.modified = true
            ok = true
        elseif operation == 'centralize' then
            meshD:centralize()
            tEntry.modified = true
            ok = true
        elseif operation == 'save' then
            local animErr = collectAnimFrameErrors(tEntry)
            if not animErr then
                ok = meshD:save(tEntry.fileName, false, false)
                if ok then tEntry.modified = false end
            end
        elseif operation == 'saveRecalcNormals' then
            local animErr = collectAnimFrameErrors(tEntry)
            if not animErr then
                ok = meshD:save(tEntry.fileName, true, false)
                if ok then
                    tEntry.modified = false
                    if tEntry.info then tEntry.info.hasNormal = true end
                end
            end
        end
        if ok then
            iSuccess = iSuccess + 1
            iLastPreviewedIndex = 0
        else
            iFailed = iFailed + 1
        end
    end
    local msg = string.format('Apply to %d: %d success, %d failed', #tLoadedMeshes, iSuccess, iFailed)
    if operation == 'removeNormals' and iTotalVertices > 0 then
        msg = msg .. string.format('\n%d vertices, ~%s saved', iTotalVertices, formatBytes(iTotalBytesSaved))
    elseif operation == 'addNormals' and iTotalVertices > 0 then
        msg = msg .. string.format('\n%d vertices', iTotalVertices)
    end
    tUtil.showMessage(msg)
end

function showApplyToAllMenu()
    if tImGui.BeginMenu(tLang.L("apply_to_all")) then
        local enabled = (#tLoadedMeshes > 0)
        if tImGui.MenuItem(tLang.L("remove_normals"), nil, false, enabled) then
            applyToAll('removeNormals')
        end
        if tImGui.MenuItem(tLang.L("add_normals"), nil, false, enabled) then
            applyToAll('addNormals')
        end
        if tImGui.MenuItem(tLang.L("centralize"), nil, false, enabled) then
            applyToAll('centralize')
        end
        if tImGui.MenuItem(tLang.L("save_all_overwrite"), nil, false, enabled) then
            applyToAll('save')
        end
        if tImGui.MenuItem(tLang.L("save_all_calc_normals"), nil, false, enabled) then
            applyToAll('saveRecalcNormals')
        end
        tImGui.EndMenu()
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
            tImGui.Separator()
            if tImGui.MenuItem('Load OBJ(s)') then
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
    local is_opened, closed_clicked = tImGui.Begin(tLang.L(tWindowsTitle.title_mesh_tree), true, tImGui.Flags('ImGuiWindowFlags_NoMove'))

    if is_opened then
        if tImGui.BeginMenuBar() then
            if tImGui.MenuItem('Load Mesh(s)') then
                onLoadMeshFromFile()
            end
            if tImGui.MenuItem(tLang.L("load_from_folder")) then
                onLoadMeshFromFolder()
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
    local iW = mbm.getSizeScreen()
    local winW = 240
    tImGui.SetNextWindowPos({x = iW - winW - 5, y = 25}, tImGui.Flags('ImGuiCond_Always'))
    local wFlags = tImGui.Flags('ImGuiWindowFlags_NoMove', 'ImGuiWindowFlags_AlwaysAutoResize',
                                'ImGuiWindowFlags_NoCollapse')
    local opened = tImGui.Begin(tLang.L('camera_panel') .. '##camWin', false, wFlags)
    if opened then
        -- Mode toggle: 2D / 3D radio buttons
        local prev3d = bCameraMode3D
        local camMode = bCameraMode3D and 1 or 0
        camMode = tImGui.RadioButton(tLang.L('camera_2d') .. '##camMode', camMode, 0)
        tImGui.SameLine()
        camMode = tImGui.RadioButton(tLang.L('camera_3d') .. '##camMode', camMode, 1)
        bCameraMode3D = (camMode == 1)
        if prev3d ~= bCameraMode3D then
            iLastPreviewedIndex = 0  -- force preview reload with correct coord type
            -- Sync origin line visibility to the newly active camera
            originLine2dX.visible = (not bCameraMode3D) and bShowOrigin2d
            originLine2dY.visible = (not bCameraMode3D) and bShowOrigin2d
            originLine3dX.visible = bCameraMode3D and bShowOrigin3d
            originLine3dY.visible = bCameraMode3D and bShowOrigin3d
            originLine3dZ.visible = bCameraMode3D and bShowOrigin3d
        end
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
                local px, py, pz = cam3dGetPos(c)
                tImGui.PushItemWidth(72)

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
            tImGui.PushItemWidth(72)
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
            tImGui.TextDisabled(tLang.L('cam_hint_2d'))
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
    showCameraWindow()
    showMeshTreeWindow()
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
