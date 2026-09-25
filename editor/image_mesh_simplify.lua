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

-- One engine worker at a time. Callers keep their normal control flow across yields.
local M={}
function M.resume(E)
    local task=E.meshTask
    if not task then return end
    local result=table.pack(coroutine.resume(task))
    if not result[1] or coroutine.status(task)=='dead' then
        E.meshTask=nil; E.simplifyProgress=nil
    end
    if not result[1] then error(result[2],0) end
    return table.unpack(result,2,result.n)
end
function M.run(E,fn,...)
    if E.meshTask then return false end
    local args=table.pack(...)
    E.meshTask=coroutine.create(function() return fn(table.unpack(args,1,args.n)) end)
    return M.resume(E)
end
function M.apply(E,asset,options,report)
    if not options.simplify then return end
    local worker,err=require('mesh_simplify_pipeline').start(asset,options.simplifyMode,
        options.simplifyRatio,nil,1,options.simplifyDetails,options.simplifyBoundary,
        options.planarAngle,options.planarTolerance,options.maxVertices,true)
    if not worker then error(string.format(tLang.L('simplify_failed_fmt'),tostring(err)),0) end
    E.simplifyAsset=worker;E.simplifyCancelRequested=nil;E.simplifyProgress=0
    coroutine.yield()
    while true do
        local status=worker:getSimplifyStatus()
        if status.state~='running' then
            local cancelled=E.simplifyCancelRequested or status.state=='cancelled'
            E.simplifyAsset=nil;E.simplifyCancelRequested=nil;E.simplifyProgress=nil
            if cancelled then
                E.generationCancelled=true;E.batch=nil;E.statisticsRequested=nil
                error('ime_generation_cancelled',0)
            end
        end
        if status.state=='running' then E.simplifyProgress=status.progress or 0 end
        if status.state=='failed' then
            error(string.format(tLang.L('simplify_failed_fmt'),tostring(status.error)),0)
        end
        if status.state=='completed' then
            report.simplification=status.report
            report.sourceTriangles=report.triangles; report.sourceVertices=report.vertices
            report.triangles=status.report.resultTriangleCount
            report.vertices=status.report.resultVertexCount
            return
        end
        coroutine.yield()
    end
end
return M
