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

package.path = 'editor/?.lua;' .. package.path
local Model = require 'mesh_texture_paint_model'

local function dpCall(fn, ...)
    local result = table.pack(pcall(fn, ...))
    if not result[1] then print('MESH TEXTURE PAINT MESH SMOKE ERROR: ' .. tostring(result[2])) end
    return table.unpack(result, 1, result.n)
end

function onInitScene()
    local ok, errorMessage = xpcall(function()
        mbm.addPath('src/test-lib')
        local asset = meshDebug:new()
        assert(asset:load('Crate.msh'))
        assert(asset:check())
        local totalFrames = assert(asset:getTotalFrame())
        assert(totalFrames >= 1)

        local subsetCount = assert(asset:getTotalSubset(1))
        assert(subsetCount >= 1)
        local totalTriangles = 0
        for subset = 1, subsetCount do
            local snapshot, snapshotError = Model.snapshotSubset(asset, 1, subset, dpCall)
            assert(snapshot, snapshotError)
            assert(#snapshot.vertices > 0)
            assert(#snapshot.triangles > 0)
            totalTriangles = totalTriangles + #snapshot.triangles
        end
        assert(totalTriangles > 0)
        print('MESH TEXTURE PAINT MESH SMOKE PASS: frames=' .. totalFrames ..
            ' subsets=' .. subsetCount .. ' triangles=' .. totalTriangles)
    end, debug.traceback)
    if not ok then
        print('MESH TEXTURE PAINT MESH SMOKE FAIL: ' .. tostring(errorMessage))
    end
    mbm.quit()
end
