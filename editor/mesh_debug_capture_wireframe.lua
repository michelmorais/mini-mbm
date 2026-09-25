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

local Wire = require 'image_mesh_wireframe'
local M = {}
function M.release(entry)
    if entry.captureWire then Wire.release(entry.captureWire) end
    entry.captureWire = nil
end
function M.update(entry, resolved)
    if entry.captureWire and entry.captureWire.resolved == resolved then return end
    M.release(entry)
    local groups = resolved.groups
    local adapter = {}
    function adapter:getTotalSubset() return #groups end
    function adapter:getTotalVertex(_,s) return #groups[s].vertices end
    function adapter:getVertex(_,s) return groups[s].vertices end
    function adapter:getIndex(_,s)
        local indices = {}
        for _,tri in ipairs(groups[s].triangles) do
            for _,v in ipairs(tri) do indices[#indices+1]=v end
        end
        return indices
    end
    local view = {preview=true, resolved=resolved}
    entry.captureWire = view
    Wire.ensure(view, adapter)
    view.wireObject:setPos(0,0,0)
    view.wireObject.alwaysOnTop = true
    view.wireObject.visible = true
end
return M
