--[[---------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------]]

local M = {}

function M.buildGlobalTriangleIndices(localIndices, vertexCount, globalVertexOffset)
    vertexCount = math.max(0, math.floor(tonumber(vertexCount) or 0))
    globalVertexOffset = math.max(0, math.floor(tonumber(globalVertexOffset) or 0))
    local result = {}
    if type(localIndices) == 'table' and #localIndices > 0 then
        if #localIndices % 3 ~= 0 then return nil, 'indexed triangle list is not divisible by 3' end
        for _, index in ipairs(localIndices) do
            local numericIndex = tonumber(index)
            if not numericIndex or numericIndex < 1 or numericIndex > vertexCount then
                return nil, 'triangle index is outside the subset vertex range'
            end
            result[#result + 1] = math.floor(numericIndex) + globalVertexOffset
        end
        return result
    end
    if vertexCount % 3 ~= 0 then
        return nil, 'non-indexed triangle list is not divisible by 3'
    end
    for vertex = 1, vertexCount do
        result[#result + 1] = globalVertexOffset + vertex
    end
    return result
end

return M
