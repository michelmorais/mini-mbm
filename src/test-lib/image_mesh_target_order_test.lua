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
local Simplify=require 'image_mesh_simplify'
local Reduce=require 'image_mesh_frequency'
local originalCgal=package.loaded.mesh_cgal
local originalLang=tLang
tLang={L=function(key) return key..': %s' end}
local function check(order,remesh,cancel)
    local asset={count=1000,vertices=600}
    local calls={}
    local deferred=false
    function asset:startSimplify(ratio)
        assert(not self.split,'QEM received artificial normal boundaries')
        calls[#calls+1]='qem';self.target=math.floor(self.count*ratio);return true
    end
    function asset:getSimplifyStatus()
        if cancel then return {state='cancelled'} end
        self.count=self.target;self.vertices=self.count
        return {state='completed',report={sourceTriangleCount=1000,sourceVertexCount=600,
            resultTriangleCount=self.count,resultVertexCount=self.vertices,unchanged=false}}
    end
    local function worker(kind,delayNormals,count)
        calls[#calls+1]=kind;deferred=delayNormals
        asset.split=not delayNormals;asset.count=count;asset.vertices=count
        local report={resultTriangleCount=count,resultVertexCount=count,unchanged=false,
            sourceTriangleCount=1000,sourceVertexCount=600}
        report[kind]={}
        return {getSimplifyStatus=function() return {state='completed',report=report} end,
            finalizeNormals=function()
                if delayNormals then calls[#calls+1]='normals';asset.split=true end
                return true,asset.vertices
            end}
    end
    package.loaded.mesh_cgal={
        start=function(_,_,_,_,_,_,_,delayNormals) return worker('cgal',delayNormals,math.floor(asset.count*.8)) end,
        startRemesh=function(_,_,_,_,_,_,_,_,_,_,delayNormals) return worker('remesh',delayNormals,1200) end}
    local E={};local report={triangles=1000,vertices=600}
    local options={geometryTargetTriangles=150,simplify=true,simplifyMode='cgal_qem',
        simplifyOrder=order,remesh=remesh,simplifyDetails=true,simplifyBoundary=0}
    local co=coroutine.create(function() Simplify.apply(E,asset,options,report) end)
    local ok,err
    repeat ok,err=coroutine.resume(co) until not ok or coroutine.status(co)=='dead'
    if cancel then
        assert(not ok and err=='ime_generation_cancelled' and E.generationCancelled)
        assert(not E.simplifyAsset)
        return
    end
    assert(ok,err)
    local expected
    if order=='qem_cgal' then expected=remesh and 'remesh,qem,cgal' or 'qem,cgal'
    else expected=remesh and 'cgal,remesh,qem,normals' or 'cgal,qem,normals' end
    assert(table.concat(calls,',')==expected,table.concat(calls,','))
    assert(report.triangles<=150 and report.triangles==asset.count)
    assert(report.simplification.resultTriangleCount==report.triangles)
    assert(report.simplification.sourceTriangleCount==1000 and report.sourceTriangles==1000)
    assert(report.simplification.simplifyOrder==order and report.simplification.cgal)
end
for _,order in ipairs{'qem_cgal','cgal_qem'} do
    check(order,false);check(order,true);check(order,false,true)
end
local current=78758
local count,attempts=Reduce.reduce(150,current,function(ratio)
    local requested=math.floor(current*ratio)
    if requested<392 then return nil,'constrained' end
    current=requested;return current
end,16)
assert(count>=392 and count<410 and attempts<=16,'large constrained source stopped too early')
package.loaded.mesh_cgal=originalCgal;tLang=originalLang
print('IMAGE MESH TARGET ORDER OK: both orders, normal topology, remesh, cancellation, bounded fallback')
