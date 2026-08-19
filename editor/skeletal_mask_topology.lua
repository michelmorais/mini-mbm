--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
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

local function copyMask(mask)
    local copied = {}
    for index,selected in pairs(mask or {}) do
        if selected then copied[index] = true end
    end
    return copied
end

local function closeCompatibleSeams(mask,seamByVertex)
    local closed = copyMask(mask)
    for index in pairs(mask) do
        for _,copyIndex in ipairs(seamByVertex[index] or {}) do
            closed[copyIndex] = true
        end
    end
    return closed
end

local function growOneRing(mask,adjacency,seamByVertex,includeSeams)
    local current = includeSeams and closeCompatibleSeams(mask,seamByVertex) or copyMask(mask)
    local grown = copyMask(current)
    for index in pairs(current) do
        for neighbor in pairs(adjacency[index] or {}) do
            grown[neighbor] = true
        end
    end
    return includeSeams and closeCompatibleSeams(grown,seamByVertex) or grown
end

local function shrinkOneRing(mask,adjacency,seamByVertex,includeSeams)
    local current = copyMask(mask)
    local boundary = {}
    for index in pairs(current) do
        local group = includeSeams and seamByVertex[index] or nil
        group = group or {index}
        local touchesOutside = false
        for _,member in ipairs(group) do
            if not current[member] then
                touchesOutside = true
                break
            end
            for neighbor in pairs(adjacency[member] or {}) do
                if not current[neighbor] then
                    touchesOutside = true
                    break
                end
            end
            if touchesOutside then break end
        end
        if touchesOutside then
            for _,member in ipairs(group) do
                if current[member] then boundary[member] = true end
            end
        end
    end
    local shrunk = {}
    for index in pairs(current) do
        if not boundary[index] then shrunk[index] = true end
    end
    return shrunk
end

function M.adjust(mask,adjacency,seamByVertex,ringCount,mode,includeSeams)
    local result = copyMask(mask)
    local rings = math.max(1,math.min(10,math.floor(tonumber(ringCount) or 1)))
    local operation = mode == 'shrink' and shrinkOneRing or growOneRing
    for _=1,rings do
        result = operation(result,adjacency or {},seamByVertex or {},includeSeams == true)
    end
    return result
end

function M.count(mask)
    local total = 0
    for _,selected in pairs(mask or {}) do
        if selected then total = total + 1 end
    end
    return total
end

return M
