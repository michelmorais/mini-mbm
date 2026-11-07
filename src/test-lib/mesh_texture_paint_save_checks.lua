--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                           |
|                                                                                                                       |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated            |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation         |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and         |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                      |
|                                                                                                                       |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                         |
|                                                                                                                       |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE    |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR   |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR         |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        |
|-----------------------------------------------------------------------------------------------------------------------|
]]--

-- Integration checks called by mesh_texture_paint_preview_smoke.lua after painting.
local Project = require 'mesh_texture_paint_project'
local Raster = require 'mesh_texture_paint_raster'
local M = {}

local function read(path)
    local file = assert(io.open(path, 'rb'))
    local bytes = assert(file:read('*a')); assert(file:close())
    return bytes
end
local function write(path, bytes)
    local file = assert(io.open(path, 'wb'))
    assert(file:write(bytes)); assert(file:close())
end

function M.run(state, editorLoop, upvalue, temp)
    local saveProject = assert(upvalue(editorLoop, 'saveProject'))
    local saveMesh = assert(upvalue(editorLoop, 'saveMeshAs'))
    local saveTexture = assert(upvalue(editorLoop, 'saveTextureAs'))
    local project = temp .. '.meshpaint.lua'
    local layer = temp .. '.meshpaint.paint.png'
    local exportedMesh, exportedTexture = temp .. '_painted.msh', temp .. '_painted.png'
    local standalone = temp .. '_standalone.png'
    local saveFile, writePixels, rename, saveLua = mbm.saveFile, mbm.writeImagePixels, os.rename, Project.saveLua
    local sourceSave = state.sourceAsset.save
    local sourceMeshBytes, sourceTextureBytes = read(state.meshPath), read(state.target.path)
    local previewTexture = state.previewAsset:getTexture(1, state.target.subsets[1])
    local otherPreviewTexture = state.previewAsset:getTexture(1, 2)
    local layerBytes = Raster.toBytes(state.layer)
    local expected = Raster.compose(state.base, state.layer).bytes
    local function failPublish(path)
        os.rename = function(from, to)
            if to == path and from:find('.tmp.', 1, true) then
                return nil, 'injected publication failure'
            end
            return rename(from, to)
        end
    end
    local ok, err = xpcall(function()
        assert(saveProject(project))
        assert(not state.modified and state.projectPath == project)
        local reopened = Project.loadLua(project)
        assert(reopened.paint_layer == layer)
        assert(mbm.readImagePixels(layer) == layerBytes, 'Saved project lost paint pixels')
        local oldProject = read(project)

        -- Explicit false must be handled as failure, even though the binding did not throw.
        state.modified = true
        mbm.writeImagePixels = function() return false, 'injected image encoder failure' end
        assert(not saveProject(project))
        assert(state.modified and state.projectPath == project)
        assert(read(project) == oldProject and mbm.readImagePixels(layer) == layerBytes)
        mbm.writeImagePixels = writePixels

        -- Distinct old bytes make premature publication observable.
        write(layer, 'previous paint file')
        Project.saveLua = function() error('injected project encoder failure') end
        assert(not saveProject(project))
        assert(read(layer) == 'previous paint file' and read(project) == oldProject)
        assert(state.modified)
        Project.saveLua = saveLua
        failPublish(project)
        assert(not saveProject(project))
        assert(read(layer) == 'previous paint file' and read(project) == oldProject)
        assert(state.modified and state.projectPath == project)
        os.rename = rename
        assert(saveProject(project))
        assert(mbm.readImagePixels(layer) == layerBytes and not state.modified)

        mbm.saveFile = function() return exportedMesh end
        -- Simulate a material left in the private preview by an earlier selection.
        assert(state.previewAsset:setTexture(1, 2, state.previewTexturePath))
        write(exportedMesh, 'previous mesh file'); write(exportedTexture, 'previous texture file')
        state.exportDirty = true
        local oldMeshPath, oldTexturePath = state.exportMeshPath, state.exportTexturePath
        state.sourceAsset.save = function() return false end
        assert(not saveMesh())
        assert(read(exportedMesh) == 'previous mesh file' and read(exportedTexture) == 'previous texture file')
        state.sourceAsset.save = sourceSave
        failPublish(exportedMesh)
        assert(not saveMesh())
        assert(read(exportedMesh) == 'previous mesh file' and read(exportedTexture) == 'previous texture file')
        assert(state.exportMeshPath == oldMeshPath and state.exportTexturePath == oldTexturePath and state.exportDirty)
        os.rename = rename
        assert(saveMesh())
        assert(state.exportMeshPath == exportedMesh and state.exportTexturePath == exportedTexture and not state.exportDirty)
        assert(mbm.readImagePixels(exportedTexture) == expected, 'Exported texture differs from the paint composition')
        local mesh = meshDebug:new()
        assert(mesh:load(exportedMesh) and mesh:check())
        local beforeAnim, afterAnim = table.pack(state.sourceAsset:getAnim(1)), table.pack(mesh:getAnim(1))
        assert(beforeAnim.n == afterAnim.n)
        for i = 1, beforeAnim.n do assert(beforeAnim[i] == afterAnim[i], 'Export changed animation') end
        assert(mesh:getTotalFrame() == state.sourceAsset:getTotalFrame())
        for frame = 1, state.sourceAsset:getTotalFrame() do
            assert(mesh:getTotalSubset(frame) == state.sourceAsset:getTotalSubset(frame))
            for subset = 1, state.sourceAsset:getTotalSubset(frame) do
                local count = state.sourceAsset:getTotalVertex(frame, subset)
                assert(mesh:getTotalVertex(frame, subset) == count)
                local before = state.sourceAsset:getVertex(frame, subset, 1, count)
                local after = mesh:getVertex(frame, subset, 1, count)
                for i, vertex in ipairs(before) do
                    for key, value in pairs(vertex) do assert(after[i][key] == value, 'Export changed vertex ' .. key) end
                end
                local beforeIndices, afterIndices = state.sourceAsset:getIndex(frame, subset), mesh:getIndex(frame, subset)
                if beforeIndices then
                    assert(afterIndices and #beforeIndices == #afterIndices)
                    for i, value in ipairs(beforeIndices) do assert(afterIndices[i] == value) end
                else
                    assert(not afterIndices)
                end
            end
        end
        assert(mbm.getFullPath(mesh:getTexture(1, state.target.subsets[1])) == exportedTexture,
            'Exported mesh references the wrong texture')
        assert(state.previewAsset:getTexture(1, state.target.subsets[1]) == previewTexture,
            'Export mutated the preview material')
        assert(mesh:getTexture(1, 2) == state.sourceAsset:getTexture(1, 2),
            'Export inherited a private texture from another preview material')
        assert(read(state.meshPath) == sourceMeshBytes and read(state.target.path) == sourceTextureBytes,
            'Saving overwrote the source files')

        mbm.saveFile = function() return standalone end
        write(standalone, 'previous standalone texture')
        failPublish(standalone)
        state.exportDirty = true
        assert(not saveTexture())
        assert(read(standalone) == 'previous standalone texture' and state.exportDirty)
        assert(state.exportTexturePath == exportedTexture)
        os.rename = rename
        assert(saveTexture() and not state.exportDirty)
        assert(mbm.readImagePixels(standalone) == expected)
        mbm.saveFile = function() return state.target.path end
        assert(not saveTexture(), 'Source texture can be overwritten')
        assert(read(state.target.path) == sourceTextureBytes)
    end, debug.traceback)
    mbm.saveFile, mbm.writeImagePixels, os.rename, Project.saveLua = saveFile, writePixels, rename, saveLua
    state.sourceAsset.save = sourceSave
    assert(state.previewAsset:setTexture(1, 2, otherPreviewTexture))
    for _, path in ipairs({project, layer, exportedMesh, exportedTexture, standalone}) do os.remove(path) end
    assert(ok, err)
    print('MESH TEXTURE PAINT SAVE CHECKS PASS')
end

return M
