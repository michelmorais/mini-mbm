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

-- Bounded search over complete pipelines. Each evaluation owns a fresh source copy.
local Model=require 'image_mesh_model'
local Quality=require 'image_mesh_target_quality'
local M={}
function M.search(target,sourceCount,evaluate)
    local best,lower,upper,requested=nil,0,nil,math.min(target,sourceCount)
    local visited={}
    for attempt=1,12 do
        requested=math.max(1,math.min(sourceCount,math.floor(requested)))
        if visited[requested] then break end
        visited[requested]=true
        local result=evaluate(requested)
        if result and result.quality.accepted then
            local reached=result.triangles<=target
            local bestReached=best and best.triangles<=target
            if not best or (reached and not bestReached) or
                (reached and bestReached and result.quality.score<best.quality.score) or
                (not reached and not bestReached and result.triangles<best.triangles) then best=result end
            if reached and result.triangles>=target*.9 then
                best=best or result;best.attempts=attempt;return best
            end
        end
        if best then best.attempts=attempt end
        -- Too much surface loss calls for more source geometry, even when the count fits.
        if not result or not result.quality.accepted or result.triangles<=target then
            lower=requested
        else upper=requested end
        if upper and upper>lower then requested=(lower+upper)/2
        else
            upper=nil
            if requested>=sourceCount then break end
            requested=math.min(sourceCount,requested*2)
        end
    end
    return best
end
local function clone(source)
    local copy=meshDebug:new()
    copy:setType('mesh');copy:setModeDraw(source:getModeDraw())
    copy:setModeFrontFace(source:getModeFrontFace());copy:setModeCullFace(source:getModeCullFace())
    copy:setMaterial(source:getMaterial())
    assert(copy:copyFrameFrom(source,1)>0,'Could not copy target-search source')
    copy:addAnim('Static',1,1,1,0)
    return copy
end
local function dpCall(fn,...)
    local result=table.pack(pcall(fn,...))
    if not result[1] then print('[image_mesh_target] '..tostring(result[2])) end
    return table.unpack(result,1,result.n)
end
function M.apply(E,asset,options,report,process)
    local sourceTriangles,sourceVertices=report.triangles,report.vertices
    local reference=Quality.sample(asset)
    local manual=Model.copy(options);manual.geometryTargetTriangles=nil
    local attempts=0
    E.targetSearch={cancelled=false}
    local ok,best=dpCall(M.search,options.geometryTargetTriangles,sourceTriangles,function(requested)
        if E.targetSearch.cancelled then
            E.generationCancelled=true;E.batch=nil;E.statisticsRequested=nil
            error('ime_generation_cancelled',0)
        end
        attempts=attempts+1
        local candidate=clone(asset)
        local result=Model.copy(report)
        manual.simplifyRatio=math.min(1,(requested+.5)/sourceTriangles)
        manual.simplify=options.simplify;manual.simplifyMode=options.simplifyMode
        if requested>=sourceTriangles then
            manual.simplify=options.simplifyMode=='cgal_qem'
            manual.simplifyMode=manual.simplify and 'cgal' or 'none'
        end
        local ok,err=dpCall(process,E,candidate,manual,result)
        if not ok then
            local message=tostring(err)
            if not E.generationCancelled and (message:find('topology constraints prevent reaching the requested triangle count',1,true)
                or message:find('locked source boundaries require at least ',1,true)) then return nil end
            error(err,0)
        end
        local quality=Quality.compare(reference,Quality.sample(candidate,reference.grid))
        local evaluation={asset=candidate,report=result,triangles=result.triangles,quality=quality}
        coroutine.yield()
        if E.targetSearch.cancelled then
            E.generationCancelled=true;E.batch=nil;E.statisticsRequested=nil
            error('ime_generation_cancelled',0)
        end
        -- Native buffers outweigh the Lua userdata accounting; collect between candidates only.
        collectgarbage('collect')
        return evaluation
    end)
    E.targetSearch=nil
    if not ok then error(best,0) end
    if best then
        asset:removeFrame(1)
        assert(asset:copyFrameFrom(best.asset,1)>0,'Could not install target-search result')
        for k,v in pairs(best.report) do report[k]=v end
        report.targetQuality=best.quality
    else
        report.targetQuality={accepted=true,score=0,retainedSource=true}
    end
    report.sourceTriangles=sourceTriangles;report.sourceVertices=sourceVertices
    report.targetReductionAttempts=attempts
end
return M
