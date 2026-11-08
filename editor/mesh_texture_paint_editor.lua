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
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|
]]--

tImGui = require 'ImGui'
tUtil = require 'editor_utils'
local Pick = require 'mesh_texture_paint_pick'
local Model = require 'mesh_texture_paint_model'
local Raster = require 'mesh_texture_paint_raster'
local Stamp = require 'mesh_texture_paint_stamp'
local Project = require 'mesh_texture_paint_project'
local PaintIO = require 'mesh_texture_paint_io'

local STAMP_RESOLUTION_MIN = 9
local STAMP_RESOLUTION_MAX = 257
local DILATION_RADIUS_MAX = 8
local PROJECT_SAMPLE_MAX = 33
local PROJECT_SAMPLE_SPACING = 8
local PROJECT_STROKE_MAX_STEPS = 32
local STROKE_POINTS_PER_FRAME = 1
-- The UV preview is an inspection aid, not part of the paint viewport. Keep
-- its immediate-mode draw budget bounded because ImGui rebuilds these lines
-- every frame, including while the editor is idle.
local UV_OVERLAY_MAX_TRIANGLES = 2048
local UV_OVERLAY_MAX_BOUNDARY_EDGES = 1024
local CAMERA_WINDOW_MAX_HEIGHT = 390

local state = {
    camera = nil,
    meshPath = nil,
    sourceAsset = nil,
    previewAsset = nil,
    previewMesh = nil,
    previewMeshPath = nil,
    previewBoundTexturePath = nil,
    previewTexturePath = nil,
    previewUiTexturePath = nil,
    previewTextureInfo = nil,
    previewTextureIsUiCopy = false,
    meshPreviewTextureInfo = nil,
    stamp = nil,
    stampTexturePath = nil,
    stampTextureInfo = nil,
    externalStamp = nil,
    stampOverlay = false,
    stampSelectionShape = 'square',
    stampSelection = nil,
    stampSelectionDrag = nil,
    previewReady = false,
    previewRequiresMesh = false,
    snapshots = {},
    targets = {},
    targetIndex = 0,
    target = nil,
    base = nil,
    layer = nil,
    composite = nil,
    textureWidth = 0,
    textureHeight = 0,
    previewDirty = false,
    compositeDirtyTiles = nil,
    previewJob = nil,
    previewCancelled = false,
    previewBuilds = 0,
    previewTextureReloads = 0,
    previewMeshReloads = 0,
    projectPath = nil,
    modified = false,
    exportTexturePath = nil,
    exportMeshPath = nil,
    exportDirty = true,
    status = 'Open an MSH to start painting.',
    brush = {radius = 8, hardness = 1, opacity = 1, stampResolution = 65, mode = 'paint', shape = 'circle',
        highContrastCursor = true, cursorColor = {r=1,g=0,b=1,a=1},
        showUvOverlay = false, color = {r=1,g=0,b=0,a=1}},
    paintThrough = false,
    limitToSubset = false,
    selectedSubset = 0,
    limitToUvIsland = false,
    selectedUvIsland = 0,
    dilationRadius = 0,
    pendingDilationRadius = nil,
    brushMaxRadius = 64,
    meshDiagonal = 1,
    cameraOrbit = {fx=0, fy=0, fz=0, distance=5, azimuth=0, elevation=.2},
    cameraRevision = 0,
    cursorCache = {},
    mouse = {x=0, y=0},
    hoverHit = nil,
    lastUvIslandHit = nil,
    hoverPickPending = false,
    hoverPickElapsed = 0,
    orbiting = false,
    panning = false,
    painting = false,
    stroke = nil,
    undo = {},
    redo = {},
    keyControl = false,
    pendingAction = nil,
    openUnsavedPopup = false,
    titles = {
        controls = 'mesh_texture_paint_controls',
        texture = 'mesh_texture_paint_texture',
        camera = 'mesh_texture_paint_camera',
    },
}

local brushUvDerivatives
local stopPreviewAnimations
local projectSamplesAtScreen
local surfaceSamplesAtHit

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then
        print('[mesh_texture_paint_editor] ' .. tostring(result[2]))
    end
    return table.unpack(result, 1, result.n)
end

local function imguiCapturesMouse()
    -- WantCaptureMouse also covers active combo popups and item drags. The
    -- hovered-window fallback protects input callbacks using the previous
    -- completed ImGui frame.
    return tImGui.GetWantCaptureMouse() or tImGui.IsAnyWindowHovered()
end

local function imguiCapturesKeyboard()
    return tImGui.GetWantCaptureKeyboard()
end

local function loadTextureInfo(path)
    local loaded, textureInfo = dpCall(mbm.loadTexture, path)
    if not loaded or not textureInfo then
        return false, textureInfo
    end
    local inspected, isLoaded = dpCall(function() return textureInfo:isLoaded() end)
    if inspected and isLoaded then
        return true, textureInfo
    end
    local reloaded, reloadResult = dpCall(function() return textureInfo:reload(path) end)
    if not reloaded or not reloadResult then
        return false, reloadResult or 'texture cache returned an unloaded texture'
    end
    return true, textureInfo
end

local function clearStamp()
    if state.stampTextureInfo then state.stampTextureInfo:release() end
    if state.stampTexturePath then os.remove(state.stampTexturePath) end
    state.stamp, state.stampTexturePath, state.stampTextureInfo = nil, nil, nil
end

local function clearExternalStamp()
    if state.externalStamp and state.externalStamp.textureInfo then
        state.externalStamp.textureInfo:release()
    end
    state.externalStamp = nil
    state.stampOverlay = false
    state.stampSelection = nil
    state.stampSelectionDrag = nil
end

local function clearPreview()
    if state.previewMesh and state.previewAsset then
        dpCall(function() return meshDebug:loadMeshPreview(state.previewMesh, nil) end)
    end
    if state.previewMesh then state.previewMesh:destroy() end
    if state.previewTextureInfo and state.previewTextureIsUiCopy then
        state.previewTextureInfo:release()
    end
    if state.meshPreviewTextureInfo then
        state.meshPreviewTextureInfo:release()
    end
    clearStamp()
    clearExternalStamp()
    state.previewMesh, state.previewAsset, state.previewTextureInfo = nil, nil, nil
    state.meshPreviewTextureInfo = nil
    state.previewReady, state.previewRequiresMesh = false, false
    state.previewBoundTexturePath = nil
    if state.previewMeshPath then os.remove(state.previewMeshPath) end
    if state.previewTexturePath then os.remove(state.previewTexturePath) end
    state.previewMeshPath, state.previewTexturePath, state.previewUiTexturePath = nil, nil, nil
    state.previewTextureIsUiCopy = false
    state.exportTexturePath, state.exportMeshPath, state.exportDirty = nil, nil, true
    state.previewCancelled = false
    state.compositeDirtyTiles = nil
    state.previewJob = nil
    state.previewCancelled = false
    state.hoverHit = nil
    state.hoverPickPending = false
    state.hoverPickElapsed = 0
end

local function resolvePath(path)
    return (path and mbm.getFullPath(path)) or path
end

local function pathDirectory(path)
    return path and path:match('^(.*)[/\\]') or nil
end

local function normalizedTexture(path)
    if not path or path == '' or path:sub(1, 1) == '#' then return nil end
    return resolvePath(path)
end

local function applyCamera()
    local c = state.cameraOrbit
    local x = c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth)
    local y = c.fy + c.distance * math.sin(c.elevation)
    local z = c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
    state.camera:setPos(x, y, z)
    state.camera:setFocus(c.fx, c.fy, c.fz)
    state.camera:setFar(math.max(2000, c.distance * 12))
    state.cameraRevision = state.cameraRevision + 1
    state.hoverHit = nil
    state.hoverPickPending = true
    state.hoverPickElapsed = 0
end

local function frameCamera()
    local bounds
    for _, snapshot in ipairs(state.snapshots) do
        for _, vertex in ipairs(snapshot.vertices) do
            bounds = bounds or {minX=vertex.x,maxX=vertex.x,minY=vertex.y,maxY=vertex.y,minZ=vertex.z,maxZ=vertex.z}
            bounds.minX, bounds.maxX = math.min(bounds.minX, vertex.x), math.max(bounds.maxX, vertex.x)
            bounds.minY, bounds.maxY = math.min(bounds.minY, vertex.y), math.max(bounds.maxY, vertex.y)
            bounds.minZ, bounds.maxZ = math.min(bounds.minZ, vertex.z), math.max(bounds.maxZ, vertex.z)
        end
    end
    if not bounds then return end
    state.cameraOrbit.fx = (bounds.minX + bounds.maxX) * .5
    state.cameraOrbit.fy = (bounds.minY + bounds.maxY) * .5
    state.cameraOrbit.fz = (bounds.minZ + bounds.maxZ) * .5
    local dx, dy, dz = bounds.maxX - bounds.minX, bounds.maxY - bounds.minY, bounds.maxZ - bounds.minZ
    state.meshDiagonal = math.sqrt(dx*dx + dy*dy + dz*dz)
    state.cameraOrbit.distance = math.max(.1, state.meshDiagonal * 1.6)
    applyCamera()
end

local function colorBytes()
    local c = state.brush.color
    return {r=math.floor((c.r or 0) * 255 + .5), g=math.floor((c.g or 0) * 255 + .5),
        b=math.floor((c.b or 0) * 255 + .5), a=math.floor((c.a or 1) * 255 + .5)}
end

local function normalizeDilationRadius(value)
    return math.max(0, math.min(DILATION_RADIUS_MAX, math.floor((tonumber(value) or 0) + .5)))
end

local function applyDilationRadius(value)
    value = normalizeDilationRadius(value)
    if state.dilationRadius == value then return false end
    state.dilationRadius = value
    state.previewDirty = true
    state.previewCancelled = false
    state.modified = true
    state.exportDirty = true
    if state.layer then
        state.compositeDirtyTiles = Raster.tilesForRect(state.layer, 0, 0,
            state.layer.width - 1, state.layer.height - 1)
    end
    return true
end

local function releaseTextureWork()
    state.base, state.layer, state.composite, state.target = nil, nil, nil, nil
    state.textureWidth, state.textureHeight = 0, 0
end

local function availableUvIslands(target)
    if not target then return {} end
    if state.limitToSubset and state.selectedSubset > 0 then
        return target.uvIslandsBySubset[state.selectedSubset] or {}
    end
    return target.uvIslands or {}
end

local function ensureSelectedUvIsland()
    local islands = availableUvIslands(state.target)
    for _, island in ipairs(islands) do
        if island.id == state.selectedUvIsland then return end
    end
    state.selectedUvIsland = islands[1] and islands[1].id or 0
end

local function uvIslandFromHit(hit)
    if not (hit and hit.triangle and state.target) then return nil end
    local islandId = hit.triangle.uvIsland
    if not islandId then return nil end
    local island = state.target.uvIslands[islandId]
    if not island then return nil end
    return islandId, island
end

local function selectUvIslandFromHit(hit)
    local islandId, island = uvIslandFromHit(hit)
    if not islandId then
        state.status = tLang.L('mtp_no_uv_island_under_cursor')
        return false
    end
    state.selectedUvIsland = islandId
    state.limitToUvIsland = true
    state.lastUvIslandHit = hit
    state.hoverHit = hit
    state.hoverPickPending = false
    state.status = string.format(tLang.L('mtp_uv_island_selected'), islandId,
        #island.triangleIndices)
    return true
end

local function selectTarget(index, preparedTextureInfo)
    local target = state.targets[index]
    if not target then return false end
    local loaded, result = preparedTextureInfo ~= nil, preparedTextureInfo
    if not loaded then loaded, result = loadTextureInfo(target.path) end
    if not loaded or not result then
        state.status = 'Could not load texture: ' .. tostring(result)
        return false
    end
    if state.previewTextureInfo and state.previewTextureIsUiCopy then
        state.previewTextureInfo:release()
    end
    state.previewTextureInfo = nil
    state.previewTextureIsUiCopy = false
    state.exportTexturePath, state.exportMeshPath, state.exportDirty = nil, nil, true
    if state.meshPreviewTextureInfo then
        state.meshPreviewTextureInfo:release()
        state.meshPreviewTextureInfo = nil
    end
    local textureInfo = result
    state.previewJob = nil
    clearStamp()
    clearExternalStamp()
    releaseTextureWork()
    state.targetIndex, state.target = index, target
    state.pendingDilationRadius = nil
    state.lastUvIslandHit = nil
    local hasSelectedSubset = false
    for _, subset in ipairs(target.subsets or {}) do
        if subset == state.selectedSubset then hasSelectedSubset = true; break end
    end
    if not hasSelectedSubset then state.selectedSubset = target.subsets[1] or 0 end
    ensureSelectedUvIsland()
    state.hoverHit = nil
    state.hoverPickPending = false
    state.hoverPickElapsed = 0
    state.previewTextureInfo = textureInfo
    state.textureWidth, state.textureHeight = textureInfo.width, textureInfo.height
    state.brushMaxRadius = math.max(4, math.min(256,
        math.floor(math.min(state.textureWidth, state.textureHeight) * .12)))
    state.brush.radius = math.min(state.brush.radius, state.brushMaxRadius)
    state.composite = nil
    state.compositeDirtyTiles = nil
    state.undo, state.redo = {}, {}
    state.modified = false
    state.previewDirty = true
    state.previewRequiresMesh = true
    state.status = string.format('Texture selected: %s (%dx%d)', target.path,
        state.textureWidth, state.textureHeight)
    return true
end

local function paintSelectionKey()
    local subset = state.limitToSubset and state.selectedSubset or 0
    local island = state.limitToUvIsland and state.selectedUvIsland or 0
    return tostring(subset) .. ':' .. tostring(island)
end

local function paintTriangles()
    if not state.target then return {} end
    local key = paintSelectionKey()
    local cached = state.target.selectionTriangles[key]
    if cached then return cached end
    local source = state.target.triangles
    if state.limitToSubset and state.selectedSubset > 0 then
        source = state.target.subsetTriangles[state.selectedSubset] or {}
    end
    if state.limitToUvIsland and state.selectedUvIsland > 0 then
        cached = {}
        for _, triangle in ipairs(source) do
            if triangle.uvIsland == state.selectedUvIsland then cached[#cached + 1] = triangle end
        end
    else
        cached = source
    end
    state.target.selectionTriangles[key] = cached
    return cached
end

local function paintAcceleration()
    if not state.target then return nil end
    return state.target.pickAccelerations[paintSelectionKey()]
end

local function ensureEditableTexture()
    if state.layer then return true end
    if not state.target then return false end
    local ok, bytes, width, height = dpCall(mbm.readImagePixels, state.target.path)
    if not ok or not bytes then
        state.status = 'Could not read texture: ' .. tostring(bytes or width)
        return false
    end
    if width ~= state.textureWidth or height ~= state.textureHeight then
        state.status = 'Texture dimensions changed while loading.'
        return false
    end
    state.base = Raster.newBuffer(width, height, bytes)
    state.layer = Raster.newLayer(state.base, 32)
    state.composite = nil
    return true
end

local function loadPreviewTexture()
    if not state.layer or not state.layer.touched then
        if state.previewTextureInfo then
            local inspected, loaded = dpCall(function()
                return state.previewTextureInfo:isLoaded()
            end)
            if not inspected or not loaded then
                local reloaded, textureInfo = loadTextureInfo(state.target.path)
                if not reloaded or not textureInfo then
                    state.status = 'Could not load preview texture: ' .. tostring(textureInfo)
                    return nil
                end
                state.previewTextureInfo = textureInfo
                state.previewTextureIsUiCopy = false
            end
        else
            local loaded, textureInfo = loadTextureInfo(state.target.path)
            if not loaded or not textureInfo then
                state.status = 'Could not load preview texture: ' .. tostring(textureInfo)
                return nil
            end
            state.previewTextureInfo = textureInfo
        end
        return state.target.path
    end
    assert(state.composite and state.previewTexturePath)
    assert(mbm.writeImagePixels(state.previewTexturePath, state.composite.bytes,
        state.composite.width, state.composite.height))
    assert(state.previewUiTexturePath)
    assert(mbm.writeImagePixels(state.previewUiTexturePath, state.composite.bytes,
        state.composite.width, state.composite.height))
    if not state.meshPreviewTextureInfo then
        local loaded, textureInfo = loadTextureInfo(state.previewTexturePath)
        assert(loaded and textureInfo)
        state.meshPreviewTextureInfo = textureInfo
    else
        assert(state.meshPreviewTextureInfo:reload(state.previewTexturePath))
        state.previewTextureReloads = state.previewTextureReloads + 1
    end
    if not state.previewTextureIsUiCopy then
        local loaded, textureInfo = loadTextureInfo(state.previewUiTexturePath)
        assert(loaded and textureInfo)
        state.previewTextureInfo = textureInfo
        state.previewTextureIsUiCopy = true
    else
        assert(state.previewTextureInfo:reload(state.previewUiTexturePath))
    end
    return state.previewTexturePath
end

local function requestPreviewRebuild()
    if state.previewDirty and not state.painting and not state.previewJob and not state.previewCancelled then
        local previous = state.composite or state.base
        state.previewJob = {
            phase = 'compose', progress = 0,
            previousPreview = previous and
                Raster.newBuffer(previous.width, previous.height, previous.bytes) or nil,
            previousPreviewTextureIsUiCopy = state.previewTextureIsUiCopy,
        }
    end
end

local function restorePreviewBeforeJob(job)
    local previous = job.previousPreview
    if not previous then return end
    state.composite = previous
    state.compositeDirtyTiles = Raster.tilesForRect(state.layer, 0, 0,
        state.layer.width - 1, state.layer.height - 1)
    if state.previewTexturePath then
        dpCall(mbm.writeImagePixels, state.previewTexturePath, previous.bytes,
            previous.width, previous.height)
    end
    if state.meshPreviewTextureInfo and state.previewTexturePath then
        dpCall(function() return state.meshPreviewTextureInfo:reload(state.previewTexturePath) end)
    end
    if job.previousPreviewTextureIsUiCopy then
        if state.previewUiTexturePath then
            dpCall(mbm.writeImagePixels, state.previewUiTexturePath, previous.bytes,
                previous.width, previous.height)
            if state.previewTextureInfo then
                dpCall(function() return state.previewTextureInfo:reload(state.previewUiTexturePath) end)
            end
        end
        state.previewTextureIsUiCopy = true
        return
    end
    if state.previewTextureInfo and state.previewTextureIsUiCopy then
        dpCall(function() return state.previewTextureInfo:release() end)
        state.previewTextureInfo = nil
    end
    local loaded, textureInfo = loadTextureInfo(state.target.path)
    if loaded and textureInfo then
        state.previewTextureInfo = textureInfo
        state.previewTextureIsUiCopy = false
    end
end

local function cancelPreviewJob()
    local job = state.previewJob
    if not job then return false end
    restorePreviewBeforeJob(job)
    state.previewJob = nil
    state.previewCancelled = true
    state.previewDirty = true
    state.previewRequiresMesh = true
    state.status = tLang.L('mtp_preview_cancelled')
    return true
end

local function resumePreviewJob()
    state.previewCancelled = false
    state.previewDirty = true
    state.previewRequiresMesh = true
    state.status = tLang.L('mtp_preview_resumed')
end

local function processPreviewJob()
    if not (state.previewAsset and state.previewMesh and state.target) then
        state.previewJob = nil
        return
    end
    local job = state.previewJob
    if not job then return end
    if job.phase == 'compose' then
        if state.dilationRadius > 0 and state.layer and state.layer.touched then
            local paddedLayer = Raster.cloneLayer(state.layer)
            Raster.dilate(paddedLayer, state.dilationRadius)
            state.composite = Raster.compose(state.base, paddedLayer)
            state.compositeDirtyTiles = nil
        elseif state.compositeDirtyTiles and state.layer then
            if not state.composite then
                state.composite = Raster.newBuffer(state.base.width, state.base.height, state.base.bytes)
            end
            Raster.composeInto(state.base, state.layer, state.composite, state.compositeDirtyTiles)
            state.compositeDirtyTiles = nil
        elseif state.layer and not state.composite and state.layer.touched then
            state.composite = Raster.compose(state.base, state.layer)
        end
        job.progress, job.phase = .35, 'write'
        return
    end
    if job.phase == 'write' then
        job.texturePath = loadPreviewTexture()
        if not job.texturePath then
            state.previewJob = nil
            state.previewCancelled = true
            return
        end
        job.progress = .7
        -- Reloading pixels updates an already-bound TEXTURE. The first stroke
        -- switches from the source to the private paint texture, so it must
        -- also update the mesh binding before later strokes can reuse it.
        local needsBinding = state.previewBoundTexturePath ~= job.texturePath
        job.phase = (state.previewRequiresMesh or not state.previewReady or needsBinding) and 'mesh' or 'finish'
        return
    end
    if job.phase == 'mesh' then
        for _, subset in ipairs(state.target.subsets) do
            assert(state.previewAsset:setTexture(1, subset, job.texturePath))
        end
        assert(state.previewAsset:save(state.previewMeshPath, false, false, false))
        assert(meshDebug:loadMeshPreview(state.previewMesh, state.previewMeshPath))
        state.previewBoundTexturePath = job.texturePath
        stopPreviewAnimations(state.previewMesh)
        state.previewMeshReloads = state.previewMeshReloads + 1
        state.previewReady = true
        job.progress, job.phase = .9, 'finish'
        return
    end
    state.previewMesh.alwaysRender = true
    state.previewMesh.visible = true
    state.previewRequiresMesh = false
    state.previewDirty = false
    state.previewBuilds = state.previewBuilds + 1
    state.previewJob = nil
end

stopPreviewAnimations = function(previewMesh)
    if not previewMesh then return end
    dpCall(function() return previewMesh:stopSkeletalAnimation() end)
    dpCall(function() return previewMesh:stopSkeletalAnimationAbsoluteLayer() end)
    local ok, total = dpCall(function() return previewMesh:getTotalArticulatedAnimations() end)
    if not ok then return end
    for index = 1, tonumber(total) or 0 do
        local nameOk, name = dpCall(function()
            return previewMesh:getArticulatedAnimationName(index)
        end)
        if nameOk and type(name) == 'string' and name ~= '' then
            dpCall(function() return previewMesh:disableArticulatedAnimation(name) end)
        end
    end
end

local function loadMesh(path, preferredTexturePath)
    local fullPath = resolvePath(path) or path
    local directory = pathDirectory(fullPath)
    if directory then mbm.addPath(directory) end
    local source = meshDebug:new()
    assert(source:load(fullPath))
    assert(source:check())
    local frame = 1
    local snapshots, targetMap = {}, {}
    local totalSubsets = assert(source:getTotalSubset(frame))
    for subset = 1, totalSubsets do
        local snapshot, errorMessage = Model.snapshotSubset(source, frame, subset, dpCall)
        if not snapshot then error(errorMessage, 0) end
        snapshots[#snapshots + 1] = snapshot
        local texturePath = normalizedTexture(snapshot.texture)
        if texturePath then
            local target = targetMap[texturePath]
            if not target then
                target = {path=texturePath, subsets={}, triangles={}, subsetTriangles={}}
                targetMap[texturePath] = target
            end
            target.subsets[#target.subsets + 1] = subset
            target.subsetTriangles[subset] = {}
            for _, triangle in ipairs(snapshot.triangles) do
                triangle.subset = subset
                target.triangles[#target.triangles + 1] = triangle
                target.subsetTriangles[subset][#target.subsetTriangles[subset] + 1] = triangle
            end
        end
    end
    local targets = {}
    for _, target in pairs(targetMap) do targets[#targets + 1] = target end
    table.sort(targets, function(left, right) return left.path < right.path end)
    for _, target in ipairs(targets) do
        local islandInfo = Model.buildUvIslands(target.triangles)
        target.uvIslands = islandInfo.islands
        target.islandTriangles = {}
        target.uvIslandsBySubset = {}
        target.uvBoundaryEdgesByIsland = {}
        for index, islandId in ipairs(islandInfo.islandByTriangle) do
            local triangle = target.triangles[index]
            triangle.uvIsland = islandId
            target.islandTriangles[islandId] = target.islandTriangles[islandId] or {}
            target.islandTriangles[islandId][#target.islandTriangles[islandId] + 1] = triangle
            local subsetIslands = target.uvIslandsBySubset[triangle.subset]
            if not subsetIslands then
                subsetIslands = {}
                target.uvIslandsBySubset[triangle.subset] = subsetIslands
            end
            if #target.islandTriangles[islandId] == 1 then
                subsetIslands[#subsetIslands + 1] = target.uvIslands[islandId]
            end
        end
        target.uvBoundaryEdges = Model.buildUvBoundaryEdges(target.triangles)
        target.uvBoundaryEdgesBySubset = {}
        target.selectionTriangles = {}
        target.pickAccelerations = {}
        for subset, triangles in pairs(target.subsetTriangles) do
            target.uvBoundaryEdgesBySubset[subset] = Model.buildUvBoundaryEdges(triangles)
        end
        for islandId, triangles in pairs(target.islandTriangles) do
            target.uvBoundaryEdgesByIsland[islandId] = Model.buildUvBoundaryEdges(triangles)
        end
        local overlaps = Model.countUvOverlaps(target.triangles)
        target.uvOverlapCount = overlaps.count
        target.uvOverlapTruncated = overlaps.truncated
    end
    assert(#targets > 0, 'mesh has no file-backed primary texture with UVs')
    local initialTargetIndex = 1
    if preferredTexturePath then
        for index, target in ipairs(targets) do
            if target.path == preferredTexturePath then initialTargetIndex = index; break end
        end
        assert(targets[initialTargetIndex].path == preferredTexturePath,
            'preferred project texture is not used by the mesh')
    end
    local textureLoaded, textureInfo = loadTextureInfo(targets[initialTargetIndex].path)
    assert(textureLoaded, textureInfo or 'could not load the selected texture')
    local previewAsset = meshDebug:new()
    assert(previewAsset:load(fullPath))
    local previewMesh = mesh:new('3d')
    local previewMeshPath = tUtil.getTemporaryFilePath('.msh')
    local previewTexturePath = tUtil.getTemporaryFilePath('.png')
    local previewUiTexturePath = tUtil.getTemporaryFilePath('.png')

    clearPreview()
    state.meshPath, state.sourceAsset, state.snapshots = fullPath, source, snapshots
    state.targets = targets
    state.previewAsset = previewAsset
    state.previewMesh = previewMesh
    state.previewMeshPath = previewMeshPath
    state.previewTexturePath = previewTexturePath
    state.previewUiTexturePath = previewUiTexturePath
    state.paintThrough = false
    state.limitToSubset = false
    state.selectedSubset = 0
    state.limitToUvIsland = false
    state.selectedUvIsland = 0
    state.lastUvIslandHit = nil
    state.dilationRadius, state.pendingDilationRadius = 0, nil
    state.cameraOrbit = {fx=0,fy=0,fz=0,distance=5,azimuth=0,elevation=.2}
    state.camera:setFar(9999999)
    frameCamera()
    assert(selectTarget(initialTargetIndex, textureInfo))
    state.projectPath = nil
    state.modified = false
    state.status = string.format('Loaded %s. Paint with right drag; orbit with left drag.', fullPath)
end

local function beginStroke(hit, screenX, screenY)
    state.stroke = {before={}, tiles={}, changed=false, lastU=hit.uv.u, lastV=hit.uv.v,
        lastScreenX=screenX, lastScreenY=screenY, modifiedBefore=state.modified,
        exportDirtyBefore=state.exportDirty, previewDirtyBefore=state.previewDirty,
        previewRequiresMeshBefore=state.previewRequiresMesh, touchedBefore=state.layer.touched,
        compositeDirtyTilesBefore=state.compositeDirtyTiles, queue={{hit=hit, x=screenX, y=screenY}},
        queueHead=1, finishRequested=false}
    state.painting = true
    local dab = {x=hit.uv.u*(state.layer.width-1), y=hit.uv.v*(state.layer.height-1),
        radius=state.brush.radius}
    local tiles = Raster.tilesForDab(state.layer, dab.x, dab.y, dab.radius)
    for _, tile in ipairs(tiles) do
        if not state.stroke.before[tile.key] then
            state.stroke.before[tile.key] = Raster.snapshotTiles(state.layer, {tile}).tiles[1]
            state.stroke.tiles[#state.stroke.tiles+1] = tile
        end
    end
end

local function enqueueStrokeHit(hit, screenX, screenY)
    if not (state.stroke and hit and hit.uv) then return end
    local stroke = state.stroke
    local previous = stroke.queue[#stroke.queue]
    if previous then
        local dx, dy = screenX - previous.x, screenY - previous.y
        if dx * dx + dy * dy < 1 then return end
    end
    stroke.queue[#stroke.queue + 1] = {hit=hit, x=screenX, y=screenY}
end

local function applyStrokeHit(hit, screenX, screenY)
    if not (state.painting and hit and hit.uv and state.layer) then return end
    if state.brush.mode == 'stamp' and not state.stamp then return end
    local x = math.max(0, math.min(state.layer.width-1, hit.uv.u*(state.layer.width-1)))
    local y = math.max(0, math.min(state.layer.height-1, hit.uv.v*(state.layer.height-1)))
    local radius = state.brush.radius
    local projectedSamples
    local surfaceSamples
    local screenProjectedMode = state.brush.mode == 'project' or
        ((state.brush.mode == 'paint' or state.brush.mode == 'erase') and
            state.brush.shape == 'square')
    if screenProjectedMode and screenX and screenY then
        projectedSamples = {}
        local lastX = state.stroke.lastScreenX or screenX
        local lastY = state.stroke.lastScreenY or screenY
        local distance = math.sqrt((screenX - lastX) ^ 2 + (screenY - lastY) ^ 2)
        local steps = math.min(PROJECT_STROKE_MAX_STEPS,
            math.max(1, math.ceil(distance / PROJECT_SAMPLE_SPACING)))
        for step = 1, steps do
            local interpolation = step / steps
            local sampleX = lastX + (screenX - lastX) * interpolation
            local sampleY = lastY + (screenY - lastY) * interpolation
            local sampleHit = step == steps and hit or hitAtScreen(sampleX, sampleY)
            if sampleHit then
                local samples = projectSamplesAtScreen(sampleHit, sampleX, sampleY,
                    state.brush.mode == 'project' and state.paintThrough)
                for _, sample in ipairs(samples or {}) do projectedSamples[#projectedSamples + 1] = sample end
            end
        end
        state.stroke.lastScreenX, state.stroke.lastScreenY = screenX, screenY
    end
    if state.brush.mode == 'surface' then
        surfaceSamples = surfaceSamplesAtHit(hit, screenX, screenY)
    end
    local tiles = (projectedSamples or surfaceSamples) and
        Raster.tilesForPixels(state.layer, projectedSamples or surfaceSamples) or
        Raster.tilesForDab(state.layer, x, y, radius)
    for _, tile in ipairs(tiles) do
        if not state.stroke.before[tile.key] then
            state.stroke.before[tile.key] = Raster.snapshotTiles(state.layer, {tile}).tiles[1]
            state.stroke.tiles[#state.stroke.tiles+1] = tile
        end
    end
    local result
    if (state.brush.mode == 'project' or
            ((state.brush.mode == 'paint' or state.brush.mode == 'erase') and
                state.brush.shape == 'square')) and projectedSamples then
        result = Raster.applyProjectedSamples(state.layer, projectedSamples, state.brush.opacity,
            colorBytes(), true, state.brush.shape,
            state.brush.mode == 'erase' and 'erase' or 'paint')
    elseif state.brush.mode == 'surface' and surfaceSamples then
        result = Raster.applyProjectedSamples(state.layer, surfaceSamples, state.brush.opacity,
            colorBytes(), true, state.brush.shape)
    elseif state.brush.mode == 'stamp' and state.stamp then
        local destinationDerivatives = screenX and screenY and
            brushUvDerivatives(hit, screenX, screenY)
        local flipU = destinationDerivatives and
            ((destinationDerivatives.duDx < 0) ~= (state.stamp.sourceFlipU == true)) or false
        local flipV = destinationDerivatives and
            ((destinationDerivatives.dvDy < 0) ~= (state.stamp.sourceFlipV == true)) or false
        result = Raster.applyStamp(state.layer, state.stamp.patch, x, y, radius,
            state.brush.hardness, state.brush.opacity, true, flipU, flipV, state.brush.shape,
            state.stamp.scale or 1, math.rad(state.stamp.rotation or 0))
    elseif state.brush.mode == 'paint' or state.brush.mode == 'erase' then
        result = Raster.applyDab(state.layer, {x=x, y=y, radius=radius,
            hardness=state.brush.hardness, opacity=state.brush.opacity,
            color=colorBytes(), mode=state.brush.mode, shape=state.brush.shape, deferSync=true})
    end
    if not result then return end
    if result.changedPixels > 0 then
        state.stroke.changed = true
        state.previewDirty = true
        state.previewCancelled = false
        state.modified = true
    end
end

local function hitAtScreen(x, y)
    if not state.target then return nil end
    local ok, ox, oy, oz, dx, dy, dz = dpCall(mbm.getPickRay, x, y)
    if not ok then return nil end
    local origin, direction = {x=ox,y=oy,z=oz}, {x=dx,y=dy,z=dz}
    local triangles, acceleration = paintTriangles(), paintAcceleration()
    if acceleration then
        return Pick.pickTrianglesBVH(origin, direction, acceleration)
    end
    return Pick.pickTriangles(origin, direction, triangles)
end

local function hitsAtScreen(x, y)
    if not state.target then return {} end
    local ok, ox, oy, oz, dx, dy, dz = dpCall(mbm.getPickRay, x, y)
    if not ok then return {} end
    local origin, direction = {x=ox,y=oy,z=oz}, {x=dx,y=dy,z=dz}
    local triangles, acceleration = paintTriangles(), paintAcceleration()
    if acceleration then
        return Pick.pickTrianglesBVHAll(origin, direction, acceleration)
    end
    return Pick.pickTrianglesAll(origin, direction, triangles)
end

local function ensureTargetPickAcceleration()
    if not state.target then return end
    local key = paintSelectionKey()
    if not state.target.pickAccelerations[key] then
        state.target.pickAccelerations[key] = Pick.buildBVH(paintTriangles(), 8)
    end
end

local function currentVisibleBuffer()
    if state.dilationRadius > 0 and state.layer and state.layer.touched then
        local paddedLayer = Raster.cloneLayer(state.layer)
        Raster.dilate(paddedLayer, state.dilationRadius)
        return Raster.compose(state.base, paddedLayer)
    end
    if state.compositeDirtyTiles then
        if not state.composite then
            state.composite = Raster.newBuffer(state.base.width, state.base.height, state.base.bytes)
        end
        Raster.composeInto(state.base, state.layer, state.composite, state.compositeDirtyTiles)
        state.compositeDirtyTiles = nil
    elseif not state.composite and state.layer and state.layer.touched then
        state.composite = Raster.compose(state.base, state.layer)
    end
    return state.composite or state.base
end

local function updateHoverHit(x, y)
    local hit = hitAtScreen(x, y)
    state.hoverHit = hit
    if hit and hit.triangle and hit.triangle.uvIsland then
        state.lastUvIslandHit = hit
    end
    state.hoverPickPending = false
    state.hoverPickElapsed = 0
end

local function processHoverPick(delta)
    if not state.hoverPickPending or state.painting or state.orbiting or state.panning or state.previewJob then return end
    if imguiCapturesMouse() then return end
    state.hoverPickElapsed = state.hoverPickElapsed + math.max(0, math.min(0.25, tonumber(delta) or 0))
    if state.hoverPickElapsed < .12 then return end
    updateHoverHit(state.mouse.x, state.mouse.y)
end

local function hitSingleTriangleAtScreen(x, y, triangle)
    local ok, ox, oy, oz, dx, dy, dz = dpCall(mbm.getPickRay, x, y)
    if not ok then return nil end
    local hit = Pick.intersectRayTriangle({x=ox,y=oy,z=oz}, {x=dx,y=dy,z=dz},
        triangle.a, triangle.b, triangle.c)
    if not hit then return nil end
    hit.triangle = triangle
    hit.uv = Pick.interpolateUV(hit, triangle.uvA, triangle.uvB, triangle.uvC)
    if not hit.uv then return nil end
    return hit
end

brushUvDerivatives = function(hit, rayX, rayY)
    local triangle = hit.triangle
    if not triangle or not hit.uv then return nil end
    local sampleDistance = 2
    local left = hitSingleTriangleAtScreen(rayX - sampleDistance, rayY, triangle)
    local right = hitSingleTriangleAtScreen(rayX + sampleDistance, rayY, triangle)
    local up = hitSingleTriangleAtScreen(rayX, rayY - sampleDistance, triangle)
    local down = hitSingleTriangleAtScreen(rayX, rayY + sampleDistance, triangle)
    local function derivative(minus, plus, center, component)
        if minus and plus then
            return (plus.uv[component] - minus.uv[component]) / (sampleDistance * 2)
        end
        if plus then return (plus.uv[component] - center.uv[component]) / sampleDistance end
        if minus then return (center.uv[component] - minus.uv[component]) / sampleDistance end
        return nil
    end
    local duDx = derivative(left, right, hit, 'u')
    local dvDx = derivative(left, right, hit, 'v')
    local duDy = derivative(up, down, hit, 'u')
    local dvDy = derivative(up, down, hit, 'v')
    if not (duDx and dvDx and duDy and dvDy) then return nil end
    local determinant = duDx * dvDy - duDy * dvDx
    if math.abs(determinant) < 1e-10 then return nil end
    return {duDx=duDx, dvDx=dvDx, duDy=duDy, dvDy=dvDy, determinant=determinant}
end

local function brushCursorPoints(hit, rayX, rayY, drawX, drawY)
    local derivatives = brushUvDerivatives(hit, rayX, rayY)
    if not derivatives then return nil end
    local radiusU = state.brush.radius / math.max(1, state.textureWidth - 1)
    local radiusV = state.brush.radius / math.max(1, state.textureHeight - 1)
    local points = {}
    local shape = state.brush.shape == 'square' and 'square' or 'circle'
    local count = shape == 'square' and 4 or 32
    for index = 0, count do
        local targetU, targetV
        if shape == 'square' then
            local corner = index % 4
            targetU = (corner == 0 or corner == 3) and -radiusU or radiusU
            targetV = (corner == 0 or corner == 1) and -radiusV or radiusV
        else
            local angle = math.pi * 2 * index / 32
            targetU = math.cos(angle) * radiusU
            targetV = math.sin(angle) * radiusV
        end
        local screenX = (targetU * derivatives.dvDy - targetV * derivatives.duDy) /
            derivatives.determinant
        local screenY = (targetV * derivatives.duDx - targetU * derivatives.dvDx) /
            derivatives.determinant
        points[#points + 1] = {x=drawX + screenX, y=drawY + screenY}
    end
    return points, derivatives
end

local function equivalentScreenRadius(points, center)
    if not points or #points < 2 then return nil end
    local total = 0
    local count = #points - 1
    for index = 1, count do
        local point = points[index]
        local dx, dy = point.x - center.x, point.y - center.y
        total = total + dx * dx + dy * dy
    end
    return math.sqrt(total / count)
end

local function cachedBrushCursorGeometry(hit, rayX, rayY, drawX, drawY)
    local cache = state.cursorCache
    if cache.hit == hit and cache.rayX == rayX and cache.rayY == rayY and
            cache.drawX == drawX and cache.drawY == drawY and
            cache.radius == state.brush.radius and cache.textureWidth == state.textureWidth and
            cache.textureHeight == state.textureHeight and cache.shape == state.brush.shape and
            cache.cameraRevision == state.cameraRevision then
        return cache.points, cache.screenRadius
    end
    local points = brushCursorPoints(hit, rayX, rayY, drawX, drawY)
    local screenRadius = points and equivalentScreenRadius(points, {x=drawX,y=drawY}) or nil
    cache.hit, cache.rayX, cache.rayY = hit, rayX, rayY
    cache.drawX, cache.drawY = drawX, drawY
    cache.radius = state.brush.radius
    cache.textureWidth, cache.textureHeight = state.textureWidth, state.textureHeight
    cache.shape = state.brush.shape
    cache.cameraRevision = state.cameraRevision
    cache.points, cache.screenRadius = points, screenRadius
    return points, screenRadius
end

local function pickColorAtHit(hit)
    if not (hit and hit.uv and state.layer and state.base) then
        state.status = tLang.L('mtp_pick_no_surface')
        return
    end
    local buffer = currentVisibleBuffer()
    local x = hit.uv.u * (buffer.width - 1)
    local y = hit.uv.v * (buffer.height - 1)
    local color = Raster.sample(buffer, x, y)
    state.brush.color = {r=color.r / 255, g=color.g / 255, b=color.b / 255, a=color.a / 255}
    state.status = string.format(tLang.L('mtp_color_picked'), color.r, color.g, color.b)
end

local function pointInsidePolygon(x, y, points)
    local inside = false
    local previous = points[#points]
    for _, current in ipairs(points) do
        local crosses = (current.y > y) ~= (previous.y > y)
        if crosses then
            local intersection = (previous.x - current.x) * (y - current.y) /
                (previous.y - current.y) + current.x
            if x < intersection then inside = not inside end
        end
        previous = current
    end
    return inside
end

projectSamplesAtScreen = function(hit, screenX, screenY, paintThrough)
    ensureTargetPickAcceleration()
    local footprint = brushCursorPoints(hit, screenX, screenY, screenX, screenY)
    if not footprint then return nil end
    local screenRadius = equivalentScreenRadius(footprint, {x=screenX,y=screenY})
    if not screenRadius or screenRadius <= 0 then return nil end
    local diameter = screenRadius * 2
    local side = math.min(PROJECT_SAMPLE_MAX,
        math.max(9, math.ceil(diameter / PROJECT_SAMPLE_SPACING) + 1))
    if side % 2 == 0 then side = math.min(PROJECT_SAMPLE_MAX, side + 1) end
    local sampleSpacing = diameter / math.max(1, side - 1)
    local sampleRadius = state.brush.radius * 1.25 / math.max(1, side - 1)
    local derivatives = brushUvDerivatives(hit, screenX, screenY)
    if derivatives then
        local duPerScreenPixel = math.sqrt(derivatives.duDx * derivatives.duDx +
            derivatives.duDy * derivatives.duDy)
        local dvPerScreenPixel = math.sqrt(derivatives.dvDx * derivatives.dvDx +
            derivatives.dvDy * derivatives.dvDy)
        local texturePixelsPerScreenPixel = math.max(
            duPerScreenPixel * math.max(1, state.textureWidth - 1),
            dvPerScreenPixel * math.max(1, state.textureHeight - 1))
        sampleRadius = math.max(sampleRadius,
            sampleSpacing * texturePixelsPerScreenPixel * math.sqrt(2) * .55)
    end
    sampleRadius = math.max(1, sampleRadius)
    local samples = {}
    for row = 0, side - 1 do
        local sampleY = screenY - screenRadius + diameter * row / (side - 1)
        for column = 0, side - 1 do
            local sampleX = screenX - screenRadius + diameter * column / (side - 1)
            local normalizedX = (sampleX - screenX) / screenRadius
            local normalizedY = (sampleY - screenY) / screenRadius
            local squareDistance = math.max(math.abs(normalizedX), math.abs(normalizedY)) * math.sqrt(2)
            local distance = state.brush.shape == 'square' and math.min(1, squareDistance) or
                math.sqrt(normalizedX * normalizedX + normalizedY * normalizedY)
            local inside = state.brush.shape == 'square' and
                pointInsidePolygon(sampleX, sampleY, footprint) or distance <= 1
            if inside then
                local sampleHits = paintThrough == true and hitsAtScreen(sampleX, sampleY) or
                    {hitAtScreen(sampleX, sampleY)}
                local inner = state.brush.hardness
                local amount = distance <= inner and 1 or
                    (inner >= 1 and 0 or math.max(0, 1 - (distance - inner) / (1 - inner)))
                if amount > 0 then
                    for _, sampleHit in ipairs(sampleHits) do
                        if sampleHit and sampleHit.uv then
                            samples[#samples + 1] = {
                                x = sampleHit.uv.u * (state.layer.width - 1),
                                y = sampleHit.uv.v * (state.layer.height - 1),
                                amount = amount,
                                radius = sampleRadius,
                                shape = state.brush.shape,
                            }
                        end
                    end
                end
            end
        end
    end
    return samples
end

surfaceSamplesAtHit = function(hit, screenX, screenY)
    if not (hit and hit.position and hit.uv and state.target) then return {} end
    ensureTargetPickAcceleration()
    local surfaceRadius = state.meshDiagonal * state.brush.radius /
        math.max(1, math.min(state.textureWidth, state.textureHeight))
    local footprint = screenX and brushCursorPoints(hit, screenX, screenY, screenX, screenY)
    if footprint then
        for index = 1, #footprint, 4 do
            local edgeHit = hitAtScreen(footprint[index].x, footprint[index].y)
            if edgeHit and edgeHit.position then
                local dx = edgeHit.position.x - hit.position.x
                local dy = edgeHit.position.y - hit.position.y
                local dz = edgeHit.position.z - hit.position.z
                surfaceRadius = math.max(surfaceRadius, math.sqrt(dx * dx + dy * dy + dz * dz))
            end
        end
    end
    surfaceRadius = math.max(surfaceRadius, state.meshDiagonal * .0001)
    local triangles, acceleration = paintTriangles(), paintAcceleration()
    local basis = Pick.surfaceBrushBasis(hit.triangle)
    local candidates = Pick.queryTrianglesSphere(hit.position, surfaceRadius,
        acceleration, triangles)
    local samples = Pick.surfaceSamplesAtPoint(hit.position, surfaceRadius,
        triangles, state.layer.width, state.layer.height, {
            candidates = candidates,
            hardness = state.brush.hardness,
            shape = state.brush.shape,
            tangent = basis and basis.tangent,
            bitangent = basis and basis.bitangent,
            maxSamples = 65536,
        })
    if #samples == 0 then
        samples[1] = {
            x = hit.uv.u * (state.layer.width - 1),
            y = hit.uv.v * (state.layer.height - 1),
            amount = 1,
            radius = 0,
        }
    end
    return samples
end

local function captureStampFromView(hit, screenX, screenY, buffer, radius)
    local footprint, derivatives = brushCursorPoints(hit, screenX, screenY, screenX, screenY)
    if not footprint then return nil end
    local minX, maxX, minY, maxY = screenX, screenX, screenY, screenY
    for _, point in ipairs(footprint) do
        minX, maxX = math.min(minX, point.x), math.max(maxX, point.x)
        minY, maxY = math.min(minY, point.y), math.max(maxY, point.y)
    end
    local maxResolution = state.brush.stampResolution or 65
    local side = math.floor(math.max(STAMP_RESOLUTION_MIN,
        math.min(STAMP_RESOLUTION_MAX, maxResolution, radius * 2 + 1)) + .5)
    if side % 2 == 0 then side = side - 1 end
    local sourceRadius = (side - 1) * .5
    local rows = {}
    for row = 0, side - 1 do
        local normalizedY = row / (side - 1)
        local sampleY = minY + (maxY - minY) * normalizedY
        local pixels = {}
        for column = 0, side - 1 do
            local normalizedX = column / (side - 1)
            local sampleX = minX + (maxX - minX) * normalizedX
            local pixel
            if pointInsidePolygon(sampleX, sampleY, footprint) then
                local sampleHit = hitAtScreen(sampleX, sampleY)
                if sampleHit and sampleHit.uv then
                    local x = sampleHit.uv.u * (buffer.width - 1)
                    local y = sampleHit.uv.v * (buffer.height - 1)
                    local color = Raster.sample(buffer, x, y)
                    pixel = string.char(color.r, color.g, color.b, color.a)
                end
            end
            pixels[#pixels + 1] = pixel or string.char(0, 0, 0, 0)
        end
        rows[#rows + 1] = table.concat(pixels)
    end
    return {width=side, height=side, radius=sourceRadius, bytes=table.concat(rows), screenSpace=true,
        sourceFlipU=derivatives.duDx < 0, sourceFlipV=derivatives.dvDy < 0}
end

local function pickStampSource(hit, screenX, screenY)
    if not (hit and hit.uv and state.layer and state.base) then
        state.status = tLang.L('mtp_pick_no_surface')
        return
    end
    local buffer = currentVisibleBuffer()
    local x = hit.uv.u * (buffer.width - 1)
    local y = hit.uv.v * (buffer.height - 1)
    local radius = math.max(1, math.floor(state.brush.radius + .5))
    ensureTargetPickAcceleration()
    local patch = captureStampFromView(hit, screenX, screenY, buffer, radius)
    if not patch then
        patch = Raster.extractStamp(buffer, x, y, radius)
        patch.sourceFlipU, patch.sourceFlipV = false, false
    end
    clearStamp()
    state.stamp = {x=x, y=y, radius=radius, patch=patch, source='view', scale=1, rotation=0}
    state.stampTexturePath = tUtil.getTemporaryFilePath('.png')
    local saved, errorMessage = dpCall(mbm.writeImagePixels, state.stampTexturePath,
        patch.bytes, patch.width, patch.height)
    if not saved then
        clearStamp()
        state.status = 'Could not create stamp preview: ' .. tostring(errorMessage)
        return
    end
    local loaded, textureInfo = loadTextureInfo(state.stampTexturePath)
    if not loaded or not textureInfo then
        clearStamp()
        state.status = 'Could not load stamp preview: ' .. tostring(textureInfo)
        return
    end
    state.stampTextureInfo = textureInfo
    state.status = tLang.L('mtp_stamp_selected')
end

local function openExternalStamp()
    local path = mbm.openFile(state.meshPath or '', table.unpack(tUtil.supported_images))
    if not path then return end
    local ok, bytes, width, height = dpCall(mbm.readImagePixels, path)
    if not ok or type(bytes) ~= 'string' or not width or not height or width < 1 or height < 1 then
        state.status = tLang.L('mtp_stamp_external_load_failed')
        return
    end
    local loaded, textureInfo = loadTextureInfo(path)
    if not loaded or not textureInfo then
        state.status = tLang.L('mtp_stamp_external_load_failed')
        return
    end
    clearExternalStamp()
    state.externalStamp = {path=path, bytes=bytes, width=width, height=height, textureInfo=textureInfo}
    state.stampOverlay = true
    state.stampSelection = nil
    state.stampSelectionDrag = nil
    state.status = tLang.L('mtp_stamp_external_help')
end

local function finishExternalStampSelection()
    local source = state.externalStamp
    local selection = state.stampSelection
    if not (source and selection) then
        state.status = tLang.L('mtp_stamp_selection_invalid')
        return
    end
    local patch = Stamp.crop(source.bytes, source.width, source.height, selection)
    local sourcePath = source.path
    clearStamp()
    state.stamp = {x=0, y=0, radius=patch.radius, patch=patch, source='external', scale=1,
        rotation=0, sourcePath=sourcePath, selection=selection}
    state.stampTexturePath = tUtil.getTemporaryFilePath('.png')
    local saved, errorMessage = dpCall(mbm.writeImagePixels, state.stampTexturePath,
        patch.bytes, patch.width, patch.height)
    if not saved then
        clearStamp()
        state.status = 'Could not create stamp preview: ' .. tostring(errorMessage)
        return
    end
    local loaded, textureInfo = loadTextureInfo(state.stampTexturePath)
    if not loaded or not textureInfo then
        clearStamp()
        state.status = 'Could not load stamp preview: ' .. tostring(textureInfo)
        return
    end
    state.stampTextureInfo = textureInfo
    clearExternalStamp()
    state.status = tLang.L('mtp_stamp_selected')
end

local function cancelExternalStampSelection()
    clearExternalStamp()
    state.status = tLang.L('mtp_stamp_external_cancelled')
end

local function showExternalStampOverlay()
    local source = state.externalStamp
    if not (state.stampOverlay and source and source.textureInfo) then return end
    local screenWidth, screenHeight = mbm.getSizeScreen()
    local imageAreaWidth = math.max(64, screenWidth - 80)
    local imageAreaHeight = math.max(64, screenHeight - 170)
    local scale = math.min(imageAreaWidth / source.width, imageAreaHeight / source.height)
    local imageWidth, imageHeight = source.width * scale, source.height * scale
    local imageX = (screenWidth - imageWidth) * .5
    local imageY = 48 + (imageAreaHeight - imageHeight) * .5
    local flags = {'ImGuiWindowFlags_NoDecoration', 'ImGuiWindowFlags_NoSavedSettings',
        'ImGuiWindowFlags_NoNav', 'ImGuiWindowFlags_NoBringToFrontOnFocus'}
    tImGui.SetNextWindowPos({x=0,y=0}, tImGui.Flags('ImGuiCond_Always'), {x=0,y=0})
    tImGui.SetNextWindowSize({x=screenWidth,y=screenHeight}, tImGui.Flags('ImGuiCond_Always'))
    tImGui.SetNextWindowBgAlpha(.94)
    local opened = tImGui.Begin('##mtpExternalStamp', false, tImGui.Flags(flags))
    if opened then
        tImGui.SetCursorScreenPos({x=imageX,y=imageY})
        local imageOrigin = tImGui.GetCursorScreenPos()
        local cell = 12
        for row = 0, math.ceil(imageHeight / cell) - 1 do
            for column = 0, math.ceil(imageWidth / cell) - 1 do
                local even = (row + column) % 2 == 0
                tImGui.AddRectFilled(
                    {x=imageOrigin.x + column * cell, y=imageOrigin.y + row * cell},
                    {x=math.min(imageOrigin.x + (column + 1) * cell, imageOrigin.x + imageWidth),
                        y=math.min(imageOrigin.y + (row + 1) * cell, imageOrigin.y + imageHeight)},
                    even and {r=.22,g=.22,b=.22,a=1} or {r=.12,g=.12,b=.12,a=1})
            end
        end
        tImGui.Image(source.textureInfo, {x=imageWidth,y=imageHeight})

        local mouse = tImGui.GetMousePos()
        local inside = mouse and mouse.x >= imageOrigin.x and mouse.x <= imageOrigin.x + imageWidth and
            mouse.y >= imageOrigin.y and mouse.y <= imageOrigin.y + imageHeight
        local function imageCoordinate(value, origin, extent, maximum)
            return math.floor(math.max(0, math.min(maximum, (value - origin) / extent * maximum)) + .5)
        end
        if inside and tImGui.IsMouseClicked(0, false) then
            state.stampSelectionDrag = {
                x=imageCoordinate(mouse.x, imageOrigin.x, imageWidth, source.width - 1),
                y=imageCoordinate(mouse.y, imageOrigin.y, imageHeight, source.height - 1)}
            state.stampSelection = nil
        end
        if state.stampSelectionDrag and tImGui.IsMouseDown(0) and mouse then
            local x = imageCoordinate(mouse.x, imageOrigin.x, imageWidth, source.width - 1)
            local y = imageCoordinate(mouse.y, imageOrigin.y, imageHeight, source.height - 1)
            state.stampSelection = Stamp.normalizeSelection(state.stampSelectionDrag.x,
                state.stampSelectionDrag.y, x, y, source.width, source.height,
                state.stampSelectionShape)
        elseif state.stampSelectionDrag and tImGui.IsMouseReleased(0) then
            state.stampSelectionDrag = nil
        end
        if state.stampSelection then
            local selection = state.stampSelection
            local sourceSpanX, sourceSpanY = math.max(1, source.width - 1), math.max(1, source.height - 1)
            local minimum = {x=imageOrigin.x + selection.x / sourceSpanX * imageWidth,
                y=imageOrigin.y + selection.y / sourceSpanY * imageHeight}
            local maximum = {x=imageOrigin.x + (selection.x + selection.width - 1) / sourceSpanX * imageWidth,
                y=imageOrigin.y + (selection.y + selection.height - 1) / sourceSpanY * imageHeight}
            tImGui.SetImDrawListToForeground(true)
            if selection.shape == 'circle' then
                local center = {x=(minimum.x + maximum.x) * .5, y=(minimum.y + maximum.y) * .5}
                tImGui.AddCircle(center, math.min(maximum.x - minimum.x, maximum.y - minimum.y) * .5,
                    {r=1,g=.1,b=.1,a=1}, 64, 3)
            else
                tImGui.AddRect(minimum, maximum, {r=1,g=.1,b=.1,a=1}, 0, 0, 3)
            end
            tImGui.SetImDrawListToForeground(false)
        end
        tImGui.SetCursorScreenPos({x=32,y=screenHeight - 100})
        tImGui.Text(tLang.L('mtp_stamp_selection_shape'))
        local shapeIndex = state.stampSelectionShape == 'circle' and 2 or 1
        local oldShapeIndex = shapeIndex
        shapeIndex = tImGui.RadioButton(tLang.L('mtp_stamp_selection_square'), shapeIndex, 1)
        tImGui.SameLine()
        shapeIndex = tImGui.RadioButton(tLang.L('mtp_stamp_selection_circle'), shapeIndex, 2)
        if shapeIndex ~= oldShapeIndex then
            state.stampSelectionShape = shapeIndex == 2 and 'circle' or 'square'
            state.stampSelection = nil
        end
        tImGui.SameLine()
        local canConfirm = state.stampSelection ~= nil
        if tImGui.Button(tLang.L('mtp_stamp_confirm_selection'), {x=150,y=0}) and canConfirm then
            finishExternalStampSelection()
        end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('mtp_stamp_cancel_selection'), {x=100,y=0}) then
            cancelExternalStampSelection()
        end
    end
    tImGui.End()
end

local function showBrushCursor()
    if not (state.target and state.hoverHit and state.textureWidth > 0 and state.textureHeight > 0) then return end
    if state.painting or state.previewJob or tImGui.GetWantCaptureMouse() then return end
    local mouse = tImGui.GetMousePos()
    if not mouse or not tonumber(mouse.x) or not tonumber(mouse.y) or
            mouse.x < -100000 or mouse.y < -100000 then return end
    local paintColor = {r=state.brush.color.r or 1,g=state.brush.color.g or 0,
        b=state.brush.color.b or 0,a=.95}
    local highContrast = state.brush.highContrastCursor == true
    local color = state.brush.mode == 'pick' and {r=1,g=.8,b=.2,a=.95} or paintColor
    if highContrast and state.brush.mode ~= 'pick' then
        color = {r=state.brush.cursorColor.r or 1, g=state.brush.cursorColor.g or 0,
            b=state.brush.cursorColor.b or 1, a=.95}
    end
    local outlineColor = {r=0,g=0,b=0,a=.95}
    tImGui.SetImDrawListToForeground(true)
    local points = cachedBrushCursorGeometry(state.hoverHit,
        state.mouse.x, state.mouse.y, mouse.x, mouse.y)
    if points then
        if highContrast then tImGui.AddPolyline(points, outlineColor, true, 4) end
        tImGui.AddPolyline(points, color, true, 1.8)
    else
        local width, height = mbm.getSizeScreen()
        local textureSize = math.max(state.textureWidth, state.textureHeight)
        local meshScale = state.meshDiagonal / math.max(state.cameraOrbit.distance * 1.6, .001)
        meshScale = math.max(.15, math.min(1, meshScale))
        local projectedMeshSize = math.min(width, height) * meshScale
        local radius = math.max(4, math.min(256,
            state.brush.radius / textureSize * projectedMeshSize))
        local center = {x=mouse.x,y=mouse.y}
        local shape = state.brush.shape == 'square' and 'square' or 'circle'
        if shape == 'square' then
            local minimum = {x=center.x - radius, y=center.y - radius}
            local maximum = {x=center.x + radius, y=center.y + radius}
            if highContrast then tImGui.AddRect(minimum, maximum, outlineColor, 0, 0, 4) end
            tImGui.AddRect(minimum, maximum, color, 0, 0, 1.8)
        else
            if highContrast then tImGui.AddCircle(center, radius, outlineColor, 32, 4) end
            tImGui.AddCircle(center, radius, color, 32, 1.8)
        end
    end
    tImGui.SetImDrawListToForeground(false)
end

local function showBrushColorPreview()
    local color = state.brush.color
    local width, height, segments = 144, 16, 24
    local origin = tImGui.GetCursorScreenPos()
    local effectiveAlpha = (color.a or 1) * state.brush.opacity
    local hardness = state.brush.hardness
    for index = 0, segments - 1 do
        local start = index / segments
        local finish = (index + 1) / segments
        local position = (start + finish) * .5
        local falloff = position <= hardness and 1 or
            (hardness >= 1 and 0 or 1 - (position - hardness) / (1 - hardness))
        tImGui.AddRectFilled(
            {x=origin.x + start * width, y=origin.y},
            {x=origin.x + finish * width, y=origin.y + height},
            {r=color.r or 1, g=color.g or 1, b=color.b or 1, a=effectiveAlpha * falloff})
    end
    local hardnessX = origin.x + hardness * width
    tImGui.AddLine({x=hardnessX,y=origin.y}, {x=hardnessX,y=origin.y + height},
        {r=1,g=1,b=1,a=.8}, 1)
    tImGui.Dummy({x=width, y=height})
    tImGui.Text(string.format(tLang.L('mtp_effective_alpha'), effectiveAlpha))
end

local function showCheckerboard(size)
    local origin = tImGui.GetCursorScreenPos()
    local cell = 8
    local rows = math.ceil(size / cell)
    for row = 0, rows - 1 do
        for column = 0, rows - 1 do
            local even = (row + column) % 2 == 0
            local color = even and {r=.22,g=.22,b=.22,a=1} or {r=.12,g=.12,b=.12,a=1}
            tImGui.AddRectFilled(
                {x=origin.x + column * cell, y=origin.y + row * cell},
                {x=math.min(origin.x + (column + 1) * cell, origin.x + size),
                    y=math.min(origin.y + (row + 1) * cell, origin.y + size)}, color)
        end
    end
end

local function showUvOverlay(origin, size)
    if not (state.target and state.target.triangles) then return end
    local triangles = paintTriangles()
    local step = math.max(1, math.ceil(#triangles / UV_OVERLAY_MAX_TRIANGLES))
    local color = {r=.2,g=.85,b=1,a=.7}
    local function drawTriangle(triangle, lineColor, thickness)
        local a, b, c = triangle.uvA, triangle.uvB, triangle.uvC
        if not (a and b and c) then return end
        local pa = {x=origin.x + a.u * size, y=origin.y + a.v * size}
        local pb = {x=origin.x + b.u * size, y=origin.y + b.v * size}
        local pc = {x=origin.x + c.u * size, y=origin.y + c.v * size}
        tImGui.AddLine(pa, pb, lineColor, thickness)
        tImGui.AddLine(pb, pc, lineColor, thickness)
        tImGui.AddLine(pc, pa, lineColor, thickness)
    end
    for index = 1, #triangles, step do
        drawTriangle(triangles[index], color, 1)
    end
    local boundaryEdges = state.limitToUvIsland and
        state.target.uvBoundaryEdgesByIsland[state.selectedUvIsland] or
        state.limitToSubset and state.target.uvBoundaryEdgesBySubset[state.selectedSubset] or
        state.target.uvBoundaryEdges or {}
    local boundaryStep = math.max(1, math.ceil(#boundaryEdges / UV_OVERLAY_MAX_BOUNDARY_EDGES))
    local boundaryColor = {r=1,g=.45,b=.05,a=.95}
    for index = 1, #boundaryEdges, boundaryStep do
        local edge = boundaryEdges[index]
        local first = {x=origin.x + edge.a.u * size, y=origin.y + edge.a.v * size}
        local second = {x=origin.x + edge.b.u * size, y=origin.y + edge.b.v * size}
        tImGui.AddLine(first, second, boundaryColor, 2)
    end
    local hoveredIslandId = uvIslandFromHit(state.lastUvIslandHit)
    local highlightedIslandId = state.limitToUvIsland and state.selectedUvIsland or hoveredIslandId
    if highlightedIslandId and state.target.islandTriangles[highlightedIslandId] then
        local highlightColor = highlightedIslandId == hoveredIslandId and
            {r=1,g=.9,b=.1,a=1} or {r=1,g=.2,b=.8,a=1}
        local highlighted = state.target.islandTriangles[highlightedIslandId]
        local highlightStep = math.max(1, math.ceil(#highlighted / UV_OVERLAY_MAX_TRIANGLES))
        for index = 1, #highlighted, highlightStep do
            drawTriangle(highlighted[index], highlightColor, 2)
        end
    end
    if state.hoverHit and state.hoverHit.uv then
        local uv = state.hoverHit.uv
        local point = {x=origin.x + uv.u * size, y=origin.y + uv.v * size}
        local radius = math.max(3, state.brush.radius * size /
            math.max(1, math.min(state.textureWidth, state.textureHeight)))
        tImGui.AddCircle(point, radius, {r=1,g=.2,b=.1,a=1}, 24, 2)
    end
end

local function finishStroke()
    if not state.stroke then state.painting = false; return end
    if state.stroke.changed and #state.stroke.tiles > 0 then
        local after = Raster.snapshotTiles(state.layer, state.stroke.tiles)
        local before = {width=state.layer.width,height=state.layer.height,tiles={}}
        for _, tile in pairs(state.stroke.before) do before.tiles[#before.tiles+1] = tile end
        state.undo[#state.undo+1] = {before=before, after=after}
        state.redo = {}
        state.compositeDirtyTiles = state.stroke.tiles
        state.exportDirty = true
    end
    state.stroke, state.painting = nil, false
    if state.previewDirty then state.status = 'Stroke applied. Preview updated.' end
end

local function cancelStroke()
    local stroke = state.stroke
    if not stroke then state.painting = false; return false end
    if stroke.changed and #stroke.tiles > 0 then
        local before = {width=state.layer.width, height=state.layer.height, tiles={}}
        for _, tile in pairs(stroke.before) do before.tiles[#before.tiles + 1] = tile end
        Raster.restoreTiles(state.layer, before)
        local dirtyByKey = {}
        local dirtyTiles = {}
        for _, tile in ipairs(stroke.compositeDirtyTilesBefore or {}) do
            if not dirtyByKey[tile.key] then dirtyByKey[tile.key] = true; dirtyTiles[#dirtyTiles + 1] = tile end
        end
        for _, tile in ipairs(stroke.tiles) do
            if not dirtyByKey[tile.key] then dirtyByKey[tile.key] = true; dirtyTiles[#dirtyTiles + 1] = tile end
        end
        state.compositeDirtyTiles = dirtyTiles
        state.previewDirty = true
        state.previewRequiresMesh = stroke.previewRequiresMeshBefore
    else
        state.compositeDirtyTiles = stroke.compositeDirtyTilesBefore
        state.previewDirty = stroke.previewDirtyBefore
        state.previewRequiresMesh = stroke.previewRequiresMeshBefore
    end
    state.layer.touched = stroke.touchedBefore
    state.modified = stroke.modifiedBefore
    state.exportDirty = stroke.exportDirtyBefore
    state.stroke, state.painting = nil, false
    state.status = tLang.L('mtp_stroke_cancelled')
    return true
end

local function processPendingStroke()
    local stroke = state.stroke
    if not stroke then return end
    local processed = 0
    while stroke.queueHead <= #stroke.queue and processed < STROKE_POINTS_PER_FRAME do
        local point = stroke.queue[stroke.queueHead]
        stroke.queueHead = stroke.queueHead + 1
        applyStrokeHit(point.hit, point.x, point.y)
        processed = processed + 1
    end
    if stroke.queueHead > #stroke.queue then
        stroke.queue, stroke.queueHead = {}, 1
        if stroke.finishRequested then finishStroke() end
    end
end

local function history(redo)
    local source = redo and state.redo or state.undo
    local destination = redo and state.undo or state.redo
    local action = table.remove(source)
    if not action then return end
    local from = redo and action.after or action.before
    local to = redo and action.before or action.after
    Raster.restoreTiles(state.layer, from)
    destination[#destination+1] = {before=to, after=from}
    state.compositeDirtyTiles = from.tiles
    state.previewDirty = true
    state.previewCancelled = false
    state.modified = true
    state.exportDirty = true
end

local function withExtension(path, extension)
    if path:sub(-#extension):lower() == extension:lower() then return path end
    return path .. extension
end

local function suggestedProjectPath()
    local source = state.meshPath or 'mesh-texture-paint'
    local directory, name = source:match('^(.*[/\\])([^/\\]+)$')
    directory, name = directory or '', name or source
    local stem = name:gsub('%.[^%.]+$', '')
    return directory .. stem .. '.meshpaint.lua'
end

local function paintLayerPath(projectPath)
    return projectPath:gsub('%.lua$', '') .. '.paint.png'
end

local function makeProject(projectPath)
    local firstSubset = state.target and state.target.subsets[1] or 1
    return {
        version = Project.VERSION,
        source_mesh = resolvePath(state.meshPath) or state.meshPath,
        source_texture = resolvePath(state.target.path) or state.target.path,
        paint_layer = paintLayerPath(projectPath),
        working_texture = (state.exportTexturePath and not state.exportDirty) and state.exportTexturePath or nil,
        frame = 1,
        subset = state.selectedSubset > 0 and state.selectedSubset or firstSubset,
        texture_role = 'base_color',
        uv_channel = 0,
        projection_mode = state.brush.mode == 'project' and
            (state.paintThrough and 'project_stencil_through' or 'project_stencil') or
            state.brush.mode == 'surface' and 'surface_3d' or 'uv_brush',
        dilation_radius = state.dilationRadius,
        brush_shape = state.brush.shape == 'square' and 'square' or 'circle',
        limit_to_subset = state.limitToSubset,
        limit_to_uv_island = state.limitToUvIsland,
        uv_island = state.selectedUvIsland > 0 and state.selectedUvIsland or 1,
    }
end

local function isSourcePath(path)
    if path == resolvePath(state.meshPath) then return true end
    for _, target in ipairs(state.targets) do
        if path == resolvePath(target.path) then return true end
    end
    return false
end

local function saveProject(path)
    if not (state.target and state.meshPath) then
        state.status = 'Open a mesh and select a texture before saving a project.'
        return false
    end
    if not ensureEditableTexture() then return false end
    path = withExtension(resolvePath(path) or path, '.lua')
    local project = makeProject(path)
    if isSourcePath(path) or isSourcePath(project.paint_layer) then
        state.status = tLang.L('mtp_export_source_protected')
        return false
    end
    local bytes = Raster.toBytes(state.layer)
    local saved, errorMessage = PaintIO.publish({
        {path=project.paint_layer, write=function(temporary)
            return mbm.writeImagePixels(temporary, bytes, state.layer.width, state.layer.height)
        end},
        {path=path, write=function(temporary) return Project.saveLua(project, temporary) end},
    }, dpCall)
    if not saved then
        state.status = tLang.L('mtp_project_save_failed') .. ': ' .. tostring(errorMessage)
        return false
    end
    state.projectPath, state.modified = path, false
    state.status = tLang.L('mtp_project_saved') .. ': ' .. path .. (errorMessage and ('; ' .. errorMessage) or '')
    return true
end

local function saveProjectAs()
    local path = mbm.saveFile(state.projectPath or suggestedProjectPath(), 'lua')
    if not path or path == '' then return false end
    local called, saved = dpCall(saveProject, path)
    return called and saved == true
end

local function outputBaseName(path, suffix, extension)
    local directory, name = path:match('^(.*[/\\])([^/\\]+)$')
    directory, name = directory or '', name or path
    local stem = name:gsub('%.[^%.]+$', '')
    return directory .. stem .. suffix .. extension
end

local function suggestedTextureExportPath()
    return outputBaseName(state.target.path, '_painted', '.png')
end

local function suggestedMeshExportPath()
    return outputBaseName(state.meshPath, '_painted', '.msh')
end

local function texturePathForMeshPath(meshPath)
    local path = meshPath:gsub('%.[^%.]+$', '.png')
    return path == meshPath and meshPath .. '.png' or path
end

local function preparePreviewForExport()
    if not state.previewAsset or not state.target then return false, 'no mesh loaded' end
    finishStroke()
    state.previewCancelled = false
    requestPreviewRebuild()
    local guard = 0
    while state.previewJob and guard < 8 do
        local called, errorMessage = dpCall(processPreviewJob)
        if not called then return false, errorMessage end
        guard = guard + 1
    end
    if state.previewJob or state.previewDirty then
        return false, 'preview is still processing'
    end
    return true
end

local function saveTextureAs()
    if not state.target then
        state.status = tLang.L('mtp_export_no_mesh')
        return false
    end
    if not ensureEditableTexture() then return false end
    local path = mbm.saveFile(state.exportTexturePath or suggestedTextureExportPath(), 'png')
    if not path or path == '' then return false end
    path = resolvePath(path) or path
    if isSourcePath(path) then
        state.status = tLang.L('mtp_export_source_protected')
        return false
    end
    local buffer = currentVisibleBuffer()
    local saved, errorMessage = PaintIO.publish({{path=path, write=function(temporary)
        return mbm.writeImagePixels(temporary, buffer.bytes, buffer.width, buffer.height)
    end}}, dpCall)
    if not saved then
        state.status = tLang.L('mtp_export_failed') .. ': ' .. tostring(errorMessage)
        return false
    end
    state.exportTexturePath, state.exportDirty = path, false
    state.status = tLang.L('mtp_texture_exported') .. ': ' .. path .. (errorMessage and ('; ' .. errorMessage) or '')
    return true
end

local function saveMeshAs()
    if not state.target then
        state.status = tLang.L('mtp_export_no_mesh')
        return false
    end
    local ready, errorMessage = preparePreviewForExport()
    if not ready then
        state.status = tLang.L('mtp_export_failed') .. ': ' .. tostring(errorMessage)
        return false
    end
    local path = mbm.saveFile(state.exportMeshPath or suggestedMeshExportPath(), 'msh')
    if not path or path == '' then return false end
    path = resolvePath(path) or path
    if isSourcePath(path) then
        state.status = tLang.L('mtp_export_source_protected')
        return false
    end
    local texturePath = texturePathForMeshPath(path)
    if isSourcePath(texturePath) then
        state.status = tLang.L('mtp_export_source_protected')
        return false
    end
    local buffer = currentVisibleBuffer()
    local saved, saveError = PaintIO.publish({
        {path=texturePath, write=function(temporary)
            return mbm.writeImagePixels(temporary, buffer.bytes, buffer.width, buffer.height)
        end},
        {path=path, write=function(temporary)
            -- Clone the source snapshot through the staged file. Exporting must
            -- not mutate preview bindings or inherit private textures from a
            -- previously selected material, nor reread a changed source on disk.
            assert(state.sourceAsset:save(temporary, false, false, false))
            local candidate = meshDebug:new()
            assert(candidate:load(temporary))
            for _, subset in ipairs(state.target.subsets) do
                assert(candidate:setTexture(1, subset, texturePath))
            end
            assert(candidate:check())
            return candidate:save(temporary, false, false, false)
        end},
    }, dpCall)
    if not saved then
        state.status = tLang.L('mtp_export_failed') .. ': ' .. tostring(saveError)
        return false
    end
    state.exportMeshPath = path
    state.exportTexturePath, state.exportDirty = texturePath, false
    state.status = tLang.L('mtp_mesh_exported') .. ': ' .. path .. (saveError and ('; ' .. saveError) or '')
    return true
end

local function validateProjectTarget(project)
    assert(project.frame == 1, 'project frame is not supported by this editor session')
    local fullPath = resolvePath(project.source_mesh) or project.source_mesh
    local targetPath = resolvePath(project.source_texture) or project.source_texture
    local candidate = meshDebug:new()
    assert(candidate:load(fullPath))
    assert(candidate:check())
    local totalSubsets = assert(candidate:getTotalSubset(project.frame))
    local targetSubsets, targetTriangles = {}, {}
    for subset = 1, totalSubsets do
        local snapshot, errorMessage = Model.snapshotSubset(candidate, project.frame, subset, dpCall)
        if not snapshot then error(errorMessage or 'could not inspect project mesh', 0) end
        local snapshotTexture = normalizedTexture(snapshot.texture)
        if snapshotTexture == targetPath then
            targetSubsets[#targetSubsets + 1] = subset
            for _, triangle in ipairs(snapshot.triangles) do
                triangle.subset = subset
                targetTriangles[#targetTriangles + 1] = triangle
            end
        end
    end
    assert(#targetSubsets > 0, 'project texture is not used by the mesh')
    local selected = false
    for _, subset in ipairs(targetSubsets) do
        if subset == project.subset then selected = true; break end
    end
    assert(selected, 'project subset is not used by the selected texture')
    if project.limit_to_uv_island then
        local islandInfo = Model.buildUvIslands(targetTriangles)
        local found = false
        for _, islandId in ipairs(islandInfo.islandByTriangle) do
            if islandId == project.uv_island then found = true; break end
        end
        assert(found, 'project UV island is not used by the selected texture')
    end
    return {meshPath=fullPath, texturePath=targetPath, targetSubsets=targetSubsets}
end

local function openProject(path)
    local ok, project = dpCall(Project.loadLua, resolvePath(path) or path)
    if not ok then
        state.status = tLang.L('mtp_project_load_failed') .. ': ' .. tostring(project)
        return false
    end
    local paintRead, paintBytes, paintWidth, paintHeight = dpCall(
        mbm.readImagePixels, project.paint_layer)
    if not paintRead or not paintBytes then
        state.status = tLang.L('mtp_project_load_failed') .. ': paint layer could not be read.'
        return false
    end
    local sourceRead, sourceBytes, sourceWidth, sourceHeight = dpCall(
        mbm.readImagePixels, project.source_texture)
    if not sourceRead or not sourceBytes then
        state.status = tLang.L('mtp_project_load_failed') .. ': source texture could not be read.'
        return false
    end
    if paintWidth ~= sourceWidth or paintHeight ~= sourceHeight then
        state.status = tLang.L('mtp_project_load_failed') ..
            ': paint layer dimensions do not match the source texture.'
        return false
    end
    local inspected, inspection = dpCall(validateProjectTarget, project)
    if not inspected then
        state.status = tLang.L('mtp_project_load_failed') .. ': ' .. tostring(inspection)
        return false
    end
    local loaded = dpCall(loadMesh, inspection.meshPath, inspection.texturePath)
    if not loaded then return false end
    state.selectedSubset = project.subset
    state.limitToSubset = project.limit_to_subset == true
    state.selectedUvIsland = project.uv_island
    state.limitToUvIsland = project.limit_to_uv_island == true
    state.brush.mode = (project.projection_mode == 'project_stencil' or
        project.projection_mode == 'project_stencil_through') and 'project' or
        project.projection_mode == 'surface_3d' and 'surface' or 'paint'
    state.brush.shape = project.brush_shape == 'square' and 'square' or 'circle'
    state.paintThrough = project.projection_mode == 'project_stencil_through'
    state.dilationRadius = normalizeDilationRadius(project.dilation_radius)
    state.pendingDilationRadius = nil
    if paintWidth ~= sourceWidth or paintHeight ~= sourceHeight then
        error('project paint layer dimensions do not match the source texture')
    end
    state.base = Raster.newBuffer(sourceWidth, sourceHeight, sourceBytes)
    state.layer = Raster.newLayer(state.base, 32)
    state.layer.bytes = paintBytes
    state.layer.touched = true
    state.composite = nil
    state.compositeDirtyTiles = Raster.tilesForRect(state.layer, 0, 0,
        state.layer.width - 1, state.layer.height - 1)
    state.undo, state.redo = {}, {}
    state.projectPath, state.modified = resolvePath(path) or path, false
    state.previewDirty, state.previewRequiresMesh = true, true
    state.status = tLang.L('mtp_project_loaded') .. ': ' .. state.projectPath
    return true
end

local function performSessionAction(action)
    if not action then return false end
    state.pendingAction = nil
    if action.kind == 'mesh' then
        return dpCall(loadMesh, action.path)
    end
    if action.kind == 'project' then
        return dpCall(openProject, action.path)
    end
    if action.kind == 'reload' then
        return dpCall(loadMesh, action.path)
    end
    if action.kind == 'target' then
        return dpCall(selectTarget, action.index)
    end
    if action.kind == 'quit' then
        mbm.quit()
        return true
    end
    return false
end

local function saveCurrentProject()
    if not state.target then return true end
    if state.projectPath then
        local called, saved = dpCall(saveProject, state.projectPath)
        return called and saved == true
    end
    return saveProjectAs()
end

local function requestSessionAction(action)
    if state.stroke then finishStroke() end
    if state.modified then
        state.pendingAction = action
        state.openUnsavedPopup = true
        return false
    end
    return performSessionAction(action)
end

local function showUnsavedChangesModal()
    if state.openUnsavedPopup then
        tImGui.OpenPopup('mesh_texture_paint_unsaved')
        state.openUnsavedPopup = false
    end
    local opened = tImGui.BeginPopupModal('mesh_texture_paint_unsaved', false,
        tImGui.Flags('ImGuiWindowFlags_AlwaysAutoResize'))
    if not opened then return end
    tImGui.Text(tLang.L('mtp_unsaved_title'))
    tImGui.TextWrapped(tLang.L('mtp_unsaved_message'))
    tImGui.Separator()
    if tImGui.Button(tLang.L('mtp_unsaved_save'), {x=150, y=0}) then
        local action = state.pendingAction
        if saveCurrentProject() then
            tImGui.CloseCurrentPopup()
            performSessionAction(action)
        else
            state.status = tLang.L('mtp_unsaved_save_failed')
        end
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('mtp_unsaved_discard'), {x=150, y=0}) then
        local action = state.pendingAction
        state.modified = false
        tImGui.CloseCurrentPopup()
        performSessionAction(action)
    end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('mtp_unsaved_cancel'), {x=100, y=0}) then
        state.pendingAction = nil
        tImGui.CloseCurrentPopup()
    end
    tImGui.EndPopup()
end

local function showMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu(tLang.L('menu_file')) then
        if tImGui.MenuItem(tLang.L('mtp_open_msh')) then
            local path = mbm.openFile(state.meshPath or '', 'msh')
            if path then requestSessionAction({kind='mesh', path=path}) end
        end
        if tImGui.MenuItem(tLang.L('mtp_open_project')) then
            local path = mbm.openFile(state.projectPath or '', 'lua')
            if path then requestSessionAction({kind='project', path=path}) end
        end
        if tImGui.MenuItem(tLang.L('mtp_save_project'), 'Ctrl+S', false, state.target ~= nil) then
            if state.projectPath then dpCall(saveProject, state.projectPath) else saveProjectAs() end
        end
        if tImGui.MenuItem(tLang.L('mtp_save_project_as'), nil, false, state.target ~= nil) then saveProjectAs() end
        if tImGui.MenuItem(tLang.L('mtp_save_texture_as'), nil, false, state.target ~= nil) then
            dpCall(saveTextureAs)
        end
        if tImGui.MenuItem(tLang.L('mtp_save_mesh_as'), nil, false, state.target ~= nil) then
            dpCall(saveMeshAs)
        end
        if tImGui.MenuItem(tLang.L('mtp_reload')) and state.meshPath then
            requestSessionAction({kind='reload', path=state.meshPath})
        end
        if tImGui.MenuItem(tLang.L('menu_quit')) then
            requestSessionAction({kind='quit'})
        end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_edit')) then
        if tImGui.MenuItem(tLang.L('undo'), 'Ctrl+Z', false, #state.undo > 0) then history(false) end
        if tImGui.MenuItem(tLang.L('redo'), 'Ctrl+Y', false, #state.redo > 0) then history(true) end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu(tLang.L('menu_options')) then
        tLang.renderLanguageSubmenu()
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

local function showBrushModeTooltip(key)
    if not tImGui.IsItemHovered(0) then return end
    tImGui.BeginTooltip()
    tImGui.PushTextWrapPos(420)
    tImGui.TextWrapped(tLang.L(key))
    tImGui.PopTextWrapPos()
    tImGui.EndTooltip()
end

local function showControls()
    tUtil.setInitialWindowPositionLeft(state.titles.controls, 0, 20, 340)
    local opened = tImGui.Begin(state.titles.controls, true, 0)
    if opened then
        tImGui.TextWrapped(state.status)
        if state.meshPath then tImGui.TextWrapped('Mesh: ' .. state.meshPath) end
        if state.projectPath then tImGui.TextWrapped('Project: ' .. state.projectPath) end
        if state.previewJob then
            tImGui.Text(tLang.L('mtp_processing'))
            tImGui.ProgressBar(state.previewJob.progress, {x=-1,y=0},
                string.format('%.0f%%', state.previewJob.progress * 100))
        end
        if state.previewCancelled then
            tImGui.TextWrapped(tLang.L('mtp_preview_cancelled'))
            if tImGui.Button(tLang.L('mtp_resume_preview')) then resumePreviewJob() end
        end
        if state.stroke and state.stroke.queue then
            local pending = math.max(0, #state.stroke.queue - state.stroke.queueHead + 1)
            if pending > 0 then
                tImGui.Text(string.format(tLang.L('mtp_processing_stroke'), pending))
            end
        end
        if #state.targets > 0 then
            local labels = {}
            for _, target in ipairs(state.targets) do labels[#labels+1] = tUtil.getShortName(target.path) end
            local changed, selected = tImGui.Combo(tLang.L('texture'), state.targetIndex, labels, -1)
            if changed and selected ~= state.targetIndex then
                requestSessionAction({kind='target', index=selected})
            end
            if state.target and state.target.uvOverlapCount and state.target.uvOverlapCount > 0 then
                tImGui.TextWrapped(string.format(tLang.L('mtp_uv_overlap_warning'),
                    state.target.uvOverlapCount))
                if state.target.uvOverlapTruncated then
                    tImGui.TextWrapped(tLang.L('mtp_uv_overlap_limited'))
                end
            end
            if state.target and #state.target.subsets > 0 then
                local oldLimit = state.limitToSubset
                state.limitToSubset = tImGui.Checkbox(tLang.L('mtp_limit_subset'), state.limitToSubset)
                showBrushModeTooltip('mtp_limit_subset_tooltip')
                if state.limitToSubset then
                    local subsetLabels = {}
                    local selectedIndex = 1
                    for index, subset in ipairs(state.target.subsets) do
                        subsetLabels[#subsetLabels + 1] = string.format('%s %d',
                            tLang.L('mtp_subset'), subset)
                        if subset == state.selectedSubset then selectedIndex = index end
                    end
                    local oldSubset = state.selectedSubset
                    local subsetChanged, selected = tImGui.Combo(tLang.L('mtp_subset'),
                        selectedIndex, subsetLabels, -1)
                    if subsetChanged and state.target.subsets[selected] then
                        state.selectedSubset = state.target.subsets[selected]
                    end
                    if oldSubset ~= state.selectedSubset then
                    ensureSelectedUvIsland()
                        state.hoverHit = nil
                        state.hoverPickPending = false
                    end
                end
                if oldLimit ~= state.limitToSubset then
                    state.hoverHit = nil
                    state.hoverPickPending = false
                end
                local oldIslandLimit = state.limitToUvIsland
                state.limitToUvIsland = tImGui.Checkbox(tLang.L('mtp_limit_uv_island'),
                    state.limitToUvIsland)
                showBrushModeTooltip('mtp_limit_uv_island_tooltip')
                local availableIslands = availableUvIslands(state.target)
                if state.limitToUvIsland then
                    local islandLabels, selectedIndex = {}, 1
                    for index, island in ipairs(availableIslands) do
                        local triangleCount = #island.triangleIndices
                        islandLabels[#islandLabels + 1] = string.format('%s %d (%d)',
                            tLang.L('mtp_uv_island'), island.id, triangleCount)
                        if island.id == state.selectedUvIsland then selectedIndex = index end
                    end
                    local islandChanged, selected = tImGui.Combo(tLang.L('mtp_uv_island'),
                        selectedIndex, islandLabels, -1)
                    if islandChanged and availableIslands[selected] then
                        state.selectedUvIsland = availableIslands[selected].id
                    end
                end
                if oldIslandLimit ~= state.limitToUvIsland then
                    ensureSelectedUvIsland()
                    state.hoverHit = nil
                    state.hoverPickPending = false
                end
                if state.limitToUvIsland then
                    ensureSelectedUvIsland()
                end
                local hoveredIslandId, hoveredIsland = uvIslandFromHit(state.lastUvIslandHit)
                if hoveredIslandId then
                    tImGui.TextWrapped(string.format(tLang.L('mtp_hovered_uv_island'),
                        hoveredIslandId, #hoveredIsland.triangleIndices))
                else
                    tImGui.TextWrapped(tLang.L('mtp_select_uv_island_help'))
                end
            end
        end
        tImGui.Separator()
        local changed, value
        state.brush.highContrastCursor = tImGui.Checkbox(tLang.L('mtp_high_contrast_cursor'),
            state.brush.highContrastCursor)
        if state.brush.highContrastCursor then
            changed, value = tImGui.ColorEdit4(tLang.L('mtp_cursor_color'), state.brush.cursorColor)
            if changed then state.brush.cursorColor = value end
        end
        tImGui.Text(tLang.L('mtp_mode'))
        local modeIndex = state.brush.mode == 'paint' and 1 or state.brush.mode == 'erase' and 2 or
            state.brush.mode == 'pick' and 3 or state.brush.mode == 'stamp' and 4 or
            state.brush.mode == 'project' and 5 or 6
        local oldModeIndex = modeIndex
        modeIndex = tImGui.RadioButton(tLang.L('mtp_paint'), modeIndex, 1)
        showBrushModeTooltip('mtp_paint_tooltip')
        modeIndex = tImGui.RadioButton(tLang.L('mtp_erase'), modeIndex, 2)
        showBrushModeTooltip('mtp_erase_tooltip')
        modeIndex = tImGui.RadioButton(tLang.L('mtp_pick_color'), modeIndex, 3)
        showBrushModeTooltip('mtp_pick_color_tooltip')
        modeIndex = tImGui.RadioButton(tLang.L('mtp_stamp'), modeIndex, 4)
        showBrushModeTooltip('mtp_stamp_tooltip')
        modeIndex = tImGui.RadioButton(tLang.L('mtp_project_stencil'), modeIndex, 5)
        showBrushModeTooltip('mtp_project_stencil_tooltip')
        modeIndex = tImGui.RadioButton(tLang.L('mtp_surface_3d'), modeIndex, 6)
        showBrushModeTooltip('mtp_surface_3d_tooltip')
        if modeIndex ~= oldModeIndex then
            state.brush.mode = modeIndex == 1 and 'paint' or modeIndex == 2 and 'erase' or
                modeIndex == 3 and 'pick' or modeIndex == 4 and 'stamp' or
                modeIndex == 5 and 'project' or 'surface'
        end
        if state.brush.mode == 'project' then
            state.paintThrough = tImGui.Checkbox(tLang.L('mtp_paint_through'), state.paintThrough)
            showBrushModeTooltip('mtp_paint_through_tooltip')
        end
        if state.brush.mode == 'stamp' then
            tImGui.TextWrapped(state.stamp and tLang.L('mtp_stamp_selected') or tLang.L('mtp_stamp_help'))
            if tImGui.Button(tLang.L('mtp_stamp_import_external')) then
                openExternalStamp()
            end
            showBrushModeTooltip('mtp_stamp_import_external_help')
            local stampResolution = state.brush.stampResolution or 65
            changed, value = tImGui.SliderFloat(tLang.L('mtp_stamp_resolution'), stampResolution,
                STAMP_RESOLUTION_MIN, STAMP_RESOLUTION_MAX, '%.0f')
            if changed then
                local newResolution = math.max(STAMP_RESOLUTION_MIN,
                    math.min(STAMP_RESOLUTION_MAX, math.floor(value + .5)))
                if newResolution % 2 == 0 then newResolution = newResolution + 1 end
                state.brush.stampResolution = newResolution
                if state.stamp and state.stamp.source ~= 'external' then
                    clearStamp()
                    state.status = tLang.L('mtp_stamp_resolution_changed')
                end
            end
            if tImGui.IsItemHovered(0) then
                tImGui.BeginTooltip()
                tImGui.PushTextWrapPos(360)
                tImGui.TextWrapped(tLang.L('mtp_stamp_resolution_help'))
                tImGui.PopTextWrapPos()
                tImGui.EndTooltip()
            end
        end
        changed, value = tImGui.SliderFloat(tLang.L('radius'), state.brush.radius, 1, state.brushMaxRadius, '%.0f')
        if changed then
            state.brush.radius = value
            if state.stamp and state.stamp.source ~= 'external' then
                clearStamp()
                state.status = tLang.L('mtp_stamp_radius_changed')
            end
        end
        tImGui.Text(string.format(tLang.L('mtp_radius_limit'), state.brushMaxRadius))
        tImGui.Text(tLang.L('mtp_shape'))
        local shapeIndex = state.brush.shape == 'square' and 2 or 1
        local oldShapeIndex = shapeIndex
        shapeIndex = tImGui.RadioButton(tLang.L('mtp_shape_circle'), shapeIndex, 1)
        shapeIndex = tImGui.RadioButton(tLang.L('mtp_shape_square'), shapeIndex, 2)
        if shapeIndex ~= oldShapeIndex then
            state.brush.shape = shapeIndex == 2 and 'square' or 'circle'
            state.cursorCache.points, state.cursorCache.screenRadius = nil, nil
            if state.stamp and state.stamp.source ~= 'external' then
                clearStamp()
                state.status = tLang.L('mtp_stamp_shape_changed')
            end
        end
        if state.brush.mode == 'stamp' and state.stamp then
            local stampScale = state.stamp.scale or 1
            changed, value = tImGui.SliderFloat(tLang.L('mtp_stamp_scale'), stampScale,
                .1, 8, '%.2f')
            if changed then state.stamp.scale = math.max(.1, math.min(8, value)) end
            showBrushModeTooltip('mtp_stamp_scale_help')
            local stampRotation = state.stamp.rotation or 0
            changed, value = tImGui.SliderFloat(tLang.L('mtp_stamp_rotation'), stampRotation,
                -180, 180, '%.1f deg')
            if changed then state.stamp.rotation = math.max(-180, math.min(180, value)) end
            showBrushModeTooltip('mtp_stamp_rotation_help')
        end
        local dilationValue = state.pendingDilationRadius or state.dilationRadius
        changed, value = tImGui.SliderFloat(tLang.L('mtp_dilation_radius'), dilationValue,
            0, DILATION_RADIUS_MAX, '%.0f')
        local dilationEditFinished = tImGui.IsItemDeactivatedAfterEdit()
        if changed then
            state.pendingDilationRadius = normalizeDilationRadius(value)
        end
        if dilationEditFinished and state.pendingDilationRadius ~= nil then
            local pending = state.pendingDilationRadius
            state.pendingDilationRadius = nil
            applyDilationRadius(pending)
        end
        if tImGui.IsItemHovered(0) then
            tImGui.BeginTooltip()
            tImGui.PushTextWrapPos(360)
            tImGui.TextWrapped(tLang.L('mtp_dilation_radius_help'))
            tImGui.PopTextWrapPos()
            tImGui.EndTooltip()
        end
        changed, value = tImGui.SliderFloat(tLang.L('mtp_hardness'), state.brush.hardness, 0, 1, '%.2f')
        if changed then state.brush.hardness = value end
        changed, value = tImGui.SliderFloat(tLang.L('mtp_opacity'), state.brush.opacity, 0, 1, '%.2f')
        if changed then state.brush.opacity = value end
        changed, value = tImGui.ColorEdit4(tLang.L('mtp_color'), state.brush.color)
        if changed then state.brush.color = value end
        tImGui.Text(tLang.L('mtp_effective_color'))
        showBrushColorPreview()
        if tImGui.Button(tLang.L('undo')) then history(false) end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('redo')) then history(true) end
        tImGui.Text(string.format('Preview builds: %d | texture reloads: %d | mesh reloads: %d',
            state.previewBuilds, state.previewTextureReloads, state.previewMeshReloads))
    end
    tImGui.End()
end

local function showTexture()
    tUtil.setInitialWindowPositionRight(state.titles.texture, 0, 430, 320)
    local opened = tImGui.Begin(state.titles.texture, true, 0)
    if opened then
        local available = tImGui.GetContentRegionAvail()
        local stampShown = state.stampTextureInfo and state.stampTextureInfo:isLoaded()
        if state.previewTextureInfo and state.previewTextureInfo:isLoaded() then
            state.brush.showUvOverlay = tImGui.Checkbox(tLang.L('mtp_show_uv_overlay'),
                state.brush.showUvOverlay)
            showBrushModeTooltip('mtp_show_uv_overlay_tooltip')
            local reservedHeight = stampShown and 190 or 30
            local size = math.max(64, math.min(300, available.x, available.y - reservedHeight))
            local origin = tImGui.GetCursorScreenPos()
            tImGui.Image(state.previewTextureInfo, {x=size,y=size})
            if state.brush.showUvOverlay then showUvOverlay(origin, size) end
        else
            tImGui.TextWrapped(tLang.L('mtp_texture_help'))
        end
        if stampShown then
            tImGui.Separator()
            tImGui.Text(tLang.L('mtp_stamp_texture'))
            local stampSize = math.max(48, math.min(140, available.x, available.y))
            showCheckerboard(stampSize)
            tImGui.Image(state.stampTextureInfo, {x=stampSize,y=stampSize})
        elseif state.brush.mode == 'stamp' then
            tImGui.Separator()
            tImGui.TextWrapped(tLang.L('mtp_stamp_none'))
        end
    end
    tImGui.End()
end

local function showCamera()
    tUtil.setInitialWindowPositionRight(state.titles.camera, 0, 20, 330, nil,
        CAMERA_WINDOW_MAX_HEIGHT)
    local opened = tImGui.Begin(tLang.L('mtp_camera') .. '##' .. state.titles.camera, true,
        tImGui.Flags('ImGuiWindowFlags_NoCollapse'))
    if opened then
        if tImGui.Button(tLang.L('mtp_frame_mesh')) then frameCamera() end
        tImGui.SameLine()
        if tImGui.Button(tLang.L('reset_camera')) then
            state.cameraOrbit.azimuth, state.cameraOrbit.elevation = 0, .2
            frameCamera()
        end
        tImGui.Separator()
        if tUtil.drawOrbitGizmo(state.cameraOrbit, {size=110}) then applyCamera() end
        tImGui.Separator()
        tUtil.pushResponsiveItemWidth(72)
        local changed, value = tImGui.InputFloat(tLang.L('cam_distance'), state.cameraOrbit.distance,
            state.cameraOrbit.distance * .1, state.cameraOrbit.distance, '%.3f', 0)
        if changed and value > 0 then state.cameraOrbit.distance = value; applyCamera() end
        local fx, nx = tImGui.DragFloat('X##mtpCamFocusX', state.cameraOrbit.fx, 0.01, 0, 0, '%.3f', 0)
        local fy, ny = tImGui.DragFloat('Y##mtpCamFocusY', state.cameraOrbit.fy, 0.01, 0, 0, '%.3f', 0)
        local fz, nz = tImGui.DragFloat('Z##mtpCamFocusZ', state.cameraOrbit.fz, 0.01, 0, 0, '%.3f', 0)
        if fx then state.cameraOrbit.fx = nx end
        if fy then state.cameraOrbit.fy = ny end
        if fz then state.cameraOrbit.fz = nz end
        if fx or fy or fz then applyCamera() end
        tImGui.PopItemWidth()
        tImGui.Separator()
        tImGui.TextWrapped(tLang.L('mtp_camera_help'))
    end
    tImGui.End()
end

function onInitScene()
    state.camera = mbm.getCamera('3d')
    state.camera:setFar(9999999)
    tUtil.sMessageOverlay = tLang.L('mtp_welcome')
end

function onLoop(delta)
    tLang.applyPendingLanguage()
    processPendingStroke()
    requestPreviewRebuild()
    processHoverPick(delta)
    if state.previewJob and not state.painting then
        local ok, errorMessage = dpCall(processPreviewJob)
        if not ok then
            state.previewJob = nil
            state.status = 'Preview processing failed: ' .. tostring(errorMessage)
        end
    end
    showMenu()
    showUnsavedChangesModal()
    showControls()
    showCamera()
    showTexture()
    showExternalStampOverlay()
    showBrushCursor()
    tUtil.showOverlayMessage()
end

function onTouchDown(key, x, y)
    state.mouse.x, state.mouse.y = x, y
    if state.stampOverlay then return end
    if imguiCapturesMouse() then return end
    if state.previewJob then return end
    if key == 0 then
        state.orbiting = true
        state.hoverHit = nil
        state.hoverPickPending = false
    elseif key == 1 then
        local hit = hitAtScreen(x, y)
        if not (hit and hit.uv) then
            state.status = tLang.L('mtp_pick_no_surface')
            return
        end
        state.hoverHit = hit
        state.hoverPickPending = false
        state.hoverPickElapsed = 0
        if not ensureEditableTexture() then return end
        if state.brush.mode == 'stamp' and (not state.stamp or state.keyControl) then
            pickStampSource(hit, x, y)
        elseif state.keyControl then
            selectUvIslandFromHit(hit)
        elseif state.brush.mode == 'pick' then
            pickColorAtHit(hit)
        else
            beginStroke(hit, x, y)
        end
    elseif key == 2 then
        state.panning = true
        state.hoverHit = nil
        state.hoverPickPending = false
    end
end

function onTouchMove(key, x, y)
    local dx, dy = x - state.mouse.x, y - state.mouse.y
    state.mouse.x, state.mouse.y = x, y
    if state.stampOverlay then return end
    if imguiCapturesMouse() or state.previewJob then return end
    if state.painting then
        local hit = hitAtScreen(x, y)
        state.hoverHit = hit
        enqueueStrokeHit(hit, x, y)
    elseif state.orbiting then
        state.cameraOrbit.azimuth = state.cameraOrbit.azimuth + dx * .005
        state.cameraOrbit.elevation = math.max(-math.pi*.49,
            math.min(math.pi*.49, state.cameraOrbit.elevation + dy*.005))
        applyCamera()
    elseif state.panning then
        local c = state.cameraOrbit
        local px = c.fx + c.distance * math.cos(c.elevation) * math.sin(c.azimuth)
        local pz = c.fz + c.distance * math.cos(c.elevation) * math.cos(c.azimuth)
        local vx, vz = c.fx - px, c.fz - pz
        local length = math.sqrt(vx * vx + vz * vz)
        local rx, rz = 1, 0
        if length > 0 then rx, rz = vz / length, -vx / length end
        local scale = c.distance * .001
        c.fx = c.fx + rx * (-dx) * scale
        c.fy = c.fy + dy * scale
        c.fz = c.fz + rz * (-dx) * scale
        applyCamera()
    elseif not state.orbiting and not state.panning then
        state.hoverPickPending = true
    end
end

function onTouchUp(key, x, y)
    if state.stampOverlay then
        state.mouse.x, state.mouse.y = x, y
        return
    end
    local captured = imguiCapturesMouse()
    if key == 1 then
        if state.stroke then state.stroke.finishRequested = true else finishStroke() end
    end
    state.orbiting, state.panning = false, false
    state.mouse.x, state.mouse.y = x, y
    if key ~= 1 and not captured then state.hoverPickPending = true end
end

function onTouchZoom(zoom)
    if state.stampOverlay or imguiCapturesMouse() or state.previewJob then return end
    state.cameraOrbit.distance = math.max(.1, state.cameraOrbit.distance * (zoom > 0 and .9 or 1.1))
    applyCamera()
    state.hoverPickPending = true
    state.hoverPickElapsed = .12
end

function onKeyDown(key)
    if key == mbm.getKeyCode('ESC') and state.stampOverlay then
        cancelExternalStampSelection()
        return
    end
    if key == mbm.getKeyCode('ESC') and state.stroke then
        cancelStroke()
        return
    end
    if key == mbm.getKeyCode('ESC') and state.previewJob then
        cancelPreviewJob()
        return
    end
    if imguiCapturesKeyboard() then return end
    if key == mbm.getKeyCode('control') then state.keyControl = true
    elseif state.keyControl and key == mbm.getKeyCode('Z') then history(false)
    elseif state.keyControl and key == mbm.getKeyCode('Y') then history(true)
    elseif state.keyControl and key == mbm.getKeyCode('O') then
        local path = mbm.openFile(state.projectPath or '', 'lua')
        if path then requestSessionAction({kind='project', path=path}) end
    elseif state.keyControl and key == mbm.getKeyCode('S') and state.layer then
        if state.projectPath then dpCall(saveProject, state.projectPath) else saveProjectAs() end
    elseif state.keyControl and key == mbm.getKeyCode('Q') then
        requestSessionAction({kind='quit'})
    end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then state.keyControl = false end
end

function onEndScene()
    finishStroke()
    clearPreview()
end
