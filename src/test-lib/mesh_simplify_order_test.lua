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
local Pipeline=require 'mesh_simplify_pipeline'
local Model=require 'image_mesh_model'
local Modes=require 'mesh_simplify_modes'
local Presets=require 'image_mesh_presets'
local originalCgal=package.loaded.mesh_cgal
local function run(order,cancelStage,failStage)
    local calls={}
    local asset={count=100}
    function asset:getTotalSubset() return 1 end
    function asset:getTotalIndex() return self.count*3 end
    function asset:startSimplify(ratio)
        calls[#calls+1]='qem';self.source=self.count;self.target=math.floor(self.count*ratio)
        if failStage=='qem-start' then return false,'qem failed' end
        return true
    end
    function asset:cancelSimplify() self.cancelled=true end
    function asset:getSimplifyStatus()
        if self.cancelled then return {state='cancelled'} end
        if failStage=='qem' then return {state='failed',error='qem failed'} end
        self.count=self.target
        return {state='completed',report={sourceTriangleCount=self.source,sourceVertexCount=90,
            resultTriangleCount=self.count,resultVertexCount=50,unchanged=false,
            maximumRelativeError=.02,maximumGeometricError=2,collapseCount=12}}
    end
    package.loaded.mesh_cgal={start=function()
        calls[#calls+1]='cgal'
        if failStage=='cgal-start' then return nil,'cgal failed' end
        local source=asset.count
        local worker={}
        function worker:cancelSimplify() self.cancelled=true end
        function worker:finalizeNormals() return true,42 end
        function worker:getSimplifyStatus()
            if self.cancelled then return {state='cancelled'} end
            if failStage=='cgal' then return {state='failed',error='cgal failed'} end
            asset.count=math.floor(source*.8)
            return {state='completed',report={sourceTriangleCount=source,sourceVertexCount=90,
                resultTriangleCount=asset.count,resultVertexCount=42,unchanged=false,qemRan=false,
                maximumRelativeError=0,maximumGeometricError=0,cgal={regions=1}}}
        end
        return worker
    end}
    local job,err=Pipeline.start(asset,'cgal_qem',.5,nil,1,true,0,10,.05,65535,true,{simplifyOrder=order})
    if not job then assert(failStage==calls[1]..'-start',err);return end
    if cancelStage==1 then job:cancelSimplify() end
    local status=job:getSimplifyStatus()
    if status.state=='running' then
        if cancelStage==2 then job:cancelSimplify() end
        status=job:getSimplifyStatus()
    end
    if cancelStage then assert(status.state=='cancelled')
    elseif failStage then assert(status.state=='failed')
    else
        assert(status.state=='completed')
        assert(table.concat(calls,',')==(order=='qem_cgal' and 'qem,cgal' or 'cgal,qem'))
        assert(status.report.sourceTriangleCount==100)
        assert(status.report.resultTriangleCount==(order=='qem_cgal' and 40 or 50))
        assert(status.report.maximumRelativeError==.02 and status.report.collapseCount==12)
    end
    assert(job:getSimplifyStatus()==status,'terminal result must be stable')
end
for _,order in ipairs{'qem_cgal','cgal_qem'} do
    run(order)
    for stage=1,2 do run(order,stage) end
    for _,failure in ipairs{'qem-start','cgal-start','qem','cgal'} do run(order,nil,failure) end
end
package.loaded.mesh_cgal=originalCgal
local p=Model.new('source.png',32,32)
local r=Model.add(p,'rectangle',0,0,32,32)
assert(Model.options(p,r).simplifyOrder=='qem_cgal')
p.defaults.simplifyOrder=nil
assert(Model.options(p,r).simplifyOrder=='qem_cgal','legacy default')
for _,order in ipairs{'qem_cgal','cgal_qem'} do
    r.overrides.simplifyOrder=order
    Presets.store(p,order,Model.options(p,r))
    assert(p.presets[#p.presets].settings.simplifyOrder==order)
end
Model.validate(p)
assert(not pcall(Model.validateOptions,{simplifyOrder='invalid'},false))
local oldGui,oldLang=tImGui,tLang
local shown=0
local state={simplifyOrder='qem_cgal'}
tLang={L=function(k) return k end}
tImGui={Separator=function() end,Checkbox=function(_,v) return v end,
    SetNextItemWidth=function() end,
    IsItemHovered=function() return false end,TextWrapped=function() end,
    Combo=function(_,index,labels)
        shown=shown+1;assert(index==1 and labels[2]=='Coplanar -> QEM');return true,2
    end}
Modes.orderCombo('qem','test',state);assert(shown==0)
Modes.orderCombo('cgal_qem','test',state);assert(shown==1 and state.simplifyOrder=='cgal_qem')
tImGui.Combo=function() shown=shown+1;return false end
Modes.orderCombo('cgal_qem','test',state);assert(state.simplifyOrder=='cgal_qem')
tImGui,tLang=oldGui,oldLang
print('SIMPLIFICATION ORDER OK: stages, targets, cancellation, failures, defaults, presets, widget')
