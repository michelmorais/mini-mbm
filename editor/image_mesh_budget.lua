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
local function L(key) return tLang.L('ime_'..key) end
local stages={
    {'for manual curved relief','curved'},
    {'including side texture seams','seams'},
    {'during painted height refinement','painting'},
    {'while aligning grooves','alignment'},
    {'after groove alignment','alignment'},
    {'during contour refinement','contour'},
    {'by contour','contour'},
}

-- Both native budget formats are supported. Do not relabel resources that fit.
function M.error(region,message)
    message=tostring(message)
    if not message:find('Geometry budget exceeded',1,true) then return region.name..': '..message end
    local details={}
    for kind,relation,count,limit in message:gmatch('(%a+) ([>= ]*)(%d+), limit (%d+)') do
        count,limit=tonumber(count),tonumber(limit)
        if (kind=='vertices' or kind=='triangles') and count>limit then
            local label=L(kind=='vertices' and 'maxVertices' or 'maxTriangles')
            details[#details+1]=string.format(L(relation:find('>',1,true) and 'budget_exceeded' or 'budget_exact'),label,count,limit)
        end
    end
    if #details==0 then return region.name..': '..message end
    local stage='base'
    for _,entry in ipairs(stages) do
        if message:find(entry[1],1,true) then stage=entry[2];break end
    end
    return region.name..': '..table.concat(details,'\n')..'\n'..L('budget_stage_'..stage)..'\n'..L('budget_advice_'..stage)..'\n'..L('budget_generation_first')
end

function M.panel(E)
    if not tImGui.CollapsingHeader(L('budget_diagnostics')) then return end
    tImGui.TextWrapped(L('budget_scope'))
    local report=E.report
    if not report or E.drag or E.paintDrag or E.previewStale or E.meshTask then
        tImGui.TextWrapped(L('budget_pending'))
        return
    end
    local vertexLimit,triangleLimit=report.vertexLimit,report.triangleLimit
    if not vertexLimit or not triangleLimit then return end
    local vertices=report.sourceVertices or report.vertices
    local triangles=report.sourceTriangles or report.triangles
    tImGui.TextWrapped(string.format(L('budget_usage'),L('maxVertices'),vertices,vertexLimit,100*vertices/vertexLimit))
    tImGui.TextWrapped(string.format(L('budget_usage'),L('maxTriangles'),triangles,triangleLimit,100*triangles/triangleLimit))
    if report.sourceVertices then
        tImGui.TextWrapped(string.format(L('budget_final'),report.vertices,report.triangles))
    end
    if vertices>=vertexLimit*0.9 or triangles>=triangleLimit*0.9 then
        tImGui.TextWrapped(L('budget_near'))
    end
    tImGui.TextWrapped(L('budget_generation_first'))
end
return M
