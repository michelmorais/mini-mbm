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

local Model=require 'image_mesh_model'
local M={}
function M.destroy(E)
    if E.heightObject then E.heightObject:destroy(); E.heightObject=nil end
    if E.heightPath then os.remove(E.heightPath); E.heightPath=nil end
    E.heightScale=nil; E.heightRegion=nil
end
function M.build(E)
    M.destroy(E)
    local region=Model.copy(E.draft)
    region.overrides={}
    for k in pairs(Model.defaults) do region.overrides[k]=E.values[k] end
    local candidate=Model.copy(E.project); candidate.regions={region}
    Model.validate(candidate)
    local options=Model.options(E.project,region)
    E.heightPath=tUtil.getTemporaryFilePath('.png')
    local ok,message=mbm.generateImageMeshMap(E.project.image.path,options,E.heightPath,E.heightView==3)
    if not ok then error(message,0) end
    local object=texture:new('2dw'); E.heightObject=object
    assert(object:load(E.heightPath))
    object:setSize(region.w,region.h)
    E.heightRegion=region
    E.heightBuilds=(E.heightBuilds or 0)+1
end
function M.sync(E,protectedCall)
    local key=E.revision..':'..E.selected..':'..E.heightView
    if E.heightKey~=key then M.destroy(E); E.heightKey=key; E.heightRequested=true; E.heightError=nil end
    local visible=E.editMode and E.heightView~=1 and E.draft~=nil and not E.drag
    if visible and E.heightRequested then
        E.heightRequested=false
        local ok=protectedCall(M.build,E)
        if not ok then M.destroy(E); E.heightError=E.status else E.heightError=nil end
    end
    if E.heightObject then
        E.heightObject.visible=visible
        if E.heightScale~=E.zoom then
            local r=E.heightRegion
            E.heightObject:setScale(E.zoom,E.zoom)
            E.heightObject:setPos((r.x+r.w/2-E.project.image.width/2)*E.zoom,
                (E.project.image.height/2-r.y-r.h/2)*E.zoom,0.5)
            E.heightScale=E.zoom
        end
    end
end
return M
