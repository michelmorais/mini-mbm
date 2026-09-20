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

local Wire=require 'image_mesh_wireframe'
local M={}
function M.release(E)
    local source=E.comparison
    if source then
        Wire.release(source)
        if source.preview then
            meshDebug:loadMeshPreview(source.preview,nil); source.preview:destroy()
        end
        if source.previewPath then os.remove(source.previewPath) end
    end
    E.comparison=nil; E.compareSideBySide=false
end
local function bounds(vertices)
    local lo,hi=math.huge,-math.huge
    for _,v in ipairs(vertices) do lo=math.min(lo,v.x); hi=math.max(hi,v.x) end
    return {min=lo,max=hi,width=hi-lo}
end
function M.capture(E,asset,vertices)
    M.release(E)
    local source={previewPath=tUtil.getTemporaryFilePath('.msh'),bounds=bounds(vertices)}
    E.comparison=source
    assert(asset:save(source.previewPath,false,false,true),tLang.L('ime_export_failed'))
    source.preview=mesh:new('3d'); source.preview.visible=false
    assert(meshDebug:loadMeshPreview(source.preview,source.previewPath),tLang.L('ime_preview_failed'))
    source.preview.alwaysRender=true; source.preview.visible=false
end
function M.layout(E,asset)
    local source=E.comparison
    if not source then return end
    local final=bounds(asset:getVertex(1,1,1,asset:getTotalVertex(1,1)))
    source.resultBounds=final
    local gap=math.max(source.bounds.width,final.width)*0.15
    source.gap=math.max(0.001,gap)
    local total=source.bounds.width+source.gap+final.width
    source.offset=-total/2-source.bounds.min
    source.resultOffset=total/2-final.max
    source.totalWidth=total
end
local function position(E)
    local source=E.comparison
    local x=E.compareSideBySide and source.resultOffset or 0
    E.preview:setPos(x,0,0)
    if E.wireObject then E.wireObject:setPos(x,0,0) end
    source.preview:setPos(source.offset,0,0)
    if source.wireObject then source.wireObject:setPos(source.offset,0,0) end
end
function M.ensureWire(E)
    Wire.ensure(E)
    if E.compareSideBySide and E.comparison then Wire.ensure(E.comparison) end
    if E.comparison then position(E) end
end
function M.sync(E)
    Wire.sync(E)
    local source=E.comparison
    if not source then return end
    local visible=not E.editMode and not E.dirty and E.preview~=nil and E.compareSideBySide
    source.preview.visible=visible and not E.wireframe
    if source.wireObject then source.wireObject.visible=visible and E.wireframe end
end
function M.select(E,sideBySide)
    if not E.comparison or E.dirty then return end
    if E.compareSideBySide==sideBySide then return end
    if E.wireframe then
        Wire.ensure(E)
        if sideBySide then Wire.ensure(E.comparison) end
    end
    if sideBySide then E.comparison.singleDistance=E.orbit.distance end
    E.compareSideBySide=sideBySide
    position(E)
    E.fitDistance=sideBySide and math.max(E.singleFitDistance,E.comparison.totalWidth*2.7) or E.singleFitDistance
    E.orbit.distance=sideBySide and math.max(E.orbit.distance,E.fitDistance) or E.comparison.singleDistance
    M.sync(E)
    return true
end
return M
