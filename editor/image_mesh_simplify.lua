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
local function awaitWorker(E,worker)
    E.simplifyAsset=worker;E.simplifyProgress=0;E.simplifyCancelRequested=nil
    coroutine.yield()
    while true do
        local status=worker:getSimplifyStatus()
        if status.state~='running' then
            local cancelled=E.simplifyCancelRequested or status.state=='cancelled'
            E.simplifyAsset=nil;E.simplifyProgress=nil;E.simplifyCancelRequested=nil
            if cancelled then
                E.generationCancelled=true;E.batch=nil;E.statisticsRequested=nil
                error('ime_generation_cancelled',0)
            end
            if status.state=='failed' then
                error(string.format(tLang.L('simplify_failed_fmt'),tostring(status.error)),0)
            end
            return status.report
        end
        E.simplifyProgress=status.progress or 0
        coroutine.yield()
    end
end
local function applyTarget(E,asset,options,report)
    local sourceTriangles,sourceVertices=report.triangles,report.vertices
    local combined=options.simplifyMode=='cgal_qem'
    local qemFirst=(options.simplifyOrder or 'qem_cgal')=='qem_cgal'
    local planarWorker,planarReport
    local function planar(deferNormals)
        local worker,err=require('mesh_cgal').start(asset,nil,1,options.planarAngle,
            options.planarTolerance,options.maxVertices,true,deferNormals,nil,options.cgalRepairTopology~=false)
        if not worker then error(string.format(tLang.L('simplify_failed_fmt'),tostring(err)),0) end
        local result=awaitWorker(E,worker)
        report.triangles=result.resultTriangleCount;report.vertices=result.resultVertexCount
        return worker,result
    end
    if combined and not qemFirst then planarWorker,planarReport=planar(true) end
    -- Remeshing remains before target reduction so it cannot inflate the final budget.
    local preliminary=require('image_mesh_model').copy(options)
    preliminary.geometryTargetTriangles=nil;preliminary.simplify=false
    local remeshWorker=M.apply(E,asset,preliminary,report,true)
    local qemReport
    -- Large combined inputs need more bisection steps after an infeasible QEM target.
    -- Keep a finite limit; never relax topology or boundary protections to meet it.
    local count,err=require('image_mesh_frequency').reduce(options.geometryTargetTriangles,report.triangles,function(ratio)
        local ok,message=asset:startSimplify(ratio,nil,1,options.simplifyDetails,options.simplifyBoundary)
        if not ok then return nil,message end
        E.simplifyAsset=asset;E.simplifyProgress=0;E.simplifyCancelRequested=nil
        coroutine.yield()
        while true do
            local status=asset:getSimplifyStatus()
            if status.state~='running' then
                local cancelled=E.simplifyCancelRequested or status.state=='cancelled'
                E.simplifyAsset=nil;E.simplifyProgress=nil;E.simplifyCancelRequested=nil
                if cancelled then
                    E.generationCancelled=true;E.batch=nil;E.statisticsRequested=nil
                    error('ime_generation_cancelled',0)
                end
                if status.state=='failed' then
                    local message=tostring(status.error)
                    if message:find('topology constraints prevent reaching the requested triangle count',1,true)
                        or message:find('locked source boundaries require at least ',1,true) then
                        return nil,'constrained'
                    end
                    return nil,message
                end
                qemReport=status.report
                report.vertices=status.report.resultVertexCount
                return status.report.resultTriangleCount
            end
            E.simplifyProgress=status.progress or 0
            coroutine.yield()
        end
    end,combined and 16 or 7)
    if not count then error(string.format(tLang.L('simplify_failed_fmt'),tostring(err)),0) end
    report.triangles=count
    report.targetReductionAttempts=err
    if combined and qemFirst then planarWorker,planarReport=planar(false) end
    local normalsWorker=remeshWorker or planarWorker
    if combined and qemFirst then normalsWorker=planarWorker end
    if normalsWorker then
        local ok,vertices=normalsWorker:finalizeNormals()
        if not ok then error(string.format(tLang.L('simplify_failed_fmt'),tostring(vertices)),0) end
        if vertices then report.vertices=vertices end
    end
    local final=qemReport or planarReport
    if final then
        if combined then
            final.backend='cgal_qem';final.simplifyOrder=qemFirst and 'qem_cgal' or 'cgal_qem'
            final.qemRan=qemReport~=nil;final.cgal=planarReport.cgal
        end
        final.sourceTriangleCount=sourceTriangles;final.sourceVertexCount=sourceVertices
        final.resultTriangleCount=report.triangles;final.resultVertexCount=report.vertices
        report.simplification=final
    end
    report.sourceTriangles=sourceTriangles;report.sourceVertices=sourceVertices
end
function M.apply(E,asset,options,report,deferNormals)
    if options.voxelized then return end
    if options.geometryTargetTriangles then return applyTarget(E,asset,options,report) end
    if options.simplify and options.simplifyMode~='none' then
        local worker,err=require('mesh_simplify_pipeline').start(asset,options.simplifyMode,
            options.simplifyRatio,nil,1,options.simplifyDetails,options.simplifyBoundary,
            options.planarAngle,options.planarTolerance,options.maxVertices,true,
            {simplifyOrder=options.simplifyOrder or 'qem_cgal',edgeLengthFraction=options.remeshEdgeLengthFraction,
             iterations=options.remeshIterations,featureAngle=options.remeshFeatureAngle,repairTopology=options.cgalRepairTopology~=false})
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
                break
            end
            coroutine.yield()
        end
    end
    if not options.remesh then return end
    local worker,err=require('mesh_cgal').startRemesh(asset,nil,1,
        options.remeshEdgeLengthFraction,options.remeshIterations,options.remeshFeatureAngle,
        options.maxVertices,true,nil,options.remeshRepairTopology,deferNormals)
    if not worker then error(string.format(tLang.L('simplify_failed_fmt'),tostring(err)),0) end
    E.simplifyAsset=worker;E.simplifyCancelRequested=nil;E.simplifyProgress=0
    local sourceTriangles,sourceVertices=report.triangles,report.vertices
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
            report.remesh=status.report.remesh
            report.remesh.source_triangles=sourceTriangles
            report.remesh.source_vertices=sourceVertices
            report.remesh.result_triangles=status.report.resultTriangleCount
            report.remesh.result_vertices=status.report.resultVertexCount
            report.remesh.maximum_relative_error=status.report.maximumRelativeError or 0
            report.triangles=status.report.resultTriangleCount
            report.vertices=status.report.resultVertexCount
            return worker
        end
        coroutine.yield()
    end
end
return M
