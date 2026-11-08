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

local function clamp(value, minimum, maximum)
    if value < minimum then return minimum end
    if value > maximum then return maximum end
    return value
end

local function byte(value)
    return clamp(math.floor(value + .5), 0, 255)
end

local function coordinate(value, maximum)
    return clamp(math.floor(tonumber(value) or 0), 0, maximum)
end

local function normalizedBounds(first, second, maximum)
    first, second = coordinate(first, maximum), coordinate(second, maximum)
    if first > second then first, second = second, first end
    return first, second
end

function M.normalizeSelection(x0, y0, x1, y1, width, height, shape)
    width, height = math.floor(tonumber(width) or 0), math.floor(tonumber(height) or 0)
    if width < 1 or height < 1 then return nil end
    x0, x1 = normalizedBounds(x0, x1, width - 1)
    y0, y1 = normalizedBounds(y0, y1, height - 1)
    if shape == 'square' or shape == 'circle' then
        local side = math.min(x1 - x0, y1 - y0)
        x1, y1 = math.min(width - 1, x0 + side), math.min(height - 1, y0 + side)
    end
    if x1 <= x0 or y1 <= y0 then return nil end
    return {x=x0, y=y0, width=x1-x0+1, height=y1-y0+1,
        shape=shape == 'circle' and 'circle' or 'square'}
end

local function sourcePixel(bytes, width, x, y)
    local offset = (y * width + x) * 4 + 1
    return bytes:byte(offset), bytes:byte(offset + 1), bytes:byte(offset + 2), bytes:byte(offset + 3)
end

function M.crop(bytes, width, height, selection)
    assert(type(bytes) == 'string', 'stamp source bytes are required')
    assert(type(selection) == 'table', 'stamp selection is required')
    local rows = {}
    local centerX = (selection.width - 1) * .5
    local centerY = (selection.height - 1) * .5
    local radiusX = math.max(.5, selection.width * .5)
    local radiusY = math.max(.5, selection.height * .5)
    for row = 0, selection.height - 1 do
        local pixels = {}
        for column = 0, selection.width - 1 do
            local sourceX = clamp(selection.x + column, 0, width - 1)
            local sourceY = clamp(selection.y + row, 0, height - 1)
            local red, green, blue, alpha = sourcePixel(bytes, width, sourceX, sourceY)
            if selection.shape == 'circle' then
                local dx = (column - centerX) / radiusX
                local dy = (row - centerY) / radiusY
                if dx * dx + dy * dy > 1 then alpha = 0 end
            end
            pixels[#pixels + 1] = string.char(byte(red), byte(green), byte(blue), byte(alpha))
        end
        rows[#rows + 1] = table.concat(pixels)
    end
    return {width=selection.width, height=selection.height, bytes=table.concat(rows),
        radius=math.max(selection.width, selection.height) * .5, selection=selection}
end

return M
