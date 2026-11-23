--[[
-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                    |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                     |
|                                                                                                                      |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated         |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation      |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and     |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                   |
|                                                                                                                      |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of      |
| the Software.                                                                                                        |
|                                                                                                                      |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR|
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.     |
|                                                                                                                      |
|----------------------------------------------------------------------------------------------------------------------|
]]--

local M = {}

local function newDraft(boneCount)
    return {name = 'socket', bone = boneCount > 0 and 1 or 0,
        tx = 0, ty = 0, tz = 0, rx = 0, ry = 0, rz = 0, rw = 1,
        sx = 1, sy = 1, sz = 1}
end

local function refresh(state, meshD, safeCall)
    local okBones, report = safeCall(function() return meshD:getSkeletonBindReport(false) end)
    state.bones = okBones and type(report) == 'table' and report.bones or {}
    local okSockets, sockets = safeCall(function() return meshD:getSkeletalSocketReport() end)
    state.sockets = okSockets and type(sockets) == 'table' and sockets or {}
    if state.selected > #state.sockets then state.selected = 0 end
end

local function loadDraft(state, socketIndex)
    local socket = state.sockets[socketIndex]
    if not socket then
        state.selected = 0
        state.draft = newDraft(#state.bones)
        return
    end
    local boneIndex = 1
    for index, bone in ipairs(state.bones) do
        if bone.name == socket.boneName then boneIndex = index break end
    end
    state.draft = {name = socket.name or 'socket', bone = boneIndex,
        tx = socket.translationX or 0, ty = socket.translationY or 0,
        tz = socket.translationZ or 0, rx = socket.rotationX or 0,
        ry = socket.rotationY or 0, rz = socket.rotationZ or 0,
        rw = socket.rotationW or 1, sx = socket.scaleX or 1,
        sy = socket.scaleY or 1, sz = socket.scaleZ or 1}
end

local function applyMutation(state, meshD, safeCall, callbacks, operation, historyKey, successKey)
    local snapshot = callbacks.stage(historyKey)
    local ok = snapshot and select(1, safeCall(operation)) or false
    if not ok then
        if snapshot then callbacks.discard(snapshot) end
        return false
    end
    callbacks.commit(snapshot, historyKey)
    state.modified = true
    callbacks.refresh()
    callbacks.rebuild()
    callbacks.status(successKey, false)
    return true
end

function M.draw(state, meshD, index, safeCall, callbacks, lang, imgui)
    state.socketAuthoring = state.socketAuthoring or {
        data = nil, sockets = {}, bones = {}, selected = 0, draft = newDraft(0),
        lastLoadedSelected = nil
    }
    local authoring = state.socketAuthoring
    if authoring.data ~= meshD then
        authoring.data = meshD
        authoring.sockets = {}
        authoring.bones = {}
        authoring.selected = 0
        authoring.lastObservedBoneIndex = nil
        authoring.lastLoadedSelected = nil
        authoring.draft = newDraft(0)
        refresh(authoring, meshD, safeCall)
    end

    local selection = state.boneEditorSelection
    local selectedBoneIndex = selection and selection.boneIndex or state.boneIndex
    if selectedBoneIndex and selectedBoneIndex >= 1 and selectedBoneIndex <= #authoring.bones and
            authoring.lastObservedBoneIndex ~= selectedBoneIndex then
        authoring.lastObservedBoneIndex = selectedBoneIndex
        authoring.draft.bone = selectedBoneIndex
    end

    if authoring.lastLoadedSelected ~= authoring.selected then
        loadDraft(authoring, authoring.selected)
        authoring.lastLoadedSelected = authoring.selected
    end

    imgui.TextDisabled(lang.L('swl_socket_authoring_help'))
    local editInView=imgui.Checkbox(lang.L('swl_socket_view_edit') ..
        '##swlSocketViewEdit-' .. index,state.socketEditMode)
    if editInView ~= state.socketEditMode then
        state.socketEditMode = editInView
        state.socketGizmo.drag = nil
        if editInView then
            state.boneEditorSelection = nil
            state.boneEditorSelectedIndex = nil
            state.boneEditorPendingCycle = nil
            state.socketGizmo.selectedIndex = authoring.selected > 0 and authoring.selected or nil
        end
        if callbacks.visuals then callbacks.visuals() end
    end
    if imgui.IsItemHovered() and imgui.BeginTooltip() then
        imgui.Text(lang.L('swl_socket_view_edit_help'))
        imgui.EndTooltip()
    end
    if imgui.Button(lang.L('swl_socket_refresh') .. '##swlSocketRefresh-' .. index) then
        refresh(authoring, meshD, safeCall)
        loadDraft(authoring, authoring.selected)
        authoring.lastLoadedSelected = authoring.selected
        state.socketGizmo.selectedIndex = authoring.selected > 0 and authoring.selected or nil
        if callbacks.visuals then callbacks.visuals() end
    end
    if #authoring.bones == 0 then
        imgui.TextDisabled(lang.L('swl_socket_requires_skeleton'))
        return
    end

    local socketOptions = {lang.L('swl_socket_new')}
    for _, socket in ipairs(authoring.sockets) do
        socketOptions[#socketOptions + 1] = socket.name or '<unnamed>'
    end
    local changed, selected = imgui.Combo(lang.L('swl_socket_select') .. '##swlSocketSelect-' .. index,
        authoring.selected + 1, socketOptions, -1)
    if changed and selected then
        authoring.selected = selected - 1
        loadDraft(authoring, authoring.selected)
        authoring.lastLoadedSelected = authoring.selected
        state.socketGizmo.selectedIndex = authoring.selected > 0 and authoring.selected or nil
        if callbacks.visuals then callbacks.visuals() end
    end

    local draft = authoring.draft
    imgui.PushItemWidth(210)
    local nameChanged, name = imgui.InputText(lang.L('swl_socket_name') .. '##swlSocketName-' .. index,
        draft.name or '', imgui.Flags('ImGuiInputTextFlags_None'))
    if nameChanged then draft.name = name end
    local boneOptions = {}
    for _, bone in ipairs(authoring.bones) do boneOptions[#boneOptions + 1] = bone.name or '<unnamed>' end
    local boneChanged, bone = imgui.Combo(lang.L('swl_socket_bone') .. '##swlSocketBone-' .. index,
        draft.bone, boneOptions, -1)
    if boneChanged and bone then draft.bone = bone end
    imgui.PopItemWidth()

    local noFlags = imgui.Flags('ImGuiInputTextFlags_None')
    local function socketFloat(label, key)
        imgui.PushItemWidth(90)
        local valueChanged, value = imgui.InputFloat(label .. '##swlSocket-' .. key .. '-' .. index,
            draft[key] or 0, 0.01, 0.1, '%.4f', noFlags)
        imgui.PopItemWidth()
        if valueChanged and value then draft[key] = value end
    end
    imgui.Text(lang.L('swl_socket_translation'))
    socketFloat('X', 'tx'); imgui.SameLine(); socketFloat('Y', 'ty'); imgui.SameLine(); socketFloat('Z', 'tz')
    imgui.Text(lang.L('swl_socket_rotation'))
    socketFloat('X', 'rx'); imgui.SameLine(); socketFloat('Y', 'ry'); imgui.SameLine(); socketFloat('Z', 'rz')
    socketFloat('W', 'rw')
    imgui.Text(lang.L('swl_socket_scale'))
    socketFloat('X', 'sx'); imgui.SameLine(); socketFloat('Y', 'sy'); imgui.SameLine(); socketFloat('Z', 'sz')

    local isNew = authoring.selected == 0
    if imgui.Button(lang.L(isNew and 'swl_socket_add' or 'swl_socket_update') .. '##swlSocketSave-' .. index) then
        local function operation()
            if isNew then
                return meshD:addSkeletalSocket(draft.name, draft.bone, draft.tx, draft.ty, draft.tz,
                    draft.rx, draft.ry, draft.rz, draft.rw, draft.sx, draft.sy, draft.sz)
            end
            return meshD:updateSkeletalSocket(authoring.selected, draft.name, draft.bone,
                draft.tx, draft.ty, draft.tz, draft.rx, draft.ry, draft.rz, draft.rw,
                draft.sx, draft.sy, draft.sz)
        end
        if applyMutation(state, meshD, safeCall, callbacks, operation,
                isNew and 'swl_socket_added' or 'swl_socket_updated',
                isNew and 'swl_socket_added' or 'swl_socket_updated') then
            refresh(authoring, meshD, safeCall)
            if isNew then authoring.selected = #authoring.sockets end
            loadDraft(authoring, authoring.selected)
            authoring.lastLoadedSelected = authoring.selected
            state.socketGizmo.selectedIndex = authoring.selected > 0 and authoring.selected or nil
            if callbacks.visuals then callbacks.visuals() end
        end
    end
    if not isNew then
        imgui.SameLine()
        if imgui.Button(lang.L('swl_socket_remove') .. '##swlSocketRemove-' .. index) then
            if applyMutation(state, meshD, safeCall, callbacks,
                    function() return meshD:removeSkeletalSocket(authoring.selected) end,
                    'swl_socket_removed', 'swl_socket_removed') then
                authoring.selected = 0
                refresh(authoring, meshD, safeCall)
                loadDraft(authoring, 0)
                authoring.lastLoadedSelected = 0
                state.socketGizmo.selectedIndex = nil
                if callbacks.visuals then callbacks.visuals() end
            end
        end
    end
end

return M
