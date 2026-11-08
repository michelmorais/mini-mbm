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

local source = debug.getinfo(1, 'S').source
local testDirectory = source:sub(1, 1) == '@' and source:sub(2):match('^(.*[/\\])') or ''
package.path = testDirectory .. '../../editor/?.lua;' .. package.path

local Stamp = require 'mesh_texture_paint_stamp'

local function pixel(bytes, width, x, y)
    local offset = (y * width + x) * 4 + 1
    return bytes:byte(offset, offset + 3)
end

local sourceBytes = string.rep(string.char(10, 20, 30, 255), 25)
local square = assert(Stamp.normalizeSelection(1, 0, 4, 3, 5, 5, 'square'))
assert(square.x == 1 and square.y == 0 and square.width == 4 and square.height == 4,
    'square selection must be normalized to a square')
local squareCrop = Stamp.crop(sourceBytes, 5, 5, square)
assert(squareCrop.width == 4 and squareCrop.height == 4 and #squareCrop.bytes == 64,
    'square crop dimensions are incorrect')

local circle = assert(Stamp.normalizeSelection(0, 0, 4, 4, 5, 5, 'circle'))
assert(circle.width == 5 and circle.height == 5, 'circle selection must have a circular footprint')
local circleCrop = Stamp.crop(sourceBytes, 5, 5, circle)
local _, _, _, cornerAlpha = pixel(circleCrop.bytes, circleCrop.width, 0, 0)
local _, _, _, centerAlpha = pixel(circleCrop.bytes, circleCrop.width, 2, 2)
assert(cornerAlpha == 0 and centerAlpha == 255,
    'circle crop must clear pixels outside the circular selection')

local reversed = assert(Stamp.normalizeSelection(4, 4, 1, 1, 5, 5, 'square'))
assert(reversed.x == 1 and reversed.y == 1 and reversed.width == 4 and reversed.height == 4,
    'reversed drag must normalize selection bounds')

print('mesh texture paint stamp tests passed')
