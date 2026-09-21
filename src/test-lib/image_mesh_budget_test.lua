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
|-----------------------------------------------------------------------------------------------------------------------]]

package.path='editor/?.lua;'..package.path
tLang=require 'lang.language'
local Budget=require 'image_mesh_budget'
for _,language in ipairs({'en','pt_br'}) do
    tLang.current=language
    local function L(k) return tLang.L('ime_'..k) end
    local region={name='module_002'}
    local result=Budget.error(region,'Geometry budget exceeded during painted height refinement: vertices >= 80000, limit 65535; triangles >= 140000, limit 131070;')
    assert(result:find('80000',1,true) and result:find('140000',1,true))
    assert(result:find(L('budget_stage_painting'),1,true))
    assert(result:find(L('budget_generation_first'),1,true))
    result=Budget.error(region,'Geometry budget exceeded including side texture seams: vertices 70000, limit 65535; triangles 100000, limit 131070. Reduce side repeats or geometry resolution.')
    assert(result:find(string.format(L('budget_exact'),L('maxVertices'),70000,65535),1,true))
    assert(not result:find('100000',1,true),'resource within its budget was reported as exceeded')
    assert(result:find(L('budget_advice_seams'),1,true))
    assert(Budget.error(region,'Unexpected native error')=='module_002: Unexpected native error')
    local lines={}
    tImGui={CollapsingHeader=function() return true end,TextWrapped=function(text) lines[#lines+1]=text end}
    local report={vertices=100,triangles=200,sourceVertices=950,sourceTriangles=1800,vertexLimit=1000,triangleLimit=2000}
    local E={report=report,values={maxVertices=10}}
    Budget.panel(E)
    assert(table.concat(lines):find('950 / 1000',1,true),'must use applied limits, not draft')
    assert(table.concat(lines):find(L('budget_near'),1,true))
    assert(table.concat(lines):find(string.format(L('budget_final'),100,200),1,true))
    for _,flag in ipairs({'previewStale','drag','paintDrag','meshTask'}) do
        E[flag]=true;lines={};Budget.panel(E)
        assert(table.concat(lines):find(L('budget_pending'),1,true))
        assert(not table.concat(lines):find('950',1,true),'stale generation usage shown')
        E[flag]=nil
    end
    E.report=nil;lines={};Budget.panel(E);assert(lines[2]==L('budget_pending'))
    tImGui.CollapsingHeader=function() return false end
    lines={};Budget.panel(E);assert(#lines==0)
end
print('IMAGE MESH BUDGET FORMATS / STAGES / APPLIED LIMITS / SIMPLIFICATION / STALE / LOCALIZATION OK')
