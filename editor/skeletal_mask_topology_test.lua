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

local topology = require "skeletal_mask_topology"

local chain = {
    [1]={[2]=true},
    [2]={[1]=true,[3]=true},
    [3]={[2]=true,[4]=true},
    [4]={[3]=true,[5]=true},
    [5]={[4]=true},
}

local function expectMask(actual,expected)
    assert(topology.count(actual) == #expected)
    for _,index in ipairs(expected) do assert(actual[index] == true) end
end

expectMask(topology.adjust({[3]=true},chain,nil,1,'grow',false),{2,3,4})
expectMask(topology.adjust({[3]=true},chain,nil,2,'grow',false),{1,2,3,4,5})
expectMask(topology.adjust({[2]=true,[3]=true,[4]=true},chain,nil,1,'shrink',false),{3})
expectMask(topology.adjust({[2]=true,[3]=true,[4]=true},chain,nil,2,'shrink',false),{})

local seamAdjacency = {
    [1]={[2]=true},
    [2]={[1]=true},
    [20]={[21]=true},
    [21]={[20]=true},
}
local seams = {
    [2]={2,20},
    [20]={2,20},
}

expectMask(topology.adjust({[2]=true},seamAdjacency,seams,1,'grow',false),{1,2})
expectMask(topology.adjust({[2]=true},seamAdjacency,seams,1,'grow',true),{1,2,20,21})
expectMask(topology.adjust({[1]=true,[2]=true,[20]=true},seamAdjacency,seams,1,
    'shrink',true),{1})
expectMask(topology.adjust({[2]=true},seamAdjacency,seams,1,'shrink',true),{})

local original = {[3]=true}
topology.adjust(original,chain,nil,1,'grow',false)
expectMask(original,{3})

print('skeletal mask topology tests passed')
