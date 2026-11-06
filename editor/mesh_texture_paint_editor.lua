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

local state = {
    camera = nil,
    meshPath = nil,
    sourceAsset = nil,
    previewAsset = nil,
    previewMesh = nil,
    previewMeshPath = nil,
    previewTexturePath = nil,
    previewTextureInfo = nil,
    snapshots = {},
    targets = {},
    targetIndex = 0,
    target = nil,
    base = nil,
    layer = nil,
    composite = nil,
    previewDirty = false,
    previewBuilds = 0,
    status = 'Open an MSH to start painting.',
    brush = {radius = 8, hardness = 1, opacity = 1, mode = 'paint', color = {r=1,g=0,b=0,a=1}},
    cameraOrbit = {fx=0, fy=0, fz=0, distance=5, azimuth=0, elevation=.2},
    mouse = {x=0, y=0},
    orbiting = false,
    panning = false,
    painting = false,
    stroke = nil,
    undo = {},
    redo = {},
    keyControl = false,
    titles = {
        controls = 'mesh_texture_paint_controls',
        texture = 'mesh_texture_paint_texture',
    },
}

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then
        print('[mesh_texture_paint_editor] ' .. tostring(result[2]))
    end
    return table.unpack(result, 1, result.n)
end

local function clearPreview()
    if state.previewMesh and state.previewAsset then
        dpCall(function() return meshDebug:loadMeshPreview(state.previewMesh, nil) end)
    end
    if state.previewMesh then state.previewMesh:destroy() end
    if state.previewTextureInfo then state.previewTextureInfo:release() end
    state.previewMesh, state.previewAsset, state.previewTextureInfo = nil, nil, nil
    if state.previewMeshPath then os.remove(state.previewMeshPath) end
    if state.previewTexturePath then os.remove(state.previewTexturePath) end
    state.previewMeshPath, state.previewTexturePath = nil, nil
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
    state.cameraOrbit.distance = math.max(.1, math.sqrt(dx*dx + dy*dy + dz*dz) * 1.6)
    applyCamera()
end

local function colorBytes()
    local c = state.brush.color
    return {r=math.floor((c.r or 0) * 255 + .5), g=math.floor((c.g or 0) * 255 + .5),
        b=math.floor((c.b or 0) * 255 + .5), a=math.floor((c.a or 1) * 255 + .5)}
end

local function targetForTexture(path)
    for index, target in ipairs(state.targets) do
        if target.path == path then return index, target end
    end
    local target = {path=path, subsets={}, triangles={}}
    state.targets[#state.targets + 1] = target
    return #state.targets, target
end

local function releaseTextureWork()
    state.base, state.layer, state.composite, state.target = nil, nil, nil, nil
end

local function selectTarget(index)
    local target = state.targets[index]
    if not target then return false end
    local ok, bytes, width, height = dpCall(mbm.readImagePixels, target.path)
    if not ok or not bytes then
        state.status = 'Could not read texture: ' .. tostring(bytes or width)
        return false
    end
    releaseTextureWork()
    state.targetIndex, state.target = index, target
    state.base = Raster.newBuffer(width, height, bytes)
    state.layer = Raster.newLayer(state.base, 32)
    state.composite = Raster.compose(state.base, state.layer)
    state.undo, state.redo = {}, {}
    state.previewDirty = true
    state.status = string.format('Texture selected: %s (%dx%d)', target.path, width, height)
    return true
end

local function loadPreviewTexture()
    assert(state.composite and state.previewTexturePath)
    assert(mbm.writeImagePixels(state.previewTexturePath, state.composite.bytes,
        state.composite.width, state.composite.height))
    if state.previewTextureInfo then
        assert(state.previewTextureInfo:reload(state.previewTexturePath))
    else
        state.previewTextureInfo = assert(mbm.loadTexture(state.previewTexturePath))
    end
end

local function rebuildPreview()
    if not (state.previewAsset and state.previewMesh and state.target and state.layer) then return end
    state.composite = Raster.compose(state.base, state.layer)
    loadPreviewTexture()
    for _, subset in ipairs(state.target.subsets) do
        assert(state.previewAsset:setTexture(1, subset, state.previewTexturePath))
    end
    assert(state.previewAsset:save(state.previewMeshPath, false, false, false))
    assert(meshDebug:loadMeshPreview(state.previewMesh, state.previewMeshPath))
    state.previewMesh.alwaysRender = true
    state.previewMesh.visible = true
    state.previewDirty = false
    state.previewBuilds = state.previewBuilds + 1
end

local function loadMesh(path)
    clearPreview()
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
                target = {path=texturePath, subsets={}, triangles={}}
                targetMap[texturePath] = target
            end
            target.subsets[#target.subsets + 1] = subset
            for _, triangle in ipairs(snapshot.triangles) do target.triangles[#target.triangles + 1] = triangle end
        end
    end
    state.meshPath, state.sourceAsset, state.snapshots = fullPath, source, snapshots
    state.targets = {}
    for _, target in pairs(targetMap) do state.targets[#state.targets + 1] = target end
    table.sort(state.targets, function(left, right) return left.path < right.path end)
    assert(#state.targets > 0, 'mesh has no file-backed primary texture with UVs')
    state.previewAsset = meshDebug:new()
    assert(state.previewAsset:load(fullPath))
    state.previewMesh = mesh:new('3d')
    state.previewMeshPath = tUtil.getTemporaryFilePath('.msh')
    state.previewTexturePath = tUtil.getTemporaryFilePath('.png')
    state.cameraOrbit = {fx=0,fy=0,fz=0,distance=5,azimuth=0,elevation=.2}
    state.camera:setFar(9999999)
    frameCamera()
    assert(selectTarget(1))
    rebuildPreview()
    state.status = string.format('Loaded %s. Paint with right drag; orbit with left drag.', fullPath)
end

local function beginStroke(hit)
    state.stroke = {before={}, tiles={}, lastU=hit.uv.u, lastV=hit.uv.v}
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

local function applyStrokeHit(hit)
    if not (state.painting and hit and hit.uv and state.layer) then return end
    local x = math.max(0, math.min(state.layer.width-1, hit.uv.u*(state.layer.width-1)))
    local y = math.max(0, math.min(state.layer.height-1, hit.uv.v*(state.layer.height-1)))
    local radius = state.brush.radius
    for _, tile in ipairs(Raster.tilesForDab(state.layer, x, y, radius)) do
        if not state.stroke.before[tile.key] then
            state.stroke.before[tile.key] = Raster.snapshotTiles(state.layer, {tile}).tiles[1]
            state.stroke.tiles[#state.stroke.tiles+1] = tile
        end
    end
    local result = Raster.applyDab(state.layer, {x=x, y=y, radius=radius,
        hardness=state.brush.hardness, opacity=state.brush.opacity,
        color=colorBytes(), mode=state.brush.mode})
    if result.changedPixels > 0 then state.previewDirty = true end
end

local function hitAtScreen(x, y)
    if not state.target then return nil end
    local ok, ox, oy, oz, dx, dy, dz = dpCall(mbm.getPickRay, x, y)
    if not ok then return nil end
    return Pick.pickTriangles({x=ox,y=oy,z=oz}, {x=dx,y=dy,z=dz}, state.target.triangles)
end

local function finishStroke()
    if not state.stroke then state.painting = false; return end
    if state.previewDirty and #state.stroke.tiles > 0 then
        local after = Raster.snapshotTiles(state.layer, state.stroke.tiles)
        local before = {width=state.layer.width,height=state.layer.height,tiles={}}
        for _, tile in pairs(state.stroke.before) do before.tiles[#before.tiles+1] = tile end
        state.undo[#state.undo+1] = {before=before, after=after}
        state.redo = {}
    end
    state.stroke, state.painting = nil, false
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
    state.previewDirty = true
end

local function showMenu()
    if not tImGui.BeginMainMenuBar() then return end
    if tImGui.BeginMenu('File') then
        if tImGui.MenuItem('Open MSH') then
            local path = mbm.openFile(state.meshPath or '', 'msh')
            if path then dpCall(loadMesh, path) end
        end
        if tImGui.MenuItem('Reload') and state.meshPath then dpCall(loadMesh, state.meshPath) end
        if tImGui.MenuItem('Quit') then mbm.quit() end
        tImGui.EndMenu()
    end
    if tImGui.BeginMenu('Edit') then
        if tImGui.MenuItem('Undo', 'Ctrl+Z') then history(false) end
        if tImGui.MenuItem('Redo', 'Ctrl+Y') then history(true) end
        tImGui.EndMenu()
    end
    tImGui.EndMainMenuBar()
end

local function showControls()
    tUtil.setInitialWindowPositionLeft(state.titles.controls, 0, 20, 340)
    local opened = tImGui.Begin(state.titles.controls, true, 0)
    if opened then
        if tImGui.Button('Open MSH') then
            local path = mbm.openFile(state.meshPath or '', 'msh')
            if path then dpCall(loadMesh, path) end
        end
        tImGui.SameLine()
        if tImGui.Button('Reload') and state.meshPath then dpCall(loadMesh, state.meshPath) end
        tImGui.Separator()
        tImGui.TextWrapped(state.status)
        if state.meshPath then tImGui.TextWrapped('Mesh: ' .. state.meshPath) end
        if #state.targets > 0 then
            local labels = {}
            for _, target in ipairs(state.targets) do labels[#labels+1] = tUtil.getShortName(target.path) end
            local changed, selected = tImGui.Combo('Texture', state.targetIndex, labels, -1)
            if changed and selected ~= state.targetIndex then dpCall(selectTarget, selected) end
        end
        tImGui.Separator()
        local changed, value = tImGui.Combo('Mode', state.brush.mode == 'paint' and 1 or 2, {'Paint', 'Erase'}, -1)
        if changed then state.brush.mode = value == 1 and 'paint' or 'erase' end
        changed, value = tImGui.SliderFloat('Radius', state.brush.radius, 1, 512, '%.0f')
        if changed then state.brush.radius = value end
        changed, value = tImGui.SliderFloat('Hardness', state.brush.hardness, 0, 1, '%.2f')
        if changed then state.brush.hardness = value end
        changed, value = tImGui.SliderFloat('Opacity', state.brush.opacity, 0, 1, '%.2f')
        if changed then state.brush.opacity = value end
        changed, value = tImGui.ColorEdit4('Color', state.brush.color)
        if changed then state.brush.color = value end
        if tImGui.Button('Undo') then history(false) end
        tImGui.SameLine()
        if tImGui.Button('Redo') then history(true) end
        tImGui.Text(string.format('Preview builds: %d', state.previewBuilds))
    end
    tImGui.End()
end

local function showTexture()
    tUtil.setInitialWindowPositionRight(state.titles.texture, 0, 20, 320)
    local opened = tImGui.Begin(state.titles.texture, true, 0)
    if opened then
        if state.previewTextureInfo and state.previewTextureInfo:isLoaded() then
            local available = tImGui.GetContentRegionAvail()
            local size = math.max(64, math.min(300, available.x, available.y - 30))
            tImGui.Image(state.previewTextureInfo, {x=size,y=size}, {x=0,y=0}, {x=1,y=1})
        else
            tImGui.TextWrapped('Load a mesh with a file-backed UV texture.')
        end
    end
    tImGui.End()
end

function onInitScene()
    state.camera = mbm.getCamera('3d')
    state.camera:setFar(9999999)
    tUtil.sMessageOverlay = 'Mesh Texture Paint: open an MSH to start.'
end

function onLoop(delta)
    showMenu()
    showControls()
    showTexture()
    if state.previewDirty then dpCall(rebuildPreview) end
end

function onTouchDown(key, x, y)
    state.mouse.x, state.mouse.y = x, y
    if tImGui.GetWantCaptureMouse() then return end
    if key == 0 then state.orbiting = true
    elseif key == 1 then
        local hit = hitAtScreen(x, y)
        if hit and hit.uv then beginStroke(hit); applyStrokeHit(hit) end
    elseif key == 2 then state.panning = true end
end

function onTouchMove(key, x, y)
    if tImGui.GetWantCaptureMouse() then return end
    local dx, dy = x - state.mouse.x, y - state.mouse.y
    if state.painting then
        applyStrokeHit(hitAtScreen(x, y))
    elseif state.orbiting then
        state.cameraOrbit.azimuth = state.cameraOrbit.azimuth - dx * .005
        state.cameraOrbit.elevation = math.max(-math.pi*.49,
            math.min(math.pi*.49, state.cameraOrbit.elevation + dy*.005))
        applyCamera()
    elseif state.panning then
        local scale = state.cameraOrbit.distance * .001
        state.cameraOrbit.fx = state.cameraOrbit.fx - dx * scale
        state.cameraOrbit.fy = state.cameraOrbit.fy + dy * scale
        applyCamera()
    end
    state.mouse.x, state.mouse.y = x, y
end

function onTouchUp(key, x, y)
    if key == 1 then finishStroke() end
    state.orbiting, state.panning = false, false
    state.mouse.x, state.mouse.y = x, y
end

function onTouchZoom(zoom)
    state.cameraOrbit.distance = math.max(.1, state.cameraOrbit.distance * (zoom > 0 and .9 or 1.1))
    applyCamera()
end

function onKeyDown(key)
    if key == mbm.getKeyCode('control') then state.keyControl = true
    elseif state.keyControl and key == mbm.getKeyCode('Z') then history(false)
    elseif state.keyControl and key == mbm.getKeyCode('Y') then history(true) end
end

function onKeyUp(key)
    if key == mbm.getKeyCode('control') then state.keyControl = false end
end

function onEndScene()
    finishStroke()
    clearPreview()
end
