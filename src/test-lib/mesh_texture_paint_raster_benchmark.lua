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
]]--

local source = debug.getinfo(1, 'S').source
local testDirectory = source:sub(1, 1) == '@' and source:sub(2):match('^(.*[/\\])') or ''
package.path = testDirectory .. '../../editor/?.lua;' .. package.path

local Raster = require 'mesh_texture_paint_raster'
local requestedSize = tonumber(arg and arg[1])
local sizes = requestedSize and {requestedSize} or {1024, 2048, 4096}

for _, size in ipairs(sizes) do
    assert(size >= 1 and size == math.floor(size), 'benchmark size must be a positive integer')
    local layer = Raster.newLayer(size, size, 32)
    local center = math.floor(size * .5)
    local paint = Raster.applyDab(layer, {
        x = center, y = center, radius = 8, hardness = 1, opacity = 1,
        color = {r = 220, g = 80, b = 40, a = 255},
    })
    assert(paint.changedPixels > 0)
    local cloneStarted = os.clock()
    local padded = Raster.cloneLayer(layer)
    local cloneElapsed = os.clock() - cloneStarted
    local started = os.clock()
    local result = Raster.dilate(padded, 8, true)
    local elapsed = os.clock() - started
    assert(result.changedPixels > 0)
    print(string.format(
        'MESH TEXTURE PAINT DILATE BENCH size=%d clone=%.6f dilate=%.6f changed=%d',
        size, cloneElapsed, elapsed, result.changedPixels))
end
