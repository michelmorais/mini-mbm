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

-- Read-only articulated playback for Mesh Debug. Authoring lives in its own editor.
local M={}
function M.draw(entry,data,preview,ready,call)
    local s=entry.articulatedPlayback
    if not s or s.data~=data or s.preview~=preview then
        s={data=data,preview=preview,clips={},selected=1}; entry.articulatedPlayback=s
        for i=1,data:getTotalArticulatedAnimations() do
            local name,_,_,priority=data:getArticulatedAnimation(i)
            s.clips[i]={name=name,priority=priority}
        end
    end
    if #s.clips==0 then tImGui.TextDisabled(tLang.L('articulated_no_clips')); return end
    local function stop()
        return preview:disableArticulatedAnimation(s.clips[s.selected].name)
    end
    local function play()
        local c=s.clips[s.selected]
        return preview:playArticulatedAnimation(c.name,c.priority,0,1)
    end
    tImGui.BeginDisabled(not ready)
    tImGui.PushItemWidth(220)
    if tImGui.BeginCombo(tLang.L('articulated_clip'),s.clips[s.selected].name) then
        for i,c in ipairs(s.clips) do
            -- Selection must not interrupt the active clip or restart one that has ended.
            if tImGui.Selectable(c.name..'##artPlay'..i,s.selected==i) and ready then s.selected=i end
        end
        tImGui.EndCombo()
    end
    tImGui.PopItemWidth()
    if tImGui.Button(tLang.L('ase_play')) and ready then call(play) end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('ase_pause')) and ready then call(function() return preview:pauseArticulatedAnimation(s.clips[s.selected].name) end) end
    tImGui.SameLine()
    if tImGui.Button(tLang.L('ase_tl_stop')) and ready then call(stop) end
    tImGui.EndDisabled()
    if not ready then tImGui.TextWrapped(tLang.L('articulated_save_to_preview')) end
    tImGui.TextWrapped(tLang.L('mesh_debug_articulated_controls'))
    tImGui.TextWrapped(tLang.L('ame_debug_playback'))
end
return M
