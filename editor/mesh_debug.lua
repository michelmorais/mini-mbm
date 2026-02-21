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

if not mbm.get('USE_EDITOR_FEATURES') then
    mbm.messageBox('Missing features','Is necessary to compile using USE_EDITOR_FEATURES to run this editor','ok','error',0)
    mbm.quit()
end

-- Mesh entry: { fileName, meshDebug, info, loaded }
-- info from meshDebug:getInfo(fileName) - type, hasNormal, hasTexture, totalFrames, etc.

function onInitScene()
    camera2d              = mbm.getCamera("2d")
    tLoadedMeshes         = {}
    sLastMeshPath         = mbm.get('user_home') or mbm.get('HOME') or '~'
    sLastFolderPath       = sLastMeshPath
    bShowMeshTree         = true
    tWindowsTitle         = {
        title_mesh_tree   = 'Mesh Debug - Loaded Meshes',
        title_apply_all   = 'Apply to All'
    }
    tUtil.sMessageOverlay = 'Welcome to Mesh Debug Editor! Load meshes from File or Folder.'
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
            tUtil.showMessage(string.format('Loaded %d mesh(es)', #tFiles))
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
        tUtil.showMessage(string.format('Loaded %d mesh(es) from folder (%d total)', iAdded, #tFiles))
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
    table.insert(tLoadedMeshes, {
        fileName = fileName,
        meshDebug = meshD,
        info = info,
        loaded = true
    })
    return true
end

function removeMeshFromTable(index)
    table.remove(tLoadedMeshes, index)
end

-- Returns total vertex count across all frames/subsets, or 0 on error
function getMeshTotalVertices(meshD)
    local total = 0
    local ok, nFrames = pcall(function() return meshD:getTotalFrame() end)
    if not ok or not nFrames then return 0 end
    for f = 1, nFrames do
        local ok2, nSubsets = pcall(function() return meshD:getTotalSubset(f) end)
        if ok2 and nSubsets then
            for s = 1, nSubsets do
                local ok3, nV = pcall(function() return meshD:getTotalVertex(f, s) end)
                if ok3 and nV then
                    total = total + nV
                end
            end
        end
    end
    return total
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

function showMeshOptions(tEntry, index)
    local meshD = tEntry.meshDebug
    local info = tEntry.info
    local shortName = tUtil.getShortName(tEntry.fileName)

    if tImGui.TreeNodeEx('Normals', 0, 'normals-' .. index) then
        if info and info.hasNormal then
            tImGui.TextDisabled('Has normals')
        else
            tImGui.TextDisabled('No normals')
        end
        if tImGui.Button('Remove Normals##' .. index) then
            local nVertices = 0
            if info and info.hasNormal then
                nVertices = getMeshTotalVertices(meshD)
            end
            meshD:removeNormals()
            if tEntry.info then tEntry.info.hasNormal = false end
            if nVertices > 0 then
                local bytesSaved = nVertices * 12  -- 3 floats per normal
                tUtil.showMessage(string.format('Removed normals: %s\n%d vertices (~%s saved)', shortName, nVertices, formatBytes(bytesSaved)))
            else
                tUtil.showMessage('Removed normals: ' .. shortName)
            end
        end
        tImGui.SameLine()
        if tImGui.Button('Add Normals##' .. index) then
            local nVertices = getMeshTotalVertices(meshD)
            meshD:addNormals()
            if tEntry.info then tEntry.info.hasNormal = true end
            if nVertices > 0 then
                tUtil.showMessage(string.format('Added normals: %s\n%d vertices', shortName, nVertices))
            else
                tUtil.showMessage('Added normals: ' .. shortName)
            end
        end
        tImGui.TreePop()
    end

    if tImGui.TreeNodeEx('Transform', 0, 'transform-' .. index) then
        if tImGui.Button('Centralize##' .. index) then
            meshD:centralize()
            tUtil.showMessage(string.format('Centralized: %s', shortName))
        end
        tImGui.TreePop()
    end

    if tImGui.TreeNodeEx('Validation', 0, 'validation-' .. index) then
        if tImGui.Button('Check##' .. index) then
            local ok, err = meshD:check()
            if ok then
                tUtil.showMessage(string.format('Check OK: %s', shortName))
            else
                tUtil.showMessageWarn(string.format('Check failed: %s\n%s', shortName, err or ''))
            end
        end
        tImGui.TreePop()
    end

    if tImGui.TreeNodeEx('Save', 0, 'save-' .. index) then
        if tImGui.Button('Save (overwrite)##' .. index) then
            local ok = meshD:save(tEntry.fileName, false, false)
            if ok then
                tUtil.showMessage(string.format('Saved: %s', shortName))
            else
                tUtil.showMessageWarn(string.format('Save failed: %s', shortName))
            end
        end
        tImGui.SameLine()
        if tImGui.Button('Save (recalc normals)##' .. index) then
            local ok = meshD:save(tEntry.fileName, true, false)
            if ok then
                tUtil.showMessage(string.format('Saved: %s', shortName))
            else
                tUtil.showMessageWarn(string.format('Save failed: %s', shortName))
            end
        end
        tImGui.TreePop()
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
            ok = true
        elseif operation == 'addNormals' then
            iTotalVertices = iTotalVertices + getMeshTotalVertices(meshD)
            meshD:addNormals()
            if tEntry.info then tEntry.info.hasNormal = true end
            ok = true
        elseif operation == 'centralize' then
            meshD:centralize()
            ok = true
        elseif operation == 'save' then
            ok = meshD:save(tEntry.fileName, false, false)
        elseif operation == 'saveRecalcNormals' then
            ok = meshD:save(tEntry.fileName, true, false)
        end
        if ok then
            iSuccess = iSuccess + 1
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
    if tImGui.BeginMenu('Apply to All') then
        local enabled = (#tLoadedMeshes > 0)
        if tImGui.MenuItem('Remove Normals', nil, false, enabled) then
            applyToAll('removeNormals')
        end
        if tImGui.MenuItem('Add Normals', nil, false, enabled) then
            applyToAll('addNormals')
        end
        if tImGui.MenuItem('Centralize', nil, false, enabled) then
            applyToAll('centralize')
        end
        if tImGui.MenuItem('Save All (overwrite)', nil, false, enabled) then
            applyToAll('save')
        end
        if tImGui.MenuItem('Save All (recalc normals)', nil, false, enabled) then
            applyToAll('saveRecalcNormals')
        end
        tImGui.EndMenu()
    end
end

function main_menu_mesh_debug()
    if tImGui.BeginMainMenuBar() then
        if tImGui.BeginMenu('File') then
            if tImGui.MenuItem('Load Mesh(s)') then
                onLoadMeshFromFile()
            end
            if tImGui.MenuItem('Load from Folder') then
                onLoadMeshFromFolder()
            end
            tImGui.Separator()
            showApplyToAllMenu()
            tImGui.Separator()
            if tImGui.MenuItem('Clear All') then
                tLoadedMeshes = {}
                tUtil.showMessage('Cleared all meshes')
            end
            tImGui.EndMenu()
        end
        if tImGui.BeginMenu('View') then
            local pressed, checked = tImGui.MenuItem('Show Mesh Tree', nil, bShowMeshTree)
            if pressed then
                bShowMeshTree = not bShowMeshTree
            end
            tImGui.EndMenu()
        end
        tImGui.EndMainMenuBar()
    end
end

function showMeshTreeWindow()
    if not bShowMeshTree then return end

    local width = 350
    tUtil.setInitialWindowPositionLeft(tWindowsTitle.title_mesh_tree, 0, 0, width, width + 100)
    local is_opened, closed_clicked = tImGui.Begin(tWindowsTitle.title_mesh_tree, true, tImGui.Flags('ImGuiWindowFlags_NoMove'))

    if is_opened then
        if tImGui.BeginMenuBar() then
            if tImGui.MenuItem('Load Mesh(s)') then
                onLoadMeshFromFile()
            end
            if tImGui.MenuItem('Load from Folder') then
                onLoadMeshFromFolder()
            end
            showApplyToAllMenu()
            tImGui.EndMenuBar()
        end

        tImGui.TextDisabled(string.format('%d mesh(es) loaded', #tLoadedMeshes))

        if #tLoadedMeshes == 0 then
            tImGui.TextWrapped('Use File menu or Load from Folder to add meshes.')
        else
            local flags = tImGui.Flags('ImGuiTreeNodeFlags_DefaultOpen')
            local tToRemove = {}
            for i = 1, #tLoadedMeshes do
                local tEntry = tLoadedMeshes[i]
                local shortName = tUtil.getShortName(tEntry.fileName)
                local typeStr = (tEntry.info and tEntry.info.type) or '?'
                local label = string.format('%s [%s]', shortName, typeStr)

                if tImGui.TreeNodeEx(label, flags, 'mesh-' .. i) then
                    showMeshOptions(tEntry, i)
                    if tImGui.Button('Remove from list##' .. i) then
                        table.insert(tToRemove, i)
                    end
                    tImGui.TreePop()
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

function loop(delta)
    main_menu_mesh_debug()
    showMeshTreeWindow()
    tUtil.showOverlayMessage()
end

function onTouchDown() end
function onTouchMove() end
function onTouchUp() end
function onKeyDown() end
function onKeyUp() end
