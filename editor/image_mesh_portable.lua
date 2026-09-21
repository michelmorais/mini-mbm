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
-- Stage the complete module before replacing any previously exported files.
function M.save(asset,path,dpCall,crop)
    local stem=IO.directory(path)..'/'..path:match('[^/\\]+$'):gsub('%.msh$',''):sub(1,48)
    local plans,files={},{}
    local serial=0
    local function stage(target)
        local temporary,backup
        repeat
            serial=serial+1
            temporary=target..'.ime-tmp-'..serial;backup=target..'.ime-backup-'..serial
        until not IO.exists(temporary) and not IO.exists(backup)
        local f={target=target,temporary=temporary,backup=backup}
        files[#files+1]=f
        return f
    end
    for subset=1,asset:getTotalSubset(1) do
        local texture=asset:getTexture(1,subset)
        if texture:sub(1,1)~='#' then
            local vertices,bounds=nil,{0,0,1,1}
            if crop then
                vertices=asset:getVertex(1,subset,1,asset:getTotalVertex(1,subset))
                bounds={1,1,0,0}
                for _,v in ipairs(vertices) do
                    bounds[1]=math.min(bounds[1],v.u);bounds[2]=math.min(bounds[2],v.v)
                    bounds[3]=math.max(bounds[3],v.u);bounds[4]=math.max(bounds[4],v.v)
                end
            end
            plans[#plans+1]={subset=subset,source=texture,file=stage(stem..string.format('_texture_%02d.png',subset)),vertices=vertices,bounds=bounds}
        end
    end
    local meshFile=stage(path)
    local ok,err=dpCall(function()
        for _,p in ipairs(plans) do
            local b=p.bounds
            local su,sv,ou,ov=mbm.exportImageMeshTexture(p.source,p.file.temporary,b[1],b[2],b[3],b[4],crop and 4 or 0)
            assert(su,sv)
            if crop then
                for _,v in ipairs(p.vertices) do v.u=v.u*su+ou;v.v=v.v*sv+ov end
                asset:setVertex(1,p.subset,1,p.vertices)
            end
            assert(asset:setTexture(1,p.subset,p.file.target:match('[^/\\]+$')))
        end
        assert(asset:save(meshFile.temporary,false,false,true,true),tLang.L('ime_export_failed'))
        for _,f in ipairs(files) do
            if IO.exists(f.target) then assert(os.rename(f.target,f.backup));f.backedUp=true end
            assert(os.rename(f.temporary,f.target));f.installed=true
        end
    end)
    if not ok then
        local rollbackErrors={}
        for i=#files,1,-1 do
            local f=files[i]
            if f.installed then os.remove(f.target) end
            if f.backedUp then
                local restored,why=os.rename(f.backup,f.target)
                if not restored then rollbackErrors[#rollbackErrors+1]=f.backup..': '..tostring(why) end
            end
            os.remove(f.temporary)
        end
        if #rollbackErrors>0 then err=tostring(err)..' / Restore: '..table.concat(rollbackErrors,'; ') end
        error(err,0)
    end
    for _,f in ipairs(files) do if f.backedUp then os.remove(f.backup) end end
    return true
end
return M
