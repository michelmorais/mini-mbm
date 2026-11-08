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
|------------------------------------------------------------------------------------------------------------------------|
]]--

local source = debug.getinfo(1, 'S').source
local testDirectory = source:sub(1, 1) == '@' and source:sub(2):match('^(.*[/\\])') or ''
package.path = testDirectory .. '../../editor/?.lua;' .. package.path

local Layout = require 'mesh_texture_paint_layout'

local function assertInside(layout, width, height)
    assert(layout.imageX >= 0, 'stamp image extends past the left edge')
    assert(layout.imageY >= 0, 'stamp image extends past the top edge')
    assert(layout.imageX + layout.imageWidth <= width + .001, 'stamp image extends past the right edge')
    assert(layout.imageY + layout.imageHeight <= layout.footerY + .001,
        'stamp image overlaps the confirmation controls')
    assert(layout.footerY >= 0 and layout.footerY < height, 'confirmation controls are outside the viewport')
    assert(layout.footerY + layout.footerHeight <= height + .001,
        'confirmation footer extends outside the viewport')
    assert(layout.footerHeight >= 80, 'confirmation footer is too short for its controls')
end

local fullScreen = Layout.externalStamp(1920, 1080, 4000, 3000, 24)
assertInside(fullScreen, 1920, 1080)

local scaledWindow = Layout.externalStamp(1280, 720, 600, 2400, 24)
assertInside(scaledWindow, 1280, 720)
assert(scaledWindow.imageHeight <= 720, 'portrait image was not fitted to the real client height')

local compactWindow = Layout.externalStamp(800, 600, 640, 480, 20)
assertInside(compactWindow, 800, 600)

print('mesh texture paint layout tests passed')
