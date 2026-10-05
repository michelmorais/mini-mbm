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

package.path='editor/?.lua;'..package.path
local Target=require 'image_mesh_target'
local Quality=require 'image_mesh_target_quality'
local visited={}
local best=Target.search(150,1000,function(requested)
    visited[#visited+1]=requested
    if requested==150 then return nil end
    if requested==300 then return {triangles=12,quality={accepted=false,score=1}} end
    if requested==600 then return {triangles=200,quality={accepted=true,score=.1}} end
    assert(requested==450)
    return {triangles=142,quality={accepted=true,score=.15}}
end)
assert(best.triangles==142 and #visited==4)
assert(not Target.search(150,1000,function() return {triangles=12,quality={accepted=false,score=1}} end))
-- A front groove, plus a back face: back depth must not mask lost front detail.
local function surface(flat)
    local vertices={}
    for y=0,1 do for x=0,4 do
        vertices[#vertices+1]={x=x,y=y,z=flat and 1 or (x==2 and 0 or 1)}
    end end
    local indices={}
    for x=1,4 do
        for _,i in ipairs{x,x+1,x+6,x,x+6,x+5} do indices[#indices+1]=i end
    end
    local k=#vertices
    for _,v in ipairs{{x=0,y=0,z=-20},{x=4,y=0,z=-20},{x=4,y=1,z=-20},{x=0,y=1,z=-20}} do vertices[#vertices+1]=v end
    for _,i in ipairs{1,3,2,1,4,3} do indices[#indices+1]=k+i end
    return {getTotalSubset=function() return 1 end,getTotalVertex=function() return #vertices end,
        getVertex=function() return vertices end,getIndex=function() return indices end}
end
local ref=Quality.sample(surface(false))
local identical=Quality.compare(ref,Quality.sample(surface(false),ref.grid))
assert(identical.accepted and identical.score==0 and identical.range<1)
local flat=Quality.compare(ref,Quality.sample(surface(true),ref.grid))
assert(not flat.accepted and flat.depth==0 and flat.slope>.9)
local missing=Quality.compare(ref,{values={},grid=ref.grid})
assert(not missing.accepted and missing.missing>0)
-- Exercise source cloning and commit through the actual target runner.
local savedMesh,savedSample,savedCompare=meshDebug,Quality.sample,Quality.compare
local function asset(count)
    local a={count=count}
    function a:setType() end
    function a:getModeDraw() return 'TRIANGLES' end
    function a:getModeFrontFace() return 'CCW' end
    function a:getModeCullFace() return 'NONE' end
    function a:getMaterial() return {} end
    function a:setModeDraw() end
    function a:setModeFrontFace() end
    function a:setModeCullFace() end
    function a:setMaterial() end
    function a:addAnim() end
    function a:removeFrame() self.count=0 end
    function a:copyFrameFrom(other) self.count=other.count;return 1 end
    return a
end
meshDebug={new=function() return asset(0) end}
Quality.sample=function(a) return {count=a.count,grid={}} end
Quality.compare=function(_,candidate) return {accepted=candidate.count~=12,score=.1} end
local source=asset(1000)
local report={triangles=1000,vertices=600}
local E={}
local calls=0
local co=coroutine.create(function()
    Target.apply(E,source,{geometryTargetTriangles=150},report,function(_,candidate,options,result)
        calls=calls+1
        assert(candidate.count==1000 and source.count==1000,'cumulative reduction or source mutation')
        local requested=math.floor(1000*options.simplifyRatio)
        if requested==150 then error('topology constraints prevent reaching the requested triangle count',0) end
        candidate.count=requested==300 and 12 or (requested==600 and 200 or 142)
        result.triangles=candidate.count
    end)
end)
repeat local ok,err=coroutine.resume(co);assert(ok,err) until coroutine.status(co)=='dead'
assert(source.count==142 and report.triangles==142 and calls==4 and not E.targetSearch)
source=asset(1000);report={triangles=1000,vertices=600};E={}
co=coroutine.create(function()
    Target.apply(E,source,{geometryTargetTriangles=150},report,function(_,candidate,_,result)
        candidate.count=142;result.triangles=142
    end)
end)
assert(coroutine.resume(co));require('image_mesh_generation').cancel(E)
local ok,err=coroutine.resume(co)
assert(not ok and err=='ime_generation_cancelled' and source.count==1000 and not E.targetSearch)
source=asset(1000);report={triangles=1000,vertices=600};E={}
co=coroutine.create(function()
    Target.apply(E,source,{geometryTargetTriangles=150},report,function(_,candidate,_,result)
        candidate.count=12;result.triangles=12
    end)
end)
repeat local passed,message=coroutine.resume(co);assert(passed,message) until coroutine.status(co)=='dead'
assert(source.count==1000 and report.targetQuality.retainedSource,'unsafe fallback replaced the source')
source=asset(1000);report={triangles=1000,vertices=600};E={}
co=coroutine.create(function()
    Target.apply(E,source,{geometryTargetTriangles=1500,simplify=true,simplifyMode='qem'},report,
        function(_,_,options) assert(not options.simplify,'100% request must skip QEM') end)
end)
repeat local passed,message=coroutine.resume(co);assert(passed,message) until coroutine.status(co)=='dead'
assert(source.count==1000 and report.triangles==1000)
meshDebug,Quality.sample,Quality.compare=savedMesh,savedSample,savedCompare
print('IMAGE MESH TARGET QUALITY OK: complete pipeline, fresh source, groove rejection, cancellation, commit')
