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

-- Callers own a disposable working mesh and publish it only after completion.
local M={}
local function triangles(asset,subset,frame)
    local count=0
    for s=subset or 1,subset or asset:getTotalSubset(frame) do
        local indices=asset:getTotalIndex(frame,s)
        count=count+(indices>0 and indices or asset:getTotalVertex(frame,s))/3
    end
    return count
end
function M.start(asset,mode,ratio,subset,frame,details,boundary,angle,distance,maxVertices,normals,remeshSettings)
    if mode=='none' then return nil,'No simplification method selected' end
    if mode=='remesh' then
        return require('mesh_cgal').startRemesh(asset,subset,frame,
            remeshSettings and remeshSettings.edgeLengthFraction or .03,
            remeshSettings and remeshSettings.iterations or 3,
            remeshSettings and remeshSettings.featureAngle or 45,maxVertices,normals,remeshSettings and remeshSettings.targetTriangles)
    end
    if mode~='cgal' and mode~='cgal_qem' then
        local ok,err=asset:startSimplify(ratio,subset,frame,details,boundary)
        if not ok then return nil,err end
        return asset
    end
    local source=triangles(asset,subset,1)
    local target
    if mode=='cgal_qem' then
        if type(ratio)~='number' or ratio~=ratio or ratio<=0 or ratio>1 then
            return nil,'Invalid simplification ratio'
        end
        target=math.max(1,math.floor(source*ratio))
    end
    local worker,err=require('mesh_cgal').start(asset,subset,frame,angle,distance,maxVertices,normals,mode=='cgal_qem')
    if not worker or mode=='cgal' then return worker,err end
    local cgalWorker=worker
    local job={stage='cgal'}
    local cgalReport,terminal,cancelled
    function job:cancelSimplify()
        cancelled=true
        return worker:cancelSimplify()
    end
    function job:getSimplifyStatus()
        if terminal then return terminal end
        local status=worker:getSimplifyStatus()
        if status.state=='running' then
            return {state='running',progress=(self.stage=='qem' and .5 or 0)+(status.progress or 0)*.5}
        end
        if cancelled then status={state='cancelled'} end
        if status.state~='completed' then terminal=status;return status end
        if self.stage=='cgal' then
            cgalReport=status.report
            cgalReport.backend='cgal_qem'
            -- Native quality metrics describe only the QEM stage, never CGAL's input.
            cgalReport.maximumGeometricError=0;cgalReport.maximumRelativeError=0
            local remaining=triangles(asset,subset,1)
            if remaining>target then
                -- Use the middle of the integer bucket to survive float conversion.
                local ok,message=asset:startSimplify((target+.5)/remaining,subset,frame,details,boundary)
                if not ok then terminal={state='failed',error=message};return terminal end
                worker=asset;self.stage='qem'
                return {state='running',progress=.5}
            end
        else
            local report=status.report
            report.backend='cgal_qem';report.qemRan=true;report.cgal=cgalReport.cgal
            report.sourceVertexCount=cgalReport.sourceVertexCount
            report.sourceTriangleCount=cgalReport.sourceTriangleCount
            report.unchanged=cgalReport.unchanged and report.unchanged
        end
        local ok,vertices=cgalWorker:finalizeNormals()
        if not ok then terminal={state='failed',error=vertices};return terminal end
        if vertices then status.report.resultVertexCount=vertices end
        terminal=status
        return terminal
    end
    return job
end
return M
