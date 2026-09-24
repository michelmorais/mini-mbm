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

local M={}
function M.vertices(asset,rotate)
    local all={}
    for subset=1,asset:getTotalSubset(1) do
        local list=asset:getVertex(1,subset,1,asset:getTotalVertex(1,subset))
        for _,v in ipairs(list) do
            if rotate then v.x=-v.x;v.z=-v.z;v.nx=-v.nx;v.nz=-v.nz end
            all[#all+1]=v
        end
        if rotate then asset:setVertex(1,subset,1,list) end
    end
    return all
end
function M.geometry(asset)
    local all,indices={},{}
    for subset=1,asset:getTotalSubset(1) do
        local offset=#all
        for _,v in ipairs(asset:getVertex(1,subset,1,asset:getTotalVertex(1,subset))) do all[#all+1]=v end
        local subsetIndices=asset:getIndex(1,subset)
        if subsetIndices then
            for _,i in ipairs(subsetIndices) do indices[#indices+1]=offset+i end
        else
            for i=offset+1,#all do indices[#indices+1]=i end
        end
    end
    return all,indices
end
return M
