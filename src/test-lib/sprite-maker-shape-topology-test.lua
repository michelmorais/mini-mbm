--[[
/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
|-----------------------------------------------------------------------------------------------------------------------*/
]]

-- Run from the repository root through mini-mbm with --disable_select_monitor.
-- Exercise real Sprite Maker constructors/callbacks without navigating its UI.
package.path = "editor/?.lua;" .. package.path
tImGui = require "ImGui"
local editor = setmetatable({}, {__index = _G})
assert(loadfile("editor/sprite_maker.lua", "t", editor))()
editor.iNumNickName = 0
editor.tFrameAddOptions = {}
local objects, checks = {}, {}
local started
local textureInfo = {file_name = "#FFFFFFFF", width = 100, height = 100}

local function observe(object, expectedFront, expectedVertices, named)
    objects[#objects+1] = object
    object.bKeepVisibleOnFirstRender = true
    object.alwaysRender = true
    local previousIndices
    object:onRender(function(self, vertex, uv, indices)
        local ok, err = pcall(function()
            assert(#indices == expectedFront * 2, "render topology lost its back faces")
            local returnedVertex, returnedUv = editor.onRenderShape(self, vertex, uv, indices)
            assert(returnedVertex == self.vertex and returnedUv == self.uv)
            assert(#returnedVertex == expectedVertices and #returnedUv == expectedVertices)
            local expected = named and expectedFront or expectedFront * 2
            assert(#self.index_read_only == expected, "wrong authoring index count")
            assert(#self.index_buffer_edit == expected, "wrong export index count")
            for i=1, expected do assert(self.index_read_only[i] == indices[i]) end
            if previousIndices then
                if named then assert(self.index_read_only == previousIndices, "rebuilt immutable indices") end
                assert(self.vertex[1].x == 7, "vertex edit discarded")
                assert(self.index_buffer_edit[1] == 2, "editable index reset")
                checks[self] = true
            else
                previousIndices = self.index_read_only
                self.vertex[1].x = 7
                self.index_buffer_edit[1] = 2
            end
        end)
        if not ok then print("SPRITE SHAPE TOPOLOGY FAIL: " .. tostring(err)); mbm.quit() end
        return self.vertex, self.uv
    end)
end

function onInitScene()
    started = mbm.getTimeRun()
    local ok, err = pcall(function()
        observe(editor.newRectShape(textureInfo, 100, 100, 2, {x=0,y=0}, {x=100,y=100}), 6, 4, true)
        for _, c in ipairs({{"rectangle",5,18,8}, {"circle",18,54,19}, {"triangle",4,12,6}}) do
            observe(editor.newShape(c[1], textureInfo, 100, 100, c[2]), c[3], c[4], true)
        end
        -- Explicit/custom topology (including intentionally double-sided imports)
        -- must never be cut in half by the editor's named-primitive adaptation.
        local custom = shape:new("2dw")
        assert(custom:createDynamicIndexed({-50,-50,0,50,50,-50}, {1,2,3,1,3,2}, {0,0,0.5,1,1,0}, "sprite-topology-custom"))
        observe(custom, 3, 3, false)
    end)
    if not ok then print("SPRITE SHAPE TOPOLOGY FAIL: " .. tostring(err)); mbm.quit() end
end

function onLoop()
    local count = 0
    for _ in pairs(checks) do count = count + 1 end
    if count == 5 then
        print("SPRITE SHAPE TOPOLOGY PASS (named primitives, custom topology, cached indices, vertex/index edits)")
        mbm.quit()
    elseif mbm.getTimeRun() - started > 5 then
        print("SPRITE SHAPE TOPOLOGY FAIL: callbacks did not complete")
        mbm.quit()
    end
end
