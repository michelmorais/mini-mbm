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
    E.comparison=nil; E.compareOriginal=false
end
function M.capture(E,asset)
    M.release(E)
    local source={previewPath=tUtil.getTemporaryFilePath('.msh')}
    E.comparison=source
    assert(asset:save(source.previewPath,false,false,true),tLang.L('ime_export_failed'))
    source.preview=mesh:new('3d'); source.preview.visible=false
    assert(meshDebug:loadMeshPreview(source.preview,source.previewPath),tLang.L('ime_preview_failed'))
    source.preview.alwaysRender=true; source.preview.visible=false
end
function M.ensureWire(E)
    Wire.ensure(E.compareOriginal and E.comparison or E)
end
function M.sync(E)
    Wire.sync(E)
    local source=E.comparison
    if not source then return end
    local visible=not E.editMode and not E.dirty and E.preview~=nil and E.compareOriginal
    source.preview.visible=visible and not E.wireframe
    if source.wireObject then source.wireObject.visible=visible and E.wireframe end
    if E.compareOriginal then
        if E.preview then E.preview.visible=false end
        if E.wireObject then E.wireObject.visible=false end
    end
end
function M.select(E,original)
    if not E.comparison or E.dirty then return end
    if E.compareOriginal==original then return end
    -- Build the requested wireframe at most once, before changing the active view.
    if E.wireframe then Wire.ensure(original and E.comparison or E) end
    E.compareOriginal=original
    M.sync(E)
end
return M
