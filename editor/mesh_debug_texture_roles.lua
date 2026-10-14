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

]]--

local M = {}
local roleValues = {'primary', 'normal', 'specular', 'emissive', 'mask'}
local roleKeys = {'tex_role_primary', 'tex_role_normal', 'tex_role_specular', 'tex_role_emissive', 'tex_role_mask'}

-- With no single mesh target, leave texture names empty instead of scanning the batch.
function M.draw(tx, index, meshD, dpCall)
    local roleIndex = 1
    for i = 1, #roleValues do
        if roleValues[i] == tx.role then
            roleIndex = i
            break
        end
    end
    local specificTextureTarget = meshD and tx.frame > 0 and tx.subset > 0
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
            tImGui.Text(tLang.L(roleKeys[roleRow]))
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
end

return M
