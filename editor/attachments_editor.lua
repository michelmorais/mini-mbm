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
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|------------------------------------------------------------------------------------------------------------------------|

   Attachments Editor

   Independent Phase 1 authoring skeleton for root-transform attachment definitions.
   It edits definition data and preview markers without depending on the interaction editor.
]]--

tImGui = require 'ImGui'
tUtil = require 'editor_utils'

local S = {
    definition = {
        version = 1,
        source = 'source',
        target = 'target',
        sourceType = 'root',
        inherit = {position = true, rotation = true, scale = false},
        offset = {
            position = {x = 100, y = 0, z = 0},
            rotation = {x = 0, y = 0, z = 0},
            scale = {x = 1, y = 1, z = 1},
        },
    },
    path = '',
    status = 'Ready',
    modified = false,
    camera = nil,
    sourceMarker = nil,
    targetMarker = nil,
    relationLine = nil,
    preview = {source = {x = 0, y = 0}, target = {x = 100, y = 0}},
    dirtyPreview = true,
}

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then print('[attachments_editor] ' .. tostring(result[2])) end
    return table.unpack(result, 1, result.n)
end

local function numberInput(label, value)
    local changed, nextValue = tImGui.InputFloat(label, value, 0.01, 0.1, '%.4f')
    return changed and nextValue or value, changed
end

local function saveDefinition(path)
    if not path or path == '' then path = mbm.saveFile('', 'att.lua') end
    if not path then return end
    local output = {'attachmentDefinition = {}'}
    tUtil.save('attachmentDefinition', S.definition, output)
    local file = io.open(path, 'w')
    if not file then
        S.status = 'Could not save definition'
        return
    end
    for _, line in ipairs(output) do file:write(line .. '\n') end
    file:close()
    S.path, S.modified, S.status = path, false, 'Saved ' .. path
end

local function loadDefinition(path)
    path = path or mbm.openFile(S.path or '', 'att.lua')
    if not path then return end
    local fn, err = loadfile(path)
    if not fn then S.status = err; return end
    attachmentDefinition = nil
    local ok, loadError = dpCall(fn)
    if not ok or type(attachmentDefinition) ~= 'table' then
        S.status = loadError or 'Invalid attachment definition'
        return
    end
    local loaded = attachmentDefinition
    S.definition.source = tostring(loaded.source or 'source')
    S.definition.target = tostring(loaded.target or 'target')
    S.definition.sourceType = tostring(loaded.sourceType or 'root')
    S.definition.inherit = loaded.inherit or S.definition.inherit
    S.definition.offset = loaded.offset or S.definition.offset
    S.path, S.modified, S.status = path, false, 'Loaded ' .. path
    S.dirtyPreview = true
end

local function markModified()
    S.modified = true
    S.dirtyPreview = true
end

local function rebuildPreview()
    local p = S.definition.offset.position
    S.preview.target.x = S.preview.source.x + (p.x or 0)
    S.preview.target.y = S.preview.source.y + (p.y or 0)
    if S.sourceMarker then S.sourceMarker:setPos(S.preview.source.x, S.preview.source.y) end
    if S.targetMarker then S.targetMarker:setPos(S.preview.target.x, S.preview.target.y) end
    if S.relationLine then
        S.relationLine:set({S.preview.source.x, S.preview.source.y,
                            S.preview.target.x, S.preview.target.y}, 1)
    end
    S.dirtyPreview = false
end

local function captureOffset()
    S.definition.offset.position.x = S.preview.target.x - S.preview.source.x
    S.definition.offset.position.y = S.preview.target.y - S.preview.source.y
    S.definition.offset.position.z = 0
    markModified()
    S.status = 'Captured preview offset'
end

local function showTransform(title, value, suffix)
    tImGui.Text(title)
    for _, axis in ipairs({'x', 'y', 'z'}) do
        local nextValue, changed = numberInput(axis .. suffix, value[axis] or 0)
        if changed then value[axis] = nextValue; markModified() end
    end
end

function onInitScene()
    S.camera = mbm.getCamera('2d')
    S.sourceMarker = shape:new('2dw', 0, 0)
    S.sourceMarker:create('rectangle', 30, 30)
    S.sourceMarker:setColor(0.2, 0.8, 0.3)
    S.targetMarker = shape:new('2dw', 100, 0)
    S.targetMarker:create('rectangle', 22, 22)
    S.targetMarker:setColor(0.9, 0.7, 0.1)
    S.relationLine = line:new('2dw', 0, 0)
    S.relationLine:setColor(0.8, 0.8, 0.8)
    S.relationLine:add({0, 0, 100, 0})
    rebuildPreview()
end

local function showMainWindow()
    tUtil.setInitialWindowPositionLeft('attachments_main', 20, 20, 430, 520, 760)
    local opened = tImGui.Begin('Attachments - Phase 1', true)
    if opened then
        tImGui.Text('Root transform attachment definition')
        tImGui.Separator()
        local nextSource, sourceChanged = tImGui.InputText('Source##attachment_source', S.definition.source)
        if sourceChanged then S.definition.source = nextSource; markModified() end
        local nextTarget, targetChanged = tImGui.InputText('Target##attachment_target', S.definition.target)
        if targetChanged then S.definition.target = nextTarget; markModified() end
        tImGui.Text('Source type: root')
        tImGui.Separator()
        tImGui.Text('Inherited components')
        for _, component in ipairs({'position', 'rotation', 'scale'}) do
            local changed, value = tImGui.Checkbox(component .. '##inherit_' .. component,
                                                    S.definition.inherit[component] ~= false)
            if changed then S.definition.inherit[component] = value; markModified() end
        end
        tImGui.Separator()
        showTransform('Offset position', S.definition.offset.position, '##offset_position')
        showTransform('Offset rotation (radians)', S.definition.offset.rotation, '##offset_rotation')
        showTransform('Offset scale', S.definition.offset.scale, '##offset_scale')
        tImGui.Separator()
        if tImGui.Button('Capture offset') then captureOffset() end
        tImGui.SameLine()
        if tImGui.Button('Save') then saveDefinition(S.path) end
        tImGui.SameLine()
        if tImGui.Button('Save as...') then saveDefinition(nil) end
        if tImGui.Button('Load') then loadDefinition(nil) end
        tImGui.SameLine()
        if tImGui.Button('Reset preview') then S.preview.target.x = 100; S.preview.target.y = 0; markModified() end
        tImGui.Text(S.status .. (S.modified and ' *' or ''))
        tImGui.Text('Green = source, yellow = target')
    end
    tImGui.End()
end

function onLoop(delta)
    if S.dirtyPreview then rebuildPreview() end
    showMainWindow()
end

function onKeyDown(key)
    if key == mbm.getKeyCode('ESC') then mbm.quit() end
end
