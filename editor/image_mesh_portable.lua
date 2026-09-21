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

local IO=require 'image_mesh_io'
local M={}
-- Called on a fresh generated asset, after simplification and export orientation.
function M.save(asset,path,dpCall)
    local stem=IO.directory(path)..'/'..path:match('[^/\\]+$'):gsub('%.msh$',''):sub(1,48)
    local plans,files={},{}
    assert(not IO.exists(path),tLang.L('ime_file_exists')..' '..path)
    for subset=1,asset:getTotalSubset(1) do
        local texture=asset:getTexture(1,subset)
        if texture:sub(1,1)~='#' then
            local vertices=asset:getVertex(1,subset,1,asset:getTotalVertex(1,subset))
            local bounds={1,1,0,0}
            for _,v in ipairs(vertices) do
                bounds[1]=math.min(bounds[1],v.u);bounds[2]=math.min(bounds[2],v.v)
                bounds[3]=math.max(bounds[3],v.u);bounds[4]=math.max(bounds[4],v.v)
            end
            local target=stem..string.format('_texture_%02d.png',subset)
            assert(not IO.exists(target),tLang.L('ime_file_exists')..' '..target)
            plans[#plans+1]={subset=subset,source=texture,target=target,vertices=vertices,bounds=bounds}
        end
    end
    local ok,err=dpCall(function()
        for _,p in ipairs(plans) do
            files[#files+1]=p.target
            local b=p.bounds
            local su,sv,ou,ov=mbm.exportImageMeshTexture(p.source,p.target,b[1],b[2],b[3],b[4],4)
            assert(su,sv)
            for _,v in ipairs(p.vertices) do v.u=v.u*su+ou;v.v=v.v*sv+ov end
            asset:setVertex(1,p.subset,1,p.vertices)
            asset:setTexture(1,p.subset,p.target:match('[^/\\]+$'))
        end
        files[#files+1]=path
        assert(asset:save(path,false,false,true,true),tLang.L('ime_export_failed'))
    end)
    if not ok then
        for _,file in ipairs(files) do os.remove(file) end
        error(err,0)
    end
    return true
end
return M
