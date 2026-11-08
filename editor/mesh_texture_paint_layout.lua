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

local M = {}

local function positiveInteger(value, fallback)
    value = tonumber(value)
    if not value or value < 1 then return fallback end
    return math.max(1, math.floor(value + .5))
end

-- Return a layout in ImGui display coordinates. The display dimensions must
-- be the real client/backbuffer dimensions, not the engine's logical design
-- resolution returned by mbm.getSizeScreen().
function M.externalStamp(screenWidth, screenHeight, sourceWidth, sourceHeight, menuBarHeight)
    screenWidth = positiveInteger(screenWidth, 1)
    screenHeight = positiveInteger(screenHeight, 1)
    sourceWidth = positiveInteger(sourceWidth, 1)
    sourceHeight = positiveInteger(sourceHeight, 1)
    menuBarHeight = math.max(0, tonumber(menuBarHeight) or 0)

    local edgeMargin = math.min(32, screenWidth * .05)
    -- The confirmation UI uses three rows: help text, shape radios, and
    -- confirm/cancel buttons. Keep enough client space for all of them.
    local footerHeight = math.min(96, math.max(80, screenHeight * .15))
    local footerGap = math.min(16, math.max(8, screenHeight * .02))
    local imageTop = math.min(screenHeight, menuBarHeight + 16)
    local footerY = math.max(imageTop, screenHeight - footerHeight)
    local imageBottom = math.max(imageTop + 1, footerY - footerGap)
    local imageAreaWidth = math.max(1, screenWidth - edgeMargin * 2)
    local imageAreaHeight = math.max(1, imageBottom - imageTop)
    local scale = math.min(imageAreaWidth / sourceWidth, imageAreaHeight / sourceHeight)
    local imageWidth = sourceWidth * scale
    local imageHeight = sourceHeight * scale

    return {
        imageX = (screenWidth - imageWidth) * .5,
        imageY = imageTop + (imageAreaHeight - imageHeight) * .5,
        imageWidth = imageWidth,
        imageHeight = imageHeight,
        footerX = edgeMargin,
        footerY = footerY,
        footerHeight = footerHeight,
        screenWidth = screenWidth,
        screenHeight = screenHeight,
    }
end

return M
