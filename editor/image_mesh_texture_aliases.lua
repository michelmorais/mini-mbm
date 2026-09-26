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

   Short texture aliases for temporary Image Mesh preview meshes.
]]--

local IO=require 'image_mesh_io'
local M={}

local function fileName(path)
    return path:gsub('\\','/'):match('([^/]+)$') or path
end

local function normalize(path)
    return (path or ''):gsub('\\','/')
end

local function shortHash(value)
    -- Keep each arithmetic step below 2^24. The engine may build Lua with float32
    -- lua_Number, where multiplying a full 32-bit hash silently loses low bits.
    local lowA,highA=0,0
    local lowB,highB=5381,0
    for index=1,#value do
        local byte=value:byte(index)
        local productA=lowA*33+byte
        lowA=productA%65536
        highA=(highA*33+math.floor(productA/65536))%65536

        local previousLowB=lowB
        local productB=previousLowB*63+byte
        lowB=productB%65536
        highB=(highB*63+previousLowB+math.floor(productB/65536))%65536
    end
    return string.format('%04x%04x%04x%04x',highA,lowA,highB,lowB)
end

local function copyFile(source,destination)
    local input=io.open(source,'rb')
    if not input then return false end
    local output=io.open(destination,'wb')
    if not output then input:close();return false end
    while true do
        local chunk=input:read(64*1024)
        if not chunk then break end
        if not output:write(chunk) then
            input:close();output:close();return false
        end
    end
    input:close()
    return output:close()
end

-- Mesh V11 stores a texture basename in a 64-byte field (63 usable bytes).
-- `aliasAll` also isolates Mesh Debug projects that share a basename in the engine texture cache.
function M.apply(state,asset,directory,namespace,aliasAll)
    state.tempTextureAliases=state.tempTextureAliases or {}
    state.nextTempTextureId=state.nextTempTextureId or 0
    local replacements,changed={},{}
    local function restore()
        for index=#changed,1,-1 do
            local item=changed[index]
            asset:setTexture(1,item.subset,item.texture)
        end
        changed={}
    end

    for subset=1,asset:getTotalSubset(1) do
        local texture=asset:getTexture(1,subset)
        if texture and texture~='' and texture:sub(1,1)~='#' then
            local resolved=texture
            if not IO.exists(resolved) then resolved=mbm.getFullPath(texture) or texture end
            assert(IO.exists(resolved),'ime_texture_missing: '..texture)
            local base=fileName(resolved)
            if aliasAll or #base>63 then
                local alias=state.tempTextureAliases[resolved]
                if not alias then
                    local extension=base:match('(%.[^%.]+)$') or ''
                    assert(#extension<=24 and extension:match('^%.[%w]+$'),
                        'ime_texture_extension_missing: '..texture)
                    state.nextTempTextureId=state.nextTempTextureId+1
                    local key=shortHash(normalize(namespace)..'\0'..normalize(resolved))
                    alias=string.format('i%s_t%03d%s',key,state.nextTempTextureId,extension)
                    local destination=directory..package.config:sub(1,1)..alias
                    assert(copyFile(resolved,destination),'ime_texture_copy_failed: '..texture)
                    state.tempTextureAliases[resolved]=alias
                end
                replacements[#replacements+1]={subset=subset,texture=texture,alias=alias}
            end
        end
    end
    for _,replacement in ipairs(replacements) do
        changed[#changed+1]=replacement
        if not asset:setTexture(1,replacement.subset,replacement.alias) then
            restore()
            error('ime_texture_alias_failed: '..replacement.texture,0)
        end
    end
    return restore
end

local previewState=nil
local function hasLongTexture(asset)
    for subset=1,asset:getTotalSubset(1) do
        local texture=asset:getTexture(1,subset)
        if texture and texture~='' and texture:sub(1,1)~='#' and #fileName(texture)>63 then
            return true
        end
    end
    return false
end

function M.savePreview(asset,path,namespace)
    if not hasLongTexture(asset) then return asset:save(path,false,false,true) end
    if not previewState then
        local directory=tUtil.getTemporaryFilePath('_mini_mbm_imesh_preview_textures')
        assert(mbm.createDirectories(directory),'ime_temporary_folder_failed')
        mbm.addPath(directory)
        previewState={directory=directory,tempTextureAliases={},nextTempTextureId=0}
    end
    local restore=M.apply(previewState,asset,previewState.directory,namespace,false)
    local saved=asset:save(path,false,false,true)
    restore()
    return saved
end

return M
