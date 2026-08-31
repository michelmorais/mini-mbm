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

local source = debug.getinfo(1, 'S').source
local testDirectory = source:sub(1, 1) == '@' and source:sub(2):match('^(.*[/\\])') or ''
package.path = testDirectory .. '../../editor/?.lua;' .. package.path

local helper = require 'mesh_debug_export_helper'

local nonIndexed = assert(helper.buildGlobalTriangleIndices(nil, 6, 10))
assert(table.concat(nonIndexed, ',') == '11,12,13,14,15,16')

local indexed = assert(helper.buildGlobalTriangleIndices({1, 3, 2}, 3, 7))
assert(table.concat(indexed, ',') == '8,10,9')

assert(helper.buildGlobalTriangleIndices(nil, 4, 0) == nil)
assert(helper.buildGlobalTriangleIndices({1, 2, 4}, 3, 0) == nil)
assert(helper.buildGlobalTriangleIndices({1, 2}, 3, 0) == nil)
